/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
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
#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVectorType.hpp"
#include "tcuTexture.hpp"

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
	VkBufferImageCopy	bufferImageCopy;
	VkImageBlit			imageBlit;
};

struct TestParams
{
	union Data
	{
		struct Buffer
		{
			VkDeviceSize	size;
		}	buffer;
		struct Image
		{
			VkFormat		format;
			VkExtent3D		extent;
		}	image;
	}	src, dst;

	std::vector<CopyRegion>	regions;
};

class CopiesAndBlittingTestInstance : public vkt::TestInstance
{
public:
										CopiesAndBlittingTestInstance		(Context&	context,
																			 TestParams	testParams);
	virtual								~CopiesAndBlittingTestInstance		(void);
	virtual tcu::TestStatus				iterate								(void) = 0;
	enum FillMode
	{
		FILL_MODE_SEQUENTIAL = 0,
		FILL_MODE_RANDOM,
		FILL_MODE_WHITE,
		FILL_MODE_RED,

		FILL_MODE_LAST
	};
protected:
	const TestParams					m_params;

	Move<VkCommandPool>					m_cmdPool;
	Move<VkCommandBuffer>				m_cmdBuffer;
	Move<VkFence>						m_fence;
	de::MovePtr<tcu::TextureLevel>		m_sourceTextureLevel;
	de::MovePtr<tcu::TextureLevel>		m_destinationTextureLevel;
	de::MovePtr<tcu::TextureLevel>		m_expectedTextureLevel;

	VkCommandBufferBeginInfo			m_cmdBufferBeginInfo;

	void								generateBuffer						(tcu::PixelBufferAccess buffer, int width, int height, int depth = 1, FillMode = FILL_MODE_SEQUENTIAL);
	virtual void						generateExpectedResult				(void);
	void								uploadBuffer						(tcu::ConstPixelBufferAccess bufferAccess, const Allocation& bufferAlloc);
	void								uploadImage							(tcu::ConstPixelBufferAccess imageAccess, const VkImage& image);
	virtual tcu::TestStatus				checkTestResult						(tcu::ConstPixelBufferAccess result);
	virtual void						copyRegionToTextureLevel			(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst, CopyRegion region) = 0;
	VkImageAspectFlags					getAspectFlag						(tcu::TextureFormat format);
	deUint32							calculateSize						(tcu::ConstPixelBufferAccess src) const
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
	for (int z = 0; z < depth; z++)
	{
		for (int y = 0; y < height; y++)
		{
			for (int x = 0; x < width; x++)
			{
				switch (mode)
				{
					case FILL_MODE_SEQUENTIAL:
						buffer.setPixel(tcu::UVec4(x, y, z, 255), x, y, z);
						break;
					case FILL_MODE_WHITE:
						buffer.setPixel(tcu::UVec4(255, 255, 255, 255), x, y, z);
						break;
					case FILL_MODE_RED:
						buffer.setPixel(tcu::UVec4(255, 0, 0, 255), x, y, z);
						break;
					case FILL_MODE_RANDOM:
						buffer.setPixel(tcu::UVec4(rnd.getUint8(), rnd.getUint8(), rnd.getUint8(), 255), x, y, z);
					default:
						break;
				}
			}
		}
	}
}

void CopiesAndBlittingTestInstance::uploadBuffer(tcu::ConstPixelBufferAccess bufferAccess, const Allocation& bufferAlloc)
{
	const DeviceInterface&		vk			= m_context.getDeviceInterface();
	const VkDevice				vkDevice	= m_context.getDevice();
	const deUint32				bufferSize	= calculateSize(bufferAccess);

	// Write buffer data
	deMemcpy(bufferAlloc.getHostPtr(), bufferAccess.getDataPtr(), bufferSize);
	flushMappedMemoryRange(vk, vkDevice, bufferAlloc.getMemory(), bufferAlloc.getOffset(), bufferSize);
}

void CopiesAndBlittingTestInstance::uploadImage(tcu::ConstPixelBufferAccess imageAccess, const VkImage& image)
{
	const DeviceInterface&		vk					= m_context.getDeviceInterface();
	const VkDevice				vkDevice			= m_context.getDevice();
	const VkQueue				queue				= m_context.getUniversalQueue();
	const deUint32				queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	SimpleAllocator				memAlloc			(vk, vkDevice, getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));

	Move<VkBuffer>				buffer;
	const deUint32				bufferSize		= calculateSize(imageAccess);
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
			1u,											// deUint32				queueFamilyIndexCount;
			&queueFamilyIndex,							// const deUint32*		pQueueFamilyIndices;
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
		VK_ACCESS_TRANSFER_WRITE_BIT,					// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,			// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32					dstQueueFamilyIndex;
		image,											// VkImage					image;
		{												// VkImageSubresourceRange	subresourceRange;
			getAspectFlag(imageAccess.getFormat()),	// VkImageAspect	aspect;
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
		VK_IMAGE_LAYOUT_GENERAL,						// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32					dstQueueFamilyIndex;
		image,											// VkImage					image;
		{												// VkImageSubresourceRange	subresourceRange;
			getAspectFlag(imageAccess.getFormat()),	// VkImageAspect	aspect;
			0u,										// deUint32			baseMipLevel;
			1u,										// deUint32			mipLevels;
			0u,										// deUint32			baseArraySlice;
			1u,										// deUint32			arraySize;
		}
	};

	const VkBufferImageCopy					copyRegion				=
	{
		0u,												// VkDeviceSize				bufferOffset;
		(deUint32)imageAccess.getWidth(),				// deUint32					bufferRowLength;
		(deUint32)imageAccess.getHeight(),				// deUint32					bufferImageHeight;
		{												// VkImageSubresourceLayers	imageSubresource;
			getAspectFlag(imageAccess.getFormat()),	// VkImageAspect	aspect;
			0u,										// deUint32			mipLevel;
			0u,										// deUint32			baseArrayLayer;
			1u,										// deUint32			layerCount;
		},
		{ 0, 0, 0 },									// VkOffset3D				imageOffset;
		{
			(deUint32)imageAccess.getWidth(),
			(deUint32)imageAccess.getHeight(),
			1u
		}												// VkExtent3D				imageExtent;
	};

	// Write buffer data
	deMemcpy(bufferAlloc->getHostPtr(), imageAccess.getDataPtr(), bufferSize);
	flushMappedMemoryRange(vk, vkDevice, bufferAlloc->getMemory(), bufferAlloc->getOffset(), bufferSize);

	// Copy buffer to image
	const VkCommandBufferBeginInfo			cmdBufferBeginInfo		=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,			// VkStructureType					sType;
		DE_NULL,												// const void*						pNext;
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,			// VkCommandBufferUsageFlags		flags;
		(const VkCommandBufferInheritanceInfo*)DE_NULL,
	};

	VK_CHECK(vk.beginCommandBuffer(*cmdBuffer, &cmdBufferBeginInfo));
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &preBufferBarrier, 1, &preImageBarrier);
	vk.cmdCopyBufferToImage(*cmdBuffer, *buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &copyRegion);
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &postImageBarrier);
	VK_CHECK(vk.endCommandBuffer(*cmdBuffer));

	const VkSubmitInfo						submitInfo				=
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType			sType;
		DE_NULL,						// const void*				pNext;
		0u,								// deUint32					waitSemaphoreCount;
		DE_NULL,						// const VkSemaphore*		pWaitSemaphores;
		(const VkPipelineStageFlags*)DE_NULL,
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
	const tcu::ConstPixelBufferAccess	expected	= m_expectedTextureLevel->getAccess();
	const tcu::UVec4					treshold	(0, 0, 0, 0);

	if (!tcu::intThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparsion", expected, result, treshold, tcu::COMPARE_LOG_RESULT))
		return tcu::TestStatus::fail("CopiesAndBlitting test");

	return tcu::TestStatus::pass("CopiesAndBlitting test");
}

void CopiesAndBlittingTestInstance::generateExpectedResult()
{
	const tcu::ConstPixelBufferAccess src = m_sourceTextureLevel->getAccess();
	const tcu::ConstPixelBufferAccess dst = m_destinationTextureLevel->getAccess();

	m_expectedTextureLevel = de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(dst.getFormat(), dst.getWidth(), dst.getHeight(), dst.getDepth()));
	tcu::copy(m_expectedTextureLevel->getAccess(), dst);
	for (deUint32 i = 0; i < m_params.regions.size(); i++)
		copyRegionToTextureLevel(src, m_expectedTextureLevel->getAccess(), m_params.regions[i]);
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

	virtual TestInstance*	createInstance				(Context&					context) const = 0;
};

VkImageAspectFlags CopiesAndBlittingTestInstance::getAspectFlag(tcu::TextureFormat format)
{
	VkImageAspectFlags aspectFlag = 0;
	aspectFlag |= (tcu::hasDepthComponent(format.order)? VK_IMAGE_ASPECT_DEPTH_BIT : 0);
	aspectFlag |= (tcu::hasStencilComponent(format.order)? VK_IMAGE_ASPECT_STENCIL_BIT : 0);

	if (!aspectFlag)
		aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;

	return aspectFlag;
}

de::MovePtr<tcu::TextureLevel> CopiesAndBlittingTestInstance::readImage	(const vk::DeviceInterface&	vk,
																		 vk::VkDevice				device,
																		 vk::VkQueue				queue,
																		 vk::Allocator&				allocator,
																		 vk::VkImage				image,
																		 vk::VkFormat				format,
																		 const VkExtent3D			imageSize)
{
	Move<VkBuffer>					buffer;
	de::MovePtr<Allocation>			bufferAlloc;
	Move<VkCommandBuffer>			cmdBuffer;
	Move<VkFence>					fence;
	const deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	const tcu::TextureFormat		tcuFormat			= mapVkFormat(format);
	const VkDeviceSize				pixelDataSize		= imageSize.width * imageSize.height * imageSize.depth * tcu::getPixelSize(tcuFormat);
	de::MovePtr<tcu::TextureLevel>	resultLevel			(new tcu::TextureLevel(tcuFormat, imageSize.width, imageSize.height, imageSize.depth));

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
			1u,											// deUint32				queueFamilyIndexCount;
			&queueFamilyIndex,							// const deUint32*		pQueueFamilyIndices;
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
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			srcAccessMask;
		VK_ACCESS_TRANSFER_READ_BIT,				// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,		// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
		image,										// VkImage					image;
		{											// VkImageSubresourceRange	subresourceRange;
			getAspectFlag(tcuFormat),	// VkImageAspectFlags	aspectMask;
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

	// Copy image to buffer

	const VkBufferImageCopy copyRegion =
	{
		0u,											// VkDeviceSize				bufferOffset;
		(deUint32)imageSize.width,					// deUint32					bufferRowLength;
		(deUint32)imageSize.height,					// deUint32					bufferImageHeight;
		{ getAspectFlag(tcuFormat), 0u, 0u, 1u },	// VkImageSubresourceLayers	imageSubresource;
		{ 0, 0, 0 },								// VkOffset3D				imageOffset;
		imageSize									// VkExtent3D				imageExtent;
	};

	const VkCommandBufferBeginInfo cmdBufferBeginInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,			// VkStructureType					sType;
		DE_NULL,												// const void*						pNext;
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,			// VkCommandBufferUsageFlags		flags;
		(const VkCommandBufferInheritanceInfo*)DE_NULL,
	};

	VK_CHECK(vk.beginCommandBuffer(*cmdBuffer, &cmdBufferBeginInfo));
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &imageBarrier);
	vk.cmdCopyImageToBuffer(*cmdBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *buffer, 1, &copyRegion);
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &bufferBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
	VK_CHECK(vk.endCommandBuffer(*cmdBuffer));

	const VkSubmitInfo submitInfo =
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType			sType;
		DE_NULL,						// const void*				pNext;
		0u,								// deUint32					waitSemaphoreCount;
		DE_NULL,						// const VkSemaphore*		pWaitSemaphores;
		(const VkPipelineStageFlags*)DE_NULL,
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

class CopyImageToImage : public CopiesAndBlittingTestInstance
{
public:
										CopyImageToImage			(Context&	context,
																	 TestParams params);
	virtual tcu::TestStatus				iterate						(void);
private:
	Move<VkImage>						m_source;
	de::MovePtr<Allocation>				m_sourceImageAlloc;
	Move<VkImage>						m_destination;
	de::MovePtr<Allocation>				m_destinationImageAlloc;

	virtual void						copyRegionToTextureLevel	(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst, CopyRegion region);
};

CopyImageToImage::CopyImageToImage (Context& context, TestParams params)
	: CopiesAndBlittingTestInstance(context, params)
{
	const DeviceInterface&		vk					= context.getDeviceInterface();
	const VkDevice				vkDevice			= context.getDevice();
	const deUint32				queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	SimpleAllocator				memAlloc			(vk, vkDevice, getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice()));

	VkImageFormatProperties properties;
	if ((context.getInstanceInterface().getPhysicalDeviceImageFormatProperties (context.getPhysicalDevice(),
																				m_params.src.image.format,
																				VK_IMAGE_TYPE_2D,
																				VK_IMAGE_TILING_OPTIMAL,
																				VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 0,
																				&properties) == VK_ERROR_FORMAT_NOT_SUPPORTED) ||
		(context.getInstanceInterface().getPhysicalDeviceImageFormatProperties (context.getPhysicalDevice(),
																				m_params.dst.image.format,
																				VK_IMAGE_TYPE_2D,
																				VK_IMAGE_TILING_OPTIMAL,
																				VK_IMAGE_USAGE_TRANSFER_DST_BIT, 0,
																				&properties) == VK_ERROR_FORMAT_NOT_SUPPORTED))
	{
		TCU_THROW(NotSupportedError, "Format not supported");
	}

	// Create source image
	{
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
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
			    VK_IMAGE_USAGE_TRANSFER_DST_BIT,	// VkImageUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
			1u,										// deUint32				queueFamilyCount;
			&queueFamilyIndex,						// const deUint32*		pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout		initialLayout;
		};

		m_source = createImage(vk, vkDevice, &sourceImageParams);
		m_sourceImageAlloc = memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_source), MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(vkDevice, *m_source, m_sourceImageAlloc->getMemory(), m_sourceImageAlloc->getOffset()));
	}

	// Create destination image
	{
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
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
			    VK_IMAGE_USAGE_TRANSFER_DST_BIT,	// VkImageUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
			1u,										// deUint32				queueFamilyCount;
			&queueFamilyIndex,						// const deUint32*		pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout		initialLayout;
		};

		m_destination = createImage(vk, vkDevice, &destinationImageParams);
		m_destinationImageAlloc = memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_destination), MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(vkDevice, *m_destination, m_destinationImageAlloc->getMemory(), m_destinationImageAlloc->getOffset()));
	}
}

tcu::TestStatus CopyImageToImage::iterate()
{
	tcu::TextureFormat srcTcuFormat = mapVkFormat(m_params.src.image.format);
	tcu::TextureFormat dstTcuFormat = mapVkFormat(m_params.dst.image.format);
	m_sourceTextureLevel = de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(srcTcuFormat,
																				m_params.src.image.extent.width,
																				m_params.src.image.extent.height,
																				m_params.src.image.extent.depth));
	generateBuffer(m_sourceTextureLevel->getAccess(), m_params.src.image.extent.width, m_params.src.image.extent.height, m_params.src.image.extent.depth, FILL_MODE_WHITE);
	m_destinationTextureLevel = de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(dstTcuFormat,
																					 (int)m_params.dst.image.extent.width,
																					 (int)m_params.dst.image.extent.height,
																					 (int)m_params.dst.image.extent.depth));
	generateBuffer(m_destinationTextureLevel->getAccess(), m_params.dst.image.extent.width, m_params.dst.image.extent.height, m_params.dst.image.extent.depth, FILL_MODE_SEQUENTIAL);
	generateExpectedResult();

	uploadImage(m_sourceTextureLevel->getAccess(), m_source.get());
	uploadImage(m_destinationTextureLevel->getAccess(), m_destination.get());

	const DeviceInterface&		vk					= m_context.getDeviceInterface();
	const VkDevice				vkDevice			= m_context.getDevice();
	const VkQueue				queue				= m_context.getUniversalQueue();
	SimpleAllocator				memAlloc			(vk, vkDevice, getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));

	VkImageCopy* imageCopies = ((VkImageCopy*)deMalloc(m_params.regions.size() * sizeof(VkImageCopy)));
	for (deUint32 i = 0; i < m_params.regions.size(); i++)
		imageCopies[i] = m_params.regions[i].imageCopy;

	const VkImageMemoryBarrier imageBarriers[] =
	{
		// source image
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			srcAccessMask;
			VK_ACCESS_TRANSFER_READ_BIT,				// VkAccessFlags			dstAccessMask;
			VK_IMAGE_LAYOUT_GENERAL,					// VkImageLayout			oldLayout;
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,		// VkImageLayout			newLayout;
			VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
			m_source.get(),								// VkImage					image;
			{											// VkImageSubresourceRange	subresourceRange;
				getAspectFlag(srcTcuFormat),	// VkImageAspectFlags	aspectMask;
				0u,								// deUint32				baseMipLevel;
				1u,								// deUint32				mipLevels;
				0u,								// deUint32				baseArraySlice;
				1u								// deUint32				arraySize;
			}
		},
		// destination image
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			srcAccessMask;
			VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			dstAccessMask;
			VK_IMAGE_LAYOUT_GENERAL,					// VkImageLayout			oldLayout;
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			newLayout;
			VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
			m_destination.get(),						// VkImage					image;
			{											// VkImageSubresourceRange	subresourceRange;
				getAspectFlag(dstTcuFormat),	// VkImageAspectFlags	aspectMask;
				0u,								// deUint32				baseMipLevel;
				1u,								// deUint32				mipLevels;
				0u,								// deUint32				baseArraySlice;
				1u								// deUint32				arraySize;
			}
		},
	};

	const VkCommandBufferBeginInfo cmdBufferBeginInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,			// VkStructureType					sType;
		DE_NULL,												// const void*						pNext;
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,			// VkCommandBufferUsageFlags		flags;
		(const VkCommandBufferInheritanceInfo*)DE_NULL,
	};

	VK_CHECK(vk.beginCommandBuffer(*m_cmdBuffer, &cmdBufferBeginInfo));
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, DE_LENGTH_OF_ARRAY(imageBarriers), imageBarriers);
	vk.cmdCopyImage(*m_cmdBuffer, m_source.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_destination.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (deUint32)m_params.regions.size(), imageCopies);
	VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));

	const VkSubmitInfo submitInfo =
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType			sType;
		DE_NULL,						// const void*				pNext;
		0u,								// deUint32					waitSemaphoreCount;
		DE_NULL,						// const VkSemaphore*		pWaitSemaphores;
		(const VkPipelineStageFlags*)DE_NULL,
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

void CopyImageToImage::copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst, CopyRegion region)
{
	VkOffset3D srcOffset	= region.imageCopy.srcOffset;
	VkOffset3D dstOffset	= region.imageCopy.dstOffset;
	VkExtent3D extent		= region.imageCopy.extent;

	const tcu::ConstPixelBufferAccess	srcSubRegion = tcu::getSubregion(src, srcOffset.x, srcOffset.y, extent.width, extent.height);
	// CopyImage acts like a memcpy. Replace the destination format with the srcformat to use a memcpy.
	const tcu::PixelBufferAccess		dstWithSrcFormat(srcSubRegion.getFormat(), dst.getSize(), dst.getDataPtr());
	const tcu::PixelBufferAccess		dstSubRegion = tcu::getSubregion(dstWithSrcFormat, dstOffset.x, dstOffset.y, extent.width, extent.height);

	tcu::copy(dstSubRegion, srcSubRegion);
}

class CopyImageToImageTestCase : public vkt::TestCase
{
public:
							CopyImageToImageTestCase	(tcu::TestContext&				testCtx,
														 const std::string&				name,
														 const std::string&				description,
														 const TestParams				params)
								: vkt::TestCase			(testCtx, name, description)
								, m_params				(params)
							{}

	virtual					~CopyImageToImageTestCase	(void) {}

	virtual TestInstance*	createInstance				(Context&						context) const
							{
								return new CopyImageToImage(context, m_params);
							}
private:
	TestParams				m_params;
};

// Copy from buffer to buffer.

class CopyBufferToBuffer : public CopiesAndBlittingTestInstance
{
public:
								CopyBufferToBuffer			(Context& context, TestParams params);
	virtual tcu::TestStatus		iterate						(void);
private:
	virtual void				copyRegionToTextureLevel	(tcu::ConstPixelBufferAccess, tcu::PixelBufferAccess, CopyRegion);
	Move<VkBuffer>				m_source;
	de::MovePtr<Allocation>		m_sourceBufferAlloc;
	Move<VkBuffer>				m_destination;
	de::MovePtr<Allocation>		m_destinationBufferAlloc;
};

CopyBufferToBuffer::CopyBufferToBuffer (Context& context, TestParams params)
	: CopiesAndBlittingTestInstance	(context, params)
{
	const DeviceInterface&		vk					= context.getDeviceInterface();
	const VkDevice				vkDevice			= context.getDevice();
	const deUint32				queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	SimpleAllocator				memAlloc			(vk, vkDevice, getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice()));

	// Create source buffer
	{
		const VkBufferCreateInfo	sourceBufferParams	=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			0u,											// VkBufferCreateFlags	flags;
			m_params.src.buffer.size,					// VkDeviceSize			size;
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,			// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
			1u,											// deUint32				queueFamilyIndexCount;
			&queueFamilyIndex,							// const deUint32*		pQueueFamilyIndices;
		};

		m_source				= createBuffer(vk, vkDevice, &sourceBufferParams);
		m_sourceBufferAlloc		= memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_source), MemoryRequirement::HostVisible);
		VK_CHECK(vk.bindBufferMemory(vkDevice, *m_source, m_sourceBufferAlloc->getMemory(), m_sourceBufferAlloc->getOffset()));
	}

	// Create desctination buffer
	{
		const VkBufferCreateInfo	destinationBufferParams	=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			0u,											// VkBufferCreateFlags	flags;
			m_params.dst.buffer.size,					// VkDeviceSize			size;
			VK_BUFFER_USAGE_TRANSFER_DST_BIT,			// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
			1u,											// deUint32				queueFamilyIndexCount;
			&queueFamilyIndex,							// const deUint32*		pQueueFamilyIndices;
		};

		m_destination				= createBuffer(vk, vkDevice, &destinationBufferParams);
		m_destinationBufferAlloc	= memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_destination), MemoryRequirement::HostVisible);
		VK_CHECK(vk.bindBufferMemory(vkDevice, *m_destination, m_destinationBufferAlloc->getMemory(), m_destinationBufferAlloc->getOffset()));
	}
}

tcu::TestStatus CopyBufferToBuffer::iterate()
{
	const int srcLevelWidth = (int)(m_params.src.buffer.size/4); // Here the format is VK_FORMAT_R32_UINT, we need to divide the buffer size by 4
	m_sourceTextureLevel		= de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(mapVkFormat(VK_FORMAT_R32_UINT), srcLevelWidth, 1));
	generateBuffer(m_sourceTextureLevel->getAccess(), srcLevelWidth, 1, 1, FILL_MODE_RED);

	const int dstLevelWidth = (int)(m_params.dst.buffer.size/4);
	m_destinationTextureLevel	= de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(mapVkFormat(VK_FORMAT_R32_UINT), dstLevelWidth, 1));
	generateBuffer(m_destinationTextureLevel->getAccess(), dstLevelWidth, 1, 1, FILL_MODE_WHITE);

	generateExpectedResult();

	uploadBuffer(m_sourceTextureLevel->getAccess(), *m_sourceBufferAlloc);
	uploadBuffer(m_destinationTextureLevel->getAccess(), *m_destinationBufferAlloc);

	const DeviceInterface&		vk			= m_context.getDeviceInterface();
	const VkDevice				vkDevice	= m_context.getDevice();
	const VkQueue				queue		= m_context.getUniversalQueue();

	const VkBufferMemoryBarrier srcBufferBarrier =
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType	sType;
		DE_NULL,									// const void*		pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags	srcAccessMask;
		VK_ACCESS_HOST_READ_BIT,					// VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			dstQueueFamilyIndex;
		*m_source,									// VkBuffer			buffer;
		0u,											// VkDeviceSize		offset;
		m_params.src.buffer.size					// VkDeviceSize		size;
	};

	const VkBufferMemoryBarrier dstBufferBarrier	=
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType	sType;
		DE_NULL,									// const void*		pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags	srcAccessMask;
		VK_ACCESS_HOST_READ_BIT,					// VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			dstQueueFamilyIndex;
		*m_destination,								// VkBuffer			buffer;
		0u,											// VkDeviceSize		offset;
		m_params.dst.buffer.size					// VkDeviceSize		size;
	};

	VkBufferCopy* bufferCopies = ((VkBufferCopy*)deMalloc(m_params.regions.size() * sizeof(VkBufferCopy)));
	for (deUint32 i = 0; i < m_params.regions.size(); i++)
		bufferCopies[i] = m_params.regions[i].bufferCopy;

	const VkCommandBufferBeginInfo cmdBufferBeginInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,			// VkStructureType					sType;
		DE_NULL,												// const void*						pNext;
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,			// VkCommandBufferUsageFlags		flags;
		(const VkCommandBufferInheritanceInfo*)DE_NULL,
	};

	VK_CHECK(vk.beginCommandBuffer(*m_cmdBuffer, &cmdBufferBeginInfo));
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &srcBufferBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
	vk.cmdCopyBuffer(*m_cmdBuffer, m_source.get(), m_destination.get(), (deUint32)m_params.regions.size(), bufferCopies);
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &dstBufferBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
	VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));

	const VkSubmitInfo submitInfo =
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType			sType;
		DE_NULL,						// const void*				pNext;
		0u,								// deUint32					waitSemaphoreCount;
		DE_NULL,						// const VkSemaphore*		pWaitSemaphores;
		(const VkPipelineStageFlags*)DE_NULL,
		1u,								// deUint32					commandBufferCount;
		&m_cmdBuffer.get(),				// const VkCommandBuffer*	pCommandBuffers;
		0u,								// deUint32					signalSemaphoreCount;
		DE_NULL							// const VkSemaphore*		pSignalSemaphores;
	};

	VK_CHECK(vk.resetFences(vkDevice, 1, &m_fence.get()));
	VK_CHECK(vk.queueSubmit(queue, 1, &submitInfo, *m_fence));
	VK_CHECK(vk.waitForFences(vkDevice, 1, &m_fence.get(), true, ~(0ull) /* infinity */));

	// Read buffer data
	de::MovePtr<tcu::TextureLevel>	resultLevel		(new tcu::TextureLevel(mapVkFormat(VK_FORMAT_R32_UINT), dstLevelWidth, 1));
	invalidateMappedMemoryRange(vk, vkDevice, m_destinationBufferAlloc->getMemory(), m_destinationBufferAlloc->getOffset(), m_params.dst.buffer.size);
	tcu::copy(*resultLevel, tcu::ConstPixelBufferAccess(resultLevel->getFormat(), resultLevel->getSize(), m_destinationBufferAlloc->getHostPtr()));
	deFree(bufferCopies);

	return checkTestResult(resultLevel->getAccess());
}

void CopyBufferToBuffer::copyRegionToTextureLevel (tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst, CopyRegion region)
{
	deMemcpy((deUint8*) dst.getDataPtr() + region.bufferCopy.dstOffset,
				(deUint8*) src.getDataPtr() + region.bufferCopy.srcOffset,
				(size_t)region.bufferCopy.size);
}

class BufferToBufferTestCase : public vkt::TestCase
{
public:
							BufferToBufferTestCase	(tcu::TestContext&	testCtx,
													 const std::string&	name,
													 const std::string&	description,
													 const TestParams	params)
								: vkt::TestCase		(testCtx, name, description)
								, m_params			(params)
							{}
	virtual					~BufferToBufferTestCase	(void) {}

	virtual TestInstance*	createInstance			(Context& context) const
							{
								return new CopyBufferToBuffer(context, m_params);
							}
private:
	TestParams				m_params;
};

// Copy from image to buffer.

class CopyImageToBuffer : public CopiesAndBlittingTestInstance
{
public:
								CopyImageToBuffer			(Context&	context,
															 TestParams	testParams);
	virtual						~CopyImageToBuffer			(void) {}
	virtual tcu::TestStatus		iterate						(void);
private:
	virtual void				copyRegionToTextureLevel	(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst, CopyRegion region);

	tcu::TextureFormat			m_textureFormat;
	VkDeviceSize				m_bufferSize;

	Move<VkImage>				m_source;
	de::MovePtr<Allocation>		m_sourceImageAlloc;
	Move<VkBuffer>				m_destination;
	de::MovePtr<Allocation>		m_destinationBufferAlloc;
};

CopyImageToBuffer::CopyImageToBuffer (Context& context, TestParams testParams)
	: CopiesAndBlittingTestInstance(context, testParams)
	, m_textureFormat(mapVkFormat(testParams.src.image.format))
	, m_bufferSize(m_params.dst.buffer.size * tcu::getPixelSize(m_textureFormat))
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
			m_bufferSize,								// VkDeviceSize			size;
			VK_BUFFER_USAGE_TRANSFER_DST_BIT,			// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
			1u,											// deUint32				queueFamilyIndexCount;
			&queueFamilyIndex,							// const deUint32*		pQueueFamilyIndices;
		};

		m_destination				= createBuffer(vk, vkDevice, &destinationBufferParams);
		m_destinationBufferAlloc	= memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_destination), MemoryRequirement::HostVisible);
		VK_CHECK(vk.bindBufferMemory(vkDevice, *m_destination, m_destinationBufferAlloc->getMemory(), m_destinationBufferAlloc->getOffset()));
	}
}

tcu::TestStatus CopyImageToBuffer::iterate()
{
	m_sourceTextureLevel = de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(m_textureFormat,
																				m_params.src.image.extent.width,
																				m_params.src.image.extent.height,
																				m_params.src.image.extent.depth));
	generateBuffer(m_sourceTextureLevel->getAccess(), m_params.src.image.extent.width, m_params.src.image.extent.height, m_params.src.image.extent.depth, FILL_MODE_RED);
	m_destinationTextureLevel = de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(m_textureFormat, (int)m_params.dst.buffer.size, 1));
	generateBuffer(m_destinationTextureLevel->getAccess(), (int)m_params.dst.buffer.size, 1, 1);

	generateExpectedResult();

	uploadImage(m_sourceTextureLevel->getAccess(), *m_source);
	uploadBuffer(m_destinationTextureLevel->getAccess(), *m_destinationBufferAlloc);

	const DeviceInterface&		vk			= m_context.getDeviceInterface();
	const VkDevice				vkDevice	= m_context.getDevice();
	const VkQueue				queue		= m_context.getUniversalQueue();

	// Barriers for copying image to buffer
	const VkImageMemoryBarrier imageBarrier =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			srcAccessMask;
		VK_ACCESS_TRANSFER_READ_BIT,				// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_GENERAL,					// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,		// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
		*m_source,									// VkImage					image;
		{											// VkImageSubresourceRange	subresourceRange;
			getAspectFlag(m_textureFormat),	// VkImageAspectFlags	aspectMask;
			0u,								// deUint32				baseMipLevel;
			1u,								// deUint32				mipLevels;
			0u,								// deUint32				baseArraySlice;
			1u								// deUint32				arraySize;
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
		m_bufferSize								// VkDeviceSize		size;
	};

	// Copy from image to buffer
	VkBufferImageCopy* bufferImageCopies = ((VkBufferImageCopy*)deMalloc(m_params.regions.size() * sizeof(VkBufferImageCopy)));
	for (deUint32 i = 0; i < m_params.regions.size(); i++)
		bufferImageCopies[i] = m_params.regions[i].bufferImageCopy;

	const VkCommandBufferBeginInfo cmdBufferBeginInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,			// VkStructureType					sType;
		DE_NULL,												// const void*						pNext;
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,			// VkCommandBufferUsageFlags		flags;
		(const VkCommandBufferInheritanceInfo*)DE_NULL,
	};

	VK_CHECK(vk.beginCommandBuffer(*m_cmdBuffer, &cmdBufferBeginInfo));
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &imageBarrier);
	vk.cmdCopyImageToBuffer(*m_cmdBuffer, m_source.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_destination.get(), (deUint32)m_params.regions.size(), bufferImageCopies);
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &bufferBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
	VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));

	const VkSubmitInfo				submitInfo		=
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType			sType;
		DE_NULL,						// const void*				pNext;
		0u,								// deUint32					waitSemaphoreCount;
		DE_NULL,						// const VkSemaphore*		pWaitSemaphores;
		(const VkPipelineStageFlags*)DE_NULL,
		1u,								// deUint32					commandBufferCount;
		&m_cmdBuffer.get(),				// const VkCommandBuffer*	pCommandBuffers;
		0u,								// deUint32					signalSemaphoreCount;
		DE_NULL							// const VkSemaphore*		pSignalSemaphores;
	};

	VK_CHECK(vk.resetFences(vkDevice, 1, &m_fence.get()));
	VK_CHECK(vk.queueSubmit(queue, 1, &submitInfo, *m_fence));
	VK_CHECK(vk.waitForFences(vkDevice, 1, &m_fence.get(), true, ~(0ull) /* infinity */));

	// Read buffer data
	de::MovePtr<tcu::TextureLevel>	resultLevel		(new tcu::TextureLevel(m_textureFormat, (int)m_params.dst.buffer.size, 1));
	invalidateMappedMemoryRange(vk, vkDevice, m_destinationBufferAlloc->getMemory(), m_destinationBufferAlloc->getOffset(), m_bufferSize);
	tcu::copy(*resultLevel, tcu::ConstPixelBufferAccess(resultLevel->getFormat(), resultLevel->getSize(), m_destinationBufferAlloc->getHostPtr()));
	deFree(bufferImageCopies);

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

	virtual TestInstance*	createInstance				(Context&				context) const
							{
								return new CopyImageToBuffer(context, m_params);
							}
private:
	TestParams				m_params;
};

void CopyImageToBuffer::copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst, CopyRegion region)
{
	deUint32 rowLength = region.bufferImageCopy.bufferRowLength;
	if (!rowLength)
		rowLength = region.bufferImageCopy.imageExtent.width;

	deUint32 imageHeight = region.bufferImageCopy.bufferImageHeight;
	if (!imageHeight)
		imageHeight = region.bufferImageCopy.imageExtent.height;

	const int			texelSize	= src.getFormat().getPixelSize();
	const VkExtent3D	extent		= region.bufferImageCopy.imageExtent;
	const VkOffset3D	srcOffset	= region.bufferImageCopy.imageOffset;
	const int			texelOffset	= (int) region.bufferImageCopy.bufferOffset / texelSize;

	for (deUint32 z = 0; z < extent.depth; z++)
	{
		for (deUint32 y = 0; y < extent.height; y++)
		{
			int									texelIndex		= texelOffset + (z * imageHeight + y) *  rowLength;
			const tcu::ConstPixelBufferAccess	srcSubRegion	= tcu::getSubregion(src, srcOffset.x, srcOffset.y + y, srcOffset.z + z,
																					region.bufferImageCopy.imageExtent.width, 1, 1);
			const tcu::PixelBufferAccess		dstSubRegion	= tcu::getSubregion(dst, texelIndex, 0, region.bufferImageCopy.imageExtent.width, 1);
			tcu::copy(dstSubRegion, srcSubRegion);
		}
	}
}

// Copy from buffer to image.

class CopyBufferToImage : public CopiesAndBlittingTestInstance
{
public:
								CopyBufferToImage			(Context&	context,
															 TestParams	testParams);
	virtual						~CopyBufferToImage			(void) {}
	virtual tcu::TestStatus		iterate						(void);
private:
	virtual void				copyRegionToTextureLevel	(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst, CopyRegion region);

	tcu::TextureFormat			m_textureFormat;
	VkDeviceSize				m_bufferSize;

	Move<VkBuffer>				m_source;
	de::MovePtr<Allocation>		m_sourceBufferAlloc;
	Move<VkImage>				m_destination;
	de::MovePtr<Allocation>		m_destinationImageAlloc;
};

CopyBufferToImage::CopyBufferToImage (Context& context, TestParams testParams)
	: CopiesAndBlittingTestInstance(context, testParams)
	, m_textureFormat(mapVkFormat(testParams.dst.image.format))
	, m_bufferSize(m_params.src.buffer.size * tcu::getPixelSize(m_textureFormat))
{
	const DeviceInterface&		vk					= context.getDeviceInterface();
	const VkDevice				vkDevice			= context.getDevice();
	const deUint32				queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	SimpleAllocator				memAlloc			(vk, vkDevice, getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice()));

	// Create source buffer
	{
		const VkBufferCreateInfo	sourceBufferParams		=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			0u,											// VkBufferCreateFlags	flags;
			m_bufferSize,								// VkDeviceSize			size;
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,			// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
			1u,											// deUint32				queueFamilyIndexCount;
			&queueFamilyIndex,							// const deUint32*		pQueueFamilyIndices;
		};

		m_source				= createBuffer(vk, vkDevice, &sourceBufferParams);
		m_sourceBufferAlloc		= memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_source), MemoryRequirement::HostVisible);
		VK_CHECK(vk.bindBufferMemory(vkDevice, *m_source, m_sourceBufferAlloc->getMemory(), m_sourceBufferAlloc->getOffset()));
	}

	// Create destination image
	{
		const VkImageCreateInfo		destinationImageParams	=
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
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
				VK_IMAGE_USAGE_TRANSFER_DST_BIT,	// VkImageUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
			1u,										// deUint32				queueFamilyCount;
			&queueFamilyIndex,						// const deUint32*		pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout		initialLayout;
		};

		m_destination			= createImage(vk, vkDevice, &destinationImageParams);
		m_destinationImageAlloc	= memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_destination), MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(vkDevice, *m_destination, m_destinationImageAlloc->getMemory(), m_destinationImageAlloc->getOffset()));
	}
}

tcu::TestStatus CopyBufferToImage::iterate()
{
	m_sourceTextureLevel = de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(m_textureFormat, (int)m_params.src.buffer.size, 1));
	generateBuffer(m_sourceTextureLevel->getAccess(), (int)m_params.src.buffer.size, 1, 1);
	m_destinationTextureLevel = de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(m_textureFormat,
																					m_params.dst.image.extent.width,
																					m_params.dst.image.extent.height,
																					m_params.dst.image.extent.depth));

	generateBuffer(m_destinationTextureLevel->getAccess(), m_params.dst.image.extent.width, m_params.dst.image.extent.height, m_params.dst.image.extent.depth, FILL_MODE_WHITE);

	generateExpectedResult();

	uploadBuffer(m_sourceTextureLevel->getAccess(), *m_sourceBufferAlloc);
	uploadImage(m_destinationTextureLevel->getAccess(), *m_destination);

	const DeviceInterface&		vk			= m_context.getDeviceInterface();
	const VkDevice				vkDevice	= m_context.getDevice();
	const VkQueue				queue		= m_context.getUniversalQueue();
	SimpleAllocator				memAlloc	(vk, vkDevice, getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));

	const VkImageMemoryBarrier imageBarrier =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			srcAccessMask;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_GENERAL,					// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
		*m_destination,								// VkImage					image;
		{											// VkImageSubresourceRange	subresourceRange;
			getAspectFlag(m_textureFormat),	// VkImageAspectFlags	aspectMask;
			0u,								// deUint32				baseMipLevel;
			1u,								// deUint32				mipLevels;
			0u,								// deUint32				baseArraySlice;
			1u								// deUint32				arraySize;
		}
	};

	// Copy from buffer to image
	VkBufferImageCopy* bufferImageCopies = ((VkBufferImageCopy*)deMalloc(m_params.regions.size() * sizeof(VkBufferImageCopy)));
	for (deUint32 i = 0; i < m_params.regions.size(); i++)
		bufferImageCopies[i] = m_params.regions[i].bufferImageCopy;

	const VkCommandBufferBeginInfo cmdBufferBeginInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,			// VkStructureType					sType;
		DE_NULL,												// const void*						pNext;
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,			// VkCommandBufferUsageFlags		flags;
		(const VkCommandBufferInheritanceInfo*)DE_NULL,
	};

	VK_CHECK(vk.beginCommandBuffer(*m_cmdBuffer, &cmdBufferBeginInfo));
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &imageBarrier);
	vk.cmdCopyBufferToImage(*m_cmdBuffer, m_source.get(), m_destination.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (deUint32)m_params.regions.size(), bufferImageCopies);
	VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));

	const VkSubmitInfo				submitInfo		=
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType			sType;
		DE_NULL,						// const void*				pNext;
		0u,								// deUint32					waitSemaphoreCount;
		DE_NULL,						// const VkSemaphore*		pWaitSemaphores;
		(const VkPipelineStageFlags*)DE_NULL,
		1u,								// deUint32					commandBufferCount;
		&m_cmdBuffer.get(),				// const VkCommandBuffer*	pCommandBuffers;
		0u,								// deUint32					signalSemaphoreCount;
		DE_NULL							// const VkSemaphore*		pSignalSemaphores;
	};

	VK_CHECK(vk.resetFences(vkDevice, 1, &m_fence.get()));
	VK_CHECK(vk.queueSubmit(queue, 1, &submitInfo, *m_fence));
	VK_CHECK(vk.waitForFences(vkDevice, 1, &m_fence.get(), true, ~(0ull) /* infinity */));

	de::MovePtr<tcu::TextureLevel>	resultLevel	= readImage(vk, vkDevice, queue, memAlloc, *m_destination, m_params.dst.image.format, m_params.dst.image.extent);
	deFree(bufferImageCopies);

	return checkTestResult(resultLevel->getAccess());
}

class CopyBufferToImageTestCase : public vkt::TestCase
{
public:
							CopyBufferToImageTestCase	(tcu::TestContext&		testCtx,
														 const std::string&		name,
														 const std::string&		description,
														 const TestParams		params)
								: vkt::TestCase			(testCtx, name, description)
								, m_params				(params)
							{}

	virtual					~CopyBufferToImageTestCase	(void) {}

	virtual TestInstance*	createInstance				(Context&				context) const
							{
								return new CopyBufferToImage(context, m_params);
							}
private:
	TestParams				m_params;
};

void CopyBufferToImage::copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst, CopyRegion region)
{
	deUint32 rowLength = region.bufferImageCopy.bufferRowLength;
	if (!rowLength)
		rowLength = region.bufferImageCopy.imageExtent.width;

	deUint32 imageHeight = region.bufferImageCopy.bufferImageHeight;
	if (!imageHeight)
		imageHeight = region.bufferImageCopy.imageExtent.height;

	const int			texelSize	= dst.getFormat().getPixelSize();
	const VkExtent3D	extent		= region.bufferImageCopy.imageExtent;
	const VkOffset3D	dstOffset	= region.bufferImageCopy.imageOffset;
	const int			texelOffset	= (int) region.bufferImageCopy.bufferOffset / texelSize;

	for (deUint32 z = 0; z < extent.depth; z++)
	{
		for (deUint32 y = 0; y < extent.height; y++)
		{
			int									texelIndex		= texelOffset + (z * imageHeight + y) *  rowLength;
			const tcu::ConstPixelBufferAccess	srcSubRegion	= tcu::getSubregion(src, texelIndex, 0, region.bufferImageCopy.imageExtent.width, 1);
			const tcu::PixelBufferAccess		dstSubRegion	= tcu::getSubregion(dst, dstOffset.x, dstOffset.y + y, dstOffset.z + z,
																					region.bufferImageCopy.imageExtent.width, 1, 1);
			tcu::copy(dstSubRegion, srcSubRegion);
		}
	}
}

} // anonymous

tcu::TestCaseGroup* createCopiesAndBlittingTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	copiesAndBlittingTests	(new tcu::TestCaseGroup(testCtx, "copy_and_blit", "Copies And Blitting Tests"));

	const VkExtent3D				defaultExtent			= {256, 256, 1};
	const VkImageSubresourceLayers	defaultSourceLayer		=
	{
		VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
		0u,							// uint32_t				mipLevel;
		0u,							// uint32_t				baseArrayLayer;
		1u,							// uint32_t				layerCount;
	};

	{
		std::ostringstream description;
		description << "Copy from image to image";

		TestParams	params;
		params.src.image.format	= VK_FORMAT_R8G8B8A8_UINT;
		params.src.image.extent	= defaultExtent;
		params.dst.image.format	= VK_FORMAT_R8G8B8A8_UINT;
		params.dst.image.extent	= defaultExtent;

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
				{0, 0, 0},		// VkOffset3D				srcOffset;
				sourceLayer,	// VkImageSubresourceLayers	dstSubresource;
				{0, 0, 0},		// VkOffset3D				dstOffset;
				{256, 256, 1},	// VkExtent3D				extent;
			};

			CopyRegion imageCopy;
			imageCopy.imageCopy = testCopy;

			params.regions.push_back(imageCopy);
		}

		copiesAndBlittingTests->addChild(new CopyImageToImageTestCase(testCtx, "imageToImage_whole", description.str(), params));
	}

	{
		std::ostringstream description;
		description << "Copy from image to image";

		TestParams	params;
		params.src.image.format	= VK_FORMAT_R8G8B8A8_UINT;
		params.src.image.extent	= defaultExtent;
		params.dst.image.format	= VK_FORMAT_R32_UINT;
		params.dst.image.extent	= defaultExtent;

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
				{0, 0, 0},		// VkOffset3D				srcOffset;
				sourceLayer,	// VkImageSubresourceLayers	dstSubresource;
				{0, 0, 0},		// VkOffset3D				dstOffset;
				{256, 256, 1},	// VkExtent3D				extent;
			};

			CopyRegion imageCopy;
			imageCopy.imageCopy = testCopy;

			params.regions.push_back(imageCopy);
		}

		copiesAndBlittingTests->addChild(new CopyImageToImageTestCase(testCtx, "image_to_image_whole_different_format_uncompressed", description.str(), params));
	}

	{
		std::ostringstream description;
		description << "Copy from image to image";

		TestParams	params;
		params.src.image.format	= VK_FORMAT_R8G8B8A8_UINT;
		params.src.image.extent	= defaultExtent;
		params.dst.image.format	= VK_FORMAT_R8G8B8A8_UINT;
		params.dst.image.extent	= defaultExtent;

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
				{0, 0, 0},		// VkOffset3D				srcOffset;
				sourceLayer,	// VkImageSubresourceLayers	dstSubresource;
				{64, 98, 0},	// VkOffset3D				dstOffset;
				{16, 16, 1},	// VkExtent3D				extent;
			};

			CopyRegion imageCopy;
			imageCopy.imageCopy = testCopy;

			params.regions.push_back(imageCopy);
		}

		copiesAndBlittingTests->addChild(new CopyImageToImageTestCase(testCtx, "image_to_image_partial", description.str(), params));
	}

	{
		std::ostringstream description;
		description << "Copy from image to image";

		TestParams	params;
		params.src.image.format	= VK_FORMAT_R8G8B8A8_UINT;
		params.src.image.extent	= defaultExtent;
		params.dst.image.format	= VK_FORMAT_R8G8B8A8_UINT;
		params.dst.image.extent	= defaultExtent;

		for (deInt32 i = 0; i < 16; i++)
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
				sourceLayer,			// VkImageSubresourceLayers	srcSubresource;
				{0, 0, 0},				// VkOffset3D				srcOffset;
				sourceLayer,			// VkImageSubresourceLayers	dstSubresource;
				{i*16, 240-i*16, 0},	// VkOffset3D				dstOffset;
				{16, 16, 1},			// VkExtent3D				extent;
			};

			CopyRegion imageCopy;
			imageCopy.imageCopy = testCopy;

			params.regions.push_back(imageCopy);
		}

		copiesAndBlittingTests->addChild(new CopyImageToImageTestCase(testCtx, "image_to_image_partial_multiple", description.str(), params));
	}

	// Copy image to buffer testcases.
	{
		std::ostringstream	description;
		description << "Copy from image to buffer";

		TestParams	params;
		params.src.image.format	= VK_FORMAT_R8G8B8A8_UINT;
		params.src.image.extent	= defaultExtent;
		params.dst.buffer.size	= 256 * 256;

		const VkBufferImageCopy			bufferImageCopy	=
		{
			0u,						// VkDeviceSize				bufferOffset;
			0u,						// uint32_t					bufferRowLength;
			0u,						// uint32_t					bufferImageHeight;
			defaultSourceLayer,		// VkImageSubresourceLayers	imageSubresource;
			{0, 0, 0},				// VkOffset3D				imageOffset;
			{16, 16, 1}				// VkExtent3D				imageExtent;
		};
		CopyRegion copyRegion;
		copyRegion.bufferImageCopy = bufferImageCopy;

		params.regions.push_back(copyRegion);

		copiesAndBlittingTests->addChild(new CopyImageToBufferTestCase(testCtx, "image_to_buffer", description.str(), params));
	}

	// Copy buffer to image testcases.
	{
		std::ostringstream	description;
		description << "Copy from buffer to image";

		TestParams	params;
		params.src.buffer.size	= 256 * 256;
		params.dst.image.format	= VK_FORMAT_R8G8B8A8_UINT;
		params.dst.image.extent	= defaultExtent;

		const VkBufferImageCopy			bufferImageCopy	=
		{
			0u,						// VkDeviceSize				bufferOffset;
			0u,						// uint32_t					bufferRowLength;
			0u,						// uint32_t					bufferImageHeight;
			defaultSourceLayer,		// VkImageSubresourceLayers	imageSubresource;
			{0, 0, 0},				// VkOffset3D				imageOffset;
			{16, 16, 1}				// VkExtent3D				imageExtent;
		};
		CopyRegion copyRegion;
		copyRegion.bufferImageCopy = bufferImageCopy;

		params.regions.push_back(copyRegion);

		copiesAndBlittingTests->addChild(new CopyBufferToImageTestCase(testCtx, "buffer_to_image", description.str(), params));
	}

	{
		std::ostringstream	description;
		description << "Copy from buffer to buffer: whole buffer.";

		TestParams params;
		params.src.buffer.size = 256;
		params.dst.buffer.size = 256;
		const VkBufferCopy bufferCopy = {
			0u,		// VkDeviceSize	srcOffset;
			0u,		// VkDeviceSize	dstOffset;
			256u,	// VkDeviceSize	size;
		};
		CopyRegion copyRegion;
		copyRegion.bufferCopy = bufferCopy;

		params.regions.push_back(copyRegion);

		copiesAndBlittingTests->addChild(new BufferToBufferTestCase(testCtx, "buffer_to_buffer_whole", description.str(), params));
	}

	{
		std::ostringstream	description;
		description << "Copy from buffer to buffer: small area.";

		TestParams params;
		params.src.buffer.size = 16;
		params.dst.buffer.size = 16;
		const VkBufferCopy bufferCopy = {
			12u,	// VkDeviceSize	srcOffset;
			4u,		// VkDeviceSize	dstOffset;
			1u,		// VkDeviceSize	size;
		};
		CopyRegion copyRegion;
		copyRegion.bufferCopy = bufferCopy;

		params.regions.push_back(copyRegion);

		copiesAndBlittingTests->addChild(new BufferToBufferTestCase(testCtx, "buffer_to_buffer_small", description.str(), params));
	}

	{
		std::ostringstream	description;
		description << "Copy from buffer to buffer: more regions.";

		const deUint32 size = 16;

		TestParams params;
		params.src.buffer.size = size;
		params.dst.buffer.size = size * (size + 1);

		// Copy region with size 1..size
		for (unsigned int i = 1; i <= size; i++)
		{
			const VkBufferCopy bufferCopy = {
				0,		// VkDeviceSize	srcOffset;
				i*size,	// VkDeviceSize	dstOffset;
				i,		// VkDeviceSize	size;
			};
			CopyRegion copyRegion;
			copyRegion.bufferCopy = bufferCopy;
			params.regions.push_back(copyRegion);
		}
		copiesAndBlittingTests->addChild(new BufferToBufferTestCase(testCtx, "buffer_to_buffer_regions", description.str(), params));
	}

	{
		std::ostringstream description;
		description << "Copy from image to image depth";

		TestParams	params;
		params.src.image.format	= VK_FORMAT_D32_SFLOAT;
		params.src.image.extent	= defaultExtent;
		params.dst.image.format	= VK_FORMAT_D32_SFLOAT;
		params.dst.image.extent	= defaultExtent;

		{
			const VkImageSubresourceLayers sourceLayer =
			{
				VK_IMAGE_ASPECT_DEPTH_BIT,	// VkImageAspectFlags	aspectMask;
				0u,							// uint32_t				mipLevel;
				0u,							// uint32_t				baseArrayLayer;
				1u							// uint32_t				layerCount;
			};
			const VkImageCopy testCopy =
			{
				sourceLayer,	// VkImageSubresourceLayers	srcSubresource;
				{0, 0, 0},		// VkOffset3D				srcOffset;
				sourceLayer,	// VkImageSubresourceLayers	dstSubresource;
				{64, 98, 0},	// VkOffset3D				dstOffset;
				{16, 16, 1},	// VkExtent3D				extent;
			};

			CopyRegion imageCopy;
			imageCopy.imageCopy = testCopy;

			params.regions.push_back(imageCopy);
		}

		copiesAndBlittingTests->addChild(new CopyImageToImageTestCase(testCtx, "image_to_image_depth", description.str(), params));
	}

	{
		std::ostringstream description;
		description << "Copy from image to image stencil";

		TestParams	params;
		params.src.image.format	= VK_FORMAT_S8_UINT;
		params.src.image.extent	= defaultExtent;
		params.dst.image.format	= VK_FORMAT_S8_UINT;
		params.dst.image.extent	= defaultExtent;

		{
			const VkImageSubresourceLayers sourceLayer =
			{
				VK_IMAGE_ASPECT_STENCIL_BIT,	// VkImageAspectFlags	aspectMask;
				0u,								// uint32_t				mipLevel;
				0u,								// uint32_t				baseArrayLayer;
				1u								// uint32_t				layerCount;
			};
			const VkImageCopy testCopy =
			{
				sourceLayer,	// VkImageSubresourceLayers	srcSubresource;
				{0, 0, 0},		// VkOffset3D				srcOffset;
				sourceLayer,	// VkImageSubresourceLayers	dstSubresource;
				{64, 98, 0},	// VkOffset3D				dstOffset;
				{16, 16, 1},	// VkExtent3D				extent;
			};

			CopyRegion imageCopy;
			imageCopy.imageCopy = testCopy;

			params.regions.push_back(imageCopy);
		}

		copiesAndBlittingTests->addChild(new CopyImageToImageTestCase(testCtx, "image_to_image_stencil", description.str(), params));
	}

	return copiesAndBlittingTests.release();
}

} // api
} // vkt
