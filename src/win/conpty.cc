#include "win/conpty.h"
#include <iostream>
#include <vector>
#include <windows.h>

namespace conpty {

ConPTY::ConPTY() 
    : hPipeIn(INVALID_HANDLE_VALUE)
    , hPipeOut(INVALID_HANDLE_VALUE)
    , hPC(nullptr)
    , hProcess(INVALID_HANDLE_VALUE)
    , hThread(INVALID_HANDLE_VALUE)
    , processId(0)  
    , isInitialized(false) {
}

ConPTY::~ConPTY() {
    Close();
}

bool ConPTY::Create(SHORT cols, SHORT rows) {
    if (isInitialized) {
        return false;
    }

    if (!CreatePipes()) {
        return false;
    }

    if (!CreatePseudoConsole(cols, rows)) {
        Close();
        return false;
    }

    isInitialized = true;
    return true;
}

bool ConPTY::CreatePipes() {
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };
    
    // Créer les pipes avec des noms uniques
    WCHAR pipeName[MAX_PATH];
    swprintf_s(pipeName, L"\\\\.\\pipe\\conpty-%08x", GetCurrentProcessId());
    
    hPipeIn = CreateNamedPipeW(
        pipeName,
        PIPE_ACCESS_INBOUND,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        1,
        0,
        0,
        0,
        &sa
    );

    if (hPipeIn == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to create input pipe: " << GetLastError() << std::endl;
        return false;
    }

    swprintf_s(pipeName, L"\\\\.\\pipe\\conpty-%08x-out", GetCurrentProcessId());
    hPipeOut = CreateNamedPipeW(
        pipeName,
        PIPE_ACCESS_OUTBOUND,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        1,
        0,
        0,
        0,
        &sa
    );

    if (hPipeOut == INVALID_HANDLE_VALUE) {
        CloseHandle(hPipeIn);
        hPipeIn = INVALID_HANDLE_VALUE;
        std::cerr << "Failed to create output pipe: " << GetLastError() << std::endl;
        return false;
    }

    return true;
}

bool ConPTY::CreatePseudoConsole(SHORT cols, SHORT rows) {
    HMODULE hLibrary = LoadLibraryExW(L"kernel32.dll", nullptr, 0);
    if (!hLibrary) {
        std::cerr << "Failed to load kernel32.dll" << std::endl;
        return false;
    }

    typedef HRESULT (*PFNCREATEPSEUDOCONSOLE)(COORD c, HANDLE hIn, HANDLE hOut, DWORD dwFlags, HPCON* phpcon);
    auto pfnCreatePseudoConsole = reinterpret_cast<PFNCREATEPSEUDOCONSOLE>(
        GetProcAddress(hLibrary, "CreatePseudoConsole"));

    if (!pfnCreatePseudoConsole) {
        std::cerr << "Failed to get CreatePseudoConsole function" << std::endl;
        FreeLibrary(hLibrary);
        return false;
    }

    COORD size = { cols, rows };
    HRESULT hr = pfnCreatePseudoConsole(size, hPipeIn, hPipeOut, 0, &hPC);
    
    FreeLibrary(hLibrary);

    if (FAILED(hr)) {
        std::cerr << "Failed to create pseudo console: 0x" << std::hex << hr << std::endl;
        return false;
    }

    return true;
}

bool ConPTY::Start(const std::wstring& command) {
    if (!isInitialized) {
        return false;
    }

    STARTUPINFOEX siEx = { 0 };
    siEx.StartupInfo.cb = sizeof(STARTUPINFOEX);

    SIZE_T size;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
    siEx.lpAttributeList = (PPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(
        GetProcessHeap(), 0, size);

    if (!InitializeProcThreadAttributeList(siEx.lpAttributeList, 1, 0, &size)) {
        return false;
    }

    if (!UpdateProcThreadAttribute(
        siEx.lpAttributeList,
        0,
        PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
        hPC,
        sizeof(HPCON),
        nullptr,
        nullptr)) {
        HeapFree(GetProcessHeap(), 0, siEx.lpAttributeList);
        return false;
    }

    PROCESS_INFORMATION pi = { 0 };
    WCHAR cmdline[MAX_PATH];
    wcscpy_s(cmdline, command.c_str());

    BOOL success = CreateProcessW(
        nullptr,
        cmdline,
        nullptr,
        nullptr,
        FALSE,
        EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &siEx.StartupInfo,
        &pi);

    if (success) {
        hProcess = pi.hProcess;
        hThread = pi.hThread;
        processId = pi.dwProcessId;
    }

    HeapFree(GetProcessHeap(), 0, siEx.lpAttributeList);
    return success ? true : false;
}

bool ConPTY::Write(const char* data, DWORD length, DWORD* written) {
    if (!isInitialized) return false;

    // Convertir en UTF-16 pour Windows
    int wlen = MultiByteToWideChar(CP_UTF8, 0, data, length, NULL, 0);
    std::vector<WCHAR> wdata(wlen);
    MultiByteToWideChar(CP_UTF8, 0, data, length, wdata.data(), wlen);
    
    // Convertir en retour en UTF-8 pour le pipe
    int utf8len = WideCharToMultiByte(CP_UTF8, 0, wdata.data(), wlen, NULL, 0, NULL, NULL);
    std::vector<char> utf8data(utf8len);
    WideCharToMultiByte(CP_UTF8, 0, wdata.data(), wlen, utf8data.data(), utf8len, NULL, NULL);
    
    return WriteFile(hPipeOut, utf8data.data(), utf8len, written, nullptr);
}

bool ConPTY::Read(char* data, DWORD length, DWORD* read) {
    if (!isInitialized) return false;

    DWORD bytesRead = 0;
    bool success = ReadFile(hPipeIn, data, length, &bytesRead, nullptr);
    
    if (success && bytesRead > 0) {
        *read = bytesRead;
    } else {
        *read = 0;
    }
    
    return success;
}

bool ConPTY::Resize(SHORT cols, SHORT rows) {
    if (!isInitialized) return false;

    HMODULE hLibrary = LoadLibraryExW(L"kernel32.dll", nullptr, 0);
    if (!hLibrary) return false;

    typedef HRESULT (*PFNRESIZEPSEUDOCONSOLE)(HPCON hPC, COORD size);
    auto pfnResizePseudoConsole = reinterpret_cast<PFNRESIZEPSEUDOCONSOLE>(
        GetProcAddress(hLibrary, "ResizePseudoConsole"));

    if (!pfnResizePseudoConsole) {
        FreeLibrary(hLibrary);
        return false;
    }

    COORD size = { cols, rows };
    HRESULT hr = pfnResizePseudoConsole(hPC, size);
    
    FreeLibrary(hLibrary);
    return SUCCEEDED(hr);
}

void ConPTY::Close() {
    if (hPC != nullptr) {
        HMODULE hLibrary = LoadLibraryExW(L"kernel32.dll", nullptr, 0);
        if (hLibrary) {
            typedef void (*PFNCLOSEPSEUDOCONSOLE)(HPCON hPC);
            auto pfnClosePseudoConsole = reinterpret_cast<PFNCLOSEPSEUDOCONSOLE>(
                GetProcAddress(hLibrary, "ClosePseudoConsole"));
            
            if (pfnClosePseudoConsole) {
                pfnClosePseudoConsole(hPC);
            }
            FreeLibrary(hLibrary);
        }
        hPC = nullptr;
    }

    if (hProcess != INVALID_HANDLE_VALUE) {
        TerminateProcess(hProcess, 0);
        CloseHandle(hProcess);
        hProcess = INVALID_HANDLE_VALUE;
    }

    if (hThread != INVALID_HANDLE_VALUE) {
        CloseHandle(hThread);
        hThread = INVALID_HANDLE_VALUE;
    }

    if (hPipeIn != INVALID_HANDLE_VALUE) {
        CloseHandle(hPipeIn);
        hPipeIn = INVALID_HANDLE_VALUE;
    }

    if (hPipeOut != INVALID_HANDLE_VALUE) {
        CloseHandle(hPipeOut);
        hPipeOut = INVALID_HANDLE_VALUE;
    }

    processId = 0;
    isInitialized = false;
}

} // namespace conpty