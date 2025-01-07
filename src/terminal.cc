// terminal.cc
#include <nan.h>
#include <string>
#include <memory>
#include <windows.h>
#include <iostream>
#include <fstream>
#include <ctime>
#include <sstream>
#include <mutex>
#include <thread>
#include "Logger/logger.h"

class Terminal : public Nan::ObjectWrap {
public:
    static NAN_MODULE_INIT(Init) {
        v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
        tpl->SetClassName(Nan::New("Terminal").ToLocalChecked());
        tpl->InstanceTemplate()->SetInternalFieldCount(1);

        Nan::SetPrototypeMethod(tpl, "write", Write);
        Nan::SetPrototypeMethod(tpl, "onData", OnData);
        Nan::SetPrototypeMethod(tpl, "resize", Resize);
        Nan::SetPrototypeMethod(tpl, "destroy", Destroy);

        constructor().Reset(Nan::GetFunction(tpl).ToLocalChecked());
        Nan::Set(target, Nan::New("Terminal").ToLocalChecked(),
                 Nan::GetFunction(tpl).ToLocalChecked());
    }

private:
    explicit Terminal(int cols = 80, int rows = 24, const std::wstring& initialPath = L"")
        : cols(cols), rows(rows), isRunning(false),
          hStdin(NULL), hStdout(NULL), hProcess(NULL),
          hConsole(NULL), readThread(NULL),
          workingDirectory(initialPath) {
        
        InitializeCriticalSection(&criticalSection);
        Logger::Info("Terminal constructor called");

        SECURITY_ATTRIBUTES sa;
        STARTUPINFOW si;
        HANDLE hStdinRead = NULL, hStdoutWrite = NULL;

        ZeroMemory(&sa, sizeof(sa));
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;

        if (!CreatePipe(&hStdin, &hStdoutWrite, &sa, 65536) ||
            !CreatePipe(&hStdinRead, &hStdout, &sa, 65536)) {
            Logger::Error("Failed to create pipes");
            return;
        }

        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(STARTUPINFOW);
        si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        si.hStdInput = hStdinRead;
        si.hStdOutput = hStdoutWrite;
        si.hStdError = hStdoutWrite;

        WCHAR cmdLine[] = L"cmd.exe";
        ZeroMemory(&pi, sizeof(pi));

        if (CreateProcessW(NULL, cmdLine, NULL, NULL, TRUE,
                         CREATE_NEW_CONSOLE, NULL,
                         workingDirectory.empty() ? NULL : workingDirectory.c_str(),
                         &si, &pi)) {
            
            isRunning = true;

            // Configuration des modes de la console
            DWORD mode;
            if (GetConsoleMode(hStdin, &mode)) {
                mode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
                mode |= ENABLE_VIRTUAL_TERMINAL_INPUT | ENABLE_PROCESSED_INPUT;
                SetConsoleMode(hStdin, mode);
            }

            if (GetConsoleMode(hStdout, &mode)) {
                mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING |
                       ENABLE_PROCESSED_OUTPUT |
                       ENABLE_WRAP_AT_EOL_OUTPUT;
                SetConsoleMode(hStdout, mode);
            }

            readThread = CreateThread(NULL, 0, ReadThread, this, 0, NULL);

            const char* initCommands[] = {
                "@echo off\r\n",
                "prompt $P$G\r\n",
                "cls\r\n"
            };

            for (const char* cmd : initCommands) {
                DWORD written;
                WriteFile(hStdin, cmd, strlen(cmd), &written, NULL);
                FlushFileBuffers(hStdin);
            }

            // Configuration de la taille de la console
            if (HANDLE consoleHandle = CreateFileW(L"CONOUT$",
                                            GENERIC_READ | GENERIC_WRITE,
                                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                                            NULL, OPEN_EXISTING, 0, NULL)) {
                
                COORD size = {static_cast<SHORT>(cols), static_cast<SHORT>(rows)};
                SMALL_RECT rect = {0, 0,
                                 static_cast<SHORT>(cols - 1),
                                 static_cast<SHORT>(rows - 1)};

                SetConsoleScreenBufferSize(consoleHandle, size);
                SetConsoleWindowInfo(consoleHandle, TRUE, &rect);
                CloseHandle(consoleHandle);
            }
        } else {
            Logger::Error("Failed to create process");
        }

        if (hStdinRead) CloseHandle(hStdinRead);
        if (hStdoutWrite) CloseHandle(hStdoutWrite);
    }

    ~Terminal() {
        isRunning = false;
        EnterCriticalSection(&criticalSection);

        if (readThread) {
            WaitForSingleObject(readThread, 1000);
            CloseHandle(readThread);
        }

        if (pi.hProcess) {
            TerminateProcess(pi.hProcess, 0);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }

        if (hStdin) CloseHandle(hStdin);
        if (hStdout) CloseHandle(hStdout);
        if (hConsole) CloseHandle(hConsole);

        LeaveCriticalSection(&criticalSection);
        DeleteCriticalSection(&criticalSection);
    }

    static DWORD WINAPI ReadThread(LPVOID param) {
        Terminal* terminal = static_cast<Terminal*>(param);
        char buffer[4096];
        DWORD bytesRead;

        Logger::Debug("Read thread started");

        while (terminal->isRunning) {
            if (ReadFile(terminal->hStdout, buffer, sizeof(buffer) - 1, &bytesRead, NULL)) {
                if (bytesRead > 0) {
                    EnterCriticalSection(&terminal->criticalSection);

                    buffer[bytesRead] = '\0';
                    v8::Isolate* isolate = v8::Isolate::GetCurrent();

                    if (isolate && !terminal->dataCallback.IsEmpty()) {
                        v8::HandleScope scope(isolate);
                        v8::Local<v8::Value> argv[] = {
                            Nan::New(buffer).ToLocalChecked()
                        };
                        Nan::AsyncResource async("Terminal:Read");
                        terminal->dataCallback.Call(1, argv, &async);
                    }

                    LeaveCriticalSection(&terminal->criticalSection);
                }
            }
            Sleep(1);
        }
        return 0;
    }

    static NAN_METHOD(New);
    static NAN_METHOD(Write);
    static NAN_METHOD(OnData);
    static NAN_METHOD(Resize);
    static NAN_METHOD(Destroy);

    static inline Nan::Persistent<v8::Function>& constructor() {
        static Nan::Persistent<v8::Function> my_constructor;
        return my_constructor;
    }

    HANDLE hStdin;
    HANDLE hStdout;
    HANDLE hProcess;
    bool isRunning;
    PROCESS_INFORMATION pi;
    HANDLE hConsole;
    Nan::Callback dataCallback;
    int cols;
    int rows;
    std::wstring workingDirectory;
    HANDLE readThread;
    CRITICAL_SECTION criticalSection;
};

// Implémentation des méthodes statiques de Terminal
NAN_METHOD(Terminal::New) {
    if (!info.IsConstructCall()) {
        return Nan::ThrowError("Constructor must be called with new");
    }

    int cols = 80, rows = 24;
    std::wstring initialPath;

    if (info.Length() > 0 && info[0]->IsObject()) {
        v8::Local<v8::Object> options = Nan::To<v8::Object>(info[0]).ToLocalChecked();

        v8::Local<v8::String> cols_key = Nan::New("cols").ToLocalChecked();
        v8::Local<v8::String> rows_key = Nan::New("rows").ToLocalChecked();
        v8::Local<v8::String> cwd_key = Nan::New("cwd").ToLocalChecked();

        if (Nan::Has(options, cols_key).FromJust()) {
            cols = Nan::To<int32_t>(Nan::Get(options, cols_key).ToLocalChecked()).FromJust();
        }
        if (Nan::Has(options, rows_key).FromJust()) {
            rows = Nan::To<int32_t>(Nan::Get(options, rows_key).ToLocalChecked()).FromJust();
        }
        if (Nan::Has(options, cwd_key).FromJust()) {
            v8::String::Value cwdValue(v8::Isolate::GetCurrent(),
                                     Nan::Get(options, cwd_key).ToLocalChecked());
            if (*cwdValue) {
                initialPath = std::wstring(reinterpret_cast<const wchar_t*>(*cwdValue));
            }
        }
    }

    Terminal* obj = new Terminal(cols, rows, initialPath);
    obj->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
}

NAN_METHOD(Terminal::Write) {
    Terminal* terminal = Nan::ObjectWrap::Unwrap<Terminal>(info.Holder());

    if (!info[0]->IsString()) {
        return Nan::ThrowError("Data argument must be a string");
    }

    v8::String::Utf8Value data(v8::Isolate::GetCurrent(), info[0]);
    if (!terminal->isRunning || terminal->hStdin == INVALID_HANDLE_VALUE) {
        return;
    }

    EnterCriticalSection(&terminal->criticalSection);

    std::string command(*data);
    if (!command.empty() && (command.back() != '\n' && command.back() != '\r')) {
        command += "\r\n";
    }

    DWORD written;
    WriteFile(terminal->hStdin, command.c_str(), command.length(), &written, NULL);
    FlushFileBuffers(terminal->hStdin);

    LeaveCriticalSection(&terminal->criticalSection);

    Logger::Info("Data written to terminal");
    info.GetReturnValue().Set(Nan::True());
}

NAN_METHOD(Terminal::OnData) {
    Terminal* terminal = Nan::ObjectWrap::Unwrap<Terminal>(info.Holder());

    if (!info[0]->IsFunction()) {
        return Nan::ThrowError("Callback must be a function");
    }

    terminal->dataCallback.Reset(info[0].As<v8::Function>());
}

NAN_METHOD(Terminal::Resize) {
    Terminal* terminal = Nan::ObjectWrap::Unwrap<Terminal>(info.Holder());

    if (info.Length() < 2) {
        return Nan::ThrowError("Columns and rows are required");
    }

    terminal->cols = Nan::To<int32_t>(info[0]).FromJust();
    terminal->rows = Nan::To<int32_t>(info[1]).FromJust();

    HANDLE consoleHandle = CreateFileW(L"CONOUT$",
                                     GENERIC_READ | GENERIC_WRITE,
                                     FILE_SHARE_READ | FILE_SHARE_WRITE,
                                     NULL, OPEN_EXISTING, 0, NULL);

    if (consoleHandle != INVALID_HANDLE_VALUE) {
        EnterCriticalSection(&terminal->criticalSection);

        COORD size = {static_cast<SHORT>(terminal->cols),
                     static_cast<SHORT>(terminal->rows)};
        SMALL_RECT rect = {0, 0,
                          static_cast<SHORT>(terminal->cols - 1),
                          static_cast<SHORT>(terminal->rows - 1)};

        SetConsoleScreenBufferSize(consoleHandle, size);
        SetConsoleWindowInfo(consoleHandle, TRUE, &rect);

        CloseHandle(consoleHandle);
        LeaveCriticalSection(&terminal->criticalSection);
    }
}

NAN_METHOD(Terminal::Destroy) {
    Terminal* terminal = Nan::ObjectWrap::Unwrap<Terminal>(info.Holder());
    terminal->isRunning = false;
    terminal->dataCallback.Reset();
}

NODE_MODULE(terminal, Terminal::Init)