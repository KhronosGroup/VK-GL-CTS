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
 * \brief Win32 GLES3 wrapper platform.
 *//*--------------------------------------------------------------------*/

#include "tcuWin32GLES3Platform.hpp"
#include "gluRenderConfig.hpp"
#include "gluRenderContext.hpp"
#include "tcuRenderTarget.hpp"
#include "gl3Context.h"
#include "glwInitES30Direct.hpp"

struct gl3PlatformContext_s
{
	tcu::wgl::Context*	context;

	gl3PlatformContext_s (void)
		: context(DE_NULL)
	{
	}
};

gl3FunctionPtr gl3PlatformContext_getProcAddress (gl3PlatformContext* platformCtx, const char* procName)
{
	DE_ASSERT(platformCtx && platformCtx->context);
	return platformCtx->context->getGLFunction(procName);
}

namespace tcu
{

enum
{
	DEFAULT_WINDOW_WIDTH	= 400,
	DEFAULT_WINDOW_HEIGHT	= 300
};

// Win32GLES3Context

class Win32GLES3Context : public glu::RenderContext
{
public:
									Win32GLES3Context			(const wgl::Core& wgl, HINSTANCE instance, const glu::RenderConfig& config);
									~Win32GLES3Context			(void);

	glu::ContextType				getType						(void) const	{ return glu::CONTEXTTYPE_ES3;	}
	const RenderTarget&				getRenderTarget				(void) const	{ return m_renderTarget;		}
	void							postIterate					(void);
	const glw::Functions&			getFunctions				(void) const	{ return m_functions;			}

private:
									Win32GLES3Context			(const Win32GLES3Context& other);
	Win32GLES3Context&				operator=					(const Win32GLES3Context& other);

	RenderTarget					m_renderTarget;
	Win32Window						m_window;

	gl3PlatformContext				m_platformCtx;
	gl3Context*						m_context;

	glw::Functions					m_functions;
};

typedef const char* (GL_APIENTRY* glGetStringHackFunc) (GLenum str);

Win32GLES3Context::Win32GLES3Context (const wgl::Core& wgl, HINSTANCE instance, const glu::RenderConfig& config)
	: m_renderTarget(config.width	!= glu::RenderConfig::DONT_CARE ? config.width	: DEFAULT_WINDOW_WIDTH,
					 config.height	!= glu::RenderConfig::DONT_CARE ? config.height	: DEFAULT_WINDOW_HEIGHT,
					 PixelFormat(8, 8, 8, 8), 24, 8, 0)
	, m_window		(instance, m_renderTarget.getWidth(), m_renderTarget.getHeight())
	, m_context		(DE_NULL)
{
	const HDC		deviceCtx		= m_window.getDeviceContext();
	const int		pixelFormat		= wgl::choosePixelFormat(wgl, deviceCtx, config);

	if (pixelFormat < 0)
		throw NotSupportedError("No compatible WGL pixel format found");

	m_platformCtx.context = new wgl::Context(&wgl, m_window.getDeviceContext(), wgl::PROFILE_COMPATIBILITY, 3, 3, pixelFormat);

	try
	{
		m_context = gl3Context_create(&m_platformCtx);
		if (!m_context)
			throw ResourceError("Failed to create GLES3 wrapper context");

		gl3Context_setCurrentContext(m_context);
		glw::initES30Direct(&m_functions);

		m_window.setVisible(config.windowVisibility != glu::RenderConfig::VISIBILITY_HIDDEN);

		{
			const wgl::PixelFormatInfo	info	= wgl.getPixelFormatInfo(deviceCtx, pixelFormat);
			const IVec2					size	= m_window.getSize();

			m_renderTarget = tcu::RenderTarget(size.x(), size.y(),
											   tcu::PixelFormat(info.redBits, info.greenBits, info.blueBits, info.alphaBits),
											   info.depthBits, info.stencilBits,
											   info.sampleBuffers ? info.samples : 0);
		}
	}
	catch (...)
	{
		if (m_context)
			gl3Context_destroy(m_context);
		delete m_platformCtx.context;
		throw;
	}
}

Win32GLES3Context::~Win32GLES3Context (void)
{
	if (m_context)
		gl3Context_destroy(m_context);

	delete m_platformCtx.context;
}

void Win32GLES3Context::postIterate (void)
{
	m_platformCtx.context->swapBuffers();
}

// Win32GLES3ContextFactory

class Win32GLES3ContextFactory : public glu::ContextFactory
{
public:
								Win32GLES3ContextFactory	(HINSTANCE instance);
								~Win32GLES3ContextFactory	(void);

	virtual glu::RenderContext*	createContext				(const glu::RenderConfig& config, const tcu::CommandLine& cmdLine) const;

private:
	const HINSTANCE				m_instance;
	wgl::Core					m_wglCore;
};

Win32GLES3ContextFactory::Win32GLES3ContextFactory (HINSTANCE instance)
	: glu::ContextFactory	("gles3_wrapper",	"GLES3 Wrapper Context")
	, m_instance			(instance)
	, m_wglCore				(instance)
{
}

Win32GLES3ContextFactory::~Win32GLES3ContextFactory (void)
{
}

glu::RenderContext* Win32GLES3ContextFactory::createContext (const glu::RenderConfig& config, const tcu::CommandLine&) const
{
	if (config.type == glu::CONTEXTTYPE_ES3)
		return new Win32GLES3Context(m_wglCore, m_instance, config);
	else
		throw NotSupportedError("Unsupported rendering context type");
}

// Win32GLES3Platform

Win32GLES3Platform::Win32GLES3Platform (void)
{
	const HINSTANCE instance = GetModuleHandle(NULL);

	// Set priority to lower.
	SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);

	m_glContextFactoryRegistry.registerFactory(new Win32GLES3ContextFactory(instance));
}

Win32GLES3Platform::~Win32GLES3Platform (void)
{
}

bool Win32GLES3Platform::processEvents (void)
{
	MSG msg;
	while (PeekMessage(&msg, NULL, 0, 0, TRUE))
	{
		DispatchMessage(&msg);
		if (msg.message == WM_QUIT)
			return false;
	}
	return true;
}

} // tcu

// Platform factory
tcu::Platform* createPlatform (void)
{
	return new tcu::Win32GLES3Platform();
}
