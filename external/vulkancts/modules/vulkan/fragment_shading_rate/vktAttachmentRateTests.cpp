/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
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
 *//*!
 * \file
 * \brief Tests for VK_KHR_fragment_shading_rate
 *//*--------------------------------------------------------------------*/

#include "vktAttachmentRateTests.hpp"

#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkQueryUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkPlatform.hpp"
#include "vkBuilderUtil.hpp"

#include "vktTestGroupUtil.hpp"
#include "vktTestCase.hpp"
#include "vktCustomInstancesDevices.hpp"

#include "deDefs.h"
#include "deMath.h"
#include "deSharedPtr.hpp"
#include "deString.h"
#include "deSTLUtil.hpp"

#include "tcuTestCase.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuStringTemplate.hpp"

#include <string>
#include <vector>
#include <limits>
#include <map>

namespace vkt
{
namespace FragmentShadingRate
{
namespace
{

using namespace vk;

// flag used to test TM_SETUP_RATE_WITH_ATOMICS_IN_COMPUTE_SHADER;
// when it is 1 instead of using atomic operations to fill image
// plain store will be used as it is always supported
#define DEBUG_USE_STORE_INSTEAD_OF_ATOMICS 0

enum TestMode
{
	TM_SETUP_RATE_WITH_ATOMICS_IN_COMPUTE_SHADER = 0,
	TM_SETUP_RATE_WITH_FRAGMENT_SHADER,
	TM_SETUP_RATE_WITH_COPYING_FROM_OTHER_IMAGE,
	TM_SETUP_RATE_WITH_COPYING_FROM_EXCLUSIVE_IMAGE_USING_TRANSFER_QUEUE,
	TM_SETUP_RATE_WITH_COPYING_FROM_CONCURENT_IMAGE_USING_TRANSFER_QUEUE,
	TM_SETUP_RATE_WITH_LINEAR_TILED_IMAGE,

	TM_TWO_SUBPASS
};

struct TestParams
{
	TestMode		mode;

	VkFormat		srFormat;
	VkExtent2D		srRate;

	bool			useDynamicRendering;
	bool			useImagelessFramebuffer;
};

deUint32 calculateRate(deUint32 rateWidth, deUint32 rateHeight)
{
	return (deCtz32(rateWidth) << 2u) | deCtz32(rateHeight);
}

class AttachmentRateInstance : public TestInstance
{
public:
							AttachmentRateInstance	(Context& context, const de::SharedPtr<TestParams> params);
	tcu::TestStatus			iterate					(void);

private:

	// Helper structure used by buildFramebuffer method.
	// It is used to build regular or imageless framebuffer.
	struct FBAttachmentInfo
	{
		VkFormat			format;
		VkImageUsageFlags	usage;
		deUint32			width;
		deUint32			height;
		VkImageView			view;
	};

private:

	de::MovePtr<ImageWithMemory>	buildImageWithMemory		(VkFormat						format,
																 deUint32						width,
																 deUint32						height,
																 VkImageUsageFlags				usage,
																 VkImageTiling					tiling = VK_IMAGE_TILING_OPTIMAL,
																 std::vector<deUint32>			queueFamilies = std::vector<deUint32>());
	de::MovePtr<BufferWithMemory>	buildBufferWithMemory		(deUint32						size,
																 VkBufferUsageFlags				usage);
	Move<VkImageView>				buildImageView				(VkFormat						format,
																 VkImage						image);

	void							buildColorBufferObjects		(deUint32						cbIindex,
																 VkImageUsageFlags				cbUsage);
	void							buildShadingRateObjects		(deUint32						srIndex,
																 deUint32						width,
																 deUint32						height,
																 VkImageUsageFlags				srUsage,
																 VkImageTiling					srTiling = VK_IMAGE_TILING_OPTIMAL);
	void							buildCounterBufferObjects	();

	Move<VkRenderPass>				buildRenderPass				(VkFormat								cbFormat,
																 deUint32								sr1TileWidth = 0,
																 deUint32								sr1TileHeight = 0,
																 deUint32								sr2TileWidth = 0,
																 deUint32								sr2TileHeight = 0) const;
	Move<VkFramebuffer>				buildFramebuffer			(VkRenderPass							renderPass,
																 const std::vector<FBAttachmentInfo>&	attachmentInfo) const;
	Move<VkPipelineLayout>			buildPipelineLayout			(const VkDescriptorSetLayout*			setLayouts = DE_NULL) const;
	Move<VkPipeline>				buildGraphicsPipeline		(deUint32								subpass,
																 VkRenderPass							renderPass,
																 VkFormat								cbFormat,
																 VkPipelineLayout						layout,
																 VkShaderModule							vertShader,
																 VkShaderModule							fragShader,
																 bool									useShadingRate = VK_TRUE) const;
	Move<VkPipeline>				buildComputePipeline		(VkShaderModule							compShader,
																 VkPipelineLayout						pipelineLayout) const;
	VkDescriptorSetAllocateInfo		makeDescriptorSetAllocInfo	(VkDescriptorPool						descriptorPool,
																 const VkDescriptorSetLayout*			pSetLayouts) const;

	void							startRendering				(const VkCommandBuffer					commandBuffer,
																 const VkRenderPass						renderPass,
																 const VkFramebuffer					framebuffer,
																 const VkRect2D&						renderArea,
																 const std::vector<FBAttachmentInfo>&	attachmentInfo,
																 const deUint32							srTileWidth = 0,
																 const deUint32							srTileHeight = 0) const;
	void							finishRendering				(const VkCommandBuffer					commandBuffer) const;

	bool							verifyUsingAtomicChecks		(deUint32						tileWidth,
																 deUint32						tileHeight,
																 deUint32						rateWidth,
																 deUint32						rateHeight,
																 deUint32*						outBufferPtr) const;

	bool							runComputeShaderMode		(void);
	bool							runFragmentShaderMode		(void);
	bool							runCopyMode					(void);
	bool							runCopyModeOnTransferQueue	(void);
	bool							runFillLinearTiledImage		(void);
	bool							runTwoSubpassMode			(void);

private:

	// When custom device is used then m_device and m_allocator are used in all
	// helper methods. Otherwise default device and allocator from context are used.
	// Those objects are used only by tests from runCopyModeOnTransferQueue.
	Move<VkDevice>					m_device;
	de::MovePtr<Allocator>			m_allocator;

	const de::SharedPtr<TestParams>	m_params;
	const deUint32					m_cbWidth;
	const deUint32					m_cbHeight;
	VkFormat						m_cbFormat;
	VkImageUsageFlags				m_cbUsage;
	VkImageUsageFlags				m_srUsage;

	// structures commonly used by most of tests
	const VkImageSubresourceLayers	m_defaultImageSubresourceLayers;
	const VkImageSubresourceRange	m_defaultImageSubresourceRange;
	const VkBufferImageCopy			m_defaultBufferImageCopy;

	// objects commonly used by most of tests
	de::MovePtr<ImageWithMemory>	m_cbImage[2];
	Move<VkImageView>				m_cbImageView[2];
	de::MovePtr<BufferWithMemory>	m_cbReadBuffer[2];

	de::MovePtr<ImageWithMemory>	m_srImage[2];
	Move<VkImageView>				m_srImageView[2];

	Move<VkDescriptorSetLayout>		m_counterBufferDescriptorSetLayout;
	Move<VkDescriptorPool>			m_counterBufferDescriptorPool;
	Move<VkDescriptorSet>			m_counterBufferDescriptorSet;
	de::MovePtr<BufferWithMemory>	m_counterBuffer;

	// properties commonly used by most of tests
	VkExtent2D						m_minTileSize;
	VkExtent2D						m_maxTileSize;
	deUint32						m_maxAspectRatio;
};

AttachmentRateInstance::AttachmentRateInstance(Context& context, const de::SharedPtr<TestParams> params)
	: vkt::TestInstance					(context)
	, m_params							(params)
	, m_cbWidth							(60)
	, m_cbHeight						(60)
	, m_cbFormat						(VK_FORMAT_R32G32B32A32_UINT)
	, m_cbUsage							(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
	, m_srUsage							(VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
	, m_defaultImageSubresourceLayers	(makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u))
	, m_defaultImageSubresourceRange	(makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0, 1u))
	, m_defaultBufferImageCopy			(makeBufferImageCopy({ m_cbWidth, m_cbHeight, 1u }, m_defaultImageSubresourceLayers))
{
	// prepare data needed to calculate tile sizes
	const auto& srProperties	= m_context.getFragmentShadingRateProperties();
	m_minTileSize				= srProperties.minFragmentShadingRateAttachmentTexelSize;
	m_maxTileSize				= srProperties.maxFragmentShadingRateAttachmentTexelSize;
	m_maxAspectRatio			= srProperties.maxFragmentShadingRateAttachmentTexelSizeAspectRatio;
}

de::MovePtr<ImageWithMemory> AttachmentRateInstance::buildImageWithMemory (VkFormat					format,
																		   deUint32					width,
																		   deUint32					height,
																		   VkImageUsageFlags		usage,
																		   VkImageTiling			tiling,
																		   std::vector<deUint32>	queueFamilies)
{
	VkImageCreateInfo imageCreateInfo
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		(VkImageCreateFlags)0u,						// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,							// VkImageType				imageType;
		format,										// VkFormat					format;
		{
			width,									// deUint32					width;
			height,									// deUint32					height;
			1u										// deUint32					depth;
		},											// VkExtent3D				extent;
		1u,											// deUint32					mipLevels;
		1u,											// deUint32					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits	samples;
		tiling,										// VkImageTiling			tiling;
		usage,										// VkImageUsageFlags		usage;
		queueFamilies.empty() ?
			VK_SHARING_MODE_EXCLUSIVE :
			VK_SHARING_MODE_CONCURRENT,				// VkSharingMode			sharingMode;
		(deUint32)queueFamilies.size(),				// deUint32					queueFamilyIndexCount;
		queueFamilies.data(),						// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED					// VkImageLayout			initialLayout;
	};

	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	VkDevice				device				= m_device.get() ? *m_device : m_context.getDevice();
	vk::Allocator&			allocator			= m_allocator.get() ? *m_allocator : m_context.getDefaultAllocator();
	vk::MemoryRequirement	memoryRequirement	= (tiling == VK_IMAGE_TILING_LINEAR) ? MemoryRequirement::HostVisible : MemoryRequirement::Any;
	return de::MovePtr<ImageWithMemory>(new ImageWithMemory(vk, device, allocator, imageCreateInfo, memoryRequirement));
}

de::MovePtr<BufferWithMemory> AttachmentRateInstance::buildBufferWithMemory(deUint32 size, VkBufferUsageFlags usage)
{
	const DeviceInterface&		vk				= m_context.getDeviceInterface();
	VkDevice					device			= m_device.get() ? *m_device : m_context.getDevice();
	vk::Allocator&				allocator		= m_allocator.get() ? *m_allocator : m_context.getDefaultAllocator();
	const VkBufferCreateInfo	readBufferInfo	= makeBufferCreateInfo(size, usage);

	return de::MovePtr<BufferWithMemory>(new BufferWithMemory(vk, device, allocator, readBufferInfo, MemoryRequirement::HostVisible));
}

Move<VkImageView> AttachmentRateInstance::buildImageView (VkFormat format, VkImage image)
{
	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	VkDevice				device				= m_device.get() ? *m_device : m_context.getDevice();
	VkImageSubresourceRange	subresourceRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

	return makeImageView(vk, device, image, VK_IMAGE_VIEW_TYPE_2D, format, subresourceRange);
};

void AttachmentRateInstance::buildColorBufferObjects(deUint32 cbIndex, VkImageUsageFlags cbUsage)
{
	DE_ASSERT(cbIndex < 2);

	m_cbImage[cbIndex]			= buildImageWithMemory(m_cbFormat, m_cbWidth, m_cbHeight, cbUsage);
	m_cbImageView[cbIndex]		= buildImageView(m_cbFormat, m_cbImage[cbIndex]->get());
	m_cbReadBuffer[cbIndex]		= buildBufferWithMemory(m_cbWidth * m_cbHeight * deUint32(sizeof(int)) * 4u, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
}

void AttachmentRateInstance::buildShadingRateObjects(deUint32 srIndex, deUint32 width, deUint32 height, VkImageUsageFlags srUsage, VkImageTiling srTiling)
{
	DE_ASSERT(srIndex < 2);

	m_srImage[srIndex]		= buildImageWithMemory(m_params->srFormat, width, height, srUsage, srTiling);
	m_srImageView[srIndex]	= buildImageView(m_params->srFormat, m_srImage[srIndex]->get());
}

void AttachmentRateInstance::buildCounterBufferObjects()
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	VkDevice				device	= m_device.get() ? *m_device : m_context.getDevice();

	m_counterBufferDescriptorPool		= DescriptorPoolBuilder()
											.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u)
											.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	m_counterBufferDescriptorSetLayout	= DescriptorSetLayoutBuilder()
											.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
											.build(vk, device);

	const VkDescriptorSetAllocateInfo descriptorSetAllocInfo = makeDescriptorSetAllocInfo(*m_counterBufferDescriptorPool,
																						  &(*m_counterBufferDescriptorSetLayout));
	m_counterBufferDescriptorSet = allocateDescriptorSet(vk, device, &descriptorSetAllocInfo);

	// create ssbo buffer for atomic counter
	deUint32 ssboSize	= deUint32(sizeof(deUint32));
	m_counterBuffer		= buildBufferWithMemory(ssboSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

	const VkDescriptorBufferInfo descriptorInfo = makeDescriptorBufferInfo(**m_counterBuffer, 0, ssboSize);
	DescriptorSetUpdateBuilder()
		.writeSingle(*m_counterBufferDescriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfo)
		.update(vk, device);

	// reset counter
	*((deUint32*)m_counterBuffer->getAllocation().getHostPtr()) = 0u;
	flushAlloc(vk, device, m_counterBuffer->getAllocation());
}

Move<VkRenderPass> AttachmentRateInstance::buildRenderPass(VkFormat cbFormat,
														   deUint32 sr0TileWidth, deUint32 sr0TileHeight,
														   deUint32 sr1TileWidth, deUint32 sr1TileHeight) const
{
	if (m_params->useDynamicRendering)
		return Move<VkRenderPass>();

	const bool		useShadingRate0		= (sr0TileWidth * sr0TileHeight > 0);
	const bool		useShadingRate1		= (sr1TileWidth * sr1TileHeight > 0);

	deUint32		attachmentCount		= 1;
	const deUint32	subpassCount		= 1 + useShadingRate1;

	std::vector<VkAttachmentReference2> colorAttachmentReferences(subpassCount, {
		VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,							// VkStructureType					sType;
		DE_NULL,															// const void*						pNext;
		0,																	// uint32_t							attachment;
		VK_IMAGE_LAYOUT_GENERAL,											// VkImageLayout					layout;
		0,																	// VkImageAspectFlags				aspectMask;
	});

	std::vector<VkAttachmentReference2> fragmentShadingRateAttachments(subpassCount, {
		VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,							// VkStructureType					sType;
		DE_NULL,															// const void*						pNext;
		1,																	// uint32_t							attachment;
		VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR,		// VkImageLayout					layout;
		0,																	// VkImageAspectFlags				aspectMask;
	});

	std::vector<VkFragmentShadingRateAttachmentInfoKHR> shadingRateAttachmentInfos(subpassCount, {
		VK_STRUCTURE_TYPE_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR,		// VkStructureType					sType;
		DE_NULL,															// const void*						pNext;
		&fragmentShadingRateAttachments[0],									// const VkAttachmentReference2*	pFragmentShadingRateAttachment;
		{ sr0TileWidth, sr0TileHeight },									// VkExtent2D						shadingRateAttachmentTexelSize;
	});

	std::vector<VkSubpassDescription2> subpassDescriptions(subpassCount, {
		VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,							// VkStructureType					sType;
		DE_NULL,															// const void*						pNext;
		(vk::VkSubpassDescriptionFlags)0,									// VkSubpassDescriptionFlags		flags;
		vk::VK_PIPELINE_BIND_POINT_GRAPHICS,								// VkPipelineBindPoint				pipelineBindPoint;
		0u,																	// uint32_t							viewMask;
		0u,																	// uint32_t							inputAttachmentCount;
		DE_NULL,															// const VkAttachmentReference2*	pInputAttachments;
		1,																	// uint32_t							colorAttachmentCount;
		&colorAttachmentReferences[0],										// const VkAttachmentReference2*	pColorAttachments;
		DE_NULL,															// const VkAttachmentReference2*	pResolveAttachments;
		DE_NULL,															// const VkAttachmentReference2*	pDepthStencilAttachment;
		0u,																	// uint32_t							preserveAttachmentCount;
		DE_NULL,															// const uint32_t*					pPreserveAttachments;
	});

	std::vector<VkAttachmentDescription2> attachmentDescriptions(2 * subpassCount, {
		VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,							// VkStructureType					sType;
		DE_NULL,															// const void*						pNext;
		(VkAttachmentDescriptionFlags)0u,									// VkAttachmentDescriptionFlags		flags;
		cbFormat,															// VkFormat							format;
		VK_SAMPLE_COUNT_1_BIT,												// VkSampleCountFlagBits			samples;
		VK_ATTACHMENT_LOAD_OP_CLEAR,										// VkAttachmentLoadOp				loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,										// VkAttachmentStoreOp				storeOp;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,									// VkAttachmentLoadOp				stencilLoadOp;
		VK_ATTACHMENT_STORE_OP_DONT_CARE,									// VkAttachmentStoreOp				stencilStoreOp;
		VK_IMAGE_LAYOUT_GENERAL,											// VkImageLayout					initialLayout;
		VK_IMAGE_LAYOUT_GENERAL												// VkImageLayout					finalLayout;
	});

	if (useShadingRate0)
	{
		attachmentCount							= 2;
		subpassDescriptions[0].pNext			= &shadingRateAttachmentInfos[0];
		attachmentDescriptions[1].format		= m_params->srFormat;
		attachmentDescriptions[1].loadOp		= VK_ATTACHMENT_LOAD_OP_LOAD;
		attachmentDescriptions[1].storeOp		= VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachmentDescriptions[1].initialLayout = VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR;
	}

	if (useShadingRate1)
	{
		attachmentCount													= 4;
		colorAttachmentReferences[1].attachment							= 2;
		fragmentShadingRateAttachments[1].attachment					= 3;
		shadingRateAttachmentInfos[1].pFragmentShadingRateAttachment	= &fragmentShadingRateAttachments[1];
		shadingRateAttachmentInfos[1].shadingRateAttachmentTexelSize	= { sr1TileWidth, sr1TileHeight };
		subpassDescriptions[1].pNext									= &shadingRateAttachmentInfos[1];
		subpassDescriptions[1].pColorAttachments						= &colorAttachmentReferences[1];

		attachmentDescriptions[3].format		= m_params->srFormat;
		attachmentDescriptions[3].loadOp		= VK_ATTACHMENT_LOAD_OP_LOAD;
		attachmentDescriptions[3].storeOp		= VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachmentDescriptions[3].initialLayout = VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR;
	}

	const VkRenderPassCreateInfo2 renderPassParams
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2,						// VkStructureType					sType;
		DE_NULL,															// const void*						pNext;
		(vk::VkRenderPassCreateFlags)0,										// VkRenderPassCreateFlags			flags;
		attachmentCount,													// uint32_t							attachmentCount;
		attachmentDescriptions.data(),										// const VkAttachmentDescription2*	pAttachments;
		subpassCount,														// uint32_t							subpassCount;
		subpassDescriptions.data(),											// const VkSubpassDescription2*		pSubpasses;
		0u,																	// uint32_t							dependencyCount;
		DE_NULL,															// const VkSubpassDependency2*		pDependencies;
		0u,																	// uint32_t							correlatedViewMaskCount;
		DE_NULL,															// const uint32_t*					pCorrelatedViewMasks;
	};

	VkDevice device = m_device.get() ? *m_device : m_context.getDevice();
	return createRenderPass2(m_context.getDeviceInterface(), device, &renderPassParams);
}

Move<VkFramebuffer> AttachmentRateInstance::buildFramebuffer(VkRenderPass renderPass, const std::vector<FBAttachmentInfo>& attachmentInfo) const
{
	if (m_params->useDynamicRendering)
		return Move<VkFramebuffer>();

	VkDevice device = m_device.get() ? *m_device : m_context.getDevice();

	VkFramebufferCreateInfo framebufferParams
	{
		vk::VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,						// VkStructureType				sType;
		DE_NULL,															// const void*					pNext;
		(vk::VkFramebufferCreateFlags)0u,									// VkFramebufferCreateFlags		flags;
		renderPass,															// VkRenderPass					renderPass;
		(deUint32)attachmentInfo.size(),									// uint32_t						attachmentCount;
		DE_NULL,															// const VkImageView*			pAttachments;
		attachmentInfo[0].width,											// uint32_t						width;
		attachmentInfo[0].height,											// uint32_t						height;
		1u,																	// uint32_t						layers;
	};

	if (m_params->useImagelessFramebuffer)
	{
		std::vector<VkFramebufferAttachmentImageInfo> framebufferAttachmentImageInfo(attachmentInfo.size(),
			{
				VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO,		// VkStructureType		sType;
				DE_NULL,													// const void*			pNext;
				(VkImageCreateFlags)0u,										// VkImageCreateFlags	flags;
				0u,															// VkImageUsageFlags	usage;
				0u,															// deUint32				width;
				0u,															// deUint32				height;
				1u,															// deUint32				layerCount;
				1u,															// deUint32				viewFormatCount;
				DE_NULL														// const VkFormat*		pViewFormats;
			}
		);

		for (deUint32 i = 0; i < (deUint32)attachmentInfo.size(); ++i)
		{
			const auto&	src = attachmentInfo[i];
			auto&		dst = framebufferAttachmentImageInfo[i];

			dst.usage			= src.usage;
			dst.width			= src.width;
			dst.height			= src.height;
			dst.pViewFormats	= &src.format;
		}

		VkFramebufferAttachmentsCreateInfo framebufferAttachmentsCreateInfo
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO,			// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			(deUint32)framebufferAttachmentImageInfo.size(),				// deUint32									attachmentImageInfoCount;
			framebufferAttachmentImageInfo.data()							// const VkFramebufferAttachmentImageInfo*	pAttachmentImageInfos;
		};

		framebufferParams.pNext	= &framebufferAttachmentsCreateInfo;
		framebufferParams.flags	= VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT;

		return createFramebuffer(m_context.getDeviceInterface(), device, &framebufferParams);
	}

	// create array containing just attachment views
	std::vector<VkImageView> attachments(attachmentInfo.size(), 0);
	for (deUint32 i = 0; i < (deUint32)attachmentInfo.size(); ++i)
		attachments[i] = attachmentInfo[i].view;

	framebufferParams.pAttachments = attachments.data();

	return createFramebuffer(m_context.getDeviceInterface(), device, &framebufferParams);
}

Move<VkPipelineLayout> AttachmentRateInstance::buildPipelineLayout(const VkDescriptorSetLayout* setLayouts) const
{
	const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType					sType;
		DE_NULL,											// const void*						pNext;
		(VkPipelineLayoutCreateFlags)0,						// VkPipelineLayoutCreateFlags		flags;
		(setLayouts != DE_NULL),							// uint32_t							setLayoutCount;
		setLayouts,											// const VkDescriptorSetLayout*		pSetLayouts;
		0u,													// uint32_t							pushConstantRangeCount;
		DE_NULL,											// const VkPushConstantRange*		pPushConstantRanges;
	};

	VkDevice device = m_device.get() ? *m_device : m_context.getDevice();
	return createPipelineLayout(m_context.getDeviceInterface(), device, &pipelineLayoutCreateInfo, NULL);
}

Move<VkPipeline> AttachmentRateInstance::buildGraphicsPipeline(deUint32 subpass, VkRenderPass renderPass, VkFormat cbFormat, VkPipelineLayout pipelineLayout,
															   VkShaderModule vertShader, VkShaderModule fragShader, bool useShadingRate) const
{
	std::vector<VkPipelineShaderStageCreateInfo> pipelineShaderStageParams(2,
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,			// VkStructureType								sType
		DE_NULL,														// const void*									pNext
		0u,																// VkPipelineShaderStageCreateFlags				flags
		VK_SHADER_STAGE_VERTEX_BIT,										// VkShaderStageFlagBits						stage
		vertShader,														// VkShaderModule								module
		"main",															// const char*									pName
		DE_NULL															// const VkSpecializationInfo*					pSpecializationInfo
	});

	pipelineShaderStageParams[1].stage	= VK_SHADER_STAGE_FRAGMENT_BIT;
	pipelineShaderStageParams[1].module	= fragShader;

	const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType								sType
		DE_NULL,														// const void*									pNext
		(VkPipelineVertexInputStateCreateFlags)0,						// VkPipelineVertexInputStateCreateFlags		flags
		0u,																// deUint32										vertexBindingDescriptionCount
		DE_NULL,														// const VkVertexInputBindingDescription*		pVertexBindingDescriptions
		0u,																// deUint32										vertexAttributeDescriptionCount
		DE_NULL															// const VkVertexInputAttributeDescription*		pVertexAttributeDescriptions
	};

	const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo
	{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType								sType
		DE_NULL,														// const void*									pNext
		0u,																// VkPipelineInputAssemblyStateCreateFlags		flags
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,							// VkPrimitiveTopology							topology
		VK_FALSE														// VkBool32										primitiveRestartEnable
	};

	tcu::UVec2	size		(m_cbWidth, m_cbHeight);
	VkViewport	viewport	= makeViewport	(size);
	VkRect2D	scissor		= makeRect2D	(size);

	const VkPipelineViewportStateCreateInfo viewportStateCreateInfo
	{
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,			// VkStructureType								sType
		DE_NULL,														// const void*									pNext
		(VkPipelineViewportStateCreateFlags)0,							// VkPipelineViewportStateCreateFlags			flags
		1u,																// deUint32										viewportCount
		&viewport,														// const VkViewport*							pViewports
		1u,																// deUint32										scissorCount
		&scissor														// const VkRect2D*								pScissors
	};

	const VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		// VkStructureType								sType
		DE_NULL,														// const void*									pNext
		0u,																// VkPipelineRasterizationStateCreateFlags		flags
		VK_FALSE,														// VkBool32										depthClampEnable
		VK_FALSE,														// VkBool32										rasterizerDiscardEnable
		VK_POLYGON_MODE_FILL,											// VkPolygonMode								polygonMode
		VK_CULL_MODE_NONE,												// VkCullModeFlags								cullMode
		VK_FRONT_FACE_COUNTER_CLOCKWISE,								// VkFrontFace									frontFace
		VK_FALSE,														// VkBool32										depthBiasEnable
		0.0f,															// float										depthBiasConstantFactor
		0.0f,															// float										depthBiasClamp
		0.0f,															// float										depthBiasSlopeFactor
		1.0f															// float										lineWidth
	};

	const VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,		// VkStructureType								sType
		DE_NULL,														// const void*									pNext
		0u,																// VkPipelineMultisampleStateCreateFlags		flags
		VK_SAMPLE_COUNT_1_BIT,											// VkSampleCountFlagBits						rasterizationSamples
		VK_FALSE,														// VkBool32										sampleShadingEnable
		1.0f,															// float										minSampleShading
		DE_NULL,														// const VkSampleMask*							pSampleMask
		VK_FALSE,														// VkBool32										alphaToCoverageEnable
		VK_FALSE														// VkBool32										alphaToOneEnable
	};

	const VkStencilOpState stencilOpState
	{
		VK_STENCIL_OP_KEEP,												// VkStencilOp									failOp
		VK_STENCIL_OP_KEEP,												// VkStencilOp									passOp
		VK_STENCIL_OP_KEEP,												// VkStencilOp									depthFailOp
		VK_COMPARE_OP_NEVER,											// VkCompareOp									compareOp
		0,																// deUint32										compareMask
		0,																// deUint32										writeMask
		0																// deUint32										reference
	};

	const VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,		// VkStructureType								sType
		DE_NULL,														// const void*									pNext
		0u,																// VkPipelineDepthStencilStateCreateFlags		flags
		VK_FALSE,														// VkBool32										depthTestEnable
		VK_FALSE,														// VkBool32										depthWriteEnable
		VK_COMPARE_OP_LESS_OR_EQUAL,									// VkCompareOp									depthCompareOp
		VK_FALSE,														// VkBool32										depthBoundsTestEnable
		VK_FALSE,														// VkBool32										stencilTestEnable
		stencilOpState,													// VkStencilOpState								front
		stencilOpState,													// VkStencilOpState								back
		0.0f,															// float										minDepthBounds
		1.0f,															// float										maxDepthBounds
	};

	const VkPipelineColorBlendAttachmentState colorBlendAttachmentState
	{
		VK_FALSE,														// VkBool32										blendEnable
		VK_BLEND_FACTOR_ZERO,											// VkBlendFactor								srcColorBlendFactor
		VK_BLEND_FACTOR_ZERO,											// VkBlendFactor								dstColorBlendFactor
		VK_BLEND_OP_ADD,												// VkBlendOp									colorBlendOp
		VK_BLEND_FACTOR_ZERO,											// VkBlendFactor								srcAlphaBlendFactor
		VK_BLEND_FACTOR_ZERO,											// VkBlendFactor								dstAlphaBlendFactor
		VK_BLEND_OP_ADD,												// VkBlendOp									alphaBlendOp
		VK_COLOR_COMPONENT_R_BIT										// VkColorComponentFlags						colorWriteMask
		| VK_COLOR_COMPONENT_G_BIT
		| VK_COLOR_COMPONENT_B_BIT
		| VK_COLOR_COMPONENT_A_BIT
	};

	const VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,		// VkStructureType								sType
		DE_NULL,														// const void*									pNext
		0u,																// VkPipelineColorBlendStateCreateFlags			flags
		VK_FALSE,														// VkBool32										logicOpEnable
		VK_LOGIC_OP_CLEAR,												// VkLogicOp									logicOp
		1u,																// deUint32										attachmentCount
		&colorBlendAttachmentState,										// const VkPipelineColorBlendAttachmentState*	pAttachments
		{ 0.0f, 0.0f, 0.0f, 0.0f }										// float										blendConstants[4]
	};

	const VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo
	{
		VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,			// VkStructureType								sType
		DE_NULL,														// const void*									pNext
		0u,																// VkPipelineDynamicStateCreateFlags			flags
		0u,																// deUint32										dynamicStateCount
		DE_NULL															// const VkDynamicState*						pDynamicStates
	};

	VkPipelineFragmentShadingRateStateCreateInfoKHR shadingRateStateCreateInfo
	{
		VK_STRUCTURE_TYPE_PIPELINE_FRAGMENT_SHADING_RATE_STATE_CREATE_INFO_KHR,	// VkStructureType						sType;
		DE_NULL,																// const void*							pNext;
		{ 1, 1 },																// VkExtent2D							fragmentSize;
		{ VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR,
		  VK_FRAGMENT_SHADING_RATE_COMBINER_OP_REPLACE_KHR }					// VkFragmentShadingRateCombinerOpKHR	combinerOps[2];
	};

	void* pNext = useShadingRate ? &shadingRateStateCreateInfo : DE_NULL;
	VkPipelineRenderingCreateInfoKHR renderingCreateInfo
	{
		VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
		pNext,
		0u,
		1u,
		&cbFormat,
		VK_FORMAT_UNDEFINED,
		VK_FORMAT_UNDEFINED
	};

	if (m_params->useDynamicRendering)
		pNext = &renderingCreateInfo;

	VkGraphicsPipelineCreateInfo pipelineCreateInfo
	{
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,		// VkStructureType									sType
		pNext,													// const void*										pNext
		0u,														// VkPipelineCreateFlags							flags
		(deUint32)pipelineShaderStageParams.size(),				// deUint32											stageCount
		&pipelineShaderStageParams[0],							// const VkPipelineShaderStageCreateInfo*			pStages
		&vertexInputStateCreateInfo,							// const VkPipelineVertexInputStateCreateInfo*		pVertexInputState
		&inputAssemblyStateCreateInfo,							// const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState
		DE_NULL,												// const VkPipelineTessellationStateCreateInfo*		pTessellationState
		&viewportStateCreateInfo,								// const VkPipelineViewportStateCreateInfo*			pViewportState
		&rasterizationStateCreateInfo,							// const VkPipelineRasterizationStateCreateInfo*	pRasterizationState
		&multisampleStateCreateInfo,							// const VkPipelineMultisampleStateCreateInfo*		pMultisampleState
		&depthStencilStateCreateInfo,							// const VkPipelineDepthStencilStateCreateInfo*		pDepthStencilState
		&colorBlendStateCreateInfo,								// const VkPipelineColorBlendStateCreateInfo*		pColorBlendState
		&dynamicStateCreateInfo,								// const VkPipelineDynamicStateCreateInfo*			pDynamicState
		pipelineLayout,											// VkPipelineLayout									layout
		renderPass,												// VkRenderPass										renderPass
		subpass,												// deUint32											subpass
		DE_NULL,												// VkPipeline										basePipelineHandle
		0														// deInt32											basePipelineIndex;
	};

	if (useShadingRate && m_params->useDynamicRendering)
		pipelineCreateInfo.flags |= VK_PIPELINE_CREATE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;

	VkDevice device = m_device.get() ? *m_device : m_context.getDevice();
	return createGraphicsPipeline(m_context.getDeviceInterface(), device, DE_NULL, &pipelineCreateInfo);
}

Move<VkPipeline> AttachmentRateInstance::buildComputePipeline(VkShaderModule compShader, VkPipelineLayout pipelineLayout) const
{
	const VkPipelineShaderStageCreateInfo stageCreateInfo
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType						sType;
		DE_NULL,												// const void*							pNext;
		0u,														// VkPipelineShaderStageCreateFlags		flags;
		VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits				stage;
		compShader,												// VkShaderModule						module;
		"main",													// const char*							pName;
		DE_NULL													// const VkSpecializationInfo*			pSpecializationInfo;
	};

	const VkComputePipelineCreateInfo createInfo
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,			// VkStructureType						sType;
		DE_NULL,												// const void*							pNext;
		0u,														// VkPipelineCreateFlags				flags;
		stageCreateInfo,										// VkPipelineShaderStageCreateInfo		stage;
		pipelineLayout,											// VkPipelineLayout						layout;
		(VkPipeline)0,											// VkPipeline							basePipelineHandle;
		0u,														// int32_t								basePipelineIndex;
	};

	VkDevice device = m_device.get() ? *m_device : m_context.getDevice();
	return createComputePipeline(m_context.getDeviceInterface(), device, (vk::VkPipelineCache)0u, &createInfo);
}

VkDescriptorSetAllocateInfo AttachmentRateInstance::makeDescriptorSetAllocInfo(VkDescriptorPool descriptorPool, const VkDescriptorSetLayout* pSetLayouts) const
{
	return
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,		// VkStructureType					sType;
		DE_NULL,											// const void*						pNext;
		descriptorPool,										// VkDescriptorPool					descriptorPool;
		1u,													// uint32_t							setLayoutCount;
		pSetLayouts,										// const VkDescriptorSetLayout*		pSetLayouts;
	};
}

void AttachmentRateInstance::startRendering(const VkCommandBuffer					commandBuffer,
											const VkRenderPass						renderPass,
											const VkFramebuffer						framebuffer,
											const VkRect2D&							renderArea,
											const std::vector<FBAttachmentInfo>&	attachmentInfo,
											const deUint32							srTileWidth,
											const deUint32							srTileHeight) const
{
	const DeviceInterface&		vk			(m_context.getDeviceInterface());
	std::vector<VkClearValue>	clearColor	(attachmentInfo.size(), makeClearValueColorU32(0, 0, 0, 0));

	if (m_params->useDynamicRendering)
	{
		VkRenderingFragmentShadingRateAttachmentInfoKHR shadingRateAttachmentInfo
		{
			VK_STRUCTURE_TYPE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR,	// VkStructureType		sType;
			DE_NULL,																// const void*			pNext;
			DE_NULL,																// VkImageView			imageView;
			VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR,			// VkImageLayout		imageLayout;
			{ 0, 0 }																// VkExtent2D			shadingRateAttachmentTexelSize;
		};

		VkRenderingAttachmentInfoKHR colorAttachment
		{
			vk::VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,	// VkStructureType						sType;
			DE_NULL,												// const void*							pNext;
			attachmentInfo[0].view,									// VkImageView							imageView;
			VK_IMAGE_LAYOUT_GENERAL,								// VkImageLayout						imageLayout;
			VK_RESOLVE_MODE_NONE,									// VkResolveModeFlagBits				resolveMode;
			DE_NULL,												// VkImageView							resolveImageView;
			VK_IMAGE_LAYOUT_UNDEFINED,								// VkImageLayout						resolveImageLayout;
			VK_ATTACHMENT_LOAD_OP_CLEAR,							// VkAttachmentLoadOp					loadOp;
			VK_ATTACHMENT_STORE_OP_STORE,							// VkAttachmentStoreOp					storeOp;
			clearColor[0]											// VkClearValue							clearValue;
		};

		VkRenderingInfoKHR renderingInfo
		{
			vk::VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
			DE_NULL,
			0,														// VkRenderingFlagsKHR					flags;
			renderArea,												// VkRect2D								renderArea;
			1u,														// deUint32								layerCount;
			0u,														// deUint32								viewMask;
			1u,														// deUint32								colorAttachmentCount;
			&colorAttachment,										// const VkRenderingAttachmentInfoKHR*	pColorAttachments;
			DE_NULL,												// const VkRenderingAttachmentInfoKHR*	pDepthAttachment;
			DE_NULL,												// const VkRenderingAttachmentInfoKHR*	pStencilAttachment;
		};

		// when shading rate is used it is defined as a second entry in attachmentInfo
		if ((attachmentInfo.size() == 2) &&
			(attachmentInfo[1].usage & VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR))
		{
			shadingRateAttachmentInfo.imageView							= attachmentInfo[1].view;
			shadingRateAttachmentInfo.shadingRateAttachmentTexelSize	= { srTileWidth, srTileHeight };
			renderingInfo.pNext											= &shadingRateAttachmentInfo;
		}

		vk.cmdBeginRendering(commandBuffer, &renderingInfo);

		return;
	}

	std::vector<VkImageView>			attachments(attachmentInfo.size(), 0);
	VkRenderPassAttachmentBeginInfo		renderPassAttachmentBeginInfo;
	void*								pNext(DE_NULL);

	if (m_params->useImagelessFramebuffer)
	{
		// create array containing attachment views
		for (deUint32 i = 0; i < (deUint32)attachmentInfo.size(); ++i)
			attachments[i] = attachmentInfo[i].view;

		renderPassAttachmentBeginInfo =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO,		// VkStructureType		sType;
			DE_NULL,													// const void*			pNext;
			(deUint32)attachments.size(),								// deUint32				attachmentCount;
			attachments.data()											// const VkImageView*	pAttachments;
		};

		pNext = &renderPassAttachmentBeginInfo;
	}

	beginRenderPass(vk, commandBuffer, renderPass, framebuffer, renderArea,
					(deUint32)clearColor.size(), clearColor.data(), VK_SUBPASS_CONTENTS_INLINE, pNext);
}

void AttachmentRateInstance::finishRendering(const VkCommandBuffer commandBuffer) const
{
	const DeviceInterface& vk = m_context.getDeviceInterface();

	if (m_params->useDynamicRendering)
		endRendering(vk, commandBuffer);
	else
		endRenderPass(vk, commandBuffer);
}

tcu::TestStatus AttachmentRateInstance::iterate(void)
{
	// instead of creating many classes that derive from large common class
	// each test mode is defined in separate run* method, those methods
	// then use same helper methods defined in this class

	typedef bool (AttachmentRateInstance::*MethodPtr)();
	const std::map<TestMode, MethodPtr> modeFuncMap
	{
		{ TM_SETUP_RATE_WITH_ATOMICS_IN_COMPUTE_SHADER,							&AttachmentRateInstance::runComputeShaderMode },
		{ TM_SETUP_RATE_WITH_FRAGMENT_SHADER,									&AttachmentRateInstance::runFragmentShaderMode },
		{ TM_SETUP_RATE_WITH_COPYING_FROM_OTHER_IMAGE,							&AttachmentRateInstance::runCopyMode },
		{ TM_SETUP_RATE_WITH_COPYING_FROM_EXCLUSIVE_IMAGE_USING_TRANSFER_QUEUE,	&AttachmentRateInstance::runCopyModeOnTransferQueue },
		{ TM_SETUP_RATE_WITH_COPYING_FROM_CONCURENT_IMAGE_USING_TRANSFER_QUEUE,	&AttachmentRateInstance::runCopyModeOnTransferQueue },
		{ TM_SETUP_RATE_WITH_LINEAR_TILED_IMAGE,								&AttachmentRateInstance::runFillLinearTiledImage },
		{ TM_TWO_SUBPASS,														&AttachmentRateInstance::runTwoSubpassMode },
	};

	if ((this->*modeFuncMap.at(m_params->mode))())
		return tcu::TestStatus::pass("Pass");

	return tcu::TestStatus::fail("Fail");
}

bool AttachmentRateInstance::verifyUsingAtomicChecks(deUint32 tileWidth, deUint32 tileHeight,
													 deUint32 rateWidth, deUint32 rateHeight,
													 deUint32* outBufferPtr) const
{
	tcu::TestLog&			log					(m_context.getTestContext().getLog());
	tcu::TextureLevel		errorMaskStorage	(tcu::TextureFormat(tcu::TextureFormat::RGB, tcu::TextureFormat::UNORM_INT8), m_cbWidth, m_cbHeight, 1u);
	tcu::PixelBufferAccess	errorMaskAccess		(errorMaskStorage.getAccess());

	deUint32		wrongFragments						= 0;
	const deUint32	fragmentsWithSameAtomicValueCount	= rateWidth * rateHeight;

	// map that uses atomic value as a kay and maps it to all fragments sharing same atomic
	std::map<deUint32, std::vector<tcu::UVec2> > fragmentsWithSameAtomicValueMap;

	// this method asumes that top and left edge of triangle are parallel to axes
	// and we can store just single coordinate for those edges
	deUint32 triangleLeftEdgeX	= 0;
	deUint32 triangleTopEdgeY	= 0;

	// this method assumes that greatest angle in the triangle points to the top-left corner of FB;
	// this vector will then store fragments on the right edge of triangle; vector index represents y coordinate and value is x
	std::vector<deUint32> fragmentsOnTheRightTriangleEdgeVect(m_cbHeight, 0);

	tcu::clear(errorMaskAccess, tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0));

	// loop over all fragments and validate the output
	for (deUint32 cbFragmentY = 0; cbFragmentY < m_cbHeight; ++cbFragmentY)
	for (deUint32 cbFragmentX = 0; cbFragmentX < m_cbWidth;  ++cbFragmentX)
	{
		deUint32* fragmentColor = &outBufferPtr[4 * (cbFragmentY * m_cbWidth + cbFragmentX)];

		// fragment not covered by primitive, skip it
		if (fragmentColor[2] == 0)
			continue;

		// first fragment we hit will define top and left triangle edges
		if ((triangleTopEdgeY + triangleLeftEdgeX) == 0)
		{
			triangleLeftEdgeX	= cbFragmentX;
			triangleTopEdgeY	= cbFragmentY;
		}

		// constantly overwrite coordinate on right edge so that we are left with the farthest one
		fragmentsOnTheRightTriangleEdgeVect[cbFragmentY] = cbFragmentX;

		// make sure that fragment g and a components are 0
		if ((fragmentColor[1] != 0) || (fragmentColor[3] != 0))
		{
			++wrongFragments;
			continue;
		}

		deUint32 rate			= fragmentColor[0];
		deUint32 fragmentRateX	= 1 << ((rate / 4) & 3);
		deUint32 fragmentRateY	= 1 << (rate & 3);

		// check if proper rate was used for fragment
		if ((fragmentRateX != rateWidth) ||
			(fragmentRateY != rateHeight))
		{
			++wrongFragments;
			errorMaskAccess.setPixel(tcu::Vec4(1.0f, 0.5f, 0.0f, 1.0f), cbFragmentX, cbFragmentY, 0u);
			continue;
		}

		// mark correct fragments using few green shades so rates are visible
		deUint32 atomicValue = fragmentColor[2];
		errorMaskAccess.setPixel(tcu::Vec4(0.0f, 1.0f - float(atomicValue % 7) * 0.1f, 0.0f, 1.0f), cbFragmentX, cbFragmentY, 0u);

		// find proper set in map and add value to it after doing verification with existing items
		auto fragmentsSetMapIt = fragmentsWithSameAtomicValueMap.find(atomicValue);
		if (fragmentsSetMapIt == fragmentsWithSameAtomicValueMap.end())
		{
			fragmentsWithSameAtomicValueMap[atomicValue] = { tcu::UVec2(cbFragmentX, cbFragmentY) };
			fragmentsWithSameAtomicValueMap[atomicValue].reserve(fragmentsWithSameAtomicValueCount);
		}
		else
		{
			// make sure that fragments added to set are near the top-left fragment
			auto& fragmentsSet = fragmentsSetMapIt->second;
			if (((cbFragmentX - fragmentsSet[0].x()) > rateWidth) ||
				((cbFragmentY - fragmentsSet[0].y()) > rateHeight))
			{
				++wrongFragments;
				errorMaskAccess.setPixel(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f), cbFragmentX, cbFragmentY, 0u);
			}

			fragmentsWithSameAtomicValueMap[atomicValue].emplace_back(cbFragmentX, cbFragmentY);
		}
	}

	// check if there are no valid fragmenst at all
	if ((triangleTopEdgeY + triangleLeftEdgeX) == 0)
	{
		log << tcu::TestLog::Message
			<< "No valid fragments."
			<< tcu::TestLog::EndMessage;
		return false;
	}

	// if checks failed skip checking other tile sizes
	if (wrongFragments)
	{
		log << tcu::TestLog::Message
			<< "Failed " << wrongFragments << " fragments for tileWidth: " << tileWidth << ", tileHeight: " << tileHeight
			<< tcu::TestLog::EndMessage
			<< tcu::TestLog::Image("ErrorMask", "Error mask", errorMaskAccess);
		return false;
	}

	// do additional checks
	tcu::Vec4 fragmentColor(0.0f, 1.0f, 0.0f, 1.0f);

	tcu::clear(errorMaskAccess, tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0));

	// make sure that there is same number of fragments that share same atomic value
	for (auto& fragmentsSetMapIt : fragmentsWithSameAtomicValueMap)
	{
		// mark correct fragments using few green shades so rates are visible
		fragmentColor = tcu::Vec4(0.0f, 1.0f - float(fragmentsSetMapIt.first % 7) * 0.1f, 0.0f, 1.0f);

		const auto& fragmentSet = fragmentsSetMapIt.second;;
		if (fragmentSet.size() != fragmentsWithSameAtomicValueCount)
		{
			const auto&	topLeftFragment		= fragmentSet[0];
			deUint32	triangleRightEdgeX	= fragmentsOnTheRightTriangleEdgeVect[topLeftFragment.y()];

			// we can only count this as an error if set is fully inside of triangle, sets on
			// edges may not have same number of fragments as sets fully located in the triangle
			if ((topLeftFragment.y() > (triangleTopEdgeY)) &&
				(topLeftFragment.x() > (triangleLeftEdgeX)) &&
				(topLeftFragment.x() < (triangleRightEdgeX - rateWidth)))
			{
				wrongFragments += (deUint32)fragmentSet.size();
				fragmentColor	= tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f);
			}
		}

		// mark all fragmens from set with proper color
		for (auto& fragment : fragmentSet)
			errorMaskAccess.setPixel(fragmentColor, fragment.x(), fragment.y(), 0u);
	}

	if (wrongFragments)
	{
		log << tcu::TestLog::Message
			<< "Wrong number of fragments with same atomic value (" << wrongFragments << ") for tileWidth: " << tileWidth << ", tileHeight: " << tileHeight
			<< tcu::TestLog::EndMessage
			<< tcu::TestLog::Image("ErrorMask", "Error mask", errorMaskAccess);
		return false;
	}

	return true;
}

bool AttachmentRateInstance::runComputeShaderMode(void)
{
	// clear the shading rate attachment, then using a compute shader, set the shading rate attachment
	// values to the desired rate using various atomic operations, then use it to draw a basic triangle
	// and do basic checks

	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	VkDevice				device				= m_context.getDevice();
	deUint32				queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	VkMemoryBarrier			memoryBarrier		{ VK_STRUCTURE_TYPE_MEMORY_BARRIER, DE_NULL, 0u, 0u };

	Move<VkShaderModule>	compShader			= createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"), 0);
	Move<VkShaderModule>	vertShader			= createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0);
	Move<VkShaderModule>	fragShader			= createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0);

	Move<VkCommandPool>		cmdPool				= createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
	Move<VkCommandBuffer>	cmdBuffer			= allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	// setup descriptor set with storage image for compute pipeline
	Move<VkDescriptorSetLayout>			computeDescriptorSetLayout		= DescriptorSetLayoutBuilder()
																			.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
																			.build(vk, device);
	Move<VkDescriptorPool>				computeDescriptorPool			= DescriptorPoolBuilder()
																			.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1u)
																			.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	const VkDescriptorSetAllocateInfo	computeDescriptorSetAllocInfo	= makeDescriptorSetAllocInfo(*computeDescriptorPool, &(*computeDescriptorSetLayout));
	Move<VkDescriptorSet>				computeDescriptorSet			= allocateDescriptorSet(vk, device, &computeDescriptorSetAllocInfo);

	m_srUsage |= VK_IMAGE_USAGE_STORAGE_BIT;

	buildCounterBufferObjects();
	buildColorBufferObjects(0, m_cbUsage);

	// iterate over all possible tile sizes
	for (deUint32 tileWidth  = m_minTileSize.width;  tileWidth  <= m_maxTileSize.width;  tileWidth  *= 2)
	for (deUint32 tileHeight = m_minTileSize.height; tileHeight <= m_maxTileSize.height; tileHeight *= 2)
	{
		// skip tile sizes that have unsuported aspect ratio
		deUint32 aspectRatio = (tileHeight > tileWidth) ? (tileHeight / tileWidth) : (tileWidth / tileHeight);
		if (aspectRatio > m_maxAspectRatio)
			continue;

		// calculate size of shading rate attachment
		deUint32 srWidth  = (m_cbWidth  + tileWidth  - 1) / tileWidth;
		deUint32 srHeight = (m_cbHeight + tileHeight - 1) / tileHeight;

		buildShadingRateObjects(0, srWidth, srHeight, m_srUsage);

		const VkDescriptorImageInfo  computeDescriptorInfo  = makeDescriptorImageInfo(DE_NULL, *m_srImageView[0], VK_IMAGE_LAYOUT_GENERAL);
		DescriptorSetUpdateBuilder()
			.writeSingle(*computeDescriptorSet,  DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  &computeDescriptorInfo)
			.update(vk, device);

		Move<VkPipelineLayout>	computePipelineLayout	= buildPipelineLayout(&(*computeDescriptorSetLayout));
		Move<VkPipelineLayout>	graphicsPipelineLayout	= buildPipelineLayout(&(*m_counterBufferDescriptorSetLayout));
		Move<VkPipeline>		computePipeline			= buildComputePipeline(*compShader, *computePipelineLayout);
		Move<VkRenderPass>		renderPass				= buildRenderPass(m_cbFormat, tileWidth, tileHeight);
		Move<VkPipeline>		graphicsPipeline		= buildGraphicsPipeline(0, *renderPass, m_cbFormat, *graphicsPipelineLayout, *vertShader, *fragShader);

		std::vector<FBAttachmentInfo> attachmentInfo
		{
			{ m_cbFormat,			m_cbUsage, m_cbWidth, m_cbHeight, *m_cbImageView[0] },
			{ m_params->srFormat,	m_srUsage, srWidth,   srHeight,   *m_srImageView[0] }
		};
		Move<VkFramebuffer> framebuffer = buildFramebuffer(*renderPass, attachmentInfo);

		beginCommandBuffer(vk, *cmdBuffer, 0u);

		// wait till sr image layout is changed
		VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		VkImageMemoryBarrier srImageBarrierGeneral =
			makeImageMemoryBarrier(
				VK_ACCESS_NONE_KHR,
				VK_ACCESS_NONE_KHR,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_GENERAL,
				**m_srImage[0],
				m_defaultImageSubresourceRange);
		vk.cmdPipelineBarrier(*cmdBuffer, srcStageMask, dstStageMask, 0, 0, DE_NULL, 0, DE_NULL, 1, &srImageBarrierGeneral);

		// fill sr image using compute shader
		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipeline);
		vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipelineLayout, 0, 1, &(*computeDescriptorSet), 0, DE_NULL);
		vk.cmdDispatch(*cmdBuffer, srWidth, srHeight, 1);

		// wait till sr image is ready and change sr images layout
		srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;
		memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		memoryBarrier.dstAccessMask = VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR;
		VkImageMemoryBarrier srImageBarrierShadingRate =
			makeImageMemoryBarrier(
				VK_ACCESS_SHADER_WRITE_BIT,
				VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR,
				VK_IMAGE_LAYOUT_GENERAL,
				VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR,
				**m_srImage[0],
				m_defaultImageSubresourceRange);
		vk.cmdPipelineBarrier(*cmdBuffer, srcStageMask, dstStageMask, 0, 1, &memoryBarrier, 0, DE_NULL, 1, &srImageBarrierShadingRate);

		// wait till cb image layout is changed
		srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkImageMemoryBarrier cbImageBarrier =
			makeImageMemoryBarrier(
				VK_ACCESS_NONE_KHR,
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_GENERAL,
				**m_cbImage[0],
				m_defaultImageSubresourceRange);
		vk.cmdPipelineBarrier(*cmdBuffer, srcStageMask, dstStageMask, 0, 0, DE_NULL, 0, DE_NULL, 1, &cbImageBarrier);

		startRendering(*cmdBuffer, *renderPass, *framebuffer, makeRect2D(m_cbWidth, m_cbHeight), attachmentInfo, tileWidth, tileHeight);

		// draw single triangle to cb
		vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipelineLayout, 0, 1, &(*m_counterBufferDescriptorSet), 0, DE_NULL);
		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);
		vk.cmdDraw(*cmdBuffer, 3u, 1, 0u, 0u);

		finishRendering(*cmdBuffer);

		// wait till color attachment is fully written
		srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
		memoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		memoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		vk.cmdPipelineBarrier(*cmdBuffer, srcStageMask, dstStageMask, 0, 1, &memoryBarrier, 0, DE_NULL, 0, DE_NULL);

		// read back color buffer image
		vk.cmdCopyImageToBuffer(*cmdBuffer, **m_cbImage[0], VK_IMAGE_LAYOUT_GENERAL, **m_cbReadBuffer[0], 1u, &m_defaultBufferImageCopy);

		endCommandBuffer(vk, *cmdBuffer);

		// submit commands and wait
		const VkQueue queue = m_context.getUniversalQueue();
		submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

		invalidateAlloc(vk, device, m_cbReadBuffer[0]->getAllocation());
		if (!verifyUsingAtomicChecks(tileWidth, tileHeight,
									 m_params->srRate.width, m_params->srRate.height,
									 (deUint32*)m_cbReadBuffer[0]->getAllocation().getHostPtr()))
			return false;

	}  // iterate over all possible tile sizes

	return true;
}

bool AttachmentRateInstance::runFragmentShaderMode(void)
{
	// Set up the image as a color attachment, and render rate to it,
	// then use it to draw a basic triangle and do basic checks

	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	VkDevice				device				= m_context.getDevice();
	deUint32				queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	VkMemoryBarrier			memoryBarrier		{ VK_STRUCTURE_TYPE_MEMORY_BARRIER, DE_NULL, 0u, 0u };

	Move<VkShaderModule>	vertSetupShader		= createShaderModule(vk, device, m_context.getBinaryCollection().get("vert_setup"), 0);
	Move<VkShaderModule>	fragSetupShader		= createShaderModule(vk, device, m_context.getBinaryCollection().get("frag_setup"), 0);
	Move<VkShaderModule>	vertShader			= createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0);
	Move<VkShaderModule>	fragShader			= createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0);

	Move<VkCommandPool>		cmdPool				= createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
	Move<VkCommandBuffer>	cmdBuffer			= allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	m_srUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	buildCounterBufferObjects();
	buildColorBufferObjects(0, m_cbUsage);

	// iterate over all possible tile sizes
	for (deUint32 tileWidth  = m_minTileSize.width;  tileWidth  <= m_maxTileSize.width;  tileWidth  *= 2)
	for (deUint32 tileHeight = m_minTileSize.height; tileHeight <= m_maxTileSize.height; tileHeight *= 2)
	{
		// skip tile sizes that have unsuported aspect ratio
		deUint32 aspectRatio = (tileHeight > tileWidth) ? (tileHeight / tileWidth) : (tileWidth / tileHeight);
		if (aspectRatio > m_maxAspectRatio)
			continue;

		// calculate size of shading rate attachment
		deUint32 srWidth  = (m_cbWidth  + tileWidth  - 1) / tileWidth;
		deUint32 srHeight = (m_cbHeight + tileHeight - 1) / tileHeight;

		buildShadingRateObjects(0, srWidth, srHeight, m_srUsage);

		Move<VkPipelineLayout>	setupPipelineLayout	= buildPipelineLayout();
		Move<VkPipelineLayout>	ratePipelineLayout	= buildPipelineLayout(&(*m_counterBufferDescriptorSetLayout));
		Move<VkRenderPass>		setupRenderPass		= buildRenderPass(m_params->srFormat);
		Move<VkRenderPass>		rateRenderPass		= buildRenderPass(m_cbFormat, tileWidth, tileHeight);
		Move<VkPipeline>		setupPipeline		= buildGraphicsPipeline(0, *setupRenderPass, m_params->srFormat, *setupPipelineLayout, *vertSetupShader, *fragSetupShader, DE_FALSE);
		Move<VkPipeline>		ratePipeline		= buildGraphicsPipeline(0, *rateRenderPass, m_cbFormat, *ratePipelineLayout, *vertShader, *fragShader);

		std::vector<FBAttachmentInfo> setupAttachmentInfo
		{
			{ m_params->srFormat, m_srUsage, srWidth, srHeight, *m_srImageView[0] }
		};
		std::vector<FBAttachmentInfo> rateAttachmentInfo
		{
			{ m_cbFormat,			m_cbUsage, m_cbWidth, m_cbHeight, *m_cbImageView[0] },
			{ m_params->srFormat,	m_srUsage, srWidth,   srHeight,   *m_srImageView[0] }
		};
		Move<VkFramebuffer> setupFramebuffer	= buildFramebuffer(*setupRenderPass, setupAttachmentInfo);
		Move<VkFramebuffer> rateFramebuffer		= buildFramebuffer(*rateRenderPass, rateAttachmentInfo);

		beginCommandBuffer(vk, *cmdBuffer, 0u);

		// wait till sr image layout is changed
		VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkImageMemoryBarrier srImageBarrierGeneral =
			makeImageMemoryBarrier(
				VK_ACCESS_NONE_KHR,
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_GENERAL,
				**m_srImage[0],
				m_defaultImageSubresourceRange);
		vk.cmdPipelineBarrier(*cmdBuffer, srcStageMask, dstStageMask, 0, 0, DE_NULL, 0, DE_NULL, 1, &srImageBarrierGeneral);

		// render rate to sr image
		startRendering(*cmdBuffer, *setupRenderPass, *setupFramebuffer, makeRect2D(srWidth, srHeight), setupAttachmentInfo);

		// draw single triangle to cb
		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *setupPipeline);
		vk.cmdDraw(*cmdBuffer, 3u, 1, 0u, 0u);

		finishRendering(*cmdBuffer);

		// wait till sr image is ready and change sr images layout
		srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;
		VkImageMemoryBarrier srImageBarrierShadingRate =
			makeImageMemoryBarrier(
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR,
				VK_IMAGE_LAYOUT_GENERAL,
				VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR,
				**m_srImage[0],
				m_defaultImageSubresourceRange);
		vk.cmdPipelineBarrier(*cmdBuffer, srcStageMask, dstStageMask, 0, 0, DE_NULL, 0, DE_NULL, 1, &srImageBarrierShadingRate);

		// wait till cb image layout is changed
		srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkImageMemoryBarrier cbImageBarrier =
			makeImageMemoryBarrier(
				VK_ACCESS_NONE_KHR,
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_GENERAL,
				**m_cbImage[0],
				m_defaultImageSubresourceRange);
		vk.cmdPipelineBarrier(*cmdBuffer, srcStageMask, dstStageMask, 0, 0, DE_NULL, 0, DE_NULL, 1, &cbImageBarrier);

		startRendering(*cmdBuffer, *rateRenderPass, *rateFramebuffer, makeRect2D(m_cbWidth, m_cbHeight), rateAttachmentInfo, tileWidth, tileHeight);

		// draw single triangle to cb
		vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *ratePipelineLayout, 0, 1, &(*m_counterBufferDescriptorSet), 0, DE_NULL);
		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *ratePipeline);
		vk.cmdDraw(*cmdBuffer, 3u, 1, 0u, 0u);

		finishRendering(*cmdBuffer);

		// wait till color attachment is fully written
		srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
		memoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		memoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		vk.cmdPipelineBarrier(*cmdBuffer, srcStageMask, dstStageMask, 0, 1, &memoryBarrier, 0, DE_NULL, 0, DE_NULL);

		// read back color buffer image
		vk.cmdCopyImageToBuffer(*cmdBuffer, **m_cbImage[0], VK_IMAGE_LAYOUT_GENERAL, **m_cbReadBuffer[0], 1u, &m_defaultBufferImageCopy);

		endCommandBuffer(vk, *cmdBuffer);

		// submit commands and wait
		const VkQueue queue = m_context.getUniversalQueue();
		submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

		invalidateAlloc(vk, device, m_cbReadBuffer[0]->getAllocation());
		if (!verifyUsingAtomicChecks(tileWidth, tileHeight,
									 m_params->srRate.width, m_params->srRate.height,
									 (deUint32*)m_cbReadBuffer[0]->getAllocation().getHostPtr()))
			return false;

	} // iterate over all possible tile sizes

	return true;
}

bool AttachmentRateInstance::runCopyMode (void)
{
	// Clear a separate image of the same format to that rate, copy it to
	// the shading rate image, then use it to draw a basic triangle and do basic checks

	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	VkDevice				device				= m_context.getDevice();
	deUint32				queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	VkMemoryBarrier			memoryBarrier		{ VK_STRUCTURE_TYPE_MEMORY_BARRIER, DE_NULL, 0u, 0u };

	Move<VkShaderModule>	vertShader			= createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0);
	Move<VkShaderModule>	fragShader			= createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0);

	Move<VkCommandPool>		cmdPool				= createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
	Move<VkCommandBuffer>	cmdBuffer			= allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	buildCounterBufferObjects();
	buildColorBufferObjects(0, m_cbUsage);

	// iterate over all possible tile sizes
	for (deUint32 tileWidth  = m_minTileSize.width;  tileWidth  <= m_maxTileSize.width;  tileWidth  *= 2)
	for (deUint32 tileHeight = m_minTileSize.height; tileHeight <= m_maxTileSize.height; tileHeight *= 2)
	{
		// skip tile sizes that have unsuported aspect ratio
		deUint32 aspectRatio = (tileHeight > tileWidth) ? (tileHeight / tileWidth) : (tileWidth / tileHeight);
		if (aspectRatio > m_maxAspectRatio)
			continue;

		// calculate size of shading rate attachment
		deUint32 srWidth  = (m_cbWidth  + tileWidth  - 1) / tileWidth;
		deUint32 srHeight = (m_cbHeight + tileHeight - 1) / tileHeight;

		buildShadingRateObjects(0, srWidth, srHeight, m_srUsage);

		// create image that will be source for shading rate image
		de::MovePtr<ImageWithMemory> srSrcImage = buildImageWithMemory(m_params->srFormat, srWidth, srHeight, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

		Move<VkPipelineLayout>	graphicsPipelineLayout	= buildPipelineLayout(&(*m_counterBufferDescriptorSetLayout));
		Move<VkRenderPass>		renderPass				= buildRenderPass(m_cbFormat, tileWidth, tileHeight);
		Move<VkPipeline>		graphicsPipeline		= buildGraphicsPipeline(0, *renderPass, m_cbFormat, *graphicsPipelineLayout, *vertShader, *fragShader);

		std::vector<FBAttachmentInfo> attachmentInfo
		{
			{ m_cbFormat,			m_cbUsage, m_cbWidth, m_cbHeight, *m_cbImageView[0] },
			{ m_params->srFormat,	m_srUsage, srWidth,   srHeight,   *m_srImageView[0] }
		};
		Move<VkFramebuffer> framebuffer = buildFramebuffer(*renderPass, attachmentInfo);

		beginCommandBuffer(vk, *cmdBuffer, 0u);

		// wait till sr images layout are changed
		VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
		VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
		std::vector<VkImageMemoryBarrier> srImageBarrierGeneral(2,
			makeImageMemoryBarrier(
				VK_ACCESS_NONE_KHR,
				VK_ACCESS_NONE_KHR,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_GENERAL,
				**m_srImage[0],
				m_defaultImageSubresourceRange));
		srImageBarrierGeneral[1].image = **srSrcImage;
		vk.cmdPipelineBarrier(*cmdBuffer, srcStageMask, dstStageMask, 0, 0, DE_NULL, 0, DE_NULL, 2, srImageBarrierGeneral.data());

		// clear source sr image with proper rate
		VkClearColorValue clearValue = { { 0, 0, 0, 0 } };
		clearValue.uint32[0] = calculateRate(m_params->srRate.width, m_params->srRate.height);
		vk.cmdClearColorImage(*cmdBuffer, **srSrcImage, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &m_defaultImageSubresourceRange);

		// wait till sr source image is ready
		srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
		memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		memoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		vk.cmdPipelineBarrier(*cmdBuffer, srcStageMask, dstStageMask, 0, 1, &memoryBarrier, 0, DE_NULL, 0, DE_NULL);

		// copy sr source image to sr image used during rendering
		VkImageCopy imageCopyRegion
		{
			m_defaultImageSubresourceLayers,	// VkImageSubresourceLayers	srcSubresource;
			{0, 0, 0},							// VkOffset3D				srcOffset;
			m_defaultImageSubresourceLayers,	// VkImageSubresourceLayers	dstSubresource;
			{0, 0, 0},							// VkOffset3D				dstOffset;
			{ srWidth, srHeight, 1u }			// VkExtent3D				extent;
		};
		vk.cmdCopyImage(*cmdBuffer, **srSrcImage, VK_IMAGE_LAYOUT_GENERAL, **m_srImage[0], VK_IMAGE_LAYOUT_GENERAL, 1, &imageCopyRegion);

		// wait till sr image is ready and change sr images layout
		srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;
		memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		memoryBarrier.dstAccessMask = VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR;
		VkImageMemoryBarrier srImageBarrierShadingRate =
			makeImageMemoryBarrier(
				VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR,
				VK_IMAGE_LAYOUT_GENERAL,
				VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR,
				**m_srImage[0],
				m_defaultImageSubresourceRange);
		vk.cmdPipelineBarrier(*cmdBuffer, srcStageMask, dstStageMask, 0, 1, &memoryBarrier, 0, DE_NULL, 1, &srImageBarrierShadingRate);

		// wait till cb image layout is changed
		srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkImageMemoryBarrier cbImageBarrier =
			makeImageMemoryBarrier(
				VK_ACCESS_NONE_KHR,
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_GENERAL,
				**m_cbImage[0],
				m_defaultImageSubresourceRange);
		vk.cmdPipelineBarrier(*cmdBuffer, srcStageMask, dstStageMask, 0, 0, DE_NULL, 0, DE_NULL, 1, &cbImageBarrier);

		startRendering(*cmdBuffer, *renderPass, *framebuffer, makeRect2D(m_cbWidth, m_cbHeight), attachmentInfo, tileWidth, tileHeight);

		// draw single triangle to cb
		vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipelineLayout, 0, 1, &(*m_counterBufferDescriptorSet), 0, DE_NULL);
		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);
		vk.cmdDraw(*cmdBuffer, 3u, 1, 0u, 0u);

		finishRendering(*cmdBuffer);

		// wait till color attachment is fully written
		srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
		memoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		memoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		vk.cmdPipelineBarrier(*cmdBuffer, srcStageMask, dstStageMask, 0, 1, &memoryBarrier, 0, DE_NULL, 0, DE_NULL);

		// read back color buffer image
		vk.cmdCopyImageToBuffer(*cmdBuffer, **m_cbImage[0], VK_IMAGE_LAYOUT_GENERAL, **m_cbReadBuffer[0], 1u, &m_defaultBufferImageCopy);

		endCommandBuffer(vk, *cmdBuffer);

		// submit commands and wait
		const VkQueue queue = m_context.getUniversalQueue();
		submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

		invalidateAlloc(vk, device, m_cbReadBuffer[0]->getAllocation());
		if (!verifyUsingAtomicChecks(tileWidth, tileHeight,
									 m_params->srRate.width, m_params->srRate.height,
									 (deUint32*)m_cbReadBuffer[0]->getAllocation().getHostPtr()))
			return false;

	} // iterate over all possible tile sizes

	return true;
}

bool AttachmentRateInstance::runCopyModeOnTransferQueue(void)
{
	// Clear a separate image of the same format to that rate, copy it to
	// the shading rate image on separate transfer queue and then use copied
	// image to draw a basic triangle and do basic checks

	const DeviceInterface&					vk							= m_context.getDeviceInterface();
	const PlatformInterface&				vkp							= m_context.getPlatformInterface();
	const InstanceInterface&				vki							= m_context.getInstanceInterface();
	VkPhysicalDevice						pd							= m_context.getPhysicalDevice();
	deUint32								transferQueueFamilyIndex	= std::numeric_limits<deUint32>::max();
	deUint32								graphicsQueueFamilyIndex	= std::numeric_limits<deUint32>::max();
	VkMemoryBarrier							memoryBarrier				{ VK_STRUCTURE_TYPE_MEMORY_BARRIER, DE_NULL, 0u, 0u };
	std::vector<VkQueueFamilyProperties>	queueFamilyProperties		= getPhysicalDeviceQueueFamilyProperties(vki, pd);

	// find graphics and transfer queue families
	for (deUint32 queueNdx = 0; queueNdx < queueFamilyProperties.size(); queueNdx++)
	{
		VkQueueFlags queueFlags = queueFamilyProperties[queueNdx].queueFlags;
		if ((graphicsQueueFamilyIndex == std::numeric_limits<deUint32>::max()) && (queueFlags & VK_QUEUE_GRAPHICS_BIT))
			graphicsQueueFamilyIndex = queueNdx;
		else if ((queueNdx != graphicsQueueFamilyIndex) && (queueFlags & VK_QUEUE_TRANSFER_BIT))
			transferQueueFamilyIndex = queueNdx;
	}
	if (transferQueueFamilyIndex == std::numeric_limits<deUint32>::max())
		TCU_THROW(NotSupportedError, "No separate transfer queue");

	// using queueFamilies vector to determine if sr image uses exclusiv or concurrent sharing
	std::vector<deUint32> queueFamilies;
	if (m_params->mode == TM_SETUP_RATE_WITH_COPYING_FROM_CONCURENT_IMAGE_USING_TRANSFER_QUEUE)
		queueFamilies = { graphicsQueueFamilyIndex, transferQueueFamilyIndex };

	// create custom device
	{
		const float queuePriorities = 1.0f;
		std::vector<VkDeviceQueueCreateInfo> queueInfo(2,
			{
				VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,		// VkStructureType					sType;
				DE_NULL,										// const void*						pNext;
				(VkDeviceQueueCreateFlags)0u,					// VkDeviceQueueCreateFlags			flags;
				transferQueueFamilyIndex,						// uint32_t							queueFamilyIndex;
				1u,												// uint32_t							queueCount;
				&queuePriorities								// const float*						pQueuePriorities;
			});
		queueInfo[1].queueFamilyIndex = graphicsQueueFamilyIndex;

		VkPhysicalDeviceFeatures deviceFeatures;
		vki.getPhysicalDeviceFeatures(pd, &deviceFeatures);

		VkPhysicalDeviceFragmentShadingRateFeaturesKHR	fsrFeatures				{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR, DE_NULL, DE_FALSE, DE_FALSE, DE_TRUE };
		VkPhysicalDeviceImagelessFramebufferFeaturesKHR	ifbFeatures				{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES, DE_NULL, DE_TRUE };
		VkPhysicalDeviceFeatures2						createPhysicalFeature	{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &fsrFeatures, deviceFeatures };

		std::vector<const char*> enabledExtensions = { "VK_KHR_fragment_shading_rate" };
		if (m_params->useImagelessFramebuffer)
		{
			enabledExtensions.push_back("VK_KHR_imageless_framebuffer");
			fsrFeatures.pNext = &ifbFeatures;
		}

		std::vector<const char*> enabledLayers = getValidationLayers(vki, pd);
		VkDeviceCreateInfo deviceInfo
		{
			VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,				// VkStructureType					sType;
			&createPhysicalFeature,								// const void*						pNext;
			(VkDeviceCreateFlags)0u,							// VkDeviceCreateFlags				flags;
			2u,													// uint32_t							queueCreateInfoCount;
			queueInfo.data(),									// const VkDeviceQueueCreateInfo*	pQueueCreateInfos;
			static_cast<deUint32>(enabledLayers.size()),		// uint32_t							enabledLayerCount;
			de::dataOrNull(enabledLayers),						// const char* const*				ppEnabledLayerNames;
			static_cast<deUint32>(enabledExtensions.size()),	// uint32_t							enabledExtensionCount;
			enabledExtensions.data(),							// const char* const*				ppEnabledExtensionNames;
			DE_NULL												// const VkPhysicalDeviceFeatures*	pEnabledFeatures;
		};

		m_device		= createDevice(vkp, m_context.getInstance(), vki, pd, &deviceInfo);
		m_allocator		= de::MovePtr<Allocator>(new SimpleAllocator(m_context.getDeviceInterface(), *m_device, getPhysicalDeviceMemoryProperties(vki, pd)));
	}

	VkQueue transferQueue;
	vk.getDeviceQueue(*m_device, transferQueueFamilyIndex, 0u, &transferQueue);
	VkQueue graphicsQueue;
	vk.getDeviceQueue(*m_device, graphicsQueueFamilyIndex, 0u, &graphicsQueue);

	// create transfer and graphics command buffers
	Move<VkCommandPool>		transferCmdPool		= createCommandPool(vk, *m_device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, transferQueueFamilyIndex);
	Move<VkCommandBuffer>	transferCmdBuffer	= allocateCommandBuffer(vk, *m_device, *transferCmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	Move<VkCommandPool>		graphicsCmdPool		= createCommandPool(vk, *m_device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, graphicsQueueFamilyIndex);
	Move<VkCommandBuffer>	graphicsCmdBuffer	= allocateCommandBuffer(vk, *m_device, *graphicsCmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	Move<VkShaderModule>	vertShader			= createShaderModule(vk, *m_device, m_context.getBinaryCollection().get("vert"), 0);
	Move<VkShaderModule>	fragShader			= createShaderModule(vk, *m_device, m_context.getBinaryCollection().get("frag"), 0);

	buildColorBufferObjects(0, m_cbUsage);
	buildCounterBufferObjects();

	// iterate over all possible tile sizes
	for (deUint32 tileWidth  = m_minTileSize.width;  tileWidth  <= m_maxTileSize.width;  tileWidth  *= 2)
	for (deUint32 tileHeight = m_minTileSize.height; tileHeight <= m_maxTileSize.height; tileHeight *= 2)
	{
		// skip tile sizes that have unsuported aspect ratio
		deUint32 aspectRatio = (tileHeight > tileWidth) ? (tileHeight / tileWidth) : (tileWidth / tileHeight);
		if (aspectRatio > m_maxAspectRatio)
			continue;

		// calculate size of shading rate attachment
		deUint32 srWidth  = (m_cbWidth  + tileWidth  - 1) / tileWidth;
		deUint32 srHeight = (m_cbHeight + tileHeight - 1) / tileHeight;

		// create image that will be source for shading rate image
		de::MovePtr<ImageWithMemory> srSrcImage = buildImageWithMemory(m_params->srFormat, srWidth, srHeight,
																	   VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

		// create buffer that will contain shading rate source data
		tcu::TextureFormat				srTextureFormat		= mapVkFormat(m_params->srFormat);
		deUint32						srWriteBufferSize	= srWidth * srHeight * getNumUsedChannels(srTextureFormat.order) * getChannelSize(srTextureFormat.type);
		de::MovePtr<BufferWithMemory>	srSrcBuffer			= buildBufferWithMemory(srWriteBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

		// fill buffer with tested shading rate
		deUint8*	srWriteBufferHostPtr	= (deUint8*)srSrcBuffer->getAllocation().getHostPtr();
		deUint8		value					= (deUint8)calculateRate(m_params->srRate.width, m_params->srRate.height);
		deMemset(srWriteBufferHostPtr, value, (size_t)srWriteBufferSize);
		flushAlloc(vk, *m_device, srSrcBuffer->getAllocation());

		// create shading rate iamge
		m_srImage[0]		= buildImageWithMemory(m_params->srFormat, srWidth, srHeight,
												   VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
												   VK_IMAGE_TILING_OPTIMAL, queueFamilies);
		m_srImageView[0]	= buildImageView(m_params->srFormat, m_srImage[0]->get());

		Move<VkPipelineLayout>	graphicsPipelineLayout	= buildPipelineLayout(&(*m_counterBufferDescriptorSetLayout));
		Move<VkRenderPass>		renderPass				= buildRenderPass(m_cbFormat, tileWidth, tileHeight);
		Move<VkPipeline>		graphicsPipeline		= buildGraphicsPipeline(0, *renderPass, m_cbFormat, *graphicsPipelineLayout, *vertShader, *fragShader);

		std::vector<FBAttachmentInfo> attachmentInfo
		{
			{ m_cbFormat,			m_cbUsage, m_cbWidth, m_cbHeight, *m_cbImageView[0] },
			{ m_params->srFormat,	m_srUsage, srWidth,   srHeight,   *m_srImageView[0] }
		};
		Move<VkFramebuffer> framebuffer = buildFramebuffer(*renderPass, attachmentInfo);

		beginCommandBuffer(vk, *transferCmdBuffer, 0u);

		// wait till sr data is ready in buffer and change sr image layouts to general
		VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
		VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
		memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		memoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		std::vector<VkImageMemoryBarrier> srImageBarrierGeneral(2,
			makeImageMemoryBarrier(
				VK_ACCESS_NONE_KHR,
				VK_ACCESS_NONE_KHR,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_GENERAL,
				**m_srImage[0],
				m_defaultImageSubresourceRange));
		srImageBarrierGeneral[1].image = **srSrcImage;
		vk.cmdPipelineBarrier(*transferCmdBuffer, srcStageMask, dstStageMask, 0, 1, &memoryBarrier, 0, DE_NULL, 2, srImageBarrierGeneral.data());

		// copy sr data to images
		const VkBufferImageCopy srCopyBuffer = makeBufferImageCopy({ srWidth, srHeight, 1u }, m_defaultImageSubresourceLayers);
		vk.cmdCopyBufferToImage(*transferCmdBuffer, **srSrcBuffer, **srSrcImage, VK_IMAGE_LAYOUT_GENERAL, 1, &srCopyBuffer);

		// wait till sr source image is ready
		srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
		memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		memoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		vk.cmdPipelineBarrier(*transferCmdBuffer, srcStageMask, dstStageMask, 0, 1, &memoryBarrier, 0, DE_NULL, 0, DE_NULL);

		// copy sr source image to sr image used during rendering
		VkImageCopy imageCopyRegion
		{
			m_defaultImageSubresourceLayers,	// VkImageSubresourceLayers	srcSubresource;
			{0, 0, 0},							// VkOffset3D				srcOffset;
			m_defaultImageSubresourceLayers,	// VkImageSubresourceLayers	dstSubresource;
			{0, 0, 0},							// VkOffset3D				dstOffset;
			{ srWidth, srHeight, 1u }			// VkExtent3D				extent;
		};
		vk.cmdCopyImage(*transferCmdBuffer, **srSrcImage, VK_IMAGE_LAYOUT_GENERAL, **m_srImage[0], VK_IMAGE_LAYOUT_GENERAL, 1, &imageCopyRegion);

		// release exclusive ownership from the transfer queue family
		srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		VkImageMemoryBarrier srImageBarrierOwnershipTransfer =
			makeImageMemoryBarrier(
				VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_ACCESS_NONE_KHR,
				VK_IMAGE_LAYOUT_GENERAL,
				VK_IMAGE_LAYOUT_GENERAL,
				**m_srImage[0], m_defaultImageSubresourceRange);
		if (m_params->mode == TM_SETUP_RATE_WITH_COPYING_FROM_EXCLUSIVE_IMAGE_USING_TRANSFER_QUEUE)
		{
			srImageBarrierOwnershipTransfer.srcQueueFamilyIndex = transferQueueFamilyIndex;
			srImageBarrierOwnershipTransfer.dstQueueFamilyIndex = graphicsQueueFamilyIndex;
		}
		vk.cmdPipelineBarrier(*transferCmdBuffer, srcStageMask, dstStageMask, 0, 0, DE_NULL, 0, DE_NULL, 1, &srImageBarrierOwnershipTransfer);

		endCommandBuffer(vk, *transferCmdBuffer);

		beginCommandBuffer(vk, *graphicsCmdBuffer, 0u);

		// acquire exclusive ownership for the graphics queue family - while changing sr images layout
		vk.cmdPipelineBarrier(*graphicsCmdBuffer, srcStageMask, dstStageMask, 0, 0, DE_NULL, 0, DE_NULL, 1, &srImageBarrierOwnershipTransfer);

		// wait till sr image layout is changed
		srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;
		VkImageMemoryBarrier srImageBarrierShadingRate =
			makeImageMemoryBarrier(
				VK_ACCESS_NONE_KHR,
				VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR,
				VK_IMAGE_LAYOUT_GENERAL,
				VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR,
				**m_srImage[0],
				m_defaultImageSubresourceRange);
		vk.cmdPipelineBarrier(*graphicsCmdBuffer, srcStageMask, dstStageMask, 0, 0, DE_NULL, 0, DE_NULL, 1, &srImageBarrierShadingRate);

		// wait till cb image layout is changed
		srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkImageMemoryBarrier cbImageBarrier =
			makeImageMemoryBarrier(
				VK_ACCESS_NONE_KHR,
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_GENERAL,
				**m_cbImage[0],
				m_defaultImageSubresourceRange);
		vk.cmdPipelineBarrier(*graphicsCmdBuffer, srcStageMask, dstStageMask, 0, 0, DE_NULL, 0, DE_NULL, 1, &cbImageBarrier);

		startRendering(*graphicsCmdBuffer, *renderPass, *framebuffer, makeRect2D(m_cbWidth, m_cbHeight), attachmentInfo, tileWidth, tileHeight);

		// draw single triangle to cb
		vk.cmdBindDescriptorSets(*graphicsCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipelineLayout, 0, 1, &(*m_counterBufferDescriptorSet), 0, DE_NULL);
		vk.cmdBindPipeline(*graphicsCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);
		vk.cmdDraw(*graphicsCmdBuffer, 3u, 1, 0u, 0u);

		finishRendering(*graphicsCmdBuffer);

		// wait till color attachment is fully written
		srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
		memoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		memoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		vk.cmdPipelineBarrier(*graphicsCmdBuffer, srcStageMask, dstStageMask, 0, 1, &memoryBarrier, 0, DE_NULL, 0, DE_NULL);

		// read back color buffer image
		vk.cmdCopyImageToBuffer(*graphicsCmdBuffer, **m_cbImage[0], VK_IMAGE_LAYOUT_GENERAL, **m_cbReadBuffer[0], 1u, &m_defaultBufferImageCopy);

		endCommandBuffer(vk, *graphicsCmdBuffer);

		// create synchronization objects
		Move<VkSemaphore>	semaphore		= createSemaphore(vk, *m_device);
		Move<VkFence>		transferFence	= createFence(vk, *m_device);
		Move<VkFence>		graphicsFence	= createFence(vk, *m_device);

		const VkSubmitInfo transferSubmitInfo
		{
			VK_STRUCTURE_TYPE_SUBMIT_INFO,						// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			0u,													// deUint32						waitSemaphoreCount;
			DE_NULL,											// const VkSemaphore*			pWaitSemaphores;
			DE_NULL,											// const VkPipelineStageFlags*	pWaitDstStageMask;
			1u,													// deUint32						commandBufferCount;
			&*transferCmdBuffer,								// const VkCommandBuffer*		pCommandBuffers;
			1u,													// deUint32						signalSemaphoreCount;
			&*semaphore,										// const VkSemaphore*			pSignalSemaphores;
		};
		const VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		const VkSubmitInfo graphicsSubmitInfo
		{
			VK_STRUCTURE_TYPE_SUBMIT_INFO,						// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			1u,													// deUint32						waitSemaphoreCount;
			&*semaphore,										// const VkSemaphore*			pWaitSemaphores;
			&waitDstStageMask,									// const VkPipelineStageFlags*	pWaitDstStageMask;
			1u,													// deUint32						commandBufferCount;
			&*graphicsCmdBuffer,								// const VkCommandBuffer*		pCommandBuffers;
			0u,													// deUint32						signalSemaphoreCount;
			DE_NULL,											// const VkSemaphore*			pSignalSemaphores;
		};

		// submit commands to both queues
		VK_CHECK(vk.queueSubmit(transferQueue, 1u, &transferSubmitInfo, *transferFence));
		VK_CHECK(vk.queueSubmit(graphicsQueue, 1u, &graphicsSubmitInfo, *graphicsFence));

		VkFence fences[] = { *graphicsFence, *transferFence };
		VK_CHECK(vk.waitForFences(*m_device, 2u, fences, DE_TRUE, ~0ull));

		invalidateAlloc(vk, *m_device, m_cbReadBuffer[0]->getAllocation());
		if (!verifyUsingAtomicChecks(tileWidth, tileHeight,
									 m_params->srRate.width, m_params->srRate.height,
									 (deUint32*)m_cbReadBuffer[0]->getAllocation().getHostPtr()))
			return false;

	} // iterate over all possible tile sizes

	return true;
}

bool AttachmentRateInstance::runFillLinearTiledImage(void)
{
	// Create a linear tiled fragment shading rate attachment image and set
	// its data on the host, then draw a basic triangle and do basic checks

	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	VkDevice				device				= m_context.getDevice();
	deUint32				queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	VkImageSubresource		imageSubresource	= makeImageSubresource(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u);
	VkMemoryBarrier			memoryBarrier		{ VK_STRUCTURE_TYPE_MEMORY_BARRIER, DE_NULL, 0u, 0u };
	VkSubresourceLayout		srImageLayout;

	Move<VkShaderModule>	vertShader			= createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0);
	Move<VkShaderModule>	fragShader			= createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0);

	Move<VkCommandPool>		cmdPool				= createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
	Move<VkCommandBuffer>	cmdBuffer			= allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	buildCounterBufferObjects();
	buildColorBufferObjects(0, m_cbUsage);

	// iterate over all possible tile sizes
	for (deUint32 tileWidth  = m_minTileSize.width;  tileWidth  <= m_maxTileSize.width;  tileWidth  *= 2)
	for (deUint32 tileHeight = m_minTileSize.height; tileHeight <= m_maxTileSize.height; tileHeight *= 2)
	{
		// skip tile sizes that have unsuported aspect ratio
		deUint32 aspectRatio = (tileHeight > tileWidth) ? (tileHeight / tileWidth) : (tileWidth / tileHeight);
		if (aspectRatio > m_maxAspectRatio)
			continue;

		// calculate size of shading rate attachment
		deUint32 srWidth  = (m_cbWidth  + tileWidth  - 1) / tileWidth;
		deUint32 srHeight = (m_cbHeight + tileHeight - 1) / tileHeight;

		buildShadingRateObjects(0, srWidth, srHeight, m_srUsage, VK_IMAGE_TILING_LINEAR);

		deUint8*	imagePtr	= reinterpret_cast<deUint8*>(m_srImage[0]->getAllocation().getHostPtr());
		deUint8		value		= (deUint8)calculateRate(m_params->srRate.width, m_params->srRate.height);

		// fill sr image on the host row by row
		vk.getImageSubresourceLayout(device, **m_srImage[0], &imageSubresource, &srImageLayout);
		for (deUint32 srTexelRow = 0; srTexelRow < srHeight; srTexelRow++)
		{
			deUint8* rowDst = imagePtr + srImageLayout.offset + srImageLayout.rowPitch * srTexelRow;
			deMemset(rowDst, value, (size_t)srWidth);
		}

		Move<VkPipelineLayout>	graphicsPipelineLayout	= buildPipelineLayout(&(*m_counterBufferDescriptorSetLayout));
		Move<VkRenderPass>		renderPass				= buildRenderPass(m_cbFormat, tileWidth, tileHeight);
		Move<VkPipeline>		graphicsPipeline		= buildGraphicsPipeline(0, *renderPass, m_cbFormat, *graphicsPipelineLayout, *vertShader, *fragShader);

		std::vector<FBAttachmentInfo> attachmentInfo
		{
			{ m_cbFormat,			m_cbUsage, m_cbWidth, m_cbHeight, *m_cbImageView[0] },
			{ m_params->srFormat,	m_srUsage, srWidth,   srHeight,   *m_srImageView[0] }
		};
		Move<VkFramebuffer> framebuffer = buildFramebuffer(*renderPass, attachmentInfo);

		beginCommandBuffer(vk, *cmdBuffer, 0u);

		// wait till sr image layout is changed
		VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;
		VkImageMemoryBarrier srImageBarrierAttachment =
			makeImageMemoryBarrier(
				VK_ACCESS_NONE_KHR,
				VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR,
				**m_srImage[0],
				m_defaultImageSubresourceRange);
		vk.cmdPipelineBarrier(*cmdBuffer, srcStageMask, dstStageMask, 0, 0, DE_NULL, 0, DE_NULL, 1, &srImageBarrierAttachment);

		// wait till cb image layout is changed
		srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkImageMemoryBarrier cbImageBarrier =
			makeImageMemoryBarrier(
				VK_ACCESS_NONE_KHR,
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_GENERAL,
				**m_cbImage[0],
				m_defaultImageSubresourceRange);
		vk.cmdPipelineBarrier(*cmdBuffer, srcStageMask, dstStageMask, 0, 0, DE_NULL, 0, DE_NULL, 1, &cbImageBarrier);

		startRendering(*cmdBuffer, *renderPass, *framebuffer, makeRect2D(m_cbWidth, m_cbHeight), attachmentInfo, tileWidth, tileHeight);

		// draw single triangle to cb
		vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipelineLayout, 0, 1, &(*m_counterBufferDescriptorSet), 0, DE_NULL);
		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);
		vk.cmdDraw(*cmdBuffer, 3u, 1, 0u, 0u);

		finishRendering(*cmdBuffer);

		// wait till color attachment is fully written
		srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
		memoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		memoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		vk.cmdPipelineBarrier(*cmdBuffer, srcStageMask, dstStageMask, 0, 1, &memoryBarrier, 0, DE_NULL, 0, DE_NULL);

		// read back color buffer image
		vk.cmdCopyImageToBuffer(*cmdBuffer, **m_cbImage[0], VK_IMAGE_LAYOUT_GENERAL, **m_cbReadBuffer[0], 1u, &m_defaultBufferImageCopy);

		endCommandBuffer(vk, *cmdBuffer);

		// submit commands and wait
		const VkQueue queue = m_context.getUniversalQueue();
		submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

		invalidateAlloc(vk, device, m_cbReadBuffer[0]->getAllocation());
		if (!verifyUsingAtomicChecks(tileWidth, tileHeight,
									 m_params->srRate.width, m_params->srRate.height,
									 (deUint32*)m_cbReadBuffer[0]->getAllocation().getHostPtr()))
			return false;

	} // iterate over all possible tile sizes

	return true;
}

bool AttachmentRateInstance::runTwoSubpassMode(void)
{
	// Set up a two-subpass render pass with different shading rate attachments used in each subpass.
	// Then draw a basic triangle in each subpass and do basic checks.

	const InstanceInterface&	vki					= m_context.getInstanceInterface();
	const DeviceInterface&		vk					= m_context.getDeviceInterface();
	VkPhysicalDevice			pd					= m_context.getPhysicalDevice();
	VkDevice					device				= m_context.getDevice();
	deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	VkMemoryBarrier				memoryBarrier		{ VK_STRUCTURE_TYPE_MEMORY_BARRIER, DE_NULL, 0u, 0u };

	Move<VkShaderModule>		vertShader0			= createShaderModule(vk, device, m_context.getBinaryCollection().get("vert0"), 0);
	Move<VkShaderModule>		vertShader1			= createShaderModule(vk, device, m_context.getBinaryCollection().get("vert1"), 0);
	Move<VkShaderModule>		fragShader			= createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0);

	Move<VkCommandPool>			cmdPool				= createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
	Move<VkCommandBuffer>		cmdBuffer			= allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	// fetch information about supported rates
	deUint32 supportedFragmentShadingRateCount;
	std::vector<VkPhysicalDeviceFragmentShadingRateKHR> supportedFragmentShadingRates;
	vki.getPhysicalDeviceFragmentShadingRatesKHR(pd, &supportedFragmentShadingRateCount, DE_NULL);
	supportedFragmentShadingRates.resize(supportedFragmentShadingRateCount, {
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_KHR,				// VkStructureType		sType;
		DE_NULL,																	// void*				pNext;
		VK_SAMPLE_COUNT_1_BIT,														// VkSampleCountFlags	sampleCounts;
		{ 0, 0 }																	// VkExtent2D			fragmentSize;
		});
	vki.getPhysicalDeviceFragmentShadingRatesKHR(pd, &supportedFragmentShadingRateCount, &supportedFragmentShadingRates[0]);

	// grab min and max tile sieze and biggest and smallest rate
	deUint32 sr0Width		= (m_cbWidth + m_minTileSize.width - 1) / m_minTileSize.width;
	deUint32 sr0Height		= (m_cbHeight + m_minTileSize.height - 1) / m_minTileSize.height;
	deUint32 sr1Width		= (m_cbWidth + m_maxTileSize.width - 1) / m_maxTileSize.width;
	deUint32 sr1Height		= (m_cbHeight + m_maxTileSize.height - 1) / m_maxTileSize.height;
	deUint32 sr0RateWidth	= supportedFragmentShadingRates[0].fragmentSize.width;										// bigets supported rate
	deUint32 sr0RateHeight	= supportedFragmentShadingRates[0].fragmentSize.height;
	deUint32 sr1RateWidth	= supportedFragmentShadingRates[supportedFragmentShadingRateCount - 2].fragmentSize.width;	// smallest supported rate excluding {1, 1}
	deUint32 sr1RateHeight	= supportedFragmentShadingRates[supportedFragmentShadingRateCount - 2].fragmentSize.height;

	buildColorBufferObjects(0, m_cbUsage);
	buildColorBufferObjects(1, m_cbUsage);
	buildShadingRateObjects(0, sr0Width, sr0Height, m_srUsage);
	buildShadingRateObjects(1, sr1Width, sr1Height, m_srUsage);
	buildCounterBufferObjects();

	Move<VkRenderPass>		renderPass			= buildRenderPass(m_cbFormat, m_minTileSize.width, m_minTileSize.height, m_maxTileSize.width, m_maxTileSize.height);
	Move<VkPipelineLayout>	pipelineLayout		= buildPipelineLayout(&(*m_counterBufferDescriptorSetLayout));
	Move<VkPipeline>		graphicsPipeline0	= buildGraphicsPipeline(0, *renderPass, m_cbFormat, *pipelineLayout, *vertShader0, *fragShader);
	Move<VkPipeline>		graphicsPipeline1	= buildGraphicsPipeline(1, *renderPass, m_cbFormat, *pipelineLayout, *vertShader1, *fragShader);

	std::vector<FBAttachmentInfo> attachmentInfo
	{
		{ m_cbFormat,			m_cbUsage, m_cbWidth, m_cbHeight, *m_cbImageView[0] },
		{ m_params->srFormat,	m_srUsage, sr0Width,  sr0Height,  *m_srImageView[0] },
		{ m_cbFormat,			m_cbUsage, m_cbWidth, m_cbHeight, *m_cbImageView[1] },
		{ m_params->srFormat,	m_srUsage, sr1Width,  sr1Height,  *m_srImageView[1] }
	};
	Move<VkFramebuffer> framebuffer = buildFramebuffer(*renderPass, attachmentInfo);

	beginCommandBuffer(vk, *cmdBuffer, 0u);

	// change sr image layouts to general
	VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
	VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
	std::vector<VkImageMemoryBarrier> srImageBarrierGeneral(2,
		makeImageMemoryBarrier(
			VK_ACCESS_NONE_KHR,
			VK_ACCESS_NONE_KHR,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_GENERAL,
			**m_srImage[0],
			m_defaultImageSubresourceRange));
	srImageBarrierGeneral[1].image = **m_srImage[1];
	vk.cmdPipelineBarrier(*cmdBuffer, srcStageMask, dstStageMask, 0, 0, DE_NULL, 0, DE_NULL, 2, srImageBarrierGeneral.data());

	VkClearColorValue clearValues[2] = { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } };
	clearValues[0].uint32[0] = calculateRate(sr0RateWidth, sr0RateHeight);
	clearValues[1].uint32[0] = calculateRate(sr1RateWidth, sr1RateHeight);
	vk.cmdClearColorImage(*cmdBuffer, **m_srImage[0], VK_IMAGE_LAYOUT_GENERAL, &clearValues[0], 1, &m_defaultImageSubresourceRange);
	vk.cmdClearColorImage(*cmdBuffer, **m_srImage[1], VK_IMAGE_LAYOUT_GENERAL, &clearValues[1], 1, &m_defaultImageSubresourceRange);

	// wait till sr data is ready and change sr images layout
	srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;
	memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	memoryBarrier.dstAccessMask = VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR;
	std::vector<VkImageMemoryBarrier> srImageBarrierShadingRate(2,
		makeImageMemoryBarrier(
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR,
			VK_IMAGE_LAYOUT_GENERAL,
			VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR,
			**m_srImage[0],
			m_defaultImageSubresourceRange));
	srImageBarrierShadingRate[1].image = **m_srImage[1];
	vk.cmdPipelineBarrier(*cmdBuffer, srcStageMask, dstStageMask, 0, 1, &memoryBarrier, 0, DE_NULL, 2, srImageBarrierShadingRate.data());

	// wait till cb image layouts are changed
	srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	std::vector<VkImageMemoryBarrier> cbImageBarrier(2,
		makeImageMemoryBarrier(
			VK_ACCESS_NONE_KHR,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_GENERAL,
			**m_cbImage[0],
			m_defaultImageSubresourceRange));
	cbImageBarrier[1].image = **m_cbImage[1];
	vk.cmdPipelineBarrier(*cmdBuffer, srcStageMask, dstStageMask, 0, 0, DE_NULL, 0, DE_NULL, 2, cbImageBarrier.data());

	startRendering(*cmdBuffer, *renderPass, *framebuffer, makeRect2D(m_cbWidth, m_cbHeight), attachmentInfo);

	// draw single triangle to first cb
	vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0, 1, &(*m_counterBufferDescriptorSet), 0, DE_NULL);
	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline0);
	vk.cmdDraw(*cmdBuffer, 3u, 1, 0u, 0u);

	vk.cmdNextSubpass(*cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);

	// draw single triangle to second cb
	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline1);
	vk.cmdDraw(*cmdBuffer, 3u, 1, 0u, 0u);

	finishRendering(*cmdBuffer);

	// wait till color attachments are fully written
	srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
	memoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	memoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	vk.cmdPipelineBarrier(*cmdBuffer, srcStageMask, dstStageMask, 0, 1, &memoryBarrier, 0, DE_NULL, 0, DE_NULL);

	// read back color buffer images
	vk.cmdCopyImageToBuffer(*cmdBuffer, **m_cbImage[0], VK_IMAGE_LAYOUT_GENERAL, **m_cbReadBuffer[0], 1u, &m_defaultBufferImageCopy);
	vk.cmdCopyImageToBuffer(*cmdBuffer, **m_cbImage[1], VK_IMAGE_LAYOUT_GENERAL, **m_cbReadBuffer[1], 1u, &m_defaultBufferImageCopy);

	endCommandBuffer(vk, *cmdBuffer);

	// submit commands and wait
	const VkQueue queue = m_context.getUniversalQueue();
	submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

	// read back buffer with color attachment 1 data
	Allocation& cb0BuffAlloc = m_cbReadBuffer[0]->getAllocation();
	invalidateAlloc(vk, device, cb0BuffAlloc);

	// read back buffer with color attachment 2 data
	Allocation& cb1BuffAlloc = m_cbReadBuffer[1]->getAllocation();
	invalidateAlloc(vk, device, cb1BuffAlloc);

	// validate both attachemtns triangle
	return (verifyUsingAtomicChecks(m_minTileSize.width, m_minTileSize.height,
									sr0RateWidth, sr0RateHeight,
									(deUint32*)m_cbReadBuffer[0]->getAllocation().getHostPtr()) &&
			verifyUsingAtomicChecks(m_maxTileSize.width, m_maxTileSize.height,
									sr1RateWidth, sr1RateHeight,
									(deUint32*)m_cbReadBuffer[1]->getAllocation().getHostPtr()));
}

class AttachmentRateTestCase : public TestCase
{
public:
					AttachmentRateTestCase	(tcu::TestContext& context, const char* name, de::SharedPtr<TestParams> params);
					~AttachmentRateTestCase	(void) = default;

	void			initPrograms			(SourceCollections& programCollection) const override;
	TestInstance*	createInstance			(Context& context) const override;
	void			checkSupport			(Context& context) const override;

private:

	const de::SharedPtr<TestParams> m_params;
};

AttachmentRateTestCase::AttachmentRateTestCase(tcu::TestContext& context, const char* name, de::SharedPtr<TestParams> params)
	: vkt::TestCase	(context, name, "")
	, m_params		(params)
{
}

void AttachmentRateTestCase::checkSupport(Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_fragment_shading_rate");

	if (m_params->useDynamicRendering)
		context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");
	if (m_params->useImagelessFramebuffer)
		context.requireDeviceFunctionality("VK_KHR_imageless_framebuffer");

	if (!context.getFragmentShadingRateFeatures().attachmentFragmentShadingRate)
		TCU_THROW(NotSupportedError, "pipelineFragmentShadingRate not supported");

	const vk::InstanceInterface&	vk					= context.getInstanceInterface();
	const vk::VkPhysicalDevice		pd					= context.getPhysicalDevice();

	VkImageFormatProperties			imageProperties;
	VkImageUsageFlags				srUsage				= VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR |
														  VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	VkResult result = vk.getPhysicalDeviceImageFormatProperties(pd, m_params->srFormat, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, srUsage, 0, &imageProperties);
	if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
		TCU_THROW(NotSupportedError, "Format not supported");

	if (m_params->mode != TM_TWO_SUBPASS)
	{
		deUint32												supportedFragmentShadingRateCount;
		VkExtent2D												testedRate = m_params->srRate;
		std::vector<VkPhysicalDeviceFragmentShadingRateKHR>		supportedFragmentShadingRates;

		// fetch information about supported rates
		vk.getPhysicalDeviceFragmentShadingRatesKHR(pd, &supportedFragmentShadingRateCount, DE_NULL);
		supportedFragmentShadingRates.resize(supportedFragmentShadingRateCount, {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_KHR,				// VkStructureType		sType;
			DE_NULL,																	// void*				pNext;
			VK_SAMPLE_COUNT_1_BIT,														// VkSampleCountFlags	sampleCounts;
			{ 0, 0 }																	// VkExtent2D			fragmentSize;
			});
		vk.getPhysicalDeviceFragmentShadingRatesKHR(pd, &supportedFragmentShadingRateCount, &supportedFragmentShadingRates[0]);

		// check if rate required by test is not supported
		if (std::none_of(supportedFragmentShadingRates.begin(), supportedFragmentShadingRates.end(),
			[&testedRate](const VkPhysicalDeviceFragmentShadingRateKHR& r)
			{ return (r.fragmentSize.width == testedRate.width && r.fragmentSize.height == testedRate.height); }))
		{
			TCU_THROW(NotSupportedError, "Rate not supported");
		}
	}

	VkFormatFeatureFlags requiredFeatures = 0;
	if (m_params->mode == TM_SETUP_RATE_WITH_ATOMICS_IN_COMPUTE_SHADER)
		requiredFeatures = VK_FORMAT_FEATURE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR | VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT;
	else if ((m_params->mode == TM_SETUP_RATE_WITH_COPYING_FROM_OTHER_IMAGE) ||
			 (m_params->mode == TM_SETUP_RATE_WITH_COPYING_FROM_EXCLUSIVE_IMAGE_USING_TRANSFER_QUEUE) ||
			 (m_params->mode == TM_SETUP_RATE_WITH_COPYING_FROM_CONCURENT_IMAGE_USING_TRANSFER_QUEUE) ||
			 (m_params->mode == TM_SETUP_RATE_WITH_LINEAR_TILED_IMAGE))
		requiredFeatures = VK_FORMAT_FEATURE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;
	else if (m_params->mode == TM_SETUP_RATE_WITH_FRAGMENT_SHADER)
		requiredFeatures = VK_FORMAT_FEATURE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;

#if DEBUG_USE_STORE_INSTEAD_OF_ATOMICS == 1
	if (m_params->mode == TM_SETUP_RATE_WITH_ATOMICS_IN_COMPUTE_SHADER)
		requiredFeatures = VK_FORMAT_FEATURE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR | VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
#endif

	if (requiredFeatures)
	{
		const VkFormatProperties formatProperties = getPhysicalDeviceFormatProperties(vk, pd, m_params->srFormat);

		if (m_params->mode == TM_SETUP_RATE_WITH_LINEAR_TILED_IMAGE)
		{
			if ((formatProperties.linearTilingFeatures & requiredFeatures) != requiredFeatures)
				TCU_THROW(NotSupportedError, "Required format feature bits not supported");
		}
		else if ((formatProperties.optimalTilingFeatures & requiredFeatures) != requiredFeatures)
			TCU_THROW(NotSupportedError, "Required format feature bits not supported");
	}
}

void AttachmentRateTestCase::initPrograms(SourceCollections& programCollection) const
{
	deUint32 rateValue = calculateRate(m_params->srRate.width, m_params->srRate.height);

	if (m_params->mode == TM_SETUP_RATE_WITH_ATOMICS_IN_COMPUTE_SHADER)
	{
		std::stringstream compStream;
		compStream <<
			"#version 450\n"
			"layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
			"layout(r32ui, binding = 0) coherent uniform highp uimage2D srImage;\n"
			"void main (void)\n"
			"{\n"
#if DEBUG_USE_STORE_INSTEAD_OF_ATOMICS == 1
			"  imageStore(srImage, ivec2(gl_GlobalInvocationID.xy), uvec4(" << rateValue << "));\n"
#else
			"  imageAtomicAdd(srImage, ivec2(gl_GlobalInvocationID.xy), " << rateValue << ");\n"
#endif
			"}\n";

		programCollection.glslSources.add("comp") << glu::ComputeSource(compStream.str());
	}

	tcu::StringTemplate vertTemplate(
		"#version 450 core\n"
		"out gl_PerVertex\n"
		"{\n"
		"  vec4 gl_Position;\n"
		"};\n"
		"void main()\n"
		"{\n"
		"  gl_Position = vec4(float(1.0 - 2.0 * int(gl_VertexIndex != 1)) * ${SCALE} + ${TRANSLATE},\n"
		"                     float(1.0 - 2.0 * int(gl_VertexIndex > 0))  * ${SCALE} + ${TRANSLATE}, 0.0, 1.0);\n"
		"}\n");

	std::map<std::string, std::string> specializationMap
	{
		{"SCALE",		"0.8" },
		{"TRANSLATE",	"0.0" },
	};

	if (m_params->mode == TM_TWO_SUBPASS)
	{
		specializationMap["SCALE"]		=  "0.4";
		specializationMap["TRANSLATE"]	= "-0.5";
		programCollection.glslSources.add("vert0") << glu::VertexSource(vertTemplate.specialize(specializationMap));

		specializationMap["SCALE"]		= "0.4";
		specializationMap["TRANSLATE"]	= "0.5";
		programCollection.glslSources.add("vert1") << glu::VertexSource(vertTemplate.specialize(specializationMap));
	}
	else
	{
		programCollection.glslSources.add("vert") << glu::VertexSource(vertTemplate.specialize(specializationMap));
	}

	if (m_params->mode == TM_SETUP_RATE_WITH_FRAGMENT_SHADER)
	{
		// use large triangle that will cover whole color buffer
		specializationMap["SCALE"]		= "9.0";
		specializationMap["TRANSLATE"]	= "0.0";
		programCollection.glslSources.add("vert_setup") << glu::VertexSource(vertTemplate.specialize(specializationMap));

		std::stringstream fragStream;
		fragStream <<
			"#version 450 core\n"
			"layout(location = 0) out uint outColor;\n"
			"void main()\n"
			"{\n"
			"  outColor.x = " << rateValue << ";\n"
			"}\n";
		programCollection.glslSources.add("frag_setup") << glu::FragmentSource(fragStream.str());
	}

	std::string frag =
		"#version 450 core\n"
		"#extension GL_EXT_fragment_shading_rate : enable\n"
		"layout(set = 0, binding = 0) buffer Block { uint counter; } buf;\n"
		"layout(location = 0) out uvec4 outColor;\n"
		"void main()\n"
		"{\n"
		"  outColor.x = gl_ShadingRateEXT;\n"
		"  outColor.y = 0;\n"
		"  outColor.z = atomicAdd(buf.counter, 1);\n"
		"  outColor.w = 0;\n"
		"}\n";
	programCollection.glslSources.add("frag") << glu::FragmentSource(frag);
}

TestInstance* AttachmentRateTestCase::createInstance(Context& context) const
{
	return new AttachmentRateInstance(context, m_params);
}

}	// anonymous

void createAttachmentRateTests(tcu::TestContext& testCtx, tcu::TestCaseGroup* parentGroup, bool useDynamicRendering)
{
	struct SRFormat
	{
		VkFormat		format;
		const char*		name;
	};

	const std::vector<SRFormat> srFormats
	{
		{ VK_FORMAT_R8_UINT,				"r8_uint" },
		{ VK_FORMAT_R8G8_UINT,				"r8g8_uint" },
		{ VK_FORMAT_R8G8B8_UINT,			"r8g8b8_uint" },
		{ VK_FORMAT_R8G8B8A8_UINT,			"r8g8b8a8_uint" },
		{ VK_FORMAT_R16_UINT,				"r16_uint" },
		{ VK_FORMAT_R16G16_UINT,			"r16g16_uint" },
		{ VK_FORMAT_R16G16B16_UINT,			"r16g16b16_uint" },
		{ VK_FORMAT_R16G16B16A16_UINT,		"r16g16b16a16_uint" },
		{ VK_FORMAT_R32_UINT,				"r32_uint" },
		{ VK_FORMAT_R32G32_UINT,			"r32g32_uint" },
		{ VK_FORMAT_R32G32B32_UINT,			"r32g32b32_uint" },
		{ VK_FORMAT_R32G32B32A32_UINT,		"r32g32b32a32_uint" },
		{ VK_FORMAT_R64_UINT,				"r64_uint" },
		{ VK_FORMAT_R64G64_UINT,			"r64g64_uint" },
		{ VK_FORMAT_R64G64B64_UINT,			"r64g64b64_uint" },
		{ VK_FORMAT_R64G64B64A64_UINT,		"r64g64b64a64_uint" },
	};

	struct SRRate
	{
		VkExtent2D		count;
		const char*		name;
	};

	const std::vector<SRRate> srRates
	{
		{ {1, 1},	"rate_1x1" },
		{ {1, 2},	"rate_1x2" },
		{ {1, 4},	"rate_1x4" },
		{ {2, 1},	"rate_2x1" },
		{ {2, 2},	"rate_2x2" },
		{ {2, 4},	"rate_2x4" },
		{ {4, 1},	"rate_4x1" },
		{ {4, 2},	"rate_4x2" },
		{ {4, 4},	"rate_4x4" },
	};

	struct TestModeParam
	{
		TestMode		mode;
		const char*		name;
	};

	const std::vector<TestModeParam> testModeParams
	{
		{ TM_SETUP_RATE_WITH_ATOMICS_IN_COMPUTE_SHADER,							"setup_with_atomics" },
		{ TM_SETUP_RATE_WITH_FRAGMENT_SHADER,									"setup_with_fragment" },
		{ TM_SETUP_RATE_WITH_COPYING_FROM_OTHER_IMAGE,							"setup_with_copying" },
		{ TM_SETUP_RATE_WITH_COPYING_FROM_CONCURENT_IMAGE_USING_TRANSFER_QUEUE,	"setup_with_copying_using_transfer_queue_concurent" },
		{ TM_SETUP_RATE_WITH_COPYING_FROM_EXCLUSIVE_IMAGE_USING_TRANSFER_QUEUE,	"setup_with_copying_using_transfer_queue_exclusive" },
		{ TM_SETUP_RATE_WITH_LINEAR_TILED_IMAGE,								"setup_with_linear_tiled_image" },
	};

	de::MovePtr<tcu::TestCaseGroup> mainGroup(new tcu::TestCaseGroup(testCtx, "attachment_rate", ""));

	for (const auto& testModeParam : testModeParams)
	{
		de::MovePtr<tcu::TestCaseGroup> testModeGroup(new tcu::TestCaseGroup(testCtx, testModeParam.name, ""));

		for (const auto& srFormat : srFormats)
		{
			de::MovePtr<tcu::TestCaseGroup> formatGroup(new tcu::TestCaseGroup(testCtx, srFormat.name, ""));
			for (const auto& srRate : srRates)
			{
				formatGroup->addChild(new AttachmentRateTestCase(testCtx, srRate.name, de::SharedPtr<TestParams>(
					new TestParams
					{
						testModeParam.mode,						// TestMode			mode;
						srFormat.format,						// VkFormat			srFormat;
						srRate.count,							// VkExtent2D		srRate;
						useDynamicRendering,					// bool				useDynamicRendering;
						false									// bool				useImagelessFramebuffer;
					}
				)));

				// duplicate all tests for imageless framebuffer
				if (!useDynamicRendering)
				{
					std::string imagelessName = std::string(srRate.name) + "_imageless";
					formatGroup->addChild(new AttachmentRateTestCase(testCtx, imagelessName.c_str(), de::SharedPtr<TestParams>(
						new TestParams
						{
							testModeParam.mode,					// TestMode			mode;
							srFormat.format,					// VkFormat			srFormat;
							srRate.count,						// VkExtent2D		srRate;
							false,								// bool				useDynamicRendering;
							true								// bool				useImagelessFramebuffer;
						}
					)));
				}
			}

			testModeGroup->addChild(formatGroup.release());
		}

		mainGroup->addChild(testModeGroup.release());
	}

	if (!useDynamicRendering)
	{
		de::MovePtr<tcu::TestCaseGroup> miscGroup(new tcu::TestCaseGroup(testCtx, "misc", ""));
		miscGroup->addChild(new AttachmentRateTestCase(testCtx, "two_subpass", de::SharedPtr<TestParams>(
			new TestParams
			{
				TM_TWO_SUBPASS,									// TestMode			mode;
				VK_FORMAT_R8_UINT,								// VkFormat			srFormat;
				{0, 0},											// VkExtent2D		srRate;					// not used in TM_TWO_SUBPASS
				false,											// bool				useDynamicRendering;
				false											// bool				useImagelessFramebuffer;
			}
		)));
		mainGroup->addChild(miscGroup.release());
	}

	parentGroup->addChild(mainGroup.release());
}

}	// FragmentShadingRage
}	// vkt
