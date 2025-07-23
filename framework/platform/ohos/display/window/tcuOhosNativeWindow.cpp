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

#include "tcuOhosNativeWindow.hpp"
#include "egluGLContextFactory.hpp"
#include "eglwLibrary.hpp"
#include "eglwFunctions.hpp"
#include "eglwEnums.hpp"
#include "deUniquePtr.hpp"

namespace tcu
{
namespace OHOS_ROSEN
{
namespace egl
{

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

OhosWindow::OhosWindow (OhosDisplay& display, const WindowParams& params)
    : NativeWindow    (CAPABILITIES)
{
    //TODO: 创建rosen window
    if (display.getPlatformNative() != nullptr) {
        printf("OhosWindow params: width=%d, height=%d\n", params.width, params.height);
    }
    printf("OhosWindow::OhosWindow\n");
}

eglw::EGLNativeWindowType OhosWindow::getLegacyNative (void)
{ 
    //TODO: 创建rosen window
    printf("OhosWindow::getLegacyNative\n");
    return nullptr; 
}

void* OhosWindow::getPlatformExtension (void)
{ 
    //TODO: 创建rosen window
    printf("OhosWindow::getPlatformExtension\n");
    return nullptr; 
}

void* OhosWindow::getPlatformNative (void)
{ 
    //TODO: 创建rosen window
    printf("OhosWindow::getPlatformNative\n");
    return nullptr; 
}

IVec2 OhosWindow::getSurfaceSize (void) const
{
    //TODO: 设置surface
    IVec2 ret;
    return ret;
}

void OhosWindow::setSurfaceSize (IVec2 size)
{
    //TODO: 设置size
    printf("setSurfaceSize IVec2: %d\n", size.x());
}

} // egl
} // OHOS
} // tcu