/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
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
 * \brief
 */ /*-------------------------------------------------------------------*/

#include "es32cContextFlagsTests.hpp"
#include "gluRenderContext.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "tcuTestLog.hpp"

namespace es32cts
{

class ContextFlagsCase : public deqp::TestCase
{
private:
	glu::RenderContext* m_caseContext;
	glu::ContextFlags   m_passedFlags;
	glw::GLint			m_expectedResult;

public:
	ContextFlagsCase(deqp::Context& context, glu::ContextFlags passedFlags, glw::GLint expectedResult, const char* name,
					 const char* description)
		: deqp::TestCase(context, name, description)
		, m_caseContext(NULL)
		, m_passedFlags(passedFlags)
		, m_expectedResult(expectedResult)
	{
	}

	void createContextWithFlags(glu::ContextFlags ctxFlags);
	void releaseContext(void);

	virtual void		  deinit(void);
	virtual IterateResult iterate(void);
};

void ContextFlagsCase::createContextWithFlags(glu::ContextFlags ctxFlags)
{
	glu::RenderConfig renderCfg(glu::ContextType(m_context.getRenderContext().getType().getAPI(), ctxFlags));

	glu::parseRenderConfig(&renderCfg, m_context.getTestContext().getCommandLine());

	renderCfg.surfaceType = glu::RenderConfig::SURFACETYPE_OFFSCREEN_GENERIC;

	m_caseContext = glu::createRenderContext(m_testCtx.getPlatform(), m_testCtx.getCommandLine(), renderCfg);
	m_caseContext->makeCurrent();
}

void ContextFlagsCase::releaseContext(void)
{
	if (m_caseContext)
	{
		delete m_caseContext;
		m_caseContext = NULL;
		m_context.getRenderContext().makeCurrent();
	}
}

void ContextFlagsCase::deinit(void)
{
	releaseContext();
}

tcu::TestNode::IterateResult ContextFlagsCase::iterate(void)
{
	glw::GLint flags = 0;

	createContextWithFlags(m_passedFlags);

	const glw::Functions& gl = m_caseContext->getFunctions();
	gl.getIntegerv(GL_CONTEXT_FLAGS, &flags);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glGetIntegerv");

	if (flags != m_expectedResult)
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "Test failed! glGet returned wrong  value " << flags
						   << ", expected " << m_expectedResult << "]." << tcu::TestLog::EndMessage;

		m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
	}
	else
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

	releaseContext();
	return STOP;
}

ContextFlagsTests::ContextFlagsTests(deqp::Context& context)
	: TestCaseGroup(context, "context_flags", "Verifies if context flags query results are as expected.")
{
}

void ContextFlagsTests::init()
{
	deqp::TestCaseGroup::init();

	try
	{
		addChild(new ContextFlagsCase(m_context, glu::ContextFlags(0), 0, "noFlagsSetCase", "Verifies no flags case."));
		addChild(new ContextFlagsCase(m_context, glu::CONTEXT_DEBUG, GL_CONTEXT_FLAG_DEBUG_BIT, "debugFlagSetCase",
									  "Verifies debug flag case.."));
		addChild(new ContextFlagsCase(m_context, glu::CONTEXT_ROBUST, GL_CONTEXT_FLAG_ROBUST_ACCESS_BIT,
									  "robustFlagSetCase", "Verifies robust access flag case."));

		addChild(new ContextFlagsCase(m_context, glu::CONTEXT_DEBUG | glu::CONTEXT_ROBUST,
									  GL_CONTEXT_FLAG_DEBUG_BIT | GL_CONTEXT_FLAG_ROBUST_ACCESS_BIT, "allFlagsSetCase",
									  "Verifies both debug and robust access flags case."));
	}
	catch (...)
	{
		// Destroy context.
		deqp::TestCaseGroup::deinit();
		throw;
	}
}

} // es32cts namespace
