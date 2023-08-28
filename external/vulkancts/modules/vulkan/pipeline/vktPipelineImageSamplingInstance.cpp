/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Imagination Technologies Ltd.
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
 * \brief Image sampling case
 *//*--------------------------------------------------------------------*/

#include "vktPipelineImageSamplingInstance.hpp"
#include "vktPipelineClearUtil.hpp"
#include "vktPipelineReferenceRenderer.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"
#include "tcuTexLookupVerifier.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuTestLog.hpp"
#include "deSTLUtil.hpp"

namespace vkt
{
namespace pipeline
{

using namespace vk;
using de::MovePtr;
using de::UniquePtr;

namespace
{
de::MovePtr<Allocation> allocateBuffer (const InstanceInterface&	vki,
										const DeviceInterface&		vkd,
										const VkPhysicalDevice&		physDevice,
										const VkDevice				device,
										const VkBuffer&				buffer,
										const MemoryRequirement		requirement,
										Allocator&					allocator,
										AllocationKind				allocationKind)
{
	switch (allocationKind)
	{
		case ALLOCATION_KIND_SUBALLOCATED:
		{
			const VkMemoryRequirements	memoryRequirements	= getBufferMemoryRequirements(vkd, device, buffer);

			return allocator.allocate(memoryRequirements, requirement);
		}

		case ALLOCATION_KIND_DEDICATED:
		{
			return allocateDedicated(vki, vkd, physDevice, device, buffer, requirement);
		}

		default:
		{
			TCU_THROW(InternalError, "Invalid allocation kind");
		}
	}
}

de::MovePtr<Allocation> allocateImage (const InstanceInterface&		vki,
									   const DeviceInterface&		vkd,
									   const VkPhysicalDevice&		physDevice,
									   const VkDevice				device,
									   const VkImage&				image,
									   const MemoryRequirement		requirement,
									   Allocator&					allocator,
									   AllocationKind				allocationKind)
{
	switch (allocationKind)
	{
		case ALLOCATION_KIND_SUBALLOCATED:
		{
			const VkMemoryRequirements	memoryRequirements	= getImageMemoryRequirements(vkd, device, image);

			return allocator.allocate(memoryRequirements, requirement);
		}

		case ALLOCATION_KIND_DEDICATED:
		{
			return allocateDedicated(vki, vkd, physDevice, device, image, requirement);
		}

		default:
		{
			TCU_THROW(InternalError, "Invalid allocation kind");
		}
	}
}

static VkImageType getCompatibleImageType (VkImageViewType viewType)
{
	switch (viewType)
	{
		case VK_IMAGE_VIEW_TYPE_1D:				return VK_IMAGE_TYPE_1D;
		case VK_IMAGE_VIEW_TYPE_1D_ARRAY:		return VK_IMAGE_TYPE_1D;
		case VK_IMAGE_VIEW_TYPE_2D:				return VK_IMAGE_TYPE_2D;
		case VK_IMAGE_VIEW_TYPE_2D_ARRAY:		return VK_IMAGE_TYPE_2D;
		case VK_IMAGE_VIEW_TYPE_3D:				return VK_IMAGE_TYPE_3D;
		case VK_IMAGE_VIEW_TYPE_CUBE:			return VK_IMAGE_TYPE_2D;
		case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:		return VK_IMAGE_TYPE_2D;
		default:
			break;
	}

	DE_ASSERT(false);
	return VK_IMAGE_TYPE_1D;
}

template<typename TcuFormatType>
static MovePtr<TestTexture> createTestTexture (const TcuFormatType format, VkImageViewType viewType, const tcu::IVec3& size, int layerCount)
{
	MovePtr<TestTexture>	texture;
	const VkImageType		imageType = getCompatibleImageType(viewType);

	switch (imageType)
	{
		case VK_IMAGE_TYPE_1D:
			if (layerCount == 1)
				texture = MovePtr<TestTexture>(new TestTexture1D(format, size.x()));
			else
				texture = MovePtr<TestTexture>(new TestTexture1DArray(format, size.x(), layerCount));

			break;

		case VK_IMAGE_TYPE_2D:
			if (layerCount == 1)
			{
				texture = MovePtr<TestTexture>(new TestTexture2D(format, size.x(), size.y()));
			}
			else
			{
				if (viewType == VK_IMAGE_VIEW_TYPE_CUBE || viewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)
				{
					if (layerCount == tcu::CUBEFACE_LAST && viewType == VK_IMAGE_VIEW_TYPE_CUBE)
					{
						texture = MovePtr<TestTexture>(new TestTextureCube(format, size.x()));
					}
					else
					{
						DE_ASSERT(layerCount % tcu::CUBEFACE_LAST == 0);

						texture = MovePtr<TestTexture>(new TestTextureCubeArray(format, size.x(), layerCount));
					}
				}
				else
				{
					texture = MovePtr<TestTexture>(new TestTexture2DArray(format, size.x(), size.y(), layerCount));
				}
			}

			break;

		case VK_IMAGE_TYPE_3D:
			texture = MovePtr<TestTexture>(new TestTexture3D(format, size.x(), size.y(), size.z()));
			break;

		default:
			DE_ASSERT(false);
	}

	return texture;
}

} // anonymous

void checkSupportImageSamplingInstance (Context& context, ImageSamplingInstanceParams params)
{

	if (de::abs(params.samplerParams.mipLodBias) > context.getDeviceProperties().limits.maxSamplerLodBias)
		TCU_THROW(NotSupportedError, "Unsupported sampler Lod bias value");

	if (!isSupportedSamplableFormat(context.getInstanceInterface(), context.getPhysicalDevice(), params.imageFormat))
		throw tcu::NotSupportedError(std::string("Unsupported format for sampling: ") + getFormatName(params.imageFormat));

	if ((deUint32)params.imageCount > context.getDeviceProperties().limits.maxColorAttachments)
		throw tcu::NotSupportedError(std::string("Unsupported render target count: ") + de::toString(params.imageCount));

	if ((params.samplerParams.minFilter == VK_FILTER_LINEAR ||
		 params.samplerParams.magFilter == VK_FILTER_LINEAR ||
		 params.samplerParams.mipmapMode == VK_SAMPLER_MIPMAP_MODE_LINEAR) &&
		!isLinearFilteringSupported(context.getInstanceInterface(), context.getPhysicalDevice(), params.imageFormat, VK_IMAGE_TILING_OPTIMAL))
		throw tcu::NotSupportedError(std::string("Unsupported format for linear filtering: ") + getFormatName(params.imageFormat));

	if (params.separateStencilUsage)
	{
		context.requireDeviceFunctionality("VK_EXT_separate_stencil_usage");
		context.requireInstanceFunctionality("VK_KHR_get_physical_device_properties2");

		const VkImageStencilUsageCreateInfo  stencilUsage	=
		{
			VK_STRUCTURE_TYPE_IMAGE_STENCIL_USAGE_CREATE_INFO,
			DE_NULL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT
		};

		const VkPhysicalDeviceImageFormatInfo2	formatInfo2		=
		{
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,		//	VkStructureType			sType
			params.separateStencilUsage ? &stencilUsage
										: DE_NULL,						//	const void*				pNext
			params.imageFormat,											//	VkFormat				format
			getCompatibleImageType(params.imageViewType),				//	VkImageType				type
			VK_IMAGE_TILING_OPTIMAL,									//	VkImageTiling			tiling
			VK_IMAGE_USAGE_SAMPLED_BIT
			| VK_IMAGE_USAGE_TRANSFER_DST_BIT,							//	VkImageUsageFlags		usage
			(VkImageCreateFlags)0u										//	VkImageCreateFlags		flags
		};

		VkImageFormatProperties2				extProperties	=
		{
			VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
			DE_NULL,
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

		if ((context.getInstanceInterface().getPhysicalDeviceImageFormatProperties2(context.getPhysicalDevice(), &formatInfo2, &extProperties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
			|| extProperties.imageFormatProperties.maxExtent.width < (deUint32)params.imageSize.x()
			|| extProperties.imageFormatProperties.maxExtent.height < (deUint32)params.imageSize.y())
		{
			TCU_THROW(NotSupportedError, "Image format not supported");
		}
	}

	void const* pNext = params.samplerParams.pNext;
	while (pNext != DE_NULL)
	{
		const VkStructureType nextType = *reinterpret_cast<const VkStructureType*>(pNext);
		switch (nextType)
		{
			case VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO:
			{
				context.requireDeviceFunctionality("VK_EXT_sampler_filter_minmax");

				if (!isMinMaxFilteringSupported(context.getInstanceInterface(), context.getPhysicalDevice(), params.imageFormat, VK_IMAGE_TILING_OPTIMAL))
					throw tcu::NotSupportedError(std::string("Unsupported format for min/max filtering: ") + getFormatName(params.imageFormat));

				pNext = reinterpret_cast<const VkSamplerReductionModeCreateInfo*>(pNext)->pNext;
				break;
			}
			case VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO:
				context.requireDeviceFunctionality("VK_KHR_sampler_ycbcr_conversion");

				pNext = reinterpret_cast<const VkSamplerYcbcrConversionInfo*>(pNext)->pNext;
				break;
			case VK_STRUCTURE_TYPE_SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT:
				pNext = reinterpret_cast<const VkSamplerCustomBorderColorCreateInfoEXT*>(pNext)->pNext;

				if (!context.getCustomBorderColorFeaturesEXT().customBorderColors)
				{
					throw tcu::NotSupportedError("customBorderColors feature is not supported");
				}

				break;
			default:
				TCU_FAIL("Unrecognized sType in chained sampler create info");
		}
	}

	if (params.samplerParams.addressModeU == VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE ||
		params.samplerParams.addressModeV == VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE ||
		params.samplerParams.addressModeW == VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE)
	{
		context.requireDeviceFunctionality("VK_KHR_sampler_mirror_clamp_to_edge");
	}

	if ((isCompressedFormat(params.imageFormat) || isDepthStencilFormat(params.imageFormat)) && params.imageViewType == VK_IMAGE_VIEW_TYPE_3D)
	{
		// \todo [2016-01-22 pyry] Mandate VK_ERROR_FORMAT_NOT_SUPPORTED
		try
		{
			const VkImageFormatProperties	formatProperties	= getPhysicalDeviceImageFormatProperties(context.getInstanceInterface(),
																										 context.getPhysicalDevice(),
																										 params.imageFormat,
																										 VK_IMAGE_TYPE_3D,
																										 VK_IMAGE_TILING_OPTIMAL,
																										 VK_IMAGE_USAGE_SAMPLED_BIT,
																										 (VkImageCreateFlags)0);

			if (formatProperties.maxExtent.width == 0 &&
				formatProperties.maxExtent.height == 0 &&
				formatProperties.maxExtent.depth == 0)
				TCU_THROW(NotSupportedError, "3D compressed or depth format not supported");
		}
		catch (const Error&)
		{
			TCU_THROW(NotSupportedError, "3D compressed or depth format not supported");
		}
	}

	if (params.imageViewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_IMAGE_CUBE_ARRAY);

	if (params.allocationKind == ALLOCATION_KIND_DEDICATED)
		context.requireDeviceFunctionality("VK_KHR_dedicated_allocation");

#ifndef CTS_USES_VULKANSC
	if (context.isDeviceFunctionalitySupported("VK_KHR_portability_subset"))
	{
		const auto portabilitySubsetFeatures	= context.getPortabilitySubsetFeatures();
		const auto componentMapping				= params.componentMapping;
		if (!portabilitySubsetFeatures.imageViewFormatSwizzle &&
			((componentMapping.r != VK_COMPONENT_SWIZZLE_IDENTITY) ||
			 (componentMapping.g != VK_COMPONENT_SWIZZLE_IDENTITY) ||
			 (componentMapping.b != VK_COMPONENT_SWIZZLE_IDENTITY) ||
			 (componentMapping.a != VK_COMPONENT_SWIZZLE_IDENTITY)))
		{
			TCU_THROW(NotSupportedError, "VK_KHR_portability_subset: Implementation does not support remapping format components");
		}
	}

	bool formatRgba10x6WithoutYCbCrSampler = context.getRGBA10X6FormatsFeaturesEXT().formatRgba10x6WithoutYCbCrSampler;
#else
	bool formatRgba10x6WithoutYCbCrSampler = VK_FALSE;
#endif // CTS_USES_VULKANSC

	if ((params.imageFormat == VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16) && (params.subresourceRange.levelCount > 1) && (formatRgba10x6WithoutYCbCrSampler == VK_FALSE))
	{
		TCU_THROW(NotSupportedError, "formatRgba10x6WithoutYCbCrSampler not supported");
	}
}

ImageSamplingInstance::ImageSamplingInstance (Context&						context,
											  ImageSamplingInstanceParams	params)
	: vkt::TestInstance				(context)
	, m_allocationKind				(params.allocationKind)
	, m_samplingType				(params.samplingType)
	, m_imageViewType				(params.imageViewType)
	, m_imageFormat					(params.imageFormat)
	, m_imageSize					(params.imageSize)
	, m_layerCount					(params.layerCount)
	, m_imageCount					(params.imageCount)
	, m_componentMapping			(params.componentMapping)
	, m_componentMask				(true)
	, m_subresourceRange			(params.subresourceRange)
	, m_samplerParams				(params.samplerParams)
	, m_samplerLod					(params.samplerLod)
	, m_renderSize					(params.renderSize)
	, m_colorFormat					(VK_FORMAT_R8G8B8A8_UNORM)
	, m_vertices					(params.vertices)
	, m_graphicsPipeline			(context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(), context.getDevice(), m_context.getDeviceExtensions(), params.pipelineConstructionType, params.pipelineCreateFlags)
	, m_pipelineConstructionType	(params.pipelineConstructionType)
	, m_imageLayout					(params.imageLayout)
{
}

void ImageSamplingInstance::setup ()
{
	const InstanceInterface&				vki						= m_context.getInstanceInterface();
	const DeviceInterface&					vk						= m_context.getDeviceInterface();
	const VkPhysicalDevice					physDevice				= m_context.getPhysicalDevice();
	const VkDevice							vkDevice				= m_context.getDevice();
	const VkQueue							queue					= m_context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
	SimpleAllocator							memAlloc				(vk, vkDevice, getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));
	const VkComponentMapping				componentMappingRGBA	= { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };

	void const* pNext = m_samplerParams.pNext;
	while (pNext != DE_NULL)
	{
		const VkStructureType nextType = *reinterpret_cast<const VkStructureType*>(pNext);
		switch (nextType)
		{
			case VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO:
			{
				VkPhysicalDeviceSamplerFilterMinmaxProperties	physicalDeviceSamplerMinMaxProperties =
				{
					VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES,
					DE_NULL,
					DE_FALSE,
					DE_FALSE
				};
				VkPhysicalDeviceProperties2						physicalDeviceProperties;
				physicalDeviceProperties.sType	= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
				physicalDeviceProperties.pNext	= &physicalDeviceSamplerMinMaxProperties;

				vki.getPhysicalDeviceProperties2(m_context.getPhysicalDevice(), &physicalDeviceProperties);

				if (physicalDeviceSamplerMinMaxProperties.filterMinmaxImageComponentMapping != VK_TRUE)
				{
					// If filterMinmaxImageComponentMapping is VK_FALSE the component mapping of the image
					// view used with min/max filtering must have been created with the r component set to
					// VK_COMPONENT_SWIZZLE_IDENTITY. Only the r component of the sampled image value is
					// defined and the other component values are undefined

					m_componentMask = tcu::BVec4(true, false, false, false);

					if (m_componentMapping.r != VK_COMPONENT_SWIZZLE_IDENTITY && m_componentMapping.r != VK_COMPONENT_SWIZZLE_R)
					{
						TCU_THROW(NotSupportedError, "filterMinmaxImageComponentMapping is not supported (R mapping is not IDENTITY)");
					}
				}
				pNext = reinterpret_cast<const VkSamplerReductionModeCreateInfo*>(pNext)->pNext;
			}
			break;
			case VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO:
				pNext = reinterpret_cast<const VkSamplerYcbcrConversionInfo*>(pNext)->pNext;
				break;
			case VK_STRUCTURE_TYPE_SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT:
			{
				const VkSamplerCustomBorderColorCreateInfoEXT customBorderColorCreateInfo = *reinterpret_cast<const VkSamplerCustomBorderColorCreateInfoEXT*>(pNext);

				VkPhysicalDeviceCustomBorderColorFeaturesEXT	physicalDeviceCustomBorderColorFeatures =
				{
					VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT,
					DE_NULL,
					DE_FALSE,
					DE_FALSE
				};
				VkPhysicalDeviceFeatures2						physicalDeviceFeatures;
				physicalDeviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
				physicalDeviceFeatures.pNext = &physicalDeviceCustomBorderColorFeatures;

				vki.getPhysicalDeviceFeatures2(m_context.getPhysicalDevice(), &physicalDeviceFeatures);

				if (physicalDeviceCustomBorderColorFeatures.customBorderColors != VK_TRUE)
				{
					TCU_THROW(NotSupportedError, "customBorderColors are not supported");
				}

				if (physicalDeviceCustomBorderColorFeatures.customBorderColorWithoutFormat != VK_TRUE &&
					customBorderColorCreateInfo.format == VK_FORMAT_UNDEFINED)
				{
					TCU_THROW(NotSupportedError, "customBorderColorWithoutFormat is not supported");
				}

				pNext = reinterpret_cast<const VkSamplerCustomBorderColorCreateInfoEXT*>(pNext)->pNext;
			}
			break;
			default:
				TCU_FAIL("Unrecognized sType in chained sampler create info");
		}
	}

	// Create texture images, views and samplers
	{
		VkImageCreateFlags			imageFlags			= 0u;

		if (m_imageViewType == VK_IMAGE_VIEW_TYPE_CUBE || m_imageViewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)
			imageFlags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

		// Initialize texture data
		if (isCompressedFormat(m_imageFormat))
			m_texture = createTestTexture(mapVkCompressedFormat(m_imageFormat), m_imageViewType, m_imageSize, m_layerCount);
		else
			m_texture = createTestTexture(mapVkFormat(m_imageFormat), m_imageViewType, m_imageSize, m_layerCount);

		const VkImageCreateInfo	imageParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,							// VkStructureType			sType;
			DE_NULL,														// const void*				pNext;
			imageFlags,														// VkImageCreateFlags		flags;
			getCompatibleImageType(m_imageViewType),						// VkImageType				imageType;
			m_imageFormat,													// VkFormat					format;
			{																// VkExtent3D				extent;
				(deUint32)m_imageSize.x(),
				(deUint32)m_imageSize.y(),
				(deUint32)m_imageSize.z()
			},
			(deUint32)m_texture->getNumLevels(),							// deUint32					mipLevels;
			(deUint32)m_layerCount,											// deUint32					arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,											// VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,										// VkImageTiling			tiling;
			VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,	// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,										// VkSharingMode			sharingMode;
			1u,																// deUint32					queueFamilyIndexCount;
			&queueFamilyIndex,												// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED										// VkImageLayout			initialLayout;
		};

		m_images.resize(m_imageCount);
		m_imageAllocs.resize(m_imageCount);
		m_imageViews.resize(m_imageCount);

		for (int imgNdx = 0; imgNdx < m_imageCount; ++imgNdx)
		{
			m_images[imgNdx] = SharedImagePtr(new UniqueImage(createImage(vk, vkDevice, &imageParams)));
			m_imageAllocs[imgNdx] = SharedAllocPtr(new UniqueAlloc(allocateImage(vki, vk, physDevice, vkDevice, **m_images[imgNdx], MemoryRequirement::Any, memAlloc, m_allocationKind)));
			VK_CHECK(vk.bindImageMemory(vkDevice, **m_images[imgNdx], (*m_imageAllocs[imgNdx])->getMemory(), (*m_imageAllocs[imgNdx])->getOffset()));

			// Upload texture data
			uploadTestTexture(vk, vkDevice, queue, queueFamilyIndex, memAlloc, *m_texture, **m_images[imgNdx]);

			// Create image view and sampler
			const VkImageViewCreateInfo imageViewParams =
			{
				VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// VkStructureType			sType;
				DE_NULL,									// const void*				pNext;
				0u,											// VkImageViewCreateFlags	flags;
				**m_images[imgNdx],							// VkImage					image;
				m_imageViewType,							// VkImageViewType			viewType;
				m_imageFormat,								// VkFormat					format;
				m_componentMapping,							// VkComponentMapping		components;
				m_subresourceRange,							// VkImageSubresourceRange	subresourceRange;
			};

			m_imageViews[imgNdx] = SharedImageViewPtr(new UniqueImageView(createImageView(vk, vkDevice, &imageViewParams)));
		}

		m_sampler	= createSampler(vk, vkDevice, &m_samplerParams);
	}

	// Create descriptor set for image and sampler
	{
		DescriptorPoolBuilder descriptorPoolBuilder;
		if (m_samplingType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
			descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_SAMPLER, 1u);
		descriptorPoolBuilder.addType(m_samplingType, m_imageCount);
		m_descriptorPool = descriptorPoolBuilder.build(vk, vkDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
			m_samplingType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ? m_imageCount + 1u : m_imageCount);

		DescriptorSetLayoutBuilder setLayoutBuilder;
		if (m_samplingType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
			setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		setLayoutBuilder.addArrayBinding(m_samplingType, m_imageCount, VK_SHADER_STAGE_FRAGMENT_BIT);
		m_descriptorSetLayout = setLayoutBuilder.build(vk, vkDevice);

		const VkDescriptorSetAllocateInfo descriptorSetAllocateInfo =
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,		// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			*m_descriptorPool,									// VkDescriptorPool				descriptorPool;
			1u,													// deUint32						setLayoutCount;
			&m_descriptorSetLayout.get()						// const VkDescriptorSetLayout*	pSetLayouts;
		};

		m_descriptorSet = allocateDescriptorSet(vk, vkDevice, &descriptorSetAllocateInfo);

		const VkSampler sampler = m_samplingType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ? DE_NULL : *m_sampler;
		std::vector<VkDescriptorImageInfo> descriptorImageInfo(m_imageCount);
		for (int imgNdx = 0; imgNdx < m_imageCount; ++imgNdx)
		{
			descriptorImageInfo[imgNdx].sampler		= sampler;									// VkSampler		sampler;
			descriptorImageInfo[imgNdx].imageView	= **m_imageViews[imgNdx];					// VkImageView		imageView;
			descriptorImageInfo[imgNdx].imageLayout	= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;	// VkImageLayout	imageLayout;
		}

		DescriptorSetUpdateBuilder setUpdateBuilder;
		if (m_samplingType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
		{
			const VkDescriptorImageInfo descriptorSamplerInfo =
			{
				*m_sampler,									// VkSampler		sampler;
				DE_NULL,									// VkImageView		imageView;
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL	// VkImageLayout	imageLayout;
			};
			setUpdateBuilder.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0), VK_DESCRIPTOR_TYPE_SAMPLER, &descriptorSamplerInfo);
		}

		const deUint32 binding = m_samplingType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ? 1u : 0u;
		setUpdateBuilder.writeArray(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(binding), m_samplingType, m_imageCount, descriptorImageInfo.data());
		setUpdateBuilder.update(vk, vkDevice);
	}

	// Create color images and views
	{
		const VkImageCreateInfo colorImageParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,										// VkStructureType			sType;
			DE_NULL,																	// const void*				pNext;
			0u,																			// VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,															// VkImageType				imageType;
			m_colorFormat,																// VkFormat					format;
			{ (deUint32)m_renderSize.x(), (deUint32)m_renderSize.y(), 1u },				// VkExtent3D				extent;
			1u,																			// deUint32					mipLevels;
			1u,																			// deUint32					arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,														// VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,													// VkImageTiling			tiling;
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,		// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,													// VkSharingMode			sharingMode;
			1u,																			// deUint32					queueFamilyIndexCount;
			&queueFamilyIndex,															// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED													// VkImageLayout			initialLayout;
		};

		m_colorImages.resize(m_imageCount);
		m_colorImageAllocs.resize(m_imageCount);
		m_colorAttachmentViews.resize(m_imageCount);

		for (int imgNdx = 0; imgNdx < m_imageCount; ++imgNdx)
		{
			m_colorImages[imgNdx] = SharedImagePtr(new UniqueImage(createImage(vk, vkDevice, &colorImageParams)));
			m_colorImageAllocs[imgNdx] = SharedAllocPtr(new UniqueAlloc(allocateImage(vki, vk, physDevice, vkDevice, **m_colorImages[imgNdx], MemoryRequirement::Any, memAlloc, m_allocationKind)));
			VK_CHECK(vk.bindImageMemory(vkDevice, **m_colorImages[imgNdx], (*m_colorImageAllocs[imgNdx])->getMemory(), (*m_colorImageAllocs[imgNdx])->getOffset()));

			const VkImageViewCreateInfo colorAttachmentViewParams =
			{
				VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,			// VkStructureType			sType;
				DE_NULL,											// const void*				pNext;
				0u,													// VkImageViewCreateFlags	flags;
				**m_colorImages[imgNdx],							// VkImage					image;
				VK_IMAGE_VIEW_TYPE_2D,								// VkImageViewType			viewType;
				m_colorFormat,										// VkFormat					format;
				componentMappingRGBA,								// VkComponentMapping		components;
				{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u }		// VkImageSubresourceRange	subresourceRange;
			};

			m_colorAttachmentViews[imgNdx] = SharedImageViewPtr(new UniqueImageView(createImageView(vk, vkDevice, &colorAttachmentViewParams)));
		}
	}

	// Create render pass
	{
		std::vector<VkAttachmentDescription>	colorAttachmentDescriptions(m_imageCount);
		std::vector<VkAttachmentReference>		colorAttachmentReferences(m_imageCount);

		for (int imgNdx = 0; imgNdx < m_imageCount; ++imgNdx)
		{
			colorAttachmentDescriptions[imgNdx].flags			= 0u;										// VkAttachmentDescriptionFlags		flags;
			colorAttachmentDescriptions[imgNdx].format			= m_colorFormat;							// VkFormat							format;
			colorAttachmentDescriptions[imgNdx].samples			= VK_SAMPLE_COUNT_1_BIT;					// VkSampleCountFlagBits			samples;
			colorAttachmentDescriptions[imgNdx].loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR;				// VkAttachmentLoadOp				loadOp;
			colorAttachmentDescriptions[imgNdx].storeOp			= VK_ATTACHMENT_STORE_OP_STORE;				// VkAttachmentStoreOp				storeOp;
			colorAttachmentDescriptions[imgNdx].stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;			// VkAttachmentLoadOp				stencilLoadOp;
			colorAttachmentDescriptions[imgNdx].stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;			// VkAttachmentStoreOp				stencilStoreOp;
			colorAttachmentDescriptions[imgNdx].initialLayout	= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;	// VkImageLayout					initialLayout;
			colorAttachmentDescriptions[imgNdx].finalLayout		= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;	// VkImageLayout					finalLayout;

			colorAttachmentReferences[imgNdx].attachment		= (deUint32)imgNdx;							// deUint32							attachment;
			colorAttachmentReferences[imgNdx].layout			= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;	// VkImageLayout					layout;
		}

		const VkSubpassDescription subpassDescription =
		{
			0u,													// VkSubpassDescriptionFlags	flags;
			VK_PIPELINE_BIND_POINT_GRAPHICS,					// VkPipelineBindPoint			pipelineBindPoint;
			0u,													// deUint32						inputAttachmentCount;
			DE_NULL,											// const VkAttachmentReference*	pInputAttachments;
			(deUint32)m_imageCount,								// deUint32						colorAttachmentCount;
			&colorAttachmentReferences[0],						// const VkAttachmentReference*	pColorAttachments;
			DE_NULL,											// const VkAttachmentReference*	pResolveAttachments;
			DE_NULL,											// const VkAttachmentReference*	pDepthStencilAttachment;
			0u,													// deUint32						preserveAttachmentCount;
			DE_NULL												// const VkAttachmentReference*	pPreserveAttachments;
		};

		const VkRenderPassCreateInfo renderPassParams =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,			// VkStructureType					sType;
			DE_NULL,											// const void*						pNext;
			0u,													// VkRenderPassCreateFlags			flags;
			(deUint32)m_imageCount,								// deUint32							attachmentCount;
			&colorAttachmentDescriptions[0],					// const VkAttachmentDescription*	pAttachments;
			1u,													// deUint32							subpassCount;
			&subpassDescription,								// const VkSubpassDescription*		pSubpasses;
			0u,													// deUint32							dependencyCount;
			DE_NULL												// const VkSubpassDependency*		pDependencies;
		};

		m_renderPass = RenderPassWrapper(m_pipelineConstructionType, vk, vkDevice, &renderPassParams);
	}

	// Create framebuffer
	{
		std::vector<VkImage>		images			(m_imageCount);
		std::vector<VkImageView>	pAttachments	(m_imageCount);
		for (int imgNdx = 0; imgNdx < m_imageCount; ++imgNdx)
		{
			images[imgNdx] = m_colorImages[imgNdx]->get();
			pAttachments[imgNdx] = m_colorAttachmentViews[imgNdx]->get();
		}

		const VkFramebufferCreateInfo framebufferParams =
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,			// VkStructureType			sType;
			DE_NULL,											// const void*				pNext;
			0u,													// VkFramebufferCreateFlags	flags;
			*m_renderPass,										// VkRenderPass				renderPass;
			(deUint32)m_imageCount,								// deUint32					attachmentCount;
			&pAttachments[0],									// const VkImageView*		pAttachments;
			(deUint32)m_renderSize.x(),							// deUint32					width;
			(deUint32)m_renderSize.y(),							// deUint32					height;
			1u													// deUint32					layers;
		};

		m_renderPass.createFramebuffer(vk, vkDevice, &framebufferParams, images);
	}

	// Create pipeline layout
	{
#ifndef CTS_USES_VULKANSC
		VkPipelineLayoutCreateFlags pipelineLayoutFlags = (!isConstructionTypeLibrary(m_pipelineConstructionType)) ? 0u : deUint32(VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT);
#else
		VkPipelineLayoutCreateFlags pipelineLayoutFlags = 0u;
#endif // CTS_USES_VULKANSC
		VkPipelineLayoutCreateInfo	pipelineLayoutParams
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			pipelineLayoutFlags,								// VkPipelineLayoutCreateFlags	flags;
			0u,													// deUint32						setLayoutCount;
			DE_NULL,											// const VkDescriptorSetLayout*	pSetLayouts;
			0u,													// deUint32						pushConstantRangeCount;
			DE_NULL												// const VkPushConstantRange*	pPushConstantRanges;
		};

		m_preRasterizationStatePipelineLayout	= PipelineLayoutWrapper(m_pipelineConstructionType, vk, vkDevice, &pipelineLayoutParams);
		pipelineLayoutParams.setLayoutCount		= 1u;
		pipelineLayoutParams.pSetLayouts		= &m_descriptorSetLayout.get();
		m_fragmentStatePipelineLayout			= PipelineLayoutWrapper(m_pipelineConstructionType, vk, vkDevice, &pipelineLayoutParams);
	}

	m_vertexShaderModule	= ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("tex_vert"), 0);
	m_fragmentShaderModule	= ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("tex_frag"), 0);

	// Create pipeline
	{
		const VkVertexInputBindingDescription vertexInputBindingDescription =
		{
			0u,									// deUint32					binding;
			sizeof(Vertex4Tex4),				// deUint32					strideInBytes;
			VK_VERTEX_INPUT_RATE_VERTEX			// VkVertexInputStepRate	inputRate;
		};

		const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[2] =
		{
			{
				0u,										// deUint32	location;
				0u,										// deUint32	binding;
				VK_FORMAT_R32G32B32A32_SFLOAT,			// VkFormat	format;
				0u										// deUint32	offset;
			},
			{
				1u,										// deUint32	location;
				0u,										// deUint32	binding;
				VK_FORMAT_R32G32B32A32_SFLOAT,			// VkFormat	format;
				DE_OFFSET_OF(Vertex4Tex4, texCoord),	// deUint32	offset;
			}
		};

		const VkPipelineVertexInputStateCreateInfo vertexInputStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			0u,																// VkPipelineVertexInputStateCreateFlags	flags;
			1u,																// deUint32									vertexBindingDescriptionCount;
			&vertexInputBindingDescription,									// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
			2u,																// deUint32									vertexAttributeDescriptionCount;
			vertexInputAttributeDescriptions								// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
		};

		const std::vector<VkViewport>	viewports	{ makeViewport(m_renderSize) };
		const std::vector<VkRect2D>		scissors	{ makeRect2D(m_renderSize) };

		std::vector<VkPipelineColorBlendAttachmentState>	colorBlendAttachmentStates(m_imageCount);

		for (int imgNdx = 0; imgNdx < m_imageCount; ++imgNdx)
		{
			colorBlendAttachmentStates[imgNdx].blendEnable			= false;												// VkBool32					blendEnable;
			colorBlendAttachmentStates[imgNdx].srcColorBlendFactor	= VK_BLEND_FACTOR_ONE;									// VkBlendFactor			srcColorBlendFactor;
			colorBlendAttachmentStates[imgNdx].dstColorBlendFactor	= VK_BLEND_FACTOR_ZERO;									// VkBlendFactor			dstColorBlendFactor;
			colorBlendAttachmentStates[imgNdx].colorBlendOp			= VK_BLEND_OP_ADD;										// VkBlendOp				colorBlendOp;
			colorBlendAttachmentStates[imgNdx].srcAlphaBlendFactor	= VK_BLEND_FACTOR_ONE;									// VkBlendFactor			srcAlphaBlendFactor;
			colorBlendAttachmentStates[imgNdx].dstAlphaBlendFactor	= VK_BLEND_FACTOR_ZERO;									// VkBlendFactor			dstAlphaBlendFactor;
			colorBlendAttachmentStates[imgNdx].alphaBlendOp			= VK_BLEND_OP_ADD;										// VkBlendOp				alphaBlendOp;
			colorBlendAttachmentStates[imgNdx].colorWriteMask		= VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |	// VkColorComponentFlags	colorWriteMask;
																		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		}

		const VkPipelineColorBlendStateCreateInfo colorBlendStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType								sType;
			DE_NULL,													// const void*									pNext;
			0u,															// VkPipelineColorBlendStateCreateFlags			flags;
			false,														// VkBool32										logicOpEnable;
			VK_LOGIC_OP_COPY,											// VkLogicOp									logicOp;
			(deUint32)m_imageCount,										// deUint32										attachmentCount;
			&colorBlendAttachmentStates[0],								// const VkPipelineColorBlendAttachmentState*	pAttachments;
			{ 0.0f, 0.0f, 0.0f, 0.0f }									// float										blendConstants[4];
		};

		m_graphicsPipeline.setMonolithicPipelineLayout(m_fragmentStatePipelineLayout)
						  .setDefaultDepthStencilState()
						  .setDefaultRasterizationState()
						  .setDefaultMultisampleState()
						  .setupVertexInputState(&vertexInputStateParams)
						  .setupPreRasterizationShaderState(viewports,
														scissors,
														m_preRasterizationStatePipelineLayout,
														*m_renderPass,
														0u,
														m_vertexShaderModule)
						  .setupFragmentShaderState(m_fragmentStatePipelineLayout, *m_renderPass, 0u, m_fragmentShaderModule)
						  .setupFragmentOutputState(*m_renderPass, 0u, &colorBlendStateParams)
						  .buildPipeline();
	}

	// Create vertex buffer
	{
		const VkDeviceSize			vertexBufferSize	= (VkDeviceSize)(m_vertices.size() * sizeof(Vertex4Tex4));
		const VkBufferCreateInfo	vertexBufferParams	=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			0u,											// VkBufferCreateFlags	flags;
			vertexBufferSize,							// VkDeviceSize			size;
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,			// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
			1u,											// deUint32				queueFamilyIndexCount;
			&queueFamilyIndex							// const deUint32*		pQueueFamilyIndices;
		};

		DE_ASSERT(vertexBufferSize > 0);

		m_vertexBuffer		= createBuffer(vk, vkDevice, &vertexBufferParams);
		m_vertexBufferAlloc = allocateBuffer(vki, vk, physDevice, vkDevice, *m_vertexBuffer, MemoryRequirement::HostVisible, memAlloc, m_allocationKind);
		VK_CHECK(vk.bindBufferMemory(vkDevice, *m_vertexBuffer, m_vertexBufferAlloc->getMemory(), m_vertexBufferAlloc->getOffset()));

		// Load vertices into vertex buffer
		deMemcpy(m_vertexBufferAlloc->getHostPtr(), &m_vertices[0], (size_t)vertexBufferSize);
		flushAlloc(vk, vkDevice, *m_vertexBufferAlloc);
	}

	// Create command pool
	m_cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);

	// Create command buffer
	{
		const std::vector<VkClearValue> attachmentClearValues (m_imageCount, defaultClearValue(m_colorFormat));

		std::vector<VkImageMemoryBarrier> preAttachmentBarriers(m_imageCount);

		for (int imgNdx = 0; imgNdx < m_imageCount; ++imgNdx)
		{
			preAttachmentBarriers[imgNdx].sType								= VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;	// VkStructureType			sType;
			preAttachmentBarriers[imgNdx].pNext								= DE_NULL;									// const void*				pNext;
			preAttachmentBarriers[imgNdx].srcAccessMask						= 0u;										// VkAccessFlags			srcAccessMask;
			preAttachmentBarriers[imgNdx].dstAccessMask						= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;		// VkAccessFlags			dstAccessMask;
			preAttachmentBarriers[imgNdx].oldLayout							= VK_IMAGE_LAYOUT_UNDEFINED;				// VkImageLayout			oldLayout;
			preAttachmentBarriers[imgNdx].newLayout							= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;	// VkImageLayout			newLayout;
			preAttachmentBarriers[imgNdx].srcQueueFamilyIndex				= VK_QUEUE_FAMILY_IGNORED;					// deUint32					srcQueueFamilyIndex;
			preAttachmentBarriers[imgNdx].dstQueueFamilyIndex				= VK_QUEUE_FAMILY_IGNORED;					// deUint32					dstQueueFamilyIndex;
			preAttachmentBarriers[imgNdx].image								= **m_colorImages[imgNdx];					// VkImage					image;
			preAttachmentBarriers[imgNdx].subresourceRange.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT;				// VkImageSubresourceRange	subresourceRange;
			preAttachmentBarriers[imgNdx].subresourceRange.baseMipLevel		= 0u;
			preAttachmentBarriers[imgNdx].subresourceRange.levelCount		= 1u;
			preAttachmentBarriers[imgNdx].subresourceRange.baseArrayLayer	= 0u;
			preAttachmentBarriers[imgNdx].subresourceRange.layerCount		= 1u;
		}

		m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

		beginCommandBuffer(vk, *m_cmdBuffer, 0u);

		vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, (VkDependencyFlags)0,
			0u, DE_NULL, 0u, DE_NULL, (deUint32)m_imageCount, &preAttachmentBarriers[0]);

		m_renderPass.begin(vk, *m_cmdBuffer, makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y()), (deUint32)attachmentClearValues.size(), &attachmentClearValues[0]);

		m_graphicsPipeline.bind(*m_cmdBuffer);

		m_fragmentStatePipelineLayout.bindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, 1, &m_descriptorSet.get(), 0, DE_NULL);

		const VkDeviceSize vertexBufferOffset = 0;
		vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);
		vk.cmdDraw(*m_cmdBuffer, (deUint32)m_vertices.size(), 1, 0, 0);

		m_renderPass.end(vk, *m_cmdBuffer);
		endCommandBuffer(vk, *m_cmdBuffer);
	}
}

ImageSamplingInstance::~ImageSamplingInstance (void)
{
}

tcu::TestStatus ImageSamplingInstance::iterate (void)
{
	const DeviceInterface&		vk			= m_context.getDeviceInterface();
	const VkDevice				vkDevice	= m_context.getDevice();
	const VkQueue				queue		= m_context.getUniversalQueue();

	setup();

	submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());

	return verifyImage();
}

namespace
{

bool isLookupResultValid (const tcu::Texture1DView&		texture,
						  const tcu::Sampler&			sampler,
						  const tcu::LookupPrecision&	precision,
						  const tcu::Vec4&				coords,
						  const tcu::Vec2&				lodBounds,
						  const tcu::Vec4&				result)
{
	return tcu::isLookupResultValid(texture, sampler, precision, coords.x(), lodBounds, result);
}

bool isLookupResultValid (const tcu::Texture1DArrayView&	texture,
						  const tcu::Sampler&				sampler,
						  const tcu::LookupPrecision&		precision,
						  const tcu::Vec4&					coords,
						  const tcu::Vec2&					lodBounds,
						  const tcu::Vec4&					result)
{
	return tcu::isLookupResultValid(texture, sampler, precision, coords.swizzle(0,1), lodBounds, result);
}

bool isLookupResultValid (const tcu::Texture2DView&		texture,
						  const tcu::Sampler&			sampler,
						  const tcu::LookupPrecision&	precision,
						  const tcu::Vec4&				coords,
						  const tcu::Vec2&				lodBounds,
						  const tcu::Vec4&				result)
{
	return tcu::isLookupResultValid(texture, sampler, precision, coords.swizzle(0,1), lodBounds, result);
}

bool isLookupResultValid (const tcu::Texture2DArrayView&	texture,
						  const tcu::Sampler&				sampler,
						  const tcu::LookupPrecision&		precision,
						  const tcu::Vec4&					coords,
						  const tcu::Vec2&					lodBounds,
						  const tcu::Vec4&					result)
{
	return tcu::isLookupResultValid(texture, sampler, precision, coords.swizzle(0,1,2), lodBounds, result);
}

bool isLookupResultValid (const tcu::TextureCubeView&	texture,
						  const tcu::Sampler&			sampler,
						  const tcu::LookupPrecision&	precision,
						  const tcu::Vec4&				coords,
						  const tcu::Vec2&				lodBounds,
						  const tcu::Vec4&				result)
{
	return tcu::isLookupResultValid(texture, sampler, precision, coords.swizzle(0,1,2), lodBounds, result);
}

bool isLookupResultValid (const tcu::TextureCubeArrayView&	texture,
						  const tcu::Sampler&				sampler,
						  const tcu::LookupPrecision&		precision,
						  const tcu::Vec4&					coords,
						  const tcu::Vec2&					lodBounds,
						  const tcu::Vec4&					result)
{
	return tcu::isLookupResultValid(texture, sampler, precision, tcu::IVec4(precision.coordBits.x()), coords, lodBounds, result);
}

bool isLookupResultValid(const tcu::Texture3DView&		texture,
						 const tcu::Sampler&			sampler,
						 const tcu::LookupPrecision&	precision,
						 const tcu::Vec4&				coords,
						 const tcu::Vec2&				lodBounds,
						 const tcu::Vec4&				result)
{
	return tcu::isLookupResultValid(texture, sampler, precision, coords.swizzle(0,1,2), lodBounds, result);
}

template<typename TextureViewType>
bool validateResultImage (const TextureViewType&				texture,
						  const tcu::Sampler&					sampler,
						  const tcu::ConstPixelBufferAccess&	texCoords,
						  const tcu::Vec2&						lodBounds,
						  const tcu::LookupPrecision&			lookupPrecision,
						  const tcu::Vec4&						lookupScale,
						  const tcu::Vec4&						lookupBias,
						  const tcu::ConstPixelBufferAccess&	result,
						  const tcu::PixelBufferAccess&			errorMask)
{
	const int	w		= result.getWidth();
	const int	h		= result.getHeight();
	bool		allOk	= true;

	for (int y = 0; y < h; ++y)
	{
		for (int x = 0; x < w; ++x)
		{
			const tcu::Vec4		resultPixel	= result.getPixel(x, y);
			const tcu::Vec4		resultColor	= (resultPixel - lookupBias) / lookupScale;
			const tcu::Vec4		texCoord	= texCoords.getPixel(x, y);
			const bool			pixelOk		= isLookupResultValid(texture, sampler, lookupPrecision, texCoord, lodBounds, resultColor);

			errorMask.setPixel(tcu::Vec4(pixelOk?0.0f:1.0f, pixelOk?1.0f:0.0f, 0.0f, 1.0f), x, y);

			if (!pixelOk)
				allOk = false;
		}
	}

	return allOk;
}

template<typename ScalarType>
ScalarType getSwizzledComp (const tcu::Vector<ScalarType, 4>& vec, vk::VkComponentSwizzle comp, int identityNdx)
{
	if (comp == vk::VK_COMPONENT_SWIZZLE_IDENTITY)
		return vec[identityNdx];
	else if (comp == vk::VK_COMPONENT_SWIZZLE_ZERO)
		return ScalarType(0);
	else if (comp == vk::VK_COMPONENT_SWIZZLE_ONE)
		return ScalarType(1);
	else
		return vec[comp - vk::VK_COMPONENT_SWIZZLE_R];
}

template<typename ScalarType>
tcu::Vector<ScalarType, 4> swizzle (const tcu::Vector<ScalarType, 4>& vec, const vk::VkComponentMapping& swz)
{
	return tcu::Vector<ScalarType, 4>(getSwizzledComp(vec, swz.r, 0),
									  getSwizzledComp(vec, swz.g, 1),
									  getSwizzledComp(vec, swz.b, 2),
									  getSwizzledComp(vec, swz.a, 3));
}

/*--------------------------------------------------------------------*//*!
* \brief Swizzle scale or bias vector by given mapping
*
* \param vec scale or bias vector
* \param swz swizzle component mapping, may include ZERO, ONE, or IDENTITY
* \param zeroOrOneValue vector value for component swizzled as ZERO or ONE
* \return swizzled vector
*//*--------------------------------------------------------------------*/
tcu::Vec4 swizzleScaleBias (const tcu::Vec4& vec, const vk::VkComponentMapping& swz, float zeroOrOneValue)
{

	// Remove VK_COMPONENT_SWIZZLE_IDENTITY to avoid addressing channelValues[0]
	const vk::VkComponentMapping nonIdentitySwz =
	{
		swz.r == VK_COMPONENT_SWIZZLE_IDENTITY ? VK_COMPONENT_SWIZZLE_R : swz.r,
		swz.g == VK_COMPONENT_SWIZZLE_IDENTITY ? VK_COMPONENT_SWIZZLE_G : swz.g,
		swz.b == VK_COMPONENT_SWIZZLE_IDENTITY ? VK_COMPONENT_SWIZZLE_B : swz.b,
		swz.a == VK_COMPONENT_SWIZZLE_IDENTITY ? VK_COMPONENT_SWIZZLE_A : swz.a
	};

	const float channelValues[] =
	{
		-1.0f,				// impossible
		zeroOrOneValue,		// SWIZZLE_ZERO
		zeroOrOneValue,		// SWIZZLE_ONE
		vec.x(),
		vec.y(),
		vec.z(),
		vec.w(),
	};

	return tcu::Vec4(channelValues[nonIdentitySwz.r], channelValues[nonIdentitySwz.g], channelValues[nonIdentitySwz.b], channelValues[nonIdentitySwz.a]);
}

template<typename ScalarType>
void swizzleT (const tcu::ConstPixelBufferAccess& src, const tcu::PixelBufferAccess& dst, const vk::VkComponentMapping& swz)
{
	for (int z = 0; z < dst.getDepth(); ++z)
	for (int y = 0; y < dst.getHeight(); ++y)
	for (int x = 0; x < dst.getWidth(); ++x)
		dst.setPixel(swizzle(src.getPixelT<ScalarType>(x, y, z), swz), x, y, z);
}

void swizzleFromSRGB (const tcu::ConstPixelBufferAccess& src, const tcu::PixelBufferAccess& dst, const vk::VkComponentMapping& swz)
{
	for (int z = 0; z < dst.getDepth(); ++z)
	for (int y = 0; y < dst.getHeight(); ++y)
	for (int x = 0; x < dst.getWidth(); ++x)
		dst.setPixel(swizzle(tcu::sRGBToLinear(src.getPixelT<float>(x, y, z)), swz), x, y, z);
}

void swizzle (const tcu::ConstPixelBufferAccess& src, const tcu::PixelBufferAccess& dst, const vk::VkComponentMapping& swz)
{
	const tcu::TextureChannelClass	chnClass	= tcu::getTextureChannelClass(dst.getFormat().type);

	DE_ASSERT(src.getWidth() == dst.getWidth() &&
			  src.getHeight() == dst.getHeight() &&
			  src.getDepth() == dst.getDepth());

	if (chnClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER)
		swizzleT<deInt32>(src, dst, swz);
	else if (chnClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER)
		swizzleT<deUint32>(src, dst, swz);
	else if (tcu::isSRGB(src.getFormat()) && !tcu::isSRGB(dst.getFormat()))
		swizzleFromSRGB(src, dst, swz);
	else
		swizzleT<float>(src, dst, swz);
}

bool isIdentitySwizzle (const vk::VkComponentMapping& swz)
{
	return (swz.r == vk::VK_COMPONENT_SWIZZLE_IDENTITY || swz.r == vk::VK_COMPONENT_SWIZZLE_R) &&
		   (swz.g == vk::VK_COMPONENT_SWIZZLE_IDENTITY || swz.g == vk::VK_COMPONENT_SWIZZLE_G) &&
		   (swz.b == vk::VK_COMPONENT_SWIZZLE_IDENTITY || swz.b == vk::VK_COMPONENT_SWIZZLE_B) &&
		   (swz.a == vk::VK_COMPONENT_SWIZZLE_IDENTITY || swz.a == vk::VK_COMPONENT_SWIZZLE_A);
}

template<typename TextureViewType> struct TexViewTraits;

template<> struct TexViewTraits<tcu::Texture1DView>			{ typedef tcu::Texture1D		TextureType; };
template<> struct TexViewTraits<tcu::Texture1DArrayView>	{ typedef tcu::Texture1DArray	TextureType; };
template<> struct TexViewTraits<tcu::Texture2DView>			{ typedef tcu::Texture2D		TextureType; };
template<> struct TexViewTraits<tcu::Texture2DArrayView>	{ typedef tcu::Texture2DArray	TextureType; };
template<> struct TexViewTraits<tcu::TextureCubeView>		{ typedef tcu::TextureCube		TextureType; };
template<> struct TexViewTraits<tcu::TextureCubeArrayView>	{ typedef tcu::TextureCubeArray	TextureType; };
template<> struct TexViewTraits<tcu::Texture3DView>			{ typedef tcu::Texture3D		TextureType; };

template<typename TextureViewType>
typename TexViewTraits<TextureViewType>::TextureType* createSkeletonClone (tcu::TextureFormat format, const tcu::ConstPixelBufferAccess& level0);

tcu::TextureFormat getSwizzleTargetFormat (tcu::TextureFormat format)
{
	// Swizzled texture needs to hold all four channels
	// \todo [2016-09-21 pyry] We could save some memory by using smaller formats
	//						   when possible (for example U8).

	const tcu::TextureChannelClass	chnClass	= tcu::getTextureChannelClass(format.type);

	if (chnClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER)
		return tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::SIGNED_INT32);
	else if (chnClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER)
		return tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNSIGNED_INT32);
	else
		return tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::FLOAT);
}

template<>
tcu::Texture1D* createSkeletonClone<tcu::Texture1DView> (tcu::TextureFormat format, const tcu::ConstPixelBufferAccess& level0)
{
	return new tcu::Texture1D(format, level0.getWidth());
}

template<>
tcu::Texture1DArray* createSkeletonClone<tcu::Texture1DArrayView> (tcu::TextureFormat format, const tcu::ConstPixelBufferAccess& level0)
{
	return new tcu::Texture1DArray(format, level0.getWidth(), level0.getHeight());
}

template<>
tcu::Texture2D* createSkeletonClone<tcu::Texture2DView> (tcu::TextureFormat format, const tcu::ConstPixelBufferAccess& level0)
{
	return new tcu::Texture2D(format, level0.getWidth(), level0.getHeight());
}

template<>
tcu::Texture2DArray* createSkeletonClone<tcu::Texture2DArrayView> (tcu::TextureFormat format, const tcu::ConstPixelBufferAccess& level0)
{
	return new tcu::Texture2DArray(format, level0.getWidth(), level0.getHeight(), level0.getDepth());
}

template<>
tcu::Texture3D* createSkeletonClone<tcu::Texture3DView> (tcu::TextureFormat format, const tcu::ConstPixelBufferAccess& level0)
{
	return new tcu::Texture3D(format, level0.getWidth(), level0.getHeight(), level0.getDepth());
}

template<>
tcu::TextureCubeArray* createSkeletonClone<tcu::TextureCubeArrayView> (tcu::TextureFormat format, const tcu::ConstPixelBufferAccess& level0)
{
	return new tcu::TextureCubeArray(format, level0.getWidth(), level0.getDepth());
}

template<typename TextureViewType>
MovePtr<typename TexViewTraits<TextureViewType>::TextureType> createSwizzledCopy (const TextureViewType& texture, const vk::VkComponentMapping& swz)
{
	MovePtr<typename TexViewTraits<TextureViewType>::TextureType>	copy	(createSkeletonClone<TextureViewType>(getSwizzleTargetFormat(texture.getLevel(0).getFormat()), texture.getLevel(0)));

	for (int levelNdx = 0; levelNdx < texture.getNumLevels(); ++levelNdx)
	{
		copy->allocLevel(levelNdx);
		swizzle(texture.getLevel(levelNdx), copy->getLevel(levelNdx), swz);
	}

	return copy;
}

template<>
MovePtr<tcu::TextureCube> createSwizzledCopy (const tcu::TextureCubeView& texture, const vk::VkComponentMapping& swz)
{
	MovePtr<tcu::TextureCube>	copy	(new tcu::TextureCube(getSwizzleTargetFormat(texture.getLevelFace(0, tcu::CUBEFACE_NEGATIVE_X).getFormat()), texture.getSize()));

	for (int faceNdx = 0; faceNdx < tcu::CUBEFACE_LAST; ++faceNdx)
	{
		for (int levelNdx = 0; levelNdx < texture.getNumLevels(); ++levelNdx)
		{
			copy->allocLevel((tcu::CubeFace)faceNdx, levelNdx);
			swizzle(texture.getLevelFace(levelNdx, (tcu::CubeFace)faceNdx), copy->getLevelFace(levelNdx, (tcu::CubeFace)faceNdx), swz);
		}
	}

	return copy;
}

template<typename TextureViewType>
bool validateResultImage (const TextureViewType&				texture,
						  const tcu::Sampler&					sampler,
						  const vk::VkComponentMapping&			swz,
						  const tcu::ConstPixelBufferAccess&	texCoords,
						  const tcu::Vec2&						lodBounds,
						  const tcu::LookupPrecision&			lookupPrecision,
						  const tcu::Vec4&						lookupScale,
						  const tcu::Vec4&						lookupBias,
						  const tcu::ConstPixelBufferAccess&	result,
						  const tcu::PixelBufferAccess&			errorMask)
{
	if (isIdentitySwizzle(swz))
		return validateResultImage(texture, sampler, texCoords, lodBounds, lookupPrecision, lookupScale, lookupBias, result, errorMask);
	else
	{
		// There is (currently) no way to handle swizzling inside validation loop
		// and thus we need to pre-swizzle the texture.
		UniquePtr<typename TexViewTraits<TextureViewType>::TextureType>	swizzledTex	(createSwizzledCopy(texture, swz));

		return validateResultImage(*swizzledTex, sampler, texCoords, lodBounds, lookupPrecision, swizzleScaleBias(lookupScale, swz, 1.0f), swizzleScaleBias(lookupBias, swz, 0.0f), result, errorMask);
	}
}

vk::VkImageSubresourceRange resolveSubresourceRange (const TestTexture& testTexture, const vk::VkImageSubresourceRange& subresource)
{
	vk::VkImageSubresourceRange	resolved					= subresource;

	if (subresource.levelCount == VK_REMAINING_MIP_LEVELS)
		resolved.levelCount = testTexture.getNumLevels()-subresource.baseMipLevel;

	if (subresource.layerCount == VK_REMAINING_ARRAY_LAYERS)
		resolved.layerCount = testTexture.getArraySize()-subresource.baseArrayLayer;

	return resolved;
}

MovePtr<tcu::Texture1DView> getTexture1DView (const TestTexture& testTexture, const vk::VkImageSubresourceRange& subresource, std::vector<tcu::ConstPixelBufferAccess>& levels)
{
	DE_ASSERT(subresource.layerCount == 1);

	levels.resize(subresource.levelCount);

	for (int levelNdx = 0; levelNdx < (int)levels.size(); ++levelNdx)
	{
		const tcu::ConstPixelBufferAccess& srcLevel = testTexture.getLevel((int)subresource.baseMipLevel+levelNdx, subresource.baseArrayLayer);

		levels[levelNdx] = tcu::getSubregion(srcLevel, 0, 0, 0, srcLevel.getWidth(), 1, 1);
	}

	return MovePtr<tcu::Texture1DView>(new tcu::Texture1DView((int)levels.size(), &levels[0]));
}

MovePtr<tcu::Texture1DArrayView> getTexture1DArrayView (const TestTexture& testTexture, const vk::VkImageSubresourceRange& subresource, std::vector<tcu::ConstPixelBufferAccess>& levels)
{
	const TestTexture1D*		tex1D		= dynamic_cast<const TestTexture1D*>(&testTexture);
	const TestTexture1DArray*	tex1DArray	= dynamic_cast<const TestTexture1DArray*>(&testTexture);

	DE_ASSERT(!!tex1D != !!tex1DArray);
	DE_ASSERT(tex1DArray || subresource.baseArrayLayer == 0);

	levels.resize(subresource.levelCount);

	for (int levelNdx = 0; levelNdx < (int)levels.size(); ++levelNdx)
	{
		const tcu::ConstPixelBufferAccess& srcLevel = tex1D ? tex1D->getTexture().getLevel((int)subresource.baseMipLevel+levelNdx)
															: tex1DArray->getTexture().getLevel((int)subresource.baseMipLevel+levelNdx);

		levels[levelNdx] = tcu::getSubregion(srcLevel, 0, (int)subresource.baseArrayLayer, 0, srcLevel.getWidth(), (int)subresource.layerCount, 1);
	}

	return MovePtr<tcu::Texture1DArrayView>(new tcu::Texture1DArrayView((int)levels.size(), &levels[0]));
}

MovePtr<tcu::Texture2DView> getTexture2DView (const TestTexture& testTexture, const vk::VkImageSubresourceRange& subresource, std::vector<tcu::ConstPixelBufferAccess>& levels)
{
	const TestTexture2D*		tex2D		= dynamic_cast<const TestTexture2D*>(&testTexture);
	const TestTexture2DArray*	tex2DArray	= dynamic_cast<const TestTexture2DArray*>(&testTexture);

	DE_ASSERT(subresource.layerCount == 1);
	DE_ASSERT(!!tex2D != !!tex2DArray);
	DE_ASSERT(tex2DArray || subresource.baseArrayLayer == 0);

	levels.resize(subresource.levelCount);

	for (int levelNdx = 0; levelNdx < (int)levels.size(); ++levelNdx)
	{
		const tcu::ConstPixelBufferAccess& srcLevel = tex2D ? tex2D->getTexture().getLevel((int)subresource.baseMipLevel+levelNdx)
															: tex2DArray->getTexture().getLevel((int)subresource.baseMipLevel+levelNdx);

		levels[levelNdx] = tcu::getSubregion(srcLevel, 0, 0, (int)subresource.baseArrayLayer, srcLevel.getWidth(), srcLevel.getHeight(), 1);
	}

	return MovePtr<tcu::Texture2DView>(new tcu::Texture2DView((int)levels.size(), &levels[0]));
}

MovePtr<tcu::Texture2DArrayView> getTexture2DArrayView (const TestTexture& testTexture, const vk::VkImageSubresourceRange& subresource, std::vector<tcu::ConstPixelBufferAccess>& levels)
{
	const TestTexture2D*		tex2D		= dynamic_cast<const TestTexture2D*>(&testTexture);
	const TestTexture2DArray*	tex2DArray	= dynamic_cast<const TestTexture2DArray*>(&testTexture);

	DE_ASSERT(!!tex2D != !!tex2DArray);
	DE_ASSERT(tex2DArray || subresource.baseArrayLayer == 0);

	levels.resize(subresource.levelCount);

	for (int levelNdx = 0; levelNdx < (int)levels.size(); ++levelNdx)
	{
		const tcu::ConstPixelBufferAccess& srcLevel = tex2D ? tex2D->getTexture().getLevel((int)subresource.baseMipLevel+levelNdx)
															: tex2DArray->getTexture().getLevel((int)subresource.baseMipLevel+levelNdx);

		levels[levelNdx] = tcu::getSubregion(srcLevel, 0, 0, (int)subresource.baseArrayLayer, srcLevel.getWidth(), srcLevel.getHeight(), (int)subresource.layerCount);
	}

	return MovePtr<tcu::Texture2DArrayView>(new tcu::Texture2DArrayView((int)levels.size(), &levels[0]));
}

MovePtr<tcu::TextureCubeView> getTextureCubeView (const TestTexture& testTexture, const vk::VkImageSubresourceRange& subresource, std::vector<tcu::ConstPixelBufferAccess>& levels)
{
	const static tcu::CubeFace s_faceMap[tcu::CUBEFACE_LAST] =
	{
		tcu::CUBEFACE_POSITIVE_X,
		tcu::CUBEFACE_NEGATIVE_X,
		tcu::CUBEFACE_POSITIVE_Y,
		tcu::CUBEFACE_NEGATIVE_Y,
		tcu::CUBEFACE_POSITIVE_Z,
		tcu::CUBEFACE_NEGATIVE_Z
	};

	const TestTextureCube*		texCube			= dynamic_cast<const TestTextureCube*>(&testTexture);
	const TestTextureCubeArray*	texCubeArray	= dynamic_cast<const TestTextureCubeArray*>(&testTexture);

	DE_ASSERT(!!texCube != !!texCubeArray);
	DE_ASSERT(subresource.layerCount == 6);
	DE_ASSERT(texCubeArray || subresource.baseArrayLayer == 0);

	levels.resize(subresource.levelCount*tcu::CUBEFACE_LAST);

	for (int faceNdx = 0; faceNdx < tcu::CUBEFACE_LAST; ++faceNdx)
	{
		for (int levelNdx = 0; levelNdx < (int)subresource.levelCount; ++levelNdx)
		{
			const tcu::ConstPixelBufferAccess& srcLevel = texCubeArray ? texCubeArray->getTexture().getLevel((int)subresource.baseMipLevel+levelNdx)
																	   : texCube->getTexture().getLevelFace(levelNdx, s_faceMap[faceNdx]);

			levels[faceNdx*subresource.levelCount + levelNdx] = tcu::getSubregion(srcLevel, 0, 0, (int)subresource.baseArrayLayer + (texCubeArray ? faceNdx : 0), srcLevel.getWidth(), srcLevel.getHeight(), 1);
		}
	}

	{
		const tcu::ConstPixelBufferAccess*	reordered[tcu::CUBEFACE_LAST];

		for (int faceNdx = 0; faceNdx < tcu::CUBEFACE_LAST; ++faceNdx)
			reordered[s_faceMap[faceNdx]] = &levels[faceNdx*subresource.levelCount];

		return MovePtr<tcu::TextureCubeView>(new tcu::TextureCubeView((int)subresource.levelCount, reordered));
	}
}

MovePtr<tcu::TextureCubeArrayView> getTextureCubeArrayView (const TestTexture& testTexture, const vk::VkImageSubresourceRange& subresource, std::vector<tcu::ConstPixelBufferAccess>& levels)
{
	const TestTextureCubeArray*	texCubeArray	= dynamic_cast<const TestTextureCubeArray*>(&testTexture);

	DE_ASSERT(texCubeArray);
	DE_ASSERT(subresource.layerCount%6 == 0);

	levels.resize(subresource.levelCount);

	for (int levelNdx = 0; levelNdx < (int)subresource.levelCount; ++levelNdx)
	{
		const tcu::ConstPixelBufferAccess& srcLevel = texCubeArray->getTexture().getLevel((int)subresource.baseMipLevel+levelNdx);

		levels[levelNdx] = tcu::getSubregion(srcLevel, 0, 0, (int)subresource.baseArrayLayer, srcLevel.getWidth(), srcLevel.getHeight(), (int)subresource.layerCount);
	}

	return MovePtr<tcu::TextureCubeArrayView>(new tcu::TextureCubeArrayView((int)levels.size(), &levels[0]));
}

MovePtr<tcu::Texture3DView> getTexture3DView (const TestTexture& testTexture, const vk::VkImageSubresourceRange& subresource, std::vector<tcu::ConstPixelBufferAccess>& levels)
{
	DE_ASSERT(subresource.baseArrayLayer == 0 && subresource.layerCount == 1);

	levels.resize(subresource.levelCount);

	for (int levelNdx = 0; levelNdx < (int)levels.size(); ++levelNdx)
		levels[levelNdx] = testTexture.getLevel((int)subresource.baseMipLevel+levelNdx, subresource.baseArrayLayer);

	return MovePtr<tcu::Texture3DView>(new tcu::Texture3DView((int)levels.size(), &levels[0]));
}

bool validateResultImage (const TestTexture&					texture,
						  const VkImageViewType					imageViewType,
						  const VkImageSubresourceRange&		subresource,
						  const tcu::Sampler&					sampler,
						  const vk::VkComponentMapping&			componentMapping,
						  const tcu::ConstPixelBufferAccess&	coordAccess,
						  const tcu::Vec2&						lodBounds,
						  const tcu::LookupPrecision&			lookupPrecision,
						  const tcu::Vec4&						lookupScale,
						  const tcu::Vec4&						lookupBias,
						  const tcu::ConstPixelBufferAccess&	resultAccess,
						  const tcu::PixelBufferAccess&			errorAccess)
{
	std::vector<tcu::ConstPixelBufferAccess>	levels;

	switch (imageViewType)
	{
		case VK_IMAGE_VIEW_TYPE_1D:
		{
			UniquePtr<tcu::Texture1DView>			texView(getTexture1DView(texture, subresource, levels));

			return validateResultImage(*texView, sampler, componentMapping, coordAccess, lodBounds, lookupPrecision, lookupScale, lookupBias, resultAccess, errorAccess);
		}

		case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
		{
			UniquePtr<tcu::Texture1DArrayView>		texView(getTexture1DArrayView(texture, subresource, levels));

			return validateResultImage(*texView, sampler, componentMapping, coordAccess, lodBounds, lookupPrecision, lookupScale, lookupBias, resultAccess, errorAccess);
		}

		case VK_IMAGE_VIEW_TYPE_2D:
		{
			UniquePtr<tcu::Texture2DView>			texView(getTexture2DView(texture, subresource, levels));

			return validateResultImage(*texView, sampler, componentMapping, coordAccess, lodBounds, lookupPrecision, lookupScale, lookupBias, resultAccess, errorAccess);
		}

		case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
		{
			UniquePtr<tcu::Texture2DArrayView>		texView(getTexture2DArrayView(texture, subresource, levels));

			return validateResultImage(*texView, sampler, componentMapping, coordAccess, lodBounds, lookupPrecision, lookupScale, lookupBias, resultAccess, errorAccess);
		}

		case VK_IMAGE_VIEW_TYPE_CUBE:
		{
			UniquePtr<tcu::TextureCubeView>			texView(getTextureCubeView(texture, subresource, levels));

			return validateResultImage(*texView, sampler, componentMapping, coordAccess, lodBounds, lookupPrecision, lookupScale, lookupBias, resultAccess, errorAccess);
		}

		case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
		{
			UniquePtr<tcu::TextureCubeArrayView>	texView(getTextureCubeArrayView(texture, subresource, levels));

			return validateResultImage(*texView, sampler, componentMapping, coordAccess, lodBounds, lookupPrecision, lookupScale, lookupBias, resultAccess, errorAccess);
		}

		case VK_IMAGE_VIEW_TYPE_3D:
		{
			UniquePtr<tcu::Texture3DView>			texView(getTexture3DView(texture, subresource, levels));

			return validateResultImage(*texView, sampler, componentMapping, coordAccess, lodBounds, lookupPrecision, lookupScale, lookupBias, resultAccess, errorAccess);
		}

		default:
			DE_ASSERT(false);
			return false;
	}
}

} // anonymous

tcu::TestStatus ImageSamplingInstance::verifyImage (void)
{
	const VkPhysicalDeviceLimits&		limits					= m_context.getDeviceProperties().limits;
	// \note Color buffer is used to capture coordinates - not sampled texture values
	const tcu::TextureFormat			colorFormat				(tcu::TextureFormat::RGBA, tcu::TextureFormat::FLOAT);
	const tcu::TextureFormat			depthStencilFormat;		// Undefined depth/stencil format.
	const CoordinateCaptureProgram		coordCaptureProgram;
	const rr::Program					rrProgram				= coordCaptureProgram.getReferenceProgram();
	ReferenceRenderer					refRenderer				(m_renderSize.x(), m_renderSize.y(), 1, colorFormat, depthStencilFormat, &rrProgram);
	const bool							useStencilAspect		= (m_subresourceRange.aspectMask == VK_IMAGE_ASPECT_STENCIL_BIT);

	bool								compareOkAll			= true;

	tcu::Vec4							lookupScale				(1.0f);
	tcu::Vec4							lookupBias				(0.0f);

	getLookupScaleBias(m_imageFormat, lookupScale, lookupBias, useStencilAspect);

	// Render out coordinates
	{
		const rr::RenderState renderState(refRenderer.getViewportState(), m_context.getDeviceProperties().limits.subPixelPrecisionBits);
		refRenderer.draw(renderState, rr::PRIMITIVETYPE_TRIANGLES, m_vertices);
	}

	// Verify results
	{
		const tcu::Sampler					sampler			= mapVkSampler(m_samplerParams);
		const float							referenceLod	= de::clamp(m_samplerParams.mipLodBias + m_samplerLod, m_samplerParams.minLod, m_samplerParams.maxLod);
		const float							lodError		= 1.0f / static_cast<float>((1u << limits.mipmapPrecisionBits) - 1u);
		const tcu::Vec2						lodBounds		(referenceLod - lodError, referenceLod + lodError);
		const vk::VkImageSubresourceRange	subresource		= resolveSubresourceRange(*m_texture, m_subresourceRange);

		const tcu::ConstPixelBufferAccess	coordAccess		= refRenderer.getAccess();
		tcu::TextureLevel					errorMask		(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8), (int)m_renderSize.x(), (int)m_renderSize.y());
		const tcu::PixelBufferAccess		errorAccess		= errorMask.getAccess();

		const bool							isNearestOnly	= (m_samplerParams.minFilter == VK_FILTER_NEAREST && m_samplerParams.magFilter == VK_FILTER_NEAREST);

		tcu::LookupPrecision				lookupPrecision;

		// Set precision requirements - very low for these tests as
		// the point of the test is not to validate accuracy.
		lookupPrecision.coordBits		= tcu::IVec3(17, 17, 17);
		lookupPrecision.uvwBits			= tcu::IVec3(5, 5, 5);
		lookupPrecision.colorMask		= m_componentMask;
		lookupPrecision.colorThreshold	= tcu::computeFixedPointThreshold(max((tcu::IVec4(8, 8, 8, 8) - (isNearestOnly ? 1 : 2)), tcu::IVec4(0))) / swizzleScaleBias(lookupScale, m_componentMapping, 1.0f);

		if (m_imageFormat == VK_FORMAT_BC5_UNORM_BLOCK || m_imageFormat == VK_FORMAT_BC5_SNORM_BLOCK)
			lookupPrecision.colorThreshold = tcu::Vec4(0.06f, 0.06f, 0.06f, 0.06f);
		if (tcu::isSRGB(m_texture->getTextureFormat()))
			lookupPrecision.colorThreshold += tcu::Vec4(4.f / 255.f);

		de::MovePtr<TestTexture>			textureCopy;
		TestTexture*						texture			= DE_NULL;

		if (isCombinedDepthStencilType(m_texture->getTextureFormat().type))
		{
			// Verification loop does not support reading from combined depth stencil texture levels.
			// Get rid of stencil component.

			tcu::TextureFormat::ChannelOrder	channelOrder	= tcu::TextureFormat::CHANNELORDER_LAST;
			tcu::TextureFormat::ChannelType		channelType		= tcu::TextureFormat::CHANNELTYPE_LAST;

			if (subresource.aspectMask == VK_IMAGE_ASPECT_STENCIL_BIT)
			{
				channelOrder	= tcu::TextureFormat::S;
				channelType		= tcu::TextureFormat::UNSIGNED_INT8;
			}
			else
			{
				channelOrder = tcu::TextureFormat::D;

				switch (m_texture->getTextureFormat().type)
				{
				case tcu::TextureFormat::UNSIGNED_INT_16_8_8:
					channelType = tcu::TextureFormat::UNORM_INT16;
					break;
				case tcu::TextureFormat::UNSIGNED_INT_24_8:
				case tcu::TextureFormat::UNSIGNED_INT_24_8_REV:
					channelType = tcu::TextureFormat::UNORM_INT24;
					break;
				case tcu::TextureFormat::FLOAT_UNSIGNED_INT_24_8_REV:
					channelType = tcu::TextureFormat::FLOAT;
					break;
				default:
					DE_FATAL("Unhandled texture format type in switch");
				}
			}

			textureCopy	= m_texture->copy(tcu::TextureFormat(channelOrder, channelType));
			texture		= textureCopy.get();
		}
		else
		{
			texture		= m_texture.get();
		}

		for (int imgNdx = 0; imgNdx < m_imageCount; ++imgNdx)
		{
			// Read back result image
			UniquePtr<tcu::TextureLevel>		result			(readColorAttachment(m_context.getDeviceInterface(),
																					 m_context.getDevice(),
																					 m_context.getUniversalQueue(),
																					 m_context.getUniversalQueueFamilyIndex(),
																					 m_context.getDefaultAllocator(),
																					 **m_colorImages[imgNdx],
																					 m_colorFormat,
																					 m_renderSize));
			const tcu::ConstPixelBufferAccess	resultAccess	= result->getAccess();
			bool								compareOk		= validateResultImage(*texture,
																					  m_imageViewType,
																					  subresource,
																					  sampler,
																					  m_componentMapping,
																					  coordAccess,
																					  lodBounds,
																					  lookupPrecision,
																					  lookupScale,
																					  lookupBias,
																					  resultAccess,
																					  errorAccess);
			if (!compareOk)
				m_context.getTestContext().getLog()
				<< tcu::TestLog::Image("Result", "Result Image", resultAccess)
				<< tcu::TestLog::Image("ErrorMask", "Error Mask", errorAccess);

			compareOkAll = compareOkAll && compareOk;
		}
	}

	if (compareOkAll)
		return tcu::TestStatus::pass("Result image matches reference");
	else
		return tcu::TestStatus::fail("Image mismatch");
}

} // pipeline
} // vkt
