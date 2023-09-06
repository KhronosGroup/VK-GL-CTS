/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2022-2022 The Khronos Group Inc.
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
 */

/*!
 * \file  esextFragmentShadingRateTests.cpp
 * \brief Base test group for fragment shading rate tests
 */ /*-------------------------------------------------------------------*/

#include "esextcFragmentShadingRateTests.hpp"
#include "esextcFragmentShadingRateAPI.hpp"
#include "esextcFragmentShadingRateComplex.hpp"
#include "esextcFragmentShadingRateRenderTarget.hpp"
#include "glwEnums.hpp"

namespace glcts
{

/// Constructor
///
/// @param context       Test context
/// @param extParams   extra parameters
FragmentShadingRateTests::FragmentShadingRateTests(glcts::Context& context, const ExtParameters& extParams)
	: TestCaseGroupBase(context, extParams, "fragment_shading_rate", "Fragment Shading Rate")
{
}

/// Initializes test cases for fragment shading rate tests
void FragmentShadingRateTests::init(void)
{
	// Initialize base class
	TestCaseGroupBase::init();

	// Case 1 - via basic shading rate function
	addChild(new FragmentShadingRateAPI(m_context, m_extParams));

	// Case 2 - fragment shading rate combination cases
	addChild(new FragmentShadingRateComplex(m_context, m_extParams));

	// Case 3 - fragment shading rate attachment cases
	addChild(new FragmentShadingRateRenderTarget(m_context, m_extParams));
}

namespace fsrutils
{

/// Tranlate Primitive ID to packed size of shading rate
///
/// @param shadingRate shading rate enumeration to pack integer
///
/// @return packed shading rate which is combined log of width and height
deUint32 packShadingRate(glw::GLenum shadingRate)
{
	deUint32 width_shift;  // 0, 1, 2
	deUint32 height_shift; // 0, 1, 2

	switch (shadingRate)
	{
	case GL_SHADING_RATE_1X1_PIXELS_EXT:
		width_shift	 = 0;
		height_shift = 0;
		break;
	case GL_SHADING_RATE_1X2_PIXELS_EXT:
		width_shift	 = 0;
		height_shift = 1;
		break;
	case GL_SHADING_RATE_1X4_PIXELS_EXT:
		width_shift	 = 0;
		height_shift = 2;
		break;
	case GL_SHADING_RATE_2X1_PIXELS_EXT:
		width_shift	 = 1;
		height_shift = 0;
		break;
	case GL_SHADING_RATE_2X2_PIXELS_EXT:
		width_shift	 = 1;
		height_shift = 1;
		break;
	case GL_SHADING_RATE_2X4_PIXELS_EXT:
		width_shift	 = 1;
		height_shift = 2;
		break;
	case GL_SHADING_RATE_4X1_PIXELS_EXT:
		width_shift	 = 2;
		height_shift = 0;
		break;
	case GL_SHADING_RATE_4X2_PIXELS_EXT:
		width_shift	 = 2;
		height_shift = 1;
		break;
	case GL_SHADING_RATE_4X4_PIXELS_EXT:
		width_shift	 = 2;
		height_shift = 2;
		break;
	default:
		width_shift	 = 0;
		height_shift = 0;
		DE_ASSERT(0);
		break;
	}

	const deUint32 packedShadingRate = ((width_shift << 2) | height_shift);
	return packedShadingRate;
}

} // namespace fsrutils
} // namespace glcts
