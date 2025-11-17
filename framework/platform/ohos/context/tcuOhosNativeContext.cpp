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
#include "tcuOhosNativeContext.hpp"
#include "egluGLContextFactory.hpp"
#include "eglwLibrary.hpp"
#include "eglwFunctions.hpp"
#include "eglwEnums.hpp"
#include "deUniquePtr.hpp"
#include "tcuCommandLine.hpp"
#include "deDynamicLibrary.hpp"

#include "ohos_context_i.h"

using namespace tcu;
using namespace OHOS_ROSEN;
using namespace egl;

using de::MovePtr;
using de::UniquePtr;
using eglu::GLContextFactory;
using eglu::NativeDisplay;
using eglu::NativeDisplayFactory;
using eglu::NativePixmap;
using eglu::NativePixmapFactory;
using eglu::NativeWindow;
using eglu::NativeWindowFactory;
using eglu::WindowParams;
using glu::ContextFactory;
using std::string;
using tcu::TextureLevel;

class GLFunctionLoader : public glw::FunctionLoader
{
public:
    GLFunctionLoader(const char *path)
        : m_library(path)
    {
    }

    glw::GenericFuncType get(const char *name) const
    {
        return m_library.getFunction(name);
    }

private:
    de::DynamicLibrary m_library;
};

OhosRendContext::OhosRendContext(const glu::RenderConfig &config, const tcu::CommandLine &cmdLine)
    : m_contextType(config.type)
{
    DE_UNREF(cmdLine);

    printf("~~~~~~~OhosRendContext~~~~~~~~\n");
    printf("config.width = %d\n", config.width);
    printf("config.height = %d\n", config.height);
    printf("config.redBits = %d\n", config.redBits);
    printf("config.greenBits = %d\n", config.greenBits);
    printf("config.blueBits = %d\n", config.blueBits);
    printf("config.alphaBits = %d\n", config.alphaBits);
    printf("config.depthBits = %d\n", config.depthBits);
    printf("config.stencilBits = %d\n", config.stencilBits);
    printf("config.numSamples = %d\n", config.numSamples);
    printf("config.type.getMajorVersion() = %d\n", config.type.getMajorVersion());
    printf("config.type.getMinorVersion() = %d\n", config.type.getMinorVersion());
    printf("~~~~~~~~~~~~~~~\n");
    // TODO:
    int32_t w = config.width;
    int32_t h = config.height;
    if (w == -1)
    {
        w = 512;
    }
    if (h == -1)
    {
        h = 512;
    }

    OHOS::RCI_GLES_VERSION ver;
    if (config.type.getMajorVersion() == 2 && config.type.getMinorVersion() == 0)
    {
        ver = OHOS::RCI_GLES_VERSION::V20;
    }
    else if (config.type.getMajorVersion() == 3 && config.type.getMinorVersion() == 0)
    {
        ver = OHOS::RCI_GLES_VERSION::V30;
    }
    else if (config.type.getMajorVersion() == 3 && config.type.getMinorVersion() == 1)
    {
        ver = OHOS::RCI_GLES_VERSION::V31;
    }
    else if (config.type.getMajorVersion() == 3 && config.type.getMinorVersion() == 2)
    {
        ver = OHOS::RCI_GLES_VERSION::V32;
    }
    else
    {
        printf("not supoort version: ~~~~~~~~~~~~~~~\n");
        throw tcu::NotSupportedError("not support version");
    }

    OHOS::RCI_PIXEL_FORMAT pf = {
        .redBits = 8,
        .greenBits = 8,
        .blueBits = 8,
        .alphaBits = 8,
        .depthBits = 24,
        .stencilBits = 8,
        .numSamples = 4,
    };
    if (config.redBits != -1)
    {
        pf.redBits = config.redBits;
    }
    if (config.greenBits != -1)
    {
        pf.greenBits = config.greenBits;
    }
    if (config.blueBits != -1)
    {
        pf.blueBits = config.blueBits;
    }
    if (config.alphaBits != -1)
    {
        pf.alphaBits = config.alphaBits;
    }
    if (config.depthBits != -1)
    {
        pf.depthBits = config.depthBits;
    }
    if (config.stencilBits != -1)
    {
        pf.stencilBits = config.stencilBits;
    }
    if (config.numSamples != -1)
    {
        pf.numSamples = config.numSamples;
    }

    OHOS::RCI_SURFACE_TYPE surfaceType;
    switch (config.surfaceType)
    {
    case glu::RenderConfig::SURFACETYPE_DONT_CARE:
        surfaceType = OHOS::RCI_SURFACE_TYPE::NONE;
        break;
    case glu::RenderConfig::SURFACETYPE_OFFSCREEN_NATIVE:
        surfaceType = OHOS::RCI_SURFACE_TYPE::PIXMAP;
        break;
    case glu::RenderConfig::SURFACETYPE_OFFSCREEN_GENERIC:
        surfaceType = OHOS::RCI_SURFACE_TYPE::PBUFFER;
        break;
    case glu::RenderConfig::SURFACETYPE_WINDOW:
        surfaceType = OHOS::RCI_SURFACE_TYPE::WINDOW;
        break;
    case glu::RenderConfig::SURFACETYPE_LAST:
        TCU_CHECK_INTERNAL(false);
    }

    OHOS::RCI_PROFILE profile;
    switch (config.type.getProfile())
    {
    case glu::PROFILE_ES:
        profile = OHOS::RCI_PROFILE::ES;
        break;
    case glu::PROFILE_CORE:
        profile = OHOS::RCI_PROFILE::CORE;
        break;
    case glu::PROFILE_COMPATIBILITY:
        profile = OHOS::RCI_PROFILE::COMPATIBILITY;
        break;
    case glu::PROFILE_LAST:
        TCU_CHECK_INTERNAL(false);
    }

    int flags = 0;
    if ((config.type.getFlags() & glu::CONTEXT_DEBUG) != 0)
        flags |= static_cast<int>(OHOS::RCI_CONTEXT_FLAG::DEBUG);

    if ((config.type.getFlags() & glu::CONTEXT_ROBUST) != 0)
        flags |= static_cast<int>(OHOS::RCI_CONTEXT_FLAG::ROBUST);

    if ((config.type.getFlags() & glu::CONTEXT_FORWARD_COMPATIBLE) != 0)
        flags |= static_cast<int>(OHOS::RCI_CONTEXT_FLAG::FORWARD_COMPATIBLE);

    if (!OHOS::OhosContextI::GetInstance().SetConfig(w, h, ver, pf, surfaceType, profile, static_cast<OHOS::RCI_CONTEXT_FLAG>(flags)))
    {
        throw tcu::NotSupportedError("not support context");
    }
    OHOS::OhosContextI::GetInstance().InitNativeWindow();
    OHOS::OhosContextI::GetInstance().InitEglSurface();
    OHOS::OhosContextI::GetInstance().InitEglContext();
    OHOS::OhosContextI::GetInstance().MakeCurrent();

    if (config.type.getMajorVersion() == 2 || config.type.getMajorVersion() == 3)
    {
        GLFunctionLoader loader("libGLESv3.so");
        glu::initFunctions(&m_glFunctions, &loader, config.type.getAPI());
    }
    else
    {
        throw tcu::NotSupportedError("not support version");
    }
    while(m_glFunctions.getError()!=0) 
    {
        printf("err pass\n");
    }

    m_renderTarget = tcu::RenderTarget(512, 512, PixelFormat(8, 8, 8, 8),
                                       OHOS::OhosContextI::GetInstance().GetAttrib(EGL_DEPTH_SIZE),
                                       OHOS::OhosContextI::GetInstance().GetAttrib(EGL_STENCIL_SIZE),
                                       OHOS::OhosContextI::GetInstance().GetAttrib(EGL_SAMPLES));
};

OhosRendContext::~OhosRendContext(void)
{
    printf("~~~~~~~DEOhosRendContext~~~~~~~~\n");
    OHOS::OhosContextI::GetInstance().SwapBuffer();
};

void OhosRendContext::postIterate(void)
{
    OHOS::OhosContextI::GetInstance().SwapBuffer();
}