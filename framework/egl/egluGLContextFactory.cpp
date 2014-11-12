/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
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
 * \brief GL context factory using EGL.
 *//*--------------------------------------------------------------------*/

#include "egluGLContextFactory.hpp"

#include "tcuRenderTarget.hpp"
#include "tcuPlatform.hpp"
#include "tcuCommandLine.hpp"

#include "gluDefs.hpp"

#include "egluDefs.hpp"
#include "egluHeaderWrapper.hpp"
#include "egluUtil.hpp"
#include "egluGLUtil.hpp"
#include "egluNativeWindow.hpp"
#include "egluNativePixmap.hpp"
#include "egluStrUtil.hpp"

#include "glwInitFunctions.hpp"
#include "glwInitES20Direct.hpp"
#include "glwInitES30Direct.hpp"

#include "deDynamicLibrary.hpp"
#include "deSTLUtil.hpp"

#include <string>
#include <string>
#include <sstream>

using std::string;
using std::vector;

// \todo [2014-03-12 pyry] Use command line arguments for libraries?

// Default library names
#if !defined(DEQP_GLES2_LIBRARY_PATH)
#	if (DE_OS == DE_OS_WIN32)
#		define DEQP_GLES2_LIBRARY_PATH "libGLESv2.dll"
#	else
#		define DEQP_GLES2_LIBRARY_PATH "libGLESv2.so"
#	endif
#endif

#if !defined(DEQP_GLES3_LIBRARY_PATH)
#	define DEQP_GLES3_LIBRARY_PATH DEQP_GLES2_LIBRARY_PATH
#endif

#if !defined(DEQP_OPENGL_LIBRARY_PATH)
#	if (DE_OS == DE_OS_WIN32)
#		define DEQP_OPENGL_LIBRARY_PATH "opengl32.dll"
#	else
#		define DEQP_OPENGL_LIBRARY_PATH "libGL.so"
#	endif
#endif

namespace eglu
{

namespace
{

enum
{
	DEFAULT_OFFSCREEN_WIDTH		= 512,
	DEFAULT_OFFSCREEN_HEIGHT	= 512
};

class GetProcFuncLoader : public glw::FunctionLoader
{
public:
	glw::GenericFuncType get (const char* name) const
	{
		return (glw::GenericFuncType)eglGetProcAddress(name);
	}
};

class DynamicFuncLoader : public glw::FunctionLoader
{
public:
	DynamicFuncLoader	(de::DynamicLibrary* library)
		: m_library(library)
	{
	}

	glw::GenericFuncType get (const char* name) const
	{
		return (glw::GenericFuncType)m_library->getFunction(name);
	}

private:
	de::DynamicLibrary*	m_library;
};

class RenderContext : public GLRenderContext
{
public:
										RenderContext			(const NativeDisplayFactory* displayFactory, const NativeWindowFactory* windowFactory, const NativePixmapFactory* pixmapFactory, const glu::RenderConfig& config);
	virtual								~RenderContext			(void);

	virtual glu::ContextType			getType					(void) const { return m_renderConfig.type;	}
	virtual const glw::Functions&		getFunctions			(void) const { return m_glFunctions;		}
	virtual const tcu::RenderTarget&	getRenderTarget			(void) const { return m_glRenderTarget;		}
	virtual void						postIterate				(void);

	virtual EGLDisplay					getEGLDisplay			(void) const { return m_eglDisplay;			}
	virtual EGLContext					getEGLContext			(void) const { return m_eglContext;			}

private:
	void								create					(const NativeDisplayFactory* displayFactory, const NativeWindowFactory* windowFactory, const NativePixmapFactory* pixmapFactory, const glu::RenderConfig& config);
	void								destroy					(void);

	const glu::RenderConfig				m_renderConfig;
	const NativeWindowFactory* const	m_nativeWindowFactory;	// Stored in case window must be re-created

	NativeDisplay*						m_display;
	NativeWindow*						m_window;
	NativePixmap*						m_pixmap;

	EGLDisplay							m_eglDisplay;
	EGLConfig							m_eglConfig;
	EGLSurface							m_eglSurface;
	EGLContext							m_eglContext;

	tcu::RenderTarget					m_glRenderTarget;
	de::DynamicLibrary*					m_dynamicGLLibrary;
	glw::Functions						m_glFunctions;
};

RenderContext::RenderContext (const NativeDisplayFactory* displayFactory, const NativeWindowFactory* windowFactory, const NativePixmapFactory* pixmapFactory, const glu::RenderConfig& config)
	: m_renderConfig		(config)
	, m_nativeWindowFactory	(windowFactory)
	, m_display				(DE_NULL)
	, m_window				(DE_NULL)
	, m_pixmap				(DE_NULL)

	, m_eglDisplay			(EGL_NO_DISPLAY)
	, m_eglSurface			(EGL_NO_SURFACE)
	, m_eglContext			(EGL_NO_CONTEXT)

	, m_dynamicGLLibrary	(DE_NULL)
{
	DE_ASSERT(displayFactory);

	try
	{
		create(displayFactory, windowFactory, pixmapFactory, config);
	}
	catch (...)
	{
		destroy();
		throw;
	}
}

RenderContext::~RenderContext(void)
{
	try
	{
		destroy();
	}
	catch (...)
	{
		// destroy() calls EGL functions that are checked and may throw exceptions
	}

	delete m_window;
	delete m_pixmap;
	delete m_display;
	delete m_dynamicGLLibrary;
}

bool configMatches (EGLDisplay display, EGLConfig eglConfig, const glu::RenderConfig& renderConfig)
{
	// \todo [2014-03-12 pyry] Check other attributes like double-buffer bit.

	{
		EGLint		renderableType		= 0;
		EGLint		requiredRenderable	= apiRenderableType(renderConfig.type.getAPI());

		EGLU_CHECK_CALL(eglGetConfigAttrib(display, eglConfig, EGL_RENDERABLE_TYPE, &renderableType));

		if ((renderableType & requiredRenderable) == 0)
			return false;
	}

	if (renderConfig.surfaceType != (glu::RenderConfig::SurfaceType)glu::RenderConfig::DONT_CARE)
	{
		EGLint		surfaceType		= 0;
		EGLint		requiredSurface	= 0;

		switch (renderConfig.surfaceType)
		{
			case glu::RenderConfig::SURFACETYPE_WINDOW:				requiredSurface = EGL_WINDOW_BIT;	break;
			case glu::RenderConfig::SURFACETYPE_OFFSCREEN_NATIVE:	requiredSurface = EGL_PIXMAP_BIT;	break;
			case glu::RenderConfig::SURFACETYPE_OFFSCREEN_GENERIC:	requiredSurface = EGL_PBUFFER_BIT;	break;
			default:
				DE_ASSERT(false);
		}

		EGLU_CHECK_CALL(eglGetConfigAttrib(display, eglConfig, EGL_SURFACE_TYPE, &surfaceType));

		if ((surfaceType & requiredSurface) == 0)
			return false;
	}

	{
		static const struct
		{
			int	glu::RenderConfig::*field;
			EGLint attrib;
		} s_attribs[] =
		{
			{ &glu::RenderConfig::id,			EGL_CONFIG_ID		},
			{ &glu::RenderConfig::redBits,		EGL_RED_SIZE		},
			{ &glu::RenderConfig::greenBits,	EGL_GREEN_SIZE		},
			{ &glu::RenderConfig::blueBits,		EGL_BLUE_SIZE		},
			{ &glu::RenderConfig::alphaBits,	EGL_ALPHA_SIZE		},
			{ &glu::RenderConfig::depthBits,	EGL_DEPTH_SIZE		},
			{ &glu::RenderConfig::stencilBits,	EGL_STENCIL_SIZE	},
			{ &glu::RenderConfig::numSamples,	EGL_SAMPLES			},
		};

		for (int attribNdx = 0; attribNdx < DE_LENGTH_OF_ARRAY(s_attribs); attribNdx++)
		{
			if (renderConfig.*s_attribs[attribNdx].field != glu::RenderConfig::DONT_CARE)
			{
				EGLint value = 0;
				EGLU_CHECK_CALL(eglGetConfigAttrib(display, eglConfig, s_attribs[attribNdx].attrib, &value));
				if (value != renderConfig.*s_attribs[attribNdx].field)
					return false;
			}
		}
	}

	return true;
}

EGLConfig chooseConfig (EGLDisplay display, const glu::RenderConfig& config)
{
	const std::vector<EGLConfig> configs = eglu::getConfigs(display);

	for (vector<EGLConfig>::const_iterator iter = configs.begin(); iter != configs.end(); ++iter)
	{
		if (configMatches(display, *iter, config))
			return *iter;
	}

	throw tcu::NotSupportedError("Matching EGL config not found", DE_NULL, __FILE__, __LINE__);
}

static WindowParams::Visibility getNativeWindowVisibility (glu::RenderConfig::Visibility visibility)
{
	using glu::RenderConfig;

	switch (visibility)
	{
		case RenderConfig::VISIBILITY_HIDDEN:		return WindowParams::VISIBILITY_HIDDEN;
		case RenderConfig::VISIBILITY_VISIBLE:		return WindowParams::VISIBILITY_VISIBLE;
		case RenderConfig::VISIBILITY_FULLSCREEN:	return WindowParams::VISIBILITY_FULLSCREEN;
		default:
			DE_ASSERT(visibility == (RenderConfig::Visibility)RenderConfig::DONT_CARE);
			return WindowParams::VISIBILITY_DONT_CARE;
	}
}

typedef std::pair<NativeWindow*, EGLSurface> WindowSurfacePair;
typedef std::pair<NativePixmap*, EGLSurface> PixmapSurfacePair;

WindowSurfacePair createWindow (NativeDisplay* nativeDisplay, const NativeWindowFactory* windowFactory, EGLDisplay eglDisplay, EGLConfig eglConfig, const glu::RenderConfig& config)
{
	const int						width			= (config.width		== glu::RenderConfig::DONT_CARE ? WindowParams::SIZE_DONT_CARE	: config.width);
	const int						height			= (config.height	== glu::RenderConfig::DONT_CARE ? WindowParams::SIZE_DONT_CARE	: config.height);
	const WindowParams::Visibility	visibility		= getNativeWindowVisibility(config.windowVisibility);
	NativeWindow*					nativeWindow	= DE_NULL;
	EGLSurface						surface			= EGL_NO_SURFACE;
	const EGLAttrib					attribList[]	= { EGL_NONE };

	nativeWindow = windowFactory->createWindow(nativeDisplay, eglDisplay, eglConfig, &attribList[0], WindowParams(width, height, visibility));

	try
	{
		surface = eglu::createWindowSurface(*nativeDisplay, *nativeWindow, eglDisplay, eglConfig, attribList);
	}
	catch (...)
	{
		delete nativeWindow;
		throw;
	}

	return WindowSurfacePair(nativeWindow, surface);
}

PixmapSurfacePair createPixmap (NativeDisplay* nativeDisplay, const NativePixmapFactory* pixmapFactory, EGLDisplay eglDisplay, EGLConfig eglConfig, const glu::RenderConfig& config)
{
	const int			width			= (config.width		== glu::RenderConfig::DONT_CARE ? DEFAULT_OFFSCREEN_WIDTH	: config.width);
	const int			height			= (config.height	== glu::RenderConfig::DONT_CARE ? DEFAULT_OFFSCREEN_HEIGHT	: config.height);
	NativePixmap*		nativePixmap	= DE_NULL;
	EGLSurface			surface			= EGL_NO_SURFACE;
	const EGLAttrib		attribList[]	= { EGL_NONE };

	nativePixmap = pixmapFactory->createPixmap(nativeDisplay, eglDisplay, eglConfig, &attribList[0], width, height);

	try
	{
		surface = eglu::createPixmapSurface(*nativeDisplay, *nativePixmap, eglDisplay, eglConfig, attribList);
	}
	catch (...)
	{
		delete nativePixmap;
		throw;
	}

	return PixmapSurfacePair(nativePixmap, surface);
}

EGLSurface createPBuffer (EGLDisplay display, EGLConfig eglConfig, const glu::RenderConfig& config)
{
	const int		width			= (config.width		== glu::RenderConfig::DONT_CARE ? DEFAULT_OFFSCREEN_WIDTH	: config.width);
	const int		height			= (config.height	== glu::RenderConfig::DONT_CARE ? DEFAULT_OFFSCREEN_HEIGHT	: config.height);
	EGLSurface		surface;
	const EGLint	attribList[]	=
	{
		EGL_WIDTH,	width,
		EGL_HEIGHT,	height,
		EGL_NONE
	};

	surface = eglCreatePbufferSurface(display, eglConfig, &(attribList[0]));
	EGLU_CHECK_MSG("eglCreatePbufferSurface()");

	return surface;
}

void RenderContext::create (const NativeDisplayFactory* displayFactory, const NativeWindowFactory* windowFactory, const NativePixmapFactory* pixmapFactory, const glu::RenderConfig& config)
{
	glu::RenderConfig::SurfaceType	surfaceType	= config.surfaceType;

	DE_ASSERT(displayFactory);

	m_display		= displayFactory->createDisplay();
	m_eglDisplay	= eglu::getDisplay(*m_display);

	{
		EGLint major = 0;
		EGLint minor = 0;
		EGLU_CHECK_CALL(eglInitialize(m_eglDisplay, &major, &minor));
	}

	m_eglConfig	= chooseConfig(m_eglDisplay, config);

	if (surfaceType == glu::RenderConfig::SURFACETYPE_DONT_CARE)
	{
		// Choose based on what selected configuration supports
		const EGLint supportedTypes = eglu::getConfigAttribInt(m_eglDisplay, m_eglConfig, EGL_SURFACE_TYPE);

		if ((supportedTypes & EGL_WINDOW_BIT) != 0)
			surfaceType = glu::RenderConfig::SURFACETYPE_WINDOW;
		else if ((supportedTypes & EGL_PBUFFER_BIT) != 0)
			surfaceType = glu::RenderConfig::SURFACETYPE_OFFSCREEN_GENERIC;
		else if ((supportedTypes & EGL_PIXMAP_BIT) != 0)
			surfaceType = glu::RenderConfig::SURFACETYPE_OFFSCREEN_NATIVE;
		else
			throw tcu::NotSupportedError("Selected EGL config doesn't support any surface types", DE_NULL, __FILE__, __LINE__);
	}

	switch (surfaceType)
	{
		case glu::RenderConfig::SURFACETYPE_WINDOW:
		{
			if (windowFactory)
			{
				const WindowSurfacePair windowSurface = createWindow(m_display, windowFactory, m_eglDisplay, m_eglConfig, config);
				m_window		= windowSurface.first;
				m_eglSurface	= windowSurface.second;
			}
			else
				throw tcu::NotSupportedError("EGL platform doesn't support windows", DE_NULL, __FILE__, __LINE__);
			break;
		}

		case glu::RenderConfig::SURFACETYPE_OFFSCREEN_NATIVE:
		{
			if (pixmapFactory)
			{
				const PixmapSurfacePair pixmapSurface = createPixmap(m_display, pixmapFactory, m_eglDisplay, m_eglConfig, config);
				m_pixmap		= pixmapSurface.first;
				m_eglSurface	= pixmapSurface.second;
			}
			else
				throw tcu::NotSupportedError("EGL platform doesn't support pixmaps", DE_NULL, __FILE__, __LINE__);
			break;
		}

		case glu::RenderConfig::SURFACETYPE_OFFSCREEN_GENERIC:
			m_eglSurface = createPBuffer(m_eglDisplay, m_eglConfig, config);
			break;

		default:
			throw tcu::InternalError("Invalid surface type");
	}

	m_eglContext = createGLContext(m_eglDisplay, m_eglConfig, config.type);

	EGLU_CHECK_CALL(eglMakeCurrent(m_eglDisplay, m_eglSurface, m_eglSurface, m_eglContext));

	// Init core functions

	if (hasExtension(m_eglDisplay, "EGL_KHR_get_all_proc_addresses"))
	{
		// Use eglGetProcAddress() for core functions
		GetProcFuncLoader funcLoader;
		glu::initCoreFunctions(&m_glFunctions, &funcLoader, config.type.getAPI());
	}
#if !defined(DEQP_GLES2_RUNTIME_LOAD)
	else if (config.type.getAPI() == glu::ApiType::es(2,0))
	{
		glw::initES20Direct(&m_glFunctions);
	}
#endif
#if !defined(DEQP_GLES3_RUNTIME_LOAD)
	else if (config.type.getAPI() == glu::ApiType::es(3,0))
	{
		glw::initES30Direct(&m_glFunctions);
	}
#endif
	else
	{
		const char* libraryPath = DE_NULL;

		if (glu::isContextTypeES(config.type))
		{
			if (config.type.getMinorVersion() <= 2)
				libraryPath = DEQP_GLES2_LIBRARY_PATH;
			else
				libraryPath = DEQP_GLES3_LIBRARY_PATH;
		}
		else
			libraryPath = DEQP_OPENGL_LIBRARY_PATH;

		m_dynamicGLLibrary = new de::DynamicLibrary(libraryPath);

		DynamicFuncLoader funcLoader(m_dynamicGLLibrary);
		glu::initCoreFunctions(&m_glFunctions, &funcLoader, config.type.getAPI());
	}

	// Init extension functions
	{
		GetProcFuncLoader extLoader;
		glu::initExtensionFunctions(&m_glFunctions, &extLoader, config.type.getAPI());
	}

	{
		EGLint				width, height, depthBits, stencilBits, numSamples;
		tcu::PixelFormat	pixelFmt;

		eglQuerySurface(m_eglDisplay, m_eglSurface, EGL_WIDTH, &width);
		eglQuerySurface(m_eglDisplay, m_eglSurface, EGL_HEIGHT, &height);

		eglGetConfigAttrib(m_eglDisplay, m_eglConfig, EGL_RED_SIZE,		&pixelFmt.redBits);
		eglGetConfigAttrib(m_eglDisplay, m_eglConfig, EGL_GREEN_SIZE,	&pixelFmt.greenBits);
		eglGetConfigAttrib(m_eglDisplay, m_eglConfig, EGL_BLUE_SIZE,	&pixelFmt.blueBits);
		eglGetConfigAttrib(m_eglDisplay, m_eglConfig, EGL_ALPHA_SIZE,	&pixelFmt.alphaBits);

		eglGetConfigAttrib(m_eglDisplay, m_eglConfig, EGL_DEPTH_SIZE,	&depthBits);
		eglGetConfigAttrib(m_eglDisplay, m_eglConfig, EGL_STENCIL_SIZE,	&stencilBits);
		eglGetConfigAttrib(m_eglDisplay, m_eglConfig, EGL_SAMPLES,		&numSamples);

		EGLU_CHECK_MSG("Failed to query config attributes");

		m_glRenderTarget = tcu::RenderTarget(width, height, pixelFmt, depthBits, stencilBits, numSamples);
	}
}

void RenderContext::destroy (void)
{
	if (m_eglDisplay != EGL_NO_DISPLAY)
	{
		EGLU_CHECK_CALL(eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));

		if (m_eglSurface != EGL_NO_SURFACE)
			EGLU_CHECK_CALL(eglDestroySurface(m_eglDisplay, m_eglSurface));

		if (m_eglContext != EGL_NO_CONTEXT)
			EGLU_CHECK_CALL(eglDestroyContext(m_eglDisplay, m_eglContext));

		EGLU_CHECK_CALL(eglTerminate(m_eglDisplay));

		m_eglDisplay	= EGL_NO_DISPLAY;
		m_eglSurface	= EGL_NO_SURFACE;
		m_eglContext	= EGL_NO_CONTEXT;
	}

	delete m_window;
	delete m_pixmap;
	delete m_display;
	delete m_dynamicGLLibrary;

	m_window			= DE_NULL;
	m_pixmap			= DE_NULL;
	m_display			= DE_NULL;
	m_dynamicGLLibrary	= DE_NULL;
}

void RenderContext::postIterate (void)
{
	if (m_window)
	{
		EGLBoolean	swapOk		= eglSwapBuffers(m_eglDisplay, m_eglSurface);
		EGLint		error		= eglGetError();
		const bool	badWindow	= error == EGL_BAD_SURFACE || error == EGL_BAD_NATIVE_WINDOW;

		if (!swapOk && !badWindow)
			throw tcu::ResourceError(string("eglSwapBuffers() failed: ") + getErrorStr(error).toString());

		try
		{
			m_window->processEvents();
		}
		catch (const WindowDestroyedError&)
		{
			tcu::print("Warning: Window destroyed, recreating...\n");

			EGLU_CHECK_CALL(eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
			EGLU_CHECK_CALL(eglDestroySurface(m_eglDisplay, m_eglSurface));
			m_eglSurface = EGL_NO_SURFACE;

			delete m_window;
			m_window = DE_NULL;

			try
			{
				WindowSurfacePair windowSurface = createWindow(m_display, m_nativeWindowFactory, m_eglDisplay, m_eglConfig, m_renderConfig);
				m_window		= windowSurface.first;
				m_eglSurface	= windowSurface.second;

				EGLU_CHECK_CALL(eglMakeCurrent(m_eglDisplay, m_eglSurface, m_eglSurface, m_eglContext));

				swapOk	= EGL_TRUE;
				error	= EGL_SUCCESS;
			}
			catch (const std::exception& e)
			{
				if (m_eglSurface)
				{
					eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
					eglDestroySurface(m_eglDisplay, m_eglSurface);
					m_eglSurface = EGL_NO_SURFACE;
				}

				delete m_window;
				m_window = DE_NULL;

				throw tcu::ResourceError(string("Failed to re-create window: ") + e.what());
			}
		}

		if (!swapOk)
		{
			DE_ASSERT(badWindow);
			throw tcu::ResourceError(string("eglSwapBuffers() failed: ") + getErrorStr(error).toString());
		}

		// Refresh dimensions
		{
			int	newWidth	= 0;
			int	newHeight	= 0;

			eglQuerySurface(m_eglDisplay, m_eglSurface, EGL_WIDTH,	&newWidth);
			eglQuerySurface(m_eglDisplay, m_eglSurface, EGL_HEIGHT,	&newHeight);
			EGLU_CHECK_MSG("Failed to query window size");

			if (newWidth	!= m_glRenderTarget.getWidth() ||
				newHeight	!= m_glRenderTarget.getHeight())
			{
				tcu::print("Warning: Window size changed (%dx%d -> %dx%d), test results might be invalid!\n",
						   m_glRenderTarget.getWidth(), m_glRenderTarget.getHeight(), newWidth, newHeight);

				m_glRenderTarget = tcu::RenderTarget(newWidth, newHeight,
													 m_glRenderTarget.getPixelFormat(),
													 m_glRenderTarget.getDepthBits(),
													 m_glRenderTarget.getStencilBits(),
													 m_glRenderTarget.getNumSamples());
			}
		}
	}
	else
	{
		// \todo [2014-05-02 mika] Should we call flush or finish? Old platform uses finish() but flush() is closer to the behaviour of eglSwapBuffers()
		m_glFunctions.flush();
		GLU_EXPECT_NO_ERROR(m_glFunctions.getError(), "glFlush()");
	}
}

} // anonymous

GLContextFactory::GLContextFactory (const NativeDisplayFactoryRegistry& displayFactoryRegistry)
	: glu::ContextFactory		("egl", "EGL OpenGL Context")
	, m_displayFactoryRegistry	(displayFactoryRegistry)
{
}

namespace
{

template<typename Factory>
const Factory* selectFactory (const tcu::FactoryRegistry<Factory>& registry, const char* objectTypeName, const char* cmdLineArg)
{
	if (cmdLineArg)
	{
		const Factory* factory = registry.getFactoryByName(cmdLineArg);

		if (factory)
			return factory;
		else
		{
			tcu::print("ERROR: Unknown or unsupported EGL %s type '%s'", objectTypeName, cmdLineArg);
			tcu::print("Available EGL %s types:\n", objectTypeName);
			for (size_t ndx = 0; ndx < registry.getFactoryCount(); ndx++)
				tcu::print("  %s: %s\n", registry.getFactoryByIndex(ndx)->getName(), registry.getFactoryByIndex(ndx)->getDescription());

			throw tcu::NotSupportedError((string("Unsupported or unknown EGL ") + objectTypeName + " type '" + cmdLineArg + "'").c_str(), DE_NULL, __FILE__, __LINE__);
		}
	}
	else if (!registry.empty())
		return registry.getDefaultFactory();
	else
		return DE_NULL;
}

} // anonymous

glu::RenderContext* GLContextFactory::createContext (const glu::RenderConfig& config, const tcu::CommandLine& cmdLine) const
{
	const NativeDisplayFactory* displayFactory = selectFactory(m_displayFactoryRegistry, "display", cmdLine.getEGLDisplayType());

	if (displayFactory)
	{
		// \note windowFactory & pixmapFactory are not mandatory
		const NativeWindowFactory*	windowFactory	= selectFactory(displayFactory->getNativeWindowRegistry(), "window", cmdLine.getEGLWindowType());
		const NativePixmapFactory*	pixmapFactory	= selectFactory(displayFactory->getNativePixmapRegistry(), "pixmap", cmdLine.getEGLPixmapType());

		return new RenderContext(displayFactory, windowFactory, pixmapFactory, config);
	}
	else
		throw tcu::NotSupportedError("No EGL displays available", DE_NULL, __FILE__, __LINE__);
}

} // eglu
