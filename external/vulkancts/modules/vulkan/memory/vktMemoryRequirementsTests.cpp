/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \brief Buffer and image memory requirements tests.
 *//*--------------------------------------------------------------------*/

#include "vktMemoryRequirementsTests.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkStrUtil.hpp"
#include "vkTypeUtil.hpp"

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"

#include "tcuResultCollector.hpp"
#include "tcuTestLog.hpp"

namespace vkt
{
namespace memory
{
namespace
{
using namespace vk;
using de::MovePtr;

Move<VkBuffer> makeBuffer (const DeviceInterface& vk, const VkDevice device, const VkDeviceSize size, const VkBufferCreateFlags flags, const VkBufferUsageFlags usage)
{
	const VkBufferCreateInfo createInfo =
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType        sType;
		DE_NULL,									// const void*            pNext;
		flags,										// VkBufferCreateFlags    flags;
		size,										// VkDeviceSize           size;
		usage,										// VkBufferUsageFlags     usage;
		VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode          sharingMode;
		0u,											// uint32_t               queueFamilyIndexCount;
		DE_NULL,									// const uint32_t*        pQueueFamilyIndices;
	};
	return createBuffer(vk, device, &createInfo);
}

//! Get an index of each set bit, starting from the least significant bit.
std::vector<deUint32> bitsToIndices (deUint32 bits)
{
	std::vector<deUint32> indices;
	for (deUint32 i = 0u; bits != 0u; ++i, bits >>= 1)
	{
		if (bits & 1u)
			indices.push_back(i);
	}
	return indices;
}

VkMemoryRequirements getBufferMemoryRequirements (const DeviceInterface& vk, const VkDevice device, const VkDeviceSize size, const VkBufferCreateFlags flags, const VkBufferUsageFlags usage)
{
	const Unique<VkBuffer> buffer(makeBuffer(vk, device, size, flags, usage));
	return getBufferMemoryRequirements(vk, device, *buffer);
}

template<typename T>
T nextEnum (T value)
{
	return static_cast<T>(static_cast<deUint32>(value) + 1);
}

template<typename T>
T nextFlag (T value)
{
	if (value)
		return static_cast<T>(static_cast<deUint32>(value) << 1);
	else
		return static_cast<T>(1);
}

template<typename T>
T nextFlagExcluding (T value, T excludedFlags)
{
	deUint32 tmp = static_cast<deUint32>(value);
	while ((tmp = nextFlag(tmp)) & static_cast<deUint32>(excludedFlags));
	return static_cast<T>(tmp);
}

void requireBufferSparseFeatures (const InstanceInterface& vki, const VkPhysicalDevice physDevice, const VkBufferCreateFlags flags)
{
	const VkPhysicalDeviceFeatures features = getPhysicalDeviceFeatures(vki, physDevice);

	if ((flags & VK_BUFFER_CREATE_SPARSE_BINDING_BIT) && !features.sparseBinding)
		TCU_THROW(NotSupportedError, "Feature not supported: sparseBinding");

	if ((flags & VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT) && !features.sparseResidencyBuffer)
		TCU_THROW(NotSupportedError, "Feature not supported: sparseResidencyBuffer");

	if ((flags & VK_BUFFER_CREATE_SPARSE_ALIASED_BIT) && !features.sparseResidencyAliased)
		TCU_THROW(NotSupportedError, "Feature not supported: sparseResidencyAliased");
}

void verifyBufferRequirements (tcu::ResultCollector&					result,
							   const VkPhysicalDeviceMemoryProperties&	deviceMemoryProperties,
							   const VkMemoryRequirements&				requirements,
							   const VkMemoryRequirements&				allUsageFlagsRequirements,
							   const VkPhysicalDeviceLimits&			limits,
							   const VkBufferCreateFlags				bufferFlags,
							   const VkBufferUsageFlags					usage)
{
	if (result.check(requirements.memoryTypeBits != 0, "VkMemoryRequirements memoryTypeBits has no bits set"))
	{
		typedef std::vector<deUint32>::const_iterator	IndexIterator;
		const std::vector<deUint32>						usedMemoryTypeIndices			= bitsToIndices(requirements.memoryTypeBits);
		bool											deviceLocalMemoryFound			= false;
		bool											hostVisibleCoherentMemoryFound	= false;

		for (IndexIterator memoryTypeNdx = usedMemoryTypeIndices.begin(); memoryTypeNdx != usedMemoryTypeIndices.end(); ++memoryTypeNdx)
		{
			if (*memoryTypeNdx >= deviceMemoryProperties.memoryTypeCount)
			{
				result.fail("VkMemoryRequirements memoryTypeBits contains bits for non-existing memory types");
				continue;
			}

			const VkMemoryPropertyFlags	memoryPropertyFlags = deviceMemoryProperties.memoryTypes[*memoryTypeNdx].propertyFlags;

			if (memoryPropertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
				deviceLocalMemoryFound = true;

			if (memoryPropertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
				hostVisibleCoherentMemoryFound = true;

			result.check((memoryPropertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) == 0u,
				"Memory type includes VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT");
		}

		result.check(deIsPowerOfTwo64(static_cast<deUint64>(requirements.alignment)) == DE_TRUE,
			"VkMemoryRequirements alignment isn't power of two");

		if (usage & (VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT))
		{
			result.check(requirements.alignment >= limits.minTexelBufferOffsetAlignment,
				"VkMemoryRequirements alignment doesn't respect minTexelBufferOffsetAlignment");
		}

		if (usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
		{
			result.check(requirements.alignment >= limits.minUniformBufferOffsetAlignment,
				"VkMemoryRequirements alignment doesn't respect minUniformBufferOffsetAlignment");
		}

		if (usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
		{
			result.check(requirements.alignment >= limits.minStorageBufferOffsetAlignment,
				"VkMemoryRequirements alignment doesn't respect minStorageBufferOffsetAlignment");
		}

		result.check(deviceLocalMemoryFound,
			"None of the required memory types included VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT");

		result.check((bufferFlags & VK_BUFFER_CREATE_SPARSE_BINDING_BIT) || hostVisibleCoherentMemoryFound,
			"Required memory type doesn't include VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT and VK_MEMORY_PROPERTY_HOST_COHERENT_BIT");

		result.check((requirements.memoryTypeBits & allUsageFlagsRequirements.memoryTypeBits) == requirements.memoryTypeBits,
			"Memory type bits aren't a subset of memory type bits for all usage flags combined");
	}
}

tcu::TestStatus testBuffer (Context& context, const VkBufferCreateFlags bufferFlags)
{
	const DeviceInterface&					vk							= context.getDeviceInterface();
	const InstanceInterface&				vki							= context.getInstanceInterface();
	const VkDevice							device						= context.getDevice();
	const VkPhysicalDevice					physDevice					= context.getPhysicalDevice();

	requireBufferSparseFeatures(vki, physDevice, bufferFlags);

	const VkPhysicalDeviceMemoryProperties	memoryProperties			= getPhysicalDeviceMemoryProperties(vki, physDevice);
	const VkPhysicalDeviceLimits			limits						= getPhysicalDeviceProperties(vki, physDevice).limits;
	const VkBufferUsageFlags				allUsageFlags				= static_cast<VkBufferUsageFlags>((VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT << 1) - 1);
	const VkMemoryRequirements				allUsageFlagsRequirements	= getBufferMemoryRequirements(vk, device, 1024, bufferFlags, allUsageFlags); // doesn't depend on size
	tcu::TestLog&							log							= context.getTestContext().getLog();
	bool									allPass						= true;

	const VkDeviceSize sizeCases[] =
	{
		1	 * 1024,
		8    * 1024,
		64   * 1024,
		1024 * 1024,
	};

	for (VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT; usage <= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT; usage = nextFlag(usage))
	{
		deUint32		previousMemoryTypeBits	= 0u;
		VkDeviceSize	previousAlignment		= 0u;

		log << tcu::TestLog::Message << "Verify a buffer with usage flags: " << de::toString(getBufferUsageFlagsStr(usage)) << tcu::TestLog::EndMessage;

		for (const VkDeviceSize* pSize = sizeCases; pSize < sizeCases + DE_LENGTH_OF_ARRAY(sizeCases); ++pSize)
		{
			log << tcu::TestLog::Message << "- size " << *pSize << " bytes" << tcu::TestLog::EndMessage;

			const VkMemoryRequirements	requirements	= getBufferMemoryRequirements(vk, device, *pSize, bufferFlags, usage);
			tcu::ResultCollector		result			(log, "ERROR: ");

			// Check:
			// - requirements for a particular buffer usage
			// - memoryTypeBits are a subset of bits for requirements with all usage flags combined
			verifyBufferRequirements(result, memoryProperties, requirements, allUsageFlagsRequirements, limits, bufferFlags, usage);

			// Check that for the same usage and create flags:
			// - memoryTypeBits are the same
			// - alignment is the same
			if (pSize > sizeCases)
			{
				result.check(requirements.memoryTypeBits == previousMemoryTypeBits,
					"memoryTypeBits differ from the ones in the previous buffer size");

				result.check(requirements.alignment == previousAlignment,
					"alignment differs from the one in the previous buffer size");
			}

			if (result.getResult() != QP_TEST_RESULT_PASS)
				allPass = false;

			previousMemoryTypeBits	= requirements.memoryTypeBits;
			previousAlignment		= requirements.alignment;
		}

		if (!allPass)
			break;
	}

	return allPass ? tcu::TestStatus::pass("Pass") : tcu::TestStatus::fail("Some memory requirements were incorrect");
}

void requireImageSparseFeatures (const InstanceInterface& vki, const VkPhysicalDevice physDevice, const VkImageCreateFlags createFlags)
{
	const VkPhysicalDeviceFeatures features = getPhysicalDeviceFeatures(vki, physDevice);

	if ((createFlags & VK_IMAGE_CREATE_SPARSE_BINDING_BIT) && !features.sparseBinding)
		TCU_THROW(NotSupportedError, "Feature not supported: sparseBinding");

	if ((createFlags & VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT) && !(features.sparseResidencyImage2D || features.sparseResidencyImage3D))
		TCU_THROW(NotSupportedError, "Feature not supported: sparseResidencyImage (2D and 3D)");

	if ((createFlags & VK_IMAGE_CREATE_SPARSE_ALIASED_BIT) && !features.sparseResidencyAliased)
		TCU_THROW(NotSupportedError, "Feature not supported: sparseResidencyAliased");
}

bool imageUsageMatchesFormatFeatures (const VkImageUsageFlags usage, const VkFormatFeatureFlags featureFlags)
{
	if ((usage & VK_IMAGE_USAGE_SAMPLED_BIT) && (featureFlags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
		return true;
	if ((usage & VK_IMAGE_USAGE_STORAGE_BIT) && (featureFlags & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT))
		return true;
	if ((usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)) && (featureFlags & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT))
		return true;
	if ((usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) && (featureFlags & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
		return true;

	return false;
}

//! This catches both invalid as well as legal but unsupported combinations of image parameters
bool isImageSupported (const InstanceInterface& vki, const VkPhysicalDevice physDevice, const VkImageCreateInfo& info)
{
	DE_ASSERT(info.extent.width >= 1u && info.extent.height >= 1u && info.extent.depth >= 1u);

	if (info.imageType == VK_IMAGE_TYPE_1D)
	{
		DE_ASSERT(info.extent.height == 1u && info.extent.depth == 1u);
	}
	else if (info.imageType == VK_IMAGE_TYPE_2D)
	{
		DE_ASSERT(info.extent.depth == 1u);

		if (info.flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT)
		{
			DE_ASSERT(info.extent.width == info.extent.height);
			DE_ASSERT(info.arrayLayers >= 6u && (info.arrayLayers % 6u) == 0u);
		}
	}

	if ((info.flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) && info.imageType != VK_IMAGE_TYPE_2D)
		return false;

	if ((info.samples != VK_SAMPLE_COUNT_1_BIT) &&
		(info.imageType != VK_IMAGE_TYPE_2D || (info.flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) || info.tiling != VK_IMAGE_TILING_OPTIMAL || info.mipLevels > 1u))
		return false;

	if ((info.usage & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT) &&
		(info.usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)) == 0u)
		return false;

	const VkPhysicalDeviceFeatures features = getPhysicalDeviceFeatures(vki, physDevice);

	if (info.flags & VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT)
	{
		DE_ASSERT(info.tiling == VK_IMAGE_TILING_OPTIMAL);

		if (info.imageType == VK_IMAGE_TYPE_2D && !features.sparseResidencyImage2D)
			return false;
		if (info.imageType == VK_IMAGE_TYPE_3D && !features.sparseResidencyImage3D)
			return false;
		if (info.samples == VK_SAMPLE_COUNT_2_BIT && !features.sparseResidency2Samples)
			return false;
		if (info.samples == VK_SAMPLE_COUNT_4_BIT && !features.sparseResidency4Samples)
			return false;
		if (info.samples == VK_SAMPLE_COUNT_8_BIT && !features.sparseResidency8Samples)
			return false;
		if (info.samples == VK_SAMPLE_COUNT_16_BIT && !features.sparseResidency16Samples)
			return false;
		if (info.samples == VK_SAMPLE_COUNT_32_BIT || info.samples == VK_SAMPLE_COUNT_64_BIT)
			return false;
	}

	if (info.samples != VK_SAMPLE_COUNT_1_BIT && (info.usage & VK_IMAGE_USAGE_STORAGE_BIT) && !features.shaderStorageImageMultisample)
		return false;

	switch (info.format)
	{
		case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
		case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
		case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
		case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
		case VK_FORMAT_BC2_UNORM_BLOCK:
		case VK_FORMAT_BC2_SRGB_BLOCK:
		case VK_FORMAT_BC3_UNORM_BLOCK:
		case VK_FORMAT_BC3_SRGB_BLOCK:
		case VK_FORMAT_BC4_UNORM_BLOCK:
		case VK_FORMAT_BC4_SNORM_BLOCK:
		case VK_FORMAT_BC5_UNORM_BLOCK:
		case VK_FORMAT_BC5_SNORM_BLOCK:
		case VK_FORMAT_BC6H_UFLOAT_BLOCK:
		case VK_FORMAT_BC6H_SFLOAT_BLOCK:
		case VK_FORMAT_BC7_UNORM_BLOCK:
		case VK_FORMAT_BC7_SRGB_BLOCK:
			if (!features.textureCompressionBC)
				return false;
			break;

		case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
		case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
		case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
		case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
		case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
		case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
		case VK_FORMAT_EAC_R11_UNORM_BLOCK:
		case VK_FORMAT_EAC_R11_SNORM_BLOCK:
		case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
		case VK_FORMAT_EAC_R11G11_SNORM_BLOCK:
			if (!features.textureCompressionETC2)
				return false;
			break;

		case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
		case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
		case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:
		case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:
		case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
		case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
		case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:
		case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:
		case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
		case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
		case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:
		case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:
		case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:
		case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:
		case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
		case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
		case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:
		case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:
		case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:
		case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:
		case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:
		case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:
		case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:
		case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:
		case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:
		case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:
		case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:
		case VK_FORMAT_ASTC_12x12_SRGB_BLOCK:
			if (!features.textureCompressionASTC_LDR)
				return false;
			break;

		default:
			break;
	}

	const VkFormatProperties	formatProperties	= getPhysicalDeviceFormatProperties(vki, physDevice, info.format);
	const VkFormatFeatureFlags	formatFeatures		= (info.tiling == VK_IMAGE_TILING_LINEAR ? formatProperties.linearTilingFeatures
																							 : formatProperties.optimalTilingFeatures);

	if (!imageUsageMatchesFormatFeatures(info.usage, formatFeatures))
		return false;

	VkImageFormatProperties		imageFormatProperties;
	const VkResult				result				= vki.getPhysicalDeviceImageFormatProperties(
														physDevice, info.format, info.imageType, info.tiling, info.usage, info.flags, &imageFormatProperties);

	if (result == VK_SUCCESS)
	{
		if (info.arrayLayers > imageFormatProperties.maxArrayLayers)
			return false;
		if (info.mipLevels > imageFormatProperties.maxMipLevels)
			return false;
		if ((info.samples & imageFormatProperties.sampleCounts) == 0u)
			return false;
	}

	return result == VK_SUCCESS;
}

VkExtent3D makeExtentForImage (const VkImageType imageType)
{
	VkExtent3D extent = { 64u, 64u, 4u };

	if (imageType == VK_IMAGE_TYPE_1D)
		extent.height = extent.depth = 1u;
	else if (imageType == VK_IMAGE_TYPE_2D)
		extent.depth = 1u;

	return extent;
}

bool isFormatMatchingAspect (const VkFormat format, const VkImageAspectFlags aspect)
{
	DE_ASSERT(aspect == VK_IMAGE_ASPECT_COLOR_BIT || aspect == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT));

	// D/S formats are laid out next to each other in the enum
	const bool isDepthStencilFormat = (format >= VK_FORMAT_D16_UNORM && format <= VK_FORMAT_D32_SFLOAT_S8_UINT);

	return (aspect == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) == isDepthStencilFormat;
}

void verifyImageRequirements (tcu::ResultCollector&						result,
							  const VkPhysicalDeviceMemoryProperties&	deviceMemoryProperties,
							  const VkMemoryRequirements&				requirements,
							  const VkImageCreateInfo&					imageInfo)
{
	if (result.check(requirements.memoryTypeBits != 0, "VkMemoryRequirements memoryTypeBits has no bits set"))
	{
		typedef std::vector<deUint32>::const_iterator	IndexIterator;
		const std::vector<deUint32>						usedMemoryTypeIndices			= bitsToIndices(requirements.memoryTypeBits);
		bool											deviceLocalMemoryFound			= false;
		bool											hostVisibleCoherentMemoryFound	= false;

		for (IndexIterator memoryTypeNdx = usedMemoryTypeIndices.begin(); memoryTypeNdx != usedMemoryTypeIndices.end(); ++memoryTypeNdx)
		{
			if (*memoryTypeNdx >= deviceMemoryProperties.memoryTypeCount)
			{
				result.fail("VkMemoryRequirements memoryTypeBits contains bits for non-existing memory types");
				continue;
			}

			const VkMemoryPropertyFlags	memoryPropertyFlags = deviceMemoryProperties.memoryTypes[*memoryTypeNdx].propertyFlags;

			if (memoryPropertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
				deviceLocalMemoryFound = true;

			if (memoryPropertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
				hostVisibleCoherentMemoryFound = true;

			if (memoryPropertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT)
			{
				result.check((imageInfo.usage & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT) != 0u,
					"Memory type includes VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT for a non-transient attachment image");
			}
		}

		result.check(deIsPowerOfTwo64(static_cast<deUint64>(requirements.alignment)) == DE_TRUE,
			"VkMemoryRequirements alignment isn't power of two");

		result.check(deviceLocalMemoryFound,
			"None of the required memory types included VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT");

		result.check(imageInfo.tiling == VK_IMAGE_TILING_OPTIMAL || hostVisibleCoherentMemoryFound,
			"Required memory type doesn't include VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT and VK_MEMORY_PROPERTY_HOST_COHERENT_BIT");
	}
}

std::string getImageInfoString (const VkImageCreateInfo& imageInfo)
{
	std::ostringstream str;

	switch (imageInfo.imageType)
	{
		case VK_IMAGE_TYPE_1D:			str << "1D "; break;
		case VK_IMAGE_TYPE_2D:			str << "2D "; break;
		case VK_IMAGE_TYPE_3D:			str << "3D "; break;
		default:						break;
	}

	switch (imageInfo.tiling)
	{
		case VK_IMAGE_TILING_OPTIMAL:	str << "(optimal) "; break;
		case VK_IMAGE_TILING_LINEAR:	str << "(linear) "; break;
		default:						break;
	}

	str << "extent:[" << imageInfo.extent.width << ", " << imageInfo.extent.height << ", " << imageInfo.extent.depth << "] ";
	str << imageInfo.format << " ";
	str << "samples:" << static_cast<deUint32>(imageInfo.samples) << " ";
	str << "flags:" << static_cast<deUint32>(imageInfo.flags) << " ";
	str << "usage:" << static_cast<deUint32>(imageInfo.usage) << " ";

	return str.str();
}

struct ImageParams
{
	VkImageCreateFlags		flags;
	VkImageTiling			tiling;
	bool					transient;
};

tcu::TestStatus testImage (Context& context, const ImageParams params)
{
	const DeviceInterface&		vk				= context.getDeviceInterface();
	const InstanceInterface&	vki				= context.getInstanceInterface();
	const VkDevice				device			= context.getDevice();
	const VkPhysicalDevice		physDevice		= context.getPhysicalDevice();
	const VkImageCreateFlags	sparseFlags		= VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT | VK_IMAGE_CREATE_SPARSE_ALIASED_BIT;
	const VkImageUsageFlags		transientFlags	= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;

	requireImageSparseFeatures(vki, physDevice, params.flags);

	const VkPhysicalDeviceMemoryProperties	memoryProperties		= getPhysicalDeviceMemoryProperties(vki, physDevice);
	const deUint32							notInitializedBits		= ~0u;
	const VkImageAspectFlags				colorAspect				= VK_IMAGE_ASPECT_COLOR_BIT;
	const VkImageAspectFlags				depthStencilAspect		= VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	const VkImageAspectFlags				allAspects[2]			= { colorAspect, depthStencilAspect };
	tcu::TestLog&							log						= context.getTestContext().getLog();
	bool									allPass					= true;
	deUint32								numCheckedImages		= 0u;

	log << tcu::TestLog::Message << "Verify memory requirements for the following parameter combinations:" << tcu::TestLog::EndMessage;

	for (deUint32 loopAspectNdx = 0u; loopAspectNdx < DE_LENGTH_OF_ARRAY(allAspects); ++loopAspectNdx)
	{
		const VkImageAspectFlags	aspect					= allAspects[loopAspectNdx];
		deUint32					previousMemoryTypeBits	= notInitializedBits;

		for (VkFormat loopFormat = VK_FORMAT_R4G4_UNORM_PACK8; loopFormat <= VK_FORMAT_ASTC_12x12_SRGB_BLOCK; loopFormat = nextEnum(loopFormat))
		if  (isFormatMatchingAspect(loopFormat, aspect))
		{
			// memoryTypeBits may differ between depth/stencil formats
			if (aspect == depthStencilAspect)
				previousMemoryTypeBits = notInitializedBits;

			for (VkImageType			loopImageType	= VK_IMAGE_TYPE_1D;					loopImageType	!= VK_IMAGE_TYPE_LAST;					loopImageType	= nextEnum(loopImageType))
			for (VkImageCreateFlags		loopCreateFlags	= (VkImageCreateFlags)0;			loopCreateFlags	<= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;	loopCreateFlags	= nextFlagExcluding(loopCreateFlags, sparseFlags))
			for (VkImageUsageFlags		loopUsageFlags	= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;	loopUsageFlags	<= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;	loopUsageFlags	= nextFlagExcluding(loopUsageFlags, transientFlags))
			for (VkSampleCountFlagBits	loopSampleCount	= VK_SAMPLE_COUNT_1_BIT;			loopSampleCount	<= VK_SAMPLE_COUNT_16_BIT;				loopSampleCount	= nextFlag(loopSampleCount))
			{
				const VkImageCreateFlags	actualCreateFlags	= loopCreateFlags | params.flags;
				const VkImageUsageFlags		actualUsageFlags	= loopUsageFlags  | (params.transient ? VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT : (VkImageUsageFlagBits)0);
				const bool					isCube				= (actualCreateFlags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) != 0u;
				const VkImageCreateInfo		imageInfo			=
				{
					VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,		// VkStructureType          sType;
					DE_NULL,									// const void*              pNext;
					actualCreateFlags,							// VkImageCreateFlags       flags;
					loopImageType,								// VkImageType              imageType;
					loopFormat,									// VkFormat                 format;
					makeExtentForImage(loopImageType),			// VkExtent3D               extent;
					1u,											// uint32_t                 mipLevels;
					(isCube ? 6u : 1u),							// uint32_t                 arrayLayers;
					loopSampleCount,							// VkSampleCountFlagBits    samples;
					params.tiling,								// VkImageTiling            tiling;
					actualUsageFlags,							// VkImageUsageFlags        usage;
					VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode            sharingMode;
					0u,											// uint32_t                 queueFamilyIndexCount;
					DE_NULL,									// const uint32_t*          pQueueFamilyIndices;
					VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout            initialLayout;
				};

				if (!isImageSupported(vki, physDevice, imageInfo))
					continue;

				log << tcu::TestLog::Message << "- " << getImageInfoString(imageInfo) << tcu::TestLog::EndMessage;
				++numCheckedImages;

				const Unique<VkImage>		image			(createImage(vk, device, &imageInfo));
				const VkMemoryRequirements	requirements	= getImageMemoryRequirements(vk, device, *image);
				tcu::ResultCollector		result			(log, "ERROR: ");

				verifyImageRequirements(result, memoryProperties, requirements, imageInfo);

				// For the same tiling, transient usage, and sparse flags, (and format, if D/S) memoryTypeBits must be the same for all images
				result.check((previousMemoryTypeBits == notInitializedBits) || (requirements.memoryTypeBits == previousMemoryTypeBits),
								"memoryTypeBits differ from the ones in the previous image configuration");

				if (result.getResult() != QP_TEST_RESULT_PASS)
					allPass = false;

				previousMemoryTypeBits = requirements.memoryTypeBits;
			}
		}
	}

	if (numCheckedImages == 0u)
		log << tcu::TestLog::Message << "NOTE: No supported image configurations -- nothing to check" << tcu::TestLog::EndMessage;

	return allPass ? tcu::TestStatus::pass("Pass") : tcu::TestStatus::fail("Some memory requirements were incorrect");
}

void populateTestGroup (tcu::TestCaseGroup* group)
{
	// Buffers
	{
		const struct
		{
			VkBufferCreateFlags		flags;
			const char* const		name;
		} bufferCases[] =
		{
			{ (VkBufferCreateFlags)0,																								"regular"					},
			{ VK_BUFFER_CREATE_SPARSE_BINDING_BIT,																					"sparse"					},
			{ VK_BUFFER_CREATE_SPARSE_BINDING_BIT | VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT,											"sparse_residency"			},
			{ VK_BUFFER_CREATE_SPARSE_BINDING_BIT											| VK_BUFFER_CREATE_SPARSE_ALIASED_BIT,	"sparse_aliased"			},
			{ VK_BUFFER_CREATE_SPARSE_BINDING_BIT | VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT	| VK_BUFFER_CREATE_SPARSE_ALIASED_BIT,	"sparse_residency_aliased"	},
		};

		de::MovePtr<tcu::TestCaseGroup> bufferGroup(new tcu::TestCaseGroup(group->getTestContext(), "buffer", ""));

		for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(bufferCases); ++ndx)
			addFunctionCase(bufferGroup.get(), bufferCases[ndx].name, "", testBuffer, bufferCases[ndx].flags);

		group->addChild(bufferGroup.release());
	}

	// Images
	{
		const struct
		{
			VkImageCreateFlags		flags;
			bool					transient;
			const char* const		name;
		} imageFlagsCases[] =
		{
			{ (VkImageCreateFlags)0,																								false,	"regular"					},
			{ (VkImageCreateFlags)0,																								true,	"transient"					},
			{ VK_IMAGE_CREATE_SPARSE_BINDING_BIT,																					false,	"sparse"					},
			{ VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT,											false,	"sparse_residency"			},
			{ VK_IMAGE_CREATE_SPARSE_BINDING_BIT											| VK_IMAGE_CREATE_SPARSE_ALIASED_BIT,	false,	"sparse_aliased"			},
			{ VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT		| VK_IMAGE_CREATE_SPARSE_ALIASED_BIT,	false,	"sparse_residency_aliased"	},
		};

		de::MovePtr<tcu::TestCaseGroup> imageGroup(new tcu::TestCaseGroup(group->getTestContext(), "image", ""));

		for (int flagsNdx = 0; flagsNdx < DE_LENGTH_OF_ARRAY(imageFlagsCases); ++flagsNdx)
		for (int tilingNdx = 0; tilingNdx <= 1; ++tilingNdx)
		{
			ImageParams			params;
			std::ostringstream	caseName;

			params.flags		=  imageFlagsCases[flagsNdx].flags;
			params.transient	=  imageFlagsCases[flagsNdx].transient;
			caseName			<< imageFlagsCases[flagsNdx].name;

			if (tilingNdx != 0)
			{
				params.tiling =  VK_IMAGE_TILING_OPTIMAL;
				caseName      << "_tiling_optimal";
			}
			else
			{
				params.tiling =  VK_IMAGE_TILING_LINEAR;
				caseName      << "_tiling_linear";
			}

			if ((params.flags & VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT) && (params.tiling == VK_IMAGE_TILING_LINEAR))
				continue;

			addFunctionCase(imageGroup.get(), caseName.str(), "", testImage, params);
		}

		group->addChild(imageGroup.release());
	}
}

} // anonymous

tcu::TestCaseGroup* createRequirementsTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "requirements", "Buffer and image memory requirements", populateTestGroup);
}

} // memory
} // vkt
