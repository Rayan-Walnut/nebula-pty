#ifndef CONPTY_H
#define CONPTY_H

#include <windows.h>
#include <string>

namespace conpty {

class ConPTY {
public:
    ConPTY();
    ~ConPTY();

    bool Create(SHORT width, SHORT height);
    bool Start(const std::wstring& command);
    bool Write(const void* data, DWORD length, DWORD* written);
    bool Read(void* data, DWORD length, DWORD* read);
    bool Resize(SHORT width, SHORT height);
    void Close();

private:
    HPCON hPC;
    HANDLE hPipeIn;
    HANDLE hPipeOut;
    HANDLE hProcess;
    bool running;

    void CleanupHandles();
};

} // namespace conpty

#endif // CONPTY_H