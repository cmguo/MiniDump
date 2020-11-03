#ifndef MINIDUMPPER_H
#define MINIDUMPPER_H

#include <string>

class MiniDumpper
{
public:
    MiniDumpper(int pid);

    MiniDumpper(std::wstring const & name);

public:
    bool CreateMiniDump();

private:
    bool SetDumpPrivileges();

    // Sets the progress message and percent completed
    void SetProgress(std::wstring sStatusMsg, int percentCompleted, bool bRelative=true);

    bool IsCancelled();

public:
    int OnMinidumpProgress(void * const CallbackInput,
        void * CallbackOutput);

private:
    static int FindProcessId(std::wstring const & name);

    // Formats the error message.
    static std::wstring FormatErrorMsg(unsigned long dwErrorCode);

private:
    int m_dwProcessId;
};

#endif // MINIDUMPPER_H
