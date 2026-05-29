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

#include <stdint.h>
#include <string>
#include <stdio.h>

#include <ace/xcomponent/native_interface_xcomponent.h>

#include "plugin_manager.h"
#include "plugin_common.h"

enum ContextType {
    APP_LIFECYCLE = 0,
    JS_PAGE_LIFECYCLE,
};

PluginManager PluginManager::manager_;

void VsyncCallback(long long timestamp, void *data) {
    PluginManager *pm = (PluginManager *)data;
    pm->OnVsync();
}

void PluginManager::DoVsync(){
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> remove;
        for (auto it : renderContextMap_) {
            if (!it.second.render_->OnVsync()) {
                remove.push_back(it.first);
            }
        }
        for (auto it : remove) {
            PluginRender::RemoveInstance(it);
            renderContextMap_.erase(it);
        }
    }

    OH_NativeVSync_RequestFrame(vsync_, VsyncCallback, this);
}

void PluginManager::OnVsync() {
    uv_work_t *work = new uv_work_t;
    uv_queue_work(
        mainLoop_, work, [](uv_work_t *) {},
        [](uv_work_t *work, int status) {
            delete work;
            PluginManager::GetInstance()->DoVsync();
        });
}

PluginManager::PluginManager() {
    vsync_ = OH_NativeVSync_Create("vkglcts", 2);
    OH_NativeVSync_RequestFrame(vsync_, VsyncCallback, this);
}

PluginManager::~PluginManager() { OH_NativeVSync_Destroy(vsync_); }

bool PluginManager::Export(napi_env env, napi_value exports) {
    napi_status status;
    napi_value exportInstance = nullptr;
    OH_NativeXComponent *nativeXComponent = nullptr;
    int32_t ret;
    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {};
    uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;

    status = napi_get_named_property(env, exports, OH_NATIVE_XCOMPONENT_OBJ, &exportInstance);
    if (status != napi_ok) {
        LOGE("Export false 1");
        return false;
    }

    status = napi_unwrap(env, exportInstance, reinterpret_cast<void **>(&nativeXComponent));
    if (status != napi_ok) {
        LOGE("Export false 2");
        return false;
    }

    ret = OH_NativeXComponent_GetXComponentId(nativeXComponent, idStr, &idSize);
    if (ret != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        LOGE("Export false 3");
        return false;
    }

    std::string id(idStr);
    auto context = PluginManager::GetInstance();
    if (context) {
        {
            uv_loop_t *loop = nullptr;
            napi_get_uv_event_loop(env, &loop);
            context->OnCreateNative(env, loop);
        }
        std::lock_guard<std::mutex> lock(mutex_);
        renderContextMap_[id].native_ = nativeXComponent;
        renderContextMap_[id].render_ = PluginRender::GetInstance(id);
        renderContextMap_[id].render_->SetNativeXComponent(nativeXComponent);
        renderContextMap_[id].render_->Export(env, exports);
        LOGE("Export ok %{public}s", id.c_str());
        return true;
    }
    LOGE("Export false 4");
    return false;
}

void PluginManager::MainOnMessage(const uv_async_t *req) { LOGD("MainOnMessage Triggered"); }
napi_value PluginManager::NapiOnCreate(napi_env env, napi_callback_info info) {
    LOGD("PluginManager::NapiOnCreate");
    uv_loop_t *loop = nullptr;
    uv_check_t *check = new uv_check_t;
    NAPI_CALL(env, napi_get_uv_event_loop(env, &loop));
    PluginManager::GetInstance()->OnCreateNative(env, loop);
    return nullptr;
}

napi_value PluginManager::NapiOnShow(napi_env env, napi_callback_info info) {
    PluginManager::GetInstance()->OnShowNative();
    return nullptr;
}

napi_value PluginManager::NapiOnHide(napi_env env, napi_callback_info info) {
    PluginManager::GetInstance()->OnHideNative();
    return nullptr;
}

napi_value PluginManager::NapiOnDestroy(napi_env env, napi_callback_info info) {
    PluginManager::GetInstance()->OnDestroyNative();
    return nullptr;
}

void PluginManager::OnCreateNative(napi_env env, uv_loop_t *loop) {
    mainEnv_ = env;
    mainLoop_ = loop;
    if (mainLoop_) {
        uv_async_init(mainLoop_, &mainOnMessageSignal_, reinterpret_cast<uv_async_cb>(PluginManager::MainOnMessage));
    }
}

void PluginManager::OnShowNative() { LOGD("PluginManager::OnShowNative"); }
void PluginManager::OnHideNative() { LOGD("PluginManager::OnHideNative"); }
void PluginManager::OnDestroyNative() { LOGD("PluginManager::OnDestroyNative"); }

napi_value PluginManager::NapiOnPageShow(napi_env env, napi_callback_info info) {
    LOGD("PluginManager::NapiOnPageShow");
    return nullptr;
}

napi_value PluginManager::NapiOnPageHide(napi_env env, napi_callback_info info) {
    LOGD("PluginManager::NapiOnPageHide");
    return nullptr;
}

void PluginManager::OnPageShowNative() { LOGD("PluginManager::OnPageShowNative"); }

void PluginManager::OnPageHideNative() { LOGD("PluginManager::OnPageHideNative"); }