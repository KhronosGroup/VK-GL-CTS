/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2016 Google Inc.
 * Copyright (c) 2016-2019 The Khronos Group Inc.
 * Copyright (c) 2019 NVIDIA Corporation.
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
 * \brief OpenGL/OpenGL ES Test Package that only gets run in a single config
 */ /*-------------------------------------------------------------------*/

#include "glcSingleConfigTestPackage.hpp"
#include "gluStateReset.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "tcuTestLog.hpp"
#include "tcuWaiverUtil.hpp"

#include "subgroups/glcSubgroupsTests.hpp"

namespace glcts
{

class TestCaseWrapper : public tcu::TestCaseExecutor
{
public:
	TestCaseWrapper(SingleConfigTestPackage& package, de::SharedPtr<tcu::WaiverUtil> waiverMechanism);
	~TestCaseWrapper(void);

	void init(tcu::TestCase* testCase, const std::string& path);
	void deinit(tcu::TestCase* testCase);
	tcu::TestNode::IterateResult iterate(tcu::TestCase* testCase);

private:
	SingleConfigTestPackage& m_testPackage;
	de::SharedPtr<tcu::WaiverUtil> m_waiverMechanism;
};

TestCaseWrapper::TestCaseWrapper(SingleConfigTestPackage& package, de::SharedPtr<tcu::WaiverUtil> waiverMechanism)
	: m_testPackage(package)
	, m_waiverMechanism(waiverMechanism)
{
}

TestCaseWrapper::~TestCaseWrapper(void)
{
}

void TestCaseWrapper::init(tcu::TestCase* testCase, const std::string& path)
{
	if (m_waiverMechanism->isOnWaiverList(path))
		throw tcu::TestException("Waived test", QP_TEST_RESULT_WAIVER);

	testCase->init();
}

void TestCaseWrapper::deinit(tcu::TestCase* testCase)
{
	testCase->deinit();

	glu::resetState(m_testPackage.getContext().getRenderContext(), m_testPackage.getContext().getContextInfo());
}

tcu::TestNode::IterateResult TestCaseWrapper::iterate(tcu::TestCase* testCase)
{
	tcu::TestContext&			 testCtx   = m_testPackage.getContext().getTestContext();
	glu::RenderContext&			 renderCtx = m_testPackage.getContext().getRenderContext();
	tcu::TestCase::IterateResult result;

	// Clear to surrender-blue
	{
		const glw::Functions& gl = renderCtx.getFunctions();
		gl.clearColor(0.0f, 0.0f, 0.0f, 1.f);
		gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	}

	result = testCase->iterate();

	// Call implementation specific post-iterate routine (usually handles native events and swaps buffers)
	try
	{
		renderCtx.postIterate();
		return result;
	}
	catch (const tcu::ResourceError&)
	{
		testCtx.getLog().endCase(QP_TEST_RESULT_RESOURCE_ERROR, "Resource error in context post-iteration routine");
		testCtx.setTerminateAfter(true);
		return tcu::TestNode::STOP;
	}
	catch (const std::exception&)
	{
		testCtx.getLog().endCase(QP_TEST_RESULT_FAIL, "Error in context post-iteration routine");
		return tcu::TestNode::STOP;
	}
}

SingleConfigTestPackage::SingleConfigTestPackage(tcu::TestContext& testCtx, const char* packageName,
												 glu::ContextType renderContextType)
	: deqp::TestPackage(testCtx, packageName, "CTS Single Config Package",
						renderContextType, "gl_cts/data/")
{
}

SingleConfigTestPackage::~SingleConfigTestPackage(void)
{
	deqp::TestPackage::deinit();
}

void SingleConfigTestPackage::init(void)
{
	// Call init() in parent - this creates context.
	deqp::TestPackage::init();

	try
	{
		// Add main test groups
		addChild(new glc::subgroups::GlSubgroupTests(getContext()));
	}
	catch (...)
	{
		// Destroy context.
		deqp::TestPackage::deinit();
		throw;
	}
}

tcu::TestCaseExecutor* SingleConfigTestPackage::createExecutor(void) const
{
	return new TestCaseWrapper(const_cast<SingleConfigTestPackage&>(*this), m_waiverMechanism);
}

} // glcts
