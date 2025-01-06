#include <napi.h>
#include <string>
#include <memory>

#ifdef _WIN32
#include <windows.h>
#include <winpty.h>

class Terminal : public Napi::ObjectWrap<Terminal> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports) {
        Napi::Function func = DefineClass(env, "Terminal", {
            InstanceMethod("write", &Terminal::Write),
            InstanceMethod("onData", &Terminal::OnData),
            InstanceMethod("resize", &Terminal::Resize),
            InstanceMethod("destroy", &Terminal::Destroy)
        });

        constructor = Napi::Persistent(func);
        constructor.SuppressDestruct();
        exports.Set("Terminal", func);
        return exports;
    }

    Terminal(const Napi::CallbackInfo& info) : Napi::ObjectWrap<Terminal>(info) {
        Napi::Env env = info.Env();
        
        winpty_error_ptr_t error = nullptr;
        winpty_config_t* config = winpty_config_new(0, &error);
        
        if (error != nullptr) {
            const char* error_msg = winpty_error_msg(error);
            winpty_error_free(error);
            throw Napi::Error::New(env, error_msg);
        }

        int cols = 80;
        int rows = 24;

        if (info.Length() > 0 && info[0].IsObject()) {
            Napi::Object options = info[0].As<Napi::Object>();
            if (options.Has("cols")) {
                cols = options.Get("cols").As<Napi::Number>().Int32Value();
            }
            if (options.Has("rows")) {
                rows = options.Get("rows").As<Napi::Number>().Int32Value();
            }
        }

        winpty_config_set_initial_size(config, cols, rows);
        pty = winpty_open(config, &error);
        winpty_config_free(config);

        if (error != nullptr) {
            const char* error_msg = winpty_error_msg(error);
            winpty_error_free(error);
            throw Napi::Error::New(env, error_msg);
        }

        const wchar_t* cmdline = L"cmd.exe";
        const wchar_t* cwd = nullptr;
        const wchar_t* env = nullptr;

        HANDLE process_handle = nullptr;
        process = winpty_spawn(
            pty,
            cmdline,
            cwd,
            env,
            &process_handle,
            nullptr,
            &error
        );

        if (error != nullptr) {
            const char* error_msg = winpty_error_msg(error);
            winpty_error_free(error);
            winpty_free(pty);
            throw Napi::Error::New(env, error_msg);
        }

        this->process_handle = process_handle;
    }

    ~Terminal() {
        Destroy(Napi::CallbackInfo(Env(), nullptr, 0));
    }

private:
    static Napi::FunctionReference constructor;
    winpty_t* pty = nullptr;
    BOOL process = FALSE;
    HANDLE process_handle = nullptr;
    Napi::FunctionReference dataCallback;

    Napi::Value Write(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();
        
        if (info.Length() < 1) {
            throw Napi::Error::New(env, "Data argument required");
        }

        std::string data = info[0].As<Napi::String>().Utf8Value();
        
        if (pty != nullptr) {
            HANDLE conin = CreateFileW(
                winpty_conin_name(pty),
                GENERIC_WRITE,
                0,
                nullptr,
                OPEN_EXISTING,
                0,
                nullptr
            );

            if (conin != INVALID_HANDLE_VALUE) {
                DWORD written;
                WriteFile(conin, data.c_str(), data.length(), &written, nullptr);
                CloseHandle(conin);
            }
        }

        return env.Undefined();
    }

    Napi::Value OnData(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();
        
        if (info.Length() < 1 || !info[0].IsFunction()) {
            throw Napi::Error::New(env, "Callback function required");
        }

        dataCallback = Napi::Persistent(info[0].As<Napi::Function>());

        if (pty != nullptr) {
            HANDLE conout = CreateFileW(
                winpty_conout_name(pty),
                GENERIC_READ,
                0,
                nullptr,
                OPEN_EXISTING,
                FILE_FLAG_OVERLAPPED,
                nullptr
            );

            if (conout != INVALID_HANDLE_VALUE) {
                // TODO: Implement async reading
                CloseHandle(conout);
            }
        }

        return env.Undefined();
    }

    Napi::Value Resize(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();
        
        if (info.Length() < 2) {
            throw Napi::Error::New(env, "Columns and rows required");
        }

        if (pty != nullptr) {
            int cols = info[0].As<Napi::Number>().Int32Value();
            int rows = info[1].As<Napi::Number>().Int32Value();

            winpty_error_ptr_t error = nullptr;
            winpty_set_size(pty, cols, rows, &error);
            
            if (error != nullptr) {
                const char* error_msg = winpty_error_msg(error);
                winpty_error_free(error);
                throw Napi::Error::New(env, error_msg);
            }
        }

        return env.Undefined();
    }

    Napi::Value Destroy(const Napi::CallbackInfo& info) {
        if (process_handle != nullptr) {
            TerminateProcess(process_handle, 0);
            CloseHandle(process_handle);
            process_handle = nullptr;
        }

        if (pty != nullptr) {
            winpty_free(pty);
            pty = nullptr;
        }

        return info.Env().Undefined();
    }
};

Napi::FunctionReference Terminal::constructor;
#endif

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    return Terminal::Init(env, exports);
}

NODE_API_MODULE(terminal, Init)