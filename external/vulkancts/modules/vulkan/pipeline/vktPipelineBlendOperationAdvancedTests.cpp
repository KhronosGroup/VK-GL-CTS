/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 Valve Corporation.
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2023 LunarG, Inc.
 * Copyright (c) 2023 Nintendo
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
 * \brief VK_EXT_blend_operation_advanced tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineBlendOperationAdvancedTests.hpp"
#include "vktPipelineImageUtil.hpp"
#include "vktPipelineReferenceRenderer.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkObjUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuImageCompare.hpp"
#include "tcuCommandLine.hpp"

#include <cmath>

namespace vkt
{
namespace pipeline
{

using namespace vk;

namespace
{
using tcu::Vec3;
using tcu::Vec4;

const deUint32 widthArea	= 32u;
const deUint32 heightArea	= 32u;

static const float A1 = 0.750f; // Between 1    and 0.5
static const float A2 = 0.375f; // Between 0.5  and 0.25
static const float A3 = 0.125f; // Between 0.25 and 0.0

const Vec4 srcColors[] = {
					   // Test that pre-multiplied is converted correctly.
					   // Should not test invalid premultiplied colours (1, 1, 1, 0).
					   { 1.000f, 0.750f, 0.500f, 1.00f },
					   { 0.250f, 0.125f, 0.000f, 1.00f },

					   // Test clamping.
					   { 1.000f, 0.750f, 0.500f, 1.00f },
					   { 0.250f, 0.125f, 0.000f, 1.00f },
					   { 1.000f, 0.750f, 0.500f, 1.00f },
					   { 0.250f, 0.125f, 0.000f, 1.00f },

					   // Combinations that test other branches of blend equations.
					   { 1.000f, 0.750f, 0.500f, 1.00f },
					   { 0.250f, 0.125f, 0.000f, 1.00f },
					   { 1.000f, 0.750f, 0.500f, 1.00f },
					   { 0.250f, 0.125f, 0.000f, 1.00f },
					   { 1.000f, 0.750f, 0.500f, 1.00f },
					   { 0.250f, 0.125f, 0.000f, 1.00f },
					   { 1.000f, 0.750f, 0.500f, 1.00f },
					   { 0.250f, 0.125f, 0.000f, 1.00f },
					   { 1.000f, 0.750f, 0.500f, 1.00f },
					   { 0.250f, 0.125f, 0.000f, 1.00f },

					   // Above block with few different pre-multiplied alpha values.
					   { 1.000f * A1, 0.750f * A1, 0.500f * A1, 1.00f * A1},
					   { 0.250f * A1, 0.125f * A1, 0.000f * A1, 1.00f * A1},
					   { 1.000f * A1, 0.750f * A1, 0.500f * A1, 1.00f * A1},
					   { 0.250f * A1, 0.125f * A1, 0.000f * A1, 1.00f * A1},
					   { 1.000f * A1, 0.750f * A1, 0.500f * A1, 1.00f * A1},
					   { 0.250f * A1, 0.125f * A1, 0.000f * A1, 1.00f * A1},
					   { 1.000f * A1, 0.750f * A1, 0.500f * A1, 1.00f * A1},
					   { 0.250f * A1, 0.125f * A1, 0.000f * A1, 1.00f * A1},
					   { 1.000f * A1, 0.750f * A1, 0.500f * A1, 1.00f * A1},
					   { 0.250f * A1, 0.125f * A1, 0.000f * A1, 1.00f * A1},

					   { 1.000f * A2, 0.750f * A2, 0.500f * A2, 1.00f * A2},
					   { 0.250f * A2, 0.125f * A2, 0.000f * A2, 1.00f * A2},
					   { 1.000f * A2, 0.750f * A2, 0.500f * A2, 1.00f * A2},
					   { 0.250f * A2, 0.125f * A2, 0.000f * A2, 1.00f * A2},
					   { 1.000f * A2, 0.750f * A2, 0.500f * A2, 1.00f * A2},
					   { 0.250f * A2, 0.125f * A2, 0.000f * A2, 1.00f * A2},
					   { 1.000f * A2, 0.750f * A2, 0.500f * A2, 1.00f * A2},
					   { 0.250f * A2, 0.125f * A2, 0.000f * A2, 1.00f * A2},
					   { 1.000f * A2, 0.750f * A2, 0.500f * A2, 1.00f * A2},
					   { 0.250f * A2, 0.125f * A2, 0.000f * A2, 1.00f * A2},

					   { 1.000f * A3, 0.750f * A3, 0.500f * A3, 1.00f * A3},
					   { 0.250f * A3, 0.125f * A3, 0.000f * A3, 1.00f * A3},
					   { 1.000f * A3, 0.750f * A3, 0.500f * A3, 1.00f * A3},
					   { 0.250f * A3, 0.125f * A3, 0.000f * A3, 1.00f * A3},
					   { 1.000f * A3, 0.750f * A3, 0.500f * A3, 1.00f * A3},
					   { 0.250f * A3, 0.125f * A3, 0.000f * A3, 1.00f * A3},
					   { 1.000f * A3, 0.750f * A3, 0.500f * A3, 1.00f * A3},
					   { 0.250f * A3, 0.125f * A3, 0.000f * A3, 1.00f * A3},
					   { 1.000f * A3, 0.750f * A3, 0.500f * A3, 1.00f * A3},
					   { 0.250f * A3, 0.125f * A3, 0.000f * A3, 1.00f * A3},

					   // Add some source colors with alpha component that is different than the respective destination color
					   { 0.750f, 0.750f, 0.500f, 0.750f },
					   { 0.250f, 0.500f, 0.500f, 0.750f },
					   { 0.250f, 0.125f, 0.000f, 0.500f },
					   { 0.250f, 0.250f, 0.500f, 0.500f },
					   { 0.250f, 0.125f, 0.000f, 0.250f },
					   { 0.125f, 0.125f, 0.125f, 0.250f }};

const Vec4 dstColors[] = {
					   // Test that pre-multiplied is converted correctly.
					   // Should not test invalid premultiplied colours (1, 1, 1, 0).
					   { 0.000f, 0.000f, 0.000f, 0.00f },
					   { 0.000f, 0.000f, 0.000f, 0.00f },

					   // Test clamping.
					   { -0.125f, -0.125f, -0.125f, 1.00f },
					   { -0.125f, -0.125f, -0.125f, 1.00f },
					   {  1.125f,  1.125f,  1.125f, 1.00f },
					   {  1.125f,  1.125f,  1.125f, 1.00f },

					   // Combinations that test other branches of blend equations.
					   { 1.000f, 1.000f, 1.000f, 1.00f },
					   { 1.000f, 1.000f, 1.000f, 1.00f },
					   { 0.500f, 0.500f, 0.500f, 1.00f },
					   { 0.500f, 0.500f, 0.500f, 1.00f },
					   { 0.250f, 0.250f, 0.250f, 1.00f },
					   { 0.250f, 0.250f, 0.250f, 1.00f },
					   { 0.125f, 0.125f, 0.125f, 1.00f },
					   { 0.125f, 0.125f, 0.125f, 1.00f },
					   { 0.000f, 0.000f, 0.000f, 1.00f },
					   { 0.000f, 0.000f, 0.000f, 1.00f },

					   // Above block with few different pre-multiplied alpha values.
					   { 1.000f * A1, 1.000f * A1, 1.000f * A1, 1.00f * A1},
					   { 1.000f * A1, 1.000f * A1, 1.000f * A1, 1.00f * A1},
					   { 0.500f * A1, 0.500f * A1, 0.500f * A1, 1.00f * A1},
					   { 0.500f * A1, 0.500f * A1, 0.500f * A1, 1.00f * A1},
					   { 0.250f * A1, 0.250f * A1, 0.250f * A1, 1.00f * A1},
					   { 0.250f * A1, 0.250f * A1, 0.250f * A1, 1.00f * A1},
					   { 0.125f * A1, 0.125f * A1, 0.125f * A1, 1.00f * A1},
					   { 0.125f * A1, 0.125f * A1, 0.125f * A1, 1.00f * A1},
					   { 0.000f * A1, 0.000f * A1, 0.000f * A1, 1.00f * A1},
					   { 0.000f * A1, 0.000f * A1, 0.000f * A1, 1.00f * A1},

					   { 1.000f * A2, 1.000f * A2, 1.000f * A2, 1.00f * A2},
					   { 1.000f * A2, 1.000f * A2, 1.000f * A2, 1.00f * A2},
					   { 0.500f * A2, 0.500f * A2, 0.500f * A2, 1.00f * A2},
					   { 0.500f * A2, 0.500f * A2, 0.500f * A2, 1.00f * A2},
					   { 0.250f * A2, 0.250f * A2, 0.250f * A2, 1.00f * A2},
					   { 0.250f * A2, 0.250f * A2, 0.250f * A2, 1.00f * A2},
					   { 0.125f * A2, 0.125f * A2, 0.125f * A2, 1.00f * A2},
					   { 0.125f * A2, 0.125f * A2, 0.125f * A2, 1.00f * A2},
					   { 0.000f * A2, 0.000f * A2, 0.000f * A2, 1.00f * A2},
					   { 0.000f * A2, 0.000f * A2, 0.000f * A2, 1.00f * A2},

					   { 1.000f * A3, 1.000f * A3, 1.000f * A3, 1.00f * A3},
					   { 1.000f * A3, 1.000f * A3, 1.000f * A3, 1.00f * A3},
					   { 0.500f * A3, 0.500f * A3, 0.500f * A3, 1.00f * A3},
					   { 0.500f * A3, 0.500f * A3, 0.500f * A3, 1.00f * A3},
					   { 0.250f * A3, 0.250f * A3, 0.250f * A3, 1.00f * A3 },
					   { 0.250f * A3, 0.250f * A3, 0.250f * A3, 1.00f * A3 },
					   { 0.125f * A3, 0.125f * A3, 0.125f * A3, 1.00f * A3 },
					   { 0.125f * A3, 0.125f * A3, 0.125f * A3, 1.00f * A3 },
					   { 0.000f * A3, 0.000f * A3, 0.000f * A3, 1.00f * A3 },
					   { 0.000f * A3, 0.000f * A3, 0.000f * A3, 1.00f * A3 },

					   // Add some source colors with alpha component that is different than the respective source color
					   { 1.000f, 1.000f, 1.000f, 1.000f },
					   { 0.250f, 0.250f, 0.250f, 0.500f },
					   { 0.500f, 0.500f, 0.500f, 0.750f },
					   { 0.250f, 0.250f, 0.250f, 0.250f },
					   { 0.250f, 0.250f, 0.250f, 0.500f },
					   { 0.125f, 0.125f, 0.125f, 0.125f }};

const	Vec4	clearColorVec4  (1.0f, 1.0f, 1.0f, 1.0f);

enum TestMode
{
	TEST_MODE_GENERIC = 0,
	TEST_MODE_COHERENT = 1,
};

struct BlendOperationAdvancedParam
{
	PipelineConstructionType		pipelineConstructionType;
	TestMode						testMode;
	deUint32						testNumber;
	std::vector<VkBlendOp>			blendOps;
	deBool							coherentOperations;
	deBool							independentBlend;
	deUint32						colorAttachmentsCount;
	VkBool32						premultipliedSrcColor;
	VkBool32						premultipliedDstColor;
	VkBlendOverlapEXT				overlap;
	VkFormat						format;
};

// helper functions
const std::string generateTestName (struct BlendOperationAdvancedParam param)
{
	std::ostringstream result;

	result << ((param.testMode == TEST_MODE_COHERENT && !param.coherentOperations) ? "barrier_" : "");
	result << "color_attachments_" << param.colorAttachmentsCount;
	result << "_" << de::toLower(getBlendOverlapEXTStr(param.overlap).toString().substr(3));
	result << (!param.premultipliedSrcColor ? "_nonpremultipliedsrc" : "");
	result << (!param.premultipliedDstColor ? "_nonpremultiplieddst" : "");
	result << "_" << param.testNumber;
	if (param.format == VK_FORMAT_R8G8B8A8_UNORM)
		result << "_r8g8b8a8_unorm";
	return result.str();
}

const std::string generateTestDescription ()
{
	std::string result("Test advanced blend operations");
	return result;
}

Vec3 calculateWeightingFactors(BlendOperationAdvancedParam param,
									float alphaSrc, float alphaDst)
{
	Vec3 p = Vec3(0.0f, 0.0f, 0.0f);
	switch(param.overlap)
	{
	case VK_BLEND_OVERLAP_UNCORRELATED_EXT:
		p.x() = alphaSrc * alphaDst;
		p.y() = alphaSrc * (1.0f - alphaDst);
		p.z() = alphaDst * (1.0f - alphaSrc);
		break;
	case VK_BLEND_OVERLAP_CONJOINT_EXT:
		p.x() = deFloatMin(alphaSrc, alphaDst);
		p.y() = deFloatMax(alphaSrc - alphaDst, 0.0f);
		p.z() = deFloatMax(alphaDst - alphaSrc, 0.0f);
		break;
	case VK_BLEND_OVERLAP_DISJOINT_EXT:
		p.x() = deFloatMax(alphaSrc + alphaDst - 1.0f, 0.0f);
		p.y() = deFloatMin(alphaSrc, 1.0f - alphaDst);
		p.z() = deFloatMin(alphaDst, 1.0f - alphaSrc);
		break;
	default:
		DE_FATAL("Unsupported Advanced Blend Overlap Mode");
	}
	return p;
}

	Vec3 calculateXYZFactors(VkBlendOp op)
{
	Vec3 xyz = Vec3(0.0f, 0.0f, 0.0f);
	switch (op)
	{
	case VK_BLEND_OP_ZERO_EXT:
		xyz = Vec3(0.0f, 0.0f, 0.0f);
		break;

	case VK_BLEND_OP_DST_ATOP_EXT:
	case VK_BLEND_OP_SRC_EXT:
		xyz = Vec3(1.0f, 1.0f, 0.0f);
		break;

	case VK_BLEND_OP_DST_EXT:
		xyz = Vec3(1.0f, 0.0f, 1.0f);
		break;

	case VK_BLEND_OP_HSL_LUMINOSITY_EXT:
	case VK_BLEND_OP_HSL_COLOR_EXT:
	case VK_BLEND_OP_HSL_SATURATION_EXT:
	case VK_BLEND_OP_HSL_HUE_EXT:
	case VK_BLEND_OP_HARDMIX_EXT:
	case VK_BLEND_OP_PINLIGHT_EXT:
	case VK_BLEND_OP_LINEARLIGHT_EXT:
	case VK_BLEND_OP_VIVIDLIGHT_EXT:
	case VK_BLEND_OP_LINEARBURN_EXT:
	case VK_BLEND_OP_LINEARDODGE_EXT:
	case VK_BLEND_OP_EXCLUSION_EXT:
	case VK_BLEND_OP_DIFFERENCE_EXT:
	case VK_BLEND_OP_SOFTLIGHT_EXT:
	case VK_BLEND_OP_HARDLIGHT_EXT:
	case VK_BLEND_OP_COLORBURN_EXT:
	case VK_BLEND_OP_COLORDODGE_EXT:
	case VK_BLEND_OP_LIGHTEN_EXT:
	case VK_BLEND_OP_DARKEN_EXT:
	case VK_BLEND_OP_OVERLAY_EXT:
	case VK_BLEND_OP_SCREEN_EXT:
	case VK_BLEND_OP_MULTIPLY_EXT:
	case VK_BLEND_OP_SRC_OVER_EXT:
	case VK_BLEND_OP_DST_OVER_EXT:
		xyz = Vec3(1.0f, 1.0f, 1.0f);
		break;

	case VK_BLEND_OP_SRC_IN_EXT:
	case VK_BLEND_OP_DST_IN_EXT:
		xyz = Vec3(1.0f, 0.0f, 0.0f);
		break;

	case VK_BLEND_OP_SRC_OUT_EXT:
		xyz = Vec3(0.0f, 1.0f, 0.0f);
		break;

	case VK_BLEND_OP_DST_OUT_EXT:
		xyz = Vec3(0.0f, 0.0f, 1.0f);
		break;

	case VK_BLEND_OP_INVERT_RGB_EXT:
	case VK_BLEND_OP_INVERT_EXT:
	case VK_BLEND_OP_SRC_ATOP_EXT:
		xyz = Vec3(1.0f, 0.0f, 1.0f);
		break;

	case VK_BLEND_OP_XOR_EXT:
		xyz = Vec3(0.0f, 1.0f, 1.0f);
		break;

	default:
		DE_FATAL("Unsupported f/X/Y/Z Advanced Blend Operations Mode");
	}

	return xyz;
}

float blendOpOverlay(float src, float dst)
{
	if (dst <= 0.5f)
		return (2.0f * src * dst);
	else
		return (1.0f - (2.0f * (1.0f - src) * (1.0f - dst)));
}

float blendOpColorDodge(float src, float dst)
{
	if (dst <= 0.0f)
		return 0.0f;
	else if (src < 1.0f)
		return deFloatMin(1.0f, (dst / (1.0f - src)));
	else
		return 1.0f;
}

float blendOpColorBurn(float src, float dst)
{
	if (dst >= 1.0f)
		return 1.0f;
	else if (src > 0.0f)
		return 1.0f - deFloatMin(1.0f, (1.0f - dst) / src);
	else
		return 0.0f;
}

float blendOpHardlight(float src, float dst)
{
	if (src <= 0.5f)
		return 2.0f * src * dst;
	else
		return 1.0f - (2.0f * (1.0f - src) * (1.0f - dst));
}

float blendOpSoftlight(float src, float dst)
{
	if (src <= 0.5f)
		return dst - ((1.0f - (2.0f * src)) * dst * (1.0f - dst));
	else if (dst <= 0.25f)
		return dst + (((2.0f * src) - 1.0f) * dst * ((((16.0f * dst) - 12.0f) * dst) + 3.0f));
	else
		return dst + (((2.0f * src) - 1.0f) * (deFloatSqrt(dst) - dst));
}

float blendOpLinearDodge(float src, float dst)
{
	if ((src + dst) <= 1.0f)
		return src + dst;
	else
		return 1.0f;
}

float blendOpLinearBurn(float src, float dst)
{
	if ((src + dst) > 1.0f)
		return src + dst - 1.0f;
	else
		return 0.0f;
}

float blendOpVividLight(float src, float dst)
{
	if (src <= 0.0f)
		return 0.0f;
	if (src < 0.5f)
		return 1.0f - (deFloatMin(1.0f, (1.0f - dst) / (2.0f * src)));
	if (src < 1.0f)
		return deFloatMin(1.0f, dst / (2.0f * (1.0f - src)));
	else
		return 1.0f;
}

float blendOpLinearLight(float src, float dst)
{
	if ((2.0f * src + dst) > 2.0f)
		return 1.0f;
	if ((2.0f * src + dst) <= 1.0f)
		return 0.0f;
	return (2.0f * src) + dst - 1.0f;
}

float blendOpPinLight(float src, float dst)
{
	if (((2.0f * src - 1.0f) > dst) && src < 0.5f)
		return 0.0f;
	if (((2.0f * src - 1.0f) > dst) && src >= 0.5f)
		return 2.0f * src - 1.0f;
	if (((2.0f * src - 1.0f) <= dst) && src < (0.5f * dst))
		return 2.0f * src;
	if (((2.0f * src - 1.0f) <= dst) && src >= (0.5f * dst))
		return dst;
	return 0.0f;
}

float blendOpHardmix(float src, float dst)
{
	if ((src + dst) < 1.0f)
		return 0.0f;
	else
		return 1.0f;
}

float minv3(Vec3 c)
{
	return deFloatMin(deFloatMin(c.x(), c.y()), c.z());
}

float maxv3(Vec3 c)
{
	return deFloatMax(deFloatMax(c.x(), c.y()), c.z());
}

float lumv3(Vec3 c)
{
	return dot(c, Vec3(0.3f, 0.59f, 0.11f));
}

float satv3(Vec3 c)
{
	return maxv3(c) - minv3(c);
}

// If any color components are outside [0,1], adjust the color to
// get the components in range.
Vec3 clipColor(Vec3 color)
{
	float lum = lumv3(color);
	float mincol = minv3(color);
	float maxcol = maxv3(color);

	if (mincol < 0.0)
	{
		color = lum + ((color - lum) * lum) / (lum - mincol);
	}
	if (maxcol > 1.0)
	{
		color = lum + ((color - lum) * (1.0f - lum)) / (maxcol - lum);
	}
	return color;
}

// Take the base RGB color <cbase> and override its luminosity
// with that of the RGB color <clum>.
Vec3 setLum(Vec3 cbase, Vec3 clum)
{
	float lbase = lumv3(cbase);
	float llum = lumv3(clum);
	float ldiff = llum - lbase;

	Vec3 color = cbase + Vec3(ldiff);
	return clipColor(color);
}

// Take the base RGB color <cbase> and override its saturation with
// that of the RGB color <csat>.  The override the luminosity of the
// result with that of the RGB color <clum>.
Vec3 setLumSat(Vec3 cbase, Vec3 csat, Vec3 clum)
{
	float minbase = minv3(cbase);
	float sbase = satv3(cbase);
	float ssat = satv3(csat);
	Vec3 color;

	if (sbase > 0)
	{
		// Equivalent (modulo rounding errors) to setting the
		// smallest (R,G,B) component to 0, the largest to <ssat>,
		// and interpolating the "middle" component based on its
		// original value relative to the smallest/largest.
		color = (cbase - minbase) * ssat / sbase;
	} else {
		color = Vec3(0.0f);
	}
	return setLum(color, clum);
}

Vec3 calculateFFunction(VkBlendOp op,
						Vec3 src, Vec3 dst)
{
	Vec3 f = Vec3(0.0f, 0.0f, 0.0f);

	switch (op)
	{
	case VK_BLEND_OP_XOR_EXT:
	case VK_BLEND_OP_SRC_OUT_EXT:
	case VK_BLEND_OP_DST_OUT_EXT:
	case VK_BLEND_OP_ZERO_EXT:
		f = Vec3(0.0f, 0.0f, 0.0f);
		break;

	case VK_BLEND_OP_SRC_ATOP_EXT:
	case VK_BLEND_OP_SRC_IN_EXT:
	case VK_BLEND_OP_SRC_OVER_EXT:
	case VK_BLEND_OP_SRC_EXT:
		f = src;
		break;

	case VK_BLEND_OP_DST_ATOP_EXT:
	case VK_BLEND_OP_DST_IN_EXT:
	case VK_BLEND_OP_DST_OVER_EXT:
	case VK_BLEND_OP_DST_EXT:
		f = dst;
		break;

	case VK_BLEND_OP_MULTIPLY_EXT:
		f = src * dst;
		break;

	case VK_BLEND_OP_SCREEN_EXT:
		f = src + dst - (src*dst);
		break;

	case VK_BLEND_OP_OVERLAY_EXT:
		f.x() = blendOpOverlay(src.x(), dst.x());
		f.y() = blendOpOverlay(src.y(), dst.y());
		f.z() = blendOpOverlay(src.z(), dst.z());
		break;

	case VK_BLEND_OP_DARKEN_EXT:
		f.x() = deFloatMin(src.x(), dst.x());
		f.y() = deFloatMin(src.y(), dst.y());
		f.z() = deFloatMin(src.z(), dst.z());
		break;

	case VK_BLEND_OP_LIGHTEN_EXT:
		f.x() = deFloatMax(src.x(), dst.x());
		f.y() = deFloatMax(src.y(), dst.y());
		f.z() = deFloatMax(src.z(), dst.z());
		break;

	case VK_BLEND_OP_COLORDODGE_EXT:
		f.x() = blendOpColorDodge(src.x(), dst.x());
		f.y() = blendOpColorDodge(src.y(), dst.y());
		f.z() = blendOpColorDodge(src.z(), dst.z());
		break;

	case VK_BLEND_OP_COLORBURN_EXT:
		f.x() = blendOpColorBurn(src.x(), dst.x());
		f.y() = blendOpColorBurn(src.y(), dst.y());
		f.z() = blendOpColorBurn(src.z(), dst.z());
		break;

	case VK_BLEND_OP_HARDLIGHT_EXT:
		f.x() = blendOpHardlight(src.x(), dst.x());
		f.y() = blendOpHardlight(src.y(), dst.y());
		f.z() = blendOpHardlight(src.z(), dst.z());
		break;

	case VK_BLEND_OP_SOFTLIGHT_EXT:
		f.x() = blendOpSoftlight(src.x(), dst.x());
		f.y() = blendOpSoftlight(src.y(), dst.y());
		f.z() = blendOpSoftlight(src.z(), dst.z());
		break;

	case VK_BLEND_OP_DIFFERENCE_EXT:
		f.x() = deFloatAbs(dst.x() - src.x());
		f.y() = deFloatAbs(dst.y() - src.y());
		f.z() = deFloatAbs(dst.z() - src.z());
		break;


	case VK_BLEND_OP_EXCLUSION_EXT:
		f = src + dst - (2.0f * src * dst);
		break;

	case VK_BLEND_OP_INVERT_EXT:
		f = 1.0f - dst;
		break;

	case VK_BLEND_OP_INVERT_RGB_EXT:
		f = src * (1.0f - dst);
		break;

	case VK_BLEND_OP_LINEARDODGE_EXT:
		f.x() = blendOpLinearDodge(src.x(), dst.x());
		f.y() = blendOpLinearDodge(src.y(), dst.y());
		f.z() = blendOpLinearDodge(src.z(), dst.z());
		break;

	case VK_BLEND_OP_LINEARBURN_EXT:
		f.x() = blendOpLinearBurn(src.x(), dst.x());
		f.y() = blendOpLinearBurn(src.y(), dst.y());
		f.z() = blendOpLinearBurn(src.z(), dst.z());
		break;

	case VK_BLEND_OP_VIVIDLIGHT_EXT:
		f.x() = blendOpVividLight(src.x(), dst.x());
		f.y() = blendOpVividLight(src.y(), dst.y());
		f.z() = blendOpVividLight(src.z(), dst.z());
		break;

	case VK_BLEND_OP_LINEARLIGHT_EXT:
		f.x() = blendOpLinearLight(src.x(), dst.x());
		f.y() = blendOpLinearLight(src.y(), dst.y());
		f.z() = blendOpLinearLight(src.z(), dst.z());
		break;

	case VK_BLEND_OP_PINLIGHT_EXT:
		f.x() = blendOpPinLight(src.x(), dst.x());
		f.y() = blendOpPinLight(src.y(), dst.y());
		f.z() = blendOpPinLight(src.z(), dst.z());
		break;

	case VK_BLEND_OP_HARDMIX_EXT:
		f.x() = blendOpHardmix(src.x(), dst.x());
		f.y() = blendOpHardmix(src.y(), dst.y());
		f.z() = blendOpHardmix(src.z(), dst.z());
		break;

	case VK_BLEND_OP_HSL_HUE_EXT:
		f = setLumSat(src, dst, dst);
		break;

	case VK_BLEND_OP_HSL_SATURATION_EXT:
		f = setLumSat(dst, src, dst);
		break;

	case VK_BLEND_OP_HSL_COLOR_EXT:
		f = setLum(src, dst);
		break;

	case VK_BLEND_OP_HSL_LUMINOSITY_EXT:
		f = setLum(dst, src);
		break;

	default:
		DE_FATAL("Unsupported f/X/Y/Z Advanced Blend Operations Mode");
	}

	return f;
}

Vec4 additionalRGBBlendOperations(VkBlendOp op,
								  Vec4 src, Vec4 dst)
{
	Vec4 res = Vec4(0.0f, 0.0f, 0.0f, 1.0f);

	switch (op)
	{
	case VK_BLEND_OP_PLUS_EXT:
		res = src + dst;
		break;

	case VK_BLEND_OP_PLUS_CLAMPED_EXT:
		res.x() = deFloatMin(1.0f, src.x() + dst.x());
		res.y() = deFloatMin(1.0f, src.y() + dst.y());
		res.z() = deFloatMin(1.0f, src.z() + dst.z());
		res.w() = deFloatMin(1.0f, src.w() + dst.w());
		break;

	case VK_BLEND_OP_PLUS_CLAMPED_ALPHA_EXT:
		res.x() = deFloatMin(deFloatMin(1.0f, src.w() + dst.w()), src.x() + dst.x());
		res.y() = deFloatMin(deFloatMin(1.0f, src.w() + dst.w()), src.y() + dst.y());
		res.z() = deFloatMin(deFloatMin(1.0f, src.w() + dst.w()), src.z() + dst.z());
		res.w() = deFloatMin(1.0f, src.w() + dst.w());
		break;

	case VK_BLEND_OP_PLUS_DARKER_EXT:
		res.x() = deFloatMax(0.0f, deFloatMin(1.0f, src.w() + dst.w()) - ((src.w() - src.x()) + (dst.w() - dst.x())));
		res.y() = deFloatMax(0.0f, deFloatMin(1.0f, src.w() + dst.w()) - ((src.w() - src.y()) + (dst.w() - dst.y())));
		res.z() = deFloatMax(0.0f, deFloatMin(1.0f, src.w() + dst.w()) - ((src.w() - src.z()) + (dst.w() - dst.z())));
		res.w() = deFloatMin(1.0f, src.w() + dst.w());
		break;

	case VK_BLEND_OP_MINUS_EXT:
		res = dst - src;
		break;

	case VK_BLEND_OP_MINUS_CLAMPED_EXT:
		res.x() = deFloatMax(0.0f, dst.x() - src.x());
		res.y() = deFloatMax(0.0f, dst.y() - src.y());
		res.z() = deFloatMax(0.0f, dst.z() - src.z());
		res.w() = deFloatMax(0.0f, dst.w() - src.w());
		break;

	case VK_BLEND_OP_CONTRAST_EXT:
		res.x() = (dst.w() / 2.0f) + 2.0f * (dst.x() - (dst.w() / 2.0f)) * (src.x() - (src.w() / 2.0f));
		res.y() = (dst.w() / 2.0f) + 2.0f * (dst.y() - (dst.w() / 2.0f)) * (src.y() - (src.w() / 2.0f));
		res.z() = (dst.w() / 2.0f) + 2.0f * (dst.z() - (dst.w() / 2.0f)) * (src.z() - (src.w() / 2.0f));
		res.w() = dst.w();
		break;

	case VK_BLEND_OP_INVERT_OVG_EXT:
		res.x() = src.w() * (1.0f - dst.x()) + (1.0f - src.w()) * dst.x();
		res.y() = src.w() * (1.0f - dst.y()) + (1.0f - src.w()) * dst.y();
		res.z() = src.w() * (1.0f - dst.z()) + (1.0f - src.w()) * dst.z();
		res.w() = src.w() + dst.w() - src.w() * dst.w();
		break;

	case VK_BLEND_OP_RED_EXT:
		res = dst;
		res.x() = src.x();
		break;

	case VK_BLEND_OP_GREEN_EXT:
		res = dst;
		res.y() = src.y();
		break;

	case VK_BLEND_OP_BLUE_EXT:
		res = dst;
		res.z() = src.z();
		break;

	default:
		DE_FATAL("Unsupported blend operation");
	}
	return res;
}

Vec4 calculateFinalColor(BlendOperationAdvancedParam param, VkBlendOp op,
						 Vec4 source, Vec4 destination)
{
	Vec4 result = Vec4(0.0f, 0.0f, 0.0f, 1.0f);
	Vec3 srcColor = source.xyz();
	Vec3 dstColor = destination.xyz();

	// Calculate weighting factors
	Vec3 p = calculateWeightingFactors(param, source.w(), destination.w());

	if (op > VK_BLEND_OP_MAX && op < VK_BLEND_OP_PLUS_EXT)
	{
		{
			// If srcPremultiplied is set to VK_TRUE, the fragment color components
			// are considered to have been premultiplied by the A component prior to
			// blending. The base source color (Rs',Gs',Bs') is obtained by dividing
			// through by the A component.
			if (param.premultipliedSrcColor)
			{
				if (source.w() != 0.0f)
					srcColor = srcColor / source.w();
				else
					srcColor = Vec3(0.0f, 0.0f, 0.0f);
			}
			// If dstPremultiplied is set to VK_TRUE, the destination components are
			// considered to have been premultiplied by the A component prior to
			// blending. The base destination color (Rd',Gd',Bd') is obtained by dividing
			// through by the A component.
			if (param.premultipliedDstColor)
			{
				if (destination.w() != 0.0f)
					dstColor = dstColor / destination.w();
				else
					dstColor = Vec3(0.0f, 0.0f, 0.0f);
			}
		}

		// Calculate X, Y, Z terms of the equation
		Vec3 xyz = calculateXYZFactors(op);
		Vec3 fSrcDst = calculateFFunction(op, srcColor, dstColor);

		result.x() = fSrcDst.x() * p.x() + xyz.y() * srcColor.x() * p.y() + xyz.z() * dstColor.x() * p.z();
		result.y() = fSrcDst.y() * p.x() + xyz.y() * srcColor.y() * p.y() + xyz.z() * dstColor.y() * p.z();
		result.z() = fSrcDst.z() * p.x() + xyz.y() * srcColor.z() * p.y() + xyz.z() * dstColor.z() * p.z();
		result.w() = xyz.x() * p.x() + xyz.y() * p.y() + xyz.z() * p.z();
	}
	else if (op >= VK_BLEND_OP_PLUS_EXT && op < VK_BLEND_OP_MAX_ENUM)
	{
		// Premultiply colors for additional RGB blend operations. The formula is different than the rest of operations.
		{
			if (!param.premultipliedSrcColor)
			{
				srcColor = srcColor * source.w();
			}

			if (!param.premultipliedDstColor)
			{
				dstColor = dstColor * destination.w();
			}

		}
		Vec4 src = Vec4(srcColor.x(), srcColor.y(), srcColor.z(), source.w());
		Vec4 dst = Vec4(dstColor.x(), dstColor.y(), dstColor.z(), destination.w());
		result = additionalRGBBlendOperations(op, src, dst);
	}
	else
	{
		DE_FATAL("Unsupported Blend Operation");
	}
	return result;
}

static inline void getCoordinates (deUint32 index, deInt32 &x, deInt32 &y)
{
	x = index % widthArea;
	y = index / heightArea;
}

static inline std::vector<Vec4> createPoints (void)
{
	std::vector<Vec4> vertices;
	vertices.push_back(Vec4(-1.0f, -1.0f, 0.0f, 1.0f));
	vertices.push_back(Vec4( 1.0f,  1.0f, 0.0f, 1.0f));
	vertices.push_back(Vec4(-1.0f,  1.0f, 0.0f, 1.0f));
	vertices.push_back(Vec4(-1.0f, -1.0f, 0.0f, 1.0f));
	vertices.push_back(Vec4( 1.0f,  1.0f, 0.0f, 1.0f));
	vertices.push_back(Vec4( 1.0f, -1.0f, 0.0f, 1.0f));
	return vertices;
}

template <class Test>
vkt::TestCase* newTestCase (tcu::TestContext&					testContext,
							const BlendOperationAdvancedParam	testParam)
{
	return new Test(testContext,
					generateTestName(testParam).c_str(),
					generateTestDescription().c_str(),
					testParam);
}

RenderPassWrapper makeTestRenderPass (BlendOperationAdvancedParam			param,
									  const DeviceInterface&				vk,
									  const VkDevice						device,
									  const VkFormat						colorFormat,
									  VkAttachmentLoadOp					colorLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR)
{
	const VkAttachmentDescription			colorAttachmentDescription			=
	{
		(VkAttachmentDescriptionFlags)0,				// VkAttachmentDescriptionFlags		flags
		colorFormat,									// VkFormat							format
		VK_SAMPLE_COUNT_1_BIT,							// VkSampleCountFlagBits			samples
		colorLoadOp,									// VkAttachmentLoadOp				loadOp
		VK_ATTACHMENT_STORE_OP_STORE,					// VkAttachmentStoreOp				storeOp
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,				// VkAttachmentLoadOp				stencilLoadOp
		VK_ATTACHMENT_STORE_OP_DONT_CARE,				// VkAttachmentStoreOp				stencilStoreOp
		(colorLoadOp == VK_ATTACHMENT_LOAD_OP_LOAD) ?
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL :
			VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout					initialLayout
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL		// VkImageLayout					finalLayout
	};

	std::vector<VkAttachmentDescription>	attachmentDescriptions;
	std::vector<VkAttachmentReference>		colorAttachmentRefs;


	for (deUint32 i = 0; i < param.colorAttachmentsCount; i++)
	{
		attachmentDescriptions.push_back(colorAttachmentDescription);
		const VkAttachmentReference		colorAttachmentRef	=
		{
			i,											// deUint32		attachment
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout	layout
		};

		colorAttachmentRefs.push_back(colorAttachmentRef);
	}

	const VkSubpassDescription				subpassDescription					=
	{
		(VkSubpassDescriptionFlags)0,							// VkSubpassDescriptionFlags		flags
		VK_PIPELINE_BIND_POINT_GRAPHICS,						// VkPipelineBindPoint				pipelineBindPoint
		0u,														// deUint32							inputAttachmentCount
		DE_NULL,												// const VkAttachmentReference*		pInputAttachments
		param.colorAttachmentsCount,							// deUint32							colorAttachmentCount
		colorAttachmentRefs.data(),								// const VkAttachmentReference*		pColorAttachments
		DE_NULL,												// const VkAttachmentReference*		pResolveAttachments
		DE_NULL,												// const VkAttachmentReference*		pDepthStencilAttachment
		0u,														// deUint32							preserveAttachmentCount
		DE_NULL													// const deUint32*					pPreserveAttachments
	};

	const VkRenderPassCreateInfo			renderPassInfo						=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,									// VkStructureType					sType
		DE_NULL,																	// const void*						pNext
		(VkRenderPassCreateFlags)0,													// VkRenderPassCreateFlags			flags
		(deUint32)attachmentDescriptions.size(),									// deUint32							attachmentCount
		attachmentDescriptions.data(),												// const VkAttachmentDescription*	pAttachments
		1u,																			// deUint32							subpassCount
		&subpassDescription,														// const VkSubpassDescription*		pSubpasses
		0u,																			// deUint32							dependencyCount
		DE_NULL																		// const VkSubpassDependency*		pDependencies
	};

	return RenderPassWrapper(param.pipelineConstructionType, vk, device, &renderPassInfo);
}

Move<VkBuffer> createBufferAndBindMemory (Context& context, VkDeviceSize size, VkBufferUsageFlags usage, de::MovePtr<Allocation>* pAlloc)
{
	const DeviceInterface&	vk				 = context.getDeviceInterface();
	const VkDevice			vkDevice		 = context.getDevice();
	const deUint32			queueFamilyIndex = context.getUniversalQueueFamilyIndex();

	const VkBufferCreateInfo vertexBufferParams =
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
		DE_NULL,									// const void*			pNext;
		0u,											// VkBufferCreateFlags	flags;
		size,										// VkDeviceSize			size;
		usage,										// VkBufferUsageFlags	usage;
		VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
		1u,											// deUint32				queueFamilyCount;
		&queueFamilyIndex							// const deUint32*		pQueueFamilyIndices;
	};

	Move<VkBuffer> vertexBuffer = createBuffer(vk, vkDevice, &vertexBufferParams);

	*pAlloc = context.getDefaultAllocator().allocate(getBufferMemoryRequirements(vk, vkDevice, *vertexBuffer), MemoryRequirement::HostVisible);
	VK_CHECK(vk.bindBufferMemory(vkDevice, *vertexBuffer, (*pAlloc)->getMemory(), (*pAlloc)->getOffset()));

	return vertexBuffer;
}

Move<VkImage> createImage2DAndBindMemory (Context&							context,
										  VkFormat							format,
										  deUint32							width,
										  deUint32							height,
										  VkImageUsageFlags					usage,
										  VkSampleCountFlagBits				sampleCount,
										  de::details::MovePtr<Allocation>* pAlloc)
{
	const DeviceInterface&	vk				 = context.getDeviceInterface();
	const VkDevice			vkDevice		 = context.getDevice();
	const deUint32			queueFamilyIndex = context.getUniversalQueueFamilyIndex();

	const VkImageCreateInfo colorImageParams =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,										// VkStructureType		sType;
		DE_NULL,																	// const void*			pNext;
		0u,																			// VkImageCreateFlags	flags;
		VK_IMAGE_TYPE_2D,															// VkImageType			imageType;
		format,																		// VkFormat				format;
		{ width, height, 1u },														// VkExtent3D			extent;
		1u,																			// deUint32				mipLevels;
		1u,																			// deUint32				arraySize;
		sampleCount,																// deUint32				samples;
		VK_IMAGE_TILING_OPTIMAL,													// VkImageTiling		tiling;
		usage,																		// VkImageUsageFlags	usage;
		VK_SHARING_MODE_EXCLUSIVE,													// VkSharingMode		sharingMode;
		1u,																			// deUint32				queueFamilyCount;
		&queueFamilyIndex,															// const deUint32*		pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,													// VkImageLayout		initialLayout;
	};

	Move<VkImage> image = createImage(vk, vkDevice, &colorImageParams);

	*pAlloc = context.getDefaultAllocator().allocate(getImageMemoryRequirements(vk, vkDevice, *image), MemoryRequirement::Any);
	VK_CHECK(vk.bindImageMemory(vkDevice, *image, (*pAlloc)->getMemory(), (*pAlloc)->getOffset()));

	return image;
}

// Test Classes
class BlendOperationAdvancedTestInstance : public vkt::TestInstance
{
public:
								BlendOperationAdvancedTestInstance		(Context&				context,
																		 const BlendOperationAdvancedParam	param);
	virtual						~BlendOperationAdvancedTestInstance		(void);
	virtual tcu::TestStatus		iterate									(void);
protected:
			void				prepareRenderPass						(const GraphicsPipelineWrapper& pipeline) const;
			void				prepareCommandBuffer					(void) const;
			void				buildPipeline							(VkBool32 premultiplySrc, VkBool32 premultiplyDst);
			deBool				verifyTestResult						(void);
protected:
	const BlendOperationAdvancedParam		m_param;
	const tcu::UVec2						m_renderSize;
	const VkFormat							m_colorFormat;
	PipelineLayoutWrapper					m_pipelineLayout;

	Move<VkBuffer>							m_vertexBuffer;
	de::MovePtr<Allocation>					m_vertexBufferMemory;
	std::vector<Vec4>						m_vertices;

	RenderPassWrapper						m_renderPass;
	Move<VkCommandPool>						m_cmdPool;
	Move<VkCommandBuffer>					m_cmdBuffer;
	std::vector<Move<VkImage>>				m_colorImages;
	std::vector<Move<VkImageView>>			m_colorAttachmentViews;
	std::vector<de::MovePtr<Allocation>>	m_colorImageAllocs;
	std::vector<VkImageMemoryBarrier>		m_imageLayoutBarriers;
	GraphicsPipelineWrapper					m_pipeline;

	ShaderWrapper							m_shaderModules[2];
};

void BlendOperationAdvancedTestInstance::buildPipeline (VkBool32 srcPremultiplied,
													   VkBool32 dstPremultiplied)
{
	const DeviceInterface&			vk			= m_context.getDeviceInterface();
	const VkDevice					vkDevice	= m_context.getDevice();

	const std::vector<VkRect2D>		scissor		{ makeRect2D(m_renderSize) };
	const std::vector<VkViewport>	viewport	{ makeViewport(m_renderSize) };

	const VkPipelineColorBlendAdvancedStateCreateInfoEXT blendAdvancedStateParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_ADVANCED_STATE_CREATE_INFO_EXT,	// VkStructureType		sType;
		DE_NULL,																// const void*			pNext;
		srcPremultiplied,														// VkBool32				srcPremultiplied;
		dstPremultiplied,														// VkBool32				dstPremultiplied;
		m_param.overlap,														// VkBlendOverlapEXT	blendOverlap;
	};

	std::vector<VkPipelineColorBlendAttachmentState>	colorBlendAttachmentStates;

	for (deUint32 i = 0; i < m_param.colorAttachmentsCount; i++)
	{
		const VkPipelineColorBlendAttachmentState colorBlendAttachmentState =
		{
			VK_TRUE,														// VkBool32									blendEnable;
			VK_BLEND_FACTOR_ONE,											// VkBlendFactor							srcColorBlendFactor;
			VK_BLEND_FACTOR_ONE,											// VkBlendFactor							dstColorBlendFactor;
			m_param.blendOps[i],											// VkBlendOp								colorBlendOp;
			VK_BLEND_FACTOR_ONE,											// VkBlendFactor							srcAlphaBlendFactor;
			VK_BLEND_FACTOR_ONE,											// VkBlendFactor							dstAlphaBlendFactor;
			m_param.blendOps[i],											// VkBlendOp								alphaBlendOp;
			VK_COLOR_COMPONENT_R_BIT |
			VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT |
			VK_COLOR_COMPONENT_A_BIT										// VkColorComponentFlags					colorWriteMask;
		};
		colorBlendAttachmentStates.emplace_back(colorBlendAttachmentState);
	}

	const VkPipelineColorBlendStateCreateInfo colorBlendStateParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType								sType;
		&blendAdvancedStateParams,									// const void*									pNext;
		0u,															// VkPipelineColorBlendStateCreateFlags			flags;
		VK_FALSE,													// VkBool32										logicOpEnable;
		VK_LOGIC_OP_COPY,											// VkLogicOp									logicOp;
		(deUint32)colorBlendAttachmentStates.size(),				// deUint32										attachmentCount;
		colorBlendAttachmentStates.data(),							// const VkPipelineColorBlendAttachmentState*	pAttachments;
		{ 0.0f, 0.0f, 0.0f, 0.0f },									// float										blendConst[4];
	};

	const VkPipelineMultisampleStateCreateInfo  multisampleStateParams	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType								sType;
		DE_NULL,													// const void*									pNext;
		0u,															// VkPipelineMultisampleStateCreateFlags		flags;
		VK_SAMPLE_COUNT_1_BIT,										// VkSampleCountFlagBits						rasterizationSamples;
		VK_FALSE,													// VkBool32										sampleShadingEnable;
		0.0f,														// float										minSampleShading;
		DE_NULL,													// const VkSampleMask*							pSampleMask;
		VK_FALSE,													// VkBool32										alphaToCoverageEnable;
		VK_FALSE,													// VkBool32										alphaToOneEnable;
	};

	VkPipelineDepthStencilStateCreateInfo depthStencilStateParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, // VkStructureType								sType;
		DE_NULL,													// const void*									pNext;
		0u,															// VkPipelineDepthStencilStateCreateFlags		flags;
		VK_FALSE,													// VkBool32										depthTestEnable;
		VK_FALSE,													// VkBool32										depthWriteEnable;
		VK_COMPARE_OP_NEVER,										// VkCompareOp									depthCompareOp;
		VK_FALSE,													// VkBool32										depthBoundsTestEnable;
		VK_FALSE,													// VkBool32										stencilTestEnable;
		// VkStencilOpState front;
		{
			VK_STENCIL_OP_KEEP,		// VkStencilOp	failOp;
			VK_STENCIL_OP_KEEP,		// VkStencilOp	passOp;
			VK_STENCIL_OP_KEEP,		// VkStencilOp	depthFailOp;
			VK_COMPARE_OP_NEVER,	// VkCompareOp	compareOp;
			0u,						// deUint32		compareMask;
			0u,						// deUint32		writeMask;
			0u,						// deUint32		reference;
		},
		// VkStencilOpState back;
		{
			VK_STENCIL_OP_KEEP,		// VkStencilOp	failOp;
			VK_STENCIL_OP_KEEP,		// VkStencilOp	passOp;
			VK_STENCIL_OP_KEEP,		// VkStencilOp	depthFailOp;
			VK_COMPARE_OP_NEVER,	// VkCompareOp	compareOp;
			0u,						// deUint32		compareMask;
			0u,						// deUint32		writeMask;
			0u,						// deUint32		reference;
		},
		0.0f,														// float										minDepthBounds;
		1.0f,														// float										maxDepthBounds;
	};

	const VkDynamicState dynamicState = VK_DYNAMIC_STATE_SCISSOR;
	const VkPipelineDynamicStateCreateInfo dynamicStateParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,	// VkStructureType						sType;
		DE_NULL,												// const void*							pNext;
		0u,														// VkPipelineDynamicStateCreateFlags	flags;
		1u,														// uint32_t								dynamicStateCount;
		&dynamicState											// const VkDynamicState*				pDynamicStates;
	};

	m_shaderModules[0] = ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("vert"), 0);
	m_shaderModules[1] = ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("frag"), 0);

	m_pipeline.setDynamicState(&dynamicStateParams)
			  .setDefaultRasterizationState()
			  .setupVertexInputState()
			  .setupPreRasterizationShaderState(viewport, scissor, m_pipelineLayout, *m_renderPass, 0u, m_shaderModules[0])
			  .setupFragmentShaderState(m_pipelineLayout, *m_renderPass, 0u, m_shaderModules[1], &depthStencilStateParams, &multisampleStateParams)
			  .setupFragmentOutputState(*m_renderPass, 0u, &colorBlendStateParams, &multisampleStateParams)
			  .setMonolithicPipelineLayout(m_pipelineLayout)
			  .buildPipeline();
}

void BlendOperationAdvancedTestInstance::prepareRenderPass (const GraphicsPipelineWrapper& pipeline) const
{
	const DeviceInterface&	vk				 = m_context.getDeviceInterface();

	std::vector<VkClearValue>	attachmentClearValues;

	for (deUint32 i = 0; i < m_param.colorAttachmentsCount; i++)
		attachmentClearValues.emplace_back(makeClearValueColor(clearColorVec4));

	m_renderPass.begin(vk, *m_cmdBuffer, makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y()),
					m_param.colorAttachmentsCount, attachmentClearValues.data());
	pipeline.bind(*m_cmdBuffer);
	VkDeviceSize offsets = 0u;
	vk.cmdBindVertexBuffers(*m_cmdBuffer, 0u, 1u, &m_vertexBuffer.get(), &offsets);

	// Draw all colors
	deUint32 skippedColors = 0u;
	for (deUint32 color = 0; color < DE_LENGTH_OF_ARRAY(srcColors); color++)
	{
		// Skip ill-formed colors when we have non-premultiplied destination colors.
		if (m_param.premultipliedDstColor == VK_FALSE)
		{
			deBool skipColor = false;
			for (deUint32 i = 0; i < m_param.colorAttachmentsCount; i++)
			{
				Vec4 calculatedColor = calculateFinalColor(m_param, m_param.blendOps[i], srcColors[color], dstColors[color]);
				if (calculatedColor.w() <= 0.0f && calculatedColor != Vec4(0.0f))
				{
					// Skip ill-formed colors, because the spec says the result is undefined.
					skippedColors++;
					skipColor = true;
					break;
				}
			}
			if (skipColor)
				continue;
		}

		deInt32 x = 0;
		deInt32 y = 0;
		getCoordinates(color, x, y);

		// Set source color as push constant
		vk.cmdPushConstants(*m_cmdBuffer, *m_pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0u, sizeof(Vec4), &srcColors[color]);

		VkRect2D scissor = makeRect2D(x, y, 1u, 1u);
		if (vk::isConstructionTypeShaderObject(m_param.pipelineConstructionType))
		{
#ifndef CTS_USES_VULKANSC
			vk.cmdSetScissorWithCount(*m_cmdBuffer, 1u, &scissor);
#else
			vk.cmdSetScissorWithCountEXT(*m_cmdBuffer, 1u, &scissor);
#endif
		}
		else
		{
			vk.cmdSetScissor(*m_cmdBuffer, 0u, 1u, &scissor);
		}

		// To set destination color, we do clear attachment restricting the area to the respective pixel of each color attachment.
		{
			// Set destination color as push constant.
			std::vector<VkClearAttachment> attachments;
			VkClearValue clearValue = vk::makeClearValueColorVec4(dstColors[color]);

			for (deUint32 i = 0; i < m_param.colorAttachmentsCount; i++)
			{
				VkClearAttachment	attachment	=
				{
					VK_IMAGE_ASPECT_COLOR_BIT,
					i,
					clearValue
				};
				attachments.emplace_back(attachment);
			}

			const VkClearRect rect =
			{
				scissor,
				0u,
				1u
			};
			vk.cmdClearAttachments(*m_cmdBuffer, (deUint32)attachments.size(), attachments.data(), 1u, &rect);
		}

		// Draw
		vk.cmdDraw(*m_cmdBuffer, (deUint32)m_vertices.size(), 1u, 0u, 0u);
	}

	// If we break this assert, then we are not testing anything in this test.
	DE_ASSERT(skippedColors < DE_LENGTH_OF_ARRAY(srcColors));

	// Log number of skipped colors
	if (skippedColors != 0u)
	{
		tcu::TestLog& log = m_context.getTestContext().getLog();
		log << tcu::TestLog::Message << "Skipped " << skippedColors << " out of " << DE_LENGTH_OF_ARRAY(srcColors) << " color cases due to ill-formed colors" << tcu::TestLog::EndMessage;
	}
	m_renderPass.end(vk, *m_cmdBuffer);
}

void BlendOperationAdvancedTestInstance::prepareCommandBuffer () const
{
	const DeviceInterface&	vk				 = m_context.getDeviceInterface();


	beginCommandBuffer(vk, *m_cmdBuffer, 0u);

	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, (VkDependencyFlags)0,
						  0u, DE_NULL, 0u, DE_NULL, (deUint32)m_imageLayoutBarriers.size(), m_imageLayoutBarriers.data());

	prepareRenderPass(m_pipeline);

	endCommandBuffer(vk, *m_cmdBuffer);
}

BlendOperationAdvancedTestInstance::BlendOperationAdvancedTestInstance	(Context&							context,
																		 const BlendOperationAdvancedParam	param)
	: TestInstance			(context)
	, m_param				(param)
	, m_renderSize			(tcu::UVec2(widthArea, heightArea))
	, m_colorFormat			(param.format)
	, m_pipeline			(m_context.getInstanceInterface(), m_context.getDeviceInterface(), m_context.getPhysicalDevice(), m_context.getDevice(), m_context.getDeviceExtensions(), param.pipelineConstructionType)
{
	const DeviceInterface&		vk				 = m_context.getDeviceInterface();
	const VkDevice				vkDevice		 = m_context.getDevice();
	const deUint32				queueFamilyIndex = context.getUniversalQueueFamilyIndex();

	// Create vertex buffer and upload data
	{
		// Load vertices into vertex buffer
		m_vertices		= createPoints();
		DE_ASSERT((deUint32)m_vertices.size() == 6);

		m_vertexBuffer	= createBufferAndBindMemory(m_context, m_vertices.size() * sizeof(Vec4), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &m_vertexBufferMemory);
		deMemcpy(m_vertexBufferMemory->getHostPtr(), m_vertices.data(), m_vertices.size() * sizeof(Vec4));
		flushAlloc(vk, vkDevice, *m_vertexBufferMemory);
	}

	// Create render pass
	m_renderPass = makeTestRenderPass(param, vk, vkDevice, m_colorFormat);

	const VkComponentMapping	componentMappingRGBA = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};

	// Create color images
	for (deUint32 i = 0; i < param.colorAttachmentsCount; i++)
	{
		de::MovePtr<Allocation>	colorImageAlloc;
		m_colorImageAllocs.emplace_back(colorImageAlloc);

		Move<VkImage>			colorImage	= createImage2DAndBindMemory(m_context,
																		 m_colorFormat,
																		 m_renderSize.x(),
																		 m_renderSize.y(),
																		 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
																		 VK_SAMPLE_COUNT_1_BIT,
																		 &m_colorImageAllocs.back());
		m_colorImages.emplace_back(colorImage);

		// Set up image layout transition barriers
		{
			VkImageMemoryBarrier colorImageBarrier =
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,					// VkStructureType			sType;
				DE_NULL,												// const void*				pNext;
				0u,														// VkAccessFlags			srcAccessMask;
				(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
				 VK_ACCESS_COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT),	// VkAccessFlags			dstAccessMask;
				VK_IMAGE_LAYOUT_UNDEFINED,								// VkImageLayout			oldLayout;
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,				// VkImageLayout			newLayout;
				VK_QUEUE_FAMILY_IGNORED,								// deUint32					srcQueueFamilyIndex;
				VK_QUEUE_FAMILY_IGNORED,								// deUint32					dstQueueFamilyIndex;
				*m_colorImages.back(),									// VkImage					image;
				{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u },			// VkImageSubresourceRange	subresourceRange;
			};

			m_imageLayoutBarriers.emplace_back(colorImageBarrier);
		}

		// Create color attachment view
		{
			VkImageViewCreateInfo colorAttachmentViewParams =
			{
				VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		// VkStructureType			sType;
				DE_NULL,										// const void*				pNext;
				0u,												// VkImageViewCreateFlags	flags;
				*m_colorImages.back(),							// VkImage					image;
				VK_IMAGE_VIEW_TYPE_2D,							// VkImageViewType			viewType;
				m_colorFormat,									// VkFormat					format;
				componentMappingRGBA,							// VkComponentMapping		components;
				{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u },	// VkImageSubresourceRange	subresourceRange;
			};

			m_colorAttachmentViews.emplace_back(createImageView(vk, vkDevice, &colorAttachmentViewParams));
		}
	}

	// Create framebuffer
	{
		std::vector<VkImage>		images;
		std::vector<VkImageView>	imageViews;

		for (auto& movePtr : m_colorImages)
			images.push_back(movePtr.get());

		for (auto& movePtr : m_colorAttachmentViews)
			imageViews.push_back(movePtr.get());

		const VkFramebufferCreateInfo framebufferParams =
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,			// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			0u,													// VkFramebufferCreateFlags		flags;
			*m_renderPass,										// VkRenderPass					renderPass;
			(deUint32)imageViews.size(),						// deUint32						attachmentCount;
			imageViews.data(),									// const VkImageView*			pAttachments;
			(deUint32)m_renderSize.x(),							// deUint32						width;
			(deUint32)m_renderSize.y(),							// deUint32						height;
			1u,													// deUint32						layers;
		};

		m_renderPass.createFramebuffer(vk, vkDevice, &framebufferParams, images);
	}


	// Create pipeline layout
	{
		const VkPushConstantRange pushConstantRange =
		{
			VK_SHADER_STAGE_FRAGMENT_BIT,		// VkShaderStageFlags	stageFlags
			0,									// deUint32				offset
			sizeof(Vec4)						// deUint32				size
		};

		const VkPipelineLayoutCreateInfo pipelineLayoutParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType					sType;
			DE_NULL,											// const void*						pNext;
			0u,													// VkPipelineLayoutCreateFlags		flags;
			0u,													// deUint32							setLayoutCount;
			DE_NULL,											// const VkDescriptorSetLayout*		pSetLayouts;
			1u,													// deUint32							pushConstantRangeCount;
			&pushConstantRange									// const VkPushConstantRange*		pPushConstantRanges;
		};

		m_pipelineLayout = PipelineLayoutWrapper(m_param.pipelineConstructionType, vk, vkDevice, &pipelineLayoutParams);
	}

	// Create pipeline
	buildPipeline(m_param.premultipliedSrcColor, m_param.premultipliedDstColor);

	// Create command pool
	m_cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);

	// Create command buffer
	m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
}

BlendOperationAdvancedTestInstance::~BlendOperationAdvancedTestInstance (void)
{
}

tcu::TestStatus BlendOperationAdvancedTestInstance::iterate (void)
{
	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	const VkDevice			vkDevice			= m_context.getDevice();
	const VkQueue			queue				= m_context.getUniversalQueue();
	tcu::TestLog&			log					= m_context.getTestContext().getLog();

	// Log the blend operations to test
	{
		if (m_param.independentBlend)
		{
			for (deUint32 i = 0; (i < m_param.colorAttachmentsCount); i++)
				log << tcu::TestLog::Message << "Color attachment " << i << " uses depth op: "<< de::toLower(getBlendOpStr(m_param.blendOps[i]).toString().substr(3)) << tcu::TestLog::EndMessage;

		}
		else
		{
			log << tcu::TestLog::Message << "All color attachments use depth op: " << de::toLower(getBlendOpStr(m_param.blendOps[0]).toString().substr(3)) << tcu::TestLog::EndMessage;

		}
	}
	prepareCommandBuffer();
	submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());

	if (verifyTestResult() == DE_FALSE)
		return tcu::TestStatus::fail("Image mismatch");

	return tcu::TestStatus::pass("Result images matches references");
}

deBool BlendOperationAdvancedTestInstance::verifyTestResult ()
{
	deBool							compareOk			= DE_TRUE;
	const DeviceInterface&			vk					= m_context.getDeviceInterface();
	const VkDevice					vkDevice			= m_context.getDevice();
	const VkQueue					queue				= m_context.getUniversalQueue();
	const deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	Allocator&						allocator			= m_context.getDefaultAllocator();
	std::vector<tcu::TextureLevel>	referenceImages;

	for (deUint32 colorAtt = 0; colorAtt < m_param.colorAttachmentsCount; colorAtt++)
	{
		tcu::TextureLevel		refImage			(vk::mapVkFormat(m_colorFormat), 32, 32);
		tcu::clear(refImage.getAccess(), clearColorVec4);
		referenceImages.emplace_back(refImage);
	}

	for (deUint32 color = 0; color < DE_LENGTH_OF_ARRAY(srcColors); color++)
	{
		deBool skipColor = DE_FALSE;

		// Check if any color attachment will generate an ill-formed color. If that's the case, skip that color in the verification.
		for (deUint32 colorAtt = 0; colorAtt < m_param.colorAttachmentsCount; colorAtt++)
		{
			Vec4 rectColor = calculateFinalColor(m_param, m_param.blendOps[colorAtt], srcColors[color], dstColors[color]);

			if (m_param.premultipliedDstColor == VK_FALSE)
			{
				if (rectColor.w() > 0.0f)
				{
					rectColor.x() = rectColor.x() / rectColor.w();
					rectColor.y() = rectColor.y() / rectColor.w();
					rectColor.z() = rectColor.z() / rectColor.w();
				}
				else
				{
					// Skip the color check if it is ill-formed.
					if (rectColor != Vec4(0.0f))
					{
						skipColor = DE_TRUE;
						break;
					}
				}
			}

			// If pixel value is not normal (inf, nan, denorm), skip it
			if (!std::isnormal(rectColor.x()) ||
				!std::isnormal(rectColor.y()) ||
				!std::isnormal(rectColor.z()) ||
				!std::isnormal(rectColor.w()))
				skipColor = DE_TRUE;
		}

		// Skip ill-formed colors that appears in any color attachment.
		if (skipColor)
			continue;

		// If we reach this point, the final color for all color attachment is not ill-formed.
		for (deUint32 colorAtt = 0; colorAtt < m_param.colorAttachmentsCount; colorAtt++)
		{
			Vec4 rectColor = calculateFinalColor(m_param, m_param.blendOps[colorAtt], srcColors[color], dstColors[color]);
			if (m_param.premultipliedDstColor == VK_FALSE)
			{
				if (rectColor.w() > 0.0f)
				{
					rectColor.x() = rectColor.x() / rectColor.w();
					rectColor.y() = rectColor.y() / rectColor.w();
					rectColor.z() = rectColor.z() / rectColor.w();
				}
				else
				{
					// Ill-formed colors were already skipped
					DE_ASSERT(rectColor == Vec4(0.0f));
				}
			}
			deInt32 x = 0;
			deInt32 y = 0;
			getCoordinates(color, x, y);
			tcu::clear(tcu::getSubregion(referenceImages[colorAtt].getAccess(), x, y, 1u, 1u), rectColor);
		}
	}

	for (deUint32 colorAtt = 0; colorAtt < m_param.colorAttachmentsCount; colorAtt++)
	{
		// Compare image
		de::MovePtr<tcu::TextureLevel> result = vkt::pipeline::readColorAttachment(vk, vkDevice, queue, queueFamilyIndex, allocator, *m_colorImages[colorAtt], m_colorFormat, m_renderSize);
		std::ostringstream name;
		name << "Image comparison. Color attachment: "  << colorAtt << ". Depth op: " << de::toLower(getBlendOpStr(m_param.blendOps[colorAtt]).toString().substr(3));

		// R8G8B8A8 threshold was derived experimentally.
		compareOk = tcu::floatThresholdCompare(m_context.getTestContext().getLog(),
											   "FloatImageCompare",
											   name.str().c_str(),
											   referenceImages[colorAtt].getAccess(),
											   result->getAccess(),
											   clearColorVec4,
											   m_colorFormat == VK_FORMAT_R8G8B8A8_UNORM ? Vec4(0.15f, 0.15f, 0.15f, 0.13f) : Vec4(0.01f, 0.01f, 0.01f, 0.01f),
											   tcu::COMPARE_LOG_RESULT);
#ifdef CTS_USES_VULKANSC
		if (m_context.getTestContext().getCommandLine().isSubProcess())
#endif // CTS_USES_VULKANSC
		{
			if (!compareOk)
				return DE_FALSE;
		}
	}
	return DE_TRUE;
}

class BlendOperationAdvancedTest : public vkt::TestCase
{
public:
							BlendOperationAdvancedTest	(tcu::TestContext&					testContext,
														 const std::string&					name,
														 const std::string&					description,
														 const BlendOperationAdvancedParam	param)
								: vkt::TestCase (testContext, name, description)
								, m_param		(param)
								{ }
	virtual					~BlendOperationAdvancedTest	(void) { }
	virtual void			initPrograms		(SourceCollections&	programCollection) const;
	virtual TestInstance*	createInstance		(Context&				context) const;
	virtual void			checkSupport		(Context& context) const;

protected:
		const BlendOperationAdvancedParam       m_param;
};

void BlendOperationAdvancedTest::checkSupport(Context& context) const
{
	const InstanceInterface&	vki				 = context.getInstanceInterface();

	context.requireDeviceFunctionality("VK_EXT_blend_operation_advanced");

	VkPhysicalDeviceBlendOperationAdvancedPropertiesEXT blendProperties;
	blendProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BLEND_OPERATION_ADVANCED_PROPERTIES_EXT;
	blendProperties.pNext = DE_NULL;

	VkPhysicalDeviceProperties2 properties2;
	properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	properties2.pNext = &blendProperties;
	vki.getPhysicalDeviceProperties2(context.getPhysicalDevice(), &properties2);

	if (!blendProperties.advancedBlendAllOperations)
	{
		for (deUint32 index = 0u; index < m_param.blendOps.size(); index++)
		{
			switch (m_param.blendOps[index])
			{
			case VK_BLEND_OP_MULTIPLY_EXT:
			case VK_BLEND_OP_SCREEN_EXT:
			case VK_BLEND_OP_OVERLAY_EXT:
			case VK_BLEND_OP_DARKEN_EXT:
			case VK_BLEND_OP_LIGHTEN_EXT:
			case VK_BLEND_OP_COLORDODGE_EXT:
			case VK_BLEND_OP_COLORBURN_EXT:
			case VK_BLEND_OP_HARDLIGHT_EXT:
			case VK_BLEND_OP_SOFTLIGHT_EXT:
			case VK_BLEND_OP_DIFFERENCE_EXT:
			case VK_BLEND_OP_EXCLUSION_EXT:
			case VK_BLEND_OP_HSL_HUE_EXT:
			case VK_BLEND_OP_HSL_SATURATION_EXT:
			case VK_BLEND_OP_HSL_COLOR_EXT:
			case VK_BLEND_OP_HSL_LUMINOSITY_EXT:
				break;
			default:
				throw tcu::NotSupportedError("Unsupported all advanced blend operations and unsupported advanced blend operation");
			}
		}
	}

	if (m_param.colorAttachmentsCount > blendProperties.advancedBlendMaxColorAttachments)
	{
		std::ostringstream error;
		error << "Unsupported number of color attachments (" << blendProperties.advancedBlendMaxColorAttachments << " < " << m_param.colorAttachmentsCount;
		throw tcu::NotSupportedError(error.str().c_str());
	}

	if (m_param.overlap != VK_BLEND_OVERLAP_UNCORRELATED_EXT && !blendProperties.advancedBlendCorrelatedOverlap)
	{
		throw tcu::NotSupportedError("Unsupported blend correlated overlap");
	}

	if (m_param.colorAttachmentsCount > 1 && m_param.independentBlend && !blendProperties.advancedBlendIndependentBlend)
	{
		throw tcu::NotSupportedError("Unsupported independent blend");
	}

	if (!m_param.premultipliedSrcColor && !blendProperties.advancedBlendNonPremultipliedSrcColor)
	{
		throw tcu::NotSupportedError("Unsupported non-premultiplied source color");
	}

	if (!m_param.premultipliedDstColor && !blendProperties.advancedBlendNonPremultipliedDstColor)
	{
		throw tcu::NotSupportedError("Unsupported non-premultiplied destination color");
	}

	const VkPhysicalDeviceBlendOperationAdvancedFeaturesEXT blendFeatures = context.getBlendOperationAdvancedFeaturesEXT();
	if (m_param.coherentOperations && !blendFeatures.advancedBlendCoherentOperations)
	{
		throw tcu::NotSupportedError("Unsupported required coherent operations");
	}
	checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_param.pipelineConstructionType);
}

void BlendOperationAdvancedTest::initPrograms (SourceCollections& programCollection) const
{
	programCollection.glslSources.add("vert") << glu::VertexSource(
				"#version 310 es\n"
				"layout(location = 0) in vec4 position;\n"
				"void main (void)\n"
				"{\n"
				"  gl_Position = position;\n"
				"}\n");

	std::ostringstream fragmentSource;
	fragmentSource << "#version 310 es\n";
	fragmentSource << "layout(push_constant) uniform Color { highp vec4 color; };\n";
	for (deUint32 i = 0; i < m_param.colorAttachmentsCount; i++)
		fragmentSource << "layout(location = "<< i <<") out highp vec4 fragColor" << i <<";\n";
	fragmentSource << "void main (void)\n";
	fragmentSource << "{\n";
	for (deUint32 i = 0; i < m_param.colorAttachmentsCount; i++)
		fragmentSource << "  fragColor" << i <<" = color;\n";
	fragmentSource << "}\n";
	programCollection.glslSources.add("frag") << glu::FragmentSource(fragmentSource.str().c_str());
}

class BlendOperationAdvancedTestCoherentInstance : public vkt::TestInstance
{
public:
								BlendOperationAdvancedTestCoherentInstance		(Context&				context,
																				 const BlendOperationAdvancedParam	param);
	virtual						~BlendOperationAdvancedTestCoherentInstance		(void);
	virtual tcu::TestStatus		iterate									(void);
protected:
			void				prepareRenderPass						(GraphicsPipelineWrapper& pipeline,
																		 RenderPassWrapper& renderpass, deBool secondDraw);
	virtual	void				prepareCommandBuffer					(void);
	virtual	void				buildPipeline							(void);
	virtual	tcu::TestStatus		verifyTestResult						(void);

protected:
	const BlendOperationAdvancedParam		m_param;
	const tcu::UVec2						m_renderSize;
	const VkFormat							m_colorFormat;
	PipelineLayoutWrapper					m_pipelineLayout;

	Move<VkBuffer>							m_vertexBuffer;
	de::MovePtr<Allocation>					m_vertexBufferMemory;
	std::vector<Vec4>						m_vertices;

	std::vector<RenderPassWrapper>			m_renderPasses;
	Move<VkCommandPool>						m_cmdPool;
	Move<VkCommandBuffer>					m_cmdBuffer;
	Move<VkImage>							m_colorImage;
	Move<VkImageView>						m_colorAttachmentView;
	de::MovePtr<Allocation>					m_colorImageAlloc;
	std::vector<VkImageMemoryBarrier>		m_imageLayoutBarriers;
	std::vector<GraphicsPipelineWrapper>	m_pipelines;

	ShaderWrapper							m_shaderModules[2];
	deUint32								m_shaderStageCount;
	VkPipelineShaderStageCreateInfo			m_shaderStageInfo[2];
};

BlendOperationAdvancedTestCoherentInstance::~BlendOperationAdvancedTestCoherentInstance (void)
{
}

void BlendOperationAdvancedTestCoherentInstance::buildPipeline ()
{
	const InstanceInterface&		vki				= m_context.getInstanceInterface();
	const DeviceInterface&			vk				= m_context.getDeviceInterface();
	const VkPhysicalDevice			physicalDevice	= m_context.getPhysicalDevice();
	const VkDevice					vkDevice		= m_context.getDevice();

	const std::vector<VkRect2D>		scissor			{ makeRect2D(m_renderSize) };
	const std::vector<VkViewport>	viewport		{ makeViewport(m_renderSize) };

	const VkPipelineColorBlendAdvancedStateCreateInfoEXT blendAdvancedStateParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_ADVANCED_STATE_CREATE_INFO_EXT,	// VkStructureType		sType;
		DE_NULL,																// const void*			pNext;
		VK_TRUE,																// VkBool32				srcPremultiplied;
		VK_TRUE,																// VkBool32				dstPremultiplied;
		m_param.overlap,														// VkBlendOverlapEXT	blendOverlap;
	};

	std::vector<VkPipelineColorBlendAttachmentState>	colorBlendAttachmentStates;

	// One VkPipelineColorBlendAttachmentState for each pipeline, we only have one color attachment.
	for (deUint32 i = 0; i < 2; i++)
	{
		const VkPipelineColorBlendAttachmentState colorBlendAttachmentState =
		{
			VK_TRUE,														// VkBool32									blendEnable;
			VK_BLEND_FACTOR_ONE,											// VkBlendFactor							srcColorBlendFactor;
			VK_BLEND_FACTOR_ONE,											// VkBlendFactor							dstColorBlendFactor;
			m_param.blendOps[i],											// VkBlendOp								colorBlendOp;
			VK_BLEND_FACTOR_ONE,											// VkBlendFactor							srcAlphaBlendFactor;
			VK_BLEND_FACTOR_ONE,											// VkBlendFactor							dstAlphaBlendFactor;
			m_param.blendOps[i],											// VkBlendOp								alphaBlendOp;
			VK_COLOR_COMPONENT_R_BIT |
			VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT |
			VK_COLOR_COMPONENT_A_BIT										// VkColorComponentFlags					colorWriteMask;
		};
		colorBlendAttachmentStates.emplace_back(colorBlendAttachmentState);
	}

	std::vector<VkPipelineColorBlendStateCreateInfo> colorBlendStateParams;
	VkPipelineColorBlendStateCreateInfo colorBlendStateParam =
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType								sType;
		&blendAdvancedStateParams,									// const void*									pNext;
		0u,															// VkPipelineColorBlendStateCreateFlags			flags;
		VK_FALSE,													// VkBool32										logicOpEnable;
		VK_LOGIC_OP_COPY,											// VkLogicOp									logicOp;
		1u,															// deUint32										attachmentCount;
		&colorBlendAttachmentStates[0],								// const VkPipelineColorBlendAttachmentState*	pAttachments;
		{ 0.0f, 0.0f, 0.0f, 0.0f },									// float										blendConst[4];
	};
	colorBlendStateParams.emplace_back(colorBlendStateParam);

	// For the second pipeline, the blendOp changed.
	colorBlendStateParam.pAttachments = &colorBlendAttachmentStates[1];
	colorBlendStateParams.emplace_back(colorBlendStateParam);

	const VkPipelineMultisampleStateCreateInfo  multisampleStateParams	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType								sType;
		DE_NULL,													// const void*									pNext;
		0u,															// VkPipelineMultisampleStateCreateFlags		flags;
		VK_SAMPLE_COUNT_1_BIT,										// VkSampleCountFlagBits						rasterizationSamples;
		VK_FALSE,													// VkBool32										sampleShadingEnable;
		0.0f,														// float										minSampleShading;
		DE_NULL,													// const VkSampleMask*							pSampleMask;
		VK_FALSE,													// VkBool32										alphaToCoverageEnable;
		VK_FALSE,													// VkBool32										alphaToOneEnable;
	};

	VkPipelineDepthStencilStateCreateInfo depthStencilStateParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, // VkStructureType								sType;
		DE_NULL,													// const void*									pNext;
		0u,															// VkPipelineDepthStencilStateCreateFlags		flags;
		VK_FALSE,													// VkBool32										depthTestEnable;
		VK_FALSE,													// VkBool32										depthWriteEnable;
		VK_COMPARE_OP_NEVER,										// VkCompareOp									depthCompareOp;
		VK_FALSE,													// VkBool32										depthBoundsTestEnable;
		VK_FALSE,													// VkBool32										stencilTestEnable;
		// VkStencilOpState front;
		{
			VK_STENCIL_OP_KEEP,		// VkStencilOp	failOp;
			VK_STENCIL_OP_KEEP,		// VkStencilOp	passOp;
			VK_STENCIL_OP_KEEP,		// VkStencilOp	depthFailOp;
			VK_COMPARE_OP_NEVER,	// VkCompareOp	compareOp;
			0u,						// deUint32		compareMask;
			0u,						// deUint32		writeMask;
			0u,						// deUint32		reference;
		},
		// VkStencilOpState back;
		{
			VK_STENCIL_OP_KEEP,		// VkStencilOp	failOp;
			VK_STENCIL_OP_KEEP,		// VkStencilOp	passOp;
			VK_STENCIL_OP_KEEP,		// VkStencilOp	depthFailOp;
			VK_COMPARE_OP_NEVER,	// VkCompareOp	compareOp;
			0u,						// deUint32		compareMask;
			0u,						// deUint32		writeMask;
			0u,						// deUint32		reference;
		},
		0.0f,														// float										minDepthBounds;
		1.0f,														// float										maxDepthBounds;
	};

	const VkDynamicState dynamicState = VK_DYNAMIC_STATE_SCISSOR;
	const VkPipelineDynamicStateCreateInfo dynamicStateParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,	// VkStructureType						sType;
		DE_NULL,												// const void*							pNext;
		0u,														// VkPipelineDynamicStateCreateFlags	flags;
		1u,														// uint32_t								dynamicStateCount;
		&dynamicState											// const VkDynamicState*				pDynamicStates;
	};

	m_shaderModules[0] = ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("vert"), 0);
	m_shaderModules[1] = ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("frag"), 0);

	m_pipelines.reserve(2);

	// Create first pipeline
	m_pipelines.emplace_back(vki, vk, physicalDevice, vkDevice, m_context.getDeviceExtensions(), m_param.pipelineConstructionType);
	m_pipelines.back()
		.setDynamicState(&dynamicStateParams)
		.setDefaultRasterizationState()
		.setupVertexInputState()
		.setupPreRasterizationShaderState(viewport, scissor, m_pipelineLayout, m_renderPasses[0].get(), 0u, m_shaderModules[0])
		.setupFragmentShaderState(m_pipelineLayout, m_renderPasses[0].get(), 0u, m_shaderModules[1], &depthStencilStateParams, &multisampleStateParams)
		.setupFragmentOutputState(m_renderPasses[0].get(), 0u, &colorBlendStateParams[0], &multisampleStateParams)
		.setMonolithicPipelineLayout(m_pipelineLayout)
		.buildPipeline();

	// Create second pipeline
	m_pipelines.emplace_back(vki, vk, physicalDevice, vkDevice, m_context.getDeviceExtensions(), m_param.pipelineConstructionType);
	m_pipelines.back()
		.setDynamicState(&dynamicStateParams)
		.setDefaultRasterizationState()
		.setupVertexInputState()
		.setupPreRasterizationShaderState(viewport, scissor, m_pipelineLayout, m_renderPasses[1].get(), 0u, m_shaderModules[0])
		.setupFragmentShaderState(m_pipelineLayout, m_renderPasses[1].get(), 0u, m_shaderModules[1], &depthStencilStateParams, &multisampleStateParams)
		.setupFragmentOutputState(m_renderPasses[1].get(), 0u, &colorBlendStateParams[1], &multisampleStateParams)
		.setMonolithicPipelineLayout(m_pipelineLayout)
		.buildPipeline();
}

void BlendOperationAdvancedTestCoherentInstance::prepareRenderPass (GraphicsPipelineWrapper& pipeline, RenderPassWrapper& renderpass, deBool secondDraw)
{
	const DeviceInterface&	vk				 = m_context.getDeviceInterface();

	VkClearValue	attachmentClearValue = makeClearValueColor(clearColorVec4);

	renderpass.begin(vk, *m_cmdBuffer, makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y()),
					(secondDraw ? 0u : 1u),
					(secondDraw ? DE_NULL : &attachmentClearValue));

	pipeline.bind(*m_cmdBuffer);
	VkDeviceSize offsets = 0u;
	vk.cmdBindVertexBuffers(*m_cmdBuffer, 0u, 1u, &m_vertexBuffer.get(), &offsets);

	// There are two different renderpasses, each of them draw
	// one half of the colors.
	deUint32 skippedColors = 0u;
	for (deUint32 color = 0; color < DE_LENGTH_OF_ARRAY(srcColors)/2; color++)
	{
		// Skip ill-formed colors when we have non-premultiplied destination colors.
		if (m_param.premultipliedDstColor == VK_FALSE)
		{
			deBool skipColor = false;
			for (deUint32 i = 0; i < m_param.colorAttachmentsCount; i++)
			{
				Vec4 calculatedColor = calculateFinalColor(m_param, m_param.blendOps[i], srcColors[color], dstColors[color]);
				if (calculatedColor.w() <= 0.0f && calculatedColor != Vec4(0.0f))
				{
					// Skip ill-formed colors, because the spec says the result is undefined.
					skippedColors++;
					skipColor = true;
					break;
				}
			}
			if (skipColor)
				continue;
		}
		deInt32 x = 0;
		deInt32 y = 0;
		getCoordinates(color, x, y);

		deUint32 index = secondDraw ? (color + DE_LENGTH_OF_ARRAY(srcColors) / 2) : color;

		// Set source color as push constant
		vk.cmdPushConstants(*m_cmdBuffer, *m_pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0u, sizeof(Vec4), &srcColors[index]);
		VkRect2D scissor = makeRect2D(x, y, 1u, 1u);
		if (vk::isConstructionTypeShaderObject(m_param.pipelineConstructionType))
		{
#ifndef CTS_USES_VULKANSC
			vk.cmdSetScissorWithCount(*m_cmdBuffer, 1u, &scissor);
#else
			vk.cmdSetScissorWithCountEXT(*m_cmdBuffer, 1u, &scissor);
#endif
		}
		else
		{
			vk.cmdSetScissor(*m_cmdBuffer, 0u, 1u, &scissor);
		}

		// To set destination color, we do clear attachment restricting the area to the respective pixel of each color attachment.
		// Only clear in the first draw, for the second draw the destination color is the result of the first draw's blend.
		if (secondDraw == DE_FALSE)
		{
			std::vector<VkClearAttachment> attachments;
			VkClearValue clearValue = vk::makeClearValueColorVec4(dstColors[index]);

			const VkClearAttachment	attachment	=
			{
				VK_IMAGE_ASPECT_COLOR_BIT,
				0u,
				clearValue
			};

			const VkClearRect rect =
			{
				scissor,
				0u,
				1u
			};
			vk.cmdClearAttachments(*m_cmdBuffer, 1u, &attachment, 1u, &rect);
		}

		// Draw
		vk.cmdDraw(*m_cmdBuffer, (deUint32)m_vertices.size(), 1u, 0u, 0u);
	}

	// If we break this assert, then we are not testing anything in this test.
	DE_ASSERT(skippedColors < (DE_LENGTH_OF_ARRAY(srcColors) / 2));

	// Log number of skipped colors
	if (skippedColors != 0u)
	{
		tcu::TestLog& log = m_context.getTestContext().getLog();
		log << tcu::TestLog::Message << "Skipped " << skippedColors << " out of " << (DE_LENGTH_OF_ARRAY(srcColors) / 2) << " color cases due to ill-formed colors" << tcu::TestLog::EndMessage;
	}
	renderpass.end(vk, *m_cmdBuffer);
}

void BlendOperationAdvancedTestCoherentInstance::prepareCommandBuffer ()
{
	const DeviceInterface&	vk				 = m_context.getDeviceInterface();

	beginCommandBuffer(vk, *m_cmdBuffer, 0u);

	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, (VkDependencyFlags)0,
						  0u, DE_NULL, 0u, DE_NULL, (deUint32)m_imageLayoutBarriers.size(), m_imageLayoutBarriers.data());

	prepareRenderPass(m_pipelines[0], m_renderPasses[0], false);

	if (m_param.coherentOperations == DE_FALSE)
	{
		const VkImageMemoryBarrier colorImageBarrier =
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,					// VkStructureType			sType;
			DE_NULL,												// const void*				pNext;
			(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
			 VK_ACCESS_COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT),	// VkAccessFlags			srcAccessMask;
			(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
			 VK_ACCESS_COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT),	// VkAccessFlags			dstAccessMask;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,				// VkImageLayout			oldLayout;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,				// VkImageLayout			newLayout;
			VK_QUEUE_FAMILY_IGNORED,								// deUint32					srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,								// deUint32					dstQueueFamilyIndex;
			*m_colorImage,											// VkImage					image;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u },			// VkImageSubresourceRange	subresourceRange;
		};
		vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
							  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, (VkDependencyFlags)0,
							  0u, DE_NULL, 0u, DE_NULL, 1u, &colorImageBarrier);
	}

	prepareRenderPass(m_pipelines[1], m_renderPasses[1], true);

	endCommandBuffer(vk, *m_cmdBuffer);
}

BlendOperationAdvancedTestCoherentInstance::BlendOperationAdvancedTestCoherentInstance	(Context&							context,
																						 const BlendOperationAdvancedParam	param)
	: TestInstance			(context)
	, m_param				(param)
	, m_renderSize			(tcu::UVec2(widthArea, heightArea))
	, m_colorFormat			(param.format)
	, m_shaderStageCount	(0)
{
	const DeviceInterface&		vk				 = m_context.getDeviceInterface();
	const VkDevice				vkDevice		 = m_context.getDevice();
	const deUint32				queueFamilyIndex = context.getUniversalQueueFamilyIndex();

	// Create vertex buffer
	{
		m_vertices		= createPoints();
		DE_ASSERT((deUint32)m_vertices.size() == 6);

		m_vertexBuffer	= createBufferAndBindMemory(m_context, m_vertices.size() * sizeof(Vec4), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &m_vertexBufferMemory);
		// Load vertices into vertex buffer
		deMemcpy(m_vertexBufferMemory->getHostPtr(), m_vertices.data(), m_vertices.size() * sizeof(Vec4));
		flushAlloc(vk, vkDevice, *m_vertexBufferMemory);
	}

	// Create render passes
	m_renderPasses.emplace_back(makeTestRenderPass(param, vk, vkDevice, m_colorFormat, VK_ATTACHMENT_LOAD_OP_CLEAR));
	m_renderPasses.emplace_back(makeTestRenderPass(param, vk, vkDevice, m_colorFormat, VK_ATTACHMENT_LOAD_OP_LOAD));

	const VkComponentMapping	componentMappingRGBA = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};

	// Create color image
	m_colorImage	= createImage2DAndBindMemory(m_context,
												 m_colorFormat,
												 m_renderSize.x(),
												 m_renderSize.y(),
												 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
												 VK_SAMPLE_COUNT_1_BIT,
												 &m_colorImageAlloc);
	// Set up image layout transition barriers
	{
		VkImageMemoryBarrier colorImageBarrier =
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,					// VkStructureType			sType;
			DE_NULL,												// const void*				pNext;
			0u,														// VkAccessFlags			srcAccessMask;
			(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
			 VK_ACCESS_COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT),	// VkAccessFlags			dstAccessMask;
			VK_IMAGE_LAYOUT_UNDEFINED,								// VkImageLayout			oldLayout;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,				// VkImageLayout			newLayout;
			VK_QUEUE_FAMILY_IGNORED,								// deUint32					srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,								// deUint32					dstQueueFamilyIndex;
			*m_colorImage,											// VkImage					image;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u },			// VkImageSubresourceRange	subresourceRange;
		};

		m_imageLayoutBarriers.emplace_back(colorImageBarrier);
	}

	// Create color attachment view
	{
		VkImageViewCreateInfo colorAttachmentViewParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			0u,												// VkImageViewCreateFlags	flags;
			*m_colorImage,									// VkImage					image;
			VK_IMAGE_VIEW_TYPE_2D,							// VkImageViewType			viewType;
			m_colorFormat,									// VkFormat					format;
			componentMappingRGBA,							// VkComponentMapping		components;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u },	// VkImageSubresourceRange	subresourceRange;
		};

		m_colorAttachmentView = createImageView(vk, vkDevice, &colorAttachmentViewParams);
	}

	// Create framebuffers
	{
		VkFramebufferCreateInfo framebufferParams =
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,			// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			0u,													// VkFramebufferCreateFlags		flags;
			m_renderPasses[0].get(),							// VkRenderPass					renderPass;
			1u,													// deUint32						attachmentCount;
			&m_colorAttachmentView.get(),						// const VkImageView*			pAttachments;
			(deUint32)m_renderSize.x(),							// deUint32						width;
			(deUint32)m_renderSize.y(),							// deUint32						height;
			1u,													// deUint32						layers;
		};

		m_renderPasses[0].createFramebuffer(vk, vkDevice, &framebufferParams, *m_colorImage);
		framebufferParams.renderPass = m_renderPasses[1].get();
		m_renderPasses[1].createFramebuffer(vk, vkDevice, &framebufferParams, *m_colorImage);
	}

	// Create pipeline layout
	{
		const VkPushConstantRange pushConstantRange =
		{
			VK_SHADER_STAGE_FRAGMENT_BIT,		// VkShaderStageFlags	stageFlags
			0,									// deUint32				offset
			sizeof(Vec4)						// deUint32				size
		};

		const VkPipelineLayoutCreateInfo pipelineLayoutParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType					sType;
			DE_NULL,											// const void*						pNext;
			0u,													// VkPipelineLayoutCreateFlags		flags;
			0u,													// deUint32							setLayoutCount;
			DE_NULL,											// const VkDescriptorSetLayout*		pSetLayouts;
			1u,													// deUint32							pushConstantRangeCount;
			&pushConstantRange									// const VkPushConstantRange*		pPushConstantRanges;
		};

		m_pipelineLayout = PipelineLayoutWrapper(m_param.pipelineConstructionType, vk, vkDevice, &pipelineLayoutParams);
	}

	// Create pipeline
	buildPipeline();

	// Create command pool
	m_cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);

	// Create command buffer
	m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
}

tcu::TestStatus BlendOperationAdvancedTestCoherentInstance::iterate (void)
{
	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	const VkDevice			vkDevice			= m_context.getDevice();
	const VkQueue			queue				= m_context.getUniversalQueue();
	tcu::TestLog&			log					= m_context.getTestContext().getLog();

	// Log the blend operations to test
	{
		DE_ASSERT(m_param.blendOps.size() == 2u);
		log << tcu::TestLog::Message << "First depth op: " << de::toLower(getBlendOpStr(m_param.blendOps[0]).toString().substr(3)) << tcu::TestLog::EndMessage;
		log << tcu::TestLog::Message << "Second depth op: " << de::toLower(getBlendOpStr(m_param.blendOps[1]).toString().substr(3)) << tcu::TestLog::EndMessage;

	}

	prepareCommandBuffer();

	submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());
	return verifyTestResult();
}

tcu::TestStatus BlendOperationAdvancedTestCoherentInstance::verifyTestResult (void)
{
	deBool					compareOk			= DE_TRUE;
	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	const VkDevice			vkDevice			= m_context.getDevice();
	const VkQueue			queue				= m_context.getUniversalQueue();
	const deUint32			queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	Allocator&				allocator			= m_context.getDefaultAllocator();
	tcu::TextureLevel		refImage			(vk::mapVkFormat(m_colorFormat), 32, 32);

	tcu::clear(refImage.getAccess(), clearColorVec4);

	// Generate reference image
	for (deUint32 color = 0; color < DE_LENGTH_OF_ARRAY(srcColors)/2; color++)
	{
		deUint32 secondDrawColorIndex = color + DE_LENGTH_OF_ARRAY(srcColors)/2;
		// Calculate first draw final color
		Vec4 rectColorTmp = calculateFinalColor(m_param, m_param.blendOps[0], srcColors[color], dstColors[color]);

		if (m_param.premultipliedDstColor == VK_FALSE)
		{
			if (rectColorTmp.w() > 0.0f)
			{
				rectColorTmp.x() = rectColorTmp.x() / rectColorTmp.w();
				rectColorTmp.y() = rectColorTmp.y() / rectColorTmp.w();
				rectColorTmp.z() = rectColorTmp.z() / rectColorTmp.w();
			}
			else
			{
				// Skip the color check if it is ill-formed.
				if (rectColorTmp != Vec4(0.0f))
					continue;
			}
		}
		// Calculate second draw final color
		Vec4 rectColor = calculateFinalColor(m_param, m_param.blendOps[1], srcColors[secondDrawColorIndex], rectColorTmp);
		if (m_param.premultipliedDstColor == VK_FALSE)
		{
			if (rectColor.w() > 0.0f)
			{
				rectColor.x() = rectColor.x() / rectColor.w();
				rectColor.y() = rectColor.y() / rectColor.w();
				rectColor.z() = rectColor.z() / rectColor.w();
			}
			else
			{
				// Skip the color check if it is ill-formed.
				if (rectColor != Vec4(0.0f))
					continue;
			}
		}

		deInt32 x = 0;
		deInt32 y = 0;
		getCoordinates(color, x, y);
		tcu::clear(tcu::getSubregion(refImage.getAccess(), x, y, 1u, 1u), rectColor);
	}

	de::MovePtr<tcu::TextureLevel> result = vkt::pipeline::readColorAttachment(vk, vkDevice, queue, queueFamilyIndex, allocator, *m_colorImage, m_colorFormat, m_renderSize);
	std::ostringstream name;
	name << "Image comparison. Depth ops: " << de::toLower(getBlendOpStr(m_param.blendOps[0]).toString().substr(3)) << " and " << de::toLower(getBlendOpStr(m_param.blendOps[1]).toString().substr(3));

	// R8G8B8A8 threshold was derived experimentally.
	compareOk = tcu::floatThresholdCompare(m_context.getTestContext().getLog(),
										   "FloatImageCompare",
										   name.str().c_str(),
										   refImage.getAccess(),
										   result->getAccess(),
										   clearColorVec4,
										   m_colorFormat == VK_FORMAT_R8G8B8A8_UNORM ? Vec4(0.13f, 0.13f, 0.13f, 0.13f) : Vec4(0.01f, 0.01f, 0.01f, 0.01f),
										   tcu::COMPARE_LOG_RESULT);
	if (!compareOk)
		return tcu::TestStatus::fail("Image mismatch");

	return tcu::TestStatus::pass("Result images matches references");
}

TestInstance* BlendOperationAdvancedTest::createInstance (Context& context) const
{
	if (m_param.testMode == TEST_MODE_GENERIC)
		return new BlendOperationAdvancedTestInstance(context, m_param);
	else
		return new BlendOperationAdvancedTestCoherentInstance(context, m_param);
}

} // anonymous

tcu::TestCaseGroup* createBlendOperationAdvancedTests (tcu::TestContext& testCtx, PipelineConstructionType pipelineConstructionType)
{
	enum nonpremultiplyEnum
	{
		PREMULTIPLY_SRC = 1u,
		PREMULTIPLY_DST = 2u
	};
	deUint32	premultiplyModes[] = { 0u, PREMULTIPLY_SRC, PREMULTIPLY_DST, PREMULTIPLY_SRC | PREMULTIPLY_DST };
	deUint32	colorAttachmentCounts[] = { 1u, 2u, 4u, 8u, 16u };
	deBool		coherentOps[] = { DE_FALSE, DE_TRUE };
	VkBlendOp	blendOps[] =
	{
		VK_BLEND_OP_ZERO_EXT, VK_BLEND_OP_SRC_EXT, VK_BLEND_OP_DST_EXT,	VK_BLEND_OP_SRC_OVER_EXT, VK_BLEND_OP_DST_OVER_EXT,
		VK_BLEND_OP_SRC_IN_EXT, VK_BLEND_OP_DST_IN_EXT, VK_BLEND_OP_SRC_OUT_EXT, VK_BLEND_OP_DST_OUT_EXT, VK_BLEND_OP_SRC_ATOP_EXT,
		VK_BLEND_OP_DST_ATOP_EXT, VK_BLEND_OP_XOR_EXT, VK_BLEND_OP_MULTIPLY_EXT, VK_BLEND_OP_SCREEN_EXT, VK_BLEND_OP_OVERLAY_EXT,
		VK_BLEND_OP_DARKEN_EXT, VK_BLEND_OP_LIGHTEN_EXT, VK_BLEND_OP_COLORDODGE_EXT, VK_BLEND_OP_COLORBURN_EXT, VK_BLEND_OP_HARDLIGHT_EXT,
		VK_BLEND_OP_SOFTLIGHT_EXT, VK_BLEND_OP_DIFFERENCE_EXT, VK_BLEND_OP_EXCLUSION_EXT, VK_BLEND_OP_INVERT_EXT, VK_BLEND_OP_INVERT_RGB_EXT,
		VK_BLEND_OP_LINEARDODGE_EXT, VK_BLEND_OP_LINEARBURN_EXT, VK_BLEND_OP_VIVIDLIGHT_EXT, VK_BLEND_OP_LINEARLIGHT_EXT, VK_BLEND_OP_PINLIGHT_EXT,
		VK_BLEND_OP_HARDMIX_EXT, VK_BLEND_OP_HSL_HUE_EXT, VK_BLEND_OP_HSL_SATURATION_EXT, VK_BLEND_OP_HSL_COLOR_EXT, VK_BLEND_OP_HSL_LUMINOSITY_EXT,
		VK_BLEND_OP_PLUS_EXT, VK_BLEND_OP_PLUS_CLAMPED_EXT, VK_BLEND_OP_PLUS_CLAMPED_ALPHA_EXT, VK_BLEND_OP_PLUS_DARKER_EXT, VK_BLEND_OP_MINUS_EXT,
		VK_BLEND_OP_MINUS_CLAMPED_EXT, VK_BLEND_OP_CONTRAST_EXT, VK_BLEND_OP_INVERT_OVG_EXT, VK_BLEND_OP_RED_EXT, VK_BLEND_OP_GREEN_EXT, VK_BLEND_OP_BLUE_EXT,
	};

	de::MovePtr<tcu::TestCaseGroup> tests (new tcu::TestCaseGroup(testCtx, "blend_operation_advanced", "VK_EXT_blend_operation_advanced tests"));
	de::Random						rnd				(deStringHash(tests->getName()));

	de::MovePtr<tcu::TestCaseGroup> opsTests (new tcu::TestCaseGroup(testCtx, "ops", "Test each blend operation advance op"));


	for (deUint32 colorAttachmentCount = 0u; colorAttachmentCount < DE_LENGTH_OF_ARRAY(colorAttachmentCounts); colorAttachmentCount++)
	{
		for (deUint32 overlap = 0; overlap <= VK_BLEND_OVERLAP_CONJOINT_EXT; overlap++)
		{
			for (deUint32 premultiply = 0u; premultiply < DE_LENGTH_OF_ARRAY(premultiplyModes); premultiply++)
			{
				deUint32 testNumber = 0u;
				for (deUint64 blendOp = 0u; blendOp < DE_LENGTH_OF_ARRAY(blendOps); blendOp++)
				{
					deBool isAdditionalRGBBlendOp = blendOps[blendOp] >= VK_BLEND_OP_PLUS_EXT && blendOps[blendOp] < VK_BLEND_OP_MAX_ENUM;

					// Additional RGB Blend operations are not affected by the blend overlap modes
					if (isAdditionalRGBBlendOp && overlap != VK_BLEND_OVERLAP_UNCORRELATED_EXT)
						continue;

					BlendOperationAdvancedParam testParams;
					testParams.pipelineConstructionType = pipelineConstructionType;
					testParams.testMode					= TEST_MODE_GENERIC;
					testParams.overlap					= (VkBlendOverlapEXT) overlap;
					testParams.coherentOperations		= DE_FALSE;
					testParams.colorAttachmentsCount	= colorAttachmentCounts[colorAttachmentCount];
					testParams.independentBlend			= DE_FALSE;
					testParams.premultipliedSrcColor	= (premultiplyModes[premultiply] & PREMULTIPLY_SRC) ? VK_TRUE : VK_FALSE;
					testParams.premultipliedDstColor	= (premultiplyModes[premultiply] & PREMULTIPLY_DST) ? VK_TRUE : VK_FALSE;
					testParams.testNumber				= testNumber++;
					testParams.format					= VK_FORMAT_R16G16B16A16_SFLOAT;

					for (deUint32 numColorAtt = 0; numColorAtt < colorAttachmentCounts[colorAttachmentCount]; numColorAtt++)
						testParams.blendOps.push_back(blendOps[blendOp]);
					opsTests->addChild(newTestCase<BlendOperationAdvancedTest>(testCtx, testParams));

					testParams.format = VK_FORMAT_R8G8B8A8_UNORM;
					opsTests->addChild(newTestCase<BlendOperationAdvancedTest>(testCtx, testParams));
				}
			}
		}
	}
	tests->addChild(opsTests.release());

	// Independent Blend Tests: test more than one color attachment.
	de::MovePtr<tcu::TestCaseGroup> independentTests (new tcu::TestCaseGroup(testCtx, "independent", "Test independent blend feature"));
	deUint32 testNumber = 0u;

	for (deUint32 colorAttachmentCount = 1u; colorAttachmentCount < DE_LENGTH_OF_ARRAY(colorAttachmentCounts); colorAttachmentCount++)
	{
		BlendOperationAdvancedParam testParams;
		testParams.pipelineConstructionType = pipelineConstructionType;
		testParams.testMode					= TEST_MODE_GENERIC;
		testParams.overlap					= VK_BLEND_OVERLAP_UNCORRELATED_EXT;
		testParams.coherentOperations		= DE_FALSE;
		testParams.colorAttachmentsCount	= colorAttachmentCounts[colorAttachmentCount];
		testParams.independentBlend			= DE_TRUE;
		testParams.premultipliedSrcColor	= VK_TRUE;
		testParams.premultipliedDstColor	= VK_TRUE;
		testParams.testNumber				= testNumber++;
		testParams.format					= VK_FORMAT_R16G16B16A16_SFLOAT;

		for (deUint32 numColorAtt = 0; numColorAtt < colorAttachmentCounts[colorAttachmentCount]; numColorAtt++)
		{
			deUint32 i = de::randomScalar<deUint32>(rnd, 0, DE_LENGTH_OF_ARRAY(blendOps) - 1);
			testParams.blendOps.push_back(blendOps[i]);
		}
		independentTests->addChild(newTestCase<BlendOperationAdvancedTest>(testCtx, testParams));

		testParams.format = VK_FORMAT_R8G8B8A8_UNORM;
		independentTests->addChild(newTestCase<BlendOperationAdvancedTest>(testCtx, testParams));
	}

	tests->addChild(independentTests.release());

	// Coherent tests, do two consecutive advanced blending operations on the same color attachment.
	de::MovePtr<tcu::TestCaseGroup> coherentTests (new tcu::TestCaseGroup(testCtx, "coherent", "Test coherent memory"));
	testNumber = 0u;

	for (deUint32 coherent = 0u; coherent < DE_LENGTH_OF_ARRAY(coherentOps); coherent++)
	{
		BlendOperationAdvancedParam testParams;
		testParams.pipelineConstructionType = pipelineConstructionType;
		testParams.testMode					= TEST_MODE_COHERENT;
		testParams.overlap					= VK_BLEND_OVERLAP_UNCORRELATED_EXT;
		testParams.coherentOperations		= coherentOps[coherent];
		testParams.colorAttachmentsCount	= 1u;
		testParams.independentBlend			= DE_FALSE;
		testParams.premultipliedSrcColor	= VK_TRUE;
		testParams.premultipliedDstColor	= VK_TRUE;
		testParams.testNumber				= testNumber++;
		testParams.format					= VK_FORMAT_R16G16B16A16_SFLOAT;

		// We do two consecutive advanced blending operations
		deUint32 i = de::randomScalar<deUint32>(rnd, 0, DE_LENGTH_OF_ARRAY(blendOps) - 1);
		testParams.blendOps.push_back(blendOps[i]);
		i = de::randomScalar<deUint32>(rnd, 0, DE_LENGTH_OF_ARRAY(blendOps) - 1);
		testParams.blendOps.push_back(blendOps[i]);

		coherentTests->addChild(newTestCase<BlendOperationAdvancedTest>(testCtx, testParams));

		testParams.format = VK_FORMAT_R8G8B8A8_UNORM;
		coherentTests->addChild(newTestCase<BlendOperationAdvancedTest>(testCtx, testParams));
	}
	tests->addChild(coherentTests.release());


	return tests.release();
}

} // pipeline

} // vkt
