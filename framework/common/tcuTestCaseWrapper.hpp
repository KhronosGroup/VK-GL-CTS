#ifndef _TCUTESTCASEWRAPPER_HPP
#define _TCUTESTCASEWRAPPER_HPP
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

#include "tcuDefs.hpp"
#include "tcuTestContext.hpp"
#include "tcuTestCase.hpp"

namespace tcu
{

class TestCaseWrapper
{
public:
										TestCaseWrapper			(TestContext& testCtx);
	virtual								~TestCaseWrapper		(void);

	virtual bool						initTestCase			(TestCase* testCase);
	virtual bool						deinitTestCase			(TestCase* testCase);

	virtual TestNode::IterateResult		iterateTestCase			(TestCase* testCase);

protected:
	TestContext&						m_testCtx;

	deUint64							m_testStartTime;		//!< For logging test case durations.
};

} // tcu

#endif // _TCUTESTCASEWRAPPER_HPP
