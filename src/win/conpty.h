#pragma once

#include <Windows.h>
#include <string>

#ifndef PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
#define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE \
    ProcThreadAttributeValue(22, FALSE, TRUE, FALSE)
#endif

typedef void *HPCON;

namespace conpty {

class ConPTY {
public:
    ConPTY();
    ~ConPTY();

    bool Create(SHORT cols, SHORT rows);
    bool Start(const std::wstring &command);
    bool Write(const char *data, DWORD length, DWORD *written);
    bool Read(char *data, DWORD length, DWORD *read);
    bool Resize(SHORT cols, SHORT rows);
    void Close();

    DWORD GetProcessId() const { return processId; }

private:
    bool CreatePipes();
    bool CreatePseudoConsole(SHORT cols, SHORT rows);
    bool StartProcess(const std::wstring &command);

    HANDLE hPipeIn;
    HANDLE hPipeOut;
    HPCON hPC;
    HANDLE hProcess;
    HANDLE hThread;
    DWORD processId;
    bool isInitialized;
};

} // namespace conpty