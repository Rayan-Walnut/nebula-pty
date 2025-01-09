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