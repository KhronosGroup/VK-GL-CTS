/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
 * Copyright (c) 2021 Valve Corporation.
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
 * \brief Tests for VK_VALVE_mutable_descriptor_type.
 *//*--------------------------------------------------------------------*/
#include "vktBindingValveMutableTests.hpp"
#include "vktTestCase.hpp"

#include "vkDefs.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkRayTracingUtil.hpp"

#include "deUniquePtr.hpp"
#include "deSTLUtil.hpp"
#include "deStringUtil.hpp"

#include <vector>
#include <algorithm>
#include <iterator>
#include <set>
#include <sstream>
#include <limits>

namespace vkt
{
namespace BindingModel
{

namespace
{

using namespace vk;

deUint32 getDescriptorNumericValue (deUint32 iteration, deUint32 bindingIdx, deUint32 descriptorIdx = 0u)
{
	// When assigning numeric values for the descriptor contents, each descriptor will get 0x5aIIBBDD. II is an octed containing the
	// iteration index. BB is an octet containing the binding index and DD is the descriptor index inside that binding.
	constexpr deUint32 kNumericValueBase = 0x5a000000u;

	return (kNumericValueBase | ((iteration & 0xFFu) << 16) | ((bindingIdx & 0xFFu) << 8) | (descriptorIdx & 0xFFu));
}

deUint16 getAccelerationStructureOffsetX (deUint32 descriptorNumericValue)
{
	// Keep the lowest 16 bits (binding and descriptor idx) as the offset.
	return static_cast<deUint16>(descriptorNumericValue);
}

// Value that will be stored in the output buffer to signal success reading values.
deUint32 getExpectedOutputBufferValue ()
{
	return 2u;
}

// This value will be stored in an image to be sampled when checking descriptors containing samplers alone.
deUint32 getExternalSampledImageValue ()
{
	return 0x41322314u;
}

// Value that will be ORed with the descriptor value before writing.
deUint32 getStoredValueMask ()
{
	return 0xFF000000u;
}

VkFormat getDescriptorImageFormat ()
{
	return VK_FORMAT_R32_UINT;
}

VkExtent3D getDefaultExtent ()
{
	return makeExtent3D(1u, 1u, 1u);
}

// Convert value to hexadecimal.
std::string toHex (deUint32 val)
{
	std::ostringstream s;
	s << "0x" << std::hex << val << "u";
	return s.str();
}

// Returns the list of descriptor types that cannot be part of a mutable descriptor.
std::vector<VkDescriptorType> getForbiddenMutableTypes ()
{
	return std::vector<VkDescriptorType>
		{
			VK_DESCRIPTOR_TYPE_MUTABLE_VALVE,
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
			VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT,
		};
}

// Returns the list of descriptor types that are mandatory for the extension.
std::vector<VkDescriptorType> getMandatoryMutableTypes ()
{
	return std::vector<VkDescriptorType>
		{
			VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
			VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
			VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
		};
}

// This helps quickly transform a vector of descriptor types into a bitmask, which makes it easier to check some conditions.
enum DescriptorTypeFlagBits
{
	DTFB_SAMPLER                    = (1 << 0),
	DTFB_COMBINED_IMAGE_SAMPLER     = (1 << 1),
	DTFB_SAMPLED_IMAGE              = (1 << 2),
	DTFB_STORAGE_IMAGE              = (1 << 3),
	DTFB_UNIFORM_TEXEL_BUFFER       = (1 << 4),
	DTFB_STORAGE_TEXEL_BUFFER       = (1 << 5),
	DTFB_UNIFORM_BUFFER             = (1 << 6),
	DTFB_STORAGE_BUFFER             = (1 << 7),
	DTFB_UNIFORM_BUFFER_DYNAMIC     = (1 << 8),
	DTFB_STORAGE_BUFFER_DYNAMIC     = (1 << 9),
	DTFB_INPUT_ATTACHMENT           = (1 << 10),
	DTFB_INLINE_UNIFORM_BLOCK_EXT   = (1 << 11),
	DTFB_ACCELERATION_STRUCTURE_KHR = (1 << 12),
	DTFB_ACCELERATION_STRUCTURE_NV  = (1 << 13),
	DTFB_MUTABLE_VALVE              = (1 << 14),
};

using DescriptorTypeFlags = deUint32;

// Convert type to its corresponding flag bit.
DescriptorTypeFlagBits toDescriptorTypeFlagBit (VkDescriptorType descriptorType)
{
	switch (descriptorType)
	{
	case VK_DESCRIPTOR_TYPE_SAMPLER:                        return DTFB_SAMPLER;
	case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:         return DTFB_COMBINED_IMAGE_SAMPLER;
	case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:                  return DTFB_SAMPLED_IMAGE;
	case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:                  return DTFB_STORAGE_IMAGE;
	case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:           return DTFB_UNIFORM_TEXEL_BUFFER;
	case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:           return DTFB_STORAGE_TEXEL_BUFFER;
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:                 return DTFB_UNIFORM_BUFFER;
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:                 return DTFB_STORAGE_BUFFER;
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:         return DTFB_UNIFORM_BUFFER_DYNAMIC;
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:         return DTFB_STORAGE_BUFFER_DYNAMIC;
	case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:               return DTFB_INPUT_ATTACHMENT;
	case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT:       return DTFB_INLINE_UNIFORM_BLOCK_EXT;
	case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:     return DTFB_ACCELERATION_STRUCTURE_KHR;
	case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV:      return DTFB_ACCELERATION_STRUCTURE_NV;
	case VK_DESCRIPTOR_TYPE_MUTABLE_VALVE:                  return DTFB_MUTABLE_VALVE;
	default: break;
	}

	// Unreachable.
	DE_ASSERT(false);
	return DTFB_SAMPLER;
}

// Convert vector of descriptor types to a bitfield.
DescriptorTypeFlags toDescriptorTypeFlags (const std::vector<VkDescriptorType>& types)
{
	DescriptorTypeFlags result = 0u;
	for (const auto& t : types)
		result |= toDescriptorTypeFlagBit(t);
	return result;
}

// Convert bitfield to vector of descriptor types.
std::vector<VkDescriptorType> toDescriptorTypeVector (DescriptorTypeFlags bitfield)
{
	std::vector<VkDescriptorType> result;

	if (bitfield & DTFB_SAMPLER)                     result.push_back(VK_DESCRIPTOR_TYPE_SAMPLER);
	if (bitfield & DTFB_COMBINED_IMAGE_SAMPLER)      result.push_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	if (bitfield & DTFB_SAMPLED_IMAGE)               result.push_back(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	if (bitfield & DTFB_STORAGE_IMAGE)               result.push_back(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	if (bitfield & DTFB_UNIFORM_TEXEL_BUFFER)        result.push_back(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER);
	if (bitfield & DTFB_STORAGE_TEXEL_BUFFER)        result.push_back(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER);
	if (bitfield & DTFB_UNIFORM_BUFFER)              result.push_back(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	if (bitfield & DTFB_STORAGE_BUFFER)              result.push_back(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	if (bitfield & DTFB_UNIFORM_BUFFER_DYNAMIC)      result.push_back(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);
	if (bitfield & DTFB_STORAGE_BUFFER_DYNAMIC)      result.push_back(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);
	if (bitfield & DTFB_INPUT_ATTACHMENT)            result.push_back(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);
	if (bitfield & DTFB_INLINE_UNIFORM_BLOCK_EXT)    result.push_back(VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT);
	if (bitfield & DTFB_ACCELERATION_STRUCTURE_KHR)  result.push_back(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
	if (bitfield & DTFB_ACCELERATION_STRUCTURE_NV)   result.push_back(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV);
	if (bitfield & DTFB_MUTABLE_VALVE)               result.push_back(VK_DESCRIPTOR_TYPE_MUTABLE_VALVE);

	return result;
}

// How to create the source set when copying descriptors from another set.
// * MUTABLE means to transform bindings into mutable bindings.
// * NONMUTABLE means to transform bindings into non-mutable bindings.
enum class SourceSetStrategy
{
	MUTABLE = 0,
	NONMUTABLE,
	NO_SOURCE,
};

enum class PoolMutableStrategy
{
	KEEP_TYPES = 0,
	EXPAND_TYPES,
	NO_TYPES,
};

// Type of information that's present in VkWriteDescriptorSet.
enum class WriteType
{
	IMAGE_INFO = 0,
	BUFFER_INFO,
	BUFFER_VIEW,
	ACCELERATION_STRUCTURE_INFO,
};

struct WriteInfo
{
	WriteType writeType;
	union
	{
		VkDescriptorImageInfo                           imageInfo;
		VkDescriptorBufferInfo                          bufferInfo;
		VkBufferView                                    bufferView;
		VkWriteDescriptorSetAccelerationStructureKHR    asInfo;
	};

	explicit WriteInfo (const VkDescriptorImageInfo& info_)
		: writeType(WriteType::IMAGE_INFO)
		, imageInfo(info_)
	{}

	explicit WriteInfo (const VkDescriptorBufferInfo& info_)
		: writeType(WriteType::BUFFER_INFO)
		, bufferInfo(info_)
	{}

	explicit WriteInfo (VkBufferView view_)
		: writeType(WriteType::BUFFER_VIEW)
		, bufferView(view_)
	{}

	explicit WriteInfo (const VkWriteDescriptorSetAccelerationStructureKHR& asInfo_)
		: writeType(WriteType::ACCELERATION_STRUCTURE_INFO)
		, asInfo(asInfo_)
	{}
};

// Resource backing up a single binding.
enum class ResourceType
{
	SAMPLER = 0,
	IMAGE,
	COMBINED_IMAGE_SAMPLER,
	BUFFER,
	BUFFER_VIEW,
	ACCELERATION_STRUCTURE,
};

// Type of resource backing up a particular descriptor type.
ResourceType toResourceType (VkDescriptorType descriptorType)
{
	ResourceType resourceType = ResourceType::SAMPLER;
	switch (descriptorType)
	{
	case VK_DESCRIPTOR_TYPE_SAMPLER:
		resourceType = ResourceType::SAMPLER;
		break;

	case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
		resourceType = ResourceType::COMBINED_IMAGE_SAMPLER;
		break;

	case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
	case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
	case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
		resourceType = ResourceType::IMAGE;
		break;

	case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
	case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
		resourceType = ResourceType::BUFFER_VIEW;
		break;

	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
		resourceType = ResourceType::BUFFER;
		break;

	case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
		resourceType = ResourceType::ACCELERATION_STRUCTURE;
		break;

	default:
		DE_ASSERT(false);
		break;
	}

	return resourceType;
}

bool isShaderWritable (VkDescriptorType descriptorType)
{
	return (descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER || descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
	        descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER);
}

Move<VkSampler> makeDefaultSampler (const DeviceInterface& vkd, VkDevice device)
{
	const VkSamplerCreateInfo samplerCreateInfo = {
		VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,  //  VkStructureType			sType;
		nullptr,                                //  const void*				pNext;
		0u,                                     //  VkSamplerCreateFlags	flags;
		VK_FILTER_NEAREST,                      //  VkFilter				magFilter;
		VK_FILTER_NEAREST,                      //  VkFilter				minFilter;
		VK_SAMPLER_MIPMAP_MODE_NEAREST,         //  VkSamplerMipmapMode		mipmapMode;
		VK_SAMPLER_ADDRESS_MODE_REPEAT,         //  VkSamplerAddressMode	addressModeU;
		VK_SAMPLER_ADDRESS_MODE_REPEAT,         //  VkSamplerAddressMode	addressModeV;
		VK_SAMPLER_ADDRESS_MODE_REPEAT,         //  VkSamplerAddressMode	addressModeW;
		0.f,                                    //  float					mipLodBias;
		VK_FALSE,                               //  VkBool32				anisotropyEnable;
		1.f,                                    //  float					maxAnisotropy;
		VK_FALSE,                               //  VkBool32				compareEnable;
		VK_COMPARE_OP_ALWAYS,                   //  VkCompareOp				compareOp;
		0.f,                                    //  float					minLod;
		0.f,                                    //  float					maxLod;
		VK_BORDER_COLOR_INT_TRANSPARENT_BLACK,  //  VkBorderColor			borderColor;
		VK_FALSE,                               //  VkBool32				unnormalizedCoordinates;
	};

	return createSampler(vkd, device, &samplerCreateInfo);
}

de::MovePtr<ImageWithMemory> makeDefaultImage (const DeviceInterface& vkd, VkDevice device, Allocator& alloc)
{
	const auto              extent     = makeExtent3D(1u, 1u, 1u);
	const VkImageUsageFlags usageFlags = (
		VK_IMAGE_USAGE_SAMPLED_BIT
		| VK_IMAGE_USAGE_STORAGE_BIT
		| VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
		| VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
		| VK_IMAGE_USAGE_TRANSFER_SRC_BIT
		| VK_IMAGE_USAGE_TRANSFER_DST_BIT);

	const VkImageCreateInfo imageCreateInfo = {
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,    //  VkStructureType			sType;
		nullptr,                                //  const void*				pNext;
		0u,                                     //  VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,                       //  VkImageType				imageType;
		getDescriptorImageFormat(),             //  VkFormat				format;
		extent,                                 //  VkExtent3D				extent;
		1u,                                     //  deUint32				mipLevels;
		1u,                                     //  deUint32				arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,                  //  VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,                //  VkImageTiling			tiling;
		usageFlags,                             //  VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,              //  VkSharingMode			sharingMode;
		0u,                                     //  deUint32				queueFamilyIndexCount;
		nullptr,                                //  const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,              //  VkImageLayout			initialLayout;
	};
	return de::MovePtr<ImageWithMemory>(new ImageWithMemory(vkd, device, alloc, imageCreateInfo, MemoryRequirement::Any));
}

Move<VkImageView> makeDefaultImageView (const DeviceInterface& vkd, VkDevice device, VkImage image)
{
	const auto subresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	return makeImageView(vkd, device, image, VK_IMAGE_VIEW_TYPE_2D, getDescriptorImageFormat(), subresourceRange);
}

de::MovePtr<BufferWithMemory> makeDefaultBuffer (const DeviceInterface& vkd, VkDevice device, Allocator& alloc, deUint32 numElements = 1u)
{
	const VkBufferUsageFlags bufferUsage = (
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
		| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
		| VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT
		| VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT
		| VK_BUFFER_USAGE_TRANSFER_SRC_BIT
		| VK_BUFFER_USAGE_TRANSFER_DST_BIT);

	const auto bufferSize = static_cast<VkDeviceSize>(sizeof(deUint32) * static_cast<size_t>(numElements));

	const auto bufferCreateInfo = makeBufferCreateInfo(bufferSize, bufferUsage);

	return de::MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, alloc, bufferCreateInfo, MemoryRequirement::HostVisible));
}

Move<VkBufferView> makeDefaultBufferView (const DeviceInterface& vkd, VkDevice device, VkBuffer buffer)
{
	const auto bufferOffset = static_cast<VkDeviceSize>(0);
	const auto bufferSize   = static_cast<VkDeviceSize>(sizeof(deUint32));

	return makeBufferView(vkd, device, buffer, getDescriptorImageFormat(), bufferOffset, bufferSize);
}

struct AccelerationStructureData
{
	using TLASPtr = de::MovePtr<TopLevelAccelerationStructure>;
	using BLASPtr = de::MovePtr<BottomLevelAccelerationStructure>;

	TLASPtr tlas;
	BLASPtr blas;

	void swap (AccelerationStructureData& other)
	{
		auto myTlasPtr = tlas.release();
		auto myBlasPtr = blas.release();

		auto otherTlasPtr = other.tlas.release();
		auto otherBlasPtr = other.blas.release();

		tlas = TLASPtr(otherTlasPtr);
		blas = BLASPtr(otherBlasPtr);

		other.tlas = TLASPtr(myTlasPtr);
		other.blas = BLASPtr(myBlasPtr);
	}

	AccelerationStructureData () : tlas() , blas() {}

	AccelerationStructureData (AccelerationStructureData&& other)
		: AccelerationStructureData()
	{
		swap(other);
	}

	AccelerationStructureData& operator= (AccelerationStructureData&& other)
	{
		swap(other);
		return *this;
	}
};

AccelerationStructureData makeDefaultAccelerationStructure (const DeviceInterface& vkd, VkDevice device, VkCommandBuffer cmdBuffer, Allocator& alloc, bool triangles, deUint16 offsetX)
{
	AccelerationStructureData data;

	// Triangle around (offsetX, 0) with depth 5.0.
	const float middleX = static_cast<float>(offsetX);
	const float leftX   = middleX - 0.5f;
	const float rightX  = middleX + 0.5f;
	const float topY    = 0.5f;
	const float bottomY = -0.5f;
	const float depth   = 5.0f;

	std::vector<tcu::Vec3> vertices;

	if (triangles)
	{
		vertices.reserve(3u);
		vertices.emplace_back(middleX, topY, depth);
		vertices.emplace_back(rightX, bottomY, depth);
		vertices.emplace_back(leftX, bottomY, depth);
	}
	else
	{
		vertices.reserve(2u);
		vertices.emplace_back(leftX, bottomY, depth);
		vertices.emplace_back(rightX, topY, depth);
	}

	data.tlas = makeTopLevelAccelerationStructure();
	data.blas = makeBottomLevelAccelerationStructure();

	VkGeometryInstanceFlagsKHR instanceFlags = 0u;
	if (triangles)
		instanceFlags |= VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;

	data.blas->addGeometry(vertices, triangles, VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR);
	data.blas->createAndBuild(vkd, device, cmdBuffer, alloc);

	de::SharedPtr<BottomLevelAccelerationStructure> blasSharedPtr (data.blas.release());
	data.tlas->setInstanceCount(1u);
	data.tlas->addInstance(blasSharedPtr, identityMatrix3x4, 0u, 0xFFu, 0u, instanceFlags);
	data.tlas->createAndBuild(vkd, device, cmdBuffer, alloc);

	return data;
}

const auto kShaderAccess = (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

struct Resource
{
	VkDescriptorType              descriptorType;
	ResourceType                  resourceType;
	Move<VkSampler>               sampler;
	de::MovePtr<ImageWithMemory>  imageWithMemory;
	Move<VkImageView>             imageView;
	de::MovePtr<BufferWithMemory> bufferWithMemory;
	Move<VkBufferView>            bufferView;
	AccelerationStructureData     asData;
	deUint32                      initialValue;

	Resource (VkDescriptorType descriptorType_, const DeviceInterface& vkd, VkDevice device, Allocator& alloc, deUint32 qIndex, VkQueue queue, bool useAABBs, deUint32 initialValue_, deUint32 numElements = 1u)
		: descriptorType	(descriptorType_)
		, resourceType      (toResourceType(descriptorType))
		, sampler           ()
		, imageWithMemory   ()
		, imageView         ()
		, bufferWithMemory  ()
		, bufferView        ()
		, asData            ()
		, initialValue      (initialValue_)
	{
		if (numElements != 1u)
			DE_ASSERT(resourceType == ResourceType::BUFFER);

		switch (resourceType)
		{
		case ResourceType::SAMPLER:
			sampler = makeDefaultSampler(vkd, device);
			break;

		case ResourceType::IMAGE:
			imageWithMemory = makeDefaultImage(vkd, device, alloc);
			imageView       = makeDefaultImageView(vkd, device, imageWithMemory->get());
			break;

		case ResourceType::COMBINED_IMAGE_SAMPLER:
			sampler         = makeDefaultSampler(vkd, device);
			imageWithMemory = makeDefaultImage(vkd, device, alloc);
			imageView       = makeDefaultImageView(vkd, device, imageWithMemory->get());
			break;

		case ResourceType::BUFFER:
			bufferWithMemory = makeDefaultBuffer(vkd, device, alloc, numElements);
			break;

		case ResourceType::BUFFER_VIEW:
			bufferWithMemory = makeDefaultBuffer(vkd, device, alloc);
			bufferView       = makeDefaultBufferView(vkd, device, bufferWithMemory->get());
			break;

		case ResourceType::ACCELERATION_STRUCTURE:
			{
				const auto cmdPool      = makeCommandPool(vkd, device, qIndex);
				const auto cmdBufferPtr = allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
				const auto cmdBuffer    = cmdBufferPtr.get();
				const bool triangles    = !useAABBs;

				beginCommandBuffer(vkd, cmdBuffer);
				asData = makeDefaultAccelerationStructure(vkd, device, cmdBuffer, alloc, triangles, getAccelerationStructureOffsetX(initialValue));
				endCommandBuffer(vkd, cmdBuffer);
				submitCommandsAndWait(vkd, device, queue, cmdBuffer);
			}
			break;

		default:
			DE_ASSERT(false);
			break;
		}

		if (imageWithMemory || bufferWithMemory)
		{
			const auto cmdPool      = makeCommandPool(vkd, device, qIndex);
			const auto cmdBufferPtr = allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
			const auto cmdBuffer    = cmdBufferPtr.get();

			if (imageWithMemory)
			{
				// Prepare staging buffer.
				const auto               bufferSize        = static_cast<VkDeviceSize>(sizeof(initialValue));
				const VkBufferUsageFlags bufferUsage       = (VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
				const auto               stagingBufferInfo = makeBufferCreateInfo(bufferSize, bufferUsage);

				BufferWithMemory stagingBuffer(vkd, device, alloc, stagingBufferInfo, MemoryRequirement::HostVisible);
				auto& bufferAlloc = stagingBuffer.getAllocation();
				void* bufferData  = bufferAlloc.getHostPtr();

				deMemcpy(bufferData, &initialValue, sizeof(initialValue));
				flushAlloc(vkd, device, bufferAlloc);

				beginCommandBuffer(vkd, cmdBuffer);

				// Transition and copy image.
				const auto copyRegion         = makeBufferImageCopy(makeExtent3D(1u, 1u, 1u),
																	makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u));

				// Switch image to TRANSFER_DST_OPTIMAL before copying data to it.
				const auto subresourceRange   = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

				const auto preTransferBarrier = makeImageMemoryBarrier(
					0u, VK_ACCESS_TRANSFER_WRITE_BIT,
					VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					imageWithMemory->get(), subresourceRange);

				vkd.cmdPipelineBarrier(
					cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
					0u, nullptr, 0u, nullptr, 1u, &preTransferBarrier);

				// Copy data to image.
				vkd.cmdCopyBufferToImage(cmdBuffer, stagingBuffer.get(), imageWithMemory->get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &copyRegion);

				// Switch image to the GENERAL layout before reading or writing to it from shaders.
				const auto postTransferBarrier = makeImageMemoryBarrier(
					VK_ACCESS_TRANSFER_WRITE_BIT, kShaderAccess,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
					imageWithMemory->get(), subresourceRange);

				vkd.cmdPipelineBarrier(
					cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0u,
					0u, nullptr, 0u, nullptr, 1u, &postTransferBarrier);

				endCommandBuffer(vkd, cmdBuffer);
				submitCommandsAndWait(vkd, device, queue, cmdBuffer);
			}

			if (bufferWithMemory)
			{
				auto& bufferAlloc = bufferWithMemory->getAllocation();
				void* bufferData  = bufferAlloc.getHostPtr();

				const std::vector<deUint32> bufferValues(numElements, initialValue);
				deMemcpy(bufferData, bufferValues.data(), de::dataSize(bufferValues));
				flushAlloc(vkd, device, bufferAlloc);

				beginCommandBuffer(vkd, cmdBuffer);

				// Make sure host writes happen before shader reads/writes. Note: this barrier is not needed in theory.
				const auto hostToShaderBarrier = makeMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT, kShaderAccess);

				vkd.cmdPipelineBarrier(
					cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0u,
					1u, &hostToShaderBarrier, 0u, nullptr, 0u, nullptr);

				endCommandBuffer(vkd, cmdBuffer);
				submitCommandsAndWait(vkd, device, queue, cmdBuffer);
			}
		}
	}

	// Remove problematic copy constructor.
	Resource (const Resource&) = delete;

	// Make it movable.
	Resource (Resource&& other) noexcept
		: descriptorType	(other.descriptorType)
		, resourceType      (other.resourceType)
		, sampler           (other.sampler)
		, imageWithMemory   (other.imageWithMemory.release())
		, imageView         (other.imageView)
		, bufferWithMemory  (other.bufferWithMemory.release())
		, bufferView        (other.bufferView)
		, asData			(std::move(other.asData))
		, initialValue      (other.initialValue)
	{}

	~Resource ()
	{}

	WriteInfo makeWriteInfo () const
	{
		using WriteInfoPtr = de::MovePtr<WriteInfo>;

		WriteInfoPtr writeInfo;

		switch (resourceType)
		{
		case ResourceType::SAMPLER:
			{
				const VkDescriptorImageInfo imageInfo = { sampler.get(), DE_NULL, VK_IMAGE_LAYOUT_UNDEFINED };
				writeInfo = WriteInfoPtr (new WriteInfo(imageInfo));
			}
			break;

		case ResourceType::IMAGE:
			{
				const VkDescriptorImageInfo imageInfo = { DE_NULL, imageView.get(), VK_IMAGE_LAYOUT_GENERAL };
				writeInfo = WriteInfoPtr (new WriteInfo(imageInfo));
			}
			break;

		case ResourceType::COMBINED_IMAGE_SAMPLER:
			{
				const VkDescriptorImageInfo imageInfo = { sampler.get(), imageView.get(), VK_IMAGE_LAYOUT_GENERAL };
				writeInfo = WriteInfoPtr (new WriteInfo(imageInfo));
			}
			break;

		case ResourceType::BUFFER:
			{
				const VkDescriptorBufferInfo bufferInfo = { bufferWithMemory->get(), 0ull, static_cast<VkDeviceSize>(sizeof(deUint32)) };
				writeInfo = WriteInfoPtr (new WriteInfo(bufferInfo));
			}
			break;

		case ResourceType::BUFFER_VIEW:
			writeInfo = WriteInfoPtr (new WriteInfo(bufferView.get()));
			break;

		case ResourceType::ACCELERATION_STRUCTURE:
			{
				VkWriteDescriptorSetAccelerationStructureKHR asWrite = initVulkanStructure();
				asWrite.accelerationStructureCount = 1u;
				asWrite.pAccelerationStructures    = asData.tlas.get()->getPtr();
				writeInfo = WriteInfoPtr (new WriteInfo(asWrite));
			}
			break;

		default:
			DE_ASSERT(false);
			break;
		}

		return *writeInfo;
	}

	tcu::Maybe<deUint32> getStoredValue (const DeviceInterface& vkd, VkDevice device, Allocator& alloc, deUint32 qIndex, VkQueue queue, deUint32 position = 0u) const
	{
		if (position != 0u)
			DE_ASSERT(static_cast<bool>(bufferWithMemory));

		if (imageWithMemory || bufferWithMemory)
		{
			// Command pool and buffer.
			const auto cmdPool      = makeCommandPool(vkd, device, qIndex);
			const auto cmdBufferPtr = allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
			const auto cmdBuffer    = cmdBufferPtr.get();

			if (imageWithMemory)
			{
				// Prepare staging buffer.
				deUint32                 result;
				const auto               bufferSize        = static_cast<VkDeviceSize>(sizeof(result));
				const VkBufferUsageFlags bufferUsage       = (VK_BUFFER_USAGE_TRANSFER_DST_BIT);
				const auto               stagingBufferInfo = makeBufferCreateInfo(bufferSize, bufferUsage);

				BufferWithMemory stagingBuffer(vkd, device, alloc, stagingBufferInfo, MemoryRequirement::HostVisible);
				auto& bufferAlloc = stagingBuffer.getAllocation();
				void* bufferData  = bufferAlloc.getHostPtr();

				// Copy image value to staging buffer.
				beginCommandBuffer(vkd, cmdBuffer);

				// Make sure shader accesses happen before transfers and prepare image for transfer.
				const auto colorResourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

				const auto preTransferBarrier = makeImageMemoryBarrier(
					kShaderAccess, VK_ACCESS_TRANSFER_READ_BIT,
					VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					imageWithMemory->get(), colorResourceRange);

				vkd.cmdPipelineBarrier(
					cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
					0u, nullptr, 0u, nullptr, 1u, &preTransferBarrier);

				// Copy image contents to staging buffer.
				const auto copyRegion = makeBufferImageCopy(makeExtent3D(1u, 1u, 1u),
															makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u));
				vkd.cmdCopyImageToBuffer(cmdBuffer, imageWithMemory->get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer.get(), 1u, &copyRegion);

				// Make sure writes are visible from the host.
				const auto postTransferBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
				vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &postTransferBarrier, 0u, nullptr, 0u, nullptr);

				endCommandBuffer(vkd, cmdBuffer);
				submitCommandsAndWait(vkd, device, queue, cmdBuffer);

				// Get value from staging buffer.
				invalidateAlloc(vkd, device, bufferAlloc);
				deMemcpy(&result, bufferData, sizeof(result));
				return tcu::just(result);
			}

			if (bufferWithMemory)
			{
				auto&       bufferAlloc = bufferWithMemory->getAllocation();
				auto        bufferData  = reinterpret_cast<const char*>(bufferAlloc.getHostPtr());
				deUint32    result;

				// Make sure shader writes are visible from the host.
				beginCommandBuffer(vkd, cmdBuffer);

				const auto shaderToHostBarrier = makeMemoryBarrier(kShaderAccess, VK_ACCESS_HOST_READ_BIT);
				vkd.cmdPipelineBarrier(
					cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u,
					1u, &shaderToHostBarrier, 0u, nullptr, 0u, nullptr);

				endCommandBuffer(vkd, cmdBuffer);
				submitCommandsAndWait(vkd, device, queue, cmdBuffer);

				invalidateAlloc(vkd, device, bufferAlloc);
				deMemcpy(&result, bufferData + sizeof(deUint32) * static_cast<size_t>(position), sizeof(result));
				return tcu::just(result);
			}
		}

		return tcu::nothing<deUint32>();
	}
};

struct BindingInterface
{
	// Minimum number of iterations to test all mutable types.
	virtual deUint32 maxTypes () const = 0;

	// Types that will be used by the binding at a given iteration.
	virtual std::vector<VkDescriptorType> typesAtIteration (deUint32 iteration) const = 0;

	// Binding's main type.
	virtual VkDescriptorType mainType () const = 0;

	// Binding's list of mutable types, if present.
	virtual std::vector<VkDescriptorType> mutableTypes () const = 0;

	// Descriptor count in the binding.
	virtual size_t size () const = 0;

	// Is the binding an array binding?
	virtual bool isArray () const = 0;

	// Is the binding an unbounded array?
	virtual bool isUnbounded () const = 0;

	// Will the binding use different descriptor types in a given iteration?
	virtual bool needsAliasing (deUint32 iteration) const
	{
		const auto                 typesVec = typesAtIteration(iteration);
		std::set<VkDescriptorType> descTypes(begin(typesVec), end(typesVec));
		return (descTypes.size() > 1u);
	}

	// Will the binding need aliasing on any iteration up to a given number?
	virtual bool needsAliasingUpTo (deUint32 numIterations) const
	{
		std::vector<bool> needsAliasingFlags;
		needsAliasingFlags.reserve(numIterations);

		for (deUint32 iter = 0u; iter < numIterations; ++iter)
			needsAliasingFlags.push_back(needsAliasing(iter));

		return std::any_of(begin(needsAliasingFlags), end(needsAliasingFlags), [] (bool f) { return f; });
	}

private:
	virtual bool hasDescriptorType (deUint32 iteration, VkDescriptorType descriptorType) const
	{
		const auto typesVec = typesAtIteration(iteration);
		return (std::find(begin(typesVec), end(typesVec), descriptorType) != end(typesVec));
	}

public:
	// Convert one particular binding to a mutable or non-mutable equivalent binding, returning the equivalent binding.
	virtual de::MovePtr<BindingInterface> toMutable (deUint32 iteration) const = 0;
	virtual de::MovePtr<BindingInterface> toNonMutable (deUint32 iteration) const = 0;

	// Create resources needed to back up this binding.
	virtual std::vector<Resource> createResources (
		const DeviceInterface& vkd, VkDevice device, Allocator& alloc, deUint32 qIndex, VkQueue queue,
		deUint32 iteration, bool useAABBs, deUint32 baseValue) const = 0;

	// Get GLSL binding declarations. Note: no array size means no array, if size is < 0 it means unbounded array.
	virtual std::string glslDeclarations (deUint32 iteration, deUint32 setNum, deUint32 bindingNum, deUint32 inputAttachmentIdx, tcu::Maybe<deInt32> arraySize) const = 0;

	// Get GLSL statements to check this binding.
	virtual std::string glslCheckStatements (deUint32 iteration, deUint32 setNum, deUint32 bindingNum, deUint32 baseValue, tcu::Maybe<deUint32> arrayIndex, bool usePushConstants) const = 0;
};

// Represents a single binding that will be used in a test.
class SingleBinding : public BindingInterface
{
private:
	VkDescriptorType              type;             // The descriptor type.
	std::vector<VkDescriptorType> mutableTypesVec;  // The types that will be used for each iteration of a test if mutable.

public:
	SingleBinding (VkDescriptorType type_, std::vector<VkDescriptorType> mutableTypes_)
		: type              (type_)
		, mutableTypesVec   (std::move(mutableTypes_))
	{
		static const auto kForbiddenMutableTypes = getForbiddenMutableTypes();
		const auto        kBeginForbidden        = begin(kForbiddenMutableTypes);
		const auto        kEndForbidden          = end(kForbiddenMutableTypes);

		// For release builds.
		DE_UNREF(kBeginForbidden);
		DE_UNREF(kEndForbidden);

		if (type != VK_DESCRIPTOR_TYPE_MUTABLE_VALVE)
		{
			DE_ASSERT(mutableTypesVec.empty());
		}
		else
		{
			DE_ASSERT(!mutableTypesVec.empty());
			DE_ASSERT(std::none_of(begin(mutableTypesVec), end(mutableTypesVec),
			                       [&kBeginForbidden, &kEndForbidden] (VkDescriptorType t) -> bool {
				                       return std::find(kBeginForbidden, kEndForbidden, t) != kEndForbidden;
			                       }));
		}
	}

	deUint32 maxTypes () const override
	{
		if (type != VK_DESCRIPTOR_TYPE_MUTABLE_VALVE)
			return 1u;
		const auto vecSize = mutableTypesVec.size();
		DE_ASSERT(vecSize <= std::numeric_limits<deUint32>::max());
		return static_cast<deUint32>(vecSize);
	}

	VkDescriptorType typeAtIteration (deUint32 iteration) const
	{
		return typesAtIteration(iteration)[0];
	}

	std::vector<VkDescriptorType> usedTypes () const
	{
		if (type != VK_DESCRIPTOR_TYPE_MUTABLE_VALVE)
			return std::vector<VkDescriptorType>(1u, type);
		return mutableTypesVec;
	}

	std::vector<VkDescriptorType> typesAtIteration (deUint32 iteration) const override
	{
		const auto typesVec = usedTypes();
		return std::vector<VkDescriptorType>(1u, typesVec[static_cast<size_t>(iteration) % typesVec.size()]);
	}

	VkDescriptorType mainType () const override
	{
		return type;
	}

	std::vector<VkDescriptorType> mutableTypes () const override
	{
		return mutableTypesVec;
	}

	size_t size () const override
	{
		return size_t{1u};
	}

	bool isArray () const override
	{
		return false;
	}

	bool isUnbounded () const override
	{
		return false;
	}

	de::MovePtr<BindingInterface> toMutable (deUint32 iteration) const override
	{
		DE_UNREF(iteration);

		static const auto kMandatoryMutableTypeFlags = toDescriptorTypeFlags(getMandatoryMutableTypes());
		if (type == VK_DESCRIPTOR_TYPE_MUTABLE_VALVE)
		{
			const auto descFlags = (toDescriptorTypeFlags(mutableTypesVec) | kMandatoryMutableTypeFlags);
			return de::MovePtr<BindingInterface>(new SingleBinding(type, toDescriptorTypeVector(descFlags)));
		}

		// Make sure it's not a forbidden mutable type.
		static const auto kForbiddenMutableTypes = getForbiddenMutableTypes();
		DE_ASSERT(std::find(begin(kForbiddenMutableTypes), end(kForbiddenMutableTypes), type) == end(kForbiddenMutableTypes));

		// Convert the binding to mutable using a wider set of descriptor types if possible, including the binding type.
		const auto descFlags = (kMandatoryMutableTypeFlags | toDescriptorTypeFlagBit(type));

		return de::MovePtr<BindingInterface>(new SingleBinding(VK_DESCRIPTOR_TYPE_MUTABLE_VALVE, toDescriptorTypeVector(descFlags)));
	}

	de::MovePtr<BindingInterface> toNonMutable (deUint32 iteration) const override
	{
		return de::MovePtr<BindingInterface>(new SingleBinding(typeAtIteration(iteration), std::vector<VkDescriptorType>()));
	}

	std::vector<Resource> createResources (
		const DeviceInterface& vkd, VkDevice device, Allocator& alloc, deUint32 qIndex, VkQueue queue,
		deUint32 iteration, bool useAABBs, deUint32 baseValue) const override
	{
		const auto descriptorType = typeAtIteration(iteration);

		std::vector<Resource> resources;
		resources.emplace_back(descriptorType, vkd, device, alloc, qIndex, queue, useAABBs, baseValue);
		return resources;
	}

	std::string glslDeclarations (deUint32 iteration, deUint32 setNum, deUint32 bindingNum, deUint32 inputAttachmentIdx, tcu::Maybe<deInt32> arraySize) const override
	{
		const auto         descriptorType = typeAtIteration(iteration);
		const std::string  arraySuffix    = ((static_cast<bool>(arraySize)) ? ((arraySize.get() < 0) ? "[]" : ("[" + de::toString(arraySize.get()) + "]")) : "");
		const std::string  layoutAttribs  = "set=" + de::toString(setNum) + ", binding=" + de::toString(bindingNum);
		const std::string  bindingSuffix  = "_" + de::toString(setNum) + "_" + de::toString(bindingNum);
		const std::string  nameSuffix     = bindingSuffix + arraySuffix;
		std::ostringstream declarations;

		declarations << "layout (";

		switch (descriptorType)
		{
		case VK_DESCRIPTOR_TYPE_SAMPLER:
			declarations << layoutAttribs << ") uniform sampler sampler" << nameSuffix;
			break;

		case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			declarations << layoutAttribs << ") uniform usampler2D combinedSampler" << nameSuffix;
			break;

		case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
			declarations << layoutAttribs << ") uniform utexture2D sampledImage" << nameSuffix;
			break;

		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			declarations << layoutAttribs << ") uniform uboBlock" << bindingSuffix << " { uint val; } ubo" << nameSuffix;
			break;

		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
			declarations << layoutAttribs << ") buffer sboBlock" << bindingSuffix << " { uint val; } ssbo" << nameSuffix;
			break;

		case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
			declarations << layoutAttribs << ") uniform utextureBuffer uniformTexel" << nameSuffix;
			break;

		case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
			declarations << layoutAttribs << ", r32ui) uniform uimageBuffer storageTexel" << nameSuffix;
			break;

		case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
			declarations << layoutAttribs << ", r32ui) uniform uimage2D storageImage" << nameSuffix;
			break;

		case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
			declarations << layoutAttribs << ", input_attachment_index=" << inputAttachmentIdx << ") uniform usubpassInput inputAttachment" << nameSuffix;
			break;

		case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
			declarations << layoutAttribs << ") uniform accelerationStructureEXT accelerationStructure" << nameSuffix;
			break;

		default:
			DE_ASSERT(false);
			break;
		}

		declarations << ";\n";

		return declarations.str();
	}

	std::string glslCheckStatements (deUint32 iteration, deUint32 setNum, deUint32 bindingNum, deUint32 baseValue_, tcu::Maybe<deUint32> arrayIndex, bool usePushConstants) const override
	{
		const auto        descriptorType = typeAtIteration(iteration);
		const std::string bindingSuffix  = "_" + de::toString(setNum) + "_" + de::toString(bindingNum);

		std::string indexSuffix;
		if (arrayIndex)
		{
			indexSuffix = de::toString(arrayIndex.get());
			if (usePushConstants)
				indexSuffix += " + pc.zero";
			indexSuffix = "[" + indexSuffix + "]";
		}

		const std::string nameSuffix         = bindingSuffix + indexSuffix;
		const std::string baseValue          = toHex(baseValue_);
		const std::string externalImageValue = toHex(getExternalSampledImageValue());
		const std::string mask               = toHex(getStoredValueMask());

		std::ostringstream checks;

		// Note: all of these depend on an external anyError uint variable.
		switch (descriptorType)
		{
		case VK_DESCRIPTOR_TYPE_SAMPLER:
			// Note this depends on an "externalSampledImage" binding.
			checks << "    {\n";
			checks << "      uint readValue = texture(usampler2D(externalSampledImage, sampler" << nameSuffix << "), vec2(0, 0)).r;\n";
			checks << "      debugPrintfEXT(\"iteration-" << iteration << nameSuffix << ": 0x%xu\\n\", readValue);\n";
			checks << "      anyError |= ((readValue == " << externalImageValue << ") ? 0u : 1u);\n";
			//checks << "      anyError = readValue;\n";
			checks << "    }\n";
			break;

		case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			checks << "    {\n";
			checks << "      uint readValue = texture(combinedSampler" << nameSuffix << ", vec2(0, 0)).r;\n";
			checks << "      debugPrintfEXT(\"iteration-" << iteration << nameSuffix << ": 0x%xu\\n\", readValue);\n";
			checks << "      anyError |= ((readValue == " << baseValue << ") ? 0u : 1u);\n";
			//checks << "      anyError = readValue;\n";
			checks << "    }\n";
			break;

		case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
			// Note this depends on an "externalSampler" binding.
			checks << "    {\n";
			checks << "      uint readValue = texture(usampler2D(sampledImage" << nameSuffix << ", externalSampler), vec2(0, 0)).r;\n";
			checks << "      debugPrintfEXT(\"iteration-" << iteration << nameSuffix << ": 0x%xu\\n\", readValue);\n";
			checks << "      anyError |= ((readValue == " << baseValue << ") ? 0u : 1u);\n";
			//checks << "      anyError = readValue;\n";
			checks << "    }\n";
			break;

		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			checks << "    {\n";
			checks << "      uint readValue = ubo" << nameSuffix << ".val;\n";
			checks << "      debugPrintfEXT(\"iteration-" << iteration << nameSuffix << ": 0x%xu\\n\", readValue);\n";
			checks << "      anyError |= ((readValue == " << baseValue << ") ? 0u : 1u);\n";
			//checks << "      anyError = readValue;\n";
			checks << "    }\n";
			break;

		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
			checks << "    {\n";
			checks << "      uint readValue = ssbo" << nameSuffix << ".val;\n";
			checks << "      debugPrintfEXT(\"iteration-" << iteration << nameSuffix << ": 0x%xu\\n\", readValue);\n";
			checks << "      anyError |= ((readValue == " << baseValue << ") ? 0u : 1u);\n";
			//checks << "      anyError = readValue;\n";
			// Check writes.
			checks << "      ssbo" << nameSuffix << ".val = (readValue | " << mask << ");\n";
			checks << "    }\n";
			break;

		case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
			checks << "    {\n";
			checks << "      uint readValue = texelFetch(uniformTexel" << nameSuffix << ", 0).x;\n";
			checks << "      debugPrintfEXT(\"iteration-" << iteration << nameSuffix << ": 0x%xu\\n\", readValue);\n";
			checks << "      anyError |= ((readValue == " << baseValue << ") ? 0u : 1u);\n";
			//checks << "      anyError = readValue;\n";
			checks << "    }\n";
			break;

		case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
			checks << "    {\n";
			checks << "      uint readValue = imageLoad(storageTexel" << nameSuffix << ", 0).x;\n";
			checks << "      debugPrintfEXT(\"iteration-" << iteration << nameSuffix << ": 0x%xu\\n\", readValue);\n";
			checks << "      anyError |= ((readValue == " << baseValue << ") ? 0u : 1u);\n";
			//checks << "      anyError = readValue;\n";
			checks << "      readValue |= " << mask << ";\n";
			// Check writes.
			checks << "      imageStore(storageTexel" << nameSuffix << ", 0, uvec4(readValue, 0, 0, 0));\n";
			checks << "    }\n";
			break;

		case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
			checks << "    {\n";
			checks << "      uint readValue = imageLoad(storageImage" << nameSuffix << ", ivec2(0, 0)).x;\n";
			checks << "      debugPrintfEXT(\"iteration-" << iteration << nameSuffix << ": 0x%xu\\n\", readValue);\n";
			checks << "      anyError |= ((readValue == " << baseValue << ") ? 0u : 1u);\n";
			//checks << "      anyError = readValue;\n";
			checks << "      readValue |= " << mask << ";\n";
			// Check writes.
			checks << "      imageStore(storageImage" << nameSuffix << ", ivec2(0, 0), uvec4(readValue, 0, 0, 0));\n";
			checks << "    }\n";
			break;

		case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
			checks << "    {\n";
			checks << "      uint readValue = subpassLoad(inputAttachment" << nameSuffix << ").x;\n";
			checks << "      debugPrintfEXT(\"iteration-" << iteration << nameSuffix << ": 0x%xu\\n\", readValue);\n";
			checks << "      anyError |= ((readValue == " << baseValue << ") ? 0u : 1u);\n";
			//checks << "      anyError = readValue;\n";
			checks << "    }\n";
			break;

		case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
			checks << "    {\n";
			checks << "      const uint cullMask = 0xFF;\n";
			checks << "      const vec3 origin = vec3(" << getAccelerationStructureOffsetX(baseValue_) << ".0, 0.0, 0.0);\n";
			checks << "      const vec3 direction = vec3(0.0, 0.0, 1.0);\n";
			checks << "      const float tmin = 1.0;\n";
			checks << "      const float tmax = 10.0;\n";
			checks << "      uint candidateFound = 0u;\n";
			checks << "      rayQueryEXT rq;\n";
			checks << "      rayQueryInitializeEXT(rq, accelerationStructure" << nameSuffix << ", gl_RayFlagsNoneEXT, cullMask, origin, tmin, direction, tmax);\n";
			checks << "      while (rayQueryProceedEXT(rq)) {\n";
			checks << "        const uint candidateType = rayQueryGetIntersectionTypeEXT(rq, false);\n";
			checks << "        if (candidateType == gl_RayQueryCandidateIntersectionTriangleEXT || candidateType == gl_RayQueryCandidateIntersectionAABBEXT) {\n";
			checks << "          candidateFound = 1u;\n";
			checks << "        }\n";
			checks << "      }\n";
			checks << "      anyError |= ((candidateFound == 1u) ? 0u : 1u);\n";
			checks << "    }\n";
			break;

		default:
			DE_ASSERT(false);
			break;
		}

		return checks.str();
	}
};

// Represents an array of bindings. Individual bindings are stored as SingleBindings because each one of them may take a different
// type in each iteration (i.e. they can all have different descriptor type vectors).
class ArrayBinding : public BindingInterface
{
private:
	bool                       unbounded;
	std::vector<SingleBinding> bindings;

public:
	ArrayBinding (bool unbounded_, std::vector<SingleBinding> bindings_)
		: unbounded (unbounded_)
		, bindings  (std::move(bindings_))
	{
		// We need to check all single bindings have the same effective type, even if mutable descriptors have different orders.
		DE_ASSERT(!bindings.empty());

		std::set<VkDescriptorType>    basicTypes;
		std::set<DescriptorTypeFlags> bindingTypes;

		for (const auto& b : bindings)
		{
			basicTypes.insert(b.mainType());
			bindingTypes.insert(toDescriptorTypeFlags(b.usedTypes()));
		}

		DE_ASSERT(basicTypes.size() == 1u);
		DE_ASSERT(bindingTypes.size() == 1u);

		// For release builds.
		DE_UNREF(basicTypes);
		DE_UNREF(bindingTypes);
	}

	deUint32 maxTypes () const override
	{
		// Each binding may have the same effective type but a different number of iterations due to repeated types.
		std::vector<size_t> bindingSizes;
		bindingSizes.reserve(bindings.size());

		std::transform(begin(bindings), end(bindings), std::back_inserter(bindingSizes),
		               [] (const SingleBinding& b) { return b.usedTypes().size(); });

		const auto maxElement = std::max_element(begin(bindingSizes), end(bindingSizes));
		DE_ASSERT(maxElement != end(bindingSizes));
		DE_ASSERT(*maxElement <= std::numeric_limits<deUint32>::max());
		return static_cast<deUint32>(*maxElement);
	}

	std::vector<VkDescriptorType> typesAtIteration (deUint32 iteration) const override
	{
		std::vector<VkDescriptorType> result;
		result.reserve(bindings.size());

		for (const auto& b : bindings)
			result.push_back(b.typeAtIteration(iteration));

		return result;
	}

	VkDescriptorType mainType () const override
	{
		return bindings[0].mainType();
	}

	std::vector<VkDescriptorType> mutableTypes () const override
	{
		return bindings[0].mutableTypes();
	}

	size_t size () const override
	{
		return bindings.size();
	}

	bool isArray () const override
	{
		return true;
	}

	bool isUnbounded () const override
	{
		return unbounded;
	}

	de::MovePtr<BindingInterface> toMutable (deUint32 iteration) const override
	{
		// Replicate the first binding once converted, as all are equivalent.
		const auto                       firstBindingPtr = bindings[0].toMutable(iteration);
		const auto                       firstBinding    = *dynamic_cast<SingleBinding*>(firstBindingPtr.get());
		const std::vector<SingleBinding> newBindings     (bindings.size(), firstBinding);

		return de::MovePtr<BindingInterface>(new ArrayBinding(unbounded, newBindings));
	}

	de::MovePtr<BindingInterface> toNonMutable (deUint32 iteration) const override
	{
		// Make sure this binding can be converted to nonmutable for a given iteration.
		DE_ASSERT(!needsAliasing(iteration));

		// We could use each SingleBinding's toNonMutable(), but this is the same.
		const auto                       descType       = bindings[0].typeAtIteration(iteration);
		const SingleBinding              firstBinding   (descType, std::vector<VkDescriptorType>());
		const std::vector<SingleBinding> newBindings    (bindings.size(), firstBinding);

		return de::MovePtr<BindingInterface>(new ArrayBinding(unbounded, newBindings));
	}

	std::vector<Resource> createResources (
		const DeviceInterface& vkd, VkDevice device, Allocator& alloc, deUint32 qIndex, VkQueue queue,
		deUint32 iteration, bool useAABBs, deUint32 baseValue) const override
	{
		std::vector<Resource> resources;
		const auto            numBindings = static_cast<deUint32>(bindings.size());

		for (deUint32 i = 0u; i < numBindings; ++i)
		{
			auto resourceVec = bindings[i].createResources(vkd, device, alloc, qIndex, queue, iteration, useAABBs, baseValue + i);
			resources.emplace_back(std::move(resourceVec[0]));
		}

		return resources;
	}

	// We will ignore the array size parameter.
	std::string glslDeclarations (deUint32 iteration, deUint32 setNum, deUint32 bindingNum, deUint32 inputAttachmentIdx, tcu::Maybe<deInt32> arraySize) const override
	{
		const auto descriptorCount = bindings.size();
		const auto arraySizeVal    = (isUnbounded() ? tcu::just(deInt32{-1}) : tcu::just(static_cast<deInt32>(descriptorCount)));

		DE_UNREF(arraySize);
		DE_ASSERT(descriptorCount < static_cast<size_t>(std::numeric_limits<deInt32>::max()));

		// Maybe a single declaration is enough.
		if (!needsAliasing(iteration))
			return bindings[0].glslDeclarations(iteration, setNum, bindingNum, inputAttachmentIdx, arraySizeVal);

		// Aliasing needed. Avoid reusing types.
		const auto                 descriptorTypes = typesAtIteration(iteration);
		std::set<VkDescriptorType> usedTypes;
		std::ostringstream         declarations;

		for (size_t descriptorIdx = 0u; descriptorIdx < descriptorCount; ++descriptorIdx)
		{
			const auto& descriptorType = descriptorTypes[descriptorIdx];
			if (usedTypes.count(descriptorType) > 0)
				continue;

			usedTypes.insert(descriptorType);
			declarations << bindings[descriptorIdx].glslDeclarations(iteration, setNum, bindingNum, inputAttachmentIdx, arraySizeVal);
		}

		return declarations.str();
	}

	std::string glslCheckStatements (deUint32 iteration, deUint32 setNum, deUint32 bindingNum, deUint32 baseValue_, tcu::Maybe<deUint32> arrayIndex, bool usePushConstants) const override
	{
		DE_ASSERT(!arrayIndex);
		DE_UNREF(arrayIndex); // For release builds.

		std::ostringstream checks;
		const auto         numDescriptors = static_cast<deUint32>(bindings.size());

		for (deUint32 descriptorIdx  = 0u; descriptorIdx < numDescriptors; ++descriptorIdx)
		{
			const auto& binding = bindings[descriptorIdx];
			checks << binding.glslCheckStatements(iteration, setNum, bindingNum, baseValue_ + descriptorIdx, tcu::just(descriptorIdx), usePushConstants);
		}

		return checks.str();
	}
};

class DescriptorSet;

using DescriptorSetPtr = de::SharedPtr<DescriptorSet>;

class DescriptorSet
{
public:
	using BindingInterfacePtr   = de::MovePtr<BindingInterface>;
	using BindingPtrVector      = std::vector<BindingInterfacePtr>;

private:
	BindingPtrVector bindings;

public:
	explicit DescriptorSet (BindingPtrVector& bindings_)
		: bindings(std::move(bindings_))
	{
		DE_ASSERT(!bindings.empty());
	}

	size_t numBindings () const
	{
		return bindings.size();
	}

	const BindingInterface* getBinding (size_t bindingIdx) const
	{
		return bindings.at(bindingIdx).get();
	}

	// Maximum number of descriptor types used by any binding in the set.
	deUint32 maxTypes () const
	{
		std::vector<deUint32> maxSizes;
		maxSizes.reserve(bindings.size());

		std::transform(begin(bindings), end(bindings), std::back_inserter(maxSizes),
		               [] (const BindingInterfacePtr& b) { return b->maxTypes(); });

		const auto maxElement = std::max_element(begin(maxSizes), end(maxSizes));
		DE_ASSERT(maxElement != end(maxSizes));
		return *maxElement;
	}

	// Create another descriptor set that can be the source for copies when setting descriptor values.
	DescriptorSetPtr genSourceSet (SourceSetStrategy strategy, deUint32 iteration) const
	{
		BindingPtrVector newBindings;
		for (const auto& b : bindings)
		{
			if (strategy == SourceSetStrategy::MUTABLE)
				newBindings.push_back(b->toMutable(iteration));
			else
				newBindings.push_back(b->toNonMutable(iteration));
		}

		return DescriptorSetPtr(new DescriptorSet(newBindings));
	}

	// Makes a descriptor pool that can be used when allocating descriptors for this set.
	Move<VkDescriptorPool> makeDescriptorPool (const DeviceInterface& vkd, VkDevice device, PoolMutableStrategy strategy, VkDescriptorPoolCreateFlags flags) const
	{
		std::vector<VkDescriptorPoolSize>             poolSizes;
		std::vector<std::vector<VkDescriptorType>>    mutableTypesVec;
		std::vector<VkMutableDescriptorTypeListVALVE> mutableTypeLists;

		// Make vector element addresses stable.
		const auto bindingCount = numBindings();
		poolSizes.reserve(bindingCount);
		mutableTypesVec.reserve(bindingCount);
		mutableTypeLists.reserve(bindingCount);

		for (const auto& b : bindings)
		{
			const auto                 mainType = b->mainType();
			const VkDescriptorPoolSize poolSize = {
				mainType,
				static_cast<deUint32>(b->size()),
			};
			poolSizes.push_back(poolSize);

			if (strategy == PoolMutableStrategy::KEEP_TYPES || strategy == PoolMutableStrategy::EXPAND_TYPES)
			{
				if (mainType == VK_DESCRIPTOR_TYPE_MUTABLE_VALVE)
				{
					if (strategy == PoolMutableStrategy::KEEP_TYPES)
					{
						mutableTypesVec.emplace_back(b->mutableTypes());
					}
					else
					{
						// Expand the type list with the mandatory types.
						static const auto mandatoryTypesFlags = toDescriptorTypeFlags(getMandatoryMutableTypes());
						const auto        bindingTypes        = toDescriptorTypeVector(mandatoryTypesFlags | toDescriptorTypeFlags(b->mutableTypes()));

						mutableTypesVec.emplace_back(bindingTypes);
					}

					const auto& lastVec = mutableTypesVec.back();
					const VkMutableDescriptorTypeListVALVE typeList = { static_cast<deUint32>(lastVec.size()), de::dataOrNull(lastVec) };
					mutableTypeLists.push_back(typeList);
				}
				else
				{
					const VkMutableDescriptorTypeListVALVE typeList = { 0u, nullptr };
					mutableTypeLists.push_back(typeList);
				}
			}
			else if (strategy == PoolMutableStrategy::NO_TYPES)
				; // Do nothing, we will not use any type list.
			else
				DE_ASSERT(false);
		}

		VkDescriptorPoolCreateInfo poolCreateInfo = initVulkanStructure();

		poolCreateInfo.maxSets       = 1u;
		poolCreateInfo.flags         = flags;
		poolCreateInfo.poolSizeCount = static_cast<deUint32>(poolSizes.size());
		poolCreateInfo.pPoolSizes    = de::dataOrNull(poolSizes);

		VkMutableDescriptorTypeCreateInfoVALVE mutableInfo = initVulkanStructure();

		if (strategy == PoolMutableStrategy::KEEP_TYPES || strategy == PoolMutableStrategy::EXPAND_TYPES)
		{
			mutableInfo.mutableDescriptorTypeListCount = static_cast<deUint32>(mutableTypeLists.size());
			mutableInfo.pMutableDescriptorTypeLists    = de::dataOrNull(mutableTypeLists);
			poolCreateInfo.pNext                       = &mutableInfo;
		}

		return createDescriptorPool(vkd, device, &poolCreateInfo);
	}

private:
	// Building the descriptor set layout create info structure is cumbersome, so we'll reuse the same procedure to check support
	// and create the layout. This structure contains the result. "supported" is created as an enum to avoid the Move<> to bool
	// conversion cast in the contructors.
	struct DescriptorSetLayoutResult
	{
		enum class LayoutSupported { NO = 0, YES };

		LayoutSupported             supported;
		Move<VkDescriptorSetLayout> layout;

		explicit DescriptorSetLayoutResult (Move<VkDescriptorSetLayout>&& layout_)
			: supported (LayoutSupported::YES)
			, layout    (layout_)
		{}

		explicit DescriptorSetLayoutResult (LayoutSupported supported_)
			: supported (supported_)
			, layout    ()
		{}
	};

	DescriptorSetLayoutResult makeOrCheckDescriptorSetLayout (bool checkOnly, const DeviceInterface& vkd, VkDevice device, VkShaderStageFlags stageFlags, VkDescriptorSetLayoutCreateFlags createFlags) const
	{
		const auto                                    numIterations = maxTypes();
		std::vector<VkDescriptorSetLayoutBinding>     bindingsVec;
		std::vector<std::vector<VkDescriptorType>>    mutableTypesVec;
		std::vector<VkMutableDescriptorTypeListVALVE> mutableTypeLists;

		// Make vector element addresses stable.
		const auto bindingCount = numBindings();
		bindingsVec.reserve(bindingCount);
		mutableTypesVec.reserve(bindingCount);
		mutableTypeLists.reserve(bindingCount);

		for (size_t bindingIdx = 0u; bindingIdx < bindings.size(); ++bindingIdx)
		{
			const auto& binding = bindings[bindingIdx];
			const auto mainType = binding->mainType();

			const VkDescriptorSetLayoutBinding layoutBinding = {
				static_cast<deUint32>(bindingIdx),        //    deUint32			binding;
				mainType,                                 //    VkDescriptorType	descriptorType;
				static_cast<deUint32>(binding->size()),   //    deUint32			descriptorCount;
				stageFlags,                               //    VkShaderStageFlags	stageFlags;
				nullptr,                                  //    const VkSampler*	pImmutableSamplers;
			};
			bindingsVec.push_back(layoutBinding);

			// This list may be empty for non-mutable types, which is fine.
			mutableTypesVec.push_back(binding->mutableTypes());
			const auto& lastVec = mutableTypesVec.back();

			const VkMutableDescriptorTypeListVALVE typeList = {
				static_cast<deUint32>(lastVec.size()),  //  deUint32				descriptorTypeCount;
				de::dataOrNull(lastVec),                //  const VkDescriptorType*	pDescriptorTypes;
			};
			mutableTypeLists.push_back(typeList);
		}

		// Make sure to include the variable descriptor count and/or update after bind binding flags.
		const bool        updateAfterBind = ((createFlags & VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT) != 0u);
		bool              lastIsUnbounded = false;
		bool              aliasingNeded   = false;
		std::vector<bool> bindingNeedsAliasing(bindings.size(), false);

		for (size_t bindingIdx = 0; bindingIdx < bindings.size(); ++bindingIdx)
		{
			if (bindingIdx < bindings.size() - 1)
				DE_ASSERT(!bindings[bindingIdx]->isUnbounded());
			else
				lastIsUnbounded = bindings[bindingIdx]->isUnbounded();

			if (bindings[bindingIdx]->needsAliasingUpTo(numIterations))
			{
				bindingNeedsAliasing[bindingIdx] = true;
				aliasingNeded = true;
			}
		}

		using FlagsCreateInfoPtr = de::MovePtr<VkDescriptorSetLayoutBindingFlagsCreateInfo>;
		using BindingFlagsVecPtr = de::MovePtr<std::vector<VkDescriptorBindingFlags>>;

		FlagsCreateInfoPtr flagsCreateInfo;
		BindingFlagsVecPtr bindingFlagsVec;

		if (updateAfterBind || lastIsUnbounded || aliasingNeded)
		{
			flagsCreateInfo = FlagsCreateInfoPtr(new VkDescriptorSetLayoutBindingFlagsCreateInfo);
			*flagsCreateInfo = initVulkanStructure();

			bindingFlagsVec = BindingFlagsVecPtr(new std::vector<VkDescriptorBindingFlags>(bindingsVec.size(), (updateAfterBind ? VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT : 0)));
			if (lastIsUnbounded)
				bindingFlagsVec->back() |= VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;

			for (size_t bindingIdx = 0; bindingIdx < bindings.size(); ++bindingIdx)
			{
				if (bindingNeedsAliasing[bindingIdx])
					bindingFlagsVec->at(bindingIdx) |= VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
			}

			flagsCreateInfo->bindingCount  = static_cast<deUint32>(bindingFlagsVec->size());
			flagsCreateInfo->pBindingFlags = de::dataOrNull(*bindingFlagsVec);
		}

		const VkMutableDescriptorTypeCreateInfoVALVE createInfoValve = {
			VK_STRUCTURE_TYPE_MUTABLE_DESCRIPTOR_TYPE_CREATE_INFO_VALVE,
			flagsCreateInfo.get(),
			static_cast<deUint32>(mutableTypeLists.size()),
			de::dataOrNull(mutableTypeLists),
		};

		const VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,    //  VkStructureType						sType;
			&createInfoValve,                                       //  const void*							pNext;
			createFlags,                                            //  VkDescriptorSetLayoutCreateFlags	flags;
			static_cast<deUint32>(bindingsVec.size()),              //  deUint32							bindingCount;
			de::dataOrNull(bindingsVec),                            //  const VkDescriptorSetLayoutBinding*	pBindings;
		};

		if (checkOnly)
		{
			VkDescriptorSetLayoutSupport support = initVulkanStructure();
			vkd.getDescriptorSetLayoutSupport(device, &layoutCreateInfo, &support);
			DescriptorSetLayoutResult result((support.supported == VK_TRUE) ? DescriptorSetLayoutResult::LayoutSupported::YES
			                                                                : DescriptorSetLayoutResult::LayoutSupported::NO);
			return result;
		}
		else
		{
			DescriptorSetLayoutResult result(createDescriptorSetLayout(vkd, device, &layoutCreateInfo));
			return result;
		}
	}

public:
	Move<VkDescriptorSetLayout> makeDescriptorSetLayout (const DeviceInterface& vkd, VkDevice device, VkShaderStageFlags stageFlags, VkDescriptorSetLayoutCreateFlags createFlags) const
	{
		return makeOrCheckDescriptorSetLayout(false /*checkOnly*/, vkd, device, stageFlags, createFlags).layout;
	}

	bool checkDescriptorSetLayout (const DeviceInterface& vkd, VkDevice device, VkShaderStageFlags stageFlags, VkDescriptorSetLayoutCreateFlags createFlags) const
	{
		return (makeOrCheckDescriptorSetLayout(true /*checkOnly*/, vkd, device, stageFlags, createFlags).supported == DescriptorSetLayoutResult::LayoutSupported::YES);
	}

	size_t numDescriptors () const
	{
		size_t total = 0;
		for (const auto& b : bindings)
			total += b->size();
		return total;
	}

	std::vector<Resource> createResources (const DeviceInterface& vkd, VkDevice device, Allocator& alloc, deUint32 qIndex, VkQueue queue, deUint32 iteration, bool useAABBs) const
	{
		// Create resources for each binding.
		std::vector<Resource> result;
		result.reserve(numDescriptors());

		const auto bindingsCount = static_cast<deUint32>(bindings.size());

		for (deUint32 bindingIdx = 0u; bindingIdx < bindingsCount; ++bindingIdx)
		{
			const auto& binding             = bindings[bindingIdx];
			auto        bindingResources    = binding->createResources(vkd, device, alloc, qIndex, queue, iteration, useAABBs, getDescriptorNumericValue(iteration, bindingIdx));

			for (auto& resource : bindingResources)
				result.emplace_back(std::move(resource));
		}

		return result;
	}

	// Updates a descriptor set with the given resources. Note: the set must have been created with a layout that's compatible with this object.
	void updateDescriptorSet (const DeviceInterface& vkd, VkDevice device, VkDescriptorSet set, deUint32 iteration, const std::vector<Resource>& resources) const
	{
		// Make sure the number of resources is correct.
		const auto numResources = resources.size();
		DE_ASSERT(numDescriptors() == numResources);

		std::vector<VkWriteDescriptorSet> descriptorWrites;
		descriptorWrites.reserve(numResources);

		std::vector<VkDescriptorImageInfo>                          imageInfoVec;
		std::vector<VkDescriptorBufferInfo>                         bufferInfoVec;
		std::vector<VkBufferView>                                   bufferViewVec;
		std::vector<VkWriteDescriptorSetAccelerationStructureKHR>   asWriteVec;
		size_t                                                      resourceIdx = 0;

		// We'll be storing pointers to elements of these vectors as we're appending elements, so we need their addresses to be stable.
		imageInfoVec.reserve(numResources);
		bufferInfoVec.reserve(numResources);
		bufferViewVec.reserve(numResources);
		asWriteVec.reserve(numResources);

		for (size_t bindingIdx = 0; bindingIdx < bindings.size(); ++bindingIdx)
		{
			const auto& binding         = bindings[bindingIdx];
			const auto  descriptorTypes = binding->typesAtIteration(iteration);

			for (size_t descriptorIdx = 0; descriptorIdx < binding->size(); ++descriptorIdx)
			{
				// Make sure the resource type matches the expected value.
				const auto& resource       = resources[resourceIdx];
				const auto& descriptorType = descriptorTypes[descriptorIdx];

				DE_ASSERT(resource.descriptorType == descriptorType);

				// Obtain the descriptor write info for the resource.
				const auto writeInfo = resource.makeWriteInfo();

				switch (writeInfo.writeType)
				{
				case WriteType::IMAGE_INFO:                  imageInfoVec.push_back(writeInfo.imageInfo);   break;
				case WriteType::BUFFER_INFO:                 bufferInfoVec.push_back(writeInfo.bufferInfo); break;
				case WriteType::BUFFER_VIEW:                 bufferViewVec.push_back(writeInfo.bufferView); break;
				case WriteType::ACCELERATION_STRUCTURE_INFO: asWriteVec.push_back(writeInfo.asInfo);        break;
				default: DE_ASSERT(false); break;
				}

				// Add a new VkWriteDescriptorSet struct or extend the last one with more info. This helps us exercise different implementation code paths.
				bool extended = false;

				if (!descriptorWrites.empty() && descriptorIdx > 0)
				{
					auto& last = descriptorWrites.back();
					if (last.dstSet == set /* this should always be true */ &&
					    last.dstBinding == bindingIdx && (last.dstArrayElement + last.descriptorCount) == descriptorIdx &&
					    last.descriptorType == descriptorType &&
					    writeInfo.writeType != WriteType::ACCELERATION_STRUCTURE_INFO)
					{
						// The new write should be in the same vector (imageInfoVec, bufferInfoVec or bufferViewVec) so increasing the count works.
						++last.descriptorCount;
						extended = true;
					}
				}

				if (!extended)
				{
					const VkWriteDescriptorSet write = {
						VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
						((writeInfo.writeType == WriteType::ACCELERATION_STRUCTURE_INFO) ? &asWriteVec.back() : nullptr),
						set,
						static_cast<deUint32>(bindingIdx),
						static_cast<deUint32>(descriptorIdx),
						1u,
						descriptorType,
						(writeInfo.writeType == WriteType::IMAGE_INFO  ? &imageInfoVec.back()  : nullptr),
						(writeInfo.writeType == WriteType::BUFFER_INFO ? &bufferInfoVec.back() : nullptr),
						(writeInfo.writeType == WriteType::BUFFER_VIEW ? &bufferViewVec.back() : nullptr),
					};
					descriptorWrites.push_back(write);
				}

				++resourceIdx;
			}
		}

		// Finally, update descriptor set with all the writes.
		vkd.updateDescriptorSets(device, static_cast<deUint32>(descriptorWrites.size()), de::dataOrNull(descriptorWrites), 0u, nullptr);
	}

	// Copies between descriptor sets. They must be compatible and related to this set.
	void copyDescriptorSet (const DeviceInterface& vkd, VkDevice device, VkDescriptorSet srcSet, VkDescriptorSet dstSet) const
	{
		std::vector<VkCopyDescriptorSet> copies;

		for (size_t bindingIdx = 0; bindingIdx < numBindings(); ++bindingIdx)
		{
			const auto& binding         = getBinding(bindingIdx);
			const auto  bindingNumber   = static_cast<deUint32>(bindingIdx);
			const auto  descriptorCount = static_cast<deUint32>(binding->size());

			const VkCopyDescriptorSet copy =
			{
				VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET,
				nullptr,
				// set, binding, array element.
				srcSet, bindingNumber, 0u,
				dstSet, bindingNumber, 0u,
				descriptorCount,
			};

			copies.push_back(copy);
		}

		vkd.updateDescriptorSets(device, 0u, nullptr, static_cast<deUint32>(copies.size()), de::dataOrNull(copies));
	}

	// Does any binding in the set need aliasing in a given iteration?
	bool needsAliasing (deUint32 iteration) const
	{
		std::vector<bool> aliasingNeededFlags;
		aliasingNeededFlags.reserve(bindings.size());

		std::transform(begin(bindings), end(bindings), std::back_inserter(aliasingNeededFlags),
		               [iteration] (const BindingInterfacePtr& b) { return b->needsAliasing(iteration); });
		return std::any_of(begin(aliasingNeededFlags), end(aliasingNeededFlags), [] (bool f) { return f; });
	}

	// Does any binding in the set need aliasing in any iteration?
	bool needsAnyAliasing () const
	{
		const auto        numIterations       = maxTypes();
		std::vector<bool> aliasingNeededFlags (numIterations, false);

		for (deUint32 iteration = 0; iteration < numIterations; ++iteration)
		    aliasingNeededFlags[iteration] = needsAliasing(iteration);

		return std::any_of(begin(aliasingNeededFlags), end(aliasingNeededFlags), [] (bool f) { return f; });
	}

	// Is the last binding an unbounded array?
	bool lastBindingIsUnbounded () const
	{
		if (bindings.empty())
			return false;
		return bindings.back()->isUnbounded();
	}

	// Get the variable descriptor count for the last binding if any.
	tcu::Maybe<deUint32> getVariableDescriptorCount () const
	{
		if (lastBindingIsUnbounded())
			return tcu::just(static_cast<deUint32>(bindings.back()->size()));
		return tcu::nothing<deUint32>();
	}

	// Check if the set contains a descriptor type of the given type at the given iteration.
	bool containsTypeAtIteration (VkDescriptorType descriptorType, deUint32 iteration) const
	{
		return std::any_of(begin(bindings), end(bindings),
		                   [descriptorType, iteration] (const BindingInterfacePtr& b) {
			                   const auto types = b->typesAtIteration(iteration);
			                   return de::contains(begin(types), end(types), descriptorType);
		                   });
	}

	// Is any binding an array?
	bool hasArrays () const
	{
		return std::any_of(begin(bindings), end(bindings), [] (const BindingInterfacePtr& b) { return b->isArray(); });
	}
};

enum class UpdateType
{
	WRITE = 0,
	COPY,
};

enum class SourceSetType
{
	NORMAL = 0,
	HOST_ONLY,
	NO_SOURCE,
};

enum class UpdateMoment
{
	NORMAL = 0,
	UPDATE_AFTER_BIND,
};

enum class TestingStage
{
	COMPUTE = 0,
	VERTEX,
	TESS_EVAL,
	TESS_CONTROL,
	GEOMETRY,
	FRAGMENT,
	RAY_GEN,
	INTERSECTION,
	ANY_HIT,
	CLOSEST_HIT,
	MISS,
	CALLABLE,
};

enum class ArrayAccessType
{
	CONSTANT = 0,
	PUSH_CONSTANT,
	NO_ARRAY,
};

// Are we testing a ray tracing pipeline stage?
bool isRayTracingStage (TestingStage stage)
{
	switch (stage)
	{
	case TestingStage::RAY_GEN:
	case TestingStage::INTERSECTION:
	case TestingStage::ANY_HIT:
	case TestingStage::CLOSEST_HIT:
	case TestingStage::MISS:
	case TestingStage::CALLABLE:
		return true;
		break;
	default:
		break;
	}

	return false;
}

struct TestParams
{
	DescriptorSetPtr    descriptorSet;
	UpdateType          updateType;
	SourceSetStrategy   sourceSetStrategy;
	SourceSetType       sourceSetType;
	PoolMutableStrategy poolMutableStrategy;
	UpdateMoment        updateMoment;
	ArrayAccessType     arrayAccessType;
	TestingStage        testingStage;

	VkShaderStageFlags getStageFlags () const
	{
		VkShaderStageFlags flags = 0u;

		switch (testingStage)
		{
		case TestingStage::COMPUTE:			flags |= VK_SHADER_STAGE_COMPUTE_BIT;					break;
		case TestingStage::VERTEX:			flags |= VK_SHADER_STAGE_VERTEX_BIT;					break;
		case TestingStage::TESS_EVAL:		flags |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;	break;
		case TestingStage::TESS_CONTROL:	flags |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;		break;
		case TestingStage::GEOMETRY:		flags |= VK_SHADER_STAGE_GEOMETRY_BIT;					break;
		case TestingStage::FRAGMENT:		flags |= VK_SHADER_STAGE_FRAGMENT_BIT;					break;
		case TestingStage::RAY_GEN:			flags |= VK_SHADER_STAGE_RAYGEN_BIT_KHR;				break;
		case TestingStage::INTERSECTION:	flags |= VK_SHADER_STAGE_INTERSECTION_BIT_KHR;			break;
		case TestingStage::ANY_HIT:			flags |= VK_SHADER_STAGE_ANY_HIT_BIT_KHR;				break;
		case TestingStage::CLOSEST_HIT:		flags |= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;			break;
		case TestingStage::MISS:			flags |= VK_SHADER_STAGE_MISS_BIT_KHR;					break;
		case TestingStage::CALLABLE:		flags |= VK_SHADER_STAGE_CALLABLE_BIT_KHR;				break;
		default:
			DE_ASSERT(false);
			break;
		}

		return flags;
	}

	VkPipelineStageFlags getPipelineWriteStage () const
	{
		VkPipelineStageFlags flags = 0u;

		switch (testingStage)
		{
		case TestingStage::COMPUTE:			flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;					break;
		case TestingStage::VERTEX:			flags |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;					break;
		case TestingStage::TESS_EVAL:		flags |= VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;	break;
		case TestingStage::TESS_CONTROL:	flags |= VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT;		break;
		case TestingStage::GEOMETRY:		flags |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;					break;
		case TestingStage::FRAGMENT:		flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;					break;
		case TestingStage::RAY_GEN:			// fallthrough
		case TestingStage::INTERSECTION:	// fallthrough
		case TestingStage::ANY_HIT:			// fallthrough
		case TestingStage::CLOSEST_HIT:		// fallthrough
		case TestingStage::MISS:			// fallthrough
		case TestingStage::CALLABLE:		flags |= VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;			break;
		default:
			DE_ASSERT(false);
			break;
		}

		return flags;
	}

private:
	VkDescriptorSetLayoutCreateFlags getLayoutCreateFlags (bool isSourceSet) const
	{
		// UPDATE_AFTER_BIND cannot be used with HOST_ONLY sets.
		//DE_ASSERT(!(updateMoment == UpdateMoment::UPDATE_AFTER_BIND && sourceSetType == SourceSetType::HOST_ONLY));

		VkDescriptorSetLayoutCreateFlags createFlags = 0u;

		if ((!isSourceSet || sourceSetType != SourceSetType::HOST_ONLY) && updateMoment == UpdateMoment::UPDATE_AFTER_BIND)
			createFlags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

		if (isSourceSet && sourceSetType == SourceSetType::HOST_ONLY)
			createFlags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_HOST_ONLY_POOL_BIT_VALVE;

		return createFlags;
	}

public:
	VkDescriptorSetLayoutCreateFlags getSrcLayoutCreateFlags () const
	{
		return getLayoutCreateFlags(true);
	}

	VkDescriptorSetLayoutCreateFlags getDstLayoutCreateFlags () const
	{
		return getLayoutCreateFlags(false);
	}

private:
	VkDescriptorPoolCreateFlags getPoolCreateFlags (bool isSourceSet) const
	{
		// UPDATE_AFTER_BIND cannot be used with HOST_ONLY sets.
		//DE_ASSERT(!(updateMoment == UpdateMoment::UPDATE_AFTER_BIND && sourceSetType == SourceSetType::HOST_ONLY));

		VkDescriptorPoolCreateFlags poolCreateFlags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

		if ((!isSourceSet || sourceSetType != SourceSetType::HOST_ONLY) && updateMoment == UpdateMoment::UPDATE_AFTER_BIND)
			poolCreateFlags |= VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;

		if (isSourceSet && sourceSetType == SourceSetType::HOST_ONLY)
			poolCreateFlags |= VK_DESCRIPTOR_POOL_CREATE_HOST_ONLY_BIT_VALVE;

		return poolCreateFlags;
	}

public:
	VkDescriptorPoolCreateFlags getSrcPoolCreateFlags () const
	{
		return getPoolCreateFlags(true);
	}

	VkDescriptorPoolCreateFlags getDstPoolCreateFlags () const
	{
		return getPoolCreateFlags(false);
	}

	VkPipelineBindPoint getBindPoint () const
	{
		if (testingStage == TestingStage::COMPUTE)
			return VK_PIPELINE_BIND_POINT_COMPUTE;
		if (isRayTracingStage(testingStage))
			return VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;
		return VK_PIPELINE_BIND_POINT_GRAPHICS;
	}
};

class MutableTypesTest : public TestCase
{
public:
	MutableTypesTest (tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params)
		: TestCase(testCtx, name, description)
		, m_params(params)
	{}

	~MutableTypesTest () override = default;

	void            initPrograms        (vk::SourceCollections& programCollection) const override;
	TestInstance*   createInstance      (Context& context) const override;
	void            checkSupport        (Context& context) const override;

private:
	TestParams      m_params;
};

class MutableTypesInstance : public TestInstance
{
public:
	MutableTypesInstance (Context& context, const TestParams& params)
		: TestInstance  (context)
		, m_params      (params)
	{}

	~MutableTypesInstance () override = default;

	tcu::TestStatus iterate () override;

private:
	TestParams      m_params;
};

// Check if a descriptor set contains a given descriptor type in any iteration up to maxTypes().
bool containsAnyDescriptorType (const DescriptorSet& descriptorSet, VkDescriptorType descriptorType)
{
	const auto numIterations = descriptorSet.maxTypes();

	for (deUint32 iter = 0u; iter < numIterations; ++iter)
	{
		if (descriptorSet.containsTypeAtIteration(descriptorType, iter))
			return true;
	}

	return false;
}

// Check if testing this descriptor set needs an external image (for sampler descriptors).
bool needsExternalImage (const DescriptorSet& descriptorSet)
{
	return containsAnyDescriptorType(descriptorSet, VK_DESCRIPTOR_TYPE_SAMPLER);
}

// Check if testing this descriptor set needs an external sampler (for sampled images).
bool needsExternalSampler (const DescriptorSet& descriptorSet)
{
	return containsAnyDescriptorType(descriptorSet, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
}

// Check if this descriptor set contains a input attachments.
bool usesInputAttachments (const DescriptorSet& descriptorSet)
{
	return containsAnyDescriptorType(descriptorSet, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);
}

// Check if this descriptor set contains acceleration structures.
bool usesAccelerationStructures (const DescriptorSet& descriptorSet)
{
	return containsAnyDescriptorType(descriptorSet, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
}

std::string shaderName (deUint32 iteration)
{
	return ("iteration-" + de::toString(iteration));
}

void MutableTypesTest::initPrograms (vk::SourceCollections& programCollection) const
{
	const bool						usePushConstants      = (m_params.arrayAccessType == ArrayAccessType::PUSH_CONSTANT);
	const bool						useExternalImage      = needsExternalImage(*m_params.descriptorSet);
	const bool						useExternalSampler    = needsExternalSampler(*m_params.descriptorSet);
	const bool						rayQueries            = usesAccelerationStructures(*m_params.descriptorSet);
	const bool						rayTracing            = isRayTracingStage(m_params.testingStage);
	const auto						numIterations         = m_params.descriptorSet->maxTypes();
	const auto						numBindings           = m_params.descriptorSet->numBindings();
	const vk::ShaderBuildOptions	rtBuildOptions        (programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

	// Extra set and bindings for external resources.
	std::ostringstream extraSet;
	deUint32           extraBindings = 0u;

	extraSet << "layout (set=1, binding=" << extraBindings++ << ") buffer OutputBufferBlock { uint value[" << numIterations << "]; } outputBuffer;\n";
	if (useExternalImage)
		extraSet << "layout (set=1, binding=" << extraBindings++ << ") uniform utexture2D externalSampledImage;\n";
	if (useExternalSampler)
		extraSet << "layout (set=1, binding=" << extraBindings++ << ") uniform sampler externalSampler;\n";
	// The extra binding below will be declared in the "passthrough" ray generation shader.
#if 0
	if (rayTracing)
		extraSet << "layout (set=1, binding=" << extraBindings++ << ") uniform accelerationStructureEXT externalAS;\n";
#endif

	// Common vertex preamble.
	std::ostringstream vertexPreamble;
	vertexPreamble
			<< "vec2 vertexPositions[3] = vec2[](\n"
			<< "  vec2(0.0, -0.5),\n"
			<< "  vec2(0.5, 0.5),\n"
			<< "  vec2(-0.5, 0.5)\n"
			<< ");\n"
			;

	// Vertex shader body common statements.
	std::ostringstream vertexBodyCommon;
	vertexBodyCommon << "  gl_Position = vec4(vertexPositions[gl_VertexIndex], 0.0, 1.0);\n";

	// Common tessellation control preamble.
	std::ostringstream tescPreamble;
	tescPreamble
		<< "layout (vertices=3) out;\n"
		<< "in gl_PerVertex\n"
		<< "{\n"
		<< "  vec4 gl_Position;\n"
		<< "} gl_in[gl_MaxPatchVertices];\n"
		<< "out gl_PerVertex\n"
		<< "{\n"
		<< "  vec4 gl_Position;\n"
		<< "} gl_out[];\n"
		;

	// Common tessellation control body.
	std::ostringstream tescBodyCommon;
	tescBodyCommon
		<< "  gl_TessLevelInner[0] = 1.0;\n"
		<< "  gl_TessLevelInner[1] = 1.0;\n"
		<< "  gl_TessLevelOuter[0] = 1.0;\n"
		<< "  gl_TessLevelOuter[1] = 1.0;\n"
		<< "  gl_TessLevelOuter[2] = 1.0;\n"
		<< "  gl_TessLevelOuter[3] = 1.0;\n"
		<< "  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
		;

	// Common tessellation evaluation preamble.
	std::ostringstream tesePreamble;
	tesePreamble
		<< "layout (triangles, fractional_odd_spacing, cw) in;\n"
		<< "in gl_PerVertex\n"
		<< "{\n"
		<< "  vec4 gl_Position;\n"
		<< "} gl_in[gl_MaxPatchVertices];\n"
		<< "out gl_PerVertex\n"
		<< "{\n"
		<< "  vec4 gl_Position;\n"
		<< "};\n"
		;

	// Common tessellation evaluation body.
	std::ostringstream teseBodyCommon;
	teseBodyCommon
		<< "  gl_Position = (gl_TessCoord.x * gl_in[0].gl_Position) +\n"
		<< "                (gl_TessCoord.y * gl_in[1].gl_Position) +\n"
		<< "                (gl_TessCoord.z * gl_in[2].gl_Position);\n"
		;

	// Shader preamble.
	std::ostringstream preamble;

	preamble
		<< "#version 460\n"
		<< "#extension GL_EXT_nonuniform_qualifier : enable\n"
		<< "#extension GL_EXT_debug_printf : enable\n"
		<< (rayTracing ? "#extension GL_EXT_ray_tracing : enable\n" : "")
		<< (rayQueries ? "#extension GL_EXT_ray_query : enable\n" : "")
		<< "\n"
		;

	if (m_params.testingStage == TestingStage::VERTEX)
	{
		preamble << vertexPreamble.str();
	}
	else if (m_params.testingStage == TestingStage::COMPUTE)
	{
		preamble
			<< "layout (local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
			<< "\n"
			;
	}
	else if (m_params.testingStage == TestingStage::GEOMETRY)
	{
		preamble
			<< "layout (triangles) in;\n"
			<< "layout (triangle_strip, max_vertices=3) out;\n"
			<< "in gl_PerVertex\n"
			<< "{\n"
			<< "  vec4 gl_Position;\n"
			<< "} gl_in[3];\n"
			<< "out gl_PerVertex\n"
			<< "{\n"
			<< "  vec4 gl_Position;\n"
			<< "};\n"
			;
	}
	else if (m_params.testingStage == TestingStage::TESS_CONTROL)
	{
		preamble << tescPreamble.str();
	}
	else if (m_params.testingStage == TestingStage::TESS_EVAL)
	{
		preamble << tesePreamble.str();
	}
	else if (m_params.testingStage == TestingStage::CALLABLE)
	{
		preamble << "layout (location=0) callableDataInEXT float unusedCallableData;\n";
	}
	else if (m_params.testingStage == TestingStage::CLOSEST_HIT ||
			 m_params.testingStage == TestingStage::ANY_HIT ||
			 m_params.testingStage == TestingStage::MISS)
	{
		preamble << "layout (location=0) rayPayloadInEXT float unusedRayPayload;\n";
	}
	else if (m_params.testingStage == TestingStage::INTERSECTION)
	{
		preamble << "hitAttributeEXT vec3 hitAttribute;\n";
	}

	preamble << extraSet.str();
	if (usePushConstants)
		preamble << "layout (push_constant, std430) uniform PushConstantBlock { uint zero; } pc;\n";
	preamble << "\n";

	// We need to create a shader per iteration.
	for (deUint32 iter = 0u; iter < numIterations; ++iter)
	{
		// Shader preamble.
		std::ostringstream shader;
		shader << preamble.str();

		deUint32 inputAttachmentCount = 0u;

		// Descriptor declarations for this iteration.
		for (size_t bindingIdx = 0; bindingIdx < numBindings; ++bindingIdx)
		{
			DE_ASSERT(bindingIdx <= std::numeric_limits<deUint32>::max());

			const auto binding            = m_params.descriptorSet->getBinding(bindingIdx);
			const auto bindingTypes       = binding->typesAtIteration(iter);
			const auto hasInputAttachment = de::contains(begin(bindingTypes), end(bindingTypes), VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);
			const auto isArray            = binding->isArray();
			const auto isUnbounded        = binding->isUnbounded();
			const auto bindingSize        = binding->size();

			// If the binding is an input attachment, make sure it's not an array.
			DE_ASSERT(!hasInputAttachment || !isArray);

			// Make sure the descriptor count fits a deInt32 if needed.
			DE_ASSERT(!isArray || isUnbounded || bindingSize <= static_cast<size_t>(std::numeric_limits<deInt32>::max()));

			const auto arraySize = (isArray ? (isUnbounded ? tcu::just(deInt32{-1}) : tcu::just(static_cast<deInt32>(bindingSize)))
			                                : tcu::nothing<deInt32>());

			shader << binding->glslDeclarations(iter, 0u, static_cast<deUint32>(bindingIdx), inputAttachmentCount, arraySize);

			if (hasInputAttachment)
				++inputAttachmentCount;
		}

		// Main body.
		shader
			<< "\n"
			<< "void main() {\n"
			// This checks if we are the first invocation to arrive here, so the checks are executed only once.
			<< "  const uint flag = atomicCompSwap(outputBuffer.value[" << iter << "], 0u, 1u);\n"
			<< "  if (flag == 0u) {\n"
			<< "    uint anyError = 0u;\n"
			;

		for (size_t bindingIdx = 0; bindingIdx < numBindings; ++bindingIdx)
		{
			const auto binding = m_params.descriptorSet->getBinding(bindingIdx);
			const auto idx32 = static_cast<deUint32>(bindingIdx);
			shader << binding->glslCheckStatements(iter, 0u, idx32, getDescriptorNumericValue(iter, idx32), tcu::nothing<deUint32>(), usePushConstants);
		}

		shader
			<< "    if (anyError == 0u) {\n"
			<< "      atomicAdd(outputBuffer.value[" << iter << "], 1u);\n"
			<< "    }\n"
			<< "  }\n" // Closes if (flag == 0u).
			;

		if (m_params.testingStage == TestingStage::VERTEX)
		{
			shader << vertexBodyCommon.str();
		}
		else if (m_params.testingStage == TestingStage::GEOMETRY)
		{
			shader
				<< "  gl_Position = gl_in[0].gl_Position; EmitVertex();\n"
				<< "  gl_Position = gl_in[1].gl_Position; EmitVertex();\n"
				<< "  gl_Position = gl_in[2].gl_Position; EmitVertex();\n"
				;
		}
		else if (m_params.testingStage == TestingStage::TESS_CONTROL)
		{
			shader << tescBodyCommon.str();
		}
		else if (m_params.testingStage == TestingStage::TESS_EVAL)
		{
			shader << teseBodyCommon.str();
		}

		shader
			<< "}\n" // End of main().
			;

		{
			const auto	shaderNameStr	= shaderName(iter);
			const auto	shaderStr		= shader.str();
			auto&		glslSource		= programCollection.glslSources.add(shaderNameStr);

			if (m_params.testingStage == TestingStage::COMPUTE)
				glslSource << glu::ComputeSource(shaderStr);
			else if (m_params.testingStage == TestingStage::VERTEX)
				glslSource << glu::VertexSource(shaderStr);
			else if (m_params.testingStage == TestingStage::FRAGMENT)
				glslSource << glu::FragmentSource(shaderStr);
			else if (m_params.testingStage == TestingStage::GEOMETRY)
				glslSource << glu::GeometrySource(shaderStr);
			else if (m_params.testingStage == TestingStage::TESS_CONTROL)
				glslSource << glu::TessellationControlSource(shaderStr);
			else if (m_params.testingStage == TestingStage::TESS_EVAL)
				glslSource << glu::TessellationEvaluationSource(shaderStr);
			else if (m_params.testingStage == TestingStage::RAY_GEN)
				glslSource << glu::RaygenSource(updateRayTracingGLSL(shaderStr));
			else if (m_params.testingStage == TestingStage::INTERSECTION)
				glslSource << glu::IntersectionSource(updateRayTracingGLSL(shaderStr));
			else if (m_params.testingStage == TestingStage::ANY_HIT)
				glslSource << glu::AnyHitSource(updateRayTracingGLSL(shaderStr));
			else if (m_params.testingStage == TestingStage::CLOSEST_HIT)
				glslSource << glu::ClosestHitSource(updateRayTracingGLSL(shaderStr));
			else if (m_params.testingStage == TestingStage::MISS)
				glslSource << glu::MissSource(updateRayTracingGLSL(shaderStr));
			else if (m_params.testingStage == TestingStage::CALLABLE)
				glslSource << glu::CallableSource(updateRayTracingGLSL(shaderStr));
			else
				DE_ASSERT(false);

			if (rayTracing || rayQueries)
				glslSource << rtBuildOptions;
		}
	}

	if (m_params.testingStage == TestingStage::FRAGMENT
		|| m_params.testingStage == TestingStage::GEOMETRY
		|| m_params.testingStage == TestingStage::TESS_CONTROL
		|| m_params.testingStage == TestingStage::TESS_EVAL)
	{
		// Add passthrough vertex shader that works for points.
		std::ostringstream vertPassthrough;
		vertPassthrough
			<< "#version 460\n"
			<< "out gl_PerVertex\n"
			<< "{\n"
			<< "  vec4 gl_Position;\n"
			<< "};\n"
			<< vertexPreamble.str()
			<< "void main() {\n"
			<< vertexBodyCommon.str()
			<< "}\n"
			;
		programCollection.glslSources.add("vert") << glu::VertexSource(vertPassthrough.str());
	}

	if (m_params.testingStage == TestingStage::TESS_CONTROL)
	{
		// Add passthrough tessellation evaluation shader.
		std::ostringstream tesePassthrough;
		tesePassthrough
			<< "#version 460\n"
			<< tesePreamble.str()
			<< "void main (void)\n"
			<< "{\n"
			<< teseBodyCommon.str()
			<< "}\n"
			;

		programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(tesePassthrough.str());
	}

	if (m_params.testingStage == TestingStage::TESS_EVAL)
	{
		// Add passthrough tessellation control shader.
		std::ostringstream tescPassthrough;
		tescPassthrough
			<< "#version 460\n"
			<< tescPreamble.str()
			<< "void main (void)\n"
			<< "{\n"
			<< tescBodyCommon.str()
			<< "}\n"
			;

		programCollection.glslSources.add("tesc") << glu::TessellationControlSource(tescPassthrough.str());
	}

	if (rayTracing && m_params.testingStage != TestingStage::RAY_GEN)
	{
		// Add a "passthrough" ray generation shader.
		std::ostringstream rgen;
		rgen
			<< "#version 460 core\n"
			<< "#extension GL_EXT_ray_tracing : require\n"
			<< "layout (set=1, binding=" << extraBindings << ") uniform accelerationStructureEXT externalAS;\n"
			<< ((m_params.testingStage == TestingStage::CALLABLE)
				? "layout (location=0) callableDataEXT float unusedCallableData;\n"
				: "layout (location=0) rayPayloadEXT float unusedRayPayload;\n")
			<< "\n"
			<< "void main()\n"
			<< "{\n"
			;

		if (m_params.testingStage == TestingStage::INTERSECTION
			|| m_params.testingStage == TestingStage::ANY_HIT
			|| m_params.testingStage == TestingStage::CLOSEST_HIT
			|| m_params.testingStage == TestingStage::MISS)
		{
			// We need to trace rays in this case to get hits or misses.
			const auto zDir = ((m_params.testingStage == TestingStage::MISS) ? "-1.0" : "1.0");

			rgen
				<< "  const uint cullMask = 0xFF;\n"
				<< "  const float tMin = 1.0;\n"
				<< "  const float tMax = 10.0;\n"
				<< "  const vec3 origin = vec3(0.0, 0.0, 0.0);\n"
				<< "  const vec3 direction = vec3(0.0, 0.0, " << zDir << ");\n"
				<< "  traceRayEXT(externalAS, gl_RayFlagsNoneEXT, cullMask, 0, 0, 0, origin, tMin, direction, tMax, 0);\n"
				;

		}
		else if (m_params.testingStage == TestingStage::CALLABLE)
		{
			rgen << "  executeCallableEXT(0, 0);\n";
		}

		// End of main().
		rgen << "}\n";

		programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(rgen.str())) << rtBuildOptions;

		// Intersection shaders will ignore the intersection, so we need a passthrough miss shader.
		if (m_params.testingStage == TestingStage::INTERSECTION)
		{
			std::ostringstream miss;
			miss
				<< "#version 460 core\n"
				<< "#extension GL_EXT_ray_tracing : require\n"
				<< "layout (location=0) rayPayloadEXT float unusedRayPayload;\n"
				<< "\n"
				<< "void main()\n"
				<< "{\n"
				<< "}\n"
				;

			programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(miss.str())) << rtBuildOptions;
		}
	}
}

TestInstance* MutableTypesTest::createInstance (Context& context) const
{
	return new MutableTypesInstance(context, m_params);
}

void requirePartiallyBound (Context& context)
{
	context.requireDeviceFunctionality("VK_EXT_descriptor_indexing");
	const auto& indexingFeatures = context.getDescriptorIndexingFeatures();
	if (!indexingFeatures.descriptorBindingPartiallyBound)
		TCU_THROW(NotSupportedError, "Partially bound bindings not supported");
}

void requireVariableDescriptorCount (Context& context)
{
	context.requireDeviceFunctionality("VK_EXT_descriptor_indexing");
	const auto& indexingFeatures = context.getDescriptorIndexingFeatures();
	if (!indexingFeatures.descriptorBindingVariableDescriptorCount)
		TCU_THROW(NotSupportedError, "Variable descriptor count not supported");
}

// Calculates the set of used descriptor types for a given set and iteration count, for bindings matching a predicate.
std::set<VkDescriptorType> getUsedDescriptorTypes (const DescriptorSet& descriptorSet, deUint32 numIterations, bool (*predicate)(const BindingInterface* binding))
{
	std::set<VkDescriptorType> usedDescriptorTypes;

	for (size_t bindingIdx = 0; bindingIdx < descriptorSet.numBindings(); ++bindingIdx)
	{
		const auto bindingPtr = descriptorSet.getBinding(bindingIdx);
		if (predicate(bindingPtr))
		{
			for (deUint32 iter = 0u; iter < numIterations; ++iter)
			{
				const auto descTypes = bindingPtr->typesAtIteration(iter);
				usedDescriptorTypes.insert(begin(descTypes), end(descTypes));
			}
		}
	}

	return usedDescriptorTypes;
}

std::set<VkDescriptorType> getAllUsedDescriptorTypes (const DescriptorSet& descriptorSet, deUint32 numIterations)
{
	return getUsedDescriptorTypes(descriptorSet, numIterations, [] (const BindingInterface*) { return true; });
}

std::set<VkDescriptorType> getUsedArrayDescriptorTypes (const DescriptorSet& descriptorSet, deUint32 numIterations)
{
	return getUsedDescriptorTypes(descriptorSet, numIterations, [] (const BindingInterface* b) { return b->isArray(); });
}

// Are we testing a vertex pipeline stage?
bool isVertexStage (TestingStage stage)
{
	switch (stage)
	{
	case TestingStage::VERTEX:
	case TestingStage::TESS_CONTROL:
	case TestingStage::TESS_EVAL:
	case TestingStage::GEOMETRY:
		return true;
		break;
	default:
		break;
	}

	return false;
}

void MutableTypesTest::checkSupport (Context& context) const
{
	context.requireDeviceFunctionality("VK_VALVE_mutable_descriptor_type");

	// Check ray tracing if needed.
	const bool rayTracing = isRayTracingStage(m_params.testingStage);

	if (rayTracing)
	{
		context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
		context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");
	}

	// Check if ray queries are needed. Ray queries are used to verify acceleration structure descriptors.
	const bool rayQueriesNeeded = usesAccelerationStructures(*m_params.descriptorSet);
	if (rayQueriesNeeded)
	{
		context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
		context.requireDeviceFunctionality("VK_KHR_ray_query");
	}

	// We'll use iterations to check each mutable type, as needed.
	const auto numIterations = m_params.descriptorSet->maxTypes();

	if (m_params.descriptorSet->lastBindingIsUnbounded())
		requireVariableDescriptorCount(context);

	for (deUint32 iter = 0u; iter < numIterations; ++iter)
	{
		if (m_params.descriptorSet->needsAliasing(iter))
		{
			requirePartiallyBound(context);
			break;
		}
	}

	if (m_params.updateMoment == UpdateMoment::UPDATE_AFTER_BIND)
	{
		// Check update after bind for each used descriptor type.
		const auto& usedDescriptorTypes = getAllUsedDescriptorTypes(*m_params.descriptorSet, numIterations);
		const auto& indexingFeatures    = context.getDescriptorIndexingFeatures();

		for (const auto& descType : usedDescriptorTypes)
		{
			switch (descType)
			{
			case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
				if (!indexingFeatures.descriptorBindingUniformBufferUpdateAfterBind)
					TCU_THROW(NotSupportedError, "Update-after-bind not supported for uniform buffers");
				break;

			case VK_DESCRIPTOR_TYPE_SAMPLER:
			case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
				if (!indexingFeatures.descriptorBindingSampledImageUpdateAfterBind)
					TCU_THROW(NotSupportedError, "Update-after-bind not supported for samplers and sampled images");
				break;

			case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
				if (!indexingFeatures.descriptorBindingStorageImageUpdateAfterBind)
					TCU_THROW(NotSupportedError, "Update-after-bind not supported for storage images");
				break;

			case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
			case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
				if (!indexingFeatures.descriptorBindingStorageBufferUpdateAfterBind)
					TCU_THROW(NotSupportedError, "Update-after-bind not supported for storage buffers");
				break;

			case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
				if (!indexingFeatures.descriptorBindingUniformTexelBufferUpdateAfterBind)
					TCU_THROW(NotSupportedError, "Update-after-bind not supported for uniform texel buffers");
				break;

			case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
				if (!indexingFeatures.descriptorBindingStorageTexelBufferUpdateAfterBind)
					TCU_THROW(NotSupportedError, "Update-after-bind not supported for storage texel buffers");
				break;

			case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
				TCU_THROW(InternalError, "Tests do not support update-after-bind with input attachments");
				break;

			case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT:
				{
					// Just in case we ever mix some of these in.
					context.requireDeviceFunctionality("VK_EXT_inline_uniform_block");
					const auto& iubFeatures = context.getInlineUniformBlockFeaturesEXT();
					if (!iubFeatures.descriptorBindingInlineUniformBlockUpdateAfterBind)
						TCU_THROW(NotSupportedError, "Update-after-bind not supported for inline uniform blocks");
				}
				break;

			case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
				{
					// Just in case we ever mix some of these in.
					context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
					const auto& asFeatures = context.getAccelerationStructureFeatures();
					if (!asFeatures.descriptorBindingAccelerationStructureUpdateAfterBind)
						TCU_THROW(NotSupportedError, "Update-after-bind not supported for acceleration structures");
				}
				break;

			case VK_DESCRIPTOR_TYPE_MUTABLE_VALVE:
				TCU_THROW(InternalError, "Found VK_DESCRIPTOR_TYPE_MUTABLE_VALVE in list of used descriptor types");
				break;

			default:
				TCU_THROW(InternalError, "Unexpected descriptor type found in list of used descriptor types: " + de::toString(descType));
				break;
			}
		}
	}

	if (m_params.arrayAccessType == ArrayAccessType::PUSH_CONSTANT)
	{
		// These require dynamically uniform indices.
		const auto& usedDescriptorTypes         = getUsedArrayDescriptorTypes(*m_params.descriptorSet, numIterations);
		const auto& features                    = context.getDeviceFeatures();
		const auto descriptorIndexingSupported  = context.isDeviceFunctionalitySupported("VK_EXT_descriptor_indexing");
		const auto& indexingFeatures            = context.getDescriptorIndexingFeatures();

		for (const auto& descType : usedDescriptorTypes)
		{
			switch (descType)
			{
			case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
				if (!features.shaderUniformBufferArrayDynamicIndexing)
					TCU_THROW(NotSupportedError, "Dynamic indexing not supported for uniform buffers");
				break;

			case VK_DESCRIPTOR_TYPE_SAMPLER:
			case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
				if (!features.shaderSampledImageArrayDynamicIndexing)
					TCU_THROW(NotSupportedError, "Dynamic indexing not supported for samplers and sampled images");
				break;

			case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
				if (!features.shaderStorageImageArrayDynamicIndexing)
					TCU_THROW(NotSupportedError, "Dynamic indexing not supported for storage images");
				break;

			case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
			case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
				if (!features.shaderStorageBufferArrayDynamicIndexing)
					TCU_THROW(NotSupportedError, "Dynamic indexing not supported for storage buffers");
				break;

			case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
				if (!descriptorIndexingSupported || !indexingFeatures.shaderUniformTexelBufferArrayDynamicIndexing)
					TCU_THROW(NotSupportedError, "Dynamic indexing not supported for uniform texel buffers");
				break;

			case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
				if (!descriptorIndexingSupported || !indexingFeatures.shaderStorageTexelBufferArrayDynamicIndexing)
					TCU_THROW(NotSupportedError, "Dynamic indexing not supported for storage texel buffers");
				break;

			case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
				if (!descriptorIndexingSupported || !indexingFeatures.shaderInputAttachmentArrayDynamicIndexing)
					TCU_THROW(NotSupportedError, "Dynamic indexing not supported for input attachments");
				break;

			case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
				context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
				break;

			case VK_DESCRIPTOR_TYPE_MUTABLE_VALVE:
				TCU_THROW(InternalError, "Found VK_DESCRIPTOR_TYPE_MUTABLE_VALVE in list of used array descriptor types");
				break;

			default:
				TCU_THROW(InternalError, "Unexpected descriptor type found in list of used descriptor types: " + de::toString(descType));
				break;
			}
		}
	}

	// Check layout support.
	{
		const auto& vkd               = context.getDeviceInterface();
		const auto  device            = context.getDevice();
		const auto  stageFlags        = m_params.getStageFlags();

		{
			const auto  layoutCreateFlags = m_params.getDstLayoutCreateFlags();
			const auto  supported         = m_params.descriptorSet->checkDescriptorSetLayout(vkd, device, stageFlags, layoutCreateFlags);

			if (!supported)
				TCU_THROW(NotSupportedError, "Required descriptor set layout not supported");
		}

		if (m_params.updateType == UpdateType::COPY)
		{
			const auto  layoutCreateFlags = m_params.getSrcLayoutCreateFlags();
			const auto  supported         = m_params.descriptorSet->checkDescriptorSetLayout(vkd, device, stageFlags, layoutCreateFlags);

			if (!supported)
				TCU_THROW(NotSupportedError, "Required descriptor set layout for source set not supported");

			// Check specific layouts for the different source sets are supported.
			for (deUint32 iter = 0u; iter < numIterations; ++iter)
			{
				const auto srcSet             = m_params.descriptorSet->genSourceSet(m_params.sourceSetStrategy, iter);
				const auto srcLayoutSupported = srcSet->checkDescriptorSetLayout(vkd, device, stageFlags, layoutCreateFlags);

				if (!srcLayoutSupported)
					TCU_THROW(NotSupportedError, "Descriptor set layout for source set at iteration " + de::toString(iter) + " not supported");
			}
		}
	}

	// Check supported stores and stages.
	const bool vertexStage   = isVertexStage(m_params.testingStage);
	const bool fragmentStage = (m_params.testingStage == TestingStage::FRAGMENT);
	const bool geometryStage = (m_params.testingStage == TestingStage::GEOMETRY);
	const bool tessellation  = (m_params.testingStage == TestingStage::TESS_CONTROL || m_params.testingStage == TestingStage::TESS_EVAL);

	const auto& features = context.getDeviceFeatures();

	if (vertexStage && !features.vertexPipelineStoresAndAtomics)
		TCU_THROW(NotSupportedError, "Vertex pipeline stores and atomics not supported");

	if (fragmentStage && !features.fragmentStoresAndAtomics)
		TCU_THROW(NotSupportedError, "Fragment shader stores and atomics not supported");

	if (geometryStage && !features.geometryShader)
		TCU_THROW(NotSupportedError, "Geometry shader not supported");

	if (tessellation && !features.tessellationShader)
		TCU_THROW(NotSupportedError, "Tessellation shaders not supported");
}

// What to do at each iteration step. Used to apply UPDATE_AFTER_BIND or not.
enum class Step
{
	UPDATE = 0,
	BIND,
};

// Create render pass.
Move<VkRenderPass> buildRenderPass (const DeviceInterface& vkd, VkDevice device, const std::vector<Resource>& resources)
{
	const auto imageFormat = getDescriptorImageFormat();

	std::vector<VkAttachmentDescription>    attachmentDescriptions;
	std::vector<VkAttachmentReference>      attachmentReferences;
	std::vector<deUint32>                   attachmentIndices;

	for (const auto& resource : resources)
	{
		if (resource.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
		{
			const auto nextIndex = static_cast<deUint32>(attachmentDescriptions.size());

			const VkAttachmentDescription description = {
				0u,                                 //  VkAttachmentDescriptionFlags	flags;
				imageFormat,                        //  VkFormat						format;
				VK_SAMPLE_COUNT_1_BIT,              //  VkSampleCountFlagBits			samples;
				VK_ATTACHMENT_LOAD_OP_LOAD,         //  VkAttachmentLoadOp				loadOp;
				VK_ATTACHMENT_STORE_OP_DONT_CARE,   //  VkAttachmentStoreOp				storeOp;
				VK_ATTACHMENT_LOAD_OP_DONT_CARE,    //  VkAttachmentLoadOp				stencilLoadOp;
				VK_ATTACHMENT_STORE_OP_DONT_CARE,   //  VkAttachmentStoreOp				stencilStoreOp;
				VK_IMAGE_LAYOUT_GENERAL,            //  VkImageLayout					initialLayout;
				VK_IMAGE_LAYOUT_GENERAL,            //  VkImageLayout					finalLayout;
			};

			const VkAttachmentReference reference = { nextIndex, VK_IMAGE_LAYOUT_GENERAL };

			attachmentIndices.push_back(nextIndex);
			attachmentDescriptions.push_back(description);
			attachmentReferences.push_back(reference);
		}
	}

	const auto attachmentCount = static_cast<deUint32>(attachmentDescriptions.size());
	DE_ASSERT(attachmentCount == static_cast<deUint32>(attachmentIndices.size()));
	DE_ASSERT(attachmentCount == static_cast<deUint32>(attachmentReferences.size()));

	const VkSubpassDescription subpassDescription =
	{
		0u,                                     //  VkSubpassDescriptionFlags		flags;
		VK_PIPELINE_BIND_POINT_GRAPHICS,        //  VkPipelineBindPoint				pipelineBindPoint;
		attachmentCount,                        //  deUint32						inputAttachmentCount;
		de::dataOrNull(attachmentReferences),   //  const VkAttachmentReference*	pInputAttachments;
		0u,                                     //  deUint32						colorAttachmentCount;
		nullptr,                                //  const VkAttachmentReference*	pColorAttachments;
		0u,                                     //  const VkAttachmentReference*	pResolveAttachments;
		nullptr,                                //  const VkAttachmentReference*	pDepthStencilAttachment;
		0u,                                     //  deUint32						preserveAttachmentCount;
		nullptr,                                //  const deUint32*					pPreserveAttachments;
	};

	const VkRenderPassCreateInfo renderPassCreateInfo =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,              //  VkStructureType					sType;
		nullptr,                                                //  const void*						pNext;
		0u,                                                     //  VkRenderPassCreateFlags			flags;
		static_cast<deUint32>(attachmentDescriptions.size()),   //  deUint32						attachmentCount;
		de::dataOrNull(attachmentDescriptions),                 //  const VkAttachmentDescription*	pAttachments;
		1u,                                                     //  deUint32						subpassCount;
		&subpassDescription,                                    //  const VkSubpassDescription*		pSubpasses;
		0u,                                                     //  deUint32						dependencyCount;
		nullptr,                                                //  const VkSubpassDependency*		pDependencies;
	};

	return createRenderPass(vkd, device, &renderPassCreateInfo);
}

// Create a graphics pipeline.
Move<VkPipeline> buildGraphicsPipeline (const DeviceInterface& vkd, VkDevice device, VkPipelineLayout pipelineLayout,
										VkShaderModule vertModule,
										VkShaderModule tescModule,
										VkShaderModule teseModule,
										VkShaderModule geomModule,
										VkShaderModule fragModule,
										VkRenderPass renderPass)
{
	const auto                    extent    = getDefaultExtent();
	const std::vector<VkViewport> viewports (1u, makeViewport(extent));
	const std::vector<VkRect2D>   scissors  (1u, makeRect2D(extent));
	const auto                    hasTess   = (tescModule != DE_NULL || teseModule != DE_NULL);
	const auto                    topology  = (hasTess ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);


	const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = initVulkanStructure();

	const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,    //  VkStructureType							sType;
		nullptr,                                                        //  const void*								pNext;
		0u,                                                             //  VkPipelineInputAssemblyStateCreateFlags	flags;
		topology,                                                       //  VkPrimitiveTopology						topology;
		VK_FALSE,                                                       //  VkBool32								primitiveRestartEnable;
	};

	const VkPipelineTessellationStateCreateInfo tessellationStateCreateInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,  //	VkStructureType							sType;
		nullptr,                                                    //	const void*								pNext;
		0u,                                                         //	VkPipelineTessellationStateCreateFlags	flags;
		(hasTess ? 3u : 0u),                                        //	deUint32								patchControlPoints;
	};

	const VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,  //  VkStructureType						sType;
		nullptr,                                                //  const void*							pNext;
		0u,                                                     //  VkPipelineViewportStateCreateFlags	flags;
		static_cast<deUint32>(viewports.size()),                //  deUint32							viewportCount;
		de::dataOrNull(viewports),                              //  const VkViewport*					pViewports;
		static_cast<deUint32>(scissors.size()),                 //  deUint32							scissorCount;
		de::dataOrNull(scissors),                               //  const VkRect2D*						pScissors;
	};

	const VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, //  VkStructureType							sType;
		nullptr,                                                    //  const void*								pNext;
		0u,                                                         //  VkPipelineRasterizationStateCreateFlags	flags;
		VK_FALSE,                                                   //  VkBool32								depthClampEnable;
		(fragModule == DE_NULL ? VK_TRUE : VK_FALSE),               //  VkBool32								rasterizerDiscardEnable;
		VK_POLYGON_MODE_FILL,                                       //  VkPolygonMode							polygonMode;
		VK_CULL_MODE_NONE,                                          //  VkCullModeFlags							cullMode;
		VK_FRONT_FACE_CLOCKWISE,                                    //  VkFrontFace								frontFace;
		VK_FALSE,                                                   //  VkBool32								depthBiasEnable;
		0.0f,                                                       //  float									depthBiasConstantFactor;
		0.0f,                                                       //  float									depthBiasClamp;
		0.0f,                                                       //  float									depthBiasSlopeFactor;
		1.0f,                                                       //  float									lineWidth;
	};

	const VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,   //  VkStructureType							sType;
		nullptr,                                                    //  const void*								pNext;
		0u,                                                         //  VkPipelineMultisampleStateCreateFlags	flags;
		VK_SAMPLE_COUNT_1_BIT,                                      //  VkSampleCountFlagBits					rasterizationSamples;
		VK_FALSE,                                                   //  VkBool32								sampleShadingEnable;
		1.0f,                                                       //  float									minSampleShading;
		nullptr,                                                    //  const VkSampleMask*						pSampleMask;
		VK_FALSE,                                                   //  VkBool32								alphaToCoverageEnable;
		VK_FALSE,                                                   //  VkBool32								alphaToOneEnable;
	};

	const VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = initVulkanStructure();

	const VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = initVulkanStructure();

	return makeGraphicsPipeline(vkd, device, pipelineLayout,
	                            vertModule, tescModule, teseModule, geomModule, fragModule,
	                            renderPass, 0u, &vertexInputStateCreateInfo, &inputAssemblyStateCreateInfo,
	                            (hasTess ? &tessellationStateCreateInfo : nullptr), &viewportStateCreateInfo,
								&rasterizationStateCreateInfo, &multisampleStateCreateInfo,
	                            &depthStencilStateCreateInfo, &colorBlendStateCreateInfo, nullptr);
}

Move<VkFramebuffer> buildFramebuffer (const DeviceInterface& vkd, VkDevice device, VkRenderPass renderPass, const std::vector<Resource>& resources)
{
	const auto extent = getDefaultExtent();

	std::vector<VkImageView> inputAttachments;
	for (const auto& resource : resources)
	{
		if (resource.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
			inputAttachments.push_back(resource.imageView.get());
	}

	const VkFramebufferCreateInfo framebufferCreateInfo =
	{
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,		//	VkStructureType				sType;
		nullptr,										//	const void*					pNext;
		0u,												//	VkFramebufferCreateFlags	flags;
		renderPass,										//	VkRenderPass				renderPass;
		static_cast<deUint32>(inputAttachments.size()),	//	deUint32					attachmentCount;
		de:: dataOrNull(inputAttachments),				//	const VkImageView*			pAttachments;
		extent.width,									//	deUint32					width;
		extent.height,									//	deUint32					height;
		extent.depth,									//	deUint32					layers;
	};

	return createFramebuffer(vkd, device, &framebufferCreateInfo);
}

tcu::TestStatus MutableTypesInstance::iterate ()
{
	const auto device  = m_context.getDevice();
	const auto physDev = m_context.getPhysicalDevice();
	const auto qIndex  = m_context.getUniversalQueueFamilyIndex();
	const auto queue   = m_context.getUniversalQueue();

	const auto& vki      = m_context.getInstanceInterface();
	const auto& vkd      = m_context.getDeviceInterface();
	auto      & alloc    = m_context.getDefaultAllocator();
	const auto& paramSet = m_params.descriptorSet;

	const auto numIterations      = paramSet->maxTypes();
	const bool useExternalImage   = needsExternalImage(*m_params.descriptorSet);
	const bool useExternalSampler = needsExternalSampler(*m_params.descriptorSet);
	const auto stageFlags         = m_params.getStageFlags();
	const bool srcSetNeeded       = (m_params.updateType == UpdateType::COPY);
	const bool updateAfterBind    = (m_params.updateMoment == UpdateMoment::UPDATE_AFTER_BIND);
	const auto bindPoint          = m_params.getBindPoint();
	const bool rayTracing         = isRayTracingStage(m_params.testingStage);
	const bool useAABBs           = (m_params.testingStage == TestingStage::INTERSECTION);

	// Resources for each iteration.
	std::vector<std::vector<Resource>> allResources;
	allResources.reserve(numIterations);

	// Command pool.
	const auto cmdPool = makeCommandPool(vkd, device, qIndex);

	// Descriptor pool and set for the active (dst) descriptor set.
	const auto dstPoolFlags   = m_params.getDstPoolCreateFlags();
	const auto dstLayoutFlags = m_params.getDstLayoutCreateFlags();

	const auto dstPool   = paramSet->makeDescriptorPool(vkd, device, m_params.poolMutableStrategy, dstPoolFlags);
	const auto dstLayout = paramSet->makeDescriptorSetLayout(vkd, device, stageFlags, dstLayoutFlags);
	const auto varCount  = paramSet->getVariableDescriptorCount();

	using VariableCountInfoPtr = de::MovePtr<VkDescriptorSetVariableDescriptorCountAllocateInfo>;

	VariableCountInfoPtr dstVariableCountInfo;
	if (varCount)
	{
		dstVariableCountInfo = VariableCountInfoPtr(new VkDescriptorSetVariableDescriptorCountAllocateInfo);
		*dstVariableCountInfo = initVulkanStructure();

		dstVariableCountInfo->descriptorSetCount = 1u;
		dstVariableCountInfo->pDescriptorCounts  = &(varCount.get());
	}
	const auto dstSet = makeDescriptorSet(vkd, device, dstPool.get(), dstLayout.get(), dstVariableCountInfo.get());

	// Source pool and set (optional).
	const auto                  srcPoolFlags   = m_params.getSrcPoolCreateFlags();
	const auto                  srcLayoutFlags = m_params.getSrcLayoutCreateFlags();
	DescriptorSetPtr            iterationSrcSet;
	Move<VkDescriptorPool>      srcPool;
	Move<VkDescriptorSetLayout> srcLayout;
	Move<VkDescriptorSet>       srcSet;

	// Extra set for external resources and output buffer.
	std::vector<Resource> extraResources;
	extraResources.emplace_back(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vkd, device, alloc, qIndex, queue, useAABBs, 0u, numIterations);
	if (useExternalImage)
		extraResources.emplace_back(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, vkd, device, alloc, qIndex, queue, useAABBs, getExternalSampledImageValue());
	if (useExternalSampler)
		extraResources.emplace_back(VK_DESCRIPTOR_TYPE_SAMPLER, vkd, device, alloc, qIndex, queue, useAABBs, 0u);
	if (rayTracing)
		extraResources.emplace_back(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, vkd, device, alloc, qIndex, queue, useAABBs, 0u);

	Move<VkDescriptorPool> extraPool;
	{
		DescriptorPoolBuilder poolBuilder;
		poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		if (useExternalImage)
			poolBuilder.addType(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
		if (useExternalSampler)
			poolBuilder.addType(VK_DESCRIPTOR_TYPE_SAMPLER);
		if (rayTracing)
			poolBuilder.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
		extraPool = poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	}

	Move<VkDescriptorSetLayout> extraLayout;
	{
		DescriptorSetLayoutBuilder layoutBuilder;
		layoutBuilder.addBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u, stageFlags, nullptr);
		if (useExternalImage)
			layoutBuilder.addBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1u, stageFlags, nullptr);
		if (useExternalSampler)
			layoutBuilder.addBinding(VK_DESCRIPTOR_TYPE_SAMPLER, 1u, stageFlags, nullptr);
		if (rayTracing)
		{
			// The extra acceleration structure is used from the ray generation shader only.
			layoutBuilder.addBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1u, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr);
		}
		extraLayout = layoutBuilder.build(vkd, device);
	}

	const auto extraSet = makeDescriptorSet(vkd, device, extraPool.get(), extraLayout.get());

	// Update extra set.
	using DescriptorBufferInfoPtr = de::MovePtr<VkDescriptorBufferInfo>;
	using DescriptorImageInfoPtr  = de::MovePtr<VkDescriptorImageInfo>;
	using DescriptorASInfoPtr     = de::MovePtr<VkWriteDescriptorSetAccelerationStructureKHR>;

	deUint32                bindingCount = 0u;
	DescriptorBufferInfoPtr bufferInfoPtr;
	DescriptorImageInfoPtr  imageInfoPtr;
	DescriptorImageInfoPtr  samplerInfoPtr;
	DescriptorASInfoPtr     asWriteInfoPtr;

    const auto outputBufferSize = static_cast<VkDeviceSize>(sizeof(deUint32) * static_cast<size_t>(numIterations));
	bufferInfoPtr = DescriptorBufferInfoPtr(new VkDescriptorBufferInfo(makeDescriptorBufferInfo(extraResources[bindingCount++].bufferWithMemory->get(), 0ull, outputBufferSize)));
	if (useExternalImage)
		imageInfoPtr = DescriptorImageInfoPtr(new VkDescriptorImageInfo(makeDescriptorImageInfo(DE_NULL, extraResources[bindingCount++].imageView.get(), VK_IMAGE_LAYOUT_GENERAL)));
	if (useExternalSampler)
		samplerInfoPtr = DescriptorImageInfoPtr(new VkDescriptorImageInfo(makeDescriptorImageInfo(extraResources[bindingCount++].sampler.get(), DE_NULL, VK_IMAGE_LAYOUT_GENERAL)));
	if (rayTracing)
	{
		asWriteInfoPtr = DescriptorASInfoPtr(new VkWriteDescriptorSetAccelerationStructureKHR);
		*asWriteInfoPtr = initVulkanStructure();
		asWriteInfoPtr->accelerationStructureCount  = 1u;
		asWriteInfoPtr->pAccelerationStructures     = extraResources[bindingCount++].asData.tlas.get()->getPtr();
	}

	{
		bindingCount = 0u;
		DescriptorSetUpdateBuilder updateBuilder;
		updateBuilder.writeSingle(extraSet.get(), DescriptorSetUpdateBuilder::Location::binding(bindingCount++), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bufferInfoPtr.get());
		if (useExternalImage)
			updateBuilder.writeSingle(extraSet.get(), DescriptorSetUpdateBuilder::Location::binding(bindingCount++), VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, imageInfoPtr.get());
		if (useExternalSampler)
			updateBuilder.writeSingle(extraSet.get(), DescriptorSetUpdateBuilder::Location::binding(bindingCount++), VK_DESCRIPTOR_TYPE_SAMPLER, samplerInfoPtr.get());
		if (rayTracing)
			updateBuilder.writeSingle(extraSet.get(), DescriptorSetUpdateBuilder::Location::binding(bindingCount++), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, asWriteInfoPtr.get());
		updateBuilder.update(vkd, device);
	}

	// Push constants.
	const deUint32            zero    = 0u;
	const VkPushConstantRange pcRange = {stageFlags, 0u /*offset*/, static_cast<deUint32>(sizeof(zero)) /*size*/ };

	// Needed for some test variants.
	Move<VkShaderModule> vertPassthrough;
	Move<VkShaderModule> tesePassthrough;
	Move<VkShaderModule> tescPassthrough;
	Move<VkShaderModule> rgenPassthrough;
	Move<VkShaderModule> missPassthrough;

	if (m_params.testingStage == TestingStage::FRAGMENT
		|| m_params.testingStage == TestingStage::GEOMETRY
		|| m_params.testingStage == TestingStage::TESS_CONTROL
		|| m_params.testingStage == TestingStage::TESS_EVAL)
	{
		vertPassthrough = createShaderModule(vkd, device, m_context.getBinaryCollection().get("vert"), 0u);
	}

	if (m_params.testingStage == TestingStage::TESS_CONTROL)
	{
		tesePassthrough = createShaderModule(vkd, device, m_context.getBinaryCollection().get("tese"), 0u);
	}

	if (m_params.testingStage == TestingStage::TESS_EVAL)
	{
		tescPassthrough = createShaderModule(vkd, device, m_context.getBinaryCollection().get("tesc"), 0u);
	}

	if (m_params.testingStage == TestingStage::CLOSEST_HIT
		|| m_params.testingStage == TestingStage::ANY_HIT
		|| m_params.testingStage == TestingStage::INTERSECTION
		|| m_params.testingStage == TestingStage::MISS
		|| m_params.testingStage == TestingStage::CALLABLE)
	{
		rgenPassthrough = createShaderModule(vkd, device, m_context.getBinaryCollection().get("rgen"), 0u);
	}

	if (m_params.testingStage == TestingStage::INTERSECTION)
	{
		missPassthrough = createShaderModule(vkd, device, m_context.getBinaryCollection().get("miss"), 0u);
	}

	for (deUint32 iteration = 0u; iteration < numIterations; ++iteration)
	{
		// Generate source set for the current iteration.
		if (srcSetNeeded)
		{
			// Free previous descriptor set before rebuilding the pool.
			srcSet          = Move<VkDescriptorSet>();
			iterationSrcSet = paramSet->genSourceSet(m_params.sourceSetStrategy, iteration);
			srcPool         = iterationSrcSet->makeDescriptorPool(vkd, device, m_params.poolMutableStrategy, srcPoolFlags);
			srcLayout       = iterationSrcSet->makeDescriptorSetLayout(vkd, device, stageFlags, srcLayoutFlags);

			const auto srcVarCount = iterationSrcSet->getVariableDescriptorCount();
			VariableCountInfoPtr srcVariableCountInfo;

			if (srcVarCount)
			{
				srcVariableCountInfo = VariableCountInfoPtr(new VkDescriptorSetVariableDescriptorCountAllocateInfo);
				*srcVariableCountInfo = initVulkanStructure();

				srcVariableCountInfo->descriptorSetCount = 1u;
				srcVariableCountInfo->pDescriptorCounts = &(srcVarCount.get());
			}

			srcSet = makeDescriptorSet(vkd, device, srcPool.get(), srcLayout.get(), srcVariableCountInfo.get());
		}

		// Set layouts and sets used in the pipeline.
		const std::vector<VkDescriptorSetLayout> setLayouts = {dstLayout.get(), extraLayout.get()};
		const std::vector<VkDescriptorSet>       usedSets   = {dstSet.get(), extraSet.get()};

		// Create resources.
		allResources.emplace_back(paramSet->createResources(vkd, device, alloc, qIndex, queue, iteration, useAABBs));
		const auto& resources = allResources.back();

		// Make pipeline for the current iteration.
		const auto pipelineLayout = makePipelineLayout(vkd, device, static_cast<deUint32>(setLayouts.size()), de::dataOrNull(setLayouts), 1u, &pcRange);
		const auto moduleName     = shaderName(iteration);
		const auto shaderModule   = createShaderModule(vkd, device, m_context.getBinaryCollection().get(moduleName), 0u);

		Move<VkPipeline>     pipeline;
		Move<VkRenderPass>   renderPass;
		Move<VkFramebuffer>  framebuffer;

		deUint32 shaderGroupHandleSize		= 0u;
		deUint32 shaderGroupBaseAlignment	= 1u;

		de::MovePtr<BufferWithMemory>	raygenSBT;
		de::MovePtr<BufferWithMemory>	missSBT;
		de::MovePtr<BufferWithMemory>	hitSBT;
		de::MovePtr<BufferWithMemory>	callableSBT;

		VkStridedDeviceAddressRegionKHR	raygenSBTRegion		= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
		VkStridedDeviceAddressRegionKHR	missSBTRegion		= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
		VkStridedDeviceAddressRegionKHR	hitSBTRegion		= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
		VkStridedDeviceAddressRegionKHR	callableSBTRegion	= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);

		if (bindPoint == VK_PIPELINE_BIND_POINT_COMPUTE)
			pipeline = makeComputePipeline(vkd, device, pipelineLayout.get(), 0u, shaderModule.get(), 0u, nullptr);
		else if (bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS)
		{
			VkShaderModule vertModule = DE_NULL;
			VkShaderModule teseModule = DE_NULL;
			VkShaderModule tescModule = DE_NULL;
			VkShaderModule geomModule = DE_NULL;
			VkShaderModule fragModule = DE_NULL;

			if (m_params.testingStage == TestingStage::VERTEX)
				vertModule = shaderModule.get();
			else if (m_params.testingStage == TestingStage::FRAGMENT)
			{
				vertModule = vertPassthrough.get();
				fragModule = shaderModule.get();
			}
			else if (m_params.testingStage == TestingStage::GEOMETRY)
			{
				vertModule = vertPassthrough.get();
				geomModule = shaderModule.get();
			}
			else if (m_params.testingStage == TestingStage::TESS_CONTROL)
			{
				vertModule = vertPassthrough.get();
				teseModule = tesePassthrough.get();
				tescModule = shaderModule.get();
			}
			else if (m_params.testingStage == TestingStage::TESS_EVAL)
			{
				vertModule = vertPassthrough.get();
				tescModule = tescPassthrough.get();
				teseModule = shaderModule.get();
			}
			else
				DE_ASSERT(false);

			renderPass  = buildRenderPass(vkd, device, resources);
			pipeline    = buildGraphicsPipeline(vkd, device, pipelineLayout.get(), vertModule, tescModule, teseModule, geomModule, fragModule, renderPass.get());
			framebuffer = buildFramebuffer(vkd, device, renderPass.get(), resources);
		}
		else if (bindPoint == VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR)
		{
			const auto rayTracingPipeline       = de::newMovePtr<RayTracingPipeline>();
			const auto rayTracingPropertiesKHR	= makeRayTracingProperties(vki, physDev);
			shaderGroupHandleSize				= rayTracingPropertiesKHR->getShaderGroupHandleSize();
			shaderGroupBaseAlignment			= rayTracingPropertiesKHR->getShaderGroupBaseAlignment();

			VkShaderModule rgenModule = DE_NULL;
			VkShaderModule isecModule = DE_NULL;
			VkShaderModule ahitModule = DE_NULL;
			VkShaderModule chitModule = DE_NULL;
			VkShaderModule missModule = DE_NULL;
			VkShaderModule callModule = DE_NULL;

			const deUint32  rgenGroup   = 0u;
			deUint32        hitGroup    = 0u;
			deUint32        missGroup   = 0u;
			deUint32        callGroup   = 0u;

			if (m_params.testingStage == TestingStage::RAY_GEN)
			{
				rgenModule = shaderModule.get();
				rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, rgenModule, rgenGroup);
			}
			else if (m_params.testingStage == TestingStage::INTERSECTION)
			{
				hitGroup   = 1u;
				missGroup  = 2u;
				rgenModule = rgenPassthrough.get();
				missModule = missPassthrough.get();
				isecModule = shaderModule.get();
				rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, rgenModule, rgenGroup);
				rayTracingPipeline->addShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR, isecModule, hitGroup);
				rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR, missModule, missGroup);
			}
			else if (m_params.testingStage == TestingStage::ANY_HIT)
			{
				hitGroup   = 1u;
				rgenModule = rgenPassthrough.get();
				ahitModule = shaderModule.get();
				rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, rgenModule, rgenGroup);
				rayTracingPipeline->addShader(VK_SHADER_STAGE_ANY_HIT_BIT_KHR, ahitModule, hitGroup);
			}
			else if (m_params.testingStage == TestingStage::CLOSEST_HIT)
			{
				hitGroup   = 1u;
				rgenModule = rgenPassthrough.get();
				chitModule = shaderModule.get();
				rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, rgenModule, rgenGroup);
				rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, chitModule, hitGroup);
			}
			else if (m_params.testingStage == TestingStage::MISS)
			{
				missGroup  = 1u;
				rgenModule = rgenPassthrough.get();
				missModule = shaderModule.get();
				rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, rgenModule, rgenGroup);
				rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR, missModule, missGroup);
			}
			else if (m_params.testingStage == TestingStage::CALLABLE)
			{
				callGroup  = 1u;
				rgenModule = rgenPassthrough.get();
				callModule = shaderModule.get();
				rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, rgenModule, rgenGroup);
				rayTracingPipeline->addShader(VK_SHADER_STAGE_CALLABLE_BIT_KHR, callModule, callGroup);
			}
			else
				DE_ASSERT(false);

			pipeline = rayTracingPipeline->createPipeline(vkd, device, pipelineLayout.get());

			raygenSBT = rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline.get(), alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, rgenGroup, 1u);
			raygenSBTRegion = makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, raygenSBT->get(), 0ull), shaderGroupHandleSize, shaderGroupHandleSize);

			if (missGroup > 0u)
			{
				missSBT = rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline.get(), alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, missGroup, 1u);
				missSBTRegion = makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, missSBT->get(), 0ull), shaderGroupHandleSize, shaderGroupHandleSize);
			}

			if (hitGroup > 0u)
			{
				hitSBT = rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline.get(), alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, hitGroup, 1u);
				hitSBTRegion = makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, hitSBT->get(), 0ull), shaderGroupHandleSize, shaderGroupHandleSize);
			}

			if (callGroup > 0u)
			{
				callableSBT = rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline.get(), alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, callGroup, 1u);
				callableSBTRegion = makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, callableSBT->get(), 0ull), shaderGroupHandleSize, shaderGroupHandleSize);
			}
		}
		else
			DE_ASSERT(false);

		// Command buffer for the current iteration.
		const auto cmdBufferPtr = allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		const auto cmdBuffer    = cmdBufferPtr.get();

		beginCommandBuffer(vkd, cmdBuffer);

		const Step steps[] = {
			(updateAfterBind ? Step::BIND : Step::UPDATE),
			(updateAfterBind ? Step::UPDATE : Step::BIND)
		};

		for (const auto& step : steps)
		{
			if (step == Step::BIND)
			{
				vkd.cmdBindPipeline(cmdBuffer, bindPoint, pipeline.get());
				vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, pipelineLayout.get(), 0u, static_cast<deUint32>(usedSets.size()), de::dataOrNull(usedSets), 0u, nullptr);
			}
			else // Step::UPDATE
			{
				if (srcSetNeeded)
				{
					// Note: these operations need to be called on paramSet and not iterationSrcSet. The latter is a compatible set
					// that's correct and contains compatible bindings but, when a binding has been changed from non-mutable to
					// mutable or to an extended mutable type, the list of descriptor types for the mutable bindings in
					// iterationSrcSet are not in iteration order like they are in the original set and must not be taken into
					// account to update or copy sets.
					paramSet->updateDescriptorSet(vkd, device, srcSet.get(), iteration, resources);
					paramSet->copyDescriptorSet(vkd, device, srcSet.get(), dstSet.get());
				}
				else
				{
					paramSet->updateDescriptorSet(vkd, device, dstSet.get(), iteration, resources);
				}
			}
		}

		// Run shader.
		vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), stageFlags, 0u, static_cast<deUint32>(sizeof(zero)), &zero);

		if (bindPoint == VK_PIPELINE_BIND_POINT_COMPUTE)
			vkd.cmdDispatch(cmdBuffer, 1u, 1u, 1u);
		else if (bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS)
		{
			const auto extent     = getDefaultExtent();
			const auto renderArea = makeRect2D(extent);

			beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), renderArea);
			vkd.cmdDraw(cmdBuffer, 3u, 1u, 0u, 0u);
			endRenderPass(vkd, cmdBuffer);
		}
		else if (bindPoint == VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR)
		{
			vkd.cmdTraceRaysKHR(cmdBuffer, &raygenSBTRegion, &missSBTRegion, &hitSBTRegion, &callableSBTRegion, 1u, 1u, 1u);
		}
		else
			DE_ASSERT(false);

		endCommandBuffer(vkd, cmdBuffer);
		submitCommandsAndWait(vkd, device, queue, cmdBuffer);

		// Verify output buffer.
		{
			const auto outputBufferVal = extraResources[0].getStoredValue(vkd, device, alloc, qIndex, queue, iteration);
			DE_ASSERT(static_cast<bool>(outputBufferVal));

			const auto expectedValue = getExpectedOutputBufferValue();
			if (outputBufferVal.get() != expectedValue)
			{
				std::ostringstream msg;
				msg << "Iteration " << iteration << ": unexpected value found in output buffer (expected " << expectedValue << " and found " << outputBufferVal.get() << ")";
				TCU_FAIL(msg.str());
			}
		}

		// Verify descriptor writes.
		{
			size_t     resourcesOffset = 0;
			const auto writeMask       = getStoredValueMask();
			const auto numBindings     = paramSet->numBindings();

			for (deUint32 bindingIdx = 0u; bindingIdx < numBindings; ++bindingIdx)
			{
				const auto binding = paramSet->getBinding(bindingIdx);
				const auto bindingTypes = binding->typesAtIteration(iteration);

				for (size_t descriptorIdx = 0; descriptorIdx < bindingTypes.size(); ++descriptorIdx)
				{
					const auto& descriptorType = bindingTypes[descriptorIdx];
					if (!isShaderWritable(descriptorType))
						continue;

					const auto& resource        = resources[resourcesOffset + descriptorIdx];
					const auto  initialValue    = resource.initialValue;
					const auto  storedValuePtr  = resource.getStoredValue(vkd, device, alloc, qIndex, queue);

					DE_ASSERT(static_cast<bool>(storedValuePtr));
					const auto storedValue   = storedValuePtr.get();
					const auto expectedValue = (initialValue | writeMask);
					if (expectedValue != storedValue)
					{
						std::ostringstream msg;
						msg << "Iteration " << iteration << ": descriptor at binding " << bindingIdx << " index " << descriptorIdx
						    << " with type " << de::toString(descriptorType) << " contains unexpected value " << std::hex
							<< storedValue << " (expected " << expectedValue << ")";
						TCU_FAIL(msg.str());
					}
				}

				resourcesOffset += bindingTypes.size();
			}
		}
	}

	return tcu::TestStatus::pass("Pass");
}

using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

void createMutableTestVariants (tcu::TestContext& testCtx, tcu::TestCaseGroup* parentGroup, const DescriptorSetPtr& descriptorSet, const std::vector<TestingStage>& stagesToTest)
{
	const struct
	{
		UpdateType  updateType;
		const char* name;
	} updateTypes[] = {
		{UpdateType::WRITE, "update_write"},
		{UpdateType::COPY,  "update_copy"},
	};

	const struct
	{
		SourceSetStrategy   sourceSetStrategy;
		const char*         name;
	} sourceStrategies[] = {
		{SourceSetStrategy::MUTABLE,    "mutable_source"},
		{SourceSetStrategy::NONMUTABLE, "nonmutable_source"},
		{SourceSetStrategy::NO_SOURCE,  "no_source"},
	};

	const struct
	{
		SourceSetType   sourceSetType;
		const char*     name;
	} sourceTypes[] = {
		{SourceSetType::NORMAL,    "normal_source"},
		{SourceSetType::HOST_ONLY, "host_only_source"},
		{SourceSetType::NO_SOURCE, "no_source"},
	};

	const struct
	{
		PoolMutableStrategy poolMutableStrategy;
		const char*         name;
	} poolStrategies[] = {
		{PoolMutableStrategy::KEEP_TYPES,   "pool_same_types"},
		{PoolMutableStrategy::NO_TYPES,     "pool_no_types"},
		{PoolMutableStrategy::EXPAND_TYPES, "pool_expand_types"},
	};

	const struct
	{
		UpdateMoment    updateMoment;
		const char*     name;
	} updateMoments[] = {
		{UpdateMoment::NORMAL,            "pre_update"},
		{UpdateMoment::UPDATE_AFTER_BIND, "update_after_bind"},
	};

	const struct
	{
		ArrayAccessType arrayAccessType;
		const char*     name;
	} arrayAccessTypes[] = {
		{ArrayAccessType::CONSTANT,      "index_constant"},
		{ArrayAccessType::PUSH_CONSTANT, "index_push_constant"},
		{ArrayAccessType::NO_ARRAY,      "no_array"},
	};

	const struct StageAndName
	{
		TestingStage    testingStage;
		const char*     name;
	} testStageList[] = {
		{TestingStage::COMPUTE,      "comp"},
		{TestingStage::VERTEX,       "vert"},
		{TestingStage::TESS_CONTROL, "tesc"},
		{TestingStage::TESS_EVAL,    "tese"},
		{TestingStage::GEOMETRY,     "geom"},
		{TestingStage::FRAGMENT,     "frag"},
		{TestingStage::RAY_GEN,      "rgen"},
		{TestingStage::INTERSECTION, "isec"},
		{TestingStage::ANY_HIT,      "ahit"},
		{TestingStage::CLOSEST_HIT,  "chit"},
		{TestingStage::MISS,         "miss"},
		{TestingStage::CALLABLE,     "call"},
	};

	const bool hasArrays           = descriptorSet->hasArrays();
	const bool hasInputAttachments = usesInputAttachments(*descriptorSet);

	for (const auto& ut : updateTypes)
	{
		GroupPtr updateGroup(new tcu::TestCaseGroup(testCtx, ut.name, ""));

		for (const auto& srcStrategy : sourceStrategies)
		{
			// Skip combinations that make no sense.
			if (ut.updateType == UpdateType::WRITE && srcStrategy.sourceSetStrategy != SourceSetStrategy::NO_SOURCE)
				continue;

			if (ut.updateType == UpdateType::COPY && srcStrategy.sourceSetStrategy == SourceSetStrategy::NO_SOURCE)
				continue;

			if (srcStrategy.sourceSetStrategy == SourceSetStrategy::NONMUTABLE && descriptorSet->needsAnyAliasing())
				continue;

			GroupPtr srcStrategyGroup(new tcu::TestCaseGroup(testCtx, srcStrategy.name, ""));

			for (const auto& srcType : sourceTypes)
			{
				// Skip combinations that make no sense.
				if (ut.updateType == UpdateType::WRITE && srcType.sourceSetType != SourceSetType::NO_SOURCE)
					continue;

				if (ut.updateType == UpdateType::COPY && srcType.sourceSetType == SourceSetType::NO_SOURCE)
					continue;

				GroupPtr srcTypeGroup(new tcu::TestCaseGroup(testCtx, srcType.name, ""));

				for (const auto& poolStrategy: poolStrategies)
				{
					GroupPtr poolStrategyGroup(new tcu::TestCaseGroup(testCtx, poolStrategy.name, ""));

					for (const auto& moment : updateMoments)
					{
						//if (moment.updateMoment == UpdateMoment::UPDATE_AFTER_BIND && srcType.sourceSetType == SourceSetType::HOST_ONLY)
						//	continue;

						if (moment.updateMoment == UpdateMoment::UPDATE_AFTER_BIND && hasInputAttachments)
							continue;

						GroupPtr momentGroup(new tcu::TestCaseGroup(testCtx, moment.name, ""));

						for (const auto& accessType : arrayAccessTypes)
						{
							// Skip combinations that make no sense.
							if (hasArrays && accessType.arrayAccessType == ArrayAccessType::NO_ARRAY)
								continue;

							if (!hasArrays && accessType.arrayAccessType != ArrayAccessType::NO_ARRAY)
								continue;

							GroupPtr accessTypeGroup(new tcu::TestCaseGroup(testCtx, accessType.name, ""));

							for (const auto& testStage : stagesToTest)
							{
								const auto beginItr = std::begin(testStageList);
								const auto endItr   = std::end(testStageList);
								const auto iter     = std::find_if(beginItr, endItr, [testStage] (const StageAndName& ts) { return ts.testingStage == testStage; });

								DE_ASSERT(iter != endItr);
								const auto& stage = *iter;

								if (hasInputAttachments && stage.testingStage != TestingStage::FRAGMENT)
									continue;

								TestParams params = {
									descriptorSet,
									ut.updateType,
									srcStrategy.sourceSetStrategy,
									srcType.sourceSetType,
									poolStrategy.poolMutableStrategy,
									moment.updateMoment,
									accessType.arrayAccessType,
									stage.testingStage,
								};

								accessTypeGroup->addChild(new MutableTypesTest(testCtx, stage.name, "", params));
							}

							momentGroup->addChild(accessTypeGroup.release());
						}

						poolStrategyGroup->addChild(momentGroup.release());
					}

					srcTypeGroup->addChild(poolStrategyGroup.release());
				}

				srcStrategyGroup->addChild(srcTypeGroup.release());
			}

			updateGroup->addChild(srcStrategyGroup.release());
		}

		parentGroup->addChild(updateGroup.release());
	}
}

}

std::string descriptorTypeStr (VkDescriptorType descriptorType)
{
	static const auto prefixLen = std::string("VK_DESCRIPTOR_TYPE_").size();
	return de::toLower(de::toString(descriptorType).substr(prefixLen));
}

tcu::TestCaseGroup* createDescriptorValveMutableTests (tcu::TestContext& testCtx)
{
	GroupPtr mainGroup(new tcu::TestCaseGroup(testCtx, "mutable_descriptor", "Tests for VK_VALVE_mutable_descriptor_type"));

	const VkDescriptorType basicDescriptorTypes[] = {
		VK_DESCRIPTOR_TYPE_SAMPLER,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
		VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
		VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
	};

	static const auto mandatoryTypes = getMandatoryMutableTypes();

	using StageVec = std::vector<TestingStage>;

	const StageVec allStages =
	{
		TestingStage::COMPUTE,
		TestingStage::VERTEX,
		TestingStage::TESS_CONTROL,
		TestingStage::TESS_EVAL,
		TestingStage::GEOMETRY,
		TestingStage::FRAGMENT,
		TestingStage::RAY_GEN,
		TestingStage::INTERSECTION,
		TestingStage::ANY_HIT,
		TestingStage::CLOSEST_HIT,
		TestingStage::MISS,
		TestingStage::CALLABLE,
	};

	const StageVec reducedStages =
	{
		TestingStage::COMPUTE,
		TestingStage::VERTEX,
		TestingStage::FRAGMENT,
		TestingStage::RAY_GEN,
	};

	const StageVec computeOnly =
	{
		TestingStage::COMPUTE,
	};

	// Basic tests with a single mutable descriptor.
	{
		GroupPtr singleCases(new tcu::TestCaseGroup(testCtx, "single", "Basic mutable descriptor tests with a single mutable descriptor"));

		for (const auto& descriptorType : basicDescriptorTypes)
		{
			const auto                          groupName = descriptorTypeStr(descriptorType);
			const std::vector<VkDescriptorType> actualTypes(1u, descriptorType);

			DescriptorSetPtr setPtr;
			{
				DescriptorSet::BindingPtrVector setBindings;
				setBindings.emplace_back(new SingleBinding(VK_DESCRIPTOR_TYPE_MUTABLE_VALVE, actualTypes));
				setPtr = DescriptorSetPtr(new DescriptorSet(setBindings));
			}

			GroupPtr subGroup(new tcu::TestCaseGroup(testCtx, groupName.c_str(), ""));
			createMutableTestVariants(testCtx, subGroup.get(), setPtr, allStages);

			singleCases->addChild(subGroup.release());
		}

		// Case with a single descriptor that iterates several types.
		{
			DescriptorSetPtr setPtr;
			{
				DescriptorSet::BindingPtrVector setBindings;
				setBindings.emplace_back(new SingleBinding(VK_DESCRIPTOR_TYPE_MUTABLE_VALVE, mandatoryTypes));
				setPtr = DescriptorSetPtr(new DescriptorSet(setBindings));
			}

			GroupPtr subGroup(new tcu::TestCaseGroup(testCtx, "all_mandatory", ""));
			createMutableTestVariants(testCtx, subGroup.get(), setPtr, reducedStages);

			singleCases->addChild(subGroup.release());
		}

		// Cases that try to verify switching from any descriptor type to any other is possible.
		{
			GroupPtr subGroup(new tcu::TestCaseGroup(testCtx, "switches", "Test switching from one to another descriptor type works as expected"));

			for (const auto& initialDescriptorType : basicDescriptorTypes)
			{
				for (const auto& finalDescriptorType : basicDescriptorTypes)
				{
					if (initialDescriptorType == finalDescriptorType)
						continue;

					const std::vector<VkDescriptorType> mutableTypes { initialDescriptorType, finalDescriptorType };
					DescriptorSet::BindingPtrVector setBindings;
					setBindings.emplace_back(new SingleBinding(VK_DESCRIPTOR_TYPE_MUTABLE_VALVE, mutableTypes));

					DescriptorSetPtr setPtr = DescriptorSetPtr(new DescriptorSet(setBindings));

					const auto groupName = descriptorTypeStr(initialDescriptorType) + "_" + descriptorTypeStr(finalDescriptorType);
					GroupPtr combinationGroup(new tcu::TestCaseGroup(testCtx, groupName.c_str(), ""));
					createMutableTestVariants(testCtx, combinationGroup.get(), setPtr, reducedStages);
					subGroup->addChild(combinationGroup.release());
				}
			}

			singleCases->addChild(subGroup.release());
		}

		mainGroup->addChild(singleCases.release());
	}

	// Cases with a single non-mutable descriptor. This provides some basic checks to verify copying to non-mutable bindings works.
	{
		GroupPtr singleNonMutableGroup (new tcu::TestCaseGroup(testCtx, "single_nonmutable", "Tests using a single non-mutable descriptor"));

		for (const auto& descriptorType : basicDescriptorTypes)
		{
			DescriptorSet::BindingPtrVector bindings;
			bindings.emplace_back(new SingleBinding(descriptorType, std::vector<VkDescriptorType>()));
			DescriptorSetPtr descriptorSet (new DescriptorSet(bindings));

			const auto groupName = descriptorTypeStr(descriptorType);
			GroupPtr descGroup (new tcu::TestCaseGroup(testCtx, groupName.c_str(), ""));

			createMutableTestVariants(testCtx, descGroup.get(), descriptorSet, reducedStages);
			singleNonMutableGroup->addChild(descGroup.release());
		}

		mainGroup->addChild(singleNonMutableGroup.release());
	}

	const struct {
		bool unbounded;
		const char* name;
	} unboundedCases[] = {
		{false, "constant_size"},
		{true,  "unbounded"},
	};

	const struct {
		bool aliasing;
		const char* name;
	} aliasingCases[] = {
		{false, "noaliasing"},
		{true,  "aliasing"},
	};

	const struct {
		bool oneArrayOnly;
		bool mixNonMutable;
		const char* groupName;
		const char* groupDesc;
	} arrayCountGroups[] = {
		{true,  false, "one_array",             "Tests using an array of mutable descriptors"},
		{false, false, "multiple_arrays",       "Tests using multiple arrays of mutable descriptors"},
		{false, true,  "multiple_arrays_mixed", "Tests using multiple arrays of mutable descriptors mixed with arrays of nonmutable ones"},
	};

	for (const auto& variant : arrayCountGroups)
	{
		GroupPtr arrayGroup(new tcu::TestCaseGroup(testCtx, variant.groupName, variant.groupDesc));

		for (const auto& unboundedCase : unboundedCases)
		{
			GroupPtr unboundedGroup(new tcu::TestCaseGroup(testCtx, unboundedCase.name, ""));

			for (const auto& aliasingCase : aliasingCases)
			{
				GroupPtr aliasingGroup(new tcu::TestCaseGroup(testCtx, aliasingCase.name, ""));

				DescriptorSet::BindingPtrVector setBindings;

				// Prepare descriptors for this test variant.
				for (size_t mandatoryTypesRotation = 0; mandatoryTypesRotation < mandatoryTypes.size(); ++mandatoryTypesRotation)
				{
					const bool isLastBinding = (variant.oneArrayOnly || mandatoryTypesRotation == mandatoryTypes.size() - 1u);
					const bool isUnbounded   = (unboundedCase.unbounded && isLastBinding);

					// Create a rotation of the mandatory types for each mutable array binding.
					auto mandatoryTypesVector = mandatoryTypes;
					{
						const auto beginPtr = &mandatoryTypesVector[0];
						const auto endPtr   = beginPtr + mandatoryTypesVector.size();
						std::rotate(beginPtr, &mandatoryTypesVector[mandatoryTypesRotation], endPtr);
					}

					std::vector<SingleBinding> arrayBindings;

					if (aliasingCase.aliasing)
					{
						// With aliasing, the descriptor types rotate in each descriptor.
						for (size_t typeIdx = 0; typeIdx < mandatoryTypesVector.size(); ++typeIdx)
						{
							auto       rotatedTypes = mandatoryTypesVector;
							const auto beginPtr     = &rotatedTypes[0];
							const auto endPtr       = beginPtr + rotatedTypes.size();

							std::rotate(beginPtr, &rotatedTypes[typeIdx], endPtr);

							arrayBindings.emplace_back(VK_DESCRIPTOR_TYPE_MUTABLE_VALVE, rotatedTypes);
						}
					}
					else
					{
						// Without aliasing, all descriptors use the same type at the same time.
						const SingleBinding noAliasingBinding(VK_DESCRIPTOR_TYPE_MUTABLE_VALVE, mandatoryTypesVector);
						arrayBindings.resize(mandatoryTypesVector.size(), noAliasingBinding);
					}

					setBindings.emplace_back(new ArrayBinding(isUnbounded, arrayBindings));

					if (variant.mixNonMutable && !isUnbounded)
					{
						// Create a non-mutable array binding interleaved with the other ones.
						const SingleBinding nonMutableBinding(mandatoryTypes[mandatoryTypesRotation], std::vector<VkDescriptorType>());
						std::vector<SingleBinding> nonMutableBindings(mandatoryTypes.size(), nonMutableBinding);
						setBindings.emplace_back(new ArrayBinding(false, nonMutableBindings));
					}

					if (variant.oneArrayOnly)
						break;
				}

				DescriptorSetPtr descriptorSet(new DescriptorSet(setBindings));
				createMutableTestVariants(testCtx, aliasingGroup.get(), descriptorSet, computeOnly);

				unboundedGroup->addChild(aliasingGroup.release());
			}

			arrayGroup->addChild(unboundedGroup.release());
		}

		mainGroup->addChild(arrayGroup.release());
	}

	// Cases with a single mutable binding followed by an array of mutable bindings.
	// The array will use a single type beyond the mandatory ones.
	{
		GroupPtr singleAndArrayGroup(new tcu::TestCaseGroup(testCtx, "single_and_array", "Tests using a single mutable binding followed by a mutable array binding"));

		for (const auto& descriptorType : basicDescriptorTypes)
		{
			// Input attachments will not use arrays.
			if (descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
				continue;

			if (de::contains(begin(mandatoryTypes), end(mandatoryTypes), descriptorType))
				continue;

			const auto groupName = descriptorTypeStr(descriptorType);
			GroupPtr descTypeGroup(new tcu::TestCaseGroup(testCtx, groupName.c_str(), ""));

			for (const auto& aliasingCase : aliasingCases)
			{
				GroupPtr aliasingGroup(new tcu::TestCaseGroup(testCtx, aliasingCase.name, ""));

				DescriptorSet::BindingPtrVector setBindings;
				std::vector<SingleBinding> arrayBindings;

				// Single mutable descriptor as the first binding.
				setBindings.emplace_back(new SingleBinding(VK_DESCRIPTOR_TYPE_MUTABLE_VALVE, mandatoryTypes));

				// Descriptor array as the second binding.
				auto arrayBindingDescTypes = mandatoryTypes;
				arrayBindingDescTypes.push_back(descriptorType);

				if (aliasingCase.aliasing)
				{
					// With aliasing, the descriptor types rotate in each descriptor.
					for (size_t typeIdx = 0; typeIdx < arrayBindingDescTypes.size(); ++typeIdx)
					{
						auto       rotatedTypes = arrayBindingDescTypes;
						const auto beginPtr     = &rotatedTypes[0];
						const auto endPtr       = beginPtr + rotatedTypes.size();

						std::rotate(beginPtr, &rotatedTypes[typeIdx], endPtr);

						arrayBindings.emplace_back(VK_DESCRIPTOR_TYPE_MUTABLE_VALVE, rotatedTypes);
					}
				}
				else
				{
					// Without aliasing, all descriptors use the same type at the same time.
					const SingleBinding noAliasingBinding(VK_DESCRIPTOR_TYPE_MUTABLE_VALVE, arrayBindingDescTypes);
					arrayBindings.resize(arrayBindingDescTypes.size(), noAliasingBinding);
				}

				// Second binding: array binding.
				setBindings.emplace_back(new ArrayBinding(false/*unbounded*/, arrayBindings));

				// Create set and test variants.
				DescriptorSetPtr descriptorSet(new DescriptorSet(setBindings));
				createMutableTestVariants(testCtx, aliasingGroup.get(), descriptorSet, computeOnly);

				descTypeGroup->addChild(aliasingGroup.release());
			}

			singleAndArrayGroup->addChild(descTypeGroup.release());
		}

		mainGroup->addChild(singleAndArrayGroup.release());
	}

	// Cases with several mutable non-array bindings.
	{
		GroupPtr multipleGroup    (new tcu::TestCaseGroup(testCtx, "multiple", "Tests using multiple mutable bindings"));
		GroupPtr mutableOnlyGroup (new tcu::TestCaseGroup(testCtx, "mutable_only", "Tests using only mutable descriptors"));
		GroupPtr mixedGroup       (new tcu::TestCaseGroup(testCtx, "mixed", "Tests mixing mutable descriptors an non-mutable descriptors"));

		// Each descriptor will have a different type in every iteration, like in the one_array aliasing case.
		for (int groupIdx = 0; groupIdx < 2; ++groupIdx)
		{
			const bool mixed = (groupIdx == 1);
			DescriptorSet::BindingPtrVector setBindings;

			for (size_t typeIdx = 0; typeIdx < mandatoryTypes.size(); ++typeIdx)
			{
				auto       rotatedTypes = mandatoryTypes;
				const auto beginPtr     = &rotatedTypes[0];
				const auto endPtr       = beginPtr + rotatedTypes.size();

				std::rotate(beginPtr, &rotatedTypes[typeIdx], endPtr);
				setBindings.emplace_back(new SingleBinding(VK_DESCRIPTOR_TYPE_MUTABLE_VALVE, rotatedTypes));

				// Additional non-mutable binding interleaved with the mutable ones.
				if (mixed)
					setBindings.emplace_back(new SingleBinding(rotatedTypes[0], std::vector<VkDescriptorType>()));
			}
			DescriptorSetPtr descriptorSet(new DescriptorSet(setBindings));

			const auto dstGroup = (mixed ? mixedGroup.get() : mutableOnlyGroup.get());
			createMutableTestVariants(testCtx, dstGroup, descriptorSet, computeOnly);
		}

		multipleGroup->addChild(mutableOnlyGroup.release());
		multipleGroup->addChild(mixedGroup.release());
		mainGroup->addChild(multipleGroup.release());
	}

	return mainGroup.release();
}

} // BindingModel
} // vkt
