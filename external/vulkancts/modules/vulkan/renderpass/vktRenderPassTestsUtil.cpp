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
 * \brief RenderPass test utils
 *//*--------------------------------------------------------------------*/

#include "vktRenderPassTestsUtil.hpp"
#include "tcuTestCase.hpp"
#include "vkMemUtil.hpp"
#include "vkRefUtil.hpp"

#include <vector>

using namespace vk;

namespace vkt
{
namespace renderpass
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

// For internal to RP/RP2 conversions

AttachmentReference::AttachmentReference (deUint32				attachment,
										  VkImageLayout			layout,
										  VkImageAspectFlags	aspectMask)

	:m_attachment	(attachment)
	,m_layout		(layout)
	,m_aspectMask	(aspectMask)
{
}

deUint32 AttachmentReference::getAttachment (void) const
{
	return m_attachment;
}

VkImageLayout AttachmentReference::getImageLayout (void) const
{
	return m_layout;
}

VkImageAspectFlags AttachmentReference::getAspectMask (void) const
{
	return m_aspectMask;
}

void AttachmentReference::setImageLayout (VkImageLayout layout)
{
	m_layout = layout;
}

Subpass::Subpass (VkPipelineBindPoint						pipelineBindPoint,
				  VkSubpassDescriptionFlags					flags,
				  const std::vector<AttachmentReference>&	inputAttachments,
				  const std::vector<AttachmentReference>&	colorAttachments,
				  const std::vector<AttachmentReference>&	resolveAttachments,
				  AttachmentReference						depthStencilAttachment,
				  const std::vector<deUint32>&				preserveAttachments,
				  bool										omitBlendState)

	:m_pipelineBindPoint		(pipelineBindPoint)
	,m_flags					(flags)
	,m_inputAttachments			(inputAttachments)
	,m_colorAttachments			(colorAttachments)
	,m_resolveAttachments		(resolveAttachments)
	,m_depthStencilAttachment	(depthStencilAttachment)
	,m_preserveAttachments		(preserveAttachments)
	,m_omitBlendState			(omitBlendState)
{
}

VkPipelineBindPoint Subpass::getPipelineBindPoint (void) const
{
	return m_pipelineBindPoint;
}

VkSubpassDescriptionFlags Subpass::getFlags (void) const
{
	return m_flags;
}

const std::vector<AttachmentReference>& Subpass::getInputAttachments (void) const
{
	return m_inputAttachments;
}

const std::vector<AttachmentReference>& Subpass::getColorAttachments (void) const
{
	return m_colorAttachments;
}

const std::vector<AttachmentReference>& Subpass::getResolveAttachments (void) const
{
	return m_resolveAttachments;
}

const AttachmentReference& Subpass::getDepthStencilAttachment (void) const
{
	return m_depthStencilAttachment;
}

const std::vector<deUint32>& Subpass::getPreserveAttachments (void) const
{
	return m_preserveAttachments;
}

bool Subpass::getOmitBlendState (void) const
{
	return m_omitBlendState;
}

SubpassDependency::SubpassDependency (deUint32				srcPass,
									  deUint32				dstPass,

									  VkPipelineStageFlags	srcStageMask,
									  VkPipelineStageFlags	dstStageMask,

									  VkAccessFlags			srcAccessMask,
									  VkAccessFlags			dstAccessMask,

									  VkDependencyFlags		flags)

	: m_srcPass			(srcPass)
	, m_dstPass			(dstPass)

	, m_srcStageMask	(srcStageMask)
	, m_dstStageMask	(dstStageMask)

	, m_srcAccessMask	(srcAccessMask)
	, m_dstAccessMask	(dstAccessMask)
	, m_flags			(flags)
{
}

deUint32 SubpassDependency::getSrcPass (void) const
{
	return m_srcPass;
}

deUint32 SubpassDependency::getDstPass (void) const
{
	return m_dstPass;
}

VkPipelineStageFlags SubpassDependency::getSrcStageMask (void) const
{
	return m_srcStageMask;
}

VkPipelineStageFlags SubpassDependency::getDstStageMask (void) const
{
	return m_dstStageMask;
}

VkAccessFlags SubpassDependency::getSrcAccessMask (void) const
{
	return m_srcAccessMask;
}

VkAccessFlags SubpassDependency::getDstAccessMask (void) const
{
	return m_dstAccessMask;
}

VkDependencyFlags SubpassDependency::getFlags (void) const
{
	return m_flags;
}

Attachment::Attachment (VkFormat				format,
						VkSampleCountFlagBits	samples,

						VkAttachmentLoadOp		loadOp,
						VkAttachmentStoreOp		storeOp,

						VkAttachmentLoadOp		stencilLoadOp,
						VkAttachmentStoreOp		stencilStoreOp,

						VkImageLayout			initialLayout,
						VkImageLayout			finalLayout)

	: m_format			(format)
	, m_samples			(samples)
	, m_loadOp			(loadOp)
	, m_storeOp			(storeOp)
	, m_stencilLoadOp	(stencilLoadOp)
	, m_stencilStoreOp	(stencilStoreOp)
	, m_initialLayout	(initialLayout)
	, m_finalLayout		(finalLayout)
{
}

VkFormat Attachment::getFormat (void) const
{
	return m_format;
}

VkSampleCountFlagBits Attachment::getSamples (void) const
{
	return m_samples;
}

VkAttachmentLoadOp Attachment::getLoadOp (void) const
{
	return m_loadOp;
}

VkAttachmentStoreOp Attachment::getStoreOp (void) const
{
	return m_storeOp;
}

VkAttachmentLoadOp Attachment::getStencilLoadOp (void) const
{
	return m_stencilLoadOp;
}

VkAttachmentStoreOp Attachment::getStencilStoreOp (void) const
{
	return m_stencilStoreOp;
}

VkImageLayout Attachment::getInitialLayout (void) const
{
	return m_initialLayout;
}

VkImageLayout Attachment::getFinalLayout (void) const
{
	return m_finalLayout;
}

RenderPass::RenderPass (const std::vector<Attachment>&						attachments,
						const std::vector<Subpass>&							subpasses,
						const std::vector<SubpassDependency>&				dependencies,
						const std::vector<VkInputAttachmentAspectReference>	inputAspects)

	: m_attachments		(attachments)
	, m_subpasses		(subpasses)
	, m_dependencies	(dependencies)
	, m_inputAspects	(inputAspects)
{
}

const std::vector<Attachment>& RenderPass::getAttachments (void) const
{
	return m_attachments;
}

const std::vector<Subpass>& RenderPass::getSubpasses (void) const
{
	return m_subpasses;
}

const std::vector<SubpassDependency>& RenderPass::getDependencies (void) const
{
	return m_dependencies;
}

const std::vector<VkInputAttachmentAspectReference>& RenderPass::getInputAspects (void) const
{
	return m_inputAspects;
}

template <typename AttachmentDesc>
AttachmentDesc createAttachmentDescription (const Attachment& attachment)
{
	const AttachmentDesc attachmentDescription	//  VkAttachmentDescription							||  VkAttachmentDescription2
	(
												//													||  VkStructureType					sType;
		DE_NULL,								//													||  const void*						pNext;
		0u,										//  VkAttachmentDescriptionFlags	flags;			||  VkAttachmentDescriptionFlags	flags;
		attachment.getFormat(),					//  VkFormat						format;			||  VkFormat						format;
		attachment.getSamples(),				//  VkSampleCountFlagBits			samples;		||  VkSampleCountFlagBits			samples;
		attachment.getLoadOp(),					//  VkAttachmentLoadOp				loadOp;			||  VkAttachmentLoadOp				loadOp;
		attachment.getStoreOp(),				//  VkAttachmentStoreOp				storeOp;		||  VkAttachmentStoreOp				storeOp;
		attachment.getStencilLoadOp(),			//  VkAttachmentLoadOp				stencilLoadOp;	||  VkAttachmentLoadOp				stencilLoadOp;
		attachment.getStencilStoreOp(),			//  VkAttachmentStoreOp				stencilStoreOp;	||  VkAttachmentStoreOp				stencilStoreOp;
		attachment.getInitialLayout(),			//  VkImageLayout					initialLayout;	||  VkImageLayout					initialLayout;
		attachment.getFinalLayout()				//  VkImageLayout					finalLayout;	||  VkImageLayout					finalLayout;
	);

	return attachmentDescription;
}

template <typename AttachmentRef>
AttachmentRef createAttachmentReference (const AttachmentReference& referenceInfo)
{
	const AttachmentRef	reference		//  VkAttachmentReference				||  VkAttachmentReference2
	(
										//										||  VkStructureType				sType;
		DE_NULL,						//										||  const void*					pNext;
		referenceInfo.getAttachment(),	//  deUint32				attachment;	||  deUint32					attachment;
		referenceInfo.getImageLayout(),	//  VkImageLayout			layout;		||  VkImageLayout				layout;
		referenceInfo.getAspectMask()	//										||  VkImageAspectFlags			aspectMask;
	);

	return reference;
}

template <typename SubpassDesc, typename AttachmentRef>
SubpassDesc createSubpassDescription (const Subpass&				subpass,
									  std::vector<AttachmentRef>*	attachmentReferenceLists,
									  std::vector<deUint32>*		preserveAttachmentReferences)
{
	std::vector<AttachmentRef>&	inputAttachmentReferences			= attachmentReferenceLists[0];
	std::vector<AttachmentRef>&	colorAttachmentReferences			= attachmentReferenceLists[1];
	std::vector<AttachmentRef>&	resolveAttachmentReferences			= attachmentReferenceLists[2];
	std::vector<AttachmentRef>&	depthStencilAttachmentReferences	= attachmentReferenceLists[3];

	for (size_t attachmentNdx = 0; attachmentNdx < subpass.getColorAttachments().size(); attachmentNdx++)
		colorAttachmentReferences.push_back(createAttachmentReference<AttachmentRef>(subpass.getColorAttachments()[attachmentNdx]));

	for (size_t attachmentNdx = 0; attachmentNdx < subpass.getInputAttachments().size(); attachmentNdx++)
		inputAttachmentReferences.push_back(createAttachmentReference<AttachmentRef>(subpass.getInputAttachments()[attachmentNdx]));

	for (size_t attachmentNdx = 0; attachmentNdx < subpass.getResolveAttachments().size(); attachmentNdx++)
		resolveAttachmentReferences.push_back(createAttachmentReference<AttachmentRef>(subpass.getResolveAttachments()[attachmentNdx]));

	depthStencilAttachmentReferences.push_back(createAttachmentReference<AttachmentRef>(subpass.getDepthStencilAttachment()));

	for (size_t attachmentNdx = 0; attachmentNdx < subpass.getPreserveAttachments().size(); attachmentNdx++)
		preserveAttachmentReferences->push_back(subpass.getPreserveAttachments()[attachmentNdx]);

	DE_ASSERT(resolveAttachmentReferences.empty() || colorAttachmentReferences.size() == resolveAttachmentReferences.size());

	{
		const SubpassDesc subpassDescription														//  VkSubpassDescription										||  VkSubpassDescription2
		(
																									//																||  VkStructureType						sType;
			DE_NULL,																				//																||  const void*							pNext;
			subpass.getFlags(),																		//  VkSubpassDescriptionFlags		flags;						||  VkSubpassDescriptionFlags			flags;
			subpass.getPipelineBindPoint(),															//  VkPipelineBindPoint				pipelineBindPoint;			||  VkPipelineBindPoint					pipelineBindPoint;
			0u,																						//																||  deUint32							viewMask;
			(deUint32)inputAttachmentReferences.size(),												//  deUint32						inputAttachmentCount;		||  deUint32							inputAttachmentCount;
			inputAttachmentReferences.empty() ? DE_NULL : &inputAttachmentReferences[0],			//  const VkAttachmentReference*	pInputAttachments;			||  const VkAttachmentReference2*		pInputAttachments;
			(deUint32)colorAttachmentReferences.size(),												//  deUint32						colorAttachmentCount;		||  deUint32							colorAttachmentCount;
			colorAttachmentReferences.empty() ? DE_NULL :  &colorAttachmentReferences[0],			//  const VkAttachmentReference*	pColorAttachments;			||  const VkAttachmentReference2*		pColorAttachments;
			resolveAttachmentReferences.empty() ? DE_NULL : &resolveAttachmentReferences[0],		//  const VkAttachmentReference*	pResolveAttachments;		||  const VkAttachmentReference2*		pResolveAttachments;
			&depthStencilAttachmentReferences[0],													//  const VkAttachmentReference*	pDepthStencilAttachment;	||  const VkAttachmentReference2*		pDepthStencilAttachment;
			(deUint32)preserveAttachmentReferences->size(),											//  deUint32						preserveAttachmentCount;	||  deUint32							preserveAttachmentCount;
			preserveAttachmentReferences->empty() ? DE_NULL : &(*preserveAttachmentReferences)[0]	//  const deUint32*					pPreserveAttachments;		||  const deUint32*						pPreserveAttachments;
		);

		return subpassDescription;
	}
}

VkMemoryBarrier2KHR createMemoryBarrierFromSubpassDependency(const SubpassDependency& dependencyInfo)
{
	return
	{
		VK_STRUCTURE_TYPE_MEMORY_BARRIER_2_KHR,												// VkStructureType				sType
		DE_NULL,																			// const void*					pNext
		static_cast<VkPipelineStageFlags2KHR>	(dependencyInfo.getSrcStageMask()),			// VkPipelineStageFlags2KHR		srcStageMask
		static_cast<VkAccessFlags2KHR>			(dependencyInfo.getSrcAccessMask()),		// VkAccessFlags2KHR			srcAccessMask
		static_cast<VkPipelineStageFlags2KHR>	(dependencyInfo.getDstStageMask()),			// VkPipelineStageFlags2KHR		dstStageMask
		static_cast<VkAccessFlags2KHR>			(dependencyInfo.getDstAccessMask())			// VkAccessFlags2KHR			dstAccessMask
	};
}

template <typename SubpassDep>
SubpassDep createSubpassDependency (const SubpassDependency& dependencyInfo, VkMemoryBarrier2KHR* memoryBarrierPtr = DE_NULL)
{
	VkPipelineStageFlags	srcStageMask	= dependencyInfo.getSrcStageMask();
	VkPipelineStageFlags	dstStageMask	= dependencyInfo.getDstStageMask();
	VkAccessFlags			srcAccessMask	= dependencyInfo.getSrcAccessMask();
	VkAccessFlags			dstAccessMask	= dependencyInfo.getDstAccessMask();

	// If an instance of VkMemoryBarrier2KHR is included in the pNext chain, srcStageMask,
	// dstStageMask, srcAccessMask and dstAccessMask parameters are ignored. The synchronization
	// and access scopes instead are defined by the parameters of VkMemoryBarrier2KHR.
	if (memoryBarrierPtr)
	{
		srcStageMask	= 0;
		dstStageMask	= 0;
		srcAccessMask	= 0;
		dstAccessMask	= 0;
	}

	return									//  VkSubpassDependency						||  VkSubpassDependency2
	{
		memoryBarrierPtr,					//											||	const void*					pNext
		dependencyInfo.getSrcPass(),		//  deUint32				srcSubpass		||	deUint32					srcSubpass
		dependencyInfo.getDstPass(),		//  deUint32				dstSubpass		||	deUint32					dstSubpass
		srcStageMask,						//  VkPipelineStageFlags	srcStageMask	||	VkPipelineStageFlags		srcStageMask
		dstStageMask,						//  VkPipelineStageFlags	dstStageMask	||	VkPipelineStageFlags		dstStageMask
		srcAccessMask,						//  VkAccessFlags			srcAccessMask	||	VkAccessFlags				srcAccessMask
		dstAccessMask,						//  VkAccessFlags			dstAccessMask	||	VkAccessFlags				dstAccessMask
		dependencyInfo.getFlags(),			//  VkDependencyFlags		dependencyFlags	||	VkDependencyFlags			dependencyFlags
		0u									//	deInt32					viewOffset		||	deInt32						viewOffset
	};
}

de::MovePtr<VkRenderPassInputAttachmentAspectCreateInfo> createRenderPassInputAttachmentAspectCreateInfo (const RenderPass& renderPassInfo)
{
	de::MovePtr<VkRenderPassInputAttachmentAspectCreateInfo> result (DE_NULL);

	if (!renderPassInfo.getInputAspects().empty())
	{
		const VkRenderPassInputAttachmentAspectCreateInfo	inputAspectCreateInfo	=
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_INPUT_ATTACHMENT_ASPECT_CREATE_INFO,
			DE_NULL,

			(deUint32)renderPassInfo.getInputAspects().size(),
			renderPassInfo.getInputAspects().data(),
		};

		result = de::MovePtr<VkRenderPassInputAttachmentAspectCreateInfo>(new VkRenderPassInputAttachmentAspectCreateInfo(inputAspectCreateInfo));
	}

	return result;
}

template<typename AttachmentDesc, typename AttachmentRef, typename SubpassDesc, typename SubpassDep, typename RenderPassCreateInfo>
Move<VkRenderPass> createRenderPass (const DeviceInterface&	vk,
									 VkDevice				device,
									 const RenderPass&		renderPassInfo,
									 SynchronizationType	synchronizationType)
{
	const size_t												perSubpassAttachmentReferenceLists = 4;
	std::vector<AttachmentDesc>									attachments;
	std::vector<SubpassDesc>									subpasses;
	std::vector<SubpassDep>										dependencies;
	std::vector<VkMemoryBarrier2KHR>							memoryBarriers;
	std::vector<std::vector<AttachmentRef> >					attachmentReferenceLists(renderPassInfo.getSubpasses().size() * perSubpassAttachmentReferenceLists);
	std::vector<std::vector<deUint32> >							preserveAttachments(renderPassInfo.getSubpasses().size());
	de::MovePtr<VkRenderPassInputAttachmentAspectCreateInfo>	inputAspectCreateInfo(createRenderPassInputAttachmentAspectCreateInfo(renderPassInfo));

	for (size_t attachmentNdx = 0; attachmentNdx < renderPassInfo.getAttachments().size(); attachmentNdx++)
		attachments.push_back(createAttachmentDescription<AttachmentDesc>(renderPassInfo.getAttachments()[attachmentNdx]));

	for (size_t subpassNdx = 0; subpassNdx < renderPassInfo.getSubpasses().size(); subpassNdx++)
		subpasses.push_back(createSubpassDescription<SubpassDesc>(renderPassInfo.getSubpasses()[subpassNdx], &(attachmentReferenceLists[subpassNdx * perSubpassAttachmentReferenceLists]), &preserveAttachments[subpassNdx]));

	if (synchronizationType == SYNCHRONIZATION_TYPE_SYNCHRONIZATION2)
	{
		// reserve space to avoid reallocation in vector that will invalidate pointers
		memoryBarriers.reserve(renderPassInfo.getDependencies().size());
		for (const auto& dependency : renderPassInfo.getDependencies())
		{
			memoryBarriers.push_back(createMemoryBarrierFromSubpassDependency(dependency));
			dependencies.push_back(createSubpassDependency<SubpassDep>(dependency, &memoryBarriers.back()));
		}
	}
	else
	{
		for (const auto& dependency : renderPassInfo.getDependencies())
			dependencies.push_back(createSubpassDependency<SubpassDep>(dependency));
	}

	const RenderPassCreateInfo	renderPassCreator				//  VkRenderPassCreateInfo								||  VkRenderPassCreateInfo2
	(
																//  VkStructureType					sType;				||  VkStructureType						sType;
		inputAspectCreateInfo.get(),							//  const void*						pNext;				||  const void*							pNext;
		(VkRenderPassCreateFlags)0u,							//  VkRenderPassCreateFlags			flags;				||  VkRenderPassCreateFlags				flags;
		(deUint32)attachments.size(),							//  deUint32						attachmentCount;	||  deUint32							attachmentCount;
		(attachments.empty() ? DE_NULL : &attachments[0]),		//  const VkAttachmentDescription*	pAttachments;		||  const VkAttachmentDescription2*		pAttachments;
		(deUint32)subpasses.size(),								//  deUint32						subpassCount;		||  deUint32							subpassCount;
		(subpasses.empty() ? DE_NULL : &subpasses[0]),			//  const VkSubpassDescription*		pSubpasses;			||  const VkSubpassDescription2*		pSubpasses;
		(deUint32)dependencies.size(),							//  deUint32						dependencyCount;	||  deUint32							dependencyCount;
		(dependencies.empty() ? DE_NULL : &dependencies[0]),	//  const VkSubpassDependency*		pDependencies;		||  const VkSubpassDependency2*			pDependencies;
		0u,														//														||  deUint32							correlatedViewMaskCount;
		DE_NULL													//														||  const deUint32*						pCorrelatedViewMasks;
	);

	return renderPassCreator.createRenderPass(vk, device);
}

Move<VkRenderPass> createRenderPass (const DeviceInterface&	vk,
									 VkDevice				device,
									 const RenderPass&		renderPassInfo,
									 RenderPassType			renderPassType,
									 SynchronizationType	synchronizationType)
{
	switch (renderPassType)
	{
		case RENDERPASS_TYPE_LEGACY:
			return createRenderPass<AttachmentDescription1,
									AttachmentReference1,
									SubpassDescription1,
									SubpassDependency1,
									RenderPassCreateInfo1>(vk, device, renderPassInfo, SYNCHRONIZATION_TYPE_LEGACY);
		case RENDERPASS_TYPE_RENDERPASS2:
			return createRenderPass<AttachmentDescription2,
									AttachmentReference2,
									SubpassDescription2,
									SubpassDependency2,
									RenderPassCreateInfo2>(vk, device, renderPassInfo, synchronizationType);
		default:
			TCU_THROW(InternalError, "Impossible");
	}
}

} // renderpass

} // vkt
