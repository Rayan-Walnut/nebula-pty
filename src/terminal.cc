<<<<<<< HEAD
#include "terminal.h"
#include "win/conpty.h"
#include "win/path_util.h"
#include <iostream>
#include <thread>
#include <vector>

namespace terminal {

WebTerminal::WebTerminal(const Napi::CallbackInfo& info) 
    : Napi::ObjectWrap<WebTerminal>(info),
    pty(std::make_unique<conpty::ConPTY>()),
    running(false) {
    std::cout << "Terminal constructor called" << std::endl;
}

WebTerminal::~WebTerminal() {
    std::cout << "Terminal destructor called" << std::endl;
    running = false;
    if (readThread.joinable()) {
        readThread.join();
    }
    pty->Close();
}

Napi::Object WebTerminal::Init(Napi::Env env, Napi::Object exports) {
    Napi::Function func = DefineClass(env, "WebTerminal", {
        InstanceMethod("startProcess", &WebTerminal::StartProcess),
        InstanceMethod("write", &WebTerminal::Write),
        InstanceMethod("onData", &WebTerminal::OnData),
        InstanceMethod("resize", &WebTerminal::Resize),
        InstanceMethod("echo", &WebTerminal::Echo)
    });

    Napi::FunctionReference* constructor = new Napi::FunctionReference();
    *constructor = Napi::Persistent(func);
    env.SetInstanceData(constructor);

    exports.Set("WebTerminal", func);
    return exports;
}

void WebTerminal::ReadLoop() {
    std::cout << "ReadLoop started..." << std::endl;
    const DWORD bufSize = 4096;
    std::vector<char> buffer(bufSize);
    DWORD bytesRead;

    while (running) {
        if (pty->Read(buffer.data(), bufSize - 1, &bytesRead)) {
            if (bytesRead > 0) {
                std::cout << "Read " << bytesRead << " bytes from PTY" << std::endl;
                buffer[bytesRead] = '\0';
                std::string data(buffer.data(), bytesRead);
                
                auto callback = [](Napi::Env env, Napi::Function jsCallback, std::string* data) {
                    std::cout << "Calling JS callback with data: " << *data << std::endl;
                    jsCallback.Call({Napi::String::New(env, *data)});
                    delete data;
                };

                tsfn.NonBlockingCall(new std::string(data), callback);
            }
        } else {
            DWORD error = GetLastError();
            if (error != ERROR_NO_DATA) {  // Ignorer les erreurs de "pas de données"
                std::cout << "Read failed with error: " << error << std::endl;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    std::cout << "ReadLoop ending..." << std::endl;
    tsfn.Release();
}

Napi::Value WebTerminal::Echo(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    if (info.Length() < 1 || !info[0].IsString()) {
        Napi::TypeError::New(env, "String expected").ThrowAsJavaScriptException();
        return env.Null();
    }

    return info[0];
}

Napi::Value WebTerminal::StartProcess(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    try {
        SHORT width = 80, height = 24;
        if (info.Length() > 0 && info[0].IsObject()) {
            Napi::Object options = info[0].As<Napi::Object>();
            if (options.Has("cols")) {
                width = static_cast<SHORT>(options.Get("cols").As<Napi::Number>().Int32Value());
            }
            if (options.Has("rows")) {
                height = static_cast<SHORT>(options.Get("rows").As<Napi::Number>().Int32Value());
            }
        }

        std::cout << "Creating PTY with size: " << width << "x" << height << std::endl;

        if (!pty->Create(width, height)) {
            std::cout << "ConPTY creation failed with error: " << GetLastError() << std::endl;
            throw std::runtime_error("Failed to create pseudo console");
        }

        std::wstring shellPath = path_util::GetShellPath();
        std::cout << "Starting shell at: " << std::string(shellPath.begin(), shellPath.end()) << std::endl;
        
        running = true;  // Mettre running à true AVANT de démarrer le processus
        
        if (!pty->Start(shellPath)) {
            running = false;
            std::cout << "Failed to start shell with error: " << GetLastError() << std::endl;
            throw std::runtime_error("Failed to start process");
        }

        return env.Undefined();
    }
    catch (const std::exception& e) {
        running = false;
        std::cerr << "Error in StartProcess: " << e.what() << std::endl;
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value WebTerminal::Write(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsString()) {
        Napi::TypeError::New(env, "String expected").ThrowAsJavaScriptException();
        return env.Null();
    }

    try {
        std::string data = info[0].As<Napi::String>().Utf8Value();
        std::cout << "Writing to PTY: " << data << std::endl;
        
        DWORD written;
        if (!pty->Write(data.c_str(), static_cast<DWORD>(data.length()), &written)) {
            std::cout << "Write failed with error: " << GetLastError() << std::endl;
            throw std::runtime_error("Write failed");
        }
        std::cout << "Successfully wrote " << written << " bytes" << std::endl;

        return env.Undefined();
    }
    catch (const std::exception& e) {
        std::cerr << "Error in Write: " << e.what() << std::endl;
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value WebTerminal::OnData(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsFunction()) {
        Napi::TypeError::New(env, "Function expected").ThrowAsJavaScriptException();
        return env.Null();
    }

    std::cout << "Setting up data callback" << std::endl;
    tsfn = Napi::ThreadSafeFunction::New(
        env,
        info[0].As<Napi::Function>(),
        "Terminal Callback",
        0,
        1
    );

    readThread = std::thread([this]() {
        this->ReadLoop();
    });

    return env.Undefined();
}

Napi::Value WebTerminal::Resize(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
        Napi::TypeError::New(env, "Numbers expected for columns and rows").ThrowAsJavaScriptException();
        return env.Null();
    }

    try {
        SHORT cols = static_cast<SHORT>(info[0].As<Napi::Number>().Int32Value());
        SHORT rows = static_cast<SHORT>(info[1].As<Napi::Number>().Int32Value());

        if (!pty->Resize(cols, rows)) {
            throw std::runtime_error("Resize failed");
        }

        return env.Undefined();
    }
    catch (const std::exception& e) {
        std::cerr << "Error in Resize: " << e.what() << std::endl;
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

} // namespace terminal

// Module initialization
Napi::Object Init(Napi::Env env, Napi::Object exports) {
    return terminal::WebTerminal::Init(env, exports);
}

NODE_API_MODULE(terminal, Init)
=======
#include <nan.h>
#include <windows.h>
#include <wincon.h>
#include <processthreadsapi.h>
#include <string>
#include <atomic>
#include <filesystem>

// ConPTY related definitions
#ifndef PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
#define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE ProcThreadAttributeValue(22, FALSE, TRUE, FALSE)
#endif

// Function pointer types for ConPTY APIs
using CreatePseudoConsoleFn = HRESULT (*)(COORD, HANDLE, HANDLE, DWORD, HPCON *);
using ResizePseudoConsoleFn = HRESULT (*)(HPCON, COORD);
using ClosePseudoConsoleFn = void (*)(HPCON);

class Terminal : public Nan::ObjectWrap
{
private:
    struct ConPTYAPI
    {
        CreatePseudoConsoleFn CreatePseudoConsole;
        ResizePseudoConsoleFn ResizePseudoConsole;
        ClosePseudoConsoleFn ClosePseudoConsole;

        ConPTYAPI()
        {
            HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
            if (!hKernel32)
                throw std::runtime_error("Failed to get kernel32.dll handle");

            CreatePseudoConsole = (CreatePseudoConsoleFn)GetProcAddress(hKernel32, "CreatePseudoConsole");
            ResizePseudoConsole = (ResizePseudoConsoleFn)GetProcAddress(hKernel32, "ResizePseudoConsole");
            ClosePseudoConsole = (ClosePseudoConsoleFn)GetProcAddress(hKernel32, "ClosePseudoConsole");

            if (!CreatePseudoConsole || !ResizePseudoConsole || !ClosePseudoConsole)
            {
                throw std::runtime_error("ConPTY APIs not available on this Windows version");
            }
        }
    };

    HPCON hPC = nullptr;
    HANDLE hPipeIn = INVALID_HANDLE_VALUE;
    HANDLE hPipeOut = INVALID_HANDLE_VALUE;
    PROCESS_INFORMATION pi = {0};
    COORD consoleSize = {80, 30};
    std::atomic<bool> isRunning;
    std::string workingDirectory;
    Nan::Callback dataCallback;
    std::unique_ptr<ConPTYAPI> conPTY;

    explicit Terminal(const char *initialPath = nullptr) : isRunning(true)
    {
        try
        {
            conPTY = std::make_unique<ConPTYAPI>();
            SetInitialPath(initialPath);
            InitializePseudoConsole();
        }
        catch (const std::exception &e)
        {
            Cleanup();
            throw;
        }
    }

    void SetInitialPath(const char *path)
    {
        if (!path || !*path)
            return;

        std::filesystem::path fsPath(path);
        if (!std::filesystem::exists(fsPath) || !std::filesystem::is_directory(fsPath))
        {
            throw std::runtime_error("Invalid directory path");
        }
        workingDirectory = std::filesystem::absolute(fsPath).string();
    }

    void InitializePseudoConsole()
    {
        SECURITY_ATTRIBUTES sa = {sizeof(sa), nullptr, TRUE};
        HANDLE hPipeInRead = INVALID_HANDLE_VALUE;
        HANDLE hPipeOutWrite = INVALID_HANDLE_VALUE;

        try
        {
            if (!CreatePipe(&hPipeInRead, &hPipeIn, &sa, 0) ||
                !CreatePipe(&hPipeOut, &hPipeOutWrite, &sa, 0))
            {
                throw std::runtime_error("Failed to create pipes");
            }

            HRESULT hr = conPTY->CreatePseudoConsole(
                consoleSize, hPipeInRead, hPipeOutWrite, 0, &hPC);
            if (FAILED(hr))
                throw std::runtime_error("Failed to create pseudo console");

            SetHandleInformation(hPipeIn, HANDLE_FLAG_INHERIT, 0);
            SetHandleInformation(hPipeOut, HANDLE_FLAG_INHERIT, 0);
        }
        catch (...)
        {
            if (hPipeInRead != INVALID_HANDLE_VALUE)
                CloseHandle(hPipeInRead);
            if (hPipeOutWrite != INVALID_HANDLE_VALUE)
                CloseHandle(hPipeOutWrite);
            throw;
        }
    }

    static NAN_METHOD(Destroy)
    {
        Terminal *terminal = Nan::ObjectWrap::Unwrap<Terminal>(info.Holder());
        if (terminal)
        {
            terminal->Cleanup();
        }
        info.GetReturnValue().Set(Nan::Undefined());
    }

    bool StartProcess(const std::string &command)
    {
        STARTUPINFOEXW si;
        ZeroMemory(&si, sizeof(si));
        si.StartupInfo.cb = sizeof(STARTUPINFOEXW);

        SIZE_T size;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &size);

        si.lpAttributeList = (PPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(
            GetProcessHeap(), 0, size);
        if (!si.lpAttributeList)
            return false;

        if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &size))
        {
            HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
            return false;
        }

        if (!UpdateProcThreadAttribute(si.lpAttributeList, 0,
                                       PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                                       hPC, sizeof(HPCON), nullptr, nullptr))
        {
            HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
            return false;
        }

        // Préparer la commande
        std::wstring wCommand;
        if (command == "python")
        {
            // Pour Python, on utilise le chemin complet
            wCommand = L"python.exe";
        }
        else
        {
            // Pour les autres commandes
            wCommand = std::wstring(command.begin(), command.end());
        }

        BOOL success = CreateProcessW(
            nullptr,
            &wCommand[0],
            nullptr,
            nullptr,
            TRUE,                         // Modifier ici pour hériter les handles
            EXTENDED_STARTUPINFO_PRESENT, // Enlever CREATE_NO_WINDOW
            nullptr,
            workingDirectory.empty() ? nullptr : std::filesystem::path(workingDirectory).wstring().c_str(),
            &si.StartupInfo,
            &pi);

        DeleteProcThreadAttributeList(si.lpAttributeList);
        HeapFree(GetProcessHeap(), 0, si.lpAttributeList);

        return success != 0;
    }

    void Cleanup()
    {
        isRunning = false;

        if (pi.hProcess)
        {
            TerminateProcess(pi.hProcess, 0);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            ZeroMemory(&pi, sizeof(pi));
        }

        if (hPC)
        {
            conPTY->ClosePseudoConsole(hPC);
            hPC = nullptr;
        }

        for (HANDLE *h : {&hPipeIn, &hPipeOut})
        {
            if (*h != INVALID_HANDLE_VALUE)
            {
                CloseHandle(*h);
                *h = INVALID_HANDLE_VALUE;
            }
        }

        dataCallback.Reset();
    }

    ~Terminal()
    {
        Cleanup();
    }

    // Node.js methods implementations
    static NAN_METHOD(New)
    {
        if (!info.IsConstructCall())
        {
            return Nan::ThrowError("Constructor must be called with new");
        }

        try
        {
            const char *initialPath = nullptr;
            if (info.Length() > 0)
            {
                if (info[0]->IsString())
                {
                    Nan::Utf8String pathStr(info[0]);
                    initialPath = *pathStr;
                }
                else if (info[0]->IsObject())
                {
                    auto options = info[0].As<v8::Object>();
                    auto cwdKey = Nan::New("cwd").ToLocalChecked();
                    if (Nan::Has(options, cwdKey).FromJust())
                    {
                        Nan::Utf8String cwdStr(Nan::Get(options, cwdKey).ToLocalChecked());
                        initialPath = *cwdStr;
                    }
                }
            }

            Terminal *obj = new Terminal(initialPath);
            obj->Wrap(info.This());
            info.GetReturnValue().Set(info.This());
        }
        catch (const std::exception &e)
        {
            return Nan::ThrowError(e.what());
        }
    }

    static NAN_METHOD(Write)
    {
        auto *terminal = Nan::ObjectWrap::Unwrap<Terminal>(info.Holder());
        if (!info[0]->IsString())
        {
            return Nan::ThrowTypeError("Argument must be a string");
        }

        Nan::Utf8String data(info[0]);
        DWORD written;
        if (!WriteFile(terminal->hPipeIn, *data, data.length(), &written, nullptr))
        {
            return Nan::ThrowError("Failed to write to terminal");
        }

        info.GetReturnValue().Set(Nan::New<v8::Number>(written));
    }

    static NAN_METHOD(Read)
    {
        auto *terminal = Nan::ObjectWrap::Unwrap<Terminal>(info.Holder());

        std::string output;
        char buffer[4096];
        DWORD bytesRead;

        while (ReadFile(terminal->hPipeOut, buffer, sizeof(buffer) - 1, &bytesRead, nullptr))
        {
            if (bytesRead == 0)
                break;
            buffer[bytesRead] = '\0';
            output += buffer;
        }

        info.GetReturnValue().Set(Nan::New(output).ToLocalChecked());
    }

    static NAN_METHOD(Resize)
    {
        auto *terminal = Nan::ObjectWrap::Unwrap<Terminal>(info.Holder());

        if (!info[0]->IsNumber() || !info[1]->IsNumber())
        {
            return Nan::ThrowTypeError("Columns and rows must be numbers");
        }

        terminal->consoleSize.X = (SHORT)Nan::To<uint32_t>(info[0]).FromJust();
        terminal->consoleSize.Y = (SHORT)Nan::To<uint32_t>(info[1]).FromJust();

        HRESULT hr = terminal->conPTY->ResizePseudoConsole(terminal->hPC, terminal->consoleSize);
        if (FAILED(hr))
        {
            return Nan::ThrowError("Failed to resize console");
        }

        info.GetReturnValue().Set(Nan::True());
    }

public:
    std::string ExecuteCommand(const std::string &cmd)
    {
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = NULL;

        HANDLE hReadPipe, hWritePipe;
        if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0))
        {
            throw std::runtime_error("Failed to create pipe");
        }

        SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOW si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        ZeroMemory(&pi, sizeof(pi));

        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = NULL;
        si.hStdOutput = hWritePipe;
        si.hStdError = hWritePipe;

        // Configuration PowerShell avec encodage UTF-8
        std::wstring cmdLine = L"powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \"" +
                               std::wstring(L"$OutputEncoding = [System.Text.Encoding]::UTF8; ") +
                               L"[Console]::OutputEncoding = [System.Text.Encoding]::UTF8; " +
                               L"[Console]::InputEncoding = [System.Text.Encoding]::UTF8; " +
                               std::wstring(cmd.begin(), cmd.end()) + L"\"";

        BOOL success = CreateProcessW(
            NULL,
            &cmdLine[0],
            NULL,
            NULL,
            TRUE,
            CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
            NULL,
            workingDirectory.empty() ? NULL : std::filesystem::path(workingDirectory).wstring().c_str(),
            &si,
            &pi);

        if (!success)
        {
            CloseHandle(hReadPipe);
            CloseHandle(hWritePipe);
            throw std::runtime_error("Failed to execute command");
        }

        CloseHandle(hWritePipe);

        std::string output;
        char buffer[4096];
        DWORD bytesRead;

        while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL))
        {
            if (bytesRead == 0)
                break;
            buffer[bytesRead] = '\0';
            output += buffer;
        }

        WaitForSingleObject(pi.hProcess, INFINITE);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hReadPipe);

        return output;
    }

    static NAN_METHOD(ExecuteCommand)
    {
        if (!info[0]->IsString())
        {
            return Nan::ThrowTypeError("Command must be a string");
        }

        Terminal *terminal = Nan::ObjectWrap::Unwrap<Terminal>(info.Holder());
        Nan::Utf8String command(info[0]);

        try
        {
            std::string result = terminal->ExecuteCommand(*command);
            info.GetReturnValue().Set(Nan::New(result).ToLocalChecked());
        }
        catch (const std::exception &e)
        {
            return Nan::ThrowError(e.what());
        }
    }
    static NAN_METHOD(ExecuteInteractive)
    {
        if (!info[0]->IsString())
        {
            return Nan::ThrowTypeError("Command must be a string");
        }

        Terminal *terminal = Nan::ObjectWrap::Unwrap<Terminal>(info.Holder());
        Nan::Utf8String command(info[0]);

        try
        {
            if (!terminal->StartProcess(*command))
            {
                return Nan::ThrowError("Failed to start interactive process");
            }
            info.GetReturnValue().Set(Nan::True());
        }
        catch (const std::exception &e)
        {
            return Nan::ThrowError(e.what());
        }
    }
    static NAN_MODULE_INIT(Init)
    {
        v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
        tpl->SetClassName(Nan::New("Terminal").ToLocalChecked());
        tpl->InstanceTemplate()->SetInternalFieldCount(1);

        Nan::SetPrototypeMethod(tpl, "write", Write);
        Nan::SetPrototypeMethod(tpl, "read", Read);
        Nan::SetPrototypeMethod(tpl, "resize", Resize);
        Nan::SetPrototypeMethod(tpl, "executeCommand", ExecuteCommand);
        Nan::SetPrototypeMethod(tpl, "executeInteractive", ExecuteInteractive);
        Nan::SetPrototypeMethod(tpl, "destroy", Destroy); // Ajout ici

        constructor().Reset(Nan::GetFunction(tpl).ToLocalChecked());
        Nan::Set(target, Nan::New("Terminal").ToLocalChecked(),
                 Nan::GetFunction(tpl).ToLocalChecked());
    }

private:
    static inline Nan::Persistent<v8::Function> &constructor()
    {
        static Nan::Persistent<v8::Function> my_constructor;
        return my_constructor;
    }
};

NODE_MODULE(terminal, Terminal::Init)
>>>>>>> 5b0892aac6276a1b3194bb950640099db31811b9
