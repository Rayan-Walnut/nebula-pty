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