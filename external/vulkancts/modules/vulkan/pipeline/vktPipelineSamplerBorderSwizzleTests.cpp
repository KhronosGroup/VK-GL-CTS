/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
 * Copyright (c) 2021 Valve Corporation.
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
 * \brief Border color swizzle tests
 *//*--------------------------------------------------------------------*/
#include "vktPipelineSamplerBorderSwizzleTests.hpp"
#include "vktPipelineImageUtil.hpp"

#include "vkImageUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "tcuMaybe.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuFloat.hpp"

#include "deRandom.hpp"

#include <string>
#include <sstream>
#include <array>
#include <cstring>
#include <algorithm>

namespace vkt
{
namespace pipeline
{

namespace
{

using namespace vk;

// Returns true if the mapping doesn't alter each component.
bool isIdentitySwizzle (const VkComponentMapping& mapping)
{
	return (
		(mapping.r == VK_COMPONENT_SWIZZLE_R || mapping.r == VK_COMPONENT_SWIZZLE_IDENTITY) &&
		(mapping.g == VK_COMPONENT_SWIZZLE_G || mapping.g == VK_COMPONENT_SWIZZLE_IDENTITY) &&
		(mapping.b == VK_COMPONENT_SWIZZLE_B || mapping.b == VK_COMPONENT_SWIZZLE_IDENTITY) &&
		(mapping.a == VK_COMPONENT_SWIZZLE_A || mapping.a == VK_COMPONENT_SWIZZLE_IDENTITY)
	);
}

struct TestParams
{
	VkFormat						textureFormat;
	VkClearColorValue				textureColor;
	VkComponentMapping				componentMapping;
	VkBorderColor					borderColor;
	tcu::Maybe<int>					componentGather;
	bool							useSamplerSwizzleHint;

	// Pseudorandom elements.
	tcu::Vec2						textureCoordinates;
	tcu::Maybe<VkClearColorValue>	customBorderColor;

	bool isCustom (void) const
	{
		return (borderColor == VK_BORDER_COLOR_INT_CUSTOM_EXT || borderColor == VK_BORDER_COLOR_FLOAT_CUSTOM_EXT);
	}

	bool isOpaqueBlack (void) const
	{
		return (borderColor == VK_BORDER_COLOR_INT_OPAQUE_BLACK || borderColor == VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK);
	}

	bool isIdentity (void) const
	{
		return isIdentitySwizzle(componentMapping);
	}
};

struct SpecConstants
{
	float	u;
	float	v;
	deInt32	gatherFlag;
	//deInt32	gatherComp;
};

class BorderSwizzleCase : public vkt::TestCase
{
public:
							BorderSwizzleCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params);
	virtual					~BorderSwizzleCase		(void) {}

	virtual void			initPrograms			(vk::SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance			(Context& context) const;
	virtual void			checkSupport			(Context& context) const;

protected:
	TestParams				m_params;
};

class BorderSwizzleInstance : public vkt::TestInstance
{
public:
								BorderSwizzleInstance	(Context& context, const TestParams &params);
	virtual						~BorderSwizzleInstance	(void) {}

	VkExtent3D					getImageExtent			(void) const;
	virtual tcu::TestStatus		iterate					(void);

protected:
	TestParams				m_params;
};

BorderSwizzleCase::BorderSwizzleCase(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params)
	: vkt::TestCase	(testCtx, name, description)
	, m_params		(params)
{
}

void BorderSwizzleCase::checkSupport (Context& context) const
{
	const auto&				vki					= context.getInstanceInterface();
	const auto				physicalDevice		= context.getPhysicalDevice();
	VkImageFormatProperties	formatProperties;

	const auto result = vki.getPhysicalDeviceImageFormatProperties(
		physicalDevice, m_params.textureFormat, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT), 0u, &formatProperties);

	if (result != VK_SUCCESS)
	{
		if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
			TCU_THROW(NotSupportedError, "Format not supported for sampling");
		TCU_FAIL("vkGetPhysicalDeviceImageFormatProperties returned " + de::toString(result));
	}

	const auto&	borderColorFeatures		= context.getCustomBorderColorFeaturesEXT();
	const auto&	borderSwizzleFeatures	= context.getBorderColorSwizzleFeaturesEXT();
	const bool	identity				= m_params.isIdentity();

	if (m_params.useSamplerSwizzleHint)
		context.requireDeviceFunctionality("VK_EXT_border_color_swizzle");

	if (m_params.isCustom())
	{
		if (!borderColorFeatures.customBorderColors)
			TCU_THROW(NotSupportedError, "Custom border colors not supported");

		if (!identity)
		{
			if (!borderSwizzleFeatures.borderColorSwizzle)
				TCU_THROW(NotSupportedError, "Custom border color with non-identity swizzle not supported");

			if (!m_params.useSamplerSwizzleHint && !borderSwizzleFeatures.borderColorSwizzleFromImage)
				TCU_THROW(NotSupportedError, "Custom border color with non-identity swizzle not supported without specifying sampler border mapping");
		}
	}
	else if (m_params.isOpaqueBlack())
	{
		if (!identity)
		{
			if (!borderSwizzleFeatures.borderColorSwizzle)
				TCU_THROW(NotSupportedError, "Opaque black with non-identity swizzle not supported");

			if (!m_params.useSamplerSwizzleHint && !borderSwizzleFeatures.borderColorSwizzleFromImage)
				TCU_THROW(NotSupportedError, "Opaque black with non-identity swizzle not supported without specifying sampler border mapping");
		}
	}
}

enum class FormatType
{
	SIGNED_INT = 0,
	UNSIGNED_INT,
	FLOAT,
};

FormatType getFormatType (VkFormat format)
{
	if (isIntFormat(format))
		return FormatType::SIGNED_INT;

	if (isUintFormat(format))
		return FormatType::UNSIGNED_INT;

	return FormatType::FLOAT;
}

// Output color attachment format will vary slightly with the chosen texture format to accomodate different clear colors.
VkFormat getColorAttachmentFormat (VkFormat textureFormat)
{
	const auto formatType = getFormatType(textureFormat);

	if (formatType == FormatType::SIGNED_INT)
		return VK_FORMAT_R32G32B32A32_SINT;

	if (formatType == FormatType::UNSIGNED_INT)
		return VK_FORMAT_R32G32B32A32_UINT;

	return VK_FORMAT_R32G32B32A32_SFLOAT;
}

void BorderSwizzleCase::initPrograms (vk::SourceCollections& programCollection) const
{
	std::ostringstream vert;
	vert
		<< "#version 450\n"
		<< "\n"
		<< "void main()\n"
		<< "{\n"
		// Full-screen clockwise triangle strip with 4 vertices.
		<< "	const float x = (-1.0+2.0*((gl_VertexIndex & 2)>>1));\n"
		<< "	const float y = ( 1.0-2.0* (gl_VertexIndex % 2));\n"
		<< "	gl_Position = vec4(x, y, 0.0, 1.0);\n"
		<< "}\n"
		;

	const auto formatType = getFormatType(m_params.textureFormat);

	std::string	prefix;
	if (formatType == FormatType::SIGNED_INT)
		prefix = "i";
	else if (formatType == FormatType::UNSIGNED_INT)
		prefix = "u";

	const std::string	samplerType		= prefix + "sampler2D";
	const std::string	outColorType	= prefix + "vec4";
	// Note: glslang will complain if the gather component is not a compile-time constant.
	const int			gatherComp		= (m_params.componentGather ? m_params.componentGather.get() : 0);

	// Note the spec constants here should match the SpecConstants structure.
	std::ostringstream frag;
	frag
		<< "#version 450\n"
		<< "\n"
		<< "layout (constant_id=0) const float u = 0.0f;\n"
		<< "layout (constant_id=1) const float v = 0.0f;\n"
		<< "layout (constant_id=2) const int gatherFlag = 0;\n"
		//<< "layout (constant_id=3) const int gatherComp = 0;\n"
		<< "\n"
		<< "layout (set=0, binding=0) uniform " << samplerType << " texSampler;\n"
		<< "\n"
		<< "layout (location=0) out " << outColorType << " colorOut;\n"
		<< "\n"
		<< "void main()\n"
		<< "{\n"
		<< "	const vec2 coords = vec2(u, v);\n"
		<< "\n"
		<< "	if (gatherFlag != 0)\n"
		<< "	{\n"
		<< "		colorOut = textureGather(texSampler, coords, " << gatherComp << ");\n"
		<< "	}\n"
		<< "	else\n"
		<< "	{\n"
		<< "		colorOut = texture(texSampler, coords);\n"
		<< "	}\n"
		<< "}\n"
		;

	programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
	programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

TestInstance* BorderSwizzleCase::createInstance (Context& context) const
{
	return new BorderSwizzleInstance(context, m_params);
}

BorderSwizzleInstance::BorderSwizzleInstance (Context& context, const TestParams &params)
	: vkt::TestInstance	(context)
	, m_params			(params)
{}

VkExtent3D BorderSwizzleInstance::getImageExtent (void) const
{
	return makeExtent3D(16u, 16u, 1u);
}

// Reinterprets the exponent and mantissa in the floating point number as an integer.
// Function copied from vktApiImageClearingTests.cpp but changed return type to deUint64.
deUint64 calcFloatDiff (float a, float b)
{
	const int		asign	= tcu::Float32(a).sign();
	const int		bsign	= tcu::Float32(a).sign();

	const deUint32	avalue	= (tcu::Float32(a).bits() & ((0x1u << 31u) - 1u));
	const deUint32	bvalue	= (tcu::Float32(b).bits() & ((0x1u << 31u) - 1u));

	if (asign != bsign)
		return avalue + bvalue + 1u;
	else if (avalue < bvalue)
		return bvalue - avalue;
	else
		return avalue - bvalue;
}

// Convert VkComponentMapping to an array of 4 VkComponentSwizzle elements.
tcu::Vector<VkComponentSwizzle, 4> makeComponentSwizzleVec(const VkComponentMapping& mapping)
{
	const tcu::Vector<VkComponentSwizzle, 4> result = {{ mapping.r, mapping.g, mapping.b, mapping.a }};
	return result;
}

// Apply swizzling to an array of 4 elements.
template <typename T>
tcu::Vector<T, 4> applySwizzle (const tcu::Vector<T, 4>& orig, const VkComponentMapping& mapping)
{
	const auto			swizzles	= makeComponentSwizzleVec(mapping);
	tcu::Vector<T, 4>	result;

	for (int i = 0; i < decltype(swizzles)::SIZE; ++i)
	{
		const auto cs = swizzles[i];
		DE_ASSERT(cs >= VK_COMPONENT_SWIZZLE_IDENTITY && cs <= VK_COMPONENT_SWIZZLE_A);

		if (cs == VK_COMPONENT_SWIZZLE_IDENTITY)
			result[i] = orig[i];
		else if (cs == VK_COMPONENT_SWIZZLE_ZERO)
			result[i] = static_cast<T>(0);
		else if (cs == VK_COMPONENT_SWIZZLE_ONE)
			result[i] = static_cast<T>(1);
		else
			result[i] = orig[cs - VK_COMPONENT_SWIZZLE_R];
	}

	return result;
}

// Apply gathering to an array of 4 elements.
template <typename T>
tcu::Vector<T, 4> applyGather (const tcu::Vector<T, 4>& orig, int compNum)
{
	tcu::Vector<T, 4> result;

	for (int i = 0; i < decltype(result)::SIZE; ++i)
		result[i] = orig[compNum];

	return result;
}

// Transforms an input border color, once expanded, to the expected output color.
template <typename T>
tcu::Vector<T, 4> getExpectedColor (const tcu::Vector<T, 4>& color, const TestParams& params)
{
	tcu::Vector<T, 4> result = color;

	result = applySwizzle(result, params.componentMapping);

	if (params.componentGather)
		result = applyGather(result, *params.componentGather);

	return result;
}

// Transforms an input border color to the expected output color.
// Uses the proper union member depending on the test parameters and takes into account "Conversion to RGBA" from the spec.
VkClearColorValue getExpectedColor (const VkClearColorValue& color, const TestParams& params)
{
	const auto			numComp		= tcu::getNumUsedChannels(mapVkFormat(params.textureFormat).order);
	const auto			formatType	= getFormatType(params.textureFormat);
	VkClearColorValue	result;

	DE_ASSERT(numComp >= 0 && numComp <= 4);

	if (formatType == FormatType::UNSIGNED_INT)
	{
		tcu::UVec4 borderColor (0u, 0u, 0u, 0u);

		for (int i = 0; i < numComp; ++i)
			borderColor[i] = color.uint32[i];

		if (numComp < 4)
			borderColor[3] = 1u;

		const auto expected = getExpectedColor(borderColor, params);

		for (int i = 0; i < decltype(expected)::SIZE; ++i)
			result.uint32[i] = expected[i];
	}
	else if (formatType == FormatType::SIGNED_INT)
	{
		tcu::IVec4 borderColor (0, 0, 0, 0);

		for (int i = 0; i < numComp; ++i)
			borderColor[i] = color.int32[i];

		if (numComp < 4)
			borderColor[3] = 1;

		const auto expected = getExpectedColor(borderColor, params);

		for (int i = 0; i < decltype(expected)::SIZE; ++i)
			result.int32[i] = expected[i];
	}
	else
	{
		DE_ASSERT(formatType == FormatType::FLOAT);

		tcu::Vec4 borderColor (.0f, .0f, .0f, .0f);

		for (int i = 0; i < numComp; ++i)
			borderColor[i] = color.float32[i];

		if (numComp < 4)
			borderColor[3] = 1.0f;

		const auto expected = getExpectedColor(borderColor, params);

		for (int i = 0; i < decltype(expected)::SIZE; ++i)
			result.float32[i] = expected[i];
	}

	return result;
}

// Compare color buffer to the expected border color.
//
// This method was copied from vktApiImageClearingTests.cpp and adapted to this use case:
//
// * Taking into account the texture format instead of the color buffer format when calculating acceptable thresholds.
// * Applying swizzles and gathering to said thresholds.
// * Making thresholds more strict for components that do not come from custom borders.
// * Checking the full image in a single pass.
//
// The color buffer format is supposed to be at least as precise as the texture format.
bool comparePixelToColorClearValue (const TestParams&					params,
									const tcu::ConstPixelBufferAccess&	access,
									const tcu::TextureFormat&			textureFormat,
									const VkClearColorValue&			ref,
									std::string&						stringResult)
{
	const auto	bufferFormat	= access.getFormat();
	const auto	channelClass	= getTextureChannelClass(textureFormat.type);
	// We must compare all available channels in the color buffer to check RGBA conversion.
	const auto	channelMask		= getTextureFormatChannelMask(bufferFormat);

	switch (channelClass)
	{
		case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
		case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
		{
			tcu::Vec4			refColor	(ref.float32[0],
											 ref.float32[1],
											 ref.float32[2],
											 ref.float32[3]);
			tcu::Vec4			threshold	(0.0f);

			if (params.isCustom())
			{
				// Relax thresholds for custom color components.
				const tcu::IVec4	bitDepth	(getTextureFormatBitDepth(textureFormat));
				const int			modifier	= (channelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT) ? 0 : 1;

				threshold = tcu::Vec4 (bitDepth[0] > 0 ? 1.0f / ((float)(1 << (bitDepth[0] - modifier)) - 1.0f) : 0.0f,
									   bitDepth[1] > 0 ? 1.0f / ((float)(1 << (bitDepth[1] - modifier)) - 1.0f) : 0.0f,
									   bitDepth[2] > 0 ? 1.0f / ((float)(1 << (bitDepth[2] - modifier)) - 1.0f) : 0.0f,
									   bitDepth[3] > 0 ? 1.0f / ((float)(1 << (bitDepth[3] - modifier)) - 1.0f) : 0.0f);

				if (isSRGB(textureFormat))
				{
					// Widen thresholds a bit due to possible low-precision sRGB conversions.
					for (int i = 0; i < decltype(threshold)::SIZE; ++i)
						threshold[i] *= 2.0f;
				}
			}

			// Apply swizzle and gather to thresholds.
			threshold = applySwizzle(threshold, params.componentMapping);

			if (params.componentGather)
				threshold = applyGather(threshold, *params.componentGather);

			for (int z = 0; z < access.getDepth(); ++z)
			for (int y = 0; y < access.getHeight(); ++y)
			for (int x = 0; x < access.getWidth(); ++x)
			{
				const tcu::Vec4	resColor	(access.getPixel(x, y, z));
				const bool		result		= !(anyNotEqual(logicalAnd(lessThanEqual(absDiff(resColor, refColor), threshold), channelMask), channelMask));

				if (!result || (x == 0 && y == 0 && z == 0))
				{
					std::stringstream s;
					s << "Ref:" << refColor << " Threshold:" << threshold << " Color:" << resColor;
					stringResult = s.str();
				}

				if (!result)
					return false;
			}

			return true;
		}

		case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
		{
			const tcu::UVec4	refColor	(ref.uint32[0],
											 ref.uint32[1],
											 ref.uint32[2],
											 ref.uint32[3]);
			tcu::UVec4			threshold	(0u);

			if (params.isCustom())
			{
				// Relax thresholds for custom color components.
				const tcu::IVec4 bitDepth (getTextureFormatBitDepth(textureFormat));

				threshold = tcu::UVec4 ((bitDepth[0] > 0) ? 1 : 0,
										(bitDepth[1] > 0) ? 1 : 0,
										(bitDepth[2] > 0) ? 1 : 0,
										(bitDepth[3] > 0) ? 1 : 0);
			}

			// Apply swizzle and gather to thresholds.
			threshold = applySwizzle(threshold, params.componentMapping);

			if (params.componentGather)
				threshold = applyGather(threshold, *params.componentGather);

			for (int z = 0; z < access.getDepth(); ++z)
			for (int y = 0; y < access.getHeight(); ++y)
			for (int x = 0; x < access.getWidth(); ++x)
			{
				const tcu::UVec4	resColor	(access.getPixelUint(x, y, z));
				const bool			result		= !(anyNotEqual(logicalAnd(lessThanEqual(absDiff(resColor, refColor), threshold), channelMask), channelMask));

				if (!result || (x == 0 && y == 0 && z == 0))
				{
					std::stringstream s;
					s << "Ref:" << refColor << " Threshold:" << threshold << " Color:" << resColor;
					stringResult = s.str();
				}

				if (!result)
					return false;
			}

			return true;
		}

		case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
		{
			const tcu::IVec4	refColor	(ref.int32[0],
											 ref.int32[1],
											 ref.int32[2],
											 ref.int32[3]);
			tcu::IVec4			threshold	(0);

			if (params.isCustom())
			{
				// Relax thresholds for custom color components.
				const tcu::IVec4 bitDepth (getTextureFormatBitDepth(textureFormat));

				threshold = tcu::IVec4 ((bitDepth[0] > 0) ? 1 : 0,
										(bitDepth[1] > 0) ? 1 : 0,
										(bitDepth[2] > 0) ? 1 : 0,
										(bitDepth[3] > 0) ? 1 : 0);
			}

			// Apply swizzle and gather to thresholds.
			threshold = applySwizzle(threshold, params.componentMapping);

			if (params.componentGather)
				threshold = applyGather(threshold, *params.componentGather);

			for (int z = 0; z < access.getDepth(); ++z)
			for (int y = 0; y < access.getHeight(); ++y)
			for (int x = 0; x < access.getWidth(); ++x)
			{
				const tcu::IVec4	resColor	(access.getPixelInt(x, y, z));
				const bool			result		= !(anyNotEqual(logicalAnd(lessThanEqual(absDiff(resColor, refColor), threshold), channelMask), channelMask));

				if (!result || (x == 0 && y == 0 && z == 0))
				{
					std::stringstream s;
					s << "Ref:" << refColor << " Threshold:" << threshold << " Color:" << resColor;
					stringResult = s.str();
				}

				if (!result)
					return false;
			}

			return true;
		}

		case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
		{
			using u64v4 = tcu::Vector<deUint64, 4>;

			const tcu::Vec4		refColor		(ref.float32[0],
												 ref.float32[1],
												 ref.float32[2],
												 ref.float32[3]);
			u64v4				threshold		(0ull);

			if (params.isCustom())
			{
				// Relax thresholds for custom color components.
				const tcu::IVec4	mantissaBitsI	(getTextureFormatMantissaBitDepth(textureFormat));
				const u64v4			mantissaBits	(mantissaBitsI.x(), mantissaBitsI.y(), mantissaBitsI.z(), mantissaBitsI.w());

				threshold = u64v4 ((mantissaBits[0] > 0ull) ? 10ull * (1ull << (23ull - mantissaBits[0])) : 0ull,
								   (mantissaBits[1] > 0ull) ? 10ull * (1ull << (23ull - mantissaBits[1])) : 0ull,
								   (mantissaBits[2] > 0ull) ? 10ull * (1ull << (23ull - mantissaBits[2])) : 0ull,
								   (mantissaBits[3] > 0ull) ? 10ull * (1ull << (23ull - mantissaBits[3])) : 0ull);
			}

			// Apply swizzle and gather to thresholds.
			threshold = applySwizzle(threshold, params.componentMapping);

			if (params.componentGather)
				threshold = applyGather(threshold, *params.componentGather);

			DE_ASSERT(allEqual(greaterThanEqual(threshold, u64v4(0u)), tcu::BVec4(true)));

			for (int z = 0; z < access.getDepth(); ++z)
			for (int y = 0; y < access.getHeight(); ++y)
			for (int x = 0; x < access.getWidth(); ++x)
			{
				const tcu::Vec4	resColor (access.getPixel(x, y, z));

				for (int ndx = 0; ndx < decltype(resColor)::SIZE; ndx++)
				{
					const bool result = !(calcFloatDiff(resColor[ndx], refColor[ndx]) > threshold[ndx] && channelMask[ndx]);

					if (!result || (x == 0 && y == 0 && z == 0))
					{
						float				floatThreshold	= tcu::Float32((deUint32)(threshold)[0]).asFloat();
						tcu::Vec4			thresholdVec4	(floatThreshold,
															 floatThreshold,
															 floatThreshold,
															 floatThreshold);
						std::stringstream	s;

						s << "Ref:" << refColor << " Threshold:" << thresholdVec4 << " Color:" << resColor;
						stringResult = s.str();
					}

					if (!result)
						return false;
				}
			}

			return true;
		}

		default:
			DE_FATAL("Invalid channel class");
			return false;
	}
}

// Gets the clear color value from the border color. See "Texel Replacement" in the spec.
VkClearColorValue getBorderClearColorValue (const TestParams& params)
{
	VkClearColorValue result;
	deMemset(&result, 0, sizeof(result));

	switch (params.borderColor)
	{
	case VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK:	/* memset works. */															break;
	case VK_BORDER_COLOR_INT_TRANSPARENT_BLACK:		/* memset works. */															break;
	case VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK:		result.float32[3] = 1.0f;													break;
	case VK_BORDER_COLOR_INT_OPAQUE_BLACK:			result.int32[3] = 1;														break;
	case VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE:		for (size_t i = 0; i < 4; ++i) result.float32[i] = 1.0f;					break;
	case VK_BORDER_COLOR_INT_OPAQUE_WHITE:			for (size_t i = 0; i < 4; ++i) result.int32[i] = 1;							break;
	case VK_BORDER_COLOR_FLOAT_CUSTOM_EXT:			// fallthrough.
	case VK_BORDER_COLOR_INT_CUSTOM_EXT:			DE_ASSERT(params.customBorderColor); result = *params.customBorderColor;	break;
	default:										DE_ASSERT(false);															break;
	}

	return result;
}

tcu::TestStatus BorderSwizzleInstance::iterate (void)
{
	const auto&	vkd						= m_context.getDeviceInterface();
	const auto	device					= m_context.getDevice();
	auto&		alloc					= m_context.getDefaultAllocator();
	const auto	queue					= m_context.getUniversalQueue();
	const auto	qIndex					= m_context.getUniversalQueueFamilyIndex();
	const auto	extent					= getImageExtent();
	const auto	custom					= m_params.isCustom();
	const auto	colorAttachmentFormat	= getColorAttachmentFormat(m_params.textureFormat);
	const auto	colorSubresourceRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

	// Texture.
	const VkImageCreateInfo textureCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
		nullptr,								//	const void*				pNext;
		0u,										//	VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
		m_params.textureFormat,					//	VkFormat				format;
		extent,									//	VkExtent3D				extent;
		1u,										//	deUint32				mipLevels;
		1u,										//	deUint32				arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
		(VK_IMAGE_USAGE_SAMPLED_BIT				//	VkImageUsageFlags		usage;
		|VK_IMAGE_USAGE_TRANSFER_DST_BIT),
		VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
		0u,										//	deUint32				queueFamilyIndexCount;
		nullptr,								//	const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
	};

	ImageWithMemory texture (vkd, device, alloc, textureCreateInfo, MemoryRequirement::Any);

	const VkImageViewCreateInfo textureViewCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	//	VkStructureType			sType;
		nullptr,									//	const void*				pNext;
		0u,											//	VkImageViewCreateFlags	flags;
		texture.get(),								//	VkImage					image;
		VK_IMAGE_VIEW_TYPE_2D,						//	VkImageViewType			viewType;
		m_params.textureFormat,						//	VkFormat				format;
		m_params.componentMapping,					//	VkComponentMapping		components;
		colorSubresourceRange,						//	VkImageSubresourceRange	subresourceRange;
	};

	const auto textureView = createImageView(vkd, device, &textureViewCreateInfo);

	// Color attachment.
	const VkImageCreateInfo colorAttachmentInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
		nullptr,								//	const void*				pNext;
		0u,										//	VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
		colorAttachmentFormat,					//	VkFormat				format;
		extent,									//	VkExtent3D				extent;
		1u,										//	deUint32				mipLevels;
		1u,										//	deUint32				arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
		(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
		| VK_IMAGE_USAGE_TRANSFER_SRC_BIT),		//	VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
		0u,										//	deUint32				queueFamilyIndexCount;
		nullptr,								//	const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
	};

	ImageWithMemory colorAttachment (vkd, device, alloc, colorAttachmentInfo, MemoryRequirement::Any);

	const auto colorAttachmentView = makeImageView(vkd, device, colorAttachment.get(), VK_IMAGE_VIEW_TYPE_2D, colorAttachmentInfo.format, colorSubresourceRange);

	// Texure sampler.
	de::MovePtr<VkSamplerCustomBorderColorCreateInfoEXT> customBorderColorInfo;

	const VkSamplerBorderColorComponentMappingCreateInfoEXT borderColorMappingInfo =
	{
		VK_STRUCTURE_TYPE_SAMPLER_BORDER_COLOR_COMPONENT_MAPPING_CREATE_INFO_EXT,
		nullptr,
		m_params.componentMapping,
		isSrgbFormat(m_params.textureFormat),
	};

	const void* pNext = nullptr;

	if (custom)
	{
		customBorderColorInfo	= de::MovePtr<VkSamplerCustomBorderColorCreateInfoEXT>(new VkSamplerCustomBorderColorCreateInfoEXT);
		*customBorderColorInfo	= initVulkanStructure();

		DE_ASSERT(m_params.customBorderColor);
		VkClearColorValue colorValue = m_params.customBorderColor.get();

		if (m_params.useSamplerSwizzleHint)
			customBorderColorInfo->pNext = &borderColorMappingInfo;

		// TODO: try combinations with customBorderColorWithoutFormat if supported?
		customBorderColorInfo->format				= m_params.textureFormat;
		customBorderColorInfo->customBorderColor	= colorValue;

		pNext = customBorderColorInfo.get();
	}
	else
	{
		if (m_params.useSamplerSwizzleHint)
			pNext = &borderColorMappingInfo;
	}

	const VkSamplerCreateInfo samplerCreateInfo =
	{
		VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,				//	VkStructureType			sType;
		pNext,												//	const void*				pNext;
		0u,													//	VkSamplerCreateFlags	flags;
		VK_FILTER_NEAREST,									//	VkFilter				magFilter;
		VK_FILTER_NEAREST,									//	VkFilter				minFilter;
		VK_SAMPLER_MIPMAP_MODE_NEAREST,						//	VkSamplerMipmapMode		mipmapMode;
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,			//	VkSamplerAddressMode	addressModeU;
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,			//	VkSamplerAddressMode	addressModeV;
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,			//	VkSamplerAddressMode	addressModeW;
		0u,													//	float					mipLodBias;
		VK_FALSE,											//	VkBool32				anisotropyEnable;
		0.0f,												//	float					maxAnisotropy;
		VK_FALSE,											//	VkBool32				compareEnable;
		VK_COMPARE_OP_NEVER,								//	VkCompareOp				compareOp;
		0.0f,												//	float					minLod;
		1.0f,												//	float					maxLod;
		m_params.borderColor,								//	VkBorderColor			borderColor;
		VK_FALSE,											//	VkBool32				unnormalizedCoordinates;
	};

	const auto sampler = createSampler(vkd, device, &samplerCreateInfo);

	// Descriptor set layout.
	DescriptorSetLayoutBuilder dsLayoutBuilder;
	dsLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	const auto dsLayout = dsLayoutBuilder.build(vkd, device);

	// Pipeline layout.
	const auto pipelineLayout = makePipelineLayout(vkd, device, dsLayout.get());

	// Descriptor pool.
	DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	const auto descriptorPool = poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

	// Descriptor set.
	const auto descriptorSet = makeDescriptorSet(vkd, device, descriptorPool.get(), dsLayout.get());

	// Update descriptor set.
	{
		DescriptorSetUpdateBuilder updateBuilder;
		VkDescriptorImageInfo descriptorImageInfo = makeDescriptorImageInfo(sampler.get(), textureView.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &descriptorImageInfo);
		updateBuilder.update(vkd, device);
	}

	// Render pass.
	const auto renderPass = makeRenderPass(vkd, device, colorAttachmentFormat);

	// Shader modules.
	const auto vertShader = createShaderModule(vkd, device, m_context.getBinaryCollection().get("vert"), 0u);
	const auto fragShader = createShaderModule(vkd, device, m_context.getBinaryCollection().get("frag"), 0u);

	// Pipeline.
	const VkPipelineShaderStageCreateInfo vertStageInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	//	VkStructureType						sType;
		nullptr,												//	const void*							pNext;
		0u,														//	VkPipelineShaderStageCreateFlags	flags;
		VK_SHADER_STAGE_VERTEX_BIT,								//	VkShaderStageFlagBits				stage;
		vertShader.get(),										//	VkShaderModule						module;
		"main",													//	const char*							pName;
		nullptr,												//	const VkSpecializationInfo*			pSpecializationInfo;
	};

	const SpecConstants specConstantData =
	{
		m_params.textureCoordinates.x(),
		m_params.textureCoordinates.y(),
		(m_params.componentGather ? 1 : 0),
		//(m_params.componentGather ? *m_params.componentGather : -1),
	};

	const VkSpecializationMapEntry specializationMap[] =
	{
		{	0u, offsetof(SpecConstants, u),				sizeof(specConstantData.u)			},
		{	1u, offsetof(SpecConstants, v),				sizeof(specConstantData.v)			},
		{	2u, offsetof(SpecConstants, gatherFlag),	sizeof(specConstantData.gatherFlag)	},
		//{	3u, offsetof(SpecConstants, gatherComp),	sizeof(specConstantData.gatherComp)	},
	};

	const VkSpecializationInfo specializationInfo =
	{
		static_cast<deUint32>(DE_LENGTH_OF_ARRAY(specializationMap)),	//	deUint32						mapEntryCount;
		specializationMap,												//	const VkSpecializationMapEntry*	pMapEntries;
		static_cast<deUintptr>(sizeof(specConstantData)),				//	deUintptr						dataSize;
		&specConstantData,												//	const void*						pData;
	};

	const VkPipelineShaderStageCreateInfo fragStageInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	//	VkStructureType						sType;
		nullptr,												//	const void*							pNext;
		0u,														//	VkPipelineShaderStageCreateFlags	flags;
		VK_SHADER_STAGE_FRAGMENT_BIT,							//	VkShaderStageFlagBits				stage;
		fragShader.get(),										//	VkShaderModule						module;
		"main",													//	const char*							pName;
		&specializationInfo,									//	const VkSpecializationInfo*			pSpecializationInfo;
	};

	const VkPipelineShaderStageCreateInfo shaderStagesInfo[] = { vertStageInfo, fragStageInfo };

	const VkPipelineVertexInputStateCreateInfo vertexInputInfo = initVulkanStructure();

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo = initVulkanStructure();
	inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

	const auto viewport	= makeViewport(extent);
	const auto scissor	= makeRect2D(extent);

	const VkPipelineViewportStateCreateInfo viewportInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,	//	VkStructureType						sType;
		nullptr,												//	const void*							pNext;
		0u,														//	VkPipelineViewportStateCreateFlags	flags;
		1u,														//	deUint32							viewportCount;
		&viewport,												//	const VkViewport*					pViewports;
		1u,														//	deUint32							scissorCount;
		&scissor,												//	const VkRect2D*						pScissors;
	};

	const VkPipelineTessellationStateCreateInfo tessInfo = initVulkanStructure();

	const VkPipelineRasterizationStateCreateInfo rasterizationInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,	//	VkStructureType							sType;
		nullptr,													//	const void*								pNext;
		0u,															//	VkPipelineRasterizationStateCreateFlags	flags;
		VK_FALSE,													//	VkBool32								depthClampEnable;
		VK_FALSE,													//	VkBool32								rasterizerDiscardEnable;
		VK_POLYGON_MODE_FILL,										//	VkPolygonMode							polygonMode;
		VK_CULL_MODE_NONE,											//	VkCullModeFlags							cullMode;
		VK_FRONT_FACE_CLOCKWISE,									//	VkFrontFace								frontFace;
		VK_FALSE,													//	VkBool32								depthBiasEnable;
		0.0f,														//	float									depthBiasConstantFactor;
		0.0f,														//	float									depthBiasClamp;
		0.0f,														//	float									depthBiasSlopeFactor;
		1.0f,														//	float									lineWidth;
	};

	const VkPipelineMultisampleStateCreateInfo multisampleInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	//	VkStructureType							sType;
		nullptr,													//	const void*								pNext;
		0u,															//	VkPipelineMultisampleStateCreateFlags	flags;
		VK_SAMPLE_COUNT_1_BIT,										//	VkSampleCountFlagBits					rasterizationSamples;
		VK_FALSE,													//	VkBool32								sampleShadingEnable;
		1.0f,														//	float									minSampleShading;
		nullptr,													//	const VkSampleMask*						pSampleMask;
		VK_FALSE,													//	VkBool32								alphaToCoverageEnable;
		VK_FALSE,													//	VkBool32								alphaToOneEnable;
	};

	const VkPipelineDepthStencilStateCreateInfo depthStencilInfo = initVulkanStructure();

	VkPipelineColorBlendAttachmentState colorBlendAttachmentState;
	deMemset(&colorBlendAttachmentState, 0, sizeof(colorBlendAttachmentState));
	colorBlendAttachmentState.colorWriteMask = (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);

	const VkPipelineColorBlendStateCreateInfo colorBlendInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	//	VkStructureType								sType;
		nullptr,													//	const void*									pNext;
		0u,															//	VkPipelineColorBlendStateCreateFlags		flags;
		VK_FALSE,													//	VkBool32									logicOpEnable;
		VK_LOGIC_OP_CLEAR,											//	VkLogicOp									logicOp;
		1u,															//	deUint32									attachmentCount;
		&colorBlendAttachmentState,									//	const VkPipelineColorBlendAttachmentState*	pAttachments;
		{ .0f, .0f, .0f, .0f },										//	float										blendConstants[4];
	};

	const VkGraphicsPipelineCreateInfo pipelineCreateInfo =
	{
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,				//	VkStructureType									sType;
		nullptr,														//	const void*										pNext;
		0u,																//	VkPipelineCreateFlags							flags;
		static_cast<deUint32>(DE_LENGTH_OF_ARRAY(shaderStagesInfo)),	//	deUint32										stageCount;
		shaderStagesInfo,												//	const VkPipelineShaderStageCreateInfo*			pStages;
		&vertexInputInfo,												//	const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
		&inputAssemblyInfo,												//	const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
		&tessInfo,														//	const VkPipelineTessellationStateCreateInfo*	pTessellationState;
		&viewportInfo,													//	const VkPipelineViewportStateCreateInfo*		pViewportState;
		&rasterizationInfo,												//	const VkPipelineRasterizationStateCreateInfo*	pRasterizationState;
		&multisampleInfo,												//	const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
		&depthStencilInfo,												//	const VkPipelineDepthStencilStateCreateInfo*	pDepthStencilState;
		&colorBlendInfo,												//	const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
		nullptr,														//	const VkPipelineDynamicStateCreateInfo*			pDynamicState;
		pipelineLayout.get(),											//	VkPipelineLayout								layout;
		renderPass.get(),												//	VkRenderPass									renderPass;
		0u,																//	deUint32										subpass;
		DE_NULL,														//	VkPipeline										basePipelineHandle;
		0,																//	deInt32											basePipelineIndex;
	};

	const auto graphicsPipeline = createGraphicsPipeline(vkd, device, DE_NULL, &pipelineCreateInfo);

	// Framebuffer.
	const auto framebuffer = makeFramebuffer(vkd, device, renderPass.get(), colorAttachmentView.get(), extent.width, extent.height);

	// Command pool and buffer.
	const auto cmdPool		= makeCommandPool(vkd, device, qIndex);
	const auto cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	// Empty clear color for the framebuffer.
	VkClearValue zeroClearColor;
	deMemset(&zeroClearColor, 0, sizeof(zeroClearColor));

	// Texture barriers to fill it before using it.
	const auto preClearBarrier = makeImageMemoryBarrier(
		0u,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		texture.get(),
		colorSubresourceRange);

	const auto postClearBarrier = makeImageMemoryBarrier(
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		texture.get(),
		colorSubresourceRange);

	// Record and submit.
	beginCommandBuffer(vkd, cmdBuffer);

	// Prepare texture.
	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &preClearBarrier);
	vkd.cmdClearColorImage(cmdBuffer, texture.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &m_params.textureColor, 1u, &colorSubresourceRange);
	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &postClearBarrier);

	// Read from the texture to render a full-screen quad to the color buffer.
	beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissor, zeroClearColor);
	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.get());
	vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0u, 1u, &descriptorSet.get(), 0u, nullptr);
	vkd.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);
	endRenderPass(vkd, cmdBuffer);

	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Verify color buffer.
	const auto	renderSize				= tcu::UVec2(extent.width, extent.height);
	const auto	colorAttachmentLevel	= readColorAttachment(vkd, device, queue, qIndex, alloc, colorAttachment.get(), colorAttachmentFormat, renderSize);
	const auto	colorPixels				= colorAttachmentLevel->getAccess();
	const auto	tcuTextureFormat		= mapVkFormat(m_params.textureFormat);
	const auto	borderColor				= getBorderClearColorValue(m_params);
	const auto	expectedColor			= getExpectedColor(borderColor, m_params);
	std::string	resultMsg;

	if (!comparePixelToColorClearValue(m_params, colorPixels, tcuTextureFormat, expectedColor, resultMsg))
		TCU_FAIL(resultMsg);

	return tcu::TestStatus::pass(resultMsg);
}

using ComponentSwizzleArray = std::array<VkComponentSwizzle, 4>;

// Convert the component swizzle array to a component mapping structure.
void makeComponentMapping(VkComponentMapping& mapping, const ComponentSwizzleArray& array)
{
	mapping.r = array[0];
	mapping.g = array[1];
	mapping.b = array[2];
	mapping.a = array[3];
}

std::string swizzleArrayToString(const ComponentSwizzleArray& swizzles)
{
	std::ostringstream stream;

	for (const auto& s : swizzles)
	{
		switch (s)
		{
		case VK_COMPONENT_SWIZZLE_IDENTITY:	stream << "i"; break;
		case VK_COMPONENT_SWIZZLE_ZERO:		stream << "0"; break;
		case VK_COMPONENT_SWIZZLE_ONE:		stream << "1"; break;
		case VK_COMPONENT_SWIZZLE_R:		stream << "r"; break;
		case VK_COMPONENT_SWIZZLE_G:		stream << "g"; break;
		case VK_COMPONENT_SWIZZLE_B:		stream << "b"; break;
		case VK_COMPONENT_SWIZZLE_A:		stream << "a"; break;
		default:
			DE_ASSERT(false); break;
		}
	}

	return stream.str();
}

// Generate mapping permutations for the swizzle components.
// Note: using every permutation for component swizzle values results in 7^4=2401 combinations, which are too many.
std::vector<ComponentSwizzleArray> genMappingPermutations ()
{
	std::vector<ComponentSwizzleArray>	result;
	const ComponentSwizzleArray			standardSwizzle	= {{ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A }};

	// Standard normal swizzle.
	result.push_back(standardSwizzle);

	// Add a few combinations with rotated swizzles.
	for (size_t rotations = 1u; rotations < standardSwizzle.size(); ++rotations)
	{
		ComponentSwizzleArray rotatedSwizzle = standardSwizzle;
		std::rotate(rotatedSwizzle.begin(), rotatedSwizzle.begin() + rotations, rotatedSwizzle.end());
		result.push_back(rotatedSwizzle);
	}

	// Try placing each special value in each of the positions.
	VkComponentSwizzle specialSwizzles[] = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_ONE, VK_COMPONENT_SWIZZLE_ZERO };
	for (const auto& special : specialSwizzles)
	{
		for (size_t pos = 0; pos < standardSwizzle.size(); ++pos)
		{
			ComponentSwizzleArray newArray = standardSwizzle;
			newArray[pos] = special;
			result.push_back(newArray);
		}
	}

	return result;
}

std::string gatherIndexToString(int gatherIndex)
{
	if (gatherIndex < 0)
		return "no_gather";
	return "gather_" + std::to_string(gatherIndex);
}

bool isIntegerBorder (VkBorderColor borderType)
{
	bool isInt = false;
	switch (borderType)
	{
	case VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK:
	case VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK:
	case VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE:
	case VK_BORDER_COLOR_FLOAT_CUSTOM_EXT:
		isInt = false; break;
	case VK_BORDER_COLOR_INT_TRANSPARENT_BLACK:
	case VK_BORDER_COLOR_INT_OPAQUE_BLACK:
	case VK_BORDER_COLOR_INT_OPAQUE_WHITE:
	case VK_BORDER_COLOR_INT_CUSTOM_EXT:
		isInt = true; break;
	default:
		DE_ASSERT(false); break;
	}

	return isInt;
}

tcu::Vec2 getRandomBorderCoordinates (de::Random& rnd)
{
	tcu::Vec2 coords;

	// Two bits to decide which coordinates will be out of range (at least one).
	const deUint32 outOfRangeMask = static_cast<deUint32>(rnd.getInt(1, 3));

	for (int i = 0; i < 2; ++i)
	{
		// Each coord will be in the [0.0, 0.9] range if in range, [1.1, 5.0] or [-5.0, -1.1] if out of range.
		bool	outOfRange	= (outOfRangeMask & (1<<i));
		bool	negative	= (outOfRange && rnd.getBool());
		float	minCoord	= (outOfRange ? 1.1f : 0.0f);
		float	maxCoord	= (outOfRange ? 5.0f : 0.9f);
		float	value		= (negative ? -1.0f : 1.0f) * rnd.getFloat(minCoord, maxCoord);

		coords[i] = value;
	}

	return coords;
}

// Generate a random clear color usable for the given format.
VkClearColorValue getRandomClearColor (VkFormat format, de::Random& rnd)
{
	VkClearColorValue color;
	deMemset(&color, 0, sizeof(color));

	const auto		tcuFormat		= mapVkFormat(format);
	const auto		numComponents	= tcu::getNumUsedChannels(tcuFormat.order);
	const auto		formatType		= getFormatType(format);

	for (int i = 0; i < numComponents; ++i)
	{
		if (formatType == FormatType::SIGNED_INT || formatType == FormatType::UNSIGNED_INT)
		{
			const auto		componentSize	= tcu::getChannelSize(tcuFormat.type);

			DE_ASSERT(componentSize > 0);

			const deUint64	mask			= (1ull << (componentSize*8)) - 1ull;
			const deUint64	signBit			= (1ull << (componentSize*8-1));
			const deUint64	signMask		= (~mask); // Used to extend the sign bit.
			const auto value = rnd.getUint64();

			if (formatType == FormatType::SIGNED_INT)
			{
				// Extend sign bit for negative values.
				auto finalValue = (value & mask);
				if (finalValue & signBit)
					finalValue |= signMask;
				color.int32[i] = static_cast<deInt32>(finalValue);
			}
			else
				color.uint32[i] = static_cast<deUint32>(value & mask);
		}
		else
			color.float32[i] = rnd.getFloat();
	}

	return color;
}

} // anonymous

tcu::TestCaseGroup* createSamplerBorderSwizzleTests (tcu::TestContext& testCtx)
{
	const deUint32 baseSeed = 1610707317u;

	const VkFormat textureFormats[] =
	{
		//VK_FORMAT_UNDEFINED,
		VK_FORMAT_R4G4_UNORM_PACK8,
		VK_FORMAT_R4G4B4A4_UNORM_PACK16,
		VK_FORMAT_B4G4R4A4_UNORM_PACK16,
		VK_FORMAT_R5G6B5_UNORM_PACK16,
		VK_FORMAT_B5G6R5_UNORM_PACK16,
		VK_FORMAT_R5G5B5A1_UNORM_PACK16,
		VK_FORMAT_B5G5R5A1_UNORM_PACK16,
		VK_FORMAT_A1R5G5B5_UNORM_PACK16,
		VK_FORMAT_R8_UNORM,
		VK_FORMAT_R8_SNORM,
		//VK_FORMAT_R8_USCALED,
		//VK_FORMAT_R8_SSCALED,
		VK_FORMAT_R8_UINT,
		VK_FORMAT_R8_SINT,
		VK_FORMAT_R8_SRGB,
		VK_FORMAT_R8G8_UNORM,
		VK_FORMAT_R8G8_SNORM,
		//VK_FORMAT_R8G8_USCALED,
		//VK_FORMAT_R8G8_SSCALED,
		VK_FORMAT_R8G8_UINT,
		VK_FORMAT_R8G8_SINT,
		VK_FORMAT_R8G8_SRGB,
		VK_FORMAT_R8G8B8_UNORM,
		VK_FORMAT_R8G8B8_SNORM,
		//VK_FORMAT_R8G8B8_USCALED,
		//VK_FORMAT_R8G8B8_SSCALED,
		VK_FORMAT_R8G8B8_UINT,
		VK_FORMAT_R8G8B8_SINT,
		VK_FORMAT_R8G8B8_SRGB,
		VK_FORMAT_B8G8R8_UNORM,
		VK_FORMAT_B8G8R8_SNORM,
		//VK_FORMAT_B8G8R8_USCALED,
		//VK_FORMAT_B8G8R8_SSCALED,
		VK_FORMAT_B8G8R8_UINT,
		VK_FORMAT_B8G8R8_SINT,
		VK_FORMAT_B8G8R8_SRGB,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_R8G8B8A8_SNORM,
		//VK_FORMAT_R8G8B8A8_USCALED,
		//VK_FORMAT_R8G8B8A8_SSCALED,
		VK_FORMAT_R8G8B8A8_UINT,
		VK_FORMAT_R8G8B8A8_SINT,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_FORMAT_B8G8R8A8_SNORM,
		//VK_FORMAT_B8G8R8A8_USCALED,
		//VK_FORMAT_B8G8R8A8_SSCALED,
		VK_FORMAT_B8G8R8A8_UINT,
		VK_FORMAT_B8G8R8A8_SINT,
		VK_FORMAT_B8G8R8A8_SRGB,
		 VK_FORMAT_A8B8G8R8_UNORM_PACK32,
		 VK_FORMAT_A8B8G8R8_SNORM_PACK32,
		// VK_FORMAT_A8B8G8R8_USCALED_PACK32,
		// VK_FORMAT_A8B8G8R8_SSCALED_PACK32,
		// VK_FORMAT_A8B8G8R8_UINT_PACK32,
		// VK_FORMAT_A8B8G8R8_SINT_PACK32,
		// VK_FORMAT_A8B8G8R8_SRGB_PACK32,
		VK_FORMAT_A2R10G10B10_UNORM_PACK32,
		VK_FORMAT_A2R10G10B10_SNORM_PACK32,
		// VK_FORMAT_A2R10G10B10_USCALED_PACK32,
		// VK_FORMAT_A2R10G10B10_SSCALED_PACK32,
		// VK_FORMAT_A2R10G10B10_UINT_PACK32,
		// VK_FORMAT_A2R10G10B10_SINT_PACK32,
		VK_FORMAT_A2B10G10R10_UNORM_PACK32,
		VK_FORMAT_A2B10G10R10_SNORM_PACK32,
		// VK_FORMAT_A2B10G10R10_USCALED_PACK32,
		// VK_FORMAT_A2B10G10R10_SSCALED_PACK32,
		// VK_FORMAT_A2B10G10R10_UINT_PACK32,
		// VK_FORMAT_A2B10G10R10_SINT_PACK32,
		VK_FORMAT_R16_UNORM,
		VK_FORMAT_R16_SNORM,
		//VK_FORMAT_R16_USCALED,
		//VK_FORMAT_R16_SSCALED,
		VK_FORMAT_R16_UINT,
		VK_FORMAT_R16_SINT,
		VK_FORMAT_R16_SFLOAT,
		VK_FORMAT_R16G16_UNORM,
		VK_FORMAT_R16G16_SNORM,
		//VK_FORMAT_R16G16_USCALED,
		//VK_FORMAT_R16G16_SSCALED,
		VK_FORMAT_R16G16_UINT,
		VK_FORMAT_R16G16_SINT,
		VK_FORMAT_R16G16_SFLOAT,
		VK_FORMAT_R16G16B16_UNORM,
		VK_FORMAT_R16G16B16_SNORM,
		//VK_FORMAT_R16G16B16_USCALED,
		//VK_FORMAT_R16G16B16_SSCALED,
		VK_FORMAT_R16G16B16_UINT,
		VK_FORMAT_R16G16B16_SINT,
		VK_FORMAT_R16G16B16_SFLOAT,
		VK_FORMAT_R16G16B16A16_UNORM,
		VK_FORMAT_R16G16B16A16_SNORM,
		//VK_FORMAT_R16G16B16A16_USCALED,
		//VK_FORMAT_R16G16B16A16_SSCALED,
		VK_FORMAT_R16G16B16A16_UINT,
		VK_FORMAT_R16G16B16A16_SINT,
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_FORMAT_R32_UINT,
		VK_FORMAT_R32_SINT,
		VK_FORMAT_R32_SFLOAT,
		VK_FORMAT_R32G32_UINT,
		VK_FORMAT_R32G32_SINT,
		VK_FORMAT_R32G32_SFLOAT,
		VK_FORMAT_R32G32B32_UINT,
		VK_FORMAT_R32G32B32_SINT,
		VK_FORMAT_R32G32B32_SFLOAT,
		VK_FORMAT_R32G32B32A32_UINT,
		VK_FORMAT_R32G32B32A32_SINT,
		VK_FORMAT_R32G32B32A32_SFLOAT,
	};

	const auto mappingPermutations = genMappingPermutations();

	const struct
	{
		VkBorderColor	borderType;
		const char*		borderTypeName;
	}
	borderColors[] =
	{
		{	VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,	"transparent_black"	},
		{	VK_BORDER_COLOR_INT_TRANSPARENT_BLACK,		"transparent_black"	},
		{	VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,			"opaque_black"		},
		{	VK_BORDER_COLOR_INT_OPAQUE_BLACK,			"opaque_black"		},
		{	VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,			"opaque_white"		},
		{	VK_BORDER_COLOR_INT_OPAQUE_WHITE,			"opaque_white"		},
		{	VK_BORDER_COLOR_FLOAT_CUSTOM_EXT,			"custom"			},
		{	VK_BORDER_COLOR_INT_CUSTOM_EXT,				"custom"			},
	};

	const struct
	{
		bool		useSwizzleHint;
		const char*	name;
	} swizzleHintCases[] =
	{
		{ false,	"no_swizzle_hint"	},
		{ true,		"with_swizzle_hint"	},
	};

	de::MovePtr<tcu::TestCaseGroup> mainGroup(new tcu::TestCaseGroup(testCtx, "border_swizzle", "Border color swizzle tests"));

	for (const auto& format : textureFormats)
	{
		const auto						skip		= std::strlen("VK_FORMAT_");
		const std::string				formatName	= de::toLower(std::string(getFormatName(format)).substr(skip));
		de::MovePtr<tcu::TestCaseGroup>	formatGroup	(new tcu::TestCaseGroup(testCtx, formatName.c_str(), ""));

		for (size_t mappingIdx = 0u; mappingIdx < mappingPermutations.size(); ++mappingIdx)
		{
			const auto&						mapping			= mappingPermutations[mappingIdx];
			de::MovePtr<tcu::TestCaseGroup>	mappingGroup	(new tcu::TestCaseGroup(testCtx, swizzleArrayToString(mapping).c_str(), ""));

			for (int borderColorIdx = 0; borderColorIdx < DE_LENGTH_OF_ARRAY(borderColors); ++borderColorIdx)
			{
				const auto&						borderColor		= borderColors[borderColorIdx];
				de::MovePtr<tcu::TestCaseGroup>	borderTypeGroup	(new tcu::TestCaseGroup(testCtx, borderColor.borderTypeName, ""));

				const auto formatType	= getFormatType(format);
				const auto isIntBorder	= isIntegerBorder(borderColor.borderType);

				// Skip cases that do not make sense for the format and border type combination.
				if (isIntBorder && formatType == FormatType::FLOAT)
					continue;
				else if (!isIntBorder && formatType != FormatType::FLOAT)
					continue;

				for (int gatherIdx = -1; gatherIdx <= 3; ++gatherIdx)
				{
					const auto						componentGather	= gatherIndexToString(gatherIdx);
					de::MovePtr<tcu::TestCaseGroup>	gatherGroup		(new tcu::TestCaseGroup(testCtx, componentGather.c_str(), ""));

					for (const auto& swizzleHint : swizzleHintCases)
					{
						TestParams params;

						const deUint32	seed	= baseSeed + static_cast<deUint32>(format) + static_cast<deUint32>(mappingIdx) + static_cast<deUint32>(borderColorIdx) + static_cast<deUint32>(gatherIdx);
						de::Random		rnd		(seed);

						params.textureFormat	= format;
						params.textureColor		= getRandomClearColor(format, rnd);

						makeComponentMapping(params.componentMapping, mapping);
						params.borderColor			= borderColor.borderType;
						params.componentGather		= ((gatherIdx < 0) ? tcu::nothing<int>() : tcu::just(gatherIdx));
						params.textureCoordinates	= getRandomBorderCoordinates(rnd);

						if (params.isCustom())
							params.customBorderColor = tcu::just(getRandomClearColor(format, rnd));
						else
							params.customBorderColor = tcu::nothing<VkClearColorValue>();

						params.useSamplerSwizzleHint = swizzleHint.useSwizzleHint;

						gatherGroup->addChild(new BorderSwizzleCase(testCtx, swizzleHint.name, "", params));
					}

					borderTypeGroup->addChild(gatherGroup.release());
				}

				mappingGroup->addChild(borderTypeGroup.release());
			}

			formatGroup->addChild(mappingGroup.release());
		}

		mainGroup->addChild(formatGroup.release());
	}

	return mainGroup.release();
}

} // pipeline
} // vkt



