#include <nan.h>
#include <string>
#include <memory>
#include <windows.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <mutex>
#include <thread>
#include <vector>
#include "Logger/logger.h"

class Terminal : public Nan::ObjectWrap {
public:
    static NAN_MODULE_INIT(Init) {
        v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
        tpl->SetClassName(Nan::New("Terminal").ToLocalChecked());
        tpl->InstanceTemplate()->SetInternalFieldCount(1);

        Nan::SetPrototypeMethod(tpl, "write", Write);
        Nan::SetPrototypeMethod(tpl, "onData", OnData);
        Nan::SetPrototypeMethod(tpl, "destroy", Destroy);

        constructor().Reset(Nan::GetFunction(tpl).ToLocalChecked());
        Nan::Set(target, Nan::New("Terminal").ToLocalChecked(),
                 Nan::GetFunction(tpl).ToLocalChecked());
    }

private:
    explicit Terminal(const std::wstring& initialPath = L"") : 
        isRunning(false), hStdin(NULL), hStdout(NULL), readThread(NULL) {
        
        InitializeCriticalSection(&criticalSection);
        Logger::Info("Starting terminal initialization");

        // Création des pipes
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = NULL;

        // Créer les pipes pour stdin et stdout
        if (!CreatePipe(&childStdin_Read, &childStdin_Write, &sa, 0)) {
            Logger::Error("StdIn CreatePipe failed");
            return;
        }
        if (!CreatePipe(&childStdout_Read, &childStdout_Write, &sa, 0)) {
            Logger::Error("StdOut CreatePipe failed");
            CloseHandle(childStdin_Read);
            CloseHandle(childStdin_Write);
            return;
        }

        // Assurer que le côté écriture de stdin et le côté lecture de stdout ne sont pas hérités
        if (!SetHandleInformation(childStdin_Write, HANDLE_FLAG_INHERIT, 0) ||
            !SetHandleInformation(childStdout_Read, HANDLE_FLAG_INHERIT, 0)) {
            Logger::Error("SetHandleInformation failed");
            return;
        }

        STARTUPINFOW si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        ZeroMemory(&pi, sizeof(pi));
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = childStdin_Read;
        si.hStdOutput = childStdout_Write;
        si.hStdError = childStdout_Write;

        std::wstring cmdLine = L"powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass";

        Logger::Info("Launching PowerShell...");
        BOOL success = CreateProcessW(
            NULL,
            (LPWSTR)cmdLine.c_str(),
            NULL,
            NULL,
            TRUE,
            CREATE_NO_WINDOW,
            NULL,
            initialPath.empty() ? NULL : initialPath.c_str(),
            &si,
            &pi
        );

        if (!success) {
            Logger::Error("CreateProcess failed with error: " + std::to_string(GetLastError()));
            return;
        }

        // Stocker les handles dont nous avons besoin
        hStdin = childStdin_Write;
        hStdout = childStdout_Read;
        processHandle = pi.hProcess;
        threadHandle = pi.hThread;
        isRunning = true;

        // Fermer les handles dont nous n'avons plus besoin
        CloseHandle(childStdin_Read);
        CloseHandle(childStdout_Write);

        // Démarrer le thread de lecture
        readThread = CreateThread(NULL, 0, ReadThread, this, 0, NULL);

        // Envoyer les commandes d'initialisation
        const char *initCmds[] = {
            "$Host.UI.RawUI.WindowTitle='Terminal Web';\r\n",
            "$OutputEncoding = [System.Text.Encoding]::UTF8;\r\n",
            "[Console]::OutputEncoding = [System.Text.Encoding]::UTF8;\r\n",
            "function Prompt { 'PS ' + (Get-Location).Path + '> ' };\r\n",
            "cls;\r\n"
        };

        // Envoyer chaque commande d'initialisation
        for (const char* cmd : initCmds) {
            DWORD written;
            WriteFile(hStdin, cmd, strlen(cmd), &written, NULL);
            FlushFileBuffers(hStdin);
            Sleep(100);
        }

        // Envoyer la commande cd séparément
        if (!initialPath.empty()) {
            // Convertir le chemin wide en UTF-8
            int size_needed = WideCharToMultiByte(CP_UTF8, 0, initialPath.c_str(), -1, NULL, 0, NULL, NULL);
            std::vector<char> pathBuffer(size_needed);
            WideCharToMultiByte(CP_UTF8, 0, initialPath.c_str(), -1, pathBuffer.data(), size_needed, NULL, NULL);
            
            std::string cdCommand = "cd '" + std::string(pathBuffer.data()) + "';\r\n";
            DWORD written;
            WriteFile(hStdin, cdCommand.c_str(), cdCommand.length(), &written, NULL);
            FlushFileBuffers(hStdin);
        }

        Logger::Info("Terminal initialization complete");
    }

    ~Terminal() {
        Logger::Info("Terminal destructor called");
        isRunning = false;
        EnterCriticalSection(&criticalSection);

        if (readThread) {
            WaitForSingleObject(readThread, 1000);
            CloseHandle(readThread);
        }

        if (processHandle) CloseHandle(processHandle);
        if (threadHandle) CloseHandle(threadHandle);
        if (hStdin) CloseHandle(hStdin);
        if (hStdout) CloseHandle(hStdout);

        LeaveCriticalSection(&criticalSection);
        DeleteCriticalSection(&criticalSection);
    }

    static DWORD WINAPI ReadThread(LPVOID param) {
        Terminal* terminal = static_cast<Terminal*>(param);
        char buffer[4096];
        DWORD bytesRead;

        Logger::Info("ReadThread started");

        while (terminal->isRunning) {
            if (ReadFile(terminal->hStdout, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                EnterCriticalSection(&terminal->criticalSection);
                
                buffer[bytesRead] = '\0';
                Logger::Debug("Read from PowerShell: " + std::string(buffer));

                v8::Isolate* isolate = v8::Isolate::GetCurrent();
                if (isolate && !terminal->dataCallback.IsEmpty()) {
                    v8::HandleScope scope(isolate);
                    v8::Local<v8::Value> argv[] = { Nan::New(buffer).ToLocalChecked() };
                    Nan::AsyncResource async("Terminal:Read");
                    terminal->dataCallback.Call(1, argv, &async);
                }

                LeaveCriticalSection(&terminal->criticalSection);
            }
            Sleep(10);
        }
        return 0;
    }

    static NAN_METHOD(Write) {
        Terminal* terminal = Nan::ObjectWrap::Unwrap<Terminal>(info.Holder());
        if (!info[0]->IsString()) {
            Logger::Error("Invalid argument type in Write");
            return;
        }

        if (!terminal->isRunning || terminal->hStdin == INVALID_HANDLE_VALUE) {
            Logger::Error("Terminal not running or invalid handle");
            return;
        }

        v8::String::Utf8Value data(v8::Isolate::GetCurrent(), info[0]);
        std::string command(*data);
        Logger::Info("Received command: " + command);

        // Vérifier si le processus PowerShell est toujours en cours d'exécution
        DWORD exitCode = 0;
        if (GetExitCodeProcess(terminal->processHandle, &exitCode) && exitCode != STILL_ACTIVE) {
            Logger::Error("PowerShell process is not running (Exit code: " + std::to_string(exitCode) + ")");
            return;
        }

        if (!command.empty()) {
            // Ajouter \r\n si nécessaire
            if (command.back() != '\n') {
                command += "\r\n";
            }

            DWORD bytesWritten = 0;
            BOOL success = WriteFile(
                terminal->hStdin,
                command.c_str(),
                static_cast<DWORD>(command.length()),
                &bytesWritten,
                NULL
            );

            if (!success) {
                DWORD error = GetLastError();
                Logger::Error("WriteFile failed - Error code: " + std::to_string(error));
            } else {
                Logger::Info("Successfully wrote " + std::to_string(bytesWritten) + " bytes");
                FlushFileBuffers(terminal->hStdin);
            }
        }

        info.GetReturnValue().Set(Nan::True());
    }

    static NAN_METHOD(OnData) {
        Terminal* terminal = Nan::ObjectWrap::Unwrap<Terminal>(info.Holder());
        if (info[0]->IsFunction()) {
            terminal->dataCallback.Reset(info[0].As<v8::Function>());
        }
    }

    static NAN_METHOD(Destroy) {
        Terminal* terminal = Nan::ObjectWrap::Unwrap<Terminal>(info.Holder());
        terminal->isRunning = false;
        terminal->dataCallback.Reset();
    }

    static NAN_METHOD(New) {
        if (!info.IsConstructCall()) {
            return Nan::ThrowError("Constructor must be called with new");
        }

        std::wstring initialPath;
        if (info.Length() > 0 && info[0]->IsObject()) {
            v8::Local<v8::Object> options = Nan::To<v8::Object>(info[0]).ToLocalChecked();
            v8::Local<v8::String> cwd_key = Nan::New("cwd").ToLocalChecked();
            
            if (Nan::Has(options, cwd_key).FromJust()) {
                v8::String::Value cwdValue(v8::Isolate::GetCurrent(),
                                         Nan::Get(options, cwd_key).ToLocalChecked());
                if (*cwdValue) {
                    initialPath = std::wstring(reinterpret_cast<const wchar_t*>(*cwdValue));
                }
            }
        }

        Terminal* obj = new Terminal(initialPath);
        obj->Wrap(info.This());
        info.GetReturnValue().Set(info.This());
    }

    static inline Nan::Persistent<v8::Function>& constructor() {
        static Nan::Persistent<v8::Function> my_constructor;
        return my_constructor;
    }

    bool isRunning;
    HANDLE hStdin;
    HANDLE hStdout;
    HANDLE readThread;
    HANDLE processHandle;
    HANDLE threadHandle;
    HANDLE childStdin_Read;
    HANDLE childStdin_Write;
    HANDLE childStdout_Read;
    HANDLE childStdout_Write;
    Nan::Callback dataCallback;
    CRITICAL_SECTION criticalSection;
};

NODE_MODULE(terminal, Terminal::Init)