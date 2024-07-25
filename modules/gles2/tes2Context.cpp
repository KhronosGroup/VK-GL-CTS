/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 2.0 Module
 * -------------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief OpenGL ES 2.0 test context.
 *//*--------------------------------------------------------------------*/

#include "tes2Context.hpp"
#include "gluContextFactory.hpp"
#include "gluRenderContext.hpp"
#include "gluRenderConfig.hpp"
#include "gluFboRenderContext.hpp"
#include "gluContextInfo.hpp"
#include "tcuCommandLine.hpp"
#include "glwWrapper.hpp"

#include "gluDefs.hpp"

namespace deqp
{
namespace gles2
{

Context::Context(tcu::TestContext &testCtx) : m_testCtx(testCtx), m_renderCtx(DE_NULL), m_contextInfo(DE_NULL)
{
    try
    {
        m_renderCtx   = glu::createDefaultRenderContext(m_testCtx.getPlatform(), m_testCtx.getCommandLine(),
                                                        glu::ApiType::es(2, 0));
        m_contextInfo = glu::ContextInfo::create(*m_renderCtx);

        // Set up function table for transparent wrapper.
        glw::setCurrentThreadFunctions(&m_renderCtx->getFunctions());
    }
    catch (...)
    {
        glw::setCurrentThreadFunctions(DE_NULL);

        delete m_contextInfo;
        delete m_renderCtx;

        throw;
    }
}

Context::~Context(void)
{
    // Remove functions from wrapper.
    glw::setCurrentThreadFunctions(DE_NULL);

    delete m_contextInfo;
    delete m_renderCtx;
}

const tcu::RenderTarget &Context::getRenderTarget(void) const
{
    return m_renderCtx->getRenderTarget();
}

} // namespace gles2
} // namespace deqp
