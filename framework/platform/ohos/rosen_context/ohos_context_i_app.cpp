/*
 * Copyright (c) 2022 Shenzhen Kaihong Digital Industry Development Co., Ltd.
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

#include <stdio.h>
#include "ohos_context_i.h"
using namespace OHOS;

OhosContextI *g_instance = nullptr;

void OhosContextI::SetInstance(void *instance) {
   // printf("iapp: setinstance\n");
   g_instance = static_cast<OhosContextI *>(instance);
}
 
OhosContextI &OhosContextI::GetInstance() {
   // printf("iapp getinstance\n");
   return *g_instance;
}

void OhosContextI::HiLog(const char *format, ...) {}