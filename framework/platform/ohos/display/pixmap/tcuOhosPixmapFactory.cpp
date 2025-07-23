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

#include "display/tcuOhosNativeDisplay.hpp"
#include "tcuOhosPixmapFactory.hpp"
#include "tcuOhosNativePixmap.hpp"
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

NativePixmap* OhosPixmapFactory::createPixmap (NativeDisplay* nativeDisplay,
                                           int            width,
                                           int            height) const
{
    //TODO create pixmap
    // OhosDisplay*                display        = dynamic_cast<OhosDisplay*>(nativeDisplay);
    if (nativeDisplay != nullptr) {
        printf("OhosPixmapFactory::createPixmap width=%d, height=%d,\n", width, height);
    }
    
    return new OhosPixmap();
}

} // egl
} // OHOS
} // tcu