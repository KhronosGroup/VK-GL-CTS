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
#ifndef _TCUOHOSROSENNATIVEDISPLAY_HPP
#define _TCUOHOSROSENNATIVEDISPLAY_HPP

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

class OhosLibrary : public eglw::DefaultLibrary
{
public:
    OhosLibrary (void)
        : eglw::DefaultLibrary("libEGL_impl.so")
    {
    }
};


class OhosDisplay : public NativeDisplay
{
public:
    static const Capability CAPABILITIES        = Capability(CAPABILITY_GET_DISPLAY_LEGACY);

    OhosDisplay                (void);
    // virtual ~OhosDisplay         (void) {};

    void*                        getPlatformNative        (void);
    eglw::EGLNativeDisplayType    getPlatformExtension    (void);
    eglw::EGLNativeDisplayType    getLegacyNative            (void);

    // NativeDisplay&                getOhosDisplay            (void)            { return *this;}
    const eglw::Library&        getLibrary                (void) const    { return m_library;    }
    const eglw::EGLAttrib*        getPlatformAttributes    (void) const    { return DE_NULL; }

private:
    OhosLibrary                        m_library;
};

} // egl
} // OHOS
} // tcu
#endif // _TCUOHOSROSENNATIVEDISPLAY_HPP