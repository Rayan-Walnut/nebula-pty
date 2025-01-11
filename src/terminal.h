#ifndef TERMINAL_H
#define TERMINAL_H

#include <napi.h>
#include <windows.h>
#include <string>
#include <memory>
#include <thread>

namespace conpty {
    class ConPTY;
}

namespace terminal {

class WebTerminal : public Napi::ObjectWrap<WebTerminal> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    WebTerminal(const Napi::CallbackInfo& info);
    ~WebTerminal();

private:
    // Node.js methods
    Napi::Value StartProcess(const Napi::CallbackInfo& info);
    Napi::Value Write(const Napi::CallbackInfo& info);
    Napi::Value OnData(const Napi::CallbackInfo& info);
    Napi::Value Resize(const Napi::CallbackInfo& info);
    Napi::Value Echo(const Napi::CallbackInfo& info);

    // Internal methods
    void ReadLoop();

    // Member variables
    std::unique_ptr<conpty::ConPTY> pty;
    Napi::ThreadSafeFunction tsfn;
    bool running;
    std::thread readThread;
};

}  // namespace terminal

#endif // TERMINAL_H