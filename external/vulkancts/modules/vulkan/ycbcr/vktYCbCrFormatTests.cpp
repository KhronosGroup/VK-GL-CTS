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
 * \brief YCbCr Format Tests
 *//*--------------------------------------------------------------------*/

#include "vktYCbCrFormatTests.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktShaderExecutor.hpp"
#include "vktYCbCrUtil.hpp"

#include "vkStrUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkPlatform.hpp"

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

Move<VkImage> createTestImage (const DeviceInterface&	vkd,
							   VkDevice					device,
							   VkFormat					format,
							   const UVec2&				size,
							   VkImageCreateFlags		createFlags,
							   VkImageTiling			tiling,
							   VkImageLayout			layout,
							   deUint32					arrayLayers)
{
	const VkImageCreateInfo		createInfo	=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		DE_NULL,
		createFlags,
		VK_IMAGE_TYPE_2D,
		format,
		makeExtent3D(size.x(), size.y(), 1u),
		1u,				// mipLevels
		arrayLayers,	// arrayLayers
		VK_SAMPLE_COUNT_1_BIT,
		tiling,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		0u,
		(const deUint32*)DE_NULL,
		layout,
	};

	return createImage(vkd, device, &createInfo);
}

Move<VkImageView> createImageView (const DeviceInterface&		vkd,
								   VkDevice						device,
								   VkImage						image,
								   VkFormat						format,
								   VkSamplerYcbcrConversion		conversion,
								   deUint32						layerCount)
{
	const VkSamplerYcbcrConversionInfo		conversionInfo	=
	{
		VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO,
		DE_NULL,
		conversion
	};
	const VkImageViewCreateInfo				viewInfo		=
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		&conversionInfo,
		(VkImageViewCreateFlags)0,
		image,
		(layerCount > 1) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D,
		format,
		{
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
		},
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, layerCount },
	};

	return createImageView(vkd, device, &viewInfo);
}

Move<VkDescriptorSetLayout> createDescriptorSetLayout (const DeviceInterface& vkd, VkDevice device, VkSampler sampler)
{
	const VkDescriptorSetLayoutBinding		binding			=
	{
		0u,													// binding
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		1u,													// descriptorCount
		VK_SHADER_STAGE_ALL,
		&sampler
	};
	const VkDescriptorSetLayoutCreateInfo	layoutInfo	=
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		DE_NULL,
		(VkDescriptorSetLayoutCreateFlags)0u,
		1u,
		&binding,
	};

	return createDescriptorSetLayout(vkd, device, &layoutInfo);
}

Move<VkDescriptorPool> createDescriptorPool (const DeviceInterface& vkd, VkDevice device, const deUint32 combinedSamplerDescriptorCount)
{
	const VkDescriptorPoolSize			poolSizes[]	=
	{
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,	combinedSamplerDescriptorCount	},
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
										   VkImageView				imageView)
{
	Move<VkDescriptorSet>					descSet;

	{
		const VkDescriptorSetAllocateInfo	allocInfo			=
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
		const VkDescriptorImageInfo			imageInfo			=
		{
			0xdeadbeef,    // Not required to be valid. Use something invalid and not NULL
			imageView,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};
		const VkWriteDescriptorSet			descriptorWrite		=
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			DE_NULL,
			*descSet,
			0u,		// dstBinding
			0u,		// dstArrayElement
			1u,		// descriptorCount
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			&imageInfo,
			(const VkDescriptorBufferInfo*)DE_NULL,
			(const VkBufferView*)DE_NULL,
		};

		vkd.updateDescriptorSets(device, 1u, &descriptorWrite, 0u, DE_NULL);
	}

	return descSet;
}

struct TestParameters
{
	VkFormat			format;
	UVec2				size;
	VkImageCreateFlags	flags;
	VkImageTiling		tiling;
	glu::ShaderType		shaderType;
	bool				useMappedMemory;
	bool				useArrayLayers;

	TestParameters (VkFormat			format_,
					const UVec2&		size_,
					VkImageCreateFlags	flags_,
					VkImageTiling		tiling_,
					glu::ShaderType		shaderType_,
					bool				useMappedMemory_,
					bool				useArrayLayers_)
		: format			(format_)
		, size				(size_)
		, flags				(flags_)
		, tiling			(tiling_)
		, shaderType		(shaderType_)
		, useMappedMemory	(useMappedMemory_)
		, useArrayLayers	(useArrayLayers_)
	{
	}

	TestParameters (void)
		: format			(VK_FORMAT_UNDEFINED)
		, flags				(0u)
		, tiling			(VK_IMAGE_TILING_OPTIMAL)
		, shaderType		(glu::SHADERTYPE_LAST)
		, useMappedMemory	(false)
		, useArrayLayers	(false)
	{
	}
};

ShaderSpec getShaderSpec (const TestParameters& params)
{
	ShaderSpec spec;

	spec.inputs.push_back(Symbol("texCoord", glu::VarType(glu::TYPE_FLOAT_VEC2, glu::PRECISION_HIGHP)));
	spec.outputs.push_back(Symbol("result", glu::VarType(glu::TYPE_FLOAT_VEC4, glu::PRECISION_HIGHP)));

	if (params.useArrayLayers)
	{
		spec.globalDeclarations =
			"layout(binding = 0, set = 1) uniform highp sampler2DArray u_image;\n";
		spec.source =
			"result = texture(u_image, vec3(texCoord, 1u));\n";
	}
	else
	{
		spec.globalDeclarations =
			"layout(binding = 0, set = 1) uniform highp sampler2D u_image;\n";
		spec.source =
			"result = texture(u_image, texCoord);\n";
	}

	return spec;
}

void checkSupport (Context& context, const TestParameters params)
{
	checkImageSupport(context, params.format, params.flags, params.tiling);

	if (params.useArrayLayers)
	{
		if (!context.isDeviceFunctionalitySupported("VK_EXT_ycbcr_image_arrays"))
			TCU_THROW(NotSupportedError, "VK_EXT_ycbcr_image_arrays is not supported");

		VkImageFormatProperties properties = getPhysicalDeviceImageFormatProperties(context.getInstanceInterface(), context.getPhysicalDevice(),
			params.format, VK_IMAGE_TYPE_2D, params.tiling, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, params.flags);
		if (properties.maxArrayLayers < 2)
			TCU_THROW(NotSupportedError, "Image format does not support more than 1 maxArrayLayers");
	}
}

void generateLookupCoordinates (const UVec2& imageSize, vector<Vec2>* dst)
{
	dst->resize(imageSize.x() * imageSize.y());

	for (deUint32 texelY = 0; texelY < imageSize.y(); ++texelY)
	for (deUint32 texelX = 0; texelX < imageSize.x(); ++texelX)
	{
		const float		x	= ((float)texelX + 0.5f) / (float)imageSize.x();
		const float		y	= ((float)texelY + 0.5f) / (float)imageSize.y();

		(*dst)[texelY*imageSize.x() + texelX] = Vec2(x, y);
	}
}

tcu::TestStatus testFormat (Context& context, TestParameters params)
{
	const DeviceInterface&					vkd						= context.getDeviceInterface();
	const VkDevice							device					= context.getDevice();

	const VkFormat							format					= params.format;
	const PlanarFormatDescription			formatInfo				= getPlanarFormatDescription(format);
	const UVec2								size					= params.size;
	const VkImageCreateFlags				createFlags				= params.flags;
	const VkImageTiling						tiling					= params.tiling;
	const bool								mappedMemory			= params.useMappedMemory;
	const deUint32							arrayLayers				= (params.useArrayLayers) ? 2u : 1u;
	const deUint32							arrayLayer				= arrayLayers - 1u;

	const Unique<VkImage>					image					(createTestImage(vkd, device, format, size, createFlags, tiling, mappedMemory ? VK_IMAGE_LAYOUT_PREINITIALIZED : VK_IMAGE_LAYOUT_UNDEFINED, arrayLayers));
	const vector<AllocationSp>				allocations				(allocateAndBindImageMemory(vkd, device, context.getDefaultAllocator(), *image, format, createFlags, mappedMemory ? MemoryRequirement::HostVisible : MemoryRequirement::Any));

	const VkSamplerYcbcrConversionCreateInfo
											conversionInfo			=
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
	const Unique<VkSamplerYcbcrConversion>	conversion				(createSamplerYcbcrConversion(vkd, device, &conversionInfo));
	const Unique<VkImageView>				imageView				(createImageView(vkd, device, *image, format, *conversion, arrayLayers));

	const VkSamplerYcbcrConversionInfo		samplerConversionInfo	=
	{
		VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO,
		DE_NULL,
		*conversion,
	};

	const VkSamplerCreateInfo				samplerInfo				=
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

	const VkPhysicalDeviceImageFormatInfo2			imageFormatInfo	=
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
		DE_NULL,
		params.format,
		VK_IMAGE_TYPE_2D,
		params.tiling,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		params.flags,
	};
	VkSamplerYcbcrConversionImageFormatProperties	ycbcrProperties	=
	{
		VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_IMAGE_FORMAT_PROPERTIES,
		DE_NULL,
		0,
	};
	VkImageFormatProperties2						extProperties	=
	{
		VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
		&ycbcrProperties,
		{
			{
				0,	// width
				0,	// height
				0,	// depth
			},
			0u,		// maxMipLevels
			0u,		// maxArrayLayers
			0,		// sampleCounts
			0u,		// maxResourceSize
		},
	};
	VkResult						propsResult;
	const CustomInstance			instance	(createCustomInstanceWithExtension(context, "VK_KHR_get_physical_device_properties2"));
	const InstanceDriver&			vki			(instance.getDriver());

	// Verify that a yuv image consumes at least one descriptor
	propsResult = vki.getPhysicalDeviceImageFormatProperties2(context.getPhysicalDevice(), &imageFormatInfo, &extProperties);

	TCU_CHECK(propsResult == VK_SUCCESS);
	TCU_CHECK(ycbcrProperties.combinedImageSamplerDescriptorCount >= 1);

	const Unique<VkSampler>					sampler					(createSampler(vkd, device, &samplerInfo));

	const Unique<VkDescriptorSetLayout>		descLayout				(createDescriptorSetLayout(vkd, device, *sampler));
	const Unique<VkDescriptorPool>			descPool				(createDescriptorPool(vkd, device, ycbcrProperties.combinedImageSamplerDescriptorCount));
	const Unique<VkDescriptorSet>			descSet					(createDescriptorSet(vkd, device, *descPool, *descLayout, *imageView));

	MultiPlaneImageData						imageData				(format, size);

	// Zero fill unused layer
	if (params.useArrayLayers)
	{
		fillZero(&imageData);

		if (mappedMemory)
		{
			fillImageMemory(vkd,
							device,
							context.getUniversalQueueFamilyIndex(),
							*image,
							allocations,
							imageData,
							(VkAccessFlags)VK_ACCESS_SHADER_READ_BIT,
							VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
							0);
		}
		else
		{
			uploadImage(vkd,
						device,
						context.getUniversalQueueFamilyIndex(),
						context.getDefaultAllocator(),
						*image,
						imageData,
						(VkAccessFlags)VK_ACCESS_SHADER_READ_BIT,
						VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
						0);
		}
	}

	// Prepare texture data
	fillGradient(&imageData, Vec4(0.0f), Vec4(1.0f));

	if (mappedMemory)
	{
		// Fill and prepare image
		fillImageMemory(vkd,
						device,
						context.getUniversalQueueFamilyIndex(),
						*image,
						allocations,
						imageData,
						(VkAccessFlags)VK_ACCESS_SHADER_READ_BIT,
						VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
						arrayLayer);
	}
	else
	{
		// Upload and prepare image
		uploadImage(vkd,
					device,
					context.getUniversalQueueFamilyIndex(),
					context.getDefaultAllocator(),
					*image,
					imageData,
					(VkAccessFlags)VK_ACCESS_SHADER_READ_BIT,
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					arrayLayer);
	}

	{
		vector<Vec2>	texCoord;
		vector<Vec4>	result;
		vector<Vec4>	reference;
		bool			allOk		= true;
		Vec4			threshold	(0.02f);

		generateLookupCoordinates(size, &texCoord);

		result.resize(texCoord.size());
		reference.resize(texCoord.size());

		{
			UniquePtr<ShaderExecutor>	executor	(createExecutor(context, params.shaderType, getShaderSpec(params), *descLayout));
			const void*					inputs[]	= { texCoord[0].getPtr() };
			void*						outputs[]	= { result[0].getPtr() };

			executor->execute((int)texCoord.size(), inputs, outputs, *descSet);
		}

		for (deUint32 channelNdx = 0; channelNdx < 4; channelNdx++)
		{
			if (formatInfo.hasChannelNdx(channelNdx))
			{
				const tcu::ConstPixelBufferAccess	channelAccess	= imageData.getChannelAccess(channelNdx);
				const tcu::Sampler					refSampler		= mapVkSampler(samplerInfo);
				const tcu::Texture2DView			refTexView		(1u, &channelAccess);

				for (size_t ndx = 0; ndx < texCoord.size(); ++ndx)
				{
					const Vec2&	coord	= texCoord[ndx];
					reference[ndx][channelNdx] = refTexView.sample(refSampler, coord.x(), coord.y(), 0.0f)[0];
				}
			}
			else
			{
				for (size_t ndx = 0; ndx < texCoord.size(); ++ndx)
					reference[ndx][channelNdx] = channelNdx == 3 ? 1.0f : 0.0f;
			}
		}

		for (size_t ndx = 0; ndx < texCoord.size(); ++ndx)
		{
			if (boolAny(greaterThanEqual(abs(result[ndx] - reference[ndx]), threshold)))
			{
				context.getTestContext().getLog()
					<< TestLog::Message << "ERROR: At " << texCoord[ndx]
										<< ": got " << result[ndx]
										<< ", expected " << reference[ndx]
					<< TestLog::EndMessage;
				allOk = false;
			}
		}

		if (allOk)
			return tcu::TestStatus::pass("All samples passed");
		else
		{
			const tcu::ConstPixelBufferAccess	refAccess	(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::FLOAT),
															 tcu::IVec3((int)size.x(), (int)size.y(), 1u),
															 reference[0].getPtr());
			const tcu::ConstPixelBufferAccess	resAccess	(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::FLOAT),
															 tcu::IVec3((int)size.x(), (int)size.y(), 1u),
															 result[0].getPtr());

			context.getTestContext().getLog()
				<< TestLog::Image("Result", "Result Image", resAccess, Vec4(1.0f), Vec4(0.0f))
				<< TestLog::Image("Reference", "Reference Image", refAccess, Vec4(1.0f), Vec4(0.0f));

			return tcu::TestStatus::fail("Got invalid results");
		}
	}
}

void initPrograms (SourceCollections& dst, TestParameters params)
{
	const ShaderSpec	spec	= getShaderSpec(params);

	generateSources(params.shaderType, spec, dst);
}

void populatePerFormatGroup (tcu::TestCaseGroup* group, VkFormat format)
{
	const UVec2				size			(66, 32);
	const glu::ShaderType	shaderTypes[]	=
	{
		glu::SHADERTYPE_VERTEX,
		glu::SHADERTYPE_FRAGMENT,
		glu::SHADERTYPE_GEOMETRY,
		glu::SHADERTYPE_TESSELLATION_CONTROL,
		glu::SHADERTYPE_TESSELLATION_EVALUATION,
		glu::SHADERTYPE_COMPUTE
	};
	const struct
	{
		const char*		name;
		VkImageTiling	value;
	} tilings[] =
	{
		{ "optimal",	VK_IMAGE_TILING_OPTIMAL },
		{ "linear",		VK_IMAGE_TILING_LINEAR }
	};

	for (glu::ShaderType shaderType : shaderTypes)
	for (int tilingNdx = 0; tilingNdx < DE_LENGTH_OF_ARRAY(tilings); tilingNdx++)
	for (int useArrayLayers = 0; useArrayLayers < 2; useArrayLayers++)
	{
		const VkImageTiling		tiling			= tilings[tilingNdx].value;
		const char* const		tilingName		= tilings[tilingNdx].name;
		const char* const		shaderTypeName	= glu::getShaderTypeName(shaderType);
		const string			name			= string(shaderTypeName) + "_" + tilingName + ((useArrayLayers) ? "_array" : "");

		addFunctionCaseWithPrograms(group, name, checkSupport, initPrograms, testFormat, TestParameters(format, size, 0u, tiling, shaderType, false, useArrayLayers));

		if (getPlaneCount(format) > 1)
			addFunctionCaseWithPrograms(group, name + "_disjoint", checkSupport, initPrograms, testFormat, TestParameters(format, size, (VkImageCreateFlags)VK_IMAGE_CREATE_DISJOINT_BIT, tiling, shaderType, false, useArrayLayers));

		if (tiling == VK_IMAGE_TILING_LINEAR)
		{
			addFunctionCaseWithPrograms(group, name + "_mapped", checkSupport, initPrograms, testFormat, TestParameters(format, size, 0u, tiling, shaderType, true, useArrayLayers));

			if (getPlaneCount(format) > 1)
				addFunctionCaseWithPrograms(group, name + "_disjoint_mapped", checkSupport, initPrograms, testFormat, TestParameters(format, size, (VkImageCreateFlags)VK_IMAGE_CREATE_DISJOINT_BIT, tiling, shaderType, true, useArrayLayers));
		}
	}
}

void populateFormatGroup (tcu::TestCaseGroup* group)
{
	for (int formatNdx = VK_YCBCR_FORMAT_FIRST; formatNdx < VK_YCBCR_FORMAT_LAST; formatNdx++)
	{
		const VkFormat					format			= (VkFormat)formatNdx;
		const string					formatName		= de::toLower(de::toString(format).substr(10));

		group->addChild(createTestGroup<VkFormat>(group->getTestContext(), formatName, populatePerFormatGroup, format));
	}

	for (int formatNdx = VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT; formatNdx <= VK_FORMAT_G16_B16R16_2PLANE_444_UNORM_EXT; formatNdx++)
	{
		const VkFormat					format			= (VkFormat)formatNdx;
		const string					formatName		= de::toLower(de::toString(format).substr(10));

		group->addChild(createTestGroup<VkFormat>(group->getTestContext(), formatName, populatePerFormatGroup, format));
	}
}

} // namespace

tcu::TestCaseGroup* createFormatTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "format", populateFormatGroup);
}

} // ycbcr
} // vkt
