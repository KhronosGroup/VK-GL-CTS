/*-------------------------------------------------------------------------
 * drawElements Internal Test Module
 * ---------------------------------
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
 * \brief Miscellaneous framework tests.
 *//*--------------------------------------------------------------------*/

#include "ditFrameworkTests.hpp"

#include "tcuFloatFormat.hpp"

namespace dit
{

class CommonFrameworkTests : public tcu::TestCaseGroup
{
public:
	CommonFrameworkTests (tcu::TestContext& testCtx)
		: tcu::TestCaseGroup(testCtx, "common", "Tests for the common utility framework")
	{
	}

	void init (void)
	{
		addChild(new SelfCheckCase(m_testCtx, "float_format","tcu::FloatFormat_selfTest()",
								   tcu::FloatFormat_selfTest));
	}
};

FrameworkTests::FrameworkTests (tcu::TestContext& testCtx)
	: tcu::TestCaseGroup(testCtx, "framework", "Miscellaneous framework tests")
{
}

FrameworkTests::~FrameworkTests (void)
{
}

void FrameworkTests::init (void)
{
	addChild(new CommonFrameworkTests(m_testCtx));
}

}
