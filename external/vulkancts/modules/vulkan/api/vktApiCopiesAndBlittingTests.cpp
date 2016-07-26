/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015-2016 The Khronos Group Inc.
 * Copyright (c) 2015-2016 Samsung Electronics Co., Ltd.
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

#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"
#include "deMath.h"

#include "tcuImageCompare.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVectorType.hpp"
#include "tcuVectorUtil.hpp"

#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkTypeUtil.hpp"

namespace vkt
{

namespace api
{

namespace
{
enum MirrorMode
{
	MIRROR_MODE_NONE = 0,
	MIRROR_MODE_X = (1<<0),
	MIRROR_MODE_Y = (1<<1),
	MIRROR_MODE_XY = MIRROR_MODE_X | MIRROR_MODE_Y,

	MIRROR_MODE_LAST
};

}

using namespace vk;

namespace
{

VkImageAspectFlags getAspectFlags (tcu::TextureFormat format)
{
	VkImageAspectFlags	aspectFlag	= 0;
	aspectFlag |= (tcu::hasDepthComponent(format.order)? VK_IMAGE_ASPECT_DEPTH_BIT : 0);
	aspectFlag |= (tcu::hasStencilComponent(format.order)? VK_IMAGE_ASPECT_STENCIL_BIT : 0);

	if (!aspectFlag)
		aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;

	return aspectFlag;
}

// This is effectively same as vk::isFloatFormat(mapTextureFormat(format))
// except that it supports some formats that are not mappable to VkFormat.
// When we are checking combined depth and stencil formats, each aspect is
// checked separately, and in some cases we construct PBA with a format that
// is not mappable to VkFormat.
bool isFloatFormat (tcu::TextureFormat format)
{
	return tcu::getTextureChannelClass(format.type) == tcu::TEXTURECHANNELCLASS_FLOATING_POINT;
}

union CopyRegion
{
	VkBufferCopy		bufferCopy;
	VkImageCopy			imageCopy;
	VkBufferImageCopy	bufferImageCopy;
	VkImageBlit			imageBlit;
	VkImageResolve		imageResolve;
};

struct ImageParms
{
	VkImageType		imageType;
	VkFormat		format;
	VkExtent3D		extent;
};

struct TestParams
{
	union
	{
		struct
		{
			VkDeviceSize	size;
		} buffer;

		ImageParms	image;
	} src, dst;

	std::vector<CopyRegion>	regions;

	union
	{
		VkFilter				filter;
		VkSampleCountFlagBits	samples;
	};
};

inline deUint32 getArraySize(const ImageParms& parms)
{
	return (parms.imageType == VK_IMAGE_TYPE_2D) ? parms.extent.depth : 1u;
}

inline VkExtent3D getExtent3D(const ImageParms& parms)
{
	const VkExtent3D		extent					=
	{
		parms.extent.width,
		parms.extent.height,
		(parms.imageType == VK_IMAGE_TYPE_2D) ? 1u : parms.extent.depth
	};
	return extent;
}

const tcu::TextureFormat mapCombinedToDepthTransferFormat (const tcu::TextureFormat& combinedFormat)
{
	tcu::TextureFormat format;
	switch (combinedFormat.type)
	{
		case tcu::TextureFormat::UNSIGNED_INT_16_8_8:
			format = tcu::TextureFormat(tcu::TextureFormat::D, tcu::TextureFormat::UNORM_INT16);
			break;
		case tcu::TextureFormat::UNSIGNED_INT_24_8_REV:
			format = tcu::TextureFormat(tcu::TextureFormat::D, tcu::TextureFormat::UNSIGNED_INT_24_8_REV);
			break;
		case tcu::TextureFormat::FLOAT_UNSIGNED_INT_24_8_REV:
			format = tcu::TextureFormat(tcu::TextureFormat::D, tcu::TextureFormat::FLOAT);
			break;
		default:
			DE_ASSERT(false);
			break;
	}
	return format;
}

class CopiesAndBlittingTestInstance : public vkt::TestInstance
{
public:
										CopiesAndBlittingTestInstance		(Context&	context,
																			 TestParams	testParams);
	virtual tcu::TestStatus				iterate								(void) = 0;

	enum FillMode
	{
		FILL_MODE_GRADIENT = 0,
		FILL_MODE_WHITE,
		FILL_MODE_RED,
		FILL_MODE_MULTISAMPLE,

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

	void								generateBuffer						(tcu::PixelBufferAccess buffer, int width, int height, int depth = 1, FillMode = FILL_MODE_GRADIENT);
	virtual void						generateExpectedResult				(void);
	void								uploadBuffer						(tcu::ConstPixelBufferAccess bufferAccess, const Allocation& bufferAlloc);
	void								uploadImage							(const tcu::ConstPixelBufferAccess& src, VkImage dst, const ImageParms& parms);
	virtual tcu::TestStatus				checkTestResult						(tcu::ConstPixelBufferAccess result);
	virtual void						copyRegionToTextureLevel			(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst, CopyRegion region) = 0;
	deUint32							calculateSize						(tcu::ConstPixelBufferAccess src) const
										{
											return src.getWidth() * src.getHeight() * src.getDepth() * tcu::getPixelSize(src.getFormat());
										}

	de::MovePtr<tcu::TextureLevel>		readImage							(vk::VkImage				image,
																			 const ImageParms&			imageParms);
	void								submitCommandsAndWait				(const DeviceInterface&		vk,
																			const VkDevice				device,
																			const VkQueue				queue,
																			const VkCommandBuffer&		cmdBuffer);

private:
	void								uploadImageAspect					(const tcu::ConstPixelBufferAccess&	src,
																			 const VkImage&						dst,
																			 const ImageParms&					parms);
	void								readImageAspect						(vk::VkImage						src,
																			 const tcu::PixelBufferAccess&		dst,
																			 const ImageParms&					parms);
};

CopiesAndBlittingTestInstance::CopiesAndBlittingTestInstance (Context& context, TestParams testParams)
	: vkt::TestInstance	(context)
	, m_params			(testParams)
{
	const DeviceInterface&		vk					= context.getDeviceInterface();
	const VkDevice				vkDevice			= context.getDevice();
	const deUint32				queueFamilyIndex	= context.getUniversalQueueFamilyIndex();

	// Create command pool
	{
		const VkCommandPoolCreateInfo		cmdPoolParams			=
		{
			VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,										// const void*			pNext;
			VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,// VkCmdPoolCreateFlags	flags;
			queueFamilyIndex,								// deUint32				queueFamilyIndex;
		};

		m_cmdPool = createCommandPool(vk, vkDevice, &cmdPoolParams);
	}

	// Create command buffer
	{
		const VkCommandBufferAllocateInfo	cmdBufferAllocateInfo	=
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
		const VkFenceCreateInfo				fenceParams				=
		{
			VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,								// const void*			pNext;
			0u										// VkFenceCreateFlags	flags;
		};

		m_fence = createFence(vk, vkDevice, &fenceParams);
	}
}

void CopiesAndBlittingTestInstance::generateBuffer (tcu::PixelBufferAccess buffer, int width, int height, int depth, FillMode mode)
{
	if (mode == FILL_MODE_GRADIENT)
	{
		tcu::fillWithComponentGradients(buffer, tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
		return;
	}

	const tcu::Vec4		redColor	(1.0, 0.0, 0.0, 1.0);
	const tcu::Vec4		greenColor	(0.0, 1.0, 0.0, 1.0);
	const tcu::Vec4		blueColor	(0.0, 0.0, 1.0, 1.0);
	const tcu::Vec4		whiteColor	(1.0, 1.0, 1.0, 1.0);

	for (int z = 0; z < depth; z++)
	{
		for (int y = 0; y < height; y++)
		{
			for (int x = 0; x < width; x++)
			{
				switch (mode)
				{
					case FILL_MODE_WHITE:
						if (tcu::isCombinedDepthStencilType(buffer.getFormat().type))
						{
							buffer.setPixDepth(1.0f, x, y, z);
							if (tcu::hasStencilComponent(buffer.getFormat().order))
								buffer.setPixStencil(255, x, y, z);
						}
						else
							buffer.setPixel(whiteColor, x, y, z);
						break;
					case FILL_MODE_RED:
						if (tcu::isCombinedDepthStencilType(buffer.getFormat().type))
						{
							buffer.setPixDepth(redColor[x % 4], x, y, z);
							if (tcu::hasStencilComponent(buffer.getFormat().order))
								buffer.setPixStencil(255 * (int)redColor[y % 4], x, y, z);
						}
						else
							buffer.setPixel(redColor, x, y, z);
						break;
					case FILL_MODE_MULTISAMPLE:
						buffer.setPixel((x == y) ? tcu::Vec4(0.0, 0.5, 0.5, 1.0) : ((x > y) ? greenColor : blueColor), x, y, z);
						break;
					default:
						break;
				}
			}
		}
	}
}

void CopiesAndBlittingTestInstance::uploadBuffer (tcu::ConstPixelBufferAccess bufferAccess, const Allocation& bufferAlloc)
{
	const DeviceInterface&		vk			= m_context.getDeviceInterface();
	const VkDevice				vkDevice	= m_context.getDevice();
	const deUint32				bufferSize	= calculateSize(bufferAccess);

	// Write buffer data
	deMemcpy(bufferAlloc.getHostPtr(), bufferAccess.getDataPtr(), bufferSize);
	flushMappedMemoryRange(vk, vkDevice, bufferAlloc.getMemory(), bufferAlloc.getOffset(), bufferSize);
}

void CopiesAndBlittingTestInstance::uploadImageAspect (const tcu::ConstPixelBufferAccess& imageAccess, const VkImage& image, const ImageParms& parms)
{
	const DeviceInterface&		vk					= m_context.getDeviceInterface();
	const VkDevice				vkDevice			= m_context.getDevice();
	const VkQueue				queue				= m_context.getUniversalQueue();
	const deUint32				queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	Allocator&					memAlloc			= m_context.getDefaultAllocator();

	Move<VkBuffer>				buffer;
	const deUint32				bufferSize			= calculateSize(imageAccess);
	de::MovePtr<Allocation>		bufferAlloc;
	const deUint32				arraySize			= getArraySize(parms);
	const VkExtent3D			imageExtent			= getExtent3D(parms);

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

	const VkImageAspectFlags				aspect					= getAspectFlags(imageAccess.getFormat());
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
			aspect,			// VkImageAspectFlags	aspect;
			0u,				// deUint32				baseMipLevel;
			1u,				// deUint32				mipLevels;
			0u,				// deUint32				baseArraySlice;
			arraySize,		// deUint32				arraySize;
		}
	};

	const VkImageMemoryBarrier				postImageBarrier		=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,					// VkAccessFlags			srcAccessMask;
		VK_ACCESS_TRANSFER_WRITE_BIT,					// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,			// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,			// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32					dstQueueFamilyIndex;
		image,											// VkImage					image;
		{												// VkImageSubresourceRange	subresourceRange;
			aspect,						// VkImageAspectFlags	aspect;
			0u,							// deUint32				baseMipLevel;
			1u,							// deUint32				mipLevels;
			0u,							// deUint32				baseArraySlice;
			arraySize,					// deUint32				arraySize;
		}
	};

	const VkBufferImageCopy		copyRegion		=
	{
		0u,												// VkDeviceSize				bufferOffset;
		(deUint32)imageAccess.getWidth(),				// deUint32					bufferRowLength;
		(deUint32)imageAccess.getHeight(),				// deUint32					bufferImageHeight;
		{
			getAspectFlags(imageAccess.getFormat()),		// VkImageAspectFlags	aspect;
			0u,												// deUint32				mipLevel;
			0u,												// deUint32				baseArrayLayer;
			arraySize,										// deUint32				layerCount;
		},												// VkImageSubresourceLayers	imageSubresource;
		{ 0, 0, 0 },									// VkOffset3D				imageOffset;
		imageExtent										// VkExtent3D				imageExtent;
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

	VK_CHECK(vk.beginCommandBuffer(*m_cmdBuffer, &cmdBufferBeginInfo));
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &preBufferBarrier, 1, &preImageBarrier);
	vk.cmdCopyBufferToImage(*m_cmdBuffer, *buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &copyRegion);
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &postImageBarrier);
	VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));

	submitCommandsAndWait(vk, vkDevice, queue, *m_cmdBuffer);
}

void CopiesAndBlittingTestInstance::uploadImage (const tcu::ConstPixelBufferAccess& src, VkImage dst, const ImageParms& parms)
{
	if (tcu::isCombinedDepthStencilType(src.getFormat().type))
	{
		if (tcu::hasDepthComponent(src.getFormat().order))
		{
			tcu::TextureLevel	depthTexture	(mapCombinedToDepthTransferFormat(src.getFormat()), src.getWidth(), src.getHeight(), src.getDepth());
			tcu::copy(depthTexture.getAccess(), tcu::getEffectiveDepthStencilAccess(src, tcu::Sampler::MODE_DEPTH));
			uploadImageAspect(depthTexture.getAccess(), dst, parms);
		}

		if (tcu::hasStencilComponent(src.getFormat().order))
		{
			tcu::TextureLevel	stencilTexture	(tcu::getEffectiveDepthStencilTextureFormat(src.getFormat(), tcu::Sampler::MODE_STENCIL), src.getWidth(), src.getHeight(), src.getDepth());
			tcu::copy(stencilTexture.getAccess(), tcu::getEffectiveDepthStencilAccess(src, tcu::Sampler::MODE_STENCIL));
			uploadImageAspect(stencilTexture.getAccess(), dst, parms);
		}
	}
	else
		uploadImageAspect(src, dst, parms);
}

tcu::TestStatus CopiesAndBlittingTestInstance::checkTestResult (tcu::ConstPixelBufferAccess result)
{
	const tcu::ConstPixelBufferAccess	expected	= m_expectedTextureLevel->getAccess();

	if (isFloatFormat(result.getFormat()))
	{
		const tcu::Vec4	threshold (0.0f);
		if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparsion", expected, result, threshold, tcu::COMPARE_LOG_RESULT))
			return tcu::TestStatus::fail("CopiesAndBlitting test");
	}
	else
	{
		const tcu::UVec4 threshold (0u);
		if (!tcu::intThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparsion", expected, result, threshold, tcu::COMPARE_LOG_RESULT))
			return tcu::TestStatus::fail("CopiesAndBlitting test");
	}

	return tcu::TestStatus::pass("CopiesAndBlitting test");
}

void CopiesAndBlittingTestInstance::generateExpectedResult (void)
{
	const tcu::ConstPixelBufferAccess	src	= m_sourceTextureLevel->getAccess();
	const tcu::ConstPixelBufferAccess	dst	= m_destinationTextureLevel->getAccess();

	m_expectedTextureLevel	= de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(dst.getFormat(), dst.getWidth(), dst.getHeight(), dst.getDepth()));
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
								: vkt::TestCase	(testCtx, name, description)
							{}

	virtual TestInstance*	createInstance				(Context&					context) const = 0;
};

void CopiesAndBlittingTestInstance::readImageAspect (vk::VkImage					image,
													 const tcu::PixelBufferAccess&	dst,
													 const ImageParms&				imageParms)
{
	const DeviceInterface&		vk					= m_context.getDeviceInterface();
	const VkDevice				device				= m_context.getDevice();
	const VkQueue				queue				= m_context.getUniversalQueue();
	Allocator&					allocator			= m_context.getDefaultAllocator();

	Move<VkBuffer>				buffer;
	de::MovePtr<Allocation>		bufferAlloc;
	const deUint32				queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	const VkDeviceSize			pixelDataSize		= calculateSize(dst);
	const VkExtent3D			imageExtent			= getExtent3D(imageParms);

	// Create destination buffer
	{
		const VkBufferCreateInfo			bufferParams			=
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
		bufferAlloc	= allocator.allocate(getBufferMemoryRequirements(vk, device, *buffer), MemoryRequirement::HostVisible);
		VK_CHECK(vk.bindBufferMemory(device, *buffer, bufferAlloc->getMemory(), bufferAlloc->getOffset()));

		deMemset(bufferAlloc->getHostPtr(), 0, static_cast<size_t>(pixelDataSize));
		flushMappedMemoryRange(vk, device, bufferAlloc->getMemory(), bufferAlloc->getOffset(), pixelDataSize);
	}

	// Barriers for copying image to buffer
	const VkImageAspectFlags				aspect					= getAspectFlags(dst.getFormat());
	const VkImageMemoryBarrier				imageBarrier			=
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
			aspect,					// VkImageAspectFlags	aspectMask;
			0u,						// deUint32				baseMipLevel;
			1u,						// deUint32				mipLevels;
			0u,						// deUint32				baseArraySlice;
			getArraySize(imageParms)// deUint32				arraySize;
		}
	};

	const VkBufferMemoryBarrier				bufferBarrier			=
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

	const VkImageMemoryBarrier				postImageBarrier		=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		VK_ACCESS_TRANSFER_READ_BIT,				// VkAccessFlags			srcAccessMask;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,		// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
		image,										// VkImage					image;
		{
			aspect,									// VkImageAspectFlags	aspectMask;
			0u,											// deUint32				baseMipLevel;
			1u,											// deUint32				mipLevels;
			0u,											// deUint32				baseArraySlice;
			getArraySize(imageParms)					// deUint32				arraySize;
		}											// VkImageSubresourceRange	subresourceRange;
	};

	// Copy image to buffer
	const VkBufferImageCopy		copyRegion		=
	{
		0u,									// VkDeviceSize				bufferOffset;
		(deUint32)dst.getWidth(),			// deUint32					bufferRowLength;
		(deUint32)dst.getHeight(),			// deUint32					bufferImageHeight;
		{
			aspect,								// VkImageAspectFlags		aspect;
			0u,									// deUint32					mipLevel;
			0u,									// deUint32					baseArrayLayer;
			getArraySize(imageParms),			// deUint32					layerCount;
		},									// VkImageSubresourceLayers	imageSubresource;
		{ 0, 0, 0 },						// VkOffset3D				imageOffset;
		imageExtent							// VkExtent3D				imageExtent;
	};

	const VkCommandBufferBeginInfo			cmdBufferBeginInfo		=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,			// VkStructureType					sType;
		DE_NULL,												// const void*						pNext;
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,			// VkCommandBufferUsageFlags		flags;
		(const VkCommandBufferInheritanceInfo*)DE_NULL,
	};

	VK_CHECK(vk.beginCommandBuffer(*m_cmdBuffer, &cmdBufferBeginInfo));
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &imageBarrier);
	vk.cmdCopyImageToBuffer(*m_cmdBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *buffer, 1u, &copyRegion);
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &bufferBarrier, 1, &postImageBarrier);
	VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));

	submitCommandsAndWait(vk, device, queue, *m_cmdBuffer);

	// Read buffer data
	invalidateMappedMemoryRange(vk, device, bufferAlloc->getMemory(), bufferAlloc->getOffset(), pixelDataSize);
	tcu::copy(dst, tcu::ConstPixelBufferAccess(dst.getFormat(), dst.getSize(), bufferAlloc->getHostPtr()));
}

void CopiesAndBlittingTestInstance::submitCommandsAndWait (const DeviceInterface& vk, const VkDevice device, const VkQueue queue, const VkCommandBuffer& cmdBuffer)
{
	const VkSubmitInfo						submitInfo				=
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType			sType;
		DE_NULL,						// const void*				pNext;
		0u,								// deUint32					waitSemaphoreCount;
		DE_NULL,						// const VkSemaphore*		pWaitSemaphores;
		(const VkPipelineStageFlags*)DE_NULL,
		1u,								// deUint32					commandBufferCount;
		&cmdBuffer,						// const VkCommandBuffer*	pCommandBuffers;
		0u,								// deUint32					signalSemaphoreCount;
		DE_NULL							// const VkSemaphore*		pSignalSemaphores;
	};

	VK_CHECK(vk.resetFences(device, 1, &m_fence.get()));
	VK_CHECK(vk.queueSubmit(queue, 1, &submitInfo, *m_fence));
	VK_CHECK(vk.waitForFences(device, 1, &m_fence.get(), true, ~(0ull) /* infinity */));
}

de::MovePtr<tcu::TextureLevel> CopiesAndBlittingTestInstance::readImage	(vk::VkImage		image,
																		 const ImageParms&	parms)
{
	const tcu::TextureFormat		imageFormat	= mapVkFormat(parms.format);
	de::MovePtr<tcu::TextureLevel>	resultLevel	(new tcu::TextureLevel(imageFormat, parms.extent.width, parms.extent.height, parms.extent.depth));

	if (tcu::isCombinedDepthStencilType(imageFormat.type))
	{
		if (tcu::hasDepthComponent(imageFormat.order))
		{
			tcu::TextureLevel	depthTexture	(mapCombinedToDepthTransferFormat(imageFormat), parms.extent.width, parms.extent.height, parms.extent.depth);
			readImageAspect(image, depthTexture.getAccess(), parms);
			tcu::copy(tcu::getEffectiveDepthStencilAccess(resultLevel->getAccess(), tcu::Sampler::MODE_DEPTH), depthTexture.getAccess());
		}

		if (tcu::hasStencilComponent(imageFormat.order))
		{
			tcu::TextureLevel	stencilTexture	(tcu::getEffectiveDepthStencilTextureFormat(imageFormat, tcu::Sampler::MODE_STENCIL), parms.extent.width, parms.extent.height, parms.extent.depth);
			readImageAspect(image, stencilTexture.getAccess(), parms);
			tcu::copy(tcu::getEffectiveDepthStencilAccess(resultLevel->getAccess(), tcu::Sampler::MODE_STENCIL), stencilTexture.getAccess());
		}
	}
	else
		readImageAspect(image, resultLevel->getAccess(), parms);

	return resultLevel;
}

// Copy from image to image.

class CopyImageToImage : public CopiesAndBlittingTestInstance
{
public:
										CopyImageToImage			(Context&	context,
																	 TestParams params);
	virtual tcu::TestStatus				iterate						(void);

protected:
	virtual tcu::TestStatus				checkTestResult				(tcu::ConstPixelBufferAccess result);

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
	Allocator&					memAlloc			= context.getDefaultAllocator();

	VkImageFormatProperties properties;
	if ((context.getInstanceInterface().getPhysicalDeviceImageFormatProperties (context.getPhysicalDevice(),
																				m_params.src.image.format,
																				VK_IMAGE_TYPE_2D,
																				VK_IMAGE_TILING_OPTIMAL,
																				VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
																				0,
																				&properties) == VK_ERROR_FORMAT_NOT_SUPPORTED) ||
		(context.getInstanceInterface().getPhysicalDeviceImageFormatProperties (context.getPhysicalDevice(),
																				m_params.dst.image.format,
																				VK_IMAGE_TYPE_2D,
																				VK_IMAGE_TILING_OPTIMAL,
																				VK_IMAGE_USAGE_TRANSFER_DST_BIT,
																				0,
																				&properties) == VK_ERROR_FORMAT_NOT_SUPPORTED))
	{
		TCU_THROW(NotSupportedError, "Format not supported");
	}

	// Create source image
	{
		const VkImageCreateInfo	sourceImageParams		=
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

		m_source				= createImage(vk, vkDevice, &sourceImageParams);
		m_sourceImageAlloc		= memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_source), MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(vkDevice, *m_source, m_sourceImageAlloc->getMemory(), m_sourceImageAlloc->getOffset()));
	}

	// Create destination image
	{
		const VkImageCreateInfo	destinationImageParams	=
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

tcu::TestStatus CopyImageToImage::iterate (void)
{
	const tcu::TextureFormat	srcTcuFormat		= mapVkFormat(m_params.src.image.format);
	const tcu::TextureFormat	dstTcuFormat		= mapVkFormat(m_params.dst.image.format);
	m_sourceTextureLevel = de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(srcTcuFormat,
																				m_params.src.image.extent.width,
																				m_params.src.image.extent.height,
																				m_params.src.image.extent.depth));
	generateBuffer(m_sourceTextureLevel->getAccess(), m_params.src.image.extent.width, m_params.src.image.extent.height, m_params.src.image.extent.depth, FILL_MODE_RED);
	m_destinationTextureLevel = de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(dstTcuFormat,
																					 (int)m_params.dst.image.extent.width,
																					 (int)m_params.dst.image.extent.height,
																					 (int)m_params.dst.image.extent.depth));
	generateBuffer(m_destinationTextureLevel->getAccess(), m_params.dst.image.extent.width, m_params.dst.image.extent.height, m_params.dst.image.extent.depth, FILL_MODE_GRADIENT);
	generateExpectedResult();

	uploadImage(m_sourceTextureLevel->getAccess(), m_source.get(), m_params.src.image);
	uploadImage(m_destinationTextureLevel->getAccess(), m_destination.get(), m_params.dst.image);

	const DeviceInterface&		vk					= m_context.getDeviceInterface();
	const VkDevice				vkDevice			= m_context.getDevice();
	const VkQueue				queue				= m_context.getUniversalQueue();

	std::vector<VkImageCopy>	imageCopies;
	for (deUint32 i = 0; i < m_params.regions.size(); i++)
		imageCopies.push_back(m_params.regions[i].imageCopy);

	const VkImageMemoryBarrier	imageBarriers[]		=
	{
		// source image
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			srcAccessMask;
			VK_ACCESS_TRANSFER_READ_BIT,				// VkAccessFlags			dstAccessMask;
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			oldLayout;
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,		// VkImageLayout			newLayout;
			VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
			m_source.get(),								// VkImage					image;
			{											// VkImageSubresourceRange	subresourceRange;
				getAspectFlags(srcTcuFormat),	// VkImageAspectFlags	aspectMask;
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
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			oldLayout;
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			newLayout;
			VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
			m_destination.get(),						// VkImage					image;
			{											// VkImageSubresourceRange	subresourceRange;
				getAspectFlags(dstTcuFormat),	// VkImageAspectFlags	aspectMask;
				0u,								// deUint32				baseMipLevel;
				1u,								// deUint32				mipLevels;
				0u,								// deUint32				baseArraySlice;
				1u								// deUint32				arraySize;
			}
		},
	};

	const VkCommandBufferBeginInfo	cmdBufferBeginInfo	=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,			// VkStructureType					sType;
		DE_NULL,												// const void*						pNext;
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,			// VkCommandBufferUsageFlags		flags;
		(const VkCommandBufferInheritanceInfo*)DE_NULL,
	};

	VK_CHECK(vk.beginCommandBuffer(*m_cmdBuffer, &cmdBufferBeginInfo));
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, DE_LENGTH_OF_ARRAY(imageBarriers), imageBarriers);
	vk.cmdCopyImage(*m_cmdBuffer, m_source.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_destination.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (deUint32)m_params.regions.size(), imageCopies.data());
	VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));

	submitCommandsAndWait (vk, vkDevice, queue, *m_cmdBuffer);

	de::MovePtr<tcu::TextureLevel>	resultTextureLevel	= readImage(*m_destination, m_params.dst.image);

	return checkTestResult(resultTextureLevel->getAccess());
}

tcu::TestStatus CopyImageToImage::checkTestResult (tcu::ConstPixelBufferAccess result)
{
	const tcu::Vec4	fThreshold (0.0f);
	const tcu::UVec4 uThreshold (0u);

	if (tcu::isCombinedDepthStencilType(result.getFormat().type))
	{
		if (tcu::hasDepthComponent(result.getFormat().order))
		{
			const tcu::Sampler::DepthStencilMode	mode				= tcu::Sampler::MODE_DEPTH;
			const tcu::ConstPixelBufferAccess		depthResult			= tcu::getEffectiveDepthStencilAccess(result, mode);
			const tcu::ConstPixelBufferAccess		expectedResult		= tcu::getEffectiveDepthStencilAccess(m_expectedTextureLevel->getAccess(), mode);

			if (isFloatFormat(result.getFormat()))
			{
				if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparsion", expectedResult, depthResult, fThreshold, tcu::COMPARE_LOG_RESULT))
					return tcu::TestStatus::fail("CopiesAndBlitting test");
			}
			else
			{
				if (!tcu::intThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparsion", expectedResult, depthResult, uThreshold, tcu::COMPARE_LOG_RESULT))
					return tcu::TestStatus::fail("CopiesAndBlitting test");
			}
		}

		if (tcu::hasStencilComponent(result.getFormat().order))
		{
			const tcu::Sampler::DepthStencilMode	mode				= tcu::Sampler::MODE_STENCIL;
			const tcu::ConstPixelBufferAccess		stencilResult		= tcu::getEffectiveDepthStencilAccess(result, mode);
			const tcu::ConstPixelBufferAccess		expectedResult		= tcu::getEffectiveDepthStencilAccess(m_expectedTextureLevel->getAccess(), mode);

			if (isFloatFormat(result.getFormat()))
			{
				if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparsion", expectedResult, stencilResult, fThreshold, tcu::COMPARE_LOG_RESULT))
					return tcu::TestStatus::fail("CopiesAndBlitting test");
			}
			else
			{
				if (!tcu::intThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparsion", expectedResult, stencilResult, uThreshold, tcu::COMPARE_LOG_RESULT))
					return tcu::TestStatus::fail("CopiesAndBlitting test");
			}
		}
	}
	else
	{
		if (isFloatFormat(result.getFormat()))
		{
			if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparsion", m_expectedTextureLevel->getAccess(), result, fThreshold, tcu::COMPARE_LOG_RESULT))
				return tcu::TestStatus::fail("CopiesAndBlitting test");
		}
		else
		{
			if (!tcu::intThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparsion", m_expectedTextureLevel->getAccess(), result, uThreshold, tcu::COMPARE_LOG_RESULT))
				return tcu::TestStatus::fail("CopiesAndBlitting test");
		}
	}

	return tcu::TestStatus::pass("CopiesAndBlitting test");
}

void CopyImageToImage::copyRegionToTextureLevel (tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst, CopyRegion region)
{
	const VkOffset3D	srcOffset	= region.imageCopy.srcOffset;
	const VkOffset3D	dstOffset	= region.imageCopy.dstOffset;
	const VkExtent3D	extent		= region.imageCopy.extent;

	if (tcu::isCombinedDepthStencilType(src.getFormat().type))
	{
		DE_ASSERT(src.getFormat() == dst.getFormat());

		// Copy depth.
		if (tcu::hasDepthComponent(src.getFormat().order))
		{
			const tcu::ConstPixelBufferAccess	srcSubRegion	= getEffectiveDepthStencilAccess(tcu::getSubregion(src, srcOffset.x, srcOffset.y, srcOffset.z, extent.width, extent.height, extent.depth), tcu::Sampler::MODE_DEPTH);
			const tcu::PixelBufferAccess		dstSubRegion	= getEffectiveDepthStencilAccess(tcu::getSubregion(dst, dstOffset.x, dstOffset.y, dstOffset.z, extent.width, extent.height, extent.depth), tcu::Sampler::MODE_DEPTH);
			tcu::copy(dstSubRegion, srcSubRegion);
		}

		// Copy stencil.
		if (tcu::hasStencilComponent(src.getFormat().order))
		{
			const tcu::ConstPixelBufferAccess	srcSubRegion	= getEffectiveDepthStencilAccess(tcu::getSubregion(src, srcOffset.x, srcOffset.y, srcOffset.z, extent.width, extent.height, extent.depth), tcu::Sampler::MODE_STENCIL);
			const tcu::PixelBufferAccess		dstSubRegion	= getEffectiveDepthStencilAccess(tcu::getSubregion(dst, dstOffset.x, dstOffset.y, dstOffset.z, extent.width, extent.height, extent.depth), tcu::Sampler::MODE_STENCIL);
			tcu::copy(dstSubRegion, srcSubRegion);
		}
	}
	else
	{
		const tcu::ConstPixelBufferAccess	srcSubRegion		= tcu::getSubregion(src, srcOffset.x, srcOffset.y, srcOffset.z, extent.width, extent.height, extent.depth);
		// CopyImage acts like a memcpy. Replace the destination format with the srcformat to use a memcpy.
		const tcu::PixelBufferAccess		dstWithSrcFormat	(srcSubRegion.getFormat(), dst.getSize(), dst.getDataPtr());
		const tcu::PixelBufferAccess		dstSubRegion		= tcu::getSubregion(dstWithSrcFormat, dstOffset.x, dstOffset.y, dstOffset.z, extent.width, extent.height, extent.depth);

		tcu::copy(dstSubRegion, srcSubRegion);
	}
}

class CopyImageToImageTestCase : public vkt::TestCase
{
public:
							CopyImageToImageTestCase	(tcu::TestContext&				testCtx,
														 const std::string&				name,
														 const std::string&				description,
														 const TestParams				params)
								: vkt::TestCase	(testCtx, name, description)
								, m_params		(params)
							{}

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
	Allocator&					memAlloc			= context.getDefaultAllocator();

	// Create source buffer
	{
		const VkBufferCreateInfo	sourceBufferParams		=
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

	// Create destination buffer
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

tcu::TestStatus CopyBufferToBuffer::iterate (void)
{
	const int srcLevelWidth		= (int)(m_params.src.buffer.size/4); // Here the format is VK_FORMAT_R32_UINT, we need to divide the buffer size by 4
	m_sourceTextureLevel		= de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(mapVkFormat(VK_FORMAT_R32_UINT), srcLevelWidth, 1));
	generateBuffer(m_sourceTextureLevel->getAccess(), srcLevelWidth, 1, 1, FILL_MODE_RED);

	const int dstLevelWidth		= (int)(m_params.dst.buffer.size/4);
	m_destinationTextureLevel	= de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(mapVkFormat(VK_FORMAT_R32_UINT), dstLevelWidth, 1));
	generateBuffer(m_destinationTextureLevel->getAccess(), dstLevelWidth, 1, 1, FILL_MODE_WHITE);

	generateExpectedResult();

	uploadBuffer(m_sourceTextureLevel->getAccess(), *m_sourceBufferAlloc);
	uploadBuffer(m_destinationTextureLevel->getAccess(), *m_destinationBufferAlloc);

	const DeviceInterface&		vk			= m_context.getDeviceInterface();
	const VkDevice				vkDevice	= m_context.getDevice();
	const VkQueue				queue		= m_context.getUniversalQueue();

	const VkBufferMemoryBarrier		srcBufferBarrier	=
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType	sType;
		DE_NULL,									// const void*		pNext;
		VK_ACCESS_HOST_WRITE_BIT,					// VkAccessFlags	srcAccessMask;
		VK_ACCESS_TRANSFER_READ_BIT,				// VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			dstQueueFamilyIndex;
		*m_source,									// VkBuffer			buffer;
		0u,											// VkDeviceSize		offset;
		m_params.src.buffer.size					// VkDeviceSize		size;
	};

	const VkBufferMemoryBarrier		dstBufferBarrier	=
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

	std::vector<VkBufferCopy>		bufferCopies;
	for (deUint32 i = 0; i < m_params.regions.size(); i++)
		bufferCopies.push_back(m_params.regions[i].bufferCopy);

	const VkCommandBufferBeginInfo	cmdBufferBeginInfo	=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,			// VkStructureType					sType;
		DE_NULL,												// const void*						pNext;
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,			// VkCommandBufferUsageFlags		flags;
		(const VkCommandBufferInheritanceInfo*)DE_NULL,
	};

	VK_CHECK(vk.beginCommandBuffer(*m_cmdBuffer, &cmdBufferBeginInfo));
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &srcBufferBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
	vk.cmdCopyBuffer(*m_cmdBuffer, m_source.get(), m_destination.get(), (deUint32)m_params.regions.size(), &bufferCopies[0]);
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &dstBufferBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
	VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));

	const VkSubmitInfo				submitInfo			=
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
								: vkt::TestCase	(testCtx, name, description)
								, m_params		(params)
							{}

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
	Allocator&					memAlloc			= context.getDefaultAllocator();

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

tcu::TestStatus CopyImageToBuffer::iterate (void)
{
	m_sourceTextureLevel = de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(m_textureFormat,
																				m_params.src.image.extent.width,
																				m_params.src.image.extent.height,
																				m_params.src.image.extent.depth));
	generateBuffer(m_sourceTextureLevel->getAccess(), m_params.src.image.extent.width, m_params.src.image.extent.height, m_params.src.image.extent.depth);
	m_destinationTextureLevel = de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(m_textureFormat, (int)m_params.dst.buffer.size, 1));
	generateBuffer(m_destinationTextureLevel->getAccess(), (int)m_params.dst.buffer.size, 1, 1);

	generateExpectedResult();

	uploadImage(m_sourceTextureLevel->getAccess(), *m_source, m_params.src.image);
	uploadBuffer(m_destinationTextureLevel->getAccess(), *m_destinationBufferAlloc);

	const DeviceInterface&		vk			= m_context.getDeviceInterface();
	const VkDevice				vkDevice	= m_context.getDevice();
	const VkQueue				queue		= m_context.getUniversalQueue();

	// Barriers for copying image to buffer
	const VkImageMemoryBarrier		imageBarrier		=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			srcAccessMask;
		VK_ACCESS_TRANSFER_READ_BIT,				// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,		// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
		*m_source,									// VkImage					image;
		{											// VkImageSubresourceRange	subresourceRange;
			getAspectFlags(m_textureFormat),	// VkImageAspectFlags	aspectMask;
			0u,								// deUint32				baseMipLevel;
			1u,								// deUint32				mipLevels;
			0u,								// deUint32				baseArraySlice;
			1u								// deUint32				arraySize;
		}
	};

	const VkBufferMemoryBarrier		bufferBarrier		=
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
	std::vector<VkBufferImageCopy>	bufferImageCopies;
	for (deUint32 i = 0; i < m_params.regions.size(); i++)
		bufferImageCopies.push_back(m_params.regions[i].bufferImageCopy);

	const VkCommandBufferBeginInfo	cmdBufferBeginInfo	=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,			// VkStructureType					sType;
		DE_NULL,												// const void*						pNext;
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,			// VkCommandBufferUsageFlags		flags;
		(const VkCommandBufferInheritanceInfo*)DE_NULL,
	};

	VK_CHECK(vk.beginCommandBuffer(*m_cmdBuffer, &cmdBufferBeginInfo));
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &imageBarrier);
	vk.cmdCopyImageToBuffer(*m_cmdBuffer, m_source.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_destination.get(), (deUint32)m_params.regions.size(), &bufferImageCopies[0]);
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &bufferBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
	VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));

	submitCommandsAndWait (vk, vkDevice, queue, *m_cmdBuffer);

	// Read buffer data
	de::MovePtr<tcu::TextureLevel>	resultLevel		(new tcu::TextureLevel(m_textureFormat, (int)m_params.dst.buffer.size, 1));
	invalidateMappedMemoryRange(vk, vkDevice, m_destinationBufferAlloc->getMemory(), m_destinationBufferAlloc->getOffset(), m_bufferSize);
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
								: vkt::TestCase	(testCtx, name, description)
								, m_params		(params)
							{}

	virtual TestInstance*	createInstance				(Context&				context) const
							{
								return new CopyImageToBuffer(context, m_params);
							}
private:
	TestParams				m_params;
};

void CopyImageToBuffer::copyRegionToTextureLevel (tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst, CopyRegion region)
{
	deUint32			rowLength	= region.bufferImageCopy.bufferRowLength;
	if (!rowLength)
		rowLength = region.bufferImageCopy.imageExtent.width;

	deUint32			imageHeight	= region.bufferImageCopy.bufferImageHeight;
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
			int									texelIndex		= texelOffset + (z * imageHeight + y) *	rowLength;
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
	Allocator&					memAlloc			= context.getDefaultAllocator();

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

tcu::TestStatus CopyBufferToImage::iterate (void)
{
	m_sourceTextureLevel = de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(m_textureFormat, (int)m_params.src.buffer.size, 1));
	generateBuffer(m_sourceTextureLevel->getAccess(), (int)m_params.src.buffer.size, 1, 1);
	m_destinationTextureLevel = de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(m_textureFormat,
																					m_params.dst.image.extent.width,
																					m_params.dst.image.extent.height,
																					m_params.dst.image.extent.depth));

	generateBuffer(m_destinationTextureLevel->getAccess(), m_params.dst.image.extent.width, m_params.dst.image.extent.height, m_params.dst.image.extent.depth);

	generateExpectedResult();

	uploadBuffer(m_sourceTextureLevel->getAccess(), *m_sourceBufferAlloc);
	uploadImage(m_destinationTextureLevel->getAccess(), *m_destination, m_params.dst.image);

	const DeviceInterface&		vk			= m_context.getDeviceInterface();
	const VkDevice				vkDevice	= m_context.getDevice();
	const VkQueue				queue		= m_context.getUniversalQueue();

	const VkImageMemoryBarrier	imageBarrier	=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			srcAccessMask;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
		*m_destination,								// VkImage					image;
		{											// VkImageSubresourceRange	subresourceRange;
			getAspectFlags(m_textureFormat),	// VkImageAspectFlags	aspectMask;
			0u,								// deUint32				baseMipLevel;
			1u,								// deUint32				mipLevels;
			0u,								// deUint32				baseArraySlice;
			1u								// deUint32				arraySize;
		}
	};

	// Copy from buffer to image
	std::vector<VkBufferImageCopy>		bufferImageCopies;
	for (deUint32 i = 0; i < m_params.regions.size(); i++)
		bufferImageCopies.push_back(m_params.regions[i].bufferImageCopy);

	const VkCommandBufferBeginInfo	cmdBufferBeginInfo	=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,			// VkStructureType					sType;
		DE_NULL,												// const void*						pNext;
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,			// VkCommandBufferUsageFlags		flags;
		(const VkCommandBufferInheritanceInfo*)DE_NULL,
	};

	VK_CHECK(vk.beginCommandBuffer(*m_cmdBuffer, &cmdBufferBeginInfo));
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &imageBarrier);
	vk.cmdCopyBufferToImage(*m_cmdBuffer, m_source.get(), m_destination.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (deUint32)m_params.regions.size(), bufferImageCopies.data());
	VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));

	submitCommandsAndWait (vk, vkDevice, queue, *m_cmdBuffer);

	de::MovePtr<tcu::TextureLevel>	resultLevel	= readImage(*m_destination, m_params.dst.image);

	return checkTestResult(resultLevel->getAccess());
}

class CopyBufferToImageTestCase : public vkt::TestCase
{
public:
							CopyBufferToImageTestCase	(tcu::TestContext&		testCtx,
														 const std::string&		name,
														 const std::string&		description,
														 const TestParams		params)
								: vkt::TestCase	(testCtx, name, description)
								, m_params		(params)
							{}

	virtual					~CopyBufferToImageTestCase	(void) {}

	virtual TestInstance*	createInstance				(Context&				context) const
							{
								return new CopyBufferToImage(context, m_params);
							}
private:
	TestParams				m_params;
};

void CopyBufferToImage::copyRegionToTextureLevel (tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst, CopyRegion region)
{
	deUint32			rowLength	= region.bufferImageCopy.bufferRowLength;
	if (!rowLength)
		rowLength = region.bufferImageCopy.imageExtent.width;

	deUint32			imageHeight	= region.bufferImageCopy.bufferImageHeight;
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
			int									texelIndex		= texelOffset + (z * imageHeight + y) *	rowLength;
			const tcu::ConstPixelBufferAccess	srcSubRegion	= tcu::getSubregion(src, texelIndex, 0, region.bufferImageCopy.imageExtent.width, 1);
			const tcu::PixelBufferAccess		dstSubRegion	= tcu::getSubregion(dst, dstOffset.x, dstOffset.y + y, dstOffset.z + z,
																					region.bufferImageCopy.imageExtent.width, 1, 1);
			tcu::copy(dstSubRegion, srcSubRegion);
		}
	}
}

// Copy from image to image with scaling.

class BlittingImages : public CopiesAndBlittingTestInstance
{
public:
										BlittingImages					(Context&	context,
																		 TestParams params);
	virtual tcu::TestStatus				iterate							(void);
protected:
	virtual tcu::TestStatus				checkTestResult					(tcu::ConstPixelBufferAccess result);
	virtual void						copyRegionToTextureLevel		(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst, CopyRegion region);
	virtual void						generateExpectedResult			(void);
private:
	bool								checkClampedAndUnclampedResult	(const tcu::ConstPixelBufferAccess&	result,
																		 const tcu::ConstPixelBufferAccess&	clampedReference,
																		 const tcu::ConstPixelBufferAccess&	unclampedReference,
																		 VkImageAspectFlagBits				aspect);
	Move<VkImage>						m_source;
	de::MovePtr<Allocation>				m_sourceImageAlloc;
	Move<VkImage>						m_destination;
	de::MovePtr<Allocation>				m_destinationImageAlloc;

	de::MovePtr<tcu::TextureLevel>		m_unclampedExpectedTextureLevel;
};

BlittingImages::BlittingImages (Context& context, TestParams params)
	: CopiesAndBlittingTestInstance(context, params)
{
	const DeviceInterface&		vk					= context.getDeviceInterface();
	const VkDevice				vkDevice			= context.getDevice();
	const deUint32				queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	Allocator&					memAlloc			= context.getDefaultAllocator();

	VkImageFormatProperties properties;
	if ((context.getInstanceInterface().getPhysicalDeviceImageFormatProperties (context.getPhysicalDevice(),
																				m_params.src.image.format,
																				VK_IMAGE_TYPE_2D,
																				VK_IMAGE_TILING_OPTIMAL,
																				VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
																				0,
																				&properties) == VK_ERROR_FORMAT_NOT_SUPPORTED) ||
		(context.getInstanceInterface().getPhysicalDeviceImageFormatProperties (context.getPhysicalDevice(),
																				m_params.dst.image.format,
																				VK_IMAGE_TYPE_2D,
																				VK_IMAGE_TILING_OPTIMAL,
																				VK_IMAGE_USAGE_TRANSFER_DST_BIT,
																				0,
																				&properties) == VK_ERROR_FORMAT_NOT_SUPPORTED))
	{
		TCU_THROW(NotSupportedError, "Format not supported");
	}

	VkFormatProperties srcFormatProperties;
	context.getInstanceInterface().getPhysicalDeviceFormatProperties(context.getPhysicalDevice(), m_params.src.image.format, &srcFormatProperties);
	if (!(srcFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT))
	{
		TCU_THROW(NotSupportedError, "Format feature blit source not supported");
	}

	VkFormatProperties dstFormatProperties;
	context.getInstanceInterface().getPhysicalDeviceFormatProperties(context.getPhysicalDevice(), m_params.dst.image.format, &dstFormatProperties);
	if (!(dstFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT))
	{
		TCU_THROW(NotSupportedError, "Format feature blit destination not supported");
	}

	if (m_params.filter == VK_FILTER_LINEAR)
	{
		if (!(srcFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
			TCU_THROW(NotSupportedError, "Source format feature sampled image filter linear not supported");
		if (!(dstFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
			TCU_THROW(NotSupportedError, "Destination format feature sampled image filter linear not supported");
	}

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

		m_source = createImage(vk, vkDevice, &sourceImageParams);
		m_sourceImageAlloc = memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_source), MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(vkDevice, *m_source, m_sourceImageAlloc->getMemory(), m_sourceImageAlloc->getOffset()));
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

		m_destination = createImage(vk, vkDevice, &destinationImageParams);
		m_destinationImageAlloc = memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_destination), MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(vkDevice, *m_destination, m_destinationImageAlloc->getMemory(), m_destinationImageAlloc->getOffset()));
	}
}

tcu::TestStatus BlittingImages::iterate (void)
{
	const tcu::TextureFormat	srcTcuFormat		= mapVkFormat(m_params.src.image.format);
	const tcu::TextureFormat	dstTcuFormat		= mapVkFormat(m_params.dst.image.format);
	m_sourceTextureLevel = de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(srcTcuFormat,
																				m_params.src.image.extent.width,
																				m_params.src.image.extent.height,
																				m_params.src.image.extent.depth));
	generateBuffer(m_sourceTextureLevel->getAccess(), m_params.src.image.extent.width, m_params.src.image.extent.height, m_params.src.image.extent.depth, FILL_MODE_GRADIENT);
	m_destinationTextureLevel = de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(dstTcuFormat,
																					 (int)m_params.dst.image.extent.width,
																					 (int)m_params.dst.image.extent.height,
																					 (int)m_params.dst.image.extent.depth));
	generateBuffer(m_destinationTextureLevel->getAccess(), m_params.dst.image.extent.width, m_params.dst.image.extent.height, m_params.dst.image.extent.depth, FILL_MODE_WHITE);
	generateExpectedResult();

	uploadImage(m_sourceTextureLevel->getAccess(), m_source.get(), m_params.src.image);
	uploadImage(m_destinationTextureLevel->getAccess(), m_destination.get(), m_params.dst.image);

	const DeviceInterface&		vk					= m_context.getDeviceInterface();
	const VkDevice				vkDevice			= m_context.getDevice();
	const VkQueue				queue				= m_context.getUniversalQueue();

	std::vector<VkImageBlit>	regions;
	for (deUint32 i = 0; i < m_params.regions.size(); i++)
		regions.push_back(m_params.regions[i].imageBlit);

	// Barriers for copying image to buffer
	const VkImageMemoryBarrier		srcImageBarrier		=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			srcAccessMask;
		VK_ACCESS_TRANSFER_READ_BIT,				// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,		// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
		m_source.get(),								// VkImage					image;
		{											// VkImageSubresourceRange	subresourceRange;
			getAspectFlags(srcTcuFormat),	// VkImageAspectFlags	aspectMask;
			0u,								// deUint32				baseMipLevel;
			1u,								// deUint32				mipLevels;
			0u,								// deUint32				baseArraySlice;
			1u								// deUint32				arraySize;
		}
	};

	const VkImageMemoryBarrier		dstImageBarrier		=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			srcAccessMask;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
		m_destination.get(),						// VkImage					image;
		{											// VkImageSubresourceRange	subresourceRange;
			getAspectFlags(dstTcuFormat),	// VkImageAspectFlags	aspectMask;
			0u,								// deUint32				baseMipLevel;
			1u,								// deUint32				mipLevels;
			0u,								// deUint32				baseArraySlice;
			1u								// deUint32				arraySize;
		}
	};

	const VkCommandBufferBeginInfo	cmdBufferBeginInfo	=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,			// VkStructureType					sType;
		DE_NULL,												// const void*						pNext;
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,			// VkCommandBufferUsageFlags		flags;
		(const VkCommandBufferInheritanceInfo*)DE_NULL,
	};

	VK_CHECK(vk.beginCommandBuffer(*m_cmdBuffer, &cmdBufferBeginInfo));
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &srcImageBarrier);
	vk.cmdBlitImage(*m_cmdBuffer, m_source.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_destination.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (deUint32)m_params.regions.size(), &regions[0], m_params.filter);
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &dstImageBarrier);
	VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));

	submitCommandsAndWait (vk, vkDevice, queue, *m_cmdBuffer);

	de::MovePtr<tcu::TextureLevel> resultTextureLevel = readImage(*m_destination, m_params.dst.image);

	return checkTestResult(resultTextureLevel->getAccess());
}

static float calculateFloatConversionError (int srcBits)
{
	if (srcBits > 0)
	{
		const int	clampedBits	= de::clamp<int>(srcBits, 0, 32);
		const float	srcMaxValue	= de::max((float)(1ULL<<clampedBits) - 1.0f, 1.0f);
		const float	error		= 1.0f / srcMaxValue;

		return de::clamp<float>(error, 0.0f, 1.0f);
	}
	else
		return 1.0f;
}

tcu::Vec4 getFormatThreshold (const tcu::TextureFormat& format)
{
	tcu::Vec4 threshold(0.01f);

	switch (format.type)
	{
	case tcu::TextureFormat::HALF_FLOAT:
		threshold = tcu::Vec4(0.005f);
		break;

	case tcu::TextureFormat::FLOAT:
	case tcu::TextureFormat::FLOAT64:
		threshold = tcu::Vec4(0.001f);
		break;

	case tcu::TextureFormat::UNSIGNED_INT_11F_11F_10F_REV:
		threshold = tcu::Vec4(0.02f, 0.02f, 0.0625f, 1.0f);
		break;

	case tcu::TextureFormat::UNSIGNED_INT_999_E5_REV:
		threshold = tcu::Vec4(0.05f, 0.05f, 0.05f, 1.0f);
		break;

	default:
		const tcu::IVec4 bits = tcu::getTextureFormatMantissaBitDepth(format);
		threshold = tcu::Vec4(calculateFloatConversionError(bits.x()),
				      calculateFloatConversionError(bits.y()),
				      calculateFloatConversionError(bits.z()),
				      calculateFloatConversionError(bits.w()));
	}

	// Return value matching the channel order specified by the format
	if (format.order == tcu::TextureFormat::BGR || format.order == tcu::TextureFormat::BGRA)
		return threshold.swizzle(2, 1, 0, 3);
	else
		return threshold;
}

tcu::TextureFormat getFormatAspect (VkFormat format, VkImageAspectFlagBits aspect)
{
	const tcu::TextureFormat	baseFormat	= mapVkFormat(format);

	if (isCombinedDepthStencilType(baseFormat.type))
	{
		if (aspect == VK_IMAGE_ASPECT_DEPTH_BIT)
			return getEffectiveDepthStencilTextureFormat(baseFormat, tcu::Sampler::MODE_DEPTH);
		else if (aspect == VK_IMAGE_ASPECT_STENCIL_BIT)
			return getEffectiveDepthStencilTextureFormat(baseFormat, tcu::Sampler::MODE_STENCIL);
		else
			DE_FATAL("Invalid aspect");
	}

	return baseFormat;
}

bool BlittingImages::checkClampedAndUnclampedResult (const tcu::ConstPixelBufferAccess&	result,
													 const tcu::ConstPixelBufferAccess& clampedExpected,
													 const tcu::ConstPixelBufferAccess& unclampedExpected,
													 VkImageAspectFlagBits				aspect)
{
	tcu::TestLog&				log			(m_context.getTestContext().getLog());
	const bool					isLinear	= m_params.filter == VK_FILTER_LINEAR;
	const tcu::TextureFormat	srcFormat	= getFormatAspect(m_params.src.image.format, aspect);
	const tcu::TextureFormat	dstFormat	= result.getFormat();
	bool						isOk		= false;

	DE_ASSERT(dstFormat == getFormatAspect(m_params.dst.image.format, aspect));

	if (isLinear)
		log << tcu::TestLog::Section("ClampedSourceImage", "Region with clamped edges on source image.");

	if (isFloatFormat(dstFormat))
	{
		const bool		srcIsSRGB	= tcu::isSRGB(srcFormat);
		const tcu::Vec4	srcMaxDiff	= getFormatThreshold(srcFormat) * tcu::Vec4(srcIsSRGB ? 2.0f : 1.0f);
		const tcu::Vec4	dstMaxDiff	= getFormatThreshold(dstFormat);
		const tcu::Vec4	threshold	= tcu::max(srcMaxDiff, dstMaxDiff);

		isOk = tcu::floatThresholdCompare(log, "Compare", "Result comparsion", clampedExpected, result, threshold, tcu::COMPARE_LOG_RESULT);

		if (isLinear)
			log << tcu::TestLog::EndSection;

		if (!isOk && isLinear)
		{
			log << tcu::TestLog::Section("NonClampedSourceImage", "Region with non-clamped edges on source image.");
			isOk = tcu::floatThresholdCompare(log, "Compare", "Result comparsion", unclampedExpected, result, threshold, tcu::COMPARE_LOG_RESULT);
			log << tcu::TestLog::EndSection;
		}
	}
	else
	{
		tcu::UVec4	threshold;
		// Calculate threshold depending on channel width of destination format.
		const tcu::IVec4	bitDepth	= tcu::getTextureFormatBitDepth(dstFormat);
		for (deUint32 i = 0; i < 4; ++i)
			threshold[i] = de::max( (0x1 << bitDepth[i]) / 256, 1);

		isOk = tcu::intThresholdCompare(log, "Compare", "Result comparsion", clampedExpected, result, threshold, tcu::COMPARE_LOG_RESULT);

		if (isLinear)
			log << tcu::TestLog::EndSection;

		if (!isOk && isLinear)
		{
			log << tcu::TestLog::Section("NonClampedSourceImage", "Region with non-clamped edges on source image.");
			isOk = tcu::intThresholdCompare(log, "Compare", "Result comparsion", unclampedExpected, result, threshold, tcu::COMPARE_LOG_RESULT);
			log << tcu::TestLog::EndSection;
		}
	}
	return isOk;
}

tcu::TestStatus BlittingImages::checkTestResult (tcu::ConstPixelBufferAccess result)
{
	DE_ASSERT(m_params.filter == VK_FILTER_NEAREST || m_params.filter == VK_FILTER_LINEAR);

	if (tcu::isCombinedDepthStencilType(result.getFormat().type))
	{
		if (tcu::hasDepthComponent(result.getFormat().order))
		{
			const tcu::Sampler::DepthStencilMode	mode				= tcu::Sampler::MODE_DEPTH;
			const tcu::ConstPixelBufferAccess		depthResult			= tcu::getEffectiveDepthStencilAccess(result, mode);
			const tcu::ConstPixelBufferAccess		clampedExpected		= tcu::getEffectiveDepthStencilAccess(m_expectedTextureLevel->getAccess(), mode);
			const tcu::ConstPixelBufferAccess		unclampedExpected	= m_params.filter == VK_FILTER_LINEAR ? tcu::getEffectiveDepthStencilAccess(m_unclampedExpectedTextureLevel->getAccess(), mode) : tcu::ConstPixelBufferAccess();

			if (!checkClampedAndUnclampedResult(depthResult, clampedExpected, unclampedExpected, VK_IMAGE_ASPECT_DEPTH_BIT))
			{
				return tcu::TestStatus::fail("CopiesAndBlitting test");
			}
		}

		if (tcu::hasStencilComponent(result.getFormat().order))
		{
			const tcu::Sampler::DepthStencilMode	mode				= tcu::Sampler::MODE_STENCIL;
			const tcu::ConstPixelBufferAccess		stencilResult		= tcu::getEffectiveDepthStencilAccess(result, mode);
			const tcu::ConstPixelBufferAccess		clampedExpected		= tcu::getEffectiveDepthStencilAccess(m_expectedTextureLevel->getAccess(), mode);
			const tcu::ConstPixelBufferAccess		unclampedExpected	= m_params.filter == VK_FILTER_LINEAR ? tcu::getEffectiveDepthStencilAccess(m_unclampedExpectedTextureLevel->getAccess(), mode) : tcu::ConstPixelBufferAccess();

			if (!checkClampedAndUnclampedResult(stencilResult, clampedExpected, unclampedExpected, VK_IMAGE_ASPECT_STENCIL_BIT))
			{
				return tcu::TestStatus::fail("CopiesAndBlitting test");
			}
		}
	}
	else
	{
		if (!checkClampedAndUnclampedResult(result, m_expectedTextureLevel->getAccess(), m_params.filter == VK_FILTER_LINEAR ? m_unclampedExpectedTextureLevel->getAccess() : tcu::ConstPixelBufferAccess(), VK_IMAGE_ASPECT_COLOR_BIT))
		{
			return tcu::TestStatus::fail("CopiesAndBlitting test");
		}
	}

	return tcu::TestStatus::pass("CopiesAndBlitting test");
}

tcu::Vec4 linearToSRGBIfNeeded (const tcu::TextureFormat& format, const tcu::Vec4& color)
{
	return isSRGB(format) ? linearToSRGB(color) : color;
}

void scaleFromWholeSrcBuffer (const tcu::PixelBufferAccess& dst, const tcu::ConstPixelBufferAccess& src, const VkOffset3D regionOffset, const VkOffset3D regionExtent, tcu::Sampler::FilterMode filter)
{
	DE_ASSERT(filter == tcu::Sampler::LINEAR);
	DE_ASSERT(dst.getDepth() == 1 && src.getDepth() == 1);

	tcu::Sampler sampler(tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE,
					filter, filter, 0.0f, false);

	float sX = (float)regionExtent.x / (float)dst.getWidth();
	float sY = (float)regionExtent.y / (float)dst.getHeight();

	for (int y = 0; y < dst.getHeight(); y++)
	for (int x = 0; x < dst.getWidth(); x++)
		dst.setPixel(linearToSRGBIfNeeded(dst.getFormat(), src.sample2D(sampler, filter, (float)regionOffset.x + ((float)x+0.5f)*sX, (float)regionOffset.y + ((float)y+0.5f)*sY, 0)), x, y);
}

void blit (const tcu::PixelBufferAccess& dst, const tcu::ConstPixelBufferAccess& src, const tcu::Sampler::FilterMode filter, const MirrorMode mirrorMode)
{
	DE_ASSERT(filter == tcu::Sampler::NEAREST || filter == tcu::Sampler::LINEAR);

	tcu::Sampler sampler(tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE,
						 filter, filter, 0.0f, false);

	const float sX = (float)src.getWidth() / (float)dst.getWidth();
	const float sY = (float)src.getHeight() / (float)dst.getHeight();
	const float sZ = (float)src.getDepth() / (float)dst.getDepth();

	tcu::Mat2 rotMatrix;
	rotMatrix(0,0) = (mirrorMode & MIRROR_MODE_X) ? -1.0f : 1.0f;
	rotMatrix(0,1) = 0.0f;
	rotMatrix(1,0) = 0.0f;
	rotMatrix(1,1) = (mirrorMode & MIRROR_MODE_Y) ? -1.0f : 1.0f;

	const int xOffset = (mirrorMode & MIRROR_MODE_X) ? dst.getWidth() - 1 : 0;
	const int yOffset = (mirrorMode & MIRROR_MODE_Y) ? dst.getHeight() - 1 : 0;

	if (dst.getDepth() == 1 && src.getDepth() == 1)
	{
		for (int y = 0; y < dst.getHeight(); ++y)
		for (int x = 0; x < dst.getWidth(); ++x)
		{
			const tcu::Vec2 xy = rotMatrix * tcu::Vec2((float)x,(float)y);
			dst.setPixel(linearToSRGBIfNeeded(dst.getFormat(), src.sample2D(sampler, filter, ((float)x+0.5f)*sX, ((float)y+0.5f)*sY, 0)), (int)round(xy[0]) + xOffset, (int)round(xy[1]) + yOffset);
		}
	}
	else
	{
		for (int z = 0; z < dst.getDepth(); ++z)
		for (int y = 0; y < dst.getHeight(); ++y)
		for (int x = 0; x < dst.getWidth(); ++x)
		{
			const tcu::Vec2 xy = rotMatrix * tcu::Vec2((float)x,(float)y);
			dst.setPixel(linearToSRGBIfNeeded(dst.getFormat(), src.sample3D(sampler, filter, ((float)x+0.5f)*sX, ((float)y+0.5f)*sY, ((float)z+0.5f)*sZ)), (int)round(xy[0]) + xOffset, (int)round(xy[1]) + yOffset, z);
		}
	}
}

void flipCoordinates (CopyRegion& region, const MirrorMode mirrorMode)
{
	const VkOffset3D dstOffset0 = region.imageBlit.dstOffsets[0];
	const VkOffset3D dstOffset1 = region.imageBlit.dstOffsets[1];
	const VkOffset3D srcOffset0 = region.imageBlit.srcOffsets[0];
	const VkOffset3D srcOffset1 = region.imageBlit.srcOffsets[1];

	if (mirrorMode > MIRROR_MODE_NONE && mirrorMode < MIRROR_MODE_LAST)
	{
		//sourceRegion
		region.imageBlit.srcOffsets[0].x = std::min(srcOffset0.x, srcOffset1.x);
		region.imageBlit.srcOffsets[0].y = std::min(srcOffset0.y, srcOffset1.y);

		region.imageBlit.srcOffsets[1].x = std::max(srcOffset0.x, srcOffset1.x);
		region.imageBlit.srcOffsets[1].y = std::max(srcOffset0.y, srcOffset1.y);

		//destinationRegion
		region.imageBlit.dstOffsets[0].x = std::min(dstOffset0.x, dstOffset1.x);
		region.imageBlit.dstOffsets[0].y = std::min(dstOffset0.y, dstOffset1.y);

		region.imageBlit.dstOffsets[1].x = std::max(dstOffset0.x, dstOffset1.x);
		region.imageBlit.dstOffsets[1].y = std::max(dstOffset0.y, dstOffset1.y);
	}
}

MirrorMode getMirrorMode(const VkOffset3D x1, const VkOffset3D x2)
{
	if (x1.x >= x2.x && x1.y >= x2.y)
	{
		return MIRROR_MODE_XY;
	}
	else if (x1.x <= x2.x && x1.y <= x2.y)
	{
		return MIRROR_MODE_NONE;
	}
	else if (x1.x <= x2.x && x1.y >= x2.y)
	{
		return MIRROR_MODE_Y;
	}
	else if (x1.x >= x2.x && x1.y <= x2.y)
	{
		return MIRROR_MODE_X;
	}
	return MIRROR_MODE_LAST;
}

MirrorMode getMirrorMode(const VkOffset3D s1, const VkOffset3D s2, const VkOffset3D d1, const VkOffset3D d2)
{
	const MirrorMode source		 = getMirrorMode(s1, s2);
	const MirrorMode destination = getMirrorMode(d1, d2);

	if (source == destination)
	{
		return MIRROR_MODE_NONE;
	}
	else if ((source == MIRROR_MODE_XY && destination == MIRROR_MODE_X)	  || (destination == MIRROR_MODE_XY && source == MIRROR_MODE_X) ||
			 (source == MIRROR_MODE_Y && destination == MIRROR_MODE_NONE) || (destination == MIRROR_MODE_Y && source == MIRROR_MODE_NONE))
	{
		return MIRROR_MODE_Y;
	}
	else if ((source == MIRROR_MODE_XY && destination == MIRROR_MODE_Y)	  || (destination == MIRROR_MODE_XY && source == MIRROR_MODE_Y) ||
			 (source == MIRROR_MODE_X && destination == MIRROR_MODE_NONE) || (destination == MIRROR_MODE_X && source == MIRROR_MODE_NONE))
	{
		return MIRROR_MODE_X;
	}
	else if ((source == MIRROR_MODE_XY && destination == MIRROR_MODE_NONE) || (destination == MIRROR_MODE_XY && source == MIRROR_MODE_NONE))
	{
		return MIRROR_MODE_XY;
	}
	return MIRROR_MODE_LAST;
}

void BlittingImages::copyRegionToTextureLevel (tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst, CopyRegion region)
{
	const MirrorMode mirrorMode = getMirrorMode(region.imageBlit.srcOffsets[0],
												region.imageBlit.srcOffsets[1],
												region.imageBlit.dstOffsets[0],
												region.imageBlit.dstOffsets[1]);

	flipCoordinates(region, mirrorMode);

	const VkOffset3D					srcOffset		= region.imageBlit.srcOffsets[0];
	const VkOffset3D					srcExtent		=
	{
		region.imageBlit.srcOffsets[1].x - srcOffset.x,
		region.imageBlit.srcOffsets[1].y - srcOffset.y,
		region.imageBlit.srcOffsets[1].z - srcOffset.z
	};
	const VkOffset3D					dstOffset		= region.imageBlit.dstOffsets[0];
	const VkOffset3D					dstExtent		=
	{
		region.imageBlit.dstOffsets[1].x - dstOffset.x,
		region.imageBlit.dstOffsets[1].y - dstOffset.y,
		region.imageBlit.dstOffsets[1].z - dstOffset.z
	};
	const tcu::Sampler::FilterMode		filter			= (m_params.filter == VK_FILTER_LINEAR) ? tcu::Sampler::LINEAR : tcu::Sampler::NEAREST;

	if (tcu::isCombinedDepthStencilType(src.getFormat().type))
	{
		DE_ASSERT(src.getFormat() == dst.getFormat());
		// Scale depth.
		if (tcu::hasDepthComponent(src.getFormat().order))
		{
			const tcu::ConstPixelBufferAccess	srcSubRegion	= getEffectiveDepthStencilAccess(tcu::getSubregion(src, srcOffset.x, srcOffset.y, srcExtent.x, srcExtent.y), tcu::Sampler::MODE_DEPTH);
			const tcu::PixelBufferAccess		dstSubRegion	= getEffectiveDepthStencilAccess(tcu::getSubregion(dst, dstOffset.x, dstOffset.y, dstExtent.x, dstExtent.y), tcu::Sampler::MODE_DEPTH);
			tcu::scale(dstSubRegion, srcSubRegion, filter);

			if (filter == tcu::Sampler::LINEAR)
			{
				const tcu::ConstPixelBufferAccess	depthSrc			= getEffectiveDepthStencilAccess(src, tcu::Sampler::MODE_DEPTH);
				const tcu::PixelBufferAccess		unclampedSubRegion	= getEffectiveDepthStencilAccess(tcu::getSubregion(m_unclampedExpectedTextureLevel->getAccess(), dstOffset.x, dstOffset.y, dstExtent.x, dstExtent.y), tcu::Sampler::MODE_DEPTH);
				scaleFromWholeSrcBuffer(unclampedSubRegion, depthSrc, srcOffset, srcExtent, filter);
			}
		}

		// Scale stencil.
		if (tcu::hasStencilComponent(src.getFormat().order))
		{
			const tcu::ConstPixelBufferAccess	srcSubRegion	= getEffectiveDepthStencilAccess(tcu::getSubregion(src, srcOffset.x, srcOffset.y, srcExtent.x, srcExtent.y), tcu::Sampler::MODE_STENCIL);
			const tcu::PixelBufferAccess		dstSubRegion	= getEffectiveDepthStencilAccess(tcu::getSubregion(dst, dstOffset.x, dstOffset.y, dstExtent.x, dstExtent.y), tcu::Sampler::MODE_STENCIL);
			blit(dstSubRegion, srcSubRegion, filter, mirrorMode);

			if (filter == tcu::Sampler::LINEAR)
			{
				const tcu::ConstPixelBufferAccess	stencilSrc			= getEffectiveDepthStencilAccess(src, tcu::Sampler::MODE_STENCIL);
				const tcu::PixelBufferAccess		unclampedSubRegion	= getEffectiveDepthStencilAccess(tcu::getSubregion(m_unclampedExpectedTextureLevel->getAccess(), dstOffset.x, dstOffset.y, dstExtent.x, dstExtent.y), tcu::Sampler::MODE_STENCIL);
				scaleFromWholeSrcBuffer(unclampedSubRegion, stencilSrc, srcOffset, srcExtent, filter);
			}
		}
	}
	else
	{
		const tcu::ConstPixelBufferAccess	srcSubRegion	= tcu::getSubregion(src, srcOffset.x, srcOffset.y, srcExtent.x, srcExtent.y);
		const tcu::PixelBufferAccess		dstSubRegion	= tcu::getSubregion(dst, dstOffset.x, dstOffset.y, dstExtent.x, dstExtent.y);
		blit(dstSubRegion, srcSubRegion, filter, mirrorMode);

		if (filter == tcu::Sampler::LINEAR)
		{
			const tcu::PixelBufferAccess	unclampedSubRegion	= tcu::getSubregion(m_unclampedExpectedTextureLevel->getAccess(), dstOffset.x, dstOffset.y, dstExtent.x, dstExtent.y);
			scaleFromWholeSrcBuffer(unclampedSubRegion, src, srcOffset, srcExtent, filter);
		}
	}
}

void BlittingImages::generateExpectedResult (void)
{
	const tcu::ConstPixelBufferAccess	src	= m_sourceTextureLevel->getAccess();
	const tcu::ConstPixelBufferAccess	dst	= m_destinationTextureLevel->getAccess();

	m_expectedTextureLevel			= de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(dst.getFormat(), dst.getWidth(), dst.getHeight(), dst.getDepth()));
	tcu::copy(m_expectedTextureLevel->getAccess(), dst);

	if (m_params.filter == VK_FILTER_LINEAR)
	{
		m_unclampedExpectedTextureLevel	= de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(dst.getFormat(), dst.getWidth(), dst.getHeight(), dst.getDepth()));
		tcu::copy(m_unclampedExpectedTextureLevel->getAccess(), dst);
	}

	for (deUint32 i = 0; i < m_params.regions.size(); i++)
	{
		CopyRegion region = m_params.regions[i];
		copyRegionToTextureLevel(src, m_expectedTextureLevel->getAccess(), region);
	}
}

class BlittingTestCase : public vkt::TestCase
{
public:
							BlittingTestCase		(tcu::TestContext&				testCtx,
													 const std::string&				name,
													 const std::string&				description,
													 const TestParams				params)
								: vkt::TestCase	(testCtx, name, description)
								, m_params		(params)
							{}

	virtual TestInstance*	createInstance			(Context&						context) const
							{
								return new BlittingImages(context, m_params);
							}
private:
	TestParams				m_params;
};

// Resolve image to image.

enum ResolveImageToImageOptions{NO_OPTIONAL_OPERATION, COPY_MS_IMAGE_TO_MS_IMAGE, COPY_MS_IMAGE_TO_ARRAY_MS_IMAGE};
class ResolveImageToImage : public CopiesAndBlittingTestInstance
{
public:
												ResolveImageToImage			(Context&							context,
																			 TestParams							params,
																			 const ResolveImageToImageOptions	options);
	virtual tcu::TestStatus						iterate						(void);
protected:
	virtual tcu::TestStatus						checkTestResult				(tcu::ConstPixelBufferAccess result);
	void										copyMSImageToMSImage		(void);
private:
	Move<VkImage>								m_multisampledImage;
	de::MovePtr<Allocation>						m_multisampledImageAlloc;

	Move<VkImage>								m_destination;
	de::MovePtr<Allocation>						m_destinationImageAlloc;

	Move<VkImage>								m_multisampledCopyImage;
	de::MovePtr<Allocation>						m_multisampledCopyImageAlloc;

	const ResolveImageToImageOptions			m_options;

	virtual void								copyRegionToTextureLevel	(tcu::ConstPixelBufferAccess	src,
																			 tcu::PixelBufferAccess			dst,
																			 CopyRegion						region);
};

ResolveImageToImage::ResolveImageToImage (Context& context, TestParams params, const ResolveImageToImageOptions options)
	: CopiesAndBlittingTestInstance	(context, params)
	, m_options						(options)
{
	const VkSampleCountFlagBits	rasterizationSamples	= m_params.samples;

	if (!(context.getDeviceProperties().limits.framebufferColorSampleCounts & rasterizationSamples))
		throw tcu::NotSupportedError("Unsupported number of rasterization samples");

	const DeviceInterface&		vk						= context.getDeviceInterface();
	const VkDevice				vkDevice				= context.getDevice();
	const deUint32				queueFamilyIndex		= context.getUniversalQueueFamilyIndex();
	Allocator&					memAlloc				= m_context.getDefaultAllocator();

	const VkComponentMapping	componentMappingRGBA	= { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
	Move<VkRenderPass>			renderPass;

	Move<VkShaderModule>		vertexShaderModule		= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("vert"), 0);
	Move<VkShaderModule>		fragmentShaderModule	= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("frag"), 0);
	std::vector<tcu::Vec4>		vertices;

	Move<VkBuffer>				vertexBuffer;
	de::MovePtr<Allocation>		vertexBufferAlloc;

	Move<VkPipelineLayout>		pipelineLayout;
	Move<VkPipeline>			graphicsPipeline;

	VkImageFormatProperties properties;
	if ((context.getInstanceInterface().getPhysicalDeviceImageFormatProperties (context.getPhysicalDevice(),
																				m_params.src.image.format,
																				m_params.src.image.imageType,
																				VK_IMAGE_TILING_OPTIMAL,
																				VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 0,
																				&properties) == VK_ERROR_FORMAT_NOT_SUPPORTED) ||
		(context.getInstanceInterface().getPhysicalDeviceImageFormatProperties (context.getPhysicalDevice(),
																				m_params.dst.image.format,
																				m_params.dst.image.imageType,
																				VK_IMAGE_TILING_OPTIMAL,
																				VK_IMAGE_USAGE_TRANSFER_DST_BIT, 0,
																				&properties) == VK_ERROR_FORMAT_NOT_SUPPORTED))
	{
		TCU_THROW(NotSupportedError, "Format not supported");
	}

	// Create color image.
	{
		VkImageCreateInfo	colorImageParams	=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,									// VkStructureType			sType;
			DE_NULL,																// const void*				pNext;
			0u,																		// VkImageCreateFlags		flags;
			m_params.src.image.imageType,											// VkImageType				imageType;
			m_params.src.image.format,												// VkFormat					format;
			getExtent3D(m_params.src.image),										// VkExtent3D				extent;
			1u,																		// deUint32					mipLevels;
			getArraySize(m_params.src.image),										// deUint32					arrayLayers;
			rasterizationSamples,													// VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,												// VkImageTiling			tiling;
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,	// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,												// VkSharingMode			sharingMode;
			1u,																		// deUint32					queueFamilyIndexCount;
			&queueFamilyIndex,														// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED,												// VkImageLayout			initialLayout;
		};

		m_multisampledImage				= createImage(vk, vkDevice, &colorImageParams);

		// Allocate and bind color image memory.
		m_multisampledImageAlloc		= memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_multisampledImage), MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(vkDevice, *m_multisampledImage, m_multisampledImageAlloc->getMemory(), m_multisampledImageAlloc->getOffset()));

		switch (m_options)
		{
			case COPY_MS_IMAGE_TO_MS_IMAGE:
			{
				colorImageParams.usage			= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
				m_multisampledCopyImage			= createImage(vk, vkDevice, &colorImageParams);
				// Allocate and bind color image memory.
				m_multisampledCopyImageAlloc	= memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_multisampledCopyImage), MemoryRequirement::Any);
				VK_CHECK(vk.bindImageMemory(vkDevice, *m_multisampledCopyImage, m_multisampledCopyImageAlloc->getMemory(), m_multisampledCopyImageAlloc->getOffset()));
				break;
			}

			case COPY_MS_IMAGE_TO_ARRAY_MS_IMAGE:
			{
				colorImageParams.usage			= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
				colorImageParams.arrayLayers	= getArraySize(m_params.dst.image);
				m_multisampledCopyImage			= createImage(vk, vkDevice, &colorImageParams);
				// Allocate and bind color image memory.
				m_multisampledCopyImageAlloc	= memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_multisampledCopyImage), MemoryRequirement::Any);
				VK_CHECK(vk.bindImageMemory(vkDevice, *m_multisampledCopyImage, m_multisampledCopyImageAlloc->getMemory(), m_multisampledCopyImageAlloc->getOffset()));
				break;
			}

			default :
				break;
		}
	}

	// Create destination image.
	{
		const VkImageCreateInfo	destinationImageParams	=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,								// const void*			pNext;
			0u,										// VkImageCreateFlags	flags;
			m_params.dst.image.imageType,			// VkImageType			imageType;
			m_params.dst.image.format,				// VkFormat				format;
			getExtent3D(m_params.dst.image),		// VkExtent3D			extent;
			1u,										// deUint32				mipLevels;
			getArraySize(m_params.dst.image),		// deUint32				arraySize;
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

	// Barriers for copying image to buffer
	VkImageMemoryBarrier		srcImageBarrier		=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		0u,											// VkAccessFlags			srcAccessMask;
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,		// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
		m_multisampledImage.get(),					// VkImage					image;
		{											// VkImageSubresourceRange	subresourceRange;
			VK_IMAGE_ASPECT_COLOR_BIT,			// VkImageAspectFlags	aspectMask;
			0u,									// deUint32				baseMipLevel;
			1u,									// deUint32				mipLevels;
			0u,									// deUint32				baseArraySlice;
			getArraySize(m_params.src.image)	// deUint32				arraySize;
		}
	};

		// Create render pass.
	{
		const VkAttachmentDescription	attachmentDescriptions[1]	=
		{
			{
				0u,											// VkAttachmentDescriptionFlags		flags;
				m_params.src.image.format,					// VkFormat							format;
				rasterizationSamples,						// VkSampleCountFlagBits			samples;
				VK_ATTACHMENT_LOAD_OP_CLEAR,				// VkAttachmentLoadOp				loadOp;
				VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp				storeOp;
				VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp				stencilLoadOp;
				VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp				stencilStoreOp;
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout					initialLayout;
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout					finalLayout;
			},
		};

		const VkAttachmentReference		colorAttachmentReference	=
		{
			0u,													// deUint32			attachment;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL			// VkImageLayout	layout;
		};

		const VkSubpassDescription		subpassDescription			=
		{
			0u,									// VkSubpassDescriptionFlags	flags;
			VK_PIPELINE_BIND_POINT_GRAPHICS,	// VkPipelineBindPoint			pipelineBindPoint;
			0u,									// deUint32						inputAttachmentCount;
			DE_NULL,							// const VkAttachmentReference*	pInputAttachments;
			1u,									// deUint32						colorAttachmentCount;
			&colorAttachmentReference,			// const VkAttachmentReference*	pColorAttachments;
			DE_NULL,							// const VkAttachmentReference*	pResolveAttachments;
			DE_NULL,							// const VkAttachmentReference*	pDepthStencilAttachment;
			0u,									// deUint32						preserveAttachmentCount;
			DE_NULL								// const VkAttachmentReference*	pPreserveAttachments;
		};

		const VkRenderPassCreateInfo	renderPassParams			=
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,	// VkStructureType					sType;
			DE_NULL,									// const void*						pNext;
			0u,											// VkRenderPassCreateFlags			flags;
			1u,											// deUint32							attachmentCount;
			attachmentDescriptions,						// const VkAttachmentDescription*	pAttachments;
			1u,											// deUint32							subpassCount;
			&subpassDescription,						// const VkSubpassDescription*		pSubpasses;
			0u,											// deUint32							dependencyCount;
			DE_NULL										// const VkSubpassDependency*		pDependencies;
		};

		renderPass	= createRenderPass(vk, vkDevice, &renderPassParams);
	}

	// Create pipeline layout
	{
		const VkPipelineLayoutCreateInfo	pipelineLayoutParams	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType					sType;
			DE_NULL,											// const void*						pNext;
			0u,													// VkPipelineLayoutCreateFlags		flags;
			0u,													// deUint32							setLayoutCount;
			DE_NULL,											// const VkDescriptorSetLayout*		pSetLayouts;
			0u,													// deUint32							pushConstantRangeCount;
			DE_NULL												// const VkPushConstantRange*		pPushConstantRanges;
		};

		pipelineLayout	= createPipelineLayout(vk, vkDevice, &pipelineLayoutParams);
	}

	{
		const tcu::Vec4	a	(-1.0, -1.0, 0.0, 1.0);
		const tcu::Vec4	b	(1.0, -1.0, 0.0, 1.0);
		const tcu::Vec4	c	(1.0, 1.0, 0.0, 1.0);
		// Add triangle.
		vertices.push_back(a);
		vertices.push_back(c);
		vertices.push_back(b);
	}

	// Create vertex buffer.
	{
		const VkDeviceSize			vertexDataSize		= vertices.size() * sizeof(tcu::Vec4);
		const VkBufferCreateInfo	vertexBufferParams	=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			0u,											// VkBufferCreateFlags	flags;
			vertexDataSize,								// VkDeviceSize			size;
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,			// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
			1u,											// deUint32				queueFamilyIndexCount;
			&queueFamilyIndex							// const deUint32*		pQueueFamilyIndices;
		};

		vertexBuffer		= createBuffer(vk, vkDevice, &vertexBufferParams);
		vertexBufferAlloc	= memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *vertexBuffer), MemoryRequirement::HostVisible);

		VK_CHECK(vk.bindBufferMemory(vkDevice, *vertexBuffer, vertexBufferAlloc->getMemory(), vertexBufferAlloc->getOffset()));

		// Load vertices into vertex buffer.
		deMemcpy(vertexBufferAlloc->getHostPtr(), vertices.data(), (size_t)vertexDataSize);
		flushMappedMemoryRange(vk, vkDevice, vertexBufferAlloc->getMemory(), vertexBufferAlloc->getOffset(), vertexDataSize);
	}

	{
		Move<VkFramebuffer>		framebuffer;
		Move<VkImageView>		sourceAttachmentView;
		const VkExtent3D		extent3D = getExtent3D(m_params.src.image);

		// Create color attachment view.
		{
			const VkImageViewCreateInfo	colorAttachmentViewParams	=
			{
				VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,				// VkStructureType			sType;
				DE_NULL,												// const void*				pNext;
				0u,														// VkImageViewCreateFlags	flags;
				*m_multisampledImage,									// VkImage					image;
				VK_IMAGE_VIEW_TYPE_2D,									// VkImageViewType			viewType;
				m_params.src.image.format,								// VkFormat					format;
				componentMappingRGBA,									// VkComponentMapping		components;
				{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u }	// VkImageSubresourceRange	subresourceRange;
			};
			sourceAttachmentView	= createImageView(vk, vkDevice, &colorAttachmentViewParams);
		}

		// Create framebuffer
		{
			const VkImageView				attachments[1]		=
			{
				*sourceAttachmentView,
			};

			const VkFramebufferCreateInfo	framebufferParams	=
			{
				VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// VkStructureType				sType;
				DE_NULL,									// const void*					pNext;
				0u,											// VkFramebufferCreateFlags		flags;
				*renderPass,								// VkRenderPass					renderPass;
				1u,											// deUint32						attachmentCount;
				attachments,								// const VkImageView*			pAttachments;
				extent3D.width,								// deUint32						width;
				extent3D.height,							// deUint32						height;
				1u											// deUint32						layers;
			};

			framebuffer	= createFramebuffer(vk, vkDevice, &framebufferParams);
		}

		// Create pipeline
		{
			const VkPipelineShaderStageCreateInfo			shaderStageParams[2]				=
			{
				{
					VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,		// VkStructureType						sType;
					DE_NULL,													// const void*							pNext;
					0u,															// VkPipelineShaderStageCreateFlags		flags;
					VK_SHADER_STAGE_VERTEX_BIT,									// VkShaderStageFlagBits				stage;
					*vertexShaderModule,										// VkShaderModule						module;
					"main",														// const char*							pName;
					DE_NULL														// const VkSpecializationInfo*			pSpecializationInfo;
				},
				{
					VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,		// VkStructureType						sType;
					DE_NULL,													// const void*							pNext;
					0u,															// VkPipelineShaderStageCreateFlags		flags;
					VK_SHADER_STAGE_FRAGMENT_BIT,								// VkShaderStageFlagBits				stage;
					*fragmentShaderModule,										// VkShaderModule						module;
					"main",														// const char*							pName;
					DE_NULL														// const VkSpecializationInfo*			pSpecializationInfo;
				}
			};

			const VkVertexInputBindingDescription			vertexInputBindingDescription		=
			{
				0u,									// deUint32				binding;
				sizeof(tcu::Vec4),					// deUint32				stride;
				VK_VERTEX_INPUT_RATE_VERTEX			// VkVertexInputRate	inputRate;
			};

			const VkVertexInputAttributeDescription			vertexInputAttributeDescriptions[1]	=
			{
				{
					0u,									// deUint32	location;
					0u,									// deUint32	binding;
					VK_FORMAT_R32G32B32A32_SFLOAT,		// VkFormat	format;
					0u									// deUint32	offset;
				}
			};

			const VkPipelineVertexInputStateCreateInfo		vertexInputStateParams				=
			{
				VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType							sType;
				DE_NULL,														// const void*								pNext;
				0u,																// VkPipelineVertexInputStateCreateFlags	flags;
				1u,																// deUint32									vertexBindingDescriptionCount;
				&vertexInputBindingDescription,									// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
				1u,																// deUint32									vertexAttributeDescriptionCount;
				vertexInputAttributeDescriptions								// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
			};

			const VkPipelineInputAssemblyStateCreateInfo	inputAssemblyStateParams			=
			{
				VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType							sType;
				DE_NULL,														// const void*								pNext;
				0u,																// VkPipelineInputAssemblyStateCreateFlags	flags;
				VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,							// VkPrimitiveTopology						topology;
				false															// VkBool32									primitiveRestartEnable;
			};

			const VkViewport	viewport	=
			{
				0.0f,							// float	x;
				0.0f,							// float	y;
				(float)extent3D.width,	// float	width;
				(float)extent3D.height,	// float	height;
				0.0f,							// float	minDepth;
				1.0f							// float	maxDepth;
			};

			const VkRect2D		scissor		=
			{
				{ 0, 0 },										// VkOffset2D	offset;
				{ extent3D.width, extent3D.height }	// VkExtent2D	extent;
			};

			const VkPipelineViewportStateCreateInfo			viewportStateParams		=
			{
				VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,			// VkStructureType						sType;
				DE_NULL,														// const void*							pNext;
				0u,																// VkPipelineViewportStateCreateFlags	flags;
				1u,																// deUint32								viewportCount;
				&viewport,														// const VkViewport*					pViewports;
				1u,																// deUint32								scissorCount;
				&scissor														// const VkRect2D*						pScissors;
			};

			const VkPipelineRasterizationStateCreateInfo	rasterStateParams		=
			{
				VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		// VkStructureType							sType;
				DE_NULL,														// const void*								pNext;
				0u,																// VkPipelineRasterizationStateCreateFlags	flags;
				false,															// VkBool32									depthClampEnable;
				false,															// VkBool32									rasterizerDiscardEnable;
				VK_POLYGON_MODE_FILL,											// VkPolygonMode							polygonMode;
				VK_CULL_MODE_NONE,												// VkCullModeFlags							cullMode;
				VK_FRONT_FACE_COUNTER_CLOCKWISE,								// VkFrontFace								frontFace;
				VK_FALSE,														// VkBool32									depthBiasEnable;
				0.0f,															// float									depthBiasConstantFactor;
				0.0f,															// float									depthBiasClamp;
				0.0f,															// float									depthBiasSlopeFactor;
				1.0f															// float									lineWidth;
			};

			const VkPipelineMultisampleStateCreateInfo	multisampleStateParams		=
			{
				VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,		// VkStructureType							sType;
				DE_NULL,														// const void*								pNext;
				0u,																// VkPipelineMultisampleStateCreateFlags	flags;
				rasterizationSamples,											// VkSampleCountFlagBits					rasterizationSamples;
				VK_FALSE,														// VkBool32									sampleShadingEnable;
				0.0f,															// float									minSampleShading;
				DE_NULL,														// const VkSampleMask*						pSampleMask;
				VK_FALSE,														// VkBool32									alphaToCoverageEnable;
				VK_FALSE														// VkBool32									alphaToOneEnable;
			};

			const VkPipelineColorBlendAttachmentState	colorBlendAttachmentState	=
			{
				false,														// VkBool32			blendEnable;
				VK_BLEND_FACTOR_ONE,										// VkBlend			srcBlendColor;
				VK_BLEND_FACTOR_ZERO,										// VkBlend			destBlendColor;
				VK_BLEND_OP_ADD,											// VkBlendOp		blendOpColor;
				VK_BLEND_FACTOR_ONE,										// VkBlend			srcBlendAlpha;
				VK_BLEND_FACTOR_ZERO,										// VkBlend			destBlendAlpha;
				VK_BLEND_OP_ADD,											// VkBlendOp		blendOpAlpha;
				(VK_COLOR_COMPONENT_R_BIT |
				 VK_COLOR_COMPONENT_G_BIT |
				 VK_COLOR_COMPONENT_B_BIT |
				 VK_COLOR_COMPONENT_A_BIT)									// VkChannelFlags	channelWriteMask;
			};

			const VkPipelineColorBlendStateCreateInfo	colorBlendStateParams	=
			{
				VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType								sType;
				DE_NULL,													// const void*									pNext;
				0u,															// VkPipelineColorBlendStateCreateFlags			flags;
				false,														// VkBool32										logicOpEnable;
				VK_LOGIC_OP_COPY,											// VkLogicOp									logicOp;
				1u,															// deUint32										attachmentCount;
				&colorBlendAttachmentState,									// const VkPipelineColorBlendAttachmentState*	pAttachments;
				{ 0.0f, 0.0f, 0.0f, 0.0f }									// float										blendConstants[4];
			};

			const VkGraphicsPipelineCreateInfo			graphicsPipelineParams	=
			{
				VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,	// VkStructureType									sType;
				DE_NULL,											// const void*										pNext;
				0u,													// VkPipelineCreateFlags							flags;
				2u,													// deUint32											stageCount;
				shaderStageParams,									// const VkPipelineShaderStageCreateInfo*			pStages;
				&vertexInputStateParams,							// const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
				&inputAssemblyStateParams,							// const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
				DE_NULL,											// const VkPipelineTessellationStateCreateInfo*		pTessellationState;
				&viewportStateParams,								// const VkPipelineViewportStateCreateInfo*			pViewportState;
				&rasterStateParams,									// const VkPipelineRasterizationStateCreateInfo*	pRasterizationState;
				&multisampleStateParams,							// const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
				DE_NULL,											// const VkPipelineDepthStencilStateCreateInfo*		pDepthStencilState;
				&colorBlendStateParams,								// const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
				DE_NULL,											// const VkPipelineDynamicStateCreateInfo*			pDynamicState;
				*pipelineLayout,									// VkPipelineLayout									layout;
				*renderPass,										// VkRenderPass										renderPass;
				0u,													// deUint32											subpass;
				0u,													// VkPipeline										basePipelineHandle;
				0u													// deInt32											basePipelineIndex;
			};

			graphicsPipeline	= createGraphicsPipeline(vk, vkDevice, DE_NULL, &graphicsPipelineParams);
		}

		// Create command buffer
		{
			const VkCommandBufferBeginInfo cmdBufferBeginInfo =
			{
				VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	// VkStructureType					sType;
				DE_NULL,										// const void*						pNext;
				0u,												// VkCommandBufferUsageFlags		flags;
				(const VkCommandBufferInheritanceInfo*)DE_NULL,
			};

			const VkClearValue clearValues[1] =
			{
				makeClearValueColorF32(0.0f, 0.0f, 1.0f, 1.0f),
			};

			const VkRenderPassBeginInfo renderPassBeginInfo =
			{
				VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,				// VkStructureType		sType;
				DE_NULL,												// const void*			pNext;
				*renderPass,											// VkRenderPass			renderPass;
				*framebuffer,											// VkFramebuffer		framebuffer;
				{
					{ 0, 0 },
					{ extent3D.width, extent3D.height }
				},														// VkRect2D				renderArea;
				1u,														// deUint32				clearValueCount;
				clearValues												// const VkClearValue*	pClearValues;
			};

			VK_CHECK(vk.beginCommandBuffer(*m_cmdBuffer, &cmdBufferBeginInfo));
			vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &srcImageBarrier);
			vk.cmdBeginRenderPass(*m_cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			const VkDeviceSize	vertexBufferOffset	= 0u;

			vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);
			vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer.get(), &vertexBufferOffset);
				vk.cmdDraw(*m_cmdBuffer, (deUint32)vertices.size(), 1, 0, 0);

			vk.cmdEndRenderPass(*m_cmdBuffer);
			VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));
		}

		// Queue submit.
		{
			const VkQueue	queue	= m_context.getUniversalQueue();
			submitCommandsAndWait (vk, vkDevice, queue, *m_cmdBuffer);
		}
	}
}

tcu::TestStatus ResolveImageToImage::iterate (void)
{
	const tcu::TextureFormat		srcTcuFormat		= mapVkFormat(m_params.src.image.format);
	const tcu::TextureFormat		dstTcuFormat		= mapVkFormat(m_params.dst.image.format);

	// upload the destination image
		m_destinationTextureLevel	= de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(dstTcuFormat,
																				(int)m_params.dst.image.extent.width,
																				(int)m_params.dst.image.extent.height,
																				(int)m_params.dst.image.extent.depth));
		generateBuffer(m_destinationTextureLevel->getAccess(), m_params.dst.image.extent.width, m_params.dst.image.extent.height, m_params.dst.image.extent.depth);
		uploadImage(m_destinationTextureLevel->getAccess(), m_destination.get(), m_params.dst.image);

		m_sourceTextureLevel = de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(srcTcuFormat,
																		(int)m_params.src.image.extent.width,
																		(int)m_params.src.image.extent.height,
																		(int)m_params.dst.image.extent.depth));

		generateBuffer(m_sourceTextureLevel->getAccess(), m_params.src.image.extent.width, m_params.src.image.extent.height, m_params.dst.image.extent.depth, FILL_MODE_MULTISAMPLE);
		generateExpectedResult();

	switch (m_options)
	{
		case COPY_MS_IMAGE_TO_MS_IMAGE:
		case COPY_MS_IMAGE_TO_ARRAY_MS_IMAGE:
			copyMSImageToMSImage();
			break;
		default:
			break;
	}

	const DeviceInterface&			vk					= m_context.getDeviceInterface();
	const VkDevice					vkDevice			= m_context.getDevice();
	const VkQueue					queue				= m_context.getUniversalQueue();

	std::vector<VkImageResolve>		imageResolves;
	for (deUint32 i = 0; i < m_params.regions.size(); i++)
		imageResolves.push_back(m_params.regions[i].imageResolve);

	const VkImageMemoryBarrier	imageBarriers[]		=
	{
		// source image
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,		// VkAccessFlags			srcAccessMask;
			VK_ACCESS_TRANSFER_READ_BIT,				// VkAccessFlags			dstAccessMask;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout			oldLayout;
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,		// VkImageLayout			newLayout;
			VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
			m_multisampledImage.get(),					// VkImage					image;
			{											// VkImageSubresourceRange	subresourceRange;
				getAspectFlags(srcTcuFormat),		// VkImageAspectFlags	aspectMask;
				0u,									// deUint32				baseMipLevel;
				1u,									// deUint32				mipLevels;
				0u,									// deUint32				baseArraySlice;
				getArraySize(m_params.dst.image)	// deUint32				arraySize;
			}
		},
		// destination image
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			0u,											// VkAccessFlags			srcAccessMask;
			VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			dstAccessMask;
			VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout			oldLayout;
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			newLayout;
			VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
			m_destination.get(),						// VkImage					image;
			{											// VkImageSubresourceRange	subresourceRange;
				getAspectFlags(dstTcuFormat),		// VkImageAspectFlags	aspectMask;
				0u,									// deUint32				baseMipLevel;
				1u,									// deUint32				mipLevels;
				0u,									// deUint32				baseArraySlice;
				getArraySize(m_params.dst.image)	// deUint32				arraySize;
			}
		},
	};

	const VkImageMemoryBarrier postImageBarrier =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,	// VkStructureType			sType;
		DE_NULL,								// const void*				pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,			// VkAccessFlags			srcAccessMask;
		VK_ACCESS_TRANSFER_WRITE_BIT,			// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,	// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,	// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,				// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,				// deUint32					dstQueueFamilyIndex;
		m_destination.get(),					// VkImage					image;
			{									// VkImageSubresourceRange	subresourceRange;
				getAspectFlags(dstTcuFormat),	// VkImageAspectFlags		aspectMask;
				0u,								// deUint32					baseMipLevel;
				1u,								// deUint32					mipLevels;
				0u,								// deUint32					baseArraySlice;
				getArraySize(m_params.dst.image)// deUint32					arraySize;
			}
	};

	const VkCommandBufferBeginInfo	cmdBufferBeginInfo	=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,			// VkStructureType					sType;
		DE_NULL,												// const void*						pNext;
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,			// VkCommandBufferUsageFlags		flags;
		(const VkCommandBufferInheritanceInfo*)DE_NULL,
	};

	VK_CHECK(vk.beginCommandBuffer(*m_cmdBuffer, &cmdBufferBeginInfo));
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, DE_LENGTH_OF_ARRAY(imageBarriers), imageBarriers);
	vk.cmdResolveImage(*m_cmdBuffer, m_multisampledImage.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_destination.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (deUint32)m_params.regions.size(), imageResolves.data());
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &postImageBarrier);
	VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));

	submitCommandsAndWait (vk, vkDevice, queue, *m_cmdBuffer);

	// check the result of resolving image
	{
		de::MovePtr<tcu::TextureLevel>	resultTextureLevel	= readImage(*m_destination, m_params.dst.image);

		if (QP_TEST_RESULT_PASS != checkTestResult(resultTextureLevel->getAccess()).getCode())
			return tcu::TestStatus::fail("CopiesAndBlitting test");
	}
	return tcu::TestStatus::pass("CopiesAndBlitting test");
}

tcu::TestStatus ResolveImageToImage::checkTestResult (tcu::ConstPixelBufferAccess result)
{
	const tcu::ConstPixelBufferAccess	expected		= m_expectedTextureLevel->getAccess();
	const float							fuzzyThreshold	= 0.01f;

	for (int arrayLayerNdx = 0; arrayLayerNdx < (int)getArraySize(m_params.dst.image); ++arrayLayerNdx)
	{
		const tcu::ConstPixelBufferAccess	expectedSub	= getSubregion (expected, 0, 0, arrayLayerNdx, expected.getWidth(), expected.getHeight(), 1u);
		const tcu::ConstPixelBufferAccess	resultSub	= getSubregion (result, 0, 0, arrayLayerNdx, result.getWidth(), result.getHeight(), 1u);
		if (!tcu::fuzzyCompare(m_context.getTestContext().getLog(), "Compare", "Result comparsion", expectedSub, resultSub, fuzzyThreshold, tcu::COMPARE_LOG_RESULT))
			return tcu::TestStatus::fail("CopiesAndBlitting test");
	}

	return tcu::TestStatus::pass("CopiesAndBlitting test");
}

void ResolveImageToImage::copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst, CopyRegion region)
{
	VkOffset3D srcOffset	= region.imageResolve.srcOffset;
			srcOffset.z		= region.imageResolve.srcSubresource.baseArrayLayer;
	VkOffset3D dstOffset	= region.imageResolve.dstOffset;
			dstOffset.z		= region.imageResolve.dstSubresource.baseArrayLayer;
	VkExtent3D extent		= region.imageResolve.extent;

	const tcu::ConstPixelBufferAccess	srcSubRegion		= getSubregion (src, srcOffset.x, srcOffset.y, srcOffset.z, extent.width, extent.height, extent.depth);
	// CopyImage acts like a memcpy. Replace the destination format with the srcformat to use a memcpy.
	const tcu::PixelBufferAccess		dstWithSrcFormat	(srcSubRegion.getFormat(), dst.getSize(), dst.getDataPtr());
	const tcu::PixelBufferAccess		dstSubRegion		= getSubregion (dstWithSrcFormat, dstOffset.x, dstOffset.y, dstOffset.z, extent.width, extent.height, extent.depth);

	tcu::copy(dstSubRegion, srcSubRegion);
}

void ResolveImageToImage::copyMSImageToMSImage (void)
{
	const DeviceInterface&			vk					= m_context.getDeviceInterface();
	const VkDevice					vkDevice			= m_context.getDevice();
	const VkQueue					queue				= m_context.getUniversalQueue();
	const tcu::TextureFormat		srcTcuFormat		= mapVkFormat(m_params.src.image.format);
	std::vector<VkImageCopy>		imageCopies;

	for (deUint32 layerNdx = 0; layerNdx < getArraySize(m_params.dst.image); ++layerNdx)
	{
		const VkImageSubresourceLayers	sourceSubresourceLayers	=
		{
			getAspectFlags(srcTcuFormat),	// VkImageAspectFlags	aspectMask;
			0u,								// uint32_t				mipLevel;
			0u,								// uint32_t				baseArrayLayer;
			1u								// uint32_t				layerCount;
		};

		const VkImageSubresourceLayers	destinationSubresourceLayers	=
		{
			getAspectFlags(srcTcuFormat),	// VkImageAspectFlags	aspectMask;//getAspectFlags(dstTcuFormat)
			0u,								// uint32_t				mipLevel;
			layerNdx,						// uint32_t				baseArrayLayer;
			1u								// uint32_t				layerCount;
		};

		const VkImageCopy				imageCopy	=
		{
			sourceSubresourceLayers,			// VkImageSubresourceLayers	srcSubresource;
			{0, 0, 0},							// VkOffset3D				srcOffset;
			destinationSubresourceLayers,		// VkImageSubresourceLayers	dstSubresource;
			{0, 0, 0},							// VkOffset3D				dstOffset;
			 getExtent3D(m_params.src.image),	// VkExtent3D				extent;
		};
		imageCopies.push_back(imageCopy);
	}

	const VkImageMemoryBarrier		imageBarriers[]		=
	{
		//// source image
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,		// VkAccessFlags			srcAccessMask;
			VK_ACCESS_TRANSFER_READ_BIT,				// VkAccessFlags			dstAccessMask;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout			oldLayout;
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,		// VkImageLayout			newLayout;
			VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
			m_multisampledImage.get(),					// VkImage					image;
			{											// VkImageSubresourceRange	subresourceRange;
				getAspectFlags(srcTcuFormat),		// VkImageAspectFlags	aspectMask;
				0u,									// deUint32				baseMipLevel;
				1u,									// deUint32				mipLevels;
				0u,									// deUint32				baseArraySlice;
				getArraySize(m_params.src.image)	// deUint32				arraySize;
			}
		},
		// destination image
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			0,											// VkAccessFlags			srcAccessMask;
			VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			dstAccessMask;
			VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout			oldLayout;
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			newLayout;
			VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
			m_multisampledCopyImage.get(),				// VkImage					image;
			{											// VkImageSubresourceRange	subresourceRange;
				getAspectFlags(srcTcuFormat),		// VkImageAspectFlags	aspectMask;
				0u,									// deUint32				baseMipLevel;
				1u,									// deUint32				mipLevels;
				0u,									// deUint32				baseArraySlice;
				getArraySize(m_params.dst.image)	// deUint32				arraySize;
			}
		},
	};

	const VkImageMemoryBarrier	postImageBarriers		=
	// source image
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			srcAccessMask;
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,		// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
		m_multisampledCopyImage.get(),				// VkImage					image;
		{											// VkImageSubresourceRange	subresourceRange;
			getAspectFlags(srcTcuFormat),		// VkImageAspectFlags	aspectMask;
			0u,									// deUint32				baseMipLevel;
			1u,									// deUint32				mipLevels;
			0u,									// deUint32				baseArraySlice;
			getArraySize(m_params.dst.image)	// deUint32				arraySize;
		}
	};

	const VkCommandBufferBeginInfo	cmdBufferBeginInfo	=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,			// VkStructureType					sType;
		DE_NULL,												// const void*						pNext;
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,			// VkCommandBufferUsageFlags		flags;
		(const VkCommandBufferInheritanceInfo*)DE_NULL,
	};

	VK_CHECK(vk.beginCommandBuffer(*m_cmdBuffer, &cmdBufferBeginInfo));
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, DE_LENGTH_OF_ARRAY(imageBarriers), imageBarriers);
	vk.cmdCopyImage(*m_cmdBuffer, m_multisampledImage.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_multisampledCopyImage.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (deUint32)imageCopies.size(), imageCopies.data());
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1u, &postImageBarriers);
	VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));

	submitCommandsAndWait (vk, vkDevice, queue, *m_cmdBuffer);

	m_multisampledImage = m_multisampledCopyImage;
}

class ResolveImageToImageTestCase : public vkt::TestCase
{
public:
							ResolveImageToImageTestCase	(tcu::TestContext&					testCtx,
														 const std::string&					name,
														 const std::string&					description,
														 const TestParams					params,
														 const ResolveImageToImageOptions	options = NO_OPTIONAL_OPERATION)
								: vkt::TestCase	(testCtx, name, description)
								, m_params		(params)
								, m_options		(options)
							{}
	virtual	void			initPrograms				(SourceCollections&		programCollection) const;

	virtual TestInstance*	createInstance				(Context&				context) const
							{
								return new ResolveImageToImage(context, m_params, m_options);
							}
private:
	TestParams							m_params;
	const ResolveImageToImageOptions	m_options;
};

void ResolveImageToImageTestCase::initPrograms (SourceCollections& programCollection) const
{
	programCollection.glslSources.add("vert") << glu::VertexSource(
		"#version 310 es\n"
		"layout (location = 0) in highp vec4 a_position;\n"
		"void main()\n"
		"{\n"
		"	gl_Position = a_position;\n"
		"}\n");

	programCollection.glslSources.add("frag") << glu::FragmentSource(
		"#version 310 es\n"
		"layout (location = 0) out highp vec4 o_color;\n"
		"void main()\n"
		"{\n"
		"	o_color = vec4(0.0, 1.0, 0.0, 1.0);\n"
		"}\n");
}

std::string getSampleCountCaseName (VkSampleCountFlagBits sampleFlag)
{
	return de::toLower(de::toString(getSampleCountFlagsStr(sampleFlag)).substr(16));
}

std::string getFormatCaseName (VkFormat format)
{
	return de::toLower(de::toString(getFormatStr(format)).substr(10));
}

void addCopyImageTestsAllFormats (tcu::TestCaseGroup*	testCaseGroup,
								  tcu::TestContext&		testCtx,
								  TestParams&			params)
{
	const VkFormat	compatibleFormats8Bit[]			=
	{
		VK_FORMAT_R4G4_UNORM_PACK8,
		VK_FORMAT_R8_UNORM,
		VK_FORMAT_R8_SNORM,
		VK_FORMAT_R8_USCALED,
		VK_FORMAT_R8_SSCALED,
		VK_FORMAT_R8_UINT,
		VK_FORMAT_R8_SINT,
		VK_FORMAT_R8_SRGB,

		VK_FORMAT_UNDEFINED
	};
	const VkFormat	compatibleFormats16Bit[]		=
	{
		VK_FORMAT_R4G4B4A4_UNORM_PACK16,
		VK_FORMAT_B4G4R4A4_UNORM_PACK16,
		VK_FORMAT_R5G6B5_UNORM_PACK16,
		VK_FORMAT_B5G6R5_UNORM_PACK16,
		VK_FORMAT_R5G5B5A1_UNORM_PACK16,
		VK_FORMAT_B5G5R5A1_UNORM_PACK16,
		VK_FORMAT_A1R5G5B5_UNORM_PACK16,
		VK_FORMAT_R8G8_UNORM,
		VK_FORMAT_R8G8_SNORM,
		VK_FORMAT_R8G8_USCALED,
		VK_FORMAT_R8G8_SSCALED,
		VK_FORMAT_R8G8_UINT,
		VK_FORMAT_R8G8_SINT,
		VK_FORMAT_R8G8_SRGB,
		VK_FORMAT_R16_UNORM,
		VK_FORMAT_R16_SNORM,
		VK_FORMAT_R16_USCALED,
		VK_FORMAT_R16_SSCALED,
		VK_FORMAT_R16_UINT,
		VK_FORMAT_R16_SINT,
		VK_FORMAT_R16_SFLOAT,

		VK_FORMAT_UNDEFINED
	 };
	const VkFormat	compatibleFormats24Bit[]		=
	{
		VK_FORMAT_R8G8B8_UNORM,
		VK_FORMAT_R8G8B8_SNORM,
		VK_FORMAT_R8G8B8_USCALED,
		VK_FORMAT_R8G8B8_SSCALED,
		VK_FORMAT_R8G8B8_UINT,
		VK_FORMAT_R8G8B8_SINT,
		VK_FORMAT_R8G8B8_SRGB,
		VK_FORMAT_B8G8R8_UNORM,
		VK_FORMAT_B8G8R8_SNORM,
		VK_FORMAT_B8G8R8_USCALED,
		VK_FORMAT_B8G8R8_SSCALED,
		VK_FORMAT_B8G8R8_UINT,
		VK_FORMAT_B8G8R8_SINT,
		VK_FORMAT_B8G8R8_SRGB,

		VK_FORMAT_UNDEFINED
	 };
	const VkFormat	compatibleFormats32Bit[]		=
	{
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_R8G8B8A8_SNORM,
		VK_FORMAT_R8G8B8A8_USCALED,
		VK_FORMAT_R8G8B8A8_SSCALED,
		VK_FORMAT_R8G8B8A8_UINT,
		VK_FORMAT_R8G8B8A8_SINT,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_FORMAT_B8G8R8A8_SNORM,
		VK_FORMAT_B8G8R8A8_USCALED,
		VK_FORMAT_B8G8R8A8_SSCALED,
		VK_FORMAT_B8G8R8A8_UINT,
		VK_FORMAT_B8G8R8A8_SINT,
		VK_FORMAT_B8G8R8A8_SRGB,
		VK_FORMAT_A8B8G8R8_UNORM_PACK32,
		VK_FORMAT_A8B8G8R8_SNORM_PACK32,
		VK_FORMAT_A8B8G8R8_USCALED_PACK32,
		VK_FORMAT_A8B8G8R8_SSCALED_PACK32,
		VK_FORMAT_A8B8G8R8_UINT_PACK32,
		VK_FORMAT_A8B8G8R8_SINT_PACK32,
		VK_FORMAT_A8B8G8R8_SRGB_PACK32,
		VK_FORMAT_A2R10G10B10_UNORM_PACK32,
		VK_FORMAT_A2R10G10B10_SNORM_PACK32,
		VK_FORMAT_A2R10G10B10_USCALED_PACK32,
		VK_FORMAT_A2R10G10B10_SSCALED_PACK32,
		VK_FORMAT_A2R10G10B10_UINT_PACK32,
		VK_FORMAT_A2R10G10B10_SINT_PACK32,
		VK_FORMAT_A2B10G10R10_UNORM_PACK32,
		VK_FORMAT_A2B10G10R10_SNORM_PACK32,
		VK_FORMAT_A2B10G10R10_USCALED_PACK32,
		VK_FORMAT_A2B10G10R10_SSCALED_PACK32,
		VK_FORMAT_A2B10G10R10_UINT_PACK32,
		VK_FORMAT_A2B10G10R10_SINT_PACK32,
		VK_FORMAT_R16G16_UNORM,
		VK_FORMAT_R16G16_SNORM,
		VK_FORMAT_R16G16_USCALED,
		VK_FORMAT_R16G16_SSCALED,
		VK_FORMAT_R16G16_UINT,
		VK_FORMAT_R16G16_SINT,
		VK_FORMAT_R16G16_SFLOAT,
		VK_FORMAT_R32_UINT,
		VK_FORMAT_R32_SINT,
		VK_FORMAT_R32_SFLOAT,

		VK_FORMAT_UNDEFINED
	 };
	const VkFormat	compatibleFormats48Bit[]		=
	{
		VK_FORMAT_R16G16B16_UNORM,
		VK_FORMAT_R16G16B16_SNORM,
		VK_FORMAT_R16G16B16_USCALED,
		VK_FORMAT_R16G16B16_SSCALED,
		VK_FORMAT_R16G16B16_UINT,
		VK_FORMAT_R16G16B16_SINT,
		VK_FORMAT_R16G16B16_SFLOAT,

		VK_FORMAT_UNDEFINED
	 };
	const VkFormat	compatibleFormats64Bit[]		=
	{
		VK_FORMAT_R16G16B16A16_UNORM,
		VK_FORMAT_R16G16B16A16_SNORM,
		VK_FORMAT_R16G16B16A16_USCALED,
		VK_FORMAT_R16G16B16A16_SSCALED,
		VK_FORMAT_R16G16B16A16_UINT,
		VK_FORMAT_R16G16B16A16_SINT,
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_FORMAT_R32G32_UINT,
		VK_FORMAT_R32G32_SINT,
		VK_FORMAT_R32G32_SFLOAT,
		VK_FORMAT_R64_UINT,
		VK_FORMAT_R64_SINT,
		VK_FORMAT_R64_SFLOAT,

		VK_FORMAT_UNDEFINED
	 };
	const VkFormat	compatibleFormats96Bit[]		=
	{
		VK_FORMAT_R32G32B32_UINT,
		VK_FORMAT_R32G32B32_SINT,
		VK_FORMAT_R32G32B32_SFLOAT,

		VK_FORMAT_UNDEFINED
	 };
	const VkFormat	compatibleFormats128Bit[]		=
	{
		VK_FORMAT_R32G32B32A32_UINT,
		VK_FORMAT_R32G32B32A32_SINT,
		VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_FORMAT_R64G64_UINT,
		VK_FORMAT_R64G64_SINT,
		VK_FORMAT_R64G64_SFLOAT,

		VK_FORMAT_UNDEFINED
	 };
	const VkFormat	compatibleFormats192Bit[]		=
	{
		VK_FORMAT_R64G64B64_UINT,
		VK_FORMAT_R64G64B64_SINT,
		VK_FORMAT_R64G64B64_SFLOAT,

		VK_FORMAT_UNDEFINED
	 };
	const VkFormat	compatibleFormats256Bit[]		=
	{
		VK_FORMAT_R64G64B64A64_UINT,
		VK_FORMAT_R64G64B64A64_SINT,
		VK_FORMAT_R64G64B64A64_SFLOAT,

		VK_FORMAT_UNDEFINED
	};

	const VkFormat*	colorImageFormatsToTest[]		=
	{
		compatibleFormats8Bit,
		compatibleFormats16Bit,
		compatibleFormats24Bit,
		compatibleFormats32Bit,
		compatibleFormats48Bit,
		compatibleFormats64Bit,
		compatibleFormats96Bit,
		compatibleFormats128Bit,
		compatibleFormats192Bit,
		compatibleFormats256Bit,
	};
	const size_t	numOfColorImageFormatsToTest	= DE_LENGTH_OF_ARRAY(colorImageFormatsToTest);

	for (size_t compatibleFormatsIndex = 0; compatibleFormatsIndex < numOfColorImageFormatsToTest; ++compatibleFormatsIndex)
	{
		const VkFormat*	compatibleFormats	= colorImageFormatsToTest[compatibleFormatsIndex];
		for (size_t srcFormatIndex = 0; compatibleFormats[srcFormatIndex] != VK_FORMAT_UNDEFINED; ++srcFormatIndex)
		{
			params.src.image.format	= compatibleFormats[srcFormatIndex];
			for (size_t dstFormatIndex = 0; compatibleFormats[dstFormatIndex] != VK_FORMAT_UNDEFINED; ++dstFormatIndex)
			{
				params.dst.image.format	= compatibleFormats[dstFormatIndex];

				if (!isSupportedByFramework(params.src.image.format) || !isSupportedByFramework(params.dst.image.format))
					continue;

				std::ostringstream	testName;
				testName << getFormatCaseName(params.src.image.format) << "_" << getFormatCaseName(params.dst.image.format);
				std::ostringstream	description;
				description << "Copy from src " << params.src.image.format << " to dst " << params.dst.image.format;

				testCaseGroup->addChild(new CopyImageToImageTestCase(testCtx, testName.str(), description.str(), params));
			}
		}
	}
}

void addBlittingTestsAllFormats (tcu::TestCaseGroup*	testCaseGroup,
								 tcu::TestContext&		testCtx,
								 TestParams&			params)
{
	// Test Image formats.
	const VkFormat	compatibleFormatsUInts[]			=
	{
		VK_FORMAT_R8_UINT,
		VK_FORMAT_R8G8_UINT,
		VK_FORMAT_R8G8B8_UINT,
		VK_FORMAT_B8G8R8_UINT,
		VK_FORMAT_R8G8B8A8_UINT,
		VK_FORMAT_B8G8R8A8_UINT,
		VK_FORMAT_A8B8G8R8_UINT_PACK32,
		VK_FORMAT_A2R10G10B10_UINT_PACK32,
		VK_FORMAT_A2B10G10R10_UINT_PACK32,
		VK_FORMAT_R16_UINT,
		VK_FORMAT_R16G16_UINT,
		VK_FORMAT_R16G16B16_UINT,
		VK_FORMAT_R16G16B16A16_UINT,
		VK_FORMAT_R32_UINT,
		VK_FORMAT_R32G32_UINT,
		VK_FORMAT_R32G32B32_UINT,
		VK_FORMAT_R32G32B32A32_UINT,
		VK_FORMAT_R64_UINT,
		VK_FORMAT_R64G64_UINT,
		VK_FORMAT_R64G64B64_UINT,
		VK_FORMAT_R64G64B64A64_UINT,

		VK_FORMAT_UNDEFINED
	};
	const VkFormat	compatibleFormatsSInts[]			=
	{
		VK_FORMAT_R8_SINT,
		VK_FORMAT_R8G8_SINT,
		VK_FORMAT_R8G8B8_SINT,
		VK_FORMAT_B8G8R8_SINT,
		VK_FORMAT_R8G8B8A8_SINT,
		VK_FORMAT_B8G8R8A8_SINT,
		VK_FORMAT_A8B8G8R8_SINT_PACK32,
		VK_FORMAT_A2R10G10B10_SINT_PACK32,
		VK_FORMAT_A2B10G10R10_SINT_PACK32,
		VK_FORMAT_R16_SINT,
		VK_FORMAT_R16G16_SINT,
		VK_FORMAT_R16G16B16_SINT,
		VK_FORMAT_R16G16B16A16_SINT,
		VK_FORMAT_R32_SINT,
		VK_FORMAT_R32G32_SINT,
		VK_FORMAT_R32G32B32_SINT,
		VK_FORMAT_R32G32B32A32_SINT,
		VK_FORMAT_R64_SINT,
		VK_FORMAT_R64G64_SINT,
		VK_FORMAT_R64G64B64_SINT,
		VK_FORMAT_R64G64B64A64_SINT,

		VK_FORMAT_UNDEFINED
	};
	const VkFormat	compatibleFormatsFloats[]			=
	{
		VK_FORMAT_R4G4_UNORM_PACK8,
		VK_FORMAT_R4G4B4A4_UNORM_PACK16,
		VK_FORMAT_B4G4R4A4_UNORM_PACK16,
		VK_FORMAT_R5G6B5_UNORM_PACK16,
		VK_FORMAT_B5G6R5_UNORM_PACK16,
		VK_FORMAT_R5G5B5A1_UNORM_PACK16,
		VK_FORMAT_B5G5R5A1_UNORM_PACK16,
		VK_FORMAT_A1R5G5B5_UNORM_PACK16,
		VK_FORMAT_R8_UNORM,
		VK_FORMAT_R8_SNORM,
		VK_FORMAT_R8_USCALED,
		VK_FORMAT_R8_SSCALED,
		VK_FORMAT_R8G8_UNORM,
		VK_FORMAT_R8G8_SNORM,
		VK_FORMAT_R8G8_USCALED,
		VK_FORMAT_R8G8_SSCALED,
		VK_FORMAT_R8G8B8_UNORM,
		VK_FORMAT_R8G8B8_SNORM,
		VK_FORMAT_R8G8B8_USCALED,
		VK_FORMAT_R8G8B8_SSCALED,
		VK_FORMAT_B8G8R8_UNORM,
		VK_FORMAT_B8G8R8_SNORM,
		VK_FORMAT_B8G8R8_USCALED,
		VK_FORMAT_B8G8R8_SSCALED,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_R8G8B8A8_SNORM,
		VK_FORMAT_R8G8B8A8_USCALED,
		VK_FORMAT_R8G8B8A8_SSCALED,
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_FORMAT_B8G8R8A8_SNORM,
		VK_FORMAT_B8G8R8A8_USCALED,
		VK_FORMAT_B8G8R8A8_SSCALED,
		VK_FORMAT_A8B8G8R8_UNORM_PACK32,
		VK_FORMAT_A8B8G8R8_SNORM_PACK32,
		VK_FORMAT_A8B8G8R8_USCALED_PACK32,
		VK_FORMAT_A8B8G8R8_SSCALED_PACK32,
		VK_FORMAT_A2R10G10B10_UNORM_PACK32,
		VK_FORMAT_A2R10G10B10_SNORM_PACK32,
		VK_FORMAT_A2R10G10B10_USCALED_PACK32,
		VK_FORMAT_A2R10G10B10_SSCALED_PACK32,
		VK_FORMAT_A2B10G10R10_UNORM_PACK32,
		VK_FORMAT_A2B10G10R10_SNORM_PACK32,
		VK_FORMAT_A2B10G10R10_USCALED_PACK32,
		VK_FORMAT_A2B10G10R10_SSCALED_PACK32,
		VK_FORMAT_R16_UNORM,
		VK_FORMAT_R16_SNORM,
		VK_FORMAT_R16_USCALED,
		VK_FORMAT_R16_SSCALED,
		VK_FORMAT_R16_SFLOAT,
		VK_FORMAT_R16G16_UNORM,
		VK_FORMAT_R16G16_SNORM,
		VK_FORMAT_R16G16_USCALED,
		VK_FORMAT_R16G16_SSCALED,
		VK_FORMAT_R16G16_SFLOAT,
		VK_FORMAT_R16G16B16_UNORM,
		VK_FORMAT_R16G16B16_SNORM,
		VK_FORMAT_R16G16B16_USCALED,
		VK_FORMAT_R16G16B16_SSCALED,
		VK_FORMAT_R16G16B16_SFLOAT,
		VK_FORMAT_R16G16B16A16_UNORM,
		VK_FORMAT_R16G16B16A16_SNORM,
		VK_FORMAT_R16G16B16A16_USCALED,
		VK_FORMAT_R16G16B16A16_SSCALED,
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_FORMAT_R32_SFLOAT,
		VK_FORMAT_R32G32_SFLOAT,
		VK_FORMAT_R32G32B32_SFLOAT,
		VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_FORMAT_R64_SFLOAT,
		VK_FORMAT_R64G64_SFLOAT,
		VK_FORMAT_R64G64B64_SFLOAT,
		VK_FORMAT_R64G64B64A64_SFLOAT,
//		VK_FORMAT_B10G11R11_UFLOAT_PACK32,
//		VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,
//		VK_FORMAT_BC1_RGB_UNORM_BLOCK,
//		VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
//		VK_FORMAT_BC2_UNORM_BLOCK,
//		VK_FORMAT_BC3_UNORM_BLOCK,
//		VK_FORMAT_BC4_UNORM_BLOCK,
//		VK_FORMAT_BC4_SNORM_BLOCK,
//		VK_FORMAT_BC5_UNORM_BLOCK,
//		VK_FORMAT_BC5_SNORM_BLOCK,
//		VK_FORMAT_BC6H_UFLOAT_BLOCK,
//		VK_FORMAT_BC6H_SFLOAT_BLOCK,
//		VK_FORMAT_BC7_UNORM_BLOCK,
//		VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK,
//		VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK,
//		VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK,
//		VK_FORMAT_EAC_R11_UNORM_BLOCK,
//		VK_FORMAT_EAC_R11_SNORM_BLOCK,
//		VK_FORMAT_EAC_R11G11_UNORM_BLOCK,
//		VK_FORMAT_EAC_R11G11_SNORM_BLOCK,
//		VK_FORMAT_ASTC_4x4_UNORM_BLOCK,
//		VK_FORMAT_ASTC_5x4_UNORM_BLOCK,
//		VK_FORMAT_ASTC_5x5_UNORM_BLOCK,
//		VK_FORMAT_ASTC_6x5_UNORM_BLOCK,
//		VK_FORMAT_ASTC_6x6_UNORM_BLOCK,
//		VK_FORMAT_ASTC_8x5_UNORM_BLOCK,
//		VK_FORMAT_ASTC_8x6_UNORM_BLOCK,
//		VK_FORMAT_ASTC_8x8_UNORM_BLOCK,
//		VK_FORMAT_ASTC_10x5_UNORM_BLOCK,
//		VK_FORMAT_ASTC_10x6_UNORM_BLOCK,
//		VK_FORMAT_ASTC_10x8_UNORM_BLOCK,
//		VK_FORMAT_ASTC_10x10_UNORM_BLOCK,
//		VK_FORMAT_ASTC_12x10_UNORM_BLOCK,
//		VK_FORMAT_ASTC_12x12_UNORM_BLOCK,

		VK_FORMAT_UNDEFINED
	};
	const VkFormat	compatibleFormatsSrgb[]				=
	{
		VK_FORMAT_R8_SRGB,
		VK_FORMAT_R8G8_SRGB,
		VK_FORMAT_R8G8B8_SRGB,
		VK_FORMAT_B8G8R8_SRGB,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_FORMAT_B8G8R8A8_SRGB,
		VK_FORMAT_A8B8G8R8_SRGB_PACK32,
//		VK_FORMAT_BC1_RGB_SRGB_BLOCK,
//		VK_FORMAT_BC1_RGBA_SRGB_BLOCK,
//		VK_FORMAT_BC2_SRGB_BLOCK,
//		VK_FORMAT_BC3_SRGB_BLOCK,
//		VK_FORMAT_BC7_SRGB_BLOCK,
//		VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK,
//		VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK,
//		VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK,
//		VK_FORMAT_ASTC_4x4_SRGB_BLOCK,
//		VK_FORMAT_ASTC_5x4_SRGB_BLOCK,
//		VK_FORMAT_ASTC_5x5_SRGB_BLOCK,
//		VK_FORMAT_ASTC_6x5_SRGB_BLOCK,
//		VK_FORMAT_ASTC_6x6_SRGB_BLOCK,
//		VK_FORMAT_ASTC_8x5_SRGB_BLOCK,
//		VK_FORMAT_ASTC_8x6_SRGB_BLOCK,
//		VK_FORMAT_ASTC_8x8_SRGB_BLOCK,
//		VK_FORMAT_ASTC_10x5_SRGB_BLOCK,
//		VK_FORMAT_ASTC_10x6_SRGB_BLOCK,
//		VK_FORMAT_ASTC_10x8_SRGB_BLOCK,
//		VK_FORMAT_ASTC_10x10_SRGB_BLOCK,
//		VK_FORMAT_ASTC_12x10_SRGB_BLOCK,
//		VK_FORMAT_ASTC_12x12_SRGB_BLOCK,

		VK_FORMAT_UNDEFINED
	};

	const struct {
		const VkFormat*	compatibleFormats;
		const bool		onlyNearest;
	}	colorImageFormatsToTest[]			=
	{
		{ compatibleFormatsUInts,	true	},
		{ compatibleFormatsSInts,	true	},
		{ compatibleFormatsFloats,	false	},
		{ compatibleFormatsSrgb,	false	},
	};
	const size_t	numOfColorImageFormatsToTest		= DE_LENGTH_OF_ARRAY(colorImageFormatsToTest);

	for (size_t compatibleFormatsIndex = 0; compatibleFormatsIndex < numOfColorImageFormatsToTest; ++compatibleFormatsIndex)
	{
		const VkFormat*	compatibleFormats	= colorImageFormatsToTest[compatibleFormatsIndex].compatibleFormats;
		const bool		onlyNearest			= colorImageFormatsToTest[compatibleFormatsIndex].onlyNearest;
		for (size_t srcFormatIndex = 0; compatibleFormats[srcFormatIndex] != VK_FORMAT_UNDEFINED; ++srcFormatIndex)
		{
			params.src.image.format	= compatibleFormats[srcFormatIndex];
			for (size_t dstFormatIndex = 0; compatibleFormats[dstFormatIndex] != VK_FORMAT_UNDEFINED; ++dstFormatIndex)
			{
				params.dst.image.format	= compatibleFormats[dstFormatIndex];

				if (!isSupportedByFramework(params.src.image.format) || !isSupportedByFramework(params.dst.image.format))
					continue;

				std::ostringstream	testName;
				testName << getFormatCaseName(params.src.image.format) << "_" << getFormatCaseName(params.dst.image.format);
				std::ostringstream	description;
				description << "Blit image from src " << params.src.image.format << " to dst " << params.dst.image.format;

				params.filter			= VK_FILTER_NEAREST;
				testCaseGroup->addChild(new BlittingTestCase(testCtx, testName.str() + "_nearest", description.str(), params));

				if (!onlyNearest)
				{
					params.filter		= VK_FILTER_LINEAR;
					testCaseGroup->addChild(new BlittingTestCase(testCtx, testName.str() + "_linear", description.str(), params));
				}
			}
		}
	}
}

} // anonymous

tcu::TestCaseGroup* createCopiesAndBlittingTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	copiesAndBlittingTests	(new tcu::TestCaseGroup(testCtx, "copy_and_blit", "Copies And Blitting Tests"));

	de::MovePtr<tcu::TestCaseGroup>	imageToImageTests		(new tcu::TestCaseGroup(testCtx, "image_to_image", "Copy from image to image"));
	de::MovePtr<tcu::TestCaseGroup>	imgToImgSimpleTests		(new tcu::TestCaseGroup(testCtx, "simple_tests", "Copy from image to image simple tests"));
	de::MovePtr<tcu::TestCaseGroup>	imgToImgAllFormatsTests	(new tcu::TestCaseGroup(testCtx, "all_formats", "Copy from image to image with all compatible formats"));

	de::MovePtr<tcu::TestCaseGroup>	imageToBufferTests		(new tcu::TestCaseGroup(testCtx, "image_to_buffer", "Copy from image to buffer"));
	de::MovePtr<tcu::TestCaseGroup>	bufferToImageTests		(new tcu::TestCaseGroup(testCtx, "buffer_to_image", "Copy from buffer to image"));
	de::MovePtr<tcu::TestCaseGroup>	bufferToBufferTests		(new tcu::TestCaseGroup(testCtx, "buffer_to_buffer", "Copy from buffer to buffer"));

	de::MovePtr<tcu::TestCaseGroup>	blittingImageTests		(new tcu::TestCaseGroup(testCtx, "blit_image", "Blitting image"));
	de::MovePtr<tcu::TestCaseGroup>	blitImgSimpleTests		(new tcu::TestCaseGroup(testCtx, "simple_tests", "Blitting image simple tests"));
	de::MovePtr<tcu::TestCaseGroup>	blitImgAllFormatsTests	(new tcu::TestCaseGroup(testCtx, "all_formats", "Blitting image with all compatible formats"));

	de::MovePtr<tcu::TestCaseGroup>	resolveImageTests		(new tcu::TestCaseGroup(testCtx, "resolve_image", "Resolve image"));

	const deInt32					defaultSize				= 64;
	const deInt32					defaultHalfSize			= defaultSize / 2;
	const deInt32					defaultFourthSize		= defaultSize / 4;
	const VkExtent3D				defaultExtent			= {defaultSize, defaultSize, 1};
	const VkExtent3D				defaultHalfExtent		= {defaultHalfSize, defaultHalfSize, 1};

	const VkImageSubresourceLayers	defaultSourceLayer		=
	{
		VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
		0u,							// uint32_t				mipLevel;
		0u,							// uint32_t				baseArrayLayer;
		1u,							// uint32_t				layerCount;
	};

	const VkFormat	depthAndStencilFormats[]	=
	{
		VK_FORMAT_D16_UNORM,
		VK_FORMAT_X8_D24_UNORM_PACK32,
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_S8_UINT,
		VK_FORMAT_D16_UNORM_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D32_SFLOAT_S8_UINT,
	};

	// Copy image to image testcases.
	{
		TestParams			params;
		params.src.image.imageType	= VK_IMAGE_TYPE_2D;
		params.src.image.format		= VK_FORMAT_R8G8B8A8_UINT;
		params.src.image.extent		= defaultExtent;
		params.dst.image.format		= VK_FORMAT_R8G8B8A8_UINT;
		params.dst.image.extent		= defaultExtent;

		{
			const VkImageCopy				testCopy	=
			{
				defaultSourceLayer,	// VkImageSubresourceLayers	srcSubresource;
				{0, 0, 0},			// VkOffset3D				srcOffset;
				defaultSourceLayer,	// VkImageSubresourceLayers	dstSubresource;
				{0, 0, 0},			// VkOffset3D				dstOffset;
				defaultExtent,		// VkExtent3D				extent;
			};

			CopyRegion	imageCopy;
			imageCopy.imageCopy	= testCopy;

			params.regions.push_back(imageCopy);
		}

		imgToImgSimpleTests->addChild(new CopyImageToImageTestCase(testCtx, "whole_image", "Whole image", params));
	}

	{
		TestParams			params;
		params.src.image.imageType	= VK_IMAGE_TYPE_2D;
		params.src.image.format		= VK_FORMAT_R8G8B8A8_UINT;
		params.src.image.extent		= defaultExtent;
		params.dst.image.imageType	= VK_IMAGE_TYPE_2D;
		params.dst.image.format		= VK_FORMAT_R32_UINT;
		params.dst.image.extent		= defaultExtent;

		{
			const VkImageCopy				testCopy	=
			{
				defaultSourceLayer,	// VkImageSubresourceLayers	srcSubresource;
				{0, 0, 0},			// VkOffset3D				srcOffset;
				defaultSourceLayer,	// VkImageSubresourceLayers	dstSubresource;
				{0, 0, 0},			// VkOffset3D				dstOffset;
				defaultExtent,		// VkExtent3D				extent;
			};

			CopyRegion	imageCopy;
			imageCopy.imageCopy	= testCopy;

			params.regions.push_back(imageCopy);
		}

		imgToImgSimpleTests->addChild(new CopyImageToImageTestCase(testCtx, "whole_image_diff_fromat", "Whole image with different format", params));
	}

	{
		TestParams			params;
		params.src.image.imageType	= VK_IMAGE_TYPE_2D;
		params.src.image.format		= VK_FORMAT_R8G8B8A8_UINT;
		params.src.image.extent		= defaultExtent;
		params.dst.image.imageType	= VK_IMAGE_TYPE_2D;
		params.dst.image.format		= VK_FORMAT_R8G8B8A8_UINT;
		params.dst.image.extent		= defaultExtent;

		{
			const VkImageCopy				testCopy	=
			{
				defaultSourceLayer,									// VkImageSubresourceLayers	srcSubresource;
				{0, 0, 0},											// VkOffset3D				srcOffset;
				defaultSourceLayer,									// VkImageSubresourceLayers	dstSubresource;
				{defaultFourthSize, defaultFourthSize / 2, 0},		// VkOffset3D				dstOffset;
				{defaultFourthSize / 2, defaultFourthSize / 2, 1},	// VkExtent3D				extent;
			};

			CopyRegion	imageCopy;
			imageCopy.imageCopy	= testCopy;

			params.regions.push_back(imageCopy);
		}

		imgToImgSimpleTests->addChild(new CopyImageToImageTestCase(testCtx, "partial_image", "Partial image", params));
	}

	{
		TestParams			params;
		params.src.image.imageType	= VK_IMAGE_TYPE_2D;
		params.src.image.format		= VK_FORMAT_D32_SFLOAT;
		params.src.image.extent		= defaultExtent;
		params.dst.image.imageType	= VK_IMAGE_TYPE_2D;
		params.dst.image.format		= VK_FORMAT_D32_SFLOAT;
		params.dst.image.extent		= defaultExtent;

		{
			const VkImageSubresourceLayers  sourceLayer =
			{
				VK_IMAGE_ASPECT_DEPTH_BIT,	// VkImageAspectFlags	aspectMask;
				0u,							// uint32_t				mipLevel;
				0u,							// uint32_t				baseArrayLayer;
				1u							// uint32_t				layerCount;
			};
			const VkImageCopy				testCopy	=
			{
				sourceLayer,										// VkImageSubresourceLayers	srcSubresource;
				{0, 0, 0},											// VkOffset3D				srcOffset;
				sourceLayer,										// VkImageSubresourceLayers	dstSubresource;
				{defaultFourthSize, defaultFourthSize / 2, 0},		// VkOffset3D				dstOffset;
				{defaultFourthSize / 2, defaultFourthSize / 2, 1},	// VkExtent3D				extent;
			};

			CopyRegion	imageCopy;
			imageCopy.imageCopy	= testCopy;

			params.regions.push_back(imageCopy);
		}

		imgToImgSimpleTests->addChild(new CopyImageToImageTestCase(testCtx, "depth", "With depth", params));
	}

	{
		TestParams			params;
		params.src.image.imageType	= VK_IMAGE_TYPE_2D;
		params.src.image.format		= VK_FORMAT_S8_UINT;
		params.src.image.extent		= defaultExtent;
		params.dst.image.imageType	= VK_IMAGE_TYPE_2D;
		params.dst.image.format		= VK_FORMAT_S8_UINT;
		params.dst.image.extent		= defaultExtent;

		{
			const VkImageSubresourceLayers  sourceLayer =
			{
				VK_IMAGE_ASPECT_STENCIL_BIT,	// VkImageAspectFlags	aspectMask;
				0u,								// uint32_t				mipLevel;
				0u,								// uint32_t				baseArrayLayer;
				1u								// uint32_t				layerCount;
			};
			const VkImageCopy				testCopy	=
			{
				sourceLayer,										// VkImageSubresourceLayers	srcSubresource;
				{0, 0, 0},											// VkOffset3D				srcOffset;
				sourceLayer,										// VkImageSubresourceLayers	dstSubresource;
				{defaultFourthSize, defaultFourthSize / 2, 0},		// VkOffset3D				dstOffset;
				{defaultFourthSize / 2, defaultFourthSize / 2, 1},	// VkExtent3D				extent;
			};

			CopyRegion	imageCopy;
			imageCopy.imageCopy	= testCopy;

			params.regions.push_back(imageCopy);
		}

		imgToImgSimpleTests->addChild(new CopyImageToImageTestCase(testCtx, "stencil", "With stencil", params));
	}

	{
		// Test Color formats.
		{
			TestParams			params;
			params.src.image.imageType	= VK_IMAGE_TYPE_2D;
			params.src.image.extent	= defaultExtent;
			params.dst.image.imageType	= VK_IMAGE_TYPE_2D;
			params.dst.image.extent	= defaultExtent;

			for (deInt32 i = 0; i < defaultSize; i += defaultFourthSize)
			{
				const VkImageCopy				testCopy	=
				{
					defaultSourceLayer,								// VkImageSubresourceLayers	srcSubresource;
					{0, 0, 0},										// VkOffset3D				srcOffset;
					defaultSourceLayer,								// VkImageSubresourceLayers	dstSubresource;
					{i, defaultSize - i - defaultFourthSize, 0},	// VkOffset3D				dstOffset;
					{defaultFourthSize, defaultFourthSize, 1},		// VkExtent3D				extent;
				};

				CopyRegion	imageCopy;
				imageCopy.imageCopy	= testCopy;

				params.regions.push_back(imageCopy);
			}

			addCopyImageTestsAllFormats(imgToImgAllFormatsTests.get(), testCtx, params);
		}

		// Test Depth and Stencil formats.
		{
			const std::string	description	("Copy image to image with depth/stencil formats ");
			const std::string	testName	("depth_stencil");

			for (size_t compatibleFormatsIndex = 0; compatibleFormatsIndex < DE_LENGTH_OF_ARRAY(depthAndStencilFormats); ++compatibleFormatsIndex)
			{
				TestParams params;

				params.src.image.imageType	= VK_IMAGE_TYPE_2D;
				params.dst.image.imageType	= VK_IMAGE_TYPE_2D;
				params.src.image.extent		= defaultExtent;
				params.dst.image.extent		= defaultExtent;
				params.src.image.format		= depthAndStencilFormats[compatibleFormatsIndex];
				params.dst.image.format		= params.src.image.format;
				std::ostringstream	oss;
				oss << testName << "_" << getFormatCaseName(params.src.image.format) << "_" << getFormatCaseName(params.dst.image.format);

				const VkImageSubresourceLayers	defaultDepthSourceLayer		= { VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, 1u };
				const VkImageSubresourceLayers	defaultStencilSourceLayer	= { VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, 1u };

				for (deInt32 i = 0; i < defaultSize; i += defaultFourthSize)
				{
					CopyRegion			copyRegion;
					const VkOffset3D	srcOffset	= {0, 0, 0};
					const VkOffset3D	dstOffset	= {i, defaultSize - i - defaultFourthSize, 0};
					const VkExtent3D	extent		= {defaultFourthSize, defaultFourthSize, 1};

					if (tcu::hasDepthComponent(mapVkFormat(params.src.image.format).order))
					{
						const VkImageCopy				testCopy	=
						{
							defaultDepthSourceLayer,	// VkImageSubresourceLayers	srcSubresource;
							srcOffset,					// VkOffset3D				srcOffset;
							defaultDepthSourceLayer,	// VkImageSubresourceLayers	dstSubresource;
							dstOffset,					// VkOffset3D				dstOffset;
							extent,						// VkExtent3D				extent;
						};

						copyRegion.imageCopy	= testCopy;
						params.regions.push_back(copyRegion);
					}
					if (tcu::hasStencilComponent(mapVkFormat(params.src.image.format).order))
					{
						const VkImageCopy				testCopy	=
						{
							defaultStencilSourceLayer,	// VkImageSubresourceLayers	srcSubresource;
							srcOffset,					// VkOffset3D				srcOffset;
							defaultStencilSourceLayer,	// VkImageSubresourceLayers	dstSubresource;
							dstOffset,					// VkOffset3D				dstOffset;
							extent,						// VkExtent3D				extent;
						};

						copyRegion.imageCopy	= testCopy;
						params.regions.push_back(copyRegion);
					}
				}

				imgToImgAllFormatsTests->addChild(new CopyImageToImageTestCase(testCtx, oss.str(), description, params));
			}
		}
	}
	imageToImageTests->addChild(imgToImgSimpleTests.release());
	imageToImageTests->addChild(imgToImgAllFormatsTests.release());

	// Copy image to buffer testcases.
	{
		TestParams	params;
		params.src.image.imageType	= VK_IMAGE_TYPE_2D;
		params.src.image.format		= VK_FORMAT_R8G8B8A8_UNORM;
		params.src.image.extent		= defaultExtent;
		params.dst.buffer.size		= defaultSize * defaultSize;

		const VkBufferImageCopy	bufferImageCopy	=
		{
			0u,											// VkDeviceSize				bufferOffset;
			0u,											// uint32_t					bufferRowLength;
			0u,											// uint32_t					bufferImageHeight;
			defaultSourceLayer,							// VkImageSubresourceLayers	imageSubresource;
			{0, 0, 0},									// VkOffset3D				imageOffset;
			defaultExtent								// VkExtent3D				imageExtent;
		};
		CopyRegion	copyRegion;
		copyRegion.bufferImageCopy	= bufferImageCopy;

		params.regions.push_back(copyRegion);

		imageToBufferTests->addChild(new CopyImageToBufferTestCase(testCtx, "whole", "Copy from image to buffer", params));
	}

	{
		TestParams	params;
		params.src.image.imageType	= VK_IMAGE_TYPE_2D;
		params.src.image.format		= VK_FORMAT_R8G8B8A8_UNORM;
		params.src.image.extent		= defaultExtent;
		params.dst.buffer.size		= defaultSize * defaultSize;

		const VkBufferImageCopy	bufferImageCopy	=
		{
			defaultSize * defaultHalfSize,				// VkDeviceSize				bufferOffset;
			0u,											// uint32_t					bufferRowLength;
			0u,											// uint32_t					bufferImageHeight;
			defaultSourceLayer,							// VkImageSubresourceLayers	imageSubresource;
			{defaultFourthSize, defaultFourthSize, 0},	// VkOffset3D				imageOffset;
			defaultHalfExtent							// VkExtent3D				imageExtent;
		};
		CopyRegion	copyRegion;
		copyRegion.bufferImageCopy	= bufferImageCopy;

		params.regions.push_back(copyRegion);

		imageToBufferTests->addChild(new CopyImageToBufferTestCase(testCtx, "buffer_offset", "Copy from image to buffer with buffer offset", params));
	}

	{
		TestParams	params;
		params.src.image.imageType	= VK_IMAGE_TYPE_2D;
		params.src.image.format		= VK_FORMAT_R8G8B8A8_UNORM;
		params.src.image.extent		= defaultExtent;
		params.dst.buffer.size		= defaultSize * defaultSize;

		const int			pixelSize	= tcu::getPixelSize(mapVkFormat(params.src.image.format));
		const VkDeviceSize	bufferSize	= pixelSize * params.dst.buffer.size;
		const VkDeviceSize	offsetSize	= pixelSize * defaultFourthSize * defaultFourthSize;
		deUint32			divisor		= 1;
		for (VkDeviceSize offset = 0; offset < bufferSize - offsetSize; offset += offsetSize, ++divisor)
		{
			const deUint32			bufferRowLength		= defaultFourthSize;
			const deUint32			bufferImageHeight	= defaultFourthSize;
			const VkExtent3D		imageExtent			= {defaultFourthSize / divisor, defaultFourthSize, 1};
			DE_ASSERT(!bufferRowLength || bufferRowLength >= imageExtent.width);
			DE_ASSERT(!bufferImageHeight || bufferImageHeight >= imageExtent.height);
			DE_ASSERT(imageExtent.width * imageExtent.height *imageExtent.depth <= offsetSize);

			CopyRegion				region;
			const VkBufferImageCopy	bufferImageCopy		=
			{
				offset,						// VkDeviceSize				bufferOffset;
				bufferRowLength,			// uint32_t					bufferRowLength;
				bufferImageHeight,			// uint32_t					bufferImageHeight;
				defaultSourceLayer,			// VkImageSubresourceLayers	imageSubresource;
				{0, 0, 0},					// VkOffset3D				imageOffset;
				imageExtent					// VkExtent3D				imageExtent;
			};
			region.bufferImageCopy	= bufferImageCopy;
			params.regions.push_back(region);
		}

		imageToBufferTests->addChild(new CopyImageToBufferTestCase(testCtx, "regions", "Copy from image to buffer with multiple regions", params));
	}

	// Copy buffer to image testcases.
	{
		TestParams	params;
		params.src.buffer.size		= defaultSize * defaultSize;
		params.dst.image.imageType	= VK_IMAGE_TYPE_2D;
		params.dst.image.format		= VK_FORMAT_R8G8B8A8_UINT;
		params.dst.image.extent		= defaultExtent;

		const VkBufferImageCopy	bufferImageCopy	=
		{
			0u,											// VkDeviceSize				bufferOffset;
			0u,											// uint32_t					bufferRowLength;
			0u,											// uint32_t					bufferImageHeight;
			defaultSourceLayer,							// VkImageSubresourceLayers	imageSubresource;
			{0, 0, 0},									// VkOffset3D				imageOffset;
			defaultExtent								// VkExtent3D				imageExtent;
		};
		CopyRegion	copyRegion;
		copyRegion.bufferImageCopy	= bufferImageCopy;

		params.regions.push_back(copyRegion);

		bufferToImageTests->addChild(new CopyBufferToImageTestCase(testCtx, "whole", "Copy from buffer to image", params));
	}

	{
		TestParams	params;
		params.src.buffer.size		= defaultSize * defaultSize;
		params.dst.image.imageType	= VK_IMAGE_TYPE_2D;
		params.dst.image.format		= VK_FORMAT_R8G8B8A8_UNORM;
		params.dst.image.extent		= defaultExtent;

		CopyRegion	region;
		deUint32	divisor	= 1;
		for (int offset = 0; (offset + defaultFourthSize / divisor < defaultSize) && (defaultFourthSize > divisor); offset += defaultFourthSize / divisor++)
		{
			const VkBufferImageCopy	bufferImageCopy	=
			{
				0u,																// VkDeviceSize				bufferOffset;
				0u,																// uint32_t					bufferRowLength;
				0u,																// uint32_t					bufferImageHeight;
				defaultSourceLayer,												// VkImageSubresourceLayers	imageSubresource;
				{offset, defaultHalfSize, 0},									// VkOffset3D				imageOffset;
				{defaultFourthSize / divisor, defaultFourthSize / divisor, 1}	// VkExtent3D				imageExtent;
			};
			region.bufferImageCopy	= bufferImageCopy;
			params.regions.push_back(region);
		}

		bufferToImageTests->addChild(new CopyBufferToImageTestCase(testCtx, "regions", "Copy from buffer to image with multiple regions", params));
	}

	{
		TestParams	params;
		params.src.buffer.size		= defaultSize * defaultSize;
		params.dst.image.imageType	= VK_IMAGE_TYPE_2D;
		params.dst.image.format		= VK_FORMAT_R8G8B8A8_UNORM;
		params.dst.image.extent		= defaultExtent;

		const VkBufferImageCopy	bufferImageCopy	=
		{
			defaultFourthSize,							// VkDeviceSize				bufferOffset;
			defaultHalfSize + defaultFourthSize,		// uint32_t					bufferRowLength;
			defaultHalfSize + defaultFourthSize,		// uint32_t					bufferImageHeight;
			defaultSourceLayer,							// VkImageSubresourceLayers	imageSubresource;
			{defaultFourthSize, defaultFourthSize, 0},	// VkOffset3D				imageOffset;
			defaultHalfExtent							// VkExtent3D				imageExtent;
		};
		CopyRegion	copyRegion;
		copyRegion.bufferImageCopy	= bufferImageCopy;

		params.regions.push_back(copyRegion);

		bufferToImageTests->addChild(new CopyBufferToImageTestCase(testCtx, "buffer_offset", "Copy from buffer to image with buffer offset", params));
	}

	// Copy buffer to buffer testcases.
	{
		TestParams			params;
		params.src.buffer.size	= defaultSize;
		params.dst.buffer.size	= defaultSize;

		const VkBufferCopy	bufferCopy	=
		{
			0u,				// VkDeviceSize	srcOffset;
			0u,				// VkDeviceSize	dstOffset;
			defaultSize,	// VkDeviceSize	size;
		};

		CopyRegion	copyRegion;
		copyRegion.bufferCopy	= bufferCopy;
		params.regions.push_back(copyRegion);

		bufferToBufferTests->addChild(new BufferToBufferTestCase(testCtx, "whole", "Whole buffer", params));
	}

	{
		TestParams			params;
		params.src.buffer.size	= defaultFourthSize;
		params.dst.buffer.size	= defaultFourthSize;

		const VkBufferCopy	bufferCopy	=
		{
			12u,	// VkDeviceSize	srcOffset;
			4u,		// VkDeviceSize	dstOffset;
			1u,		// VkDeviceSize	size;
		};

		CopyRegion	copyRegion;
		copyRegion.bufferCopy = bufferCopy;
		params.regions.push_back(copyRegion);

		bufferToBufferTests->addChild(new BufferToBufferTestCase(testCtx, "partial", "Partial", params));
	}

	{
		const deUint32		size		= 16;
		TestParams			params;
		params.src.buffer.size	= size;
		params.dst.buffer.size	= size * (size + 1);

		// Copy region with size 1..size
		for (unsigned int i = 1; i <= size; i++)
		{
			const VkBufferCopy	bufferCopy	=
			{
				0,			// VkDeviceSize	srcOffset;
				i * size,	// VkDeviceSize	dstOffset;
				i,			// VkDeviceSize	size;
			};

			CopyRegion	copyRegion;
			copyRegion.bufferCopy = bufferCopy;
			params.regions.push_back(copyRegion);
		}

		bufferToBufferTests->addChild(new BufferToBufferTestCase(testCtx, "regions", "Multiple regions", params));
	}

	// Blitting testcases.
	{
		const std::string	description	("Blit without scaling (whole)");
		const std::string	testName	("whole");

		TestParams			params;
		params.src.image.imageType	= VK_IMAGE_TYPE_2D;
		params.src.image.format		= VK_FORMAT_R8G8B8A8_UNORM;
		params.src.image.extent		= defaultExtent;
		params.dst.image.imageType	= VK_IMAGE_TYPE_2D;
		params.dst.image.extent		= defaultExtent;

		{
			const VkImageBlit				imageBlit	=
			{
				defaultSourceLayer,	// VkImageSubresourceLayers	srcSubresource;
				{
					{0, 0, 0},
					{defaultSize, defaultSize, 1}
				},					// VkOffset3D				srcOffsets[2];

				defaultSourceLayer,	// VkImageSubresourceLayers	dstSubresource;
				{
					{0, 0, 0},
					{defaultSize, defaultSize, 1}
				}					// VkOffset3D				dstOffset[2];
			};

			CopyRegion	region;
			region.imageBlit = imageBlit;
			params.regions.push_back(region);
		}

		// Filter is VK_FILTER_NEAREST.
		{
			params.filter			= VK_FILTER_NEAREST;

			params.dst.image.format	= VK_FORMAT_R8G8B8A8_UNORM;
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + "_nearest", description, params));

			params.dst.image.format	= VK_FORMAT_R32_SFLOAT;
			const std::string	descriptionOfRGBAToR32	(description + " and different formats (R8G8B8A8 -> R32)");
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + getFormatCaseName(params.dst.image.format) + "_nearest", descriptionOfRGBAToR32, params));

			params.dst.image.format	= VK_FORMAT_B8G8R8A8_UNORM;
			const std::string	descriptionOfRGBAToBGRA	(description + " and different formats (R8G8B8A8 -> B8G8R8A8)");
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + getFormatCaseName(params.dst.image.format) + "_nearest", descriptionOfRGBAToBGRA, params));
		}

		// Filter is VK_FILTER_LINEAR.
		{
			params.filter			= VK_FILTER_LINEAR;

			params.dst.image.format	= VK_FORMAT_R8G8B8A8_UNORM;
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + "_linear", description + " (VK_FILTER_LINEAR)", params));

			params.dst.image.format	= VK_FORMAT_R32_SFLOAT;
			const std::string	descriptionOfRGBAToR32	(description + " and different formats (R8G8B8A8 -> R32)" + " (VK_FILTER_LINEAR)");
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + getFormatCaseName(params.dst.image.format) + "_linear", descriptionOfRGBAToR32, params));

			params.dst.image.format	= VK_FORMAT_B8G8R8A8_UNORM;
			const std::string	descriptionOfRGBAToBGRA	(description + " and different formats (R8G8B8A8 -> B8G8R8A8)" + " (VK_FILTER_LINEAR)");
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + getFormatCaseName(params.dst.image.format) + "_linear", descriptionOfRGBAToBGRA, params));
		}
	}

	{
		const std::string	description	("Flipping x and y coordinates (whole)");
		const std::string	testName	("mirror_xy");

		TestParams			params;
		params.src.image.imageType	= VK_IMAGE_TYPE_2D;
		params.src.image.format		= VK_FORMAT_R8G8B8A8_UNORM;
		params.src.image.extent		= defaultExtent;
		params.dst.image.imageType	= VK_IMAGE_TYPE_2D;
		params.dst.image.extent		= defaultExtent;

		{
			const VkImageBlit				imageBlit	=
			{
				defaultSourceLayer,	// VkImageSubresourceLayers	srcSubresource;
				{
					{0, 0, 0},
					{defaultSize, defaultSize, 1}
				},					// VkOffset3D				srcOffsets[2];

				defaultSourceLayer,	// VkImageSubresourceLayers	dstSubresource;
				{
					{defaultSize, defaultSize, 1},
					{0, 0, 0}
				}					// VkOffset3D				dstOffset[2];
			};

			CopyRegion	region;
			region.imageBlit = imageBlit;
			params.regions.push_back(region);
		}

		// Filter is VK_FILTER_NEAREST.
		{
			params.filter			= VK_FILTER_NEAREST;

			params.dst.image.format	= VK_FORMAT_R8G8B8A8_UNORM;
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + "_nearest", description, params));

			params.dst.image.format	= VK_FORMAT_R32_SFLOAT;
			const std::string	descriptionOfRGBAToR32	(description + " and different formats (R8G8B8A8 -> R32)");
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + getFormatCaseName(params.dst.image.format) + "_nearest", descriptionOfRGBAToR32, params));

			params.dst.image.format	= VK_FORMAT_B8G8R8A8_UNORM;
			const std::string	descriptionOfRGBAToBGRA	(description + " and different formats (R8G8B8A8 -> B8G8R8A8)");
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + "_" +  getFormatCaseName(params.dst.image.format) + "_nearest", descriptionOfRGBAToBGRA, params));
		}

		// Filter is VK_FILTER_LINEAR.
		{
			params.filter			= VK_FILTER_LINEAR;

			params.dst.image.format	= VK_FORMAT_R8G8B8A8_UNORM;
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + "_linear", description + " (VK_FILTER_LINEAR)", params));

			params.dst.image.format	= VK_FORMAT_R32_SFLOAT;
			const std::string	descriptionOfRGBAToR32	(description + " and different formats (R8G8B8A8 -> R32)" + " (VK_FILTER_LINEAR)");
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + "_" + getFormatCaseName(params.dst.image.format) + "_linear", descriptionOfRGBAToR32, params));

			params.dst.image.format	= VK_FORMAT_B8G8R8A8_UNORM;
			const std::string	descriptionOfRGBAToBGRA	(description + " and different formats (R8G8B8A8 -> B8G8R8A8)" + " (VK_FILTER_LINEAR)");
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + "_" + getFormatCaseName(params.dst.image.format) + "_linear", descriptionOfRGBAToBGRA, params));
		}
	}

	{
		const std::string	description	("Flipping x coordinates (whole)");
		const std::string	testName	("mirror_x");

		TestParams			params;
		params.src.image.imageType	= VK_IMAGE_TYPE_2D;
		params.src.image.format		= VK_FORMAT_R8G8B8A8_UNORM;
		params.src.image.extent		= defaultExtent;
		params.dst.image.imageType	= VK_IMAGE_TYPE_2D;
		params.dst.image.extent		= defaultExtent;

		{
			const VkImageBlit				imageBlit	=
			{
				defaultSourceLayer,	// VkImageSubresourceLayers	srcSubresource;
				{
					{0, 0, 0},
					{defaultSize, defaultSize, 1}
				},					// VkOffset3D				srcOffsets[2];

				defaultSourceLayer,	// VkImageSubresourceLayers	dstSubresource;
				{
					{defaultSize, 0, 0},
					{0, defaultSize, 1}
				}					// VkOffset3D				dstOffset[2];
			};

			CopyRegion	region;
			region.imageBlit = imageBlit;
			params.regions.push_back(region);
		}

		// Filter is VK_FILTER_NEAREST.
		{
			params.filter			= VK_FILTER_NEAREST;

			params.dst.image.format	= VK_FORMAT_R8G8B8A8_UNORM;
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + "_nearest", description, params));

			params.dst.image.format	= VK_FORMAT_R32_SFLOAT;
			const std::string	descriptionOfRGBAToR32	(description + " and different formats (R8G8B8A8 -> R32)");
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + "_" + getFormatCaseName(params.dst.image.format) + "_nearest", descriptionOfRGBAToR32, params));

			params.dst.image.format	= VK_FORMAT_B8G8R8A8_UNORM;
			const std::string	descriptionOfRGBAToBGRA	(description + " and different formats (R8G8B8A8 -> B8G8R8A8)");
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + "_" + getFormatCaseName(params.dst.image.format) + "_nearest", descriptionOfRGBAToBGRA, params));
		}

		// Filter is VK_FILTER_LINEAR.
		{
			params.filter			= VK_FILTER_LINEAR;

			params.dst.image.format	= VK_FORMAT_R8G8B8A8_UNORM;
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + "_linear", description + " (VK_FILTER_LINEAR)", params));

			params.dst.image.format	= VK_FORMAT_R32_SFLOAT;
			const std::string	descriptionOfRGBAToR32	(description + " and different formats (R8G8B8A8 -> R32)" + " (VK_FILTER_LINEAR)");
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + "_" + getFormatCaseName(params.dst.image.format) + "_linear", descriptionOfRGBAToR32, params));

			params.dst.image.format	= VK_FORMAT_B8G8R8A8_UNORM;
			const std::string	descriptionOfRGBAToBGRA	(description + " and different formats (R8G8B8A8 -> B8G8R8A8)" + " (VK_FILTER_LINEAR)");
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + "_" + getFormatCaseName(params.dst.image.format) + "_linear", descriptionOfRGBAToBGRA, params));
		}
	}

	{
		const std::string	description	("Flipping Y coordinates (whole)");
		const std::string	testName	("mirror_y");

		TestParams			params;
		params.src.image.imageType	= VK_IMAGE_TYPE_2D;
		params.src.image.format		= VK_FORMAT_R8G8B8A8_UNORM;
		params.src.image.extent		= defaultExtent;
		params.dst.image.imageType	= VK_IMAGE_TYPE_2D;
		params.dst.image.extent		= defaultExtent;

		{
			const VkImageBlit				imageBlit	=
			{
				defaultSourceLayer,	// VkImageSubresourceLayers	srcSubresource;
				{
					{0, 0, 0},
					{defaultSize, defaultSize, 1}
				},					// VkOffset3D				srcOffsets[2];

				defaultSourceLayer,	// VkImageSubresourceLayers	dstSubresource;
				{
					{0, defaultSize, 1},
					{defaultSize, 0, 0}
				}					// VkOffset3D				dstOffset[2];
			};

			CopyRegion	region;
			region.imageBlit = imageBlit;
			params.regions.push_back(region);
		}

		// Filter is VK_FILTER_NEAREST.
		{
			params.filter			= VK_FILTER_NEAREST;

			params.dst.image.format	= VK_FORMAT_R8G8B8A8_UNORM;
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + "_nearest", description, params));

			params.dst.image.format	= VK_FORMAT_R32_SFLOAT;
			const std::string	descriptionOfRGBAToR32	(description + " and different formats (R8G8B8A8 -> R32)");
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + "_" + getFormatCaseName(params.dst.image.format) + "_nearest", descriptionOfRGBAToR32, params));

			params.dst.image.format	= VK_FORMAT_B8G8R8A8_UNORM;
			const std::string	descriptionOfRGBAToBGRA	(description + " and different formats (R8G8B8A8 -> B8G8R8A8)");
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + "_" + getFormatCaseName(params.dst.image.format) + "_nearest", descriptionOfRGBAToBGRA, params));
		}

		// Filter is VK_FILTER_LINEAR.
		{
			params.filter			= VK_FILTER_LINEAR;

			params.dst.image.format	= VK_FORMAT_R8G8B8A8_UNORM;
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + "_linear", description + " (VK_FILTER_LINEAR)", params));

			params.dst.image.format	= VK_FORMAT_R32_SFLOAT;
			const std::string	descriptionOfRGBAToR32	(description + " and different formats (R8G8B8A8 -> R32)" + " (VK_FILTER_LINEAR)");
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + "_" + getFormatCaseName(params.dst.image.format) + "_linear", descriptionOfRGBAToR32, params));

			params.dst.image.format	= VK_FORMAT_B8G8R8A8_UNORM;
			const std::string	descriptionOfRGBAToBGRA	(description + " and different formats (R8G8B8A8 -> B8G8R8A8)" + " (VK_FILTER_LINEAR)");
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + "_" + getFormatCaseName(params.dst.image.format) + "_linear", descriptionOfRGBAToBGRA, params));
		}
	}

	{
		const std::string	description	("Mirroring subregions in image (no flip ,y flip ,x flip, xy flip)");
		const std::string	testName	("mirror_subregions");

		TestParams			params;
		params.src.image.imageType	= VK_IMAGE_TYPE_2D;
		params.src.image.format		= VK_FORMAT_R8G8B8A8_UNORM;
		params.src.image.extent		= defaultExtent;
		params.dst.image.imageType	= VK_IMAGE_TYPE_2D;
		params.dst.image.extent		= defaultExtent;

		// No mirroring.
		{
			const VkImageBlit				imageBlit	=
			{
				defaultSourceLayer,	// VkImageSubresourceLayers	srcSubresource;
				{
					{0, 0, 0},
					{defaultHalfSize, defaultHalfSize, 1}
				},					// VkOffset3D				srcOffsets[2];

				defaultSourceLayer,	// VkImageSubresourceLayers	dstSubresource;
				{
					{0, 0, 0},
					{defaultHalfSize, defaultHalfSize, 1}
				}					// VkOffset3D				dstOffset[2];
			};

			CopyRegion	region;
			region.imageBlit = imageBlit;
			params.regions.push_back(region);
		}

		// Flipping y coordinates.
		{
			const VkImageBlit				imageBlit	=
			{
				defaultSourceLayer,	// VkImageSubresourceLayers	srcSubresource;
				{
					{defaultHalfSize, 0, 0},
					{defaultSize, defaultHalfSize, 1}
				},					// VkOffset3D				srcOffsets[2];

				defaultSourceLayer,	// VkImageSubresourceLayers	dstSubresource;
				{
					{defaultHalfSize, defaultHalfSize, 0},
					{defaultSize, 0, 1}
				}					// VkOffset3D				dstOffset[2];
			};

			CopyRegion	region;
			region.imageBlit = imageBlit;
			params.regions.push_back(region);
		}

		// Flipping x coordinates.
		{
			const VkImageBlit				imageBlit	=
			{
				defaultSourceLayer,	// VkImageSubresourceLayers	srcSubresource;
				{
					{0, defaultHalfSize, 0},
					{defaultHalfSize, defaultSize, 1}
				},					// VkOffset3D				srcOffsets[2];

				defaultSourceLayer,	// VkImageSubresourceLayers	dstSubresource;
				{
					{defaultHalfSize, defaultHalfSize, 0},
					{0, defaultSize, 1}
				}					// VkOffset3D				dstOffset[2];
			};

			CopyRegion	region;
			region.imageBlit = imageBlit;
			params.regions.push_back(region);
		}

		// Flipping x and y coordinates.
		{
			const VkImageBlit				imageBlit	=
			{
				defaultSourceLayer,	// VkImageSubresourceLayers	srcSubresource;
				{
					{defaultHalfSize, defaultHalfSize, 0},
					{defaultSize, defaultSize, 1}
				},					// VkOffset3D				srcOffsets[2];

				defaultSourceLayer,	// VkImageSubresourceLayers	dstSubresource;
				{
					{defaultSize, defaultSize, 0},
					{defaultHalfSize, defaultHalfSize, 1}
				}					// VkOffset3D				dstOffset[2];
			};

			CopyRegion	region;
			region.imageBlit = imageBlit;
			params.regions.push_back(region);
		}

		// Filter is VK_FILTER_NEAREST.
		{
			params.filter			= VK_FILTER_NEAREST;

			params.dst.image.format	= VK_FORMAT_R8G8B8A8_UNORM;
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + "_nearest", description, params));

			params.dst.image.format	= VK_FORMAT_R32_SFLOAT;
			const std::string	descriptionOfRGBAToR32	(description + " and different formats (R8G8B8A8 -> R32)");
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + "_" + getFormatCaseName(params.dst.image.format) + "_nearest", descriptionOfRGBAToR32, params));

			params.dst.image.format	= VK_FORMAT_B8G8R8A8_UNORM;
			const std::string	descriptionOfRGBAToBGRA	(description + " and different formats (R8G8B8A8 -> B8G8R8A8)");
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + "_" + getFormatCaseName(params.dst.image.format) + "_nearest", descriptionOfRGBAToBGRA, params));
		}

		// Filter is VK_FILTER_LINEAR.
		{
			params.filter			= VK_FILTER_LINEAR;

			params.dst.image.format	= VK_FORMAT_R8G8B8A8_UNORM;
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + "_linear", description + " (VK_FILTER_LINEAR)", params));

			params.dst.image.format	= VK_FORMAT_R32_SFLOAT;
			const std::string	descriptionOfRGBAToR32	(description + " and different formats (R8G8B8A8 -> R32)" + " (VK_FILTER_LINEAR)");
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + "_" + getFormatCaseName(params.dst.image.format) + "_linear", descriptionOfRGBAToR32, params));

			params.dst.image.format	= VK_FORMAT_B8G8R8A8_UNORM;
			const std::string	descriptionOfRGBAToBGRA	(description + " and different formats (R8G8B8A8 -> B8G8R8A8)" + " (VK_FILTER_LINEAR)");
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + "_" + getFormatCaseName(params.dst.image.format) + "_linear", descriptionOfRGBAToBGRA, params));
		}
	}

	{
		const std::string	description	("Blit with scaling (whole, src extent bigger)");
		const std::string	testName	("scaling_whole1");

		TestParams			params;
		params.src.image.imageType	= VK_IMAGE_TYPE_2D;
		params.src.image.format		= VK_FORMAT_R8G8B8A8_UNORM;
		params.src.image.extent		= defaultExtent;
		params.dst.image.imageType	= VK_IMAGE_TYPE_2D;
		params.dst.image.extent		= defaultHalfExtent;

		{
			const VkImageBlit				imageBlit	=
			{
				defaultSourceLayer,	// VkImageSubresourceLayers	srcSubresource;
				{
					{0, 0, 0},
					{defaultSize, defaultSize, 1}
				},					// VkOffset3D					srcOffsets[2];

				defaultSourceLayer,	// VkImageSubresourceLayers	dstSubresource;
				{
					{0, 0, 0},
					{defaultHalfSize, defaultHalfSize, 1}
				}					// VkOffset3D					dstOffset[2];
			};

			CopyRegion	region;
			region.imageBlit	= imageBlit;
			params.regions.push_back(region);
		}

		// Filter is VK_FILTER_NEAREST.
		{
			params.filter			= VK_FILTER_NEAREST;

			params.dst.image.format	= VK_FORMAT_R8G8B8A8_UNORM;
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + "_nearest", description, params));

			params.dst.image.format	= VK_FORMAT_R32_SFLOAT;
			const std::string	descriptionOfRGBAToR32	(description + " and different formats (R8G8B8A8 -> R32)");
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + getFormatCaseName(params.dst.image.format) + "_nearest", descriptionOfRGBAToR32, params));

			params.dst.image.format	= VK_FORMAT_B8G8R8A8_UNORM;
			const std::string	descriptionOfRGBAToBGRA	(description + " and different formats (R8G8B8A8 -> B8G8R8A8)");
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + getFormatCaseName(params.dst.image.format) + "_nearest", descriptionOfRGBAToBGRA, params));
		}

		// Filter is VK_FILTER_LINEAR.
		{
			params.filter			= VK_FILTER_LINEAR;

			params.dst.image.format	= VK_FORMAT_R8G8B8A8_UNORM;
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + "_linear", description + " (VK_FILTER_LINEAR)", params));

			params.dst.image.format	= VK_FORMAT_R32_SFLOAT;
			const std::string	descriptionOfRGBAToR32	(description + " and different formats (R8G8B8A8 -> R32)" + " (VK_FILTER_LINEAR)");
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + getFormatCaseName(params.dst.image.format) + "_linear", descriptionOfRGBAToR32, params));

			params.dst.image.format	= VK_FORMAT_B8G8R8A8_UNORM;
			const std::string	descriptionOfRGBAToBGRA	(description + " and different formats (R8G8B8A8 -> B8G8R8A8)" + " (VK_FILTER_LINEAR)");
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + getFormatCaseName(params.dst.image.format) + "_linear", descriptionOfRGBAToBGRA, params));
		}
	}

	{
		const std::string	description	("Blit with scaling (whole, dst extent bigger)");
		const std::string	testName	("scaling_whole2");

		TestParams			params;
		params.src.image.imageType	= VK_IMAGE_TYPE_2D;
		params.src.image.format		= VK_FORMAT_R8G8B8A8_UNORM;
		params.src.image.extent		= defaultHalfExtent;
		params.dst.image.imageType	= VK_IMAGE_TYPE_2D;
		params.dst.image.extent		= defaultExtent;

		{
			const VkImageBlit				imageBlit	=
			{
				defaultSourceLayer,	// VkImageSubresourceLayers	srcSubresource;
				{
					{0, 0, 0},
					{defaultHalfSize, defaultHalfSize, 1}
				},					// VkOffset3D					srcOffsets[2];

				defaultSourceLayer,	// VkImageSubresourceLayers	dstSubresource;
				{
					{0, 0, 0},
					{defaultSize, defaultSize, 1}
				}					// VkOffset3D					dstOffset[2];
			};

			CopyRegion	region;
			region.imageBlit	= imageBlit;
			params.regions.push_back(region);
		}

		// Filter is VK_FILTER_NEAREST.
		{
			params.filter			= VK_FILTER_NEAREST;

			params.dst.image.format	= VK_FORMAT_R8G8B8A8_UNORM;
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + "_nearest", description, params));

			params.dst.image.format	= VK_FORMAT_R32_SFLOAT;
			const std::string	descriptionOfRGBAToR32	(description + " and different formats (R8G8B8A8 -> R32)");
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + getFormatCaseName(params.dst.image.format) + "_nearest", descriptionOfRGBAToR32, params));

			params.dst.image.format	= VK_FORMAT_B8G8R8A8_UNORM;
			const std::string	descriptionOfRGBAToBGRA	(description + " and different formats (R8G8B8A8 -> B8G8R8A8)");
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + getFormatCaseName(params.dst.image.format) + "_nearest", descriptionOfRGBAToBGRA, params));
		}

		// Filter is VK_FILTER_LINEAR.
		{
			params.filter			= VK_FILTER_LINEAR;

			params.dst.image.format	= VK_FORMAT_R8G8B8A8_UNORM;
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + "_linear", description + " (VK_FILTER_LINEAR)", params));

			params.dst.image.format	= VK_FORMAT_R32_SFLOAT;
			const std::string	descriptionOfRGBAToR32	(description + " and different formats (R8G8B8A8 -> R32)" + " (VK_FILTER_LINEAR)");
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + getFormatCaseName(params.dst.image.format) + "_linear", descriptionOfRGBAToR32, params));

			params.dst.image.format	= VK_FORMAT_B8G8R8A8_UNORM;
			const std::string	descriptionOfRGBAToBGRA	(description + " and different formats (R8G8B8A8 -> B8G8R8A8)" + " (VK_FILTER_LINEAR)");
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + getFormatCaseName(params.dst.image.format) + "_linear", descriptionOfRGBAToBGRA, params));
		}
	}

	{
		const std::string	description	("Blit with scaling and offset (whole, dst extent bigger)");
		const std::string	testName	("scaling_and_offset");

		TestParams			params;
		params.src.image.imageType	= VK_IMAGE_TYPE_2D;
		params.src.image.format		= VK_FORMAT_R8G8B8A8_UNORM;
		params.src.image.extent		= defaultExtent;
		params.dst.image.imageType	= VK_IMAGE_TYPE_2D;
		params.dst.image.extent		= defaultExtent;

		{
			const VkImageBlit				imageBlit	=
			{
				defaultSourceLayer,	// VkImageSubresourceLayers	srcSubresource;
				{
					{defaultFourthSize, defaultFourthSize, 0},
					{defaultFourthSize*3, defaultFourthSize*3, 1}
				},					// VkOffset3D					srcOffsets[2];

				defaultSourceLayer,	// VkImageSubresourceLayers	dstSubresource;
				{
					{0, 0, 0},
					{defaultSize, defaultSize, 1}
				}					// VkOffset3D					dstOffset[2];
			};

			CopyRegion	region;
			region.imageBlit	= imageBlit;
			params.regions.push_back(region);
		}

		// Filter is VK_FILTER_NEAREST.
		{
			params.filter			= VK_FILTER_NEAREST;

			params.dst.image.format	= VK_FORMAT_R8G8B8A8_UNORM;
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + "_nearest", description, params));

			params.dst.image.format	= VK_FORMAT_R32_SFLOAT;
			const std::string	descriptionOfRGBAToR32	(description + " and different formats (R8G8B8A8 -> R32)");
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + getFormatCaseName(params.dst.image.format) + "_nearest", descriptionOfRGBAToR32, params));

			params.dst.image.format	= VK_FORMAT_B8G8R8A8_UNORM;
			const std::string	descriptionOfRGBAToBGRA	(description + " and different formats (R8G8B8A8 -> B8G8R8A8)");
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + getFormatCaseName(params.dst.image.format) + "_nearest", descriptionOfRGBAToBGRA, params));
		}

		// Filter is VK_FILTER_LINEAR.
		{
			params.filter			= VK_FILTER_LINEAR;

			params.dst.image.format	= VK_FORMAT_R8G8B8A8_UNORM;
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + "_linear", description + " (VK_FILTER_LINEAR)", params));

			params.dst.image.format	= VK_FORMAT_R32_SFLOAT;
			const std::string	descriptionOfRGBAToR32	(description + " and different formats (R8G8B8A8 -> R32)" + " (VK_FILTER_LINEAR)");
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + getFormatCaseName(params.dst.image.format) + "_linear", descriptionOfRGBAToR32, params));

			params.dst.image.format	= VK_FORMAT_B8G8R8A8_UNORM;
			const std::string	descriptionOfRGBAToBGRA	(description + " and different formats (R8G8B8A8 -> B8G8R8A8)" + " (VK_FILTER_LINEAR)");
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + getFormatCaseName(params.dst.image.format) + "_linear", descriptionOfRGBAToBGRA, params));
		}
	}

	{
		const std::string	description	("Blit without scaling (partial)");
		const std::string	testName	("without_scaling_partial");

		TestParams			params;
		params.src.image.imageType	= VK_IMAGE_TYPE_2D;
		params.src.image.format		= VK_FORMAT_R8G8B8A8_UNORM;
		params.src.image.extent		= defaultExtent;
		params.dst.image.imageType	= VK_IMAGE_TYPE_2D;
		params.dst.image.extent		= defaultExtent;

		{
			CopyRegion	region;
			for (int i = 0; i < defaultSize; i += defaultFourthSize)
			{
				const VkImageBlit			imageBlit	=
				{
					defaultSourceLayer,	// VkImageSubresourceLayers	srcSubresource;
					{
						{defaultSize - defaultFourthSize - i, defaultSize - defaultFourthSize - i, 0},
						{defaultSize - i, defaultSize - i, 1}
					},					// VkOffset3D					srcOffsets[2];

					defaultSourceLayer,	// VkImageSubresourceLayers	dstSubresource;
					{
						{i, i, 0},
						{i + defaultFourthSize, i + defaultFourthSize, 1}
					}					// VkOffset3D					dstOffset[2];
				};
				region.imageBlit	= imageBlit;
				params.regions.push_back(region);
			}
		}

		// Filter is VK_FILTER_NEAREST.
		{
			params.filter			= VK_FILTER_NEAREST;

			params.dst.image.format	= VK_FORMAT_R8G8B8A8_UNORM;
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + "_nearest", description, params));

			params.dst.image.format	= VK_FORMAT_R32_SFLOAT;
			const std::string	descriptionOfRGBAToR32	(description + " and different formats (R8G8B8A8 -> R32)");
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + getFormatCaseName(params.dst.image.format) + "_nearest", descriptionOfRGBAToR32, params));

			params.dst.image.format	= VK_FORMAT_B8G8R8A8_UNORM;
			const std::string	descriptionOfRGBAToBGRA	(description + " and different formats (R8G8B8A8 -> B8G8R8A8)");
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + getFormatCaseName(params.dst.image.format) + "_nearest", descriptionOfRGBAToBGRA, params));
		}

		// Filter is VK_FILTER_LINEAR.
		{
			params.filter			= VK_FILTER_LINEAR;

			params.dst.image.format	= VK_FORMAT_R8G8B8A8_UNORM;
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + "_linear", description + " (VK_FILTER_LINEAR)", params));

			params.dst.image.format	= VK_FORMAT_R32_SFLOAT;
			const std::string	descriptionOfRGBAToR32	(description + " and different formats (R8G8B8A8 -> R32)" + " (VK_FILTER_LINEAR)");
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + getFormatCaseName(params.dst.image.format) + "_linear", descriptionOfRGBAToR32, params));

			params.dst.image.format	= VK_FORMAT_B8G8R8A8_UNORM;
			const std::string	descriptionOfRGBAToBGRA	(description + " and different formats (R8G8B8A8 -> B8G8R8A8)" + " (VK_FILTER_LINEAR)");
			blitImgSimpleTests->addChild(new BlittingTestCase(testCtx, testName + getFormatCaseName(params.dst.image.format) + "_linear", descriptionOfRGBAToBGRA, params));
		}
	}

	{
		const std::string	description	("Blit with scaling (partial)");
		const std::string	testName	("scaling_partial");

		// Test Color formats.
		{
			TestParams	params;
			params.src.image.imageType	= VK_IMAGE_TYPE_2D;
			params.src.image.extent		= defaultExtent;
			params.dst.image.imageType	= VK_IMAGE_TYPE_2D;
			params.dst.image.extent		= defaultExtent;

			CopyRegion	region;
			for (int i = 0, j = 1; (i + defaultFourthSize / j < defaultSize) && (defaultFourthSize > j); i += defaultFourthSize / j++)
			{
				const VkImageBlit			imageBlit	=
				{
					defaultSourceLayer,	// VkImageSubresourceLayers	srcSubresource;
					{
						{0, 0, 0},
						{defaultSize, defaultSize, 1}
					},					// VkOffset3D					srcOffsets[2];

					defaultSourceLayer,	// VkImageSubresourceLayers	dstSubresource;
					{
						{i, 0, 0},
						{i + defaultFourthSize / j, defaultFourthSize / j, 1}
					}					// VkOffset3D					dstOffset[2];
				};
				region.imageBlit	= imageBlit;
				params.regions.push_back(region);
			}
			for (int i = 0; i < defaultSize; i += defaultFourthSize)
			{
				const VkImageBlit			imageBlit	=
				{
					defaultSourceLayer,	// VkImageSubresourceLayers	srcSubresource;
					{
						{i, i, 0},
						{i + defaultFourthSize, i + defaultFourthSize, 1}
					},					// VkOffset3D					srcOffsets[2];

					defaultSourceLayer,	// VkImageSubresourceLayers	dstSubresource;
					{
						{i, defaultSize / 2, 0},
						{i + defaultFourthSize, defaultSize / 2 + defaultFourthSize, 1}
					}					// VkOffset3D					dstOffset[2];
				};
				region.imageBlit	= imageBlit;
				params.regions.push_back(region);
			}

			addBlittingTestsAllFormats(blitImgAllFormatsTests.get(), testCtx, params);
		}

		// Test Depth and Stencil formats.
		{
			for (size_t compatibleFormatsIndex = 0; compatibleFormatsIndex < DE_LENGTH_OF_ARRAY(depthAndStencilFormats); ++compatibleFormatsIndex)
			{
				TestParams params;

				params.src.image.imageType	= VK_IMAGE_TYPE_2D;
				params.src.image.extent		= defaultExtent;
				params.dst.image.extent		= defaultExtent;
				params.src.image.format		= depthAndStencilFormats[compatibleFormatsIndex];
				params.dst.image.imageType	= VK_IMAGE_TYPE_2D;
				params.dst.image.format		= params.src.image.format;
				std::ostringstream	oss;
				oss << testName << "_" << getFormatCaseName(params.src.image.format) << "_" << getFormatCaseName(params.dst.image.format);

				const VkImageSubresourceLayers	defaultDepthSourceLayer		= { VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, 1u };
				const VkImageSubresourceLayers	defaultStencilSourceLayer	= { VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, 1u };

				CopyRegion	region;
				for (int i = 0, j = 1; (i + defaultFourthSize / j < defaultSize) && (defaultFourthSize > j); i += defaultFourthSize / j++)
				{
					const VkOffset3D	srcOffset0	= {0, 0, 0};
					const VkOffset3D	srcOffset1	= {defaultSize, defaultSize, 1};
					const VkOffset3D	dstOffset0	= {i, 0, 0};
					const VkOffset3D	dstOffset1	= {i + defaultFourthSize / j, defaultFourthSize / j, 1};

					if (tcu::hasDepthComponent(mapVkFormat(params.src.image.format).order))
					{
						const VkImageBlit			imageBlit	=
						{
							defaultDepthSourceLayer,		// VkImageSubresourceLayers	srcSubresource;
							{ srcOffset0 , srcOffset1 },	// VkOffset3D					srcOffsets[2];
							defaultDepthSourceLayer,		// VkImageSubresourceLayers	dstSubresource;
							{ dstOffset0 , dstOffset1 },	// VkOffset3D					dstOffset[2];
						};
						region.imageBlit	= imageBlit;
						params.regions.push_back(region);
					}
					if (tcu::hasStencilComponent(mapVkFormat(params.src.image.format).order))
					{
						const VkImageBlit			imageBlit	=
						{
							defaultStencilSourceLayer,		// VkImageSubresourceLayers	srcSubresource;
							{ srcOffset0 , srcOffset1 },	// VkOffset3D					srcOffsets[2];
							defaultStencilSourceLayer,		// VkImageSubresourceLayers	dstSubresource;
							{ dstOffset0 , dstOffset1 },	// VkOffset3D					dstOffset[2];
						};
						region.imageBlit	= imageBlit;
						params.regions.push_back(region);
					}
				}
				for (int i = 0; i < defaultSize; i += defaultFourthSize)
				{
					const VkOffset3D	srcOffset0	= {i, i, 0};
					const VkOffset3D	srcOffset1	= {i + defaultFourthSize, i + defaultFourthSize, 1};
					const VkOffset3D	dstOffset0	= {i, defaultSize / 2, 0};
					const VkOffset3D	dstOffset1	= {i + defaultFourthSize, defaultSize / 2 + defaultFourthSize, 1};

					if (tcu::hasDepthComponent(mapVkFormat(params.src.image.format).order))
					{
						const VkImageBlit			imageBlit	=
						{
							defaultDepthSourceLayer,		// VkImageSubresourceLayers	srcSubresource;
							{ srcOffset0, srcOffset1 },		// VkOffset3D					srcOffsets[2];
							defaultDepthSourceLayer,		// VkImageSubresourceLayers	dstSubresource;
							{ dstOffset0, dstOffset1 }		// VkOffset3D					dstOffset[2];
						};
						region.imageBlit	= imageBlit;
						params.regions.push_back(region);
					}
					if (tcu::hasStencilComponent(mapVkFormat(params.src.image.format).order))
					{
						const VkImageBlit			imageBlit	=
						{
							defaultStencilSourceLayer,		// VkImageSubresourceLayers	srcSubresource;
							{ srcOffset0, srcOffset1 },		// VkOffset3D					srcOffsets[2];
							defaultStencilSourceLayer,		// VkImageSubresourceLayers	dstSubresource;
							{ dstOffset0, dstOffset1 }		// VkOffset3D					dstOffset[2];
						};
						region.imageBlit	= imageBlit;
						params.regions.push_back(region);
					}
				}

				params.filter			= VK_FILTER_NEAREST;
				blitImgAllFormatsTests->addChild(new BlittingTestCase(testCtx, oss.str() + "_nearest", description, params));
			}
		}
	}
	blittingImageTests->addChild(blitImgSimpleTests.release());
	blittingImageTests->addChild(blitImgAllFormatsTests.release());

	// Resolve image to image testcases.
	const VkSampleCountFlagBits	samples[]		=
	{
		VK_SAMPLE_COUNT_2_BIT,
		VK_SAMPLE_COUNT_4_BIT,
		VK_SAMPLE_COUNT_8_BIT,
		VK_SAMPLE_COUNT_16_BIT,
		VK_SAMPLE_COUNT_32_BIT,
		VK_SAMPLE_COUNT_64_BIT
	};
	const VkExtent3D			resolveExtent	= {256u, 256u, 1};

	{
		const std::string	description	("Resolve from image to image");
		const std::string	testName	("whole");

		TestParams			params;
		params.src.image.imageType	= VK_IMAGE_TYPE_2D;
		params.src.image.format		= VK_FORMAT_R8G8B8A8_UNORM;
		params.src.image.extent		= resolveExtent;
		params.dst.image.imageType	= VK_IMAGE_TYPE_2D;
		params.dst.image.format		= VK_FORMAT_R8G8B8A8_UNORM;
		params.dst.image.extent		= resolveExtent;

		{
			const VkImageSubresourceLayers	sourceLayer	=
			{
				VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
				0u,							// uint32_t				mipLevel;
				0u,							// uint32_t				baseArrayLayer;
				1u							// uint32_t				layerCount;
			};
			const VkImageResolve			testResolve	=
			{
				sourceLayer,	// VkImageSubresourceLayers	srcSubresource;
				{0, 0, 0},		// VkOffset3D				srcOffset;
				sourceLayer,	// VkImageSubresourceLayers	dstSubresource;
				{0, 0, 0},		// VkOffset3D				dstOffset;
				resolveExtent,	// VkExtent3D				extent;
			};

			CopyRegion	imageResolve;
			imageResolve.imageResolve	= testResolve;
			params.regions.push_back(imageResolve);
		}

		for (int samplesIndex = 0; samplesIndex < DE_LENGTH_OF_ARRAY(samples); ++samplesIndex)
		{
			params.samples = samples[samplesIndex];
			std::ostringstream caseName;
			caseName << testName << "_" << getSampleCountCaseName(samples[samplesIndex]);
			resolveImageTests->addChild(new ResolveImageToImageTestCase(testCtx, caseName.str(), description, params));
		}
	}

	{
		const std::string	description	("Resolve from image to image");
		const std::string	testName	("partial");

		TestParams			params;
		params.src.image.imageType	= VK_IMAGE_TYPE_2D;
		params.src.image.format		= VK_FORMAT_R8G8B8A8_UNORM;
		params.src.image.extent		= resolveExtent;
		params.dst.image.imageType	= VK_IMAGE_TYPE_2D;
		params.dst.image.format		= VK_FORMAT_R8G8B8A8_UNORM;
		params.dst.image.extent		= resolveExtent;

		{
			const VkImageSubresourceLayers	sourceLayer	=
			{
				VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
				0u,							// uint32_t				mipLevel;
				0u,							// uint32_t				baseArrayLayer;
				1u							// uint32_t				layerCount;
			};
			const VkImageResolve			testResolve	=
			{
				sourceLayer,	// VkImageSubresourceLayers	srcSubresource;
				{0, 0, 0},		// VkOffset3D				srcOffset;
				sourceLayer,	// VkImageSubresourceLayers	dstSubresource;
				{64u, 64u, 0},		// VkOffset3D				dstOffset;
				{128u, 128u, 1u},	// VkExtent3D				extent;
			};

			CopyRegion	imageResolve;
			imageResolve.imageResolve = testResolve;
			params.regions.push_back(imageResolve);
		}

		for (int samplesIndex = 0; samplesIndex < DE_LENGTH_OF_ARRAY(samples); ++samplesIndex)
		{
			params.samples = samples[samplesIndex];
			std::ostringstream caseName;
			caseName << testName << "_" << getSampleCountCaseName(samples[samplesIndex]);
			resolveImageTests->addChild(new ResolveImageToImageTestCase(testCtx, caseName.str(), description, params));
		}
	}

	{
		const std::string	description	("Resolve from image to image");
		const std::string	testName	("with_regions");

		TestParams			params;
		params.src.image.imageType	= VK_IMAGE_TYPE_2D;
		params.src.image.format		= VK_FORMAT_R8G8B8A8_UNORM;
		params.src.image.extent		= resolveExtent;
		params.dst.image.imageType	= VK_IMAGE_TYPE_2D;
		params.dst.image.format		= VK_FORMAT_R8G8B8A8_UNORM;
		params.dst.image.extent		= resolveExtent;

		{
			const VkImageSubresourceLayers	sourceLayer	=
			{
				VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
				0u,							// uint32_t				mipLevel;
				0u,							// uint32_t				baseArrayLayer;
				1u							// uint32_t				layerCount;
			};

			for (int i = 0; i < 256; i += 64)
			{
				const VkImageResolve			testResolve	=
				{
					sourceLayer,	// VkImageSubresourceLayers	srcSubresource;
					{i, i, 0},		// VkOffset3D				srcOffset;
					sourceLayer,	// VkImageSubresourceLayers	dstSubresource;
					{i, 0, 0},		// VkOffset3D				dstOffset;
					{64u, 64u, 1u},	// VkExtent3D				extent;
				};

				CopyRegion	imageResolve;
				imageResolve.imageResolve = testResolve;
				params.regions.push_back(imageResolve);
			}
		}

		for (int samplesIndex = 0; samplesIndex < DE_LENGTH_OF_ARRAY(samples); ++samplesIndex)
		{
			params.samples = samples[samplesIndex];
			std::ostringstream caseName;
			caseName << testName << "_" << getSampleCountCaseName(samples[samplesIndex]);
			resolveImageTests->addChild(new ResolveImageToImageTestCase(testCtx, caseName.str(), description, params));
		}
	}

	{
		const std::string	description	("Resolve from image to image");
		const std::string	testName	("whole_copy_before_resolving");

		TestParams			params;
		params.src.image.imageType	= VK_IMAGE_TYPE_2D;
		params.src.image.format		= VK_FORMAT_R8G8B8A8_UNORM;
		params.src.image.extent		= defaultExtent;
		params.dst.image.imageType	= VK_IMAGE_TYPE_2D;
		params.dst.image.format		= VK_FORMAT_R8G8B8A8_UNORM;
		params.dst.image.extent		= defaultExtent;

		{
			const VkImageSubresourceLayers	sourceLayer	=
			{
				VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
				0u,							// uint32_t				mipLevel;
				0u,							// uint32_t				baseArrayLayer;
				1u							// uint32_t				layerCount;
			};

			const VkImageResolve			testResolve	=
			{
				sourceLayer,		// VkImageSubresourceLayers	srcSubresource;
				{0, 0, 0},			// VkOffset3D				srcOffset;
				sourceLayer,		// VkImageSubresourceLayers	dstSubresource;
				{0, 0, 0},			// VkOffset3D				dstOffset;
				defaultExtent,		// VkExtent3D				extent;
			};

			CopyRegion	imageResolve;
			imageResolve.imageResolve	= testResolve;
			params.regions.push_back(imageResolve);
		}

		for (int samplesIndex = 0; samplesIndex < DE_LENGTH_OF_ARRAY(samples); ++samplesIndex)
		{
			params.samples = samples[samplesIndex];
			std::ostringstream caseName;
			caseName << testName << "_" << getSampleCountCaseName(samples[samplesIndex]);
			resolveImageTests->addChild(new ResolveImageToImageTestCase(testCtx, caseName.str(), description, params, COPY_MS_IMAGE_TO_MS_IMAGE));
		}
	}

	{
		const std::string	description	("Resolve from image to image");
		const std::string	testName	("whole_array_image");

		TestParams			params;
		params.src.image.imageType		= VK_IMAGE_TYPE_2D;
		params.src.image.format			= VK_FORMAT_R8G8B8A8_UNORM;
		params.src.image.extent			= defaultExtent;
		params.dst.image.imageType		= VK_IMAGE_TYPE_2D;
		params.dst.image.format			= VK_FORMAT_R8G8B8A8_UNORM;
		params.dst.image.extent			= defaultExtent;
		params.dst.image.extent.depth	= 5u;

		for (deUint32 layerNdx=0; layerNdx < params.dst.image.extent.depth; ++layerNdx)
		{
			const VkImageSubresourceLayers	sourceLayer	=
			{
				VK_IMAGE_ASPECT_COLOR_BIT,		// VkImageAspectFlags	aspectMask;
				0u,								// uint32_t				mipLevel;
				layerNdx,						// uint32_t				baseArrayLayer;
				1u								// uint32_t				layerCount;
			};

			const VkImageResolve			testResolve	=
			{
				sourceLayer,		// VkImageSubresourceLayers	srcSubresource;
				{0, 0, 0},			// VkOffset3D				srcOffset;
				sourceLayer,		// VkImageSubresourceLayers	dstSubresource;
				{0, 0, 0},			// VkOffset3D				dstOffset;
				defaultExtent,		// VkExtent3D				extent;
			};

			CopyRegion	imageResolve;
			imageResolve.imageResolve	= testResolve;
			params.regions.push_back(imageResolve);
		}

		for (int samplesIndex = 0; samplesIndex < DE_LENGTH_OF_ARRAY(samples); ++samplesIndex)
		{
			params.samples = samples[samplesIndex];
			std::ostringstream caseName;
			caseName << testName << "_" << getSampleCountCaseName(samples[samplesIndex]);
			resolveImageTests->addChild(new ResolveImageToImageTestCase(testCtx, caseName.str(), description, params, COPY_MS_IMAGE_TO_ARRAY_MS_IMAGE));
		}
	}

	copiesAndBlittingTests->addChild(imageToImageTests.release());
	copiesAndBlittingTests->addChild(imageToBufferTests.release());
	copiesAndBlittingTests->addChild(bufferToImageTests.release());
	copiesAndBlittingTests->addChild(bufferToBufferTests.release());
	copiesAndBlittingTests->addChild(blittingImageTests.release());
	copiesAndBlittingTests->addChild(resolveImageTests.release());

	return copiesAndBlittingTests.release();
}

} // api
} // vkt
