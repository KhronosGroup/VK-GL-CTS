/*-------------------------------------------------------------------------
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
 * \brief YCbCr Image View Tests
 *//*--------------------------------------------------------------------*/

#include "vktYCbCrViewTests.hpp"
#include "vktYCbCrUtil.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktShaderExecutor.hpp"

#include "vkStrUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkCmdUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuVectorUtil.hpp"

#include "deStringUtil.hpp"
#include "deSharedPtr.hpp"
#include "deUniquePtr.hpp"
#include "deRandom.hpp"
#include "deSTLUtil.hpp"

namespace vkt
{
namespace ycbcr
{
namespace
{

using namespace vk;
using namespace shaderexecutor;

using tcu::UVec2;
using tcu::Vec2;
using tcu::Vec4;
using tcu::TestLog;
using de::MovePtr;
using de::UniquePtr;
using std::vector;
using std::string;

// List of some formats compatible with formats listed in "Plane Format Compatibility Table".
const VkFormat s_compatible_formats[] =
{
	// 8-bit compatibility class
	// Compatible format for VK_FORMAT_R8_UNORM
	VK_FORMAT_R4G4_UNORM_PACK8,
	VK_FORMAT_R8_UINT,
	VK_FORMAT_R8_SINT,
	// 16-bit compatibility class
	// Compatible formats with VK_FORMAT_R8G8_UNORM, VK_FORMAT_R10X6_UNORM_PACK16, VK_FORMAT_R12X4_UNORM_PACK16 and VK_FORMAT_R16_UNORM
	VK_FORMAT_R8G8_UNORM,
	VK_FORMAT_R8G8_UINT,
	VK_FORMAT_R10X6_UNORM_PACK16,
	VK_FORMAT_R12X4_UNORM_PACK16,
	VK_FORMAT_R16_UNORM,
	VK_FORMAT_R16_UINT,
	VK_FORMAT_R16_SINT,
	VK_FORMAT_R4G4B4A4_UNORM_PACK16,
	// 32-bit compatibility class
	// Compatible formats for VK_FORMAT_R10X6G10X6_UNORM_2PACK16, VK_FORMAT_R12X4G12X4_UNORM_2PACK16 and VK_FORMAT_R16G16_UNORM
	VK_FORMAT_R10X6G10X6_UNORM_2PACK16,
	VK_FORMAT_R12X4G12X4_UNORM_2PACK16,
	VK_FORMAT_R16G16_UNORM,
	VK_FORMAT_R8G8B8A8_UNORM,
	VK_FORMAT_R8G8B8A8_UINT,
	VK_FORMAT_R32_UINT,
};

inline bool formatsAreCompatible (const VkFormat format0, const VkFormat format1)
{
	return format0 == format1 || mapVkFormat(format0).getPixelSize() == mapVkFormat(format1).getPixelSize();
}

Move<VkImage> createTestImage (const DeviceInterface&	vkd,
							   VkDevice					device,
							   VkFormat					format,
							   const UVec2&				size,
							   VkImageCreateFlags		createFlags)
{
	const VkImageCreateInfo		createInfo	=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		DE_NULL,
		createFlags,
		VK_IMAGE_TYPE_2D,
		format,
		makeExtent3D(size.x(), size.y(), 1u),
		1u,		// mipLevels
		1u,		// arrayLayers
		VK_SAMPLE_COUNT_1_BIT,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		0u,
		(const deUint32*)DE_NULL,
		VK_IMAGE_LAYOUT_UNDEFINED,
	};

	return createImage(vkd, device, &createInfo);
}

Move<VkImageView> createImageView (const DeviceInterface&				vkd,
								   VkDevice								device,
								   VkImage								image,
								   VkFormat								format,
								   VkImageAspectFlagBits				imageAspect,
								   const VkSamplerYcbcrConversionInfo*	samplerConversionInfo)
{
	const VkImageViewCreateInfo				viewInfo	=
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		samplerConversionInfo,
		(VkImageViewCreateFlags)0,
		image,
		VK_IMAGE_VIEW_TYPE_2D,
		format,
		{
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
		},
		{ (VkImageAspectFlags)imageAspect, 0u, 1u, 0u, 1u },
	};

	return createImageView(vkd, device, &viewInfo);
}

// Descriptor layout for set 1:
// 0: Plane view bound as COMBINED_IMAGE_SAMPLER
// 1: "Whole" image bound as COMBINED_IMAGE_SAMPLER
//    + immutable sampler (required for color conversion)

Move<VkDescriptorSetLayout> createDescriptorSetLayout (const DeviceInterface& vkd, VkDevice device, VkSampler conversionSampler)
{
	const VkDescriptorSetLayoutBinding		bindings[]	=
	{
		{
			0u,												// binding
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			1u,												// descriptorCount
			VK_SHADER_STAGE_ALL,
			(const VkSampler*)DE_NULL
		},
		{
			1u,												// binding
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			1u,												// descriptorCount
			VK_SHADER_STAGE_ALL,
			&conversionSampler
		}
	};
	const VkDescriptorSetLayoutCreateInfo	layoutInfo	=
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		DE_NULL,
		(VkDescriptorSetLayoutCreateFlags)0u,
		DE_LENGTH_OF_ARRAY(bindings),
		bindings,
	};

	return createDescriptorSetLayout(vkd, device, &layoutInfo);
}

Move<VkDescriptorPool> createDescriptorPool (const DeviceInterface& vkd, VkDevice device, const deUint32 combinedSamplerDescriptorCount)
{
	const VkDescriptorPoolSize			poolSizes[]	=
	{
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,	2u * combinedSamplerDescriptorCount	},
	};
	const VkDescriptorPoolCreateInfo	poolInfo	=
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		DE_NULL,
		(VkDescriptorPoolCreateFlags)VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
		1u,		// maxSets
		DE_LENGTH_OF_ARRAY(poolSizes),
		poolSizes,
	};

	return createDescriptorPool(vkd, device, & poolInfo);
}

Move<VkDescriptorSet> createDescriptorSet (const DeviceInterface&	vkd,
										   VkDevice					device,
										   VkDescriptorPool			descPool,
										   VkDescriptorSetLayout	descLayout,
										   VkImageView				planeView,
										   VkSampler				planeViewSampler,
										   VkImageView				wholeView,
										   VkSampler				wholeViewSampler)
{
	Move<VkDescriptorSet>	descSet;

	{
		const VkDescriptorSetAllocateInfo	allocInfo	=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			DE_NULL,
			descPool,
			1u,
			&descLayout,
		};

		descSet = allocateDescriptorSet(vkd, device, &allocInfo);
	}

	{
		const VkDescriptorImageInfo		imageInfo0			=
		{
			planeViewSampler,
			planeView,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};
		const VkDescriptorImageInfo		imageInfo1			=
		{
			wholeViewSampler,
			wholeView,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};
		const VkWriteDescriptorSet		descriptorWrites[]		=
		{
			{
				VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				DE_NULL,
				*descSet,
				0u,		// dstBinding
				0u,		// dstArrayElement
				1u,		// descriptorCount
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				&imageInfo0,
				(const VkDescriptorBufferInfo*)DE_NULL,
				(const VkBufferView*)DE_NULL,
			},
			{
				VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				DE_NULL,
				*descSet,
				1u,		// dstBinding
				0u,		// dstArrayElement
				1u,		// descriptorCount
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				&imageInfo1,
				(const VkDescriptorBufferInfo*)DE_NULL,
				(const VkBufferView*)DE_NULL,
			}
		};

		vkd.updateDescriptorSets(device, DE_LENGTH_OF_ARRAY(descriptorWrites), descriptorWrites, 0u, DE_NULL);
	}

	return descSet;
}

void executeImageBarrier (const DeviceInterface&		vkd,
						  VkDevice						device,
						  deUint32						queueFamilyNdx,
						  VkPipelineStageFlags			srcStage,
						  VkPipelineStageFlags			dstStage,
						  const VkImageMemoryBarrier&	barrier)
{
	const VkQueue					queue		= getDeviceQueue(vkd, device, queueFamilyNdx, 0u);
	const Unique<VkCommandPool>		cmdPool		(createCommandPool(vkd, device, (VkCommandPoolCreateFlags)0, queueFamilyNdx));
	const Unique<VkCommandBuffer>	cmdBuffer	(allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	beginCommandBuffer(vkd, *cmdBuffer);

	vkd.cmdPipelineBarrier(*cmdBuffer,
						   srcStage,
						   dstStage,
						   (VkDependencyFlags)0u,
						   0u,
						   (const VkMemoryBarrier*)DE_NULL,
						   0u,
						   (const VkBufferMemoryBarrier*)DE_NULL,
						   1u,
						   &barrier);

	endCommandBuffer(vkd, *cmdBuffer);

	submitCommandsAndWait(vkd, device, queue, *cmdBuffer);
}

struct TestParameters
{
	enum ViewType
	{
		VIEWTYPE_IMAGE_VIEW	= 0,
		VIEWTYPE_MEMORY_ALIAS,

		VIEWTYPE_LAST
	};

	ViewType			viewType;
	VkFormat			format;
	UVec2				size;
	VkImageCreateFlags	createFlags;
	deUint32			planeNdx;
	VkFormat			planeCompatibleFormat;
	glu::ShaderType		shaderType;
	deBool				isCompatibilityFormat;

	TestParameters (ViewType viewType_, VkFormat format_, const UVec2& size_, VkImageCreateFlags createFlags_, deUint32 planeNdx_, VkFormat planeCompatibleFormat_, glu::ShaderType shaderType_, deBool isCompatibilityFormat_)
		: viewType				(viewType_)
		, format				(format_)
		, size					(size_)
		, createFlags			(createFlags_)
		, planeNdx				(planeNdx_)
		, planeCompatibleFormat	(planeCompatibleFormat_)
		, shaderType			(shaderType_)
		, isCompatibilityFormat	(isCompatibilityFormat_)
	{
	}

	TestParameters (void)
		: viewType				(VIEWTYPE_LAST)
		, format				(VK_FORMAT_UNDEFINED)
		, createFlags			(0u)
		, planeNdx				(0u)
		, planeCompatibleFormat	(VK_FORMAT_UNDEFINED)
		, shaderType			(glu::SHADERTYPE_LAST)
		, isCompatibilityFormat	(false)
	{
	}
};

static std::string getSamplerDecl(VkFormat f) {
	if (isIntFormat(f))			return "isampler2D";
	else if (isUintFormat(f))	return "usampler2D";
	else						return "sampler2D";
}

ShaderSpec getShaderSpec (const TestParameters& params)
{
	ShaderSpec spec;

	spec.inputs.push_back(Symbol("texCoord", glu::VarType(glu::TYPE_FLOAT_VEC2, glu::PRECISION_HIGHP)));
	spec.outputs.push_back(Symbol("result0", glu::VarType(glu::TYPE_FLOAT_VEC4, glu::PRECISION_HIGHP)));
	spec.outputs.push_back(Symbol("result1", glu::VarType(glu::TYPE_FLOAT_VEC4, glu::PRECISION_HIGHP)));

	const std::string sampler = getSamplerDecl(params.planeCompatibleFormat);
	spec.globalDeclarations =
		"layout(binding = 1, set = 1) uniform highp sampler2D u_image;\n"
		"layout(binding = 0, set = 1) uniform highp " + sampler + " u_planeView;\n";

	spec.source =
		"result0 = texture(u_image, texCoord);\n"
		"result1 = vec4(texture(u_planeView, texCoord));\n";

	return spec;
}


void generateLookupCoordinates (const UVec2& imageSize, size_t numCoords, de::Random* rnd, vector<Vec2>* dst)
{
	dst->resize(numCoords);

	for (size_t coordNdx = 0; coordNdx < numCoords; ++coordNdx)
	{
		const deUint32	texelX	= rnd->getUint32() % imageSize.x();
		const deUint32	texelY	= rnd->getUint32() % imageSize.y();
		const float		x		= ((float)texelX + 0.5f) / (float)imageSize.x();
		const float		y		= ((float)texelY + 0.5f) / (float)imageSize.y();

		(*dst)[coordNdx] = Vec2(x, y);
	}
}

void checkImageUsageSupport (Context&			context,
							 VkFormat			format,
							 VkImageUsageFlags	usage)
{
	const VkFormatProperties	formatProperties	= getPhysicalDeviceFormatProperties(context.getInstanceInterface(),
																						context.getPhysicalDevice(),
																						format);
	const VkFormatFeatureFlags	featureFlags		= formatProperties.optimalTilingFeatures;

	if ((usage & VK_IMAGE_USAGE_SAMPLED_BIT) != 0
		&& (featureFlags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) == 0)
	{
		TCU_THROW(NotSupportedError, "Format doesn't support sampling");
	}

	// Other image usages are not handled currently
	DE_ASSERT((usage & ~(VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT)) == 0);
}

void checkSupport(Context& context, TestParameters params)
{
	const VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	checkImageSupport(context, params.format, params.createFlags);
	checkImageUsageSupport(context, params.format, usage);
	checkImageUsageSupport(context, params.planeCompatibleFormat, usage);
}

tcu::TestStatus testPlaneView (Context& context, TestParameters params)
{
	de::Random						randomGen		(deInt32Hash((deUint32)params.format)	^
													 deInt32Hash((deUint32)params.planeNdx)	^
													 deInt32Hash((deUint32)params.shaderType));

	const InstanceInterface&		vk				= context.getInstanceInterface();
	const DeviceInterface&			vkd				= context.getDeviceInterface();
	const VkDevice					device			= context.getDevice();

	const VkFormat					format			= params.format;
	const VkImageCreateFlags		createFlags		= params.createFlags;
	const PlanarFormatDescription	formatInfo		= getPlanarFormatDescription(format);
	const UVec2						size			= params.size;
	const UVec2						planeExtent		= getPlaneExtent(formatInfo, size, params.planeNdx, 0);
	const Unique<VkImage>			image			(createTestImage(vkd, device, format, size, createFlags));
	const Unique<VkImage>			imageAlias		((params.viewType == TestParameters::VIEWTYPE_MEMORY_ALIAS)
													 ? createTestImage(vkd, device, params.planeCompatibleFormat, planeExtent, createFlags)
													 : Move<VkImage>());
	const vector<AllocationSp>		allocations		(allocateAndBindImageMemory(vkd, device, context.getDefaultAllocator(), *image, format, createFlags));

	if (imageAlias)
	{
		if ((createFlags & VK_IMAGE_CREATE_DISJOINT_BIT) != 0)
		{
			VkBindImagePlaneMemoryInfo	planeInfo	=
			{
				VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO_KHR,
				DE_NULL,
				VK_IMAGE_ASPECT_PLANE_0_BIT_KHR
			};

			VkBindImageMemoryInfo coreInfo	=
			{
				VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO_KHR,
				&planeInfo,
				*imageAlias,
				allocations[params.planeNdx]->getMemory(),
				allocations[params.planeNdx]->getOffset(),
			};

			VK_CHECK(vkd.bindImageMemory2(device, 1, &coreInfo));
		}
		else
		{
			VK_CHECK(vkd.bindImageMemory(device, *imageAlias, allocations[params.planeNdx]->getMemory(), allocations[params.planeNdx]->getOffset()));
		}
	}

	const VkSamplerYcbcrConversionCreateInfo	conversionInfo	=
	{
		VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO,
		DE_NULL,
		format,
		VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY,
		VK_SAMPLER_YCBCR_RANGE_ITU_FULL,
		{
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
		},
		VK_CHROMA_LOCATION_MIDPOINT,
		VK_CHROMA_LOCATION_MIDPOINT,
		VK_FILTER_NEAREST,
		VK_FALSE,									// forceExplicitReconstruction
	};
	const Unique<VkSamplerYcbcrConversion>		conversion	(createSamplerYcbcrConversion(vkd, device, &conversionInfo));
	const VkSamplerYcbcrConversionInfo			samplerConversionInfo	=
	{
		VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO,
		DE_NULL,
		*conversion,
	};
	const Unique<VkImageView>					wholeView	(createImageView(vkd, device, *image, format, VK_IMAGE_ASPECT_COLOR_BIT, &samplerConversionInfo));
	const Unique<VkImageView>					planeView	(createImageView(vkd,
																			 device,
																			 !imageAlias ? *image : *imageAlias,
																			 params.planeCompatibleFormat,
																			 !imageAlias ? getPlaneAspect(params.planeNdx) : VK_IMAGE_ASPECT_COLOR_BIT,
																			 DE_NULL));

	const VkSamplerCreateInfo					wholeSamplerInfo		=
	{
		VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		&samplerConversionInfo,
		0u,
		VK_FILTER_NEAREST,							// magFilter
		VK_FILTER_NEAREST,							// minFilter
		VK_SAMPLER_MIPMAP_MODE_NEAREST,				// mipmapMode
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		// addressModeU
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		// addressModeV
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		// addressModeW
		0.0f,										// mipLodBias
		VK_FALSE,									// anisotropyEnable
		1.0f,										// maxAnisotropy
		VK_FALSE,									// compareEnable
		VK_COMPARE_OP_ALWAYS,						// compareOp
		0.0f,										// minLod
		0.0f,										// maxLod
		VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,	// borderColor
		VK_FALSE,									// unnormalizedCoords
	};
	const VkSamplerCreateInfo					planeSamplerInfo		=
	{
		VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		DE_NULL,
		0u,
		VK_FILTER_NEAREST,							// magFilter
		VK_FILTER_NEAREST,							// minFilter
		VK_SAMPLER_MIPMAP_MODE_NEAREST,				// mipmapMode
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		// addressModeU
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		// addressModeV
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		// addressModeW
		0.0f,										// mipLodBias
		VK_FALSE,									// anisotropyEnable
		1.0f,										// maxAnisotropy
		VK_FALSE,									// compareEnable
		VK_COMPARE_OP_ALWAYS,						// compareOp
		0.0f,										// minLod
		0.0f,										// maxLod
		VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,	// borderColor
		VK_FALSE,									// unnormalizedCoords
	};

	deUint32									combinedSamplerDescriptorCount = 1;
	{
		const VkPhysicalDeviceImageFormatInfo2			imageFormatInfo				=
		{
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,	// sType;
			DE_NULL,												// pNext;
			format,													// format;
			VK_IMAGE_TYPE_2D,										// type;
			VK_IMAGE_TILING_OPTIMAL,								// tiling;
			VK_IMAGE_USAGE_TRANSFER_DST_BIT |
			VK_IMAGE_USAGE_SAMPLED_BIT,								// usage;
			createFlags												// flags;
		};

		VkSamplerYcbcrConversionImageFormatProperties	samplerYcbcrConversionImage = {};
		samplerYcbcrConversionImage.sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_IMAGE_FORMAT_PROPERTIES;
		samplerYcbcrConversionImage.pNext = DE_NULL;

		VkImageFormatProperties2						imageFormatProperties		= {};
		imageFormatProperties.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
		imageFormatProperties.pNext = &samplerYcbcrConversionImage;

		VkResult result = vk.getPhysicalDeviceImageFormatProperties2(context.getPhysicalDevice(), &imageFormatInfo, &imageFormatProperties);
		if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
			TCU_THROW(NotSupportedError, "Format not supported.");
		VK_CHECK(result);
		combinedSamplerDescriptorCount = samplerYcbcrConversionImage.combinedImageSamplerDescriptorCount;
	}

	const Unique<VkSampler>					wholeSampler(createSampler(vkd, device, &wholeSamplerInfo));
	const Unique<VkSampler>					planeSampler(createSampler(vkd, device, &planeSamplerInfo));

	const Unique<VkDescriptorSetLayout>		descLayout	(createDescriptorSetLayout(vkd, device, *wholeSampler));
	const Unique<VkDescriptorPool>			descPool	(createDescriptorPool(vkd, device, combinedSamplerDescriptorCount));
	const Unique<VkDescriptorSet>			descSet		(createDescriptorSet(vkd, device, *descPool, *descLayout, *planeView, *planeSampler, *wholeView, *wholeSampler));

	MultiPlaneImageData						imageData	(format, size);

	// Prepare texture data
	fillRandom(&randomGen, &imageData);

	if (imageAlias)
	{
		// Transition alias to right layout first
		const VkImageMemoryBarrier		initAliasBarrier	=
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			DE_NULL,
			(VkAccessFlags)0,
			VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_QUEUE_FAMILY_IGNORED,
			VK_QUEUE_FAMILY_IGNORED,
			*imageAlias,
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u }
		};

		executeImageBarrier(vkd,
							device,
							context.getUniversalQueueFamilyIndex(),
							(VkPipelineStageFlags)VK_PIPELINE_STAGE_HOST_BIT,
							(VkPipelineStageFlags)VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
							initAliasBarrier);
	}

	// Upload and prepare image
	uploadImage(vkd,
				device,
				context.getUniversalQueueFamilyIndex(),
				context.getDefaultAllocator(),
				*image,
				imageData,
				(VkAccessFlags)VK_ACCESS_SHADER_READ_BIT,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	{
		const size_t	numValues		= 500;
		vector<Vec2>	texCoord		(numValues);
		vector<Vec4>	resultWhole		(numValues);
		vector<Vec4>	resultPlane		(numValues);
		vector<Vec4>	referenceWhole	(numValues);
		vector<Vec4>	referencePlane	(numValues);
		bool			allOk			= true;
		Vec4			threshold		(0.02f);

		generateLookupCoordinates(size, numValues, &randomGen, &texCoord);

		{
			UniquePtr<ShaderExecutor>	executor	(createExecutor(context, params.shaderType, getShaderSpec(params), *descLayout));
			const void*					inputs[]	= { texCoord[0].getPtr() };
			void*						outputs[]	= { resultWhole[0].getPtr(), resultPlane[0].getPtr() };

			executor->execute((int)numValues, inputs, outputs, *descSet);
		}

		// Whole image sampling reference
		for (deUint32 channelNdx = 0; channelNdx < 4; channelNdx++)
		{
			if (formatInfo.hasChannelNdx(channelNdx))
			{
				const tcu::ConstPixelBufferAccess	channelAccess	= imageData.getChannelAccess(channelNdx);
				const tcu::Sampler					refSampler		= mapVkSampler(wholeSamplerInfo);
				const tcu::Texture2DView			refTexView		(1u, &channelAccess);

				for (size_t ndx = 0; ndx < numValues; ++ndx)
				{
					const Vec2&	coord	= texCoord[ndx];
					referenceWhole[ndx][channelNdx] = refTexView.sample(refSampler, coord.x(), coord.y(), 0.0f)[0];
				}
			}
			else
			{
				for (size_t ndx = 0; ndx < numValues; ++ndx)
					referenceWhole[ndx][channelNdx] = channelNdx == 3 ? 1.0f : 0.0f;
			}
		}

		// Plane view sampling reference
		{
			const tcu::ConstPixelBufferAccess	planeAccess		(mapVkFormat(params.planeCompatibleFormat),
																 tcu::IVec3((int)planeExtent.x(), (int)planeExtent.y(), 1),
																 imageData.getPlanePtr(params.planeNdx));
			const tcu::Sampler					refSampler		= mapVkSampler(planeSamplerInfo);
			const tcu::Texture2DView			refTexView		(1u, &planeAccess);

			for (size_t ndx = 0; ndx < numValues; ++ndx)
			{
				const Vec2&	coord	= texCoord[ndx];
				referencePlane[ndx] = refTexView.sample(refSampler, coord.x(), coord.y(), 0.0f);
			}
		}

		for (int viewNdx = 0; viewNdx < 2; ++viewNdx)
		{
			const char* const	viewName	= (viewNdx == 0) ? "complete image"	: "plane view";
			const vector<Vec4>&	reference	= (viewNdx == 0) ? referenceWhole	: referencePlane;
			const vector<Vec4>&	result		= (viewNdx == 0) ? resultWhole		: resultPlane;

			for (size_t ndx = 0; ndx < numValues; ++ndx)
			{
				if (boolAny(greaterThanEqual(abs(result[ndx] - reference[ndx]), threshold)))
				{
					context.getTestContext().getLog()
						<< TestLog::Message << "ERROR: When sampling " << viewName << " at " << texCoord[ndx]
											<< ": got " << result[ndx]
											<< ", expected " << reference[ndx]
						<< TestLog::EndMessage;
					allOk = false;
				}
			}
		}

		if (allOk)
			return tcu::TestStatus::pass("All samples passed");
		else
			return tcu::TestStatus::fail("Got invalid results");
	}
}

void initPrograms (SourceCollections& dst, TestParameters params)
{
	const ShaderSpec	spec	= getShaderSpec(params);

	generateSources(params.shaderType, spec, dst);
}

void addPlaneViewCase (tcu::TestCaseGroup* group, const TestParameters& params)
{
	std::ostringstream name;

	name << de::toLower(de::toString(params.format).substr(10));

	if ((params.viewType != TestParameters::VIEWTYPE_MEMORY_ALIAS) &&
		((params.createFlags & VK_IMAGE_CREATE_DISJOINT_BIT) != 0))
		name << "_disjoint";

	name << "_plane_" << params.planeNdx;

	if (params.isCompatibilityFormat)
	{
		name << "_compatible_format_" << de::toLower(de::toString(params.planeCompatibleFormat).substr(10));
	}

	addFunctionCaseWithPrograms(group, name.str(), "", checkSupport, initPrograms, testPlaneView, params);
}

void populateViewTypeGroup (tcu::TestCaseGroup* group, TestParameters::ViewType viewType)
{
	const glu::ShaderType		shaderType	= glu::SHADERTYPE_FRAGMENT;
	const UVec2					size		(32, 58);
	const VkImageCreateFlags	baseFlags	= (VkImageCreateFlags)VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT
											| (viewType == TestParameters::VIEWTYPE_MEMORY_ALIAS ? (VkImageCreateFlags)VK_IMAGE_CREATE_ALIAS_BIT : 0u);

	auto addTests = [&](int formatNdx)
	{
		const VkFormat	format		= (VkFormat)formatNdx;
		const deUint32	numPlanes	= getPlaneCount(format);

		if (numPlanes == 1)
			return; // Plane views not possible

		for (int isDisjoint = 0; isDisjoint < 2; ++isDisjoint)
		{
			const VkImageCreateFlags	flags	= baseFlags | (isDisjoint == 1 ? (VkImageCreateFlags)VK_IMAGE_CREATE_DISJOINT_BIT : 0u);

			if ((viewType == TestParameters::VIEWTYPE_MEMORY_ALIAS) &&
				((flags & VK_IMAGE_CREATE_DISJOINT_BIT) == 0))
				continue; // Memory alias cases require disjoint planes

			for (deUint32 planeNdx = 0; planeNdx < numPlanes; ++planeNdx)
			{
				const VkFormat planeFormat = getPlaneCompatibleFormat(format, planeNdx);
				// Add test case using image view with a format taken from the "Plane Format Compatibility Table"
				addPlaneViewCase(group, TestParameters(viewType, format, size, flags, planeNdx, planeFormat, shaderType, DE_FALSE));

				// Add test cases using image view with a format that is compatible with the plane's format.
				// For example: VK_FORMAT_R4G4_UNORM_PACK8 is compatible with VK_FORMAT_R8_UNORM.
				for (const auto& compatibleFormat : s_compatible_formats)
				{
					if (compatibleFormat == planeFormat)
						continue;

					if (!formatsAreCompatible(planeFormat, compatibleFormat))
						continue;

					addPlaneViewCase(group, TestParameters(viewType, format, size, flags, planeNdx, compatibleFormat, shaderType, DE_TRUE));
				}
			}
		}
	};

	for (int formatNdx = VK_YCBCR_FORMAT_FIRST; formatNdx < VK_YCBCR_FORMAT_LAST; formatNdx++)
	{
		addTests(formatNdx);
	}

	for (int formatNdx = VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT; formatNdx < VK_FORMAT_G16_B16R16_2PLANE_444_UNORM_EXT; formatNdx++)
	{
		addTests(formatNdx);
	}
}

void populateViewGroup (tcu::TestCaseGroup* group)
{
	addTestGroup(group, "image_view",	"Plane View via VkImageView",		populateViewTypeGroup,	TestParameters::VIEWTYPE_IMAGE_VIEW);
	addTestGroup(group, "memory_alias",	"Plane View via Memory Aliasing",	populateViewTypeGroup,	TestParameters::VIEWTYPE_MEMORY_ALIAS);
}

} // anonymous

tcu::TestCaseGroup* createViewTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "plane_view", "YCbCr Plane View Tests", populateViewGroup);
}

} // ycbcr
} // vkt

