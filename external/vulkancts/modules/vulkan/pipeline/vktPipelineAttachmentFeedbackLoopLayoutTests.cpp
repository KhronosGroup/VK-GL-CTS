/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 Valve Corporation.
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
 * \brief VK_EXT_attachment_feedback_loop_layout Tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineAttachmentFeedbackLoopLayoutTests.hpp"
#include "vktPipelineImageSamplingInstance.hpp"
#include "vktPipelineImageUtil.hpp"
#include "vktPipelineVertexUtil.hpp"
#include "vktTestCase.hpp"
#include "vktPipelineClearUtil.hpp"

#include "vkImageUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkPrograms.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBufferWithMemory.hpp"

#include "tcuPlatform.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuMaybe.hpp"

#include "deStringUtil.hpp"
#include "deMemory.h"

#include <iomanip>
#include <sstream>
#include <vector>
#include <string>
#include <memory>
#include <utility>
#include <algorithm>

namespace vkt
{
namespace pipeline
{

using namespace vk;
using de::MovePtr;

namespace
{

enum TestMode
{
	TEST_MODE_READ_ONLY = 0,
	TEST_MODE_WRITE_ONLY = 1,
	TEST_MODE_READ_WRITE_SAME_PIXEL = 2,		// Sample from and write to the same pixel
	TEST_MODE_READ_WRITE_DIFFERENT_AREAS = 3,	// Sample from one half of the image and write the values to the other half
};

enum ImageAspectTestMode
{
	IMAGE_ASPECT_TEST_COLOR = 0,
	IMAGE_ASPECT_TEST_DEPTH = 1,
	IMAGE_ASPECT_TEST_STENCIL = 2,
};

VkImageAspectFlagBits testModeToAspectFlags (ImageAspectTestMode testMode)
{
	switch (testMode)
	{
	case IMAGE_ASPECT_TEST_COLOR:		return VK_IMAGE_ASPECT_COLOR_BIT;
	case IMAGE_ASPECT_TEST_DEPTH:		return VK_IMAGE_ASPECT_DEPTH_BIT;
	case IMAGE_ASPECT_TEST_STENCIL:		return VK_IMAGE_ASPECT_STENCIL_BIT;
	default:							break;
	}

	DE_ASSERT(false);
	return VK_IMAGE_ASPECT_FLAG_BITS_MAX_ENUM;
}

enum class PipelineStateMode
{
	STATIC = 0,							// Static only.
	DYNAMIC_WITH_ZERO_STATIC,			// Dynamic, with static flags 0.
	DYNAMIC_WITH_CONTRADICTORY_STATIC,	// Dynamic, with static flags contradicting the dynamic state (see below).
};

VkPipelineCreateFlags aspectFlagsToPipelineCreateFlags (VkImageAspectFlags aspectFlags)
{
	VkPipelineCreateFlags pipelineFlags = 0u;

	if ((aspectFlags & VK_IMAGE_ASPECT_COLOR_BIT) != 0u)
		pipelineFlags |= VK_PIPELINE_CREATE_COLOR_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT;

	if ((aspectFlags & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0u)
		pipelineFlags |= VK_PIPELINE_CREATE_DEPTH_STENCIL_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT;

	return pipelineFlags;
}

VkPipelineCreateFlags getStaticPipelineCreateFlags (VkImageAspectFlags usedFlags, PipelineStateMode stateMode)
{
	if (stateMode == PipelineStateMode::STATIC)
		return aspectFlagsToPipelineCreateFlags(usedFlags);

	if (stateMode == PipelineStateMode::DYNAMIC_WITH_ZERO_STATIC)
		return 0u;

	// Statically include all flags which are not present in the used flags that will be set dynamically.
	VkPipelineCreateFlags pipelineStaticFlags	= (VK_PIPELINE_CREATE_COLOR_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT | VK_PIPELINE_CREATE_DEPTH_STENCIL_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT);
	VkPipelineCreateFlags pipelineUsedFlags		= aspectFlagsToPipelineCreateFlags(usedFlags);

	pipelineStaticFlags &= (~pipelineUsedFlags);
	return pipelineStaticFlags;
}

// Output images are a square of this size
const int outputImageSize = 256;

ImageAspectTestMode getImageAspectTestMode (const VkFormat format)
{
	if (tcu::hasDepthComponent(mapVkFormat(format).order))
		return IMAGE_ASPECT_TEST_DEPTH;

	if (tcu::hasStencilComponent(mapVkFormat(format).order))
		return IMAGE_ASPECT_TEST_STENCIL;

	return IMAGE_ASPECT_TEST_COLOR;
};

class SamplerViewType {
public:
	SamplerViewType (vk::VkImageViewType type, bool normalized = true)
		: m_viewType(type), m_normalized(normalized)
	{
		if (!normalized)
			DE_ASSERT(type == vk::VK_IMAGE_VIEW_TYPE_2D || type == vk::VK_IMAGE_VIEW_TYPE_1D);
	}

	operator vk::VkImageViewType () const
	{
		return m_viewType;
	}

	bool isNormalized () const
	{
		return m_normalized;
	}

private:
	vk::VkImageViewType m_viewType;
	bool				m_normalized;
};

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

VkImageAspectFlags getAspectFlags (tcu::TextureFormat format)
{
	VkImageAspectFlags	aspectFlag	= 0;
	aspectFlag |= (tcu::hasDepthComponent(format.order)? VK_IMAGE_ASPECT_DEPTH_BIT : 0);
	aspectFlag |= (tcu::hasStencilComponent(format.order)? VK_IMAGE_ASPECT_STENCIL_BIT : 0);

	if (!aspectFlag)
		aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;

	return aspectFlag;
}

VkImageAspectFlags getAspectFlags (VkFormat format)
{
	if (isCompressedFormat(format))
		return VK_IMAGE_ASPECT_COLOR_BIT;
	else
		return getAspectFlags(mapVkFormat(format));
}

tcu::TextureFormat getSizeCompatibleTcuTextureFormat (VkFormat format)
{
	if (isCompressedFormat(format))
		return (getBlockSizeInBytes(format) == 8) ? mapVkFormat(VK_FORMAT_R16G16B16A16_UINT) : mapVkFormat(VK_FORMAT_R32G32B32A32_UINT);
	else
		return mapVkFormat(format);
}

// Utilities to create test nodes
std::string getFormatCaseName (const VkFormat format)
{
	const std::string fullName = getFormatName(format);

	DE_ASSERT(de::beginsWith(fullName, "VK_FORMAT_"));

	return de::toLower(fullName.substr(10));
}

class AttachmentFeedbackLoopLayoutImageSamplingInstance : public ImageSamplingInstance
{
public:
								AttachmentFeedbackLoopLayoutImageSamplingInstance	(Context&						context,
																					 ImageSamplingInstanceParams	params,
																					 bool							useImageAsColorOrDSAttachment_,
																					 bool							useDifferentAreasSampleWrite_,
																					 bool							interleaveReadWriteComponents_,
																					 ImageAspectTestMode			imageAspectTestMode,
																					 PipelineStateMode				pipelineStateMode,
																					 bool							useMaintenance5_);

	virtual						~AttachmentFeedbackLoopLayoutImageSamplingInstance	(void);

	virtual tcu::TestStatus		iterate												(void) override;

protected:
	virtual tcu::TestStatus		verifyImage											(void) override;
	virtual void				setup												(void) override;

	ImageSamplingInstanceParams		m_params;
	const bool						m_useImageAsColorOrDSAttachment;
	const bool						m_useDifferentAreasSampleWrite;
	const bool						m_interleaveReadWriteComponents;
	const ImageAspectTestMode		m_imageAspectTestMode;
	const PipelineStateMode			m_pipelineStateMode;
	const bool						m_useMaintenance5;
};

class AttachmentFeedbackLoopLayoutDepthStencilImageSamplingInstance : public AttachmentFeedbackLoopLayoutImageSamplingInstance
{
public:
								AttachmentFeedbackLoopLayoutDepthStencilImageSamplingInstance	(Context&						context,
																								 ImageSamplingInstanceParams	params,
																								 bool							useImageAsColorOrDSAttachment_,
																								 bool							useDifferentAreasSampleWrite_,
																								 bool							interleaveReadWriteComponents_,
																								 ImageAspectTestMode			imageAspectTestMode,
																								 PipelineStateMode				pipelineStateMode,
																								 bool							useMaintenance5_);

	virtual						~AttachmentFeedbackLoopLayoutDepthStencilImageSamplingInstance	(void);

	virtual tcu::TestStatus		iterate												(void) override;

protected:
	virtual tcu::TestStatus		verifyImage											(void) override;
	virtual void				setup												(void) override;

	bool										m_separateStencilUsage;

	std::vector<SharedImagePtr>					m_dsImages;
	std::vector<SharedAllocPtr>					m_dsImageAllocs;
	std::vector<SharedImageViewPtr>				m_dsAttachmentViews;
};

AttachmentFeedbackLoopLayoutImageSamplingInstance::AttachmentFeedbackLoopLayoutImageSamplingInstance (Context&						context,
																									  ImageSamplingInstanceParams	params,
																									  bool							useImageAsColorOrDSAttachment_,
																									  bool							useDifferentAreasSampleWrite_,
																									  bool							interleaveReadWriteComponents_,
																									  ImageAspectTestMode			imageAspectTestMode,
																									  PipelineStateMode				pipelineStateMode,
																									  bool							useMaintenance5_)
	: ImageSamplingInstance				(context, params)
	, m_params							(params)
	, m_useImageAsColorOrDSAttachment	(useImageAsColorOrDSAttachment_)
	, m_useDifferentAreasSampleWrite	(useDifferentAreasSampleWrite_)
	, m_interleaveReadWriteComponents	(interleaveReadWriteComponents_)
	, m_imageAspectTestMode				(imageAspectTestMode)
	, m_pipelineStateMode				(pipelineStateMode)
	, m_useMaintenance5					(useMaintenance5_)
{
}

void AttachmentFeedbackLoopLayoutImageSamplingInstance::setup (void)
{
	const InstanceInterface&				vki						= m_context.getInstanceInterface();
	const DeviceInterface&					vk						= m_context.getDeviceInterface();
	const VkPhysicalDevice					physDevice				= m_context.getPhysicalDevice();
	const VkDevice							vkDevice				= m_context.getDevice();
	const VkQueue							queue					= m_context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
	SimpleAllocator							memAlloc				(vk, vkDevice, getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));
	const VkComponentMapping				componentMappingRGBA	= { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
	tcu::UVec2								renderSize				= m_useImageAsColorOrDSAttachment ? tcu::UVec2({ (unsigned)m_imageSize.x(), (unsigned)m_imageSize.y() }) : m_renderSize;

	DE_ASSERT(m_samplerParams.pNext == DE_NULL);

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

		VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
			VK_IMAGE_USAGE_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

		if (isDepthStencilFormat(m_imageFormat))
			imageUsageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		else
			imageUsageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

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
			imageUsageFlags,												// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,										// VkSharingMode			sharingMode;
			1u,																// deUint32					queueFamilyIndexCount;
			&queueFamilyIndex,												// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED										// VkImageLayout			initialLayout;
		};

		checkImageSupport(vki, physDevice, imageParams);

		m_images.resize(m_imageCount);
		m_imageAllocs.resize(m_imageCount);
		m_imageViews.resize(m_imageCount);

		// Create command pool
		m_cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
		m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

		for (int imgNdx = 0; imgNdx < m_imageCount; ++imgNdx)
		{
			m_images[imgNdx] = SharedImagePtr(new UniqueImage(createImage(vk, vkDevice, &imageParams)));
			m_imageAllocs[imgNdx] = SharedAllocPtr(new UniqueAlloc(allocateImage(vki, vk, physDevice, vkDevice, **m_images[imgNdx], MemoryRequirement::Any, memAlloc, m_allocationKind)));
			VK_CHECK(vk.bindImageMemory(vkDevice, **m_images[imgNdx], (*m_imageAllocs[imgNdx])->getMemory(), (*m_imageAllocs[imgNdx])->getOffset()));

			// Upload texture data
			uploadTestTexture(vk, vkDevice, queue, queueFamilyIndex, memAlloc, *m_texture, **m_images[imgNdx], m_imageLayout);

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
			descriptorImageInfo[imgNdx].imageLayout	= m_imageLayout;							// VkImageLayout	imageLayout;
		}

		DescriptorSetUpdateBuilder setUpdateBuilder;
		if (m_samplingType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
		{
			const VkDescriptorImageInfo descriptorSamplerInfo =
			{
				*m_sampler,									// VkSampler		sampler;
				DE_NULL,									// VkImageView		imageView;
				m_imageLayout,								// VkImageLayout	imageLayout;
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
			{ (deUint32)renderSize.x(), (deUint32)renderSize.y(), 1u },					// VkExtent3D				extent;
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

		checkImageSupport(vki, physDevice, colorImageParams);

		m_colorImages.resize(m_imageCount);
		m_colorImageAllocs.resize(m_imageCount);
		m_colorAttachmentViews.resize(m_imageCount);

		if (m_useImageAsColorOrDSAttachment)
		{
			for (int imgNdx = 0; imgNdx < m_imageCount; ++imgNdx)
			{
				m_colorImages[imgNdx] = m_images[imgNdx];
				m_colorImageAllocs[imgNdx] = m_imageAllocs[imgNdx];
				m_colorAttachmentViews[imgNdx] = m_imageViews[imgNdx];
			}
		}
		else
		{
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
	}

	// Create render pass
	{
		std::vector<VkAttachmentDescription>	attachmentDescriptions(m_imageCount);
		std::vector<VkAttachmentReference>		attachmentReferences(m_imageCount);

		VkAttachmentLoadOp	loadOp		= m_useImageAsColorOrDSAttachment ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
		VkImageLayout		imageLayout	= m_useImageAsColorOrDSAttachment ? m_imageLayout : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		for (int imgNdx = 0; imgNdx < m_imageCount; ++imgNdx)
		{
			attachmentDescriptions[imgNdx].flags			= 0u;																	// VkAttachmentDescriptionFlags		flags;
			attachmentDescriptions[imgNdx].format			= m_useImageAsColorOrDSAttachment ? m_imageFormat : m_colorFormat;		// VkFormat							format;
			attachmentDescriptions[imgNdx].samples			= VK_SAMPLE_COUNT_1_BIT;												// VkSampleCountFlagBits			samples;
			attachmentDescriptions[imgNdx].loadOp			= loadOp;																// VkAttachmentLoadOp				loadOp;
			attachmentDescriptions[imgNdx].storeOp			= VK_ATTACHMENT_STORE_OP_STORE;											// VkAttachmentStoreOp				storeOp;
			attachmentDescriptions[imgNdx].stencilLoadOp	= loadOp;																// VkAttachmentLoadOp				stencilLoadOp;
			attachmentDescriptions[imgNdx].stencilStoreOp	= VK_ATTACHMENT_STORE_OP_STORE;											// VkAttachmentStoreOp				stencilStoreOp;
			attachmentDescriptions[imgNdx].initialLayout	= imageLayout;															// VkImageLayout					initialLayout;
			attachmentDescriptions[imgNdx].finalLayout		= imageLayout;															// VkImageLayout					finalLayout;

			attachmentReferences[imgNdx].attachment			= (deUint32)imgNdx;														// deUint32							attachment;
			attachmentReferences[imgNdx].layout				= imageLayout;															// VkImageLayout					layout;
		}

		const VkSubpassDescription subpassDescription =
		{
			0u,													// VkSubpassDescriptionFlags	flags;
			VK_PIPELINE_BIND_POINT_GRAPHICS,					// VkPipelineBindPoint			pipelineBindPoint;
			0u,													// deUint32						inputAttachmentCount;
			DE_NULL,											// const VkAttachmentReference*	pInputAttachments;
			(deUint32)m_imageCount,								// deUint32						colorAttachmentCount;
			&attachmentReferences[0],							// const VkAttachmentReference*	pColorAttachments;
			DE_NULL,											// const VkAttachmentReference*	pResolveAttachments;
			DE_NULL,											// const VkAttachmentReference*	pDepthStencilAttachment;
			0u,													// deUint32						preserveAttachmentCount;
			DE_NULL												// const VkAttachmentReference*	pPreserveAttachments;
		};

		std::vector<VkSubpassDependency> subpassDependencies;

		if (m_useImageAsColorOrDSAttachment)
		{
			const VkSubpassDependency spdVal =
			{
				0u,																	//	uint32_t				srcSubpass;
				0u,																	//	uint32_t				dstSubpass;
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,								//	VkPipelineStageFlags	srcStageMask;
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,						//	VkPipelineStageFlags	dstStageMask;
				VK_ACCESS_SHADER_READ_BIT,											//	VkAccessFlags			srcAccessMask;
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,								//	VkAccessFlags			dstAccessMask;
				VK_DEPENDENCY_FEEDBACK_LOOP_BIT_EXT | VK_DEPENDENCY_BY_REGION_BIT,	//	VkDependencyFlags		dependencyFlags;
			};

			subpassDependencies.push_back(spdVal);
		}

		const VkRenderPassCreateInfo renderPassParams =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,			// VkStructureType					sType;
			DE_NULL,											// const void*						pNext;
			0u,													// VkRenderPassCreateFlags			flags;
			(deUint32)attachmentDescriptions.size(),			// deUint32							attachmentCount;
			&attachmentDescriptions[0],							// const VkAttachmentDescription*	pAttachments;
			1u,													// deUint32							subpassCount;
			&subpassDescription,								// const VkSubpassDescription*		pSubpasses;
			static_cast<uint32_t>(subpassDependencies.size()),	// deUint32							dependencyCount;
			de::dataOrNull(subpassDependencies),				// const VkSubpassDependency*		pDependencies;
		};

		m_renderPass = RenderPassWrapper(m_pipelineConstructionType, vk, vkDevice, &renderPassParams);
	}

	// Create framebuffer
	{
		std::vector<VkImage> images(m_imageCount);
		std::vector<VkImageView> pAttachments(m_imageCount);
		for (int imgNdx = 0; imgNdx < m_imageCount; ++imgNdx)
		{
			images[imgNdx] = m_colorImages[imgNdx]->get();
			pAttachments[imgNdx] =  m_colorAttachmentViews[imgNdx]->get();
		}

		const VkFramebufferCreateInfo framebufferParams =
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,			// VkStructureType			sType;
			DE_NULL,											// const void*				pNext;
			0u,													// VkFramebufferCreateFlags	flags;
			*m_renderPass,										// VkRenderPass				renderPass;
			(deUint32)m_imageCount,								// deUint32					attachmentCount;
			&pAttachments[0],									// const VkImageView*		pAttachments;
			(deUint32)renderSize.x(),							// deUint32					width;
			(deUint32)renderSize.y(),							// deUint32					height;
			1u													// deUint32					layers;
		};

		m_renderPass.createFramebuffer(vk, vkDevice, &framebufferParams, images);
	}

	// Create pipeline layouts
	{
		const VkPipelineLayoutCreateInfo pipelineLayoutParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT,	// VkPipelineLayoutCreateFlags	flags;
			0u,													// deUint32						setLayoutCount;
			DE_NULL,											// const VkDescriptorSetLayout*	pSetLayouts;
			0u,													// deUint32						pushConstantRangeCount;
			DE_NULL												// const VkPushConstantRange*	pPushConstantRanges;
		};

		m_preRasterizationStatePipelineLayout = PipelineLayoutWrapper(m_pipelineConstructionType, vk, vkDevice, &pipelineLayoutParams);
	}
	{
		const VkPipelineLayoutCreateInfo pipelineLayoutParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT,	// VkPipelineLayoutCreateFlags	flags;
			1u,													// deUint32						setLayoutCount;
			&m_descriptorSetLayout.get(),						// const VkDescriptorSetLayout*	pSetLayouts;
			0u,													// deUint32						pushConstantRangeCount;
			DE_NULL												// const VkPushConstantRange*	pPushConstantRanges;
		};

		m_fragmentStatePipelineLayout = PipelineLayoutWrapper(m_pipelineConstructionType, vk, vkDevice, &pipelineLayoutParams);
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

		const std::vector<VkViewport>	viewports	(1, makeViewport(renderSize));
		const std::vector<VkRect2D>		scissors	(1, makeRect2D(renderSize));

		std::vector<VkPipelineColorBlendAttachmentState>	colorBlendAttachmentStates(m_imageCount);

		VkColorComponentFlags colorComponents = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		if (m_interleaveReadWriteComponents)
			colorComponents = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_B_BIT;

		for (int imgNdx = 0; imgNdx < m_imageCount; ++imgNdx)
		{
			colorBlendAttachmentStates[imgNdx].blendEnable			= false;												// VkBool32					blendEnable;
			colorBlendAttachmentStates[imgNdx].srcColorBlendFactor	= VK_BLEND_FACTOR_ONE;									// VkBlendFactor			srcColorBlendFactor;
			colorBlendAttachmentStates[imgNdx].dstColorBlendFactor	= VK_BLEND_FACTOR_ZERO;									// VkBlendFactor			dstColorBlendFactor;
			colorBlendAttachmentStates[imgNdx].colorBlendOp			= VK_BLEND_OP_ADD;										// VkBlendOp				colorBlendOp;
			colorBlendAttachmentStates[imgNdx].srcAlphaBlendFactor	= VK_BLEND_FACTOR_ONE;									// VkBlendFactor			srcAlphaBlendFactor;
			colorBlendAttachmentStates[imgNdx].dstAlphaBlendFactor	= VK_BLEND_FACTOR_ZERO;									// VkBlendFactor			dstAlphaBlendFactor;
			colorBlendAttachmentStates[imgNdx].alphaBlendOp			= VK_BLEND_OP_ADD;										// VkBlendOp				alphaBlendOp;
			colorBlendAttachmentStates[imgNdx].colorWriteMask		= colorComponents;										// VkColorComponentFlags	colorWriteMask;
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

		std::vector<VkDynamicState> dynamicStates;
		if (m_pipelineStateMode != PipelineStateMode::STATIC)
			dynamicStates.push_back(VK_DYNAMIC_STATE_ATTACHMENT_FEEDBACK_LOOP_ENABLE_EXT);

		const VkPipelineDynamicStateCreateInfo dynamicStateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			nullptr,
			0u,
			de::sizeU32(dynamicStates),
			de::dataOrNull(dynamicStates),
		};

		if (m_useMaintenance5)
			m_graphicsPipeline.setPipelineCreateFlags2(translateCreateFlag(m_params.pipelineCreateFlags));

		m_graphicsPipeline.setDynamicState(&dynamicStateInfo)
						  .setMonolithicPipelineLayout(m_fragmentStatePipelineLayout)
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

	// Create command buffer
	{
		VkFormat clearFormat = m_useImageAsColorOrDSAttachment ? m_imageFormat : m_colorFormat;
		const std::vector<VkClearValue> attachmentClearValues (m_imageCount, defaultClearValue(clearFormat));

		std::vector<VkImageMemoryBarrier> preAttachmentBarriers(m_imageCount);

		VkAccessFlags dstAccessMask = isDepthStencilFormat(m_imageFormat) ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT : VK_ACCESS_SHADER_READ_BIT;
		VkPipelineStageFlags pipelineStageFlags = isDepthStencilFormat(m_imageFormat) ? VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

		for (int imgNdx = 0; imgNdx < m_imageCount; ++imgNdx)
		{
			preAttachmentBarriers[imgNdx].sType								= VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;								// VkStructureType			sType;
			preAttachmentBarriers[imgNdx].pNext								= DE_NULL;																// const void*				pNext;
			preAttachmentBarriers[imgNdx].srcAccessMask						= VK_ACCESS_TRANSFER_WRITE_BIT;											// VkAccessFlags			srcAccessMask;
			preAttachmentBarriers[imgNdx].dstAccessMask						= dstAccessMask;														// VkAccessFlags			dstAccessMask;
			preAttachmentBarriers[imgNdx].oldLayout							= m_imageLayout;														// VkImageLayout			oldLayout;
			preAttachmentBarriers[imgNdx].newLayout							= m_imageLayout;														// VkImageLayout			newLayout;
			preAttachmentBarriers[imgNdx].srcQueueFamilyIndex				= VK_QUEUE_FAMILY_IGNORED;												// deUint32					srcQueueFamilyIndex;
			preAttachmentBarriers[imgNdx].dstQueueFamilyIndex				= VK_QUEUE_FAMILY_IGNORED;												// deUint32					dstQueueFamilyIndex;
			preAttachmentBarriers[imgNdx].image								= **m_images[imgNdx];													// VkImage					image;
			preAttachmentBarriers[imgNdx].subresourceRange					= m_subresourceRange;													// VkImageSubresourceRange	subresourceRange;
		}

		beginCommandBuffer(vk, *m_cmdBuffer, 0u);

		vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, pipelineStageFlags, (VkDependencyFlags)0,
			0u, DE_NULL, 0u, DE_NULL, (deUint32)m_imageCount, &preAttachmentBarriers[0]);

		if (!m_useImageAsColorOrDSAttachment)
		{
			// Pipeline barrier for the color attachment, which is a different image than the sampled one.
			for (int imgNdx = 0; imgNdx < m_imageCount; ++imgNdx)
			{
				preAttachmentBarriers[imgNdx].sType								= VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;								// VkStructureType			sType;
				preAttachmentBarriers[imgNdx].pNext								= DE_NULL;																// const void*				pNext;
				preAttachmentBarriers[imgNdx].srcAccessMask						= (VkAccessFlagBits)0u;													// VkAccessFlags			srcAccessMask;
				preAttachmentBarriers[imgNdx].dstAccessMask						= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;									// VkAccessFlags			dstAccessMask;
				preAttachmentBarriers[imgNdx].oldLayout							= VK_IMAGE_LAYOUT_UNDEFINED;											// VkImageLayout			oldLayout;
				preAttachmentBarriers[imgNdx].newLayout							= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;								// VkImageLayout			newLayout;
				preAttachmentBarriers[imgNdx].srcQueueFamilyIndex				= VK_QUEUE_FAMILY_IGNORED;												// deUint32					srcQueueFamilyIndex;
				preAttachmentBarriers[imgNdx].dstQueueFamilyIndex				= VK_QUEUE_FAMILY_IGNORED;												// deUint32					dstQueueFamilyIndex;
				preAttachmentBarriers[imgNdx].image								= **m_colorImages[imgNdx];												// VkImage					image;
				preAttachmentBarriers[imgNdx].subresourceRange.aspectMask		= getAspectFlags(m_colorFormat);										// VkImageSubresourceRange	subresourceRange;
				preAttachmentBarriers[imgNdx].subresourceRange.baseMipLevel		= 0u;
				preAttachmentBarriers[imgNdx].subresourceRange.levelCount		= 1u;
				preAttachmentBarriers[imgNdx].subresourceRange.baseArrayLayer	= 0u;
				preAttachmentBarriers[imgNdx].subresourceRange.layerCount		= 1u;
			}

			vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, (VkDependencyFlags)0,
								  0u, DE_NULL, 0u, DE_NULL, (deUint32)m_imageCount, &preAttachmentBarriers[0]);

			m_renderPass.begin(vk, *m_cmdBuffer, makeRect2D(0, 0, renderSize.x(), renderSize.y()), (deUint32)attachmentClearValues.size(), &attachmentClearValues[0]);
		}
		else
		{
			// Do not clear the color attachments as we are using the sampled texture as color attachment as well.
			m_renderPass.begin(vk, *m_cmdBuffer, makeRect2D(0, 0, renderSize.x(), renderSize.y()), 0u, DE_NULL);
		}

		m_graphicsPipeline.bind(*m_cmdBuffer);

		vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_fragmentStatePipelineLayout, 0, 1, &m_descriptorSet.get(), 0, DE_NULL);

		const VkDeviceSize vertexBufferOffset = 0;
		vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);

		if (m_pipelineStateMode != PipelineStateMode::STATIC)
			vk.cmdSetAttachmentFeedbackLoopEnableEXT(*m_cmdBuffer, testModeToAspectFlags(m_imageAspectTestMode));

		vk.cmdDraw(*m_cmdBuffer, (deUint32)m_vertices.size(), 1, 0, 0);

		m_renderPass.end(vk, *m_cmdBuffer);
		endCommandBuffer(vk, *m_cmdBuffer);
	}
}

AttachmentFeedbackLoopLayoutDepthStencilImageSamplingInstance::AttachmentFeedbackLoopLayoutDepthStencilImageSamplingInstance (Context&						context,
																															  ImageSamplingInstanceParams	params,
																															  bool							useImageAsColorOrDSAttachment_,
																															  bool							useDifferentAreasSampleWrite_,
																															  bool							interleaveReadWriteComponents_,
																															  ImageAspectTestMode			imageAspectTestMode,
																															  PipelineStateMode				pipelineStateMode,
																															  bool							useMaintenance5_)
	: AttachmentFeedbackLoopLayoutImageSamplingInstance		(context, params, useImageAsColorOrDSAttachment_, useDifferentAreasSampleWrite_, interleaveReadWriteComponents_, imageAspectTestMode, pipelineStateMode, useMaintenance5_)
	, m_separateStencilUsage								(params.separateStencilUsage)
{
}

AttachmentFeedbackLoopLayoutDepthStencilImageSamplingInstance::~AttachmentFeedbackLoopLayoutDepthStencilImageSamplingInstance (void)
{
}

void AttachmentFeedbackLoopLayoutDepthStencilImageSamplingInstance::setup (void)
{
	const InstanceInterface&				vki						= m_context.getInstanceInterface();
	const DeviceInterface&					vk						= m_context.getDeviceInterface();
	const VkPhysicalDevice					physDevice				= m_context.getPhysicalDevice();
	const VkDevice							vkDevice				= m_context.getDevice();
	const VkQueue							queue					= m_context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
	SimpleAllocator							memAlloc				(vk, vkDevice, getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));
	tcu::UVec2								renderSize				= tcu::UVec2({ (unsigned)m_imageSize.x(), (unsigned)m_imageSize.y() });

	DE_ASSERT(m_useImageAsColorOrDSAttachment && isDepthStencilFormat(m_imageFormat));
	DE_ASSERT(m_samplerParams.pNext == DE_NULL);

	// Create texture images, views
	{
		VkImageCreateFlags			imageFlags			= 0u;

		if (m_imageViewType == VK_IMAGE_VIEW_TYPE_CUBE || m_imageViewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)
			imageFlags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

		// Initialize texture data
		if (isCompressedFormat(m_imageFormat))
			m_texture = createTestTexture(mapVkCompressedFormat(m_imageFormat), m_imageViewType, m_imageSize, m_layerCount);
		else
			m_texture = createTestTexture(mapVkFormat(m_imageFormat), m_imageViewType, m_imageSize, m_layerCount);

		VkImageUsageFlags imageUsageFlags =
			VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT |
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

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
			imageUsageFlags,												// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,										// VkSharingMode			sharingMode;
			1u,																// deUint32					queueFamilyIndexCount;
			&queueFamilyIndex,												// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED										// VkImageLayout			initialLayout;
		};

		checkImageSupport(vki, physDevice, imageParams);

		m_images.resize(m_imageCount);
		m_imageAllocs.resize(m_imageCount);

		// Create command pool
		m_cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
		m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

		int numImageViews = m_interleaveReadWriteComponents ? m_imageCount + 1 : m_imageCount;
		m_imageViews.resize(numImageViews);

		for (int imgNdx = 0; imgNdx < m_imageCount; ++imgNdx)
		{
			m_images[imgNdx] = SharedImagePtr(new UniqueImage(createImage(vk, vkDevice, &imageParams)));
			m_imageAllocs[imgNdx] = SharedAllocPtr(new UniqueAlloc(allocateImage(vki, vk, physDevice, vkDevice, **m_images[imgNdx], MemoryRequirement::Any, memAlloc, m_allocationKind)));
			VK_CHECK(vk.bindImageMemory(vkDevice, **m_images[imgNdx], (*m_imageAllocs[imgNdx])->getMemory(), (*m_imageAllocs[imgNdx])->getOffset()));

			// Upload texture data
			uploadTestTexture(vk, vkDevice, queue, queueFamilyIndex, memAlloc, *m_texture, **m_images[imgNdx], m_imageLayout);

		}

		for (int imgNdx = 0; imgNdx < numImageViews; ++imgNdx)
		{
			VkImage image = (m_interleaveReadWriteComponents && imgNdx == m_imageCount) ? **m_images[imgNdx - 1] : **m_images[imgNdx];

			// Create image view and sampler
			VkImageViewCreateInfo imageViewParams =
			{
				VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// VkStructureType			sType;
				DE_NULL,									// const void*				pNext;
				0u,											// VkImageViewCreateFlags	flags;
				image,										// VkImage					image;
				m_imageViewType,							// VkImageViewType			viewType;
				m_imageFormat,								// VkFormat					format;
				m_componentMapping,							// VkComponentMapping		components;
				m_subresourceRange,							// VkImageSubresourceRange	subresourceRange;
			};

			if (m_interleaveReadWriteComponents && imgNdx == m_imageCount)
			{
				imageViewParams.subresourceRange.aspectMask = getImageAspectFlags(mapVkFormat(m_imageFormat));
			}

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
			descriptorImageInfo[imgNdx].imageLayout	= m_imageLayout;							// VkImageLayout	imageLayout;
		}

		DescriptorSetUpdateBuilder setUpdateBuilder;
		if (m_samplingType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
		{
			const VkDescriptorImageInfo descriptorSamplerInfo =
			{
				*m_sampler,									// VkSampler		sampler;
				DE_NULL,									// VkImageView		imageView;
				m_imageLayout,								// VkImageLayout	imageLayout;
			};
			setUpdateBuilder.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0), VK_DESCRIPTOR_TYPE_SAMPLER, &descriptorSamplerInfo);
		}

		const deUint32 binding = m_samplingType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ? 1u : 0u;
		setUpdateBuilder.writeArray(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(binding), m_samplingType, m_imageCount, descriptorImageInfo.data());
		setUpdateBuilder.update(vk, vkDevice);
	}

	// Create depth-stencil images and views, no color attachment
	{
		m_dsImages.resize(m_imageCount);
		m_dsImageAllocs.resize(m_imageCount);
		m_dsAttachmentViews.resize(m_imageCount);

		for (int imgNdx = 0; imgNdx < m_imageCount; ++imgNdx)
		{
			m_dsImages[imgNdx] = m_images[imgNdx];
			m_dsImageAllocs[imgNdx] = m_imageAllocs[imgNdx];
			m_dsAttachmentViews[imgNdx] = m_interleaveReadWriteComponents ? m_imageViews[imgNdx + 1] : m_imageViews[imgNdx];
		}
	}

	// Create render pass
	{
		std::vector<VkAttachmentDescription>	attachmentDescriptions(m_imageCount);
		std::vector<VkAttachmentReference>		attachmentReferences(m_imageCount);

		for (int imgNdx = 0; imgNdx < m_imageCount; ++imgNdx)
		{
			attachmentDescriptions[imgNdx].flags			= 0u;																	// VkAttachmentDescriptionFlags		flags;
			attachmentDescriptions[imgNdx].format			= m_useImageAsColorOrDSAttachment ? m_imageFormat : m_colorFormat;		// VkFormat							format;
			attachmentDescriptions[imgNdx].samples			= VK_SAMPLE_COUNT_1_BIT;												// VkSampleCountFlagBits			samples;
			attachmentDescriptions[imgNdx].loadOp			= VK_ATTACHMENT_LOAD_OP_LOAD;											// VkAttachmentLoadOp				loadOp;
			attachmentDescriptions[imgNdx].storeOp			= VK_ATTACHMENT_STORE_OP_STORE;											// VkAttachmentStoreOp				storeOp;
			attachmentDescriptions[imgNdx].stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_LOAD;											// VkAttachmentLoadOp				stencilLoadOp;
			attachmentDescriptions[imgNdx].stencilStoreOp	= VK_ATTACHMENT_STORE_OP_STORE;											// VkAttachmentStoreOp				stencilStoreOp;
			attachmentDescriptions[imgNdx].initialLayout	= m_imageLayout;														// VkImageLayout					initialLayout;
			attachmentDescriptions[imgNdx].finalLayout		= m_imageLayout;														// VkImageLayout					finalLayout;

			attachmentReferences[imgNdx].attachment			= (deUint32)imgNdx;														// deUint32							attachment;
			attachmentReferences[imgNdx].layout				= m_imageLayout;														// VkImageLayout					layout;
		}

		const VkSubpassDescription subpassDescription =
		{
			0u,																				// VkSubpassDescriptionFlags	flags;
			VK_PIPELINE_BIND_POINT_GRAPHICS,												// VkPipelineBindPoint			pipelineBindPoint;
			0u,																				// deUint32						inputAttachmentCount;
			DE_NULL,																		// const VkAttachmentReference*	pInputAttachments;
			0u,																				// deUint32						colorAttachmentCount;
			DE_NULL,																		// const VkAttachmentReference*	pColorAttachments;
			DE_NULL,																		// const VkAttachmentReference*	pResolveAttachments;
			&attachmentReferences[0],														// const VkAttachmentReference*	pDepthStencilAttachment;
			0u,																				// deUint32						preserveAttachmentCount;
			DE_NULL																			// const VkAttachmentReference*	pPreserveAttachments;
		};

		std::vector<VkSubpassDependency> subpassDependencies;

		if (m_useImageAsColorOrDSAttachment)
		{
			const auto srcStageMask		= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			const auto srcAccessMask	= VK_ACCESS_SHADER_READ_BIT;
			const auto dstStageMask		= (VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
			const auto dstAccessMask	= (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT);

			const VkSubpassDependency spdVal =
			{
				0u,																	//	uint32_t				srcSubpass;
				0u,																	//	uint32_t				dstSubpass;
				srcStageMask,														//	VkPipelineStageFlags	srcStageMask;
				dstStageMask,														//	VkPipelineStageFlags	dstStageMask;
				srcAccessMask,														//	VkAccessFlags			srcAccessMask;
				dstAccessMask,														//	VkAccessFlags			dstAccessMask;
				VK_DEPENDENCY_FEEDBACK_LOOP_BIT_EXT | VK_DEPENDENCY_BY_REGION_BIT,	//	VkDependencyFlags		dependencyFlags;
			};

			subpassDependencies.push_back(spdVal);
		}

		const VkRenderPassCreateInfo renderPassParams =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,			// VkStructureType					sType;
			DE_NULL,											// const void*						pNext;
			0u,													// VkRenderPassCreateFlags			flags;
			(deUint32)attachmentDescriptions.size(),			// deUint32							attachmentCount;
			&attachmentDescriptions[0],							// const VkAttachmentDescription*	pAttachments;
			1u,													// deUint32							subpassCount;
			&subpassDescription,								// const VkSubpassDescription*		pSubpasses;
			static_cast<uint32_t>(subpassDependencies.size()),	// deUint32							dependencyCount;
			de::dataOrNull(subpassDependencies),				// const VkSubpassDependency*		pDependencies;
		};

		m_renderPass = RenderPassWrapper(m_pipelineConstructionType, vk, vkDevice, &renderPassParams);
	}

	// Create framebuffer
	{
		std::vector<VkImage> images(m_imageCount);
		std::vector<VkImageView> pAttachments(m_imageCount);
		for (int imgNdx = 0; imgNdx < m_imageCount; ++imgNdx)
		{
			images[imgNdx] = m_dsImages[imgNdx]->get();
			pAttachments[imgNdx] = m_dsAttachmentViews[imgNdx]->get();
		}

		const VkFramebufferCreateInfo framebufferParams =
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,			// VkStructureType			sType;
			DE_NULL,											// const void*				pNext;
			0u,													// VkFramebufferCreateFlags	flags;
			*m_renderPass,										// VkRenderPass				renderPass;
			(deUint32)m_imageCount,								// deUint32					attachmentCount;
			&pAttachments[0],									// const VkImageView*		pAttachments;
			(deUint32)renderSize.x(),							// deUint32					width;
			(deUint32)renderSize.y(),							// deUint32					height;
			1u													// deUint32					layers;
		};

		m_renderPass.createFramebuffer(vk, vkDevice, &framebufferParams, images);
	}

	// Create pipeline layouts
	{
		const VkPipelineLayoutCreateInfo pipelineLayoutParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			0u,													// VkPipelineLayoutCreateFlags	flags;
			0u,													// deUint32						setLayoutCount;
			DE_NULL,											// const VkDescriptorSetLayout*	pSetLayouts;
			0u,													// deUint32						pushConstantRangeCount;
			DE_NULL												// const VkPushConstantRange*	pPushConstantRanges;
		};

		m_preRasterizationStatePipelineLayout = PipelineLayoutWrapper(m_pipelineConstructionType, vk, vkDevice, &pipelineLayoutParams);
	}
	{
		const VkPipelineLayoutCreateInfo pipelineLayoutParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT,	// VkPipelineLayoutCreateFlags	flags;
			0u,													// deUint32						setLayoutCount;
			DE_NULL,											// const VkDescriptorSetLayout*	pSetLayouts;
			0u,													// deUint32						pushConstantRangeCount;
			DE_NULL												// const VkPushConstantRange*	pPushConstantRanges;
		};

		m_preRasterizationStatePipelineLayout = PipelineLayoutWrapper(m_pipelineConstructionType, vk, vkDevice, &pipelineLayoutParams);
	}
	{
		const VkPipelineLayoutCreateInfo pipelineLayoutParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT,	// VkPipelineLayoutCreateFlags	flags;
			1u,													// deUint32						setLayoutCount;
			&m_descriptorSetLayout.get(),						// const VkDescriptorSetLayout*	pSetLayouts;
			0u,													// deUint32						pushConstantRangeCount;
			DE_NULL												// const VkPushConstantRange*	pPushConstantRanges;
		};

		m_fragmentStatePipelineLayout = PipelineLayoutWrapper(m_pipelineConstructionType, vk, vkDevice, &pipelineLayoutParams);
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

		const std::vector<VkViewport>	viewports	(1, makeViewport(renderSize));
		const std::vector<VkRect2D>		scissors	(1, makeRect2D(renderSize));

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
			colorBlendAttachmentStates[imgNdx].colorWriteMask		= 0u;													// VkColorComponentFlags	colorWriteMask;
		}

		const VkPipelineColorBlendStateCreateInfo colorBlendStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType								sType;
			DE_NULL,													// const void*									pNext;
			0u,															// VkPipelineColorBlendStateCreateFlags			flags;
			false,														// VkBool32										logicOpEnable;
			VK_LOGIC_OP_COPY,											// VkLogicOp									logicOp;
			0u,															// deUint32										attachmentCount;
			DE_NULL,													// const VkPipelineColorBlendAttachmentState*	pAttachments;
			{ 0.0f, 0.0f, 0.0f, 0.0f }									// float										blendConstants[4];
		};

		VkBool32 depthTestEnable =
			((m_subresourceRange.aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT) && !m_interleaveReadWriteComponents) ||
			((m_subresourceRange.aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT) && m_interleaveReadWriteComponents);

		VkBool32 stencilTestEnable =
			((m_subresourceRange.aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT) && !m_interleaveReadWriteComponents) ||
			((m_subresourceRange.aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT) && m_interleaveReadWriteComponents);

		const auto stencilFrontOpState	= makeStencilOpState(vk::VK_STENCIL_OP_KEEP, vk::VK_STENCIL_OP_REPLACE, vk::VK_STENCIL_OP_KEEP, vk::VK_COMPARE_OP_NEVER, 0xFFu, 0xFFu, 0u);
		const auto stencilBackOpState	= makeStencilOpState(vk::VK_STENCIL_OP_KEEP, vk::VK_STENCIL_OP_REPLACE, vk::VK_STENCIL_OP_KEEP, vk::VK_COMPARE_OP_ALWAYS, 0xFFu, 0xFFu, 0u);

		const VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,							// VkStructureType							sType
			DE_NULL,																			// const void*								pNext
			0u,																					// VkPipelineDepthStencilStateCreateFlags	flags
			depthTestEnable,																	// VkBool32									depthTestEnable
			depthTestEnable,																	// VkBool32									depthWriteEnable
			VK_COMPARE_OP_ALWAYS,																// VkCompareOp								depthCompareOp
			DE_FALSE,																			// VkBool32									depthBoundsTestEnable
			stencilTestEnable,																	// VkBool32									stencilTestEnable
			stencilFrontOpState,																// VkStencilOpState							front
			stencilBackOpState,																	// VkStencilOpState							back
			0.0f,																				// float									minDepthBounds
			1.0f,																				// float									maxDepthBounds;
		};

		std::vector<VkDynamicState> dynamicStates;
		if (m_pipelineStateMode != PipelineStateMode::STATIC)
			dynamicStates.push_back(VK_DYNAMIC_STATE_ATTACHMENT_FEEDBACK_LOOP_ENABLE_EXT);

		const VkPipelineDynamicStateCreateInfo dynamicStateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			nullptr,
			0u,
			de::sizeU32(dynamicStates),
			de::dataOrNull(dynamicStates),
		};

		if (m_useMaintenance5)
			m_graphicsPipeline.setPipelineCreateFlags2(translateCreateFlag(m_params.pipelineCreateFlags));

		m_graphicsPipeline.setDynamicState(&dynamicStateInfo)
						  .setMonolithicPipelineLayout(m_fragmentStatePipelineLayout)
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
						  .setupFragmentShaderState(m_fragmentStatePipelineLayout, *m_renderPass, 0u, m_fragmentShaderModule, &depthStencilStateCreateInfo)
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

	// Create command buffer
	{
		std::vector<VkImageMemoryBarrier> preAttachmentBarriers(m_imageCount);

		for (int imgNdx = 0; imgNdx < m_imageCount; ++imgNdx)
		{
			preAttachmentBarriers[imgNdx].sType								= VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;								// VkStructureType			sType;
			preAttachmentBarriers[imgNdx].pNext								= DE_NULL;																// const void*				pNext;
			preAttachmentBarriers[imgNdx].srcAccessMask						= VK_ACCESS_TRANSFER_WRITE_BIT;																	// VkAccessFlags			srcAccessMask;
			preAttachmentBarriers[imgNdx].dstAccessMask						= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;	// VkAccessFlags			dstAccessMask;
			preAttachmentBarriers[imgNdx].oldLayout							= m_imageLayout;														// VkImageLayout			oldLayout;
			preAttachmentBarriers[imgNdx].newLayout							= m_imageLayout;														// VkImageLayout			newLayout;
			preAttachmentBarriers[imgNdx].srcQueueFamilyIndex				= VK_QUEUE_FAMILY_IGNORED;												// deUint32					srcQueueFamilyIndex;
			preAttachmentBarriers[imgNdx].dstQueueFamilyIndex				= VK_QUEUE_FAMILY_IGNORED;												// deUint32					dstQueueFamilyIndex;
			preAttachmentBarriers[imgNdx].image								= **m_dsImages[imgNdx];													// VkImage					image;
			preAttachmentBarriers[imgNdx].subresourceRange.aspectMask		= getAspectFlags(m_imageFormat);										// VkImageSubresourceRange	subresourceRange;
			preAttachmentBarriers[imgNdx].subresourceRange.baseMipLevel		= 0u;
			preAttachmentBarriers[imgNdx].subresourceRange.levelCount		= 1u;
			preAttachmentBarriers[imgNdx].subresourceRange.baseArrayLayer	= 0u;
			preAttachmentBarriers[imgNdx].subresourceRange.layerCount		= 1u;
		}

		beginCommandBuffer(vk, *m_cmdBuffer, 0u);

		vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, (VkDependencyFlags)0,
			0u, DE_NULL, 0u, DE_NULL, (deUint32)m_imageCount, &preAttachmentBarriers[0]);

		// Do not clear the color attachments as we are using the texture as color attachment.
		m_renderPass.begin(vk, *m_cmdBuffer, makeRect2D(0, 0, renderSize.x(), renderSize.y()), 0u, DE_NULL);

		m_graphicsPipeline.bind(*m_cmdBuffer);

		vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_fragmentStatePipelineLayout, 0, 1, &m_descriptorSet.get(), 0, DE_NULL);

		const VkDeviceSize vertexBufferOffset = 0;
		vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);

		if (m_pipelineStateMode != PipelineStateMode::STATIC)
			vk.cmdSetAttachmentFeedbackLoopEnableEXT(*m_cmdBuffer, testModeToAspectFlags(m_imageAspectTestMode));

		vk.cmdDraw(*m_cmdBuffer, (deUint32)m_vertices.size(), 1, 0, 0);

		m_renderPass.end(vk, *m_cmdBuffer);
		endCommandBuffer(vk, *m_cmdBuffer);
	}
}

tcu::TestStatus AttachmentFeedbackLoopLayoutDepthStencilImageSamplingInstance::verifyImage(void)
{
	const tcu::TextureFormat	tcuFormat		= getSizeCompatibleTcuTextureFormat(m_imageFormat);
	const bool					isDepth			= (!m_interleaveReadWriteComponents && (m_subresourceRange.aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT)) ||
												   (m_interleaveReadWriteComponents && (m_subresourceRange.aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT));
	const bool					isStencil		= (!m_interleaveReadWriteComponents && (m_subresourceRange.aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT)) ||
												   (m_interleaveReadWriteComponents && (m_subresourceRange.aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT));
	// ImageSamplingInstance::verifyImage() doesn't support stencil sampling.
	if (!m_useImageAsColorOrDSAttachment && !isStencil)
		return ImageSamplingInstance::verifyImage();

	const tcu::Vec4	fThreshold (0.005f);
	const tcu::UVec4 uThreshold (0u); // Due to unsigned normalized fixed-point integers conversion to floats and viceversa.
	tcu::UVec2 renderSize = tcu::UVec2({ (unsigned)m_imageSize.x(), (unsigned)m_imageSize.y() });

	de::MovePtr<tcu::TextureLevel> referenceTextureLevel = de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(tcuFormat,
																							  m_imageSize.x(),
																							  m_imageSize.y(),
																							  m_imageSize.z()));

	for (int z = 0; z < m_imageSize.z(); z++)
		for (int y = 0; y < m_imageSize.y(); y++)
			for (int x = 0; x < m_imageSize.x(); x++)
			{
				if (isDepth)
				{
					float depth = 0.0f;
					if (m_interleaveReadWriteComponents)
					{
						int stencil = 1 + m_texture->getLevel(0, 0).getPixStencil(x, y, z);
						depth = static_cast<float>(stencil) / 255.0f;
					}
					else
					{
						if (m_useDifferentAreasSampleWrite && x < m_imageSize.x() / 2)
							depth = m_texture->getLevel(0, 0).getPixDepth(x + (m_imageSize.x() / 2), y, z) + 0.1f;
						else
							depth = m_texture->getLevel(0, 0).getPixDepth(x, y, z);

						if (!m_useDifferentAreasSampleWrite)
							depth += 0.1f;
					}

					depth = deFloatClamp(depth, 0.0f, 1.0f);
					referenceTextureLevel->getAccess().setPixDepth(depth, x, y, z);
				}
				if (isStencil)
				{
					int stencil = 0;
					if (m_interleaveReadWriteComponents)
					{
						float depth = m_texture->getLevel(0, 0).getPixDepth(x, y, z) + 0.1f;
						stencil = static_cast<int>(depth * 255.0f);
					}
					else
					{
						if (m_useDifferentAreasSampleWrite && x < m_imageSize.x() / 2)
							stencil = 1 + m_texture->getLevel(0, 0).getPixStencil(x + (m_imageSize.x() / 2), y, z);
						else
							stencil = m_texture->getLevel(0, 0).getPixStencil(x, y, z);

						if (!m_useDifferentAreasSampleWrite)
							stencil += 1;

						stencil = deClamp32(stencil, 0, 255);
					}

					referenceTextureLevel->getAccess().setPixStencil(stencil, x, y, z);
				}
			}

	for (int imgNdx = 0; imgNdx < m_imageCount; ++imgNdx)
	{
		if (isDepth)
		{
			// Read back result image
			de::MovePtr<tcu::TextureLevel>			resultTexture			(readDepthAttachment(m_context.getDeviceInterface(),
																				m_context.getDevice(),
																				m_context.getUniversalQueue(),
																				m_context.getUniversalQueueFamilyIndex(),
																				m_context.getDefaultAllocator(),
																				**m_dsImages[imgNdx],
																				m_imageFormat,
																				renderSize,
																				VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT));

			const tcu::ConstPixelBufferAccess		result	= resultTexture->getAccess();
			const tcu::Sampler::DepthStencilMode	mode				= tcu::Sampler::MODE_DEPTH;
			const tcu::ConstPixelBufferAccess		depthResult			= tcu::getEffectiveDepthStencilAccess(result, mode);
			const tcu::ConstPixelBufferAccess		expectedResult		= tcu::getEffectiveDepthStencilAccess(referenceTextureLevel->getAccess(), mode);
			bool									isIntegerFormat		= isUintFormat(mapTextureFormat(depthResult.getFormat())) || isIntFormat(mapTextureFormat(depthResult.getFormat()));

			if (!isIntegerFormat)
			{
				if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison", expectedResult, depthResult, fThreshold, tcu::COMPARE_LOG_RESULT))
					return tcu::TestStatus::fail("Failed depth");
			}
			else
			{
				if (!tcu::intThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison", expectedResult, depthResult, uThreshold, tcu::COMPARE_LOG_RESULT))
					return tcu::TestStatus::fail("Failed depth");
			}
		}

		if (isStencil)
		{
			// Read back result image
			de::MovePtr<tcu::TextureLevel>			resultTexture			(readStencilAttachment(m_context.getDeviceInterface(),
																				m_context.getDevice(),
																				m_context.getUniversalQueue(),
																				m_context.getUniversalQueueFamilyIndex(),
																				m_context.getDefaultAllocator(),
																				**m_dsImages[imgNdx],
																				m_imageFormat,
																				renderSize,
																				VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT));

			const tcu::ConstPixelBufferAccess		result	= resultTexture->getAccess();
			const tcu::Sampler::DepthStencilMode	mode				= tcu::Sampler::MODE_STENCIL;
			const tcu::ConstPixelBufferAccess		stencilResult		= tcu::getEffectiveDepthStencilAccess(result, mode);
			const tcu::ConstPixelBufferAccess		expectedResult		= tcu::getEffectiveDepthStencilAccess(referenceTextureLevel->getAccess(), mode);
			bool									isIntegerFormat		= isUintFormat(mapTextureFormat(stencilResult.getFormat())) || isIntFormat(mapTextureFormat(stencilResult.getFormat()));

			if (!isIntegerFormat)
			{
				if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison", expectedResult, stencilResult, fThreshold, tcu::COMPARE_LOG_RESULT))
					return tcu::TestStatus::fail("Failed stencil");
			}
			else
			{
				if (!tcu::intThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison", expectedResult, stencilResult, uThreshold, tcu::COMPARE_LOG_RESULT))
					return tcu::TestStatus::fail("Failed stencil");
			}
		}
	}

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus AttachmentFeedbackLoopLayoutDepthStencilImageSamplingInstance::iterate (void)
{
	const DeviceInterface&		vk			= m_context.getDeviceInterface();
	const VkDevice				vkDevice	= m_context.getDevice();
	const VkQueue				queue		= m_context.getUniversalQueue();

	setup();
	submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());

	return verifyImage();
}

tcu::TestStatus AttachmentFeedbackLoopLayoutImageSamplingInstance::verifyImage(void)
{
	if (!m_useImageAsColorOrDSAttachment)
		return ImageSamplingInstance::verifyImage();

	const tcu::Vec4	fThreshold (0.01f);
	const tcu::UVec4 uThreshold (1u);
	tcu::UVec2 renderSize = tcu::UVec2({ (unsigned)m_imageSize.x(), (unsigned)m_imageSize.y() });

	const tcu::TextureFormat	tcuFormat		= getSizeCompatibleTcuTextureFormat(m_imageFormat);
	de::MovePtr<tcu::TextureLevel> referenceTextureLevel = de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(tcuFormat,
																												m_imageSize.x(),
																												m_imageSize.y(),
																												m_imageSize.z()));

	for (int z = 0; z < m_imageSize.z(); z++)
		for (int y = 0; y < m_imageSize.y(); y++)
			for (int x = 0; x < m_imageSize.x(); x++)
			{
				tcu::Vec4 color = tcu::Vec4(1.0f);

				if (m_useDifferentAreasSampleWrite && (x < m_imageSize.x() / 2))
					color = m_texture->getLevel(0, 0).getPixel(x + (m_imageSize.x() / 2), y, z) + tcu::Vec4(0.1f);
				else
					color = m_texture->getLevel(0, 0).getPixel(x, y, z);

				if (!m_useDifferentAreasSampleWrite)
					color += tcu::Vec4(0.1f);

				if (m_interleaveReadWriteComponents)
				{
					tcu::Vec4 sampledColor = m_texture->getLevel(0, 0).getPixel(x, y, z);
					color.x() = color.y();
					color.y() = sampledColor.y();
					color.z() = color.w();
					color.w() = sampledColor.w();
				}

				color.x() = deFloatClamp(color.x(), 0.0f, 1.0f);
				color.y() = deFloatClamp(color.y(), 0.0f, 1.0f);
				color.z() = deFloatClamp(color.z(), 0.0f, 1.0f);
				color.w() = deFloatClamp(color.w(), 0.0f, 1.0f);

				referenceTextureLevel->getAccess().setPixel(color, x, y, z);
			}

	for (int imgNdx = 0; imgNdx < m_imageCount; ++imgNdx)
	{
		// Read back result image
		de::MovePtr<tcu::TextureLevel>		resultTexture			(readColorAttachment(m_context.getDeviceInterface(),
																						 m_context.getDevice(),
																						 m_context.getUniversalQueue(),
																						 m_context.getUniversalQueueFamilyIndex(),
																						 m_context.getDefaultAllocator(),
																						 **m_colorImages[imgNdx],
																						 m_colorFormat,
																						 renderSize,
																						 vk::VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT));
		const tcu::ConstPixelBufferAccess	result	= resultTexture->getAccess();
		const bool							isIntegerFormat	= isUintFormat(m_imageFormat) || isIntFormat(m_imageFormat);

		if (!isIntegerFormat)
		{
			if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison", referenceTextureLevel->getAccess(), result, fThreshold, tcu::COMPARE_LOG_RESULT))
				return tcu::TestStatus::fail("Failed color");
		}
		else
		{
			if (!tcu::intThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison", referenceTextureLevel->getAccess(), result, uThreshold, tcu::COMPARE_LOG_RESULT))
				return tcu::TestStatus::fail("Failed color");
		}
	}

	return tcu::TestStatus::pass("Pass");
}

AttachmentFeedbackLoopLayoutImageSamplingInstance::~AttachmentFeedbackLoopLayoutImageSamplingInstance (void)
{
}

tcu::TestStatus AttachmentFeedbackLoopLayoutImageSamplingInstance::iterate (void)
{
	const DeviceInterface&		vk			= m_context.getDeviceInterface();
	const VkDevice				vkDevice	= m_context.getDevice();
	const VkQueue				queue		= m_context.getUniversalQueue();

	setup();
	submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());

	return verifyImage();
}

class AttachmentFeedbackLoopLayoutSamplerTest : public vkt::TestCase {
public:
										AttachmentFeedbackLoopLayoutSamplerTest			(tcu::TestContext&				testContext,
																						 vk::PipelineConstructionType	pipelineConstructionType,
																						 const char*					name,
																						 const char*					description,
																						 SamplerViewType				imageViewType,
																						 VkFormat						imageFormat,
																						 int							imageSize,
																						 VkDescriptorType				imageDescriptorType,
																						 float							samplerLod,
																						 TestMode						testMode,
																						 ImageAspectTestMode			imageAspectTestMode,
																						 bool							interleaveReadWriteComponents,
																						 PipelineStateMode				pipelineStateMode,
																						 bool							useMaintenance5);
	virtual								~AttachmentFeedbackLoopLayoutSamplerTest		(void) {}

	virtual ImageSamplingInstanceParams	getImageSamplingInstanceParams	(SamplerViewType	imageViewType,
																		 VkFormat			imageFormat,
																		 int				imageSize,
																		 VkDescriptorType	imageDescriptorType,
																		 float				samplerLod) const;

	virtual void						initPrograms					(SourceCollections& sourceCollections) const;
	virtual void						checkSupport					(Context& context) const;
	virtual TestInstance*				createInstance					(Context& context) const;
	virtual tcu::UVec2					getRenderSize					(SamplerViewType viewType) const;
	virtual std::vector<Vertex4Tex4>	createVertices					(void) const;
	virtual VkSamplerCreateInfo			getSamplerCreateInfo			(void) const;
	virtual VkComponentMapping			getComponentMapping				(void) const;

	static std::string					getGlslSamplerType				(const tcu::TextureFormat& format, SamplerViewType type);
	static tcu::IVec3					getImageSize					(SamplerViewType viewType, int size);
	static int							getArraySize					(SamplerViewType viewType);

	static std::string					getGlslSampler (const tcu::TextureFormat& format, VkImageViewType type, VkDescriptorType samplingType, int imageCount);
	static std::string					getGlslTextureType (const tcu::TextureFormat& format, VkImageViewType type);
	static std::string					getGlslSamplerDecl (int imageCount);
	static std::string					getGlslTextureDecl (int imageCount);

protected:
	vk::PipelineConstructionType		m_pipelineConstructionType;
	SamplerViewType						m_imageViewType;
	VkFormat							m_imageFormat;
	int									m_imageSize;
	VkDescriptorType					m_imageDescriptorType;
	float								m_samplerLod;
	TestMode							m_testMode;
	ImageAspectTestMode					m_imageAspectTestMode;
	bool								m_interleaveReadWriteComponents;
	PipelineStateMode					m_pipelineStateMode;
	bool								m_useMaintenance5;
};

// AttachmentFeedbackLoopLayoutSamplerTest

AttachmentFeedbackLoopLayoutSamplerTest::AttachmentFeedbackLoopLayoutSamplerTest	(tcu::TestContext&				testContext,
																					 vk::PipelineConstructionType	pipelineConstructionType,
																					 const char*					name,
																					 const char*					description,
																					 SamplerViewType				imageViewType,
																					 VkFormat						imageFormat,
																					 int							imageSize,
																					 VkDescriptorType				imageDescriptorType,
																					 float							samplerLod,
																					 TestMode						testMode,
																					 ImageAspectTestMode			imageAspectTestMode,
																					 bool							interleaveReadWriteComponents,
																					 PipelineStateMode				pipelineStateMode,
																					 bool							useMaintenance5)
	: vkt::TestCase					(testContext, name, description)
	, m_pipelineConstructionType	(pipelineConstructionType)
	, m_imageViewType				(imageViewType)
	, m_imageFormat					(imageFormat)
	, m_imageSize					(imageSize)
	, m_imageDescriptorType			(imageDescriptorType)
	, m_samplerLod					(samplerLod)
	, m_testMode					(testMode)
	, m_imageAspectTestMode			(imageAspectTestMode)
	, m_interleaveReadWriteComponents	(interleaveReadWriteComponents)
	, m_pipelineStateMode			(pipelineStateMode)
	, m_useMaintenance5				(useMaintenance5)
{
}

ImageSamplingInstanceParams AttachmentFeedbackLoopLayoutSamplerTest::getImageSamplingInstanceParams (SamplerViewType	imageViewType,
																									 VkFormat			imageFormat,
																									 int				imageSize,
																									 VkDescriptorType	imageDescriptorType,
																									 float				samplerLod) const
{
	const tcu::UVec2				renderSize			= getRenderSize(imageViewType);
	const std::vector<Vertex4Tex4>	vertices			= createVertices();
	const VkSamplerCreateInfo		samplerParams		= getSamplerCreateInfo();
	const VkComponentMapping		componentMapping	= getComponentMapping();

	VkImageAspectFlags				imageAspect			= 0u;
	VkPipelineCreateFlags			pipelineCreateFlags = 0u;

	if (!isCompressedFormat(imageFormat))
	{
		if (m_imageAspectTestMode == IMAGE_ASPECT_TEST_COLOR)
		{
			DE_ASSERT(!tcu::hasDepthComponent(mapVkFormat(imageFormat).order) &&
					  !tcu::hasStencilComponent(mapVkFormat(imageFormat).order));
		}
		else if (m_imageAspectTestMode == IMAGE_ASPECT_TEST_DEPTH)
			DE_ASSERT(tcu::hasDepthComponent(mapVkFormat(imageFormat).order));
		else if (m_imageAspectTestMode == IMAGE_ASPECT_TEST_STENCIL)
			DE_ASSERT(tcu::hasStencilComponent(mapVkFormat(imageFormat).order));

		imageAspect			= testModeToAspectFlags(m_imageAspectTestMode);
		pipelineCreateFlags	= getStaticPipelineCreateFlags(imageAspect, m_pipelineStateMode);
	}
	else
	{
		imageAspect = VK_IMAGE_ASPECT_COLOR_BIT;
	}

	const VkImageSubresourceRange	subresourceRange	=
	{
		imageAspect,										// VkImageAspectFlags	aspectMask;
		0u,													// deUint32				baseMipLevel;
		1u,													// deUint32				mipLevels;
		0u,													// deUint32				baseArrayLayer;
		(deUint32)getArraySize(imageViewType)				// deUint32				arraySize;
	};

	return ImageSamplingInstanceParams(m_pipelineConstructionType, renderSize, imageViewType, imageFormat,
									   getImageSize(imageViewType, imageSize),
									   getArraySize(imageViewType),
									   componentMapping, subresourceRange,
									   samplerParams, samplerLod, vertices, false,
									   imageDescriptorType, 1u, ALLOCATION_KIND_SUBALLOCATED,
									   vk::VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT,
									   pipelineCreateFlags);
}

void AttachmentFeedbackLoopLayoutSamplerTest::checkSupport (Context& context) const
{
	context.requireDeviceFunctionality("VK_EXT_attachment_feedback_loop_layout");
	if (m_useMaintenance5)
		context.requireDeviceFunctionality("VK_KHR_maintenance5");

	if (m_pipelineStateMode != PipelineStateMode::STATIC)
		context.requireDeviceFunctionality("VK_EXT_attachment_feedback_loop_dynamic_state");

	checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_pipelineConstructionType);

	vk::VkPhysicalDeviceAttachmentFeedbackLoopLayoutFeaturesEXT attachmentFeedbackLoopLayoutFeatures =
	{
		vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ATTACHMENT_FEEDBACK_LOOP_LAYOUT_FEATURES_EXT,	// VkStructureType	sType;
		DE_NULL,																				// void*			pNext;
		DE_FALSE,																				// VkBool32		attachmentFeedbackLoopLayout;
	};

	vk::VkPhysicalDeviceFeatures2 features2;
	deMemset(&features2, 0, sizeof(features2));
	features2.sType = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features2.pNext = &attachmentFeedbackLoopLayoutFeatures;

	context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &features2);

	if (attachmentFeedbackLoopLayoutFeatures.attachmentFeedbackLoopLayout == DE_FALSE)
	{
		throw tcu::NotSupportedError("attachmentFeedbackLoopLayout not supported");
	}

	ImageSamplingInstanceParams	params = getImageSamplingInstanceParams(m_imageViewType, m_imageFormat, m_imageSize, m_imageDescriptorType, m_samplerLod);
	checkSupportImageSamplingInstance(context, params);

	bool useImageAsColorOrDSAttachment	= m_testMode >= TEST_MODE_READ_WRITE_SAME_PIXEL;
	if (useImageAsColorOrDSAttachment)
	{
		VkFormatProperties	formatProps;
		const InstanceInterface& instanceInterface = context.getInstanceInterface();
		VkFormatFeatureFlags attachmentFormatFeature = isDepthStencilFormat(params.imageFormat) ?
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;

		instanceInterface.getPhysicalDeviceFormatProperties(context.getPhysicalDevice(), params.imageFormat, &formatProps);
		bool error =
			(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT ) == 0u ||
			(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT ) == 0u ||
			(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_DST_BIT ) == 0u ||
			(formatProps.optimalTilingFeatures & attachmentFormatFeature ) == 0u;

		if (error)
		{
			throw tcu::NotSupportedError("format doesn't support some required features");
		}

		if ((!m_interleaveReadWriteComponents && m_imageAspectTestMode == IMAGE_ASPECT_TEST_STENCIL) ||
			(m_interleaveReadWriteComponents && m_imageAspectTestMode == IMAGE_ASPECT_TEST_DEPTH))
			context.requireDeviceFunctionality("VK_EXT_shader_stencil_export");
	}
}

std::string AttachmentFeedbackLoopLayoutSamplerTest::getGlslTextureType (const tcu::TextureFormat& format, VkImageViewType type)
{
	std::ostringstream textureType;

	if (tcu::getTextureChannelClass(format.type) == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER)
		textureType << "u";
	else if (tcu::getTextureChannelClass(format.type) == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER)
		textureType << "i";

	switch (type)
	{
		case VK_IMAGE_VIEW_TYPE_1D:
			textureType << "texture1D";
			break;

		case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
			textureType << "texture1DArray";
			break;

		case VK_IMAGE_VIEW_TYPE_2D:
			textureType << "texture2D";
			break;

		case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
			textureType << "texture2DArray";
			break;

		case VK_IMAGE_VIEW_TYPE_3D:
			textureType << "texture3D";
			break;

		case VK_IMAGE_VIEW_TYPE_CUBE:
			textureType << "textureCube";
			break;

		case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
			textureType << "textureCubeArray";
			break;

		default:
			DE_FATAL("Unknown image view type");
	}

	return textureType.str();
}

std::string AttachmentFeedbackLoopLayoutSamplerTest::getGlslSamplerDecl (int imageCount)
{
	std::ostringstream samplerArray;
	samplerArray << "texSamplers[" << imageCount << "]";

	return imageCount > 1 ? samplerArray.str() : "texSampler";
}

std::string AttachmentFeedbackLoopLayoutSamplerTest::getGlslTextureDecl (int imageCount)
{
	std::ostringstream textureArray;
	textureArray << "texImages[" << imageCount << "]";

	return imageCount > 1 ? textureArray.str() : "texImage";
}

std::string AttachmentFeedbackLoopLayoutSamplerTest::getGlslSampler (const tcu::TextureFormat& format, VkImageViewType type, VkDescriptorType samplingType, int imageCount)
{
	std::string texSampler	= imageCount > 1 ? "texSamplers[i]" : "texSampler";
	std::string texImage	= imageCount > 1 ? "texImages[i]" : "texImage";

	switch (samplingType)
	{
		case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
			return getGlslSamplerType(format, type) + "(" + texImage + ", texSampler)";
		case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
		default:
			return texSampler;
	}
}

void AttachmentFeedbackLoopLayoutSamplerTest::initPrograms (SourceCollections& sourceCollections) const
{
	std::ostringstream				vertexSrc;
	std::ostringstream				fragmentSrc;
	const char*						texCoordSwizzle	= DE_NULL;
	const VkFormat					vkFormat = m_imageAspectTestMode == IMAGE_ASPECT_TEST_STENCIL ? VK_FORMAT_S8_UINT : m_imageFormat;
	const tcu::TextureFormat		format			= (isCompressedFormat(m_imageFormat)) ? tcu::getUncompressedFormat(mapVkCompressedFormat(vkFormat))
																						  : mapVkFormat(vkFormat);
	tcu::Vec4						lookupScale;
	tcu::Vec4						lookupBias;

	getLookupScaleBias(m_imageFormat, lookupScale, lookupBias);

	switch (m_imageViewType)
	{
		case VK_IMAGE_VIEW_TYPE_1D:
			texCoordSwizzle = "x";
			break;
		case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
		case VK_IMAGE_VIEW_TYPE_2D:
			texCoordSwizzle = "xy";
			break;
		case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
		case VK_IMAGE_VIEW_TYPE_3D:
		case VK_IMAGE_VIEW_TYPE_CUBE:
			texCoordSwizzle = "xyz";
			break;
		case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
			texCoordSwizzle = "xyzw";
			break;
		default:
			DE_ASSERT(false);
			break;
	}

	vertexSrc << "#version 440\n"
			  << "layout(location = 0) in vec4 position;\n"
			  << "layout(location = 1) in vec4 texCoords;\n"
			  << "layout(location = 0) out highp vec4 vtxTexCoords;\n"
			  << "out gl_PerVertex {\n"
			  << "	vec4 gl_Position;\n"
			  << "};\n"
			  << "void main (void)\n"
			  << "{\n"
			  << "	gl_Position = position;\n"
			  << "	vtxTexCoords = texCoords;\n"
			  << "}\n";

	fragmentSrc << "#version 440\n";

	if ((m_imageAspectTestMode == IMAGE_ASPECT_TEST_STENCIL && m_testMode >= TEST_MODE_READ_WRITE_SAME_PIXEL) ||
		(m_imageAspectTestMode == IMAGE_ASPECT_TEST_DEPTH && m_interleaveReadWriteComponents))
	{
		fragmentSrc << "#extension GL_ARB_shader_stencil_export: require\n";
	}

	switch (m_imageDescriptorType)
	{
		case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
			fragmentSrc
				<< "layout(set = 0, binding = 0) uniform highp sampler texSampler;\n"
				<< "layout(set = 0, binding = 1) uniform highp " << getGlslTextureType(format, m_imageViewType) << " " << getGlslTextureDecl(1u) << ";\n";
			break;
		case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
		default:
			fragmentSrc
				<< "layout(set = 0, binding = 0) uniform highp " << getGlslSamplerType(format, m_imageViewType) << " " << getGlslSamplerDecl(1u) << ";\n";
	}

	if (m_imageAspectTestMode == IMAGE_ASPECT_TEST_COLOR || m_testMode == TEST_MODE_READ_ONLY)
		fragmentSrc	<< "layout(location = 0) out highp vec4 fragColor;\n";

	fragmentSrc	<< "layout(location = 0) in highp vec4 vtxTexCoords;\n"
				<< "void main (void)\n"
				<< "{\n";

	if (m_imageAspectTestMode == IMAGE_ASPECT_TEST_STENCIL && m_testMode != TEST_MODE_READ_ONLY)
		fragmentSrc	<< "	uvec4 read_data = ";
	else
		fragmentSrc	<< "	vec4 read_data = ";

	if (m_imageAspectTestMode == IMAGE_ASPECT_TEST_DEPTH && m_testMode >= TEST_MODE_READ_WRITE_SAME_PIXEL)
	{
		fragmentSrc << "vec4(1.0f, 0.0f, 0.0f, 1.0f);\n";

		fragmentSrc << "	read_data.x = ";
		if (m_samplerLod > 0.0f)
		{
			DE_ASSERT(m_imageViewType.isNormalized());
			fragmentSrc << "textureLod(" << getGlslSampler(format, m_imageViewType, m_imageDescriptorType, 1u) << ", vtxTexCoords." << texCoordSwizzle << ", " << std::fixed <<  m_samplerLod << ").x";
		}
		else
		{
			if (m_imageViewType.isNormalized())
				fragmentSrc << "texture(" << getGlslSampler(format, m_imageViewType, m_imageDescriptorType, 1u) << ", vtxTexCoords." << texCoordSwizzle << ").x" << std::fixed;
			else
				fragmentSrc << "textureLod(" << getGlslSampler(format, m_imageViewType, m_imageDescriptorType, 1u) << ", vtxTexCoords." << texCoordSwizzle << ", 0).x" << std::fixed;
		}

		fragmentSrc << " + 0.1f;\n";
	}
	else if (m_imageAspectTestMode == IMAGE_ASPECT_TEST_STENCIL && m_testMode == TEST_MODE_READ_ONLY)
	{
		if (m_samplerLod > 0.0f)
		{
			DE_ASSERT(m_imageViewType.isNormalized());
			fragmentSrc << "vec4(textureLod(" << getGlslSampler(format, m_imageViewType, m_imageDescriptorType, 1u) << ", vtxTexCoords." << texCoordSwizzle << ", " << std::fixed <<  m_samplerLod << ").x / 255.0f, 0.0f, 0.0f, 1.0f)";
		}
		else
		{
			if (m_imageViewType.isNormalized())
				fragmentSrc << "vec4(texture(" << getGlslSampler(format, m_imageViewType, m_imageDescriptorType, 1u) << ", vtxTexCoords." << texCoordSwizzle << ").x / 255.0f, 0.0f, 0.0f, 1.0f)" << std::fixed;
			else
				fragmentSrc << "vec4(textureLod(" << getGlslSampler(format, m_imageViewType, m_imageDescriptorType, 1u) << ", vtxTexCoords." << texCoordSwizzle << ", 0).x / 255.0f, 0.0f, 0.0f, 1.0f)" << std::fixed;
		}

		fragmentSrc << ";\n";
	}
	else
	{
		if (m_samplerLod > 0.0f)
		{
			DE_ASSERT(m_imageViewType.isNormalized());
			fragmentSrc << "textureLod(" << getGlslSampler(format, m_imageViewType, m_imageDescriptorType, 1u) << ", vtxTexCoords." << texCoordSwizzle << ", " << std::fixed <<  m_samplerLod << ")";
		}
		else
		{
			if (m_imageViewType.isNormalized())
				fragmentSrc << "texture(" << getGlslSampler(format, m_imageViewType, m_imageDescriptorType, 1u) << ", vtxTexCoords." << texCoordSwizzle << ")" << std::fixed;
			else
				fragmentSrc << "textureLod(" << getGlslSampler(format, m_imageViewType, m_imageDescriptorType, 1u) << ", vtxTexCoords." << texCoordSwizzle << ", 0)" << std::fixed;
		}

		if (m_testMode >= TEST_MODE_READ_WRITE_SAME_PIXEL)
		{
			if (m_imageAspectTestMode == IMAGE_ASPECT_TEST_STENCIL)
				fragmentSrc << " + uvec4(1u, 0u, 0u, 0)";
			else
				fragmentSrc << " + vec4(0.1f)";
		}

		fragmentSrc << ";\n";
	}

	if (m_interleaveReadWriteComponents)
	{
		if (m_imageAspectTestMode == IMAGE_ASPECT_TEST_COLOR)
		{
			fragmentSrc << "	fragColor = vec4(1.0f);\n"
						<< "	fragColor.x = read_data.y;\n"
						<< "	fragColor.z = read_data.w;\n";
		}
		else if (m_imageAspectTestMode == IMAGE_ASPECT_TEST_DEPTH)
		{
			fragmentSrc << "	gl_FragStencilRefARB = int(clamp(read_data.x * 255.0f, 0.0f, 255.0f));\n";
		}
		else
		{
			fragmentSrc << "	gl_FragDepth = clamp(float(read_data.x) / 255.0f, 0.0f, 1.0f);\n";
		}
	}
	else
	{
		if (m_imageAspectTestMode == IMAGE_ASPECT_TEST_DEPTH && m_testMode >= TEST_MODE_READ_WRITE_SAME_PIXEL)
		{
			fragmentSrc << "	gl_FragDepth = clamp(read_data.x, 0.0f, 1.0f);\n";
		}
		else if (m_imageAspectTestMode == IMAGE_ASPECT_TEST_STENCIL && m_testMode >= TEST_MODE_READ_WRITE_SAME_PIXEL)
		{
			fragmentSrc << "	gl_FragStencilRefARB = int(clamp(read_data.x, 0u, 255u));\n";
		}
		else
		{
			fragmentSrc << "	fragColor = read_data;\n";
		}
	}

	fragmentSrc	<< "}\n";

	sourceCollections.glslSources.add("tex_vert") << glu::VertexSource(vertexSrc.str());
	sourceCollections.glslSources.add("tex_frag") << glu::FragmentSource(fragmentSrc.str());
}

TestInstance* AttachmentFeedbackLoopLayoutSamplerTest::createInstance (Context& context) const
{
	const bool useImageAsColorOrDSAttachment	= m_testMode >= TEST_MODE_READ_WRITE_SAME_PIXEL;
	const bool useDifferentAreasSampleWrite		= m_testMode == TEST_MODE_READ_WRITE_DIFFERENT_AREAS;

	if (m_imageAspectTestMode != IMAGE_ASPECT_TEST_COLOR && useImageAsColorOrDSAttachment)
		return new AttachmentFeedbackLoopLayoutDepthStencilImageSamplingInstance(context, getImageSamplingInstanceParams(m_imageViewType, m_imageFormat, m_imageSize, m_imageDescriptorType, m_samplerLod), useImageAsColorOrDSAttachment, useDifferentAreasSampleWrite, m_interleaveReadWriteComponents, m_imageAspectTestMode, m_pipelineStateMode, m_useMaintenance5);
	return new AttachmentFeedbackLoopLayoutImageSamplingInstance(context, getImageSamplingInstanceParams(m_imageViewType, m_imageFormat, m_imageSize, m_imageDescriptorType, m_samplerLod), useImageAsColorOrDSAttachment, useDifferentAreasSampleWrite, m_interleaveReadWriteComponents, m_imageAspectTestMode, m_pipelineStateMode, m_useMaintenance5);
}

tcu::UVec2 AttachmentFeedbackLoopLayoutSamplerTest::getRenderSize (SamplerViewType viewType) const
{
	if (viewType == VK_IMAGE_VIEW_TYPE_1D || viewType == VK_IMAGE_VIEW_TYPE_2D)
	{
		return tcu::UVec2(16u, 16u);
	}
	else
	{
		return tcu::UVec2(16u * 3u, 16u * 2u);
	}
}

std::vector<Vertex4Tex4> createFullscreenQuadArray (vk::VkImageViewType viewType, unsigned arraySize)
{
	using tcu::Vec4;
	std::vector<Vertex4Tex4>	verticesArray;

	const Vertex4Tex4 lowerLeftVertex =
	{
		Vec4(-1.0f, -1.0f, 0.0f, 1.0f),
		Vec4(0.0f, 0.0f, 0.0f, 0.0f)
	};
	const Vertex4Tex4 upperLeftVertex =
	{
		Vec4(-1.0f, 1.0f, 0.0f, 1.0f),
		Vec4(0.0f, 1.0f, 0.0f, 0.0f)
	};
	const Vertex4Tex4 lowerRightVertex =
	{
		Vec4(1.0f, -1.0f, 0.0f, 1.0f),
		Vec4(1.0f, 0.0f, 0.0f, 0.0f)
	};
	const Vertex4Tex4 upperRightVertex =
	{
		Vec4(1.0f, 1.0f, 0.0f, 1.0f),
		Vec4(1.0f, 1.0f, 0.0f, 0.0f)
	};

	for (unsigned arrayNdx = 0; arrayNdx < arraySize; arrayNdx++)
	{
		Vertex4Tex4 vertices[6] =
		{
			lowerLeftVertex,
			upperLeftVertex,
			lowerRightVertex,

			upperLeftVertex,
			lowerRightVertex,
			upperRightVertex
		};

		for (int i = 0; i < 6; i++)
		{
			if (viewType == vk::VK_IMAGE_VIEW_TYPE_1D_ARRAY)
			{
				vertices[i].position.y() = (float)arrayNdx;
				vertices[i].texCoord.y() = (float)arrayNdx;
			}
			else
			{
				vertices[i].position.z() = (float)arrayNdx;
				vertices[i].texCoord.z() = (float)arrayNdx;
			}
			verticesArray.push_back(vertices[i]);
		}
	}

	return verticesArray;
}

std::vector<Vertex4Tex4> createTestQuadAttachmentFeedbackLoopLayout (vk::VkImageViewType viewType)
{
	std::vector<Vertex4Tex4> vertices;

	switch (viewType)
	{
		case vk::VK_IMAGE_VIEW_TYPE_1D:
		case vk::VK_IMAGE_VIEW_TYPE_2D:
			vertices = createFullscreenQuad();
			break;

		case vk::VK_IMAGE_VIEW_TYPE_1D_ARRAY:
			vertices = createFullscreenQuadArray(viewType, 6u);
			break;

		case vk::VK_IMAGE_VIEW_TYPE_2D_ARRAY:
		case vk::VK_IMAGE_VIEW_TYPE_3D:
		case vk::VK_IMAGE_VIEW_TYPE_CUBE:
		case vk::VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
			vertices = createFullscreenQuadArray(viewType, 6u);
			break;

		default:
			DE_ASSERT(false);
			break;
	}

	return vertices;
}

std::vector<Vertex4Tex4> AttachmentFeedbackLoopLayoutSamplerTest::createVertices (void) const
{
	std::vector<Vertex4Tex4> vertices = m_testMode != TEST_MODE_READ_WRITE_DIFFERENT_AREAS ?
		createTestQuadMosaic(m_imageViewType) :
		createTestQuadAttachmentFeedbackLoopLayout(m_imageViewType);
	for (unsigned int i = 0; i < vertices.size(); ++i) {
		if (m_testMode == TEST_MODE_READ_WRITE_DIFFERENT_AREAS)
		{
			vertices[i].texCoord.x() = std::max(vertices[i].texCoord.x(), 0.5f);
			vertices[i].position.x() = std::min(vertices[i].position.x(), 0.0f);
		}
		if (!m_imageViewType.isNormalized()) {
			const float imageSize = static_cast<float>(m_imageSize);
			for (int j = 0; j < tcu::Vec4::SIZE; ++j)
				vertices[i].texCoord[j] *= imageSize;
		}
	}
	return vertices;
}

VkSamplerCreateInfo AttachmentFeedbackLoopLayoutSamplerTest::getSamplerCreateInfo (void) const
{
	const VkSamplerCreateInfo defaultSamplerParams =
	{
		VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,										// VkStructureType			sType;
		DE_NULL,																	// const void*				pNext;
		0u,																			// VkSamplerCreateFlags		flags;
		VK_FILTER_NEAREST,															// VkFilter					magFilter;
		VK_FILTER_NEAREST,															// VkFilter					minFilter;
		VK_SAMPLER_MIPMAP_MODE_NEAREST,												// VkSamplerMipmapMode		mipmapMode;
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,										// VkSamplerAddressMode		addressModeU;
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,										// VkSamplerAddressMode		addressModeV;
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,										// VkSamplerAddressMode		addressModeW;
		0.0f,																		// float					mipLodBias;
		VK_FALSE,																	// VkBool32					anisotropyEnable;
		1.0f,																		// float					maxAnisotropy;
		false,																		// VkBool32					compareEnable;
		VK_COMPARE_OP_NEVER,														// VkCompareOp				compareOp;
		0.0f,																		// float					minLod;
		(m_imageViewType.isNormalized() ? 0.25f : 0.0f),							// float					maxLod;
		getFormatBorderColor(BORDER_COLOR_TRANSPARENT_BLACK, m_imageFormat, false),	// VkBorderColor			borderColor;
		!m_imageViewType.isNormalized(),											// VkBool32					unnormalizedCoordinates;
	};

	return defaultSamplerParams;
}

VkComponentMapping AttachmentFeedbackLoopLayoutSamplerTest::getComponentMapping (void) const
{
	const VkComponentMapping	componentMapping	= { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
	return componentMapping;
}

std::string AttachmentFeedbackLoopLayoutSamplerTest::getGlslSamplerType (const tcu::TextureFormat& format, SamplerViewType type)
{
	std::ostringstream samplerType;

	if (tcu::getTextureChannelClass(format.type) == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER)
		samplerType << "u";
	else if (tcu::getTextureChannelClass(format.type) == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER)
		samplerType << "i";

	switch (type)
	{
		case VK_IMAGE_VIEW_TYPE_1D:
			samplerType << "sampler1D";
			break;

		case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
			samplerType << "sampler1DArray";
			break;

		case VK_IMAGE_VIEW_TYPE_2D:
			samplerType << "sampler2D";
			break;

		case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
			samplerType << "sampler2DArray";
			break;

		case VK_IMAGE_VIEW_TYPE_3D:
			samplerType << "sampler3D";
			break;

		case VK_IMAGE_VIEW_TYPE_CUBE:
			samplerType << "samplerCube";
			break;

		case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
			samplerType << "samplerCubeArray";
			break;

		default:
			DE_FATAL("Unknown image view type");
			break;
	}

	return samplerType.str();
}

tcu::IVec3 AttachmentFeedbackLoopLayoutSamplerTest::getImageSize (SamplerViewType viewType, int size)
{
	switch (viewType)
	{
		case VK_IMAGE_VIEW_TYPE_1D:
		case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
			return tcu::IVec3(size, 1, 1);

		case VK_IMAGE_VIEW_TYPE_3D:
			return tcu::IVec3(size, size, 4);

		default:
			break;
	}

	return tcu::IVec3(size, size, 1);
}

int AttachmentFeedbackLoopLayoutSamplerTest::getArraySize (SamplerViewType viewType)
{
	switch (viewType)
	{
		case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
		case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
		case VK_IMAGE_VIEW_TYPE_CUBE:
			return 6;

		case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
			return 36;

		default:
			break;
	}

	return 1;
}
} // anonymous

tcu::TestCaseGroup* createAttachmentFeedbackLoopLayoutSamplerTests (tcu::TestContext& testCtx, vk::PipelineConstructionType pipelineConstructionType)
{
	// TODO: implement layer rendering with a geometry shader to render to arrays, 3D and cube images.
	const struct
	{
		SamplerViewType		type;
		const char*			name;
		bool				readOnly;
	}
	imageViewTypes[] =
	{
		{ VK_IMAGE_VIEW_TYPE_1D,			"1d", false },
		{ { VK_IMAGE_VIEW_TYPE_1D, false },	"1d_unnormalized", false },
		{ VK_IMAGE_VIEW_TYPE_1D_ARRAY,		"1d_array", true },
		{ VK_IMAGE_VIEW_TYPE_2D,			"2d", false },
		{ { VK_IMAGE_VIEW_TYPE_2D, false },	"2d_unnormalized", false },
		{ VK_IMAGE_VIEW_TYPE_2D_ARRAY,		"2d_array", true },
		{ VK_IMAGE_VIEW_TYPE_3D,			"3d", true },
		{ VK_IMAGE_VIEW_TYPE_CUBE,			"cube", true },
		{ VK_IMAGE_VIEW_TYPE_CUBE_ARRAY,	"cube_array", true }
	};

	const VkFormat formats[] =
	{
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_D16_UNORM,
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_D16_UNORM_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D32_SFLOAT_S8_UINT,
		VK_FORMAT_S8_UINT
	};

	de::MovePtr<tcu::TestCaseGroup> samplingTypeTests		(new tcu::TestCaseGroup(testCtx, "sampler", ""));

	const struct
	{
		enum TestMode		mode;
		const char*			name;
	}
	testModes[] =
	{
		{ TEST_MODE_READ_ONLY,							"_read" },
		{ TEST_MODE_READ_WRITE_SAME_PIXEL,				"_read_write_same_pixel" },
		{ TEST_MODE_READ_WRITE_DIFFERENT_AREAS,			"_read_write_different_areas" },
	};

	const char* imageAspectTestModes[] = { "_color", "_depth", "_stencil" };

	const struct
	{
		VkDescriptorType	type;
		const char*			name;
	}
	imageDescriptorTypes[] =
	{
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,	"combined_image_sampler" },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,				"sampled_image" },
	};

	const struct
	{
		bool				interleaveReadWriteComponents;
		const char*			name;
	}
	interleaveReadWriteComponentsModes[] =
	{
		{ false,							"" },
		{ true,								"_interleave_read_write_components" },
	};

	const struct
	{
		const PipelineStateMode	pipelineStateMode;
		const char*				suffix;
	} pipelineStateModes[] =
	{
		{ PipelineStateMode::STATIC,							""						},
		{ PipelineStateMode::DYNAMIC_WITH_ZERO_STATIC,			"_dynamic_zero_static"	},
		{ PipelineStateMode::DYNAMIC_WITH_CONTRADICTORY_STATIC,	"_dynamic_bad_static"	},
	};

	for (int imageDescriptorTypeNdx = 0; imageDescriptorTypeNdx < DE_LENGTH_OF_ARRAY(imageDescriptorTypes); imageDescriptorTypeNdx++)
	{
		VkDescriptorType					imageDescriptorType		= imageDescriptorTypes[imageDescriptorTypeNdx].type;
		de::MovePtr<tcu::TestCaseGroup>	imageDescriptorTypeGroup	(new tcu::TestCaseGroup(testCtx, imageDescriptorTypes[imageDescriptorTypeNdx].name, (std::string("Uses a ") + imageDescriptorTypes[imageDescriptorTypeNdx].name + " sampler").c_str()));
		de::MovePtr<tcu::TestCaseGroup> imageTypeTests		(new tcu::TestCaseGroup(testCtx, "image_type", ""));

		for (int viewTypeNdx = 0; viewTypeNdx < DE_LENGTH_OF_ARRAY(imageViewTypes); viewTypeNdx++)
		{
			const SamplerViewType			viewType		= imageViewTypes[viewTypeNdx].type;
			de::MovePtr<tcu::TestCaseGroup> viewTypeGroup   (new tcu::TestCaseGroup(testCtx, imageViewTypes[viewTypeNdx].name, (std::string("Uses a ") + imageViewTypes[viewTypeNdx].name + " view").c_str()));
			de::MovePtr<tcu::TestCaseGroup>	formatTests		(new tcu::TestCaseGroup(testCtx, "format", "Tests samplable formats"));

			for (size_t formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(formats); formatNdx++)
			{
				const VkFormat	format			= formats[formatNdx];
				const bool		isCompressed	= isCompressedFormat(format);
				const bool		isDepthStencil	= !isCompressed && tcu::hasDepthComponent(mapVkFormat(format).order) && tcu::hasStencilComponent(mapVkFormat(format).order);
				ImageAspectTestMode	imageAspectTestMode = getImageAspectTestMode(format);

				if (isCompressed)
				{
					// Do not use compressed formats with 1D and 1D array textures.
					if (viewType == VK_IMAGE_VIEW_TYPE_1D || viewType == VK_IMAGE_VIEW_TYPE_1D_ARRAY)
						break;
				}

				for (int testModeNdx = 0; testModeNdx < DE_LENGTH_OF_ARRAY(testModes); testModeNdx++)
				{
					if (imageViewTypes[viewTypeNdx].readOnly && testModes[testModeNdx].mode != TEST_MODE_READ_ONLY)
						continue;

					for (int restrictColorNdx = 0; restrictColorNdx < DE_LENGTH_OF_ARRAY(interleaveReadWriteComponentsModes); restrictColorNdx++)
					{
						// Limit the interleaveReadWriteComponents test to the ones sampling and writing to the same pixel, to avoid having more tests that are not really adding coverage.
						if (interleaveReadWriteComponentsModes[restrictColorNdx].interleaveReadWriteComponents &&
							testModes[testModeNdx].mode != TEST_MODE_READ_WRITE_SAME_PIXEL)
							continue;

						// If the format is depth-only or stencil-only, do not read one component and write it to the other, as it is missing.
						if (interleaveReadWriteComponentsModes[restrictColorNdx].interleaveReadWriteComponents &&
							(tcu::hasDepthComponent(mapVkFormat(format).order) || tcu::hasStencilComponent(mapVkFormat(format).order)) && !isDepthStencil)
							continue;

						for (const auto& pipelineStateMode : pipelineStateModes)
						{
							std::string name = getFormatCaseName(format) + imageAspectTestModes[imageAspectTestMode] + testModes[testModeNdx].name + interleaveReadWriteComponentsModes[restrictColorNdx].name + pipelineStateMode.suffix;
							formatTests->addChild(new AttachmentFeedbackLoopLayoutSamplerTest(testCtx, pipelineConstructionType, name.c_str(), "", viewType, format, outputImageSize, imageDescriptorType, 0.0f, testModes[testModeNdx].mode, imageAspectTestMode, interleaveReadWriteComponentsModes[restrictColorNdx].interleaveReadWriteComponents, pipelineStateMode.pipelineStateMode, false));

							if (!isCompressed && isDepthStencil)
							{
								// Image is depth-stencil. Add the stencil case as well.
								std::string stencilTestName = getFormatCaseName(format) + imageAspectTestModes[IMAGE_ASPECT_TEST_STENCIL] + testModes[testModeNdx].name + interleaveReadWriteComponentsModes[restrictColorNdx].name + pipelineStateMode.suffix;
								formatTests->addChild(new AttachmentFeedbackLoopLayoutSamplerTest(testCtx, pipelineConstructionType, stencilTestName.c_str(), "", viewType, format, outputImageSize, imageDescriptorType, 0.0f, testModes[testModeNdx].mode, IMAGE_ASPECT_TEST_STENCIL, interleaveReadWriteComponentsModes[restrictColorNdx].interleaveReadWriteComponents, pipelineStateMode.pipelineStateMode, false));
							}
						}
					}
				}
			}

			viewTypeGroup->addChild(formatTests.release());
			imageTypeTests->addChild(viewTypeGroup.release());
		}
		imageDescriptorTypeGroup->addChild(imageTypeTests.release());
		samplingTypeTests->addChild(imageDescriptorTypeGroup.release());
	}

	if (pipelineConstructionType == PipelineConstructionType::PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
	{
		de::MovePtr<tcu::TestCaseGroup> miscGroup(new tcu::TestCaseGroup(testCtx, "misc", ""));
		miscGroup->addChild(new AttachmentFeedbackLoopLayoutSamplerTest(testCtx, pipelineConstructionType, "maintenance5_color_attachment", "", VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, outputImageSize, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 0.0f, TEST_MODE_READ_ONLY, IMAGE_ASPECT_TEST_COLOR, false, PipelineStateMode::STATIC, true));
		miscGroup->addChild(new AttachmentFeedbackLoopLayoutSamplerTest(testCtx, pipelineConstructionType, "maintenance5_ds_attachment", "", VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_D16_UNORM, outputImageSize, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 0.0f, TEST_MODE_READ_ONLY, IMAGE_ASPECT_TEST_DEPTH, false, PipelineStateMode::STATIC, true));
		samplingTypeTests->addChild(miscGroup.release());
	}

	return samplingTypeTests.release();
}

tcu::TestCaseGroup* createAttachmentFeedbackLoopLayoutTests (tcu::TestContext& testCtx, vk::PipelineConstructionType pipelineConstructionType)
{
	de::MovePtr<tcu::TestCaseGroup> attachmentFeedbackLoopLayoutTests(new tcu::TestCaseGroup(testCtx, "attachment_feedback_loop_layout", "VK_EXT_attachment_feedback_loop_layout tests"));
	{
		attachmentFeedbackLoopLayoutTests->addChild(createAttachmentFeedbackLoopLayoutSamplerTests(testCtx, pipelineConstructionType));
	}

	return attachmentFeedbackLoopLayoutTests.release();
}

} // pipeline
} // vkt
