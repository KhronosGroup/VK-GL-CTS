/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 3.1 Module
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
 * \brief OpenGL ES 3.1 Test Package
 *//*--------------------------------------------------------------------*/

#include "tes31TestPackage.hpp"
#include "tes31TestCaseWrapper.hpp"
#include "tes31InfoTests.hpp"
#include "es31fFunctionalTests.hpp"
#include "es31sStressTests.hpp"
#include "gluStateReset.hpp"
#include "gluRenderContext.hpp"
#include "gluContextInfo.hpp"
#include "tcuTestLog.hpp"
#include "tcuCommandLine.hpp"
#include "tcuWaiverUtil.hpp"
#include "glwEnums.hpp"

namespace deqp
{
namespace gles31
{

TestPackage::TestPackage (tcu::TestContext& testCtx)
	: tcu::TestPackage	(testCtx, "dEQP-GLES31", "dEQP OpenGL ES 3.1 Tests")
	, m_archive			(testCtx.getRootArchive(), "gles31/")
	, m_context			(DE_NULL)
	, m_waiverMechanism (new tcu::WaiverUtil)
{
}

TestPackage::~TestPackage (void)
{
	// Destroy children first since destructors may access context.
	TestNode::deinit();
	delete m_context;
}

void TestPackage::init (void)
{
	try
	{
		// Create context
		// Some of the tests will test ES3.2 functionality if supported so try to create a 3.2 context
		// first. If that doesn't work then create an ES3.1 context.
		try
		{
			m_context = new Context(m_testCtx, glu::ApiType::es(3, 2));
		}
		catch (...)
		{
			m_context = new Context(m_testCtx, glu::ApiType::es(3, 1));
		}

		// Setup waiver mechanism
		if (m_testCtx.getCommandLine().getRunMode() == tcu::RUNMODE_EXECUTE)
		{
			const glu::ContextInfo&	contextInfo = m_context->getContextInfo();
			std::string				vendor		= contextInfo.getString(GL_VENDOR);
			std::string				renderer	= contextInfo.getString(GL_RENDERER);
			const tcu::CommandLine&	commandLine	= m_context->getTestContext().getCommandLine();
			tcu::SessionInfo		sessionInfo	(vendor, renderer, commandLine.getInitialCmdLine());
			m_waiverMechanism->setup(commandLine.getWaiverFileName(), m_name, vendor, renderer, sessionInfo);
			m_context->getTestContext().getLog().writeSessionInfo(sessionInfo.get());
		}

		// Add main test groups
		addChild(new InfoTests							(*m_context));
		addChild(new Functional::GLES31FunctionalTests	(*m_context));
		addChild(new Stress::StressTests				(*m_context));
	}
	catch (...)
	{
		delete m_context;
		m_context = DE_NULL;

		throw;
	}
}

void TestPackage::deinit (void)
{
	TestNode::deinit();
	delete m_context;
	m_context = DE_NULL;
}

tcu::TestCaseExecutor* TestPackage::createExecutor (void) const
{
	return new TestCaseWrapper<TestPackage>(const_cast<TestPackage&>(*this), m_waiverMechanism);
}

} // gles31
} // deqp
