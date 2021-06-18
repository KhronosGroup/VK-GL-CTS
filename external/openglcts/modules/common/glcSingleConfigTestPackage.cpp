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

#include "glcSubgroupsTests.hpp"
#include "gl4cEnhancedLayoutsTests.hpp"
#include "../gles31/es31cArrayOfArraysTests.hpp"

namespace glcts
{

class TestCaseWrapper : public tcu::TestCaseExecutor
{
public:
	TestCaseWrapper(deqp::TestPackage& package, de::SharedPtr<tcu::WaiverUtil> waiverMechanism);
	~TestCaseWrapper(void);

	void init(tcu::TestCase* testCase, const std::string& path);
	void deinit(tcu::TestCase* testCase);
	tcu::TestNode::IterateResult iterate(tcu::TestCase* testCase);

private:
	deqp::TestPackage& m_testPackage;
	de::SharedPtr<tcu::WaiverUtil> m_waiverMechanism;
};

TestCaseWrapper::TestCaseWrapper(deqp::TestPackage& package, de::SharedPtr<tcu::WaiverUtil> waiverMechanism)
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

SingleConfigGL43TestPackage::SingleConfigGL43TestPackage(tcu::TestContext& testCtx, const char* packageName, const char* description,
												 glu::ContextType renderContextType)
	: deqp::TestPackage(testCtx, packageName, description, renderContextType, "gl_cts/data/")
{
}

SingleConfigGL43TestPackage::~SingleConfigGL43TestPackage(void)
{

}

void SingleConfigGL43TestPackage::init(void)
{
	// Call init() in parent - this creates context.
	deqp::TestPackage::init();

	try
	{
		// Add main test groups
		addChild(new glcts::ArrayOfArraysTestGroupGL(getContext()));
	}
	catch (...)
	{
		// Destroy context.
		deqp::TestPackage::deinit();
		throw;
	}
}

tcu::TestCaseExecutor* SingleConfigGL43TestPackage::createExecutor(void) const
{
	return new TestCaseWrapper(const_cast<SingleConfigGL43TestPackage&>(*this), m_waiverMechanism);
}

SingleConfigGL44TestPackage::SingleConfigGL44TestPackage(tcu::TestContext& testCtx, const char* packageName, const char* description,
												 glu::ContextType renderContextType)
	: SingleConfigGL43TestPackage(testCtx, packageName, description, renderContextType)
{
}

SingleConfigGL44TestPackage::~SingleConfigGL44TestPackage(void)
{

}

void SingleConfigGL44TestPackage::init(void)
{
	// Call init() in parent - this creates context.
	SingleConfigGL43TestPackage::init();

	try
	{
		// Add main test groups
		addChild(new gl4cts::EnhancedLayoutsTests(getContext()));
	}
	catch (...)
	{
		// Destroy context.
		deqp::TestPackage::deinit();
		throw;
	}
}

SingleConfigGL45TestPackage::SingleConfigGL45TestPackage(tcu::TestContext& testCtx, const char* packageName, const char* description,
												 glu::ContextType renderContextType)
	: SingleConfigGL44TestPackage(testCtx, packageName, description, renderContextType)
{
}

SingleConfigGL45TestPackage::~SingleConfigGL45TestPackage(void)
{

}

void SingleConfigGL45TestPackage::init(void)
{
	// Call init() in parent - this creates context.
	SingleConfigGL44TestPackage::init();

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

SingleConfigGL46TestPackage::SingleConfigGL46TestPackage(tcu::TestContext& testCtx, const char* packageName, const char* description,
												 glu::ContextType renderContextType)
	: SingleConfigGL45TestPackage(testCtx, packageName, description, renderContextType)
{
}

SingleConfigGL46TestPackage::~SingleConfigGL46TestPackage(void)
{

}

void SingleConfigGL46TestPackage::init(void)
{
	// Call init() in parent - this creates context.
	SingleConfigGL45TestPackage::init();

	try
	{
		// Add main test groups
	}
	catch (...)
	{
		// Destroy context.
		deqp::TestPackage::deinit();
		throw;
	}
}

SingleConfigES32TestPackage::SingleConfigES32TestPackage(tcu::TestContext& testCtx, const char* packageName, const char* description,
												 glu::ContextType renderContextType)
	: deqp::TestPackage(testCtx, packageName, description, renderContextType, "gl_cts/data/")
{
}

SingleConfigES32TestPackage::~SingleConfigES32TestPackage(void)
{

}

void SingleConfigES32TestPackage::init(void)
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

tcu::TestCaseExecutor* SingleConfigES32TestPackage::createExecutor(void) const
{
	return new TestCaseWrapper(const_cast<SingleConfigES32TestPackage&>(*this), m_waiverMechanism);
}

} // glcts
