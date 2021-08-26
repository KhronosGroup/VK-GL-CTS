/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
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
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkPlatform.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "tcuTextureUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkPrograms.hpp"
#include "vkTypeUtil.hpp"
#include "vkAllocationCallbackUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vktApiCommandBuffersTests.hpp"
#include "vktApiBufferComputeInstance.hpp"
#include "vktApiComputeInstanceResultBuffer.hpp"
#include "deSharedPtr.hpp"
#include "deRandom.hpp"
#include <sstream>
#include <limits>

namespace vkt
{
namespace api
{
namespace
{

using namespace vk;

typedef de::SharedPtr<vk::Unique<vk::VkEvent> >	VkEventSp;

// Global variables
const deUint64								INFINITE_TIMEOUT		= ~(deUint64)0u;


template <deUint32 NumBuffers>
class CommandBufferBareTestEnvironment
{
public:
											CommandBufferBareTestEnvironment	(Context&						context,
																				 VkCommandPoolCreateFlags		commandPoolCreateFlags);

	VkCommandPool							getCommandPool						(void) const					{ return *m_commandPool; }
	VkCommandBuffer							getCommandBuffer					(deUint32 bufferIndex) const;

protected:
	Context&								m_context;
	const VkDevice							m_device;
	const DeviceInterface&					m_vkd;
	const VkQueue							m_queue;
	const deUint32							m_queueFamilyIndex;
	Allocator&								m_allocator;

	// \note All VkCommandBuffers are allocated from m_commandPool so there is no need
	//       to free them separately as the auto-generated dtor will do that through
	//       destroying the pool.
	Move<VkCommandPool>						m_commandPool;
	VkCommandBuffer							m_primaryCommandBuffers[NumBuffers];
};

template <deUint32 NumBuffers>
CommandBufferBareTestEnvironment<NumBuffers>::CommandBufferBareTestEnvironment(Context& context, VkCommandPoolCreateFlags commandPoolCreateFlags)
	: m_context								(context)
	, m_device								(context.getDevice())
	, m_vkd									(context.getDeviceInterface())
	, m_queue								(context.getUniversalQueue())
	, m_queueFamilyIndex					(context.getUniversalQueueFamilyIndex())
	, m_allocator							(context.getDefaultAllocator())
{
	m_commandPool = createCommandPool(m_vkd, m_device, commandPoolCreateFlags, m_queueFamilyIndex);

	const VkCommandBufferAllocateInfo		cmdBufferAllocateInfo	=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				// VkStructureType             sType;
		DE_NULL,													// const void*                 pNext;
		*m_commandPool,												// VkCommandPool               commandPool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,							// VkCommandBufferLevel        level;
		NumBuffers												// deUint32                    commandBufferCount;
	};

	VK_CHECK(m_vkd.allocateCommandBuffers(m_device, &cmdBufferAllocateInfo, m_primaryCommandBuffers));
}

template <deUint32 NumBuffers>
VkCommandBuffer CommandBufferBareTestEnvironment<NumBuffers>::getCommandBuffer(deUint32 bufferIndex) const
{
	DE_ASSERT(bufferIndex < NumBuffers);
	return m_primaryCommandBuffers[bufferIndex];
}

class CommandBufferRenderPassTestEnvironment : public CommandBufferBareTestEnvironment<1>
{
public:
											CommandBufferRenderPassTestEnvironment	(Context&						context,
																					 VkCommandPoolCreateFlags		commandPoolCreateFlags);

	VkRenderPass							getRenderPass							(void) const { return *m_renderPass; }
	VkFramebuffer							getFrameBuffer							(void) const { return *m_frameBuffer; }
	VkCommandBuffer							getPrimaryCommandBuffer					(void) const { return getCommandBuffer(0); }
	VkCommandBuffer							getSecondaryCommandBuffer				(void) const { return *m_secondaryCommandBuffer; }

	void									beginPrimaryCommandBuffer				(VkCommandBufferUsageFlags usageFlags);
	void									beginSecondaryCommandBuffer				(VkCommandBufferUsageFlags usageFlags, bool framebufferHint);
	void									beginRenderPass							(VkSubpassContents content);
	void									submitPrimaryCommandBuffer				(void);
	de::MovePtr<tcu::TextureLevel>			readColorAttachment						(void);

	static const VkImageType				DEFAULT_IMAGE_TYPE;
	static const VkFormat					DEFAULT_IMAGE_FORMAT;
	static const VkExtent3D					DEFAULT_IMAGE_SIZE;
	static const VkRect2D					DEFAULT_IMAGE_AREA;

protected:

	Move<VkImage>							m_colorImage;
	Move<VkImageView>						m_colorImageView;
	Move<VkRenderPass>						m_renderPass;
	Move<VkFramebuffer>						m_frameBuffer;
	de::MovePtr<Allocation>					m_colorImageMemory;
	Move<VkCommandBuffer>					m_secondaryCommandBuffer;

};

const VkImageType		CommandBufferRenderPassTestEnvironment::DEFAULT_IMAGE_TYPE		= VK_IMAGE_TYPE_2D;
const VkFormat			CommandBufferRenderPassTestEnvironment::DEFAULT_IMAGE_FORMAT	= VK_FORMAT_R8G8B8A8_UINT;
const VkExtent3D		CommandBufferRenderPassTestEnvironment::DEFAULT_IMAGE_SIZE		= {255, 255, 1};
const VkRect2D			CommandBufferRenderPassTestEnvironment::DEFAULT_IMAGE_AREA		=
{
	{ 0u, 0u, },												//	VkOffset2D	offset;
	{ DEFAULT_IMAGE_SIZE.width,	DEFAULT_IMAGE_SIZE.height },	//	VkExtent2D	extent;
};

CommandBufferRenderPassTestEnvironment::CommandBufferRenderPassTestEnvironment(Context& context, VkCommandPoolCreateFlags commandPoolCreateFlags)
	: CommandBufferBareTestEnvironment<1>		(context, commandPoolCreateFlags)
{
	m_renderPass = makeRenderPass(m_vkd, m_device, DEFAULT_IMAGE_FORMAT);

	{
		const VkImageCreateInfo					imageCreateInfo			=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,		// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			0u,											// VkImageCreateFlags		flags;
			DEFAULT_IMAGE_TYPE,							// VkImageType				imageType;
			DEFAULT_IMAGE_FORMAT,						// VkFormat					format;
			DEFAULT_IMAGE_SIZE,							// VkExtent3D				extent;
			1,											// deUint32					mipLevels;
			1,											// deUint32					arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,					// VkImageTiling			tiling;
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
			VK_IMAGE_USAGE_TRANSFER_DST_BIT,			// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode			sharingMode;
			1,											// deUint32					queueFamilyIndexCount;
			&m_queueFamilyIndex,						// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED					// VkImageLayout			initialLayout;
		};

		m_colorImage = createImage(m_vkd, m_device, &imageCreateInfo, DE_NULL);
	}

	m_colorImageMemory = m_allocator.allocate(getImageMemoryRequirements(m_vkd, m_device, *m_colorImage), MemoryRequirement::Any);
	VK_CHECK(m_vkd.bindImageMemory(m_device, *m_colorImage, m_colorImageMemory->getMemory(), m_colorImageMemory->getOffset()));

	{
		const VkImageViewCreateInfo				imageViewCreateInfo		=
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// VkStructureType				sType;
			DE_NULL,									// const void*					pNext;
			0u,											// VkImageViewCreateFlags		flags;
			*m_colorImage,								// VkImage						image;
			VK_IMAGE_VIEW_TYPE_2D,						// VkImageViewType				viewType;
			DEFAULT_IMAGE_FORMAT,						// VkFormat						format;
			{
				VK_COMPONENT_SWIZZLE_R,
				VK_COMPONENT_SWIZZLE_G,
				VK_COMPONENT_SWIZZLE_B,
				VK_COMPONENT_SWIZZLE_A
			},											// VkComponentMapping			components;
			{
				VK_IMAGE_ASPECT_COLOR_BIT,					// VkImageAspectFlags			aspectMask;
				0u,											// deUint32						baseMipLevel;
				1u,											// deUint32						mipLevels;
				0u,											// deUint32						baseArrayLayer;
				1u,											// deUint32						arraySize;
			},											// VkImageSubresourceRange		subresourceRange;
		};

		m_colorImageView = createImageView(m_vkd, m_device, &imageViewCreateInfo, DE_NULL);
	}

	{
		const VkImageView						attachmentViews[1]		=
		{
			*m_colorImageView
		};

		const VkFramebufferCreateInfo			framebufferCreateInfo	=
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			0u,											// VkFramebufferCreateFlags	flags;
			*m_renderPass,								// VkRenderPass				renderPass;
			1,											// deUint32					attachmentCount;
			attachmentViews,							// const VkImageView*		pAttachments;
			DEFAULT_IMAGE_SIZE.width,					// deUint32					width;
			DEFAULT_IMAGE_SIZE.height,					// deUint32					height;
			1u,											// deUint32					layers;
		};

		m_frameBuffer = createFramebuffer(m_vkd, m_device, &framebufferCreateInfo, DE_NULL);
	}

	{
		const VkCommandBufferAllocateInfo		cmdBufferAllocateInfo	=
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				// VkStructureType             sType;
			DE_NULL,													// const void*                 pNext;
			*m_commandPool,												// VkCommandPool               commandPool;
			VK_COMMAND_BUFFER_LEVEL_SECONDARY,							// VkCommandBufferLevel        level;
			1u															// deUint32                    commandBufferCount;
		};

		m_secondaryCommandBuffer = allocateCommandBuffer(m_vkd, m_device, &cmdBufferAllocateInfo);

	}
}

void CommandBufferRenderPassTestEnvironment::beginRenderPass(VkSubpassContents content)
{
	vk::beginRenderPass(m_vkd, m_primaryCommandBuffers[0], *m_renderPass, *m_frameBuffer, DEFAULT_IMAGE_AREA, tcu::UVec4(17, 59, 163, 251), content);
}

void CommandBufferRenderPassTestEnvironment::beginPrimaryCommandBuffer(VkCommandBufferUsageFlags usageFlags)
{
	beginCommandBuffer(m_vkd, m_primaryCommandBuffers[0], usageFlags);
}

void CommandBufferRenderPassTestEnvironment::beginSecondaryCommandBuffer(VkCommandBufferUsageFlags usageFlags, bool framebufferHint)
{
	const VkCommandBufferInheritanceInfo	commandBufferInheritanceInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,		// VkStructureType                  sType;
		DE_NULL,												// const void*                      pNext;
		*m_renderPass,											// VkRenderPass                     renderPass;
		0u,														// deUint32                         subpass;
		(framebufferHint ? *m_frameBuffer : DE_NULL),			// VkFramebuffer                    framebuffer;
		VK_FALSE,												// VkBool32                         occlusionQueryEnable;
		0u,														// VkQueryControlFlags              queryFlags;
		0u														// VkQueryPipelineStatisticFlags    pipelineStatistics;
	};

	const VkCommandBufferBeginInfo			commandBufferBeginInfo	=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,			// VkStructureType                          sType;
		DE_NULL,												// const void*                              pNext;
		usageFlags,												// VkCommandBufferUsageFlags                flags;
		&commandBufferInheritanceInfo							// const VkCommandBufferInheritanceInfo*    pInheritanceInfo;
	};

	VK_CHECK(m_vkd.beginCommandBuffer(*m_secondaryCommandBuffer, &commandBufferBeginInfo));

}

void CommandBufferRenderPassTestEnvironment::submitPrimaryCommandBuffer(void)
{
	submitCommandsAndWait(m_vkd, m_device, m_queue, *m_primaryCommandBuffers);
}

de::MovePtr<tcu::TextureLevel> CommandBufferRenderPassTestEnvironment::readColorAttachment ()
{
	Move<VkBuffer>					buffer;
	de::MovePtr<Allocation>			bufferAlloc;
	const tcu::TextureFormat		tcuFormat		= mapVkFormat(DEFAULT_IMAGE_FORMAT);
	const VkDeviceSize				pixelDataSize	= DEFAULT_IMAGE_SIZE.height * DEFAULT_IMAGE_SIZE.height * tcuFormat.getPixelSize();
	de::MovePtr<tcu::TextureLevel>	resultLevel		(new tcu::TextureLevel(tcuFormat, DEFAULT_IMAGE_SIZE.width, DEFAULT_IMAGE_SIZE.height));

	// Create destination buffer
	{
		const VkBufferCreateInfo bufferParams =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			0u,											// VkBufferCreateFlags	flags;
			pixelDataSize,								// VkDeviceSize			size;
			VK_BUFFER_USAGE_TRANSFER_DST_BIT,			// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
			0u,											// deUint32				queueFamilyIndexCount;
			DE_NULL										// const deUint32*		pQueueFamilyIndices;
		};

		buffer		= createBuffer(m_vkd, m_device, &bufferParams);
		bufferAlloc = m_allocator.allocate(getBufferMemoryRequirements(m_vkd, m_device, *buffer), MemoryRequirement::HostVisible);
		VK_CHECK(m_vkd.bindBufferMemory(m_device, *buffer, bufferAlloc->getMemory(), bufferAlloc->getOffset()));
	}

	// Copy image to buffer
	beginPrimaryCommandBuffer(0);
	copyImageToBuffer(m_vkd, m_primaryCommandBuffers[0], *m_colorImage, *buffer, tcu::IVec2(DEFAULT_IMAGE_SIZE.width, DEFAULT_IMAGE_SIZE.height));
	endCommandBuffer(m_vkd, m_primaryCommandBuffers[0]);

	submitPrimaryCommandBuffer();

	// Read buffer data
	invalidateAlloc(m_vkd, m_device, *bufferAlloc);
	tcu::copy(*resultLevel, tcu::ConstPixelBufferAccess(resultLevel->getFormat(), resultLevel->getSize(), bufferAlloc->getHostPtr()));

	return resultLevel;
}


// Testcases
/********* 19.1. Command Pools (5.1 in VK 1.0 Spec) ***************************/
tcu::TestStatus createPoolNullParamsTest(Context& context)
{
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	createCommandPool(vk, vkDevice, 0u, queueFamilyIndex);

	return tcu::TestStatus::pass("Command Pool allocated correctly.");
}

tcu::TestStatus createPoolNonNullAllocatorTest(Context& context)
{
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();
	const VkAllocationCallbacks*			allocationCallbacks		= getSystemAllocator();

	const VkCommandPoolCreateInfo			cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					// sType;
		DE_NULL,													// pNext;
		0u,															// flags;
		queueFamilyIndex,											// queueFamilyIndex;
	};

	createCommandPool(vk, vkDevice, &cmdPoolParams, allocationCallbacks);

	return tcu::TestStatus::pass("Command Pool allocated correctly.");
}

tcu::TestStatus createPoolTransientBitTest(Context& context)
{
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	const VkCommandPoolCreateInfo			cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					// sType;
		DE_NULL,													// pNext;
		VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,						// flags;
		queueFamilyIndex,											// queueFamilyIndex;
	};

	createCommandPool(vk, vkDevice, &cmdPoolParams, DE_NULL);

	return tcu::TestStatus::pass("Command Pool allocated correctly.");
}

tcu::TestStatus createPoolResetBitTest(Context& context)
{
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	const VkCommandPoolCreateInfo			cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					// sType;
		DE_NULL,													// pNext;
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,			// flags;
		queueFamilyIndex,											// queueFamilyIndex;
	};

	createCommandPool(vk, vkDevice, &cmdPoolParams, DE_NULL);

	return tcu::TestStatus::pass("Command Pool allocated correctly.");
}

tcu::TestStatus resetPoolReleaseResourcesBitTest(Context& context)
{
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	const VkCommandPoolCreateInfo			cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					// sType;
		DE_NULL,													// pNext;
		0u,															// flags;
		queueFamilyIndex,											// queueFamilyIndex;
	};

	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams, DE_NULL));

	VK_CHECK(vk.resetCommandPool(vkDevice, *cmdPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT));

	return tcu::TestStatus::pass("Command Pool allocated correctly.");
}

tcu::TestStatus resetPoolNoFlagsTest(Context& context)
{
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	const VkCommandPoolCreateInfo			cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					// sType;
		DE_NULL,													// pNext;
		0u,															// flags;
		queueFamilyIndex,											// queueFamilyIndex;
	};

	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams, DE_NULL));

	VK_CHECK(vk.resetCommandPool(vkDevice, *cmdPool, 0u));

	return tcu::TestStatus::pass("Command Pool allocated correctly.");
}

bool executeCommandBuffer (const VkDevice			device,
						   const DeviceInterface&	vk,
						   const VkQueue			queue,
						   const VkCommandBuffer	commandBuffer,
						   const bool				exitBeforeEndCommandBuffer = false)
{
	const Unique<VkEvent>			event					(createEvent(vk, device));
	beginCommandBuffer(vk, commandBuffer, 0u);
	{
		const VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		vk.cmdSetEvent(commandBuffer, *event, stageMask);
		if (exitBeforeEndCommandBuffer)
			return exitBeforeEndCommandBuffer;
	}
	endCommandBuffer(vk, commandBuffer);

	submitCommandsAndWait(vk, device, queue, commandBuffer);

	// check if buffer has been executed
	const VkResult result = vk.getEventStatus(device, *event);
	return result == VK_EVENT_SET;
}

tcu::TestStatus resetPoolReuseTest (Context& context)
{
	const VkDevice						vkDevice			= context.getDevice();
	const DeviceInterface&				vk					= context.getDeviceInterface();
	const deUint32						queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	const VkQueue						queue				= context.getUniversalQueue();

	const VkCommandPoolCreateInfo		cmdPoolParams		=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,	// sType;
		DE_NULL,									// pNext;
		0u,											// flags;
		queueFamilyIndex							// queueFamilyIndex;
	};
	const Unique<VkCommandPool>			cmdPool				(createCommandPool(vk, vkDevice, &cmdPoolParams, DE_NULL));
	const VkCommandBufferAllocateInfo	cmdBufParams		=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,	// sType;
		DE_NULL,										// pNext;
		*cmdPool,										// commandPool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,				// level;
		1u												// bufferCount;
	};
	const Move<VkCommandBuffer>			commandBuffers[]	=
	{
		allocateCommandBuffer(vk, vkDevice, &cmdBufParams),
		allocateCommandBuffer(vk, vkDevice, &cmdBufParams)
	};

	if (!executeCommandBuffer(vkDevice, vk, queue, *(commandBuffers[0])))
		return tcu::TestStatus::fail("Failed");
	if (!executeCommandBuffer(vkDevice, vk, queue, *(commandBuffers[1]), true))
		return tcu::TestStatus::fail("Failed");

	VK_CHECK(vk.resetCommandPool(vkDevice, *cmdPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT));

	if (!executeCommandBuffer(vkDevice, vk, queue, *(commandBuffers[0])))
		return tcu::TestStatus::fail("Failed");
	if (!executeCommandBuffer(vkDevice, vk, queue, *(commandBuffers[1])))
		return tcu::TestStatus::fail("Failed");

	{
		const Unique<VkCommandBuffer> afterResetCommandBuffers(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));
		if (!executeCommandBuffer(vkDevice, vk, queue, *afterResetCommandBuffers))
			return tcu::TestStatus::fail("Failed");
	}

	return tcu::TestStatus::pass("Passed");
}

/******** 19.2. Command Buffer Lifetime (5.2 in VK 1.0 Spec) ******************/
tcu::TestStatus allocatePrimaryBufferTest(Context& context)
{
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	const VkCommandPoolCreateInfo			cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					// sType;
		DE_NULL,													// pNext;
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,			// flags;
		queueFamilyIndex,											// queueFamilyIndex;
	};
	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCommandBufferAllocateInfo		cmdBufParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				// sType;
		DE_NULL,													// pNext;
		*cmdPool,													// commandPool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,							// level;
		1u,															// bufferCount;
	};
	const Unique<VkCommandBuffer>			cmdBuf					(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

	return tcu::TestStatus::pass("Buffer was created correctly.");
}

tcu::TestStatus allocateManyPrimaryBuffersTest(Context& context)
{

	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	const VkCommandPoolCreateInfo			cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					//	VkStructureType				sType;
		DE_NULL,													//	const void*					pNext;
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,			//	VkCommandPoolCreateFlags	flags;
		queueFamilyIndex,											//	deUint32					queueFamilyIndex;
	};
	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// \todo Determining the minimum number of command buffers should be a function of available system memory and driver capabilities.
#if (DE_PTR_SIZE == 4)
	const unsigned minCommandBuffer = 1024;
#else
	const unsigned minCommandBuffer = 10000;
#endif

	// Command buffer
	const VkCommandBufferAllocateInfo		cmdBufParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				//	VkStructureType				sType;
		DE_NULL,													//	const void*					pNext;
		*cmdPool,													//	VkCommandPool				pool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,							//	VkCommandBufferLevel		level;
		minCommandBuffer,											//	uint32_t					bufferCount;
	};

	// do not keep the handles to buffers, as they will be freed with command pool

	// allocate the minimum required amount of buffers
	VkCommandBuffer cmdBuffers[minCommandBuffer];
	VK_CHECK(vk.allocateCommandBuffers(vkDevice, &cmdBufParams, cmdBuffers));

	std::ostringstream out;
	out << "allocateManyPrimaryBuffersTest succeded: created " << minCommandBuffer << " command buffers";

	return tcu::TestStatus::pass(out.str());
}

tcu::TestStatus allocateSecondaryBufferTest(Context& context)
{
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	const VkCommandPoolCreateInfo			cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					// sType;
		DE_NULL,													// pNext;
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,			// flags;
		queueFamilyIndex,											// queueFamilyIndex;
	};
	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCommandBufferAllocateInfo		cmdBufParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				// sType;
		DE_NULL,													// pNext;
		*cmdPool,													// commandPool;
		VK_COMMAND_BUFFER_LEVEL_SECONDARY,							// level;
		1u,															// bufferCount;
	};
	const Unique<VkCommandBuffer>			cmdBuf					(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

	return tcu::TestStatus::pass("Buffer was created correctly.");
}

tcu::TestStatus allocateManySecondaryBuffersTest(Context& context)
{

	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	const VkCommandPoolCreateInfo			cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					//	VkStructureType				sType;
		DE_NULL,													//	const void*					pNext;
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,			//	VkCommandPoolCreateFlags	flags;
		queueFamilyIndex,											//	deUint32					queueFamilyIndex;
	};
	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// \todo Determining the minimum number of command buffers should be a function of available system memory and driver capabilities.
#if (DE_PTR_SIZE == 4)
	const unsigned minCommandBuffer = 1024;
#else
	const unsigned minCommandBuffer = 10000;
#endif

	// Command buffer
	const VkCommandBufferAllocateInfo		cmdBufParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				//	VkStructureType				sType;
		DE_NULL,													//	const void*					pNext;
		*cmdPool,													//	VkCommandPool				pool;
		VK_COMMAND_BUFFER_LEVEL_SECONDARY,							//	VkCommandBufferLevel		level;
		minCommandBuffer,											//	uint32_t					bufferCount;
	};

	// do not keep the handles to buffers, as they will be freed with command pool

	// allocate the minimum required amount of buffers
	VkCommandBuffer cmdBuffers[minCommandBuffer];
	VK_CHECK(vk.allocateCommandBuffers(vkDevice, &cmdBufParams, cmdBuffers));

	std::ostringstream out;
	out << "allocateManySecondaryBuffersTest succeded: created " << minCommandBuffer << " command buffers";

	return tcu::TestStatus::pass(out.str());
}

tcu::TestStatus executePrimaryBufferTest(Context& context)
{
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkQueue							queue					= context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	const VkCommandPoolCreateInfo			cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					//	VkStructureType				sType;
		DE_NULL,													//	const void*					pNext;
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,			//	VkCommandPoolCreateFlags	flags;
		queueFamilyIndex,											//	deUint32					queueFamilyIndex;
	};
	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCommandBufferAllocateInfo		cmdBufParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				//	VkStructureType				sType;
		DE_NULL,													//	const void*					pNext;
		*cmdPool,													//	VkCommandPool				pool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,							//	VkCommandBufferLevel		level;
		1u,															//	uint32_t					bufferCount;
	};
	const Unique<VkCommandBuffer>			primCmdBuf				(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

	// create event that will be used to check if secondary command buffer has been executed
	const Unique<VkEvent>					event					(createEvent(vk, vkDevice));

	// reset event
	VK_CHECK(vk.resetEvent(vkDevice, *event));

	// record primary command buffer
	beginCommandBuffer(vk, *primCmdBuf, 0u);
	{
		// allow execution of event during every stage of pipeline
		VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

		// record setting event
		vk.cmdSetEvent(*primCmdBuf, *event,stageMask);
	}
	endCommandBuffer(vk, *primCmdBuf);

	submitCommandsAndWait(vk, vkDevice, queue, primCmdBuf.get());

	// check if buffer has been executed
	VkResult result = vk.getEventStatus(vkDevice,*event);
	if (result == VK_EVENT_SET)
		return tcu::TestStatus::pass("Execute Primary Command Buffer succeeded");

	return tcu::TestStatus::fail("Execute Primary Command Buffer FAILED");
}

tcu::TestStatus executeLargePrimaryBufferTest(Context& context)
{
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkQueue							queue					= context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();
	const deUint32							LARGE_BUFFER_SIZE		= 10000;

	const VkCommandPoolCreateInfo			cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					//	VkStructureType				sType;
		DE_NULL,													//	const void*					pNext;
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,			//	VkCommandPoolCreateFlags	flags;
		queueFamilyIndex,											//	deUint32					queueFamilyIndex;
	};
	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCommandBufferAllocateInfo		cmdBufParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				//	VkStructureType				sType;
		DE_NULL,													//	const void*					pNext;
		*cmdPool,													//	VkCommandPool				pool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,							//	VkCommandBufferLevel		level;
		1u,															//	uint32_t					bufferCount;
	};
	const Unique<VkCommandBuffer>			primCmdBuf				(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

	std::vector<VkEventSp>					events;
	for (deUint32 ndx = 0; ndx < LARGE_BUFFER_SIZE; ++ndx)
		events.push_back(VkEventSp(new vk::Unique<VkEvent>(createEvent(vk, vkDevice))));

	// record primary command buffer
	beginCommandBuffer(vk, *primCmdBuf, 0u);
	{
		// set all the events
		for (deUint32 ndx = 0; ndx < LARGE_BUFFER_SIZE; ++ndx)
		{
			vk.cmdSetEvent(*primCmdBuf, events[ndx]->get(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
		}
	}
	endCommandBuffer(vk, *primCmdBuf);

	submitCommandsAndWait(vk, vkDevice, queue, primCmdBuf.get());

	// check if the buffer was executed correctly - all events had their status
	// changed
	tcu::TestStatus testResult = tcu::TestStatus::incomplete();

	for (deUint32 ndx = 0; ndx < LARGE_BUFFER_SIZE; ++ndx)
	{
		if (vk.getEventStatus(vkDevice, events[ndx]->get()) != VK_EVENT_SET)
		{
			testResult = tcu::TestStatus::fail("An event was not set.");
			break;
		}
	}

	if (!testResult.isComplete())
		testResult = tcu::TestStatus::pass("All events set correctly.");

	return testResult;
}

tcu::TestStatus resetBufferImplicitlyTest(Context& context)
{
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkQueue							queue					= context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	const VkCommandPoolCreateInfo			cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					// sType;
		DE_NULL,													// pNext;
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,			// flags;
		queueFamilyIndex,											// queueFamilyIndex;
	};
	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCommandBufferAllocateInfo		cmdBufParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				// sType;
		DE_NULL,													// pNext;
		*cmdPool,													// pool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,							// level;
		1u,															// bufferCount;
	};
	const Unique<VkCommandBuffer>			cmdBuf						(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

	const Unique<VkEvent>					event					(createEvent(vk, vkDevice));

	// Put the command buffer in recording state.
	beginCommandBuffer(vk, *cmdBuf, 0u);
	{
		// Set the event
		vk.cmdSetEvent(*cmdBuf, *event, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
	}
	endCommandBuffer(vk, *cmdBuf);

	submitCommandsAndWait(vk, vkDevice, queue, cmdBuf.get());

	// Check if the buffer was executed
	if (vk.getEventStatus(vkDevice, *event) != VK_EVENT_SET)
		return tcu::TestStatus::fail("Failed to set the event.");

	// Reset the event
	vk.resetEvent(vkDevice, *event);
	if(vk.getEventStatus(vkDevice, *event) != VK_EVENT_RESET)
		return tcu::TestStatus::fail("Failed to reset the event.");

	// Reset the command buffer by putting it in recording state again. This
	// should empty the command buffer.
	beginCommandBuffer(vk, *cmdBuf, 0u);
	endCommandBuffer(vk, *cmdBuf);

	// Submit the command buffer after resetting. It should have no commands
	// recorded, so the event should remain unsignaled.
	submitCommandsAndWait(vk, vkDevice, queue, cmdBuf.get());

	// Check if the event remained unset.
	if(vk.getEventStatus(vkDevice, *event) == VK_EVENT_RESET)
		return tcu::TestStatus::pass("Buffer was reset correctly.");
	else
		return tcu::TestStatus::fail("Buffer was not reset correctly.");
}

using  de::SharedPtr;
typedef SharedPtr<Unique<VkEvent> >			VkEventShared;

template<typename T>
inline SharedPtr<Unique<T> > makeSharedPtr (Move<T> move)
{
	return SharedPtr<Unique<T> >(new Unique<T>(move));
}

bool submitAndCheck (Context& context, std::vector<VkCommandBuffer>& cmdBuffers, std::vector <VkEventShared>& events)
{
	const VkDevice						vkDevice	= context.getDevice();
	const DeviceInterface&				vk			= context.getDeviceInterface();
	const VkQueue						queue		= context.getUniversalQueue();
	const Unique<VkFence>				fence		(createFence(vk, vkDevice));

	const VkSubmitInfo					submitInfo	=
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,				// sType
		DE_NULL,									// pNext
		0u,											// waitSemaphoreCount
		DE_NULL,									// pWaitSemaphores
		(const VkPipelineStageFlags*)DE_NULL,		// pWaitDstStageMask
		static_cast<deUint32>(cmdBuffers.size()),	// commandBufferCount
		&cmdBuffers[0],								// pCommandBuffers
		0u,											// signalSemaphoreCount
		DE_NULL,									// pSignalSemaphores
	};

	VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo, fence.get()));
	VK_CHECK(vk.waitForFences(vkDevice, 1u, &fence.get(), 0u, INFINITE_TIMEOUT));

	for(int eventNdx = 0; eventNdx < static_cast<int>(events.size()); ++eventNdx)
	{
		if (vk.getEventStatus(vkDevice, **events[eventNdx]) != VK_EVENT_SET)
			return false;
		vk.resetEvent(vkDevice, **events[eventNdx]);
	}

	return true;
}

void createCommadBuffers (const DeviceInterface&		vk,
						  const VkDevice				vkDevice,
						  deUint32						bufferCount,
						  VkCommandPool					pool,
						  const VkCommandBufferLevel	cmdBufferLevel,
						  VkCommandBuffer*				pCommandBuffers)
{
	const VkCommandBufferAllocateInfo		cmdBufParams	=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,	//	VkStructureType				sType;
		DE_NULL,										//	const void*					pNext;
		pool,											//	VkCommandPool				pool;
		cmdBufferLevel,									//	VkCommandBufferLevel		level;
		bufferCount,									//	uint32_t					bufferCount;
	};
	VK_CHECK(vk.allocateCommandBuffers(vkDevice, &cmdBufParams, pCommandBuffers));
}

void addCommandsToBuffer (const DeviceInterface& vk, std::vector<VkCommandBuffer>& cmdBuffers, std::vector <VkEventShared>& events)
{
	const VkCommandBufferInheritanceInfo	secCmdBufInheritInfo	=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
		DE_NULL,
		(VkRenderPass)0u,								// renderPass
		0u,												// subpass
		(VkFramebuffer)0u,								// framebuffer
		VK_FALSE,										// occlusionQueryEnable
		(VkQueryControlFlags)0u,						// queryFlags
		(VkQueryPipelineStatisticFlags)0u,				// pipelineStatistics
	};

	const VkCommandBufferBeginInfo		cmdBufBeginInfo	=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	// sType
		DE_NULL,										// pNext
		0u,												// flags
		&secCmdBufInheritInfo,							// pInheritanceInfo;
	};

	for(int bufferNdx = 0; bufferNdx < static_cast<int>(cmdBuffers.size()); ++bufferNdx)
	{
		VK_CHECK(vk.beginCommandBuffer(cmdBuffers[bufferNdx], &cmdBufBeginInfo));
		vk.cmdSetEvent(cmdBuffers[bufferNdx], **events[bufferNdx % events.size()], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
		endCommandBuffer(vk, cmdBuffers[bufferNdx]);
	}
}

bool executeSecondaryCmdBuffer (Context&						context,
								VkCommandPool					pool,
								std::vector<VkCommandBuffer>&	cmdBuffersSecondary,
								std::vector <VkEventShared>&	events)
{
	const VkDevice					vkDevice		= context.getDevice();
	const DeviceInterface&			vk				= context.getDeviceInterface();
	std::vector<VkCommandBuffer>	cmdBuffer		(1);

	createCommadBuffers(vk, vkDevice, 1u, pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, &cmdBuffer[0]);
	beginCommandBuffer(vk, cmdBuffer[0], 0u);
	vk.cmdExecuteCommands(cmdBuffer[0], static_cast<deUint32>(cmdBuffersSecondary.size()), &cmdBuffersSecondary[0]);
	endCommandBuffer(vk, cmdBuffer[0]);

	bool returnValue = submitAndCheck(context, cmdBuffer, events);
	vk.freeCommandBuffers(vkDevice, pool, 1u, &cmdBuffer[0]);
	return returnValue;
}

tcu::TestStatus trimCommandPoolTest (Context& context, const VkCommandBufferLevel cmdBufferLevel)
{
	if (!context.isDeviceFunctionalitySupported("VK_KHR_maintenance1"))
		TCU_THROW(NotSupportedError, "Extension VK_KHR_maintenance1 not supported");

	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	//test parameters
	const deUint32							cmdBufferIterationCount	= 300u;
	const deUint32							cmdBufferCount			= 10u;

	const VkCommandPoolCreateInfo			cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					// sType;
		DE_NULL,													// pNext;
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,			// flags;
		queueFamilyIndex,											// queueFamilyIndex;
	};
	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	std::vector <VkEventShared>				events;
	for (deUint32 ndx = 0u; ndx < cmdBufferCount; ++ndx)
		events.push_back(makeSharedPtr(createEvent(vk, vkDevice)));

	{
		std::vector<VkCommandBuffer> cmdBuffers(cmdBufferCount);
		createCommadBuffers(vk, vkDevice, cmdBufferCount, *cmdPool, cmdBufferLevel, &cmdBuffers[0]);

		for (deUint32 cmdBufferIterationrNdx = 0; cmdBufferIterationrNdx < cmdBufferIterationCount; ++cmdBufferIterationrNdx)
		{
			addCommandsToBuffer(vk, cmdBuffers, events);

			//Peak, situation when we use a lot more command buffers
			if (cmdBufferIterationrNdx % 10u == 0)
			{
				std::vector<VkCommandBuffer> cmdBuffersPeak(cmdBufferCount * 10u);
				createCommadBuffers(vk, vkDevice, static_cast<deUint32>(cmdBuffersPeak.size()), *cmdPool, cmdBufferLevel, &cmdBuffersPeak[0]);
				addCommandsToBuffer(vk, cmdBuffersPeak, events);

				switch(cmdBufferLevel)
				{
					case VK_COMMAND_BUFFER_LEVEL_PRIMARY:
						if (!submitAndCheck(context, cmdBuffersPeak, events))
							return tcu::TestStatus::fail("Fail");
						break;
					case VK_COMMAND_BUFFER_LEVEL_SECONDARY:
						if (!executeSecondaryCmdBuffer(context, *cmdPool, cmdBuffersPeak, events))
							return tcu::TestStatus::fail("Fail");
						break;
					default:
						DE_ASSERT(0);
				}
				vk.freeCommandBuffers(vkDevice, *cmdPool, static_cast<deUint32>(cmdBuffersPeak.size()), &cmdBuffersPeak[0]);
			}

			vk.trimCommandPool(vkDevice, *cmdPool, (VkCommandPoolTrimFlags)0);

			switch(cmdBufferLevel)
			{
				case VK_COMMAND_BUFFER_LEVEL_PRIMARY:
					if (!submitAndCheck(context, cmdBuffers, events))
						return tcu::TestStatus::fail("Fail");
					break;
				case VK_COMMAND_BUFFER_LEVEL_SECONDARY:
					if (!executeSecondaryCmdBuffer(context, *cmdPool, cmdBuffers, events))
						return tcu::TestStatus::fail("Fail");
					break;
				default:
					DE_ASSERT(0);
			}

			for (deUint32 bufferNdx = cmdBufferIterationrNdx % 3u; bufferNdx < cmdBufferCount; bufferNdx+=2u)
			{
				vk.freeCommandBuffers(vkDevice, *cmdPool, 1u, &cmdBuffers[bufferNdx]);
				createCommadBuffers(vk, vkDevice, 1u, *cmdPool, cmdBufferLevel, &cmdBuffers[bufferNdx]);
			}
		}
	}

	return tcu::TestStatus::pass("Pass");
}

/******** 19.3. Command Buffer Recording (5.3 in VK 1.0 Spec) *****************/
tcu::TestStatus recordSinglePrimaryBufferTest(Context& context)
{
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	const VkCommandPoolCreateInfo			cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					//	VkStructureType				sType;
		DE_NULL,													//	const void*					pNext;
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,			//	VkCommandPoolCreateFlags	flags;
		queueFamilyIndex,											//	deUint32					queueFamilyIndex;
	};
	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCommandBufferAllocateInfo		cmdBufParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				//	VkStructureType				sType;
		DE_NULL,													//	const void*					pNext;
		*cmdPool,													//	VkCommandPool				pool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,							//	VkCommandBufferLevel		level;
		1u,															//	uint32_t					bufferCount;
	};
	const Unique<VkCommandBuffer>			primCmdBuf				(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

	// create event that will be used to check if secondary command buffer has been executed
	const Unique<VkEvent>					event					(createEvent(vk, vkDevice));

	// record primary command buffer
	beginCommandBuffer(vk, *primCmdBuf, 0u);
	{
		// record setting event
		vk.cmdSetEvent(*primCmdBuf, *event, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
	}
	endCommandBuffer(vk, *primCmdBuf);

	return tcu::TestStatus::pass("Primary buffer recorded successfully.");
}

tcu::TestStatus recordLargePrimaryBufferTest(Context &context)
{
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkQueue							queue					= context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	const VkCommandPoolCreateInfo			cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					//	VkStructureType				sType;
		DE_NULL,													//	const void*					pNext;
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,			//	VkCommandPoolCreateFlags	flags;
		queueFamilyIndex,											//	deUint32					queueFamilyIndex;
	};
	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCommandBufferAllocateInfo		cmdBufParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				//	VkStructureType				sType;
		DE_NULL,													//	const void*					pNext;
		*cmdPool,													//	VkCommandPool				pool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,							//	VkCommandBufferLevel		level;
		1u,															//	uint32_t					bufferCount;
	};
	const Unique<VkCommandBuffer>			primCmdBuf				(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

	// create event that will be used to check if secondary command buffer has been executed
	const Unique<VkEvent>					event					(createEvent(vk, vkDevice));

	// reset event
	VK_CHECK(vk.resetEvent(vkDevice, *event));

	// record primary command buffer
	beginCommandBuffer(vk, *primCmdBuf, 0u);
	{
		// allow execution of event during every stage of pipeline
		VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

		// define minimal amount of commands to accept
		const long long unsigned minNumCommands = 10000llu;

		for ( long long unsigned currentCommands = 0; currentCommands < minNumCommands / 2; ++currentCommands )
		{
			// record setting event
			vk.cmdSetEvent(*primCmdBuf, *event,stageMask);

			// record resetting event
			vk.cmdResetEvent(*primCmdBuf, *event,stageMask);
		}

	}
	endCommandBuffer(vk, *primCmdBuf);

	submitCommandsAndWait(vk, vkDevice, queue, primCmdBuf.get());

	return tcu::TestStatus::pass("hugeTest succeeded");
}

tcu::TestStatus recordSingleSecondaryBufferTest(Context& context)
{
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	const VkCommandPoolCreateInfo			cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					//	VkStructureType				sType;
		DE_NULL,													//	const void*					pNext;
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,			//	VkCommandPoolCreateFlags	flags;
		queueFamilyIndex,											//	deUint32					queueFamilyIndex;
	};
	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCommandBufferAllocateInfo		cmdBufParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				//	VkStructureType				sType;
		DE_NULL,													//	const void*					pNext;
		*cmdPool,													//	VkCommandPool				pool;
		VK_COMMAND_BUFFER_LEVEL_SECONDARY,							//	VkCommandBufferLevel		level;
		1u,															//	uint32_t					bufferCount;
	};
	const Unique<VkCommandBuffer>			secCmdBuf				(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

	const VkCommandBufferInheritanceInfo	secCmdBufInheritInfo	=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
		DE_NULL,
		(VkRenderPass)0u,											// renderPass
		0u,															// subpass
		(VkFramebuffer)0u,											// framebuffer
		VK_FALSE,													// occlusionQueryEnable
		(VkQueryControlFlags)0u,									// queryFlags
		(VkQueryPipelineStatisticFlags)0u,							// pipelineStatistics
	};
	const VkCommandBufferBeginInfo			secCmdBufBeginInfo		=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		DE_NULL,
		0,															// flags
		&secCmdBufInheritInfo,
	};

	// create event that will be used to check if secondary command buffer has been executed
	const Unique<VkEvent>					event					(createEvent(vk, vkDevice));

	// record primary command buffer
	VK_CHECK(vk.beginCommandBuffer(*secCmdBuf, &secCmdBufBeginInfo));
	{
		// record setting event
		vk.cmdSetEvent(*secCmdBuf, *event, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
	}
	endCommandBuffer(vk, *secCmdBuf);

	return tcu::TestStatus::pass("Secondary buffer recorded successfully.");
}

tcu::TestStatus recordLargeSecondaryBufferTest(Context &context)
{
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkQueue							queue					= context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	const VkCommandPoolCreateInfo			cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					//	VkStructureType				sType;
		DE_NULL,													//	const void*					pNext;
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,			//	VkCommandPoolCreateFlags	flags;
		queueFamilyIndex,											//	deUint32					queueFamilyIndex;
	};
	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCommandBufferAllocateInfo		cmdBufParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				//	VkStructureType				sType;
		DE_NULL,													//	const void*					pNext;
		*cmdPool,													//	VkCommandPool				pool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,							//	VkCommandBufferLevel		level;
		1u,															//	uint32_t					bufferCount;
	};
	const Unique<VkCommandBuffer>			primCmdBuf				(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

	const VkCommandBufferAllocateInfo		secCmdBufParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				//	VkStructureType				sType;
		DE_NULL,													//	const void*					pNext;
		*cmdPool,													//	VkCommandPool				pool;
		VK_COMMAND_BUFFER_LEVEL_SECONDARY,							//	VkCommandBufferLevel		level;
		1u,															//	uint32_t					bufferCount;
	};
	const Unique<VkCommandBuffer>			secCmdBuf				(allocateCommandBuffer(vk, vkDevice, &secCmdBufParams));

	const VkCommandBufferInheritanceInfo	secCmdBufInheritInfo	=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
		DE_NULL,
		(VkRenderPass)0u,											// renderPass
		0u,															// subpass
		(VkFramebuffer)0u,											// framebuffer
		VK_FALSE,													// occlusionQueryEnable
		(VkQueryControlFlags)0u,									// queryFlags
		(VkQueryPipelineStatisticFlags)0u,							// pipelineStatistics
	};
	const VkCommandBufferBeginInfo			secCmdBufBeginInfo		=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		DE_NULL,
		0,															// flags
		&secCmdBufInheritInfo,
	};

	// create event that will be used to check if secondary command buffer has been executed
	const Unique<VkEvent>					event					(createEvent(vk, vkDevice));

	// reset event
	VK_CHECK(vk.resetEvent(vkDevice, *event));

	// record primary command buffer
	beginCommandBuffer(vk, *primCmdBuf, 0u);
	{
		// record secondary command buffer
		VK_CHECK(vk.beginCommandBuffer(*secCmdBuf, &secCmdBufBeginInfo));
		{
			// allow execution of event during every stage of pipeline
			VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

			// define minimal amount of commands to accept
			const long long unsigned minNumCommands = 10000llu;

			for ( long long unsigned currentCommands = 0; currentCommands < minNumCommands / 2; ++currentCommands )
			{
				// record setting event
				vk.cmdSetEvent(*primCmdBuf, *event,stageMask);

				// record resetting event
				vk.cmdResetEvent(*primCmdBuf, *event,stageMask);
			}
		}

		// end recording of secondary buffers
		endCommandBuffer(vk, *secCmdBuf);

		// execute secondary buffer
		vk.cmdExecuteCommands(*primCmdBuf, 1, &secCmdBuf.get());
	}
	endCommandBuffer(vk, *primCmdBuf);

	submitCommandsAndWait(vk, vkDevice, queue, primCmdBuf.get());

	return tcu::TestStatus::pass("hugeTest succeeded");
}

tcu::TestStatus submitPrimaryBufferTwiceTest(Context& context)
{
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkQueue							queue					= context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	const VkCommandPoolCreateInfo			cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					//	VkStructureType				sType;
		DE_NULL,													//	const void*					pNext;
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,			//	VkCommandPoolCreateFlags	flags;
		queueFamilyIndex,											//	deUint32					queueFamilyIndex;
	};
	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCommandBufferAllocateInfo		cmdBufParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		*cmdPool,													//	VkCommandPool				pool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,							//	VkCommandBufferLevel		level;
		1u,															//	uint32_t					bufferCount;
	};
	const Unique<VkCommandBuffer>			primCmdBuf				(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

	// create event that will be used to check if secondary command buffer has been executed
	const Unique<VkEvent>					event					(createEvent(vk, vkDevice));

	// reset event
	VK_CHECK(vk.resetEvent(vkDevice, *event));

	// record primary command buffer
	beginCommandBuffer(vk, *primCmdBuf, 0u);
	{
		// allow execution of event during every stage of pipeline
		VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

		// record setting event
		vk.cmdSetEvent(*primCmdBuf, *event,stageMask);
	}
	endCommandBuffer(vk, *primCmdBuf);

	submitCommandsAndWait(vk, vkDevice, queue, primCmdBuf.get());

	// check if buffer has been executed
	VkResult result = vk.getEventStatus(vkDevice,*event);
	if (result != VK_EVENT_SET)
		return tcu::TestStatus::fail("Submit Twice Test FAILED");

	// reset event
	VK_CHECK(vk.resetEvent(vkDevice, *event));

	submitCommandsAndWait(vk, vkDevice, queue, primCmdBuf.get());

	// check if buffer has been executed
	result = vk.getEventStatus(vkDevice,*event);
	if (result != VK_EVENT_SET)
		return tcu::TestStatus::fail("Submit Twice Test FAILED");
	else
		return tcu::TestStatus::pass("Submit Twice Test succeeded");
}

tcu::TestStatus submitSecondaryBufferTwiceTest(Context& context)
{
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkQueue							queue					= context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	const VkCommandPoolCreateInfo			cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					//	VkStructureType				sType;
		DE_NULL,													//	const void*					pNext;
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,			//	VkCommandPoolCreateFlags	flags;
		queueFamilyIndex,											//	deUint32					queueFamilyIndex;
	};

	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCommandBufferAllocateInfo		cmdBufParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		*cmdPool,													//	VkCommandPool				pool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,							//	VkCommandBufferLevel		level;
		1u,															//	uint32_t					bufferCount;
	};

	const Unique<VkCommandBuffer>			primCmdBuf1				(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));
	const Unique<VkCommandBuffer>			primCmdBuf2				(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

	// Secondary Command buffer
	const VkCommandBufferAllocateInfo		secCmdBufParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		*cmdPool,													//	VkCommandPool				pool;
		VK_COMMAND_BUFFER_LEVEL_SECONDARY,							//	VkCommandBufferLevel		level;
		1u,															//	uint32_t					bufferCount;
	};
	const Unique<VkCommandBuffer>			secCmdBuf				(allocateCommandBuffer(vk, vkDevice, &secCmdBufParams));

	const VkCommandBufferInheritanceInfo	secCmdBufInheritInfo	=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
		DE_NULL,
		(VkRenderPass)0u,											// renderPass
		0u,															// subpass
		(VkFramebuffer)0u,											// framebuffer
		VK_FALSE,													// occlusionQueryEnable
		(VkQueryControlFlags)0u,									// queryFlags
		(VkQueryPipelineStatisticFlags)0u,							// pipelineStatistics
	};
	const VkCommandBufferBeginInfo			secCmdBufBeginInfo		=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		DE_NULL,
		0u,															// flags
		&secCmdBufInheritInfo,
	};

	// create event that will be used to check if secondary command buffer has been executed
	const Unique<VkEvent>					event					(createEvent(vk, vkDevice));

	// reset event
	VK_CHECK(vk.resetEvent(vkDevice, *event));

	// record first primary command buffer
	beginCommandBuffer(vk, *primCmdBuf1, 0u);
	{
		// record secondary command buffer
		VK_CHECK(vk.beginCommandBuffer(*secCmdBuf, &secCmdBufBeginInfo));
		{
			// allow execution of event during every stage of pipeline
			VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

			// record setting event
			vk.cmdSetEvent(*secCmdBuf, *event,stageMask);
		}

		// end recording of secondary buffers
		endCommandBuffer(vk, *secCmdBuf);

		// execute secondary buffer
		vk.cmdExecuteCommands(*primCmdBuf1, 1, &secCmdBuf.get());
	}
	endCommandBuffer(vk, *primCmdBuf1);

	submitCommandsAndWait(vk, vkDevice, queue, primCmdBuf1.get());

	// check if secondary buffer has been executed
	VkResult result = vk.getEventStatus(vkDevice,*event);
	if (result != VK_EVENT_SET)
		return tcu::TestStatus::fail("Submit Twice Secondary Command Buffer FAILED");

	// reset first primary buffer
	vk.resetCommandBuffer( *primCmdBuf1, 0u);

	// reset event to allow receiving it again
	VK_CHECK(vk.resetEvent(vkDevice, *event));

	// record second primary command buffer
	beginCommandBuffer(vk, *primCmdBuf2, 0u);
	{
		// execute secondary buffer
		vk.cmdExecuteCommands(*primCmdBuf2, 1, &secCmdBuf.get());
	}
	// end recording
	endCommandBuffer(vk, *primCmdBuf2);

	submitCommandsAndWait(vk, vkDevice, queue, primCmdBuf2.get());

	// check if secondary buffer has been executed
	result = vk.getEventStatus(vkDevice,*event);
	if (result != VK_EVENT_SET)
		return tcu::TestStatus::fail("Submit Twice Secondary Command Buffer FAILED");
	else
		return tcu::TestStatus::pass("Submit Twice Secondary Command Buffer succeeded");
}

tcu::TestStatus oneTimeSubmitFlagPrimaryBufferTest(Context& context)
{
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkQueue							queue					= context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	const VkCommandPoolCreateInfo			cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					//	VkStructureType				sType;
		DE_NULL,													//	const void*					pNext;
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,			//	VkCommandPoolCreateFlags	flags;
		queueFamilyIndex,											//	deUint32					queueFamilyIndex;
	};
	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCommandBufferAllocateInfo		cmdBufParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				//	VkStructureType				sType;
		DE_NULL,													//	const void*					pNext;
		*cmdPool,													//	VkCommandPool				pool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,							//	VkCommandBufferLevel		level;
		1u,															//	uint32_t					bufferCount;
	};
	const Unique<VkCommandBuffer>			primCmdBuf				(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

	// create event that will be used to check if secondary command buffer has been executed
	const Unique<VkEvent>					event					(createEvent(vk, vkDevice));

	// reset event
	VK_CHECK(vk.resetEvent(vkDevice, *event));

	// record primary command buffer
	beginCommandBuffer(vk, *primCmdBuf);
	{
		// allow execution of event during every stage of pipeline
		VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

		// record setting event
		vk.cmdSetEvent(*primCmdBuf, *event,stageMask);
	}
	endCommandBuffer(vk, *primCmdBuf);

	submitCommandsAndWait(vk, vkDevice, queue, primCmdBuf.get());

	// check if buffer has been executed
	VkResult result = vk.getEventStatus(vkDevice,*event);
	if (result != VK_EVENT_SET)
		return tcu::TestStatus::fail("oneTimeSubmitFlagPrimaryBufferTest FAILED");

	// record primary command buffer again - implicit reset because of VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
	beginCommandBuffer(vk, *primCmdBuf);
	{
		// allow execution of event during every stage of pipeline
		VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

		// record setting event
		vk.cmdSetEvent(*primCmdBuf, *event,stageMask);
	}
	endCommandBuffer(vk, *primCmdBuf);

	submitCommandsAndWait(vk, vkDevice, queue, primCmdBuf.get());

	// check if buffer has been executed
	result = vk.getEventStatus(vkDevice,*event);
	if (result != VK_EVENT_SET)
		return tcu::TestStatus::fail("oneTimeSubmitFlagPrimaryBufferTest FAILED");
	else
		return tcu::TestStatus::pass("oneTimeSubmitFlagPrimaryBufferTest succeeded");
}

tcu::TestStatus oneTimeSubmitFlagSecondaryBufferTest(Context& context)
{
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkQueue							queue					= context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	const VkCommandPoolCreateInfo			cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					//	VkStructureType				sType;
		DE_NULL,													//	const void*					pNext;
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,			//	VkCommandPoolCreateFlags	flags;
		queueFamilyIndex,											//	deUint32					queueFamilyIndex;
	};

	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCommandBufferAllocateInfo		cmdBufParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		*cmdPool,													//	VkCommandPool				pool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,							//	VkCommandBufferLevel		level;
		1u,															//	uint32_t					bufferCount;
	};

	const Unique<VkCommandBuffer>			primCmdBuf1				(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));
	const Unique<VkCommandBuffer>			primCmdBuf2				(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

	// Secondary Command buffer
	const VkCommandBufferAllocateInfo		secCmdBufParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		*cmdPool,													//	VkCommandPool				pool;
		VK_COMMAND_BUFFER_LEVEL_SECONDARY,							//	VkCommandBufferLevel		level;
		1u,															//	uint32_t					bufferCount;
	};
	const Unique<VkCommandBuffer>			secCmdBuf				(allocateCommandBuffer(vk, vkDevice, &secCmdBufParams));

	const VkCommandBufferInheritanceInfo	secCmdBufInheritInfo	=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
		DE_NULL,
		(VkRenderPass)0u,											// renderPass
		0u,															// subpass
		(VkFramebuffer)0u,											// framebuffer
		VK_FALSE,													// occlusionQueryEnable
		(VkQueryControlFlags)0u,									// queryFlags
		(VkQueryPipelineStatisticFlags)0u,							// pipelineStatistics
	};
	const VkCommandBufferBeginInfo			secCmdBufBeginInfo		=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		DE_NULL,
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,				// flags
		&secCmdBufInheritInfo,
	};

	// create event that will be used to check if secondary command buffer has been executed
	const Unique<VkEvent>					event					(createEvent(vk, vkDevice));

	// reset event
	VK_CHECK(vk.resetEvent(vkDevice, *event));

	// record first primary command buffer
	beginCommandBuffer(vk, *primCmdBuf1, 0u);
	{
		// record secondary command buffer
		VK_CHECK(vk.beginCommandBuffer(*secCmdBuf, &secCmdBufBeginInfo));
		{
			// allow execution of event during every stage of pipeline
			VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

			// record setting event
			vk.cmdSetEvent(*secCmdBuf, *event,stageMask);
		}

		// end recording of secondary buffers
		endCommandBuffer(vk, *secCmdBuf);

		// execute secondary buffer
		vk.cmdExecuteCommands(*primCmdBuf1, 1, &secCmdBuf.get());
	}
	endCommandBuffer(vk, *primCmdBuf1);

	submitCommandsAndWait(vk, vkDevice, queue, primCmdBuf1.get());

	// check if secondary buffer has been executed
	VkResult result = vk.getEventStatus(vkDevice,*event);
	if (result != VK_EVENT_SET)
		return tcu::TestStatus::fail("Submit Twice Secondary Command Buffer FAILED");

	// reset first primary buffer
	vk.resetCommandBuffer( *primCmdBuf1, 0u);

	// reset event to allow receiving it again
	VK_CHECK(vk.resetEvent(vkDevice, *event));

	// record secondary command buffer again
	VK_CHECK(vk.beginCommandBuffer(*secCmdBuf, &secCmdBufBeginInfo));
	{
		// allow execution of event during every stage of pipeline
		VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

		// record setting event
		vk.cmdSetEvent(*secCmdBuf, *event,stageMask);
	}
	// end recording of secondary buffers
	endCommandBuffer(vk, *secCmdBuf);

	// record second primary command buffer
	beginCommandBuffer(vk, *primCmdBuf2, 0u);
	{
		// execute secondary buffer
		vk.cmdExecuteCommands(*primCmdBuf2, 1, &secCmdBuf.get());
	}
	// end recording
	endCommandBuffer(vk, *primCmdBuf2);

	submitCommandsAndWait(vk, vkDevice, queue, primCmdBuf2.get());

	// check if secondary buffer has been executed
	result = vk.getEventStatus(vkDevice,*event);
	if (result != VK_EVENT_SET)
		return tcu::TestStatus::fail("oneTimeSubmitFlagSecondaryBufferTest FAILED");
	else
		return tcu::TestStatus::pass("oneTimeSubmitFlagSecondaryBufferTest succeeded");
}

tcu::TestStatus renderPassContinueTest(Context& context, bool framebufferHint)
{
	const DeviceInterface&					vkd						= context.getDeviceInterface();
	CommandBufferRenderPassTestEnvironment	env						(context, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	VkCommandBuffer							primaryCommandBuffer	= env.getPrimaryCommandBuffer();
	VkCommandBuffer							secondaryCommandBuffer	= env.getSecondaryCommandBuffer();
	const deUint32							clearColor[4]			= { 2, 47, 131, 211 };

	const VkClearAttachment					clearAttachment			=
	{
		VK_IMAGE_ASPECT_COLOR_BIT,									// VkImageAspectFlags	aspectMask;
		0,															// deUint32				colorAttachment;
		makeClearValueColorU32(clearColor[0],
							   clearColor[1],
							   clearColor[2],
							   clearColor[3])						// VkClearValue			clearValue;
	};

	const VkClearRect						clearRect				=
	{
		CommandBufferRenderPassTestEnvironment::DEFAULT_IMAGE_AREA,	// VkRect2D	rect;
		0u,															// deUint32	baseArrayLayer;
		1u															// deUint32	layerCount;
	};

	env.beginSecondaryCommandBuffer(VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, framebufferHint);
	vkd.cmdClearAttachments(secondaryCommandBuffer, 1, &clearAttachment, 1, &clearRect);
	endCommandBuffer(vkd, secondaryCommandBuffer);


	env.beginPrimaryCommandBuffer(0);
	env.beginRenderPass(VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
	vkd.cmdExecuteCommands(primaryCommandBuffer, 1, &secondaryCommandBuffer);
	endRenderPass(vkd, primaryCommandBuffer);

	endCommandBuffer(vkd, primaryCommandBuffer);

	env.submitPrimaryCommandBuffer();

	de::MovePtr<tcu::TextureLevel>			result					= env.readColorAttachment();
	tcu::PixelBufferAccess					pixelBufferAccess		= result->getAccess();

	for (deUint32 i = 0; i < (CommandBufferRenderPassTestEnvironment::DEFAULT_IMAGE_SIZE.width * CommandBufferRenderPassTestEnvironment::DEFAULT_IMAGE_SIZE.height); ++i)
	{
		deUint8* colorData = reinterpret_cast<deUint8*>(pixelBufferAccess.getDataPtr());
		for (int colorComponent = 0; colorComponent < 4; ++colorComponent)
			if (colorData[i * 4 + colorComponent] != clearColor[colorComponent])
				return tcu::TestStatus::fail("clear value mismatch");
	}

	return tcu::TestStatus::pass("render pass continue test passed");
}

tcu::TestStatus simultaneousUsePrimaryBufferTest(Context& context)
{
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkQueue							queue					= context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	const VkCommandPoolCreateInfo			cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					//	VkStructureType				sType;
		DE_NULL,													//	const void*					pNext;
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,			//	VkCommandPoolCreateFlags	flags;
		queueFamilyIndex,											//	deUint32					queueFamilyIndex;
	};
	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCommandBufferAllocateInfo		cmdBufParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				//	VkStructureType				sType;
		DE_NULL,													//	const void*					pNext;
		*cmdPool,													//	VkCommandPool				pool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,							//	VkCommandBufferLevel		level;
		1u,															//	uint32_t					bufferCount;
	};
	const Unique<VkCommandBuffer>			primCmdBuf				(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

	// create event that will be used to check if secondary command buffer has been executed
	const Unique<VkEvent>					eventOne				(createEvent(vk, vkDevice));
	const Unique<VkEvent>					eventTwo				(createEvent(vk, vkDevice));

	// reset event
	VK_CHECK(vk.resetEvent(vkDevice, *eventOne));

	// record primary command buffer
	beginCommandBuffer(vk, *primCmdBuf, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);
	{
		// wait for event
		vk.cmdWaitEvents(*primCmdBuf, 1u, &eventOne.get(), VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0u, DE_NULL, 0u, DE_NULL, 0u, DE_NULL);

		// Set the second event
		vk.cmdSetEvent(*primCmdBuf, eventTwo.get(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
	}
	endCommandBuffer(vk, *primCmdBuf);

	// create fence to wait for execution of queue
	const Unique<VkFence>					fence1					(createFence(vk, vkDevice));
	const Unique<VkFence>					fence2					(createFence(vk, vkDevice));

	const VkSubmitInfo						submitInfo				=
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,								// sType
		DE_NULL,													// pNext
		0u,															// waitSemaphoreCount
		DE_NULL,													// pWaitSemaphores
		(const VkPipelineStageFlags*)DE_NULL,						// pWaitDstStageMask
		1,															// commandBufferCount
		&primCmdBuf.get(),											// pCommandBuffers
		0u,															// signalSemaphoreCount
		DE_NULL,													// pSignalSemaphores
	};

	// submit first buffer
	VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo, *fence1));

	// submit second buffer
	VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo, *fence2));

	// wait for both buffer to stop at event for 100 microseconds
	vk.waitForFences(vkDevice, 1, &fence1.get(), 0u, 100000);
	vk.waitForFences(vkDevice, 1, &fence2.get(), 0u, 100000);

	// set event
	VK_CHECK(vk.setEvent(vkDevice, *eventOne));

	// wait for end of execution of the first buffer
	VK_CHECK(vk.waitForFences(vkDevice, 1, &fence1.get(), 0u, INFINITE_TIMEOUT));
	// wait for end of execution of the second buffer
	VK_CHECK(vk.waitForFences(vkDevice, 1, &fence2.get(), 0u, INFINITE_TIMEOUT));

	// TODO: this will be true if the command buffer was executed only once
	// TODO: add some test that will say if it was executed twice

	// check if buffer has been executed
	VkResult result = vk.getEventStatus(vkDevice, *eventTwo);
	if (result == VK_EVENT_SET)
		return tcu::TestStatus::pass("simultaneous use - primary buffers test succeeded");
	else
		return tcu::TestStatus::fail("simultaneous use - primary buffers test FAILED");
}

tcu::TestStatus simultaneousUseSecondaryBufferTest(Context& context)
{
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkQueue							queue					= context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	const VkCommandPoolCreateInfo			cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					//	VkStructureType				sType;
		DE_NULL,													//	const void*					pNext;
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,			//	VkCommandPoolCreateFlags	flags;
		queueFamilyIndex,											//	deUint32					queueFamilyIndex;
	};
	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCommandBufferAllocateInfo		cmdBufParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		*cmdPool,													//	VkCommandPool				pool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,							//	VkCommandBufferLevel		level;
		1u,															//	uint32_t					bufferCount;
	};
	const Unique<VkCommandBuffer>			primCmdBuf				(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

	// Secondary Command buffer params
	const VkCommandBufferAllocateInfo		secCmdBufParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		*cmdPool,													//	VkCommandPool				pool;
		VK_COMMAND_BUFFER_LEVEL_SECONDARY,							//	VkCommandBufferLevel		level;
		1u,															//	uint32_t					bufferCount;
	};
	const Unique<VkCommandBuffer>			secCmdBuf				(allocateCommandBuffer(vk, vkDevice, &secCmdBufParams));

	const VkCommandBufferInheritanceInfo	secCmdBufInheritInfo	=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
		DE_NULL,
		(VkRenderPass)0u,											// renderPass
		0u,															// subpass
		(VkFramebuffer)0u,											// framebuffer
		VK_FALSE,													// occlusionQueryEnable
		(VkQueryControlFlags)0u,									// queryFlags
		(VkQueryPipelineStatisticFlags)0u,							// pipelineStatistics
	};
	const VkCommandBufferBeginInfo			secCmdBufBeginInfo		=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		DE_NULL,
		VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,				// flags
		&secCmdBufInheritInfo,
	};

	// create event that will be used to check if secondary command buffer has been executed
	const Unique<VkEvent>					eventOne				(createEvent(vk, vkDevice));
	const Unique<VkEvent>					eventTwo				(createEvent(vk, vkDevice));

	// reset event
	VK_CHECK(vk.resetEvent(vkDevice, *eventOne));
	VK_CHECK(vk.resetEvent(vkDevice, *eventTwo));

	// record secondary command buffer
	VK_CHECK(vk.beginCommandBuffer(*secCmdBuf, &secCmdBufBeginInfo));
	{
		// allow execution of event during every stage of pipeline
		VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

		// wait for event
		vk.cmdWaitEvents(*secCmdBuf, 1, &eventOne.get(), stageMask, stageMask, 0, DE_NULL, 0u, DE_NULL, 0u, DE_NULL);

		// reset event
		vk.cmdSetEvent(*secCmdBuf, *eventTwo, stageMask);
	}
	// end recording of secondary buffers
	endCommandBuffer(vk, *secCmdBuf);

	// record primary command buffer
	beginCommandBuffer(vk, *primCmdBuf, 0u);
	{
		// execute secondary buffer
		vk.cmdExecuteCommands(*primCmdBuf, 1, &secCmdBuf.get());
	}
	endCommandBuffer(vk, *primCmdBuf);

	// create fence to wait for execution of queue
	const Unique<VkFence>					fence					(createFence(vk, vkDevice));

	const VkSubmitInfo						submitInfo				=
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,								// sType
		DE_NULL,													// pNext
		0u,															// waitSemaphoreCount
		DE_NULL,													// pWaitSemaphores
		(const VkPipelineStageFlags*)DE_NULL,						// pWaitDstStageMask
		1,															// commandBufferCount
		&primCmdBuf.get(),											// pCommandBuffers
		0u,															// signalSemaphoreCount
		DE_NULL,													// pSignalSemaphores
	};

	// submit primary buffer, the secondary should be executed too
	VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo, *fence));

	// wait for both buffers to stop at event for 100 microseconds
	vk.waitForFences(vkDevice, 1, &fence.get(), 0u, 100000);

	// set event
	VK_CHECK(vk.setEvent(vkDevice, *eventOne));

	// wait for end of execution of queue
	VK_CHECK(vk.waitForFences(vkDevice, 1, &fence.get(), 0u, INFINITE_TIMEOUT));

	// TODO: this will be true if the command buffer was executed only once
	// TODO: add some test that will say if it was executed twice

	// check if secondary buffer has been executed
	VkResult result = vk.getEventStatus(vkDevice,*eventTwo);
	if (result == VK_EVENT_SET)
		return tcu::TestStatus::pass("Simultaneous Secondary Command Buffer Execution succeeded");
	else
		return tcu::TestStatus::fail("Simultaneous Secondary Command Buffer Execution FAILED");
}

tcu::TestStatus simultaneousUseSecondaryBufferOnePrimaryBufferTest(Context& context)
{
	const VkDevice							vkDevice = context.getDevice();
	const DeviceInterface&					vk = context.getDeviceInterface();
	const VkQueue							queue = context.getUniversalQueue();
	const deUint32							queueFamilyIndex = context.getUniversalQueueFamilyIndex();
	Allocator&								allocator = context.getDefaultAllocator();
	const ComputeInstanceResultBuffer		result(vk, vkDevice, allocator, 0.0f);

	const VkCommandPoolCreateInfo			cmdPoolParams =
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					//	VkStructureType				sType;
		DE_NULL,													//	const void*					pNext;
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,			//	VkCommandPoolCreateFlags	flags;
		queueFamilyIndex,											//	deUint32					queueFamilyIndex;
	};
	const Unique<VkCommandPool>				cmdPool(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCommandBufferAllocateInfo		cmdBufParams =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		*cmdPool,													//	VkCommandPool				pool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,							//	VkCommandBufferLevel		level;
		1u,															//	uint32_t					bufferCount;
	};
	const Unique<VkCommandBuffer>			primCmdBuf(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

	// Secondary Command buffer params
	const VkCommandBufferAllocateInfo		secCmdBufParams =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		*cmdPool,													//	VkCommandPool				pool;
		VK_COMMAND_BUFFER_LEVEL_SECONDARY,							//	VkCommandBufferLevel		level;
		1u,															//	uint32_t					bufferCount;
	};
	const Unique<VkCommandBuffer>			secCmdBuf(allocateCommandBuffer(vk, vkDevice, &secCmdBufParams));

	const VkCommandBufferInheritanceInfo	secCmdBufInheritInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
		DE_NULL,
		(VkRenderPass)0u,
		0u,															// subpass
		(VkFramebuffer)0u,
		VK_FALSE,													// occlusionQueryEnable
		(VkQueryControlFlags)0u,
		(VkQueryPipelineStatisticFlags)0u,
	};
	const VkCommandBufferBeginInfo			secCmdBufBeginInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		DE_NULL,
		VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,				// flags
		&secCmdBufInheritInfo,
	};

	const deUint32							offset = (0u);
	const deUint32							addressableSize = 256;
	const deUint32							dataSize = 8;
	de::MovePtr<Allocation>					bufferMem;
	const Unique<VkBuffer>					buffer(createDataBuffer(context, offset, addressableSize, 0x00, dataSize, 0x5A, &bufferMem));
	// Secondary command buffer will have a compute shader that does an atomic increment to make sure that all instances of secondary buffers execute
	const Unique<VkDescriptorSetLayout>		descriptorSetLayout(createDescriptorSetLayout(context));
	const Unique<VkDescriptorPool>			descriptorPool(createDescriptorPool(context));
	const Unique<VkDescriptorSet>			descriptorSet(createDescriptorSet(context, *descriptorPool, *descriptorSetLayout, *buffer, offset, result.getBuffer()));
	const VkDescriptorSet					descriptorSets[] = { *descriptorSet };
	const int								numDescriptorSets = DE_LENGTH_OF_ARRAY(descriptorSets);

	const VkPipelineLayoutCreateInfo layoutCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,				// sType
		DE_NULL,													// pNext
		(VkPipelineLayoutCreateFlags)0,
		numDescriptorSets,											// setLayoutCount
		&descriptorSetLayout.get(),									// pSetLayouts
		0u,															// pushConstantRangeCount
		DE_NULL,													// pPushConstantRanges
	};
	Unique<VkPipelineLayout>				pipelineLayout(createPipelineLayout(vk, vkDevice, &layoutCreateInfo));

	const Unique<VkShaderModule>			computeModule(createShaderModule(vk, vkDevice, context.getBinaryCollection().get("compute_increment"), (VkShaderModuleCreateFlags)0u));

	const VkPipelineShaderStageCreateInfo	shaderCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		DE_NULL,
		(VkPipelineShaderStageCreateFlags)0,
		VK_SHADER_STAGE_COMPUTE_BIT,								// stage
		*computeModule,												// shader
		"main",
		DE_NULL,													// pSpecializationInfo
	};

	const VkComputePipelineCreateInfo		pipelineCreateInfo =
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		DE_NULL,
		0u,															// flags
		shaderCreateInfo,											// cs
		*pipelineLayout,											// layout
		(vk::VkPipeline)0,											// basePipelineHandle
		0u,															// basePipelineIndex
	};

	const VkBufferMemoryBarrier				bufferBarrier =
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,					// sType
		DE_NULL,													// pNext
		VK_ACCESS_SHADER_WRITE_BIT,									// srcAccessMask
		VK_ACCESS_HOST_READ_BIT,									// dstAccessMask
		VK_QUEUE_FAMILY_IGNORED,									// srcQueueFamilyIndex
		VK_QUEUE_FAMILY_IGNORED,									// destQueueFamilyIndex
		*buffer,													// buffer
		(VkDeviceSize)0u,											// offset
		(VkDeviceSize)VK_WHOLE_SIZE,								// size
	};

	const Unique<VkPipeline>				pipeline(createComputePipeline(vk, vkDevice, (VkPipelineCache)0u, &pipelineCreateInfo));

	// record secondary command buffer
	VK_CHECK(vk.beginCommandBuffer(*secCmdBuf, &secCmdBufBeginInfo));
	{
		vk.cmdBindPipeline(*secCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
		vk.cmdBindDescriptorSets(*secCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0, numDescriptorSets, descriptorSets, 0, 0);
		vk.cmdDispatch(*secCmdBuf, 1u, 1u, 1u);
		vk.cmdPipelineBarrier(*secCmdBuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0,
						  0, (const VkMemoryBarrier*)DE_NULL,
						  1, &bufferBarrier,
						  0, (const VkImageMemoryBarrier*)DE_NULL);
	}
	// end recording of secondary buffer
	endCommandBuffer(vk, *secCmdBuf);

	// record primary command buffer
	beginCommandBuffer(vk, *primCmdBuf, 0u);
	{
		// execute secondary buffer twice in same primary
		vk.cmdExecuteCommands(*primCmdBuf, 1, &secCmdBuf.get());
		vk.cmdExecuteCommands(*primCmdBuf, 1, &secCmdBuf.get());
	}
	endCommandBuffer(vk, *primCmdBuf);

	submitCommandsAndWait(vk, vkDevice, queue, primCmdBuf.get());

	deUint32 resultCount;
	result.readResultContentsTo(&resultCount);
	// check if secondary buffer has been executed
	if (resultCount == 2)
		return tcu::TestStatus::pass("Simultaneous Secondary Command Buffer Execution succeeded");
	else
		return tcu::TestStatus::fail("Simultaneous Secondary Command Buffer Execution FAILED");
}

enum class BadInheritanceInfoCase
{
	RANDOM_PTR = 0,
	RANDOM_PTR_CONTINUATION,
	RANDOM_DATA_PTR,
	INVALID_STRUCTURE_TYPE,
	VALID_NONSENSE_TYPE,
};

tcu::TestStatus badInheritanceInfoTest (Context& context, BadInheritanceInfoCase testCase)
{
	const auto&							vkd					= context.getDeviceInterface();
	const auto							device				= context.getDevice();
	const auto							queue				= context.getUniversalQueue();
	const auto							queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	auto&								allocator			= context.getDefaultAllocator();
	const ComputeInstanceResultBuffer	result				(vkd, device, allocator, 0.0f);

	// Command pool and command buffer.
	const auto							cmdPool			= makeCommandPool(vkd, device, queueFamilyIndex);
	const auto							cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto							cmdBuffer		= cmdBufferPtr.get();

	// Buffers, descriptor set layouts and descriptor sets.
	const deUint32							offset			= 0u;
	const deUint32							addressableSize	= 256u;
	const deUint32							dataSize		= 8u;

	// The uniform buffer will not be used by the shader but is needed by auxiliar functions here.
	de::MovePtr<Allocation>					bufferMem;
	const Unique<VkBuffer>					buffer(createDataBuffer(context, offset, addressableSize, 0x00, dataSize, 0x5A, &bufferMem));

	const Unique<VkDescriptorSetLayout>		descriptorSetLayout	(createDescriptorSetLayout(context));
	const Unique<VkDescriptorPool>			descriptorPool		(createDescriptorPool(context));
	const Unique<VkDescriptorSet>			descriptorSet		(createDescriptorSet(context, *descriptorPool, *descriptorSetLayout, *buffer, offset, result.getBuffer()));
	const VkDescriptorSet					descriptorSets[]	= { *descriptorSet };
	const int								numDescriptorSets	= DE_LENGTH_OF_ARRAY(descriptorSets);

	// Pipeline layout.
	const auto								pipelineLayout		= makePipelineLayout(vkd, device, descriptorSetLayout.get());

	// Compute shader module.
	const Unique<VkShaderModule>			computeModule		(createShaderModule(vkd, device, context.getBinaryCollection().get("compute_increment"), (VkShaderModuleCreateFlags)0u));

	const VkPipelineShaderStageCreateInfo	shaderCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		DE_NULL,
		(VkPipelineShaderStageCreateFlags)0,
		VK_SHADER_STAGE_COMPUTE_BIT,								// stage
		*computeModule,												// shader
		"main",
		DE_NULL,													// pSpecializationInfo
	};

	const VkComputePipelineCreateInfo		pipelineCreateInfo	=
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		DE_NULL,
		0u,															// flags
		shaderCreateInfo,											// cs
		*pipelineLayout,											// layout
		(vk::VkPipeline)0,											// basePipelineHandle
		0u,															// basePipelineIndex
	};

	const Unique<VkPipeline>				pipeline			(createComputePipeline(vkd, device, (VkPipelineCache)0u, &pipelineCreateInfo));

	// Compute to host barrier to read result.
	const VkBufferMemoryBarrier				bufferBarrier		=
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,					// sType
		DE_NULL,													// pNext
		VK_ACCESS_SHADER_WRITE_BIT,									// srcAccessMask
		VK_ACCESS_HOST_READ_BIT,									// dstAccessMask
		VK_QUEUE_FAMILY_IGNORED,									// srcQueueFamilyIndex
		VK_QUEUE_FAMILY_IGNORED,									// destQueueFamilyIndex
		*buffer,													// buffer
		(VkDeviceSize)0u,											// offset
		(VkDeviceSize)VK_WHOLE_SIZE,								// size
	};

	// Record command buffer and submit it.
	VkCommandBufferBeginInfo				beginInfo			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	//	VkStructureType							sType;
		nullptr,										//	const void*								pNext;
		0u,												//	VkCommandBufferUsageFlags				flags;
		nullptr,										//	const VkCommandBufferInheritanceInfo*	pInheritanceInfo;
	};

	// Structures used in different test types.
	VkCommandBufferInheritanceInfo			inheritanceInfo;
	VkBufferCreateInfo						validNonsenseStructure;
	struct
	{
		VkStructureType	sType;
		void*			pNext;
	} invalidStructure;

	if (testCase == BadInheritanceInfoCase::RANDOM_PTR || testCase == BadInheritanceInfoCase::RANDOM_PTR_CONTINUATION)
	{
		de::Random						rnd		(1602600778u);
		VkCommandBufferInheritanceInfo*	info;
		auto							ptrData	= reinterpret_cast<deUint8*>(&info);

		// Fill pointer value with pseudorandom garbage.
		for (size_t i = 0; i < sizeof(info); ++i)
			*ptrData++ = rnd.getUint8();

		beginInfo.pInheritanceInfo = info;

		// Try to trick the implementation into reading pInheritanceInfo one more way.
		if (testCase == BadInheritanceInfoCase::RANDOM_PTR_CONTINUATION)
			beginInfo.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;

	}
	else if (testCase == BadInheritanceInfoCase::RANDOM_DATA_PTR)
	{
		de::Random		rnd	(1602601141u);
		auto			itr	= reinterpret_cast<deUint8*>(&inheritanceInfo);

		// Fill inheritance info data structure with random data.
		for (size_t i = 0; i < sizeof(inheritanceInfo); ++i)
			*itr++ = rnd.getUint8();

		beginInfo.pInheritanceInfo = &inheritanceInfo;
	}
	else if (testCase == BadInheritanceInfoCase::INVALID_STRUCTURE_TYPE)
	{
		de::Random	rnd			(1602658515u);
		auto		ptrData		= reinterpret_cast<deUint8*>(&(invalidStructure.pNext));
		invalidStructure.sType	= VK_STRUCTURE_TYPE_MAX_ENUM;

		// Fill pNext pointer with random data.
		for (size_t i = 0; i < sizeof(invalidStructure.pNext); ++i)
			*ptrData++ = rnd.getUint8();

		beginInfo.pInheritanceInfo = reinterpret_cast<VkCommandBufferInheritanceInfo*>(&invalidStructure);
	}
	else if (testCase == BadInheritanceInfoCase::VALID_NONSENSE_TYPE)
	{
		validNonsenseStructure.sType					= VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		validNonsenseStructure.pNext					= nullptr;
		validNonsenseStructure.flags					= 0u;
		validNonsenseStructure.size						= 1024u;
		validNonsenseStructure.usage					= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		validNonsenseStructure.sharingMode				= VK_SHARING_MODE_EXCLUSIVE;
		validNonsenseStructure.queueFamilyIndexCount	= 0u;
		validNonsenseStructure.pQueueFamilyIndices		= nullptr;

		beginInfo.pInheritanceInfo						= reinterpret_cast<VkCommandBufferInheritanceInfo*>(&validNonsenseStructure);
	}
	else
	{
		DE_ASSERT(false);
	}

	VK_CHECK(vkd.beginCommandBuffer(cmdBuffer, &beginInfo));
	{
		vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
		vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0, numDescriptorSets, descriptorSets, 0, 0);
		vkd.cmdDispatch(cmdBuffer, 1u, 1u, 1u);
		vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0,
							   0, (const VkMemoryBarrier*)DE_NULL,
							   1, &bufferBarrier,
							   0, (const VkImageMemoryBarrier*)DE_NULL);
	}
	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	deUint32 resultCount;
	result.readResultContentsTo(&resultCount);

	// Make sure the command buffer was run.
	if (resultCount != 1u)
	{
		std::ostringstream msg;
		msg << "Invalid value found in results buffer (expected value 1u but found " << resultCount << ")";
		return tcu::TestStatus::fail(msg.str());
	}

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus simultaneousUseSecondaryBufferTwoPrimaryBuffersTest(Context& context)
{
	const VkDevice							vkDevice = context.getDevice();
	const DeviceInterface&					vk = context.getDeviceInterface();
	const VkQueue							queue = context.getUniversalQueue();
	const deUint32							queueFamilyIndex = context.getUniversalQueueFamilyIndex();
	Allocator&								allocator = context.getDefaultAllocator();
	const ComputeInstanceResultBuffer		result(vk, vkDevice, allocator, 0.0f);

	const VkCommandPoolCreateInfo			cmdPoolParams =
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					//	VkStructureType				sType;
		DE_NULL,													//	const void*					pNext;
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,			//	VkCommandPoolCreateFlags	flags;
		queueFamilyIndex,											//	deUint32					queueFamilyIndex;
	};
	const Unique<VkCommandPool>				cmdPool(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCommandBufferAllocateInfo		cmdBufParams =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		*cmdPool,													//	VkCommandPool				pool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,							//	VkCommandBufferLevel		level;
		1u,															//	uint32_t					bufferCount;
	};
	// Two separate primary cmd buffers that will be executed with the same secondary cmd buffer
	const deUint32 numPrimCmdBufs = 2;
	const Unique<VkCommandBuffer>			primCmdBufOne(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));
	const Unique<VkCommandBuffer>			primCmdBufTwo(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));
	VkCommandBuffer primCmdBufs[numPrimCmdBufs];
	primCmdBufs[0] = primCmdBufOne.get();
	primCmdBufs[1] = primCmdBufTwo.get();

	// Secondary Command buffer params
	const VkCommandBufferAllocateInfo		secCmdBufParams =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		*cmdPool,													//	VkCommandPool				pool;
		VK_COMMAND_BUFFER_LEVEL_SECONDARY,							//	VkCommandBufferLevel		level;
		1u,															//	uint32_t					bufferCount;
	};
	const Unique<VkCommandBuffer>			secCmdBuf(allocateCommandBuffer(vk, vkDevice, &secCmdBufParams));

	const VkCommandBufferBeginInfo			primCmdBufBeginInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		DE_NULL,
		0,															// flags
		(const VkCommandBufferInheritanceInfo*)DE_NULL,
	};

	const VkCommandBufferInheritanceInfo	secCmdBufInheritInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
		DE_NULL,
		(VkRenderPass)0u,											// renderPass
		0u,															// subpass
		(VkFramebuffer)0u,											// framebuffer
		VK_FALSE,													// occlusionQueryEnable
		(VkQueryControlFlags)0u,									// queryFlags
		(VkQueryPipelineStatisticFlags)0u,							// pipelineStatistics
	};
	const VkCommandBufferBeginInfo			secCmdBufBeginInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		DE_NULL,
		VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,				// flags
		&secCmdBufInheritInfo,
	};

	const deUint32							offset = (0u);
	const deUint32							addressableSize = 256;
	const deUint32							dataSize = 8;
	de::MovePtr<Allocation>					bufferMem;
	const Unique<VkBuffer>					buffer(createDataBuffer(context, offset, addressableSize, 0x00, dataSize, 0x5A, &bufferMem));
	// Secondary command buffer will have a compute shader that does an atomic increment to make sure that all instances of secondary buffers execute
	const Unique<VkDescriptorSetLayout>		descriptorSetLayout(createDescriptorSetLayout(context));
	const Unique<VkDescriptorPool>			descriptorPool(createDescriptorPool(context));
	const Unique<VkDescriptorSet>			descriptorSet(createDescriptorSet(context, *descriptorPool, *descriptorSetLayout, *buffer, offset, result.getBuffer()));
	const VkDescriptorSet					descriptorSets[] = { *descriptorSet };
	const int								numDescriptorSets = DE_LENGTH_OF_ARRAY(descriptorSets);

	const VkPipelineLayoutCreateInfo layoutCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,				// sType
		DE_NULL,													// pNext
		(VkPipelineLayoutCreateFlags)0,
		numDescriptorSets,											// setLayoutCount
		&descriptorSetLayout.get(),									// pSetLayouts
		0u,															// pushConstantRangeCount
		DE_NULL,													// pPushConstantRanges
	};
	Unique<VkPipelineLayout>				pipelineLayout(createPipelineLayout(vk, vkDevice, &layoutCreateInfo));

	const Unique<VkShaderModule>			computeModule(createShaderModule(vk, vkDevice, context.getBinaryCollection().get("compute_increment"), (VkShaderModuleCreateFlags)0u));

	const VkPipelineShaderStageCreateInfo	shaderCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		DE_NULL,
		(VkPipelineShaderStageCreateFlags)0,
		VK_SHADER_STAGE_COMPUTE_BIT,								// stage
		*computeModule,												// shader
		"main",
		DE_NULL,													// pSpecializationInfo
	};

	const VkComputePipelineCreateInfo		pipelineCreateInfo =
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		DE_NULL,
		0u,															// flags
		shaderCreateInfo,											// cs
		*pipelineLayout,											// layout
		(vk::VkPipeline)0,											// basePipelineHandle
		0u,															// basePipelineIndex
	};

	const Unique<VkPipeline>				pipeline(createComputePipeline(vk, vkDevice, (VkPipelineCache)0u, &pipelineCreateInfo));

	// record secondary command buffer
	VK_CHECK(vk.beginCommandBuffer(*secCmdBuf, &secCmdBufBeginInfo));
	{
		vk.cmdBindPipeline(*secCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
		vk.cmdBindDescriptorSets(*secCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0, numDescriptorSets, descriptorSets, 0, 0);
		vk.cmdDispatch(*secCmdBuf, 1u, 1u, 1u);
	}
	// end recording of secondary buffer
	endCommandBuffer(vk, *secCmdBuf);

	// record primary command buffers
	// Insert one instance of same secondary command buffer into two separate primary command buffers
	VK_CHECK(vk.beginCommandBuffer(*primCmdBufOne, &primCmdBufBeginInfo));
	{
		vk.cmdExecuteCommands(*primCmdBufOne, 1, &secCmdBuf.get());
	}
	endCommandBuffer(vk, *primCmdBufOne);

	VK_CHECK(vk.beginCommandBuffer(*primCmdBufTwo, &primCmdBufBeginInfo));
	{
		vk.cmdExecuteCommands(*primCmdBufTwo, 1, &secCmdBuf.get());
	}
	endCommandBuffer(vk, *primCmdBufTwo);

	// create fence to wait for execution of queue
	const Unique<VkFence>					fence(createFence(vk, vkDevice));

	const VkSubmitInfo						submitInfo =
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,								// sType
		DE_NULL,													// pNext
		0u,															// waitSemaphoreCount
		DE_NULL,													// pWaitSemaphores
		(const VkPipelineStageFlags*)DE_NULL,						// pWaitDstStageMask
		numPrimCmdBufs,												// commandBufferCount
		primCmdBufs,												// pCommandBuffers
		0u,															// signalSemaphoreCount
		DE_NULL,													// pSignalSemaphores
	};

	// submit primary buffers, the secondary should be executed too
	VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo, *fence));

	// wait for end of execution of queue
	VK_CHECK(vk.waitForFences(vkDevice, 1, &fence.get(), 0u, INFINITE_TIMEOUT));

	deUint32 resultCount;
	result.readResultContentsTo(&resultCount);
	// check if secondary buffer has been executed
	if (resultCount == 2)
		return tcu::TestStatus::pass("Simultaneous Secondary Command Buffer Execution succeeded");
	else
		return tcu::TestStatus::fail("Simultaneous Secondary Command Buffer Execution FAILED");
}

tcu::TestStatus recordBufferQueryPreciseWithFlagTest(Context& context)
{
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	if (!context.getDeviceFeatures().inheritedQueries)
		TCU_THROW(NotSupportedError, "Inherited queries feature is not supported");

	const VkCommandPoolCreateInfo			cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					// sType;
		DE_NULL,													// pNext;
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,			// flags;
		queueFamilyIndex,											// queueFamilyIndex;
	};
	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCommandBufferAllocateInfo		primCmdBufParams		=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				// sType;
		DE_NULL,													// pNext;
		*cmdPool,													// pool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,							// level;
		1u,															// flags;
	};
	const Unique<VkCommandBuffer>			primCmdBuf				(allocateCommandBuffer(vk, vkDevice, &primCmdBufParams));

	// Secondary Command buffer params
	const VkCommandBufferAllocateInfo		secCmdBufParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				// sType;
		DE_NULL,													// pNext;
		*cmdPool,													// pool;
		VK_COMMAND_BUFFER_LEVEL_SECONDARY,							// level;
		1u,															// flags;
	};
	const Unique<VkCommandBuffer>			secCmdBuf				(allocateCommandBuffer(vk, vkDevice, &secCmdBufParams));

	const VkCommandBufferBeginInfo			primBufferBeginInfo		=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,				// sType
		DE_NULL,													// pNext
		0u,															// flags
		(const VkCommandBufferInheritanceInfo*)DE_NULL,
	};

	const VkCommandBufferInheritanceInfo	secBufferInheritInfo	=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
		DE_NULL,
		0u,															// renderPass
		0u,															// subpass
		0u,															// framebuffer
		VK_TRUE,													// occlusionQueryEnable
		VK_QUERY_CONTROL_PRECISE_BIT,								// queryFlags
		(VkQueryPipelineStatisticFlags)0u,							// pipelineStatistics
	};
	const VkCommandBufferBeginInfo			secBufferBeginInfo		=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,				// sType
		DE_NULL,													// pNext
		0u,															// flags
		&secBufferInheritInfo,
	};

	const VkQueryPoolCreateInfo				queryPoolCreateInfo		=
	{
		VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,					// sType
		DE_NULL,													// pNext
		(VkQueryPoolCreateFlags)0,									// flags
		VK_QUERY_TYPE_OCCLUSION,									// queryType
		1u,															// entryCount
		0u,															// pipelineStatistics
	};
	Unique<VkQueryPool>						queryPool				(createQueryPool(vk, vkDevice, &queryPoolCreateInfo));

	VK_CHECK(vk.beginCommandBuffer(secCmdBuf.get(), &secBufferBeginInfo));
	endCommandBuffer(vk, secCmdBuf.get());

	VK_CHECK(vk.beginCommandBuffer(primCmdBuf.get(), &primBufferBeginInfo));
	{
		vk.cmdResetQueryPool(primCmdBuf.get(), queryPool.get(), 0u, 1u);
		vk.cmdBeginQuery(primCmdBuf.get(), queryPool.get(), 0u, VK_QUERY_CONTROL_PRECISE_BIT);
		{
			vk.cmdExecuteCommands(primCmdBuf.get(), 1u, &secCmdBuf.get());
		}
		vk.cmdEndQuery(primCmdBuf.get(), queryPool.get(), 0u);
	}
	endCommandBuffer(vk, primCmdBuf.get());

	return tcu::TestStatus::pass("Successfully recorded a secondary command buffer allowing a precise occlusion query.");
}

tcu::TestStatus recordBufferQueryImpreciseWithFlagTest(Context& context)
{
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	if (!context.getDeviceFeatures().inheritedQueries)
		TCU_THROW(NotSupportedError, "Inherited queries feature is not supported");

	const VkCommandPoolCreateInfo			cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					// sType;
		DE_NULL,													// pNext;
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,			// flags;
		queueFamilyIndex,											// queueFamilyIndex;
	};
	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCommandBufferAllocateInfo		primCmdBufParams		=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				// sType;
		DE_NULL,													// pNext;
		*cmdPool,													// pool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,							// level;
		1u,															// flags;
	};
	const Unique<VkCommandBuffer>			primCmdBuf				(allocateCommandBuffer(vk, vkDevice, &primCmdBufParams));

	// Secondary Command buffer params
	const VkCommandBufferAllocateInfo		secCmdBufParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				// sType;
		DE_NULL,													// pNext;
		*cmdPool,													// pool;
		VK_COMMAND_BUFFER_LEVEL_SECONDARY,							// level;
		1u,															// flags;
	};
	const Unique<VkCommandBuffer>			secCmdBuf				(allocateCommandBuffer(vk, vkDevice, &secCmdBufParams));

	const VkCommandBufferBeginInfo			primBufferBeginInfo		=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,				// sType
		DE_NULL,													// pNext
		0u,															// flags
		(const VkCommandBufferInheritanceInfo*)DE_NULL,
	};

	const VkCommandBufferInheritanceInfo	secBufferInheritInfo	=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
		DE_NULL,
		0u,															// renderPass
		0u,															// subpass
		0u,															// framebuffer
		VK_TRUE,													// occlusionQueryEnable
		VK_QUERY_CONTROL_PRECISE_BIT,								// queryFlags
		(VkQueryPipelineStatisticFlags)0u,							// pipelineStatistics
	};
	const VkCommandBufferBeginInfo			secBufferBeginInfo		=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,				// sType
		DE_NULL,													// pNext
		0u,															// flags
		&secBufferInheritInfo,
	};

	// Create an occlusion query with VK_QUERY_CONTROL_PRECISE_BIT set
	const VkQueryPoolCreateInfo				queryPoolCreateInfo		=
	{
		VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,					// sType
		DE_NULL,													// pNext
		0u,															// flags
		VK_QUERY_TYPE_OCCLUSION,									// queryType
		1u,															// entryCount
		0u,															// pipelineStatistics
	};
	Unique<VkQueryPool>						queryPool				(createQueryPool(vk, vkDevice, &queryPoolCreateInfo));

	VK_CHECK(vk.beginCommandBuffer(secCmdBuf.get(), &secBufferBeginInfo));
	endCommandBuffer(vk, secCmdBuf.get());

	VK_CHECK(vk.beginCommandBuffer(primCmdBuf.get(), &primBufferBeginInfo));
	{
		vk.cmdResetQueryPool(primCmdBuf.get(), queryPool.get(), 0u, 1u);
		vk.cmdBeginQuery(primCmdBuf.get(), queryPool.get(), 0u, VK_QUERY_CONTROL_PRECISE_BIT);
		{
			vk.cmdExecuteCommands(primCmdBuf.get(), 1u, &secCmdBuf.get());
		}
		vk.cmdEndQuery(primCmdBuf.get(), queryPool.get(), 0u);
	}
	endCommandBuffer(vk, primCmdBuf.get());

	return tcu::TestStatus::pass("Successfully recorded a secondary command buffer allowing a precise occlusion query.");
}

tcu::TestStatus recordBufferQueryImpreciseWithoutFlagTest(Context& context)
{
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	if (!context.getDeviceFeatures().inheritedQueries)
		TCU_THROW(NotSupportedError, "Inherited queries feature is not supported");

	const VkCommandPoolCreateInfo			cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					// sType;
		DE_NULL,													// pNext;
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,			// flags;
		queueFamilyIndex,											// queueFamilyIndex;
	};
	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCommandBufferAllocateInfo		primCmdBufParams		=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				// sType;
		DE_NULL,													// pNext;
		*cmdPool,													// pool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,							// level;
		1u,															// flags;
	};
	const Unique<VkCommandBuffer>			primCmdBuf				(allocateCommandBuffer(vk, vkDevice, &primCmdBufParams));

	// Secondary Command buffer params
	const VkCommandBufferAllocateInfo		secCmdBufParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				// sType;
		DE_NULL,													// pNext;
		*cmdPool,													// pool;
		VK_COMMAND_BUFFER_LEVEL_SECONDARY,							// level;
		1u,															// flags;
	};
	const Unique<VkCommandBuffer>			secCmdBuf				(allocateCommandBuffer(vk, vkDevice, &secCmdBufParams));

	const VkCommandBufferBeginInfo			primBufferBeginInfo		=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,				// sType
		DE_NULL,													// pNext
		0u,															// flags
		(const VkCommandBufferInheritanceInfo*)DE_NULL,
	};

	const VkCommandBufferInheritanceInfo	secBufferInheritInfo	=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
		DE_NULL,
		0u,															// renderPass
		0u,															// subpass
		0u,															// framebuffer
		VK_TRUE,													// occlusionQueryEnable
		0u,															// queryFlags
		(VkQueryPipelineStatisticFlags)0u,							// pipelineStatistics
	};
	const VkCommandBufferBeginInfo			secBufferBeginInfo		=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,				// sType
		DE_NULL,													// pNext
		0u,															// flags
		&secBufferInheritInfo,
	};

	// Create an occlusion query with VK_QUERY_CONTROL_PRECISE_BIT set
	const VkQueryPoolCreateInfo				queryPoolCreateInfo		=
	{
		VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,					// sType
		DE_NULL,													// pNext
		(VkQueryPoolCreateFlags)0,
		VK_QUERY_TYPE_OCCLUSION,
		1u,
		0u,
	};
	Unique<VkQueryPool>						queryPool				(createQueryPool(vk, vkDevice, &queryPoolCreateInfo));

	VK_CHECK(vk.beginCommandBuffer(secCmdBuf.get(), &secBufferBeginInfo));
	endCommandBuffer(vk, secCmdBuf.get());

	VK_CHECK(vk.beginCommandBuffer(primCmdBuf.get(), &primBufferBeginInfo));
	{
		vk.cmdResetQueryPool(primCmdBuf.get(), queryPool.get(), 0u, 1u);
		vk.cmdBeginQuery(primCmdBuf.get(), queryPool.get(), 0u, VK_QUERY_CONTROL_PRECISE_BIT);
		{
			vk.cmdExecuteCommands(primCmdBuf.get(), 1u, &secCmdBuf.get());
		}
		vk.cmdEndQuery(primCmdBuf.get(), queryPool.get(), 0u);
	}
	endCommandBuffer(vk, primCmdBuf.get());

	return tcu::TestStatus::pass("Successfully recorded a secondary command buffer allowing a precise occlusion query.");
}

/******** 19.4. Command Buffer Submission (5.4 in VK 1.0 Spec) ****************/
tcu::TestStatus submitBufferCountNonZero(Context& context)
{
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkQueue							queue					= context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	const deUint32							BUFFER_COUNT			= 5u;

	const VkCommandPoolCreateInfo			cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					// sType;
		DE_NULL,													// pNext;
		0u,															// flags;
		queueFamilyIndex,											// queueFamilyIndex;
	};
	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCommandBufferAllocateInfo		cmdBufParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				// sType;
		DE_NULL,													// pNext;
		*cmdPool,													// pool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,							// level;
		BUFFER_COUNT,												// bufferCount;
	};
	VkCommandBuffer cmdBuffers[BUFFER_COUNT];
	VK_CHECK(vk.allocateCommandBuffers(vkDevice, &cmdBufParams, cmdBuffers));

	const VkCommandBufferBeginInfo			cmdBufBeginInfo			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,				// sType
		DE_NULL,													// pNext
		0u,															// flags
		(const VkCommandBufferInheritanceInfo*)DE_NULL,
	};

	std::vector<VkEventSp>					events;
	for (deUint32 ndx = 0; ndx < BUFFER_COUNT; ++ndx)
	{
		events.push_back(VkEventSp(new vk::Unique<VkEvent>(createEvent(vk, vkDevice))));
	}

	// Record the command buffers
	for (deUint32 ndx = 0; ndx < BUFFER_COUNT; ++ndx)
	{
		VK_CHECK(vk.beginCommandBuffer(cmdBuffers[ndx], &cmdBufBeginInfo));
		{
			vk.cmdSetEvent(cmdBuffers[ndx], events[ndx]->get(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
		}
		endCommandBuffer(vk, cmdBuffers[ndx]);
	}

	// We'll use a fence to wait for the execution of the queue
	const Unique<VkFence>					fence					(createFence(vk, vkDevice));

	const VkSubmitInfo						submitInfo				=
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,								// sType
		DE_NULL,													// pNext
		0u,															// waitSemaphoreCount
		DE_NULL,													// pWaitSemaphores
		(const VkPipelineStageFlags*)DE_NULL,						// pWaitDstStageMask
		BUFFER_COUNT,												// commandBufferCount
		cmdBuffers,													// pCommandBuffers
		0u,															// signalSemaphoreCount
		DE_NULL,													// pSignalSemaphores
	};

	// Submit the alpha command buffer to the queue
	VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo, fence.get()));
	// Wait for the queue
	VK_CHECK(vk.waitForFences(vkDevice, 1u, &fence.get(), VK_TRUE, INFINITE_TIMEOUT));

	// Check if the buffers were executed
	tcu::TestStatus testResult = tcu::TestStatus::incomplete();

	for (deUint32 ndx = 0; ndx < BUFFER_COUNT; ++ndx)
	{
		if (vk.getEventStatus(vkDevice, events[ndx]->get()) != VK_EVENT_SET)
		{
			testResult = tcu::TestStatus::fail("Failed to set the event.");
			break;
		}
	}

	if (!testResult.isComplete())
		testResult = tcu::TestStatus::pass("All buffers were submitted and executed correctly.");

	return testResult;
}

tcu::TestStatus submitBufferCountEqualZero(Context& context)
{
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkQueue							queue					= context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	const deUint32							BUFFER_COUNT			= 2u;

	const VkCommandPoolCreateInfo			cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					// sType;
		DE_NULL,													// pNext;
		0u,															// flags;
		queueFamilyIndex,											// queueFamilyIndex;
	};
	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCommandBufferAllocateInfo		cmdBufParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				// sType;
		DE_NULL,													// pNext;
		*cmdPool,													// pool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,							// level;
		BUFFER_COUNT,												// bufferCount;
	};
	VkCommandBuffer cmdBuffers[BUFFER_COUNT];
	VK_CHECK(vk.allocateCommandBuffers(vkDevice, &cmdBufParams, cmdBuffers));

	const VkCommandBufferBeginInfo			cmdBufBeginInfo			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,				// sType
		DE_NULL,													// pNext
		0u,															// flags
		(const VkCommandBufferInheritanceInfo*)DE_NULL,
	};

	std::vector<VkEventSp>					events;
	for (deUint32 ndx = 0; ndx < BUFFER_COUNT; ++ndx)
		events.push_back(VkEventSp(new vk::Unique<VkEvent>(createEvent(vk, vkDevice))));

	// Record the command buffers
	for (deUint32 ndx = 0; ndx < BUFFER_COUNT; ++ndx)
	{
		VK_CHECK(vk.beginCommandBuffer(cmdBuffers[ndx], &cmdBufBeginInfo));
		{
			vk.cmdSetEvent(cmdBuffers[ndx], events[ndx]->get(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
		}
		endCommandBuffer(vk, cmdBuffers[ndx]);
	}

	// We'll use a fence to wait for the execution of the queue
	const Unique<VkFence>					fenceZero				(createFence(vk, vkDevice));
	const Unique<VkFence>					fenceOne				(createFence(vk, vkDevice));

	const VkSubmitInfo						submitInfoCountZero		=
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,								// sType
		DE_NULL,													// pNext
		0u,															// waitSemaphoreCount
		DE_NULL,													// pWaitSemaphores
		(const VkPipelineStageFlags*)DE_NULL,						// pWaitDstStageMask
		1u,															// commandBufferCount
		&cmdBuffers[0],												// pCommandBuffers
		0u,															// signalSemaphoreCount
		DE_NULL,													// pSignalSemaphores
	};

	const VkSubmitInfo						submitInfoCountOne		=
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,								// sType
		DE_NULL,													// pNext
		0u,															// waitSemaphoreCount
		DE_NULL,													// pWaitSemaphores
		(const VkPipelineStageFlags*)DE_NULL,						// pWaitDstStageMask
		1u,															// commandBufferCount
		&cmdBuffers[1],												// pCommandBuffers
		0u,															// signalSemaphoreCount
		DE_NULL,													// pSignalSemaphores
	};

	// Submit the command buffers to the queue
	// We're performing two submits to make sure that the first one has
	// a chance to be processed before we check the event's status
	VK_CHECK(vk.queueSubmit(queue, 0, &submitInfoCountZero, fenceZero.get()));
	VK_CHECK(vk.queueSubmit(queue, 1, &submitInfoCountOne, fenceOne.get()));

	const VkFence							fences[]				=
	{
		fenceZero.get(),
		fenceOne.get(),
	};

	// Wait for the queue
	VK_CHECK(vk.waitForFences(vkDevice, (deUint32)DE_LENGTH_OF_ARRAY(fences), fences, VK_TRUE, INFINITE_TIMEOUT));

	// Check if the first buffer was executed
	tcu::TestStatus testResult = tcu::TestStatus::incomplete();

	if (vk.getEventStatus(vkDevice, events[0]->get()) == VK_EVENT_SET)
		testResult = tcu::TestStatus::fail("The first event was signaled.");
	else
		testResult = tcu::TestStatus::pass("The first submission was ignored.");

	return testResult;
}

tcu::TestStatus submitBufferWaitSingleSemaphore(Context& context)
{
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkQueue							queue					= context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	const VkCommandPoolCreateInfo			cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					// VkStructureType				sType;
		DE_NULL,													// const void*					pNext;
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,			// VkCommandPoolCreateFlags		flags;
		queueFamilyIndex,											// deUint32						queueFamilyIndex;
	};
	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCommandBufferAllocateInfo		cmdBufParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				// VkStructureType				sType;
		DE_NULL,													// const void*					pNext;
		*cmdPool,													// VkCommandPool				pool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,							// VkCommandBufferLevel			level;
		1u,															// uint32_t						bufferCount;
	};

	// Create two command buffers
	const Unique<VkCommandBuffer>			primCmdBuf1				(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));
	const Unique<VkCommandBuffer>			primCmdBuf2				(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

	const VkCommandBufferBeginInfo			primCmdBufBeginInfo		=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,				// sType
		DE_NULL,													// pNext
		0,															// flags
		DE_NULL														// const VkCommandBufferInheritanceInfo*	pInheritanceInfo;
	};

	// create two events that will be used to check if command buffers has been executed
	const Unique<VkEvent>					event1					(createEvent(vk, vkDevice));
	const Unique<VkEvent>					event2					(createEvent(vk, vkDevice));

	// reset events
	VK_CHECK(vk.resetEvent(vkDevice, *event1));
	VK_CHECK(vk.resetEvent(vkDevice, *event2));

	// record first command buffer
	VK_CHECK(vk.beginCommandBuffer(*primCmdBuf1, &primCmdBufBeginInfo));
	{
		// allow execution of event during every stage of pipeline
		VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

		// record setting event
		vk.cmdSetEvent(*primCmdBuf1, *event1,stageMask);
	}
	endCommandBuffer(vk, *primCmdBuf1);

	// record second command buffer
	VK_CHECK(vk.beginCommandBuffer(*primCmdBuf2, &primCmdBufBeginInfo));
	{
		// allow execution of event during every stage of pipeline
		VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

		// record setting event
		vk.cmdSetEvent(*primCmdBuf2, *event2,stageMask);
	}
	endCommandBuffer(vk, *primCmdBuf2);

	// create fence to wait for execution of queue
	const Unique<VkFence>					fence					(createFence(vk, vkDevice));

	// create semaphore for use in this test
	const Unique <VkSemaphore>				semaphore				(createSemaphore(vk, vkDevice));

	// create submit info for first buffer - signalling semaphore
	const VkSubmitInfo						submitInfo1				=
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,								// sType
		DE_NULL,													// pNext
		0u,															// waitSemaphoreCount
		DE_NULL,													// pWaitSemaphores
		DE_NULL,													// pWaitDstStageMask
		1,															// commandBufferCount
		&primCmdBuf1.get(),											// pCommandBuffers
		1u,															// signalSemaphoreCount
		&semaphore.get(),											// pSignalSemaphores
	};

	// Submit the command buffer to the queue
	VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo1, *fence));

	// wait for end of execution of queue
	VK_CHECK(vk.waitForFences(vkDevice, 1, &fence.get(), 0u, INFINITE_TIMEOUT));

	// check if buffer has been executed
	VkResult result = vk.getEventStatus(vkDevice,*event1);
	if (result != VK_EVENT_SET)
		return tcu::TestStatus::fail("Submit Buffer and Wait for Single Semaphore Test FAILED");

	const VkPipelineStageFlags				waitDstStageFlags		= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

	// create submit info for second buffer - waiting for semaphore
	const VkSubmitInfo						submitInfo2				=
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,								// sType
		DE_NULL,													// pNext
		1u,															// waitSemaphoreCount
		&semaphore.get(),											// pWaitSemaphores
		&waitDstStageFlags,											// pWaitDstStageMask
		1,															// commandBufferCount
		&primCmdBuf2.get(),											// pCommandBuffers
		0u,															// signalSemaphoreCount
		DE_NULL,													// pSignalSemaphores
	};

	// reset fence, so it can be used again
	VK_CHECK(vk.resetFences(vkDevice, 1u, &fence.get()));

	// Submit the second command buffer to the queue
	VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo2, *fence));

	// wait for end of execution of queue
	VK_CHECK(vk.waitForFences(vkDevice, 1, &fence.get(), 0u, INFINITE_TIMEOUT));

	// check if second buffer has been executed
	// if it has been executed, it means that the semaphore was signalled - so test if passed
	result = vk.getEventStatus(vkDevice,*event1);
	if (result != VK_EVENT_SET)
		return tcu::TestStatus::fail("Submit Buffer and Wait for Single Semaphore Test FAILED");

	return tcu::TestStatus::pass("Submit Buffer and Wait for Single Semaphore Test succeeded");
}

tcu::TestStatus submitBufferWaitManySemaphores(Context& context)
{
	// This test will create numSemaphores semaphores, and signal them in NUM_SEMAPHORES submits to queue
	// After that the numSubmissions queue submissions will wait for each semaphore

	const deUint32							numSemaphores			= 10u;  // it must be multiply of numSubmission
	const deUint32							numSubmissions			= 2u;
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkQueue							queue					= context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	const VkCommandPoolCreateInfo			cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					// VkStructureType				sType;
		DE_NULL,													// const void*					pNext;
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,			// VkCommandPoolCreateFlags		flags;
		queueFamilyIndex,											// deUint32						queueFamilyIndex;
	};
	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCommandBufferAllocateInfo		cmdBufParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				// VkStructureType				sType;
		DE_NULL,													// const void*					pNext;
		*cmdPool,													// VkCommandPool				pool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,							// VkCommandBufferLevel			level;
		1u,															// uint32_t						bufferCount;
	};

	// Create command buffer
	const Unique<VkCommandBuffer>			primCmdBuf				(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

	const VkCommandBufferBeginInfo			primCmdBufBeginInfo		=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,				// sType
		DE_NULL,													// pNext
		0,															// flags
		DE_NULL														// const VkCommandBufferInheritanceInfo*	pInheritanceInfo;
	};

	// create event that will be used to check if command buffers has been executed
	const Unique<VkEvent>					event					(createEvent(vk, vkDevice));

	// reset event - at creation state is undefined
	VK_CHECK(vk.resetEvent(vkDevice, *event));

	// record command buffer
	VK_CHECK(vk.beginCommandBuffer(*primCmdBuf, &primCmdBufBeginInfo));
	{
		// allow execution of event during every stage of pipeline
		VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

		// record setting event
		vk.cmdSetEvent(*primCmdBuf, *event,stageMask);
	}
	endCommandBuffer(vk, *primCmdBuf);

	// create fence to wait for execution of queue
	const Unique<VkFence>					fence					(createFence(vk, vkDevice));

	// numSemaphores is declared const, so this array can be static
	// the semaphores will be destroyed automatically at end of scope
	Move <VkSemaphore>						semaphoreArray[numSemaphores];
	VkSemaphore								semaphores[numSemaphores];

	for (deUint32 idx = 0; idx < numSemaphores; ++idx) {
		// create semaphores for use in this test
		semaphoreArray[idx] = createSemaphore(vk, vkDevice);
		semaphores[idx] = semaphoreArray[idx].get();
	}

	{
		// create submit info for buffer - signal semaphores
		const VkSubmitInfo submitInfo1 =
		{
			VK_STRUCTURE_TYPE_SUBMIT_INFO,							// sType
			DE_NULL,												// pNext
			0u,														// waitSemaphoreCount
			DE_NULL,												// pWaitSemaphores
			DE_NULL,												// pWaitDstStageMask
			1,														// commandBufferCount
			&primCmdBuf.get(),										// pCommandBuffers
			numSemaphores,											// signalSemaphoreCount
			semaphores												// pSignalSemaphores
		};
		// Submit the command buffer to the queue
		VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo1, *fence));

		// wait for end of execution of queue
		VK_CHECK(vk.waitForFences(vkDevice, 1, &fence.get(), 0u, INFINITE_TIMEOUT));

		// check if buffer has been executed
		VkResult result = vk.getEventStatus(vkDevice,*event);
		if (result != VK_EVENT_SET)
			return tcu::TestStatus::fail("Submit Buffer and Wait for Many Semaphores Test FAILED");

		// reset event, so next buffers can set it again
		VK_CHECK(vk.resetEvent(vkDevice, *event));

		// reset fence, so it can be used again
		VK_CHECK(vk.resetFences(vkDevice, 1u, &fence.get()));
	}

	const deUint32							numberOfSemaphoresToBeWaitedByOneSubmission	= numSemaphores / numSubmissions;
	const std::vector<VkPipelineStageFlags>	waitDstStageFlags							(numberOfSemaphoresToBeWaitedByOneSubmission, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

	// the following code waits for the semaphores set above - numSubmissions queues will wait for each semaphore from above
	for (deUint32 idxSubmission = 0; idxSubmission < numSubmissions; ++idxSubmission) {

		// create submit info for buffer - waiting for semaphore
		const VkSubmitInfo				submitInfo2				=
		{
			VK_STRUCTURE_TYPE_SUBMIT_INFO,												// sType
			DE_NULL,																	// pNext
			numberOfSemaphoresToBeWaitedByOneSubmission,								// waitSemaphoreCount
			semaphores + (numberOfSemaphoresToBeWaitedByOneSubmission * idxSubmission),	// pWaitSemaphores
			waitDstStageFlags.data(),													// pWaitDstStageMask
			1,																			// commandBufferCount
			&primCmdBuf.get(),															// pCommandBuffers
			0u,																			// signalSemaphoreCount
			DE_NULL,																	// pSignalSemaphores
		};

		// Submit the second command buffer to the queue
		VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo2, *fence));

		// wait for 1 second.
		VK_CHECK(vk.waitForFences(vkDevice, 1, &fence.get(), 0u, 1000 * 1000 * 1000));

		// check if second buffer has been executed
		// if it has been executed, it means that the semaphore was signalled - so test if passed
		VkResult result = vk.getEventStatus(vkDevice,*event);
		if (result != VK_EVENT_SET)
			return tcu::TestStatus::fail("Submit Buffer and Wait for Many Semaphores Test FAILED");

		// reset fence, so it can be used again
		VK_CHECK(vk.resetFences(vkDevice, 1u, &fence.get()));

		// reset event, so next buffers can set it again
		VK_CHECK(vk.resetEvent(vkDevice, *event));
	}

	return tcu::TestStatus::pass("Submit Buffer and Wait for Many Semaphores Test succeeded");
}

tcu::TestStatus submitBufferNullFence(Context& context)
{
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkQueue							queue					= context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	const short								BUFFER_COUNT			= 2;

	const VkCommandPoolCreateInfo			cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					// sType;
		DE_NULL,													// pNext;
		0u,															// flags;
		queueFamilyIndex,											// queueFamilyIndex;
	};
	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCommandBufferAllocateInfo		cmdBufParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				// sType;
		DE_NULL,													// pNext;
		*cmdPool,													// pool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,							// level;
		1u,															// bufferCount;
	};
	VkCommandBuffer cmdBuffers[BUFFER_COUNT];
	for (deUint32 ndx = 0; ndx < BUFFER_COUNT; ++ndx)
		VK_CHECK(vk.allocateCommandBuffers(vkDevice, &cmdBufParams, &cmdBuffers[ndx]));

	const VkCommandBufferBeginInfo			cmdBufBeginInfo			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,				// sType
		DE_NULL,													// pNext
		0u,															// flags
		(const VkCommandBufferInheritanceInfo*)DE_NULL,
	};

	std::vector<VkEventSp>					events;
	for (deUint32 ndx = 0; ndx < BUFFER_COUNT; ++ndx)
		events.push_back(VkEventSp(new vk::Unique<VkEvent>(createEvent(vk, vkDevice))));

	// Record the command buffers
	for (deUint32 ndx = 0; ndx < BUFFER_COUNT; ++ndx)
	{
		VK_CHECK(vk.beginCommandBuffer(cmdBuffers[ndx], &cmdBufBeginInfo));
		{
			vk.cmdSetEvent(cmdBuffers[ndx], events[ndx]->get(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
		}
		endCommandBuffer(vk, cmdBuffers[ndx]);
	}

	// We'll use a fence to wait for the execution of the queue
	const Unique<VkFence>					fence					(createFence(vk, vkDevice));

	const VkSubmitInfo						submitInfoNullFence		=
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,								// sType
		DE_NULL,													// pNext
		0u,															// waitSemaphoreCount
		DE_NULL,													// pWaitSemaphores
		(const VkPipelineStageFlags*)DE_NULL,						// pWaitDstStageMask
		1u,															// commandBufferCount
		&cmdBuffers[0],												// pCommandBuffers
		0u,															// signalSemaphoreCount
		DE_NULL,													// pSignalSemaphores
	};

	const VkSubmitInfo						submitInfoNonNullFence	=
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,								// sType
		DE_NULL,													// pNext
		0u,															// waitSemaphoreCount
		DE_NULL,													// pWaitSemaphores
		(const VkPipelineStageFlags*)DE_NULL,						// pWaitDstStageMask
		1u,															// commandBufferCount
		&cmdBuffers[1],												// pCommandBuffers
		0u,															// signalSemaphoreCount
		DE_NULL,													// pSignalSemaphores
	};

	// Perform two submissions - one with no fence, the other one with a valid
	// fence Hoping submitting the other buffer will give the first one time to
	// execute
	VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfoNullFence, DE_NULL));
	VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfoNonNullFence, fence.get()));

	// Wait for the queue
	VK_CHECK(vk.waitForFences(vkDevice, 1u, &fence.get(), VK_TRUE, INFINITE_TIMEOUT));


	tcu::TestStatus testResult = tcu::TestStatus::incomplete();

	//Fence guaranteed that all buffers submited before fence were executed
	if (vk.getEventStatus(vkDevice, events[0]->get()) != VK_EVENT_SET || vk.getEventStatus(vkDevice, events[1]->get()) != VK_EVENT_SET)
	{
		testResult = tcu::TestStatus::fail("One of the buffers was not executed.");
	}
	else
	{
		testResult = tcu::TestStatus::pass("Buffers have been submitted and executed correctly.");
	}

	vk.queueWaitIdle(queue);
	return testResult;
}

tcu::TestStatus submitTwoBuffersOneBufferNullWithFence(Context& context)
{
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkQueue							queue					= context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();
	const deUint32							BUFFER_COUNT			= 2u;

	const VkCommandPoolCreateInfo			cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,			// sType;
		DE_NULL,											// pNext;
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,	// flags;
		queueFamilyIndex,									// queueFamilyIndex;
	};
	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	const VkCommandBufferAllocateInfo		cmdBufParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,	// sType;
		DE_NULL,										// pNext;
		*cmdPool,										// pool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,				// level;
		BUFFER_COUNT,									// bufferCount;
	};

	VkCommandBuffer							cmdBuffers[BUFFER_COUNT];
	VK_CHECK(vk.allocateCommandBuffers(vkDevice, &cmdBufParams, cmdBuffers));

	const VkCommandBufferBeginInfo			cmdBufBeginInfo			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	// sType
		DE_NULL,										// pNext
		0u,												// flags
		(const VkCommandBufferInheritanceInfo*)DE_NULL,	// pInheritanceInfo
	};

	std::vector<VkEventSp>					events;
	for (deUint32 ndx = 0; ndx < BUFFER_COUNT; ++ndx)
		events.push_back(VkEventSp(new vk::Unique<VkEvent>(createEvent(vk, vkDevice))));

	// Record the command buffers
	for (deUint32 ndx = 0; ndx < BUFFER_COUNT; ++ndx)
	{
		VK_CHECK(vk.beginCommandBuffer(cmdBuffers[ndx], &cmdBufBeginInfo));
		{
			vk.cmdSetEvent(cmdBuffers[ndx], events[ndx]->get(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
		}
		VK_CHECK(vk.endCommandBuffer(cmdBuffers[ndx]));
	}

	// First command buffer
	const VkSubmitInfo						submitInfoNonNullFirst	=
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,				// sType
		DE_NULL,									// pNext
		0u,											// waitSemaphoreCount
		DE_NULL,									// pWaitSemaphores
		(const VkPipelineStageFlags*)DE_NULL,		// pWaitDstStageMask
		1u,											// commandBufferCount
		&cmdBuffers[0],								// pCommandBuffers
		0u,											// signalSemaphoreCount
		DE_NULL,									// pSignalSemaphores
	};

	// Second command buffer
	const VkSubmitInfo						submitInfoNonNullSecond	=
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,				// sType
		DE_NULL,									// pNext
		0u,											// waitSemaphoreCount
		DE_NULL,									// pWaitSemaphores
		(const VkPipelineStageFlags*)DE_NULL,		// pWaitDstStageMask
		1u,											// commandBufferCount
		&cmdBuffers[1],								// pCommandBuffers
		0u,											// signalSemaphoreCount
		DE_NULL,									// pSignalSemaphores
	};

	// Fence will be submitted with the null queue
	const Unique<VkFence>					fence					(createFence(vk, vkDevice));

	// Perform two separate queueSubmit calls on the same queue followed
	// by a third call with no submitInfos and with a valid fence
	VK_CHECK(vk.queueSubmit(queue,	1u,	&submitInfoNonNullFirst,	DE_NULL));
	VK_CHECK(vk.queueSubmit(queue,	1u,	&submitInfoNonNullSecond,	DE_NULL));
	VK_CHECK(vk.queueSubmit(queue,	0u,	DE_NULL,					fence.get()));

	// Wait for the queue
	VK_CHECK(vk.waitForFences(vkDevice, 1u, &fence.get(), VK_TRUE, INFINITE_TIMEOUT));

	return tcu::TestStatus::pass("Buffers have been submitted correctly");
}

/******** 19.5. Secondary Command Buffer Execution (5.6 in VK 1.0 Spec) *******/
tcu::TestStatus executeSecondaryBufferTest(Context& context)
{
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkQueue							queue					= context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	const VkCommandPoolCreateInfo			cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					// sType;
		DE_NULL,													// pNext;
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,			// flags;
		queueFamilyIndex,											// queueFamilyIndex;
	};
	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCommandBufferAllocateInfo		cmdBufParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				// sType;
		DE_NULL,													// pNext;
		*cmdPool,													// commandPool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,							// level;
		1u,															// bufferCount;
	};
	const Unique<VkCommandBuffer>			primCmdBuf				(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

	// Secondary Command buffer
	const VkCommandBufferAllocateInfo		secCmdBufParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				// sType;
		DE_NULL,													// pNext;
		*cmdPool,													// commandPool;
		VK_COMMAND_BUFFER_LEVEL_SECONDARY,							// level;
		1u,															// bufferCount;
	};
	const Unique<VkCommandBuffer>			secCmdBuf				(allocateCommandBuffer(vk, vkDevice, &secCmdBufParams));

	const VkCommandBufferBeginInfo			primCmdBufBeginInfo		=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,				// sType
		DE_NULL,													// pNext
		0u,															// flags
		(const VkCommandBufferInheritanceInfo*)DE_NULL,
	};

	const VkCommandBufferInheritanceInfo	secCmdBufInheritInfo	=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
		DE_NULL,
		DE_NULL,													// renderPass
		0u,															// subpass
		DE_NULL,													// framebuffer
		VK_FALSE,													// occlusionQueryEnable
		(VkQueryControlFlags)0u,									// queryFlags
		(VkQueryPipelineStatisticFlags)0u,							// pipelineStatistics
	};
	const VkCommandBufferBeginInfo			secCmdBufBeginInfo		=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,				// sType
		DE_NULL,													// pNext
		0u,															// flags
		&secCmdBufInheritInfo,
	};

	// create event that will be used to check if secondary command buffer has been executed
	const Unique<VkEvent>					event					(createEvent(vk, vkDevice));

	// reset event
	VK_CHECK(vk.resetEvent(vkDevice, *event));

	// record secondary command buffer
	VK_CHECK(vk.beginCommandBuffer(*secCmdBuf, &secCmdBufBeginInfo));
	{
		// allow execution of event during every stage of pipeline
		VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		// record setting event
		vk.cmdSetEvent(*secCmdBuf, *event, stageMask);
	}
	// end recording of the secondary buffer
	endCommandBuffer(vk, *secCmdBuf);

	// record primary command buffer
	VK_CHECK(vk.beginCommandBuffer(*primCmdBuf, &primCmdBufBeginInfo));
	{
		// execute secondary buffer
		vk.cmdExecuteCommands(*primCmdBuf, 1u, &secCmdBuf.get());
	}
	endCommandBuffer(vk, *primCmdBuf);

	submitCommandsAndWait(vk, vkDevice, queue, primCmdBuf.get());

	// check if secondary buffer has been executed
	VkResult result = vk.getEventStatus(vkDevice, *event);
	if (result == VK_EVENT_SET)
		return tcu::TestStatus::pass("executeSecondaryBufferTest succeeded");

	return tcu::TestStatus::fail("executeSecondaryBufferTest FAILED");
}

tcu::TestStatus executeSecondaryBufferTwiceTest(Context& context)
{
	const deUint32							BUFFER_COUNT			= 10u;
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkQueue							queue					= context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	const VkCommandPoolCreateInfo			cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					//	VkStructureType				sType;
		DE_NULL,													//	const void*					pNext;
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,			//	VkCommandPoolCreateFlags	flags;
		queueFamilyIndex,											//	deUint32					queueFamilyIndex;
	};
	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCommandBufferAllocateInfo		cmdBufParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		*cmdPool,													//	VkCommandPool				pool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,							//	VkCommandBufferLevel		level;
		1u,															//	uint32_t					bufferCount;
	};
	const Unique<VkCommandBuffer>			primCmdBufOne			(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));
	const Unique<VkCommandBuffer>			primCmdBufTwo			(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

	// Secondary Command buffers params
	const VkCommandBufferAllocateInfo		secCmdBufParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		*cmdPool,													//	VkCommandPool				pool;
		VK_COMMAND_BUFFER_LEVEL_SECONDARY,							//	VkCommandBufferLevel		level;
		BUFFER_COUNT,												//	uint32_t					bufferCount;
	};
	VkCommandBuffer cmdBuffers[BUFFER_COUNT];
	VK_CHECK(vk.allocateCommandBuffers(vkDevice, &secCmdBufParams, cmdBuffers));

	const VkCommandBufferBeginInfo			primCmdBufBeginInfo		=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		DE_NULL,
		0,															// flags
		(const VkCommandBufferInheritanceInfo*)DE_NULL,
	};

	const VkCommandBufferInheritanceInfo	secCmdBufInheritInfo	=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
		DE_NULL,
		(VkRenderPass)0u,											// renderPass
		0u,															// subpass
		(VkFramebuffer)0u,											// framebuffer
		VK_FALSE,													// occlusionQueryEnable
		(VkQueryControlFlags)0u,									// queryFlags
		(VkQueryPipelineStatisticFlags)0u,							// pipelineStatistics
	};
	const VkCommandBufferBeginInfo			secCmdBufBeginInfo		=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		DE_NULL,
		VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,				// flags
		&secCmdBufInheritInfo,
	};

	// create event that will be used to check if secondary command buffer has been executed
	const Unique<VkEvent>					eventOne				(createEvent(vk, vkDevice));

	// reset event
	VK_CHECK(vk.resetEvent(vkDevice, *eventOne));

	for (deUint32 ndx = 0; ndx < BUFFER_COUNT; ++ndx)
	{
		// record secondary command buffer
		VK_CHECK(vk.beginCommandBuffer(cmdBuffers[ndx], &secCmdBufBeginInfo));
		{
			// allow execution of event during every stage of pipeline
			VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

			// wait for event
			vk.cmdWaitEvents(cmdBuffers[ndx], 1, &eventOne.get(), stageMask, stageMask, 0, DE_NULL, 0u, DE_NULL, 0u, DE_NULL);
		}
		// end recording of secondary buffers
		endCommandBuffer(vk, cmdBuffers[ndx]);
	}

	// record primary command buffer one
	VK_CHECK(vk.beginCommandBuffer(*primCmdBufOne, &primCmdBufBeginInfo));
	{
		// execute one secondary buffer
		vk.cmdExecuteCommands(*primCmdBufOne, 1, cmdBuffers );
	}
	endCommandBuffer(vk, *primCmdBufOne);

	// record primary command buffer two
	VK_CHECK(vk.beginCommandBuffer(*primCmdBufTwo, &primCmdBufBeginInfo));
	{
		// execute one secondary buffer with all buffers
		vk.cmdExecuteCommands(*primCmdBufTwo, BUFFER_COUNT, cmdBuffers );
	}
	endCommandBuffer(vk, *primCmdBufTwo);

	// create fence to wait for execution of queue
	const Unique<VkFence>					fenceOne				(createFence(vk, vkDevice));
	const Unique<VkFence>					fenceTwo				(createFence(vk, vkDevice));

	const VkSubmitInfo						submitInfoOne			=
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,								// sType
		DE_NULL,													// pNext
		0u,															// waitSemaphoreCount
		DE_NULL,													// pWaitSemaphores
		(const VkPipelineStageFlags*)DE_NULL,						// pWaitDstStageMask
		1,															// commandBufferCount
		&primCmdBufOne.get(),										// pCommandBuffers
		0u,															// signalSemaphoreCount
		DE_NULL,													// pSignalSemaphores
	};

	// submit primary buffer, the secondary should be executed too
	VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfoOne, *fenceOne));

	// wait for buffer to stop at event for 100 microseconds
	vk.waitForFences(vkDevice, 1, &fenceOne.get(), 0u, 100000);

	const VkSubmitInfo						submitInfoTwo			=
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,								// sType
		DE_NULL,													// pNext
		0u,															// waitSemaphoreCount
		DE_NULL,													// pWaitSemaphores
		(const VkPipelineStageFlags*)DE_NULL,						// pWaitDstStageMask
		1,															// commandBufferCount
		&primCmdBufTwo.get(),										// pCommandBuffers
		0u,															// signalSemaphoreCount
		DE_NULL,													// pSignalSemaphores
	};

	// submit second primary buffer, the secondary should be executed too
	VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfoTwo, *fenceTwo));

	// wait for all buffers to stop at event for 100 microseconds
	vk.waitForFences(vkDevice, 1, &fenceOne.get(), 0u, 100000);

	// now all buffers are waiting at eventOne
	// set event eventOne
	VK_CHECK(vk.setEvent(vkDevice, *eventOne));

	// wait for end of execution of fenceOne
	VK_CHECK(vk.waitForFences(vkDevice, 1, &fenceOne.get(), 0u, INFINITE_TIMEOUT));

	// wait for end of execution of second queue
	VK_CHECK(vk.waitForFences(vkDevice, 1, &fenceTwo.get(), 0u, INFINITE_TIMEOUT));

	return tcu::TestStatus::pass("executeSecondaryBufferTwiceTest succeeded");
}

/******** 19.6. Commands Allowed Inside Command Buffers (? in VK 1.0 Spec) **/
tcu::TestStatus orderBindPipelineTest(Context& context)
{
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkDevice							device					= context.getDevice();
	const VkQueue							queue					= context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();
	Allocator&								allocator				= context.getDefaultAllocator();
	const ComputeInstanceResultBuffer		result					(vk, device, allocator);

	enum
	{
		ADDRESSABLE_SIZE = 256, // allocate a lot more than required
	};

	const tcu::Vec4							colorA1					= tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f);
	const tcu::Vec4							colorA2					= tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f);
	const tcu::Vec4							colorB1					= tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f);
	const tcu::Vec4							colorB2					= tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f);

	const deUint32							dataOffsetA				= (0u);
	const deUint32							dataOffsetB				= (0u);
	const deUint32							viewOffsetA				= (0u);
	const deUint32							viewOffsetB				= (0u);
	const deUint32							bufferSizeA				= dataOffsetA + ADDRESSABLE_SIZE;
	const deUint32							bufferSizeB				= dataOffsetB + ADDRESSABLE_SIZE;

	de::MovePtr<Allocation>					bufferMemA;
	const Unique<VkBuffer>					bufferA					(createColorDataBuffer(dataOffsetA, bufferSizeA, colorA1, colorA2, &bufferMemA, context));

	de::MovePtr<Allocation>					bufferMemB;
	const Unique<VkBuffer>					bufferB					(createColorDataBuffer(dataOffsetB, bufferSizeB, colorB1, colorB2, &bufferMemB, context));

	const Unique<VkDescriptorSetLayout>		descriptorSetLayout		(createDescriptorSetLayout(context));
	const Unique<VkDescriptorPool>			descriptorPool			(createDescriptorPool(context));
	const Unique<VkDescriptorSet>			descriptorSet			(createDescriptorSet(*descriptorPool, *descriptorSetLayout, *bufferA, viewOffsetA, *bufferB, viewOffsetB, result.getBuffer(), context));
	const VkDescriptorSet					descriptorSets[]		= { *descriptorSet };
	const int								numDescriptorSets		= DE_LENGTH_OF_ARRAY(descriptorSets);

	const VkPipelineLayoutCreateInfo layoutCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,				// sType
		DE_NULL,													// pNext
		(VkPipelineLayoutCreateFlags)0,
		numDescriptorSets,											// setLayoutCount
		&descriptorSetLayout.get(),									// pSetLayouts
		0u,															// pushConstantRangeCount
		DE_NULL,													// pPushConstantRanges
	};
	Unique<VkPipelineLayout>				pipelineLayout			(createPipelineLayout(vk, device, &layoutCreateInfo));

	const Unique<VkShaderModule>			computeModuleGood		(createShaderModule(vk, device, context.getBinaryCollection().get("compute_good"), (VkShaderModuleCreateFlags)0u));
	const Unique<VkShaderModule>			computeModuleBad		(createShaderModule(vk, device, context.getBinaryCollection().get("compute_bad"),  (VkShaderModuleCreateFlags)0u));

	const VkPipelineShaderStageCreateInfo	shaderCreateInfoGood	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		DE_NULL,
		(VkPipelineShaderStageCreateFlags)0,
		VK_SHADER_STAGE_COMPUTE_BIT,								// stage
		*computeModuleGood,											// shader
		"main",
		DE_NULL,													// pSpecializationInfo
	};

	const VkPipelineShaderStageCreateInfo	shaderCreateInfoBad	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		DE_NULL,
		(vk::VkPipelineShaderStageCreateFlags)0,
		vk::VK_SHADER_STAGE_COMPUTE_BIT,							// stage
		*computeModuleBad,											// shader
		"main",
		DE_NULL,													// pSpecializationInfo
	};

	const VkComputePipelineCreateInfo		createInfoGood			=
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		DE_NULL,
		0u,															// flags
		shaderCreateInfoGood,										// cs
		*pipelineLayout,											// layout
		(vk::VkPipeline)0,											// basePipelineHandle
		0u,															// basePipelineIndex
	};

	const VkComputePipelineCreateInfo		createInfoBad			=
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		DE_NULL,
		0u,															// flags
		shaderCreateInfoBad,										// cs
		*pipelineLayout,											// descriptorSetLayout.get()
		(VkPipeline)0,												// basePipelineHandle
		0u,															// basePipelineIndex
	};

	const Unique<VkPipeline>				pipelineGood			(createComputePipeline(vk, device, (VkPipelineCache)0u, &createInfoGood));
	const Unique<VkPipeline>				pipelineBad				(createComputePipeline(vk, device, (VkPipelineCache)0u, &createInfoBad));

	const VkAccessFlags						inputBit				= (VK_ACCESS_UNIFORM_READ_BIT);
	const VkBufferMemoryBarrier				bufferBarriers[]		=
	{
		{
			VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
			DE_NULL,
			VK_ACCESS_HOST_WRITE_BIT,									// srcAccessMask
			inputBit,													// dstAccessMask
			VK_QUEUE_FAMILY_IGNORED,									// srcQueueFamilyIndex
			VK_QUEUE_FAMILY_IGNORED,									// destQueueFamilyIndex
			*bufferA,													// buffer
			(VkDeviceSize)0u,											// offset
			(VkDeviceSize)bufferSizeA,									// size
		},
		{
			VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
			DE_NULL,
			VK_ACCESS_HOST_WRITE_BIT,									// srcAccessMask
			inputBit,													// dstAccessMask
			VK_QUEUE_FAMILY_IGNORED,									// srcQueueFamilyIndex
			VK_QUEUE_FAMILY_IGNORED,									// destQueueFamilyIndex
			*bufferB,													// buffer
			(VkDeviceSize)0u,											// offset
			(VkDeviceSize)bufferSizeB,									// size
		}
	};

	const deUint32							numSrcBuffers			= 1u;

	const deUint32* const					dynamicOffsets			= (DE_NULL);
	const deUint32							numDynamicOffsets		= (0);
	const int								numPreBarriers			= numSrcBuffers;
	const vk::VkBufferMemoryBarrier* const	postBarriers			= result.getResultReadBarrier();
	const int								numPostBarriers			= 1;
	const tcu::Vec4							refQuadrantValue14		= (colorA2);
	const tcu::Vec4							refQuadrantValue23		= (colorA1);
	const tcu::Vec4							references[4]			=
	{
		refQuadrantValue14,
		refQuadrantValue23,
		refQuadrantValue23,
		refQuadrantValue14,
	};
	tcu::Vec4								results[4];

	// submit and wait begin

	const tcu::UVec3 numWorkGroups = tcu::UVec3(4, 1u, 1);

	const VkCommandPoolCreateInfo			cmdPoolCreateInfo		=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					// sType;
		DE_NULL,													// pNext
		VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,						// flags
		queueFamilyIndex,											// queueFamilyIndex
	};
	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, device, &cmdPoolCreateInfo));
	const VkCommandBufferAllocateInfo		cmdBufCreateInfo		=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				// sType
		DE_NULL,													// pNext
		*cmdPool,													// commandPool
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,							// level
		1u,															// bufferCount;
	};

	const VkCommandBufferBeginInfo			cmdBufBeginInfo			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,				// sType
		DE_NULL,													// pNext
		0u,															// flags
		(const VkCommandBufferInheritanceInfo*)DE_NULL,
	};

	const Unique<VkCommandBuffer>			cmd						(allocateCommandBuffer(vk, device, &cmdBufCreateInfo));

	VK_CHECK(vk.beginCommandBuffer(*cmd, &cmdBufBeginInfo));

	vk.cmdBindPipeline(*cmd, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineBad);
	vk.cmdBindPipeline(*cmd, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineGood);
	vk.cmdBindDescriptorSets(*cmd, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0, numDescriptorSets, descriptorSets, numDynamicOffsets, dynamicOffsets);

	if (numPreBarriers)
		vk.cmdPipelineBarrier(*cmd, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, (VkDependencyFlags)0,
							  0, (const VkMemoryBarrier*)DE_NULL,
							  numPreBarriers, bufferBarriers,
							  0, (const VkImageMemoryBarrier*)DE_NULL);

	vk.cmdDispatch(*cmd, numWorkGroups.x(), numWorkGroups.y(), numWorkGroups.z());
	vk.cmdPipelineBarrier(*cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0,
						  0, (const VkMemoryBarrier*)DE_NULL,
						  numPostBarriers, postBarriers,
						  0, (const VkImageMemoryBarrier*)DE_NULL);
	endCommandBuffer(vk, *cmd);

	// run
	// submit second primary buffer, the secondary should be executed too
	submitCommandsAndWait(vk, device, queue, cmd.get());

	// submit and wait end
	result.readResultContentsTo(&results);

	// verify
	if (results[0] == references[0] &&
		results[1] == references[1] &&
		results[2] == references[2] &&
		results[3] == references[3])
	{
		return tcu::TestStatus::pass("Pass");
	}
	else if (results[0] == tcu::Vec4(-1.0f) &&
			 results[1] == tcu::Vec4(-1.0f) &&
			 results[2] == tcu::Vec4(-1.0f) &&
			 results[3] == tcu::Vec4(-1.0f))
	{
		context.getTestContext().getLog()
		<< tcu::TestLog::Message
		<< "Result buffer was not written to."
		<< tcu::TestLog::EndMessage;
		return tcu::TestStatus::fail("Result buffer was not written to");
	}
	else
	{
		context.getTestContext().getLog()
		<< tcu::TestLog::Message
		<< "Error expected ["
		<< references[0] << ", "
		<< references[1] << ", "
		<< references[2] << ", "
		<< references[3] << "], got ["
		<< results[0] << ", "
		<< results[1] << ", "
		<< results[2] << ", "
		<< results[3] << "]"
		<< tcu::TestLog::EndMessage;
		return tcu::TestStatus::fail("Invalid result values");
	}
}

enum StateTransitionTest
{
	STT_RECORDING_TO_INITIAL	= 0,
	STT_EXECUTABLE_TO_INITIAL,
	STT_RECORDING_TO_INVALID,
	STT_EXECUTABLE_TO_INVALID,
};

tcu::TestStatus executeStateTransitionTest(Context& context, StateTransitionTest type)
{
	const VkDevice					vkDevice			= context.getDevice();
	const DeviceInterface&			vk					= context.getDeviceInterface();
	const VkQueue					queue				= context.getUniversalQueue();
	const deUint32					queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	const Unique<VkCommandPool>		cmdPool				(createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>	cmdBuffer			(allocateCommandBuffer(vk, vkDevice, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	const Unique<VkEvent>			globalEvent			(createEvent(vk, vkDevice));

	VK_CHECK(vk.resetEvent(vkDevice, *globalEvent));

	switch (type)
	{
		case STT_RECORDING_TO_INITIAL:
		{
			beginCommandBuffer(vk, *cmdBuffer, 0u);
			vk.cmdSetEvent(*cmdBuffer, *globalEvent, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
			break;
			// command buffer is still in recording state
		}
		case STT_EXECUTABLE_TO_INITIAL:
		{
			beginCommandBuffer(vk, *cmdBuffer, 0u);
			vk.cmdSetEvent(*cmdBuffer, *globalEvent, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
			endCommandBuffer(vk, *cmdBuffer);
			break;
			// command buffer is still in executable state
		}
		case STT_RECORDING_TO_INVALID:
		{
			VkSubpassDescription subpassDescription;
			deMemset(&subpassDescription, 0, sizeof(VkSubpassDescription));
			subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

			VkRenderPassCreateInfo renderPassCreateInfo
			{
				VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
				DE_NULL, 0, 0, DE_NULL,
				1, &subpassDescription, 0, DE_NULL
			};

			// Error here - renderpass and framebuffer were created localy
			Move <VkRenderPass> renderPass = createRenderPass(vk, vkDevice, &renderPassCreateInfo);

			VkFramebufferCreateInfo framebufferCreateInfo
			{
				VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, DE_NULL,
				0, *renderPass, 0, DE_NULL, 16, 16, 1
			};
			Move <VkFramebuffer> framebuffer = createFramebuffer(vk, vkDevice, &framebufferCreateInfo);

			VkRenderPassBeginInfo renderPassBeginInfo =
			{
				VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
				DE_NULL, *renderPass, *framebuffer, { { 0, 0 }, { 16, 16 } },
				0, DE_NULL
			};

			beginCommandBuffer(vk, *cmdBuffer, 0u);
			vk.cmdBeginRenderPass(*cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			vk.cmdEndRenderPass(*cmdBuffer);

			// not executing endCommandBuffer(vk, *cmdBuffer);
			// command buffer is still in recording state
			break;
			// renderpass and framebuffer are destroyed; command buffer should be now in invalid state
		}
		case STT_EXECUTABLE_TO_INVALID:
		{
			// create event that will be used to check if command buffer has been executed
			const Unique<VkEvent> localEvent(createEvent(vk, vkDevice));
			VK_CHECK(vk.resetEvent(vkDevice, *localEvent));

			beginCommandBuffer(vk, *cmdBuffer, 0u);
			vk.cmdSetEvent(*cmdBuffer, *localEvent, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
			endCommandBuffer(vk, *cmdBuffer);
			// command buffer is in executable state
			break;
			// localEvent is destroyed; command buffer should be now in invalid state
		}
	}

	VK_CHECK(vk.resetEvent(vkDevice, *globalEvent));

	vk.resetCommandBuffer(*cmdBuffer, 0u);
	// command buffer should now be back in initial state

	// verify commandBuffer
	beginCommandBuffer(vk, *cmdBuffer, 0u);
	vk.cmdSetEvent(*cmdBuffer, *globalEvent, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, vkDevice, queue, *cmdBuffer);

	// check if buffer has been executed
	VkResult result = vk.getEventStatus(vkDevice, *globalEvent);
	if (result != VK_EVENT_SET)
		return tcu::TestStatus::fail("Submit failed");

	return tcu::TestStatus::pass("Pass");
}

// Shaders
void genComputeSource (SourceCollections& programCollection)
{
	const char* const						versionDecl				= glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES);
	std::ostringstream						bufGood;

	bufGood << versionDecl << "\n"
	<< ""
	<< "layout(local_size_x = 1u, local_size_y = 1u, local_size_z = 1u) in;\n"
	<< "layout(set = 0, binding = 1u, std140) uniform BufferName\n"
	<< "{\n"
	<< "	highp vec4 colorA;\n"
	<< "	highp vec4 colorB;\n"
	<< "} b_instance;\n"
	<< "layout(set = 0, binding = 0, std140) writeonly buffer OutBuf\n"
	<< "{\n"
	<< "	highp vec4 read_colors[4];\n"
	<< "} b_out;\n"
	<< "void main(void)\n"
	<< "{\n"
	<< "	highp int quadrant_id = int(gl_WorkGroupID.x);\n"
	<< "	highp vec4 result_color;\n"
	<< "	if (quadrant_id == 1 || quadrant_id == 2)\n"
	<< "		result_color = b_instance.colorA;\n"
	<< "	else\n"
	<< "		result_color = b_instance.colorB;\n"
	<< "	b_out.read_colors[gl_WorkGroupID.x] = result_color;\n"
	<< "}\n";

	programCollection.glslSources.add("compute_good") << glu::ComputeSource(bufGood.str());

	std::ostringstream	bufBad;

	bufBad	<< versionDecl << "\n"
	<< ""
	<< "layout(local_size_x = 1u, local_size_y = 1u, local_size_z = 1u) in;\n"
	<< "layout(set = 0, binding = 1u, std140) uniform BufferName\n"
	<< "{\n"
	<< "	highp vec4 colorA;\n"
	<< "	highp vec4 colorB;\n"
	<< "} b_instance;\n"
	<< "layout(set = 0, binding = 0, std140) writeonly buffer OutBuf\n"
	<< "{\n"
	<< "	highp vec4 read_colors[4];\n"
	<< "} b_out;\n"
	<< "void main(void)\n"
	<< "{\n"
	<< "	highp int quadrant_id = int(gl_WorkGroupID.x);\n"
	<< "	highp vec4 result_color;\n"
	<< "	if (quadrant_id == 1 || quadrant_id == 2)\n"
	<< "		result_color = b_instance.colorA;\n"
	<< "	else\n"
	<< "		result_color = b_instance.colorB;\n"
	<< "	b_out.read_colors[gl_WorkGroupID.x] = vec4(0.0, 0.0, 0.0, 0.0);\n"
	<< "}\n";

	programCollection.glslSources.add("compute_bad") << glu::ComputeSource(bufBad.str());
}

void genComputeIncrementSource (SourceCollections& programCollection)
{
	const char* const						versionDecl = glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES);
	std::ostringstream						bufIncrement;

	bufIncrement << versionDecl << "\n"
		<< ""
		<< "layout(local_size_x = 1u, local_size_y = 1u, local_size_z = 1u) in;\n"
		<< "layout(set = 0, binding = 0, std140) buffer InOutBuf\n"
		<< "{\n"
		<< "    coherent uint count;\n"
		<< "} b_in_out;\n"
		<< "void main(void)\n"
		<< "{\n"
		<< "	atomicAdd(b_in_out.count, 1u);\n"
		<< "}\n";

	programCollection.glslSources.add("compute_increment") << glu::ComputeSource(bufIncrement.str());
}

void genComputeIncrementSourceBadInheritance(SourceCollections& programCollection, BadInheritanceInfoCase testCase)
{
	DE_UNREF(testCase);
	return genComputeIncrementSource(programCollection);
}

void checkEventSupport (Context& context)
{
	if (context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") && !context.getPortabilitySubsetFeatures().events)
		TCU_THROW(NotSupportedError, "VK_KHR_portability_subset: Events are not supported by this implementation");
}

void checkEventSupport (Context& context, const VkCommandBufferLevel)
{
	checkEventSupport(context);
}

struct ManyDrawsParams
{
	VkCommandBufferLevel	level;
	VkExtent3D				imageExtent;
	deUint32				seed;

	ManyDrawsParams(VkCommandBufferLevel level_, const VkExtent3D& extent_, deUint32 seed_)
		: level			(level_)
		, imageExtent	(extent_)
		, seed			(seed_)
	{}
};

struct ManyDrawsVertex
{
	using Color = tcu::Vector<deUint8, 4>;

	tcu::Vec2	coords;
	Color		color;

	ManyDrawsVertex (const tcu::Vec2& coords_, const Color& color_) : coords(coords_), color(color_) {}
};

VkFormat getSupportedDepthStencilFormat (const InstanceInterface& vki, VkPhysicalDevice physDev)
{
	const VkFormat				formatList[]	= { VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT };
	const VkFormatFeatureFlags	requirements	= (VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT);

	for (int i = 0; i < DE_LENGTH_OF_ARRAY(formatList); ++i)
	{
		const auto properties = getPhysicalDeviceFormatProperties(vki, physDev, formatList[i]);
		if ((properties.optimalTilingFeatures & requirements) == requirements)
			return formatList[i];
	}

	TCU_THROW(NotSupportedError, "No suitable depth/stencil format support");
	return VK_FORMAT_UNDEFINED;
}

class ManyDrawsCase : public TestCase
{
public:
							ManyDrawsCase			(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const ManyDrawsParams& params);
	virtual					~ManyDrawsCase			(void) {}

	virtual void			checkSupport			(Context& context) const;
	virtual void			initPrograms			(vk::SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance			(Context& context) const;

	static VkFormat			getColorFormat			(void) { return VK_FORMAT_R8G8B8A8_UINT; }

protected:
	ManyDrawsParams			m_params;
};

class ManyDrawsInstance : public TestInstance
{
public:
								ManyDrawsInstance	(Context& context, const ManyDrawsParams& params);
	virtual						~ManyDrawsInstance	(void) {}

	virtual tcu::TestStatus		iterate				(void);

protected:
	ManyDrawsParams				m_params;
};

using BufferPtr = de::MovePtr<BufferWithMemory>;
using ImagePtr = de::MovePtr<ImageWithMemory>;

struct ManyDrawsVertexBuffers
{
	BufferPtr stagingBuffer;
	BufferPtr vertexBuffer;
};

struct ManyDrawsAllocatedData
{
	ManyDrawsVertexBuffers	frontBuffers;
	ManyDrawsVertexBuffers	backBuffers;
	ImagePtr				colorAttachment;
	ImagePtr				dsAttachment;
	BufferPtr				colorCheckBuffer;
	BufferPtr				stencilCheckBuffer;

	static deUint32 calcNumPixels (const VkExtent3D& extent)
	{
		DE_ASSERT(extent.depth == 1u);
		return (extent.width * extent.height);
	}
	static deUint32 calcNumVertices (const VkExtent3D& extent)
	{
		// One triangle (3 vertices) per output image pixel.
		return (calcNumPixels(extent) * 3u);
	}

	static VkDeviceSize calcVertexBufferSize (const VkExtent3D& extent)
	{
		return calcNumVertices(extent) * sizeof(ManyDrawsVertex);
	}

	static void makeVertexBuffers (const DeviceInterface& vkd, VkDevice device, Allocator& alloc, VkDeviceSize size, ManyDrawsVertexBuffers& buffers)
	{
		const auto stagingBufferInfo	= makeBufferCreateInfo(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
		const auto vertexBufferInfo		= makeBufferCreateInfo(size, (VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT));

		buffers.stagingBuffer	= BufferPtr(new BufferWithMemory(vkd, device, alloc, stagingBufferInfo, MemoryRequirement::HostVisible));
		buffers.vertexBuffer	= BufferPtr(new BufferWithMemory(vkd, device, alloc, vertexBufferInfo, MemoryRequirement::Any));
	}

	ManyDrawsAllocatedData (const DeviceInterface &vkd, VkDevice device, Allocator &alloc, const VkExtent3D& imageExtent, VkFormat colorFormat, VkFormat dsFormat)
	{
		const auto numPixels		= calcNumPixels(imageExtent);
		const auto vertexBufferSize	= calcVertexBufferSize(imageExtent);

		makeVertexBuffers(vkd, device, alloc, vertexBufferSize, frontBuffers);
		makeVertexBuffers(vkd, device, alloc, vertexBufferSize, backBuffers);

		const auto colorUsage	= (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
		const auto dsUsage		= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

		const VkImageCreateInfo colorAttachmentInfo =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
			nullptr,								//	const void*				pNext;
			0u,										//	VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
			colorFormat,							//	VkFormat				format;
			imageExtent,							//	VkExtent3D				extent;
			1u,										//	deUint32				mipLevels;
			1u,										//	deUint32				arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
			colorUsage,								//	VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
			0u,										//	deUint32				queueFamilyIndexCount;
			nullptr,								//	const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
		};
		colorAttachment = ImagePtr(new ImageWithMemory(vkd, device, alloc, colorAttachmentInfo, MemoryRequirement::Any));

		const VkImageCreateInfo dsAttachmentInfo =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
			nullptr,								//	const void*				pNext;
			0u,										//	VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
			dsFormat,								//	VkFormat				format;
			imageExtent,							//	VkExtent3D				extent;
			1u,										//	deUint32				mipLevels;
			1u,										//	deUint32				arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
			dsUsage,								//	VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
			0u,										//	deUint32				queueFamilyIndexCount;
			nullptr,								//	const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
		};
		dsAttachment = ImagePtr(new ImageWithMemory(vkd, device, alloc, dsAttachmentInfo, MemoryRequirement::Any));

		const auto colorCheckBufferSize		= static_cast<VkDeviceSize>(numPixels * tcu::getPixelSize(mapVkFormat(colorFormat)));
		const auto colorCheckBufferInfo		= makeBufferCreateInfo(colorCheckBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);

		colorCheckBuffer = BufferPtr(new BufferWithMemory(vkd, device, alloc, colorCheckBufferInfo, MemoryRequirement::HostVisible));

		const auto stencilFormat			= tcu::TextureFormat(tcu::TextureFormat::S, tcu::TextureFormat::UNSIGNED_INT8);
		const auto stencilCheckBufferSize	= static_cast<VkDeviceSize>(numPixels * tcu::getPixelSize(stencilFormat));
		const auto stencilCheckBufferInfo	= makeBufferCreateInfo(stencilCheckBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);

		stencilCheckBuffer = BufferPtr(new BufferWithMemory(vkd, device, alloc, stencilCheckBufferInfo, MemoryRequirement::HostVisible));
	}
};

ManyDrawsCase::ManyDrawsCase (tcu::TestContext& testCtx, const std::string& name, const std::string& description, const ManyDrawsParams& params)
	: TestCase	(testCtx, name, description)
	, m_params	(params)
{}

void ManyDrawsCase::checkSupport (Context& context) const
{
	const auto& vki			= context.getInstanceInterface();
	const auto	physDev		= context.getPhysicalDevice();
	const auto&	vkd			= context.getDeviceInterface();
	const auto	device		= context.getDevice();
	auto&		alloc		= context.getDefaultAllocator();
	const auto	dsFormat	= getSupportedDepthStencilFormat(vki, physDev);

	try
	{
		ManyDrawsAllocatedData allocatedData(vkd, device, alloc, m_params.imageExtent, getColorFormat(), dsFormat);
	}
	catch (const vk::Error& err)
	{
		const auto result = err.getError();
		if (result == VK_ERROR_OUT_OF_HOST_MEMORY || result == VK_ERROR_OUT_OF_DEVICE_MEMORY)
			TCU_THROW(NotSupportedError, "Not enough memory to run this test");
		throw;
	}
}

void ManyDrawsCase::initPrograms (vk::SourceCollections& programCollection) const
{
	std::ostringstream vert;
	vert
		<< "#version 450\n"
		<< "\n"
		<< "layout(location=0) in vec2 inCoords;\n"
		<< "layout(location=1) in uvec4 inColor;\n"
		<< "\n"
		<< "layout(location=0) out flat uvec4 outColor;\n"
		<< "\n"
		<< "void main()\n"
		<< "{\n"
		<< "    gl_Position = vec4(inCoords, 0.0, 1.0);\n"
		<< "    outColor = inColor;\n"
		<< "}\n"
		;

	std::ostringstream frag;
	frag
		<< "#version 450\n"
		<< "\n"
		<< "layout(location=0) in flat uvec4 inColor;\n"
		<< "layout(location=0) out uvec4 outColor;\n"
		<< "\n"
		<< "void main()\n"
		<< "{\n"
		<< "	outColor = inColor;\n"
		<< "}\n"
		;

	programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
	programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

TestInstance* ManyDrawsCase::createInstance (Context& context) const
{
	return new ManyDrawsInstance(context, m_params);
}

ManyDrawsInstance::ManyDrawsInstance (Context& context, const ManyDrawsParams& params)
	: TestInstance	(context)
	, m_params		(params)
{}

void copyAndFlush (const DeviceInterface& vkd, VkDevice device, BufferWithMemory& buffer, const std::vector<ManyDrawsVertex>& vertices)
{
	auto& alloc		= buffer.getAllocation();
	void* hostPtr	= alloc.getHostPtr();

	deMemcpy(hostPtr, vertices.data(), de::dataSize(vertices));
	flushAlloc(vkd, device, alloc);
}

tcu::TestStatus ManyDrawsInstance::iterate (void)
{
	const auto&	vki					= m_context.getInstanceInterface();
	const auto	physDev				= m_context.getPhysicalDevice();
	const auto&	vkd					= m_context.getDeviceInterface();
	const auto	device				= m_context.getDevice();
	auto&		alloc				= m_context.getDefaultAllocator();
	const auto	qIndex				= m_context.getUniversalQueueFamilyIndex();
	const auto	queue				= m_context.getUniversalQueue();

	const auto	colorFormat			= ManyDrawsCase::getColorFormat();
	const auto	dsFormat			= getSupportedDepthStencilFormat(vki, physDev);
	const auto	vertexBufferSize	= ManyDrawsAllocatedData::calcVertexBufferSize(m_params.imageExtent);
	const auto	vertexBufferOffset	= static_cast<VkDeviceSize>(0);
	const auto	numPixels			= ManyDrawsAllocatedData::calcNumPixels(m_params.imageExtent);
	const auto	numVertices			= ManyDrawsAllocatedData::calcNumVertices(m_params.imageExtent);
	const auto	alphaValue			= std::numeric_limits<deUint8>::max();
	const auto	pixelWidth			= 2.0f / static_cast<float>(m_params.imageExtent.width);	// Normalized size.
	const auto	pixelWidthHalf		= pixelWidth / 2.0f;										// Normalized size.
	const auto	pixelHeight			= 2.0f / static_cast<float>(m_params.imageExtent.height);	// Normalized size.
	const auto	useSecondary		= (m_params.level == VK_COMMAND_BUFFER_LEVEL_SECONDARY);

	// Allocate all needed data up front.
	ManyDrawsAllocatedData testData(vkd, device, alloc, m_params.imageExtent, colorFormat, dsFormat);

	// Generate random colors.
	de::Random							rnd(m_params.seed);
	std::vector<ManyDrawsVertex::Color>	colors;

	colors.reserve(numPixels);
	for (deUint32 i = 0; i < numPixels; ++i)
	{
#if 0
		const deUint8 red	= ((i      ) & 0xFFu);
		const deUint8 green	= ((i >>  8) & 0xFFu);
		const deUint8 blue	= ((i >> 16) & 0xFFu);
		colors.push_back(ManyDrawsVertex::Color(red, green, blue, alphaValue));
#else
		colors.push_back(ManyDrawsVertex::Color(rnd.getUint8(), rnd.getUint8(), rnd.getUint8(), alphaValue));
#endif
	}

	// Fill vertex data. One triangle per pixel, front and back.
	std::vector<ManyDrawsVertex> frontVector;
	std::vector<ManyDrawsVertex> backVector;
	frontVector.reserve(numVertices);
	backVector.reserve(numVertices);

	for (deUint32 y = 0; y < m_params.imageExtent.height; ++y)
	for (deUint32 x = 0; x < m_params.imageExtent.width; ++x)
	{
		float x_left	= static_cast<float>(x) * pixelWidth - 1.0f;
		float x_mid		= x_left + pixelWidthHalf;
		float x_right	= x_left + pixelWidth;
		float y_top		= static_cast<float>(y) * pixelHeight - 1.0f;
		float y_bottom	= y_top + pixelHeight;

		// Triangles in the "back" mesh will have different colors.
		const auto		colorIdx		= y * m_params.imageExtent.width + x;
		const auto&		frontColor		= colors[colorIdx];
		const auto&		backColor		= colors[colors.size() - 1u - colorIdx];

		const tcu::Vec2	triangle[3u]	=
		{
			tcu::Vec2(x_left, y_top),
			tcu::Vec2(x_right, y_top),
			tcu::Vec2(x_mid, y_bottom),
		};

		frontVector.emplace_back(triangle[0], frontColor);
		frontVector.emplace_back(triangle[1], frontColor);
		frontVector.emplace_back(triangle[2], frontColor);

		backVector.emplace_back(triangle[0], backColor);
		backVector.emplace_back(triangle[1], backColor);
		backVector.emplace_back(triangle[2], backColor);
	}

	// Copy vertex data to staging buffers.
	copyAndFlush(vkd, device, *testData.frontBuffers.stagingBuffer, frontVector);
	copyAndFlush(vkd, device, *testData.backBuffers.stagingBuffer, backVector);

	// Color attachment view.
	const auto		colorResourceRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const auto		colorAttachmentView	= makeImageView(vkd, device, testData.colorAttachment->get(), VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorResourceRange);

	// Depth/stencil attachment view.
	const auto		dsResourceRange		= makeImageSubresourceRange((VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT), 0u, 1u, 0u, 1u);
	const auto		dsAttachmentView	= makeImageView(vkd, device, testData.dsAttachment->get(), VK_IMAGE_VIEW_TYPE_2D, dsFormat, dsResourceRange);

	const VkImageView	attachmentArray[]	= { colorAttachmentView.get(), dsAttachmentView.get() };
	const auto			numAttachments		= static_cast<deUint32>(DE_LENGTH_OF_ARRAY(attachmentArray));

	const auto renderPass	= makeRenderPass(vkd, device, colorFormat, dsFormat);
	const auto framebuffer	= makeFramebuffer(vkd, device, renderPass.get(), numAttachments, attachmentArray, m_params.imageExtent.width, m_params.imageExtent.height);

	const auto vertModule	= createShaderModule(vkd, device, m_context.getBinaryCollection().get("vert"), 0u);
	const auto fragModule	= createShaderModule(vkd, device, m_context.getBinaryCollection().get("frag"), 0u);

	const std::vector<VkViewport>	viewports	(1u, makeViewport(m_params.imageExtent));
	const std::vector<VkRect2D>		scissors	(1u, makeRect2D(m_params.imageExtent));

	const auto descriptorSetLayout	= DescriptorSetLayoutBuilder().build(vkd, device);
	const auto pipelineLayout		= makePipelineLayout(vkd, device, descriptorSetLayout.get());

	const VkVertexInputBindingDescription bindings[] =
	{
		makeVertexInputBindingDescription(0u, static_cast<deUint32>(sizeof(ManyDrawsVertex)), VK_VERTEX_INPUT_RATE_VERTEX),
	};

	const VkVertexInputAttributeDescription attributes[] =
	{
		makeVertexInputAttributeDescription(0u, 0u, VK_FORMAT_R32G32_SFLOAT, static_cast<deUint32>(offsetof(ManyDrawsVertex, coords))),
		makeVertexInputAttributeDescription(1u, 0u, VK_FORMAT_R8G8B8A8_UINT, static_cast<deUint32>(offsetof(ManyDrawsVertex, color))),
	};

	const VkPipelineVertexInputStateCreateInfo inputState =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	//	VkStructureType								sType;
		nullptr,													//	const void*									pNext;
		0u,															//	VkPipelineVertexInputStateCreateFlags		flags;
		static_cast<deUint32>(DE_LENGTH_OF_ARRAY(bindings)),		//	deUint32									vertexBindingDescriptionCount;
		bindings,													//	const VkVertexInputBindingDescription*		pVertexBindingDescriptions;
		static_cast<deUint32>(DE_LENGTH_OF_ARRAY(attributes)),		//	deUint32									vertexAttributeDescriptionCount;
		attributes,													//	const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
	};

	// Stencil state: this is key for checking and obtaining the right results. The stencil buffer will be cleared to 0. The first
	// set of draws ("front" set of triangles) will pass the test and increment the stencil value to 1. The second set of draws
	// ("back" set of triangles, not really in the back because all of them have depth 0.0) will not pass the stencil test then, but
	// still increment the stencil value to 2.
	//
	// At the end of the test, if every draw command was executed correctly in the expected order, the color buffer will have the
	// colors of the front set, and the stencil buffer will be full of 2s.
	const auto stencilOpState = makeStencilOpState(VK_STENCIL_OP_INCREMENT_AND_CLAMP, VK_STENCIL_OP_INCREMENT_AND_CLAMP, VK_STENCIL_OP_KEEP,
		VK_COMPARE_OP_EQUAL, 0xFFu, 0xFFu, 0u);

	const VkPipelineDepthStencilStateCreateInfo dsState =
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	// VkStructureType                          sType
		nullptr,													// const void*                              pNext
		0u,															// VkPipelineDepthStencilStateCreateFlags   flags
		VK_FALSE,													// VkBool32                                 depthTestEnable
		VK_FALSE,													// VkBool32                                 depthWriteEnable
		VK_COMPARE_OP_NEVER,										// VkCompareOp                              depthCompareOp
		VK_FALSE,													// VkBool32                                 depthBoundsTestEnable
		VK_TRUE,													// VkBool32                                 stencilTestEnable
		stencilOpState,												// VkStencilOpState                         front
		stencilOpState,												// VkStencilOpState                         back
		0.0f,														// float                                    minDepthBounds
		1.0f,														// float                                    maxDepthBounds
	};

	const auto pipeline = makeGraphicsPipeline(vkd, device, pipelineLayout.get(),
			vertModule.get(), DE_NULL, DE_NULL, DE_NULL, fragModule.get(),
			renderPass.get(), viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0u, 0u,
			&inputState, nullptr, nullptr, &dsState);

	// Command pool and buffers.
	using CmdBufferPtr = Move<VkCommandBuffer>;
	const auto cmdPool = makeCommandPool(vkd, device, qIndex);

	CmdBufferPtr	primaryCmdBufferPtr;
	CmdBufferPtr	secondaryCmdBufferPtr;
	VkCommandBuffer	primaryCmdBuffer;
	VkCommandBuffer	secondaryCmdBuffer;
	VkCommandBuffer	drawsCmdBuffer;

	primaryCmdBufferPtr		= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	primaryCmdBuffer		= primaryCmdBufferPtr.get();
	drawsCmdBuffer			= primaryCmdBuffer;
	beginCommandBuffer(vkd, primaryCmdBuffer);

	// Clear values.
	std::vector<VkClearValue> clearValues(2u);
	clearValues[0] = makeClearValueColorU32(0u, 0u, 0u, 0u);
	clearValues[1] = makeClearValueDepthStencil(1.0f, 0u);

	// Copy staging buffers to vertex buffers.
	const auto copyRegion = makeBufferCopy(0ull, 0ull, vertexBufferSize);
	vkd.cmdCopyBuffer(primaryCmdBuffer, testData.frontBuffers.stagingBuffer->get(), testData.frontBuffers.vertexBuffer->get(), 1u, &copyRegion);
	vkd.cmdCopyBuffer(primaryCmdBuffer, testData.backBuffers.stagingBuffer->get(), testData.backBuffers.vertexBuffer->get(), 1u, &copyRegion);

	// Use barrier for vertex reads.
	const auto vertexBarier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT);
	vkd.cmdPipelineBarrier(primaryCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0u, 1u, &vertexBarier, 0u, nullptr, 0u, nullptr);

	// Change depth/stencil attachment layout.
	const auto dsBarrier = makeImageMemoryBarrier(0, (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, testData.dsAttachment->get(), dsResourceRange);
	vkd.cmdPipelineBarrier(primaryCmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, (VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT), 0u, 0u, nullptr, 0u, nullptr, 1u, &dsBarrier);

	beginRenderPass(vkd, primaryCmdBuffer, renderPass.get(), framebuffer.get(),
		scissors[0], static_cast<deUint32>(clearValues.size()), clearValues.data(),
		(useSecondary ? VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS : VK_SUBPASS_CONTENTS_INLINE));

	if (useSecondary)
	{
		secondaryCmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_SECONDARY);
		secondaryCmdBuffer		= secondaryCmdBufferPtr.get();
		drawsCmdBuffer			= secondaryCmdBuffer;

		const VkCommandBufferInheritanceInfo inheritanceInfo =
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,	//	VkStructureType					sType;
			nullptr,											//	const void*						pNext;
			renderPass.get(),									//	VkRenderPass					renderPass;
			0u,													//	deUint32						subpass;
			framebuffer.get(),									//	VkFramebuffer					framebuffer;
			0u,													//	VkBool32						occlusionQueryEnable;
			0u,													//	VkQueryControlFlags				queryFlags;
			0u,													//	VkQueryPipelineStatisticFlags	pipelineStatistics;
		};

		const VkCommandBufferUsageFlags	usageFlags	= (VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT | VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
		const VkCommandBufferBeginInfo	beginInfo	=
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			nullptr,
			usageFlags,										//	VkCommandBufferUsageFlags				flags;
			&inheritanceInfo,								//	const VkCommandBufferInheritanceInfo*	pInheritanceInfo;
		};

		VK_CHECK(vkd.beginCommandBuffer(secondaryCmdBuffer, &beginInfo));
	}

	// Bind pipeline.
	vkd.cmdBindPipeline(drawsCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());

	// Draw triangles in front.
	vkd.cmdBindVertexBuffers(drawsCmdBuffer, 0u, 1u, &testData.frontBuffers.vertexBuffer->get(), &vertexBufferOffset);
	for (deUint32 i = 0; i < numPixels; ++i)
		vkd.cmdDraw(drawsCmdBuffer, 3u, 1u, i*3u, 0u);

	// Draw triangles in the "back". This should have no effect due to the stencil test.
	vkd.cmdBindVertexBuffers(drawsCmdBuffer, 0u, 1u, &testData.backBuffers.vertexBuffer->get(), &vertexBufferOffset);
	for (deUint32 i = 0; i < numPixels; ++i)
		vkd.cmdDraw(drawsCmdBuffer, 3u, 1u, i*3u, 0u);

	if (useSecondary)
	{
		endCommandBuffer(vkd, secondaryCmdBuffer);
		vkd.cmdExecuteCommands(primaryCmdBuffer, 1u, &secondaryCmdBuffer);
	}

	endRenderPass(vkd, primaryCmdBuffer);

	// Copy color and depth/stencil attachments to verification buffers.
	const auto colorAttachmentBarrier = makeImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, testData.colorAttachment->get(), colorResourceRange);
	vkd.cmdPipelineBarrier(primaryCmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &colorAttachmentBarrier);

	const auto colorResourceLayers	= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const auto colorCopyRegion		= makeBufferImageCopy(m_params.imageExtent, colorResourceLayers);
	vkd.cmdCopyImageToBuffer(primaryCmdBuffer, testData.colorAttachment->get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, testData.colorCheckBuffer->get(), 1u, &colorCopyRegion);

	const auto stencilAttachmentBarrier = makeImageMemoryBarrier(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, testData.dsAttachment->get(), dsResourceRange);
	vkd.cmdPipelineBarrier(primaryCmdBuffer, (VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT), VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &stencilAttachmentBarrier);

	const auto stencilResourceLayers	= makeImageSubresourceLayers(VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, 1u);
	const auto stencilCopyRegion		= makeBufferImageCopy(m_params.imageExtent, stencilResourceLayers);
	vkd.cmdCopyImageToBuffer(primaryCmdBuffer, testData.dsAttachment->get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, testData.stencilCheckBuffer->get(), 1u, &stencilCopyRegion);

	const auto verificationBuffersBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	vkd.cmdPipelineBarrier(primaryCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &verificationBuffersBarrier, 0u, nullptr, 0u, nullptr);

	endCommandBuffer(vkd, primaryCmdBuffer);
	submitCommandsAndWait(vkd, device, queue, primaryCmdBuffer);

	// Check buffer contents.
	auto& colorCheckBufferAlloc	= testData.colorCheckBuffer->getAllocation();
	void* colorCheckBufferData	= colorCheckBufferAlloc.getHostPtr();
	invalidateAlloc(vkd, device, colorCheckBufferAlloc);

	auto& stencilCheckBufferAlloc	= testData.stencilCheckBuffer->getAllocation();
	void* stencilCheckBufferData	= stencilCheckBufferAlloc.getHostPtr();
	invalidateAlloc(vkd, device, stencilCheckBufferAlloc);

	const auto iWidth			= static_cast<int>(m_params.imageExtent.width);
	const auto iHeight			= static_cast<int>(m_params.imageExtent.height);
	const auto colorTcuFormat	= mapVkFormat(colorFormat);
	const auto stencilTcuFormat	= tcu::TextureFormat(tcu::TextureFormat::S, tcu::TextureFormat::UNSIGNED_INT8);

	tcu::TextureLevel			referenceLevel		(colorTcuFormat, iWidth, iHeight);
	tcu::PixelBufferAccess		referenceAccess		= referenceLevel.getAccess();
	tcu::TextureLevel			colorErrorLevel		(mapVkFormat(VK_FORMAT_R8G8B8A8_UNORM), iWidth, iHeight);
	tcu::PixelBufferAccess		colorErrorAccess	= colorErrorLevel.getAccess();
	tcu::TextureLevel			stencilErrorLevel	(mapVkFormat(VK_FORMAT_R8G8B8A8_UNORM), iWidth, iHeight);
	tcu::PixelBufferAccess		stencilErrorAccess	= stencilErrorLevel.getAccess();
	tcu::ConstPixelBufferAccess	colorAccess			(colorTcuFormat, iWidth, iHeight, 1, colorCheckBufferData);
	tcu::ConstPixelBufferAccess	stencilAccess		(stencilTcuFormat, iWidth, iHeight, 1, stencilCheckBufferData);
	const tcu::Vec4				green				(0.0f, 1.0f, 0.0f, 1.0f);
	const tcu::Vec4				red					(1.0f, 0.0f, 0.0f, 1.0f);
	const int					expectedStencil		= 2;
	bool						colorFail			= false;
	bool						stencilFail			= false;

	for (int y = 0; y < iHeight; ++y)
	for (int x = 0; x < iWidth; ++x)
	{
		const tcu::UVec4	colorValue		= colorAccess.getPixelUint(x, y);
		const auto			expectedPixel	= colors[y * iWidth + x];
		const tcu::UVec4	expectedValue	(expectedPixel.x(), expectedPixel.y(), expectedPixel.z(), expectedPixel.w());
		const bool			colorMismatch	= (colorValue != expectedValue);

		const auto			stencilValue	= stencilAccess.getPixStencil(x, y);
		const bool			stencilMismatch	= (stencilValue != expectedStencil);

		referenceAccess.setPixel(expectedValue, x, y);
		colorErrorAccess.setPixel((colorMismatch ? red : green), x, y);
		stencilErrorAccess.setPixel((stencilMismatch ? red : green), x, y);

		if (stencilMismatch)
			stencilFail = true;

		if (colorMismatch)
			colorFail = true;
	}

	if (colorFail || stencilFail)
	{
		auto& log = m_context.getTestContext().getLog();
		log
			<< tcu::TestLog::ImageSet("Result", "")
			<< tcu::TestLog::Image("ColorOutput", "", colorAccess)
			<< tcu::TestLog::Image("ColorReference", "", referenceAccess)
			<< tcu::TestLog::Image("ColorError", "", colorErrorAccess)
			<< tcu::TestLog::Image("StencilError", "", stencilErrorAccess)
			<< tcu::TestLog::EndImageSet
			;
		TCU_FAIL("Mismatched output and reference color or stencil; please check test log --");
	}

	return tcu::TestStatus::pass("Pass");
}

} // anonymous

tcu::TestCaseGroup* createCommandBuffersTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	commandBuffersTests	(new tcu::TestCaseGroup(testCtx, "command_buffers", "Command Buffers Tests"));

	/* 19.1. Command Pools (5.1 in VK 1.0 Spec) */
	addFunctionCase				(commandBuffersTests.get(), "pool_create_null_params",			"",	createPoolNullParamsTest);
	addFunctionCase				(commandBuffersTests.get(), "pool_create_non_null_allocator",	"",	createPoolNonNullAllocatorTest);
	addFunctionCase				(commandBuffersTests.get(), "pool_create_transient_bit",		"",	createPoolTransientBitTest);
	addFunctionCase				(commandBuffersTests.get(), "pool_create_reset_bit",			"",	createPoolResetBitTest);
	addFunctionCase				(commandBuffersTests.get(), "pool_reset_release_res",			"",	resetPoolReleaseResourcesBitTest);
	addFunctionCase				(commandBuffersTests.get(), "pool_reset_no_flags_res",			"",	resetPoolNoFlagsTest);
	addFunctionCase				(commandBuffersTests.get(), "pool_reset_reuse",					"",	checkEventSupport, resetPoolReuseTest);
	/* 19.2. Command Buffer Lifetime (5.2 in VK 1.0 Spec) */
	addFunctionCase				(commandBuffersTests.get(), "allocate_single_primary",			"", allocatePrimaryBufferTest);
	addFunctionCase				(commandBuffersTests.get(), "allocate_many_primary",			"",	allocateManyPrimaryBuffersTest);
	addFunctionCase				(commandBuffersTests.get(), "allocate_single_secondary",		"", allocateSecondaryBufferTest);
	addFunctionCase				(commandBuffersTests.get(), "allocate_many_secondary",			"", allocateManySecondaryBuffersTest);
	addFunctionCase				(commandBuffersTests.get(), "execute_small_primary",			"",	checkEventSupport, executePrimaryBufferTest);
	addFunctionCase				(commandBuffersTests.get(), "execute_large_primary",			"",	checkEventSupport, executeLargePrimaryBufferTest);
	addFunctionCase				(commandBuffersTests.get(), "reset_implicit",					"", checkEventSupport, resetBufferImplicitlyTest);
	addFunctionCase				(commandBuffersTests.get(), "trim_command_pool",				"", checkEventSupport, trimCommandPoolTest, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	addFunctionCase				(commandBuffersTests.get(), "trim_command_pool_secondary",		"", checkEventSupport, trimCommandPoolTest, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
	/* 19.3. Command Buffer Recording (5.3 in VK 1.0 Spec) */
	addFunctionCase				(commandBuffersTests.get(), "record_single_primary",			"",	checkEventSupport, recordSinglePrimaryBufferTest);
	addFunctionCase				(commandBuffersTests.get(), "record_many_primary",				"", checkEventSupport, recordLargePrimaryBufferTest);
	addFunctionCase				(commandBuffersTests.get(), "record_single_secondary",			"",	checkEventSupport, recordSingleSecondaryBufferTest);
	addFunctionCase				(commandBuffersTests.get(), "record_many_secondary",			"", checkEventSupport, recordLargeSecondaryBufferTest);
	{
		deUint32	seed		= 1614182419u;
		const auto	smallExtent	= makeExtent3D(128u, 128u, 1u);
		const auto	largeExtent	= makeExtent3D(512u, 512u, 1u);

		commandBuffersTests->addChild(new ManyDrawsCase(testCtx, "record_many_draws_primary_1",		"", ManyDrawsParams(VK_COMMAND_BUFFER_LEVEL_PRIMARY,	smallExtent,	seed++)));
		commandBuffersTests->addChild(new ManyDrawsCase(testCtx, "record_many_draws_primary_2",		"", ManyDrawsParams(VK_COMMAND_BUFFER_LEVEL_PRIMARY,	largeExtent,	seed++)));
		commandBuffersTests->addChild(new ManyDrawsCase(testCtx, "record_many_draws_secondary_1",	"", ManyDrawsParams(VK_COMMAND_BUFFER_LEVEL_SECONDARY,	smallExtent,	seed++)));
		commandBuffersTests->addChild(new ManyDrawsCase(testCtx, "record_many_draws_secondary_2",	"", ManyDrawsParams(VK_COMMAND_BUFFER_LEVEL_SECONDARY,	largeExtent,	seed++)));
	}
	addFunctionCase				(commandBuffersTests.get(), "submit_twice_primary",				"",	checkEventSupport, submitPrimaryBufferTwiceTest);
	addFunctionCase				(commandBuffersTests.get(), "submit_twice_secondary",			"",	checkEventSupport, submitSecondaryBufferTwiceTest);
	addFunctionCase				(commandBuffersTests.get(), "record_one_time_submit_primary",	"",	checkEventSupport, oneTimeSubmitFlagPrimaryBufferTest);
	addFunctionCase				(commandBuffersTests.get(), "record_one_time_submit_secondary",	"",	checkEventSupport, oneTimeSubmitFlagSecondaryBufferTest);
	addFunctionCase				(commandBuffersTests.get(), "render_pass_continue",				"",	renderPassContinueTest, true);
	addFunctionCase				(commandBuffersTests.get(), "render_pass_continue_no_fb",		"",	renderPassContinueTest, false);
	addFunctionCase				(commandBuffersTests.get(), "record_simul_use_primary",			"",	checkEventSupport, simultaneousUsePrimaryBufferTest);
	addFunctionCase				(commandBuffersTests.get(), "record_simul_use_secondary",		"",	checkEventSupport, simultaneousUseSecondaryBufferTest);
	addFunctionCaseWithPrograms (commandBuffersTests.get(), "record_simul_use_secondary_one_primary", "", genComputeIncrementSource, simultaneousUseSecondaryBufferOnePrimaryBufferTest);
	addFunctionCaseWithPrograms (commandBuffersTests.get(), "record_simul_use_secondary_two_primary", "", genComputeIncrementSource, simultaneousUseSecondaryBufferTwoPrimaryBuffersTest);
	addFunctionCase				(commandBuffersTests.get(), "record_query_precise_w_flag",		"",	recordBufferQueryPreciseWithFlagTest);
	addFunctionCase				(commandBuffersTests.get(), "record_query_imprecise_w_flag",	"",	recordBufferQueryImpreciseWithFlagTest);
	addFunctionCase				(commandBuffersTests.get(), "record_query_imprecise_wo_flag",	"",	recordBufferQueryImpreciseWithoutFlagTest);
	addFunctionCaseWithPrograms (commandBuffersTests.get(), "bad_inheritance_info_random",		"", genComputeIncrementSourceBadInheritance, badInheritanceInfoTest, BadInheritanceInfoCase::RANDOM_PTR);
	addFunctionCaseWithPrograms (commandBuffersTests.get(), "bad_inheritance_info_random_cont",	"", genComputeIncrementSourceBadInheritance, badInheritanceInfoTest, BadInheritanceInfoCase::RANDOM_PTR_CONTINUATION);
	addFunctionCaseWithPrograms (commandBuffersTests.get(), "bad_inheritance_info_random_data",	"", genComputeIncrementSourceBadInheritance, badInheritanceInfoTest, BadInheritanceInfoCase::RANDOM_DATA_PTR);
	addFunctionCaseWithPrograms (commandBuffersTests.get(), "bad_inheritance_info_invalid_type", "", genComputeIncrementSourceBadInheritance, badInheritanceInfoTest, BadInheritanceInfoCase::INVALID_STRUCTURE_TYPE);
	addFunctionCaseWithPrograms (commandBuffersTests.get(), "bad_inheritance_info_valid_nonsense_type", "", genComputeIncrementSourceBadInheritance, badInheritanceInfoTest, BadInheritanceInfoCase::VALID_NONSENSE_TYPE);
	/* 19.4. Command Buffer Submission (5.4 in VK 1.0 Spec) */
	addFunctionCase				(commandBuffersTests.get(), "submit_count_non_zero",			"", checkEventSupport, submitBufferCountNonZero);
	addFunctionCase				(commandBuffersTests.get(), "submit_count_equal_zero",			"", checkEventSupport, submitBufferCountEqualZero);
	addFunctionCase				(commandBuffersTests.get(), "submit_wait_single_semaphore",		"", checkEventSupport, submitBufferWaitSingleSemaphore);
	addFunctionCase				(commandBuffersTests.get(), "submit_wait_many_semaphores",		"", checkEventSupport, submitBufferWaitManySemaphores);
	addFunctionCase				(commandBuffersTests.get(), "submit_null_fence",				"", checkEventSupport, submitBufferNullFence);
	addFunctionCase				(commandBuffersTests.get(), "submit_two_buffers_one_buffer_null_with_fence", "", checkEventSupport, submitTwoBuffersOneBufferNullWithFence);
	/* 19.5. Secondary Command Buffer Execution (5.6 in VK 1.0 Spec) */
	addFunctionCase				(commandBuffersTests.get(), "secondary_execute",				"",	checkEventSupport, executeSecondaryBufferTest);
	addFunctionCase				(commandBuffersTests.get(), "secondary_execute_twice",			"",	checkEventSupport, executeSecondaryBufferTwiceTest);
	/* 19.6. Commands Allowed Inside Command Buffers (? in VK 1.0 Spec) */
	addFunctionCaseWithPrograms (commandBuffersTests.get(), "order_bind_pipeline",				"", genComputeSource, orderBindPipelineTest);
	/* Verify untested transitions between command buffer states */
	addFunctionCase				(commandBuffersTests.get(), "recording_to_ininitial",			"", executeStateTransitionTest, STT_RECORDING_TO_INITIAL);
	addFunctionCase				(commandBuffersTests.get(), "executable_to_ininitial",			"", executeStateTransitionTest, STT_EXECUTABLE_TO_INITIAL);
	addFunctionCase				(commandBuffersTests.get(), "recording_to_invalid",				"", executeStateTransitionTest, STT_RECORDING_TO_INVALID);
	addFunctionCase				(commandBuffersTests.get(), "executable_to_invalid",			"", executeStateTransitionTest, STT_EXECUTABLE_TO_INVALID);

	return commandBuffersTests.release();
}

} // api
} // vkt

