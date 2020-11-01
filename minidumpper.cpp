#include "minidumpper.h"

#include <Windows.h>
#include <DbgHelp.h>

#include <string>
#include <assert.h>

MiniDumpper::MiniDumpper(int pid)
{
    m_dwProcessId = pid;
}

// This callback function is called by MinidumpWriteDump
static BOOL CALLBACK MiniDumpCallback(
    PVOID CallbackParam,
    PMINIDUMP_CALLBACK_INPUT CallbackInput,
    PMINIDUMP_CALLBACK_OUTPUT CallbackOutput )
{
    // Delegate back to the CErrorReportSender
    MiniDumpper* pErrorReportSender = (MiniDumpper*)CallbackParam;
    return pErrorReportSender->OnMinidumpProgress(CallbackInput, CallbackOutput);
}

bool MiniDumpper::CreateMiniDump()
{
    BOOL bStatus = FALSE;
    HMODULE hDbgHelp = nullptr;
    HANDLE hFile = nullptr;
    MINIDUMP_CALLBACK_INFORMATION mci;

    SYSTEMTIME st;
    GetLocalTime(&st);

    std::wstring sMinidumpFile = TEXT("crashdump-00-00-00.dmp");
    wsprintfW(&sMinidumpFile[0], TEXT("crashdump-%02d-%02d-%02d.dmp"), st.wHour, st.wMinute, st.wSecond);

    std::wstring sErrorMsg;

    // Update progress
    SetProgress(TEXT("Creating crash dump file..."), 0, false);
    SetProgress(TEXT("[creating_dump]"), 0, false);

    // Load dbghelp.dll
    const std::wstring sDebugHelpDLL_name = TEXT("dbghelp.dll");
    hDbgHelp = LoadLibrary(sDebugHelpDLL_name.c_str());

    if(hDbgHelp==nullptr)
    {
        sErrorMsg = TEXT("dbghelp.dll couldn't be loaded");
        SetProgress(TEXT("dbghelp.dll couldn't be loaded."), 0, false);
        goto cleanup;
    }

    // Try to adjust process privilegies to be able to generate minidumps.
    SetDumpPrivileges();

    // Create the minidump file
    hFile = CreateFile(
        sMinidumpFile.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    // Check if file has been created
    if(hFile==INVALID_HANDLE_VALUE)
    {
        DWORD dwError = GetLastError();
        std::wstring sMsg = TEXT("Couldn't create minidump file: ");
        sMsg += FormatErrorMsg(dwError);
        SetProgress(sMsg, 0, false);
        sErrorMsg = sMsg;
        return FALSE;
    }

    // Set valid dbghelp API version
    typedef LPAPI_VERSION (WINAPI* LPIMAGEHLPAPIVERSIONEX)(LPAPI_VERSION AppVersion);
    LPIMAGEHLPAPIVERSIONEX lpImagehlpApiVersionEx =
        (LPIMAGEHLPAPIVERSIONEX)GetProcAddress(hDbgHelp, "ImagehlpApiVersionEx");
    assert(lpImagehlpApiVersionEx!=nullptr);
    if(lpImagehlpApiVersionEx!=nullptr)
    {
        API_VERSION CompiledApiVer;
        CompiledApiVer.MajorVersion = 6;
        CompiledApiVer.MinorVersion = 1;
        CompiledApiVer.Revision = 11;
        CompiledApiVer.Reserved = 0;
        LPAPI_VERSION pActualApiVer = lpImagehlpApiVersionEx(&CompiledApiVer);
        pActualApiVer;
        //ATLASSERT(CompiledApiVer.MajorVersion==pActualApiVer->MajorVersion);
        //ATLASSERT(CompiledApiVer.MinorVersion==pActualApiVer->MinorVersion);
        //ATLASSERT(CompiledApiVer.Revision==pActualApiVer->Revision);
    }

    // Write minidump to the file

    mci.CallbackRoutine = MiniDumpCallback;
    mci.CallbackParam = this;

    typedef BOOL (WINAPI *LPMINIDUMPWRITEDUMP)(
        HANDLE hProcess,
        DWORD ProcessId,
        HANDLE hFile,
        MINIDUMP_TYPE DumpType,
        CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
        CONST PMINIDUMP_USER_STREAM_INFORMATION UserEncoderParam,
        CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam);

    // Get address of MiniDumpWirteDump function
    LPMINIDUMPWRITEDUMP pfnMiniDumpWriteDump =
        (LPMINIDUMPWRITEDUMP)GetProcAddress(hDbgHelp, "MiniDumpWriteDump");
    if(!pfnMiniDumpWriteDump)
    {
        SetProgress(TEXT("Bad MiniDumpWriteDump function."), 0, false);
        sErrorMsg = TEXT("Bad MiniDumpWriteDump function");
        return FALSE;
    }

    // Open client process
    HANDLE hProcess = OpenProcess(
        PROCESS_ALL_ACCESS,
        FALSE,
        m_dwProcessId);

    // Now actually write the minidump
    BOOL bWriteDump = pfnMiniDumpWriteDump(
        hProcess,
        m_dwProcessId,
        hFile,
        MiniDumpNormal,
        nullptr,
        nullptr,
        nullptr);

    // Check result
    if(!bWriteDump)
    {
        std::wstring sMsg = FormatErrorMsg(GetLastError());
        SetProgress(TEXT("Error writing dump."), 0, false);
        SetProgress(sMsg, 0, false);
        sErrorMsg = sMsg;
        goto cleanup;
    }

    // Update progress
    bStatus = TRUE;
    SetProgress(TEXT("Finished creating dump."), 100, false);

cleanup:

    // Close file
    if(hFile)
        CloseHandle(hFile);

    // Unload dbghelp.dll
    if(hDbgHelp)
        FreeLibrary(hDbgHelp);

    return bStatus;
}

bool MiniDumpper::SetDumpPrivileges()
{
    // This method is used to have the current process be able to call MiniDumpWriteDump
    // This code was taken from:
    // http://social.msdn.microsoft.com/Forums/en-US/vcgeneral/thread/f54658a4-65d2-4196-8543-7e71f3ece4b6/


    BOOL       fSuccess  = FALSE;
    HANDLE      TokenHandle = nullptr;
    TOKEN_PRIVILEGES TokenPrivileges;

    if (!OpenProcessToken(GetCurrentProcess(),
        TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
        &TokenHandle))
    {
        SetProgress(TEXT("SetDumpPrivileges: Could not get the process token"), 0);
        goto Cleanup;
    }

    TokenPrivileges.PrivilegeCount = 1;

    if (!LookupPrivilegeValue(nullptr,
        SE_DEBUG_NAME,
        &TokenPrivileges.Privileges[0].Luid))
    {
        SetProgress(TEXT("SetDumpPrivileges: Couldn't lookup SeDebugPrivilege name"), 0);
        goto Cleanup;
    }

    TokenPrivileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    //Add privileges here.
    if (!AdjustTokenPrivileges(TokenHandle,
        FALSE,
        &TokenPrivileges,
        sizeof(TokenPrivileges),
        nullptr,
        nullptr))
    {
        SetProgress(TEXT("SetDumpPrivileges: Could not revoke the debug privilege"), 0);
        goto Cleanup;
    }

    fSuccess = TRUE;

Cleanup:

    if (TokenHandle)
    {
        CloseHandle(TokenHandle);
    }

    return fSuccess;
}

void MiniDumpper::SetProgress(std::wstring sStatusMsg, int percentCompleted, bool bRelative)
{
    wprintf(TEXT("Progress %s %i\n"), sStatusMsg.c_str(), percentCompleted);
}

bool MiniDumpper::IsCancelled()
{
    return false;
}


// This method is called when MinidumpWriteDump notifies us about
// currently performed action
int MiniDumpper::OnMinidumpProgress(void * const CallbackInput,
                                            void * CallbackOutput)
{
    switch(reinterpret_cast<PMINIDUMP_CALLBACK_INPUT>(CallbackInput)->CallbackType)
    {
    case CancelCallback:
        {
            // This callback allows to cancel minidump generation
            if(IsCancelled())
            {
                reinterpret_cast<PMINIDUMP_CALLBACK_OUTPUT>(CallbackOutput)->Cancel = TRUE;
                SetProgress(TEXT("Dump generation cancelled by user"), 0, true);
            }
        }
        break;

    case ModuleCallback:
        {
            // We are currently dumping some module
            std::wstring sMsg = TEXT("Dumping info for module");
            sMsg += reinterpret_cast<PMINIDUMP_CALLBACK_INPUT>(CallbackInput)->Module.FullPath;

            // Update progress
            SetProgress(sMsg, 0, true);
        }
        break;
    case ThreadCallback:
        {
            // We are currently dumping some thread
            std::wstring sMsg = TEXT("Dumping info for thread ");
            WCHAR buf[16];
            wsprintfW(buf, TEXT("0x%X"), reinterpret_cast<PMINIDUMP_CALLBACK_INPUT>(CallbackInput)->Thread.ThreadId);
            sMsg += buf;
            SetProgress(sMsg, 0, true);
        }
        break;

    }

    return TRUE;
}

std::wstring MiniDumpper::FormatErrorMsg(DWORD dwErrorCode)
{
    LPTSTR msg = 0;
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_ALLOCATE_BUFFER,
        NULL, dwErrorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&msg, 0, NULL);
    std::wstring str = msg;
    GlobalFree(msg);
    return str;
}

