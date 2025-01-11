#include "conpty.h"
#include <iostream>
#include <vector>

// ConPTY API
typedef HRESULT (*PFNCREATEPSEUDOCONSOLE)(COORD size, HANDLE hInput, HANDLE hOutput, DWORD dwFlags, HPCON* phPC);
typedef HRESULT (*PFNRESIZEPSEUDOCONSOLE)(HPCON hPC, COORD size);
typedef void (*PFNCLOSEPSEUDOCONSOLE)(HPCON hPC);

namespace conpty {

ConPTY::ConPTY() : hPC(nullptr), hPipeIn(nullptr), hPipeOut(nullptr), 
    hProcess(nullptr), running(false) {
}

ConPTY::~ConPTY() {
    Close();
}

bool ConPTY::Create(SHORT width, SHORT height) {
    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!hKernel32) return false;

    auto CreatePseudoConsole = reinterpret_cast<PFNCREATEPSEUDOCONSOLE>(
        GetProcAddress(hKernel32, "CreatePseudoConsole"));
    if (!CreatePseudoConsole) return false;

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE hPipeInRead, hPipeInWrite, hPipeOutRead, hPipeOutWrite;

    if (!CreatePipe(&hPipeInRead, &hPipeInWrite, &sa, 0)) return false;
    if (!CreatePipe(&hPipeOutRead, &hPipeOutWrite, &sa, 0)) {
        CloseHandle(hPipeInRead);
        CloseHandle(hPipeInWrite);
        return false;
    }

    COORD size = { width, height };
    HRESULT hr = CreatePseudoConsole(size, hPipeInRead, hPipeOutWrite, 0, &hPC);

    if (FAILED(hr)) {
        CloseHandle(hPipeInRead);
        CloseHandle(hPipeInWrite);
        CloseHandle(hPipeOutRead);
        CloseHandle(hPipeOutWrite);
        return false;
    }

    hPipeIn = hPipeInWrite;
    hPipeOut = hPipeOutRead;
    CloseHandle(hPipeInRead);
    CloseHandle(hPipeOutWrite);

    return true;
}

bool ConPTY::Start(const std::wstring& command) {
    STARTUPINFOEXW si;
    PROCESS_INFORMATION pi;
    SIZE_T size = 0;

    ZeroMemory(&si, sizeof(si));
    si.StartupInfo.cb = sizeof(si);

    InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
    si.lpAttributeList = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(new char[size]);
    
    if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &size)) {
        delete[] reinterpret_cast<char*>(si.lpAttributeList);
        return false;
    }

    if (!UpdateProcThreadAttribute(
        si.lpAttributeList,
        0,
        PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
        hPC,
        sizeof(HPCON),
        nullptr,
        nullptr
    )) {
        DeleteProcThreadAttributeList(si.lpAttributeList);
        delete[] reinterpret_cast<char*>(si.lpAttributeList);
        return false;
    }

    std::vector<wchar_t> cmdline(command.begin(), command.end());
    cmdline.push_back(0);

    if (!CreateProcessW(
        nullptr,
        cmdline.data(),
        nullptr,
        nullptr,
        FALSE,
        EXTENDED_STARTUPINFO_PRESENT,
        nullptr,
        nullptr,
        &si.StartupInfo,
        &pi
    )) {
        DeleteProcThreadAttributeList(si.lpAttributeList);
        delete[] reinterpret_cast<char*>(si.lpAttributeList);
        return false;
    }

    hProcess = pi.hProcess;
    CloseHandle(pi.hThread);
    DeleteProcThreadAttributeList(si.lpAttributeList);
    delete[] reinterpret_cast<char*>(si.lpAttributeList);

    running = true;
    return true;
}

bool ConPTY::Write(const void* data, DWORD length, DWORD* written) {
    return WriteFile(hPipeIn, data, length, written, nullptr);
}

bool ConPTY::Read(void* data, DWORD length, DWORD* read) {
    return ReadFile(hPipeOut, data, length, read, nullptr);
}

bool ConPTY::Resize(SHORT width, SHORT height) {
    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!hKernel32) return false;

    auto ResizePseudoConsole = reinterpret_cast<PFNRESIZEPSEUDOCONSOLE>(
        GetProcAddress(hKernel32, "ResizePseudoConsole"));
    if (!ResizePseudoConsole) return false;

    COORD size = { width, height };
    return SUCCEEDED(ResizePseudoConsole(hPC, size));
}

void ConPTY::Close() {
    running = false;

    if (hProcess) {
        TerminateProcess(hProcess, 0);
        CloseHandle(hProcess);
        hProcess = nullptr;
    }

    if (hPC) {
        HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
        if (hKernel32) {
            auto ClosePseudoConsole = reinterpret_cast<PFNCLOSEPSEUDOCONSOLE>(
                GetProcAddress(hKernel32, "ClosePseudoConsole"));
            if (ClosePseudoConsole) {
                ClosePseudoConsole(hPC);
            }
        }
        hPC = nullptr;
    }

    if (hPipeIn) {
        CloseHandle(hPipeIn);
        hPipeIn = nullptr;
    }

    if (hPipeOut) {
        CloseHandle(hPipeOut);
        hPipeOut = nullptr;
    }
}

void ConPTY::CleanupHandles() {
    if (hPipeIn) CloseHandle(hPipeIn);
    if (hPipeOut) CloseHandle(hPipeOut);
    if (hProcess) CloseHandle(hProcess);
    hPipeIn = nullptr;
    hPipeOut = nullptr;
    hProcess = nullptr;
}

} // namespace conpty