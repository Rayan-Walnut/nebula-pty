#include "win/conpty.h"
#include <iostream>
#include <vector>

namespace conpty {

ConPTY::ConPTY() 
    : hPipeIn(INVALID_HANDLE_VALUE)
    , hPipeOut(INVALID_HANDLE_VALUE)
    , hPtyIn(INVALID_HANDLE_VALUE)
    , hPtyOut(INVALID_HANDLE_VALUE)
    , hPC(nullptr)
    , hProcess(INVALID_HANDLE_VALUE)
    , hThread(INVALID_HANDLE_VALUE)
    , processId(0)
    , isInitialized(false) {
}

ConPTY::~ConPTY() {
    Close();
}

bool ConPTY::IsActive() const {
    return (hPC != nullptr);
}

bool ConPTY::CreatePipes() {
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };
    
    // Créer des pipes anonymes
    if (!CreatePipe(&hPtyIn, &hPipeIn, &sa, 0)) {
        std::cerr << "Failed to create input pipe: " << GetLastError() << std::endl;
        return false;
    }

    if (!CreatePipe(&hPipeOut, &hPtyOut, &sa, 0)) {
        CloseHandle(hPtyIn);
        CloseHandle(hPipeIn);
        std::cerr << "Failed to create output pipe: " << GetLastError() << std::endl;
        return false;
    }

    // Configurer les handles pour l'E/S asynchrone
    if (!SetHandleInformation(hPipeIn, HANDLE_FLAG_INHERIT, 0) ||
        !SetHandleInformation(hPipeOut, HANDLE_FLAG_INHERIT, 0)) {
        Close();
        return false;
    }

    return true;
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

bool ConPTY::CreatePseudoConsole(SHORT cols, SHORT rows) {
    HMODULE hLibrary = LoadLibraryExW(L"kernel32.dll", nullptr, 0);
    if (!hLibrary) {
        return false;
    }

    typedef HRESULT (*PFNCREATEPSEUDOCONSOLE)(COORD c, HANDLE hIn, HANDLE hOut, DWORD dwFlags, HPCON* phpcon);
    auto pfnCreatePseudoConsole = reinterpret_cast<PFNCREATEPSEUDOCONSOLE>(
        GetProcAddress(hLibrary, "CreatePseudoConsole"));

    if (!pfnCreatePseudoConsole) {
        FreeLibrary(hLibrary);
        return false;
    }

    COORD size = { cols, rows };
    HRESULT hr = pfnCreatePseudoConsole(size, hPtyIn, hPtyOut, 0, &hPC);
    
    FreeLibrary(hLibrary);

    return SUCCEEDED(hr);
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
        EXTENDED_STARTUPINFO_PRESENT,
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
    if (!isInitialized || hPipeIn == INVALID_HANDLE_VALUE) {
        return false;
    }

    return WriteFile(hPipeIn, data, length, written, nullptr);
}

bool ConPTY::Read(char* data, DWORD length, DWORD* read) {
    if (!isInitialized || hPipeOut == INVALID_HANDLE_VALUE) {
        return false;
    }

    return ReadFile(hPipeOut, data, length, read, nullptr);
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

    if (hPtyIn != INVALID_HANDLE_VALUE) {
        CloseHandle(hPtyIn);
        hPtyIn = INVALID_HANDLE_VALUE;
    }

    if (hPtyOut != INVALID_HANDLE_VALUE) {
        CloseHandle(hPtyOut);
        hPtyOut = INVALID_HANDLE_VALUE;
    }

    processId = 0;
    isInitialized = false;
}

} // namespace conpty