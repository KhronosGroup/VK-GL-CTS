/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2016 Google Inc.
 * Copyright (c) 2016 The Khronos Group Inc.
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
 */ /*!
 * \file
 * \brief OpenGL Conformance Test Package Base Class
 */ /*-------------------------------------------------------------------*/

#include "glcTestPackage.hpp"
#include "gluContextInfo.hpp"
#include "tcuTestLog.hpp"
#include "tcuCommandLine.hpp"
#include "tcuWaiverUtil.hpp"
#include "glwEnums.hpp"

namespace deqp
{

PackageContext::PackageContext(tcu::TestContext &testCtx, glu::ContextType renderContextType)
    : m_context(testCtx, renderContextType)
    , m_caseWrapper(m_context)
{
}

PackageContext::~PackageContext(void)
{
}

TestPackage::TestPackage(tcu::TestContext &testCtx, const char *name, const char *description,
                         glu::ContextType renderContextType, const char *resourcesPath)
    : tcu::TestPackage(testCtx, name, description)
    , m_waiverMechanism(new tcu::WaiverUtil)
    , m_renderContextType(renderContextType)
    , m_packageCtx(DE_NULL)
    , m_archive(testCtx.getRootArchive(), resourcesPath)
{
}

TestPackage::~TestPackage(void)
{
    // Destroy all children before destroying context since destructors may access context.
    tcu::TestNode::deinit();
    delete m_packageCtx;
}

void TestPackage::init(void)
{
    try
    {
        // Create context
        m_packageCtx = new PackageContext(m_testCtx, m_renderContextType);

        // Setup waiver mechanism
        if (m_testCtx.getCommandLine().getRunMode() == tcu::RUNMODE_EXECUTE)
        {
            Context &context                    = m_packageCtx->getContext();
            const glu::ContextInfo &contextInfo = context.getContextInfo();
            std::string vendor                  = contextInfo.getString(GL_VENDOR);
            std::string renderer                = contextInfo.getString(GL_RENDERER);
            const tcu::CommandLine &commandLine = context.getTestContext().getCommandLine();
            tcu::SessionInfo sessionInfo(vendor, renderer, commandLine.getInitialCmdLine());
            m_waiverMechanism->setup(commandLine.getWaiverFileName(), m_name, vendor, renderer, sessionInfo);
            context.getTestContext().getLog().writeSessionInfo(sessionInfo.get());
        }
    }
    catch (...)
    {
        delete m_packageCtx;
        m_packageCtx = DE_NULL;

        throw;
    }
}

void TestPackage::deinit(void)
{
    tcu::TestNode::deinit();
    delete m_packageCtx;
    m_packageCtx = DE_NULL;
}

} // namespace deqp
