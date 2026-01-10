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

#ifndef _PLUGIN_MANAGER_H_
#define _PLUGIN_MANAGER_H_

#include <string>
#include <unordered_map>
#include <mutex>

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <napi/native_api.h>
#include <uv.h>
#include <native_vsync/native_vsync.h>

#include "native_common.h"
#include "plugin_render.h"

struct RenderContext {
    OH_NativeXComponent* native_;
    PluginRender* render_;
};

class PluginManager {
public:
    PluginManager();
    ~PluginManager();
    void OnVsync();
    void DoVsync();

    static PluginManager* GetInstance()
    {
        return &PluginManager::manager_;
    }

    // static napi_value GetContext(napi_env env, napi_callback_info info);

    /******************************APP Lifecycle******************************/
    static napi_value NapiOnCreate(napi_env env, napi_callback_info info);
    static napi_value NapiOnShow(napi_env env, napi_callback_info info);
    static napi_value NapiOnHide(napi_env env, napi_callback_info info);
    static napi_value NapiOnDestroy(napi_env env, napi_callback_info info);

    void OnCreateNative(napi_env env, uv_loop_t* loop);
    void OnShowNative();
    void OnHideNative();
    void OnDestroyNative();
    /*********************************************************************/

    /******************************声明式范式******************************/
    /**                      JS Page : Lifecycle                        **/
    static napi_value NapiOnPageShow(napi_env env, napi_callback_info info);
    static napi_value NapiOnPageHide(napi_env env, napi_callback_info info);
    void OnPageShowNative();
    void OnPageHideNative();
    /*************************************************************************/

public:
    // Napi export
    bool Export(napi_env env, napi_value exports);
private:
    static void MainOnMessage(const uv_async_t* req);
    static PluginManager manager_;

    std::string id_;
    std::unordered_map<std::string, RenderContext> renderContextMap_;
    OH_NativeVSync *vsync_ = nullptr;
    std::mutex mutex_;
    bool bInited;

public:
    napi_env mainEnv_ = nullptr;
    uv_loop_t* mainLoop_ = nullptr;
    uv_async_t mainOnMessageSignal_ {};
};

#endif // _PLUGIN_MANAGER_H_