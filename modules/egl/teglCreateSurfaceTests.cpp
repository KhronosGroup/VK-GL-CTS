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
 * \brief Simple surface construction test.
 *//*--------------------------------------------------------------------*/

#include "teglCreateSurfaceTests.hpp"

#include "egluNativeDisplay.hpp"
#include "egluNativeWindow.hpp"
#include "egluNativePixmap.hpp"
#include "egluUtil.hpp"

#include "teglSimpleConfigCase.hpp"
#include "tcuTestContext.hpp"
#include "tcuCommandLine.hpp"
#include "tcuTestLog.hpp"

#include "deSTLUtil.hpp"
#include "deUniquePtr.hpp"

#include <memory>

#if !defined(EGL_EXT_platform_base)
#	define EGL_EXT_platform_base 1
	typedef EGLDisplay (EGLAPIENTRYP PFNEGLGETPLATFORMDISPLAYEXTPROC) (EGLenum platform, void *native_display, const EGLint *attrib_list);
	typedef EGLSurface (EGLAPIENTRYP PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC) (EGLDisplay dpy, EGLConfig config, void *native_window, const EGLint *attrib_list);
	typedef EGLSurface (EGLAPIENTRYP PFNEGLCREATEPLATFORMPIXMAPSURFACEEXTPROC) (EGLDisplay dpy, EGLConfig config, void *native_pixmap, const EGLint *attrib_list);
#endif // EGL_EXT_platform_base

using std::vector;
using tcu::TestLog;

namespace deqp
{
namespace egl
{
namespace
{

void checkEGLPlatformSupport (const char* platformExt)
{
	std::vector<std::string> extensions = eglu::getPlatformExtensions();

	if (!de::contains(extensions.begin(), extensions.end(), platformExt))
		throw tcu::NotSupportedError((std::string("Platform extension '") + platformExt + "' not supported").c_str(), "", __FILE__, __LINE__);
}

EGLSurface createWindowSurface (EGLDisplay display, EGLConfig config, eglu::NativeDisplay& nativeDisplay, eglu::NativeWindow& window, bool useLegacyCreate)
{
	EGLSurface surface = EGL_NO_SURFACE;

	if (useLegacyCreate)
	{
		surface = eglCreateWindowSurface(display, config, window.getLegacyNative(), DE_NULL);
		TCU_CHECK_EGL_MSG("eglCreateWindowSurface() failed");
	}
	else
	{
		checkEGLPlatformSupport(nativeDisplay.getPlatformExtensionName());

		PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC createPlatformWindowSurfaceEXT = (PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC)eglGetProcAddress("eglCreatePlatformWindowSurfaceEXT");
		TCU_CHECK_EGL_MSG("eglGetProcAddress() failed");

		surface = createPlatformWindowSurfaceEXT(display, config, window.getPlatformNative(), DE_NULL);
		TCU_CHECK_EGL_MSG("eglCreatePlatformWindowSurfaceEXT() failed");
	}

	return surface;
}

EGLSurface createPixmapSurface (EGLDisplay display, EGLConfig config, eglu::NativeDisplay& nativeDisplay, eglu::NativePixmap& pixmap, bool useLegacyCreate)
{
	EGLSurface surface = EGL_NO_SURFACE;

	if (useLegacyCreate)
	{
		surface = eglCreatePixmapSurface(display, config, pixmap.getLegacyNative(), DE_NULL);
		TCU_CHECK_EGL_MSG("eglCreatePixmapSurface() failed");
	}
	else
	{
		checkEGLPlatformSupport(nativeDisplay.getPlatformExtensionName());

		PFNEGLCREATEPLATFORMPIXMAPSURFACEEXTPROC createPlatformPixmapSurfaceEXT = (PFNEGLCREATEPLATFORMPIXMAPSURFACEEXTPROC)eglGetProcAddress("eglCreatePlatformPixmapSurfaceEXT");
		TCU_CHECK_EGL_MSG("eglGetProcAddress() failed");

		surface = createPlatformPixmapSurfaceEXT(display, config, pixmap.getPlatformNative(), DE_NULL);
		TCU_CHECK_EGL_MSG("eglCreatePlatformPixmapSurfaceEXT() failed");
	}

	return surface;
}

class CreateWindowSurfaceCase : public SimpleConfigCase
{
public:
	CreateWindowSurfaceCase (EglTestContext& eglTestCtx, const char* name, const char* description, bool useLegacyCreate, const vector<EGLint>& configIds)
		: SimpleConfigCase	(eglTestCtx, name, description, configIds)
		, m_useLegacyCreate	(useLegacyCreate)
	{
	}

	void executeForConfig (tcu::egl::Display& display, EGLConfig config)
	{
		TestLog&	log		= m_testCtx.getLog();
		EGLint		id		= display.getConfigAttrib(config, EGL_CONFIG_ID);

		// \todo [2011-03-23 pyry] Iterate thru all possible combinations of EGL_RENDER_BUFFER, EGL_VG_COLORSPACE and EGL_VG_ALPHA_FORMAT

		if (m_useLegacyCreate)
		{
			if ((m_eglTestCtx.getNativeWindowFactory().getCapabilities() & eglu::NativeWindow::CAPABILITY_CREATE_SURFACE_LEGACY) == 0)
				throw tcu::NotSupportedError("Native window doesn't support legacy eglCreateWindowSurface()", "", __FILE__, __LINE__);
		}
		else
		{
			if ((m_eglTestCtx.getNativeWindowFactory().getCapabilities() & eglu::NativeWindow::CAPABILITY_CREATE_SURFACE_PLATFORM) == 0)
				throw tcu::NotSupportedError("Native window doesn't support eglCreatePlatformWindowSurfaceEXT()", "", __FILE__, __LINE__);
		}

		log << TestLog::Message << "Creating window surface with config ID " << id << TestLog::EndMessage;
		TCU_CHECK_EGL();

		{
			const int							width			= 64;
			const int							height			= 64;
			de::UniquePtr<eglu::NativeWindow>	window			(m_eglTestCtx.createNativeWindow(display.getEGLDisplay(), config, DE_NULL, width, height, eglu::parseWindowVisibility(m_testCtx.getCommandLine())));
			tcu::egl::WindowSurface				surface			(display, createWindowSurface(display.getEGLDisplay(), config, m_eglTestCtx.getNativeDisplay(), *window, m_useLegacyCreate));

			EGLint								windowWidth		= 0;
			EGLint								windowHeight	= 0;

			TCU_CHECK_EGL_CALL(eglQuerySurface(display.getEGLDisplay(), surface.getEGLSurface(), EGL_WIDTH,		&windowWidth));
			TCU_CHECK_EGL_CALL(eglQuerySurface(display.getEGLDisplay(), surface.getEGLSurface(), EGL_HEIGHT,	&windowHeight));

			if (windowWidth <= 0 || windowHeight <= 0)
			{
				log << TestLog::Message << "  Fail, invalid surface size " << windowWidth << "x" << windowHeight << TestLog::EndMessage;
				m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid surface size");
			}
			else
				log << TestLog::Message << "  Pass" << TestLog::EndMessage;
		}
	}

private:
	bool	m_useLegacyCreate;
};

class CreatePixmapSurfaceCase : public SimpleConfigCase
{
public:
	CreatePixmapSurfaceCase (EglTestContext& eglTestCtx, const char* name, const char* description, bool useLegacyCreate, const vector<EGLint>& configIds)
		: SimpleConfigCase(eglTestCtx, name, description, configIds)
		, m_useLegacyCreate	(useLegacyCreate)
	{
	}

	void executeForConfig (tcu::egl::Display& display, EGLConfig config)
	{
		TestLog&	log		= m_testCtx.getLog();
		EGLint		id		= display.getConfigAttrib(config, EGL_CONFIG_ID);

		// \todo [2011-03-23 pyry] Iterate thru all possible combinations of EGL_RENDER_BUFFER, EGL_VG_COLORSPACE and EGL_VG_ALPHA_FORMAT

		if (m_useLegacyCreate)
		{
			if ((m_eglTestCtx.getNativePixmapFactory().getCapabilities() & eglu::NativePixmap::CAPABILITY_CREATE_SURFACE_LEGACY) == 0)
				throw tcu::NotSupportedError("Native pixmap doesn't support legacy eglCreatePixmapSurface()", "", __FILE__, __LINE__);
		}
		else
		{
			if ((m_eglTestCtx.getNativePixmapFactory().getCapabilities() & eglu::NativePixmap::CAPABILITY_CREATE_SURFACE_PLATFORM) == 0)
				throw tcu::NotSupportedError("Native pixmap doesn't support eglCreatePlatformPixmapSurfaceEXT()", "", __FILE__, __LINE__);
		}

		log << TestLog::Message << "Creating pixmap surface with config ID " << id << TestLog::EndMessage;
		TCU_CHECK_EGL();

		{
			const int							width			= 64;
			const int							height			= 64;
			de::UniquePtr<eglu::NativePixmap>	pixmap			(m_eglTestCtx.createNativePixmap(display.getEGLDisplay(), config, DE_NULL, width, height));
			tcu::egl::PixmapSurface				surface			(display, createPixmapSurface(display.getEGLDisplay(), config, m_eglTestCtx.getNativeDisplay(), *pixmap, m_useLegacyCreate));
			EGLint								pixmapWidth		= 0;
			EGLint								pixmapHeight	= 0;

			TCU_CHECK_EGL_CALL(eglQuerySurface(display.getEGLDisplay(), surface.getEGLSurface(), EGL_WIDTH,		&pixmapWidth));
			TCU_CHECK_EGL_CALL(eglQuerySurface(display.getEGLDisplay(), surface.getEGLSurface(), EGL_HEIGHT,	&pixmapHeight));

			if (pixmapWidth <= 0 || pixmapHeight <= 0)
			{
				log << TestLog::Message << "  Fail, invalid surface size " << pixmapWidth << "x" << pixmapHeight << TestLog::EndMessage;
				m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid surface size");
			}
			else
				log << TestLog::Message << "  Pass" << TestLog::EndMessage;
		}
	}

private:
	bool	m_useLegacyCreate;
};

class CreatePbufferSurfaceCase : public SimpleConfigCase
{
public:
	CreatePbufferSurfaceCase (EglTestContext& eglTestCtx, const char* name, const char* description, const vector<EGLint>& configIds)
		: SimpleConfigCase(eglTestCtx, name, description, configIds)
	{
	}

	void executeForConfig (tcu::egl::Display& display, EGLConfig config)
	{
		TestLog&	log		= m_testCtx.getLog();
		EGLint		id		= display.getConfigAttrib(config, EGL_CONFIG_ID);
		int			width	= 64;
		int			height	= 64;

		// \todo [2011-03-23 pyry] Iterate thru all possible combinations of EGL_RENDER_BUFFER, EGL_VG_COLORSPACE and EGL_VG_ALPHA_FORMAT

		log << TestLog::Message << "Creating pbuffer surface with config ID " << id << TestLog::EndMessage;
		TCU_CHECK_EGL();

		// Clamp to maximums reported by implementation
		width	= deMin32(width, display.getConfigAttrib(config, EGL_MAX_PBUFFER_WIDTH));
		height	= deMin32(height, display.getConfigAttrib(config, EGL_MAX_PBUFFER_HEIGHT));

		if (width == 0 || height == 0)
		{
			log << TestLog::Message << "  Fail, maximum pbuffer size of " << width << "x" << height << " reported" << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid maximum pbuffer size");
			return;
		}

		// \todo [2011-03-23 pyry] Texture-backed variants!

		EGLint attribs[] =
		{
			EGL_WIDTH,			width,
			EGL_HEIGHT,			height,
			EGL_TEXTURE_FORMAT,	EGL_NO_TEXTURE,
			EGL_NONE
		};

		EGLSurface surface = eglCreatePbufferSurface(display.getEGLDisplay(), config, attribs);
		TCU_CHECK_EGL_MSG("Failed to create pbuffer");
		TCU_CHECK(surface != EGL_NO_SURFACE);
		eglDestroySurface(display.getEGLDisplay(), surface);

		log << TestLog::Message << "  Pass" << TestLog::EndMessage;
	}
};

} // anonymous

CreateSurfaceTests::CreateSurfaceTests (EglTestContext& eglTestCtx)
	: TestCaseGroup(eglTestCtx, "create_surface", "Basic surface construction tests")
{
}

CreateSurfaceTests::~CreateSurfaceTests (void)
{
}

void CreateSurfaceTests::init (void)
{
	// Window surfaces
	{
		tcu::TestCaseGroup* windowGroup = new tcu::TestCaseGroup(m_testCtx, "window", "Window surfaces");
		addChild(windowGroup);

		eglu::FilterList filters;
		filters << (eglu::ConfigSurfaceType() & EGL_WINDOW_BIT);

		vector<NamedConfigIdSet> configIdSets;
		NamedConfigIdSet::getDefaultSets(configIdSets, m_eglTestCtx.getConfigs(), filters);

		for (vector<NamedConfigIdSet>::iterator i = configIdSets.begin(); i != configIdSets.end(); i++)
			windowGroup->addChild(new CreateWindowSurfaceCase(m_eglTestCtx, i->getName(), i->getDescription(), true, i->getConfigIds()));
	}

	// Pixmap surfaces
	{
		tcu::TestCaseGroup* pixmapGroup = new tcu::TestCaseGroup(m_testCtx, "pixmap", "Pixmap surfaces");
		addChild(pixmapGroup);

		eglu::FilterList filters;
		filters << (eglu::ConfigSurfaceType() & EGL_PIXMAP_BIT);

		vector<NamedConfigIdSet> configIdSets;
		NamedConfigIdSet::getDefaultSets(configIdSets, m_eglTestCtx.getConfigs(), filters);

		for (vector<NamedConfigIdSet>::iterator i = configIdSets.begin(); i != configIdSets.end(); i++)
			pixmapGroup->addChild(new CreatePixmapSurfaceCase(m_eglTestCtx, i->getName(), i->getDescription(), true, i->getConfigIds()));
	}

	// Pbuffer surfaces
	{
		tcu::TestCaseGroup* pbufferGroup = new tcu::TestCaseGroup(m_testCtx, "pbuffer", "Pbuffer surfaces");
		addChild(pbufferGroup);

		eglu::FilterList filters;
		filters << (eglu::ConfigSurfaceType() & EGL_PBUFFER_BIT);

		vector<NamedConfigIdSet> configIdSets;
		NamedConfigIdSet::getDefaultSets(configIdSets, m_eglTestCtx.getConfigs(), filters);

		for (vector<NamedConfigIdSet>::iterator i = configIdSets.begin(); i != configIdSets.end(); i++)
			pbufferGroup->addChild(new CreatePbufferSurfaceCase(m_eglTestCtx, i->getName(), i->getDescription(), i->getConfigIds()));
	}

	// Window surfaces with new platform extension
	{
		tcu::TestCaseGroup* windowGroup = new tcu::TestCaseGroup(m_testCtx, "platform_window", "Window surfaces with platform extension");
		addChild(windowGroup);

		eglu::FilterList filters;
		filters << (eglu::ConfigSurfaceType() & EGL_WINDOW_BIT);

		vector<NamedConfigIdSet> configIdSets;
		NamedConfigIdSet::getDefaultSets(configIdSets, m_eglTestCtx.getConfigs(), filters);

		for (vector<NamedConfigIdSet>::iterator i = configIdSets.begin(); i != configIdSets.end(); i++)
			windowGroup->addChild(new CreateWindowSurfaceCase(m_eglTestCtx, i->getName(), i->getDescription(), false, i->getConfigIds()));
	}

	// Pixmap surfaces with new platform extension
	{
		tcu::TestCaseGroup* pixmapGroup = new tcu::TestCaseGroup(m_testCtx, "platform_pixmap", "Pixmap surfaces with platform extension");
		addChild(pixmapGroup);

		eglu::FilterList filters;
		filters << (eglu::ConfigSurfaceType() & EGL_PIXMAP_BIT);

		vector<NamedConfigIdSet> configIdSets;
		NamedConfigIdSet::getDefaultSets(configIdSets, m_eglTestCtx.getConfigs(), filters);

		for (vector<NamedConfigIdSet>::iterator i = configIdSets.begin(); i != configIdSets.end(); i++)
			pixmapGroup->addChild(new CreatePixmapSurfaceCase(m_eglTestCtx, i->getName(), i->getDescription(), false, i->getConfigIds()));
	}
}

} // egl
} // deqp
