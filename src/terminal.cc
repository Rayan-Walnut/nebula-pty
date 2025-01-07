#include <nan.h>
#include <string>
#include <memory>
#include <windows.h>

class Terminal : public Nan::ObjectWrap {
public:
    static NAN_MODULE_INIT(Init) {
        v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
        tpl->SetClassName(Nan::New("Terminal").ToLocalChecked());
        tpl->InstanceTemplate()->SetInternalFieldCount(1);

        // Définition des méthodes du prototype
        Nan::SetPrototypeMethod(tpl, "write", Write);
        Nan::SetPrototypeMethod(tpl, "onData", OnData);
        Nan::SetPrototypeMethod(tpl, "resize", Resize);
        Nan::SetPrototypeMethod(tpl, "destroy", Destroy);

        constructor().Reset(Nan::GetFunction(tpl).ToLocalChecked());
        Nan::Set(target, Nan::New("Terminal").ToLocalChecked(),
            Nan::GetFunction(tpl).ToLocalChecked());
    }

private:
    HANDLE hStdin;
    HANDLE hStdout;
    HANDLE hProcess;
    bool isRunning;
    PROCESS_INFORMATION pi;
    HANDLE hConsole;
    Nan::Callback dataCallback;
    int cols;
    int rows;
    HANDLE readThread;
    CRITICAL_SECTION criticalSection;

    explicit Terminal(int cols = 80, int rows = 24) 
        : cols(cols), rows(rows), isRunning(false), 
          hStdin(NULL), hStdout(NULL), hProcess(NULL), 
          hConsole(NULL), readThread(NULL) {
        
        InitializeCriticalSection(&criticalSection);

        SECURITY_ATTRIBUTES sa;
        STARTUPINFOW si;
        HANDLE hStdinRead = NULL, hStdoutWrite = NULL;

        // Initialisation des attributs de sécurité
        ZeroMemory(&sa, sizeof(sa));
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;

        // Création des pipes
        if (!CreatePipe(&hStdin, &hStdoutWrite, &sa, 0) ||
            !CreatePipe(&hStdinRead, &hStdout, &sa, 0)) {
            return;
        }

        // Configuration des buffers des pipes
        DWORD pipeSize = 65536;  // 64KB buffer
        SetNamedPipeHandleState(hStdout, NULL, NULL, &pipeSize);

        // Configuration du processus
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(STARTUPINFOW);
        si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        si.hStdInput = hStdinRead;
        si.hStdOutput = hStdoutWrite;
        si.hStdError = hStdoutWrite;

        // Création du processus CMD avec configuration personnalisée
        WCHAR cmdLine[] = L"cmd.exe /q /k prompt $P$G";
        ZeroMemory(&pi, sizeof(pi));

        if (CreateProcessW(
                NULL,           // Module
                cmdLine,        // Ligne de commande
                NULL,           // Sécurité du processus
                NULL,           // Sécurité du thread
                TRUE,           // Héritage des handles
                CREATE_NEW_CONSOLE, // Flags de création
                NULL,           // Environnement
                NULL,           // Répertoire courant
                &si,           // Info de démarrage
                &pi            // Info du processus
            )) {
            isRunning = true;

            // Configuration du mode console
            DWORD mode;
            GetConsoleMode(hStdout, &mode);
            mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING | 
                   ENABLE_PROCESSED_OUTPUT | 
                   ENABLE_WRAP_AT_EOL_OUTPUT;
            SetConsoleMode(hStdout, mode);

            // Configuration du mode d'entrée
            GetConsoleMode(hStdin, &mode);
            mode |= ENABLE_PROCESSED_INPUT | 
                   ENABLE_LINE_INPUT | 
                   ENABLE_ECHO_INPUT;
            SetConsoleMode(hStdin, mode);

            // Démarrage du thread de lecture
            readThread = CreateThread(
                NULL,           // Attributs de sécurité
                0,             // Taille de la pile
                ReadThread,    // Fonction du thread
                this,          // Paramètre
                0,             // Flags de création
                NULL           // ID du thread
            );

            // Configuration initiale de la taille
            COORD bufferSize = { static_cast<SHORT>(cols), static_cast<SHORT>(rows) };
            SMALL_RECT windowSize = { 0, 0, static_cast<SHORT>(cols - 1), static_cast<SHORT>(rows - 1) };
            
            hConsole = CreateFileW(
                L"CONOUT$",
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL,
                OPEN_EXISTING,
                0,
                NULL
            );

            if (hConsole != INVALID_HANDLE_VALUE) {
                SetConsoleScreenBufferSize(hConsole, bufferSize);
                SetConsoleWindowInfo(hConsole, TRUE, &windowSize);
            }
        }

        // Fermeture des handles temporaires
        if (hStdinRead) CloseHandle(hStdinRead);
        if (hStdoutWrite) CloseHandle(hStdoutWrite);
    }

    ~Terminal() {
        isRunning = false;

        EnterCriticalSection(&criticalSection);
        
        if (readThread) {
            WaitForSingleObject(readThread, 1000);
            CloseHandle(readThread);
            readThread = NULL;
        }

        if (pi.hProcess) {
            TerminateProcess(pi.hProcess, 0);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }

        if (hStdin) CloseHandle(hStdin);
        if (hStdout) CloseHandle(hStdout);
        if (hConsole) CloseHandle(hConsole);

        LeaveCriticalSection(&criticalSection);
        DeleteCriticalSection(&criticalSection);
    }

    static DWORD WINAPI ReadThread(LPVOID param) {
        Terminal* terminal = static_cast<Terminal*>(param);
        char buffer[4096];
        DWORD bytesRead;
        
        while (terminal->isRunning) {
            EnterCriticalSection(&terminal->criticalSection);
            
            bool success = ReadFile(
                terminal->hStdout,
                buffer,
                sizeof(buffer) - 1,
                &bytesRead,
                NULL
            );
            
            if (success && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                
                v8::Isolate* isolate = v8::Isolate::GetCurrent();
                if (isolate != nullptr && !terminal->dataCallback.IsEmpty()) {
                    v8::HandleScope scope(isolate);
                    v8::Local<v8::Value> argv[] = { 
                        Nan::New(buffer).ToLocalChecked() 
                    };
                    Nan::AsyncResource async("Terminal:Read");
                    terminal->dataCallback.Call(1, argv, &async);
                }
            }
            
            LeaveCriticalSection(&terminal->criticalSection);
            Sleep(1);  // Réduire la charge CPU
        }
        
        return 0;
    }

    static NAN_METHOD(New) {
        if (!info.IsConstructCall()) {
            return Nan::ThrowError("Le Terminal doit être appelé avec new");
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

        Terminal* obj = new Terminal(cols, rows);
        obj->Wrap(info.This());
        info.GetReturnValue().Set(info.This());
    }

    static NAN_METHOD(Write) {
        Terminal* terminal = Nan::ObjectWrap::Unwrap<Terminal>(info.Holder());

        if (!info[0]->IsString()) {
            return Nan::ThrowError("L'argument data doit être une chaîne");
        }

        v8::String::Utf8Value data(v8::Isolate::GetCurrent(), info[0]);
        
        if (terminal->isRunning && terminal->hStdin != INVALID_HANDLE_VALUE) {
            EnterCriticalSection(&terminal->criticalSection);
            
            const char* input = *data;
            size_t length = strlen(input);
            std::string fullCommand(input);

            // Ajout automatique du retour chariot si nécessaire
            if (length > 0 && input[length-1] != '\r' && input[length-1] != '\n') {
                fullCommand += "\r\n";
            }

            DWORD written;
            WriteFile(
                terminal->hStdin,
                fullCommand.c_str(),
                fullCommand.length(),
                &written,
                NULL
            );
            FlushFileBuffers(terminal->hStdin);
            
            LeaveCriticalSection(&terminal->criticalSection);
        }
    }

    static NAN_METHOD(OnData) {
        Terminal* terminal = Nan::ObjectWrap::Unwrap<Terminal>(info.Holder());

        if (!info[0]->IsFunction()) {
            return Nan::ThrowError("L'argument callback doit être une fonction");
        }

        terminal->dataCallback.Reset(info[0].As<v8::Function>());
    }

    static NAN_METHOD(Resize) {
        Terminal* terminal = Nan::ObjectWrap::Unwrap<Terminal>(info.Holder());

        if (info.Length() < 2) {
            return Nan::ThrowError("Les colonnes et les lignes sont requises");
        }

        int cols = Nan::To<int32_t>(info[0]).FromJust();
        int rows = Nan::To<int32_t>(info[1]).FromJust();

        terminal->cols = cols;
        terminal->rows = rows;

        if (terminal->hConsole != INVALID_HANDLE_VALUE) {
            EnterCriticalSection(&terminal->criticalSection);
            
            COORD size = {
                static_cast<SHORT>(cols),
                static_cast<SHORT>(rows)
            };
            SMALL_RECT rect = {
                0,
                0,
                static_cast<SHORT>(cols - 1),
                static_cast<SHORT>(rows - 1)
            };
            
            SetConsoleScreenBufferSize(terminal->hConsole, size);
            SetConsoleWindowInfo(terminal->hConsole, TRUE, &rect);
            
            LeaveCriticalSection(&terminal->criticalSection);
        }
    }

    static NAN_METHOD(Destroy) {
        Terminal* terminal = Nan::ObjectWrap::Unwrap<Terminal>(info.Holder());
        terminal->isRunning = false;
        terminal->dataCallback.Reset();
    }

    static inline Nan::Persistent<v8::Function>& constructor() {
        static Nan::Persistent<v8::Function> my_constructor;
        return my_constructor;
    }
};

NODE_MODULE(terminal, Terminal::Init)