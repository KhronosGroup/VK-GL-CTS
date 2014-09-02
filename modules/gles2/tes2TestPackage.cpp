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
 * \brief OpenGL ES 2.0 Test Package
 *//*--------------------------------------------------------------------*/

#include "tes2TestPackage.hpp"
#include "es2fFunctionalTests.hpp"
#include "es2pPerformanceTests.hpp"
#include "tes2InfoTests.hpp"
#include "tes2CapabilityTests.hpp"
#include "es2aAccuracyTests.hpp"
#include "es2sStressTests.hpp"

namespace deqp
{
namespace gles2
{

PackageContext::PackageContext (tcu::TestContext& testCtx)
	: m_context		(DE_NULL)
	, m_caseWrapper	(DE_NULL)
{
	try
	{
		m_context		= new Context(testCtx);
		m_caseWrapper	= new TestCaseWrapper(testCtx, m_context->getRenderContext());
	}
	catch (...)
	{
		delete m_caseWrapper;
		delete m_context;

		throw;
	}
}

PackageContext::~PackageContext (void)
{
	delete m_caseWrapper;
	delete m_context;
}

TestPackage::TestPackage (tcu::TestContext& testCtx)
	: tcu::TestPackage	(testCtx, "dEQP-GLES2", "dEQP OpenGL ES 2.0 Tests")
	, m_packageCtx		(DE_NULL)
	, m_archive			(testCtx.getRootArchive(), "gles2/")
{
}

TestPackage::~TestPackage (void)
{
	// Destroy children first since destructors may access context.
	TestNode::deinit();
	delete m_packageCtx;
}

void TestPackage::init (void)
{
	try
	{
		// Create context
		m_packageCtx = new PackageContext(m_testCtx);

		// Add main test groups
		addChild(new InfoTests						(m_packageCtx->getContext()));
		addChild(new CapabilityTests				(m_packageCtx->getContext()));
		addChild(new Functional::FunctionalTests	(m_packageCtx->getContext()));
		addChild(new Accuracy::AccuracyTests		(m_packageCtx->getContext()));
		addChild(new Performance::PerformanceTests	(m_packageCtx->getContext()));
		addChild(new Stress::StressTests			(m_packageCtx->getContext()));
	}
	catch (...)
	{
		delete m_packageCtx;
		m_packageCtx = DE_NULL;

		throw;
	}
}

void TestPackage::deinit (void)
{
	TestNode::deinit();
	delete m_packageCtx;
	m_packageCtx = DE_NULL;
}

} // gles2
} // deqp
