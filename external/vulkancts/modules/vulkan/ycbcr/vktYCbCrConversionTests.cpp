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

template<typename T>
inline de::SharedPtr<vk::Unique<T> > makeSharedPtr(vk::Move<T> move)
{
	return de::SharedPtr<vk::Unique<T> >(new vk::Unique<T>(move));
}

ShaderSpec createShaderSpec (deUint32 samplerBinding, const std::vector<vk::VkSamplerYcbcrModelConversion>&	colorModels)
{
	ShaderSpec spec;

	spec.inputs.push_back(Symbol("uv", glu::VarType(glu::TYPE_FLOAT_VEC2, glu::PRECISION_HIGHP)));
	// shader with single sampler
	if (colorModels.size()==1)
	{
		spec.globalDeclarations	= "layout(set=" + de::toString((int)EXTRA_RESOURCES_DESCRIPTOR_SET_INDEX) + ", binding=" + de::toString(samplerBinding) + ") uniform highp sampler2D u_sampler;";

		spec.outputs.push_back(Symbol("o_color", glu::VarType(glu::TYPE_FLOAT_VEC4, glu::PRECISION_HIGHP)));

		spec.source				= "o_color = texture(u_sampler, uv);\n";
	}
	else // shader with array of samplers
	{
		spec.globalDeclarations	= "layout(set=" + de::toString((int)EXTRA_RESOURCES_DESCRIPTOR_SET_INDEX) + ", binding=" + de::toString(samplerBinding) + ") uniform highp sampler2D u_sampler[" + de::toString(colorModels.size()) +  "];";

		for (int i = 0; i < (int)colorModels.size(); i++)
		{
			spec.outputs.push_back(Symbol(string("o_color") + de::toString(i), glu::VarType(glu::TYPE_FLOAT_VEC4, glu::PRECISION_HIGHP)));

			spec.source += string("o_color") + de::toString(i) + " = texture(u_sampler[" + de::toString(i) + "], uv);\n";
		}
	}
	return spec;
}

void genTexCoords (std::vector<Vec2>&	coords,
				   const UVec2&			srcSize,
				   const UVec2&			dstSize)
{
	for (deUint32 y = 0; y < dstSize.y(); y++)
	for (deUint32 x = 0; x < dstSize.x(); x++)
	{
		const float	fx	= (float)x;
		const float	fy	= (float)y;

		const float	fw	= (float)srcSize.x();
		const float	fh	= (float)srcSize.y();

		const float	s	= 1.5f * ((fx * 1.5f * fw + fx) / (1.5f * fw * 1.5f * fw)) - 0.25f;
		const float	t	= 1.5f * ((fy * 1.5f * fh + fy) / (1.5f * fh * 1.5f * fh)) - 0.25f;

		coords.push_back(Vec2(s, t));
	}
}

void genOneToOneTexCoords (std::vector<Vec2>&	coords,
						   const UVec2&			size)
{
	for (deUint32 y = 0; y < size.y(); y++)
	for (deUint32 x = 0; x < size.x(); x++)
	{
		const float s = ((float)x + 0.5f) / (float)size.x();
		const float t = ((float)y + 0.5f) / (float)size.y();

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
				 vk::VkComponentMapping					componentMapping_,
				 const UVec2							srcSize_,
				 const UVec2							dstSize_,
				 deUint32								samplerBinding_)
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
		, srcSize					(srcSize_)
		, dstSize					(dstSize_)
		, samplerBinding			(samplerBinding_)
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
	const UVec2								srcSize;
	const UVec2								dstSize;
	deUint32								samplerBinding;
};

vk::Move<vk::VkDescriptorSetLayout> createDescriptorSetLayout (const vk::DeviceInterface&											vkd,
															   vk::VkDevice															device,
															   const std::vector<de::SharedPtr<vk::Unique<vk::VkSampler> > >&		samplers,
															   deUint32																samplerBinding)
{
	std::vector<vk::VkSampler> sampler;
	for (size_t i = 0; i < samplers.size(); i++)
		sampler.push_back(samplers[i]->get());
	const vk::VkDescriptorSetLayoutBinding		layoutBindings[] =
	{
		{
			samplerBinding,
			vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			(deUint32)sampler.size(),
			vk::VK_SHADER_STAGE_ALL,
			sampler.data()
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

vk::Move<vk::VkDescriptorPool> createDescriptorPool (const vk::DeviceInterface&											vkd,
													 vk::VkDevice														device,
													 const std::vector<de::SharedPtr<vk::Unique<vk::VkSampler> > >&		samplers,
													 const deUint32														combinedSamplerDescriptorCount)
{
	const vk::VkDescriptorPoolSize			poolSizes[]					=
	{
		{ vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, (deUint32)samplers.size() * combinedSamplerDescriptorCount }
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

vk::Move<vk::VkDescriptorSet> createDescriptorSet (const vk::DeviceInterface&											vkd,
												   vk::VkDevice															device,
												   vk::VkDescriptorPool													descriptorPool,
												   vk::VkDescriptorSetLayout											layout,
												   const std::vector<de::SharedPtr<vk::Unique<vk::VkSampler> > >&		samplers,
												   const std::vector<de::SharedPtr<vk::Unique<vk::VkImageView> > >&		imageViews,
												   deUint32																samplerBinding)
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
	std::vector<vk::VkDescriptorImageInfo>	imageInfo;
	for (size_t i = 0; i < samplers.size(); i++)
	{
		const vk::VkDescriptorImageInfo	ii =
		{
			samplers[i]->get(),
			imageViews[i]->get(),
			vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};
		imageInfo.push_back(ii);
	}

	{
		const vk::VkWriteDescriptorSet	writes[]	=
		{
			{
				vk::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				DE_NULL,

				*descriptorSet,
				samplerBinding,
				0u,
				(deUint32)imageInfo.size(),
				vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				imageInfo.data(),
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
		vk::VK_IMAGE_LAYOUT_PREINITIALIZED,
	};

	return vk::createImage(vkd, device, &createInfo);
}

vk::Move<vk::VkImageView> createImageView (const vk::DeviceInterface&		vkd,
										   vk::VkDevice						device,
										   vk::VkImage						image,
										   vk::VkFormat						format,
										   vk::VkSamplerYcbcrConversion		conversion)
{
	// Both mappings should be equivalent: alternate between the two for different formats.
	const vk::VkComponentMapping	mappingA	= { vk::VK_COMPONENT_SWIZZLE_IDENTITY, vk::VK_COMPONENT_SWIZZLE_IDENTITY, vk::VK_COMPONENT_SWIZZLE_IDENTITY, vk::VK_COMPONENT_SWIZZLE_IDENTITY };
	const vk::VkComponentMapping	mappingB	= { vk::VK_COMPONENT_SWIZZLE_R, vk::VK_COMPONENT_SWIZZLE_G, vk::VK_COMPONENT_SWIZZLE_B, vk::VK_COMPONENT_SWIZZLE_A };
	const vk::VkComponentMapping&	mapping		= ((static_cast<int>(format) % 2 == 0) ? mappingA : mappingB);

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
		mapping,
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

void evalShader (Context&												context,
				 glu::ShaderType										shaderType,
				 const MultiPlaneImageData&								imageData,
				 const UVec2&											size,
				 vk::VkFormat											format,
				 vk::VkImageTiling										imageTiling,
				 bool													disjoint,
				 vk::VkFilter											textureFilter,
				 vk::VkSamplerAddressMode								addressModeU,
				 vk::VkSamplerAddressMode								addressModeV,
				 const std::vector<vk::VkSamplerYcbcrModelConversion>&	colorModels,
				 vk::VkSamplerYcbcrRange								colorRange,
				 vk::VkChromaLocation									xChromaOffset,
				 vk::VkChromaLocation									yChromaOffset,
				 vk::VkFilter											chromaFilter,
				 const vk::VkComponentMapping&							componentMapping,
				 bool													explicitReconstruction,
				 const vector<Vec2>&									sts,
				 deUint32												samplerBinding,
				 vector<vector<Vec4> >&									results)
{
	const vk::InstanceInterface&											vk				(context.getInstanceInterface());
	const vk::DeviceInterface&												vkd				(context.getDeviceInterface());
	const vk::VkDevice														device			(context.getDevice());
	std::vector<de::SharedPtr<vk::Unique<vk::VkSamplerYcbcrConversion> > >	conversions;
	std::vector<de::SharedPtr<vk::Unique<vk::VkSampler> > >					samplers;
#if !defined(FAKE_COLOR_CONVERSION)
	for (int i = 0; i < (int)colorModels.size(); i++)
	{
		conversions.push_back(makeSharedPtr(createConversion(vkd, device, format, colorModels[i], colorRange, xChromaOffset, yChromaOffset, chromaFilter, componentMapping, explicitReconstruction)));
		samplers.push_back(makeSharedPtr(createSampler(vkd, device, textureFilter, addressModeU, addressModeV, conversions[i]->get())));
	}
#else
	DE_UNREF(colorRange);
	DE_UNREF(xChromaOffset);
	DE_UNREF(yChromaOffset);
	DE_UNREF(chromaFilter);
	DE_UNREF(explicitReconstruction);
	DE_UNREF(componentMapping);
	samplers.push_back(makeSharedPtr(createSampler(vkd, device, textureFilter, addressModeU, addressModeV, (vk::VkSamplerYcbcrConversion)0u)));
#endif
	const vk::Unique<vk::VkImage>								image				(createImage(vkd, device, format, size, disjoint, imageTiling));
	const vk::MemoryRequirement									memoryRequirement	(imageTiling == vk::VK_IMAGE_TILING_OPTIMAL
																					? vk::MemoryRequirement::Any
																					: vk::MemoryRequirement::HostVisible);
	const vk::VkImageCreateFlags								createFlags			(disjoint ? vk::VK_IMAGE_CREATE_DISJOINT_BIT : (vk::VkImageCreateFlagBits)0u);
	const vector<AllocationSp>									imageMemory			(allocateAndBindImageMemory(vkd, device, context.getDefaultAllocator(), *image, format, createFlags, memoryRequirement));
	std::vector<de::SharedPtr<vk::Unique<vk::VkImageView > > >	imageViews;
#if defined(FAKE_COLOR_CONVERSION)
	imageViews.push_back(makeSharedPtr(createImageView(vkd, device, *image, format, (vk::VkSamplerYcbcrConversion)0)));
#else
	for (int i = 0; i < (int)colorModels.size(); i++)
	{
		imageViews.push_back(makeSharedPtr(createImageView(vkd, device, *image, format, conversions[i]->get())));
	}
#endif

	deUint32 combinedSamplerDescriptorCount = 1;
	{
		const vk::VkPhysicalDeviceImageFormatInfo2 imageFormatInfo =
		{
			vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,	//VkStructureType		sType;
			DE_NULL,													//const void*			pNext;
			format,														//VkFormat				format;
			vk::VK_IMAGE_TYPE_2D,										//VkImageType			type;
			imageTiling,												//VkImageTiling			tiling;
			vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT |
			vk::VK_IMAGE_USAGE_SAMPLED_BIT,								//VkImageUsageFlags		usage;
			disjoint ?
			(vk::VkImageCreateFlags)vk::VK_IMAGE_CREATE_DISJOINT_BIT :
			(vk::VkImageCreateFlags)0u									//VkImageCreateFlags	flags;
		};

		vk::VkSamplerYcbcrConversionImageFormatProperties samplerYcbcrConversionImage = {};
		samplerYcbcrConversionImage.sType = vk::VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_IMAGE_FORMAT_PROPERTIES;
		samplerYcbcrConversionImage.pNext = DE_NULL;

		vk::VkImageFormatProperties2 imageFormatProperties = {};
		imageFormatProperties.sType = vk::VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
		imageFormatProperties.pNext = &samplerYcbcrConversionImage;

		VK_CHECK(vk.getPhysicalDeviceImageFormatProperties2(context.getPhysicalDevice(), &imageFormatInfo, &imageFormatProperties));
		combinedSamplerDescriptorCount = samplerYcbcrConversionImage.combinedImageSamplerDescriptorCount;
	}


	const vk::Unique<vk::VkDescriptorSetLayout>			layout				(createDescriptorSetLayout(vkd, device, samplers, samplerBinding));
	const vk::Unique<vk::VkDescriptorPool>				descriptorPool		(createDescriptorPool(vkd, device, samplers, combinedSamplerDescriptorCount));
	const vk::Unique<vk::VkDescriptorSet>				descriptorSet		(createDescriptorSet(vkd, device, *descriptorPool, *layout, samplers, imageViews, samplerBinding));

	const ShaderSpec									spec				(createShaderSpec(samplerBinding, colorModels));
	const de::UniquePtr<ShaderExecutor>					executor			(createExecutor(context, shaderType, spec, *layout));

	if (imageTiling == vk::VK_IMAGE_TILING_OPTIMAL)
		uploadImage(vkd, device, context.getUniversalQueueFamilyIndex(), context.getDefaultAllocator(), *image, imageData, vk::VK_ACCESS_SHADER_READ_BIT, vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	else
		fillImageMemory(vkd, device, context.getUniversalQueueFamilyIndex(), *image, imageMemory, imageData, vk::VK_ACCESS_SHADER_READ_BIT, vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	for(int i=0; i<(int)results.size(); i++)
		results[i].resize(sts.size());

	{
		const void* const	inputs[]	=
		{
			&sts[0]
		};
		vector<void*> outputs;
		for (int i = 0; i < (int)results.size(); i++)
			outputs.push_back((void*)results[i].data());

		executor->execute((int)sts.size(), inputs, outputs.data(), *descriptorSet);
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
	if( config.colorModel != vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_LAST )
		log << TestLog::Message << "ColorModel: " << config.colorModel << TestLog::EndMessage;
	else
		log << TestLog::Message << "ColorModel: array of samplers" << TestLog::EndMessage;
	log << TestLog::Message << "ComponentMapping: " << config.componentMapping << TestLog::EndMessage;
}

void checkSupport (Context& context, const TestConfig config)
{
#if !defined(FAKE_COLOR_CONVERSION)
	if (!context.isDeviceFunctionalitySupported("VK_KHR_sampler_ycbcr_conversion"))
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
	}
	catch (const vk::Error& err)
	{
		if (err.getError() == vk::VK_ERROR_FORMAT_NOT_SUPPORTED)
			TCU_THROW(NotSupportedError, "Format not supported");

		throw;
	}
#endif
}

tcu::TestStatus textureConversionTest (Context& context, const TestConfig config)
{
	const std::vector<FloatFormat>	filteringPrecision		(getPrecision(config.format));
	const std::vector<FloatFormat>	conversionPrecision		(getPrecision(config.format));
	const deUint32					subTexelPrecisionBits	(vk::getPhysicalDeviceProperties(context.getInstanceInterface(), context.getPhysicalDevice()).limits.subTexelPrecisionBits);
	const tcu::UVec4				bitDepth				(getYCbCrBitDepth(config.format));
	TestLog&						log						(context.getTestContext().getLog());
	bool							explicitReconstruction	= config.explicitReconstruction;
	const UVec2						srcSize					= config.srcSize;
	const UVec2						dstSize					= config.dstSize;
	bool							isOk					= true;

	logTestCaseInfo(log, config);

#if !defined(FAKE_COLOR_CONVERSION)
	try
	{
		const vk::VkFormatProperties	properties	(vk::getPhysicalDeviceFormatProperties(context.getInstanceInterface(), context.getPhysicalDevice(), config.format));
		const vk::VkFormatFeatureFlags	features	(config.imageTiling == vk::VK_IMAGE_TILING_OPTIMAL
													? properties.optimalTilingFeatures
													: properties.linearTilingFeatures);

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
		const vk::PlanarFormatDescription	planeInfo				(vk::getPlanarFormatDescription(config.format));
		MultiPlaneImageData					src						(config.format, srcSize);

		deUint32							nullAccessData			(0u);
		ChannelAccess						nullAccess				(tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT, 1u, IVec3(srcSize.x(), srcSize.y(), 1), IVec3(0, 0, 0), &nullAccessData, 0u);
		deUint32							nullAccessAlphaData		(~0u);
		ChannelAccess						nullAccessAlpha			(tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT, 1u, IVec3(srcSize.x(), srcSize.y(), 1), IVec3(0, 0, 0), &nullAccessAlphaData, 0u);
		ChannelAccess						rChannelAccess			(planeInfo.hasChannelNdx(0) ? getChannelAccess(src, planeInfo, srcSize, 0) : nullAccess);
		ChannelAccess						gChannelAccess			(planeInfo.hasChannelNdx(1) ? getChannelAccess(src, planeInfo, srcSize, 1) : nullAccess);
		ChannelAccess						bChannelAccess			(planeInfo.hasChannelNdx(2) ? getChannelAccess(src, planeInfo, srcSize, 2) : nullAccess);
		ChannelAccess						aChannelAccess			(planeInfo.hasChannelNdx(3) ? getChannelAccess(src, planeInfo, srcSize, 3) : nullAccessAlpha);
		const bool							implicitNearestCosited	((config.chromaFilter == vk::VK_FILTER_NEAREST && !config.explicitReconstruction) &&
																	 (config.xChromaOffset == vk::VK_CHROMA_LOCATION_COSITED_EVEN_KHR || config.yChromaOffset == vk::VK_CHROMA_LOCATION_COSITED_EVEN_KHR));

		vector<Vec2>						sts;
		vector<vector<Vec4> >				results;
		vector<vector<Vec4> >				minBounds;
		vector<vector<Vec4> >				minMidpointBounds;
		vector<vector<Vec4> >				maxBounds;
		vector<vector<Vec4> >				maxMidpointBounds;
		vector<vector<Vec4> >				uvBounds;
		vector<vector<IVec4> >				ijBounds;

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

		if (dstSize.x() > srcSize.x() && dstSize.y() > srcSize.y())
			genTexCoords(sts, srcSize, dstSize);
		else
			genOneToOneTexCoords(sts, dstSize);

		std::vector< vk::VkSamplerYcbcrModelConversion> colorModels;
		if (config.colorModel != vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_LAST)
		{
			colorModels.push_back(config.colorModel);
		}
		else
		{
			int ycbcrModelConverionCount = std::min( (int)vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_LAST, 4 );
			for (int i = 0; i < ycbcrModelConverionCount; i++)
			{
				colorModels.push_back((vk::VkSamplerYcbcrModelConversion)(vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY + i));
			}
		}

		for (int i = 0; i < (int)colorModels.size(); i++)
		{
			vector<Vec4>				minBound;
			vector<Vec4>				minMidpointBound;
			vector<Vec4>				maxBound;
			vector<Vec4>				maxMidpointBound;
			vector<Vec4>				uvBound;
			vector<IVec4>				ijBound;

			calculateBounds(rChannelAccess, gChannelAccess, bChannelAccess, aChannelAccess, bitDepth, sts, filteringPrecision, conversionPrecision, subTexelPrecisionBits, config.textureFilter, colorModels[i], config.colorRange, config.chromaFilter, config.xChromaOffset, config.yChromaOffset, config.componentMapping, explicitReconstruction, config.addressModeU, config.addressModeV, minBound, maxBound, uvBound, ijBound);

			if (implicitNearestCosited)
			{
				calculateBounds(rChannelAccess, gChannelAccess, bChannelAccess, aChannelAccess, bitDepth, sts, filteringPrecision, conversionPrecision, subTexelPrecisionBits, config.textureFilter, colorModels[i], config.colorRange, config.chromaFilter, vk::VK_CHROMA_LOCATION_MIDPOINT_KHR, vk::VK_CHROMA_LOCATION_MIDPOINT_KHR, config.componentMapping, explicitReconstruction, config.addressModeU, config.addressModeV, minMidpointBound, maxMidpointBound, uvBound, ijBound);
			}
			results.push_back			(vector<Vec4>());
			minBounds.push_back			(minBound);
			minMidpointBounds.push_back	(minMidpointBound);
			maxBounds.push_back			(maxBound);
			maxMidpointBounds.push_back	(maxMidpointBound);
			uvBounds.push_back			(uvBound);
			ijBounds.push_back			(ijBound);
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
			tcu::TextureLevel	srcImage	(vk::mapVkFormat(config.format), srcSize.x(), srcSize.y());

			for (int y = 0; y < (int)srcSize.y(); y++)
			for (int x = 0; x < (int)srcSize.x(); x++)
			{
				const IVec3 pos (x, y, 0);
				srcImage.getAccess().setPixel(Vec4(rChannelAccess.getChannel(pos), gChannelAccess.getChannel(pos), bChannelAccess.getChannel(pos), aChannelAccess.getChannel(pos)), x, y);
			}

			log << TestLog::Image("SourceImage", "SourceImage", srcImage.getAccess());
		}

		evalShader(context, config.shaderType, src, srcSize, config.format, config.imageTiling, config.disjoint, config.textureFilter, config.addressModeU, config.addressModeV, colorModels, config.colorRange, config.xChromaOffset, config.yChromaOffset, config.chromaFilter, config.componentMapping, config.explicitReconstruction, sts, config.samplerBinding, results);

		{
			std::vector<tcu::TextureLevel>	minImages;
			std::vector<tcu::TextureLevel>	maxImages;
			std::vector<tcu::TextureLevel>	minMidpointImages;
			std::vector<tcu::TextureLevel>	maxMidpointImages;
			std::vector<tcu::TextureLevel>	resImages;
			for (int i = 0; i < (int)colorModels.size(); i++)
			{
				minImages.push_back			(tcu::TextureLevel(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::FLOAT), dstSize.x(), dstSize.y()));
				maxImages.push_back			(tcu::TextureLevel(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::FLOAT), dstSize.x(), dstSize.y()));
				minMidpointImages.push_back	(tcu::TextureLevel(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::FLOAT), dstSize.x(), dstSize.y()));
				maxMidpointImages.push_back	(tcu::TextureLevel(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::FLOAT), dstSize.x(), dstSize.y()));
				resImages.push_back			(tcu::TextureLevel(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::FLOAT), dstSize.x(), dstSize.y()));
			}

			for (int i = 0; i < (int)colorModels.size(); i++)
			for (int y = 0; y < (int)(dstSize.y()); y++)
			for (int x = 0; x < (int)(dstSize.x()); x++)
			{
				const int ndx = x + y * (int)(dstSize.x());
				minImages[i].getAccess().setPixel(minBounds[i][ndx], x, y);
				maxImages[i].getAccess().setPixel(maxBounds[i][ndx], x, y);
			}

			for (int i = 0; i < (int)colorModels.size(); i++)
			for (int y = 0; y < (int)(dstSize.y()); y++)
			for (int x = 0; x < (int)(dstSize.x()); x++)
			{
				const int ndx = x + y * (int)(dstSize.x());
				resImages[i].getAccess().setPixel(results[i][ndx], x, y);
			}

			if (implicitNearestCosited)
			{
				for (int i = 0; i < (int)colorModels.size(); i++)
				for (int y = 0; y < (int)(dstSize.y()); y++)
				for (int x = 0; x < (int)(dstSize.x()); x++)
				{
					const int ndx = x + y * (int)(dstSize.x());
					minMidpointImages[i].getAccess().setPixel(minMidpointBounds[i][ndx], x, y);
					maxMidpointImages[i].getAccess().setPixel(maxMidpointBounds[i][ndx], x, y);
				}
			}

			for (int i = 0; i < (int)colorModels.size(); i++)
			{
				const Vec4	scale	(1.0f);
				const Vec4	bias	(0.0f);

				log << TestLog::Image(string("MinBoundImage_") + de::toString(i), string("MinBoundImage_") + de::toString(i), minImages[i].getAccess(), scale, bias);
				log << TestLog::Image(string("MaxBoundImage_") + de::toString(i), string("MaxBoundImage_") + de::toString(i), maxImages[i].getAccess(), scale, bias);

				if (implicitNearestCosited)
				{
					log << TestLog::Image(string("MinMidpointBoundImage_") + de::toString(i), string("MinMidpointBoundImage_") + de::toString(i), minMidpointImages[i].getAccess(), scale, bias);
					log << TestLog::Image(string("MaxMidpointBoundImage_") + de::toString(i), string("MaxMidpointBoundImage_") + de::toString(i), maxMidpointImages[i].getAccess(), scale, bias);
				}

				log << TestLog::Image(string("ResultImage_") + de::toString(i), string("ResultImage_") + de::toString(i), resImages[i].getAccess(), scale, bias);
			}
		}

		size_t errorCount = 0;

		for (int i = 0; i < (int)colorModels.size(); i++)
		for (size_t ndx = 0; ndx < sts.size(); ndx++)
		{
			bool fail;
			if (implicitNearestCosited)
			{
				fail = (tcu::boolAny(tcu::lessThan(results[i][ndx], minMidpointBounds[i][ndx])) || tcu::boolAny(tcu::greaterThan(results[i][ndx], maxMidpointBounds[i][ndx]))) &&
						(tcu::boolAny(tcu::lessThan(results[i][ndx], minBounds[i][ndx])) || tcu::boolAny(tcu::greaterThan(results[i][ndx], maxBounds[i][ndx])));
			}
			else
			{
				fail = tcu::boolAny(tcu::lessThan(results[i][ndx], minBounds[i][ndx])) || tcu::boolAny(tcu::greaterThan(results[i][ndx], maxBounds[i][ndx]));
			}

			if (fail)
			{
				log << TestLog::Message << "Fail: " << i << " " << sts[ndx] << " " << results[i][ndx] << TestLog::EndMessage;
				log << TestLog::Message << "  Min : " << minBounds[i][ndx] << TestLog::EndMessage;
				log << TestLog::Message << "  Max : " << maxBounds[i][ndx] << TestLog::EndMessage;
				log << TestLog::Message << "  Threshold: " << (maxBounds[i][ndx] - minBounds[i][ndx]) << TestLog::EndMessage;
				log << TestLog::Message << "  UMin : " << uvBounds[i][ndx][0] << TestLog::EndMessage;
				log << TestLog::Message << "  UMax : " << uvBounds[i][ndx][1] << TestLog::EndMessage;
				log << TestLog::Message << "  VMin : " << uvBounds[i][ndx][2] << TestLog::EndMessage;
				log << TestLog::Message << "  VMax : " << uvBounds[i][ndx][3] << TestLog::EndMessage;
				log << TestLog::Message << "  IMin : " << ijBounds[i][ndx][0] << TestLog::EndMessage;
				log << TestLog::Message << "  IMax : " << ijBounds[i][ndx][1] << TestLog::EndMessage;
				log << TestLog::Message << "  JMin : " << ijBounds[i][ndx][2] << TestLog::EndMessage;
				log << TestLog::Message << "  JMax : " << ijBounds[i][ndx][3] << TestLog::EndMessage;

				if (isXChromaSubsampled(config.format))
				{
					log << TestLog::Message << "  LumaAlphaValues : " << TestLog::EndMessage;
					log << TestLog::Message << "    Offset : (" << ijBounds[i][ndx][0] << ", " << ijBounds[i][ndx][2] << ")" << TestLog::EndMessage;

					for (deInt32 k = ijBounds[i][ndx][2]; k <= ijBounds[i][ndx][3] + (config.textureFilter == vk::VK_FILTER_LINEAR ? 1 : 0); k++)
					{
						const deInt32		wrappedK	= wrap(config.addressModeV, k, gChannelAccess.getSize().y());
						bool				first		= true;
						std::ostringstream	line;

						for (deInt32 j = ijBounds[i][ndx][0]; j <= ijBounds[i][ndx][1] + (config.textureFilter == vk::VK_FILTER_LINEAR ? 1 : 0); j++)
						{
							const deInt32	wrappedJ	= wrap(config.addressModeU, j, gChannelAccess.getSize().x());

							if (!first)
							{
								line << ", ";
								first = false;
							}

							line << "(" << std::setfill(' ') << std::setw(5) << gChannelAccess.getChannelUint(IVec3(wrappedJ, wrappedK, 0))
								<< ", " << std::setfill(' ') << std::setw(5) << aChannelAccess.getChannelUint(IVec3(wrappedJ, wrappedK, 0)) << ")";
						}
						log << TestLog::Message << "    " << line.str() << TestLog::EndMessage;
					}

					{
						const IVec2 chromaJRange	(divFloor(ijBounds[i][ndx][0], 2) - 1, divFloor(ijBounds[i][ndx][1] + (config.textureFilter == vk::VK_FILTER_LINEAR ? 1 : 0), 2) + 1);
						const IVec2 chromaKRange	(isYChromaSubsampled(config.format)
													? IVec2(divFloor(ijBounds[i][ndx][2], 2) - 1, divFloor(ijBounds[i][ndx][3] + (config.textureFilter == vk::VK_FILTER_LINEAR ? 1 : 0), 2) + 1)
													: IVec2(ijBounds[i][ndx][2], ijBounds[i][ndx][3] + (config.textureFilter == vk::VK_FILTER_LINEAR ? 1 : 0)));

						log << TestLog::Message << "  ChromaValues : " << TestLog::EndMessage;
						log << TestLog::Message << "    Offset : (" << chromaJRange[0] << ", " << chromaKRange[0] << ")" << TestLog::EndMessage;

						for (deInt32 k = chromaKRange[0]; k <= chromaKRange[1]; k++)
						{
							const deInt32		wrappedK	= wrap(config.addressModeV, k, rChannelAccess.getSize().y());
							bool				first		= true;
							std::ostringstream	line;

							for (deInt32 j = chromaJRange[0]; j <= chromaJRange[1]; j++)
							{
								const deInt32	wrappedJ	= wrap(config.addressModeU, j, rChannelAccess.getSize().x());

								if (!first)
								{
									line << ", ";
									first = false;
								}

								line << "(" << std::setfill(' ') << std::setw(5) << rChannelAccess.getChannelUint(IVec3(wrappedJ, wrappedK, 0))
									<< ", " << std::setfill(' ') << std::setw(5) << bChannelAccess.getChannelUint(IVec3(wrappedJ, wrappedK, 0)) << ")";
							}
							log << TestLog::Message << "    " << line.str() << TestLog::EndMessage;
						}
					}
				}
				else
				{
					log << TestLog::Message << "  Values : " << TestLog::EndMessage;
					log << TestLog::Message << "    Offset : (" << ijBounds[i][ndx][0] << ", " << ijBounds[i][ndx][2] << ")" << TestLog::EndMessage;

					for (deInt32 k = ijBounds[i][ndx][2]; k <= ijBounds[i][ndx][3] + (config.textureFilter == vk::VK_FILTER_LINEAR ? 1 : 0); k++)
					{
						const deInt32		wrappedK	= wrap(config.addressModeV, k, rChannelAccess.getSize().y());
						bool				first		= true;
						std::ostringstream	line;

						for (deInt32 j = ijBounds[i][ndx][0]; j <= ijBounds[i][ndx][1] + (config.textureFilter == vk::VK_FILTER_LINEAR ? 1 : 0); j++)
						{
							const deInt32	wrappedJ	= wrap(config.addressModeU, j, rChannelAccess.getSize().x());

							if (!first)
							{
								line << ", ";
								first = false;
							}

							line << "(" << std::setfill(' ') << std::setw(5) << rChannelAccess.getChannelUint(IVec3(wrappedJ, wrappedK, 0))
								<< ", " << std::setfill(' ') << std::setw(5) << gChannelAccess.getChannelUint(IVec3(wrappedJ, wrappedK, 0))
								<< ", " << std::setfill(' ') << std::setw(5) << bChannelAccess.getChannelUint(IVec3(wrappedJ, wrappedK, 0))
								<< ", " << std::setfill(' ') << std::setw(5) << aChannelAccess.getChannelUint(IVec3(wrappedJ, wrappedK, 0)) << ")";
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
	std::vector< vk::VkSamplerYcbcrModelConversion> colorModels;
	if (config.colorModel != vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_LAST)
	{
		colorModels.push_back(config.colorModel);
	}
	else
	{
		int ycbcrModelConverionCount = std::min((int)vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_LAST, 4);
		for (int i = 0; i < ycbcrModelConverionCount; i++)
		{
			colorModels.push_back((vk::VkSamplerYcbcrModelConversion)(vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY + i));
		}
	}
#if !defined(FAKE_COLOR_CONVERSION)
	const ShaderSpec spec (createShaderSpec(config.samplerBinding, colorModels));

	generateSources(config.shaderType, spec, dst);
#else
	const tcu::UVec4	bits	(getYCbCrBitDepth(config.format));
	ShaderSpec			spec;

	spec.globalDeclarations = "layout(set=" + de::toString((int)EXTRA_RESOURCES_DESCRIPTOR_SET_INDEX) + ", binding=" + de::toString(config.samplerBinding) + ") uniform highp sampler2D u_sampler;";

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

// Alternate between swizzle_identity and their equivalents. Both should work.
const vk::VkComponentMapping& getIdentitySwizzle (void)
{
	static bool alternate = false;
	static const vk::VkComponentMapping mappingA = { vk::VK_COMPONENT_SWIZZLE_IDENTITY, vk::VK_COMPONENT_SWIZZLE_IDENTITY, vk::VK_COMPONENT_SWIZZLE_IDENTITY, vk::VK_COMPONENT_SWIZZLE_IDENTITY };
	static const vk::VkComponentMapping mappingB = { vk::VK_COMPONENT_SWIZZLE_R, vk::VK_COMPONENT_SWIZZLE_G, vk::VK_COMPONENT_SWIZZLE_B, vk::VK_COMPONENT_SWIZZLE_A };

	const vk::VkComponentMapping& mapping = (alternate ? mappingB : mappingA);
	alternate = (!alternate);
	return mapping;
}

struct YCbCrConversionTestBuilder
{
	const std::vector<vk::VkFormat> noChromaSubsampledFormats =
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
		vk::VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM,
		vk::VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT,
		vk::VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT,
		vk::VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT,
		vk::VK_FORMAT_G16_B16R16_2PLANE_444_UNORM_EXT,
	};
	const std::vector<vk::VkFormat> xChromaSubsampledFormats =
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
	const std::vector<vk::VkFormat> xyChromaSubsampledFormats =
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
	struct ColorModelStruct
	{
		const char* const							name;
		const vk::VkSamplerYcbcrModelConversion	value;
	};
	const std::vector<ColorModelStruct> colorModels =
	{
		{ "rgb_identity",	vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY		},
		{ "ycbcr_identity",	vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_IDENTITY	},
		{ "ycbcr_709",		vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_709			},
		{ "ycbcr_601",		vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_601			},
		{ "ycbcr_2020",		vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_2020		}
	};
	const std::vector<RangeNamePair> colorRanges =
	{
		{ "itu_full",		vk::VK_SAMPLER_YCBCR_RANGE_ITU_FULL		},
		{ "itu_narrow",		vk::VK_SAMPLER_YCBCR_RANGE_ITU_NARROW	}
	};
	const std::vector<ChromaLocationNamePair> chromaLocations =
	{
		{ "cosited",		vk::VK_CHROMA_LOCATION_COSITED_EVEN	},
		{ "midpoint",		vk::VK_CHROMA_LOCATION_MIDPOINT		}
	};
	struct TextureFilterStruct
	{
		const char* const	name;
		vk::VkFilter		value;
	};
	const std::vector<TextureFilterStruct> textureFilters =
	{
		{ "linear",			vk::VK_FILTER_LINEAR	},
		{ "nearest",		vk::VK_FILTER_NEAREST	}
	};
	// Used by the chroma reconstruction tests
	const vk::VkSamplerYcbcrModelConversion		defaultColorModel		= vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY;
	const vk::VkSamplerYcbcrRange				defaultColorRange		= vk::VK_SAMPLER_YCBCR_RANGE_ITU_FULL;
	const vk::VkComponentMapping				swappedChromaSwizzle	=
	{
		vk::VK_COMPONENT_SWIZZLE_B,
		vk::VK_COMPONENT_SWIZZLE_IDENTITY,
		vk::VK_COMPONENT_SWIZZLE_R,
		vk::VK_COMPONENT_SWIZZLE_IDENTITY
	};
	const std::vector<glu::ShaderType> shaderTypes =
	{
		glu::SHADERTYPE_VERTEX,
		glu::SHADERTYPE_FRAGMENT,
		glu::SHADERTYPE_COMPUTE
	};
	struct ImageTilingStruct
	{
		const char*			name;
		vk::VkImageTiling	value;
	};
	const std::vector<ImageTilingStruct> imageTilings =
	{
		{ "tiling_linear",	vk::VK_IMAGE_TILING_LINEAR },
		{ "tiling_optimal",	vk::VK_IMAGE_TILING_OPTIMAL }
	};
	struct SamplerBindingStruct
	{
		const char*			name;
		deUint32			value;
	};
	const std::vector<SamplerBindingStruct> samplerBindings =
	{
		{ "binding_0",		0	},
		{ "binding_7",		7	},
		{ "binding_15",		15	},
		{ "binding_31",		31	}
	};

	void buildTests(tcu::TestCaseGroup* testGroup)
	{
		tcu::TestContext&							testCtx(testGroup->getTestContext());
		de::Random									rng(1978765638u);

		// Test formats without chroma reconstruction
		for (size_t formatNdx = 0; formatNdx < noChromaSubsampledFormats.size(); formatNdx++)
		{
			const vk::VkFormat						format(noChromaSubsampledFormats[formatNdx]);
			const std::string						formatName(de::toLower(std::string(getFormatName(format)).substr(10)));
			de::MovePtr<tcu::TestCaseGroup>			formatGroup(new tcu::TestCaseGroup(testCtx, formatName.c_str(), ("Tests for color conversion using format " + formatName).c_str()));
			const UVec2								srcSize(isXChromaSubsampled(format) ? 12 : 7,
				isYChromaSubsampled(format) ? 8 : 13);
			const UVec2								dstSize(srcSize.x() + srcSize.x() / 2,
				srcSize.y() + srcSize.y() / 2);

			for (size_t modelNdx = 0; modelNdx < colorModels.size(); modelNdx++)
			{
				const char* const						colorModelName(colorModels[modelNdx].name);
				const vk::VkSamplerYcbcrModelConversion	colorModel(colorModels[modelNdx].value);

				if (colorModel != vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY && getYCbCrFormatChannelCount(format) < 3)
					continue;

				de::MovePtr<tcu::TestCaseGroup>			colorModelGroup(new tcu::TestCaseGroup(testCtx, colorModelName, ("Tests for color model " + string(colorModelName)).c_str()));

				if (colorModel == vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY)
				{
					for (size_t textureFilterNdx = 0; textureFilterNdx < textureFilters.size(); textureFilterNdx++)
					{
						const char* const					textureFilterName(textureFilters[textureFilterNdx].name);
						const vk::VkFilter					textureFilter(textureFilters[textureFilterNdx].value);

						for (size_t tilingNdx = 0; tilingNdx < imageTilings.size(); tilingNdx++)
						{
							const vk::VkImageTiling			tiling(imageTilings[tilingNdx].value);
							const char* const				tilingName(imageTilings[tilingNdx].name);
							const glu::ShaderType			shaderType(rng.choose<glu::ShaderType>(begin(shaderTypes), end(shaderTypes)));
							const vk::VkSamplerYcbcrRange	colorRange(rng.choose<RangeNamePair>(begin(colorRanges), end(colorRanges)).value);
							const vk::VkChromaLocation		chromaLocation(rng.choose<ChromaLocationNamePair>(begin(chromaLocations), end(chromaLocations)).value);

							for (size_t bindingNdx = 0; bindingNdx < samplerBindings.size(); bindingNdx++)
							{
								const deUint32					samplerBinding(samplerBindings[bindingNdx].value);
								string							samplerBindingName((samplerBindings[bindingNdx].value != 0) ? string("_") + samplerBindings[bindingNdx].name : string());
								const TestConfig				config(shaderType, format, tiling, textureFilter, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
									textureFilter, chromaLocation, chromaLocation, false, false,
									colorRange, colorModel, getIdentitySwizzle(), srcSize, dstSize, samplerBinding);

								addFunctionCaseWithPrograms(colorModelGroup.get(), std::string(textureFilterName) + "_" + tilingName + samplerBindingName, "", checkSupport, createTestShaders, textureConversionTest, config);
							}
						}
					}
				}
				else
				{
					for (size_t rangeNdx = 0; rangeNdx < colorRanges.size(); rangeNdx++)
					{
						const char* const				colorRangeName(colorRanges[rangeNdx].name);
						const vk::VkSamplerYcbcrRange	colorRange(colorRanges[rangeNdx].value);

						// Narrow range doesn't really work with formats that have less than 8 bits
						if (colorRange == vk::VK_SAMPLER_YCBCR_RANGE_ITU_NARROW)
						{
							const UVec4					bitDepth(getYCbCrBitDepth(format));

							if (bitDepth[0] < 8 || bitDepth[1] < 8 || bitDepth[2] < 8)
								continue;
						}

						de::MovePtr<tcu::TestCaseGroup>		colorRangeGroup(new tcu::TestCaseGroup(testCtx, colorRangeName, ("Tests for color range " + string(colorRangeName)).c_str()));

						for (size_t textureFilterNdx = 0; textureFilterNdx < textureFilters.size(); textureFilterNdx++)
						{
							const char* const				textureFilterName(textureFilters[textureFilterNdx].name);
							const vk::VkFilter				textureFilter(textureFilters[textureFilterNdx].value);

							for (size_t tilingNdx = 0; tilingNdx < imageTilings.size(); tilingNdx++)
							{
								const vk::VkImageTiling		tiling(imageTilings[tilingNdx].value);
								const char* const			tilingName(imageTilings[tilingNdx].name);
								const glu::ShaderType		shaderType(rng.choose<glu::ShaderType>(shaderTypes.begin(), shaderTypes.end()));
								const vk::VkChromaLocation	chromaLocation(rng.choose<ChromaLocationNamePair>(chromaLocations.begin(), chromaLocations.end()).value);
								for (size_t bindingNdx = 0; bindingNdx < samplerBindings.size(); bindingNdx++)								{
									const deUint32				samplerBinding(samplerBindings[bindingNdx].value);
									string						samplerBindingName((samplerBindings[bindingNdx].value != 0) ? string("_") + samplerBindings[bindingNdx].name : string());
									const TestConfig			config(shaderType, format, tiling, textureFilter, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
										textureFilter, chromaLocation, chromaLocation, false, false,
										colorRange, colorModel, getIdentitySwizzle(), srcSize, dstSize, samplerBinding);

									addFunctionCaseWithPrograms(colorRangeGroup.get(), std::string(textureFilterName) + "_" + tilingName + samplerBindingName, "", checkSupport, createTestShaders, textureConversionTest, config);
								}
							}
						}

						colorModelGroup->addChild(colorRangeGroup.release());
					}
				}

				formatGroup->addChild(colorModelGroup.release());
			}

			// Color conversion tests for array of samplers ( noChromaSubsampledFormats )
			if (getYCbCrFormatChannelCount(format) >= 3)
				buildArrayOfSamplersTests(format, srcSize, dstSize, formatGroup, testCtx, rng);

			testGroup->addChild(formatGroup.release());
		}

		// Test formats with x chroma reconstruction
		for (size_t formatNdx = 0; formatNdx < xChromaSubsampledFormats.size(); formatNdx++)
		{
			const vk::VkFormat				format(xChromaSubsampledFormats[formatNdx]);
			const std::string				formatName(de::toLower(std::string(getFormatName(format)).substr(10)));
			de::MovePtr<tcu::TestCaseGroup>	formatGroup(new tcu::TestCaseGroup(testCtx, formatName.c_str(), ("Tests for color conversion using format " + formatName).c_str()));
			const UVec2						srcSize(isXChromaSubsampled(format) ? 12 : 7,
				isYChromaSubsampled(format) ? 8 : 13);
			const UVec2						dstSize(srcSize.x() + srcSize.x() / 2,
				srcSize.y() + srcSize.y() / 2);

			// Color conversion tests
			{
				de::MovePtr<tcu::TestCaseGroup>	conversionGroup(new tcu::TestCaseGroup(testCtx, "color_conversion", ""));

				for (size_t xChromaOffsetNdx = 0; xChromaOffsetNdx < chromaLocations.size(); xChromaOffsetNdx++)
				{
					const char* const			xChromaOffsetName(chromaLocations[xChromaOffsetNdx].name);
					const vk::VkChromaLocation	xChromaOffset(chromaLocations[xChromaOffsetNdx].value);

					for (size_t modelNdx = 0; modelNdx < colorModels.size(); modelNdx++)
					{
						const char* const						colorModelName(colorModels[modelNdx].name);
						const vk::VkSamplerYcbcrModelConversion	colorModel(colorModels[modelNdx].value);

						if (colorModel != vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY && getYCbCrFormatChannelCount(format) < 3)
							continue;


						if (colorModel == vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY)
						{
							for (size_t tilingNdx = 0; tilingNdx < imageTilings.size(); tilingNdx++)
							{
								const vk::VkImageTiling			tiling(imageTilings[tilingNdx].value);
								const char* const				tilingName(imageTilings[tilingNdx].name);
								const vk::VkSamplerYcbcrRange	colorRange(rng.choose<RangeNamePair>(begin(colorRanges), end(colorRanges)).value);
								const glu::ShaderType			shaderType(rng.choose<glu::ShaderType>(begin(shaderTypes), end(shaderTypes)));
								const vk::VkChromaLocation		yChromaOffset(rng.choose<ChromaLocationNamePair>(begin(chromaLocations), end(chromaLocations)).value);
								for (size_t bindingNdx = 0; bindingNdx < samplerBindings.size(); bindingNdx++)
								{
									const deUint32					samplerBinding(samplerBindings[bindingNdx].value);
									string							samplerBindingName((samplerBindings[bindingNdx].value != 0) ? string("_") + samplerBindings[bindingNdx].name : string());
									const TestConfig				config(shaderType, format, tiling, vk::VK_FILTER_NEAREST, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
										vk::VK_FILTER_NEAREST, xChromaOffset, yChromaOffset, false, false,
										colorRange, colorModel, getIdentitySwizzle(), srcSize, dstSize, samplerBinding);

									addFunctionCaseWithPrograms(conversionGroup.get(), string(colorModelName) + "_" + tilingName + "_" + xChromaOffsetName + samplerBindingName, "", checkSupport, createTestShaders, textureConversionTest, config);
								}
							}
						}
						else
						{
							for (size_t rangeNdx = 0; rangeNdx < colorRanges.size(); rangeNdx++)
							{
								const char* const				colorRangeName(colorRanges[rangeNdx].name);
								const vk::VkSamplerYcbcrRange	colorRange(colorRanges[rangeNdx].value);

								// Narrow range doesn't really work with formats that have less than 8 bits
								if (colorRange == vk::VK_SAMPLER_YCBCR_RANGE_ITU_NARROW)
								{
									const UVec4					bitDepth(getYCbCrBitDepth(format));

									if (bitDepth[0] < 8 || bitDepth[1] < 8 || bitDepth[2] < 8)
										continue;
								}

								for (size_t tilingNdx = 0; tilingNdx < imageTilings.size(); tilingNdx++)
								{
									const vk::VkImageTiling		tiling(imageTilings[tilingNdx].value);
									const char* const			tilingName(imageTilings[tilingNdx].name);
									const glu::ShaderType		shaderType(rng.choose<glu::ShaderType>(begin(shaderTypes), end(shaderTypes)));
									const vk::VkChromaLocation	yChromaOffset(rng.choose<ChromaLocationNamePair>(begin(chromaLocations), end(chromaLocations)).value);
									for (size_t bindingNdx = 0; bindingNdx < samplerBindings.size(); bindingNdx++)
									{
										const deUint32				samplerBinding(samplerBindings[bindingNdx].value);
										string						samplerBindingName((samplerBindings[bindingNdx].value != 0) ? string("_") + samplerBindings[bindingNdx].name : string());
										const TestConfig			config(shaderType, format, tiling, vk::VK_FILTER_NEAREST, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
											vk::VK_FILTER_NEAREST, xChromaOffset, yChromaOffset, false, false,
											colorRange, colorModel, getIdentitySwizzle(), srcSize, dstSize, samplerBinding);

										addFunctionCaseWithPrograms(conversionGroup.get(), string(colorModelName) + "_" + colorRangeName + "_" + tilingName + "_" + xChromaOffsetName + samplerBindingName, "", checkSupport, createTestShaders, textureConversionTest, config);
									}
								}
							}
						}
					}
				}

				formatGroup->addChild(conversionGroup.release());
			}

			// Color conversion tests for array of samplers ( xChromaSubsampledFormats )
			if (getYCbCrFormatChannelCount(format) >= 3)
				buildArrayOfSamplersTests(format, srcSize, dstSize, formatGroup, testCtx, rng);

			// Chroma reconstruction tests
			{
				de::MovePtr<tcu::TestCaseGroup>	reconstrucGroup(new tcu::TestCaseGroup(testCtx, "chroma_reconstruction", ""));

				for (size_t textureFilterNdx = 0; textureFilterNdx < textureFilters.size(); textureFilterNdx++)
				{
					const char* const				textureFilterName(textureFilters[textureFilterNdx].name);
					const vk::VkFilter				textureFilter(textureFilters[textureFilterNdx].value);
					de::MovePtr<tcu::TestCaseGroup>	textureFilterGroup(new tcu::TestCaseGroup(testCtx, textureFilterName, textureFilterName));

					for (size_t explicitReconstructionNdx = 0; explicitReconstructionNdx < 2; explicitReconstructionNdx++)
					{
						const bool	explicitReconstruction(explicitReconstructionNdx == 1);

						for (size_t disjointNdx = 0; disjointNdx < 2; disjointNdx++)
						{
							const bool	disjoint(disjointNdx == 1);

							for (size_t xChromaOffsetNdx = 0; xChromaOffsetNdx < chromaLocations.size(); xChromaOffsetNdx++)
							{
								const vk::VkChromaLocation		xChromaOffset(chromaLocations[xChromaOffsetNdx].value);
								const char* const				xChromaOffsetName(chromaLocations[xChromaOffsetNdx].name);

								for (size_t tilingNdx = 0; tilingNdx < imageTilings.size(); tilingNdx++)
								{
									const vk::VkImageTiling		tiling(imageTilings[tilingNdx].value);
									const char* const			tilingName(imageTilings[tilingNdx].name);

									{
										const glu::ShaderType			shaderType(rng.choose<glu::ShaderType>(begin(shaderTypes), end(shaderTypes)));
										const vk::VkChromaLocation		yChromaOffset(rng.choose<ChromaLocationNamePair>(begin(chromaLocations), end(chromaLocations)).value);
										const TestConfig				config(shaderType, format, tiling, textureFilter, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
											vk::VK_FILTER_LINEAR, xChromaOffset, yChromaOffset, explicitReconstruction, disjoint,
											defaultColorRange, defaultColorModel, getIdentitySwizzle(), srcSize, dstSize, 0);

										addFunctionCaseWithPrograms(textureFilterGroup.get(), string(explicitReconstruction ? "explicit_linear_" : "default_linear_") + xChromaOffsetName + "_" + tilingName + (disjoint ? "_disjoint" : ""), "", checkSupport, createTestShaders, textureConversionTest, config);
									}

									{
										const glu::ShaderType			shaderType(rng.choose<glu::ShaderType>(begin(shaderTypes), end(shaderTypes)));
										const vk::VkChromaLocation		yChromaOffset(rng.choose<ChromaLocationNamePair>(begin(chromaLocations), end(chromaLocations)).value);
										const TestConfig				config(shaderType, format, tiling, textureFilter, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
											vk::VK_FILTER_LINEAR, xChromaOffset, yChromaOffset, explicitReconstruction, disjoint,
											defaultColorRange, defaultColorModel, swappedChromaSwizzle, srcSize, dstSize, 0);

										addFunctionCaseWithPrograms(textureFilterGroup.get(), string(explicitReconstruction ? "explicit_linear_" : "default_linear_") + xChromaOffsetName + "_" + tilingName + (disjoint ? "_disjoint" : "") + "_swapped_chroma", "", checkSupport, createTestShaders, textureConversionTest, config);
									}

									if (!explicitReconstruction)
									{
										{
											const glu::ShaderType			shaderType(rng.choose<glu::ShaderType>(begin(shaderTypes), end(shaderTypes)));
											const vk::VkChromaLocation		yChromaOffset(rng.choose<ChromaLocationNamePair>(begin(chromaLocations), end(chromaLocations)).value);
											const TestConfig				config(shaderType, format, tiling, textureFilter, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
												vk::VK_FILTER_NEAREST, xChromaOffset, yChromaOffset, explicitReconstruction, disjoint,
												defaultColorRange, defaultColorModel, getIdentitySwizzle(), srcSize, dstSize, 0);

											addFunctionCaseWithPrograms(textureFilterGroup.get(), string("default_nearest_") + xChromaOffsetName + "_" + tilingName + (disjoint ? "_disjoint" : ""), "", checkSupport, createTestShaders, textureConversionTest, config);
										}

										{
											const glu::ShaderType			shaderType(rng.choose<glu::ShaderType>(begin(shaderTypes), end(shaderTypes)));
											const vk::VkChromaLocation		yChromaOffset(rng.choose<ChromaLocationNamePair>(begin(chromaLocations), end(chromaLocations)).value);
											const TestConfig				config(shaderType, format, tiling, textureFilter, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
												vk::VK_FILTER_NEAREST, xChromaOffset, yChromaOffset, explicitReconstruction, disjoint,
												defaultColorRange, defaultColorModel, swappedChromaSwizzle, srcSize, dstSize, 0);

											addFunctionCaseWithPrograms(textureFilterGroup.get(), string("default_nearest_") + xChromaOffsetName + "_" + tilingName + (disjoint ? "_disjoint" : "") + "_swapped_chroma", "", checkSupport, createTestShaders, textureConversionTest, config);
										}
									}
								}
							}

							if (explicitReconstruction)
							{
								for (size_t tilingNdx = 0; tilingNdx < imageTilings.size(); tilingNdx++)
								{
									const vk::VkImageTiling	tiling(imageTilings[tilingNdx].value);
									const char* const		tilingName(imageTilings[tilingNdx].name);
									{
										const glu::ShaderType			shaderType(rng.choose<glu::ShaderType>(begin(shaderTypes), end(shaderTypes)));
										const vk::VkChromaLocation		chromaLocation(rng.choose<ChromaLocationNamePair>(begin(chromaLocations), end(chromaLocations)).value);
										const TestConfig				config(shaderType, format, tiling, textureFilter, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
											vk::VK_FILTER_NEAREST, chromaLocation, chromaLocation, explicitReconstruction, disjoint,
											defaultColorRange, defaultColorModel, getIdentitySwizzle(), srcSize, dstSize, 0);

										addFunctionCaseWithPrograms(textureFilterGroup.get(), string("explicit_nearest") + "_" + tilingName + (disjoint ? "_disjoint" : ""), "", checkSupport, createTestShaders, textureConversionTest, config);
									}

									{
										const glu::ShaderType			shaderType(rng.choose<glu::ShaderType>(begin(shaderTypes), end(shaderTypes)));
										const vk::VkChromaLocation		chromaLocation(rng.choose<ChromaLocationNamePair>(begin(chromaLocations), end(chromaLocations)).value);
										const TestConfig				config(shaderType, format, tiling, textureFilter, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
											vk::VK_FILTER_NEAREST, chromaLocation, chromaLocation, explicitReconstruction, disjoint,
											defaultColorRange, defaultColorModel, swappedChromaSwizzle, srcSize, dstSize, 0);

										addFunctionCaseWithPrograms(textureFilterGroup.get(), string("explicit_nearest") + "_" + tilingName + (disjoint ? "_disjoint" : "") + "_swapped_chroma", "", checkSupport, createTestShaders, textureConversionTest, config);
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
		for (size_t formatNdx = 0; formatNdx < xyChromaSubsampledFormats.size(); formatNdx++)
		{
			const vk::VkFormat				format(xyChromaSubsampledFormats[formatNdx]);
			const std::string				formatName(de::toLower(std::string(getFormatName(format)).substr(10)));
			de::MovePtr<tcu::TestCaseGroup>	formatGroup(new tcu::TestCaseGroup(testCtx, formatName.c_str(), ("Tests for color conversion using format " + formatName).c_str()));
			const UVec2						srcSize(isXChromaSubsampled(format) ? 12 : 7,
				isYChromaSubsampled(format) ? 8 : 13);
			const UVec2						dstSize(srcSize.x() + srcSize.x() / 2,
				srcSize.y() + srcSize.y() / 2);

			// Color conversion tests
			{
				de::MovePtr<tcu::TestCaseGroup>	conversionGroup(new tcu::TestCaseGroup(testCtx, "color_conversion", ""));

				for (size_t chromaOffsetNdx = 0; chromaOffsetNdx < chromaLocations.size(); chromaOffsetNdx++)
				{
					const char* const			chromaOffsetName(chromaLocations[chromaOffsetNdx].name);
					const vk::VkChromaLocation	chromaOffset(chromaLocations[chromaOffsetNdx].value);

					for (size_t modelNdx = 0; modelNdx < colorModels.size(); modelNdx++)
					{
						const char* const							colorModelName(colorModels[modelNdx].name);
						const vk::VkSamplerYcbcrModelConversion		colorModel(colorModels[modelNdx].value);

						if (colorModel != vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY && getYCbCrFormatChannelCount(format) < 3)
							continue;

						if (colorModel == vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY)
						{
							for (size_t tilingNdx = 0; tilingNdx < imageTilings.size(); tilingNdx++)
							{
								const vk::VkImageTiling				tiling(imageTilings[tilingNdx].value);
								const char* const					tilingName(imageTilings[tilingNdx].name);
								const vk::VkSamplerYcbcrRange		colorRange(rng.choose<RangeNamePair>(begin(colorRanges), end(colorRanges)).value);
								const glu::ShaderType				shaderType(rng.choose<glu::ShaderType>(begin(shaderTypes), end(shaderTypes)));
								for (size_t bindingNdx = 0; bindingNdx < samplerBindings.size(); bindingNdx++)
								{
									const deUint32						samplerBinding(samplerBindings[bindingNdx].value);
									string								samplerBindingName((samplerBindings[bindingNdx].value != 0) ? string("_") + samplerBindings[bindingNdx].name : string());
									const TestConfig					config(shaderType, format, tiling, vk::VK_FILTER_NEAREST, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
										vk::VK_FILTER_NEAREST, chromaOffset, chromaOffset, false, false,
										colorRange, colorModel, getIdentitySwizzle(), srcSize, dstSize, samplerBinding);

									addFunctionCaseWithPrograms(conversionGroup.get(), std::string(colorModelName) + "_" + tilingName + "_" + chromaOffsetName + samplerBindingName, "", checkSupport, createTestShaders, textureConversionTest, config);
								}
							}
						}
						else
						{
							for (size_t rangeNdx = 0; rangeNdx < colorRanges.size(); rangeNdx++)
							{
								const char* const					colorRangeName(colorRanges[rangeNdx].name);
								const vk::VkSamplerYcbcrRange		colorRange(colorRanges[rangeNdx].value);

								// Narrow range doesn't really work with formats that have less than 8 bits
								if (colorRange == vk::VK_SAMPLER_YCBCR_RANGE_ITU_NARROW)
								{
									const UVec4	bitDepth(getYCbCrBitDepth(format));

									if (bitDepth[0] < 8 || bitDepth[1] < 8 || bitDepth[2] < 8)
										continue;
								}

								for (size_t tilingNdx = 0; tilingNdx < imageTilings.size(); tilingNdx++)
								{
									const vk::VkImageTiling			tiling(imageTilings[tilingNdx].value);
									const char* const				tilingName(imageTilings[tilingNdx].name);
									const glu::ShaderType			shaderType(rng.choose<glu::ShaderType>(begin(shaderTypes), end(shaderTypes)));
									for (size_t bindingNdx = 0; bindingNdx < samplerBindings.size(); bindingNdx++)
									{
										const deUint32					samplerBinding(samplerBindings[bindingNdx].value);
										string							samplerBindingName((samplerBindings[bindingNdx].value != 0) ? string("_") + samplerBindings[bindingNdx].name : string());
										const TestConfig				config(shaderType, format, tiling, vk::VK_FILTER_NEAREST, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
											vk::VK_FILTER_NEAREST, chromaOffset, chromaOffset, false, false,
											colorRange, colorModel, getIdentitySwizzle(), srcSize, dstSize, samplerBinding);

										addFunctionCaseWithPrograms(conversionGroup.get(), string(colorModelName) + "_" + colorRangeName + "_" + tilingName + "_" + chromaOffsetName + samplerBindingName, "", checkSupport, createTestShaders, textureConversionTest, config);
									}
								}
							}
						}
					}
				}

				formatGroup->addChild(conversionGroup.release());
			}

			// Color conversion tests for array of samplers ( xyChromaSubsampledFormats )
			if (getYCbCrFormatChannelCount(format) >= 3)
				buildArrayOfSamplersTests(format, srcSize, dstSize, formatGroup, testCtx, rng);

			// Chroma reconstruction tests
			{
				de::MovePtr<tcu::TestCaseGroup>	reconstrucGroup(new tcu::TestCaseGroup(testCtx, "chroma_reconstruction", ""));

				for (size_t textureFilterNdx = 0; textureFilterNdx < textureFilters.size(); textureFilterNdx++)
				{
					const char* const				textureFilterName(textureFilters[textureFilterNdx].name);
					const vk::VkFilter				textureFilter(textureFilters[textureFilterNdx].value);
					de::MovePtr<tcu::TestCaseGroup>	textureFilterGroup(new tcu::TestCaseGroup(testCtx, textureFilterName, textureFilterName));

					for (size_t explicitReconstructionNdx = 0; explicitReconstructionNdx < 2; explicitReconstructionNdx++)
					{
						const bool	explicitReconstruction(explicitReconstructionNdx == 1);

						for (size_t disjointNdx = 0; disjointNdx < 2; disjointNdx++)
						{
							const bool	disjoint(disjointNdx == 1);

							for (size_t xChromaOffsetNdx = 0; xChromaOffsetNdx < chromaLocations.size(); xChromaOffsetNdx++)
								for (size_t yChromaOffsetNdx = 0; yChromaOffsetNdx < chromaLocations.size(); yChromaOffsetNdx++)
								{
									const vk::VkChromaLocation		xChromaOffset(chromaLocations[xChromaOffsetNdx].value);
									const char* const				xChromaOffsetName(chromaLocations[xChromaOffsetNdx].name);

									const vk::VkChromaLocation		yChromaOffset(chromaLocations[yChromaOffsetNdx].value);
									const char* const				yChromaOffsetName(chromaLocations[yChromaOffsetNdx].name);

									for (size_t tilingNdx = 0; tilingNdx < imageTilings.size(); tilingNdx++)
									{
										const vk::VkImageTiling	tiling(imageTilings[tilingNdx].value);
										const char* const		tilingName(imageTilings[tilingNdx].name);
										{
											const glu::ShaderType	shaderType(rng.choose<glu::ShaderType>(begin(shaderTypes), end(shaderTypes)));
											const TestConfig		config(shaderType, format, tiling, textureFilter, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
												vk::VK_FILTER_LINEAR, xChromaOffset, yChromaOffset, explicitReconstruction, disjoint,
												defaultColorRange, defaultColorModel, getIdentitySwizzle(), srcSize, dstSize, 0);

											addFunctionCaseWithPrograms(textureFilterGroup.get(), string(explicitReconstruction ? "explicit_linear_" : "default_linear_") + xChromaOffsetName + "_" + yChromaOffsetName + "_" + tilingName + (disjoint ? "_disjoint" : ""), "", checkSupport, createTestShaders, textureConversionTest, config);
										}

										{
											const glu::ShaderType	shaderType(rng.choose<glu::ShaderType>(begin(shaderTypes), end(shaderTypes)));
											const TestConfig		config(shaderType, format, tiling, textureFilter, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
												vk::VK_FILTER_LINEAR, xChromaOffset, yChromaOffset, explicitReconstruction, disjoint,
												defaultColorRange, defaultColorModel, swappedChromaSwizzle, srcSize, dstSize, 0);

											addFunctionCaseWithPrograms(textureFilterGroup.get(), string(explicitReconstruction ? "explicit_linear_" : "default_linear_") + xChromaOffsetName + "_" + yChromaOffsetName + "_" + tilingName + (disjoint ? "_disjoint" : "") + "_swapped_chroma", "", checkSupport, createTestShaders, textureConversionTest, config);
										}

										if (!explicitReconstruction)
										{
											{
												const glu::ShaderType	shaderType(rng.choose<glu::ShaderType>(begin(shaderTypes), end(shaderTypes)));
												const TestConfig		config(shaderType, format, tiling, textureFilter, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
													vk::VK_FILTER_NEAREST, xChromaOffset, yChromaOffset, explicitReconstruction, disjoint,
													defaultColorRange, defaultColorModel, getIdentitySwizzle(), srcSize, dstSize, 0);

												addFunctionCaseWithPrograms(textureFilterGroup.get(), string("default_nearest_") + xChromaOffsetName + "_" + yChromaOffsetName + "_" + tilingName + (disjoint ? "_disjoint" : ""), "", checkSupport, createTestShaders, textureConversionTest, config);
											}

											{
												const glu::ShaderType	shaderType(rng.choose<glu::ShaderType>(begin(shaderTypes), end(shaderTypes)));
												const TestConfig		config(shaderType, format, tiling, textureFilter, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
													vk::VK_FILTER_NEAREST, xChromaOffset, yChromaOffset, explicitReconstruction, disjoint,
													defaultColorRange, defaultColorModel, swappedChromaSwizzle, srcSize, dstSize, 0);

												addFunctionCaseWithPrograms(textureFilterGroup.get(), string("default_nearest_") + xChromaOffsetName + "_" + yChromaOffsetName + "_" + tilingName + (disjoint ? "_disjoint" : "") + "_swapped_chroma", "", checkSupport, createTestShaders, textureConversionTest, config);
											}
										}
									}
								}

							if (explicitReconstruction)
							{
								for (size_t tilingNdx = 0; tilingNdx < imageTilings.size(); tilingNdx++)
								{
									const vk::VkImageTiling	tiling(imageTilings[tilingNdx].value);
									const char* const		tilingName(imageTilings[tilingNdx].name);
									{
										const glu::ShaderType			shaderType(rng.choose<glu::ShaderType>(begin(shaderTypes), end(shaderTypes)));
										const vk::VkChromaLocation		chromaLocation(rng.choose<ChromaLocationNamePair>(begin(chromaLocations), end(chromaLocations)).value);
										const TestConfig				config(shaderType, format, tiling, textureFilter, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
											vk::VK_FILTER_NEAREST, chromaLocation, chromaLocation, explicitReconstruction, disjoint,
											defaultColorRange, defaultColorModel, getIdentitySwizzle(), srcSize, dstSize, 0);

										addFunctionCaseWithPrograms(textureFilterGroup.get(), string("explicit_nearest") + "_" + tilingName + (disjoint ? "_disjoint" : ""), "", checkSupport, createTestShaders, textureConversionTest, config);
									}

									{
										const glu::ShaderType			shaderType(rng.choose<glu::ShaderType>(begin(shaderTypes), end(shaderTypes)));
										const vk::VkChromaLocation		chromaLocation(rng.choose<ChromaLocationNamePair>(begin(chromaLocations), end(chromaLocations)).value);
										const TestConfig				config(shaderType, format, tiling, textureFilter, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
											vk::VK_FILTER_NEAREST, chromaLocation, chromaLocation, explicitReconstruction, disjoint,
											defaultColorRange, defaultColorModel, swappedChromaSwizzle, srcSize, dstSize, 0);

										addFunctionCaseWithPrograms(textureFilterGroup.get(), string("explicit_nearest") + "_" + tilingName + (disjoint ? "_disjoint" : "") + "_swapped_chroma", "", checkSupport, createTestShaders, textureConversionTest, config);
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

		{
			const UVec2 imageSizes[] =
			{
				UVec2(16, 16),
				UVec2(20, 12)
			};

			de::MovePtr<tcu::TestCaseGroup>				oneToOneGroup(new tcu::TestCaseGroup(testCtx, "one_to_one", "Ycbcr images sampled to a frame buffer of the same dimentions."));

			const vk::VkFormat							format(vk::VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM_KHR);
			const vk::VkFilter							filter(vk::VK_FILTER_NEAREST);

			for (size_t sizeNdx = 0; sizeNdx < DE_LENGTH_OF_ARRAY(imageSizes); sizeNdx++)
			{
				const UVec2								srcSize(imageSizes[sizeNdx]);

				for (size_t xChromaOffsetNdx = 0; xChromaOffsetNdx < chromaLocations.size(); xChromaOffsetNdx++)
				{
					const vk::VkChromaLocation			xChromaOffset(chromaLocations[xChromaOffsetNdx].value);
					const char* const					xChromaOffsetName(chromaLocations[xChromaOffsetNdx].name);

					for (size_t yChromaOffsetNdx = 0; yChromaOffsetNdx < chromaLocations.size(); yChromaOffsetNdx++)
					{
						const vk::VkChromaLocation		yChromaOffset(chromaLocations[yChromaOffsetNdx].value);
						const char* const				yChromaOffsetName(chromaLocations[yChromaOffsetNdx].name);

						for (size_t tilingNdx = 0; tilingNdx < imageTilings.size(); tilingNdx++)
						{
							const vk::VkImageTiling		tiling(imageTilings[tilingNdx].value);
							const char* const			tilingName(imageTilings[tilingNdx].name);

							const glu::ShaderType		shaderType(rng.choose<glu::ShaderType>(begin(shaderTypes), end(shaderTypes)));

							const TestConfig			config(shaderType, format, tiling, filter, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
								filter, xChromaOffset, yChromaOffset, false, false,
								defaultColorRange, defaultColorModel, getIdentitySwizzle(), srcSize, srcSize, 0);
							std::ostringstream			testName;
							testName << string("implicit_nearest_") << srcSize.x() << "x" << srcSize.y() << "_" << tilingName << "_" << xChromaOffsetName << "_" << yChromaOffsetName;

							addFunctionCaseWithPrograms(oneToOneGroup.get(), testName.str(), "", checkSupport, createTestShaders, textureConversionTest, config);
						}
					}
				}
			}

			testGroup->addChild(oneToOneGroup.release());
		}
	}

	void buildArrayOfSamplersTests(const vk::VkFormat& format, const UVec2& srcSize, const UVec2& dstSize, de::MovePtr<tcu::TestCaseGroup>& formatGroup, tcu::TestContext& testCtx, de::Random& rng )
	{
		de::MovePtr<tcu::TestCaseGroup>			samplerArrayGroup(new tcu::TestCaseGroup(testCtx, "sampler_array", "Tests for array of samplers"));

		for (size_t textureFilterNdx = 0; textureFilterNdx < textureFilters.size(); textureFilterNdx++)
		{
			const char* const					textureFilterName(textureFilters[textureFilterNdx].name);
			const vk::VkFilter					textureFilter(textureFilters[textureFilterNdx].value);

			for (size_t tilingNdx = 0; tilingNdx < imageTilings.size(); tilingNdx++)
			{
				const vk::VkImageTiling			tiling(imageTilings[tilingNdx].value);
				const char* const				tilingName(imageTilings[tilingNdx].name);
				const glu::ShaderType			shaderType(rng.choose<glu::ShaderType>(shaderTypes.begin(), shaderTypes.end()));
				const vk::VkSamplerYcbcrRange	colorRange(vk::VK_SAMPLER_YCBCR_RANGE_ITU_FULL);
				const vk::VkChromaLocation		chromaLocation(rng.choose<ChromaLocationNamePair>(chromaLocations.begin(), chromaLocations.end()).value);

				for (size_t bindingNdx = 0; bindingNdx < samplerBindings.size(); bindingNdx++)
				{
					const deUint32					samplerBinding(samplerBindings[bindingNdx].value);
					string							samplerBindingName((samplerBindings[bindingNdx].value != 0) ? string("_") + samplerBindings[bindingNdx].name : string());
					// colorModel==VK_SAMPLER_YCBCR_MODEL_CONVERSION_LAST means that we want to create an array of samplers instead of a single sampler
					const TestConfig				config(shaderType, format, tiling, textureFilter, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
						textureFilter, chromaLocation, chromaLocation, false, false,
						colorRange, vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_LAST, getIdentitySwizzle(), srcSize, dstSize, samplerBinding);

					addFunctionCaseWithPrograms(samplerArrayGroup.get(), std::string(textureFilterName) + "_" + tilingName + samplerBindingName, "", checkSupport, createTestShaders, textureConversionTest, config);
				}
			}
		}
		formatGroup->addChild(samplerArrayGroup.release());
	}
};

void initTests(tcu::TestCaseGroup* testGroup)
{
	YCbCrConversionTestBuilder testBuilder;
	testBuilder.buildTests(testGroup);
}

} // anonymous

tcu::TestCaseGroup* createConversionTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "conversion", "Sampler YCbCr Conversion Tests", initTests);
}

} // ycbcr
} // vkt
