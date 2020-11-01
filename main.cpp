#include "minidumpper.h"

#include <Windows.h>

int main()
{
    // Get command line parameters.
    LPCWSTR szCommandLine = GetCommandLineW();

    // Split command line.
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(szCommandLine, &argc);

    // Check parameter count.
    if(argc < 2)
        return 1; // No arguments passed, exit.

    int pid = wcstol(argv[1], nullptr, 10);

    int interval = 0;

    if (argc > 2)
        interval = wcstol(argv[2], nullptr, 10);

    MiniDumpper dumpper(pid);

    dumpper.CreateMiniDump();

    while (interval) {
        if (SleepEx(interval * 1000, TRUE) != 0)
            break;
        dumpper.CreateMiniDump();
    }

    return 0;
}
