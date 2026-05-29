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
#ifndef _TCUOHOSROSENNATIVEWINDOW_HPP
#define _TCUOHOSROSENNATIVEWINDOW_HPP

#include "egluGLContextFactory.hpp"
#include "eglwLibrary.hpp"
#include "eglwFunctions.hpp"
#include "eglwEnums.hpp"
#include "deUniquePtr.hpp"

#include "display/tcuOhosNativeDisplay.hpp"

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

class OhosWindow : public NativeWindow
{
public:
    static const Capability    CAPABILITIES        = Capability(CAPABILITY_CREATE_SURFACE_LEGACY |
                                                             CAPABILITY_CREATE_SURFACE_PLATFORM |
                                                             CAPABILITY_CREATE_SURFACE_PLATFORM_EXTENSION |
                                                             CAPABILITY_GET_SURFACE_SIZE |
                                                             CAPABILITY_SET_SURFACE_SIZE |
                                                             CAPABILITY_GET_SCREEN_SIZE);

    OhosWindow        (OhosDisplay&    display, const WindowParams&    params);

    eglw::EGLNativeWindowType    getLegacyNative            (void);
    void*                        getPlatformExtension    (void);
    void*                        getPlatformNative        (void);

    IVec2                        getSurfaceSize        (void) const;
    void                        setSurfaceSize        (IVec2 size);
    IVec2                        getScreenSize        (void) const { return getSurfaceSize(); }

};

} // egl
} // OHOS
} // tcu
#endif // _TCUOHOSROSENNATIVEWINDOW_HPP