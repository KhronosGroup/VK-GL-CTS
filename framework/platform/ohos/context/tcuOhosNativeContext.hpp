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
#ifndef _TCUOHOSROSENNATIVECONTEXT_HPP
#define _TCUOHOSROSENNATIVECONTEXT_HPP

#include "egluGLContextFactory.hpp"
#include "eglwLibrary.hpp"
#include "eglwFunctions.hpp"
#include "eglwEnums.hpp"
#include "deUniquePtr.hpp"
#include "glwFunctions.hpp"
#include "tcuRenderTarget.hpp"

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
using glu::RenderContext;
using eglu::GLContextFactory;
using eglu::NativeDisplay;
using eglu::NativeDisplayFactory;
using eglu::NativeWindow;
using eglu::NativeWindowFactory;
using eglu::NativePixmap;
using eglu::NativePixmapFactory;
using eglu::WindowParams;
using tcu::TextureLevel;

class OhosRendContext : public RenderContext
{
public:
    OhosRendContext(const glu::RenderConfig& config, const tcu::CommandLine& cmdLine);
    virtual ~OhosRendContext(void);

    glu::ContextType getType(void) const    { return m_contextType; }
    const glw::Functions&        getFunctions    (void) const    { return m_glFunctions; }
    const tcu::RenderTarget&    getRenderTarget    (void) const { return m_renderTarget; }
    void postIterate    (void);

    virtual glw::GenericFuncType    getProcAddress    (const char* name) const { return m_egl.getProcAddress(name); }
private:
    const eglw::DefaultLibrary      m_egl;
    const glu::ContextType        m_contextType;
    glw::Functions            m_glFunctions;
    tcu::RenderTarget        m_renderTarget;
};

} // egl
} // OHOS
} // tcu
#endif // _TCUOHOSROSENNATIVECONTEXT_HPP