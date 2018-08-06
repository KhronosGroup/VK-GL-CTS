/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 Google Inc.
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
 * \brief Texture color conversion tests
 *//*--------------------------------------------------------------------*/

#include "vktYCbCrConversionTests.hpp"

#include "vktShaderExecutor.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktYCbCrUtil.hpp"

#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkQueryUtil.hpp"

#include "tcuInterval.hpp"
#include "tcuTestLog.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVector.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuFloatFormat.hpp"
#include "tcuFloat.hpp"

#include "deRandom.hpp"
#include "deSTLUtil.hpp"
#include "deSharedPtr.hpp"

#include "deMath.h"
#include "deFloat16.h"

#include <vector>
#include <iomanip>

// \todo When defined color conversion extension is not used and conversion is performed in the shader
// #define FAKE_COLOR_CONVERSION

using tcu::Vec2;
using tcu::Vec4;

using tcu::UVec2;
using tcu::UVec4;

using tcu::IVec2;
using tcu::IVec3;
using tcu::IVec4;

using tcu::TestLog;
using tcu::FloatFormat;

using std::vector;
using std::string;

using namespace vkt::shaderexecutor;

namespace vkt
{
namespace ycbcr
{
namespace
{
typedef de::SharedPtr<vk::Unique<vk::VkBuffer> > VkBufferSp;
typedef de::SharedPtr<vk::Allocation> AllocationSp;

ShaderSpec createShaderSpec (void)
{
	ShaderSpec spec;

	spec.globalDeclarations = "layout(set=" + de::toString((int)EXTRA_RESOURCES_DESCRIPTOR_SET_INDEX) + ", binding=0) uniform highp sampler2D u_sampler;";

	spec.inputs.push_back(Symbol("uv", glu::VarType(glu::TYPE_FLOAT_VEC2, glu::PRECISION_HIGHP)));
	spec.outputs.push_back(Symbol("o_color", glu::VarType(glu::TYPE_FLOAT_VEC4, glu::PRECISION_HIGHP)));

	spec.source = "o_color = texture(u_sampler, uv);\n";

	return spec;
}

void genTexCoords (std::vector<Vec2>&	coords,
				   const UVec2&			size)
{
	for (deUint32 y = 0; y < size.y() + (size.y() / 2); y++)
	for (deUint32 x = 0; x < size.x() + (size.x() / 2); x++)
	{
		const float	fx	= (float)x;
		const float	fy	= (float)y;

		const float	fw	= (float)size.x();
		const float	fh	= (float)size.y();

		const float	s	= 1.5f * ((fx * 1.5f * fw + fx) / (1.5f * fw * 1.5f * fw)) - 0.25f;
		const float	t	= 1.5f * ((fy * 1.5f * fh + fy) / (1.5f * fh * 1.5f * fh)) - 0.25f;

		coords.push_back(Vec2(s, t));
	}
}

struct TestConfig
{
	TestConfig	(glu::ShaderType						shaderType_,
				 vk::VkFormat							format_,
				 vk::VkImageTiling						imageTiling_,
				 vk::VkFilter							textureFilter_,
				 vk::VkSamplerAddressMode				addressModeU_,
				 vk::VkSamplerAddressMode				addressModeV_,

				 vk::VkFilter							chromaFilter_,
				 vk::VkChromaLocation					xChromaOffset_,
				 vk::VkChromaLocation					yChromaOffset_,
				 bool									explicitReconstruction_,
				 bool									disjoint_,

				 vk::VkSamplerYcbcrRange				colorRange_,
				 vk::VkSamplerYcbcrModelConversion		colorModel_,
				 vk::VkComponentMapping					componentMapping_)
		: shaderType				(shaderType_)
		, format					(format_)
		, imageTiling				(imageTiling_)
		, textureFilter				(textureFilter_)
		, addressModeU				(addressModeU_)
		, addressModeV				(addressModeV_)

		, chromaFilter				(chromaFilter_)
		, xChromaOffset				(xChromaOffset_)
		, yChromaOffset				(yChromaOffset_)
		, explicitReconstruction	(explicitReconstruction_)
		, disjoint					(disjoint_)

		, colorRange				(colorRange_)
		, colorModel				(colorModel_)
		, componentMapping			(componentMapping_)
	{
	}

	glu::ShaderType							shaderType;
	vk::VkFormat							format;
	vk::VkImageTiling						imageTiling;
	vk::VkFilter							textureFilter;
	vk::VkSamplerAddressMode				addressModeU;
	vk::VkSamplerAddressMode				addressModeV;

	vk::VkFilter							chromaFilter;
	vk::VkChromaLocation					xChromaOffset;
	vk::VkChromaLocation					yChromaOffset;
	bool									explicitReconstruction;
	bool									disjoint;

	vk::VkSamplerYcbcrRange					colorRange;
	vk::VkSamplerYcbcrModelConversion		colorModel;
	vk::VkComponentMapping					componentMapping;
};

vk::Move<vk::VkDescriptorSetLayout> createDescriptorSetLayout (const vk::DeviceInterface&	vkd,
															   vk::VkDevice					device,
															   vk::VkSampler				sampler)
{
	const vk::VkDescriptorSetLayoutBinding		layoutBindings[]	=
	{
		{
			0u,
			vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			1u,
			vk::VK_SHADER_STAGE_ALL,
			&sampler
		}
	};
	const vk::VkDescriptorSetLayoutCreateInfo	layoutCreateInfo	=
	{
		vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		DE_NULL,

		0u,
		DE_LENGTH_OF_ARRAY(layoutBindings),
		layoutBindings
	};

	return vk::createDescriptorSetLayout(vkd, device, &layoutCreateInfo);
}

vk::Move<vk::VkDescriptorPool> createDescriptorPool (const vk::DeviceInterface&	vkd,
													 vk::VkDevice				device)
{
	const vk::VkDescriptorPoolSize			poolSizes[]					=
	{
		{ vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1u, }
	};
	const vk::VkDescriptorPoolCreateInfo	descriptorPoolCreateInfo	=
	{
		vk::VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		DE_NULL,
		vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,

		1u,
		DE_LENGTH_OF_ARRAY(poolSizes),
		poolSizes
	};

	return createDescriptorPool(vkd, device, &descriptorPoolCreateInfo);
}

vk::Move<vk::VkDescriptorSet> createDescriptorSet (const vk::DeviceInterface&	vkd,
												   vk::VkDevice					device,
												   vk::VkDescriptorPool			descriptorPool,
												   vk::VkDescriptorSetLayout	layout,
												   vk::VkSampler				sampler,
												   vk::VkImageView				imageView)
{
	const vk::VkDescriptorSetAllocateInfo		descriptorSetAllocateInfo	=
	{
		vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		DE_NULL,

		descriptorPool,
		1u,
		&layout
	};
	vk::Move<vk::VkDescriptorSet>	descriptorSet	(vk::allocateDescriptorSet(vkd, device, &descriptorSetAllocateInfo));
	const vk::VkDescriptorImageInfo	imageInfo		=
	{
		sampler,
		imageView,
		vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	};

	{
		const vk::VkWriteDescriptorSet	writes[]	=
		{
			{
				vk::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				DE_NULL,

				*descriptorSet,
				0u,
				0u,
				1u,
				vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				&imageInfo,
				DE_NULL,
				DE_NULL
			}
		};

		vkd.updateDescriptorSets(device, DE_LENGTH_OF_ARRAY(writes), writes, 0u, DE_NULL);
	}

	return descriptorSet;
}

vk::Move<vk::VkSampler> createSampler (const vk::DeviceInterface&		vkd,
									   vk::VkDevice						device,
									   vk::VkFilter						textureFilter,
									   vk::VkSamplerAddressMode			addressModeU,
									   vk::VkSamplerAddressMode			addressModeV,
									   vk::VkSamplerYcbcrConversion		conversion)
{
#if !defined(FAKE_COLOR_CONVERSION)
	const vk::VkSamplerYcbcrConversionInfo	samplerConversionInfo	=
	{
		vk::VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO,
		DE_NULL,
		conversion
	};
#else
	DE_UNREF(conversion);
#endif
	const vk::VkSamplerCreateInfo	createInfo	=
	{
		vk::VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
#if !defined(FAKE_COLOR_CONVERSION)
		&samplerConversionInfo,
#else
		DE_NULL,
#endif

		0u,
		textureFilter,
		textureFilter,
		vk::VK_SAMPLER_MIPMAP_MODE_NEAREST,
		addressModeU,
		addressModeV,
		vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		0.0f,
		VK_FALSE,
		1.0f,
		VK_FALSE,
		vk::VK_COMPARE_OP_ALWAYS,
		0.0f,
		0.0f,
		vk::VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
		VK_FALSE,
	};

	return createSampler(vkd, device, &createInfo);
}

vk::Move<vk::VkImage> createImage (const vk::DeviceInterface&	vkd,
								   vk::VkDevice					device,
								   vk::VkFormat					format,
								   const UVec2&					size,
								   bool							disjoint,
								   vk::VkImageTiling			tiling)
{
	const vk::VkImageCreateInfo createInfo =
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		DE_NULL,
		disjoint ? (vk::VkImageCreateFlags)vk::VK_IMAGE_CREATE_DISJOINT_BIT : (vk::VkImageCreateFlags)0u,

		vk::VK_IMAGE_TYPE_2D,
		format,
		vk::makeExtent3D(size.x(), size.y(), 1u),
		1u,
		1u,
		vk::VK_SAMPLE_COUNT_1_BIT,
		tiling,
		vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT | vk::VK_IMAGE_USAGE_SAMPLED_BIT,
		vk::VK_SHARING_MODE_EXCLUSIVE,
		0u,
		(const deUint32*)DE_NULL,
		vk::VK_IMAGE_LAYOUT_UNDEFINED,
	};

	return vk::createImage(vkd, device, &createInfo);
}

vk::Move<vk::VkImageView> createImageView (const vk::DeviceInterface&		vkd,
										   vk::VkDevice						device,
										   vk::VkImage						image,
										   vk::VkFormat						format,
										   vk::VkSamplerYcbcrConversion		conversion)
{
#if !defined(FAKE_COLOR_CONVERSION)
	const vk::VkSamplerYcbcrConversionInfo	conversionInfo	=
	{
		vk::VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO,
		DE_NULL,
		conversion
	};
#else
	DE_UNREF(conversion);
#endif
	const vk::VkImageViewCreateInfo				viewInfo		=
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
#if defined(FAKE_COLOR_CONVERSION)
		DE_NULL,
#else
		&conversionInfo,
#endif
		(vk::VkImageViewCreateFlags)0,

		image,
		vk::VK_IMAGE_VIEW_TYPE_2D,
		format,
		{
			vk::VK_COMPONENT_SWIZZLE_IDENTITY,
			vk::VK_COMPONENT_SWIZZLE_IDENTITY,
			vk::VK_COMPONENT_SWIZZLE_IDENTITY,
			vk::VK_COMPONENT_SWIZZLE_IDENTITY,
		},
		{ vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u },
	};

	return vk::createImageView(vkd, device, &viewInfo);
}

vk::Move<vk::VkSamplerYcbcrConversion> createConversion (const vk::DeviceInterface&				vkd,
														 vk::VkDevice							device,
														 vk::VkFormat							format,
														 vk::VkSamplerYcbcrModelConversion		colorModel,
														 vk::VkSamplerYcbcrRange				colorRange,
														 vk::VkChromaLocation					xChromaOffset,
														 vk::VkChromaLocation					yChromaOffset,
														 vk::VkFilter							chromaFilter,
														 const vk::VkComponentMapping&			componentMapping,
														 bool									explicitReconstruction)
{
	const vk::VkSamplerYcbcrConversionCreateInfo	conversionInfo	=
	{
		vk::VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO,
		DE_NULL,

		format,
		colorModel,
		colorRange,
		componentMapping,
		xChromaOffset,
		yChromaOffset,
		chromaFilter,
		explicitReconstruction ? VK_TRUE : VK_FALSE
	};

	return vk::createSamplerYcbcrConversion(vkd, device, &conversionInfo);
}

void evalShader (Context&								context,
				 glu::ShaderType						shaderType,
				 const MultiPlaneImageData&				imageData,
				 const UVec2&							size,
				 vk::VkFormat							format,
				 vk::VkImageTiling						imageTiling,
				 bool									disjoint,
				 vk::VkFilter							textureFilter,
				 vk::VkSamplerAddressMode				addressModeU,
				 vk::VkSamplerAddressMode				addressModeV,
				 vk::VkSamplerYcbcrModelConversion		colorModel,
				 vk::VkSamplerYcbcrRange				colorRange,
				 vk::VkChromaLocation					xChromaOffset,
				 vk::VkChromaLocation					yChromaOffset,
				 vk::VkFilter							chromaFilter,
				 const vk::VkComponentMapping&			componentMapping,
				 bool									explicitReconstruction,
				 const vector<Vec2>&					sts,
				 vector<Vec4>&							results)
{
	const vk::DeviceInterface&							vkd					(context.getDeviceInterface());
	const vk::VkDevice									device				(context.getDevice());
#if !defined(FAKE_COLOR_CONVERSION)
	const vk::Unique<vk::VkSamplerYcbcrConversion>		conversion			(createConversion(vkd, device, format, colorModel, colorRange, xChromaOffset, yChromaOffset, chromaFilter, componentMapping, explicitReconstruction));
	const vk::Unique<vk::VkSampler>						sampler				(createSampler(vkd, device, textureFilter, addressModeU, addressModeV, *conversion));
#else
	DE_UNREF(colorModel);
	DE_UNREF(colorRange);
	DE_UNREF(xChromaOffset);
	DE_UNREF(yChromaOffset);
	DE_UNREF(chromaFilter);
	DE_UNREF(explicitReconstruction);
	DE_UNREF(componentMapping);
	DE_UNREF(createConversion);
	const vk::Unique<vk::VkSampler>						sampler				(createSampler(vkd, device, textureFilter, addressModeU, addressModeV, (vk::VkSamplerYcbcrConversion)0u));
#endif
	const vk::Unique<vk::VkImage>						image				(createImage(vkd, device, format, size, disjoint, imageTiling));
	const vk::MemoryRequirement							memoryRequirement	(imageTiling == vk::VK_IMAGE_TILING_OPTIMAL
																			? vk::MemoryRequirement::Any
																			: vk::MemoryRequirement::HostVisible);
	const vk::VkImageCreateFlags						createFlags			(disjoint ? vk::VK_IMAGE_CREATE_DISJOINT_BIT : (vk::VkImageCreateFlagBits)0u);
	const vector<AllocationSp>							imageMemory			(allocateAndBindImageMemory(vkd, device, context.getDefaultAllocator(), *image, format, createFlags, memoryRequirement));
#if defined(FAKE_COLOR_CONVERSION)
	const vk::Unique<vk::VkImageView>					imageView			(createImageView(vkd, device, *image, format, (vk::VkSamplerYcbcrConversion)0));
#else
	const vk::Unique<vk::VkImageView>					imageView			(createImageView(vkd, device, *image, format, *conversion));
#endif

	const vk::Unique<vk::VkDescriptorSetLayout>			layout				(createDescriptorSetLayout(vkd, device, *sampler));
	const vk::Unique<vk::VkDescriptorPool>				descriptorPool		(createDescriptorPool(vkd, device));
	const vk::Unique<vk::VkDescriptorSet>				descriptorSet		(createDescriptorSet(vkd, device, *descriptorPool, *layout, *sampler, *imageView));

	const ShaderSpec									spec				(createShaderSpec());
	const de::UniquePtr<ShaderExecutor>					executor			(createExecutor(context, shaderType, spec, *layout));

	if (imageTiling == vk::VK_IMAGE_TILING_OPTIMAL)
		uploadImage(vkd, device, context.getUniversalQueueFamilyIndex(), context.getDefaultAllocator(), *image, imageData, vk::VK_ACCESS_SHADER_READ_BIT, vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	else
		fillImageMemory(vkd, device, context.getUniversalQueueFamilyIndex(), *image, imageMemory, imageData, vk::VK_ACCESS_SHADER_READ_BIT, vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	results.resize(sts.size());

	{
		const void* const	inputs[]	=
		{
			&sts[0]
		};
		void* const			outputs[]	=
		{
			&results[0]
		};

		executor->execute((int)sts.size(), inputs, outputs, *descriptorSet);
	}
}

void logTestCaseInfo (TestLog& log, const TestConfig& config)
{
	log << TestLog::Message << "ShaderType: " << config.shaderType << TestLog::EndMessage;
	log << TestLog::Message << "Format: "  << config.format << TestLog::EndMessage;
	log << TestLog::Message << "ImageTiling: " << config.imageTiling << TestLog::EndMessage;
	log << TestLog::Message << "TextureFilter: " << config.textureFilter << TestLog::EndMessage;
	log << TestLog::Message << "AddressModeU: " << config.addressModeU << TestLog::EndMessage;
	log << TestLog::Message << "AddressModeV: " << config.addressModeV << TestLog::EndMessage;
	log << TestLog::Message << "ChromaFilter: " << config.chromaFilter << TestLog::EndMessage;
	log << TestLog::Message << "XChromaOffset: " << config.xChromaOffset << TestLog::EndMessage;
	log << TestLog::Message << "YChromaOffset: " << config.yChromaOffset << TestLog::EndMessage;
	log << TestLog::Message << "ExplicitReconstruction: " << (config.explicitReconstruction ? "true" : "false") << TestLog::EndMessage;
	log << TestLog::Message << "Disjoint: " << (config.disjoint ? "true" : "false") << TestLog::EndMessage;
	log << TestLog::Message << "ColorRange: " << config.colorRange << TestLog::EndMessage;
	log << TestLog::Message << "ColorModel: " << config.colorModel << TestLog::EndMessage;
	log << TestLog::Message << "ComponentMapping: " << config.componentMapping << TestLog::EndMessage;
}


tcu::TestStatus textureConversionTest (Context& context, const TestConfig config)
{
	const FloatFormat	filteringPrecision		(getYCbCrFilteringPrecision(config.format));
	const FloatFormat	conversionPrecision		(getYCbCrConversionPrecision(config.format));
	const deUint32		subTexelPrecisionBits	(vk::getPhysicalDeviceProperties(context.getInstanceInterface(), context.getPhysicalDevice()).limits.subTexelPrecisionBits);
	const tcu::UVec4	bitDepth				(getYCbCrBitDepth(config.format));
	const UVec2			size					(isXChromaSubsampled(config.format) ? 12 : 7,
												 isYChromaSubsampled(config.format) ?  8 : 13);
	TestLog&			log						(context.getTestContext().getLog());
	bool				explicitReconstruction	= config.explicitReconstruction;
	bool				isOk					= true;

	logTestCaseInfo(log, config);

#if !defined(FAKE_COLOR_CONVERSION)
	if (!vk::isDeviceExtensionSupported(context.getUsedApiVersion(), context.getDeviceExtensions(), "VK_KHR_sampler_ycbcr_conversion"))
		TCU_THROW(NotSupportedError, "Extension VK_KHR_sampler_ycbcr_conversion not supported");

	try
	{
		const vk::VkFormatProperties	properties	(vk::getPhysicalDeviceFormatProperties(context.getInstanceInterface(), context.getPhysicalDevice(), config.format));
		const vk::VkFormatFeatureFlags	features	(config.imageTiling == vk::VK_IMAGE_TILING_OPTIMAL
													? properties.optimalTilingFeatures
													: properties.linearTilingFeatures);

		if ((features & (vk::VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT | vk::VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT)) == 0)
			TCU_THROW(NotSupportedError, "Format doesn't support YCbCr conversions");

		if ((features & vk::VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) == 0)
			TCU_THROW(NotSupportedError, "Format doesn't support sampling");

		if (config.textureFilter == vk::VK_FILTER_LINEAR && ((features & vk::VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT) == 0))
			TCU_THROW(NotSupportedError, "Format doesn't support YCbCr linear chroma reconstruction");

		if (config.chromaFilter == vk::VK_FILTER_LINEAR && ((features & vk::VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT) == 0))
			TCU_THROW(NotSupportedError, "Format doesn't support YCbCr linear chroma reconstruction");

		if (config.chromaFilter != config.textureFilter && ((features & vk::VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_SEPARATE_RECONSTRUCTION_FILTER_BIT) == 0))
			TCU_THROW(NotSupportedError, "Format doesn't support different chroma and texture filters");

		if (config.explicitReconstruction && ((features & vk::VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_FORCEABLE_BIT) == 0))
			TCU_THROW(NotSupportedError, "Format doesn't support explicit chroma reconstruction");

		if (config.disjoint && ((features & vk::VK_FORMAT_FEATURE_DISJOINT_BIT) == 0))
			TCU_THROW(NotSupportedError, "Format doesn't disjoint planes");

		if (isXChromaSubsampled(config.format) && (config.xChromaOffset == vk::VK_CHROMA_LOCATION_COSITED_EVEN) && ((features & vk::VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT) == 0))
			TCU_THROW(NotSupportedError, "Format doesn't support cosited chroma samples");

		if (isXChromaSubsampled(config.format) && (config.xChromaOffset == vk::VK_CHROMA_LOCATION_MIDPOINT) && ((features & vk::VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT) == 0))
			TCU_THROW(NotSupportedError, "Format doesn't support midpoint chroma samples");

		if (isYChromaSubsampled(config.format) && (config.yChromaOffset == vk::VK_CHROMA_LOCATION_COSITED_EVEN) && ((features & vk::VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT) == 0))
			TCU_THROW(NotSupportedError, "Format doesn't support cosited chroma samples");

		if (isYChromaSubsampled(config.format) && (config.yChromaOffset == vk::VK_CHROMA_LOCATION_MIDPOINT) && ((features & vk::VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT) == 0))
			TCU_THROW(NotSupportedError, "Format doesn't support midpoint chroma samples");

		if ((features & vk::VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_BIT) != 0)
			explicitReconstruction = true;

		log << TestLog::Message << "FormatFeatures: " << vk::getFormatFeatureFlagsStr(features) << TestLog::EndMessage;
	}
	catch (const vk::Error& err)
	{
		if (err.getError() == vk::VK_ERROR_FORMAT_NOT_SUPPORTED)
			TCU_THROW(NotSupportedError, "Format not supported");

		throw;
	}
#endif

	{
		const vk::PlanarFormatDescription	planeInfo					(vk::getPlanarFormatDescription(config.format));
		MultiPlaneImageData					src							(config.format, size);

		deUint32							nullAccessData				(0u);
		ChannelAccess						nullAccess					(tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT, 1u, IVec3(size.x(), size.y(), 1), IVec3(0, 0, 0), &nullAccessData, 0u);
		deUint32							nullAccessAlphaData			(~0u);
		ChannelAccess						nullAccessAlpha				(tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT, 1u, IVec3(size.x(), size.y(), 1), IVec3(0, 0, 0), &nullAccessAlphaData, 0u);
		ChannelAccess						rChannelAccess				(planeInfo.hasChannelNdx(0) ? getChannelAccess(src, planeInfo, size, 0) : nullAccess);
		ChannelAccess						gChannelAccess				(planeInfo.hasChannelNdx(1) ? getChannelAccess(src, planeInfo, size, 1) : nullAccess);
		ChannelAccess						bChannelAccess				(planeInfo.hasChannelNdx(2) ? getChannelAccess(src, planeInfo, size, 2) : nullAccess);
		ChannelAccess						aChannelAccess				(planeInfo.hasChannelNdx(3) ? getChannelAccess(src, planeInfo, size, 3) : nullAccessAlpha);
		const bool							implicitNearestCosited		((config.chromaFilter == vk::VK_FILTER_NEAREST && !config.explicitReconstruction) &&
																		 (config.xChromaOffset == vk::VK_CHROMA_LOCATION_COSITED_EVEN_KHR || config.yChromaOffset == vk::VK_CHROMA_LOCATION_COSITED_EVEN_KHR));

		vector<Vec2>						sts;
		vector<Vec4>						results;
		vector<Vec4>						minBounds;
		vector<Vec4>						minMidpointBounds;
		vector<Vec4>						maxBounds;
		vector<Vec4>						maxMidpointBounds;
		vector<Vec4>						uvBounds;
		vector<IVec4>						ijBounds;

		for (deUint32 planeNdx = 0; planeNdx < planeInfo.numPlanes; planeNdx++)
			deMemset(src.getPlanePtr(planeNdx), 0u, src.getPlaneSize(planeNdx));

		// \todo Limit values to only values that produce defined values using selected colorRange and colorModel? The verification code handles those cases already correctly.
		if (planeInfo.hasChannelNdx(0))
		{
			for (int y = 0; y < rChannelAccess.getSize().y(); y++)
			for (int x = 0; x < rChannelAccess.getSize().x(); x++)
				rChannelAccess.setChannel(IVec3(x, y, 0), (float)x / (float)rChannelAccess.getSize().x());
		}

		if (planeInfo.hasChannelNdx(1))
		{
			for (int y = 0; y < gChannelAccess.getSize().y(); y++)
			for (int x = 0; x < gChannelAccess.getSize().x(); x++)
				gChannelAccess.setChannel(IVec3(x, y, 0), (float)y / (float)gChannelAccess.getSize().y());
		}

		if (planeInfo.hasChannelNdx(2))
		{
			for (int y = 0; y < bChannelAccess.getSize().y(); y++)
			for (int x = 0; x < bChannelAccess.getSize().x(); x++)
				bChannelAccess.setChannel(IVec3(x, y, 0), (float)(x + y) / (float)(bChannelAccess.getSize().x() + bChannelAccess.getSize().y()));
		}

		if (planeInfo.hasChannelNdx(3))
		{
			for (int y = 0; y < aChannelAccess.getSize().y(); y++)
			for (int x = 0; x < aChannelAccess.getSize().x(); x++)
				aChannelAccess.setChannel(IVec3(x, y, 0), (float)(x * y) / (float)(aChannelAccess.getSize().x() * aChannelAccess.getSize().y()));
		}

		genTexCoords(sts, size);

		calculateBounds(rChannelAccess, gChannelAccess, bChannelAccess, aChannelAccess, bitDepth, sts, filteringPrecision, conversionPrecision, subTexelPrecisionBits, config.textureFilter, config.colorModel, config.colorRange, config.chromaFilter, config.xChromaOffset, config.yChromaOffset, config.componentMapping, explicitReconstruction, config.addressModeU, config.addressModeV, minBounds, maxBounds, uvBounds, ijBounds);

		// Handle case: If implicit reconstruction and chromaFilter == NEAREST, an implementation may behave as if both chroma offsets are MIDPOINT.
		if (implicitNearestCosited)
		{
			calculateBounds(rChannelAccess, gChannelAccess, bChannelAccess, aChannelAccess, bitDepth, sts, filteringPrecision, conversionPrecision, subTexelPrecisionBits, config.textureFilter, config.colorModel, config.colorRange, config.chromaFilter, vk::VK_CHROMA_LOCATION_MIDPOINT_KHR, vk::VK_CHROMA_LOCATION_MIDPOINT_KHR, config.componentMapping, explicitReconstruction, config.addressModeU, config.addressModeV, minMidpointBounds, maxMidpointBounds, uvBounds, ijBounds);
		}

		if (vk::isYCbCrFormat(config.format))
		{
			tcu::TextureLevel	rImage	(tcu::TextureFormat(tcu::TextureFormat::R, tcu::TextureFormat::FLOAT), rChannelAccess.getSize().x(), rChannelAccess.getSize().y());
			tcu::TextureLevel	gImage	(tcu::TextureFormat(tcu::TextureFormat::R, tcu::TextureFormat::FLOAT), gChannelAccess.getSize().x(), gChannelAccess.getSize().y());
			tcu::TextureLevel	bImage	(tcu::TextureFormat(tcu::TextureFormat::R, tcu::TextureFormat::FLOAT), bChannelAccess.getSize().x(), bChannelAccess.getSize().y());
			tcu::TextureLevel	aImage	(tcu::TextureFormat(tcu::TextureFormat::R, tcu::TextureFormat::FLOAT), aChannelAccess.getSize().x(), aChannelAccess.getSize().y());

			for (int y = 0; y < (int)rChannelAccess.getSize().y(); y++)
			for (int x = 0; x < (int)rChannelAccess.getSize().x(); x++)
				rImage.getAccess().setPixel(Vec4(rChannelAccess.getChannel(IVec3(x, y, 0))), x, y);

			for (int y = 0; y < (int)gChannelAccess.getSize().y(); y++)
			for (int x = 0; x < (int)gChannelAccess.getSize().x(); x++)
				gImage.getAccess().setPixel(Vec4(gChannelAccess.getChannel(IVec3(x, y, 0))), x, y);

			for (int y = 0; y < (int)bChannelAccess.getSize().y(); y++)
			for (int x = 0; x < (int)bChannelAccess.getSize().x(); x++)
				bImage.getAccess().setPixel(Vec4(bChannelAccess.getChannel(IVec3(x, y, 0))), x, y);

			for (int y = 0; y < (int)aChannelAccess.getSize().y(); y++)
			for (int x = 0; x < (int)aChannelAccess.getSize().x(); x++)
				aImage.getAccess().setPixel(Vec4(aChannelAccess.getChannel(IVec3(x, y, 0))), x, y);

			{
				const Vec4	scale	(1.0f);
				const Vec4	bias	(0.0f);

				log << TestLog::Image("SourceImageR", "SourceImageR", rImage.getAccess(), scale, bias);
				log << TestLog::Image("SourceImageG", "SourceImageG", gImage.getAccess(), scale, bias);
				log << TestLog::Image("SourceImageB", "SourceImageB", bImage.getAccess(), scale, bias);
				log << TestLog::Image("SourceImageA", "SourceImageA", aImage.getAccess(), scale, bias);
			}
		}
		else
		{
			tcu::TextureLevel	srcImage	(vk::mapVkFormat(config.format), size.x(), size.y());

			for (int y = 0; y < (int)size.y(); y++)
			for (int x = 0; x < (int)size.x(); x++)
			{
				const IVec3 pos (x, y, 0);
				srcImage.getAccess().setPixel(Vec4(rChannelAccess.getChannel(pos), gChannelAccess.getChannel(pos), bChannelAccess.getChannel(pos), aChannelAccess.getChannel(pos)), x, y);
			}

			log << TestLog::Image("SourceImage", "SourceImage", srcImage.getAccess());
		}

		evalShader(context, config.shaderType, src, size, config.format, config.imageTiling, config.disjoint, config.textureFilter, config.addressModeU, config.addressModeV, config.colorModel, config.colorRange, config.xChromaOffset, config.yChromaOffset, config.chromaFilter, config.componentMapping, config.explicitReconstruction, sts, results);

		{
			tcu::TextureLevel	minImage			(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::FLOAT), size.x() + (size.x() / 2), size.y() + (size.y() / 2));
			tcu::TextureLevel	maxImage			(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::FLOAT), size.x() + (size.x() / 2), size.y() + (size.y() / 2));
			tcu::TextureLevel	minMidpointImage	(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::FLOAT), size.x() + (size.x() / 2), size.y() + (size.y() / 2));
			tcu::TextureLevel	maxMidpointImage	(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::FLOAT), size.x() + (size.x() / 2), size.y() + (size.y() / 2));
			tcu::TextureLevel	resImage			(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::FLOAT), size.x() + (size.x() / 2), size.y() + (size.y() / 2));

			for (int y = 0; y < (int)(size.y() + (size.y() / 2)); y++)
			for (int x = 0; x < (int)(size.x() + (size.x() / 2)); x++)
			{
				const int ndx = x + y * (int)(size.x() + (size.x() / 2));
				minImage.getAccess().setPixel(minBounds[ndx], x, y);
				maxImage.getAccess().setPixel(maxBounds[ndx], x, y);
			}

			for (int y = 0; y < (int)(size.y() + (size.y() / 2)); y++)
			for (int x = 0; x < (int)(size.x() + (size.x() / 2)); x++)
			{
				const int ndx = x + y * (int)(size.x() + (size.x() / 2));
				resImage.getAccess().setPixel(results[ndx], x, y);
			}

			if (implicitNearestCosited)
			{
				for (int y = 0; y < (int)(size.y() + (size.y() / 2)); y++)
				for (int x = 0; x < (int)(size.x() + (size.x() / 2)); x++)
				{
					const int ndx = x + y * (int)(size.x() + (size.x() / 2));
					minMidpointImage.getAccess().setPixel(minMidpointBounds[ndx], x, y);
					maxMidpointImage.getAccess().setPixel(maxMidpointBounds[ndx], x, y);
				}
			}

			{
				const Vec4	scale	(1.0f);
				const Vec4	bias	(0.0f);

				log << TestLog::Image("MinBoundImage", "MinBoundImage", minImage.getAccess(), scale, bias);
				log << TestLog::Image("MaxBoundImage", "MaxBoundImage", maxImage.getAccess(), scale, bias);

				if (implicitNearestCosited)
				{
					log << TestLog::Image("MinMidpointBoundImage", "MinMidpointBoundImage", minMidpointImage.getAccess(), scale, bias);
					log << TestLog::Image("MaxMidpointBoundImage", "MaxMidpointBoundImage", maxMidpointImage.getAccess(), scale, bias);
				}

				log << TestLog::Image("ResultImage", "ResultImage", resImage.getAccess(), scale, bias);
			}
		}

		size_t errorCount = 0;

		for (size_t ndx = 0; ndx < sts.size(); ndx++)
		{
			bool fail;
			if (implicitNearestCosited)
			{
				fail = (tcu::boolAny(tcu::lessThan(results[ndx], minMidpointBounds[ndx])) || tcu::boolAny(tcu::greaterThan(results[ndx], maxMidpointBounds[ndx]))) &&
						(tcu::boolAny(tcu::lessThan(results[ndx], minBounds[ndx])) || tcu::boolAny(tcu::greaterThan(results[ndx], maxBounds[ndx])));
			}
			else
			{
				fail = tcu::boolAny(tcu::lessThan(results[ndx], minBounds[ndx])) || tcu::boolAny(tcu::greaterThan(results[ndx], maxBounds[ndx]));
			}

			if (fail)
			{
				log << TestLog::Message << "Fail: " << sts[ndx] << " " << results[ndx] << TestLog::EndMessage;
				log << TestLog::Message << "  Min : " << minBounds[ndx] << TestLog::EndMessage;
				log << TestLog::Message << "  Max : " << maxBounds[ndx] << TestLog::EndMessage;
				log << TestLog::Message << "  Threshold: " << (maxBounds[ndx] - minBounds[ndx]) << TestLog::EndMessage;
				log << TestLog::Message << "  UMin : " << uvBounds[ndx][0] << TestLog::EndMessage;
				log << TestLog::Message << "  UMax : " << uvBounds[ndx][1] << TestLog::EndMessage;
				log << TestLog::Message << "  VMin : " << uvBounds[ndx][2] << TestLog::EndMessage;
				log << TestLog::Message << "  VMax : " << uvBounds[ndx][3] << TestLog::EndMessage;
				log << TestLog::Message << "  IMin : " << ijBounds[ndx][0] << TestLog::EndMessage;
				log << TestLog::Message << "  IMax : " << ijBounds[ndx][1] << TestLog::EndMessage;
				log << TestLog::Message << "  JMin : " << ijBounds[ndx][2] << TestLog::EndMessage;
				log << TestLog::Message << "  JMax : " << ijBounds[ndx][3] << TestLog::EndMessage;

				if (isXChromaSubsampled(config.format))
				{
					log << TestLog::Message << "  LumaAlphaValues : " << TestLog::EndMessage;
					log << TestLog::Message << "    Offset : (" << ijBounds[ndx][0] << ", " << ijBounds[ndx][2] << ")" << TestLog::EndMessage;

					for (deInt32 j = ijBounds[ndx][2]; j <= ijBounds[ndx][3] + (config.textureFilter == vk::VK_FILTER_LINEAR ? 1 : 0); j++)
					{
						const deInt32		wrappedJ	= wrap(config.addressModeV, j, gChannelAccess.getSize().y());
						bool				first		= true;
						std::ostringstream	line;

						for (deInt32 i = ijBounds[ndx][0]; i <= ijBounds[ndx][1] + (config.textureFilter == vk::VK_FILTER_LINEAR ? 1 : 0); i++)
						{
							const deInt32	wrappedI	= wrap(config.addressModeU, i, gChannelAccess.getSize().x());

							if (!first)
							{
								line << ", ";
								first = false;
							}

							line << "(" << std::setfill(' ') << std::setw(5) << gChannelAccess.getChannelUint(IVec3(wrappedI, wrappedJ, 0))
								<< ", " << std::setfill(' ') << std::setw(5) << aChannelAccess.getChannelUint(IVec3(wrappedI, wrappedJ, 0)) << ")";
						}
						log << TestLog::Message << "    " << line.str() << TestLog::EndMessage;
					}

					{
						const IVec2 chromaIRange	(divFloor(ijBounds[ndx][0], 2) - 1, divFloor(ijBounds[ndx][1] + (config.textureFilter == vk::VK_FILTER_LINEAR ? 1 : 0), 2) + 1);
						const IVec2 chromaJRange	(isYChromaSubsampled(config.format)
													? IVec2(divFloor(ijBounds[ndx][2], 2) - 1, divFloor(ijBounds[ndx][3] + (config.textureFilter == vk::VK_FILTER_LINEAR ? 1 : 0), 2) + 1)
													: IVec2(ijBounds[ndx][2], ijBounds[ndx][3] + (config.textureFilter == vk::VK_FILTER_LINEAR ? 1 : 0)));

						log << TestLog::Message << "  ChromaValues : " << TestLog::EndMessage;
						log << TestLog::Message << "    Offset : (" << chromaIRange[0] << ", " << chromaJRange[0] << ")" << TestLog::EndMessage;

						for (deInt32 j = chromaJRange[0]; j <= chromaJRange[1]; j++)
						{
							const deInt32		wrappedJ	= wrap(config.addressModeV, j, rChannelAccess.getSize().y());
							bool				first		= true;
							std::ostringstream	line;

							for (deInt32 i = chromaIRange[0]; i <= chromaIRange[1]; i++)
							{
								const deInt32	wrappedI	= wrap(config.addressModeU, i, rChannelAccess.getSize().x());

								if (!first)
								{
									line << ", ";
									first = false;
								}

								line << "(" << std::setfill(' ') << std::setw(5) << rChannelAccess.getChannelUint(IVec3(wrappedI, wrappedJ, 0))
									<< ", " << std::setfill(' ') << std::setw(5) << bChannelAccess.getChannelUint(IVec3(wrappedI, wrappedJ, 0)) << ")";
							}
							log << TestLog::Message << "    " << line.str() << TestLog::EndMessage;
						}
					}
				}
				else
				{
					log << TestLog::Message << "  Values : " << TestLog::EndMessage;
					log << TestLog::Message << "    Offset : (" << ijBounds[ndx][0] << ", " << ijBounds[ndx][2] << ")" << TestLog::EndMessage;

					for (deInt32 j = ijBounds[ndx][2]; j <= ijBounds[ndx][3] + (config.textureFilter == vk::VK_FILTER_LINEAR ? 1 : 0); j++)
					{
						const deInt32		wrappedJ	= wrap(config.addressModeV, j, rChannelAccess.getSize().y());
						bool				first		= true;
						std::ostringstream	line;

						for (deInt32 i = ijBounds[ndx][0]; i <= ijBounds[ndx][1] + (config.textureFilter == vk::VK_FILTER_LINEAR ? 1 : 0); i++)
						{
							const deInt32	wrappedI	= wrap(config.addressModeU, i, rChannelAccess.getSize().x());

							if (!first)
							{
								line << ", ";
								first = false;
							}

							line << "(" << std::setfill(' ') << std::setw(5) << rChannelAccess.getChannelUint(IVec3(wrappedI, wrappedJ, 0))
								<< ", " << std::setfill(' ') << std::setw(5) << gChannelAccess.getChannelUint(IVec3(wrappedI, wrappedJ, 0))
								<< ", " << std::setfill(' ') << std::setw(5) << bChannelAccess.getChannelUint(IVec3(wrappedI, wrappedJ, 0))
								<< ", " << std::setfill(' ') << std::setw(5) << aChannelAccess.getChannelUint(IVec3(wrappedI, wrappedJ, 0)) << ")";
						}
						log << TestLog::Message << "    " << line.str() << TestLog::EndMessage;
					}
				}

				errorCount++;
				isOk = false;

				if (errorCount > 30)
				{
					log << TestLog::Message << "Encountered " << errorCount << " errors. Omitting rest of the per result logs." << TestLog::EndMessage;
					break;
				}
			}
		}
	}

	if (isOk)
		return tcu::TestStatus::pass("Pass");
	else
		return tcu::TestStatus::fail("Result comparison failed");
}

#if defined(FAKE_COLOR_CONVERSION)
const char* swizzleToCompName (const char* identity, vk::VkComponentSwizzle swizzle)
{
	switch (swizzle)
	{
		case vk::VK_COMPONENT_SWIZZLE_IDENTITY:	return identity;
		case vk::VK_COMPONENT_SWIZZLE_R:		return "r";
		case vk::VK_COMPONENT_SWIZZLE_G:		return "g";
		case vk::VK_COMPONENT_SWIZZLE_B:		return "b";
		case vk::VK_COMPONENT_SWIZZLE_A:		return "a";
		default:
			DE_FATAL("Unsupported swizzle");
			return DE_NULL;
	}
}
#endif

void createTestShaders (vk::SourceCollections& dst, TestConfig config)
{
#if !defined(FAKE_COLOR_CONVERSION)
	const ShaderSpec spec (createShaderSpec());

	generateSources(config.shaderType, spec, dst);
#else
	const UVec4	bits	(getBitDepth(config.format));
	ShaderSpec	spec;

	spec.globalDeclarations = "layout(set=" + de::toString((int)EXTRA_RESOURCES_DESCRIPTOR_SET_INDEX) + ", binding=0) uniform highp sampler2D u_sampler;";

	spec.inputs.push_back(Symbol("uv", glu::VarType(glu::TYPE_FLOAT_VEC2, glu::PRECISION_HIGHP)));
	spec.outputs.push_back(Symbol("o_color", glu::VarType(glu::TYPE_FLOAT_VEC4, glu::PRECISION_HIGHP)));

	std::ostringstream	source;

	source << "highp vec4 inputColor = texture(u_sampler, uv);\n";

	source << "highp float r = inputColor." << swizzleToCompName("r", config.componentMapping.r) << ";\n";
	source << "highp float g = inputColor." << swizzleToCompName("g", config.componentMapping.g) << ";\n";
	source << "highp float b = inputColor." << swizzleToCompName("b", config.componentMapping.b) << ";\n";
	source << "highp float a = inputColor." << swizzleToCompName("a", config.componentMapping.a) << ";\n";

	switch (config.colorRange)
	{
		case vk::VK_SAMPLER_YCBCR_RANGE_ITU_FULL:
			source << "highp float cr = r - (float(" << (0x1u << (bits[0] - 0x1u)) << ") / float(" << ((0x1u << bits[0]) - 1u) << "));\n";
			source << "highp float y  = g;\n";
			source << "highp float cb = b - (float(" << (0x1u << (bits[2] - 0x1u)) << ") / float(" << ((0x1u << bits[2]) - 1u) << "));\n";
			break;

		case vk::VK_SAMPLER_YCBCR_RANGE_ITU_NARROW:
			source << "highp float cr = (r * float(" << ((0x1u << bits[0]) - 1u) << ") - float(" << (128u * (0x1u << (bits[0] - 8))) << ")) / float(" << (224u * (0x1u << (bits[0] - 8))) << ");\n";
			source << "highp float y  = (g * float(" << ((0x1u << bits[1]) - 1u) << ") - float(" << (16u * (0x1u << (bits[1] - 8))) << ")) / float(" << (219u * (0x1u << (bits[1] - 8))) << ");\n";
			source << "highp float cb = (b * float(" << ((0x1u << bits[2]) - 1u) << ") - float(" << (128u * (0x1u << (bits[2] - 8))) << ")) / float(" << (224u * (0x1u << (bits[2] - 8))) << ");\n";
			break;

		default:
			DE_FATAL("Unknown color range");
	}

	source << "highp vec4 color;\n";

	switch (config.colorModel)
	{
		case vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY:
			source << "color = vec4(r, g, b, a);\n";
			break;

		case vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_IDENTITY:
			source << "color = vec4(cr, y, cb, a);\n";
			break;

		case vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_601:
			source << "color = vec4(y + 1.402 * cr, y - float(" << (0.202008 / 0.587) << ") * cb - float(" << (0.419198 / 0.587) << ") * cr, y + 1.772 * cb, a);\n";
			break;

		case vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_709:
			source << "color = vec4(y + 1.5748 * cr, y - float(" << (0.13397432 / 0.7152) << ") * cb - float(" << (0.33480248 / 0.7152) << ") * cr, y + 1.8556 * cb, a);\n";
			break;

		case vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_2020:
			source << "color = vec4(y + 1.4746 * cr, (y - float(" << (0.11156702 / 0.6780) << ") * cb) - float(" << (0.38737742 / 0.6780) << ") * cr, y + 1.8814 * cb, a);\n";
			break;

		default:
			DE_FATAL("Unknown color model");
	};

	source << "o_color = color;\n";

	spec.source = source.str();
	generateSources(config.shaderType, spec, dst);
#endif
}

struct RangeNamePair
{
	const char*					name;
	vk::VkSamplerYcbcrRange		value;
};


struct ChromaLocationNamePair
{
	const char*					name;
	vk::VkChromaLocation		value;
};

void initTests (tcu::TestCaseGroup* testGroup)
{
	const vk::VkFormat noChromaSubsampledFormats[] =
	{
		vk::VK_FORMAT_R4G4B4A4_UNORM_PACK16,
		vk::VK_FORMAT_B4G4R4A4_UNORM_PACK16,
		vk::VK_FORMAT_R5G6B5_UNORM_PACK16,
		vk::VK_FORMAT_B5G6R5_UNORM_PACK16,
		vk::VK_FORMAT_R5G5B5A1_UNORM_PACK16,
		vk::VK_FORMAT_B5G5R5A1_UNORM_PACK16,
		vk::VK_FORMAT_A1R5G5B5_UNORM_PACK16,
		vk::VK_FORMAT_R8G8B8_UNORM,
		vk::VK_FORMAT_B8G8R8_UNORM,
		vk::VK_FORMAT_R8G8B8A8_UNORM,
		vk::VK_FORMAT_B8G8R8A8_UNORM,
		vk::VK_FORMAT_A8B8G8R8_UNORM_PACK32,
		vk::VK_FORMAT_A2R10G10B10_UNORM_PACK32,
		vk::VK_FORMAT_A2B10G10R10_UNORM_PACK32,
		vk::VK_FORMAT_R16G16B16_UNORM,
		vk::VK_FORMAT_R16G16B16A16_UNORM,
		vk::VK_FORMAT_R10X6_UNORM_PACK16,
		vk::VK_FORMAT_R10X6G10X6_UNORM_2PACK16,
		vk::VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16,
		vk::VK_FORMAT_R12X4_UNORM_PACK16,
		vk::VK_FORMAT_R12X4G12X4_UNORM_2PACK16,
		vk::VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16,
		vk::VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM,
		vk::VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16,
		vk::VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16,
		vk::VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM
	};
	const vk::VkFormat xChromaSubsampledFormats[] =
	{
		vk::VK_FORMAT_G8B8G8R8_422_UNORM,
		vk::VK_FORMAT_B8G8R8G8_422_UNORM,
		vk::VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM,
		vk::VK_FORMAT_G8_B8R8_2PLANE_422_UNORM,

		vk::VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16,
		vk::VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16,
		vk::VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16,
		vk::VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16,
		vk::VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16,
		vk::VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16,
		vk::VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16,
		vk::VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16,
		vk::VK_FORMAT_G16B16G16R16_422_UNORM,
		vk::VK_FORMAT_B16G16R16G16_422_UNORM,
		vk::VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM,
		vk::VK_FORMAT_G16_B16R16_2PLANE_422_UNORM,
	};
	const vk::VkFormat xyChromaSubsampledFormats[] =
	{
		vk::VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM,
		vk::VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
		vk::VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16,
		vk::VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16,
		vk::VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16,
		vk::VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16,
		vk::VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM,
		vk::VK_FORMAT_G16_B16R16_2PLANE_420_UNORM,
	};
	const struct
	{
		const char* const							name;
		const vk::VkSamplerYcbcrModelConversion	value;
	} colorModels[] =
	{
		{ "rgb_identity",	vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY		},
		{ "ycbcr_identity",	vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_IDENTITY	},
		{ "ycbcr_709",		vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_709			},
		{ "ycbcr_601",		vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_601			},
		{ "ycbcr_2020",		vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_2020		}
	};
	const RangeNamePair colorRanges[]	=
	{
		{ "itu_full",		vk::VK_SAMPLER_YCBCR_RANGE_ITU_FULL		},
		{ "itu_narrow",		vk::VK_SAMPLER_YCBCR_RANGE_ITU_NARROW	}
	};
	const ChromaLocationNamePair chromaLocations[] =
	{
		{ "cosited",		vk::VK_CHROMA_LOCATION_COSITED_EVEN	},
		{ "midpoint",		vk::VK_CHROMA_LOCATION_MIDPOINT		}
	};
	const struct
	{
		const char* const	name;
		vk::VkFilter		value;
	} textureFilters[] =
	{
		{ "linear",			vk::VK_FILTER_LINEAR	},
		{ "nearest",		vk::VK_FILTER_NEAREST	}
	};
	// Used by the chroma reconstruction tests
	const vk::VkSamplerYcbcrModelConversion		defaultColorModel		(vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY);
	const vk::VkSamplerYcbcrRange				defaultColorRange		(vk::VK_SAMPLER_YCBCR_RANGE_ITU_FULL);
	const vk::VkComponentMapping				identitySwizzle			=
	{
		vk::VK_COMPONENT_SWIZZLE_IDENTITY,
		vk::VK_COMPONENT_SWIZZLE_IDENTITY,
		vk::VK_COMPONENT_SWIZZLE_IDENTITY,
		vk::VK_COMPONENT_SWIZZLE_IDENTITY
	};
	const vk::VkComponentMapping				swappedChromaSwizzle	=
	{
		vk::VK_COMPONENT_SWIZZLE_B,
		vk::VK_COMPONENT_SWIZZLE_IDENTITY,
		vk::VK_COMPONENT_SWIZZLE_R,
		vk::VK_COMPONENT_SWIZZLE_IDENTITY
	};
	const glu::ShaderType						shaderTypes[]			=
	{
		glu::SHADERTYPE_VERTEX,
		glu::SHADERTYPE_FRAGMENT,
		glu::SHADERTYPE_COMPUTE
	};
	const struct
	{
		const char*			name;
		vk::VkImageTiling	value;
	}											imageTilings[]			=
	{
		{ "tiling_linear",	vk::VK_IMAGE_TILING_LINEAR },
		{ "tiling_optimal",	vk::VK_IMAGE_TILING_OPTIMAL }
	};
	tcu::TestContext&							testCtx					(testGroup->getTestContext());
	de::Random									rng						(1978765638u);

	// Test formats without chroma reconstruction
	for (size_t formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(noChromaSubsampledFormats); formatNdx++)
	{
		const vk::VkFormat						format					(noChromaSubsampledFormats[formatNdx]);
		const std::string						formatName				(de::toLower(std::string(getFormatName(format)).substr(10)));
		de::MovePtr<tcu::TestCaseGroup>			formatGroup				(new tcu::TestCaseGroup(testCtx, formatName.c_str(), ("Tests for color conversion using format " + formatName).c_str()));

		for (size_t modelNdx = 0; modelNdx < DE_LENGTH_OF_ARRAY(colorModels); modelNdx++)
		{
			const char* const						colorModelName		(colorModels[modelNdx].name);
			const vk::VkSamplerYcbcrModelConversion	colorModel			(colorModels[modelNdx].value);

			if (colorModel != vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY && getYCbCrFormatChannelCount(format) < 3)
				continue;

			de::MovePtr<tcu::TestCaseGroup>			colorModelGroup		(new tcu::TestCaseGroup(testCtx, colorModelName, ("Tests for color model " + string(colorModelName)).c_str()));

			if (colorModel == vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY)
			{
				for (size_t textureFilterNdx = 0; textureFilterNdx < DE_LENGTH_OF_ARRAY(textureFilters); textureFilterNdx++)
				{
					const char* const					textureFilterName	(textureFilters[textureFilterNdx].name);
					const vk::VkFilter					textureFilter		(textureFilters[textureFilterNdx].value);

					for (size_t tilingNdx = 0; tilingNdx < DE_LENGTH_OF_ARRAY(imageTilings); tilingNdx++)
					{
						const vk::VkImageTiling			tiling				(imageTilings[tilingNdx].value);
						const char* const				tilingName			(imageTilings[tilingNdx].name);
						const glu::ShaderType			shaderType			(rng.choose<glu::ShaderType>(DE_ARRAY_BEGIN(shaderTypes), DE_ARRAY_END(shaderTypes)));
						const vk::VkSamplerYcbcrRange	colorRange			(rng.choose<RangeNamePair, const RangeNamePair*>(DE_ARRAY_BEGIN(colorRanges), DE_ARRAY_END(colorRanges)).value);
						const vk::VkChromaLocation		chromaLocation		(rng.choose<ChromaLocationNamePair, const ChromaLocationNamePair*>(DE_ARRAY_BEGIN(chromaLocations), DE_ARRAY_END(chromaLocations)).value);

						const TestConfig				config				(shaderType, format, tiling, textureFilter, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
																				textureFilter, chromaLocation, chromaLocation, false, false,
																				colorRange, colorModel, identitySwizzle);

						addFunctionCaseWithPrograms(colorModelGroup.get(), std::string(textureFilterName) + "_" + tilingName, "", createTestShaders, textureConversionTest, config);
					}
				}
			}
			else
			{
				for (size_t rangeNdx = 0; rangeNdx < DE_LENGTH_OF_ARRAY(colorRanges); rangeNdx++)
				{
					const char* const				colorRangeName	(colorRanges[rangeNdx].name);
					const vk::VkSamplerYcbcrRange	colorRange		(colorRanges[rangeNdx].value);

					// Narrow range doesn't really work with formats that have less than 8 bits
					if (colorRange == vk::VK_SAMPLER_YCBCR_RANGE_ITU_NARROW)
					{
						const UVec4					bitDepth		(getYCbCrBitDepth(format));

						if (bitDepth[0] < 8 || bitDepth[1] < 8 || bitDepth[2] < 8)
							continue;
					}

					de::MovePtr<tcu::TestCaseGroup>		colorRangeGroup	(new tcu::TestCaseGroup(testCtx, colorRangeName, ("Tests for color range " + string(colorRangeName)).c_str()));

					for (size_t textureFilterNdx = 0; textureFilterNdx < DE_LENGTH_OF_ARRAY(textureFilters); textureFilterNdx++)
					{
						const char* const				textureFilterName	(textureFilters[textureFilterNdx].name);
						const vk::VkFilter				textureFilter		(textureFilters[textureFilterNdx].value);

						for (size_t tilingNdx = 0; tilingNdx < DE_LENGTH_OF_ARRAY(imageTilings); tilingNdx++)
						{
							const vk::VkImageTiling		tiling				(imageTilings[tilingNdx].value);
							const char* const			tilingName			(imageTilings[tilingNdx].name);
							const glu::ShaderType		shaderType			(rng.choose<glu::ShaderType>(DE_ARRAY_BEGIN(shaderTypes), DE_ARRAY_END(shaderTypes)));
							const vk::VkChromaLocation	chromaLocation		(rng.choose<ChromaLocationNamePair, const ChromaLocationNamePair*>(DE_ARRAY_BEGIN(chromaLocations), DE_ARRAY_END(chromaLocations)).value);
							const TestConfig			config				(shaderType, format, tiling, textureFilter, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
																				textureFilter, chromaLocation, chromaLocation, false, false,
																				colorRange, colorModel, identitySwizzle);

							addFunctionCaseWithPrograms(colorRangeGroup.get(), std::string(textureFilterName) + "_" + tilingName, "", createTestShaders, textureConversionTest, config);
						}
					}

					colorModelGroup->addChild(colorRangeGroup.release());
				}
			}

			formatGroup->addChild(colorModelGroup.release());
		}

		testGroup->addChild(formatGroup.release());
	}

	// Test formats with x chroma reconstruction
	for (size_t formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(xChromaSubsampledFormats); formatNdx++)
	{
		const vk::VkFormat				format		(xChromaSubsampledFormats[formatNdx]);
		const std::string				formatName	(de::toLower(std::string(getFormatName(format)).substr(10)));
		de::MovePtr<tcu::TestCaseGroup>	formatGroup	(new tcu::TestCaseGroup(testCtx, formatName.c_str(), ("Tests for color conversion using format " + formatName).c_str()));

		// Color conversion tests
		{
			de::MovePtr<tcu::TestCaseGroup>	conversionGroup	(new tcu::TestCaseGroup(testCtx, "color_conversion", ""));

			for (size_t xChromaOffsetNdx = 0; xChromaOffsetNdx < DE_LENGTH_OF_ARRAY(chromaLocations); xChromaOffsetNdx++)
			{
				const char* const			xChromaOffsetName	(chromaLocations[xChromaOffsetNdx].name);
				const vk::VkChromaLocation	xChromaOffset		(chromaLocations[xChromaOffsetNdx].value);

				for (size_t modelNdx = 0; modelNdx < DE_LENGTH_OF_ARRAY(colorModels); modelNdx++)
				{
					const char* const						colorModelName	(colorModels[modelNdx].name);
					const vk::VkSamplerYcbcrModelConversion	colorModel		(colorModels[modelNdx].value);

					if (colorModel != vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY && getYCbCrFormatChannelCount(format) < 3)
						continue;


					if (colorModel == vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY)
					{
						for (size_t tilingNdx = 0; tilingNdx < DE_LENGTH_OF_ARRAY(imageTilings); tilingNdx++)
						{
							const vk::VkImageTiling			tiling			(imageTilings[tilingNdx].value);
							const char* const				tilingName		(imageTilings[tilingNdx].name);
							const vk::VkSamplerYcbcrRange	colorRange		(rng.choose<RangeNamePair, const RangeNamePair*>(DE_ARRAY_BEGIN(colorRanges), DE_ARRAY_END(colorRanges)).value);
							const glu::ShaderType			shaderType		(rng.choose<glu::ShaderType>(DE_ARRAY_BEGIN(shaderTypes), DE_ARRAY_END(shaderTypes)));
							const vk::VkChromaLocation		yChromaOffset	(rng.choose<ChromaLocationNamePair, const ChromaLocationNamePair*>(DE_ARRAY_BEGIN(chromaLocations), DE_ARRAY_END(chromaLocations)).value);
							const TestConfig				config			(shaderType, format, tiling, vk::VK_FILTER_NEAREST, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
																			 vk::VK_FILTER_NEAREST, xChromaOffset, yChromaOffset, false, false,
																			 colorRange, colorModel, identitySwizzle);

							addFunctionCaseWithPrograms(conversionGroup.get(), std::string(colorModelName) + "_" + tilingName + "_" + xChromaOffsetName, "", createTestShaders, textureConversionTest, config);
						}
					}
					else
					{
						for (size_t rangeNdx = 0; rangeNdx < DE_LENGTH_OF_ARRAY(colorRanges); rangeNdx++)
						{
							const char* const				colorRangeName	(colorRanges[rangeNdx].name);
							const vk::VkSamplerYcbcrRange	colorRange		(colorRanges[rangeNdx].value);

							// Narrow range doesn't really work with formats that have less than 8 bits
							if (colorRange == vk::VK_SAMPLER_YCBCR_RANGE_ITU_NARROW)
							{
								const UVec4					bitDepth		(getYCbCrBitDepth(format));

								if (bitDepth[0] < 8 || bitDepth[1] < 8 || bitDepth[2] < 8)
									continue;
							}

							for (size_t tilingNdx = 0; tilingNdx < DE_LENGTH_OF_ARRAY(imageTilings); tilingNdx++)
							{
								const vk::VkImageTiling		tiling			(imageTilings[tilingNdx].value);
								const char* const			tilingName		(imageTilings[tilingNdx].name);
								const glu::ShaderType		shaderType		(rng.choose<glu::ShaderType>(DE_ARRAY_BEGIN(shaderTypes), DE_ARRAY_END(shaderTypes)));
								const vk::VkChromaLocation	yChromaOffset	(rng.choose<ChromaLocationNamePair, const ChromaLocationNamePair*>(DE_ARRAY_BEGIN(chromaLocations), DE_ARRAY_END(chromaLocations)).value);
								const TestConfig			config			(shaderType, format, tiling, vk::VK_FILTER_NEAREST, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
																			vk::VK_FILTER_NEAREST, xChromaOffset, yChromaOffset, false, false,
																			colorRange, colorModel, identitySwizzle);

								addFunctionCaseWithPrograms(conversionGroup.get(), (string(colorModelName) + "_" + colorRangeName + "_" + tilingName + "_" + xChromaOffsetName).c_str(), "", createTestShaders, textureConversionTest, config);
							}
						}
					}
				}
			}

			formatGroup->addChild(conversionGroup.release());
		}

		// Chroma reconstruction tests
		{
			de::MovePtr<tcu::TestCaseGroup>	reconstrucGroup	(new tcu::TestCaseGroup(testCtx, "chroma_reconstruction", ""));

			for (size_t textureFilterNdx = 0; textureFilterNdx < DE_LENGTH_OF_ARRAY(textureFilters); textureFilterNdx++)
			{
				const char* const				textureFilterName	(textureFilters[textureFilterNdx].name);
				const vk::VkFilter				textureFilter		(textureFilters[textureFilterNdx].value);
				de::MovePtr<tcu::TestCaseGroup>	textureFilterGroup	(new tcu::TestCaseGroup(testCtx, textureFilterName, textureFilterName));

				for (size_t explicitReconstructionNdx = 0; explicitReconstructionNdx < 2; explicitReconstructionNdx++)
				{
					const bool	explicitReconstruction	(explicitReconstructionNdx == 1);

					for (size_t disjointNdx = 0; disjointNdx < 2; disjointNdx++)
					{
						const bool	disjoint	(disjointNdx == 1);

						for (size_t xChromaOffsetNdx = 0; xChromaOffsetNdx < DE_LENGTH_OF_ARRAY(chromaLocations); xChromaOffsetNdx++)
						{
							const vk::VkChromaLocation		xChromaOffset		(chromaLocations[xChromaOffsetNdx].value);
							const char* const				xChromaOffsetName	(chromaLocations[xChromaOffsetNdx].name);

							for (size_t tilingNdx = 0; tilingNdx < DE_LENGTH_OF_ARRAY(imageTilings); tilingNdx++)
							{
								const vk::VkImageTiling		tiling				(imageTilings[tilingNdx].value);
								const char* const			tilingName			(imageTilings[tilingNdx].name);

								{
									const glu::ShaderType			shaderType		(rng.choose<glu::ShaderType>(DE_ARRAY_BEGIN(shaderTypes), DE_ARRAY_END(shaderTypes)));
									const vk::VkChromaLocation		yChromaOffset	(rng.choose<ChromaLocationNamePair, const ChromaLocationNamePair*>(DE_ARRAY_BEGIN(chromaLocations), DE_ARRAY_END(chromaLocations)).value);
									const TestConfig				config			(shaderType, format, tiling, textureFilter, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
																						vk::VK_FILTER_LINEAR, xChromaOffset, yChromaOffset, explicitReconstruction, disjoint,
																						defaultColorRange, defaultColorModel, identitySwizzle);

									addFunctionCaseWithPrograms(textureFilterGroup.get(), string(explicitReconstruction ? "explicit_linear_" : "default_linear_") + xChromaOffsetName + "_" + tilingName + (disjoint ? "_disjoint" : ""), "", createTestShaders, textureConversionTest, config);
								}

								{
									const glu::ShaderType			shaderType		(rng.choose<glu::ShaderType>(DE_ARRAY_BEGIN(shaderTypes), DE_ARRAY_END(shaderTypes)));
									const vk::VkChromaLocation		yChromaOffset	(rng.choose<ChromaLocationNamePair, const ChromaLocationNamePair*>(DE_ARRAY_BEGIN(chromaLocations), DE_ARRAY_END(chromaLocations)).value);
									const TestConfig				config			(shaderType, format, tiling, textureFilter, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
																						vk::VK_FILTER_LINEAR, xChromaOffset, yChromaOffset, explicitReconstruction, disjoint,
																						defaultColorRange, defaultColorModel, swappedChromaSwizzle);

									addFunctionCaseWithPrograms(textureFilterGroup.get(), string(explicitReconstruction ? "explicit_linear_" : "default_linear_") + xChromaOffsetName + "_" + tilingName + (disjoint ? "_disjoint" : "") + "_swapped_chroma", "", createTestShaders, textureConversionTest, config);
								}

								if (!explicitReconstruction)
								{
									{
										const glu::ShaderType			shaderType		(rng.choose<glu::ShaderType>(DE_ARRAY_BEGIN(shaderTypes), DE_ARRAY_END(shaderTypes)));
										const vk::VkChromaLocation		yChromaOffset	(rng.choose<ChromaLocationNamePair, const ChromaLocationNamePair*>(DE_ARRAY_BEGIN(chromaLocations), DE_ARRAY_END(chromaLocations)).value);
										const TestConfig				config			(shaderType, format, tiling, textureFilter, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
																							vk::VK_FILTER_NEAREST, xChromaOffset, yChromaOffset, explicitReconstruction, disjoint,
																							defaultColorRange, defaultColorModel, identitySwizzle);

										addFunctionCaseWithPrograms(textureFilterGroup.get(), string("default_nearest_") + xChromaOffsetName + "_" + tilingName + (disjoint ? "_disjoint" : ""), "", createTestShaders, textureConversionTest, config);
									}

									{
										const glu::ShaderType			shaderType		(rng.choose<glu::ShaderType>(DE_ARRAY_BEGIN(shaderTypes), DE_ARRAY_END(shaderTypes)));
										const vk::VkChromaLocation		yChromaOffset	(rng.choose<ChromaLocationNamePair, const ChromaLocationNamePair*>(DE_ARRAY_BEGIN(chromaLocations), DE_ARRAY_END(chromaLocations)).value);
										const TestConfig				config			(shaderType, format, tiling, textureFilter, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
																							vk::VK_FILTER_NEAREST, xChromaOffset, yChromaOffset, explicitReconstruction, disjoint,
																							defaultColorRange, defaultColorModel, swappedChromaSwizzle);

										addFunctionCaseWithPrograms(textureFilterGroup.get(), string("default_nearest_") + xChromaOffsetName + "_" + tilingName + (disjoint ? "_disjoint" : "") + "_swapped_chroma", "", createTestShaders, textureConversionTest, config);
									}
								}
							}
						}

						if (explicitReconstruction)
						{
							for (size_t tilingNdx = 0; tilingNdx < DE_LENGTH_OF_ARRAY(imageTilings); tilingNdx++)
							{
								const vk::VkImageTiling	tiling		(imageTilings[tilingNdx].value);
								const char* const		tilingName	(imageTilings[tilingNdx].name);
								{
									const glu::ShaderType			shaderType		(rng.choose<glu::ShaderType>(DE_ARRAY_BEGIN(shaderTypes), DE_ARRAY_END(shaderTypes)));
									const vk::VkChromaLocation		chromaLocation	(rng.choose<ChromaLocationNamePair, const ChromaLocationNamePair*>(DE_ARRAY_BEGIN(chromaLocations), DE_ARRAY_END(chromaLocations)).value);
									const TestConfig				config			(shaderType, format, tiling, textureFilter, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
																						vk::VK_FILTER_NEAREST, chromaLocation, chromaLocation, explicitReconstruction, disjoint,
																						defaultColorRange, defaultColorModel, identitySwizzle);

									addFunctionCaseWithPrograms(textureFilterGroup.get(), string("explicit_nearest") + "_" + tilingName + (disjoint ? "_disjoint" : ""), "", createTestShaders, textureConversionTest, config);
								}

								{
									const glu::ShaderType			shaderType		(rng.choose<glu::ShaderType>(DE_ARRAY_BEGIN(shaderTypes), DE_ARRAY_END(shaderTypes)));
									const vk::VkChromaLocation		chromaLocation	(rng.choose<ChromaLocationNamePair, const ChromaLocationNamePair*>(DE_ARRAY_BEGIN(chromaLocations), DE_ARRAY_END(chromaLocations)).value);
									const TestConfig				config			(shaderType, format, tiling, textureFilter, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
																						vk::VK_FILTER_NEAREST, chromaLocation, chromaLocation, explicitReconstruction, disjoint,
																						defaultColorRange, defaultColorModel, swappedChromaSwizzle);

									addFunctionCaseWithPrograms(textureFilterGroup.get(), string("explicit_nearest") + "_" + tilingName + (disjoint ? "_disjoint" : "") + "_swapped_chroma", "", createTestShaders, textureConversionTest, config);
								}
							}
						}
					}
				}

				reconstrucGroup->addChild(textureFilterGroup.release());
			}

			formatGroup->addChild(reconstrucGroup.release());
		}

		testGroup->addChild(formatGroup.release());
	}

	// Test formats with xy chroma reconstruction
	for (size_t formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(xyChromaSubsampledFormats); formatNdx++)
	{
		const vk::VkFormat				format		(xyChromaSubsampledFormats[formatNdx]);
		const std::string				formatName	(de::toLower(std::string(getFormatName(format)).substr(10)));
		de::MovePtr<tcu::TestCaseGroup>	formatGroup	(new tcu::TestCaseGroup(testCtx, formatName.c_str(), ("Tests for color conversion using format " + formatName).c_str()));

		// Color conversion tests
		{
			de::MovePtr<tcu::TestCaseGroup>	conversionGroup	(new tcu::TestCaseGroup(testCtx, "color_conversion", ""));

			for (size_t chromaOffsetNdx = 0; chromaOffsetNdx < DE_LENGTH_OF_ARRAY(chromaLocations); chromaOffsetNdx++)
			{
				const char* const			chromaOffsetName	(chromaLocations[chromaOffsetNdx].name);
				const vk::VkChromaLocation	chromaOffset		(chromaLocations[chromaOffsetNdx].value);

				for (size_t modelNdx = 0; modelNdx < DE_LENGTH_OF_ARRAY(colorModels); modelNdx++)
				{
					const char* const							colorModelName	(colorModels[modelNdx].name);
					const vk::VkSamplerYcbcrModelConversion		colorModel		(colorModels[modelNdx].value);

					if (colorModel != vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY && getYCbCrFormatChannelCount(format) < 3)
						continue;

					if (colorModel == vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY)
					{
						for (size_t tilingNdx = 0; tilingNdx < DE_LENGTH_OF_ARRAY(imageTilings); tilingNdx++)
						{
							const vk::VkImageTiling				tiling			(imageTilings[tilingNdx].value);
							const char* const					tilingName		(imageTilings[tilingNdx].name);
							const vk::VkSamplerYcbcrRange		colorRange		(rng.choose<RangeNamePair, const RangeNamePair*>(DE_ARRAY_BEGIN(colorRanges), DE_ARRAY_END(colorRanges)).value);
							const glu::ShaderType				shaderType		(rng.choose<glu::ShaderType>(DE_ARRAY_BEGIN(shaderTypes), DE_ARRAY_END(shaderTypes)));
							const TestConfig					config			(shaderType, format, tiling, vk::VK_FILTER_NEAREST, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
																				 vk::VK_FILTER_NEAREST, chromaOffset, chromaOffset, false, false,
																				 colorRange, colorModel, identitySwizzle);

							addFunctionCaseWithPrograms(conversionGroup.get(), std::string(colorModelName) + "_" + tilingName + "_" + chromaOffsetName, "", createTestShaders, textureConversionTest, config);
						}
					}
					else
					{
						for (size_t rangeNdx = 0; rangeNdx < DE_LENGTH_OF_ARRAY(colorRanges); rangeNdx++)
						{
							const char* const					colorRangeName	(colorRanges[rangeNdx].name);
							const vk::VkSamplerYcbcrRange		colorRange		(colorRanges[rangeNdx].value);

							// Narrow range doesn't really work with formats that have less than 8 bits
							if (colorRange == vk::VK_SAMPLER_YCBCR_RANGE_ITU_NARROW)
							{
								const UVec4	bitDepth	(getYCbCrBitDepth(format));

								if (bitDepth[0] < 8 || bitDepth[1] < 8 || bitDepth[2] < 8)
									continue;
							}

							for (size_t tilingNdx = 0; tilingNdx < DE_LENGTH_OF_ARRAY(imageTilings); tilingNdx++)
							{
								const vk::VkImageTiling			tiling			(imageTilings[tilingNdx].value);
								const char* const				tilingName		(imageTilings[tilingNdx].name);
								const glu::ShaderType			shaderType		(rng.choose<glu::ShaderType>(DE_ARRAY_BEGIN(shaderTypes), DE_ARRAY_END(shaderTypes)));
								const TestConfig				config			(shaderType, format, tiling, vk::VK_FILTER_NEAREST, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
																					vk::VK_FILTER_NEAREST, chromaOffset, chromaOffset, false, false,
																					colorRange, colorModel, identitySwizzle);

								addFunctionCaseWithPrograms(conversionGroup.get(), (string(colorModelName) + "_" + colorRangeName + "_" + tilingName + "_" + chromaOffsetName).c_str(), "", createTestShaders, textureConversionTest, config);
							}
						}
					}
				}
			}

			formatGroup->addChild(conversionGroup.release());
		}

		// Chroma reconstruction tests
		{
			de::MovePtr<tcu::TestCaseGroup>	reconstrucGroup	(new tcu::TestCaseGroup(testCtx, "chroma_reconstruction", ""));

			for (size_t textureFilterNdx = 0; textureFilterNdx < DE_LENGTH_OF_ARRAY(textureFilters); textureFilterNdx++)
			{
				const char* const				textureFilterName	(textureFilters[textureFilterNdx].name);
				const vk::VkFilter				textureFilter		(textureFilters[textureFilterNdx].value);
				de::MovePtr<tcu::TestCaseGroup>	textureFilterGroup	(new tcu::TestCaseGroup(testCtx, textureFilterName, textureFilterName));

				for (size_t explicitReconstructionNdx = 0; explicitReconstructionNdx < 2; explicitReconstructionNdx++)
				{
					const bool	explicitReconstruction	(explicitReconstructionNdx == 1);

					for (size_t disjointNdx = 0; disjointNdx < 2; disjointNdx++)
					{
						const bool	disjoint	(disjointNdx == 1);

						for (size_t xChromaOffsetNdx = 0; xChromaOffsetNdx < DE_LENGTH_OF_ARRAY(chromaLocations); xChromaOffsetNdx++)
						for (size_t yChromaOffsetNdx = 0; yChromaOffsetNdx < DE_LENGTH_OF_ARRAY(chromaLocations); yChromaOffsetNdx++)
						{
							const vk::VkChromaLocation		xChromaOffset		(chromaLocations[xChromaOffsetNdx].value);
							const char* const				xChromaOffsetName	(chromaLocations[xChromaOffsetNdx].name);

							const vk::VkChromaLocation		yChromaOffset		(chromaLocations[yChromaOffsetNdx].value);
							const char* const				yChromaOffsetName	(chromaLocations[yChromaOffsetNdx].name);

							for (size_t tilingNdx = 0; tilingNdx < DE_LENGTH_OF_ARRAY(imageTilings); tilingNdx++)
							{
								const vk::VkImageTiling	tiling		(imageTilings[tilingNdx].value);
								const char* const		tilingName	(imageTilings[tilingNdx].name);
								{
									const glu::ShaderType	shaderType	(rng.choose<glu::ShaderType>(DE_ARRAY_BEGIN(shaderTypes), DE_ARRAY_END(shaderTypes)));
									const TestConfig		config		(shaderType, format, tiling, textureFilter, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
																			vk::VK_FILTER_LINEAR, xChromaOffset, yChromaOffset, explicitReconstruction, disjoint,
																			defaultColorRange, defaultColorModel, identitySwizzle);

									addFunctionCaseWithPrograms(textureFilterGroup.get(), string(explicitReconstruction ? "explicit_linear_" : "default_linear_") + xChromaOffsetName + "_" + yChromaOffsetName + "_" + tilingName + (disjoint ? "_disjoint" : ""), "", createTestShaders, textureConversionTest, config);
								}

								{
									const glu::ShaderType	shaderType	(rng.choose<glu::ShaderType>(DE_ARRAY_BEGIN(shaderTypes), DE_ARRAY_END(shaderTypes)));
									const TestConfig		config		(shaderType, format, tiling, textureFilter, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
																			vk::VK_FILTER_LINEAR, xChromaOffset, yChromaOffset, explicitReconstruction, disjoint,
																			defaultColorRange, defaultColorModel, swappedChromaSwizzle);

									addFunctionCaseWithPrograms(textureFilterGroup.get(), string(explicitReconstruction ? "explicit_linear_" : "default_linear_") + xChromaOffsetName + "_" + yChromaOffsetName + "_" + tilingName + (disjoint ? "_disjoint" : "") + "_swapped_chroma", "", createTestShaders, textureConversionTest, config);
								}

								if (!explicitReconstruction)
								{
									{
										const glu::ShaderType	shaderType	(rng.choose<glu::ShaderType>(DE_ARRAY_BEGIN(shaderTypes), DE_ARRAY_END(shaderTypes)));
										const TestConfig		config		(shaderType, format, tiling, textureFilter, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
																				vk::VK_FILTER_NEAREST, xChromaOffset, yChromaOffset, explicitReconstruction, disjoint,
																				defaultColorRange, defaultColorModel, identitySwizzle);

										addFunctionCaseWithPrograms(textureFilterGroup.get(), string("default_nearest_") + xChromaOffsetName + "_" + yChromaOffsetName + "_" + tilingName + (disjoint ? "_disjoint" : ""), "", createTestShaders, textureConversionTest, config);
									}

									{
										const glu::ShaderType	shaderType	(rng.choose<glu::ShaderType>(DE_ARRAY_BEGIN(shaderTypes), DE_ARRAY_END(shaderTypes)));
										const TestConfig		config		(shaderType, format, tiling, textureFilter, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
																				vk::VK_FILTER_NEAREST, xChromaOffset, yChromaOffset, explicitReconstruction, disjoint,
																				defaultColorRange, defaultColorModel, swappedChromaSwizzle);

										addFunctionCaseWithPrograms(textureFilterGroup.get(), string("default_nearest_") + xChromaOffsetName + "_" + yChromaOffsetName + "_" + tilingName + (disjoint ? "_disjoint" : "") + "_swapped_chroma", "", createTestShaders, textureConversionTest, config);
									}
								}
							}
						}

						if (explicitReconstruction)
						{
							for (size_t tilingNdx = 0; tilingNdx < DE_LENGTH_OF_ARRAY(imageTilings); tilingNdx++)
							{
								const vk::VkImageTiling	tiling		(imageTilings[tilingNdx].value);
								const char* const		tilingName	(imageTilings[tilingNdx].name);
								{
									const glu::ShaderType			shaderType		(rng.choose<glu::ShaderType>(DE_ARRAY_BEGIN(shaderTypes), DE_ARRAY_END(shaderTypes)));
									const vk::VkChromaLocation		chromaLocation	(rng.choose<ChromaLocationNamePair, const ChromaLocationNamePair*>(DE_ARRAY_BEGIN(chromaLocations), DE_ARRAY_END(chromaLocations)).value);
									const TestConfig				config			(shaderType, format, tiling, textureFilter, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
																						vk::VK_FILTER_NEAREST, chromaLocation, chromaLocation, explicitReconstruction, disjoint,
																						defaultColorRange, defaultColorModel, identitySwizzle);

									addFunctionCaseWithPrograms(textureFilterGroup.get(), string("explicit_nearest") + "_" + tilingName + (disjoint ? "_disjoint" : ""), "", createTestShaders, textureConversionTest, config);
								}

								{
									const glu::ShaderType			shaderType		(rng.choose<glu::ShaderType>(DE_ARRAY_BEGIN(shaderTypes), DE_ARRAY_END(shaderTypes)));
									const vk::VkChromaLocation		chromaLocation	(rng.choose<ChromaLocationNamePair, const ChromaLocationNamePair*>(DE_ARRAY_BEGIN(chromaLocations), DE_ARRAY_END(chromaLocations)).value);
									const TestConfig				config			(shaderType, format, tiling, textureFilter, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
																						vk::VK_FILTER_NEAREST, chromaLocation, chromaLocation, explicitReconstruction, disjoint,
																						defaultColorRange, defaultColorModel, swappedChromaSwizzle);

									addFunctionCaseWithPrograms(textureFilterGroup.get(), string("explicit_nearest") + "_" + tilingName + (disjoint ? "_disjoint" : "") + "_swapped_chroma", "", createTestShaders, textureConversionTest, config);
								}
							}
						}
					}
				}

				reconstrucGroup->addChild(textureFilterGroup.release());
			}

			formatGroup->addChild(reconstrucGroup.release());
		}

		testGroup->addChild(formatGroup.release());
	}
}

} // anonymous

tcu::TestCaseGroup* createConversionTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "conversion", "Sampler YCbCr Conversion Tests", initTests);
}

} // ycbcr
} // vkt
