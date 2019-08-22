#ifndef _VKTMULTIVIEWRENDERPASSUTIL_HPP
#define _VKTMULTIVIEWRENDERPASSUTIL_HPP
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

#include "tcuDefs.hpp"
#include "vkRef.hpp"
#include "vkDefs.hpp"
#include "vkTypeUtil.hpp"

namespace vkt
{
namespace MultiView
{
using namespace vk;

class AttachmentDescription1 : public vk::VkAttachmentDescription
{
public:
	AttachmentDescription1	(const void*					pNext,
							 VkAttachmentDescriptionFlags	flags,
							 VkFormat						format,
							 VkSampleCountFlagBits			samples,
							 VkAttachmentLoadOp				loadOp,
							 VkAttachmentStoreOp			storeOp,
							 VkAttachmentLoadOp				stencilLoadOp,
							 VkAttachmentStoreOp			stencilStoreOp,
							 VkImageLayout					initialLayout,
							 VkImageLayout					finalLayout);
};

class AttachmentDescription2 : public vk::VkAttachmentDescription2
{
public:
	AttachmentDescription2	(const void*					pNext,
							 VkAttachmentDescriptionFlags	flags,
							 VkFormat						format,
							 VkSampleCountFlagBits			samples,
							 VkAttachmentLoadOp				loadOp,
							 VkAttachmentStoreOp			storeOp,
							 VkAttachmentLoadOp				stencilLoadOp,
							 VkAttachmentStoreOp			stencilStoreOp,
							 VkImageLayout					initialLayout,
							 VkImageLayout					finalLayout);
};

class AttachmentReference1 : public vk::VkAttachmentReference
{
public:
	AttachmentReference1	(const void*		pNext,
							 deUint32			attachment,
							 VkImageLayout		layout,
							 VkImageAspectFlags	aspectMask);
};

class AttachmentReference2 : public vk::VkAttachmentReference2
{
public:
	AttachmentReference2	(const void*		pNext,
							 deUint32			attachment,
							 VkImageLayout		layout,
							 VkImageAspectFlags	aspectMask);
};

class SubpassDescription1 : public vk::VkSubpassDescription
{
public:
	SubpassDescription1	(const void*						pNext,
						 VkSubpassDescriptionFlags			flags,
						 VkPipelineBindPoint				pipelineBindPoint,
						 deUint32							viewMask,
						 deUint32							inputAttachmentCount,
						 const VkAttachmentReference*		pInputAttachments,
						 deUint32							colorAttachmentCount,
						 const VkAttachmentReference*		pColorAttachments,
						 const VkAttachmentReference*		pResolveAttachments,
						 const VkAttachmentReference*		pDepthStencilAttachment,
						 deUint32							preserveAttachmentCount,
						 const deUint32*					pPreserveAttachments);
};

class SubpassDescription2 : public vk::VkSubpassDescription2
{
public:
	SubpassDescription2	(const void*						pNext,
						 VkSubpassDescriptionFlags			flags,
						 VkPipelineBindPoint				pipelineBindPoint,
						 deUint32							viewMask,
						 deUint32							inputAttachmentCount,
						 const VkAttachmentReference2*		pInputAttachments,
						 deUint32							colorAttachmentCount,
						 const VkAttachmentReference2*		pColorAttachments,
						 const VkAttachmentReference2*		pResolveAttachments,
						 const VkAttachmentReference2*		pDepthStencilAttachment,
						 deUint32							preserveAttachmentCount,
						 const deUint32*					pPreserveAttachments);
};

class SubpassDependency1 : public vk::VkSubpassDependency
{
public:
	SubpassDependency1	(const void*			pNext,
						 deUint32				srcSubpass,
						 deUint32				dstSubpass,
						 VkPipelineStageFlags	srcStageMask,
						 VkPipelineStageFlags	dstStageMask,
						 VkAccessFlags			srcAccessMask,
						 VkAccessFlags			dstAccessMask,
						 VkDependencyFlags		dependencyFlags,
						 deInt32				viewOffset);
};

class SubpassDependency2 : public vk::VkSubpassDependency2
{
public:
	SubpassDependency2	(const void*			pNext,
						 deUint32				srcSubpass,
						 deUint32				dstSubpass,
						 VkPipelineStageFlags	srcStageMask,
						 VkPipelineStageFlags	dstStageMask,
						 VkAccessFlags			srcAccessMask,
						 VkAccessFlags			dstAccessMask,
						 VkDependencyFlags		dependencyFlags,
						 deInt32				viewOffset);
};

class RenderPassCreateInfo1 : public VkRenderPassCreateInfo
{
public:
							RenderPassCreateInfo1	(const void*						pNext,
													 VkRenderPassCreateFlags			flags,
													 deUint32							attachmentCount,
													 const VkAttachmentDescription*		pAttachments,
													 deUint32							subpassCount,
													 const VkSubpassDescription*		pSubpasses,
													 deUint32							dependencyCount,
													 const VkSubpassDependency*			pDependencies,
													 deUint32							correlatedViewMaskCount,
													 const deUint32*					pCorrelatedViewMasks);

	Move<VkRenderPass>		createRenderPass		(const DeviceInterface& vk,
													 VkDevice device) const;
};

class RenderPassCreateInfo2 : public VkRenderPassCreateInfo2
{
public:
							RenderPassCreateInfo2	(const void*						pNext,
													 VkRenderPassCreateFlags			flags,
													 deUint32							attachmentCount,
													 const VkAttachmentDescription2*	pAttachments,
													 deUint32							subpassCount,
													 const VkSubpassDescription2*		pSubpasses,
													 deUint32							dependencyCount,
													 const VkSubpassDependency2*		pDependencies,
													 deUint32							correlatedViewMaskCount,
													 const deUint32*					pCorrelatedViewMasks);

	Move<VkRenderPass>		createRenderPass		(const DeviceInterface& vk,
													 VkDevice device) const;
};

class SubpassBeginInfo1
{
public:
						SubpassBeginInfo1	(const void*		pNext,
											 VkSubpassContents	contents);

	VkSubpassContents	contents;
};

class SubpassBeginInfo2 : public VkSubpassBeginInfo
{
public:
						SubpassBeginInfo2	(const void*		pNext,
											 VkSubpassContents	contents);
};

class SubpassEndInfo1
{
public:
						SubpassEndInfo1	(const void*	pNext);
};

class SubpassEndInfo2 : public VkSubpassEndInfo
{
public:
						SubpassEndInfo2	(const void*	pNext);
};

class RenderpassSubpass1
{
public:
	typedef SubpassBeginInfo1		SubpassBeginInfo;
	typedef SubpassEndInfo1			SubpassEndInfo;

	static void	cmdBeginRenderPass	(const DeviceInterface&			vk,
									 VkCommandBuffer				cmdBuffer,
									 const VkRenderPassBeginInfo*	pRenderPassBegin,
									 const SubpassBeginInfo*		pSubpassBeginInfo);

	static void	cmdNextSubpass		(const DeviceInterface&			vk,
									 VkCommandBuffer				cmdBuffer,
									 const SubpassBeginInfo*		pSubpassBeginInfo,
									 const SubpassEndInfo*			pSubpassEndInfo);

	static void	cmdEndRenderPass	(const DeviceInterface&			vk,
									 VkCommandBuffer				cmdBuffer,
									 const SubpassEndInfo*			pSubpassEndInfo);
};

class RenderpassSubpass2
{
public:
	typedef SubpassBeginInfo2		SubpassBeginInfo;
	typedef SubpassEndInfo2			SubpassEndInfo;

	static void	cmdBeginRenderPass	(const DeviceInterface&			vk,
									 VkCommandBuffer				cmdBuffer,
									 const VkRenderPassBeginInfo*	pRenderPassBegin,
									 const SubpassBeginInfo*		pSubpassBeginInfo);

	static void	cmdNextSubpass		(const DeviceInterface&			vk,
									 VkCommandBuffer				cmdBuffer,
									 const SubpassBeginInfo*		pSubpassBeginInfo,
									 const SubpassEndInfo*			pSubpassEndInfo);

	static void	cmdEndRenderPass	(const DeviceInterface&			vk,
									 VkCommandBuffer				cmdBuffer,
									 const SubpassEndInfo*			pSubpassEndInfo);
};

} // renderpass

} // vkt

#endif // _VKTMULTIVIEWRENDERPASSUTIL_HPP
