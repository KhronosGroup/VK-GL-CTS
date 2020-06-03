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

#include "tcuImageCompare.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVectorType.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuTexLookupVerifier.hpp"

#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include <set>

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

enum AllocationKind
{
	ALLOCATION_KIND_SUBALLOCATED,
	ALLOCATION_KIND_DEDICATED,
};

template <typename Type>
class BinaryCompare
{
public:
	bool operator() (const Type& a, const Type& b) const
	{
		return deMemCmp(&a, &b, sizeof(Type)) < 0;
	}
};

typedef std::set<vk::VkFormat, BinaryCompare<vk::VkFormat> >	FormatSet;

FormatSet dedicatedAllocationImageToImageFormatsToTestSet;
FormatSet dedicatedAllocationBlittingFormatsToTestSet;

using namespace vk;

VkImageAspectFlags getAspectFlags (tcu::TextureFormat format)
{
	VkImageAspectFlags	aspectFlag	= 0;
	aspectFlag |= (tcu::hasDepthComponent(format.order)? VK_IMAGE_ASPECT_DEPTH_BIT : 0);
	aspectFlag |= (tcu::hasStencilComponent(format.order)? VK_IMAGE_ASPECT_STENCIL_BIT : 0);

	if (!aspectFlag)
		aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;

	return aspectFlag;
}

VkImageAspectFlags getAspectFlags (VkFormat format)
{
	if (isCompressedFormat(format))
		return VK_IMAGE_ASPECT_COLOR_BIT;
	else
		return getAspectFlags(mapVkFormat(format));
}

tcu::TextureFormat getSizeCompatibleTcuTextureFormat (VkFormat format)
{
	if (isCompressedFormat(format))
		return (getBlockSizeInBytes(format) == 8) ? mapVkFormat(VK_FORMAT_R16G16B16A16_UINT) : mapVkFormat(VK_FORMAT_R32G32B32A32_UINT);
	else
		return mapVkFormat(format);
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
	VkImageTiling	tiling;
	VkImageLayout	operationLayout;
};

struct TestParams
{
	union Data
	{
		struct Buffer
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

	AllocationKind	allocationKind;
	deUint32		mipLevels;
	deBool			singleCommand;
	deUint32		barrierCount;
	deBool			separateDepthStencilLayouts;

	TestParams (void)
	{
		mipLevels					= 1u;
		singleCommand				= DE_TRUE;
		barrierCount				= 1u;
		separateDepthStencilLayouts	= DE_FALSE;
	}
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
			const VkMemoryRequirements memoryRequirements = getBufferMemoryRequirements(vkd, device, buffer);

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
			const VkMemoryRequirements memoryRequirements = getImageMemoryRequirements(vkd, device, image);

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


inline deUint32 getArraySize(const ImageParms& parms)
{
	return (parms.imageType != VK_IMAGE_TYPE_3D) ? parms.extent.depth : 1u;
}

inline VkImageCreateFlags getCreateFlags(const ImageParms& parms)
{
	return parms.imageType == VK_IMAGE_TYPE_2D && parms.extent.depth % 6 == 0 ?
		VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
}

inline VkExtent3D getExtent3D(const ImageParms& parms, deUint32 mipLevel = 0u)
{
	const bool			isCompressed	= isCompressedFormat(parms.format);
	const deUint32		blockWidth		= (isCompressed) ? getBlockWidth(parms.format) : 1u;
	const deUint32		blockHeight		= (isCompressed) ? getBlockHeight(parms.format) : 1u;

	if (isCompressed && mipLevel != 0u)
		DE_FATAL("Not implemented");

	const VkExtent3D	extent			=
	{
		(parms.extent.width >> mipLevel) * blockWidth,
		(parms.imageType != VK_IMAGE_TYPE_1D) ? ((parms.extent.height >> mipLevel) * blockHeight) : 1u,
		(parms.imageType == VK_IMAGE_TYPE_3D) ? parms.extent.depth : 1u,
	};
	return extent;
}

const tcu::TextureFormat mapCombinedToDepthTransferFormat (const tcu::TextureFormat& combinedFormat)
{
	tcu::TextureFormat format;
	switch (combinedFormat.type)
	{
		case tcu::TextureFormat::UNORM_INT16:
		case tcu::TextureFormat::UNSIGNED_INT_16_8_8:
			format = tcu::TextureFormat(tcu::TextureFormat::D, tcu::TextureFormat::UNORM_INT16);
			break;
		case tcu::TextureFormat::UNSIGNED_INT_24_8_REV:
			format = tcu::TextureFormat(tcu::TextureFormat::D, tcu::TextureFormat::UNSIGNED_INT_24_8_REV);
			break;
		case tcu::TextureFormat::FLOAT_UNSIGNED_INT_24_8_REV:
		case tcu::TextureFormat::FLOAT:
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
	de::MovePtr<tcu::TextureLevel>		m_expectedTextureLevel[16];

	VkCommandBufferBeginInfo			m_cmdBufferBeginInfo;

	void								generateBuffer						(tcu::PixelBufferAccess buffer, int width, int height, int depth = 1, FillMode = FILL_MODE_GRADIENT);
	virtual void						generateExpectedResult				(void);
	void								uploadBuffer						(tcu::ConstPixelBufferAccess bufferAccess, const Allocation& bufferAlloc);
	void								uploadImage							(const tcu::ConstPixelBufferAccess& src, VkImage dst, const ImageParms& parms, const deUint32 mipLevels = 1u);
	virtual tcu::TestStatus				checkTestResult						(tcu::ConstPixelBufferAccess result);
	virtual void						copyRegionToTextureLevel			(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst, CopyRegion region, deUint32 mipLevel = 0u) = 0;
	deUint32							calculateSize						(tcu::ConstPixelBufferAccess src) const
										{
											return src.getWidth() * src.getHeight() * src.getDepth() * tcu::getPixelSize(src.getFormat());
										}

	de::MovePtr<tcu::TextureLevel>		readImage							(vk::VkImage				image,
																			 const ImageParms&			imageParms,
																			 const deUint32				mipLevel = 0u);

private:
	void								uploadImageAspect					(const tcu::ConstPixelBufferAccess&	src,
																			 const VkImage&						dst,
																			 const ImageParms&					parms,
																			 const deUint32						mipLevels = 1u);
	void								readImageAspect						(vk::VkImage						src,
																			 const tcu::PixelBufferAccess&		dst,
																			 const ImageParms&					parms,
																			 const deUint32						mipLevel = 0u);
};

CopiesAndBlittingTestInstance::CopiesAndBlittingTestInstance (Context& context, TestParams testParams)
	: vkt::TestInstance	(context)
	, m_params			(testParams)
{
	const DeviceInterface&		vk					= context.getDeviceInterface();
	const VkDevice				vkDevice			= context.getDevice();
	const deUint32				queueFamilyIndex	= context.getUniversalQueueFamilyIndex();

	// Create command pool
	m_cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);

	// Create command buffer
	m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	// Create fence
	m_fence = createFence(vk, vkDevice);
}

void CopiesAndBlittingTestInstance::generateBuffer (tcu::PixelBufferAccess buffer, int width, int height, int depth, FillMode mode)
{
	const tcu::TextureChannelClass	channelClass	= tcu::getTextureChannelClass(buffer.getFormat().type);
	tcu::Vec4						maxValue		(1.0f);

	if (buffer.getFormat().order == tcu::TextureFormat::S)
	{
		// Stencil-only is stored in the first component. Stencil is always 8 bits.
		maxValue.x() = 1 << 8;
	}
	else if (buffer.getFormat().order == tcu::TextureFormat::DS)
	{
		// In a combined format, fillWithComponentGradients expects stencil in the fourth component.
		maxValue.w() = 1 << 8;
	}
	else if (channelClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER || channelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER)
	{
		// The tcu::Vectors we use as pixels are 32-bit, so clamp to that.
		const tcu::IVec4	bits	= tcu::min(tcu::getTextureFormatBitDepth(buffer.getFormat()), tcu::IVec4(32));
		const int			signBit	= (channelClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER ? 1 : 0);

		for (int i = 0; i < 4; ++i)
		{
			if (bits[i] != 0)
				maxValue[i] = static_cast<float>((deUint64(1) << (bits[i] - signBit)) - 1);
		}
	}

	if (mode == FILL_MODE_GRADIENT)
	{
		tcu::fillWithComponentGradients(buffer, tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), maxValue);
		return;
	}

	const tcu::Vec4		redColor	(maxValue.x(),	0.0,			0.0,			maxValue.w());
	const tcu::Vec4		greenColor	(0.0,			maxValue.y(),	0.0,			maxValue.w());
	const tcu::Vec4		blueColor	(0.0,			0.0,			maxValue.z(),	maxValue.w());
	const tcu::Vec4		whiteColor	(maxValue.x(),	maxValue.y(),	maxValue.z(),	maxValue.w());

	for (int z = 0; z < depth;  ++z)
	for (int y = 0; y < height; ++y)
	for (int x = 0; x < width;  ++x)
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
					buffer.setPixDepth(redColor[0], x, y, z);
					if (tcu::hasStencilComponent(buffer.getFormat().order))
						buffer.setPixStencil((int)redColor[3], x, y, z);
				}
				else
					buffer.setPixel(redColor, x, y, z);
				break;

			case FILL_MODE_MULTISAMPLE:
			{
				float xScaled = static_cast<float>(x) / static_cast<float>(width);
				float yScaled = static_cast<float>(y) / static_cast<float>(height);
				buffer.setPixel((xScaled == yScaled) ? tcu::Vec4(0.0, 0.5, 0.5, 1.0) : ((xScaled > yScaled) ? greenColor : blueColor), x, y, z);
				break;
			}

			default:
				break;
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
	flushAlloc(vk, vkDevice, bufferAlloc);
}

void CopiesAndBlittingTestInstance::uploadImageAspect (const tcu::ConstPixelBufferAccess& imageAccess, const VkImage& image, const ImageParms& parms, const deUint32 mipLevels)
{
	const InstanceInterface&		vki					= m_context.getInstanceInterface();
	const DeviceInterface&			vk					= m_context.getDeviceInterface();
	const VkPhysicalDevice			vkPhysDevice		= m_context.getPhysicalDevice();
	const VkDevice					vkDevice			= m_context.getDevice();
	const VkQueue					queue				= m_context.getUniversalQueue();
	const deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	Allocator&						memAlloc			= m_context.getDefaultAllocator();
	Move<VkBuffer>					buffer;
	const deUint32					bufferSize			= calculateSize(imageAccess);
	de::MovePtr<Allocation>			bufferAlloc;
	const deUint32					arraySize			= getArraySize(parms);
	const VkExtent3D				imageExtent			= getExtent3D(parms);
	std::vector <VkBufferImageCopy>	copyRegions;

	// Create source buffer
	{
		const VkBufferCreateInfo	bufferParams		=
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
		bufferAlloc = allocateBuffer(vki, vk, vkPhysDevice, vkDevice, *buffer, MemoryRequirement::HostVisible, memAlloc, m_params.allocationKind);
		VK_CHECK(vk.bindBufferMemory(vkDevice, *buffer, bufferAlloc->getMemory(), bufferAlloc->getOffset()));
	}

	// Barriers for copying buffer to image
	const VkBufferMemoryBarrier		preBufferBarrier	=
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

	const VkImageAspectFlags		formatAspect		= (m_params.separateDepthStencilLayouts) ? getAspectFlags(imageAccess.getFormat()) : getAspectFlags(parms.format);
	const bool						skipPreImageBarrier	= (m_params.separateDepthStencilLayouts) ? false : ((formatAspect == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT) &&
														  getAspectFlags(imageAccess.getFormat()) == VK_IMAGE_ASPECT_STENCIL_BIT));

	const VkImageMemoryBarrier		preImageBarrier		=
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
			formatAspect,	// VkImageAspectFlags	aspect;
			0u,				// deUint32				baseMipLevel;
			mipLevels,		// deUint32				mipLevels;
			0u,				// deUint32				baseArraySlice;
			arraySize,		// deUint32				arraySize;
		}
	};

	const VkImageMemoryBarrier		postImageBarrier	=
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
			formatAspect,				// VkImageAspectFlags	aspect;
			0u,							// deUint32				baseMipLevel;
			mipLevels,					// deUint32				mipLevels;
			0u,							// deUint32				baseArraySlice;
			arraySize,					// deUint32				arraySize;
		}
	};

	for (deUint32 mipLevelNdx = 0; mipLevelNdx < mipLevels; mipLevelNdx++)
	{
		const VkExtent3D		copyExtent	=
		{
			imageExtent.width	>> mipLevelNdx,
			imageExtent.height	>> mipLevelNdx,
			imageExtent.depth
		};

		const VkBufferImageCopy	copyRegion	=
		{
			0u,												// VkDeviceSize				bufferOffset;
			copyExtent.width,								// deUint32					bufferRowLength;
			copyExtent.height,								// deUint32					bufferImageHeight;
			{
				getAspectFlags(imageAccess.getFormat()),		// VkImageAspectFlags	aspect;
				mipLevelNdx,									// deUint32				mipLevel;
				0u,												// deUint32				baseArrayLayer;
				arraySize,										// deUint32				layerCount;
			},												// VkImageSubresourceLayers	imageSubresource;
			{ 0, 0, 0 },									// VkOffset3D				imageOffset;
			copyExtent										// VkExtent3D				imageExtent;
		};

		copyRegions.push_back(copyRegion);
	}

	// Write buffer data
	deMemcpy(bufferAlloc->getHostPtr(), imageAccess.getDataPtr(), bufferSize);
	flushAlloc(vk, vkDevice, *bufferAlloc);

	// Copy buffer to image
	beginCommandBuffer(vk, *m_cmdBuffer);
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL,
						  1, &preBufferBarrier, (skipPreImageBarrier ? 0 : 1), (skipPreImageBarrier ? DE_NULL : &preImageBarrier));
	vk.cmdCopyBufferToImage(*m_cmdBuffer, *buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (deUint32)copyRegions.size(), &copyRegions[0]);
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &postImageBarrier);
	endCommandBuffer(vk, *m_cmdBuffer);

	submitCommandsAndWait(vk, vkDevice, queue, *m_cmdBuffer);
}

void CopiesAndBlittingTestInstance::uploadImage (const tcu::ConstPixelBufferAccess& src, VkImage dst, const ImageParms& parms, const deUint32 mipLevels)
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
		uploadImageAspect(src, dst, parms, mipLevels);
}

tcu::TestStatus CopiesAndBlittingTestInstance::checkTestResult (tcu::ConstPixelBufferAccess result)
{
	const tcu::ConstPixelBufferAccess	expected	= m_expectedTextureLevel[0]->getAccess();

	if (isFloatFormat(result.getFormat()))
	{
		const tcu::Vec4	threshold (0.0f);
		if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison", expected, result, threshold, tcu::COMPARE_LOG_RESULT))
			return tcu::TestStatus::fail("CopiesAndBlitting test");
	}
	else
	{
		const tcu::UVec4 threshold (0u);
		if (tcu::hasDepthComponent(result.getFormat().order) || tcu::hasStencilComponent(result.getFormat().order))
		{
			if (!tcu::dsThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison", expected, result, 0.1f, tcu::COMPARE_LOG_RESULT))
				return tcu::TestStatus::fail("CopiesAndBlitting test");
		}
		else
		{
			if (!tcu::intThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison", expected, result, threshold, tcu::COMPARE_LOG_RESULT))
				return tcu::TestStatus::fail("CopiesAndBlitting test");
		}
	}

	return tcu::TestStatus::pass("CopiesAndBlitting test");
}

void CopiesAndBlittingTestInstance::generateExpectedResult (void)
{
	const tcu::ConstPixelBufferAccess	src	= m_sourceTextureLevel->getAccess();
	const tcu::ConstPixelBufferAccess	dst	= m_destinationTextureLevel->getAccess();

	m_expectedTextureLevel[0]	= de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(dst.getFormat(), dst.getWidth(), dst.getHeight(), dst.getDepth()));
	tcu::copy(m_expectedTextureLevel[0]->getAccess(), dst);

	for (deUint32 i = 0; i < m_params.regions.size(); i++)
		copyRegionToTextureLevel(src, m_expectedTextureLevel[0]->getAccess(), m_params.regions[i]);
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
													 const ImageParms&				imageParms,
													 const deUint32					mipLevel)
{
	const InstanceInterface&	vki					= m_context.getInstanceInterface();
	const DeviceInterface&		vk					= m_context.getDeviceInterface();
	const VkPhysicalDevice		physDevice			= m_context.getPhysicalDevice();
	const VkDevice				device				= m_context.getDevice();
	const VkQueue				queue				= m_context.getUniversalQueue();
	Allocator&					allocator			= m_context.getDefaultAllocator();

	Move<VkBuffer>				buffer;
	de::MovePtr<Allocation>		bufferAlloc;
	const deUint32				queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	const VkDeviceSize			pixelDataSize		= calculateSize(dst);
	const VkExtent3D			imageExtent			= getExtent3D(imageParms, mipLevel);

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
		bufferAlloc = allocateBuffer(vki, vk, physDevice, device, *buffer, MemoryRequirement::HostVisible, allocator, m_params.allocationKind);
		VK_CHECK(vk.bindBufferMemory(device, *buffer, bufferAlloc->getMemory(), bufferAlloc->getOffset()));

		deMemset(bufferAlloc->getHostPtr(), 0, static_cast<size_t>(pixelDataSize));
		flushAlloc(vk, device, *bufferAlloc);
	}

	// Barriers for copying image to buffer
	const VkImageAspectFlags				formatAspect			= getAspectFlags(imageParms.format);
	const VkImageMemoryBarrier				imageBarrier			=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			srcAccessMask;
		VK_ACCESS_TRANSFER_READ_BIT,				// VkAccessFlags			dstAccessMask;
		imageParms.operationLayout,					// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,		// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
		image,										// VkImage					image;
		{											// VkImageSubresourceRange	subresourceRange;
			formatAspect,			// VkImageAspectFlags	aspectMask;
			mipLevel,				// deUint32				baseMipLevel;
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
		imageParms.operationLayout,					// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
		image,										// VkImage					image;
		{
			formatAspect,								// VkImageAspectFlags	aspectMask;
			mipLevel,									// deUint32				baseMipLevel;
			1u,											// deUint32				mipLevels;
			0u,											// deUint32				baseArraySlice;
			getArraySize(imageParms)					// deUint32				arraySize;
		}											// VkImageSubresourceRange	subresourceRange;
	};

	// Copy image to buffer
	const VkImageAspectFlags	aspect			= getAspectFlags(dst.getFormat());
	const VkBufferImageCopy		copyRegion		=
	{
		0u,								// VkDeviceSize				bufferOffset;
		imageExtent.width,				// deUint32					bufferRowLength;
		imageExtent.height,				// deUint32					bufferImageHeight;
		{
			aspect,							// VkImageAspectFlags		aspect;
			mipLevel,						// deUint32					mipLevel;
			0u,								// deUint32					baseArrayLayer;
			getArraySize(imageParms),		// deUint32					layerCount;
		},								// VkImageSubresourceLayers	imageSubresource;
		{ 0, 0, 0 },					// VkOffset3D				imageOffset;
		imageExtent						// VkExtent3D				imageExtent;
	};

	beginCommandBuffer(vk, *m_cmdBuffer);
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &imageBarrier);
	vk.cmdCopyImageToBuffer(*m_cmdBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *buffer, 1u, &copyRegion);
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &bufferBarrier, 1, &postImageBarrier);
	endCommandBuffer(vk, *m_cmdBuffer);

	submitCommandsAndWait(vk, device, queue, *m_cmdBuffer);

	// Read buffer data
	invalidateAlloc(vk, device, *bufferAlloc);
	tcu::copy(dst, tcu::ConstPixelBufferAccess(dst.getFormat(), dst.getSize(), bufferAlloc->getHostPtr()));
}

de::MovePtr<tcu::TextureLevel> CopiesAndBlittingTestInstance::readImage	(vk::VkImage		image,
																		 const ImageParms&	parms,
																		 const deUint32		mipLevel)
{
	const tcu::TextureFormat		imageFormat	= getSizeCompatibleTcuTextureFormat(parms.format);
	de::MovePtr<tcu::TextureLevel>	resultLevel	(new tcu::TextureLevel(imageFormat, parms.extent.width >> mipLevel, parms.extent.height >> mipLevel, parms.extent.depth));

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
		readImageAspect(image, resultLevel->getAccess(), parms, mipLevel);

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
	virtual tcu::TestStatus				checkTestResult				(tcu::ConstPixelBufferAccess result = tcu::ConstPixelBufferAccess());

private:
	Move<VkImage>						m_source;
	de::MovePtr<Allocation>				m_sourceImageAlloc;
	Move<VkImage>						m_destination;
	de::MovePtr<Allocation>				m_destinationImageAlloc;

	virtual void						copyRegionToTextureLevel	(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst, CopyRegion region, deUint32 mipLevel = 0u);
};

CopyImageToImage::CopyImageToImage (Context& context, TestParams params)
	: CopiesAndBlittingTestInstance(context, params)
{
	const InstanceInterface&	vki					= context.getInstanceInterface();
	const DeviceInterface&		vk					= context.getDeviceInterface();
	const VkPhysicalDevice		vkPhysDevice		= context.getPhysicalDevice();
	const VkDevice				vkDevice			= context.getDevice();
	const deUint32				queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	Allocator&					memAlloc			= context.getDefaultAllocator();

	// Create source image
	{
		const VkImageCreateInfo	sourceImageParams		=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,								// const void*			pNext;
			getCreateFlags(m_params.src.image),		// VkImageCreateFlags	flags;
			m_params.src.image.imageType,			// VkImageType			imageType;
			m_params.src.image.format,				// VkFormat				format;
			getExtent3D(m_params.src.image),		// VkExtent3D			extent;
			1u,										// deUint32				mipLevels;
			getArraySize(m_params.src.image),		// deUint32				arraySize;
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
		m_sourceImageAlloc		= allocateImage(vki, vk, vkPhysDevice, vkDevice, *m_source, MemoryRequirement::Any, memAlloc, m_params.allocationKind);
		VK_CHECK(vk.bindImageMemory(vkDevice, *m_source, m_sourceImageAlloc->getMemory(), m_sourceImageAlloc->getOffset()));
	}

	// Create destination image
	{
		const VkImageCreateInfo	destinationImageParams	=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,								// const void*			pNext;
			getCreateFlags(m_params.dst.image),		// VkImageCreateFlags	flags;
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
		m_destinationImageAlloc	= allocateImage(vki, vk, vkPhysDevice, vkDevice, *m_destination, MemoryRequirement::Any, memAlloc, m_params.allocationKind);
		VK_CHECK(vk.bindImageMemory(vkDevice, *m_destination, m_destinationImageAlloc->getMemory(), m_destinationImageAlloc->getOffset()));
	}
}

tcu::TestStatus CopyImageToImage::iterate (void)
{
	const bool					srcCompressed		= isCompressedFormat(m_params.src.image.format);
	const bool					dstCompressed		= isCompressedFormat(m_params.dst.image.format);

	const tcu::TextureFormat	srcTcuFormat		= getSizeCompatibleTcuTextureFormat(m_params.src.image.format);
	const tcu::TextureFormat	dstTcuFormat		= getSizeCompatibleTcuTextureFormat(m_params.dst.image.format);

	m_sourceTextureLevel = de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(srcTcuFormat,
																				(int)m_params.src.image.extent.width,
																				(int)m_params.src.image.extent.height,
																				(int)m_params.src.image.extent.depth));
	generateBuffer(m_sourceTextureLevel->getAccess(), m_params.src.image.extent.width, m_params.src.image.extent.height, m_params.src.image.extent.depth, FILL_MODE_GRADIENT);
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
	{
		VkImageCopy imageCopy = m_params.regions[i].imageCopy;

		// When copying between compressed and uncompressed formats the extent
		// members represent the texel dimensions of the source image.
		if (srcCompressed)
		{
			const deUint32	blockWidth	= getBlockWidth(m_params.src.image.format);
			const deUint32	blockHeight	= getBlockHeight(m_params.src.image.format);

			imageCopy.srcOffset.x *= blockWidth;
			imageCopy.srcOffset.y *= blockHeight;
			imageCopy.extent.width *= blockWidth;
			imageCopy.extent.height *= blockHeight;
		}

		if (dstCompressed)
		{
			const deUint32	blockWidth	= getBlockWidth(m_params.dst.image.format);
			const deUint32	blockHeight	= getBlockHeight(m_params.dst.image.format);

			imageCopy.dstOffset.x *= blockWidth;
			imageCopy.dstOffset.y *= blockHeight;
		}

		imageCopies.push_back(imageCopy);
	}

	const VkImageMemoryBarrier	imageBarriers[]		=
	{
		// source image
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			srcAccessMask;
			VK_ACCESS_TRANSFER_READ_BIT,				// VkAccessFlags			dstAccessMask;
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			oldLayout;
			m_params.src.image.operationLayout,			// VkImageLayout			newLayout;
			VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
			m_source.get(),								// VkImage					image;
			{											// VkImageSubresourceRange	subresourceRange;
				getAspectFlags(srcTcuFormat),	// VkImageAspectFlags	aspectMask;
				0u,								// deUint32				baseMipLevel;
				1u,								// deUint32				mipLevels;
				0u,								// deUint32				baseArraySlice;
				getArraySize(m_params.src.image)// deUint32				arraySize;
			}
		},
		// destination image
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			srcAccessMask;
			VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			dstAccessMask;
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			oldLayout;
			m_params.dst.image.operationLayout,			// VkImageLayout			newLayout;
			VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
			m_destination.get(),						// VkImage					image;
			{											// VkImageSubresourceRange	subresourceRange;
				getAspectFlags(dstTcuFormat),	// VkImageAspectFlags	aspectMask;
				0u,								// deUint32				baseMipLevel;
				1u,								// deUint32				mipLevels;
				0u,								// deUint32				baseArraySlice;
				getArraySize(m_params.dst.image)// deUint32				arraySize;
			}
		},
	};

	beginCommandBuffer(vk, *m_cmdBuffer);
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, DE_LENGTH_OF_ARRAY(imageBarriers), imageBarriers);
	vk.cmdCopyImage(*m_cmdBuffer, m_source.get(), m_params.src.image.operationLayout, m_destination.get(), m_params.dst.image.operationLayout, (deUint32)imageCopies.size(), imageCopies.data());
	endCommandBuffer(vk, *m_cmdBuffer);

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
			const tcu::ConstPixelBufferAccess		expectedResult		= tcu::getEffectiveDepthStencilAccess(m_expectedTextureLevel[0]->getAccess(), mode);

			if (isFloatFormat(result.getFormat()))
			{
				if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison", expectedResult, depthResult, fThreshold, tcu::COMPARE_LOG_RESULT))
					return tcu::TestStatus::fail("CopiesAndBlitting test");
			}
			else
			{
				if (!tcu::intThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison", expectedResult, depthResult, uThreshold, tcu::COMPARE_LOG_RESULT))
					return tcu::TestStatus::fail("CopiesAndBlitting test");
			}
		}

		if (tcu::hasStencilComponent(result.getFormat().order))
		{
			const tcu::Sampler::DepthStencilMode	mode				= tcu::Sampler::MODE_STENCIL;
			const tcu::ConstPixelBufferAccess		stencilResult		= tcu::getEffectiveDepthStencilAccess(result, mode);
			const tcu::ConstPixelBufferAccess		expectedResult		= tcu::getEffectiveDepthStencilAccess(m_expectedTextureLevel[0]->getAccess(), mode);

			if (isFloatFormat(result.getFormat()))
			{
				if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison", expectedResult, stencilResult, fThreshold, tcu::COMPARE_LOG_RESULT))
					return tcu::TestStatus::fail("CopiesAndBlitting test");
			}
			else
			{
				if (!tcu::intThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison", expectedResult, stencilResult, uThreshold, tcu::COMPARE_LOG_RESULT))
					return tcu::TestStatus::fail("CopiesAndBlitting test");
			}
		}
	}
	else
	{
		if (isFloatFormat(result.getFormat()))
		{
			if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison", m_expectedTextureLevel[0]->getAccess(), result, fThreshold, tcu::COMPARE_LOG_RESULT))
				return tcu::TestStatus::fail("CopiesAndBlitting test");
		}
		else if (isSnormFormat(mapTextureFormat(result.getFormat())))
		{
			// There may be an ambiguity between two possible binary representations of 1.0.
			// Get rid of that by expanding the data to floats and re-normalizing again.

			tcu::TextureLevel resultSnorm	(result.getFormat(), result.getWidth(), result.getHeight(), result.getDepth());
			{
				tcu::TextureLevel resultFloat	(tcu::TextureFormat(resultSnorm.getFormat().order, tcu::TextureFormat::FLOAT), resultSnorm.getWidth(), resultSnorm.getHeight(), resultSnorm.getDepth());

				tcu::copy(resultFloat.getAccess(), result);
				tcu::copy(resultSnorm, resultFloat.getAccess());
			}

			tcu::TextureLevel expectedSnorm	(m_expectedTextureLevel[0]->getFormat(), m_expectedTextureLevel[0]->getWidth(), m_expectedTextureLevel[0]->getHeight(), m_expectedTextureLevel[0]->getDepth());

			{
				tcu::TextureLevel expectedFloat	(tcu::TextureFormat(expectedSnorm.getFormat().order, tcu::TextureFormat::FLOAT), expectedSnorm.getWidth(), expectedSnorm.getHeight(), expectedSnorm.getDepth());

				tcu::copy(expectedFloat.getAccess(), m_expectedTextureLevel[0]->getAccess());
				tcu::copy(expectedSnorm, expectedFloat.getAccess());
			}

			if (!tcu::intThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison", expectedSnorm.getAccess(), resultSnorm.getAccess(), uThreshold, tcu::COMPARE_LOG_RESULT))
				return tcu::TestStatus::fail("CopiesAndBlitting test");
		}
		else
		{
			if (!tcu::intThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison", m_expectedTextureLevel[0]->getAccess(), result, uThreshold, tcu::COMPARE_LOG_RESULT))
				return tcu::TestStatus::fail("CopiesAndBlitting test");
		}
	}

	return tcu::TestStatus::pass("CopiesAndBlitting test");
}

void CopyImageToImage::copyRegionToTextureLevel (tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst, CopyRegion region, deUint32 mipLevel)
{
	DE_UNREF(mipLevel);

	VkOffset3D	srcOffset	= region.imageCopy.srcOffset;
	VkOffset3D	dstOffset	= region.imageCopy.dstOffset;
	VkExtent3D	extent		= region.imageCopy.extent;

	if (m_params.src.image.imageType == VK_IMAGE_TYPE_3D && m_params.dst.image.imageType == VK_IMAGE_TYPE_2D)
	{
		dstOffset.z = srcOffset.z;
		extent.depth = std::max(region.imageCopy.extent.depth, region.imageCopy.dstSubresource.layerCount);
	}
	if (m_params.src.image.imageType == VK_IMAGE_TYPE_2D && m_params.dst.image.imageType == VK_IMAGE_TYPE_3D)
	{
		srcOffset.z = dstOffset.z;
		extent.depth = std::max(region.imageCopy.extent.depth, region.imageCopy.srcSubresource.layerCount);
	}


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

	virtual void			checkSupport				(Context&						context) const
	{
		if (m_params.allocationKind == ALLOCATION_KIND_DEDICATED)
		{
			if (!context.isDeviceFunctionalitySupported("VK_KHR_dedicated_allocation"))
				TCU_THROW(NotSupportedError, "VK_KHR_dedicated_allocation is not supported");
		}

		if (m_params.separateDepthStencilLayouts)
			if (!context.isDeviceFunctionalitySupported("VK_KHR_separate_depth_stencil_layouts"))
				TCU_THROW(NotSupportedError, "VK_KHR_separate_depth_stencil_layouts is not supported");

		if ((m_params.dst.image.imageType == VK_IMAGE_TYPE_3D && m_params.src.image.imageType == VK_IMAGE_TYPE_2D) ||
			(m_params.dst.image.imageType == VK_IMAGE_TYPE_2D && m_params.src.image.imageType == VK_IMAGE_TYPE_3D))
		{
			if (!context.isDeviceFunctionalitySupported("VK_KHR_maintenance1"))
				TCU_THROW(NotSupportedError, "Extension VK_KHR_maintenance1 not supported");
		}

		const VkPhysicalDeviceLimits	limits		= context.getDeviceProperties().limits;
		VkImageFormatProperties			properties;

		if ((context.getInstanceInterface().getPhysicalDeviceImageFormatProperties (context.getPhysicalDevice(),
																					m_params.src.image.format,
																					m_params.src.image.imageType,
																					VK_IMAGE_TILING_OPTIMAL,
																					VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
																					0,
																					&properties) == VK_ERROR_FORMAT_NOT_SUPPORTED) ||
			(context.getInstanceInterface().getPhysicalDeviceImageFormatProperties (context.getPhysicalDevice(),
																					m_params.dst.image.format,
																					m_params.dst.image.imageType,
																					VK_IMAGE_TILING_OPTIMAL,
																					VK_IMAGE_USAGE_TRANSFER_DST_BIT,
																					0,
																					&properties) == VK_ERROR_FORMAT_NOT_SUPPORTED))
		{
			TCU_THROW(NotSupportedError, "Format not supported");
		}

		// Check maxImageDimension2D
		{
			if (m_params.src.image.imageType == VK_IMAGE_TYPE_2D && (m_params.src.image.extent.width > limits.maxImageDimension2D
				|| m_params.src.image.extent.height > limits.maxImageDimension2D))
			{
				TCU_THROW(NotSupportedError, "Requested 2D src image dimensions not supported");
			}

			if (m_params.dst.image.imageType == VK_IMAGE_TYPE_2D && (m_params.dst.image.extent.width > limits.maxImageDimension2D
				|| m_params.dst.image.extent.height > limits.maxImageDimension2D))
			{
				TCU_THROW(NotSupportedError, "Requested 2D dst image dimensions not supported");
			}
		}

		// Check maxImageDimension3D
		{
			if (m_params.src.image.imageType == VK_IMAGE_TYPE_3D && (m_params.src.image.extent.width > limits.maxImageDimension3D
				|| m_params.src.image.extent.height > limits.maxImageDimension3D
				|| m_params.src.image.extent.depth > limits.maxImageDimension3D))
			{
				TCU_THROW(NotSupportedError, "Requested 3D src image dimensions not supported");
			}

			if (m_params.dst.image.imageType == VK_IMAGE_TYPE_3D && (m_params.dst.image.extent.width > limits.maxImageDimension3D
				|| m_params.dst.image.extent.height > limits.maxImageDimension3D
				|| m_params.src.image.extent.depth > limits.maxImageDimension3D))
			{
				TCU_THROW(NotSupportedError, "Requested 3D dst image dimensions not supported");
			}
		}
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
	virtual void				copyRegionToTextureLevel	(tcu::ConstPixelBufferAccess, tcu::PixelBufferAccess, CopyRegion, deUint32 mipLevel = 0u);
	Move<VkBuffer>				m_source;
	de::MovePtr<Allocation>		m_sourceBufferAlloc;
	Move<VkBuffer>				m_destination;
	de::MovePtr<Allocation>		m_destinationBufferAlloc;
};

CopyBufferToBuffer::CopyBufferToBuffer (Context& context, TestParams params)
	: CopiesAndBlittingTestInstance	(context, params)
{
	const InstanceInterface&	vki					= context.getInstanceInterface();
	const DeviceInterface&		vk					= context.getDeviceInterface();
	const VkPhysicalDevice		vkPhysDevice		= context.getPhysicalDevice();
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
		m_sourceBufferAlloc		= allocateBuffer(vki, vk, vkPhysDevice, vkDevice, *m_source, MemoryRequirement::HostVisible, memAlloc, m_params.allocationKind);
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
		m_destinationBufferAlloc	= allocateBuffer(vki, vk, vkPhysDevice, vkDevice, *m_destination, MemoryRequirement::HostVisible, memAlloc, m_params.allocationKind);
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

	beginCommandBuffer(vk, *m_cmdBuffer);
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &srcBufferBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
	vk.cmdCopyBuffer(*m_cmdBuffer, m_source.get(), m_destination.get(), (deUint32)m_params.regions.size(), &bufferCopies[0]);
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &dstBufferBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
	endCommandBuffer(vk, *m_cmdBuffer);
	submitCommandsAndWait(vk, vkDevice, queue, *m_cmdBuffer);

	// Read buffer data
	de::MovePtr<tcu::TextureLevel>	resultLevel		(new tcu::TextureLevel(mapVkFormat(VK_FORMAT_R32_UINT), dstLevelWidth, 1));
	invalidateAlloc(vk, vkDevice, *m_destinationBufferAlloc);
	tcu::copy(*resultLevel, tcu::ConstPixelBufferAccess(resultLevel->getFormat(), resultLevel->getSize(), m_destinationBufferAlloc->getHostPtr()));

	return checkTestResult(resultLevel->getAccess());
}

void CopyBufferToBuffer::copyRegionToTextureLevel (tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst, CopyRegion region, deUint32 mipLevel)
{
	DE_UNREF(mipLevel);

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
	virtual void				copyRegionToTextureLevel	(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst, CopyRegion region, deUint32 mipLevel = 0u);

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
	const InstanceInterface&	vki					= context.getInstanceInterface();
	const DeviceInterface&		vk					= context.getDeviceInterface();
	const VkPhysicalDevice		vkPhysDevice		= context.getPhysicalDevice();
	const VkDevice				vkDevice			= context.getDevice();
	const deUint32				queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	Allocator&					memAlloc			= context.getDefaultAllocator();

	// Create source image
	{
		const VkImageCreateInfo		sourceImageParams		=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,								// const void*			pNext;
			getCreateFlags(m_params.src.image),		// VkImageCreateFlags	flags;
			m_params.src.image.imageType,			// VkImageType			imageType;
			m_params.src.image.format,				// VkFormat				format;
			getExtent3D(m_params.src.image),		// VkExtent3D			extent;
			1u,										// deUint32				mipLevels;
			getArraySize(m_params.src.image),		// deUint32				arraySize;
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
		m_sourceImageAlloc	= allocateImage(vki, vk, vkPhysDevice, vkDevice, *m_source, MemoryRequirement::Any, memAlloc, m_params.allocationKind);
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
		m_destinationBufferAlloc	= allocateBuffer(vki, vk, vkPhysDevice, vkDevice, *m_destination, MemoryRequirement::HostVisible, memAlloc, m_params.allocationKind);
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

	beginCommandBuffer(vk, *m_cmdBuffer);
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &imageBarrier);
	vk.cmdCopyImageToBuffer(*m_cmdBuffer, m_source.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_destination.get(), (deUint32)m_params.regions.size(), &bufferImageCopies[0]);
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &bufferBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
	endCommandBuffer(vk, *m_cmdBuffer);

	submitCommandsAndWait (vk, vkDevice, queue, *m_cmdBuffer);

	// Read buffer data
	de::MovePtr<tcu::TextureLevel>	resultLevel		(new tcu::TextureLevel(m_textureFormat, (int)m_params.dst.buffer.size, 1));
	invalidateAlloc(vk, vkDevice, *m_destinationBufferAlloc);
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

void CopyImageToBuffer::copyRegionToTextureLevel (tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst, CopyRegion region, deUint32 mipLevel)
{
	DE_UNREF(mipLevel);

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
	virtual void				copyRegionToTextureLevel	(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst, CopyRegion region, deUint32 mipLevel = 0u);

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
	const InstanceInterface&	vki					= context.getInstanceInterface();
	const DeviceInterface&		vk					= context.getDeviceInterface();
	const VkPhysicalDevice		vkPhysDevice		= context.getPhysicalDevice();
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
		m_sourceBufferAlloc		= allocateBuffer(vki, vk, vkPhysDevice, vkDevice, *m_source, MemoryRequirement::HostVisible, memAlloc, m_params.allocationKind);
		VK_CHECK(vk.bindBufferMemory(vkDevice, *m_source, m_sourceBufferAlloc->getMemory(), m_sourceBufferAlloc->getOffset()));
	}

	// Create destination image
	{
		const VkImageCreateInfo		destinationImageParams	=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,								// const void*			pNext;
			getCreateFlags(m_params.dst.image),		// VkImageCreateFlags	flags;
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
		m_destinationImageAlloc	= allocateImage(vki, vk, vkPhysDevice, vkDevice, *m_destination, MemoryRequirement::Any, memAlloc, m_params.allocationKind);
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

	beginCommandBuffer(vk, *m_cmdBuffer);
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &imageBarrier);
	vk.cmdCopyBufferToImage(*m_cmdBuffer, m_source.get(), m_destination.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (deUint32)m_params.regions.size(), bufferImageCopies.data());
	endCommandBuffer(vk, *m_cmdBuffer);

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

void CopyBufferToImage::copyRegionToTextureLevel (tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst, CopyRegion region, deUint32 mipLevel)
{
	DE_UNREF(mipLevel);

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

class CopyBufferToDepthStencil : public CopiesAndBlittingTestInstance
{
public:
								CopyBufferToDepthStencil	(Context& context,
															 TestParams	testParams);
	virtual tcu::TestStatus		iterate						(void);
private:
	virtual void				copyRegionToTextureLevel	(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst, CopyRegion region, deUint32 mipLevel = 0u);

	tcu::TextureFormat			m_textureFormat;
	VkDeviceSize				m_bufferSize;

	Move<VkBuffer>				m_source;
	de::MovePtr<Allocation>		m_sourceBufferAlloc;
	Move<VkImage>				m_destination;
	de::MovePtr<Allocation>		m_destinationImageAlloc;
};

void CopyBufferToDepthStencil::copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst, CopyRegion region, deUint32 mipLevel)
{
	DE_UNREF(mipLevel);

	deUint32			rowLength	= region.bufferImageCopy.bufferRowLength;
	if (!rowLength)
		rowLength = region.bufferImageCopy.imageExtent.width;

	deUint32			imageHeight = region.bufferImageCopy.bufferImageHeight;
	if (!imageHeight)
		imageHeight = region.bufferImageCopy.imageExtent.height;

	const int			texelSize	= dst.getFormat().getPixelSize();
	const VkExtent3D	extent		= region.bufferImageCopy.imageExtent;
	const VkOffset3D	dstOffset	= region.bufferImageCopy.imageOffset;
	const int			texelOffset = (int)region.bufferImageCopy.bufferOffset / texelSize;

	for (deUint32 z = 0; z < extent.depth; z++)
	{
		for (deUint32 y = 0; y < extent.height; y++)
		{
			int									texelIndex = texelOffset + (z * imageHeight + y) * rowLength;
			const tcu::ConstPixelBufferAccess	srcSubRegion = tcu::getSubregion(src, texelIndex, 0, region.bufferImageCopy.imageExtent.width, 1);
			const tcu::PixelBufferAccess		dstSubRegion = tcu::getSubregion(dst, dstOffset.x, dstOffset.y + y, dstOffset.z + z,
				region.bufferImageCopy.imageExtent.width, 1, 1);

			if (region.bufferImageCopy.imageSubresource.aspectMask == VK_IMAGE_ASPECT_DEPTH_BIT)
			{
				tcu::copy(dstSubRegion, tcu::getEffectiveDepthStencilAccess(srcSubRegion, tcu::Sampler::MODE_DEPTH), DE_FALSE);
			}
			else
			{
				tcu::copy(dstSubRegion, tcu::getEffectiveDepthStencilAccess(srcSubRegion, tcu::Sampler::MODE_STENCIL), DE_FALSE);
			}
		}
	}
}

bool isSupportedDepthStencilFormat(const InstanceInterface& vki, const VkPhysicalDevice physDevice, const VkFormat format)
{
	VkFormatProperties formatProps;
	vki.getPhysicalDeviceFormatProperties(physDevice, format, &formatProps);
	return (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0;
}

CopyBufferToDepthStencil::CopyBufferToDepthStencil(Context& context, TestParams testParams)
	: CopiesAndBlittingTestInstance(context, testParams)
	, m_textureFormat(mapVkFormat(testParams.dst.image.format))
	, m_bufferSize(0)
{
	const InstanceInterface&	vki					= context.getInstanceInterface();
	const DeviceInterface&		vk					= context.getDeviceInterface();
	const VkPhysicalDevice		vkPhysDevice		= context.getPhysicalDevice();
	const VkDevice				vkDevice			= context.getDevice();
	const deUint32				queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	Allocator&					memAlloc			= context.getDefaultAllocator();
	const bool					hasDepth			= tcu::hasDepthComponent(mapVkFormat(m_params.dst.image.format).order);
	const bool					hasStencil			= tcu::hasStencilComponent(mapVkFormat(m_params.dst.image.format).order);

	if (!isSupportedDepthStencilFormat(vki, vkPhysDevice, testParams.dst.image.format))
	{
		TCU_THROW(NotSupportedError, "Image format not supported.");
	}

	if (hasDepth)
	{
		glw::GLuint texelSize = m_textureFormat.getPixelSize();
		if (texelSize > sizeof(float))
		{
			// We must have D32F_S8 format, depth must be packed so we only need
			// to allocate space for the D32F part. Stencil will be separate
			texelSize = sizeof(float);
		}
		m_bufferSize += static_cast<VkDeviceSize>(m_params.dst.image.extent.width) * static_cast<VkDeviceSize>(m_params.dst.image.extent.height) * static_cast<VkDeviceSize>(texelSize);
	}
	if (hasStencil)
	{
		// Stencil is always 8bits and packed.
		m_bufferSize += static_cast<VkDeviceSize>(m_params.dst.image.extent.width) * static_cast<VkDeviceSize>(m_params.dst.image.extent.height);
	}

	// Create source buffer, this is where the depth & stencil data will go that's used by test's regions.
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
		m_sourceBufferAlloc		= allocateBuffer(vki, vk, vkPhysDevice, vkDevice, *m_source, MemoryRequirement::HostVisible, memAlloc, m_params.allocationKind);
		VK_CHECK(vk.bindBufferMemory(vkDevice, *m_source, m_sourceBufferAlloc->getMemory(), m_sourceBufferAlloc->getOffset()));
	}

	// Create destination image
	{
		const VkImageCreateInfo		destinationImageParams	=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,								// const void*			pNext;
			getCreateFlags(m_params.dst.image),		// VkImageCreateFlags	flags;
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

		m_destination				= createImage(vk, vkDevice, &destinationImageParams);
		m_destinationImageAlloc		= allocateImage(vki, vk, vkPhysDevice, vkDevice, *m_destination, MemoryRequirement::Any, memAlloc, m_params.allocationKind);
		VK_CHECK(vk.bindImageMemory(vkDevice, *m_destination, m_destinationImageAlloc->getMemory(), m_destinationImageAlloc->getOffset()));
	}
}

tcu::TestStatus CopyBufferToDepthStencil::iterate(void)
{
	// Create source depth/stencil content. Treat as 1D texture to get different pattern
	m_sourceTextureLevel = de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(m_textureFormat, (int)m_params.src.buffer.size, 1));
	// Fill buffer with linear gradiant
	generateBuffer(m_sourceTextureLevel->getAccess(), (int)m_params.src.buffer.size, 1, 1);

	// Create image layer for depth/stencil
	m_destinationTextureLevel = de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(m_textureFormat,
		m_params.dst.image.extent.width,
		m_params.dst.image.extent.height,
		m_params.dst.image.extent.depth));

	// Fill image layer with 2D gradiant
	generateBuffer(m_destinationTextureLevel->getAccess(), m_params.dst.image.extent.width, m_params.dst.image.extent.height, m_params.dst.image.extent.depth);

	// Fill m_extendedTextureLevel with copy of m_destinationTextureLevel
	// Then iterate over each of the regions given in m_params.regions and copy m_sourceTextureLevel content to m_extendedTextureLevel
	// This emulates what the HW will be doing.
	generateExpectedResult();

	// Upload our source depth/stencil content to the source buffer
	// This is the buffer that will be used by region commands
	std::vector<VkBufferImageCopy>	bufferImageCopies;
	VkDeviceSize					bufferOffset	= 0;
	const VkDevice					vkDevice		= m_context.getDevice();
	const DeviceInterface&			vk				= m_context.getDeviceInterface();
	const VkQueue					queue			= m_context.getUniversalQueue();
	char*							dstPtr			= reinterpret_cast<char*>(m_sourceBufferAlloc->getHostPtr());
	bool							depthLoaded		= DE_FALSE;
	bool							stencilLoaded	= DE_FALSE;
	VkDeviceSize					depthOffset		= 0;
	VkDeviceSize					stencilOffset	= 0;

	// To be able to test ordering depth & stencil differently
	// We take the given copy regions and use that as the desired order
	// and copy the appropriate data into place and compute the appropriate
	// data offsets to be used in the copy command.
	for (deUint32 i = 0; i < m_params.regions.size(); i++)
	{
		tcu::ConstPixelBufferAccess bufferAccess	= m_sourceTextureLevel->getAccess();
		deUint32					bufferSize		= bufferAccess.getWidth() * bufferAccess.getHeight() * bufferAccess.getDepth();
		VkBufferImageCopy			copyData		= m_params.regions[i].bufferImageCopy;
		char*						srcPtr;

		if (copyData.imageSubresource.aspectMask == VK_IMAGE_ASPECT_DEPTH_BIT && !depthLoaded)
		{
			if (!depthLoaded)
			{
				// Create level that is same component as depth buffer (e.g. D16, D24, D32F)
				tcu::TextureLevel	depthTexture(mapCombinedToDepthTransferFormat(bufferAccess.getFormat()), bufferAccess.getWidth(), bufferAccess.getHeight(), bufferAccess.getDepth());
				bufferSize *= tcu::getPixelSize(depthTexture.getFormat());
				// Copy depth component only from source data. This gives us packed depth-only data.
				tcu::copy(depthTexture.getAccess(), tcu::getEffectiveDepthStencilAccess(bufferAccess, tcu::Sampler::MODE_DEPTH));
				srcPtr = (char*)depthTexture.getAccess().getDataPtr();
				// Copy packed depth-only data to output buffer
				deMemcpy(dstPtr, srcPtr, bufferSize);
				depthLoaded = DE_TRUE;
				depthOffset = bufferOffset;
				dstPtr += bufferSize;
				bufferOffset += bufferSize;
			}
			copyData.bufferOffset += depthOffset;
		}
		else if (!stencilLoaded)
		{
			if (!stencilLoaded)
			{
				// Create level that is same component as stencil buffer (always 8-bits)
				tcu::TextureLevel	stencilTexture(tcu::getEffectiveDepthStencilTextureFormat(bufferAccess.getFormat(), tcu::Sampler::MODE_STENCIL), bufferAccess.getWidth(), bufferAccess.getHeight(), bufferAccess.getDepth());
				// Copy stencil component only from source data. This gives us packed stencil-only data.
				tcu::copy(stencilTexture.getAccess(), tcu::getEffectiveDepthStencilAccess(bufferAccess, tcu::Sampler::MODE_STENCIL));
				srcPtr = (char*)stencilTexture.getAccess().getDataPtr();
				// Copy packed stencil-only data to output buffer
				deMemcpy(dstPtr, srcPtr, bufferSize);
				stencilLoaded = DE_TRUE;
				stencilOffset = bufferOffset;
				dstPtr += bufferSize;
				bufferOffset += bufferSize;
			}
			copyData.bufferOffset += stencilOffset;
		}

		bufferImageCopies.push_back(copyData);
	}

	flushAlloc(vk, vkDevice, *m_sourceBufferAlloc);

	// Upload the depth/stencil data from m_destinationTextureLevel to initialize
	// depth and stencil to known values.
	// Uses uploadImageAspect so makes its own buffers for depth and stencil
	// aspects (as needed) and copies them with independent vkCmdCopyBufferToImage commands.
	uploadImage(m_destinationTextureLevel->getAccess(), *m_destination, m_params.dst.image);

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

	// Copy from buffer to depth/stencil image

	beginCommandBuffer(vk, *m_cmdBuffer);
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &imageBarrier);

	if (m_params.singleCommand)
	{
		// Issue a single copy command with regions defined by the test.
		vk.cmdCopyBufferToImage(*m_cmdBuffer, m_source.get(), m_destination.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (deUint32)m_params.regions.size(), bufferImageCopies.data());
	}
	else
	{
		// Issue a a copy command per region defined by the test.
		for (deUint32 i = 0; i < bufferImageCopies.size(); i++)
		{
			vk.cmdCopyBufferToImage(*m_cmdBuffer, m_source.get(), m_destination.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferImageCopies[i]);
		}
	}
	endCommandBuffer(vk, *m_cmdBuffer);

	submitCommandsAndWait(vk, vkDevice, queue, *m_cmdBuffer);

	de::MovePtr<tcu::TextureLevel>	resultLevel = readImage(*m_destination, m_params.dst.image);

	// For combined depth/stencil formats both aspects are checked even when the test only
	// copies one. Clear such aspects here for both the result and the reference.
	if (tcu::hasDepthComponent(m_textureFormat.order) && !depthLoaded)
	{
		tcu::clearDepth(m_expectedTextureLevel[0]->getAccess(), 0.0f);
		tcu::clearDepth(resultLevel->getAccess(), 0.0f);
	}
	if (tcu::hasStencilComponent(m_textureFormat.order) && !stencilLoaded)
	{
		tcu::clearStencil(m_expectedTextureLevel[0]->getAccess(), 0);
		tcu::clearStencil(resultLevel->getAccess(), 0);
	}

	return checkTestResult(resultLevel->getAccess());
}

class CopyBufferToDepthStencilTestCase : public vkt::TestCase
{
public:
							CopyBufferToDepthStencilTestCase	(tcu::TestContext&		testCtx,
																 const std::string&		name,
																 const std::string&		description,
																 const TestParams		params)
								: vkt::TestCase(testCtx, name, description)
								, m_params(params)
							{}

	virtual					~CopyBufferToDepthStencilTestCase	(void) {}

	virtual TestInstance*	createInstance						(Context&				context) const
							{
								return new CopyBufferToDepthStencil(context, m_params);
							}
private:
	TestParams				m_params;
};

// Copy from image to image with scaling.

class BlittingImages : public CopiesAndBlittingTestInstance
{
public:
										BlittingImages					(Context&	context,
																		 TestParams params);
	virtual tcu::TestStatus				iterate							(void);
protected:
	virtual tcu::TestStatus				checkTestResult					(tcu::ConstPixelBufferAccess result);
	virtual void						copyRegionToTextureLevel		(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst, CopyRegion region, deUint32 mipLevel = 0u);
	virtual void						generateExpectedResult			(void);
private:
	bool								checkLinearFilteredResult		(const tcu::ConstPixelBufferAccess&	result,
																		 const tcu::ConstPixelBufferAccess&	clampedReference,
																		 const tcu::ConstPixelBufferAccess&	unclampedReference,
																		 const tcu::TextureFormat&			sourceFormat);
	bool								checkNearestFilteredResult		(const tcu::ConstPixelBufferAccess&	result,
																		 const tcu::ConstPixelBufferAccess& source);

	Move<VkImage>						m_source;
	de::MovePtr<Allocation>				m_sourceImageAlloc;
	Move<VkImage>						m_destination;
	de::MovePtr<Allocation>				m_destinationImageAlloc;

	de::MovePtr<tcu::TextureLevel>		m_unclampedExpectedTextureLevel;
};

BlittingImages::BlittingImages (Context& context, TestParams params)
	: CopiesAndBlittingTestInstance(context, params)
{
	const InstanceInterface&	vki					= context.getInstanceInterface();
	const DeviceInterface&		vk					= context.getDeviceInterface();
	const VkPhysicalDevice		vkPhysDevice		= context.getPhysicalDevice();
	const VkDevice				vkDevice			= context.getDevice();
	const deUint32				queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	Allocator&					memAlloc			= context.getDefaultAllocator();

	// Create source image
	{
		const VkImageCreateInfo		sourceImageParams		=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,								// const void*			pNext;
			getCreateFlags(m_params.src.image),		// VkImageCreateFlags	flags;
			m_params.src.image.imageType,			// VkImageType			imageType;
			m_params.src.image.format,				// VkFormat				format;
			getExtent3D(m_params.src.image),		// VkExtent3D			extent;
			1u,										// deUint32				mipLevels;
			getArraySize(m_params.src.image),		// deUint32				arraySize;
			VK_SAMPLE_COUNT_1_BIT,					// deUint32				samples;
			m_params.src.image.tiling,				// VkImageTiling		tiling;
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
				VK_IMAGE_USAGE_TRANSFER_DST_BIT,	// VkImageUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
			1u,										// deUint32				queueFamilyCount;
			&queueFamilyIndex,						// const deUint32*		pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout		initialLayout;
		};

		m_source = createImage(vk, vkDevice, &sourceImageParams);
		m_sourceImageAlloc = allocateImage(vki, vk, vkPhysDevice, vkDevice, *m_source, MemoryRequirement::Any, memAlloc, m_params.allocationKind);
		VK_CHECK(vk.bindImageMemory(vkDevice, *m_source, m_sourceImageAlloc->getMemory(), m_sourceImageAlloc->getOffset()));
	}

	// Create destination image
	{
		const VkImageCreateInfo		destinationImageParams	=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,								// const void*			pNext;
			getCreateFlags(m_params.dst.image),		// VkImageCreateFlags	flags;
			m_params.dst.image.imageType,			// VkImageType			imageType;
			m_params.dst.image.format,				// VkFormat				format;
			getExtent3D(m_params.dst.image),		// VkExtent3D			extent;
			1u,										// deUint32				mipLevels;
			getArraySize(m_params.dst.image),		// deUint32				arraySize;
			VK_SAMPLE_COUNT_1_BIT,					// deUint32				samples;
			m_params.dst.image.tiling,				// VkImageTiling		tiling;
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
				VK_IMAGE_USAGE_TRANSFER_DST_BIT,	// VkImageUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
			1u,										// deUint32				queueFamilyCount;
			&queueFamilyIndex,						// const deUint32*		pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout		initialLayout;
		};

		m_destination = createImage(vk, vkDevice, &destinationImageParams);
		m_destinationImageAlloc = allocateImage(vki, vk, vkPhysDevice, vkDevice, *m_destination, MemoryRequirement::Any, memAlloc, m_params.allocationKind);
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
		m_params.src.image.operationLayout,			// VkImageLayout			newLayout;
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
		m_params.dst.image.operationLayout,			// VkImageLayout			newLayout;
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

	beginCommandBuffer(vk, *m_cmdBuffer);
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &srcImageBarrier);
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &dstImageBarrier);
	vk.cmdBlitImage(*m_cmdBuffer, m_source.get(), m_params.src.image.operationLayout, m_destination.get(), m_params.dst.image.operationLayout, (deUint32)m_params.regions.size(), &regions[0], m_params.filter);
	endCommandBuffer(vk, *m_cmdBuffer);
	submitCommandsAndWait(vk, vkDevice, queue, *m_cmdBuffer);

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

	case tcu::TextureFormat::UNORM_INT_1010102_REV:
		threshold = tcu::Vec4(0.002f, 0.002f, 0.002f, 0.3f);
		break;

	case tcu:: TextureFormat::UNORM_INT8:
		threshold = tcu::Vec4(0.008f, 0.008f, 0.008f, 0.008f);
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

bool BlittingImages::checkLinearFilteredResult (const tcu::ConstPixelBufferAccess&	result,
												const tcu::ConstPixelBufferAccess&	clampedExpected,
												const tcu::ConstPixelBufferAccess&	unclampedExpected,
												const tcu::TextureFormat&			srcFormat)
{
	tcu::TestLog&					log				(m_context.getTestContext().getLog());
	const tcu::TextureFormat		dstFormat		= result.getFormat();
	const tcu::TextureChannelClass	dstChannelClass = tcu::getTextureChannelClass(dstFormat.type);
	const tcu::TextureChannelClass	srcChannelClass = tcu::getTextureChannelClass(srcFormat.type);
	bool							isOk			= false;

	log << tcu::TestLog::Section("ClampedSourceImage", "Region with clamped edges on source image.");

	// if either of srcImage or dstImage was created with a signed/unsigned integer VkFormat,
	// the other must also have been created with a signed/unsigned integer VkFormat
	bool dstImageIsIntClass = dstChannelClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER ||
							  dstChannelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER;
	bool srcImageIsIntClass = srcChannelClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER ||
							  srcChannelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER;
	if (dstImageIsIntClass != srcImageIsIntClass)
	{
		log << tcu::TestLog::EndSection;
		return false;
	}

	if (isFloatFormat(dstFormat))
	{
		const bool		srcIsSRGB	= tcu::isSRGB(srcFormat);
		const tcu::Vec4	srcMaxDiff	= getFormatThreshold(srcFormat) * tcu::Vec4(srcIsSRGB ? 2.0f : 1.0f);
		const tcu::Vec4	dstMaxDiff	= getFormatThreshold(dstFormat);
		const tcu::Vec4	threshold	= tcu::max(srcMaxDiff, dstMaxDiff);

		isOk = tcu::floatThresholdCompare(log, "Compare", "Result comparsion", clampedExpected, result, threshold, tcu::COMPARE_LOG_RESULT);
		log << tcu::TestLog::EndSection;

		if (!isOk)
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
		log << tcu::TestLog::EndSection;

		if (!isOk)
		{
			log << tcu::TestLog::Section("NonClampedSourceImage", "Region with non-clamped edges on source image.");
			isOk = tcu::intThresholdCompare(log, "Compare", "Result comparsion", unclampedExpected, result, threshold, tcu::COMPARE_LOG_RESULT);
			log << tcu::TestLog::EndSection;
		}
	}

	return isOk;
}

//! Utility to encapsulate coordinate computation and loops.
struct CompareEachPixelInEachRegion
{
	virtual		 ~CompareEachPixelInEachRegion  (void) {}
	virtual bool compare								(const void* pUserData, const int x, const int y, const int z, const tcu::Vec3& srcNormCoord) const = 0;

	bool forEach (const void*							pUserData,
				  const std::vector<CopyRegion>&		regions,
				  const int								sourceWidth,
				  const int								sourceHeight,
				  const int								sourceDepth,
				  const tcu::PixelBufferAccess&			errorMask) const
	{
		bool compareOk = true;

		for (std::vector<CopyRegion>::const_iterator regionIter = regions.begin(); regionIter != regions.end(); ++regionIter)
		{
			const VkImageBlit& blit = regionIter->imageBlit;

			const int	xStart	= deMin32(blit.dstOffsets[0].x, blit.dstOffsets[1].x);
			const int	yStart	= deMin32(blit.dstOffsets[0].y, blit.dstOffsets[1].y);
			const int	zStart	= deMin32(blit.dstOffsets[0].z, blit.dstOffsets[1].z);
			const int	xEnd	= deMax32(blit.dstOffsets[0].x, blit.dstOffsets[1].x);
			const int	yEnd	= deMax32(blit.dstOffsets[0].y, blit.dstOffsets[1].y);
			const int	zEnd	= deMax32(blit.dstOffsets[0].z, blit.dstOffsets[1].z);
			const float	xScale	= static_cast<float>(blit.srcOffsets[1].x - blit.srcOffsets[0].x) / static_cast<float>(blit.dstOffsets[1].x - blit.dstOffsets[0].x);
			const float	yScale	= static_cast<float>(blit.srcOffsets[1].y - blit.srcOffsets[0].y) / static_cast<float>(blit.dstOffsets[1].y - blit.dstOffsets[0].y);
			const float	zScale	= static_cast<float>(blit.srcOffsets[1].z - blit.srcOffsets[0].z) / static_cast<float>(blit.dstOffsets[1].z - blit.dstOffsets[0].z);
			const float srcInvW	= 1.0f / static_cast<float>(sourceWidth);
			const float srcInvH	= 1.0f / static_cast<float>(sourceHeight);
			const float srcInvD	= 1.0f / static_cast<float>(sourceDepth);

			for (int z = zStart; z < zEnd; z++)
			for (int y = yStart; y < yEnd; y++)
			for (int x = xStart; x < xEnd; x++)
			{
				const tcu::Vec3 srcNormCoord
				(
					(xScale * (static_cast<float>(x - blit.dstOffsets[0].x) + 0.5f) + static_cast<float>(blit.srcOffsets[0].x)) * srcInvW,
					(yScale * (static_cast<float>(y - blit.dstOffsets[0].y) + 0.5f) + static_cast<float>(blit.srcOffsets[0].y)) * srcInvH,
					(zScale * (static_cast<float>(z - blit.dstOffsets[0].z) + 0.5f) + static_cast<float>(blit.srcOffsets[0].z)) * srcInvD
				);

				if (!compare(pUserData, x, y, z, srcNormCoord))
				{
					errorMask.setPixel(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f), x, y, z);
					compareOk = false;
				}
			}
		}
		return compareOk;
	}
};

tcu::Vec4 getFloatOrFixedPointFormatThreshold (const tcu::TextureFormat& format)
{
	const tcu::TextureChannelClass	channelClass	= tcu::getTextureChannelClass(format.type);
	const tcu::IVec4				bitDepth		= tcu::getTextureFormatBitDepth(format);

	if (channelClass == tcu::TEXTURECHANNELCLASS_FLOATING_POINT)
	{
		return getFormatThreshold(format);
	}
	else if (channelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT ||
			 channelClass == tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT)
	{
		const bool	isSigned	= (channelClass == tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT);
		const float	range		= isSigned ? 1.0f - (-1.0f)
										   : 1.0f -   0.0f;

		tcu::Vec4 v;
		for (int i = 0; i < 4; ++i)
		{
			if (bitDepth[i] == 0)
				v[i] = 1.0f;
			else
				v[i] = range / static_cast<float>((1 << bitDepth[i]) - 1);
		}
		return v;
	}
	else
	{
		DE_ASSERT(0);
		return tcu::Vec4();
	}
}

bool floatNearestBlitCompare (const tcu::ConstPixelBufferAccess&	source,
							  const tcu::ConstPixelBufferAccess&	result,
							  const tcu::PixelBufferAccess&			errorMask,
							  const std::vector<CopyRegion>&		regions)
{
	const tcu::Sampler		sampler		(tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::NEAREST, tcu::Sampler::NEAREST);
	tcu::LookupPrecision	precision;

	{
		const tcu::IVec4	dstBitDepth	= tcu::getTextureFormatBitDepth(result.getFormat());
		const tcu::Vec4		srcMaxDiff	= getFloatOrFixedPointFormatThreshold(source.getFormat());
		const tcu::Vec4		dstMaxDiff	= getFloatOrFixedPointFormatThreshold(result.getFormat());

		precision.colorMask		 = tcu::notEqual(dstBitDepth, tcu::IVec4(0));
		precision.colorThreshold = tcu::max(srcMaxDiff, dstMaxDiff);
	}

	const struct Capture
	{
		const tcu::ConstPixelBufferAccess&	source;
		const tcu::ConstPixelBufferAccess&	result;
		const tcu::Sampler&					sampler;
		const tcu::LookupPrecision&			precision;
		const bool							isSRGB;
	} capture =
	{
		source, result, sampler, precision, tcu::isSRGB(result.getFormat())
	};

	const struct Loop : CompareEachPixelInEachRegion
	{
		Loop (void) {}

		bool compare (const void* pUserData, const int x, const int y, const int z, const tcu::Vec3& srcNormCoord) const
		{
			const Capture&					c					= *static_cast<const Capture*>(pUserData);
			const tcu::TexLookupScaleMode	lookupScaleDontCare	= tcu::TEX_LOOKUP_SCALE_MINIFY;
			tcu::Vec4						dstColor			= c.result.getPixel(x, y, z);

			// TexLookupVerifier performs a conversion to linear space, so we have to as well
			if (c.isSRGB)
				dstColor = tcu::sRGBToLinear(dstColor);

			return tcu::isLevel3DLookupResultValid(c.source, c.sampler, lookupScaleDontCare, c.precision, srcNormCoord, dstColor);
		}
	} loop;

	return loop.forEach(&capture, regions, source.getWidth(), source.getHeight(), source.getDepth(), errorMask);
}

bool intNearestBlitCompare (const tcu::ConstPixelBufferAccess&	source,
							const tcu::ConstPixelBufferAccess&	result,
							const tcu::PixelBufferAccess&		errorMask,
							const std::vector<CopyRegion>&		regions)
{
	const tcu::Sampler		sampler		(tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::NEAREST, tcu::Sampler::NEAREST);
	tcu::IntLookupPrecision	precision;

	{
		const tcu::IVec4	srcBitDepth	= tcu::getTextureFormatBitDepth(source.getFormat());
		const tcu::IVec4	dstBitDepth	= tcu::getTextureFormatBitDepth(result.getFormat());

		for (deUint32 i = 0; i < 4; ++i) {
			precision.colorThreshold[i]	= de::max(de::max(srcBitDepth[i] / 8, dstBitDepth[i] / 8), 1);
			precision.colorMask[i]		= dstBitDepth[i] != 0;
		}
	}

	// Prepare a source image with a matching (converted) pixel format. Ideally, we would've used a wrapper that
	// does the conversion on the fly without wasting memory, but this approach is more straightforward.
	tcu::TextureLevel				convertedSourceTexture	(result.getFormat(), source.getWidth(), source.getHeight(), source.getDepth());
	const tcu::PixelBufferAccess	convertedSource			= convertedSourceTexture.getAccess();

	for (int z = 0; z < source.getDepth();	++z)
	for (int y = 0; y < source.getHeight(); ++y)
	for (int x = 0; x < source.getWidth();  ++x)
		convertedSource.setPixel(source.getPixelInt(x, y, z), x, y, z);	// will be clamped to max. representable value

	const struct Capture
	{
		const tcu::ConstPixelBufferAccess&	source;
		const tcu::ConstPixelBufferAccess&	result;
		const tcu::Sampler&					sampler;
		const tcu::IntLookupPrecision&		precision;
	} capture =
	{
		convertedSource, result, sampler, precision
	};

	const struct Loop : CompareEachPixelInEachRegion
	{
		Loop (void) {}

		bool compare (const void* pUserData, const int x, const int y, const int z, const tcu::Vec3& srcNormCoord) const
		{
			const Capture&					c					= *static_cast<const Capture*>(pUserData);
			const tcu::TexLookupScaleMode	lookupScaleDontCare	= tcu::TEX_LOOKUP_SCALE_MINIFY;
			const tcu::IVec4				dstColor			= c.result.getPixelInt(x, y, z);

			return tcu::isLevel3DLookupResultValid(c.source, c.sampler, lookupScaleDontCare, c.precision, srcNormCoord, dstColor);
		}
	} loop;

	return loop.forEach(&capture, regions, source.getWidth(), source.getHeight(), source.getDepth(), errorMask);
}

bool BlittingImages::checkNearestFilteredResult (const tcu::ConstPixelBufferAccess&	result,
												 const tcu::ConstPixelBufferAccess& source)
{
	tcu::TestLog&					log				(m_context.getTestContext().getLog());
	const tcu::TextureFormat		dstFormat		= result.getFormat();
	const tcu::TextureFormat		srcFormat		= source.getFormat();
	const tcu::TextureChannelClass	dstChannelClass = tcu::getTextureChannelClass(dstFormat.type);
	const tcu::TextureChannelClass	srcChannelClass = tcu::getTextureChannelClass(srcFormat.type);

	tcu::TextureLevel		errorMaskStorage	(tcu::TextureFormat(tcu::TextureFormat::RGB, tcu::TextureFormat::UNORM_INT8), result.getWidth(), result.getHeight(), result.getDepth());
	tcu::PixelBufferAccess	errorMask			= errorMaskStorage.getAccess();
	tcu::Vec4				pixelBias			(0.0f, 0.0f, 0.0f, 0.0f);
	tcu::Vec4				pixelScale			(1.0f, 1.0f, 1.0f, 1.0f);
	bool					ok					= false;

	tcu::clear(errorMask, tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0));

	// if either of srcImage or dstImage was created with a signed/unsigned integer VkFormat,
	// the other must also have been created with a signed/unsigned integer VkFormat
	bool dstImageIsIntClass = dstChannelClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER ||
							  dstChannelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER;
	bool srcImageIsIntClass = srcChannelClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER ||
							  srcChannelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER;
	if (dstImageIsIntClass != srcImageIsIntClass)
		return false;

	if (dstImageIsIntClass)
	{
		ok = intNearestBlitCompare(source, result, errorMask, m_params.regions);
	}
	else
		ok = floatNearestBlitCompare(source, result, errorMask, m_params.regions);

	if (result.getFormat() != tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8))
		tcu::computePixelScaleBias(result, pixelScale, pixelBias);

	if (!ok)
	{
		log << tcu::TestLog::ImageSet("Compare", "Result comparsion")
			<< tcu::TestLog::Image("Result", "Result", result, pixelScale, pixelBias)
			<< tcu::TestLog::Image("ErrorMask",	"Error mask", errorMask)
			<< tcu::TestLog::EndImageSet;
	}
	else
	{
		log << tcu::TestLog::ImageSet("Compare", "Result comparsion")
			<< tcu::TestLog::Image("Result", "Result", result, pixelScale, pixelBias)
			<< tcu::TestLog::EndImageSet;
	}

	return ok;
}

tcu::TestStatus BlittingImages::checkTestResult (tcu::ConstPixelBufferAccess result)
{
	DE_ASSERT(m_params.filter == VK_FILTER_NEAREST || m_params.filter == VK_FILTER_LINEAR);
	const std::string failMessage("Result image is incorrect");

	if (m_params.filter == VK_FILTER_LINEAR)
	{
		if (tcu::isCombinedDepthStencilType(result.getFormat().type))
		{
			if (tcu::hasDepthComponent(result.getFormat().order))
			{
				const tcu::Sampler::DepthStencilMode	mode				= tcu::Sampler::MODE_DEPTH;
				const tcu::ConstPixelBufferAccess		depthResult			= tcu::getEffectiveDepthStencilAccess(result, mode);
				const tcu::ConstPixelBufferAccess		clampedExpected		= tcu::getEffectiveDepthStencilAccess(m_expectedTextureLevel[0]->getAccess(), mode);
				const tcu::ConstPixelBufferAccess		unclampedExpected	= tcu::getEffectiveDepthStencilAccess(m_unclampedExpectedTextureLevel->getAccess(), mode);
				const tcu::TextureFormat				sourceFormat		= tcu::getEffectiveDepthStencilTextureFormat(mapVkFormat(m_params.src.image.format), mode);

				if (!checkLinearFilteredResult(depthResult, clampedExpected, unclampedExpected, sourceFormat))
					return tcu::TestStatus::fail(failMessage);
			}

			if (tcu::hasStencilComponent(result.getFormat().order))
			{
				const tcu::Sampler::DepthStencilMode	mode				= tcu::Sampler::MODE_STENCIL;
				const tcu::ConstPixelBufferAccess		stencilResult		= tcu::getEffectiveDepthStencilAccess(result, mode);
				const tcu::ConstPixelBufferAccess		clampedExpected		= tcu::getEffectiveDepthStencilAccess(m_expectedTextureLevel[0]->getAccess(), mode);
				const tcu::ConstPixelBufferAccess		unclampedExpected	= tcu::getEffectiveDepthStencilAccess(m_unclampedExpectedTextureLevel->getAccess(), mode);
				const tcu::TextureFormat				sourceFormat		= tcu::getEffectiveDepthStencilTextureFormat(mapVkFormat(m_params.src.image.format), mode);

				if (!checkLinearFilteredResult(stencilResult, clampedExpected, unclampedExpected, sourceFormat))
					return tcu::TestStatus::fail(failMessage);
			}
		}
		else
		{
			const tcu::TextureFormat	sourceFormat	= mapVkFormat(m_params.src.image.format);

			if (!checkLinearFilteredResult(result, m_expectedTextureLevel[0]->getAccess(), m_unclampedExpectedTextureLevel->getAccess(), sourceFormat))
				return tcu::TestStatus::fail(failMessage);
		}
	}
	else // NEAREST filtering
	{
		if (tcu::isCombinedDepthStencilType(result.getFormat().type))
		{
			if (tcu::hasDepthComponent(result.getFormat().order))
			{
				const tcu::Sampler::DepthStencilMode	mode			= tcu::Sampler::MODE_DEPTH;
				const tcu::ConstPixelBufferAccess		depthResult		= tcu::getEffectiveDepthStencilAccess(result, mode);
				const tcu::ConstPixelBufferAccess		depthSource		= tcu::getEffectiveDepthStencilAccess(m_sourceTextureLevel->getAccess(), mode);

				if (!checkNearestFilteredResult(depthResult, depthSource))
					return tcu::TestStatus::fail(failMessage);
			}

			if (tcu::hasStencilComponent(result.getFormat().order))
			{
				const tcu::Sampler::DepthStencilMode	mode			= tcu::Sampler::MODE_STENCIL;
				const tcu::ConstPixelBufferAccess		stencilResult	= tcu::getEffectiveDepthStencilAccess(result, mode);
				const tcu::ConstPixelBufferAccess		stencilSource	= tcu::getEffectiveDepthStencilAccess(m_sourceTextureLevel->getAccess(), mode);

				if (!checkNearestFilteredResult(stencilResult, stencilSource))
					return tcu::TestStatus::fail(failMessage);
			}
		}
		else
		{
			if (!checkNearestFilteredResult(result, m_sourceTextureLevel->getAccess()))
				return tcu::TestStatus::fail(failMessage);
		}
	}

	return tcu::TestStatus::pass("Pass");
}

tcu::Vec4 linearToSRGBIfNeeded (const tcu::TextureFormat& format, const tcu::Vec4& color)
{
	return isSRGB(format) ? linearToSRGB(color) : color;
}

void scaleFromWholeSrcBuffer (const tcu::PixelBufferAccess& dst, const tcu::ConstPixelBufferAccess& src, const VkOffset3D regionOffset, const VkOffset3D regionExtent, tcu::Sampler::FilterMode filter, const MirrorMode mirrorMode = MIRROR_MODE_NONE)
{
	DE_ASSERT(filter == tcu::Sampler::LINEAR);
	DE_ASSERT(dst.getDepth() == 1 && src.getDepth() == 1);

	tcu::Sampler sampler(tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE,
					filter, filter, 0.0f, false);

	float sX = (float)regionExtent.x / (float)dst.getWidth();
	float sY = (float)regionExtent.y / (float)dst.getHeight();

	for (int y = 0; y < dst.getHeight(); y++)
	for (int x = 0; x < dst.getWidth(); x++)
	{
		float srcX = (mirrorMode == MIRROR_MODE_X || mirrorMode == MIRROR_MODE_XY) ? (float)regionExtent.x + (float)regionOffset.x - ((float)x+0.5f)*sX : (float)regionOffset.x + ((float)x+0.5f)*sX;
		float srcY = (mirrorMode == MIRROR_MODE_Y || mirrorMode == MIRROR_MODE_XY) ? (float)regionExtent.y + (float)regionOffset.y - ((float)y+0.5f)*sY : (float)regionOffset.y + ((float)y+0.5f)*sY;
		dst.setPixel(linearToSRGBIfNeeded(dst.getFormat(), src.sample2D(sampler, filter, srcX, srcY, 0)), x, y);
	}
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

void BlittingImages::copyRegionToTextureLevel (tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst, CopyRegion region, deUint32 mipLevel)
{
	DE_UNREF(mipLevel);

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
				scaleFromWholeSrcBuffer(unclampedSubRegion, depthSrc, srcOffset, srcExtent, filter, mirrorMode);
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
				scaleFromWholeSrcBuffer(unclampedSubRegion, stencilSrc, srcOffset, srcExtent, filter, mirrorMode);
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
			scaleFromWholeSrcBuffer(unclampedSubRegion, src, srcOffset, srcExtent, filter, mirrorMode);
		}
	}
}

void BlittingImages::generateExpectedResult (void)
{
	const tcu::ConstPixelBufferAccess	src	= m_sourceTextureLevel->getAccess();
	const tcu::ConstPixelBufferAccess	dst	= m_destinationTextureLevel->getAccess();

	m_expectedTextureLevel[0]		= de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(dst.getFormat(), dst.getWidth(), dst.getHeight(), dst.getDepth()));
	tcu::copy(m_expectedTextureLevel[0]->getAccess(), dst);

	if (m_params.filter == VK_FILTER_LINEAR)
	{
		m_unclampedExpectedTextureLevel	= de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(dst.getFormat(), dst.getWidth(), dst.getHeight(), dst.getDepth()));
		tcu::copy(m_unclampedExpectedTextureLevel->getAccess(), dst);
	}

	for (deUint32 i = 0; i < m_params.regions.size(); i++)
	{
		CopyRegion region = m_params.regions[i];
		copyRegionToTextureLevel(src, m_expectedTextureLevel[0]->getAccess(), region);
	}
}

class BlitImageTestCase : public vkt::TestCase
{
public:
							BlitImageTestCase		(tcu::TestContext&				testCtx,
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

	virtual void			checkSupport			(Context&						context) const
	{
		VkImageFormatProperties properties;
		if ((context.getInstanceInterface().getPhysicalDeviceImageFormatProperties (context.getPhysicalDevice(),
																					m_params.src.image.format,
																					m_params.src.image.imageType,
																					m_params.src.image.tiling,
																					VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
																					0,
																					&properties) == VK_ERROR_FORMAT_NOT_SUPPORTED) ||
			(context.getInstanceInterface().getPhysicalDeviceImageFormatProperties (context.getPhysicalDevice(),
																					m_params.dst.image.format,
																					m_params.dst.image.imageType,
																					m_params.dst.image.tiling,
																					VK_IMAGE_USAGE_TRANSFER_DST_BIT,
																					0,
																					&properties) == VK_ERROR_FORMAT_NOT_SUPPORTED))
		{
			TCU_THROW(NotSupportedError, "Format not supported");
		}

		VkFormatProperties srcFormatProperties;
		context.getInstanceInterface().getPhysicalDeviceFormatProperties(context.getPhysicalDevice(), m_params.src.image.format, &srcFormatProperties);
		VkFormatFeatureFlags srcFormatFeatures = m_params.src.image.tiling == VK_IMAGE_TILING_LINEAR ? srcFormatProperties.linearTilingFeatures : srcFormatProperties.optimalTilingFeatures;
		if (!(srcFormatFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT))
		{
			TCU_THROW(NotSupportedError, "Format feature blit source not supported");
		}

		VkFormatProperties dstFormatProperties;
		context.getInstanceInterface().getPhysicalDeviceFormatProperties(context.getPhysicalDevice(), m_params.dst.image.format, &dstFormatProperties);
		VkFormatFeatureFlags dstFormatFeatures = m_params.dst.image.tiling == VK_IMAGE_TILING_LINEAR ? dstFormatProperties.linearTilingFeatures : dstFormatProperties.optimalTilingFeatures;
		if (!(dstFormatFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT))
		{
			TCU_THROW(NotSupportedError, "Format feature blit destination not supported");
		}

		if (m_params.filter == VK_FILTER_LINEAR && !(srcFormatFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
			TCU_THROW(NotSupportedError, "Source format feature sampled image filter linear not supported");
	}

private:
	TestParams				m_params;
};

class BlittingMipmaps : public CopiesAndBlittingTestInstance
{
public:
										BlittingMipmaps					(Context&   context,
																		 TestParams params);
	virtual tcu::TestStatus				iterate							(void);
protected:
	virtual tcu::TestStatus				checkTestResult					(tcu::ConstPixelBufferAccess result = tcu::ConstPixelBufferAccess());
	virtual void						copyRegionToTextureLevel		(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst, CopyRegion region, deUint32 mipLevel = 0u);
	virtual void						generateExpectedResult			(void);
private:
	bool								checkLinearFilteredResult		(void);
	bool								checkNearestFilteredResult		(void);

	Move<VkImage>						m_source;
	de::MovePtr<Allocation>				m_sourceImageAlloc;
	Move<VkImage>						m_destination;
	de::MovePtr<Allocation>				m_destinationImageAlloc;

	de::MovePtr<tcu::TextureLevel>		m_unclampedExpectedTextureLevel[16];
};

BlittingMipmaps::BlittingMipmaps (Context& context, TestParams params)
	: CopiesAndBlittingTestInstance (context, params)
{
	const InstanceInterface&	vki					= context.getInstanceInterface();
	const DeviceInterface&		vk					= context.getDeviceInterface();
	const VkPhysicalDevice		vkPhysDevice		= context.getPhysicalDevice();
	const VkDevice				vkDevice			= context.getDevice();
	const deUint32				queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	Allocator&					memAlloc			= context.getDefaultAllocator();

	// Create source image
	{
		const VkImageCreateInfo		sourceImageParams		=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,								// const void*			pNext;
			getCreateFlags(m_params.src.image),		// VkImageCreateFlags	flags;
			m_params.src.image.imageType,			// VkImageType			imageType;
			m_params.src.image.format,				// VkFormat				format;
			getExtent3D(m_params.src.image),		// VkExtent3D			extent;
			1u,										// deUint32				mipLevels;
			getArraySize(m_params.src.image),		// deUint32				arraySize;
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
		m_sourceImageAlloc = allocateImage(vki, vk, vkPhysDevice, vkDevice, *m_source, MemoryRequirement::Any, memAlloc, m_params.allocationKind);
		VK_CHECK(vk.bindImageMemory(vkDevice, *m_source, m_sourceImageAlloc->getMemory(), m_sourceImageAlloc->getOffset()));
	}

	// Create destination image
	{
		const VkImageCreateInfo		destinationImageParams  =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,								// const void*			pNext;
			getCreateFlags(m_params.dst.image),		// VkImageCreateFlags	flags;
			m_params.dst.image.imageType,			// VkImageType			imageType;
			m_params.dst.image.format,				// VkFormat				format;
			getExtent3D(m_params.dst.image),		// VkExtent3D			extent;
			m_params.mipLevels,						// deUint32				mipLevels;
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

		m_destination = createImage(vk, vkDevice, &destinationImageParams);
		m_destinationImageAlloc = allocateImage(vki, vk, vkPhysDevice, vkDevice, *m_destination, MemoryRequirement::Any, memAlloc, m_params.allocationKind);
		VK_CHECK(vk.bindImageMemory(vkDevice, *m_destination, m_destinationImageAlloc->getMemory(), m_destinationImageAlloc->getOffset()));
	}
}

tcu::TestStatus BlittingMipmaps::iterate (void)
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

	uploadImage(m_destinationTextureLevel->getAccess(), m_destination.get(), m_params.dst.image, m_params.mipLevels);

	const DeviceInterface&		vk					= m_context.getDeviceInterface();
	const VkDevice				vkDevice			= m_context.getDevice();
	const VkQueue				queue				= m_context.getUniversalQueue();

	std::vector<VkImageBlit>	regions;
	for (deUint32 i = 0; i < m_params.regions.size(); i++)
		regions.push_back(m_params.regions[i].imageBlit);

	// Copy source image to mip level 0 when generating mipmaps with multiple blit commands
	if (!m_params.singleCommand)
		uploadImage(m_sourceTextureLevel->getAccess(), m_destination.get(), m_params.dst.image, 1u);

	beginCommandBuffer(vk, *m_cmdBuffer);

	// Blit all mip levels with a single blit command
	if (m_params.singleCommand)
	{
		{
			// Source image layout
			const VkImageMemoryBarrier		srcImageBarrier		=
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
				DE_NULL,									// const void*				pNext;
				VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			srcAccessMask;
				VK_ACCESS_TRANSFER_READ_BIT,				// VkAccessFlags			dstAccessMask;
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			oldLayout;
				m_params.src.image.operationLayout,			// VkImageLayout			newLayout;
				VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
				VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
				m_source.get(),								// VkImage					image;
				{											// VkImageSubresourceRange	subresourceRange;
					getAspectFlags(srcTcuFormat),		// VkImageAspectFlags   aspectMask;
					0u,									// deUint32				baseMipLevel;
					1u,									// deUint32				mipLevels;
					0u,									// deUint32				baseArraySlice;
					getArraySize(m_params.src.image)	// deUint32				arraySize;
				}
			};

			// Destination image layout
			const VkImageMemoryBarrier		dstImageBarrier		=
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
				DE_NULL,									// const void*				pNext;
				VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			srcAccessMask;
				VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			dstAccessMask;
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			oldLayout;
				m_params.dst.image.operationLayout,			// VkImageLayout			newLayout;
				VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
				VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
				m_destination.get(),						// VkImage					image;
				{											// VkImageSubresourceRange	subresourceRange;
					getAspectFlags(dstTcuFormat),		// VkImageAspectFlags   aspectMask;
					0u,									// deUint32				baseMipLevel;
					m_params.mipLevels,					// deUint32				mipLevels;
					0u,									// deUint32				baseArraySlice;
					getArraySize(m_params.dst.image)	// deUint32				arraySize;
				}
			};

			vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &srcImageBarrier);
			vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &dstImageBarrier);
			vk.cmdBlitImage(*m_cmdBuffer, m_source.get(), m_params.src.image.operationLayout, m_destination.get(), m_params.dst.image.operationLayout, (deUint32)regions.size(), &regions[0], m_params.filter);
		}
	}
	// Blit mip levels with multiple blit commands
	else
	{
		// Prepare all mip levels for reading
		{
			for (deUint32 barrierno = 0; barrierno < m_params.barrierCount; barrierno++)
			{
				VkImageMemoryBarrier preImageBarrier =
				{
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,									// VkStructureType	sType;
					DE_NULL,																// const void*		pNext;
					VK_ACCESS_TRANSFER_WRITE_BIT,											// VkAccessFlags	srcAccessMask;
					VK_ACCESS_TRANSFER_READ_BIT,											// VkAccessFlags	dstAccessMask;
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,									// VkImageLayout	oldLayout;
					m_params.src.image.operationLayout,										// VkImageLayout	newLayout;
					VK_QUEUE_FAMILY_IGNORED,												// deUint32			srcQueueFamilyIndex;
					VK_QUEUE_FAMILY_IGNORED,												// deUint32			dstQueueFamilyIndex;
					m_destination.get(),													// VkImage			image;
					{																		// VkImageSubresourceRange	subresourceRange;
						getAspectFlags(dstTcuFormat),										// VkImageAspectFlags	aspectMask;
							0u,																// deUint32				baseMipLevel;
							VK_REMAINING_MIP_LEVELS,										// deUint32				mipLevels;
							0u,																// deUint32				baseArraySlice;
							getArraySize(m_params.src.image)								// deUint32				arraySize;
					}
				};

				if (getArraySize(m_params.src.image) == 1)
				{
					DE_ASSERT(barrierno < m_params.mipLevels);
					preImageBarrier.subresourceRange.baseMipLevel	= barrierno;
					preImageBarrier.subresourceRange.levelCount		= (barrierno + 1 < m_params.barrierCount) ? 1 : VK_REMAINING_MIP_LEVELS;
				}
				else
				{
					preImageBarrier.subresourceRange.baseArrayLayer	= barrierno;
					preImageBarrier.subresourceRange.layerCount		= (barrierno + 1 < m_params.barrierCount) ? 1 : VK_REMAINING_ARRAY_LAYERS;
				}
				vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &preImageBarrier);
			}
		}

		for (deUint32 regionNdx = 0u; regionNdx < (deUint32)regions.size(); regionNdx++)
		{
			const deUint32					mipLevel			= regions[regionNdx].dstSubresource.mipLevel;

			// Prepare single mip level for writing
			const VkImageMemoryBarrier		preImageBarrier		=
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
				DE_NULL,									// const void*					pNext;
				VK_ACCESS_TRANSFER_READ_BIT,				// VkAccessFlags			srcAccessMask;
				VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			dstAccessMask;
				m_params.src.image.operationLayout,			// VkImageLayout			oldLayout;
				m_params.dst.image.operationLayout,			// VkImageLayout			newLayout;
				VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
				VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
				m_destination.get(),						// VkImage					image;
				{											// VkImageSubresourceRange	subresourceRange;
					getAspectFlags(dstTcuFormat),		// VkImageAspectFlags	aspectMask;
					mipLevel,							// deUint32				baseMipLevel;
					1u,									// deUint32				mipLevels;
					0u,									// deUint32				baseArraySlice;
					getArraySize(m_params.dst.image)	// deUint32				arraySize;
				}
			};

			// Prepare single mip level for reading
			const VkImageMemoryBarrier		postImageBarrier	=
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
				DE_NULL,									// const void*				pNext;
				VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			srcAccessMask;
				VK_ACCESS_TRANSFER_READ_BIT,				// VkAccessFlags			dstAccessMask;
				m_params.dst.image.operationLayout,			// VkImageLayout			oldLayout;
				m_params.src.image.operationLayout,			// VkImageLayout			newLayout;
				VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
				VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
				m_destination.get(),						// VkImage					image;
				{											// VkImageSubresourceRange	subresourceRange;
					getAspectFlags(dstTcuFormat),		// VkImageAspectFlags	aspectMask;
					mipLevel,							// deUint32				baseMipLevel;
					1u,									// deUint32				mipLevels;
					0u,									// deUint32				baseArraySlice;
					getArraySize(m_params.src.image)	// deUint32				arraySize;
				}
			};

			vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &preImageBarrier);
			vk.cmdBlitImage(*m_cmdBuffer, m_destination.get(), m_params.src.image.operationLayout, m_destination.get(), m_params.dst.image.operationLayout, 1u, &regions[regionNdx], m_params.filter);
			vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &postImageBarrier);
		}

		// Prepare all mip levels for writing
		{
			const VkImageMemoryBarrier		postImageBarrier		=
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
				DE_NULL,									// const void*				pNext;
				VK_ACCESS_TRANSFER_READ_BIT,				// VkAccessFlags			srcAccessMask;
				VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			dstAccessMask;
				m_params.src.image.operationLayout,			// VkImageLayout			oldLayout;
				m_params.dst.image.operationLayout,			// VkImageLayout			newLayout;
				VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
				VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
				m_destination.get(),						// VkImage					image;
				{											// VkImageSubresourceRange	subresourceRange;
					getAspectFlags(dstTcuFormat),		// VkImageAspectFlags	aspectMask;
					0u,									// deUint32				baseMipLevel;
					VK_REMAINING_MIP_LEVELS,			// deUint32				mipLevels;
					0u,									// deUint32				baseArraySlice;
					getArraySize(m_params.dst.image)	// deUint32				arraySize;
				}
			};

			vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &postImageBarrier);
		}
	}

	endCommandBuffer(vk, *m_cmdBuffer);
	submitCommandsAndWait(vk, vkDevice, queue, *m_cmdBuffer);

	return checkTestResult();
}

bool BlittingMipmaps::checkLinearFilteredResult (void)
{
	tcu::TestLog&				log				(m_context.getTestContext().getLog());
	bool						allLevelsOk		= true;

	for (deUint32 mipLevelNdx = 0u; mipLevelNdx < m_params.mipLevels; mipLevelNdx++)
	{
		// Update reference results with previous results that have been verified.
		// This needs to be done such that accumulated errors don't exceed the fixed threshold.
		for (deUint32 i = 0; i < m_params.regions.size(); i++)
		{
			const CopyRegion region = m_params.regions[i];
			const deUint32 srcMipLevel = m_params.regions[i].imageBlit.srcSubresource.mipLevel;
			const deUint32 dstMipLevel = m_params.regions[i].imageBlit.dstSubresource.mipLevel;
			de::MovePtr<tcu::TextureLevel>	prevResultLevel;
			tcu::ConstPixelBufferAccess src;
			if (srcMipLevel < mipLevelNdx)
			{
				// Generate expected result from rendered result that was previously verified
				prevResultLevel	= readImage(*m_destination, m_params.dst.image, srcMipLevel);
				src = prevResultLevel->getAccess();
			}
			else
			{
				// Previous reference mipmaps might have changed, so recompute expected result
				src = m_expectedTextureLevel[srcMipLevel]->getAccess();
			}
			copyRegionToTextureLevel(src, m_expectedTextureLevel[dstMipLevel]->getAccess(), region, dstMipLevel);
		}

		de::MovePtr<tcu::TextureLevel>			resultLevel			= readImage(*m_destination, m_params.dst.image, mipLevelNdx);
		const tcu::ConstPixelBufferAccess&		resultAccess		= resultLevel->getAccess();

		const tcu::Sampler::DepthStencilMode	mode				= tcu::hasDepthComponent(resultAccess.getFormat().order)	?   tcu::Sampler::MODE_DEPTH :
																	  tcu::hasStencilComponent(resultAccess.getFormat().order)  ?   tcu::Sampler::MODE_STENCIL :
																																	tcu::Sampler::MODE_LAST;
		const tcu::ConstPixelBufferAccess		result				= tcu::hasDepthComponent(resultAccess.getFormat().order)	?   getEffectiveDepthStencilAccess(resultAccess, mode) :
																	  tcu::hasStencilComponent(resultAccess.getFormat().order)  ?   getEffectiveDepthStencilAccess(resultAccess, mode) :
																																	resultAccess;
		const tcu::ConstPixelBufferAccess		clampedLevel		= tcu::hasDepthComponent(resultAccess.getFormat().order)	?   getEffectiveDepthStencilAccess(m_expectedTextureLevel[mipLevelNdx]->getAccess(), mode) :
																	  tcu::hasStencilComponent(resultAccess.getFormat().order)  ?   getEffectiveDepthStencilAccess(m_expectedTextureLevel[mipLevelNdx]->getAccess(), mode) :
																																	m_expectedTextureLevel[mipLevelNdx]->getAccess();
		const tcu::ConstPixelBufferAccess		unclampedLevel		= tcu::hasDepthComponent(resultAccess.getFormat().order)	?   getEffectiveDepthStencilAccess(m_unclampedExpectedTextureLevel[mipLevelNdx]->getAccess(), mode) :
																	  tcu::hasStencilComponent(resultAccess.getFormat().order)  ?   getEffectiveDepthStencilAccess(m_unclampedExpectedTextureLevel[mipLevelNdx]->getAccess(), mode) :
																																	m_unclampedExpectedTextureLevel[mipLevelNdx]->getAccess();
		const tcu::TextureFormat				srcFormat			= tcu::hasDepthComponent(resultAccess.getFormat().order)	?   tcu::getEffectiveDepthStencilTextureFormat(mapVkFormat(m_params.src.image.format), mode) :
																	  tcu::hasStencilComponent(resultAccess.getFormat().order)  ?   tcu::getEffectiveDepthStencilTextureFormat(mapVkFormat(m_params.src.image.format), mode) :
																																	mapVkFormat(m_params.src.image.format);

		const tcu::TextureFormat				dstFormat			= result.getFormat();
		bool									singleLevelOk		= false;
		std::vector <CopyRegion>				mipLevelRegions;

		for (size_t regionNdx = 0u; regionNdx < m_params.regions.size(); regionNdx++)
			if (m_params.regions.at(regionNdx).imageBlit.dstSubresource.mipLevel == mipLevelNdx)
				mipLevelRegions.push_back(m_params.regions.at(regionNdx));

		log << tcu::TestLog::Section("ClampedSourceImage", "Region with clamped edges on source image.");

		if (isFloatFormat(dstFormat))
		{
			const bool		srcIsSRGB   = tcu::isSRGB(srcFormat);
			const tcu::Vec4 srcMaxDiff  = getFormatThreshold(srcFormat) * tcu::Vec4(srcIsSRGB ? 2.0f : 1.0f);
			const tcu::Vec4 dstMaxDiff  = getFormatThreshold(dstFormat);
			const tcu::Vec4 threshold   = tcu::max(srcMaxDiff, dstMaxDiff);

			singleLevelOk = tcu::floatThresholdCompare(log, "Compare", "Result comparsion", clampedLevel, result, threshold, tcu::COMPARE_LOG_RESULT);
			log << tcu::TestLog::EndSection;

			if (!singleLevelOk)
			{
				log << tcu::TestLog::Section("NonClampedSourceImage", "Region with non-clamped edges on source image.");
				singleLevelOk = tcu::floatThresholdCompare(log, "Compare", "Result comparsion", unclampedLevel, result, threshold, tcu::COMPARE_LOG_RESULT);
				log << tcu::TestLog::EndSection;
			}
		}
		else
		{
			tcu::UVec4  threshold;
			// Calculate threshold depending on channel width of destination format.
			const tcu::IVec4	bitDepth	= tcu::getTextureFormatBitDepth(dstFormat);
			for (deUint32 i = 0; i < 4; ++i)
				threshold[i] = de::max((0x1 << bitDepth[i]) / 256, 2);

			singleLevelOk = tcu::intThresholdCompare(log, "Compare", "Result comparsion", clampedLevel, result, threshold, tcu::COMPARE_LOG_RESULT);
			log << tcu::TestLog::EndSection;

			if (!singleLevelOk)
			{
				log << tcu::TestLog::Section("NonClampedSourceImage", "Region with non-clamped edges on source image.");
				singleLevelOk = tcu::intThresholdCompare(log, "Compare", "Result comparsion", unclampedLevel, result, threshold, tcu::COMPARE_LOG_RESULT);
				log << tcu::TestLog::EndSection;
			}
		}
		allLevelsOk &= singleLevelOk;
	}

	return allLevelsOk;
}

bool BlittingMipmaps::checkNearestFilteredResult (void)
{
	bool						allLevelsOk		= true;
	tcu::TestLog&				log				(m_context.getTestContext().getLog());

	for (deUint32 mipLevelNdx = 0u; mipLevelNdx < m_params.mipLevels; mipLevelNdx++)
	{
		de::MovePtr<tcu::TextureLevel>			resultLevel			= readImage(*m_destination, m_params.dst.image, mipLevelNdx);
		const tcu::ConstPixelBufferAccess&		resultAccess		= resultLevel->getAccess();

		const tcu::Sampler::DepthStencilMode	mode				= tcu::hasDepthComponent(resultAccess.getFormat().order)	?   tcu::Sampler::MODE_DEPTH :
																	  tcu::hasStencilComponent(resultAccess.getFormat().order)  ?   tcu::Sampler::MODE_STENCIL :
																																	tcu::Sampler::MODE_LAST;
		const tcu::ConstPixelBufferAccess		result				= tcu::hasDepthComponent(resultAccess.getFormat().order)	?   getEffectiveDepthStencilAccess(resultAccess, mode) :
																	  tcu::hasStencilComponent(resultAccess.getFormat().order)  ?   getEffectiveDepthStencilAccess(resultAccess, mode) :
																																	resultAccess;
		const tcu::ConstPixelBufferAccess		source				= (m_params.singleCommand || mipLevelNdx == 0) ?			//  Read from source image
																	  tcu::hasDepthComponent(resultAccess.getFormat().order)	?   tcu::getEffectiveDepthStencilAccess(m_sourceTextureLevel->getAccess(), mode) :
																	  tcu::hasStencilComponent(resultAccess.getFormat().order)  ?   tcu::getEffectiveDepthStencilAccess(m_sourceTextureLevel->getAccess(), mode) :
																																	m_sourceTextureLevel->getAccess()
																																//  Read from destination image
																	: tcu::hasDepthComponent(resultAccess.getFormat().order)	?   tcu::getEffectiveDepthStencilAccess(m_expectedTextureLevel[mipLevelNdx - 1u]->getAccess(), mode) :
																	  tcu::hasStencilComponent(resultAccess.getFormat().order)  ?   tcu::getEffectiveDepthStencilAccess(m_expectedTextureLevel[mipLevelNdx - 1u]->getAccess(), mode) :
																																	m_expectedTextureLevel[mipLevelNdx - 1u]->getAccess();
		const tcu::TextureFormat				dstFormat			= result.getFormat();
		const tcu::TextureChannelClass			dstChannelClass		= tcu::getTextureChannelClass(dstFormat.type);
		bool									singleLevelOk		= false;
		std::vector <CopyRegion>				mipLevelRegions;

		for (size_t regionNdx = 0u; regionNdx < m_params.regions.size(); regionNdx++)
			if (m_params.regions.at(regionNdx).imageBlit.dstSubresource.mipLevel == mipLevelNdx)
				mipLevelRegions.push_back(m_params.regions.at(regionNdx));

		tcu::TextureLevel				errorMaskStorage	(tcu::TextureFormat(tcu::TextureFormat::RGB, tcu::TextureFormat::UNORM_INT8), result.getWidth(), result.getHeight(), result.getDepth());
		tcu::PixelBufferAccess			errorMask			= errorMaskStorage.getAccess();
		tcu::Vec4						pixelBias			(0.0f, 0.0f, 0.0f, 0.0f);
		tcu::Vec4						pixelScale			(1.0f, 1.0f, 1.0f, 1.0f);

		tcu::clear(errorMask, tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0));

		if (dstChannelClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER ||
			dstChannelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER)
		{
			singleLevelOk = intNearestBlitCompare(source, result, errorMask, mipLevelRegions);
		}
		else
			singleLevelOk = floatNearestBlitCompare(source, result, errorMask, mipLevelRegions);

		if (dstFormat != tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8))
			tcu::computePixelScaleBias(result, pixelScale, pixelBias);

		if (!singleLevelOk)
		{
			log << tcu::TestLog::ImageSet("Compare", "Result comparsion, level " + de::toString(mipLevelNdx))
				<< tcu::TestLog::Image("Result", "Result", result, pixelScale, pixelBias)
				<< tcu::TestLog::Image("Reference", "Reference", source, pixelScale, pixelBias)
				<< tcu::TestLog::Image("ErrorMask", "Error mask", errorMask)
				<< tcu::TestLog::EndImageSet;
		}
		else
		{
			log << tcu::TestLog::ImageSet("Compare", "Result comparsion, level " + de::toString(mipLevelNdx))
				<< tcu::TestLog::Image("Result", "Result", result, pixelScale, pixelBias)
				<< tcu::TestLog::EndImageSet;
		}

		allLevelsOk &= singleLevelOk;
	}

	return allLevelsOk;
}

tcu::TestStatus BlittingMipmaps::checkTestResult (tcu::ConstPixelBufferAccess result)
{
	DE_UNREF(result);
	DE_ASSERT(m_params.filter == VK_FILTER_NEAREST || m_params.filter == VK_FILTER_LINEAR);
	const std::string failMessage("Result image is incorrect");

	if (m_params.filter == VK_FILTER_LINEAR)
	{
		if (!checkLinearFilteredResult())
			return tcu::TestStatus::fail(failMessage);
	}
	else // NEAREST filtering
	{
		if (!checkNearestFilteredResult())
			return tcu::TestStatus::fail(failMessage);
	}

	return tcu::TestStatus::pass("Pass");
}

void BlittingMipmaps::copyRegionToTextureLevel (tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst, CopyRegion region, deUint32 mipLevel)
{
	DE_ASSERT(src.getDepth() == dst.getDepth());

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
				const tcu::PixelBufferAccess		unclampedSubRegion	= getEffectiveDepthStencilAccess(tcu::getSubregion(m_unclampedExpectedTextureLevel[0]->getAccess(), dstOffset.x, dstOffset.y, dstExtent.x, dstExtent.y), tcu::Sampler::MODE_DEPTH);
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
				const tcu::PixelBufferAccess		unclampedSubRegion	= getEffectiveDepthStencilAccess(tcu::getSubregion(m_unclampedExpectedTextureLevel[0]->getAccess(), dstOffset.x, dstOffset.y, dstExtent.x, dstExtent.y), tcu::Sampler::MODE_STENCIL);
				scaleFromWholeSrcBuffer(unclampedSubRegion, stencilSrc, srcOffset, srcExtent, filter);
			}
		}
	}
	else
	{
		for (int layerNdx = 0u; layerNdx < src.getDepth(); layerNdx++)
		{
			const tcu::ConstPixelBufferAccess	srcSubRegion	= tcu::getSubregion(src, srcOffset.x, srcOffset.y, layerNdx, srcExtent.x, srcExtent.y, 1);
			const tcu::PixelBufferAccess		dstSubRegion	= tcu::getSubregion(dst, dstOffset.x, dstOffset.y, layerNdx, dstExtent.x, dstExtent.y, 1);
			blit(dstSubRegion, srcSubRegion, filter, mirrorMode);

			if (filter == tcu::Sampler::LINEAR)
			{
				const tcu::PixelBufferAccess	unclampedSubRegion	= tcu::getSubregion(m_unclampedExpectedTextureLevel[mipLevel]->getAccess(), dstOffset.x, dstOffset.y, layerNdx, dstExtent.x, dstExtent.y, 1);
				scaleFromWholeSrcBuffer(unclampedSubRegion, srcSubRegion, srcOffset, srcExtent, filter);
			}
		}
	}
}

void BlittingMipmaps::generateExpectedResult (void)
{
	const tcu::ConstPixelBufferAccess	src	= m_sourceTextureLevel->getAccess();
	const tcu::ConstPixelBufferAccess	dst	= m_destinationTextureLevel->getAccess();

	for (deUint32 mipLevelNdx = 0u; mipLevelNdx < m_params.mipLevels; mipLevelNdx++)
		m_expectedTextureLevel[mipLevelNdx] = de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(dst.getFormat(), dst.getWidth() >> mipLevelNdx, dst.getHeight() >> mipLevelNdx, dst.getDepth()));

	tcu::copy(m_expectedTextureLevel[0]->getAccess(), src);

	if (m_params.filter == VK_FILTER_LINEAR)
	{
		for (deUint32 mipLevelNdx = 0u; mipLevelNdx < m_params.mipLevels; mipLevelNdx++)
			m_unclampedExpectedTextureLevel[mipLevelNdx] = de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(dst.getFormat(), dst.getWidth() >> mipLevelNdx, dst.getHeight() >> mipLevelNdx, dst.getDepth()));

		tcu::copy(m_unclampedExpectedTextureLevel[0]->getAccess(), src);
	}

	for (deUint32 i = 0; i < m_params.regions.size(); i++)
	{
		CopyRegion region = m_params.regions[i];
		copyRegionToTextureLevel(m_expectedTextureLevel[m_params.regions[i].imageBlit.srcSubresource.mipLevel]->getAccess(), m_expectedTextureLevel[m_params.regions[i].imageBlit.dstSubresource.mipLevel]->getAccess(), region, m_params.regions[i].imageBlit.dstSubresource.mipLevel);
	}
}

class BlitMipmapTestCase : public vkt::TestCase
{
public:
							BlitMipmapTestCase		(tcu::TestContext&				testCtx,
													 const std::string&				name,
													 const std::string&				description,
													 const TestParams				params)
								: vkt::TestCase	(testCtx, name, description)
								, m_params		(params)
	{}

	virtual TestInstance*	createInstance			(Context&						context) const
	{
		return new BlittingMipmaps(context, m_params);
	}

	virtual void			checkSupport			(Context&						context) const
	{
		const InstanceInterface&	vki					= context.getInstanceInterface();
		const VkPhysicalDevice		vkPhysDevice		= context.getPhysicalDevice();
		{
			VkImageFormatProperties	properties;
			if (context.getInstanceInterface().getPhysicalDeviceImageFormatProperties (context.getPhysicalDevice(),
																						m_params.src.image.format,
																						VK_IMAGE_TYPE_2D,
																						VK_IMAGE_TILING_OPTIMAL,
																						VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
																						0,
																						&properties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
			{
				TCU_THROW(NotSupportedError, "Format not supported");
			}
			else if ((m_params.src.image.extent.width	> properties.maxExtent.width)	||
						(m_params.src.image.extent.height	> properties.maxExtent.height)	||
						(m_params.src.image.extent.depth	> properties.maxArrayLayers))
			{
				TCU_THROW(NotSupportedError, "Image size not supported");
			}
		}

		{
			VkImageFormatProperties	properties;
			if (context.getInstanceInterface().getPhysicalDeviceImageFormatProperties (context.getPhysicalDevice(),
																						m_params.dst.image.format,
																						VK_IMAGE_TYPE_2D,
																						VK_IMAGE_TILING_OPTIMAL,
																						VK_IMAGE_USAGE_TRANSFER_DST_BIT,
																						0,
																						&properties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
			{
				TCU_THROW(NotSupportedError, "Format not supported");
			}
			else if ((m_params.dst.image.extent.width	> properties.maxExtent.width)	||
						(m_params.dst.image.extent.height	> properties.maxExtent.height)	||
						(m_params.dst.image.extent.depth	> properties.maxArrayLayers))
			{
				TCU_THROW(NotSupportedError, "Image size not supported");
			}
			else if (m_params.mipLevels > properties.maxMipLevels)
			{
				TCU_THROW(NotSupportedError, "Number of mip levels not supported");
			}
		}

		const VkFormatProperties	srcFormatProperties	= getPhysicalDeviceFormatProperties (vki, vkPhysDevice, m_params.src.image.format);
		if (!(srcFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT))
		{
			TCU_THROW(NotSupportedError, "Format feature blit source not supported");
		}

		const VkFormatProperties	dstFormatProperties	= getPhysicalDeviceFormatProperties (vki, vkPhysDevice, m_params.dst.image.format);
		if (!(dstFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT))
		{
			TCU_THROW(NotSupportedError, "Format feature blit destination not supported");
		}

		if (m_params.filter == VK_FILTER_LINEAR && !(srcFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
			TCU_THROW(NotSupportedError, "Source format feature sampled image filter linear not supported");
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
	virtual tcu::TestStatus						checkTestResult				(tcu::ConstPixelBufferAccess result = tcu::ConstPixelBufferAccess());
	void										copyMSImageToMSImage		(deUint32 copyArraySize);
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
																			 CopyRegion						region,
																			 deUint32						mipLevel = 0u);
};

ResolveImageToImage::ResolveImageToImage (Context& context, TestParams params, const ResolveImageToImageOptions options)
	: CopiesAndBlittingTestInstance	(context, params)
	, m_options						(options)
{
	const InstanceInterface&	vki						= context.getInstanceInterface();
	const DeviceInterface&		vk						= context.getDeviceInterface();
	const VkPhysicalDevice		vkPhysDevice			= context.getPhysicalDevice();
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

	const VkSampleCountFlagBits	rasterizationSamples	= m_params.samples;

	// Create color image.
	{
		VkImageCreateInfo	colorImageParams	=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,									// VkStructureType			sType;
			DE_NULL,																// const void*				pNext;
			getCreateFlags(m_params.src.image),										// VkImageCreateFlags		flags;
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

		m_multisampledImage						= createImage(vk, vkDevice, &colorImageParams);

		// Allocate and bind color image memory.
		m_multisampledImageAlloc				= allocateImage(vki, vk, vkPhysDevice, vkDevice, *m_multisampledImage, MemoryRequirement::Any, memAlloc, m_params.allocationKind);
		VK_CHECK(vk.bindImageMemory(vkDevice, *m_multisampledImage, m_multisampledImageAlloc->getMemory(), m_multisampledImageAlloc->getOffset()));

		switch (m_options)
		{
			case COPY_MS_IMAGE_TO_MS_IMAGE:
			{
				colorImageParams.usage			= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
				m_multisampledCopyImage			= createImage(vk, vkDevice, &colorImageParams);
				// Allocate and bind color image memory.
				m_multisampledCopyImageAlloc	= allocateImage(vki, vk, vkPhysDevice, vkDevice, *m_multisampledCopyImage, MemoryRequirement::Any, memAlloc, m_params.allocationKind);
				VK_CHECK(vk.bindImageMemory(vkDevice, *m_multisampledCopyImage, m_multisampledCopyImageAlloc->getMemory(), m_multisampledCopyImageAlloc->getOffset()));
				break;
			}

			case COPY_MS_IMAGE_TO_ARRAY_MS_IMAGE:
			{
				colorImageParams.usage			= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
				colorImageParams.arrayLayers	= getArraySize(m_params.dst.image);
				m_multisampledCopyImage			= createImage(vk, vkDevice, &colorImageParams);
				// Allocate and bind color image memory.
				m_multisampledCopyImageAlloc	= allocateImage(vki, vk, vkPhysDevice, vkDevice, *m_multisampledCopyImage, MemoryRequirement::Any, memAlloc, m_params.allocationKind);
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
			getCreateFlags(m_params.dst.image),		// VkImageCreateFlags	flags;
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
		m_destinationImageAlloc	= allocateImage(vki, vk, vkPhysDevice, vkDevice, *m_destination, MemoryRequirement::Any, memAlloc, m_params.allocationKind);
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

	// Create upper half triangle.
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
		vertexBufferAlloc	= allocateBuffer(vki, vk, vkPhysDevice, vkDevice, *vertexBuffer, MemoryRequirement::HostVisible, memAlloc, m_params.allocationKind);
		VK_CHECK(vk.bindBufferMemory(vkDevice, *vertexBuffer, vertexBufferAlloc->getMemory(), vertexBufferAlloc->getOffset()));

		// Load vertices into vertex buffer.
		deMemcpy(vertexBufferAlloc->getHostPtr(), vertices.data(), (size_t)vertexDataSize);
		flushAlloc(vk, vkDevice, *vertexBufferAlloc);
	}

	{
		Move<VkFramebuffer>		framebuffer;
		Move<VkImageView>		sourceAttachmentView;

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
					VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,			// VkStructureType				sType;
					DE_NULL,											// const void*					pNext;
					0u,													// VkFramebufferCreateFlags		flags;
					*renderPass,										// VkRenderPass					renderPass;
					1u,													// deUint32						attachmentCount;
					attachments,										// const VkImageView*			pAttachments;
					m_params.src.image.extent.width,					// deUint32						width;
					m_params.src.image.extent.height,					// deUint32						height;
					1u													// deUint32						layers;
			};

			framebuffer	= createFramebuffer(vk, vkDevice, &framebufferParams);
		}

		// Create pipeline
		{
			const std::vector<VkViewport>	viewports	(1, makeViewport(m_params.src.image.extent));
			const std::vector<VkRect2D>		scissors	(1, makeRect2D(m_params.src.image.extent));

			const VkPipelineMultisampleStateCreateInfo	multisampleStateParams		=
			{
				VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType							sType;
				DE_NULL,													// const void*								pNext;
				0u,															// VkPipelineMultisampleStateCreateFlags	flags;
				rasterizationSamples,										// VkSampleCountFlagBits					rasterizationSamples;
				VK_FALSE,													// VkBool32									sampleShadingEnable;
				0.0f,														// float									minSampleShading;
				DE_NULL,													// const VkSampleMask*						pSampleMask;
				VK_FALSE,													// VkBool32									alphaToCoverageEnable;
				VK_FALSE													// VkBool32									alphaToOneEnable;
			};

			graphicsPipeline = makeGraphicsPipeline(vk,										// const DeviceInterface&                        vk
													vkDevice,								// const VkDevice                                device
													*pipelineLayout,						// const VkPipelineLayout                        pipelineLayout
													*vertexShaderModule,					// const VkShaderModule                          vertexShaderModule
													DE_NULL,								// const VkShaderModule                          tessellationControlModule
													DE_NULL,								// const VkShaderModule                          tessellationEvalModule
													DE_NULL,								// const VkShaderModule                          geometryShaderModule
													*fragmentShaderModule,					// const VkShaderModule                          fragmentShaderModule
													*renderPass,							// const VkRenderPass                            renderPass
													viewports,								// const std::vector<VkViewport>&                viewports
													scissors,								// const std::vector<VkRect2D>&                  scissors
													VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,	// const VkPrimitiveTopology                     topology
													0u,										// const deUint32                                subpass
													0u,										// const deUint32                                patchControlPoints
													DE_NULL,								// const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
													DE_NULL,								// const VkPipelineRasterizationStateCreateInfo* rasterizationStateCreateInfo
													&multisampleStateParams);				// const VkPipelineMultisampleStateCreateInfo*   multisampleStateCreateInfo
		}

		// Create command buffer
		{
			beginCommandBuffer(vk, *m_cmdBuffer, 0u);
			vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &srcImageBarrier);
			beginRenderPass(vk, *m_cmdBuffer, *renderPass, *framebuffer, makeRect2D(0, 0, m_params.src.image.extent.width, m_params.src.image.extent.height), tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));

			const VkDeviceSize	vertexBufferOffset	= 0u;

			vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);
			vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer.get(), &vertexBufferOffset);
			vk.cmdDraw(*m_cmdBuffer, (deUint32)vertices.size(), 1, 0, 0);

			endRenderPass(vk, *m_cmdBuffer);
			endCommandBuffer(vk, *m_cmdBuffer);
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

	VkImage		sourceImage		= m_multisampledImage.get();
	deUint32	sourceArraySize	= getArraySize(m_params.src.image);

	switch (m_options)
	{
		case COPY_MS_IMAGE_TO_ARRAY_MS_IMAGE:
			// Duplicate the multisampled image to a multisampled image array
			sourceArraySize	= getArraySize(m_params.dst.image); // fall through
		case COPY_MS_IMAGE_TO_MS_IMAGE:
			copyMSImageToMSImage(sourceArraySize);
			sourceImage	= m_multisampledCopyImage.get();
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
			sourceImage,								// VkImage					image;
			{											// VkImageSubresourceRange	subresourceRange;
				getAspectFlags(srcTcuFormat),		// VkImageAspectFlags	aspectMask;
				0u,									// deUint32				baseMipLevel;
				1u,									// deUint32				mipLevels;
				0u,									// deUint32				baseArraySlice;
				sourceArraySize						// deUint32				arraySize;
			}
		},
		// destination image
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			0u,											// VkAccessFlags			srcAccessMask;
			VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			dstAccessMask;
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			oldLayout;
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
		VK_ACCESS_HOST_READ_BIT,				// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,	// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,	// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,				// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,				// deUint32					dstQueueFamilyIndex;
		m_destination.get(),					// VkImage					image;
		{										// VkImageSubresourceRange	subresourceRange;
			getAspectFlags(dstTcuFormat),		// VkImageAspectFlags		aspectMask;
			0u,									// deUint32					baseMipLevel;
			1u,									// deUint32					mipLevels;
			0u,									// deUint32					baseArraySlice;
			getArraySize(m_params.dst.image)	// deUint32					arraySize;
		}
	};

	beginCommandBuffer(vk, *m_cmdBuffer);
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, DE_LENGTH_OF_ARRAY(imageBarriers), imageBarriers);
	vk.cmdResolveImage(*m_cmdBuffer, sourceImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_destination.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (deUint32)m_params.regions.size(), imageResolves.data());
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &postImageBarrier);
	endCommandBuffer(vk, *m_cmdBuffer);
	submitCommandsAndWait(vk, vkDevice, queue, *m_cmdBuffer);

	de::MovePtr<tcu::TextureLevel>	resultTextureLevel	= readImage(*m_destination, m_params.dst.image);

	return checkTestResult(resultTextureLevel->getAccess());
}

tcu::TestStatus ResolveImageToImage::checkTestResult (tcu::ConstPixelBufferAccess result)
{
	const tcu::ConstPixelBufferAccess	expected		= m_expectedTextureLevel[0]->getAccess();
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

void ResolveImageToImage::copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst, CopyRegion region, deUint32 mipLevel)
{
	DE_UNREF(mipLevel);

	VkOffset3D srcOffset	= region.imageResolve.srcOffset;
			srcOffset.z		= region.imageResolve.srcSubresource.baseArrayLayer;
	VkOffset3D dstOffset	= region.imageResolve.dstOffset;
			dstOffset.z		= region.imageResolve.dstSubresource.baseArrayLayer;
	VkExtent3D extent		= region.imageResolve.extent;
			extent.depth		= region.imageResolve.srcSubresource.layerCount;

	const tcu::ConstPixelBufferAccess	srcSubRegion		= getSubregion (src, srcOffset.x, srcOffset.y, srcOffset.z, extent.width, extent.height, extent.depth);
	// CopyImage acts like a memcpy. Replace the destination format with the srcformat to use a memcpy.
	const tcu::PixelBufferAccess		dstWithSrcFormat	(srcSubRegion.getFormat(), dst.getSize(), dst.getDataPtr());
	const tcu::PixelBufferAccess		dstSubRegion		= getSubregion (dstWithSrcFormat, dstOffset.x, dstOffset.y, dstOffset.z, extent.width, extent.height, extent.depth);

	tcu::copy(dstSubRegion, srcSubRegion);
}

void ResolveImageToImage::copyMSImageToMSImage (deUint32 copyArraySize)
{
	const DeviceInterface&			vk					= m_context.getDeviceInterface();
	const VkDevice					vkDevice			= m_context.getDevice();
	const VkQueue					queue				= m_context.getUniversalQueue();
	const tcu::TextureFormat		srcTcuFormat		= mapVkFormat(m_params.src.image.format);
	std::vector<VkImageCopy>		imageCopies;

	for (deUint32 layerNdx = 0; layerNdx < copyArraySize; ++layerNdx)
	{
		const VkImageSubresourceLayers	sourceSubresourceLayers	=
		{
			getAspectFlags(srcTcuFormat),	// VkImageAspectFlags	aspectMask;
			0u,								// deUint32				mipLevel;
			0u,								// deUint32				baseArrayLayer;
			1u								// deUint32				layerCount;
		};

		const VkImageSubresourceLayers	destinationSubresourceLayers	=
		{
			getAspectFlags(srcTcuFormat),	// VkImageAspectFlags	aspectMask;//getAspectFlags(dstTcuFormat)
			0u,								// deUint32				mipLevel;
			layerNdx,						// deUint32				baseArrayLayer;
			1u								// deUint32				layerCount;
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
				copyArraySize						// deUint32				arraySize;
			}
		},
	};

	const VkImageMemoryBarrier	postImageBarriers		=
	// destination image
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
			copyArraySize						// deUint32				arraySize;
		}
	};

	beginCommandBuffer(vk, *m_cmdBuffer);
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, DE_LENGTH_OF_ARRAY(imageBarriers), imageBarriers);
	vk.cmdCopyImage(*m_cmdBuffer, m_multisampledImage.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_multisampledCopyImage.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (deUint32)imageCopies.size(), imageCopies.data());
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1u, &postImageBarriers);
	endCommandBuffer(vk, *m_cmdBuffer);

	submitCommandsAndWait (vk, vkDevice, queue, *m_cmdBuffer);
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

	virtual void			checkSupport				(Context&				context) const
	{
		const VkSampleCountFlagBits	rasterizationSamples = m_params.samples;

		if (!(context.getDeviceProperties().limits.framebufferColorSampleCounts & rasterizationSamples))
			throw tcu::NotSupportedError("Unsupported number of rasterization samples");

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

std::string getImageLayoutCaseName (VkImageLayout layout)
{
	switch (layout)
	{
		case VK_IMAGE_LAYOUT_GENERAL:
			return "general";
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			return "optimal";
		default:
			DE_ASSERT(false);
			return "";
	}
}

const deInt32					defaultSize				= 64;
const deInt32					defaultHalfSize			= defaultSize / 2;
const deInt32					defaultFourthSize		= defaultSize / 4;
const deInt32					defaultSixteenthSize	= defaultSize / 16;
const VkExtent3D				defaultExtent			= {defaultSize, defaultSize, 1};
const VkExtent3D				defaultHalfExtent		= {defaultHalfSize, defaultHalfSize, 1};
const VkExtent3D				default1dExtent			= {defaultSize, 1, 1};
const VkExtent3D				default3dExtent			= {defaultFourthSize, defaultFourthSize, defaultFourthSize};

const VkImageSubresourceLayers	defaultSourceLayer		=
{
	VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
	0u,							// deUint32				mipLevel;
	0u,							// deUint32				baseArrayLayer;
	1u,							// deUint32				layerCount;
};

void addImageToImageSimpleTests (tcu::TestCaseGroup* group, AllocationKind allocationKind)
{
	tcu::TestContext& testCtx	= group->getTestContext();

	{
		TestParams	params;
		params.src.image.imageType			= VK_IMAGE_TYPE_2D;
		params.src.image.format				= VK_FORMAT_R8G8B8A8_UINT;
		params.src.image.extent				= defaultExtent;
		params.src.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
		params.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		params.dst.image.imageType			= VK_IMAGE_TYPE_2D;
		params.dst.image.format				= VK_FORMAT_R8G8B8A8_UINT;
		params.dst.image.extent				= defaultExtent;
		params.dst.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
		params.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		params.allocationKind				= allocationKind;

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

		group->addChild(new CopyImageToImageTestCase(testCtx, "whole_image", "Whole image", params));
	}

	{
		TestParams	params;
		params.src.image.imageType			= VK_IMAGE_TYPE_2D;
		params.src.image.format				= VK_FORMAT_R8G8B8A8_UINT;
		params.src.image.extent				= defaultExtent;
		params.src.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
		params.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		params.dst.image.imageType			= VK_IMAGE_TYPE_2D;
		params.dst.image.format				= VK_FORMAT_R32_UINT;
		params.dst.image.extent				= defaultExtent;
		params.dst.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
		params.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		params.allocationKind				= allocationKind;

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

		group->addChild(new CopyImageToImageTestCase(testCtx, "whole_image_diff_fromat", "Whole image with different format", params));
	}

	{
		TestParams	params;
		params.src.image.imageType			= VK_IMAGE_TYPE_2D;
		params.src.image.format				= VK_FORMAT_R8G8B8A8_UINT;
		params.src.image.extent				= defaultExtent;
		params.src.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
		params.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		params.dst.image.imageType			= VK_IMAGE_TYPE_2D;
		params.dst.image.format				= VK_FORMAT_R8G8B8A8_UINT;
		params.dst.image.extent				= defaultExtent;
		params.dst.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
		params.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		params.allocationKind				= allocationKind;

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

		group->addChild(new CopyImageToImageTestCase(testCtx, "partial_image", "Partial image", params));
	}

	{
		TestParams	params;
		params.src.image.imageType			= VK_IMAGE_TYPE_2D;
		params.src.image.format				= VK_FORMAT_D32_SFLOAT;
		params.src.image.extent				= defaultExtent;
		params.src.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
		params.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		params.dst.image.imageType			= VK_IMAGE_TYPE_2D;
		params.dst.image.format				= VK_FORMAT_D32_SFLOAT;
		params.dst.image.extent				= defaultExtent;
		params.dst.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
		params.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		params.allocationKind				= allocationKind;

		{
			const VkImageSubresourceLayers  sourceLayer =
			{
				VK_IMAGE_ASPECT_DEPTH_BIT,	// VkImageAspectFlags	aspectMask;
				0u,							// deUint32				mipLevel;
				0u,							// deUint32				baseArrayLayer;
				1u							// deUint32				layerCount;
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

		group->addChild(new CopyImageToImageTestCase(testCtx, "depth", "With depth", params));
	}

	{
		TestParams	params;
		params.src.image.imageType			= VK_IMAGE_TYPE_2D;
		params.src.image.format				= VK_FORMAT_S8_UINT;
		params.src.image.extent				= defaultExtent;
		params.src.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
		params.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		params.dst.image.imageType			= VK_IMAGE_TYPE_2D;
		params.dst.image.format				= VK_FORMAT_S8_UINT;
		params.dst.image.extent				= defaultExtent;
		params.dst.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
		params.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		params.allocationKind				= allocationKind;

		{
			const VkImageSubresourceLayers  sourceLayer =
			{
				VK_IMAGE_ASPECT_STENCIL_BIT,	// VkImageAspectFlags	aspectMask;
				0u,								// deUint32				mipLevel;
				0u,								// deUint32				baseArrayLayer;
				1u								// deUint32				layerCount;
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

		group->addChild(new CopyImageToImageTestCase(testCtx, "stencil", "With stencil", params));
	}
}

struct CopyColorTestParams
{
	TestParams		params;
	const VkFormat*	compatibleFormats;
};

void addImageToImageAllFormatsColorSrcFormatDstFormatTests (tcu::TestCaseGroup* group, TestParams params)
{
	const VkImageLayout copySrcLayouts[]		=
	{
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_IMAGE_LAYOUT_GENERAL
	};
	const VkImageLayout copyDstLayouts[]		=
	{
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_GENERAL
	};

	for (int srcLayoutNdx = 0u; srcLayoutNdx < DE_LENGTH_OF_ARRAY(copySrcLayouts); ++srcLayoutNdx)
	{
		params.src.image.operationLayout = copySrcLayouts[srcLayoutNdx];

		for (int dstLayoutNdx = 0u; dstLayoutNdx < DE_LENGTH_OF_ARRAY(copyDstLayouts); ++dstLayoutNdx)
		{
			params.dst.image.operationLayout = copyDstLayouts[dstLayoutNdx];

			const std::string testName	= getImageLayoutCaseName(params.src.image.operationLayout) + "_" +
										  getImageLayoutCaseName(params.dst.image.operationLayout);
			const std::string description	= "From layout " + getImageLayoutCaseName(params.src.image.operationLayout) +
											  " to " + getImageLayoutCaseName(params.dst.image.operationLayout);
			group->addChild(new CopyImageToImageTestCase(group->getTestContext(), testName, description, params));
		}
	}
}

bool isAllowedImageToImageAllFormatsColorSrcFormatTests(CopyColorTestParams& testParams)
{
	bool result = true;

	if (testParams.params.allocationKind == ALLOCATION_KIND_DEDICATED)
	{
		DE_ASSERT(!dedicatedAllocationImageToImageFormatsToTestSet.empty());

		result =
			de::contains(dedicatedAllocationImageToImageFormatsToTestSet, testParams.params.dst.image.format) ||
			de::contains(dedicatedAllocationImageToImageFormatsToTestSet, testParams.params.src.image.format);
	}

	return result;
}

void addImageToImageAllFormatsColorSrcFormatTests (tcu::TestCaseGroup* group, CopyColorTestParams testParams)
{
	for (int dstFormatIndex = 0; testParams.compatibleFormats[dstFormatIndex] != VK_FORMAT_UNDEFINED; ++dstFormatIndex)
	{
		testParams.params.dst.image.format = testParams.compatibleFormats[dstFormatIndex];

		const VkFormat		srcFormat	= testParams.params.src.image.format;
		const VkFormat		dstFormat	= testParams.params.dst.image.format;

		if (!isSupportedByFramework(dstFormat) && !isCompressedFormat(dstFormat))
			continue;

		if (!isAllowedImageToImageAllFormatsColorSrcFormatTests(testParams))
			continue;

		if (isCompressedFormat(srcFormat) && isCompressedFormat(dstFormat))
			if ((getBlockWidth(srcFormat) != getBlockWidth(dstFormat)) || (getBlockHeight(srcFormat) != getBlockHeight(dstFormat)))
				continue;

		const std::string	description	= "Copy to destination format " + getFormatCaseName(dstFormat);
		addTestGroup(group, getFormatCaseName(dstFormat), description, addImageToImageAllFormatsColorSrcFormatDstFormatTests, testParams.params);
	}
}

const VkFormat	compatibleFormats8Bit[]		=
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
const VkFormat	compatibleFormats16Bit[]	=
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
const VkFormat	compatibleFormats24Bit[]	=
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
const VkFormat	compatibleFormats32Bit[]	=
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
const VkFormat	compatibleFormats48Bit[]	=
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
const VkFormat	compatibleFormats64Bit[]	=
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

	VK_FORMAT_BC1_RGB_UNORM_BLOCK,
	VK_FORMAT_BC1_RGB_SRGB_BLOCK,
	VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
	VK_FORMAT_BC1_RGBA_SRGB_BLOCK,
	VK_FORMAT_BC4_UNORM_BLOCK,
	VK_FORMAT_BC4_SNORM_BLOCK,

	VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK,
	VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK,
	VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK,
	VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK,

	VK_FORMAT_EAC_R11_UNORM_BLOCK,
	VK_FORMAT_EAC_R11_SNORM_BLOCK,

	VK_FORMAT_UNDEFINED
};
const VkFormat	compatibleFormats96Bit[]	=
{
	VK_FORMAT_R32G32B32_UINT,
	VK_FORMAT_R32G32B32_SINT,
	VK_FORMAT_R32G32B32_SFLOAT,

	VK_FORMAT_UNDEFINED
};
const VkFormat	compatibleFormats128Bit[]	=
{
	VK_FORMAT_R32G32B32A32_UINT,
	VK_FORMAT_R32G32B32A32_SINT,
	VK_FORMAT_R32G32B32A32_SFLOAT,
	VK_FORMAT_R64G64_UINT,
	VK_FORMAT_R64G64_SINT,
	VK_FORMAT_R64G64_SFLOAT,

	VK_FORMAT_BC2_UNORM_BLOCK,
	VK_FORMAT_BC2_SRGB_BLOCK,
	VK_FORMAT_BC3_UNORM_BLOCK,
	VK_FORMAT_BC3_SRGB_BLOCK,
	VK_FORMAT_BC5_UNORM_BLOCK,
	VK_FORMAT_BC5_SNORM_BLOCK,
	VK_FORMAT_BC6H_UFLOAT_BLOCK,
	VK_FORMAT_BC6H_SFLOAT_BLOCK,
	VK_FORMAT_BC7_UNORM_BLOCK,
	VK_FORMAT_BC7_SRGB_BLOCK,

	VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK,
	VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK,

	VK_FORMAT_EAC_R11G11_UNORM_BLOCK,
	VK_FORMAT_EAC_R11G11_SNORM_BLOCK,

	VK_FORMAT_ASTC_4x4_UNORM_BLOCK,
	VK_FORMAT_ASTC_4x4_SRGB_BLOCK,
	VK_FORMAT_ASTC_5x4_UNORM_BLOCK,
	VK_FORMAT_ASTC_5x4_SRGB_BLOCK,
	VK_FORMAT_ASTC_5x5_UNORM_BLOCK,
	VK_FORMAT_ASTC_5x5_SRGB_BLOCK,
	VK_FORMAT_ASTC_6x5_UNORM_BLOCK,
	VK_FORMAT_ASTC_6x5_SRGB_BLOCK,
	VK_FORMAT_ASTC_6x6_UNORM_BLOCK,
	VK_FORMAT_ASTC_6x6_SRGB_BLOCK,
	VK_FORMAT_ASTC_8x5_UNORM_BLOCK,
	VK_FORMAT_ASTC_8x5_SRGB_BLOCK,
	VK_FORMAT_ASTC_8x6_UNORM_BLOCK,
	VK_FORMAT_ASTC_8x6_SRGB_BLOCK,
	VK_FORMAT_ASTC_8x8_UNORM_BLOCK,
	VK_FORMAT_ASTC_8x8_SRGB_BLOCK,
	VK_FORMAT_ASTC_10x5_UNORM_BLOCK,
	VK_FORMAT_ASTC_10x5_SRGB_BLOCK,
	VK_FORMAT_ASTC_10x6_UNORM_BLOCK,
	VK_FORMAT_ASTC_10x6_SRGB_BLOCK,
	VK_FORMAT_ASTC_10x8_UNORM_BLOCK,
	VK_FORMAT_ASTC_10x8_SRGB_BLOCK,
	VK_FORMAT_ASTC_10x10_UNORM_BLOCK,
	VK_FORMAT_ASTC_10x10_SRGB_BLOCK,
	VK_FORMAT_ASTC_12x10_UNORM_BLOCK,
	VK_FORMAT_ASTC_12x10_SRGB_BLOCK,
	VK_FORMAT_ASTC_12x12_UNORM_BLOCK,
	VK_FORMAT_ASTC_12x12_SRGB_BLOCK,

	VK_FORMAT_UNDEFINED
};
const VkFormat	compatibleFormats192Bit[]	=
{
	VK_FORMAT_R64G64B64_UINT,
	VK_FORMAT_R64G64B64_SINT,
	VK_FORMAT_R64G64B64_SFLOAT,

	VK_FORMAT_UNDEFINED
};
const VkFormat	compatibleFormats256Bit[]	=
{
	VK_FORMAT_R64G64B64A64_UINT,
	VK_FORMAT_R64G64B64A64_SINT,
	VK_FORMAT_R64G64B64A64_SFLOAT,

	VK_FORMAT_UNDEFINED
};

const VkFormat*	colorImageFormatsToTest[]	=
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
	compatibleFormats256Bit
};

const VkFormat	dedicatedAllocationImageToImageFormatsToTest[]	=
{
	// From compatibleFormats8Bit
	VK_FORMAT_R4G4_UNORM_PACK8,
	VK_FORMAT_R8_SRGB,

	// From compatibleFormats16Bit
	VK_FORMAT_R4G4B4A4_UNORM_PACK16,
	VK_FORMAT_R16_SFLOAT,

	// From compatibleFormats24Bit
	VK_FORMAT_R8G8B8_UNORM,
	VK_FORMAT_B8G8R8_SRGB,

	// From compatibleFormats32Bit
	VK_FORMAT_R8G8B8A8_UNORM,
	VK_FORMAT_R32_SFLOAT,

	// From compatibleFormats48Bit
	VK_FORMAT_R16G16B16_UNORM,
	VK_FORMAT_R16G16B16_SFLOAT,

	// From compatibleFormats64Bit
	VK_FORMAT_R16G16B16A16_UNORM,
	VK_FORMAT_R64_SFLOAT,

	// From compatibleFormats96Bit
	VK_FORMAT_R32G32B32_UINT,
	VK_FORMAT_R32G32B32_SFLOAT,

	// From compatibleFormats128Bit
	VK_FORMAT_R32G32B32A32_UINT,
	VK_FORMAT_R64G64_SFLOAT,

	// From compatibleFormats192Bit
	VK_FORMAT_R64G64B64_UINT,
	VK_FORMAT_R64G64B64_SFLOAT,

	// From compatibleFormats256Bit
	VK_FORMAT_R64G64B64A64_UINT,
	VK_FORMAT_R64G64B64A64_SFLOAT,
};

void addImageToImageAllFormatsColorTests (tcu::TestCaseGroup* group, AllocationKind allocationKind)
{
	TestParams	params;
	params.src.image.imageType	= VK_IMAGE_TYPE_2D;
	params.src.image.tiling		= VK_IMAGE_TILING_OPTIMAL;
	params.src.image.extent		= defaultExtent;
	params.dst.image.imageType	= VK_IMAGE_TYPE_2D;
	params.dst.image.tiling		= VK_IMAGE_TILING_OPTIMAL;
	params.dst.image.extent		= defaultExtent;
	params.allocationKind		= allocationKind;

	for (deInt32 i = 0; i < defaultSize; i += defaultFourthSize)
	{
		const VkImageCopy				testCopy =
		{
			defaultSourceLayer,								// VkImageSubresourceLayers	srcSubresource;
			{0, 0, 0},										// VkOffset3D				srcOffset;
			defaultSourceLayer,								// VkImageSubresourceLayers	dstSubresource;
			{i, defaultSize - i - defaultFourthSize, 0},	// VkOffset3D				dstOffset;
			{defaultFourthSize, defaultFourthSize, 1},		// VkExtent3D				extent;
		};

		CopyRegion	imageCopy;
		imageCopy.imageCopy = testCopy;

		params.regions.push_back(imageCopy);
	}

	if (allocationKind == ALLOCATION_KIND_DEDICATED)
	{
		const int numOfColorImageFormatsToTestFilter = DE_LENGTH_OF_ARRAY(colorImageFormatsToTest);
		for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < numOfColorImageFormatsToTestFilter; ++compatibleFormatsIndex)
			dedicatedAllocationImageToImageFormatsToTestSet.insert(dedicatedAllocationImageToImageFormatsToTest[compatibleFormatsIndex]);
	}

	const int numOfColorImageFormatsToTest = DE_LENGTH_OF_ARRAY(colorImageFormatsToTest);
	for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < numOfColorImageFormatsToTest; ++compatibleFormatsIndex)
	{
		const VkFormat*	compatibleFormats	= colorImageFormatsToTest[compatibleFormatsIndex];
		for (int srcFormatIndex = 0; compatibleFormats[srcFormatIndex] != VK_FORMAT_UNDEFINED; ++srcFormatIndex)
		{
			params.src.image.format = compatibleFormats[srcFormatIndex];
			if (!isSupportedByFramework(params.src.image.format) && !isCompressedFormat(params.src.image.format))
				continue;

			CopyColorTestParams	testParams;
			testParams.params				= params;
			testParams.compatibleFormats	= compatibleFormats;

			const std::string description	= "Copy from source format " + getFormatCaseName(params.src.image.format);
			addTestGroup(group, getFormatCaseName(params.src.image.format), description, addImageToImageAllFormatsColorSrcFormatTests, testParams);
		}
	}
}

void addImageToImageDimensionsTests (tcu::TestCaseGroup* group, AllocationKind allocationKind)
{
	tcu::TestContext&		testCtx				= group->getTestContext();

	const VkFormat			testFormats[][2]	=
	{
		// From compatibleFormats8Bit
		{
			VK_FORMAT_R4G4_UNORM_PACK8,
			VK_FORMAT_R8_SRGB
		},
		// From compatibleFormats16Bit
		{
			VK_FORMAT_R4G4B4A4_UNORM_PACK16,
			VK_FORMAT_R16_SFLOAT,
		},
		// From compatibleFormats24Bit
		{
			VK_FORMAT_R8G8B8_UNORM,
			VK_FORMAT_B8G8R8_SRGB
		},
		// From compatibleFormats32Bit
		{
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_FORMAT_R32_SFLOAT
		},
		// From compatibleFormats48Bit
		{
			VK_FORMAT_R16G16B16_UNORM,
			VK_FORMAT_R16G16B16_SFLOAT
		},
		// From compatibleFormats64Bit
		{
			VK_FORMAT_R16G16B16A16_UNORM,
			VK_FORMAT_R64_SFLOAT
		},
		// From compatibleFormats96Bit
		{
			VK_FORMAT_R32G32B32_UINT,
			VK_FORMAT_R32G32B32_SFLOAT
		},
		// From compatibleFormats128Bit
		{
			VK_FORMAT_R32G32B32A32_UINT,
			VK_FORMAT_R64G64_SFLOAT
		},
		// From compatibleFormats192Bit
		{
			VK_FORMAT_R64G64B64_UINT,
			VK_FORMAT_R64G64B64_SFLOAT,
		},
		// From compatibleFormats256Bit
		{
			VK_FORMAT_R64G64B64A64_UINT,
			VK_FORMAT_R64G64B64A64_SFLOAT
		}
	};

	const tcu::UVec2		imageDimensions[]	=
	{
		// large pot x small pot
		tcu::UVec2(4096,	4u),
		tcu::UVec2(8192,	4u),
		tcu::UVec2(16384,	4u),
		tcu::UVec2(32768,	4u),

		// large pot x small npot
		tcu::UVec2(4096,	6u),
		tcu::UVec2(8192,	6u),
		tcu::UVec2(16384,	6u),
		tcu::UVec2(32768,	6u),

		// small pot x large pot
		tcu::UVec2(4u, 4096),
		tcu::UVec2(4u, 8192),
		tcu::UVec2(4u, 16384),
		tcu::UVec2(4u, 32768),

		// small npot x large pot
		tcu::UVec2(6u, 4096),
		tcu::UVec2(6u, 8192),
		tcu::UVec2(6u, 16384),
		tcu::UVec2(6u, 32768)
	};

	const VkImageLayout		copySrcLayouts[]	=
	{
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_IMAGE_LAYOUT_GENERAL
	};

	const VkImageLayout		copyDstLayouts[]	=
	{
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_GENERAL
	};

	if (allocationKind == ALLOCATION_KIND_DEDICATED)
	{
		for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < DE_LENGTH_OF_ARRAY(testFormats); compatibleFormatsIndex++)
			dedicatedAllocationImageToImageFormatsToTestSet.insert(dedicatedAllocationImageToImageFormatsToTest[compatibleFormatsIndex]);
	}

	// Image dimensions
	for (size_t dimensionNdx = 0; dimensionNdx < DE_LENGTH_OF_ARRAY(imageDimensions); dimensionNdx++)
	{
		CopyRegion				copyRegion;
		CopyColorTestParams		testParams;

		const VkExtent3D		extent			= { imageDimensions[dimensionNdx].x(), imageDimensions[dimensionNdx].y(), 1 };

		const VkImageCopy		testCopy		=
		{
			defaultSourceLayer,	// VkImageSubresourceLayers	srcSubresource;
			{0, 0, 0},			// VkOffset3D				srcOffset;
			defaultSourceLayer,	// VkImageSubresourceLayers	dstSubresource;
			{0, 0, 0},			// VkOffset3D				dstOffset;
			extent,				// VkExtent3D				extent;
		};

		testParams.params.src.image.tiling		= VK_IMAGE_TILING_OPTIMAL;
		testParams.params.src.image.imageType	= VK_IMAGE_TYPE_2D;
		testParams.params.src.image.extent		= extent;

		testParams.params.dst.image.tiling		= VK_IMAGE_TILING_OPTIMAL;
		testParams.params.dst.image.imageType	= VK_IMAGE_TYPE_2D;
		testParams.params.dst.image.extent		= extent;

		copyRegion.imageCopy					= testCopy;
		testParams.params.allocationKind		= allocationKind;

		testParams.params.regions.push_back(copyRegion);

		const std::string	dimensionStr		= "src" + de::toString(testParams.params.src.image.extent.width) + "x" + de::toString(testParams.params.src.image.extent.height)
												  + "_dst" + de::toString(testParams.params.dst.image.extent.width) + "x" + de::toString(testParams.params.dst.image.extent.height);
		tcu::TestCaseGroup*	imageSizeGroup		= new tcu::TestCaseGroup(testCtx, dimensionStr.c_str(), ("Image sizes " + dimensionStr).c_str());

		// Compatible formats for copying
		for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < DE_LENGTH_OF_ARRAY(testFormats); compatibleFormatsIndex++)
		{
			const VkFormat* compatibleFormats = testFormats[compatibleFormatsIndex];

			testParams.compatibleFormats = compatibleFormats;

			// Source image format
			for (int srcFormatIndex = 0; srcFormatIndex < DE_LENGTH_OF_ARRAY(testFormats[compatibleFormatsIndex]); srcFormatIndex++)
			{
				testParams.params.src.image.format = testParams.compatibleFormats[srcFormatIndex];

				if (!isSupportedByFramework(testParams.params.src.image.format) && !isCompressedFormat(testParams.params.src.image.format))
					continue;

				const std::string	srcDescription	= "Copy from source format " + getFormatCaseName(testParams.params.src.image.format);
				tcu::TestCaseGroup*	srcFormatGroup	= new tcu::TestCaseGroup(testCtx, getFormatCaseName(testParams.params.src.image.format).c_str(), srcDescription.c_str());

				// Destination image format
				for (int dstFormatIndex = 0; dstFormatIndex < DE_LENGTH_OF_ARRAY(testFormats[compatibleFormatsIndex]); dstFormatIndex++)
				{
					testParams.params.dst.image.format = testParams.compatibleFormats[dstFormatIndex];

					if (!isSupportedByFramework(testParams.params.dst.image.format) && !isCompressedFormat(testParams.params.dst.image.format))
						continue;

					if (!isAllowedImageToImageAllFormatsColorSrcFormatTests(testParams))
						continue;

					if (isCompressedFormat(testParams.params.src.image.format) && isCompressedFormat(testParams.params.dst.image.format))
					{
						if ((getBlockWidth(testParams.params.src.image.format) != getBlockWidth(testParams.params.dst.image.format))
							|| (getBlockHeight(testParams.params.src.image.format) != getBlockHeight(testParams.params.dst.image.format)))
							continue;
					}

					const std::string	dstDescription	= "Copy to destination format " + getFormatCaseName(testParams.params.dst.image.format);
					tcu::TestCaseGroup*	dstFormatGroup	= new tcu::TestCaseGroup(testCtx, getFormatCaseName(testParams.params.dst.image.format).c_str(), dstDescription.c_str());

					// Source/destionation image layouts
					for (int srcLayoutNdx = 0u; srcLayoutNdx < DE_LENGTH_OF_ARRAY(copySrcLayouts); srcLayoutNdx++)
					{
						testParams.params.src.image.operationLayout = copySrcLayouts[srcLayoutNdx];

						for (int dstLayoutNdx = 0u; dstLayoutNdx < DE_LENGTH_OF_ARRAY(copyDstLayouts); dstLayoutNdx++)
						{
							testParams.params.dst.image.operationLayout = copyDstLayouts[dstLayoutNdx];

							const std::string	testName	= getImageLayoutCaseName(testParams.params.src.image.operationLayout) + "_" + getImageLayoutCaseName(testParams.params.dst.image.operationLayout);
							const std::string	description	= "From layout " + getImageLayoutCaseName(testParams.params.src.image.operationLayout) + " to " + getImageLayoutCaseName(testParams.params.dst.image.operationLayout);
							const TestParams	params		= testParams.params;

							dstFormatGroup->addChild(new CopyImageToImageTestCase(testCtx, testName, description, params));
						}
					}

					srcFormatGroup->addChild(dstFormatGroup);
				}

				imageSizeGroup->addChild(srcFormatGroup);
			}
		}

		group->addChild(imageSizeGroup);
	}
}

void addImageToImageAllFormatsDepthStencilFormatsTests (tcu::TestCaseGroup* group, TestParams params)
{
	const VkImageLayout copySrcLayouts[]		=
	{
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_IMAGE_LAYOUT_GENERAL
	};
	const VkImageLayout copyDstLayouts[]		=
	{
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_GENERAL
	};

	for (int srcLayoutNdx = 0u; srcLayoutNdx < DE_LENGTH_OF_ARRAY(copySrcLayouts); ++srcLayoutNdx)
	{
		params.src.image.operationLayout = copySrcLayouts[srcLayoutNdx];
		for (int dstLayoutNdx = 0u; dstLayoutNdx < DE_LENGTH_OF_ARRAY(copyDstLayouts); ++dstLayoutNdx)
		{
			params.dst.image.operationLayout = copyDstLayouts[dstLayoutNdx];

			const std::string testName		= getImageLayoutCaseName(params.src.image.operationLayout) + "_" +
											  getImageLayoutCaseName(params.dst.image.operationLayout);
			const std::string description	= "From layout " + getImageLayoutCaseName(params.src.image.operationLayout) +
											  " to " + getImageLayoutCaseName(params.dst.image.operationLayout);
			group->addChild(new CopyImageToImageTestCase(group->getTestContext(), testName, description, params));
		}
	}
}

void addImageToImageAllFormatsDepthStencilTests (tcu::TestCaseGroup* group, AllocationKind allocationKind)
{
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

	// 2D tests.
	for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < DE_LENGTH_OF_ARRAY(depthAndStencilFormats); ++compatibleFormatsIndex)
	{
		TestParams	params;
		params.src.image.imageType			= VK_IMAGE_TYPE_2D;
		params.dst.image.imageType			= VK_IMAGE_TYPE_2D;
		params.src.image.extent				= defaultExtent;
		params.dst.image.extent				= defaultExtent;
		params.src.image.format				= depthAndStencilFormats[compatibleFormatsIndex];
		params.dst.image.format				= params.src.image.format;
		params.src.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
		params.dst.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
		params.allocationKind				= allocationKind;
		params.separateDepthStencilLayouts	= DE_FALSE;

		bool hasDepth	= tcu::hasDepthComponent(mapVkFormat(params.src.image.format).order);
		bool hasStencil	= tcu::hasStencilComponent(mapVkFormat(params.src.image.format).order);

		const VkImageSubresourceLayers		defaultDepthSourceLayer		= { VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, 1u };
		const VkImageSubresourceLayers		defaultStencilSourceLayer	= { VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, 1u };

		for (deInt32 i = 0; i < defaultSize; i += defaultFourthSize)
		{
			CopyRegion			copyRegion;
			const VkOffset3D	srcOffset	= {0, 0, 0};
			const VkOffset3D	dstOffset	= {i, defaultSize - i - defaultFourthSize, 0};
			const VkExtent3D	extent		= {defaultFourthSize, defaultFourthSize, 1};

			if (hasDepth)
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
			if (hasStencil)
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

		const std::string testName		= "2d_"+ getFormatCaseName(params.src.image.format) + "_" + getFormatCaseName(params.dst.image.format);
		const std::string description	= "2D copy from " + getFormatCaseName(params.src.image.format) + " to " + getFormatCaseName(params.dst.image.format);
		addTestGroup(group, testName, description, addImageToImageAllFormatsDepthStencilFormatsTests, params);
	}

	// 1D tests.
	for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < DE_LENGTH_OF_ARRAY(depthAndStencilFormats); ++compatibleFormatsIndex)
	{
		TestParams	params;
		params.src.image.imageType			= VK_IMAGE_TYPE_1D;
		params.dst.image.imageType			= VK_IMAGE_TYPE_1D;
		params.src.image.extent				= default1dExtent;
		params.dst.image.extent				= default1dExtent;
		params.src.image.format				= depthAndStencilFormats[compatibleFormatsIndex];
		params.dst.image.format				= params.src.image.format;
		params.src.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
		params.dst.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
		params.allocationKind				= allocationKind;

		bool hasDepth	= tcu::hasDepthComponent(mapVkFormat(params.src.image.format).order);
		bool hasStencil	= tcu::hasStencilComponent(mapVkFormat(params.src.image.format).order);

		const VkImageSubresourceLayers		defaultDepthSourceLayer		= { VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, 1u };
		const VkImageSubresourceLayers		defaultStencilSourceLayer	= { VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, 1u };

		for (deInt32 i = defaultFourthSize; i < defaultSize; i += defaultSize / 2)
		{
			CopyRegion			copyRegion;
			const VkOffset3D	srcOffset	= {0, 0, 0};
			const VkOffset3D	dstOffset	= {i, 0, 0};
			const VkExtent3D	extent		= {defaultFourthSize, 1, 1};

			if (hasDepth)
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
			if (hasStencil)
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

		const std::string testName		= "1d_"+ getFormatCaseName(params.src.image.format) + "_" + getFormatCaseName(params.dst.image.format);
		const std::string description	= "1D copy from " + getFormatCaseName(params.src.image.format) + " to " + getFormatCaseName(params.dst.image.format);
		addTestGroup(group, testName, description, addImageToImageAllFormatsDepthStencilFormatsTests, params);
	}

	// 3D tests. Note we use smaller dimensions here for performance reasons.
	for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < DE_LENGTH_OF_ARRAY(depthAndStencilFormats); ++compatibleFormatsIndex)
	{
		TestParams	params;
		params.src.image.imageType			= VK_IMAGE_TYPE_3D;
		params.dst.image.imageType			= VK_IMAGE_TYPE_3D;
		params.src.image.extent				= default3dExtent;
		params.dst.image.extent				= default3dExtent;
		params.src.image.format				= depthAndStencilFormats[compatibleFormatsIndex];
		params.dst.image.format				= params.src.image.format;
		params.src.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
		params.dst.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
		params.allocationKind				= allocationKind;

		bool hasDepth	= tcu::hasDepthComponent(mapVkFormat(params.src.image.format).order);
		bool hasStencil	= tcu::hasStencilComponent(mapVkFormat(params.src.image.format).order);

		const VkImageSubresourceLayers		defaultDepthSourceLayer		= { VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, 1u };
		const VkImageSubresourceLayers		defaultStencilSourceLayer	= { VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, 1u };

		for (deInt32 i = 0; i < defaultFourthSize; i += defaultSixteenthSize)
		{
			CopyRegion			copyRegion;
			const VkOffset3D	srcOffset	= {0, 0, 0};
			const VkOffset3D	dstOffset	= {i, defaultFourthSize - i - defaultSixteenthSize, i};
			const VkExtent3D	extent		= {defaultSixteenthSize, defaultSixteenthSize, defaultSixteenthSize};

			if (hasDepth)
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
			if (hasStencil)
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

		const std::string testName		= "3d_"+ getFormatCaseName(params.src.image.format) + "_" + getFormatCaseName(params.dst.image.format);
		const std::string description	= "3D copy from " + getFormatCaseName(params.src.image.format) + " to " + getFormatCaseName(params.dst.image.format);
		addTestGroup(group, testName, description, addImageToImageAllFormatsDepthStencilFormatsTests, params);

		if (hasDepth && hasStencil)
		{
			params.separateDepthStencilLayouts	= DE_TRUE;
			const std::string testName2		= getFormatCaseName(params.src.image.format) + "_" + getFormatCaseName(params.dst.image.format) + "_separate_layouts";
			const std::string description2	= "Copy from " + getFormatCaseName(params.src.image.format) + " to " + getFormatCaseName(params.dst.image.format) + " with separate depth/stencil layouts";
			addTestGroup(group, testName2, description2, addImageToImageAllFormatsDepthStencilFormatsTests, params);
		}
	}
}

void addImageToImageAllFormatsTests (tcu::TestCaseGroup* group, AllocationKind allocationKind)
{
	addTestGroup(group, "color", "Copy image to image with color formats", addImageToImageAllFormatsColorTests, allocationKind);
	addTestGroup(group, "depth_stencil", "Copy image to image with depth/stencil formats", addImageToImageAllFormatsDepthStencilTests, allocationKind);
}

void addImageToImage3dImagesTests (tcu::TestCaseGroup* group, AllocationKind allocationKind)
{
	tcu::TestContext& testCtx	= group->getTestContext();

	{
		TestParams	params3DTo2D;
		const deUint32	slicesLayers			= 16u;
		params3DTo2D.src.image.imageType		= VK_IMAGE_TYPE_3D;
		params3DTo2D.src.image.format			= VK_FORMAT_R8G8B8A8_UINT;
		params3DTo2D.src.image.extent			= defaultHalfExtent;
		params3DTo2D.src.image.extent.depth		= slicesLayers;
		params3DTo2D.src.image.tiling			= VK_IMAGE_TILING_OPTIMAL;
		params3DTo2D.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		params3DTo2D.dst.image.imageType		= VK_IMAGE_TYPE_2D;
		params3DTo2D.dst.image.format			= VK_FORMAT_R8G8B8A8_UINT;
		params3DTo2D.dst.image.extent			= defaultHalfExtent;
		params3DTo2D.dst.image.extent.depth		= slicesLayers;
		params3DTo2D.dst.image.tiling			= VK_IMAGE_TILING_OPTIMAL;
		params3DTo2D.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		params3DTo2D.allocationKind				= allocationKind;

		for (deUint32 slicesLayersNdx = 0; slicesLayersNdx < slicesLayers; ++slicesLayersNdx)
		{
			const VkImageSubresourceLayers	sourceLayer	=
			{
				VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
				0u,							// deUint32				mipLevel;
				0u,							// deUint32				baseArrayLayer;
				1u							// deUint32				layerCount;
			};

			const VkImageSubresourceLayers	destinationLayer	=
			{
				VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
				0u,							// deUint32				mipLevel;
				slicesLayersNdx,			// deUint32				baseArrayLayer;
				1u							// deUint32				layerCount;
			};

			const VkImageCopy				testCopy	=
			{
				sourceLayer,						// VkImageSubresourceLayers	srcSubresource;
				{0, 0, (deInt32)slicesLayersNdx},	// VkOffset3D					srcOffset;
				destinationLayer,					// VkImageSubresourceLayers	dstSubresource;
				{0, 0, 0},							// VkOffset3D					dstOffset;
				defaultHalfExtent,					// VkExtent3D					extent;
			};

			CopyRegion	imageCopy;
			imageCopy.imageCopy	= testCopy;

			params3DTo2D.regions.push_back(imageCopy);
		}
		group->addChild(new CopyImageToImageTestCase(testCtx, "3d_to_2d_by_slices", "copy 2d layers to 3d slices one by one", params3DTo2D));
	}

	{
		TestParams	params2DTo3D;
		const deUint32	slicesLayers			= 16u;
		params2DTo3D.src.image.imageType		= VK_IMAGE_TYPE_2D;
		params2DTo3D.src.image.format			= VK_FORMAT_R8G8B8A8_UINT;
		params2DTo3D.src.image.extent			= defaultHalfExtent;
		params2DTo3D.src.image.extent.depth		= slicesLayers;
		params2DTo3D.src.image.tiling			= VK_IMAGE_TILING_OPTIMAL;
		params2DTo3D.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		params2DTo3D.dst.image.imageType		= VK_IMAGE_TYPE_3D;
		params2DTo3D.dst.image.format			= VK_FORMAT_R8G8B8A8_UINT;
		params2DTo3D.dst.image.extent			= defaultHalfExtent;
		params2DTo3D.dst.image.extent.depth		= slicesLayers;
		params2DTo3D.dst.image.tiling			= VK_IMAGE_TILING_OPTIMAL;
		params2DTo3D.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		params2DTo3D.allocationKind				= allocationKind;

		for (deUint32 slicesLayersNdx = 0; slicesLayersNdx < slicesLayers; ++slicesLayersNdx)
		{
			const VkImageSubresourceLayers	sourceLayer	=
			{
				VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
				0u,							// deUint32				mipLevel;
				slicesLayersNdx,			// deUint32				baseArrayLayer;
				1u							// deUint32				layerCount;
			};

			const VkImageSubresourceLayers	destinationLayer	=
			{
				VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
				0u,							// deUint32				mipLevel;
				0u,							// deUint32				baseArrayLayer;
				1u							// deUint32				layerCount;
			};

			const VkImageCopy				testCopy	=
			{
				sourceLayer,						// VkImageSubresourceLayers	srcSubresource;
				{0, 0, 0},							// VkOffset3D				srcOffset;
				destinationLayer,					// VkImageSubresourceLayers	dstSubresource;
				{0, 0, (deInt32)slicesLayersNdx},	// VkOffset3D				dstOffset;
				defaultHalfExtent,					// VkExtent3D				extent;
			};

			CopyRegion	imageCopy;
			imageCopy.imageCopy	= testCopy;

			params2DTo3D.regions.push_back(imageCopy);
		}

		group->addChild(new CopyImageToImageTestCase(testCtx, "2d_to_3d_by_layers", "copy 3d slices to 2d layers one by one", params2DTo3D));
	}

	{
		TestParams	params3DTo2D;
		const deUint32	slicesLayers			= 16u;
		params3DTo2D.src.image.imageType		= VK_IMAGE_TYPE_3D;
		params3DTo2D.src.image.format			= VK_FORMAT_R8G8B8A8_UINT;
		params3DTo2D.src.image.extent			= defaultHalfExtent;
		params3DTo2D.src.image.extent.depth		= slicesLayers;
		params3DTo2D.src.image.tiling			= VK_IMAGE_TILING_OPTIMAL;
		params3DTo2D.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		params3DTo2D.dst.image.imageType		= VK_IMAGE_TYPE_2D;
		params3DTo2D.dst.image.format			= VK_FORMAT_R8G8B8A8_UINT;
		params3DTo2D.dst.image.extent			= defaultHalfExtent;
		params3DTo2D.dst.image.extent.depth		= slicesLayers;
		params3DTo2D.dst.image.tiling			= VK_IMAGE_TILING_OPTIMAL;
		params3DTo2D.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		params3DTo2D.allocationKind				= allocationKind;

		{
			const VkImageSubresourceLayers	sourceLayer	=
			{
				VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
				0u,							// deUint32				mipLevel;
				0u,							// deUint32				baseArrayLayer;
				1u							// deUint32				layerCount;
			};

			const VkImageSubresourceLayers	destinationLayer	=
			{
				VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
				0u,							// deUint32				mipLevel;
				0,							// deUint32				baseArrayLayer;
				slicesLayers				// deUint32				layerCount;
			};

			const VkImageCopy				testCopy	=
			{
				sourceLayer,					// VkImageSubresourceLayers	srcSubresource;
				{0, 0, 0},						// VkOffset3D				srcOffset;
				destinationLayer,				// VkImageSubresourceLayers	dstSubresource;
				{0, 0, 0},						// VkOffset3D				dstOffset;
				params3DTo2D.src.image.extent	// VkExtent3D				extent;
			};

			CopyRegion	imageCopy;
			imageCopy.imageCopy	= testCopy;

			params3DTo2D.regions.push_back(imageCopy);
		}
		group->addChild(new CopyImageToImageTestCase(testCtx, "3d_to_2d_whole", "copy 3d slices to 2d layers all at once", params3DTo2D));
	}

	{
		TestParams	params2DTo3D;
		const deUint32	slicesLayers			= 16u;
		params2DTo3D.src.image.imageType		= VK_IMAGE_TYPE_2D;
		params2DTo3D.src.image.format			= VK_FORMAT_R8G8B8A8_UINT;
		params2DTo3D.src.image.extent			= defaultHalfExtent;
		params2DTo3D.src.image.extent.depth		= slicesLayers;
		params2DTo3D.src.image.tiling			= VK_IMAGE_TILING_OPTIMAL;
		params2DTo3D.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		params2DTo3D.dst.image.imageType		= VK_IMAGE_TYPE_3D;
		params2DTo3D.dst.image.format			= VK_FORMAT_R8G8B8A8_UINT;
		params2DTo3D.dst.image.extent			= defaultHalfExtent;
		params2DTo3D.dst.image.extent.depth		= slicesLayers;
		params2DTo3D.dst.image.tiling			= VK_IMAGE_TILING_OPTIMAL;
		params2DTo3D.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		params2DTo3D.allocationKind				= allocationKind;

		{
			const VkImageSubresourceLayers	sourceLayer	=
			{
				VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
				0u,							// deUint32				mipLevel;
				0u,							// deUint32				baseArrayLayer;
				slicesLayers				// deUint32				layerCount;
			};

			const VkImageSubresourceLayers	destinationLayer	=
			{
				VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
				0u,							// deUint32				mipLevel;
				0u,							// deUint32				baseArrayLayer;
				1u							// deUint32				layerCount;
			};

			const VkImageCopy				testCopy	=
			{
				sourceLayer,					// VkImageSubresourceLayers	srcSubresource;
				{0, 0, 0},						// VkOffset3D				srcOffset;
				destinationLayer,				// VkImageSubresourceLayers	dstSubresource;
				{0, 0, 0},						// VkOffset3D				dstOffset;
				params2DTo3D.src.image.extent,	// VkExtent3D				extent;
			};

			CopyRegion	imageCopy;
			imageCopy.imageCopy	= testCopy;

			params2DTo3D.regions.push_back(imageCopy);
		}

		group->addChild(new CopyImageToImageTestCase(testCtx, "2d_to_3d_whole", "copy 2d layers to 3d slices all at once", params2DTo3D));
	}

	{
		TestParams	params3DTo2D;
		const deUint32	slicesLayers			= 16u;
		params3DTo2D.src.image.imageType		= VK_IMAGE_TYPE_3D;
		params3DTo2D.src.image.format			= VK_FORMAT_R8G8B8A8_UINT;
		params3DTo2D.src.image.extent			= defaultHalfExtent;
		params3DTo2D.src.image.extent.depth		= slicesLayers;
		params3DTo2D.src.image.tiling			= VK_IMAGE_TILING_OPTIMAL;
		params3DTo2D.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		params3DTo2D.dst.image.imageType		= VK_IMAGE_TYPE_2D;
		params3DTo2D.dst.image.format			= VK_FORMAT_R8G8B8A8_UINT;
		params3DTo2D.dst.image.extent			= defaultHalfExtent;
		params3DTo2D.dst.image.extent.depth		= slicesLayers;
		params3DTo2D.dst.image.tiling			= VK_IMAGE_TILING_OPTIMAL;
		params3DTo2D.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		params3DTo2D.allocationKind				= allocationKind;

		const deUint32 regionWidth				= defaultHalfExtent.width / slicesLayers -1;
		const deUint32 regionHeight				= defaultHalfExtent.height / slicesLayers -1 ;

		for (deUint32 slicesLayersNdx = 0; slicesLayersNdx < slicesLayers; ++slicesLayersNdx)
		{
			const VkImageSubresourceLayers	sourceLayer	=
			{
				VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
				0u,							// deUint32				mipLevel;
				0u,							// deUint32				baseArrayLayer;
				1u							// deUint32				layerCount;
			};

			const VkImageSubresourceLayers	destinationLayer	=
			{
					VK_IMAGE_ASPECT_COLOR_BIT,		// VkImageAspectFlags	aspectMask;
					0u,								// deUint32				mipLevel;
					slicesLayersNdx,				// deUint32				baseArrayLayer;
					1u								// deUint32				layerCount;
			};


			const VkImageCopy				testCopy	=
			{
				sourceLayer,															// VkImageSubresourceLayers	srcSubresource;
				{0, (deInt32)(regionHeight*slicesLayersNdx), (deInt32)slicesLayersNdx},	// VkOffset3D				srcOffset;
					destinationLayer,													// VkImageSubresourceLayers	dstSubresource;
					{(deInt32)(regionWidth*slicesLayersNdx), 0, 0},						// VkOffset3D				dstOffset;
					{
						(defaultHalfExtent.width - regionWidth*slicesLayersNdx),
						(defaultHalfExtent.height - regionHeight*slicesLayersNdx),
						1
					}																	// VkExtent3D				extent;
			};

			CopyRegion	imageCopy;
			imageCopy.imageCopy = testCopy;
			params3DTo2D.regions.push_back(imageCopy);
		}
		group->addChild(new CopyImageToImageTestCase(testCtx, "3d_to_2d_regions", "copy 3d slices regions to 2d layers", params3DTo2D));
	}

	{
		TestParams	params2DTo3D;
		const deUint32	slicesLayers			= 16u;
		params2DTo3D.src.image.imageType		= VK_IMAGE_TYPE_2D;
		params2DTo3D.src.image.format			= VK_FORMAT_R8G8B8A8_UINT;
		params2DTo3D.src.image.extent			= defaultHalfExtent;
		params2DTo3D.src.image.extent.depth		= slicesLayers;
		params2DTo3D.src.image.tiling			= VK_IMAGE_TILING_OPTIMAL;
		params2DTo3D.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		params2DTo3D.dst.image.imageType		= VK_IMAGE_TYPE_3D;
		params2DTo3D.dst.image.format			= VK_FORMAT_R8G8B8A8_UINT;
		params2DTo3D.dst.image.extent			= defaultHalfExtent;
		params2DTo3D.dst.image.extent.depth		= slicesLayers;
		params2DTo3D.dst.image.tiling			= VK_IMAGE_TILING_OPTIMAL;
		params2DTo3D.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		params2DTo3D.allocationKind				= allocationKind;

		const deUint32 regionWidth				= defaultHalfExtent.width / slicesLayers -1;
		const deUint32 regionHeight				= defaultHalfExtent.height / slicesLayers -1 ;

		for (deUint32 slicesLayersNdx = 0; slicesLayersNdx < slicesLayers; ++slicesLayersNdx)
		{
			const VkImageSubresourceLayers	sourceLayer	=
			{
				VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
				0u,							// deUint32				mipLevel;
				slicesLayersNdx,			// deUint32				baseArrayLayer;
				1u							// deUint32				layerCount;
			};

			const VkImageSubresourceLayers	destinationLayer	=
			{
				VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
				0u,							// deUint32				mipLevel;
				0u,							// deUint32				baseArrayLayer;
				1u							// deUint32				layerCount;
			};

			const VkImageCopy				testCopy	=
			{
				sourceLayer,																// VkImageSubresourceLayers	srcSubresource;
				{(deInt32)(regionWidth*slicesLayersNdx), 0, 0},								// VkOffset3D				srcOffset;
				destinationLayer,															// VkImageSubresourceLayers	dstSubresource;
				{0, (deInt32)(regionHeight*slicesLayersNdx), (deInt32)(slicesLayersNdx)},	// VkOffset3D				dstOffset;
				{
					defaultHalfExtent.width - regionWidth*slicesLayersNdx,
					defaultHalfExtent.height - regionHeight*slicesLayersNdx,
					1
				}																			// VkExtent3D				extent;
			};

			CopyRegion	imageCopy;
			imageCopy.imageCopy	= testCopy;

			params2DTo3D.regions.push_back(imageCopy);
		}

		group->addChild(new CopyImageToImageTestCase(testCtx, "2d_to_3d_regions", "copy 2d layers regions to 3d slices", params2DTo3D));
	}
}

void addImageToImageTests (tcu::TestCaseGroup* group, AllocationKind allocationKind)
{
	addTestGroup(group, "simple_tests", "Copy from image to image simple tests", addImageToImageSimpleTests, allocationKind);
	addTestGroup(group, "all_formats", "Copy from image to image with all compatible formats", addImageToImageAllFormatsTests, allocationKind);
	addTestGroup(group, "3d_images", "Coping operations on 3d images", addImageToImage3dImagesTests, allocationKind);
	addTestGroup(group, "dimensions", "Copying operations on different image dimensions", addImageToImageDimensionsTests, allocationKind);
}

void addImageToBufferTests (tcu::TestCaseGroup* group, AllocationKind allocationKind)
{
	tcu::TestContext& testCtx	= group->getTestContext();

	{
		TestParams	params;
		params.src.image.imageType			= VK_IMAGE_TYPE_2D;
		params.src.image.format				= VK_FORMAT_R8G8B8A8_UNORM;
		params.src.image.extent				= defaultExtent;
		params.src.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
		params.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		params.dst.buffer.size				= defaultSize * defaultSize;
		params.allocationKind				= allocationKind;

		const VkBufferImageCopy	bufferImageCopy	=
		{
			0u,											// VkDeviceSize				bufferOffset;
			0u,											// deUint32					bufferRowLength;
			0u,											// deUint32					bufferImageHeight;
			defaultSourceLayer,							// VkImageSubresourceLayers	imageSubresource;
			{0, 0, 0},									// VkOffset3D				imageOffset;
			defaultExtent								// VkExtent3D				imageExtent;
		};
		CopyRegion	copyRegion;
		copyRegion.bufferImageCopy	= bufferImageCopy;

		params.regions.push_back(copyRegion);

		group->addChild(new CopyImageToBufferTestCase(testCtx, "whole", "Copy from image to buffer", params));
	}

	{
		TestParams	params;
		params.src.image.imageType			= VK_IMAGE_TYPE_2D;
		params.src.image.format				= VK_FORMAT_R8G8B8A8_UNORM;
		params.src.image.extent				= defaultExtent;
		params.src.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
		params.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		params.dst.buffer.size				= defaultSize * defaultSize;
		params.allocationKind				= allocationKind;

		const VkBufferImageCopy	bufferImageCopy	=
		{
			defaultSize * defaultHalfSize,				// VkDeviceSize				bufferOffset;
			0u,											// deUint32					bufferRowLength;
			0u,											// deUint32					bufferImageHeight;
			defaultSourceLayer,							// VkImageSubresourceLayers	imageSubresource;
			{defaultFourthSize, defaultFourthSize, 0},	// VkOffset3D				imageOffset;
			defaultHalfExtent							// VkExtent3D				imageExtent;
		};
		CopyRegion	copyRegion;
		copyRegion.bufferImageCopy	= bufferImageCopy;

		params.regions.push_back(copyRegion);

		group->addChild(new CopyImageToBufferTestCase(testCtx, "buffer_offset", "Copy from image to buffer with buffer offset", params));
	}

	{
		TestParams	params;
		params.src.image.imageType			= VK_IMAGE_TYPE_2D;
		params.src.image.format				= VK_FORMAT_R8G8B8A8_UNORM;
		params.src.image.extent				= defaultExtent;
		params.src.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
		params.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		params.dst.buffer.size				= defaultSize * defaultSize;
		params.allocationKind				= allocationKind;

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
				bufferRowLength,			// deUint32					bufferRowLength;
				bufferImageHeight,			// deUint32					bufferImageHeight;
				defaultSourceLayer,			// VkImageSubresourceLayers	imageSubresource;
				{0, 0, 0},					// VkOffset3D				imageOffset;
				imageExtent					// VkExtent3D				imageExtent;
			};
			region.bufferImageCopy	= bufferImageCopy;
			params.regions.push_back(region);
		}

		group->addChild(new CopyImageToBufferTestCase(testCtx, "regions", "Copy from image to buffer with multiple regions", params));
	}

	{
		TestParams				params;
		params.src.image.imageType			= VK_IMAGE_TYPE_2D;
		params.src.image.format				= VK_FORMAT_R8G8B8A8_UNORM;
		params.src.image.extent				= defaultExtent;
		params.src.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
		params.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		params.dst.buffer.size				= (defaultHalfSize - 1u) * defaultSize + defaultHalfSize;
		params.allocationKind				= allocationKind;

		const VkBufferImageCopy	bufferImageCopy	=
		{
			0u,											// VkDeviceSize				bufferOffset;
			defaultSize,								// deUint32					bufferRowLength;
			defaultSize,								// deUint32					bufferImageHeight;
			defaultSourceLayer,							// VkImageSubresourceLayers	imageSubresource;
			{defaultFourthSize, defaultFourthSize, 0},	// VkOffset3D				imageOffset;
			defaultHalfExtent							// VkExtent3D				imageExtent;
		};
		CopyRegion				copyRegion;
		copyRegion.bufferImageCopy	= bufferImageCopy;

		params.regions.push_back(copyRegion);

		group->addChild(new CopyImageToBufferTestCase(testCtx, "tightly_sized_buffer", "Copy from image to a buffer that is just large enough to contain the data", params));
	}

	{
		TestParams				params;
		params.src.image.imageType			= VK_IMAGE_TYPE_2D;
		params.src.image.format				= VK_FORMAT_R8G8B8A8_UNORM;
		params.src.image.extent				= defaultExtent;
		params.src.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
		params.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		params.dst.buffer.size				= (defaultHalfSize - 1u) * defaultSize + defaultHalfSize + defaultFourthSize;
		params.allocationKind				= allocationKind;

		const VkBufferImageCopy	bufferImageCopy	=
		{
			defaultFourthSize,							// VkDeviceSize				bufferOffset;
			defaultSize,								// deUint32					bufferRowLength;
			defaultSize,								// deUint32					bufferImageHeight;
			defaultSourceLayer,							// VkImageSubresourceLayers	imageSubresource;
			{defaultFourthSize, defaultFourthSize, 0},	// VkOffset3D				imageOffset;
			defaultHalfExtent							// VkExtent3D				imageExtent;
		};
		CopyRegion				copyRegion;
		copyRegion.bufferImageCopy	= bufferImageCopy;

		params.regions.push_back(copyRegion);

		group->addChild(new CopyImageToBufferTestCase(testCtx, "tightly_sized_buffer_offset", "Copy from image to a buffer that is just large enough to contain the data", params));
	}
}

void addBufferToDepthStencilTests(tcu::TestCaseGroup* group, AllocationKind allocationKind)
{
	tcu::TestContext& testCtx = group->getTestContext();

	const struct
	{
		const char* name;
		const VkFormat							format;
	} depthAndStencilFormats[] =
	{
		{ "d16_unorm",				VK_FORMAT_D16_UNORM				},
		{ "x8_d24_unorm_pack32",	VK_FORMAT_X8_D24_UNORM_PACK32	},
		{ "d32_sfloat",				VK_FORMAT_D32_SFLOAT			},
		{ "d16_unorm_s8_uint",		VK_FORMAT_D16_UNORM_S8_UINT		},
		{ "d24_unorm_s8_uint",		VK_FORMAT_D24_UNORM_S8_UINT		},
		{ "d32_sfloat_s8_uint",		VK_FORMAT_D32_SFLOAT_S8_UINT	}
	};

	const VkImageSubresourceLayers	depthSourceLayer =
	{
		VK_IMAGE_ASPECT_DEPTH_BIT,		// VkImageAspectFlags	aspectMask;
		0u,							// deUint32				mipLevel;
		0u,							// deUint32				baseArrayLayer;
		1u,							// deUint32				layerCount;
	};

	const VkBufferImageCopy	bufferDepthCopy =
	{
		0u,											// VkDeviceSize				bufferOffset;
		0u,											// deUint32					bufferRowLength;
		0u,											// deUint32					bufferImageHeight;
		depthSourceLayer,							// VkImageSubresourceLayers	imageSubresource;
		{0, 0, 0},									// VkOffset3D				imageOffset;
		defaultExtent								// VkExtent3D				imageExtent;
	};
	CopyRegion	copyDepthRegion;
	copyDepthRegion.bufferImageCopy = bufferDepthCopy;

	const VkImageSubresourceLayers	stencilSourceLayer =
	{
		VK_IMAGE_ASPECT_STENCIL_BIT,		// VkImageAspectFlags	aspectMask;
		0u,							// deUint32				mipLevel;
		0u,							// deUint32				baseArrayLayer;
		1u,							// deUint32				layerCount;
	};

	const VkBufferImageCopy	bufferStencilCopy =
	{
		0u,											// VkDeviceSize				bufferOffset;
		0u,											// deUint32					bufferRowLength;
		0u,											// deUint32					bufferImageHeight;
		stencilSourceLayer,							// VkImageSubresourceLayers	imageSubresource;
		{0, 0, 0},									// VkOffset3D				imageOffset;
		defaultExtent								// VkExtent3D				imageExtent;
	};

	CopyRegion	copyStencilRegion;
	copyStencilRegion.bufferImageCopy = bufferStencilCopy;

	// Note: Depth stencil tests I want to do
	// Formats: D16, D24S8, D32FS8
	// Test writing each component with separate CopyBufferToImage commands
	// Test writing both components in one CopyBufferToImage command
	// Swap order of writes of Depth & Stencil
	// whole surface, subimages?
	// Similar tests as BufferToImage?
	for (const auto config : depthAndStencilFormats)
	{
		// TODO: Check that this format is supported before creating tests?
		//if (isSupportedDepthStencilFormat(vki, physDevice, VK_FORMAT_D24_UNORM_S8_UINT))

		const tcu::TextureFormat format = mapVkFormat(config.format);
		const bool hasDepth = tcu::hasDepthComponent(format.order);
		const bool hasStencil = tcu::hasStencilComponent(format.order);
		std::string description = config.name;

		TestParams	params;
		params.src.buffer.size = defaultSize * defaultSize;
		params.dst.image.imageType = VK_IMAGE_TYPE_2D;
		params.dst.image.format = config.format;
		params.dst.image.extent = defaultExtent;
		params.dst.image.tiling = VK_IMAGE_TILING_OPTIMAL;
		params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		params.allocationKind = allocationKind;

		if (hasDepth && hasStencil)
		{
			params.singleCommand = DE_TRUE;

			params.regions.push_back(copyDepthRegion);
			params.regions.push_back(copyStencilRegion);

			group->addChild(new CopyBufferToDepthStencilTestCase(testCtx, description + "_DS", "Copy from depth&stencil to image", params));

			params.singleCommand = DE_FALSE;

			group->addChild(new CopyBufferToDepthStencilTestCase(testCtx, description + "_D_S", "Copy from depth then stencil to image", params));

			params.regions.clear();
			params.regions.push_back(copyStencilRegion);
			params.regions.push_back(copyDepthRegion);

			group->addChild(new CopyBufferToDepthStencilTestCase(testCtx, description + "_S_D", "Copy from depth then stencil to image", params));

			params.singleCommand = DE_TRUE;
			group->addChild(new CopyBufferToDepthStencilTestCase(testCtx, description + "_SD", "Copy from depth&stencil to image", params));

		}

		if (hasStencil)
		{
			params.regions.clear();
			params.regions.push_back(copyStencilRegion);

			group->addChild(new CopyBufferToDepthStencilTestCase(testCtx, description + "_S", "Copy from stencil to image", params));
		}


		if (hasDepth)
		{
			params.regions.clear();
			params.regions.push_back(copyDepthRegion);

			group->addChild(new CopyBufferToDepthStencilTestCase(testCtx, description + "_D", "Copy from depth to image", params));
		}
	}
}

void addBufferToImageTests (tcu::TestCaseGroup* group, AllocationKind allocationKind)
{
	tcu::TestContext& testCtx	= group->getTestContext();

	{
		TestParams	params;
		params.src.buffer.size				= defaultSize * defaultSize;
		params.dst.image.imageType			= VK_IMAGE_TYPE_2D;
		params.dst.image.format				= VK_FORMAT_R8G8B8A8_UINT;
		params.dst.image.extent				= defaultExtent;
		params.dst.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
		params.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		params.allocationKind				= allocationKind;

		const VkBufferImageCopy	bufferImageCopy	=
		{
			0u,											// VkDeviceSize				bufferOffset;
			0u,											// deUint32					bufferRowLength;
			0u,											// deUint32					bufferImageHeight;
			defaultSourceLayer,							// VkImageSubresourceLayers	imageSubresource;
			{0, 0, 0},									// VkOffset3D				imageOffset;
			defaultExtent								// VkExtent3D				imageExtent;
		};
		CopyRegion	copyRegion;
		copyRegion.bufferImageCopy	= bufferImageCopy;

		params.regions.push_back(copyRegion);

		group->addChild(new CopyBufferToImageTestCase(testCtx, "whole", "Copy from buffer to image", params));
	}

	{
		TestParams	params;
		params.src.buffer.size				= defaultSize * defaultSize;
		params.dst.image.imageType			= VK_IMAGE_TYPE_2D;
		params.dst.image.format				= VK_FORMAT_R8G8B8A8_UNORM;
		params.dst.image.extent				= defaultExtent;
		params.dst.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
		params.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		params.allocationKind				= allocationKind;

		CopyRegion	region;
		deUint32	divisor	= 1;
		for (int offset = 0; (offset + defaultFourthSize / divisor < defaultSize) && (defaultFourthSize > divisor); offset += defaultFourthSize / divisor++)
		{
			const VkBufferImageCopy	bufferImageCopy	=
			{
				0u,																// VkDeviceSize				bufferOffset;
				0u,																// deUint32					bufferRowLength;
				0u,																// deUint32					bufferImageHeight;
				defaultSourceLayer,												// VkImageSubresourceLayers	imageSubresource;
				{offset, defaultHalfSize, 0},									// VkOffset3D				imageOffset;
				{defaultFourthSize / divisor, defaultFourthSize / divisor, 1}	// VkExtent3D				imageExtent;
			};
			region.bufferImageCopy	= bufferImageCopy;
			params.regions.push_back(region);
		}

		group->addChild(new CopyBufferToImageTestCase(testCtx, "regions", "Copy from buffer to image with multiple regions", params));
	}

	{
		TestParams	params;
		params.src.buffer.size				= defaultSize * defaultSize;
		params.dst.image.imageType			= VK_IMAGE_TYPE_2D;
		params.dst.image.format				= VK_FORMAT_R8G8B8A8_UNORM;
		params.dst.image.extent				= defaultExtent;
		params.dst.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
		params.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		params.allocationKind				= allocationKind;

		const VkBufferImageCopy	bufferImageCopy	=
		{
			defaultFourthSize,							// VkDeviceSize				bufferOffset;
			defaultHalfSize + defaultFourthSize,		// deUint32					bufferRowLength;
			defaultHalfSize + defaultFourthSize,		// deUint32					bufferImageHeight;
			defaultSourceLayer,							// VkImageSubresourceLayers	imageSubresource;
			{defaultFourthSize, defaultFourthSize, 0},	// VkOffset3D				imageOffset;
			defaultHalfExtent							// VkExtent3D				imageExtent;
		};
		CopyRegion	copyRegion;
		copyRegion.bufferImageCopy	= bufferImageCopy;

		params.regions.push_back(copyRegion);

		group->addChild(new CopyBufferToImageTestCase(testCtx, "buffer_offset", "Copy from buffer to image with buffer offset", params));
	}

	{
		TestParams				params;
		params.src.buffer.size				= (defaultHalfSize - 1u) * defaultSize + defaultHalfSize;
		params.dst.image.imageType			= VK_IMAGE_TYPE_2D;
		params.dst.image.format				= VK_FORMAT_R8G8B8A8_UNORM;
		params.dst.image.extent				= defaultExtent;
		params.dst.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
		params.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		params.allocationKind				= allocationKind;

		const VkBufferImageCopy	bufferImageCopy	=
		{
			0u,											// VkDeviceSize				bufferOffset;
			defaultSize,								// deUint32					bufferRowLength;
			defaultSize,								// deUint32					bufferImageHeight;
			defaultSourceLayer,							// VkImageSubresourceLayers	imageSubresource;
			{defaultFourthSize, defaultFourthSize, 0},	// VkOffset3D				imageOffset;
			defaultHalfExtent							// VkExtent3D				imageExtent;
		};
		CopyRegion				copyRegion;
		copyRegion.bufferImageCopy	= bufferImageCopy;

		params.regions.push_back(copyRegion);

		group->addChild(new CopyBufferToImageTestCase(testCtx, "tightly_sized_buffer", "Copy from buffer that is just large enough to contain the accessed elements", params));
	}

	{
		TestParams				params;
		params.src.buffer.size				= (defaultHalfSize - 1u) * defaultSize + defaultHalfSize + defaultFourthSize;
		params.dst.image.imageType			= VK_IMAGE_TYPE_2D;
		params.dst.image.format				= VK_FORMAT_R8G8B8A8_UNORM;
		params.dst.image.extent				= defaultExtent;
		params.dst.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
		params.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		params.allocationKind				= allocationKind;

		const VkBufferImageCopy	bufferImageCopy	=
		{
			defaultFourthSize,							// VkDeviceSize				bufferOffset;
			defaultSize,								// deUint32					bufferRowLength;
			defaultSize,								// deUint32					bufferImageHeight;
			defaultSourceLayer,							// VkImageSubresourceLayers	imageSubresource;
			{defaultFourthSize, defaultFourthSize, 0},	// VkOffset3D				imageOffset;
			defaultHalfExtent							// VkExtent3D				imageExtent;
		};
		CopyRegion				copyRegion;
		copyRegion.bufferImageCopy	= bufferImageCopy;

		params.regions.push_back(copyRegion);

		group->addChild(new CopyBufferToImageTestCase(testCtx, "tightly_sized_buffer_offset", "Copy from buffer that is just large enough to contain the accessed elements", params));
	}
}

void addBufferToBufferTests (tcu::TestCaseGroup* group, AllocationKind allocationKind)
{
	tcu::TestContext&				testCtx					= group->getTestContext();

	{
		TestParams			params;
		params.src.buffer.size	= defaultSize;
		params.dst.buffer.size	= defaultSize;
		params.allocationKind	= allocationKind;

		const VkBufferCopy	bufferCopy	=
		{
			0u,				// VkDeviceSize	srcOffset;
			0u,				// VkDeviceSize	dstOffset;
			defaultSize,	// VkDeviceSize	size;
		};

		CopyRegion	copyRegion;
		copyRegion.bufferCopy	= bufferCopy;
		params.regions.push_back(copyRegion);

		group->addChild(new BufferToBufferTestCase(testCtx, "whole", "Whole buffer", params));
	}

	// Filter is VK_FILTER_NEAREST.
	{
		TestParams			params;
		params.src.buffer.size	= defaultFourthSize;
		params.dst.buffer.size	= defaultFourthSize;
		params.allocationKind	= allocationKind;

		const VkBufferCopy	bufferCopy	=
		{
			12u,	// VkDeviceSize	srcOffset;
			4u,		// VkDeviceSize	dstOffset;
			1u,		// VkDeviceSize	size;
		};

		CopyRegion	copyRegion;
		copyRegion.bufferCopy = bufferCopy;
		params.regions.push_back(copyRegion);

		group->addChild(new BufferToBufferTestCase(testCtx, "partial", "Partial", params));
	}

	{
		const deUint32		size		= 16;
		TestParams			params;
		params.src.buffer.size	= size;
		params.dst.buffer.size	= size * (size + 1);
		params.allocationKind	= allocationKind;

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

		group->addChild(new BufferToBufferTestCase(testCtx, "regions", "Multiple regions", params));
	}
}

void addBlittingImageSimpleWholeTests (tcu::TestCaseGroup* group, AllocationKind allocationKind)
{
	tcu::TestContext&	testCtx			= group->getTestContext();
	TestParams			params;
	params.src.image.imageType			= VK_IMAGE_TYPE_2D;
	params.src.image.format				= VK_FORMAT_R8G8B8A8_UNORM;
	params.src.image.extent				= defaultExtent;
	params.src.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
	params.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	params.dst.image.imageType			= VK_IMAGE_TYPE_2D;
	params.dst.image.extent				= defaultExtent;
	params.dst.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
	params.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	params.allocationKind				= allocationKind;

	{
		const VkImageBlit				imageBlit =
		{
			defaultSourceLayer,	// VkImageSubresourceLayers	srcSubresource;
			{
				{ 0, 0, 0 },
				{ defaultSize, defaultSize, 1 }
			},					// VkOffset3D				srcOffsets[2];

			defaultSourceLayer,	// VkImageSubresourceLayers	dstSubresource;
			{
				{ 0, 0, 0 },
				{ defaultSize, defaultSize, 1 }
			}					// VkOffset3D				dstOffset[2];
		};

		CopyRegion	region;
		region.imageBlit = imageBlit;
		params.regions.push_back(region);
	}

	// Filter is VK_FILTER_NEAREST.
	{
		params.filter					= VK_FILTER_NEAREST;
		const std::string description	= "Nearest filter";

		params.dst.image.format = VK_FORMAT_R8G8B8A8_UNORM;
		group->addChild(new BlitImageTestCase(testCtx, "nearest", description, params));

		params.dst.image.format = VK_FORMAT_R32_SFLOAT;
		const std::string	descriptionOfRGBAToR32(description + " and different formats (R8G8B8A8 -> R32)");
		group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_nearest", descriptionOfRGBAToR32, params));

		params.dst.image.format = VK_FORMAT_B8G8R8A8_UNORM;
		const std::string	descriptionOfRGBAToBGRA(description + " and different formats (R8G8B8A8 -> B8G8R8A8)");
		group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_nearest", descriptionOfRGBAToBGRA, params));
	}

	// Filter is VK_FILTER_LINEAR.
	{
		params.filter					= VK_FILTER_LINEAR;
		const std::string description	= "Linear filter";

		params.dst.image.format = VK_FORMAT_R8G8B8A8_UNORM;
		group->addChild(new BlitImageTestCase(testCtx, "linear", description, params));

		params.dst.image.format = VK_FORMAT_R32_SFLOAT;
		const std::string	descriptionOfRGBAToR32(description + " and different formats (R8G8B8A8 -> R32)");
		group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_linear", descriptionOfRGBAToR32, params));

		params.dst.image.format = VK_FORMAT_B8G8R8A8_UNORM;
		const std::string	descriptionOfRGBAToBGRA(description + " and different formats (R8G8B8A8 -> B8G8R8A8)");
		group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_linear", descriptionOfRGBAToBGRA, params));
	}
}

void addBlittingImageSimpleMirrorXYTests (tcu::TestCaseGroup* group, AllocationKind allocationKind)
{
	tcu::TestContext&	testCtx			= group->getTestContext();
	TestParams			params;
	params.src.image.imageType			= VK_IMAGE_TYPE_2D;
	params.src.image.format				= VK_FORMAT_R8G8B8A8_UNORM;
	params.src.image.extent				= defaultExtent;
	params.src.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
	params.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	params.dst.image.imageType			= VK_IMAGE_TYPE_2D;
	params.dst.image.extent				= defaultExtent;
	params.dst.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
	params.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	params.allocationKind				= allocationKind;

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
				{defaultSize, defaultSize, 0},
				{0, 0, 1}
			}					// VkOffset3D				dstOffset[2];
		};

		CopyRegion	region;
		region.imageBlit = imageBlit;
		params.regions.push_back(region);
	}

	// Filter is VK_FILTER_NEAREST.
	{
		params.filter					= VK_FILTER_NEAREST;
		const std::string description	= "Nearest filter";

		params.dst.image.format	= VK_FORMAT_R8G8B8A8_UNORM;
		group->addChild(new BlitImageTestCase(testCtx, "nearest", description, params));

		params.dst.image.format	= VK_FORMAT_R32_SFLOAT;
		const std::string	descriptionOfRGBAToR32	(description + " and different formats (R8G8B8A8 -> R32)");
		group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_nearest", descriptionOfRGBAToR32, params));

		params.dst.image.format	= VK_FORMAT_B8G8R8A8_UNORM;
		const std::string	descriptionOfRGBAToBGRA	(description + " and different formats (R8G8B8A8 -> B8G8R8A8)");
		group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_nearest", descriptionOfRGBAToBGRA, params));
	}

	// Filter is VK_FILTER_LINEAR.
	{
		params.filter					= VK_FILTER_LINEAR;
		const std::string description	= "Linear filter";

		params.dst.image.format	= VK_FORMAT_R8G8B8A8_UNORM;
		group->addChild(new BlitImageTestCase(testCtx, "linear", description, params));

		params.dst.image.format	= VK_FORMAT_R32_SFLOAT;
		const std::string	descriptionOfRGBAToR32	(description + " and different formats (R8G8B8A8 -> R32)");
		group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_linear", descriptionOfRGBAToR32, params));

		params.dst.image.format	= VK_FORMAT_B8G8R8A8_UNORM;
		const std::string	descriptionOfRGBAToBGRA	(description + " and different formats (R8G8B8A8 -> B8G8R8A8)");
		group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_linear", descriptionOfRGBAToBGRA, params));
	}
}

void addBlittingImageSimpleMirrorXTests (tcu::TestCaseGroup* group, AllocationKind allocationKind)
{
	tcu::TestContext&	testCtx			= group->getTestContext();
	TestParams			params;
	params.src.image.imageType			= VK_IMAGE_TYPE_2D;
	params.src.image.format				= VK_FORMAT_R8G8B8A8_UNORM;
	params.src.image.extent				= defaultExtent;
	params.src.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
	params.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	params.dst.image.imageType			= VK_IMAGE_TYPE_2D;
	params.dst.image.extent				= defaultExtent;
	params.dst.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
	params.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	params.allocationKind				= allocationKind;

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
		params.filter					= VK_FILTER_NEAREST;
		const std::string description	= "Nearest filter";

		params.dst.image.format	= VK_FORMAT_R8G8B8A8_UNORM;
		group->addChild(new BlitImageTestCase(testCtx, "nearest", description, params));

		params.dst.image.format	= VK_FORMAT_R32_SFLOAT;
		const std::string	descriptionOfRGBAToR32	(description + " and different formats (R8G8B8A8 -> R32)");
		group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_nearest", descriptionOfRGBAToR32, params));

		params.dst.image.format	= VK_FORMAT_B8G8R8A8_UNORM;
		const std::string	descriptionOfRGBAToBGRA	(description + " and different formats (R8G8B8A8 -> B8G8R8A8)");
		group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_nearest", descriptionOfRGBAToBGRA, params));
	}

	// Filter is VK_FILTER_LINEAR.
	{
		params.filter					= VK_FILTER_LINEAR;
		const std::string description	= "Linear filter";

		params.dst.image.format	= VK_FORMAT_R8G8B8A8_UNORM;
		group->addChild(new BlitImageTestCase(testCtx, "linear", description, params));

		params.dst.image.format	= VK_FORMAT_R32_SFLOAT;
		const std::string	descriptionOfRGBAToR32	(description + " and different formats (R8G8B8A8 -> R32)");
		group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_linear", descriptionOfRGBAToR32, params));

		params.dst.image.format	= VK_FORMAT_B8G8R8A8_UNORM;
		const std::string	descriptionOfRGBAToBGRA	(description + " and different formats (R8G8B8A8 -> B8G8R8A8)");
		group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_linear", descriptionOfRGBAToBGRA, params));
	}
}

void addBlittingImageSimpleMirrorYTests (tcu::TestCaseGroup* group, AllocationKind allocationKind)
{
	tcu::TestContext&	testCtx			= group->getTestContext();
	TestParams			params;
	params.src.image.imageType			= VK_IMAGE_TYPE_2D;
	params.src.image.format				= VK_FORMAT_R8G8B8A8_UNORM;
	params.src.image.extent				= defaultExtent;
	params.src.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
	params.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	params.dst.image.imageType			= VK_IMAGE_TYPE_2D;
	params.dst.image.extent				= defaultExtent;
	params.dst.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
	params.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	params.allocationKind				= allocationKind;

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
				{0, defaultSize, 0},
				{defaultSize, 0, 1}
			}					// VkOffset3D				dstOffset[2];
		};

		CopyRegion	region;
		region.imageBlit = imageBlit;
		params.regions.push_back(region);
	}

	// Filter is VK_FILTER_NEAREST.
	{
		params.filter					= VK_FILTER_NEAREST;
		const std::string description	= "Nearest filter";

		params.dst.image.format	= VK_FORMAT_R8G8B8A8_UNORM;
		group->addChild(new BlitImageTestCase(testCtx, "nearest", description, params));

		params.dst.image.format	= VK_FORMAT_R32_SFLOAT;
		const std::string	descriptionOfRGBAToR32	(description + " and different formats (R8G8B8A8 -> R32)");
		group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_nearest", descriptionOfRGBAToR32, params));

		params.dst.image.format	= VK_FORMAT_B8G8R8A8_UNORM;
		const std::string	descriptionOfRGBAToBGRA	(description + " and different formats (R8G8B8A8 -> B8G8R8A8)");
		group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_nearest", descriptionOfRGBAToBGRA, params));
	}

	// Filter is VK_FILTER_LINEAR.
	{
		params.filter					= VK_FILTER_LINEAR;
		const std::string description	= "Linear filter";

		params.dst.image.format	= VK_FORMAT_R8G8B8A8_UNORM;
		group->addChild(new BlitImageTestCase(testCtx, "linear", description, params));

		params.dst.image.format	= VK_FORMAT_R32_SFLOAT;
		const std::string	descriptionOfRGBAToR32	(description + " and different formats (R8G8B8A8 -> R32)");
		group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_linear", descriptionOfRGBAToR32, params));

		params.dst.image.format	= VK_FORMAT_B8G8R8A8_UNORM;
		const std::string	descriptionOfRGBAToBGRA	(description + " and different formats (R8G8B8A8 -> B8G8R8A8)");
		group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_linear", descriptionOfRGBAToBGRA, params));
	}
}

void addBlittingImageSimpleMirrorSubregionsTests (tcu::TestCaseGroup* group, AllocationKind allocationKind)
{
	tcu::TestContext&	testCtx			= group->getTestContext();
	TestParams			params;
	params.src.image.imageType			= VK_IMAGE_TYPE_2D;
	params.src.image.format				= VK_FORMAT_R8G8B8A8_UNORM;
	params.src.image.extent				= defaultExtent;
	params.src.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
	params.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	params.dst.image.imageType			= VK_IMAGE_TYPE_2D;
	params.dst.image.extent				= defaultExtent;
	params.dst.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
	params.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	params.allocationKind				= allocationKind;

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
		params.filter					= VK_FILTER_NEAREST;
		const std::string description	= "Nearest filter";

		params.dst.image.format	= VK_FORMAT_R8G8B8A8_UNORM;
		group->addChild(new BlitImageTestCase(testCtx, "nearest", description, params));

		params.dst.image.format	= VK_FORMAT_R32_SFLOAT;
		const std::string	descriptionOfRGBAToR32	(description + " and different formats (R8G8B8A8 -> R32)");
		group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_nearest", descriptionOfRGBAToR32, params));

		params.dst.image.format	= VK_FORMAT_B8G8R8A8_UNORM;
		const std::string	descriptionOfRGBAToBGRA	(description + " and different formats (R8G8B8A8 -> B8G8R8A8)");
		group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_nearest", descriptionOfRGBAToBGRA, params));
	}

	// Filter is VK_FILTER_LINEAR.
	{
		params.filter					= VK_FILTER_LINEAR;
		const std::string description	= "Linear filter";

		params.dst.image.format	= VK_FORMAT_R8G8B8A8_UNORM;
		group->addChild(new BlitImageTestCase(testCtx, "linear", description, params));

		params.dst.image.format	= VK_FORMAT_R32_SFLOAT;
		const std::string	descriptionOfRGBAToR32	(description + " and different formats (R8G8B8A8 -> R32)");
		group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_linear", descriptionOfRGBAToR32, params));

		params.dst.image.format	= VK_FORMAT_B8G8R8A8_UNORM;
		const std::string	descriptionOfRGBAToBGRA	(description + " and different formats (R8G8B8A8 -> B8G8R8A8)");
		group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_linear", descriptionOfRGBAToBGRA, params));
	}
}

void addBlittingImageSimpleScalingWhole1Tests (tcu::TestCaseGroup* group, AllocationKind allocationKind)
{
	tcu::TestContext&	testCtx			= group->getTestContext();
	TestParams			params;
	params.src.image.imageType			= VK_IMAGE_TYPE_2D;
	params.src.image.format				= VK_FORMAT_R8G8B8A8_UNORM;
	params.src.image.extent				= defaultExtent;
	params.src.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
	params.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	params.dst.image.imageType			= VK_IMAGE_TYPE_2D;
	params.dst.image.extent				= defaultHalfExtent;
	params.dst.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
	params.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	params.allocationKind				= allocationKind;

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
		params.filter					= VK_FILTER_NEAREST;
		const std::string description	= "Nearest filter";

		params.dst.image.format	= VK_FORMAT_R8G8B8A8_UNORM;
		group->addChild(new BlitImageTestCase(testCtx, "nearest", description, params));

		params.dst.image.format	= VK_FORMAT_R32_SFLOAT;
		const std::string	descriptionOfRGBAToR32	(description + " and different formats (R8G8B8A8 -> R32)");
		group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_nearest", descriptionOfRGBAToR32, params));

		params.dst.image.format	= VK_FORMAT_B8G8R8A8_UNORM;
		const std::string	descriptionOfRGBAToBGRA	(description + " and different formats (R8G8B8A8 -> B8G8R8A8)");
		group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_nearest", descriptionOfRGBAToBGRA, params));
	}

	// Filter is VK_FILTER_LINEAR.
	{
		params.filter					= VK_FILTER_LINEAR;
		const std::string description	= "Linear filter";

		params.dst.image.format	= VK_FORMAT_R8G8B8A8_UNORM;
		group->addChild(new BlitImageTestCase(testCtx, "linear", description, params));

		params.dst.image.format	= VK_FORMAT_R32_SFLOAT;
		const std::string	descriptionOfRGBAToR32	(description + " and different formats (R8G8B8A8 -> R32)" );
		group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_linear", descriptionOfRGBAToR32, params));

		params.dst.image.format	= VK_FORMAT_B8G8R8A8_UNORM;
		const std::string	descriptionOfRGBAToBGRA	(description + " and different formats (R8G8B8A8 -> B8G8R8A8)");
		group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_linear", descriptionOfRGBAToBGRA, params));
	}
}

void addBlittingImageSimpleScalingWhole2Tests (tcu::TestCaseGroup* group, AllocationKind allocationKind)
{
	tcu::TestContext&	testCtx			= group->getTestContext();
	TestParams			params;
	params.src.image.imageType			= VK_IMAGE_TYPE_2D;
	params.src.image.format				= VK_FORMAT_R8G8B8A8_UNORM;
	params.src.image.extent				= defaultHalfExtent;
	params.src.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
	params.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	params.dst.image.imageType			= VK_IMAGE_TYPE_2D;
	params.dst.image.extent				= defaultExtent;
	params.dst.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
	params.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	params.allocationKind				= allocationKind;

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
		params.filter					= VK_FILTER_NEAREST;
		const std::string description	= "Nearest filter";

		params.dst.image.format	= VK_FORMAT_R8G8B8A8_UNORM;
		group->addChild(new BlitImageTestCase(testCtx, "nearest", description, params));

		params.dst.image.format	= VK_FORMAT_R32_SFLOAT;
		const std::string	descriptionOfRGBAToR32	(description + " and different formats (R8G8B8A8 -> R32)");
		group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_nearest", descriptionOfRGBAToR32, params));

		params.dst.image.format	= VK_FORMAT_B8G8R8A8_UNORM;
		const std::string	descriptionOfRGBAToBGRA	(description + " and different formats (R8G8B8A8 -> B8G8R8A8)");
		group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_nearest", descriptionOfRGBAToBGRA, params));
	}

	// Filter is VK_FILTER_LINEAR.
	{
		params.filter					= VK_FILTER_LINEAR;
		const std::string description	= "Linear filter";

		params.dst.image.format	= VK_FORMAT_R8G8B8A8_UNORM;
		group->addChild(new BlitImageTestCase(testCtx, "linear", description, params));

		params.dst.image.format	= VK_FORMAT_R32_SFLOAT;
		const std::string	descriptionOfRGBAToR32	(description + " and different formats (R8G8B8A8 -> R32)");
		group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_linear", descriptionOfRGBAToR32, params));

		params.dst.image.format	= VK_FORMAT_B8G8R8A8_UNORM;
		const std::string	descriptionOfRGBAToBGRA	(description + " and different formats (R8G8B8A8 -> B8G8R8A8)");
		group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_linear", descriptionOfRGBAToBGRA, params));
	}
}

void addBlittingImageSimpleScalingAndOffsetTests (tcu::TestCaseGroup* group, AllocationKind allocationKind)
{
	tcu::TestContext&	testCtx			= group->getTestContext();
	TestParams			params;
	params.src.image.imageType			= VK_IMAGE_TYPE_2D;
	params.src.image.format				= VK_FORMAT_R8G8B8A8_UNORM;
	params.src.image.extent				= defaultExtent;
	params.src.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
	params.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	params.dst.image.imageType			= VK_IMAGE_TYPE_2D;
	params.dst.image.extent				= defaultExtent;
	params.dst.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
	params.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	params.allocationKind				= allocationKind;

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
		params.filter					= VK_FILTER_NEAREST;
		const std::string description	= "Nearest filter";

		params.dst.image.format	= VK_FORMAT_R8G8B8A8_UNORM;
		group->addChild(new BlitImageTestCase(testCtx, "nearest", description, params));

		params.dst.image.format	= VK_FORMAT_R32_SFLOAT;
		const std::string	descriptionOfRGBAToR32	(description + " and different formats (R8G8B8A8 -> R32)");
		group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_nearest", descriptionOfRGBAToR32, params));

		params.dst.image.format	= VK_FORMAT_B8G8R8A8_UNORM;
		const std::string	descriptionOfRGBAToBGRA	(description + " and different formats (R8G8B8A8 -> B8G8R8A8)");
		group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_nearest", descriptionOfRGBAToBGRA, params));
	}

	// Filter is VK_FILTER_LINEAR.
	{
		params.filter					= VK_FILTER_LINEAR;
		const std::string description	= "Linear filter";

		params.dst.image.format	= VK_FORMAT_R8G8B8A8_UNORM;
		group->addChild(new BlitImageTestCase(testCtx, "linear", description, params));

		params.dst.image.format	= VK_FORMAT_R32_SFLOAT;
		const std::string	descriptionOfRGBAToR32	(description + " and different formats (R8G8B8A8 -> R32)");
		group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_linear", descriptionOfRGBAToR32, params));

		params.dst.image.format	= VK_FORMAT_B8G8R8A8_UNORM;
		const std::string	descriptionOfRGBAToBGRA	(description + " and different formats (R8G8B8A8 -> B8G8R8A8)");
		group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_linear", descriptionOfRGBAToBGRA, params));
	}
}

void addBlittingImageSimpleWithoutScalingPartialTests (tcu::TestCaseGroup* group, AllocationKind allocationKind)
{
	tcu::TestContext&	testCtx			= group->getTestContext();
	TestParams			params;
	params.src.image.imageType			= VK_IMAGE_TYPE_2D;
	params.src.image.format				= VK_FORMAT_R8G8B8A8_UNORM;
	params.src.image.extent				= defaultExtent;
	params.src.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
	params.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	params.dst.image.imageType			= VK_IMAGE_TYPE_2D;
	params.dst.image.extent				= defaultExtent;
	params.dst.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
	params.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	params.allocationKind				= allocationKind;

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
		params.filter					= VK_FILTER_NEAREST;
		const std::string description	= "Nearest filter";

		params.dst.image.format	= VK_FORMAT_R8G8B8A8_UNORM;
		group->addChild(new BlitImageTestCase(testCtx, "nearest", description, params));


		params.dst.image.format	= VK_FORMAT_R32_SFLOAT;
		const std::string	descriptionOfRGBAToR32	(description + " and different formats (R8G8B8A8 -> R32)");
		group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_nearest", descriptionOfRGBAToR32, params));

		params.dst.image.format	= VK_FORMAT_B8G8R8A8_UNORM;
		const std::string	descriptionOfRGBAToBGRA	(description + " and different formats (R8G8B8A8 -> B8G8R8A8)");
		group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_nearest", descriptionOfRGBAToBGRA, params));
	}

	// Filter is VK_FILTER_LINEAR.
	{
		params.filter					= VK_FILTER_LINEAR;
		const std::string description	= "Linear filter";

		params.dst.image.format	= VK_FORMAT_R8G8B8A8_UNORM;
		group->addChild(new BlitImageTestCase(testCtx, "linear", description, params));

		params.dst.image.format	= VK_FORMAT_R32_SFLOAT;
		const std::string	descriptionOfRGBAToR32	(description + " and different formats (R8G8B8A8 -> R32)");
		group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_linear", descriptionOfRGBAToR32, params));

		params.dst.image.format	= VK_FORMAT_B8G8R8A8_UNORM;
		const std::string	descriptionOfRGBAToBGRA	(description + " and different formats (R8G8B8A8 -> B8G8R8A8)");
		group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_linear", descriptionOfRGBAToBGRA, params));
	}
}

void addBlittingImageSimpleTests (tcu::TestCaseGroup* group, AllocationKind allocationKind)
{
	addTestGroup(group, "whole", "Blit without scaling (whole)", addBlittingImageSimpleWholeTests, allocationKind);
	addTestGroup(group, "mirror_xy", "Flipping x and y coordinates (whole)", addBlittingImageSimpleMirrorXYTests, allocationKind);
	addTestGroup(group, "mirror_x", "Flipping x coordinates (whole)", addBlittingImageSimpleMirrorXTests, allocationKind);
	addTestGroup(group, "mirror_y", "Flipping y coordinates (whole)", addBlittingImageSimpleMirrorYTests, allocationKind);
	addTestGroup(group, "mirror_subregions", "Mirroring subregions in image (no flip, y flip, x flip, xy flip)", addBlittingImageSimpleMirrorSubregionsTests, allocationKind);
	addTestGroup(group, "scaling_whole1", "Blit with scaling (whole, src extent bigger)", addBlittingImageSimpleScalingWhole1Tests, allocationKind);
	addTestGroup(group, "scaling_whole2", "Blit with scaling (whole, dst extent bigger)", addBlittingImageSimpleScalingWhole2Tests, allocationKind);
	addTestGroup(group, "scaling_and_offset", "Blit with scaling and offset (whole, dst extent bigger)", addBlittingImageSimpleScalingAndOffsetTests, allocationKind);
	addTestGroup(group, "without_scaling_partial", "Blit without scaling (partial)", addBlittingImageSimpleWithoutScalingPartialTests, allocationKind);
}

struct BlitColorTestParams
{
	TestParams		params;
	const VkFormat*	compatibleFormats;
	bool			onlyNearest;
};

bool isAllowedBlittingAllFormatsColorSrcFormatTests(const BlitColorTestParams& testParams)
{
	bool result = true;

	if (testParams.params.allocationKind == ALLOCATION_KIND_DEDICATED)
	{
		DE_ASSERT(!dedicatedAllocationBlittingFormatsToTestSet.empty());

		result =
			de::contains(dedicatedAllocationBlittingFormatsToTestSet, testParams.params.dst.image.format) ||
			de::contains(dedicatedAllocationBlittingFormatsToTestSet, testParams.params.src.image.format);
	}

	return result;
}

const VkFormat	linearOtherImageFormatsToTest[]	=
{
	// From compatibleFormats8Bit
	VK_FORMAT_R4G4_UNORM_PACK8,
	VK_FORMAT_R8_SRGB,

	// From compatibleFormats16Bit
	VK_FORMAT_R4G4B4A4_UNORM_PACK16,
	VK_FORMAT_R16_SFLOAT,

	// From compatibleFormats24Bit
	VK_FORMAT_R8G8B8_UNORM,
	VK_FORMAT_B8G8R8_SRGB,

	// From compatibleFormats32Bit
	VK_FORMAT_R8G8B8A8_UNORM,
	VK_FORMAT_R32_SFLOAT,

	// From compatibleFormats48Bit
	VK_FORMAT_R16G16B16_UNORM,
	VK_FORMAT_R16G16B16_SFLOAT,

	// From compatibleFormats64Bit
	VK_FORMAT_R16G16B16A16_UNORM,
	VK_FORMAT_R64_SFLOAT,

	// From compatibleFormats96Bit
	VK_FORMAT_R32G32B32_UINT,
	VK_FORMAT_R32G32B32_SFLOAT,

	// From compatibleFormats128Bit
	VK_FORMAT_R32G32B32A32_UINT,
	VK_FORMAT_R64G64_SFLOAT,

	// From compatibleFormats192Bit
	VK_FORMAT_R64G64B64_UINT,
	VK_FORMAT_R64G64B64_SFLOAT,

	// From compatibleFormats256Bit
	VK_FORMAT_R64G64B64A64_UINT,
	VK_FORMAT_R64G64B64A64_SFLOAT,
};

std::string getBlitImageTilingLayoutCaseName (VkImageTiling tiling, VkImageLayout layout)
{
	switch (tiling)
	{
		case VK_IMAGE_TILING_OPTIMAL:
			return getImageLayoutCaseName(layout);
		case VK_IMAGE_TILING_LINEAR:
			return "linear";
		default:
			DE_ASSERT(false);
			return "";
	}
}

void addBlittingImageAllFormatsColorSrcFormatDstFormatTests (tcu::TestCaseGroup* group, BlitColorTestParams testParams)
{
	tcu::TestContext& testCtx				= group->getTestContext();

	FormatSet linearOtherImageFormatsToTestSet;
	const int numOfOtherImageFormatsToTestFilter = DE_LENGTH_OF_ARRAY(linearOtherImageFormatsToTest);
	for (int otherImageFormatsIndex = 0; otherImageFormatsIndex < numOfOtherImageFormatsToTestFilter; ++otherImageFormatsIndex)
		linearOtherImageFormatsToTestSet.insert(linearOtherImageFormatsToTest[otherImageFormatsIndex]);

	const VkImageTiling blitSrcTilings[]	=
	{
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_TILING_LINEAR,
	};
	const VkImageLayout blitSrcLayouts[]	=
	{
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_IMAGE_LAYOUT_GENERAL
	};
	const VkImageTiling blitDstTilings[]	=
	{
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_TILING_LINEAR,
	};
	const VkImageLayout blitDstLayouts[]	=
	{
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_GENERAL
	};

	for (int srcTilingNdx = 0u; srcTilingNdx < DE_LENGTH_OF_ARRAY(blitSrcTilings); ++srcTilingNdx)
	{
		testParams.params.src.image.tiling = blitSrcTilings[srcTilingNdx];

		for (int srcLayoutNdx = 0u; srcLayoutNdx < DE_LENGTH_OF_ARRAY(blitSrcLayouts); ++srcLayoutNdx)
		{
			testParams.params.src.image.operationLayout = blitSrcLayouts[srcLayoutNdx];

			// Don't bother testing VK_IMAGE_TILING_LINEAR + VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL as it's likely to be the same as VK_IMAGE_LAYOUT_GENERAL
			if (testParams.params.src.image.tiling == VK_IMAGE_TILING_LINEAR && testParams.params.src.image.operationLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
				continue;

			for (int dstTilingNdx = 0u; dstTilingNdx < DE_LENGTH_OF_ARRAY(blitDstTilings); ++dstTilingNdx)
			{
				testParams.params.dst.image.tiling = blitDstTilings[dstTilingNdx];

				for (int dstLayoutNdx = 0u; dstLayoutNdx < DE_LENGTH_OF_ARRAY(blitDstLayouts); ++dstLayoutNdx)
				{
					testParams.params.dst.image.operationLayout = blitDstLayouts[dstLayoutNdx];

					// Don't bother testing VK_IMAGE_TILING_LINEAR + VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL as it's likely to be the same as VK_IMAGE_LAYOUT_GENERAL
					if (testParams.params.dst.image.tiling == VK_IMAGE_TILING_LINEAR && testParams.params.dst.image.operationLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
						continue;

					if ((testParams.params.dst.image.tiling == VK_IMAGE_TILING_LINEAR && !de::contains(linearOtherImageFormatsToTestSet, testParams.params.src.image.format)) ||
					    (testParams.params.src.image.tiling == VK_IMAGE_TILING_LINEAR && !de::contains(linearOtherImageFormatsToTestSet, testParams.params.dst.image.format)))
						continue;

					testParams.params.filter			= VK_FILTER_NEAREST;
					const std::string testName			= getBlitImageTilingLayoutCaseName(testParams.params.src.image.tiling, testParams.params.src.image.operationLayout) + "_" +
														  getBlitImageTilingLayoutCaseName(testParams.params.dst.image.tiling, testParams.params.dst.image.operationLayout);
					const std::string description		= "Blit from layout " + getBlitImageTilingLayoutCaseName(testParams.params.src.image.tiling, testParams.params.src.image.operationLayout) +
														  " to " + getBlitImageTilingLayoutCaseName(testParams.params.dst.image.tiling, testParams.params.dst.image.operationLayout);
					group->addChild(new BlitImageTestCase(testCtx, testName + "_nearest", description, testParams.params));

					if (!testParams.onlyNearest)
					{
						testParams.params.filter		= VK_FILTER_LINEAR;
						group->addChild(new BlitImageTestCase(testCtx, testName + "_linear", description, testParams.params));
					}
				}
			}
		}
	}
}

void addBlittingImageAllFormatsColorSrcFormatTests (tcu::TestCaseGroup* group, BlitColorTestParams testParams)
{
	for (int dstFormatIndex = 0; testParams.compatibleFormats[dstFormatIndex] != VK_FORMAT_UNDEFINED; ++dstFormatIndex)
	{
		testParams.params.dst.image.format	= testParams.compatibleFormats[dstFormatIndex];
		if (!isSupportedByFramework(testParams.params.dst.image.format))
			continue;

		if (!isAllowedBlittingAllFormatsColorSrcFormatTests(testParams))
			continue;

		const std::string description	= "Blit destination format " + getFormatCaseName(testParams.params.dst.image.format);
		addTestGroup(group, getFormatCaseName(testParams.params.dst.image.format), description, addBlittingImageAllFormatsColorSrcFormatDstFormatTests, testParams);
	}
}

const VkFormat	compatibleFormatsUInts[]	=
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
const VkFormat	compatibleFormatsSInts[]	=
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
const VkFormat	compatibleFormatsFloats[]	=
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
	VK_FORMAT_B10G11R11_UFLOAT_PACK32,
	VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,
//	VK_FORMAT_BC1_RGB_UNORM_BLOCK,
//	VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
//	VK_FORMAT_BC2_UNORM_BLOCK,
//	VK_FORMAT_BC3_UNORM_BLOCK,
//	VK_FORMAT_BC4_UNORM_BLOCK,
//	VK_FORMAT_BC4_SNORM_BLOCK,
//	VK_FORMAT_BC5_UNORM_BLOCK,
//	VK_FORMAT_BC5_SNORM_BLOCK,
//	VK_FORMAT_BC6H_UFLOAT_BLOCK,
//	VK_FORMAT_BC6H_SFLOAT_BLOCK,
//	VK_FORMAT_BC7_UNORM_BLOCK,
//	VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK,
//	VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK,
//	VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK,
//	VK_FORMAT_EAC_R11_UNORM_BLOCK,
//	VK_FORMAT_EAC_R11_SNORM_BLOCK,
//	VK_FORMAT_EAC_R11G11_UNORM_BLOCK,
//	VK_FORMAT_EAC_R11G11_SNORM_BLOCK,
//	VK_FORMAT_ASTC_4x4_UNORM_BLOCK,
//	VK_FORMAT_ASTC_5x4_UNORM_BLOCK,
//	VK_FORMAT_ASTC_5x5_UNORM_BLOCK,
//	VK_FORMAT_ASTC_6x5_UNORM_BLOCK,
//	VK_FORMAT_ASTC_6x6_UNORM_BLOCK,
//	VK_FORMAT_ASTC_8x5_UNORM_BLOCK,
//	VK_FORMAT_ASTC_8x6_UNORM_BLOCK,
//	VK_FORMAT_ASTC_8x8_UNORM_BLOCK,
//	VK_FORMAT_ASTC_10x5_UNORM_BLOCK,
//	VK_FORMAT_ASTC_10x6_UNORM_BLOCK,
//	VK_FORMAT_ASTC_10x8_UNORM_BLOCK,
//	VK_FORMAT_ASTC_10x10_UNORM_BLOCK,
//	VK_FORMAT_ASTC_12x10_UNORM_BLOCK,
//	VK_FORMAT_ASTC_12x12_UNORM_BLOCK,

	VK_FORMAT_UNDEFINED
};
const VkFormat	compatibleFormatsSrgb[]		=
{
	VK_FORMAT_R8_SRGB,
	VK_FORMAT_R8G8_SRGB,
	VK_FORMAT_R8G8B8_SRGB,
	VK_FORMAT_B8G8R8_SRGB,
	VK_FORMAT_R8G8B8A8_SRGB,
	VK_FORMAT_B8G8R8A8_SRGB,
	VK_FORMAT_A8B8G8R8_SRGB_PACK32,
//	VK_FORMAT_BC1_RGB_SRGB_BLOCK,
//	VK_FORMAT_BC1_RGBA_SRGB_BLOCK,
//	VK_FORMAT_BC2_SRGB_BLOCK,
//	VK_FORMAT_BC3_SRGB_BLOCK,
//	VK_FORMAT_BC7_SRGB_BLOCK,
//	VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK,
//	VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK,
//	VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK,
//	VK_FORMAT_ASTC_4x4_SRGB_BLOCK,
//	VK_FORMAT_ASTC_5x4_SRGB_BLOCK,
//	VK_FORMAT_ASTC_5x5_SRGB_BLOCK,
//	VK_FORMAT_ASTC_6x5_SRGB_BLOCK,
//	VK_FORMAT_ASTC_6x6_SRGB_BLOCK,
//	VK_FORMAT_ASTC_8x5_SRGB_BLOCK,
//	VK_FORMAT_ASTC_8x6_SRGB_BLOCK,
//	VK_FORMAT_ASTC_8x8_SRGB_BLOCK,
//	VK_FORMAT_ASTC_10x5_SRGB_BLOCK,
//	VK_FORMAT_ASTC_10x6_SRGB_BLOCK,
//	VK_FORMAT_ASTC_10x8_SRGB_BLOCK,
//	VK_FORMAT_ASTC_10x10_SRGB_BLOCK,
//	VK_FORMAT_ASTC_12x10_SRGB_BLOCK,
//	VK_FORMAT_ASTC_12x12_SRGB_BLOCK,

	VK_FORMAT_UNDEFINED
};

const VkFormat	dedicatedAllocationBlittingFormatsToTest[]	=
{
	// compatibleFormatsUInts
	VK_FORMAT_R8_UINT,
	VK_FORMAT_R64G64B64A64_UINT,

	// compatibleFormatsSInts
	VK_FORMAT_R8_SINT,
	VK_FORMAT_R64G64B64A64_SINT,

	// compatibleFormatsFloats
	VK_FORMAT_R4G4_UNORM_PACK8,
	VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,

	// compatibleFormatsSrgb
	VK_FORMAT_R8_SRGB,
	VK_FORMAT_A8B8G8R8_SRGB_PACK32,
};

void addBlittingImageAllFormatsColorTests (tcu::TestCaseGroup* group, AllocationKind allocationKind)
{
	const struct {
		const VkFormat*	compatibleFormats;
		const bool		onlyNearest;
	}	colorImageFormatsToTestBlit[]			=
	{
		{ compatibleFormatsUInts,	true	},
		{ compatibleFormatsSInts,	true	},
		{ compatibleFormatsFloats,	false	},
		{ compatibleFormatsSrgb,	false	},
	};

	const int	numOfColorImageFormatsToTest		= DE_LENGTH_OF_ARRAY(colorImageFormatsToTestBlit);

	TestParams	params;
	params.src.image.imageType	= VK_IMAGE_TYPE_2D;
	params.src.image.extent		= defaultExtent;
	params.src.image.tiling		= VK_IMAGE_TILING_OPTIMAL;
	params.dst.image.imageType	= VK_IMAGE_TYPE_2D;
	params.dst.image.extent		= defaultExtent;
	params.dst.image.tiling		= VK_IMAGE_TILING_OPTIMAL;
	params.allocationKind		= allocationKind;

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

	if (allocationKind == ALLOCATION_KIND_DEDICATED)
	{
		const int numOfColorImageFormatsToTestFilter = DE_LENGTH_OF_ARRAY(dedicatedAllocationBlittingFormatsToTest);
		for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < numOfColorImageFormatsToTestFilter; ++compatibleFormatsIndex)
			dedicatedAllocationBlittingFormatsToTestSet.insert(dedicatedAllocationBlittingFormatsToTest[compatibleFormatsIndex]);
	}

	for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < numOfColorImageFormatsToTest; ++compatibleFormatsIndex)
	{
		const VkFormat*	compatibleFormats	= colorImageFormatsToTestBlit[compatibleFormatsIndex].compatibleFormats;
		const bool		onlyNearest			= colorImageFormatsToTestBlit[compatibleFormatsIndex].onlyNearest;
		for (int srcFormatIndex = 0; compatibleFormats[srcFormatIndex] != VK_FORMAT_UNDEFINED; ++srcFormatIndex)
		{
			params.src.image.format	= compatibleFormats[srcFormatIndex];
			if (!isSupportedByFramework(params.src.image.format))
				continue;

			BlitColorTestParams		testParams;
			testParams.params				= params;
			testParams.compatibleFormats	= compatibleFormats;
			testParams.onlyNearest			= onlyNearest;

			const std::string description	= "Blit source format " + getFormatCaseName(params.src.image.format);
			addTestGroup(group, getFormatCaseName(params.src.image.format), description, addBlittingImageAllFormatsColorSrcFormatTests, testParams);
		}
	}
}

void addBlittingImageAllFormatsDepthStencilFormatsTests (tcu::TestCaseGroup* group, TestParams params)
{
	const VkImageLayout blitSrcLayouts[]	=
	{
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_IMAGE_LAYOUT_GENERAL
	};
	const VkImageLayout blitDstLayouts[]	=
	{
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_GENERAL
	};

	for (int srcLayoutNdx = 0u; srcLayoutNdx < DE_LENGTH_OF_ARRAY(blitSrcLayouts); ++srcLayoutNdx)
	{
		params.src.image.operationLayout	= blitSrcLayouts[srcLayoutNdx];

		for (int dstLayoutNdx = 0u; dstLayoutNdx < DE_LENGTH_OF_ARRAY(blitDstLayouts); ++dstLayoutNdx)
		{
			params.dst.image.operationLayout	= blitDstLayouts[dstLayoutNdx];
			params.filter						= VK_FILTER_NEAREST;

			const std::string testName		= getImageLayoutCaseName(params.src.image.operationLayout) + "_" +
											  getImageLayoutCaseName(params.dst.image.operationLayout);
			const std::string description	= "Blit from " + getImageLayoutCaseName(params.src.image.operationLayout) +
											  " to " + getImageLayoutCaseName(params.dst.image.operationLayout);

			group->addChild(new BlitImageTestCase(group->getTestContext(), testName + "_nearest", description, params));
		}
	}
}

void addBlittingImageAllFormatsDepthStencilTests (tcu::TestCaseGroup* group, AllocationKind allocationKind)
{
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

	const VkImageSubresourceLayers	defaultDepthSourceLayer		= { VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, 1u };
	const VkImageSubresourceLayers	defaultStencilSourceLayer	= { VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, 1u };
	const VkImageSubresourceLayers	defaultDSSourceLayer		= { VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, 1u };

	// 2D tests
	for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < DE_LENGTH_OF_ARRAY(depthAndStencilFormats); ++compatibleFormatsIndex)
	{
		TestParams	params;
		params.src.image.imageType			= VK_IMAGE_TYPE_2D;
		params.src.image.extent				= defaultExtent;
		params.src.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
		params.src.image.format				= depthAndStencilFormats[compatibleFormatsIndex];
		params.dst.image.extent				= defaultExtent;
		params.dst.image.imageType			= VK_IMAGE_TYPE_2D;
		params.dst.image.format				= params.src.image.format;
		params.dst.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
		params.allocationKind				= allocationKind;
		params.separateDepthStencilLayouts	= DE_FALSE;

		bool hasDepth	= tcu::hasDepthComponent(mapVkFormat(params.src.image.format).order);
		bool hasStencil	= tcu::hasStencilComponent(mapVkFormat(params.src.image.format).order);

		CopyRegion	region;
		for (int i = 0, j = 1; (i + defaultFourthSize / j < defaultSize) && (defaultFourthSize > j); i += defaultFourthSize / j++)
		{
			const VkOffset3D	srcOffset0	= {0, 0, 0};
			const VkOffset3D	srcOffset1	= {defaultSize, defaultSize, 1};
			const VkOffset3D	dstOffset0	= {i, 0, 0};
			const VkOffset3D	dstOffset1	= {i + defaultFourthSize / j, defaultFourthSize / j, 1};

			if (hasDepth)
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
			if (hasStencil)
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

			if (hasDepth)
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
			if (hasStencil)
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
			if (hasDepth && hasStencil)
			{
				const VkOffset3D			dstDSOffset0	= {i, 3 * defaultFourthSize, 0};
				const VkOffset3D			dstDSOffset1	= {i + defaultFourthSize, defaultSize, 1};
				const VkImageBlit			imageBlit	=
				{
					defaultDSSourceLayer,			// VkImageSubresourceLayers	srcSubresource;
					{ srcOffset0, srcOffset1 },		// VkOffset3D					srcOffsets[2];
					defaultDSSourceLayer,			// VkImageSubresourceLayers	dstSubresource;
					{ dstDSOffset0, dstDSOffset1 }	// VkOffset3D					dstOffset[2];
				};
				region.imageBlit	= imageBlit;
				params.regions.push_back(region);
			}
		}

		const std::string testName		= "2d_" + getFormatCaseName(params.src.image.format) + "_" +
										  getFormatCaseName(params.dst.image.format);
		const std::string description	= "2D blit from " + getFormatCaseName(params.src.image.format) +
										  " to " + getFormatCaseName(params.dst.image.format);
		addTestGroup(group, testName, description, addBlittingImageAllFormatsDepthStencilFormatsTests, params);
	}

	// 1D tests
	for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < DE_LENGTH_OF_ARRAY(depthAndStencilFormats); ++compatibleFormatsIndex)
	{
		TestParams	params;
		params.src.image.imageType			= VK_IMAGE_TYPE_1D;
		params.dst.image.imageType			= VK_IMAGE_TYPE_1D;
		params.src.image.extent				= default1dExtent;
		params.dst.image.extent				= default1dExtent;
		params.src.image.format				= depthAndStencilFormats[compatibleFormatsIndex];
		params.dst.image.format				= params.src.image.format;
		params.src.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
		params.dst.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
		params.allocationKind				= allocationKind;

		bool hasDepth	= tcu::hasDepthComponent(mapVkFormat(params.src.image.format).order);
		bool hasStencil	= tcu::hasStencilComponent(mapVkFormat(params.src.image.format).order);

		CopyRegion	region;
		for (int i = 0; i < defaultSize; i += defaultSize / 2)
		{
			const VkOffset3D	srcOffset0	= {0, 0, 0};
			const VkOffset3D	srcOffset1	= {defaultSize, 1, 1};
			const VkOffset3D	dstOffset0	= {i, 0, 0};
			const VkOffset3D	dstOffset1	= {i + defaultFourthSize, 1, 1};

			if (hasDepth)
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
			if (hasStencil)
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

		{
			const VkOffset3D	srcOffset0	= {0, 0, 0};
			const VkOffset3D	srcOffset1	= {defaultFourthSize, 1, 1};
			const VkOffset3D	dstOffset0	= {defaultFourthSize, 0, 0};
			const VkOffset3D	dstOffset1	= {2 * defaultFourthSize, 1, 1};

			if (hasDepth)
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
			if (hasStencil)
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
			if (hasDepth && hasStencil)
			{
				const VkOffset3D			dstDSOffset0	= {3 * defaultFourthSize, 0, 0};
				const VkOffset3D			dstDSOffset1	= {3 * defaultFourthSize + defaultFourthSize / 2, 1, 1};
				const VkImageBlit			imageBlit	=
				{
					defaultDSSourceLayer,			// VkImageSubresourceLayers	srcSubresource;
					{ srcOffset0, srcOffset1 },		// VkOffset3D					srcOffsets[2];
					defaultDSSourceLayer,			// VkImageSubresourceLayers	dstSubresource;
					{ dstDSOffset0, dstDSOffset1 }	// VkOffset3D					dstOffset[2];
				};
				region.imageBlit	= imageBlit;
				params.regions.push_back(region);
			}
		}

		const std::string testName		= "1d_" + getFormatCaseName(params.src.image.format) + "_" +
										  getFormatCaseName(params.dst.image.format);
		const std::string description	= "1D blit from " + getFormatCaseName(params.src.image.format) +
										  " to " + getFormatCaseName(params.dst.image.format);
		addTestGroup(group, testName, description, addBlittingImageAllFormatsDepthStencilFormatsTests, params);
	}

	// 3D tests. Note we use smaller dimensions here for performance reasons.
	for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < DE_LENGTH_OF_ARRAY(depthAndStencilFormats); ++compatibleFormatsIndex)
	{
		TestParams	params;
		params.src.image.imageType			= VK_IMAGE_TYPE_3D;
		params.dst.image.imageType			= VK_IMAGE_TYPE_3D;
		params.src.image.extent				= default3dExtent;
		params.dst.image.extent				= default3dExtent;
		params.src.image.format				= depthAndStencilFormats[compatibleFormatsIndex];
		params.dst.image.format				= params.src.image.format;
		params.src.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
		params.dst.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
		params.allocationKind				= allocationKind;

		bool hasDepth	= tcu::hasDepthComponent(mapVkFormat(params.src.image.format).order);
		bool hasStencil	= tcu::hasStencilComponent(mapVkFormat(params.src.image.format).order);

		CopyRegion	region;
		for (int i = 0, j = 1; (i + defaultSixteenthSize / j < defaultFourthSize) && (defaultSixteenthSize > j); i += defaultSixteenthSize / j++)
		{
			const VkOffset3D	srcOffset0	= {0, 0, 0};
			const VkOffset3D	srcOffset1	= {defaultFourthSize, defaultFourthSize, defaultFourthSize};
			const VkOffset3D	dstOffset0	= {i, 0, i};
			const VkOffset3D	dstOffset1	= {i + defaultSixteenthSize / j, defaultSixteenthSize / j, i + defaultSixteenthSize / j};

			if (hasDepth)
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
			if (hasStencil)
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
		for (int i = 0; i < defaultFourthSize; i += defaultSixteenthSize)
		{
			const VkOffset3D	srcOffset0	= {i, i, i};
			const VkOffset3D	srcOffset1	= {i + defaultSixteenthSize, i + defaultSixteenthSize, i + defaultSixteenthSize};
			const VkOffset3D	dstOffset0	= {i, defaultFourthSize / 2, i};
			const VkOffset3D	dstOffset1	= {i + defaultSixteenthSize, defaultFourthSize / 2 + defaultSixteenthSize, i + defaultSixteenthSize};

			if (hasDepth)
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
			if (hasStencil)
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
			if (hasDepth && hasStencil)
			{
				const VkOffset3D			dstDSOffset0	= {i, 3 * defaultSixteenthSize, i};
				const VkOffset3D			dstDSOffset1	= {i + defaultSixteenthSize, defaultFourthSize, i + defaultSixteenthSize};
				const VkImageBlit			imageBlit	=
				{
					defaultDSSourceLayer,			// VkImageSubresourceLayers	srcSubresource;
					{ srcOffset0, srcOffset1 },		// VkOffset3D					srcOffsets[2];
					defaultDSSourceLayer,			// VkImageSubresourceLayers	dstSubresource;
					{ dstDSOffset0, dstDSOffset1 }	// VkOffset3D					dstOffset[2];
				};
				region.imageBlit	= imageBlit;
				params.regions.push_back(region);
			}
		}

		const std::string testName		= "3d_" + getFormatCaseName(params.src.image.format) + "_" +
										  getFormatCaseName(params.dst.image.format);
		const std::string description	= "3D blit from " + getFormatCaseName(params.src.image.format) +
										  " to " + getFormatCaseName(params.dst.image.format);
		addTestGroup(group, testName, description, addBlittingImageAllFormatsDepthStencilFormatsTests, params);

		if (hasDepth && hasStencil)
		{
			params.separateDepthStencilLayouts	= DE_TRUE;
			const std::string testName2		= getFormatCaseName(params.src.image.format) + "_" +
											  getFormatCaseName(params.dst.image.format) + "_separate_layouts";
			const std::string description2	= "Blit from " + getFormatCaseName(params.src.image.format) +
											  " to " + getFormatCaseName(params.dst.image.format) + " with separate depth/stencil layouts";
			addTestGroup(group, testName2, description2, addBlittingImageAllFormatsDepthStencilFormatsTests, params);
		}
	}
}

void addBlittingImageAllFormatsMipmapFormatTests (tcu::TestCaseGroup* group, BlitColorTestParams testParams)
{
	tcu::TestContext& testCtx				= group->getTestContext();

	const VkImageLayout blitSrcLayouts[]	=
	{
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_IMAGE_LAYOUT_GENERAL
	};
	const VkImageLayout blitDstLayouts[]	=
	{
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_GENERAL
	};

	for (int srcLayoutNdx = 0u; srcLayoutNdx < DE_LENGTH_OF_ARRAY(blitSrcLayouts); ++srcLayoutNdx)
	{
		testParams.params.src.image.operationLayout = blitSrcLayouts[srcLayoutNdx];
		for (int dstLayoutNdx = 0u; dstLayoutNdx < DE_LENGTH_OF_ARRAY(blitDstLayouts); ++dstLayoutNdx)
		{
			testParams.params.dst.image.operationLayout = blitDstLayouts[dstLayoutNdx];

			testParams.params.filter			= VK_FILTER_NEAREST;
			const std::string testName			= getImageLayoutCaseName(testParams.params.src.image.operationLayout) + "_" +
												  getImageLayoutCaseName(testParams.params.dst.image.operationLayout);
			const std::string description		= "Blit from layout " + getImageLayoutCaseName(testParams.params.src.image.operationLayout) +
												  " to " + getImageLayoutCaseName(testParams.params.dst.image.operationLayout);
			group->addChild(new BlitMipmapTestCase(testCtx, testName + "_nearest", description, testParams.params));

			if (!testParams.onlyNearest)
			{
				testParams.params.filter		= VK_FILTER_LINEAR;
				group->addChild(new BlitMipmapTestCase(testCtx, testName + "_linear", description, testParams.params));
			}
		}
	}
}

void addBlittingImageAllFormatsBaseLevelMipmapTests (tcu::TestCaseGroup* group, AllocationKind allocationKind)
{
	const struct
	{
		const VkFormat* const	compatibleFormats;
		const bool				onlyNearest;
	}	colorImageFormatsToTestBlit[]			=
	{
		{ compatibleFormatsUInts,	true	},
		{ compatibleFormatsSInts,	true	},
		{ compatibleFormatsFloats,	false	},
		{ compatibleFormatsSrgb,	false	},
	};

	const int	numOfColorImageFormatsToTest	= DE_LENGTH_OF_ARRAY(colorImageFormatsToTestBlit);

	const int	layerCountsToTest[]				=
	{
		1,
		6
	};

	TestParams	params;
	params.src.image.imageType	= VK_IMAGE_TYPE_2D;
	params.src.image.extent		= defaultExtent;
	params.src.image.tiling		= VK_IMAGE_TILING_OPTIMAL;
	params.dst.image.imageType	= VK_IMAGE_TYPE_2D;
	params.dst.image.extent		= defaultExtent;
	params.dst.image.tiling		= VK_IMAGE_TILING_OPTIMAL;
	params.allocationKind		= allocationKind;
	params.mipLevels			= deLog2Floor32(deMinu32(defaultExtent.width, defaultExtent.height)) + 1u;
	params.singleCommand		= DE_TRUE;

	CopyRegion	region;
	for (deUint32 mipLevelNdx = 0u; mipLevelNdx < params.mipLevels; mipLevelNdx++)
	{
		VkImageSubresourceLayers	destLayer	= defaultSourceLayer;
		destLayer.mipLevel = mipLevelNdx;

		const VkImageBlit			imageBlit	=
		{
			defaultSourceLayer,	// VkImageSubresourceLayers	srcSubresource;
			{
				{0, 0, 0},
				{defaultSize, defaultSize, 1}
			},					// VkOffset3D					srcOffsets[2];

			destLayer,			// VkImageSubresourceLayers	dstSubresource;
			{
				{0, 0, 0},
				{defaultSize >> mipLevelNdx, defaultSize >> mipLevelNdx, 1}
			}					// VkOffset3D					dstOffset[2];
		};
		region.imageBlit	= imageBlit;
		params.regions.push_back(region);
	}

	if (allocationKind == ALLOCATION_KIND_DEDICATED)
	{
		const int numOfColorImageFormatsToTestFilter = DE_LENGTH_OF_ARRAY(dedicatedAllocationBlittingFormatsToTest);
		for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < numOfColorImageFormatsToTestFilter; ++compatibleFormatsIndex)
			dedicatedAllocationBlittingFormatsToTestSet.insert(dedicatedAllocationBlittingFormatsToTest[compatibleFormatsIndex]);
	}

	for (int layerCountIndex = 0; layerCountIndex < DE_LENGTH_OF_ARRAY(layerCountsToTest); layerCountIndex++)
	{
		const int						layerCount		= layerCountsToTest[layerCountIndex];
		const std::string				layerGroupName	= "layercount_" + de::toString(layerCount);
		const std::string				layerGroupDesc	= "Blit mipmaps with layerCount = " + de::toString(layerCount);

		de::MovePtr<tcu::TestCaseGroup>	layerCountGroup	(new tcu::TestCaseGroup(group->getTestContext(), layerGroupName.c_str(), layerGroupDesc.c_str()));

		for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < numOfColorImageFormatsToTest; ++compatibleFormatsIndex)
		{
			const VkFormat*	compatibleFormats	= colorImageFormatsToTestBlit[compatibleFormatsIndex].compatibleFormats;
			const bool		onlyNearest			= colorImageFormatsToTestBlit[compatibleFormatsIndex].onlyNearest;

			for (int srcFormatIndex = 0; compatibleFormats[srcFormatIndex] != VK_FORMAT_UNDEFINED; ++srcFormatIndex)
			{
				params.src.image.format	= compatibleFormats[srcFormatIndex];
				params.dst.image.format	= compatibleFormats[srcFormatIndex];

				if (!isSupportedByFramework(params.src.image.format))
					continue;

				const std::string description	= "Blit source format " + getFormatCaseName(params.src.image.format);

				BlitColorTestParams testParams;
				testParams.params				= params;
				testParams.compatibleFormats	= compatibleFormats;
				testParams.onlyNearest			= onlyNearest;

				testParams.params.src.image.extent.depth = layerCount;
				testParams.params.dst.image.extent.depth = layerCount;

				for (size_t regionNdx = 0; regionNdx < testParams.params.regions.size(); regionNdx++)
				{
					testParams.params.regions[regionNdx].imageBlit.srcSubresource.layerCount = layerCount;
					testParams.params.regions[regionNdx].imageBlit.dstSubresource.layerCount = layerCount;
				}

				addTestGroup(layerCountGroup.get(), getFormatCaseName(params.src.image.format), description, addBlittingImageAllFormatsMipmapFormatTests, testParams);
			}
		}
		group->addChild(layerCountGroup.release());
	}
}

void addBlittingImageAllFormatsPreviousLevelMipmapTests (tcu::TestCaseGroup* group, AllocationKind allocationKind)
{
	const struct
	{
		const VkFormat* const	compatibleFormats;
		const bool				onlyNearest;
	}	colorImageFormatsToTestBlit[]			=
	{
		{ compatibleFormatsUInts,	true	},
		{ compatibleFormatsSInts,	true	},
		{ compatibleFormatsFloats,	false	},
		{ compatibleFormatsSrgb,	false	},
	};

	const int	numOfColorImageFormatsToTest	= DE_LENGTH_OF_ARRAY(colorImageFormatsToTestBlit);

	const int	layerCountsToTest[]				=
	{
		1,
		6
	};

	TestParams	params;
	params.src.image.imageType	= VK_IMAGE_TYPE_2D;
	params.src.image.extent		= defaultExtent;
	params.src.image.tiling		= VK_IMAGE_TILING_OPTIMAL;
	params.dst.image.imageType	= VK_IMAGE_TYPE_2D;
	params.dst.image.extent		= defaultExtent;
	params.dst.image.tiling		= VK_IMAGE_TILING_OPTIMAL;
	params.allocationKind		= allocationKind;
	params.mipLevels			= deLog2Floor32(deMinu32(defaultExtent.width, defaultExtent.height)) + 1u;
	params.singleCommand		= DE_FALSE;

	CopyRegion	region;
	for (deUint32 mipLevelNdx = 1u; mipLevelNdx < params.mipLevels; mipLevelNdx++)
	{
		VkImageSubresourceLayers	srcLayer	= defaultSourceLayer;
		VkImageSubresourceLayers	destLayer	= defaultSourceLayer;

		srcLayer.mipLevel	= mipLevelNdx - 1u;
		destLayer.mipLevel	= mipLevelNdx;

		const VkImageBlit			imageBlit	=
		{
			srcLayer,			// VkImageSubresourceLayers	srcSubresource;
			{
				{0, 0, 0},
				{defaultSize >> (mipLevelNdx - 1u), defaultSize >> (mipLevelNdx - 1u), 1}
			},					// VkOffset3D					srcOffsets[2];

			destLayer,			// VkImageSubresourceLayers	dstSubresource;
			{
				{0, 0, 0},
				{defaultSize >> mipLevelNdx, defaultSize >> mipLevelNdx, 1}
			}					// VkOffset3D					dstOffset[2];
		};
		region.imageBlit	= imageBlit;
		params.regions.push_back(region);
	}

	if (allocationKind == ALLOCATION_KIND_DEDICATED)
	{
		const int numOfColorImageFormatsToTestFilter = DE_LENGTH_OF_ARRAY(dedicatedAllocationBlittingFormatsToTest);
		for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < numOfColorImageFormatsToTestFilter; ++compatibleFormatsIndex)
			dedicatedAllocationBlittingFormatsToTestSet.insert(dedicatedAllocationBlittingFormatsToTest[compatibleFormatsIndex]);
	}

	for (int layerCountIndex = 0; layerCountIndex < DE_LENGTH_OF_ARRAY(layerCountsToTest); layerCountIndex++)
	{
		const int						layerCount		= layerCountsToTest[layerCountIndex];
		const std::string				layerGroupName	= "layercount_" + de::toString(layerCount);
		const std::string				layerGroupDesc	= "Blit mipmaps with layerCount = " + de::toString(layerCount);

		de::MovePtr<tcu::TestCaseGroup>	layerCountGroup	(new tcu::TestCaseGroup(group->getTestContext(), layerGroupName.c_str(), layerGroupDesc.c_str()));

		for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < numOfColorImageFormatsToTest; ++compatibleFormatsIndex)
		{
			const VkFormat*	compatibleFormats	= colorImageFormatsToTestBlit[compatibleFormatsIndex].compatibleFormats;
			const bool		onlyNearest			= colorImageFormatsToTestBlit[compatibleFormatsIndex].onlyNearest;

			for (int srcFormatIndex = 0; compatibleFormats[srcFormatIndex] != VK_FORMAT_UNDEFINED; ++srcFormatIndex)
			{
				params.src.image.format						= compatibleFormats[srcFormatIndex];
				params.dst.image.format						= compatibleFormats[srcFormatIndex];

				if (!isSupportedByFramework(params.src.image.format))
					continue;

				const std::string	description				= "Blit source format " + getFormatCaseName(params.src.image.format);

				BlitColorTestParams	testParams;
				testParams.params							= params;
				testParams.compatibleFormats				= compatibleFormats;
				testParams.onlyNearest						= onlyNearest;

				testParams.params.src.image.extent.depth	= layerCount;
				testParams.params.dst.image.extent.depth	= layerCount;

				for (size_t regionNdx = 0; regionNdx < testParams.params.regions.size(); regionNdx++)
				{
					testParams.params.regions[regionNdx].imageBlit.srcSubresource.layerCount = layerCount;
					testParams.params.regions[regionNdx].imageBlit.dstSubresource.layerCount = layerCount;
				}

				addTestGroup(layerCountGroup.get(), getFormatCaseName(params.src.image.format), description, addBlittingImageAllFormatsMipmapFormatTests, testParams);
			}
		}
		group->addChild(layerCountGroup.release());
	}

	for (int multiLayer = 0; multiLayer < 2; multiLayer++)
	{
		const int layerCount = multiLayer ? 6 : 1;

		for (int barrierCount = 1; barrierCount < 4; barrierCount++)
		{
			if (layerCount != 1 || barrierCount != 1)
			{
				const std::string				barrierGroupName = (multiLayer ? "layerbarriercount_" : "mipbarriercount_") + de::toString(barrierCount);
				const std::string				barrierGroupDesc = "Use " + de::toString(barrierCount) + " image barriers";

				de::MovePtr<tcu::TestCaseGroup>	barrierCountGroup(new tcu::TestCaseGroup(group->getTestContext(), barrierGroupName.c_str(), barrierGroupDesc.c_str()));

				params.barrierCount = barrierCount;

				// Only go through a few common formats
				for (int srcFormatIndex = 2; srcFormatIndex < 6; ++srcFormatIndex)
				{
					params.src.image.format						= compatibleFormatsUInts[srcFormatIndex];
					params.dst.image.format						= compatibleFormatsUInts[srcFormatIndex];

					if (!isSupportedByFramework(params.src.image.format))
						continue;

					const std::string description				= "Blit source format " + getFormatCaseName(params.src.image.format);

					BlitColorTestParams testParams;
					testParams.params							= params;
					testParams.compatibleFormats				= compatibleFormatsUInts;
					testParams.onlyNearest						= true;

					testParams.params.src.image.extent.depth	= layerCount;
					testParams.params.dst.image.extent.depth	= layerCount;

					for (size_t regionNdx = 0; regionNdx < testParams.params.regions.size(); regionNdx++)
					{
						testParams.params.regions[regionNdx].imageBlit.srcSubresource.layerCount = layerCount;
						testParams.params.regions[regionNdx].imageBlit.dstSubresource.layerCount = layerCount;
					}

					addTestGroup(barrierCountGroup.get(), getFormatCaseName(params.src.image.format), description, addBlittingImageAllFormatsMipmapFormatTests, testParams);
				}
				group->addChild(barrierCountGroup.release());
			}
		}
	}
}

void addBlittingImageAllFormatsMipmapTests (tcu::TestCaseGroup* group, AllocationKind allocationKind)
{
	addTestGroup(group, "from_base_level", "Generate all mipmap levels from base level", addBlittingImageAllFormatsBaseLevelMipmapTests, allocationKind);
	addTestGroup(group, "from_previous_level", "Generate next mipmap level from previous level", addBlittingImageAllFormatsPreviousLevelMipmapTests, allocationKind);
}

void addBlittingImageAllFormatsTests (tcu::TestCaseGroup* group, AllocationKind allocationKind)
{
	addTestGroup(group, "color", "Blitting image with color formats", addBlittingImageAllFormatsColorTests, allocationKind);
	addTestGroup(group, "depth_stencil", "Blitting image with depth/stencil formats", addBlittingImageAllFormatsDepthStencilTests, allocationKind);
	addTestGroup(group, "generate_mipmaps", "Generating mipmaps with vkCmdBlitImage()", addBlittingImageAllFormatsMipmapTests, allocationKind);
}

void addBlittingImageTests (tcu::TestCaseGroup* group, AllocationKind allocationKind)
{
	addTestGroup(group, "simple_tests", "Blitting image simple tests", addBlittingImageSimpleTests, allocationKind);
	addTestGroup(group, "all_formats", "Blitting image with all compatible formats", addBlittingImageAllFormatsTests, allocationKind);
}

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

void addResolveImageWholeTests (tcu::TestCaseGroup* group, AllocationKind allocationKind)
{
	TestParams	params;
	params.src.image.imageType			= VK_IMAGE_TYPE_2D;
	params.src.image.format				= VK_FORMAT_R8G8B8A8_UNORM;
	params.src.image.extent				= resolveExtent;
	params.src.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
	params.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	params.dst.image.imageType			= VK_IMAGE_TYPE_2D;
	params.dst.image.format				= VK_FORMAT_R8G8B8A8_UNORM;
	params.dst.image.extent				= resolveExtent;
	params.dst.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
	params.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	params.allocationKind				= allocationKind;

	{
		const VkImageSubresourceLayers	sourceLayer	=
		{
			VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
			0u,							// deUint32				mipLevel;
			0u,							// deUint32				baseArrayLayer;
			1u							// deUint32				layerCount;
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
		params.samples					= samples[samplesIndex];
		const std::string description	= "With " + getSampleCountCaseName(samples[samplesIndex]);
		group->addChild(new ResolveImageToImageTestCase(group->getTestContext(), getSampleCountCaseName(samples[samplesIndex]), description, params));
	}
}

void addResolveImagePartialTests (tcu::TestCaseGroup* group, AllocationKind allocationKind)
{
	TestParams	params;
	params.src.image.imageType			= VK_IMAGE_TYPE_2D;
	params.src.image.format				= VK_FORMAT_R8G8B8A8_UNORM;
	params.src.image.extent				= resolveExtent;
	params.src.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
	params.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	params.dst.image.imageType			= VK_IMAGE_TYPE_2D;
	params.dst.image.format				= VK_FORMAT_R8G8B8A8_UNORM;
	params.dst.image.extent				= resolveExtent;
	params.dst.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
	params.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	params.allocationKind				= allocationKind;

	{
		const VkImageSubresourceLayers	sourceLayer	=
		{
			VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
			0u,							// deUint32				mipLevel;
			0u,							// deUint32				baseArrayLayer;
			1u							// deUint32				layerCount;
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
		params.samples					= samples[samplesIndex];
		const std::string description	= "With " + getSampleCountCaseName(samples[samplesIndex]);
		group->addChild(new ResolveImageToImageTestCase(group->getTestContext(), getSampleCountCaseName(samples[samplesIndex]), description, params));
	}
}

void addResolveImageWithRegionsTests (tcu::TestCaseGroup* group, AllocationKind allocationKind)
{
	TestParams	params;
	params.src.image.imageType			= VK_IMAGE_TYPE_2D;
	params.src.image.format				= VK_FORMAT_R8G8B8A8_UNORM;
	params.src.image.extent				= resolveExtent;
	params.src.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
	params.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	params.dst.image.imageType			= VK_IMAGE_TYPE_2D;
	params.dst.image.format				= VK_FORMAT_R8G8B8A8_UNORM;
	params.dst.image.extent				= resolveExtent;
	params.dst.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
	params.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	params.allocationKind				= allocationKind;

	{
		const VkImageSubresourceLayers	sourceLayer	=
		{
			VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
			0u,							// deUint32				mipLevel;
			0u,							// deUint32				baseArrayLayer;
			1u							// deUint32				layerCount;
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
		params.samples					= samples[samplesIndex];
		const std::string description	= "With " + getSampleCountCaseName(samples[samplesIndex]);
		group->addChild(new ResolveImageToImageTestCase(group->getTestContext(), getSampleCountCaseName(samples[samplesIndex]), description, params));
	}
}

void addResolveImageWholeCopyBeforeResolvingTests (tcu::TestCaseGroup* group, AllocationKind allocationKind)
{
	TestParams	params;
	params.src.image.imageType			= VK_IMAGE_TYPE_2D;
	params.src.image.format				= VK_FORMAT_R8G8B8A8_UNORM;
	params.src.image.extent				= defaultExtent;
	params.src.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
	params.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	params.dst.image.imageType			= VK_IMAGE_TYPE_2D;
	params.dst.image.format				= VK_FORMAT_R8G8B8A8_UNORM;
	params.dst.image.extent				= defaultExtent;
	params.dst.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
	params.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	params.allocationKind				= allocationKind;

	{
		const VkImageSubresourceLayers	sourceLayer	=
		{
			VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
			0u,							// deUint32				mipLevel;
			0u,							// deUint32				baseArrayLayer;
			1u							// deUint32				layerCount;
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
		params.samples					= samples[samplesIndex];
		const std::string description	= "With " + getSampleCountCaseName(samples[samplesIndex]);
		group->addChild(new ResolveImageToImageTestCase(group->getTestContext(), getSampleCountCaseName(samples[samplesIndex]), description, params, COPY_MS_IMAGE_TO_MS_IMAGE));
	}
}

void addResolveImageWholeArrayImageTests (tcu::TestCaseGroup* group, AllocationKind allocationKind)
{
	TestParams	params;
	params.src.image.imageType			= VK_IMAGE_TYPE_2D;
	params.src.image.format				= VK_FORMAT_R8G8B8A8_UNORM;
	params.src.image.extent				= defaultExtent;
	params.src.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
	params.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	params.dst.image.imageType			= VK_IMAGE_TYPE_2D;
	params.dst.image.format				= VK_FORMAT_R8G8B8A8_UNORM;
	params.dst.image.extent				= defaultExtent;
	params.dst.image.extent.depth		= 5u;
	params.dst.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
	params.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	params.allocationKind				= allocationKind;

	for (deUint32 layerNdx=0; layerNdx < params.dst.image.extent.depth; ++layerNdx)
	{
		const VkImageSubresourceLayers	sourceLayer	=
		{
			VK_IMAGE_ASPECT_COLOR_BIT,		// VkImageAspectFlags	aspectMask;
			0u,								// deUint32				mipLevel;
			layerNdx,						// deUint32				baseArrayLayer;
			1u								// deUint32				layerCount;
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
		params.samples					= samples[samplesIndex];
		const std::string description	= "With " + getSampleCountCaseName(samples[samplesIndex]);
		group->addChild(new ResolveImageToImageTestCase(group->getTestContext(), getSampleCountCaseName(samples[samplesIndex]), description, params, COPY_MS_IMAGE_TO_ARRAY_MS_IMAGE));
	}
}

void addResolveImageWholeArrayImageSingleRegionTests (tcu::TestCaseGroup* group, AllocationKind allocationKind)
{
	TestParams	params;
	params.src.image.imageType			= VK_IMAGE_TYPE_2D;
	params.src.image.format				= VK_FORMAT_R8G8B8A8_UNORM;
	params.src.image.extent				= defaultExtent;
	params.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	params.dst.image.imageType			= VK_IMAGE_TYPE_2D;
	params.dst.image.format				= VK_FORMAT_R8G8B8A8_UNORM;
	params.dst.image.extent				= defaultExtent;
	params.dst.image.extent.depth		= 5u;
	params.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	params.allocationKind				= allocationKind;

	const VkImageSubresourceLayers	sourceLayer	=
	{
		VK_IMAGE_ASPECT_COLOR_BIT,		// VkImageAspectFlags	aspectMask;
		0u,								// uint32_t				mipLevel;
		0,						// uint32_t				baseArrayLayer;
		params.dst.image.extent.depth			// uint32_t				layerCount;
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

	for (int samplesIndex = 0; samplesIndex < DE_LENGTH_OF_ARRAY(samples); ++samplesIndex)
	{
		params.samples					= samples[samplesIndex];
		const std::string description	= "With " + getSampleCountCaseName(samples[samplesIndex]);
		group->addChild(new ResolveImageToImageTestCase(group->getTestContext(), getSampleCountCaseName(samples[samplesIndex]), description, params, COPY_MS_IMAGE_TO_ARRAY_MS_IMAGE));
	}
}

void addResolveImageDiffImageSizeTests (tcu::TestCaseGroup* group, AllocationKind allocationKind)
{
	tcu::TestContext&	testCtx			= group->getTestContext();
	TestParams			params;
	params.src.image.imageType			= VK_IMAGE_TYPE_2D;
	params.src.image.format				= VK_FORMAT_R8G8B8A8_UNORM;
	params.src.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
	params.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	params.dst.image.imageType			= VK_IMAGE_TYPE_2D;
	params.dst.image.format				= VK_FORMAT_R8G8B8A8_UNORM;
	params.dst.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
	params.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	params.allocationKind				= allocationKind;

	{
		const VkImageSubresourceLayers	sourceLayer	=
		{
			VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
			0u,							// deUint32				mipLevel;
			0u,							// deUint32				baseArrayLayer;
			1u							// deUint32				layerCount;
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

	const VkExtent3D imageExtents[]		=
	{
		{ resolveExtent.width + 10,	resolveExtent.height,		resolveExtent.depth },
		{ resolveExtent.width,		resolveExtent.height * 2,	resolveExtent.depth },
		{ resolveExtent.width,		resolveExtent.height,		resolveExtent.depth + 10 }
	};

	for (int srcImageExtentIndex = 0; srcImageExtentIndex < DE_LENGTH_OF_ARRAY(imageExtents); ++srcImageExtentIndex)
	{
		const VkExtent3D&	srcImageSize	= imageExtents[srcImageExtentIndex];
		params.src.image.extent				= srcImageSize;
		params.dst.image.extent				= resolveExtent;
		for (int samplesIndex = 0; samplesIndex < DE_LENGTH_OF_ARRAY(samples); ++samplesIndex)
		{
			params.samples	= samples[samplesIndex];
			std::ostringstream testName;
			testName << "src_" << srcImageSize.width << "_" << srcImageSize.height << "_" << srcImageSize.depth << "_" << getSampleCountCaseName(samples[samplesIndex]);
			std::ostringstream description;
			description << "With " << getSampleCountCaseName(samples[samplesIndex]) << " and source image size ("
						<< srcImageSize.width << ", " << srcImageSize.height << ", " << srcImageSize.depth << ")";
			group->addChild(new ResolveImageToImageTestCase(testCtx, testName.str(), description.str(), params));
		}
	}
	for (int dstImageExtentIndex = 0; dstImageExtentIndex < DE_LENGTH_OF_ARRAY(imageExtents); ++dstImageExtentIndex)
	{
		const VkExtent3D&	dstImageSize	= imageExtents[dstImageExtentIndex];
		params.src.image.extent				= resolveExtent;
		params.dst.image.extent				= dstImageSize;
		for (int samplesIndex = 0; samplesIndex < DE_LENGTH_OF_ARRAY(samples); ++samplesIndex)
		{
			params.samples	= samples[samplesIndex];
			std::ostringstream testName;
			testName << "dst_" << dstImageSize.width << "_" << dstImageSize.height << "_" << dstImageSize.depth << "_" << getSampleCountCaseName(samples[samplesIndex]);
			std::ostringstream description;
			description << "With " << getSampleCountCaseName(samples[samplesIndex]) << " and destination image size ("
						<< dstImageSize.width << ", " << dstImageSize.height << ", " << dstImageSize.depth << ")";
			group->addChild(new ResolveImageToImageTestCase(testCtx, testName.str(), description.str(), params));
		}
	}
}

void addResolveImageTests (tcu::TestCaseGroup* group, AllocationKind allocationKind)
{
	addTestGroup(group, "whole", "Resolve from image to image (whole)", addResolveImageWholeTests, allocationKind);
	addTestGroup(group, "partial", "Resolve from image to image (partial)", addResolveImagePartialTests, allocationKind);
	addTestGroup(group, "with_regions", "Resolve from image to image (with regions)", addResolveImageWithRegionsTests, allocationKind);
	addTestGroup(group, "whole_copy_before_resolving", "Resolve from image to image (whole copy before resolving)", addResolveImageWholeCopyBeforeResolvingTests, allocationKind);
	addTestGroup(group, "whole_array_image", "Resolve from image to image (whole array image)", addResolveImageWholeArrayImageTests, allocationKind);
	addTestGroup(group, "whole_array_image_one_region", "Resolve from image to image (whole array image with single region)", addResolveImageWholeArrayImageSingleRegionTests, allocationKind);
	addTestGroup(group, "diff_image_size", "Resolve from image to image of different size", addResolveImageDiffImageSizeTests, allocationKind);
}

void addCopiesAndBlittingTests (tcu::TestCaseGroup* group, AllocationKind allocationKind)
{
	addTestGroup(group, "image_to_image", "Copy from image to image", addImageToImageTests, allocationKind);
	addTestGroup(group, "image_to_buffer", "Copy from image to buffer", addImageToBufferTests, allocationKind);
	addTestGroup(group, "buffer_to_image", "Copy from buffer to image", addBufferToImageTests, allocationKind);
	addTestGroup(group, "buffer_to_depthstencil", "Copy from buffer to depth/Stencil", addBufferToDepthStencilTests, allocationKind);
	addTestGroup(group, "buffer_to_buffer", "Copy from buffer to buffer", addBufferToBufferTests, allocationKind);
	addTestGroup(group, "blit_image", "Blitting image", addBlittingImageTests, allocationKind);
	addTestGroup(group, "resolve_image", "Resolve image", addResolveImageTests, allocationKind);
}

void addCoreCopiesAndBlittingTests (tcu::TestCaseGroup* group)
{
	addCopiesAndBlittingTests(group, ALLOCATION_KIND_SUBALLOCATED);
}

void addDedicatedAllocationCopiesAndBlittingTests (tcu::TestCaseGroup* group)
{
	addCopiesAndBlittingTests(group, ALLOCATION_KIND_DEDICATED);
}

} // anonymous

tcu::TestCaseGroup* createCopiesAndBlittingTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	copiesAndBlittingTests(new tcu::TestCaseGroup(testCtx, "copy_and_blit", "Copies And Blitting Tests"));

	copiesAndBlittingTests->addChild(createTestGroup(testCtx, "core",					"Core Copies And Blitting Tests",								addCoreCopiesAndBlittingTests));
	copiesAndBlittingTests->addChild(createTestGroup(testCtx, "dedicated_allocation",	"Copies And Blitting Tests For Dedicated Memory Allocation",	addDedicatedAllocationCopiesAndBlittingTests));

	return copiesAndBlittingTests.release();
}

} // api
} // vkt
