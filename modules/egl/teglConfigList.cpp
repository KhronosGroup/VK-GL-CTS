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
 * \brief EGL tests
 *//*--------------------------------------------------------------------*/

#include "teglConfigList.hpp"
#include "tcuEgl.hpp"
#include "tcuTestLog.hpp"
#include "egluStrUtil.hpp"
#include "deStringUtil.hpp"

#include <vector>

using std::vector;

namespace deqp
{
namespace egl
{

ConfigList::ConfigList (EglTestContext& eglTestCtx)
	: TestCase(eglTestCtx, "configs", "Output the list of configs from EGL")

{
}

ConfigList::~ConfigList (void)
{
}

void ConfigList::init (void)
{
}

void ConfigList::deinit (void)
{
}

tcu::TestNode::IterateResult ConfigList::iterate (void)
{
	tcu::TestLog&		log			= m_testCtx.getLog();
	EGLDisplay			display		= m_eglTestCtx.getDisplay().getEGLDisplay();
	vector<EGLConfig>	configs;

	// \todo [2011-03-23 pyry] Check error codes!

	// \todo [kalle 10/08/2010] Get EGL version.

	m_eglTestCtx.getDisplay().getConfigs(configs);

	log.startEglConfigSet("EGL-configs", "List of all EGL configs");

	// \todo [kalle 10/08/2010] Add validity checks for the values?
	// \todo [kalle 10/08/2010] Adapt for different EGL versions

	for (int i = 0; i < (int)configs.size(); i++)
	{
		qpEglConfigInfo info;
		EGLint val = 0;

		eglGetConfigAttrib(display, configs[i], EGL_BUFFER_SIZE, &val);
		info.bufferSize = val;

		eglGetConfigAttrib(display, configs[i], EGL_RED_SIZE, &val);
		info.redSize = val;

		eglGetConfigAttrib(display, configs[i], EGL_GREEN_SIZE, &val);
		info.greenSize = val;

		eglGetConfigAttrib(display, configs[i], EGL_BLUE_SIZE, &val);
		info.blueSize = val;

		eglGetConfigAttrib(display, configs[i], EGL_LUMINANCE_SIZE, &val);
		info.luminanceSize = val;

		eglGetConfigAttrib(display, configs[i], EGL_ALPHA_SIZE, &val);
		info.alphaSize = val;

		eglGetConfigAttrib(display, configs[i], EGL_ALPHA_MASK_SIZE, &val);
		info.alphaMaskSize = val;

		eglGetConfigAttrib(display, configs[i], EGL_BIND_TO_TEXTURE_RGB, &val);
		info.bindToTextureRGB = val == EGL_TRUE ? DE_TRUE : DE_FALSE;

		eglGetConfigAttrib(display, configs[i], EGL_BIND_TO_TEXTURE_RGBA, &val);
		info.bindToTextureRGBA = val == EGL_TRUE ? DE_TRUE : DE_FALSE;

		eglGetConfigAttrib(display, configs[i], EGL_COLOR_BUFFER_TYPE, &val);
		std::string colorBufferType = de::toString(eglu::getColorBufferTypeStr(val));
		info.colorBufferType = colorBufferType.c_str();

		eglGetConfigAttrib(display, configs[i], EGL_CONFIG_CAVEAT, &val);
		std::string caveat = de::toString(eglu::getConfigCaveatStr(val));
		info.configCaveat = caveat.c_str();

		eglGetConfigAttrib(display, configs[i], EGL_CONFIG_ID, &val);
		info.configID = val;

		eglGetConfigAttrib(display, configs[i], EGL_CONFORMANT, &val);
		std::string conformant = de::toString(eglu::getAPIBitsStr(val));
		info.conformant = conformant.c_str();

		eglGetConfigAttrib(display, configs[i], EGL_DEPTH_SIZE, &val);
		info.depthSize = val;

		eglGetConfigAttrib(display, configs[i], EGL_LEVEL, &val);
		info.level = val;

		eglGetConfigAttrib(display, configs[i], EGL_MAX_PBUFFER_WIDTH, &val);
		info.maxPBufferWidth = val;

		eglGetConfigAttrib(display, configs[i], EGL_MAX_PBUFFER_HEIGHT, &val);
		info.maxPBufferHeight = val;

		eglGetConfigAttrib(display, configs[i], EGL_MAX_PBUFFER_PIXELS, &val);
		info.maxPBufferPixels = val;

		eglGetConfigAttrib(display, configs[i], EGL_MAX_SWAP_INTERVAL, &val);
		info.maxSwapInterval = val;

		eglGetConfigAttrib(display, configs[i], EGL_MIN_SWAP_INTERVAL, &val);
		info.minSwapInterval = val;

		eglGetConfigAttrib(display, configs[i], EGL_NATIVE_RENDERABLE, &val);
		info.nativeRenderable = val == EGL_TRUE ? DE_TRUE : DE_FALSE;

		eglGetConfigAttrib(display, configs[i], EGL_RENDERABLE_TYPE, &val);
		std::string renderableTypes = de::toString(eglu::getAPIBitsStr(val));
		info.renderableType = renderableTypes.c_str();

		eglGetConfigAttrib(display, configs[i], EGL_SAMPLE_BUFFERS, &val);
		info.sampleBuffers = val;

		eglGetConfigAttrib(display, configs[i], EGL_SAMPLES, &val);
		info.samples = val;

		eglGetConfigAttrib(display, configs[i], EGL_STENCIL_SIZE, &val);
		info.stencilSize = val;

		eglGetConfigAttrib(display, configs[i], EGL_SURFACE_TYPE, &val);
		std::string surfaceTypes = de::toString(eglu::getSurfaceBitsStr(val));
		info.surfaceTypes = surfaceTypes.c_str();

		eglGetConfigAttrib(display, configs[i], EGL_TRANSPARENT_TYPE, &val);
		std::string transparentType = de::toString(eglu::getTransparentTypeStr(val));
		info.transparentType = transparentType.c_str();

		eglGetConfigAttrib(display, configs[i], EGL_TRANSPARENT_RED_VALUE, &val);
		info.transparentRedValue = val;

		eglGetConfigAttrib(display, configs[i], EGL_TRANSPARENT_GREEN_VALUE, &val);
		info.transparentGreenValue = val;

		eglGetConfigAttrib(display, configs[i], EGL_TRANSPARENT_BLUE_VALUE, &val);
		info.transparentBlueValue = val;

		log.writeEglConfig(&info);
	}
	log.endEglConfigSet();

	getTestContext().setTestResult(QP_TEST_RESULT_PASS, "");

	return TestNode::STOP;
}

} // egl
} // deqp
