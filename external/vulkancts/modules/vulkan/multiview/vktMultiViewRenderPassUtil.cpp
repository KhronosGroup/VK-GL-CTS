/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
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
 * \brief RenderPass utils
 *//*--------------------------------------------------------------------*/

#include "vktMultiViewRenderPassUtil.hpp"
#include "tcuTestCase.hpp"
#include "vkRefUtil.hpp"

using namespace vk;

namespace vkt
{
namespace MultiView
{

AttachmentDescription1::AttachmentDescription1 (const void*						pNext_,
												VkAttachmentDescriptionFlags	flags_,
												VkFormat						format_,
												VkSampleCountFlagBits			samples_,
												VkAttachmentLoadOp				loadOp_,
												VkAttachmentStoreOp				storeOp_,
												VkAttachmentLoadOp				stencilLoadOp_,
												VkAttachmentStoreOp				stencilStoreOp_,
												VkImageLayout					initialLayout_,
												VkImageLayout					finalLayout_)
{
	DE_ASSERT(pNext_ == DE_NULL);

	// No sType field in this struct
	DE_UNREF(pNext_);
	flags			= flags_;
	format			= format_;
	samples			= samples_;
	loadOp			= loadOp_;
	storeOp			= storeOp_;
	stencilLoadOp	= stencilLoadOp_;
	stencilStoreOp	= stencilStoreOp_;
	initialLayout	= initialLayout_;
	finalLayout		= finalLayout_;
}

AttachmentDescription2::AttachmentDescription2 (const void*						pNext_,
												VkAttachmentDescriptionFlags	flags_,
												VkFormat						format_,
												VkSampleCountFlagBits			samples_,
												VkAttachmentLoadOp				loadOp_,
												VkAttachmentStoreOp				storeOp_,
												VkAttachmentLoadOp				stencilLoadOp_,
												VkAttachmentStoreOp				stencilStoreOp_,
												VkImageLayout					initialLayout_,
												VkImageLayout					finalLayout_)
{
	sType			= VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
	pNext			= pNext_;
	flags			= flags_;
	format			= format_;
	samples			= samples_;
	loadOp			= loadOp_;
	storeOp			= storeOp_;
	stencilLoadOp	= stencilLoadOp_;
	stencilStoreOp	= stencilStoreOp_;
	initialLayout	= initialLayout_;
	finalLayout		= finalLayout_;
}

AttachmentReference1::AttachmentReference1 (const void*			pNext_,
											deUint32			attachment_,
											VkImageLayout		layout_,
											VkImageAspectFlags	aspectMask_)
{
	DE_ASSERT(pNext_ == DE_NULL);
	DE_ASSERT(aspectMask_ == 0);

	// No sType field in this struct
	DE_UNREF	(pNext_);
	attachment	= attachment_;
	layout		= layout_;
	DE_UNREF	(aspectMask_);
}

AttachmentReference2::AttachmentReference2 (const void*			pNext_,
											deUint32			attachment_,
											VkImageLayout		layout_,
											VkImageAspectFlags	aspectMask_)
{
	sType		= VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
	pNext		= pNext_;
	attachment	= attachment_;
	layout		= layout_;
	aspectMask	= aspectMask_;
}

SubpassDescription1::SubpassDescription1 (const void*						pNext_,
										  VkSubpassDescriptionFlags			flags_,
										  VkPipelineBindPoint				pipelineBindPoint_,
										  deUint32							viewMask_,
										  deUint32							inputAttachmentCount_,
										  const VkAttachmentReference*		pInputAttachments_,
										  deUint32							colorAttachmentCount_,
										  const VkAttachmentReference*		pColorAttachments_,
										  const VkAttachmentReference*		pResolveAttachments_,
										  const VkAttachmentReference*		pDepthStencilAttachment_,
										  deUint32							preserveAttachmentCount_,
										  const deUint32*					pPreserveAttachments_)
{
	DE_ASSERT(pNext_ == DE_NULL);
	DE_ASSERT(viewMask_ == 0);

	// No sType field in this struct
	DE_UNREF				(pNext_);
	flags					= flags_;
	pipelineBindPoint		= pipelineBindPoint_;
	DE_UNREF				(viewMask_);
	inputAttachmentCount	= inputAttachmentCount_;
	pInputAttachments		= pInputAttachments_;
	colorAttachmentCount	= colorAttachmentCount_;
	pColorAttachments		= pColorAttachments_;
	pResolveAttachments		= pResolveAttachments_;
	pDepthStencilAttachment	= pDepthStencilAttachment_;
	preserveAttachmentCount	= preserveAttachmentCount_;
	pPreserveAttachments	= pPreserveAttachments_;
}

SubpassDescription2::SubpassDescription2 (const void*						pNext_,
										  VkSubpassDescriptionFlags			flags_,
										  VkPipelineBindPoint				pipelineBindPoint_,
										  deUint32							viewMask_,
										  deUint32							inputAttachmentCount_,
										  const VkAttachmentReference2*		pInputAttachments_,
										  deUint32							colorAttachmentCount_,
										  const VkAttachmentReference2*		pColorAttachments_,
										  const VkAttachmentReference2*		pResolveAttachments_,
										  const VkAttachmentReference2*		pDepthStencilAttachment_,
										  deUint32							preserveAttachmentCount_,
										  const deUint32*					pPreserveAttachments_)
{
	sType					= VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2;
	pNext					= pNext_;
	flags					= flags_;
	pipelineBindPoint		= pipelineBindPoint_;
	viewMask				= viewMask_;
	inputAttachmentCount	= inputAttachmentCount_;
	pInputAttachments		= pInputAttachments_;
	colorAttachmentCount	= colorAttachmentCount_;
	pColorAttachments		= pColorAttachments_;
	pResolveAttachments		= pResolveAttachments_;
	pDepthStencilAttachment	= pDepthStencilAttachment_;
	preserveAttachmentCount	= preserveAttachmentCount_;
	pPreserveAttachments	= pPreserveAttachments_;
}

SubpassDependency1::SubpassDependency1 (const void*				pNext_,
										deUint32				srcSubpass_,
										deUint32				dstSubpass_,
										VkPipelineStageFlags	srcStageMask_,
										VkPipelineStageFlags	dstStageMask_,
										VkAccessFlags			srcAccessMask_,
										VkAccessFlags			dstAccessMask_,
										VkDependencyFlags		dependencyFlags_,
										deInt32					viewOffset_)
{
	DE_ASSERT(pNext_ == DE_NULL);
	DE_ASSERT(viewOffset_ == 0);

	// No sType field in this struct
	DE_UNREF		(pNext_);
	srcSubpass		= srcSubpass_;
	dstSubpass		= dstSubpass_;
	srcStageMask	= srcStageMask_;
	dstStageMask	= dstStageMask_;
	srcAccessMask	= srcAccessMask_;
	dstAccessMask	= dstAccessMask_;
	dependencyFlags	= dependencyFlags_;
	DE_UNREF		(viewOffset_);
}

SubpassDependency2::SubpassDependency2 (const void*				pNext_,
										deUint32				srcSubpass_,
										deUint32				dstSubpass_,
										VkPipelineStageFlags	srcStageMask_,
										VkPipelineStageFlags	dstStageMask_,
										VkAccessFlags			srcAccessMask_,
										VkAccessFlags			dstAccessMask_,
										VkDependencyFlags		dependencyFlags_,
										deInt32					viewOffset_)
{
	sType			= VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
	pNext			= pNext_;
	srcSubpass		= srcSubpass_;
	dstSubpass		= dstSubpass_;
	srcStageMask	= srcStageMask_;
	dstStageMask	= dstStageMask_;
	srcAccessMask	= srcAccessMask_;
	dstAccessMask	= dstAccessMask_;
	dependencyFlags	= dependencyFlags_;
	viewOffset		= viewOffset_;
}

RenderPassCreateInfo1::RenderPassCreateInfo1 (const void*						pNext_,
											  VkRenderPassCreateFlags			flags_,
											  deUint32							attachmentCount_,
											  const VkAttachmentDescription*	pAttachments_,
											  deUint32							subpassCount_,
											  const VkSubpassDescription*		pSubpasses_,
											  deUint32							dependencyCount_,
											  const VkSubpassDependency*		pDependencies_,
											  deUint32							correlatedViewMaskCount_,
											  const deUint32*					pCorrelatedViewMasks_)
{
	DE_ASSERT(correlatedViewMaskCount_ == 0);
	DE_ASSERT(pCorrelatedViewMasks_ == DE_NULL);

	sType					= VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	pNext					= pNext_;
	flags					= flags_;
	attachmentCount			= attachmentCount_;
	pAttachments			= pAttachments_;
	subpassCount			= subpassCount_;
	pSubpasses				= pSubpasses_;
	dependencyCount			= dependencyCount_;
	pDependencies			= pDependencies_;
	DE_UNREF				(correlatedViewMaskCount_);
	DE_UNREF				(pCorrelatedViewMasks_);
}

Move<VkRenderPass>	RenderPassCreateInfo1::createRenderPass (const DeviceInterface& vk, VkDevice device) const
{
	return vk::createRenderPass(vk, device, this);
}

RenderPassCreateInfo2::RenderPassCreateInfo2 (const void*							pNext_,
											  VkRenderPassCreateFlags				flags_,
											  deUint32								attachmentCount_,
											  const VkAttachmentDescription2*		pAttachments_,
											  deUint32								subpassCount_,
											  const VkSubpassDescription2*			pSubpasses_,
											  deUint32								dependencyCount_,
											  const VkSubpassDependency2*			pDependencies_,
											  deUint32								correlatedViewMaskCount_,
											  const deUint32*						pCorrelatedViewMasks_)
{
	sType					= VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2;
	pNext					= pNext_;
	flags					= flags_;
	attachmentCount			= attachmentCount_;
	pAttachments			= pAttachments_;
	subpassCount			= subpassCount_;
	pSubpasses				= pSubpasses_;
	dependencyCount			= dependencyCount_;
	pDependencies			= pDependencies_;
	correlatedViewMaskCount	= correlatedViewMaskCount_;
	pCorrelatedViewMasks	= pCorrelatedViewMasks_;
}

Move<VkRenderPass>	RenderPassCreateInfo2::createRenderPass (const DeviceInterface& vk, VkDevice device) const
{
	return vk::createRenderPass2(vk, device, this);
}

SubpassBeginInfo1::SubpassBeginInfo1 (const void*		pNext_,
									  VkSubpassContents	contents_)
	: contents	(contents_)
{
	DE_ASSERT(pNext_ == DE_NULL);

	DE_UNREF(pNext_);
}

SubpassBeginInfo2::SubpassBeginInfo2 (const void*		pNext_,
									  VkSubpassContents	contents_)
{
	sType		= VK_STRUCTURE_TYPE_SUBPASS_BEGIN_INFO;
	pNext		= pNext_;
	contents	= contents_;
}

SubpassEndInfo1::SubpassEndInfo1 (const void*	pNext_)
{
	DE_ASSERT(pNext_ == DE_NULL);

	DE_UNREF(pNext_);
}

SubpassEndInfo2::SubpassEndInfo2 (const void*	pNext_)
{
	sType	= VK_STRUCTURE_TYPE_SUBPASS_END_INFO;
	pNext	= pNext_;
}

void RenderpassSubpass1::cmdBeginRenderPass (const DeviceInterface&			vk,
											 VkCommandBuffer				cmdBuffer,
											 const VkRenderPassBeginInfo*	pRenderPassBegin,
											 const SubpassBeginInfo*		pSubpassBeginInfo)
{
	DE_ASSERT(pSubpassBeginInfo != DE_NULL);

	vk.cmdBeginRenderPass(cmdBuffer, pRenderPassBegin, pSubpassBeginInfo->contents);
}

void RenderpassSubpass1::cmdNextSubpass (const DeviceInterface&		vk,
										 VkCommandBuffer			cmdBuffer,
										 const SubpassBeginInfo*	pSubpassBeginInfo,
										 const SubpassEndInfo*		pSubpassEndInfo)
{
	DE_UNREF(pSubpassEndInfo);
	DE_ASSERT(pSubpassBeginInfo != DE_NULL);

	vk.cmdNextSubpass(cmdBuffer, pSubpassBeginInfo->contents);
}

void RenderpassSubpass1::cmdEndRenderPass (const DeviceInterface&	vk,
										   VkCommandBuffer			cmdBuffer,
										   const SubpassEndInfo*	pSubpassEndInfo)
{
	DE_UNREF(pSubpassEndInfo);

	vk.cmdEndRenderPass(cmdBuffer);
}

void RenderpassSubpass2::cmdBeginRenderPass (const DeviceInterface&			vk,
											 VkCommandBuffer				cmdBuffer,
											 const VkRenderPassBeginInfo*	pRenderPassBegin,
											 const SubpassBeginInfo*		pSubpassBeginInfo)
{
	vk.cmdBeginRenderPass2(cmdBuffer, pRenderPassBegin, pSubpassBeginInfo);
}

void RenderpassSubpass2::cmdNextSubpass (const DeviceInterface&		vk,
										 VkCommandBuffer			cmdBuffer,
										 const SubpassBeginInfo*	pSubpassBeginInfo,
										 const SubpassEndInfo*		pSubpassEndInfo)
{
	DE_ASSERT(pSubpassBeginInfo != DE_NULL);
	DE_ASSERT(pSubpassEndInfo != DE_NULL);

	vk.cmdNextSubpass2(cmdBuffer, pSubpassBeginInfo, pSubpassEndInfo);
}

void RenderpassSubpass2::cmdEndRenderPass (const DeviceInterface&	vk,
										   VkCommandBuffer			cmdBuffer,
										   const SubpassEndInfo*	pSubpassEndInfo)
{
	DE_ASSERT(pSubpassEndInfo != DE_NULL);

	vk.cmdEndRenderPass2(cmdBuffer, pSubpassEndInfo);
}

} // renderpass

} // vkt

