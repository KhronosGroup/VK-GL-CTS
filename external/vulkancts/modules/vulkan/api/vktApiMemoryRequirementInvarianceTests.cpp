/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 Google Inc.
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
 *//*--------------------------------------------------------------------*/

#include "vktApiMemoryRequirementInvarianceTests.hpp"
#include "vktApiBufferAndImageAllocationUtil.hpp"
#include "deRandom.h"
#include "tcuTestLog.hpp"
#include "vkQueryUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkImageUtil.hpp"


namespace vkt
{
namespace api
{

using namespace vk;

// Number of items to allocate
const unsigned int		testCycles								= 1000u;

// All legal memory combinations (spec chapter 10.2: Device Memory)
const unsigned int		legalMemoryTypeCount					= 11u;
const MemoryRequirement	legalMemoryTypes[legalMemoryTypeCount]	=
{
	MemoryRequirement::Any,
	MemoryRequirement::HostVisible	| MemoryRequirement::Coherent,
	MemoryRequirement::HostVisible	| MemoryRequirement::Cached,
	MemoryRequirement::HostVisible	| MemoryRequirement::Cached			| MemoryRequirement::Coherent,
	MemoryRequirement::Local,
	MemoryRequirement::Local		| MemoryRequirement::HostVisible	| MemoryRequirement::Coherent,
	MemoryRequirement::Local		| MemoryRequirement::HostVisible	| MemoryRequirement::Cached,
	MemoryRequirement::Local		| MemoryRequirement::HostVisible	| MemoryRequirement::Cached		| MemoryRequirement::Coherent,
	MemoryRequirement::Local		| MemoryRequirement::LazilyAllocated,
	MemoryRequirement::Protected,
	MemoryRequirement::Protected	| MemoryRequirement::Local
};

class IObjectAllocator
{
public:
					IObjectAllocator	()	{};
	virtual			~IObjectAllocator	()	{};
	virtual void	allocate			(Context&	context)	= 0;
	virtual void	deallocate			(Context&	context)	= 0;
	virtual	size_t	getSize				(Context&	context)	= 0;
};

class BufferAllocator : public IObjectAllocator
{
public:
					BufferAllocator		(deRandom& random, deBool dedicated, std::vector<int>& memoryTypes);
	virtual			~BufferAllocator	();
	virtual void	allocate			(Context&	context);
	virtual void	deallocate			(Context&	context);
	virtual	size_t	getSize				(Context&	context);
private:
	bool					m_dedicated;
	Move<VkBuffer>			m_buffer;
	VkDeviceSize			m_size;
	VkBufferUsageFlags		m_usage;
	int						m_memoryType;
	de::MovePtr<Allocation>	m_bufferAlloc;
};

BufferAllocator::BufferAllocator (deRandom& random, deBool dedicated, std::vector<int>& memoryTypes)
{
	// If dedicated allocation is supported, randomly pick it
	m_dedicated		= dedicated && deRandom_getBool(&random);
	// Random buffer sizes to find potential issues caused by strange alignment
	m_size			= (deRandom_getUint32(&random) % 1024) + 7;
	// Pick a random usage from the 9 VkBufferUsageFlags.
	m_usage			= 1 << (deRandom_getUint32(&random) % 9);
	// Pick random memory type from the supported ones
	m_memoryType	= memoryTypes[deRandom_getUint32(&random) % memoryTypes.size()];
}

BufferAllocator::~BufferAllocator ()
{
}

void BufferAllocator::allocate (Context& context)
{
	Allocator&						memAlloc	= context.getDefaultAllocator();
	de::MovePtr<IBufferAllocator>	allocator;
	MemoryRequirement				requirement	= legalMemoryTypes[m_memoryType];

	if (m_dedicated)
		allocator = de::MovePtr<IBufferAllocator>(new BufferDedicatedAllocation);
	else
		allocator = de::MovePtr<IBufferAllocator>(new BufferSuballocation);

	allocator->createTestBuffer(
		m_size,
		m_usage,
		context,
		memAlloc,
		m_buffer,
		requirement,
		m_bufferAlloc);
}

void BufferAllocator::deallocate (Context& context)
{
	const DeviceInterface&	vk		= context.getDeviceInterface();
	const vk::VkDevice&		device	= context.getDevice();

	vk.destroyBuffer(device, m_buffer.disown(), DE_NULL);
	m_bufferAlloc.clear();
}

size_t BufferAllocator::getSize (Context &context)
{
	const DeviceInterface&	vk		= context.getDeviceInterface();
	const vk::VkDevice&		device	= context.getDevice();
	VkMemoryRequirements	memReq;

	vk.getBufferMemoryRequirements(device, *m_buffer, &memReq);

	return (size_t)memReq.size;
}

class ImageAllocator : public IObjectAllocator
{
public:
					ImageAllocator	(deRandom& random, deBool dedicated, std::vector<int>& linearformats, std::vector<int>& optimalformats, std::vector<int>& memoryTypes);
	virtual			~ImageAllocator	();
	virtual void	allocate		(Context&	context);
	virtual void	deallocate		(Context&	context);
	virtual	size_t	getSize			(Context&	context);
private:
	deBool					m_dedicated;
	deBool					m_linear;
	Move<vk::VkImage>		m_image;
	tcu::IVec2				m_size;
	vk::VkFormat			m_colorFormat;
	de::MovePtr<Allocation>	m_imageAlloc;
	int						m_memoryType;
};

ImageAllocator::ImageAllocator (deRandom& random, deBool dedicated, std::vector<int>& linearformats, std::vector<int>& optimalformats, std::vector<int>& memoryTypes)
{
	// If dedicated allocation is supported, pick it randomly
	m_dedicated		= dedicated && deRandom_getBool(&random);
	// If linear formats are supported, pick it randomly
	m_linear		= (linearformats.size() > 0) && deRandom_getBool(&random);

	if (m_linear)
		m_colorFormat = (VkFormat)linearformats[deRandom_getUint32(&random) % linearformats.size()];
	else
		m_colorFormat = (VkFormat)optimalformats[deRandom_getUint32(&random) % optimalformats.size()];

	int	widthAlignment	= (isYCbCr420Format(m_colorFormat) || isYCbCr422Format(m_colorFormat)) ? 2 : 1;
	int	heightAlignment	= isYCbCr420Format(m_colorFormat) ? 2 : 1;

	// Random small size for causing potential alignment issues
	m_size			= tcu::IVec2((deRandom_getUint32(&random) % 16 + 3) & ~(widthAlignment - 1),
								 (deRandom_getUint32(&random) % 16 + 3) & ~(heightAlignment - 1));
	// Pick random memory type from the supported set
	m_memoryType	= memoryTypes[deRandom_getUint32(&random) % memoryTypes.size()];
}

ImageAllocator::~ImageAllocator ()
{
}

void ImageAllocator::allocate (Context& context)
{
	Allocator&						memAlloc	= context.getDefaultAllocator();
	de::MovePtr<IImageAllocator>	allocator;
	MemoryRequirement				requirement	= legalMemoryTypes[m_memoryType];

	if (m_dedicated)
		allocator = de::MovePtr<IImageAllocator>(new ImageDedicatedAllocation);
	else
		allocator = de::MovePtr<IImageAllocator>(new ImageSuballocation);

	allocator->createTestImage(
		m_size,
		m_colorFormat,
		context,
		memAlloc,
		m_image,
		requirement,
		m_imageAlloc,
		m_linear ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL);
}

void ImageAllocator::deallocate (Context& context)
{
	const DeviceInterface&	vk		= context.getDeviceInterface();
	const VkDevice&			device	= context.getDevice();

	vk.destroyImage(device, m_image.disown(), DE_NULL);
	m_imageAlloc.clear();
}

size_t ImageAllocator::getSize (Context &context)
{
	const DeviceInterface&	vk		= context.getDeviceInterface();
	const VkDevice&			device	= context.getDevice();
	VkMemoryRequirements	memReq;

	vk.getImageMemoryRequirements(device, *m_image, &memReq);

	return (size_t)memReq.size;
}

class InvarianceInstance : public vkt::TestInstance
{
public:
							InvarianceInstance			(Context&		context,
														 const deUint32	seed);
	virtual					~InvarianceInstance			(void);
	virtual	tcu::TestStatus	iterate						(void);
private:
	deRandom	m_random;
};

InvarianceInstance::InvarianceInstance	(Context&		context,
										 const deUint32	seed)
	: vkt::TestInstance	(context)
{
	deRandom_init(&m_random, seed);
}

InvarianceInstance::~InvarianceInstance (void)
{
}

tcu::TestStatus InvarianceInstance::iterate (void)
{
	de::MovePtr<IObjectAllocator>			objs[testCycles];
	size_t									refSizes[testCycles];
	unsigned int							order[testCycles];
	bool									success							= true;
	const deBool							isDedicatedAllocationSupported	= m_context.isDeviceFunctionalitySupported("VK_KHR_dedicated_allocation");
	const deBool							isYcbcrSupported				= m_context.isDeviceFunctionalitySupported("VK_KHR_sampler_ycbcr_conversion");
	std::vector<int>						optimalFormats;
	std::vector<int>						linearFormats;
	std::vector<int>						memoryTypes;
	vk::VkPhysicalDeviceMemoryProperties	memProperties;

	// List of all VkFormat enums
	const unsigned int						formatlist[]					= {
		VK_FORMAT_UNDEFINED,
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
		VK_FORMAT_R8_UINT,
		VK_FORMAT_R8_SINT,
		VK_FORMAT_R8_SRGB,
		VK_FORMAT_R8G8_UNORM,
		VK_FORMAT_R8G8_SNORM,
		VK_FORMAT_R8G8_USCALED,
		VK_FORMAT_R8G8_SSCALED,
		VK_FORMAT_R8G8_UINT,
		VK_FORMAT_R8G8_SINT,
		VK_FORMAT_R8G8_SRGB,
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
		VK_FORMAT_R16_UNORM,
		VK_FORMAT_R16_SNORM,
		VK_FORMAT_R16_USCALED,
		VK_FORMAT_R16_SSCALED,
		VK_FORMAT_R16_UINT,
		VK_FORMAT_R16_SINT,
		VK_FORMAT_R16_SFLOAT,
		VK_FORMAT_R16G16_UNORM,
		VK_FORMAT_R16G16_SNORM,
		VK_FORMAT_R16G16_USCALED,
		VK_FORMAT_R16G16_SSCALED,
		VK_FORMAT_R16G16_UINT,
		VK_FORMAT_R16G16_SINT,
		VK_FORMAT_R16G16_SFLOAT,
		VK_FORMAT_R16G16B16_UNORM,
		VK_FORMAT_R16G16B16_SNORM,
		VK_FORMAT_R16G16B16_USCALED,
		VK_FORMAT_R16G16B16_SSCALED,
		VK_FORMAT_R16G16B16_UINT,
		VK_FORMAT_R16G16B16_SINT,
		VK_FORMAT_R16G16B16_SFLOAT,
		VK_FORMAT_R16G16B16A16_UNORM,
		VK_FORMAT_R16G16B16A16_SNORM,
		VK_FORMAT_R16G16B16A16_USCALED,
		VK_FORMAT_R16G16B16A16_SSCALED,
		VK_FORMAT_R16G16B16A16_UINT,
		VK_FORMAT_R16G16B16A16_SINT,
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_FORMAT_R32_UINT,
		VK_FORMAT_R32_SINT,
		VK_FORMAT_R32_SFLOAT,
		VK_FORMAT_R32G32_UINT,
		VK_FORMAT_R32G32_SINT,
		VK_FORMAT_R32G32_SFLOAT,
		VK_FORMAT_R32G32B32_UINT,
		VK_FORMAT_R32G32B32_SINT,
		VK_FORMAT_R32G32B32_SFLOAT,
		VK_FORMAT_R32G32B32A32_UINT,
		VK_FORMAT_R32G32B32A32_SINT,
		VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_FORMAT_R64_UINT,
		VK_FORMAT_R64_SINT,
		VK_FORMAT_R64_SFLOAT,
		VK_FORMAT_R64G64_UINT,
		VK_FORMAT_R64G64_SINT,
		VK_FORMAT_R64G64_SFLOAT,
		VK_FORMAT_R64G64B64_UINT,
		VK_FORMAT_R64G64B64_SINT,
		VK_FORMAT_R64G64B64_SFLOAT,
		VK_FORMAT_R64G64B64A64_UINT,
		VK_FORMAT_R64G64B64A64_SINT,
		VK_FORMAT_R64G64B64A64_SFLOAT,
		VK_FORMAT_B10G11R11_UFLOAT_PACK32,
		VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,
		VK_FORMAT_D16_UNORM,
		VK_FORMAT_X8_D24_UNORM_PACK32,
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_S8_UINT,
		VK_FORMAT_D16_UNORM_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D32_SFLOAT_S8_UINT,
		VK_FORMAT_BC1_RGB_UNORM_BLOCK,
		VK_FORMAT_BC1_RGB_SRGB_BLOCK,
		VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
		VK_FORMAT_BC1_RGBA_SRGB_BLOCK,
		VK_FORMAT_BC2_UNORM_BLOCK,
		VK_FORMAT_BC2_SRGB_BLOCK,
		VK_FORMAT_BC3_UNORM_BLOCK,
		VK_FORMAT_BC3_SRGB_BLOCK,
		VK_FORMAT_BC4_UNORM_BLOCK,
		VK_FORMAT_BC4_SNORM_BLOCK,
		VK_FORMAT_BC5_UNORM_BLOCK,
		VK_FORMAT_BC5_SNORM_BLOCK,
		VK_FORMAT_BC6H_UFLOAT_BLOCK,
		VK_FORMAT_BC6H_SFLOAT_BLOCK,
		VK_FORMAT_BC7_UNORM_BLOCK,
		VK_FORMAT_BC7_SRGB_BLOCK,
		VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK,
		VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK,
		VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK,
		VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK,
		VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK,
		VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK,
		VK_FORMAT_EAC_R11_UNORM_BLOCK,
		VK_FORMAT_EAC_R11_SNORM_BLOCK,
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
		VK_FORMAT_G8B8G8R8_422_UNORM,
		VK_FORMAT_B8G8R8G8_422_UNORM,
		VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM,
		VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
		VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM,
		VK_FORMAT_G8_B8R8_2PLANE_422_UNORM,
		VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM,
		VK_FORMAT_R10X6_UNORM_PACK16,
		VK_FORMAT_R10X6G10X6_UNORM_2PACK16,
		VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16,
		VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16,
		VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16,
		VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16,
		VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16,
		VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16,
		VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16,
		VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16,
		VK_FORMAT_R12X4_UNORM_PACK16,
		VK_FORMAT_R12X4G12X4_UNORM_2PACK16,
		VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16,
		VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16,
		VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16,
		VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16,
		VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16,
		VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16,
		VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16,
		VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16,
		VK_FORMAT_G16B16G16R16_422_UNORM,
		VK_FORMAT_B16G16R16G16_422_UNORM,
		VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM,
		VK_FORMAT_G16_B16R16_2PLANE_420_UNORM,
		VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM,
		VK_FORMAT_G16_B16R16_2PLANE_422_UNORM,
		VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM,
		VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG,
		VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG,
		VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG,
		VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG,
		VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG,
		VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG,
		VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG,
		VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG
	};
	int										formatCount						= (int)(sizeof(formatlist) / sizeof(unsigned int));

	// If ycbcr is not supported, only use the standard texture formats
	if (!isYcbcrSupported)
		formatCount = 184;

	// Find supported image formats
	for (int i = 0; i < formatCount; i++)
	{
		vk::VkImageFormatProperties imageformatprops;

		// Check for support in linear tiling mode
		if (m_context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
			m_context.getPhysicalDevice(),
			(VkFormat)formatlist[i],
			VK_IMAGE_TYPE_2D,
			VK_IMAGE_TILING_LINEAR,
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			0,
			&imageformatprops) == VK_SUCCESS)
			linearFormats.push_back(formatlist[i]);

		// Check for support in optimal tiling mode
		if (m_context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
			m_context.getPhysicalDevice(),
			(VkFormat)formatlist[i],
			VK_IMAGE_TYPE_2D,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
			0,
			&imageformatprops) == VK_SUCCESS)
			optimalFormats.push_back(formatlist[i]);
	}

	// Check for supported heap types
	m_context.getInstanceInterface().getPhysicalDeviceMemoryProperties(m_context.getPhysicalDevice(), &memProperties);

	for (unsigned int j = 0; j < legalMemoryTypeCount; j++)
	{
		bool found = false;
		for (unsigned int i = 0; !found && i < memProperties.memoryTypeCount; i++)
		{
			if (legalMemoryTypes[j].matchesHeap(memProperties.memoryTypes[i].propertyFlags))
			{
				memoryTypes.push_back(j);
				found = true;
			}
		}
	}

	// Log the used image types and heap types
	tcu::TestLog& log = m_context.getTestContext().getLog();

	{
		std::ostringstream values;
		for (unsigned int i = 0; i < linearFormats.size(); i++)
			values << " " << linearFormats[i];
		log << tcu::TestLog::Message << "Using linear formats:" << values.str() << tcu::TestLog::EndMessage;
	}

	{
		std::ostringstream values;
		for (unsigned int i = 0; i < optimalFormats.size(); i++)
			values << " " << optimalFormats[i];
		log << tcu::TestLog::Message << "Using optimal formats:" << values.str() << tcu::TestLog::EndMessage;
	}

	{
		std::ostringstream values;
		for (unsigned int i = 0; i < memoryTypes.size(); i++)
			values << " " << memoryTypes[i];
		log << tcu::TestLog::Message << "Using memory types:" << values.str() << tcu::TestLog::EndMessage;
	}

	for (unsigned int i = 0; i < testCycles; i++)
	{
		if (deRandom_getBool(&m_random))
			objs[i] = de::MovePtr<IObjectAllocator>(new BufferAllocator(m_random, isDedicatedAllocationSupported, memoryTypes));
		else
			objs[i] = de::MovePtr<IObjectAllocator>(new ImageAllocator(m_random, isDedicatedAllocationSupported, linearFormats, optimalFormats, memoryTypes));
		order[i] = i;
	}

	// First get reference values for the object sizes
	for (unsigned int i = 0; i < testCycles; i++)
	{
		objs[i]->allocate(m_context);
		refSizes[i] = objs[i]->getSize(m_context);
		objs[i]->deallocate(m_context);
	}

	// Shuffle order by swapping random pairs
	for (unsigned int i = 0; i < testCycles; i++)
	{
		int a = deRandom_getUint32(&m_random) % testCycles;
		int b = deRandom_getUint32(&m_random) % testCycles;
		DE_SWAP(int, order[a], order[b]);
	}

	// Allocate objects in shuffled order
	for (unsigned int i = 0; i < testCycles; i++)
		objs[order[i]]->allocate(m_context);

	// Check for size mismatches
	for (unsigned int i = 0; i < testCycles; i++)
	{
		size_t val = objs[order[i]]->getSize(m_context);

		if (val != refSizes[order[i]])
		{
			success = false;
			log	<< tcu::TestLog::Message
				<< "Object "
				<< order[i]
				<< " size mismatch ("
				<< val
				<< " != "
				<< refSizes[order[i]]
				<< ")"
				<< tcu::TestLog::EndMessage;
		}
	}

	// Clean up
	for (unsigned int i = 0; i < testCycles; i++)
		objs[order[i]]->deallocate(m_context);

	if (success)
		return tcu::TestStatus::pass("Pass");

	return tcu::TestStatus::fail("One or more allocation is not invariant");
}

class InvarianceCase : public vkt::TestCase
{
public:
							InvarianceCase	(tcu::TestContext&	testCtx,
											 const std::string&	name,
											 const std::string&	description);
	virtual					~InvarianceCase	(void);

	virtual TestInstance*	createInstance	(Context&			context) const;
};

InvarianceCase::InvarianceCase	(tcu::TestContext&	testCtx,
								 const std::string&	name,
								 const std::string&	description)
	: vkt::TestCase(testCtx, name, description)
{
}

InvarianceCase::~InvarianceCase()
{
}

TestInstance* InvarianceCase::createInstance (Context& context) const
{
	return new InvarianceInstance(context, 0x600613);
}

tcu::TestCaseGroup* createMemoryRequirementInvarianceTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> invarianceTests(new tcu::TestCaseGroup(testCtx, "invariance", "Memory requirement invariance tests"));

	// Only one child, leaving room for potential targeted cases later on.
	invarianceTests->addChild(new InvarianceCase(testCtx, "random", std::string("Random case")));

	return invarianceTests.release();
}

} // api
} // vkt
