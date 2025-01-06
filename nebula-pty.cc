#define NODE_ADDON_API_DISABLE_CPP_EXCEPTIONS
#include <napi.h>

// Une simple fonction qui retourne "Hello World"
Napi::String Hello(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    return Napi::String::New(env, "Hello World");
}

// Fonction d'initialisation
Napi::Object Init(Napi::Env env, Napi::Object exports) {
    // On exporte la fonction 'hello'
    exports.Set("hello", Napi::Function::New(env, Hello));
    return exports;
}

NODE_API_MODULE(test_addon, Init)