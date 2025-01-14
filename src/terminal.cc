#include "terminal.h"
#include "win/conpty.h"
#include <iostream>
#include <thread>
#include <vector>
#include <Windows.h>

WebTerminal::WebTerminal(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<WebTerminal>(info),
      pty(nullptr),
      running(false),
      initialized(false),
      processId(0) {
    std::cout << "Terminal constructor called" << std::endl;
    pty = std::make_unique<conpty::ConPTY>();
}

WebTerminal::~WebTerminal() {
    std::cout << "Terminal destructor called" << std::endl;
    running = false;
    if (readThread.joinable()) {
        readThread.join();
    }
    if (pty) {
        pty->Close();
    }
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
    if (!pty) return;

    std::cout << "ReadLoop started with running = " << (running ? "true" : "false") << std::endl;
    const DWORD bufSize = 4096;
    std::vector<char> buffer(bufSize);
    DWORD bytesRead;

    // Attendre un peu que le processus soit initialisé
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    int readAttempts = 0;
    while (running.load()) {
        std::cout << "Read attempt #" << ++readAttempts << std::endl;
        
        if (pty->Read(buffer.data(), bufSize - 1, &bytesRead)) {
            std::cout << "Read successful, bytes read: " << bytesRead << std::endl;
            if (bytesRead > 0) {
                buffer[bytesRead] = '\0';
                std::cout << "Data read: " << buffer.data() << std::endl;
                
                auto callback = [](Napi::Env env, Napi::Function jsCallback, std::vector<char>* data) {
                    auto buf = Napi::Buffer<char>::Copy(env, data->data(), data->size());
                    jsCallback.Call({buf});
                    delete data;
                };

                std::vector<char>* dataToSend = new std::vector<char>(buffer.begin(), buffer.begin() + bytesRead);
                tsfn.NonBlockingCall(dataToSend, callback);
            }
        } else {
            DWORD error = GetLastError();
            std::cout << "Read attempt failed with error: " << error << std::endl;
            if (error != ERROR_NO_DATA && error != ERROR_BROKEN_PIPE) {
                std::cout << "Breaking read loop due to error" << std::endl;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    std::cout << "ReadLoop ending with running = " << (running ? "true" : "false") << std::endl;
    tsfn.Release();
}

Napi::Value WebTerminal::StartProcess(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (!pty) {
        throw Napi::Error::New(env, "PTY not initialized");
    }

    try {
        running = true;
        std::cout << "Setting running to true" << std::endl;

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
            running = false;
            std::cout << "ConPTY creation failed with error: " << GetLastError() << std::endl;
            throw std::runtime_error("Failed to create pseudo console");
        }

        std::wstring shellPath = L"cmd.exe";
        std::cout << "Starting shell at: cmd.exe" << std::endl;

        if (!pty->Start(shellPath)) {
            running = false;
            std::cout << "Failed to start shell with error: " << GetLastError() << std::endl;
            throw std::runtime_error("Failed to start process");
        }

        // Attendre que le processus soit démarré
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        processId = pty->GetProcessId();
        initialized = true;

        std::cout << "Process started with PID: " << processId << std::endl;

        // Test d'écriture initial
        DWORD written;
        const char* initialCmd = "echo Terminal Ready\r\n";
        if (!pty->Write(initialCmd, strlen(initialCmd), &written)) {
            std::cout << "Initial write test failed with error: " << GetLastError() << std::endl;
        }

        return Napi::Number::New(env, processId);
    }
    catch (const std::exception& e) {
        running = false;
        std::cerr << "Error in StartProcess: " << e.what() << std::endl;
        throw Napi::Error::New(env, e.what());
    }
}

Napi::Value WebTerminal::Write(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (!pty) {
        throw Napi::Error::New(env, "PTY not initialized");
    }

    if (info.Length() < 1 || !info[0].IsString()) {
        throw Napi::TypeError::New(env, "String expected");
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
        throw Napi::Error::New(env, e.what());
    }
}

Napi::Value WebTerminal::OnData(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsFunction()) {
        throw Napi::TypeError::New(env, "Function expected");
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

    if (!pty) {
        throw Napi::Error::New(env, "PTY not initialized");
    }

    if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
        throw Napi::TypeError::New(env, "Numbers expected for columns and rows");
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
        throw Napi::Error::New(env, e.what());
    }
}

Napi::Value WebTerminal::Echo(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    if (info.Length() < 1 || !info[0].IsString()) {
        Napi::TypeError::New(env, "String expected").ThrowAsJavaScriptException();
        return env.Null();
    }

    return info[0];
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    return WebTerminal::Init(env, exports);
}

NODE_API_MODULE(terminal, Init)