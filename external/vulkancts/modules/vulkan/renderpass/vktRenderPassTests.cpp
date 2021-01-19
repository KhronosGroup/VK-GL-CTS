/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Google Inc.
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
 * \brief RenderPass tests
 *//*--------------------------------------------------------------------*/

#include "vktRenderPassTests.hpp"
#include "vktRenderPassTestsUtil.hpp"

#include "vktRenderPassMultisampleTests.hpp"
#include "vktRenderPassMultisampleResolveTests.hpp"
#include "vktRenderPassSampleReadTests.hpp"
#include "vktRenderPassSparseRenderTargetTests.hpp"
#include "vktRenderPassSubpassDependencyTests.hpp"
#include "vktRenderPassUnusedAttachmentTests.hpp"
#include "vktRenderPassUnusedClearAttachmentTests.hpp"
#include "vktRenderPassDepthStencilResolveTests.hpp"
#include "vktRenderPassUnusedAttachmentSparseFillingTests.hpp"
#include "vktRenderPassFragmentDensityMapTests.hpp"
#include "vktRenderPassMultipleSubpassesMultipleCommandBuffersTests.hpp"

#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkDefs.hpp"
#include "vkDeviceUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkStrUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "tcuFloat.hpp"
#include "tcuFormatUtil.hpp"
#include "tcuMaybe.hpp"
#include "tcuResultCollector.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVectorUtil.hpp"

#include "deRandom.hpp"
#include "deSTLUtil.hpp"
#include "deSharedPtr.hpp"
#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"

#include <limits>
#include <set>
#include <string>
#include <vector>
#include <memory>

using namespace vk;

using tcu::BVec4;
using tcu::IVec2;
using tcu::IVec4;
using tcu::UVec2;
using tcu::UVec4;
using tcu::Vec2;
using tcu::Vec4;

using tcu::Maybe;
using tcu::just;
using tcu::nothing;

using tcu::ConstPixelBufferAccess;
using tcu::PixelBufferAccess;

using tcu::TestLog;

using de::UniquePtr;

using std::pair;
using std::set;
using std::string;
using std::vector;

namespace vkt
{
namespace
{
using namespace renderpass;

typedef vector<deUint8>	DepthValuesArray;

static const deUint8	DEPTH_VALUES[]	= { 0u, 255u, 1u };

enum AllocationKind
{
	ALLOCATION_KIND_SUBALLOCATED,
	ALLOCATION_KIND_DEDICATED,
};

struct TestConfigExternal
{
	TestConfigExternal (AllocationKind	allocationKind_,
						RenderPassType	renderPassType_)
	: allocationKind	(allocationKind_)
	, renderPassType	(renderPassType_)
	{
	}

	AllocationKind	allocationKind;
	RenderPassType	renderPassType;
};

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

enum BoolOp
{
	BOOLOP_AND,
	BOOLOP_OR,
	BOOLOP_EQ,
	BOOLOP_NEQ
};

const char* boolOpToString (BoolOp op)
{
	switch (op)
	{
		case BOOLOP_OR:
			return "||";

		case BOOLOP_AND:
			return "&&";

		case BOOLOP_EQ:
			return "==";

		case BOOLOP_NEQ:
			return "!=";

		default:
			DE_FATAL("Unknown boolean operation.");
			return DE_NULL;
	}
}

bool performBoolOp (BoolOp op, bool a, bool b)
{
	switch (op)
	{
		case BOOLOP_OR:
			return a || b;

		case BOOLOP_AND:
			return a && b;

		case BOOLOP_EQ:
			return a == b;

		case BOOLOP_NEQ:
			return a != b;

		default:
			DE_FATAL("Unknown boolean operation.");
			return false;
	}
}

BoolOp boolOpFromIndex (size_t index)
{
	const BoolOp ops[] =
	{
		BOOLOP_OR,
		BOOLOP_AND,
		BOOLOP_EQ,
		BOOLOP_NEQ
	};

	return ops[index % DE_LENGTH_OF_ARRAY(ops)];
}

static float requiredDepthEpsilon(VkFormat format)
{
	// Possible precision loss in the unorm depth pipeline means that we need to check depths
	// that go in and back out of the depth buffer with an epsilon rather than an exact match
	deUint32 unormBits = 0;

	switch (format)
	{
	case VK_FORMAT_D16_UNORM:
		unormBits = 16;
		break;
	case VK_FORMAT_X8_D24_UNORM_PACK32:
	case VK_FORMAT_D24_UNORM_S8_UINT:
		unormBits = 24;
		break;
	case VK_FORMAT_D32_SFLOAT:
	case VK_FORMAT_D32_SFLOAT_S8_UINT:
	default:
		unormBits = 0;
		break;
	}

	if (unormBits > 0)
		return 1.0f / (float)((1 << unormBits) - 1);

	return 0.0f; // Require exact match
}

static bool depthsEqual(float a, float b, float epsilon)
{
	return fabs(a - b) <= epsilon;
}

Move<VkFramebuffer> createFramebuffer (const DeviceInterface&	vk,
									   VkDevice					device,
									   VkFramebufferCreateFlags	pCreateInfo_flags,
									   VkRenderPass				pCreateInfo_renderPass,
									   deUint32					pCreateInfo_attachmentCount,
									   const VkImageView*		pCreateInfo_pAttachments,
									   deUint32					pCreateInfo_width,
									   deUint32					pCreateInfo_height,
									   deUint32					pCreateInfo_layers)
{
	const VkFramebufferCreateInfo pCreateInfo =
	{
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		DE_NULL,
		pCreateInfo_flags,
		pCreateInfo_renderPass,
		pCreateInfo_attachmentCount,
		pCreateInfo_pAttachments,
		pCreateInfo_width,
		pCreateInfo_height,
		pCreateInfo_layers,
	};
	return createFramebuffer(vk, device, &pCreateInfo);
}

Move<VkImage> createImage (const DeviceInterface&	vk,
						   VkDevice					device,
						   VkImageCreateFlags		pCreateInfo_flags,
						   VkImageType				pCreateInfo_imageType,
						   VkFormat					pCreateInfo_format,
						   VkExtent3D				pCreateInfo_extent,
						   deUint32					pCreateInfo_mipLevels,
						   deUint32					pCreateInfo_arrayLayers,
						   VkSampleCountFlagBits	pCreateInfo_samples,
						   VkImageTiling			pCreateInfo_tiling,
						   VkImageUsageFlags		pCreateInfo_usage,
						   VkSharingMode			pCreateInfo_sharingMode,
						   deUint32					pCreateInfo_queueFamilyCount,
						   const deUint32*			pCreateInfo_pQueueFamilyIndices,
						   VkImageLayout			pCreateInfo_initialLayout)
{
	const VkImageCreateInfo pCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		DE_NULL,
		pCreateInfo_flags,
		pCreateInfo_imageType,
		pCreateInfo_format,
		pCreateInfo_extent,
		pCreateInfo_mipLevels,
		pCreateInfo_arrayLayers,
		pCreateInfo_samples,
		pCreateInfo_tiling,
		pCreateInfo_usage,
		pCreateInfo_sharingMode,
		pCreateInfo_queueFamilyCount,
		pCreateInfo_pQueueFamilyIndices,
		pCreateInfo_initialLayout
	};
	return createImage(vk, device, &pCreateInfo);
}

void bindBufferMemory (const DeviceInterface& vk, VkDevice device, VkBuffer buffer, VkDeviceMemory mem, VkDeviceSize memOffset)
{
	VK_CHECK(vk.bindBufferMemory(device, buffer, mem, memOffset));
}

void bindImageMemory (const DeviceInterface& vk, VkDevice device, VkImage image, VkDeviceMemory mem, VkDeviceSize memOffset)
{
	VK_CHECK(vk.bindImageMemory(device, image, mem, memOffset));
}

Move<VkImageView> createImageView (const DeviceInterface&	vk,
									VkDevice				device,
									VkImageViewCreateFlags	pCreateInfo_flags,
									VkImage					pCreateInfo_image,
									VkImageViewType			pCreateInfo_viewType,
									VkFormat				pCreateInfo_format,
									VkComponentMapping		pCreateInfo_components,
									VkImageSubresourceRange	pCreateInfo_subresourceRange)
{
	const VkImageViewCreateInfo pCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		DE_NULL,
		pCreateInfo_flags,
		pCreateInfo_image,
		pCreateInfo_viewType,
		pCreateInfo_format,
		pCreateInfo_components,
		pCreateInfo_subresourceRange,
	};
	return createImageView(vk, device, &pCreateInfo);
}

Move<VkBuffer> createBuffer (const DeviceInterface&	vk,
							 VkDevice				device,
							 VkBufferCreateFlags	pCreateInfo_flags,
							 VkDeviceSize			pCreateInfo_size,
							 VkBufferUsageFlags		pCreateInfo_usage,
							 VkSharingMode			pCreateInfo_sharingMode,
							 deUint32				pCreateInfo_queueFamilyCount,
							 const deUint32*		pCreateInfo_pQueueFamilyIndices)
{
	const VkBufferCreateInfo pCreateInfo =
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		DE_NULL,
		pCreateInfo_flags,
		pCreateInfo_size,
		pCreateInfo_usage,
		pCreateInfo_sharingMode,
		pCreateInfo_queueFamilyCount,
		pCreateInfo_pQueueFamilyIndices,
	};
	return createBuffer(vk, device, &pCreateInfo);
}

VkRenderPassBeginInfo createRenderPassBeginInfo (VkRenderPass			pRenderPassBegin_renderPass,
												 VkFramebuffer			pRenderPassBegin_framebuffer,
												 VkRect2D				pRenderPassBegin_renderArea,
												 deUint32				pRenderPassBegin_clearValueCount,
												 const VkClearValue*	pRenderPassBegin_pAttachmentClearValues)
{
	const VkRenderPassBeginInfo renderPassBeginInfo =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		DE_NULL,
		pRenderPassBegin_renderPass,
		pRenderPassBegin_framebuffer,
		pRenderPassBegin_renderArea,
		pRenderPassBegin_clearValueCount,
		pRenderPassBegin_pAttachmentClearValues,
	};

	return renderPassBeginInfo;
}

void beginCommandBuffer (const DeviceInterface&			vk,
						 VkCommandBuffer				cmdBuffer,
						 VkCommandBufferUsageFlags		pBeginInfo_flags,
						 VkRenderPass					pInheritanceInfo_renderPass,
						 deUint32						pInheritanceInfo_subpass,
						 VkFramebuffer					pInheritanceInfo_framebuffer,
						 VkBool32						pInheritanceInfo_occlusionQueryEnable,
						 VkQueryControlFlags			pInheritanceInfo_queryFlags,
						 VkQueryPipelineStatisticFlags	pInheritanceInfo_pipelineStatistics)
{
	const VkCommandBufferInheritanceInfo pInheritanceInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
		DE_NULL,
		pInheritanceInfo_renderPass,
		pInheritanceInfo_subpass,
		pInheritanceInfo_framebuffer,
		pInheritanceInfo_occlusionQueryEnable,
		pInheritanceInfo_queryFlags,
		pInheritanceInfo_pipelineStatistics,
	};
	const VkCommandBufferBeginInfo pBeginInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		DE_NULL,
		pBeginInfo_flags,
		&pInheritanceInfo,
	};
	VK_CHECK(vk.beginCommandBuffer(cmdBuffer, &pBeginInfo));
}

void queueSubmit (const DeviceInterface& vk, VkQueue queue, deUint32 cmdBufferCount, const VkCommandBuffer* pCmdBuffers, VkFence fence)
{
	const VkSubmitInfo submitInfo =
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,
		DE_NULL,
		0u,								// waitSemaphoreCount
		(const VkSemaphore*)DE_NULL,	// pWaitSemaphores
		(const VkPipelineStageFlags*)DE_NULL,
		cmdBufferCount,					// commandBufferCount
		pCmdBuffers,
		0u,								// signalSemaphoreCount
		(const VkSemaphore*)DE_NULL,	// pSignalSemaphores
	};
	VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo, fence));
}

void waitForFences (const DeviceInterface& vk, VkDevice device, deUint32 fenceCount, const VkFence* pFences, VkBool32 waitAll, deUint64 timeout)
{
	VK_CHECK(vk.waitForFences(device, fenceCount, pFences, waitAll, timeout));
}

VkImageAspectFlags getImageAspectFlags (VkFormat vkFormat)
{
	const tcu::TextureFormat format = mapVkFormat(vkFormat);

	DE_STATIC_ASSERT(tcu::TextureFormat::CHANNELORDER_LAST == 21);

	switch (format.order)
	{
		case tcu::TextureFormat::DS:
			return VK_IMAGE_ASPECT_STENCIL_BIT | VK_IMAGE_ASPECT_DEPTH_BIT;

		case tcu::TextureFormat::D:
			return VK_IMAGE_ASPECT_DEPTH_BIT;

		case tcu::TextureFormat::S:
			return VK_IMAGE_ASPECT_STENCIL_BIT;

		default:
			return VK_IMAGE_ASPECT_COLOR_BIT;
	}
}

VkAccessFlags getAllMemoryReadFlags (void)
{
	return VK_ACCESS_TRANSFER_READ_BIT
		   | VK_ACCESS_UNIFORM_READ_BIT
		   | VK_ACCESS_HOST_READ_BIT
		   | VK_ACCESS_INDEX_READ_BIT
		   | VK_ACCESS_SHADER_READ_BIT
		   | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
		   | VK_ACCESS_INDIRECT_COMMAND_READ_BIT
		   | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
		   | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT
		   | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
}

VkAccessFlags getAllMemoryWriteFlags (void)
{
	return VK_ACCESS_TRANSFER_WRITE_BIT
		   | VK_ACCESS_HOST_WRITE_BIT
		   | VK_ACCESS_SHADER_WRITE_BIT
		   | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
		   | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
}

VkAccessFlags getMemoryFlagsForLayout (const VkImageLayout layout)
{
	switch (layout)
	{
		case VK_IMAGE_LAYOUT_GENERAL:										return getAllMemoryReadFlags() | getAllMemoryWriteFlags();
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:						return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:				return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:				return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:						return VK_ACCESS_SHADER_READ_BIT;
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:							return VK_ACCESS_TRANSFER_READ_BIT;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:							return VK_ACCESS_TRANSFER_WRITE_BIT;
		case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:	return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
		case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:	return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
		default:
			return (VkAccessFlags)0;
	}
}

VkPipelineStageFlags getAllPipelineStageFlags (void)
{
	/* All relevant flags for a pipeline containing VS+PS. */
	return VK_PIPELINE_STAGE_TRANSFER_BIT
		   | VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
		   | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT
		   | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT
		   | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
		   | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
		   | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
		   | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT
		   | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
		   | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
		   | VK_PIPELINE_STAGE_HOST_BIT;
}

class AttachmentReference
{
public:
						AttachmentReference		(deUint32			attachment,
												 VkImageLayout		layout,
												 VkImageAspectFlags	aspectMask = static_cast<VkImageAspectFlags>(0u))
		: m_attachment	(attachment)
		, m_layout		(layout)
		, m_aspectMask	(aspectMask)
	{
	}

	deUint32			getAttachment			(void) const { return m_attachment;	}
	VkImageLayout		getImageLayout			(void) const { return m_layout;		}
	VkImageAspectFlags	getAspectMask			(void) const { return m_aspectMask;	}
	void				setImageLayout			(VkImageLayout layout) { m_layout = layout;	}

private:
	deUint32			m_attachment;
	VkImageLayout		m_layout;
	VkImageAspectFlags	m_aspectMask;
};

class Subpass
{
public:
										Subpass						(VkPipelineBindPoint				pipelineBindPoint,
																	 VkSubpassDescriptionFlags			flags,
																	 const vector<AttachmentReference>&	inputAttachments,
																	 const vector<AttachmentReference>&	colorAttachments,
																	 const vector<AttachmentReference>&	resolveAttachments,
																	 AttachmentReference				depthStencilAttachment,
																	 const vector<deUint32>&			preserveAttachments,
																	 bool								omitBlendState = false)
		: m_pipelineBindPoint		(pipelineBindPoint)
		, m_flags					(flags)
		, m_inputAttachments		(inputAttachments)
		, m_colorAttachments		(colorAttachments)
		, m_resolveAttachments		(resolveAttachments)
		, m_depthStencilAttachment	(depthStencilAttachment)
		, m_preserveAttachments		(preserveAttachments)
		, m_omitBlendState			(omitBlendState)
	{
	}

	VkPipelineBindPoint					getPipelineBindPoint		(void) const { return m_pipelineBindPoint;		}
	VkSubpassDescriptionFlags			getFlags					(void) const { return m_flags;					}
	const vector<AttachmentReference>&	getInputAttachments			(void) const { return m_inputAttachments;		}
	const vector<AttachmentReference>&	getColorAttachments			(void) const { return m_colorAttachments;		}
	const vector<AttachmentReference>&	getResolveAttachments		(void) const { return m_resolveAttachments;		}
	const AttachmentReference&			getDepthStencilAttachment	(void) const { return m_depthStencilAttachment;	}
	const vector<deUint32>&				getPreserveAttachments		(void) const { return m_preserveAttachments;	}
	bool								getOmitBlendState			(void) const { return m_omitBlendState;			}

private:
	VkPipelineBindPoint					m_pipelineBindPoint;
	VkSubpassDescriptionFlags			m_flags;

	vector<AttachmentReference>			m_inputAttachments;
	vector<AttachmentReference>			m_colorAttachments;
	vector<AttachmentReference>			m_resolveAttachments;
	AttachmentReference					m_depthStencilAttachment;

	vector<deUint32>					m_preserveAttachments;
	bool								m_omitBlendState;
};

class SubpassDependency
{
public:
							SubpassDependency	(deUint32				srcPass,
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

	deUint32				getSrcPass			(void) const { return m_srcPass;		}
	deUint32				getDstPass			(void) const { return m_dstPass;		}

	VkPipelineStageFlags	getSrcStageMask		(void) const { return m_srcStageMask;	}
	VkPipelineStageFlags	getDstStageMask		(void) const { return m_dstStageMask;	}

	VkAccessFlags			getSrcAccessMask	(void) const { return m_srcAccessMask;	}
	VkAccessFlags			getDstAccessMask	(void) const { return m_dstAccessMask;	}

	VkDependencyFlags		getFlags			(void) const { return m_flags;		}

	void					setSrcAccessMask	(const VkAccessFlags& flags) { m_srcAccessMask = flags; }
	void					setDstAccessMask	(const VkAccessFlags& flags) { m_dstAccessMask = flags; }

private:
	deUint32				m_srcPass;
	deUint32				m_dstPass;

	VkPipelineStageFlags	m_srcStageMask;
	VkPipelineStageFlags	m_dstStageMask;

	VkAccessFlags			m_srcAccessMask;
	VkAccessFlags			m_dstAccessMask;
	VkDependencyFlags		m_flags;
};

class Attachment
{
public:
							Attachment			(VkFormat				format,
												 VkSampleCountFlagBits	samples,

												 VkAttachmentLoadOp		loadOp,
												 VkAttachmentStoreOp	storeOp,

												 VkAttachmentLoadOp		stencilLoadOp,
												 VkAttachmentStoreOp	stencilStoreOp,

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

	VkFormat				getFormat			(void) const { return m_format;			}
	VkSampleCountFlagBits	getSamples			(void) const { return m_samples;		}

	VkAttachmentLoadOp		getLoadOp			(void) const { return m_loadOp;			}
	VkAttachmentStoreOp		getStoreOp			(void) const { return m_storeOp;		}


	VkAttachmentLoadOp		getStencilLoadOp	(void) const { return m_stencilLoadOp;	}
	VkAttachmentStoreOp		getStencilStoreOp	(void) const { return m_stencilStoreOp;	}

	VkImageLayout			getInitialLayout	(void) const { return m_initialLayout;	}
	VkImageLayout			getFinalLayout		(void) const { return m_finalLayout;	}

private:
	VkFormat				m_format;
	VkSampleCountFlagBits	m_samples;

	VkAttachmentLoadOp		m_loadOp;
	VkAttachmentStoreOp		m_storeOp;

	VkAttachmentLoadOp		m_stencilLoadOp;
	VkAttachmentStoreOp		m_stencilStoreOp;

	VkImageLayout			m_initialLayout;
	VkImageLayout			m_finalLayout;
};

class RenderPass
{
public:
														RenderPass		(const vector<Attachment>&						attachments,
																		 const vector<Subpass>&							subpasses,
																		 const vector<SubpassDependency>&				dependencies,
																		 const vector<VkInputAttachmentAspectReference>	inputAspects = vector<VkInputAttachmentAspectReference>())
		: m_attachments		(attachments)
		, m_subpasses		(subpasses)
		, m_dependencies	(dependencies)
		, m_inputAspects	(inputAspects)
	{
	}

	const vector<Attachment>&							getAttachments	(void) const { return m_attachments;	}
	const vector<Subpass>&								getSubpasses	(void) const { return m_subpasses;		}
	const vector<SubpassDependency>&					getDependencies	(void) const { return m_dependencies;	}
	const vector<VkInputAttachmentAspectReference>&		getInputAspects	(void) const { return m_inputAspects;	}

private:
	const vector<Attachment>							m_attachments;
	const vector<Subpass>								m_subpasses;
	const vector<SubpassDependency>						m_dependencies;
	const vector<VkInputAttachmentAspectReference>		m_inputAspects;
};

struct TestConfig
{
	enum RenderTypes
	{
		RENDERTYPES_NONE	= 0,
		RENDERTYPES_CLEAR	= (1<<1),
		RENDERTYPES_DRAW	= (1<<2)
	};

	enum CommandBufferTypes
	{
		COMMANDBUFFERTYPES_INLINE		= (1<<0),
		COMMANDBUFFERTYPES_SECONDARY	= (1<<1)
	};

	enum ImageMemory
	{
		IMAGEMEMORY_STRICT		= (1<<0),
		IMAGEMEMORY_LAZY		= (1<<1)
	};

						TestConfig (const RenderPass&			renderPass_,
									RenderTypes					renderTypes_,
									CommandBufferTypes			commandBufferTypes_,
									ImageMemory					imageMemory_,
									const UVec2&				targetSize_,
									const UVec2&				renderPos_,
									const UVec2&				renderSize_,
									deBool						useFormatCompCount_,
									deUint32					seed_,
									deUint32					drawStartNdx_,
									AllocationKind				allocationKind_,
									RenderPassType				renderPassType_,
									vector<DeviceCoreFeature>	requiredFeatures_ = vector<DeviceCoreFeature>())
		: renderPass			(renderPass_)
		, renderTypes			(renderTypes_)
		, commandBufferTypes	(commandBufferTypes_)
		, imageMemory			(imageMemory_)
		, targetSize			(targetSize_)
		, renderPos				(renderPos_)
		, renderSize			(renderSize_)
		, useFormatCompCount	(useFormatCompCount_)
		, seed					(seed_)
		, drawStartNdx			(drawStartNdx_)
		, allocationKind		(allocationKind_)
		, renderPassType		(renderPassType_)
		, requiredFeatures		(requiredFeatures_)
	{
		DepthValuesArray	shuffledDepthValues	(&DEPTH_VALUES[0], &DEPTH_VALUES[DE_LENGTH_OF_ARRAY(DEPTH_VALUES)]);
		de::Random			rng					(seed + 1);

		rng.shuffle(shuffledDepthValues.begin(), shuffledDepthValues.end());

		depthValues.push_back(shuffledDepthValues[0]);
		depthValues.push_back(shuffledDepthValues[1]);
	}

	RenderPass					renderPass;
	RenderTypes					renderTypes;
	CommandBufferTypes			commandBufferTypes;
	ImageMemory					imageMemory;
	UVec2						targetSize;
	UVec2						renderPos;
	UVec2						renderSize;
	deBool						useFormatCompCount;
	deUint32					seed;
	deUint32					drawStartNdx;
	AllocationKind				allocationKind;
	RenderPassType				renderPassType;
	vector<DeviceCoreFeature>	requiredFeatures;
	DepthValuesArray			depthValues;
};

TestConfig::RenderTypes operator| (TestConfig::RenderTypes a, TestConfig::RenderTypes b)
{
	return (TestConfig::RenderTypes)(((deUint32)a) | ((deUint32)b));
}

TestConfig::CommandBufferTypes operator| (TestConfig::CommandBufferTypes a, TestConfig::CommandBufferTypes b)
{
	return (TestConfig::CommandBufferTypes)(((deUint32)a) | ((deUint32)b));
}

TestConfig::ImageMemory operator| (TestConfig::ImageMemory a, TestConfig::ImageMemory b)
{
	return (TestConfig::ImageMemory)(((deUint32)a) | ((deUint32)b));
}

void checkSupport (Context& context, TestConfig config)
{
	for (size_t featureNdx = 0; featureNdx < config.requiredFeatures.size(); featureNdx++)
		context.requireDeviceCoreFeature(config.requiredFeatures[featureNdx]);
}

void logRenderPassInfo (TestLog&			log,
						const RenderPass&	renderPass)
{
	const bool					useExternalInputAspect	= !renderPass.getInputAspects().empty();
	const tcu::ScopedLogSection	section					(log, "RenderPass", "RenderPass");

	{
		const tcu::ScopedLogSection	attachmentsSection	(log, "Attachments", "Attachments");
		const vector<Attachment>&	attachments			= renderPass.getAttachments();

		for (size_t attachmentNdx = 0; attachmentNdx < attachments.size(); attachmentNdx++)
		{
			const tcu::ScopedLogSection	attachmentSection	(log, "Attachment" + de::toString(attachmentNdx), "Attachment " + de::toString(attachmentNdx));
			const Attachment&			attachment			= attachments[attachmentNdx];

			log << TestLog::Message << "Format: " << attachment.getFormat() << TestLog::EndMessage;
			log << TestLog::Message << "Samples: " << attachment.getSamples() << TestLog::EndMessage;

			log << TestLog::Message << "LoadOp: " << attachment.getLoadOp() << TestLog::EndMessage;
			log << TestLog::Message << "StoreOp: " << attachment.getStoreOp() << TestLog::EndMessage;

			log << TestLog::Message << "StencilLoadOp: " << attachment.getStencilLoadOp() << TestLog::EndMessage;
			log << TestLog::Message << "StencilStoreOp: " << attachment.getStencilStoreOp() << TestLog::EndMessage;

			log << TestLog::Message << "InitialLayout: " << attachment.getInitialLayout() << TestLog::EndMessage;
			log << TestLog::Message << "FinalLayout: " << attachment.getFinalLayout() << TestLog::EndMessage;
		}
	}

	if (useExternalInputAspect)
	{
		const tcu::ScopedLogSection	inputAspectSection	(log, "InputAspects", "InputAspects");

		for (size_t aspectNdx = 0; aspectNdx < renderPass.getInputAspects().size(); aspectNdx++)
		{
			const VkInputAttachmentAspectReference&	inputAspect	(renderPass.getInputAspects()[aspectNdx]);

			log << TestLog::Message << "Subpass: " << inputAspect.subpass << TestLog::EndMessage;
			log << TestLog::Message << "InputAttachmentIndex: " << inputAspect.inputAttachmentIndex << TestLog::EndMessage;
			log << TestLog::Message << "AspectFlags: " << getImageAspectFlagsStr(inputAspect.aspectMask) << TestLog::EndMessage;
		}
	}

	{
		const tcu::ScopedLogSection	subpassesSection	(log, "Subpasses", "Subpasses");
		const vector<Subpass>&		subpasses			= renderPass.getSubpasses();

		for (size_t subpassNdx = 0; subpassNdx < subpasses.size(); subpassNdx++)
		{
			const tcu::ScopedLogSection			subpassSection		(log, "Subpass" + de::toString(subpassNdx), "Subpass " + de::toString(subpassNdx));
			const Subpass&						subpass				= subpasses[subpassNdx];

			const vector<AttachmentReference>&	inputAttachments	= subpass.getInputAttachments();
			const vector<AttachmentReference>&	colorAttachments	= subpass.getColorAttachments();
			const vector<AttachmentReference>&	resolveAttachments	= subpass.getResolveAttachments();
			const vector<deUint32>&				preserveAttachments	= subpass.getPreserveAttachments();

			if (!inputAttachments.empty())
			{
				const tcu::ScopedLogSection	inputAttachmentsSection	(log, "Inputs", "Inputs");

				for (size_t inputNdx = 0; inputNdx < inputAttachments.size(); inputNdx++)
				{
					const tcu::ScopedLogSection	inputAttachmentSection	(log, "Input" + de::toString(inputNdx), "Input " + de::toString(inputNdx));
					const AttachmentReference&	inputAttachment			= inputAttachments[inputNdx];

					log << TestLog::Message << "Attachment: " << inputAttachment.getAttachment() << TestLog::EndMessage;
					log << TestLog::Message << "Layout: " << inputAttachment.getImageLayout() << TestLog::EndMessage;
					if (!useExternalInputAspect)
						log << TestLog::Message << "AspectMask: " << inputAttachment.getAspectMask() << TestLog::EndMessage;
				}
			}

			if (subpass.getDepthStencilAttachment().getAttachment() != VK_ATTACHMENT_UNUSED)
			{
				const tcu::ScopedLogSection	depthStencilAttachmentSection	(log, "DepthStencil", "DepthStencil");
				const AttachmentReference&	depthStencilAttachment			= subpass.getDepthStencilAttachment();

				log << TestLog::Message << "Attachment: " << depthStencilAttachment.getAttachment() << TestLog::EndMessage;
				log << TestLog::Message << "Layout: " << depthStencilAttachment.getImageLayout() << TestLog::EndMessage;
			}

			if (!colorAttachments.empty())
			{
				const tcu::ScopedLogSection	colorAttachmentsSection	(log, "Colors", "Colors");

				for (size_t colorNdx = 0; colorNdx < colorAttachments.size(); colorNdx++)
				{
					const tcu::ScopedLogSection	colorAttachmentSection	(log, "Color" + de::toString(colorNdx), "Color " + de::toString(colorNdx));
					const AttachmentReference&	colorAttachment			= colorAttachments[colorNdx];

					log << TestLog::Message << "Attachment: " << colorAttachment.getAttachment() << TestLog::EndMessage;
					log << TestLog::Message << "Layout: " << colorAttachment.getImageLayout() << TestLog::EndMessage;
				}
			}

			if (!resolveAttachments.empty())
			{
				const tcu::ScopedLogSection	resolveAttachmentsSection	(log, "Resolves", "Resolves");

				for (size_t resolveNdx = 0; resolveNdx < resolveAttachments.size(); resolveNdx++)
				{
					const tcu::ScopedLogSection	resolveAttachmentSection	(log, "Resolve" + de::toString(resolveNdx), "Resolve " + de::toString(resolveNdx));
					const AttachmentReference&	resolveAttachment			= resolveAttachments[resolveNdx];

					log << TestLog::Message << "Attachment: " << resolveAttachment.getAttachment() << TestLog::EndMessage;
					log << TestLog::Message << "Layout: " << resolveAttachment.getImageLayout() << TestLog::EndMessage;
				}
			}

			if (!preserveAttachments.empty())
			{
				const tcu::ScopedLogSection	preserveAttachmentsSection	(log, "Preserves", "Preserves");

				for (size_t preserveNdx = 0; preserveNdx < preserveAttachments.size(); preserveNdx++)
				{
					const tcu::ScopedLogSection	preserveAttachmentSection	(log, "Preserve" + de::toString(preserveNdx), "Preserve " + de::toString(preserveNdx));
					const deUint32				preserveAttachment			= preserveAttachments[preserveNdx];

					log << TestLog::Message << "Attachment: " << preserveAttachment << TestLog::EndMessage;
				}
			}
		}

	}

	if (!renderPass.getDependencies().empty())
	{
		const tcu::ScopedLogSection	dependenciesSection	(log, "Dependencies", "Dependencies");

		for (size_t depNdx = 0; depNdx < renderPass.getDependencies().size(); depNdx++)
		{
			const tcu::ScopedLogSection	dependencySection	(log, "Dependency" + de::toString(depNdx), "Dependency " + de::toString(depNdx));
			const SubpassDependency&	dep					= renderPass.getDependencies()[depNdx];

			log << TestLog::Message << "Source: " << dep.getSrcPass() << TestLog::EndMessage;
			log << TestLog::Message << "Destination: " << dep.getDstPass() << TestLog::EndMessage;

			log << TestLog::Message << "Source Stage Mask: " << dep.getSrcStageMask() << TestLog::EndMessage;
			log << TestLog::Message << "Destination Stage Mask: " << dep.getDstStageMask() << TestLog::EndMessage;

			log << TestLog::Message << "Input Mask: " << dep.getDstAccessMask() << TestLog::EndMessage;
			log << TestLog::Message << "Output Mask: " << dep.getSrcAccessMask() << TestLog::EndMessage;
			log << TestLog::Message << "Dependency Flags: " << getDependencyFlagsStr(dep.getFlags()) << TestLog::EndMessage;
		}
	}
}

std::string clearColorToString (VkFormat vkFormat, VkClearColorValue value, deBool useFormatCompCount)
{
	const tcu::TextureFormat		format			= mapVkFormat(vkFormat);
	const tcu::TextureChannelClass	channelClass	= tcu::getTextureChannelClass(format.type);
	const tcu::BVec4				channelMask		= tcu::getTextureFormatChannelMask(format);
	const deUint32					componentCount	= (useFormatCompCount ? (deUint32)tcu::getNumUsedChannels(format.order) : 4);

	std::ostringstream				stream;

	stream << "(";

	switch (channelClass)
	{
		case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
			for (deUint32 i = 0; i < componentCount; i++)
			{
				if (i > 0)
					stream << ", ";

				if (channelMask[i])
					stream << value.int32[i];
				else
					stream << "Undef";
			}
			break;

		case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
			for (deUint32 i = 0; i < componentCount; i++)
			{
				if (i > 0)
					stream << ", ";

				if (channelMask[i])
					stream << value.uint32[i];
				else
					stream << "Undef";
			}
			break;

		case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
		case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
		case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
			for (deUint32 i = 0; i < componentCount; i++)
			{
				if (i > 0)
					stream << ", ";

				if (channelMask[i])
					stream << value.float32[i];
				else
					stream << "Undef";
			}
			break;

		default:
			DE_FATAL("Unknown channel class");
	}

	stream << ")";

	return stream.str();
}

std::string clearValueToString (VkFormat vkFormat, VkClearValue value, deBool useFormatCompCount)
{
	const tcu::TextureFormat	format	= mapVkFormat(vkFormat);

	if (tcu::hasStencilComponent(format.order) || tcu::hasDepthComponent(format.order))
	{
		std::ostringstream stream;

		stream << "(";

		if (tcu::hasStencilComponent(format.order))
			stream << "stencil: " << value.depthStencil.stencil;

		if (tcu::hasStencilComponent(format.order) && tcu::hasDepthComponent(format.order))
			stream << ", ";

		if (tcu::hasDepthComponent(format.order))
			stream << "depth: " << value.depthStencil.depth;

		stream << ")";

		return stream.str();
	}
	else
		return clearColorToString(vkFormat, value.color, useFormatCompCount);
}

VkClearColorValue randomColorClearValue (const Attachment& attachment, de::Random& rng, deBool useFormatCompCount)
{
	const float						clearNan		= tcu::Float32::nan().asFloat();
	const tcu::TextureFormat		format			= mapVkFormat(attachment.getFormat());
	const tcu::TextureChannelClass	channelClass	= tcu::getTextureChannelClass(format.type);
	const tcu::BVec4				channelMask		= tcu::getTextureFormatChannelMask(format);
	const deUint32					componentCount	= (useFormatCompCount ? (deUint32)tcu::getNumUsedChannels(format.order) : 4);
	VkClearColorValue				clearColor;

	switch (channelClass)
	{
		case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
		{
			for (deUint32 ndx = 0; ndx < componentCount; ndx++)
			{
				if (!channelMask[ndx])
					clearColor.int32[ndx] = std::numeric_limits<deInt32>::min();
				else
					clearColor.uint32[ndx] = rng.getBool() ? 1u : 0u;
			}
			break;
		}

		case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
		{
			for (deUint32 ndx = 0; ndx < componentCount; ndx++)
			{
				if (!channelMask[ndx])
					clearColor.uint32[ndx] = std::numeric_limits<deUint32>::max();
				else
					clearColor.uint32[ndx] = rng.getBool() ? 1u : 0u;
			}
			break;
		}

		case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
		case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
		case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
		{
			for (deUint32 ndx = 0; ndx < componentCount; ndx++)
			{
				if (!channelMask[ndx])
					clearColor.float32[ndx] = clearNan;
				else
					clearColor.float32[ndx] = rng.getBool() ? 1.0f : 0.0f;
			}
			break;
		}

		default:
			DE_FATAL("Unknown channel class");
	}

	return clearColor;
}

template <typename AttachmentDesc>
AttachmentDesc createAttachmentDescription (const Attachment& attachment)
{
	const AttachmentDesc	attachmentDescription	//  VkAttachmentDescription										||  VkAttachmentDescription2KHR
	(
													//																||  VkStructureType						sType;
		DE_NULL,									//																||  const void*							pNext;
		0u,											//  VkAttachmentDescriptionFlags	flags;						||  VkAttachmentDescriptionFlags		flags;
		attachment.getFormat(),						//  VkFormat						format;						||  VkFormat							format;
		attachment.getSamples(),					//  VkSampleCountFlagBits			samples;					||  VkSampleCountFlagBits				samples;
		attachment.getLoadOp(),						//  VkAttachmentLoadOp				loadOp;						||  VkAttachmentLoadOp					loadOp;
		attachment.getStoreOp(),					//  VkAttachmentStoreOp				storeOp;					||  VkAttachmentStoreOp					storeOp;
		attachment.getStencilLoadOp(),				//  VkAttachmentLoadOp				stencilLoadOp;				||  VkAttachmentLoadOp					stencilLoadOp;
		attachment.getStencilStoreOp(),				//  VkAttachmentStoreOp				stencilStoreOp;				||  VkAttachmentStoreOp					stencilStoreOp;
		attachment.getInitialLayout(),				//  VkImageLayout					initialLayout;				||  VkImageLayout						initialLayout;
		attachment.getFinalLayout()					//  VkImageLayout					finalLayout;				||  VkImageLayout						finalLayout;
	);

	return attachmentDescription;
}

template <typename AttachmentRef>
AttachmentRef createAttachmentReference (const AttachmentReference& referenceInfo)
{
	const AttachmentRef	reference					//  VkAttachmentReference										||  VkAttachmentReference2KHR
	(
													//																||  VkStructureType						sType;
		DE_NULL,									//																||  const void*							pNext;
		referenceInfo.getAttachment(),				//  deUint32						attachment;					||  deUint32							attachment;
		referenceInfo.getImageLayout(),				//  VkImageLayout					layout;						||  VkImageLayout						layout;
		referenceInfo.getAspectMask()				//																||  VkImageAspectFlags					aspectMask;
	);

	return reference;
}

template <typename SubpassDesc, typename AttachmentRef>
SubpassDesc createSubpassDescription (const Subpass&			subpass,
									  vector<AttachmentRef>*	attachmentReferenceLists,
									  vector<deUint32>*			preserveAttachmentReferences)
{
	vector<AttachmentRef>&	inputAttachmentReferences			= attachmentReferenceLists[0];
	vector<AttachmentRef>&	colorAttachmentReferences			= attachmentReferenceLists[1];
	vector<AttachmentRef>&	resolveAttachmentReferences			= attachmentReferenceLists[2];
	vector<AttachmentRef>&	depthStencilAttachmentReferences	= attachmentReferenceLists[3];

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
		const SubpassDesc subpassDescription														//  VkSubpassDescription										||  VkSubpassDescription2KHR
		(
																									//																||  VkStructureType						sType;
			DE_NULL,																				//																||  const void*							pNext;
			subpass.getFlags(),																		//  VkSubpassDescriptionFlags		flags;						||  VkSubpassDescriptionFlags			flags;
			subpass.getPipelineBindPoint(),															//  VkPipelineBindPoint				pipelineBindPoint;			||  VkPipelineBindPoint					pipelineBindPoint;
			0u,																						//																||  deUint32							viewMask;
			(deUint32)inputAttachmentReferences.size(),												//  deUint32						inputAttachmentCount;		||  deUint32							inputAttachmentCount;
			inputAttachmentReferences.empty() ? DE_NULL : &inputAttachmentReferences[0],			//  const VkAttachmentReference*	pInputAttachments;			||  const VkAttachmentReference2KHR*	pInputAttachments;
			(deUint32)colorAttachmentReferences.size(),												//  deUint32						colorAttachmentCount;		||  deUint32							colorAttachmentCount;
			colorAttachmentReferences.empty() ? DE_NULL :  &colorAttachmentReferences[0],			//  const VkAttachmentReference*	pColorAttachments;			||  const VkAttachmentReference2KHR*	pColorAttachments;
			resolveAttachmentReferences.empty() ? DE_NULL : &resolveAttachmentReferences[0],		//  const VkAttachmentReference*	pResolveAttachments;		||  const VkAttachmentReference2KHR*	pResolveAttachments;
			&depthStencilAttachmentReferences[0],													//  const VkAttachmentReference*	pDepthStencilAttachment;	||  const VkAttachmentReference2KHR*	pDepthStencilAttachment;
			(deUint32)preserveAttachmentReferences->size(),											//  deUint32						preserveAttachmentCount;	||  deUint32							preserveAttachmentCount;
			preserveAttachmentReferences->empty() ? DE_NULL : &(*preserveAttachmentReferences)[0]	//  const deUint32*					pPreserveAttachments;		||  const deUint32*						pPreserveAttachments;
		);

		return subpassDescription;
	}
}

template <typename SubpassDep>
SubpassDep createSubpassDependency (const SubpassDependency& dependencyInfo)
{
	const SubpassDep	dependency			//  VkSubpassDependency											||  VkSubpassDependency2KHR
	(
											//																||	VkStructureType						sType;
		DE_NULL,							//																||	const void*							pNext;
		dependencyInfo.getSrcPass(),		//  deUint32						srcSubpass;					||	deUint32							srcSubpass;
		dependencyInfo.getDstPass(),		//  deUint32						dstSubpass;					||	deUint32							dstSubpass;
		dependencyInfo.getSrcStageMask(),	//  VkPipelineStageFlags			srcStageMask;				||	VkPipelineStageFlags				srcStageMask;
		dependencyInfo.getDstStageMask(),	//  VkPipelineStageFlags			dstStageMask;				||	VkPipelineStageFlags				dstStageMask;
		dependencyInfo.getSrcAccessMask(),	//  VkAccessFlags					srcAccessMask;				||	VkAccessFlags						srcAccessMask;
		dependencyInfo.getDstAccessMask(),	//  VkAccessFlags					dstAccessMask;				||	VkAccessFlags						dstAccessMask;
		dependencyInfo.getFlags(),			//  VkDependencyFlags				dependencyFlags;			||	VkDependencyFlags					dependencyFlags;
		0u									//																||	deInt32								viewOffset;
	);

	return dependency;
}

de::MovePtr<VkRenderPassInputAttachmentAspectCreateInfo> createRenderPassInputAttachmentAspectCreateInfo(const RenderPass& renderPassInfo)
{
	de::MovePtr<VkRenderPassInputAttachmentAspectCreateInfo>	result	(DE_NULL);

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
									 const RenderPass&		renderPassInfo)
{
	const size_t												perSubpassAttachmentReferenceLists = 4;
	vector<AttachmentDesc>										attachments;
	vector<SubpassDesc>											subpasses;
	vector<SubpassDep>											dependencies;
	vector<vector<AttachmentRef> >								attachmentReferenceLists(renderPassInfo.getSubpasses().size() * perSubpassAttachmentReferenceLists);
	vector<vector<deUint32> >									preserveAttachments(renderPassInfo.getSubpasses().size());
	de::MovePtr<VkRenderPassInputAttachmentAspectCreateInfo>	inputAspectCreateInfo(createRenderPassInputAttachmentAspectCreateInfo(renderPassInfo));

	for (size_t attachmentNdx = 0; attachmentNdx < renderPassInfo.getAttachments().size(); attachmentNdx++)
		attachments.push_back(createAttachmentDescription<AttachmentDesc>(renderPassInfo.getAttachments()[attachmentNdx]));

	for (size_t subpassNdx = 0; subpassNdx < renderPassInfo.getSubpasses().size(); subpassNdx++)
		subpasses.push_back(createSubpassDescription<SubpassDesc>(renderPassInfo.getSubpasses()[subpassNdx], &(attachmentReferenceLists[subpassNdx * perSubpassAttachmentReferenceLists]), &preserveAttachments[subpassNdx]));

	for (size_t depNdx = 0; depNdx < renderPassInfo.getDependencies().size(); depNdx++)
		dependencies.push_back(createSubpassDependency<SubpassDep>(renderPassInfo.getDependencies()[depNdx]));

	const RenderPassCreateInfo	renderPassCreator				//  VkRenderPassCreateInfo										||  VkRenderPassCreateInfo2KHR
	(
																//  VkStructureType					sType;						||  VkStructureType						sType;
		inputAspectCreateInfo.get(),							//  const void*						pNext;						||  const void*							pNext;
		(VkRenderPassCreateFlags)0u,							//  VkRenderPassCreateFlags			flags;						||  VkRenderPassCreateFlags				flags;
		(deUint32)attachments.size(),							//  deUint32						attachmentCount;			||  deUint32							attachmentCount;
		(attachments.empty() ? DE_NULL : &attachments[0]),		//  const VkAttachmentDescription*	pAttachments;				||  const VkAttachmentDescription2KHR*	pAttachments;
		(deUint32)subpasses.size(),								//  deUint32						subpassCount;				||  deUint32							subpassCount;
		(subpasses.empty() ? DE_NULL : &subpasses[0]),			//  const VkSubpassDescription*		pSubpasses;					||  const VkSubpassDescription2KHR*		pSubpasses;
		(deUint32)dependencies.size(),							//  deUint32						dependencyCount;			||  deUint32							dependencyCount;
		(dependencies.empty() ? DE_NULL : &dependencies[0]),	//  const VkSubpassDependency*		pDependencies;				||  const VkSubpassDependency2KHR*		pDependencies;
		0u,														//																||  deUint32							correlatedViewMaskCount;
		DE_NULL													//																||  const deUint32*						pCorrelatedViewMasks;
	);

	return renderPassCreator.createRenderPass(vk, device);
}

Move<VkRenderPass> createRenderPass (const DeviceInterface&	vk,
									 VkDevice				device,
									 const RenderPass&		renderPassInfo,
									 const RenderPassType	renderPassType)
{
	switch (renderPassType)
	{
		case RENDERPASS_TYPE_LEGACY:
			return createRenderPass<AttachmentDescription1, AttachmentReference1, SubpassDescription1, SubpassDependency1, RenderPassCreateInfo1>(vk, device, renderPassInfo);
		case RENDERPASS_TYPE_RENDERPASS2:
			return createRenderPass<AttachmentDescription2, AttachmentReference2, SubpassDescription2, SubpassDependency2, RenderPassCreateInfo2>(vk, device, renderPassInfo);
		default:
			TCU_THROW(InternalError, "Impossible");
	}
}

Move<VkFramebuffer> createFramebuffer (const DeviceInterface&		vk,
									   VkDevice						device,
									   VkRenderPass					renderPass,
									   const UVec2&					size,
									   const vector<VkImageView>&	attachments)
{
	return createFramebuffer(vk, device, 0u, renderPass, (deUint32)attachments.size(), attachments.empty() ? DE_NULL : &attachments[0], size.x(), size.y(), 1u);
}

Move<VkImage> createAttachmentImage (const DeviceInterface&	vk,
									 VkDevice				device,
									 deUint32				queueIndex,
									 const UVec2&			size,
									 VkFormat				format,
									 VkSampleCountFlagBits	samples,
									 VkImageUsageFlags		usageFlags,
									 VkImageLayout			layout)
{
	VkImageUsageFlags			targetUsageFlags	= 0;
	const tcu::TextureFormat	textureFormat		= mapVkFormat(format);

	DE_ASSERT(!(tcu::hasDepthComponent(vk::mapVkFormat(format).order) || tcu::hasStencilComponent(vk::mapVkFormat(format).order))
					|| ((usageFlags & vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0));

	DE_ASSERT((tcu::hasDepthComponent(vk::mapVkFormat(format).order) || tcu::hasStencilComponent(vk::mapVkFormat(format).order))
					|| ((usageFlags & vk::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0));

	if (tcu::hasDepthComponent(textureFormat.order) || tcu::hasStencilComponent(textureFormat.order))
		targetUsageFlags |= vk::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	else
		targetUsageFlags |= vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	return createImage(vk, device,
					   (VkImageCreateFlags)0,
					   VK_IMAGE_TYPE_2D,
					   format,
					   vk::makeExtent3D(size.x(), size.y(), 1u),
					   1u /* mipLevels */,
					   1u /* arraySize */,
					   samples,
					   VK_IMAGE_TILING_OPTIMAL,
					   usageFlags | targetUsageFlags,
					   VK_SHARING_MODE_EXCLUSIVE,
					   1,
					   &queueIndex,
					   layout);
}

de::MovePtr<Allocation> createImageMemory (const InstanceInterface&	vki,
										   const VkPhysicalDevice&	vkd,
										   const DeviceInterface&	vk,
										   VkDevice					device,
										   Allocator&				allocator,
										   VkImage					image,
										   bool						lazy,
										   AllocationKind			allocationKind)
{
	const MemoryRequirement memoryRequirement	= lazy ? MemoryRequirement::LazilyAllocated : MemoryRequirement::Any;
	de::MovePtr<Allocation> allocation			= allocateImage(vki, vk, vkd, device, image, memoryRequirement, allocator, allocationKind);

	bindImageMemory(vk, device, image, allocation->getMemory(), allocation->getOffset());

	return allocation;
}

Move<VkImageView> createImageAttachmentView (const DeviceInterface&	vk,
											 VkDevice				device,
											 VkImage				image,
											 VkFormat				format,
											 VkImageAspectFlags		aspect)
{
	const VkImageSubresourceRange range =
	{
		aspect,
		0,
		1,
		0,
		1
	};

	return createImageView(vk, device, 0u, image, VK_IMAGE_VIEW_TYPE_2D, format, makeComponentMappingRGBA(), range);
}

VkClearValue randomClearValue (const Attachment& attachment, de::Random& rng, deBool useFormatCompCount, const DepthValuesArray& depthValues)
{
	const float					clearNan	= tcu::Float32::nan().asFloat();
	const tcu::TextureFormat	format		= mapVkFormat(attachment.getFormat());

	if (tcu::hasStencilComponent(format.order) || tcu::hasDepthComponent(format.order))
	{
		VkClearValue clearValue;

		clearValue.depthStencil.depth	= clearNan;
		clearValue.depthStencil.stencil	= 0xCDu;

		if (tcu::hasStencilComponent(format.order))
			clearValue.depthStencil.stencil	= rng.getBool()
											? 0xFFu
											: 0x0u;

		if (tcu::hasDepthComponent(format.order))
			clearValue.depthStencil.depth	= float(depthValues[rng.getBool() ? 1 : 0]) / 255.0f;

		return clearValue;
	}
	else
	{
		VkClearValue clearValue;

		clearValue.color = randomColorClearValue(attachment, rng, useFormatCompCount);

		return clearValue;
	}
}

class AttachmentResources
{
public:
	AttachmentResources (const InstanceInterface&	vki,
						 const VkPhysicalDevice&	physDevice,
						 const DeviceInterface&		vk,
						 VkDevice					device,
						 Allocator&					allocator,
						 deUint32					queueIndex,
						 const UVec2&				size,
						 const Attachment&			attachmentInfo,
						 VkImageUsageFlags			usageFlags,
						 const AllocationKind		allocationKind)
		: m_image			(createAttachmentImage(vk, device, queueIndex, size, attachmentInfo.getFormat(), attachmentInfo.getSamples(), usageFlags, VK_IMAGE_LAYOUT_UNDEFINED))
		, m_imageMemory		(createImageMemory(vki, physDevice, vk, device, allocator, *m_image, ((usageFlags & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT) != 0), allocationKind))
		, m_attachmentView	(createImageAttachmentView(vk, device, *m_image, attachmentInfo.getFormat(), getImageAspectFlags(attachmentInfo.getFormat())))
	{
		const tcu::TextureFormat	format			= mapVkFormat(attachmentInfo.getFormat());
		const bool					isDepthFormat	= tcu::hasDepthComponent(format.order);
		const bool					isStencilFormat	= tcu::hasStencilComponent(format.order);

		if (isDepthFormat && isStencilFormat)
		{
			m_depthInputAttachmentView		= createImageAttachmentView(vk, device, *m_image, attachmentInfo.getFormat(), VK_IMAGE_ASPECT_DEPTH_BIT);
			m_stencilInputAttachmentView	= createImageAttachmentView(vk, device, *m_image, attachmentInfo.getFormat(), VK_IMAGE_ASPECT_STENCIL_BIT);

			m_inputAttachmentViews = std::make_pair(*m_depthInputAttachmentView, *m_stencilInputAttachmentView);
		}
		else
			m_inputAttachmentViews = std::make_pair(*m_attachmentView, (vk::VkImageView)0u);

		if ((usageFlags & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT) == 0)
		{
			if (tcu::hasDepthComponent(format.order) && tcu::hasStencilComponent(format.order))
			{
				const tcu::TextureFormat	depthFormat		= getDepthCopyFormat(attachmentInfo.getFormat());
				const tcu::TextureFormat	stencilFormat	= getStencilCopyFormat(attachmentInfo.getFormat());

				m_bufferSize			= size.x() * size.y() * depthFormat.getPixelSize();
				m_secondaryBufferSize	= size.x() * size.y() * stencilFormat.getPixelSize();

				m_buffer				= createBuffer(vk, device, 0, m_bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, 1, &queueIndex);
				m_bufferMemory			= allocateBuffer(vki, vk, physDevice, device, *m_buffer, MemoryRequirement::HostVisible, allocator, allocationKind);

				bindBufferMemory(vk, device, *m_buffer, m_bufferMemory->getMemory(), m_bufferMemory->getOffset());

				m_secondaryBuffer		= createBuffer(vk, device, 0, m_secondaryBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, 1, &queueIndex);
				m_secondaryBufferMemory	= allocateBuffer(vki, vk, physDevice, device, *m_secondaryBuffer, MemoryRequirement::HostVisible, allocator, allocationKind);

				bindBufferMemory(vk, device, *m_secondaryBuffer, m_secondaryBufferMemory->getMemory(), m_secondaryBufferMemory->getOffset());
			}
			else
			{
				m_bufferSize	= size.x() * size.y() * format.getPixelSize();

				m_buffer		= createBuffer(vk, device, 0, m_bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, 1, &queueIndex);
				m_bufferMemory	= allocateBuffer(vki, vk, physDevice, device, *m_buffer, MemoryRequirement::HostVisible, allocator, allocationKind);

				bindBufferMemory(vk, device, *m_buffer, m_bufferMemory->getMemory(), m_bufferMemory->getOffset());
			}
		}
	}

	const pair<VkImageView, VkImageView>& getInputAttachmentViews (void) const
	{
		return m_inputAttachmentViews;
	}

	~AttachmentResources (void)
	{
	}

	VkImageView getAttachmentView (void) const
	{
		return *m_attachmentView;
	}

	VkImage getImage (void) const
	{
		return *m_image;
	}

	VkBuffer getBuffer (void) const
	{
		DE_ASSERT(*m_buffer != DE_NULL);
		return *m_buffer;
	}

	VkDeviceSize getBufferSize (void) const
	{
		DE_ASSERT(*m_buffer != DE_NULL);
		return m_bufferSize;
	}

	const Allocation& getResultMemory (void) const
	{
		DE_ASSERT(m_bufferMemory);
		return *m_bufferMemory;
	}

	VkBuffer getSecondaryBuffer (void) const
	{
		DE_ASSERT(*m_secondaryBuffer != DE_NULL);
		return *m_secondaryBuffer;
	}

	VkDeviceSize getSecondaryBufferSize (void) const
	{
		DE_ASSERT(*m_secondaryBuffer != DE_NULL);
		return m_secondaryBufferSize;
	}

	const Allocation& getSecondaryResultMemory (void) const
	{
		DE_ASSERT(m_secondaryBufferMemory);
		return *m_secondaryBufferMemory;
	}

private:
	const Unique<VkImage>			m_image;
	const UniquePtr<Allocation>		m_imageMemory;
	const Unique<VkImageView>		m_attachmentView;

	Move<VkImageView>				m_depthInputAttachmentView;
	Move<VkImageView>				m_stencilInputAttachmentView;
	pair<VkImageView, VkImageView>	m_inputAttachmentViews;

	Move<VkBuffer>					m_buffer;
	VkDeviceSize					m_bufferSize;
	de::MovePtr<Allocation>			m_bufferMemory;

	Move<VkBuffer>					m_secondaryBuffer;
	VkDeviceSize					m_secondaryBufferSize;
	de::MovePtr<Allocation>			m_secondaryBufferMemory;
};

void uploadBufferData (const DeviceInterface&	vk,
					   VkDevice					device,
					   const Allocation&		memory,
					   size_t					size,
					   const void*				data,
					   VkDeviceSize				nonCoherentAtomSize)
{
	// Expand the range to flush to account for the nonCoherentAtomSize
	const VkDeviceSize roundedOffset	= de::roundDown(memory.getOffset(), nonCoherentAtomSize);
	const VkDeviceSize roundedSize		= de::roundUp(memory.getOffset() - roundedOffset + static_cast<VkDeviceSize>(size), nonCoherentAtomSize);

	const VkMappedMemoryRange range =
	{
		VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,	// sType;
		DE_NULL,								// pNext;
		memory.getMemory(),						// mem;
		roundedOffset,							// offset;
		roundedSize,							// size;
	};
	void* const ptr = memory.getHostPtr();

	deMemcpy(ptr, data, size);
	VK_CHECK(vk.flushMappedMemoryRanges(device, 1, &range));
}

VkImageAspectFlagBits getPrimaryImageAspect (tcu::TextureFormat::ChannelOrder order)
{
	DE_STATIC_ASSERT(tcu::TextureFormat::CHANNELORDER_LAST == 21);

	switch (order)
	{
		case tcu::TextureFormat::D:
		case tcu::TextureFormat::DS:
			return VK_IMAGE_ASPECT_DEPTH_BIT;

		case tcu::TextureFormat::S:
			return VK_IMAGE_ASPECT_STENCIL_BIT;

		default:
			return VK_IMAGE_ASPECT_COLOR_BIT;
	}
}

deUint32 getAttachmentNdx (const vector<AttachmentReference>& colorAttachments, size_t ndx)
{
	return (colorAttachments[ndx].getAttachment() == VK_ATTACHMENT_UNUSED) ? (deUint32)ndx : colorAttachments[ndx].getAttachment();
}

class RenderQuad
{
public:
					RenderQuad			(const Vec2& posA, const Vec2& posB)
		: m_vertices(6)
	{
		m_vertices[0] = posA;
		m_vertices[1] = Vec2(posA[0], posB[1]);
		m_vertices[2] = posB;

		m_vertices[3] = posB;
		m_vertices[4] = Vec2(posB[0], posA[1]);
		m_vertices[5] = posA;
	}

	const Vec2&		getCornerA			(void) const
	{
		return m_vertices[0];
	}

	const Vec2&		getCornerB			(void) const
	{
		return m_vertices[2];
	}

	const void*		getVertexPointer	(void) const
	{
		return &m_vertices[0];
	}

	size_t			getVertexDataSize	(void) const
	{
		return sizeof(Vec2) * m_vertices.size();
	}

private:
	vector<Vec2>	m_vertices;
};

class ColorClear
{
public:
								ColorClear	(const UVec2&				offset,
											 const UVec2&				size,
											 const VkClearColorValue&	color)
		: m_offset	(offset)
		, m_size	(size)
		, m_color	(color)
	{
	}

	const UVec2&				getOffset	(void) const { return m_offset;	}
	const UVec2&				getSize		(void) const { return m_size;	}
	const VkClearColorValue&	getColor	(void) const { return m_color;	}

private:
	UVec2						m_offset;
	UVec2						m_size;
	VkClearColorValue			m_color;
};

class DepthStencilClear
{
public:
					DepthStencilClear	(const UVec2&	offset,
										 const UVec2&	size,
										 float			depth,
										 deUint32		stencil)
		: m_offset	(offset)
		, m_size	(size)
		, m_depth	(depth)
		, m_stencil	(stencil)
	{
	}

	const UVec2&	getOffset			(void) const { return m_offset;		}
	const UVec2&	getSize				(void) const { return m_size;		}
	float			getDepth			(void) const { return m_depth;		}
	deUint32		getStencil			(void) const { return m_stencil;	}

private:
	const UVec2		m_offset;
	const UVec2		m_size;

	const float		m_depth;
	const deUint32	m_stencil;
};

class SubpassRenderInfo
{
public:
									SubpassRenderInfo				(const RenderPass&					renderPass,
																	 deUint32							subpassIndex,
																	 deUint32							drawStartNdx,

																	 bool								isSecondary_,
																	 bool								omitBlendState_,

																	 const UVec2&						viewportOffset,
																	 const UVec2&						viewportSize,

																	 const Maybe<RenderQuad>&			renderQuad,
																	 const vector<ColorClear>&			colorClears,
																	 const Maybe<DepthStencilClear>&	depthStencilClear)
		: m_viewportOffset		(viewportOffset)
		, m_viewportSize		(viewportSize)
		, m_subpassIndex		(subpassIndex)
		, m_drawStartNdx		(drawStartNdx)
		, m_isSecondary			(isSecondary_)
		, m_omitBlendState		(omitBlendState_)
		, m_flags				(renderPass.getSubpasses()[subpassIndex].getFlags())
		, m_renderQuad			(renderQuad)
		, m_colorClears			(colorClears)
		, m_depthStencilClear	(depthStencilClear)
		, m_colorAttachments	(renderPass.getSubpasses()[subpassIndex].getColorAttachments())
		, m_inputAttachments	(renderPass.getSubpasses()[subpassIndex].getInputAttachments())
	{
		for (deUint32 attachmentNdx = 0; attachmentNdx < (deUint32)m_colorAttachments.size(); attachmentNdx++)
			m_colorAttachmentInfo.push_back(renderPass.getAttachments()[getAttachmentNdx(m_colorAttachments, attachmentNdx)]);

		if (renderPass.getSubpasses()[subpassIndex].getDepthStencilAttachment().getAttachment() != VK_ATTACHMENT_UNUSED)
		{
			m_depthStencilAttachment		= tcu::just(renderPass.getSubpasses()[subpassIndex].getDepthStencilAttachment());
			m_depthStencilAttachmentInfo	= tcu::just(renderPass.getAttachments()[renderPass.getSubpasses()[subpassIndex].getDepthStencilAttachment().getAttachment()]);
		}
	}

	const UVec2&					getViewportOffset				(void) const { return m_viewportOffset;		}
	const UVec2&					getViewportSize					(void) const { return m_viewportSize;		}

	deUint32						getSubpassIndex					(void) const { return m_subpassIndex;		}
	deUint32						getDrawStartNdx					(void) const { return m_drawStartNdx;		}
	bool							isSecondary						(void) const { return m_isSecondary;		}
	bool							getOmitBlendState				(void) const { return m_omitBlendState;		}

	const Maybe<RenderQuad>&		getRenderQuad					(void) const { return m_renderQuad;			}
	const vector<ColorClear>&		getColorClears					(void) const { return m_colorClears;		}
	const Maybe<DepthStencilClear>&	getDepthStencilClear			(void) const { return m_depthStencilClear;	}

	deUint32						getInputAttachmentCount			(void) const { return (deUint32)m_inputAttachments.size(); }
	deUint32						getInputAttachmentIndex			(deUint32 attachmentNdx) const { return m_inputAttachments[attachmentNdx].getAttachment(); }
	VkImageLayout					getInputAttachmentLayout		(deUint32 attachmentNdx) const { return m_inputAttachments[attachmentNdx].getImageLayout(); }

	deUint32						getColorAttachmentCount			(void) const { return (deUint32)m_colorAttachments.size(); }
	VkImageLayout					getColorAttachmentLayout		(deUint32 attachmentNdx) const { return m_colorAttachments[attachmentNdx].getImageLayout(); }
	deUint32						getColorAttachmentIndex			(deUint32 attachmentNdx) const { return m_colorAttachments[attachmentNdx].getAttachment(); }
	const Attachment&				getColorAttachment				(deUint32 attachmentNdx) const { return m_colorAttachmentInfo[attachmentNdx]; }
	Maybe<VkImageLayout>			getDepthStencilAttachmentLayout	(void) const { return m_depthStencilAttachment ? tcu::just(m_depthStencilAttachment->getImageLayout()) : tcu::nothing<VkImageLayout>(); }
	Maybe<deUint32>					getDepthStencilAttachmentIndex	(void) const { return m_depthStencilAttachment ? tcu::just(m_depthStencilAttachment->getAttachment()) : tcu::nothing<deUint32>(); };
	const Maybe<Attachment>&		getDepthStencilAttachment		(void) const { return m_depthStencilAttachmentInfo; }
	VkSubpassDescriptionFlags		getSubpassFlags					(void) const { return m_flags; }

private:
	UVec2							m_viewportOffset;
	UVec2							m_viewportSize;

	deUint32						m_subpassIndex;
	deUint32						m_drawStartNdx;
	bool							m_isSecondary;
	bool							m_omitBlendState;
	VkSubpassDescriptionFlags		m_flags;

	Maybe<RenderQuad>				m_renderQuad;
	vector<ColorClear>				m_colorClears;
	Maybe<DepthStencilClear>		m_depthStencilClear;

	vector<AttachmentReference>		m_colorAttachments;
	vector<Attachment>				m_colorAttachmentInfo;

	Maybe<AttachmentReference>		m_depthStencilAttachment;
	Maybe<Attachment>				m_depthStencilAttachmentInfo;

	vector<AttachmentReference>		m_inputAttachments;
};

Move<VkPipeline> createSubpassPipeline (const DeviceInterface&		vk,
										VkDevice					device,
										VkRenderPass				renderPass,
										VkShaderModule				vertexShaderModule,
										VkShaderModule				fragmentShaderModule,
										VkPipelineLayout			pipelineLayout,
										const SubpassRenderInfo&	renderInfo)
{
	Maybe<VkSampleCountFlagBits>					rasterSamples;
	vector<VkPipelineColorBlendAttachmentState>		attachmentBlendStates;

	for (deUint32 attachmentNdx = 0; attachmentNdx < renderInfo.getColorAttachmentCount(); attachmentNdx++)
	{
		const Attachment& attachment = renderInfo.getColorAttachment(attachmentNdx);

		DE_ASSERT(!rasterSamples || *rasterSamples == attachment.getSamples());

		rasterSamples = attachment.getSamples();

		{
			const VkPipelineColorBlendAttachmentState attachmentBlendState =
			{
				VK_FALSE,																									// blendEnable
				VK_BLEND_FACTOR_SRC_ALPHA,																					// srcBlendColor
				VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,																		// destBlendColor
				VK_BLEND_OP_ADD,																							// blendOpColor
				VK_BLEND_FACTOR_ONE,																						// srcBlendAlpha
				VK_BLEND_FACTOR_ONE,																						// destBlendAlpha
				VK_BLEND_OP_ADD,																							// blendOpAlpha
				(attachmentNdx < renderInfo.getDrawStartNdx() ? (deUint32)0 :
					VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT)	// channelWriteMask
			};

			attachmentBlendStates.push_back(attachmentBlendState);
		}
	}

	if (renderInfo.getDepthStencilAttachment())
	{
		const Attachment& attachment = *renderInfo.getDepthStencilAttachment();

		DE_ASSERT(!rasterSamples || *rasterSamples == attachment.getSamples());
		rasterSamples = attachment.getSamples();
	}

	// If there are no attachment use single sample
	if (!rasterSamples)
		rasterSamples = VK_SAMPLE_COUNT_1_BIT;

	const VkVertexInputBindingDescription			vertexBinding		=
	{
		0u,															// binding
		(deUint32)sizeof(tcu::Vec2),								// strideInBytes
		VK_VERTEX_INPUT_RATE_VERTEX,								// stepRate
	};

	const VkVertexInputAttributeDescription			vertexAttrib		=
	{
		0u,															// location
		0u,															// binding
		VK_FORMAT_R32G32_SFLOAT,									// format
		0u,															// offsetInBytes
	};

	const VkPipelineVertexInputStateCreateInfo		vertexInputState	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	//	sType
		DE_NULL,													//	pNext
		(VkPipelineVertexInputStateCreateFlags)0u,
		1u,															//	bindingCount
		&vertexBinding,												//	pVertexBindingDescriptions
		1u,															//	attributeCount
		&vertexAttrib,												//	pVertexAttributeDescriptions
	};

	const VkPipelineInputAssemblyStateCreateInfo	inputAssemblyState	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType                            sType
		DE_NULL,														// const void*                                pNext
		0u,																// VkPipelineInputAssemblyStateCreateFlags    flags
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,							// VkPrimitiveTopology                        topology
		VK_FALSE														// VkBool32                                   primitiveRestartEnable
	};

	const VkViewport								viewport			=
	{
		(float)renderInfo.getViewportOffset().x(),	(float)renderInfo.getViewportOffset().y(),
		(float)renderInfo.getViewportSize().x(),	(float)renderInfo.getViewportSize().y(),
		0.0f, 1.0f
	};

	const VkRect2D									scissor				=
	{
		{ (deInt32)renderInfo.getViewportOffset().x(),	(deInt32)renderInfo.getViewportOffset().y() },
		{ renderInfo.getViewportSize().x(),				renderInfo.getViewportSize().y() }
	};

	const VkPipelineViewportStateCreateInfo			viewportState		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,	// VkStructureType                             sType
		DE_NULL,												// const void*                                 pNext
		(VkPipelineViewportStateCreateFlags)0,					// VkPipelineViewportStateCreateFlags          flags
		1u,														// deUint32                                    viewportCount
		&viewport,												// const VkViewport*                           pViewports
		1u,														// deUint32                                    scissorCount
		&scissor												// const VkRect2D*                             pScissors
	};

	const VkPipelineRasterizationStateCreateInfo	rasterizationState	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,	// VkStructureType                            sType
		DE_NULL,													// const void*                                pNext
		0u,															// VkPipelineRasterizationStateCreateFlags    flags
		VK_FALSE,													// VkBool32                                   depthClampEnable
		VK_FALSE,													// VkBool32                                   rasterizerDiscardEnable
		VK_POLYGON_MODE_FILL,										// VkPolygonMode                              polygonMode
		VK_CULL_MODE_NONE,											// VkCullModeFlags                            cullMode
		VK_FRONT_FACE_COUNTER_CLOCKWISE,							// VkFrontFace                                frontFace
		VK_FALSE,													// VkBool32                                   depthBiasEnable
		0.0f,														// float                                      depthBiasConstantFactor
		0.0f,														// float                                      depthBiasClamp
		0.0f,														// float                                      depthBiasSlopeFactor
		1.0f														// float                                      lineWidth
	};

	const VkPipelineMultisampleStateCreateInfo		multisampleState	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,		// sType
		DE_NULL,														// pNext
		(VkPipelineMultisampleStateCreateFlags)0u,
		*rasterSamples,													// rasterSamples
		VK_FALSE,														// sampleShadingEnable
		0.0f,															// minSampleShading
		DE_NULL,														// pSampleMask
		VK_FALSE,														// alphaToCoverageEnable
		VK_FALSE,														// alphaToOneEnable
	};
	const size_t	stencilIndex	= renderInfo.getSubpassIndex();

	const VkBool32	writeDepth		= renderInfo.getDepthStencilAttachmentLayout()
										&& *renderInfo.getDepthStencilAttachmentLayout() != VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
										&& *renderInfo.getDepthStencilAttachmentLayout() != VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL
									? VK_TRUE
									: VK_FALSE;

	const VkBool32	writeStencil	= renderInfo.getDepthStencilAttachmentLayout()
										&& *renderInfo.getDepthStencilAttachmentLayout() != VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
										&& *renderInfo.getDepthStencilAttachmentLayout() != VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL
									? VK_TRUE
									: VK_FALSE;

	const VkPipelineDepthStencilStateCreateInfo depthStencilState =
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	// sType
		DE_NULL,													// pNext
		(VkPipelineDepthStencilStateCreateFlags)0u,
		writeDepth,													// depthTestEnable
		writeDepth,													// depthWriteEnable
		VK_COMPARE_OP_ALWAYS,										// depthCompareOp
		VK_FALSE,													// depthBoundsEnable
		writeStencil,												// stencilTestEnable
		{
			VK_STENCIL_OP_REPLACE,									// stencilFailOp
			VK_STENCIL_OP_REPLACE,									// stencilPassOp
			VK_STENCIL_OP_REPLACE,									// stencilDepthFailOp
			VK_COMPARE_OP_ALWAYS,									// stencilCompareOp
			~0u,													// stencilCompareMask
			~0u,													// stencilWriteMask
			((stencilIndex % 2) == 0) ? ~0x0u : 0x0u				// stencilReference
		},															// front
		{
			VK_STENCIL_OP_REPLACE,									// stencilFailOp
			VK_STENCIL_OP_REPLACE,									// stencilPassOp
			VK_STENCIL_OP_REPLACE,									// stencilDepthFailOp
			VK_COMPARE_OP_ALWAYS,									// stencilCompareOp
			~0u,													// stencilCompareMask
			~0u,													// stencilWriteMask
			((stencilIndex % 2) == 0) ? ~0x0u : 0x0u				// stencilReference
		},															// back

		0.0f,														// minDepthBounds;
		1.0f														// maxDepthBounds;
	};

	const VkPipelineColorBlendStateCreateInfo blendState =
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,			// sType
		DE_NULL,															// pNext
		(VkPipelineColorBlendStateCreateFlags)0u,
		VK_FALSE,															// logicOpEnable
		VK_LOGIC_OP_COPY,													// logicOp
		(deUint32)attachmentBlendStates.size(),								// attachmentCount
		attachmentBlendStates.empty() ? DE_NULL : &attachmentBlendStates[0],// pAttachments
		{ 0.0f, 0.0f, 0.0f, 0.0f }											// blendConst
	};

	return makeGraphicsPipeline(vk,								// const DeviceInterface&                        vk
								device,							// const VkDevice                                device
								pipelineLayout,					// const VkPipelineLayout                        pipelineLayout
								vertexShaderModule,				// const VkShaderModule                          vertexShaderModule
								DE_NULL,						// const VkShaderModule                          tessellationControlShaderModule
								DE_NULL,						// const VkShaderModule                          tessellationEvalShaderModule
								DE_NULL,						// const VkShaderModule                          geometryShaderModule
								fragmentShaderModule,			// const VkShaderModule                          fragmentShaderModule
								renderPass,						// const VkRenderPass                            renderPass
								renderInfo.getSubpassIndex(),	// const deUint32                                subpass
								&vertexInputState,				// const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
								&inputAssemblyState,			// const VkPipelineInputAssemblyStateCreateInfo* pInputAssemblyState;
								DE_NULL,						// const VkPipelineRasterizationStateCreateInfo* rasterizationStateCreateInfo
								&viewportState,					// const VkPipelineViewportStateCreateInfo*      pViewportStat;
								&rasterizationState,			// const VkPipelineRasterizationStateCreateInfo* pRasterizationState
								&multisampleState,				// const VkPipelineMultisampleStateCreateInfo*   multisampleStateCreateInfo
								&depthStencilState,				// const VkPipelineDepthStencilStateCreateInfo*  depthStencilStateCreateInfo
								renderInfo.getOmitBlendState()
									? DE_NULL : &blendState);	// const VkPipelineColorBlendStateCreateInfo*    colorBlendStateCreateInfo
}

class SubpassRenderer
{
public:
	SubpassRenderer (Context&										context,
					 const DeviceInterface&							vk,
					 VkDevice										device,
					 Allocator&										allocator,
					 VkRenderPass									renderPass,
					 VkFramebuffer									framebuffer,
					 VkCommandPool									commandBufferPool,
					 deUint32										queueFamilyIndex,
					 const vector<VkImage>&							attachmentImages,
					 const vector<pair<VkImageView, VkImageView> >&	attachmentViews,
					 const SubpassRenderInfo&						renderInfo,
					 const vector<Attachment>&						attachmentInfos,
					 const AllocationKind							allocationKind)
		: m_renderInfo	(renderInfo)
	{
		const InstanceInterface&				vki				= context.getInstanceInterface();
		const VkPhysicalDevice&					physDevice		= context.getPhysicalDevice();
		const deUint32							subpassIndex	= renderInfo.getSubpassIndex();
		vector<VkDescriptorSetLayoutBinding>	bindings;

		for (deUint32 colorAttachmentNdx = 0; colorAttachmentNdx < renderInfo.getColorAttachmentCount();  colorAttachmentNdx++)
		{
			const deUint32 attachmentNdx	= (renderInfo.getColorAttachmentIndex(colorAttachmentNdx) == VK_ATTACHMENT_UNUSED) ? colorAttachmentNdx
											: renderInfo.getColorAttachmentIndex(colorAttachmentNdx);

			m_colorAttachmentImages.push_back(attachmentImages[attachmentNdx]);
		}

		if (renderInfo.getDepthStencilAttachmentIndex())
			m_depthStencilAttachmentImage = attachmentImages[*renderInfo.getDepthStencilAttachmentIndex()];

		if (renderInfo.getRenderQuad())
		{
			const RenderQuad&	renderQuad	= *renderInfo.getRenderQuad();

			if (renderInfo.getInputAttachmentCount() > 0)
			{
				deUint32								bindingIndex	= 0;

				for (deUint32 inputAttachmentNdx = 0; inputAttachmentNdx < renderInfo.getInputAttachmentCount(); inputAttachmentNdx++)
				{
					const Attachment			attachmentInfo	= attachmentInfos[renderInfo.getInputAttachmentIndex(inputAttachmentNdx)];
					const VkImageLayout			layout			= renderInfo.getInputAttachmentLayout(inputAttachmentNdx);
					const tcu::TextureFormat	format			= mapVkFormat(attachmentInfo.getFormat());
					const bool					isDepthFormat	= tcu::hasDepthComponent(format.order);
					const bool					isStencilFormat	= tcu::hasStencilComponent(format.order);
					const deUint32				bindingCount	= (isDepthFormat && layout != VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL)
																	&& (isStencilFormat && layout != VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL)
																? 2u
																: 1u;

					for (deUint32 bindingNdx = 0; bindingNdx < bindingCount; bindingNdx++)
					{
						const VkDescriptorSetLayoutBinding binding =
						{
							bindingIndex,
							vk::VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
							1u,
							vk::VK_SHADER_STAGE_FRAGMENT_BIT,
							DE_NULL
						};

						bindings.push_back(binding);
						bindingIndex++;
					}
				}

				const VkDescriptorSetLayoutCreateInfo createInfo =
				{
					vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
					DE_NULL,

					0u,
					(deUint32)bindings.size(),
					&bindings[0]
				};

				m_descriptorSetLayout = vk::createDescriptorSetLayout(vk, device, &createInfo);
			}

			const VkDescriptorSetLayout			descriptorSetLayout		= *m_descriptorSetLayout;
			const VkPipelineLayoutCreateInfo	pipelineLayoutParams	=
			{
				VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,			// sType;
				DE_NULL,												// pNext;
				(vk::VkPipelineLayoutCreateFlags)0,
				m_descriptorSetLayout ? 1u :0u ,						// setLayoutCount;
				m_descriptorSetLayout ? &descriptorSetLayout : DE_NULL,	// pSetLayouts;
				0u,														// pushConstantRangeCount;
				DE_NULL,												// pPushConstantRanges;
			};

			m_vertexShaderModule	= createShaderModule(vk, device, context.getBinaryCollection().get(de::toString(subpassIndex) + "-vert"), 0u);
			m_fragmentShaderModule	= createShaderModule(vk, device, context.getBinaryCollection().get(de::toString(subpassIndex) + "-frag"), 0u);
			m_pipelineLayout		= createPipelineLayout(vk, device, &pipelineLayoutParams);
			m_pipeline				= createSubpassPipeline(vk, device, renderPass, *m_vertexShaderModule, *m_fragmentShaderModule, *m_pipelineLayout, m_renderInfo);

			// Round up the vertex buffer size to honor nonCoherentAtomSize.
			const auto	properties			= vk::getPhysicalDeviceProperties(context.getInstanceInterface(), context.getPhysicalDevice());
			const auto	vertexBufferSize	= de::roundUp(static_cast<VkDeviceSize>(renderQuad.getVertexDataSize()), properties.limits.nonCoherentAtomSize);

			m_vertexBuffer			= createBuffer(vk, device, 0u, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, 1u, &queueFamilyIndex);
			m_vertexBufferMemory	= allocateBuffer(vki, vk, physDevice, device, *m_vertexBuffer, MemoryRequirement::HostVisible, allocator, allocationKind);

			bindBufferMemory(vk, device, *m_vertexBuffer, m_vertexBufferMemory->getMemory(), m_vertexBufferMemory->getOffset());

			uploadBufferData(vk, device, *m_vertexBufferMemory, renderQuad.getVertexDataSize(), renderQuad.getVertexPointer(), properties.limits.nonCoherentAtomSize);

			if (renderInfo.getInputAttachmentCount() > 0)
			{
				{
					const VkDescriptorPoolSize poolSize =
					{
						vk::VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
						// \note Reserve 2 per input attachment since depthStencil attachments require 2.
						renderInfo.getInputAttachmentCount() * 2u
					};
					const VkDescriptorPoolCreateInfo createInfo =
					{
						vk::VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
						DE_NULL,
						VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,

						// \note Reserve 2 per input attachment since depthStencil attachments require 2.
						renderInfo.getInputAttachmentCount() * 2u,
						1u,
						&poolSize
					};

					m_descriptorPool = vk::createDescriptorPool(vk, device, &createInfo);
				}
				{
					const VkDescriptorSetAllocateInfo	allocateInfo =
					{
						vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
						DE_NULL,

						*m_descriptorPool,
						1u,
						&descriptorSetLayout
					};

					m_descriptorSet = vk::allocateDescriptorSet(vk, device, &allocateInfo);
				}
				{
					vector<VkWriteDescriptorSet>	writes			(bindings.size());
					vector<VkDescriptorImageInfo>	imageInfos		(bindings.size());
					deUint32						bindingIndex	= 0;

					for (deUint32 inputAttachmentNdx = 0; inputAttachmentNdx < renderInfo.getInputAttachmentCount(); inputAttachmentNdx++)
					{
						const Attachment			attachmentInfo			= attachmentInfos[renderInfo.getInputAttachmentIndex(inputAttachmentNdx)];
						const tcu::TextureFormat	format					= mapVkFormat(attachmentInfo.getFormat());
						const bool					isDepthFormat			= tcu::hasDepthComponent(format.order);
						const bool					isStencilFormat			= tcu::hasStencilComponent(format.order);
						const VkImageLayout			inputAttachmentLayout	= renderInfo.getInputAttachmentLayout(inputAttachmentNdx);


						if (isDepthFormat && isStencilFormat)
						{
							if (inputAttachmentLayout != VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL)
							{
								const VkDescriptorImageInfo	imageInfo =
								{
									(VkSampler)0,
									attachmentViews[renderInfo.getInputAttachmentIndex(inputAttachmentNdx)].first,
									inputAttachmentLayout
								};
								imageInfos[bindingIndex] = imageInfo;

								{
									const VkWriteDescriptorSet	write =
									{
										VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
										DE_NULL,

										*m_descriptorSet,
										bindingIndex,
										0u,
										1u,
										VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
										&imageInfos[bindingIndex],
										DE_NULL,
										DE_NULL
									};
									writes[bindingIndex] = write;

									bindingIndex++;
								}
							}

							if (inputAttachmentLayout != VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL)
							{
								const VkDescriptorImageInfo	imageInfo =
								{
									(VkSampler)0,
									attachmentViews[renderInfo.getInputAttachmentIndex(inputAttachmentNdx)].second,
									inputAttachmentLayout
								};
								imageInfos[bindingIndex] = imageInfo;

								{
									const VkWriteDescriptorSet	write =
									{
										VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
										DE_NULL,

										*m_descriptorSet,
										bindingIndex,
										0u,
										1u,
										VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
										&imageInfos[bindingIndex],
										DE_NULL,
										DE_NULL
									};
									writes[bindingIndex] = write;

									bindingIndex++;
								}
							}
						}
						else
						{
							const VkDescriptorImageInfo	imageInfo =
							{
								(VkSampler)0,
								attachmentViews[renderInfo.getInputAttachmentIndex(inputAttachmentNdx)].first,
								inputAttachmentLayout
							};
							imageInfos[bindingIndex] = imageInfo;

							{
								const VkWriteDescriptorSet	write =
								{
									VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
									DE_NULL,

									*m_descriptorSet,
									bindingIndex,
									0u,
									1u,
									VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
									&imageInfos[bindingIndex],
									DE_NULL,
									DE_NULL
								};
								writes[bindingIndex] = write;

								bindingIndex++;
							}
						}
					}

					vk.updateDescriptorSets(device, (deUint32)writes.size(), &writes[0], 0u, DE_NULL);
				}
			}
		}

		if (renderInfo.isSecondary())
		{
			m_commandBuffer = allocateCommandBuffer(vk, device, commandBufferPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);

			beginCommandBuffer(vk, *m_commandBuffer, vk::VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, renderPass, subpassIndex, framebuffer, VK_FALSE, (VkQueryControlFlags)0, (VkQueryPipelineStatisticFlags)0);
			pushRenderCommands(vk, *m_commandBuffer);
			endCommandBuffer(vk, *m_commandBuffer);
		}
	}

	bool isSecondary (void) const
	{
		return m_commandBuffer;
	}

	VkCommandBuffer getCommandBuffer (void) const
	{
		DE_ASSERT(isSecondary());
		return *m_commandBuffer;
	}

	void pushRenderCommands (const DeviceInterface&		vk,
							 VkCommandBuffer			commandBuffer)
	{
		if (!m_renderInfo.getColorClears().empty())
		{
			const vector<ColorClear>&	colorClears	(m_renderInfo.getColorClears());

			for (deUint32 attachmentNdx = 0; attachmentNdx < m_renderInfo.getColorAttachmentCount(); attachmentNdx++)
			{
				const ColorClear&		colorClear	= colorClears[attachmentNdx];
				const VkClearAttachment	attachment	=
				{
					VK_IMAGE_ASPECT_COLOR_BIT,
					attachmentNdx,
					makeClearValue(colorClear.getColor()),
				};
				const VkClearRect		rect		=
				{
					{
						{ (deInt32)colorClear.getOffset().x(),	(deInt32)colorClear.getOffset().y()	},
						{ colorClear.getSize().x(),				colorClear.getSize().y()			}
					},					// rect
					0u,					// baseArrayLayer
					1u,					// layerCount
				};

				vk.cmdClearAttachments(commandBuffer, 1u, &attachment, 1u, &rect);
			}
		}

		if (m_renderInfo.getDepthStencilClear())
		{
			const DepthStencilClear&	depthStencilClear	= *m_renderInfo.getDepthStencilClear();
			const deUint32				attachmentNdx		= m_renderInfo.getColorAttachmentCount();
			tcu::TextureFormat			format				= mapVkFormat(m_renderInfo.getDepthStencilAttachment()->getFormat());
			const VkImageLayout			layout				= *m_renderInfo.getDepthStencilAttachmentLayout();
			const VkClearAttachment		attachment			=
			{
				(VkImageAspectFlags)((hasDepthComponent(format.order) && layout != VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL ? VK_IMAGE_ASPECT_DEPTH_BIT : 0)
					| (hasStencilComponent(format.order) && layout != VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL ? VK_IMAGE_ASPECT_STENCIL_BIT : 0)),
				attachmentNdx,
				makeClearValueDepthStencil(depthStencilClear.getDepth(), depthStencilClear.getStencil())
			};
			const VkClearRect				rect				=
			{
				{
					{ (deInt32)depthStencilClear.getOffset().x(),	(deInt32)depthStencilClear.getOffset().y()	},
					{ depthStencilClear.getSize().x(),				depthStencilClear.getSize().y()				}
				},							// rect
				0u,							// baseArrayLayer
				1u,							// layerCount
			};

			if ((tcu::hasDepthComponent(format.order) && layout != VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL)
				|| (tcu::hasStencilComponent(format.order) && layout != VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL))
			{
				vk.cmdClearAttachments(commandBuffer, 1u, &attachment, 1u, &rect);
			}
		}

		vector<VkImageMemoryBarrier>	selfDeps;
		VkPipelineStageFlags			srcStages = 0;
		VkPipelineStageFlags			dstStages = 0;

		for (deUint32 inputAttachmentNdx = 0; inputAttachmentNdx < m_renderInfo.getInputAttachmentCount(); inputAttachmentNdx++)
		{
			for (deUint32 colorAttachmentNdx = 0; colorAttachmentNdx < m_renderInfo.getColorAttachmentCount(); colorAttachmentNdx++)
			{
				if (m_renderInfo.getInputAttachmentIndex(inputAttachmentNdx) == m_renderInfo.getColorAttachmentIndex(colorAttachmentNdx))
				{
					const VkImageMemoryBarrier	barrier   =
					{
						VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// sType
						DE_NULL,										// pNext

						VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,			// srcAccessMask
						VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,			// dstAccessMask

						VK_IMAGE_LAYOUT_GENERAL,						// oldLayout
						VK_IMAGE_LAYOUT_GENERAL,						// newLayout

						VK_QUEUE_FAMILY_IGNORED,						// srcQueueFamilyIndex
						VK_QUEUE_FAMILY_IGNORED,						// destQueueFamilyIndex

						m_colorAttachmentImages[colorAttachmentNdx],	// image
						{												// subresourceRange
							VK_IMAGE_ASPECT_COLOR_BIT,						// aspect
							0,												// baseMipLevel
							1,												// mipLevels
							0,												// baseArraySlice
							1												// arraySize
						}
					};

					srcStages |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
					dstStages |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

					selfDeps.push_back(barrier);
				}
			}

			if (m_renderInfo.getDepthStencilAttachmentIndex() && (m_renderInfo.getInputAttachmentIndex(inputAttachmentNdx) == *m_renderInfo.getDepthStencilAttachmentIndex()))
			{
				const tcu::TextureFormat	format		= mapVkFormat(m_renderInfo.getDepthStencilAttachment()->getFormat());
				const bool					hasDepth	= hasDepthComponent(format.order);
				const bool					hasStencil	= hasStencilComponent(format.order);
				const VkImageMemoryBarrier	barrier		=
				{
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			                // sType;
					DE_NULL,										                // pNext;

					VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,	                // srcAccessMask
					VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,			                // dstAccessMask

					m_renderInfo.getInputAttachmentLayout(inputAttachmentNdx),      // oldLayout
					m_renderInfo.getInputAttachmentLayout(inputAttachmentNdx),      // newLayout;

					VK_QUEUE_FAMILY_IGNORED,						                // srcQueueFamilyIndex;
					VK_QUEUE_FAMILY_IGNORED,						                // destQueueFamilyIndex;

					m_depthStencilAttachmentImage,					                // image;
					{												                // subresourceRange;
						(hasDepth ? (VkImageAspectFlags)VK_IMAGE_ASPECT_DEPTH_BIT : 0u)
							| (hasStencil ? (VkImageAspectFlags)VK_IMAGE_ASPECT_STENCIL_BIT : 0u),	// aspect;
						0,															// baseMipLevel;
						1,															// mipLevels;
						0,															// baseArraySlice;
						1															// arraySize;
					}
				};

				srcStages |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
				dstStages |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

				selfDeps.push_back(barrier);
			}
		}

		if (!selfDeps.empty())
		{
			DE_ASSERT(srcStages != 0);
			DE_ASSERT(dstStages != 0);
			vk.cmdPipelineBarrier(commandBuffer, srcStages, dstStages, VK_DEPENDENCY_BY_REGION_BIT, 0, DE_NULL, 0, DE_NULL, (deUint32)selfDeps.size(), &selfDeps[0]);
		}

		if (m_renderInfo.getRenderQuad())
		{
			const VkDeviceSize	offset			= 0;
			const VkBuffer		vertexBuffer	= *m_vertexBuffer;

			vk.cmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

			if (m_descriptorSet)
			{
				const VkDescriptorSet descriptorSet = *m_descriptorSet;
				vk.cmdBindDescriptorSets(commandBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayout, 0u, 1u, &descriptorSet, 0u, NULL);
			}

			vk.cmdBindVertexBuffers(commandBuffer, 0u, 1u, &vertexBuffer, &offset);
			vk.cmdDraw(commandBuffer, 6u, 1u, 0u, 0u);
		}
	}

private:
	const SubpassRenderInfo		m_renderInfo;
	Move<VkCommandBuffer>		m_commandBuffer;
	Move<VkPipeline>			m_pipeline;
	Move<VkDescriptorSetLayout>	m_descriptorSetLayout;
	Move<VkPipelineLayout>		m_pipelineLayout;

	Move<VkShaderModule>		m_vertexShaderModule;
	Move<VkShaderModule>		m_fragmentShaderModule;

	Move<VkDescriptorPool>		m_descriptorPool;
	Move<VkDescriptorSet>		m_descriptorSet;
	Move<VkBuffer>				m_vertexBuffer;
	de::MovePtr<Allocation>		m_vertexBufferMemory;
	vector<VkImage>				m_colorAttachmentImages;
	VkImage						m_depthStencilAttachmentImage;
};

void pushImageInitializationCommands (const DeviceInterface&								vk,
									  VkCommandBuffer										commandBuffer,
									  const vector<Attachment>&								attachmentInfo,
									  const vector<de::SharedPtr<AttachmentResources> >&	attachmentResources,
									  deUint32												queueIndex,
									  const vector<Maybe<VkClearValue> >&					clearValues)
{
	{
		vector<VkImageMemoryBarrier>	initializeLayouts;

		for (size_t attachmentNdx = 0; attachmentNdx < attachmentInfo.size(); attachmentNdx++)
		{
			if (!clearValues[attachmentNdx])
				continue;

			const VkImageMemoryBarrier barrier =
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,							// sType;
				DE_NULL,														// pNext;

				(VkAccessFlags)0,												// srcAccessMask
				getAllMemoryReadFlags() | VK_ACCESS_TRANSFER_WRITE_BIT,			// dstAccessMask

				VK_IMAGE_LAYOUT_UNDEFINED,										// oldLayout
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,							// newLayout;

				queueIndex,														// srcQueueFamilyIndex;
				queueIndex,														// destQueueFamilyIndex;

				attachmentResources[attachmentNdx]->getImage(),					// image;
				{																// subresourceRange;
					getImageAspectFlags(attachmentInfo[attachmentNdx].getFormat()),		// aspect;
					0,																	// baseMipLevel;
					1,																	// mipLevels;
					0,																	// baseArraySlice;
					1																	// arraySize;
				}
			};

			initializeLayouts.push_back(barrier);
		}

		if (!initializeLayouts.empty())
			vk.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
								  VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, (VkDependencyFlags)0,
								  0, (const VkMemoryBarrier*)DE_NULL,
								  0, (const VkBufferMemoryBarrier*)DE_NULL,
								  (deUint32)initializeLayouts.size(), &initializeLayouts[0]);
	}

	for (size_t attachmentNdx = 0; attachmentNdx < attachmentInfo.size(); attachmentNdx++)
	{
		if (!clearValues[attachmentNdx])
			continue;

		const tcu::TextureFormat format = mapVkFormat(attachmentInfo[attachmentNdx].getFormat());

		if (hasStencilComponent(format.order) || hasDepthComponent(format.order))
		{
			const float						clearNan		= tcu::Float32::nan().asFloat();
			const float						clearDepth		= hasDepthComponent(format.order) ? clearValues[attachmentNdx]->depthStencil.depth : clearNan;
			const deUint32					clearStencil	= hasStencilComponent(format.order) ? clearValues[attachmentNdx]->depthStencil.stencil : 0xDEu;
			const VkClearDepthStencilValue	depthStencil	=
			{
				clearDepth,
				clearStencil
			};
			const VkImageSubresourceRange range =
			{
				(VkImageAspectFlags)((hasDepthComponent(format.order) ? VK_IMAGE_ASPECT_DEPTH_BIT : 0)
									 | (hasStencilComponent(format.order) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0)),
				0,
				1,
				0,
				1
			};

			vk.cmdClearDepthStencilImage(commandBuffer, attachmentResources[attachmentNdx]->getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &depthStencil, 1, &range);
		}
		else
		{
			const VkImageSubresourceRange	range		=
			{
				VK_IMAGE_ASPECT_COLOR_BIT,	// aspectMask;
				0,							// baseMipLevel;
				1,							// mipLevels;
				0,							// baseArrayLayer;
				1							// layerCount;
			};
			const VkClearColorValue			clearColor	= clearValues[attachmentNdx]->color;

			vk.cmdClearColorImage(commandBuffer, attachmentResources[attachmentNdx]->getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);
		}
	}

	{
		vector<VkImageMemoryBarrier>	renderPassLayouts;

		for (size_t attachmentNdx = 0; attachmentNdx < attachmentInfo.size(); attachmentNdx++)
		{
			const VkImageLayout			oldLayout	= clearValues[attachmentNdx] ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
			const VkImageMemoryBarrier	barrier		=
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,					// sType;
				DE_NULL,												// pNext;

				getMemoryFlagsForLayout(oldLayout),																		// srcAccessMask
				getAllMemoryReadFlags() | getMemoryFlagsForLayout(attachmentInfo[attachmentNdx].getInitialLayout()),	// dstAccessMask

				oldLayout,												// oldLayout
				attachmentInfo[attachmentNdx].getInitialLayout(),		// newLayout;

				queueIndex,												// srcQueueFamilyIndex;
				queueIndex,												// destQueueFamilyIndex;

				attachmentResources[attachmentNdx]->getImage(),			// image;
				{														// subresourceRange;
					getImageAspectFlags(attachmentInfo[attachmentNdx].getFormat()),		// aspect;
					0,																	// baseMipLevel;
					1,																	// mipLevels;
					0,																	// baseArraySlice;
					1																	// arraySize;
				}
			};

			renderPassLayouts.push_back(barrier);
		}

		if (!renderPassLayouts.empty())
			vk.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
								  VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, (VkDependencyFlags)0,
								  0, (const VkMemoryBarrier*)DE_NULL,
								  0, (const VkBufferMemoryBarrier*)DE_NULL,
								  (deUint32)renderPassLayouts.size(), &renderPassLayouts[0]);
	}
}

template<typename RenderpassSubpass>
void pushRenderPassCommands (const DeviceInterface&							vk,
							 VkCommandBuffer								commandBuffer,
							 VkRenderPass									renderPass,
							 VkFramebuffer									framebuffer,
							 const vector<de::SharedPtr<SubpassRenderer> >&	subpassRenderers,
							 const UVec2&									renderPos,
							 const UVec2&									renderSize,
							 const vector<Maybe<VkClearValue> >&			renderPassClearValues,
							 TestConfig::RenderTypes						render)
{
	const float											clearNan				= tcu::Float32::nan().asFloat();
	vector<VkClearValue>								attachmentClearValues;
	const typename RenderpassSubpass::SubpassEndInfo	subpassEndInfo			(DE_NULL);

	for (size_t attachmentNdx = 0; attachmentNdx < renderPassClearValues.size(); attachmentNdx++)
	{
		if (renderPassClearValues[attachmentNdx])
			attachmentClearValues.push_back(*renderPassClearValues[attachmentNdx]);
		else
			attachmentClearValues.push_back(makeClearValueColorF32(clearNan, clearNan, clearNan, clearNan));
	}

	{
		const VkRect2D renderArea =
		{
			{ (deInt32)renderPos.x(),	(deInt32)renderPos.y()	},
			{ renderSize.x(),			renderSize.y()			}
		};

		for (size_t subpassNdx = 0; subpassNdx < subpassRenderers.size(); subpassNdx++)
		{
			const VkSubpassContents								contents			= subpassRenderers[subpassNdx]->isSecondary() ? VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS : VK_SUBPASS_CONTENTS_INLINE;
			const typename RenderpassSubpass::SubpassBeginInfo	subpassBeginInfo	(DE_NULL, contents);
			const VkRenderPassBeginInfo							renderPassBeginInfo	= createRenderPassBeginInfo(renderPass,
																												framebuffer,
																												renderArea,
																												(deUint32)attachmentClearValues.size(),
																												attachmentClearValues.empty() ? DE_NULL : &attachmentClearValues[0]);

			if (subpassNdx == 0)
				RenderpassSubpass::cmdBeginRenderPass(vk, commandBuffer, &renderPassBeginInfo, &subpassBeginInfo);
			else
				RenderpassSubpass::cmdNextSubpass(vk, commandBuffer, &subpassBeginInfo, &subpassEndInfo);

			if (render)
			{
				if (contents == VK_SUBPASS_CONTENTS_INLINE)
				{
					subpassRenderers[subpassNdx]->pushRenderCommands(vk, commandBuffer);
				}
				else if (contents == VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS)
				{
					const VkCommandBuffer cmd = subpassRenderers[subpassNdx]->getCommandBuffer();
					vk.cmdExecuteCommands(commandBuffer, 1, &cmd);
				}
				else
					DE_FATAL("Invalid contents");
			}
		}

		RenderpassSubpass::cmdEndRenderPass(vk, commandBuffer, &subpassEndInfo);
	}
}

void pushRenderPassCommands (const DeviceInterface&							vk,
							 VkCommandBuffer								commandBuffer,
							 VkRenderPass									renderPass,
							 VkFramebuffer									framebuffer,
							 const vector<de::SharedPtr<SubpassRenderer> >&	subpassRenderers,
							 const UVec2&									renderPos,
							 const UVec2&									renderSize,
							 const vector<Maybe<VkClearValue> >&			renderPassClearValues,
							 TestConfig::RenderTypes						render,
							 RenderPassType									renderPassType)
{
	switch (renderPassType)
	{
		case RENDERPASS_TYPE_LEGACY:
			return pushRenderPassCommands<RenderpassSubpass1>(vk, commandBuffer, renderPass, framebuffer, subpassRenderers, renderPos, renderSize, renderPassClearValues, render);
		case RENDERPASS_TYPE_RENDERPASS2:
			return pushRenderPassCommands<RenderpassSubpass2>(vk, commandBuffer, renderPass, framebuffer, subpassRenderers, renderPos, renderSize, renderPassClearValues, render);
		default:
			TCU_THROW(InternalError, "Impossible");
	}
}

void pushReadImagesToBuffers (const DeviceInterface&								vk,
							  VkCommandBuffer										commandBuffer,
							  deUint32												queueIndex,

							  const vector<de::SharedPtr<AttachmentResources> >&	attachmentResources,
							  const vector<Attachment>&								attachmentInfo,
							  const vector<bool>&									isLazy,

							  const UVec2&											targetSize)
{
	{
		vector<VkImageMemoryBarrier>	imageBarriers;

		for (size_t attachmentNdx = 0; attachmentNdx < attachmentInfo.size(); attachmentNdx++)
		{
			if (isLazy[attachmentNdx])
				continue;

			const VkImageLayout			oldLayout	= attachmentInfo[attachmentNdx].getFinalLayout();
			const VkImageMemoryBarrier	barrier		=
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,							// sType
				DE_NULL,														// pNext

				getAllMemoryWriteFlags() | getMemoryFlagsForLayout(oldLayout),	// srcAccessMask
				getAllMemoryReadFlags(),										// dstAccessMask

				oldLayout,														// oldLayout
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,							// newLayout

				queueIndex,														// srcQueueFamilyIndex
				queueIndex,														// destQueueFamilyIndex

				attachmentResources[attachmentNdx]->getImage(),					// image
				{																// subresourceRange
					getImageAspectFlags(attachmentInfo[attachmentNdx].getFormat()),		// aspect;
					0,																	// baseMipLevel
					1,																	// mipLevels
					0,																	// baseArraySlice
					1																	// arraySize
				}
			};

			imageBarriers.push_back(barrier);
		}

		if (!imageBarriers.empty())
			vk.cmdPipelineBarrier(commandBuffer,
								  getAllPipelineStageFlags(),
								  getAllPipelineStageFlags(),
								  (VkDependencyFlags)0,
								  0, (const VkMemoryBarrier*)DE_NULL,
								  0, (const VkBufferMemoryBarrier*)DE_NULL,
								  (deUint32)imageBarriers.size(), &imageBarriers[0]);
	}

	for (size_t attachmentNdx = 0; attachmentNdx < attachmentInfo.size(); attachmentNdx++)
	{
		if (isLazy[attachmentNdx])
			continue;

		const tcu::TextureFormat::ChannelOrder	order	= mapVkFormat(attachmentInfo[attachmentNdx].getFormat()).order;
		const VkBufferImageCopy					rect	=
		{
			0, // bufferOffset
			0, // bufferRowLength
			0, // bufferImageHeight
			{							// imageSubresource
				(vk::VkImageAspectFlags)getPrimaryImageAspect(mapVkFormat(attachmentInfo[attachmentNdx].getFormat()).order),	// aspect
				0,						// mipLevel
				0,						// arraySlice
				1						// arraySize
			},
			{ 0, 0, 0 },				// imageOffset
			{ targetSize.x(), targetSize.y(), 1u }		// imageExtent
		};

		vk.cmdCopyImageToBuffer(commandBuffer, attachmentResources[attachmentNdx]->getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, attachmentResources[attachmentNdx]->getBuffer(), 1, &rect);

		if (tcu::TextureFormat::DS == order)
		{
			const VkBufferImageCopy stencilRect =
			{
				0,										// bufferOffset
				0,										// bufferRowLength
				0,										// bufferImageHeight
				{									// imageSubresource
					VK_IMAGE_ASPECT_STENCIL_BIT,	// aspect
					0,								// mipLevel
					0,								// arraySlice
					1								// arraySize
				},
				{ 0, 0, 0 },							// imageOffset
				{ targetSize.x(), targetSize.y(), 1u }	// imageExtent
			};

			vk.cmdCopyImageToBuffer(commandBuffer, attachmentResources[attachmentNdx]->getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, attachmentResources[attachmentNdx]->getSecondaryBuffer(), 1, &stencilRect);
		}
	}

	{
		vector<VkBufferMemoryBarrier>	bufferBarriers;

		for (size_t attachmentNdx = 0; attachmentNdx < attachmentInfo.size(); attachmentNdx++)
		{
			if (isLazy[attachmentNdx])
				continue;

			const tcu::TextureFormat::ChannelOrder	order			= mapVkFormat(attachmentInfo[attachmentNdx].getFormat()).order;
			const VkBufferMemoryBarrier				bufferBarrier	=
			{
				VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
				DE_NULL,

				getAllMemoryWriteFlags(),
				getAllMemoryReadFlags(),

				queueIndex,
				queueIndex,

				attachmentResources[attachmentNdx]->getBuffer(),
				0,
				attachmentResources[attachmentNdx]->getBufferSize()
			};

			bufferBarriers.push_back(bufferBarrier);

			if (tcu::TextureFormat::DS == order)
			{
				const VkBufferMemoryBarrier secondaryBufferBarrier =
				{
					VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
					DE_NULL,

					getAllMemoryWriteFlags(),
					getAllMemoryReadFlags(),

					queueIndex,
					queueIndex,

					attachmentResources[attachmentNdx]->getSecondaryBuffer(),
					0,
					attachmentResources[attachmentNdx]->getSecondaryBufferSize()
				};

				bufferBarriers.push_back(secondaryBufferBarrier);
			}
		}

		if (!bufferBarriers.empty())
			vk.cmdPipelineBarrier(commandBuffer,
								  getAllPipelineStageFlags(),
								  getAllPipelineStageFlags(),
								  (VkDependencyFlags)0,
								  0, (const VkMemoryBarrier*)DE_NULL,
								  (deUint32)bufferBarriers.size(), &bufferBarriers[0],
								  0, (const VkImageMemoryBarrier*)DE_NULL);
	}
}

class PixelValue
{
public:
				PixelValue		(const Maybe<bool>&	x = nothing<bool>(),
								 const Maybe<bool>&	y = nothing<bool>(),
								 const Maybe<bool>&	z = nothing<bool>(),
								 const Maybe<bool>&	w = nothing<bool>());

	void		setUndefined	(size_t ndx);
	void		setValue		(size_t ndx, bool value);
	Maybe<bool>	getValue		(size_t ndx) const;

private:
	deUint16	m_status;
};

PixelValue::PixelValue (const Maybe<bool>&	x,
						const Maybe<bool>&	y,
						const Maybe<bool>&	z,
						const Maybe<bool>&	w)
	: m_status (0)
{
	const Maybe<bool> values[] =
	{
		x, y, z, w
	};

	for (size_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(values); ndx++)
	{
		if (values[ndx])
			setValue(ndx, *values[ndx]);
		else
			setUndefined(ndx);
	}

	DE_ASSERT(m_status <= 0xFFu);
}

void PixelValue::setUndefined (size_t ndx)
{
	DE_ASSERT(ndx < 4);
	DE_ASSERT(m_status <= 0xFFu);

	m_status &= (deUint16)~(0x1u << (deUint16)(ndx * 2));
	DE_ASSERT(m_status <= 0xFFu);
}

void PixelValue::setValue (size_t ndx, bool value)
{
	DE_ASSERT(ndx < 4);
	DE_ASSERT(m_status <= 0xFFu);

	m_status = (deUint16)(m_status | (deUint16)(0x1u << (ndx * 2)));

	if (value)
		m_status = (deUint16)(m_status | (deUint16)(0x1u << (ndx * 2 + 1)));
	else
		m_status &= (deUint16)~(0x1u << (deUint16)(ndx * 2 + 1));

	DE_ASSERT(m_status <= 0xFFu);
}

Maybe<bool> PixelValue::getValue (size_t ndx) const
{
	DE_ASSERT(ndx < 4);
	DE_ASSERT(m_status <= 0xFFu);

	if ((m_status & (0x1u << (deUint16)(ndx * 2))) != 0)
	{
		return just((m_status & (0x1u << (deUint32)(ndx * 2 + 1))) != 0);
	}
	else
		return nothing<bool>();
}

void clearReferenceValues (vector<PixelValue>&	values,
						   const UVec2&			targetSize,
						   const UVec2&			offset,
						   const UVec2&			size,
						   const BVec4&			mask,
						   const PixelValue&	value)
{
	DE_ASSERT(targetSize.x() * targetSize.y() == (deUint32)values.size());
	DE_ASSERT(offset.x() + size.x() <= targetSize.x());
	DE_ASSERT(offset.y() + size.y() <= targetSize.y());

	for (deUint32 y = offset.y(); y < offset.y() + size.y(); y++)
	for (deUint32 x = offset.x(); x < offset.x() + size.x(); x++)
	{
		for (int compNdx = 0; compNdx < 4; compNdx++)
		{
			if (mask[compNdx])
			{
				if (value.getValue(compNdx))
					values[x + y * targetSize.x()].setValue(compNdx, *value.getValue(compNdx));
				else
					values[x + y * targetSize.x()].setUndefined(compNdx);
			}
		}
	}
}

void markUndefined (vector<PixelValue>&	values,
					const BVec4&		mask,
					const UVec2&		targetSize,
					const UVec2&		offset,
					const UVec2&		size)
{
	DE_ASSERT(targetSize.x() * targetSize.y() == (deUint32)values.size());

	for (deUint32 y = offset.y(); y < offset.y() + size.y(); y++)
	for (deUint32 x = offset.x(); x < offset.x() + size.x(); x++)
	{
		for (int compNdx = 0; compNdx < 4; compNdx++)
		{
			if (mask[compNdx])
				values[x + y * targetSize.x()].setUndefined(compNdx);
		}
	}
}

PixelValue clearValueToPixelValue (const VkClearValue&			value,
								   const tcu::TextureFormat&	format,
								   const DepthValuesArray&		depthValues)
{
	const bool	isDepthAttachment			= hasDepthComponent(format.order);
	const bool	isStencilAttachment			= hasStencilComponent(format.order);
	const bool	isDepthOrStencilAttachment	= isDepthAttachment || isStencilAttachment;
	PixelValue	pixelValue;

	if (isDepthOrStencilAttachment)
	{
		if (isDepthAttachment)
		{
			if (value.depthStencil.depth == float(depthValues[1]) / 255.0f)
				pixelValue.setValue(0, true);
			else if (value.depthStencil.depth == float(depthValues[0]) / 255.0f)
				pixelValue.setValue(0, false);
			else
				DE_FATAL("Unknown depth value");
		}

		if (isStencilAttachment)
		{
			if (value.depthStencil.stencil == 0xFFu)
				pixelValue.setValue(1, true);
			else if (value.depthStencil.stencil == 0x0u)
				pixelValue.setValue(1, false);
			else
				DE_FATAL("Unknown stencil value");
		}
	}
	else
	{
		const tcu::TextureChannelClass	channelClass	= tcu::getTextureChannelClass(format.type);
		const tcu::BVec4				channelMask		= tcu::getTextureFormatChannelMask(format);

		switch (channelClass)
		{
			case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
				for (int i = 0; i < 4; i++)
				{
					if (channelMask[i])
					{
						if (value.color.int32[i] == 1)
							pixelValue.setValue(i, true);
						else if (value.color.int32[i] == 0)
							pixelValue.setValue(i, false);
						else
							DE_FATAL("Unknown clear color value");
					}
				}
				break;

			case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
				for (int i = 0; i < 4; i++)
				{
					if (channelMask[i])
					{
						if (value.color.uint32[i] == 1u)
							pixelValue.setValue(i, true);
						else if (value.color.uint32[i] == 0u)
							pixelValue.setValue(i, false);
						else
							DE_FATAL("Unknown clear color value");
					}
				}
				break;

			case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
			case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
			case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
				for (int i = 0; i < 4; i++)
				{
					if (channelMask[i])
					{
						if (value.color.float32[i] == 1.0f)
							pixelValue.setValue(i, true);
						else if (value.color.float32[i] == 0.0f)
							pixelValue.setValue(i, false);
						else
							DE_FATAL("Unknown clear color value");
					}
				}
				break;

			default:
				DE_FATAL("Unknown channel class");
		}
	}

	return pixelValue;
}

void renderReferenceValues (vector<vector<PixelValue> >&		referenceAttachments,
							const RenderPass&					renderPassInfo,
							const UVec2&						targetSize,
							const vector<Maybe<VkClearValue> >&	imageClearValues,
							const vector<Maybe<VkClearValue> >&	renderPassClearValues,
							const vector<SubpassRenderInfo>&	subpassRenderInfo,
							const UVec2&						renderPos,
							const UVec2&						renderSize,
							const deUint32						drawStartNdx,
							const DepthValuesArray&				depthValues)
{
	const vector<Subpass>&	subpasses		= renderPassInfo.getSubpasses();
	vector<bool>			attachmentUsed	(renderPassInfo.getAttachments().size(), false);

	referenceAttachments.resize(renderPassInfo.getAttachments().size());

	for (size_t attachmentNdx = 0; attachmentNdx < renderPassInfo.getAttachments().size(); attachmentNdx++)
	{
		const Attachment			attachment	= renderPassInfo.getAttachments()[attachmentNdx];
		const tcu::TextureFormat	format		= mapVkFormat(attachment.getFormat());
		vector<PixelValue>&			reference	= referenceAttachments[attachmentNdx];

		reference.resize(targetSize.x() * targetSize.y());

		if (imageClearValues[attachmentNdx])
			clearReferenceValues(reference, targetSize, UVec2(0, 0), targetSize, BVec4(true), clearValueToPixelValue(*imageClearValues[attachmentNdx], format, depthValues));
	}

	for (size_t subpassNdx = 0; subpassNdx < subpasses.size(); subpassNdx++)
	{
		const Subpass&						subpass				= subpasses[subpassNdx];
		const SubpassRenderInfo&			renderInfo			= subpassRenderInfo[subpassNdx];
		const vector<AttachmentReference>&	colorAttachments	= subpass.getColorAttachments();

		// Apply load op if attachment was used for the first time
		for (size_t attachmentNdx = 0; attachmentNdx < colorAttachments.size(); attachmentNdx++)
		{
			const deUint32 attachmentIndex = getAttachmentNdx(colorAttachments, attachmentNdx);

			if (!attachmentUsed[attachmentIndex] && colorAttachments[attachmentNdx].getAttachment() != VK_ATTACHMENT_UNUSED)
			{
				const Attachment&			attachment	= renderPassInfo.getAttachments()[attachmentIndex];
				vector<PixelValue>&			reference	= referenceAttachments[attachmentIndex];
				const tcu::TextureFormat	format		= mapVkFormat(attachment.getFormat());

				DE_ASSERT(!tcu::hasDepthComponent(format.order));
				DE_ASSERT(!tcu::hasStencilComponent(format.order));

				if (attachment.getLoadOp() == VK_ATTACHMENT_LOAD_OP_CLEAR)
					clearReferenceValues(reference, targetSize, renderPos, renderSize, BVec4(true), clearValueToPixelValue(*renderPassClearValues[attachmentIndex], format, depthValues));
				else if (attachment.getLoadOp() == VK_ATTACHMENT_LOAD_OP_DONT_CARE)
					markUndefined(reference, BVec4(true), targetSize, renderPos, renderSize);

				attachmentUsed[attachmentIndex] = true;
			}
		}

		// Apply load op to depth/stencil attachment if it was used for the first time
		if (subpass.getDepthStencilAttachment().getAttachment() != VK_ATTACHMENT_UNUSED)
		{
			const deUint32 attachmentIndex = subpass.getDepthStencilAttachment().getAttachment();

			// Apply load op if attachment was used for the first time
			if (!attachmentUsed[attachmentIndex])
			{
				const Attachment&			attachment	= renderPassInfo.getAttachments()[attachmentIndex];
				vector<PixelValue>&			reference	= referenceAttachments[attachmentIndex];
				const tcu::TextureFormat	format		= mapVkFormat(attachment.getFormat());

				if (tcu::hasDepthComponent(format.order))
				{
					if (attachment.getLoadOp() == VK_ATTACHMENT_LOAD_OP_CLEAR)
						clearReferenceValues(reference, targetSize, renderPos, renderSize, BVec4(true, false, false, false), clearValueToPixelValue(*renderPassClearValues[attachmentIndex], format, depthValues));
					else if (attachment.getLoadOp() == VK_ATTACHMENT_LOAD_OP_DONT_CARE)
						markUndefined(reference, BVec4(true, false, false, false), targetSize, renderPos, renderSize);
				}

				if (tcu::hasStencilComponent(format.order))
				{
					if (attachment.getStencilLoadOp() == VK_ATTACHMENT_LOAD_OP_CLEAR)
						clearReferenceValues(reference, targetSize, renderPos, renderSize, BVec4(false, true, false, false), clearValueToPixelValue(*renderPassClearValues[attachmentIndex], format, depthValues));
					else if (attachment.getStencilLoadOp() == VK_ATTACHMENT_LOAD_OP_DONT_CARE)
						markUndefined(reference, BVec4(false, true, false, false), targetSize, renderPos, renderSize);
				}

				attachmentUsed[attachmentIndex] = true;
			}
		}

		for (size_t colorClearNdx = 0; colorClearNdx < renderInfo.getColorClears().size(); colorClearNdx++)
		{
			const ColorClear&			colorClear		= renderInfo.getColorClears()[colorClearNdx];
			const UVec2					offset			= colorClear.getOffset();
			const UVec2					size			= colorClear.getSize();
			const deUint32				attachmentIndex	= subpass.getColorAttachments()[colorClearNdx].getAttachment();
			const Attachment&			attachment		= renderPassInfo.getAttachments()[attachmentIndex];
			const tcu::TextureFormat	format			= mapVkFormat(attachment.getFormat());
			vector<PixelValue>&			reference		= referenceAttachments[attachmentIndex];
			VkClearValue				value;

			value.color = colorClear.getColor();

			clearReferenceValues(reference, targetSize, offset, size, BVec4(true), clearValueToPixelValue(value, format, depthValues));
		}

		if (renderInfo.getDepthStencilClear())
		{
			const DepthStencilClear&	dsClear			= *renderInfo.getDepthStencilClear();
			const UVec2					offset			= dsClear.getOffset();
			const UVec2					size			= dsClear.getSize();
			const deUint32				attachmentIndex	= subpass.getDepthStencilAttachment().getAttachment();
			const VkImageLayout			layout			= subpass.getDepthStencilAttachment().getImageLayout();
			const Attachment&			attachment		= renderPassInfo.getAttachments()[attachmentIndex];
			const tcu::TextureFormat	format			= mapVkFormat(attachment.getFormat());
			const bool					hasStencil		= tcu::hasStencilComponent(format.order)
														&& layout != VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL;
			const bool					hasDepth		= tcu::hasDepthComponent(format.order)
														&& layout != VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;
			vector<PixelValue>&			reference		= referenceAttachments[attachmentIndex];
			VkClearValue				value;

			value.depthStencil.depth = dsClear.getDepth();
			value.depthStencil.stencil = dsClear.getStencil();

			clearReferenceValues(reference, targetSize, offset, size, BVec4(hasDepth, hasStencil, false, false), clearValueToPixelValue(value, format, depthValues));
		}

		if (renderInfo.getRenderQuad())
		{
			const RenderQuad&	renderQuad	= *renderInfo.getRenderQuad();
			const Vec2			posA		= renderQuad.getCornerA();
			const Vec2			posB		= renderQuad.getCornerB();
			const Vec2			origin		= Vec2((float)renderInfo.getViewportOffset().x(), (float)renderInfo.getViewportOffset().y()) + Vec2((float)renderInfo.getViewportSize().x(), (float)renderInfo.getViewportSize().y()) / Vec2(2.0f);
			const Vec2			p			= Vec2((float)renderInfo.getViewportSize().x(), (float)renderInfo.getViewportSize().y()) / Vec2(2.0f);
			const IVec2			posAI		(deRoundFloatToInt32(origin.x() + (p.x() * posA.x())),
											 deRoundFloatToInt32(origin.y() + (p.y() * posA.y())));
			const IVec2			posBI		(deRoundFloatToInt32(origin.x() + (p.x() * posB.x())),
											 deRoundFloatToInt32(origin.y() + (p.y() * posB.y())));

			DE_ASSERT(posAI.x() < posBI.x());
			DE_ASSERT(posAI.y() < posBI.y());

			if (subpass.getInputAttachments().empty())
			{
				for (size_t attachmentRefNdx = drawStartNdx; attachmentRefNdx < subpass.getColorAttachments().size(); attachmentRefNdx++)
				{
					const deUint32				attachmentIndex	= subpass.getColorAttachments()[attachmentRefNdx].getAttachment();

					if (attachmentIndex == VK_ATTACHMENT_UNUSED)
						continue;

					const Attachment&			attachment		= renderPassInfo.getAttachments()[attachmentIndex];
					const tcu::TextureFormat	format			= mapVkFormat(attachment.getFormat());
					const tcu::BVec4			channelMask		= tcu::getTextureFormatChannelMask(format);
					vector<PixelValue>&			reference		= referenceAttachments[attachmentIndex];

					for (int y = posAI.y(); y < (int)posBI.y(); y++)
					for (int x = posAI.x(); x < (int)posBI.x(); x++)
					{
						for (int compNdx = 0; compNdx < 4; compNdx++)
						{
							const size_t	index	= subpassNdx + attachmentIndex + compNdx;
							const BoolOp	op		= boolOpFromIndex(index);
							const bool		boolX	= x % 2 == (int)(index % 2);
							const bool		boolY	= y % 2 == (int)((index / 2) % 2);

							if (channelMask[compNdx])
								reference[x + y * targetSize.x()].setValue(compNdx, performBoolOp(op, boolX, boolY));
						}
					}
				}

				if (subpass.getDepthStencilAttachment().getAttachment() != VK_ATTACHMENT_UNUSED)
				{
					const deUint32				attachmentIndex	= subpass.getDepthStencilAttachment().getAttachment();
					const VkImageLayout			layout			= subpass.getDepthStencilAttachment().getImageLayout();
					const Attachment&			attachment		= renderPassInfo.getAttachments()[attachmentIndex];
					const tcu::TextureFormat	format			= mapVkFormat(attachment.getFormat());
					vector<PixelValue>&			reference		= referenceAttachments[attachmentIndex];

					for (int y = posAI.y(); y < (int)posBI.y(); y++)
					for (int x = posAI.x(); x < (int)posBI.x(); x++)
					{
						if (tcu::hasDepthComponent(format.order)
							&& layout != VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
							&& layout != VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL)
						{
							const size_t	index	= subpassNdx + 1;
							const BoolOp	op		= boolOpFromIndex(index);
							const bool		boolX	= x % 2 == (int)(index % 2);
							const bool		boolY	= y % 2 == (int)((index / 2) % 2);

							reference[x + y * targetSize.x()].setValue(0, performBoolOp(op, boolX, boolY));
						}

						if (tcu::hasStencilComponent(format.order)
							&& layout != VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
							&& layout != VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL)
						{
							const size_t	index	= subpassNdx;
							reference[x + y * targetSize.x()].setValue(1, (index % 2) == 0);
						}
					}
				}
			}
			else
			{
				size_t					outputComponentCount	= 0;
				vector<Maybe<bool> >	inputs;

				DE_ASSERT(posAI.x() < posBI.x());
				DE_ASSERT(posAI.y() < posBI.y());

				for (size_t attachmentRefNdx = 0; attachmentRefNdx < subpass.getColorAttachments().size(); attachmentRefNdx++)
				{
					const deUint32				attachmentIndex	= subpass.getColorAttachments()[attachmentRefNdx].getAttachment();
					const Attachment&			attachment		= renderPassInfo.getAttachments()[attachmentIndex];
					const tcu::TextureFormat	format			= mapVkFormat(attachment.getFormat());
					const int					componentCount	= tcu::getNumUsedChannels(format.order);

					outputComponentCount += (size_t)componentCount;
				}

				if (subpass.getDepthStencilAttachment().getAttachment() != VK_ATTACHMENT_UNUSED
					&& subpass.getDepthStencilAttachment().getImageLayout() != VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
					&& subpass.getDepthStencilAttachment().getImageLayout() != VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL)
				{
					const Attachment&			attachment	(renderPassInfo.getAttachments()[subpass.getDepthStencilAttachment().getAttachment()]);
					const tcu::TextureFormat	format		(mapVkFormat(attachment.getFormat()));

					if (tcu::hasDepthComponent(format.order))
						outputComponentCount++;
				}

				if (outputComponentCount > 0)
				{
					for (int y = posAI.y(); y < (int)posBI.y(); y++)
					for (int x = posAI.x(); x < (int)posBI.x(); x++)
					{
						for (size_t inputAttachmentNdx = 0; inputAttachmentNdx < subpass.getInputAttachments().size(); inputAttachmentNdx++)
						{
							const deUint32				attachmentIndex	= subpass.getInputAttachments()[inputAttachmentNdx].getAttachment();
							const VkImageLayout			layout			= subpass.getInputAttachments()[inputAttachmentNdx].getImageLayout();
							const Attachment&			attachment		= renderPassInfo.getAttachments()[attachmentIndex];
							const tcu::TextureFormat	format			= mapVkFormat(attachment.getFormat());
							const int					componentCount	= tcu::getNumUsedChannels(format.order);

							for (int compNdx = 0; compNdx < componentCount; compNdx++)
							{
								if ((compNdx != 0 || layout != VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL)
									&& (compNdx != 1 || layout != VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL))
								{
									inputs.push_back(referenceAttachments[attachmentIndex][x + y * targetSize.x()].getValue(compNdx));
								}
							}
						}

						const size_t inputsPerOutput = inputs.size() >= outputComponentCount
														? ((inputs.size() / outputComponentCount)
															+ ((inputs.size() % outputComponentCount) != 0 ? 1 : 0))
														: 1;

						size_t outputValueNdx = 0;

						for (size_t attachmentRefNdx = 0; attachmentRefNdx < subpass.getColorAttachments().size(); attachmentRefNdx++)
						{
							const deUint32				attachmentIndex	= subpass.getColorAttachments()[attachmentRefNdx].getAttachment();
							const Attachment&			attachment		= renderPassInfo.getAttachments()[attachmentIndex];
							const tcu::TextureFormat	format			= mapVkFormat(attachment.getFormat());
							vector<PixelValue>&			reference		= referenceAttachments[attachmentIndex];
							const int					componentCount	= tcu::getNumUsedChannels(format.order);

							for (int compNdx = 0; compNdx < componentCount; compNdx++)
							{
								const size_t	index	= subpassNdx + attachmentIndex + outputValueNdx;
								const BoolOp	op		= boolOpFromIndex(index);
								const bool		boolX	= x % 2 == (int)(index % 2);
								const bool		boolY	= y % 2 == (int)((index / 2) % 2);
								Maybe<bool>		output	= tcu::just(performBoolOp(op, boolX, boolY));

								for (size_t i = 0; i < inputsPerOutput; i++)
								{
									if (!output)
										break;
									else if (!inputs[((outputValueNdx + compNdx) * inputsPerOutput + i) % inputs.size()])
										output = tcu::nothing<bool>();
									else
										output = (*output) == (*inputs[((outputValueNdx + compNdx) * inputsPerOutput + i) % inputs.size()]);
								}

								if (output)
									reference[x + y * targetSize.x()].setValue(compNdx, *output);
								else
									reference[x + y * targetSize.x()].setUndefined(compNdx);
							}

							outputValueNdx += componentCount;
						}

						if (subpass.getDepthStencilAttachment().getAttachment() != VK_ATTACHMENT_UNUSED
							&& subpass.getDepthStencilAttachment().getImageLayout() != VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
							&& subpass.getDepthStencilAttachment().getImageLayout() != VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL)
						{
							const deUint32		attachmentIndex	= subpass.getDepthStencilAttachment().getAttachment();
							vector<PixelValue>&	reference		= referenceAttachments[attachmentIndex];
							const size_t		index			= subpassNdx + attachmentIndex;
							const BoolOp		op				= boolOpFromIndex(index);
							const bool			boolX			= x % 2 == (int)(index % 2);
							const bool			boolY			= y % 2 == (int)((index / 2) % 2);
							Maybe<bool>			output			= tcu::just(performBoolOp(op, boolX, boolY));

							for (size_t i = 0; i < inputsPerOutput; i++)
							{
								if (!output)
									break;
								else if (inputs[(outputValueNdx * inputsPerOutput + i) % inputs.size()])
									output = (*output) == (*inputs[(outputValueNdx * inputsPerOutput + i) % inputs.size()]);
								else
									output = tcu::nothing<bool>();
							}

							if (output)
								reference[x + y * targetSize.x()].setValue(0, *output);
							else
								reference[x + y * targetSize.x()].setUndefined(0);
						}

						inputs.clear();
					}
				}

				if (subpass.getDepthStencilAttachment().getAttachment() != VK_ATTACHMENT_UNUSED
					&& subpass.getDepthStencilAttachment().getImageLayout() != VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
					&& subpass.getDepthStencilAttachment().getImageLayout() != VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL)
				{
					const deUint32				attachmentIndex	= subpass.getDepthStencilAttachment().getAttachment();
					const Attachment&			attachment		= renderPassInfo.getAttachments()[attachmentIndex];
					const tcu::TextureFormat	format			= mapVkFormat(attachment.getFormat());
					vector<PixelValue>&			reference		= referenceAttachments[attachmentIndex];

					if (tcu::hasStencilComponent(format.order))
					{
						for (int y = posAI.y(); y < (int)posBI.y(); y++)
						for (int x = posAI.x(); x < (int)posBI.x(); x++)
						{
							const size_t	index	= subpassNdx;
							reference[x + y * targetSize.x()].setValue(1, (index % 2) == 0);
						}
					}
				}
			}
		}
	}

	// Mark all attachments that were used but not stored as undefined
	for (size_t attachmentIndex = 0; attachmentIndex < renderPassInfo.getAttachments().size(); attachmentIndex++)
	{
		const Attachment			attachment					= renderPassInfo.getAttachments()[attachmentIndex];
		const tcu::TextureFormat	format						= mapVkFormat(attachment.getFormat());
		vector<PixelValue>&			reference					= referenceAttachments[attachmentIndex];
		const bool					isStencilAttachment			= hasStencilComponent(format.order);
		const bool					isDepthOrStencilAttachment	= hasDepthComponent(format.order) || isStencilAttachment;

		if (attachmentUsed[attachmentIndex] && renderPassInfo.getAttachments()[attachmentIndex].getStoreOp() == VK_ATTACHMENT_STORE_OP_DONT_CARE)
		{
			if (isDepthOrStencilAttachment)
				markUndefined(reference, BVec4(true, false, false, false), targetSize, renderPos, renderSize);
			else
				markUndefined(reference, BVec4(true), targetSize, renderPos, renderSize);
		}

		if (attachmentUsed[attachmentIndex] && isStencilAttachment && renderPassInfo.getAttachments()[attachmentIndex].getStencilStoreOp() == VK_ATTACHMENT_STORE_OP_DONT_CARE)
			markUndefined(reference, BVec4(false, true, false, false), targetSize, renderPos, renderSize);
	}
}

void renderReferenceImagesFromValues (vector<tcu::TextureLevel>&			referenceImages,
									  const vector<vector<PixelValue> >&	referenceValues,
									  const UVec2&							targetSize,
									  const RenderPass&						renderPassInfo,
									  const DepthValuesArray&				depthValues)
{
	referenceImages.resize(referenceValues.size());

	for (size_t attachmentNdx = 0; attachmentNdx < renderPassInfo.getAttachments().size(); attachmentNdx++)
	{
		const Attachment			attachment			= renderPassInfo.getAttachments()[attachmentNdx];
		const tcu::TextureFormat	format				= mapVkFormat(attachment.getFormat());
		const vector<PixelValue>&	reference			= referenceValues[attachmentNdx];
		const bool					hasDepth			= tcu::hasDepthComponent(format.order);
		const bool					hasStencil			= tcu::hasStencilComponent(format.order);
		const bool					hasDepthOrStencil	= hasDepth || hasStencil;
		tcu::TextureLevel&			referenceImage		= referenceImages[attachmentNdx];

		referenceImage.setStorage(format, targetSize.x(), targetSize.y());

		if (hasDepthOrStencil)
		{
			if (hasDepth)
			{
				const PixelBufferAccess depthAccess (tcu::getEffectiveDepthStencilAccess(referenceImage.getAccess(), tcu::Sampler::MODE_DEPTH));

				for (deUint32 y = 0; y < targetSize.y(); y++)
				for (deUint32 x = 0; x < targetSize.x(); x++)
				{
					if (reference[x + y * targetSize.x()].getValue(0))
					{
						if (*reference[x + y * targetSize.x()].getValue(0))
							depthAccess.setPixDepth(float(depthValues[1]) / 255.0f, x, y);
						else
							depthAccess.setPixDepth(float(depthValues[0]) / 255.0f, x, y);
					}
					else // Fill with 3x3 grid
						depthAccess.setPixDepth(((x / 3) % 2) == ((y / 3) % 2) ? 0.33f : 0.66f, x, y);
				}
			}

			if (hasStencil)
			{
				const PixelBufferAccess stencilAccess (tcu::getEffectiveDepthStencilAccess(referenceImage.getAccess(), tcu::Sampler::MODE_STENCIL));

				for (deUint32 y = 0; y < targetSize.y(); y++)
				for (deUint32 x = 0; x < targetSize.x(); x++)
				{
					if (reference[x + y * targetSize.x()].getValue(1))
					{
						if (*reference[x + y * targetSize.x()].getValue(1))
							stencilAccess.setPixStencil(0xFFu, x, y);
						else
							stencilAccess.setPixStencil(0x0u, x, y);
					}
					else // Fill with 3x3 grid
						stencilAccess.setPixStencil(((x / 3) % 2) == ((y / 3) % 2) ? 85 : 170, x, y);
				}
			}
		}
		else
		{
			for (deUint32 y = 0; y < targetSize.y(); y++)
			for (deUint32 x = 0; x < targetSize.x(); x++)
			{
				tcu::Vec4 color;

				for (int compNdx = 0; compNdx < 4; compNdx++)
				{
					if (reference[x + y * targetSize.x()].getValue(compNdx))
					{
						if (*reference[x + y * targetSize.x()].getValue(compNdx))
							color[compNdx] = 1.0f;
						else
							color[compNdx] = 0.0f;
					}
					else // Fill with 3x3 grid
						color[compNdx] = ((compNdx + (x / 3)) % 2) == ((y / 3) % 2) ? 0.33f : 0.66f;
				}

				referenceImage.getAccess().setPixel(color, x, y);
			}
		}
	}
}

bool verifyColorAttachment (const vector<PixelValue>&		reference,
							const ConstPixelBufferAccess&	result,
							const PixelBufferAccess&		errorImage,
							const deBool					useFormatCompCount)
{
	const Vec4	red		(1.0f, 0.0f, 0.0f, 1.0f);
	const Vec4	green	(0.0f, 1.0f, 0.0f, 1.0f);
	bool		ok		= true;

	DE_ASSERT(result.getWidth() * result.getHeight() == (int)reference.size());
	DE_ASSERT(result.getWidth() == errorImage.getWidth());
	DE_ASSERT(result.getHeight() == errorImage.getHeight());

	for (int y = 0; y < result.getHeight(); y++)
	for (int x = 0; x < result.getWidth(); x++)
	{
		const Vec4			resultColor		= result.getPixel(x, y);
		const PixelValue&	referenceValue	= reference[x + y * result.getWidth()];
		bool				pixelOk			= true;
		const deUint32		componentCount	= useFormatCompCount ? (deUint32)tcu::getNumUsedChannels(result.getFormat().order) : 4;

		for (deUint32 compNdx = 0; compNdx < componentCount; compNdx++)
		{
			const Maybe<bool> maybeValue = referenceValue.getValue(compNdx);

			if (maybeValue)
			{
				const bool value = *maybeValue;

				if ((value && (resultColor[compNdx] != 1.0f))
					|| (!value && resultColor[compNdx] != 0.0f))
					pixelOk = false;
			}
		}

		if (!pixelOk)
		{
			errorImage.setPixel(red, x, y);
			ok = false;
		}
		else
			errorImage.setPixel(green, x, y);
	}

	return ok;
}

// Setting the alpha value to 1.0f by default helps visualization when the alpha channel is not used.
const tcu::Vec4	kDefaultColorForLog	{0.0f, 0.0f, 0.0f, 1.0f};
const float		kTrueComponent		= 1.0f;
const float		kFalseComponent		= 0.5f;
const float		kUnsetComponentLow	= 0.0f;
const float		kUnsetComponentHigh	= 0.25f;

std::unique_ptr<tcu::TextureLevel> renderColorImageForLog (const ConstPixelBufferAccess& image, int numChannels)
{
	// Same channel order, but using UNORM_INT8 for the color format.
	const auto							order			= image.getFormat().order;
	const tcu::TextureFormat			loggableFormat	{order, tcu::TextureFormat::UNORM_INT8};
	const int							width			= image.getWidth();
	const int							height			= image.getHeight();
	std::unique_ptr<tcu::TextureLevel>	result			{new tcu::TextureLevel{loggableFormat, width, height}};
	auto								access			= result->getAccess();
	tcu::Vec4							outColor		= kDefaultColorForLog;

	for (int x = 0; x < width; ++x)
	for (int y = 0; y < height; ++y)
	{
		const auto value = image.getPixel(x, y);
		for (int c = 0; c < numChannels; ++c)
		{
			if (value[c] == 0.0f)
				outColor[c] = kFalseComponent;
			else if (value[c] == 1.0f)
				outColor[c] = kTrueComponent;
			else
				DE_ASSERT(false);
		}
		access.setPixel(outColor, x, y);
	}

	return result;
}

std::unique_ptr<tcu::TextureLevel> renderColorImageForLog (const vector<PixelValue>& reference, const UVec2& targetSize, int numChannels)
{
	const tcu::TextureFormat			loggableFormat	{tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8};
	const int							width			= static_cast<int>(targetSize.x());
	const int							height			= static_cast<int>(targetSize.y());
	std::unique_ptr<tcu::TextureLevel>	result			{new tcu::TextureLevel{loggableFormat, width, height}};
	auto								access			= result->getAccess();
	tcu::Vec4							outColor		= kDefaultColorForLog;

	for (int x = 0; x < width; ++x)
	for (int y = 0; y < height; ++y)
	{
		const int index = x + y * width;
		for (int c = 0; c < numChannels; ++c)
		{
			const auto maybeValue = reference[index].getValue(c);
			if (maybeValue)
				outColor[c] = ((*maybeValue) ? kTrueComponent : kFalseComponent);
			else
				outColor[c] = ((((x / 3) % 2) == ((y / 3) % 2)) ? kUnsetComponentLow : kUnsetComponentHigh);
		}
		access.setPixel(outColor, x, y);
	}

	return result;
}

bool verifyDepthAttachment (const vector<PixelValue>&		reference,
							const ConstPixelBufferAccess&	result,
							const PixelBufferAccess&		errorImage,
							const DepthValuesArray&			depthValues,
							float							epsilon)
{
	const Vec4	red		(1.0f, 0.0f, 0.0f, 1.0f);
	const Vec4	green	(0.0f, 1.0f, 0.0f, 1.0f);
	bool		ok		= true;

	DE_ASSERT(result.getWidth() * result.getHeight() == (int)reference.size());
	DE_ASSERT(result.getWidth() == errorImage.getWidth());
	DE_ASSERT(result.getHeight() == errorImage.getHeight());

	for (int y = 0; y < result.getHeight(); y++)
	for (int x = 0; x < result.getWidth(); x++)
	{
		bool pixelOk = true;

		const float			resultDepth		= result.getPixDepth(x, y);
		const PixelValue&	referenceValue	= reference[x + y * result.getWidth()];
		const Maybe<bool>	maybeValue		= referenceValue.getValue(0);

		if (maybeValue)
		{
			const bool value = *maybeValue;

			if ((value && !depthsEqual(resultDepth, float(depthValues[1]) / 255.0f, epsilon))
				|| (!value && !depthsEqual(resultDepth, float(depthValues[0]) / 255.0f, epsilon)))
				pixelOk = false;
		}

		if (!pixelOk)
		{
			errorImage.setPixel(red, x, y);
			ok = false;
		}
		else
			errorImage.setPixel(green, x, y);
	}

	return ok;
}

bool verifyStencilAttachment (const vector<PixelValue>&		reference,
							  const ConstPixelBufferAccess&	result,
							  const PixelBufferAccess&		errorImage)
{
	const Vec4	red		(1.0f, 0.0f, 0.0f, 1.0f);
	const Vec4	green	(0.0f, 1.0f, 0.0f, 1.0f);
	bool		ok		= true;

	DE_ASSERT(result.getWidth() * result.getHeight() == (int)reference.size());
	DE_ASSERT(result.getWidth() == errorImage.getWidth());
	DE_ASSERT(result.getHeight() == errorImage.getHeight());

	for (int y = 0; y < result.getHeight(); y++)
	for (int x = 0; x < result.getWidth(); x++)
	{
		bool pixelOk = true;

		const deUint32		resultStencil	= result.getPixStencil(x, y);
		const PixelValue&	referenceValue	= reference[x + y * result.getWidth()];
		const Maybe<bool>	maybeValue		= referenceValue.getValue(1);

		if (maybeValue)
		{
			const bool value = *maybeValue;

			if ((value && (resultStencil != 0xFFu))
				|| (!value && resultStencil != 0x0u))
				pixelOk = false;
		}

		if (!pixelOk)
		{
			errorImage.setPixel(red, x, y);
			ok = false;
		}
		else
			errorImage.setPixel(green, x, y);
	}

	return ok;
}

bool logAndVerifyImages (TestLog&											log,
						 const DeviceInterface&								vk,
						 VkDevice											device,
						 const vector<de::SharedPtr<AttachmentResources> >&	attachmentResources,
						 const vector<bool>&								attachmentIsLazy,
						 const RenderPass&									renderPassInfo,
						 const vector<Maybe<VkClearValue> >&				renderPassClearValues,
						 const vector<Maybe<VkClearValue> >&				imageClearValues,
						 const vector<SubpassRenderInfo>&					subpassRenderInfo,
						 const UVec2&										targetSize,
						 const TestConfig&									config)
{
	vector<vector<PixelValue> >	referenceValues;
	vector<tcu::TextureLevel>	referenceAttachments;
	bool						isOk					= true;

	log << TestLog::Message << "Reference images fill undefined pixels with 3x3 grid pattern." << TestLog::EndMessage;

	renderReferenceValues(referenceValues, renderPassInfo, targetSize, imageClearValues, renderPassClearValues, subpassRenderInfo, config.renderPos, config.renderSize, config.drawStartNdx, config.depthValues);
	renderReferenceImagesFromValues(referenceAttachments, referenceValues, targetSize, renderPassInfo, config.depthValues);

	for (size_t attachmentNdx = 0; attachmentNdx < renderPassInfo.getAttachments().size(); attachmentNdx++)
	{
		if (!attachmentIsLazy[attachmentNdx])
		{
			bool						attachmentOK	= true;
			const Attachment			attachment		= renderPassInfo.getAttachments()[attachmentNdx];
			const tcu::TextureFormat	format			= mapVkFormat(attachment.getFormat());

			if (tcu::hasDepthComponent(format.order) && tcu::hasStencilComponent(format.order))
			{
				const tcu::TextureFormat	depthFormat			= getDepthCopyFormat(attachment.getFormat());
				void* const					depthPtr			= attachmentResources[attachmentNdx]->getResultMemory().getHostPtr();

				const tcu::TextureFormat	stencilFormat		= getStencilCopyFormat(attachment.getFormat());
				void* const					stencilPtr			= attachmentResources[attachmentNdx]->getSecondaryResultMemory().getHostPtr();

				invalidateAlloc(vk, device, attachmentResources[attachmentNdx]->getResultMemory());
				invalidateAlloc(vk, device, attachmentResources[attachmentNdx]->getSecondaryResultMemory());

				{
					bool							depthOK				= true;
					bool							stencilOK			= true;
					const ConstPixelBufferAccess	depthAccess			(depthFormat, targetSize.x(), targetSize.y(), 1, depthPtr);
					const ConstPixelBufferAccess	stencilAccess		(stencilFormat, targetSize.x(), targetSize.y(), 1, stencilPtr);
					tcu::TextureLevel				depthErrorImage		(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8), targetSize.x(), targetSize.y());
					tcu::TextureLevel				stencilErrorImage	(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8), targetSize.x(), targetSize.y());

					if (renderPassInfo.getAttachments()[attachmentNdx].getStoreOp() == VK_ATTACHMENT_STORE_OP_STORE
						&& !verifyDepthAttachment(referenceValues[attachmentNdx], depthAccess, depthErrorImage.getAccess(), config.depthValues, requiredDepthEpsilon(attachment.getFormat())))
					{
						depthOK = false;
					}

					if (renderPassInfo.getAttachments()[attachmentNdx].getStencilStoreOp() == VK_ATTACHMENT_STORE_OP_STORE
						&& !verifyStencilAttachment(referenceValues[attachmentNdx], stencilAccess, stencilErrorImage.getAccess()))
					{
						stencilOK = false;
					}

					if (!depthOK || !stencilOK)
					{
						const auto attachmentNdxStr = de::toString(attachmentNdx);

						// Output images.
						log << TestLog::ImageSet("OutputAttachments" + attachmentNdxStr, "Output depth and stencil attachments " + attachmentNdxStr);
						log << TestLog::Image("Attachment" + attachmentNdxStr + "Depth", "Attachment " + attachmentNdxStr + " Depth", depthAccess);
						log << TestLog::Image("Attachment" + attachmentNdxStr + "Stencil", "Attachment " + attachmentNdxStr + " Stencil", stencilAccess);
						log << TestLog::EndImageSet;

						// Reference images. These will be logged as image sets due to having depth and stencil aspects.
						log << TestLog::Image("AttachmentReferences" + attachmentNdxStr, "Reference images " + attachmentNdxStr, referenceAttachments[attachmentNdx].getAccess());

						// Error masks.
						log << TestLog::ImageSet("ErrorMasks" + attachmentNdxStr, "Error masks " + attachmentNdxStr);
						if (!depthOK)
							log << TestLog::Image("DepthAttachmentError" + attachmentNdxStr, "Depth Attachment Error " + attachmentNdxStr, depthErrorImage.getAccess());
						if (!stencilOK)
							log << TestLog::Image("StencilAttachmentError" + attachmentNdxStr, "Stencil Attachment Error " + attachmentNdxStr, stencilErrorImage.getAccess());
						log << TestLog::EndImageSet;

						attachmentOK = false;
					}
				}
			}
			else
			{
				void* const	ptr	= attachmentResources[attachmentNdx]->getResultMemory().getHostPtr();

				invalidateAlloc(vk, device, attachmentResources[attachmentNdx]->getResultMemory());

				bool							depthOK		= true;
				bool							stencilOK	= true;
				bool							colorOK		= true;
				const ConstPixelBufferAccess	access		(format, targetSize.x(), targetSize.y(), 1, ptr);
				tcu::TextureLevel				errorImage	(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8), targetSize.x(), targetSize.y());

				if (tcu::hasDepthComponent(format.order))
				{
					if ((renderPassInfo.getAttachments()[attachmentNdx].getStoreOp() == VK_ATTACHMENT_STORE_OP_STORE || renderPassInfo.getAttachments()[attachmentNdx].getStencilStoreOp() == VK_ATTACHMENT_STORE_OP_STORE)
						&& !verifyDepthAttachment(referenceValues[attachmentNdx], access, errorImage.getAccess(), config.depthValues, requiredDepthEpsilon(attachment.getFormat())))
					{
						depthOK = false;
					}
				}
				else if (tcu::hasStencilComponent(format.order))
				{
					if ((renderPassInfo.getAttachments()[attachmentNdx].getStoreOp() == VK_ATTACHMENT_STORE_OP_STORE || renderPassInfo.getAttachments()[attachmentNdx].getStencilStoreOp() == VK_ATTACHMENT_STORE_OP_STORE)
						&& !verifyStencilAttachment(referenceValues[attachmentNdx], access, errorImage.getAccess()))
					{
						stencilOK = false;
					}
				}
				else
				{
					if ((renderPassInfo.getAttachments()[attachmentNdx].getStoreOp() == VK_ATTACHMENT_STORE_OP_STORE || renderPassInfo.getAttachments()[attachmentNdx].getStencilStoreOp() == VK_ATTACHMENT_STORE_OP_STORE)
						&& !verifyColorAttachment(referenceValues[attachmentNdx], access, errorImage.getAccess(), config.useFormatCompCount))
					{
						colorOK = false;
					}
				}

				if (!depthOK || !stencilOK || !colorOK)
				{
					log << TestLog::ImageSet("TestImages", "Output attachment, reference image and error mask");
					if (!depthOK || !stencilOK)
					{
						// Log without conversions.
						log << TestLog::Image("Attachment" + de::toString(attachmentNdx), "Attachment " + de::toString(attachmentNdx), access);
						log << TestLog::Image("AttachmentReference" + de::toString(attachmentNdx), "Attachment reference " + de::toString(attachmentNdx), referenceAttachments[attachmentNdx].getAccess());
					}
					else
					{
						// Convert color images to better reflect test status and output in any format.
						const auto numChannels		= tcu::getNumUsedChannels(access.getFormat().order);
						const auto attachmentForLog	= renderColorImageForLog(access, numChannels);
						const auto referenceForLog	= renderColorImageForLog(referenceValues[attachmentNdx], targetSize, numChannels);

						log << TestLog::Message << "Check the attachment formats and test data to verify which components affect the test result." << TestLog::EndMessage;
						log << TestLog::Message << "In the reference image, unset pixel components are marked with a 3x3 grid storing values 0.0 and 0.25, pixel components set to false are stored as 0.5 and pixel components set to true are stored as 1.0." << TestLog::EndMessage;
						log << TestLog::Message << "Output attachment pixel components are always set to 0.5 or 1.0 but may not be taken into account if not set in the reference image." << TestLog::EndMessage;

						log << TestLog::Image("Attachment" + de::toString(attachmentNdx), "Attachment " + de::toString(attachmentNdx), attachmentForLog->getAccess());
						log << TestLog::Image("AttachmentReference" + de::toString(attachmentNdx), "Attachment reference " + de::toString(attachmentNdx), referenceForLog->getAccess());
					}
					log << TestLog::Image("AttachmentError" + de::toString(attachmentNdx), "Attachment Error " + de::toString(attachmentNdx), errorImage.getAccess());
					log << TestLog::EndImageSet;

					attachmentOK = false;
				}
			}

			if (!attachmentOK)
				isOk = false;
		}
	}

	return isOk;
}

std::string getInputAttachmentType (VkFormat vkFormat)
{
	const tcu::TextureFormat		format			= mapVkFormat(vkFormat);
	const tcu::TextureChannelClass	channelClass	= tcu::getTextureChannelClass(format.type);

	switch (channelClass)
	{
		case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
			return "isubpassInput";

		case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
			return "usubpassInput";

		case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
		case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
		case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
			return "subpassInput";

		default:
			DE_FATAL("Unknown channel class");
			return "";
	}
}

std::string getAttachmentType (VkFormat vkFormat, deBool useFormatCompCount)
{
	const tcu::TextureFormat		format			= mapVkFormat(vkFormat);
	const tcu::TextureChannelClass	channelClass	= tcu::getTextureChannelClass(format.type);
	const size_t					componentCount	= (size_t)tcu::getNumUsedChannels(format.order);

	switch (channelClass)
	{
		case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
			if (useFormatCompCount)
				return (componentCount == 1 ? "int" : "ivec" + de::toString(componentCount));
			else
				return "ivec4";

		case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
			if (useFormatCompCount)
				return (componentCount == 1 ? "uint" : "uvec" + de::toString(componentCount));
			else
				return "uvec4";

		case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
		case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
		case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
			if (useFormatCompCount)
				return (componentCount == 1 ? "float" : "vec" + de::toString(componentCount));
			else
				return "vec4";

		default:
			DE_FATAL("Unknown channel class");
			return "";
	}
}

void createTestShaders (SourceCollections& dst, TestConfig config)
{
	if (config.renderTypes & TestConfig::RENDERTYPES_DRAW)
	{
		const vector<Subpass>&	subpasses	= config.renderPass.getSubpasses();

		for (size_t subpassNdx = 0; subpassNdx < subpasses.size(); subpassNdx++)
		{
			const Subpass&		subpass					= subpasses[subpassNdx];
			deUint32			inputAttachmentBinding	= 0;
			std::ostringstream	vertexShader;
			std::ostringstream	fragmentShader;

			vertexShader << "#version 310 es\n"
						 << "layout(location = 0) in highp vec2 a_position;\n"
						 << "void main (void) {\n"
						 << "\tgl_Position = vec4(a_position, 1.0, 1.0);\n"
						 << "}\n";

			fragmentShader << "#version 310 es\n"
						   << "precision highp float;\n";

			bool hasAnyDepthFormats = false;

			for (size_t attachmentNdx = config.drawStartNdx; attachmentNdx < subpass.getInputAttachments().size(); attachmentNdx++)
			{
				const deUint32				attachmentIndex	= subpass.getInputAttachments()[attachmentNdx].getAttachment();
				const VkImageLayout			layout			= subpass.getInputAttachments()[attachmentNdx].getImageLayout();
				const Attachment			attachment		= config.renderPass.getAttachments()[attachmentIndex];
				const tcu::TextureFormat	format			= mapVkFormat(attachment.getFormat());
				const bool					isDepthFormat	= tcu::hasDepthComponent(format.order);
				const bool					isStencilFormat	= tcu::hasStencilComponent(format.order);

				if (isDepthFormat || isStencilFormat)
				{
					if (isDepthFormat && layout != VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL)
					{
						hasAnyDepthFormats = true;
						fragmentShader << "layout(input_attachment_index = " << attachmentNdx << ", set=0, binding=" << inputAttachmentBinding << ") uniform highp subpassInput i_depth" << attachmentNdx << ";\n";
						inputAttachmentBinding++;
					}

					if (isStencilFormat && layout != VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL)
					{
						fragmentShader << "layout(input_attachment_index = " << attachmentNdx << ", set=0, binding=" << inputAttachmentBinding << ") uniform highp usubpassInput i_stencil" << attachmentNdx << ";\n";
						inputAttachmentBinding++;
					}
				}
				else
				{
					const std::string attachmentType = getInputAttachmentType(attachment.getFormat());

					fragmentShader << "layout(input_attachment_index = " << attachmentNdx << ", set=0, binding=" << inputAttachmentBinding << ") uniform highp " << attachmentType << " i_color" << attachmentNdx << ";\n";
					inputAttachmentBinding++;
				}
			}

			for (size_t attachmentNdx = config.drawStartNdx; attachmentNdx < subpass.getColorAttachments().size(); attachmentNdx++)
			{
				const std::string attachmentType = getAttachmentType(config.renderPass.getAttachments()[getAttachmentNdx(subpass.getColorAttachments(), attachmentNdx)].getFormat(), config.useFormatCompCount);
				fragmentShader << "layout(location = " << attachmentNdx << ") out highp " << attachmentType << " o_color" << attachmentNdx << ";\n";
			}

			if (hasAnyDepthFormats)
				fragmentShader << "\nbool depthsEqual(float a, float b, float epsilon) {\n"
								<< "\treturn abs(a - b) <= epsilon;\n}\n\n";

			fragmentShader << "void main (void) {\n";

			if (subpass.getInputAttachments().empty())
			{
				for (size_t attachmentNdx = config.drawStartNdx; attachmentNdx < subpass.getColorAttachments().size(); attachmentNdx++)
				{
					const deUint32				attachmentIndex	= subpass.getColorAttachments()[attachmentNdx].getAttachment();

					if (attachmentIndex == VK_ATTACHMENT_UNUSED)
						continue;

					const Attachment			attachment		= config.renderPass.getAttachments()[attachmentIndex];
					const tcu::TextureFormat	format			= mapVkFormat(attachment.getFormat());
					const size_t				componentCount	= config.useFormatCompCount ? (size_t)tcu::getNumUsedChannels(format.order) : 4;
					const std::string			attachmentType	= getAttachmentType(attachment.getFormat(), config.useFormatCompCount);

					fragmentShader << "\to_color" << attachmentNdx << " = " << attachmentType << "(" << attachmentType + "(";

					for (size_t compNdx = 0; compNdx < componentCount; compNdx++)
					{
						const size_t	index	= subpassNdx + attachmentIndex + compNdx;
						const BoolOp	op		= boolOpFromIndex(index);

						if (compNdx > 0)
							fragmentShader << ",\n\t\t";

						fragmentShader	<< "((int(gl_FragCoord.x) % 2 == " << (index % 2)
										<< ") " << boolOpToString(op) << " ("
										<< "int(gl_FragCoord.y) % 2 == " << ((index / 2) % 2)
										<< ") ? 1.0 : 0.0)";
					}

					fragmentShader << "));\n";
				}

				if (subpass.getDepthStencilAttachment().getAttachment() != VK_ATTACHMENT_UNUSED
					&& subpass.getDepthStencilAttachment().getImageLayout() != VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
					&& subpass.getDepthStencilAttachment().getImageLayout() != VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL)
				{
					const size_t	index	= subpassNdx + 1;
					const BoolOp	op		= boolOpFromIndex(index);

					fragmentShader	<< "\tgl_FragDepth = ((int(gl_FragCoord.x) % 2 == " << (index % 2)
									<< ") " << boolOpToString(op) << " ("
									<< "int(gl_FragCoord.y) % 2 == " << ((index / 2) % 2)
									<< ") ? " << deUint32(config.depthValues[1]) << ".0f/255.0f : " << deUint32(config.depthValues[0]) << ".0f/255.0f);\n";
				}
			}
			else
			{
				size_t	inputComponentCount		= 0;
				size_t	outputComponentCount	= 0;

				for (size_t attachmentNdx = config.drawStartNdx; attachmentNdx < subpass.getInputAttachments().size(); attachmentNdx++)
				{
					const deUint32				attachmentIndex	= subpass.getInputAttachments()[attachmentNdx].getAttachment();
					const VkImageLayout			layout			= subpass.getInputAttachments()[attachmentNdx].getImageLayout();
					const Attachment			attachment		= config.renderPass.getAttachments()[attachmentIndex];
					const tcu::TextureFormat	format			= mapVkFormat(attachment.getFormat());
					const size_t				componentCount	= (size_t)tcu::getNumUsedChannels(format.order);

					if (layout == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL)
						inputComponentCount += 1;
					else if (layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL)
						inputComponentCount += 1;
					else
						inputComponentCount += componentCount;
				}

				for (size_t attachmentNdx = config.drawStartNdx; attachmentNdx < subpass.getColorAttachments().size(); attachmentNdx++)
				{
					const deUint32				attachmentIndex	= subpass.getColorAttachments()[attachmentNdx].getAttachment();
					const Attachment			attachment		= config.renderPass.getAttachments()[attachmentIndex];
					const tcu::TextureFormat	format			= mapVkFormat(attachment.getFormat());
					const size_t				componentCount	= (size_t)tcu::getNumUsedChannels(format.order);

					outputComponentCount += componentCount;
				}

				if (subpass.getDepthStencilAttachment().getAttachment() != VK_ATTACHMENT_UNUSED
					&& subpass.getDepthStencilAttachment().getImageLayout() != VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
					&& subpass.getDepthStencilAttachment().getImageLayout() != VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL)
				{
					outputComponentCount++;
				}

				if (outputComponentCount > 0)
				{
					const size_t inputsPerOutput = inputComponentCount >= outputComponentCount
													? ((inputComponentCount / outputComponentCount)
														+ ((inputComponentCount % outputComponentCount) != 0 ? 1 : 0))
													: 1;

					fragmentShader << "\tbool inputs[" << inputComponentCount << "];\n";

					if (outputComponentCount > 0)
						fragmentShader << "\tbool outputs[" << outputComponentCount << "];\n";

					size_t inputValueNdx = 0;

					for (size_t attachmentNdx = config.drawStartNdx; attachmentNdx < subpass.getInputAttachments().size(); attachmentNdx++)
					{
						const char* const	components[]	=
						{
							"x", "y", "z", "w"
						};
						const deUint32				attachmentIndex	= subpass.getInputAttachments()[attachmentNdx].getAttachment();
						const VkImageLayout			layout			= subpass.getInputAttachments()[attachmentNdx].getImageLayout();
						const Attachment			attachment		= config.renderPass.getAttachments()[attachmentIndex];
						const tcu::TextureFormat	format			= mapVkFormat(attachment.getFormat());
						const size_t				componentCount	= (size_t)tcu::getNumUsedChannels(format.order);
						const bool					isDepthFormat	= tcu::hasDepthComponent(format.order);
						const bool					isStencilFormat	= tcu::hasStencilComponent(format.order);

						if (isDepthFormat || isStencilFormat)
						{
							if (isDepthFormat && layout != VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL)
							{
								fragmentShader << "\tinputs[" << inputValueNdx << "] = depthsEqual(" << deUint32(config.depthValues[1]) <<
									".0f/255.0f, float(subpassLoad(i_depth" << attachmentNdx << ").x), " <<
									std::fixed << std::setprecision(12) << requiredDepthEpsilon(attachment.getFormat()) << ");\n";
								inputValueNdx++;
							}

							if (isStencilFormat && layout != VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL)
							{
								fragmentShader << "\tinputs[" << inputValueNdx << "] = 255u == subpassLoad(i_stencil" << attachmentNdx << ").x;\n";
								inputValueNdx++;
							}
						}
						else
						{
							for (size_t compNdx = 0; compNdx < componentCount; compNdx++)
							{
								fragmentShader << "\tinputs[" << inputValueNdx << "] = 1.0 == float(subpassLoad(i_color" << attachmentNdx << ")." << components[compNdx] << ");\n";
								inputValueNdx++;
							}
						}
					}

					size_t outputValueNdx = 0;

					for (size_t attachmentNdx = config.drawStartNdx; attachmentNdx < subpass.getColorAttachments().size(); attachmentNdx++)
					{
						const deUint32				attachmentIndex	= subpass.getColorAttachments()[attachmentNdx].getAttachment();
						const Attachment			attachment		= config.renderPass.getAttachments()[attachmentIndex];
						const std::string			attachmentType	= getAttachmentType(config.renderPass.getAttachments()[attachmentIndex].getFormat(), config.useFormatCompCount);
						const tcu::TextureFormat	format			= mapVkFormat(attachment.getFormat());
						const size_t				componentCount	= (size_t)tcu::getNumUsedChannels(format.order);

						for (size_t compNdx = 0; compNdx < componentCount; compNdx++)
						{
							const size_t	index	= subpassNdx + attachmentIndex + outputValueNdx;
							const BoolOp	op		= boolOpFromIndex(index);

							fragmentShader << "\toutputs[" << outputValueNdx + compNdx << "] = "
											<< "(int(gl_FragCoord.x) % 2 == " << (index % 2)
											<< ") " << boolOpToString(op) << " ("
											<< "int(gl_FragCoord.y) % 2 == " << ((index / 2) % 2)
											<< ");\n";

							for (size_t i = 0; i < inputsPerOutput; i++)
								fragmentShader << "\toutputs[" << outputValueNdx + compNdx << "] = outputs[" << outputValueNdx + compNdx << "] == inputs[" <<  ((outputValueNdx + compNdx) * inputsPerOutput + i) %  inputComponentCount << "];\n";
						}

						fragmentShader << "\to_color" << attachmentNdx << " = " << attachmentType << "(";

						for (size_t compNdx = 0; compNdx < (config.useFormatCompCount ? componentCount : 4); compNdx++)
						{
							if (compNdx > 0)
								fragmentShader << ", ";

							if (compNdx < componentCount)
								fragmentShader << "outputs[" << outputValueNdx + compNdx << "]";
							else
								fragmentShader << "0";
						}

						outputValueNdx += componentCount;

						fragmentShader << ");\n";
					}

					if (subpass.getDepthStencilAttachment().getAttachment() != VK_ATTACHMENT_UNUSED
						&& subpass.getDepthStencilAttachment().getImageLayout() != VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
						&& subpass.getDepthStencilAttachment().getImageLayout() != VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL)
					{
						const deUint32	attachmentIndex	= subpass.getDepthStencilAttachment().getAttachment();
						const size_t	index			= subpassNdx + attachmentIndex;
						const BoolOp	op				= boolOpFromIndex(index);

						fragmentShader << "\toutputs[" << outputValueNdx << "] = "
										<< "(int(gl_FragCoord.x) % 2 == " << (index % 2)
										<< ") " << boolOpToString(op) << " ("
										<< "int(gl_FragCoord.y) % 2 == " << ((index / 2) % 2)
										<< ");\n";

						for (size_t i = 0; i < inputsPerOutput; i++)
							fragmentShader << "\toutputs[" << outputValueNdx << "] = outputs[" << outputValueNdx << "] == inputs[" <<  (outputValueNdx * inputsPerOutput + i) %  inputComponentCount << "];\n";

						fragmentShader << "\tgl_FragDepth = outputs[" << outputValueNdx << "] ? " << deUint32(config.depthValues[1]) << ".0f/255.0f : " << deUint32(config.depthValues[0]) << ".0f/255.0f;\n";
					}
				}
			}

			fragmentShader << "}\n";

			dst.glslSources.add(de::toString(subpassNdx) + "-vert") << glu::VertexSource(vertexShader.str());
			dst.glslSources.add(de::toString(subpassNdx) + "-frag") << glu::FragmentSource(fragmentShader.str());
		}
	}
}

void initializeAttachmentIsLazy (vector<bool>& attachmentIsLazy, const vector<Attachment>& attachments, TestConfig::ImageMemory imageMemory)
{
	bool lastAttachmentWasLazy	= false;

	for (size_t attachmentNdx = 0; attachmentNdx < attachments.size(); attachmentNdx++)
	{
		if (attachments[attachmentNdx].getLoadOp() != VK_ATTACHMENT_LOAD_OP_LOAD
			&& attachments[attachmentNdx].getStoreOp() != VK_ATTACHMENT_STORE_OP_STORE
			&& attachments[attachmentNdx].getStencilLoadOp() != VK_ATTACHMENT_LOAD_OP_LOAD
			&& attachments[attachmentNdx].getStencilStoreOp() != VK_ATTACHMENT_STORE_OP_STORE)
		{
			if (imageMemory == TestConfig::IMAGEMEMORY_LAZY || (imageMemory & TestConfig::IMAGEMEMORY_LAZY && !lastAttachmentWasLazy))
			{
				attachmentIsLazy.push_back(true);

				lastAttachmentWasLazy	= true;
			}
			else if (imageMemory & TestConfig::IMAGEMEMORY_STRICT)
			{
				attachmentIsLazy.push_back(false);
				lastAttachmentWasLazy = false;
			}
			else
				DE_FATAL("Unknown imageMemory");
		}
		else
			attachmentIsLazy.push_back(false);
	}
}

enum AttachmentRefType
{
	ATTACHMENTREFTYPE_COLOR,
	ATTACHMENTREFTYPE_DEPTH_STENCIL,
	ATTACHMENTREFTYPE_INPUT,
	ATTACHMENTREFTYPE_RESOLVE,
};

VkImageUsageFlags getImageUsageFromLayout (VkImageLayout layout)
{
	switch (layout)
	{
		case VK_IMAGE_LAYOUT_GENERAL:
		case VK_IMAGE_LAYOUT_PREINITIALIZED:
			return 0;

		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
			return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			return VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			return VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			return VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		default:
			DE_FATAL("Unexpected image layout");
			return 0;
	}
}

void getImageUsageFromAttachmentReferences(vector<VkImageUsageFlags>& attachmentImageUsage, AttachmentRefType refType, size_t count, const AttachmentReference* references)
{
	for (size_t referenceNdx = 0; referenceNdx < count; ++referenceNdx)
	{
		const deUint32 attachment = references[referenceNdx].getAttachment();

		if (attachment != VK_ATTACHMENT_UNUSED)
		{
			VkImageUsageFlags usage;

			switch (refType)
			{
				case ATTACHMENTREFTYPE_COLOR:
				case ATTACHMENTREFTYPE_RESOLVE:
					usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
					break;

				case ATTACHMENTREFTYPE_DEPTH_STENCIL:
					usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
					break;

				case ATTACHMENTREFTYPE_INPUT:
					usage = VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
					break;

				default:
					DE_FATAL("Unexpected attachment reference type");
					usage = 0;
					break;
			}

			attachmentImageUsage[attachment] |= usage;
		}
	}
}

void getImageUsageFromAttachmentReferences(vector<VkImageUsageFlags>& attachmentImageUsage, AttachmentRefType refType, const vector<AttachmentReference>& references)
{
	if (!references.empty())
	{
		getImageUsageFromAttachmentReferences(attachmentImageUsage, refType, references.size(), &references[0]);
	}
}

void initializeAttachmentImageUsage (Context &context, vector<VkImageUsageFlags>& attachmentImageUsage, const RenderPass& renderPassInfo, const vector<bool>& attachmentIsLazy, const vector<Maybe<VkClearValue> >& clearValues)
{
	attachmentImageUsage.resize(renderPassInfo.getAttachments().size(), VkImageUsageFlags(0));

	for (size_t subpassNdx = 0; subpassNdx < renderPassInfo.getSubpasses().size(); ++subpassNdx)
	{
		const Subpass& subpass = renderPassInfo.getSubpasses()[subpassNdx];

		getImageUsageFromAttachmentReferences(attachmentImageUsage, ATTACHMENTREFTYPE_COLOR, subpass.getColorAttachments());
		getImageUsageFromAttachmentReferences(attachmentImageUsage, ATTACHMENTREFTYPE_DEPTH_STENCIL, 1, &subpass.getDepthStencilAttachment());
		getImageUsageFromAttachmentReferences(attachmentImageUsage, ATTACHMENTREFTYPE_INPUT, subpass.getInputAttachments());
		getImageUsageFromAttachmentReferences(attachmentImageUsage, ATTACHMENTREFTYPE_RESOLVE, subpass.getResolveAttachments());
	}

	for (size_t attachmentNdx = 0; attachmentNdx < renderPassInfo.getAttachments().size(); attachmentNdx++)
	{
		const Attachment& attachment = renderPassInfo.getAttachments()[attachmentNdx];
		const VkFormatProperties	formatProperties	= getPhysicalDeviceFormatProperties(context.getInstanceInterface(), context.getPhysicalDevice(), attachment.getFormat());
		const VkFormatFeatureFlags	supportedFeatures	= formatProperties.optimalTilingFeatures;

		if ((supportedFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0)
			attachmentImageUsage[attachmentNdx] |= VK_IMAGE_USAGE_SAMPLED_BIT;

		if ((supportedFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) != 0)
			attachmentImageUsage[attachmentNdx] |= VK_IMAGE_USAGE_STORAGE_BIT;

		attachmentImageUsage[attachmentNdx] |= getImageUsageFromLayout(attachment.getInitialLayout());
		attachmentImageUsage[attachmentNdx] |= getImageUsageFromLayout(attachment.getFinalLayout());

		if (!attachmentIsLazy[attachmentNdx])
		{
			if (clearValues[attachmentNdx])
				attachmentImageUsage[attachmentNdx] |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

			attachmentImageUsage[attachmentNdx] |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		}
		else
		{
			const VkImageUsageFlags allowedTransientBits = static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);

			attachmentImageUsage[attachmentNdx] &= allowedTransientBits;
			attachmentImageUsage[attachmentNdx] |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
		}
	}
}

void initializeSubpassIsSecondary (vector<bool>& subpassIsSecondary, const vector<Subpass>& subpasses, TestConfig::CommandBufferTypes commandBuffer)
{
	bool lastSubpassWasSecondary = false;

	for (size_t subpassNdx = 0; subpassNdx < subpasses.size(); subpassNdx++)
	{
		if (commandBuffer == TestConfig::COMMANDBUFFERTYPES_SECONDARY || (commandBuffer & TestConfig::COMMANDBUFFERTYPES_SECONDARY && !lastSubpassWasSecondary))
		{
			subpassIsSecondary.push_back(true);
			lastSubpassWasSecondary = true;
		}
		else if (commandBuffer & TestConfig::COMMANDBUFFERTYPES_INLINE)
		{
			subpassIsSecondary.push_back(false);
			lastSubpassWasSecondary = false;
		}
		else
			DE_FATAL("Unknown commandBuffer");
	}
}

void initializeImageClearValues (de::Random& rng, vector<Maybe<VkClearValue> >& clearValues, const vector<Attachment>& attachments, const vector<bool>& isLazy, deBool useFormatCompCount, const DepthValuesArray& depthValues)
{
	for (size_t attachmentNdx = 0; attachmentNdx < attachments.size(); attachmentNdx++)
	{
		if (!isLazy[attachmentNdx])
			clearValues.push_back(just(randomClearValue(attachments[attachmentNdx], rng, useFormatCompCount, depthValues)));
		else
			clearValues.push_back(nothing<VkClearValue>());
	}
}

void initializeRenderPassClearValues (de::Random& rng, vector<Maybe<VkClearValue> >& clearValues, const vector<Attachment>& attachments, deBool useFormatCompCount, const DepthValuesArray& depthValues)
{
	for (size_t attachmentNdx = 0; attachmentNdx < attachments.size(); attachmentNdx++)
	{
		if (attachments[attachmentNdx].getLoadOp() == VK_ATTACHMENT_LOAD_OP_CLEAR
			|| attachments[attachmentNdx].getStencilLoadOp() == VK_ATTACHMENT_LOAD_OP_CLEAR)
		{
			clearValues.push_back(just(randomClearValue(attachments[attachmentNdx], rng, useFormatCompCount, depthValues)));
		}
		else
			clearValues.push_back(nothing<VkClearValue>());
	}
}

void logSubpassRenderInfo (TestLog& log, const SubpassRenderInfo& info, TestConfig config)
{
	log << TestLog::Message << "Viewport, offset: " << info.getViewportOffset() << ", size: " << info.getViewportSize() << TestLog::EndMessage;

	if (info.isSecondary())
		log << TestLog::Message << "Subpass uses secondary command buffers" << TestLog::EndMessage;
	else
		log << TestLog::Message << "Subpass uses inlined commands" << TestLog::EndMessage;

	for (deUint32 attachmentNdx = 0; attachmentNdx < info.getColorClears().size(); attachmentNdx++)
	{
		const ColorClear&	colorClear	= info.getColorClears()[attachmentNdx];

		log << TestLog::Message << "Clearing color attachment " << attachmentNdx
			<< ". Offset: " << colorClear.getOffset()
			<< ", Size: " << colorClear.getSize()
			<< ", Color: " << clearColorToString(info.getColorAttachment(attachmentNdx).getFormat(), colorClear.getColor(), config.useFormatCompCount) << TestLog::EndMessage;
	}

	if (info.getDepthStencilClear())
	{
		const DepthStencilClear&	depthStencilClear	= *info.getDepthStencilClear();

		log << TestLog::Message << "Clearing depth stencil attachment"
			<< ". Offset: " << depthStencilClear.getOffset()
			<< ", Size: " << depthStencilClear.getSize()
			<< ", Depth: " << depthStencilClear.getDepth()
			<< ", Stencil: " << depthStencilClear.getStencil() << TestLog::EndMessage;
	}

	if (info.getRenderQuad())
	{
		const RenderQuad&	renderQuad	= *info.getRenderQuad();

		log << TestLog::Message << "Rendering grid quad to " << renderQuad.getCornerA() << " -> " << renderQuad.getCornerB() << TestLog::EndMessage;
	}
}

void logTestCaseInfo (TestLog&								log,
					  const TestConfig&						config,
					  const vector<bool>&					attachmentIsLazy,
					  const vector<Maybe<VkClearValue> >&	imageClearValues,
					  const vector<Maybe<VkClearValue> >&	renderPassClearValues,
					  const vector<SubpassRenderInfo>&		subpassRenderInfo)
{
	const RenderPass&	renderPass	= config.renderPass;

	logRenderPassInfo(log, renderPass);

	DE_ASSERT(attachmentIsLazy.size() == renderPass.getAttachments().size());
	DE_ASSERT(imageClearValues.size() == renderPass.getAttachments().size());
	DE_ASSERT(renderPassClearValues.size() == renderPass.getAttachments().size());

	log << TestLog::Message << "TargetSize: " << config.targetSize << TestLog::EndMessage;
	log << TestLog::Message << "Render area, Offset: " << config.renderPos << ", Size: " << config.renderSize << TestLog::EndMessage;

	for (size_t attachmentNdx = 0; attachmentNdx < attachmentIsLazy.size(); attachmentNdx++)
	{
		const tcu::ScopedLogSection	section	(log, "Attachment" + de::toString(attachmentNdx), "Attachment " + de::toString(attachmentNdx));

		if (attachmentIsLazy[attachmentNdx])
			log << TestLog::Message << "Is lazy." << TestLog::EndMessage;

		if (imageClearValues[attachmentNdx])
			log << TestLog::Message << "Image is cleared to " << clearValueToString(renderPass.getAttachments()[attachmentNdx].getFormat(),
					*imageClearValues[attachmentNdx], config.useFormatCompCount) << " before rendering." << TestLog::EndMessage;

		if (renderPass.getAttachments()[attachmentNdx].getLoadOp() == VK_ATTACHMENT_LOAD_OP_CLEAR && renderPassClearValues[attachmentNdx])
			log << TestLog::Message << "Attachment is cleared to " << clearValueToString(renderPass.getAttachments()[attachmentNdx].getFormat(),
					*renderPassClearValues[attachmentNdx], config.useFormatCompCount) << " in the beginning of the render pass." << TestLog::EndMessage;
	}

	for (size_t subpassNdx = 0; subpassNdx < renderPass.getSubpasses().size(); subpassNdx++)
	{
		const tcu::ScopedLogSection section (log, "Subpass" + de::toString(subpassNdx), "Subpass " + de::toString(subpassNdx));

		logSubpassRenderInfo(log, subpassRenderInfo[subpassNdx], config);
	}
}

float roundToViewport (float x, deUint32 offset, deUint32 size)
{
	const float		origin	= (float)(offset) + ((float(size) / 2.0f));
	const float		p		= (float)(size) / 2.0f;
	const deInt32	xi		= deRoundFloatToInt32(origin + (p * x));

	return (((float)xi) - origin) / p;
}

void initializeSubpassRenderInfo (vector<SubpassRenderInfo>& renderInfos, de::Random& rng, const RenderPass& renderPass, const TestConfig& config)
{
	const TestConfig::CommandBufferTypes	commandBuffer			= config.commandBufferTypes;
	const vector<Subpass>&					subpasses				= renderPass.getSubpasses();
	bool									lastSubpassWasSecondary	= false;

	for (deUint32 subpassNdx = 0; subpassNdx < (deUint32)subpasses.size(); subpassNdx++)
	{
		const Subpass&				subpass				= subpasses[subpassNdx];
		const bool					subpassIsSecondary	= commandBuffer == TestConfig::COMMANDBUFFERTYPES_SECONDARY
														|| (commandBuffer & TestConfig::COMMANDBUFFERTYPES_SECONDARY && !lastSubpassWasSecondary) ? true : false;
		const bool					omitBlendState		= subpass.getOmitBlendState();
		const UVec2					viewportSize		((config.renderSize * UVec2(2)) / UVec2(3));
		const UVec2					viewportOffset		(config.renderPos.x() + (subpassNdx % 2) * (config.renderSize.x() / 3),
														 config.renderPos.y() + ((subpassNdx / 2) % 2) * (config.renderSize.y() / 3));

		vector<ColorClear>			colorClears;
		Maybe<DepthStencilClear>	depthStencilClear;
		Maybe<RenderQuad>			renderQuad;

		lastSubpassWasSecondary		= subpassIsSecondary;

		if (config.renderTypes & TestConfig::RENDERTYPES_CLEAR)
		{
			const vector<AttachmentReference>&	colorAttachments	= subpass.getColorAttachments();

			for (size_t attachmentRefNdx = 0; attachmentRefNdx < colorAttachments.size(); attachmentRefNdx++)
			{
				const AttachmentReference&	attachmentRef	= colorAttachments[attachmentRefNdx];
				const Attachment&			attachment		= renderPass.getAttachments()[attachmentRef.getAttachment()];
				const UVec2					size			((viewportSize * UVec2(2)) / UVec2(3));
				const UVec2					offset			(viewportOffset.x() + ((deUint32)attachmentRefNdx % 2u) * (viewportSize.x() / 3u),
															 viewportOffset.y() + (((deUint32)attachmentRefNdx / 2u) % 2u) * (viewportSize.y() / 3u));
				const VkClearColorValue		color			= randomColorClearValue(attachment, rng, config.useFormatCompCount);

				colorClears.push_back(ColorClear(offset, size, color));
			}

			if (subpass.getDepthStencilAttachment().getAttachment() != VK_ATTACHMENT_UNUSED)
			{
				const Attachment&	attachment	= renderPass.getAttachments()[subpass.getDepthStencilAttachment().getAttachment()];
				const UVec2			size		((viewportSize * UVec2(2)) / UVec2(3));
				const UVec2			offset		(viewportOffset.x() + ((deUint32)colorAttachments.size() % 2u) * (viewportSize.x() / 3u),
												 viewportOffset.y() + (((deUint32)colorAttachments.size() / 2u) % 2u) * (viewportSize.y() / 3u));
				const VkClearValue	value		= randomClearValue(attachment, rng, config.useFormatCompCount, config.depthValues);

				depthStencilClear = tcu::just(DepthStencilClear(offset, size, value.depthStencil.depth, value.depthStencil.stencil));
			}
		}

		if (config.renderTypes & TestConfig::RENDERTYPES_DRAW)
		{
			const float	w	= (subpassNdx % 2) == 0 ? 1.0f : 1.25f;
			const float	h	= (subpassNdx % 2) == 0 ? 1.25f : 1.0f;

			const float	x0	= roundToViewport((subpassNdx % 2) == 0 ? 1.0f - w : -1.0f, viewportOffset.x(), viewportSize.x());
			const float	x1	= roundToViewport((subpassNdx % 2) == 0 ? 1.0f : -1.0f + w, viewportOffset.x(), viewportSize.x());

			const float	y0	= roundToViewport(((subpassNdx / 2) % 2) == 0 ? 1.0f - h : -1.0f, viewportOffset.y(), viewportSize.y());
			const float	y1	= roundToViewport(((subpassNdx / 2) % 2) == 0 ? 1.0f : -1.0f + h, viewportOffset.y(), viewportSize.y());

			renderQuad = tcu::just(RenderQuad(tcu::Vec2(x0, y0), tcu::Vec2(x1, y1)));
		}

		renderInfos.push_back(SubpassRenderInfo(renderPass, subpassNdx, config.drawStartNdx, subpassIsSecondary, omitBlendState, viewportOffset, viewportSize, renderQuad, colorClears, depthStencilClear));
	}
}

void checkTextureFormatSupport (TestLog&					log,
								const InstanceInterface&	vk,
								VkPhysicalDevice			device,
								const vector<Attachment>&	attachments)
{
	bool supported = true;

	for (size_t attachmentNdx = 0; attachmentNdx < attachments.size(); attachmentNdx++)
	{
		const Attachment&			attachment					= attachments[attachmentNdx];
		const tcu::TextureFormat	format						= mapVkFormat(attachment.getFormat());
		const bool					isDepthOrStencilAttachment	= hasDepthComponent(format.order) || hasStencilComponent(format.order);
		const VkFormatFeatureFlags	flags						= isDepthOrStencilAttachment? VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
		VkFormatProperties			properties;

		vk.getPhysicalDeviceFormatProperties(device, attachment.getFormat(), &properties);

		if ((properties.optimalTilingFeatures & flags) != flags)
		{
			supported = false;
			log << TestLog::Message << "Format: " << attachment.getFormat() << " not supported as " << (isDepthOrStencilAttachment ? "depth stencil attachment" : "color attachment") << TestLog::EndMessage;
		}
	}

	if (!supported)
		TCU_THROW(NotSupportedError, "Format not supported");
}

tcu::TestStatus renderPassTest (Context& context, TestConfig config)
{
	const UVec2							targetSize			= config.targetSize;
	const UVec2							renderPos			= config.renderPos;
	const UVec2							renderSize			= config.renderSize;
	const RenderPass&					renderPassInfo		= config.renderPass;

	TestLog&							log					= context.getTestContext().getLog();
	de::Random							rng					(config.seed);

	vector<bool>						attachmentIsLazy;
	vector<VkImageUsageFlags>			attachmentImageUsage;
	vector<Maybe<VkClearValue> >		imageClearValues;
	vector<Maybe<VkClearValue> >		renderPassClearValues;

	vector<bool>						subpassIsSecondary;
	vector<SubpassRenderInfo>			subpassRenderInfo;

	if (config.renderPassType == RENDERPASS_TYPE_RENDERPASS2)
		context.requireDeviceFunctionality("VK_KHR_create_renderpass2");

	if (config.allocationKind == ALLOCATION_KIND_DEDICATED)
	{
		if (!context.isDeviceFunctionalitySupported("VK_KHR_dedicated_allocation"))
			TCU_THROW(NotSupportedError, "VK_KHR_dedicated_allocation is not supported");
	}

	if (!renderPassInfo.getInputAspects().empty())
	{
		if (!context.isDeviceFunctionalitySupported("VK_KHR_maintenance2"))
			TCU_THROW(NotSupportedError, "Extension VK_KHR_maintenance2 not supported.");
	}

	{
		bool requireDepthStencilLayout = false;

		for (size_t attachmentNdx = 0; attachmentNdx < renderPassInfo.getAttachments().size(); attachmentNdx++)
		{
			if (renderPassInfo.getAttachments()[attachmentNdx].getInitialLayout() == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL
				|| renderPassInfo.getAttachments()[attachmentNdx].getInitialLayout() == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL
				|| renderPassInfo.getAttachments()[attachmentNdx].getFinalLayout() == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL
				|| renderPassInfo.getAttachments()[attachmentNdx].getFinalLayout() == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL)
			{
				requireDepthStencilLayout = true;
				break;
			}
		}

		for (size_t subpassNdx = 0; subpassNdx < renderPassInfo.getSubpasses().size() && !requireDepthStencilLayout; subpassNdx++)
		{
			const Subpass& subpass (renderPassInfo.getSubpasses()[subpassNdx]);

			for (size_t attachmentNdx = 0; attachmentNdx < subpass.getColorAttachments().size(); attachmentNdx++)
			{
				if (subpass.getColorAttachments()[attachmentNdx].getImageLayout() == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL
					|| subpass.getColorAttachments()[attachmentNdx].getImageLayout() == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL)
				{
					requireDepthStencilLayout = true;
					break;
				}
			}

			for (size_t attachmentNdx = 0; !requireDepthStencilLayout && attachmentNdx < subpass.getInputAttachments().size(); attachmentNdx++)
			{
				if (subpass.getInputAttachments()[attachmentNdx].getImageLayout() == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL
					|| subpass.getInputAttachments()[attachmentNdx].getImageLayout() == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL)
				{
					requireDepthStencilLayout = true;
					break;
				}
			}

			for (size_t attachmentNdx = 0; !requireDepthStencilLayout && attachmentNdx < subpass.getResolveAttachments().size(); attachmentNdx++)
			{
				if (subpass.getResolveAttachments()[attachmentNdx].getImageLayout() == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL
					|| subpass.getResolveAttachments()[attachmentNdx].getImageLayout() == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL)
				{
					requireDepthStencilLayout = true;
					break;
				}
			}

			if (subpass.getDepthStencilAttachment().getImageLayout() == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL
				|| subpass.getDepthStencilAttachment().getImageLayout() == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL)
			{
				requireDepthStencilLayout = true;
				break;
			}
		}

		if (requireDepthStencilLayout && !context.isDeviceFunctionalitySupported("VK_KHR_maintenance2"))
			TCU_THROW(NotSupportedError, "VK_KHR_maintenance2 is not supported");
	}

	initializeAttachmentIsLazy(attachmentIsLazy, renderPassInfo.getAttachments(), config.imageMemory);
	initializeImageClearValues(rng, imageClearValues, renderPassInfo.getAttachments(), attachmentIsLazy, config.useFormatCompCount, config.depthValues);
	initializeAttachmentImageUsage(context, attachmentImageUsage, renderPassInfo, attachmentIsLazy, imageClearValues);
	initializeRenderPassClearValues(rng, renderPassClearValues, renderPassInfo.getAttachments(), config.useFormatCompCount, config.depthValues);

	initializeSubpassIsSecondary(subpassIsSecondary, renderPassInfo.getSubpasses(), config.commandBufferTypes);
	initializeSubpassRenderInfo(subpassRenderInfo, rng, renderPassInfo, config);

	logTestCaseInfo(log, config, attachmentIsLazy, imageClearValues, renderPassClearValues, subpassRenderInfo);

	checkTextureFormatSupport(log, context.getInstanceInterface(), context.getPhysicalDevice(), config.renderPass.getAttachments());

	{
		const vk::VkPhysicalDeviceProperties properties = vk::getPhysicalDeviceProperties(context.getInstanceInterface(), context.getPhysicalDevice());

		log << TestLog::Message << "Max color attachments: " << properties.limits.maxColorAttachments << TestLog::EndMessage;

		for (size_t subpassNdx = 0; subpassNdx < renderPassInfo.getSubpasses().size(); subpassNdx++)
		{
			 if (renderPassInfo.getSubpasses()[subpassNdx].getColorAttachments().size() > (size_t)properties.limits.maxColorAttachments)
				 TCU_THROW(NotSupportedError, "Subpass uses more than maxColorAttachments.");
		}
	}

	{
		const InstanceInterface&					vki									= context.getInstanceInterface();
		const VkPhysicalDevice&						physDevice							= context.getPhysicalDevice();
		const VkDevice								device								= context.getDevice();
		const DeviceInterface&						vk									= context.getDeviceInterface();
		const VkQueue								queue								= context.getUniversalQueue();
		const deUint32								queueIndex							= context.getUniversalQueueFamilyIndex();
		Allocator&									allocator							= context.getDefaultAllocator();

		const Unique<VkRenderPass>					renderPass							(createRenderPass(vk, device, renderPassInfo, config.renderPassType));
		const Unique<VkCommandPool>					commandBufferPool					(createCommandPool(vk, device, 0, queueIndex));
		const Unique<VkCommandBuffer>				initializeImagesCommandBuffer		(allocateCommandBuffer(vk, device, *commandBufferPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
		const Unique<VkCommandBuffer>				renderCommandBuffer					(allocateCommandBuffer(vk, device, *commandBufferPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
		const Unique<VkCommandBuffer>				readImagesToBuffersCommandBuffer	(allocateCommandBuffer(vk, device, *commandBufferPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

		vector<de::SharedPtr<AttachmentResources> >	attachmentResources;
		vector<de::SharedPtr<SubpassRenderer> >		subpassRenderers;
		vector<VkImage>								attachmentImages;
		vector<VkImageView>							attachmentViews;
		vector<pair<VkImageView, VkImageView> >		inputAttachmentViews;

		for (size_t attachmentNdx = 0; attachmentNdx < renderPassInfo.getAttachments().size(); attachmentNdx++)
		{
			const Attachment&	attachmentInfo	= renderPassInfo.getAttachments()[attachmentNdx];

			attachmentResources.push_back(de::SharedPtr<AttachmentResources>(new AttachmentResources(vki, physDevice, vk, device, allocator, queueIndex, targetSize, attachmentInfo, attachmentImageUsage[attachmentNdx], config.allocationKind)));
			attachmentViews.push_back(attachmentResources[attachmentNdx]->getAttachmentView());
			attachmentImages.push_back(attachmentResources[attachmentNdx]->getImage());

			inputAttachmentViews.push_back(attachmentResources[attachmentNdx]->getInputAttachmentViews());
		}

		beginCommandBuffer(vk, *initializeImagesCommandBuffer, (VkCommandBufferUsageFlags)0, DE_NULL, 0, DE_NULL, VK_FALSE, (VkQueryControlFlags)0, (VkQueryPipelineStatisticFlags)0);
		pushImageInitializationCommands(vk, *initializeImagesCommandBuffer, renderPassInfo.getAttachments(), attachmentResources, queueIndex, imageClearValues);
		endCommandBuffer(vk, *initializeImagesCommandBuffer);

		{
			const Unique<VkFramebuffer> framebuffer (createFramebuffer(vk, device, *renderPass, targetSize, attachmentViews));

			for (size_t subpassNdx = 0; subpassNdx < renderPassInfo.getSubpasses().size(); subpassNdx++)
				subpassRenderers.push_back(de::SharedPtr<SubpassRenderer>(new SubpassRenderer(context, vk, device, allocator, *renderPass, *framebuffer, *commandBufferPool, queueIndex, attachmentImages, inputAttachmentViews, subpassRenderInfo[subpassNdx], config.renderPass.getAttachments(), config.allocationKind)));

			beginCommandBuffer(vk, *renderCommandBuffer, (VkCommandBufferUsageFlags)0, DE_NULL, 0, DE_NULL, VK_FALSE, (VkQueryControlFlags)0, (VkQueryPipelineStatisticFlags)0);
			pushRenderPassCommands(vk, *renderCommandBuffer, *renderPass, *framebuffer, subpassRenderers, renderPos, renderSize, renderPassClearValues, config.renderTypes, config.renderPassType);
			endCommandBuffer(vk, *renderCommandBuffer);

			beginCommandBuffer(vk, *readImagesToBuffersCommandBuffer, (VkCommandBufferUsageFlags)0, DE_NULL, 0, DE_NULL, VK_FALSE, (VkQueryControlFlags)0, (VkQueryPipelineStatisticFlags)0);
			pushReadImagesToBuffers(vk, *readImagesToBuffersCommandBuffer, queueIndex, attachmentResources, renderPassInfo.getAttachments(), attachmentIsLazy, targetSize);
			endCommandBuffer(vk, *readImagesToBuffersCommandBuffer);
			{
				const VkCommandBuffer commandBuffers[] =
				{
					*initializeImagesCommandBuffer,
					*renderCommandBuffer,
					*readImagesToBuffersCommandBuffer
				};
				const Unique<VkFence>	fence		(createFence(vk, device, 0u));

				queueSubmit(vk, queue, DE_LENGTH_OF_ARRAY(commandBuffers), commandBuffers, *fence);
				waitForFences(vk, device, 1, &fence.get(), VK_TRUE, ~0ull);
			}
		}

		if (logAndVerifyImages(log, vk, device, attachmentResources, attachmentIsLazy, renderPassInfo, renderPassClearValues, imageClearValues, subpassRenderInfo, targetSize, config))
			return tcu::TestStatus::pass("Pass");
		else
			return tcu::TestStatus::fail("Result verification failed");
	}
}

static const VkFormat s_coreColorFormats[] =
{
	VK_FORMAT_R5G6B5_UNORM_PACK16,
	VK_FORMAT_R8_UNORM,
	VK_FORMAT_R8_SNORM,
	VK_FORMAT_R8_UINT,
	VK_FORMAT_R8_SINT,
	VK_FORMAT_R8G8_UNORM,
	VK_FORMAT_R8G8_SNORM,
	VK_FORMAT_R8G8_UINT,
	VK_FORMAT_R8G8_SINT,
	VK_FORMAT_R8G8B8A8_UNORM,
	VK_FORMAT_R8G8B8A8_SNORM,
	VK_FORMAT_R8G8B8A8_UINT,
	VK_FORMAT_R8G8B8A8_SINT,
	VK_FORMAT_R8G8B8A8_SRGB,
	VK_FORMAT_A8B8G8R8_UNORM_PACK32,
	VK_FORMAT_A8B8G8R8_SNORM_PACK32,
	VK_FORMAT_A8B8G8R8_UINT_PACK32,
	VK_FORMAT_A8B8G8R8_SINT_PACK32,
	VK_FORMAT_A8B8G8R8_SRGB_PACK32,
	VK_FORMAT_B8G8R8A8_UNORM,
	VK_FORMAT_B8G8R8A8_SRGB,
	VK_FORMAT_A2R10G10B10_UNORM_PACK32,
	VK_FORMAT_A2B10G10R10_UNORM_PACK32,
	VK_FORMAT_A2B10G10R10_UINT_PACK32,
	VK_FORMAT_R16_UNORM,
	VK_FORMAT_R16_SNORM,
	VK_FORMAT_R16_UINT,
	VK_FORMAT_R16_SINT,
	VK_FORMAT_R16_SFLOAT,
	VK_FORMAT_R16G16_UNORM,
	VK_FORMAT_R16G16_SNORM,
	VK_FORMAT_R16G16_UINT,
	VK_FORMAT_R16G16_SINT,
	VK_FORMAT_R16G16_SFLOAT,
	VK_FORMAT_R16G16B16A16_UNORM,
	VK_FORMAT_R16G16B16A16_SNORM,
	VK_FORMAT_R16G16B16A16_UINT,
	VK_FORMAT_R16G16B16A16_SINT,
	VK_FORMAT_R16G16B16A16_SFLOAT,
	VK_FORMAT_R32_UINT,
	VK_FORMAT_R32_SINT,
	VK_FORMAT_R32_SFLOAT,
	VK_FORMAT_R32G32_UINT,
	VK_FORMAT_R32G32_SINT,
	VK_FORMAT_R32G32_SFLOAT,
	VK_FORMAT_R32G32B32A32_UINT,
	VK_FORMAT_R32G32B32A32_SINT,
	VK_FORMAT_R32G32B32A32_SFLOAT
};

static const VkFormat s_coreDepthStencilFormats[] =
{
	VK_FORMAT_D16_UNORM,

	VK_FORMAT_X8_D24_UNORM_PACK32,
	VK_FORMAT_D32_SFLOAT,

	VK_FORMAT_D24_UNORM_S8_UINT,
	VK_FORMAT_D32_SFLOAT_S8_UINT
};

void addAttachmentTests (tcu::TestCaseGroup* group, const TestConfigExternal testConfigExternal)
{
	const deUint32 attachmentCounts[] = { 1, 3, 4, 8 };
	const VkAttachmentLoadOp loadOps[] =
	{
		VK_ATTACHMENT_LOAD_OP_LOAD,
		VK_ATTACHMENT_LOAD_OP_CLEAR,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE
	};

	const VkAttachmentStoreOp storeOps[] =
	{
		VK_ATTACHMENT_STORE_OP_STORE,
		VK_ATTACHMENT_STORE_OP_DONT_CARE
	};

	const VkImageLayout initialAndFinalColorLayouts[] =
	{
		VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
	};

	const VkImageLayout initialAndFinalColorLayoutsLazy[] =
	{
		VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	};

	const VkImageLayout initialAndFinalDepthStencilLayouts[] =
	{
		VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
	};

	const VkImageLayout initialAndFinalDepthStencilLayoutsLazy[] =
	{
		VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	};

	const VkImageLayout subpassLayouts[] =
	{
		VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	};

	const VkImageLayout depthStencilLayouts[] =
	{
		VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
	};

	const TestConfig::RenderTypes renderCommands[] =
	{
		TestConfig::RENDERTYPES_NONE,
		TestConfig::RENDERTYPES_CLEAR,
		TestConfig::RENDERTYPES_DRAW,
		TestConfig::RENDERTYPES_CLEAR|TestConfig::RENDERTYPES_DRAW,
	};

	const TestConfig::CommandBufferTypes commandBuffers[] =
	{
		TestConfig::COMMANDBUFFERTYPES_INLINE,
		TestConfig::COMMANDBUFFERTYPES_SECONDARY,
		TestConfig::COMMANDBUFFERTYPES_INLINE|TestConfig::COMMANDBUFFERTYPES_SECONDARY
	};

	const TestConfig::ImageMemory imageMemories[] =
	{
		TestConfig::IMAGEMEMORY_STRICT,
		TestConfig::IMAGEMEMORY_LAZY,
		TestConfig::IMAGEMEMORY_STRICT|TestConfig::IMAGEMEMORY_LAZY
	};

	const UVec2 targetSizes[] =
	{
		UVec2(64, 64),
		UVec2(63, 65)
	};

	const UVec2 renderPositions[] =
	{
		UVec2(0, 0),
		UVec2(3, 17)
	};

	const UVec2 renderSizes[] =
	{
		UVec2(32, 32),
		UVec2(60, 47)
	};

	tcu::TestContext&	testCtx	= group->getTestContext();
	de::Random			rng		(1433774382u);

	for (size_t attachmentCountNdx = 0; attachmentCountNdx < DE_LENGTH_OF_ARRAY(attachmentCounts); attachmentCountNdx++)
	{
		const deUint32					attachmentCount			= attachmentCounts[attachmentCountNdx];
		const deUint32					testCaseCount			= (attachmentCount == 1 ? 100 : 200);
		de::MovePtr<tcu::TestCaseGroup>	attachmentCountGroup	(new tcu::TestCaseGroup(testCtx, de::toString(attachmentCount).c_str(), de::toString(attachmentCount).c_str()));

		for (size_t testCaseNdx = 0; testCaseNdx < testCaseCount; testCaseNdx++)
		{
			const bool						useDepthStencil		= rng.getBool();
			const TestConfig::ImageMemory	imageMemory			= rng.choose<TestConfig::ImageMemory>(DE_ARRAY_BEGIN(imageMemories), DE_ARRAY_END(imageMemories));
			VkImageLayout					depthStencilLayout	= VK_IMAGE_LAYOUT_GENERAL;
			vector<Attachment>				attachments;
			vector<AttachmentReference>		colorAttachmentReferences;

			for (size_t attachmentNdx = 0; attachmentNdx < attachmentCount; attachmentNdx++)
			{
				const VkSampleCountFlagBits	sampleCount		= VK_SAMPLE_COUNT_1_BIT;
				const VkFormat				format			= rng.choose<VkFormat>(DE_ARRAY_BEGIN(s_coreColorFormats), DE_ARRAY_END(s_coreColorFormats));
				const VkAttachmentLoadOp	loadOp			= rng.choose<VkAttachmentLoadOp>(DE_ARRAY_BEGIN(loadOps), DE_ARRAY_END(loadOps));
				const VkAttachmentStoreOp	storeOp			= rng.choose<VkAttachmentStoreOp>(DE_ARRAY_BEGIN(storeOps), DE_ARRAY_END(storeOps));

				const VkImageLayout			initialLayout	= (imageMemory == TestConfig::IMAGEMEMORY_STRICT)
															? rng.choose<VkImageLayout>(DE_ARRAY_BEGIN(initialAndFinalColorLayouts), DE_ARRAY_END(initialAndFinalColorLayouts))
															: rng.choose<VkImageLayout>(DE_ARRAY_BEGIN(initialAndFinalColorLayoutsLazy), DE_ARRAY_END(initialAndFinalColorLayoutsLazy));
				const VkImageLayout			finalizeLayout	= (imageMemory == TestConfig::IMAGEMEMORY_STRICT)
															? rng.choose<VkImageLayout>(DE_ARRAY_BEGIN(initialAndFinalColorLayouts), DE_ARRAY_END(initialAndFinalColorLayouts))
															: rng.choose<VkImageLayout>(DE_ARRAY_BEGIN(initialAndFinalColorLayoutsLazy), DE_ARRAY_END(initialAndFinalColorLayoutsLazy));
				const VkImageLayout			subpassLayout	= rng.choose<VkImageLayout>(DE_ARRAY_BEGIN(subpassLayouts), DE_ARRAY_END(subpassLayouts));

				const VkAttachmentLoadOp	stencilLoadOp	= rng.choose<VkAttachmentLoadOp>(DE_ARRAY_BEGIN(loadOps), DE_ARRAY_END(loadOps));
				const VkAttachmentStoreOp	stencilStoreOp	= rng.choose<VkAttachmentStoreOp>(DE_ARRAY_BEGIN(storeOps), DE_ARRAY_END(storeOps));

				attachments.push_back(Attachment(format, sampleCount, loadOp, storeOp, stencilLoadOp, stencilStoreOp, initialLayout, finalizeLayout));
				colorAttachmentReferences.push_back(AttachmentReference((deUint32)attachmentNdx, subpassLayout));
			}

			if (useDepthStencil)
			{
				const VkSampleCountFlagBits	sampleCount		= VK_SAMPLE_COUNT_1_BIT;
				const VkFormat				format			= rng.choose<VkFormat>(DE_ARRAY_BEGIN(s_coreDepthStencilFormats), DE_ARRAY_END(s_coreDepthStencilFormats));
				const VkAttachmentLoadOp	loadOp			= rng.choose<VkAttachmentLoadOp>(DE_ARRAY_BEGIN(loadOps), DE_ARRAY_END(loadOps));
				const VkAttachmentStoreOp	storeOp			= rng.choose<VkAttachmentStoreOp>(DE_ARRAY_BEGIN(storeOps), DE_ARRAY_END(storeOps));

				const VkImageLayout			initialLayout	= (imageMemory == TestConfig::IMAGEMEMORY_STRICT)
															? rng.choose<VkImageLayout>(DE_ARRAY_BEGIN(initialAndFinalDepthStencilLayouts), DE_ARRAY_END(initialAndFinalDepthStencilLayouts))
															: rng.choose<VkImageLayout>(DE_ARRAY_BEGIN(initialAndFinalDepthStencilLayoutsLazy), DE_ARRAY_END(initialAndFinalDepthStencilLayoutsLazy));
				const VkImageLayout			finalizeLayout	= (imageMemory == TestConfig::IMAGEMEMORY_STRICT)
															? rng.choose<VkImageLayout>(DE_ARRAY_BEGIN(initialAndFinalDepthStencilLayouts), DE_ARRAY_END(initialAndFinalDepthStencilLayouts))
															: rng.choose<VkImageLayout>(DE_ARRAY_BEGIN(initialAndFinalDepthStencilLayoutsLazy), DE_ARRAY_END(initialAndFinalDepthStencilLayoutsLazy));

				const VkAttachmentLoadOp	stencilLoadOp	= rng.choose<VkAttachmentLoadOp>(DE_ARRAY_BEGIN(loadOps), DE_ARRAY_END(loadOps));
				const VkAttachmentStoreOp	stencilStoreOp	= rng.choose<VkAttachmentStoreOp>(DE_ARRAY_BEGIN(storeOps), DE_ARRAY_END(storeOps));

				depthStencilLayout = rng.choose<VkImageLayout>(DE_ARRAY_BEGIN(depthStencilLayouts), DE_ARRAY_END(depthStencilLayouts));
				attachments.push_back(Attachment(format, sampleCount, loadOp, storeOp, stencilLoadOp, stencilStoreOp, initialLayout, finalizeLayout));
			}

			{
				const TestConfig::RenderTypes			render			= rng.choose<TestConfig::RenderTypes>(DE_ARRAY_BEGIN(renderCommands), DE_ARRAY_END(renderCommands));
				const TestConfig::CommandBufferTypes	commandBuffer	= rng.choose<TestConfig::CommandBufferTypes>(DE_ARRAY_BEGIN(commandBuffers), DE_ARRAY_END(commandBuffers));
				const vector<Subpass>					subpasses		(1, Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS, 0u, vector<AttachmentReference>(), colorAttachmentReferences, vector<AttachmentReference>(), AttachmentReference((useDepthStencil ? (deUint32)(attachments.size() - 1) : VK_ATTACHMENT_UNUSED), depthStencilLayout), vector<deUint32>()));
				const vector<SubpassDependency>			deps;

				const string							testCaseName	= de::toString(attachmentCountNdx * testCaseCount + testCaseNdx);
				const RenderPass						renderPass		(attachments, subpasses, deps);
				const UVec2								targetSize		= rng.choose<UVec2>(DE_ARRAY_BEGIN(targetSizes), DE_ARRAY_END(targetSizes));
				const UVec2								renderPos		= rng.choose<UVec2>(DE_ARRAY_BEGIN(renderPositions), DE_ARRAY_END(renderPositions));
				const UVec2								renderSize		= rng.choose<UVec2>(DE_ARRAY_BEGIN(renderSizes), DE_ARRAY_END(renderSizes));
				const TestConfig						testConfig		(renderPass,
																		 render,
																		 commandBuffer,
																		 imageMemory,
																		 targetSize,
																		 renderPos,
																		 renderSize,
																		 DE_FALSE,
																		 1293809,
																		 0,
																		 testConfigExternal.allocationKind,
																		 testConfigExternal.renderPassType);

				addFunctionCaseWithPrograms<TestConfig>(attachmentCountGroup.get(), testCaseName.c_str(), testCaseName.c_str(), createTestShaders, renderPassTest, testConfig);
			}
		}

		group->addChild(attachmentCountGroup.release());
	}
}

void addAttachmentWriteMaskTests (tcu::TestCaseGroup* group, const TestConfigExternal testConfigExternal)
{
	const deUint32 attachmentCounts[]	= { 1, 2, 3, 4, 8 };

	const VkFormat attachmentFormats[]	=
	{
		VK_FORMAT_R8G8B8A8_UINT,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_R5G6B5_UNORM_PACK16,
		VK_FORMAT_R8G8_UNORM
	};

	tcu::TestContext&	testCtx			= group->getTestContext();

	for (deUint32 attachmentCountNdx = 0; attachmentCountNdx < DE_LENGTH_OF_ARRAY(attachmentCounts); attachmentCountNdx++)
	{
		const deUint32	attachmentCount	= attachmentCounts[attachmentCountNdx];
		const string	groupName		= "attachment_count_" + de::toString(attachmentCount);

		de::MovePtr<tcu::TestCaseGroup>	attachmentCountGroup(new tcu::TestCaseGroup(testCtx, groupName.c_str(), de::toString(attachmentCount).c_str()));

		for (deUint32 drawStartNdx = 0; drawStartNdx < (attachmentCount); drawStartNdx++)
		{
			deUint32					formatNdx = 0;
			vector<Attachment>			attachments;
			vector<AttachmentReference>	colorAttachmentReferences;

			for (deUint32 attachmentNdx = 0; attachmentNdx < attachmentCount; attachmentNdx++)
			{
				const VkFormat				format				= attachmentFormats[formatNdx];
				const VkSampleCountFlagBits	sampleCount			= VK_SAMPLE_COUNT_1_BIT;
				const VkAttachmentLoadOp	loadOp				= VK_ATTACHMENT_LOAD_OP_CLEAR;
				const VkAttachmentStoreOp	storeOp				= VK_ATTACHMENT_STORE_OP_STORE;
				const VkAttachmentLoadOp	stencilLoadOp		= VK_ATTACHMENT_LOAD_OP_CLEAR;
				const VkAttachmentStoreOp	stencilStoreOp		= VK_ATTACHMENT_STORE_OP_STORE;
				const VkImageLayout			initialLayout		= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				const VkImageLayout			finalizeLayout		= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				const VkImageLayout			subpassLayout		= VK_IMAGE_LAYOUT_GENERAL;

				attachments.push_back(Attachment(format, sampleCount, loadOp, storeOp, stencilLoadOp, stencilStoreOp, initialLayout, finalizeLayout));
				colorAttachmentReferences.push_back(AttachmentReference((deUint32)attachmentNdx, subpassLayout));

				if (++formatNdx == DE_LENGTH_OF_ARRAY(attachmentFormats))
					formatNdx = 0;
			}

			{
				const VkImageLayout						depthStencilLayout	= VK_IMAGE_LAYOUT_GENERAL;
				const vector<Subpass>					subpass				(1, Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS, 0u, vector<AttachmentReference>(), colorAttachmentReferences, vector<AttachmentReference>(), AttachmentReference(VK_ATTACHMENT_UNUSED, depthStencilLayout), vector<deUint32>()));
				const vector<SubpassDependency>			deps;

				const string							testCaseName		= "start_index_" + de::toString(drawStartNdx);
				const RenderPass						renderPass			(attachments, subpass, deps);

				const TestConfig::RenderTypes			render				= TestConfig::RENDERTYPES_DRAW;
				const TestConfig::CommandBufferTypes	commandBuffer		= TestConfig::COMMANDBUFFERTYPES_INLINE;
				const TestConfig::ImageMemory			imageMemory			= TestConfig::IMAGEMEMORY_LAZY;
				const UVec2								targetSize			= UVec2(64, 64);
				const UVec2								renderPos			= UVec2(0, 0);
				const UVec2								renderSize			= UVec2(64, 64);
				const deBool							useFormatCompCount	= DE_TRUE;
				const vector<DeviceCoreFeature>			requiredFeatures	= {DEVICE_CORE_FEATURE_INDEPENDENT_BLEND};
				const TestConfig						testConfig			(renderPass,
																			 render,
																			 commandBuffer,
																			 imageMemory,
																			 targetSize,
																			 renderPos,
																			 renderSize,
																			 useFormatCompCount,
																			 1293809,
																			 drawStartNdx,
																			 testConfigExternal.allocationKind,
																			 testConfigExternal.renderPassType,
																			 requiredFeatures);

				addFunctionCaseWithPrograms<TestConfig>(attachmentCountGroup.get(), testCaseName.c_str(), testCaseName.c_str(), checkSupport, createTestShaders, renderPassTest, testConfig);
			}
		}

		group->addChild(attachmentCountGroup.release());
	}
}

template<typename T>
T chooseRandom (de::Random& rng, const set<T>& values)
{
	size_t							ndx		= ((size_t)rng.getUint32()) % values.size();
	typename set<T>::const_iterator	iter	= values.begin();

	for (; ndx > 0; ndx--)
		iter++;

	return *iter;
}

void addAttachmentAllocationTests (tcu::TestCaseGroup* group, const TestConfigExternal testConfigExternal)
{
	const deUint32 attachmentCounts[] = { 4, 8 };
	const VkAttachmentLoadOp loadOps[] =
	{
		VK_ATTACHMENT_LOAD_OP_LOAD,
		VK_ATTACHMENT_LOAD_OP_CLEAR,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE
	};

	const VkAttachmentStoreOp storeOps[] =
	{
		VK_ATTACHMENT_STORE_OP_STORE,
		VK_ATTACHMENT_STORE_OP_DONT_CARE
	};

	const VkImageLayout initialAndFinalColorLayouts[] =
	{
		VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
	};

	const VkImageLayout initialAndFinalDepthStencilLayouts[] =
	{
		VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
	};

	const VkImageLayout subpassLayoutsColor[] =
	{
		VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	};

	const VkImageLayout subpassLayoutsDepthStencil[] =
	{
		VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
	};

	const VkImageLayout subpassLayoutsInput[] =
	{
		VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	};

	enum AllocationType
	{
		// Each pass uses one more attachmen than previous one
		ALLOCATIONTYPE_GROW,
		// Each pass uses one less attachment than previous one
		ALLOCATIONTYPE_SHRINK,
		// Each pass drops one attachment and picks up new one
		ALLOCATIONTYPE_ROLL,
		// Start by growing and end by shrinking
		ALLOCATIONTYPE_GROW_SHRINK,
		// Each subpass has single input and single output attachment
		ALLOCATIONTYPE_IO_CHAIN,
		// Each subpass has multiple inputs and multiple outputs attachment
		ALLOCATIONTYPE_IO_GENERIC
	};

	const AllocationType allocationTypes[] =
	{
		ALLOCATIONTYPE_GROW,
		ALLOCATIONTYPE_SHRINK,
		ALLOCATIONTYPE_ROLL,
		ALLOCATIONTYPE_GROW_SHRINK,
		ALLOCATIONTYPE_IO_CHAIN,
		ALLOCATIONTYPE_IO_GENERIC
	};

	const char* const allocationTypeStr[] =
	{
		"grow",
		"shrink",
		"roll",
		"grow_shrink",
		"input_output_chain",
		"input_output",
	};

	const TestConfig::RenderTypes renderCommands[] =
	{
		TestConfig::RENDERTYPES_NONE,
		TestConfig::RENDERTYPES_CLEAR,
		TestConfig::RENDERTYPES_DRAW,
		TestConfig::RENDERTYPES_CLEAR|TestConfig::RENDERTYPES_DRAW,
	};

	const TestConfig::CommandBufferTypes commandBuffers[] =
	{
		TestConfig::COMMANDBUFFERTYPES_INLINE,
		TestConfig::COMMANDBUFFERTYPES_SECONDARY,
		TestConfig::COMMANDBUFFERTYPES_INLINE|TestConfig::COMMANDBUFFERTYPES_SECONDARY
	};

	const TestConfig::ImageMemory imageMemories[] =
	{
		TestConfig::IMAGEMEMORY_STRICT,
		TestConfig::IMAGEMEMORY_LAZY,
		TestConfig::IMAGEMEMORY_STRICT|TestConfig::IMAGEMEMORY_LAZY
	};

	const UVec2 targetSizes[] =
	{
		UVec2(64, 64),
		UVec2(63, 65)
	};

	const UVec2 renderPositions[] =
	{
		UVec2(0, 0),
		UVec2(3, 17)
	};

	const UVec2 renderSizes[] =
	{
		UVec2(32, 32),
		UVec2(60, 47)
	};

	tcu::TestContext&				testCtx	= group->getTestContext();
	de::Random						rng		(3700649827u);

	for (size_t allocationTypeNdx = 0; allocationTypeNdx < DE_LENGTH_OF_ARRAY(allocationTypes); allocationTypeNdx++)
	{
		const AllocationType			allocationType		= allocationTypes[allocationTypeNdx];
		const size_t					testCaseCount		= 100;
		de::MovePtr<tcu::TestCaseGroup>	allocationTypeGroup	(new tcu::TestCaseGroup(testCtx, allocationTypeStr[allocationTypeNdx], allocationTypeStr[allocationTypeNdx]));

		for (size_t testCaseNdx = 0; testCaseNdx < testCaseCount; testCaseNdx++)
		{
			if (allocationType == ALLOCATIONTYPE_IO_GENERIC)
			{
				const deUint32		attachmentCount	= 4u + rng.getUint32() % 31u;
				const deUint32		subpassCount	= 4u + rng.getUint32() % 31u;
				vector<Attachment>	attachments;

				set<deUint32>		definedAttachments;

				vector<Subpass>		subpasses;
				set<deUint32>		colorAttachments;
				set<deUint32>		depthStencilAttachments;

				for (deUint32 attachmentIndex = 0; attachmentIndex < attachmentCount; attachmentIndex++)
				{
					const bool					isDepthStencilAttachment	= rng.getFloat() < 0.01f;
					const VkSampleCountFlagBits	sampleCount					= VK_SAMPLE_COUNT_1_BIT;
					const VkAttachmentLoadOp	loadOp						= rng.choose<VkAttachmentLoadOp>(DE_ARRAY_BEGIN(loadOps), DE_ARRAY_END(loadOps));
					const VkAttachmentStoreOp	storeOp						= rng.choose<VkAttachmentStoreOp>(DE_ARRAY_BEGIN(storeOps), DE_ARRAY_END(storeOps));

					const VkImageLayout			initialLayout				= isDepthStencilAttachment
																			? rng.choose<VkImageLayout>(DE_ARRAY_BEGIN(initialAndFinalDepthStencilLayouts), DE_ARRAY_END(initialAndFinalDepthStencilLayouts))
																			: rng.choose<VkImageLayout>(DE_ARRAY_BEGIN(initialAndFinalColorLayouts), DE_ARRAY_END(initialAndFinalColorLayouts));
					const VkImageLayout			finalizeLayout				= isDepthStencilAttachment
																			? rng.choose<VkImageLayout>(DE_ARRAY_BEGIN(initialAndFinalDepthStencilLayouts), DE_ARRAY_END(initialAndFinalDepthStencilLayouts))
																			: rng.choose<VkImageLayout>(DE_ARRAY_BEGIN(initialAndFinalColorLayouts), DE_ARRAY_END(initialAndFinalColorLayouts));

					const VkAttachmentLoadOp	stencilLoadOp				= rng.choose<VkAttachmentLoadOp>(DE_ARRAY_BEGIN(loadOps), DE_ARRAY_END(loadOps));
					const VkAttachmentStoreOp	stencilStoreOp				= rng.choose<VkAttachmentStoreOp>(DE_ARRAY_BEGIN(storeOps), DE_ARRAY_END(storeOps));

					if (isDepthStencilAttachment)
					{
						const VkFormat	format	= rng.choose<VkFormat>(DE_ARRAY_BEGIN(s_coreDepthStencilFormats), DE_ARRAY_END(s_coreDepthStencilFormats));

						if (loadOp == VK_ATTACHMENT_LOAD_OP_LOAD || loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR
							|| stencilLoadOp == VK_ATTACHMENT_LOAD_OP_LOAD || stencilLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
							definedAttachments.insert(attachmentIndex);

						depthStencilAttachments.insert(attachmentIndex);

						attachments.push_back(Attachment(format, sampleCount, loadOp, storeOp, stencilLoadOp, stencilStoreOp, initialLayout, finalizeLayout));
					}
					else
					{
						const VkFormat	format	= rng.choose<VkFormat>(DE_ARRAY_BEGIN(s_coreColorFormats), DE_ARRAY_END(s_coreColorFormats));

						if (loadOp == VK_ATTACHMENT_LOAD_OP_LOAD || loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
							definedAttachments.insert(attachmentIndex);

						colorAttachments.insert(attachmentIndex);

						attachments.push_back(Attachment(format, sampleCount, loadOp, storeOp, stencilLoadOp, stencilStoreOp, initialLayout, finalizeLayout));
					}
				}
				vector<Maybe<deUint32> >	lastUseOfAttachment	(attachments.size(), nothing<deUint32>());
				vector<SubpassDependency>	deps;

				for (deUint32 subpassIndex = 0; subpassIndex < subpassCount; subpassIndex++)
				{
					const deUint32				colorAttachmentCount		= depthStencilAttachments.empty()
																			? 1 + rng.getUint32() % de::min(4u, (deUint32)colorAttachments.size())
																			: rng.getUint32() % (de::min(4u, (deUint32)colorAttachments.size()) + 1u);
					const deUint32				inputAttachmentCount		= rng.getUint32() % (deUint32)(de::min<size_t>(4, definedAttachments.size()) + 1);
					const bool					useDepthStencilAttachment	= !depthStencilAttachments.empty() && (colorAttachmentCount == 0 || rng.getBool());
					std::vector<deUint32>		subpassColorAttachments		(colorAttachmentCount);
					std::vector<deUint32>		subpassInputAttachments		(inputAttachmentCount);
					Maybe<deUint32>				depthStencilAttachment		(useDepthStencilAttachment
																			? just(chooseRandom(rng, depthStencilAttachments))
																			: nothing<deUint32>());
					std::vector<deUint32>		subpassPreserveAttachments;

					rng.choose(colorAttachments.begin(), colorAttachments.end(), subpassColorAttachments.begin(), colorAttachmentCount);
					rng.choose(definedAttachments.begin(), definedAttachments.end(), subpassInputAttachments.begin(), inputAttachmentCount);

					for (size_t colorAttachmentNdx = 0; colorAttachmentNdx < subpassColorAttachments.size(); colorAttachmentNdx++)
						definedAttachments.insert(subpassColorAttachments[colorAttachmentNdx]);

					if (depthStencilAttachment)
						definedAttachments.insert(*depthStencilAttachment);

					{
						std::vector<AttachmentReference>	inputAttachmentReferences;
						std::vector<AttachmentReference>	colorAttachmentReferences;
						AttachmentReference					depthStencilAttachmentReference (VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_GENERAL);

						for (size_t colorAttachmentNdx = 0; colorAttachmentNdx < subpassColorAttachments.size(); colorAttachmentNdx++)
						{
							const deUint32		colorAttachmentIndex	= subpassColorAttachments[colorAttachmentNdx];

							if (lastUseOfAttachment[colorAttachmentIndex])
							{
								deBool foundDuplicate = false;

								const deUint32			srcPass			= *lastUseOfAttachment[colorAttachmentIndex];
								const deUint32			dstPass			= subpassIndex;
								const VkDependencyFlags dependencyFlags = rng.getBool() ? (VkDependencyFlags) VK_DEPENDENCY_BY_REGION_BIT : 0u;

								const SubpassDependency newDependency(srcPass, dstPass,
																	  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
																	  | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
																	  | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
																	  | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,

																	  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
																	  | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
																	  | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
																	  | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,

																	  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
																	  VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,

																	  dependencyFlags);

								for (SubpassDependency& dependency : deps)
								{
									if (dependency.getSrcPass() == srcPass && dependency.getDstPass() == dstPass)
									{
										const VkAccessFlags newDstFlags = dependency.getDstAccessMask() | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
										dependency.setDstAccessMask(newDstFlags);
										foundDuplicate = true;
										break;
									}
								}

								if (!foundDuplicate)
								{
									deps.push_back(newDependency);
								}
							}

							lastUseOfAttachment[colorAttachmentIndex] = just(subpassIndex);

							colorAttachmentReferences.push_back(AttachmentReference((deUint32)subpassColorAttachments[colorAttachmentNdx], VK_IMAGE_LAYOUT_GENERAL));
						}

						for (size_t inputAttachmentNdx = 0; inputAttachmentNdx < subpassInputAttachments.size(); inputAttachmentNdx++)
						{
							const deUint32		inputAttachmentIndex	= subpassInputAttachments[inputAttachmentNdx];

							if(lastUseOfAttachment[inputAttachmentIndex])
							{
								deBool foundDuplicate = false;

								const deUint32			srcPass			= *lastUseOfAttachment[inputAttachmentIndex];
								const deUint32			dstPass			= subpassIndex;
								const VkDependencyFlags dependencyFlags = ((srcPass == subpassIndex) || rng.getBool()) ? (VkDependencyFlags)VK_DEPENDENCY_BY_REGION_BIT : 0u;

								const SubpassDependency newDependency(srcPass, dstPass,
																	  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
																	  | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
																	  | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
																	  | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,

																	  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
																	  | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
																	  | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
																	  | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,

																	  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
																	  VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,

																	  dependencyFlags);
								for (SubpassDependency& dependency : deps)
								{
									if (dependency.getSrcPass() == srcPass && dependency.getDstPass() == dstPass)
									{
										const VkAccessFlags newSrcFlags = dependency.getSrcAccessMask() | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
										const VkAccessFlags newDstFlags = dependency.getDstAccessMask() | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
										dependency.setDstAccessMask(newSrcFlags);
										dependency.setDstAccessMask(newDstFlags);
										foundDuplicate = true;
										break;
									}
								}

								if (!foundDuplicate)
								{
									deps.push_back(newDependency);
								}

								lastUseOfAttachment[inputAttachmentIndex] = just(subpassIndex);

								VkImageAspectFlags aspect = 0u;
								if (testConfigExternal.renderPassType == RENDERPASS_TYPE_RENDERPASS2)
								{
									bool col = colorAttachments.find(inputAttachmentIndex) != colorAttachments.end();
									aspect = col ? VK_IMAGE_ASPECT_COLOR_BIT : VK_IMAGE_ASPECT_DEPTH_BIT;
								}
								inputAttachmentReferences.push_back(AttachmentReference((deUint32)subpassInputAttachments[inputAttachmentNdx], VK_IMAGE_LAYOUT_GENERAL, aspect));
							}
						}

						if (depthStencilAttachment)
						{
							if (lastUseOfAttachment[*depthStencilAttachment])
							{
								deBool foundDuplicate = false;

								const deUint32			srcPass			= *lastUseOfAttachment[*depthStencilAttachment];
								const deUint32			dstPass			= subpassIndex;
								const VkDependencyFlags dependencyFlags = ((srcPass == subpassIndex) || rng.getBool()) ? (VkDependencyFlags)VK_DEPENDENCY_BY_REGION_BIT : 0u;

								const SubpassDependency newDependency(srcPass, dstPass,
																	  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
																	  | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
																	  | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
																	  | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,

																	  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
																	  | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
																	  | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
																	  | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,

																	  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
																	  VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,

																	  dependencyFlags);
								for (SubpassDependency& dependency : deps)
								{
									if (dependency.getSrcPass() == srcPass && dependency.getDstPass() == dstPass)
									{
										const VkAccessFlags newSrcFlags = dependency.getSrcAccessMask() | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
										const VkAccessFlags newDstFlags = dependency.getDstAccessMask() | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
										dependency.setDstAccessMask(newSrcFlags);
										dependency.setDstAccessMask(newDstFlags);
										foundDuplicate = true;
										break;
									}
								}

								if (!foundDuplicate)
								{
									deps.push_back(newDependency);
								}
							}

							lastUseOfAttachment[*depthStencilAttachment] = just(subpassIndex);

							depthStencilAttachmentReference = AttachmentReference(*depthStencilAttachment, VK_IMAGE_LAYOUT_GENERAL);
						}
						else
							depthStencilAttachmentReference = AttachmentReference(VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_GENERAL);

						vector<deUint32>	preserveAttachments;
						for (deUint32 attachmentIndex = 0; attachmentIndex < (deUint32)attachments.size(); attachmentIndex++)
						{
							if (lastUseOfAttachment[attachmentIndex] && (*lastUseOfAttachment[attachmentIndex]) != subpassIndex)
								preserveAttachments.push_back(attachmentIndex);
						}

						// Use random image layout when possible
						for (size_t colorRefIdx = 0; colorRefIdx < colorAttachmentReferences.size(); ++colorRefIdx)
						{
							bool usedAsInput = false;
							for (size_t inputRefIdx = 0; inputRefIdx < inputAttachmentReferences.size(); ++inputRefIdx)
								if (colorAttachmentReferences[colorRefIdx].getAttachment() == inputAttachmentReferences[inputRefIdx].getAttachment())
									usedAsInput = true;

							if (!usedAsInput)
								colorAttachmentReferences[colorRefIdx].setImageLayout(rng.choose<VkImageLayout>(DE_ARRAY_BEGIN(subpassLayoutsColor), DE_ARRAY_END(subpassLayoutsColor)));
						}
						for (size_t inputRefIdx = 0; inputRefIdx < inputAttachmentReferences.size(); ++inputRefIdx)
						{
							bool usedAsDepthStencil	= inputAttachmentReferences[inputRefIdx].getAttachment() == depthStencilAttachmentReference.getAttachment();
							bool usedAsColor		= false;
							for (size_t colorRefIdx = 0; colorRefIdx < colorAttachmentReferences.size(); ++colorRefIdx)
								if (inputAttachmentReferences[inputRefIdx].getAttachment() == colorAttachmentReferences[colorRefIdx].getAttachment())
									usedAsColor = true;

							if (!usedAsColor && !usedAsDepthStencil)
								inputAttachmentReferences[inputRefIdx].setImageLayout(rng.choose<VkImageLayout>(DE_ARRAY_BEGIN(subpassLayoutsInput), DE_ARRAY_END(subpassLayoutsInput)));
						}
						{
							bool usedAsInput = false;
							for (size_t inputRefIdx = 0; inputRefIdx < inputAttachmentReferences.size(); ++inputRefIdx)
								if (depthStencilAttachmentReference.getAttachment() == inputAttachmentReferences[inputRefIdx].getAttachment())
									usedAsInput = true;

							if (!usedAsInput)
								depthStencilAttachmentReference.setImageLayout(rng.choose<VkImageLayout>(DE_ARRAY_BEGIN(subpassLayoutsDepthStencil), DE_ARRAY_END(subpassLayoutsDepthStencil)));
						}

						subpasses.push_back(Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS, 0u,
												inputAttachmentReferences,
												colorAttachmentReferences,
												vector<AttachmentReference>(),
												depthStencilAttachmentReference,
												preserveAttachments));
					}
				}
				{
					const TestConfig::RenderTypes			render			= rng.choose<TestConfig::RenderTypes>(DE_ARRAY_BEGIN(renderCommands), DE_ARRAY_END(renderCommands));
					const TestConfig::CommandBufferTypes	commandBuffer	= rng.choose<TestConfig::CommandBufferTypes>(DE_ARRAY_BEGIN(commandBuffers), DE_ARRAY_END(commandBuffers));
					const TestConfig::ImageMemory			imageMemory		= rng.choose<TestConfig::ImageMemory>(DE_ARRAY_BEGIN(imageMemories), DE_ARRAY_END(imageMemories));

					const string							testCaseName	= de::toString(testCaseNdx);
					const UVec2								targetSize		= rng.choose<UVec2>(DE_ARRAY_BEGIN(targetSizes), DE_ARRAY_END(targetSizes));
					const UVec2								renderPos		= rng.choose<UVec2>(DE_ARRAY_BEGIN(renderPositions), DE_ARRAY_END(renderPositions));
					const UVec2								renderSize		= rng.choose<UVec2>(DE_ARRAY_BEGIN(renderSizes), DE_ARRAY_END(renderSizes));

					const RenderPass						renderPass		(attachments, subpasses, deps);
					const TestConfig						testConfig		(renderPass,
																			 render,
																			 commandBuffer,
																			 imageMemory,
																			 targetSize,
																			 renderPos,
																			 renderSize,
																			 DE_FALSE,
																			 80329,
																			 0,
																			 testConfigExternal.allocationKind,
																			 testConfigExternal.renderPassType);

					addFunctionCaseWithPrograms<TestConfig>(allocationTypeGroup.get(), testCaseName.c_str(), testCaseName.c_str(), createTestShaders, renderPassTest, testConfig);
				}
			}
			else
			{
				const deUint32		attachmentCount	= rng.choose<deUint32>(DE_ARRAY_BEGIN(attachmentCounts), DE_ARRAY_END(attachmentCounts));
				vector<Attachment>	attachments;
				vector<Subpass>		subpasses;

				for (size_t attachmentNdx = 0; attachmentNdx < attachmentCount; attachmentNdx++)
				{
					const VkSampleCountFlagBits	sampleCount		= VK_SAMPLE_COUNT_1_BIT;
					const VkFormat				format			= rng.choose<VkFormat>(DE_ARRAY_BEGIN(s_coreColorFormats), DE_ARRAY_END(s_coreColorFormats));
					const VkAttachmentLoadOp	loadOp			= rng.choose<VkAttachmentLoadOp>(DE_ARRAY_BEGIN(loadOps), DE_ARRAY_END(loadOps));
					const VkAttachmentStoreOp	storeOp			= rng.choose<VkAttachmentStoreOp>(DE_ARRAY_BEGIN(storeOps), DE_ARRAY_END(storeOps));

					const VkImageLayout			initialLayout	= rng.choose<VkImageLayout>(DE_ARRAY_BEGIN(initialAndFinalColorLayouts), DE_ARRAY_END(initialAndFinalColorLayouts));
					const VkImageLayout			finalizeLayout	= rng.choose<VkImageLayout>(DE_ARRAY_BEGIN(initialAndFinalColorLayouts), DE_ARRAY_END(initialAndFinalColorLayouts));

					const VkAttachmentLoadOp	stencilLoadOp	= rng.choose<VkAttachmentLoadOp>(DE_ARRAY_BEGIN(loadOps), DE_ARRAY_END(loadOps));
					const VkAttachmentStoreOp	stencilStoreOp	= rng.choose<VkAttachmentStoreOp>(DE_ARRAY_BEGIN(storeOps), DE_ARRAY_END(storeOps));

					attachments.push_back(Attachment(format, sampleCount, loadOp, storeOp, stencilLoadOp, stencilStoreOp, initialLayout, finalizeLayout));
				}

				if (allocationType == ALLOCATIONTYPE_GROW)
				{
					for (size_t subpassNdx = 0; subpassNdx < attachmentCount; subpassNdx++)
					{
						vector<AttachmentReference>	colorAttachmentReferences;

						for (size_t attachmentNdx = 0; attachmentNdx < subpassNdx + 1; attachmentNdx++)
						{
							const VkImageLayout subpassLayout = rng.choose<VkImageLayout>(DE_ARRAY_BEGIN(subpassLayoutsColor), DE_ARRAY_END(subpassLayoutsColor));

							colorAttachmentReferences.push_back(AttachmentReference((deUint32)attachmentNdx, subpassLayout));
						}

						subpasses.push_back(Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS, 0u,
												vector<AttachmentReference>(),
												colorAttachmentReferences,
												vector<AttachmentReference>(),
												AttachmentReference(VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_GENERAL),
												vector<deUint32>()));
					}
				}
				else if (allocationType == ALLOCATIONTYPE_SHRINK)
				{
					for (size_t subpassNdx = 0; subpassNdx < attachmentCount; subpassNdx++)
					{
						vector<AttachmentReference>	colorAttachmentReferences;

						for (size_t attachmentNdx = 0; attachmentNdx < (attachmentCount - subpassNdx); attachmentNdx++)
						{
							const VkImageLayout subpassLayout = rng.choose<VkImageLayout>(DE_ARRAY_BEGIN(subpassLayoutsColor), DE_ARRAY_END(subpassLayoutsColor));

							colorAttachmentReferences.push_back(AttachmentReference((deUint32)attachmentNdx, subpassLayout));
						}

						subpasses.push_back(Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS, 0u,
													vector<AttachmentReference>(),
													colorAttachmentReferences,
													vector<AttachmentReference>(),
													AttachmentReference(VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_GENERAL),
													vector<deUint32>()));
					}
				}
				else if (allocationType == ALLOCATIONTYPE_ROLL)
				{
					for (size_t subpassNdx = 0; subpassNdx < attachmentCount / 2; subpassNdx++)
					{
						vector<AttachmentReference>	colorAttachmentReferences;

						for (size_t attachmentNdx = 0; attachmentNdx < attachmentCount / 2; attachmentNdx++)
						{
							const VkImageLayout subpassLayout = rng.choose<VkImageLayout>(DE_ARRAY_BEGIN(subpassLayoutsColor), DE_ARRAY_END(subpassLayoutsColor));

							colorAttachmentReferences.push_back(AttachmentReference((deUint32)(subpassNdx + attachmentNdx), subpassLayout));
						}

						subpasses.push_back(Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS, 0u,
													vector<AttachmentReference>(),
													colorAttachmentReferences,
													vector<AttachmentReference>(),
													AttachmentReference(VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_GENERAL),
													vector<deUint32>()));
					}
				}
				else if (allocationType == ALLOCATIONTYPE_GROW_SHRINK)
				{
					for (size_t subpassNdx = 0; subpassNdx < attachmentCount; subpassNdx++)
					{
						vector<AttachmentReference>	colorAttachmentReferences;

						for (size_t attachmentNdx = 0; attachmentNdx < subpassNdx + 1; attachmentNdx++)
						{
							const VkImageLayout subpassLayout = rng.choose<VkImageLayout>(DE_ARRAY_BEGIN(subpassLayoutsColor), DE_ARRAY_END(subpassLayoutsColor));

							colorAttachmentReferences.push_back(AttachmentReference((deUint32)attachmentNdx, subpassLayout));
						}

						subpasses.push_back(Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS, 0u,
													vector<AttachmentReference>(),
													colorAttachmentReferences,
													vector<AttachmentReference>(),
													AttachmentReference(VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_GENERAL),
													vector<deUint32>()));
					}
					for (size_t subpassNdx = 0; subpassNdx < attachmentCount; subpassNdx++)
					{
						vector<AttachmentReference>	colorAttachmentReferences;

						for (size_t attachmentNdx = 0; attachmentNdx < (attachmentCount - subpassNdx); attachmentNdx++)
						{
							const VkImageLayout subpassLayout = rng.choose<VkImageLayout>(DE_ARRAY_BEGIN(subpassLayoutsColor), DE_ARRAY_END(subpassLayoutsColor));

							colorAttachmentReferences.push_back(AttachmentReference((deUint32)attachmentNdx, subpassLayout));
						}

						subpasses.push_back(Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS, 0u,
													vector<AttachmentReference>(),
													colorAttachmentReferences,
													vector<AttachmentReference>(),
													AttachmentReference(VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_GENERAL),
													vector<deUint32>()));
					}
				}
				else if (allocationType == ALLOCATIONTYPE_IO_CHAIN)
				{
					subpasses.push_back(Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS, 0u,
											vector<AttachmentReference>(),
											vector<AttachmentReference>(1, AttachmentReference(0, rng.choose<VkImageLayout>(DE_ARRAY_BEGIN(subpassLayoutsColor), DE_ARRAY_END(subpassLayoutsColor)))),
											vector<AttachmentReference>(),
											AttachmentReference(VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_GENERAL),
											vector<deUint32>()));

					for (size_t subpassNdx = 1; subpassNdx < attachmentCount; subpassNdx++)
					{
						const VkImageAspectFlags inputAttachmentAspectMask = (testConfigExternal.renderPassType == RENDERPASS_TYPE_RENDERPASS2) ? VK_IMAGE_ASPECT_COLOR_BIT : static_cast<VkImageAspectFlagBits>(0);
						subpasses.push_back(Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS, 0u,
												vector<AttachmentReference>(1, AttachmentReference((deUint32)(subpassNdx - 1), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, inputAttachmentAspectMask)),
												vector<AttachmentReference>(1, AttachmentReference((deUint32)(subpassNdx), rng.choose<VkImageLayout>(DE_ARRAY_BEGIN(subpassLayoutsColor), DE_ARRAY_END(subpassLayoutsColor)))),
												vector<AttachmentReference>(),
												AttachmentReference(VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_GENERAL),
												vector<deUint32>()));
					}
				}
				else
					DE_FATAL("Unknown allocation type");

				{
					const TestConfig::RenderTypes			render			= rng.choose<TestConfig::RenderTypes>(DE_ARRAY_BEGIN(renderCommands), DE_ARRAY_END(renderCommands));
					const TestConfig::CommandBufferTypes	commandBuffer	= rng.choose<TestConfig::CommandBufferTypes>(DE_ARRAY_BEGIN(commandBuffers), DE_ARRAY_END(commandBuffers));
					const TestConfig::ImageMemory			imageMemory		= rng.choose<TestConfig::ImageMemory>(DE_ARRAY_BEGIN(imageMemories), DE_ARRAY_END(imageMemories));

					const string							testCaseName	= de::toString(testCaseNdx);
					const UVec2								targetSize		= rng.choose<UVec2>(DE_ARRAY_BEGIN(targetSizes), DE_ARRAY_END(targetSizes));
					const UVec2								renderPos		= rng.choose<UVec2>(DE_ARRAY_BEGIN(renderPositions), DE_ARRAY_END(renderPositions));
					const UVec2								renderSize		= rng.choose<UVec2>(DE_ARRAY_BEGIN(renderSizes), DE_ARRAY_END(renderSizes));

					vector<SubpassDependency>				deps;

					for (size_t subpassNdx = 0; subpassNdx < subpasses.size() - 1; subpassNdx++)
					{
						const bool byRegion				= rng.getBool();
						deps.push_back(SubpassDependency((deUint32)subpassNdx, (deUint32)subpassNdx + 1,
														 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
															| VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
															| VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
															| VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,

														 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
															| VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
															| VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
															| VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,

														 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
														 VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,

														 byRegion ? (VkDependencyFlags)VK_DEPENDENCY_BY_REGION_BIT : 0u));
					}

					const RenderPass					renderPass		(attachments, subpasses, deps);
					const TestConfig					testConfig		(renderPass,
																		 render,
																		 commandBuffer,
																		 imageMemory,
																		 targetSize,
																		 renderPos,
																		 renderSize,
																		 DE_FALSE,
																		 80329,
																		 0,
																		 testConfigExternal.allocationKind,
																		 testConfigExternal.renderPassType);

					addFunctionCaseWithPrograms<TestConfig>(allocationTypeGroup.get(), testCaseName.c_str(), testCaseName.c_str(), createTestShaders, renderPassTest, testConfig);
				}
			}
		}
		group->addChild(allocationTypeGroup.release());
	}
}

void addSimpleTests (tcu::TestCaseGroup* group, const TestConfigExternal testConfigExternal)
{
	const UVec2	targetSize	(64, 64);
	const UVec2	renderPos	(0, 0);
	const UVec2	renderSize	(64, 64);

	// color
	{
		const RenderPass	renderPass	(vector<Attachment>(1, Attachment(VK_FORMAT_R8G8B8A8_UNORM,
																		  VK_SAMPLE_COUNT_1_BIT,
																		  VK_ATTACHMENT_LOAD_OP_CLEAR,
																		  VK_ATTACHMENT_STORE_OP_STORE,
																		  VK_ATTACHMENT_LOAD_OP_DONT_CARE,
																		  VK_ATTACHMENT_STORE_OP_DONT_CARE,
																		  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
																		  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)),
										 vector<Subpass>(1, Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS,
																	0u,
																	vector<AttachmentReference>(),
																	vector<AttachmentReference>(1, AttachmentReference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)),
																	vector<AttachmentReference>(),
																	AttachmentReference(VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_GENERAL),
																	vector<deUint32>())),
										 vector<SubpassDependency>());
		const TestConfig	testConfig	(renderPass,
										 TestConfig::RENDERTYPES_DRAW,
										 TestConfig::COMMANDBUFFERTYPES_INLINE,
										 TestConfig::IMAGEMEMORY_STRICT,
										 targetSize,
										 renderPos,
										 renderSize,
										 DE_FALSE,
										 90239,
										 0,
										 testConfigExternal.allocationKind,
										 testConfigExternal.renderPassType);

		addFunctionCaseWithPrograms<TestConfig>(group, "color", "Single color attachment case.", createTestShaders, renderPassTest, testConfig);
	}

	// depth
	{
		const RenderPass	renderPass	(vector<Attachment>(1, Attachment(VK_FORMAT_X8_D24_UNORM_PACK32,
																		  VK_SAMPLE_COUNT_1_BIT,
																		  VK_ATTACHMENT_LOAD_OP_CLEAR,
																		  VK_ATTACHMENT_STORE_OP_STORE,
																		  VK_ATTACHMENT_LOAD_OP_DONT_CARE,
																		  VK_ATTACHMENT_STORE_OP_DONT_CARE,
																		  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
																		  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)),
										 vector<Subpass>(1, Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS,
																	0u,
																	vector<AttachmentReference>(),
																	vector<AttachmentReference>(),
																	vector<AttachmentReference>(),
																	AttachmentReference(0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL),
																	vector<deUint32>())),
										 vector<SubpassDependency>());
		const TestConfig	testConfig	(renderPass,
										 TestConfig::RENDERTYPES_DRAW,
										 TestConfig::COMMANDBUFFERTYPES_INLINE,
										 TestConfig::IMAGEMEMORY_STRICT,
										 targetSize,
										 renderPos,
										 renderSize,
										 DE_FALSE,
										 90239,
										 0,
										 testConfigExternal.allocationKind,
										 testConfigExternal.renderPassType);

		addFunctionCaseWithPrograms<TestConfig>(group, "depth", "Single depth attachment case.", createTestShaders, renderPassTest, testConfig);
	}

	// stencil
	{
		const RenderPass	renderPass	(vector<Attachment>(1, Attachment(VK_FORMAT_S8_UINT,
																		  VK_SAMPLE_COUNT_1_BIT,
																		  VK_ATTACHMENT_LOAD_OP_DONT_CARE,
																		  VK_ATTACHMENT_STORE_OP_DONT_CARE,
																		  VK_ATTACHMENT_LOAD_OP_CLEAR,
																		  VK_ATTACHMENT_STORE_OP_STORE,
																		  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
																		  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)),
										 vector<Subpass>(1, Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS,
																	0u,
																	vector<AttachmentReference>(),
																	vector<AttachmentReference>(),
																	vector<AttachmentReference>(),
																	AttachmentReference(0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL),
																	vector<deUint32>())),
										 vector<SubpassDependency>());
		const TestConfig	testConfig	(renderPass,
										 TestConfig::RENDERTYPES_DRAW,
										 TestConfig::COMMANDBUFFERTYPES_INLINE,
										 TestConfig::IMAGEMEMORY_STRICT,
										 targetSize,
										 renderPos,
										 renderSize,
										 DE_FALSE,
										 90239,
										 0,
										 testConfigExternal.allocationKind,
										 testConfigExternal.renderPassType);

		addFunctionCaseWithPrograms<TestConfig>(group, "stencil", "Single stencil attachment case.", createTestShaders, renderPassTest, testConfig);
	}

	// depth_stencil
	{
		const RenderPass	renderPass	(vector<Attachment>(1, Attachment(VK_FORMAT_D24_UNORM_S8_UINT,
																		  VK_SAMPLE_COUNT_1_BIT,
																		  VK_ATTACHMENT_LOAD_OP_CLEAR,
																		  VK_ATTACHMENT_STORE_OP_STORE,
																		  VK_ATTACHMENT_LOAD_OP_CLEAR,
																		  VK_ATTACHMENT_STORE_OP_STORE,
																		  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
																		  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)),
										 vector<Subpass>(1, Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS,
																	0u,
																	vector<AttachmentReference>(),
																	vector<AttachmentReference>(),
																	vector<AttachmentReference>(),
																	AttachmentReference(0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL),
																	vector<deUint32>())),
										 vector<SubpassDependency>());
		const TestConfig	testConfig	(renderPass,
										 TestConfig::RENDERTYPES_DRAW,
										 TestConfig::COMMANDBUFFERTYPES_INLINE,
										 TestConfig::IMAGEMEMORY_STRICT,
										 targetSize,
										 renderPos,
										 renderSize,
										 DE_FALSE,
										 90239,
										 0,
										 testConfigExternal.allocationKind,
										 testConfigExternal.renderPassType);

		addFunctionCaseWithPrograms<TestConfig>(group, "depth_stencil", "Single depth stencil attachment case.", createTestShaders, renderPassTest, testConfig);
	}

	// color_depth
	{
		const Attachment	attachments[] =
		{
			Attachment(VK_FORMAT_R8G8B8A8_UNORM,
					   VK_SAMPLE_COUNT_1_BIT,
					   VK_ATTACHMENT_LOAD_OP_CLEAR,
					   VK_ATTACHMENT_STORE_OP_STORE,
					   VK_ATTACHMENT_LOAD_OP_DONT_CARE,
					   VK_ATTACHMENT_STORE_OP_DONT_CARE,
					   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
			Attachment(VK_FORMAT_X8_D24_UNORM_PACK32,
					   VK_SAMPLE_COUNT_1_BIT,
					   VK_ATTACHMENT_LOAD_OP_CLEAR,
					   VK_ATTACHMENT_STORE_OP_STORE,
					   VK_ATTACHMENT_LOAD_OP_DONT_CARE,
					   VK_ATTACHMENT_STORE_OP_DONT_CARE,
					   VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
					   VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL),
		};

		const RenderPass	renderPass	(vector<Attachment>(DE_ARRAY_BEGIN(attachments), DE_ARRAY_END(attachments)),
										 vector<Subpass>(1, Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS,
																	0u,
																	vector<AttachmentReference>(),
																	vector<AttachmentReference>(1, AttachmentReference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)),
																	vector<AttachmentReference>(),
																	AttachmentReference(1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL),
																	vector<deUint32>())),
										 vector<SubpassDependency>());
		const TestConfig	testConfig	(renderPass,
										 TestConfig::RENDERTYPES_DRAW,
										 TestConfig::COMMANDBUFFERTYPES_INLINE,
										 TestConfig::IMAGEMEMORY_STRICT,
										 targetSize,
										 renderPos,
										 renderSize,
										 DE_FALSE,
										 90239,
										 0,
										 testConfigExternal.allocationKind,
										 testConfigExternal.renderPassType);

		addFunctionCaseWithPrograms<TestConfig>(group, "color_depth", "Color and depth attachment case.", createTestShaders, renderPassTest, testConfig);
	}

	// color_stencil
	{
		const Attachment	attachments[] =
		{
			Attachment(VK_FORMAT_R8G8B8A8_UNORM,
					   VK_SAMPLE_COUNT_1_BIT,
					   VK_ATTACHMENT_LOAD_OP_CLEAR,
					   VK_ATTACHMENT_STORE_OP_STORE,
					   VK_ATTACHMENT_LOAD_OP_DONT_CARE,
					   VK_ATTACHMENT_STORE_OP_DONT_CARE,
					   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
			Attachment(VK_FORMAT_S8_UINT,
					   VK_SAMPLE_COUNT_1_BIT,
					   VK_ATTACHMENT_LOAD_OP_DONT_CARE,
					   VK_ATTACHMENT_STORE_OP_DONT_CARE,
					   VK_ATTACHMENT_LOAD_OP_CLEAR,
					   VK_ATTACHMENT_STORE_OP_STORE,
					   VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
					   VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL),
		};

		const RenderPass	renderPass	(vector<Attachment>(DE_ARRAY_BEGIN(attachments), DE_ARRAY_END(attachments)),
										 vector<Subpass>(1, Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS,
																	0u,
																	vector<AttachmentReference>(),
																	vector<AttachmentReference>(1, AttachmentReference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)),
																	vector<AttachmentReference>(),
																	AttachmentReference(1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL),
																	vector<deUint32>())),
										 vector<SubpassDependency>());
		const TestConfig	testConfig	(renderPass,
										 TestConfig::RENDERTYPES_DRAW,
										 TestConfig::COMMANDBUFFERTYPES_INLINE,
										 TestConfig::IMAGEMEMORY_STRICT,
										 targetSize,
										 renderPos,
										 renderSize,
										 DE_FALSE,
										 90239,
										 0,
										 testConfigExternal.allocationKind,
										 testConfigExternal.renderPassType);

		addFunctionCaseWithPrograms<TestConfig>(group, "color_stencil", "Color and stencil attachment case.", createTestShaders, renderPassTest, testConfig);
	}

	// color_depth_stencil
	{
		const Attachment	attachments[] =
		{
			Attachment(VK_FORMAT_R8G8B8A8_UNORM,
					   VK_SAMPLE_COUNT_1_BIT,
					   VK_ATTACHMENT_LOAD_OP_CLEAR,
					   VK_ATTACHMENT_STORE_OP_STORE,
					   VK_ATTACHMENT_LOAD_OP_DONT_CARE,
					   VK_ATTACHMENT_STORE_OP_DONT_CARE,
					   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
			Attachment(VK_FORMAT_D24_UNORM_S8_UINT,
					   VK_SAMPLE_COUNT_1_BIT,
					   VK_ATTACHMENT_LOAD_OP_CLEAR,
					   VK_ATTACHMENT_STORE_OP_STORE,
					   VK_ATTACHMENT_LOAD_OP_CLEAR,
					   VK_ATTACHMENT_STORE_OP_STORE,
					   VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
					   VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL),
		};

		const RenderPass	renderPass	(vector<Attachment>(DE_ARRAY_BEGIN(attachments), DE_ARRAY_END(attachments)),
										 vector<Subpass>(1, Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS,
																	0u,
																	vector<AttachmentReference>(),
																	vector<AttachmentReference>(1, AttachmentReference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)),
																	vector<AttachmentReference>(),
																	AttachmentReference(1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL),
																	vector<deUint32>())),
										 vector<SubpassDependency>());
		const TestConfig	testConfig	(renderPass,
										 TestConfig::RENDERTYPES_DRAW,
										 TestConfig::COMMANDBUFFERTYPES_INLINE,
										 TestConfig::IMAGEMEMORY_STRICT,
										 targetSize,
										 renderPos,
										 renderSize,
										 DE_FALSE,
										 90239,
										 0,
										 testConfigExternal.allocationKind,
										 testConfigExternal.renderPassType);

		addFunctionCaseWithPrograms<TestConfig>(group, "color_depth_stencil", "Color, depth and stencil attachment case.", createTestShaders, renderPassTest, testConfig);
	}

	// no attachments
	{
		const RenderPass	renderPass	(vector<Attachment>(),
										 vector<Subpass>(1, Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS,
																	0u,
																	vector<AttachmentReference>(),
																	vector<AttachmentReference>(),
																	vector<AttachmentReference>(),
																	AttachmentReference(VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_GENERAL),
																	vector<deUint32>())),
										vector<SubpassDependency>());
		const TestConfig	testConfig	(renderPass,
										 TestConfig::RENDERTYPES_DRAW,
										 TestConfig::COMMANDBUFFERTYPES_INLINE,
										 TestConfig::IMAGEMEMORY_STRICT,
										 targetSize,
										 renderPos,
										 renderSize,
										 DE_FALSE,
										 90239,
										 0,
										 testConfigExternal.allocationKind,
										 testConfigExternal.renderPassType);

		addFunctionCaseWithPrograms<TestConfig>(group, "no_attachments", "No attachments case.", createTestShaders, renderPassTest, testConfig);
	}

	// color_unused_omit_blend_state
	{
		vector<Subpass>		subpasses;

		// First subpass: use color attachment, create pipeline with color blend state
		subpasses.push_back(Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS,
									0u,
									vector<AttachmentReference>(),
									vector<AttachmentReference>(1, AttachmentReference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)),
									vector<AttachmentReference>(),
									AttachmentReference(VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_GENERAL),
									vector<deUint32>(),
									false));

		// Second subpass: don't use color attachment, create pipeline without color blend state
		subpasses.push_back(Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS,
									0u,
									vector<AttachmentReference>(),
									vector<AttachmentReference>(1, AttachmentReference(VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)),
									vector<AttachmentReference>(),
									AttachmentReference(VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_GENERAL),
									vector<deUint32>(),
									true));

		const RenderPass	renderPass	(vector<Attachment>(1, Attachment(VK_FORMAT_R8G8B8A8_UNORM,
																		  VK_SAMPLE_COUNT_1_BIT,
																		  VK_ATTACHMENT_LOAD_OP_CLEAR,
																		  VK_ATTACHMENT_STORE_OP_STORE,
																		  VK_ATTACHMENT_LOAD_OP_DONT_CARE,
																		  VK_ATTACHMENT_STORE_OP_DONT_CARE,
																		  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
																		  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)),
										 subpasses,
										 vector<SubpassDependency>());

		const TestConfig	testConfig	(renderPass,
										 TestConfig::RENDERTYPES_DRAW,
										 TestConfig::COMMANDBUFFERTYPES_INLINE,
										 TestConfig::IMAGEMEMORY_STRICT,
										 targetSize,
										 renderPos,
										 renderSize,
										 DE_FALSE,
										 90239,
										 0,
										 testConfigExternal.allocationKind,
										 testConfigExternal.renderPassType);
		addFunctionCaseWithPrograms<TestConfig>(group, "color_unused_omit_blend_state", "Two unused color attachment case without blend state", createTestShaders, renderPassTest, testConfig);
	}
}

std::string formatToName (VkFormat format)
{
	const std::string	formatStr	= de::toString(format);
	const std::string	prefix		= "VK_FORMAT_";

	DE_ASSERT(formatStr.substr(0, prefix.length()) == prefix);

	return de::toLower(formatStr.substr(prefix.length()));
}

void addFormatTests (tcu::TestCaseGroup* group, const TestConfigExternal testConfigExternal)
{
	tcu::TestContext&	testCtx		= group->getTestContext();

	const UVec2			targetSize	(64, 64);
	const UVec2			renderPos	(0, 0);
	const UVec2			renderSize	(64, 64);

	const struct
	{
		const char* const			str;
		const VkAttachmentStoreOp	op;
	} storeOps[] =
	{
		{ "store",		VK_ATTACHMENT_STORE_OP_STORE		},
		{ "dont_care",	VK_ATTACHMENT_STORE_OP_DONT_CARE	}
	};

	const struct
	{
		const char* const			str;
		const VkAttachmentLoadOp	op;
	} loadOps[] =
	{
		{ "clear",		VK_ATTACHMENT_LOAD_OP_CLEAR		},
		{ "load",		VK_ATTACHMENT_LOAD_OP_LOAD		},
		{ "dont_care",	VK_ATTACHMENT_LOAD_OP_DONT_CARE	}
	};

	const struct
	{
		 const char* const				str;
		 const TestConfig::RenderTypes	types;
	} renderTypes[] =
	{
		{ "clear",		TestConfig::RENDERTYPES_CLEAR								},
		{ "draw",		TestConfig::RENDERTYPES_DRAW								},
		{ "clear_draw",	TestConfig::RENDERTYPES_CLEAR|TestConfig::RENDERTYPES_DRAW	}
	};

	// Color formats
	for (size_t formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(s_coreColorFormats); formatNdx++)
	{
		const VkFormat					format		= s_coreColorFormats[formatNdx];
		de::MovePtr<tcu::TestCaseGroup>	formatGroup	(new tcu::TestCaseGroup(testCtx, formatToName(format).c_str(), de::toString(format).c_str()));

		for (size_t loadOpNdx = 0; loadOpNdx < DE_LENGTH_OF_ARRAY(loadOps); loadOpNdx++)
		{
			const VkAttachmentLoadOp		loadOp	= loadOps[loadOpNdx].op;
			de::MovePtr<tcu::TestCaseGroup>	loadOpGroup	(new tcu::TestCaseGroup(testCtx, loadOps[loadOpNdx].str, loadOps[loadOpNdx].str));

			for (size_t renderTypeNdx = 0; renderTypeNdx < DE_LENGTH_OF_ARRAY(renderTypes); renderTypeNdx++)
			{
				const RenderPass	renderPass	(vector<Attachment>(1, Attachment(format,
																				  VK_SAMPLE_COUNT_1_BIT,
																				  loadOp,
																				  VK_ATTACHMENT_STORE_OP_STORE,
																				  VK_ATTACHMENT_LOAD_OP_DONT_CARE,
																				  VK_ATTACHMENT_STORE_OP_DONT_CARE,
																				  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
																				  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)),
												 vector<Subpass>(1, Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS,
																			0u,
																			vector<AttachmentReference>(),
																			vector<AttachmentReference>(1, AttachmentReference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)),
																			vector<AttachmentReference>(),
																			AttachmentReference(VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_GENERAL),
																			vector<deUint32>())),
												 vector<SubpassDependency>());
				const TestConfig	testConfig	(renderPass,
												 renderTypes[renderTypeNdx].types,
												 TestConfig::COMMANDBUFFERTYPES_INLINE,
												 TestConfig::IMAGEMEMORY_STRICT,
												 targetSize,
												 renderPos,
												 renderSize,
												 DE_FALSE,
												 90239,
												 0,
												 testConfigExternal.allocationKind,
												 testConfigExternal.renderPassType);

				addFunctionCaseWithPrograms<TestConfig>(loadOpGroup.get(), renderTypes[renderTypeNdx].str, renderTypes[renderTypeNdx].str, createTestShaders, renderPassTest, testConfig);
			}

			formatGroup->addChild(loadOpGroup.release());
		}

		{
			de::MovePtr<tcu::TestCaseGroup>	inputGroup (new tcu::TestCaseGroup(testCtx, "input", "Test attachment format as input"));

			for (size_t loadOpNdx = 0; loadOpNdx < DE_LENGTH_OF_ARRAY(loadOps); loadOpNdx++)
			{
				const VkAttachmentLoadOp		loadOp		= loadOps[loadOpNdx].op;
				de::MovePtr<tcu::TestCaseGroup>	loadOpGroup	(new tcu::TestCaseGroup(testCtx, loadOps[loadOpNdx].str, loadOps[loadOpNdx].str));

				for (size_t storeOpNdx = 0; storeOpNdx < DE_LENGTH_OF_ARRAY(storeOps); storeOpNdx++)
				{
					const VkImageAspectFlags		inputAttachmentAspectMask	= (testConfigExternal.renderPassType == RENDERPASS_TYPE_RENDERPASS2)
																				? static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_COLOR_BIT)
																				: static_cast<VkImageAspectFlags>(0);
					const VkAttachmentStoreOp		storeOp						= storeOps[storeOpNdx].op;
					de::MovePtr<tcu::TestCaseGroup>	storeOpGroup				(new tcu::TestCaseGroup(testCtx, storeOps[storeOpNdx].str, storeOps[storeOpNdx].str));

					for (size_t useInputAspectNdx = 0; useInputAspectNdx < 2; useInputAspectNdx++)
					{
						const bool useInputAspect = useInputAspectNdx != 0;

						if (testConfigExternal.renderPassType == RENDERPASS_TYPE_RENDERPASS2 && useInputAspect)
							continue;

						for (size_t renderTypeNdx = 0; renderTypeNdx < DE_LENGTH_OF_ARRAY(renderTypes); renderTypeNdx++)
						{
							{
								vector<Attachment>							attachments;
								vector<Subpass>								subpasses;
								vector<SubpassDependency>					deps;
								vector<VkInputAttachmentAspectReference>	inputAspects;

								attachments.push_back(Attachment(format,
																 VK_SAMPLE_COUNT_1_BIT,
																 loadOp,
																 storeOp,
																 VK_ATTACHMENT_LOAD_OP_DONT_CARE,
																 VK_ATTACHMENT_STORE_OP_DONT_CARE,
																 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
																 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));

								attachments.push_back(Attachment(vk::VK_FORMAT_R8G8B8A8_UNORM,
																 VK_SAMPLE_COUNT_1_BIT,
																 VK_ATTACHMENT_LOAD_OP_DONT_CARE,
																 VK_ATTACHMENT_STORE_OP_STORE,
																 VK_ATTACHMENT_LOAD_OP_DONT_CARE,
																 VK_ATTACHMENT_STORE_OP_DONT_CARE,
																 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
																 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));

								subpasses.push_back(Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS,
															0u,
															vector<AttachmentReference>(),
															vector<AttachmentReference>(1, AttachmentReference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)),
															vector<AttachmentReference>(),
															AttachmentReference(VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_GENERAL),
															vector<deUint32>()));
								subpasses.push_back(Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS,
															0u,
															vector<AttachmentReference>(1, AttachmentReference(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, inputAttachmentAspectMask)),
															vector<AttachmentReference>(1, AttachmentReference(1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)),
															vector<AttachmentReference>(),
															AttachmentReference(VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_GENERAL),
															vector<deUint32>()));

								deps.push_back(SubpassDependency(0, 1,

																vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
																vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,

																vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
																vk::VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
																vk::VK_DEPENDENCY_BY_REGION_BIT));

								if (useInputAspect)
								{
									const VkInputAttachmentAspectReference inputAspect =
									{
										1u,
										0u,
										VK_IMAGE_ASPECT_COLOR_BIT
									};

									inputAspects.push_back(inputAspect);
								}

								{
									const RenderPass	renderPass	(attachments, subpasses, deps, inputAspects);
									const TestConfig	testConfig	(renderPass,
																	 renderTypes[renderTypeNdx].types,
																	 TestConfig::COMMANDBUFFERTYPES_INLINE,
																	 TestConfig::IMAGEMEMORY_STRICT,
																	 targetSize,
																	 renderPos,
																	 renderSize,
																	 DE_FALSE,
																	 89246,
																	 0,
																	 testConfigExternal.allocationKind,
																	 testConfigExternal.renderPassType);
									const string		testName	(renderTypes[renderTypeNdx].str + string(useInputAspect ? "_use_input_aspect" : ""));

									addFunctionCaseWithPrograms<TestConfig>(storeOpGroup.get(), testName, renderTypes[renderTypeNdx].str, createTestShaders, renderPassTest, testConfig);
								}
							}
							{
								vector<Attachment>							attachments;
								vector<Subpass>								subpasses;
								vector<SubpassDependency>					deps;
								vector<VkInputAttachmentAspectReference>	inputAspects;

								attachments.push_back(Attachment(format,
																 VK_SAMPLE_COUNT_1_BIT,
																 loadOp,
																 storeOp,
																 VK_ATTACHMENT_LOAD_OP_DONT_CARE,
																 VK_ATTACHMENT_STORE_OP_DONT_CARE,
																 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
																 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));

								subpasses.push_back(Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS,
															0u,
															vector<AttachmentReference>(),
															vector<AttachmentReference>(1, AttachmentReference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)),
															vector<AttachmentReference>(),
															AttachmentReference(VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_GENERAL),
															vector<deUint32>()));
								subpasses.push_back(Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS,
															0u,
															vector<AttachmentReference>(1, AttachmentReference(0, VK_IMAGE_LAYOUT_GENERAL, inputAttachmentAspectMask)),
															vector<AttachmentReference>(1, AttachmentReference(0, VK_IMAGE_LAYOUT_GENERAL)),
															vector<AttachmentReference>(),
															AttachmentReference(VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_GENERAL),
															vector<deUint32>()));

								deps.push_back(SubpassDependency(0, 1,
																vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
																vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,

																vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
																vk::VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
																vk::VK_DEPENDENCY_BY_REGION_BIT));

								deps.push_back(SubpassDependency(1, 1,
																vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
																vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,

																vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
																vk::VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
																vk::VK_DEPENDENCY_BY_REGION_BIT));

								if (useInputAspect)
								{
									const VkInputAttachmentAspectReference inputAspect =
									{
										1u,
										0u,
										VK_IMAGE_ASPECT_COLOR_BIT
									};

									inputAspects.push_back(inputAspect);
								}

								{
									const RenderPass renderPass (attachments, subpasses, deps, inputAspects);
									const TestConfig testConfig (renderPass,
																 renderTypes[renderTypeNdx].types,
																 TestConfig::COMMANDBUFFERTYPES_INLINE,
																 TestConfig::IMAGEMEMORY_STRICT,
																 targetSize,
																 renderPos,
																 renderSize,
																 DE_FALSE,
																 89246,
																 0,
																 testConfigExternal.allocationKind,
																 testConfigExternal.renderPassType);
									const string	testName	(string("self_dep_") + renderTypes[renderTypeNdx].str + (useInputAspect ? "_use_input_aspect" : ""));

									addFunctionCaseWithPrograms<TestConfig>(storeOpGroup.get(), testName, string("self_dep_") + renderTypes[renderTypeNdx].str, createTestShaders, renderPassTest, testConfig);
								}
							}
						}
					}

					loadOpGroup->addChild(storeOpGroup.release());
				}

				inputGroup->addChild(loadOpGroup.release());
			}

			formatGroup->addChild(inputGroup.release());
		}

		group->addChild(formatGroup.release());
	}

	// Depth stencil formats
	for (size_t formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(s_coreDepthStencilFormats); formatNdx++)
	{
		const VkFormat					vkFormat			= s_coreDepthStencilFormats[formatNdx];
		const tcu::TextureFormat		format				= mapVkFormat(vkFormat);
		const bool						isStencilAttachment	= hasStencilComponent(format.order);
		const bool						isDepthAttachment	= hasDepthComponent(format.order);
		const VkImageAspectFlags		formatAspectFlags	= (isDepthAttachment ? (VkImageAspectFlags)VK_IMAGE_ASPECT_DEPTH_BIT : 0u)
															| (isStencilAttachment ? (VkImageAspectFlags)VK_IMAGE_ASPECT_STENCIL_BIT : 0u);
		de::MovePtr<tcu::TestCaseGroup>	formatGroup			(new tcu::TestCaseGroup(testCtx, formatToName(vkFormat).c_str(), de::toString(vkFormat).c_str()));

		for (size_t loadOpNdx = 0; loadOpNdx < DE_LENGTH_OF_ARRAY(loadOps); loadOpNdx++)
		{
			const VkAttachmentLoadOp		loadOp	= loadOps[loadOpNdx].op;
			de::MovePtr<tcu::TestCaseGroup>	loadOpGroup	(new tcu::TestCaseGroup(testCtx, loadOps[loadOpNdx].str, loadOps[loadOpNdx].str));

			for (size_t renderTypeNdx = 0; renderTypeNdx < DE_LENGTH_OF_ARRAY(renderTypes); renderTypeNdx++)
			{
				{
					const RenderPass	renderPass	(vector<Attachment>(1, Attachment(vkFormat,
																					  VK_SAMPLE_COUNT_1_BIT,
																					  isDepthAttachment ? loadOp : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
																					  isDepthAttachment ? VK_ATTACHMENT_STORE_OP_STORE :VK_ATTACHMENT_STORE_OP_DONT_CARE,
																					  isStencilAttachment ? loadOp : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
																					  isStencilAttachment ? VK_ATTACHMENT_STORE_OP_STORE :VK_ATTACHMENT_STORE_OP_DONT_CARE,
																					  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
																					  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)),
													 vector<Subpass>(1, Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS,
																				0u,
																				vector<AttachmentReference>(),
																				vector<AttachmentReference>(),
																				vector<AttachmentReference>(),
																				AttachmentReference(0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL),
																				vector<deUint32>())),
													 vector<SubpassDependency>());
					const TestConfig	testConfig	(renderPass,
													 renderTypes[renderTypeNdx].types,
													 TestConfig::COMMANDBUFFERTYPES_INLINE,
													 TestConfig::IMAGEMEMORY_STRICT,
													 targetSize,
													 renderPos,
													 renderSize,
													 DE_FALSE,
													 90239,
													 0,
													 testConfigExternal.allocationKind,
													 testConfigExternal.renderPassType);

					addFunctionCaseWithPrograms<TestConfig>(loadOpGroup.get(), renderTypes[renderTypeNdx].str, renderTypes[renderTypeNdx].str, createTestShaders, renderPassTest, testConfig);
				}

				if (isStencilAttachment && isDepthAttachment && loadOp != VK_ATTACHMENT_LOAD_OP_CLEAR)
				{
					{
						const RenderPass	renderPass	(vector<Attachment>(1, Attachment(vkFormat,
																				  VK_SAMPLE_COUNT_1_BIT,
																				  isDepthAttachment ? loadOp : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
																				  isDepthAttachment ? VK_ATTACHMENT_STORE_OP_STORE :VK_ATTACHMENT_STORE_OP_DONT_CARE,
																				  isStencilAttachment ? loadOp : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
																				  isStencilAttachment ? VK_ATTACHMENT_STORE_OP_STORE :VK_ATTACHMENT_STORE_OP_DONT_CARE,
																				  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
																				  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)),
														 vector<Subpass>(1, Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS,
																					0u,
																					vector<AttachmentReference>(),
																					vector<AttachmentReference>(),
																					vector<AttachmentReference>(),
																					AttachmentReference(0, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL),
																					vector<deUint32>())),
														 vector<SubpassDependency>());
						const TestConfig	testConfig	(renderPass,
														 renderTypes[renderTypeNdx].types,
														 TestConfig::COMMANDBUFFERTYPES_INLINE,
														 TestConfig::IMAGEMEMORY_STRICT,
														 targetSize,
														 renderPos,
														 renderSize,
														 DE_FALSE,
														 90239,
														 0,
														 testConfigExternal.allocationKind,
														 testConfigExternal.renderPassType);
						const string		testName	(string(renderTypes[renderTypeNdx].str) + "_depth_read_only");

						addFunctionCaseWithPrograms<TestConfig>(loadOpGroup.get(), testName, renderTypes[renderTypeNdx].str, createTestShaders, renderPassTest, testConfig);
					}

					{
						const RenderPass	renderPass	(vector<Attachment>(1, Attachment(vkFormat,
																		  VK_SAMPLE_COUNT_1_BIT,
																		  isDepthAttachment ? loadOp : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
																		  isDepthAttachment ? VK_ATTACHMENT_STORE_OP_STORE :VK_ATTACHMENT_STORE_OP_DONT_CARE,
																		  isStencilAttachment ? loadOp : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
																		  isStencilAttachment ? VK_ATTACHMENT_STORE_OP_STORE :VK_ATTACHMENT_STORE_OP_DONT_CARE,
																		  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
																		  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)),
														 vector<Subpass>(1, Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS,
																					0u,
																					vector<AttachmentReference>(),
																					vector<AttachmentReference>(),
																					vector<AttachmentReference>(),
																					AttachmentReference(0, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL),
																					vector<deUint32>())),
														 vector<SubpassDependency>());
						const TestConfig	testConfig	(renderPass,
														 renderTypes[renderTypeNdx].types,
														 TestConfig::COMMANDBUFFERTYPES_INLINE,
														 TestConfig::IMAGEMEMORY_STRICT,
														 targetSize,
														 renderPos,
														 renderSize,
														 DE_FALSE,
														 90239,
														 0,
														 testConfigExternal.allocationKind,
														 testConfigExternal.renderPassType);
						const string		testName	(string(renderTypes[renderTypeNdx].str) + "_stencil_read_only");

						addFunctionCaseWithPrograms<TestConfig>(loadOpGroup.get(), testName, renderTypes[renderTypeNdx].str, createTestShaders, renderPassTest, testConfig);
					}
				}
			}

			formatGroup->addChild(loadOpGroup.release());
		}

		{
			de::MovePtr<tcu::TestCaseGroup>	inputGroup (new tcu::TestCaseGroup(testCtx, "input", "Test attachment format as input"));

			for (size_t loadOpNdx = 0; loadOpNdx < DE_LENGTH_OF_ARRAY(loadOps); loadOpNdx++)
			{
				const VkAttachmentLoadOp		loadOp		= loadOps[loadOpNdx].op;
				de::MovePtr<tcu::TestCaseGroup>	loadOpGroup	(new tcu::TestCaseGroup(testCtx, loadOps[loadOpNdx].str, loadOps[loadOpNdx].str));

				for (size_t storeOpNdx = 0; storeOpNdx < DE_LENGTH_OF_ARRAY(storeOps); storeOpNdx++)
				{
					const VkImageAspectFlags		inputAttachmentAspectMask	= (testConfigExternal.renderPassType == RENDERPASS_TYPE_RENDERPASS2)
																				? formatAspectFlags
																				: static_cast<VkImageAspectFlags>(0);
					const VkAttachmentStoreOp		storeOp						= storeOps[storeOpNdx].op;
					de::MovePtr<tcu::TestCaseGroup>	storeOpGroup				(new tcu::TestCaseGroup(testCtx, storeOps[storeOpNdx].str, storeOps[storeOpNdx].str));

					for (size_t useInputAspectNdx = 0; useInputAspectNdx < 2; useInputAspectNdx++)
					{
						const bool useInputAspect = useInputAspectNdx != 0;

						if (testConfigExternal.renderPassType == RENDERPASS_TYPE_RENDERPASS2 && useInputAspect)
							continue;

						for (size_t renderTypeNdx = 0; renderTypeNdx < DE_LENGTH_OF_ARRAY(renderTypes); renderTypeNdx++)
						{
							{
								vector<Attachment>							attachments;
								vector<Subpass>								subpasses;
								vector<SubpassDependency>					deps;
								vector<VkInputAttachmentAspectReference>	inputAspects;

								attachments.push_back(Attachment(vkFormat,
																 VK_SAMPLE_COUNT_1_BIT,
																 loadOp,
																 storeOp,
																 loadOp,
																 storeOp,
																 VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
																 VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));

								attachments.push_back(Attachment(vk::VK_FORMAT_R8G8B8A8_UNORM,
																 VK_SAMPLE_COUNT_1_BIT,
																 VK_ATTACHMENT_LOAD_OP_DONT_CARE,
																 VK_ATTACHMENT_STORE_OP_STORE,
																 VK_ATTACHMENT_LOAD_OP_DONT_CARE,
																 VK_ATTACHMENT_STORE_OP_DONT_CARE,
																 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
																 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));

								subpasses.push_back(Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS,
															0u,
															vector<AttachmentReference>(),
															vector<AttachmentReference>(),
															vector<AttachmentReference>(),
															AttachmentReference(0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL),
															vector<deUint32>()));
								subpasses.push_back(Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS,
															0u,
															vector<AttachmentReference>(1, AttachmentReference(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, inputAttachmentAspectMask)),
															vector<AttachmentReference>(1, AttachmentReference(1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)),
															vector<AttachmentReference>(),
															AttachmentReference(VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_GENERAL),
															vector<deUint32>()));

								deps.push_back(SubpassDependency(0, 1,
																vk::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | vk::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
																vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,

																vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
																vk::VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
																0u));

								if (useInputAspect)
								{
									const VkInputAttachmentAspectReference inputAspect =
									{
										1u,
										0u,
										(isDepthAttachment ? (VkImageAspectFlags)VK_IMAGE_ASPECT_DEPTH_BIT : 0u)
											| (isStencilAttachment ? (VkImageAspectFlags)VK_IMAGE_ASPECT_STENCIL_BIT : 0u)
									};

									inputAspects.push_back(inputAspect);
								}

								{
									const RenderPass	renderPass	(attachments, subpasses, deps, inputAspects);
									const TestConfig	testConfig	(renderPass,
																	 renderTypes[renderTypeNdx].types,
																	 TestConfig::COMMANDBUFFERTYPES_INLINE,
																	 TestConfig::IMAGEMEMORY_STRICT,
																	 targetSize,
																	 renderPos,
																	 renderSize,
																	 DE_FALSE,
																	 89246,
																	 0,
																	 testConfigExternal.allocationKind,
																	 testConfigExternal.renderPassType);
									const string		testName	(renderTypes[renderTypeNdx].str + string(useInputAspect ? "_use_input_aspect" : ""));

									addFunctionCaseWithPrograms<TestConfig>(storeOpGroup.get(), testName, renderTypes[renderTypeNdx].str, createTestShaders, renderPassTest, testConfig);
								}
							}
							{
								vector<Attachment>							attachments;
								vector<Subpass>								subpasses;
								vector<SubpassDependency>					deps;
								vector<VkInputAttachmentAspectReference>	inputAspects;

								attachments.push_back(Attachment(vkFormat,
																 VK_SAMPLE_COUNT_1_BIT,
																 loadOp,
																 storeOp,
																 VK_ATTACHMENT_LOAD_OP_DONT_CARE,
																 VK_ATTACHMENT_STORE_OP_DONT_CARE,
																 VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
																 VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));

								subpasses.push_back(Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS,
															0u,
															vector<AttachmentReference>(),
															vector<AttachmentReference>(),
															vector<AttachmentReference>(),
															AttachmentReference(0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL),
															vector<deUint32>()));
								subpasses.push_back(Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS,
															0u,
															vector<AttachmentReference>(1, AttachmentReference(0, VK_IMAGE_LAYOUT_GENERAL, inputAttachmentAspectMask)),
															vector<AttachmentReference>(),
															vector<AttachmentReference>(),
															AttachmentReference(0, VK_IMAGE_LAYOUT_GENERAL),
															vector<deUint32>()));

								deps.push_back(SubpassDependency(0, 1,
																vk::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | vk::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
																vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,

																vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
																vk::VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
																vk::VK_DEPENDENCY_BY_REGION_BIT));

								deps.push_back(SubpassDependency(1, 1,
																vk::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | vk::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
																vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
																vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
																vk::VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
																vk::VK_DEPENDENCY_BY_REGION_BIT));


								if (useInputAspect)
								{
									const VkInputAttachmentAspectReference inputAspect =
									{
										1u,
										0u,

										(isDepthAttachment ? (VkImageAspectFlags)VK_IMAGE_ASPECT_DEPTH_BIT : 0u)
											| (isStencilAttachment ? (VkImageAspectFlags)VK_IMAGE_ASPECT_STENCIL_BIT : 0u)
									};

									inputAspects.push_back(inputAspect);
								}

								{
									const RenderPass	renderPass	(attachments, subpasses, deps, inputAspects);
									const TestConfig	testConfig	(renderPass,
																	 renderTypes[renderTypeNdx].types,
																	 TestConfig::COMMANDBUFFERTYPES_INLINE,
																	 TestConfig::IMAGEMEMORY_STRICT,
																	 targetSize,
																	 renderPos,
																	 renderSize,
																	 DE_FALSE,
																	 89246,
																	 0,
																	 testConfigExternal.allocationKind,
																	 testConfigExternal.renderPassType);
									const string		testName	(string("self_dep_") + renderTypes[renderTypeNdx].str + (useInputAspect ? "_use_input_aspect" : ""));

									addFunctionCaseWithPrograms<TestConfig>(storeOpGroup.get(), testName, string("self_dep_") + renderTypes[renderTypeNdx].str, createTestShaders, renderPassTest, testConfig);
								}
							}

							if (isStencilAttachment && isDepthAttachment)
							{
								// Depth read only
								{
									vector<Attachment>							attachments;
									vector<Subpass>								subpasses;
									vector<SubpassDependency>					deps;
									vector<VkInputAttachmentAspectReference>	inputAspects;

									attachments.push_back(Attachment(vkFormat,
																	 VK_SAMPLE_COUNT_1_BIT,
																	 loadOp,
																	 storeOp,
																	 loadOp,
																	 storeOp,
																	 VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
																	 VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));

									attachments.push_back(Attachment(vk::VK_FORMAT_R8G8B8A8_UNORM,
																	 VK_SAMPLE_COUNT_1_BIT,
																	 VK_ATTACHMENT_LOAD_OP_DONT_CARE,
																	 VK_ATTACHMENT_STORE_OP_STORE,
																	 VK_ATTACHMENT_LOAD_OP_DONT_CARE,
																	 VK_ATTACHMENT_STORE_OP_DONT_CARE,
																	 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
																	 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));

									subpasses.push_back(Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS,
																0u,
																vector<AttachmentReference>(),
																vector<AttachmentReference>(),
																vector<AttachmentReference>(),
																AttachmentReference(0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL),
																vector<deUint32>()));
									subpasses.push_back(Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS,
																0u,
																vector<AttachmentReference>(1, AttachmentReference(0, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL, inputAttachmentAspectMask)),
																vector<AttachmentReference>(1, AttachmentReference(1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)),
																vector<AttachmentReference>(),
																AttachmentReference(VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_GENERAL),
																vector<deUint32>()));

									deps.push_back(SubpassDependency(0, 1,
																	vk::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | vk::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
																	vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,

																	vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
																	vk::VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
																	0u));

									if (useInputAspect)
									{
										const VkInputAttachmentAspectReference inputAspect =
										{
											1u,
											0u,

											(isDepthAttachment ? (VkImageAspectFlags)VK_IMAGE_ASPECT_DEPTH_BIT : 0u)
												| (isStencilAttachment ? (VkImageAspectFlags)VK_IMAGE_ASPECT_STENCIL_BIT : 0u)
										};

										inputAspects.push_back(inputAspect);
									}

									{
										const RenderPass	renderPass	 (attachments, subpasses, deps, inputAspects);
										const TestConfig	testConfig	 (renderPass,
																		 renderTypes[renderTypeNdx].types,
																		 TestConfig::COMMANDBUFFERTYPES_INLINE,
																		 TestConfig::IMAGEMEMORY_STRICT,
																		 targetSize,
																		 renderPos,
																		 renderSize,
																		 DE_FALSE,
																		 89246,
																		 0,
																		 testConfigExternal.allocationKind,
																		 testConfigExternal.renderPassType);
										const string		testName	(renderTypes[renderTypeNdx].str + string(useInputAspect ? "_use_input_aspect" : "") + "_depth_read_only");

										addFunctionCaseWithPrograms<TestConfig>(storeOpGroup.get(), testName, renderTypes[renderTypeNdx].str, createTestShaders, renderPassTest, testConfig);
									}
								}
								{
									vector<Attachment>							attachments;
									vector<Subpass>								subpasses;
									vector<SubpassDependency>					deps;
									vector<VkInputAttachmentAspectReference>	inputAspects;

									attachments.push_back(Attachment(vkFormat,
																	 VK_SAMPLE_COUNT_1_BIT,
																	 loadOp,
																	 storeOp,
																	 loadOp,
																	 storeOp,
																	 VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
																	 VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));

									subpasses.push_back(Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS,
																0u,
																vector<AttachmentReference>(),
																vector<AttachmentReference>(),
																vector<AttachmentReference>(),
																AttachmentReference(0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL),
																vector<deUint32>()));
									subpasses.push_back(Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS,
																0u,
																vector<AttachmentReference>(1, AttachmentReference(0, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL, inputAttachmentAspectMask)),
																vector<AttachmentReference>(),
																vector<AttachmentReference>(),
																AttachmentReference(0, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL),
																vector<deUint32>()));

									deps.push_back(SubpassDependency(0, 1,
																	vk::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | vk::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
																	vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,

																	vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
																	vk::VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
																	vk::VK_DEPENDENCY_BY_REGION_BIT));

									deps.push_back(SubpassDependency(1, 1,
																	vk::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | vk::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
																	vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,

																	vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
																	vk::VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
																	vk::VK_DEPENDENCY_BY_REGION_BIT));

									if (useInputAspect)
									{
										const VkInputAttachmentAspectReference inputAspect =
										{
											1u,
											0u,

											(isDepthAttachment ? (VkImageAspectFlags)VK_IMAGE_ASPECT_DEPTH_BIT : 0u)
												| (isStencilAttachment ? (VkImageAspectFlags)VK_IMAGE_ASPECT_STENCIL_BIT : 0u)
										};

										inputAspects.push_back(inputAspect);
									}

									{
										const RenderPass	renderPass	(attachments, subpasses, deps, inputAspects);
										const TestConfig	testConfig	(renderPass,
																		 renderTypes[renderTypeNdx].types,
																		 TestConfig::COMMANDBUFFERTYPES_INLINE,
																		 TestConfig::IMAGEMEMORY_STRICT,
																		 targetSize,
																		 renderPos,
																		 renderSize,
																		 DE_FALSE,
																		 89246,
																		 0,
																		 testConfigExternal.allocationKind,
																		 testConfigExternal.renderPassType);
										const string		testName	(string("self_dep_") + renderTypes[renderTypeNdx].str + (useInputAspect ? "_use_input_aspect" : "") + "_depth_read_only");

										addFunctionCaseWithPrograms<TestConfig>(storeOpGroup.get(), testName, string("self_dep_") + renderTypes[renderTypeNdx].str, createTestShaders, renderPassTest, testConfig);
									}
								}
								// Stencil read only
								{
									vector<Attachment>							attachments;
									vector<Subpass>								subpasses;
									vector<SubpassDependency>					deps;
									vector<VkInputAttachmentAspectReference>	inputAspects;

									attachments.push_back(Attachment(vkFormat,
																	 VK_SAMPLE_COUNT_1_BIT,
																	 loadOp,
																	 storeOp,
																	 loadOp,
																	 storeOp,
																	 VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
																	 VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));

									attachments.push_back(Attachment(vk::VK_FORMAT_R8G8B8A8_UNORM,
																	 VK_SAMPLE_COUNT_1_BIT,
																	 VK_ATTACHMENT_LOAD_OP_DONT_CARE,
																	 VK_ATTACHMENT_STORE_OP_STORE,
																	 VK_ATTACHMENT_LOAD_OP_DONT_CARE,
																	 VK_ATTACHMENT_STORE_OP_DONT_CARE,
																	 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
																	 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));

									subpasses.push_back(Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS,
																0u,
																vector<AttachmentReference>(),
																vector<AttachmentReference>(),
																vector<AttachmentReference>(),
																AttachmentReference(0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL),
																vector<deUint32>()));
									subpasses.push_back(Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS,
																0u,
																vector<AttachmentReference>(1, AttachmentReference(0, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL, inputAttachmentAspectMask)),
																vector<AttachmentReference>(1, AttachmentReference(1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)),
																vector<AttachmentReference>(),
																AttachmentReference(VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_GENERAL),
																vector<deUint32>()));

									deps.push_back(SubpassDependency(0, 1,
																	vk::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | vk::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
																	vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,

																	vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
																	vk::VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
																	0u));

									if (useInputAspect)
									{
										const VkInputAttachmentAspectReference inputAspect =
										{
											1u,
											0u,

											(isDepthAttachment ? (VkImageAspectFlags)VK_IMAGE_ASPECT_DEPTH_BIT : 0u)
												| (isStencilAttachment ? (VkImageAspectFlags)VK_IMAGE_ASPECT_STENCIL_BIT : 0u)
										};

										inputAspects.push_back(inputAspect);
									}

									{
										const RenderPass	renderPass	(attachments, subpasses, deps, inputAspects);
										const TestConfig	testConfig	(renderPass,
																		 renderTypes[renderTypeNdx].types,
																		 TestConfig::COMMANDBUFFERTYPES_INLINE,
																		 TestConfig::IMAGEMEMORY_STRICT,
																		 targetSize,
																		 renderPos,
																		 renderSize,
																		 DE_FALSE,
																		 89246,
																		 0,
																		 testConfigExternal.allocationKind,
																		 testConfigExternal.renderPassType);
										const string		testName	(renderTypes[renderTypeNdx].str + string(useInputAspect ? "_use_input_aspect" : "") + "_stencil_read_only");

										addFunctionCaseWithPrograms<TestConfig>(storeOpGroup.get(), testName, renderTypes[renderTypeNdx].str, createTestShaders, renderPassTest, testConfig);
									}
								}
								{
									vector<Attachment>							attachments;
									vector<Subpass>								subpasses;
									vector<SubpassDependency>					deps;
									vector<VkInputAttachmentAspectReference>	inputAspects;

									attachments.push_back(Attachment(vkFormat,
																	 VK_SAMPLE_COUNT_1_BIT,
																	 loadOp,
																	 storeOp,
																	 loadOp,
																	 storeOp,
																	 VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
																	 VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));

									subpasses.push_back(Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS,
																0u,
																vector<AttachmentReference>(),
																vector<AttachmentReference>(),
																vector<AttachmentReference>(),
																AttachmentReference(0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL),
																vector<deUint32>()));
									subpasses.push_back(Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS,
																0u,
																vector<AttachmentReference>(1, AttachmentReference(0, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL, inputAttachmentAspectMask)),
																vector<AttachmentReference>(),
																vector<AttachmentReference>(),
																AttachmentReference(0, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL),
																vector<deUint32>()));

									deps.push_back(SubpassDependency(0, 1,
																	vk::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | vk::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
																	vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,

																	vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
																	vk::VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
																	vk::VK_DEPENDENCY_BY_REGION_BIT));

									deps.push_back(SubpassDependency(1, 1,
																	vk::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | vk::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
																	vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,

																	vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
																	vk::VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
																	vk::VK_DEPENDENCY_BY_REGION_BIT));


									if (useInputAspect)
									{
										const VkInputAttachmentAspectReference inputAspect =
										{
											1u,
											0u,

											(isDepthAttachment ? (VkImageAspectFlags)VK_IMAGE_ASPECT_DEPTH_BIT : 0u)
												| (isStencilAttachment ? (VkImageAspectFlags)VK_IMAGE_ASPECT_STENCIL_BIT : 0u)
										};

										inputAspects.push_back(inputAspect);
									}

									{
										const RenderPass	renderPass	(attachments, subpasses, deps, inputAspects);
										const TestConfig	testConfig	(renderPass,
																		 renderTypes[renderTypeNdx].types,
																		 TestConfig::COMMANDBUFFERTYPES_INLINE,
																		 TestConfig::IMAGEMEMORY_STRICT,
																		 targetSize,
																		 renderPos,
																		 renderSize,
																		 DE_FALSE,
																		 89246,
																		 0,
																		 testConfigExternal.allocationKind,
																		 testConfigExternal.renderPassType);
										const string		testName	(string("self_dep_") + renderTypes[renderTypeNdx].str + (useInputAspect ? "_use_input_aspect" : "") + "_stencil_read_only");

										addFunctionCaseWithPrograms<TestConfig>(storeOpGroup.get(), testName, string("self_dep_") + renderTypes[renderTypeNdx].str, createTestShaders, renderPassTest, testConfig);
									}
								}
							}
						}
					}

					loadOpGroup->addChild(storeOpGroup.release());
				}

				inputGroup->addChild(loadOpGroup.release());
			}

			formatGroup->addChild(inputGroup.release());
		}

		group->addChild(formatGroup.release());
	}
}

void addRenderPassTests (tcu::TestCaseGroup* group, const AllocationKind allocationKind, const RenderPassType renderPassType)
{
	const TestConfigExternal	testConfigExternal	(allocationKind, renderPassType);

	addTestGroup(group, "simple", "Simple basic render pass tests", addSimpleTests, testConfigExternal);
	addTestGroup(group, "formats", "Tests for different image formats.", addFormatTests, testConfigExternal);
	addTestGroup(group, "attachment", "Attachment format and count tests with load and store ops and image layouts", addAttachmentTests, testConfigExternal);
	addTestGroup(group, "attachment_allocation", "Attachment allocation tests", addAttachmentAllocationTests, testConfigExternal);
	addTestGroup(group, "attachment_write_mask", "Attachment write mask tests", addAttachmentWriteMaskTests, testConfigExternal);
}

de::MovePtr<tcu::TestCaseGroup> createSuballocationTests(tcu::TestContext& testCtx, RenderPassType renderPassType)
{
	de::MovePtr<tcu::TestCaseGroup>	suballocationTestsGroup(new tcu::TestCaseGroup(testCtx, "suballocation", "Suballocation RenderPass Tests"));

	addRenderPassTests(suballocationTestsGroup.get(), ALLOCATION_KIND_SUBALLOCATED, renderPassType);

	return suballocationTestsGroup;
}

de::MovePtr<tcu::TestCaseGroup> createDedicatedAllocationTests(tcu::TestContext& testCtx, RenderPassType renderPassType)
{
	de::MovePtr<tcu::TestCaseGroup>	dedicatedAllocationTestsGroup(new tcu::TestCaseGroup(testCtx, "dedicated_allocation", "RenderPass Tests For Dedicated Allocation"));

	addRenderPassTests(dedicatedAllocationTestsGroup.get(), ALLOCATION_KIND_DEDICATED, renderPassType);

	return dedicatedAllocationTestsGroup;
}

tcu::TestCaseGroup* createRenderPassTestsInternal (tcu::TestContext& testCtx, RenderPassType renderPassType)
{
	const char*						renderpassTestsGroupName		= (renderPassType == RENDERPASS_TYPE_LEGACY) ? "renderpass" :
																	  (renderPassType == RENDERPASS_TYPE_RENDERPASS2) ? "renderpass2" :
																	  "";
	const char*						renderpassTestsGroupDescription	= (renderPassType == RENDERPASS_TYPE_LEGACY) ? "RenderPass Tests" :
																	  (renderPassType == RENDERPASS_TYPE_RENDERPASS2) ? "RenderPass2 Tests" :
																	  "";
	de::MovePtr<tcu::TestCaseGroup>	renderpassTests					(new tcu::TestCaseGroup(testCtx, renderpassTestsGroupName, renderpassTestsGroupDescription));
	de::MovePtr<tcu::TestCaseGroup>	suballocationTestGroup			= createSuballocationTests(testCtx, renderPassType);
	de::MovePtr<tcu::TestCaseGroup>	dedicatedAllocationTestGroup	= createDedicatedAllocationTests(testCtx, renderPassType);

	suballocationTestGroup->addChild((renderPassType == RENDERPASS_TYPE_LEGACY) ? createRenderPassMultisampleTests(testCtx)			: createRenderPass2MultisampleTests(testCtx));
	suballocationTestGroup->addChild((renderPassType == RENDERPASS_TYPE_LEGACY) ? createRenderPassMultisampleResolveTests(testCtx)	: createRenderPass2MultisampleResolveTests(testCtx));
	suballocationTestGroup->addChild((renderPassType == RENDERPASS_TYPE_LEGACY) ? createRenderPassSubpassDependencyTests(testCtx)	: createRenderPass2SubpassDependencyTests(testCtx));
	suballocationTestGroup->addChild((renderPassType == RENDERPASS_TYPE_LEGACY) ? createRenderPassSampleReadTests(testCtx)			: createRenderPass2SampleReadTests(testCtx));
	suballocationTestGroup->addChild((renderPassType == RENDERPASS_TYPE_LEGACY) ? createRenderPassSparseRenderTargetTests(testCtx)	: createRenderPass2SparseRenderTargetTests(testCtx));
	suballocationTestGroup->addChild(createRenderPassUnusedAttachmentTests(testCtx, renderPassType));
	suballocationTestGroup->addChild(createRenderPassUnusedClearAttachmentTests(testCtx, renderPassType));
	suballocationTestGroup->addChild(createRenderPassUnusedAttachmentSparseFillingTests(testCtx, renderPassType));

	renderpassTests->addChild(suballocationTestGroup.release());
	renderpassTests->addChild(dedicatedAllocationTestGroup.release());
	renderpassTests->addChild(createRenderPassMultipleSubpassesMultipleCommandBuffersTests(testCtx));

	if (renderPassType != RENDERPASS_TYPE_LEGACY)
	{
		renderpassTests->addChild(createRenderPass2DepthStencilResolveTests(testCtx));
		renderpassTests->addChild(createFragmentDensityMapTests(testCtx));
	}

	return renderpassTests.release();
}

} // anonymous

tcu::TestCaseGroup* createRenderPassTests (tcu::TestContext& testCtx)
{
	return createRenderPassTestsInternal(testCtx, RENDERPASS_TYPE_LEGACY);
}

tcu::TestCaseGroup* createRenderPass2Tests (tcu::TestContext& testCtx)
{
	return createRenderPassTestsInternal(testCtx, RENDERPASS_TYPE_RENDERPASS2);
}

} // vkt
