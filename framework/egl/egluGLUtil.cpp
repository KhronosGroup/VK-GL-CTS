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
 * \brief EGL utilities for interfacing with GL APIs.
 *//*--------------------------------------------------------------------*/

#include "egluGLUtil.hpp"

#include "egluUtil.hpp"
#include "eglwLibrary.hpp"
#include "eglwEnums.hpp"
#include "glwEnums.hpp"

#include <vector>

using std::vector;

namespace eglu
{

using namespace eglw;

glw::GLenum getImageGLTarget (EGLenum source)
{
	switch (source)
	{
		case EGL_GL_TEXTURE_2D_KHR:						return GL_TEXTURE_2D;
		case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR:	return GL_TEXTURE_CUBE_MAP_POSITIVE_X;
		case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y_KHR:	return GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
		case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z_KHR:	return GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
		case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X_KHR:	return GL_TEXTURE_CUBE_MAP_NEGATIVE_X;
		case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_KHR:	return GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
		case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_KHR:	return GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
		case EGL_GL_TEXTURE_3D_KHR:						return GL_TEXTURE_3D;
		case EGL_GL_RENDERBUFFER_KHR:					return GL_RENDERBUFFER;
		default:	DE_ASSERT(!"Impossible");			return GL_NONE;
	}
}

EGLint apiRenderableType (glu::ApiType apiType)
{
	switch (apiType.getProfile())
	{
		case glu::PROFILE_CORE:
		case glu::PROFILE_COMPATIBILITY:
			return EGL_OPENGL_BIT;
		case glu::PROFILE_ES:
			switch (apiType.getMajorVersion())
			{
				case 1:		return EGL_OPENGL_ES_BIT;
				case 2:		return EGL_OPENGL_ES2_BIT;
				case 3:		return EGL_OPENGL_ES3_BIT_KHR;
				default:	DE_ASSERT(!"Unknown OpenGL ES version");
			}
		default:
			DE_ASSERT(!"Unknown GL API");
	}

	return 0;
}

EGLContext createGLContext (const Library& egl, EGLDisplay display, EGLContext eglConfig, const glu::ContextType& contextType)
{
	const bool			khrCreateContextSupported	= hasExtension(egl, display, "EGL_KHR_create_context");
	EGLContext			context						= EGL_NO_CONTEXT;
	EGLenum				api							= EGL_NONE;
	vector<EGLint>		attribList;

	if (glu::isContextTypeES(contextType))
	{
		api = EGL_OPENGL_ES_API;

		if (contextType.getMajorVersion() <= 2)
		{
			attribList.push_back(EGL_CONTEXT_CLIENT_VERSION);
			attribList.push_back(contextType.getMajorVersion());
		}
		else
		{
			if (!khrCreateContextSupported)
				throw tcu::NotSupportedError("EGL_KHR_create_context is required for OpenGL ES 3.0 and newer", DE_NULL, __FILE__, __LINE__);

			attribList.push_back(EGL_CONTEXT_MAJOR_VERSION_KHR);
			attribList.push_back(contextType.getMajorVersion());
			attribList.push_back(EGL_CONTEXT_MINOR_VERSION_KHR);
			attribList.push_back(contextType.getMinorVersion());
		}
	}
	else
	{
		DE_ASSERT(glu::isContextTypeGLCore(contextType) || glu::isContextTypeGLCompatibility(contextType));

		if (!khrCreateContextSupported)
			throw tcu::NotSupportedError("EGL_KHR_create_context is required for OpenGL context creation", DE_NULL, __FILE__, __LINE__);

		api = EGL_OPENGL_API;

		attribList.push_back(EGL_CONTEXT_MAJOR_VERSION_KHR);
		attribList.push_back(contextType.getMajorVersion());
		attribList.push_back(EGL_CONTEXT_MINOR_VERSION_KHR);
		attribList.push_back(contextType.getMinorVersion());
		attribList.push_back(EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR);
		attribList.push_back(glu::isContextTypeGLCore(contextType) ? EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR
																   : EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR);
	}

	if (contextType.getFlags() != glu::ContextFlags(0))
	{
		EGLint flags = 0;

		if (!khrCreateContextSupported)
			throw tcu::NotSupportedError("EGL_KHR_create_context is required for creating robust/debug/forward-compatible contexts");

		if ((contextType.getFlags() & glu::CONTEXT_DEBUG) != 0)
			flags |= EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR;

		if ((contextType.getFlags() & glu::CONTEXT_ROBUST) != 0)
			flags |= EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR;

		if ((contextType.getFlags() & glu::CONTEXT_FORWARD_COMPATIBLE) != 0)
		{
			if (!glu::isContextTypeGLCore(contextType))
				throw tcu::NotSupportedError("Only OpenGL core contexts can be forward-compatible");

			flags |= EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR;
		}

		attribList.push_back(EGL_CONTEXT_FLAGS_KHR);
		attribList.push_back(flags);
	}

	attribList.push_back(EGL_NONE);

	EGLU_CHECK_CALL(egl, bindAPI(api));
	context = egl.createContext(display, eglConfig, EGL_NO_CONTEXT, &(attribList[0]));
	EGLU_CHECK_MSG(egl, "eglCreateContext()");

	return context;
}

static bool configMatches (const eglw::Library& egl, eglw::EGLDisplay display, eglw::EGLConfig eglConfig, const glu::RenderConfig& renderConfig)
{
	// \todo [2014-03-12 pyry] Check other attributes like double-buffer bit.

	{
		EGLint		renderableType		= 0;
		EGLint		requiredRenderable	= apiRenderableType(renderConfig.type.getAPI());

		EGLU_CHECK_CALL(egl, getConfigAttrib(display, eglConfig, EGL_RENDERABLE_TYPE, &renderableType));

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

		EGLU_CHECK_CALL(egl, getConfigAttrib(display, eglConfig, EGL_SURFACE_TYPE, &surfaceType));

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
				EGLU_CHECK_CALL(egl, getConfigAttrib(display, eglConfig, s_attribs[attribNdx].attrib, &value));
				if (value != renderConfig.*s_attribs[attribNdx].field)
					return false;
			}
		}
	}

	return true;
}

EGLConfig chooseConfig (const Library& egl, EGLDisplay display, const glu::RenderConfig& config)
{
	const std::vector<EGLConfig> configs = eglu::getConfigs(egl, display);

	for (vector<EGLConfig>::const_iterator iter = configs.begin(); iter != configs.end(); ++iter)
	{
		if (configMatches(egl, display, *iter, config))
			return *iter;
	}

	throw tcu::NotSupportedError("Matching EGL config not found", DE_NULL, __FILE__, __LINE__);
}

}
