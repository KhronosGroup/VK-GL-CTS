/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 3.0 Module
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
 * \brief OpenGL ES 3.0 Test Case Wrapper.
 *//*--------------------------------------------------------------------*/

#include "tes3TestCaseWrapper.hpp"
#include "gluStateReset.hpp"
#include "tcuTestLog.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"

namespace deqp
{
namespace gles3
{

using tcu::TestLog;

TestCaseWrapper::TestCaseWrapper (tcu::TestContext& testCtx, glu::RenderContext& renderCtx)
	: tcu::TestCaseWrapper	(testCtx)
	, m_renderCtx			(renderCtx)
{
	TCU_CHECK(contextSupports(renderCtx.getType(), glu::ApiType::es(3,0)));
}

TestCaseWrapper::~TestCaseWrapper (void)
{
}

bool TestCaseWrapper::initTestCase (tcu::TestCase* testCase)
{
	return tcu::TestCaseWrapper::initTestCase(testCase);
}

bool TestCaseWrapper::deinitTestCase (tcu::TestCase* testCase)
{
	TestLog& log = m_testCtx.getLog();

	if (!tcu::TestCaseWrapper::deinitTestCase(testCase))
		return false;

	try
	{
		// Reset state
		glu::resetState(m_renderCtx);
	}
	catch (const std::exception& e)
	{
		log << e;
		log << TestLog::Message << "Error in state reset, test program will terminate." << TestLog::EndMessage;
		return false;
	}

	return true;
}

tcu::TestNode::IterateResult TestCaseWrapper::iterateTestCase (tcu::TestCase* testCase)
{
	tcu::TestCase::IterateResult result = tcu::TestNode::STOP;

	// Clear to surrender-blue
	{
		const glw::Functions& gl = m_renderCtx.getFunctions();
		gl.clearColor(0.125f, 0.25f, 0.5f, 1.f);
		gl.clear(GL_COLOR_BUFFER_BIT);
	}

	result = tcu::TestCaseWrapper::iterateTestCase(testCase);

	// Call implementation specific post-iterate routine (usually handles native events and swaps buffers)
	try
	{
		m_renderCtx.postIterate();
		return result;
	}
	catch (const tcu::ResourceError& e)
	{
		m_testCtx.getLog() << e;
		m_testCtx.setTestResult(QP_TEST_RESULT_RESOURCE_ERROR, "Resource error in context post-iteration routine");
		m_testCtx.setTerminateAfter(true);
		return tcu::TestNode::STOP;
	}
	catch (const std::exception& e)
	{
		m_testCtx.getLog() << e;
		m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Error in context post-iteration routine");
		return tcu::TestNode::STOP;
	}
}

} // gles3
} // deqp
