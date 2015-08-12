/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Google Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be
 * included in all copies or substantial portions of the Materials.
 *
 * The Materials are Confidential Information as defined by the
 * Khronos Membership Agreement until designated non-confidential by
 * Khronos, at which point this condition clause shall be removed.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 *
 *//*!
 * \file
 * \brief Vulkan Test Package
 *//*--------------------------------------------------------------------*/

#include "vktTestPackage.hpp"

#include "tcuPlatform.hpp"
#include "tcuTestCase.hpp"
#include "tcuTestLog.hpp"

#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkBinaryRegistry.hpp"
#include "vkGlslToSpirV.hpp"

#include "deUniquePtr.hpp"

#include "vktInfo.hpp"
#include "vktApiTests.hpp"
#include "vktDeviceInitializationTests.hpp"

#include <vector>
#include <sstream>

namespace vkt
{

using std::vector;
using de::UniquePtr;
using de::MovePtr;
using tcu::TestLog;

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
	tcu::TestLog&			log			= m_context.getTestContext().getLog();
	vk::SourceCollection	sourceProgs;

	DE_UNREF(casePath); // \todo [2015-03-13 pyry] Use this to identify ProgramCollection storage path

	if (!vktCase)
		TCU_THROW(InternalError, "Test node not an instance of vkt::TestCase");

	m_progCollection.clear();
	vktCase->initPrograms(sourceProgs);

	for (vk::SourceCollection::Iterator progIter = sourceProgs.begin(); progIter != sourceProgs.end(); ++progIter)
	{
		const vk::ProgramIdentifier		progId		(casePath, progIter.getName());
		const tcu::ScopedLogSection		progSection	(log, progIter.getName(), "Program: " + progIter.getName());
		de::MovePtr<vk::ProgramBinary>	binProg;
		glu::ShaderProgramInfo			buildInfo;

		// \todo [2015-07-01 pyry] Command line parameter to control cache vs. build order?

		try
		{
			binProg	= de::MovePtr<vk::ProgramBinary>(vk::buildProgram(progIter.getProgram(), vk::PROGRAM_FORMAT_SPIRV, &buildInfo));
			log << buildInfo;
		}
		catch (const tcu::NotSupportedError& err)
		{
			// Try to load from cache
			const vk::BinaryRegistryReader	registry	(m_context.getTestContext().getArchive(), "vulkan/prebuilt");

			log << err << TestLog::Message << "Building from source not supported, loading stored binary instead" << TestLog::EndMessage;

			binProg = de::MovePtr<vk::ProgramBinary>(registry.loadProgram(progId));

			log << progIter.getProgram();
		}
		catch (const tcu::Exception&)
		{
			// Build failed for other reason
			log << buildInfo;
			throw;
		}

		TCU_CHECK_INTERNAL(binProg);

		try
		{
			std::ostringstream disasm;

			vk::disassembleSpirV(binProg->getSize(), binProg->getBinary(), &disasm);

			log << TestLog::KernelSource(disasm.str());
		}
		catch (const tcu::NotSupportedError& err)
		{
			log << err;
		}

		m_progCollection.add(progId.programName, binProg);
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
	addChild(deviceInit::createTests (m_testCtx));
}

} // vkt
