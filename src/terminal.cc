#include <nan.h>
#include <string>
#include <memory>

class Terminal : public Nan::ObjectWrap {
public:
    static NAN_MODULE_INIT(Init) {
        printf("Initialisation du module Terminal\n");
        
        v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
        tpl->SetClassName(Nan::New("Terminal").ToLocalChecked());
        tpl->InstanceTemplate()->SetInternalFieldCount(1);

        // Enregistrement des méthodes
        Nan::SetPrototypeMethod(tpl, "write", Write);
        Nan::SetPrototypeMethod(tpl, "onData", OnData);
        Nan::SetPrototypeMethod(tpl, "resize", Resize);
        Nan::SetPrototypeMethod(tpl, "destroy", Destroy);

        constructor().Reset(Nan::GetFunction(tpl).ToLocalChecked());
        
        // Export de la classe
        v8::Local<v8::Function> func = Nan::GetFunction(tpl).ToLocalChecked();
        Nan::Set(target, Nan::New("Terminal").ToLocalChecked(), func);
        
        printf("Module Terminal initialisé\n");
    }

private:
    explicit Terminal(int cols = 80, int rows = 24) {
        printf("Construction d'un nouveau Terminal (%dx%d)\n", cols, rows);
        this->cols = cols;
        this->rows = rows;
    }

    ~Terminal() {
        printf("Destruction du Terminal\n");
    }

    static NAN_METHOD(New) {
        printf("Méthode New appelée\n");
        
        if (!info.IsConstructCall()) {
            return Nan::ThrowError("Terminal doit être appelé avec new");
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
        
        printf("Nouveau Terminal créé avec succès\n");
        info.GetReturnValue().Set(info.This());
    }

    static NAN_METHOD(Write) {
        printf("Méthode Write appelée\n");
        Terminal* terminal = Nan::ObjectWrap::Unwrap<Terminal>(info.Holder());

        if (info.Length() < 1) {
            return Nan::ThrowError("Data argument required");
        }

        v8::String::Utf8Value data(v8::Isolate::GetCurrent(), info[0]);
        printf("Données reçues dans Write: %s\n", *data);

        // Test: émettre les données reçues via le callback
        if (!terminal->dataCallback.IsEmpty()) {
            v8::Local<v8::Value> argv[] = { info[0] };
            terminal->dataCallback.Call(1, argv);
            printf("Données envoyées via callback\n");
        }
    }

    static NAN_METHOD(OnData) {
        printf("Méthode OnData appelée\n");
        Terminal* terminal = Nan::ObjectWrap::Unwrap<Terminal>(info.Holder());

        if (info.Length() < 1 || !info[0]->IsFunction()) {
            return Nan::ThrowError("Callback function required");
        }

        terminal->dataCallback.Reset(info[0].As<v8::Function>());
        printf("Callback OnData enregistré\n");

        // Test d'émission de données
        v8::Local<v8::Value> argv[] = { Nan::New("Terminal prêt!\r\n").ToLocalChecked() };
        terminal->dataCallback.Call(1, argv);
        printf("Message de test envoyé via callback\n");
    }

    static NAN_METHOD(Resize) {
        printf("Méthode Resize appelée\n");
        Terminal* terminal = Nan::ObjectWrap::Unwrap<Terminal>(info.Holder());

        if (info.Length() < 2) {
            return Nan::ThrowError("Columns and rows required");
        }

        int cols = Nan::To<int32_t>(info[0]).FromJust();
        int rows = Nan::To<int32_t>(info[1]).FromJust();
        
        terminal->cols = cols;
        terminal->rows = rows;
        
        printf("Terminal redimensionné à %dx%d\n", cols, rows);
    }

    static NAN_METHOD(Destroy) {
        printf("Méthode Destroy appelée\n");
        Terminal* terminal = Nan::ObjectWrap::Unwrap<Terminal>(info.Holder());
        terminal->dataCallback.Reset();
    }

    static inline Nan::Persistent<v8::Function>& constructor() {
        static Nan::Persistent<v8::Function> my_constructor;
        return my_constructor;
    }

    Nan::Callback dataCallback;
    int cols;
    int rows;
};

NODE_MODULE(terminal, Terminal::Init)