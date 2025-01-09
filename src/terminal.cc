#include <nan.h>
#include <windows.h>
#include <string>
#include <iostream>
#include <atomic>
#include <filesystem>
std::string currentPath;

class Terminal : public Nan::ObjectWrap
{
public:
    static NAN_MODULE_INIT(Init)
    {
        std::cout << "Initialisation du module Terminal" << std::endl;

        v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
        tpl->SetClassName(Nan::New("Terminal").ToLocalChecked());
        tpl->InstanceTemplate()->SetInternalFieldCount(1);

        Nan::SetPrototypeMethod(tpl, "write", Write);
        Nan::SetPrototypeMethod(tpl, "onData", OnData);
        Nan::SetPrototypeMethod(tpl, "destroy", Destroy);
        Nan::SetPrototypeMethod(tpl, "executeCommand", ExecuteCommand);
        Nan::SetPrototypeMethod(tpl, "setWorkingDirectory", SetWorkingDirectory);

        constructor().Reset(Nan::GetFunction(tpl).ToLocalChecked());
        Nan::Set(target, Nan::New("Terminal").ToLocalChecked(),
                 Nan::GetFunction(tpl).ToLocalChecked());
    }

private:
    explicit Terminal(const char *initialPath = nullptr) : isRunning(true)
    {
        if (initialPath && *initialPath)
        {
            try
            {
                std::filesystem::path path(initialPath);
                if (std::filesystem::exists(path) && std::filesystem::is_directory(path))
                {
                    workingDirectory = std::filesystem::absolute(path).string();
                    currentPath = workingDirectory;           // Initialisation de currentPath
                    SetCurrentDirectory(currentPath.c_str()); // Set initial directory
                }
                else
                {
                    throw std::runtime_error("Le chemin spécifié n'existe pas ou n'est pas un dossier valide");
                }
            }
            catch (const std::exception &e)
            {
                throw std::runtime_error(std::string("Erreur de chemin: ") + e.what());
            }
        }
    }

    static NAN_METHOD(ExecuteCommand)
    {
        Terminal *terminal = Nan::ObjectWrap::Unwrap<Terminal>(info.Holder());

        if (!info[0]->IsString())
        {
            return Nan::ThrowTypeError("Command must be a string");
        }

        v8::String::Utf8Value command(v8::Isolate::GetCurrent(), info[0]);
        std::string result = terminal->RunCommand(*command);

        info.GetReturnValue().Set(Nan::New(result).ToLocalChecked());
    }

    static NAN_METHOD(SetWorkingDirectory)
    {
        Terminal *terminal = Nan::ObjectWrap::Unwrap<Terminal>(info.Holder());

        if (!info[0]->IsString())
        {
            return Nan::ThrowTypeError("Le chemin doit être une chaîne de caractères");
        }

        v8::String::Utf8Value path(v8::Isolate::GetCurrent(), info[0]);

        try
        {
            std::filesystem::path newPath(*path);
            if (std::filesystem::exists(newPath) && std::filesystem::is_directory(newPath))
            {
                terminal->workingDirectory = std::filesystem::absolute(newPath).string();
                info.GetReturnValue().Set(Nan::True());
            }
            else
            {
                return Nan::ThrowError("Le chemin spécifié n'existe pas ou n'est pas un dossier valide");
            }
        }
        catch (const std::exception &e)
        {
            return Nan::ThrowError(e.what());
        }
    }

    std::string RunCommand(const char *cmd)
    {
        std::string command(cmd);
        // Configuration de l'encodage
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);

        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = NULL;

        HANDLE hReadPipe, hWritePipe;
        if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0))
        {
            return "Erreur création pipe";
        }

        SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        ZeroMemory(&pi, sizeof(pi));

        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = NULL;
        si.hStdOutput = hWritePipe;
        si.hStdError = hWritePipe;
        
        // Gestion spéciale de cd
        if (command.substr(0, 3) == "cd ")
        {
            std::string newPath = command.substr(3);
            try
            {
                std::filesystem::path path(newPath);
                if (std::filesystem::exists(path))
                {
                    currentPath = std::filesystem::absolute(path).string();
                    SetCurrentDirectory(currentPath.c_str());
                    return currentPath + "\n";
                }
                return "Chemin invalide\n";
            }
            catch (...)
            {
                return "Erreur de changement de répertoire\n";
            }
        }

        // Configuration PowerShell avec encodage UTF-8
        std::string cmdLine = std::string("powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \"") + "$OutputEncoding = [System.Text.Encoding]::UTF8; " + "[Console]::OutputEncoding = [System.Text.Encoding]::UTF8; " + "[Console]::InputEncoding = [System.Text.Encoding]::UTF8; " + cmd + "\"";

        BOOL success = CreateProcessA(
            NULL,
            const_cast<LPSTR>(cmdLine.c_str()),
            NULL,
            NULL,
            TRUE,
            CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
            NULL,
            currentPath.empty() ? NULL : currentPath.c_str(), 
            &si,
            &pi);

        if (!success)
        {
            CloseHandle(hReadPipe);
            CloseHandle(hWritePipe);
            return "Erreur exécution commande";
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
                    v8::String::Utf8Value pathValue(v8::Isolate::GetCurrent(), info[0]);
                    initialPath = *pathValue;
                }
                else if (info[0]->IsObject())
                {
                    v8::Local<v8::Object> options = info[0].As<v8::Object>();
                    v8::Local<v8::String> cwdKey = Nan::New("cwd").ToLocalChecked();
                    if (Nan::Has(options, cwdKey).FromJust())
                    {
                        v8::String::Utf8Value cwdValue(v8::Isolate::GetCurrent(),
                                                       Nan::Get(options, cwdKey).ToLocalChecked());
                        if (*cwdValue)
                        {
                            initialPath = *cwdValue;
                        }
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
        info.GetReturnValue().Set(Nan::True());
    }

    static NAN_METHOD(OnData)
    {
        Terminal *terminal = Nan::ObjectWrap::Unwrap<Terminal>(info.Holder());
        if (!info[0]->IsFunction())
        {
            return Nan::ThrowTypeError("Callback must be a function");
        }
        terminal->dataCallback.Reset(info[0].As<v8::Function>());
    }

    static NAN_METHOD(Destroy)
    {
        Terminal *terminal = Nan::ObjectWrap::Unwrap<Terminal>(info.Holder());
        terminal->Cleanup();
    }

    void Cleanup()
    {
        isRunning = false;
        dataCallback.Reset();
    }

    ~Terminal()
    {
        Cleanup();
    }

    static inline Nan::Persistent<v8::Function> &constructor()
    {
        static Nan::Persistent<v8::Function> my_constructor;
        return my_constructor;
    }

    std::atomic<bool> isRunning;
    std::string workingDirectory;
    std::string currentPath; // Ajout comme variable membre
    Nan::Callback dataCallback;
};

NODE_MODULE(terminal, Terminal::Init)