/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
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
 * \brief Test case wrapper for test execution.
 *//*--------------------------------------------------------------------*/

#include "tcuTestCaseWrapper.hpp"
#include "tcuTestLog.hpp"
#include "deClock.h"

namespace tcu
{

TestCaseWrapper::TestCaseWrapper (TestContext& testCtx)
	: m_testCtx			(testCtx)
	, m_testStartTime	(0)
{
}

TestCaseWrapper::~TestCaseWrapper (void)
{
}

bool TestCaseWrapper::initTestCase (TestCase* testCase)
{
	// Initialize test case.
	TestLog&	log		= m_testCtx.getLog();
	bool		success	= false;

	// Record test start time.
	m_testStartTime = deGetMicroseconds();

	try
	{
		testCase->init();
		success = true;
	}
	catch (const std::bad_alloc&)
	{
		DE_ASSERT(!success);
		m_testCtx.setTestResult(QP_TEST_RESULT_RESOURCE_ERROR, "Failed to allocate memory in test case init");
		m_testCtx.setTerminateAfter(true);
	}
	catch (const tcu::TestException& e)
	{
		DE_ASSERT(!success);
		m_testCtx.setTestResult(e.getTestResult(), e.getMessage());
		m_testCtx.setTerminateAfter(e.isFatal());
		log << e;
	}
	catch (const tcu::Exception& e)
	{
		DE_ASSERT(!success);
		m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, e.getMessage());
		log << e;
	}

	DE_ASSERT(success || m_testCtx.getTestResult() != QP_TEST_RESULT_LAST);

	return success;
}

bool TestCaseWrapper::deinitTestCase (TestCase* testCase)
{
	bool deinitOk = false;

	// De-init case.
	try
	{
		testCase->deinit();
		deinitOk = true;
	}
	catch (const tcu::Exception& e)
	{
		m_testCtx.getLog() << e
						   << TestLog::Message << "Error in test case deinit, test program will terminate." << TestLog::EndMessage;
	}

	{
		const deInt64 duration = deGetMicroseconds()-m_testStartTime;
		m_testStartTime = 0;
		m_testCtx.getLog() << TestLog::Integer("TestDuration", "Test case duration in microseconds", "us", QP_KEY_TAG_TIME, duration);
	}

	return deinitOk;
}

TestNode::IterateResult TestCaseWrapper::iterateTestCase (TestCase* testCase)
{
	// Iterate the sub-case.
	TestLog&				log				= m_testCtx.getLog();
	TestCase::IterateResult	iterateResult	= TestCase::STOP;

	try
	{
		iterateResult = testCase->iterate();
	}
	catch (const std::bad_alloc&)
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_RESOURCE_ERROR, "Failed to allocate memory during test execution");
		m_testCtx.setTerminateAfter(true);
	}
	catch (const tcu::TestException& e)
	{
		log << e;
		m_testCtx.setTestResult(e.getTestResult(), e.getMessage());
		m_testCtx.setTerminateAfter(e.isFatal());
	}
	catch (const tcu::Exception& e)
	{
		log << e;
		m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, e.getMessage());
	}

	return iterateResult;
}

} // tcu
