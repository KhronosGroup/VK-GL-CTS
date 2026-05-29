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
#ifndef _TCUOHOSCONTEXTFACTORY_HPP
#define _TCUOHOSCONTEXTFACTORY_HPP

#include "egluGLContextFactory.hpp"
#include "eglwLibrary.hpp"
#include "eglwFunctions.hpp"
#include "eglwEnums.hpp"
#include "deUniquePtr.hpp"
#include "glwFunctions.hpp"
#include "tcuRenderTarget.hpp"

namespace tcu
{
namespace OHOS_ROSEN
{
namespace egl
{

class OhosContextFactory : public glu::ContextFactory
{
public:
    OhosContextFactory (void);
    virtual glu::RenderContext*	createContext (const glu::RenderConfig& config, 
        const tcu::CommandLine& cmdLine, const glu::RenderContext* sharedContext) const;
};

}
}
}
#endif // _TCUOHOSROSENNATIVECONTEXT_HPP