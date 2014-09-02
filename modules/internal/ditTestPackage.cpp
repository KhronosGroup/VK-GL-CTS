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
 * \brief drawElements Internal Test Package
 *//*--------------------------------------------------------------------*/

#include "ditTestPackage.hpp"
#include "ditBuildInfoTests.hpp"
#include "ditDelibsTests.hpp"
#include "ditFrameworkTests.hpp"
#include "ditImageIOTests.hpp"
#include "ditImageCompareTests.hpp"
#include "ditTestLogTests.hpp"

namespace dit
{

class DeqpTests : public tcu::TestCaseGroup
{
public:
	DeqpTests (tcu::TestContext& testCtx)
		: tcu::TestCaseGroup(testCtx, "deqp", "dEQP Test Framework Self-tests")
	{
	}

	void init (void)
	{
		addChild(new TestLogTests		(m_testCtx));
		addChild(new ImageIOTests		(m_testCtx));
		addChild(new ImageCompareTests	(m_testCtx));
	}
};

TestPackage::TestPackage (tcu::TestContext& testCtx)
	: tcu::TestPackage	(testCtx, "dE-IT", "drawElements Internal Tests")
	, m_wrapper			(testCtx)
	, m_archive			(testCtx.getRootArchive(), "internal/")
{
}

TestPackage::~TestPackage (void)
{
}

void TestPackage::init (void)
{
	addChild(new BuildInfoTests	(m_testCtx));
	addChild(new DelibsTests	(m_testCtx));
	addChild(new FrameworkTests	(m_testCtx));
	addChild(new DeqpTests		(m_testCtx));
}

void TestPackage::deinit (void)
{
	TestNode::deinit();
}

} // dit
