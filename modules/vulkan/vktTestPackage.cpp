/*-------------------------------------------------------------------------
 * drawElements Quality Program Vulkan Module
 * --------------------------------------------
 *
 * Copyright 2015 The Android Open Source Project
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
 * \brief Vulkan Test Package
 *//*--------------------------------------------------------------------*/

#include "vktTestPackage.hpp"

#include "tcuPlatform.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "deUniquePtr.hpp"

#include "vktInfo.hpp"
#include "vktApiTests.hpp"

#include <vector>

namespace vkt
{

using std::vector;
using de::UniquePtr;
using de::MovePtr;

// TestCaseExecutor

class TestCaseExecutor : public tcu::TestCaseExecutor
{
public:
											TestCaseExecutor	(tcu::TestContext& testCtx);
											~TestCaseExecutor	(void);

	virtual void							init				(tcu::TestCase* testCase, const std::string& path);
	virtual void							deinit				(tcu::TestCase* testCase);

	virtual tcu::TestNode::IterateResult	iterate				(tcu::TestCase* testCase);

private:
	vk::BinaryCollection					m_progCollection;
	de::UniquePtr<vk::Library>				m_library;
	Context									m_context;

	TestInstance*							m_instance;			//!< Current test case instance
};

static MovePtr<vk::Library> createLibrary (tcu::TestContext& testCtx)
{
	return MovePtr<vk::Library>(testCtx.getPlatform().getVulkanPlatform().createLibrary());
}

TestCaseExecutor::TestCaseExecutor (tcu::TestContext& testCtx)
	: m_library		(createLibrary(testCtx))
	, m_context		(testCtx, m_library->getPlatformInterface(), m_progCollection)
	, m_instance	(DE_NULL)
{
}

TestCaseExecutor::~TestCaseExecutor (void)
{
	delete m_instance;
}

void TestCaseExecutor::init (tcu::TestCase* testCase, const std::string& casePath)
{
	const TestCase*			vktCase		= dynamic_cast<TestCase*>(testCase);
	vk::SourceCollection	sourceProgs;

	DE_UNREF(casePath); // \todo [2015-03-13 pyry] Use this to identify ProgramCollection storage path

	if (!vktCase)
		TCU_THROW(InternalError, "Test node not an instance of vkt::TestCase");

	m_progCollection.clear();
	vktCase->initPrograms(sourceProgs);

	// \todo [2015-03-13 pyry] Need abstraction for this - sometimes built on run-time, sometimes loaded from archive
	for (vk::SourceCollection::Iterator progIter = sourceProgs.begin(); progIter != sourceProgs.end(); ++progIter)
	{
		const std::string&				name		= progIter.getName();
		const glu::ProgramSources&		srcProg		= progIter.getProgram();
		de::MovePtr<vk::ProgramBinary>	binProg		= de::MovePtr<vk::ProgramBinary>(vk::buildProgram(srcProg, vk::PROGRAM_FORMAT_GLSL));

		m_progCollection.add(name, binProg);
	}

	DE_ASSERT(!m_instance);
	m_instance = vktCase->createInstance(m_context);
}

void TestCaseExecutor::deinit (tcu::TestCase*)
{
	delete m_instance;
	m_instance = DE_NULL;
}

tcu::TestNode::IterateResult TestCaseExecutor::iterate (tcu::TestCase*)
{
	DE_ASSERT(m_instance);

	const tcu::TestStatus	result	= m_instance->iterate();

	if (result.isComplete())
	{
		// Vulkan tests shouldn't set result directly
		DE_ASSERT(m_context.getTestContext().getTestResult() == QP_TEST_RESULT_LAST);
		m_context.getTestContext().setTestResult(result.getCode(), result.getDescription().c_str());
		return tcu::TestNode::STOP;
	}
	else
		return tcu::TestNode::CONTINUE;
}

// TestPackage

TestPackage::TestPackage (tcu::TestContext& testCtx)
	: tcu::TestPackage(testCtx, "dEQP-VK", "dEQP Vulkan Tests")
{
}

TestPackage::~TestPackage (void)
{
}

tcu::TestCaseExecutor* TestPackage::createExecutor (void) const
{
	return new TestCaseExecutor(m_testCtx);
}

void TestPackage::init (void)
{
	addChild(createInfoTests	(m_testCtx));
	addChild(api::createTests	(m_testCtx));
}

} // vkt
