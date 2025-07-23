//
// Created on 2024/9/26.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#ifndef CRDP_NAPI_CALLBACK_H
#define CRDP_NAPI_CALLBACK_H

#include <napi/native_api.h>
#include <string>

class CCNapiCallback {
public:
    static CCNapiCallback &GI() {
        static CCNapiCallback instance;
        return instance;
    }
    static void RegistCallbackFunction(napi_env env, napi_value thisVar, std::string id, napi_value func);
    static void CallCallbackFunction(std::string id,std::string message);
};

#endif //CRDP_NAPI_CALLBACK_H
