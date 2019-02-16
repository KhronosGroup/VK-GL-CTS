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
#include "egluUnique.hpp"

#include "eglwLibrary.hpp"
#include "eglwEnums.hpp"

#include "teglSimpleConfigCase.hpp"
#include "tcuTestContext.hpp"
#include "tcuCommandLine.hpp"
#include "tcuTestLog.hpp"

#include "deStringUtil.hpp"
#include "deSTLUtil.hpp"
#include "deUniquePtr.hpp"

#include <memory>

namespace deqp
{
namespace egl
{

using std::vector;
using tcu::TestLog;
using namespace eglw;

namespace
{

#define EGL_MAKE_VERSION(major, minor) (((major) << 12) | (minor))

enum ApiType
{
	LEGACY,
	EXTENSION,
	EGL15
};

void checkEGLPlatformSupport (const Library& egl)
{
	const vector<std::string>	extensions	= eglu::getClientExtensions(egl);
	if (!de::contains(extensions.begin(), extensions.end(), "EGL_EXT_platform_base"))
		throw tcu::NotSupportedError("Platform extension 'EGL_EXT_platform_base' not supported", "", __FILE__, __LINE__);
}

void checkEGL15Support (const Library& egl, EGLDisplay display)
{
	// The EGL_VERSION string is laid out as follows:
	// major_version.minor_version space vendor_specific_info
	// Split version from vendor_specific_info
	std::vector<std::string> tokens = de::splitString(egl.queryString(display, EGL_VERSION), ' ');
	// split version into major & minor
	std::vector<std::string> values = de::splitString(tokens[0], '.');
	EGLint eglVersion = EGL_MAKE_VERSION(atoi(values[0].c_str()), atoi(values[1].c_str()));
	if (eglVersion < EGL_MAKE_VERSION(1, 5))
		throw tcu::NotSupportedError("EGL 1.5 not supported", "", __FILE__, __LINE__);
}

EGLSurface createWindowSurface (EGLDisplay display, EGLConfig config, eglu::NativeDisplay& nativeDisplay, eglu::NativeWindow& window, ApiType createType)
{
	const Library&	egl		= nativeDisplay.getLibrary();
	EGLSurface		surface	= EGL_NO_SURFACE;

	switch (createType)
	{
		case LEGACY:
		{
			surface = egl.createWindowSurface(display, config, window.getLegacyNative(), DE_NULL);
			EGLU_CHECK_MSG(egl, "eglCreateWindowSurface() failed");
		}
		break;
		case EXTENSION:
		{
			checkEGLPlatformSupport(egl);
			void *nativeWindow = window.getPlatformExtension();
			surface = egl.createPlatformWindowSurfaceEXT(display, config, nativeWindow, DE_NULL);
			EGLU_CHECK_MSG(egl, "eglCreatePlatformWindowSurfaceEXT() failed");
		}
		break;
		case EGL15:
		{
			checkEGL15Support(egl, display);
			surface = egl.createPlatformWindowSurface(display, config, window.getPlatformNative(), DE_NULL);
			EGLU_CHECK_MSG(egl, "eglCreatePlatformWindowSurface() failed");
		}
	}

	return surface;
}

EGLSurface createPixmapSurface (EGLDisplay display, EGLConfig config, eglu::NativeDisplay& nativeDisplay, eglu::NativePixmap& pixmap, ApiType createType)
{
	const Library&	egl		= nativeDisplay.getLibrary();
	EGLSurface		surface	= EGL_NO_SURFACE;

	switch (createType)
	{
		case LEGACY:
			surface = egl.createPixmapSurface(display, config, pixmap.getLegacyNative(), DE_NULL);
			EGLU_CHECK_MSG(egl, "eglCreatePixmapSurface() failed");
		break;
		case EXTENSION:
			checkEGLPlatformSupport(egl);
			surface = egl.createPlatformPixmapSurfaceEXT(display, config, pixmap.getPlatformExtension(), DE_NULL);
			EGLU_CHECK_MSG(egl, "eglCreatePlatformPixmapSurfaceEXT() failed");
		break;
		case EGL15:
			surface = egl.createPlatformPixmapSurface(display, config, pixmap.getPlatformNative(), DE_NULL);
			EGLU_CHECK_MSG(egl, "eglCreatePlatformPixmapSurface() failed");
		break;
	}

	return surface;
}

class CreateWindowSurfaceCase : public SimpleConfigCase
{
public:
	CreateWindowSurfaceCase (EglTestContext& eglTestCtx, const char* name, const char* description, ApiType createType, const eglu::FilterList& filters)
		: SimpleConfigCase	(eglTestCtx, name, description, filters)
		, m_createType	(createType)
	{
	}

	void executeForConfig (EGLDisplay display, EGLConfig config)
	{
		const Library&						egl				= m_eglTestCtx.getLibrary();
		TestLog&							log				= m_testCtx.getLog();
		EGLint								id				= eglu::getConfigID(egl, display, config);
		const eglu::NativeWindowFactory&	windowFactory	= eglu::selectNativeWindowFactory(m_eglTestCtx.getNativeDisplayFactory(), m_testCtx.getCommandLine());

		// \todo [2011-03-23 pyry] Iterate thru all possible combinations of EGL_RENDER_BUFFER, EGL_VG_COLORSPACE and EGL_VG_ALPHA_FORMAT

		switch (m_createType)
		{
			case LEGACY:
			{
				if ((windowFactory.getCapabilities() & eglu::NativeWindow::CAPABILITY_CREATE_SURFACE_LEGACY) == 0)
					TCU_THROW(NotSupportedError, "Native window doesn't support legacy eglCreateWindowSurface()");
			}
			break;
			case EXTENSION:
			{
				if ((windowFactory.getCapabilities() & eglu::NativeWindow::CAPABILITY_CREATE_SURFACE_PLATFORM_EXTENSION) == 0)
					TCU_THROW(NotSupportedError, "Native window doesn't support eglCreatePlatformWindowSurfaceEXT()");
			}
			break;
			case EGL15:
			{
				if ((windowFactory.getCapabilities() & eglu::NativeWindow::CAPABILITY_CREATE_SURFACE_PLATFORM) == 0)
					TCU_THROW(NotSupportedError, "Native window doesn't support eglCreatePlatformWindowSurface()");
			}
			break;
		}

		log << TestLog::Message << "Creating window surface with config ID " << id << TestLog::EndMessage;
		EGLU_CHECK_MSG(egl, "init");

		{
			const int							width			= 64;
			const int							height			= 64;
			de::UniquePtr<eglu::NativeWindow>	window			(windowFactory.createWindow(&m_eglTestCtx.getNativeDisplay(), display, config, DE_NULL, eglu::WindowParams(width, height, eglu::parseWindowVisibility(m_testCtx.getCommandLine()))));
			eglu::UniqueSurface					surface			(egl, display, createWindowSurface(display, config, m_eglTestCtx.getNativeDisplay(), *window, m_createType));

			EGLint								windowWidth		= 0;
			EGLint								windowHeight	= 0;

			EGLU_CHECK_CALL(egl, querySurface(display, *surface, EGL_WIDTH,		&windowWidth));
			EGLU_CHECK_CALL(egl, querySurface(display, *surface, EGL_HEIGHT,	&windowHeight));

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
	ApiType		m_createType;
};

class CreatePixmapSurfaceCase : public SimpleConfigCase
{
public:
	CreatePixmapSurfaceCase (EglTestContext& eglTestCtx, const char* name, const char* description, ApiType createType, const eglu::FilterList& filters)
		: SimpleConfigCase(eglTestCtx, name, description, filters)
		, m_createType	(createType)
	{
	}

	void executeForConfig (EGLDisplay display, EGLConfig config)
	{
		const Library&						egl				= m_eglTestCtx.getLibrary();
		TestLog&							log				= m_testCtx.getLog();
		EGLint								id				= eglu::getConfigID(egl, display, config);
		const eglu::NativePixmapFactory&	pixmapFactory	= eglu::selectNativePixmapFactory(m_eglTestCtx.getNativeDisplayFactory(), m_testCtx.getCommandLine());

		// \todo [2011-03-23 pyry] Iterate thru all possible combinations of EGL_RENDER_BUFFER, EGL_VG_COLORSPACE and EGL_VG_ALPHA_FORMAT

		switch (m_createType)
		{
			case LEGACY:
			{
				if ((pixmapFactory.getCapabilities() & eglu::NativePixmap::CAPABILITY_CREATE_SURFACE_LEGACY) == 0)
					TCU_THROW(NotSupportedError, "Native pixmap doesn't support legacy eglCreatePixmapSurface()");
			}
			break;
			case EXTENSION:
			{
				if ((pixmapFactory.getCapabilities() & eglu::NativePixmap::CAPABILITY_CREATE_SURFACE_PLATFORM_EXTENSION) == 0)
					TCU_THROW(NotSupportedError, "Native pixmap doesn't support eglCreatePlatformPixmapSurfaceEXT()");
			}
			break;
			case EGL15:
			{
				if ((pixmapFactory.getCapabilities() & eglu::NativePixmap::CAPABILITY_CREATE_SURFACE_PLATFORM) == 0)
					TCU_THROW(NotSupportedError, "Native pixmap doesn't support eglCreatePlatformPixmapSurface()");
			}
			break;
		};

		log << TestLog::Message << "Creating pixmap surface with config ID " << id << TestLog::EndMessage;
		EGLU_CHECK_MSG(egl, "init");

		{
			const int							width			= 64;
			const int							height			= 64;
			de::UniquePtr<eglu::NativePixmap>	pixmap			(pixmapFactory.createPixmap(&m_eglTestCtx.getNativeDisplay(), display, config, DE_NULL, width, height));
			eglu::UniqueSurface					surface			(egl, display, createPixmapSurface(display, config, m_eglTestCtx.getNativeDisplay(), *pixmap, m_createType));
			EGLint								pixmapWidth		= 0;
			EGLint								pixmapHeight	= 0;

			EGLU_CHECK_CALL(egl, querySurface(display, *surface, EGL_WIDTH,		&pixmapWidth));
			EGLU_CHECK_CALL(egl, querySurface(display, *surface, EGL_HEIGHT,	&pixmapHeight));

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
	ApiType		m_createType;
};

class CreatePbufferSurfaceCase : public SimpleConfigCase
{
public:
	CreatePbufferSurfaceCase (EglTestContext& eglTestCtx, const char* name, const char* description, const eglu::FilterList& filters)
		: SimpleConfigCase(eglTestCtx, name, description, filters)
	{
	}

	void executeForConfig (EGLDisplay display, EGLConfig config)
	{
		const Library&	egl		= m_eglTestCtx.getLibrary();
		TestLog&		log		= m_testCtx.getLog();
		EGLint			id		= eglu::getConfigID(egl, display, config);
		int				width	= 64;
		int				height	= 64;

		// \todo [2011-03-23 pyry] Iterate thru all possible combinations of EGL_RENDER_BUFFER, EGL_VG_COLORSPACE and EGL_VG_ALPHA_FORMAT

		log << TestLog::Message << "Creating pbuffer surface with config ID " << id << TestLog::EndMessage;
		EGLU_CHECK_MSG(egl, "init");

		// Clamp to maximums reported by implementation
		width	= deMin32(width, eglu::getConfigAttribInt(egl, display, config, EGL_MAX_PBUFFER_WIDTH));
		height	= deMin32(height, eglu::getConfigAttribInt(egl, display, config, EGL_MAX_PBUFFER_HEIGHT));

		if (width == 0 || height == 0)
		{
			log << TestLog::Message << "  Fail, maximum pbuffer size of " << width << "x" << height << " reported" << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid maximum pbuffer size");
			return;
		}

		// \todo [2011-03-23 pyry] Texture-backed variants!

		const EGLint attribs[] =
		{
			EGL_WIDTH,			width,
			EGL_HEIGHT,			height,
			EGL_TEXTURE_FORMAT,	EGL_NO_TEXTURE,
			EGL_NONE
		};

		EGLSurface surface = egl.createPbufferSurface(display, config, attribs);
		EGLU_CHECK_MSG(egl, "Failed to create pbuffer");
		TCU_CHECK(surface != EGL_NO_SURFACE);
		egl.destroySurface(display, surface);

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

template <deUint32 Type>
static bool surfaceType (const eglu::CandidateConfig& c)
{
	return (c.surfaceType() & Type) == Type;
}

void CreateSurfaceTests::init (void)
{
	// Window surfaces
	{
		tcu::TestCaseGroup* windowGroup = new tcu::TestCaseGroup(m_testCtx, "window", "Window surfaces");
		addChild(windowGroup);

		eglu::FilterList baseFilters;
		baseFilters << surfaceType<EGL_WINDOW_BIT>;

		vector<NamedFilterList> filterLists;
		getDefaultFilterLists(filterLists, baseFilters);

		for (vector<NamedFilterList>::iterator i = filterLists.begin(); i != filterLists.end(); i++)
			windowGroup->addChild(new CreateWindowSurfaceCase(m_eglTestCtx, i->getName(), i->getDescription(), LEGACY, *i));
	}

	// Pixmap surfaces
	{
		tcu::TestCaseGroup* pixmapGroup = new tcu::TestCaseGroup(m_testCtx, "pixmap", "Pixmap surfaces");
		addChild(pixmapGroup);

		eglu::FilterList baseFilters;
		baseFilters << surfaceType<EGL_PIXMAP_BIT>;

		vector<NamedFilterList> filterLists;
		getDefaultFilterLists(filterLists, baseFilters);

		for (vector<NamedFilterList>::iterator i = filterLists.begin(); i != filterLists.end(); i++)
			pixmapGroup->addChild(new CreatePixmapSurfaceCase(m_eglTestCtx, i->getName(), i->getDescription(), LEGACY, *i));
	}

	// Pbuffer surfaces
	{
		tcu::TestCaseGroup* pbufferGroup = new tcu::TestCaseGroup(m_testCtx, "pbuffer", "Pbuffer surfaces");
		addChild(pbufferGroup);

		eglu::FilterList baseFilters;
		baseFilters << surfaceType<EGL_PBUFFER_BIT>;

		vector<NamedFilterList> filterLists;
		getDefaultFilterLists(filterLists, baseFilters);

		for (vector<NamedFilterList>::iterator i = filterLists.begin(); i != filterLists.end(); i++)
			pbufferGroup->addChild(new CreatePbufferSurfaceCase(m_eglTestCtx, i->getName(), i->getDescription(), *i));
	}

	// Window surfaces with new platform extension
	{
		tcu::TestCaseGroup* windowGroup = new tcu::TestCaseGroup(m_testCtx, "platform_ext_window", "Window surfaces with platform extension");
		addChild(windowGroup);

		eglu::FilterList baseFilters;
		baseFilters << surfaceType<EGL_WINDOW_BIT>;

		vector<NamedFilterList> filterLists;
		getDefaultFilterLists(filterLists, baseFilters);

		for (vector<NamedFilterList>::iterator i = filterLists.begin(); i != filterLists.end(); i++)
			windowGroup->addChild(new CreateWindowSurfaceCase(m_eglTestCtx, i->getName(), i->getDescription(), EXTENSION, *i));
	}

	// Pixmap surfaces with new platform extension
	{
		tcu::TestCaseGroup* pixmapGroup = new tcu::TestCaseGroup(m_testCtx, "platform_ext_pixmap", "Pixmap surfaces with platform extension");
		addChild(pixmapGroup);

		eglu::FilterList baseFilters;
		baseFilters << surfaceType<EGL_PIXMAP_BIT>;

		vector<NamedFilterList> filterLists;
		getDefaultFilterLists(filterLists, baseFilters);

		for (vector<NamedFilterList>::iterator i = filterLists.begin(); i != filterLists.end(); i++)
			pixmapGroup->addChild(new CreatePixmapSurfaceCase(m_eglTestCtx, i->getName(), i->getDescription(), EXTENSION, *i));
	}
	//
	// Window surfaces with EGL 1.5 CreateWindowSurface
	{
		tcu::TestCaseGroup* windowGroup = new tcu::TestCaseGroup(m_testCtx, "platform_window", "Window surfaces with EGL 1.5");
		addChild(windowGroup);

		eglu::FilterList baseFilters;
		baseFilters << surfaceType<EGL_WINDOW_BIT>;

		vector<NamedFilterList> filterLists;
		getDefaultFilterLists(filterLists, baseFilters);

		for (vector<NamedFilterList>::iterator i = filterLists.begin(); i != filterLists.end(); i++)
			windowGroup->addChild(new CreateWindowSurfaceCase(m_eglTestCtx, i->getName(), i->getDescription(), EGL15, *i));
	}

	// Pixmap surfaces with EGL 1.5 CreateWindowSurface
	{
		tcu::TestCaseGroup* pixmapGroup = new tcu::TestCaseGroup(m_testCtx, "platform_pixmap", "Pixmap surfaces with EGL 1.5");
		addChild(pixmapGroup);

		eglu::FilterList baseFilters;
		baseFilters << surfaceType<EGL_PIXMAP_BIT>;

		vector<NamedFilterList> filterLists;
		getDefaultFilterLists(filterLists, baseFilters);

		for (vector<NamedFilterList>::iterator i = filterLists.begin(); i != filterLists.end(); i++)
			pixmapGroup->addChild(new CreatePixmapSurfaceCase(m_eglTestCtx, i->getName(), i->getDescription(), EGL15, *i));
	}
}

} // egl
} // deqp
