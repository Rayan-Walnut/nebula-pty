#include "win/conpty.h"
#include <iostream>

namespace conpty {

ConPTY::ConPTY()
    : hPipeIn(NULL),
      hPipeOut(NULL),
      hPtyIn(NULL),
      hPtyOut(NULL),
      hPC(NULL),
      hProcess(NULL),
      hThread(NULL),
      processId(0),
      isInitialized(false)
{
}

ConPTY::~ConPTY()
{
    Close();
}

bool ConPTY::Create(SHORT cols, SHORT rows)
{
    if (!CreatePipes())
    {
        return false;
    }

    return CreatePseudoConsole(cols, rows);
}
bool ConPTY::CreatePipes()
{
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    // Créer les pipes avec les permissions explicites et vérifier les handles
    if (!CreatePipe(&hPipeIn, &hPtyOut, &sa, 0))
    {
        DWORD error = GetLastError();
        std::cerr << "CreatePipe (in/out) failed with error: " << error << std::endl;
        return false;
    }

    // S'assurer que le handle n'est pas hérité où il ne devrait pas l'être
    if (!SetHandleInformation(hPipeIn, HANDLE_FLAG_INHERIT, 0))
    {
        DWORD error = GetLastError();
        std::cerr << "SetHandleInformation failed with error: " << error << std::endl;
        CloseHandle(hPipeIn);
        CloseHandle(hPtyOut);
        return false;
    }

    if (!CreatePipe(&hPtyIn, &hPipeOut, &sa, 0))
    {
        DWORD error = GetLastError();
        std::cerr << "CreatePipe (in/out) failed with error: " << error << std::endl;
        CloseHandle(hPipeIn);
        CloseHandle(hPtyOut);
        return false;
    }

    if (!SetHandleInformation(hPipeOut, HANDLE_FLAG_INHERIT, 0))
    {
        DWORD error = GetLastError();
        std::cerr << "SetHandleInformation failed with error: " << error << std::endl;
        CloseHandle(hPipeIn);
        CloseHandle(hPtyOut);
        CloseHandle(hPtyIn);
        CloseHandle(hPipeOut);
        return false;
    }

    return true;
}

bool ConPTY::CreatePseudoConsole(SHORT cols, SHORT rows)
{
    COORD size = { cols, rows };
    HRESULT hr = ::CreatePseudoConsole(size, hPtyIn, hPtyOut, 0, &hPC);
    
    if (FAILED(hr))
    {
        return false;
    }

    isInitialized = true;
    return true;
}

bool ConPTY::Start(const std::wstring& command, const std::wstring& cwd)
{
    if (!isInitialized)
    {
        return false;
    }

    STARTUPINFOEXW siEx{};
    siEx.StartupInfo.cb = sizeof(STARTUPINFOEXW);
    
    SIZE_T size;
    InitializeProcThreadAttributeList(NULL, 1, 0, &size);
    siEx.lpAttributeList = (PPROC_THREAD_ATTRIBUTE_LIST)new char[size];
    InitializeProcThreadAttributeList(siEx.lpAttributeList, 1, 0, &size);

    UpdateProcThreadAttribute(
        siEx.lpAttributeList,
        0,
        PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
        hPC,
        sizeof(HANDLE),
        NULL,
        NULL
    );

    PROCESS_INFORMATION pi{};
    BOOL success = CreateProcessW(
        NULL,
        const_cast<LPWSTR>(command.c_str()),
        NULL,
        NULL,
        FALSE,
        EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT,
        NULL,
        cwd.empty() ? NULL : cwd.c_str(),
        &siEx.StartupInfo,
        &pi
    );

    if (success) {
        hProcess = pi.hProcess;
        hThread = pi.hThread;
        processId = pi.dwProcessId;
    }

    delete[] reinterpret_cast<char*>(siEx.lpAttributeList);
    return success != 0;
}

void ConPTY::Close()
{
    if (hProcess != NULL)
    {
        DWORD exitCode;
        if (GetExitCodeProcess(hProcess, &exitCode) && exitCode == STILL_ACTIVE)
        {
            TerminateProcess(hProcess, 0);
        }
        CloseHandle(hProcess);
        hProcess = NULL;
    }

    if (hThread != NULL)
    {
        DWORD exitCode;
        if (GetExitCodeThread(hThread, &exitCode))
        {
            TerminateThread(hThread, 0);
        }
        CloseHandle(hThread);
        hThread = NULL;
    }

    if (hPC)
    {
        ClosePseudoConsole(hPC);
        hPC = NULL;
    }

    if (hPipeIn) CloseHandle(hPipeIn);
    if (hPipeOut) CloseHandle(hPipeOut);
    if (hPtyIn) CloseHandle(hPtyIn);
    if (hPtyOut) CloseHandle(hPtyOut);

    hPipeIn = hPipeOut = hPtyIn = hPtyOut = NULL;
    isInitialized = false;
}

bool ConPTY::IsActive() const
{
    if (!hProcess)
    {
        return false;
    }

    DWORD exitCode;
    if (!GetExitCodeProcess(hProcess, &exitCode))
    {
        return false;
    }

    return exitCode == STILL_ACTIVE;
}
bool ConPTY::Write(const char* data, DWORD length, DWORD* written)
{
    if (!hPipeIn || !isInitialized) {
        std::cerr << "Write failed: pipe not initialized" << std::endl;
        return false;
    }

    // Vérifier que le handle est valide
    if (hPipeIn == INVALID_HANDLE_VALUE) {
        std::cerr << "Write failed: invalid handle" << std::endl;
        return false;
    }

    DWORD bytesWritten = 0;
    BOOL result = WriteFile(hPipeIn, data, length, &bytesWritten, NULL);
    if (!result) {
        DWORD error = GetLastError();
        std::cerr << "WriteFile failed with error: " << error << std::endl;
        
        // Si le pipe est fermé, on essaie de le recréer
        if (error == ERROR_BROKEN_PIPE || error == ERROR_NO_DATA) {
            if (CreatePipes()) {
                result = WriteFile(hPipeIn, data, length, &bytesWritten, NULL);
            }
        }
    }
    
    if (written) {
        *written = bytesWritten;
    }
    
    return result != 0;
}

bool ConPTY::Read(char* data, DWORD length, DWORD* read)
{
    return ReadFile(hPipeOut, data, length, read, NULL) != 0;
}

bool ConPTY::Resize(SHORT cols, SHORT rows)
{
    if (!isInitialized)
    {
        return false;
    }

    COORD size = { cols, rows };
    return SUCCEEDED(ResizePseudoConsole(hPC, size));
}

} // namespace conpty