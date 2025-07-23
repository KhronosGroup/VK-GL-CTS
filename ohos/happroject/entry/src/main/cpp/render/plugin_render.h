/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _PLUGIN_RENDER_H_
#define _PLUGIN_RENDER_H_

#include <string>
#include <unordered_map>

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <napi/native_api.h>

#include "egl_core.h"


class PluginRender {
public:
    PluginRender(std::string& id);
    static PluginRender* GetInstance(std::string& id,bool weak = false);
    static void RemoveInstance(std::string& id);
    static OH_NativeXComponent_Callback* GetNXComponentCallback();

    void SetNativeXComponent(OH_NativeXComponent* component);

public:
    // NAPI interface
    napi_value Export(napi_env env, napi_value exports);

    // Exposed to JS developers by NAPI
    static napi_value NapiThreadsafeFunc(napi_env env, napi_callback_info info);
    static napi_value NapiStartTest(napi_env env, napi_callback_info info);
    static napi_value NapiRegisterCallback(napi_env env, napi_callback_info info);
    static napi_value NapiUpdateScreen(napi_env env, napi_callback_info info);
    static napi_value NapiKeyEvent(napi_env env, napi_callback_info info);
    static napi_value NapiWindowCommand(napi_env env, napi_callback_info info);

    // Callback, called by ACE XComponent
    void OnSurfaceCreated(OH_NativeXComponent* component, void* window);
    void OnSurfaceChanged(OH_NativeXComponent* component, void* window);
    void OnSurfaceDestroyed(OH_NativeXComponent* component, void* window);
    void DispatchTouchEvent(OH_NativeXComponent* component, void* window);

    void OnMouseEvent(OH_NativeXComponent *component, void *window);
    void OnMouseHover(OH_NativeXComponent *component, bool isHover);
    bool OnVsync();
public:
    static std::unordered_map<std::string, PluginRender*> instance_;
    static OH_NativeXComponent_Callback callback_;

    OH_NativeXComponent* component_;
    EGLCore* eglCore_;


    std::string id_;
    uint64_t width_;
    uint64_t height_;

    double x_;
    double y_;
    OH_NativeXComponent_TouchEvent touchEvent_;
    
    std::mutex coreMutex_;
};

#endif // _PLUGIN_RENDER_H_
