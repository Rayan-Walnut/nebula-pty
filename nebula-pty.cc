#include <nan.h>
#include <string>
#include <memory>

#ifdef _WIN32
#include <windows.h>
#include "winpty.h"
#include "winpty_constants.h"

// Utility functions for string conversion
std::wstring char_to_wstring(const char* str) {
    if (!str) return std::wstring();
    int size = MultiByteToWideChar(CP_UTF8, 0, str, -1, nullptr, 0);
    std::wstring wstr(size - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, str, -1, &wstr[0], size);
    return wstr;
}

std::string wchar_to_string(LPCWSTR wstr) {
    if (!wstr) return std::string();
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    std::string str(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &str[0], size, nullptr, nullptr);
    return str;
}

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
    explicit Terminal(int cols = 80, int rows = 24) 
        : pty(nullptr), process(FALSE), process_handle(nullptr) {
        
        winpty_error_ptr_t error = nullptr;
        winpty_config_t* config = winpty_config_new(0, &error);

        if (error != nullptr) {
            std::string error_msg = wchar_to_string(winpty_error_msg(error));
            winpty_error_free(error);
            Nan::ThrowError(error_msg.c_str());
            return;
        }

        winpty_config_set_initial_size(config, cols, rows);
        pty = winpty_open(config, &error);
        winpty_config_free(config);

        if (error != nullptr) {
            std::string error_msg = wchar_to_string(winpty_error_msg(error));
            winpty_error_free(error);
            Nan::ThrowError(error_msg.c_str());
            return;
        }

        const wchar_t* cmdline = L"cmd.exe";
        const wchar_t* cwd = nullptr;
        const wchar_t* env = nullptr;

        HANDLE thread_handle = nullptr;
        process = winpty_spawn(pty, cmdline, cwd, env, &process_handle, &thread_handle, &error);

        if (thread_handle) {
            CloseHandle(thread_handle);
        }

        if (error != nullptr) {
            std::string error_msg = wchar_to_string(winpty_error_msg(error));
            winpty_error_free(error);
            winpty_free(pty);
            Nan::ThrowError(error_msg.c_str());
            return;
        }
    }

    ~Terminal() {
        if (process_handle != nullptr) {
            TerminateProcess(process_handle, 0);
            CloseHandle(process_handle);
            process_handle = nullptr;
        }

        if (pty != nullptr) {
            winpty_free(pty);
            pty = nullptr;
        }
    }

    static NAN_METHOD(New) {
        if (!info.IsConstructCall()) {
            return Nan::ThrowError("Terminal must be called with new");
        }

        int cols = 80;
        int rows = 24;

        if (info.Length() > 0 && info[0]->IsObject()) {
            v8::Local<v8::Object> options = Nan::To<v8::Object>(info[0]).ToLocalChecked();
            
            v8::Local<v8::String> cols_key = Nan::New("cols").ToLocalChecked();
            v8::Local<v8::String> rows_key = Nan::New("rows").ToLocalChecked();

            if (Nan::Has(options, cols_key).FromJust()) {
                cols = Nan::To<int32_t>(Nan::Get(options, cols_key).ToLocalChecked()).FromJust();
            }
            if (Nan::Has(options, rows_key).FromJust()) {
                rows = Nan::To<int32_t>(Nan::Get(options, rows_key).ToLocalChecked()).FromJust();
            }
        }

        Terminal* terminal = new Terminal(cols, rows);
        terminal->Wrap(info.This());
        info.GetReturnValue().Set(info.This());
    }

    static NAN_METHOD(Write) {
        Terminal* terminal = Nan::ObjectWrap::Unwrap<Terminal>(info.Holder());

        if (info.Length() < 1) {
            return Nan::ThrowError("Data argument required");
        }

        v8::String::Utf8Value data(v8::Isolate::GetCurrent(), info[0]);
        
        if (terminal->pty != nullptr) {
            HANDLE conin = CreateFileW(
                winpty_conin_name(terminal->pty),
                GENERIC_WRITE,
                0,
                nullptr,
                OPEN_EXISTING,
                0,
                nullptr);

            if (conin != INVALID_HANDLE_VALUE) {
                DWORD written;
                WriteFile(conin, *data, data.length(), &written, nullptr);
                CloseHandle(conin);
            }
        }
    }

    static NAN_METHOD(OnData) {
        Terminal* terminal = Nan::ObjectWrap::Unwrap<Terminal>(info.Holder());

        if (info.Length() < 1 || !info[0]->IsFunction()) {
            return Nan::ThrowError("Callback function required");
        }

        terminal->dataCallback.Reset(info[0].As<v8::Function>());

        if (terminal->pty != nullptr) {
            HANDLE conout = CreateFileW(
                winpty_conout_name(terminal->pty),
                GENERIC_READ,
                0,
                nullptr,
                OPEN_EXISTING,
                FILE_FLAG_OVERLAPPED,
                nullptr);

            if (conout != INVALID_HANDLE_VALUE) {
                // TODO: Implement async reading
                CloseHandle(conout);
            }
        }
    }

    static NAN_METHOD(Resize) {
        Terminal* terminal = Nan::ObjectWrap::Unwrap<Terminal>(info.Holder());

        if (info.Length() < 2) {
            return Nan::ThrowError("Columns and rows required");
        }

        if (terminal->pty != nullptr) {
            int cols = Nan::To<int32_t>(info[0]).FromJust();
            int rows = Nan::To<int32_t>(info[1]).FromJust();

            winpty_error_ptr_t error = nullptr;
            winpty_set_size(terminal->pty, cols, rows, &error);

            if (error != nullptr) {
                std::string error_msg = wchar_to_string(winpty_error_msg(error));
                winpty_error_free(error);
                Nan::ThrowError(error_msg.c_str());
            }
        }
    }

    static NAN_METHOD(Destroy) {
        Terminal* terminal = Nan::ObjectWrap::Unwrap<Terminal>(info.Holder());

        if (terminal->process_handle != nullptr) {
            TerminateProcess(terminal->process_handle, 0);
            CloseHandle(terminal->process_handle);
            terminal->process_handle = nullptr;
        }

        if (terminal->pty != nullptr) {
            winpty_free(terminal->pty);
            terminal->pty = nullptr;
        }
    }

    static inline Nan::Persistent<v8::Function>& constructor() {
        static Nan::Persistent<v8::Function> my_constructor;
        return my_constructor;
    }

    winpty_t* pty;
    BOOL process;
    HANDLE process_handle;
    Nan::Callback dataCallback;
};

#endif

NODE_MODULE(terminal, Terminal::Init)