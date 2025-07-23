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
//#include <thread>
#include <pthread.h>

#include "plugin_render.h"
#include "plugin_common.h"
#include "plugin_manager.h"
#include "napi_callback.h"

#include <rawfile/raw_file.h>
#include <rawfile/raw_dir.h>
#include <rawfile/raw_file_manager.h>
#include <sys/stat.h>

#include "hilog/log.h"

#ifdef __cplusplus
extern "C" {
#endif

std::unordered_map<std::string, PluginRender *> PluginRender::instance_;

OH_NativeXComponent_Callback PluginRender::callback_;

void OnSurfaceCreatedCB(OH_NativeXComponent *component, void *window) {
    LOGD("OnSurfaceCreatedCB");
    int32_t ret;
    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {};
    uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;
    ret = OH_NativeXComponent_GetXComponentId(component, idStr, &idSize);
    if (ret != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        return;
    }

    std::string id(idStr);
    auto render = PluginRender::GetInstance(id);
    render->OnSurfaceCreated(component, window);
}

void OnSurfaceChangedCB(OH_NativeXComponent *component, void *window) {
    int32_t ret;
    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {};
    uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;
    ret = OH_NativeXComponent_GetXComponentId(component, idStr, &idSize);
    if (ret != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        return;
    }

    std::string id(idStr);
    auto render = PluginRender::GetInstance(id);
    render->OnSurfaceChanged(component, window);
}

void OnSurfaceDestroyedCB(OH_NativeXComponent *component, void *window) {
    int32_t ret;
    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {};
    uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;
    ret = OH_NativeXComponent_GetXComponentId(component, idStr, &idSize);
    if (ret != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        return;
    }

    std::string id(idStr);
    auto render = PluginRender::GetInstance(id);
    render->OnSurfaceDestroyed(component, window);
}

void DispatchTouchEventCB(OH_NativeXComponent *component, void *window) {
    //    LOGD("DispatchTouchEventCB");
    int32_t ret;
    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {};
    uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;
    ret = OH_NativeXComponent_GetXComponentId(component, idStr, &idSize);
    if (ret != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        return;
    }

    std::string id(idStr);
    auto render = PluginRender::GetInstance(id);
    render->DispatchTouchEvent(component, window);
}

PluginRender::PluginRender(std::string &id) : id_(id), component_(nullptr) {
    CLOGE("~~~PluginRender init");
    eglCore_ = new EGLCore(id);
    auto renderCallback = PluginRender::GetNXComponentCallback();
    renderCallback->OnSurfaceCreated = OnSurfaceCreatedCB;
    renderCallback->OnSurfaceChanged = OnSurfaceChangedCB;
    renderCallback->OnSurfaceDestroyed = OnSurfaceDestroyedCB;
    renderCallback->DispatchTouchEvent = DispatchTouchEventCB;
}

PluginRender *PluginRender::GetInstance(std::string &id, bool weak) {
    if (instance_.find(id) == instance_.end()) {
        if (weak) {
            return nullptr;
        }
        PluginRender *instance = new PluginRender(id);
        instance_[id] = instance;
        return instance;
    } else {
        return instance_[id];
    }
}

void PluginRender::RemoveInstance(std::string &id) {
    if (instance_.find(id) == instance_.end()) {
        return;
    }
    delete instance_[id];
    instance_.erase(id);
}

OH_NativeXComponent_Callback *PluginRender::GetNXComponentCallback() { return &PluginRender::callback_; }

PluginRender *ExpandRender(OH_NativeXComponent *component) {
    int32_t ret;
    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {};
    uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;
    ret = OH_NativeXComponent_GetXComponentId(component, idStr, &idSize);
    if (ret != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        return nullptr;
    }

    std::string id(idStr);
    return PluginRender::GetInstance(id);
}

void OnMouseEventCB(OH_NativeXComponent *component, void *window) {
    auto render = ExpandRender(component);
    render->OnMouseEvent(component, window);
}

void OnMouseHoverCB(OH_NativeXComponent *component, bool isHover) {
    auto render = ExpandRender(component);
    render->OnMouseHover(component, isHover);
}

static OH_NativeXComponent_MouseEvent_Callback g_mouseCallback = {
    .DispatchMouseEvent = OnMouseEventCB,
    .DispatchHoverEvent = OnMouseHoverCB,
};

void PluginRender::SetNativeXComponent(OH_NativeXComponent *component) {
    component_ = component;
    OH_NativeXComponent_RegisterCallback(component_, &PluginRender::callback_);
    OH_NativeXComponent_RegisterMouseEventCallback(component_, &g_mouseCallback);
}

void PluginRender::OnSurfaceCreated(OH_NativeXComponent *component, void *window) {
    LOGD("PluginRender::OnSurfaceCreated");
    int32_t ret = OH_NativeXComponent_GetXComponentSize(component, window, &width_, &height_);
    if (ret == OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        eglCore_->GLContextInit(window, width_, height_);
    }
}

void PluginRender::OnSurfaceChanged(OH_NativeXComponent *component, void *window) {
    LOGE("PluginRender::OnSurfaceChanged");
    int32_t ret = OH_NativeXComponent_GetXComponentSize(component, window, &width_, &height_);
    if (ret == OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        eglCore_->SetXSize(width_, height_);
    }
}

void PluginRender::OnSurfaceDestroyed(OH_NativeXComponent *component, void *window) {
    CLOGE("PluginRender::OnSurfaceDestroyed");
    delete eglCore_;
    eglCore_ = nullptr;
}

void PluginRender::OnMouseEvent(OH_NativeXComponent *component, void *window) {
    OH_NativeXComponent_MouseEvent mouseEvent;
    int32_t ret = OH_NativeXComponent_GetMouseEvent(component, window, &mouseEvent);
    if (ret == OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        // CLOGE("Mouse Info : x = %{public}f, y = %{public}f , screenX = %{public}f ,screenY = %{public}f ,action = "
        //          "%{public}d , button = %{public}d",
        // mouseEvent.x, mouseEvent.y, mouseEvent.screenX, mouseEvent.screenY, mouseEvent.action,
        // mouseEvent.button);
        int btn = mouseEvent.button;
        if (btn == 1)
            btn = 0;
        else if (btn == 2)
            btn = 2; // 右键
        else if (btn == 4)
            btn = 1; // 中键
        //         uint32_t type;       // 1:down, 2:move, 3:up ,4:向前滚动, 5:向后滚动
        int type = mouseEvent.action;
        if (type == 1)
            type = 0;
        else if (type == 2)
            type = 1;
        else if (type == 3)
            type = 2;
        if (mouseEvent.action == 0) { // 离开窗口区域，不处理
            return;
        }
        eglCore_->OnTouch(btn, mouseEvent.screenX, mouseEvent.screenY, type);
    }
}
void PluginRender::OnMouseHover(OH_NativeXComponent *component, bool isHover) {}

void PluginRender::DispatchTouchEvent(OH_NativeXComponent *component, void *window) {
    int32_t ret = OH_NativeXComponent_GetTouchEvent(component, window, &touchEvent_);

    if (ret == OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        // LOGD("Touch Info : x = %{public}f, y = %{public}f screenx = %{public}f, screeny = %{public}f", touchEvent_.x,
        // touchEvent_.y, touchEvent_.screenX, touchEvent_.screenY); for (int i=0;i<touchEvent_.numPoints;i++) {
        //     LOGE("Touch Info : dots[%{public}d] id %{public}d x = %{public}f, y = %{public}f", i,
        //     touchEvent_.touchPoints[i].id, touchEvent_.touchPoints[i].x, touchEvent_.touchPoints[i].y); LOGE("Touch
        //     Info : screenx = %{public}f, screeny = %{public}f", touchEvent_.touchPoints[i].screenX,
        //     touchEvent_.touchPoints[i].screenY); LOGE("vtimeStamp = %{public}llu, isPressed = %{public}d",
        //     touchEvent_.touchPoints[i].timeStamp, touchEvent_.touchPoints[i].isPressed);
        // }
        //         eglCore_->OnTouch(touchEvent_.id, touchEvent_.x, touchEvent_.y, touchEvent_.type);
    } else {
        LOGE("Touch fail");
    }
}

napi_value PluginRender::Export(napi_env env, napi_value exports) {
    LOGE("PluginRender::Export");
    // Register JS API
    napi_property_descriptor desc[] = {
        DECLARE_NAPI_FUNCTION("testNapiThreadsafefunc", PluginRender::NapiThreadsafeFunc),
        DECLARE_NAPI_FUNCTION("startTest", PluginRender::NapiStartTest),
        DECLARE_NAPI_FUNCTION("registerCallback", PluginRender::NapiRegisterCallback),
        DECLARE_NAPI_FUNCTION("updateScreen", PluginRender::NapiUpdateScreen),
        DECLARE_NAPI_FUNCTION("keyEvent", PluginRender::NapiKeyEvent),
        DECLARE_NAPI_FUNCTION("windowCommand", PluginRender::NapiWindowCommand),
    };
    NAPI_CALL(env, napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc));
    return exports;
}

class RawFile {
public:
    void Init(NativeResourceManager *mgr) { mgr_ = mgr; }

    void ReadFile(std::string path, uint8_t *out) {
        RawFile *file = OH_ResourceManager_OpenRawFile(mgr_, path.c_str());
        size_t len = OH_ResourceManager_GetRawFileSize(file);
        int res = OH_ResourceManager_ReadRawFile(file, out, len);
        OH_ResourceManager_CloseRawFile(file);
    }
    void EnumFiles(std::string path, std::function<void(std::string, size_t)> callback) {
        RawDir *dir = OH_ResourceManager_OpenRawDir(mgr_, path.c_str());
        int count = OH_ResourceManager_GetRawFileCount(dir);
        if (count == 0) { // 因为空目录不会打包进raw，所以这是个文件
            RawFile *file = OH_ResourceManager_OpenRawFile(mgr_, path.c_str());
            size_t len = OH_ResourceManager_GetRawFileSize(file);
            OH_ResourceManager_CloseRawFile(file);
            callback(path, len);
        } else {
            for (int i = 0; i < count; i++) {
                std::string filename = OH_ResourceManager_GetRawFileName(dir, i);
                if (path.length() > 0) {
                    EnumFiles(path + "/" + filename, callback);
                } else {
                    EnumFiles(filename, callback);
                }
                // if (OH_ResourceManager_IsRawDir(mgr_, filename.c_str())) {
                //     EnumFiles(path + "/" + filename, callback);
                // } else {
                //     callback(path + "/" + filename);
                // }
            }
        }
        OH_ResourceManager_CloseRawDir(dir);
    }

private:
    NativeResourceManager *mgr_ = nullptr;
};

static RawFile ccrf;

int mkdir_p(const char *path, mode_t mode) {
    char tmp[PATH_MAX];
    char *p = NULL;
    struct stat sb;
    size_t len;

    /* 复制路径 */
    len = strnlen(path, PATH_MAX);
    if (len == 0 || len == PATH_MAX) {
        return -1;
    }
    memcpy(tmp, path, len);
    tmp[len] = '\0';

    /* 移除末尾的斜杠 */
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
    }

    /* 检查路径是否存在且是目录 */
    if (stat(tmp, &sb) == 0) {
        if (S_ISDIR(sb.st_mode)) {
            return 0;
        }
        return -1; /* 路径存在但不是目录 */
    }

    /* 递归创建目录 */
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            /* 检查路径是否存在 */
            if (stat(tmp, &sb) != 0) {
                /* 路径不存在 - 创建目录 */
                if (mkdir(tmp, mode) != 0) {
                    CLOGE("mkdir '%{public}s' failed", tmp);
                    return -1;
                }
            } else if (!S_ISDIR(sb.st_mode)) {
                /* 不是目录 */
                return -1;
            }
            *p = '/';
        }
    }
    /* 创建最终目录 */
    if (mkdir(tmp, mode) != 0) {
        CLOGE("mkdir '%{public}s' failed", tmp);
        return -1;
    }
    return 0;
}

bool is_synced(std::string dir, bool isset = false) {
    char files_synced[PATH_MAX];
    sprintf(files_synced, "%s/synced.txt", dir.c_str());
    if (isset) {
        FILE *fp = fopen(files_synced, "w");
        fclose(fp);
        return true;
    }

    struct stat s;
    if (stat(files_synced, &s) == 0) {
        CLOGE("'%{public}s' is exist\n", files_synced);
        return true;
    } else {
        CLOGE("'%{public}s' not exist\n", files_synced);
        return false;
    }
}

// 线程安全的 JavaScript 函数对象
napi_threadsafe_function g_threadsafeFunction;

typedef struct {
    napi_threadsafe_function tsfn;
    pthread_t thread_id;
    char filesDir[PARAM1024];
    char caseName[PARAM1024];
    NativeResourceManager *mNativeResMgr;
    PluginRender *instance;
    std::string result;
} ThreadContext;


// JavaScript 回调函数
static void CallbackFunction(napi_env env, napi_value jsCallback, void *context, void *data)
{
    // 在 JavaScript 环境中执行回调函数
    size_t argc = 1;
    napi_value argv[1];
    ThreadContext *ctx = static_cast<ThreadContext*>(data);
    napi_create_string_utf8(env, ctx->result.c_str(), NAPI_AUTO_LENGTH, &argv[0]);
    napi_call_function(env, nullptr, jsCallback, argc, argv, nullptr);
}

// 线程函数，在这里异步调用 JavaScript 函数
static void* ThreadFunction(void *data)
{
    ThreadContext* ctx = (ThreadContext*)data;
    sleep(2);

    OH_LOG_Print(LOG_APP, LOG_INFO, GLOBAL_RESMGR, "ThreadFunction", "param %{public}s", ctx->filesDir);
    if (ctx->instance) {
        if (!is_synced(ctx->filesDir)) {
            ccrf.Init(ctx->mNativeResMgr);

            ccrf.EnumFiles("", [=](std::string filename, size_t len) {
                int p = filename.length() - 1;
                for (; p > 0; p--) {
                    if (filename.c_str()[p] == '/') {
                        break;
                    }
                }
                std::string path = filename.substr(0, p);
                std::string name = filename.substr(p + 1);
                CLOGE("RawFile [%{public}s][%{public}s] size = %{public}ld", path.c_str(), name.c_str(), len);
                // 把文件拷贝到filesDir目录下
                char dst[1024];
                sprintf(dst, "%s/%s", ctx->filesDir, path.c_str());
                if (path.length() > 0) {
                    struct stat s;
                    if (stat(dst, &s) == 0) {
                        if (S_ISDIR(s.st_mode)) {
//                             CLOGE("'%{public}s' is dir\n", dst);
                        } else if (S_ISREG(s.st_mode)) {
//                             CLOGE("'%{public}s' is file\n", dst);
                        }
                    } else {
//                         CLOGE("'%{public}s' not exist\n", dst);
                        if(mkdir_p(dst, 0755)!=0){
                            exit(0);
                        }
                    }
                }
                sprintf(dst, "%s/%s", ctx->filesDir, filename.c_str());
                uint8_t *data = new uint8_t[len];
                ccrf.ReadFile(filename, data);
                FILE *fp = fopen(dst, "wb");
                if (fp) {
                    fwrite(data, 1, len, fp);
                    fclose(fp);
//                     CLOGE("ok %{public}s => %{public}s", filename.c_str(), dst);
                } else {
//                     CLOGE("fail %{public}s => %{public}s", filename.c_str(), dst);
                }
                delete[] data;
            });
            is_synced(ctx->filesDir, true);
        }
        CLOGE("file %{public}s, case %{public}s", ctx->filesDir, ctx->caseName);
        ctx->result = ctx->instance->eglCore_->StartTest(ctx->filesDir, ctx->caseName);
    }
    
    // 在另一个线程中异步调用 JavaScript 函数
    napi_call_threadsafe_function(ctx->tsfn, ctx, napi_tsfn_nonblocking);
    pthread_detach(ctx->thread_id);
    return NULL;
}

napi_value PluginRender::NapiThreadsafeFunc(napi_env env, napi_callback_info info) {
    size_t argc = PARAM4;
    napi_value argv[PARAM4];
    napi_value thisArg;
    napi_status status;
    napi_value exportInstance;
    size_t result;
    const napi_extended_error_info *extended_error_info;
    OH_NativeXComponent *nativeXComponent = nullptr;
    
    int32_t ret = 0;
    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {};
    uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;

    // 解析传入的参数
    status = napi_get_cb_info(env, info, &argc, argv, &thisArg, NULL);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Failed to parse arguments");
        return NULL;
    }

    status = napi_get_named_property(env, thisArg, OH_NATIVE_XCOMPONENT_OBJ, &exportInstance);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Failed to parse xcomponent object");
        return nullptr;
    }
    
    status = napi_unwrap(env, exportInstance, reinterpret_cast<void **>(&nativeXComponent));
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Failed to unwrap xcomponent value");
        return nullptr;
    }
    
    ret = OH_NativeXComponent_GetXComponentId(nativeXComponent, idStr, &idSize);
    if (ret != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        napi_throw_error(env, NULL, "Failed to get xcomponent id");
        return nullptr;
    }
    
    std::string id(idStr);
    PluginRender *instance = PluginRender::GetInstance(id);
    
    // 检查参数数量
    if (argc < PARAM4) {
        napi_throw_error(env, NULL, "Expected 4 arguments");
        return NULL;
    }
    pthread_t thread;
    ThreadContext *ctx = (ThreadContext *)malloc(sizeof(ThreadContext));
    ctx->instance = instance;
    ctx->mNativeResMgr = OH_ResourceManager_InitNativeResourceManager(env, argv[PARAM0]);
    if (ctx->mNativeResMgr == NULL) {
        napi_throw_error(env, NULL, "get resource manager failed!");
        return nullptr;
    }
    
    status = napi_get_value_string_utf8(env, argv[PARAM1], ctx->filesDir, PARAM1024, &result);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "get file dir failed!");
        return nullptr;
    }

    status = napi_get_value_string_utf8(env, argv[PARAM2], ctx->caseName, PARAM1024, &result);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "get case name failed!");
        return nullptr;
    }
    
    // 构造js回调函数
    napi_value name;
    napi_create_string_utf8(env, "NapiThreadsafeFunc", NAPI_AUTO_LENGTH, &name);
    status = napi_create_threadsafe_function(env, argv[PARAM3], nullptr, name, 0, 1,
        nullptr, nullptr, nullptr, CallbackFunction, &ctx->tsfn);
    if (status != napi_ok) {
        status = napi_get_last_error_info(env, &extended_error_info);
        if (status == napi_ok && extended_error_info != NULL) {
            const char *errorMessage = extended_error_info->error_message != NULL ? 
                extended_error_info->error_message : "Unknown error";
            OH_LOG_Print(LOG_APP, LOG_ERROR, GLOBAL_RESMGR, TAG, "errmsg %{public}s!, engine_err_code %{public}d!.",
                         errorMessage, extended_error_info->engine_error_code);
            std::string res = "Failed to create threadsafe function em = " + std::string(errorMessage) +
                              ", eec = " + std::to_string(extended_error_info->engine_error_code) +
                              ", ec = " + std::to_string(extended_error_info->error_code);
            napi_throw_error(env, NULL, res.c_str());
            return NULL;
        }
    }
    
    pthread_create(&ctx->thread_id, nullptr, ThreadFunction, ctx);
    

    // 返回结果
    napi_value resultValue;
    status = napi_create_int32(env, ret, &resultValue);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Failed to create result value");
        return NULL;
    }

    return resultValue;
}

napi_value PluginRender::NapiStartTest(napi_env env, napi_callback_info info) {
    LOGD("NapiStartTest");
    napi_value exportInstance;
    napi_value thisArg;
    napi_status status;
    OH_NativeXComponent *nativeXComponent = nullptr;

    int32_t ret;
    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {};
    uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;

    size_t argc_ = 3;
    napi_value argv_[3];

    // napi_value thisArg;
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc_, argv_, &thisArg, NULL));

    status = napi_get_named_property(env, thisArg, OH_NATIVE_XCOMPONENT_OBJ, &exportInstance);
    if (status != napi_ok) {
        return nullptr;
    }

    status = napi_unwrap(env, exportInstance, reinterpret_cast<void **>(&nativeXComponent));
    if (status != napi_ok) {
        return nullptr;
    }

    ret = OH_NativeXComponent_GetXComponentId(nativeXComponent, idStr, &idSize);
    if (ret != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        return nullptr;
    }

    std::string id(idStr);
    PluginRender *instance = PluginRender::GetInstance(id);
    if (instance) {
        size_t result;

        char filesDir[1024];
        status = napi_get_value_string_utf8(env, argv_[1], filesDir, 1024, &result);
        if (status != napi_ok) {
            return nullptr;
        }

        char caseName[1024];
        status = napi_get_value_string_utf8(env, argv_[2], caseName, 1024, &result);
        if (status != napi_ok) {
            return nullptr;
        }

        if (!is_synced(filesDir)) {
            NativeResourceManager *mNativeResMgr = OH_ResourceManager_InitNativeResourceManager(env, argv_[0]);
            ccrf.Init(mNativeResMgr);

            ccrf.EnumFiles("", [=](std::string filename, size_t len) {
                int p = filename.length() - 1;
                for (; p > 0; p--) {
                    if (filename.c_str()[p] == '/') {
                        break;
                    }
                }
                std::string path = filename.substr(0, p);
                std::string name = filename.substr(p + 1);
//                 CLOGE("RawFile [%{public}s][%{public}s] size = %{public}ld", path.c_str(), name.c_str(), len);
                // 把文件拷贝到filesDir目录下
                char dst[1024];
                sprintf(dst, "%s/%s", filesDir, path.c_str());
                if (path.length() > 0) {
                    struct stat s;
                    if (stat(dst, &s) == 0) {
                        if (S_ISDIR(s.st_mode)) {
//                             CLOGE("'%{public}s' is dir\n", dst);
                        } else if (S_ISREG(s.st_mode)) {
//                             CLOGE("'%{public}s' is file\n", dst);
                        }
                    } else {
//                         CLOGE("'%{public}s' not exist\n", dst);
                        if(mkdir_p(dst, 0755)!=0){
                            exit(0);
                        }
                    }
                }
                sprintf(dst, "%s/%s", filesDir, filename.c_str());
                uint8_t *data = new uint8_t[len];
                ccrf.ReadFile(filename, data);
                FILE *fp = fopen(dst, "wb");
                if (fp) {
                    fwrite(data, 1, len, fp);
                    fclose(fp);
//                     CLOGE("ok %{public}s => %{public}s", filename.c_str(), dst);
                } else {
//                     CLOGE("fail %{public}s => %{public}s", filename.c_str(), dst);
                }
                delete[] data;
            });
            is_synced(filesDir, true);
        }
        //         exit(0);
        // uint32_t sessionId =
        instance->eglCore_->StartTest(filesDir, caseName);

        // napi_value sid;
        // napi_create_uint32(env, sessionId, &sid);
        // return sid;
    }
    return nullptr;
}

napi_value PluginRender::NapiRegisterCallback(napi_env env, napi_callback_info info) {
    LOGD("NapiRegisterCallback");
    napi_value exportInstance;
    napi_value thisArg;
    napi_status status;
    OH_NativeXComponent *nativeXComponent = nullptr;

    int32_t ret;
    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {};
    uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;

    size_t argc_ = 2;
    napi_value argv_[2];

    // napi_value thisArg;
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc_, argv_, &thisArg, NULL));

    status = napi_get_named_property(env, thisArg, OH_NATIVE_XCOMPONENT_OBJ, &exportInstance);
    if (status != napi_ok) {
        return nullptr;
    }

    status = napi_unwrap(env, exportInstance, reinterpret_cast<void **>(&nativeXComponent));
    if (status != napi_ok) {
        return nullptr;
    }

    ret = OH_NativeXComponent_GetXComponentId(nativeXComponent, idStr, &idSize);
    if (ret != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        return nullptr;
    }

    std::string id(idStr);
    PluginRender *instance = PluginRender::GetInstance(id);
    if (instance) {
        CCNapiCallback::GI().RegistCallbackFunction(env, argv_[1], id, argv_[0]);
        //         CCNapiCallback::GI().CallCallbackFunction(id, "hello world");
    }
    return nullptr;
}

napi_value PluginRender::NapiUpdateScreen(napi_env env, napi_callback_info info) {
    // LOGD("NapiUpdateScreen");
    napi_value exportInstance;
    napi_value thisArg;
    napi_status status;
    OH_NativeXComponent *nativeXComponent = nullptr;

    int32_t ret;
    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {};
    uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;

    // napi_value thisArg;
    NAPI_CALL(env, napi_get_cb_info(env, info, NULL, NULL, &thisArg, NULL));

    status = napi_get_named_property(env, thisArg, OH_NATIVE_XCOMPONENT_OBJ, &exportInstance);
    if (status != napi_ok) {
        return nullptr;
    }

    status = napi_unwrap(env, exportInstance, reinterpret_cast<void **>(&nativeXComponent));
    if (status != napi_ok) {
        return nullptr;
    }

    ret = OH_NativeXComponent_GetXComponentId(nativeXComponent, idStr, &idSize);
    if (ret != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        return nullptr;
    }

    std::string id(idStr);
    PluginRender *instance = PluginRender::GetInstance(id);
    //     if (instance) {
    //         std::lock_guard<std::mutex> lock(instance->coreMutex_);
    //         //         CLOGE("UpdateScreen -1");
    // //                 instance->eglCore_->UpdateScreen();
    //         instance->eglCore_->FreshUpdateResult();
    // //         CLOGE("UpdateScreen 0");
    //         // 返回更新数据：窗口xywh
    //         auto ur = instance->eglCore_->GetUpdateResult();
    //         napi_value retValue;
    //         napi_create_object(env, &retValue);
    //         //         napi_value fps;
    //         //         napi_create_double(env, instance->eglCore_->UpdateScreen(), &fps);
    //         //         napi_set_named_property(env, retValue, "fps", fps);
    // //         CLOGE("UpdateScreen 1");
    //         napi_value x, y, w, h, destoryWindow, subWindowId;
    //         napi_create_int32(env, ur->x, &x);
    //         napi_create_int32(env, ur->y, &y);
    //         napi_create_int32(env, ur->w, &w);
    //         napi_create_int32(env, ur->h, &h);
    //         napi_create_int32(env, ur->destoryWindow, &destoryWindow);
    //         napi_create_int64(env, ur->subWindowId, &subWindowId);
    //         napi_set_named_property(env, retValue, "x", x);
    //         napi_set_named_property(env, retValue, "y", y);
    //         napi_set_named_property(env, retValue, "w", w);
    //         napi_set_named_property(env, retValue, "h", h);
    //         napi_set_named_property(env, retValue, "destoryWindow", destoryWindow);
    //         napi_set_named_property(env, retValue, "subWindowId", subWindowId);
    // //         CLOGE("UpdateScreen 2");
    //         return retValue;
    //     }
    return nullptr;
}

bool PluginRender::OnVsync() {
    std::lock_guard<std::mutex> lock(coreMutex_);
    if (eglCore_ != nullptr) {
        eglCore_->UpdateScreen();
        return true;
    } else {
        return false;
    }
}

napi_value PluginRender::NapiKeyEvent(napi_env env, napi_callback_info info) {
    // LOGD("NapiKeyEvent");
    napi_value exportInstance;
    napi_value thisArg;
    napi_status status;
    OH_NativeXComponent *nativeXComponent = nullptr;

    int32_t ret;
    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {};
    uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;

    size_t argc_ = 3;
    napi_value argv_[3];

    // napi_value thisArg;
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc_, argv_, &thisArg, NULL));

    {
        int32_t wid;
        uint32_t keycode, updown;
        napi_get_value_int32(env, argv_[0], &wid);
        napi_get_value_uint32(env, argv_[1], &keycode);
        napi_get_value_uint32(env, argv_[2], &updown);
        // if (wid < 0) {
        //     wid = CcRdpdQuery::GI().BindMainWindow(wid);
        // }
        // if (CcRdpdQuery::GI().IsReady(wid)) {
        //     CcRdpdQuery::GI().SendKeyEvent(wid, keycode, updown);
        // }
    }
    return nullptr;
}

napi_value PluginRender::NapiWindowCommand(napi_env env, napi_callback_info info) {
    // LOGD("NapiKeyEvent");
    napi_value exportInstance;
    napi_value thisArg;
    napi_status status;
    OH_NativeXComponent *nativeXComponent = nullptr;

    int32_t ret;
    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {};
    uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;

    size_t argc_ = 2;
    napi_value argv_[2];

    // napi_value thisArg;
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc_, argv_, &thisArg, NULL));

    {
        int32_t wid;
        napi_get_value_int32(env, argv_[0], &wid);

        uint32_t command;
        napi_get_value_uint32(env, argv_[1], &command);

        // if(wid<0){
        //     wid=CcRdpdQuery::GI().BindMainWindow(wid);
        // }
        // if (CcRdpdQuery::GI().IsReady(wid)) {
        //     CcRdpdQuery::GI().SendWindowCommand(wid, command);
        // }
    }
    return nullptr;
}

#ifdef __cplusplus
}
#endif