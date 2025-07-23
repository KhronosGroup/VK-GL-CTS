//
// Created on 2024/9/26.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#include "napi_callback.h"
#include <map>
#include <uv.h>
#include "cc_common.h"

struct CallFunc {
    napi_env env_;
    napi_ref funcRef_;
    napi_ref thisVarRef_;
};

static std::map<std::string, CallFunc> callFuncs_;

#define CC_ASSERT(btrue)   \
     if (!(btrue)) {       \
     }
//     assert(btrue)

void CCNapiCallback::RegistCallbackFunction(napi_env env, napi_value thisVar, std::string id, napi_value func)
{
    CLOGE("RegistCallbackFunction %{public}s",id.c_str());
    callFuncs_[id].env_ = env;
    napi_status result_status = napi_create_reference(env, func, 1, &callFuncs_[id].funcRef_);
    CC_ASSERT(result_status == napi_ok);
    result_status = napi_create_reference(env, thisVar, 1, &callFuncs_[id].thisVarRef_);
    CC_ASSERT(result_status == napi_ok);
}

void CCNapiCallback::CallCallbackFunction(std::string id, std::string message)
{
    if(callFuncs_.find(id)==callFuncs_.end()){
        return;
    }
//     CLOGE("DoCallback2 (%{public}s msg:%{public}s", id.c_str(), message.c_str());
    CallFunc *pAsyncFuncs = &callFuncs_[id];

    uv_loop_s *loop = nullptr;
    napi_get_uv_event_loop(pAsyncFuncs->env_, &loop);
    uv_work_t *work = new uv_work_t;
    
    struct AsyncCallData {
        char *id;
        char *message;
    };
    AsyncCallData *data = (AsyncCallData *)malloc(sizeof(AsyncCallData));
    data->id=(char *)malloc(id.length()+1);
    data->message=(char *)malloc(message.length()+1);
    strcpy(data->id, id.c_str());
    strcpy(data->message, message.c_str());

    work->data = data;
    uv_queue_work(
        loop, work, [](uv_work_t *work) {},
        [](uv_work_t *work, int status) {
            AsyncCallData *data = (AsyncCallData *)work->data;
            CallFunc *pAsyncFuncs = &callFuncs_[std::string(data->id)];
            napi_value thisVar;
            napi_status result_status = napi_get_reference_value(pAsyncFuncs->env_, pAsyncFuncs->thisVarRef_, &thisVar);
            CC_ASSERT(result_status == napi_ok);
            napi_value func;
            result_status = napi_get_reference_value(pAsyncFuncs->env_, pAsyncFuncs->funcRef_, &func);
            CC_ASSERT(result_status == napi_ok);
            napi_value args[1];
            napi_create_string_utf8(pAsyncFuncs->env_, data->message, NAPI_AUTO_LENGTH, &args[0]);
            // napi_create_string_utf8(pAsyncFuncs->env_, data->id, NAPI_AUTO_LENGTH, &args[1]);
            napi_value result;
            result_status = napi_call_function(pAsyncFuncs->env_, thisVar, func, 1, args, &result);
            CC_ASSERT(result_status == napi_ok);
            free(data->id);
            free(data->message);
            free(data);

            delete work;
        });
}