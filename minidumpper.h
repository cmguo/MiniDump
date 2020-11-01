#ifndef MINIDUMPPER_H
#define MINIDUMPPER_H

#include <string>

class MiniDumpper
{
public:
    MiniDumpper(int pid);

public:
    bool CreateMiniDump();

private:
    bool SetDumpPrivileges();

    // Formats the error message.
    std::wstring FormatErrorMsg(unsigned long dwErrorCode);

    // Sets the progress message and percent completed
    void SetProgress(std::wstring sStatusMsg, int percentCompleted, bool bRelative=true);

    bool IsCancelled();

public:
    int OnMinidumpProgress(void * const CallbackInput,
        void * CallbackOutput);

private:
    int m_dwProcessId;
};

#endif // MINIDUMPPER_H
