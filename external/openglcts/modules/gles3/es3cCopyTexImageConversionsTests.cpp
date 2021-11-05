/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */ /*!
 * \file es3cCopyTexImageConversionsTests.cpp
 * \brief Tests verifying glCopyTexImage2D..
 */ /*-------------------------------------------------------------------*/

#include "es3cCopyTexImageConversionsTests.hpp"
#include "deMath.h"
#include "deSharedPtr.hpp"
#include "gluContextInfo.hpp"
#include "gluDefs.hpp"
#include "gluStrUtil.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTestLog.hpp"
#include <cstring>
#include <limits>
#include <map>

using namespace glw;

namespace es3cts
{

// Amount of entries database should allocate for its entries upon creation.
#define N_START_CONVERSION_DATABASE_ENTRIES (32)

// Should 3D textures be used as source attachments, this field defines
//  their depth. It MUST be at least 2, because the test implementation
//  also uses second array (counted from one) to store the data-set information.
#define TEXTURE_DEPTH (2)
// Data set height
#define TEXTURE_HEIGHT (2)
// Data set width
#define TEXTURE_WIDTH (2)

// Defines for non color-renderable textures support
#define NUMBER_OF_ELEMENTS_IN_VEC4 (4)
#define NUMBER_OF_POINTS_TO_DRAW (TEXTURE_WIDTH * TEXTURE_HEIGHT)
#define TEXTURE_COORDINATES_ARRAY_SIZE (TEXTURE_WIDTH * TEXTURE_HEIGHT * NUMBER_OF_ELEMENTS_IN_VEC4 * sizeof(float))
#define TEXTURE_2D_SAMPLER_TYPE (0)
#define TEXTURE_3D_SAMPLER_TYPE (1)
#define TEXTURE_2D_ARRAY_SAMPLER_TYPE (2)
#define TEXTURE_CUBE_SAMPLER_TYPE (3)
#define SRC_TEXTURE_COORDS_ATTRIB_INDEX (1)
#define DST_TEXTURE_COORDS_ATTRIB_INDEX (0)

// Buffer object indices used for non color-renderable textures support.
#define COMPARISON_RESULT_BUFFER_OBJECT_INDEX (0)
#define SOURCE_TEXTURE_PIXELS_BUFFER_OBJECT_INDEX (1)
#define DESTINATION_TEXTURE_PIXELS_BUFFER_OBJECT_INDEX (2)

// Stores detailed information about:
// 1) what FBO effective internalformats can be used for glCopyTexImage2D(), assuming
//	specific result texture's internalformat as passed by one of the arguments.
// 2) what internalformat the result texture object should use.
const GLenum conversionArray[] = {
	/*					 GL_RGBA		GL_RGB	   GL_LUMINANCE_ALPHA		 GL_LUMINANCE		GL_ALPHA	   GL_R8	GL_R8_SNORM  GL_RG8	   GL_RG8_SNORM  GL_RGB8  GL_RGB8_SNORM  GL_RGB565  GL_RGBA4  GL_RGB5_A1  GL_RGBA8  GL_RGBA8_SNORM  GL_RGB10_A2  GL_RGB10_A2UI  GL_SRGB8  GL_SRGB8_ALPHA8  GL_R16F  GL_RG16F  GL_RGB16F  GL_RGBA16F  GL_R32F   GL_RG32F  GL_RGB32F  GL_RGBA32F  GL_R11F_G11F_B10F  GL_RGB9_E5   GL_R8I	GL_R8UI   GL_R16I   GL_R16UI  GL_R32I   GL_R32UI  GL_RG8I   GL_RG8UI  GL_RG16I  GL_RG16UI  GL_RG32I   GL_RG32UI  GL_RGB8I  GL_RGB8UI  GL_RGB16I  GL_RGB16UI  GL_RGB32I  GL_RGB32UI  GL_RGBA8I  GL_RGBA8UI  GL_RGBA16I  GL_RGBA16UI  GL_RGBA32I  GL_RGBA32UI */
	/* GL_R8,			*/ GL_NONE,		GL_NONE,   GL_NONE,					 GL_LUMINANCE8_OES,	GL_NONE,	   GL_R8,   GL_NONE,	 GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,		  GL_NONE,   GL_NONE,  GL_NONE,	GL_NONE,  GL_NONE,		GL_NONE,	 GL_NONE,	   GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,	   GL_NONE,	 GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,   GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,	GL_NONE,	 GL_NONE,	GL_NONE,
	/* GL_RG8,			*/ GL_NONE,		GL_NONE,   GL_NONE,					 GL_LUMINANCE8_OES,	GL_NONE,	   GL_R8,   GL_NONE,	 GL_RG8,   GL_NONE,		 GL_NONE, GL_NONE,		  GL_NONE,   GL_NONE,  GL_NONE,	GL_NONE,  GL_NONE,		GL_NONE,	 GL_NONE,	   GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,	   GL_NONE,	 GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,   GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,	GL_NONE,	 GL_NONE,	GL_NONE,
	/* GL_RGB8,			*/ GL_NONE,		GL_RGB8,   GL_NONE,					 GL_LUMINANCE8_OES,	GL_NONE,	   GL_R8,   GL_NONE,	 GL_RG8,   GL_NONE,		 GL_RGB8, GL_NONE,		  GL_NONE,   GL_NONE,  GL_NONE,	GL_NONE,  GL_NONE,		GL_NONE,	 GL_NONE,	   GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,	   GL_NONE,	 GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,   GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,	GL_NONE,	 GL_NONE,	GL_NONE,
	/* GL_RGB565,		*/ GL_NONE,		GL_RGB565, GL_NONE,					 GL_LUMINANCE8_OES,	GL_NONE,	   GL_NONE, GL_NONE,	 GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,		  GL_RGB565, GL_NONE,  GL_NONE,	GL_NONE,  GL_NONE,		GL_NONE,	 GL_NONE,	   GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,	   GL_NONE,	 GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,   GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,	GL_NONE,	 GL_NONE,	GL_NONE,
	/* GL_RGBA4,		*/ GL_RGBA4,	GL_RGB565, GL_LUMINANCE8_ALPHA8_OES, GL_LUMINANCE8_OES,	GL_ALPHA8_OES, GL_NONE, GL_NONE,	 GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,		  GL_NONE,   GL_RGBA4, GL_NONE,	GL_NONE,  GL_NONE,		GL_NONE,	 GL_NONE,	   GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,	   GL_NONE,	 GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,   GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,	GL_NONE,	 GL_NONE,	GL_NONE,
	/* GL_RGB5_A1,		*/ GL_RGB5_A1,	GL_RGB565, GL_LUMINANCE8_ALPHA8_OES, GL_LUMINANCE8_OES,	GL_ALPHA8_OES, GL_NONE, GL_NONE,	 GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,		  GL_NONE,   GL_NONE,  GL_RGB5_A1, GL_NONE,  GL_NONE,	GL_NONE,	 GL_NONE,	   GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,	   GL_NONE,	 GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,   GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,	GL_NONE,	 GL_NONE,	GL_NONE,
	/* GL_RGBA8,		*/ GL_RGBA8,	GL_RGB8,   GL_LUMINANCE8_ALPHA8_OES, GL_LUMINANCE8_OES,	GL_ALPHA8_OES, GL_R8,   GL_NONE,	 GL_RG8,   GL_NONE,		 GL_RGB8, GL_NONE,		  GL_NONE,   GL_NONE,  GL_NONE,	GL_RGBA8, GL_NONE,		GL_NONE,	 GL_NONE,	   GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,	   GL_NONE,	 GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,   GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,	GL_NONE,	 GL_NONE,	GL_NONE,
	/* GL_RGB10_A2,		*/ GL_NONE,		GL_RGB8,   GL_NONE,					 GL_LUMINANCE8_OES,	GL_ALPHA8_OES, GL_NONE, GL_NONE,	 GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,		  GL_NONE,   GL_NONE,  GL_NONE,	GL_NONE,  GL_NONE,		GL_RGB10_A2, GL_NONE,	   GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,	   GL_NONE,	 GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,   GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,	GL_NONE,	 GL_NONE,	GL_NONE,
	/* GL_RGB10_A2UI,	*/ GL_NONE,		GL_NONE,   GL_NONE,					 GL_NONE,			GL_NONE,	   GL_NONE, GL_NONE,	 GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,		  GL_NONE,   GL_NONE,  GL_NONE,	GL_NONE,  GL_NONE,		GL_NONE,	 GL_RGB10_A2UI, GL_NONE, GL_NONE,		 GL_NONE, GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,	   GL_NONE,	 GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,   GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,	GL_NONE,	 GL_NONE,	GL_NONE,
	/* GL_SRGB8_ALPHA8,	*/ GL_NONE,		GL_NONE,   GL_NONE,					 GL_NONE,			GL_NONE,	   GL_NONE, GL_NONE,	 GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,		  GL_NONE,   GL_NONE,  GL_NONE,	GL_NONE,  GL_NONE,		GL_NONE,	 GL_NONE,	   GL_SRGB8, GL_SRGB8_ALPHA8, GL_NONE, GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	 GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,   GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,	GL_NONE,	 GL_NONE,	GL_NONE,
	/* GL_R8I,			*/ GL_NONE,		GL_NONE,   GL_NONE,					 GL_NONE,			GL_NONE,	   GL_NONE, GL_NONE,	 GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,		  GL_NONE,   GL_NONE,  GL_NONE,	GL_NONE,  GL_NONE,		GL_NONE,	 GL_NONE,	   GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,	   GL_NONE,	 GL_R8I,   GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,   GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,	GL_NONE,	 GL_NONE,	GL_NONE,
	/* GL_R8UI,			*/ GL_NONE,		GL_NONE,   GL_NONE,					 GL_NONE,			GL_NONE,	   GL_NONE, GL_NONE,	 GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,		  GL_NONE,   GL_NONE,  GL_NONE,	GL_NONE,  GL_NONE,		GL_NONE,	 GL_NONE,	   GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,	   GL_NONE,	 GL_NONE,  GL_R8UI,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,   GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,	GL_NONE,	 GL_NONE,	GL_NONE,
	/* GL_R16I,			*/ GL_NONE,		GL_NONE,   GL_NONE,					 GL_NONE,			GL_NONE,	   GL_NONE, GL_NONE,	 GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,		  GL_NONE,   GL_NONE,  GL_NONE,	GL_NONE,  GL_NONE,		GL_NONE,	 GL_NONE,	   GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,	   GL_NONE,	 GL_NONE,  GL_NONE,  GL_R16I,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,   GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,	GL_NONE,	 GL_NONE,	GL_NONE,
	/* GL_R16UI,		*/ GL_NONE,		GL_NONE,   GL_NONE,					 GL_NONE,			GL_NONE,	   GL_NONE, GL_NONE,	 GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,		  GL_NONE,   GL_NONE,  GL_NONE,	GL_NONE,  GL_NONE,		GL_NONE,	 GL_NONE,	   GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,	   GL_NONE,	 GL_NONE,  GL_NONE,  GL_NONE,  GL_R16UI, GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,   GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,	GL_NONE,	 GL_NONE,	GL_NONE,
	/* GL_R32I,			*/ GL_NONE,		GL_NONE,   GL_NONE,					 GL_NONE,			GL_NONE,	   GL_NONE, GL_NONE,	 GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,		  GL_NONE,   GL_NONE,  GL_NONE,	GL_NONE,  GL_NONE,		GL_NONE,	 GL_NONE,	   GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,	   GL_NONE,	 GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_R32I,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,   GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,	GL_NONE,	 GL_NONE,	GL_NONE,
	/* GL_R32UI,		*/ GL_NONE,		GL_NONE,   GL_NONE,					 GL_NONE,			GL_NONE,	   GL_NONE, GL_NONE,	 GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,		  GL_NONE,   GL_NONE,  GL_NONE,	GL_NONE,  GL_NONE,		GL_NONE,	 GL_NONE,	   GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,	   GL_NONE,	 GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_R32UI, GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,   GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,	GL_NONE,	 GL_NONE,	GL_NONE,
	/* GL_RG8I,			*/ GL_NONE,		GL_NONE,   GL_NONE,					 GL_NONE,			GL_NONE,	   GL_NONE, GL_NONE,	 GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,		  GL_NONE,   GL_NONE,  GL_NONE,	GL_NONE,  GL_NONE,		GL_NONE,	 GL_NONE,	   GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,	   GL_NONE,	 GL_R8I,   GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_RG8I,  GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,   GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,	GL_NONE,	 GL_NONE,	GL_NONE,
	/* GL_RG8UI,		*/ GL_NONE,		GL_NONE,   GL_NONE,					 GL_NONE,			GL_NONE,	   GL_NONE, GL_NONE,	 GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,		  GL_NONE,   GL_NONE,  GL_NONE,	GL_NONE,  GL_NONE,		GL_NONE,	 GL_NONE,	   GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,	   GL_NONE,	 GL_NONE,  GL_R8UI,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_RG8UI, GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,   GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,	GL_NONE,	 GL_NONE,	GL_NONE,
	/* GL_RG16I,		*/ GL_NONE,		GL_NONE,   GL_NONE,					 GL_NONE,			GL_NONE,	   GL_NONE, GL_NONE,	 GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,		  GL_NONE,   GL_NONE,  GL_NONE,	GL_NONE,  GL_NONE,		GL_NONE,	 GL_NONE,	   GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,	   GL_NONE,	 GL_NONE,  GL_NONE,  GL_R16I,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_RG16I, GL_NONE,   GL_NONE,   GL_NONE,   GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,	GL_NONE,	 GL_NONE,	GL_NONE,
	/* GL_RG16UI,		*/ GL_NONE,		GL_NONE,   GL_NONE,					 GL_NONE,			GL_NONE,	   GL_NONE, GL_NONE,	 GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,		  GL_NONE,   GL_NONE,  GL_NONE,	GL_NONE,  GL_NONE,		GL_NONE,	 GL_NONE,	   GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,	   GL_NONE,	 GL_NONE,  GL_NONE,  GL_NONE,  GL_R16UI, GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_RG16UI, GL_NONE,   GL_NONE,   GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,	GL_NONE,	 GL_NONE,	GL_NONE,
	/* GL_RG32I,		*/ GL_NONE,		GL_NONE,   GL_NONE,					 GL_NONE,			GL_NONE,	   GL_NONE, GL_NONE,	 GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,		  GL_NONE,   GL_NONE,  GL_NONE,	GL_NONE,  GL_NONE,		GL_NONE,	 GL_NONE,	   GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,	   GL_NONE,	 GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_R32I,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,   GL_RG32I,  GL_NONE,   GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,	GL_NONE,	 GL_NONE,	GL_NONE,
	/* GL_RG32UI,		*/ GL_NONE,		GL_NONE,   GL_NONE,					 GL_NONE,			GL_NONE,	   GL_NONE, GL_NONE,	 GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,		  GL_NONE,   GL_NONE,  GL_NONE,	GL_NONE,  GL_NONE,		GL_NONE,	 GL_NONE,	   GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,	   GL_NONE,	 GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_R32UI, GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,   GL_RG32UI, GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,	GL_NONE,	 GL_NONE,	GL_NONE,
	/* GL_RGBA8I,		*/ GL_NONE,		GL_NONE,   GL_NONE,					 GL_NONE,			GL_NONE,	   GL_NONE, GL_NONE,	 GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,		  GL_NONE,   GL_NONE,  GL_NONE,	GL_NONE,  GL_NONE,		GL_NONE,	 GL_NONE,	   GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,	   GL_NONE,	 GL_R8I,   GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_RG8I,  GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,   GL_RGB8I, GL_NONE,   GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_RGBA8I, GL_NONE,	GL_NONE,	GL_NONE,	 GL_NONE,	GL_NONE,
	/* GL_RGBA8UI,		*/ GL_NONE,		GL_NONE,   GL_NONE,					 GL_NONE,			GL_NONE,	   GL_NONE, GL_NONE,	 GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,		  GL_NONE,   GL_NONE,  GL_NONE,	GL_NONE,  GL_NONE,		GL_NONE,	 GL_NONE,	   GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,	   GL_NONE,	 GL_NONE,  GL_R8UI,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_RG8UI, GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,   GL_NONE,  GL_RGB8UI, GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,   GL_RGBA8UI, GL_NONE,	GL_NONE,	 GL_NONE,	GL_NONE,
	/* GL_RGBA16I,		*/ GL_NONE,		GL_NONE,   GL_NONE,					 GL_NONE,			GL_NONE,	   GL_NONE, GL_NONE,	 GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,		  GL_NONE,   GL_NONE,  GL_NONE,	GL_NONE,  GL_NONE,		GL_NONE,	 GL_NONE,	   GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,	   GL_NONE,	 GL_NONE,  GL_NONE,  GL_R16I,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_RG16I, GL_NONE,   GL_NONE,   GL_NONE,   GL_NONE,  GL_NONE,   GL_RGB16I, GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_RGBA16I, GL_NONE,	 GL_NONE,	GL_NONE,
	/* GL_RGBA16UI,		*/ GL_NONE,		GL_NONE,   GL_NONE,					 GL_NONE,			GL_NONE,	   GL_NONE, GL_NONE,	 GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,		  GL_NONE,   GL_NONE,  GL_NONE,	GL_NONE,  GL_NONE,		GL_NONE,	 GL_NONE,	   GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,	   GL_NONE,	 GL_NONE,  GL_NONE,  GL_NONE,  GL_R16UI, GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_RG16UI, GL_NONE,   GL_NONE,   GL_NONE,  GL_NONE,   GL_NONE,   GL_RGB16UI, GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,	GL_RGBA16UI, GL_NONE,	GL_NONE,
	/* GL_RGBA32I,		*/ GL_NONE,		GL_NONE,   GL_NONE,					 GL_NONE,			GL_NONE,	   GL_NONE, GL_NONE,	 GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,		  GL_NONE,   GL_NONE,  GL_NONE,	GL_NONE,  GL_NONE,		GL_NONE,	 GL_NONE,	   GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,	   GL_NONE,	 GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_R32I,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,   GL_RG32I,  GL_NONE,   GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,	GL_RGB32I, GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,	GL_NONE,	 GL_RGBA32I, GL_NONE,
	/* GL_RGBA32UI,		*/ GL_NONE,		GL_NONE,   GL_NONE,					 GL_NONE,			GL_NONE,	   GL_NONE, GL_NONE,	 GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,		  GL_NONE,   GL_NONE,  GL_NONE,	GL_NONE,  GL_NONE,		GL_NONE,	 GL_NONE,	   GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,	   GL_NONE,	 GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_R32UI, GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,   GL_RG32UI, GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,	GL_NONE,   GL_RGB32UI, GL_NONE,   GL_NONE,	GL_NONE,	GL_NONE,	 GL_NONE,	GL_RGBA32UI,
	/* GL_R16F,			*/ GL_NONE,		GL_NONE,   GL_NONE,					 GL_NONE,			GL_NONE,	   GL_NONE, GL_NONE,	 GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,		  GL_NONE,   GL_NONE,  GL_NONE,	GL_NONE,  GL_NONE,		GL_NONE,	 GL_NONE,	   GL_NONE,  GL_NONE,		 GL_R16F, GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,	   GL_NONE,	 GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,   GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,	GL_NONE,	 GL_NONE,	GL_NONE,
	/* GL_RG16F,		*/ GL_NONE,		GL_NONE,   GL_NONE,					 GL_NONE,			GL_NONE,	   GL_NONE, GL_NONE,	 GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,		  GL_NONE,   GL_NONE,  GL_NONE,	GL_NONE,  GL_NONE,		GL_NONE,	 GL_NONE,	   GL_NONE,  GL_NONE,		 GL_R16F, GL_RG16F, GL_NONE,   GL_NONE,	GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,	   GL_NONE,	 GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,   GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,	GL_NONE,	 GL_NONE,	GL_NONE,
	/* GL_R32F,			*/ GL_NONE,		GL_NONE,   GL_NONE,					 GL_NONE,			GL_NONE,	   GL_NONE, GL_NONE,	 GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,		  GL_NONE,   GL_NONE,  GL_NONE,	GL_NONE,  GL_NONE,		GL_NONE,	 GL_NONE,	   GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,  GL_NONE,   GL_NONE,	GL_R32F,  GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,	   GL_NONE,	 GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,   GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,	GL_NONE,	 GL_NONE,	GL_NONE,
	/* GL_RG32F,		*/ GL_NONE,		GL_NONE,   GL_NONE,					 GL_NONE,			GL_NONE,	   GL_NONE, GL_NONE,	 GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,		  GL_NONE,   GL_NONE,  GL_NONE,	GL_NONE,  GL_NONE,		GL_NONE,	 GL_NONE,	   GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,  GL_NONE,   GL_NONE,	GL_R32F,  GL_RG32F, GL_NONE,   GL_NONE,	GL_NONE,	   GL_NONE,	 GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,   GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,	GL_NONE,	 GL_NONE,	GL_NONE,
	/* GL_RGB16F,		*/ GL_NONE,		GL_NONE,   GL_NONE,					 GL_NONE,			GL_NONE,	   GL_NONE, GL_NONE,	 GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,		  GL_NONE,   GL_NONE,  GL_NONE,	GL_NONE,  GL_NONE,		GL_NONE,	 GL_NONE,	   GL_NONE,  GL_NONE,		 GL_R16F, GL_RG16F, GL_RGB16F, GL_NONE,	GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,	   GL_NONE,	 GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,   GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,	GL_NONE,	 GL_NONE,	GL_NONE,
	/* GL_RGBA16F,		*/ GL_NONE,		GL_NONE,   GL_NONE,					 GL_NONE,			GL_NONE,	   GL_NONE, GL_NONE,	 GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,		  GL_NONE,   GL_NONE,  GL_NONE,	GL_NONE,  GL_NONE,		GL_NONE,	 GL_NONE,	   GL_NONE,  GL_NONE,		 GL_R16F, GL_RG16F, GL_RGB16F, GL_RGBA16F, GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	 GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,   GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,	GL_NONE,	 GL_NONE,	GL_NONE,
	/* GL_RGB32F,		*/ GL_NONE,		GL_NONE,   GL_NONE,					 GL_NONE,			GL_NONE,	   GL_NONE, GL_NONE,	 GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,		  GL_NONE,   GL_NONE,  GL_NONE,	GL_NONE,  GL_NONE,		GL_NONE,	 GL_NONE,	   GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,  GL_NONE,   GL_NONE,	GL_R32F,  GL_RG32F, GL_RGB32F, GL_NONE,	GL_NONE,	   GL_NONE,	 GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,   GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,	GL_NONE,	 GL_NONE,	GL_NONE,
	/* GL_RGBA32F,		*/ GL_NONE,		GL_NONE,   GL_NONE,					 GL_NONE,			GL_NONE,	   GL_NONE, GL_NONE,	 GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,		  GL_NONE,   GL_NONE,  GL_NONE,	GL_NONE,  GL_NONE,		GL_NONE,	 GL_NONE,	   GL_NONE,  GL_NONE,		 GL_NONE, GL_NONE,  GL_NONE,   GL_NONE,	GL_R32F,  GL_RG32F, GL_RGB32F, GL_RGBA32F, GL_NONE,	   GL_NONE,	 GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,   GL_NONE,  GL_NONE,   GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,   GL_NONE,	GL_NONE,	GL_NONE,	 GL_NONE,	GL_NONE,
};

// Tells:
// 1) how many rows conversion_array uses.
// 2) what destination internalformat (NOT effective internalformat!)
//	corresponds to each entry.
// NOTE: If you need to modify this array, make sure conversion-array
//	   is updated accordingly!
const GLenum copyTexImage2DInternalFormatOrdering[] = { GL_RGBA,
														GL_RGB,
														GL_LUMINANCE_ALPHA,
														GL_LUMINANCE,
														GL_ALPHA,
														GL_R8,
														GL_R8_SNORM,
														GL_RG8,
														GL_RG8_SNORM,
														GL_RGB8,
														GL_RGB8_SNORM,
														GL_RGB565,
														GL_RGBA4,
														GL_RGB5_A1,
														GL_RGBA8,
														GL_RGBA8_SNORM,
														GL_RGB10_A2,
														GL_RGB10_A2UI,
														GL_SRGB8,
														GL_SRGB8_ALPHA8,
														GL_R16F,
														GL_RG16F,
														GL_RGB16F,
														GL_RGBA16F,
														GL_R32F,
														GL_RG32F,
														GL_RGB32F,
														GL_RGBA32F,
														GL_R11F_G11F_B10F,
														GL_RGB9_E5,
														GL_R8I,
														GL_R8UI,
														GL_R16I,
														GL_R16UI,
														GL_R32I,
														GL_R32UI,
														GL_RG8I,
														GL_RG8UI,
														GL_RG16I,
														GL_RG16UI,
														GL_RG32I,
														GL_RG32UI,
														GL_RGB8I,
														GL_RGB8UI,
														GL_RGB16I,
														GL_RGB16UI,
														GL_RGB32I,
														GL_RGB32UI,
														GL_RGBA8I,
														GL_RGBA8UI,
														GL_RGBA16I,
														GL_RGBA16UI,
														GL_RGBA32I,
														GL_RGBA32UI };

// Ordering as per Bug 9807 table for FBO effective internalformats
const GLenum fboEffectiveInternalFormatOrdering[] = {
	GL_R8,			 GL_RG8,	GL_RGB8,  GL_RGB565, GL_RGBA4,  GL_RGB5_A1, GL_RGBA8,   GL_RGB10_A2, GL_RGB10_A2UI,
	GL_SRGB8_ALPHA8, GL_R8I,	GL_R8UI,  GL_R16I,   GL_R16UI,  GL_R32I,	GL_R32UI,   GL_RG8I,	 GL_RG8UI,
	GL_RG16I,		 GL_RG16UI, GL_RG32I, GL_RG32UI, GL_RGBA8I, GL_RGBA8UI, GL_RGBA16I, GL_RGBA16UI, GL_RGBA32I,
	GL_RGBA32UI,	 GL_R16F,   GL_RG16F, GL_R32F,   GL_RG32F,  GL_RGB16F,  GL_RGBA16F, GL_RGB32F,   GL_RGBA32F,
};

// Tells how channels are ordered for a particular pixel.
enum ChannelOrder
{
	CHANNEL_ORDER_ABGR,
	CHANNEL_ORDER_BGR,
	CHANNEL_ORDER_BGRA,
	CHANNEL_ORDER_R,
	CHANNEL_ORDER_RG,
	CHANNEL_ORDER_RGB,
	CHANNEL_ORDER_RGBA,

	CHANNEL_ORDER_UNKNOWN
};

// Tells how many bits and what type is used for data representation
// for a single pixel channel.
enum ChannelDataType
{
	CHANNEL_DATA_TYPE_NONE = 0,
	CHANNEL_DATA_TYPE_SIGNED_BYTE_8BITS,
	CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS,
	CHANNEL_DATA_TYPE_SIGNED_SHORT_16BITS,
	CHANNEL_DATA_TYPE_UNSIGNED_BYTE_1BIT,
	CHANNEL_DATA_TYPE_UNSIGNED_BYTE_2BITS,
	CHANNEL_DATA_TYPE_UNSIGNED_BYTE_4BITS,
	CHANNEL_DATA_TYPE_UNSIGNED_BYTE_5BITS,
	CHANNEL_DATA_TYPE_UNSIGNED_BYTE_6BITS,
	CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS,
	CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS,
	CHANNEL_DATA_TYPE_UNSIGNED_SHORT_10BITS,
	CHANNEL_DATA_TYPE_UNSIGNED_SHORT_16BITS,
	CHANNEL_DATA_TYPE_FLOAT
};

// Structure holding uniform locations and object IDs.
// Those values are used to support non-color-renderable texture internalformat checks.
struct NonRenderableInternalformatSupportObjects
{
	GLuint comparison_result_buffer_object_id;
	GLuint dst_texture_pixels_buffer_object_id;
	GLint  dst_2D_texture_uniform_location;
	GLint  dst_Cube_texture_uniform_location;
	GLuint fragment_shader_object_id;
	GLuint program_object_id;
	GLuint src_texture_pixels_buffer_object_id;
	GLint  src_2D_texture_uniform_location;
	GLint  src_2DArray_texture_uniform_location;
	GLint  src_3D_texture_uniform_location;
	GLint  src_Cube_texture_uniform_location;
	GLuint transform_feedback_object_id;
	GLuint vertex_shader_object_id;
	GLint  channels_to_compare_uniform_location;
	GLint  samplers_to_use_uniform_location;
	GLuint src_texture_coordinates_buffer_object_id;
	GLuint dst_texture_coordinates_buffer_object_id;
};

// Structure describing contents of a channel of a single pixel.
struct ChannelData
{
	// Union that allows to access the data representation
	// in a data_type-friendly manner
	union {
		signed char	signed_byte_data;
		signed int	 signed_integer_data;
		signed short   signed_short_data;
		unsigned char  unsigned_byte_data;
		unsigned int   unsigned_integer_data;
		unsigned short unsigned_short_data;
		float		   float_data;
	};

	// Data type used for channel representation
	ChannelDataType data_type;
};

// Structure describing a single pixel.
struct PixelData
{
	// Alpha channel data descriptor
	ChannelData alpha;
	// Blue channel data descriptor
	ChannelData blue;
	// Green channel data descriptor
	ChannelData green;
	// Red channel data descriptor
	ChannelData red;

	// For source pixels:	  GL internal-format used by all channels.
	// For destination pixels: GL format to be used for gl.readPixels()
	//						 operation in order to retrieve result data
	//						 in a matching representation.
	GLenum data_internalformat;
	// For source pixels:	  GL type used by all channels.
	// For destination pixels: GL type to be used for gl.readPixels()
	//						 operation in order to retrieve result data
	//						 in a matching representation.
	GLenum data_type;
};

// To confirm contents of data stored in non-renderable internalformat, a special shader
// is used. This type definition tells which texture() function sampler should be used
// for sampling the texture data.
enum DataSamplerType
{
	DATA_SAMPLER_FLOAT,
	DATA_SAMPLER_INTEGER,
	DATA_SAMPLER_UNSIGNED_INTEGER,
};

// When a special shader is used to check whether the copy succeeded we need to know which
// channels will have to be compared
enum PixelCompareChannel
{
	PIXEL_COMPARE_CHANNEL_R	= 0x1,
	PIXEL_COMPARE_CHANNEL_G	= 0x2,
	PIXEL_COMPARE_CHANNEL_B	= 0x4,
	PIXEL_COMPARE_CHANNEL_A	= 0x8,
	PIXEL_COMPARE_CHANNEL_RG   = PIXEL_COMPARE_CHANNEL_R | PIXEL_COMPARE_CHANNEL_G,
	PIXEL_COMPARE_CHANNEL_RA   = PIXEL_COMPARE_CHANNEL_R | PIXEL_COMPARE_CHANNEL_A,
	PIXEL_COMPARE_CHANNEL_RGB  = PIXEL_COMPARE_CHANNEL_RG | PIXEL_COMPARE_CHANNEL_B,
	PIXEL_COMPARE_CHANNEL_RGBA = PIXEL_COMPARE_CHANNEL_RGB | PIXEL_COMPARE_CHANNEL_A,
};

// Structure describing a single conversion rule.
//
// For more details on meaning of these fields, please refer
// to doxygen of AddEntryToConversionDatabase() and similar.
struct ConversionDatabaseEntry
{
	// Reference destination data expected for bottom-left corner
	PixelData dst_bottomleft_corner;
	// Reference destination data expected for bottom-right corner
	PixelData dst_bottomright_corner;
	// Reference destination data expected for top-left corner
	PixelData dst_topleft_corner;
	// Reference destination data expected for top-right corner
	PixelData dst_topright_corner;

	// Input bottom-left corner data to be used for conversion
	PixelData src_bottomleft_corner;
	// Input bottom-right corner data to be used for conversion
	PixelData src_bottomright_corner;
	// Input top-left corner data to be used for conversion
	PixelData src_topleft_corner;
	// Input top-right corner data to be used for conversion
	PixelData src_topright_corner;

	// What are the channels that we need to compare if gl.readPixels
	// can't be used to read back the data
	PixelCompareChannel channels_to_compare;
};

// Structure describing contents of an opaque conversion database handle.
class ConversionDatabase
{
public:
	ConversionDatabase();
	~ConversionDatabase();

	void initializeDatabase();

	bool isTypeSupportedByGLReadPixels(GLenum type);
	bool isInternalFormatCompatibleWithType(GLenum type, GLenum internalformat);
	bool convertNormalizedUnsignedFixedPoint(int* src_input_rgba_bits, int* src_attachment_rgba_bits,
											 int* dst_attachment_rgba_bits, int* dst_output_rgba_bits, int* src_rgba,
											 int* dst_rgba);

	PixelData getAlpha8OESPixelData(GLenum type, unsigned char alpha);
	PixelData getLuminance8OESPixelData(GLenum type, unsigned char luminance);
	PixelData getLuminance8Alpha8OESPixelData(GLenum type, unsigned char luminance, unsigned char alpha);
	PixelData getR16IPixelData(int is_source_pixel, GLenum type, int red);
	PixelData getR16UIPixelData(int is_source_pixel, GLenum type, unsigned int red);
	PixelData getR32IPixelData(int is_source_pixel, GLenum type, int red);
	PixelData getR32UIPixelData(int is_source_pixel, GLenum type, unsigned int red);
	PixelData getR8IPixelData(int is_source_pixel, GLenum type, int red);
	PixelData getR8UIPixelData(int is_source_pixel, GLenum type, unsigned int red);
	PixelData getR8PixelData(int is_source_pixel, GLenum type, unsigned char red);
	PixelData getRG16IPixelData(int is_source_pixel, GLenum type, int red, int green);
	PixelData getRG16UIPixelData(int is_source_pixel, GLenum type, unsigned int red, unsigned int green);
	PixelData getRG32IPixelData(int is_source_pixel, GLenum type, int red, int green);
	PixelData getRG32UIPixelData(int is_source_pixel, GLenum type, unsigned int red, unsigned int green);
	PixelData getRG8IPixelData(int is_source_pixel, GLenum type, int red, int green);
	PixelData getRG8UIPixelData(int is_source_pixel, GLenum type, unsigned int red, unsigned int green);
	PixelData getRG8PixelData(int is_source_pixel, GLenum type, unsigned char red, unsigned char green);
	PixelData getRGB10A2PixelData(GLenum type, unsigned short red, unsigned short green, unsigned short blue,
								  unsigned char alpha);
	PixelData getRGB10A2UIPixelData(int is_source_pixel, GLenum type, unsigned int red, unsigned int green,
									unsigned int blue, unsigned int alpha);
	PixelData getRGB16IPixelData(int is_source_pixel, GLenum type, int red, int green, int blue);
	PixelData getRGB16UIPixelData(int is_source_pixel, GLenum type, unsigned int red, unsigned int green,
								  unsigned int blue);
	PixelData getRGB32IPixelData(int is_source_pixel, GLenum type, int red, int green, int blue);
	PixelData getRGB32UIPixelData(int is_source_pixel, GLenum type, unsigned int red, unsigned int green,
								  unsigned int blue);
	PixelData getRGB5A1PixelData(int is_source_pixel, GLenum type, unsigned int red, unsigned int green,
								 unsigned int blue, unsigned int alpha);
	PixelData getRGB565PixelData(int is_source_pixel, GLenum type, int red, int green, int blue);
	PixelData getRGB8PixelData(int is_source_pixel, GLenum type, unsigned char red, unsigned char green,
							   unsigned char blue);
	PixelData getRGB8IPixelData(int is_source_pixel, GLenum type, int red, int green, int blue);
	PixelData getRGB8UIPixelData(int is_source_pixel, GLenum type, unsigned int red, unsigned int green,
								 unsigned int blue);
	PixelData getRGBA16IPixelData(int is_source_pixel, GLenum type, int red, int green, int blue, int alpha);
	PixelData getRGBA16UIPixelData(int is_source_pixel, GLenum type, unsigned int red, unsigned int green,
								   unsigned int blue, unsigned int alpha);
	PixelData getRGBA32IPixelData(GLenum type, int red, int green, int blue, int alpha);

	PixelData getRGBA32UIPixelData(GLenum type, unsigned int red, unsigned int green, unsigned int blue,
								   unsigned int alpha);
	PixelData getRGBA8IPixelData(int is_source_pixel, GLenum type, int red, int green, int blue, int alpha);
	PixelData getRGBA8UIPixelData(int is_source_pixel, GLenum type, unsigned int red, unsigned int green,
								  unsigned int blue, unsigned int alpha);
	PixelData getRGBA4PixelData(int is_source_pixel, GLenum type, unsigned char red, unsigned char green,
								unsigned char blue, unsigned char alpha);
	PixelData getRGBA8PixelData(GLenum type, unsigned char red, unsigned char green, unsigned char blue,
								unsigned char alpha);
	PixelData getSRGB8Alpha8PixelData(GLenum type, unsigned char red, unsigned char green, unsigned char blue,
									  unsigned char alpha);
	PixelData getSRGB8PixelData(int is_source_pixel, GLenum type, unsigned char red, unsigned char green,
								unsigned char blue);
	PixelData getR16FPixelData(int is_source_pixel, GLenum type, float red);
	PixelData getR32FPixelData(int is_source_pixel, GLenum type, float red);
	PixelData getRG16FPixelData(int is_source_pixel, GLenum type, float red, float green);
	PixelData getRG32FPixelData(int is_source_pixel, GLenum type, float red, float green);
	PixelData getRGB16FPixelData(int is_source_pixel, GLenum type, float red, float green, float blue);
	PixelData getRGB32FPixelData(int is_source_pixel, GLenum type, float red, float green, float blue);
	PixelData getRGBA16FPixelData(GLenum type, float red, float green, float blue, float alpha);
	PixelData getRGBA32FPixelData(GLenum type, float red, float green, float blue, float alpha);

protected:
	void addEntryToConversionDatabase(PixelData src_topleft, PixelData dst_topleft, PixelData src_topright,
									  PixelData dst_topright, PixelData src_bottomleft, PixelData dst_bottomleft,
									  PixelData src_bottomright, PixelData dst_bottomright,
									  PixelCompareChannel channels_to_compare);
	void configureConversionDatabase();

public:
	// An array of _conversion_database_entry instances,
	// storing all known conversion rules.
	std::vector<ConversionDatabaseEntry> entries;

	// Amount of entries allocated in the "entries" array so far.
	unsigned int n_entries_allocated;

	// Amount of entries added to the "entries" array so far.
	unsigned int n_entries_added;
};

ConversionDatabase::ConversionDatabase() : n_entries_allocated(0), n_entries_added(0)
{
}

ConversionDatabase::~ConversionDatabase()
{
}

/** Initializes database instance. The database will be filled by the
 *  function with all available conversion rules, so it is a mistake to call
 *  ConfigureConversionDatabase() function for a handle reported by this function.
 *
 *  The handle should be released with ReleaseConversionDatabase() when no longer
 *  needed.
 *
 *  @return Handle to the newly created conversion database.
 **/
void ConversionDatabase::initializeDatabase()
{
	// Return when database was initialized earlier.
	if (!entries.empty())
		return;

	entries.resize(N_START_CONVERSION_DATABASE_ENTRIES);
	n_entries_allocated = N_START_CONVERSION_DATABASE_ENTRIES;
	n_entries_added		= 0;

	if (entries.empty())
		TCU_FAIL("Out of memory while pre-allocating space for conversion database entries");

	deMemset(&entries[0], DE_NULL, N_START_CONVERSION_DATABASE_ENTRIES * sizeof(ConversionDatabaseEntry));

	// Add all predefined entries that the test implementation is aware of
	configureConversionDatabase();
}

/** Tells whether @param type can be used for a gl.readPixels() call.
 *
 *  @param type GL type to consider.
 *
 *  @return true  if the type should be accepted by a gl.readPixels() call,
 *		  false otherwise.
 */
bool ConversionDatabase::isTypeSupportedByGLReadPixels(GLenum type)
{
	return (type == GL_INT) || (type == GL_UNSIGNED_BYTE) || (type == GL_UNSIGNED_INT) || (type == GL_FLOAT) ||
		   (type == GL_HALF_FLOAT) || (type == GL_UNSIGNED_INT_2_10_10_10_REV);
}

/** Tells whether @param type can be used with @param internalformat internal format.
 *
 *  @param type		   GLES type to consider.
 *  @param internalformat GLES internal format to consider.
 *
 *  @return true if the type is compatible with specific internal format, false otherwise.
 **/
bool ConversionDatabase::isInternalFormatCompatibleWithType(GLenum type, GLenum internalformat)
{
	bool result = false;

	switch (type)
	{
	case GL_INT:
	{
		result = (internalformat == GL_R8I) || (internalformat == GL_R16I) || (internalformat == GL_R32I) ||
				 (internalformat == GL_RG8I) || (internalformat == GL_RG16I) || (internalformat == GL_RG32I) ||
				 (internalformat == GL_RGB8I) || (internalformat == GL_RGB16I) || (internalformat == GL_RGB32I) ||
				 (internalformat == GL_RGBA8I) || (internalformat == GL_RGBA16I) || (internalformat == GL_RGBA32I);

		break;
	}

	case GL_UNSIGNED_BYTE:
	{
		result = (internalformat == GL_RGB) || (internalformat == GL_RGBA) || (internalformat == GL_LUMINANCE_ALPHA) ||
				 (internalformat == GL_LUMINANCE) || (internalformat == GL_LUMINANCE8_OES) ||
				 (internalformat == GL_LUMINANCE8_ALPHA8_OES) || (internalformat == GL_ALPHA) ||
				 (internalformat == GL_ALPHA8_OES) || (internalformat == GL_R8) || (internalformat == GL_R8_SNORM) ||
				 (internalformat == GL_RG8) || (internalformat == GL_RG8_SNORM) || (internalformat == GL_RGB8) ||
				 (internalformat == GL_SRGB8) || (internalformat == GL_RGB565) || (internalformat == GL_RGB8_SNORM) ||
				 (internalformat == GL_RGBA8) || (internalformat == GL_SRGB8_ALPHA8) ||
				 (internalformat == GL_RGBA8_SNORM) || (internalformat == GL_RGB5_A1) || (internalformat == GL_RGBA4);

		break;
	}

	case GL_UNSIGNED_INT:
	{
		result = (internalformat == GL_R8UI) || (internalformat == GL_R16UI) || (internalformat == GL_R32UI) ||
				 (internalformat == GL_RG8UI) || (internalformat == GL_RG16UI) || (internalformat == GL_RG32UI) ||
				 (internalformat == GL_RGB8UI) || (internalformat == GL_RGB10_A2UI) || (internalformat == GL_RGB16UI) ||
				 (internalformat == GL_RGB32UI) || (internalformat == GL_RGBA8UI) || (internalformat == GL_RGBA16UI) ||
				 (internalformat == GL_RGBA32UI);

		break;
	}

	case GL_UNSIGNED_INT_2_10_10_10_REV:
	{
		result = (internalformat == GL_RGB10_A2) || (internalformat == GL_RGB10_A2UI);

		break;
	}

	case GL_FLOAT:
	{
		result = (internalformat == GL_RGB) || (internalformat == GL_RGBA) || (internalformat == GL_R32F) ||
				 (internalformat == GL_RG32F) || (internalformat == GL_RGB32F) || (internalformat == GL_RGBA32F);

		break;
	}

	case GL_HALF_FLOAT:
	{
		result = (internalformat == GL_RGB) || (internalformat == GL_RGBA) || (internalformat == GL_R16F) ||
				 (internalformat == GL_RG16F) || (internalformat == GL_RGB16F) || (internalformat == GL_RGBA16F);

		break;
	}

	default:
	{
		TCU_FAIL("Unsupported type");
	}
	}

	return result;
}

/** Converts normalized unsigned fixed-point RGBA pixel representations
 *  from one resolution to another, simulating the result that one would
 *  get if glCopyTexImage2D() call was used for a single pixel, read
 *  afterward with a gl.readPixels() call.
 *
 *  @param src_input_rgba_bits	  Pointer to an array storing 4 integers, representing
 *								  amount of bits per channel, as used by input data,
 *								  that will be fed to a GL object using gl.texImage2D()
 *								  call or similar. Cannot be NULL.
 *  @param src_attachment_rgba_bits Pointer to an array storing 4 integers, representing
 *								  amount of bits per channel, as used by data storage
 *								  of an object attached to read buffer. Cannot be NULL.
 *  @param dst_attachment_rgba_bits Pointer to an array storing 4 integers, representing
 *								  amount of bits per channel, as used by data storage
 *								  of a texture object that glCopyTexImage2D() call will
 *								  initialize. Cannot be NULL.
 *  @param dst_output_rgba_bits	 Pointer to an array storing 4 integers, representing
 *								  amount of bits per channel, as requested by the user
 *								  using the gl.readPixels() call. Cannot be NULL.
 *  @param src_rgba				 Pointer to an array storing 4 values representing
 *								  RGBA channel. It is assumed the values do not exceed
 *								  allowed precision, described by @param src_input_rgba_bits.
 *								  Cannot be NULL.
 *  @param dst_rgba				 Deref will be used to store result of the conversion.
 *								  Cannot be NULL.
 *
 *  @return 1 if successful, 0 otherwise.
 *  */
bool ConversionDatabase::convertNormalizedUnsignedFixedPoint(int* src_input_rgba_bits, int* src_attachment_rgba_bits,
															 int* dst_attachment_rgba_bits, int* dst_output_rgba_bits,
															 int* src_rgba, int* dst_rgba)
{
	float a_f32					   = 0.0f;
	float b_f32					   = 0.0f;
	float dst_rgba_f[4]			   = { 0.0f };
	float g_f32					   = 0.0f;
	float r_f32					   = 0.0f;
	int   src_rgba_intermediate[4] = { src_rgba[0], src_rgba[1], src_rgba[2], src_rgba[3] };

	// Reduce or crank up precision before casting to floats
	int bit_diffs_src_intermediate[] = { abs(src_input_rgba_bits[0] - src_attachment_rgba_bits[0]),
										 abs(src_input_rgba_bits[1] - src_attachment_rgba_bits[1]),
										 abs(src_input_rgba_bits[2] - src_attachment_rgba_bits[2]),
										 abs(src_input_rgba_bits[3] - src_attachment_rgba_bits[3]) };

	for (unsigned int n = 0; n < sizeof(bit_diffs_src_intermediate) / sizeof(bit_diffs_src_intermediate[0]); ++n)
	{
		float tmp = ((float)src_rgba_intermediate[n]) / ((1 << src_input_rgba_bits[n]) - 1);
		if (tmp > 1.0f)
			tmp = 1.0f;
		tmp *= (float)((1 << src_attachment_rgba_bits[n]) - 1);
		src_rgba_intermediate[n] = (int)(0.5 + tmp);
	}

	// The following equations correspond to equation 2.1 from ES spec 3.0.2
	r_f32 = ((float)src_rgba_intermediate[0]) / (float)((1 << src_attachment_rgba_bits[0]) - 1);
	g_f32 = ((float)src_rgba_intermediate[1]) / (float)((1 << src_attachment_rgba_bits[1]) - 1);
	b_f32 = ((float)src_rgba_intermediate[2]) / (float)((1 << src_attachment_rgba_bits[2]) - 1);
	a_f32 = ((float)src_rgba_intermediate[3]) / (float)((1 << src_attachment_rgba_bits[3]) - 1);

	// Clamp to <0, 1>. Since we're dealing with unsigned ints on input, there's
	// no way we could be lower than 0.
	if (r_f32 > 1.0f)
		r_f32 = 1.0f;
	if (g_f32 > 1.0f)
		g_f32 = 1.0f;
	if (b_f32 > 1.0f)
		b_f32 = 1.0f;
	if (a_f32 > 1.0f)
		a_f32 = 1.0f;

	// The following equations are taken from table 4.5 & equation 2.3,
	// ES spec 3.0.2
	dst_rgba_f[0] = (r_f32 * (float)((1 << dst_attachment_rgba_bits[0]) - 1));
	dst_rgba_f[1] = (g_f32 * (float)((1 << dst_attachment_rgba_bits[1]) - 1));
	dst_rgba_f[2] = (b_f32 * (float)((1 << dst_attachment_rgba_bits[2]) - 1));
	dst_rgba_f[3] = (a_f32 * (float)((1 << dst_attachment_rgba_bits[3]) - 1));

	// As per spec:
	//
	// The conversion from a floating-point value f to the corresponding
	// unsigned normalized fixed-point value c is defined by first clamping
	// f to the range [0,1], then computing
	//
	// f' = convert_float_uint(f * (2^b-1), b) [2.3]
	//
	// where convert_float_uint(r,b) returns one of the two unsigned binary
	// integer values with exactly b bits which are closest to the floating-point
	// value r (where *rounding to nearest is preferred*)
	//
	// C casting truncates the remainder, so if dst_rgba_f[x] is larger than or
	// equal to 0.5, we need to take a ceiling of the value.
	for (unsigned int n = 0; n < 4 /* channels */; ++n)
	{
		if (deFloatMod(dst_rgba_f[n], 1.0f) >= 0.5f)
			dst_rgba_f[n] = deFloatCeil(dst_rgba_f[n]);
	}

	// Expand the data or reduce its precision, depending on the type requested by the caller.
	dst_rgba[0] = ((unsigned int)dst_rgba_f[0]);
	dst_rgba[1] = ((unsigned int)dst_rgba_f[1]);
	dst_rgba[2] = ((unsigned int)dst_rgba_f[2]);
	dst_rgba[3] = ((unsigned int)dst_rgba_f[3]);

	for (unsigned int n = 0; n < 4 /* channels */; ++n)
	{
		float tmp = ((float)dst_rgba[n]) / ((1 << dst_attachment_rgba_bits[n]) - 1);
		if (tmp > 1.0f)
			tmp = 1.0f;
		tmp *= (float)((1 << dst_output_rgba_bits[n]) - 1);
		dst_rgba[n] = (int)(0.5 + tmp);
	}

	return true;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_ALPHA8 internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be GL_UNSIGNED_BYTE.
 *  @param red			 Value for red channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getAlpha8OESPixelData(GLenum type, unsigned char alpha)
{
	PixelData result;

	// Sanity checks
	DE_ASSERT(type == GL_UNSIGNED_BYTE);

	// Carry on
	deMemset(&result, 0, sizeof(result));

	result.alpha.unsigned_byte_data = alpha;
	result.alpha.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
	result.blue.data_type			= CHANNEL_DATA_TYPE_NONE;
	result.green.data_type			= CHANNEL_DATA_TYPE_NONE;
	result.red.data_type			= CHANNEL_DATA_TYPE_NONE;
	result.data_internalformat		= GL_ALPHA8_OES;
	result.data_type				= type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_LUMINANCE8 internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be GL_UNSIGNED_BYTE.
 *  @param luminance	   Luminance value. Will get cloned to blue/green/red channels.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getLuminance8OESPixelData(GLenum type, unsigned char luminance)
{
	PixelData result;

	/* Sanity checks */
	DE_ASSERT(type == GL_UNSIGNED_BYTE);

	/* Carry on */
	deMemset(&result, 0, sizeof(result));

	result.alpha.unsigned_byte_data = 255;
	result.alpha.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
	result.blue.unsigned_byte_data  = luminance;
	result.blue.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
	result.green.unsigned_byte_data = luminance;
	result.green.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
	result.red.unsigned_byte_data   = luminance;
	result.red.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
	result.data_internalformat		= GL_LUMINANCE8_OES;
	result.data_type				= type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_LUMINANCE8_ALPHA8 internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be GL_UNSIGNED_BYTE.
 *  @param luminance	   Luminance value. Will be cloned to blue/green/red channels.
 *  @param alpha		   Alpha channel value.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getLuminance8Alpha8OESPixelData(GLenum type, unsigned char luminance, unsigned char alpha)
{
	PixelData result;

	/* Sanity checks */
	DE_ASSERT(type == GL_UNSIGNED_BYTE);

	/* Carry on */
	deMemset(&result, 0, sizeof(result));

	result.alpha.unsigned_byte_data = alpha;
	result.alpha.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
	result.blue.unsigned_byte_data  = luminance;
	result.blue.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
	result.green.unsigned_byte_data = luminance;
	result.green.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
	result.red.unsigned_byte_data   = luminance;
	result.red.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
	result.data_internalformat		= GL_LUMINANCE8_ALPHA8_OES;
	result.data_type				= type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_R16I internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be:
 *						 1) GL_SHORT for source pixels.
 *						 2) GL_INT for destination pixels.
 *  @param red			 Value for red channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getR16IPixelData(int is_source_pixel, GLenum type, int red)
{
	PixelData result;

	/* Sanity checks */
	if (is_source_pixel)
	{
		DE_ASSERT(type == GL_SHORT);
	} /* if (is_source_pixel) */
	else
	{
		DE_ASSERT(type == GL_INT);
	}

	/* Carry on */
	deMemset(&result, 0, sizeof(result));

	result.blue.data_type  = CHANNEL_DATA_TYPE_NONE;
	result.green.data_type = CHANNEL_DATA_TYPE_NONE;

	if (is_source_pixel)
	{
		result.red.signed_short_data = red;
		result.red.data_type		 = CHANNEL_DATA_TYPE_SIGNED_SHORT_16BITS;
	} /* if (is_source_pixel) */
	else
	{
		result.alpha.signed_integer_data = 1;
		result.alpha.data_type			 = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;
		result.red.signed_integer_data   = red;
		result.red.data_type			 = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;
	}

	result.data_internalformat = GL_R16I;
	result.data_type		   = type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_R16UI internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be:
 *						 1) GL_UNSIGNED_SHORT for source pixels.
 *						 2) GL_UNSIGNED_INT for destination pixels.
 *  @param red			 Value for red channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getR16UIPixelData(int is_source_pixel, GLenum type, unsigned int red)
{
	PixelData result;

	/* Sanity checks */
	if (is_source_pixel)
	{
		DE_ASSERT(type == GL_UNSIGNED_SHORT);
	} /* if (is_source_pixels) */
	else
	{
		DE_ASSERT(type == GL_UNSIGNED_INT);
	}

	deMemset(&result, 0, sizeof(result));

	result.alpha.data_type = CHANNEL_DATA_TYPE_NONE;
	result.blue.data_type  = CHANNEL_DATA_TYPE_NONE;
	result.green.data_type = CHANNEL_DATA_TYPE_NONE;

	if (is_source_pixel)
	{
		result.red.unsigned_short_data = red;
		result.red.data_type		   = CHANNEL_DATA_TYPE_UNSIGNED_SHORT_16BITS;
	} /* if (is_source_pixel) */
	else
	{
		result.alpha.unsigned_integer_data = 1;
		result.alpha.data_type			   = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
		result.red.unsigned_integer_data   = red;
		result.red.data_type			   = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
	}

	result.data_internalformat = GL_R16UI;
	result.data_type		   = type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_R32I internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be GL_INT.
 *  @param red			 Value for red channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getR32IPixelData(int is_source_pixel, GLenum type, int red)
{
	PixelData result;

	DE_ASSERT(type == GL_INT);

	deMemset(&result, 0, sizeof(result));

	if (!is_source_pixel)
	{
		result.alpha.data_type			 = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;
		result.alpha.signed_integer_data = 1;
	}
	else
	{
		result.alpha.data_type = CHANNEL_DATA_TYPE_NONE;
	}

	result.blue.data_type		   = CHANNEL_DATA_TYPE_NONE;
	result.green.data_type		   = CHANNEL_DATA_TYPE_NONE;
	result.red.signed_integer_data = red;
	result.red.data_type		   = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;
	result.data_internalformat	 = GL_R32I;
	result.data_type			   = type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_R32UI internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be GL_UNSIGNED_INT.
 *  @param red			 Value for red channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getR32UIPixelData(int is_source_pixel, GLenum type, unsigned int red)
{
	PixelData result;

	DE_ASSERT(type == GL_UNSIGNED_INT);

	deMemset(&result, 0, sizeof(result));

	if (!is_source_pixel)
	{
		result.alpha.data_type			   = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
		result.alpha.unsigned_integer_data = 1;
	} /* if (!is_source_pixel) */
	else
	{
		result.alpha.data_type = CHANNEL_DATA_TYPE_NONE;
	}

	result.blue.data_type			 = CHANNEL_DATA_TYPE_NONE;
	result.green.data_type			 = CHANNEL_DATA_TYPE_NONE;
	result.red.unsigned_integer_data = red;
	result.red.data_type			 = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
	result.data_internalformat		 = GL_R32UI;
	result.data_type				 = type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_R8I internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be:
 *						 1) GL_BYTE for source pixels.
 *						 2) GL_INT for destination pixels.
 *  @param red			 Value for red channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getR8IPixelData(int is_source_pixel, GLenum type, int red)
{
	PixelData result;

	// Sanity checks
	if (is_source_pixel)
		DE_ASSERT(type == GL_BYTE);
	else
		DE_ASSERT(type == GL_INT);

	// Carry on
	deMemset(&result, 0, sizeof(result));

	result.blue.data_type  = CHANNEL_DATA_TYPE_NONE;
	result.green.data_type = CHANNEL_DATA_TYPE_NONE;

	if (is_source_pixel)
	{
		result.alpha.data_type		= CHANNEL_DATA_TYPE_NONE;
		result.red.signed_byte_data = red;
		result.red.data_type		= CHANNEL_DATA_TYPE_SIGNED_BYTE_8BITS;
	}
	else
	{
		result.alpha.data_type			 = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;
		result.alpha.signed_integer_data = 1;
		result.red.data_type			 = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;
		result.red.signed_integer_data   = red;
	}

	result.data_internalformat = GL_R8I;
	result.data_type		   = type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_R8UI internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be:
 *						 1) GL_UNSIGNED_BYTE for source pixels.
 *						 2) GL_UNSIGNED_INT for destination pixels.
 *  @param red			 Value for red channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getR8UIPixelData(int is_source_pixel, GLenum type, unsigned int red)
{
	PixelData result;

	/* Sanity checks */
	if (is_source_pixel)
		DE_ASSERT(type == GL_UNSIGNED_BYTE);
	else
		DE_ASSERT(type == GL_UNSIGNED_INT);

	deMemset(&result, 0, sizeof(result));

	result.blue.data_type  = CHANNEL_DATA_TYPE_NONE;
	result.green.data_type = CHANNEL_DATA_TYPE_NONE;

	if (is_source_pixel)
	{
		result.alpha.data_type		  = CHANNEL_DATA_TYPE_NONE;
		result.red.unsigned_byte_data = red;
		result.red.data_type		  = CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
	}
	else
	{
		result.alpha.data_type			   = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
		result.alpha.unsigned_integer_data = 1;
		result.red.data_type			   = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
		result.red.unsigned_integer_data   = red;
	}

	result.data_internalformat = GL_R8UI;
	result.data_type		   = type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_R8 internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must beGL_UNSIGNED_BYTE.
 *  @param red			 Value for red channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getR8PixelData(int is_source_pixel, GLenum type, unsigned char red)
{
	PixelData result;

	DE_ASSERT(type == GL_UNSIGNED_BYTE);
	deMemset(&result, 0, sizeof(result));

	if (is_source_pixel)
	{
		result.alpha.data_type = CHANNEL_DATA_TYPE_NONE;
	}
	else
	{
		result.alpha.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
		result.alpha.unsigned_byte_data = 255;
	}

	result.blue.data_type		  = CHANNEL_DATA_TYPE_NONE;
	result.green.data_type		  = CHANNEL_DATA_TYPE_NONE;
	result.red.unsigned_byte_data = red;
	result.red.data_type		  = CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
	result.data_internalformat	= GL_R8;
	result.data_type			  = type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_RG16I internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be:
 *						 1) GL_SHORT for source pixels.
 *						 2) GL_INT for destination pixels.
 *  @param red			 Value for red channel.
 *  @param green		   Value for green channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getRG16IPixelData(int is_source_pixel, GLenum type, int red, int green)
{
	PixelData result;

	if (is_source_pixel)
	{
		DE_ASSERT(type == GL_SHORT);
	}
	else
	{
		DE_ASSERT(type == GL_INT);
	}

	deMemset(&result, 0, sizeof(result));

	result.blue.data_type = CHANNEL_DATA_TYPE_NONE;

	if (is_source_pixel)
	{
		result.alpha.data_type		   = CHANNEL_DATA_TYPE_NONE;
		result.green.signed_short_data = green;
		result.green.data_type		   = CHANNEL_DATA_TYPE_SIGNED_SHORT_16BITS;
		result.red.signed_short_data   = red;
		result.red.data_type		   = CHANNEL_DATA_TYPE_SIGNED_SHORT_16BITS;
	}
	else
	{
		result.alpha.data_type			 = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;
		result.alpha.signed_integer_data = 1;
		result.green.data_type			 = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;
		result.green.signed_integer_data = green;
		result.red.data_type			 = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;
		result.red.signed_integer_data   = red;
	}

	result.data_internalformat = GL_RG16I;
	result.data_type		   = type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_RG16UI internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be:
 *						 1) GL_UNSIGNED_SHORT for source pixels.
 *						 2) GL_UNSIGNED_INT for destination pixels.
 *  @param red			 Value for red channel.
 *  @param green		   Value for green channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getRG16UIPixelData(int is_source_pixel, GLenum type, unsigned int red, unsigned int green)
{
	PixelData result;

	if (is_source_pixel)
		DE_ASSERT(type == GL_UNSIGNED_SHORT);
	else
		DE_ASSERT(type == GL_UNSIGNED_INT);

	deMemset(&result, 0, sizeof(result));

	result.blue.data_type = CHANNEL_DATA_TYPE_NONE;

	if (is_source_pixel)
	{
		result.alpha.data_type		   = CHANNEL_DATA_TYPE_NONE;
		result.green.signed_short_data = green;
		result.green.data_type		   = CHANNEL_DATA_TYPE_UNSIGNED_SHORT_16BITS;
		result.red.signed_short_data   = red;
		result.red.data_type		   = CHANNEL_DATA_TYPE_UNSIGNED_SHORT_16BITS;
	}
	else
	{
		result.alpha.data_type			   = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
		result.alpha.unsigned_integer_data = 1;
		result.green.data_type			   = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
		result.green.unsigned_integer_data = green;
		result.red.data_type			   = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
		result.red.unsigned_integer_data   = red;
	}

	result.data_internalformat = GL_RG16UI;
	result.data_type		   = type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_RG32I internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be GL_INT.
 *  @param red			 Value for red channel.
 *  @param green		   Value for green channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getRG32IPixelData(int is_source_pixel, GLenum type, int red, int green)
{
	PixelData result;

	DE_ASSERT(type == GL_INT);

	deMemset(&result, 0, sizeof(result));

	if (is_source_pixel)
		result.alpha.data_type = CHANNEL_DATA_TYPE_NONE;
	else
	{
		result.alpha.data_type			 = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;
		result.alpha.signed_integer_data = 1;
	}

	result.blue.data_type			 = CHANNEL_DATA_TYPE_NONE;
	result.green.signed_integer_data = green;
	result.green.data_type			 = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;
	result.red.signed_integer_data   = red;
	result.red.data_type			 = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;
	result.data_internalformat		 = GL_RG32I;
	result.data_type				 = type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_RG32UI internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be GL_UNSIGNED_INT.
 *  @param red			 Value for red channel.
 *  @param green		   Value for green channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getRG32UIPixelData(int is_source_pixel, GLenum type, unsigned int red, unsigned int green)
{
	PixelData result;

	DE_ASSERT(type == GL_UNSIGNED_INT);

	deMemset(&result, 0, sizeof(result));

	if (is_source_pixel)
	{
		result.alpha.data_type = CHANNEL_DATA_TYPE_NONE;
	}
	else
	{
		result.alpha.data_type			   = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
		result.alpha.unsigned_integer_data = 1;
	}

	result.blue.data_type			   = CHANNEL_DATA_TYPE_NONE;
	result.green.unsigned_integer_data = green;
	result.green.data_type			   = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
	result.red.unsigned_integer_data   = red;
	result.red.data_type			   = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
	result.data_internalformat		   = GL_RG32UI;
	result.data_type				   = type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_RG8I internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be:
 *						 1) GL_BYTE for source pixels.
 *						 2) GL_INT for destination pixels.
 *  @param red			 Value for red channel.
 *  @param green		   Value for green channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getRG8IPixelData(int is_source_pixel, GLenum type, int red, int green)
{
	PixelData result;

	if (is_source_pixel)
		DE_ASSERT(type == GL_BYTE);
	else
		DE_ASSERT(type == GL_INT);

	deMemset(&result, 0, sizeof(result));

	result.blue.data_type = CHANNEL_DATA_TYPE_NONE;

	if (is_source_pixel)
	{
		result.alpha.data_type		  = CHANNEL_DATA_TYPE_NONE;
		result.green.signed_byte_data = green;
		result.green.data_type		  = CHANNEL_DATA_TYPE_SIGNED_BYTE_8BITS;
		result.red.signed_byte_data   = red;
		result.red.data_type		  = CHANNEL_DATA_TYPE_SIGNED_BYTE_8BITS;
	} /* if (is_source_pixel) */
	else
	{
		result.alpha.data_type			 = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;
		result.alpha.signed_integer_data = 1;
		result.green.data_type			 = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;
		result.green.signed_integer_data = green;
		result.red.data_type			 = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;
		result.red.signed_integer_data   = red;
	}

	result.data_internalformat = GL_RG8I;
	result.data_type		   = type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_RGB8UI internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be:
 *						 1) GL_UNSIGNED_BYTE for source pixels.
 *						 2) GL_UNSIGNED_INT for destination pixels.
 *  @param red			 Value for red channel.
 *  @param green		   Value for green channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getRG8UIPixelData(int is_source_pixel, GLenum type, unsigned int red, unsigned int green)
{
	PixelData result;

	if (is_source_pixel)
		DE_ASSERT(type == GL_UNSIGNED_BYTE);
	else
		DE_ASSERT(type == GL_UNSIGNED_INT);

	deMemset(&result, 0, sizeof(result));

	result.blue.data_type = CHANNEL_DATA_TYPE_NONE;

	if (is_source_pixel)
	{
		result.alpha.data_type			= CHANNEL_DATA_TYPE_NONE;
		result.green.unsigned_byte_data = green;
		result.green.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
		result.red.unsigned_byte_data   = red;
		result.red.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
	}
	else
	{
		result.alpha.data_type			   = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
		result.alpha.unsigned_integer_data = 1;
		result.green.data_type			   = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
		result.green.unsigned_integer_data = green;
		result.red.data_type			   = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
		result.red.unsigned_integer_data   = red;
	}

	result.data_internalformat = GL_RG8UI;
	result.data_type		   = type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_RG8 internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be GL_UNSIGNED_BYTE.
 *  @param red			 Value for red channel.
 *  @param green		   Value for green channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getRG8PixelData(int is_source_pixel, GLenum type, unsigned char red, unsigned char green)
{
	PixelData result;

	DE_ASSERT(type == GL_UNSIGNED_BYTE);

	deMemset(&result, 0, sizeof(result));

	if (is_source_pixel)
	{
		result.alpha.data_type = CHANNEL_DATA_TYPE_NONE;
	}
	else
	{
		result.alpha.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
		result.alpha.unsigned_byte_data = 255;
	}

	result.blue.data_type			= CHANNEL_DATA_TYPE_NONE;
	result.green.unsigned_byte_data = green;
	result.green.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
	result.red.unsigned_byte_data   = red;
	result.red.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
	result.data_internalformat		= GL_RG8;
	result.data_type				= type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_RGB10_A2 internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be GL_UNSIGNED_INT_2_10_10_10_REV.
 *  @param red			 Value for red channel.
 *  @param green		   Value for green channel.
 *  @param blue			Value for blue channel.
 *  @param alpha		   Value for alpha channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getRGB10A2PixelData(GLenum type, unsigned short red, unsigned short green,
												  unsigned short blue, unsigned char alpha)
{
	PixelData result;

	DE_ASSERT(red <= 1023);
	DE_ASSERT(green <= 1023);
	DE_ASSERT(blue <= 1023);
	DE_ASSERT(alpha <= 3);

	DE_ASSERT(type == GL_UNSIGNED_INT_2_10_10_10_REV);

	deMemset(&result, 0, sizeof(result));

	result.alpha.unsigned_byte_data  = alpha;
	result.alpha.data_type			 = CHANNEL_DATA_TYPE_UNSIGNED_BYTE_2BITS;
	result.blue.unsigned_short_data  = blue;
	result.blue.data_type			 = CHANNEL_DATA_TYPE_UNSIGNED_SHORT_10BITS;
	result.green.unsigned_short_data = green;
	result.green.data_type			 = CHANNEL_DATA_TYPE_UNSIGNED_SHORT_10BITS;
	result.red.unsigned_short_data   = red;
	result.red.data_type			 = CHANNEL_DATA_TYPE_UNSIGNED_SHORT_10BITS;
	result.data_internalformat		 = GL_RGB10_A2;
	result.data_type				 = type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_RGB10A2UI internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be:
 *						 1) GL_UNSIGNED_INT_2_10_10_10_REV for source pixels.
 *						 2) GL_UNSIGNED_INT for destination pixels.
 *  @param red			 Value for red channel.
 *  @param green		   Value for green channel.
 *  @param blue			Value for blue channel.
 *  @param alpha		   Value for alpha channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getRGB10A2UIPixelData(int is_source_pixel, GLenum type, unsigned int red,
													unsigned int green, unsigned int blue, unsigned int alpha)
{
	PixelData result;

	if (is_source_pixel)
		DE_ASSERT(type == GL_UNSIGNED_INT_2_10_10_10_REV);
	else
		DE_ASSERT(type == GL_UNSIGNED_INT);

	deMemset(&result, 0, sizeof(result));

	if (is_source_pixel)
	{
		result.alpha.unsigned_byte_data  = alpha;
		result.alpha.data_type			 = CHANNEL_DATA_TYPE_UNSIGNED_BYTE_2BITS;
		result.blue.unsigned_short_data  = blue;
		result.blue.data_type			 = CHANNEL_DATA_TYPE_UNSIGNED_SHORT_10BITS;
		result.green.unsigned_short_data = green;
		result.green.data_type			 = CHANNEL_DATA_TYPE_UNSIGNED_SHORT_10BITS;
		result.red.unsigned_short_data   = red;
		result.red.data_type			 = CHANNEL_DATA_TYPE_UNSIGNED_SHORT_10BITS;
	}
	else
	{
		result.alpha.unsigned_integer_data = alpha;
		result.alpha.data_type			   = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
		result.blue.unsigned_integer_data  = blue;
		result.blue.data_type			   = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
		result.green.unsigned_integer_data = green;
		result.green.data_type			   = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
		result.red.unsigned_integer_data   = red;
		result.red.data_type			   = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
	}

	result.data_internalformat = GL_RGB10_A2UI;
	result.data_type		   = type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_RGB16I internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be:
 *						 1) GL_SHORT for source pixels.
 *						 2) GL_INT for destination pixels.
 *  @param red			 Value for red channel.
 *  @param green		   Value for green channel.
 *  @param blue			Value for blue channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getRGB16IPixelData(int is_source_pixel, GLenum type, int red, int green, int blue)
{
	PixelData result;

	if (is_source_pixel)
		DE_ASSERT(type == GL_SHORT);
	else
		DE_ASSERT(type == GL_INT);

	deMemset(&result, 0, sizeof(result));

	if (is_source_pixel)
	{
		result.alpha.data_type		   = CHANNEL_DATA_TYPE_NONE;
		result.blue.data_type		   = CHANNEL_DATA_TYPE_SIGNED_SHORT_16BITS;
		result.blue.signed_short_data  = blue;
		result.green.data_type		   = CHANNEL_DATA_TYPE_SIGNED_SHORT_16BITS;
		result.green.signed_short_data = green;
		result.red.data_type		   = CHANNEL_DATA_TYPE_SIGNED_SHORT_16BITS;
		result.red.signed_short_data   = red;
	}
	else
	{
		result.alpha.data_type			 = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;
		result.alpha.signed_integer_data = 1;
		result.blue.data_type			 = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;
		result.blue.signed_integer_data  = blue;
		result.green.data_type			 = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;
		result.green.signed_integer_data = green;
		result.red.data_type			 = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;
		result.red.signed_integer_data   = red;
	}

	result.data_internalformat = GL_RGB16I;
	result.data_type		   = type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_RGB16UI internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be:
 *						 1) GL_UNSIGNED_SHORT for source pixels.
 *						 2) GL_UNSIGNED_INT for destination pixels.
 *  @param red			 Value for red channel.
 *  @param green		   Value for green channel.
 *  @param blue			Value for blue channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getRGB16UIPixelData(int is_source_pixel, GLenum type, unsigned int red,
												  unsigned int green, unsigned int blue)
{
	PixelData result;

	if (is_source_pixel)
		DE_ASSERT(type == GL_UNSIGNED_SHORT);
	else
		DE_ASSERT(type == GL_UNSIGNED_INT);

	deMemset(&result, 0, sizeof(result));

	if (is_source_pixel)
	{
		result.alpha.data_type			 = CHANNEL_DATA_TYPE_NONE;
		result.blue.data_type			 = CHANNEL_DATA_TYPE_UNSIGNED_SHORT_16BITS;
		result.blue.unsigned_short_data  = blue;
		result.green.data_type			 = CHANNEL_DATA_TYPE_UNSIGNED_SHORT_16BITS;
		result.green.unsigned_short_data = green;
		result.red.data_type			 = CHANNEL_DATA_TYPE_UNSIGNED_SHORT_16BITS;
		result.red.unsigned_short_data   = red;
	}
	else
	{
		result.alpha.data_type			   = CHANNEL_DATA_TYPE_NONE;
		result.alpha.unsigned_integer_data = 1;
		result.blue.data_type			   = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
		result.blue.unsigned_integer_data  = blue;
		result.green.data_type			   = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
		result.green.unsigned_integer_data = green;
		result.red.data_type			   = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
		result.red.unsigned_integer_data   = red;
	}

	result.data_internalformat = GL_RGB16UI;
	result.data_type		   = type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_RGB32I internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be GL_INT.
 *  @param red			 Value for red channel.
 *  @param green		   Value for green channel.
 *  @param blue			Value for blue channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getRGB32IPixelData(int is_source_pixel, GLenum type, int red, int green, int blue)
{
	PixelData result;

	DE_ASSERT(type == GL_INT);

	deMemset(&result, 0, sizeof(result));

	if (is_source_pixel)
	{
		result.alpha.data_type = CHANNEL_DATA_TYPE_NONE;
	}
	else
	{
		result.alpha.data_type			 = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;
		result.alpha.signed_integer_data = 1;
	}

	result.blue.data_type			 = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;
	result.blue.signed_integer_data  = blue;
	result.green.data_type			 = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;
	result.green.signed_integer_data = green;
	result.red.data_type			 = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;
	result.red.signed_integer_data   = red;
	result.data_internalformat		 = GL_RGB32I;
	result.data_type				 = type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_RGB32UI internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be GL_UNSIGNED_INT.
 *  @param red			 Value for red channel.
 *  @param green		   Value for green channel.
 *  @param blue			Value for blue channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getRGB32UIPixelData(int is_source_pixel, GLenum type, unsigned int red,
												  unsigned int green, unsigned int blue)
{
	PixelData result;

	DE_ASSERT(type == GL_UNSIGNED_INT);

	deMemset(&result, 0, sizeof(result));

	if (is_source_pixel)
	{
		result.alpha.data_type = CHANNEL_DATA_TYPE_NONE;
	}
	else
	{
		result.alpha.data_type			   = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
		result.alpha.unsigned_integer_data = 1;
	}

	result.blue.data_type			   = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
	result.blue.unsigned_integer_data  = blue;
	result.green.data_type			   = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
	result.green.unsigned_integer_data = green;
	result.red.data_type			   = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
	result.red.unsigned_integer_data   = red;
	result.data_internalformat		   = GL_RGB32UI;
	result.data_type				   = type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_RGB5A1 internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be:
 *						 1) GL_UNSIGNED_BYTE or GL_UNSIGNED_SHORT_5_5_5_1 or
 *							GL_UNSIGNED_INT_2_10_10_10_REV for source pixels.
 *						 2) GL_UNSIGNED_BYTE for destination pixels.
 *  @param red			 Value for red channel.
 *  @param green		   Value for green channel.
 *  @param blue			Value for blue channel.
 *  @param alpha		   Value for alpha channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getRGB5A1PixelData(int is_source_pixel, GLenum type, unsigned int red, unsigned int green,
												 unsigned int blue, unsigned int alpha)
{
	PixelData result;

	if (is_source_pixel)
	{
		DE_ASSERT(type == GL_UNSIGNED_BYTE || type == GL_UNSIGNED_SHORT_5_5_5_1 ||
				  type == GL_UNSIGNED_INT_2_10_10_10_REV);
	}
	else
	{
		DE_ASSERT(type == GL_UNSIGNED_BYTE);
	}

	deMemset(&result, 0, sizeof(result));

	switch (type)
	{
	case GL_UNSIGNED_BYTE:
	{
		DE_ASSERT(red <= 255);
		DE_ASSERT(green <= 255);
		DE_ASSERT(blue <= 255);
		DE_ASSERT(alpha <= 255);

		// Fill the channel data structures
		result.alpha.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
		result.alpha.unsigned_byte_data = alpha;
		result.blue.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
		result.blue.unsigned_byte_data  = blue;
		result.green.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
		result.green.unsigned_byte_data = green;
		result.red.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
		result.red.unsigned_byte_data   = red;

		break;
	}

	case GL_UNSIGNED_SHORT_5_5_5_1:
	{
		DE_ASSERT(red <= 31);
		DE_ASSERT(green <= 31);
		DE_ASSERT(blue <= 31);
		DE_ASSERT(alpha == 0 || alpha == 1);

		// Fill the channel data structures
		result.alpha.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_1BIT;
		result.alpha.unsigned_byte_data = alpha;
		result.blue.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_5BITS;
		result.blue.unsigned_byte_data  = blue;
		result.green.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_5BITS;
		result.green.unsigned_byte_data = green;
		result.red.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_5BITS;
		result.red.unsigned_byte_data   = red;

		break;
	}

	case GL_UNSIGNED_INT_2_10_10_10_REV:
	{
		// Sanity checks
		DE_ASSERT(red <= 1023);
		DE_ASSERT(green <= 1023);
		DE_ASSERT(blue <= 1023);
		DE_ASSERT(alpha <= 3);

		// Fill the channel data structures
		result.alpha.data_type			 = CHANNEL_DATA_TYPE_UNSIGNED_BYTE_2BITS;
		result.alpha.unsigned_byte_data  = alpha;
		result.blue.data_type			 = CHANNEL_DATA_TYPE_UNSIGNED_SHORT_10BITS;
		result.blue.unsigned_short_data  = blue;
		result.green.data_type			 = CHANNEL_DATA_TYPE_UNSIGNED_SHORT_10BITS;
		result.green.unsigned_short_data = green;
		result.red.data_type			 = CHANNEL_DATA_TYPE_UNSIGNED_SHORT_10BITS;
		result.red.unsigned_short_data   = red;

		break;
	}
	}

	result.data_internalformat = GL_RGB5_A1;
	result.data_type		   = type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_RGB565 internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be:
 *						 1) GL_UNSIGNED_BYTE or GL_UNSIGNED_SHORT_5_6_5 for source pixels.
 *						 2) GL_UNSIGNED_BYTE for destination pixels.
 *  @param red			 Value for red channel.
 *  @param green		   Value for green channel.
 *  @param blue			Value for blue channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getRGB565PixelData(int is_source_pixel, GLenum type, int red, int green, int blue)
{
	PixelData result;

	if (is_source_pixel)
		DE_ASSERT(type == GL_UNSIGNED_BYTE || type == GL_UNSIGNED_SHORT_5_6_5);
	else
		DE_ASSERT(type == GL_UNSIGNED_BYTE);

	deMemset(&result, 0, sizeof(result));

	switch (type)
	{
	case GL_UNSIGNED_BYTE:
	{
		// Fill the channel data structures
		result.blue.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
		result.blue.unsigned_byte_data  = blue;
		result.green.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
		result.green.unsigned_byte_data = green;
		result.red.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
		result.red.unsigned_byte_data   = red;

		break;
	}

	case GL_UNSIGNED_SHORT_5_6_5:
	{
		DE_ASSERT(red >= 0 && red <= 31);
		DE_ASSERT(green >= 0 && green <= 63);
		DE_ASSERT(blue >= 0 && blue <= 31);

		// Fill the channel data structures
		result.blue.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_5BITS;
		result.blue.unsigned_byte_data  = blue;
		result.green.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_6BITS;
		result.green.unsigned_byte_data = green;
		result.red.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_5BITS;
		result.red.unsigned_byte_data   = red;

		break;
	}
	}

	if (is_source_pixel)
	{
		result.alpha.data_type = CHANNEL_DATA_TYPE_NONE;
	}
	else
	{
		result.alpha.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
		result.alpha.unsigned_byte_data = 255;
	}

	result.data_internalformat = GL_RGB565;
	result.data_type		   = type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_RGB8 internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be GL_UNSIGNED_BYTE.
 *  @param red			 Value for red channel.
 *  @param green		   Value for green channel.
 *  @param blue			Value for blue channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getRGB8PixelData(int is_source_pixel, GLenum type, unsigned char red, unsigned char green,
											   unsigned char blue)
{
	PixelData result;

	DE_ASSERT(type == GL_UNSIGNED_BYTE);

	deMemset(&result, 0, sizeof(result));

	if (is_source_pixel)
	{
		result.alpha.data_type = CHANNEL_DATA_TYPE_NONE;
	}
	else
	{
		result.alpha.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
		result.alpha.unsigned_byte_data = 255;
	}

	result.blue.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
	result.blue.unsigned_byte_data  = blue;
	result.green.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
	result.green.unsigned_byte_data = green;
	result.red.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
	result.red.unsigned_byte_data   = red;
	result.data_internalformat		= GL_RGB8;
	result.data_type				= type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_RGB8I internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be:
 *						 1) GL_BYTE for source pixels.
 *						 2) GL_INT for destination pixels.
 *  @param red			 Value for red channel.
 *  @param green		   Value for green channel.
 *  @param blue			Value for blue channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getRGB8IPixelData(int is_source_pixel, GLenum type, int red, int green, int blue)
{
	PixelData result;

	if (is_source_pixel)
		DE_ASSERT(type == GL_BYTE);
	else
		DE_ASSERT(type == GL_INT);

	deMemset(&result, 0, sizeof(result));

	if (is_source_pixel)
	{
		result.alpha.data_type		  = CHANNEL_DATA_TYPE_NONE;
		result.blue.data_type		  = CHANNEL_DATA_TYPE_SIGNED_BYTE_8BITS;
		result.blue.signed_byte_data  = blue;
		result.green.data_type		  = CHANNEL_DATA_TYPE_SIGNED_BYTE_8BITS;
		result.green.signed_byte_data = green;
		result.red.data_type		  = CHANNEL_DATA_TYPE_SIGNED_BYTE_8BITS;
		result.red.signed_byte_data   = red;
	}
	else
	{
		result.alpha.data_type			 = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;
		result.alpha.signed_integer_data = 1;
		result.blue.data_type			 = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;
		result.blue.signed_integer_data  = blue;
		result.green.data_type			 = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;
		result.green.signed_integer_data = green;
		result.red.data_type			 = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;
		result.red.signed_integer_data   = red;
	}

	result.data_internalformat = GL_RGB8I;
	result.data_type		   = type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_RGB8UI internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be:
 *						 1) GL_UNSIGNED_BYTE for source pixels.
 *						 2) GL_UNSIGNED_INT for destination pixels.
 *  @param red			 Value for red channel.
 *  @param green		   Value for green channel.
 *  @param blue			Value for blue channel.
 *  @param alpha		   Value for alpha channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getRGB8UIPixelData(int is_source_pixel, GLenum type, unsigned int red, unsigned int green,
												 unsigned int blue)
{
	PixelData result;

	if (is_source_pixel)
		DE_ASSERT(type == GL_UNSIGNED_BYTE);
	else
		DE_ASSERT(type == GL_UNSIGNED_INT);

	deMemset(&result, 0, sizeof(result));

	if (is_source_pixel)
	{
		result.alpha.data_type			= CHANNEL_DATA_TYPE_NONE;
		result.blue.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
		result.blue.unsigned_byte_data  = blue;
		result.green.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
		result.green.unsigned_byte_data = green;
		result.red.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
		result.red.unsigned_byte_data   = red;
	}
	else
	{
		result.alpha.data_type			   = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
		result.alpha.unsigned_integer_data = 1;
		result.blue.data_type			   = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
		result.blue.unsigned_integer_data  = blue;
		result.green.data_type			   = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
		result.green.unsigned_integer_data = green;
		result.red.data_type			   = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
		result.red.unsigned_integer_data   = red;
	}

	result.data_internalformat = GL_RGB8UI;
	result.data_type		   = type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_RGBA16I internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be:
 *						 1) GL_SHORT for source pixels.
 *						 2) GL_INT for destination pixels.
 *  @param red			 Value for red channel.
 *  @param green		   Value for green channel.
 *  @param blue			Value for blue channel.
 *  @param alpha		   Value for alpha channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getRGBA16IPixelData(int is_source_pixel, GLenum type, int red, int green, int blue,
												  int alpha)
{
	PixelData result;

	if (is_source_pixel)
		DE_ASSERT(type == GL_SHORT);
	else
		DE_ASSERT(type == GL_INT);

	deMemset(&result, 0, sizeof(result));

	if (is_source_pixel)
	{
		result.alpha.data_type		   = CHANNEL_DATA_TYPE_SIGNED_SHORT_16BITS;
		result.alpha.signed_short_data = alpha;
		result.blue.data_type		   = CHANNEL_DATA_TYPE_SIGNED_SHORT_16BITS;
		result.blue.signed_short_data  = blue;
		result.green.data_type		   = CHANNEL_DATA_TYPE_SIGNED_SHORT_16BITS;
		result.green.signed_short_data = green;
		result.red.data_type		   = CHANNEL_DATA_TYPE_SIGNED_SHORT_16BITS;
		result.red.signed_short_data   = red;
	}
	else
	{
		result.alpha.data_type			 = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;
		result.alpha.signed_integer_data = alpha;
		result.blue.data_type			 = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;
		result.blue.signed_integer_data  = blue;
		result.green.data_type			 = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;
		result.green.signed_integer_data = green;
		result.red.data_type			 = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;
		result.red.signed_integer_data   = red;
	}

	result.data_internalformat = GL_RGBA16I;
	result.data_type		   = type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_RGBA16UI internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be:
 *						 1) GL_UNSIGNED_SHORT for source pixels.
 *						 2) GL_UNSIGNED_INT for destination pixels.
 *  @param red			 Value for red channel.
 *  @param green		   Value for green channel.
 *  @param blue			Value for blue channel.
 *  @param alpha		   Value for alpha channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getRGBA16UIPixelData(int is_source_pixel, GLenum type, unsigned int red,
												   unsigned int green, unsigned int blue, unsigned int alpha)
{
	PixelData result;

	if (is_source_pixel)
		DE_ASSERT(type == GL_UNSIGNED_SHORT);
	else
		DE_ASSERT(type == GL_UNSIGNED_INT);

	deMemset(&result, 0, sizeof(result));

	if (is_source_pixel)
	{
		result.alpha.data_type			 = CHANNEL_DATA_TYPE_UNSIGNED_SHORT_16BITS;
		result.alpha.unsigned_short_data = alpha;
		result.blue.data_type			 = CHANNEL_DATA_TYPE_UNSIGNED_SHORT_16BITS;
		result.blue.unsigned_short_data  = blue;
		result.green.data_type			 = CHANNEL_DATA_TYPE_UNSIGNED_SHORT_16BITS;
		result.green.unsigned_short_data = green;
		result.red.data_type			 = CHANNEL_DATA_TYPE_UNSIGNED_SHORT_16BITS;
		result.red.unsigned_short_data   = red;
	}
	else
	{
		result.alpha.data_type			   = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
		result.alpha.unsigned_integer_data = alpha;
		result.blue.data_type			   = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
		result.blue.unsigned_integer_data  = blue;
		result.green.data_type			   = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
		result.green.unsigned_integer_data = green;
		result.red.data_type			   = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
		result.red.unsigned_integer_data   = red;
	}

	result.data_internalformat = GL_RGBA16UI;
	result.data_type		   = type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_RGBA32I internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be GL_INT.
 *  @param red			 Value for red channel.
 *  @param green		   Value for green channel.
 *  @param blue			Value for blue channel.
 *  @param alpha		   Value for alpha channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getRGBA32IPixelData(GLenum type, int red, int green, int blue, int alpha)
{
	PixelData result;

	DE_ASSERT(type == GL_INT);

	deMemset(&result, 0, sizeof(result));

	result.alpha.data_type			 = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;
	result.alpha.signed_integer_data = alpha;
	result.blue.data_type			 = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;
	result.blue.signed_integer_data  = blue;
	result.green.data_type			 = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;
	result.green.signed_integer_data = green;
	result.red.data_type			 = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;
	result.red.signed_integer_data   = red;
	result.data_internalformat		 = GL_RGBA32I;
	result.data_type				 = type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_RGBA32UI internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be GL_UNSIGNED_INT.
 *  @param red			 Value for red channel.
 *  @param green		   Value for green channel.
 *  @param blue			Value for blue channel.
 *  @param alpha		   Value for alpha channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getRGBA32UIPixelData(GLenum type, unsigned int red, unsigned int green, unsigned int blue,
												   unsigned int alpha)
{
	PixelData result;

	DE_ASSERT(type == GL_UNSIGNED_INT);

	deMemset(&result, 0, sizeof(result));

	result.alpha.data_type			   = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
	result.alpha.unsigned_integer_data = alpha;
	result.blue.data_type			   = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
	result.blue.unsigned_integer_data  = blue;
	result.green.data_type			   = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
	result.green.unsigned_integer_data = green;
	result.red.data_type			   = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
	result.red.unsigned_integer_data   = red;
	result.data_internalformat		   = GL_RGBA32UI;
	result.data_type				   = type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_RGBA8I internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be:
 *						 1) GL_BYTE for source pixels.
 *						 2) GL_INT for destination pixels.
 *  @param red			 Value for red channel.
 *  @param green		   Value for green channel.
 *  @param blue			Value for blue channel.
 *  @param alpha		   Value for alpha channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getRGBA8IPixelData(int is_source_pixel, GLenum type, int red, int green, int blue,
												 int alpha)
{
	PixelData result;

	if (is_source_pixel)
		DE_ASSERT(type == GL_BYTE);
	else
		DE_ASSERT(type == GL_INT);

	deMemset(&result, 0, sizeof(result));

	if (is_source_pixel)
	{
		result.alpha.data_type		  = CHANNEL_DATA_TYPE_SIGNED_BYTE_8BITS;
		result.alpha.signed_byte_data = alpha;
		result.blue.data_type		  = CHANNEL_DATA_TYPE_SIGNED_BYTE_8BITS;
		result.blue.signed_byte_data  = blue;
		result.green.data_type		  = CHANNEL_DATA_TYPE_SIGNED_BYTE_8BITS;
		result.green.signed_byte_data = green;
		result.red.data_type		  = CHANNEL_DATA_TYPE_SIGNED_BYTE_8BITS;
		result.red.signed_byte_data   = red;
	}
	else
	{
		result.alpha.data_type			 = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;
		result.alpha.signed_integer_data = alpha;
		result.blue.data_type			 = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;
		result.blue.signed_integer_data  = blue;
		result.green.data_type			 = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;
		result.green.signed_integer_data = green;
		result.red.data_type			 = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;
		result.red.signed_integer_data   = red;
	}

	result.data_internalformat = GL_RGBA8I;
	result.data_type		   = type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_RGBA8UI internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be:
 *						 1) GL_UNSIGNED_BYTE for source pixels.
 *						 2) GL_UNSIGNED_INT for destination pixels.
 *  @param red			 Value for red channel.
 *  @param green		   Value for green channel.
 *  @param blue			Value for blue channel.
 *  @param alpha		   Value for alpha channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getRGBA8UIPixelData(int is_source_pixel, GLenum type, unsigned int red,
												  unsigned int green, unsigned int blue, unsigned int alpha)
{
	PixelData result;

	if (is_source_pixel)
		DE_ASSERT(type == GL_UNSIGNED_BYTE);
	else
		DE_ASSERT(type == GL_UNSIGNED_INT);

	deMemset(&result, 0, sizeof(result));

	if (is_source_pixel)
	{
		result.alpha.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
		result.alpha.unsigned_byte_data = alpha;
		result.blue.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
		result.blue.unsigned_byte_data  = blue;
		result.green.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
		result.green.unsigned_byte_data = green;
		result.red.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
		result.red.unsigned_byte_data   = red;
	}
	else
	{
		result.alpha.data_type			   = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
		result.alpha.unsigned_integer_data = alpha;
		result.blue.data_type			   = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
		result.blue.unsigned_integer_data  = blue;
		result.green.data_type			   = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
		result.green.unsigned_integer_data = green;
		result.red.data_type			   = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
		result.red.unsigned_integer_data   = red;
	}

	result.data_internalformat = GL_RGBA8UI;
	result.data_type		   = type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_RGBA4 internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be:
 *						 1) GL_UNSIGNED_BYTE or GL_UNSIGNED_SHORT_4_4_4_4 for source pixels.
 *						 2) GL_UNSIGNED_BYTE for destination pixels.
 *  @param red			 Value for red channel.
 *  @param green		   Value for green channel.
 *  @param blue			Value for blue channel.
 *  @param alpha		   Value for alpha channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getRGBA4PixelData(int is_source_pixel, GLenum type, unsigned char red,
												unsigned char green, unsigned char blue, unsigned char alpha)
{
	PixelData result;

	if (is_source_pixel)
		DE_ASSERT(type == GL_UNSIGNED_BYTE || type == GL_UNSIGNED_SHORT_4_4_4_4);
	else
		DE_ASSERT(type == GL_UNSIGNED_BYTE);

	deMemset(&result, 0, sizeof(result));

	switch (type)
	{
	case GL_UNSIGNED_BYTE:
	{
		// Fill the channel data structures
		result.alpha.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
		result.alpha.unsigned_byte_data = alpha;
		result.blue.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
		result.blue.unsigned_byte_data  = blue;
		result.green.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
		result.green.unsigned_byte_data = green;
		result.red.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
		result.red.unsigned_byte_data   = red;

		break;
	}

	case GL_UNSIGNED_SHORT_4_4_4_4:
	{
		DE_ASSERT(red <= 15);
		DE_ASSERT(green <= 15);
		DE_ASSERT(blue <= 15);
		DE_ASSERT(alpha <= 15);

		// Fill the channel data structures
		result.alpha.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_4BITS;
		result.alpha.unsigned_byte_data = alpha;
		result.blue.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_4BITS;
		result.blue.unsigned_byte_data  = blue;
		result.green.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_4BITS;
		result.green.unsigned_byte_data = green;
		result.red.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_4BITS;
		result.red.unsigned_byte_data   = red;

		break;
	}
	}

	result.data_internalformat = GL_RGBA4;
	result.data_type		   = type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_RGBA8 internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be GL_UNSIGNED_BYTE.
 *  @param red			 Value for red channel.
 *  @param green		   Value for green channel.
 *  @param blue			Value for blue channel.
 *  @param alpha		   Value for alpha channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getRGBA8PixelData(GLenum type, unsigned char red, unsigned char green, unsigned char blue,
												unsigned char alpha)
{
	PixelData result;

	DE_ASSERT(type == GL_UNSIGNED_BYTE);

	deMemset(&result, 0, sizeof(result));

	result.alpha.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
	result.alpha.unsigned_byte_data = alpha;
	result.blue.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
	result.blue.unsigned_byte_data  = blue;
	result.green.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
	result.green.unsigned_byte_data = green;
	result.red.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
	result.red.unsigned_byte_data   = red;
	result.data_internalformat		= GL_RGBA8;
	result.data_type				= type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_SRGB8_ALPHA8 internal format.
 *
 *  @param type			GLES type the pixel uses. Must be GL_UNSIGNED_BYTE.
 *  @param red			 Value for red channel.
 *  @param green		   Value for green channel.
 *  @param blue			Value for blue channel.
 *  @param alpha		   Value for alpha channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getSRGB8Alpha8PixelData(GLenum type, unsigned char red, unsigned char green,
													  unsigned char blue, unsigned char alpha)
{
	PixelData result = getRGBA8PixelData(type, red, green, blue, alpha);

	result.data_internalformat = GL_SRGB8_ALPHA8;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_SRGB8 internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be GL_UNSIGNED_BYTE.
 *  @param red			 Value for red channel.
 *  @param green		   Value for green channel.
 *  @param blue			Value for blue channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getSRGB8PixelData(int is_source_pixel, GLenum type, unsigned char red,
												unsigned char green, unsigned char blue)
{
	PixelData result = getSRGB8Alpha8PixelData(type, red, green, blue, 0);

	if (is_source_pixel)
	{
		result.alpha.data_type			= CHANNEL_DATA_TYPE_NONE;
		result.alpha.unsigned_byte_data = 0;
	}
	else
	{
		result.alpha.data_type			= CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
		result.alpha.unsigned_byte_data = 255;
	}

	result.data_internalformat = GL_SRGB8;
	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_R16F internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be GL_HALF_FLOAT
 *  @param red			 Value for red channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getR16FPixelData(int is_source_pixel, GLenum type, float red)
{
	PixelData result;

	DE_ASSERT(type == GL_HALF_FLOAT);

	deMemset(&result, 0, sizeof(result));

	if (is_source_pixel)
	{
		result.red.float_data = red;
		result.red.data_type  = CHANNEL_DATA_TYPE_FLOAT;
	}
	else
	{
		result.alpha.float_data = 1;
		result.alpha.data_type  = CHANNEL_DATA_TYPE_FLOAT;
		result.red.float_data   = red;
		result.red.data_type	= CHANNEL_DATA_TYPE_FLOAT;
	}

	result.data_internalformat = GL_R16F;
	result.data_type		   = type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_R32F internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be GL_FLOAT
 *  @param red			 Value for red channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getR32FPixelData(int is_source_pixel, GLenum type, float red)
{
	PixelData result;

	DE_ASSERT(type == GL_FLOAT);

	deMemset(&result, 0, sizeof(result));

	if (is_source_pixel)
	{
		result.red.float_data = red;
		result.red.data_type  = CHANNEL_DATA_TYPE_FLOAT;
	}
	else
	{
		result.alpha.float_data = 1;
		result.alpha.data_type  = CHANNEL_DATA_TYPE_FLOAT;
		result.red.float_data   = red;
		result.red.data_type	= CHANNEL_DATA_TYPE_FLOAT;
	}

	result.data_internalformat = GL_R32F;
	result.data_type		   = type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_RG16F internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be GL_HALF_FLOAT
 *  @param red			 Value for red channel.
 *  @param green		   Value for green channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getRG16FPixelData(int is_source_pixel, GLenum type, float red, float green)
{
	PixelData result;

	DE_ASSERT(type == GL_HALF_FLOAT);

	deMemset(&result, 0, sizeof(result));

	if (is_source_pixel)
	{
		result.red.float_data   = red;
		result.red.data_type	= CHANNEL_DATA_TYPE_FLOAT;
		result.green.float_data = green;
		result.green.data_type  = CHANNEL_DATA_TYPE_FLOAT;
	}
	else
	{
		result.alpha.float_data = 1;
		result.alpha.data_type  = CHANNEL_DATA_TYPE_FLOAT;
		result.red.float_data   = red;
		result.red.data_type	= CHANNEL_DATA_TYPE_FLOAT;
		result.green.float_data = green;
		result.green.data_type  = CHANNEL_DATA_TYPE_FLOAT;
	}

	result.data_internalformat = GL_RG16F;
	result.data_type		   = type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_RG32F internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be GL_FLOAT
 *  @param red			 Value for red channel.
 *  @param green		   Value for green channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getRG32FPixelData(int is_source_pixel, GLenum type, float red, float green)
{
	PixelData result;

	DE_ASSERT(type == GL_FLOAT);

	deMemset(&result, 0, sizeof(result));

	if (is_source_pixel)
	{
		result.red.float_data   = red;
		result.red.data_type	= CHANNEL_DATA_TYPE_FLOAT;
		result.green.float_data = green;
		result.green.data_type  = CHANNEL_DATA_TYPE_FLOAT;
	}
	else
	{
		result.alpha.float_data = 1;
		result.alpha.data_type  = CHANNEL_DATA_TYPE_FLOAT;
		result.red.float_data   = red;
		result.red.data_type	= CHANNEL_DATA_TYPE_FLOAT;
		result.green.float_data = green;
		result.green.data_type  = CHANNEL_DATA_TYPE_FLOAT;
	}

	result.data_internalformat = GL_RG32F;
	result.data_type		   = type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_RGB16F internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be GL_HALF_FLOAT
 *  @param red			 Value for red channel.
 *  @param green		   Value for green channel.
 *  @param blue			Value for green channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getRGB16FPixelData(int is_source_pixel, GLenum type, float red, float green, float blue)
{
	PixelData result;

	DE_ASSERT(type == GL_HALF_FLOAT);

	deMemset(&result, 0, sizeof(result));

	if (is_source_pixel)
	{
		result.red.float_data   = red;
		result.red.data_type	= CHANNEL_DATA_TYPE_FLOAT;
		result.green.float_data = green;
		result.green.data_type  = CHANNEL_DATA_TYPE_FLOAT;
		result.blue.float_data  = blue;
		result.blue.data_type   = CHANNEL_DATA_TYPE_FLOAT;
	}
	else
	{
		result.alpha.float_data = 1;
		result.alpha.data_type  = CHANNEL_DATA_TYPE_FLOAT;
		result.red.float_data   = red;
		result.red.data_type	= CHANNEL_DATA_TYPE_FLOAT;
		result.green.float_data = green;
		result.green.data_type  = CHANNEL_DATA_TYPE_FLOAT;
		result.blue.float_data  = blue;
		result.blue.data_type   = CHANNEL_DATA_TYPE_FLOAT;
	}

	result.data_internalformat = GL_RGB16F;
	result.data_type		   = type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_RGB32F internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be GL_FLOAT
 *  @param red			 Value for red channel.
 *  @param green		   Value for green channel.
 *  @param blue			Value for blue channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getRGB32FPixelData(int is_source_pixel, GLenum type, float red, float green, float blue)
{
	PixelData result;

	DE_ASSERT(type == GL_FLOAT);

	deMemset(&result, 0, sizeof(result));

	if (is_source_pixel)
	{
		result.red.float_data   = red;
		result.red.data_type	= CHANNEL_DATA_TYPE_FLOAT;
		result.green.float_data = green;
		result.green.data_type  = CHANNEL_DATA_TYPE_FLOAT;
		result.blue.float_data  = blue;
		result.blue.data_type   = CHANNEL_DATA_TYPE_FLOAT;
	}
	else
	{
		result.alpha.float_data = 1;
		result.alpha.data_type  = CHANNEL_DATA_TYPE_FLOAT;
		result.red.float_data   = red;
		result.red.data_type	= CHANNEL_DATA_TYPE_FLOAT;
		result.green.float_data = green;
		result.green.data_type  = CHANNEL_DATA_TYPE_FLOAT;
		result.blue.float_data  = blue;
		result.blue.data_type   = CHANNEL_DATA_TYPE_FLOAT;
	}

	result.data_internalformat = GL_RGB32F;
	result.data_type		   = type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_RGBA16F internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be GL_HALF_FLOAT
 *  @param red			 Value for red channel.
 *  @param green		   Value for green channel.
 *  @param blue			Value for blue channel.
 *  @param alpha		   Value for alpha channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getRGBA16FPixelData(GLenum type, float red, float green, float blue, float alpha)
{
	PixelData result;

	DE_ASSERT(type == GL_HALF_FLOAT);

	deMemset(&result, 0, sizeof(result));

	result.alpha.float_data = alpha;
	result.alpha.data_type  = CHANNEL_DATA_TYPE_FLOAT;
	result.red.float_data   = red;
	result.red.data_type	= CHANNEL_DATA_TYPE_FLOAT;
	result.green.float_data = green;
	result.green.data_type  = CHANNEL_DATA_TYPE_FLOAT;
	result.blue.float_data  = blue;
	result.blue.data_type   = CHANNEL_DATA_TYPE_FLOAT;

	result.data_internalformat = GL_RGBA16F;
	result.data_type		   = type;

	return result;
}

/** Retrieves a PixelData instance describing a single pixel stored in
 *  GL_RGBA32F internal format.
 *
 *  @param is_source_pixel 1 if the pixel is going to be used as conversion source,
 *						 0 otherwise.
 *  @param type			GLES type the pixel uses. Must be GL_FLOAT
 *  @param red			 Value for red channel.
 *  @param green		   Value for green channel.
 *  @param blue			Value for blue channel.
 *  @param alpha		   Value for alpha channel.
 *
 *  @return Filled PixelData instance.
 **/
PixelData ConversionDatabase::getRGBA32FPixelData(GLenum type, float red, float green, float blue, float alpha)
{
	PixelData result;

	DE_ASSERT(type == GL_FLOAT);

	deMemset(&result, 0, sizeof(result));

	result.alpha.float_data = alpha;
	result.alpha.data_type  = CHANNEL_DATA_TYPE_FLOAT;
	result.red.float_data   = red;
	result.red.data_type	= CHANNEL_DATA_TYPE_FLOAT;
	result.green.float_data = green;
	result.green.data_type  = CHANNEL_DATA_TYPE_FLOAT;
	result.blue.float_data  = blue;
	result.blue.data_type   = CHANNEL_DATA_TYPE_FLOAT;

	result.data_internalformat = GL_RGBA32F;
	result.data_type		   = type;

	return result;
}

/** Adds a new conversion rule to a conversion database.
 *
 *  Destination pixel datasets can only use one of the following types:
 *
 *  GL_UNSIGNED_BYTE			   (for fixed-point source types);
 *  GL_INT						 (for signed integer types);
 *  GL_UNSIGNED_INT				(for unsigned integer types);
 *  GL_UNSIGNED_INT_2_10_10_10_REV (for GL_RGB10_A2 type ONLY)
 *
 *  The type used for destination storage configures arguments that
 *  will be passed to a gl.readPixels() call in verification stage IF
 *  source internalformat is color-renderable. If not, destination type
 *  determines how result & reference data should be compared using
 *  a special program object.
 *
 *  It is illegal to:
 *
 *  1) add more than one conversion rule that uses the same source+destination
 *	 internalformat+type combination.
 *  2) use source pixels of different internalformats or types;
 *  3) use destination pixels of different internalformats or types;
 *  4) use pixel data instances using invalid internalformat or types.
 *
 *  @param src_topleft	 Pixel-data instance describing source top-left corner.
 *  @param dst_topleft	 Pixel-data instance describing destination top-left corner.
 *  @param src_topright	Pixel-data instance describing source top-right corner.
 *  @param dst_topright	Pixel-data instance describing destination top-right corner.
 *  @param src_bottomleft  Pixel-data instance describing source bottom-left corner.
 *  @param dst_bottomleft  Pixel-data instance describing destination bottom-left corner.
 *  @param src_bottomright Pixel-data instance describing source bottom-right corner.
 *  @param dst_bottomright Pixel-data instance describing destination bottom-right corner.
 **/
void ConversionDatabase::addEntryToConversionDatabase(PixelData src_topleft, PixelData dst_topleft,
													  PixelData src_topright, PixelData dst_topright,
													  PixelData src_bottomleft, PixelData dst_bottomleft,
													  PixelData src_bottomright, PixelData dst_bottomright,
													  PixelCompareChannel channels_to_compare)
{
	GLenum dst_internalformat		   = GL_NONE;
	GLenum dst_type					   = GL_NONE;
	int	is_dst_internalformat_valid = 0;
	int	is_dst_type_valid		   = 0;
	GLenum src_internalformat		   = GL_NONE;
	GLenum src_type					   = GL_NONE;

	// Sanity checks: general
	DE_ASSERT(src_topleft.data_internalformat != GL_NONE);
	DE_ASSERT(dst_topleft.data_internalformat != GL_NONE);

	if (src_topleft.data_internalformat == GL_NONE || dst_topleft.data_internalformat == GL_NONE)
		return; // if (source / destination internalformats are GL_NONE)

	DE_ASSERT(src_topleft.data_internalformat == src_topright.data_internalformat);
	DE_ASSERT(src_topleft.data_internalformat == src_bottomleft.data_internalformat);
	DE_ASSERT(src_topleft.data_internalformat == src_bottomright.data_internalformat);
	DE_ASSERT(src_topleft.data_type == src_topright.data_type);
	DE_ASSERT(src_topleft.data_type == src_bottomleft.data_type);
	DE_ASSERT(src_topleft.data_type == src_bottomright.data_type);

	if (src_topleft.data_internalformat != src_topright.data_internalformat ||
		src_topleft.data_internalformat != src_bottomleft.data_internalformat ||
		src_topleft.data_internalformat != src_bottomright.data_internalformat ||
			src_topleft.data_type != src_topright.data_type || src_topleft.data_type != src_bottomleft.data_type ||
		src_topleft.data_type != src_bottomright.data_type)
	{
		return;
	} // if (source pixels' internalformats and/or types are not the same values)

	DE_ASSERT(dst_topleft.data_internalformat == dst_topright.data_internalformat);
	DE_ASSERT(dst_topleft.data_internalformat == dst_bottomleft.data_internalformat);
	DE_ASSERT(dst_topleft.data_internalformat == dst_bottomright.data_internalformat);
	DE_ASSERT(dst_topleft.data_type == dst_topright.data_type);
	DE_ASSERT(dst_topleft.data_type == dst_bottomleft.data_type);
	DE_ASSERT(dst_topleft.data_type == dst_bottomright.data_type);

	if (dst_topleft.data_internalformat != dst_topright.data_internalformat ||
		dst_topleft.data_internalformat != dst_bottomleft.data_internalformat ||
		dst_topleft.data_internalformat != dst_bottomright.data_internalformat ||
		dst_topleft.data_type != dst_topright.data_type || dst_topleft.data_type != dst_bottomleft.data_type ||
		dst_topleft.data_type != dst_bottomright.data_type)
	{
		return;
	} // if (destination pixels' internalformats and/or types are not the same values)

	src_internalformat = src_topleft.data_internalformat;
	src_type		   = src_topleft.data_type;
	dst_internalformat = dst_topleft.data_internalformat;
	dst_type		   = dst_topleft.data_type;

	// Sanity checks: format used for destination storage
	is_dst_type_valid			= isTypeSupportedByGLReadPixels(dst_type);
	is_dst_internalformat_valid = isInternalFormatCompatibleWithType(dst_type, dst_internalformat);

	DE_ASSERT(is_dst_type_valid && is_dst_internalformat_valid);
	if (!is_dst_type_valid || !is_dst_internalformat_valid)
		TCU_FAIL("Requested destination type or internalformat is not compatible with validation requirements.");

	// Sanity checks: make sure the conversion has not been already added
	for (unsigned int n = 0; n < n_entries_added; ++n)
	{
		ConversionDatabaseEntry& entry_ptr = entries[n];

		GLenum iterated_dst_internalformat = entry_ptr.dst_topleft_corner.data_internalformat;
		GLenum iterated_dst_type		   = entry_ptr.dst_topleft_corner.data_type;
		GLenum iterated_src_internalformat = entry_ptr.src_topleft_corner.data_internalformat;
		GLenum iterated_src_type		   = entry_ptr.src_topleft_corner.data_type;
		int	is_new_rule				   = src_internalformat != iterated_src_internalformat ||
						  ((src_internalformat == iterated_src_internalformat) && (src_type != iterated_src_type)) ||
						  ((src_internalformat == iterated_src_internalformat) && (src_type == iterated_src_type) &&
						   (dst_internalformat != iterated_dst_internalformat)) ||
						  ((src_internalformat == iterated_src_internalformat) && (src_type == iterated_src_type) &&
						   (dst_internalformat == iterated_dst_internalformat) && (dst_type != iterated_dst_type));

		DE_ASSERT(is_new_rule);
		if (!is_new_rule)
			TCU_FAIL("This conversion rule already exists!");
	}

	// Make sure there's enough space to hold a new entry
	if ((n_entries_added + 1) >= n_entries_allocated)
	{
		// Realloc is needed
		n_entries_allocated <<= 1;
		entries.resize(n_entries_allocated);
		if (entries.empty())
			TCU_FAIL("Out of memory while reallocating conversion database");
	}

	// Add the new entry
	ConversionDatabaseEntry& entry_ptr = entries[n_entries_added];
	entry_ptr.dst_bottomleft_corner	= dst_bottomleft;
	entry_ptr.dst_bottomright_corner   = dst_bottomright;
	entry_ptr.dst_topleft_corner	   = dst_topleft;
	entry_ptr.dst_topright_corner	  = dst_topright;
	entry_ptr.src_bottomleft_corner	= src_bottomleft;
	entry_ptr.src_bottomright_corner   = src_bottomright;
	entry_ptr.src_topleft_corner	   = src_topleft;
	entry_ptr.src_topright_corner	  = src_topright;
	entry_ptr.channels_to_compare	  = channels_to_compare;

	++n_entries_added;
}

/** Adds all known conversion rules to a conversion database passed by argument.
 *
 *  A conversion database stores exactly one conversion rule for each valid combination
 *  of source+destination internal-formats (with an exception that for each internalformat
 *  data may be represented with many different types!).
 *  These rules are then used by copy_tex_image_conversions_required conformance test to
 *  validate successfully executed conversions.
 *
 *  A quick reminder:
 *
 *	  Source dataset corresponds to 2x2 image (using up to 4 channels) that the attachment bound to
 *  read buffer will use prior to glCopyTexImage2D() call. This image is defined by 4 Get*PixelData()
 *  calls with the first argument set to 1.
 *	  Destination dataset corresponds to 2x2 image (using up to 4 channels) that the result texture
 *  object should match (within acceptable epsilon). This image is defined by 4 Get*PixelData() calls
 *  with the first argument set to 0.
 *
 *  Source datasets are allowed to use any internalformat+type combination that is considered supported
 *  by GLES implementation.
 *  Destination datasets are only allowed to use specific types - please see AddEntryToConversionDatabase()
 *  doxygen for more details.
 *
 *  @param database Conversion database handle.
 **/
void ConversionDatabase::configureConversionDatabase()
{
	int bits_1010102[4] = { 10, 10, 10, 2 };
	int bits_4444[4]	= { 4, 4, 4, 4 };
	int bits_5551[4]	= { 5, 5, 5, 1 };
	int bits_565[4]		= { 5, 6, 5, 0 };
	int bits_888[4]		= { 8, 8, 8, 0 };
	int bits_8888[4]	= { 8, 8, 8, 8 };

	/* GL_R8 */
	{
		const unsigned char texel1[1] = { 255 };
		const unsigned char texel2[1] = { 127 };
		const unsigned char texel3[1] = { 63 };
		const unsigned char texel4[1] = { 0 };

		/* GL_R8 => GL_LUMINANCE8_OES */
		addEntryToConversionDatabase(
			getR8PixelData(1, GL_UNSIGNED_BYTE, texel1[0]), getLuminance8OESPixelData(GL_UNSIGNED_BYTE, texel1[0]),
			getR8PixelData(1, GL_UNSIGNED_BYTE, texel2[0]), getLuminance8OESPixelData(GL_UNSIGNED_BYTE, texel2[0]),
			getR8PixelData(1, GL_UNSIGNED_BYTE, texel3[0]), getLuminance8OESPixelData(GL_UNSIGNED_BYTE, texel3[0]),
			getR8PixelData(1, GL_UNSIGNED_BYTE, texel4[0]), getLuminance8OESPixelData(GL_UNSIGNED_BYTE, texel4[0]),
			PIXEL_COMPARE_CHANNEL_R);

		/* GL_R8 => GL_R8 */
		addEntryToConversionDatabase(
			getR8PixelData(1, GL_UNSIGNED_BYTE, texel1[0]), getR8PixelData(0, GL_UNSIGNED_BYTE, texel1[0]),
			getR8PixelData(1, GL_UNSIGNED_BYTE, texel2[0]), getR8PixelData(0, GL_UNSIGNED_BYTE, texel2[0]),
			getR8PixelData(1, GL_UNSIGNED_BYTE, texel3[0]), getR8PixelData(0, GL_UNSIGNED_BYTE, texel3[0]),
			getR8PixelData(1, GL_UNSIGNED_BYTE, texel4[0]), getR8PixelData(0, GL_UNSIGNED_BYTE, texel4[0]),
			PIXEL_COMPARE_CHANNEL_R);
	}

	/* GL_RG8 */
	{
		const unsigned char texel1[2] = { 255, 127 };
		const unsigned char texel2[2] = { 127, 63 };
		const unsigned char texel3[2] = { 63, 0 };
		const unsigned char texel4[2] = { 0, 255 };

		/* GL_RG8 => GL_LUMINANCE8_OES */
		addEntryToConversionDatabase(getRG8PixelData(1, GL_UNSIGNED_BYTE, texel1[0], texel1[1]),
									 getLuminance8OESPixelData(GL_UNSIGNED_BYTE, texel1[0]),
									 getRG8PixelData(1, GL_UNSIGNED_BYTE, texel2[0], texel2[1]),
									 getLuminance8OESPixelData(GL_UNSIGNED_BYTE, texel2[0]),
									 getRG8PixelData(1, GL_UNSIGNED_BYTE, texel3[0], texel3[1]),
									 getLuminance8OESPixelData(GL_UNSIGNED_BYTE, texel3[0]),
									 getRG8PixelData(1, GL_UNSIGNED_BYTE, texel4[0], texel4[1]),
									 getLuminance8OESPixelData(GL_UNSIGNED_BYTE, texel4[0]), PIXEL_COMPARE_CHANNEL_R);

		/* GL_RG8 => GL_R8 */
		addEntryToConversionDatabase(
			getRG8PixelData(1, GL_UNSIGNED_BYTE, texel1[0], texel1[1]), getR8PixelData(0, GL_UNSIGNED_BYTE, texel1[0]),
			getRG8PixelData(1, GL_UNSIGNED_BYTE, texel2[0], texel2[1]), getR8PixelData(0, GL_UNSIGNED_BYTE, texel2[0]),
			getRG8PixelData(1, GL_UNSIGNED_BYTE, texel3[0], texel3[1]), getR8PixelData(0, GL_UNSIGNED_BYTE, texel3[0]),
			getRG8PixelData(1, GL_UNSIGNED_BYTE, texel4[0], texel4[1]), getR8PixelData(0, GL_UNSIGNED_BYTE, texel4[0]),
			PIXEL_COMPARE_CHANNEL_R);

		/* GL_RG8 => GL_RG8 */
		addEntryToConversionDatabase(getRG8PixelData(1, GL_UNSIGNED_BYTE, texel1[0], texel1[1]),
									 getRG8PixelData(0, GL_UNSIGNED_BYTE, texel1[0], texel1[1]),
									 getRG8PixelData(1, GL_UNSIGNED_BYTE, texel2[0], texel2[1]),
									 getRG8PixelData(0, GL_UNSIGNED_BYTE, texel2[0], texel2[1]),
									 getRG8PixelData(1, GL_UNSIGNED_BYTE, texel3[0], texel3[1]),
									 getRG8PixelData(0, GL_UNSIGNED_BYTE, texel3[0], texel3[1]),
									 getRG8PixelData(1, GL_UNSIGNED_BYTE, texel4[0], texel4[1]),
									 getRG8PixelData(0, GL_UNSIGNED_BYTE, texel4[0], texel4[1]),
									 PIXEL_COMPARE_CHANNEL_RG);
	}

	/* GL_RGB8 */
	{
		const unsigned char texel1[3] = { 255, 127, 63 };
		const unsigned char texel2[3] = { 127, 63, 0 };
		const unsigned char texel3[3] = { 63, 0, 255 };
		const unsigned char texel4[3] = { 0, 255, 127 };

		/* GL_RGB8 => GL_RGB8 */
		addEntryToConversionDatabase(getRGB8PixelData(1, GL_UNSIGNED_BYTE, texel1[0], texel1[1], texel1[2]),
									 getRGB8PixelData(0, GL_UNSIGNED_BYTE, texel1[0], texel1[1], texel1[2]),
									 getRGB8PixelData(1, GL_UNSIGNED_BYTE, texel2[0], texel2[1], texel2[2]),
									 getRGB8PixelData(0, GL_UNSIGNED_BYTE, texel2[0], texel2[1], texel2[2]),
									 getRGB8PixelData(1, GL_UNSIGNED_BYTE, texel3[0], texel3[1], texel3[2]),
									 getRGB8PixelData(0, GL_UNSIGNED_BYTE, texel3[0], texel3[1], texel3[2]),
									 getRGB8PixelData(1, GL_UNSIGNED_BYTE, texel4[0], texel4[1], texel4[2]),
									 getRGB8PixelData(0, GL_UNSIGNED_BYTE, texel4[0], texel4[1], texel4[2]),
									 PIXEL_COMPARE_CHANNEL_RGB);

		/* GL_RGB8 => GL_LUMINANCE8_OES */
		addEntryToConversionDatabase(getRGB8PixelData(1, GL_UNSIGNED_BYTE, texel1[0], texel1[1], texel1[2]),
									 getLuminance8OESPixelData(GL_UNSIGNED_BYTE, texel1[0]),
									 getRGB8PixelData(1, GL_UNSIGNED_BYTE, texel2[0], texel2[1], texel2[2]),
									 getLuminance8OESPixelData(GL_UNSIGNED_BYTE, texel2[0]),
									 getRGB8PixelData(1, GL_UNSIGNED_BYTE, texel3[0], texel3[1], texel3[2]),
									 getLuminance8OESPixelData(GL_UNSIGNED_BYTE, texel3[0]),
									 getRGB8PixelData(1, GL_UNSIGNED_BYTE, texel4[0], texel4[1], texel4[2]),
									 getLuminance8OESPixelData(GL_UNSIGNED_BYTE, texel4[0]), PIXEL_COMPARE_CHANNEL_R);

		/* GL_RGB8 => GL_R8 */
		addEntryToConversionDatabase(getRGB8PixelData(1, GL_UNSIGNED_BYTE, texel1[0], texel1[1], texel1[2]),
									 getR8PixelData(0, GL_UNSIGNED_BYTE, texel1[0]),
									 getRGB8PixelData(1, GL_UNSIGNED_BYTE, texel2[0], texel2[1], texel2[2]),
									 getR8PixelData(0, GL_UNSIGNED_BYTE, texel2[0]),
									 getRGB8PixelData(1, GL_UNSIGNED_BYTE, texel3[0], texel3[1], texel3[2]),
									 getR8PixelData(0, GL_UNSIGNED_BYTE, texel3[0]),
									 getRGB8PixelData(1, GL_UNSIGNED_BYTE, texel4[0], texel4[1], texel4[2]),
									 getR8PixelData(0, GL_UNSIGNED_BYTE, texel4[0]), PIXEL_COMPARE_CHANNEL_R);

		/* GL_RGB8 => GL_RG8 */
		addEntryToConversionDatabase(getRGB8PixelData(1, GL_UNSIGNED_BYTE, texel1[0], texel1[1], texel1[2]),
									 getRG8PixelData(0, GL_UNSIGNED_BYTE, texel1[0], texel1[1]),
									 getRGB8PixelData(1, GL_UNSIGNED_BYTE, texel2[0], texel2[1], texel2[2]),
									 getRG8PixelData(0, GL_UNSIGNED_BYTE, texel2[0], texel2[1]),
									 getRGB8PixelData(1, GL_UNSIGNED_BYTE, texel3[0], texel3[1], texel3[2]),
									 getRG8PixelData(0, GL_UNSIGNED_BYTE, texel3[0], texel3[1]),
									 getRGB8PixelData(1, GL_UNSIGNED_BYTE, texel4[0], texel4[1], texel4[2]),
									 getRG8PixelData(0, GL_UNSIGNED_BYTE, texel4[0], texel4[1]),
									 PIXEL_COMPARE_CHANNEL_RG);
	}

	{ /* GL_RGB565 */
		int texel565_1[4] = { 31, 63, 21, 0 };
		int texel565_2[4] = { 21, 43, 11, 0 };
		int texel565_3[4] = { 11, 23, 1, 0 };
		int texel888_1[4] = { 255, 155, 55, 0 };
		int texel888_2[4] = { 176, 76, 36, 0 };
		int texel888_3[4] = { 88, 66, 44, 0 };
		int texel888_4[4] = { 20, 10, 0, 0 };

		int temp_565_to_888_bl[4]			  = { 0 };
		int temp_565_to_888_tl[4]			  = { 0 };
		int temp_565_to_888_tr[4]			  = { 0 };
		int temp_888_through_565_to_888_bl[4] = { 0 };
		int temp_888_through_565_to_888_br[4] = { 0 };
		int temp_888_through_565_to_888_tl[4] = { 0 };
		int temp_888_through_565_to_888_tr[4] = { 0 };

		convertNormalizedUnsignedFixedPoint(bits_565, bits_888, bits_888, bits_888, texel565_1, temp_565_to_888_tl);
		convertNormalizedUnsignedFixedPoint(bits_565, bits_888, bits_888, bits_888, texel565_2, temp_565_to_888_tr);
		convertNormalizedUnsignedFixedPoint(bits_565, bits_888, bits_888, bits_888, texel565_3, temp_565_to_888_bl);

		convertNormalizedUnsignedFixedPoint(bits_888, bits_565, bits_888, bits_888, texel888_1,
											temp_888_through_565_to_888_tl);
		convertNormalizedUnsignedFixedPoint(bits_888, bits_565, bits_888, bits_888, texel888_2,
											temp_888_through_565_to_888_tr);
		convertNormalizedUnsignedFixedPoint(bits_888, bits_565, bits_888, bits_888, texel888_3,
											temp_888_through_565_to_888_bl);
		convertNormalizedUnsignedFixedPoint(bits_888, bits_565, bits_888, bits_888, texel888_4,
											temp_888_through_565_to_888_br);

		/* GL_RGB565 => GL_RGB565 */
		addEntryToConversionDatabase(
			getRGB565PixelData(1, GL_UNSIGNED_SHORT_5_6_5, texel565_1[0], texel565_1[1], texel565_1[2]),
			getRGB565PixelData(0, GL_UNSIGNED_BYTE, temp_565_to_888_tl[0], temp_565_to_888_tl[1],
							   temp_565_to_888_tl[2]),
			getRGB565PixelData(1, GL_UNSIGNED_SHORT_5_6_5, texel565_2[0], texel565_2[1], texel565_2[2]),
			getRGB565PixelData(0, GL_UNSIGNED_BYTE, temp_565_to_888_tr[0], temp_565_to_888_tr[1],
							   temp_565_to_888_tr[2]),
			getRGB565PixelData(1, GL_UNSIGNED_SHORT_5_6_5, texel565_3[0], texel565_3[1], texel565_3[2]),
			getRGB565PixelData(0, GL_UNSIGNED_BYTE, temp_565_to_888_bl[0], temp_565_to_888_bl[1],
							   temp_565_to_888_bl[2]),
			getRGB565PixelData(1, GL_UNSIGNED_SHORT_5_6_5, 0, 0, 0), getRGB565PixelData(0, GL_UNSIGNED_BYTE, 0, 0, 0),
			PIXEL_COMPARE_CHANNEL_RGB);

		addEntryToConversionDatabase(
			getRGB565PixelData(1, GL_UNSIGNED_BYTE, texel888_1[0], texel888_1[1], texel888_1[2]),
			getRGB565PixelData(0, GL_UNSIGNED_BYTE, temp_888_through_565_to_888_tl[0],
							   temp_888_through_565_to_888_tl[1], temp_888_through_565_to_888_tl[2]),
			getRGB565PixelData(1, GL_UNSIGNED_BYTE, texel888_2[0], texel888_2[1], texel888_2[2]),
			getRGB565PixelData(0, GL_UNSIGNED_BYTE, temp_888_through_565_to_888_tr[0],
							   temp_888_through_565_to_888_tr[1], temp_888_through_565_to_888_tr[2]),
			getRGB565PixelData(1, GL_UNSIGNED_BYTE, texel888_3[0], texel888_3[1], texel888_3[2]),
			getRGB565PixelData(0, GL_UNSIGNED_BYTE, temp_888_through_565_to_888_bl[0],
							   temp_888_through_565_to_888_bl[1], temp_888_through_565_to_888_bl[2]),
			getRGB565PixelData(1, GL_UNSIGNED_BYTE, texel888_4[0], texel888_4[1], texel888_4[2]),
			getRGB565PixelData(0, GL_UNSIGNED_BYTE, temp_888_through_565_to_888_br[0],
							   temp_888_through_565_to_888_br[1], temp_888_through_565_to_888_br[2]),
			PIXEL_COMPARE_CHANNEL_RGB);

		/* GL_RGB565 => GL_LUMINANCE8_OES */
		addEntryToConversionDatabase(
			getRGB565PixelData(1, GL_UNSIGNED_SHORT_5_6_5, texel565_1[0], texel565_1[1], texel565_1[2]),
			getLuminance8OESPixelData(GL_UNSIGNED_BYTE, temp_565_to_888_tl[0]),
			getRGB565PixelData(1, GL_UNSIGNED_SHORT_5_6_5, texel565_2[0], texel565_2[1], texel565_2[2]),
			getLuminance8OESPixelData(GL_UNSIGNED_BYTE, temp_565_to_888_tr[0]),
			getRGB565PixelData(1, GL_UNSIGNED_SHORT_5_6_5, texel565_3[0], texel565_3[1], texel565_3[2]),
			getLuminance8OESPixelData(GL_UNSIGNED_BYTE, temp_565_to_888_bl[0]),
			getRGB565PixelData(1, GL_UNSIGNED_SHORT_5_6_5, 0, 0, 0), getLuminance8OESPixelData(GL_UNSIGNED_BYTE, 0),
			PIXEL_COMPARE_CHANNEL_R);

		addEntryToConversionDatabase(
			getRGB565PixelData(1, GL_UNSIGNED_BYTE, texel888_1[0], texel888_1[1], texel888_1[2]),
			getLuminance8OESPixelData(GL_UNSIGNED_BYTE, temp_888_through_565_to_888_tl[0]),
			getRGB565PixelData(1, GL_UNSIGNED_BYTE, texel888_2[0], texel888_2[1], texel888_2[2]),
			getLuminance8OESPixelData(GL_UNSIGNED_BYTE, temp_888_through_565_to_888_tr[0]),
			getRGB565PixelData(1, GL_UNSIGNED_BYTE, texel888_3[0], texel888_3[1], texel888_3[2]),
			getLuminance8OESPixelData(GL_UNSIGNED_BYTE, temp_888_through_565_to_888_bl[0]),
			getRGB565PixelData(1, GL_UNSIGNED_BYTE, texel888_4[0], texel888_4[1], texel888_4[2]),
			getLuminance8OESPixelData(GL_UNSIGNED_BYTE, temp_888_through_565_to_888_br[0]), PIXEL_COMPARE_CHANNEL_R);
	}

	/* GL_RGBA4 */
	{
		int texel4444_1[4] = { 15, 9, 4, 0 };
		int texel4444_2[4] = { 9, 4, 0, 15 };
		int texel4444_3[4] = { 4, 0, 15, 9 };
		int texel4444_4[4] = { 0, 15, 9, 4 };
		int texel8888_1[4] = { 255, 159, 79, 0 };
		int texel8888_2[4] = { 159, 79, 0, 255 };
		int texel8888_3[4] = { 79, 0, 255, 159 };
		int texel8888_4[4] = { 0, 255, 159, 79 };

		int temp_4444_to_565_8888_tl[4]				 = { 0 };
		int temp_4444_to_565_8888_tr[4]				 = { 0 };
		int temp_4444_to_565_8888_bl[4]				 = { 0 };
		int temp_4444_to_565_8888_br[4]				 = { 0 };
		int temp_4444_to_8888_tl[4]					 = { 0 };
		int temp_4444_to_8888_tr[4]					 = { 0 };
		int temp_4444_to_8888_bl[4]					 = { 0 };
		int temp_4444_to_8888_br[4]					 = { 0 };
		int temp_8888_through_4444_to_565_tl[4]		 = { 0 };
		int temp_8888_through_4444_to_565_tr[4]		 = { 0 };
		int temp_8888_through_4444_to_565_bl[4]		 = { 0 };
		int temp_8888_through_4444_to_565_br[4]		 = { 0 };
		int temp_8888_through_4444_to_8888_tl[4]	 = { 0 };
		int temp_8888_through_4444_to_8888_tr[4]	 = { 0 };
		int temp_8888_through_4444_to_8888_bl[4]	 = { 0 };
		int temp_8888_through_4444_to_8888_br[4]	 = { 0 };
		int temp_8888_through_4444_565_to_8888_tl[4] = { 0 };
		int temp_8888_through_4444_565_to_8888_tr[4] = { 0 };
		int temp_8888_through_4444_565_to_8888_bl[4] = { 0 };
		int temp_8888_through_4444_565_to_8888_br[4] = { 0 };

		convertNormalizedUnsignedFixedPoint(bits_4444, bits_565, bits_8888, bits_8888, texel4444_1,
											temp_4444_to_565_8888_tl);
		convertNormalizedUnsignedFixedPoint(bits_4444, bits_565, bits_8888, bits_8888, texel4444_2,
											temp_4444_to_565_8888_tr);
		convertNormalizedUnsignedFixedPoint(bits_4444, bits_565, bits_8888, bits_8888, texel4444_3,
											temp_4444_to_565_8888_bl);
		convertNormalizedUnsignedFixedPoint(bits_4444, bits_565, bits_8888, bits_8888, texel4444_4,
											temp_4444_to_565_8888_br);

		convertNormalizedUnsignedFixedPoint(bits_4444, bits_8888, bits_8888, bits_8888, texel4444_1,
											temp_4444_to_8888_tl);
		convertNormalizedUnsignedFixedPoint(bits_4444, bits_8888, bits_8888, bits_8888, texel4444_2,
											temp_4444_to_8888_tr);
		convertNormalizedUnsignedFixedPoint(bits_4444, bits_8888, bits_8888, bits_8888, texel4444_3,
											temp_4444_to_8888_bl);
		convertNormalizedUnsignedFixedPoint(bits_4444, bits_8888, bits_8888, bits_8888, texel4444_4,
											temp_4444_to_8888_br);

		convertNormalizedUnsignedFixedPoint(bits_8888, bits_4444, bits_565, bits_565, texel8888_1,
											temp_8888_through_4444_to_565_tl);
		convertNormalizedUnsignedFixedPoint(bits_8888, bits_4444, bits_565, bits_565, texel8888_2,
											temp_8888_through_4444_to_565_tr);
		convertNormalizedUnsignedFixedPoint(bits_8888, bits_4444, bits_565, bits_565, texel8888_3,
											temp_8888_through_4444_to_565_bl);
		convertNormalizedUnsignedFixedPoint(bits_8888, bits_4444, bits_565, bits_565, texel8888_4,
											temp_8888_through_4444_to_565_br);

		convertNormalizedUnsignedFixedPoint(bits_8888, bits_4444, bits_8888, bits_8888, texel8888_1,
											temp_8888_through_4444_to_8888_tl);
		convertNormalizedUnsignedFixedPoint(bits_8888, bits_4444, bits_8888, bits_8888, texel8888_2,
											temp_8888_through_4444_to_8888_tr);
		convertNormalizedUnsignedFixedPoint(bits_8888, bits_4444, bits_8888, bits_8888, texel8888_3,
											temp_8888_through_4444_to_8888_bl);
		convertNormalizedUnsignedFixedPoint(bits_8888, bits_4444, bits_8888, bits_8888, texel8888_4,
											temp_8888_through_4444_to_8888_br);

		convertNormalizedUnsignedFixedPoint(bits_8888, bits_4444, bits_565, bits_8888, texel8888_1,
											temp_8888_through_4444_565_to_8888_tl);
		convertNormalizedUnsignedFixedPoint(bits_8888, bits_4444, bits_565, bits_8888, texel8888_2,
											temp_8888_through_4444_565_to_8888_tr);
		convertNormalizedUnsignedFixedPoint(bits_8888, bits_4444, bits_565, bits_8888, texel8888_3,
											temp_8888_through_4444_565_to_8888_bl);
		convertNormalizedUnsignedFixedPoint(bits_8888, bits_4444, bits_565, bits_8888, texel8888_4,
											temp_8888_through_4444_565_to_8888_br);

		/* GL_RGBA4 => GL_RGBA4 */
		addEntryToConversionDatabase(
			getRGBA4PixelData(1, GL_UNSIGNED_SHORT_4_4_4_4, texel4444_1[0], texel4444_1[1], texel4444_1[2],
							  texel4444_1[3]),
			getRGBA4PixelData(0, GL_UNSIGNED_BYTE, temp_4444_to_8888_tl[0], temp_4444_to_8888_tl[1],
							  temp_4444_to_8888_tl[2], temp_4444_to_8888_tl[3]),
			getRGBA4PixelData(1, GL_UNSIGNED_SHORT_4_4_4_4, texel4444_2[0], texel4444_2[1], texel4444_2[2],
							  texel4444_2[3]),
			getRGBA4PixelData(0, GL_UNSIGNED_BYTE, temp_4444_to_8888_tr[0], temp_4444_to_8888_tr[1],
							  temp_4444_to_8888_tr[2], temp_4444_to_8888_tr[3]),
			getRGBA4PixelData(1, GL_UNSIGNED_SHORT_4_4_4_4, texel4444_3[0], texel4444_3[1], texel4444_3[2],
							  texel4444_3[3]),
			getRGBA4PixelData(0, GL_UNSIGNED_BYTE, temp_4444_to_8888_bl[0], temp_4444_to_8888_bl[1],
							  temp_4444_to_8888_bl[2], temp_4444_to_8888_bl[3]),
			getRGBA4PixelData(1, GL_UNSIGNED_SHORT_4_4_4_4, texel4444_4[0], texel4444_4[1], texel4444_4[2],
							  texel4444_4[3]),
			getRGBA4PixelData(0, GL_UNSIGNED_BYTE, temp_4444_to_8888_br[0], temp_4444_to_8888_br[1],
							  temp_4444_to_8888_br[2], temp_4444_to_8888_br[3]),
			PIXEL_COMPARE_CHANNEL_RGBA);

		addEntryToConversionDatabase(
			getRGBA4PixelData(1, GL_UNSIGNED_BYTE, texel8888_1[0], texel8888_1[1], texel8888_1[2], texel8888_1[3]),
			getRGBA4PixelData(0, GL_UNSIGNED_BYTE, temp_8888_through_4444_to_8888_tl[0],
							  temp_8888_through_4444_to_8888_tl[1], temp_8888_through_4444_to_8888_tl[2],
							  temp_8888_through_4444_to_8888_tl[3]),
			getRGBA4PixelData(1, GL_UNSIGNED_BYTE, texel8888_2[0], texel8888_2[1], texel8888_2[2], texel8888_2[3]),
			getRGBA4PixelData(0, GL_UNSIGNED_BYTE, temp_8888_through_4444_to_8888_tr[0],
							  temp_8888_through_4444_to_8888_tr[1], temp_8888_through_4444_to_8888_tr[2],
							  temp_8888_through_4444_to_8888_tr[3]),
			getRGBA4PixelData(1, GL_UNSIGNED_BYTE, texel8888_3[0], texel8888_3[1], texel8888_3[2], texel8888_3[3]),
			getRGBA4PixelData(0, GL_UNSIGNED_BYTE, temp_8888_through_4444_to_8888_bl[0],
							  temp_8888_through_4444_to_8888_bl[1], temp_8888_through_4444_to_8888_bl[2],
							  temp_8888_through_4444_to_8888_bl[3]),
			getRGBA4PixelData(1, GL_UNSIGNED_BYTE, texel8888_4[0], texel8888_4[1], texel8888_4[2], texel8888_4[3]),
			getRGBA4PixelData(0, GL_UNSIGNED_BYTE, temp_8888_through_4444_to_8888_br[0],
							  temp_8888_through_4444_to_8888_br[1], temp_8888_through_4444_to_8888_br[2],
							  temp_8888_through_4444_to_8888_br[3]),
			PIXEL_COMPARE_CHANNEL_RGBA);

		/* GL_RGBA4 => GL_RGB565 */
		addEntryToConversionDatabase(getRGBA4PixelData(1, GL_UNSIGNED_SHORT_4_4_4_4, texel4444_1[0], texel4444_1[1],
													   texel4444_1[2], texel4444_1[3]),
									 getRGB565PixelData(0, GL_UNSIGNED_BYTE, temp_4444_to_565_8888_tl[0],
														temp_4444_to_565_8888_tl[1], temp_4444_to_565_8888_tl[2]),
									 getRGBA4PixelData(1, GL_UNSIGNED_SHORT_4_4_4_4, texel4444_2[0], texel4444_2[1],
													   texel4444_2[2], texel4444_2[3]),
									 getRGB565PixelData(0, GL_UNSIGNED_BYTE, temp_4444_to_565_8888_tr[0],
														temp_4444_to_565_8888_tr[1], temp_4444_to_565_8888_tr[2]),
									 getRGBA4PixelData(1, GL_UNSIGNED_SHORT_4_4_4_4, texel4444_3[0], texel4444_3[1],
													   texel4444_3[2], texel4444_3[3]),
									 getRGB565PixelData(0, GL_UNSIGNED_BYTE, temp_4444_to_565_8888_bl[0],
														temp_4444_to_565_8888_bl[1], temp_4444_to_565_8888_bl[2]),
									 getRGBA4PixelData(1, GL_UNSIGNED_SHORT_4_4_4_4, texel4444_4[0], texel4444_4[1],
													   texel4444_4[2], texel4444_4[3]),
									 getRGB565PixelData(0, GL_UNSIGNED_BYTE, temp_4444_to_565_8888_br[0],
														temp_4444_to_565_8888_br[1], temp_4444_to_565_8888_br[2]),
									 PIXEL_COMPARE_CHANNEL_RGB);

		addEntryToConversionDatabase(
			getRGBA4PixelData(1, GL_UNSIGNED_BYTE, texel8888_1[0], texel8888_1[1], texel8888_1[2], texel8888_1[3]),
			getRGB565PixelData(0, GL_UNSIGNED_BYTE, temp_8888_through_4444_565_to_8888_tl[0],
							   temp_8888_through_4444_565_to_8888_tl[1], temp_8888_through_4444_565_to_8888_tl[2]),
			getRGBA4PixelData(1, GL_UNSIGNED_BYTE, texel8888_2[0], texel8888_2[1], texel8888_2[2], texel8888_2[3]),
			getRGB565PixelData(0, GL_UNSIGNED_BYTE, temp_8888_through_4444_565_to_8888_tr[0],
							   temp_8888_through_4444_565_to_8888_tr[1], temp_8888_through_4444_565_to_8888_tr[2]),
			getRGBA4PixelData(1, GL_UNSIGNED_BYTE, texel8888_3[0], texel8888_3[1], texel8888_3[2], texel8888_3[3]),
			getRGB565PixelData(0, GL_UNSIGNED_BYTE, temp_8888_through_4444_565_to_8888_bl[0],
							   temp_8888_through_4444_565_to_8888_bl[1], temp_8888_through_4444_565_to_8888_bl[2]),
			getRGBA4PixelData(1, GL_UNSIGNED_BYTE, texel8888_4[0], texel8888_4[1], texel8888_4[2], texel8888_4[3]),
			getRGB565PixelData(0, GL_UNSIGNED_BYTE, temp_8888_through_4444_565_to_8888_br[0],
							   temp_8888_through_4444_565_to_8888_br[1], temp_8888_through_4444_565_to_8888_br[2]),
			PIXEL_COMPARE_CHANNEL_RGB);

		/* GL_RGBA4 => GL_LUMINANCE8_ALPHA8_OES */
		addEntryToConversionDatabase(
			getRGBA4PixelData(1, GL_UNSIGNED_SHORT_4_4_4_4, texel4444_1[0], texel4444_1[1], texel4444_1[2],
							  texel4444_1[3]),
			getLuminance8Alpha8OESPixelData(GL_UNSIGNED_BYTE, temp_4444_to_8888_tl[0], temp_4444_to_8888_tl[3]),
			getRGBA4PixelData(1, GL_UNSIGNED_SHORT_4_4_4_4, texel4444_2[0], texel4444_2[1], texel4444_2[2],
							  texel4444_2[3]),
			getLuminance8Alpha8OESPixelData(GL_UNSIGNED_BYTE, temp_4444_to_8888_tr[0], temp_4444_to_8888_tr[3]),
			getRGBA4PixelData(1, GL_UNSIGNED_SHORT_4_4_4_4, texel4444_3[0], texel4444_3[1], texel4444_3[2],
							  texel4444_3[3]),
			getLuminance8Alpha8OESPixelData(GL_UNSIGNED_BYTE, temp_4444_to_8888_bl[0], temp_4444_to_8888_bl[3]),
			getRGBA4PixelData(1, GL_UNSIGNED_SHORT_4_4_4_4, texel4444_4[0], texel4444_4[1], texel4444_4[2],
							  texel4444_4[3]),
			getLuminance8Alpha8OESPixelData(GL_UNSIGNED_BYTE, temp_4444_to_8888_br[0], temp_4444_to_8888_br[3]),
			PIXEL_COMPARE_CHANNEL_RA);

		addEntryToConversionDatabase(
			getRGBA4PixelData(1, GL_UNSIGNED_BYTE, texel4444_1[0], texel4444_1[1], texel4444_1[2], texel4444_1[3]),
			getLuminance8Alpha8OESPixelData(GL_UNSIGNED_BYTE, temp_8888_through_4444_to_8888_tl[0],
											temp_8888_through_4444_to_8888_tl[3]),
			getRGBA4PixelData(1, GL_UNSIGNED_BYTE, texel4444_2[0], texel4444_2[1], texel4444_2[2], texel4444_2[3]),
			getLuminance8Alpha8OESPixelData(GL_UNSIGNED_BYTE, temp_8888_through_4444_to_8888_tr[0],
											temp_8888_through_4444_to_8888_tr[3]),
			getRGBA4PixelData(1, GL_UNSIGNED_BYTE, texel4444_3[0], texel4444_3[1], texel4444_3[2], texel4444_3[3]),
			getLuminance8Alpha8OESPixelData(GL_UNSIGNED_BYTE, temp_8888_through_4444_to_8888_bl[0],
											temp_8888_through_4444_to_8888_bl[3]),
			getRGBA4PixelData(1, GL_UNSIGNED_BYTE, texel4444_4[0], texel4444_4[1], texel4444_4[2], texel4444_4[3]),
			getLuminance8Alpha8OESPixelData(GL_UNSIGNED_BYTE, temp_8888_through_4444_to_8888_br[0],
											temp_8888_through_4444_to_8888_br[3]),
			PIXEL_COMPARE_CHANNEL_RA);

		/* GL_RGBA4 => GL_LUMINANCE8_OES */
		addEntryToConversionDatabase(getRGBA4PixelData(1, GL_UNSIGNED_SHORT_4_4_4_4, texel4444_1[0], texel4444_1[1],
													   texel4444_1[2], texel4444_1[3]),
									 getLuminance8OESPixelData(GL_UNSIGNED_BYTE, temp_4444_to_8888_tl[0]),
									 getRGBA4PixelData(1, GL_UNSIGNED_SHORT_4_4_4_4, texel4444_2[0], texel4444_2[1],
													   texel4444_2[2], texel4444_2[3]),
									 getLuminance8OESPixelData(GL_UNSIGNED_BYTE, temp_4444_to_8888_tr[0]),
									 getRGBA4PixelData(1, GL_UNSIGNED_SHORT_4_4_4_4, texel4444_3[0], texel4444_3[1],
													   texel4444_3[2], texel4444_3[3]),
									 getLuminance8OESPixelData(GL_UNSIGNED_BYTE, temp_4444_to_8888_bl[0]),
									 getRGBA4PixelData(1, GL_UNSIGNED_SHORT_4_4_4_4, texel4444_4[0], texel4444_4[1],
													   texel4444_4[2], texel4444_4[3]),
									 getLuminance8OESPixelData(GL_UNSIGNED_BYTE, temp_4444_to_8888_br[0]),
									 PIXEL_COMPARE_CHANNEL_R);

		addEntryToConversionDatabase(
			getRGBA4PixelData(1, GL_UNSIGNED_BYTE, texel4444_1[0], texel4444_1[1], texel4444_1[2], texel4444_1[3]),
			getLuminance8OESPixelData(GL_UNSIGNED_BYTE, temp_8888_through_4444_to_8888_tl[0]),
			getRGBA4PixelData(1, GL_UNSIGNED_BYTE, texel4444_2[0], texel4444_2[1], texel4444_2[2], texel4444_2[3]),
			getLuminance8OESPixelData(GL_UNSIGNED_BYTE, temp_8888_through_4444_to_8888_tr[0]),
			getRGBA4PixelData(1, GL_UNSIGNED_BYTE, texel4444_3[0], texel4444_3[1], texel4444_3[2], texel4444_3[3]),
			getLuminance8OESPixelData(GL_UNSIGNED_BYTE, temp_8888_through_4444_to_8888_bl[0]),
			getRGBA4PixelData(1, GL_UNSIGNED_BYTE, texel4444_4[0], texel4444_4[1], texel4444_4[2], texel4444_4[3]),
			getLuminance8OESPixelData(GL_UNSIGNED_BYTE, temp_8888_through_4444_to_8888_br[0]), PIXEL_COMPARE_CHANNEL_R);

		/* GL_RGBA4 => GL_ALPHA8_OES */
		addEntryToConversionDatabase(getRGBA4PixelData(1, GL_UNSIGNED_SHORT_4_4_4_4, texel4444_1[0], texel4444_1[1],
													   texel4444_1[2], texel4444_1[3]),
									 getAlpha8OESPixelData(GL_UNSIGNED_BYTE, temp_4444_to_8888_tl[3]),
									 getRGBA4PixelData(1, GL_UNSIGNED_SHORT_4_4_4_4, texel4444_2[0], texel4444_2[1],
													   texel4444_2[2], texel4444_2[3]),
									 getAlpha8OESPixelData(GL_UNSIGNED_BYTE, temp_4444_to_8888_tr[3]),
									 getRGBA4PixelData(1, GL_UNSIGNED_SHORT_4_4_4_4, texel4444_3[0], texel4444_3[1],
													   texel4444_3[2], texel4444_3[3]),
									 getAlpha8OESPixelData(GL_UNSIGNED_BYTE, temp_4444_to_8888_bl[3]),
									 getRGBA4PixelData(1, GL_UNSIGNED_SHORT_4_4_4_4, texel4444_4[0], texel4444_4[1],
													   texel4444_4[2], texel4444_4[3]),
									 getAlpha8OESPixelData(GL_UNSIGNED_BYTE, temp_4444_to_8888_br[3]),
									 PIXEL_COMPARE_CHANNEL_A);

		addEntryToConversionDatabase(
			getRGBA4PixelData(1, GL_UNSIGNED_BYTE, texel4444_1[0], texel4444_1[1], texel4444_1[2], texel4444_1[3]),
			getAlpha8OESPixelData(GL_UNSIGNED_BYTE, temp_8888_through_4444_to_8888_tl[3]),
			getRGBA4PixelData(1, GL_UNSIGNED_BYTE, texel4444_2[0], texel4444_2[1], texel4444_2[2], texel4444_2[3]),
			getAlpha8OESPixelData(GL_UNSIGNED_BYTE, temp_8888_through_4444_to_8888_tr[3]),
			getRGBA4PixelData(1, GL_UNSIGNED_BYTE, texel4444_3[0], texel4444_3[1], texel4444_3[2], texel4444_3[3]),
			getAlpha8OESPixelData(GL_UNSIGNED_BYTE, temp_8888_through_4444_to_8888_bl[3]),
			getRGBA4PixelData(1, GL_UNSIGNED_BYTE, texel4444_4[0], texel4444_4[1], texel4444_4[2], texel4444_4[3]),
			getAlpha8OESPixelData(GL_UNSIGNED_BYTE, temp_8888_through_4444_to_8888_br[3]), PIXEL_COMPARE_CHANNEL_A);
	}

	/* GL_RGB5_A1 */
	{
		int texel2101010_1[4] = { 1023, 703, 383, 2 };
		int texel2101010_2[4] = { 703, 383, 0, 0 };
		int texel2101010_3[4] = { 383, 0, 1023, 2 };
		int texel2101010_4[4] = { 0, 1023, 703, 0 };
		int texel5551_1[4]	= { 31, 21, 11, 1 };
		int texel5551_2[4]	= { 21, 11, 0, 0 };
		int texel5551_3[4]	= { 11, 0, 31, 1 };
		int texel5551_4[4]	= { 0, 31, 21, 0 };
		int texel8888_1[4]	= { 255, 207, 95, 255 };
		int texel8888_2[4]	= { 207, 95, 0, 0 };
		int texel8888_3[4]	= { 95, 0, 255, 255 };
		int texel8888_4[4]	= { 0, 255, 207, 0 };

		int temp_2101010rev_through_5551_to_8888_tl[4]	 = { 0 };
		int temp_2101010rev_through_5551_to_8888_tr[4]	 = { 0 };
		int temp_2101010rev_through_5551_to_8888_bl[4]	 = { 0 };
		int temp_2101010rev_through_5551_to_8888_br[4]	 = { 0 };
		int temp_2101010rev_through_5551_565_to_8888_tl[4] = { 0 };
		int temp_2101010rev_through_5551_565_to_8888_tr[4] = { 0 };
		int temp_2101010rev_through_5551_565_to_8888_bl[4] = { 0 };
		int temp_2101010rev_through_5551_565_to_8888_br[4] = { 0 };
		int temp_5551_to_8888_tl[4]						   = { 0 };
		int temp_5551_to_8888_tr[4]						   = { 0 };
		int temp_5551_to_8888_bl[4]						   = { 0 };
		int temp_5551_to_8888_br[4]						   = { 0 };
		int temp_5551_through_565_to_8888_tl[4]			   = { 0 };
		int temp_5551_through_565_to_8888_tr[4]			   = { 0 };
		int temp_5551_through_565_to_8888_bl[4]			   = { 0 };
		int temp_5551_through_565_to_8888_br[4]			   = { 0 };
		int temp_8888_through_5551_to_8888_tl[4]		   = { 0 };
		int temp_8888_through_5551_to_8888_tr[4]		   = { 0 };
		int temp_8888_through_5551_to_8888_bl[4]		   = { 0 };
		int temp_8888_through_5551_to_8888_br[4]		   = { 0 };
		int temp_8888_through_5551_565_to_8888_tl[4]	   = { 0 };
		int temp_8888_through_5551_565_to_8888_tr[4]	   = { 0 };
		int temp_8888_through_5551_565_to_8888_bl[4]	   = { 0 };
		int temp_8888_through_5551_565_to_8888_br[4]	   = { 0 };

		convertNormalizedUnsignedFixedPoint(bits_1010102, bits_5551, bits_8888, bits_8888, texel2101010_1,
											temp_2101010rev_through_5551_to_8888_tl);
		convertNormalizedUnsignedFixedPoint(bits_1010102, bits_5551, bits_8888, bits_8888, texel2101010_2,
											temp_2101010rev_through_5551_to_8888_tr);
		convertNormalizedUnsignedFixedPoint(bits_1010102, bits_5551, bits_8888, bits_8888, texel2101010_3,
											temp_2101010rev_through_5551_to_8888_bl);
		convertNormalizedUnsignedFixedPoint(bits_1010102, bits_5551, bits_8888, bits_8888, texel2101010_4,
											temp_2101010rev_through_5551_to_8888_br);

		convertNormalizedUnsignedFixedPoint(bits_1010102, bits_5551, bits_565, bits_8888, texel2101010_1,
											temp_2101010rev_through_5551_565_to_8888_tl);
		convertNormalizedUnsignedFixedPoint(bits_1010102, bits_5551, bits_565, bits_8888, texel2101010_2,
											temp_2101010rev_through_5551_565_to_8888_tr);
		convertNormalizedUnsignedFixedPoint(bits_1010102, bits_5551, bits_565, bits_8888, texel2101010_3,
											temp_2101010rev_through_5551_565_to_8888_bl);
		convertNormalizedUnsignedFixedPoint(bits_1010102, bits_5551, bits_565, bits_8888, texel2101010_4,
											temp_2101010rev_through_5551_565_to_8888_br);

		convertNormalizedUnsignedFixedPoint(bits_5551, bits_8888, bits_8888, bits_8888, texel5551_1,
											temp_5551_to_8888_tl);
		convertNormalizedUnsignedFixedPoint(bits_5551, bits_8888, bits_8888, bits_8888, texel5551_2,
											temp_5551_to_8888_tr);
		convertNormalizedUnsignedFixedPoint(bits_5551, bits_8888, bits_8888, bits_8888, texel5551_3,
											temp_5551_to_8888_bl);
		convertNormalizedUnsignedFixedPoint(bits_5551, bits_8888, bits_8888, bits_8888, texel5551_4,
											temp_5551_to_8888_br);

		convertNormalizedUnsignedFixedPoint(bits_8888, bits_5551, bits_8888, bits_8888, texel8888_1,
											temp_8888_through_5551_to_8888_tl);
		convertNormalizedUnsignedFixedPoint(bits_8888, bits_5551, bits_8888, bits_8888, texel8888_2,
											temp_8888_through_5551_to_8888_tr);
		convertNormalizedUnsignedFixedPoint(bits_8888, bits_5551, bits_8888, bits_8888, texel8888_3,
											temp_8888_through_5551_to_8888_bl);
		convertNormalizedUnsignedFixedPoint(bits_8888, bits_5551, bits_8888, bits_8888, texel8888_4,
											temp_8888_through_5551_to_8888_br);

		convertNormalizedUnsignedFixedPoint(bits_8888, bits_5551, bits_565, bits_8888, texel8888_1,
											temp_8888_through_5551_565_to_8888_tl);
		convertNormalizedUnsignedFixedPoint(bits_8888, bits_5551, bits_565, bits_8888, texel8888_2,
											temp_8888_through_5551_565_to_8888_tr);
		convertNormalizedUnsignedFixedPoint(bits_8888, bits_5551, bits_565, bits_8888, texel8888_3,
											temp_8888_through_5551_565_to_8888_bl);
		convertNormalizedUnsignedFixedPoint(bits_8888, bits_5551, bits_565, bits_8888, texel8888_4,
											temp_8888_through_5551_565_to_8888_br);

		convertNormalizedUnsignedFixedPoint(bits_5551, bits_565, bits_8888, bits_8888, texel5551_1,
											temp_5551_through_565_to_8888_tl);
		convertNormalizedUnsignedFixedPoint(bits_5551, bits_565, bits_8888, bits_8888, texel5551_2,
											temp_5551_through_565_to_8888_tr);
		convertNormalizedUnsignedFixedPoint(bits_5551, bits_565, bits_8888, bits_8888, texel5551_3,
											temp_5551_through_565_to_8888_bl);
		convertNormalizedUnsignedFixedPoint(bits_5551, bits_565, bits_8888, bits_8888, texel5551_4,
											temp_5551_through_565_to_8888_br);

		/* GL_RGB5_A1 => GL_RGB5_A1 */
		addEntryToConversionDatabase(
			getRGB5A1PixelData(1, GL_UNSIGNED_BYTE, texel8888_1[0], texel8888_1[1], texel8888_1[2], texel8888_1[3]),
			getRGB5A1PixelData(0, GL_UNSIGNED_BYTE, temp_8888_through_5551_to_8888_tl[0],
							   temp_8888_through_5551_to_8888_tl[1], temp_8888_through_5551_to_8888_tl[2],
							   temp_8888_through_5551_to_8888_tl[3]),
			getRGB5A1PixelData(1, GL_UNSIGNED_BYTE, texel8888_2[0], texel8888_2[1], texel8888_2[2], texel8888_2[3]),
			getRGB5A1PixelData(0, GL_UNSIGNED_BYTE, temp_8888_through_5551_to_8888_tr[0],
							   temp_8888_through_5551_to_8888_tr[1], temp_8888_through_5551_to_8888_tr[2],
							   temp_8888_through_5551_to_8888_tr[3]),
			getRGB5A1PixelData(1, GL_UNSIGNED_BYTE, texel8888_3[0], texel8888_3[1], texel8888_3[2], texel8888_3[3]),
			getRGB5A1PixelData(0, GL_UNSIGNED_BYTE, temp_8888_through_5551_to_8888_bl[0],
							   temp_8888_through_5551_to_8888_bl[1], temp_8888_through_5551_to_8888_bl[2],
							   temp_8888_through_5551_to_8888_bl[3]),
			getRGB5A1PixelData(1, GL_UNSIGNED_BYTE, texel8888_4[0], texel8888_4[1], texel8888_4[2], texel8888_4[3]),
			getRGB5A1PixelData(0, GL_UNSIGNED_BYTE, temp_8888_through_5551_to_8888_br[0],
							   temp_8888_through_5551_to_8888_br[1], temp_8888_through_5551_to_8888_br[2],
							   temp_8888_through_5551_to_8888_br[3]),
			PIXEL_COMPARE_CHANNEL_RGBA);

		addEntryToConversionDatabase(
			getRGB5A1PixelData(1, GL_UNSIGNED_SHORT_5_5_5_1, texel5551_1[0], texel5551_1[1], texel5551_1[2],
							   texel5551_1[3]),
			getRGB5A1PixelData(0, GL_UNSIGNED_BYTE, temp_5551_to_8888_tl[0], temp_5551_to_8888_tl[1],
							   temp_5551_to_8888_tl[2], temp_5551_to_8888_tl[3]),
			getRGB5A1PixelData(1, GL_UNSIGNED_SHORT_5_5_5_1, texel5551_2[0], texel5551_2[1], texel5551_2[2],
							   texel5551_2[3]),
			getRGB5A1PixelData(0, GL_UNSIGNED_BYTE, temp_5551_to_8888_tr[0], temp_5551_to_8888_tr[1],
							   temp_5551_to_8888_tr[2], temp_5551_to_8888_tr[3]),
			getRGB5A1PixelData(1, GL_UNSIGNED_SHORT_5_5_5_1, texel5551_3[0], texel5551_3[1], texel5551_3[2],
							   texel5551_3[3]),
			getRGB5A1PixelData(0, GL_UNSIGNED_BYTE, temp_5551_to_8888_bl[0], temp_5551_to_8888_bl[1],
							   temp_5551_to_8888_bl[2], temp_5551_to_8888_bl[3]),
			getRGB5A1PixelData(1, GL_UNSIGNED_SHORT_5_5_5_1, texel5551_4[0], texel5551_4[1], texel5551_4[2],
							   texel5551_4[3]),
			getRGB5A1PixelData(0, GL_UNSIGNED_BYTE, temp_5551_to_8888_br[0], temp_5551_to_8888_br[1],
							   temp_5551_to_8888_br[2], temp_5551_to_8888_br[3]),
			PIXEL_COMPARE_CHANNEL_RGBA);

		addEntryToConversionDatabase(
			getRGB5A1PixelData(1, GL_UNSIGNED_INT_2_10_10_10_REV, texel2101010_1[0], texel2101010_1[1],
							   texel2101010_1[2], texel2101010_1[3]),
			getRGB5A1PixelData(0, GL_UNSIGNED_BYTE, temp_2101010rev_through_5551_to_8888_tl[0],
							   temp_2101010rev_through_5551_to_8888_tl[1], temp_2101010rev_through_5551_to_8888_tl[2],
							   temp_2101010rev_through_5551_to_8888_tl[3]),
			getRGB5A1PixelData(1, GL_UNSIGNED_INT_2_10_10_10_REV, texel2101010_2[0], texel2101010_2[1],
							   texel2101010_2[2], texel2101010_2[3]),
			getRGB5A1PixelData(0, GL_UNSIGNED_BYTE, temp_2101010rev_through_5551_to_8888_tr[0],
							   temp_2101010rev_through_5551_to_8888_tr[1], temp_2101010rev_through_5551_to_8888_tr[2],
							   temp_2101010rev_through_5551_to_8888_tr[3]),
			getRGB5A1PixelData(1, GL_UNSIGNED_INT_2_10_10_10_REV, texel2101010_3[0], texel2101010_3[1],
							   texel2101010_3[2], texel2101010_3[3]),
			getRGB5A1PixelData(0, GL_UNSIGNED_BYTE, temp_2101010rev_through_5551_to_8888_bl[0],
							   temp_2101010rev_through_5551_to_8888_bl[1], temp_2101010rev_through_5551_to_8888_bl[2],
							   temp_2101010rev_through_5551_to_8888_bl[3]),
			getRGB5A1PixelData(1, GL_UNSIGNED_INT_2_10_10_10_REV, texel2101010_4[0], texel2101010_4[1],
							   texel2101010_4[2], texel2101010_4[3]),
			getRGB5A1PixelData(0, GL_UNSIGNED_BYTE, temp_2101010rev_through_5551_to_8888_br[0],
							   temp_2101010rev_through_5551_to_8888_br[1], temp_2101010rev_through_5551_to_8888_br[2],
							   temp_2101010rev_through_5551_to_8888_br[3]),
			PIXEL_COMPARE_CHANNEL_RGBA);

		/* GL_RGB5_A1 => GL_RGB565 */
		addEntryToConversionDatabase(
			getRGB5A1PixelData(1, GL_UNSIGNED_BYTE, texel8888_1[0], texel8888_1[1], texel8888_1[2], texel8888_1[3]),
			getRGB565PixelData(0, GL_UNSIGNED_BYTE, temp_8888_through_5551_565_to_8888_tl[0],
							   temp_8888_through_5551_565_to_8888_tl[1], temp_8888_through_5551_565_to_8888_tl[2]),
			getRGB5A1PixelData(1, GL_UNSIGNED_BYTE, texel8888_2[0], texel8888_2[1], texel8888_2[2], texel8888_2[3]),
			getRGB565PixelData(0, GL_UNSIGNED_BYTE, temp_8888_through_5551_565_to_8888_tr[0],
							   temp_8888_through_5551_565_to_8888_tr[1], temp_8888_through_5551_565_to_8888_tr[2]),
			getRGB5A1PixelData(1, GL_UNSIGNED_BYTE, texel8888_3[0], texel8888_3[1], texel8888_3[2], texel8888_3[3]),
			getRGB565PixelData(0, GL_UNSIGNED_BYTE, temp_8888_through_5551_565_to_8888_bl[0],
							   temp_8888_through_5551_565_to_8888_bl[1], temp_8888_through_5551_565_to_8888_bl[2]),
			getRGB5A1PixelData(1, GL_UNSIGNED_BYTE, texel8888_4[0], texel8888_4[1], texel8888_4[2], texel8888_4[3]),
			getRGB565PixelData(0, GL_UNSIGNED_BYTE, temp_8888_through_5551_565_to_8888_br[0],
							   temp_8888_through_5551_565_to_8888_br[1], temp_8888_through_5551_565_to_8888_br[2]),
			PIXEL_COMPARE_CHANNEL_RGB);

		addEntryToConversionDatabase(
			getRGB5A1PixelData(1, GL_UNSIGNED_SHORT_5_5_5_1, texel5551_1[0], texel5551_1[1], texel5551_1[2],
							   texel5551_1[3]),
			getRGB565PixelData(0, GL_UNSIGNED_BYTE, temp_5551_through_565_to_8888_tl[0],
							   temp_5551_through_565_to_8888_tl[1], temp_5551_through_565_to_8888_tl[2]),
			getRGB5A1PixelData(1, GL_UNSIGNED_SHORT_5_5_5_1, texel5551_2[0], texel5551_2[1], texel5551_2[2],
							   texel5551_2[3]),
			getRGB565PixelData(0, GL_UNSIGNED_BYTE, temp_5551_through_565_to_8888_tr[0],
							   temp_5551_through_565_to_8888_tr[1], temp_5551_through_565_to_8888_tr[2]),
			getRGB5A1PixelData(1, GL_UNSIGNED_SHORT_5_5_5_1, texel5551_3[0], texel5551_3[1], texel5551_3[2],
							   texel5551_3[3]),
			getRGB565PixelData(0, GL_UNSIGNED_BYTE, temp_5551_through_565_to_8888_bl[0],
							   temp_5551_through_565_to_8888_bl[1], temp_5551_through_565_to_8888_bl[2]),
			getRGB5A1PixelData(1, GL_UNSIGNED_SHORT_5_5_5_1, texel5551_4[0], texel5551_4[1], texel5551_4[2],
							   texel5551_4[3]),
			getRGB565PixelData(0, GL_UNSIGNED_BYTE, temp_5551_through_565_to_8888_br[0],
							   temp_5551_through_565_to_8888_br[1], temp_5551_through_565_to_8888_br[2]),
			PIXEL_COMPARE_CHANNEL_RGB);

		addEntryToConversionDatabase(
			getRGB5A1PixelData(1, GL_UNSIGNED_INT_2_10_10_10_REV, texel2101010_1[0], texel2101010_1[1],
							   texel2101010_1[2], texel2101010_1[3]),
			getRGB565PixelData(0, GL_UNSIGNED_BYTE, temp_2101010rev_through_5551_565_to_8888_tl[0],
							   temp_2101010rev_through_5551_565_to_8888_tl[1],
							   temp_2101010rev_through_5551_565_to_8888_tl[2]),
			getRGB5A1PixelData(1, GL_UNSIGNED_INT_2_10_10_10_REV, texel2101010_2[0], texel2101010_2[1],
							   texel2101010_2[2], texel2101010_2[3]),
			getRGB565PixelData(0, GL_UNSIGNED_BYTE, temp_2101010rev_through_5551_565_to_8888_tr[0],
							   temp_2101010rev_through_5551_565_to_8888_tr[1],
							   temp_2101010rev_through_5551_565_to_8888_tr[2]),
			getRGB5A1PixelData(1, GL_UNSIGNED_INT_2_10_10_10_REV, texel2101010_3[0], texel2101010_3[1],
							   texel2101010_3[2], texel2101010_3[3]),
			getRGB565PixelData(0, GL_UNSIGNED_BYTE, temp_2101010rev_through_5551_565_to_8888_bl[0],
							   temp_2101010rev_through_5551_565_to_8888_bl[1],
							   temp_2101010rev_through_5551_565_to_8888_bl[2]),
			getRGB5A1PixelData(1, GL_UNSIGNED_INT_2_10_10_10_REV, texel2101010_4[0], texel2101010_4[1],
							   texel2101010_4[2], texel2101010_4[3]),
			getRGB565PixelData(0, GL_UNSIGNED_BYTE, temp_2101010rev_through_5551_565_to_8888_br[0],
							   temp_2101010rev_through_5551_565_to_8888_br[1],
							   temp_2101010rev_through_5551_565_to_8888_br[2]),
			PIXEL_COMPARE_CHANNEL_RGB);

		/* GL_RGB5_A1 => GL_LUMINANCE8_ALPHA8_OES */
		addEntryToConversionDatabase(
			getRGB5A1PixelData(1, GL_UNSIGNED_BYTE, texel8888_1[0], texel8888_1[1], texel8888_1[2], texel8888_1[3]),
			getLuminance8Alpha8OESPixelData(GL_UNSIGNED_BYTE, temp_8888_through_5551_to_8888_tl[0],
											temp_8888_through_5551_to_8888_tl[3]),
			getRGB5A1PixelData(1, GL_UNSIGNED_BYTE, texel8888_2[0], texel8888_2[1], texel8888_2[2], texel8888_2[3]),
			getLuminance8Alpha8OESPixelData(GL_UNSIGNED_BYTE, temp_8888_through_5551_to_8888_tr[0],
											temp_8888_through_5551_to_8888_tr[3]),
			getRGB5A1PixelData(1, GL_UNSIGNED_BYTE, texel8888_3[0], texel8888_3[1], texel8888_3[2], texel8888_3[3]),
			getLuminance8Alpha8OESPixelData(GL_UNSIGNED_BYTE, temp_8888_through_5551_to_8888_bl[0],
											temp_8888_through_5551_to_8888_bl[3]),
			getRGB5A1PixelData(1, GL_UNSIGNED_BYTE, texel8888_4[0], texel8888_4[1], texel8888_4[2], texel8888_4[3]),
			getLuminance8Alpha8OESPixelData(GL_UNSIGNED_BYTE, temp_8888_through_5551_to_8888_br[0],
											temp_8888_through_5551_to_8888_br[3]),
			PIXEL_COMPARE_CHANNEL_RA);

		addEntryToConversionDatabase(
			getRGB5A1PixelData(1, GL_UNSIGNED_SHORT_5_5_5_1, texel5551_1[0], texel5551_1[1], texel5551_1[2],
							   texel5551_1[3]),
			getLuminance8Alpha8OESPixelData(GL_UNSIGNED_BYTE, temp_5551_to_8888_tl[0], temp_5551_to_8888_tl[3]),
			getRGB5A1PixelData(1, GL_UNSIGNED_SHORT_5_5_5_1, texel5551_2[0], texel5551_2[1], texel5551_2[2],
							   texel5551_2[3]),
			getLuminance8Alpha8OESPixelData(GL_UNSIGNED_BYTE, temp_5551_to_8888_tr[0], temp_5551_to_8888_tr[3]),
			getRGB5A1PixelData(1, GL_UNSIGNED_SHORT_5_5_5_1, texel5551_3[0], texel5551_3[1], texel5551_3[2],
							   texel5551_3[3]),
			getLuminance8Alpha8OESPixelData(GL_UNSIGNED_BYTE, temp_5551_to_8888_bl[0], temp_5551_to_8888_bl[3]),
			getRGB5A1PixelData(1, GL_UNSIGNED_SHORT_5_5_5_1, texel5551_4[0], texel5551_4[1], texel5551_4[2],
							   texel5551_4[3]),
			getLuminance8Alpha8OESPixelData(GL_UNSIGNED_BYTE, temp_5551_to_8888_br[0], temp_5551_to_8888_br[3]),
			PIXEL_COMPARE_CHANNEL_RA);

		addEntryToConversionDatabase(
			getRGB5A1PixelData(1, GL_UNSIGNED_INT_2_10_10_10_REV, texel2101010_1[0], texel2101010_1[1],
							   texel2101010_1[2], texel2101010_1[3]),
			getLuminance8Alpha8OESPixelData(GL_UNSIGNED_BYTE, temp_2101010rev_through_5551_to_8888_tl[0],
											temp_2101010rev_through_5551_to_8888_tl[3]),
			getRGB5A1PixelData(1, GL_UNSIGNED_INT_2_10_10_10_REV, texel2101010_2[0], texel2101010_2[1],
							   texel2101010_2[2], texel2101010_2[3]),
			getLuminance8Alpha8OESPixelData(GL_UNSIGNED_BYTE, temp_2101010rev_through_5551_to_8888_tr[0],
											temp_2101010rev_through_5551_to_8888_tr[3]),
			getRGB5A1PixelData(1, GL_UNSIGNED_INT_2_10_10_10_REV, texel2101010_3[0], texel2101010_3[1],
							   texel2101010_3[2], texel2101010_3[3]),
			getLuminance8Alpha8OESPixelData(GL_UNSIGNED_BYTE, temp_2101010rev_through_5551_to_8888_bl[0],
											temp_2101010rev_through_5551_to_8888_bl[3]),
			getRGB5A1PixelData(1, GL_UNSIGNED_INT_2_10_10_10_REV, texel2101010_4[0], texel2101010_4[1],
							   texel2101010_4[2], texel2101010_4[3]),
			getLuminance8Alpha8OESPixelData(GL_UNSIGNED_BYTE, temp_2101010rev_through_5551_to_8888_br[0],
											temp_2101010rev_through_5551_to_8888_br[3]),
			PIXEL_COMPARE_CHANNEL_RA);

		/* GL_RGB5_A1 => GL_LUMINANCE8_OES */
		addEntryToConversionDatabase(
			getRGB5A1PixelData(1, GL_UNSIGNED_BYTE, texel8888_1[0], texel8888_1[1], texel8888_1[2], texel8888_1[3]),
			getLuminance8OESPixelData(GL_UNSIGNED_BYTE, temp_8888_through_5551_to_8888_tl[0]),
			getRGB5A1PixelData(1, GL_UNSIGNED_BYTE, texel8888_2[0], texel8888_2[1], texel8888_2[2], texel8888_2[3]),
			getLuminance8OESPixelData(GL_UNSIGNED_BYTE, temp_8888_through_5551_to_8888_tr[0]),
			getRGB5A1PixelData(1, GL_UNSIGNED_BYTE, texel8888_3[0], texel8888_3[1], texel8888_3[2], texel8888_3[3]),
			getLuminance8OESPixelData(GL_UNSIGNED_BYTE, temp_8888_through_5551_to_8888_bl[0]),
			getRGB5A1PixelData(1, GL_UNSIGNED_BYTE, texel8888_4[0], texel8888_4[1], texel8888_4[2], texel8888_4[3]),
			getLuminance8OESPixelData(GL_UNSIGNED_BYTE, temp_8888_through_5551_to_8888_br[0]), PIXEL_COMPARE_CHANNEL_R);

		addEntryToConversionDatabase(getRGB5A1PixelData(1, GL_UNSIGNED_SHORT_5_5_5_1, texel5551_1[0], texel5551_1[1],
														texel5551_1[2], texel5551_1[3]),
									 getLuminance8OESPixelData(GL_UNSIGNED_BYTE, temp_5551_to_8888_tl[0]),
									 getRGB5A1PixelData(1, GL_UNSIGNED_SHORT_5_5_5_1, texel5551_2[0], texel5551_2[1],
														texel5551_2[2], texel5551_2[3]),
									 getLuminance8OESPixelData(GL_UNSIGNED_BYTE, temp_5551_to_8888_tr[0]),
									 getRGB5A1PixelData(1, GL_UNSIGNED_SHORT_5_5_5_1, texel5551_3[0], texel5551_3[1],
														texel5551_3[2], texel5551_3[3]),
									 getLuminance8OESPixelData(GL_UNSIGNED_BYTE, temp_5551_to_8888_bl[0]),
									 getRGB5A1PixelData(1, GL_UNSIGNED_SHORT_5_5_5_1, texel5551_4[0], texel5551_4[1],
														texel5551_4[2], texel5551_4[3]),
									 getLuminance8OESPixelData(GL_UNSIGNED_BYTE, temp_5551_to_8888_br[0]),
									 PIXEL_COMPARE_CHANNEL_R);

		addEntryToConversionDatabase(
			getRGB5A1PixelData(1, GL_UNSIGNED_INT_2_10_10_10_REV, texel2101010_1[0], texel2101010_1[1],
							   texel2101010_1[2], texel2101010_1[3]),
			getLuminance8OESPixelData(GL_UNSIGNED_BYTE, temp_2101010rev_through_5551_to_8888_tl[0]),
			getRGB5A1PixelData(1, GL_UNSIGNED_INT_2_10_10_10_REV, texel2101010_2[0], texel2101010_2[1],
							   texel2101010_2[2], texel2101010_2[3]),
			getLuminance8OESPixelData(GL_UNSIGNED_BYTE, temp_2101010rev_through_5551_to_8888_tr[0]),
			getRGB5A1PixelData(1, GL_UNSIGNED_INT_2_10_10_10_REV, texel2101010_3[0], texel2101010_3[1],
							   texel2101010_3[2], texel2101010_3[3]),
			getLuminance8OESPixelData(GL_UNSIGNED_BYTE, temp_2101010rev_through_5551_to_8888_bl[0]),
			getRGB5A1PixelData(1, GL_UNSIGNED_INT_2_10_10_10_REV, texel2101010_4[0], texel2101010_4[1],
							   texel2101010_4[2], texel2101010_4[3]),
			getLuminance8OESPixelData(GL_UNSIGNED_BYTE, temp_2101010rev_through_5551_to_8888_br[0]),
			PIXEL_COMPARE_CHANNEL_R);

		/* GL_RGB5_A1 => GL_ALPHA8_OES */
		addEntryToConversionDatabase(
			getRGB5A1PixelData(1, GL_UNSIGNED_BYTE, texel8888_1[0], texel8888_1[1], texel8888_1[2], texel8888_1[3]),
			getAlpha8OESPixelData(GL_UNSIGNED_BYTE, temp_8888_through_5551_to_8888_tl[3]),
			getRGB5A1PixelData(1, GL_UNSIGNED_BYTE, texel8888_2[0], texel8888_2[1], texel8888_2[2], texel8888_2[3]),
			getAlpha8OESPixelData(GL_UNSIGNED_BYTE, temp_8888_through_5551_to_8888_tr[3]),
			getRGB5A1PixelData(1, GL_UNSIGNED_BYTE, texel8888_3[0], texel8888_3[1], texel8888_3[2], texel8888_3[3]),
			getAlpha8OESPixelData(GL_UNSIGNED_BYTE, temp_8888_through_5551_to_8888_bl[3]),
			getRGB5A1PixelData(1, GL_UNSIGNED_BYTE, texel8888_4[0], texel8888_4[1], texel8888_4[2], texel8888_4[3]),
			getAlpha8OESPixelData(GL_UNSIGNED_BYTE, temp_8888_through_5551_to_8888_br[3]), PIXEL_COMPARE_CHANNEL_A);

		addEntryToConversionDatabase(getRGB5A1PixelData(1, GL_UNSIGNED_SHORT_5_5_5_1, texel5551_1[0], texel5551_1[1],
														texel5551_1[2], texel5551_1[3]),
									 getAlpha8OESPixelData(GL_UNSIGNED_BYTE, temp_5551_to_8888_tl[3]),
									 getRGB5A1PixelData(1, GL_UNSIGNED_SHORT_5_5_5_1, texel5551_2[0], texel5551_2[1],
														texel5551_2[2], texel5551_2[3]),
									 getAlpha8OESPixelData(GL_UNSIGNED_BYTE, temp_5551_to_8888_tr[3]),
									 getRGB5A1PixelData(1, GL_UNSIGNED_SHORT_5_5_5_1, texel5551_3[0], texel5551_3[1],
														texel5551_3[2], texel5551_3[3]),
									 getAlpha8OESPixelData(GL_UNSIGNED_BYTE, temp_5551_to_8888_bl[3]),
									 getRGB5A1PixelData(1, GL_UNSIGNED_SHORT_5_5_5_1, texel5551_4[0], texel5551_4[1],
														texel5551_4[2], texel5551_4[3]),
									 getAlpha8OESPixelData(GL_UNSIGNED_BYTE, temp_5551_to_8888_br[3]),
									 PIXEL_COMPARE_CHANNEL_A);

		addEntryToConversionDatabase(
			getRGB5A1PixelData(1, GL_UNSIGNED_INT_2_10_10_10_REV, texel2101010_1[0], texel2101010_1[1],
							   texel2101010_1[2], texel2101010_1[3]),
			getAlpha8OESPixelData(GL_UNSIGNED_BYTE, temp_2101010rev_through_5551_to_8888_tl[3]),
			getRGB5A1PixelData(1, GL_UNSIGNED_INT_2_10_10_10_REV, texel2101010_2[0], texel2101010_2[1],
							   texel2101010_2[2], texel2101010_2[3]),
			getAlpha8OESPixelData(GL_UNSIGNED_BYTE, temp_2101010rev_through_5551_to_8888_tr[3]),
			getRGB5A1PixelData(1, GL_UNSIGNED_INT_2_10_10_10_REV, texel2101010_3[0], texel2101010_3[1],
							   texel2101010_3[2], texel2101010_3[3]),
			getAlpha8OESPixelData(GL_UNSIGNED_BYTE, temp_2101010rev_through_5551_to_8888_bl[3]),
			getRGB5A1PixelData(1, GL_UNSIGNED_INT_2_10_10_10_REV, texel2101010_4[0], texel2101010_4[1],
							   texel2101010_4[2], texel2101010_4[3]),
			getAlpha8OESPixelData(GL_UNSIGNED_BYTE, temp_2101010rev_through_5551_to_8888_br[3]),
			PIXEL_COMPARE_CHANNEL_A);
	}

	/* GL_RGBA8 */
	{
		const unsigned char texel1[4] = { 255, 127, 63, 0 };
		const unsigned char texel2[4] = { 127, 63, 0, 255 };
		const unsigned char texel3[4] = { 63, 0, 255, 127 };
		const unsigned char texel4[4] = { 0, 255, 127, 63 };

		/* GL_RGBA8 => GL_RGBA8 */
		addEntryToConversionDatabase(getRGBA8PixelData(GL_UNSIGNED_BYTE, texel1[0], texel1[1], texel1[2], texel1[3]),
									 getRGBA8PixelData(GL_UNSIGNED_BYTE, texel1[0], texel1[1], texel1[2], texel1[3]),
									 getRGBA8PixelData(GL_UNSIGNED_BYTE, texel2[0], texel2[1], texel2[2], texel2[3]),
									 getRGBA8PixelData(GL_UNSIGNED_BYTE, texel2[0], texel2[1], texel2[2], texel2[3]),
									 getRGBA8PixelData(GL_UNSIGNED_BYTE, texel3[0], texel3[1], texel3[2], texel3[3]),
									 getRGBA8PixelData(GL_UNSIGNED_BYTE, texel3[0], texel3[1], texel3[2], texel3[3]),
									 getRGBA8PixelData(GL_UNSIGNED_BYTE, texel4[0], texel4[1], texel4[2], texel4[3]),
									 getRGBA8PixelData(GL_UNSIGNED_BYTE, texel4[0], texel4[1], texel4[2], texel4[3]),
									 PIXEL_COMPARE_CHANNEL_RGBA);

		/* GL_RGBA8 => GL_RGB8 */
		addEntryToConversionDatabase(getRGBA8PixelData(GL_UNSIGNED_BYTE, texel1[0], texel1[1], texel1[2], texel1[3]),
									 getRGB8PixelData(0, GL_UNSIGNED_BYTE, texel1[0], texel1[1], texel1[2]),
									 getRGBA8PixelData(GL_UNSIGNED_BYTE, texel2[0], texel2[1], texel2[2], texel2[3]),
									 getRGB8PixelData(0, GL_UNSIGNED_BYTE, texel2[0], texel2[1], texel2[2]),
									 getRGBA8PixelData(GL_UNSIGNED_BYTE, texel3[0], texel3[1], texel3[2], texel3[3]),
									 getRGB8PixelData(0, GL_UNSIGNED_BYTE, texel3[0], texel3[1], texel3[2]),
									 getRGBA8PixelData(GL_UNSIGNED_BYTE, texel4[0], texel4[1], texel4[2], texel4[3]),
									 getRGB8PixelData(0, GL_UNSIGNED_BYTE, texel4[0], texel4[1], texel4[2]),
									 PIXEL_COMPARE_CHANNEL_RGB);

		/* GL_RGBA8 => GL_LUMINANCE8_ALPHA8_OES */
		addEntryToConversionDatabase(getRGBA8PixelData(GL_UNSIGNED_BYTE, texel1[0], texel1[1], texel1[2], texel1[3]),
									 getLuminance8Alpha8OESPixelData(GL_UNSIGNED_BYTE, texel1[0], texel1[3]),
									 getRGBA8PixelData(GL_UNSIGNED_BYTE, texel2[0], texel2[1], texel2[2], texel2[3]),
									 getLuminance8Alpha8OESPixelData(GL_UNSIGNED_BYTE, texel2[0], texel2[3]),
									 getRGBA8PixelData(GL_UNSIGNED_BYTE, texel3[0], texel3[1], texel3[2], texel3[3]),
									 getLuminance8Alpha8OESPixelData(GL_UNSIGNED_BYTE, texel3[0], texel3[3]),
									 getRGBA8PixelData(GL_UNSIGNED_BYTE, texel4[0], texel4[1], texel4[2], texel4[3]),
									 getLuminance8Alpha8OESPixelData(GL_UNSIGNED_BYTE, texel4[0], texel4[3]),
									 PIXEL_COMPARE_CHANNEL_RA);

		/* GL_RGBA8 => GL_LUMINANCE8_OES */
		addEntryToConversionDatabase(getRGBA8PixelData(GL_UNSIGNED_BYTE, texel1[0], texel1[1], texel1[2], texel1[3]),
									 getLuminance8OESPixelData(GL_UNSIGNED_BYTE, texel1[0]),
									 getRGBA8PixelData(GL_UNSIGNED_BYTE, texel2[0], texel2[1], texel2[2], texel2[3]),
									 getLuminance8OESPixelData(GL_UNSIGNED_BYTE, texel2[0]),
									 getRGBA8PixelData(GL_UNSIGNED_BYTE, texel3[0], texel3[1], texel3[2], texel3[3]),
									 getLuminance8OESPixelData(GL_UNSIGNED_BYTE, texel3[0]),
									 getRGBA8PixelData(GL_UNSIGNED_BYTE, texel4[0], texel4[1], texel4[2], texel4[3]),
									 getLuminance8OESPixelData(GL_UNSIGNED_BYTE, texel4[0]), PIXEL_COMPARE_CHANNEL_R);

		/* GL_RGBA8 => GL_ALPHA8_OES */
		addEntryToConversionDatabase(getRGBA8PixelData(GL_UNSIGNED_BYTE, texel1[0], texel1[1], texel1[2], texel1[3]),
									 getAlpha8OESPixelData(GL_UNSIGNED_BYTE, texel1[3]),
									 getRGBA8PixelData(GL_UNSIGNED_BYTE, texel2[0], texel2[1], texel2[2], texel2[3]),
									 getAlpha8OESPixelData(GL_UNSIGNED_BYTE, texel2[3]),
									 getRGBA8PixelData(GL_UNSIGNED_BYTE, texel3[0], texel3[1], texel3[2], texel3[3]),
									 getAlpha8OESPixelData(GL_UNSIGNED_BYTE, texel3[3]),
									 getRGBA8PixelData(GL_UNSIGNED_BYTE, texel4[0], texel4[1], texel4[2], texel4[3]),
									 getAlpha8OESPixelData(GL_UNSIGNED_BYTE, texel4[3]), PIXEL_COMPARE_CHANNEL_A);

		/* GL_RGBA8 => GL_R8 */
		addEntryToConversionDatabase(getRGBA8PixelData(GL_UNSIGNED_BYTE, texel1[0], texel1[1], texel1[2], texel1[3]),
									 getR8PixelData(0, GL_UNSIGNED_BYTE, texel1[0]),
									 getRGBA8PixelData(GL_UNSIGNED_BYTE, texel2[0], texel2[1], texel2[2], texel2[3]),
									 getR8PixelData(0, GL_UNSIGNED_BYTE, texel2[0]),
									 getRGBA8PixelData(GL_UNSIGNED_BYTE, texel3[0], texel3[1], texel3[2], texel3[3]),
									 getR8PixelData(0, GL_UNSIGNED_BYTE, texel3[0]),
									 getRGBA8PixelData(GL_UNSIGNED_BYTE, texel4[0], texel4[1], texel4[2], texel4[3]),
									 getR8PixelData(0, GL_UNSIGNED_BYTE, texel4[0]), PIXEL_COMPARE_CHANNEL_R);

		/* GL_RGBA8 => GL_RG8 */
		addEntryToConversionDatabase(getRGBA8PixelData(GL_UNSIGNED_BYTE, texel1[0], texel1[1], texel1[2], texel1[3]),
									 getRG8PixelData(0, GL_UNSIGNED_BYTE, texel1[0], texel1[1]),
									 getRGBA8PixelData(GL_UNSIGNED_BYTE, texel2[0], texel2[1], texel2[2], texel2[3]),
									 getRG8PixelData(0, GL_UNSIGNED_BYTE, texel2[0], texel2[1]),
									 getRGBA8PixelData(GL_UNSIGNED_BYTE, texel3[0], texel3[1], texel3[2], texel3[3]),
									 getRG8PixelData(0, GL_UNSIGNED_BYTE, texel3[0], texel3[1]),
									 getRGBA8PixelData(GL_UNSIGNED_BYTE, texel4[0], texel4[1], texel4[2], texel4[3]),
									 getRG8PixelData(0, GL_UNSIGNED_BYTE, texel4[0], texel4[1]),
									 PIXEL_COMPARE_CHANNEL_RG);
	}

	/* GL_RGB10_A2 */
	{
		const unsigned short texel1[4] = { 1023, 682, 341, 3 };
		const unsigned short texel2[4] = { 682, 341, 0, 2 };
		const unsigned short texel3[4] = { 341, 0, 1023, 1 };
		const unsigned short texel4[4] = { 0, 1023, 682, 0 };

		/* GL_RGB10_A2 => GL_RGB10_A2 */
		addEntryToConversionDatabase(getRGB10A2PixelData(GL_UNSIGNED_INT_2_10_10_10_REV, texel1[0], texel1[1],
														 texel1[2], (unsigned char)texel1[3]),
									 getRGB10A2PixelData(GL_UNSIGNED_INT_2_10_10_10_REV, texel1[0], texel1[1],
														 texel1[2], (unsigned char)texel1[3]),
									 getRGB10A2PixelData(GL_UNSIGNED_INT_2_10_10_10_REV, texel2[0], texel2[1],
														 texel2[2], (unsigned char)texel2[3]),
									 getRGB10A2PixelData(GL_UNSIGNED_INT_2_10_10_10_REV, texel2[0], texel2[1],
														 texel2[2], (unsigned char)texel2[3]),
									 getRGB10A2PixelData(GL_UNSIGNED_INT_2_10_10_10_REV, texel3[0], texel3[1],
														 texel3[2], (unsigned char)texel3[3]),
									 getRGB10A2PixelData(GL_UNSIGNED_INT_2_10_10_10_REV, texel3[0], texel3[1],
														 texel3[2], (unsigned char)texel3[3]),
									 getRGB10A2PixelData(GL_UNSIGNED_INT_2_10_10_10_REV, texel4[0], texel4[1],
														 texel4[2], (unsigned char)texel4[3]),
									 getRGB10A2PixelData(GL_UNSIGNED_INT_2_10_10_10_REV, texel4[0], texel4[1],
														 texel4[2], (unsigned char)texel4[3]),
									 PIXEL_COMPARE_CHANNEL_RGBA);
	}

	/* GL_RGB10_A2UI */
	{
		const unsigned short texel1[4] = { 1023, 682, 341, 3 };
		const unsigned short texel2[4] = { 682, 341, 0, 2 };
		const unsigned short texel3[4] = { 341, 0, 1023, 1 };
		const unsigned short texel4[4] = { 0, 1023, 682, 0 };

		/* GL_RGB10_A2UI => GL_RGB10_A2UI */
		addEntryToConversionDatabase(
			getRGB10A2UIPixelData(1, GL_UNSIGNED_INT_2_10_10_10_REV, texel1[0], texel1[1], texel1[2], texel1[3]),
			getRGB10A2UIPixelData(0, GL_UNSIGNED_INT, texel1[0], texel1[1], texel1[2], texel1[3]),
			getRGB10A2UIPixelData(1, GL_UNSIGNED_INT_2_10_10_10_REV, texel2[0], texel2[1], texel2[2], texel2[3]),
			getRGB10A2UIPixelData(0, GL_UNSIGNED_INT, texel2[0], texel2[1], texel2[2], texel2[3]),
			getRGB10A2UIPixelData(1, GL_UNSIGNED_INT_2_10_10_10_REV, texel3[0], texel3[1], texel3[2], texel3[3]),
			getRGB10A2UIPixelData(0, GL_UNSIGNED_INT, texel3[0], texel3[1], texel3[2], texel3[3]),
			getRGB10A2UIPixelData(1, GL_UNSIGNED_INT_2_10_10_10_REV, texel4[0], texel4[1], texel4[2], texel4[3]),
			getRGB10A2UIPixelData(0, GL_UNSIGNED_INT, texel4[0], texel4[1], texel4[2], texel4[3]),
			PIXEL_COMPARE_CHANNEL_RGBA);
	}

	/* GL_SRGB8_ALPHA8 */
	{
		const unsigned char texel1[4] = { 255, 127, 63, 0 };
		const unsigned char texel2[4] = { 127, 63, 0, 255 };
		const unsigned char texel3[4] = { 63, 0, 255, 127 };
		const unsigned char texel4[4] = { 0, 255, 127, 63 };

		/* GL_SRGB8_ALPHA8 => GL_SRGB8 */
		addEntryToConversionDatabase(
			getSRGB8Alpha8PixelData(GL_UNSIGNED_BYTE, texel1[0], texel1[1], texel1[2], texel1[3]),
			getSRGB8PixelData(0, GL_UNSIGNED_BYTE, texel1[0], texel1[1], texel1[2]),
			getSRGB8Alpha8PixelData(GL_UNSIGNED_BYTE, texel2[0], texel2[1], texel2[2], texel2[3]),
			getSRGB8PixelData(0, GL_UNSIGNED_BYTE, texel2[0], texel2[1], texel2[2]),
			getSRGB8Alpha8PixelData(GL_UNSIGNED_BYTE, texel3[0], texel3[1], texel3[2], texel3[3]),
			getSRGB8PixelData(0, GL_UNSIGNED_BYTE, texel3[0], texel3[1], texel3[2]),
			getSRGB8Alpha8PixelData(GL_UNSIGNED_BYTE, texel4[0], texel4[1], texel4[2], texel4[3]),
			getSRGB8PixelData(0, GL_UNSIGNED_BYTE, texel4[0], texel4[1], texel4[2]), PIXEL_COMPARE_CHANNEL_RGB);

		/* GL_SRGB8_ALPHA8 => GL_SRGB8_ALPHA8 */
		addEntryToConversionDatabase(
			getSRGB8Alpha8PixelData(GL_UNSIGNED_BYTE, texel1[0], texel1[1], texel1[2], texel1[3]),
			getSRGB8Alpha8PixelData(GL_UNSIGNED_BYTE, texel1[0], texel1[1], texel1[2], texel1[3]),
			getSRGB8Alpha8PixelData(GL_UNSIGNED_BYTE, texel2[0], texel2[1], texel2[2], texel2[3]),
			getSRGB8Alpha8PixelData(GL_UNSIGNED_BYTE, texel2[0], texel2[1], texel2[2], texel2[3]),
			getSRGB8Alpha8PixelData(GL_UNSIGNED_BYTE, texel3[0], texel3[1], texel3[2], texel3[3]),
			getSRGB8Alpha8PixelData(GL_UNSIGNED_BYTE, texel3[0], texel3[1], texel3[2], texel3[3]),
			getSRGB8Alpha8PixelData(GL_UNSIGNED_BYTE, texel4[0], texel4[1], texel4[2], texel4[3]),
			getSRGB8Alpha8PixelData(GL_UNSIGNED_BYTE, texel4[0], texel4[1], texel4[2], texel4[3]),
			PIXEL_COMPARE_CHANNEL_RGBA);
	}

	/* GL_R8I */
	{
		const signed char texel1[1] = { 127 };
		const signed char texel2[1] = { 42 };
		const signed char texel3[1] = { -43 };
		const signed char texel4[1] = { -127 };

		/* GL_R8I => GL_R8I */
		addEntryToConversionDatabase(getR8IPixelData(1, GL_BYTE, texel1[0]), getR8IPixelData(0, GL_INT, texel1[0]),
									 getR8IPixelData(1, GL_BYTE, texel2[0]), getR8IPixelData(0, GL_INT, texel2[0]),
									 getR8IPixelData(1, GL_BYTE, texel3[0]), getR8IPixelData(0, GL_INT, texel3[0]),
									 getR8IPixelData(1, GL_BYTE, texel4[0]), getR8IPixelData(0, GL_INT, texel4[0]),
									 PIXEL_COMPARE_CHANNEL_R);
	}

	/* GL_R8UI */
	{
		const unsigned char texel1[1] = { 255 };
		const unsigned char texel2[1] = { 127 };
		const unsigned char texel3[1] = { 63 };
		const unsigned char texel4[1] = { 0 };

		/* GL_R8UI => GL_R8UI */
		addEntryToConversionDatabase(
			getR8UIPixelData(1, GL_UNSIGNED_BYTE, texel1[0]), getR8UIPixelData(0, GL_UNSIGNED_INT, texel1[0]),
			getR8UIPixelData(1, GL_UNSIGNED_BYTE, texel2[0]), getR8UIPixelData(0, GL_UNSIGNED_INT, texel2[0]),
			getR8UIPixelData(1, GL_UNSIGNED_BYTE, texel3[0]), getR8UIPixelData(0, GL_UNSIGNED_INT, texel3[0]),
			getR8UIPixelData(1, GL_UNSIGNED_BYTE, texel4[0]), getR8UIPixelData(0, GL_UNSIGNED_INT, texel4[0]),
			PIXEL_COMPARE_CHANNEL_R);
	}

	/* GL_R16I */
	{
		const signed short texel1[1] = { 32767 };
		const signed short texel2[1] = { 10922 };
		const signed short texel3[1] = { -10923 };
		const signed short texel4[1] = { -32767 };

		/* GL_R16I => GL_R16I */
		addEntryToConversionDatabase(getR16IPixelData(1, GL_SHORT, texel1[0]), getR16IPixelData(0, GL_INT, texel1[0]),
									 getR16IPixelData(1, GL_SHORT, texel2[0]), getR16IPixelData(0, GL_INT, texel2[0]),
									 getR16IPixelData(1, GL_SHORT, texel3[0]), getR16IPixelData(0, GL_INT, texel3[0]),
									 getR16IPixelData(1, GL_SHORT, texel4[0]), getR16IPixelData(0, GL_INT, texel4[0]),
									 PIXEL_COMPARE_CHANNEL_R);
	}

	/* GL_R16UI */
	{
		const unsigned short texel1[1] = { 65535 };
		const unsigned short texel2[1] = { 43690 };
		const unsigned short texel3[1] = { 21845 };
		const unsigned short texel4[1] = { 0 };

		/* GL_R16UI => GL_R16UI */
		addEntryToConversionDatabase(
			getR16UIPixelData(1, GL_UNSIGNED_SHORT, texel1[0]), getR16UIPixelData(0, GL_UNSIGNED_INT, texel1[0]),
			getR16UIPixelData(1, GL_UNSIGNED_SHORT, texel2[0]), getR16UIPixelData(0, GL_UNSIGNED_INT, texel2[0]),
			getR16UIPixelData(1, GL_UNSIGNED_SHORT, texel3[0]), getR16UIPixelData(0, GL_UNSIGNED_INT, texel3[0]),
			getR16UIPixelData(1, GL_UNSIGNED_SHORT, texel4[0]), getR16UIPixelData(0, GL_UNSIGNED_INT, texel4[0]),
			PIXEL_COMPARE_CHANNEL_R);
	}

	/* GL_R32I */
	{
		const int texel1[1] = { 2147483647l };
		const int texel2[1] = { 715827883l };
		const int texel3[1] = { -715827881l };
		const int texel4[1] = { -2147483647l };

		/* GL_R32I => GL_R32I */
		addEntryToConversionDatabase(getR32IPixelData(1, GL_INT, texel1[0]), getR32IPixelData(0, GL_INT, texel1[0]),
									 getR32IPixelData(1, GL_INT, texel2[0]), getR32IPixelData(0, GL_INT, texel2[0]),
									 getR32IPixelData(1, GL_INT, texel3[0]), getR32IPixelData(0, GL_INT, texel3[0]),
									 getR32IPixelData(1, GL_INT, texel4[0]), getR32IPixelData(0, GL_INT, texel4[0]),
									 PIXEL_COMPARE_CHANNEL_R);
	}

	/* GL_R32UI */
	{
		const unsigned int texel1[1] = { 4294967295u };
		const unsigned int texel2[1] = { 2863311530u };
		const unsigned int texel3[1] = { 1431655765u };
		const unsigned int texel4[1] = { 0 };

		/* GL_R32UI => GL_R32UI */
		addEntryToConversionDatabase(
			getR32UIPixelData(1, GL_UNSIGNED_INT, texel1[0]), getR32UIPixelData(0, GL_UNSIGNED_INT, texel1[0]),
			getR32UIPixelData(1, GL_UNSIGNED_INT, texel2[0]), getR32UIPixelData(0, GL_UNSIGNED_INT, texel2[0]),
			getR32UIPixelData(1, GL_UNSIGNED_INT, texel3[0]), getR32UIPixelData(0, GL_UNSIGNED_INT, texel3[0]),
			getR32UIPixelData(1, GL_UNSIGNED_INT, texel4[0]), getR32UIPixelData(0, GL_UNSIGNED_INT, texel4[0]),
			PIXEL_COMPARE_CHANNEL_R);
	}

	/* GL_RG8I */
	{
		const signed char texel1[2] = { 127, 42 };
		const signed char texel2[2] = { 42, -43 };
		const signed char texel3[2] = { -43, -127 };
		const signed char texel4[2] = { -127, 127 };

		/* GL_RG8I => GL_R8I */
		addEntryToConversionDatabase(
			getRG8IPixelData(1, GL_BYTE, texel1[0], texel1[1]), getR8IPixelData(0, GL_INT, texel1[0]),
			getRG8IPixelData(1, GL_BYTE, texel2[0], texel2[1]), getR8IPixelData(0, GL_INT, texel2[0]),
			getRG8IPixelData(1, GL_BYTE, texel3[0], texel3[1]), getR8IPixelData(0, GL_INT, texel3[0]),
			getRG8IPixelData(1, GL_BYTE, texel4[0], texel4[1]), getR8IPixelData(0, GL_INT, texel4[0]),
			PIXEL_COMPARE_CHANNEL_R);
		/* GL_RG8I => GL_RG8I */
		addEntryToConversionDatabase(
			getRG8IPixelData(1, GL_BYTE, texel1[0], texel1[1]), getRG8IPixelData(0, GL_INT, texel1[0], texel1[1]),
			getRG8IPixelData(1, GL_BYTE, texel2[0], texel2[1]), getRG8IPixelData(0, GL_INT, texel2[0], texel2[1]),
			getRG8IPixelData(1, GL_BYTE, texel3[0], texel3[1]), getRG8IPixelData(0, GL_INT, texel3[0], texel3[1]),
			getRG8IPixelData(1, GL_BYTE, texel4[0], texel4[1]), getRG8IPixelData(0, GL_INT, texel4[0], texel4[1]),
			PIXEL_COMPARE_CHANNEL_RG);
	}

	/* GL_RG8UI */
	{
		const unsigned char texel1[2] = { 255, 127 };
		const unsigned char texel2[2] = { 127, 63 };
		const unsigned char texel3[2] = { 63, 0 };
		const unsigned char texel4[2] = { 0, 255 };

		/* GL_RG8UI => GL_R8UI */
		addEntryToConversionDatabase(getRG8UIPixelData(1, GL_UNSIGNED_BYTE, texel1[0], texel1[1]),
									 getR8UIPixelData(0, GL_UNSIGNED_INT, texel1[0]),
									 getRG8UIPixelData(1, GL_UNSIGNED_BYTE, texel2[0], texel2[1]),
									 getR8UIPixelData(0, GL_UNSIGNED_INT, texel2[0]),
									 getRG8UIPixelData(1, GL_UNSIGNED_BYTE, texel3[0], texel3[1]),
									 getR8UIPixelData(0, GL_UNSIGNED_INT, texel3[0]),
									 getRG8UIPixelData(1, GL_UNSIGNED_BYTE, texel4[0], texel4[1]),
									 getR8UIPixelData(0, GL_UNSIGNED_INT, texel4[0]), PIXEL_COMPARE_CHANNEL_R);

		/* GL_RG8UI => GL_RG8UI */
		addEntryToConversionDatabase(getRG8UIPixelData(1, GL_UNSIGNED_BYTE, texel1[0], texel1[1]),
									 getRG8UIPixelData(0, GL_UNSIGNED_INT, texel1[0], texel1[1]),
									 getRG8UIPixelData(1, GL_UNSIGNED_BYTE, texel2[0], texel2[1]),
									 getRG8UIPixelData(0, GL_UNSIGNED_INT, texel2[0], texel2[1]),
									 getRG8UIPixelData(1, GL_UNSIGNED_BYTE, texel3[0], texel3[1]),
									 getRG8UIPixelData(0, GL_UNSIGNED_INT, texel3[0], texel3[1]),
									 getRG8UIPixelData(1, GL_UNSIGNED_BYTE, texel4[0], texel4[1]),
									 getRG8UIPixelData(0, GL_UNSIGNED_INT, texel4[0], texel4[1]),
									 PIXEL_COMPARE_CHANNEL_RG);
	}

	/* GL_RG16I */
	{
		const short texel1[2] = { 32767, 10922 };
		const short texel2[2] = { 10922, -10923 };
		const short texel3[2] = { -10923, -32767 };
		const short texel4[2] = { -32767, 32767 };

		/* GL_RG16I => GL_R16I */
		addEntryToConversionDatabase(
			getRG16IPixelData(1, GL_SHORT, texel1[0], texel1[1]), getR16IPixelData(0, GL_INT, texel1[0]),
			getRG16IPixelData(1, GL_SHORT, texel2[0], texel2[1]), getR16IPixelData(0, GL_INT, texel2[0]),
			getRG16IPixelData(1, GL_SHORT, texel3[0], texel3[1]), getR16IPixelData(0, GL_INT, texel3[0]),
			getRG16IPixelData(1, GL_SHORT, texel4[0], texel4[1]), getR16IPixelData(0, GL_INT, texel4[0]),
			PIXEL_COMPARE_CHANNEL_R);

		/* GL_RG16I => GL_RG16I */
		addEntryToConversionDatabase(
			getRG16IPixelData(1, GL_SHORT, texel1[0], texel1[1]), getRG16IPixelData(0, GL_INT, texel1[0], texel1[1]),
			getRG16IPixelData(1, GL_SHORT, texel2[0], texel2[1]), getRG16IPixelData(0, GL_INT, texel2[0], texel2[1]),
			getRG16IPixelData(1, GL_SHORT, texel3[0], texel3[1]), getRG16IPixelData(0, GL_INT, texel3[0], texel3[1]),
			getRG16IPixelData(1, GL_SHORT, texel4[0], texel4[1]), getRG16IPixelData(0, GL_INT, texel4[0], texel4[1]),
			PIXEL_COMPARE_CHANNEL_RG);
	}

	/* GL_RG16UI */
	{
		const unsigned short texel1[2] = { 65535, 43690 };
		const unsigned short texel2[2] = { 43690, 21845 };
		const unsigned short texel3[2] = { 21845, 0 };
		const unsigned short texel4[2] = { 0, 65535 };

		/* GL_RG16UI => GL_R16UI */
		addEntryToConversionDatabase(getRG16UIPixelData(1, GL_UNSIGNED_SHORT, texel1[0], texel1[1]),
									 getR16UIPixelData(0, GL_UNSIGNED_INT, texel1[0]),
									 getRG16UIPixelData(1, GL_UNSIGNED_SHORT, texel2[0], texel2[1]),
									 getR16UIPixelData(0, GL_UNSIGNED_INT, texel2[0]),
									 getRG16UIPixelData(1, GL_UNSIGNED_SHORT, texel3[0], texel3[1]),
									 getR16UIPixelData(0, GL_UNSIGNED_INT, texel3[0]),
									 getRG16UIPixelData(1, GL_UNSIGNED_SHORT, texel4[0], texel4[1]),
									 getR16UIPixelData(0, GL_UNSIGNED_INT, texel4[0]), PIXEL_COMPARE_CHANNEL_R);

		/* GL_RG16UI => GL_RG16UI */
		addEntryToConversionDatabase(getRG16UIPixelData(1, GL_UNSIGNED_SHORT, texel1[0], texel1[1]),
									 getRG16UIPixelData(0, GL_UNSIGNED_INT, texel1[0], texel1[1]),
									 getRG16UIPixelData(1, GL_UNSIGNED_SHORT, texel2[0], texel2[1]),
									 getRG16UIPixelData(0, GL_UNSIGNED_INT, texel2[0], texel2[1]),
									 getRG16UIPixelData(1, GL_UNSIGNED_SHORT, texel3[0], texel3[1]),
									 getRG16UIPixelData(0, GL_UNSIGNED_INT, texel3[0], texel3[1]),
									 getRG16UIPixelData(1, GL_UNSIGNED_SHORT, texel4[0], texel4[1]),
									 getRG16UIPixelData(0, GL_UNSIGNED_INT, texel4[0], texel4[1]),
									 PIXEL_COMPARE_CHANNEL_RG);
	}

	/* GL_RG32I */
	{
		const int texel1[2] = { 2147483647, 715827883l };
		const int texel2[2] = { 715827883, -715827881l };
		const int texel3[2] = { -715827881, -2147483647l };
		const int texel4[2] = { -2147483647, 2147483647l };

		/* GL_RG32I => GL_R32I */
		addEntryToConversionDatabase(
			getRG32IPixelData(1, GL_INT, texel1[0], texel1[1]), getR32IPixelData(0, GL_INT, texel1[0]),
			getRG32IPixelData(1, GL_INT, texel2[0], texel2[1]), getR32IPixelData(0, GL_INT, texel2[0]),
			getRG32IPixelData(1, GL_INT, texel3[0], texel3[1]), getR32IPixelData(0, GL_INT, texel3[0]),
			getRG32IPixelData(1, GL_INT, texel4[0], texel4[1]), getR32IPixelData(0, GL_INT, texel4[0]),
			PIXEL_COMPARE_CHANNEL_R);

		/* GL_RG32I => GL_RG32I */
		addEntryToConversionDatabase(
			getRG32IPixelData(1, GL_INT, texel1[0], texel1[1]), getRG32IPixelData(0, GL_INT, texel1[0], texel1[1]),
			getRG32IPixelData(1, GL_INT, texel2[0], texel2[1]), getRG32IPixelData(0, GL_INT, texel2[0], texel2[1]),
			getRG32IPixelData(1, GL_INT, texel3[0], texel3[1]), getRG32IPixelData(0, GL_INT, texel3[0], texel3[1]),
			getRG32IPixelData(1, GL_INT, texel4[0], texel4[1]), getRG32IPixelData(0, GL_INT, texel4[0], texel4[1]),
			PIXEL_COMPARE_CHANNEL_RG);
	}

	/* GL_RG32UI */
	{
		const unsigned int texel1[2] = { 4294967295u, 2863311530u };
		const unsigned int texel2[2] = { 2863311530u, 1431655765u };
		const unsigned int texel3[2] = { 1431655765u, 0 };
		const unsigned int texel4[2] = { 0, 4294967295u };

		/* GL_RG32UI => GL_R32UI */
		addEntryToConversionDatabase(getRG32UIPixelData(1, GL_UNSIGNED_INT, texel1[0], texel1[1]),
									 getR32UIPixelData(0, GL_UNSIGNED_INT, texel1[0]),
									 getRG32UIPixelData(1, GL_UNSIGNED_INT, texel2[0], texel2[1]),
									 getR32UIPixelData(0, GL_UNSIGNED_INT, texel2[0]),
									 getRG32UIPixelData(1, GL_UNSIGNED_INT, texel3[0], texel3[1]),
									 getR32UIPixelData(0, GL_UNSIGNED_INT, texel3[0]),
									 getRG32UIPixelData(1, GL_UNSIGNED_INT, texel4[0], texel4[1]),
									 getR32UIPixelData(0, GL_UNSIGNED_INT, texel4[0]), PIXEL_COMPARE_CHANNEL_R);

		/* GL_RG32UI => GL_RG32UI */
		addEntryToConversionDatabase(getRG32UIPixelData(1, GL_UNSIGNED_INT, texel1[0], texel1[1]),
									 getRG32UIPixelData(0, GL_UNSIGNED_INT, texel1[0], texel1[1]),
									 getRG32UIPixelData(1, GL_UNSIGNED_INT, texel2[0], texel2[1]),
									 getRG32UIPixelData(0, GL_UNSIGNED_INT, texel2[0], texel2[1]),
									 getRG32UIPixelData(1, GL_UNSIGNED_INT, texel3[0], texel3[1]),
									 getRG32UIPixelData(0, GL_UNSIGNED_INT, texel3[0], texel3[1]),
									 getRG32UIPixelData(1, GL_UNSIGNED_INT, texel4[0], texel4[1]),
									 getRG32UIPixelData(0, GL_UNSIGNED_INT, texel4[0], texel4[1]),
									 PIXEL_COMPARE_CHANNEL_RG);
	}

	/* GL_RGBA8I */
	{
		const signed char texel1[4] = { 127, 42, -43, -127 };
		const signed char texel2[4] = { 42, -43, -127, 127 };
		const signed char texel3[4] = { -43, -127, 127, 42 };
		const signed char texel4[4] = { -127, 127, 42, -43 };

		/* GL_RGBA8I => GL_R8I */
		addEntryToConversionDatabase(getRGBA8IPixelData(1, GL_BYTE, texel1[0], texel1[1], texel1[2], texel1[3]),
									 getR8IPixelData(0, GL_INT, texel1[0]),
									 getRGBA8IPixelData(1, GL_BYTE, texel2[0], texel2[1], texel2[2], texel2[3]),
									 getR8IPixelData(0, GL_INT, texel2[0]),
									 getRGBA8IPixelData(1, GL_BYTE, texel3[0], texel3[1], texel3[2], texel3[3]),
									 getR8IPixelData(0, GL_INT, texel3[0]),
									 getRGBA8IPixelData(1, GL_BYTE, texel4[0], texel4[1], texel4[2], texel4[3]),
									 getR8IPixelData(0, GL_INT, texel4[0]), PIXEL_COMPARE_CHANNEL_R);

		/* GL_RGBA8I => GL_RG8I */
		addEntryToConversionDatabase(getRGBA8IPixelData(1, GL_BYTE, texel1[0], texel1[1], texel1[2], texel1[3]),
									 getRG8IPixelData(0, GL_INT, texel1[0], texel1[1]),
									 getRGBA8IPixelData(1, GL_BYTE, texel2[0], texel2[1], texel2[2], texel2[3]),
									 getRG8IPixelData(0, GL_INT, texel2[0], texel2[1]),
									 getRGBA8IPixelData(1, GL_BYTE, texel3[0], texel3[1], texel3[2], texel3[3]),
									 getRG8IPixelData(0, GL_INT, texel3[0], texel3[1]),
									 getRGBA8IPixelData(1, GL_BYTE, texel4[0], texel4[1], texel4[2], texel4[3]),
									 getRG8IPixelData(0, GL_INT, texel4[0], texel4[1]), PIXEL_COMPARE_CHANNEL_RG);

		/* GL_RGBA8I => GL_RGB8I */
		addEntryToConversionDatabase(getRGBA8IPixelData(1, GL_BYTE, texel1[0], texel1[1], texel1[2], texel1[3]),
									 getRGB8IPixelData(0, GL_INT, texel1[0], texel1[1], texel1[2]),
									 getRGBA8IPixelData(1, GL_BYTE, texel2[0], texel2[1], texel2[2], texel2[3]),
									 getRGB8IPixelData(0, GL_INT, texel2[0], texel2[1], texel2[2]),
									 getRGBA8IPixelData(1, GL_BYTE, texel3[0], texel3[1], texel3[2], texel3[3]),
									 getRGB8IPixelData(0, GL_INT, texel3[0], texel3[1], texel3[2]),
									 getRGBA8IPixelData(1, GL_BYTE, texel4[0], texel4[1], texel4[2], texel4[3]),
									 getRGB8IPixelData(0, GL_INT, texel4[0], texel4[1], texel4[2]),
									 PIXEL_COMPARE_CHANNEL_RGB);

		/* GL_RGBA8I => GL_RGBA8I */
		addEntryToConversionDatabase(getRGBA8IPixelData(1, GL_BYTE, texel1[0], texel1[1], texel1[2], texel1[3]),
									 getRGBA8IPixelData(0, GL_INT, texel1[0], texel1[1], texel1[2], texel1[3]),
									 getRGBA8IPixelData(1, GL_BYTE, texel2[0], texel2[1], texel2[2], texel2[3]),
									 getRGBA8IPixelData(0, GL_INT, texel2[0], texel2[1], texel2[2], texel2[3]),
									 getRGBA8IPixelData(1, GL_BYTE, texel3[0], texel3[1], texel3[2], texel3[3]),
									 getRGBA8IPixelData(0, GL_INT, texel3[0], texel3[1], texel3[2], texel3[3]),
									 getRGBA8IPixelData(1, GL_BYTE, texel4[0], texel4[1], texel4[2], texel4[3]),
									 getRGBA8IPixelData(0, GL_INT, texel4[0], texel4[1], texel4[2], texel4[3]),
									 PIXEL_COMPARE_CHANNEL_RGBA);
	}

	/* GL_RGBA8UI */
	{
		const unsigned char texel1[4] = { 255, 127, 63, 0 };
		const unsigned char texel2[4] = { 127, 63, 0, 255 };
		const unsigned char texel3[4] = { 63, 0, 255, 127 };
		const unsigned char texel4[4] = { 0, 255, 127, 63 };

		/* GL_RGBA8UI => GL_R8UI */
		addEntryToConversionDatabase(
			getRGBA8UIPixelData(1, GL_UNSIGNED_BYTE, texel1[0], texel1[1], texel1[2], texel1[3]),
			getR8UIPixelData(0, GL_UNSIGNED_INT, texel1[0]),
			getRGBA8UIPixelData(1, GL_UNSIGNED_BYTE, texel2[0], texel2[1], texel2[2], texel2[3]),
			getR8UIPixelData(0, GL_UNSIGNED_INT, texel2[0]),
			getRGBA8UIPixelData(1, GL_UNSIGNED_BYTE, texel3[0], texel3[1], texel3[2], texel3[3]),
			getR8UIPixelData(0, GL_UNSIGNED_INT, texel3[0]),
			getRGBA8UIPixelData(1, GL_UNSIGNED_BYTE, texel4[0], texel4[1], texel4[2], texel4[3]),
			getR8UIPixelData(0, GL_UNSIGNED_INT, texel4[0]), PIXEL_COMPARE_CHANNEL_R);

		/* GL_RGBA8UI => GL_RG8UI */
		addEntryToConversionDatabase(
			getRGBA8UIPixelData(1, GL_UNSIGNED_BYTE, texel1[0], texel1[1], texel1[2], texel1[3]),
			getRG8UIPixelData(0, GL_UNSIGNED_INT, texel1[0], texel1[1]),
			getRGBA8UIPixelData(1, GL_UNSIGNED_BYTE, texel2[0], texel2[1], texel2[2], texel2[3]),
			getRG8UIPixelData(0, GL_UNSIGNED_INT, texel2[0], texel2[1]),
			getRGBA8UIPixelData(1, GL_UNSIGNED_BYTE, texel3[0], texel3[1], texel3[2], texel3[3]),
			getRG8UIPixelData(0, GL_UNSIGNED_INT, texel3[0], texel3[1]),
			getRGBA8UIPixelData(1, GL_UNSIGNED_BYTE, texel4[0], texel4[1], texel4[2], texel4[3]),
			getRG8UIPixelData(0, GL_UNSIGNED_INT, texel4[0], texel4[1]), PIXEL_COMPARE_CHANNEL_RG);

		/* GL_RGBA8UI => GL_RGB8UI */
		addEntryToConversionDatabase(
			getRGBA8UIPixelData(1, GL_UNSIGNED_BYTE, texel1[0], texel1[1], texel1[2], texel1[3]),
			getRGB8UIPixelData(0, GL_UNSIGNED_INT, texel1[0], texel1[1], texel1[2]),
			getRGBA8UIPixelData(1, GL_UNSIGNED_BYTE, texel2[0], texel2[1], texel2[2], texel2[3]),
			getRGB8UIPixelData(0, GL_UNSIGNED_INT, texel2[0], texel2[1], texel2[2]),
			getRGBA8UIPixelData(1, GL_UNSIGNED_BYTE, texel3[0], texel3[1], texel3[2], texel3[3]),
			getRGB8UIPixelData(0, GL_UNSIGNED_INT, texel3[0], texel3[1], texel3[2]),
			getRGBA8UIPixelData(1, GL_UNSIGNED_BYTE, texel4[0], texel4[1], texel4[2], texel4[3]),
			getRGB8UIPixelData(0, GL_UNSIGNED_INT, texel4[0], texel4[1], texel4[2]), PIXEL_COMPARE_CHANNEL_RGB);

		/* GL_RGBA8UI => GL_RGBA8UI */
		addEntryToConversionDatabase(
			getRGBA8UIPixelData(1, GL_UNSIGNED_BYTE, texel1[0], texel1[1], texel1[2], texel1[3]),
			getRGBA8UIPixelData(0, GL_UNSIGNED_INT, texel1[0], texel1[1], texel1[2], texel1[3]),
			getRGBA8UIPixelData(1, GL_UNSIGNED_BYTE, texel2[0], texel2[1], texel2[2], texel2[3]),
			getRGBA8UIPixelData(0, GL_UNSIGNED_INT, texel2[0], texel2[1], texel2[2], texel2[3]),
			getRGBA8UIPixelData(1, GL_UNSIGNED_BYTE, texel3[0], texel3[1], texel3[2], texel3[3]),
			getRGBA8UIPixelData(0, GL_UNSIGNED_INT, texel3[0], texel3[1], texel3[2], texel3[3]),
			getRGBA8UIPixelData(1, GL_UNSIGNED_BYTE, texel4[0], texel4[1], texel4[2], texel4[3]),
			getRGBA8UIPixelData(0, GL_UNSIGNED_INT, texel4[0], texel4[1], texel4[2], texel4[3]),
			PIXEL_COMPARE_CHANNEL_RGBA);
	}

	/* GL_RGBA16I */
	{
		const short texel1[4] = { 32767, 10922, -10923, -32767 };
		const short texel2[4] = { 10922, -10923, -32767, 32767 };
		const short texel3[4] = { -10923, -32767, 32767, 10922 };
		const short texel4[4] = { -32767, 32767, 10922, -10923 };

		/* GL_RGBA16I => GL_R16I */
		addEntryToConversionDatabase(getRGBA16IPixelData(1, GL_SHORT, texel1[0], texel1[1], texel1[2], texel1[3]),
									 getR16IPixelData(0, GL_INT, texel1[0]),
									 getRGBA16IPixelData(1, GL_SHORT, texel2[0], texel2[1], texel2[2], texel2[3]),
									 getR16IPixelData(0, GL_INT, texel2[0]),
									 getRGBA16IPixelData(1, GL_SHORT, texel3[0], texel3[1], texel3[2], texel3[3]),
									 getR16IPixelData(0, GL_INT, texel3[0]),
									 getRGBA16IPixelData(1, GL_SHORT, texel4[0], texel4[1], texel4[2], texel4[3]),
									 getR16IPixelData(0, GL_INT, texel4[0]), PIXEL_COMPARE_CHANNEL_R);

		/* GL_RGBA16I => GL_RG16I */
		addEntryToConversionDatabase(getRGBA16IPixelData(1, GL_SHORT, texel1[0], texel1[1], texel1[2], texel1[3]),
									 getRG16IPixelData(0, GL_INT, texel1[0], texel1[1]),
									 getRGBA16IPixelData(1, GL_SHORT, texel2[0], texel2[1], texel2[2], texel2[3]),
									 getRG16IPixelData(0, GL_INT, texel2[0], texel2[1]),
									 getRGBA16IPixelData(1, GL_SHORT, texel3[0], texel3[1], texel3[2], texel3[3]),
									 getRG16IPixelData(0, GL_INT, texel3[0], texel3[1]),
									 getRGBA16IPixelData(1, GL_SHORT, texel4[0], texel4[1], texel4[2], texel4[3]),
									 getRG16IPixelData(0, GL_INT, texel4[0], texel4[1]), PIXEL_COMPARE_CHANNEL_RG);

		/* GL_RGBA16I => GL_RGB16I */
		addEntryToConversionDatabase(getRGBA16IPixelData(1, GL_SHORT, texel1[0], texel1[1], texel1[2], texel1[3]),
									 getRGB16IPixelData(0, GL_INT, texel1[0], texel1[1], texel1[2]),
									 getRGBA16IPixelData(1, GL_SHORT, texel2[0], texel2[1], texel2[2], texel2[3]),
									 getRGB16IPixelData(0, GL_INT, texel2[0], texel2[1], texel2[2]),
									 getRGBA16IPixelData(1, GL_SHORT, texel3[0], texel3[1], texel3[2], texel3[3]),
									 getRGB16IPixelData(0, GL_INT, texel3[0], texel3[1], texel3[2]),
									 getRGBA16IPixelData(1, GL_SHORT, texel4[0], texel4[1], texel4[2], texel4[3]),
									 getRGB16IPixelData(0, GL_INT, texel4[0], texel4[1], texel4[2]),
									 PIXEL_COMPARE_CHANNEL_RGB);

		/* GL_RGBA16I => GL_RGBA16I */
		addEntryToConversionDatabase(getRGBA16IPixelData(1, GL_SHORT, texel1[0], texel1[1], texel1[2], texel1[3]),
									 getRGBA16IPixelData(0, GL_INT, texel1[0], texel1[1], texel1[2], texel1[3]),
									 getRGBA16IPixelData(1, GL_SHORT, texel2[0], texel2[1], texel2[2], texel2[3]),
									 getRGBA16IPixelData(0, GL_INT, texel2[0], texel2[1], texel2[2], texel2[3]),
									 getRGBA16IPixelData(1, GL_SHORT, texel3[0], texel3[1], texel3[2], texel3[3]),
									 getRGBA16IPixelData(0, GL_INT, texel3[0], texel3[1], texel3[2], texel3[3]),
									 getRGBA16IPixelData(1, GL_SHORT, texel4[0], texel4[1], texel4[2], texel4[3]),
									 getRGBA16IPixelData(0, GL_INT, texel4[0], texel4[1], texel4[2], texel4[3]),
									 PIXEL_COMPARE_CHANNEL_RGBA);
	}

	/* GL_RGBA16UI */
	{
		const unsigned short texel1[4] = { 65535, 43690, 21845, 0 };
		const unsigned short texel2[4] = { 43690, 21845, 0, 65535 };
		const unsigned short texel3[4] = { 21845, 0, 65535, 43690 };
		const unsigned short texel4[4] = { 0, 65535, 43690, 21845 };

		/* GL_RGBA16UI => GL_R16UI */
		addEntryToConversionDatabase(
			getRGBA16UIPixelData(1, GL_UNSIGNED_SHORT, texel1[0], texel1[1], texel1[2], texel1[3]),
			getR16UIPixelData(0, GL_UNSIGNED_INT, texel1[0]),
			getRGBA16UIPixelData(1, GL_UNSIGNED_SHORT, texel2[0], texel2[1], texel2[2], texel2[3]),
			getR16UIPixelData(0, GL_UNSIGNED_INT, texel2[0]),
			getRGBA16UIPixelData(1, GL_UNSIGNED_SHORT, texel3[0], texel3[1], texel3[2], texel3[3]),
			getR16UIPixelData(0, GL_UNSIGNED_INT, texel3[0]),
			getRGBA16UIPixelData(1, GL_UNSIGNED_SHORT, texel4[0], texel4[1], texel4[2], texel4[3]),
			getR16UIPixelData(0, GL_UNSIGNED_INT, texel4[0]), PIXEL_COMPARE_CHANNEL_R);

		/* GL_RGBA16UI => GL_RG16UI */
		addEntryToConversionDatabase(
			getRGBA16UIPixelData(1, GL_UNSIGNED_SHORT, texel1[0], texel1[1], texel1[2], texel1[3]),
			getRG16UIPixelData(0, GL_UNSIGNED_INT, texel1[0], texel1[1]),
			getRGBA16UIPixelData(1, GL_UNSIGNED_SHORT, texel2[0], texel2[1], texel2[2], texel2[3]),
			getRG16UIPixelData(0, GL_UNSIGNED_INT, texel2[0], texel2[1]),
			getRGBA16UIPixelData(1, GL_UNSIGNED_SHORT, texel3[0], texel3[1], texel3[2], texel3[3]),
			getRG16UIPixelData(0, GL_UNSIGNED_INT, texel3[0], texel3[1]),
			getRGBA16UIPixelData(1, GL_UNSIGNED_SHORT, texel4[0], texel4[1], texel4[2], texel4[3]),
			getRG16UIPixelData(0, GL_UNSIGNED_INT, texel4[0], texel4[1]), PIXEL_COMPARE_CHANNEL_RG);

		/* GL_RGBA16UI => GL_RGB16UI */
		addEntryToConversionDatabase(
			getRGBA16UIPixelData(1, GL_UNSIGNED_SHORT, texel1[0], texel1[1], texel1[2], texel1[3]),
			getRGB16UIPixelData(0, GL_UNSIGNED_INT, texel1[0], texel1[1], texel1[2]),
			getRGBA16UIPixelData(1, GL_UNSIGNED_SHORT, texel2[0], texel2[1], texel2[2], texel2[3]),
			getRGB16UIPixelData(0, GL_UNSIGNED_INT, texel2[0], texel2[1], texel2[2]),
			getRGBA16UIPixelData(1, GL_UNSIGNED_SHORT, texel3[0], texel3[1], texel3[2], texel3[3]),
			getRGB16UIPixelData(0, GL_UNSIGNED_INT, texel3[0], texel3[1], texel3[2]),
			getRGBA16UIPixelData(1, GL_UNSIGNED_SHORT, texel4[0], texel4[1], texel4[2], texel4[3]),
			getRGB16UIPixelData(0, GL_UNSIGNED_INT, texel4[0], texel4[1], texel4[2]), PIXEL_COMPARE_CHANNEL_RGB);

		/* GL_RGBA16UI => GL_RGBA16UI */
		addEntryToConversionDatabase(
			getRGBA16UIPixelData(1, GL_UNSIGNED_SHORT, texel1[0], texel1[1], texel1[2], texel1[3]),
			getRGBA16UIPixelData(0, GL_UNSIGNED_INT, texel1[0], texel1[1], texel1[2], texel1[3]),
			getRGBA16UIPixelData(1, GL_UNSIGNED_SHORT, texel2[0], texel2[1], texel2[2], texel2[3]),
			getRGBA16UIPixelData(0, GL_UNSIGNED_INT, texel2[0], texel2[1], texel2[2], texel2[3]),
			getRGBA16UIPixelData(1, GL_UNSIGNED_SHORT, texel3[0], texel3[1], texel3[2], texel3[3]),
			getRGBA16UIPixelData(0, GL_UNSIGNED_INT, texel3[0], texel3[1], texel3[2], texel3[3]),
			getRGBA16UIPixelData(1, GL_UNSIGNED_SHORT, texel4[0], texel4[1], texel4[2], texel4[3]),
			getRGBA16UIPixelData(0, GL_UNSIGNED_INT, texel4[0], texel4[1], texel4[2], texel4[3]),
			PIXEL_COMPARE_CHANNEL_RGBA);
	}

	/* GL_RGBA32I */
	{
		const int texel1[4] = { 2147483647, 715827883, -715827881, -2147483647 };
		const int texel2[4] = { 715827883, -715827881, -2147483647, 2147483647 };
		const int texel3[4] = { -715827881, -2147483647, 2147483647, 715827883 };
		const int texel4[4] = { -2147483647, 2147483647, 715827883, -715827881 };

		/* GL_RGBA32I => GL_R32I */
		addEntryToConversionDatabase(getRGBA32IPixelData(GL_INT, texel1[0], texel1[1], texel1[2], texel1[3]),
									 getR32IPixelData(0, GL_INT, texel1[0]),
									 getRGBA32IPixelData(GL_INT, texel2[0], texel2[1], texel2[2], texel2[3]),
									 getR32IPixelData(0, GL_INT, texel2[0]),
									 getRGBA32IPixelData(GL_INT, texel3[0], texel3[1], texel3[2], texel3[3]),
									 getR32IPixelData(0, GL_INT, texel3[0]),
									 getRGBA32IPixelData(GL_INT, texel4[0], texel4[1], texel4[2], texel4[3]),
									 getR32IPixelData(0, GL_INT, texel4[0]), PIXEL_COMPARE_CHANNEL_R);

		/* GL_RGBA32I => GL_RG32I */
		addEntryToConversionDatabase(getRGBA32IPixelData(GL_INT, texel1[0], texel1[1], texel1[2], texel1[3]),
									 getRG32IPixelData(0, GL_INT, texel1[0], texel1[1]),
									 getRGBA32IPixelData(GL_INT, texel2[0], texel2[1], texel2[2], texel2[3]),
									 getRG32IPixelData(0, GL_INT, texel2[0], texel2[1]),
									 getRGBA32IPixelData(GL_INT, texel3[0], texel3[1], texel3[2], texel3[3]),
									 getRG32IPixelData(0, GL_INT, texel3[0], texel3[1]),
									 getRGBA32IPixelData(GL_INT, texel4[0], texel4[1], texel4[2], texel4[3]),
									 getRG32IPixelData(0, GL_INT, texel4[0], texel4[1]), PIXEL_COMPARE_CHANNEL_RG);

		/* GL_RGBA32I => GL_RGB32I */
		addEntryToConversionDatabase(getRGBA32IPixelData(GL_INT, texel1[0], texel1[1], texel1[2], texel1[3]),
									 getRGB32IPixelData(0, GL_INT, texel1[0], texel1[1], texel1[2]),
									 getRGBA32IPixelData(GL_INT, texel2[0], texel2[1], texel2[2], texel2[3]),
									 getRGB32IPixelData(0, GL_INT, texel2[0], texel2[1], texel2[2]),
									 getRGBA32IPixelData(GL_INT, texel3[0], texel3[1], texel3[2], texel3[3]),
									 getRGB32IPixelData(0, GL_INT, texel3[0], texel3[1], texel3[2]),
									 getRGBA32IPixelData(GL_INT, texel4[0], texel4[1], texel4[2], texel4[3]),
									 getRGB32IPixelData(0, GL_INT, texel4[0], texel4[1], texel4[2]),
									 PIXEL_COMPARE_CHANNEL_RGB);

		/* GL_RGBA32I => GL_RGBA32I */
		addEntryToConversionDatabase(getRGBA32IPixelData(GL_INT, texel1[0], texel1[1], texel1[2], texel1[3]),
									 getRGBA32IPixelData(GL_INT, texel1[0], texel1[1], texel1[2], texel1[3]),
									 getRGBA32IPixelData(GL_INT, texel2[0], texel2[1], texel2[2], texel2[3]),
									 getRGBA32IPixelData(GL_INT, texel2[0], texel2[1], texel2[2], texel2[3]),
									 getRGBA32IPixelData(GL_INT, texel3[0], texel3[1], texel3[2], texel3[3]),
									 getRGBA32IPixelData(GL_INT, texel3[0], texel3[1], texel3[2], texel3[3]),
									 getRGBA32IPixelData(GL_INT, texel4[0], texel4[1], texel4[2], texel4[3]),
									 getRGBA32IPixelData(GL_INT, texel4[0], texel4[1], texel4[2], texel4[3]),
									 PIXEL_COMPARE_CHANNEL_RGBA);
	}

	/* GL_RGBA32UI */
	{
		const unsigned int texel1[4] = { 4294967295u, 2863311530u, 1431655765u, 0 };
		const unsigned int texel2[4] = { 2863311530u, 1431655765u, 0, 4294967295u };
		const unsigned int texel3[4] = { 1431655765u, 0, 4294967295u, 2863311530u };
		const unsigned int texel4[4] = { 0, 4294967295u, 2863311530u, 1431655765u };

		/* GL_RGBA32UI => GL_R32UI */
		addEntryToConversionDatabase(getRGBA32UIPixelData(GL_UNSIGNED_INT, texel1[0], texel1[1], texel1[2], texel1[3]),
									 getR32UIPixelData(0, GL_UNSIGNED_INT, texel1[0]),
									 getRGBA32UIPixelData(GL_UNSIGNED_INT, texel2[0], texel2[1], texel2[2], texel2[3]),
									 getR32UIPixelData(0, GL_UNSIGNED_INT, texel2[0]),
									 getRGBA32UIPixelData(GL_UNSIGNED_INT, texel3[0], texel3[1], texel3[2], texel3[3]),
									 getR32UIPixelData(0, GL_UNSIGNED_INT, texel3[0]),
									 getRGBA32UIPixelData(GL_UNSIGNED_INT, texel4[0], texel4[1], texel4[2], texel4[3]),
									 getR32UIPixelData(0, GL_UNSIGNED_INT, texel4[0]), PIXEL_COMPARE_CHANNEL_R);

		/* GL_RGBA32UI => GL_RG32UI */
		addEntryToConversionDatabase(getRGBA32UIPixelData(GL_UNSIGNED_INT, texel1[0], texel1[1], texel1[2], texel1[3]),
									 getRG32UIPixelData(0, GL_UNSIGNED_INT, texel1[0], texel1[1]),
									 getRGBA32UIPixelData(GL_UNSIGNED_INT, texel2[0], texel2[1], texel2[2], texel2[3]),
									 getRG32UIPixelData(0, GL_UNSIGNED_INT, texel2[0], texel2[1]),
									 getRGBA32UIPixelData(GL_UNSIGNED_INT, texel3[0], texel3[1], texel3[2], texel3[3]),
									 getRG32UIPixelData(0, GL_UNSIGNED_INT, texel3[0], texel3[1]),
									 getRGBA32UIPixelData(GL_UNSIGNED_INT, texel4[0], texel4[1], texel4[2], texel4[3]),
									 getRG32UIPixelData(0, GL_UNSIGNED_INT, texel4[0], texel4[1]),
									 PIXEL_COMPARE_CHANNEL_RG);

		/* GL_RGBA32UI => GL_RGB32UI */
		addEntryToConversionDatabase(getRGBA32UIPixelData(GL_UNSIGNED_INT, texel1[0], texel1[1], texel1[2], texel1[3]),
									 getRGB32UIPixelData(0, GL_UNSIGNED_INT, texel1[0], texel1[1], texel1[2]),
									 getRGBA32UIPixelData(GL_UNSIGNED_INT, texel2[0], texel2[1], texel2[2], texel2[3]),
									 getRGB32UIPixelData(0, GL_UNSIGNED_INT, texel2[0], texel2[1], texel2[2]),
									 getRGBA32UIPixelData(GL_UNSIGNED_INT, texel3[0], texel3[1], texel3[2], texel3[3]),
									 getRGB32UIPixelData(0, GL_UNSIGNED_INT, texel3[0], texel3[1], texel3[2]),
									 getRGBA32UIPixelData(GL_UNSIGNED_INT, texel4[0], texel4[1], texel4[2], texel4[3]),
									 getRGB32UIPixelData(0, GL_UNSIGNED_INT, texel4[0], texel4[1], texel4[2]),
									 PIXEL_COMPARE_CHANNEL_RGB);

		/* GL_RGBA32UI => GL_RGBA32UI */
		addEntryToConversionDatabase(getRGBA32UIPixelData(GL_UNSIGNED_INT, texel1[0], texel1[1], texel1[2], texel1[3]),
									 getRGBA32UIPixelData(GL_UNSIGNED_INT, texel1[0], texel1[1], texel1[2], texel1[3]),
									 getRGBA32UIPixelData(GL_UNSIGNED_INT, texel2[0], texel2[1], texel2[2], texel2[3]),
									 getRGBA32UIPixelData(GL_UNSIGNED_INT, texel2[0], texel2[1], texel2[2], texel2[3]),
									 getRGBA32UIPixelData(GL_UNSIGNED_INT, texel3[0], texel3[1], texel3[2], texel3[3]),
									 getRGBA32UIPixelData(GL_UNSIGNED_INT, texel3[0], texel3[1], texel3[2], texel3[3]),
									 getRGBA32UIPixelData(GL_UNSIGNED_INT, texel4[0], texel4[1], texel4[2], texel4[3]),
									 getRGBA32UIPixelData(GL_UNSIGNED_INT, texel4[0], texel4[1], texel4[2], texel4[3]),
									 PIXEL_COMPARE_CHANNEL_RGBA);
	}

	/* GL_R16F */
	{
		const float texel1[1] = { 1 };
		const float texel2[1] = { 4096 };
		const float texel3[1] = { -4096 };
		const float texel4[1] = { 32000 };

		/* GL_R16F => GL_R16F */
		addEntryToConversionDatabase(
			getR16FPixelData(1, GL_HALF_FLOAT, texel1[0]), getR16FPixelData(0, GL_HALF_FLOAT, texel1[0]),
			getR16FPixelData(1, GL_HALF_FLOAT, texel2[0]), getR16FPixelData(0, GL_HALF_FLOAT, texel2[0]),
			getR16FPixelData(1, GL_HALF_FLOAT, texel3[0]), getR16FPixelData(0, GL_HALF_FLOAT, texel3[0]),
			getR16FPixelData(1, GL_HALF_FLOAT, texel4[0]), getR16FPixelData(0, GL_HALF_FLOAT, texel4[0]),
			PIXEL_COMPARE_CHANNEL_R);
	}

	/* GL_RG16F */
	{
		const float texel1[2] = { 1, 0 };
		const float texel2[2] = { 4096, -4096 };
		const float texel3[2] = { -32000, 32000 };
		const float texel4[2] = { 1.5f, -4.7f };

		/* GL_RG16F => GL_R16F */
		addEntryToConversionDatabase(
			getRG16FPixelData(1, GL_HALF_FLOAT, texel1[0], texel1[1]), getR16FPixelData(0, GL_HALF_FLOAT, texel1[0]),
			getRG16FPixelData(1, GL_HALF_FLOAT, texel2[0], texel2[1]), getR16FPixelData(0, GL_HALF_FLOAT, texel2[0]),
			getRG16FPixelData(1, GL_HALF_FLOAT, texel3[0], texel3[1]), getR16FPixelData(0, GL_HALF_FLOAT, texel3[0]),
			getRG16FPixelData(1, GL_HALF_FLOAT, texel4[0], texel4[1]), getR16FPixelData(0, GL_HALF_FLOAT, texel4[0]),
			PIXEL_COMPARE_CHANNEL_R);

		/* GL_RG16F => GL_RG16F */
		addEntryToConversionDatabase(getRG16FPixelData(1, GL_HALF_FLOAT, texel1[0], texel1[1]),
									 getRG16FPixelData(0, GL_HALF_FLOAT, texel1[0], texel1[1]),
									 getRG16FPixelData(1, GL_HALF_FLOAT, texel2[0], texel2[1]),
									 getRG16FPixelData(0, GL_HALF_FLOAT, texel2[0], texel2[1]),
									 getRG16FPixelData(1, GL_HALF_FLOAT, texel3[0], texel3[1]),
									 getRG16FPixelData(0, GL_HALF_FLOAT, texel3[0], texel3[1]),
									 getRG16FPixelData(1, GL_HALF_FLOAT, texel4[0], texel4[1]),
									 getRG16FPixelData(0, GL_HALF_FLOAT, texel4[0], texel4[1]),
									 PIXEL_COMPARE_CHANNEL_RG);
	}

	/* GL_R32F */
	{
		const float texel1[1] = { 1 };
		const float texel2[1] = { 4096 };
		const float texel3[1] = { -4096 };
		const float texel4[1] = { 32000 };

		/* GL_R32F => GL_R32F */
		addEntryToConversionDatabase(getR32FPixelData(1, GL_FLOAT, texel1[0]), getR32FPixelData(0, GL_FLOAT, texel1[0]),
									 getR32FPixelData(1, GL_FLOAT, texel2[0]), getR32FPixelData(0, GL_FLOAT, texel2[0]),
									 getR32FPixelData(1, GL_FLOAT, texel3[0]), getR32FPixelData(0, GL_FLOAT, texel3[0]),
									 getR32FPixelData(1, GL_FLOAT, texel4[0]), getR32FPixelData(0, GL_FLOAT, texel4[0]),
									 PIXEL_COMPARE_CHANNEL_R);
	}

	/* GL_RG32F */
	{
		const float texel1[2] = { 1, 0 };
		const float texel2[2] = { 4096, -4096 };
		const float texel3[2] = { -32000, 32000 };
		const float texel4[2] = { 1.5f, -4.7f };

		/* GL_RG32F => GL_R32F */
		addEntryToConversionDatabase(
			getRG32FPixelData(1, GL_FLOAT, texel1[0], texel1[1]), getR32FPixelData(0, GL_FLOAT, texel1[0]),
			getRG32FPixelData(1, GL_FLOAT, texel2[0], texel2[1]), getR32FPixelData(0, GL_FLOAT, texel2[0]),
			getRG32FPixelData(1, GL_FLOAT, texel3[0], texel3[1]), getR32FPixelData(0, GL_FLOAT, texel3[0]),
			getRG32FPixelData(1, GL_FLOAT, texel4[0], texel4[1]), getR32FPixelData(0, GL_FLOAT, texel4[0]),
			PIXEL_COMPARE_CHANNEL_R);

		/* GL_RG32F => GL_RG32F */
		addEntryToConversionDatabase(
			getRG32FPixelData(1, GL_FLOAT, texel1[0], texel1[1]), getRG32FPixelData(0, GL_FLOAT, texel1[0], texel1[1]),
			getRG32FPixelData(1, GL_FLOAT, texel2[0], texel2[1]), getRG32FPixelData(0, GL_FLOAT, texel2[0], texel2[1]),
			getRG32FPixelData(1, GL_FLOAT, texel3[0], texel3[1]), getRG32FPixelData(0, GL_FLOAT, texel3[0], texel3[1]),
			getRG32FPixelData(1, GL_FLOAT, texel4[0], texel4[1]), getRG32FPixelData(0, GL_FLOAT, texel4[0], texel4[1]),
			PIXEL_COMPARE_CHANNEL_RG);
	}

	/* GL_RGB16F */
	{
		const float texel1[3] = { 1, 0, -1 };
		const float texel2[3] = { 4096, -4096, 127.5f };
		const float texel3[3] = { -32000, 32000, -456.7f };
		const float texel4[3] = { 1.5f, -4.7f, 123.6f };

		/* GL_RGB16F => GL_R16F */
		addEntryToConversionDatabase(getRGB16FPixelData(1, GL_HALF_FLOAT, texel1[0], texel1[1], texel1[2]),
									 getR16FPixelData(0, GL_HALF_FLOAT, texel1[0]),
									 getRGB16FPixelData(1, GL_HALF_FLOAT, texel2[0], texel2[1], texel2[2]),
									 getR16FPixelData(0, GL_HALF_FLOAT, texel2[0]),
									 getRGB16FPixelData(1, GL_HALF_FLOAT, texel3[0], texel3[1], texel3[2]),
									 getR16FPixelData(0, GL_HALF_FLOAT, texel3[0]),
									 getRGB16FPixelData(1, GL_HALF_FLOAT, texel4[0], texel4[1], texel4[2]),
									 getR16FPixelData(0, GL_HALF_FLOAT, texel4[0]), PIXEL_COMPARE_CHANNEL_R);

		/* GL_RGB16F => GL_RG16F */
		addEntryToConversionDatabase(getRGB16FPixelData(1, GL_HALF_FLOAT, texel1[0], texel1[1], texel1[2]),
									 getRG16FPixelData(0, GL_HALF_FLOAT, texel1[0], texel1[1]),
									 getRGB16FPixelData(1, GL_HALF_FLOAT, texel2[0], texel2[1], texel2[2]),
									 getRG16FPixelData(0, GL_HALF_FLOAT, texel2[0], texel2[1]),
									 getRGB16FPixelData(1, GL_HALF_FLOAT, texel3[0], texel3[1], texel3[2]),
									 getRG16FPixelData(0, GL_HALF_FLOAT, texel3[0], texel3[1]),
									 getRGB16FPixelData(1, GL_HALF_FLOAT, texel4[0], texel4[1], texel4[2]),
									 getRG16FPixelData(0, GL_HALF_FLOAT, texel4[0], texel4[1]),
									 PIXEL_COMPARE_CHANNEL_RG);

		/* GL_RGB16F => GL_RGB16F */
		addEntryToConversionDatabase(getRGB16FPixelData(1, GL_HALF_FLOAT, texel1[0], texel1[1], texel1[2]),
									 getRGB16FPixelData(0, GL_HALF_FLOAT, texel1[0], texel1[1], texel1[2]),
									 getRGB16FPixelData(1, GL_HALF_FLOAT, texel2[0], texel2[1], texel2[2]),
									 getRGB16FPixelData(0, GL_HALF_FLOAT, texel2[0], texel2[1], texel2[2]),
									 getRGB16FPixelData(1, GL_HALF_FLOAT, texel3[0], texel3[1], texel3[2]),
									 getRGB16FPixelData(0, GL_HALF_FLOAT, texel3[0], texel3[1], texel3[2]),
									 getRGB16FPixelData(1, GL_HALF_FLOAT, texel4[0], texel4[1], texel4[2]),
									 getRGB16FPixelData(0, GL_HALF_FLOAT, texel4[0], texel4[1], texel4[2]),
									 PIXEL_COMPARE_CHANNEL_RGB);
	}

	/* GL_RGBA16F */
	{
		const float texel1[4] = { 1, 0, -1, 0.25f };
		const float texel2[4] = { 4096, -4096, 127.5f, 0.5f };
		const float texel3[4] = { -32000, 32000, -456.7f, 0.75f };
		const float texel4[4] = { 1.5f, -4.7f, 123.6f, 1 };

		/* GL_RGBA16F => GL_R16F */
		addEntryToConversionDatabase(getRGBA16FPixelData(GL_HALF_FLOAT, texel1[0], texel1[1], texel1[2], texel1[3]),
									 getR16FPixelData(0, GL_HALF_FLOAT, texel1[0]),
									 getRGBA16FPixelData(GL_HALF_FLOAT, texel2[0], texel2[1], texel2[2], texel2[3]),
									 getR16FPixelData(0, GL_HALF_FLOAT, texel2[0]),
									 getRGBA16FPixelData(GL_HALF_FLOAT, texel3[0], texel3[1], texel3[2], texel3[3]),
									 getR16FPixelData(0, GL_HALF_FLOAT, texel3[0]),
									 getRGBA16FPixelData(GL_HALF_FLOAT, texel4[0], texel4[1], texel4[2], texel4[3]),
									 getR16FPixelData(0, GL_HALF_FLOAT, texel4[0]), PIXEL_COMPARE_CHANNEL_R);

		/* GL_RGBA16F => GL_RG16F */
		addEntryToConversionDatabase(getRGBA16FPixelData(GL_HALF_FLOAT, texel1[0], texel1[1], texel1[2], texel1[3]),
									 getRG16FPixelData(0, GL_HALF_FLOAT, texel1[0], texel1[1]),
									 getRGBA16FPixelData(GL_HALF_FLOAT, texel2[0], texel2[1], texel2[2], texel2[3]),
									 getRG16FPixelData(0, GL_HALF_FLOAT, texel2[0], texel2[1]),
									 getRGBA16FPixelData(GL_HALF_FLOAT, texel3[0], texel3[1], texel3[2], texel3[3]),
									 getRG16FPixelData(0, GL_HALF_FLOAT, texel3[0], texel3[1]),
									 getRGBA16FPixelData(GL_HALF_FLOAT, texel4[0], texel4[1], texel4[2], texel4[3]),
									 getRG16FPixelData(0, GL_HALF_FLOAT, texel4[0], texel4[1]),
									 PIXEL_COMPARE_CHANNEL_RG);

		/* GL_RGBA16F => GL_RGB16F */
		addEntryToConversionDatabase(getRGBA16FPixelData(GL_HALF_FLOAT, texel1[0], texel1[1], texel1[2], texel1[3]),
									 getRGB16FPixelData(0, GL_HALF_FLOAT, texel1[0], texel1[1], texel1[2]),
									 getRGBA16FPixelData(GL_HALF_FLOAT, texel2[0], texel2[1], texel2[2], texel2[3]),
									 getRGB16FPixelData(0, GL_HALF_FLOAT, texel2[0], texel2[1], texel2[2]),
									 getRGBA16FPixelData(GL_HALF_FLOAT, texel3[0], texel3[1], texel3[2], texel3[3]),
									 getRGB16FPixelData(0, GL_HALF_FLOAT, texel3[0], texel3[1], texel3[2]),
									 getRGBA16FPixelData(GL_HALF_FLOAT, texel4[0], texel4[1], texel4[2], texel4[3]),
									 getRGB16FPixelData(0, GL_HALF_FLOAT, texel4[0], texel4[1], texel4[2]),
									 PIXEL_COMPARE_CHANNEL_RGB);

		/* GL_RGBA16F => GL_RGBA16F */
		addEntryToConversionDatabase(getRGBA16FPixelData(GL_HALF_FLOAT, texel1[0], texel1[1], texel1[2], texel1[3]),
									 getRGBA16FPixelData(GL_HALF_FLOAT, texel1[0], texel1[1], texel1[2], texel1[3]),
									 getRGBA16FPixelData(GL_HALF_FLOAT, texel2[0], texel2[1], texel2[2], texel2[3]),
									 getRGBA16FPixelData(GL_HALF_FLOAT, texel2[0], texel2[1], texel2[2], texel2[3]),
									 getRGBA16FPixelData(GL_HALF_FLOAT, texel3[0], texel3[1], texel3[2], texel3[3]),
									 getRGBA16FPixelData(GL_HALF_FLOAT, texel3[0], texel3[1], texel3[2], texel3[3]),
									 getRGBA16FPixelData(GL_HALF_FLOAT, texel4[0], texel4[1], texel4[2], texel4[3]),
									 getRGBA16FPixelData(GL_HALF_FLOAT, texel4[0], texel4[1], texel4[2], texel4[3]),
									 PIXEL_COMPARE_CHANNEL_RGBA);
	}

	/* GL_RGB32F */
	{
		const float texel1[3] = { 1, 0, -1 };
		const float texel2[3] = { 4096, -4096, 127.5f };
		const float texel3[3] = { -32000, 32000, -456.7f };
		const float texel4[3] = { 1.5f, -4.7f, 123.6f };

		/* GL_RGB32F => GL_R32F */
		addEntryToConversionDatabase(
			getRGB32FPixelData(1, GL_FLOAT, texel1[0], texel1[1], texel1[2]), getR32FPixelData(0, GL_FLOAT, texel1[0]),
			getRGB32FPixelData(1, GL_FLOAT, texel2[0], texel2[1], texel2[2]), getR32FPixelData(0, GL_FLOAT, texel2[0]),
			getRGB32FPixelData(1, GL_FLOAT, texel3[0], texel3[1], texel3[2]), getR32FPixelData(0, GL_FLOAT, texel3[0]),
			getRGB32FPixelData(1, GL_FLOAT, texel4[0], texel4[1], texel4[2]), getR32FPixelData(0, GL_FLOAT, texel4[0]),
			PIXEL_COMPARE_CHANNEL_R);

		/* GL_RGB32F => GL_RG32F */
		addEntryToConversionDatabase(getRGB32FPixelData(1, GL_FLOAT, texel1[0], texel1[1], texel1[2]),
									 getRG32FPixelData(0, GL_FLOAT, texel1[0], texel1[1]),
									 getRGB32FPixelData(1, GL_FLOAT, texel2[0], texel2[1], texel2[2]),
									 getRG32FPixelData(0, GL_FLOAT, texel2[0], texel2[1]),
									 getRGB32FPixelData(1, GL_FLOAT, texel3[0], texel3[1], texel3[2]),
									 getRG32FPixelData(0, GL_FLOAT, texel3[0], texel3[1]),
									 getRGB32FPixelData(1, GL_FLOAT, texel4[0], texel4[1], texel4[2]),
									 getRG32FPixelData(0, GL_FLOAT, texel4[0], texel4[1]), PIXEL_COMPARE_CHANNEL_RG);

		/* GL_RGB32F => GL_RGB32F */
		addEntryToConversionDatabase(getRGB32FPixelData(1, GL_FLOAT, texel1[0], texel1[1], texel1[2]),
									 getRGB32FPixelData(0, GL_FLOAT, texel1[0], texel1[1], texel1[2]),
									 getRGB32FPixelData(1, GL_FLOAT, texel2[0], texel2[1], texel2[2]),
									 getRGB32FPixelData(0, GL_FLOAT, texel2[0], texel2[1], texel2[2]),
									 getRGB32FPixelData(1, GL_FLOAT, texel3[0], texel3[1], texel3[2]),
									 getRGB32FPixelData(0, GL_FLOAT, texel3[0], texel3[1], texel3[2]),
									 getRGB32FPixelData(1, GL_FLOAT, texel4[0], texel4[1], texel4[2]),
									 getRGB32FPixelData(0, GL_FLOAT, texel4[0], texel4[1], texel4[2]),
									 PIXEL_COMPARE_CHANNEL_RGB);
	}

	/* GL_RGBA32F */
	{
		const float texel1[4] = { 1, 0, -1, 0.25f };
		const float texel2[4] = { 4096, -4096, 127.5f, 0.5f };
		const float texel3[4] = { -32000, 32000, -456.7f, 0.75f };
		const float texel4[4] = { 1.5f, -4.7f, 123.6f, 1 };

		/* GL_RGBA32F => GL_R32F */
		addEntryToConversionDatabase(getRGBA32FPixelData(GL_FLOAT, texel1[0], texel1[1], texel1[2], texel1[3]),
									 getR32FPixelData(0, GL_FLOAT, texel1[0]),
									 getRGBA32FPixelData(GL_FLOAT, texel2[0], texel2[1], texel2[2], texel2[3]),
									 getR32FPixelData(0, GL_FLOAT, texel2[0]),
									 getRGBA32FPixelData(GL_FLOAT, texel3[0], texel3[1], texel3[2], texel3[3]),
									 getR32FPixelData(0, GL_FLOAT, texel3[0]),
									 getRGBA32FPixelData(GL_FLOAT, texel4[0], texel4[1], texel4[2], texel4[3]),
									 getR32FPixelData(0, GL_FLOAT, texel4[0]), PIXEL_COMPARE_CHANNEL_R);

		/* GL_RGBA32F => GL_RG32F */
		addEntryToConversionDatabase(getRGBA32FPixelData(GL_FLOAT, texel1[0], texel1[1], texel1[2], texel1[3]),
									 getRG32FPixelData(0, GL_FLOAT, texel1[0], texel1[1]),
									 getRGBA32FPixelData(GL_FLOAT, texel2[0], texel2[1], texel2[2], texel2[3]),
									 getRG32FPixelData(0, GL_FLOAT, texel2[0], texel2[1]),
									 getRGBA32FPixelData(GL_FLOAT, texel3[0], texel3[1], texel3[2], texel3[3]),
									 getRG32FPixelData(0, GL_FLOAT, texel3[0], texel3[1]),
									 getRGBA32FPixelData(GL_FLOAT, texel4[0], texel4[1], texel4[2], texel4[3]),
									 getRG32FPixelData(0, GL_FLOAT, texel4[0], texel4[1]), PIXEL_COMPARE_CHANNEL_RG);

		/* GL_RGBA32F => GL_RGB32F */
		addEntryToConversionDatabase(getRGBA32FPixelData(GL_FLOAT, texel1[0], texel1[1], texel1[2], texel1[3]),
									 getRGB32FPixelData(0, GL_FLOAT, texel1[0], texel1[1], texel1[2]),
									 getRGBA32FPixelData(GL_FLOAT, texel2[0], texel2[1], texel2[2], texel2[3]),
									 getRGB32FPixelData(0, GL_FLOAT, texel2[0], texel2[1], texel2[2]),
									 getRGBA32FPixelData(GL_FLOAT, texel3[0], texel3[1], texel3[2], texel3[3]),
									 getRGB32FPixelData(0, GL_FLOAT, texel3[0], texel3[1], texel3[2]),
									 getRGBA32FPixelData(GL_FLOAT, texel4[0], texel4[1], texel4[2], texel4[3]),
									 getRGB32FPixelData(0, GL_FLOAT, texel4[0], texel4[1], texel4[2]),
									 PIXEL_COMPARE_CHANNEL_RGB);

		/* GL_RGBA32F => GL_RGBA32F */
		addEntryToConversionDatabase(getRGBA32FPixelData(GL_FLOAT, texel1[0], texel1[1], texel1[2], texel1[3]),
									 getRGBA32FPixelData(GL_FLOAT, texel1[0], texel1[1], texel1[2], texel1[3]),
									 getRGBA32FPixelData(GL_FLOAT, texel2[0], texel2[1], texel2[2], texel2[3]),
									 getRGBA32FPixelData(GL_FLOAT, texel2[0], texel2[1], texel2[2], texel2[3]),
									 getRGBA32FPixelData(GL_FLOAT, texel3[0], texel3[1], texel3[2], texel3[3]),
									 getRGBA32FPixelData(GL_FLOAT, texel3[0], texel3[1], texel3[2], texel3[3]),
									 getRGBA32FPixelData(GL_FLOAT, texel4[0], texel4[1], texel4[2], texel4[3]),
									 getRGBA32FPixelData(GL_FLOAT, texel4[0], texel4[1], texel4[2], texel4[3]),
									 PIXEL_COMPARE_CHANNEL_RGBA);
	}
}

class TestBase : public deqp::TestCase
{
public:
	TestBase(deqp::Context& context, GLenum source_attachment_type, GLenum destination_attachment_type);
	virtual ~TestBase();

protected:
	bool getFormatAndTypeCompatibleWithInternalformat(GLenum internalformat, int index, GLenum* out_format,
													  GLenum* out_type) const;
	bool getFormatForInternalformat(GLenum internalformat, GLenum* out_format) const;
	GLenum getFBOEffectiveInternalFormatAtIndex(unsigned int index) const;
	GLenum getCopyTexImage2DInternalFormatAtIndex(unsigned int index) const;
	const char* getTargetName(GLenum target) const;
	GLenum getGeneralTargetForDetailedTarget(GLenum target);

	GLuint generateGLObject(GLenum object_type);
	bool configureGLObject(int is_source_gl_object, GLenum object_target, GLint object_id, GLenum internal_format,
						   GLenum format, GLenum type, void* data);
	void destroyGLObject(GLenum target, GLuint object_id);

	bool isValidRBOInternalFormat(GLenum internalformat) const;
	bool isColorRenderableInternalFormat(GLenum internalformat) const;
	bool isDepthRenderableInternalFormat(GLenum internalformat) const;
	bool isDepthStencilRenderableInternalFormat(GLenum internalformat) const;
	bool isFBOEffectiveInternalFormatCompatibleWithDestinationInternalFormat(GLenum src_internalformat,
																			 GLenum dst_internalformat) const;
	const char* getInternalformatString(GLenum internalformat);

protected:
	GLenum m_source_attachment_type;
	GLenum m_destination_attachment_type;
};

TestBase::TestBase(deqp::Context& context, GLenum source_attachment_type, GLenum destination_attachment_type)
	: deqp::TestCase(context, "", "")
	, m_source_attachment_type(source_attachment_type)
	, m_destination_attachment_type(destination_attachment_type)
{
	static std::map<GLenum, std::string> attachment_name_map;
	if (attachment_name_map.empty())
	{
		attachment_name_map[GL_TEXTURE_2D]					= "texture2d";
		attachment_name_map[GL_TEXTURE_CUBE_MAP_NEGATIVE_X] = "cubemap_negx";
		attachment_name_map[GL_TEXTURE_CUBE_MAP_NEGATIVE_Y] = "cubemap_negy";
		attachment_name_map[GL_TEXTURE_CUBE_MAP_NEGATIVE_Z] = "cubemap_negz";
		attachment_name_map[GL_TEXTURE_CUBE_MAP_POSITIVE_X] = "cubemap_posx";
		attachment_name_map[GL_TEXTURE_CUBE_MAP_POSITIVE_Y] = "cubemap_posy";
		attachment_name_map[GL_TEXTURE_CUBE_MAP_POSITIVE_Z] = "cubemap_posz";
		attachment_name_map[GL_TEXTURE_2D_ARRAY]			= "texture_array";
		attachment_name_map[GL_TEXTURE_3D]					= "texture3d";
		attachment_name_map[GL_RENDERBUFFER]				= "renderbuffer";
	}

	m_name = attachment_name_map[m_source_attachment_type] + "_" + attachment_name_map[m_destination_attachment_type];
}

TestBase::~TestBase()
{
}

/** For every valid GLES internalformat, gl.readPixels() can often work with a variety of different
 *  format+type combinations. This function allows to enumerate valid pairs for user-specified
 *  internal formats.
 *
 *  Enumeration should start from 0 and continue until the function starts reporting failure.
 *
 *  @param internalformat GLES internal format to consider.
 *  @param index		  Index of format+type pair to look up.
 *  @param out_format	 Deref will be used to store compatible GLES format. Cannot be NULL.
 *  @param out_type	   Deref will be used to store compatible GLES type. Cannot be NULL.
 *
 *  @return true if successful and relevant format & type information has been stored under
 *		  dereferences of corresponding arguments, false otherwise.
 **/
bool TestBase::getFormatAndTypeCompatibleWithInternalformat(GLenum internalformat, int index, GLenum* out_format,
															GLenum* out_type) const
{
	const glu::ContextInfo& contextInfo   = m_context.getContextInfo();
	bool is_ext_texture_storage_supported = contextInfo.isExtensionSupported("GL_EXT_texture_storage");
	bool is_ext_texture_type_2_10_10_10_rev_supported =
		contextInfo.isExtensionSupported("GL_EXT_texture_type_2_10_10_10_REV");

	DE_ASSERT(out_format != NULL);
	DE_ASSERT(out_type != NULL);

	if (!getFormatForInternalformat(internalformat, out_format))
		TCU_FAIL("No format known for requested internalformat");

	switch (internalformat)
	{
	case GL_ALPHA:
	{
		if (index == 0)
			*out_type = GL_UNSIGNED_BYTE;
		else
			return false;
		break;
	}

	case GL_LUMINANCE:
	{
		if (index == 0)
			*out_type = GL_UNSIGNED_BYTE;
		else
			return false;
		break;
	}

	case GL_R8:
	{
		if (index == 0)
			*out_type = GL_UNSIGNED_BYTE;
		else
			return false;
		break;
	}

	case GL_LUMINANCE_ALPHA:
	{
		if (index == 0)
			*out_type = GL_UNSIGNED_BYTE;
		else
			return false;
		break;
	}

	case GL_RG8:
	{
		if (index == 0)
			*out_type = GL_UNSIGNED_BYTE;
		else
			return false;
		break;
	}

	case GL_SRGB:
	case GL_RGB:
	{
		if (index == 0)
			*out_type = GL_UNSIGNED_BYTE;
		else if (index == 1)
			*out_type = GL_UNSIGNED_SHORT_5_6_5;
		else if (index == 2)
			*out_type = GL_UNSIGNED_INT_2_10_10_10_REV;
		else if (index == 3)
			*out_type = GL_HALF_FLOAT;
		else if (index == 4)
			*out_type = GL_FLOAT;
		else
			return false;
		break;
	}

	case GL_SRGB8:
	case GL_RGB8:
	{
		if (index == 0)
			*out_type = GL_UNSIGNED_BYTE;
		else
			return false;
		break;
	}

	case GL_RGB565:
	{
		if (index == 0)
			*out_type = GL_UNSIGNED_SHORT_5_6_5;
		else if (index == 1)
			*out_type = GL_UNSIGNED_BYTE;
		else
			return false;
		break;
	}

	case GL_RGBA:
	{
		if (index == 0)
			*out_type = GL_UNSIGNED_BYTE;
		else if (index == 1)
			*out_type = GL_UNSIGNED_SHORT_4_4_4_4;
		else if (index == 2)
			*out_type = GL_UNSIGNED_SHORT_5_5_5_1;
		else if (index == 3)
			*out_type = GL_HALF_FLOAT;
		else if (index == 4)
			*out_type = GL_FLOAT;
		else
			return false;
		break;
	}

	case GL_RGBA4:
	{
		if (index == 0)
			*out_type = GL_UNSIGNED_SHORT_4_4_4_4;
		else if (index == 1)
			*out_type = GL_UNSIGNED_BYTE;
		else
			return false;
		break;
	}

	case GL_RGB5_A1:
	{
		if (index == 0)
			*out_type = GL_UNSIGNED_SHORT_5_5_5_1;
		else if (index == 1)
			*out_type = GL_UNSIGNED_BYTE;
		else if (index == 2)
			*out_type = GL_UNSIGNED_INT_2_10_10_10_REV;
		else
			return false;
		break;
	}

	case GL_RGBA8:
	{
		if (index == 0)
			*out_type = GL_UNSIGNED_BYTE;
		else
			return false;
		break;
	}

	case GL_RGB10_A2:
	{
		if (index == 0)
			*out_type = GL_UNSIGNED_INT_2_10_10_10_REV;
		else
			return false;
		break;
	}

	case GL_RGB10_A2UI:
	{
		if (index == 0)
		{
			*out_type = GL_UNSIGNED_INT_2_10_10_10_REV;
		} /* if (index == 0) */
		else
		{
			return false;
		}

		break;
	}

	case GL_SRGB8_ALPHA8:
	{
		if (index == 0)
			*out_type = GL_UNSIGNED_BYTE;
		else
			return false;
		break;
	}

	case GL_R8I:
	{
		if (index == 0)
			*out_type = GL_BYTE;
		else
			return false;
		break;
	}

	case GL_R8UI:
	{
		if (index == 0)
			*out_type = GL_UNSIGNED_BYTE;
		else
			return false;
		break;
	}

	case GL_R16I:
	{
		if (index == 0)
			*out_type = GL_SHORT;
		else
			return false;
		break;
	}

	case GL_R16UI:
	{
		if (index == 0)
			*out_type = GL_UNSIGNED_SHORT;
		else
			return false;
		break;
	}

	case GL_R32I:
	{
		if (index == 0)
			*out_type = GL_INT;
		else
			return false;
		break;
	}

	case GL_R32UI:
	{
		if (index == 0)
			*out_type = GL_UNSIGNED_INT;
		else
			return false;
		break;
	}

	case GL_RG8I:
	{
		if (index == 0)
			*out_type = GL_BYTE;
		else
			return false;
		break;
	}

	case GL_RG8UI:
	{
		if (index == 0)
			*out_type = GL_UNSIGNED_BYTE;
		else
			return false;
		break;
	}

	case GL_RG16I:
	{
		if (index == 0)
			*out_type = GL_SHORT;
		else
			return false;
		break;
	}

	case GL_RG16UI:
	{
		if (index == 0)
			*out_type = GL_UNSIGNED_SHORT;
		else
			return false;
		break;
	}

	case GL_RG32I:
	{
		if (index == 0)
			*out_type = GL_INT;
		else
			return false;
		break;
	}

	case GL_RG32UI:
	{
		if (index == 0)
			*out_type = GL_UNSIGNED_INT;
		else
			return false;
		break;
	}

	case GL_RGB8I:
	{
		if (index == 0)
			*out_type = GL_BYTE;
		else
			return false;
		break;
	}

	case GL_RGB8UI:
	{
		if (index == 0)
			*out_type = GL_UNSIGNED_BYTE;
		else
			return false;
		break;
	}

	case GL_RGB16I:
	{
		if (index == 0)
			*out_type = GL_SHORT;
		else
			return false;
		break;
	}

	case GL_RGB16UI:
	{
		if (index == 0)
			*out_type = GL_UNSIGNED_SHORT;
		else
			return false;
		break;
	}

	case GL_RGB32I:
	{
		if (index == 0)
			*out_type = GL_INT;
		else
			return false;
		break;
	}

	case GL_RGB32UI:
	{
		if (index == 0)
			*out_type = GL_UNSIGNED_INT;
		else
			return false;
		break;
	}

	case GL_RGBA8I:
	{
		if (index == 0)
			*out_type = GL_BYTE;
		else
			return false;
		break;
	}

	case GL_RGBA8UI:
	{
		if (index == 0)
			*out_type = GL_UNSIGNED_BYTE;
		else
			return false;
		break;
	}

	case GL_RGBA16I:
	{
		if (index == 0)
			*out_type = GL_SHORT;
		else
			return false;
		break;
	}

	case GL_RGBA16UI:
	{
		if (index == 0)
			*out_type = GL_UNSIGNED_SHORT;
		else
			return false;
		break;
	}

	case GL_RGBA32I:
	{
		if (index == 0)
			*out_type = GL_INT;
		else
			return false;
		break;
	}

	case GL_RGBA32UI:
	{
		if (index == 0)
			*out_type = GL_UNSIGNED_INT;
		else
			return false;
		break;
	}

	case GL_R16F:
	{
		if (index == 0)
			*out_type = GL_HALF_FLOAT;
		else
			return false;
		break;
	}

	case GL_RG16F:
	{
		if (index == 0)
			*out_type = GL_HALF_FLOAT;
		else
			return false;
		break;
	}

	case GL_R32F:
	{
		if (index == 0)
			*out_type = GL_FLOAT;
		else
			return false;
		break;
	}

	case GL_RG32F:
	{
		if (index == 0)
			*out_type = GL_FLOAT;
		else
			return false;
		break;
	}

	case GL_RGB16F:
	{
		if (index == 0)
			*out_type = GL_HALF_FLOAT;
		else
			return false;
		break;
	}

	case GL_RGBA16F:
	{
		if (index == 0)
			*out_type = GL_HALF_FLOAT;
		else
			return false;
		break;
	}

	case GL_RGB32F:
	{
		if (index == 0)
			*out_type = GL_FLOAT;
		else
			return false;
		break;
	}

	case GL_RGBA32F:
	{
		if (index == 0)
			*out_type = GL_FLOAT;
		else
			return false;
		break;
	}

	case GL_RGB10_EXT:
	{
		if (index == 0)
		{
			if (is_ext_texture_type_2_10_10_10_rev_supported && is_ext_texture_storage_supported)
			{
				*out_type = GL_UNSIGNED_INT_2_10_10_10_REV;
			} /* if (is_ext_texture_type_2_10_10_10_rev_supported) */
			else
			{
				return false;
			}
		} /* if (index == 0) */
		else
		{
			return false;
		}
		break;
	}

	case GL_ALPHA8_EXT:
	{
		// TODO: No extension available at the time of writing.
		return false;
	}

	case GL_LUMINANCE8_EXT:
	{
		// TODO: No extension available at the time of writing.
		return false;
	}

	case GL_LUMINANCE8_ALPHA8_EXT:
	{
		// TODO: No extension available at the time of writing.
		return false;
	}

	default:
	{
		TCU_FAIL("Unsupported internalformat");
	}
	} // switch (internalformat)

	return true;
}

/** Retrieves GLES format compatible for user-specified GLES internal format.
 *
 *  @param internalformat GLES internalformat to consider.
 *  @param out_format	 Deref will be used to store the result. Cannot be NULL.
 *
 *  @return true if successful, false otherwise.
 **/
bool TestBase::getFormatForInternalformat(GLenum internalformat, GLenum* out_format) const
{
	DE_ASSERT(out_format != NULL);

	// Find out the format for user-provided internalformat
	switch (internalformat)
	{
	case GL_ALPHA:
		*out_format = GL_ALPHA;
		break;

	case GL_LUMINANCE_ALPHA:
		*out_format = GL_LUMINANCE_ALPHA;
		break;

	case GL_LUMINANCE:
	case GL_LUMINANCE8_OES:
		*out_format = GL_LUMINANCE;
		break;

	case GL_R8:
	case GL_R8_SNORM:
	case GL_R16F:
	case GL_R32F:
		*out_format = GL_RED;
		break;

	case GL_R8UI:
	case GL_R8I:
	case GL_R16UI:
	case GL_R16I:
	case GL_R32UI:
	case GL_R32I:
		*out_format = GL_RED_INTEGER;
		break;

	case GL_RG8:
	case GL_RG8_SNORM:
	case GL_RG16F:
	case GL_RG32F:
		*out_format = GL_RG;
		break;

	case GL_RG8UI:
	case GL_RG8I:
	case GL_RG16UI:
	case GL_RG16I:
	case GL_RG32UI:
	case GL_RG32I:
		*out_format = GL_RG_INTEGER;
		break;

	case GL_RGB:
	case GL_RGB8:
	case GL_SRGB8:
	case GL_RGB565:
	case GL_RGB8_SNORM:
	case GL_R11F_G11F_B10F:
	case GL_RGB9_E5:
	case GL_RGB16F:
	case GL_RGB32F:
		*out_format = GL_RGB;
		break;

	case GL_RGB8UI:
	case GL_RGB8I:
	case GL_RGB16UI:
	case GL_RGB16I:
	case GL_RGB32UI:
	case GL_RGB32I:
		*out_format = GL_RGB_INTEGER;
		break;

	case GL_RGBA:
	case GL_RGBA8:
	case GL_SRGB8_ALPHA8:
	case GL_RGBA8_SNORM:
	case GL_RGB5_A1:
	case GL_RGBA4:
	case GL_RGB10_A2:
	case GL_RGBA16F:
	case GL_RGBA32F:
		*out_format = GL_RGBA;
		break;

	case GL_RGBA8UI:
	case GL_RGBA8I:
	case GL_RGB10_A2UI:
	case GL_RGBA16UI:
	case GL_RGBA16I:
	case GL_RGBA32I:
	case GL_RGBA32UI:
		*out_format = GL_RGBA_INTEGER;
		break;

	case GL_DEPTH_COMPONENT16:
	case GL_DEPTH_COMPONENT24:
	case GL_DEPTH_COMPONENT32F:
		*out_format = GL_DEPTH_COMPONENT;
		break;

	case GL_DEPTH24_STENCIL8:
	case GL_DEPTH32F_STENCIL8:
		*out_format = GL_DEPTH_STENCIL;
		break;

	default:
		TCU_FAIL("Internalformat not recognized");
		return false;
	} // switch (internalformat)

	return true;
}

/** Retrieves FBO effective internal format at user-specified index.
 *
 *  Pays extra care not to reach outside of fbo_effective_internal_format_ordering array.
 *
 *  @param index Index to look up the internal format at.
 *
 *  @return Requested information or GL_NONE if failed or 0xFFFFFFFF if index is
 *		  outside allowed range.
 **/
GLenum TestBase::getFBOEffectiveInternalFormatAtIndex(unsigned int index) const
{
	const unsigned int n_effective_internalformats = DE_LENGTH_OF_ARRAY(fboEffectiveInternalFormatOrdering);

	DE_ASSERT(index < n_effective_internalformats);
	if (index < n_effective_internalformats)
		return fboEffectiveInternalFormatOrdering[index];

	// Return glitch
	m_testCtx.getLog() << tcu::TestLog::Message
					   << "GetFBOEffectiveInternalFormatAtIndex - Invalid index requested: " << index
					   << tcu::TestLog::EndMessage;

	return static_cast<GLenum>(0xFFFFFFFF);
}

/** Retrieves glCopyTexImage2D() internal format at user-specified index.
 *
 *  Pays extra care not to reach outside of copy_tex_image_2d_internal_format_orderingarray.
 *
 *  @param index Index to look up the internal format at.
 *
 *  @return Requested information or GL_NONE if failed or 0xFFFFFFFF if index is outside
 *		  allowed range.
 **/
GLenum TestBase::getCopyTexImage2DInternalFormatAtIndex(unsigned int index) const
{
	const unsigned int n_internalformats = DE_LENGTH_OF_ARRAY(copyTexImage2DInternalFormatOrdering);

	DE_ASSERT(index < n_internalformats);
	if (index < n_internalformats)
		return copyTexImage2DInternalFormatOrdering[index];

	// Return glitch
	m_testCtx.getLog() << tcu::TestLog::Message
					   << "GetCopyTexImage2DInternalFormatAtIndex - Invalid index requested: " << index
					   << tcu::TestLog::EndMessage;

	return static_cast<GLenum>(0xFFFFFFFF);
}

/** Retrieves a string representing name of target passed by argument.
 *
 *  @param target GLES target to retrieve a string for.
 *
 *  @return A relevant string or "?" (without double quotation marks)
 *		  if type is unrecognized.
 **/
const char* TestBase::getTargetName(GLenum target) const
{
	const char* result = "?";

	switch (target)
	{
	case GL_RENDERBUFFER:
		result = "GL_RENDERBUFFER";
		break;
	case GL_TEXTURE_2D:
		result = "GL_TEXTURE_2D";
		break;
	case GL_TEXTURE_2D_ARRAY:
		result = "GL_TEXTURE_2D_ARRAY";
		break;
	case GL_TEXTURE_3D:
		result = "GL_TEXTURE_3D";
		break;
	case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
		result = "GL_TEXTURE_CUBE_MAP_NEGATIVE_X";
		break;
	case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
		result = "GL_TEXTURE_CUBE_MAP_NEGATIVE_Y";
		break;
	case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
		result = "GL_TEXTURE_CUBE_MAP_NEGATIVE_Z";
		break;
	case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
		result = "GL_TEXTURE_CUBE_MAP_POSITIVE_X";
		break;
	case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
		result = "GL_TEXTURE_CUBE_MAP_POSITIVE_Y";
		break;
	case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
		result = "GL_TEXTURE_CUBE_MAP_POSITIVE_Z";
		break;
	}

	return result;
}

/** Returns a general texture target for cube-map texture targets or
 *  user-specified target otherwise.
 *
 *  @param target GLES target to consider. Allowed values:
 *				1)  GL_RENDERBUFFER,
 *				2)  GL_TEXTURE_2D,
 *				3)  GL_TEXTURE_2D_ARRAY,
 *				4)  GL_TEXTURE_3D,
 *				5)  GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
 *				6)  GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
 *				7)  GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
 *				8)  GL_TEXTURE_CUBE_MAP_POSITIVE_X,
 *				9)  GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
 *				10) GL_TEXTURE_CUBE_MAP_POSITIVE_Z.
 *
 *  @return General texture target or used-specified target
 *		  if successful, GL_NONE otherwise.
 */
GLenum TestBase::getGeneralTargetForDetailedTarget(GLenum target)
{
	GLenum result = GL_NONE;

	switch (target)
	{
	case GL_RENDERBUFFER:
	case GL_TEXTURE_2D:
	case GL_TEXTURE_2D_ARRAY:
	case GL_TEXTURE_3D:
	{
		result = target;

		break;
	} // renderbuffer & 2D/3D texture targets

	case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
	case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
	case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
	case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
	case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
	case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
	{
		result = GL_TEXTURE_CUBE_MAP;

		break;
	} // cube-map texture targets

	default:
	{
		TCU_FAIL("Unrecognized target");
	}
	}

	return result;
}

/** Generates a GL object of an user-requested type.
 *
 *  NOTE: It is expected no error is reported by OpenGL ES prior to
 *		the call.
 *
 *  @param object_type Type of a GL object to create. Allowed values:
 *					 1)  GL_RENDERBUFFER,
 *					 2)  GL_TEXTURE_2D,
 *					 3)  GL_TEXTURE_2D_ARRAY,
 *					 4)  GL_TEXTURE_3D,
 *					 5)  GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
 *					 6)  GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
 *					 7)  GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
 *					 8)  GL_TEXTURE_CUBE_MAP_POSITIVE_X,
 *					 9)  GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
 *					 10) GL_TEXTURE_CUBE_MAP_POSITIVE_Z.
 *
 *  @return GLES ID (different than zero) of the created object if
 *		  successful, zero otherwise.
 */
GLuint TestBase::generateGLObject(GLenum object_type)
{
	const Functions& gl			= m_context.getRenderContext().getFunctions();
	GLenum			 error_code = GL_NO_ERROR;
	GLuint			 result		= 0;

	switch (object_type)
	{
	case GL_RENDERBUFFER:
	{
		gl.genRenderbuffers(1, &result);
		break;
	}

	case GL_TEXTURE_2D:
	case GL_TEXTURE_2D_ARRAY:
	case GL_TEXTURE_3D:
	case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
	case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
	case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
	case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
	case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
	case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
	{
		gl.genTextures(1, &result);
		break;
	}

	default:
		TCU_FAIL("Unsupported source attachment type");
	}

	// check if all is good with our new object
	error_code = gl.getError();

	if (error_code != GL_NO_ERROR)
	{
		m_testCtx.getLog() << tcu::TestLog::Message
						   << "Could not generate a renderbuffer OR texture object. GL reported error: [" << error_code
						   << "]" << tcu::TestLog::EndMessage;
		return 0;
	}

	return result;
}

/** Sets up a GL object and binds it to either GL_DRAW_FRAMEBUFFER
 *  (if @param is_source_gl_object is 0) or GL_READ_FRAMEBUFFER zeroth
 *  color attachment.
 *
 *  NOTE: The function assumes the object at @param object_id of @param
 *		object_target type has already been generated!
 *
 *  @param is_source_gl_object 1 if the object should be bound to
 *							 GL_DRAW_FRAMEBUFFER target once configured,
 *							 0 to bound the object to GL_READ_FRAMEBUFFER
 *							 target instead.
 *  @param object_target	   Type of the object to configure. Allowed values:
 *							 1)  GL_RENDERBUFFER,
 *							 2)  GL_TEXTURE_2D,
 *							 3)  GL_TEXTURE_2D_ARRAY,
 *							 4)  GL_TEXTURE_3D,
 *							 5)  GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
 *							 6)  GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
 *							 7)  GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
 *							 8)  GL_TEXTURE_CUBE_MAP_POSITIVE_X,
 *							 9)  GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
 *							 10) GL_TEXTURE_CUBE_MAP_POSITIVE_Z.
 *  @param object_id		   GLES ID of the object to configure.
 *  @param internal_format	 Internal-format of the data under @param data.
 *  @param format			  Format of the data under @param data.
 *  @param type				Type the data @param data is represented with.
 *  @param data				Buffer with the data to fill the object with.
 *							 Cannot be NULL.
 *
 *  @return true if successful, false otherwise.,
 **/
bool TestBase::configureGLObject(int is_source_gl_object, GLenum object_target, GLint object_id, GLenum internal_format,
								 GLenum format, GLenum type, void* data)
{
	const Functions& gl			= m_context.getRenderContext().getFunctions();
	GLenum			 fbo_target = (is_source_gl_object == 0) ? GL_DRAW_FRAMEBUFFER : GL_READ_FRAMEBUFFER;
	bool			 result		= true;

	// Special case for GL_HALF_FLOAT -> input data is in GL_FLOAT
	if (type == GL_HALF_FLOAT)
		type = GL_FLOAT;

	switch (object_target)
	{
	case GL_RENDERBUFFER:
	{
		GLint  current_draw_fbo_id   = 0;
		GLint  current_read_fbo_id   = 0;
		GLuint temporary_draw_fbo_id = 0;
		GLuint temporary_read_fbo_id = 0;
		GLuint temporary_to_id		 = 0;

		// Retrieve current draw/read fbo bindings
		gl.getIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &current_draw_fbo_id);
		gl.getIntegerv(GL_READ_FRAMEBUFFER_BINDING, &current_read_fbo_id);

		// Set up the RBO */
		gl.bindRenderbuffer(GL_RENDERBUFFER, object_id);
		gl.renderbufferStorage(GL_RENDERBUFFER, internal_format, TEXTURE_WIDTH, TEXTURE_HEIGHT);

		// Generate a temporary 2D texture object and copy the data into it
		gl.genTextures(1, &temporary_to_id);
		gl.bindTexture(GL_TEXTURE_2D, temporary_to_id);
		gl.texImage2D(GL_TEXTURE_2D, 0 /* level */, internal_format, TEXTURE_WIDTH, TEXTURE_HEIGHT, 0 /* border */,
					  format, type, data);

		// Set up a temporary read FBO with the texture object attached to zeroth color attachment..
		gl.genFramebuffers(1, &temporary_read_fbo_id);
		gl.bindFramebuffer(GL_READ_FRAMEBUFFER, temporary_read_fbo_id);
		gl.framebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, temporary_to_id,
								0 /* level */);

		// and another one we'll bind to draw framebuffer target with the renderbuffer object attached
		gl.genFramebuffers(1, &temporary_draw_fbo_id);
		gl.bindFramebuffer(GL_DRAW_FRAMEBUFFER, temporary_draw_fbo_id);
		gl.framebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, object_id);

		// Blit the texture contents into the renderbuffer.
		gl.blitFramebuffer(0 /* srcX0 */, 0 /* srcY0 */, TEXTURE_WIDTH, TEXTURE_HEIGHT, 0 /* dstX0 */, 0 /* dstY0 */,
						   TEXTURE_WIDTH, TEXTURE_HEIGHT, GL_COLOR_BUFFER_BIT, GL_NEAREST);

		// Restore pre-call configuration
		gl.bindFramebuffer(GL_DRAW_FRAMEBUFFER, current_draw_fbo_id);
		gl.bindFramebuffer(GL_READ_FRAMEBUFFER, current_read_fbo_id);

		// Get rid of the temporary objects
		gl.bindTexture(GL_TEXTURE_2D, 0);
		gl.deleteTextures(1, &temporary_to_id);
		gl.deleteFramebuffers(1, &temporary_draw_fbo_id);
		gl.deleteFramebuffers(1, &temporary_read_fbo_id);

		// Update the pre-call framebuffer's attachment configuration
		gl.framebufferRenderbuffer(fbo_target, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, object_id);
		break;
	}

	case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
	case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
	case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
	case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
	case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
	case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
	{
		const GLenum cm_targets[] = { GL_TEXTURE_CUBE_MAP_NEGATIVE_X, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
									  GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, GL_TEXTURE_CUBE_MAP_POSITIVE_X,
									  GL_TEXTURE_CUBE_MAP_POSITIVE_Y, GL_TEXTURE_CUBE_MAP_POSITIVE_Z };
		GLenum		 general_target = getGeneralTargetForDetailedTarget(object_target);
		unsigned int n_cm_target	= 0;

		// Set up base mipmap for our source texture.
		gl.bindTexture(general_target, object_id);

		// Set up *all* faces of a cube-map (as per Bugzilla #9689 & #9807),
		// so that the CM texture is cube complete.
		for (n_cm_target = 0; n_cm_target < sizeof(cm_targets) / sizeof(cm_targets[0]); ++n_cm_target)
		{
			gl.texImage2D(cm_targets[n_cm_target], 0 /* level */, internal_format, TEXTURE_WIDTH, TEXTURE_HEIGHT,
						  0 /* border */, format, type, data);
		}

		gl.texParameterf(general_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		gl.texParameterf(general_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		gl.texParameterf(general_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		gl.texParameterf(general_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		// Set up the FBO attachment
		if (is_source_gl_object)
			gl.framebufferTexture2D(fbo_target, GL_COLOR_ATTACHMENT0, object_target, object_id, 0);

		gl.bindTexture(general_target, 0);
		break;
	}

	case GL_TEXTURE_2D:
	{
		// Set up base mipmap for our source texture.
		gl.bindTexture(object_target, object_id);
		gl.texImage2D(object_target, 0 /* level */, internal_format, TEXTURE_WIDTH, TEXTURE_HEIGHT, 0 /* border */,
					  format, type, data);

		gl.texParameterf(object_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		gl.texParameterf(object_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		gl.texParameterf(object_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		gl.texParameterf(object_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		// Set up the FBO attachment
		if (is_source_gl_object)
			gl.framebufferTexture2D(fbo_target, GL_COLOR_ATTACHMENT0, object_target, object_id, 0);

		gl.bindTexture(object_target, 0);
		break;
	}

	case GL_TEXTURE_2D_ARRAY:
	case GL_TEXTURE_3D:
	{
		// Set up base mipmap for our source texture.
		gl.bindTexture(object_target, object_id);
		gl.texImage3D(object_target, 0 /* level */, internal_format, TEXTURE_WIDTH, TEXTURE_HEIGHT, TEXTURE_DEPTH,
					  0 /* border */, format, type, NULL);
		gl.texSubImage3D(object_target, 0 /* level */, 0 /* xoffset */, 0 /* yoffset */, 1 /* zoffset */, TEXTURE_WIDTH,
						 TEXTURE_HEIGHT, 1 /* depth */, format, type, data);

		gl.texParameterf(object_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		gl.texParameterf(object_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		gl.texParameterf(object_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		gl.texParameterf(object_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		gl.texParameterf(object_target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		DE_ASSERT(is_source_gl_object);

		// Set up the FBO attachment. Make sure there is an actual difference from gl.framebufferTexture2D()
		// and use the second layer of the texture.
		gl.framebufferTextureLayer(fbo_target, GL_COLOR_ATTACHMENT0, object_id, 0 /* level */, 1 /* layer */);

		gl.bindTexture(object_target, 0);
		break;
	}

	default:
	{
		// ASSERTION FAILURE: unsupported source attachment type
		DE_ASSERT(0);
		result = false;
	}
	} /* switch (source_attachment_type) */

	if (result)
	{
		GLenum error_code = gl.getError();

		if (error_code != GL_NO_ERROR)
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "Could not set up a GL object ["
							   << (is_source_gl_object ? "source" : "destination") << "] of format ["
							   << getInternalformatString(internal_format) << "] to be used as "
							   << getTargetName(object_target) << " attachment for the test. GL reported error ["
							   << error_code << "]";
			return false;
		}
	}

	return result;
}

/** Releases a GL object. If @param target represents a texture,
 *  the object is unbound from the target prior to a gl.deleteTextures()
 *  call.
 *
 *  @param target	Type of the object to release. Allowed values:
 *				   1)  GL_RENDERBUFFER,
 *				   2)  GL_TEXTURE_2D,
 *				   3)  GL_TEXTURE_2D_ARRAY,
 *				   4)  GL_TEXTURE_3D,
 *				   5)  GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
 *				   6)  GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
 *				   7)  GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
 *				   8)  GL_TEXTURE_CUBE_MAP_POSITIVE_X,
 *				   9)  GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
 *				   10) GL_TEXTURE_CUBE_MAP_POSITIVE_Z.
 *
 *  @param object_id GLES ID of the object to release.
 */
void TestBase::destroyGLObject(GLenum target, GLuint object_id)
{
	const Functions& gl = m_context.getRenderContext().getFunctions();
	switch (target)
	{
	case GL_RENDERBUFFER:
	{
		gl.bindRenderbuffer(GL_RENDERBUFFER, 0);
		gl.deleteRenderbuffers(1, &object_id);
		break;
	}

	case GL_TEXTURE_2D:
	case GL_TEXTURE_2D_ARRAY:
	case GL_TEXTURE_3D:
	case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
	case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
	case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
	case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
	case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
	case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
	{
		GLenum general_attachment_type = getGeneralTargetForDetailedTarget(target);
		gl.bindTexture(general_attachment_type, 0);
		gl.deleteTextures(1, &object_id);
		break;
	}

	default:
	{
		TCU_FAIL("Unsupported attachment type.");
	}
	}
}

/** Tells whether @param internalformat can be used for
 *  a gl.renderbufferStorage*() call.
 *
 *  @param internalformat Internalformat to consider.
 *
 *  @return true if the internal format can be used for the call,
 *		  false otherwise.
 **/
bool TestBase::isValidRBOInternalFormat(GLenum internalformat) const
{
	// Internal format can be used for gl.renderbufferStorage()
	// call if it's either color-, depth- or stencil-renderable.
	return isColorRenderableInternalFormat(internalformat) || isDepthRenderableInternalFormat(internalformat) ||
		   isDepthStencilRenderableInternalFormat(internalformat);
}

/** Tells whether internal format @param internalformat is color-renderable.
 *
 *  @param internalformat GLES internal format to consider.
 *
 *  @return true if @param internalformat is color-renderable, false otherwise
 **/
bool TestBase::isColorRenderableInternalFormat(GLenum internalformat) const
{
	const glu::ContextInfo& contextInfo = m_context.getContextInfo();

	bool supports_fp_textures	  = contextInfo.isExtensionSupported("GL_EXT_color_buffer_float");
	bool supports_half_fp_textures = contextInfo.isExtensionSupported("GL_EXT_color_buffer_half_float");

	// Floating-point textures are only supported if
	// implementation supports GL_EXT_color_buffer_float extension
	if (!supports_fp_textures && (internalformat == GL_R32F || internalformat == GL_RG32F ||
								  internalformat == GL_RGB32F || internalformat == GL_RGBA32F))
	{
		return false;
	}

	// Half floating-point textures are only supported if
	// implementation supports GL_EXT_color_buffer_half_float extension
	if (!supports_half_fp_textures && (internalformat == GL_R16F || internalformat == GL_RG16F ||
									   internalformat == GL_RGB16F || internalformat == GL_RGBA16F))
	{
		return false;
	}

	switch (internalformat)
	{
	case GL_RGB:
	case GL_RGBA:
	case GL_R8:
	case GL_RG8:
	case GL_RGB8:
	case GL_RGB565:
	case GL_RGBA4:
	case GL_RGB5_A1:
	case GL_RGBA8:
	case GL_RGB10_A2:
	case GL_RGB10_A2UI:
	case GL_SRGB8_ALPHA8:
	case GL_R8I:
	case GL_R8UI:
	case GL_R16I:
	case GL_R16UI:
	case GL_R32I:
	case GL_R32UI:
	case GL_RG8I:
	case GL_RG8UI:
	case GL_RG16I:
	case GL_RG16UI:
	case GL_RG32I:
	case GL_RG32UI:
	case GL_RGBA8I:
	case GL_RGBA8UI:
	case GL_RGBA16I:
	case GL_RGBA16UI:
	case GL_RGBA32I:
	case GL_RGBA32UI:
		// GLES3.0 color-renderable internalformats
		return true;

	case GL_R16F:
	case GL_R32F:
	case GL_RG16F:
	case GL_RG32F:
	case GL_RGB16F:
	// GL_RGB32F not supported
	case GL_RGBA16F:
	case GL_RGBA32F:
		// Since we passed the above checks, we can assume
		// the internalformats are color-renderable
		return true;

	default:
		return false;
	}

	return false;
}

/** Tells whether internal format @param internalformat is depth-renderable.
 *
 *  @param internalformat GLES internal format to consider.
 *
 *  @return true if @param internalformat is depth-renderable, false otherwise
 **/
bool TestBase::isDepthRenderableInternalFormat(GLenum internalformat) const
{
	switch (internalformat)
	{
	case GL_DEPTH_COMPONENT16:
	case GL_DEPTH_COMPONENT24:
	case GL_DEPTH_COMPONENT32F:
		return true;
	}

	return false;
}

/** Tells whether internal format @param internalformat is depth+stencil-renderable.
 *
 *  @param internalformat GLES internal format to consider.
 *
 *  @return true if @param internalformat is depth+stencil-renderable, false otherwise
 **/
bool TestBase::isDepthStencilRenderableInternalFormat(GLenum internalformat) const
{
	switch (internalformat)
	{
	case GL_DEPTH24_STENCIL8:
	case GL_DEPTH32F_STENCIL8:
		return true;
	}

	return false;
}

/** Tells whether OpenGL ES 3.0 implementations should accept copying texture image data from
 *  a read buffer using @param src_internalformat internalformat-based storage to a texture object
 *  using an internal format @param dst_internalformat.
 *
 *  @param src_internalformat Internal format to be used for source object's data storage.
 *  @param dst_internalformat Internal format to be used for destination texture object's data storage.
 *
 *  @return true if the operation is expected to execute successfully, false otherwise.
 */
bool TestBase::isFBOEffectiveInternalFormatCompatibleWithDestinationInternalFormat(GLenum src_internalformat,
																				   GLenum dst_internalformat) const
{
	const unsigned int n_copyteximage_internalformats = DE_LENGTH_OF_ARRAY(copyTexImage2DInternalFormatOrdering);
	unsigned int	   n_dst_internalformat			  = 0;
	const unsigned int n_effective_internalformats	= DE_LENGTH_OF_ARRAY(fboEffectiveInternalFormatOrdering);
	unsigned int	   n_src_internalformat			  = 0;
	bool			   result						  = false;

	// Find out which index does the source internalformat use
	while (n_src_internalformat < n_effective_internalformats)
	{
		GLenum internalformat_at_n = getFBOEffectiveInternalFormatAtIndex(n_src_internalformat);

		if (internalformat_at_n == src_internalformat)
			break;
		else
			++n_src_internalformat;
	}

	DE_ASSERT(n_src_internalformat != n_effective_internalformats);
	if (n_src_internalformat == n_effective_internalformats)
		return false;

	// Find out which index does the target internalformat use
	while (n_dst_internalformat < n_copyteximage_internalformats)
	{
		GLenum internalformat_at_n = getCopyTexImage2DInternalFormatAtIndex(n_dst_internalformat);

		if (internalformat_at_n == dst_internalformat)
			break;
		else
			++n_dst_internalformat;
	}

	DE_ASSERT(n_dst_internalformat != n_copyteximage_internalformats);
	if (n_dst_internalformat == n_copyteximage_internalformats)
		return false;

	// Find out if the conversion is allowed
	unsigned int conversion_array_index = n_copyteximage_internalformats * n_src_internalformat + n_dst_internalformat;

	DE_ASSERT(conversion_array_index < (sizeof(conversionArray) / sizeof(GLenum)));
	if (conversion_array_index < (sizeof(conversionArray) / sizeof(GLenum)))
		result = (conversionArray[conversion_array_index] != GL_NONE);

	return result;
}

/** Retrieves a string representing name of internal format passed by argument.
 *
 *  @param internalformat GLES internal format to retrieve a string for.
 *
 *  @return A relevant string or "?" (without double quotation marks)
 *          if type is unrecognized.
 **/
const char* TestBase::getInternalformatString(GLenum internalformat)
{
	switch (internalformat)
	{
	case GL_ALPHA:
		return "GL_ALPHA";
	case GL_ALPHA8_OES:
		return "GL_ALPHA8";
	case GL_LUMINANCE:
		return "GL_LUMINANCE";
	case GL_LUMINANCE8_OES:
		return "GL_LUMINANCE8";
	case GL_LUMINANCE8_ALPHA8_OES:
		return "GL_LUMINANCE8_ALPHA8";
	case GL_LUMINANCE_ALPHA:
		return "GL_LUMINANCE_ALPHA";
	case GL_R11F_G11F_B10F:
		return "GL_R11F_G11F_B10F";
	case GL_R16F:
		return "GL_R16F";
	case GL_R16I:
		return "GL_R16I";
	case GL_R16UI:
		return "GL_R16UI";
	case GL_R32F:
		return "GL_R32F";
	case GL_R32I:
		return "GL_R32I";
	case GL_R32UI:
		return "GL_R32UI";
	case GL_R8:
		return "GL_R8";
	case GL_R8I:
		return "GL_R8I";
	case GL_R8UI:
		return "GL_R8UI";
	case GL_R8_SNORM:
		return "GL_R8_SNORM";
	case GL_RG16F:
		return "GL_RG16F";
	case GL_RG16I:
		return "GL_RG16I";
	case GL_RG16UI:
		return "GL_RG16UI";
	case GL_RG32F:
		return "GL_RG32F";
	case GL_RG32I:
		return "GL_RG32I";
	case GL_RG32UI:
		return "GL_RG32UI";
	case GL_RG8:
		return "GL_RG8";
	case GL_RG8I:
		return "GL_RG8I";
	case GL_RG8UI:
		return "GL_RG8UI";
	case GL_RG8_SNORM:
		return "GL_RG8_SNORM";
	case GL_RGB:
		return "GL_RGB";
	case GL_RGB10_A2:
		return "GL_RGB10_A2";
	case GL_RGB10_A2UI:
		return "GL_RGB10_A2UI";
	case GL_RGB16F:
		return "GL_RGB16F";
	case GL_RGB16I:
		return "GL_RGB16I";
	case GL_RGB16UI:
		return "GL_RGB16UI";
	case GL_RGB32F:
		return "GL_RGB32F";
	case GL_RGB32I:
		return "GL_RGB32I";
	case GL_RGB32UI:
		return "GL_RGB32UI";
	case GL_RGB5_A1:
		return "GL_RGB5_A1";
	case GL_RGB8:
		return "GL_RGB8";
	case GL_RGB8I:
		return "GL_RGB8I";
	case GL_RGB8UI:
		return "GL_RGB8UI";
	case GL_RGB8_SNORM:
		return "GL_RGB8_SNORM";
	case GL_RGB9_E5:
		return "GL_RGB9_E5";
	case GL_RGBA:
		return "GL_RGBA";
	case GL_RGBA16I:
		return "GL_RGBA16I";
	case GL_RGBA16UI:
		return "GL_RGBA16UI";
	case GL_RGBA4:
		return "GL_RGBA4";
	case GL_RGBA32I:
		return "GL_RGBA32I";
	case GL_RGBA32UI:
		return "GL_RGBA32UI";
	case GL_RGBA8I:
		return "GL_RGBA8I";
	case GL_RGBA8UI:
		return "GL_RGBA8UI";
	case GL_RGB565:
		return "GL_RGB565";
	case GL_RGBA16F:
		return "GL_RGBA16F";
	case GL_RGBA32F:
		return "GL_RGBA32F";
	case GL_RGBA8:
		return "GL_RGBA8";
	case GL_RGBA8_SNORM:
		return "GL_RGBA8_SNORM";
	case GL_SRGB8:
		return "GL_SRGB8";
	case GL_SRGB8_ALPHA8:
		return "GL_SRGB8_ALPHA8";
	}

	return "GL_NONE";
}

/* SPECIFICATION:
 *
 * This conformance test verifies that glCopyTexImage2D() implementation accepts
 * internalformats that are compatible with effective internalformat of current
 * read buffer.
 *
 * The test starts from creating two framebuffer objects, that it accordingly binds
 * to GL_DRAW_FRAMEBUFFER and GL_READ_FRAMEBUFFER targets. It then enters two-level loop:
 *
 * a) First level determines source attachment type: this could either be a 2D texture/cube-map
 *	face mip-map, a specific mip-map of a slice coming from a 2D texture array OR a 3D texture,
 *	or finally a render-buffer. All of these can be bound to an attachment point that is
 *	later pointed to by read buffer configuration.
 * b) Second level configures attachment type of destination. Since glCopyTexImage2D()
 *	specification limits accepted targets, only 2D texture or cube-map face targets are
 *	accepted.
 *
 * For each viable source/destination configuration, the test then enters another two-level loop:
 *
 * I)  First sub-level determines what internal format should be used for the source attachment.
 *	 All texture formats required from a conformant GLES3.0 implementation are iterated over.
 * II) Second sub-level determines internal format that should be passed as a parameter to
 *	 a glCopyTexImage2D() call.
 *
 * For each internal format pair, the test creates and configures a corresponding GL object and
 * attaches it to the read framebuffer. The test also uses a pre-generated texture object ID that
 * will be re-configured with each glCopyTexImage2D() call.
 *
 * Source data is a 2x2 array consisting of up to 4 channels with different values, represented
 * in an iteration-specific format and type. For more details, please see implementation of
 * ConfigureConversionDatabase() entry-point.
 *
 * The test then loops over all supported format+type combinations for the internal-format considered
 * and feeds them into actual glCopyTexImage2D() call. It is against the specification for the call
 * to fail at this point. Should this be the case, the test is considered to fail but will continue
 * iterating over all the loops to make sure all problems are reported within a single run.
 *
 * Once the call is determined to have finished successfully, the test attempts to read the result data.
 * This needs to be handled in two ways:
 *
 * - if internalformat is color-renderable, we can attach the result texture to the read framebuffer object
 *   and do a glReadPixels() call. For some combinations of internalformat and attachment types the implementations
 *   are allowed to report unsupported framebuffer configuration, in which case the test will proceed with testing
 *   remaining source/destination/internalformat combinations and will not consider this an error.
 * - if internalformat is not color-renderable, we need to bind the result texture to a texture unit and
 *   use a program object to determine whether the data made available are valid. THIS CASE IS NOT IMPLEMENTED
 *   YET!
 *
 * Once the data are downloaded, they are compared against reference texture data. Should the rendered output
 * diverge outside the allowed epsilon, the test will report an error but will continue iterating to make sure
 * all source/destination/internalformat combinations are covered.
 */
class RequiredCase : public TestBase
{
public:
	RequiredCase(deqp::Context& context, de::SharedPtr<ConversionDatabase> database, GLenum sourceAttachmentTypes,
				 GLenum destinationAttachmentTypes);
	virtual ~RequiredCase();

	void						 deinit(void);
	tcu::TestNode::IterateResult iterate(void);

protected:
	bool execute(GLenum src_internalformat, GLenum dst_internalformat,
				 NonRenderableInternalformatSupportObjects* objects_ptr);
	bool bindTextureToTargetToSpecificTextureUnit(GLuint to_id, GLenum texture_target, GLenum texture_unit);
	bool setUniformValues(GLint source_2D_texture_uniform_location, GLenum source_2D_texture_unit,
						  GLint source_2DArray_texture_uniform_location, GLenum source_2DArray_texture_unit,
						  GLint source_3D_texture_uniform_location, GLenum source_3D_texture_unit,
						  GLint source_Cube_texture_uniform_location, GLenum source_Cube_texture_unit,
						  GLint destination_2D_texture_uniform_location, GLenum destination_2D_texture_unit,
						  GLint destination_Cube_texture_uniform_location, GLenum destination_Cube_texture_unit,
						  GLint channels_to_compare_uniform_location, GLint channels_to_compare,
						  GLint samplers_to_use_uniform_location, GLint samplers_to_use);
	bool copyDataFromBufferObject(GLuint bo_id, std::vector<GLint>& retrieved_data);
	bool findEntryInConversionDatabase(unsigned int index, GLenum src_internalformat, GLenum src_type,
									   GLenum copyteximage2d_internalformat, GLenum* out_result_internalformat,
									   GLenum* out_dst_type, PixelData* out_src_topleft, PixelData* out_src_topright,
									   PixelData* out_src_bottomleft, PixelData* out_src_bottomright,
									   PixelData* out_dst_topleft, PixelData* out_dst_topright,
									   PixelData* out_dst_bottomleft, PixelData* out_dst_bottomright,
									   PixelCompareChannel* out_channels_to_compare);
	int getIndexOfCopyTexImage2DInternalFormat(GLenum internalformat);
	int getIndexOfFramebufferEffectiveInternalFormat(GLenum internalformat);
	bool compareExpectedResultsByReadingPixels(PixelData source_tl_pixel_data, PixelData source_tr_pixel_data,
											   PixelData source_bl_pixel_data, PixelData source_br_pixel_data,
											   PixelData reference_tl_pixel_data, PixelData reference_tr_pixel_data,
											   PixelData reference_bl_pixel_data, PixelData reference_br_pixel_data,
											   GLenum read_type, GLenum result_internalformat);
	unsigned int getSizeOfPixel(GLenum format, GLenum type);
	bool getPixelDataFromRawData(void* raw_data, GLenum raw_data_format, GLenum raw_data_type, PixelData* out_result);
	bool comparePixelData(PixelData downloaded_pixel, PixelData reference_pixel, PixelData source_pixel,
						  GLenum result_internalformat, bool has_test_failed_already);
	bool getNumberOfBitsForInternalFormat(GLenum internalformat, int* out_rgba_bits);

	bool getRawDataFromPixelData(std::vector<char>& result, PixelData topleft, PixelData topright, PixelData bottomleft,
								 PixelData bottomright);
	bool getNumberOfBitsForChannelDataType(ChannelDataType channel_data_type, int* out_n_bits);

	bool getChannelOrderForInternalformatAndType(GLenum internalformat, GLenum type, ChannelOrder* out_channel_order);
	bool generateObjectsToSupportNonColorRenderableInternalformats();
	bool prepareSupportForNonRenderableTexture(NonRenderableInternalformatSupportObjects& objects,
											   DataSamplerType							  src_texture_sampler_type,
											   DataSamplerType dst_texture_sampler_type, GLenum source_attachment_type,
											   GLenum destination_attachment_type);
	bool calculateBufferDataSize(DataSamplerType sampler_type, GLuint* buffer_data_size_ptr);
	const float* getTexCoordinates(GLenum attachment_type) const;
	bool prepareProgramAndShaderObjectsToSupportNonRenderableTexture(GLuint			 program_object_id,
																	 GLuint			 fragment_shader_object_id,
																	 GLuint			 vertex_shader_object_id,
																	 DataSamplerType src_texture_sampler_type,
																	 DataSamplerType dst_texture_sampler_type);
	bool setSourceForShaderObjectsUsedForNonRenderableTextureSupport(GLuint			 fragment_shader_object_id,
																	 GLuint			 vertex_shader_object_id,
																	 DataSamplerType src_texture_sampler_type,
																	 DataSamplerType dst_texture_sampler_type);
	bool compileAndCheckShaderCompilationStatus(GLuint shader_object_id);
	bool linkAndCheckProgramLinkStatus(GLuint program_object_id);
	bool getUniformLocations(GLuint program_object_id, GLint* source_2D_texture_uniform_location_ptr,
							 GLint* source_2DArray_texture_uniform_location_ptr,
							 GLint* source_3D_texture_uniform_location_ptr,
							 GLint* source_Cube_texture_uniform_location_ptr,
							 GLint* destination_2D_texture_uniform_location_ptr,
							 GLint* destination_Cube_texture_uniform_location_ptr,
							 GLint* channels_to_compare_uniform_location_ptr,
							 GLint* samplers_to_use_uniform_location_ptr);
	void displayPixelComparisonFailureMessage(GLint source_pixel_r, GLint source_pixel_g, GLint source_pixel_b,
											  GLint source_pixel_a, GLenum source_internalformat, GLenum source_type,
											  GLint reference_pixel_r, GLint reference_pixel_g, GLint reference_pixel_b,
											  GLint reference_pixel_a, GLenum reference_internalformat,
											  GLenum reference_type, GLint result_pixel_r, GLint result_pixel_g,
											  GLint result_pixel_b, GLint result_pixel_a, GLenum result_internalformat,
											  GLenum result_type, GLint max_epsilon_r, GLint max_epsilon_g,
											  GLint max_epsilon_b, GLint max_epsilon_a);
	DataSamplerType getDataSamplerTypeForInternalformat(GLenum internalformat);
	bool isInternalFormatCompatibleWithFPSampler(GLenum internalformat);
	bool isInternalFormatCompatibleWithIntegerSampler(GLenum internalformat);
	bool isInternalFormatCompatibleWithUnsignedIntegerSampler(GLenum internalformat);
	void destroyObjectsSupportingNonRenderableInternalformats(NonRenderableInternalformatSupportObjects& objects);
	void unbindAndDestroyBufferObject(GLuint bo_id);
	void destroyTransformFeedbackObject(GLuint transform_feedback_object_id);
	void destroyProgramAndShaderObjects(GLuint program_object_id, GLuint fragment_shader_id, GLuint vertex_shader_id);
	void unbindColorAttachments();
	void restoreBindings(GLenum src_attachment_point, GLenum dst_attachment_point, GLint bound_draw_fbo_id,
						 GLint bound_read_fbo_id);

private:
	GLuint m_dst_object_id;
	GLuint m_src_object_id;

	de::SharedPtr<ConversionDatabase> m_conversion_database;

	// Some of the internalformats considered during the test are not renderable, meaning
	// we cannot use glReadPixels() to retrieve their contents.
	// Instead, a special program object needs to be used to perform the verification in
	// actual shader.
	// We create a program object for possible each float/int/uint->float/int/uint combination.
	// All objects created during the process are stored in a dedicated
	// _non_renderable_internalformat_support_objects instance and released once the test ends.
	NonRenderableInternalformatSupportObjects m_f_src_f_dst_internalformat;
	NonRenderableInternalformatSupportObjects m_i_src_i_dst_internalformat;
	NonRenderableInternalformatSupportObjects m_ui_src_ui_dst_internalformat;
};

RequiredCase::RequiredCase(deqp::Context& context, de::SharedPtr<ConversionDatabase> database,
						   GLenum sourceAttachmentTypes, GLenum destinationAttachmentTypes)
	: TestBase(context, sourceAttachmentTypes, destinationAttachmentTypes)
	, m_dst_object_id(0)
	, m_src_object_id(0)
	, m_conversion_database(database)
{
	deMemset(&m_f_src_f_dst_internalformat, 0, sizeof(m_f_src_f_dst_internalformat));
	deMemset(&m_i_src_i_dst_internalformat, 0, sizeof(m_i_src_i_dst_internalformat));
	deMemset(&m_ui_src_ui_dst_internalformat, 0, sizeof(m_ui_src_ui_dst_internalformat));
}

RequiredCase::~RequiredCase()
{
}

void RequiredCase::deinit(void)
{
	// free shared pointer
	m_conversion_database.clear();

	// Release the source object before we continue
	if (m_src_object_id != 0)
	{
		destroyGLObject(m_source_attachment_type, m_src_object_id);

		m_src_object_id = 0;
	}

	if (m_dst_object_id != 0)
	{
		destroyGLObject(m_destination_attachment_type, m_dst_object_id);

		m_dst_object_id = 0;
	}

	destroyObjectsSupportingNonRenderableInternalformats(m_f_src_f_dst_internalformat);
	destroyObjectsSupportingNonRenderableInternalformats(m_i_src_i_dst_internalformat);
	destroyObjectsSupportingNonRenderableInternalformats(m_ui_src_ui_dst_internalformat);
}

tcu::TestNode::IterateResult RequiredCase::iterate(void)
{
	glu::RenderContext& renderContext = m_context.getRenderContext();
	const Functions&	gl			  = renderContext.getFunctions();

	GLuint draw_fbo_id = 0;
	GLuint read_fbo_id = 0;
	gl.genFramebuffers(1, &draw_fbo_id);
	gl.genFramebuffers(1, &read_fbo_id);

	gl.bindTexture(GL_TEXTURE_2D, 0);
	gl.bindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_fbo_id);
	gl.bindFramebuffer(GL_READ_FRAMEBUFFER, read_fbo_id);

	// We will be reading from zeroth color attachment
	gl.readBuffer(GL_COLOR_ATTACHMENT0);

	// Make sure the pixel storage is configured accordingly to our data sets!
	gl.pixelStorei(GL_UNPACK_ALIGNMENT, 1);
	gl.pixelStorei(GL_PACK_ALIGNMENT, 1);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glPixelStorei");

	m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");

	// Sanity checks
	DE_ASSERT(m_destination_attachment_type == GL_TEXTURE_2D ||
			  m_destination_attachment_type == GL_TEXTURE_CUBE_MAP_NEGATIVE_X ||
			  m_destination_attachment_type == GL_TEXTURE_CUBE_MAP_NEGATIVE_Y ||
			  m_destination_attachment_type == GL_TEXTURE_CUBE_MAP_NEGATIVE_Z ||
			  m_destination_attachment_type == GL_TEXTURE_CUBE_MAP_POSITIVE_X ||
			  m_destination_attachment_type == GL_TEXTURE_CUBE_MAP_POSITIVE_Y ||
			  m_destination_attachment_type == GL_TEXTURE_CUBE_MAP_POSITIVE_Z);

	// Determine general attachment type
	GLenum general_attachment_type = getGeneralTargetForDetailedTarget(m_source_attachment_type);
	if (general_attachment_type == GL_NONE)
		return STOP;

	// Set up source object
	m_src_object_id = generateGLObject(m_source_attachment_type);
	if (m_src_object_id == 0)
		return STOP;

	// Set up destination object
	m_dst_object_id = generateGLObject(m_destination_attachment_type);
	if (m_dst_object_id == 0)
		return STOP;

	// Generate all objects required to execute the non-renderable internalformat tests.
	// Can't use the shader on GL_RENDERBUFFER as source.
	if (m_source_attachment_type != GL_RENDERBUFFER && !generateObjectsToSupportNonColorRenderableInternalformats())
	{
		return STOP;
	}

	m_conversion_database.get()->initializeDatabase();

	// Run through all FBO internal formats.
	bool	  result				 = true;
	const int n_dst_internal_formats = DE_LENGTH_OF_ARRAY(copyTexImage2DInternalFormatOrdering);
	const int n_fbo_internal_formats = DE_LENGTH_OF_ARRAY(fboEffectiveInternalFormatOrdering);
	for (int n_fbo_internal_format = 0; n_fbo_internal_format < n_fbo_internal_formats; ++n_fbo_internal_format)
	{
		GLenum fbo_internalformat = fboEffectiveInternalFormatOrdering[n_fbo_internal_format];

		// Run through all destination internal formats.
		for (int n_dst_internal_format = 0; n_dst_internal_format < n_dst_internal_formats; ++n_dst_internal_format)
		{
			GLenum dst_internalformat = copyTexImage2DInternalFormatOrdering[n_dst_internal_format];

			switch (getDataSamplerTypeForInternalformat(fbo_internalformat))
			{
			case DATA_SAMPLER_FLOAT:
			{
				switch (getDataSamplerTypeForInternalformat(dst_internalformat))
				{
				case DATA_SAMPLER_FLOAT:
				{
					if (!execute(fbo_internalformat, dst_internalformat, &m_f_src_f_dst_internalformat))
					{
						// At least one conversion was invalid or failed. Test should fail,
						// but let's continue iterating over internalformats.
						result = false;
					}

					break;
				}

				case DATA_SAMPLER_INTEGER:
				case DATA_SAMPLER_UNSIGNED_INTEGER:
				{
					// There shouldn't be any valid conversion formats in this case. Just pass NULL for the non-renderable case's objects.
					// The test will fail if we try to verify the copy for different data type formats
					if (!execute(fbo_internalformat, dst_internalformat, NULL))
					{
						// At least one conversion was invalid or failed. Test should
						// fail, but let's continue iterating over internalformats.
						result = false;
					}

					break;
				}

				default:
				{
					// Unrecognized destination internalformat
					DE_ASSERT(0);
					break;
				}
				} // switch (GetDataSamplerTypeForInternalformat(dst_internalformat) )

				break;
			}

			case DATA_SAMPLER_INTEGER:
			{
				switch (getDataSamplerTypeForInternalformat(dst_internalformat))
				{
				case DATA_SAMPLER_INTEGER:
				{
					if (!execute(fbo_internalformat, dst_internalformat, &m_i_src_i_dst_internalformat))
					{
						// At least one conversion was invalid or failed. Test should fail,
						// but let's continue iterating over internalformats.
						result = false;
					}

					break;
				}

				case DATA_SAMPLER_FLOAT:
				case DATA_SAMPLER_UNSIGNED_INTEGER:
				{
					// There shouldn't be any valid conversion formats in this case. Just pass NULL for the non-renderable case's objects.
					// The test will fail if we try to verify the copy for different data type formats
					if (!execute(fbo_internalformat, dst_internalformat, NULL))
					{
						// At least one conversion was invalid or failed. Test should fail,
						// but let's continue iterating over internalformats.
						result = false;
					}

					break;
				}

				default:
				{
					// Unrecognized destination internalformat
					DE_ASSERT(0);

					break;
				}
				} // switch (GetDataSamplerTypeForInternalformat(dst_internalformat) )

				break;
			} // case DATA_SAMPLER_INTEGER:

			case DATA_SAMPLER_UNSIGNED_INTEGER:
			{
				switch (getDataSamplerTypeForInternalformat(dst_internalformat))
				{
				case DATA_SAMPLER_UNSIGNED_INTEGER:
				{
					if (!execute(fbo_internalformat, dst_internalformat, &m_ui_src_ui_dst_internalformat))
					{
						// At least one conversion was invalid or failed. Test should fail,
						// but let's continue iterating over internalformats.
						result = false;
					}

					break;
				}

				case DATA_SAMPLER_FLOAT:
				case DATA_SAMPLER_INTEGER:
				{
					// There shouldn't be any valid conversion formats in this case. Just pass NULL for the non-renderable case's objects.
					// The test will fail if we try to verify the copy for different data type formats
					if (!execute(fbo_internalformat, dst_internalformat, NULL))
					{
						// At least one conversion was invalid or failed. Test should fail,
						// but let's continue iterating over internalformats.
						result = false;
					}

					break;
				}

				default:
				{
					// Unrecognized destination internalformat?
					DE_ASSERT(0);
					break;
				}
				} // switch (GetDataSamplerTypeForInternalformat(dst_internalformat) )

				break;
			} // case DATA_SAMPLER_UNSIGNED_INTEGER

			default:
			{
				// Unrecognized source internalformat
				DE_ASSERT(0);
				break;
			}
			} // switch (GetDataSamplerTypeForInternalformat(fbo_internalformat) )
		}	 // for (all destination internalformats)
	}		  // for (all FBO internalformats)

	if (result)
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
	return STOP;
}

/** This function verifies if glCopyTexImage2D() implementation performs conversions as
 *  per GLES3.0.3 spec, and that the result data is valid. For more detailed description,
 *  please see specification of copy_tex_image_conversions_required conformance test.
 *
 *  @param conversion_database		 Conversion database handle. Cannot be NULL.
 *  @param source_attachment_type	  Tells what GL object (or which texture target)
 *									 should be used as a read buffer for
 *									 a glCopyTexImage2D) call. Allowed values:
 *									 1) GL_TEXTURE_2D,
 *									 2) GL_TEXTURE_2D_ARRAY,
 *									 3) GL_TEXTURE_3D,
 *									 4) GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
 *									 5) GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
 *									 6) GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
 *									 7) GL_TEXTURE_CUBE_MAP_POSITIVE_X,
 *									 8) GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
 *									 9) GL_TEXTURE_CUBE_MAP_POSITIVE_Z.
 *  @param destination_attachment_type Tells which texture target should be used for
 *									 a glCopyTexImage2D() call. Allowed values:
 *									 1) GL_TEXTURE_2D,
 *									 2) GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
 *									 3) GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
 *									 4) GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
 *									 5) GL_TEXTURE_CUBE_MAP_POSITIVE_X,
 *									 6) GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
 *									 7) GL_TEXTURE_CUBE_MAP_POSITIVE_Z.
 *  @param src_internalformat		  GLES internalformat that read buffer should use.
 *  @param dst_internalformat		  GLES internalformat that should be used for glReadPixels() call.
 *									 This should NOT be the expected effective internalformat!
 *  @param objects_ptr				 Deref where generated object ids are stored
 *									 (objects which were generated to support non-color-renderable internalformats).
 *									 Cannot be NULL.
 *
 *  @return true if successful, false otherwise.
 */
bool RequiredCase::execute(GLenum src_internalformat, GLenum dst_internalformat,
						   NonRenderableInternalformatSupportObjects* objects_ptr)
{
	GLenum fbo_completeness					   = GL_NONE;
	GLenum general_destination_attachment_type = GL_NONE;
	int	n_format_type_pair				   = 0;
	GLenum src_format						   = GL_NONE;
	GLenum src_type							   = GL_NONE;

	glu::RenderContext& renderContext = m_context.getRenderContext();
	const Functions&	gl			  = renderContext.getFunctions();

	// If we're using a renderbuffer as a source, make sure the internalformat
	// we'll try to use to store data in it is actually renderable
	if (m_destination_attachment_type == GL_RENDERBUFFER && !isValidRBOInternalFormat(src_internalformat))
		return true;

	// Only accept source internal formats that are color renderable
	if (!isColorRenderableInternalFormat(src_internalformat))
		return true;

	// Retrieve general destination attachment type before we continue
	if ((general_destination_attachment_type = getGeneralTargetForDetailedTarget(m_destination_attachment_type)) ==
		GL_NONE)
	{
		return false;
	}

	// Good. Check if the conversion is required - if so, we can run the test!
	if (!isFBOEffectiveInternalFormatCompatibleWithDestinationInternalFormat(src_internalformat, dst_internalformat))
		return true;

	bool			  result = true;
	std::vector<char> fbo_data(4);

	// Try using all compatible format+type pairs
	while (getFormatAndTypeCompatibleWithInternalformat(src_internalformat, n_format_type_pair, &src_format, &src_type))
	{
		// Try to find a rule in the conversion database, so that we know what data we should fill
		// the source attachment with.
		// There may be many entries for a single source internal format + type pair, so
		// iterate until the find() function fails.
		GLenum				effective_internalformat = GL_NONE;
		int					n_conversion_rule		 = 0;
		PixelData			result_bottomleft_pixel_data;
		PixelData			result_bottomright_pixel_data;
		PixelData			result_topleft_pixel_data;
		PixelData			result_topright_pixel_data;
		GLenum				result_type = GL_NONE;
		PixelData			src_bottomleft_pixel_data;
		PixelData			src_bottomright_pixel_data;
		PixelData			src_topleft_pixel_data;
		PixelData			src_topright_pixel_data;
		PixelCompareChannel channels_to_compare;

		while (findEntryInConversionDatabase(
			n_conversion_rule, src_internalformat, src_type, dst_internalformat, &effective_internalformat,
			&result_type, &src_topleft_pixel_data, &src_topright_pixel_data, &src_bottomleft_pixel_data,
			&src_bottomright_pixel_data, &result_topleft_pixel_data, &result_topright_pixel_data,
			&result_bottomleft_pixel_data, &result_bottomright_pixel_data, &channels_to_compare))
		{
#if 0
			m_testCtx.getLog() << tcu::TestLog::Message
							   << "Testing [src "
							   << getInternalformatString(src_internalformat)
							   << " " << glu::getTypeStr(src_type).toString()
							   << "]=>[" << getInternalformatString(dst_internalformat) << "effective: "
							   << getInternalformatString(effective_internalformat) << "] read with type: ["
							   << glu::getTypeStr(result_type).toString() << ", src target: [" << GetTargetName(m_source_attachment_type)
							   << "], dst target: " << GetTargetName(m_destination_attachment_type)
							   << tcu::TestLog::EndMessage;
#endif

			// Retrieve source data we can have uploaded to the source attachment
			if (!getRawDataFromPixelData(fbo_data, src_topleft_pixel_data, src_topright_pixel_data,
										 src_bottomleft_pixel_data, src_bottomright_pixel_data))
			{
				unbindColorAttachments();
				return false;
			}

			// Set up source attachment
			if (!configureGLObject(1, m_source_attachment_type, m_src_object_id, src_internalformat, src_format,
								   src_type, &fbo_data[0]))
			{
				unbindColorAttachments();
				return false;
			}

			// Make sure the source FBO configuration is supported.
			fbo_completeness = gl.checkFramebufferStatus(GL_READ_FRAMEBUFFER);

			if (fbo_completeness != GL_FRAMEBUFFER_COMPLETE)
			{
				if (fbo_completeness == GL_FRAMEBUFFER_UNSUPPORTED)
				{
					// The implementation does not allow us to use source data built using this internal-format,
					// using this particular attachment type. Break out of the loop, there's no need to carry on
					// trying.
					break;
				}
				else
				{
					m_testCtx.getLog() << tcu::TestLog::Message << "FBO error - incompleteness reason ["
									   << fbo_completeness << "]" << tcu::TestLog::EndMessage;

					// This should never happen. Consider test failed
					unbindColorAttachments();
					return false;
				}
			}

			// Ask the implementation to perform the conversion!
			switch (m_destination_attachment_type)
			{
			case GL_TEXTURE_2D:
			{
				gl.bindTexture(m_destination_attachment_type, m_dst_object_id);

				gl.copyTexImage2D(m_destination_attachment_type, 0, dst_internalformat, 0 /* x */, 0 /* y */,
								  TEXTURE_WIDTH, TEXTURE_HEIGHT, 0 /* border */);

				gl.texParameterf(m_destination_attachment_type, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				gl.texParameterf(m_destination_attachment_type, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				gl.texParameterf(m_destination_attachment_type, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				gl.texParameterf(m_destination_attachment_type, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

				gl.bindTexture(m_destination_attachment_type, 0);

				break;
			}

			case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
			case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
			case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
			case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
			case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
			case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
			{
				unsigned int j = 0;
				GLuint		 dst_format, dst_type;

				getFormatAndTypeCompatibleWithInternalformat(dst_internalformat, 0, &dst_format, &dst_type);

				gl.bindTexture(general_destination_attachment_type, m_dst_object_id);

				// Initialize all faces so that the texture is CM complete
				// It's needed in case we need to use a shader to verify the copy operation
				for (j = GL_TEXTURE_CUBE_MAP_POSITIVE_X; j <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z; j++)
				{
					if (j == m_destination_attachment_type)
					{
						// Do the copy to the destination face
						gl.copyTexImage2D(j, 0, dst_internalformat, 0 /* x */, 0 /* y */, TEXTURE_WIDTH, TEXTURE_HEIGHT,
										  0 /* border */);
					}
					else
					{
						// Clear the remaining faces to catch "copy to the wrong face" errors
						static std::vector<char> zero_data(TEXTURE_WIDTH * TEXTURE_HEIGHT * 4 * sizeof(float), 0);
						gl.texImage2D(j, 0, dst_internalformat, TEXTURE_WIDTH, TEXTURE_HEIGHT, 0, dst_format, dst_type,
									  &zero_data[0]);
					}
				}

				gl.texParameterf(general_destination_attachment_type, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				gl.texParameterf(general_destination_attachment_type, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				gl.texParameterf(general_destination_attachment_type, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				gl.texParameterf(general_destination_attachment_type, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

				gl.bindTexture(general_destination_attachment_type, 0);

				break;
			} // cube-map texture target cases

			default:
			{
				// Unsupported destination attachment type
				DE_ASSERT(0);
			}
			} // switch (destination_attachment_type)

			// Has the conversion succeeded as expected?
			GLenum error_code = gl.getError();

			if (error_code != GL_NO_ERROR)
			{
				m_testCtx.getLog() << tcu::TestLog::Message << "glCopyTexImage2D() reported an error for ["
								   << getInternalformatString(src_internalformat) << "]=>["
								   << getInternalformatString(dst_internalformat)
								   << "] internalformat conversion [target=" << getTargetName(m_source_attachment_type)
								   << "], as opposed to ES specification requirements!" << tcu::TestLog::EndMessage;

				// This test is now considered failed
				result = false;
			}
			else
			{
				// Conversion succeeded. We now need to compare the data stored by OpenGL ES with reference data.
				if (isColorRenderableInternalFormat(effective_internalformat))
				{
					gl.framebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, m_destination_attachment_type,
											m_dst_object_id, 0);

					fbo_completeness = gl.checkFramebufferStatus(GL_READ_FRAMEBUFFER);
					if (fbo_completeness != GL_FRAMEBUFFER_COMPLETE)
					{
						// Per spec:
						// Although the GL defines a wide variety of internal formats for framebuffer-
						// attachable image, such as texture images and renderbuffer images, some imple-
						// mentations may not support rendering to particular combinations of internal for-
						// mats. If the combination of formats of the images attached to a framebuffer object
						// are not supported by the implementation, then the framebuffer is not complete un-
						// der the clause labeled FRAMEBUFFER_UNSUPPORTED.
						if (fbo_completeness != GL_FRAMEBUFFER_UNSUPPORTED)
						{
							m_testCtx.getLog() << tcu::TestLog::Message
											   << "Framebuffer is considered incomplete [reason: " << fbo_completeness
											   << "] - cannot proceed with the test case" << tcu::TestLog::EndMessage;
							result = false;
						}
					}
					else
					{
						if (!compareExpectedResultsByReadingPixels(
								src_topleft_pixel_data, src_topright_pixel_data, src_bottomleft_pixel_data,
								src_bottomright_pixel_data, result_topleft_pixel_data, result_topright_pixel_data,
								result_bottomleft_pixel_data, result_bottomright_pixel_data, result_type,
								effective_internalformat))
						{
							// This test is now considered failed
							result = false;
						}
					}
					gl.framebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, m_destination_attachment_type, 0,
											0);
				} // if (IsColorRenderableInternalFormat(effective_internalformat) )
				else if (m_source_attachment_type != GL_RENDERBUFFER)
				{
					// We cannot use glReadPixels()-approach to test this internalformat.
					// The approach to be taken for non-color-renderable internalformats will
					// be to use a special vertex shader to verify texture data. Outcome of the
					// comparison will be captured using transform feedback.
					GLint			   bound_draw_fbo_id	= 0;
					GLint			   bound_read_fbo_id	= 0;
					int				   compare_result_index = 0;
					std::vector<GLint> copied_compare_result_data;
					std::vector<GLint> copied_dst_texture_data;
					std::vector<GLint> copied_src_texture_data;
					GLenum			   dst_attachment_point = GL_TEXTURE2;
					GLenum			   src_attachment_point = GL_TEXTURE1;
					GLint			   samplers_to_use		= 0;
					// unique sampler values
					GLint src_2D_texture_attachment		 = GL_TEXTURE3;
					GLint src_2DArray_texture_attachment = GL_TEXTURE4;
					GLint src_3D_texture_attachment		 = GL_TEXTURE5;
					GLint src_Cube_texture_attachment	= GL_TEXTURE6;
					GLint dst_2D_texture_attachment		 = GL_TEXTURE7;
					GLint dst_Cube_texture_attachment	= GL_TEXTURE8;

					if (m_source_attachment_type == GL_TEXTURE_2D_ARRAY)
					{
						samplers_to_use				   = TEXTURE_2D_ARRAY_SAMPLER_TYPE;
						src_2DArray_texture_attachment = src_attachment_point;
					}
					else if (m_source_attachment_type == GL_TEXTURE_3D)
					{
						samplers_to_use			  = TEXTURE_3D_SAMPLER_TYPE;
						src_3D_texture_attachment = src_attachment_point;
					}
					else if (m_source_attachment_type != GL_TEXTURE_2D)
					{
						samplers_to_use				= TEXTURE_CUBE_SAMPLER_TYPE;
						src_Cube_texture_attachment = src_attachment_point;
					}
					else
						src_2D_texture_attachment = src_attachment_point;

					if (m_destination_attachment_type != GL_TEXTURE_2D)
					{
						samplers_to_use				= (samplers_to_use | (TEXTURE_CUBE_SAMPLER_TYPE << 8));
						dst_Cube_texture_attachment = dst_attachment_point;
					}
					else
						dst_2D_texture_attachment = dst_attachment_point;

					// We will get a NULL pointer here if src and dst data type are different
					// (NORM -> INT, UNSIGNED INT -> INT etc.). It's not allowed by the spec.
					if (objects_ptr == NULL)
					{
						m_testCtx.getLog()
							<< tcu::TestLog::Message << "Source and destination should be of the same data type - "
														"cannot proceed with the test case"
							<< tcu::TestLog::EndMessage;
						result = false;
						restoreBindings(src_attachment_point, dst_attachment_point, bound_draw_fbo_id,
										bound_read_fbo_id);
						continue;
					}

					// Retrieve currently bound framebuffer (draw and read) object IDs.
					// If there is any FBO bound, glDraw*() function uses it, which is not wanted in this situation.
					// What we do here is: unbinding FBOs, issue draw calls, bind FBOs again.
					gl.getIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &bound_draw_fbo_id);
					gl.getIntegerv(GL_READ_FRAMEBUFFER_BINDING, &bound_read_fbo_id);

					// Use default framebuffer object for this case purposes.
					gl.bindFramebuffer(GL_FRAMEBUFFER, 0);

					// Bind source texture object to specific texture unit.
					if (!bindTextureToTargetToSpecificTextureUnit(m_src_object_id, m_source_attachment_type,
																  src_attachment_point))
					{
						result = false;
						restoreBindings(src_attachment_point, dst_attachment_point, bound_draw_fbo_id,
										bound_read_fbo_id);
						continue;
					}

					// Bind destination texture object to specific texture unit.
					if (!bindTextureToTargetToSpecificTextureUnit(m_dst_object_id, m_destination_attachment_type,
																  dst_attachment_point))
					{
						result = false;
						restoreBindings(src_attachment_point, dst_attachment_point, bound_draw_fbo_id,
										bound_read_fbo_id);
						continue;
					}

					// Set active program object.
					gl.useProgram(objects_ptr->program_object_id);

					if (!setUniformValues(objects_ptr->src_2D_texture_uniform_location, src_2D_texture_attachment,
										  objects_ptr->src_2DArray_texture_uniform_location,
										  src_2DArray_texture_attachment, objects_ptr->src_3D_texture_uniform_location,
										  src_3D_texture_attachment, objects_ptr->src_Cube_texture_uniform_location,
										  src_Cube_texture_attachment, objects_ptr->dst_2D_texture_uniform_location,
										  dst_2D_texture_attachment, objects_ptr->dst_Cube_texture_uniform_location,
										  dst_Cube_texture_attachment,
										  objects_ptr->channels_to_compare_uniform_location, channels_to_compare,
										  objects_ptr->samplers_to_use_uniform_location, samplers_to_use))
					{
						result = false;
						restoreBindings(src_attachment_point, dst_attachment_point, bound_draw_fbo_id,
										bound_read_fbo_id);
						continue;
					}

					gl.bindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, COMPARISON_RESULT_BUFFER_OBJECT_INDEX,
									  objects_ptr->comparison_result_buffer_object_id);
					gl.bindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, SOURCE_TEXTURE_PIXELS_BUFFER_OBJECT_INDEX,
									  objects_ptr->src_texture_pixels_buffer_object_id);
					gl.bindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, DESTINATION_TEXTURE_PIXELS_BUFFER_OBJECT_INDEX,
									  objects_ptr->dst_texture_pixels_buffer_object_id);

					// Enable texture corrdinates (vertex attribs 0 & 1)
					gl.enableVertexAttribArray(SRC_TEXTURE_COORDS_ATTRIB_INDEX);
					gl.enableVertexAttribArray(DST_TEXTURE_COORDS_ATTRIB_INDEX);

					// Begin transform feedback operations.
					gl.enable(GL_RASTERIZER_DISCARD);

					// Issue transform feedback operations.
					gl.beginTransformFeedback(GL_POINTS);
					error_code = gl.getError();
					if (GL_NO_ERROR != error_code)
					{
						m_testCtx.getLog()
							<< tcu::TestLog::Message << "An error [" << error_code
							<< "] occurred after glBeginTransformFeedback() call." << tcu::TestLog::EndMessage;
						result = false;
						restoreBindings(src_attachment_point, dst_attachment_point, bound_draw_fbo_id,
										bound_read_fbo_id);
						continue;
					}

					gl.drawArrays(GL_POINTS, 0, NUMBER_OF_POINTS_TO_DRAW);

					error_code = gl.getError();
					if (GL_NO_ERROR != error_code)
					{
						m_testCtx.getLog() << tcu::TestLog::Message << "An error [" << error_code
										   << "] occurred after glDrawArrays() call." << tcu::TestLog::EndMessage;
						result = false;
						restoreBindings(src_attachment_point, dst_attachment_point, bound_draw_fbo_id,
										bound_read_fbo_id);
						continue;
					}

					gl.endTransformFeedback();

					error_code = gl.getError();
					if (GL_NO_ERROR != error_code)
					{
						m_testCtx.getLog()
							<< tcu::TestLog::Message << "An error [" << error_code
							<< "] occurred after glEndTransformFeedback() call." << tcu::TestLog::EndMessage;
						result = false;
						restoreBindings(src_attachment_point, dst_attachment_point, bound_draw_fbo_id,
										bound_read_fbo_id);
						continue;
					}

					// Restore default active program object.
					gl.useProgram(0);

					// Make sure no error was generated at this point.
					error_code = gl.getError();
					if (GL_NO_ERROR != error_code)
					{
						m_testCtx.getLog()
							<< tcu::TestLog::Message << "An error [" << error_code
							<< "] occurred while working with transform feedback object." << tcu::TestLog::EndMessage;
						result = false;
						restoreBindings(src_attachment_point, dst_attachment_point, bound_draw_fbo_id,
										bound_read_fbo_id);
						continue;
					}

					gl.disable(GL_RASTERIZER_DISCARD);

					// Let's read the buffer data now.
					copyDataFromBufferObject(objects_ptr->comparison_result_buffer_object_id,
											 copied_compare_result_data);
					copyDataFromBufferObject(objects_ptr->src_texture_pixels_buffer_object_id, copied_src_texture_data);
					copyDataFromBufferObject(objects_ptr->dst_texture_pixels_buffer_object_id, copied_dst_texture_data);

					// Check the results.
					for (compare_result_index = 0; compare_result_index < NUMBER_OF_POINTS_TO_DRAW;
						 compare_result_index++)
					{
						if (copied_compare_result_data[compare_result_index] != 1)
						{
							int index_in_vec4_array = compare_result_index * NUMBER_OF_ELEMENTS_IN_VEC4;

							// Returned result indicates that textures are different.
							// Print texture object contents as well.
							displayPixelComparisonFailureMessage(copied_src_texture_data[index_in_vec4_array],
																 copied_src_texture_data[index_in_vec4_array + 1],
																 copied_src_texture_data[index_in_vec4_array + 2],
																 copied_src_texture_data[index_in_vec4_array + 3],
																 src_internalformat, src_type, 0, 0, 0, 0, GL_NONE,
																 GL_NONE, copied_dst_texture_data[index_in_vec4_array],
																 copied_dst_texture_data[index_in_vec4_array + 1],
																 copied_dst_texture_data[index_in_vec4_array + 2],
																 copied_dst_texture_data[index_in_vec4_array + 3],
																 dst_internalformat, result_type, 0, 0, 0, 0);

							// Report failure.
							result = false;
						}
					}

					fbo_completeness = GL_FRAMEBUFFER_COMPLETE;

					restoreBindings(src_attachment_point, dst_attachment_point, bound_draw_fbo_id, bound_read_fbo_id);
				} // if (source_attachment_type != GL_RENDERBUFFER && destination_attachment_type != GL_RENDERBUFFER)
			}	 // if (no error was reported by GLES)

			n_conversion_rule++;
		}

		// There should be at least ONE conversion rule defined
		// for each valid FBO effective internalformat =>copyteximage2d internalformat defined!
		// NOTE: This assertion can fail IF GLES implementation does not support particular FBO attachment combination.
		//	   Make sure the check is not performed, should GL_FRAMEBUFFER_UNSUPPORTED fbo status be reported.
		if (fbo_completeness != GL_FRAMEBUFFER_UNSUPPORTED)
		{
			if (n_conversion_rule == 0)
			{
				m_testCtx.getLog() << tcu::TestLog::Message << "No conversion rule for [src "
								   << getInternalformatString(src_internalformat) << " "
								   << glu::getTypeStr(src_type).toString() << "]=>["
								   << getInternalformatString(dst_internalformat)
								   << "effective: " << getInternalformatString(effective_internalformat)
								   << "] read with type: [" << glu::getTypeStr(result_type).toString()
								   << ", src target: [" << getTargetName(m_source_attachment_type)
								   << "], dst target: " << getTargetName(m_destination_attachment_type)
								   << tcu::TestLog::EndMessage;
			}
		}

		// Check next format+type combination
		n_format_type_pair++;

		// If we're copying from a renderbuffer, we don't really care about compatible format+type pairs, as
		// the effective internalformat is explicitly configured by glRenderbufferStorage() call.
		if (m_source_attachment_type == GL_RENDERBUFFER)
		{
			break;
		} // if (general_attachment_type == GL_RENDERBUFFER)
	}	 // while (internalformat has n-th legal format+type pair)

	unbindColorAttachments();
	return result;
}

/** Binds texture object to a given texture target of a specified texture unit.
 *
 * @param to_id		  Valid texture object ID to be bound.
 * @param texture_target Valid texture target to which @param to_id will be bound.
 * @param texture_unit   Texture unit to which @param to_id will be bound.
 *
 * @return GTFtrue if successful, GTFfalse otherwise.
 */
bool RequiredCase::bindTextureToTargetToSpecificTextureUnit(GLuint to_id, GLenum texture_target, GLenum texture_unit)
{
	glu::RenderContext& renderContext = m_context.getRenderContext();
	const Functions&	gl			  = renderContext.getFunctions();

	// Set active texture unit.
	gl.activeTexture(texture_unit);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glActiveTexture");

	if (texture_target == GL_TEXTURE_CUBE_MAP_POSITIVE_X || texture_target == GL_TEXTURE_CUBE_MAP_NEGATIVE_X ||
		texture_target == GL_TEXTURE_CUBE_MAP_POSITIVE_Y || texture_target == GL_TEXTURE_CUBE_MAP_NEGATIVE_Y ||
		texture_target == GL_TEXTURE_CUBE_MAP_POSITIVE_Z || texture_target == GL_TEXTURE_CUBE_MAP_NEGATIVE_Z)
	{
		texture_target = GL_TEXTURE_CUBE_MAP;
	}

	// Bind texture object to specific texture target of specified texture unit.
	gl.bindTexture(texture_target, to_id);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture");

	// Restore default active texture unit.
	gl.activeTexture(GL_TEXTURE0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glActiveTexture");

	return true;
}

/** Sets values of uniforms, that will later be used to perform data check-up for non-renderable internalformats.
 *
 * @param source_2D_texture_uniform_location		Location for source 2D texture sample uniform.
 * @param source_2D_texture_unit					Texture unit which the source 2D texture object is bound to.
 *												  Will be used to set value for @param source_2D_texture_uniform_location.
 * @param source_2DArray_texture_uniform_location   Location for source 2DArray texture sample uniform.
 * @param source_2DArray_texture_unit			   Texture unit which the source 2DArray texture object is bound to.
 *												  Will be used to set value for @param source_2DArray_texture_uniform_location.
 * @param source_3D_texture_uniform_location		Location for source 3D texture sample uniform.
 * @param source_3D_texture_unit					Texture unit which the source 3D texture object is bound to.
 *												  Will be used to set value for @param source_Cube_texture_uniform_location.
 * @param source_Cube_texture_uniform_location	  Location for source Cube texture sample uniform.
 * @param source_Cube_texture_unit				  Texture unit which the source 2D texture object is bound to.
 *												  Will be used to set value for @param source_2D_texture_uniform_location.
 * @param destination_2D_texture_uniform_location   Location for destination texture sample uniform.
 * @param destination_2D_texture_unit			   Texture unit which the destination texture object is bound to.
 *												  Will be used to set value for @param destination_2D_texture_uniform_location.
 * @param destination_Cube_texture_uniform_location Location for destination texture sample uniform.
 * @param destination_Cube_texture_unit			 Texture unit which the destination texture object is bound to.
 *												  Will be used to set value for @param destination_Cube_texture_uniform_location.
 * @param channels_to_compare_uniform_location	  Location for components to compare value uniform.
 * @param channels_to_compare					   Components to compare value.
 * @param samplers_to_use_uniform_location		  Location for samplers to use value uniform.
 * @param samplers_to_use						   samplers to use value.
 *
 * @return GTFtrue if the operation succeeded (no error was generated),
 *		 GTFfalse otherwise.
 */
bool RequiredCase::setUniformValues(GLint source_2D_texture_uniform_location, GLenum source_2D_texture_unit,
									GLint source_2DArray_texture_uniform_location, GLenum source_2DArray_texture_unit,
									GLint source_3D_texture_uniform_location, GLenum source_3D_texture_unit,
									GLint source_Cube_texture_uniform_location, GLenum source_Cube_texture_unit,
									GLint destination_2D_texture_uniform_location, GLenum destination_2D_texture_unit,
									GLint  destination_Cube_texture_uniform_location,
									GLenum destination_Cube_texture_unit, GLint channels_to_compare_uniform_location,
									GLint channels_to_compare, GLint samplers_to_use_uniform_location,
									GLint samplers_to_use)
{
	glu::RenderContext& renderContext = m_context.getRenderContext();
	const Functions&	gl			  = renderContext.getFunctions();

	if (source_2D_texture_uniform_location == -1 || source_2DArray_texture_uniform_location == -1 ||
		source_3D_texture_uniform_location == -1 || source_Cube_texture_uniform_location == -1 ||
		destination_2D_texture_uniform_location == -1 || destination_Cube_texture_uniform_location == -1 ||
		channels_to_compare_uniform_location == -1 || samplers_to_use_uniform_location == -1)
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "Cannot set uniform values for invalid uniform locations."
						   << tcu::TestLog::EndMessage;

		return false;
	} // if (input uniform locations are invalid)

	// We are now ready to set uniform values.
	gl.uniform1i(destination_2D_texture_uniform_location, destination_2D_texture_unit - GL_TEXTURE0);
	gl.uniform1i(destination_Cube_texture_uniform_location, destination_Cube_texture_unit - GL_TEXTURE0);
	gl.uniform1i(source_2D_texture_uniform_location, source_2D_texture_unit - GL_TEXTURE0);
	gl.uniform1i(source_2DArray_texture_uniform_location, source_2DArray_texture_unit - GL_TEXTURE0);
	gl.uniform1i(source_3D_texture_uniform_location, source_3D_texture_unit - GL_TEXTURE0);
	gl.uniform1i(source_Cube_texture_uniform_location, source_Cube_texture_unit - GL_TEXTURE0);
	gl.uniform1i(channels_to_compare_uniform_location, channels_to_compare);
	gl.uniform1i(samplers_to_use_uniform_location, samplers_to_use);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i");

	return true;
}

/** Retrieves and copies data stored in buffer object into allocated memory buffer.
 *  It is user's responsibility to free allocated memory.
 *
 * @param bo_id				  Valid buffer object ID from which data is retrieved.
 * @param retrieved_data_ptr_ptr Deref will be used to store retrieved buffer object data.
 *
 * @return GTFtrue if successful, GTFfalse otherwise.
 */
bool RequiredCase::copyDataFromBufferObject(GLuint bo_id, std::vector<GLint>& retrieved_data)
{
	glu::RenderContext& renderContext = m_context.getRenderContext();
	const Functions&	gl			  = renderContext.getFunctions();

	GLint buffer_size = 0;
	gl.bindBuffer(GL_ARRAY_BUFFER, bo_id);
	gl.getBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &buffer_size);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glGetBufferParameteriv");

	GLint* buffer_data_ptr = NULL;
	buffer_data_ptr		   = (GLint*)gl.mapBufferRange(GL_ARRAY_BUFFER, 0, buffer_size, GL_MAP_READ_BIT);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glMapBufferRange");

	if (buffer_data_ptr == NULL)
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "Could not map buffer object." << tcu::TestLog::EndMessage;
		return false;
	}

	// Copy retrieved buffer data.
	retrieved_data.resize(buffer_size / sizeof(GLint));
	std::memcpy(&retrieved_data[0], buffer_data_ptr, buffer_size);

	gl.unmapBuffer(GL_ARRAY_BUFFER);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glUnmapBuffer");

	return true;
}

/** Allocates a buffer of sufficient size to hold 2x2 texture data represented
 *  with @param read_type GL type, issues a glReadPixels() call and then compares
 *  retrieved data with reference data (provided by the caller using reference_*
 *  arguments).
 *  Should it happen that the call resulted in an indirect conversion, the function
 *  calculates an epsilon, taking differences in amount of bits that were used to
 *  represent the data during any stage of the conversion into consideration.
 *
 *  @param source_tl_pixel_data	Describes pixel data that was used to build source
 *								 object's contents (top-left corner).
 *  @param source_tr_pixel_data	Describes pixel data that was used to build source
 *								 object's contents (top-right corner).
 *  @param source_bl_pixel_data	Describes pixel data that was used to build source
 *								 object's contents (bottom-left corner).
 *  @param source_br_pixel_data	Describes pixel data that was used to build source
 *								 object's contents (bottom-right corner).
 *  @param reference_tl_pixel_data Describes ideal result pixel data. (top-left corner).
 *  @param reference_tr_pixel_data Describes ideal result pixel data. (top-right corner).
 *  @param reference_bl_pixel_data Describes ideal result pixel data. (bottom-left corner).
 *  @param reference_br_pixel_data Describes ideal result pixel data. (bottom-right corner).
 *  @param read_type			   GL type that will be used for glReadPixels() call. This
 *								 type should be directly related with data type used in
 *								 all reference_* pixel data arguments.
 *  @param result_internalformat   Effective internal-format, expected to be used by the
 *								 implementation to hold destination object's data.
 *  @param src_format			  GL format used for source object's data storage.
 *  @param src_type				GL type used for source object's data storage.
 *  @param src_attachment_type	 Object type or texture target of the source object.
 *  @param dst_attachment_type	 Object type or texture target of the destination object.
 *
 *  @return GTFtrue if all read pixels were correct, GTFfalse otherwise
 **/
bool RequiredCase::compareExpectedResultsByReadingPixels(PixelData source_tl_pixel_data, PixelData source_tr_pixel_data,
														 PixelData source_bl_pixel_data, PixelData source_br_pixel_data,
														 PixelData reference_tl_pixel_data,
														 PixelData reference_tr_pixel_data,
														 PixelData reference_bl_pixel_data,
														 PixelData reference_br_pixel_data, GLenum read_type,
														 GLenum result_internalformat)
{
	char*		 data_traveller_ptr		  = NULL;
	int			 n						  = 0;
	unsigned int n_bytes_per_result_pixel = 0;
	GLenum		 read_format			  = GL_NONE;
	bool		 result					  = true;

	PixelData* reference_pixels[] = {
		&reference_bl_pixel_data, &reference_br_pixel_data, &reference_tl_pixel_data, &reference_tr_pixel_data,
	};
	PixelData* source_pixels[] = { &source_bl_pixel_data, &source_br_pixel_data, &source_tl_pixel_data,
								   &source_tr_pixel_data };
	PixelData result_pixels[4];

	// Determine which read format should be used for reading.
	// Note that GLES3 accepts GL_RGBA_INTEGER format for GL_RGB10_A2UI internalformat
	// and GL_RGBA for GL_RGB10_A2 - handle this in a special case.
	if (((read_type == GL_UNSIGNED_INT_2_10_10_10_REV) && (result_internalformat == GL_RGB10_A2UI)) ||
		(read_type == GL_UNSIGNED_INT) || (read_type == GL_INT))
	{
		read_format = GL_RGBA_INTEGER;
	}
	else
	{
		read_format = GL_RGBA;
	}

	// Update read_type for GL_HALF_FLOAT
	if (read_type == GL_HALF_FLOAT)
	{
		read_type = GL_FLOAT;
	}

	// Allocate data buffer
	n_bytes_per_result_pixel = getSizeOfPixel(read_format, read_type);
	std::vector<char> data(TEXTURE_WIDTH * TEXTURE_HEIGHT * n_bytes_per_result_pixel);

	glu::RenderContext& renderContext = m_context.getRenderContext();
	const Functions&	gl			  = renderContext.getFunctions();

	// Retrieve the data.
	gl.readPixels(0, 0, TEXTURE_WIDTH, TEXTURE_HEIGHT, read_format, read_type, &data[0]);

	// Was the operation successful?
	GLenum error_code = gl.getError();
	if (error_code != GL_NO_ERROR)
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "glReadPixels() failed with error: [" << error_code << "]"
						   << tcu::TestLog::EndMessage;
		return false;
	}

	// Convert the data we read back to pixel data structures
	data_traveller_ptr = &data[0];

	for (n = 0; n < DE_LENGTH_OF_ARRAY(reference_pixels); ++n)
	{
		PixelData* result_pixel_ptr = result_pixels + n;

		if (!getPixelDataFromRawData(data_traveller_ptr, read_format, read_type, result_pixel_ptr))
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "GetPixelDataFromRawData failed!"
							   << tcu::TestLog::EndMessage;

			// Could not convert raw data to pixel data instance!
			DE_ASSERT(0);
			return false;
		} // if (raw data->pixel data conversion failed)

		// Move the data traveller
		data_traveller_ptr += n_bytes_per_result_pixel;
	} // for (all pixels)

	// Compare each pixel with reference data. For debugging purposes, compare every single pixel,
	// even if at least one comparison has already failed.
	DE_ASSERT(DE_LENGTH_OF_ARRAY(reference_pixels) == DE_LENGTH_OF_ARRAY(result_pixels));

	for (n = 0; n < DE_LENGTH_OF_ARRAY(reference_pixels); ++n)
	{
		result &= comparePixelData(result_pixels[n], *(reference_pixels[n]), *(source_pixels[n]), result_internalformat,
								   (result == 0));
	} // For each pixel

	if (result == false)
	{
		// Log a separator line for clarity
		m_testCtx.getLog() << tcu::TestLog::Message << "<-- Erroneous test case finishes." << tcu::TestLog::EndMessage;
	}

	return result;
}

/** Retrieves size (expressed in bytes) of a single pixel represented by
 *  a @param format format + @param type type pair.
 *
 *  @param format GLES format to consider.
 *  @param type   GLES type to consider.
 *
 *  @return Size of the pixel or 0 if either of the arguments was not recognized.
 **/
unsigned int RequiredCase::getSizeOfPixel(GLenum format, GLenum type)
{
	int result = 0;

	switch (format)
	{
	case GL_RED:
		result = 1;
		break;
	case GL_RED_INTEGER:
		result = 1;
		break;
	case GL_RG:
		result = 2;
		break;
	case GL_RG_INTEGER:
		result = 2;
		break;
	case GL_RGB:
		result = 3;
		break;
	case GL_RGB_INTEGER:
		result = 3;
		break;
	case GL_RGBA:
		result = 4;
		break;
	case GL_RGBA_INTEGER:
		result = 4;
		break;
	case GL_DEPTH_COMPONENT:
		result = 1;
		break;
	case GL_DEPTH_STENCIL:
		result = 2;
		break;
	case GL_LUMINANCE_ALPHA:
		result = 2;
		break;
	case GL_LUMINANCE:
		result = 1;
		break;
	case GL_ALPHA:
		result = 1;
		break;

	default:
	{
		DE_ASSERT(0);
		result = 0;
	}
	}

	switch (type)
	{
	case GL_UNSIGNED_BYTE:
		result *= 1;
		break;
	case GL_BYTE:
		result *= 1;
		break;
	case GL_UNSIGNED_SHORT:
		result *= 2;
		break;
	case GL_SHORT:
		result *= 2;
		break;
	case GL_UNSIGNED_INT:
		result *= 4;
		break;
	case GL_INT:
		result *= 4;
		break;
	case GL_HALF_FLOAT:
		result *= 2;
		break;
	case GL_FLOAT:
		result *= 4;
		break;
	case GL_UNSIGNED_SHORT_5_6_5:
		result = 2;
		break;
	case GL_UNSIGNED_SHORT_4_4_4_4:
		result = 2;
		break;
	case GL_UNSIGNED_SHORT_5_5_5_1:
		result = 2;
		break;
	case GL_UNSIGNED_INT_2_10_10_10_REV:
		result = 4;
		break;
	case GL_UNSIGNED_INT_10F_11F_11F_REV:
		result = 4;
		break;
	case GL_UNSIGNED_INT_5_9_9_9_REV:
		result = 4;
		break;
	case GL_UNSIGNED_INT_24_8:
		result = 4;
		break;
	case GL_FLOAT_32_UNSIGNED_INT_24_8_REV:
		result = 8;
		break;

	default:
	{
		DE_ASSERT(0);

		result = 0;
	}
	}

	return result;
}

/** Takes a pointer with raw data representation and converts it to
 *  four instances of _pixel_data corresponding to four corners of a
 *  quad used for verification purposes. Assumes 2x2 resolution.
 *
 *  @param raw_data		Pointer to a buffer storing the data.
 *  @param raw_data_format Format of the data stored under @param raw_data.
 *  @param raw_data_type   Type of the data stored under @param raw_data.
 *  @param out_result	  Deref will be used to store four _pixel_data instances.
 *						 Cannot be NULL, must be capacious enough to hold four
 *						 instances of the structure.
 *
 *  @return GTFtrue if successful, GTFfalse otherwise.
 **/
bool RequiredCase::getPixelDataFromRawData(void* raw_data, GLenum raw_data_format, GLenum raw_data_type,
										   PixelData* out_result)
{
	// Sanity checks: format should be equal to one of the values supported
	//				by glReadPixels()
	DE_ASSERT(raw_data_format == GL_RGBA || raw_data_format == GL_RGBA_INTEGER);

	if (raw_data_format != GL_RGBA && raw_data_format != GL_RGBA_INTEGER)
	{
		return false;
	}

	// Sanity checks: type should be equal to one of the values supported
	//				by glReadPixels()
	DE_ASSERT(raw_data_type == GL_UNSIGNED_BYTE || raw_data_type == GL_UNSIGNED_INT || raw_data_type == GL_INT ||
			  raw_data_type == GL_FLOAT || raw_data_type == GL_UNSIGNED_INT_2_10_10_10_REV_EXT);

	if (raw_data_type != GL_UNSIGNED_BYTE && raw_data_type != GL_UNSIGNED_INT && raw_data_type != GL_INT &&
		raw_data_type != GL_FLOAT && raw_data_type != GL_UNSIGNED_INT_2_10_10_10_REV_EXT)
	{
		return false;
	}

	// Reset the result structure
	deMemset(out_result, 0, sizeof(PixelData));

	out_result->data_internalformat = raw_data_format;
	out_result->data_type			= raw_data_type;

	// Fill the fields, depending on user-provided format+type pair
	if (raw_data_format == GL_RGBA && raw_data_type == GL_UNSIGNED_BYTE)
	{
		char* raw_data_ptr = reinterpret_cast<char*>(raw_data);

		out_result->alpha.data_type = CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
		out_result->blue.data_type  = CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
		out_result->green.data_type = CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;
		out_result->red.data_type   = CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS;

		out_result->red.unsigned_byte_data   = raw_data_ptr[0];
		out_result->green.unsigned_byte_data = raw_data_ptr[1];
		out_result->blue.unsigned_byte_data  = raw_data_ptr[2];
		out_result->alpha.unsigned_byte_data = raw_data_ptr[3];
	}
	else if (raw_data_format == GL_RGBA_INTEGER && raw_data_type == GL_UNSIGNED_INT)
	{
		unsigned int* raw_data_ptr = reinterpret_cast<unsigned int*>(raw_data);

		out_result->alpha.data_type = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
		out_result->blue.data_type  = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
		out_result->green.data_type = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;
		out_result->red.data_type   = CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS;

		out_result->red.unsigned_integer_data   = raw_data_ptr[0];
		out_result->green.unsigned_integer_data = raw_data_ptr[1];
		out_result->blue.unsigned_integer_data  = raw_data_ptr[2];
		out_result->alpha.unsigned_integer_data = raw_data_ptr[3];
	}
	else if (raw_data_format == GL_RGBA_INTEGER && raw_data_type == GL_INT)
	{
		signed int* raw_data_ptr = reinterpret_cast<signed int*>(raw_data);

		out_result->alpha.data_type = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;
		out_result->blue.data_type  = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;
		out_result->green.data_type = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;
		out_result->red.data_type   = CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS;

		out_result->red.signed_integer_data   = raw_data_ptr[0];
		out_result->green.signed_integer_data = raw_data_ptr[1];
		out_result->blue.signed_integer_data  = raw_data_ptr[2];
		out_result->alpha.signed_integer_data = raw_data_ptr[3];
	}
	else if (raw_data_format == GL_RGBA && raw_data_type == GL_FLOAT)
	{
		float* raw_data_ptr = reinterpret_cast<float*>(raw_data);

		out_result->alpha.data_type = CHANNEL_DATA_TYPE_FLOAT;
		out_result->blue.data_type  = CHANNEL_DATA_TYPE_FLOAT;
		out_result->green.data_type = CHANNEL_DATA_TYPE_FLOAT;
		out_result->red.data_type   = CHANNEL_DATA_TYPE_FLOAT;

		out_result->red.float_data   = raw_data_ptr[0];
		out_result->green.float_data = raw_data_ptr[1];
		out_result->blue.float_data  = raw_data_ptr[2];
		out_result->alpha.float_data = raw_data_ptr[3];
	} /* if (raw_data_format == GL_RGBA && raw_data_type == GL_FLOAT) */
	else
	{
		signed int* raw_data_ptr = (signed int*)raw_data;

		DE_ASSERT(raw_data_format == GL_RGBA && raw_data_type == GL_UNSIGNED_INT_2_10_10_10_REV);

		out_result->alpha.data_type = CHANNEL_DATA_TYPE_UNSIGNED_BYTE_2BITS;
		out_result->blue.data_type  = CHANNEL_DATA_TYPE_UNSIGNED_SHORT_10BITS;
		out_result->green.data_type = CHANNEL_DATA_TYPE_UNSIGNED_SHORT_10BITS;
		out_result->red.data_type   = CHANNEL_DATA_TYPE_UNSIGNED_SHORT_10BITS;

		out_result->alpha.unsigned_byte_data  = ((*raw_data_ptr) >> 30) & ((1 << 2) - 1);
		out_result->blue.unsigned_short_data  = ((*raw_data_ptr) >> 20) & ((1 << 10) - 1);
		out_result->green.unsigned_short_data = ((*raw_data_ptr) >> 10) & ((1 << 10) - 1);
		out_result->red.unsigned_short_data   = ((*raw_data_ptr)) & ((1 << 10) - 1);
	}

	return true;
}

/** Checks if downloaded pixel data is valid. Should the rendered values differ
 *  outside allowed range, the function logs detailed information about the problem.
 *
 *  @param downloaded_pixel		Instance of _pixel_data describing a pixel
 *								 that was rendered by the implementation.
 *  @param reference_pixel		 Instance of _pixel_data describing ideal
 *								 pixel data.
 *  @param source_pixel			Instance of _pixel_data describing the pixel
 *								 prior to conversion.
 *  @param result_internalformat   Internal format the implementation is expected
 *								 to be using for the converted data.
 *  @param src_attachment_type	 Type of the source object used for the conversion.
 *  @param dst_attachment_type	 Type of the destination object used for the conversion.
 *  @param has_test_failed_already 1 if any of the other pixels making up the test 2x2
 *								 data-set has already been determined to be corrupt.
 *								 0 otherwise.
 *  @param src_internalformat	  Internal-format used for source object's data storage.
 *  @param src_datatype			Type used for source object's data storage.
 *
 *  @return 1 if the pixels match, 0 otherwise.
 **/
bool RequiredCase::comparePixelData(PixelData downloaded_pixel, PixelData reference_pixel, PixelData source_pixel,
									GLenum result_internalformat, bool has_test_failed_already)
{
	ChannelData* channel_data[12]	= { 0 };
	int			 max_epsilon[4]		 = { 0 };
	int			 has_pixel_failed	= 0;
	int			 n_channel			 = 0;
	bool		 result				 = true;
	int			 result_rgba_bits[4] = { 0 };
	int			 source_rgba_bits[4] = { 0 };

	// Form channel data so we can later analyse channels one after another in a loop
	channel_data[0]  = &downloaded_pixel.red;
	channel_data[1]  = &reference_pixel.red;
	channel_data[2]  = &source_pixel.red;
	channel_data[3]  = &downloaded_pixel.green;
	channel_data[4]  = &reference_pixel.green;
	channel_data[5]  = &source_pixel.green;
	channel_data[6]  = &downloaded_pixel.blue;
	channel_data[7]  = &reference_pixel.blue;
	channel_data[8]  = &source_pixel.blue;
	channel_data[9]  = &downloaded_pixel.alpha;
	channel_data[10] = &reference_pixel.alpha;
	channel_data[11] = &source_pixel.alpha;

	// Retrieve number of bits used for source and result data.
	getNumberOfBitsForInternalFormat(source_pixel.data_internalformat, source_rgba_bits);
	getNumberOfBitsForInternalFormat(result_internalformat, result_rgba_bits);

	// Time for actual comparison!
	for (unsigned int n = 0; n < sizeof(channel_data) / sizeof(channel_data[0]);
		 n += 3 /* downloaded + reference + source pixel combinations */, ++n_channel)
	{
		ChannelData* downloaded_channel_ptr = channel_data[n];
		ChannelData* reference_channel_ptr  = channel_data[n + 1];

		// Calculate maximum epsilon
		int max_n_bits	 = 0;
		int min_n_bits	 = std::numeric_limits<int>::max();
		int n_dst_bits	 = result_rgba_bits[n_channel];
		int n_reading_bits = 0;
		int n_source_bits  = source_rgba_bits[n_channel];

		getNumberOfBitsForChannelDataType(downloaded_channel_ptr->data_type, &n_reading_bits);

		if (max_n_bits < n_dst_bits && n_dst_bits != 0)
		{
			max_n_bits = n_dst_bits;
		} /* if (max_n_bits < n_dst_bits && n_dst_bits != 0) */
		if (max_n_bits < n_reading_bits && n_reading_bits != 0)
		{
			max_n_bits = n_reading_bits;
		}
		if (max_n_bits < n_source_bits && n_source_bits != 0)
		{
			max_n_bits = n_source_bits;
		}

		if (n_dst_bits != 0)
		{
			min_n_bits = n_dst_bits;
		}

		if (min_n_bits > n_reading_bits && n_reading_bits != 0)
		{
			min_n_bits = n_reading_bits;
		}
		if (min_n_bits > n_source_bits && n_source_bits != 0)
		{
			min_n_bits = n_source_bits;
		}

		if (max_n_bits != min_n_bits && max_n_bits != 0)
		{
			DE_ASSERT(min_n_bits != std::numeric_limits<int>::max());

			// Allow rounding in either direction
			max_epsilon[n_channel] = deCeilFloatToInt32(((1 << max_n_bits) - 1.0f) / ((1 << min_n_bits) - 1));
		}
		else
		{
			max_epsilon[n_channel] = 0;
		}

		// At the moment, we only care about data types that correspond to GL types usable for glReadPixels() calls.
		// Please feel free to expand this switch() with support for data types you need.
		switch (downloaded_channel_ptr->data_type)
		{
		case CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS:
		{
			int delta = (downloaded_channel_ptr->signed_integer_data - reference_channel_ptr->signed_integer_data);

			if (abs(delta) > max_epsilon[n_channel])
			{
				if (result)
				{
					has_pixel_failed = 1;
					result			 = false;
				}
			}

			break;
		}

		case CHANNEL_DATA_TYPE_UNSIGNED_BYTE_2BITS:
		case CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS:
		{
			int delta = (downloaded_channel_ptr->unsigned_byte_data - reference_channel_ptr->unsigned_byte_data);

			if (abs(delta) > max_epsilon[n_channel])
			{
				if (result)
				{
					has_pixel_failed = 1;
					result			 = false;
				}
			}

			break;
		}

		case CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS:
		{
			int delta = static_cast<int>(downloaded_channel_ptr->unsigned_integer_data -
										 reference_channel_ptr->unsigned_integer_data);

			if (abs(delta) > max_epsilon[n_channel])
			{
				if (result)
				{
					has_pixel_failed = 1;
					result			 = false;
				}
			}

			break;
		}

		case CHANNEL_DATA_TYPE_UNSIGNED_SHORT_10BITS:
		{
			int delta = (downloaded_channel_ptr->unsigned_short_data - reference_channel_ptr->unsigned_short_data);

			if (abs(delta) > max_epsilon[n_channel])
			{
				if (result)
				{
					has_pixel_failed = 1;
					result			 = false;
				}
			}

			break;
		} /* case CHANNEL_DATA_TYPE_UNSIGNED_SHORT_10BITS: */

		case CHANNEL_DATA_TYPE_FLOAT:
		{
			int delta = deChopFloatToInt32(downloaded_channel_ptr->float_data - reference_channel_ptr->float_data);

			if (abs(delta) > max_epsilon[n_channel])
			{
				if (result)
				{
					has_pixel_failed = 1;
					result			 = false;
				}
			}

			break;
		}

		default:
		{
			// Unrecognized data type
			DE_ASSERT(0);
		}
		}

		if (has_pixel_failed && !has_test_failed_already)
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "Erroneous test case starts-->" << tcu::TestLog::EndMessage;
			has_test_failed_already = true;
		}
	} // for (all channels)

	if (!result)
	{
		displayPixelComparisonFailureMessage(
			channel_data[2] != NULL ? channel_data[2]->unsigned_integer_data : 0,
			channel_data[5] != NULL ? channel_data[5]->unsigned_integer_data : 0,
			channel_data[8] != NULL ? channel_data[8]->unsigned_integer_data : 0,
			channel_data[11] != NULL ? channel_data[11]->unsigned_integer_data : 0, source_pixel.data_internalformat,
			source_pixel.data_type, channel_data[1] != NULL ? channel_data[1]->unsigned_integer_data : 0,
			channel_data[4] != NULL ? channel_data[4]->unsigned_integer_data : 0,
			channel_data[7] != NULL ? channel_data[7]->unsigned_integer_data : 0,
			channel_data[10] != NULL ? channel_data[10]->unsigned_integer_data : 0, reference_pixel.data_internalformat,
			reference_pixel.data_type, channel_data[0] != NULL ? channel_data[0]->unsigned_integer_data : 0,
			channel_data[3] != NULL ? channel_data[3]->unsigned_integer_data : 0,
			channel_data[6] != NULL ? channel_data[6]->unsigned_integer_data : 0,
			channel_data[9] != NULL ? channel_data[9]->unsigned_integer_data : 0, result_internalformat,
			downloaded_pixel.data_type, max_epsilon[0], max_epsilon[1], max_epsilon[2], max_epsilon[3]);
	}

	return result;
}

/** Retrieves number of bits used for a single pixel, were it
 *  stored in @param internalformat internal format.
 *
 *  @param internalformat GLES internal format to consider.
 *  @param out_rgba_bits  Deref will be used to store 4 integers
 *						describing amount of bits that the internal
 *						format uses for subsequently R, G, B and A
 *						channels. Cannot be NULL.
 *
 *  @return GTFtrue if successful, GTFfalse otherwise.
 **/
bool RequiredCase::getNumberOfBitsForInternalFormat(GLenum internalformat, int* out_rgba_bits)
{
	deMemset(out_rgba_bits, 0, sizeof(int) * 4);

	switch (internalformat)
	{
	case GL_LUMINANCE8_OES:
		out_rgba_bits[0] = 8;
		break;
	case GL_R16I:
		out_rgba_bits[0] = 16;
		break;
	case GL_R16UI:
		out_rgba_bits[0] = 16;
		break;
	case GL_R32I:
		out_rgba_bits[0] = 32;
		break;
	case GL_R32UI:
		out_rgba_bits[0] = 32;
		break;
	case GL_R8:
		out_rgba_bits[0] = 8;
		break;
	case GL_R8_SNORM:
		out_rgba_bits[0] = 8;
		break;
	case GL_R8I:
		out_rgba_bits[0] = 8;
		break;
	case GL_R8UI:
		out_rgba_bits[0] = 8;
		break;
	case GL_RG16UI:
		out_rgba_bits[0] = 16;
		out_rgba_bits[1] = 16;
		break;
	case GL_RG16I:
		out_rgba_bits[0] = 16;
		out_rgba_bits[1] = 16;
		break;
	case GL_RG32I:
		out_rgba_bits[0] = 32;
		out_rgba_bits[1] = 32;
		break;
	case GL_RG32UI:
		out_rgba_bits[0] = 32;
		out_rgba_bits[1] = 32;
		break;
	case GL_RG8:
		out_rgba_bits[0] = 8;
		out_rgba_bits[1] = 8;
		break;
	case GL_RG8_SNORM:
		out_rgba_bits[0] = 8;
		out_rgba_bits[1] = 8;
		break;
	case GL_RG8I:
		out_rgba_bits[0] = 8;
		out_rgba_bits[1] = 8;
		break;
	case GL_RG8UI:
		out_rgba_bits[0] = 8;
		out_rgba_bits[1] = 8;
		break;
	case GL_RGB10_A2:
		out_rgba_bits[0] = 10;
		out_rgba_bits[1] = 10;
		out_rgba_bits[2] = 10;
		out_rgba_bits[3] = 2;
		break;
	case GL_RGB10_A2UI:
		out_rgba_bits[0] = 10;
		out_rgba_bits[1] = 10;
		out_rgba_bits[2] = 10;
		out_rgba_bits[3] = 2;
		break;
	case GL_RGB16I:
		out_rgba_bits[0] = 16;
		out_rgba_bits[1] = 16;
		out_rgba_bits[2] = 16;
		break;
	case GL_RGB16UI:
		out_rgba_bits[0] = 16;
		out_rgba_bits[1] = 16;
		out_rgba_bits[2] = 16;
		break;
	case GL_RGB32I:
		out_rgba_bits[0] = 32;
		out_rgba_bits[1] = 32;
		out_rgba_bits[2] = 32;
		break;
	case GL_RGB32UI:
		out_rgba_bits[0] = 32;
		out_rgba_bits[1] = 32;
		out_rgba_bits[2] = 32;
		break;
	case GL_RGB5_A1:
		out_rgba_bits[0] = 5;
		out_rgba_bits[1] = 5;
		out_rgba_bits[2] = 5;
		out_rgba_bits[3] = 1;
		break;
	case GL_RGB565:
		out_rgba_bits[0] = 5;
		out_rgba_bits[1] = 6;
		out_rgba_bits[2] = 5;
		break;
	case GL_RGB8:
		out_rgba_bits[0] = 8;
		out_rgba_bits[1] = 8;
		out_rgba_bits[2] = 8;
		break;
	case GL_RGB8_SNORM:
		out_rgba_bits[0] = 8;
		out_rgba_bits[1] = 8;
		out_rgba_bits[2] = 8;
		break;
	case GL_RGB8I:
		out_rgba_bits[0] = 8;
		out_rgba_bits[1] = 8;
		out_rgba_bits[2] = 8;
		break;
	case GL_RGB8UI:
		out_rgba_bits[0] = 8;
		out_rgba_bits[1] = 8;
		out_rgba_bits[2] = 8;
		break;
	case GL_RGBA16I:
		out_rgba_bits[0] = 16;
		out_rgba_bits[1] = 16;
		out_rgba_bits[2] = 16;
		out_rgba_bits[3] = 16;
		break;
	case GL_RGBA16UI:
		out_rgba_bits[0] = 16;
		out_rgba_bits[1] = 16;
		out_rgba_bits[2] = 16;
		out_rgba_bits[3] = 16;
		break;
	case GL_RGBA32I:
		out_rgba_bits[0] = 32;
		out_rgba_bits[1] = 32;
		out_rgba_bits[2] = 32;
		out_rgba_bits[3] = 32;
		break;
	case GL_RGBA32UI:
		out_rgba_bits[0] = 32;
		out_rgba_bits[1] = 32;
		out_rgba_bits[2] = 32;
		out_rgba_bits[3] = 32;
		break;
	case GL_RGBA4:
		out_rgba_bits[0] = 4;
		out_rgba_bits[1] = 4;
		out_rgba_bits[2] = 4;
		out_rgba_bits[3] = 4;
		break;
	case GL_RGBA8:
		out_rgba_bits[0] = 8;
		out_rgba_bits[1] = 8;
		out_rgba_bits[2] = 8;
		out_rgba_bits[3] = 8;
		break;
	case GL_RGBA8_SNORM:
		out_rgba_bits[0] = 8;
		out_rgba_bits[1] = 8;
		out_rgba_bits[2] = 8;
		out_rgba_bits[3] = 8;
		break;
	case GL_RGBA8I:
		out_rgba_bits[0] = 8;
		out_rgba_bits[1] = 8;
		out_rgba_bits[2] = 8;
		out_rgba_bits[3] = 8;
		break;
	case GL_RGBA8UI:
		out_rgba_bits[0] = 8;
		out_rgba_bits[1] = 8;
		out_rgba_bits[2] = 8;
		out_rgba_bits[3] = 8;
		break;
	case GL_SRGB8:
		out_rgba_bits[0] = 8;
		out_rgba_bits[1] = 8;
		out_rgba_bits[2] = 8;
		break;
	case GL_SRGB8_ALPHA8:
		out_rgba_bits[0] = 8;
		out_rgba_bits[1] = 8;
		out_rgba_bits[2] = 8;
		out_rgba_bits[3] = 8;
		break;
	case GL_R16F:
		out_rgba_bits[0] = 16;
		break;
	case GL_RG16F:
		out_rgba_bits[0] = 16;
		out_rgba_bits[1] = 16;
		break;
	case GL_RGB16F:
		out_rgba_bits[0] = 16;
		out_rgba_bits[1] = 16;
		out_rgba_bits[2] = 16;
		break;
	case GL_RGBA16F:
		out_rgba_bits[0] = 16;
		out_rgba_bits[1] = 16;
		out_rgba_bits[2] = 16;
		out_rgba_bits[3] = 16;
		break;
	case GL_R32F:
		out_rgba_bits[0] = 32;
		break;
	case GL_RG32F:
		out_rgba_bits[0] = 32;
		out_rgba_bits[1] = 32;
		break;
	case GL_RGB32F:
		out_rgba_bits[0] = 32;
		out_rgba_bits[1] = 32;
		out_rgba_bits[2] = 32;
		break;
	case GL_RGBA32F:
		out_rgba_bits[0] = 32;
		out_rgba_bits[1] = 32;
		out_rgba_bits[2] = 32;
		out_rgba_bits[3] = 32;
		break;

	default:
	{
		DE_ASSERT(0);
		return false;
	}
	}

	return true;
}

/** Browses the conversion database provided by user and looks for conversion rules
 *  that match the following requirements:
 *
 *  1) Source object's data internal format equal to @param src_internalformat.
 *  2) Source object's data type equal to @param src_type.
 *  3) Internal format used for glCopyTexImage2D() call equal to @param copyteximage2d_internalformat.
 *
 *  The function allows to find as many conversion rules matching these requirements as
 *  available. For any triple, caller should use incrementing values of @param index,
 *  starting from 0.
 *
 *  Source dataset corresponds to 2x2 image (using up to 4 channels) that the attachment bound to
 *  read buffer will use prior to glCopyTexImage2D() call.
 *  Destination dataset corresponds to 2x2 image (using up to 4 channels) that the result texture object
 *  should match (within acceptable epsilon).
 *
 *  @param index						 Index of conversion rule the caller is interested in reading.
 *  @param src_internalformat			Source object's data internal format to assume.
 *  @param src_type					  Source object's data type to assume.
 *  @param copyteximage2d_internalformat Internal format to be used for glCopyTexImage2D() call.
 *  @param out_result_internalformat	 Deref will be used to store internal format that GLES implementation
 *									   should use for storage of the converted data. Cannot be NULL.
 *  @param out_dst_type				  Deref will be used to store type that GLES implementation should use
 *									   for storage of the converted data. Cannot be NULL.
 *  @param out_src_topleft			   Deref will be used to store _pixel_data instance describing top-left
 *									   corner of the source dataset. Cannot be NULL.
 *  @param out_src_topright			  Deref will be used to store _pixel_data instance describing top-right
 *									   corner of the source dataset. Cannot be NULL.
 *  @param out_src_bottomleft			Deref will be used to store _pixel_data instance describing bottom-left
 *									   corner of the source dataset. Cannot be NULL.
 *  @param out_src_bottomright		   Deref will be used to store _pixel_data instance describing bottom-right
 *									   corner of the source dataset. Cannot be NULL.
 *  @param out_dst_topleft			   Deref will be used to store _pixel_data instance describing top-left
 *									   corner of the destination dataset.
 *  @param out_dst_topright			  Deref will be used to store _pixel_data instance describing top-right
 *									   corner of the destination dataset.
 *  @param out_dst_bottomleft			Deref will be used to store _pixel_data instance describing bottom-left
 *									   corner of the destination dataset.
 *  @param out_dst_bottomright		   Deref will be used to store _pixel_data instance describing bottom-right
 *									   corner of the destination dataset.
 *
 *  @return GTFtrue if @param index -th conversion rule was found, GTFfalse otherwise.
 **/
bool RequiredCase::findEntryInConversionDatabase(unsigned int index, GLenum src_internalformat, GLenum src_type,
												 GLenum  copyteximage2d_internalformat,
												 GLenum* out_result_internalformat, GLenum* out_dst_type,
												 PixelData* out_src_topleft, PixelData* out_src_topright,
												 PixelData* out_src_bottomleft, PixelData* out_src_bottomright,
												 PixelData* out_dst_topleft, PixelData* out_dst_topright,
												 PixelData* out_dst_bottomleft, PixelData* out_dst_bottomright,
												 PixelCompareChannel* out_channels_to_compare)
{
	const int conversion_array_width =
		sizeof(copyTexImage2DInternalFormatOrdering) / sizeof(copyTexImage2DInternalFormatOrdering[0]);
	int			 copyteximage2d_index				= -1;
	int			 fbo_effective_internalformat_index = -1;
	unsigned int n_entry							= 0;
	unsigned int n_matching_entries					= 0;
	GLenum		 result_internalformat				= GL_NONE;
	int			 result_internalformat_index		= -1;

	/* Sanity checks */
	DE_ASSERT(out_src_topleft != NULL);
	DE_ASSERT(out_src_topright != NULL);
	DE_ASSERT(out_src_bottomleft != NULL);
	DE_ASSERT(out_src_bottomright != NULL);
	DE_ASSERT(out_dst_topleft != NULL);
	DE_ASSERT(out_dst_topright != NULL);
	DE_ASSERT(out_dst_bottomleft != NULL);
	DE_ASSERT(out_dst_bottomright != NULL);

	// Retrieve internalformat that converted data will be stored in
	copyteximage2d_index			   = getIndexOfCopyTexImage2DInternalFormat(copyteximage2d_internalformat);
	fbo_effective_internalformat_index = getIndexOfFramebufferEffectiveInternalFormat(src_internalformat);

	DE_ASSERT(copyteximage2d_index != -1 && fbo_effective_internalformat_index != -1);
	if (copyteximage2d_index == -1 || fbo_effective_internalformat_index == -1)
		return false;

	result_internalformat_index = fbo_effective_internalformat_index * conversion_array_width + copyteximage2d_index;

	DE_ASSERT(result_internalformat_index < DE_LENGTH_OF_ARRAY(conversionArray));
	if (result_internalformat_index >= DE_LENGTH_OF_ARRAY(conversionArray))
		return false;

	result_internalformat = conversionArray[result_internalformat_index];

	DE_ASSERT(result_internalformat != GL_NONE);
	if (result_internalformat == GL_NONE)
		return false;

	// We use the simplest approach possible to keep the code as readable as possible.
	for (n_entry = 0; n_entry < m_conversion_database->n_entries_added; ++n_entry)
	{
		ConversionDatabaseEntry& entry_ptr = m_conversion_database->entries[n_entry];

		if (entry_ptr.src_bottomleft_corner.data_internalformat == src_internalformat &&
			entry_ptr.src_bottomleft_corner.data_type == src_type &&
			entry_ptr.dst_bottomleft_corner.data_internalformat == result_internalformat)
		{
			/* Is it the n-th match we're being asked for? */
			if (index == n_matching_entries)
			{
				/* Indeed! */
				*out_src_topleft	 = entry_ptr.src_topleft_corner;
				*out_src_topright	= entry_ptr.src_topright_corner;
				*out_src_bottomleft  = entry_ptr.src_bottomleft_corner;
				*out_src_bottomright = entry_ptr.src_bottomright_corner;
				*out_dst_topleft	 = entry_ptr.dst_topleft_corner;
				*out_dst_topright	= entry_ptr.dst_topright_corner;
				*out_dst_bottomleft  = entry_ptr.dst_bottomleft_corner;
				*out_dst_bottomright = entry_ptr.dst_bottomright_corner;

				*out_result_internalformat = entry_ptr.dst_topleft_corner.data_internalformat;
				*out_dst_type			   = entry_ptr.dst_topleft_corner.data_type;

				*out_channels_to_compare = entry_ptr.channels_to_compare;

				return true;
			}
			else
			{
				++n_matching_entries;
			}
		}
	}

	return false;
}

/** Retrieves index under which user-specified internalformat can be found in
 *  copy_tex_image_2d_internal_format_ordering array.
 *
 *  @param internalformat GLES internal format to look for.
 *
 *  @return Index >= 0 if successful, -1 otherwise.
 **/
int RequiredCase::getIndexOfCopyTexImage2DInternalFormat(GLenum internalformat)
{
	int max_index = DE_LENGTH_OF_ARRAY(copyTexImage2DInternalFormatOrdering);
	for (int index = 0; index < max_index; ++index)
	{
		if (copyTexImage2DInternalFormatOrdering[index] == internalformat)
			return index;
	}

	return -1;
}

/** Retrieves index under which user-specified internalformat can be found in
 *  fbo_effective_internal_format_ordering array.
 *
 *  @param internalformat GLES internal format to look for.
 *
 *  @return Index >= 0 if successful, -1 otherwise.
 **/
int RequiredCase::getIndexOfFramebufferEffectiveInternalFormat(GLenum internalformat)
{
	int max_index = DE_LENGTH_OF_ARRAY(fboEffectiveInternalFormatOrdering);
	for (int index = 0; index < max_index; ++index)
	{
		if (fboEffectiveInternalFormatOrdering[index] == internalformat)
			return index;
	}

	return -1;
}

/** Takes four pixels (described by _pixel_data structures) making up
 *  the 2x2 texture used for source objects, and converts the representation
 *  to raw data that can later be fed to glTexImage2D(), glTexImage3D() etc.
 *  calls.
 *
 *  NOTE: It is caller's responsibility to free the returned buffer when no
 *		longer used. Use free() function to deallocate the resource.
 *
 *  @param topleft	 Instance of _pixel_data describing top-left corner.
 *  @param topright	Instance of _pixel_data describing top-right corner.
 *  @param bottomleft  Instance of _pixel_data describing bottom-left corner.
 *  @param bottomright Instance of _pixel_data describing bottom-right corner.
 *
 *  @return Pointer to the buffer or NULL if failed.
 **/
bool RequiredCase::getRawDataFromPixelData(std::vector<char>& result, PixelData topleft, PixelData topright,
										   PixelData bottomleft, PixelData bottomright)
{
	ChannelOrder	 channel_order	 = CHANNEL_ORDER_UNKNOWN;
	GLenum			 format			   = GL_NONE;
	GLenum			 internalformat	= topleft.data_internalformat;
	unsigned int	 n_bytes_needed	= 0;
	unsigned int	 n_bytes_per_pixel = 0;
	unsigned int	 n_pixel		   = 0;
	const PixelData* pixels[]		   = { &bottomleft, &bottomright, &topleft, &topright };
	char*			 result_traveller  = DE_NULL;
	GLenum			 type			   = topleft.data_type;

	// Sanity checks
	DE_ASSERT(topleft.data_internalformat == topright.data_internalformat);
	DE_ASSERT(topleft.data_internalformat == bottomleft.data_internalformat);
	DE_ASSERT(topleft.data_internalformat == bottomright.data_internalformat);
	DE_ASSERT(topleft.data_type == topright.data_type);
	DE_ASSERT(topleft.data_type == bottomleft.data_type);
	DE_ASSERT(topleft.data_type == bottomright.data_type);

	// Allocate the buffer
	if (!getFormatForInternalformat(internalformat, &format))
	{
		DE_ASSERT(0);
		return false;
	} // if (no format known for requested internalformat)

	if (!getChannelOrderForInternalformatAndType(internalformat, type, &channel_order))
	{
		DE_ASSERT(0);
		return false;
	} // if (no channel order known for internalformat+type combination)

	// special case for GL_HALF_FLOAT, treat it as a FLOAT
	if (type == GL_HALF_FLOAT)
		n_bytes_per_pixel = getSizeOfPixel(format, GL_FLOAT);
	else
		n_bytes_per_pixel = getSizeOfPixel(format, type);
	n_bytes_needed		  = TEXTURE_WIDTH * TEXTURE_HEIGHT * n_bytes_per_pixel;

	if (n_bytes_needed == 0)
	{
		DE_ASSERT(0);
		return false;
	}

	result.resize(n_bytes_needed);

	// Fill the raw data buffer with data.
	result_traveller = &result[0];

	for (n_pixel = 0; n_pixel < sizeof(pixels) / sizeof(pixels[0]); ++n_pixel)
	{
		const ChannelData* channels[]			= { NULL, NULL, NULL, NULL }; /* We need up to four channels */
		int				   n_bits_for_channel_0 = 0;
		int				   n_bits_for_channel_1 = 0;
		int				   n_bits_for_channel_2 = 0;
		int				   n_bits_for_channel_3 = 0;
		const PixelData*   pixel_ptr			= pixels[n_pixel];

		switch (channel_order)
		{
		case CHANNEL_ORDER_ABGR:
		{
			channels[0] = &pixel_ptr->alpha;
			channels[1] = &pixel_ptr->blue;
			channels[2] = &pixel_ptr->green;
			channels[3] = &pixel_ptr->red;
			break;
		}

		case CHANNEL_ORDER_BGR:
		{
			channels[0] = &pixel_ptr->blue;
			channels[1] = &pixel_ptr->green;
			channels[2] = &pixel_ptr->red;
			break;
		}

		case CHANNEL_ORDER_BGRA:
		{
			channels[0] = &pixel_ptr->blue;
			channels[1] = &pixel_ptr->green;
			channels[2] = &pixel_ptr->red;
			channels[3] = &pixel_ptr->alpha;
			break;
		}

		case CHANNEL_ORDER_R:
		{
			channels[0] = &pixel_ptr->red;
			break;
		}

		case CHANNEL_ORDER_RG:
		{
			channels[0] = &pixel_ptr->red;
			channels[1] = &pixel_ptr->green;
			break;
		}

		case CHANNEL_ORDER_RGB:
		{
			channels[0] = &pixel_ptr->red;
			channels[1] = &pixel_ptr->green;
			channels[2] = &pixel_ptr->blue;
			break;
		}

		case CHANNEL_ORDER_RGBA:
		{
			channels[0] = &pixel_ptr->red;
			channels[1] = &pixel_ptr->green;
			channels[2] = &pixel_ptr->blue;
			channels[3] = &pixel_ptr->alpha;
			break;
		}

		default:
		{
			// Unrecognized channel order
			DE_ASSERT(0);
		}
		}

		// Pack the channel data, depending on channel sizes
		if (((channels[0] != NULL) &&
			 !getNumberOfBitsForChannelDataType(channels[0]->data_type, &n_bits_for_channel_0)) ||
			((channels[1] != NULL) &&
			 !getNumberOfBitsForChannelDataType(channels[1]->data_type, &n_bits_for_channel_1)) ||
			((channels[2] != NULL) &&
			 !getNumberOfBitsForChannelDataType(channels[2]->data_type, &n_bits_for_channel_2)) ||
			((channels[3] != NULL) &&
			 !getNumberOfBitsForChannelDataType(channels[3]->data_type, &n_bits_for_channel_3)))
		{
			// Unrecognized data type
			DE_ASSERT(0);
			return false;
		} // if (could not determine number of bits making up any of the channels)

		// NOTE: We will read HALF_FLOAT data as FLOAT data (32 bit) to avoid conversion before passing the data to GL
		if (channels[0] != NULL && channels[1] != NULL && channels[2] != NULL && channels[3] != NULL)
		{
			// RGBA32
			if (type == GL_HALF_FLOAT || ((n_bits_for_channel_0 == 32) && (n_bits_for_channel_1 == 32) &&
										  (n_bits_for_channel_2 == 32) && (n_bits_for_channel_3 == 32)))
			{
				unsigned int* result_traveller32 = (unsigned int*)result_traveller;

				*result_traveller32 = channels[0]->unsigned_integer_data;
				result_traveller32++;
				*result_traveller32 = channels[1]->unsigned_integer_data;
				result_traveller32++;
				*result_traveller32 = channels[2]->unsigned_integer_data;
				result_traveller32++;
				*result_traveller32 = channels[3]->unsigned_integer_data;

				result_traveller += 4 * 4;
			}
			else
				// RGBA16
				if (n_bits_for_channel_0 == 16 && n_bits_for_channel_1 == 16 && n_bits_for_channel_2 == 16 &&
					n_bits_for_channel_3 == 16)
			{
				unsigned short* result_traveller16 = (unsigned short*)result_traveller;

				*result_traveller16 = channels[0]->unsigned_short_data;
				result_traveller16++;
				*result_traveller16 = channels[1]->unsigned_short_data;
				result_traveller16++;
				*result_traveller16 = channels[2]->unsigned_short_data;
				result_traveller16++;
				*result_traveller16 = channels[3]->unsigned_short_data;

				result_traveller += 8;
			}
			else
				// RGBA4
				if (n_bits_for_channel_0 == 4 && n_bits_for_channel_1 == 4 && n_bits_for_channel_2 == 4 &&
					n_bits_for_channel_3 == 4)
			{
				unsigned short* result_traveller16 = (unsigned short*)result_traveller;

				*result_traveller16 = (channels[0]->unsigned_byte_data << 12) + (channels[1]->unsigned_byte_data << 8) +
									  (channels[2]->unsigned_byte_data << 4) + channels[3]->unsigned_byte_data;

				result_traveller += 2;
			}
			else
				// RGBA8
				if (n_bits_for_channel_0 == 8 && n_bits_for_channel_1 == 8 && n_bits_for_channel_2 == 8 &&
					n_bits_for_channel_3 == 8)
			{
				*result_traveller = channels[0]->unsigned_byte_data;
				result_traveller++;
				*result_traveller = channels[1]->unsigned_byte_data;
				result_traveller++;
				*result_traveller = channels[2]->unsigned_byte_data;
				result_traveller++;
				*result_traveller = channels[3]->unsigned_byte_data;
				result_traveller++;
			}
			else
				// RGB5A1
				if (n_bits_for_channel_0 == 5 && n_bits_for_channel_1 == 5 && n_bits_for_channel_2 == 5 &&
					n_bits_for_channel_3 == 1)
			{
				unsigned short* result_traveller16 = (unsigned short*)result_traveller;

				*result_traveller16 = (channels[0]->unsigned_byte_data << 11) + (channels[1]->unsigned_byte_data << 6) +
									  (channels[2]->unsigned_byte_data << 1) + channels[3]->unsigned_byte_data;

				result_traveller += 2;
			}
			else
				// RGB10A2_REV
				if (n_bits_for_channel_0 == 2 && n_bits_for_channel_1 == 10 && n_bits_for_channel_2 == 10 &&
					n_bits_for_channel_3 == 10)
			{
				unsigned int* result_traveller32 = (unsigned int*)result_traveller;

				DE_ASSERT(channels[0]->data_type == CHANNEL_DATA_TYPE_UNSIGNED_BYTE_2BITS);
				DE_ASSERT(channels[1]->data_type == CHANNEL_DATA_TYPE_UNSIGNED_SHORT_10BITS);
				DE_ASSERT(channels[2]->data_type == CHANNEL_DATA_TYPE_UNSIGNED_SHORT_10BITS);
				DE_ASSERT(channels[3]->data_type == CHANNEL_DATA_TYPE_UNSIGNED_SHORT_10BITS);

				*result_traveller32 = (channels[0]->unsigned_byte_data << 30) +
									  (channels[1]->unsigned_short_data << 20) +
									  (channels[2]->unsigned_short_data << 10) + channels[3]->unsigned_short_data;

				result_traveller += 4;
			}
			else
			{
				// Unsupported bit layout
				DE_ASSERT(0);
				return false;
			}
		}
		else if (channels[0] != NULL && channels[1] != NULL && channels[2] != NULL)
		{
			// RGB32
			if ((type == GL_HALF_FLOAT) ||
				((n_bits_for_channel_0 == 32) && (n_bits_for_channel_1 == 32) && (n_bits_for_channel_2 == 32)))
			{
				unsigned int* result_traveller32 = (unsigned int*)result_traveller;

				*result_traveller32 = channels[0]->unsigned_integer_data;
				result_traveller32++;
				*result_traveller32 = channels[1]->unsigned_integer_data;
				result_traveller32++;
				*result_traveller32 = channels[2]->unsigned_integer_data;

				result_traveller += 3 * 4;
			}
			else
				// RGB8
				if (n_bits_for_channel_0 == 8 && n_bits_for_channel_1 == 8 && n_bits_for_channel_2 == 8)
			{
				*result_traveller = channels[0]->unsigned_byte_data;
				result_traveller++;
				*result_traveller = channels[1]->unsigned_byte_data;
				result_traveller++;
				*result_traveller = channels[2]->unsigned_byte_data;
				result_traveller++;
			}
			else
				// RGB565
				if (n_bits_for_channel_0 == 5 && n_bits_for_channel_1 == 6 && n_bits_for_channel_2 == 5)
			{
				unsigned short* result_traveller16 = (unsigned short*)result_traveller;

				*result_traveller16 = (channels[0]->unsigned_byte_data << 11) + (channels[1]->unsigned_byte_data << 5) +
									  (channels[2]->unsigned_byte_data);

				result_traveller += 2;
			}
			else
			{
				// Unsupported bit layout
				DE_ASSERT(0);
				return false;
			}
		}
		else if (channels[0] != NULL && channels[1] != NULL)
		{
			// RG32
			if ((type == GL_HALF_FLOAT) || ((n_bits_for_channel_0 == 32) && (n_bits_for_channel_1 == 32)))
			{
				unsigned int* result_traveller32 = (unsigned int*)result_traveller;

				*result_traveller32 = channels[0]->unsigned_integer_data;
				result_traveller32++;
				*result_traveller32 = channels[1]->unsigned_integer_data;

				result_traveller += 8;
			}
			else
				// RG16
				if (n_bits_for_channel_0 == 16 && n_bits_for_channel_1 == 16)
			{
				unsigned short* result_traveller16 = (unsigned short*)result_traveller;

				*result_traveller16 = channels[0]->unsigned_short_data;
				result_traveller16++;
				*result_traveller16 = channels[1]->unsigned_short_data;

				result_traveller += 4;
			}
			else
				// RG8
				if (n_bits_for_channel_0 == 8 && n_bits_for_channel_1 == 8)
			{
				*result_traveller = channels[0]->unsigned_byte_data;
				result_traveller++;
				*result_traveller = channels[1]->unsigned_byte_data;
				result_traveller++;
			}
			else
			{
				// Unsupported bit layout
				DE_ASSERT(0);
				return false;
			}
		}
		else if (channels[0] != NULL)
		{
			// R32
			if (type == GL_HALF_FLOAT || n_bits_for_channel_0 == 32)
			{
				unsigned int* result_traveller32 = (unsigned int*)result_traveller;

				*result_traveller32 = channels[0]->unsigned_integer_data;
				result_traveller += 4;
			}
			else
				// R16
				if (n_bits_for_channel_0 == 16)
			{
				unsigned short* result_traveller16 = (unsigned short*)result_traveller;

				*result_traveller16 = channels[0]->unsigned_short_data;
				result_traveller += 2;
			}
			else
				// R8
				if (n_bits_for_channel_0 == 8)
			{
				*result_traveller = channels[0]->unsigned_byte_data;
				result_traveller++;
			}
			else
			{
				// Unsupported bit layout
				DE_ASSERT(0);
				return false;
			}
		}
		else
		{
			// Unrecognized channel data layout.
			DE_ASSERT(0);
			return false;
		}
	} // for (all pixels)

	return true;
}

/** Retrieves number of bits used for a single channel, were it stored in
 *  @param channel_data_type internal channel data type.
 *
 *  @param channel_data_type Channel data type to consider.
 *  @param out_n_bits		Deref will be used to store the amount of bits.
 *						   Cannot be NULL.
 *
 *  @return GTFtrue if successful, GTFfalse otherwise.
 **/
bool RequiredCase::getNumberOfBitsForChannelDataType(ChannelDataType channel_data_type, int* out_n_bits)
{
	DE_ASSERT(out_n_bits != NULL);
	switch (channel_data_type)
	{
	case CHANNEL_DATA_TYPE_SIGNED_BYTE_8BITS:
		*out_n_bits = 8;
		return true;

	case CHANNEL_DATA_TYPE_SIGNED_INTEGER_32BITS:
		*out_n_bits = 32;
		return true;

	case CHANNEL_DATA_TYPE_SIGNED_SHORT_16BITS:
		*out_n_bits = 16;
		return true;

	case CHANNEL_DATA_TYPE_UNSIGNED_BYTE_1BIT:
		*out_n_bits = 1;
		return true;

	case CHANNEL_DATA_TYPE_UNSIGNED_BYTE_2BITS:
		*out_n_bits = 2;
		return true;

	case CHANNEL_DATA_TYPE_UNSIGNED_BYTE_4BITS:
		*out_n_bits = 4;
		return true;

	case CHANNEL_DATA_TYPE_UNSIGNED_BYTE_5BITS:
		*out_n_bits = 5;
		return true;

	case CHANNEL_DATA_TYPE_UNSIGNED_BYTE_6BITS:
		*out_n_bits = 6;
		return true;

	case CHANNEL_DATA_TYPE_UNSIGNED_BYTE_8BITS:
		*out_n_bits = 8;
		return true;

	case CHANNEL_DATA_TYPE_UNSIGNED_INTEGER_32BITS:
		*out_n_bits = 32;
		return true;

	case CHANNEL_DATA_TYPE_UNSIGNED_SHORT_10BITS:
		*out_n_bits = 10;
		return true;

	case CHANNEL_DATA_TYPE_UNSIGNED_SHORT_16BITS:
		*out_n_bits = 16;
		return true;

	case CHANNEL_DATA_TYPE_FLOAT:
		*out_n_bits = 32;
		return true;

	case CHANNEL_DATA_TYPE_NONE:
		return true;
	}

	// Unrecognized channel data type
	DE_ASSERT(0);
	return false;
}

/** Retrieves information on channel order for user-specified internal format+type
 *  combination.
 *
 *  @param internalformat	GLES internal format to consider.
 *  @param type			  GLES type to consider.
 *  @param out_channel_order Deref will be used to store requested information.
 *						   Cannot be NULL.
 *
 *  @return GTFtrue if successful, GTFfalse otherwise.
 **/
bool RequiredCase::getChannelOrderForInternalformatAndType(GLenum internalformat, GLenum type,
														   ChannelOrder* out_channel_order)
{
	GLenum format = GL_NONE;
	DE_ASSERT(out_channel_order != NULL);

	// Determine the order
	if (!getFormatForInternalformat(internalformat, &format))
	{
		DE_ASSERT(0);
		return false;
	}

	switch (format)
	{
	case GL_RED:
	case GL_RED_INTEGER:
		// Only one order is sane
		*out_channel_order = CHANNEL_ORDER_R;
		return true;

	case GL_RG:
	case GL_RG_INTEGER:
		// Only one order is sane
		*out_channel_order = CHANNEL_ORDER_RG;
		return true;

	case GL_RGB:
	case GL_RGB_INTEGER:
		// Two options here
		if (type == GL_UNSIGNED_INT_10F_11F_11F_REV || type == GL_UNSIGNED_INT_5_9_9_9_REV)
			*out_channel_order = CHANNEL_ORDER_BGR;
		else
			*out_channel_order = CHANNEL_ORDER_RGB;
		return true;

	case GL_RGBA:
	case GL_RGBA_INTEGER:
		// Two options here
		if (type == GL_UNSIGNED_INT_2_10_10_10_REV)
			*out_channel_order = CHANNEL_ORDER_ABGR;
		else
			*out_channel_order = CHANNEL_ORDER_RGBA;
		return true;

	default:
		// Unrecognized format?
		DE_ASSERT(0);
		return false;
	}

	return false;
}

/** Creates objects required to support non color-renderable internalformats of texture objects.
 * There are different objects created for each combination of float/integer/unsigned integer internalformats
 * of source and destination texture objects created.
 *
 * @param f_src_f_dst_internalformat_ptr	Deref will be used to store created object IDs for
 *										  float source and float destination texture object.
 *										  Cannot be NULL.
 * @param i_src_i_dst_internalformat_ptr	Deref will be used to store created object IDs for
 *										  integer source and integer destination texture object.
 *										  Cannot be NULL.
 * @param ui_src_ui_dst_internalformat_ptr  Deref will be used to store created object IDs for
 *										  unsigned integer source and unsigned integer destination texture object.
 *										  Cannot be NULL.
 * @param source_attachment_type			Tells what GL object (or which texture target)
 *										  should be used as a read buffer for a glCopyTexImage2D call.
 * @param destination_attachment_type	   Tells which texture target should be used for
 *										  a glCopyTexImage2D() call.
 *
 * @return true if successful, false otherwise.
 */
bool RequiredCase::generateObjectsToSupportNonColorRenderableInternalformats()
{
	// if (failed to prepare objects for float->float shader-based checks)
	if (!prepareSupportForNonRenderableTexture(m_f_src_f_dst_internalformat, DATA_SAMPLER_FLOAT, DATA_SAMPLER_FLOAT,
											   m_source_attachment_type, m_destination_attachment_type))
	{
		return false;
	}

	// if (failed to prepare objects for int->int shader-based checks)
	if (!prepareSupportForNonRenderableTexture(m_i_src_i_dst_internalformat, DATA_SAMPLER_INTEGER, DATA_SAMPLER_INTEGER,
											   m_source_attachment_type, m_destination_attachment_type))
	{
		return false;
	}

	// if (failed to prepare objects for uint->uint shader-based checks)
	if (!prepareSupportForNonRenderableTexture(m_ui_src_ui_dst_internalformat, DATA_SAMPLER_UNSIGNED_INTEGER,
											   DATA_SAMPLER_UNSIGNED_INTEGER, m_source_attachment_type,
											   m_destination_attachment_type))
	{
		return false;
	}

	return true;
}

/** Creates and prepares buffer and program objects to be used for non-renderable texture support.
 * In case the destination texture's internalformat is not renderable,
 * glReadPixels() cannot be issued to retrieve texture object data.
 * Instead, a program object is used to retrieve and compare source and destination texture data.
 * This function creates and prepares all objects needed to support this approach.
 *
 * @param objects_ptr				 Deref will be used for storing generated object ids. Cannot be NULL.
 * @param src_texture_sampler_type	Type of the sampler to be used for sampling source texture (float/int/uint).
 * @param dst_texture_sampler_type	Type of the sampler to be used for sampling destination texture (float/int/uint).
 * @param source_attachment_type
 * @param destination_attachment_type
 *
 * @return true if the operation succeeded (no error was generated),
 *		 false otherwise.
 */
bool RequiredCase::prepareSupportForNonRenderableTexture(NonRenderableInternalformatSupportObjects& objects,
														 DataSamplerType src_texture_sampler_type,
														 DataSamplerType dst_texture_sampler_type,
														 GLenum			 source_attachment_type,
														 GLenum			 destination_attachment_type)
{
	glu::RenderContext& renderContext = m_context.getRenderContext();
	const Functions&	gl			  = renderContext.getFunctions();

	const GLuint  compare_result_size		   = NUMBER_OF_POINTS_TO_DRAW * sizeof(GLint);
	GLuint		  destination_buffer_data_size = 0;
	GLuint		  source_buffer_data_size	  = 0;
	const GLchar* varying_names[]	 = { "compare_result", "src_texture_pixel_values", "dst_texture_pixel_values" };
	const GLsizei varying_names_count = DE_LENGTH_OF_ARRAY(varying_names);

	// Create program and shader objects.
	objects.program_object_id		  = gl.createProgram();
	objects.fragment_shader_object_id = gl.createShader(GL_FRAGMENT_SHADER);
	objects.vertex_shader_object_id   = gl.createShader(GL_VERTEX_SHADER);

	// Generate buffer and transform feedback objects.
	gl.genTransformFeedbacks(1, &objects.transform_feedback_object_id);
	gl.genBuffers(1, &objects.comparison_result_buffer_object_id);
	gl.genBuffers(1, &objects.src_texture_pixels_buffer_object_id);
	gl.genBuffers(1, &objects.dst_texture_pixels_buffer_object_id);
	gl.genBuffers(1, &objects.src_texture_coordinates_buffer_object_id);
	gl.genBuffers(1, &objects.dst_texture_coordinates_buffer_object_id);

	// Calculate texture data size depending on source and destination sampler types.
	if (!calculateBufferDataSize(src_texture_sampler_type, &source_buffer_data_size))
		return false;
	if (!calculateBufferDataSize(dst_texture_sampler_type, &destination_buffer_data_size))
		return false;

	// Initialize buffer objects storage.
	gl.bindBuffer(GL_ARRAY_BUFFER, objects.comparison_result_buffer_object_id);
	gl.bufferData(GL_ARRAY_BUFFER, compare_result_size, NULL, GL_STATIC_DRAW);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glBufferData");

	gl.bindBuffer(GL_ARRAY_BUFFER, objects.src_texture_pixels_buffer_object_id);
	gl.bufferData(GL_ARRAY_BUFFER, source_buffer_data_size, NULL, GL_STATIC_DRAW);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glBufferData");

	gl.bindBuffer(GL_ARRAY_BUFFER, objects.dst_texture_pixels_buffer_object_id);
	gl.bufferData(GL_ARRAY_BUFFER, destination_buffer_data_size, NULL, GL_STATIC_DRAW);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glBufferData");

	// Initialize texture coordinates
	gl.bindBuffer(GL_ARRAY_BUFFER, objects.src_texture_coordinates_buffer_object_id);
	gl.bufferData(GL_ARRAY_BUFFER, TEXTURE_COORDINATES_ARRAY_SIZE, getTexCoordinates(source_attachment_type),
				  GL_STATIC_DRAW);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glBufferData");

	gl.vertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer");

	gl.bindBuffer(GL_ARRAY_BUFFER, objects.dst_texture_coordinates_buffer_object_id);
	gl.bufferData(GL_ARRAY_BUFFER, TEXTURE_COORDINATES_ARRAY_SIZE, getTexCoordinates(destination_attachment_type),
				  GL_STATIC_DRAW);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glBufferData");

	gl.vertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer");

	gl.bindBuffer(GL_ARRAY_BUFFER, 0);

	// Bind buffer objects to GL_TRANSFORM_FEEDBACK target at specific indices.
	gl.bindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, COMPARISON_RESULT_BUFFER_OBJECT_INDEX,
					   objects.comparison_result_buffer_object_id, 0, compare_result_size);
	gl.bindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, SOURCE_TEXTURE_PIXELS_BUFFER_OBJECT_INDEX,
					   objects.src_texture_pixels_buffer_object_id, 0, source_buffer_data_size);
	gl.bindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, DESTINATION_TEXTURE_PIXELS_BUFFER_OBJECT_INDEX,
					   objects.dst_texture_pixels_buffer_object_id, 0, destination_buffer_data_size);

	// Specify values for transform feedback.
	gl.transformFeedbackVaryings(objects.program_object_id, varying_names_count, varying_names, GL_SEPARATE_ATTRIBS);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glTransformFeedbackVaryings");

	// Prepare program and shader objects.
	if (!prepareProgramAndShaderObjectsToSupportNonRenderableTexture(
			objects.program_object_id, objects.fragment_shader_object_id, objects.vertex_shader_object_id,
			src_texture_sampler_type, dst_texture_sampler_type))
	{
		return false;
	}

	// Retrieve uniform locations.
	if (!getUniformLocations(objects.program_object_id, &objects.src_2D_texture_uniform_location,
							 &objects.src_2DArray_texture_uniform_location, &objects.src_3D_texture_uniform_location,
							 &objects.src_Cube_texture_uniform_location, &objects.dst_2D_texture_uniform_location,
							 &objects.dst_Cube_texture_uniform_location, &objects.channels_to_compare_uniform_location,
							 &objects.samplers_to_use_uniform_location))
	{
		return false;
	}

	return true;
}

/** Calculate size needed for texture object data storage to successfully
 *  capture all the data needed.
 *  For simplicity, we assume all internalformats of our concern use four
 *  components. It's not a dreadful waste of memory, given amount of data
 *  we will be checking for later on anyway.
 *
 * @param _data_sampler_type	 Type of the sampler used to read the data.
 * @param texture_data_size_ptr  Deref will be used to stored calculated result.
 *							   Cannot be NULL.
 *
 * @return true if successful, false otherwise.
 */
bool RequiredCase::calculateBufferDataSize(DataSamplerType sampler_type, GLuint* buffer_data_size_ptr)
{
	if (buffer_data_size_ptr == NULL)
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "NULL pointer passed as a deref to store calculated result."
						   << tcu::TestLog::EndMessage;
		return false;
	}

	switch (sampler_type)
	{
	case DATA_SAMPLER_FLOAT:
		*buffer_data_size_ptr = NUMBER_OF_POINTS_TO_DRAW * NUMBER_OF_ELEMENTS_IN_VEC4 * sizeof(GLfloat);
		return true;

	case DATA_SAMPLER_INTEGER:
		*buffer_data_size_ptr = NUMBER_OF_POINTS_TO_DRAW * NUMBER_OF_ELEMENTS_IN_VEC4 * sizeof(GLint);
		return true;

	case DATA_SAMPLER_UNSIGNED_INTEGER:
		*buffer_data_size_ptr = NUMBER_OF_POINTS_TO_DRAW * NUMBER_OF_ELEMENTS_IN_VEC4 * sizeof(GLuint);
		return true;

	default:
		m_testCtx.getLog() << tcu::TestLog::Message << "Unrecognized data sampler type." << tcu::TestLog::EndMessage;
		return false;
	}

	return true;
}

/** Texture coordinates to use when glReadPixels can't be used to read back the data.
 *  Different coordinates for different attachment types.
 *
 *  @param attachment_type Texture attachment type
 *
 *  @return Array of 4 3-tuples of texture coordinates to use
 */
const float* RequiredCase::getTexCoordinates(GLenum attachment_type) const
{
	static const float texture_coordinates[7][NUMBER_OF_POINTS_TO_DRAW * 4] = {
		// 2D texture, 3D texture and 2D array
		{ 0, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 1, 1, 0 },
		// Cube Map NEGATIVE_X
		{ -1, .99f, -.99f, 0, -1, .99f, .99f, 0, -1, -.99f, .99f, 0, -1, -.99f, -.99f, 0 },
		// Cube Map NEGATIVE_Y
		{ -.99f, -1, .99f, 0, .99f, -1, .99f, 0, .99f, -1, -.99f, 0, -.99f, -1, -.99f, 0 },
		// Cube Map NEGATIVE_Z
		{ .99f, .99f, -1, 0, -.99f, .99f, -1, 0, -.99f, -.99f, -1, 0, .99f, -.99f, -1, 0 },
		// Cube Map POSITIVE_X
		{ 1, .99f, .99f, 0, 1, .99f, -.99f, 0, 1, -.99f, -.99f, 0, 1, -.99f, .99f, 0 },
		// Cube Map POSITIVE_Y
		{ -.99f, 1, -.99f, 0, .99f, 1, -.99f, 0, .99f, 1, .99f, 0, -.99f, 1, .99f, 0 },
		// Cube Map POSITIVE_Z
		{ -.99f, .99f, 1, 0, .99f, .99f, 1, 0, .99f, -.99f, 1, 0, -.99f, -.99f, 1, 0 },
	};

	switch (attachment_type)
	{
	case GL_TEXTURE_2D:
	case GL_TEXTURE_2D_ARRAY:
	case GL_TEXTURE_3D:
		return texture_coordinates[0];
	case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
		return texture_coordinates[1];
	case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
		return texture_coordinates[2];
	case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
		return texture_coordinates[3];
	case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
		return texture_coordinates[4];
	case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
		return texture_coordinates[5];
	case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
		return texture_coordinates[6];
	default:
		DE_ASSERT(!"Invalid attachment type!");
		return NULL;
	}
}

/** Sets source for shader objects, compiles them and attaches to program object.
 * Program object can be used to verify whether copying texture image works correctly if
 * non-renderable internalformats are considered.
 * If all the operations succeeded, the program object is activated.
 *
 * @param program_object_id		 ID of a program object to be initialized.
 *								  The value must be a valid program object ID.
 * @param fragment_shader_object_id ID of a fragment shader object to be initialized.
 *								  The value must be a valid fragment shader object ID.
 * @param vertex_shader_object_id   ID of a vertex shader object to be initialized.
 *								  The value must be a valid vertex shader object ID.
 * @param src_texture_sampler_type  Sampler to be used for sampling source texture object.
 * @param dst_texture_sampler_type  Sampler to be used for sampling destination texture object.
 *
 * @return true if the operation succeeded, false otherwise.
 */
bool RequiredCase::prepareProgramAndShaderObjectsToSupportNonRenderableTexture(GLuint program_object_id,
																			   GLuint fragment_shader_object_id,
																			   GLuint vertex_shader_object_id,
																			   DataSamplerType src_texture_sampler_type,
																			   DataSamplerType dst_texture_sampler_type)
{
	glu::RenderContext& renderContext = m_context.getRenderContext();
	const Functions&	gl			  = renderContext.getFunctions();

	// Attach shader objects to program object.
	gl.attachShader(program_object_id, fragment_shader_object_id);
	gl.attachShader(program_object_id, vertex_shader_object_id);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glAttachShader");

	if (!setSourceForShaderObjectsUsedForNonRenderableTextureSupport(
			fragment_shader_object_id, vertex_shader_object_id, src_texture_sampler_type, dst_texture_sampler_type))
	{
		return false;
	}

	if (!compileAndCheckShaderCompilationStatus(fragment_shader_object_id))
		return false;

	if (!compileAndCheckShaderCompilationStatus(vertex_shader_object_id))
		return false;

	if (!linkAndCheckProgramLinkStatus(program_object_id))
		return false;

	return true;
}

/** Assigns source code to fragment/vertex shaders which will then be used to verify texture data..
 *
 * @param fragment_shader_object_id ID of an already created fragment shader.
 * @param vertex_shader_object_id   ID of an already created vertex shader.
 * @param src_texture_sampler_type  Type of sampler to be used for sampling source texture object (float/int/uint).
 * @param dst_texture_sampler_type  Type of sampler to be used for sampling destination texture object (float/int/uint).
 *
 * @return true if successful, false otherwise.
 */
bool RequiredCase::setSourceForShaderObjectsUsedForNonRenderableTextureSupport(GLuint fragment_shader_object_id,
																			   GLuint vertex_shader_object_id,
																			   DataSamplerType src_texture_sampler_type,
																			   DataSamplerType dst_texture_sampler_type)
{
	glu::RenderContext& renderContext = m_context.getRenderContext();
	const Functions&	gl			  = renderContext.getFunctions();

	std::map<std::string, std::string> specializationMap;

	const GLchar* fragment_shader_source = { "#version 300 es\n"
											 "void main()\n"
											 "{}\n" };
	static std::string shader_source[3];
	const GLchar* vertex_shader_source = NULL;
	const GLchar*	  source				= "#version 300 es\n"
						   "\n"
						   "	 uniform highp ${SAMPLER_PREFIX}sampler2D	  dst_texture2D;\n"
						   "	 uniform highp ${SAMPLER_PREFIX}samplerCube	dst_textureCube;\n"
						   "	 uniform highp ${SAMPLER_PREFIX}sampler2D	  src_texture2D;\n"
						   "	 uniform highp ${SAMPLER_PREFIX}sampler3D	  src_texture3D;\n"
						   "	 uniform highp ${SAMPLER_PREFIX}sampler2DArray src_texture2DArray;\n"
						   "	 uniform highp ${SAMPLER_PREFIX}samplerCube	src_textureCube;\n"
						   "	 uniform int			  channels_to_compare;\n"
						   "	 uniform int			  samplers_to_use;\n"
						   "layout(location = 0) in vec4  dst_texture_coord;\n"
						   "layout(location = 1) in vec4  src_texture_coord;\n"
						   "${OUT_QUALIFIER}   out	 ${OUT_TYPE}		   dst_texture_pixel_values;\n"
						   "${OUT_QUALIFIER}   out	 ${OUT_TYPE}		   src_texture_pixel_values;\n"
						   "flat out	 int			  compare_result;\n"
						   "\n"
						   "void main()\n"
						   "{\n"
						   "	${OUT_TYPE}	  src_texture_data;\n"
						   "	${OUT_TYPE}	  dst_texture_data;\n"
						   "	const ${EPSILON_TYPE}	epsilon		  = ${EPSILON_VALUE};\n"
						   "	int		 result		   = 1;\n"
						   "	bool		compare_red	  = (channels_to_compare & 0x1) != 0;\n"
						   "	bool		compare_green	= (channels_to_compare & 0x2) != 0;\n"
						   "	bool		compare_blue	 = (channels_to_compare & 0x4) != 0;\n"
						   "	bool		compare_alpha	= (channels_to_compare & 0x8) != 0;\n"
						   "	int		 src_sampler	  = samplers_to_use & 0xff;\n"
						   "	int		 dst_sampler	  = samplers_to_use >> 8;\n"
						   "\n"
						   "	if (src_sampler == 0)\n"
						   "	{\n"
						   "		src_texture_data = texture(src_texture2D, src_texture_coord.xy);\n"
						   "	}\n"
						   "	else if (src_sampler == 1)\n"
						   "	{\n"
						   "		src_texture_data = texture(src_texture3D, src_texture_coord.xyz);\n"
						   "	}\n"
						   "	else if (src_sampler == 2)\n"
						   "	{\n"
						   "		src_texture_data = texture(src_texture2DArray, src_texture_coord.xyz);\n"
						   "	}\n"
						   "	else\n"
						   "	{\n"
						   "		src_texture_data = texture(src_textureCube, src_texture_coord.xyz);\n"
						   "	}\n"
						   "\n"
						   "	if (dst_sampler == 0)\n"
						   "	{\n"
						   "		dst_texture_data = texture(dst_texture2D, dst_texture_coord.xy);\n"
						   "	}\n"
						   "	else\n"
						   "	{\n"
						   "		dst_texture_data = texture(dst_textureCube, dst_texture_coord.xyz);\n"
						   "	}\n"
						   "\n"
						   "	if (compare_red && ${FN}(src_texture_data.x - dst_texture_data.x) > epsilon)\n"
						   "	{\n"
						   "		result = 0;\n"
						   "	}\n"
						   "	if (compare_green && ${FN}(src_texture_data.y - dst_texture_data.y) > epsilon)\n"
						   "	{\n"
						   "		result = 0;\n"
						   "	}\n"
						   "	if (compare_blue && ${FN}(src_texture_data.z - dst_texture_data.z) > epsilon)\n"
						   "	{\n"
						   "		result = 0;\n"
						   "	}\n"
						   "	if (compare_alpha && ${FN}(src_texture_data.w - dst_texture_data.w) > epsilon)\n"
						   "	{\n"
						   "		result = 0;\n"
						   "	}\n"
						   "\n"
						   "	compare_result		   = result;\n"
						   "	dst_texture_pixel_values = dst_texture_data;\n"
						   "	src_texture_pixel_values = src_texture_data;\n"
						   "}\n";

	switch (src_texture_sampler_type)
	{
	case DATA_SAMPLER_FLOAT:
	{
		switch (dst_texture_sampler_type)
		{
		case DATA_SAMPLER_FLOAT:
		{
			specializationMap["SAMPLER_PREFIX"] = "  ";
			specializationMap["OUT_QUALIFIER"]  = "  ";
			specializationMap["OUT_TYPE"]		= "  vec4";
			specializationMap["EPSILON_TYPE"]   = "float";
			specializationMap["EPSILON_VALUE"]  = "(1.0/255.0)";
			specializationMap["FN"]				= "abs";
			shader_source[0]					= tcu::StringTemplate(source).specialize(specializationMap);

			vertex_shader_source = shader_source[0].c_str();
			break;
		}

		default:
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "Unrecognized sampler type for destination texture object."
							   << tcu::TestLog::EndMessage;
			return false;
		}
		}

		break;
	}

	case DATA_SAMPLER_INTEGER:
	{
		switch (dst_texture_sampler_type)
		{
		case DATA_SAMPLER_INTEGER:
		{
			specializationMap["SAMPLER_PREFIX"] = "i";
			specializationMap["OUT_QUALIFIER"]  = "flat";
			specializationMap["OUT_TYPE"]		= "ivec4";
			specializationMap["EPSILON_TYPE"]   = "int";
			specializationMap["EPSILON_VALUE"]  = "0";
			specializationMap["FN"]				= "abs";

			shader_source[1]	 = tcu::StringTemplate(source).specialize(specializationMap);
			vertex_shader_source = shader_source[1].c_str();
			break;
		}

		default:
		{
			m_testCtx.getLog() << tcu::TestLog::Message
							   << "Unrecognized type of internalformat of destination texture object."
							   << tcu::TestLog::EndMessage;
			return false;
		}
		}

		break;
	}

	case DATA_SAMPLER_UNSIGNED_INTEGER:
	{
		switch (dst_texture_sampler_type)
		{
		case DATA_SAMPLER_UNSIGNED_INTEGER:
		{
			specializationMap["SAMPLER_PREFIX"] = "u";
			specializationMap["OUT_QUALIFIER"]  = "flat";
			specializationMap["OUT_TYPE"]		= "uvec4";
			specializationMap["EPSILON_TYPE"]   = "uint";
			specializationMap["EPSILON_VALUE"]  = "0u";
			specializationMap["FN"]				= "";

			shader_source[2]	 = tcu::StringTemplate(source).specialize(specializationMap);
			vertex_shader_source = shader_source[2].c_str();
			break;
		}

		default:
		{
			m_testCtx.getLog() << tcu::TestLog::Message
							   << "Unrecognized type of internalformat of destination texture object."
							   << tcu::TestLog::EndMessage;
			return false;
		}
		}

		break;
	}

	default:
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "Unrecognized type of internalformat of source texture object."
						   << tcu::TestLog::EndMessage;
		return false;
	}
	}

	// Set shader source for fragment shader object.
	gl.shaderSource(fragment_shader_object_id, 1 /* part */, &fragment_shader_source, NULL);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glShaderSource");

	// Set shader source for vertex shader object.
	gl.shaderSource(vertex_shader_object_id, 1 /* part */, &vertex_shader_source, NULL);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glShaderSource");

	return true;
}

/** Compiles a shader object and returns compilation status.
 *
 * @param shader_object_id ID of a shader object to be compiled.
 *
 * @return true in case operation succeeded (no error was generated and compilation was successful),
 *		 false otherwise.
 */
bool RequiredCase::compileAndCheckShaderCompilationStatus(GLuint shader_object_id)
{
	glu::RenderContext& renderContext = m_context.getRenderContext();
	const Functions&	gl			  = renderContext.getFunctions();

	// Compile shader object.
	gl.compileShader(shader_object_id);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glCompileShader");

	// Check if compilation was successful.
	GLint shader_compile_status = GL_FALSE;
	gl.getShaderiv(shader_object_id, GL_COMPILE_STATUS, &shader_compile_status);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glGetShaderiv");

	if (GL_FALSE == shader_compile_status)
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "Shader object compilation failed." << tcu::TestLog::EndMessage;

		// Retrieve shader info log in case of failed compilation.
		GLint info_log_length = 0;
		gl.getShaderiv(shader_object_id, GL_INFO_LOG_LENGTH, &info_log_length);
		if (info_log_length != 0)
		{
			std::vector<char> log(info_log_length + 1, 0);
			gl.getShaderInfoLog(shader_object_id, info_log_length, NULL, &log[0]);
			m_testCtx.getLog() << tcu::TestLog::Message << "Shader info log = [" << &log[0] << "]"
							   << tcu::TestLog::EndMessage;
		}

		return false;
	}

	return true;
}

/** Links a program object and returns link status.
 *
 * @param program_object_id ID of a program object to be linked.
 *
 * @return true in case of the operation succeeded (no error was generated and linking end up with success),
 *		 false otherwise.
 */
bool RequiredCase::linkAndCheckProgramLinkStatus(GLuint program_object_id)
{
	glu::RenderContext& renderContext = m_context.getRenderContext();
	const Functions&	gl			  = renderContext.getFunctions();

	gl.linkProgram(program_object_id);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glLinkProgram");

	// Check if link opearation was successful.
	GLint program_link_status = GL_FALSE;
	gl.getProgramiv(program_object_id, GL_LINK_STATUS, &program_link_status);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glGetProgramiv");
	if (GL_FALSE == program_link_status)
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "Program object linking failed." << tcu::TestLog::EndMessage;

		// Retrieve program info log in case of failed linking.
		GLint info_log_length = 0;
		gl.getProgramiv(program_object_id, GL_INFO_LOG_LENGTH, &info_log_length);
		if (info_log_length != 0)
		{
			std::vector<char> log(info_log_length + 1, 0);
			gl.getProgramInfoLog(program_object_id, info_log_length, NULL, &log[0]);
			m_testCtx.getLog() << tcu::TestLog::Message << "Program info log = [" << &log[0] << "]"
							   << tcu::TestLog::EndMessage;
		}

		return false;
	}

	return true;
}

/** Retrieve locations of uniforms (source and destination texture samples)
 * and store them in derefs.
 *
 * @param program_object_id							 ID of a program object for which uniform locations are to be retrieved.
 * @param source_2D_texture_uniform_location_ptr		Deref used to store uniform location for a 2D source texture.
 *													  Cannot be NULL.
 * @param source_2DArray_texture_uniform_location_ptr   Deref used to store uniform location for a 2DArray source texture.
 *													  Cannot be NULL.
 * @param source_3D_texture_uniform_location_ptr		Deref used to store uniform location for a 3D source texture.
 *													  Cannot be NULL.
 * @param source_Cube_texture_uniform_location_ptr	  Deref used to store uniform location for a Cube source texture.
 *													  Cannot be NULL.
 * @param destination_2D_texture_uniform_location_ptr   Deref used to store uniform location for a 2D destination texture.
 *													  Cannot be NULL.
 * @param destination_Cube_texture_uniform_location_ptr Deref used to store uniform location for a Cube destination texture.
 *													  Cannot be NULL.
 * @param channels_to_compare_uniform_location_ptr	  Deref used to store uniform location for a channels_to_compare.
 *													  Cannot be NULL.
 * @param samplers_to_use_uniform_location_ptr		  Deref used to store uniform location for a samplers_to_use.
 *													  Cannot be NULL.
 *
 * @return true if the operation succeeded (no error was generated and valid uniform locations were returned),
 *		 false otherwise.
 */
bool RequiredCase::getUniformLocations(GLuint program_object_id, GLint* source_2D_texture_uniform_location_ptr,
									   GLint* source_2DArray_texture_uniform_location_ptr,
									   GLint* source_3D_texture_uniform_location_ptr,
									   GLint* source_Cube_texture_uniform_location_ptr,
									   GLint* destination_2D_texture_uniform_location_ptr,
									   GLint* destination_Cube_texture_uniform_location_ptr,
									   GLint* channels_to_compare_uniform_location_ptr,
									   GLint* samplers_to_use_uniform_location_ptr)
{
	if (source_2D_texture_uniform_location_ptr == NULL || source_2DArray_texture_uniform_location_ptr == NULL ||
		source_3D_texture_uniform_location_ptr == NULL || source_Cube_texture_uniform_location_ptr == NULL ||
		destination_2D_texture_uniform_location_ptr == NULL || destination_Cube_texture_uniform_location_ptr == NULL ||
		channels_to_compare_uniform_location_ptr == NULL || samplers_to_use_uniform_location_ptr == NULL)
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "NULL pointers passed." << tcu::TestLog::EndMessage;
		return false;
	}

	glu::RenderContext& renderContext = m_context.getRenderContext();
	const Functions&	gl			  = renderContext.getFunctions();

	// Set active program object.
	gl.useProgram(program_object_id);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram");

	GLint destination_2D_texture_uniform_location = -1;
	destination_2D_texture_uniform_location		  = gl.getUniformLocation(program_object_id, "dst_texture2D");
	if (destination_2D_texture_uniform_location == -1)
		return false;

	GLint destination_Cube_texture_uniform_location = -1;
	destination_Cube_texture_uniform_location		= gl.getUniformLocation(program_object_id, "dst_textureCube");
	if (destination_Cube_texture_uniform_location == -1)
		return false;

	GLint source_2D_texture_uniform_location = -1;
	source_2D_texture_uniform_location		 = gl.getUniformLocation(program_object_id, "src_texture2D");
	if (source_2D_texture_uniform_location == -1)
		return false;

	GLint source_2DArray_texture_uniform_location = -1;
	source_2DArray_texture_uniform_location		  = gl.getUniformLocation(program_object_id, "src_texture2DArray");
	if (source_2DArray_texture_uniform_location == -1)
		return false;

	GLint source_3D_texture_uniform_location = -1;
	source_3D_texture_uniform_location		 = gl.getUniformLocation(program_object_id, "src_texture3D");
	if (source_3D_texture_uniform_location == -1)
		return false;

	GLint source_Cube_texture_uniform_location = -1;
	source_Cube_texture_uniform_location	   = gl.getUniformLocation(program_object_id, "src_textureCube");
	if (source_Cube_texture_uniform_location == -1)
		return false;

	GLint channels_to_compare_uniform_location = -1;
	channels_to_compare_uniform_location	   = gl.getUniformLocation(program_object_id, "channels_to_compare");
	if (channels_to_compare_uniform_location == -1)
		return false;

	GLint samplers_to_use_uniform_location = -1;
	samplers_to_use_uniform_location	   = gl.getUniformLocation(program_object_id, "samplers_to_use");
	if (samplers_to_use_uniform_location == -1)
		return false;

	// We are now ready to store retrieved locations.
	*source_2D_texture_uniform_location_ptr		   = source_2D_texture_uniform_location;
	*source_2DArray_texture_uniform_location_ptr   = source_2DArray_texture_uniform_location;
	*source_3D_texture_uniform_location_ptr		   = source_3D_texture_uniform_location;
	*source_Cube_texture_uniform_location_ptr	  = source_Cube_texture_uniform_location;
	*destination_2D_texture_uniform_location_ptr   = destination_2D_texture_uniform_location;
	*destination_Cube_texture_uniform_location_ptr = destination_Cube_texture_uniform_location;
	*channels_to_compare_uniform_location_ptr	  = channels_to_compare_uniform_location;
	*samplers_to_use_uniform_location_ptr		   = samplers_to_use_uniform_location;

	// Restore default settings.
	gl.useProgram(0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram");

	return true;
}

/** Display error message with detailed information.
 *  The function should be issued only when pixel comparison failed.
 *
 * @param src_attachment_type	  Source attachment type.
 * @param dst_attachment_type	  Destination attachment type.
 * @param source_pixel_r		   R channel source pixel value.
 * @param source_pixel_g		   G channel source pixel value.
 * @param source_pixel_b		   B channel source pixel value.
 * @param source_pixel_a		   A channel source pixel value.
 * @param source_internalformat	Source internalformat.
 * @param source_type			  Source type.
 * @param reference_pixel_r		R channel reference pixel value.
 * @param reference_pixel_g		G channel reference pixel value.
 * @param reference_pixel_b		B channel reference pixel value.
 * @param reference_pixel_a		A channel reference pixel value.
 * @param reference_internalformat Reference internalformat.
 * @param reference_type		   Reference type.
 * @param result_pixel_r		   R channel result pixel value.
 * @param result_pixel_g		   G channel result pixel value.
 * @param result_pixel_b		   B channel result pixel value.
 * @param result_pixel_a		   A channel result pixel value.
 * @param result_internalformat	Result internalformat.
 * @param result_type			  Type internalformat.
 * @param max_epsilon_r			Maximum value for an epsilon used for comparison R channel pixel values.
 * @param max_epsilon_g			Maximum value for an epsilon used for comparison G channel pixel values.
 * @param max_epsilon_b			Maximum value for an epsilon used for comparison B channel pixel values.
 * @param max_epsilon_a			Maximum value for an epsilon used for comparison A channel pixel values.
 */
void RequiredCase::displayPixelComparisonFailureMessage(
	GLint source_pixel_r, GLint source_pixel_g, GLint source_pixel_b, GLint source_pixel_a,
	GLenum source_internalformat, GLenum source_type, GLint reference_pixel_r, GLint reference_pixel_g,
	GLint reference_pixel_b, GLint reference_pixel_a, GLenum reference_internalformat, GLenum reference_type,
	GLint result_pixel_r, GLint result_pixel_g, GLint result_pixel_b, GLint result_pixel_a,
	GLenum result_internalformat, GLenum result_type, GLint max_epsilon_r, GLint max_epsilon_g, GLint max_epsilon_b,
	GLint max_epsilon_a)
{
	m_testCtx.getLog() << tcu::TestLog::Message << "Conversion failed for source  ["
					   << getTargetName(m_source_attachment_type) << "] and destination ["
					   << getTargetName(m_destination_attachment_type) << "FBO attachment types."
					   << "\nSource pixel:				 [" << source_pixel_r << ", " << source_pixel_g << ", "
					   << source_pixel_b << ", " << source_pixel_a << "]\nSource internalformat:		["
					   << getInternalformatString(source_internalformat) << "]\nSource type:				  ["
					   << glu::getTypeStr(source_type).toString() << "]\nReference pixel:			  ["
					   << reference_pixel_r << ", " << reference_pixel_g << ", " << reference_pixel_b << ", "
					   << reference_pixel_a << "]\nReference internalformat:	 ["
					   << getInternalformatString(reference_internalformat) << "]\nReference type:			   ["
					   << glu::getTypeStr(reference_type).toString() << "]\nResult pixel:				 ["
					   << result_pixel_r << ", " << result_pixel_g << ", " << result_pixel_b << ", " << result_pixel_a
					   << "]\nResult internalformat:		[" << getInternalformatString(result_internalformat)
					   << "]\nType used for glReadPixels(): [" << glu::getTypeStr(result_type).toString()
					   << "]\nMaximum epsilon:			  [" << max_epsilon_r << ", " << max_epsilon_g << ", "
					   << max_epsilon_b << ", " << max_epsilon_a << "]" << tcu::TestLog::EndMessage;
}

/** Returns sampler type (float/integer/unsigned integer) that should be used for
 *  sampling a texture using data stored in specific internalformat.
 *
 * @param internalformat Internalformat to use for the query.
 *
 * @return Sampler type to9 be used..
 */
DataSamplerType RequiredCase::getDataSamplerTypeForInternalformat(GLenum internalformat)
{
	if (isInternalFormatCompatibleWithFPSampler(internalformat))
		return DATA_SAMPLER_FLOAT;
	else if (isInternalFormatCompatibleWithIntegerSampler(internalformat))
		return DATA_SAMPLER_INTEGER;
	else if (isInternalFormatCompatibleWithUnsignedIntegerSampler(internalformat))
		return DATA_SAMPLER_UNSIGNED_INTEGER;
	else
	{
		// Unrecognized internal format
		DE_ASSERT(0);
	}

	return DATA_SAMPLER_FLOAT;
}

/** Tells whether internal format @param internalformat is compatible with a floating-point
 *  texture sampling function.
 *
 *  @param internalformat GLES internal format to consider.
 *
 *  @return true if yes, false otherwise.
 **/
bool RequiredCase::isInternalFormatCompatibleWithFPSampler(GLenum internalformat)
{
	switch (internalformat)
	{
	// FP texture() GLSL function should be used for sampling textures using
	// the following internalformats
	case GL_ALPHA:
	case GL_ALPHA8_OES:
	case GL_DEPTH_COMPONENT16:
	case GL_DEPTH_COMPONENT24:
	case GL_DEPTH24_STENCIL8:
	case GL_LUMINANCE:
	case GL_LUMINANCE8_OES:
	case GL_LUMINANCE_ALPHA:
	case GL_LUMINANCE8_ALPHA8_OES:
	case GL_R8:
	case GL_R8_SNORM:
	case GL_RG8:
	case GL_RG8_SNORM:
	case GL_RGB:
	case GL_RGB5_A1:
	case GL_RGB10_A2:
	case GL_RGB565:
	case GL_RGB8:
	case GL_RGB8_SNORM:
	case GL_RGBA:
	case GL_RGBA4:
	case GL_RGBA8:
	case GL_RGBA8_SNORM:
	case GL_SRGB8:
	case GL_SRGB8_ALPHA8:

	// These are strictly floating-point internal formats
	case GL_DEPTH_COMPONENT32F:
	case GL_DEPTH32F_STENCIL8:
	case GL_R11F_G11F_B10F:
	case GL_R16F:
	case GL_R32F:
	case GL_RG16F:
	case GL_RG32F:
	case GL_RGB16F:
	case GL_RGB32F:
	case GL_RGB9_E5:
	case GL_RGBA16F:
	case GL_RGBA32F:
		return true;
	}

	return false;
}

/** Tells whether internal format @param internalformat is compatible with integer
 *  texture sampling function.
 *
 *  @param internalformat GLES internal format to consider.
 *
 *  @return true if yes, false otherwise.
 **/
bool RequiredCase::isInternalFormatCompatibleWithIntegerSampler(GLenum internalformat)
{
	switch (internalformat)
	{
	case GL_R16I:
	case GL_R32I:
	case GL_R8I:
	case GL_RG16I:
	case GL_RG32I:
	case GL_RG8I:
	case GL_RGB16I:
	case GL_RGB32I:
	case GL_RGB8I:
	case GL_RGBA16I:
	case GL_RGBA32I:
	case GL_RGBA8I:
		return true;
	}

	return false;
}

/** Tells whether internal format @param internalformat is compatible with unsigned integer
 *  texture sampling function.
 *
 *  @param internalformat GLES internal format to consider.
 *
 *  @return true if yes, false otherwise.
 **/
bool RequiredCase::isInternalFormatCompatibleWithUnsignedIntegerSampler(GLenum internalformat)
{
	switch (internalformat)
	{
	case GL_R16UI:
	case GL_R32UI:
	case GL_R8UI:
	case GL_RG16UI:
	case GL_RG32UI:
	case GL_RG8UI:
	case GL_RGB10_A2UI:
	case GL_RGB16UI:
	case GL_RGB32UI:
	case GL_RGB8UI:
	case GL_RGBA16UI:
	case GL_RGBA32UI:
	case GL_RGBA8UI:
		return true;
	}

	return false;
}

/** Deletes all objects which were created to support non-renderable texture internalformats.
 *
 * @param objects Reference to generated object.
 */
void RequiredCase::destroyObjectsSupportingNonRenderableInternalformats(
	NonRenderableInternalformatSupportObjects& objects)
{
	unbindAndDestroyBufferObject(objects.comparison_result_buffer_object_id);
	unbindAndDestroyBufferObject(objects.src_texture_pixels_buffer_object_id);
	unbindAndDestroyBufferObject(objects.dst_texture_pixels_buffer_object_id);
	unbindAndDestroyBufferObject(objects.src_texture_coordinates_buffer_object_id);
	unbindAndDestroyBufferObject(objects.dst_texture_coordinates_buffer_object_id);
	destroyTransformFeedbackObject(objects.transform_feedback_object_id);
	destroyProgramAndShaderObjects(objects.program_object_id, objects.fragment_shader_object_id,
								   objects.vertex_shader_object_id);

	objects.comparison_result_buffer_object_id		 = 0;
	objects.dst_texture_pixels_buffer_object_id		 = 0;
	objects.dst_2D_texture_uniform_location			 = -1;
	objects.dst_Cube_texture_uniform_location		 = -1;
	objects.fragment_shader_object_id				 = 0;
	objects.transform_feedback_object_id			 = 0;
	objects.program_object_id						 = 0;
	objects.src_2D_texture_uniform_location			 = -1;
	objects.src_2DArray_texture_uniform_location	 = -1;
	objects.src_3D_texture_uniform_location			 = -1;
	objects.src_Cube_texture_uniform_location		 = -1;
	objects.src_texture_pixels_buffer_object_id		 = 0;
	objects.vertex_shader_object_id					 = 0;
	objects.channels_to_compare_uniform_location	 = -1;
	objects.samplers_to_use_uniform_location		 = -1;
	objects.src_texture_coordinates_buffer_object_id = 0;
	objects.dst_texture_coordinates_buffer_object_id = 0;
}

/** Unbind and destroy buffer object which was created for transform feedback purposes.
 *
 * @param bo_id ID of a buffer object (which was created for transform feedback purposes) to be deleted.
 *			  If not zero, it is assumed that the value corresponds to valid buffer object ID.
 */
void RequiredCase::unbindAndDestroyBufferObject(GLuint bo_id)
{
	glu::RenderContext& renderContext = m_context.getRenderContext();
	const Functions&	gl			  = renderContext.getFunctions();

	// Set zero buffer object to be used for GL_TRANSFORM_FEEDBACK_BUFFER.
	gl.bindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, COMPARISON_RESULT_BUFFER_OBJECT_INDEX, 0);
	gl.bindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, SOURCE_TEXTURE_PIXELS_BUFFER_OBJECT_INDEX, 0);
	gl.bindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, DESTINATION_TEXTURE_PIXELS_BUFFER_OBJECT_INDEX, 0);
	gl.bindBuffer(GL_ARRAY_BUFFER, 0);

	if (bo_id != 0)
	{
		gl.deleteBuffers(1, &bo_id);
		GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteBuffers");
	}
}

/** Unbind and destroy transform feedback object.
 *
 * @param transform_feedback_object_id ID of a transform feedback object to be deleted.
 *									 If not zero, it is assumed that the value corresponds
 *									 to valid transform feedback object ID.
 */
void RequiredCase::destroyTransformFeedbackObject(GLuint transform_feedback_object_id)
{
	glu::RenderContext& renderContext = m_context.getRenderContext();
	const Functions&	gl			  = renderContext.getFunctions();

	// Set zero transform feedback object to be used.
	gl.bindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);

	if (transform_feedback_object_id != 0)
	{
		gl.deleteTransformFeedbacks(1, &transform_feedback_object_id);
		GLU_EXPECT_NO_ERROR(gl.getError(), "glDestroyTransformFeedbackObject");
	}
}

/** Destroy program and shader objects.
 *
 * @param program_object_id  ID of a program object to be deleted.
 *						   If not zero, it is assumed that the value corresponds to valid program object ID.
 * @param fragment_shader_id ID of a fragment shader object to be deleted.
 *						   If not zero, it is assumed that the value corresponds to valid shader object ID.
 * @param vertex_shader_id   ID of a vertex shader object to be deleted.
 *						   If not zero, it is assumed that the value corresponds to valid shader object ID.
 */
void RequiredCase::destroyProgramAndShaderObjects(GLuint program_object_id, GLuint fragment_shader_id,
												  GLuint vertex_shader_id)
{
	glu::RenderContext& renderContext = m_context.getRenderContext();
	const Functions&	gl			  = renderContext.getFunctions();

	// Use zero program object.
	gl.useProgram(0);

	// Try to destroy fragment shader object.
	if (fragment_shader_id != 0)
	{
		gl.deleteShader(fragment_shader_id);
		GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteShader");
	}

	// Try to destroy vertex shader object.
	if (vertex_shader_id != 0)
	{
		gl.deleteShader(vertex_shader_id);
		GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteShader");
	}

	// Try to destroy program object.
	if (program_object_id != 0)
	{
		gl.deleteProgram(program_object_id);
		GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteProgram");
	}
}

void RequiredCase::unbindColorAttachments()
{
	glu::RenderContext& renderContext = m_context.getRenderContext();
	const Functions&	gl			  = renderContext.getFunctions();

	switch (m_source_attachment_type)
	{
	case GL_RENDERBUFFER:
		gl.framebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, 0);
		break;
	case GL_TEXTURE_2D_ARRAY:
	case GL_TEXTURE_3D:
		gl.framebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 0, 0, 0);
		break;
	default:
		gl.framebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, m_source_attachment_type, 0, 0);
		break;
	}

	if (gl.getError() != GL_NO_ERROR)
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "Could not unbind texture objects from read/draw framebuffers"
						   << tcu::TestLog::EndMessage;
	}
}

void RequiredCase::restoreBindings(GLenum src_attachment_point, GLenum dst_attachment_point, GLint bound_draw_fbo_id,
								   GLint bound_read_fbo_id)
{
	glu::RenderContext& renderContext = m_context.getRenderContext();
	const Functions&	gl			  = renderContext.getFunctions();

	gl.disableVertexAttribArray(SRC_TEXTURE_COORDS_ATTRIB_INDEX);
	gl.disableVertexAttribArray(DST_TEXTURE_COORDS_ATTRIB_INDEX);

	gl.activeTexture(src_attachment_point);
	gl.bindTexture(getGeneralTargetForDetailedTarget(m_source_attachment_type), 0);
	gl.activeTexture(dst_attachment_point);
	gl.bindTexture(getGeneralTargetForDetailedTarget(m_destination_attachment_type), 0);
	gl.activeTexture(GL_TEXTURE0);

	// Restore previous framebuffer bindings.
	gl.bindFramebuffer(GL_DRAW_FRAMEBUFFER, bound_draw_fbo_id);
	gl.bindFramebuffer(GL_READ_FRAMEBUFFER, bound_read_fbo_id);
}

/* SPECIFICATION:
 *
 * This conformance test verifies that glCopyTexImage2D() implementation does NOT
 * accept internalformats that are incompatible with effective internalformat of
 * current read buffer.
 *
 * The test starts from creating a framebuffer object, which is then bound to
 * GL_READ_FRAMEBUFFER target. It then enters two-level loop:
 *
 * a) First level determines source attachment type: this could either be a 2D texture/cube-map
 *	face mip-map, a specific mip-map of a slice coming from a 2D texture array OR a 3D texture,
 *	or finally a render-buffer. All of these can be bound to an attachment point that is
 *	later pointed to by read buffer configuration.
 * b) Second level configures attachment type of destination. Since glCopyTexImage2D()
 *	specification limits accepted targets, only 2D texture or cube-map face targets are
 *	accepted.
 *
 * For each viable source/destination configuration, the test then enters another two-level loop:
 *
 * I)  First sub-level determines what internal format should be used for the source attachment.
 *	 All texture formats required from a conformant GLES3.0 implementation are iterated over.
 * II) Second sub-level determines internal format that should be passed as a parameter to
 *	 a glCopyTexImage2D() call.
 *
 * For each internal format pair, the test creates and configures a corresponding GL object and
 * attaches it to the read framebuffer. The test also uses a pre-generated texture object that
 * should be re-configured with each glCopyTexImage2D) call.
 *
 * The test then loops over all supported format+type combinations for the internal-format considered
 * and feeds them into actual glCopyTexImage2D() call. Since we're dealing with a negative test, these
 * calls are only made if a source/destination internalformat combination is spec-wise invalid and
 * should result in an error. If the implementation accepts a pair that would require indirect
 * conversions outside scope of the specification, the test should fail.
 */
class ForbiddenCase : public TestBase
{
public:
	ForbiddenCase(deqp::Context& context, GLenum source_attachment_types, GLenum destination_attachment_types);
	virtual ~ForbiddenCase();

	virtual tcu::TestNode::IterateResult iterate(void);

protected:
	bool execute(GLenum src_internal_format, GLenum dst_internal_format, GLuint src_object_id, GLuint dst_object_id);
};

ForbiddenCase::ForbiddenCase(deqp::Context& context, GLenum source_attachment_types,
							 GLenum destination_attachment_types)
	: TestBase(context, source_attachment_types, destination_attachment_types)
{
}

ForbiddenCase::~ForbiddenCase()
{
}

tcu::TestNode::IterateResult ForbiddenCase::iterate(void)
{
	glu::RenderContext& renderContext = m_context.getRenderContext();
	const Functions&	gl			  = renderContext.getFunctions();

	// Create a FBO we will be using throughout the test
	GLuint fbo_id = 0;
	gl.genFramebuffers(1, &fbo_id);

	gl.bindFramebuffer(GL_READ_FRAMEBUFFER, fbo_id);

	// We will be reading from zeroth color attachment
	gl.readBuffer(GL_COLOR_ATTACHMENT0);

	// Make sure the pixel storage is configured accordingly to our data sets
	gl.pixelStorei(GL_UNPACK_ALIGNMENT, 1);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glPixelStorei");

	// Sanity checks
	DE_ASSERT(m_destination_attachment_type == GL_TEXTURE_2D ||
			  m_destination_attachment_type == GL_TEXTURE_CUBE_MAP_NEGATIVE_X ||
			  m_destination_attachment_type == GL_TEXTURE_CUBE_MAP_NEGATIVE_Y ||
			  m_destination_attachment_type == GL_TEXTURE_CUBE_MAP_NEGATIVE_Z ||
			  m_destination_attachment_type == GL_TEXTURE_CUBE_MAP_POSITIVE_X ||
			  m_destination_attachment_type == GL_TEXTURE_CUBE_MAP_POSITIVE_Y ||
			  m_destination_attachment_type == GL_TEXTURE_CUBE_MAP_POSITIVE_Z);

	// Determine general attachment type
	GLenum general_attachment_type = getGeneralTargetForDetailedTarget(m_source_attachment_type);
	if (general_attachment_type == GL_NONE)
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
		return STOP;
	}

	// Set up source object
	GLuint src_object_id = generateGLObject(m_source_attachment_type);
	if (src_object_id == 0)
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
		return STOP;
	}

	// Set up destination object
	GLuint dst_object_id = generateGLObject(m_destination_attachment_type);
	if (dst_object_id == 0)
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
		return STOP;
	}

	// Run through all FBO internal formats
	bool	  result				  = true;
	int		  dstInternalFormatsCount = DE_LENGTH_OF_ARRAY(copyTexImage2DInternalFormatOrdering);
	const int fboInternalFormatsCount = DE_LENGTH_OF_ARRAY(fboEffectiveInternalFormatOrdering);
	for (int fboInternalFormatIndex = 0; fboInternalFormatIndex < fboInternalFormatsCount; ++fboInternalFormatIndex)
	{
		GLenum fboInternalIormat = fboEffectiveInternalFormatOrdering[fboInternalFormatIndex];

		// Run through all destination internal formats
		for (int dstInternalFormatUndex = 0; dstInternalFormatUndex < dstInternalFormatsCount; ++dstInternalFormatUndex)
		{
			GLenum dstInternalFormat = copyTexImage2DInternalFormatOrdering[dstInternalFormatUndex];

			if (!execute(fboInternalIormat, dstInternalFormat, src_object_id, dst_object_id))
			{
				// At least one conversion was invalid or failed. Test should
				// fail, but let's continue iterating over internalformats.
				result = false;
			}
		}
	}

	// Release GL objects before we continue
	if (dst_object_id != 0)
		destroyGLObject(m_destination_attachment_type, dst_object_id);

	if (src_object_id != 0)
		destroyGLObject(m_source_attachment_type, src_object_id);

	if (result)
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
	else
		m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");

	return STOP;
}

/** This function verifies if glCopyTexImage2D() implementation forbids conversions that
 *  are considered forbidden by GLES3.0.3 spec. For more detailed description, please
 *  consult specification of copy_tex_image_conversions_forbidden conformance test.
 *
 *  @param src_internalformat		  GLES internalformat that read buffer should use.
 *  @param src_object_id			   ID of the source GL object of @param source_attachment_type
 *									 type.
 *  @param dst_internalformat		  GLES internalformat that should be used for gl.readPixels() call.
 *									 This should NOT be the expected effective internalformat!
 *  @param dst_object_id			   ID of the destination GL object of
 *									 @param destination_attachment_type type.
 *
 *  @return true if successful, false otherwise.
 */
bool ForbiddenCase::execute(GLenum src_internal_format, GLenum dst_internal_format, GLuint src_object_id,
							GLuint dst_object_id)
{
	// Allocate the max possible size for the texture data (4 compoenents of 4 bytes each)
	static char fbo_data[TEXTURE_WIDTH * TEXTURE_HEIGHT * 4 * 4];
	GLenum		fbo_format							= GL_NONE;
	GLenum		fbo_type							= GL_NONE;
	GLenum		general_destination_attachment_type = getGeneralTargetForDetailedTarget(m_destination_attachment_type);
	int			n_src_pair							= 0;
	bool		result								= true;

	// Sanity checks
	DE_ASSERT(m_destination_attachment_type == GL_TEXTURE_2D ||
			  m_destination_attachment_type == GL_TEXTURE_CUBE_MAP_NEGATIVE_X ||
			  m_destination_attachment_type == GL_TEXTURE_CUBE_MAP_NEGATIVE_Y ||
			  m_destination_attachment_type == GL_TEXTURE_CUBE_MAP_NEGATIVE_Z ||
			  m_destination_attachment_type == GL_TEXTURE_CUBE_MAP_POSITIVE_X ||
			  m_destination_attachment_type == GL_TEXTURE_CUBE_MAP_POSITIVE_Y ||
			  m_destination_attachment_type == GL_TEXTURE_CUBE_MAP_POSITIVE_Z);

	// Skip the internalformat if it's non-renderable and we're trying to set up a renderbuffer source.
	if (m_source_attachment_type == GL_RENDERBUFFER && !isValidRBOInternalFormat(src_internal_format))
		return true;

	// Try using all compatible format+type pairs for
	const Functions& gl = m_context.getRenderContext().getFunctions();
	while (getFormatAndTypeCompatibleWithInternalformat(src_internal_format, n_src_pair, &fbo_format, &fbo_type))
	{
		// Do not test internal formats that are not deemed renderable by GLES implementation we're testing
		if (!isColorRenderableInternalFormat(src_internal_format))
			break;

		// Set up data to be used for source. Note we don't really care much about the data anyway because we want to run
		// negative tests, but in case the conversion is incorrectly allowed, we do not want this fact to be covered by
		// missing source attachment data
		if (!configureGLObject(1, m_source_attachment_type, src_object_id, src_internal_format, fbo_format, fbo_type,
							   fbo_data))
			return false;

		// Good. Check if the conversion is forbidden - if so, we can run a negative test! */
		if (!isFBOEffectiveInternalFormatCompatibleWithDestinationInternalFormat(src_internal_format,
																				 dst_internal_format))
		{
#if 0
				m_testCtx.getLog() << tcu::TestLog::Message
								   << "Testing conversion [" << getInternalformatString(src_internal_format)
								   << "]=>[" << getInternalformatString(dst_internal_format)
								   << "] for source target [" << GetTargetName(m_source_attachment_type)
								   << "] and destination target [" << GetTargetName(m_destination_attachment_type) << "]",
								   << tcu::TestLog::EndMessage;
#endif

			// Ask the implementation to perform the conversion!
			gl.bindTexture(general_destination_attachment_type, dst_object_id);
			gl.copyTexImage2D(m_destination_attachment_type, 0, dst_internal_format, 0 /* x */, 0 /* y */,
							  TEXTURE_WIDTH, TEXTURE_HEIGHT, 0 /* border */);
			gl.bindTexture(general_destination_attachment_type, 0);

			// Has the conversion failed as expected?
			GLenum error_code = gl.getError();
			if (error_code == GL_NO_ERROR)
			{
				m_testCtx.getLog() << tcu::TestLog::Message << "[" << getInternalformatString(src_internal_format)
								   << "]=>[" << getInternalformatString(dst_internal_format)
								   << "] conversion [src target=" << getTargetName(m_source_attachment_type)
								   << ", dst target=" << getTargetName(m_destination_attachment_type)
								   << "] supported contrary to GLES3.0 spec." << tcu::TestLog::EndMessage;
				// This test is now considered failed
				result = false;
			}
			else if (error_code != GL_INVALID_OPERATION)
			{
				m_testCtx.getLog() << tcu::TestLog::Message << "[" << getInternalformatString(src_internal_format)
								   << "]=>[" << getInternalformatString(dst_internal_format)
								   << "] conversion [src target=" << getTargetName(m_source_attachment_type)
								   << ", dst target=" << getTargetName(m_destination_attachment_type) << "] caused ["
								   << error_code << "] error instead of GL_INVALID_OPERATION."
								   << tcu::TestLog::EndMessage;
				// This test is now considered failed
				result = false;
			}
		}

		n_src_pair++;

		// If we're copying from a renderbuffer, we don't really care about compatible format+type pairs, as
		// the effective internalformat is explicitly configured by gl.renderbufferStorage() call.
		if (m_source_attachment_type == GL_RENDERBUFFER)
			break;
	} // for (all compatible format+type pairs)

	return result;
}

CopyTexImageConversionsTests::CopyTexImageConversionsTests(deqp::Context& context)
	: TestCaseGroup(context, "copy_tex_image_conversions", "")
{
}

void CopyTexImageConversionsTests::init()
{
	// Types of objects that can be used as source attachments for conversion process
	const GLenum sourceAttachmentTypes[] = { GL_TEXTURE_2D,
											 GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
											 GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
											 GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
											 GL_TEXTURE_CUBE_MAP_POSITIVE_X,
											 GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
											 GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
											 GL_TEXTURE_2D_ARRAY,
											 GL_TEXTURE_3D,
											 GL_RENDERBUFFER };

	// Types of objects that can be used as destination attachments for conversion process
	const GLenum destinationAttachmentTypes[] = {
		GL_TEXTURE_2D,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
		GL_TEXTURE_CUBE_MAP_POSITIVE_X,
		GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
		GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
	};

	// Set up conversion database
	de::SharedPtr<ConversionDatabase> conversionDatabase(new ConversionDatabase());

	TestCaseGroup* requiredGroup  = new deqp::TestCaseGroup(m_context, "required", "");
	TestCaseGroup* forbiddenGroup = new deqp::TestCaseGroup(m_context, "forbidden", "");
	for (int srcAttachmentIndex = 0; srcAttachmentIndex < DE_LENGTH_OF_ARRAY(sourceAttachmentTypes);
		 ++srcAttachmentIndex)
	{
		GLenum srcAttachmentType = sourceAttachmentTypes[srcAttachmentIndex];
		for (int dstAttachmentIndex = 0; dstAttachmentIndex < DE_LENGTH_OF_ARRAY(destinationAttachmentTypes);
			 ++dstAttachmentIndex)
		{
			GLenum dstAttachmentType = destinationAttachmentTypes[dstAttachmentIndex];
			requiredGroup->addChild(
				new RequiredCase(m_context, conversionDatabase, srcAttachmentType, dstAttachmentType));
			forbiddenGroup->addChild(new ForbiddenCase(m_context, srcAttachmentType, dstAttachmentType));
		}
	}

	addChild(forbiddenGroup);
	addChild(requiredGroup);
}

} // es3cts namespace
