/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015-2020 The Khronos Group Inc.
 * Copyright (c) 2020 Google Inc.
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
#include "vkBuilderUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBarrierUtil.hpp"

#include <set>
#include <array>
#include <algorithm>
#include <iterator>
#include <sstream>

namespace vkt
{

namespace api
{

namespace
{

enum FillMode
{
	FILL_MODE_GRADIENT = 0,
	FILL_MODE_WHITE,
	FILL_MODE_RED,
	FILL_MODE_MULTISAMPLE,
	FILL_MODE_BLUE_RED_X,
	FILL_MODE_BLUE_RED_Y,
	FILL_MODE_BLUE_RED_Z,

	FILL_MODE_LAST
};

enum MirrorModeBits
{
	MIRROR_MODE_X		= (1<<0),
	MIRROR_MODE_Y		= (1<<1),
	MIRROR_MODE_Z		= (1<<2),
	MIRROR_MODE_LAST	= (1<<3),
};

using MirrorMode = deUint32;

enum AllocationKind
{
	ALLOCATION_KIND_SUBALLOCATED,
	ALLOCATION_KIND_DEDICATED,
};

enum ExtensionUse
{
	EXTENSION_USE_NONE,
	EXTENSION_USE_COPY_COMMANDS2,
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

VkImageCopy2KHR convertvkImageCopyTovkImageCopy2KHR(VkImageCopy imageCopy)
{
	const VkImageCopy2KHR	imageCopy2 =
	{
		VK_STRUCTURE_TYPE_IMAGE_COPY_2_KHR,		// VkStructureType				sType;
		DE_NULL,								// const void*					pNext;
		imageCopy.srcSubresource,				// VkImageSubresourceLayers		srcSubresource;
		imageCopy.srcOffset,					// VkOffset3D					srcOffset;
		imageCopy.dstSubresource,				// VkImageSubresourceLayers		dstSubresource;
		imageCopy.dstOffset,					// VkOffset3D					dstOffset;
		imageCopy.extent						// VkExtent3D					extent;
	};
	return imageCopy2;
}
VkBufferCopy2KHR convertvkBufferCopyTovkBufferCopy2KHR(VkBufferCopy bufferCopy)
{
	const VkBufferCopy2KHR	bufferCopy2 =
	{
		VK_STRUCTURE_TYPE_BUFFER_COPY_2_KHR,	// VkStructureType				sType;
		DE_NULL,								// const void*					pNext;
		bufferCopy.srcOffset,					// VkDeviceSize					srcOffset;
		bufferCopy.dstOffset,					// VkDeviceSize					dstOffset;
		bufferCopy.size,						// VkDeviceSize					size;
	};
	return bufferCopy2;
}

VkBufferImageCopy2KHR convertvkBufferImageCopyTovkBufferImageCopy2KHR(VkBufferImageCopy bufferImageCopy)
{
	const VkBufferImageCopy2KHR	bufferImageCopy2 =
	{
		VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2_KHR,	// VkStructureType				sType;
		DE_NULL,									// const void*					pNext;
		bufferImageCopy.bufferOffset,				// VkDeviceSize					bufferOffset;
		bufferImageCopy.bufferRowLength,			// uint32_t						bufferRowLength;
		bufferImageCopy.bufferImageHeight,			// uint32_t						bufferImageHeight;
		bufferImageCopy.imageSubresource,			// VkImageSubresourceLayers		imageSubresource;
		bufferImageCopy.imageOffset,				// VkOffset3D					imageOffset;
		bufferImageCopy.imageExtent					// VkExtent3D					imageExtent;
	};
	return bufferImageCopy2;
}

VkImageBlit2KHR convertvkImageBlitTovkImageBlit2KHR(VkImageBlit imageBlit)
{
	const VkImageBlit2KHR	imageBlit2 =
	{
		VK_STRUCTURE_TYPE_IMAGE_BLIT_2_KHR,			// VkStructureType				sType;
		DE_NULL,									// const void*					pNext;
		imageBlit.srcSubresource,					// VkImageSubresourceLayers		srcSubresource;
		{											// VkOffset3D					srcOffsets[2];
			{
				imageBlit.srcOffsets[0].x,				// VkOffset3D			srcOffsets[0].x;
				imageBlit.srcOffsets[0].y,				// VkOffset3D			srcOffsets[0].y;
				imageBlit.srcOffsets[0].z				// VkOffset3D			srcOffsets[0].z;
			},
			{
				imageBlit.srcOffsets[1].x,				// VkOffset3D			srcOffsets[1].x;
				imageBlit.srcOffsets[1].y,				// VkOffset3D			srcOffsets[1].y;
				imageBlit.srcOffsets[1].z				// VkOffset3D			srcOffsets[1].z;
			}
		},
		imageBlit.dstSubresource,					// VkImageSubresourceLayers		dstSubresource;
		{											// VkOffset3D					srcOffsets[2];
			{
				imageBlit.dstOffsets[0].x,				// VkOffset3D			dstOffsets[0].x;
				imageBlit.dstOffsets[0].y,				// VkOffset3D			dstOffsets[0].y;
				imageBlit.dstOffsets[0].z				// VkOffset3D			dstOffsets[0].z;
			},
			{
				imageBlit.dstOffsets[1].x,				// VkOffset3D			dstOffsets[1].x;
				imageBlit.dstOffsets[1].y,				// VkOffset3D			dstOffsets[1].y;
				imageBlit.dstOffsets[1].z				// VkOffset3D			dstOffsets[1].z;
			}
		}
	};
	return imageBlit2;
}

VkImageResolve2KHR convertvkImageResolveTovkImageResolve2KHR(VkImageResolve imageResolve)
{
	const VkImageResolve2KHR	imageResolve2 =
	{
		VK_STRUCTURE_TYPE_IMAGE_RESOLVE_2_KHR,		// VkStructureType				sType;
		DE_NULL,									// const void*					pNext;
		imageResolve.srcSubresource,				// VkImageSubresourceLayers		srcSubresource;
		imageResolve.srcOffset,						// VkOffset3D					srcOffset;
		imageResolve.dstSubresource,				// VkImageSubresourceLayers		dstSubresource;
		imageResolve.dstOffset,						// VkOffset3D					dstOffset;
		imageResolve.extent							// VkExtent3D					extent;
	};
	return imageResolve2;
}

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
	VkBufferCopy			bufferCopy;
	VkImageCopy				imageCopy;
	VkBufferImageCopy		bufferImageCopy;
	VkImageBlit				imageBlit;
	VkImageResolve			imageResolve;
};

struct ImageParms
{
	VkImageType			imageType;
	VkFormat			format;
	VkExtent3D			extent;
	VkImageTiling		tiling;
	VkImageLayout		operationLayout;
	VkImageCreateFlags	createFlags;
	FillMode			fillMode;
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
	ExtensionUse	extensionUse;
	deUint32		mipLevels;
	deBool			singleCommand;
	deUint32		barrierCount;
	deBool			separateDepthStencilLayouts;
	deBool			clearDestination;

	TestParams (void)
	{
		mipLevels					= 1u;
		singleCommand				= DE_TRUE;
		barrierCount				= 1u;
		separateDepthStencilLayouts	= DE_FALSE;
		src.image.createFlags		= VK_IMAGE_CREATE_FLAG_BITS_MAX_ENUM;
		dst.image.createFlags		= VK_IMAGE_CREATE_FLAG_BITS_MAX_ENUM;
		src.image.fillMode			= FILL_MODE_GRADIENT;
		dst.image.fillMode			= FILL_MODE_WHITE;
		clearDestination			= DE_FALSE;
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

inline VkImageCreateFlags  getCreateFlags(const ImageParms& parms)
{
	if (parms.createFlags == VK_IMAGE_CREATE_FLAG_BITS_MAX_ENUM)
		return parms.imageType == VK_IMAGE_TYPE_2D && parms.extent.depth % 6 == 0 ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
	else
		return parms.createFlags;
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
		tcu::fillWithComponentGradients2(buffer, tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), maxValue);
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

			case FILL_MODE_BLUE_RED_X:
			case FILL_MODE_BLUE_RED_Y:
			case FILL_MODE_BLUE_RED_Z:
				bool useBlue;
				switch (mode)
				{
					case FILL_MODE_BLUE_RED_X: useBlue = (x & 1); break;
					case FILL_MODE_BLUE_RED_Y: useBlue = (y & 1); break;
					case FILL_MODE_BLUE_RED_Z: useBlue = (z & 1); break;
					default: DE_ASSERT(false); break;
				}
				if (tcu::isCombinedDepthStencilType(buffer.getFormat().type))
				{
					buffer.setPixDepth((useBlue ? blueColor[0] : redColor[0]), x, y, z);
					if (tcu::hasStencilComponent(buffer.getFormat().order))
						buffer.setPixStencil((useBlue ? (int) blueColor[3] : (int)redColor[3]), x, y, z);
				}
				else
					buffer.setPixel((useBlue ? blueColor : redColor), x, y, z);
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
	generateBuffer(m_destinationTextureLevel->getAccess(), m_params.dst.image.extent.width, m_params.dst.image.extent.height, m_params.dst.image.extent.depth, m_params.clearDestination ? FILL_MODE_WHITE : FILL_MODE_GRADIENT);
	generateExpectedResult();

	uploadImage(m_sourceTextureLevel->getAccess(), m_source.get(), m_params.src.image);
	uploadImage(m_destinationTextureLevel->getAccess(), m_destination.get(), m_params.dst.image);

	const DeviceInterface&		vk					= m_context.getDeviceInterface();
	const VkDevice				vkDevice			= m_context.getDevice();
	const VkQueue				queue				= m_context.getUniversalQueue();

	std::vector<VkImageCopy>		imageCopies;
	std::vector<VkImageCopy2KHR>	imageCopies2KHR;
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

		if (m_params.extensionUse == EXTENSION_USE_NONE)
		{
			imageCopies.push_back(imageCopy);
		}
		else
		{
			DE_ASSERT(m_params.extensionUse == EXTENSION_USE_COPY_COMMANDS2);
			imageCopies2KHR.push_back(convertvkImageCopyTovkImageCopy2KHR(imageCopy));
		}
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

	if (m_params.clearDestination)
	{
		VkImageSubresourceRange	range		= { VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u };
		VkClearColorValue		clearColor;

		clearColor.float32[0] = 1.0f;
		clearColor.float32[1] = 1.0f;
		clearColor.float32[2] = 1.0f;
		clearColor.float32[3] = 1.0f;
		vk.cmdClearColorImage(*m_cmdBuffer, m_destination.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1u, &range);
	}

	if (m_params.extensionUse == EXTENSION_USE_NONE)
	{
		vk.cmdCopyImage(*m_cmdBuffer, m_source.get(), m_params.src.image.operationLayout, m_destination.get(), m_params.dst.image.operationLayout, (deUint32)imageCopies.size(), imageCopies.data());
	}
	else
	{
		DE_ASSERT(m_params.extensionUse == EXTENSION_USE_COPY_COMMANDS2);
		const VkCopyImageInfo2KHR copyImageInfo2KHR =
		{
			VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2_KHR,	// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			m_source.get(),								// VkImage					srcImage;
			m_params.src.image.operationLayout,			// VkImageLayout			srcImageLayout;
			m_destination.get(),						// VkImage					dstImage;
			m_params.dst.image.operationLayout,			// VkImageLayout			dstImageLayout;
			(deUint32)imageCopies2KHR.size(),			// uint32_t					regionCount;
			imageCopies2KHR.data()						// const VkImageCopy2KHR*	pRegions;
		};

		vk.cmdCopyImage2KHR(*m_cmdBuffer, &copyImageInfo2KHR);
	}

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

		if (m_params.extensionUse == EXTENSION_USE_COPY_COMMANDS2)
		{
			if (!context.isDeviceFunctionalitySupported("VK_KHR_copy_commands2"))
				TCU_THROW(NotSupportedError, "VK_KHR_copy_commands2 is not supported");
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

		// Check maxImageDimension1D
		{
			if (m_params.src.image.imageType == VK_IMAGE_TYPE_1D && m_params.src.image.extent.width > limits.maxImageDimension1D)
				TCU_THROW(NotSupportedError, "Requested 1D src image dimensions not supported");

			if (m_params.dst.image.imageType == VK_IMAGE_TYPE_1D && m_params.dst.image.extent.width > limits.maxImageDimension1D)
				TCU_THROW(NotSupportedError, "Requested 1D dst image dimensions not supported");
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
	std::vector<VkBufferCopy2KHR>	bufferCopies2KHR;
	for (deUint32 i = 0; i < m_params.regions.size(); i++)
	{
		if (m_params.extensionUse == EXTENSION_USE_NONE)
		{
			bufferCopies.push_back(m_params.regions[i].bufferCopy);
		}
		else
		{
			DE_ASSERT(m_params.extensionUse == EXTENSION_USE_COPY_COMMANDS2);
			bufferCopies2KHR.push_back(convertvkBufferCopyTovkBufferCopy2KHR(m_params.regions[i].bufferCopy));
		}
	}

	beginCommandBuffer(vk, *m_cmdBuffer);
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &srcBufferBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);

	if (m_params.extensionUse == EXTENSION_USE_NONE)
	{
		vk.cmdCopyBuffer(*m_cmdBuffer, m_source.get(), m_destination.get(), (deUint32)m_params.regions.size(), &bufferCopies[0]);
	}
	else
	{
		DE_ASSERT(m_params.extensionUse == EXTENSION_USE_COPY_COMMANDS2);
		const VkCopyBufferInfo2KHR copyBufferInfo2KHR =
		{
			VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2_KHR,	// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			m_source.get(),								// VkBuffer					srcBuffer;
			m_destination.get(),						// VkBuffer					dstBuffer;
			(deUint32)m_params.regions.size(),			// uint32_t					regionCount;
			&bufferCopies2KHR[0]						// const VkBufferCopy2KHR*	pRegions;
		};

		vk.cmdCopyBuffer2KHR(*m_cmdBuffer, &copyBufferInfo2KHR);
	}

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

	virtual void			checkSupport(Context&	context) const
	{
							if (m_params.extensionUse == EXTENSION_USE_COPY_COMMANDS2)
							{
								if (!context.isDeviceFunctionalitySupported("VK_KHR_copy_commands2"))
								{
									TCU_THROW(NotSupportedError, "VK_KHR_copy_commands2 is not supported");
								}
							}
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
			0u,									// deUint32				baseMipLevel;
			1u,									// deUint32				mipLevels;
			0u,									// deUint32				baseArraySlice;
			getArraySize(m_params.src.image)	// deUint32				arraySize;
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
	std::vector<VkBufferImageCopy>		bufferImageCopies;
	std::vector<VkBufferImageCopy2KHR>	bufferImageCopies2KHR;
	for (deUint32 i = 0; i < m_params.regions.size(); i++)
	{
		if (m_params.extensionUse == EXTENSION_USE_NONE)
		{
			bufferImageCopies.push_back(m_params.regions[i].bufferImageCopy);
		}
		else
		{
			DE_ASSERT(m_params.extensionUse == EXTENSION_USE_COPY_COMMANDS2);
			bufferImageCopies2KHR.push_back(convertvkBufferImageCopyTovkBufferImageCopy2KHR(m_params.regions[i].bufferImageCopy));
		}
	}

	beginCommandBuffer(vk, *m_cmdBuffer);
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &imageBarrier);

	if (m_params.extensionUse == EXTENSION_USE_NONE)
	{
		vk.cmdCopyImageToBuffer(*m_cmdBuffer, m_source.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_destination.get(), (deUint32)m_params.regions.size(), &bufferImageCopies[0]);
	}
	else
	{
		DE_ASSERT(m_params.extensionUse == EXTENSION_USE_COPY_COMMANDS2);
		const VkCopyImageToBufferInfo2KHR copyImageToBufferInfo2KHR =
		{
			VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2_KHR,	// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			m_source.get(),										// VkImage						srcImage;
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,				// VkImageLayout				srcImageLayout;
			m_destination.get(),								// VkBuffer						dstBuffer;
			(deUint32)m_params.regions.size(),					// uint32_t						regionCount;
			&bufferImageCopies2KHR[0]							// const VkBufferImageCopy2KHR*	pRegions;
		};

		vk.cmdCopyImageToBuffer2KHR(*m_cmdBuffer, &copyImageToBufferInfo2KHR);
	}

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

	virtual void			checkSupport				(Context&				context) const
							{
								if ((m_params.extensionUse == EXTENSION_USE_COPY_COMMANDS2) &&
									(!context.isDeviceFunctionalitySupported("VK_KHR_copy_commands2")))
								{
									TCU_THROW(NotSupportedError, "VK_KHR_copy_commands2 is not supported");
								}
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

	const int			texelSize		= src.getFormat().getPixelSize();
	const VkExtent3D	extent			= region.bufferImageCopy.imageExtent;
	const VkOffset3D	srcOffset		= region.bufferImageCopy.imageOffset;
	const int			texelOffset		= (int) region.bufferImageCopy.bufferOffset / texelSize;
	const deUint32		baseArrayLayer	= region.bufferImageCopy.imageSubresource.baseArrayLayer;

	for (deUint32 z = 0; z < extent.depth; z++)
	{
		for (deUint32 y = 0; y < extent.height; y++)
		{
			int									texelIndex		= texelOffset + (z * imageHeight + y) *	rowLength;
			const tcu::ConstPixelBufferAccess	srcSubRegion	= tcu::getSubregion(src, srcOffset.x, srcOffset.y + y, srcOffset.z + z + baseArrayLayer,
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
			getArraySize(m_params.dst.image)								// deUint32				arraySize;
		}
	};

	// Copy from buffer to image
	std::vector<VkBufferImageCopy>		bufferImageCopies;
	std::vector<VkBufferImageCopy2KHR>	bufferImageCopies2KHR;
	for (deUint32 i = 0; i < m_params.regions.size(); i++)
	{
		if (m_params.extensionUse == EXTENSION_USE_NONE)
		{
			bufferImageCopies.push_back(m_params.regions[i].bufferImageCopy);
		}
		else
		{
			DE_ASSERT(m_params.extensionUse == EXTENSION_USE_COPY_COMMANDS2);
			bufferImageCopies2KHR.push_back(convertvkBufferImageCopyTovkBufferImageCopy2KHR(m_params.regions[i].bufferImageCopy));
		}
	}

	beginCommandBuffer(vk, *m_cmdBuffer);
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &imageBarrier);

	if (m_params.extensionUse == EXTENSION_USE_NONE)
	{
		vk.cmdCopyBufferToImage(*m_cmdBuffer, m_source.get(), m_destination.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (deUint32)m_params.regions.size(), bufferImageCopies.data());
	}
	else
	{
		DE_ASSERT(m_params.extensionUse == EXTENSION_USE_COPY_COMMANDS2);
		const VkCopyBufferToImageInfo2KHR copyBufferToImageInfo2KHR =
		{
			VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2_KHR,	// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			m_source.get(),										// VkBuffer						srcBuffer;
			m_destination.get(),								// VkImage						dstImage;
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,				// VkImageLayout				dstImageLayout;
			(deUint32)m_params.regions.size(),					// uint32_t						regionCount;
			bufferImageCopies2KHR.data()						// const VkBufferImageCopy2KHR*	pRegions;
		};

		vk.cmdCopyBufferToImage2KHR(*m_cmdBuffer, &copyBufferToImageInfo2KHR);
	}


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

	virtual void			checkSupport				(Context&				context) const
							{
								if ((m_params.extensionUse == EXTENSION_USE_COPY_COMMANDS2) &&
									(!context.isDeviceFunctionalitySupported("VK_KHR_copy_commands2")))
								{
									TCU_THROW(NotSupportedError, "VK_KHR_copy_commands2 is not supported");
								}
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

	const int			texelSize		= dst.getFormat().getPixelSize();
	const VkExtent3D	extent			= region.bufferImageCopy.imageExtent;
	const VkOffset3D	dstOffset		= region.bufferImageCopy.imageOffset;
	const int			texelOffset		= (int) region.bufferImageCopy.bufferOffset / texelSize;
	const deUint32		baseArrayLayer	= region.bufferImageCopy.imageSubresource.baseArrayLayer;

	for (deUint32 z = 0; z < extent.depth; z++)
	{
		for (deUint32 y = 0; y < extent.height; y++)
		{
			int texelIndex = texelOffset + (z * imageHeight + y) * rowLength;
			const tcu::ConstPixelBufferAccess srcSubRegion = tcu::getSubregion(src, texelIndex, 0, region.bufferImageCopy.imageExtent.width, 1);
			const tcu::PixelBufferAccess dstSubRegion = tcu::getSubregion(dst, dstOffset.x, dstOffset.y + y, dstOffset.z + z + baseArrayLayer,
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
	std::vector<VkBufferImageCopy>		bufferImageCopies;
	std::vector<VkBufferImageCopy2KHR>	bufferImageCopies2KHR;
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
	// we take the given copy regions and use that as the desired order
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
			copyData.bufferOffset += depthOffset;
		}
		else if (!stencilLoaded)
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

			// Reference image generation uses pixel offsets based on buffer offset.
			// We need to adjust the offset now that the stencil data is not interleaved.
			copyData.bufferOffset /= tcu::getPixelSize(m_textureFormat);

			copyData.bufferOffset += stencilOffset;
		}

		if (m_params.extensionUse == EXTENSION_USE_NONE)
		{
			bufferImageCopies.push_back(copyData);
		}
		else
		{
			DE_ASSERT(m_params.extensionUse == EXTENSION_USE_COPY_COMMANDS2);
			bufferImageCopies2KHR.push_back(convertvkBufferImageCopyTovkBufferImageCopy2KHR(copyData));
		}
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

	if (m_params.extensionUse == EXTENSION_USE_NONE)
	{
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
	}
	else
	{
		DE_ASSERT(m_params.extensionUse == EXTENSION_USE_COPY_COMMANDS2);

		if (m_params.singleCommand)
		{
			// Issue a single copy command with regions defined by the test.
			const VkCopyBufferToImageInfo2KHR copyBufferToImageInfo2KHR =
			{
				VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2_KHR,	// VkStructureType				sType;
				DE_NULL,											// const void*					pNext;
				m_source.get(),										// VkBuffer						srcBuffer;
				m_destination.get(),								// VkImage						dstImage;
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,				// VkImageLayout				dstImageLayout;
				(deUint32)m_params.regions.size(),					// uint32_t						regionCount;
				bufferImageCopies2KHR.data()						// const VkBufferImageCopy2KHR*	pRegions;
			};
			vk.cmdCopyBufferToImage2KHR(*m_cmdBuffer, &copyBufferToImageInfo2KHR);
		}
		else
		{
			// Issue a a copy command per region defined by the test.
			for (deUint32 i = 0; i < bufferImageCopies2KHR.size(); i++)
			{
				const VkCopyBufferToImageInfo2KHR copyBufferToImageInfo2KHR =
				{
					VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2_KHR,	// VkStructureType				sType;
					DE_NULL,											// const void*					pNext;
					m_source.get(),										// VkBuffer						srcBuffer;
					m_destination.get(),								// VkImage						dstImage;
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,				// VkImageLayout				dstImageLayout;
					1,													// uint32_t						regionCount;
					&bufferImageCopies2KHR[i]							// const VkBufferImageCopy2KHR*	pRegions;
				};
				// Issue a single copy command with regions defined by the test.
				vk.cmdCopyBufferToImage2KHR(*m_cmdBuffer, &copyBufferToImageInfo2KHR);
			}
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

	virtual void			checkSupport						(Context&				context) const
							{
								if ((m_params.extensionUse == EXTENSION_USE_COPY_COMMANDS2) &&
									(!context.isDeviceFunctionalitySupported("VK_KHR_copy_commands2")))
								{
									TCU_THROW(NotSupportedError, "VK_KHR_copy_commands2 is not supported");
								}
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
	bool								checkNonNearestFilteredResult	(const tcu::ConstPixelBufferAccess&	result,
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
	generateBuffer(m_sourceTextureLevel->getAccess(), m_params.src.image.extent.width, m_params.src.image.extent.height, m_params.src.image.extent.depth, m_params.src.image.fillMode);
	m_destinationTextureLevel = de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(dstTcuFormat,
																					 (int)m_params.dst.image.extent.width,
																					 (int)m_params.dst.image.extent.height,
																					 (int)m_params.dst.image.extent.depth));
	generateBuffer(m_destinationTextureLevel->getAccess(), m_params.dst.image.extent.width, m_params.dst.image.extent.height, m_params.dst.image.extent.depth, m_params.dst.image.fillMode);
	generateExpectedResult();

	uploadImage(m_sourceTextureLevel->getAccess(), m_source.get(), m_params.src.image);
	uploadImage(m_destinationTextureLevel->getAccess(), m_destination.get(), m_params.dst.image);

	const DeviceInterface&		vk					= m_context.getDeviceInterface();
	const VkDevice				vkDevice			= m_context.getDevice();
	const VkQueue				queue				= m_context.getUniversalQueue();

	std::vector<VkImageBlit>		regions;
	std::vector<VkImageBlit2KHR>	regions2KHR;
	for (deUint32 i = 0; i < m_params.regions.size(); i++)
	{
		if (m_params.extensionUse == EXTENSION_USE_NONE)
		{
			regions.push_back(m_params.regions[i].imageBlit);
		}
		else
		{
			DE_ASSERT(m_params.extensionUse == EXTENSION_USE_COPY_COMMANDS2);
			regions2KHR.push_back(convertvkImageBlitTovkImageBlit2KHR(m_params.regions[i].imageBlit));
		}
	}

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

	if (m_params.extensionUse == EXTENSION_USE_NONE)
	{
		vk.cmdBlitImage(*m_cmdBuffer, m_source.get(), m_params.src.image.operationLayout, m_destination.get(), m_params.dst.image.operationLayout, (deUint32)m_params.regions.size(), &regions[0], m_params.filter);
	}
	else
	{
		DE_ASSERT(m_params.extensionUse == EXTENSION_USE_COPY_COMMANDS2);
		const VkBlitImageInfo2KHR BlitImageInfo2KHR =
		{
			VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2_KHR,	// VkStructureType				sType;
			DE_NULL,									// const void*					pNext;
			m_source.get(),								// VkImage						srcImage;
			m_params.src.image.operationLayout,			// VkImageLayout				srcImageLayout;
			m_destination.get(),						// VkImage						dstImage;
			m_params.dst.image.operationLayout,			// VkImageLayout				dstImageLayout;
			(deUint32)m_params.regions.size(),			// uint32_t						regionCount;
			&regions2KHR[0],							// const VkImageBlit2KHR*		pRegions;
			m_params.filter,							// VkFilter						filter;
		};
		vk.cmdBlitImage2KHR(*m_cmdBuffer, &BlitImageInfo2KHR);
	}

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

bool BlittingImages::checkNonNearestFilteredResult (const tcu::ConstPixelBufferAccess&	result,
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

	// if either of srcImage or dstImage stores values as a signed/unsigned integer,
	// the other must also store values a signed/unsigned integer
	// e.g. blit unorm to uscaled is not allowed as uscaled formats store data as integers
	// despite the fact that both formats are sampled as floats
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
		const tcu::Vec4	threshold	= ( srcMaxDiff + dstMaxDiff ) * ((m_params.filter == VK_FILTER_CUBIC_EXT) ? 1.5f : 1.0f);

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
		const tcu::IVec4	dstBitDepth	= tcu::getTextureFormatBitDepth(dstFormat);
		const tcu::IVec4	srcBitDepth = tcu::getTextureFormatBitDepth(srcFormat);
		for (deUint32 i = 0; i < 4; ++i)
			threshold[i] = 1 + de::max( ( ( 1 << dstBitDepth[i] ) - 1 ) / de::clamp((1 << srcBitDepth[i]) - 1, 1, 256), 1);

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

	// if either of srcImage or dstImage stores values as a signed/unsigned integer,
	// the other must also store values a signed/unsigned integer
	// e.g. blit unorm to uscaled is not allowed as uscaled formats store data as integers
	// despite the fact that both formats are sampled as floats
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
	DE_ASSERT(m_params.filter == VK_FILTER_NEAREST || m_params.filter == VK_FILTER_LINEAR || m_params.filter == VK_FILTER_CUBIC_EXT);
	const std::string failMessage("Result image is incorrect");

	if (m_params.filter != VK_FILTER_NEAREST)
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

				if (!checkNonNearestFilteredResult(depthResult, clampedExpected, unclampedExpected, sourceFormat))
					return tcu::TestStatus::fail(failMessage);
			}

			if (tcu::hasStencilComponent(result.getFormat().order))
			{
				const tcu::Sampler::DepthStencilMode	mode				= tcu::Sampler::MODE_STENCIL;
				const tcu::ConstPixelBufferAccess		stencilResult		= tcu::getEffectiveDepthStencilAccess(result, mode);
				const tcu::ConstPixelBufferAccess		clampedExpected		= tcu::getEffectiveDepthStencilAccess(m_expectedTextureLevel[0]->getAccess(), mode);
				const tcu::ConstPixelBufferAccess		unclampedExpected	= tcu::getEffectiveDepthStencilAccess(m_unclampedExpectedTextureLevel->getAccess(), mode);
				const tcu::TextureFormat				sourceFormat		= tcu::getEffectiveDepthStencilTextureFormat(mapVkFormat(m_params.src.image.format), mode);

				if (!checkNonNearestFilteredResult(stencilResult, clampedExpected, unclampedExpected, sourceFormat))
					return tcu::TestStatus::fail(failMessage);
			}
		}
		else
		{
			const tcu::TextureFormat	sourceFormat	= mapVkFormat(m_params.src.image.format);

			if (!checkNonNearestFilteredResult(result, m_expectedTextureLevel[0]->getAccess(), m_unclampedExpectedTextureLevel->getAccess(), sourceFormat))
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

void scaleFromWholeSrcBuffer (const tcu::PixelBufferAccess& dst, const tcu::ConstPixelBufferAccess& src, const VkOffset3D regionOffset, const VkOffset3D regionExtent, tcu::Sampler::FilterMode filter, const MirrorMode mirrorMode = 0u)
{
	DE_ASSERT(filter == tcu::Sampler::LINEAR || filter == tcu::Sampler::CUBIC);

	tcu::Sampler sampler(tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE,
					filter, filter, 0.0f, false);

	float sX = (float)regionExtent.x / (float)dst.getWidth();
	float sY = (float)regionExtent.y / (float)dst.getHeight();
	float sZ = (float)regionExtent.z / (float)dst.getDepth();

	for (int z = 0; z < dst.getDepth(); z++)
	for (int y = 0; y < dst.getHeight(); y++)
	for (int x = 0; x < dst.getWidth(); x++)
	{
		float srcX = ((mirrorMode & MIRROR_MODE_X) != 0) ? (float)regionExtent.x + (float)regionOffset.x - ((float)x+0.5f)*sX : (float)regionOffset.x + ((float)x+0.5f)*sX;
		float srcY = ((mirrorMode & MIRROR_MODE_Y) != 0) ? (float)regionExtent.y + (float)regionOffset.y - ((float)y+0.5f)*sY : (float)regionOffset.y + ((float)y+0.5f)*sY;
		float srcZ = ((mirrorMode & MIRROR_MODE_Z) != 0) ? (float)regionExtent.z + (float)regionOffset.z - ((float)z+0.5f)*sZ : (float)regionOffset.z + ((float)z+0.5f)*sZ;
		if (dst.getDepth() > 1)
			dst.setPixel(linearToSRGBIfNeeded(dst.getFormat(), src.sample3D(sampler, filter, srcX, srcY, srcZ)), x, y, z);
		else
			dst.setPixel(linearToSRGBIfNeeded(dst.getFormat(), src.sample2D(sampler, filter, srcX, srcY, 0)), x, y);
	}
}

void blit (const tcu::PixelBufferAccess& dst, const tcu::ConstPixelBufferAccess& src, const tcu::Sampler::FilterMode filter, const MirrorMode mirrorMode)
{
	DE_ASSERT(filter == tcu::Sampler::NEAREST || filter == tcu::Sampler::LINEAR || filter == tcu::Sampler::CUBIC);

	tcu::Sampler sampler(tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE,
			filter, filter, 0.0f, false);

	const float sX = (float)src.getWidth() / (float)dst.getWidth();
	const float sY = (float)src.getHeight() / (float)dst.getHeight();
	const float sZ = (float)src.getDepth() / (float)dst.getDepth();

	const int xOffset = (mirrorMode & MIRROR_MODE_X) ? dst.getWidth() - 1 : 0;
	const int yOffset = (mirrorMode & MIRROR_MODE_Y) ? dst.getHeight() - 1 : 0;
	const int zOffset = (mirrorMode & MIRROR_MODE_Z) ? dst.getDepth() - 1 : 0;

	const int xScale = (mirrorMode & MIRROR_MODE_X) ? -1 : 1;
	const int yScale = (mirrorMode & MIRROR_MODE_Y) ? -1 : 1;
	const int zScale = (mirrorMode & MIRROR_MODE_Z) ? -1 : 1;

	for (int z = 0; z < dst.getDepth(); ++z)
	for (int y = 0; y < dst.getHeight(); ++y)
	for (int x = 0; x < dst.getWidth(); ++x)
	{
		dst.setPixel(linearToSRGBIfNeeded(dst.getFormat(), src.sample3D(sampler, filter, ((float)x + 0.5f) * sX, ((float)y + 0.5f) * sY, ((float)z + 0.5f) * sZ)), x * xScale + xOffset, y * yScale + yOffset, z * zScale + zOffset);
	}
}

void flipCoordinates (CopyRegion& region, const MirrorMode mirrorMode)
{
	const VkOffset3D dstOffset0 = region.imageBlit.dstOffsets[0];
	const VkOffset3D dstOffset1 = region.imageBlit.dstOffsets[1];
	const VkOffset3D srcOffset0 = region.imageBlit.srcOffsets[0];
	const VkOffset3D srcOffset1 = region.imageBlit.srcOffsets[1];

	if (mirrorMode != 0u)
	{
		//sourceRegion
		region.imageBlit.srcOffsets[0].x = std::min(srcOffset0.x, srcOffset1.x);
		region.imageBlit.srcOffsets[0].y = std::min(srcOffset0.y, srcOffset1.y);
		region.imageBlit.srcOffsets[0].z = std::min(srcOffset0.z, srcOffset1.z);

		region.imageBlit.srcOffsets[1].x = std::max(srcOffset0.x, srcOffset1.x);
		region.imageBlit.srcOffsets[1].y = std::max(srcOffset0.y, srcOffset1.y);
		region.imageBlit.srcOffsets[1].z = std::max(srcOffset0.z, srcOffset1.z);

		//destinationRegion
		region.imageBlit.dstOffsets[0].x = std::min(dstOffset0.x, dstOffset1.x);
		region.imageBlit.dstOffsets[0].y = std::min(dstOffset0.y, dstOffset1.y);
		region.imageBlit.dstOffsets[0].z = std::min(dstOffset0.z, dstOffset1.z);

		region.imageBlit.dstOffsets[1].x = std::max(dstOffset0.x, dstOffset1.x);
		region.imageBlit.dstOffsets[1].y = std::max(dstOffset0.y, dstOffset1.y);
		region.imageBlit.dstOffsets[1].z = std::max(dstOffset0.z, dstOffset1.z);
	}
}

// Mirror X, Y and Z as required by the offset values in the 3 axes.
MirrorMode getMirrorMode(const VkOffset3D from, const VkOffset3D to)
{
	MirrorMode mode = 0u;

	if (from.x > to.x)
		mode |= MIRROR_MODE_X;

	if (from.y > to.y)
		mode |= MIRROR_MODE_Y;

	if (from.z > to.z)
		mode |= MIRROR_MODE_Z;

	return mode;
}

// Mirror the axes that are mirrored either in the source or destination, but not both.
MirrorMode getMirrorMode(const VkOffset3D s1, const VkOffset3D s2, const VkOffset3D d1, const VkOffset3D d2)
{
	static const MirrorModeBits kBits[] = { MIRROR_MODE_X, MIRROR_MODE_Y, MIRROR_MODE_Z };

	const MirrorMode source		 = getMirrorMode(s1, s2);
	const MirrorMode destination = getMirrorMode(d1, d2);

	MirrorMode mode = 0u;

	for (int i = 0; i < DE_LENGTH_OF_ARRAY(kBits); ++i)
	{
		const MirrorModeBits bit = kBits[i];
		if ((source & bit) != (destination & bit))
			mode |= bit;
	}

	return mode;
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
		region.imageBlit.srcOffsets[1].z - srcOffset.z,
	};
	const VkOffset3D					dstOffset		= region.imageBlit.dstOffsets[0];
	const VkOffset3D					dstExtent		=
	{
		region.imageBlit.dstOffsets[1].x - dstOffset.x,
		region.imageBlit.dstOffsets[1].y - dstOffset.y,
		region.imageBlit.dstOffsets[1].z - dstOffset.z,
	};

	tcu::Sampler::FilterMode		filter;
	switch (m_params.filter)
	{
		case VK_FILTER_LINEAR:		filter = tcu::Sampler::LINEAR; break;
		case VK_FILTER_CUBIC_EXT:	filter = tcu::Sampler::CUBIC;  break;
		case VK_FILTER_NEAREST:
		default:					filter = tcu::Sampler::NEAREST;  break;
	}

	if (tcu::isCombinedDepthStencilType(src.getFormat().type))
	{
		DE_ASSERT(src.getFormat() == dst.getFormat());

		// Scale depth.
		if (tcu::hasDepthComponent(src.getFormat().order))
		{
			const tcu::ConstPixelBufferAccess	srcSubRegion	= getEffectiveDepthStencilAccess(tcu::getSubregion(src, srcOffset.x, srcOffset.y, srcOffset.z, srcExtent.x, srcExtent.y, srcExtent.z), tcu::Sampler::MODE_DEPTH);
			const tcu::PixelBufferAccess		dstSubRegion	= getEffectiveDepthStencilAccess(tcu::getSubregion(dst, dstOffset.x, dstOffset.y, dstOffset.z, dstExtent.x, dstExtent.y, dstExtent.z), tcu::Sampler::MODE_DEPTH);
			tcu::scale(dstSubRegion, srcSubRegion, filter);

			if (filter != tcu::Sampler::NEAREST)
			{
				const tcu::ConstPixelBufferAccess	depthSrc			= getEffectiveDepthStencilAccess(src, tcu::Sampler::MODE_DEPTH);
				const tcu::PixelBufferAccess		unclampedSubRegion	= getEffectiveDepthStencilAccess(tcu::getSubregion(m_unclampedExpectedTextureLevel->getAccess(), dstOffset.x, dstOffset.y, dstOffset.z, dstExtent.x, dstExtent.y, dstExtent.z), tcu::Sampler::MODE_DEPTH);
				scaleFromWholeSrcBuffer(unclampedSubRegion, depthSrc, srcOffset, srcExtent, filter, mirrorMode);
			}
		}

		// Scale stencil.
		if (tcu::hasStencilComponent(src.getFormat().order))
		{
			const tcu::ConstPixelBufferAccess	srcSubRegion	= getEffectiveDepthStencilAccess(tcu::getSubregion(src, srcOffset.x, srcOffset.y, srcOffset.z, srcExtent.x, srcExtent.y, srcExtent.z), tcu::Sampler::MODE_STENCIL);
			const tcu::PixelBufferAccess		dstSubRegion	= getEffectiveDepthStencilAccess(tcu::getSubregion(dst, dstOffset.x, dstOffset.y, dstOffset.z, dstExtent.x, dstExtent.y, dstExtent.z), tcu::Sampler::MODE_STENCIL);
			blit(dstSubRegion, srcSubRegion, filter, mirrorMode);

			if (filter != tcu::Sampler::NEAREST)
			{
				const tcu::ConstPixelBufferAccess	stencilSrc			= getEffectiveDepthStencilAccess(src, tcu::Sampler::MODE_STENCIL);
				const tcu::PixelBufferAccess		unclampedSubRegion	= getEffectiveDepthStencilAccess(tcu::getSubregion(m_unclampedExpectedTextureLevel->getAccess(), dstOffset.x, dstOffset.y, dstOffset.z, dstExtent.x, dstExtent.y, dstExtent.z), tcu::Sampler::MODE_STENCIL);
				scaleFromWholeSrcBuffer(unclampedSubRegion, stencilSrc, srcOffset, srcExtent, filter, mirrorMode);
			}
		}
	}
	else
	{
		const tcu::ConstPixelBufferAccess	srcSubRegion	= tcu::getSubregion(src, srcOffset.x, srcOffset.y, srcOffset.z, srcExtent.x, srcExtent.y, srcExtent.z);
		const tcu::PixelBufferAccess		dstSubRegion	= tcu::getSubregion(dst, dstOffset.x, dstOffset.y, dstOffset.z, dstExtent.x, dstExtent.y, dstExtent.z);
		blit(dstSubRegion, srcSubRegion, filter, mirrorMode);

		if (filter != tcu::Sampler::NEAREST)
		{
			const tcu::PixelBufferAccess	unclampedSubRegion	= tcu::getSubregion(m_unclampedExpectedTextureLevel->getAccess(), dstOffset.x, dstOffset.y, dstOffset.z, dstExtent.x, dstExtent.y, dstExtent.z);
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

	if (m_params.filter != VK_FILTER_NEAREST)
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
		{
			TCU_THROW(NotSupportedError, "Source format feature sampled image filter linear not supported");
		}

		if (m_params.filter == VK_FILTER_CUBIC_EXT)
		{
			context.requireDeviceFunctionality("VK_EXT_filter_cubic");

			if (!(srcFormatFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_CUBIC_BIT_EXT))
			{
				TCU_THROW(NotSupportedError, "Source format feature sampled image filter cubic not supported");
			}
		}

		if (m_params.extensionUse == EXTENSION_USE_COPY_COMMANDS2)
		{
			if (!context.isDeviceFunctionalitySupported("VK_KHR_copy_commands2"))
			{
				TCU_THROW(NotSupportedError, "VK_KHR_copy_commands2 is not supported");
			}
		}
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
	bool								checkNonNearestFilteredResult	(void);
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
	generateBuffer(m_sourceTextureLevel->getAccess(), m_params.src.image.extent.width, m_params.src.image.extent.height, m_params.src.image.extent.depth, m_params.src.image.fillMode);
	m_destinationTextureLevel = de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(dstTcuFormat,
																						(int)m_params.dst.image.extent.width,
																						(int)m_params.dst.image.extent.height,
																						(int)m_params.dst.image.extent.depth));
	generateBuffer(m_destinationTextureLevel->getAccess(), m_params.dst.image.extent.width, m_params.dst.image.extent.height, m_params.dst.image.extent.depth, m_params.dst.image.fillMode);
	generateExpectedResult();

	uploadImage(m_sourceTextureLevel->getAccess(), m_source.get(), m_params.src.image);

	uploadImage(m_destinationTextureLevel->getAccess(), m_destination.get(), m_params.dst.image, m_params.mipLevels);

	const DeviceInterface&		vk					= m_context.getDeviceInterface();
	const VkDevice				vkDevice			= m_context.getDevice();
	const VkQueue				queue				= m_context.getUniversalQueue();

	std::vector<VkImageBlit>		regions;
	std::vector<VkImageBlit2KHR>	regions2KHR;
	for (deUint32 i = 0; i < m_params.regions.size(); i++)
	{
		if (m_params.extensionUse == EXTENSION_USE_NONE)
		{
			regions.push_back(m_params.regions[i].imageBlit);
		}
		else
		{
			DE_ASSERT(m_params.extensionUse == EXTENSION_USE_COPY_COMMANDS2);
			regions2KHR.push_back(convertvkImageBlitTovkImageBlit2KHR(m_params.regions[i].imageBlit));
		}
	}

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

			if (m_params.extensionUse == EXTENSION_USE_NONE)
			{
				vk.cmdBlitImage(*m_cmdBuffer, m_source.get(), m_params.src.image.operationLayout, m_destination.get(), m_params.dst.image.operationLayout, (deUint32)m_params.regions.size(), &regions[0], m_params.filter);
			}
			else
			{
				DE_ASSERT(m_params.extensionUse == EXTENSION_USE_COPY_COMMANDS2);
				const VkBlitImageInfo2KHR BlitImageInfo2KHR =
				{
					VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2_KHR,	// VkStructureType				sType;
					DE_NULL,									// const void*					pNext;
					m_source.get(),								// VkImage						srcImage;
					m_params.src.image.operationLayout,			// VkImageLayout				srcImageLayout;
					m_destination.get(),						// VkImage						dstImage;
					m_params.dst.image.operationLayout,			// VkImageLayout				dstImageLayout;
					(deUint32)m_params.regions.size(),			// uint32_t						regionCount;
					&regions2KHR[0],							// const VkImageBlit2KHR*		pRegions;
					m_params.filter								// VkFilter						filter;
				};
				vk.cmdBlitImage2KHR(*m_cmdBuffer, &BlitImageInfo2KHR);
			}
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

		for (deUint32 regionNdx = 0u; regionNdx < (deUint32)m_params.regions.size(); regionNdx++)
		{
			const deUint32	mipLevel	= m_params.regions[regionNdx].imageBlit.dstSubresource.mipLevel;

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

			if (m_params.extensionUse == EXTENSION_USE_NONE)
			{
				vk.cmdBlitImage(*m_cmdBuffer, m_destination.get(), m_params.src.image.operationLayout, m_destination.get(), m_params.dst.image.operationLayout, 1u, &regions[regionNdx], m_params.filter);
			}
			else
			{
				DE_ASSERT(m_params.extensionUse == EXTENSION_USE_COPY_COMMANDS2);
				const VkBlitImageInfo2KHR BlitImageInfo2KHR =
				{
					VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2_KHR,	// VkStructureType				sType;
					DE_NULL,									// const void*					pNext;
					m_destination.get(),						// VkImage						srcImage;
					m_params.src.image.operationLayout,			// VkImageLayout				srcImageLayout;
					m_destination.get(),						// VkImage						dstImage;
					m_params.dst.image.operationLayout,			// VkImageLayout				dstImageLayout;
					1u,											// uint32_t						regionCount;
					&regions2KHR[regionNdx],					// const VkImageBlit2KHR*		pRegions;
					m_params.filter								// VkFilter						filter;
				};
				vk.cmdBlitImage2KHR(*m_cmdBuffer, &BlitImageInfo2KHR);
			}

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

bool BlittingMipmaps::checkNonNearestFilteredResult (void)
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
			const tcu::Vec4 threshold   = ( srcMaxDiff + dstMaxDiff ) * ((m_params.filter == VK_FILTER_CUBIC_EXT)? 1.5f : 1.0f);

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
			const tcu::IVec4	dstBitDepth	= tcu::getTextureFormatBitDepth(dstFormat);
			const tcu::IVec4	srcBitDepth = tcu::getTextureFormatBitDepth(srcFormat);
			for (deUint32 i = 0; i < 4; ++i)
				threshold[i] = 1 + de::max(((1 << dstBitDepth[i]) - 1) / de::clamp((1 << srcBitDepth[i]) - 1, 1, 256), 1);

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
	DE_ASSERT(m_params.filter == VK_FILTER_NEAREST || m_params.filter == VK_FILTER_LINEAR || m_params.filter == VK_FILTER_CUBIC_EXT);
	const std::string failMessage("Result image is incorrect");

	if (m_params.filter != VK_FILTER_NEAREST)
	{
		if (!checkNonNearestFilteredResult())
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

	tcu::Sampler::FilterMode		filter;
	switch (m_params.filter)
	{
	case VK_FILTER_LINEAR:		filter = tcu::Sampler::LINEAR; break;
	case VK_FILTER_CUBIC_EXT:	filter = tcu::Sampler::CUBIC;  break;
	case VK_FILTER_NEAREST:
	default:					filter = tcu::Sampler::NEAREST;  break;
	}

	if (tcu::isCombinedDepthStencilType(src.getFormat().type))
	{
		DE_ASSERT(src.getFormat() == dst.getFormat());
		// Scale depth.
		if (tcu::hasDepthComponent(src.getFormat().order))
		{
			const tcu::ConstPixelBufferAccess	srcSubRegion	= getEffectiveDepthStencilAccess(tcu::getSubregion(src, srcOffset.x, srcOffset.y, srcExtent.x, srcExtent.y), tcu::Sampler::MODE_DEPTH);
			const tcu::PixelBufferAccess		dstSubRegion	= getEffectiveDepthStencilAccess(tcu::getSubregion(dst, dstOffset.x, dstOffset.y, dstExtent.x, dstExtent.y), tcu::Sampler::MODE_DEPTH);
			tcu::scale(dstSubRegion, srcSubRegion, filter);

			if (filter != tcu::Sampler::NEAREST)
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

			if (filter != tcu::Sampler::NEAREST)
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

			if (filter != tcu::Sampler::NEAREST)
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

	if (m_params.filter != VK_FILTER_NEAREST)
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
			else if ((m_params.extensionUse == EXTENSION_USE_COPY_COMMANDS2)	&&
					 (!context.isDeviceFunctionalitySupported("VK_KHR_copy_commands2")))
			{
				TCU_THROW(NotSupportedError, "VK_KHR_copy_commands2 is not supported");
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

		if (m_params.filter == VK_FILTER_CUBIC_EXT)
		{
			context.requireDeviceFunctionality("VK_EXT_filter_cubic");

			if (!(srcFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_CUBIC_BIT_EXT))
			{
				TCU_THROW(NotSupportedError, "Source format feature sampled image filter cubic not supported");
			}
		}
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
	tcu::TestStatus								checkIntermediateCopy		(void);
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
	const InstanceInterface&	vki						= m_context.getInstanceInterface();
	const DeviceInterface&		vk						= m_context.getDeviceInterface();
	const VkPhysicalDevice		vkPhysDevice			= m_context.getPhysicalDevice();
	const VkDevice				vkDevice				= m_context.getDevice();
	const deUint32				queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
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
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT										// VkImageUsageFlags		usage;
				| VK_IMAGE_USAGE_TRANSFER_SRC_BIT
				| VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
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
				colorImageParams.usage			= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
				m_multisampledCopyImage			= createImage(vk, vkDevice, &colorImageParams);
				// Allocate and bind color image memory.
				m_multisampledCopyImageAlloc	= allocateImage(vki, vk, vkPhysDevice, vkDevice, *m_multisampledCopyImage, MemoryRequirement::Any, memAlloc, m_params.allocationKind);
				VK_CHECK(vk.bindImageMemory(vkDevice, *m_multisampledCopyImage, m_multisampledCopyImageAlloc->getMemory(), m_multisampledCopyImageAlloc->getOffset()));
				break;
			}

			case COPY_MS_IMAGE_TO_ARRAY_MS_IMAGE:
			{
				colorImageParams.usage			= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
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
	std::vector<VkImageResolve2KHR>	imageResolves2KHR;
	for (deUint32 i = 0; i < m_params.regions.size(); i++)
	{
		if (m_params.extensionUse == EXTENSION_USE_NONE)
		{
			imageResolves.push_back(m_params.regions[i].imageResolve);
		}
		else
		{
			DE_ASSERT(m_params.extensionUse == EXTENSION_USE_COPY_COMMANDS2);
			imageResolves2KHR.push_back(convertvkImageResolveTovkImageResolve2KHR(m_params.regions[i].imageResolve));
		}
	}

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

	if (m_params.extensionUse == EXTENSION_USE_NONE)
	{
		vk.cmdResolveImage(*m_cmdBuffer, sourceImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_destination.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (deUint32)m_params.regions.size(), imageResolves.data());
	}
	else
	{
		DE_ASSERT(m_params.extensionUse == EXTENSION_USE_COPY_COMMANDS2);
		const VkResolveImageInfo2KHR ResolveImageInfo2KHR =
		{
			VK_STRUCTURE_TYPE_RESOLVE_IMAGE_INFO_2_KHR,	// VkStructureType				sType;
			DE_NULL,									// const void*					pNext;
			sourceImage,								// VkImage						srcImage;
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,		// VkImageLayout				srcImageLayout;
			m_destination.get(),						// VkImage						dstImage;
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout				dstImageLayout;
			(deUint32)m_params.regions.size(),			// uint32_t						regionCount;
			imageResolves2KHR.data()					// const  VkImageResolve2KHR*	pRegions;
		};
		vk.cmdResolveImage2KHR(*m_cmdBuffer, &ResolveImageInfo2KHR);
	}

	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &postImageBarrier);
	endCommandBuffer(vk, *m_cmdBuffer);
	submitCommandsAndWait(vk, vkDevice, queue, *m_cmdBuffer);

	de::MovePtr<tcu::TextureLevel>	resultTextureLevel	= readImage(*m_destination, m_params.dst.image);

	if (m_options == COPY_MS_IMAGE_TO_MS_IMAGE || m_options == COPY_MS_IMAGE_TO_ARRAY_MS_IMAGE)
	{
		// Verify the intermediate multisample copy operation happens properly instead of, for example, shuffling samples around or
		// resolving the image and giving every sample the same value.
		const auto intermediateResult = checkIntermediateCopy();
		if (intermediateResult.getCode() != QP_TEST_RESULT_PASS)
			return intermediateResult;
	}

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

tcu::TestStatus ResolveImageToImage::checkIntermediateCopy (void)
{
	const		auto&	vkd					= m_context.getDeviceInterface();
	const		auto	device				= m_context.getDevice();
	const		auto	queue				= m_context.getUniversalQueue();
	const		auto	queueIndex			= m_context.getUniversalQueueFamilyIndex();
				auto&	alloc				= m_context.getDefaultAllocator();
	const		auto	currentLayout		= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	const		auto	numDstLayers		= getArraySize(m_params.dst.image);
	const		auto	numInputAttachments	= numDstLayers + 1u; // For the source image.
	constexpr	auto	numSets				= 2u; // 1 for the output buffer, 1 for the input attachments.
	const		auto	fbWidth				= m_params.src.image.extent.width;
	const		auto	fbHeight			= m_params.src.image.extent.height;

	// Push constants.
	const std::array<int, 3> pushConstantData =
	{{
		static_cast<int>(fbWidth),
		static_cast<int>(fbHeight),
		static_cast<int>(m_params.samples),
	}};
	const auto pushConstantSize = static_cast<deUint32>(pushConstantData.size() * sizeof(decltype(pushConstantData)::value_type));

	// Shader modules.
	const auto vertexModule			= createShaderModule(vkd, device, m_context.getBinaryCollection().get("vert"), 0u);
	const auto verificationModule	= createShaderModule(vkd, device, m_context.getBinaryCollection().get("verify"), 0u);

	// Descriptor sets.
	DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, numInputAttachments);
	const auto descriptorPool = poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, numSets);

	DescriptorSetLayoutBuilder layoutBuilderBuffer;
	layoutBuilderBuffer.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
	const auto outputBufferSetLayout = layoutBuilderBuffer.build(vkd, device);

	DescriptorSetLayoutBuilder layoutBuilderAttachments;
	for (deUint32 i = 0u; i < numInputAttachments; ++i)
		layoutBuilderAttachments.addSingleBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
	const auto inputAttachmentsSetLayout = layoutBuilderAttachments.build(vkd, device);

	const auto descriptorSetBuffer		= makeDescriptorSet(vkd, device, descriptorPool.get(), outputBufferSetLayout.get());
	const auto descriptorSetAttachments	= makeDescriptorSet(vkd, device, descriptorPool.get(), inputAttachmentsSetLayout.get());

	// Array with raw descriptor sets.
	const std::array<VkDescriptorSet, numSets> descriptorSets =
	{{
		descriptorSetBuffer.get(),
		descriptorSetAttachments.get(),
	}};

	// Pipeline layout.
	const std::array<VkDescriptorSetLayout, numSets> setLayouts =
	{{
		outputBufferSetLayout.get(),
		inputAttachmentsSetLayout.get(),
	}};

	const VkPushConstantRange pushConstantRange =
	{
		VK_SHADER_STAGE_FRAGMENT_BIT,	//	VkShaderStageFlags	stageFlags;
		0u,								//	deUint32			offset;
		pushConstantSize,				//	deUint32			size;
	};

	const VkPipelineLayoutCreateInfo pipelineLayoutInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	//	VkStructureType					sType;
		nullptr,										//	const void*						pNext;
		0u,												//	VkPipelineLayoutCreateFlags		flags;
		static_cast<deUint32>(setLayouts.size()),		//	deUint32						setLayoutCount;
		setLayouts.data(),								//	const VkDescriptorSetLayout*	pSetLayouts;
		1u,												//	deUint32						pushConstantRangeCount;
		&pushConstantRange,								//	const VkPushConstantRange*		pPushConstantRanges;
	};

	const auto pipelineLayout = createPipelineLayout(vkd, device, &pipelineLayoutInfo);

	// Render pass.
	const VkAttachmentDescription commonAttachmentDescription =
	{
		0u,									//	VkAttachmentDescriptionFlags	flags;
		m_params.src.image.format,			//	VkFormat						format;
		m_params.samples,					//	VkSampleCountFlagBits			samples;
		VK_ATTACHMENT_LOAD_OP_LOAD,			//	VkAttachmentLoadOp				loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,		//	VkAttachmentStoreOp				storeOp;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,	//	VkAttachmentLoadOp				stencilLoadOp;
		VK_ATTACHMENT_STORE_OP_DONT_CARE,	//	VkAttachmentStoreOp				stencilStoreOp;
		currentLayout,						//	VkImageLayout					initialLayout;
		currentLayout,						//	VkImageLayout					finalLayout;
	};
	const std::vector<VkAttachmentDescription> attachmentDescriptions(numInputAttachments, commonAttachmentDescription);

	std::vector<VkAttachmentReference> inputAttachmentReferences;
	inputAttachmentReferences.reserve(numInputAttachments);
	for (deUint32 i = 0u; i < numInputAttachments; ++i)
	{
		const VkAttachmentReference reference = { i, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		inputAttachmentReferences.push_back(reference);
	}

	const VkSubpassDescription subpassDescription =
	{
		0u,															//	VkSubpassDescriptionFlags		flags;
		VK_PIPELINE_BIND_POINT_GRAPHICS,							//	VkPipelineBindPoint				pipelineBindPoint;
		static_cast<deUint32>(inputAttachmentReferences.size()),	//	deUint32						inputAttachmentCount;
		inputAttachmentReferences.data(),							//	const VkAttachmentReference*	pInputAttachments;
		0u,															//	deUint32						colorAttachmentCount;
		nullptr,													//	const VkAttachmentReference*	pColorAttachments;
		nullptr,													//	const VkAttachmentReference*	pResolveAttachments;
		nullptr,													//	const VkAttachmentReference*	pDepthStencilAttachment;
		0u,															//	deUint32						preserveAttachmentCount;
		nullptr,													//	const deUint32*					pPreserveAttachments;
	};

	const VkRenderPassCreateInfo renderPassInfo =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,				//	VkStructureType					sType;
		nullptr,												//	const void*						pNext;
		0u,														//	VkRenderPassCreateFlags			flags;
		static_cast<deUint32>(attachmentDescriptions.size()),	//	deUint32						attachmentCount;
		attachmentDescriptions.data(),							//	const VkAttachmentDescription*	pAttachments;
		1u,														//	deUint32						subpassCount;
		&subpassDescription,									//	const VkSubpassDescription*		pSubpasses;
		0u,														//	deUint32						dependencyCount;
		nullptr,												//	const VkSubpassDependency*		pDependencies;
	};

	const auto renderPass = createRenderPass(vkd, device, &renderPassInfo);

	// Framebuffer.
	std::vector<Move<VkImageView>>	imageViews;
	std::vector<VkImageView>		imageViewsRaw;

	imageViews.push_back(makeImageView(vkd, device, m_multisampledImage.get(), VK_IMAGE_VIEW_TYPE_2D, m_params.src.image.format, makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u)));
	for (deUint32 i = 0u; i < numDstLayers; ++i)
	{
		const auto subresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, i, 1u);
		imageViews.push_back(makeImageView(vkd, device, m_multisampledCopyImage.get(), VK_IMAGE_VIEW_TYPE_2D, m_params.dst.image.format, subresourceRange));
	}

	imageViewsRaw.reserve(imageViews.size());
	std::transform(begin(imageViews), end(imageViews), std::back_inserter(imageViewsRaw), [](const Move<VkImageView>& ptr) { return ptr.get(); });

	const auto framebuffer = makeFramebuffer(vkd, device, renderPass.get(), static_cast<deUint32>(imageViewsRaw.size()), imageViewsRaw.data(), fbWidth, fbHeight);

	// Storage buffer.
	const auto			bufferCount	= static_cast<size_t>(fbWidth * fbHeight * m_params.samples);
	const auto			bufferSize	= static_cast<VkDeviceSize>(bufferCount * sizeof(deInt32));
	BufferWithMemory	buffer		(vkd, device, alloc, makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), MemoryRequirement::HostVisible);
	auto&				bufferAlloc	= buffer.getAllocation();
	void*				bufferData	= bufferAlloc.getHostPtr();

	// Update descriptor sets.
	DescriptorSetUpdateBuilder updater;

	const auto bufferInfo = makeDescriptorBufferInfo(buffer.get(), 0ull, bufferSize);
	updater.writeSingle(descriptorSetBuffer.get(), DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferInfo);

	std::vector<VkDescriptorImageInfo> imageInfos;
	imageInfos.reserve(imageViewsRaw.size());
	for (size_t i = 0; i < imageViewsRaw.size(); ++i)
		imageInfos.push_back(makeDescriptorImageInfo(DE_NULL, imageViewsRaw[i], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));

	for (size_t i = 0; i < imageInfos.size(); ++i)
		updater.writeSingle(descriptorSetAttachments.get(), DescriptorSetUpdateBuilder::Location::binding(static_cast<deUint32>(i)), VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, &imageInfos[i]);

	updater.update(vkd, device);

	// Vertex buffer.
	std::vector<tcu::Vec4> fullScreenQuad;
	{
		// Full screen quad so every framebuffer pixel and sample location is verified by the shader.
		const tcu::Vec4 topLeft		(-1.0f, -1.0f, 0.0f, 1.0f);
		const tcu::Vec4 topRight	( 1.0f, -1.0f, 0.0f, 1.0f);
		const tcu::Vec4 bottomLeft	(-1.0f,  1.0f, 0.0f, 1.0f);
		const tcu::Vec4 bottomRight	( 1.0f,  1.0f, 0.0f, 1.0f);

		fullScreenQuad.reserve(6u);
		fullScreenQuad.push_back(topLeft);
		fullScreenQuad.push_back(topRight);
		fullScreenQuad.push_back(bottomRight);
		fullScreenQuad.push_back(topLeft);
		fullScreenQuad.push_back(bottomRight);
		fullScreenQuad.push_back(bottomLeft);
	}

	const auto				vertexBufferSize	= static_cast<VkDeviceSize>(fullScreenQuad.size() * sizeof(decltype(fullScreenQuad)::value_type));
	const auto				vertexBufferInfo	= makeBufferCreateInfo(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	const BufferWithMemory	vertexBuffer		(vkd, device, alloc, vertexBufferInfo, MemoryRequirement::HostVisible);
	const auto				vertexBufferHandler	= vertexBuffer.get();
	auto&					vertexBufferAlloc	= vertexBuffer.getAllocation();
	void*					vertexBufferData	= vertexBufferAlloc.getHostPtr();
	const VkDeviceSize		vertexBufferOffset	= 0ull;

	deMemcpy(vertexBufferData, fullScreenQuad.data(), static_cast<size_t>(vertexBufferSize));
	flushAlloc(vkd, device, vertexBufferAlloc);

	// Graphics pipeline.
	const std::vector<VkViewport>	viewports	(1, makeViewport(m_params.src.image.extent));
	const std::vector<VkRect2D>		scissors	(1, makeRect2D(m_params.src.image.extent));

	const VkPipelineMultisampleStateCreateInfo	multisampleStateParams		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType							sType;
		nullptr,													// const void*								pNext;
		0u,															// VkPipelineMultisampleStateCreateFlags	flags;
		VK_SAMPLE_COUNT_1_BIT,										// VkSampleCountFlagBits					rasterizationSamples;
		VK_FALSE,													// VkBool32									sampleShadingEnable;
		0.0f,														// float									minSampleShading;
		nullptr,													// const VkSampleMask*						pSampleMask;
		VK_FALSE,													// VkBool32									alphaToCoverageEnable;
		VK_FALSE													// VkBool32									alphaToOneEnable;
	};

	const auto graphicsPipeline = makeGraphicsPipeline(
		vkd,									// const DeviceInterface&                        vk
		device,									// const VkDevice                                device
		pipelineLayout.get(),					// const VkPipelineLayout                        pipelineLayout
		vertexModule.get(),						// const VkShaderModule                          vertexShaderModule
		DE_NULL,								// const VkShaderModule                          tessellationControlModule
		DE_NULL,								// const VkShaderModule                          tessellationEvalModule
		DE_NULL,								// const VkShaderModule                          geometryShaderModule
		verificationModule.get(),				// const VkShaderModule                          fragmentShaderModule
		renderPass.get(),						// const VkRenderPass                            renderPass
		viewports,								// const std::vector<VkViewport>&                viewports
		scissors,								// const std::vector<VkRect2D>&                  scissors
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,	// const VkPrimitiveTopology                     topology
		0u,										// const deUint32                                subpass
		0u,										// const deUint32                                patchControlPoints
		nullptr,								// const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
		nullptr,								// const VkPipelineRasterizationStateCreateInfo* rasterizationStateCreateInfo
		&multisampleStateParams);				// const VkPipelineMultisampleStateCreateInfo*   multisampleStateCreateInfo

	// Command buffer.
	const auto cmdPool		= makeCommandPool(vkd, device, queueIndex);
	const auto cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	// Make sure multisample copy data is available to the fragment shader.
	const auto imagesBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_INPUT_ATTACHMENT_READ_BIT);

	// Make sure verification buffer data is available on the host.
	const auto bufferBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);

	// Record and submit command buffer.
	beginCommandBuffer(vkd, cmdBuffer);
	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u, 1u, &imagesBarrier, 0u, nullptr, 0u, nullptr);
	beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), makeRect2D(m_params.src.image.extent));
	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.get());
	vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBufferHandler, &vertexBufferOffset);
	vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), VK_SHADER_STAGE_FRAGMENT_BIT, 0u, pushConstantSize, pushConstantData.data());
	vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0u, static_cast<deUint32>(descriptorSets.size()), descriptorSets.data(), 0u, nullptr);
	vkd.cmdDraw(cmdBuffer, static_cast<deUint32>(fullScreenQuad.size()), 1u, 0u, 0u);
	endRenderPass(vkd, cmdBuffer);
	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &bufferBarrier, 0u, nullptr, 0u, nullptr);
	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Verify intermediate results.
	invalidateAlloc(vkd, device, bufferAlloc);
	std::vector<deInt32> outputFlags (bufferCount, 0);
	deMemcpy(outputFlags.data(), bufferData, static_cast<size_t>(bufferSize));

	auto& log = m_context.getTestContext().getLog();
	log << tcu::TestLog::Message << "Verifying intermediate multisample copy results" << tcu::TestLog::EndMessage;

	const auto sampleCount = static_cast<deUint32>(m_params.samples);

	for (deUint32 x = 0u; x < fbWidth; ++x)
	for (deUint32 y = 0u; y < fbHeight; ++y)
	for (deUint32 s = 0u; s < sampleCount; ++s)
	{
		const auto index = (y * fbWidth + x) * sampleCount + s;
		if (!outputFlags[index])
		{
			std::ostringstream msg;
			msg << "Intermediate verification failed for coordinates (" << x << ", " << y << ") sample " << s;
			return tcu::TestStatus::fail(msg.str());
		}
	}

	log << tcu::TestLog::Message << "Intermediate multisample copy verification passed" << tcu::TestLog::EndMessage;
	return tcu::TestStatus::pass("Pass");
}

void ResolveImageToImage::copyMSImageToMSImage (deUint32 copyArraySize)
{
	const DeviceInterface&			vk					= m_context.getDeviceInterface();
	const VkDevice					vkDevice			= m_context.getDevice();
	const VkQueue					queue				= m_context.getUniversalQueue();
	const tcu::TextureFormat		srcTcuFormat		= mapVkFormat(m_params.src.image.format);
	std::vector<VkImageCopy>		imageCopies;
	std::vector<VkImageCopy2KHR>	imageCopies2KHR;

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

		if (m_params.extensionUse == EXTENSION_USE_NONE)
		{
			imageCopies.push_back(imageCopy);
		}
		else
		{
			DE_ASSERT(m_params.extensionUse == EXTENSION_USE_COPY_COMMANDS2);
			imageCopies2KHR.push_back(convertvkImageCopyTovkImageCopy2KHR(imageCopy));
		}
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

	if (m_params.extensionUse == EXTENSION_USE_NONE)
	{
		vk.cmdCopyImage(*m_cmdBuffer, m_multisampledImage.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_multisampledCopyImage.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (deUint32)imageCopies.size(), imageCopies.data());
	}
	else
	{
		DE_ASSERT(m_params.extensionUse == EXTENSION_USE_COPY_COMMANDS2);
		const VkCopyImageInfo2KHR copyImageInfo2KHR =
		{
			VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2_KHR,	// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			m_multisampledImage.get(),					// VkImage					srcImage;
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,		// VkImageLayout			srcImageLayout;
			m_multisampledCopyImage.get(),				// VkImage					dstImage;
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			dstImageLayout;
			(deUint32)imageCopies2KHR.size(),			// uint32_t					regionCount;
			imageCopies2KHR.data()						// const VkImageCopy2KHR*	pRegions;
		};

		vk.cmdCopyImage2KHR(*m_cmdBuffer, &copyImageInfo2KHR);
	}

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

		if ((m_params.extensionUse == EXTENSION_USE_COPY_COMMANDS2)	&&
			(!context.isDeviceFunctionalitySupported("VK_KHR_copy_commands2")))
		{
			TCU_THROW(NotSupportedError, "VK_KHR_copy_commands2 is not supported");
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

	if (m_options == COPY_MS_IMAGE_TO_MS_IMAGE || m_options == COPY_MS_IMAGE_TO_ARRAY_MS_IMAGE)
	{
		// The shader verifies all layers in the copied image are the same as the source image.
		// This needs an image view per layer in the copied image.
		// Set 0 contains the output buffer.
		// Set 1 contains the input attachments.

		std::ostringstream verificationShader;

		verificationShader
			<< "#version 450\n"
			<< "\n"
			<< "layout (push_constant, std430) uniform PushConstants {\n"
			<< "    int width;\n"
			<< "    int height;\n"
			<< "    int samples;\n"
			<< "};\n"
			<< "layout (set=0, binding=0) buffer VerificationResults {\n"
			<< "    int verificationFlags[];\n"
			<< "};\n"
			<< "layout (input_attachment_index=0, set=1, binding=0) uniform subpassInputMS attachment0;\n"
			;

		const auto dstLayers = getArraySize(m_params.dst.image);
		for (deUint32 layerNdx = 0u; layerNdx < dstLayers; ++layerNdx)
		{
			const auto i = layerNdx + 1u;
			verificationShader << "layout (input_attachment_index=" << i << ", set=1, binding=" << i << ") uniform subpassInputMS attachment" << i << ";\n";
		}

		// Using a loop to iterate over each sample avoids the need for the sampleRateShading feature. The pipeline needs to be
		// created with a single sample.
		verificationShader
			<< "\n"
			<< "void main() {\n"
			<< "    for (int sampleID = 0; sampleID < samples; ++sampleID) {\n"
			<< "        vec4 orig = subpassLoad(attachment0, sampleID);\n"
			;

		for (deUint32 layerNdx = 0u; layerNdx < dstLayers; ++layerNdx)
		{
			const auto i = layerNdx + 1u;
			verificationShader << "        vec4 copy" << i << " = subpassLoad(attachment" << i << ", sampleID);\n";
		}

		std::ostringstream testCondition;
		for (deUint32 layerNdx = 0u; layerNdx < dstLayers; ++layerNdx)
		{
			const auto i = layerNdx + 1u;
			testCondition << (layerNdx == 0u ? "" : " && ") << "orig == copy" << i;
		}

		verificationShader
			<< "\n"
			<< "        ivec3 coords  = ivec3(int(gl_FragCoord.x), int(gl_FragCoord.y), sampleID);\n"
			<< "        int bufferPos = (coords.y * width + coords.x) * samples + coords.z;\n"
			<< "\n"
			<< "        verificationFlags[bufferPos] = ((" << testCondition.str() << ") ? 1 : 0); \n"
			<< "    }\n"
			<< "}\n"
			;

		programCollection.glslSources.add("verify") << glu::FragmentSource(verificationShader.str());
	}
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

void addImageToImageSimpleTests (tcu::TestCaseGroup* group, AllocationKind allocationKind, ExtensionUse extensionUse)
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
		params.extensionUse					= extensionUse;

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
		params.extensionUse					= extensionUse;

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
			imageCopy.imageCopy = testCopy;
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
		params.extensionUse					= extensionUse;

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
			imageCopy.imageCopy = testCopy;
			params.regions.push_back(imageCopy);
		}

		group->addChild(new CopyImageToImageTestCase(testCtx, "partial_image", "Partial image", params));
	}

	{
		VkExtent3D	extent					= { 65u, 63u, 1u };

		TestParams	params;
		params.src.image.imageType			= VK_IMAGE_TYPE_2D;
		params.src.image.format				= VK_FORMAT_R32_UINT;
		params.src.image.extent				= extent;
		params.src.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
		params.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		params.dst.image.imageType			= VK_IMAGE_TYPE_2D;
		params.dst.image.format				= VK_FORMAT_R8G8B8A8_UNORM;
		params.dst.image.extent				= extent;
		params.dst.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
		params.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		params.allocationKind				= allocationKind;
		params.extensionUse					= extensionUse;
		params.clearDestination				= VK_TRUE;

		{
			const VkImageCopy	testCopy	=
			{
				defaultSourceLayer,	// VkImageSubresourceLayers	srcSubresource;
				{34, 34, 0},		// VkOffset3D				srcOffset;
				defaultSourceLayer,	// VkImageSubresourceLayers	dstSubresource;
				{0, 0, 0},			// VkOffset3D				dstOffset;
				{31, 29, 1}			// VkExtent3D				extent;
			};

			CopyRegion			imageCopy;

			imageCopy.imageCopy = testCopy;
			params.regions.push_back(imageCopy);
		}

		group->addChild(new CopyImageToImageTestCase(testCtx, "partial_image_npot_diff_format_clear", "Partial image with npot dimensions, different format, and clearing of the destination image", params));
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
		params.extensionUse					= extensionUse;

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
			imageCopy.imageCopy = testCopy;
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
		params.extensionUse					= extensionUse;

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
			imageCopy.imageCopy = testCopy;
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

bool isAllowedImageToImageAllFormatsColorSrcFormatTests(const CopyColorTestParams& testParams)
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
	// If testParams.compatibleFormats is nullptr, the destination format will be copied from the source format.
	const VkFormat	srcFormatOnly[2]	= { testParams.params.src.image.format, VK_FORMAT_UNDEFINED };
	const VkFormat*	formatList			= (testParams.compatibleFormats ? testParams.compatibleFormats : srcFormatOnly);

	for (int dstFormatIndex = 0; formatList[dstFormatIndex] != VK_FORMAT_UNDEFINED; ++dstFormatIndex)
	{
		testParams.params.dst.image.format = formatList[dstFormatIndex];

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
	VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT,
	VK_FORMAT_A4B4G4R4_UNORM_PACK16_EXT,

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

void addImageToImageAllFormatsColorTests (tcu::TestCaseGroup* group, AllocationKind allocationKind, ExtensionUse extensionUse)
{
	if (allocationKind == ALLOCATION_KIND_DEDICATED)
	{
		const int numOfColorImageFormatsToTestFilter = DE_LENGTH_OF_ARRAY(colorImageFormatsToTest);
		for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < numOfColorImageFormatsToTestFilter; ++compatibleFormatsIndex)
			dedicatedAllocationImageToImageFormatsToTestSet.insert(dedicatedAllocationImageToImageFormatsToTest[compatibleFormatsIndex]);
	}

	// 2D tests.
	{
		de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "2d", "2D copies"));

		TestParams	params;
		params.src.image.imageType	= VK_IMAGE_TYPE_2D;
		params.dst.image.imageType	= VK_IMAGE_TYPE_2D;
		params.src.image.extent		= defaultExtent;
		params.dst.image.extent		= defaultExtent;
		params.src.image.tiling		= VK_IMAGE_TILING_OPTIMAL;
		params.dst.image.tiling		= VK_IMAGE_TILING_OPTIMAL;
		params.allocationKind		= allocationKind;
		params.extensionUse			= extensionUse;

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

				const std::string testName		= getFormatCaseName(params.src.image.format);
				const std::string description	= "Copy from source format " + getFormatCaseName(params.src.image.format);
				addTestGroup(subGroup.get(), testName, description, addImageToImageAllFormatsColorSrcFormatTests, testParams);
			}
		}

		group->addChild(subGroup.release());
	}

	// 1D tests.
	{
		de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "1d", "1D copies"));

		TestParams	params;
		params.src.image.imageType	= VK_IMAGE_TYPE_1D;
		params.dst.image.imageType	= VK_IMAGE_TYPE_1D;
		params.src.image.extent		= default1dExtent;
		params.dst.image.extent		= default1dExtent;
		params.src.image.tiling		= VK_IMAGE_TILING_OPTIMAL;
		params.dst.image.tiling		= VK_IMAGE_TILING_OPTIMAL;
		params.allocationKind		= allocationKind;
		params.extensionUse			= extensionUse;

		for (deInt32 i = defaultFourthSize; i < defaultSize; i += defaultSize / 2)
		{
			const VkImageCopy				testCopy =
			{
				defaultSourceLayer,			// VkImageSubresourceLayers	srcSubresource;
				{0, 0, 0},					// VkOffset3D				srcOffset;
				defaultSourceLayer,			// VkImageSubresourceLayers	dstSubresource;
				{i, 0, 0},					// VkOffset3D				dstOffset;
				{defaultFourthSize, 1, 1},	// VkExtent3D				extent;
			};

			CopyRegion	imageCopy;
			imageCopy.imageCopy = testCopy;

			params.regions.push_back(imageCopy);
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
				testParams.compatibleFormats	= nullptr;

				const std::string testName		= getFormatCaseName(params.src.image.format);
				const std::string description	= "Copy from source format " + getFormatCaseName(params.src.image.format);
				addTestGroup(subGroup.get(), testName, description, addImageToImageAllFormatsColorSrcFormatTests, testParams);
			}
		}

		group->addChild(subGroup.release());
	}

	// 3D tests. Note we use smaller dimensions here for performance reasons.
	{
		de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "3d", "3D copies"));

		TestParams	params;
		params.src.image.imageType	= VK_IMAGE_TYPE_3D;
		params.dst.image.imageType	= VK_IMAGE_TYPE_3D;
		params.src.image.extent		= default3dExtent;
		params.dst.image.extent		= default3dExtent;
		params.src.image.tiling		= VK_IMAGE_TILING_OPTIMAL;
		params.dst.image.tiling		= VK_IMAGE_TILING_OPTIMAL;
		params.allocationKind		= allocationKind;
		params.extensionUse			= extensionUse;

		for (deInt32 i = 0; i < defaultFourthSize; i += defaultSixteenthSize)
		{
			const VkImageCopy				testCopy =
			{
				defaultSourceLayer,													// VkImageSubresourceLayers	srcSubresource;
				{0, 0, 0},															// VkOffset3D				srcOffset;
				defaultSourceLayer,													// VkImageSubresourceLayers	dstSubresource;
				{i, defaultFourthSize - i - defaultSixteenthSize, i},				// VkOffset3D				dstOffset;
				{defaultSixteenthSize, defaultSixteenthSize, defaultSixteenthSize},	// VkExtent3D				extent;
			};

			CopyRegion	imageCopy;
			imageCopy.imageCopy = testCopy;

			params.regions.push_back(imageCopy);
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
				testParams.compatibleFormats	= nullptr;

				const std::string testName		= getFormatCaseName(params.src.image.format);
				const std::string description	= "Copy from source format " + getFormatCaseName(params.src.image.format);
				addTestGroup(subGroup.get(), testName, description, addImageToImageAllFormatsColorSrcFormatTests, testParams);
			}
		}

		group->addChild(subGroup.release());
	}
}

void addImageToImageDimensionsTests (tcu::TestCaseGroup* group, AllocationKind allocationKind, ExtensionUse extensionUse)
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
		testParams.params.extensionUse			= extensionUse;

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

void addImageToImageAllFormatsDepthStencilTests (tcu::TestCaseGroup* group, AllocationKind allocationKind, ExtensionUse extensionUse)
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
	{
		de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "2d", "2D copies"));

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
			params.extensionUse					= extensionUse;
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

			const std::string testName		= getFormatCaseName(params.src.image.format) + "_" + getFormatCaseName(params.dst.image.format);
			const std::string description	= "Copy from " + getFormatCaseName(params.src.image.format) + " to " + getFormatCaseName(params.dst.image.format);
			addTestGroup(subGroup.get(), testName, description, addImageToImageAllFormatsDepthStencilFormatsTests, params);

			if (hasDepth && hasStencil)
			{
				params.separateDepthStencilLayouts	= DE_TRUE;
				const std::string testName2		= getFormatCaseName(params.src.image.format) + "_" + getFormatCaseName(params.dst.image.format) + "_separate_layouts";
				const std::string description2	= "Copy from " + getFormatCaseName(params.src.image.format) + " to " + getFormatCaseName(params.dst.image.format) + " with separate depth/stencil layouts";
				addTestGroup(subGroup.get(), testName2, description2, addImageToImageAllFormatsDepthStencilFormatsTests, params);
			}
		}

		group->addChild(subGroup.release());
	}

	// 1D tests.
	{
		de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "1d", "1D copies"));

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
			params.extensionUse					= extensionUse;

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

			const std::string testName		= getFormatCaseName(params.src.image.format) + "_" + getFormatCaseName(params.dst.image.format);
			const std::string description	= "Copy from " + getFormatCaseName(params.src.image.format) + " to " + getFormatCaseName(params.dst.image.format);
			addTestGroup(subGroup.get(), testName, description, addImageToImageAllFormatsDepthStencilFormatsTests, params);

			if (hasDepth && hasStencil)
			{
				params.separateDepthStencilLayouts	= DE_TRUE;
				const std::string testName2		= getFormatCaseName(params.src.image.format) + "_" + getFormatCaseName(params.dst.image.format) + "_separate_layouts";
				const std::string description2	= "Copy from " + getFormatCaseName(params.src.image.format) + " to " + getFormatCaseName(params.dst.image.format) + " with separate depth/stencil layouts";
				addTestGroup(subGroup.get(), testName2, description2, addImageToImageAllFormatsDepthStencilFormatsTests, params);
			}
		}

		group->addChild(subGroup.release());
	}

	// 3D tests. Note we use smaller dimensions here for performance reasons.
	{
		de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "3d", "3D copies"));

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
			params.extensionUse					= extensionUse;

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

			const std::string testName		= getFormatCaseName(params.src.image.format) + "_" + getFormatCaseName(params.dst.image.format);
			const std::string description	= "Copy from " + getFormatCaseName(params.src.image.format) + " to " + getFormatCaseName(params.dst.image.format);
			addTestGroup(subGroup.get(), testName, description, addImageToImageAllFormatsDepthStencilFormatsTests, params);

			if (hasDepth && hasStencil)
			{
				params.separateDepthStencilLayouts	= DE_TRUE;
				const std::string testName2		= getFormatCaseName(params.src.image.format) + "_" + getFormatCaseName(params.dst.image.format) + "_separate_layouts";
				const std::string description2	= "Copy from " + getFormatCaseName(params.src.image.format) + " to " + getFormatCaseName(params.dst.image.format) + " with separate depth/stencil layouts";
				addTestGroup(subGroup.get(), testName2, description2, addImageToImageAllFormatsDepthStencilFormatsTests, params);
			}
		}

		group->addChild(subGroup.release());
	}
}

void addImageToImageAllFormatsTests (tcu::TestCaseGroup* group, AllocationKind allocationKind, ExtensionUse extensionUse)
{
	addTestGroup(group, "color", "Copy image to image with color formats", addImageToImageAllFormatsColorTests, allocationKind, extensionUse);
	addTestGroup(group, "depth_stencil", "Copy image to image with depth/stencil formats", addImageToImageAllFormatsDepthStencilTests, allocationKind, extensionUse);
}

void addImageToImage3dImagesTests (tcu::TestCaseGroup* group, AllocationKind allocationKind, ExtensionUse extensionUse)
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
		params3DTo2D.extensionUse				= extensionUse;

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
				{0, 0, (deInt32)slicesLayersNdx},	// VkOffset3D				srcOffset;
				destinationLayer,					// VkImageSubresourceLayers	dstSubresource;
				{0, 0, 0},							// VkOffset3D				dstOffset;
				defaultHalfExtent,					// VkExtent3D				extent;
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
		params2DTo3D.extensionUse				= extensionUse;

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
		params3DTo2D.extensionUse				= extensionUse;

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
		params2DTo3D.extensionUse				= extensionUse;

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
		params3DTo2D.extensionUse				= extensionUse;

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
		params2DTo3D.extensionUse				= extensionUse;

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

void addImageToImageCubeTests (tcu::TestCaseGroup* group, AllocationKind allocationKind, ExtensionUse extensionUse)
{
	tcu::TestContext& testCtx	= group->getTestContext();

	{
		TestParams	paramsCubeToArray;
		const deUint32	arrayLayers					= 6u;
		paramsCubeToArray.src.image.createFlags		= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		paramsCubeToArray.src.image.imageType		= VK_IMAGE_TYPE_2D;
		paramsCubeToArray.src.image.format			= VK_FORMAT_R8G8B8A8_UINT;
		paramsCubeToArray.src.image.extent			= defaultHalfExtent;
		paramsCubeToArray.src.image.extent.depth	= arrayLayers;
		paramsCubeToArray.src.image.tiling			= VK_IMAGE_TILING_OPTIMAL;
		paramsCubeToArray.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		paramsCubeToArray.dst.image.createFlags		= 0;
		paramsCubeToArray.dst.image.imageType		= VK_IMAGE_TYPE_2D;
		paramsCubeToArray.dst.image.format			= VK_FORMAT_R8G8B8A8_UINT;
		paramsCubeToArray.dst.image.extent			= defaultHalfExtent;
		paramsCubeToArray.dst.image.extent.depth	= arrayLayers;
		paramsCubeToArray.dst.image.tiling			= VK_IMAGE_TILING_OPTIMAL;
		paramsCubeToArray.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		paramsCubeToArray.allocationKind			= allocationKind;
		paramsCubeToArray.extensionUse				= extensionUse;

		for (deUint32 arrayLayersNdx = 0; arrayLayersNdx < arrayLayers; ++arrayLayersNdx)
		{
			const VkImageSubresourceLayers	sourceLayer	=
				{
					VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
					0u,							// deUint32				mipLevel;
					arrayLayersNdx,				// deUint32				baseArrayLayer;
					1u							// deUint32				layerCount;
				};

			const VkImageSubresourceLayers	destinationLayer	=
				{
					VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
					0u,							// deUint32				mipLevel;
					arrayLayersNdx,				// deUint32				baseArrayLayer;
					1u							// deUint32				layerCount;
				};

			const VkImageCopy				testCopy	=
				{
					sourceLayer,				// VkImageSubresourceLayers	srcSubresource;
					{0, 0, 0},					// VkOffset3D				srcOffset;
					destinationLayer,			// VkImageSubresourceLayers	dstSubresource;
					{0, 0, 0},					// VkOffset3D				dstOffset;
					defaultHalfExtent				// VkExtent3D				extent;
				};

			CopyRegion	imageCopy;
			imageCopy.imageCopy	= testCopy;

			paramsCubeToArray.regions.push_back(imageCopy);
		}

		group->addChild(new CopyImageToImageTestCase(testCtx, "cube_to_array_layers", "copy cube compatible image to 2d layers layer by layer", paramsCubeToArray));
	}

	{
		TestParams	paramsCubeToArray;
		const deUint32	arrayLayers					= 6u;
		paramsCubeToArray.src.image.createFlags		= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		paramsCubeToArray.src.image.imageType		= VK_IMAGE_TYPE_2D;
		paramsCubeToArray.src.image.format			= VK_FORMAT_R8G8B8A8_UINT;
		paramsCubeToArray.src.image.extent			= defaultHalfExtent;
		paramsCubeToArray.src.image.extent.depth	= arrayLayers;
		paramsCubeToArray.src.image.tiling			= VK_IMAGE_TILING_OPTIMAL;
		paramsCubeToArray.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		paramsCubeToArray.dst.image.createFlags		= 0;
		paramsCubeToArray.dst.image.imageType		= VK_IMAGE_TYPE_2D;
		paramsCubeToArray.dst.image.format			= VK_FORMAT_R8G8B8A8_UINT;
		paramsCubeToArray.dst.image.extent			= defaultHalfExtent;
		paramsCubeToArray.dst.image.extent.depth	= arrayLayers;
		paramsCubeToArray.dst.image.tiling			= VK_IMAGE_TILING_OPTIMAL;
		paramsCubeToArray.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		paramsCubeToArray.allocationKind			= allocationKind;
		paramsCubeToArray.extensionUse				= extensionUse;

		{
			const VkImageSubresourceLayers	sourceLayer	=
				{
					VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
					0u,							// deUint32				mipLevel;
					0u,							// deUint32				baseArrayLayer;
					arrayLayers					// deUint32				layerCount;
				};

			const VkImageSubresourceLayers	destinationLayer	=
				{
					VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
					0u,							// deUint32				mipLevel;
					0u,							// deUint32				baseArrayLayer;
					arrayLayers					// deUint32				layerCount;
				};

			const VkImageCopy				testCopy	=
				{
					sourceLayer,				// VkImageSubresourceLayers	srcSubresource;
					{0, 0, 0},					// VkOffset3D				srcOffset;
					destinationLayer,			// VkImageSubresourceLayers	dstSubresource;
					{0, 0, 0},					// VkOffset3D				dstOffset;
					defaultHalfExtent			// VkExtent3D				extent;
				};

			CopyRegion	imageCopy;
			imageCopy.imageCopy	= testCopy;

			paramsCubeToArray.regions.push_back(imageCopy);
		}

		group->addChild(new CopyImageToImageTestCase(testCtx, "cube_to_array_whole", "copy cube compatible image to 2d layers all at once", paramsCubeToArray));
	}

	{
		TestParams	paramsArrayToCube;
		const deUint32	arrayLayers					= 6u;
		paramsArrayToCube.src.image.createFlags		= 0;
		paramsArrayToCube.src.image.imageType		= VK_IMAGE_TYPE_2D;
		paramsArrayToCube.src.image.format			= VK_FORMAT_R8G8B8A8_UINT;
		paramsArrayToCube.src.image.extent			= defaultHalfExtent;
		paramsArrayToCube.src.image.extent.depth	= arrayLayers;
		paramsArrayToCube.src.image.tiling			= VK_IMAGE_TILING_OPTIMAL;
		paramsArrayToCube.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		paramsArrayToCube.dst.image.createFlags		= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		paramsArrayToCube.dst.image.imageType		= VK_IMAGE_TYPE_2D;
		paramsArrayToCube.dst.image.format			= VK_FORMAT_R8G8B8A8_UINT;
		paramsArrayToCube.dst.image.extent			= defaultHalfExtent;
		paramsArrayToCube.dst.image.extent.depth	= arrayLayers;
		paramsArrayToCube.dst.image.tiling			= VK_IMAGE_TILING_OPTIMAL;
		paramsArrayToCube.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		paramsArrayToCube.allocationKind			= allocationKind;
		paramsArrayToCube.extensionUse				= extensionUse;

		for (deUint32 arrayLayersNdx = 0; arrayLayersNdx < arrayLayers; ++arrayLayersNdx)
		{
			const VkImageSubresourceLayers	sourceLayer	=
				{
					VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
					0u,							// deUint32				mipLevel;
					arrayLayersNdx,				// deUint32				baseArrayLayer;
					1u							// deUint32				layerCount;
				};

			const VkImageSubresourceLayers	destinationLayer =
				{
					VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
					0u,							// deUint32				mipLevel;
					arrayLayersNdx,				// deUint32				baseArrayLayer;
					1u							// deUint32				layerCount;
				};

			const VkImageCopy				testCopy =
				{
					sourceLayer,				// VkImageSubresourceLayers	srcSubresource;
					{0, 0, 0},					// VkOffset3D				srcOffset;
					destinationLayer,			// VkImageSubresourceLayers	dstSubresource;
					{0, 0, 0},					// VkOffset3D				dstOffset;
					defaultHalfExtent			// VkExtent3D				extent;
				};

			CopyRegion	imageCopy;
			imageCopy.imageCopy	= testCopy;

			paramsArrayToCube.regions.push_back(imageCopy);
		}

		group->addChild(new CopyImageToImageTestCase(testCtx, "array_to_cube_layers", "copy 2d layers to cube compatible image layer by layer", paramsArrayToCube));
	}

	{
		TestParams	paramsArrayToCube;
		const deUint32	arrayLayers					= 6u;
		paramsArrayToCube.src.image.createFlags		= 0;
		paramsArrayToCube.src.image.imageType		= VK_IMAGE_TYPE_2D;
		paramsArrayToCube.src.image.format			= VK_FORMAT_R8G8B8A8_UINT;
		paramsArrayToCube.src.image.extent			= defaultHalfExtent;
		paramsArrayToCube.src.image.extent.depth	= arrayLayers;
		paramsArrayToCube.src.image.tiling			= VK_IMAGE_TILING_OPTIMAL;
		paramsArrayToCube.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		paramsArrayToCube.dst.image.createFlags		= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		paramsArrayToCube.dst.image.imageType		= VK_IMAGE_TYPE_2D;
		paramsArrayToCube.dst.image.format			= VK_FORMAT_R8G8B8A8_UINT;
		paramsArrayToCube.dst.image.extent			= defaultHalfExtent;
		paramsArrayToCube.dst.image.extent.depth	= arrayLayers;
		paramsArrayToCube.dst.image.tiling			= VK_IMAGE_TILING_OPTIMAL;
		paramsArrayToCube.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		paramsArrayToCube.allocationKind			= allocationKind;
		paramsArrayToCube.extensionUse				= extensionUse;

		{
			const VkImageSubresourceLayers sourceLayer =
				{
					VK_IMAGE_ASPECT_COLOR_BIT,		// VkImageAspectFlags	aspectMask;
					0u,								// deUint32				mipLevel;
					0u,								// deUint32				baseArrayLayer;
					arrayLayers						// deUint32				layerCount;
				};

			const VkImageSubresourceLayers destinationLayer =
				{
					VK_IMAGE_ASPECT_COLOR_BIT,		// VkImageAspectFlags	aspectMask;
					0u,								// deUint32				mipLevel;
					0u,								// deUint32				baseArrayLayer;
					arrayLayers						// deUint32				layerCount;
				};

			const VkImageCopy				testCopy =
				{
					sourceLayer,					// VkImageSubresourceLayers	srcSubresource;
					{0, 0, 0},						// VkOffset3D				srcOffset;
					destinationLayer,				// VkImageSubresourceLayers	dstSubresource;
					{0, 0, 0},						// VkOffset3D				dstOffset;
					defaultHalfExtent				// VkExtent3D				extent;
				};

			CopyRegion imageCopy;
			imageCopy.imageCopy = testCopy;

			paramsArrayToCube.regions.push_back(imageCopy);
		}

		group->addChild(new CopyImageToImageTestCase(testCtx, "array_to_cube_whole", "copy 2d layers to cube compatible image all at once", paramsArrayToCube));
	}

	{
		TestParams	paramsCubeToArray;
		const deUint32	arrayLayers					= 6u;
		paramsCubeToArray.src.image.createFlags		= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		paramsCubeToArray.src.image.imageType		= VK_IMAGE_TYPE_2D;
		paramsCubeToArray.src.image.format			= VK_FORMAT_R8G8B8A8_UINT;
		paramsCubeToArray.src.image.extent			= defaultHalfExtent;
		paramsCubeToArray.src.image.extent.depth	= arrayLayers;
		paramsCubeToArray.src.image.tiling			= VK_IMAGE_TILING_OPTIMAL;
		paramsCubeToArray.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		paramsCubeToArray.dst.image.createFlags		= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		paramsCubeToArray.dst.image.imageType		= VK_IMAGE_TYPE_2D;
		paramsCubeToArray.dst.image.format			= VK_FORMAT_R8G8B8A8_UINT;
		paramsCubeToArray.dst.image.extent			= defaultHalfExtent;
		paramsCubeToArray.dst.image.extent.depth	= arrayLayers;
		paramsCubeToArray.dst.image.tiling			= VK_IMAGE_TILING_OPTIMAL;
		paramsCubeToArray.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		paramsCubeToArray.allocationKind			= allocationKind;
		paramsCubeToArray.extensionUse				= extensionUse;

		for (deUint32 arrayLayersNdx = 0; arrayLayersNdx < arrayLayers; ++arrayLayersNdx)
		{
			const VkImageSubresourceLayers	sourceLayer	=
				{
					VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
					0u,							// deUint32				mipLevel;
					arrayLayersNdx,				// deUint32				baseArrayLayer;
					1u							// deUint32				layerCount;
				};

			const VkImageSubresourceLayers	destinationLayer	=
				{
					VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
					0u,							// deUint32				mipLevel;
					arrayLayersNdx,				// deUint32				baseArrayLayer;
					1u							// deUint32				layerCount;
				};

			const VkImageCopy				testCopy	=
				{
					sourceLayer,				// VkImageSubresourceLayers	srcSubresource;
					{0, 0, 0},					// VkOffset3D				srcOffset;
					destinationLayer,			// VkImageSubresourceLayers	dstSubresource;
					{0, 0, 0},					// VkOffset3D				dstOffset;
					defaultHalfExtent				// VkExtent3D				extent;
				};

			CopyRegion	imageCopy;
			imageCopy.imageCopy	= testCopy;

			paramsCubeToArray.regions.push_back(imageCopy);
		}

		group->addChild(new CopyImageToImageTestCase(testCtx, "cube_to_cube_layers", "copy cube compatible image to cube compatible image layer by layer", paramsCubeToArray));
	}

	{
		TestParams	paramsCubeToCube;
		const deUint32	arrayLayers					= 6u;
		paramsCubeToCube.src.image.createFlags		= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		paramsCubeToCube.src.image.imageType		= VK_IMAGE_TYPE_2D;
		paramsCubeToCube.src.image.format			= VK_FORMAT_R8G8B8A8_UINT;
		paramsCubeToCube.src.image.extent			= defaultHalfExtent;
		paramsCubeToCube.src.image.extent.depth		= arrayLayers;
		paramsCubeToCube.src.image.tiling			= VK_IMAGE_TILING_OPTIMAL;
		paramsCubeToCube.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		paramsCubeToCube.dst.image.createFlags		= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		paramsCubeToCube.dst.image.imageType		= VK_IMAGE_TYPE_2D;
		paramsCubeToCube.dst.image.format			= VK_FORMAT_R8G8B8A8_UINT;
		paramsCubeToCube.dst.image.extent			= defaultHalfExtent;
		paramsCubeToCube.dst.image.extent.depth		= arrayLayers;
		paramsCubeToCube.dst.image.tiling			= VK_IMAGE_TILING_OPTIMAL;
		paramsCubeToCube.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		paramsCubeToCube.allocationKind				= allocationKind;
		paramsCubeToCube.extensionUse				= extensionUse;

		{
			const VkImageSubresourceLayers	sourceLayer	=
				{
					VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
					0u,							// deUint32				mipLevel;
					0u,							// deUint32				baseArrayLayer;
					arrayLayers					// deUint32				layerCount;
				};

			const VkImageSubresourceLayers	destinationLayer	=
				{
					VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
					0u,							// deUint32				mipLevel;
					0u,							// deUint32				baseArrayLayer;
					arrayLayers					// deUint32				layerCount;
				};

			const VkImageCopy				testCopy	=
				{
					sourceLayer,				// VkImageSubresourceLayers	srcSubresource;
					{0, 0, 0},					// VkOffset3D				srcOffset;
					destinationLayer,			// VkImageSubresourceLayers	dstSubresource;
					{0, 0, 0},					// VkOffset3D				dstOffset;
					defaultHalfExtent			// VkExtent3D				extent;
				};

			CopyRegion	imageCopy;
			imageCopy.imageCopy	= testCopy;

			paramsCubeToCube.regions.push_back(imageCopy);
		}

		group->addChild(new CopyImageToImageTestCase(testCtx, "cube_to_cube_whole", "copy cube compatible image to cube compatible image all at once", paramsCubeToCube));
	}
}

void addImageToImageArrayTests (tcu::TestCaseGroup* group, AllocationKind allocationKind, ExtensionUse extensionUse)
{
	tcu::TestContext& testCtx	= group->getTestContext();

	{
		TestParams	paramsArrayToArray;
		const deUint32	arrayLayers					= 16u;
		paramsArrayToArray.src.image.imageType		= VK_IMAGE_TYPE_2D;
		paramsArrayToArray.src.image.format			= VK_FORMAT_R8G8B8A8_UINT;
		paramsArrayToArray.src.image.extent			= defaultHalfExtent;
		paramsArrayToArray.src.image.extent.depth	= arrayLayers;
		paramsArrayToArray.src.image.tiling			= VK_IMAGE_TILING_OPTIMAL;
		paramsArrayToArray.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		paramsArrayToArray.dst.image.imageType		= VK_IMAGE_TYPE_2D;
		paramsArrayToArray.dst.image.format			= VK_FORMAT_R8G8B8A8_UINT;
		paramsArrayToArray.dst.image.extent			= defaultHalfExtent;
		paramsArrayToArray.dst.image.extent.depth	= arrayLayers;
		paramsArrayToArray.dst.image.tiling			= VK_IMAGE_TILING_OPTIMAL;
		paramsArrayToArray.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		paramsArrayToArray.allocationKind			= allocationKind;
		paramsArrayToArray.extensionUse				= extensionUse;

		for (deUint32 arrayLayersNdx = 0; arrayLayersNdx < arrayLayers; ++arrayLayersNdx)
		{
			const VkImageSubresourceLayers	sourceLayer	=
					{
							VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
							0u,							// deUint32				mipLevel;
							arrayLayersNdx,				// deUint32				baseArrayLayer;
							1u							// deUint32				layerCount;
					};

			const VkImageSubresourceLayers	destinationLayer =
					{
							VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
							0u,							// deUint32				mipLevel;
							arrayLayersNdx,				// deUint32				baseArrayLayer;
							1u							// deUint32				layerCount;
					};

			const VkImageCopy				testCopy =
					{
							sourceLayer,				// VkImageSubresourceLayers	srcSubresource;
							{0, 0, 0},					// VkOffset3D				srcOffset;
							destinationLayer,			// VkImageSubresourceLayers	dstSubresource;
							{0, 0, 0},					// VkOffset3D				dstOffset;
							defaultHalfExtent			// VkExtent3D				extent;
					};

			CopyRegion	imageCopy;
			imageCopy.imageCopy	= testCopy;

			paramsArrayToArray.regions.push_back(imageCopy);
		}

		group->addChild(new CopyImageToImageTestCase(testCtx, "array_to_array_layers", "copy 2d array image to 2d array image layer by layer", paramsArrayToArray));
	}

	{
		TestParams	paramsArrayToArray;
		const deUint32	arrayLayers						= 16u;
		paramsArrayToArray.src.image.imageType			= VK_IMAGE_TYPE_2D;
		paramsArrayToArray.src.image.format				= VK_FORMAT_R8G8B8A8_UINT;
		paramsArrayToArray.src.image.extent				= defaultHalfExtent;
		paramsArrayToArray.src.image.extent.depth		= arrayLayers;
		paramsArrayToArray.src.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
		paramsArrayToArray.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		paramsArrayToArray.dst.image.imageType			= VK_IMAGE_TYPE_2D;
		paramsArrayToArray.dst.image.format				= VK_FORMAT_R8G8B8A8_UINT;
		paramsArrayToArray.dst.image.extent				= defaultHalfExtent;
		paramsArrayToArray.dst.image.extent.depth		= arrayLayers;
		paramsArrayToArray.dst.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
		paramsArrayToArray.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		paramsArrayToArray.allocationKind				= allocationKind;
		paramsArrayToArray.extensionUse					= extensionUse;

		{
			const VkImageSubresourceLayers sourceLayer =
					{
							VK_IMAGE_ASPECT_COLOR_BIT,		// VkImageAspectFlags	aspectMask;
							0u,								// deUint32				mipLevel;
							0u,								// deUint32				baseArrayLayer;
							arrayLayers						// deUint32				layerCount;
					};

			const VkImageSubresourceLayers destinationLayer =
					{
							VK_IMAGE_ASPECT_COLOR_BIT,		// VkImageAspectFlags	aspectMask;
							0u,								// deUint32				mipLevel;
							0u,								// deUint32				baseArrayLayer;
							arrayLayers						// deUint32				layerCount;
					};

			const VkImageCopy				testCopy =
					{
							sourceLayer,					// VkImageSubresourceLayers	srcSubresource;
							{0, 0, 0},						// VkOffset3D				srcOffset;
							destinationLayer,				// VkImageSubresourceLayers	dstSubresource;
							{0, 0, 0},						// VkOffset3D				dstOffset;
							defaultHalfExtent				// VkExtent3D				extent;
					};

			CopyRegion imageCopy;
			imageCopy.imageCopy = testCopy;

			paramsArrayToArray.regions.push_back(imageCopy);
		}

		group->addChild(new CopyImageToImageTestCase(testCtx, "array_to_array_whole", "copy 2d array image to 2d array image all at once", paramsArrayToArray));
	}
};

void addImageToImageTests (tcu::TestCaseGroup* group, AllocationKind allocationKind, ExtensionUse extensionUse)
{
	addTestGroup(group, "simple_tests", "Copy from image to image simple tests", addImageToImageSimpleTests, allocationKind, extensionUse);
	addTestGroup(group, "all_formats", "Copy from image to image with all compatible formats", addImageToImageAllFormatsTests, allocationKind, extensionUse);
	addTestGroup(group, "3d_images", "Coping operations on 3d images", addImageToImage3dImagesTests, allocationKind, extensionUse);
	addTestGroup(group, "dimensions", "Copying operations on different image dimensions", addImageToImageDimensionsTests, allocationKind, extensionUse);
	addTestGroup(group, "cube", "Coping operations on cube compatible images", addImageToImageCubeTests, allocationKind, extensionUse);
	addTestGroup(group, "array", "Copying operations on array of images", addImageToImageArrayTests, allocationKind, extensionUse);
}

void addImageToBufferTests (tcu::TestCaseGroup* group, AllocationKind allocationKind, ExtensionUse extensionUse)
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
		params.extensionUse					= extensionUse;

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
		params.extensionUse					= extensionUse;

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
		params.src.image.format				= VK_FORMAT_R8_UNORM;
		params.src.image.extent				= defaultExtent;
		params.src.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
		params.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		params.dst.buffer.size				= defaultSize * defaultSize;
		params.allocationKind				= allocationKind;
		params.extensionUse					= extensionUse;

		const VkBufferImageCopy	bufferImageCopy	=
		{
			defaultSize * defaultHalfSize + 1u,		// VkDeviceSize				bufferOffset;
			0u,											// deUint32					bufferRowLength;
			0u,											// deUint32					bufferImageHeight;
			defaultSourceLayer,							// VkImageSubresourceLayers	imageSubresource;
			{defaultFourthSize, defaultFourthSize, 0},	// VkOffset3D				imageOffset;
			defaultHalfExtent							// VkExtent3D				imageExtent;
		};
		CopyRegion	copyRegion;
		copyRegion.bufferImageCopy	= bufferImageCopy;

		params.regions.push_back(copyRegion);

		group->addChild(new CopyImageToBufferTestCase(testCtx, "buffer_offset_relaxed", "Copy from image to buffer with buffer offset not a multiple of 4", params));
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
		params.extensionUse					= extensionUse;

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
		params.extensionUse					= extensionUse;

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
		params.extensionUse					= extensionUse;

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

	{
		TestParams				params;
		deUint32				arrayLayers = 16u;
		params.src.image.imageType			= VK_IMAGE_TYPE_2D;
		params.src.image.format				= VK_FORMAT_R8G8B8A8_UNORM;
		params.src.image.extent				= defaultHalfExtent;
		params.src.image.extent.depth		= arrayLayers;
		params.src.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
		params.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		params.dst.buffer.size				= defaultHalfSize * defaultHalfSize * arrayLayers;
		params.allocationKind				= allocationKind;
		params.extensionUse					= extensionUse;

		const int pixelSize	= tcu::getPixelSize(mapVkFormat(params.src.image.format));
		for (deUint32 arrayLayerNdx = 0; arrayLayerNdx < arrayLayers; arrayLayerNdx++)
		{
			const VkDeviceSize offset = defaultHalfSize * defaultHalfSize * pixelSize * arrayLayerNdx;
			const VkBufferImageCopy bufferImageCopy =
				{
					offset,													// VkDeviceSize				bufferOffset;
					0u,														// deUint32					bufferRowLength;
					0u,														// deUint32					bufferImageHeight;
					{
						VK_IMAGE_ASPECT_COLOR_BIT,						// VkImageAspectFlags	aspectMask;
						0u,												// deUint32				mipLevel;
						arrayLayerNdx,									// deUint32				baseArrayLayer;
						1u,												// deUint32				layerCount;
					},														// VkImageSubresourceLayers	imageSubresource;
					{0, 0, 0},												// VkOffset3D				imageOffset;
					defaultHalfExtent										// VkExtent3D				imageExtent;
				};
			CopyRegion copyRegion;
			copyRegion.bufferImageCopy = bufferImageCopy;

			params.regions.push_back(copyRegion);
		}
		group->addChild(new CopyImageToBufferTestCase(testCtx, "array", "Copy each layer from array to buffer", params));
	}

	{
		TestParams				params;
		deUint32				arrayLayers = 16u;
		params.src.image.imageType			= VK_IMAGE_TYPE_2D;
		params.src.image.format				= VK_FORMAT_R8G8B8A8_UNORM;
		params.src.image.extent				= defaultHalfExtent;
		params.src.image.extent.depth		= arrayLayers;
		params.src.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
		params.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		params.dst.buffer.size				= defaultHalfSize * defaultHalfSize * arrayLayers;
		params.allocationKind				= allocationKind;
		params.extensionUse					= extensionUse;

		const int pixelSize	= tcu::getPixelSize(mapVkFormat(params.src.image.format));
		for (deUint32 arrayLayerNdx = 0; arrayLayerNdx < arrayLayers; arrayLayerNdx++)
		{
			const VkDeviceSize offset = defaultHalfSize * defaultHalfSize * pixelSize * arrayLayerNdx;
			const VkBufferImageCopy bufferImageCopy =
				{
					offset,													// VkDeviceSize				bufferOffset;
					defaultHalfSize,										// deUint32					bufferRowLength;
					defaultHalfSize,										// deUint32					bufferImageHeight;
					{
						VK_IMAGE_ASPECT_COLOR_BIT,						// VkImageAspectFlags	aspectMask;
						0u,												// deUint32				mipLevel;
						arrayLayerNdx,									// deUint32				baseArrayLayer;
						1u,												// deUint32				layerCount;
					},														// VkImageSubresourceLayers	imageSubresource;
					{0, 0, 0},												// VkOffset3D				imageOffset;
					defaultHalfExtent										// VkExtent3D				imageExtent;
				};
			CopyRegion copyRegion;
			copyRegion.bufferImageCopy = bufferImageCopy;

			params.regions.push_back(copyRegion);
		}
		group->addChild(new CopyImageToBufferTestCase(testCtx, "array_tightly_sized_buffer", "Copy each layer from array to tightly sized buffer", params));
	}
}

void addBufferToDepthStencilTests(tcu::TestCaseGroup* group, AllocationKind allocationKind, ExtensionUse extensionUse)
{
	tcu::TestContext& testCtx = group->getTestContext();

	const struct
	{
		const char*		name;
		const VkFormat	format;
	} depthAndStencilFormats[] =
	{
		{ "d16_unorm",				VK_FORMAT_D16_UNORM				},
		{ "x8_d24_unorm_pack32",	VK_FORMAT_X8_D24_UNORM_PACK32	},
		{ "d32_sfloat",				VK_FORMAT_D32_SFLOAT			},
		{ "d16_unorm_s8_uint",		VK_FORMAT_D16_UNORM_S8_UINT		},
		{ "d24_unorm_s8_uint",		VK_FORMAT_D24_UNORM_S8_UINT		},
		{ "d32_sfloat_s8_uint",		VK_FORMAT_D32_SFLOAT_S8_UINT	}
	};

	const VkImageSubresourceLayers	depthSourceLayer		=
	{
		VK_IMAGE_ASPECT_DEPTH_BIT,	// VkImageAspectFlags	aspectMask;
		0u,							// deUint32				mipLevel;
		0u,							// deUint32				baseArrayLayer;
		1u,							// deUint32				layerCount;
	};

	const VkBufferImageCopy			bufferDepthCopy			=
	{
		0u,											// VkDeviceSize				bufferOffset;
		0u,											// deUint32					bufferRowLength;
		0u,											// deUint32					bufferImageHeight;
		depthSourceLayer,							// VkImageSubresourceLayers	imageSubresource;
		{0, 0, 0},									// VkOffset3D				imageOffset;
		defaultExtent								// VkExtent3D				imageExtent;
	};

	const VkBufferImageCopy			bufferDepthCopyOffset	=
	{
		32,											// VkDeviceSize				bufferOffset;
		defaultHalfSize + defaultFourthSize,		// deUint32					bufferRowLength;
		defaultHalfSize + defaultFourthSize,		// deUint32					bufferImageHeight;
		depthSourceLayer,							// VkImageSubresourceLayers	imageSubresource;
		{defaultFourthSize, defaultFourthSize, 0},	// VkOffset3D				imageOffset;
		defaultHalfExtent							// VkExtent3D				imageExtent;
	};

	const VkImageSubresourceLayers	stencilSourceLayer		=
	{
		VK_IMAGE_ASPECT_STENCIL_BIT,	// VkImageAspectFlags	aspectMask;
		0u,								// deUint32				mipLevel;
		0u,								// deUint32				baseArrayLayer;
		1u,								// deUint32				layerCount;
	};

	const VkBufferImageCopy			bufferStencilCopy		=
	{
		0u,					// VkDeviceSize				bufferOffset;
		0u,					// deUint32					bufferRowLength;
		0u,					// deUint32					bufferImageHeight;
		stencilSourceLayer,	// VkImageSubresourceLayers	imageSubresource;
		{0, 0, 0},			// VkOffset3D				imageOffset;
		defaultExtent		// VkExtent3D				imageExtent;
	};

    const VkBufferImageCopy			bufferStencilCopyOffset	=
	{
		32,											// VkDeviceSize				bufferOffset;
		defaultHalfSize + defaultFourthSize,		// deUint32					bufferRowLength;
		defaultHalfSize + defaultFourthSize,		// deUint32					bufferImageHeight;
		stencilSourceLayer,							// VkImageSubresourceLayers	imageSubresource;
		{defaultFourthSize, defaultFourthSize, 0},	// VkOffset3D				imageOffset;
		defaultHalfExtent							// VkExtent3D				imageExtent;
	};

    const bool						useOffset[]				= {false, true};

	// Note: Depth stencil tests I want to do
	// Formats: D16, D24S8, D32FS8
	// Test writing each component with separate CopyBufferToImage commands
	// Test writing both components in one CopyBufferToImage command
	// Swap order of writes of Depth & Stencil
	// whole surface, subimages?
	// Similar tests as BufferToImage?
	for (const auto config : depthAndStencilFormats)
		for (const auto offset : useOffset)
		{
			// TODO: Check that this format is supported before creating tests?
			//if (isSupportedDepthStencilFormat(vki, physDevice, VK_FORMAT_D24_UNORM_S8_UINT))

			CopyRegion					copyDepthRegion;
			CopyRegion					copyStencilRegion;
			TestParams					params;
			const tcu::TextureFormat	format		= mapVkFormat(config.format);
			const bool					hasDepth	= tcu::hasDepthComponent(format.order);
			const bool					hasStencil	= tcu::hasStencilComponent(format.order);
			std::string					description	= config.name;

			if (offset)
			{
				copyDepthRegion.bufferImageCopy = bufferDepthCopyOffset;
				copyStencilRegion.bufferImageCopy = bufferStencilCopyOffset;
				description = "buffer_offset_" + description;
				params.src.buffer.size = (defaultHalfSize - 1u) * defaultSize + defaultHalfSize + defaultFourthSize;
			}
			else
			{
				copyDepthRegion.bufferImageCopy = bufferDepthCopy;
				copyStencilRegion.bufferImageCopy = bufferStencilCopy;
				params.src.buffer.size = defaultSize * defaultSize;
			}

			params.dst.image.imageType = VK_IMAGE_TYPE_2D;
			params.dst.image.format = config.format;
			params.dst.image.extent = defaultExtent;
			params.dst.image.tiling = VK_IMAGE_TILING_OPTIMAL;
			params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			params.allocationKind = allocationKind;
			params.extensionUse = extensionUse;

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

void addBufferToImageTests (tcu::TestCaseGroup* group, AllocationKind allocationKind, ExtensionUse extensionUse)
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
		params.extensionUse					= extensionUse;

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
		params.extensionUse					= extensionUse;

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
		params.extensionUse					= extensionUse;

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
		TestParams	params;
		params.src.buffer.size				= defaultSize * defaultSize;
		params.dst.image.imageType			= VK_IMAGE_TYPE_2D;
		params.dst.image.format				= VK_FORMAT_R8_UNORM;
		params.dst.image.extent				= defaultExtent;
		params.dst.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
		params.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		params.allocationKind				= allocationKind;
		params.extensionUse					= extensionUse;

		const VkBufferImageCopy	bufferImageCopy	=
		{
			defaultFourthSize + 1u,						// VkDeviceSize				bufferOffset;
			defaultHalfSize + defaultFourthSize,		// deUint32					bufferRowLength;
			defaultHalfSize + defaultFourthSize,		// deUint32					bufferImageHeight;
			defaultSourceLayer,							// VkImageSubresourceLayers	imageSubresource;
			{defaultFourthSize, defaultFourthSize, 0},	// VkOffset3D				imageOffset;
			defaultHalfExtent							// VkExtent3D				imageExtent;
		};
		CopyRegion	copyRegion;
		copyRegion.bufferImageCopy	= bufferImageCopy;

		params.regions.push_back(copyRegion);

		group->addChild(new CopyBufferToImageTestCase(testCtx, "buffer_offset_relaxed", "Copy from buffer to image with buffer offset not a multiple of 4", params));
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
		params.extensionUse					= extensionUse;

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
		params.extensionUse					= extensionUse;

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

	{
		TestParams				params;
		deUint32				arrayLayers = 16u;
		params.src.buffer.size				= defaultHalfSize * defaultHalfSize * arrayLayers;
		params.dst.image.imageType			= VK_IMAGE_TYPE_2D;
		params.dst.image.format				= VK_FORMAT_R8G8B8A8_UNORM;
		params.dst.image.extent				= defaultHalfExtent;
		params.dst.image.extent.depth		= arrayLayers;
		params.dst.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
		params.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		params.allocationKind				= allocationKind;
		params.extensionUse					= extensionUse;

		const int pixelSize	= tcu::getPixelSize(mapVkFormat(params.dst.image.format));
		for (deUint32 arrayLayerNdx = 0; arrayLayerNdx < arrayLayers; arrayLayerNdx++)
		{
			const VkDeviceSize offset = defaultHalfSize * defaultHalfSize * pixelSize * arrayLayerNdx;
			const VkBufferImageCopy bufferImageCopy =
				{
					offset,													// VkDeviceSize				bufferOffset;
					0u,														// deUint32					bufferRowLength;
					0u,														// deUint32					bufferImageHeight;
					{
						VK_IMAGE_ASPECT_COLOR_BIT,						// VkImageAspectFlags	aspectMask;
						0u,												// deUint32				mipLevel;
						arrayLayerNdx,									// deUint32				baseArrayLayer;
						1u,												// deUint32				layerCount;
					},														// VkImageSubresourceLayers	imageSubresource;
					{0, 0, 0},												// VkOffset3D				imageOffset;
					defaultHalfExtent										// VkExtent3D				imageExtent;
				};
			CopyRegion copyRegion;
			copyRegion.bufferImageCopy = bufferImageCopy;

			params.regions.push_back(copyRegion);
		}
		group->addChild(new CopyBufferToImageTestCase(testCtx, "array", "Copy from a different part of the buffer to each layer", params));
	}

	{
		TestParams				params;
		deUint32				arrayLayers = 16u;
		params.src.buffer.size				= defaultHalfSize * defaultHalfSize * arrayLayers;
		params.dst.image.imageType			= VK_IMAGE_TYPE_2D;
		params.dst.image.format				= VK_FORMAT_R8G8B8A8_UNORM;
		params.dst.image.extent				= defaultHalfExtent;
		params.dst.image.extent.depth		= arrayLayers;
		params.dst.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
		params.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		params.allocationKind				= allocationKind;
		params.extensionUse					= extensionUse;

		const int pixelSize	= tcu::getPixelSize(mapVkFormat(params.dst.image.format));
		for (deUint32 arrayLayerNdx = 0; arrayLayerNdx < arrayLayers; arrayLayerNdx++)
		{
			const VkDeviceSize offset = defaultHalfSize * defaultHalfSize * pixelSize * arrayLayerNdx;
			const VkBufferImageCopy bufferImageCopy =
				{
					offset,													// VkDeviceSize				bufferOffset;
					defaultHalfSize,										// deUint32					bufferRowLength;
					defaultHalfSize,										// deUint32					bufferImageHeight;
					{
						VK_IMAGE_ASPECT_COLOR_BIT,						// VkImageAspectFlags	aspectMask;
						0u,												// deUint32				mipLevel;
						arrayLayerNdx,									// deUint32				baseArrayLayer;
						1u,												// deUint32				layerCount;
					},														// VkImageSubresourceLayers	imageSubresource;
					{0, 0, 0},												// VkOffset3D				imageOffset;
					defaultHalfExtent										// VkExtent3D				imageExtent;
				};
			CopyRegion copyRegion;
			copyRegion.bufferImageCopy = bufferImageCopy;

			params.regions.push_back(copyRegion);
		}
		group->addChild(new CopyBufferToImageTestCase(testCtx, "array_tightly_sized_buffer", "Copy from different part of tightly sized buffer to each layer", params));
	}
}

void addBufferToBufferTests (tcu::TestCaseGroup* group, AllocationKind allocationKind, ExtensionUse extensionUse)
{
	tcu::TestContext&				testCtx					= group->getTestContext();

	{
		TestParams			params;
		params.src.buffer.size	= defaultSize;
		params.dst.buffer.size	= defaultSize;
		params.allocationKind	= allocationKind;
		params.extensionUse		= extensionUse;

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
		params.extensionUse		= extensionUse;

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
		params.extensionUse		= extensionUse;

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

void addBlittingImageSimpleTests (tcu::TestCaseGroup* group, TestParams& params)
{
	tcu::TestContext& testCtx = group->getTestContext();

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

	// Filter is VK_FILTER_CUBIC_EXT.
	// Cubic filtering can only be used with 2D images.
	if (params.dst.image.imageType == VK_IMAGE_TYPE_2D)
	{
		params.filter					= VK_FILTER_CUBIC_EXT;
		const std::string description	= "Cubic filter";

		params.dst.image.format = VK_FORMAT_R8G8B8A8_UNORM;
		group->addChild(new BlitImageTestCase(testCtx, "cubic", description, params));

		params.dst.image.format = VK_FORMAT_R32_SFLOAT;
		const std::string	descriptionOfRGBAToR32(description + " and different formats (R8G8B8A8 -> R32)");
		group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_cubic", descriptionOfRGBAToR32, params));

		params.dst.image.format = VK_FORMAT_B8G8R8A8_UNORM;
		const std::string	descriptionOfRGBAToBGRA(description + " and different formats (R8G8B8A8 -> B8G8R8A8)");
		group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_cubic", descriptionOfRGBAToBGRA, params));
	}
}

void addBlittingImageSimpleWholeTests (tcu::TestCaseGroup* group, TestParams params)
{
	DE_ASSERT(params.src.image.imageType == params.dst.image.imageType);
	const deInt32 imageDepth = params.src.image.imageType == VK_IMAGE_TYPE_3D ? defaultSize : 1;
	params.src.image.extent			= defaultExtent;
	params.dst.image.extent			= defaultExtent;
	params.src.image.extent.depth	= imageDepth;
	params.dst.image.extent.depth	= imageDepth;

	{
		const VkImageBlit imageBlit =
		{
			defaultSourceLayer,	// VkImageSubresourceLayers	srcSubresource;
			{
				{ 0, 0, 0 },
				{ defaultSize, defaultSize, imageDepth }
			},					// VkOffset3D				srcOffsets[2];

			defaultSourceLayer,	// VkImageSubresourceLayers	dstSubresource;
			{
				{ 0, 0, 0 },
				{ defaultSize, defaultSize, imageDepth }
			}					// VkOffset3D				dstOffset[2];
		};

		CopyRegion region;
		region.imageBlit = imageBlit;
		params.regions.push_back(region);
	}

	addBlittingImageSimpleTests(group, params);
}

void addBlittingImageSimpleMirrorXYTests (tcu::TestCaseGroup* group, TestParams params)
{
	DE_ASSERT(params.src.image.imageType == params.dst.image.imageType);
	const deInt32 imageDepth = params.src.image.imageType == VK_IMAGE_TYPE_3D ? defaultSize : 1;
	params.src.image.extent			= defaultExtent;
	params.dst.image.extent			= defaultExtent;
	params.src.image.extent.depth	= imageDepth;
	params.dst.image.extent.depth	= imageDepth;

	{
		const VkImageBlit imageBlit =
		{
			defaultSourceLayer,	// VkImageSubresourceLayers	srcSubresource;
			{
				{0, 0, 0},
				{defaultSize, defaultSize, imageDepth}
			},					// VkOffset3D				srcOffsets[2];

			defaultSourceLayer,	// VkImageSubresourceLayers	dstSubresource;
			{
				{defaultSize, defaultSize, 0},
				{0, 0, imageDepth}
			}					// VkOffset3D				dstOffset[2];
		};

		CopyRegion region;
		region.imageBlit = imageBlit;
		params.regions.push_back(region);
	}

	addBlittingImageSimpleTests(group, params);
}

void addBlittingImageSimpleMirrorXTests (tcu::TestCaseGroup* group, TestParams params)
{
	DE_ASSERT(params.src.image.imageType == params.dst.image.imageType);
	const deInt32 imageDepth = params.src.image.imageType == VK_IMAGE_TYPE_3D ? defaultSize : 1;
	params.src.image.extent			= defaultExtent;
	params.dst.image.extent			= defaultExtent;
	params.src.image.extent.depth	= imageDepth;
	params.dst.image.extent.depth	= imageDepth;

	{
		const VkImageBlit imageBlit =
		{
			defaultSourceLayer,	// VkImageSubresourceLayers	srcSubresource;
			{
				{0, 0, 0},
				{defaultSize, defaultSize, imageDepth}
			},					// VkOffset3D				srcOffsets[2];

			defaultSourceLayer,	// VkImageSubresourceLayers	dstSubresource;
			{
				{defaultSize, 0, 0},
				{0, defaultSize, imageDepth}
			}					// VkOffset3D				dstOffset[2];
		};

		CopyRegion region;
		region.imageBlit = imageBlit;
		params.regions.push_back(region);
	}

	addBlittingImageSimpleTests(group, params);
}

void addBlittingImageSimpleMirrorYTests (tcu::TestCaseGroup* group, TestParams params)
{
	DE_ASSERT(params.src.image.imageType == params.dst.image.imageType);
	const deInt32 imageDepth = params.src.image.imageType == VK_IMAGE_TYPE_3D ? defaultSize : 1;
	params.src.image.extent			= defaultExtent;
	params.dst.image.extent			= defaultExtent;
	params.src.image.extent.depth	= imageDepth;
	params.dst.image.extent.depth	= imageDepth;

	{
		const VkImageBlit imageBlit =
		{
			defaultSourceLayer,	// VkImageSubresourceLayers	srcSubresource;
			{
				{0, 0, 0},
				{defaultSize, defaultSize, imageDepth}
			},					// VkOffset3D				srcOffsets[2];

			defaultSourceLayer,	// VkImageSubresourceLayers	dstSubresource;
			{
				{0, defaultSize, 0},
				{defaultSize, 0, imageDepth}
			}					// VkOffset3D				dstOffset[2];
		};

		CopyRegion region;
		region.imageBlit = imageBlit;
		params.regions.push_back(region);
	}

	addBlittingImageSimpleTests(group, params);
}

void addBlittingImageSimpleMirrorZTests (tcu::TestCaseGroup* group, TestParams params)
{
	DE_ASSERT(params.src.image.imageType == params.dst.image.imageType);
	DE_ASSERT(params.src.image.imageType == VK_IMAGE_TYPE_3D);
	params.src.image.extent			= defaultExtent;
	params.dst.image.extent			= defaultExtent;
	params.src.image.extent.depth	= defaultSize;
	params.dst.image.extent.depth	= defaultSize;

	{
		const VkImageBlit imageBlit =
		{
			defaultSourceLayer,	// VkImageSubresourceLayers	srcSubresource;
			{
				{0, 0, 0},
				{defaultSize, defaultSize, defaultSize}
			},					// VkOffset3D				srcOffsets[2];

			defaultSourceLayer,	// VkImageSubresourceLayers	dstSubresource;
			{
				{0, 0, defaultSize},
				{defaultSize, defaultSize, 0}
			}					// VkOffset3D				dstOffset[2];
		};

		CopyRegion region;
		region.imageBlit = imageBlit;
		params.regions.push_back(region);
	}

	addBlittingImageSimpleTests(group, params);
}

void addBlittingImageSimpleMirrorSubregionsTests (tcu::TestCaseGroup* group, TestParams params)
{
	DE_ASSERT(params.src.image.imageType == params.dst.image.imageType);
	const deInt32 imageDepth = params.src.image.imageType == VK_IMAGE_TYPE_3D ? defaultSize : 1;
	params.src.image.extent			= defaultExtent;
	params.dst.image.extent			= defaultExtent;
	params.src.image.extent.depth	= imageDepth;
	params.dst.image.extent.depth	= imageDepth;

	// No mirroring.
	{
		const VkImageBlit imageBlit =
		{
			defaultSourceLayer,	// VkImageSubresourceLayers	srcSubresource;
			{
				{0, 0, 0},
				{defaultHalfSize, defaultHalfSize, imageDepth}
			},					// VkOffset3D				srcOffsets[2];

			defaultSourceLayer,	// VkImageSubresourceLayers	dstSubresource;
			{
				{0, 0, 0},
				{defaultHalfSize, defaultHalfSize, imageDepth}
			}					// VkOffset3D				dstOffset[2];
		};

		CopyRegion region;
		region.imageBlit = imageBlit;
		params.regions.push_back(region);
	}

	// Flipping y coordinates.
	{
		const VkImageBlit imageBlit =
		{
			defaultSourceLayer,	// VkImageSubresourceLayers	srcSubresource;
			{
				{defaultHalfSize, 0, 0},
				{defaultSize, defaultHalfSize, imageDepth}
			},					// VkOffset3D				srcOffsets[2];

			defaultSourceLayer,	// VkImageSubresourceLayers	dstSubresource;
			{
				{defaultHalfSize, defaultHalfSize, 0},
				{defaultSize, 0, imageDepth}
			}					// VkOffset3D				dstOffset[2];
		};
		CopyRegion region;
		region.imageBlit = imageBlit;
		params.regions.push_back(region);
	}

	// Flipping x coordinates.
	{
		const VkImageBlit imageBlit =
		{
			defaultSourceLayer,	// VkImageSubresourceLayers	srcSubresource;
			{
				{0, defaultHalfSize, 0},
				{defaultHalfSize, defaultSize, imageDepth}
			},					// VkOffset3D				srcOffsets[2];

			defaultSourceLayer,	// VkImageSubresourceLayers	dstSubresource;
			{
				{defaultHalfSize, defaultHalfSize, 0},
				{0, defaultSize, imageDepth}
			}					// VkOffset3D				dstOffset[2];
		};

		CopyRegion region;
		region.imageBlit = imageBlit;
		params.regions.push_back(region);
	}

	// Flipping x and y coordinates.
	{
		const VkImageBlit imageBlit =
		{
			defaultSourceLayer,	// VkImageSubresourceLayers	srcSubresource;
			{
				{defaultHalfSize, defaultHalfSize, 0},
				{defaultSize, defaultSize, imageDepth}
			},					// VkOffset3D				srcOffsets[2];

			defaultSourceLayer,	// VkImageSubresourceLayers	dstSubresource;
			{
				{defaultSize, defaultSize, 0},
				{defaultHalfSize, defaultHalfSize, imageDepth}
			}					// VkOffset3D				dstOffset[2];
		};

		CopyRegion region;
		region.imageBlit = imageBlit;
		params.regions.push_back(region);
	}

	addBlittingImageSimpleTests(group, params);
}

void addBlittingImageSimpleScalingWhole1Tests (tcu::TestCaseGroup* group, TestParams params)
{
	DE_ASSERT(params.src.image.imageType == params.dst.image.imageType);
	const deInt32	imageDepth		= params.src.image.imageType == VK_IMAGE_TYPE_3D ? defaultSize : 1;
	const deInt32	halfImageDepth	= params.src.image.imageType == VK_IMAGE_TYPE_3D ? defaultHalfSize : 1;
	params.src.image.extent			= defaultExtent;
	params.dst.image.extent			= defaultHalfExtent;
	params.src.image.extent.depth	= imageDepth;
	params.dst.image.extent.depth	= halfImageDepth;

	{
		const VkImageBlit imageBlit =
		{
			defaultSourceLayer,	// VkImageSubresourceLayers	srcSubresource;
			{
				{0, 0, 0},
				{defaultSize, defaultSize, imageDepth}
			},					// VkOffset3D					srcOffsets[2];

			defaultSourceLayer,	// VkImageSubresourceLayers	dstSubresource;
			{
				{0, 0, 0},
				{defaultHalfSize, defaultHalfSize, halfImageDepth}
			}					// VkOffset3D					dstOffset[2];
		};

		CopyRegion region;
		region.imageBlit = imageBlit;
		params.regions.push_back(region);
	}

	addBlittingImageSimpleTests(group, params);
}

void addBlittingImageSimpleScalingWhole2Tests (tcu::TestCaseGroup* group, TestParams params)
{
	DE_ASSERT(params.src.image.imageType == params.dst.image.imageType);
	const deInt32	imageDepth		= params.src.image.imageType == VK_IMAGE_TYPE_3D ? defaultSize : 1;
	const deInt32	halfImageDepth	= params.src.image.imageType == VK_IMAGE_TYPE_3D ? defaultHalfSize : 1;
	params.src.image.extent			= defaultHalfExtent;
	params.dst.image.extent			= defaultExtent;
	params.src.image.extent.depth	= halfImageDepth;
	params.dst.image.extent.depth	= imageDepth;

	{
		const VkImageBlit imageBlit =
		{
			defaultSourceLayer,	// VkImageSubresourceLayers	srcSubresource;
			{
				{0, 0, 0},
				{defaultHalfSize, defaultHalfSize, halfImageDepth}
			},					// VkOffset3D					srcOffsets[2];

			defaultSourceLayer,	// VkImageSubresourceLayers	dstSubresource;
			{
				{0, 0, 0},
				{defaultSize, defaultSize, imageDepth}
			}					// VkOffset3D					dstOffset[2];
		};

		CopyRegion region;
		region.imageBlit = imageBlit;
		params.regions.push_back(region);
	}

	addBlittingImageSimpleTests(group, params);
}

void addBlittingImageSimpleScalingAndOffsetTests (tcu::TestCaseGroup* group, TestParams params)
{
	DE_ASSERT(params.src.image.imageType == params.dst.image.imageType);
	const deInt32	imageDepth		= params.src.image.imageType == VK_IMAGE_TYPE_3D ? defaultSize : 1;
	const deInt32	srcDepthOffset	= params.src.image.imageType == VK_IMAGE_TYPE_3D ? defaultFourthSize : 0;
	const deInt32	srcDepthSize	= params.src.image.imageType == VK_IMAGE_TYPE_3D ? defaultFourthSize * 3 : 1;
	params.src.image.extent			= defaultExtent;
	params.dst.image.extent			= defaultExtent;
	params.src.image.extent.depth	= imageDepth;
	params.dst.image.extent.depth	= imageDepth;

	{
		const VkImageBlit imageBlit =
		{
			defaultSourceLayer,	// VkImageSubresourceLayers	srcSubresource;
			{
				{defaultFourthSize, defaultFourthSize, srcDepthOffset},
				{defaultFourthSize*3, defaultFourthSize*3, srcDepthSize}
			},					// VkOffset3D					srcOffsets[2];

			defaultSourceLayer,	// VkImageSubresourceLayers	dstSubresource;
			{
				{0, 0, 0},
				{defaultSize, defaultSize, imageDepth}
			}					// VkOffset3D					dstOffset[2];
		};

		CopyRegion region;
		region.imageBlit = imageBlit;
		params.regions.push_back(region);
	}

	addBlittingImageSimpleTests(group, params);
}

void addBlittingImageSimpleWithoutScalingPartialTests (tcu::TestCaseGroup* group, TestParams params)
{
	DE_ASSERT(params.src.image.imageType == params.dst.image.imageType);
	const bool is3dBlit = params.src.image.imageType == VK_IMAGE_TYPE_3D;
	params.src.image.extent	= defaultExtent;
	params.dst.image.extent	= defaultExtent;

	if (is3dBlit)
	{
		params.src.image.extent.depth = defaultSize;
		params.dst.image.extent.depth = defaultSize;
	}

	{
		CopyRegion region;
		for (int i = 0; i < defaultSize; i += defaultFourthSize)
		{
			const VkImageBlit imageBlit =
			{
				defaultSourceLayer,	// VkImageSubresourceLayers	srcSubresource;
				{
					{defaultSize - defaultFourthSize - i, defaultSize - defaultFourthSize - i, is3dBlit ? defaultSize - defaultFourthSize - i : 0},
					{defaultSize - i, defaultSize - i, is3dBlit ? defaultSize - i : 1}
				},					// VkOffset3D					srcOffsets[2];

				defaultSourceLayer,	// VkImageSubresourceLayers	dstSubresource;
				{
					{i, i, is3dBlit ? i : 0},
					{i + defaultFourthSize, i + defaultFourthSize, is3dBlit ? i + defaultFourthSize : 1}
				}					// VkOffset3D					dstOffset[2];
			};
			region.imageBlit = imageBlit;
			params.regions.push_back(region);
		}
	}

	addBlittingImageSimpleTests(group, params);
}

void addBlittingImageSimpleTests (tcu::TestCaseGroup* group, AllocationKind allocationKind, ExtensionUse extensionUse)
{
	TestParams params;
	params.src.image.format				= VK_FORMAT_R8G8B8A8_UNORM;
	params.src.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
	params.src.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	params.dst.image.tiling				= VK_IMAGE_TILING_OPTIMAL;
	params.dst.image.operationLayout	= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	params.allocationKind				= allocationKind;
	params.extensionUse					= extensionUse;
	params.src.image.imageType			= VK_IMAGE_TYPE_2D;
	params.dst.image.imageType			= VK_IMAGE_TYPE_2D;
	addTestGroup(group, "whole", "Blit without scaling (whole)", addBlittingImageSimpleWholeTests, params);
	addTestGroup(group, "mirror_xy", "Flipping x and y coordinates (whole)", addBlittingImageSimpleMirrorXYTests, params);
	addTestGroup(group, "mirror_x", "Flipping x coordinates (whole)", addBlittingImageSimpleMirrorXTests, params);
	addTestGroup(group, "mirror_y", "Flipping y coordinates (whole)", addBlittingImageSimpleMirrorYTests, params);
	addTestGroup(group, "mirror_subregions", "Mirroring subregions in image (no flip, y flip, x flip, xy flip)", addBlittingImageSimpleMirrorSubregionsTests, params);
	addTestGroup(group, "scaling_whole1", "Blit with scaling (whole, src extent bigger)", addBlittingImageSimpleScalingWhole1Tests, params);
	addTestGroup(group, "scaling_whole2", "Blit with scaling (whole, dst extent bigger)", addBlittingImageSimpleScalingWhole2Tests, params);
	addTestGroup(group, "scaling_and_offset", "Blit with scaling and offset (whole, dst extent bigger)", addBlittingImageSimpleScalingAndOffsetTests, params);
	addTestGroup(group, "without_scaling_partial", "Blit without scaling (partial)", addBlittingImageSimpleWithoutScalingPartialTests, params);

	params.src.image.imageType			= VK_IMAGE_TYPE_3D;
	params.dst.image.imageType			= VK_IMAGE_TYPE_3D;
	addTestGroup(group, "whole_3d", "3D blit without scaling (whole)", addBlittingImageSimpleWholeTests, params);
	addTestGroup(group, "mirror_xy_3d", "Flipping x and y coordinates of a 3D image (whole)", addBlittingImageSimpleMirrorXYTests, params);
	addTestGroup(group, "mirror_x_3d", "Flipping x coordinates of a 3D image (whole)", addBlittingImageSimpleMirrorXTests, params);
	addTestGroup(group, "mirror_y_3d", "Flipping y coordinates of a 3D image (whole)", addBlittingImageSimpleMirrorYTests, params);
	addTestGroup(group, "mirror_z_3d", "Flipping z coordinates of a 3D image (whole)", addBlittingImageSimpleMirrorZTests, params);
	addTestGroup(group, "mirror_subregions_3d", "Mirroring subregions in a 3D image (no flip, y flip, x flip, xy flip)", addBlittingImageSimpleMirrorSubregionsTests, params);
	addTestGroup(group, "scaling_whole1_3d", "3D blit a with scaling (whole, src extent bigger)", addBlittingImageSimpleScalingWhole1Tests, params);
	addTestGroup(group, "scaling_whole2_3d", "3D blit with scaling (whole, dst extent bigger)", addBlittingImageSimpleScalingWhole2Tests, params);
	addTestGroup(group, "scaling_and_offset_3d", "3D blit with scaling and offset (whole, dst extent bigger)", addBlittingImageSimpleScalingAndOffsetTests, params);
	addTestGroup(group, "without_scaling_partial_3d", "3D blit without scaling (partial)", addBlittingImageSimpleWithoutScalingPartialTests, params);
}

enum FilterMaskBits
{
	FILTER_MASK_NEAREST	= 0,			// Always tested.
	FILTER_MASK_LINEAR	= (1u << 0),
	FILTER_MASK_CUBIC	= (1u << 1),
};

using FilterMask = deUint32;

FilterMask makeFilterMask (bool onlyNearest, bool discardCubicFilter)
{
	FilterMask mask = FILTER_MASK_NEAREST;

	if (!onlyNearest)
	{
		mask |= FILTER_MASK_LINEAR;
		if (!discardCubicFilter)
			mask |= FILTER_MASK_CUBIC;
	}

	return mask;
}

struct BlitColorTestParams
{
	TestParams		params;
	const VkFormat*	compatibleFormats;
	FilterMask		testFilters;
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

					if (testParams.testFilters & FILTER_MASK_LINEAR)
					{
						testParams.params.filter = VK_FILTER_LINEAR;
						group->addChild(new BlitImageTestCase(testCtx, testName + "_linear", description, testParams.params));
					}

					if (testParams.testFilters & FILTER_MASK_CUBIC)
					{
						testParams.params.filter = VK_FILTER_CUBIC_EXT;
						group->addChild(new BlitImageTestCase(testCtx, testName + "_cubic", description, testParams.params));
					}

					if (testParams.params.src.image.imageType == VK_IMAGE_TYPE_3D)
					{
						const struct
						{
							FillMode	mode;
							const char*	name;
						} modeList[] =
						{
							{ FILL_MODE_BLUE_RED_X, "x" },
							{ FILL_MODE_BLUE_RED_Y, "y" },
							{ FILL_MODE_BLUE_RED_Z, "z" },
						};

						auto otherParams = testParams;
						otherParams.params.dst.image.fillMode = FILL_MODE_WHITE;

						for (int i = 0; i < DE_LENGTH_OF_ARRAY(modeList); ++i)
						{
							otherParams.params.src.image.fillMode = modeList[i].mode;

							otherParams.params.filter = VK_FILTER_LINEAR;
							group->addChild(new BlitImageTestCase(testCtx, testName + "_linear_stripes_" + modeList[i].name, description, otherParams.params));

							otherParams.params.filter = VK_FILTER_NEAREST;
							group->addChild(new BlitImageTestCase(testCtx, testName + "_nearest_stripes_" + modeList[i].name, description, otherParams.params));
						}
					}
				}
			}
		}
	}
}

void addBlittingImageAllFormatsColorSrcFormatTests (tcu::TestCaseGroup* group, BlitColorTestParams testParams)
{
	// If testParams.compatibleFormats is nullptr, the destination format will be copied from the source format.
	const VkFormat	srcFormatOnly[2]	= { testParams.params.src.image.format, VK_FORMAT_UNDEFINED };
	const VkFormat*	formatList			= (testParams.compatibleFormats ? testParams.compatibleFormats : srcFormatOnly);

	for (int dstFormatIndex = 0; formatList[dstFormatIndex] != VK_FORMAT_UNDEFINED; ++dstFormatIndex)
	{
		testParams.params.dst.image.format	= formatList[dstFormatIndex];
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

	VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT,
	VK_FORMAT_A4B4G4R4_UNORM_PACK16_EXT,

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

// skip cubic filtering test for the following data formats
const FormatSet	onlyNearestAndLinearFormatsToTest =
{
	VK_FORMAT_A8B8G8R8_USCALED_PACK32,
	VK_FORMAT_A8B8G8R8_SSCALED_PACK32,
	VK_FORMAT_A8B8G8R8_UINT_PACK32,
	VK_FORMAT_A8B8G8R8_SINT_PACK32
};

void addBlittingImageAllFormatsColorTests (tcu::TestCaseGroup* group, AllocationKind allocationKind, ExtensionUse extensionUse)
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

	if (allocationKind == ALLOCATION_KIND_DEDICATED)
	{
		const int numOfColorImageFormatsToTestFilter = DE_LENGTH_OF_ARRAY(dedicatedAllocationBlittingFormatsToTest);
		for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < numOfColorImageFormatsToTestFilter; ++compatibleFormatsIndex)
			dedicatedAllocationBlittingFormatsToTestSet.insert(dedicatedAllocationBlittingFormatsToTest[compatibleFormatsIndex]);
	}

	// 2D tests.
	{
		de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "2d", "2D blitting tests"));

		TestParams	params;
		params.src.image.imageType	= VK_IMAGE_TYPE_2D;
		params.dst.image.imageType	= VK_IMAGE_TYPE_2D;
		params.src.image.extent		= defaultExtent;
		params.dst.image.extent		= defaultExtent;
		params.src.image.tiling		= VK_IMAGE_TILING_OPTIMAL;
		params.dst.image.tiling		= VK_IMAGE_TILING_OPTIMAL;
		params.allocationKind		= allocationKind;
		params.extensionUse			= extensionUse;

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

		for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < numOfColorImageFormatsToTest; ++compatibleFormatsIndex)
		{
			const VkFormat*	compatibleFormats	= colorImageFormatsToTestBlit[compatibleFormatsIndex].compatibleFormats;
			const bool		onlyNearest			= colorImageFormatsToTestBlit[compatibleFormatsIndex].onlyNearest;
			for (int srcFormatIndex = 0; compatibleFormats[srcFormatIndex] != VK_FORMAT_UNDEFINED; ++srcFormatIndex)
			{
				params.src.image.format	= compatibleFormats[srcFormatIndex];
				if (!isSupportedByFramework(params.src.image.format))
					continue;

				const bool onlyNearestAndLinear	= de::contains(onlyNearestAndLinearFormatsToTest, params.src.image.format);

				BlitColorTestParams		testParams;
				testParams.params				= params;
				testParams.compatibleFormats	= compatibleFormats;
				testParams.testFilters			= makeFilterMask(onlyNearest, onlyNearestAndLinear);

				const std::string description	= "Blit source format " + getFormatCaseName(params.src.image.format);
				addTestGroup(subGroup.get(), getFormatCaseName(params.src.image.format), description, addBlittingImageAllFormatsColorSrcFormatTests, testParams);
			}
		}

		group->addChild(subGroup.release());
	}

	// 1D tests.
	{
		de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "1d", "1D blitting tests"));

		TestParams	params;
		params.src.image.imageType	= VK_IMAGE_TYPE_1D;
		params.dst.image.imageType	= VK_IMAGE_TYPE_1D;
		params.src.image.extent		= default1dExtent;
		params.dst.image.extent		= default1dExtent;
		params.src.image.tiling		= VK_IMAGE_TILING_OPTIMAL;
		params.dst.image.tiling		= VK_IMAGE_TILING_OPTIMAL;
		params.allocationKind		= allocationKind;
		params.extensionUse			= extensionUse;

		CopyRegion	region;
		for (int i = 0; i < defaultSize; i += defaultSize / 2)
		{
			const VkImageBlit			imageBlit	=
			{
				defaultSourceLayer,	// VkImageSubresourceLayers	srcSubresource;
				{
					{0, 0, 0},
					{defaultSize, 1, 1}
				},					// VkOffset3D					srcOffsets[2];

				defaultSourceLayer,	// VkImageSubresourceLayers	dstSubresource;
				{
					{i, 0, 0},
					{i + defaultFourthSize, 1, 1}
				}					// VkOffset3D					dstOffset[2];
			};
			region.imageBlit	= imageBlit;
			params.regions.push_back(region);
		}

		{
			const VkImageBlit			imageBlit	=
			{
				defaultSourceLayer,	// VkImageSubresourceLayers	srcSubresource;
				{
					{0, 0, 0},
					{defaultFourthSize, 1, 1}
				},					// VkOffset3D					srcOffsets[2];

				defaultSourceLayer,	// VkImageSubresourceLayers	dstSubresource;
				{
					{defaultFourthSize, 0, 0},
					{2 * defaultFourthSize, 1, 1}
				}					// VkOffset3D					dstOffset[2];
			};
			region.imageBlit	= imageBlit;
			params.regions.push_back(region);
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

				// Cubic filtering can only be used with 2D images.
				const bool onlyNearestAndLinear	= true;

				BlitColorTestParams	testParams;
				testParams.params				= params;
				testParams.compatibleFormats	= nullptr;
				testParams.testFilters			= makeFilterMask(onlyNearest, onlyNearestAndLinear);

				const std::string description	= "Blit source format " + getFormatCaseName(params.src.image.format);
				addTestGroup(subGroup.get(), getFormatCaseName(params.src.image.format), description, addBlittingImageAllFormatsColorSrcFormatTests, testParams);
			}
		}

		group->addChild(subGroup.release());
	}

	// 3D tests. Note we use smaller dimensions here for performance reasons.
	{
		de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "3d", "3D blitting tests"));

		TestParams	params;
		params.src.image.imageType	= VK_IMAGE_TYPE_3D;
		params.dst.image.imageType	= VK_IMAGE_TYPE_3D;
		params.src.image.extent		= default3dExtent;
		params.dst.image.extent		= default3dExtent;
		params.src.image.tiling		= VK_IMAGE_TILING_OPTIMAL;
		params.dst.image.tiling		= VK_IMAGE_TILING_OPTIMAL;
		params.allocationKind		= allocationKind;
		params.extensionUse			= extensionUse;

		CopyRegion	region;
		for (int i = 0, j = 1; (i + defaultSixteenthSize / j < defaultFourthSize) && (defaultSixteenthSize > j); i += defaultSixteenthSize / j++)
		{
			const VkImageBlit			imageBlit	=
			{
				defaultSourceLayer,	// VkImageSubresourceLayers	srcSubresource;
				{
					{0, 0, 0},
					{defaultFourthSize, defaultFourthSize, defaultFourthSize}
				},					// VkOffset3D					srcOffsets[2];

				defaultSourceLayer,	// VkImageSubresourceLayers	dstSubresource;
				{
					{i, 0, i},
					{i + defaultSixteenthSize / j, defaultSixteenthSize / j, i + defaultSixteenthSize / j}
				}					// VkOffset3D					dstOffset[2];
			};
			region.imageBlit	= imageBlit;
			params.regions.push_back(region);
		}
		for (int i = 0; i < defaultFourthSize; i += defaultSixteenthSize)
		{
			const VkImageBlit			imageBlit	=
			{
				defaultSourceLayer,	// VkImageSubresourceLayers	srcSubresource;
				{
					{i, i, i},
					{i + defaultSixteenthSize, i + defaultSixteenthSize, i + defaultSixteenthSize}
				},					// VkOffset3D					srcOffsets[2];

				defaultSourceLayer,	// VkImageSubresourceLayers	dstSubresource;
				{
					{i, defaultFourthSize / 2, i},
					{i + defaultSixteenthSize, defaultFourthSize / 2 + defaultSixteenthSize, i + defaultSixteenthSize}
				}					// VkOffset3D					dstOffset[2];
			};
			region.imageBlit	= imageBlit;
			params.regions.push_back(region);
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

				// Cubic filtering can only be used with 2D images.
				const bool onlyNearestAndLinear	= true;

				BlitColorTestParams	testParams;
				testParams.params				= params;
				testParams.compatibleFormats	= nullptr;
				testParams.testFilters			= makeFilterMask(onlyNearest, onlyNearestAndLinear);

				const std::string description	= "Blit source format " + getFormatCaseName(params.src.image.format);
				addTestGroup(subGroup.get(), getFormatCaseName(params.src.image.format), description, addBlittingImageAllFormatsColorSrcFormatTests, testParams);
			}
		}

		group->addChild(subGroup.release());
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

void addBlittingImageAllFormatsDepthStencilTests (tcu::TestCaseGroup* group, AllocationKind allocationKind, ExtensionUse extensionUse)
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
	{
		de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "2d", "2D blitting tests"));

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
			params.extensionUse					= extensionUse;
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

			const std::string testName		= getFormatCaseName(params.src.image.format) + "_" + getFormatCaseName(params.dst.image.format);
			const std::string description	= "Blit from " + getFormatCaseName(params.src.image.format) +
											" to " + getFormatCaseName(params.dst.image.format);
			addTestGroup(subGroup.get(), testName, description, addBlittingImageAllFormatsDepthStencilFormatsTests, params);

			if (hasDepth && hasStencil)
			{
				params.separateDepthStencilLayouts	= DE_TRUE;
				const std::string testName2		= getFormatCaseName(params.src.image.format) + "_" +
												getFormatCaseName(params.dst.image.format) + "_separate_layouts";
				const std::string description2	= "Blit from " + getFormatCaseName(params.src.image.format) +
												" to " + getFormatCaseName(params.dst.image.format) + " with separate depth/stencil layouts";
				addTestGroup(subGroup.get(), testName2, description2, addBlittingImageAllFormatsDepthStencilFormatsTests, params);
			}
		}

		group->addChild(subGroup.release());
	}

	// 1D tests
	{
		de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "1d", "1D blitting tests"));

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
			params.extensionUse					= extensionUse;

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

			const std::string testName		= getFormatCaseName(params.src.image.format) + "_" + getFormatCaseName(params.dst.image.format);
			const std::string description	= "Blit from " + getFormatCaseName(params.src.image.format) +
											" to " + getFormatCaseName(params.dst.image.format);
			addTestGroup(subGroup.get(), testName, description, addBlittingImageAllFormatsDepthStencilFormatsTests, params);

			if (hasDepth && hasStencil)
			{
				params.separateDepthStencilLayouts	= DE_TRUE;
				const std::string testName2		= getFormatCaseName(params.src.image.format) + "_" +
												getFormatCaseName(params.dst.image.format) + "_separate_layouts";
				const std::string description2	= "Blit from " + getFormatCaseName(params.src.image.format) +
												" to " + getFormatCaseName(params.dst.image.format) + " with separate depth/stencil layouts";
				addTestGroup(subGroup.get(), testName2, description2, addBlittingImageAllFormatsDepthStencilFormatsTests, params);
			}
		}

		group->addChild(subGroup.release());
	}

	// 3D tests. Note we use smaller dimensions here for performance reasons.
	{
		de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "3d", "3D blitting tests"));

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
			params.extensionUse					= extensionUse;

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

			const std::string testName		= getFormatCaseName(params.src.image.format) + "_" + getFormatCaseName(params.dst.image.format);
			const std::string description	= "Blit from " + getFormatCaseName(params.src.image.format) +
											" to " + getFormatCaseName(params.dst.image.format);
			addTestGroup(subGroup.get(), testName, description, addBlittingImageAllFormatsDepthStencilFormatsTests, params);

			if (hasDepth && hasStencil)
			{
				params.separateDepthStencilLayouts	= DE_TRUE;
				const std::string testName2		= getFormatCaseName(params.src.image.format) + "_" +
												getFormatCaseName(params.dst.image.format) + "_separate_layouts";
				const std::string description2	= "Blit from " + getFormatCaseName(params.src.image.format) +
												" to " + getFormatCaseName(params.dst.image.format) + " with separate depth/stencil layouts";
				addTestGroup(subGroup.get(), testName2, description2, addBlittingImageAllFormatsDepthStencilFormatsTests, params);
			}
		}

		group->addChild(subGroup.release());
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

			if (testParams.testFilters & FILTER_MASK_LINEAR)
			{
				testParams.params.filter = VK_FILTER_LINEAR;
				group->addChild(new BlitMipmapTestCase(testCtx, testName + "_linear", description, testParams.params));
			}

			if (testParams.testFilters & FILTER_MASK_CUBIC)
			{
				testParams.params.filter = VK_FILTER_CUBIC_EXT;
				group->addChild(new BlitMipmapTestCase(testCtx, testName + "_cubic", description, testParams.params));
			}
		}
	}
}

void addBlittingImageAllFormatsBaseLevelMipmapTests (tcu::TestCaseGroup* group, AllocationKind allocationKind, ExtensionUse extensionUse)
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
	params.extensionUse			= extensionUse;
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

				const bool onlyNearestAndLinear	= de::contains(onlyNearestAndLinearFormatsToTest, params.src.image.format);

				const std::string description	= "Blit source format " + getFormatCaseName(params.src.image.format);

				BlitColorTestParams testParams;
				testParams.params				= params;
				testParams.compatibleFormats	= compatibleFormats;
				testParams.testFilters			= makeFilterMask(onlyNearest, onlyNearestAndLinear);

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

void addBlittingImageAllFormatsPreviousLevelMipmapTests (tcu::TestCaseGroup* group, AllocationKind allocationKind, ExtensionUse extensionUse)
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
	params.extensionUse			= extensionUse;
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

				const bool			onlyNearestAndLinear	= de::contains(onlyNearestAndLinearFormatsToTest, params.src.image.format);

				const std::string	description				= "Blit source format " + getFormatCaseName(params.src.image.format);

				BlitColorTestParams	testParams;
				testParams.params							= params;
				testParams.compatibleFormats				= compatibleFormats;
				testParams.testFilters						= makeFilterMask(onlyNearest, onlyNearestAndLinear);

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
					testParams.testFilters						= FILTER_MASK_NEAREST;

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

void addBlittingImageAllFormatsMipmapTests (tcu::TestCaseGroup* group, AllocationKind allocationKind, ExtensionUse extensionUse)
{
	addTestGroup(group, "from_base_level", "Generate all mipmap levels from base level", addBlittingImageAllFormatsBaseLevelMipmapTests, allocationKind, extensionUse);
	addTestGroup(group, "from_previous_level", "Generate next mipmap level from previous level", addBlittingImageAllFormatsPreviousLevelMipmapTests, allocationKind, extensionUse);
}

void addBlittingImageAllFormatsTests (tcu::TestCaseGroup* group, AllocationKind allocationKind, ExtensionUse extensionUse)
{
	addTestGroup(group, "color", "Blitting image with color formats", addBlittingImageAllFormatsColorTests, allocationKind, extensionUse);
	addTestGroup(group, "depth_stencil", "Blitting image with depth/stencil formats", addBlittingImageAllFormatsDepthStencilTests, allocationKind, extensionUse);
	addTestGroup(group, "generate_mipmaps", "Generating mipmaps with vkCmdBlitImage()", addBlittingImageAllFormatsMipmapTests, allocationKind, extensionUse);
}

void addBlittingImageTests (tcu::TestCaseGroup* group, AllocationKind allocationKind, ExtensionUse extensionUse)
{
	addTestGroup(group, "simple_tests", "Blitting image simple tests", addBlittingImageSimpleTests, allocationKind, extensionUse);
	addTestGroup(group, "all_formats", "Blitting image with all compatible formats", addBlittingImageAllFormatsTests, allocationKind, extensionUse);
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

void addResolveImageWholeTests (tcu::TestCaseGroup* group, AllocationKind allocationKind, ExtensionUse extensionUse)
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
	params.extensionUse					= extensionUse;

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

void addResolveImagePartialTests (tcu::TestCaseGroup* group, AllocationKind allocationKind, ExtensionUse extensionUse)
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
	params.extensionUse					= extensionUse;

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

void addResolveImageWithRegionsTests (tcu::TestCaseGroup* group, AllocationKind allocationKind, ExtensionUse extensionUse)
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
	params.extensionUse					= extensionUse;

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

void addResolveImageWholeCopyBeforeResolvingTests (tcu::TestCaseGroup* group, AllocationKind allocationKind, ExtensionUse extensionUse)
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
	params.extensionUse					= extensionUse;

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

void addResolveImageWholeArrayImageTests (tcu::TestCaseGroup* group, AllocationKind allocationKind, ExtensionUse extensionUse)
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
	params.extensionUse					= extensionUse;

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

void addResolveImageWholeArrayImageSingleRegionTests (tcu::TestCaseGroup* group, AllocationKind allocationKind, ExtensionUse extensionUse)
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
	params.extensionUse					= extensionUse;

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

void addResolveImageDiffImageSizeTests (tcu::TestCaseGroup* group, AllocationKind allocationKind, ExtensionUse extensionUse)
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
	params.extensionUse					= extensionUse;

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

void addResolveImageTests (tcu::TestCaseGroup* group, AllocationKind allocationKind, ExtensionUse extensionUse)
{
	addTestGroup(group, "whole", "Resolve from image to image (whole)", addResolveImageWholeTests, allocationKind, extensionUse);
	addTestGroup(group, "partial", "Resolve from image to image (partial)", addResolveImagePartialTests, allocationKind, extensionUse);
	addTestGroup(group, "with_regions", "Resolve from image to image (with regions)", addResolveImageWithRegionsTests, allocationKind, extensionUse);
	addTestGroup(group, "whole_copy_before_resolving", "Resolve from image to image (whole copy before resolving)", addResolveImageWholeCopyBeforeResolvingTests, allocationKind, extensionUse);
	addTestGroup(group, "whole_array_image", "Resolve from image to image (whole array image)", addResolveImageWholeArrayImageTests, allocationKind, extensionUse);
	addTestGroup(group, "whole_array_image_one_region", "Resolve from image to image (whole array image with single region)", addResolveImageWholeArrayImageSingleRegionTests, allocationKind, extensionUse);
	addTestGroup(group, "diff_image_size", "Resolve from image to image of different size", addResolveImageDiffImageSizeTests, allocationKind, extensionUse);
}

void addCopiesAndBlittingTests (tcu::TestCaseGroup* group, AllocationKind allocationKind, ExtensionUse extensionUse)
{
	addTestGroup(group, "image_to_image", "Copy from image to image", addImageToImageTests, allocationKind, extensionUse);
	addTestGroup(group, "image_to_buffer", "Copy from image to buffer", addImageToBufferTests, allocationKind, extensionUse);
	addTestGroup(group, "buffer_to_image", "Copy from buffer to image", addBufferToImageTests, allocationKind, extensionUse);
	addTestGroup(group, "buffer_to_depthstencil", "Copy from buffer to depth/Stencil", addBufferToDepthStencilTests, allocationKind, extensionUse);
	addTestGroup(group, "buffer_to_buffer", "Copy from buffer to buffer", addBufferToBufferTests, allocationKind, extensionUse);
	addTestGroup(group, "blit_image", "Blitting image", addBlittingImageTests, allocationKind, extensionUse);
	addTestGroup(group, "resolve_image", "Resolve image", addResolveImageTests, allocationKind, extensionUse);
}

void addCoreCopiesAndBlittingTests(tcu::TestCaseGroup* group)
{
	addCopiesAndBlittingTests(group, ALLOCATION_KIND_SUBALLOCATED, EXTENSION_USE_NONE);
}


void addDedicatedAllocationCopiesAndBlittingTests (tcu::TestCaseGroup* group)
{
	addCopiesAndBlittingTests(group, ALLOCATION_KIND_DEDICATED, EXTENSION_USE_NONE);
}

void addExtensionCopiesAndBlittingTests(tcu::TestCaseGroup* group)
{
	addCopiesAndBlittingTests(group, ALLOCATION_KIND_DEDICATED, EXTENSION_USE_COPY_COMMANDS2);
}

} // anonymous

tcu::TestCaseGroup* createCopiesAndBlittingTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	copiesAndBlittingTests(new tcu::TestCaseGroup(testCtx, "copy_and_blit", "Copies And Blitting Tests"));

	copiesAndBlittingTests->addChild(createTestGroup(testCtx, "core", "Core Copies And Blitting Tests", addCoreCopiesAndBlittingTests));
	copiesAndBlittingTests->addChild(createTestGroup(testCtx, "dedicated_allocation",	"Copies And Blitting Tests For Dedicated Memory Allocation",	addDedicatedAllocationCopiesAndBlittingTests));
	copiesAndBlittingTests->addChild(createTestGroup(testCtx, "copy_commands2", "Copies And Blitting Tests using KHR_copy_commands2", addExtensionCopiesAndBlittingTests));

	return copiesAndBlittingTests.release();
}

} // api
} // vkt
