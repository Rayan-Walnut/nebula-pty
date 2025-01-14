#pragma once
#include <napi.h>
#include <thread>
#include <atomic>
#include <memory>
#include <Windows.h>

namespace conpty {
    class ConPTY;
}

class WebTerminal : public Napi::ObjectWrap<WebTerminal> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    WebTerminal(const Napi::CallbackInfo& info);
    ~WebTerminal();

private:
    Napi::Value StartProcess(const Napi::CallbackInfo& info);
    Napi::Value Write(const Napi::CallbackInfo& info);
    Napi::Value OnData(const Napi::CallbackInfo& info);
    Napi::Value Resize(const Napi::CallbackInfo& info);
    Napi::Value Echo(const Napi::CallbackInfo& info);
    void ReadLoop();

    std::unique_ptr<conpty::ConPTY> pty;
    std::atomic<bool> running;
    bool initialized;
    DWORD processId;
    std::thread readThread;
    Napi::ThreadSafeFunction tsfn;
};