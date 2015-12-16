/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be included
 * in all copies or substantial portions of the Materials.
 *
 * The Materials are Confidential Information as defined by the
 * Khronos Membership Agreement until designated non-confidential by Khronos,
 * at which point this condition clause shall be removed.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 *
 *//*!
 * \file
 * \brief Vulkan Copies And Blitting Tests
 *//*--------------------------------------------------------------------*/

#include "vktApiCopiesAndBlittingTests.hpp"

#include "deRandom.hpp"
#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVectorType.hpp"
#include "tcuTexture.hpp"

#include "tcuImageIO.hpp"

namespace vkt
{

namespace api
{

using namespace vk;

namespace
{

union CopyRegion
{
	VkBufferCopy		bufferCopy;
	VkImageCopy			imageCopy;
	VkBufferImageCopy	bufferToImageCopy;
	VkImageBlit			imageBlit;
};

struct TestParams
{
	union Data
	{
		struct Buffer
		{
			VkDeviceSize		size;
		}	buffer;
		struct Image
		{
			VkFormat	format;
			VkExtent3D	extent;
		}	image;
	}	src, dst;

	std::vector<CopyRegion> regions;
};

class CopiesAndBlittingTestInstance : public vkt::TestInstance
{
public:
										CopiesAndBlittingTestInstance		(Context&	context,
																			 TestParams	testParams);
	virtual								~CopiesAndBlittingTestInstance		(void);
	virtual tcu::TestStatus				iterate								(void);
	enum FillMode {
		Random,
		Sequential,
		White,
		Red
	};
protected:
	TestParams							m_params;

	Move<VkCommandPool>					m_cmdPool;
	Move<VkCommandBuffer>				m_cmdBuffer;
	Move<VkFence>						m_fence;
	de::MovePtr<tcu::TextureLevel>		m_sourceTextureLevel;
	de::MovePtr<tcu::TextureLevel>		m_destinationTextureLevel;
	de::MovePtr<tcu::TextureLevel>		m_expectedTextureLevel;
	std::vector<CopyRegion>				m_regions;

	VkCommandBufferBeginInfo			m_cmdBufferBeginInfo;

			void						generateBuffer						(tcu::PixelBufferAccess buffer, int width, int height, int depth = 1, FillMode = Sequential);
	virtual void						generateExpectedResult				(void);
			void						uploadBuffer						(tcu::ConstPixelBufferAccess src, const Allocation &bufferAlloc);
			void						uploadImage							(tcu::ConstPixelBufferAccess src, const VkImage &image);
	virtual tcu::TestStatus				checkTestResult						(tcu::ConstPixelBufferAccess result);
	virtual void						copyRegionToTextureLevel			(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst, CopyRegion region) = 0;
			deUint32					calculateSize						(tcu::ConstPixelBufferAccess src) const
										{
											return src.getWidth() * src.getHeight() * src.getDepth() * tcu::getPixelSize(src.getFormat());
										}

	de::MovePtr<tcu::TextureLevel>		readImage							(const vk::DeviceInterface&	vk,
																			 vk::VkDevice				device,
																			 vk::VkQueue				queue,
																			 vk::Allocator&				allocator,
																			 vk::VkImage				image,
																			 vk::VkFormat				format,
																			 const VkExtent3D			imageSize);
};

CopiesAndBlittingTestInstance::~CopiesAndBlittingTestInstance	(void)
{
}

CopiesAndBlittingTestInstance::CopiesAndBlittingTestInstance	(Context& context, TestParams testParams)
	: vkt::TestInstance		(context)
	, m_params			(testParams)
{
	const DeviceInterface&		vk					= context.getDeviceInterface();
	const VkDevice				vkDevice			= context.getDevice();
	const deUint32				queueFamilyIndex	= context.getUniversalQueueFamilyIndex();

	VkCommandBufferBeginInfo		cmdBufferBeginInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,			// VkStructureType					sType;
		DE_NULL,												// const void*						pNext;
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,			// VkCommandBufferUsageFlags		flags;
		DE_NULL,												// VkRenderPass						renderPass;
		0u,														// deUint32							subpass;
		DE_NULL,												// VkFramebuffer					framebuffer;
		false,													// VkBool32							occlusionQueryEnable;
		0u,														// VkQueryControlFlags				queryFlags;
		0u														// VkQueryPipelineStatisticFlags	pipelineStatistics;
	};

	m_cmdBufferBeginInfo = cmdBufferBeginInfo;

	// Create command pool
	{
		const VkCommandPoolCreateInfo cmdPoolParams =
		{
			VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,										// const void*			pNext;
			VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,			// VkCmdPoolCreateFlags	flags;
			queueFamilyIndex,								// deUint32				queueFamilyIndex;
		};

		m_cmdPool = createCommandPool(vk, vkDevice, &cmdPoolParams);
	}

	// Create command buffer
	{
		const VkCommandBufferAllocateInfo cmdBufferAllocateInfo =
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,	// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			*m_cmdPool,										// VkCommandPool			commandPool;
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,				// VkCommandBufferLevel		level;
			1u												// deUint32					bufferCount;
		};

		m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, &cmdBufferAllocateInfo);
	}

	// Create fence
	{
		const VkFenceCreateInfo fenceParams =
		{
			VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,								// const void*			pNext;
			0u										// VkFenceCreateFlags	flags;
		};

		m_fence = createFence(vk, vkDevice, &fenceParams);
	}
}

void CopiesAndBlittingTestInstance::generateBuffer(tcu::PixelBufferAccess buffer, int width, int height, int depth, FillMode mode)
{
	de::Random rnd(width ^ height ^ depth);
	for (int x = 0; x < width; x++)
		for (int y = 0; y < height; y++)
			for (int z = 0; z < depth; z++)
			{
				switch (mode) {
				case Sequential:
					buffer.setPixel(tcu::UVec4(x, y, z, 255), x, y, z);
					break;
				case White:
					buffer.setPixel(tcu::UVec4(255, 255, 255, 255), x, y, z);
					break;
				case Red:
					buffer.setPixel(tcu::UVec4(255, 0, 0, 255), x, y, z);
					break;
				case Random:
					buffer.setPixel(tcu::UVec4(rnd.getUint8(), rnd.getUint8(), rnd.getUint8(), 255), x, y, z);
				default:
					break;
				}
			}
}

void CopiesAndBlittingTestInstance::uploadBuffer(tcu::ConstPixelBufferAccess src, const Allocation &bufferAlloc)
{
	const DeviceInterface&		vk			= m_context.getDeviceInterface();
	const VkDevice				vkDevice	= m_context.getDevice();
	const deUint32				bufferSize	= calculateSize(src);

	// Write buffer data
	deMemcpy(bufferAlloc.getHostPtr(), src.getDataPtr(), bufferSize);
	flushMappedMemoryRange(vk, vkDevice, bufferAlloc.getMemory(), bufferAlloc.getOffset(), bufferSize);
}

void CopiesAndBlittingTestInstance::uploadImage(tcu::ConstPixelBufferAccess src, const VkImage &image)
{
	const DeviceInterface&		vk				= m_context.getDeviceInterface();
	const VkDevice				vkDevice		= m_context.getDevice();
	const VkQueue				queue			= m_context.getUniversalQueue();
	SimpleAllocator				memAlloc		(vk, vkDevice, getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));

	Move<VkBuffer>				buffer;
	const deUint32				bufferSize		= calculateSize(src);
	de::MovePtr<Allocation>		bufferAlloc;
	Move<VkCommandBuffer>		cmdBuffer;
	Move<VkFence>				fence;

	// Create source buffer
	{
		const VkBufferCreateInfo			bufferParams			=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			0u,											// VkBufferCreateFlags	flags;
			bufferSize,									// VkDeviceSize			size;
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,			// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
			0u,											// deUint32				queueFamilyIndexCount;
			DE_NULL,									// const deUint32*		pQueueFamilyIndices;
		};

		buffer		= createBuffer(vk, vkDevice, &bufferParams);
		bufferAlloc = memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *buffer), MemoryRequirement::HostVisible);
		VK_CHECK(vk.bindBufferMemory(vkDevice, *buffer, bufferAlloc->getMemory(), bufferAlloc->getOffset()));
	}

	// Create command buffer
	{
		const VkCommandBufferAllocateInfo	cmdBufferAllocateInfo	=
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,	// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			*m_cmdPool,										// VkCommandPool			commandPool;
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,				// VkCommandBufferLevel		level;
			1u,												// deUint32					bufferCount;
		};

		cmdBuffer = allocateCommandBuffer(vk, vkDevice, &cmdBufferAllocateInfo);
	}

	// Create fence
	{
		const VkFenceCreateInfo				fenceParams				=
		{
			VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			0u											// VkFenceCreateFlags	flags;
		};

		fence = createFence(vk, vkDevice, &fenceParams);
	}


	// Barriers for copying buffer to image
	const VkBufferMemoryBarrier				preBufferBarrier		=
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,		// VkStructureType	sType;
		DE_NULL,										// const void*		pNext;
		VK_ACCESS_HOST_WRITE_BIT,						// VkAccessFlags	srcAccessMask;
		VK_ACCESS_TRANSFER_READ_BIT,					// VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32			srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32			dstQueueFamilyIndex;
		*buffer,										// VkBuffer			buffer;
		0u,												// VkDeviceSize		offset;
		bufferSize										// VkDeviceSize		size;
	};

	const VkImageMemoryBarrier				preImageBarrier			=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		0u,												// VkAccessFlags			srcAccessMask;
		0u,												// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,			// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32					dstQueueFamilyIndex;
		image,											// VkImage					image;
		{												// VkImageSubresourceRange	subresourceRange;
			VK_IMAGE_ASPECT_COLOR_BIT,				// VkImageAspect	aspect;
			0u,										// deUint32			baseMipLevel;
			1u,										// deUint32			mipLevels;
			0u,										// deUint32			baseArraySlice;
			1u,										// deUint32			arraySize;
		}
	};

	const VkImageMemoryBarrier				postImageBarrier		=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,					// VkAccessFlags			srcAccessMask;
		VK_ACCESS_SHADER_READ_BIT,						// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,			// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,		// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32					dstQueueFamilyIndex;
		image,											// VkImage					image;
		{												// VkImageSubresourceRange	subresourceRange;
			VK_IMAGE_ASPECT_COLOR_BIT,				// VkImageAspect	aspect;
			0u,										// deUint32			baseMipLevel;
			1u,										// deUint32			mipLevels;
			0u,										// deUint32			baseArraySlice;
			1u,										// deUint32			arraySize;
		}
	};

	const void*								preCopyBarriers[2]		=
	{
		&preBufferBarrier,
		&preImageBarrier
	};
	const void* const						postCopyBarrier			= &postImageBarrier;

	const VkBufferImageCopy					copyRegion				=
	{
		0u,												// VkDeviceSize				bufferOffset;
		(deUint32)src.getWidth(),						// deUint32					bufferRowLength;
		(deUint32)src.getHeight(),						// deUint32					bufferImageHeight;
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u },		// VkImageSubresourceLayers	imageSubresource;
		{ 0, 0, 0 },									// VkOffset3D				imageOffset;
		{ src.getWidth(), src.getHeight(), 1u }			// VkExtent3D				imageExtent;
	};

	// Write buffer data
	deMemcpy(bufferAlloc->getHostPtr(), src.getDataPtr(), bufferSize);
	flushMappedMemoryRange(vk, vkDevice, bufferAlloc->getMemory(), bufferAlloc->getOffset(), bufferSize);

	// Copy buffer to image
	VkCommandBufferBeginInfo				cmdBufferBeginInfo		=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,			// VkStructureType					sType;
		DE_NULL,												// const void*						pNext;
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,			// VkCommandBufferUsageFlags		flags;
		DE_NULL,												// VkRenderPass						renderPass;
		0u,														// deUint32							subpass;
		DE_NULL,												// VkFramebuffer					framebuffer;
		false,													// VkBool32							occlusionQueryEnable;
		0u,														// VkQueryControlFlags				queryFlags;
		0u														// VkQueryPipelineStatisticFlags	pipelineStatistics;
	};

	VK_CHECK(vk.beginCommandBuffer(*cmdBuffer, &cmdBufferBeginInfo));
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_FALSE, 2u, preCopyBarriers);
	vk.cmdCopyBufferToImage(*cmdBuffer, *buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &copyRegion);
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_FALSE, 1u, &postCopyBarrier);
	VK_CHECK(vk.endCommandBuffer(*cmdBuffer));

	const VkSubmitInfo						submitInfo				=
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType			sType;
		DE_NULL,						// const void*				pNext;
		0u,								// deUint32					waitSemaphoreCount;
		DE_NULL,						// const VkSemaphore*		pWaitSemaphores;
		1u,								// deUint32					commandBufferCount;
		&cmdBuffer.get(),				// const VkCommandBuffer*	pCommandBuffers;
		0u,								// deUint32					signalSemaphoreCount;
		DE_NULL							// const VkSemaphore*		pSignalSemaphores;
	};

	VK_CHECK(vk.queueSubmit(queue, 1, &submitInfo, *fence));
	VK_CHECK(vk.waitForFences(vkDevice, 1, &fence.get(), true, ~(0ull) /* infinity */));
}

tcu::TestStatus CopiesAndBlittingTestInstance::checkTestResult(tcu::ConstPixelBufferAccess result)
{
	tcu::ConstPixelBufferAccess expected = m_expectedTextureLevel->getAccess();

	tcu::ImageIO::savePNG(m_sourceTextureLevel->getAccess(), "source.png");
	tcu::ImageIO::savePNG(m_destinationTextureLevel->getAccess(), "destination.png");
	tcu::ImageIO::savePNG(result, "result.png");
	tcu::ImageIO::savePNG(expected, "expected.png");
	if (expected.getWidth() != result.getWidth() || expected.getHeight() != result.getHeight() || expected.getDepth() != result.getDepth())
		return tcu::TestStatus::fail("The result's dimensions differs from the expected!");

	for (int x = 0; x < result.getWidth(); x++)
	{
		for (int y = 0; y < result.getHeight(); y++)
		{
			for (int z = 0; z < result.getDepth(); z++)
			{
				if (result.getPixel(x, y, z) != expected.getPixel(x, y, z))
					return tcu::TestStatus::fail("The result differs from the expected!");
			}
		}
	}

	return tcu::TestStatus::pass("CopiesAndBlitting test");
}

void CopiesAndBlittingTestInstance::generateExpectedResult()
{
	tcu::ConstPixelBufferAccess src = m_sourceTextureLevel->getAccess();
	tcu::ConstPixelBufferAccess dst = m_destinationTextureLevel->getAccess();

	m_expectedTextureLevel = de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(dst.getFormat(), dst.getWidth(), dst.getHeight(), dst.getDepth()));
	tcu::copy(m_expectedTextureLevel->getAccess(), dst);
	for (deUint32 i = 0; i < m_regions.size(); i++)
		copyRegionToTextureLevel(src, m_expectedTextureLevel->getAccess(), m_regions[i]);
}

tcu::TestStatus CopiesAndBlittingTestInstance::iterate (void)
{
	return tcu::TestStatus::pass("CopiesAndBlitting test");
}

class CopiesAndBlittingTestCase : public vkt::TestCase
{
public:
							CopiesAndBlittingTestCase	(tcu::TestContext&			testCtx,
														 const std::string&			name,
														 const std::string&			description)
								: vkt::TestCase			(testCtx, name, description)
							{}

	virtual					~CopiesAndBlittingTestCase	(void) {}
	virtual	void			initPrograms				(SourceCollections&			programCollection) const
							{}

	virtual TestInstance*	createInstance				(Context&					context) const = 0;
};

de::MovePtr<tcu::TextureLevel> CopiesAndBlittingTestInstance::readImage	(const vk::DeviceInterface&	vk,
																			vk::VkDevice				device,
																			vk::VkQueue					queue,
																			vk::Allocator&				allocator,
																			vk::VkImage					image,
																			vk::VkFormat				format,
																			const VkExtent3D			imageSize)
{
	Move<VkBuffer>					buffer;
	de::MovePtr<Allocation>			bufferAlloc;
	Move<VkCommandBuffer>			cmdBuffer;
	Move<VkFence>					fence;
	const tcu::TextureFormat		tcuFormat		= mapVkFormat(format);
	const VkDeviceSize				pixelDataSize	= imageSize.width * imageSize.height * imageSize.depth * tcu::getPixelSize(tcuFormat);
	de::MovePtr<tcu::TextureLevel>	resultLevel		(new tcu::TextureLevel(tcuFormat, imageSize.width, imageSize.height, imageSize.depth));

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

		buffer		= createBuffer(vk, device, &bufferParams);
		bufferAlloc = allocator.allocate(getBufferMemoryRequirements(vk, device, *buffer), MemoryRequirement::HostVisible);
		VK_CHECK(vk.bindBufferMemory(device, *buffer, bufferAlloc->getMemory(), bufferAlloc->getOffset()));
	}

	// Create command pool and buffer
	{
		const VkCommandBufferAllocateInfo cmdBufferAllocateInfo =
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,	// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			*m_cmdPool,										// VkCommandPool			commandPool;
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,				// VkCommandBufferLevel		level;
			1u												// deUint32					bufferCount;
		};

		cmdBuffer = allocateCommandBuffer(vk, device, &cmdBufferAllocateInfo);
	}

	// Create fence
	{
		const VkFenceCreateInfo fenceParams =
		{
			VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			0u											// VkFenceCreateFlags	flags;
		};

		fence = createFence(vk, device, &fenceParams);
	}

	// Barriers for copying image to buffer

	const VkImageMemoryBarrier imageBarrier =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,		// VkAccessFlags			srcAccessMask;
		VK_ACCESS_TRANSFER_READ_BIT,				// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,		// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
		image,										// VkImage					image;
		{											// VkImageSubresourceRange	subresourceRange;
			VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
			0u,							// deUint32				baseMipLevel;
			1u,							// deUint32				mipLevels;
			0u,							// deUint32				baseArraySlice;
			1u							// deUint32				arraySize;
		}
	};

	const VkBufferMemoryBarrier bufferBarrier =
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType	sType;
		DE_NULL,									// const void*		pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags	srcAccessMask;
		VK_ACCESS_HOST_READ_BIT,					// VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			dstQueueFamilyIndex;
		*buffer,									// VkBuffer			buffer;
		0u,											// VkDeviceSize		offset;
		pixelDataSize								// VkDeviceSize		size;
	};

	const void* const	imageBarrierPtr		= &imageBarrier;
	const void* const	bufferBarrierPtr	= &bufferBarrier;

	// Copy image to buffer

	const VkBufferImageCopy copyRegion =
	{
		0u,											// VkDeviceSize				bufferOffset;
		(deUint32)imageSize.width,				// deUint32					bufferRowLength;
		(deUint32)imageSize.height,				// deUint32					bufferImageHeight;
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u },	// VkImageSubresourceLayers	imageSubresource;
		{ 0, 0, 0 },								// VkOffset3D				imageOffset;
		imageSize									// VkExtent3D				imageExtent;
	};

	VK_CHECK(vk.beginCommandBuffer(*cmdBuffer, &m_cmdBufferBeginInfo));
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_FALSE, 1, &imageBarrierPtr);
	vk.cmdCopyImageToBuffer(*cmdBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *buffer, 1, &copyRegion);
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_FALSE, 1, &bufferBarrierPtr);
	VK_CHECK(vk.endCommandBuffer(*cmdBuffer));

	const VkSubmitInfo submitInfo =
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType			sType;
		DE_NULL,						// const void*				pNext;
		0u,								// deUint32					waitSemaphoreCount;
		DE_NULL,						// const VkSemaphore*		pWaitSemaphores;
		1u,								// deUint32					commandBufferCount;
		&cmdBuffer.get(),				// const VkCommandBuffer*	pCommandBuffers;
		0u,								// deUint32					signalSemaphoreCount;
		DE_NULL							// const VkSemaphore*		pSignalSemaphores;
	};

	VK_CHECK(vk.queueSubmit(queue, 1, &submitInfo, *fence));
	VK_CHECK(vk.waitForFences(device, 1, &fence.get(), 0, ~(0ull) /* infinity */));

	// Read buffer data
	invalidateMappedMemoryRange(vk, device, bufferAlloc->getMemory(), bufferAlloc->getOffset(), pixelDataSize);
	tcu::copy(*resultLevel, tcu::ConstPixelBufferAccess(resultLevel->getFormat(), resultLevel->getSize(), bufferAlloc->getHostPtr()));

	return resultLevel;
}

// Copy from image to image.

class ImageToImageCopies : public CopiesAndBlittingTestInstance
{
public:
										ImageToImageCopies		(Context&	context,
																 TestParams params);
	//virtual								~ImageToImageCopies		(void);
	virtual tcu::TestStatus				iterate					(void);
private:
	Move<VkImage>						m_source;
	de::MovePtr<Allocation>				m_sourceImageAlloc;
	Move<VkImage>						m_destination;
	de::MovePtr<Allocation>				m_destinationImageAlloc;

	virtual void						copyRegionToTextureLevel			(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst, CopyRegion region);
};

ImageToImageCopies::ImageToImageCopies (Context &context, TestParams params)
	: CopiesAndBlittingTestInstance(context, params)
{
	const DeviceInterface&		vk					= context.getDeviceInterface();
	const VkDevice				vkDevice			= context.getDevice();
	const deUint32				queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	SimpleAllocator				memAlloc			(vk, vkDevice, getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice()));

	const VkImageCreateInfo sourceImageParams =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType		sType;
		DE_NULL,								// const void*			pNext;
		0u,										// VkImageCreateFlags	flags;
		VK_IMAGE_TYPE_2D,						// VkImageType			imageType;
		m_params.src.image.format,				// VkFormat				format;
		m_params.src.image.extent,				// VkExtent3D			extent;
		1u,										// deUint32				mipLevels;
		1u,										// deUint32				arraySize;
		VK_SAMPLE_COUNT_1_BIT,					// deUint32				samples;
		VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling		tiling;
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT,		// VkImageUsageFlags	usage;
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
		1u,										// deUint32				queueFamilyCount;
		&queueFamilyIndex,						// const deUint32*		pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout		initialLayout;
	};

	m_source = createImage(vk, vkDevice, &sourceImageParams);
	m_sourceImageAlloc = memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_source), MemoryRequirement::Any);
	VK_CHECK(vk.bindImageMemory(vkDevice, *m_source, m_sourceImageAlloc->getMemory(), m_sourceImageAlloc->getOffset()));

	const VkImageCreateInfo destinationImageParams =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType		sType;
		DE_NULL,								// const void*			pNext;
		0u,										// VkImageCreateFlags	flags;
		VK_IMAGE_TYPE_2D,						// VkImageType			imageType;
		m_params.dst.image.format,				// VkFormat				format;
		m_params.dst.image.extent,				// VkExtent3D			extent;
		1u,										// deUint32				mipLevels;
		1u,										// deUint32				arraySize;
		VK_SAMPLE_COUNT_1_BIT,					// deUint32				samples;
		VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling		tiling;
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,		// VkImageUsageFlags	usage;
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
		1u,										// deUint32				queueFamilyCount;
		&queueFamilyIndex,						// const deUint32*		pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,	// VkImageLayout		initialLayout;
	};

	m_destination = createImage(vk, vkDevice, &destinationImageParams);
	m_destinationImageAlloc = memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_destination), MemoryRequirement::Any);
	VK_CHECK(vk.bindImageMemory(vkDevice, *m_destination, m_destinationImageAlloc->getMemory(), m_destinationImageAlloc->getOffset()));

	{
		const VkImageSubresourceLayers sourceLayer =
		{
			VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
			0u,							// uint32_t				mipLevel;
			0u,							// uint32_t				baseArrayLayer;
			1u							// uint32_t				layerCount;
		};
		const VkImageCopy testCopy =
		{
			sourceLayer,	// VkImageSubresourceLayers	srcSubresource;
			{16, 16, 0},		// VkOffset3D				srcOffset;
			sourceLayer,	// VkImageSubresourceLayers	dstSubresource;
			{0, 0, 0},		// VkOffset3D				dstOffset;
			{16, 16, 1},	// VkExtent3D				extent;
		};

		CopyRegion imageCopy;
		imageCopy.imageCopy = testCopy;

		m_regions.push_back(imageCopy);
	}

	{
		const VkImageSubresourceLayers sourceLayer =
		{
			VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
			0u,							// uint32_t				mipLevel;
			0u,							// uint32_t				baseArrayLayer;
			1u							// uint32_t				layerCount;
		};
		const VkImageCopy testCopy =
		{
			sourceLayer,	// VkImageSubresourceLayers	srcSubresource;
			{16, 16, 0},	// VkOffset3D				srcOffset;
			sourceLayer,	// VkImageSubresourceLayers	dstSubresource;
			{88, 3, 0},		// VkOffset3D				dstOffset;
			{16, 88, 1},	// VkExtent3D				extent;
		};

		CopyRegion imageCopy;
		imageCopy.imageCopy = testCopy;

		m_regions.push_back(imageCopy);
	}
}

tcu::TestStatus ImageToImageCopies::iterate()
{
	m_sourceTextureLevel = de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(mapVkFormat(m_params.src.image.format),
																				m_params.src.image.extent.width,
																				m_params.src.image.extent.height,
																				m_params.src.image.extent.depth));
	generateBuffer(m_sourceTextureLevel->getAccess(), m_params.src.image.extent.width, m_params.src.image.extent.height, m_params.src.image.extent.depth);
	m_destinationTextureLevel = de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(mapVkFormat(m_params.dst.image.format),
																					 (int)m_params.dst.image.extent.width,
																					 (int)m_params.dst.image.extent.height,
																					 (int)m_params.dst.image.extent.depth));
	generateBuffer(m_destinationTextureLevel->getAccess(), m_params.dst.image.extent.width, m_params.dst.image.extent.height, m_params.dst.image.extent.depth);
	generateExpectedResult();

	uploadImage(m_sourceTextureLevel->getAccess(), m_source.get());
	uploadImage(m_destinationTextureLevel->getAccess(), m_destination.get());

	const DeviceInterface&		vk					= m_context.getDeviceInterface();
	const VkDevice				vkDevice			= m_context.getDevice();
	const VkQueue				queue				= m_context.getUniversalQueue();
	SimpleAllocator				memAlloc			(vk, vkDevice, getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));

	VkImageCopy* imageCopies = ((VkImageCopy*)deMalloc(m_regions.size() * sizeof(VkImageCopy)));
	for (deUint32 i = 0; i < m_regions.size(); i++)
		imageCopies[i] = m_regions[i].imageCopy;

	// Barriers for copying image to buffer
	const VkImageMemoryBarrier srcImageBarrier =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,		// VkAccessFlags			srcAccessMask;
		VK_ACCESS_TRANSFER_READ_BIT,				// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,		// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
		m_source.get(),								// VkImage					image;
		{											// VkImageSubresourceRange	subresourceRange;
			VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
			0u,							// deUint32				baseMipLevel;
			1u,							// deUint32				mipLevels;
			0u,							// deUint32				baseArraySlice;
			1u							// deUint32				arraySize;
		}
	};

	const VkImageMemoryBarrier dstImageBarrier =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,		// VkAccessFlags			srcAccessMask;
		VK_ACCESS_TRANSFER_READ_BIT,				// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
		m_destination.get(),						// VkImage					image;
		{											// VkImageSubresourceRange	subresourceRange;
			VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
			0u,							// deUint32				baseMipLevel;
			1u,							// deUint32				mipLevels;
			0u,							// deUint32				baseArraySlice;
			1u							// deUint32				arraySize;
		}
	};

	const void* const	srcImageBarrierPtr		= &srcImageBarrier;
	const void* const	dstImageBarrierPtr		= &dstImageBarrier;

	VK_CHECK(vk.beginCommandBuffer(*m_cmdBuffer, &m_cmdBufferBeginInfo));
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_FALSE, 1, &srcImageBarrierPtr);
	vk.cmdCopyImage(*m_cmdBuffer, m_source.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_destination.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, m_regions.size(), imageCopies);
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_FALSE, 1, &dstImageBarrierPtr);
	VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));

	const VkSubmitInfo submitInfo =
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType			sType;
		DE_NULL,						// const void*				pNext;
		0u,								// deUint32					waitSemaphoreCount;
		DE_NULL,						// const VkSemaphore*		pWaitSemaphores;
		1u,								// deUint32					commandBufferCount;
		&m_cmdBuffer.get(),				// const VkCommandBuffer*	pCommandBuffers;
		0u,								// deUint32					signalSemaphoreCount;
		DE_NULL							// const VkSemaphore*		pSignalSemaphores;
	};

	VK_CHECK(vk.resetFences(vkDevice, 1, &m_fence.get()));
	VK_CHECK(vk.queueSubmit(queue, 1, &submitInfo, *m_fence));
	VK_CHECK(vk.waitForFences(vkDevice, 1, &m_fence.get(), true, ~(0ull) /* infinity */));
	deFree(imageCopies);

	de::MovePtr<tcu::TextureLevel> resultTextureLevel = readImage(vk, vkDevice, queue, memAlloc, *m_destination, m_params.dst.image.format, m_params.dst.image.extent);


	return checkTestResult(resultTextureLevel->getAccess());
}

void ImageToImageCopies::copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst, CopyRegion region)
{
	VkOffset3D srcOffset	= region.imageCopy.srcOffset;
	VkOffset3D dstOffset	= region.imageCopy.dstOffset;
	VkExtent3D extent		= region.imageCopy.extent;

	for (int x = 0; x < extent.width; x++)
		for (int y = 0; y < extent.height; y++)
			for (int z = 0; z < extent.depth; z++)
				dst.setPixel(src.getPixel(srcOffset.x + x, srcOffset.y + y, srcOffset.z + z), dstOffset.x + x, dstOffset.y + y, dstOffset.z + z);
}

class ImageToImageTestCase : public vkt::TestCase
{
public:
							ImageToImageTestCase		(tcu::TestContext&				testCtx,
														 const std::string&				name,
														 const std::string&				description,
														 const TestParams				params)
								: vkt::TestCase			(testCtx, name, description)
								, m_params				(params)
							{}

	virtual					~ImageToImageTestCase		(void) {}
	virtual	void			initPrograms				(SourceCollections&				programCollection) const
							{}

	virtual TestInstance*	createInstance				(Context&						context) const
							{
								return new ImageToImageCopies(context, m_params);
							}
private:
	TestParams				m_params;
};

// Copy from image to buffer.

class CopyImageToBuffer : public CopiesAndBlittingTestInstance
{
public:
								CopyImageToBuffer	(Context&	context,
													 TestParams	testParams);
	virtual tcu::TestStatus		iterate				(void);
private:
	virtual void				copyRegionToTextureLevel	(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst, CopyRegion region);

	VkDeviceSize				m_pixelDataSize;

	Move<VkImage>				m_source;
	de::MovePtr<Allocation>		m_sourceImageAlloc;
	Move<VkBuffer>				m_destination;
	de::MovePtr<Allocation>		m_destinationBufferAlloc;
};

CopyImageToBuffer::CopyImageToBuffer (Context &context, TestParams testParams)
	: CopiesAndBlittingTestInstance(context, testParams)
	, m_pixelDataSize(m_params.src.image.extent.width * m_params.src.image.extent.height * m_params.src.image.extent.depth * mapVkFormat(m_params.src.image.format).getPixelSize())
{
	const DeviceInterface&		vk					= context.getDeviceInterface();
	const VkDevice				vkDevice			= context.getDevice();
	const deUint32				queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	SimpleAllocator				memAlloc			(vk, vkDevice, getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice()));

	// Create source image
	{
		const VkImageCreateInfo		sourceImageParams		=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,								// const void*			pNext;
			0u,										// VkImageCreateFlags	flags;
			VK_IMAGE_TYPE_2D,						// VkImageType			imageType;
			m_params.src.image.format,				// VkFormat				format;
			m_params.src.image.extent,				// VkExtent3D			extent;
			1u,										// deUint32				mipLevels;
			1u,										// deUint32				arraySize;
			VK_SAMPLE_COUNT_1_BIT,					// deUint32				samples;
			VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling		tiling;
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
				VK_IMAGE_USAGE_TRANSFER_DST_BIT,	// VkImageUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
			1u,										// deUint32				queueFamilyCount;
			&queueFamilyIndex,						// const deUint32*		pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout		initialLayout;
		};

		m_source			= createImage(vk, vkDevice, &sourceImageParams);
		m_sourceImageAlloc	= memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_source), MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(vkDevice, *m_source, m_sourceImageAlloc->getMemory(), m_sourceImageAlloc->getOffset()));
	}

	// Create destination buffer
	{
		const VkBufferCreateInfo	destinationBufferParams	=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			0u,											// VkBufferCreateFlags	flags;
			m_pixelDataSize,							// VkDeviceSize			size;
			VK_BUFFER_USAGE_TRANSFER_DST_BIT,			// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
			0u,											// deUint32				queueFamilyIndexCount;
			DE_NULL										// const deUint32*		pQueueFamilyIndices;
		};

		m_destination				= createBuffer(vk, vkDevice, &destinationBufferParams);
		m_destinationBufferAlloc	= memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_destination), MemoryRequirement::HostVisible);
		VK_CHECK(vk.bindBufferMemory(vkDevice, *m_destination, m_destinationBufferAlloc->getMemory(), m_destinationBufferAlloc->getOffset()));
	}
}

tcu::TestStatus CopyImageToBuffer::iterate()
{
//	uploadImage(m_sourceTextureLevel->getAccess(), *m_source);
//	uploadBuffer(m_destinationTextureLevel->getAccess(), *m_destinationBufferAlloc);
//	generateExpectedResult();

	const DeviceInterface&		vk			= m_context.getDeviceInterface();
	const VkDevice				vkDevice	= m_context.getDevice();
	const VkQueue				queue		= m_context.getUniversalQueue();

	// Barriers for copying image to buffer
	const VkImageMemoryBarrier imageBarrier =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,		// VkAccessFlags			srcAccessMask;
		VK_ACCESS_TRANSFER_READ_BIT,				// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,		// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
		*m_source,									// VkImage					image;
		{											// VkImageSubresourceRange	subresourceRange;
			VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
			0u,							// deUint32				baseMipLevel;
			1u,							// deUint32				mipLevels;
			0u,							// deUint32				baseArraySlice;
			1u							// deUint32				arraySize;
		}
	};

	const VkBufferMemoryBarrier bufferBarrier =
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType	sType;
		DE_NULL,									// const void*		pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags	srcAccessMask;
		VK_ACCESS_HOST_READ_BIT,					// VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			dstQueueFamilyIndex;
		*m_destination,								// VkBuffer			buffer;
		0u,											// VkDeviceSize		offset;
		m_pixelDataSize								// VkDeviceSize		size;
	};

	const void* const	imageBarrierPtr		= &imageBarrier;
	const void* const	bufferBarrierPtr	= &bufferBarrier;

	// Copy from image to buffer
	const VkImageSubresourceLayers	sourceLayer		=
	{
		VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
		0u,							// uint32_t				mipLevel;
		0u,							// uint32_t				baseArrayLayer;
		1u,							// uint32_t				layerCount;
	};

	const VkBufferImageCopy			bufferImageCopy	=
	{
		0u,						// VkDeviceSize				bufferOffset;
		0u,						// uint32_t					bufferRowLength;
		0u,						// uint32_t					bufferImageHeight;
		sourceLayer,			// VkImageSubresourceLayers	imageSubresource;
		{0, 0, 0},				// VkOffset3D				imageOffset;
		{16, 16, 1}				// VkExtent3D				imageExtent;
	};

	VK_CHECK(vk.beginCommandBuffer(*m_cmdBuffer, &m_cmdBufferBeginInfo));
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_FALSE, 1, &imageBarrierPtr);
	vk.cmdCopyImageToBuffer(*m_cmdBuffer, m_source.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_destination.get(), 1u, &bufferImageCopy);
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_FALSE, 1, &bufferBarrierPtr);
	VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));

	const VkSubmitInfo				submitInfo		=
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType			sType;
		DE_NULL,						// const void*				pNext;
		0u,								// deUint32					waitSemaphoreCount;
		DE_NULL,						// const VkSemaphore*		pWaitSemaphores;
		1u,								// deUint32					commandBufferCount;
		&m_cmdBuffer.get(),				// const VkCommandBuffer*	pCommandBuffers;
		0u,								// deUint32					signalSemaphoreCount;
		DE_NULL							// const VkSemaphore*		pSignalSemaphores;
	};

	VK_CHECK(vk.resetFences(vkDevice, 1, &m_fence.get()));
	VK_CHECK(vk.queueSubmit(queue, 1, &submitInfo, *m_fence));
	VK_CHECK(vk.waitForFences(vkDevice, 1, &m_fence.get(), true, ~(0ull) /* infinity */));

	// Read buffer data
	de::MovePtr<tcu::TextureLevel>	resultLevel		(new tcu::TextureLevel(mapVkFormat(m_params.src.image.format), m_params.src.image.extent.width, m_params.src.image.extent.height));
	invalidateMappedMemoryRange(vk, vkDevice, m_destinationBufferAlloc->getMemory(), m_destinationBufferAlloc->getOffset(), m_pixelDataSize);
	tcu::copy(*resultLevel, tcu::ConstPixelBufferAccess(resultLevel->getFormat(), resultLevel->getSize(), m_destinationBufferAlloc->getHostPtr()));

	return checkTestResult(resultLevel->getAccess());
}

class CopyImageToBufferTestCase : public vkt::TestCase
{
public:
							CopyImageToBufferTestCase	(tcu::TestContext&		testCtx,
														 const std::string&		name,
														 const std::string&		description,
														 const TestParams		params)
								: vkt::TestCase			(testCtx, name, description)
								, m_params				(params)
							{}

	virtual					~CopyImageToBufferTestCase	(void) {}
	virtual	void			initPrograms				(SourceCollections&		programCollection) const
							{}

	virtual TestInstance*	createInstance				(Context&				context) const
							{
								return new CopyImageToBuffer(context, m_params);
							}
private:
	TestParams				m_params;
};

void CopyImageToBuffer::copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst, CopyRegion region)
{
	deUint32 rowLength = region.bufferToImageCopy.bufferRowLength;
	if (!rowLength)
		rowLength = region.bufferToImageCopy.imageExtent.width;

	deUint32 imageHeight = region.bufferToImageCopy.bufferImageHeight;
	if (!imageHeight)
		imageHeight = region.bufferToImageCopy.imageExtent.height;

	deUint32 texelSize = src.getFormat().getPixelSize();
	VkExtent3D extent = region.bufferToImageCopy.imageExtent;
	VkOffset3D srcOffset = region.bufferToImageCopy.imageOffset;

	void* bufferData = dst.getDataPtr();
	for (int x = 0; x < extent.width; x++)
	{
		for (int y = 0; y < extent.height; y++)
		{
			for (int z = 0; z < extent.depth; z++)
			{
				vk::VkDeviceSize pixelIndex = region.bufferToImageCopy.bufferOffset + (((z * imageHeight) + y) * rowLength + x) * texelSize;
				void* pixelPtr = (deUint8*) bufferData + pixelIndex;
				deMemcpy(pixelPtr, src.getPixelPtr(srcOffset.x + x, srcOffset.y + y, srcOffset.z + z), texelSize);
			}
		}
	}
}

} // anonymous

tcu::TestCaseGroup* createCopiesAndBlittingTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	copiesAndBlittingTests	(new tcu::TestCaseGroup(testCtx, "access", "Copies And Blitting Tests"));

	const VkExtent3D defaultExtent = {256, 256, 1};

	{
		std::ostringstream description;
		description << "Copy from image to image";

		TestParams	params;
		params.src.image.format	= VK_FORMAT_R8G8B8A8_UINT;
		params.src.image.extent	= defaultExtent;
		params.dst.image.format	= VK_FORMAT_R8G8B8A8_UINT;
		params.dst.image.extent	= defaultExtent;

		copiesAndBlittingTests->addChild(new ImageToImageTestCase(testCtx, "imageToImageWhole", description.str(), params));
	}

	// Copy image to Buffer testcases.
	{
		std::ostringstream	description;
		description << "Copy from image to buffer";

		TestParams	params;
		params.src.image.format	= VK_FORMAT_R8G8B8A8_UINT;
		params.src.image.extent	= defaultExtent;
		params.dst.buffer.size	= 19850123;

		copiesAndBlittingTests->addChild(new CopyImageToBufferTestCase(testCtx, "imageToBuffer", description.str(), params));
	}

	return copiesAndBlittingTests.release();
}

} // api
} // vkt
