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

#ifndef _PLUGIN_COMMON_H_
#define _PLUGIN_COMMON_H_

#include <string>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <functional>
#include <iostream>
#include <queue>
#include <string>
#include <vector>
#include <hilog/log.h>

#define APP_LOG_DOMAIN 0x0001
#define APP_LOG_TAG "vkglcts"
#define LOGI(...) ((void)OH_LOG_Print(LOG_APP, LOG_INFO, APP_LOG_DOMAIN, APP_LOG_TAG, __VA_ARGS__))
#define LOGD(...) ((void)OH_LOG_Print(LOG_APP, LOG_DEBUG, APP_LOG_DOMAIN, APP_LOG_TAG, __VA_ARGS__))
#define LOGW(...) ((void)OH_LOG_Print(LOG_APP, LOG_WARN, APP_LOG_DOMAIN, APP_LOG_TAG, __VA_ARGS__))
#define LOGE(...) ((void)OH_LOG_Print(LOG_APP, LOG_ERROR, APP_LOG_DOMAIN, APP_LOG_TAG, __VA_ARGS__))

constexpr int32_t RGB_565 = 2;
constexpr int32_t RGBA_8888 = 3;

constexpr int32_t STR_MAX_SIZE = 200;
constexpr int32_t LONG_STR_MAX_SIZE = 1024;
constexpr int32_t ERR_OK = 0;
constexpr int8_t NO_ERROR = 0;
constexpr int8_t ERROR = -1;
constexpr uint8_t PARAM0 = 0;
constexpr uint8_t PARAM1 = 1;
constexpr uint8_t PARAM2 = 2;
constexpr uint8_t PARAM3 = 3;
constexpr uint8_t PARAM4 = 4;
constexpr uint8_t PARAM5 = 5;
constexpr uint8_t PARAM6 = 6;
constexpr uint8_t PARAM7 = 7;
constexpr uint8_t PARAM8 = 8;
constexpr uint8_t PARAM9 = 9;
constexpr uint8_t PARAM10 = 10;
constexpr uint8_t PARAM11 = 11;
constexpr uint8_t PARAM12 = 12;
constexpr uint8_t PARAM60 = 60;
constexpr uint8_t PARAM100 = 100;
constexpr uint16_t PARAM1000 = 1000;
constexpr uint16_t PARAM1024 = 1024;
constexpr uint32_t PARAM100W = 1000000;

constexpr int32_t ARGS_ONE = 1;
constexpr int32_t ARGS_TWO = 2;
constexpr int32_t ONLY_CALLBACK_MAX_PARA = 1;
constexpr int32_t ONLY_CALLBACK_MIN_PARA = 0;

#endif // _PLUGIN_COMMON_H_