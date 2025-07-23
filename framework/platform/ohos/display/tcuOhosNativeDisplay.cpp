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

#include "tcuOhosNativeDisplay.hpp"
#include "egluGLContextFactory.hpp"
#include "eglwLibrary.hpp"
#include "eglwFunctions.hpp"
#include "eglwEnums.hpp"
#include "deUniquePtr.hpp"

using namespace tcu;
using namespace OHOS_ROSEN;
using namespace egl;

using std::string;
using de::MovePtr;
using de::UniquePtr;
using glu::ContextFactory;
using eglu::GLContextFactory;
using eglu::NativeDisplay;
using eglu::NativeDisplayFactory;
using eglu::NativeWindow;
using eglu::NativeWindowFactory;
using eglu::NativePixmap;
using eglu::NativePixmapFactory;
using eglu::WindowParams;
using tcu::TextureLevel;

OhosDisplay::OhosDisplay (void) 
    : NativeDisplay(eglu::NativeDisplay::CAPABILITY_GET_DISPLAY_LEGACY)
{
    //TODO: create display
    printf("OhosDisplay::OhosDisplay\n");
}

void* OhosDisplay::getPlatformNative (void) {
    //TODO: 获取rosen display
    printf("OhosDisplay::getPlatformNative\n");
    return this;
}

eglw::EGLNativeDisplayType OhosDisplay::getPlatformExtension (void)    
{
    //TODO: 获取rosen display
    printf("OhosDisplay::getPlatformExtension\n");
    return reinterpret_cast<eglw::EGLNativeDisplayType>(this); 
}

eglw::EGLNativeDisplayType OhosDisplay::getLegacyNative (void)    
{ 
    //TODO: 获取rosen display
    printf("OhosDisplay::getLegacyNative\n");
    return EGL_DEFAULT_DISPLAY; 
}
