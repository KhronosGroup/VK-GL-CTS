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
 * \brief Surface query tests.
 *//*--------------------------------------------------------------------*/

#include "teglQuerySurfaceTests.hpp"

#include "teglSimpleConfigCase.hpp"

#include "egluNativeDisplay.hpp"
#include "egluNativeWindow.hpp"
#include "egluNativePixmap.hpp"
#include "egluStrUtil.hpp"
#include "egluUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuTestContext.hpp"
#include "tcuCommandLine.hpp"

#include "deUniquePtr.hpp"
#include "deRandom.hpp"

#include <string>
#include <vector>

namespace deqp
{
namespace egl
{

using eglu::ConfigInfo;
using tcu::TestLog;

static void logSurfaceAttribute (tcu::TestLog& log, EGLint attribute, EGLint value)
{
	const char*								name		= eglu::getSurfaceAttribName(attribute);
	const eglu::SurfaceAttribValueFmt		valueFmt	(attribute, value);

	log << TestLog::Message << "  " << name << ": " << valueFmt << TestLog::EndMessage;
}

static void logSurfaceAttributes (tcu::TestLog& log, const tcu::egl::Surface& surface, const EGLint* attributes, int num)
{
	for (int ndx = 0; ndx < num; ndx++)
	{
		const EGLint	attrib	= attributes[ndx];

		logSurfaceAttribute(log, attrib, surface.getAttribute(attrib));
	}
}

static void logCommonSurfaceAttributes (tcu::TestLog& log, const tcu::egl::Surface& surface)
{
	static const EGLint	attributes[] =
	{
		EGL_CONFIG_ID,
		EGL_WIDTH,
		EGL_HEIGHT,
		EGL_HORIZONTAL_RESOLUTION,
		EGL_VERTICAL_RESOLUTION,
		EGL_MULTISAMPLE_RESOLVE,
		EGL_PIXEL_ASPECT_RATIO,
		EGL_RENDER_BUFFER,
		EGL_SWAP_BEHAVIOR,
		EGL_VG_ALPHA_FORMAT,
		EGL_VG_COLORSPACE
	};

	logSurfaceAttributes(log, surface, attributes, DE_LENGTH_OF_ARRAY(attributes));
}

static void logPbufferSurfaceAttributes (tcu::TestLog& log, const tcu::egl::Surface& surface)
{
	static const EGLint	attributes[] = {
		EGL_LARGEST_PBUFFER,
		EGL_TEXTURE_FORMAT,
		EGL_TEXTURE_TARGET,
		EGL_MIPMAP_TEXTURE,
		EGL_MIPMAP_LEVEL,
	};

	logSurfaceAttributes(log, surface, attributes, DE_LENGTH_OF_ARRAY(attributes));
}

class QuerySurfaceCase : public SimpleConfigCase
{
public:
	QuerySurfaceCase (EglTestContext& eglTestCtx, const char* name, const char* description, const std::vector<EGLint>& configIds);

	void checkCommonAttributes (const tcu::egl::Surface& surface, const ConfigInfo& info);
	void checkNonPbufferAttributes (EGLDisplay display, const tcu::egl::Surface& surface);
};

QuerySurfaceCase::QuerySurfaceCase (EglTestContext& eglTestCtx, const char* name, const char* description, const std::vector<EGLint>& configIds)
	: SimpleConfigCase(eglTestCtx, name, description, configIds)
{
}

void QuerySurfaceCase::checkCommonAttributes (const tcu::egl::Surface& surface, const ConfigInfo& info)
{
	tcu::TestLog&	log		= m_testCtx.getLog();

	// Attributes which are common to all surface types

	// Config ID
	{
		const EGLint	id	= surface.getAttribute(EGL_CONFIG_ID);

		if (id != info.configId)
		{
			log << TestLog::Message << "    Fail, config ID " << id << " does not match the one used to create the surface" << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Config ID mismatch");
		}
	}

	// Width and height
	{
		const EGLint	width	= surface.getWidth();
		const EGLint	height	= surface.getHeight();

		if (width <= 0 || height <= 0)
		{
			log << TestLog::Message << "    Fail, invalid surface size " << width << "x" << height << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid surface size");
		}
	}

	// Horizontal and vertical resolution
	{
		const EGLint	hRes	= surface.getAttribute(EGL_HORIZONTAL_RESOLUTION);
		const EGLint	vRes	= surface.getAttribute(EGL_VERTICAL_RESOLUTION);

		if ((hRes <= 0 || vRes <= 0) && (hRes != EGL_UNKNOWN && vRes != EGL_UNKNOWN))
		{
			log << TestLog::Message << "    Fail, invalid surface resolution " << hRes << "x" << vRes << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid surface resolution");
		}
	}

	// Pixel aspect ratio
	{
		const EGLint	pixelRatio	= surface.getAttribute(EGL_PIXEL_ASPECT_RATIO);

		if (pixelRatio <= 0 && pixelRatio != EGL_UNKNOWN)
		{
			log << TestLog::Message << "    Fail, invalid pixel aspect ratio " << surface.getAttribute(EGL_PIXEL_ASPECT_RATIO) << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid pixel aspect ratio");
		}
	}

	// Render buffer
	{
		const EGLint	renderBuffer	= surface.getAttribute(EGL_RENDER_BUFFER);

		if (renderBuffer != EGL_BACK_BUFFER && renderBuffer != EGL_SINGLE_BUFFER)
		{
			log << TestLog::Message << "    Fail, invalid render buffer value " << renderBuffer << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid render buffer");
		}
	}

	// Multisample resolve
	{
		const EGLint	multisampleResolve	= surface.getAttribute(EGL_MULTISAMPLE_RESOLVE);

		if (multisampleResolve != EGL_MULTISAMPLE_RESOLVE_DEFAULT && multisampleResolve != EGL_MULTISAMPLE_RESOLVE_BOX)
		{
			log << TestLog::Message << "    Fail, invalid multisample resolve value " << multisampleResolve << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid multisample resolve");
		}

		if (multisampleResolve == EGL_MULTISAMPLE_RESOLVE_BOX && !(info.surfaceType & EGL_MULTISAMPLE_RESOLVE_BOX_BIT))
		{
			log << TestLog::Message << "    Fail, multisample resolve is reported as box filter but configuration does not support it." << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid multisample resolve");
		}
	}

	// Swap behavior
	{
		const EGLint	swapBehavior	= surface.getAttribute(EGL_SWAP_BEHAVIOR);

		if (swapBehavior != EGL_BUFFER_DESTROYED && swapBehavior != EGL_BUFFER_PRESERVED)
		{
			log << TestLog::Message << "    Fail, invalid swap behavior value " << swapBehavior << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid swap behavior");
		}

		if (swapBehavior == EGL_BUFFER_PRESERVED && !(info.surfaceType & EGL_SWAP_BEHAVIOR_PRESERVED_BIT))
		{
			log << TestLog::Message << "    Fail, swap behavior is reported as preserve but configuration does not support it." << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid swap behavior");
		}
	}

	// OpenVG alpha format
	{
		const EGLint	vgAlphaFormat	= surface.getAttribute(EGL_VG_ALPHA_FORMAT);

		if (vgAlphaFormat != EGL_VG_ALPHA_FORMAT_NONPRE && vgAlphaFormat != EGL_VG_ALPHA_FORMAT_PRE)
		{
			log << TestLog::Message << "    Fail, invalid OpenVG alpha format value " << vgAlphaFormat << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid OpenVG alpha format");
		}

		if (vgAlphaFormat == EGL_VG_ALPHA_FORMAT_PRE && !(info.surfaceType & EGL_VG_ALPHA_FORMAT_PRE_BIT))
		{
			log << TestLog::Message << "    Fail, OpenVG is set to use premultiplied alpha but configuration does not support it." << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid OpenVG alpha format");
		}
	}

	// OpenVG color space
	{
		const EGLint	vgColorspace	= surface.getAttribute(EGL_VG_COLORSPACE);

		if (vgColorspace != EGL_VG_COLORSPACE_sRGB && vgColorspace != EGL_VG_COLORSPACE_LINEAR)
		{
			log << TestLog::Message << "    Fail, invalid OpenVG color space value " << vgColorspace << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid OpenVG color space");
		}

		if (vgColorspace == EGL_VG_COLORSPACE_LINEAR && !(info.surfaceType & EGL_VG_COLORSPACE_LINEAR_BIT))
		{
			log << TestLog::Message << "    Fail, OpenVG is set to use a linear color space but configuration does not support it." << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid OpenVG color space");
		}
	}
}

void QuerySurfaceCase::checkNonPbufferAttributes (EGLDisplay display, const tcu::egl::Surface& surface)
{
	const EGLint	uninitializedMagicValue	= -42;
	tcu::TestLog&	log						= m_testCtx.getLog();
	EGLint			value					= uninitializedMagicValue;

	static const EGLint pbufferAttribs[] = {
		EGL_LARGEST_PBUFFER,
		EGL_TEXTURE_FORMAT,
		EGL_TEXTURE_TARGET,
		EGL_MIPMAP_TEXTURE,
		EGL_MIPMAP_LEVEL,
	};

	for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(pbufferAttribs); ndx++)
	{
		const EGLint		attribute	= pbufferAttribs[ndx];
		const std::string	name		= eglu::getSurfaceAttribName(pbufferAttribs[ndx]);

		eglQuerySurface(display, surface.getEGLSurface(), attribute, &value);

		{
			const EGLint	error	= eglGetError();

			if (error != EGL_SUCCESS)
			{
				log << TestLog::Message << "    Fail, querying " << name << " from a non-pbuffer surface should not result in an error, received "
					<< eglu::getErrorStr(error) << TestLog::EndMessage;
				m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Illegal error condition");
			}

			break;
		}

		// "For a window or pixmap surface, the contents of value are not modified."
		if (value != uninitializedMagicValue)
		{
			log << TestLog::Message << "    Fail, return value contents were modified when querying " << name << " from a non-pbuffer surface." << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Illegal modification of return value");
		}
	}
}

class QuerySurfaceSimpleWindowCase : public QuerySurfaceCase
{
public:
	QuerySurfaceSimpleWindowCase (EglTestContext& eglTestCtx, const char* name, const char* description, const std::vector<EGLint>& configIds)
		: QuerySurfaceCase(eglTestCtx, name, description, configIds)
	{
	}

	void executeForConfig (tcu::egl::Display& display, EGLConfig config)
	{
		tcu::TestLog&	log		= m_testCtx.getLog();
		const int		width	= 64;
		const int		height	= 64;

		ConfigInfo		info;
		display.describeConfig(config, info);

		log << TestLog::Message << "Creating window surface with config ID " << info.configId << TestLog::EndMessage;
		TCU_CHECK_EGL();

		de::UniquePtr<eglu::NativeWindow>	window(m_eglTestCtx.createNativeWindow(display.getEGLDisplay(), config, DE_NULL, width, height, eglu::parseWindowVisibility(m_testCtx.getCommandLine())));
		tcu::egl::WindowSurface				surface(display, eglu::createWindowSurface(m_eglTestCtx.getNativeDisplay(), *window, display.getEGLDisplay(), config, DE_NULL));

		logCommonSurfaceAttributes(log, surface);

		checkCommonAttributes(surface, info);
		checkNonPbufferAttributes(display.getEGLDisplay(), surface);
	}
};

class QuerySurfaceSimplePixmapCase : public QuerySurfaceCase
{
public:
	QuerySurfaceSimplePixmapCase (EglTestContext& eglTestCtx, const char* name, const char* description, const std::vector<EGLint>& configIds)
		: QuerySurfaceCase(eglTestCtx, name, description, configIds)
	{
	}

	void executeForConfig (tcu::egl::Display& display, EGLConfig config)
	{
		tcu::TestLog&	log		= m_testCtx.getLog();
		const int		width	= 64;
		const int		height	= 64;

		ConfigInfo		info;
		display.describeConfig(config, info);

		log << TestLog::Message << "Creating pixmap surface with config ID " << info.configId << TestLog::EndMessage;
		TCU_CHECK_EGL();

		de::UniquePtr<eglu::NativePixmap>	pixmap	(m_eglTestCtx.createNativePixmap(display.getEGLDisplay(), config, DE_NULL, width, height));
		tcu::egl::PixmapSurface				surface	(display, eglu::createPixmapSurface(m_eglTestCtx.getNativeDisplay(), *pixmap, display.getEGLDisplay(), config, DE_NULL));

		logCommonSurfaceAttributes(log, surface);

		checkCommonAttributes(surface, info);
		checkNonPbufferAttributes(display.getEGLDisplay(), surface);
	}
};

class QuerySurfaceSimplePbufferCase : public QuerySurfaceCase
{
public:
	QuerySurfaceSimplePbufferCase (EglTestContext& eglTestCtx, const char* name, const char* description, const std::vector<EGLint>& configIds)
		: QuerySurfaceCase(eglTestCtx, name, description, configIds)
	{
	}

	void executeForConfig (tcu::egl::Display& display, EGLConfig config)
	{
		tcu::TestLog&	log		= m_testCtx.getLog();
		int				width	= 64;
		int				height	= 64;

		ConfigInfo		info;
		display.describeConfig(config, info);

		log << TestLog::Message << "Creating pbuffer surface with config ID " << info.configId << TestLog::EndMessage;
		TCU_CHECK_EGL();

		// Clamp to maximums reported by implementation
		width	= deMin32(width, display.getConfigAttrib(config, EGL_MAX_PBUFFER_WIDTH));
		height	= deMin32(height, display.getConfigAttrib(config, EGL_MAX_PBUFFER_HEIGHT));

		if (width == 0 || height == 0)
		{
			log << TestLog::Message << "    Fail, maximum pbuffer size of " << width << "x" << height << " reported" << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid maximum pbuffer size");
			return;
		}

		const EGLint attribs[] =
		{
			EGL_WIDTH,			width,
			EGL_HEIGHT,			height,
			EGL_TEXTURE_FORMAT,	EGL_NO_TEXTURE,
			EGL_NONE
		};

		{
			tcu::egl::PbufferSurface surface(display, config, attribs);

			logCommonSurfaceAttributes(log, surface);
			logPbufferSurfaceAttributes(log, surface);

			checkCommonAttributes(surface, info);

			// Pbuffer-specific attributes

			// Largest pbuffer
			{
				const EGLint	largestPbuffer	= surface.getAttribute(EGL_LARGEST_PBUFFER);

				if (largestPbuffer != EGL_FALSE && largestPbuffer != EGL_TRUE)
				{
					log << TestLog::Message << "    Fail, invalid largest pbuffer value " << largestPbuffer << TestLog::EndMessage;
					m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid largest pbuffer");
				}
			}

			// Texture format
			{
				const EGLint	textureFormat	= surface.getAttribute(EGL_TEXTURE_FORMAT);

				if (textureFormat != EGL_NO_TEXTURE && textureFormat != EGL_TEXTURE_RGB && textureFormat != EGL_TEXTURE_RGBA)
				{
					log << TestLog::Message << "    Fail, invalid texture format value " << textureFormat << TestLog::EndMessage;
					m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid texture format");
				}
			}

			// Texture target
			{
				const EGLint	textureTarget	= surface.getAttribute(EGL_TEXTURE_TARGET);

				if (textureTarget != EGL_NO_TEXTURE && textureTarget != EGL_TEXTURE_2D)
				{
					log << TestLog::Message << "    Fail, invalid texture target value " << textureTarget << TestLog::EndMessage;
					m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid texture target");
				}
			}

			// Mipmap texture
			{
				const EGLint	mipmapTexture	= surface.getAttribute(EGL_MIPMAP_TEXTURE);

				if (mipmapTexture != EGL_FALSE && mipmapTexture != EGL_TRUE)
				{
					log << TestLog::Message << "    Fail, invalid mipmap texture value " << mipmapTexture << TestLog::EndMessage;
					m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid mipmap texture");
				}
			}
		}
	}
};

class SurfaceAttribCase : public SimpleConfigCase
{
public:
			SurfaceAttribCase	(EglTestContext& eglTestCtx, const char* name, const char* description, const std::vector<EGLint>& configIds);
	virtual	~SurfaceAttribCase	(void) {}

	void	testAttributes		(tcu::egl::Surface& surface, const ConfigInfo& info);
};

SurfaceAttribCase::SurfaceAttribCase (EglTestContext& eglTestCtx, const char* name, const char* description, const std::vector<EGLint>& configIds)
		: SimpleConfigCase(eglTestCtx, name, description, configIds)
{
}

void SurfaceAttribCase::testAttributes (tcu::egl::Surface& surface, const ConfigInfo& info)
{
	const tcu::egl::Display&	display			= surface.getDisplay();
	tcu::TestLog&				log				= m_testCtx.getLog();
	const int					majorVersion	= display.getEGLMajorVersion();
	const int					minorVersion	= display.getEGLMinorVersion();
	de::Random					rnd				(deStringHash(m_name.c_str()) ^ 0xf215918f);

	if (majorVersion == 1 && minorVersion == 0)
	{
		log << TestLog::Message << "No attributes can be set in EGL 1.0" << TestLog::EndMessage;
		return;
	}

	// Mipmap level
	if (info.renderableType & EGL_OPENGL_ES_BIT || info.renderableType & EGL_OPENGL_ES2_BIT)
	{
		const EGLint initialValue = 0xDEADBAAD;
		EGLint value = initialValue;

		TCU_CHECK_EGL_CALL(eglQuerySurface(surface.getDisplay().getEGLDisplay(), surface.getEGLSurface(), EGL_MIPMAP_LEVEL, &value));

		logSurfaceAttribute(log, EGL_MIPMAP_LEVEL, value);

		if (dynamic_cast<tcu::egl::PbufferSurface*>(&surface))
		{
			if (value != 0)
			{
				log << TestLog::Message << "    Fail, initial mipmap level value should be 0, is " << value << TestLog::EndMessage;
				m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid default mipmap level");
			}
		}
		else if (value != initialValue)
		{
			log << TestLog::Message << "    Fail, eglQuerySurface changed value when querying EGL_MIPMAP_LEVEL for non-pbuffer surface. Result: " << value << ". Expected: " << initialValue << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "EGL_MIPMAP_LEVEL query modified result for non-pbuffer surface.");
		}

		eglSurfaceAttrib(display.getEGLDisplay(), surface.getEGLSurface(), EGL_MIPMAP_LEVEL, 1);

		{
			const EGLint	error	= eglGetError();

			if (error != EGL_SUCCESS)
			{
				log << TestLog::Message << "    Fail, setting EGL_MIPMAP_LEVEL should not result in an error, received " << eglu::getErrorStr(error) << TestLog::EndMessage;

				m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Illegal error condition");
			}
		}
	}

	// Only mipmap level can be set in EGL 1.3 and lower
	if (majorVersion == 1 && minorVersion <= 3) return;

	// Multisample resolve
	{
		const EGLint	value	= surface.getAttribute(EGL_MULTISAMPLE_RESOLVE);

		logSurfaceAttribute(log, EGL_MULTISAMPLE_RESOLVE, value);

		if (value != EGL_MULTISAMPLE_RESOLVE_DEFAULT)
		{
			log << TestLog::Message << "    Fail, initial multisample resolve value should be EGL_MULTISAMPLE_RESOLVE_DEFAULT, is "
				<< eglu::getSurfaceAttribValueStr(EGL_MULTISAMPLE_RESOLVE, value) << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid default multisample resolve");
		}

		if (info.renderableType & EGL_MULTISAMPLE_RESOLVE_BOX_BIT)
		{
			log << TestLog::Message << "    Box filter is supported by surface, trying to set." << TestLog::EndMessage;

			surface.setAttribute(EGL_MULTISAMPLE_RESOLVE, EGL_MULTISAMPLE_RESOLVE_BOX);

			if (surface.getAttribute(EGL_MULTISAMPLE_RESOLVE) != EGL_MULTISAMPLE_RESOLVE_BOX)
			{
				log << TestLog::Message << "    Fail, tried to enable box filter but value did not change.";
				m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Failed to set multisample resolve");
			}
		}
	}

	// Swap behavior
	{
		const EGLint	value	= surface.getAttribute(EGL_SWAP_BEHAVIOR);

		logSurfaceAttribute(log, EGL_SWAP_BEHAVIOR, value);

		if (info.renderableType & EGL_SWAP_BEHAVIOR_PRESERVED_BIT)
		{
			const EGLint	nextValue	= (value == EGL_BUFFER_DESTROYED) ? EGL_BUFFER_PRESERVED : EGL_BUFFER_DESTROYED;

			surface.setAttribute(EGL_SWAP_BEHAVIOR, nextValue);

			if (surface.getAttribute(EGL_SWAP_BEHAVIOR) != nextValue)
			{
				log << TestLog::Message << "  Fail, tried to set swap behavior to " << eglu::getSurfaceAttribStr(nextValue) << TestLog::EndMessage;
				m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Failed to set swap behavior");
			}
		}
	}
}

class SurfaceAttribWindowCase : public SurfaceAttribCase
{
public:
	SurfaceAttribWindowCase (EglTestContext& eglTestCtx, const char* name, const char* description, const std::vector<EGLint>& configIds)
		: SurfaceAttribCase(eglTestCtx, name, description, configIds)
	{
	}

	void executeForConfig (tcu::egl::Display& display, EGLConfig config)
	{
		tcu::TestLog&	log		= m_testCtx.getLog();
		const int		width	= 64;
		const int		height	= 64;

		ConfigInfo		info;
		display.describeConfig(config, info);

		log << TestLog::Message << "Creating window surface with config ID " << info.configId << TestLog::EndMessage;
		TCU_CHECK_EGL();

		de::UniquePtr<eglu::NativeWindow>	window(m_eglTestCtx.createNativeWindow(display.getEGLDisplay(), config, DE_NULL, width, height, eglu::parseWindowVisibility(m_testCtx.getCommandLine())));
		tcu::egl::WindowSurface				surface(display, eglu::createWindowSurface(m_eglTestCtx.getNativeDisplay(), *window, display.getEGLDisplay(), config, DE_NULL));

		testAttributes(surface, info);
	}
};

class SurfaceAttribPixmapCase : public SurfaceAttribCase
{
public:
	SurfaceAttribPixmapCase (EglTestContext& eglTestCtx, const char* name, const char* description, const std::vector<EGLint>& configIds)
		: SurfaceAttribCase(eglTestCtx, name, description, configIds)
	{
	}

	void executeForConfig (tcu::egl::Display& display, EGLConfig config)
	{
		tcu::TestLog&	log		= m_testCtx.getLog();
		const int		width	= 64;
		const int		height	= 64;

		ConfigInfo		info;
		display.describeConfig(config, info);

		log << TestLog::Message << "Creating pixmap surface with config ID " << info.configId << TestLog::EndMessage;
		TCU_CHECK_EGL();

		de::UniquePtr<eglu::NativePixmap>	pixmap	(m_eglTestCtx.createNativePixmap(display.getEGLDisplay(), config, DE_NULL, width, height));
		tcu::egl::PixmapSurface				surface	(display, eglu::createPixmapSurface(m_eglTestCtx.getNativeDisplay(), *pixmap, display.getEGLDisplay(), config, DE_NULL));

		testAttributes(surface, info);
	}
};

class SurfaceAttribPbufferCase : public SurfaceAttribCase
{
public:
	SurfaceAttribPbufferCase (EglTestContext& eglTestCtx, const char* name, const char* description, const std::vector<EGLint>& configIds)
		: SurfaceAttribCase(eglTestCtx, name, description, configIds)
	{
	}

	void executeForConfig (tcu::egl::Display& display, EGLConfig config)
	{
		tcu::TestLog&	log		= m_testCtx.getLog();
		int				width	= 64;
		int				height	= 64;

		ConfigInfo		info;
		display.describeConfig(config, info);

		log << TestLog::Message << "Creating pbuffer surface with config ID " << info.configId << TestLog::EndMessage;
		TCU_CHECK_EGL();

		// Clamp to maximums reported by implementation
		width	= deMin32(width, display.getConfigAttrib(config, EGL_MAX_PBUFFER_WIDTH));
		height	= deMin32(height, display.getConfigAttrib(config, EGL_MAX_PBUFFER_HEIGHT));

		if (width == 0 || height == 0)
		{
			log << TestLog::Message << "    Fail, maximum pbuffer size of " << width << "x" << height << " reported" << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid maximum pbuffer size");
			return;
		}

		const EGLint attribs[] =
		{
			EGL_WIDTH,			width,
			EGL_HEIGHT,			height,
			EGL_TEXTURE_FORMAT,	EGL_NO_TEXTURE,
			EGL_NONE
		};

		tcu::egl::PbufferSurface surface(display, config, attribs);

		testAttributes(surface, info);
	}
};

QuerySurfaceTests::QuerySurfaceTests (EglTestContext& eglTestCtx)
	: TestCaseGroup(eglTestCtx, "query_surface", "Surface Query Tests")
{
}

QuerySurfaceTests::~QuerySurfaceTests (void)
{
}

std::vector<EGLint> getConfigs (const tcu::egl::Display& display, EGLint surfaceType)
{
	std::vector<EGLint>			out;

	std::vector<EGLConfig> eglConfigs;
	display.getConfigs(eglConfigs);

	for (size_t ndx = 0; ndx < eglConfigs.size(); ndx++)
	{
		ConfigInfo info;
		display.describeConfig(eglConfigs[ndx], info);

		if (info.surfaceType & surfaceType)
			out.push_back(info.configId);
	}

	return out;
}

void QuerySurfaceTests::init (void)
{
	// Simple queries
	{
		tcu::TestCaseGroup* simpleGroup = new tcu::TestCaseGroup(m_testCtx, "simple", "Simple queries");
		addChild(simpleGroup);

		// Window
		{
			tcu::TestCaseGroup* windowGroup = new tcu::TestCaseGroup(m_testCtx, "window", "Window surfaces");
			simpleGroup->addChild(windowGroup);

			eglu::FilterList filters;
			filters << (eglu::ConfigSurfaceType() & EGL_WINDOW_BIT);

			std::vector<NamedConfigIdSet> configIdSets;
			NamedConfigIdSet::getDefaultSets(configIdSets, m_eglTestCtx.getConfigs(), filters);

			for (std::vector<NamedConfigIdSet>::iterator i = configIdSets.begin(); i != configIdSets.end(); i++)
				windowGroup->addChild(new QuerySurfaceSimpleWindowCase(m_eglTestCtx, i->getName(), i->getDescription(), i->getConfigIds()));
		}

		// Pixmap
		{
			tcu::TestCaseGroup* pixmapGroup = new tcu::TestCaseGroup(m_testCtx, "pixmap", "Pixmap surfaces");
			simpleGroup->addChild(pixmapGroup);

			eglu::FilterList filters;
			filters << (eglu::ConfigSurfaceType() & EGL_PIXMAP_BIT);

			std::vector<NamedConfigIdSet> configIdSets;
			NamedConfigIdSet::getDefaultSets(configIdSets, m_eglTestCtx.getConfigs(), filters);

			for (std::vector<NamedConfigIdSet>::iterator i = configIdSets.begin(); i != configIdSets.end(); i++)
				pixmapGroup->addChild(new QuerySurfaceSimplePixmapCase(m_eglTestCtx, i->getName(), i->getDescription(), i->getConfigIds()));
		}

		// Pbuffer
		{
			tcu::TestCaseGroup* pbufferGroup = new tcu::TestCaseGroup(m_testCtx, "pbuffer", "Pbuffer surfaces");
			simpleGroup->addChild(pbufferGroup);

			eglu::FilterList filters;
			filters << (eglu::ConfigSurfaceType() & EGL_PBUFFER_BIT);

			std::vector<NamedConfigIdSet> configIdSets;
			NamedConfigIdSet::getDefaultSets(configIdSets, m_eglTestCtx.getConfigs(), filters);

			for (std::vector<NamedConfigIdSet>::iterator i = configIdSets.begin(); i != configIdSets.end(); i++)
				pbufferGroup->addChild(new QuerySurfaceSimplePbufferCase(m_eglTestCtx, i->getName(), i->getDescription(), i->getConfigIds()));
		}
	}

	// Set surface attributes
	{
		tcu::TestCaseGroup* setAttributeGroup = new tcu::TestCaseGroup(m_testCtx, "set_attribute", "Setting attributes");
		addChild(setAttributeGroup);

		// Window
		{
			tcu::TestCaseGroup* windowGroup = new tcu::TestCaseGroup(m_testCtx, "window", "Window surfaces");
			setAttributeGroup->addChild(windowGroup);

			eglu::FilterList filters;
			filters << (eglu::ConfigSurfaceType() & EGL_WINDOW_BIT);

			std::vector<NamedConfigIdSet> configIdSets;
			NamedConfigIdSet::getDefaultSets(configIdSets, m_eglTestCtx.getConfigs(), filters);

			for (std::vector<NamedConfigIdSet>::iterator i = configIdSets.begin(); i != configIdSets.end(); i++)
				windowGroup->addChild(new SurfaceAttribWindowCase(m_eglTestCtx, i->getName(), i->getDescription(), i->getConfigIds()));
		}

		// Pixmap
		{
			tcu::TestCaseGroup* pixmapGroup = new tcu::TestCaseGroup(m_testCtx, "pixmap", "Pixmap surfaces");
			setAttributeGroup->addChild(pixmapGroup);

			eglu::FilterList filters;
			filters << (eglu::ConfigSurfaceType() & EGL_PIXMAP_BIT);

			std::vector<NamedConfigIdSet> configIdSets;
			NamedConfigIdSet::getDefaultSets(configIdSets, m_eglTestCtx.getConfigs(), filters);

			for (std::vector<NamedConfigIdSet>::iterator i = configIdSets.begin(); i != configIdSets.end(); i++)
				pixmapGroup->addChild(new SurfaceAttribPixmapCase(m_eglTestCtx, i->getName(), i->getDescription(), i->getConfigIds()));
		}

		// Pbuffer
		{
			tcu::TestCaseGroup* pbufferGroup = new tcu::TestCaseGroup(m_testCtx, "pbuffer", "Pbuffer surfaces");
			setAttributeGroup->addChild(pbufferGroup);

			eglu::FilterList filters;
			filters << (eglu::ConfigSurfaceType() & EGL_PBUFFER_BIT);

			std::vector<NamedConfigIdSet> configIdSets;
			NamedConfigIdSet::getDefaultSets(configIdSets, m_eglTestCtx.getConfigs(), filters);

			for (std::vector<NamedConfigIdSet>::iterator i = configIdSets.begin(); i != configIdSets.end(); i++)
				pbufferGroup->addChild(new SurfaceAttribPbufferCase(m_eglTestCtx, i->getName(), i->getDescription(), i->getConfigIds()));
		}
	}
}

} // egl
} // deqp
