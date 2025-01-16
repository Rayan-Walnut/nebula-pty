#include "terminal.h"
#include "win/conpty.h"
#include <iostream>
#include <thread>
#include <vector>
#include <Windows.h>

WebTerminal::WebTerminal(const Napi::CallbackInfo &info)
    : Napi::ObjectWrap<WebTerminal>(info),
      pty(nullptr),
      running(false),
      initialized(false),
      processId(0)
{
    std::cout << "Terminal constructor called" << std::endl;
    pty = std::make_unique<conpty::ConPTY>();
}

WebTerminal::~WebTerminal()
{
    std::cout << "Terminal destructor called" << std::endl;
    running = false;
    if (readThread.joinable())
    {
        readThread.join();
    }
    if (pty)
    {
        pty->Close();
    }
}

Napi::Object WebTerminal::Init(Napi::Env env, Napi::Object exports)
{
    Napi::Function func = DefineClass(env, "WebTerminal", {
        InstanceMethod("startProcess", &WebTerminal::StartProcess),
        InstanceMethod("write", &WebTerminal::Write),
        InstanceMethod("onData", &WebTerminal::OnData),
        InstanceMethod("resize", &WebTerminal::Resize),
        InstanceMethod("echo", &WebTerminal::Echo)
    });

    Napi::FunctionReference *constructor = new Napi::FunctionReference();
    *constructor = Napi::Persistent(func);
    env.SetInstanceData(constructor);

    exports.Set("WebTerminal", func);
    return exports;
}

void WebTerminal::ReadLoop() {
    if (!pty) {
        std::cerr << "ReadLoop: PTY is null" << std::endl;
        return;
    }

    const DWORD bufSize = 1024;
    std::vector<char> buffer(bufSize);
    DWORD bytesRead;

    while (running.load()) {
        if (pty->Read(buffer.data(), bufSize - 1, &bytesRead)) {
            if (bytesRead > 0) {
                auto callback = [](Napi::Env env, Napi::Function jsCallback, std::vector<char>* data) {
                    if (!data) return;
                    auto buf = Napi::Buffer<char>::Copy(env, data->data(), data->size());
                    jsCallback.Call({buf});
                    delete data;
                };

                std::vector<char>* dataToSend = new std::vector<char>(buffer.begin(), buffer.begin() + bytesRead);
                tsfn.NonBlockingCall(dataToSend, callback);
            }
        } else {
            DWORD error = GetLastError();
            if (error != ERROR_NO_DATA && error != ERROR_BROKEN_PIPE) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }

    try {
        tsfn.Release();
    } catch (const std::exception& e) {
        std::cerr << "Error releasing tsfn: " << e.what() << std::endl;
    }
}

Napi::Value WebTerminal::StartProcess(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (!pty)
    {
        throw Napi::Error::New(env, "PTY not initialized");
    }

    try
    {
        running = true;
        std::cout << "Setting running to true" << std::endl;

        // Valeurs par défaut
        SHORT width = 120, height = 30;
        std::wstring cwd = L"";

        // Traitement des options
        if (info.Length() > 0 && info[0].IsObject())
        {
            Napi::Object options = info[0].As<Napi::Object>();
            
            if (options.Has("cols"))
            {
                width = static_cast<SHORT>(options.Get("cols").As<Napi::Number>().Int32Value());
            }
            if (options.Has("rows"))
            {
                height = static_cast<SHORT>(options.Get("rows").As<Napi::Number>().Int32Value());
            }
            
            // Ajout du support cwd
            if (options.Has("cwd"))
            {
                std::string cwdUtf8 = options.Get("cwd").As<Napi::String>().Utf8Value();
                int wsize = MultiByteToWideChar(CP_UTF8, 0, cwdUtf8.c_str(), -1, NULL, 0);
                std::vector<wchar_t> wbuff(wsize);
                MultiByteToWideChar(CP_UTF8, 0, cwdUtf8.c_str(), -1, wbuff.data(), wsize);
                cwd = std::wstring(wbuff.data());
            }
        }

        std::cout << "Creating PTY with size: " << width << "x" << height << std::endl;

        // Configuration UTF-8
        if (!SetConsoleOutputCP(CP_UTF8) || !SetConsoleCP(CP_UTF8))
        {
            std::cerr << "Failed to set console code page to UTF-8" << std::endl;
        }

        if (!pty->Create(width, height))
        {
            running = false;
            DWORD error = GetLastError();
            std::cout << "ConPTY creation failed with error: " << error << std::endl;
            throw Napi::Error::New(env, "Failed to create pseudo console: " + std::to_string(error));
        }

        std::wstring shellPath = L"powershell.exe";
        std::cout << "Starting shell at: powershell.exe" << std::endl;

        if (!pty->Start(shellPath, cwd))
        {
            running = false;
            DWORD error = GetLastError();
            std::cout << "Failed to start shell with error: " << error << std::endl;
            throw Napi::Error::New(env, "Failed to start process: " + std::to_string(error));
        }

        if (pty->GetProcessId() == 0)
        {
            running = false;
            throw Napi::Error::New(env, "Process started but no PID obtained");
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        processId = pty->GetProcessId();
        initialized = true;

        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processId);
        if (hProcess != NULL)
        {
            DWORD exitCode;
            if (GetExitCodeProcess(hProcess, &exitCode) && exitCode != STILL_ACTIVE)
            {
                CloseHandle(hProcess);
                running = false;
                throw Napi::Error::New(env, "Process terminated prematurely");
            }
            CloseHandle(hProcess);
        }

        std::cout << "Process started with PID: " << processId << std::endl;
        return Napi::Number::New(env, processId);
    }
    catch (const std::exception &e)
    {
        running = false;
        std::cerr << "Error in StartProcess: " << e.what() << std::endl;
        throw Napi::Error::New(env, e.what());
    }
}
Napi::Value WebTerminal::Write(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (!pty)
    {
        throw Napi::Error::New(env, "PTY not initialized");
    }

    if (info.Length() < 1 || !info[0].IsString())
    {
        throw Napi::TypeError::New(env, "String expected");
    }

    try
    {
        std::string data = info[0].As<Napi::String>().Utf8Value();
        DWORD written;
        // Ajoutez des logs pour débugger
        std::cout << "Trying to write: " << data.length() << " bytes" << std::endl;
        
        if (!pty->Write(data.c_str(), static_cast<DWORD>(data.length()), &written))
        {
            DWORD error = GetLastError();
            std::cout << "Write failed with error code: " << error << std::endl;
            throw std::runtime_error("Write failed with error: " + std::to_string(error));
        }

        return env.Undefined();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error in Write: " << e.what() << std::endl;
        throw Napi::Error::New(env, e.what());
    }
}

Napi::Value WebTerminal::OnData(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsFunction())
    {
        throw Napi::TypeError::New(env, "Function expected");
    }

    std::cout << "Setting up data callback" << std::endl;
    tsfn = Napi::ThreadSafeFunction::New(
        env,
        info[0].As<Napi::Function>(),
        "Terminal Callback",
        0,
        1);

    readThread = std::thread([this]()
                             { this->ReadLoop(); });

    return env.Undefined();
}

Napi::Value WebTerminal::Resize(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (!pty)
    {
        throw Napi::Error::New(env, "PTY not initialized");
    }

    if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber())
    {
        throw Napi::TypeError::New(env, "Numbers expected for columns and rows");
    }

    try
    {
        SHORT cols = static_cast<SHORT>(info[0].As<Napi::Number>().Int32Value());
        SHORT rows = static_cast<SHORT>(info[1].As<Napi::Number>().Int32Value());

        if (!pty->Resize(cols, rows))
        {
            throw std::runtime_error("Resize failed");
        }

        return env.Undefined();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error in Resize: " << e.what() << std::endl;
        throw Napi::Error::New(env, e.what());
    }
}

Napi::Value WebTerminal::Echo(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsString())
    {
        Napi::TypeError::New(env, "String expected").ThrowAsJavaScriptException();
        return env.Null();
    }

    return info[0];
}

Napi::Object Init(Napi::Env env, Napi::Object exports)
{
    return WebTerminal::Init(env, exports);
}

NODE_API_MODULE(terminal, Init)