/*-------------------------------------------------------------------------
 * drawElements Quality Program EGL Module
 * ---------------------------------------
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
 * \brief EGL Test Package
 *//*--------------------------------------------------------------------*/

#include "teglTestPackage.hpp"

#include "tcuTestLog.hpp"
#include "tcuPlatform.hpp"
#include "tcuCommandLine.hpp"

#include "egluPlatform.hpp"
#include "egluNativeDisplay.hpp"
#include "egluNativeWindow.hpp"
#include "egluNativePixmap.hpp"

#include "teglInfoTests.hpp"
#include "teglCreateContextTests.hpp"
#include "teglQueryContextTests.hpp"
#include "teglCreateSurfaceTests.hpp"
#include "teglQuerySurfaceTests.hpp"
#include "teglChooseConfigTests.hpp"
#include "teglQueryConfigTests.hpp"
#include "teglColorClearTests.hpp"
#include "teglRenderTests.hpp"
#include "teglImageTests.hpp"
#include "teglGLES2SharingTests.hpp"
#include "teglNegativeApiTests.hpp"
#include "teglSyncTests.hpp"
#include "teglMultiThreadTests.hpp"
#include "teglGetProcAddressTests.hpp"
#include "teglMemoryStressTests.hpp"
#include "teglMakeCurrentPerfTests.hpp"
#include "teglGLES2SharedRenderingPerfTests.hpp"
#include "teglPreservingSwapTests.hpp"
#include "teglClientExtensionTests.hpp"
#include "teglCreateContextExtTests.hpp"
#include "teglSurfacelessContextTests.hpp"
#include "teglSwapBuffersTests.hpp"
#include "teglNativeColorMappingTests.hpp"
#include "teglNativeCoordMappingTests.hpp"
#include "teglResizeTests.hpp"

#include <typeinfo>

using std::vector;

namespace deqp
{
namespace egl
{

class StressTests : public TestCaseGroup
{
public:
	StressTests (EglTestContext& eglTestCtx)
		: TestCaseGroup(eglTestCtx, "stress", "EGL stress tests")
	{
	}

	void init (void)
	{
		addChild(new MemoryStressTests(m_eglTestCtx));
	}
};

class PerformanceTests : public TestCaseGroup
{
public:
	PerformanceTests (EglTestContext& eglTestCtx)
		: TestCaseGroup(eglTestCtx, "performance", "EGL performance tests")
	{
	}

	void init (void)
	{
		addChild(new MakeCurrentPerfTests			(m_eglTestCtx));
		addChild(new GLES2SharedRenderingPerfTests	(m_eglTestCtx));
	}
};

class FunctionalTests : public TestCaseGroup
{
public:
	FunctionalTests (EglTestContext& eglTestCtx)
		: TestCaseGroup(eglTestCtx, "functional", "EGL functional tests")
	{
	}

	void init (void)
	{
		addChild(new CreateContextTests			(m_eglTestCtx));
		addChild(new QueryContextTests			(m_eglTestCtx));
		addChild(new CreateSurfaceTests			(m_eglTestCtx));
		addChild(new QuerySurfaceTests			(m_eglTestCtx));
		addChild(new QueryConfigTests			(m_eglTestCtx));
		addChild(new ChooseConfigTests			(m_eglTestCtx));
		addChild(new ColorClearTests			(m_eglTestCtx));
		addChild(new RenderTests				(m_eglTestCtx));
		addChild(new ImageTests					(m_eglTestCtx));
		addChild(new SharingTests				(m_eglTestCtx));
		addChild(new NegativeApiTests			(m_eglTestCtx));
		addChild(new FenceSyncTests				(m_eglTestCtx));
		addChild(new MultiThreadedTests			(m_eglTestCtx));
		addChild(new GetProcAddressTests		(m_eglTestCtx));
		addChild(new PreservingSwapTests		(m_eglTestCtx));
		addChild(new ClientExtensionTests		(m_eglTestCtx));
		addChild(new CreateContextExtTests		(m_eglTestCtx));
		addChild(new SurfacelessContextTests	(m_eglTestCtx));
		addChild(new SwapBuffersTests			(m_eglTestCtx));
		addChild(new NativeColorMappingTests	(m_eglTestCtx));
		addChild(new NativeCoordMappingTests	(m_eglTestCtx));
		addChild(new ReusableSyncTests			(m_eglTestCtx));
		addChild(new ResizeTests				(m_eglTestCtx));
	}
};

TestCaseWrapper::TestCaseWrapper (EglTestContext& eglTestCtx)
	: tcu::TestCaseWrapper	(eglTestCtx.getTestContext())
	, m_eglTestCtx			(eglTestCtx)
{
}

TestCaseWrapper::~TestCaseWrapper (void)
{
}

bool TestCaseWrapper::initTestCase (tcu::TestCase* testCase)
{
	tcu::TestLog& log = m_eglTestCtx.getTestContext().getLog();

	// Create display
	try
	{
		m_eglTestCtx.createDefaultDisplay();
	}
	catch (const std::exception& e)
	{
		log << e;
		m_eglTestCtx.getTestContext().setTestResult(QP_TEST_RESULT_FAIL, "Failed to initialize EGL for default display");
		return false;
	}

	return tcu::TestCaseWrapper::initTestCase(testCase);
}

bool TestCaseWrapper::deinitTestCase (tcu::TestCase* testCase)
{
	tcu::TestLog& log = m_eglTestCtx.getTestContext().getLog();

	bool deinitOk = tcu::TestCaseWrapper::deinitTestCase(testCase);

	// Destroy display
	try
	{
		TCU_CHECK_EGL_CALL(eglMakeCurrent(m_eglTestCtx.getDisplay().getEGLDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
		m_eglTestCtx.destroyDefaultDisplay();
	}
	catch (const std::exception& e)
	{
		log << e;
		log << tcu::TestLog::Message << "Error in EGL deinit, test program will teminate." << tcu::TestLog::EndMessage;
		return false;
	}

	return deinitOk;
}

tcu::TestNode::IterateResult TestCaseWrapper::iterateTestCase (tcu::TestCase* testCase)
{
	return tcu::TestCaseWrapper::iterateTestCase(testCase);
}

PackageContext::PackageContext (tcu::TestContext& testCtx)
	: m_eglTestCtx	(DE_NULL)
	, m_caseWrapper	(DE_NULL)
{
	const eglu::NativeDisplayFactoryRegistry&	dpyFactoryRegistry	= testCtx.getPlatform().getEGLPlatform().getNativeDisplayFactoryRegistry();
	const char* const							displayFactoryName	= testCtx.getCommandLine().getEGLDisplayType();
	const char* const							windowFactoryName	= testCtx.getCommandLine().getEGLWindowType();
	const char* const							pixmapFactoryName	= testCtx.getCommandLine().getEGLPixmapType();

	const eglu::NativeDisplayFactory*			displayFactory		= DE_NULL;
	const eglu::NativeWindowFactory*			windowFactory		= DE_NULL;
	const eglu::NativePixmapFactory*			pixmapFactory		= DE_NULL;

	if (dpyFactoryRegistry.empty())
	{
		tcu::print("ERROR: Platform doesn't support any EGL native display types!\n");
		throw tcu::NotSupportedError("Platform doesn't have EGL any native display factories", DE_NULL, __FILE__, __LINE__);
	}

	if (!displayFactoryName)
		displayFactory = dpyFactoryRegistry.getDefaultFactory();
	else
	{
		displayFactory = dpyFactoryRegistry.getFactoryByName(displayFactoryName);

		if (!displayFactory)
		{
			tcu::print("ERROR: Unknown/unsupported EGL native display type '%s'\n", displayFactoryName);
			tcu::print("Supported EGL native display types:\n");

			for (int factoryNdx = 0; factoryNdx < (int)dpyFactoryRegistry.getFactoryCount(); factoryNdx++)
			{
				const char* name = dpyFactoryRegistry.getFactoryByIndex(factoryNdx)->getName();
				const char* desc = dpyFactoryRegistry.getFactoryByIndex(factoryNdx)->getDescription();

				tcu::print("  %s: %s\n", name, desc);
			}

			throw tcu::NotSupportedError(("Unknown EGL native display type '" + std::string(displayFactoryName) + "'.").c_str(), DE_NULL, __FILE__, __LINE__);
		}
	}

	tcu::print("Using EGL native display type '%s'\n", displayFactory->getName());

	if (!displayFactory->getNativeWindowRegistry().empty())
	{
		windowFactory = windowFactoryName ? displayFactory->getNativeWindowRegistry().getFactoryByName(windowFactoryName)
										  : displayFactory->getNativeWindowRegistry().getDefaultFactory();

		if (!windowFactory)
		{
			DE_ASSERT(windowFactoryName);
			tcu::print("ERROR: Unknown/unsupported EGL native window type '%s'\n", windowFactoryName);
			tcu::print("Supported EGL native window types for native display '%s':\n", displayFactory->getName());

			for (int factoryNdx = 0; factoryNdx < (int)displayFactory->getNativeWindowRegistry().getFactoryCount(); factoryNdx++)
			{
				const char* name = displayFactory->getNativeWindowRegistry().getFactoryByIndex(factoryNdx)->getName();
				const char* desc = displayFactory->getNativeWindowRegistry().getFactoryByIndex(factoryNdx)->getDescription();

				tcu::print("  %s: %s\n", name, desc);
			}

			throw tcu::NotSupportedError(("Unknown EGL native window type '" + std::string(windowFactoryName) + "'").c_str(), DE_NULL, __FILE__, __LINE__);
		}
	}
	else
		tcu::print("Warning: EGL native display doesn't have any native window types.\n");

	if (!displayFactory->getNativePixmapRegistry().empty())
	{
		pixmapFactory = pixmapFactoryName ? displayFactory->getNativePixmapRegistry().getFactoryByName(pixmapFactoryName)
										  : displayFactory->getNativePixmapRegistry().getDefaultFactory();

		if (!pixmapFactory)
		{
			DE_ASSERT(pixmapFactoryName);
			tcu::print("ERROR: Unknown/unsupported EGL native pixmap type '%s'\n", pixmapFactoryName);
			tcu::print("Supported EGL native pixmap types for native display '%s':\n", displayFactory->getName());

			for (int factoryNdx = 0; factoryNdx < (int)displayFactory->getNativePixmapRegistry().getFactoryCount(); factoryNdx++)
			{
				const char* name = displayFactory->getNativePixmapRegistry().getFactoryByIndex(factoryNdx)->getName();
				const char* desc = displayFactory->getNativePixmapRegistry().getFactoryByIndex(factoryNdx)->getDescription();

				tcu::print("  %s: %s\n", name, desc);
			}

			throw tcu::NotSupportedError(("Unknown EGL native pixmap type '" + std::string(pixmapFactoryName) + "'").c_str(), DE_NULL, __FILE__, __LINE__);
		}
	}
	else
		tcu::print("Warning: EGL native display doesn't have any native pixmap types.\n");

	if (windowFactory)
		tcu::print("Using EGL native window type '%s'\n", windowFactory->getName());
	if (pixmapFactory)
		tcu::print("Using EGL native pixmap type '%s'\n", pixmapFactory->getName());

	try
	{
		m_eglTestCtx	= new EglTestContext(testCtx, *displayFactory, windowFactory, pixmapFactory);
		m_caseWrapper	= new TestCaseWrapper(*m_eglTestCtx);
	}
	catch (...)
	{
		delete m_caseWrapper;
		delete m_eglTestCtx;

		throw;
	}
}

PackageContext::~PackageContext (void)
{
	delete m_caseWrapper;
	delete m_eglTestCtx;
}

TestPackage::TestPackage (tcu::TestContext& testCtx)
	: tcu::TestPackage	(testCtx, "dEQP-EGL", "dEQP EGL Tests")
	, m_packageCtx		(DE_NULL)
	, m_archive			(testCtx.getRootArchive(), "egl/")
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
	DE_ASSERT(!m_packageCtx);
	m_packageCtx = new PackageContext(m_testCtx);

	try
	{
		addChild(new InfoTests				(m_packageCtx->getEglTestContext()));
		addChild(new FunctionalTests		(m_packageCtx->getEglTestContext()));
		addChild(new PerformanceTests		(m_packageCtx->getEglTestContext()));
		addChild(new StressTests			(m_packageCtx->getEglTestContext()));
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
	tcu::TestNode::deinit();
	delete m_packageCtx;
	m_packageCtx = DE_NULL;
}

} // egl
} // deqp
