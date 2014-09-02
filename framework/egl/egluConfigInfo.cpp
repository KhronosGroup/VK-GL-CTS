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
 * \brief EGL config dst->
 *//*--------------------------------------------------------------------*/

#include "egluConfigInfo.hpp"
#include "egluDefs.hpp"

namespace eglu
{

deInt32 ConfigInfo::getAttribute (deUint32 attribute) const
{
	switch (attribute)
	{
		case EGL_BUFFER_SIZE:				return bufferSize;
		case EGL_RED_SIZE:					return redSize;
		case EGL_GREEN_SIZE:				return greenSize;
		case EGL_BLUE_SIZE:					return blueSize;
		case EGL_LUMINANCE_SIZE:			return luminanceSize;
		case EGL_ALPHA_SIZE:				return alphaSize;
		case EGL_ALPHA_MASK_SIZE:			return alphaMaskSize;
		case EGL_BIND_TO_TEXTURE_RGB:		return bindToTextureRGB;
		case EGL_BIND_TO_TEXTURE_RGBA:		return bindToTextureRGBA;
		case EGL_COLOR_BUFFER_TYPE:			return colorBufferType;
		case EGL_CONFIG_CAVEAT:				return configCaveat;
		case EGL_CONFIG_ID:					return configId;
		case EGL_CONFORMANT:				return conformant;
		case EGL_DEPTH_SIZE:				return depthSize;
		case EGL_LEVEL:						return level;
		case EGL_MAX_PBUFFER_WIDTH:			return maxPbufferWidth;
		case EGL_MAX_PBUFFER_HEIGHT:		return maxPbufferHeight;
		case EGL_MAX_SWAP_INTERVAL:			return maxSwapInterval;
		case EGL_MIN_SWAP_INTERVAL:			return minSwapInterval;
		case EGL_NATIVE_RENDERABLE:			return nativeRenderable;
		case EGL_NATIVE_VISUAL_ID:			return nativeVisualId;
		case EGL_NATIVE_VISUAL_TYPE:		return nativeVisualType;
		case EGL_RENDERABLE_TYPE:			return renderableType;
		case EGL_SAMPLE_BUFFERS:			return sampleBuffers;
		case EGL_SAMPLES:					return samples;
		case EGL_STENCIL_SIZE:				return stencilSize;
		case EGL_SURFACE_TYPE:				return surfaceType;
		case EGL_TRANSPARENT_TYPE:			return transparentType;
		case EGL_TRANSPARENT_RED_VALUE:		return transparentRedValue;
		case EGL_TRANSPARENT_GREEN_VALUE:	return transparentGreenValue;
		case EGL_TRANSPARENT_BLUE_VALUE:	return transparentBlueValue;
		default:							TCU_FAIL("Unknown attribute");
	}
}

void queryConfigInfo (EGLDisplay display, EGLConfig config, ConfigInfo* dst)
{
	eglGetConfigAttrib(display, config, EGL_BUFFER_SIZE,				&dst->bufferSize);
	eglGetConfigAttrib(display, config, EGL_RED_SIZE,					&dst->redSize);
	eglGetConfigAttrib(display, config, EGL_GREEN_SIZE,					&dst->greenSize);
	eglGetConfigAttrib(display, config, EGL_BLUE_SIZE,					&dst->blueSize);
	eglGetConfigAttrib(display, config, EGL_LUMINANCE_SIZE,				&dst->luminanceSize);
	eglGetConfigAttrib(display, config, EGL_ALPHA_SIZE,					&dst->alphaSize);
	eglGetConfigAttrib(display, config, EGL_ALPHA_MASK_SIZE,			&dst->alphaMaskSize);
	eglGetConfigAttrib(display, config, EGL_BIND_TO_TEXTURE_RGB,		(EGLint*)&dst->bindToTextureRGB);
	eglGetConfigAttrib(display, config, EGL_BIND_TO_TEXTURE_RGBA,		(EGLint*)&dst->bindToTextureRGBA);
	eglGetConfigAttrib(display, config, EGL_COLOR_BUFFER_TYPE,			(EGLint*)&dst->colorBufferType);
	eglGetConfigAttrib(display, config, EGL_CONFIG_CAVEAT,				(EGLint*)&dst->configCaveat);
	eglGetConfigAttrib(display, config, EGL_CONFIG_ID,					&dst->configId);
	eglGetConfigAttrib(display, config, EGL_CONFORMANT,					&dst->conformant);
	eglGetConfigAttrib(display, config, EGL_DEPTH_SIZE,					&dst->depthSize);
	eglGetConfigAttrib(display, config, EGL_LEVEL,						&dst->level);
	eglGetConfigAttrib(display, config, EGL_MAX_PBUFFER_WIDTH,			&dst->maxPbufferWidth);
	eglGetConfigAttrib(display, config, EGL_MAX_PBUFFER_HEIGHT,			&dst->maxPbufferHeight);
	eglGetConfigAttrib(display, config, EGL_MAX_SWAP_INTERVAL,			&dst->maxSwapInterval);
	eglGetConfigAttrib(display, config, EGL_MIN_SWAP_INTERVAL,			&dst->minSwapInterval);
	eglGetConfigAttrib(display, config, EGL_NATIVE_RENDERABLE,			(EGLint*)&dst->nativeRenderable);
	eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID,			&dst->nativeVisualId);
	eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_TYPE,			&dst->nativeVisualType);
	eglGetConfigAttrib(display, config, EGL_RENDERABLE_TYPE,			&dst->renderableType);
	eglGetConfigAttrib(display, config, EGL_SAMPLE_BUFFERS,				&dst->sampleBuffers);
	eglGetConfigAttrib(display, config, EGL_SAMPLES,					&dst->samples);
	eglGetConfigAttrib(display, config, EGL_STENCIL_SIZE,				&dst->stencilSize);
	eglGetConfigAttrib(display, config, EGL_SURFACE_TYPE,				&dst->surfaceType);
	eglGetConfigAttrib(display, config, EGL_TRANSPARENT_TYPE,			(EGLint*)&dst->transparentType);
	eglGetConfigAttrib(display, config, EGL_TRANSPARENT_RED_VALUE,		&dst->transparentRedValue);
	eglGetConfigAttrib(display, config, EGL_TRANSPARENT_GREEN_VALUE,	&dst->transparentGreenValue);
	eglGetConfigAttrib(display, config, EGL_TRANSPARENT_BLUE_VALUE,		&dst->transparentBlueValue);
	EGLU_CHECK_MSG("Failed to query config info");
}

} // eglu
