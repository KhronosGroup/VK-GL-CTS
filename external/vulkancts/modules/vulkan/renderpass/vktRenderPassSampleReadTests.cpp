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
 * \brief Tests reading of samples from a previous subpass.
 *//*--------------------------------------------------------------------*/

#include "vktRenderPassSampleReadTests.hpp"
#include "vktRenderPassTestsUtil.hpp"

#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkDefs.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "tcuImageCompare.hpp"
#include "tcuResultCollector.hpp"

#include "deUniquePtr.hpp"

using namespace vk;

using tcu::UVec4;
using tcu::Vec4;

using tcu::ConstPixelBufferAccess;
using tcu::PixelBufferAccess;

using tcu::TestLog;

using std::string;
using std::vector;

namespace vkt
{
namespace
{
using namespace renderpass;

de::MovePtr<Allocation> createBufferMemory (const DeviceInterface&	vk,
											VkDevice				device,
											Allocator&				allocator,
											VkBuffer				buffer)
{
	de::MovePtr<Allocation> allocation (allocator.allocate(getBufferMemoryRequirements(vk, device, buffer), MemoryRequirement::HostVisible));
	VK_CHECK(vk.bindBufferMemory(device, buffer, allocation->getMemory(), allocation->getOffset()));
	return allocation;
}

de::MovePtr<Allocation> createImageMemory (const DeviceInterface&	vk,
										   VkDevice					device,
										   Allocator&				allocator,
										   VkImage					image)
{
	de::MovePtr<Allocation> allocation (allocator.allocate(getImageMemoryRequirements(vk, device, image), MemoryRequirement::Any));
	VK_CHECK(vk.bindImageMemory(device, image, allocation->getMemory(), allocation->getOffset()));
	return allocation;
}

Move<VkImage> createImage (const DeviceInterface&	vk,
						   VkDevice					device,
						   VkImageCreateFlags		flags,
						   VkImageType				imageType,
						   VkFormat					format,
						   VkExtent3D				extent,
						   deUint32					mipLevels,
						   deUint32					arrayLayers,
						   VkSampleCountFlagBits	samples,
						   VkImageTiling			tiling,
						   VkImageUsageFlags		usage,
						   VkSharingMode			sharingMode,
						   deUint32					queueFamilyCount,
						   const deUint32*			pQueueFamilyIndices,
						   VkImageLayout			initialLayout)
{
	const VkImageCreateInfo createInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		DE_NULL,
		flags,
		imageType,
		format,
		extent,
		mipLevels,
		arrayLayers,
		samples,
		tiling,
		usage,
		sharingMode,
		queueFamilyCount,
		pQueueFamilyIndices,
		initialLayout
	};
	return createImage(vk, device, &createInfo);
}

Move<VkImageView> createImageView (const DeviceInterface&	vk,
								   VkDevice					device,
								   VkImageViewCreateFlags	flags,
								   VkImage					image,
								   VkImageViewType			viewType,
								   VkFormat					format,
								   VkComponentMapping		components,
								   VkImageSubresourceRange	subresourceRange)
{
	const VkImageViewCreateInfo pCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		DE_NULL,
		flags,
		image,
		viewType,
		format,
		components,
		subresourceRange,
	};
	return createImageView(vk, device, &pCreateInfo);
}

Move<VkImage> createImage (const InstanceInterface&	vki,
						   VkPhysicalDevice			physicalDevice,
						   const DeviceInterface&	vkd,
						   VkDevice					device,
						   VkFormat					vkFormat,
						   VkSampleCountFlagBits	sampleCountBit,
						   VkImageUsageFlags		usage,
						   deUint32					width,
						   deUint32					height)
{
	try
	{
		const VkImageType				imageType				(VK_IMAGE_TYPE_2D);
		const VkImageTiling				imageTiling				(VK_IMAGE_TILING_OPTIMAL);
		const VkImageFormatProperties	imageFormatProperties	(getPhysicalDeviceImageFormatProperties(vki, physicalDevice, vkFormat, imageType, imageTiling, usage, 0u));
		const VkExtent3D				imageExtent				=
		{
			width,
			height,
			1u
		};

		if (imageFormatProperties.maxExtent.width < imageExtent.width
			|| imageFormatProperties.maxExtent.height < imageExtent.height
			|| ((imageFormatProperties.sampleCounts & sampleCountBit) == 0))
		{
			TCU_THROW(NotSupportedError, "Image type not supported");
		}

		return createImage(vkd, device, 0u, imageType, vkFormat, imageExtent, 1u, 1u, sampleCountBit, imageTiling, usage, VK_SHARING_MODE_EXCLUSIVE, 0u, DE_NULL, VK_IMAGE_LAYOUT_UNDEFINED);
	}
	catch (const vk::Error& error)
	{
		if (error.getError() == VK_ERROR_FORMAT_NOT_SUPPORTED)
			TCU_THROW(NotSupportedError, "Image format not supported");

		throw;
	}
}

Move<VkImageView> createImageView (const DeviceInterface&	vkd,
								   VkDevice					device,
								   VkImage					image,
								   VkFormat					format,
								   VkImageAspectFlags		aspect)
{
	const VkImageSubresourceRange	range =
	{
		aspect,
		0u,
		1u,
		0u,
		1u
	};

	return createImageView(vkd, device, 0u, image, VK_IMAGE_VIEW_TYPE_2D, format, makeComponentMappingRGBA(), range);
}

VkDeviceSize getPixelSize (VkFormat vkFormat)
{
	const tcu::TextureFormat	format	(mapVkFormat(vkFormat));

	return format.getPixelSize();
}

Move<VkBuffer> createBuffer (const DeviceInterface&		vkd,
							 VkDevice					device,
							 VkFormat					format,
							 deUint32					width,
							 deUint32					height)
{
	const VkBufferUsageFlags	bufferUsage			(VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const VkDeviceSize			pixelSize			(getPixelSize(format));
	const VkBufferCreateInfo	createInfo			=
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		DE_NULL,
		0u,

		width * height * pixelSize,
		bufferUsage,

		VK_SHARING_MODE_EXCLUSIVE,
		0u,
		DE_NULL
	};
	return createBuffer(vkd, device, &createInfo);
}

VkSampleCountFlagBits sampleCountBitFromSampleCount (deUint32 count)
{
	switch (count)
	{
		case 1:  return VK_SAMPLE_COUNT_1_BIT;
		case 2:  return VK_SAMPLE_COUNT_2_BIT;
		case 4:  return VK_SAMPLE_COUNT_4_BIT;
		case 8:  return VK_SAMPLE_COUNT_8_BIT;
		case 16: return VK_SAMPLE_COUNT_16_BIT;
		case 32: return VK_SAMPLE_COUNT_32_BIT;
		case 64: return VK_SAMPLE_COUNT_64_BIT;

		default:
			DE_FATAL("Invalid sample count");
			return (VkSampleCountFlagBits)(0x1u << count);
	}
}

template<typename AttachmentDesc, typename AttachmentRef, typename SubpassDesc, typename SubpassDep, typename RenderPassCreateInfo>
Move<VkRenderPass> createRenderPass (const DeviceInterface&	vkd,
									 VkDevice				device,
									 VkFormat				srcFormat,
									 VkFormat				dstFormat,
									 deUint32				sampleCount,
									 RenderingType			renderingType)
{
	const VkSampleCountFlagBits			samples							(sampleCountBitFromSampleCount(sampleCount));
	const VkImageAspectFlagBits			aspectFlag						((renderingType == RENDERING_TYPE_RENDERPASS2) ?	VK_IMAGE_ASPECT_COLOR_BIT :
																															static_cast<VkImageAspectFlagBits>(0u));
	const AttachmentRef					srcAttachmentRef		//  VkAttachmentReference										||  VkAttachmentReference2KHR
	(
																//																||  VkStructureType						sType;
		DE_NULL,												//																||  const void*							pNext;
		0u,														//  deUint32						attachment;					||  deUint32							attachment;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,				//  VkImageLayout					layout;						||  VkImageLayout						layout;
		0u														//																||  VkImageAspectFlags					aspectMask;
	);
	const AttachmentRef					srcAttachmentInputRef	//  VkAttachmentReference										||  VkAttachmentReference2KHR
	(
																//																||  VkStructureType						sType;
		DE_NULL,												//																||  const void*							pNext;
		0u,														//  deUint32						attachment;					||  deUint32							attachment;
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,				//  VkImageLayout					layout;						||  VkImageLayout						layout;
		aspectFlag												//																||  VkImageAspectFlags					aspectMask;
	);
	const AttachmentRef					dstAttachmentRef		//  VkAttachmentReference										||  VkAttachmentReference2KHR
	(
																//																||  VkStructureType						sType;
		DE_NULL,												//																||  const void*							pNext;
		1u,														//  deUint32						attachment;					||  deUint32							attachment;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,				//  VkImageLayout					layout;						||  VkImageLayout						layout;
		0u														//																||  VkImageAspectFlags					aspectMask;
	);
	const AttachmentRef					dstResolveAttachmentRef	//  VkAttachmentReference										||  VkAttachmentReference2KHR
	(
																//																||  VkStructureType						sType;
		DE_NULL,												//																||  const void*							pNext;
		2u,														//  deUint32						attachment;					||  deUint32							attachment;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,				//  VkImageLayout					layout;						||  VkImageLayout						layout;
		0u														//																||  VkImageAspectFlags					aspectMask;
	);
	const SubpassDep					dependency				//  VkSubpassDependency											||  VkSubpassDependency2KHR
	(
																//																||	VkStructureType						sType;
		DE_NULL,												//																||	const void*							pNext;
		0u,														//  deUint32						srcSubpass;					||	deUint32							srcSubpass;
		1u,														//  deUint32						dstSubpass;					||	deUint32							dstSubpass;
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,			//  VkPipelineStageFlags			srcStageMask;				||	VkPipelineStageFlags				srcStageMask;
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,					//  VkPipelineStageFlags			dstStageMask;				||	VkPipelineStageFlags				dstStageMask;
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,					//  VkAccessFlags					srcAccessMask;				||	VkAccessFlags						srcAccessMask;
		VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,					//  VkAccessFlags					dstAccessMask;				||	VkAccessFlags						dstAccessMask;
		VK_DEPENDENCY_BY_REGION_BIT,							//  VkDependencyFlags				dependencyFlags;			||	VkDependencyFlags					dependencyFlags;
		0u														//																||	deInt32								viewOffset;
	);
	const AttachmentDesc				srcAttachment				//  VkAttachmentDescription										||  VkAttachmentDescription2KHR
	(
																	//																||  VkStructureType						sType;
		DE_NULL,													//																||  const void*							pNext;
		0u,															//  VkAttachmentDescriptionFlags	flags;						||  VkAttachmentDescriptionFlags		flags;
		srcFormat,													//  VkFormat						format;						||  VkFormat							format;
		samples,													//  VkSampleCountFlagBits			samples;					||  VkSampleCountFlagBits				samples;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,							//  VkAttachmentLoadOp				loadOp;						||  VkAttachmentLoadOp					loadOp;
		VK_ATTACHMENT_STORE_OP_DONT_CARE,							//  VkAttachmentStoreOp				storeOp;					||  VkAttachmentStoreOp					storeOp;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,							//  VkAttachmentLoadOp				stencilLoadOp;				||  VkAttachmentLoadOp					stencilLoadOp;
		VK_ATTACHMENT_STORE_OP_DONT_CARE,							//  VkAttachmentStoreOp				stencilStoreOp;				||  VkAttachmentStoreOp					stencilStoreOp;
		VK_IMAGE_LAYOUT_UNDEFINED,									//  VkImageLayout					initialLayout;				||  VkImageLayout						initialLayout;
		VK_IMAGE_LAYOUT_GENERAL										//  VkImageLayout					finalLayout;				||  VkImageLayout						finalLayout;
	);
	const AttachmentDesc				dstMultisampleAttachment	//  VkAttachmentDescription										||  VkAttachmentDescription2KHR
	(
																	//																||  VkStructureType						sType;
		DE_NULL,													//																||  const void*							pNext;
		0u,															//  VkAttachmentDescriptionFlags	flags;						||  VkAttachmentDescriptionFlags		flags;
		dstFormat,													//  VkFormat						format;						||  VkFormat							format;
		samples,													//  VkSampleCountFlagBits			samples;					||  VkSampleCountFlagBits				samples;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,							//  VkAttachmentLoadOp				loadOp;						||  VkAttachmentLoadOp					loadOp;
		VK_ATTACHMENT_STORE_OP_DONT_CARE,							//  VkAttachmentStoreOp				storeOp;					||  VkAttachmentStoreOp					storeOp;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,							//  VkAttachmentLoadOp				stencilLoadOp;				||  VkAttachmentLoadOp					stencilLoadOp;
		VK_ATTACHMENT_STORE_OP_DONT_CARE,							//  VkAttachmentStoreOp				stencilStoreOp;				||  VkAttachmentStoreOp					stencilStoreOp;
		VK_IMAGE_LAYOUT_UNDEFINED,									//  VkImageLayout					initialLayout;				||  VkImageLayout						initialLayout;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL					//  VkImageLayout					finalLayout;				||  VkImageLayout						finalLayout;
	);
	const AttachmentDesc				dstResolveAttachment		//  VkAttachmentDescription										||  VkAttachmentDescription2KHR
	(
																	//																||  VkStructureType						sType;
		DE_NULL,													//																||  const void*							pNext;
		0u,															//  VkAttachmentDescriptionFlags	flags;						||  VkAttachmentDescriptionFlags		flags;
		dstFormat,													//  VkFormat						format;						||  VkFormat							format;
		VK_SAMPLE_COUNT_1_BIT,										//  VkSampleCountFlagBits			samples;					||  VkSampleCountFlagBits				samples;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,							//  VkAttachmentLoadOp				loadOp;						||  VkAttachmentLoadOp					loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,								//  VkAttachmentStoreOp				storeOp;					||  VkAttachmentStoreOp					storeOp;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,							//  VkAttachmentLoadOp				stencilLoadOp;				||  VkAttachmentLoadOp					stencilLoadOp;
		VK_ATTACHMENT_STORE_OP_STORE,								//  VkAttachmentStoreOp				stencilStoreOp;				||  VkAttachmentStoreOp					stencilStoreOp;
		VK_IMAGE_LAYOUT_UNDEFINED,									//  VkImageLayout					initialLayout;				||  VkImageLayout						initialLayout;
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL						//  VkImageLayout					finalLayout;				||  VkImageLayout						finalLayout;
	);
	const AttachmentDesc				attachments[]				=
	{
		srcAttachment,
		dstMultisampleAttachment,
		dstResolveAttachment
	};
	const SubpassDesc					subpass1					//  VkSubpassDescription										||  VkSubpassDescription2KHR
	(
																	//																||  VkStructureType						sType;
		DE_NULL,													//																||  const void*							pNext;
		(VkSubpassDescriptionFlags)0,								//  VkSubpassDescriptionFlags		flags;						||  VkSubpassDescriptionFlags			flags;
		VK_PIPELINE_BIND_POINT_GRAPHICS,							//  VkPipelineBindPoint				pipelineBindPoint;			||  VkPipelineBindPoint					pipelineBindPoint;
		0u,															//																||  deUint32							viewMask;
		0u,															//  deUint32						inputAttachmentCount;		||  deUint32							inputAttachmentCount;
		DE_NULL,													//  const VkAttachmentReference*	pInputAttachments;			||  const VkAttachmentReference2KHR*	pInputAttachments;
		1u,															//  deUint32						colorAttachmentCount;		||  deUint32							colorAttachmentCount;
		&srcAttachmentRef,											//  const VkAttachmentReference*	pColorAttachments;			||  const VkAttachmentReference2KHR*	pColorAttachments;
		DE_NULL,													//  const VkAttachmentReference*	pResolveAttachments;		||  const VkAttachmentReference2KHR*	pResolveAttachments;
		DE_NULL,													//  const VkAttachmentReference*	pDepthStencilAttachment;	||  const VkAttachmentReference2KHR*	pDepthStencilAttachment;
		0u,															//  deUint32						preserveAttachmentCount;	||  deUint32							preserveAttachmentCount;
		DE_NULL														//  const deUint32*					pPreserveAttachments;		||  const deUint32*						pPreserveAttachments;
	);
	const SubpassDesc					subpass2					//  VkSubpassDescription										||  VkSubpassDescription2KHR
	(
																	//																||  VkStructureType						sType;
		DE_NULL,													//																||  const void*							pNext;
		(VkSubpassDescriptionFlags)0,								//  VkSubpassDescriptionFlags		flags;						||  VkSubpassDescriptionFlags			flags;
		VK_PIPELINE_BIND_POINT_GRAPHICS,							//  VkPipelineBindPoint				pipelineBindPoint;			||  VkPipelineBindPoint					pipelineBindPoint;
		0u,															//																||  deUint32							viewMask;
		1u,															//  deUint32						inputAttachmentCount;		||  deUint32							inputAttachmentCount;
		&srcAttachmentInputRef,										//  const VkAttachmentReference*	pInputAttachments;			||  const VkAttachmentReference2KHR*	pInputAttachments;
		1u,															//  deUint32						colorAttachmentCount;		||  deUint32							colorAttachmentCount;
		&dstAttachmentRef,											//  const VkAttachmentReference*	pColorAttachments;			||  const VkAttachmentReference2KHR*	pColorAttachments;
		&dstResolveAttachmentRef,									//  const VkAttachmentReference*	pResolveAttachments;		||  const VkAttachmentReference2KHR*	pResolveAttachments;
		DE_NULL,													//  const VkAttachmentReference*	pDepthStencilAttachment;	||  const VkAttachmentReference2KHR*	pDepthStencilAttachment;
		0u,															//  deUint32						preserveAttachmentCount;	||  deUint32							preserveAttachmentCount;
		DE_NULL														//  const deUint32*					pPreserveAttachments;		||  const deUint32*						pPreserveAttachments;
	);
	const SubpassDesc					subpasses[]					=
	{
		subpass1,
		subpass2
	};
	const RenderPassCreateInfo			renderPassCreator			//  VkRenderPassCreateInfo										||  VkRenderPassCreateInfo2KHR
	(
																	//  VkStructureType					sType;						||  VkStructureType						sType;
		DE_NULL,													//  const void*						pNext;						||  const void*							pNext;
		(VkRenderPassCreateFlags)0u,								//  VkRenderPassCreateFlags			flags;						||  VkRenderPassCreateFlags				flags;
		3u,															//  deUint32						attachmentCount;			||  deUint32							attachmentCount;
		attachments,												//  const VkAttachmentDescription*	pAttachments;				||  const VkAttachmentDescription2KHR*	pAttachments;
		2u,															//  deUint32						subpassCount;				||  deUint32							subpassCount;
		subpasses,													//  const VkSubpassDescription*		pSubpasses;					||  const VkSubpassDescription2KHR*		pSubpasses;
		1u,															//  deUint32						dependencyCount;			||  deUint32							dependencyCount;
		&dependency,												//  const VkSubpassDependency*		pDependencies;				||  const VkSubpassDependency2KHR*		pDependencies;
		0u,															//																||  deUint32							correlatedViewMaskCount;
		DE_NULL														//																||  const deUint32*						pCorrelatedViewMasks;
	);

	return renderPassCreator.createRenderPass(vkd, device);
}

Move<VkRenderPass> createRenderPass (const DeviceInterface&	vkd,
									 VkDevice				device,
									 VkFormat				srcFormat,
									 VkFormat				dstFormat,
									 deUint32				sampleCount,
									 RenderingType			renderingType)
{
	switch (renderingType)
	{
		case RENDERING_TYPE_RENDERPASS_LEGACY:
			return createRenderPass<AttachmentDescription1, AttachmentReference1, SubpassDescription1, SubpassDependency1, RenderPassCreateInfo1>(vkd, device, srcFormat, dstFormat, sampleCount, renderingType);
		case RENDERING_TYPE_RENDERPASS2:
			return createRenderPass<AttachmentDescription2, AttachmentReference2, SubpassDescription2, SubpassDependency2, RenderPassCreateInfo2>(vkd, device, srcFormat, dstFormat, sampleCount, renderingType);
		default:
			TCU_THROW(InternalError, "Impossible");
	}
}

Move<VkFramebuffer> createFramebuffer (const DeviceInterface&	vkd,
									   VkDevice					device,
									   VkRenderPass				renderPass,
									   VkImageView				srcImageView,
									   VkImageView				dstMultisampleImageView,
									   VkImageView				dstSinglesampleImageView,
									   deUint32					width,
									   deUint32					height)
{
	VkImageView attachments[] =
	{
		srcImageView,
		dstMultisampleImageView,
		dstSinglesampleImageView
	};

	const VkFramebufferCreateInfo	createInfo	=
	{
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		DE_NULL,
		0u,

		renderPass,
		3u,
		attachments,

		width,
		height,
		1u
	};

	return createFramebuffer(vkd, device, &createInfo);
}

Move<VkPipelineLayout> createRenderPipelineLayout (const DeviceInterface&	vkd,
												   VkDevice					device)
{
	const VkPipelineLayoutCreateInfo	createInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		DE_NULL,
		(vk::VkPipelineLayoutCreateFlags)0,

		0u,
		DE_NULL,

		0u,
		DE_NULL
	};

	return createPipelineLayout(vkd, device, &createInfo);
}

Move<VkPipeline> createRenderPipeline (const DeviceInterface&		vkd,
									   VkDevice						device,
									   VkRenderPass					renderPass,
									   VkPipelineLayout				pipelineLayout,
									   const vk::BinaryCollection&	binaryCollection,
									   deUint32						width,
									   deUint32						height,
									   deUint32						sampleCount)
{
	const Unique<VkShaderModule>					vertexShaderModule				(createShaderModule(vkd, device, binaryCollection.get("quad-vert"), 0u));
	const Unique<VkShaderModule>					fragmentShaderModule			(createShaderModule(vkd, device, binaryCollection.get("quad-frag"), 0u));
	const VkPipelineVertexInputStateCreateInfo		vertexInputState				=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		DE_NULL,
		(VkPipelineVertexInputStateCreateFlags)0u,

		0u,
		DE_NULL,

		0u,
		DE_NULL
	};
	const std::vector<VkViewport>					viewports						(1, makeViewport(tcu::UVec2(width, height)));
	const std::vector<VkRect2D>						scissors						(1, makeRect2D(tcu::UVec2(width, height)));

	const VkPipelineMultisampleStateCreateInfo		multisampleState				=
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		DE_NULL,
		(VkPipelineMultisampleStateCreateFlags)0u,

		sampleCountBitFromSampleCount(sampleCount),
		VK_TRUE,
		1.0f,
		DE_NULL,
		VK_FALSE,
		VK_FALSE,
	};

	return makeGraphicsPipeline(vkd,									// const DeviceInterface&                        vk
								device,									// const VkDevice                                device
								pipelineLayout,							// const VkPipelineLayout                        pipelineLayout
								*vertexShaderModule,					// const VkShaderModule                          vertexShaderModule
								DE_NULL,								// const VkShaderModule                          tessellationControlShaderModule
								DE_NULL,								// const VkShaderModule                          tessellationEvalShaderModule
								DE_NULL,								// const VkShaderModule                          geometryShaderModule
								*fragmentShaderModule,					// const VkShaderModule                          fragmentShaderModule
								renderPass,								// const VkRenderPass                            renderPass
								viewports,								// const std::vector<VkViewport>&                viewports
								scissors,								// const std::vector<VkRect2D>&                  scissors
								VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,	// const VkPrimitiveTopology                     topology
								0u,										// const deUint32                                subpass
								0u,										// const deUint32                                patchControlPoints
								&vertexInputState,						// const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
								DE_NULL,								// const VkPipelineRasterizationStateCreateInfo* rasterizationStateCreateInfo
								&multisampleState);						// const VkPipelineMultisampleStateCreateInfo*   multisampleStateCreateInfo
}

Move<VkDescriptorSetLayout> createSubpassDescriptorSetLayout (const DeviceInterface&	vkd,
															  VkDevice					device)
{
	const VkDescriptorSetLayoutBinding		bindings[]	=
	{
		{
			0u,
			VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
			1u,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			DE_NULL
		},
		{
			1u,
			VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
			1u,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			DE_NULL
		}
	};
	const VkDescriptorSetLayoutCreateInfo	createInfo	=
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		DE_NULL,
		0u,

		1u,
		bindings
	};

	return createDescriptorSetLayout(vkd, device, &createInfo);
}

Move<VkPipelineLayout> createSubpassPipelineLayout (const DeviceInterface&	vkd,
												  VkDevice					device,
												  VkDescriptorSetLayout		descriptorSetLayout)
{
	const VkPipelineLayoutCreateInfo	createInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		DE_NULL,
		(vk::VkPipelineLayoutCreateFlags)0,

		1u,
		&descriptorSetLayout,

		0u,
		DE_NULL
	};

	return createPipelineLayout(vkd, device, &createInfo);
}

Move<VkPipeline> createSubpassPipeline (const DeviceInterface&		vkd,
									  VkDevice						device,
									  VkRenderPass					renderPass,
									  VkPipelineLayout				pipelineLayout,
									  const vk::BinaryCollection&	binaryCollection,
									  deUint32						width,
									  deUint32						height,
									  deUint32						sampleCount)
{
	const Unique<VkShaderModule>					vertexShaderModule			(createShaderModule(vkd, device, binaryCollection.get("quad-vert"), 0u));
	const Unique<VkShaderModule>					fragmentShaderModule		(createShaderModule(vkd, device, binaryCollection.get("quad-subpass-frag"), 0u));

	const VkPipelineVertexInputStateCreateInfo		vertexInputState			=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		DE_NULL,
		(VkPipelineVertexInputStateCreateFlags)0u,

		0u,
		DE_NULL,

		0u,
		DE_NULL
	};

	const std::vector<VkViewport>					viewports					(1, makeViewport(tcu::UVec2(width, height)));
	const std::vector<VkRect2D>						scissors					(1, makeRect2D(tcu::UVec2(width, height)));

	const VkPipelineMultisampleStateCreateInfo		multisampleState			=
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		DE_NULL,
		(VkPipelineMultisampleStateCreateFlags)0u,

		sampleCountBitFromSampleCount(sampleCount),
		VK_FALSE,
		0.0f,
		DE_NULL,
		VK_FALSE,
		VK_FALSE,
	};

	return makeGraphicsPipeline(vkd,									// const DeviceInterface&                        vk
								device,									// const VkDevice                                device
								pipelineLayout,							// const VkPipelineLayout                        pipelineLayout
								*vertexShaderModule,					// const VkShaderModule                          vertexShaderModule
								DE_NULL,								// const VkShaderModule                          tessellationControlShaderModule
								DE_NULL,								// const VkShaderModule                          tessellationEvalShaderModule
								DE_NULL,								// const VkShaderModule                          geometryShaderModule
								*fragmentShaderModule,					// const VkShaderModule                          fragmentShaderModule
								renderPass,								// const VkRenderPass                            renderPass
								viewports,								// const std::vector<VkViewport>&                viewports
								scissors,								// const std::vector<VkRect2D>&                  scissors
								VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,	// const VkPrimitiveTopology                     topology
								1u,										// const deUint32                                subpass
								0u,										// const deUint32                                patchControlPoints
								&vertexInputState,						// const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
								DE_NULL,								// const VkPipelineRasterizationStateCreateInfo* rasterizationStateCreateInfo
								&multisampleState);						// const VkPipelineMultisampleStateCreateInfo*   multisampleStateCreateInfo
}

Move<VkDescriptorPool> createSubpassDescriptorPool (const DeviceInterface&	vkd,
												  VkDevice					device)
{
	const VkDescriptorPoolSize			size		=
	{
		VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 2u
	};
	const VkDescriptorPoolCreateInfo	createInfo	=
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		DE_NULL,
		VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,

		2u,
		1u,
		&size
	};

	return createDescriptorPool(vkd, device, &createInfo);
}

Move<VkDescriptorSet> createSubpassDescriptorSet (const DeviceInterface&	vkd,
												  VkDevice					device,
												  VkDescriptorPool			pool,
												  VkDescriptorSetLayout		layout,
												  VkImageView				imageView)
{
	const VkDescriptorSetAllocateInfo	allocateInfo	=
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		DE_NULL,

		pool,
		1u,
		&layout
	};
	Move<VkDescriptorSet> set (allocateDescriptorSet(vkd, device, &allocateInfo));

	{
		const VkDescriptorImageInfo	imageInfo	=
		{
			(VkSampler)0u,
			imageView,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};
		const VkWriteDescriptorSet	write		=
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			DE_NULL,

			*set,
			0u,
			0u,
			1u,
			VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
			&imageInfo,
			DE_NULL,
			DE_NULL
		};

		vkd.updateDescriptorSets(device, 1u, &write, 0u, DE_NULL);
	}
	return set;
}

enum TestMode
{
	TESTMODE_ADD = 0,
	TESTMODE_SELECT,

	TESTMODE_LAST
};

struct TestConfig
{
	TestConfig (deUint32 sampleCount_, TestMode testMode_, deUint32 selectedSample_, RenderingType	renderingType_)
	: sampleCount		(sampleCount_)
	, testMode			(testMode_)
	, selectedSample	(selectedSample_)
	, renderingType		(renderingType_)
	{
	}

	deUint32		sampleCount;
	TestMode		testMode;
	deUint32		selectedSample;
	RenderingType	renderingType;
};

class SampleReadTestInstance : public TestInstance
{
public:
											SampleReadTestInstance	(Context& context, TestConfig config);
											~SampleReadTestInstance	(void);

	tcu::TestStatus							iterate					(void);

	template<typename RenderpassSubpass>
	tcu::TestStatus							iterateInternal			(void);

private:
	const bool								m_extensionSupported;
	const RenderingType						m_renderingType;

	const deUint32							m_sampleCount;
	const deUint32							m_width;
	const deUint32							m_height;
	const TestMode							m_testMode;
	const deUint32							m_selectedSample;

	const Unique<VkImage>					m_srcImage;
	const de::UniquePtr<Allocation>			m_srcImageMemory;
	const Unique<VkImageView>				m_srcImageView;
	const Unique<VkImageView>				m_srcInputImageView;

	const Unique<VkImage>					m_dstMultisampleImage;
	const de::UniquePtr<Allocation>			m_dstMultisampleImageMemory;
	const Unique<VkImageView>				m_dstMultisampleImageView;

	const Unique<VkImage>					m_dstSinglesampleImage;
	const de::UniquePtr<Allocation>			m_dstSinglesampleImageMemory;
	const Unique<VkImageView>				m_dstSinglesampleImageView;

	const Unique<VkBuffer>					m_dstBuffer;
	const de::UniquePtr<Allocation>			m_dstBufferMemory;

	const Unique<VkRenderPass>				m_renderPass;
	const Unique<VkFramebuffer>				m_framebuffer;

	const Unique<VkPipelineLayout>			m_renderPipelineLayout;
	const Unique<VkPipeline>				m_renderPipeline;

	const Unique<VkDescriptorSetLayout>		m_subpassDescriptorSetLayout;
	const Unique<VkPipelineLayout>			m_subpassPipelineLayout;
	const Unique<VkPipeline>				m_subpassPipeline;
	const Unique<VkDescriptorPool>			m_subpassDescriptorPool;
	const Unique<VkDescriptorSet>			m_subpassDescriptorSet;

	const Unique<VkCommandPool>				m_commandPool;
	tcu::ResultCollector					m_resultCollector;
};

SampleReadTestInstance::SampleReadTestInstance (Context& context, TestConfig config)
	: TestInstance					(context)
	, m_extensionSupported			(context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SAMPLE_RATE_SHADING) &&
									 ((config.renderingType != RENDERING_TYPE_RENDERPASS2) || context.requireDeviceFunctionality("VK_KHR_create_renderpass2")))
	, m_renderingType				(config.renderingType)
	, m_sampleCount					(config.sampleCount)
	, m_width						(32u)
	, m_height						(32u)
	, m_testMode					(config.testMode)
	, m_selectedSample				(config.selectedSample)
	, m_srcImage					(createImage(context.getInstanceInterface(), context.getPhysicalDevice(), context.getDeviceInterface(), context.getDevice(), VK_FORMAT_R32_UINT, sampleCountBitFromSampleCount(m_sampleCount), VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, m_width, m_height))
	, m_srcImageMemory				(createImageMemory(context.getDeviceInterface(), context.getDevice(), context.getDefaultAllocator(), *m_srcImage))
	, m_srcImageView				(createImageView(context.getDeviceInterface(), context.getDevice(), *m_srcImage, VK_FORMAT_R32_UINT, VK_IMAGE_ASPECT_COLOR_BIT))
	, m_srcInputImageView			(createImageView(context.getDeviceInterface(), context.getDevice(), *m_srcImage, VK_FORMAT_R32_UINT, VK_IMAGE_ASPECT_COLOR_BIT))
	, m_dstMultisampleImage			(createImage(context.getInstanceInterface(), context.getPhysicalDevice(), context.getDeviceInterface(), context.getDevice(), VK_FORMAT_R32_UINT, sampleCountBitFromSampleCount(m_sampleCount), VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, m_width, m_height))
	, m_dstMultisampleImageMemory	(createImageMemory(context.getDeviceInterface(), context.getDevice(), context.getDefaultAllocator(), *m_dstMultisampleImage))
	, m_dstMultisampleImageView		(createImageView(context.getDeviceInterface(), context.getDevice(), *m_dstMultisampleImage, VK_FORMAT_R32_UINT, VK_IMAGE_ASPECT_COLOR_BIT))
	, m_dstSinglesampleImage		(createImage(context.getInstanceInterface(), context.getPhysicalDevice(), context.getDeviceInterface(), context.getDevice(), VK_FORMAT_R32_UINT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, m_width, m_height))
	, m_dstSinglesampleImageMemory	(createImageMemory(context.getDeviceInterface(), context.getDevice(), context.getDefaultAllocator(), *m_dstSinglesampleImage))
	, m_dstSinglesampleImageView	(createImageView(context.getDeviceInterface(), context.getDevice(), *m_dstSinglesampleImage, VK_FORMAT_R32_UINT, VK_IMAGE_ASPECT_COLOR_BIT))
	, m_dstBuffer					(createBuffer(context.getDeviceInterface(), context.getDevice(), VK_FORMAT_R32_UINT, m_width, m_height))
	, m_dstBufferMemory				(createBufferMemory(context.getDeviceInterface(), context.getDevice(), context.getDefaultAllocator(), *m_dstBuffer))
	, m_renderPass					(createRenderPass(context.getDeviceInterface(), context.getDevice(), VK_FORMAT_R32_UINT, VK_FORMAT_R32_UINT, m_sampleCount, config.renderingType))
	, m_framebuffer					(createFramebuffer(context.getDeviceInterface(), context.getDevice(), *m_renderPass, *m_srcImageView, *m_dstMultisampleImageView, *m_dstSinglesampleImageView, m_width, m_height))
	, m_renderPipelineLayout		(createRenderPipelineLayout(context.getDeviceInterface(), context.getDevice()))
	, m_renderPipeline				(createRenderPipeline(context.getDeviceInterface(), context.getDevice(), *m_renderPass, *m_renderPipelineLayout, context.getBinaryCollection(), m_width, m_height, m_sampleCount))
	, m_subpassDescriptorSetLayout	(createSubpassDescriptorSetLayout(context.getDeviceInterface(), context.getDevice()))
	, m_subpassPipelineLayout		(createSubpassPipelineLayout(context.getDeviceInterface(), context.getDevice(), *m_subpassDescriptorSetLayout))
	, m_subpassPipeline				(createSubpassPipeline(context.getDeviceInterface(), context.getDevice(), *m_renderPass, *m_subpassPipelineLayout, context.getBinaryCollection(), m_width, m_height, m_sampleCount))
	, m_subpassDescriptorPool		(createSubpassDescriptorPool(context.getDeviceInterface(), context.getDevice()))
	, m_subpassDescriptorSet		(createSubpassDescriptorSet(context.getDeviceInterface(), context.getDevice(), *m_subpassDescriptorPool, *m_subpassDescriptorSetLayout, *m_srcInputImageView))
	, m_commandPool					(createCommandPool(context.getDeviceInterface(), context.getDevice(), VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, context.getUniversalQueueFamilyIndex()))
{
}

SampleReadTestInstance::~SampleReadTestInstance (void)
{
}

tcu::TestStatus SampleReadTestInstance::iterate (void)
{
	switch (m_renderingType)
	{
		case RENDERING_TYPE_RENDERPASS_LEGACY:
			return iterateInternal<RenderpassSubpass1>();
		case RENDERING_TYPE_RENDERPASS2:
			return iterateInternal<RenderpassSubpass2>();
		default:
			TCU_THROW(InternalError, "Impossible");
	}
}

template<typename RenderpassSubpass>
tcu::TestStatus SampleReadTestInstance::iterateInternal (void)
{
	const DeviceInterface&								vkd					(m_context.getDeviceInterface());
	const VkDevice										device				(m_context.getDevice());
	const Unique<VkCommandBuffer>						commandBuffer		(allocateCommandBuffer(vkd, device, *m_commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	const typename RenderpassSubpass::SubpassBeginInfo	subpassBeginInfo	(DE_NULL, VK_SUBPASS_CONTENTS_INLINE);
	const typename RenderpassSubpass::SubpassEndInfo	subpassEndInfo		(DE_NULL);

	beginCommandBuffer(vkd, *commandBuffer);

	{
		const VkRenderPassBeginInfo beginInfo =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			DE_NULL,

			*m_renderPass,
			*m_framebuffer,

			{
				{ 0u, 0u },
				{ m_width, m_height }
			},

			0u,
			DE_NULL
		};
		RenderpassSubpass::cmdBeginRenderPass(vkd, *commandBuffer, &beginInfo, &subpassBeginInfo);
	}

	vkd.cmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_renderPipeline);

	vkd.cmdDraw(*commandBuffer, 6u, 1u, 0u, 0u);

	RenderpassSubpass::cmdNextSubpass(vkd, *commandBuffer, &subpassBeginInfo, &subpassEndInfo);

	vkd.cmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_subpassPipeline);
	vkd.cmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_subpassPipelineLayout, 0u, 1u,  &*m_subpassDescriptorSet, 0u, DE_NULL);
	vkd.cmdDraw(*commandBuffer, 6u, 1u, 0u, 0u);

	RenderpassSubpass::cmdEndRenderPass(vkd, *commandBuffer, &subpassEndInfo);

	copyImageToBuffer(vkd, *commandBuffer, *m_dstSinglesampleImage, *m_dstBuffer, tcu::IVec2(m_width, m_height), VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

	endCommandBuffer(vkd, *commandBuffer);

	submitCommandsAndWait(vkd, device, m_context.getUniversalQueue(), *commandBuffer);

	{
		invalidateAlloc(vkd, device, *m_dstBufferMemory);

		const tcu::TextureFormat			format		(mapVkFormat(VK_FORMAT_R32_UINT));
		const void* const					ptr			(m_dstBufferMemory->getHostPtr());
		const tcu::ConstPixelBufferAccess	access		(format, m_width, m_height, 1, ptr);
		tcu::TextureLevel					reference	(format, m_width, m_height);

		for (deUint32 y = 0; y < m_height; y++)
		for (deUint32 x = 0; x < m_width; x++)
		{
			deUint32		bits;

			if (m_testMode == TESTMODE_ADD)
				bits = m_sampleCount == 32 ? 0xffffffff : (1u << m_sampleCount) - 1;
			else
				bits = 1u << m_selectedSample;

			const UVec4		color	(bits, 0, 0, 0xffffffff);

			reference.getAccess().setPixel(color, x, y);
		}

		if (!tcu::intThresholdCompare(m_context.getTestContext().getLog(), "", "", reference.getAccess(), access, UVec4(0u), tcu::COMPARE_LOG_ON_ERROR))
			m_resultCollector.fail("Compare failed.");
	}

	return tcu::TestStatus(m_resultCollector.getResult(), m_resultCollector.getMessage());
}

struct Programs
{
	void init (vk::SourceCollections& dst, TestConfig config) const
	{
		std::ostringstream				fragmentShader;
		std::ostringstream				subpassShader;

		dst.glslSources.add("quad-vert") << glu::VertexSource(
			"#version 450\n"
			"out gl_PerVertex {\n"
			"\tvec4 gl_Position;\n"
			"};\n"
			"highp float;\n"
			"void main (void)\n"
			"{\n"
			"    gl_Position = vec4(((gl_VertexIndex + 2) / 3) % 2 == 0 ? -1.0 : 1.0,\n"
			"                       ((gl_VertexIndex + 1) / 3) % 2 == 0 ? -1.0 : 1.0, 0.0, 1.0);\n"
			"}\n");

		fragmentShader <<
			"#version 450\n"
			"layout(location = 0) out highp uvec4 o_color;\n"
			"void main (void)\n"
			"{\n"
			"    o_color = uvec4(1u << gl_SampleID, 0, 0, 0);\n"
			"}\n";

		dst.glslSources.add("quad-frag") << glu::FragmentSource(fragmentShader.str());

		subpassShader <<
			"#version 450\n"
			"layout(input_attachment_index = 0, set = 0, binding = 0) uniform highp usubpassInputMS i_color;\n"
			"layout(location = 0) out highp uvec4 o_color;\n"
			"void main (void)\n"
			"{\n"
			"    o_color = uvec4(0);\n";

		if (config.testMode == TESTMODE_ADD)
		{
			subpassShader <<
				"    for (int i = 0; i < " << config.sampleCount << "; i++)\n" <<
				"        o_color.r += subpassLoad(i_color, i).r;\n";
		}
		else
		{
			subpassShader <<
				"    o_color.r = subpassLoad(i_color, " << de::toString(config.selectedSample) << ").r;\n";
		}

		subpassShader << "}\n";

		dst.glslSources.add("quad-subpass-frag") << glu::FragmentSource(subpassShader.str());
	}
};

void initTests (tcu::TestCaseGroup* group, RenderingType renderingType)
{
	const deUint32			sampleCounts[]	= { 2u, 4u, 8u, 16u, 32u };
	tcu::TestContext&		testCtx			(group->getTestContext());

	for (deUint32 sampleCountNdx = 0; sampleCountNdx < DE_LENGTH_OF_ARRAY(sampleCounts); sampleCountNdx++)
	{
		const deUint32		sampleCount	(sampleCounts[sampleCountNdx]);
		{
			const TestConfig	testConfig	(sampleCount, TESTMODE_ADD, 0, renderingType);
			const std::string	testName	("numsamples_" + de::toString(sampleCount) + "_add");

			group->addChild(new InstanceFactory1<SampleReadTestInstance, TestConfig, Programs>(testCtx, tcu::NODETYPE_SELF_VALIDATE, testName.c_str(), testName.c_str(), testConfig));
		}

		for (deUint32 sample = 0; sample < sampleCount; sample++)
		{
			const TestConfig	testConfig	(sampleCount, TESTMODE_SELECT, sample, renderingType);
			const std::string	testName	("numsamples_" + de::toString(sampleCount) + "_selected_sample_" + de::toString(sample));

			group->addChild(new InstanceFactory1<SampleReadTestInstance, TestConfig, Programs>(testCtx, tcu::NODETYPE_SELF_VALIDATE, testName.c_str(), testName.c_str(), testConfig));
		}
	}
}

} // anonymous

tcu::TestCaseGroup* createRenderPassSampleReadTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "sampleread", "Sample reading tests", initTests, RENDERING_TYPE_RENDERPASS_LEGACY);
}

tcu::TestCaseGroup* createRenderPass2SampleReadTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "sampleread", "Sample reading tests", initTests, RENDERING_TYPE_RENDERPASS2);
}

} // vkt
