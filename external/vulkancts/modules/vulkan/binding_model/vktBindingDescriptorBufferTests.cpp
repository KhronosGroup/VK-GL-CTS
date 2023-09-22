/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 The Khronos Group Inc.
 * Copyright (C) 2022 Advanced Micro Devices, Inc.
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
 * \file vktBindingDescriptorBufferTests.cpp
 * \brief Descriptor buffer (extension) tests
 *//*--------------------------------------------------------------------*/

#include "deSharedPtr.hpp"
#include "deUniquePtr.hpp"
#include "deRandom.hpp"
#include "tcuCommandLine.hpp"
#include "vktBindingDescriptorBufferTests.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkStrUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkRayTracingUtil.hpp"

#include <algorithm>

// The defines below can be changed for debugging purposes, otherwise keep them as is.

#define DEBUG_FORCE_STAGED_UPLOAD			false	// false - prefer direct write to device-local memory
#define DEBUG_MIX_DIRECT_AND_STAGED_UPLOAD	true	// true  - use some staged uploads to test new access flag

// Workaround a framework script bug.
#ifndef VK_PIPELINE_STAGE_2_TRANSFER_BIT
#define VK_PIPELINE_STAGE_2_TRANSFER_BIT VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR
#endif

namespace vkt
{
namespace BindingModel
{
namespace
{
using namespace vk;
using de::MovePtr;
using de::UniquePtr;
using de::SharedPtr;

constexpr deUint32 INDEX_INVALID	= ~0u;
constexpr deUint32 OFFSET_UNUSED	= ~0u;
constexpr deUint32 HASH_MASK_FOR_AS	= (1u << 19) - 1;

constexpr deUint32	ConstResultBufferDwords		= 0x4;		// uvec4
constexpr deUint32	ConstInlineBlockDwords		= 0x40;		// 256 B spec minimum
constexpr deUint32	ConstUniformBufferDwords	= 0x1000;	// 16 KiB spec minimum
constexpr deUint32	ConstTexelBufferElements    = 512;
constexpr deUint32	ConstMaxDescriptorArraySize	= 3;		// at most define N-element descriptor arrays
constexpr deUint32  ConstRobustBufferAlignment  = 256;		// 256 is the worst-case alignment required by UBOs in robustness2
constexpr deUint32	ConstChecksPerBuffer		= 4;		// when verifying data in buffers, do at most N comparisons;
															// this is to avoid excessive shader execution time

constexpr VkComponentMapping ComponentMappingIdentity =
{
	VK_COMPONENT_SWIZZLE_IDENTITY,
	VK_COMPONENT_SWIZZLE_IDENTITY,
	VK_COMPONENT_SWIZZLE_IDENTITY,
	VK_COMPONENT_SWIZZLE_IDENTITY,
};

template<typename T>
inline deUint32 u32(const T& value)
{
	return static_cast<deUint32>(value);
}

template <typename T, typename... args_t>
inline de::MovePtr<T> newMovePtr(args_t&&... args)
{
	return de::MovePtr<T>(new T(::std::forward<args_t>(args)...));
}

template <typename T>
inline void reset(Move<T>& ptr)
{
	ptr = Move<T>();
}

template <typename T>
inline void reset(MovePtr<T>& ptr)
{
	ptr.clear();
}

template<typename T>
inline SharedPtr<UniquePtr<T> > makeSharedUniquePtr()
{
	return SharedPtr<UniquePtr<T> >(
		new UniquePtr<T>(
			new T()));
}

inline void* offsetPtr(void* ptr, VkDeviceSize offset)
{
	return reinterpret_cast<char*>(ptr) + offset;
}

inline const void* offsetPtr(const void* ptr, VkDeviceSize offset)
{
	return reinterpret_cast<const char*>(ptr) + offset;
}

// Calculate the byte offset of ptr from basePtr.
// This can be useful if an object at ptr is suballocated from a larger allocation at basePtr, for example.
inline std::size_t basePtrOffsetOf(const void* basePtr, const void* ptr)
{
	DE_ASSERT(basePtr <= ptr);
	return static_cast<std::size_t>(static_cast<const deUint8*>(ptr) - static_cast<const deUint8*>(basePtr));
}

deUint32 getShaderGroupHandleSize (const InstanceInterface& vki,
	const VkPhysicalDevice	physicalDevice)
{
	de::MovePtr<RayTracingProperties>	rayTracingPropertiesKHR;

	rayTracingPropertiesKHR = makeRayTracingProperties(vki, physicalDevice);

	return rayTracingPropertiesKHR->getShaderGroupHandleSize();
}

deUint32 getShaderGroupBaseAlignment (const InstanceInterface& vki,
	const VkPhysicalDevice	physicalDevice)
{
	de::MovePtr<RayTracingProperties>	rayTracingPropertiesKHR;

	rayTracingPropertiesKHR = makeRayTracingProperties(vki, physicalDevice);

	return rayTracingPropertiesKHR->getShaderGroupBaseAlignment();
}

VkBuffer getVkBuffer (const de::MovePtr<BufferWithMemory>& buffer)
{
	VkBuffer result = (buffer.get() == DE_NULL) ? DE_NULL : buffer->get();

	return result;
}

VkStridedDeviceAddressRegionKHR makeStridedDeviceAddressRegion (const DeviceInterface& vkd, const VkDevice device, VkBuffer buffer, VkDeviceSize size)
{
	const VkDeviceSize sizeFixed = ((buffer == DE_NULL) ? 0ull : size);

	return makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, buffer, 0), sizeFixed, sizeFixed);
}

VkDeviceAddress getAccelerationStructureDeviceAddress (DeviceDriver&				deviceDriver,
													   VkDevice						device,
													   VkAccelerationStructureKHR	accelerationStructure)
{
	const VkAccelerationStructureDeviceAddressInfoKHR	addressInfo =
	{
		VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,	// VkStructureType				sType
		DE_NULL,															// const void*					pNext
		accelerationStructure												// VkAccelerationStructureKHR	accelerationStructure
	};
	const VkDeviceAddress								deviceAddress = deviceDriver.getAccelerationStructureDeviceAddressKHR(device, &addressInfo);

	DE_ASSERT(deviceAddress != DE_NULL);

	return deviceAddress;
}

// Used to distinguish different test implementations.
enum class TestVariant : deUint32
{
	SINGLE,								// basic quick check for descriptor/shader combinations
	MULTIPLE,							// multiple buffer bindings with various descriptor types
	MAX,								// verify max(Sampler/Resource)DescriptorBufferBindings
	EMBEDDED_IMMUTABLE_SAMPLERS,		// various usages of embedded immutable samplers
	PUSH_DESCRIPTOR,					// use push descriptors and descriptor buffer at the same time
	PUSH_TEMPLATE,						// use push descriptor template and descriptor buffer at the same time
	ROBUST_BUFFER_ACCESS,				// robust buffer access
	ROBUST_NULL_DESCRIPTOR,				// robustness2 with null descriptor
	CAPTURE_REPLAY,						// capture and replay capability with descriptor buffers
};

// Optional; Used to add variations for a specific test case.
enum class SubCase : deUint32
{
	NONE,								// no sub case, i.e. a baseline test case
	IMMUTABLE_SAMPLERS,					// treat all samplers as immutable
	CAPTURE_REPLAY_CUSTOM_BORDER_COLOR,	// in capture/replay tests, test VK_EXT_custom_border_color interaction
	SINGLE_BUFFER,						// use push descriptors and descriptor buffer at the same time using single buffer
};

// A simplified descriptor binding, used to define the test case behavior at a high level.
struct SimpleBinding
{
	deUint32			set;
	deUint32			binding;
	VkDescriptorType	type;
	deUint32			count;
	deUint32			inputAttachmentIndex;

	bool				isResultBuffer;				// binding used for compute buffer results
	bool				isEmbeddedImmutableSampler;	// binding used as immutable embedded sampler
	bool				isRayTracingAS;				// binding used for raytracing acceleration structure
};

// Scan simple bindings for the binding with the compute and ray tracing shader's result storage buffer.
deUint32 getResultBufferIndex(const std::vector<SimpleBinding>& simpleBindings)
{
	bool	 found						= false;
	deUint32 resultBufferIndex	= 0;

	for (const auto& sb : simpleBindings)
	{
		if (sb.isResultBuffer)
		{
			found = true;

			break;
		}

		++resultBufferIndex;
	}

	if (!found)
	{
		resultBufferIndex = INDEX_INVALID;
	}

	return resultBufferIndex;
}


// Scan simple bindings for the binding with the ray tracing acceleration structure
deUint32 getRayTracingASIndex(const std::vector<SimpleBinding>& simpleBindings)
{
	deUint32 ndx	= 0;
	deUint32 result	= INDEX_INVALID;

	for (const auto& sb : simpleBindings)
	{
		if (sb.isRayTracingAS)
		{
			result = ndx;

			break;
		}

		++ndx;
	}

	DE_ASSERT(result != INDEX_INVALID);

	return result;
}

// The parameters for a test case (with the exclusion of simple bindings).
// Not all values are used by every test variant.
struct TestParams
{
	deUint32					hash;				// a value used to "salt" results in memory to get unique values per test case
	TestVariant					variant;			// general type of the test case
	SubCase						subcase;			// a variation of the specific test case
	VkShaderStageFlagBits		stage;				// which shader makes use of the bindings
	VkQueueFlagBits				queue;				// which queue to use for the access
	deUint32					bufferBindingCount;	// number of buffer bindings to create
	deUint32					setsPerBuffer;		// how may sets to put in one buffer binding
	bool						useMaintenance5;	// should we use VkPipelineCreateFlagBits2KHR

	// Basic, null descriptor, or capture/replay test
	VkDescriptorType			descriptor;			// descriptor type under test

	// Max bindings test and to check the supported limits in other cases
	deUint32					samplerBufferBindingCount;
	deUint32					resourceBufferBindingCount;

	// Max embedded immutable samplers test
	deUint32					embeddedImmutableSamplerBufferBindingCount;
	deUint32					embeddedImmutableSamplersPerBuffer;

	// Push descriptors
	deUint32					pushDescriptorSetIndex;		// which descriptor set is updated with push descriptor/template

	bool isCompute() const
	{
		return stage == VK_SHADER_STAGE_COMPUTE_BIT;
	}

	bool isGraphics() const
	{
		return (stage & VK_SHADER_STAGE_ALL_GRAPHICS) != 0;
	}

	bool isGeometry() const
	{
		return stage == VK_SHADER_STAGE_GEOMETRY_BIT;
	}

	bool isTessellation() const
	{
		return (stage & (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)) != 0;
	}

	bool isPushDescriptorTest() const
	{
		return (variant == TestVariant::PUSH_DESCRIPTOR) || (variant == TestVariant::PUSH_TEMPLATE);
	}

	bool isAccelerationStructure() const
	{
		return descriptor == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	}

	bool isRayTracing() const
	{
		return isAllRayTracingStages(stage);
	}

	// The resource accessed via this descriptor type has capture/replay enabled.
	bool isCaptureReplayDescriptor(VkDescriptorType otherType) const
	{
		return (variant == TestVariant::CAPTURE_REPLAY) && (descriptor == otherType);
	}

	bool isAccelerationStructureOptional () const
	{
		switch (variant)
		{
			case TestVariant::MULTIPLE:
			case TestVariant::PUSH_DESCRIPTOR:
			case TestVariant::PUSH_TEMPLATE:
				return true;
			default:
				return false;
		}
	}

	bool isAccelerationStructureObligatory () const
	{
		switch (variant)
		{
			case TestVariant::SINGLE:
			case TestVariant::ROBUST_NULL_DESCRIPTOR:
			case TestVariant::CAPTURE_REPLAY:
				return isAccelerationStructure();
			default:
				return false;
		}
	}

	// Update the hash field. Must be called after changing the value of any other parameters.
	void updateHash(uint32_t basehash)
	{
		hash = deUint32Hash(basehash);

		hash = isAccelerationStructure() ? (basehash & HASH_MASK_FOR_AS) : basehash;
	}
};

// A convenience holder for a buffer-related data.
struct BufferAlloc
{
	VkDeviceSize			size					= 0;
	VkDeviceAddress			deviceAddress			= 0;	// non-zero if used
	VkBufferUsageFlags		usage					= 0;
	uint64_t				opaqueCaptureAddress	= 0;

	Move<VkBuffer>			buffer;
	MovePtr<Allocation>		alloc;

	BufferAlloc() = default;
	BufferAlloc(BufferAlloc&) = delete;

	void loadDeviceAddress(const DeviceInterface& vk, VkDevice device)
	{
		VkBufferDeviceAddressInfo bdaInfo = initVulkanStructure();
		bdaInfo.buffer = *buffer;

		deviceAddress = vk.getBufferDeviceAddress(device, &bdaInfo);
	}
};

using BufferAllocPtr = SharedPtr<BufferAlloc>;

// A convenience holder for image-related data.
struct ImageAlloc
{
	VkImageCreateInfo		info					= {};
	VkDeviceSize			sizeBytes				= 0;
	VkImageLayout			layout					= VK_IMAGE_LAYOUT_UNDEFINED;	// layout used when image is accessed
	uint64_t				opaqueCaptureAddress	= 0;

	Move<VkImage>			image;
	Move<VkImageView>		imageView;
	MovePtr<Allocation>		alloc;

	ImageAlloc() = default;
	ImageAlloc(ImageAlloc&) = delete;
};

using ImageAllocPtr = SharedPtr<ImageAlloc>;

// A descriptor binding with supporting data.
class Binding
{
public:
    uint32_t				binding;
    VkDescriptorType		descriptorType;
    uint32_t				descriptorCount;
	VkShaderStageFlags		stageFlags;

	VkDeviceSize			offset;
	deUint32				inputAttachmentIndex;	// if used
	bool					isResultBuffer;			// used with compute shaders
	bool					isRayTracingAS;			// used with raytracing shaders

	bool					isTestableDescriptor() const
	{
		return !isRayTracingAS && !isResultBuffer;
	}

	// Index into the vector of resources in the main test class, if used.
	// It's an array, because a binding may have several arrayed descriptors.
	deUint32				perBindingResourceIndex[ConstMaxDescriptorArraySize];

	// An array of immutable samplers, if used by the binding.
	VkSampler				immutableSamplers[ConstMaxDescriptorArraySize];

	Binding()
		: binding(0)
		, descriptorType(VK_DESCRIPTOR_TYPE_SAMPLER)
		, descriptorCount(0)
		, stageFlags(0)
		, offset(0)
		, inputAttachmentIndex(0)
		, isResultBuffer(false)
		, isRayTracingAS(false)
	{
		for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(perBindingResourceIndex); ++i)
		{
			perBindingResourceIndex[i]	= INDEX_INVALID;
			immutableSamplers[i]		= 0;
		}
	}
};

// Get an array of descriptor bindings, this is used in descriptor set layout creation.
std::vector<VkDescriptorSetLayoutBinding> getDescriptorSetLayoutBindings(const std::vector<Binding>& allBindings)
{
	std::vector<VkDescriptorSetLayoutBinding> result;
	result.reserve(allBindings.size());

	for (auto& binding : allBindings)
	{
		VkDescriptorSetLayoutBinding dslBinding {};
		dslBinding.binding				= binding.binding;
		dslBinding.descriptorType		= binding.descriptorType;
		dslBinding.descriptorCount		= binding.descriptorCount;
		dslBinding.stageFlags			= binding.stageFlags;

		if (binding.immutableSamplers[0] != DE_NULL)
		{
			dslBinding.pImmutableSamplers = binding.immutableSamplers;
		}

		result.emplace_back(dslBinding);
	}

	return result;
}

// Descriptor data used with push descriptors (regular and templates).
struct PushDescriptorData
{
	VkDescriptorImageInfo		imageInfos[ConstMaxDescriptorArraySize];
	VkDescriptorBufferInfo		bufferInfos[ConstMaxDescriptorArraySize];
	VkBufferView				texelBufferViews[ConstMaxDescriptorArraySize];
	VkAccelerationStructureKHR	accelerationStructures[ConstMaxDescriptorArraySize];
};

// A convenience holder for a descriptor set layout and its bindings.
struct DescriptorSetLayoutHolder
{
	std::vector<Binding>			bindings;

	Move<VkDescriptorSetLayout>		layout;
	VkDeviceSize					sizeOfLayout					= 0;
	deUint32						bufferIndex						= INDEX_INVALID;
	VkDeviceSize					bufferOffset					= 0;
	VkDeviceSize					stagingBufferOffset				= OFFSET_UNUSED;
	bool							hasEmbeddedImmutableSamplers	= false;
	bool							usePushDescriptors				= false;	// instead of descriptor buffer

	DescriptorSetLayoutHolder() = default;
	DescriptorSetLayoutHolder(DescriptorSetLayoutHolder&) = delete;
};

using DSLPtr = SharedPtr<UniquePtr<DescriptorSetLayoutHolder> >;

// Get an array of descriptor set layouts.
std::vector<VkDescriptorSetLayout> getDescriptorSetLayouts(const std::vector<DSLPtr>& dslPtrs)
{
	std::vector<VkDescriptorSetLayout> result;
	result.reserve(dslPtrs.size());

	for (auto& pDsl : dslPtrs)
	{
		result.emplace_back((**pDsl).layout.get());
	}

	return result;
}

// A helper struct to keep descriptor's underlying resource data.
// This is intended to be flexible and support a mix of buffer/image/sampler, depending on the binding type.
struct ResourceHolder
{
	BufferAlloc									buffer;
	ImageAlloc									image;
	Move<VkSampler>								sampler;
	Move<VkSamplerYcbcrConversion>				samplerYcbcrConversion;
	Move<VkBufferView>							bufferView;
	SharedPtr<BottomLevelAccelerationStructure>	rtBlas;
	MovePtr<TopLevelAccelerationStructure>		rtTlas;

	struct
	{
		std::vector<deUint8>	bufferData;
		std::vector<deUint8>	imageData;
		std::vector<deUint8>	imageViewData;
		std::vector<deUint8>	samplerData;
		std::vector<deUint8>	accelerationStructureDataBlas;
		std::vector<deUint8>	accelerationStructureDataTlas;
	} captureReplay;

	ResourceHolder() = default;
	ResourceHolder(ResourceHolder&) = delete;
};

using ResourcePtr = SharedPtr<UniquePtr<ResourceHolder> >;

// Used in test case name generation.
std::string toString (VkQueueFlagBits queue)
{
	switch (queue)
	{
	case VK_QUEUE_GRAPHICS_BIT:		return "graphics";
	case VK_QUEUE_COMPUTE_BIT:		return "compute";

	default:
		DE_ASSERT(false);
		break;
	}
	return "";
}

// Used in test case name generation.
std::string toString (VkDescriptorType type)
{
	switch (type)
	{
	case VK_DESCRIPTOR_TYPE_SAMPLER:					return "sampler";
	case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:		return "combined_image_sampler";
	case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:				return "sampled_image";
	case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:				return "storage_image";
	case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:		return "uniform_texel_buffer";
	case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:		return "storage_texel_buffer";
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:				return "uniform_buffer";
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:				return "storage_buffer";
	case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:			return "input_attachment";
	case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK:		return "inline_uniform_block";
	case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR: return "acceleration_structure";

	default:
		DE_ASSERT(false);
		break;
	}
	return "";
}

// Used in test case name generation.
std::string toString (VkShaderStageFlagBits stage)
{
	switch (stage)
	{
		case VK_SHADER_STAGE_VERTEX_BIT:					return "vert";
		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:		return "tesc";
		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:	return "tese";
		case VK_SHADER_STAGE_GEOMETRY_BIT:					return "geom";
		case VK_SHADER_STAGE_FRAGMENT_BIT:					return "frag";
		case VK_SHADER_STAGE_COMPUTE_BIT:					return "comp";
		case VK_SHADER_STAGE_RAYGEN_BIT_KHR:				return "rgen";
		case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:				return "ahit";
		case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:			return "chit";
		case VK_SHADER_STAGE_MISS_BIT_KHR:					return "miss";
		case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:			return "sect";
		case VK_SHADER_STAGE_CALLABLE_BIT_KHR:				return "call";

		default:
			DE_ASSERT(false);
			break;
	}

	return "";
}

// Used in test case name generation.
std::string getCaseNameUpdateHash(TestParams& params, uint32_t baseHash)
{
	std::ostringstream str;

	str << toString(params.queue)
		<< "_"
		<< toString(params.stage);

	if ((params.variant == TestVariant::SINGLE) ||
		(params.variant == TestVariant::ROBUST_NULL_DESCRIPTOR) ||
		(params.variant == TestVariant::CAPTURE_REPLAY))
	{
		str << "_" << toString(params.descriptor);

		if (params.subcase == SubCase::CAPTURE_REPLAY_CUSTOM_BORDER_COLOR)
		{
			str << "_custom_border_color";
		}
	}
	else if (params.variant == TestVariant::MULTIPLE)
	{
		str << "_buffers" << params.bufferBindingCount
			<< "_sets" << params.setsPerBuffer;
	}
	else if (params.variant == TestVariant::MAX)
	{
		str << "_sampler" << params.samplerBufferBindingCount
			<< "_resource" << params.resourceBufferBindingCount;
	}
	else if (params.variant == TestVariant::EMBEDDED_IMMUTABLE_SAMPLERS)
	{
		str << "_buffers" << params.embeddedImmutableSamplerBufferBindingCount
			<< "_samplers" << params.embeddedImmutableSamplersPerBuffer;
	}
	else if (params.isPushDescriptorTest())
	{
		str << "_sets" << (params.bufferBindingCount + 1)
			<< "_push_set" << params.pushDescriptorSetIndex
			<< ((params.subcase == SubCase::SINGLE_BUFFER) ? "_single_buffer" : "");
	}

	if (params.subcase == SubCase::IMMUTABLE_SAMPLERS)
	{
		str << "_imm_samplers";
	}

	params.updateHash(baseHash ^ deStringHash(str.str().c_str()));

	return str.str();
}

// Used by shaders to identify a specific binding.
deUint32 packBindingArgs(deUint32 set, deUint32 binding, deUint32 arrayIndex)
{
	DE_ASSERT(set		 < 0x40);
	DE_ASSERT(binding	 < 0x40);
	DE_ASSERT(arrayIndex < 0x80);

	return (arrayIndex << 12) | ((set & 0x3Fu) << 6) | (binding & 0x3Fu);
}

// Used by shaders to identify a specific binding.
void unpackBindingArgs(deUint32 packed, deUint32* pOutSet, deUint32* pBinding, deUint32* pArrayIndex)
{
	if (pBinding != nullptr)
	{
		*pBinding = packed & 0x3Fu;
	}
	if (pOutSet != nullptr)
	{
		*pOutSet = (packed >> 6) & 0x3Fu;
	}
	if (pArrayIndex != nullptr)
	{
		*pArrayIndex = (packed >> 12) & 0x7Fu;
	}
}

// The expected data read through a descriptor. Try to get a unique value per test and binding.
deUint32 getExpectedData(deUint32 hash, deUint32 set, deUint32 binding, deUint32 arrayIndex = 0)
{
	return hash ^ packBindingArgs(set, binding, arrayIndex);
}

// Used by shaders.
std::string glslFormat(deUint32 value)
{
	return std::to_string(value) + "u";
}

// Generate a unique shader resource name for a binding.
std::string glslResourceName(deUint32 set, deUint32 binding)
{
	// A generic name for any accessible shader binding.
	std::ostringstream str;
	str << "res_" << set << "_" << binding;
	return str.str();
}

// Generate GLSL that declares a descriptor binding.
std::string glslDeclareBinding(
	VkDescriptorType	type,
	deUint32			set,
	deUint32			binding,
	deUint32			count,
	deUint32			attachmentIndex,
	deUint32			bufferArraySize)
{
	std::ostringstream str;

	str << "layout(set = " << set << ", binding = " << binding;

	// Additional layout information
	switch (type)
	{
	case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
	case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
		str << ", r32ui) ";
		break;
	case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
		str << ", input_attachment_index = " << attachmentIndex << ") ";
		break;
	default:
		str << ") ";
		break;
	}

	switch (type)
	{
	case VK_DESCRIPTOR_TYPE_SAMPLER:
		str << "uniform sampler ";
		break;
	case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
		str << "uniform usampler2D ";
		break;
	case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
		str << "uniform utexture2D ";
		break;
	case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
		str << "uniform uimage2D ";
		break;
	case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
		str << "uniform utextureBuffer ";
		break;
	case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
		str << "uniform uimageBuffer ";
		break;
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
	case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK:
		DE_ASSERT(bufferArraySize != 0);
		DE_ASSERT((bufferArraySize % 4) == 0);
		// std140 layout rules, each array element is aligned to 16 bytes.
		// Due to this, we will use uvec4 instead to access all dwords.
		str << "uniform Buffer_" << set << "_" << binding << " {\n"
			<< "    uvec4 data[" << (bufferArraySize / 4) << "];\n"
			<< "} ";
		break;
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
		DE_ASSERT(bufferArraySize != 0);
		str << "buffer Buffer_" << set << "_" << binding << " {\n"
			<< "    uint data[" << bufferArraySize << "];\n"
			<< "} ";
		break;
	case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
		str << "uniform usubpassInput ";
		break;
	case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
		str << "uniform accelerationStructureEXT ";
		break;
	default:
		DE_ASSERT(0);
		break;
	}

	str << glslResourceName(set, binding);

	if (count > 1)
	{
		str << "[" << count << "];\n";
	}
	else
	{
		str << ";\n";
	}

	return str.str();
}

// Generate all GLSL descriptor set/binding declarations.
std::string glslGlobalDeclarations(const TestParams& params, const std::vector<SimpleBinding>& simpleBindings, bool accStruct)
{
	DE_UNREF(params);

	std::ostringstream str;

	if (accStruct)
		str << "#extension GL_EXT_ray_query : require\n";

	for (const auto& sb : simpleBindings)
	{
		const deUint32 arraySize =
			sb.isResultBuffer ? ConstResultBufferDwords :
			(sb.type == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK) ? ConstInlineBlockDwords : ConstUniformBufferDwords;

		str << glslDeclareBinding(sb.type, sb.set, sb.binding, sb.count, sb.inputAttachmentIndex, arraySize);
	}

	if (accStruct)
	{
		str << ""
			"uint queryAS(accelerationStructureEXT rayQueryTopLevelAccelerationStructure)\n"
			"{\n"
			"	const uint  rayFlags = gl_RayFlagsNoOpaqueEXT;\n"
			"	const uint  cullMask = 0xFF;\n"
			"	const float tmin     = 0.0f;\n"
			"	const float tmax     = 524288.0f; // 2^^19\n"
			"	const vec3  origin   = vec3(0.0f, 0.0f, 0.0f);\n"
			"	const vec3  direct   = vec3(0.0f, 0.0f, 1.0f);\n"
			"	rayQueryEXT rayQuery;\n"
			"\n"
			"	rayQueryInitializeEXT(rayQuery, rayQueryTopLevelAccelerationStructure, rayFlags, cullMask, origin, tmin, direct, tmax);\n"
			"\n"
			"	if (rayQueryProceedEXT(rayQuery))\n"
			"	{\n"
			"		if (rayQueryGetIntersectionTypeEXT(rayQuery, false) == gl_RayQueryCandidateIntersectionTriangleEXT)\n"
			"		{\n"
			"			return uint(round(rayQueryGetIntersectionTEXT(rayQuery, false)));\n"
			"		}\n"
			"	}\n"
			"\n"
			"	return 0u;\n"
			"}\n"
			"\n";
	}

	return str.str();
}

// This function is used to return additional diagnostic information for a failed descriptor binding.
// For example, result Y is the packed binding information and result Z is the array index (for arrayed descriptors, or buffers).
std::string glslResultBlock(const std::string& indent, const std::string& resultY, const std::string& resultZ = "")
{
	std::ostringstream str;
	str << "{\n"
		<< indent << "	result.x += 1;\n"
		<< indent << "} else if (result.y == 0) {\n"
		<< indent << "	result.y = " << resultY << ";\n";

	if (!resultZ.empty())
	{
		str << indent << "	result.z = " << resultZ << ";\n";
	}

	str << indent << "}\n";
	return str.str();
}

// Get the number of iterations required to access all elements of a buffer.
// This mainly exists because we access UBOs as uvec4.
inline deUint32 getBufferLoopIterations(VkDescriptorType type)
{
	switch (type)
	{
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
		return ConstUniformBufferDwords / 4;

	case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK:
		return ConstInlineBlockDwords / 4;

	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
		return ConstUniformBufferDwords;

	case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
	case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
		return ConstTexelBufferElements;

	default:
		// Ignored
		return 0;
	}
}

// Generate GLSL that reads through the binding and compares the value.
// Successful reads increment a counter, while failed read will write back debug information.
std::string glslOutputVerification(const TestParams& params, const std::vector<SimpleBinding>& simpleBindings, bool)
{
	std::ostringstream str;

	if ((params.variant == TestVariant::SINGLE) ||
		(params.variant == TestVariant::MULTIPLE) ||
		(params.variant == TestVariant::PUSH_DESCRIPTOR) ||
		(params.variant == TestVariant::PUSH_TEMPLATE) ||
		(params.variant == TestVariant::ROBUST_NULL_DESCRIPTOR) ||
		(params.variant == TestVariant::CAPTURE_REPLAY))
	{
		// Read at least one value from a descriptor and compare it.
		// For buffers, verify every element.
		//
		// With null descriptors, reads must always return zero.

		for (const auto& sb : simpleBindings)
		{
			deUint32 samplerIndex = INDEX_INVALID;

			if (sb.isResultBuffer || sb.isRayTracingAS)
			{
				// Used by other bindings.
				continue;
			}

			if (sb.type == VK_DESCRIPTOR_TYPE_SAMPLER)
			{
				// Used by sampled images.
				continue;
			}
			else if (sb.type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
			{
				// Sampled images require a sampler to use.
				// Find a suitable sampler within the same descriptor set.

				bool found = false;
				samplerIndex = 0;

				for (const auto& sb1 : simpleBindings)
				{
					if ((sb.set == sb1.set) && (sb1.type == VK_DESCRIPTOR_TYPE_SAMPLER))
					{
						found = true;
						break;
					}

					++samplerIndex;
				}

				if (!found)
				{
					samplerIndex = INDEX_INVALID;
				}
			}

			const deUint32 bufferLoopIterations	= getBufferLoopIterations(sb.type);
			const deUint32 loopIncrement		= bufferLoopIterations / (ConstChecksPerBuffer - 1);

			// Ensure we won't miss the last check (the index will always be less than the buffer length).
			DE_ASSERT((bufferLoopIterations == 0) || ((bufferLoopIterations % (ConstChecksPerBuffer - 1)) != 0));

			const bool isNullDescriptor		= (params.variant == TestVariant::ROBUST_NULL_DESCRIPTOR) && (sb.type == params.descriptor);
			const bool isCustomBorderColor  = (params.subcase == SubCase::CAPTURE_REPLAY_CUSTOM_BORDER_COLOR);

			for (deUint32 arrayIndex = 0; arrayIndex < sb.count; ++arrayIndex)
			{
				// Input attachment index increases with array index.
				const auto expectedData		   = glslFormat(isNullDescriptor ? 0 : getExpectedData(params.hash, sb.set, sb.binding, sb.inputAttachmentIndex + arrayIndex));
				const auto expectedBorderColor = isNullDescriptor ? "uvec4(0)" : isCustomBorderColor ? "uvec4(2, 0, 0, 1)" : "uvec4(0, 0, 0, 1)";
				const auto bindingArgs		   = glslFormat(packBindingArgs(sb.set, sb.binding, sb.inputAttachmentIndex + arrayIndex));
				const auto& subscript		   = (sb.count > 1) ? "[" + std::to_string(arrayIndex) + "]" : "";

				if (sb.type == VK_DESCRIPTOR_TYPE_SAMPLER)
				{
					TCU_THROW(InternalError, "Sampler is tested implicitly");
				}
				else if (sb.type == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
				{
					str << "    if (queryAS(" << glslResourceName(sb.set, sb.binding) << subscript << ") == " << expectedData << ") " << glslResultBlock("\t", bindingArgs);
				}
				else if (sb.type == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
				{
					str << "	if (subpassLoad(" << glslResourceName(sb.set, sb.binding) << subscript << ").r == " << expectedData << ") " << glslResultBlock("\t", bindingArgs);
				}
				else if (sb.type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
				{
					DE_ASSERT(samplerIndex != INDEX_INVALID);
					const auto& samplerSb		 = simpleBindings[samplerIndex];
					const auto& samplerSubscript = (samplerSb.count > 1) ? "[" + std::to_string(arrayIndex % samplerSb.count) + "]" : "";

					// With samplers, verify the image color and the border color.

					std::stringstream samplerStr;
					samplerStr << "usampler2D("
							   << glslResourceName(sb.set, sb.binding)				 << subscript << ", "
							   << glslResourceName(samplerSb.set, samplerSb.binding) << samplerSubscript << ")";

					str << "	if ((textureLod(" << samplerStr.str() << ", vec2(0, 0), 0).r == " << expectedData << ") &&\n"
						<< "	    (textureLod(" << samplerStr.str() << ", vec2(-1, 0), 0) == " << expectedBorderColor << ")) "
						<< glslResultBlock("\t", bindingArgs);
				}
				else if (sb.type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
				{
					str << "	if ((textureLod(" << glslResourceName(sb.set, sb.binding) << subscript << ", vec2(0, 0), 0).r == " << expectedData << ") &&\n"
						<< "	    (textureLod(" << glslResourceName(sb.set, sb.binding) << subscript << ", vec2(-1, 0), 0) == " << expectedBorderColor << ")) "
						<< glslResultBlock("\t", bindingArgs);
				}
				else if (sb.type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
				{
					str << "	if (imageLoad(" << glslResourceName(sb.set, sb.binding) << subscript << ", ivec2(0, 0)).r == " << expectedData << ") " << glslResultBlock("\t", bindingArgs);
				}
				else if ((sb.type == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER) ||
						 (sb.type == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER))
				{
					const auto loadOp	= (sb.type == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER) ? "texelFetch" : "imageLoad";
					const auto loopData = isNullDescriptor ? expectedData : "(" + expectedData + " + i)";

					str << "	for (uint i = 0; i < " << glslFormat(bufferLoopIterations) << "; i += " << glslFormat(loopIncrement) << ") {\n"
						<< "		uint value = " << loadOp << "(" << glslResourceName(sb.set, sb.binding) << subscript << ", int(i)).r;\n"
						<< "		if (value == " << loopData << ") " << glslResultBlock("\t\t", bindingArgs, "i")
						<< "	}\n";
				}
				else if ((sb.type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) ||
						 (sb.type == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK))
				{
					const auto loopData0 = isNullDescriptor ? expectedData : "(" + expectedData + " + 4 * i + 0)";
					const auto loopData1 = isNullDescriptor ? expectedData : "(" + expectedData + " + 4 * i + 1)";
					const auto loopData2 = isNullDescriptor ? expectedData : "(" + expectedData + " + 4 * i + 2)";
					const auto loopData3 = isNullDescriptor ? expectedData : "(" + expectedData + " + 4 * i + 3)";

					str << "	for (uint i = 0; i < " << glslFormat(bufferLoopIterations) << "; i += " << glslFormat(loopIncrement) << ") {\n"
						<< "		uvec4 value = " << glslResourceName(sb.set, sb.binding) << subscript << ".data[i];\n"
						<< "		if (value.x == " << loopData0 << ") " << glslResultBlock("\t\t", bindingArgs, "4 * i + 0")
						<< "		if (value.y == " << loopData1 << ") " << glslResultBlock("\t\t", bindingArgs, "4 * i + 1")
						<< "		if (value.z == " << loopData2 << ") " << glslResultBlock("\t\t", bindingArgs, "4 * i + 2")
						<< "		if (value.w == " << loopData3 << ") " << glslResultBlock("\t\t", bindingArgs, "4 * i + 3")
						<< "	}\n";
				}
				else if (sb.type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
				{
					const auto loopData = isNullDescriptor ? expectedData : "(" + expectedData + " + i)";

					str << "	for (uint i = 0; i < " << glslFormat(bufferLoopIterations) << "; i += " << glslFormat(loopIncrement) << ") {\n"
						<< "		uint value = " << glslResourceName(sb.set, sb.binding) << subscript << ".data[i];\n"
						<< "		if (value == " << loopData << ") " << glslResultBlock("\t\t", bindingArgs, "i")
						<< "	}\n";
				}
				else
				{
					DE_ASSERT(0);
				}
			}
		}
	}
	else if (params.variant == TestVariant::ROBUST_BUFFER_ACCESS)
	{
		// With robust buffer tests, the buffer is always filled with zeros and we read with an offset that will
		// eventually cause us to read past the end of the buffer.

		for (const auto& sb : simpleBindings)
		{
			if (sb.isResultBuffer || sb.isRayTracingAS)
			{
				// Used by other bindings.
				continue;
			}

			const deUint32 bufferLoopIterations	= getBufferLoopIterations(sb.type);
			const deUint32 loopIncrement		= bufferLoopIterations / (ConstChecksPerBuffer - 1);
			const auto	   iterationOffsetStr	= glslFormat(bufferLoopIterations / 2);

			// Ensure we won't miss the last check (the index will always be less than the buffer length).
			DE_ASSERT((bufferLoopIterations == 0) || ((bufferLoopIterations % (ConstChecksPerBuffer - 1)) != 0));

			for (deUint32 arrayIndex = 0; arrayIndex < sb.count; ++arrayIndex)
			{
				const auto bindingArgs  = glslFormat(packBindingArgs(sb.set, sb.binding, sb.inputAttachmentIndex + arrayIndex));
				const auto& subscript   = (sb.count > 1) ? "[" + std::to_string(arrayIndex) + "]" : "";

				switch (sb.type)
				{
				case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
					str << "	for (uint i = 0; i < " << glslFormat(bufferLoopIterations) << ";  i += " << glslFormat(loopIncrement) << ") {\n"
						<< "		if (texelFetch(" << glslResourceName(sb.set, sb.binding) << subscript << ", int(i + " << iterationOffsetStr << ")).r == 0) " << glslResultBlock("\t\t", bindingArgs, "i + " + iterationOffsetStr)
						<< "	}\n";
					break;

				case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
					str << "	for (uint i = 0; i < " << glslFormat(bufferLoopIterations) << ";  i += " << glslFormat(loopIncrement) << ") {\n"
						<< "		if (imageLoad(" << glslResourceName(sb.set, sb.binding) << subscript << ", int(i + " << iterationOffsetStr << ")).r == 0) " << glslResultBlock("\t\t", bindingArgs, "i + " + iterationOffsetStr)
						<< "	}\n";
					break;

				case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
					str << "	for (uint i = 0; i < " << glslFormat(bufferLoopIterations) << ";  i += " << glslFormat(loopIncrement) << ") {\n"
						<< "		if (" << glslResourceName(sb.set, sb.binding) << subscript << ".data[i + " << iterationOffsetStr << "].x == 0) " << glslResultBlock("\t\t", bindingArgs, "4 * i + " + iterationOffsetStr + " + 0")
						<< "		if (" << glslResourceName(sb.set, sb.binding) << subscript << ".data[i + " << iterationOffsetStr << "].y == 0) " << glslResultBlock("\t\t", bindingArgs, "4 * i + " + iterationOffsetStr + " + 1")
						<< "		if (" << glslResourceName(sb.set, sb.binding) << subscript << ".data[i + " << iterationOffsetStr << "].z == 0) " << glslResultBlock("\t\t", bindingArgs, "4 * i + " + iterationOffsetStr + " + 2")
						<< "		if (" << glslResourceName(sb.set, sb.binding) << subscript << ".data[i + " << iterationOffsetStr << "].w == 0) " << glslResultBlock("\t\t", bindingArgs, "4 * i + " + iterationOffsetStr + " + 3")
						<< "	}\n";
					break;

				case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
					str << "	for (uint i = 0; i < " << glslFormat(bufferLoopIterations) << ";  i += " << glslFormat(loopIncrement) << ") {\n"
						<< "		if (" << glslResourceName(sb.set, sb.binding) << subscript << ".data[i + " << iterationOffsetStr << "] == 0) " << glslResultBlock("\t\t", bindingArgs, "i + " + iterationOffsetStr)
						<< "	}\n";
					break;

				default:
					DE_ASSERT(0);
					break;
				}
			}
		}
	}
	else if (params.variant == TestVariant::MAX)
	{
		std::vector<deUint32> samplerIndices;
		std::vector<deUint32> imageIndices;

		for (deUint32 i = 0; i < u32(simpleBindings.size()); ++i)
		{
			const auto& binding = simpleBindings[i];

			if (binding.type == VK_DESCRIPTOR_TYPE_SAMPLER)
			{
				samplerIndices.emplace_back(i);
			}
			else if (binding.type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
			{
				imageIndices.emplace_back(i);
			}
			// Ignore other descriptors, if any.
		}

		// Ensure that all samplers and images are accessed at least once. If we run out of one, simply reuse it.

		const auto maxIndex = deMaxu32(u32(samplerIndices.size()), u32(imageIndices.size()));

		for (deUint32 index = 0; index < maxIndex; ++index)
		{
			const auto& samplerBinding = simpleBindings[samplerIndices[index % samplerIndices.size()]];
			const auto& imageBinding   = simpleBindings[imageIndices[index % imageIndices.size()]];

			const auto expectedData		  = glslFormat(getExpectedData(params.hash, imageBinding.set, imageBinding.binding, 0));
			const auto imageBindingArgs   = glslFormat(packBindingArgs(imageBinding.set, imageBinding.binding, 0));
			const auto samplerBindingArgs = glslFormat(packBindingArgs(samplerBinding.set, samplerBinding.binding, 0));

			std::stringstream samplerStr;
			samplerStr << "usampler2D("
				<< glslResourceName(imageBinding.set, imageBinding.binding) << ", "
				<< glslResourceName(samplerBinding.set, samplerBinding.binding) << ")";

			str << "	if ((textureLod(" << samplerStr.str() << ", vec2(0, 0), 0).r == " << expectedData << ") &&\n"
				<< "	    (textureLod(" << samplerStr.str() << ", vec2(-1, 0), 0) == uvec4(0, 0, 0, 1))) "
				<< glslResultBlock("\t", imageBindingArgs, samplerBindingArgs);
		}
	}
	else if (params.variant == TestVariant::EMBEDDED_IMMUTABLE_SAMPLERS)
	{
		// The first few sets contain only samplers.
		// Then the last set contains only images.
		// Optionally, the last binding of that set is the compute result buffer.

		deUint32 firstImageIndex = 0;
		deUint32 lastImageIndex = 0;

		for (deUint32 i = 0; i < u32(simpleBindings.size()); ++i)
		{
			const auto& binding = simpleBindings[i];

			if (binding.type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
			{
				if (firstImageIndex == 0)
				{
					firstImageIndex = i;
				}

				lastImageIndex = i;
			}
		}

		DE_ASSERT(firstImageIndex == (lastImageIndex + 1 - firstImageIndex));	// same number of images and samplers

		for (deUint32 imageIndex = firstImageIndex; imageIndex <= lastImageIndex; ++imageIndex)
		{
			const auto& imageBinding = simpleBindings[imageIndex];
			const auto expectedData	 = glslFormat(getExpectedData(params.hash, imageBinding.set, imageBinding.binding, 0));
			const auto bindingArgs	 = glslFormat(packBindingArgs(imageBinding.set, imageBinding.binding, 0));

			DE_ASSERT(imageBinding.type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);

			const auto& samplerBinding	  = simpleBindings[imageIndex - firstImageIndex];
			const auto samplerBindingArgs = glslFormat(packBindingArgs(samplerBinding.set, samplerBinding.binding, 0));

			std::stringstream samplerStr;
			samplerStr << "usampler2D("
				<< glslResourceName(imageBinding.set, imageBinding.binding) << ", "
				<< glslResourceName(samplerBinding.set, samplerBinding.binding) << ")";

			str << "	if ((textureLod(" << samplerStr.str() << ", vec2(0, 0), 0).r == " << expectedData << ") &&\n"
				<< "	    (textureLod(" << samplerStr.str() << ", vec2(-1, 0), 0) == uvec4(0, 0, 0, 1))) "
				<< glslResultBlock("\t", bindingArgs, samplerBindingArgs);
		}
	}
	else
	{
		TCU_THROW(InternalError, "Not implemented");
	}

	// Compute shaders write the result to a storage buffer.
	const deUint32 computeResultBufferIndex = getResultBufferIndex(simpleBindings);

	if (computeResultBufferIndex != INDEX_INVALID)
	{
		DE_ASSERT(params.isCompute() || params.isRayTracing());
		const auto& resultSb = simpleBindings[computeResultBufferIndex];

		str << "	" << glslResourceName(resultSb.set, resultSb.binding) << ".data[0] = result.x;\n";
		str << "	" << glslResourceName(resultSb.set, resultSb.binding) << ".data[1] = result.y;\n";
		str << "	" << glslResourceName(resultSb.set, resultSb.binding) << ".data[2] = result.z;\n";
		str << "	" << glslResourceName(resultSb.set, resultSb.binding) << ".data[3] = result.w;\n";
	}

	return str.str();
}

// Base class for all test cases.
class DescriptorBufferTestCase : public TestCase
{
public:
	DescriptorBufferTestCase(
		tcu::TestContext& testCtx,
		const std::string& name,
		const std::string& description,
		const TestParams& params)

		: TestCase(testCtx, name, description)
		, m_params(params)
		, m_rng(params.hash)
	{
	}

	void			delayedInit		();
	void			initPrograms	(vk::SourceCollections&				programCollection) const;
	void			initPrograms	(vk::SourceCollections&				programCollection,
									 const std::vector<SimpleBinding>&	simpleBinding,
									 bool								accStruct,
									 bool								addService) const;
	TestInstance*	createInstance	(Context& context) const;
	void			checkSupport	(Context& context) const;

private:
	const TestParams			m_params;
	de::Random					m_rng;
	std::vector<SimpleBinding>	m_simpleBindings;
};

// Based on the basic test parameters, this function creates a number of sets/bindings that will be tested.
void DescriptorBufferTestCase::delayedInit()
{
	if ((m_params.variant == TestVariant::SINGLE) ||
		(m_params.variant == TestVariant::CAPTURE_REPLAY))
	{
		// Creates a single set with a single binding, unless additional helper resources are required.
		{
			SimpleBinding sb {};
			sb.set					= 0;
			sb.binding				= 0;
			sb.type					= m_params.descriptor;
			sb.count				= 1;

			// For inline uniforms we still use count = 1. The byte size is implicit in our tests.

			m_simpleBindings.emplace_back(sb);
		}

		// Sampled images require a sampler as well.
		if (m_params.descriptor == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
		{
			SimpleBinding sb {};
			sb.set					= 0;
			sb.binding				= u32(m_simpleBindings.size());
			sb.type					= VK_DESCRIPTOR_TYPE_SAMPLER;
			sb.count				= 1;

			m_simpleBindings.emplace_back(sb);
		}
		else if (m_params.isCaptureReplayDescriptor(VK_DESCRIPTOR_TYPE_SAMPLER))
		{
			// Samplers are usually tested implicitly, but with capture replay they are the target of specific API commands.
			// Add a sampled image to acompany the sampler.

			SimpleBinding sb {};
			sb.set					= 0;
			sb.binding				= u32(m_simpleBindings.size());
			sb.type					= VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			sb.count				= 1;

			m_simpleBindings.emplace_back(sb);
		}

		// For compute shaders add a result buffer as the last binding of the first set.
		if (m_params.isCompute() || m_params.isRayTracing())
		{
			SimpleBinding sb {};
			sb.set				= 0;
			sb.binding			= u32(m_simpleBindings.size());
			sb.type				= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			sb.count			= 1;
			sb.isResultBuffer	= true;
			sb.isRayTracingAS	= false;

			m_simpleBindings.emplace_back(sb);

			if (m_params.isRayTracing())
			{
				SimpleBinding sba{};
				sba.set				= 0;
				sba.binding			= u32(m_simpleBindings.size());
				sba.type			= VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
				sba.count			= 1;
				sba.isResultBuffer	= false;
				sba.isRayTracingAS	= true;

				m_simpleBindings.emplace_back(sba);
			}
		}
	}
	else if ((m_params.variant == TestVariant::MULTIPLE) ||
			 (m_params.variant == TestVariant::PUSH_DESCRIPTOR) ||
			 (m_params.variant == TestVariant::PUSH_TEMPLATE) ||
			 (m_params.variant == TestVariant::ROBUST_BUFFER_ACCESS) ||
			 (m_params.variant == TestVariant::ROBUST_NULL_DESCRIPTOR))
	{
		// Generate a descriptor set for each descriptor buffer binding.
		// Within a set, add bindings for each descriptor type. Bindings may have 1-3 array elements.
		// In this test we include sampler descriptors, they will be used with sampled images, if needed.

		// NOTE: For implementation simplicity, this test doesn't limit the number of descriptors accessed
		// in the shaders, which may not work on some implementations.

		// Don't overcomplicate the test logic
		DE_ASSERT(!m_params.isPushDescriptorTest() || (m_params.setsPerBuffer == 1));

		// Add one more set for push descriptors (if used)
		const auto	numSets				= (m_params.bufferBindingCount * m_params.setsPerBuffer) + (m_params.isPushDescriptorTest() ? 1 : 0);
		deUint32	attachmentIndex		= 0;

		// One set per buffer binding
		for (deUint32 set = 0; set < numSets; ++set)
		{
			std::vector<VkDescriptorType> choiceDescriptors;
			choiceDescriptors.emplace_back(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER);
			choiceDescriptors.emplace_back(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER);
			choiceDescriptors.emplace_back(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
			choiceDescriptors.emplace_back(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

			if (m_params.variant != TestVariant::ROBUST_BUFFER_ACCESS)
			{
				choiceDescriptors.emplace_back(VK_DESCRIPTOR_TYPE_SAMPLER);
				choiceDescriptors.emplace_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
				choiceDescriptors.emplace_back(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
				choiceDescriptors.emplace_back(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

				if (m_params.variant != TestVariant::ROBUST_NULL_DESCRIPTOR || (m_params.variant == TestVariant::ROBUST_NULL_DESCRIPTOR && m_params.isAccelerationStructure()))
					choiceDescriptors.emplace_back(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);	// will be replaced with VK_DESCRIPTOR_TYPE_STORAGE_BUFFER if unsupported

				if ((m_params.variant != TestVariant::ROBUST_NULL_DESCRIPTOR) &&
					(!m_params.isPushDescriptorTest() || (set != m_params.pushDescriptorSetIndex)))
				{
					choiceDescriptors.emplace_back(VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK);
				}

				if (m_params.stage == VK_SHADER_STAGE_FRAGMENT_BIT)
				{
					choiceDescriptors.emplace_back(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);
				}
			}

			// Randomize the order
			m_rng.shuffle(choiceDescriptors.begin(), choiceDescriptors.end());

			for (deUint32 binding = 0; binding < u32(choiceDescriptors.size()); ++binding)
			{
				SimpleBinding sb {};
				sb.set		= set;
				sb.binding	= binding;
				sb.type		= choiceDescriptors[binding];
				sb.count	= 1 + ((sb.type != VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK) ? m_rng.getUint32() % ConstMaxDescriptorArraySize : 0);

				// For inline uniforms we still use count = 1. The byte size is implicit in our tests.

				if (sb.type == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
				{
					sb.inputAttachmentIndex = attachmentIndex;
					attachmentIndex += sb.count;
				}

				m_simpleBindings.emplace_back(sb);
			}

			// For compute shaders add a result buffer as the last binding of the first set.
			if (set == 0 && (m_params.isCompute() || m_params.isRayTracing()))
			{
				SimpleBinding sb {};
				sb.set				= set;
				sb.binding			= u32(m_simpleBindings.size());
				sb.type				= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				sb.count			= 1;
				sb.isResultBuffer	= true;
				sb.isRayTracingAS	= false;

				m_simpleBindings.emplace_back(sb);

				if (m_params.isRayTracing())
				{
					SimpleBinding sba{};
					sba.set				= set;
					sba.binding			= u32(m_simpleBindings.size());
					sba.type			= VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
					sba.count			= 1;
					sba.isResultBuffer	= false;
					sba.isRayTracingAS	= true;

					m_simpleBindings.emplace_back(sba);
				}
			}
		}
	}
	else if (m_params.variant == TestVariant::MAX)
	{
		// Create sampler- and resource-only sets, up to specified maxiumums.
		// Each set will get its own descriptor buffer binding.

		deUint32 set		  = 0;
		deUint32 samplerIndex = 0;
		deUint32 imageIndex	  = 0;

		for (;;)
		{
			SimpleBinding sb {};
			sb.binding	= 0;
			sb.count	= 1;
			sb.set      = set;	// save the original set index here

			if (samplerIndex < m_params.samplerBufferBindingCount)
			{
				sb.set  = set;
				sb.type	= VK_DESCRIPTOR_TYPE_SAMPLER;

				m_simpleBindings.emplace_back(sb);

				++set;
				++samplerIndex;
			}

			if (imageIndex < m_params.resourceBufferBindingCount)
			{
				sb.set  = set;
				sb.type	= VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

				m_simpleBindings.emplace_back(sb);

				// Put the result buffer in the first resource set
				if ((imageIndex == 0) && (m_params.isCompute() || m_params.isRayTracing()))
				{
					sb.binding			= 1;
					sb.type				= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
					sb.isResultBuffer	= true;

					m_simpleBindings.emplace_back(sb);

					if (m_params.isRayTracing())
					{
						sb.binding			= 2;
						sb.type				= VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
						sb.isResultBuffer	= false;
						sb.isRayTracingAS	= true;

						m_simpleBindings.emplace_back(sb);
					}
				}

				++set;
				++imageIndex;
			}

			if (sb.set == set)
			{
				// We didn't add a new set, so we must be done.
				break;
			}
		}
	}
	else if (m_params.variant == TestVariant::EMBEDDED_IMMUTABLE_SAMPLERS)
	{
		// Create a number of sampler-only sets across several descriptor buffers, they will be used as embedded
		// immutable sampler buffers. Finally, add a set with images that use these samplers.

		// Buffer index maps to a set with embedded immutable samplers
		for (deUint32 bufferIndex = 0; bufferIndex < m_params.embeddedImmutableSamplerBufferBindingCount; ++bufferIndex)
		{
			for (deUint32 samplerIndex = 0; samplerIndex < m_params.embeddedImmutableSamplersPerBuffer; ++samplerIndex)
			{
				SimpleBinding sb {};
				sb.set							= bufferIndex;
				sb.binding						= samplerIndex;
				sb.count						= 1;
				sb.type							= VK_DESCRIPTOR_TYPE_SAMPLER;
				sb.isEmbeddedImmutableSampler	= true;

				m_simpleBindings.emplace_back(sb);
			}
		}

		// After the samplers come the images
		if (!m_simpleBindings.empty())
		{
			SimpleBinding sb {};
			sb.set      = m_simpleBindings.back().set + 1;
			sb.count	= 1;

			const auto numSamplers = m_params.embeddedImmutableSamplerBufferBindingCount * m_params.embeddedImmutableSamplersPerBuffer;

			for (deUint32 samplerIndex = 0; samplerIndex < numSamplers; ++samplerIndex)
			{
				sb.type		= VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
				sb.binding	= samplerIndex;

				m_simpleBindings.emplace_back(sb);
			}

			if (m_params.isCompute() || m_params.isRayTracing())
			{
				// Append the result buffer after the images
				sb.binding			+= 1;
				sb.type				= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				sb.isResultBuffer	= true;

				m_simpleBindings.emplace_back(sb);

				if (m_params.isRayTracing())
				{
					sb.binding			+= 1;
					sb.type				= VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
					sb.isResultBuffer	= false;
					sb.isRayTracingAS	= true;

					m_simpleBindings.emplace_back(sb);
				}
			}
		}
	}
}

// Generate shaders for both acceleration structures and without them
void DescriptorBufferTestCase::initPrograms (vk::SourceCollections& programs) const
{
	const bool accStruct	= m_params.isAccelerationStructureObligatory() || m_params.isAccelerationStructureOptional();

	initPrograms(programs, m_simpleBindings, accStruct, true);

	if (accStruct)
	{
		std::vector<SimpleBinding>	simpleBindings(m_simpleBindings);

		for (auto& simpleBinding : simpleBindings)
			if (simpleBinding.type == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
				simpleBinding.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

		initPrograms(programs, simpleBindings, false, false);
	}
}

// Initialize GLSL shaders used by all test cases.
void DescriptorBufferTestCase::initPrograms (vk::SourceCollections& programs, const std::vector<SimpleBinding>& simpleBindings, bool accStruct, bool addService) const
{
	// For vertex pipelines, a verification variable (in_result/out_result) is passed
	// through shader interfaces, until it can be output as a color write.
	//
	// Compute shaders still declare a "result" variable to help unify the verification logic.
	std::string extentionDeclarations	= std::string(glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_460)) + "\n"
										+ (m_params.isRayTracing() ? "#extension GL_EXT_ray_tracing : require\n" : "");

	if (m_params.isGraphics())
	{
		std::string srcDeclarations;
		std::string srcVerification;
		std::string suffix;

		if (m_params.stage == VK_SHADER_STAGE_VERTEX_BIT)
		{
			srcDeclarations = glslGlobalDeclarations(m_params, simpleBindings, accStruct) + "\n";
			srcVerification = glslOutputVerification(m_params, simpleBindings, accStruct) + "\n";
			suffix			= accStruct ? "_as" : "";
		}

		std::ostringstream str;
		str << extentionDeclarations
			<< srcDeclarations <<
			"\n"
			"layout(location = 0) out uvec4 out_result;\n"
			"\n"
			"void main (void) {\n"
			"	switch(gl_VertexIndex) {\n"
			"		case 0: gl_Position = vec4(-1, -1, 0, 1); break;\n"
			"		case 1: gl_Position = vec4(-1,  1, 0, 1); break;\n"
			"		case 2: gl_Position = vec4( 1, -1, 0, 1); break;\n"
			"\n"
			"		case 3: gl_Position = vec4( 1,  1, 0, 1); break;\n"
			"		case 4: gl_Position = vec4( 1, -1, 0, 1); break;\n"
			"		case 5: gl_Position = vec4(-1,  1, 0, 1); break;\n"
			"	}\n"
			"\n"
			"	uvec4 result = uvec4(0);\n"
			"\n"
			<< srcVerification <<
			"\n"
			"	out_result = result;\n"
			"}\n";

		if (addService || !srcDeclarations.empty()) programs.glslSources.add("vert" + suffix) << glu::VertexSource(str.str());
	}

	if (m_params.isGraphics())
	{
		std::string srcDeclarations;
		std::string srcVerification;
		std::string suffix;

		if (m_params.stage == VK_SHADER_STAGE_FRAGMENT_BIT)
		{
			srcDeclarations = glslGlobalDeclarations(m_params, simpleBindings, accStruct) + "\n";
			srcVerification = glslOutputVerification(m_params, simpleBindings, accStruct) + "\n";
			suffix			= accStruct ? "_as" : "";
		}

		std::ostringstream str;
		str << extentionDeclarations
			<< srcDeclarations <<
			"\n"
			"layout(location = 0) in flat uvec4 in_result;\n"
			"\n"
			"layout(location = 0) out uint out_color;\n"
			"\n"
			"void main (void) {\n"
			"	uvec4 result = in_result;\n"
			"\n"
			<< srcVerification <<
			"\n"
			"	if (uint(gl_FragCoord.x) == 0)	out_color = result.x;\n"
			"	if (uint(gl_FragCoord.x) == 1)	out_color = result.y;\n"
			"	if (uint(gl_FragCoord.x) == 2)	out_color = result.z;\n"
			"	if (uint(gl_FragCoord.x) == 3)	out_color = result.w;\n"
			"}\n";

		if (addService || !srcDeclarations.empty()) programs.glslSources.add("frag" + suffix) << glu::FragmentSource(str.str());
	}

	if (m_params.isGeometry())
	{
		std::string srcDeclarations = glslGlobalDeclarations(m_params, simpleBindings, accStruct) + "\n";
		std::string srcVerification = glslOutputVerification(m_params, simpleBindings, accStruct) + "\n";
		std::string suffix			= accStruct ? "_as" : "";

		std::ostringstream str;
		str << extentionDeclarations
			<< srcDeclarations <<
			"\n"
			"layout(triangles) in;\n"
			"layout(triangle_strip, max_vertices = 3) out;\n"
			"\n"
			"layout(location = 0) in  uvec4 in_result[];\n"
			"layout(location = 0) out uvec4 out_result;\n"
			"\n"
			"void main (void) {\n"
			"	for (uint i = 0; i < gl_in.length(); ++i) {\n"
			"		gl_Position = gl_in[i].gl_Position;\n"
			"\n"
			"		uvec4 result = in_result[i];\n"
			"\n"
			<< srcVerification <<
			"\n"
			"		out_result = result;\n"
			"\n"
			"		EmitVertex();\n"
			"	}\n"
			"}\n";

		if (addService || !srcDeclarations.empty()) programs.glslSources.add("geom" + suffix) << glu::GeometrySource(str.str());
	}

	if (m_params.isTessellation())
	{
		std::string srcDeclarations;
		std::string srcVerification;
		std::string suffix;

		if (m_params.stage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
		{
			srcDeclarations = glslGlobalDeclarations(m_params, simpleBindings, accStruct) + "\n";
			srcVerification = glslOutputVerification(m_params, simpleBindings, accStruct) + "\n";
			suffix			= accStruct ? "_as" : "";
		}

		std::ostringstream str;
		str << extentionDeclarations <<
			"#extension GL_EXT_tessellation_shader : require\n"
			<< srcDeclarations <<
			"\n"
			"layout(vertices = 3) out;\n"
			"\n"
			"layout(location = 0) in  uvec4 in_result[];\n"
			"layout(location = 0) out uvec4 out_result[];\n"
			"\n"
			"void main (void) {\n"
			"	gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
			"	\n"
			"	gl_TessLevelOuter[0] = 1.0;\n"
			"	gl_TessLevelOuter[1] = 1.0;\n"
			"	gl_TessLevelOuter[2] = 1.0;\n"
			"	gl_TessLevelInner[0] = 1.0;\n"
			"\n"
			"	uvec4 result = in_result[gl_InvocationID];\n"
			"\n"
			<< srcVerification <<
			"\n"
			"	out_result[gl_InvocationID] = result;\n"
			"}\n";

		if (addService || !srcDeclarations.empty()) programs.glslSources.add("tesc" + suffix) << glu::TessellationControlSource(str.str());
	}

	if (m_params.isTessellation())
	{
		std::string srcDeclarations;
		std::string srcVerification;
		std::string suffix;

		if (m_params.stage == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
		{
			srcDeclarations = glslGlobalDeclarations(m_params, simpleBindings, accStruct) + "\n";
			srcVerification = glslOutputVerification(m_params, simpleBindings, accStruct) + "\n";
			suffix			= accStruct ? "_as" : "";
		}

		std::ostringstream str;
		str << extentionDeclarations <<
			"#extension GL_EXT_tessellation_shader : require\n"
			<< srcDeclarations <<
			"\n"
			"layout(triangles) in;\n"
			"\n"
			"layout(location = 0) in  uvec4 in_result[];\n"
			"layout(location = 0) out uvec4 out_result;\n"
			"\n"
			"void main (void) {\n"
			"	gl_Position.xyz = gl_TessCoord.x * gl_in[0].gl_Position.xyz +\n"
			"	                  gl_TessCoord.y * gl_in[1].gl_Position.xyz +\n"
			"	                  gl_TessCoord.z * gl_in[2].gl_Position.xyz;\n"
			"	gl_Position.w   = 1.0;\n"
			"\n"
			"	uvec4 result = in_result[0];\n"	// Use index 0, all vertices should have the same value
			"\n"
			<< srcVerification <<
			"\n"
			"	out_result = result;\n"
			"}\n";

		if (addService || !srcDeclarations.empty()) programs.glslSources.add("tese" + suffix) << glu::TessellationEvaluationSource(str.str());
	}

	if (m_params.isCompute())
	{
		const std::string	suffix = accStruct ? "_as" : "";
		std::ostringstream	str;
		str << extentionDeclarations
			<< glslGlobalDeclarations(m_params, simpleBindings, accStruct) <<
			"\n"
			"layout(local_size_x = 1) in;\n"
			"\n"
			"void main (void) {\n"
			"	uvec4 result = uvec4(0);\n"
			"\n"
			<< glslOutputVerification(m_params, simpleBindings, accStruct) <<
			"}\n";

		programs.glslSources.add("comp" + suffix) << glu::ComputeSource(str.str());
	}

	if (m_params.isRayTracing())
	{
		const std::string				missPassthrough	= extentionDeclarations +
			"layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
			"\n"
			"void main()\n"
			"{\n"
			"}\n";
		const std::string				hitPassthrough	= extentionDeclarations +
			"hitAttributeEXT vec3 attribs;\n"
			"layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
			"\n"
			"void main()\n"
			"{\n"
			"}\n";
		const deUint32					asIndex			= getRayTracingASIndex(simpleBindings);
		const auto&						asBinding		= simpleBindings[asIndex];
		const std::string				asName			= glslResourceName(asBinding.set, asBinding.binding);
		const std::string				raygenCommon	= extentionDeclarations +
			"layout(location = 0) rayPayloadEXT vec3 hitValue;\n"
			"layout(set = " + de::toString(asBinding.set) + ", binding = " + de::toString(asBinding.binding) + ") uniform accelerationStructureEXT " + asName + ";\n"
			"\n"
			"void main()\n"
			"{\n"
			"	uint  rayFlags = 0;\n"
			"	uint  cullMask = 0xFF;\n"
			"	float tmin     = 0.0f;\n"
			"	float tmax     = 9.0f;\n"
			"	vec3  origin   = vec3(0.0f, 0.0f, 0.0f);\n"
			"	vec3  direct   = vec3(0.0f, 0.0f, -1.0f);\n"
			"	traceRayEXT(" + asName + ", rayFlags, cullMask, 0, 0, 0, origin, tmin, direct, tmax, 0);\n"
			"}\n";
		const vk::ShaderBuildOptions	buildOptions	= vk::ShaderBuildOptions(programs.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
		const std::string				suffix			= accStruct ? "_as" : "";
		const std::string				srcDeclarations	= glslGlobalDeclarations(m_params, simpleBindings, accStruct) + "\n";
		const std::string				srcVerification	= "	uvec4 result = uvec4(0);\n"
														+ glslOutputVerification(m_params, simpleBindings, accStruct) + "\n";

		switch (m_params.stage)
		{
			case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
			{
				std::stringstream css;
				css << extentionDeclarations << "\n"
					<< srcDeclarations <<
					"\n"
					"void main()\n"
					"{\n"
					<< srcVerification <<
					"}\n";

				programs.glslSources.add("rgen" + suffix) << glu::RaygenSource(css.str()) << buildOptions;

				break;
			}

			case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
			{
				if (addService) programs.glslSources.add("rgen") << glu::RaygenSource(raygenCommon) << buildOptions;

				{
					std::stringstream css;
					css << extentionDeclarations << "\n"
						<< srcDeclarations <<
						"hitAttributeEXT vec3 attribs;\n"
						"layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
						"\n"
						"void main()\n"
						"{\n"
						<< srcVerification <<
						"}\n";

					programs.glslSources.add("ahit" + suffix) << glu::AnyHitSource(css.str()) << buildOptions;
				}

				if (addService) programs.glslSources.add("chit") << glu::ClosestHitSource(hitPassthrough) << buildOptions;
				if (addService) programs.glslSources.add("miss") << glu::MissSource(missPassthrough) << buildOptions;

				break;
			}

			case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
			{
				if (addService) programs.glslSources.add("rgen") << glu::RaygenSource(raygenCommon) << buildOptions;

				{
					std::stringstream css;
					css << extentionDeclarations << "\n"
						<< srcDeclarations <<
						"layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
						"hitAttributeEXT vec3 attribs;\n"
						"\n"
						"\n"
						"void main()\n"
						"{\n"
						<< srcVerification <<
						"}\n";

					programs.glslSources.add("chit" + suffix) << glu::ClosestHitSource(css.str()) << buildOptions;
				}

				if (addService) programs.glslSources.add("ahit") << glu::AnyHitSource(hitPassthrough) << buildOptions;
				if (addService) programs.glslSources.add("miss") << glu::MissSource(missPassthrough) << buildOptions;

				break;
			}

			case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
			{
				if (addService) programs.glslSources.add("rgen") << glu::RaygenSource(raygenCommon) << buildOptions;

				{
					std::stringstream css;
					css << extentionDeclarations << "\n"
						<< srcDeclarations <<
						"hitAttributeEXT vec3 hitAttribute;\n"
						"\n"
						"void main()\n"
						"{\n"
						<< srcVerification <<
						"	hitAttribute = vec3(0.0f, 0.0f, 0.0f);\n"
						"	reportIntersectionEXT(1.0f, 0);\n"
						"}\n";

					programs.glslSources.add("sect" + suffix) << glu::IntersectionSource(css.str()) << buildOptions;
				}

				if (addService) programs.glslSources.add("ahit") << glu::AnyHitSource(hitPassthrough) << buildOptions;
				if (addService) programs.glslSources.add("chit") << glu::ClosestHitSource(hitPassthrough) << buildOptions;
				if (addService) programs.glslSources.add("miss") << glu::MissSource(missPassthrough) << buildOptions;

				break;
			}

			case VK_SHADER_STAGE_MISS_BIT_KHR:
			{
				if (addService) programs.glslSources.add("rgen") << glu::RaygenSource(raygenCommon) << buildOptions;

				{
					std::stringstream css;
					css << extentionDeclarations << "\n"
						<< srcDeclarations <<
						"\n"
						"layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
						"\n"
						"void main()\n"
						"{\n"
						<< srcVerification <<
						"}\n";

					programs.glslSources.add("miss" + suffix) << glu::MissSource(css.str()) << buildOptions;
				}

				if (addService) programs.glslSources.add("ahit") << glu::AnyHitSource(hitPassthrough) << buildOptions;
				if (addService) programs.glslSources.add("chit") << glu::ClosestHitSource(hitPassthrough) << buildOptions;

				break;
			}

			case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
			{
				{
					std::stringstream css;
					css << extentionDeclarations << "\n"
						<< (accStruct ? "#extension GL_EXT_ray_query : require\n" : "") <<
						"\n"
						"layout(location = 0) callableDataEXT float dummy;"
						"\n"
						"void main()\n"
						"{\n"
						"	executeCallableEXT(0, 0);\n"
						"}\n";

					if (addService) programs.glslSources.add("rgen") << glu::RaygenSource(css.str()) << buildOptions;
				}

				{
					std::stringstream css;
					css << extentionDeclarations << "\n"
						<< srcDeclarations <<
						"\n"
						"layout(location = 0) callableDataInEXT float dummy;"
						"\n"
						"void main()\n"
						"{\n"
						<< srcVerification <<
						"}\n";

					programs.glslSources.add("call" + suffix) << glu::CallableSource(css.str()) << buildOptions;
				}

				if (addService) programs.glslSources.add("ahit") << glu::AnyHitSource(hitPassthrough) << buildOptions;
				if (addService) programs.glslSources.add("chit") << glu::ClosestHitSource(hitPassthrough) << buildOptions;
				if (addService) programs.glslSources.add("miss") << glu::MissSource(missPassthrough) << buildOptions;

				break;
			}

			default:
				TCU_THROW(InternalError, "Unknown stage");
		}
	}
}

void DescriptorBufferTestCase::checkSupport (Context& context) const
{
	// Required to test the extension
	if (!context.isDeviceFunctionalitySupported("VK_EXT_descriptor_buffer"))
	{
		TCU_THROW(NotSupportedError, "VK_EXT_descriptor_buffer is not supported");
	}

	if (!context.isInstanceFunctionalitySupported("VK_KHR_get_physical_device_properties2"))
	{
		TCU_THROW(NotSupportedError, "VK_KHR_get_physical_device_properties2 is not supported");
	}

	if (!context.isDeviceFunctionalitySupported("VK_KHR_buffer_device_address"))
	{
		TCU_THROW(NotSupportedError, "VK_KHR_buffer_device_address is not supported");
	}

	if (!context.isDeviceFunctionalitySupported("VK_KHR_synchronization2"))
	{
		TCU_THROW(NotSupportedError, "VK_KHR_synchronization2 is not supported");
	}

	if (!context.isDeviceFunctionalitySupported("VK_EXT_descriptor_indexing"))
	{
		TCU_THROW(NotSupportedError, "VK_EXT_descriptor_indexing is not supported");
	}

	context.requireDeviceFunctionality("VK_KHR_buffer_device_address");
	context.requireDeviceFunctionality("VK_KHR_maintenance4");
	if (m_params.useMaintenance5)
		context.requireDeviceFunctionality("VK_KHR_maintenance5");

	// Optional

	if ((m_params.descriptor == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK) &&
		!context.isDeviceFunctionalitySupported("VK_EXT_inline_uniform_block"))
	{
		TCU_THROW(NotSupportedError, "VK_EXT_inline_uniform_block is not supported");
	}

	const auto& descriptorBufferFeatures = *findStructure<VkPhysicalDeviceDescriptorBufferFeaturesEXT>(&context.getDeviceFeatures2());
	const auto& descriptorBufferProps	 = *findStructure<VkPhysicalDeviceDescriptorBufferPropertiesEXT>(&context.getDeviceProperties2());

	if (!descriptorBufferFeatures.descriptorBuffer)
	{
		TCU_THROW(NotSupportedError, "descriptorBufferFeatures.descriptorBuffer is not supported");
	}

	if (m_params.variant == TestVariant::CAPTURE_REPLAY)
	{
		if (descriptorBufferFeatures.descriptorBufferCaptureReplay == VK_FALSE)
		{
			TCU_THROW(NotSupportedError, "descriptorBufferCaptureReplay feature is not supported");
		}

		if ((m_params.subcase == SubCase::CAPTURE_REPLAY_CUSTOM_BORDER_COLOR) &&
			!context.isDeviceFunctionalitySupported("VK_EXT_custom_border_color"))
		{
			TCU_THROW(NotSupportedError, "VK_EXT_custom_border_color is not supported");
		}
	}

	if (m_params.isTessellation() &&
		(context.getDeviceFeatures().tessellationShader == VK_FALSE))
	{
		TCU_THROW(NotSupportedError, "tessellationShader feature is not supported");
	}
	else if (m_params.isGeometry() &&
		(context.getDeviceFeatures().geometryShader == VK_FALSE))
	{
		TCU_THROW(NotSupportedError, "geometryShader feature is not supported");
	}

	if (m_params.bufferBindingCount * m_params.setsPerBuffer > context.getDeviceProperties().limits.maxBoundDescriptorSets)
		TCU_THROW(NotSupportedError, "Test requires more descriptor sets than specified in maxBoundDescriptorSets");

	// Test case specific
	if (m_params.isPushDescriptorTest())
	{
		context.requireDeviceFunctionality("VK_KHR_push_descriptor");

		if (descriptorBufferFeatures.descriptorBufferPushDescriptors == VK_FALSE)
		{
			TCU_THROW(NotSupportedError, "Require descriptorBufferFeatures.descriptorBufferPushDescriptors");
		}

		if (m_params.bufferBindingCount + 1 > context.getDeviceProperties().limits.maxBoundDescriptorSets)
			TCU_THROW(NotSupportedError, "Test requires more descriptor sets than specified in maxBoundDescriptorSets");

		if (m_params.subcase == SubCase::SINGLE_BUFFER)
		{
			if (descriptorBufferProps.bufferlessPushDescriptors)
				TCU_THROW(NotSupportedError, "Require bufferlessPushDescriptors to be false");
		}
		else
		{
			if (m_params.samplerBufferBindingCount + 1 > descriptorBufferProps.maxSamplerDescriptorBufferBindings)
			{
				TCU_THROW(NotSupportedError, "maxSamplerDescriptorBufferBindings is too small");
			}

			if (m_params.resourceBufferBindingCount + 1 > descriptorBufferProps.maxResourceDescriptorBufferBindings)
			{
				TCU_THROW(NotSupportedError, "maxResourceDescriptorBufferBindings is too small");
			}
		}
	}

	if (m_params.bufferBindingCount > descriptorBufferProps.maxDescriptorBufferBindings)
	{
		TCU_THROW(NotSupportedError, "maxDescriptorBufferBindings is too small");
	}

	if (m_params.samplerBufferBindingCount > descriptorBufferProps.maxSamplerDescriptorBufferBindings)
	{
		TCU_THROW(NotSupportedError, "maxSamplerDescriptorBufferBindings is too small");
	}

	if (m_params.resourceBufferBindingCount > descriptorBufferProps.maxResourceDescriptorBufferBindings)
	{
		TCU_THROW(NotSupportedError, "maxResourceDescriptorBufferBindings is too small");
	}

	if ((m_params.variant == TestVariant::ROBUST_BUFFER_ACCESS) ||
		(m_params.variant == TestVariant::ROBUST_NULL_DESCRIPTOR))
	{
		if (context.isDeviceFunctionalitySupported("VK_EXT_robustness2"))
		{
			VkPhysicalDeviceFeatures2				features2			= initVulkanStructure();
			VkPhysicalDeviceRobustness2FeaturesEXT	robustness2Features = initVulkanStructure();

			features2.pNext = &robustness2Features;

			context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &features2);

			if ((m_params.variant == TestVariant::ROBUST_NULL_DESCRIPTOR) &&
				(robustness2Features.nullDescriptor == VK_FALSE))
			{
				TCU_THROW(NotSupportedError, "robustness2 nullDescriptor is not supported");
			}

			DE_ASSERT(features2.features.robustBufferAccess);
		}
		else if (m_params.variant == TestVariant::ROBUST_NULL_DESCRIPTOR)
		{
			TCU_THROW(NotSupportedError, "VK_EXT_robustness2 is not supported");
		}
		else if (m_params.variant == TestVariant::ROBUST_BUFFER_ACCESS)
		{
			VkPhysicalDeviceFeatures features {};
			context.getInstanceInterface().getPhysicalDeviceFeatures(context.getPhysicalDevice(), &features);

			if (features.robustBufferAccess == VK_FALSE)
			{
				TCU_THROW(NotSupportedError, "robustBufferAccess is not supported");
			}
		}
	}

	if ((m_params.descriptor == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK) ||
		(m_params.variant == TestVariant::MULTIPLE) ||
		m_params.isPushDescriptorTest())
	{
		const auto& inlineUniformBlockFeatures = context.getInlineUniformBlockFeatures();

		if (!inlineUniformBlockFeatures.inlineUniformBlock)
		{
			TCU_THROW(NotSupportedError, "inlineUniformBlock is required");
		}
	}

	if (m_params.variant == TestVariant::MULTIPLE)
	{
		const VkPhysicalDeviceVulkan13Properties& vulkan13properties = *findStructure<VkPhysicalDeviceVulkan13Properties>(&context.getDeviceVulkan13Properties());

		if (m_params.bufferBindingCount > vulkan13properties.maxPerStageDescriptorInlineUniformBlocks)
			TCU_THROW(NotSupportedError, "Test require more per-stage inline uniform block bindings count. Provided " + de::toString(vulkan13properties.maxPerStageDescriptorInlineUniformBlocks));

		if (m_params.bufferBindingCount > vulkan13properties.maxDescriptorSetInlineUniformBlocks)
			TCU_THROW(NotSupportedError, "Test require more inline uniform block bindings among all stages. Provided " + de::toString(vulkan13properties.maxDescriptorSetInlineUniformBlocks));

		if (m_params.bufferBindingCount > vulkan13properties.maxPerStageDescriptorUpdateAfterBindInlineUniformBlocks)
			TCU_THROW(NotSupportedError, "Test require more per-stage inline uniform block bindings count. Provided " + de::toString(vulkan13properties.maxPerStageDescriptorUpdateAfterBindInlineUniformBlocks));

		if (m_params.bufferBindingCount > vulkan13properties.maxDescriptorSetUpdateAfterBindInlineUniformBlocks)
			TCU_THROW(NotSupportedError, "Test require more inline uniform block bindings among all stages. Provided " + de::toString(vulkan13properties.maxDescriptorSetUpdateAfterBindInlineUniformBlocks));
	}

	if (m_params.isAccelerationStructureObligatory())
	{
		context.requireDeviceFunctionality("VK_KHR_ray_query");
	}

	if (m_params.isRayTracing())
	{
		context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
		context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");
	}
}

// The base class for all test case implementations.
class DescriptorBufferTestInstance : public TestInstance
{
public:
	DescriptorBufferTestInstance(
		Context&							context,
		const TestParams&					params,
		const std::vector<SimpleBinding>&	simpleBindings);

	tcu::TestStatus					iterate						() override;

	void							createRayTracingPipeline	();
	de::MovePtr<BufferWithMemory>	createShaderBindingTable	(const InstanceInterface&			vki,
																 const DeviceInterface&				vkd,
																 const VkDevice						device,
																 const VkPhysicalDevice				physicalDevice,
																 const VkPipeline					pipeline,
																 Allocator&							allocator,
																 de::MovePtr<RayTracingPipeline>&	rayTracingPipeline,
																 const deUint32						group);
	void							addRayTracingShader			(const VkShaderStageFlagBits		stage,
																 const uint32_t						group);

	void							createGraphicsPipeline		();
	void							createDescriptorSetLayouts	();
	void							createDescriptorBuffers		();

	void							initializeBinding			(const DescriptorSetLayoutHolder&	dsl,
																 deUint32							setIndex,
																 Binding&							binding);

	void							pushDescriptorSet			(VkCommandBuffer					cmdBuf,
																 VkPipelineBindPoint				bindPoint,
																 const DescriptorSetLayoutHolder&	dsl,
																 deUint32							setIndex) const;

	void							bindDescriptorBuffers		(VkCommandBuffer					cmdBuf,
																 VkPipelineBindPoint				bindPoint) const;

	void							createBufferForBinding		(ResourceHolder&					resources,
																 VkDescriptorType					descriptorType,
																 VkBufferCreateInfo					createInfo,
																 bool								isResultBuffer) const;

	void							createImageForBinding		(ResourceHolder&					resources,
																 VkDescriptorType					descriptorType) const;

	MovePtr<Allocation>				allocate					(const VkMemoryRequirements&		memReqs,
																 const MemoryRequirement			requirement,
																 const void*						pNext = nullptr) const
	{
		return allocateExtended(
			m_context.getInstanceInterface(),
			*m_deviceInterface,
			m_context.getPhysicalDevice(),
			*m_device,
			memReqs,
			requirement,
			pNext);
	}

	// Descriptor size is used to determine the stride of a descriptor array (for bindings with multiple descriptors).
	VkDeviceSize					getDescriptorSize			(const Binding&						binding) const;

	deUint32						addDescriptorSetLayout		()
	{
		m_descriptorSetLayouts.emplace_back(makeSharedUniquePtr<DescriptorSetLayoutHolder>());
		return u32(m_descriptorSetLayouts.size()) - 1;
	}

	// The resources used by descriptors are tracked in a simple array and referenced by an index.
	deUint32 addResource()
	{
		m_resources.emplace_back(makeSharedUniquePtr<ResourceHolder>());
		return u32(m_resources.size()) - 1;
	}

	ResourceHolder& getOrCreateResource (Binding& binding, deUint32 arrayIndex)
	{
		if (binding.perBindingResourceIndex[arrayIndex] == INDEX_INVALID)
		{
			binding.perBindingResourceIndex[arrayIndex] = addResource();
		}

		ResourceHolder& result = (**m_resources[binding.perBindingResourceIndex[arrayIndex]]);

		return result;
	}

	const std::string				getShaderName				(const VkShaderStageFlagBits		stage) const
	{
		return toString(stage) + (m_params.isAccelerationStructure() && (m_params.stage == stage) ? "_as" : "");
	}

	const ProgramBinary&			getShaderBinary				(const VkShaderStageFlagBits		stage) const
	{
		return m_context.getBinaryCollection().get(getShaderName(stage));
	}

	bool							isCaptureDescriptor			(VkDescriptorType					type) const
	{
		return (m_testIteration == 0) && m_params.isCaptureReplayDescriptor(type);
	}

	bool							isReplayDescriptor			(VkDescriptorType					type) const
	{
		return (m_testIteration == 1) && m_params.isCaptureReplayDescriptor(type);
	}

	// Test cases using compute shaders always declare one binding with a result buffer.
	const BufferAlloc&				getResultBuffer				() const
	{
		DE_ASSERT(m_params.isCompute() || m_params.isRayTracing());

		const deUint32 resultBufferIndex = getResultBufferIndex(m_simpleBindings);
		DE_ASSERT(resultBufferIndex != INDEX_INVALID);
		const auto& sb = m_simpleBindings[resultBufferIndex];

		const auto binding = std::find_if(
			(**m_descriptorSetLayouts[sb.set]).bindings.begin(),
			(**m_descriptorSetLayouts[sb.set]).bindings.end(),
			[&sb](const Binding& it){ return it.binding == sb.binding; });

		DE_ASSERT(binding->descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

		// There's only one result buffer at this binding
		return (**m_resources[binding->perBindingResourceIndex[0]]).buffer;
	}

protected:
	TestParams										m_params;
	std::vector<SimpleBinding>						m_simpleBindings;

	Move<VkDevice>									m_device;
	MovePtr<DeviceDriver>							m_deviceInterface;
	VkQueue											m_queue;
	deUint32										m_queueFamilyIndex;
	MovePtr<Allocator>								m_allocatorPtr;

	VkPhysicalDeviceMemoryProperties				m_memoryProperties;
	VkPhysicalDeviceDescriptorBufferFeaturesEXT		m_descriptorBufferFeatures;
	VkPhysicalDeviceDescriptorBufferPropertiesEXT	m_descriptorBufferProperties;

	Move<VkPipeline>								m_pipeline;
	Move<VkPipelineLayout>							m_pipelineLayout;

	// Optional, for graphics pipelines
	Move<VkFramebuffer>								m_framebuffer;
	Move<VkRenderPass>								m_renderPass;
	VkRect2D										m_renderArea;
	ImageAlloc										m_colorImage;
	BufferAlloc										m_colorBuffer;			// for copying back to host visible memory

	std::vector<DSLPtr>								m_descriptorSetLayouts;
	std::vector<BufferAllocPtr>						m_descriptorBuffers;
	BufferAlloc										m_descriptorStagingBuffer;

	// Ray Tracing fields
	deUint32										m_shaders;
	deUint32										m_raygenShaderGroup;
	deUint32										m_missShaderGroup;
	deUint32										m_hitShaderGroup;
	deUint32										m_callableShaderGroup;
	deUint32										m_shaderGroupCount;

	de::MovePtr<RayTracingPipeline>					m_rayTracingPipeline;

	de::MovePtr<BufferWithMemory>					m_raygenShaderBindingTable;
	de::MovePtr<BufferWithMemory>					m_hitShaderBindingTable;
	de::MovePtr<BufferWithMemory>					m_missShaderBindingTable;
	de::MovePtr<BufferWithMemory>					m_callableShaderBindingTable;

	VkStridedDeviceAddressRegionKHR					m_raygenShaderBindingTableRegion;
	VkStridedDeviceAddressRegionKHR					m_missShaderBindingTableRegion;
	VkStridedDeviceAddressRegionKHR					m_hitShaderBindingTableRegion;
	VkStridedDeviceAddressRegionKHR					m_callableShaderBindingTableRegion;

	de::SharedPtr<BottomLevelAccelerationStructure>	m_bottomLevelAccelerationStructure;
	de::SharedPtr<TopLevelAccelerationStructure>	m_topLevelAccelerationStructure;

	// Common, but last
	std::vector<ResourcePtr>						m_resources;			// various resources used to test the descriptors
	deUint32										m_testIteration;		// for multi-pass tests such as capture/replay

};

DescriptorBufferTestInstance::DescriptorBufferTestInstance(
	Context&							context,
	const TestParams&					params,
	const std::vector<SimpleBinding>&	simpleBindings)
	: TestInstance							(context)
	, m_params								(params)
	, m_simpleBindings						(simpleBindings)
	, m_device								()
	, m_deviceInterface						()
	, m_queue								()
	, m_queueFamilyIndex					()
	, m_allocatorPtr						(DE_NULL)
	, m_memoryProperties					()
	, m_descriptorBufferFeatures			()
	, m_descriptorBufferProperties			()
	, m_pipeline							()
	, m_pipelineLayout						()
	, m_framebuffer							()
	, m_renderPass							()
	, m_renderArea							(makeRect2D(0, 0, 4, 1))
	, m_colorImage							()
	, m_colorBuffer							()
	, m_descriptorSetLayouts				()
	, m_descriptorBuffers					()
	, m_descriptorStagingBuffer				()
	, m_shaders								(0)
	, m_raygenShaderGroup					(~0u)
	, m_missShaderGroup						(~0u)
	, m_hitShaderGroup						(~0u)
	, m_callableShaderGroup					(~0u)
	, m_shaderGroupCount					(0)
	, m_rayTracingPipeline					(DE_NULL)
	, m_raygenShaderBindingTable			()
	, m_hitShaderBindingTable				()
	, m_missShaderBindingTable				()
	, m_callableShaderBindingTable			()
	, m_raygenShaderBindingTableRegion		()
	, m_missShaderBindingTableRegion		()
	, m_hitShaderBindingTableRegion			()
	, m_callableShaderBindingTableRegion	()
	, m_bottomLevelAccelerationStructure	()
	, m_topLevelAccelerationStructure		()
	, m_resources()
	, m_testIteration(0)
{
	// Need to create a new device because:
	// - We want to test graphics and compute queues,
	// - We must exclude VK_AMD_shader_fragment_mask from the enabled extensions.

	if (m_params.isAccelerationStructure() && m_params.isAccelerationStructureOptional())
	{
		if (!m_context.getRayQueryFeatures().rayQuery)
		{
			// Disable testing of acceleration structures if they ray query is not supported
			m_params.descriptor = VK_DESCRIPTOR_TYPE_MAX_ENUM;

			// Replace acceleration structures with storage buffers
			for (auto& simpleBinding : m_simpleBindings)
				if ((simpleBinding.type == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR) && !simpleBinding.isRayTracingAS)
					simpleBinding.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		}
		else
		{
			context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
		}
	}

	if ((m_params.variant == TestVariant::MULTIPLE) ||
		(m_params.variant == TestVariant::PUSH_DESCRIPTOR) ||
		(m_params.variant == TestVariant::PUSH_TEMPLATE) ||
		(m_params.variant == TestVariant::ROBUST_BUFFER_ACCESS) ||
		(m_params.variant == TestVariant::ROBUST_NULL_DESCRIPTOR))
	{
		const vk::VkPhysicalDeviceLimits&	limits									= context.getDeviceProperties().limits;
		deUint32							maxPerStageDescriptorSamplers			= 0; // VK_DESCRIPTOR_TYPE_SAMPLER or VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
		deUint32							maxPerStageDescriptorUniformBuffers		= 0; // VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
		deUint32							maxPerStageDescriptorStorageBuffers		= 0; // VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
		deUint32							maxPerStageDescriptorSampledImages		= 0; // VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, or VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER
		deUint32							maxPerStageDescriptorStorageImages		= 0; // VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, or VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER
		deUint32							maxPerStageDescriptorInputAttachments	= 0; // VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT

		for (const auto& simpleBinding : m_simpleBindings)
		{
			switch (simpleBinding.type)
			{
				case VK_DESCRIPTOR_TYPE_SAMPLER:
					maxPerStageDescriptorSamplers += simpleBinding.count;
					break;
				case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
					maxPerStageDescriptorSamplers += simpleBinding.count;
					maxPerStageDescriptorSampledImages += simpleBinding.count;
					break;
				case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
					maxPerStageDescriptorUniformBuffers += simpleBinding.count;
					break;
				case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
					maxPerStageDescriptorStorageBuffers += simpleBinding.count;
					break;
				case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
					maxPerStageDescriptorSampledImages += simpleBinding.count;
					break;
				case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
					maxPerStageDescriptorSampledImages += simpleBinding.count;
					break;
				case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
					maxPerStageDescriptorStorageImages += simpleBinding.count;
					break;
				case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
					maxPerStageDescriptorStorageImages += simpleBinding.count;
					break;
				case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
					maxPerStageDescriptorInputAttachments += simpleBinding.count;
					break;
				default:
					break;
			}
		}

#define VALIDATE_PER_STAGE_LIMIT(NAME) if (NAME > limits.NAME) TCU_THROW(NotSupportedError, std::string(#NAME) + " " + de::toString(NAME) + " is greater than limit " + de::toString(limits.NAME));
		VALIDATE_PER_STAGE_LIMIT(maxPerStageDescriptorSamplers);
		VALIDATE_PER_STAGE_LIMIT(maxPerStageDescriptorUniformBuffers);
		VALIDATE_PER_STAGE_LIMIT(maxPerStageDescriptorStorageBuffers);
		VALIDATE_PER_STAGE_LIMIT(maxPerStageDescriptorSampledImages);
		VALIDATE_PER_STAGE_LIMIT(maxPerStageDescriptorStorageImages);
		VALIDATE_PER_STAGE_LIMIT(maxPerStageDescriptorInputAttachments);
#undef VALIDATE_PER_STAGE_LIMIT
	}


	auto& inst			= context.getInstanceInterface();
	auto  physDevice	= context.getPhysicalDevice();
	auto queueProps		= getPhysicalDeviceQueueFamilyProperties(inst, physDevice);

	m_queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

	deUint32 graphicsComputeQueue	= VK_QUEUE_FAMILY_IGNORED;

	for (deUint32 i = 0; i < queueProps.size(); ++i)
	{
		if (m_params.queue == VK_QUEUE_GRAPHICS_BIT)
		{
			if ((queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
			{
				m_queueFamilyIndex = i;

				break;
			}
		}
		else if (m_params.queue == VK_QUEUE_COMPUTE_BIT)
		{
			if (((queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) &&
				((queueProps[i].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0))
			{
				m_queueFamilyIndex = i;
			}
			else if (((queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) &&
					 ((queueProps[i].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0))
			{
				graphicsComputeQueue = i;
			}
		}
	}

	// If a compute only queue could not be found, fall back to a
	// graphics & compute one.
	if (m_params.queue == VK_QUEUE_COMPUTE_BIT &&
		m_queueFamilyIndex == VK_QUEUE_FAMILY_IGNORED)
	{
		m_queueFamilyIndex = graphicsComputeQueue;
	}

	if (m_queueFamilyIndex == VK_QUEUE_FAMILY_IGNORED)
	{
		TCU_THROW(NotSupportedError, "Queue not supported");
	}

	const float priority[1] = { 0.5f };

	VkDeviceQueueCreateInfo queueInfo = initVulkanStructure();
	queueInfo.queueFamilyIndex = m_queueFamilyIndex;
	queueInfo.queueCount       = 1;
	queueInfo.pQueuePriorities = priority;

	VkPhysicalDeviceFeatures2							features2						= initVulkanStructure();
	VkPhysicalDeviceDescriptorBufferFeaturesEXT			descriptorBufferFeatures		= initVulkanStructure();
	VkPhysicalDeviceInlineUniformBlockFeaturesEXT		inlineUniformBlockFeatures		= initVulkanStructure();
	VkPhysicalDeviceSynchronization2FeaturesKHR			synchronization2Features		= initVulkanStructure();
	VkPhysicalDeviceRobustness2FeaturesEXT				robustness2Features				= initVulkanStructure();
	VkPhysicalDeviceCustomBorderColorFeaturesEXT		customBorderColorFeatures		= initVulkanStructure();
	VkPhysicalDeviceAccelerationStructureFeaturesKHR	accelerationStructureFeatures	= initVulkanStructure();
	VkPhysicalDeviceRayQueryFeaturesKHR					rayQueryFeatures				= initVulkanStructure();
	VkPhysicalDeviceRayTracingPipelineFeaturesKHR		rayTracingPipelineFeatures		= initVulkanStructure();
	VkPhysicalDeviceBufferDeviceAddressFeatures			bufferDeviceAddressFeatures		= initVulkanStructure();
	VkPhysicalDeviceMaintenance4Features				maintenance4Features			= initVulkanStructure();

	void** nextPtr = &features2.pNext;
	addToChainVulkanStructure(&nextPtr, synchronization2Features);
	addToChainVulkanStructure(&nextPtr, descriptorBufferFeatures);
	addToChainVulkanStructure(&nextPtr, bufferDeviceAddressFeatures);
	addToChainVulkanStructure(&nextPtr, maintenance4Features);

	// NOTE: VK_AMD_shader_fragment_mask must not be enabled
	std::vector<const char*> extensions;
	extensions.push_back("VK_EXT_descriptor_buffer");
	extensions.push_back("VK_KHR_buffer_device_address");
	extensions.push_back("VK_KHR_synchronization2");
	extensions.push_back("VK_EXT_descriptor_indexing");
	extensions.push_back("VK_KHR_maintenance4");

	if ((m_params.descriptor == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK) ||
		(m_params.variant == TestVariant::MULTIPLE) ||
		m_params.isPushDescriptorTest())
	{
		extensions.push_back("VK_EXT_inline_uniform_block");
		addToChainVulkanStructure(&nextPtr, inlineUniformBlockFeatures);

		if (m_params.isPushDescriptorTest())
			extensions.push_back("VK_KHR_push_descriptor");
	}
	else if (m_params.variant == TestVariant::ROBUST_NULL_DESCRIPTOR ||
			 m_params.variant == TestVariant::ROBUST_BUFFER_ACCESS)
	{
		if (context.isDeviceFunctionalitySupported("VK_EXT_robustness2"))
		{
			extensions.push_back("VK_EXT_robustness2");
			addToChainVulkanStructure(&nextPtr, robustness2Features);
		}
	}
	else if (m_params.subcase == SubCase::CAPTURE_REPLAY_CUSTOM_BORDER_COLOR)
	{
		extensions.push_back("VK_EXT_custom_border_color");
		addToChainVulkanStructure(&nextPtr, customBorderColorFeatures);
	}

	if (m_params.isAccelerationStructure() || m_params.isRayTracing())
	{
		extensions.push_back("VK_KHR_acceleration_structure");
		addToChainVulkanStructure(&nextPtr, accelerationStructureFeatures);
		extensions.push_back("VK_KHR_spirv_1_4");
		extensions.push_back("VK_KHR_deferred_host_operations");

		if (m_params.isAccelerationStructure())
		{
			extensions.push_back("VK_KHR_ray_query");
			addToChainVulkanStructure(&nextPtr, rayQueryFeatures);
			extensions.push_back("VK_KHR_deferred_host_operations");
		}

		if (m_params.isRayTracing())
		{
			extensions.push_back("VK_KHR_ray_tracing_pipeline");
			addToChainVulkanStructure(&nextPtr, rayTracingPipelineFeatures);
		}
	}

	context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &features2);

	if (m_params.variant != TestVariant::ROBUST_BUFFER_ACCESS)
	{
		features2.features.robustBufferAccess   = VK_FALSE;
		robustness2Features.robustBufferAccess2 = VK_FALSE;
		robustness2Features.robustImageAccess2  = VK_FALSE;
	}

	if (m_params.variant != TestVariant::ROBUST_NULL_DESCRIPTOR)
	{
		robustness2Features.nullDescriptor = VK_FALSE;
	}

	if (!m_params.isPushDescriptorTest())
	{
		descriptorBufferFeatures.descriptorBufferPushDescriptors = VK_FALSE;
	}

	if (!maintenance4Features.maintenance4)
		TCU_THROW(NotSupportedError, "Execution mode LocalSizeId is used, maintenance4 required");

	if (m_params.isAccelerationStructure() || m_params.isRayTracing())
	{
		if (!accelerationStructureFeatures.accelerationStructure)
			TCU_THROW(NotSupportedError, "Require accelerationStructureFeatures.accelerationStructure");

		if (m_params.isCaptureReplayDescriptor(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR))
		{
			if (!accelerationStructureFeatures.accelerationStructureCaptureReplay)
				TCU_THROW(NotSupportedError, "Require accelerationStructureFeatures.accelerationStructureCaptureReplay");
		}

		if (m_params.isAccelerationStructure())
		{
			if (!rayQueryFeatures.rayQuery)
				TCU_THROW(NotSupportedError, "Require rayQueryFeatures.rayQuery");
		}

		if (m_params.isRayTracing())
		{
			if (!rayTracingPipelineFeatures.rayTracingPipeline)
				TCU_THROW(NotSupportedError, "Require rayTracingPipelineFeatures.rayTracingPipeline");

			if (m_params.isCaptureReplayDescriptor(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR))
			{
				if (!rayTracingPipelineFeatures.rayTracingPipelineShaderGroupHandleCaptureReplay)
					TCU_THROW(NotSupportedError, "Require rayTracingPipelineFeatures.rayTracingPipelineShaderGroupHandleCaptureReplay");
			}
		}
	}

	// Should be enabled by default
	DE_ASSERT(descriptorBufferFeatures.descriptorBuffer);
	DE_ASSERT(synchronization2Features.synchronization2);

	if (m_params.variant == TestVariant::MULTIPLE || m_params.isPushDescriptorTest())
	{
		// TODO: Currently these tests assume the feature is available and there's no easy way to make it optional.
		// Rather than returning NotSupported, this should be reworked if many implementations have this limitation.
		DE_ASSERT(inlineUniformBlockFeatures.inlineUniformBlock);
	}
	else if (m_params.subcase == SubCase::CAPTURE_REPLAY_CUSTOM_BORDER_COLOR)
	{
		DE_ASSERT(customBorderColorFeatures.customBorderColors);
	}

	m_descriptorBufferFeatures		 = descriptorBufferFeatures;
	m_descriptorBufferFeatures.pNext = nullptr;

	m_descriptorBufferProperties	   = *findStructure<VkPhysicalDeviceDescriptorBufferPropertiesEXT>(&context.getDeviceProperties2());
	m_descriptorBufferProperties.pNext = nullptr;

	VkDeviceCreateInfo createInfo = initVulkanStructure(&features2);
	createInfo.pEnabledFeatures		   = DE_NULL;
	createInfo.enabledExtensionCount   = u32(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();
	createInfo.queueCreateInfoCount    = 1;
	createInfo.pQueueCreateInfos       = &queueInfo;

	m_device = createCustomDevice(
		false,
		context.getPlatformInterface(),
		context.getInstance(),
		inst,
		physDevice,
		&createInfo);

	context.getDeviceInterface().getDeviceQueue(
		*m_device,
		m_queueFamilyIndex,
		0,
		&m_queue);

	m_deviceInterface = newMovePtr<DeviceDriver>(context.getPlatformInterface(), context.getInstance(), *m_device, context.getUsedApiVersion());

	m_memoryProperties = vk::getPhysicalDeviceMemoryProperties(inst, physDevice);

	m_allocatorPtr = de::MovePtr<Allocator>(new SimpleAllocator(*m_deviceInterface, *m_device, m_memoryProperties));
}

VkDeviceSize DescriptorBufferTestInstance::getDescriptorSize(const Binding& binding) const
{
	const auto isRobustBufferAccess = (m_params.variant == TestVariant::ROBUST_BUFFER_ACCESS);

	std::size_t size = 0u;

	switch (binding.descriptorType) {
	case VK_DESCRIPTOR_TYPE_SAMPLER:
		size = m_descriptorBufferProperties.samplerDescriptorSize;
		break;

	case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
		size = m_descriptorBufferProperties.combinedImageSamplerDescriptorSize;
		break;

	case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
		size = m_descriptorBufferProperties.sampledImageDescriptorSize;
		break;

	case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
		size = m_descriptorBufferProperties.storageImageDescriptorSize;
		break;

	case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
		size = isRobustBufferAccess ? m_descriptorBufferProperties.robustUniformTexelBufferDescriptorSize
									: m_descriptorBufferProperties.uniformTexelBufferDescriptorSize;
		break;

	case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
		size = isRobustBufferAccess ? m_descriptorBufferProperties.robustStorageTexelBufferDescriptorSize
									: m_descriptorBufferProperties.storageTexelBufferDescriptorSize;
		break;

	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
		size = isRobustBufferAccess ? m_descriptorBufferProperties.robustUniformBufferDescriptorSize
									: m_descriptorBufferProperties.uniformBufferDescriptorSize;
		break;

	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
		size = isRobustBufferAccess ? m_descriptorBufferProperties.robustStorageBufferDescriptorSize
									: m_descriptorBufferProperties.storageBufferDescriptorSize;
		break;

	case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
		size = m_descriptorBufferProperties.inputAttachmentDescriptorSize;
		break;

	case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
		size = m_descriptorBufferProperties.accelerationStructureDescriptorSize;
		break;

	case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK:
		// Inline uniform block has no associated size. This is OK, because it can't be arrayed.
		break;

	default:
		DE_ASSERT(0);
		break;
	}

	return size;
}

void DescriptorBufferTestInstance::createDescriptorSetLayouts()
{
	for (auto& dslPtr : m_descriptorSetLayouts)
	{
		auto& dsl = **dslPtr;

		DE_ASSERT(!dsl.bindings.empty());

		const auto bindingsCopy = getDescriptorSetLayoutBindings(dsl.bindings);

		VkDescriptorSetLayoutCreateInfo createInfo = initVulkanStructure();
		createInfo.bindingCount = u32(bindingsCopy.size());
		createInfo.pBindings    = bindingsCopy.data();
		createInfo.flags		= VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

		if (dsl.hasEmbeddedImmutableSamplers)
		{
			createInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_EMBEDDED_IMMUTABLE_SAMPLERS_BIT_EXT;
		}
		else if (dsl.usePushDescriptors)
		{
			createInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
		}

		dsl.layout = createDescriptorSetLayout(*m_deviceInterface, *m_device, &createInfo);

		m_deviceInterface->getDescriptorSetLayoutSizeEXT(*m_device, *dsl.layout, &dsl.sizeOfLayout);

		for (auto& binding : dsl.bindings)
		{
			m_deviceInterface->getDescriptorSetLayoutBindingOffsetEXT(
				*m_device,
				*dsl.layout,
				binding.binding,
				&binding.offset);
		}
	}
}

// The test may create a variable number of descriptor buffers, based on the parameters.
//
void DescriptorBufferTestInstance::createDescriptorBuffers()
{
	DE_ASSERT(m_descriptorBuffers.empty());

	const deUint8	bufferInitialMemory					= 0xcc;			// descriptor buffer memory is initially set to this
	bool			allocateStagingBuffer				= false;		// determined after descriptors are created
	VkDeviceSize	stagingBufferDescriptorSetOffset	= 0;
	const deUint32	setsPerBuffer						= m_params.subcase == SubCase::SINGLE_BUFFER
														? m_params.bufferBindingCount + 1
														: m_params.setsPerBuffer;

	// Data tracked per buffer creation
	struct
	{
		deUint32			firstSet;
		deUint32			numSets;
		VkBufferUsageFlags	usage;
		VkDeviceSize		setOffset;
	} currentBuffer;

	currentBuffer = {};

	for (deUint32 setIndex = 0; setIndex < u32(m_descriptorSetLayouts.size()); ++setIndex)
	{
		auto& dsl = **m_descriptorSetLayouts[setIndex];

		if (dsl.hasEmbeddedImmutableSamplers || (dsl.usePushDescriptors && m_descriptorBufferProperties.bufferlessPushDescriptors && m_params.subcase != SubCase::SINGLE_BUFFER))
		{
			// Embedded immutable samplers aren't backed by a descriptor buffer.
			// Same goes for the set used with push descriptors.
			// Push descriptors might require buffer. If so, don't skip creation of buffer.

			// We musn't have started adding sets to the next buffer yet.
			DE_ASSERT(currentBuffer.numSets == 0);
			++currentBuffer.firstSet;

			continue;
		}

		// Required for binding
		currentBuffer.usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

		for (const auto& binding : dsl.bindings)
		{
			if (binding.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER)
			{
				currentBuffer.usage |= VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT;
			}
			else if (binding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
			{
				currentBuffer.usage |= VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT |
									   VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;
			}
			else
			{
				currentBuffer.usage |= VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;
			}
		}

		if (!m_descriptorBufferProperties.bufferlessPushDescriptors && dsl.usePushDescriptors)
		{
			currentBuffer.usage |= VK_BUFFER_USAGE_PUSH_DESCRIPTORS_DESCRIPTOR_BUFFER_BIT_EXT;
		}

		// Allow descriptor set layout to be size of zero bytes
		if (dsl.sizeOfLayout != 0)
		{
			// Assign this descriptor set to a new buffer
			dsl.bufferIndex  = u32(m_descriptorBuffers.size());
			dsl.bufferOffset = currentBuffer.setOffset;
		}

		currentBuffer.numSets   += 1;
		currentBuffer.setOffset += deAlignSize(
			static_cast<std::size_t>(dsl.sizeOfLayout),
			static_cast<std::size_t>(m_descriptorBufferProperties.descriptorBufferOffsetAlignment));

		VkMemoryAllocateFlagsInfo					allocFlagsInfo						= initVulkanStructure();
		allocFlagsInfo.flags |= VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

		// We've reached the limit of sets for this descriptor buffer.
		if (currentBuffer.numSets == setsPerBuffer)
		{
			vk::VkBufferCreateInfo bufferCreateInfo = makeBufferCreateInfo(currentBuffer.setOffset, currentBuffer.usage);

			if (bufferCreateInfo.size != 0)
			{
				m_descriptorBuffers.emplace_back(new BufferAlloc());
				auto& bufferAlloc = *m_descriptorBuffers.back();

				bufferAlloc.size = bufferCreateInfo.size;
				bufferAlloc.usage = bufferCreateInfo.usage;

				vk::VkBufferUsageFlags2CreateInfoKHR bufferUsageFlags2 = initVulkanStructure();;
				if (m_params.useMaintenance5)
				{
					bufferUsageFlags2.usage = (VkBufferUsageFlagBits2KHR)currentBuffer.usage;
					bufferCreateInfo.pNext = &bufferUsageFlags2;
					bufferCreateInfo.usage = 0;
				}

				bufferAlloc.buffer = vk::createBuffer(*m_deviceInterface, *m_device, &bufferCreateInfo);

				auto bufferMemReqs = getBufferMemoryRequirements(*m_deviceInterface, *m_device, *bufferAlloc.buffer);
				bool useStagedUpload = false;	// write directly to device-local memory, if possible

				if (DEBUG_FORCE_STAGED_UPLOAD)
				{
					useStagedUpload = true;
				}
				else if (DEBUG_MIX_DIRECT_AND_STAGED_UPLOAD)
				{
					// To avoid adding yet another test case permutation (which may be redundant on some implementations),
					// we are going to always test a mix of direct and staged uploads.
					useStagedUpload = ((dsl.bufferIndex % 2) == 1);
				}

				if (!useStagedUpload)
				{
					auto memReqs = MemoryRequirement::Local | MemoryRequirement::HostVisible;
					auto compatMask = bufferMemReqs.memoryTypeBits & getCompatibleMemoryTypes(m_memoryProperties, memReqs);

					if (compatMask != 0)
					{
						bufferAlloc.alloc = allocate(bufferMemReqs, memReqs, &allocFlagsInfo);
					}
					else
					{
						// No suitable memory type, fall back to a staged upload
						useStagedUpload = true;
					}
				}

				if (useStagedUpload)
				{
					DE_ASSERT(!bufferAlloc.alloc);

					if ((bufferAlloc.usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT) == 0)
					{
						bufferAlloc.buffer = Move<VkBuffer>();
						bufferAlloc.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

						bufferCreateInfo.usage = bufferAlloc.usage;

						bufferAlloc.buffer = vk::createBuffer(*m_deviceInterface, *m_device, &bufferCreateInfo);

						bufferMemReqs = getBufferMemoryRequirements(*m_deviceInterface, *m_device, *bufferAlloc.buffer);
					}

					bufferAlloc.alloc = allocate(bufferMemReqs, MemoryRequirement::Local, &allocFlagsInfo);
					allocateStagingBuffer = true;

					// Update staging buffer offsets for all sets in this buffer
					for (deUint32 i = currentBuffer.firstSet; i < currentBuffer.firstSet + currentBuffer.numSets; ++i)
					{
						(**m_descriptorSetLayouts[i]).stagingBufferOffset = stagingBufferDescriptorSetOffset;
						stagingBufferDescriptorSetOffset += (**m_descriptorSetLayouts[i]).sizeOfLayout;
					}
				}

				VK_CHECK(m_deviceInterface->bindBufferMemory(
					*m_device,
					*bufferAlloc.buffer,
					bufferAlloc.alloc->getMemory(),
					bufferAlloc.alloc->getOffset()));

				bufferAlloc.loadDeviceAddress(*m_deviceInterface, *m_device);

				if (!useStagedUpload)
				{
					// Clear the descriptor buffer memory to ensure there can be no random data there.
					deMemset(
						bufferAlloc.alloc->getHostPtr(),
						bufferInitialMemory,
						static_cast<std::size_t>(bufferAlloc.size));
				}
			}

			// Start with a new buffer
			currentBuffer = {};
			currentBuffer.firstSet = setIndex + 1;
		}
	}

	if (allocateStagingBuffer)
	{
		DE_ASSERT(!m_descriptorStagingBuffer.alloc);

		auto bufferCreateInfo = makeBufferCreateInfo(stagingBufferDescriptorSetOffset, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

		m_descriptorStagingBuffer.buffer = vk::createBuffer(*m_deviceInterface, *m_device, &bufferCreateInfo);
		m_descriptorStagingBuffer.size = bufferCreateInfo.size;

		auto bufferMemReqs = getBufferMemoryRequirements(*m_deviceInterface, *m_device, *m_descriptorStagingBuffer.buffer);

		m_descriptorStagingBuffer.alloc = allocate(bufferMemReqs, MemoryRequirement::HostVisible);

		VK_CHECK(m_deviceInterface->bindBufferMemory(
			*m_device,
			*m_descriptorStagingBuffer.buffer,
			m_descriptorStagingBuffer.alloc->getMemory(),
			m_descriptorStagingBuffer.alloc->getOffset()));

		// Clear the descriptor buffer memory to ensure there can be no random data there.
		deMemset(
			m_descriptorStagingBuffer.alloc->getHostPtr(),
			bufferInitialMemory,
			static_cast<std::size_t>(m_descriptorStagingBuffer.size));
	}
}

void DescriptorBufferTestInstance::bindDescriptorBuffers(VkCommandBuffer cmdBuf, VkPipelineBindPoint bindPoint) const
{
	std::vector<deUint32>									bufferIndices;
	std::vector<VkDeviceSize>								bufferOffsets;
	std::vector<VkDescriptorBufferBindingInfoEXT>			bufferBindingInfos;
	VkDescriptorBufferBindingPushDescriptorBufferHandleEXT	bufferBindingPushDescriptorBufferHandleEXT = initVulkanStructure();

	deUint32 firstSet = 0;

	if (m_params.variant == TestVariant::EMBEDDED_IMMUTABLE_SAMPLERS)
	{
		// These sampler sets are ordered first, so we can bind them now and increment the firstSet index.
		for (deUint32 setIndex = firstSet; setIndex < u32(m_descriptorSetLayouts.size()); ++setIndex)
		{
			const auto& dsl = **m_descriptorSetLayouts[setIndex];

			if (dsl.hasEmbeddedImmutableSamplers)
			{
				m_deviceInterface->cmdBindDescriptorBufferEmbeddedSamplersEXT(
					cmdBuf,
					bindPoint,
					*m_pipelineLayout,
					setIndex);

				// No gaps between sets.
				DE_ASSERT(firstSet == setIndex);

				firstSet = setIndex + 1;
			}
		}
	}

	for (const auto& buffer : m_descriptorBuffers)
	{
		VkDescriptorBufferBindingInfoEXT info = initVulkanStructure();

		info.address = buffer->deviceAddress;
		info.usage   = buffer->usage;

		if (!m_descriptorBufferProperties.bufferlessPushDescriptors && (buffer->usage & VK_BUFFER_USAGE_PUSH_DESCRIPTORS_DESCRIPTOR_BUFFER_BIT_EXT) != 0)
		{
			info.pNext = &bufferBindingPushDescriptorBufferHandleEXT;

			// Make sure there is only one such buffer
			DE_ASSERT(bufferBindingPushDescriptorBufferHandleEXT.buffer == DE_NULL);

			bufferBindingPushDescriptorBufferHandleEXT.buffer = *buffer->buffer;

			DE_ASSERT(bufferBindingPushDescriptorBufferHandleEXT.buffer != DE_NULL);
		}

		bufferBindingInfos.emplace_back(info);
	}

	if (bufferBindingInfos.size() != 0u)
	{
		m_deviceInterface->cmdBindDescriptorBuffersEXT(
			cmdBuf,
			u32(bufferBindingInfos.size()),
			bufferBindingInfos.data());
	}

	// Next, set the offsets for the bound buffers.

	for (deUint32 setIndex = firstSet; setIndex < u32(m_descriptorSetLayouts.size()); ++setIndex)
	{
		const auto& dsl		   = **m_descriptorSetLayouts[setIndex];
		const bool	isBoundSet = (dsl.bufferIndex != INDEX_INVALID);
		const bool  isLastSet  = ((setIndex + 1) == u32(m_descriptorSetLayouts.size()));

		if (isBoundSet)
		{
			bufferIndices.emplace_back(dsl.bufferIndex);
			bufferOffsets.emplace_back(dsl.bufferOffset);
		}

		if ((!isBoundSet || isLastSet) && !bufferIndices.empty())
		{
			m_deviceInterface->cmdSetDescriptorBufferOffsetsEXT(
				cmdBuf,
				bindPoint,
				*m_pipelineLayout,
				firstSet,
				u32(bufferIndices.size()),
				bufferIndices.data(),
				bufferOffsets.data());

			bufferIndices.clear();
			bufferOffsets.clear();

			firstSet = setIndex + 1;
		}
		else if (!isBoundSet)
		{
			// Push descriptor sets will have no buffer backing. Skip this set.
			++firstSet;
		}
	}
}

VkPipelineShaderStageCreateInfo makeShaderStageCreateInfo(VkShaderStageFlagBits stage, VkShaderModule shaderModule)
{
	VkPipelineShaderStageCreateInfo createInfo = initVulkanStructure();
	createInfo.stage				= stage;
	createInfo.module				= shaderModule;
	createInfo.pName				= "main";
	createInfo.pSpecializationInfo	= nullptr;
	return createInfo;
}

de::MovePtr<BufferWithMemory> DescriptorBufferTestInstance::createShaderBindingTable (const InstanceInterface&			vki,
																					  const DeviceInterface&			vkd,
																					  const VkDevice					device,
																					  const VkPhysicalDevice			physicalDevice,
																					  const VkPipeline					pipeline,
																					  Allocator&						allocator,
																					  de::MovePtr<RayTracingPipeline>&	rayTracingPipeline,
																					  const deUint32					group)
{
	de::MovePtr<BufferWithMemory>	shaderBindingTable;

	if (group < m_shaderGroupCount)
	{
		const deUint32	shaderGroupHandleSize		= getShaderGroupHandleSize(vki, physicalDevice);
		const deUint32	shaderGroupBaseAlignment	= getShaderGroupBaseAlignment(vki, physicalDevice);

		shaderBindingTable = rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, group, 1u);
	}

	return shaderBindingTable;
}

void DescriptorBufferTestInstance::createRayTracingPipeline()
{
	const InstanceInterface&	vki						= m_context.getInstanceInterface();
	const DeviceInterface&		vkd						= *m_deviceInterface;
	const VkDevice				device					= *m_device;
	const VkPhysicalDevice		physicalDevice			= m_context.getPhysicalDevice();
	vk::BinaryCollection&		collection				= m_context.getBinaryCollection();
	Allocator&					allocator				= *m_allocatorPtr;
	const deUint32				shaderGroupHandleSize	= getShaderGroupHandleSize(vki, physicalDevice);
	const VkShaderStageFlags	hitStages				= VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR;

	m_shaderGroupCount = 0;

	if (collection.contains(getShaderName(VK_SHADER_STAGE_RAYGEN_BIT_KHR)))			m_shaders |= VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	if (collection.contains(getShaderName(VK_SHADER_STAGE_ANY_HIT_BIT_KHR)))		m_shaders |= VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
	if (collection.contains(getShaderName(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)))	m_shaders |= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	if (collection.contains(getShaderName(VK_SHADER_STAGE_MISS_BIT_KHR)))			m_shaders |= VK_SHADER_STAGE_MISS_BIT_KHR;
	if (collection.contains(getShaderName(VK_SHADER_STAGE_INTERSECTION_BIT_KHR)))	m_shaders |= VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
	if (collection.contains(getShaderName(VK_SHADER_STAGE_CALLABLE_BIT_KHR)))		m_shaders |= VK_SHADER_STAGE_CALLABLE_BIT_KHR;

	if (0 != (m_shaders & VK_SHADER_STAGE_RAYGEN_BIT_KHR))
		m_raygenShaderGroup		= m_shaderGroupCount++;

	if (0 != (m_shaders & VK_SHADER_STAGE_MISS_BIT_KHR))
		m_missShaderGroup		= m_shaderGroupCount++;

	if (0 != (m_shaders & hitStages))
		m_hitShaderGroup		= m_shaderGroupCount++;

	if (0 != (m_shaders & VK_SHADER_STAGE_CALLABLE_BIT_KHR))
		m_callableShaderGroup	= m_shaderGroupCount++;

	m_rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();

	m_rayTracingPipeline->setCreateFlags(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

	if (0 != (m_shaders & VK_SHADER_STAGE_RAYGEN_BIT_KHR))			addRayTracingShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,			m_raygenShaderGroup);
	if (0 != (m_shaders & VK_SHADER_STAGE_ANY_HIT_BIT_KHR))			addRayTracingShader(VK_SHADER_STAGE_ANY_HIT_BIT_KHR,		m_hitShaderGroup);
	if (0 != (m_shaders & VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR))		addRayTracingShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,	m_hitShaderGroup);
	if (0 != (m_shaders & VK_SHADER_STAGE_MISS_BIT_KHR))			addRayTracingShader(VK_SHADER_STAGE_MISS_BIT_KHR,			m_missShaderGroup);
	if (0 != (m_shaders & VK_SHADER_STAGE_INTERSECTION_BIT_KHR))	addRayTracingShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR,	m_hitShaderGroup);
	if (0 != (m_shaders & VK_SHADER_STAGE_CALLABLE_BIT_KHR))		addRayTracingShader(VK_SHADER_STAGE_CALLABLE_BIT_KHR,		m_callableShaderGroup);

	m_pipelineLayout					= makePipelineLayout(vkd, device, getDescriptorSetLayouts(m_descriptorSetLayouts));
	m_pipeline							= m_rayTracingPipeline->createPipeline(vkd, device, *m_pipelineLayout);

	m_raygenShaderBindingTable			= createShaderBindingTable(vki, vkd, device, physicalDevice, *m_pipeline, allocator, m_rayTracingPipeline, m_raygenShaderGroup);
	m_missShaderBindingTable			= createShaderBindingTable(vki, vkd, device, physicalDevice, *m_pipeline, allocator, m_rayTracingPipeline, m_missShaderGroup);
	m_hitShaderBindingTable				= createShaderBindingTable(vki, vkd, device, physicalDevice, *m_pipeline, allocator, m_rayTracingPipeline, m_hitShaderGroup);
	m_callableShaderBindingTable		= createShaderBindingTable(vki, vkd, device, physicalDevice, *m_pipeline, allocator, m_rayTracingPipeline, m_callableShaderGroup);

	m_raygenShaderBindingTableRegion	= makeStridedDeviceAddressRegion(vkd, device, getVkBuffer(m_raygenShaderBindingTable),		shaderGroupHandleSize);
	m_missShaderBindingTableRegion		= makeStridedDeviceAddressRegion(vkd, device, getVkBuffer(m_missShaderBindingTable),		shaderGroupHandleSize);
	m_hitShaderBindingTableRegion		= makeStridedDeviceAddressRegion(vkd, device, getVkBuffer(m_hitShaderBindingTable),			shaderGroupHandleSize);
	m_callableShaderBindingTableRegion	= makeStridedDeviceAddressRegion(vkd, device, getVkBuffer(m_callableShaderBindingTable),	shaderGroupHandleSize);
}

void DescriptorBufferTestInstance::addRayTracingShader (const VkShaderStageFlagBits		stage,
														const uint32_t					group)
{
	DE_ASSERT(m_rayTracingPipeline != DE_NULL);

	m_rayTracingPipeline->addShader(stage, createShaderModule(*m_deviceInterface, *m_device, getShaderBinary(stage), 0), group);
}

// The graphics pipeline is very simple for this test.
// The number of shader stages is configurable. There's no vertex input, a single triangle covers the entire viewport.
// The color target uses R32_UINT format and is used to save the verifcation result.
//
void DescriptorBufferTestInstance::createGraphicsPipeline()
{
	std::vector<VkImageView> framebufferAttachments;

	{
		m_colorImage.info = initVulkanStructure();
		m_colorImage.info.flags					= 0;
		m_colorImage.info.imageType				= VK_IMAGE_TYPE_2D;
		m_colorImage.info.format				= VK_FORMAT_R32_UINT;
		m_colorImage.info.extent.width			= m_renderArea.extent.width;
		m_colorImage.info.extent.height			= m_renderArea.extent.height;
		m_colorImage.info.extent.depth			= 1;
		m_colorImage.info.mipLevels				= 1;
		m_colorImage.info.arrayLayers			= 1;
		m_colorImage.info.samples				= VK_SAMPLE_COUNT_1_BIT;
		m_colorImage.info.tiling				= VK_IMAGE_TILING_OPTIMAL;
		m_colorImage.info.usage					= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		m_colorImage.info.sharingMode			= VK_SHARING_MODE_EXCLUSIVE;
		m_colorImage.info.queueFamilyIndexCount	= 0;
		m_colorImage.info.pQueueFamilyIndices	= nullptr;
		m_colorImage.info.initialLayout			= VK_IMAGE_LAYOUT_UNDEFINED;

		m_colorImage.image = createImage(*m_deviceInterface, *m_device, &m_colorImage.info);

		auto memReqs = getImageMemoryRequirements(*m_deviceInterface, *m_device, *m_colorImage.image);
		m_colorImage.sizeBytes = memReqs.size;
		m_colorImage.alloc	   = allocate(memReqs, MemoryRequirement::Local);

		VK_CHECK(m_deviceInterface->bindImageMemory(
			*m_device,
			*m_colorImage.image,
			m_colorImage.alloc->getMemory(),
			m_colorImage.alloc->getOffset()));
	}
	{
		auto createInfo = makeBufferCreateInfo(
			m_colorImage.sizeBytes,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT);

		m_colorBuffer.buffer = createBuffer(*m_deviceInterface, *m_device, &createInfo);

		auto memReqs = getBufferMemoryRequirements(*m_deviceInterface, *m_device, *m_colorBuffer.buffer);

		m_colorBuffer.alloc = allocate(memReqs, MemoryRequirement::HostVisible);
		VK_CHECK(m_deviceInterface->bindBufferMemory(
			*m_device,
			*m_colorBuffer.buffer,
			m_colorBuffer.alloc->getMemory(),
			m_colorBuffer.alloc->getOffset()));
	}
	{
		VkImageViewCreateInfo createInfo = initVulkanStructure();
		createInfo.image			= *m_colorImage.image;
		createInfo.viewType			= VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format			= m_colorImage.info.format;
		createInfo.components		= ComponentMappingIdentity;
		createInfo.subresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);

		m_colorImage.imageView = createImageView(*m_deviceInterface, *m_device, &createInfo);
	}

	framebufferAttachments.push_back(*m_colorImage.imageView);

	{
		std::vector<VkAttachmentDescription> attachments;
		std::vector<VkAttachmentReference>	 colorRefs;
		std::vector<VkAttachmentReference>	 inputRefs;

		{
			VkAttachmentDescription colorAttachment {};
			colorAttachment.format			= VK_FORMAT_R32_UINT;
			colorAttachment.samples			= VK_SAMPLE_COUNT_1_BIT;
			colorAttachment.loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR;
			colorAttachment.storeOp			= VK_ATTACHMENT_STORE_OP_STORE;
			colorAttachment.stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			colorAttachment.stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;
			colorAttachment.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;
			colorAttachment.finalLayout		= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

			colorRefs.emplace_back(makeAttachmentReference(u32(attachments.size()), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
			attachments.emplace_back(colorAttachment);
		}

		for (deUint32 setIndex = 0; setIndex < u32(m_descriptorSetLayouts.size()); ++setIndex)
		{
			const auto& dsl = **m_descriptorSetLayouts[setIndex];

			for (deUint32 bindingIndex = 0; bindingIndex < u32(dsl.bindings.size()); ++bindingIndex)
			{
				const auto& binding = dsl.bindings[bindingIndex];

				if (binding.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
				{
					for (deUint32 arrayIndex = 0; arrayIndex < binding.descriptorCount; ++arrayIndex)
					{
						VkAttachmentDescription inputAttachment {};
						inputAttachment.format			= VK_FORMAT_R32_UINT;
						inputAttachment.samples			= VK_SAMPLE_COUNT_1_BIT;
						inputAttachment.loadOp			= VK_ATTACHMENT_LOAD_OP_LOAD;
						inputAttachment.storeOp			= VK_ATTACHMENT_STORE_OP_DONT_CARE;
						inputAttachment.stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
						inputAttachment.stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;
						inputAttachment.initialLayout	= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
						inputAttachment.finalLayout		= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

						inputRefs.emplace_back(makeAttachmentReference(u32(attachments.size()), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
						attachments.emplace_back(inputAttachment);

						const auto inputAttachmentResourceIndex = binding.perBindingResourceIndex[arrayIndex];
						framebufferAttachments.push_back(*(**m_resources[inputAttachmentResourceIndex]).image.imageView);
					}
				}
			}
		}

		VkSubpassDescription subpass {};
		subpass.pipelineBindPoint		= VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.inputAttachmentCount	= u32(inputRefs.size());
		subpass.pInputAttachments		= inputRefs.data();
		subpass.colorAttachmentCount	= u32(colorRefs.size());
		subpass.pColorAttachments		= colorRefs.data();
		subpass.pResolveAttachments		= nullptr;
		subpass.pDepthStencilAttachment	= nullptr;
		subpass.preserveAttachmentCount	= 0;
		subpass.pPreserveAttachments	= nullptr;

		VkRenderPassCreateInfo createInfo = initVulkanStructure();
		// No explicit dependencies
		createInfo.attachmentCount	= u32(attachments.size());
		createInfo.pAttachments		= attachments.data();
		createInfo.subpassCount		= 1;
		createInfo.pSubpasses		= &subpass;

		m_renderPass = createRenderPass(*m_deviceInterface, *m_device, &createInfo);
	}
	{
		VkFramebufferCreateInfo createInfo = initVulkanStructure();
		createInfo.renderPass		= *m_renderPass;
		createInfo.attachmentCount	= u32(framebufferAttachments.size());
		createInfo.pAttachments		= framebufferAttachments.data();
		createInfo.width			= m_renderArea.extent.width;
		createInfo.height			= m_renderArea.extent.height;
		createInfo.layers			= 1;

		m_framebuffer = createFramebuffer(*m_deviceInterface, *m_device, &createInfo);
	}

	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

	Move<VkShaderModule> vertModule;
	Move<VkShaderModule> tessControlModule;
	Move<VkShaderModule> tessEvalModule;
	Move<VkShaderModule> geomModule;
	Move<VkShaderModule> fragModule;

	vertModule = createShaderModule(*m_deviceInterface, *m_device, getShaderBinary(VK_SHADER_STAGE_VERTEX_BIT), 0u);
	fragModule = createShaderModule(*m_deviceInterface, *m_device, getShaderBinary(VK_SHADER_STAGE_FRAGMENT_BIT), 0u);

	shaderStages.emplace_back(makeShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, *vertModule));
	shaderStages.emplace_back(makeShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, *fragModule));

	if (m_params.isTessellation())
	{
		tessControlModule = createShaderModule(*m_deviceInterface, *m_device, getShaderBinary(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT), 0u);
		tessEvalModule	  = createShaderModule(*m_deviceInterface, *m_device, getShaderBinary(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT), 0u);

		shaderStages.emplace_back(makeShaderStageCreateInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, *tessControlModule));
		shaderStages.emplace_back(makeShaderStageCreateInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, *tessEvalModule));
	}
	else if (m_params.isGeometry())
	{
		geomModule = createShaderModule(*m_deviceInterface, *m_device, getShaderBinary(VK_SHADER_STAGE_GEOMETRY_BIT), 0u);

		shaderStages.emplace_back(makeShaderStageCreateInfo(VK_SHADER_STAGE_GEOMETRY_BIT, *geomModule));
	}

	VkPipelineVertexInputStateCreateInfo vertexInputState = initVulkanStructure();
	// No vertex input

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = initVulkanStructure();
	inputAssemblyState.topology = !!tessControlModule ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	VkPipelineTessellationStateCreateInfo tesselationState = initVulkanStructure();
	tesselationState.patchControlPoints = 3;

	VkViewport viewport = makeViewport(m_renderArea.extent);

	VkPipelineViewportStateCreateInfo viewportState = initVulkanStructure();
	viewportState.viewportCount = 1;
	viewportState.pViewports	= &viewport;
	viewportState.scissorCount	= 1;
	viewportState.pScissors		= &m_renderArea;

	VkPipelineRasterizationStateCreateInfo rasterizationState = initVulkanStructure();
    rasterizationState.depthClampEnable			= VK_FALSE;
    rasterizationState.rasterizerDiscardEnable  = VK_FALSE;
    rasterizationState.polygonMode				= VK_POLYGON_MODE_FILL;
    rasterizationState.cullMode					= VK_CULL_MODE_NONE;
    rasterizationState.frontFace				= VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizationState.depthBiasEnable			= VK_FALSE;
    rasterizationState.depthBiasConstantFactor	= 0.0f;
    rasterizationState.depthBiasClamp			= 0.0f;
    rasterizationState.depthBiasSlopeFactor		= 0.0f;
    rasterizationState.lineWidth				= 1.0f;

	VkPipelineMultisampleStateCreateInfo multisampleState = initVulkanStructure();
	// Everything else disabled/default
	multisampleState.rasterizationSamples	= VK_SAMPLE_COUNT_1_BIT;

	VkPipelineDepthStencilStateCreateInfo depthStencilState = initVulkanStructure();
	// Everything else disabled/default
    depthStencilState.minDepthBounds = 0.0f;
    depthStencilState.maxDepthBounds = 1.0f;

	VkPipelineColorBlendAttachmentState colorAttachment {};
	// Everything else disabled/default
	colorAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo colorBlendState = initVulkanStructure();
	// Everything else disabled/default
    colorBlendState.attachmentCount	= 1;
    colorBlendState.pAttachments	= &colorAttachment;

	{
		VkGraphicsPipelineCreateInfo createInfo = initVulkanStructure();
		createInfo.stageCount			= u32(shaderStages.size());
		createInfo.pStages				= shaderStages.data();
		createInfo.pVertexInputState	= &vertexInputState;
		createInfo.pInputAssemblyState	= &inputAssemblyState;
		createInfo.pTessellationState	= m_params.isTessellation() ? &tesselationState : nullptr;
		createInfo.pViewportState		= &viewportState;
		createInfo.pRasterizationState	= &rasterizationState;
		createInfo.pMultisampleState	= &multisampleState;
		createInfo.pDepthStencilState	= &depthStencilState;
		createInfo.pColorBlendState		= &colorBlendState;
		createInfo.pDynamicState		= nullptr;
		createInfo.layout				= *m_pipelineLayout;
		createInfo.renderPass			= *m_renderPass;
		createInfo.subpass				= 0;
		createInfo.basePipelineHandle	= DE_NULL;
		createInfo.basePipelineIndex	= -1;
		createInfo.flags				= VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

		m_pipeline = vk::createGraphicsPipeline(
			*m_deviceInterface,
			*m_device,
			DE_NULL, // pipeline cache
			&createInfo);
	}
}

void DescriptorBufferTestInstance::createBufferForBinding(
	ResourceHolder&		resources,
	VkDescriptorType	descriptorType,
	VkBufferCreateInfo	createInfo,
	bool				isResultBuffer) const
{
	auto& bufferResource	= resources.buffer;
	auto& captureReplayData	= resources.captureReplay.bufferData;

	createInfo.usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

	if (!isResultBuffer && isCaptureDescriptor(descriptorType))
	{
		createInfo.flags |= VK_BUFFER_CREATE_DESCRIPTOR_BUFFER_CAPTURE_REPLAY_BIT_EXT;

		DE_ASSERT(!bufferResource.buffer);
		bufferResource.buffer = createBuffer(*m_deviceInterface, *m_device, &createInfo);

		VkBufferCaptureDescriptorDataInfoEXT info = initVulkanStructure();
		info.buffer = *bufferResource.buffer;

		DE_ASSERT(captureReplayData.empty());
		captureReplayData.resize(m_descriptorBufferProperties.bufferCaptureReplayDescriptorDataSize);

		VK_CHECK(m_deviceInterface->getBufferOpaqueCaptureDescriptorDataEXT(*m_device, &info, captureReplayData.data()));
	}
	else if (!isResultBuffer && isReplayDescriptor(descriptorType))
	{
		// Free the previous buffer and its memory
		reset(bufferResource.buffer);
		reset(bufferResource.alloc);

		DE_ASSERT(!captureReplayData.empty());

		VkOpaqueCaptureDescriptorDataCreateInfoEXT info = initVulkanStructure();
		info.opaqueCaptureDescriptorData = captureReplayData.data();

		createInfo.flags |= VK_BUFFER_CREATE_DESCRIPTOR_BUFFER_CAPTURE_REPLAY_BIT_EXT;
		createInfo.pNext = &info;

		bufferResource.buffer = createBuffer(*m_deviceInterface, *m_device, &createInfo);
	}
	else
	{
		DE_ASSERT(!bufferResource.buffer);
		bufferResource.buffer = createBuffer(*m_deviceInterface, *m_device, &createInfo);
	}

	auto memReqs = getBufferMemoryRequirements(*m_deviceInterface, *m_device, *bufferResource.buffer);

	VkMemoryOpaqueCaptureAddressAllocateInfo	opaqueCaptureAddressAllocateInfo	= initVulkanStructure();
	VkMemoryAllocateFlagsInfo					allocFlagsInfo						= initVulkanStructure();
	allocFlagsInfo.flags |= VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

	if (!isResultBuffer && m_params.isCaptureReplayDescriptor(descriptorType))
	{
		allocFlagsInfo.flags |= VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT;
		allocFlagsInfo.pNext = &opaqueCaptureAddressAllocateInfo;

		if (isCaptureDescriptor(descriptorType))
		{
			opaqueCaptureAddressAllocateInfo.opaqueCaptureAddress = 0ull;
		}
		else if (isReplayDescriptor(descriptorType))
		{
			opaqueCaptureAddressAllocateInfo.opaqueCaptureAddress = bufferResource.opaqueCaptureAddress;
		}
	}

	DE_ASSERT(!bufferResource.alloc);
	bufferResource.alloc = allocate(memReqs, MemoryRequirement::HostVisible, &allocFlagsInfo);

	if (isCaptureDescriptor(descriptorType))
	{
		VkDeviceMemoryOpaqueCaptureAddressInfo memoryOpaqueCaptureAddressInfo = initVulkanStructure();

		memoryOpaqueCaptureAddressInfo.memory = bufferResource.alloc->getMemory();

		bufferResource.opaqueCaptureAddress = m_deviceInterface->getDeviceMemoryOpaqueCaptureAddress(*m_device, &memoryOpaqueCaptureAddressInfo);
	}

	VK_CHECK(m_deviceInterface->bindBufferMemory(
		*m_device,
		*bufferResource.buffer,
		bufferResource.alloc->getMemory(),
		bufferResource.alloc->getOffset()));

	bufferResource.loadDeviceAddress(*m_deviceInterface, *m_device);
}

void DescriptorBufferTestInstance::createImageForBinding(
	ResourceHolder&		resources,
	VkDescriptorType	descriptorType) const
{
	auto& imageResource	= resources.image;

	// Image
	auto& captureReplayData = resources.captureReplay.imageData;

	if (isCaptureDescriptor(descriptorType))
	{
		imageResource.info.flags |= VK_IMAGE_CREATE_DESCRIPTOR_BUFFER_CAPTURE_REPLAY_BIT_EXT;

		DE_ASSERT(!imageResource.image);
		imageResource.image = createImage(*m_deviceInterface, *m_device, &imageResource.info);

		VkImageCaptureDescriptorDataInfoEXT info = initVulkanStructure();
		info.image = *imageResource.image;

		DE_ASSERT(captureReplayData.empty());
		captureReplayData.resize(m_descriptorBufferProperties.imageCaptureReplayDescriptorDataSize);

		VK_CHECK(m_deviceInterface->getImageOpaqueCaptureDescriptorDataEXT(*m_device, &info, captureReplayData.data()));
	}
	else if (isReplayDescriptor(descriptorType))
	{
		// Free the previous image and its memory
		reset(imageResource.image);
		reset(imageResource.alloc);

		DE_ASSERT(!captureReplayData.empty());

		VkOpaqueCaptureDescriptorDataCreateInfoEXT info = initVulkanStructure();
		info.opaqueCaptureDescriptorData = captureReplayData.data();

		imageResource.info.flags |= VK_IMAGE_CREATE_DESCRIPTOR_BUFFER_CAPTURE_REPLAY_BIT_EXT;
		imageResource.info.pNext = &info;

		imageResource.image = createImage(*m_deviceInterface, *m_device, &imageResource.info);
	}
	else
	{
		DE_ASSERT(!imageResource.image);
		imageResource.image = createImage(*m_deviceInterface, *m_device, &imageResource.info);
	}

	// Memory allocation
	auto memReqs = getImageMemoryRequirements(*m_deviceInterface, *m_device, *imageResource.image);

	VkMemoryOpaqueCaptureAddressAllocateInfo	opaqueCaptureAddressAllocateInfo	= initVulkanStructure();
	VkMemoryAllocateFlagsInfo					allocFlagsInfo						= initVulkanStructure();

	if (m_params.isCaptureReplayDescriptor(descriptorType))
	{
		allocFlagsInfo.flags |= VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT | VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
		allocFlagsInfo.pNext = &opaqueCaptureAddressAllocateInfo;

		if (isCaptureDescriptor(descriptorType))
		{
			opaqueCaptureAddressAllocateInfo.opaqueCaptureAddress = 0ull;
		}
		else if (isReplayDescriptor(descriptorType))
		{
			opaqueCaptureAddressAllocateInfo.opaqueCaptureAddress = imageResource.opaqueCaptureAddress;
		}
	}

	DE_ASSERT(!imageResource.alloc);
	imageResource.sizeBytes	= memReqs.size;
	imageResource.alloc		= allocate(memReqs, MemoryRequirement::Local, &allocFlagsInfo);

	if (isCaptureDescriptor(descriptorType))
	{
		VkDeviceMemoryOpaqueCaptureAddressInfo memoryOpaqueCaptureAddressInfo = initVulkanStructure();

		memoryOpaqueCaptureAddressInfo.memory = imageResource.alloc->getMemory();

		imageResource.opaqueCaptureAddress = m_deviceInterface->getDeviceMemoryOpaqueCaptureAddress(*m_device, &memoryOpaqueCaptureAddressInfo);
	}

	VK_CHECK(m_deviceInterface->bindImageMemory(
		*m_device,
		*imageResource.image,
		imageResource.alloc->getMemory(),
		imageResource.alloc->getOffset()));

	// Image view
	{
		auto& captureReplayDataView = resources.captureReplay.imageViewData;

		DE_ASSERT(imageResource.info.imageType == VK_IMAGE_TYPE_2D);

		VkImageViewCreateInfo createInfo = initVulkanStructure();
		createInfo.image			= *imageResource.image;
		createInfo.viewType			= VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format			= imageResource.info.format;
		createInfo.components		= ComponentMappingIdentity;
		createInfo.subresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);

		if (isCaptureDescriptor(descriptorType))
		{
			createInfo.flags |= VK_IMAGE_VIEW_CREATE_DESCRIPTOR_BUFFER_CAPTURE_REPLAY_BIT_EXT;

			DE_ASSERT(!imageResource.imageView);
			imageResource.imageView = createImageView(*m_deviceInterface, *m_device, &createInfo);

			VkImageViewCaptureDescriptorDataInfoEXT info = initVulkanStructure();
			info.imageView = *imageResource.imageView;

			DE_ASSERT(captureReplayDataView.empty());
			captureReplayDataView.resize(m_descriptorBufferProperties.imageViewCaptureReplayDescriptorDataSize);

			VK_CHECK(m_deviceInterface->getImageViewOpaqueCaptureDescriptorDataEXT(*m_device, &info, captureReplayDataView.data()));
		}
		else if (isReplayDescriptor(descriptorType))
		{
			reset(imageResource.imageView);

			DE_ASSERT(!captureReplayDataView.empty());

			VkOpaqueCaptureDescriptorDataCreateInfoEXT info = initVulkanStructure();
			info.opaqueCaptureDescriptorData = captureReplayDataView.data();

			createInfo.flags |= VK_IMAGE_VIEW_CREATE_DESCRIPTOR_BUFFER_CAPTURE_REPLAY_BIT_EXT;
			createInfo.pNext = &info;

			imageResource.imageView = createImageView(*m_deviceInterface, *m_device, &createInfo);
		}
		else
		{
			// No assertion here, as we must create a new view to go with the image.
			imageResource.imageView = createImageView(*m_deviceInterface, *m_device, &createInfo);
		}
	}
}

// This function prepares a descriptor binding for use:
// - Create necessary buffer/image resources and initialize them
// - Write descriptor data into the descriptor buffer
// - Fix the memory layout of combined image samplers (if needed)
//
void DescriptorBufferTestInstance::initializeBinding(
	const DescriptorSetLayoutHolder&	dsl,
	deUint32							setIndex,
	Binding&							binding)
{
	const auto arrayCount = (binding.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK) ?
		1 : binding.descriptorCount;

	const bool mustSplitCombinedImageSampler =
		(arrayCount > 1) &&
		(binding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) &&
		(m_descriptorBufferProperties.combinedImageSamplerDescriptorSingleArray == VK_FALSE);

	const bool isRobustBufferAccess = (m_params.variant == TestVariant::ROBUST_BUFFER_ACCESS);
	const bool isNullDescriptor =
		(m_params.variant == TestVariant::ROBUST_NULL_DESCRIPTOR) &&
		(binding.descriptorType == m_params.descriptor) &&
		 binding.isTestableDescriptor();

	for (deUint32 arrayIndex = 0; arrayIndex < arrayCount; ++arrayIndex)
	{
		VkDescriptorGetInfoEXT		descGetInfo	= initVulkanStructure();
		VkDescriptorAddressInfoEXT	addressInfo	= initVulkanStructure();
		VkDescriptorImageInfo		imageInfo	{0, 0, VK_IMAGE_LAYOUT_UNDEFINED};	// must be explicitly initialized due to CTS handles inside

		descGetInfo.type = VK_DESCRIPTOR_TYPE_MAX_ENUM;

		if ((binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) ||
			(binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER))
		{
			auto& resources		 = getOrCreateResource(binding, arrayIndex);
			auto& bufferResource = resources.buffer;

			const VkBufferUsageFlags usage =
				(binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT :
				(binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) ? VK_BUFFER_USAGE_STORAGE_BUFFER_BIT : 0;
			DE_ASSERT(usage);

			bufferResource.size = sizeof(deUint32) * (binding.isResultBuffer ? ConstResultBufferDwords : ConstUniformBufferDwords);

			createBufferForBinding(
				resources,
				binding.descriptorType,
				makeBufferCreateInfo(bufferResource.size, usage),
				binding.isResultBuffer);

			deUint32* pBufferData = static_cast<deUint32*>(bufferResource.alloc->getHostPtr());

			if (binding.isResultBuffer || isRobustBufferAccess)
			{
				// We zero the buffer if it's a result buffer or if it's used with robust access.
				deMemset(pBufferData, 0, static_cast<std::size_t>(bufferResource.size));
			}
			else
			{
				const auto data = getExpectedData(m_params.hash, setIndex, binding.binding, arrayIndex);

				for (deUint32 i = 0; i < ConstUniformBufferDwords; ++i)
				{
					pBufferData[i] = data + i;
				}
			}

			addressInfo.address = bufferResource.deviceAddress;
			addressInfo.range   = bufferResource.size;
			addressInfo.format  = VK_FORMAT_UNDEFINED;

			DE_UNREF(ConstRobustBufferAlignment);
			DE_ASSERT(binding.isResultBuffer || !isRobustBufferAccess || ((addressInfo.range % ConstRobustBufferAlignment) == 0));

			descGetInfo.type				= binding.descriptorType;
			descGetInfo.data.pUniformBuffer = isNullDescriptor ? nullptr : &addressInfo;	// and pStorageBuffer
		}
		else if (binding.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
		{
			// Inline uniforms don't use a backing buffer.
			DE_ASSERT(binding.perBindingResourceIndex[arrayIndex] == INDEX_INVALID);
		}
		else if ((binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER) ||
				 (binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER))
		{
			auto& resources		 = getOrCreateResource(binding, arrayIndex);
			auto& bufferResource = resources.buffer;

			const VkBufferUsageFlags usage =
				(binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER) ? VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT :
				(binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER) ? VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT : 0;
			DE_ASSERT(usage);

			bufferResource.size = ConstTexelBufferElements * sizeof(deUint32);

			createBufferForBinding(
				resources,
				binding.descriptorType,
				makeBufferCreateInfo(bufferResource.size, usage),
				binding.isResultBuffer);

			if (m_params.isPushDescriptorTest())
			{
				// Push descriptors use buffer views.
				auto& bufferViewResource = (**m_resources[binding.perBindingResourceIndex[arrayIndex]]).bufferView;

				bufferViewResource = makeBufferView(
					*m_deviceInterface,
					*m_device,
					*bufferResource.buffer,
					VK_FORMAT_R32_UINT,
					0,
					bufferResource.size);
			}

			deUint32* pBufferData = static_cast<deUint32*>(bufferResource.alloc->getHostPtr());

			if (isRobustBufferAccess)
			{
				// Zero the buffer used with robust access.
				deMemset(pBufferData, 0, static_cast<std::size_t>(bufferResource.size));
			}
			else
			{
				const auto data = getExpectedData(m_params.hash, setIndex, binding.binding, arrayIndex);

				for (deUint32 i = 0; i < ConstTexelBufferElements; ++i)
				{
					pBufferData[i] = data + i;
				}
			}

			addressInfo.address = bufferResource.deviceAddress;
			addressInfo.range	= bufferResource.size;
			addressInfo.format	= VK_FORMAT_R32_UINT;

			DE_UNREF(ConstRobustBufferAlignment);
			DE_ASSERT(!isRobustBufferAccess || ((addressInfo.range % ConstRobustBufferAlignment) == 0));

			descGetInfo.type					 = binding.descriptorType;
			descGetInfo.data.pUniformTexelBuffer = isNullDescriptor ? nullptr : &addressInfo;	// and pStorageTexelBuffer
		}
		else if ((binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) ||
				 (binding.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) ||
				 (binding.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT) ||
				 (binding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER))
		{
			// Check if we had already added the resource while handling samplers.
			auto& resources		= getOrCreateResource(binding, arrayIndex);
			auto& imageResource = resources.image;
			auto& stagingBuffer = resources.buffer;

			{
				VkImageLayout	  layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				VkImageUsageFlags usage  = VK_IMAGE_USAGE_TRANSFER_DST_BIT;

				if (binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
				{
					usage |= VK_IMAGE_USAGE_STORAGE_BIT;
					layout = VK_IMAGE_LAYOUT_GENERAL;
				}
				else if (binding.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
				{
					usage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
				}
				else
				{
					usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
				}

				// We ensure the extent matches the render area, for the sake of input attachment case.
				imageResource.info = initVulkanStructure();
				imageResource.info.flags					= 0;
				imageResource.info.imageType				= VK_IMAGE_TYPE_2D;
				imageResource.info.format					= VK_FORMAT_R32_UINT;
				imageResource.info.extent.width				= m_renderArea.extent.width;
				imageResource.info.extent.height			= m_renderArea.extent.height;
				imageResource.info.extent.depth				= 1;
				imageResource.info.mipLevels				= 1;
				imageResource.info.arrayLayers				= 1;
				imageResource.info.samples					= VK_SAMPLE_COUNT_1_BIT;
				imageResource.info.tiling					= VK_IMAGE_TILING_OPTIMAL;
				imageResource.info.usage					= usage;
				imageResource.info.sharingMode				= VK_SHARING_MODE_EXCLUSIVE;
				imageResource.info.queueFamilyIndexCount	= 0;
				imageResource.info.pQueueFamilyIndices		= nullptr;
				imageResource.info.initialLayout			= VK_IMAGE_LAYOUT_UNDEFINED;

				createImageForBinding(resources, binding.descriptorType);

				imageResource.layout = layout;

				imageInfo.imageLayout = layout;
				imageInfo.imageView   = *imageResource.imageView;

				descGetInfo.type = binding.descriptorType;

				if (binding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
				{
					if (isNullDescriptor)
						imageInfo.imageView = DE_NULL;

					descGetInfo.data.pCombinedImageSampler = &imageInfo;
				}
				else
					descGetInfo.data.pStorageImage = isNullDescriptor ? nullptr : &imageInfo;
			}
			{
				const auto numPixels = m_renderArea.extent.width * m_renderArea.extent.height;
				stagingBuffer.size = sizeof(deUint32) * numPixels;
				auto createInfo = makeBufferCreateInfo(stagingBuffer.size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

				stagingBuffer.buffer = createBuffer(*m_deviceInterface, *m_device, &createInfo);

				auto memReqs = getBufferMemoryRequirements(*m_deviceInterface, *m_device, *stagingBuffer.buffer);

				stagingBuffer.alloc = allocate(memReqs, MemoryRequirement::HostVisible);

				VK_CHECK(m_deviceInterface->bindBufferMemory(
					*m_device,
					*stagingBuffer.buffer,
					stagingBuffer.alloc->getMemory(),
					stagingBuffer.alloc->getOffset()));

				// Fill the whole image uniformly
				deUint32* pBufferData = static_cast<deUint32*>(stagingBuffer.alloc->getHostPtr());
				deUint32 expectedData;

				if (binding.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
				{
					expectedData = getExpectedData(m_params.hash, setIndex, binding.binding, binding.inputAttachmentIndex + arrayIndex);
				}
				else
				{
					expectedData = getExpectedData(m_params.hash, setIndex, binding.binding, arrayIndex);
				}

				std::fill(pBufferData, pBufferData + numPixels, expectedData);
			}

			if (binding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
			{
				DE_ASSERT(m_params.variant != TestVariant::EMBEDDED_IMMUTABLE_SAMPLERS);

				DE_ASSERT(binding.perBindingResourceIndex[arrayIndex] != INDEX_INVALID);
				auto& resourceSampler = (**m_resources[binding.perBindingResourceIndex[arrayIndex]]).sampler;

				imageInfo.sampler = *resourceSampler;
			}
		}
		else if (binding.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER)
		{
			if (m_params.variant != TestVariant::EMBEDDED_IMMUTABLE_SAMPLERS)
			{
				DE_ASSERT(binding.perBindingResourceIndex[arrayIndex] != INDEX_INVALID);
				auto& resourceSampler = (**m_resources[binding.perBindingResourceIndex[arrayIndex]]).sampler;

				descGetInfo.type = binding.descriptorType;
				descGetInfo.data.pSampler = &*resourceSampler;
			}
		}
		else if (binding.descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
		{
			Allocator&									allocator		= *m_allocatorPtr;
			const deUint32								expectedData	= getExpectedData(m_params.hash, setIndex, binding.binding, arrayIndex);
			const float									zDepth			= float(expectedData);
			const std::vector<tcu::Vec3>				vertices
			{
				tcu::Vec3(-1.0f, -1.0f, zDepth),
				tcu::Vec3(-1.0f,  1.0f, zDepth),
				tcu::Vec3( 1.0f, -1.0f, zDepth),

				tcu::Vec3(-1.0f,  1.0f, zDepth),
				tcu::Vec3( 1.0f,  1.0f, zDepth),
				tcu::Vec3( 1.0f, -1.0f, zDepth),
			};
			auto&										resources			= getOrCreateResource(binding, arrayIndex);
			const bool									replayableBinding	= binding.isTestableDescriptor();
			VkAccelerationStructureCreateFlagsKHR		createFlags			= (m_params.isCaptureReplayDescriptor(binding.descriptorType) && replayableBinding)
																			? static_cast<VkAccelerationStructureCreateFlagsKHR>(VK_ACCELERATION_STRUCTURE_CREATE_DESCRIPTOR_BUFFER_CAPTURE_REPLAY_BIT_EXT)
																			: static_cast<VkAccelerationStructureCreateFlagsKHR>(0u);
			vk::MemoryRequirement						memoryReqs			= (m_params.isCaptureReplayDescriptor(binding.descriptorType) && replayableBinding)
																			? MemoryRequirement::DeviceAddressCaptureReplay
																			: MemoryRequirement::Any;
			VkOpaqueCaptureDescriptorDataCreateInfoEXT	infos[]				= { initVulkanStructure(), initVulkanStructure() };
			VkOpaqueCaptureDescriptorDataCreateInfoEXT*	infoPtrs[]			= { DE_NULL, DE_NULL };

			if (isReplayDescriptor(binding.descriptorType) && replayableBinding)
			{
				resources.rtBlas.clear();
				resources.rtTlas.clear();

				std::vector<deUint8>* captureReplayDatas[] = { &resources.captureReplay.accelerationStructureDataBlas, &resources.captureReplay.accelerationStructureDataTlas };

				for (int ndx = 0; ndx < 2; ++ndx)
				{
					std::vector<deUint8>&						captureReplayData	= *captureReplayDatas[ndx];
					VkOpaqueCaptureDescriptorDataCreateInfoEXT&	info				= infos[ndx];

					info.opaqueCaptureDescriptorData = captureReplayData.data();
					infoPtrs[ndx] = &infos[ndx];
				}
			}

			{
				DE_ASSERT(resources.rtBlas.get() == DE_NULL);

				resources.rtBlas = de::SharedPtr<BottomLevelAccelerationStructure>(makeBottomLevelAccelerationStructure().release());
				if (binding.isRayTracingAS)
					resources.rtBlas->setDefaultGeometryData(m_params.stage);
				else
					resources.rtBlas->setGeometryData(vertices, true);
				resources.rtBlas->setCreateFlags(createFlags);
				resources.rtBlas->create(*m_deviceInterface, *m_device, allocator, 0, 0, infoPtrs[0], memoryReqs);
			}

			{
				DE_ASSERT(resources.rtTlas.get() == DE_NULL);

				resources.rtTlas = makeTopLevelAccelerationStructure();
				resources.rtTlas->addInstance(resources.rtBlas);
				resources.rtTlas->setCreateFlags(createFlags);
				resources.rtTlas->create(*m_deviceInterface, *m_device, allocator, 0, 0, infoPtrs[1], memoryReqs);
			}

			if (isCaptureDescriptor(binding.descriptorType) && replayableBinding)
			{
				const VkAccelerationStructureKHR*	accelerationStructures[]	= { resources.rtBlas->getPtr() , resources.rtTlas->getPtr() };
				std::vector<deUint8>*				captureReplayDatas[]		= { &resources.captureReplay.accelerationStructureDataBlas, &resources.captureReplay.accelerationStructureDataTlas };

				for (int ndx = 0; ndx < 2; ++ndx)
				{
					VkAccelerationStructureCaptureDescriptorDataInfoEXT	info					= initVulkanStructure();
					const VkAccelerationStructureKHR*					accelerationStructure	= accelerationStructures[ndx];
					std::vector<deUint8>&								captureReplayData		= *captureReplayDatas[ndx];

					DE_ASSERT(accelerationStructure != DE_NULL && *accelerationStructure != DE_NULL);
					DE_ASSERT(captureReplayData.empty());

					info.accelerationStructure = *accelerationStructure;

					captureReplayData.resize(m_descriptorBufferProperties.accelerationStructureCaptureReplayDescriptorDataSize);

					VK_CHECK(m_deviceInterface->getAccelerationStructureOpaqueCaptureDescriptorDataEXT(*m_device, &info, captureReplayData.data()));
				}
			}

			descGetInfo.type = binding.descriptorType;
			descGetInfo.data.accelerationStructure	= isNullDescriptor ? DE_NULL : getAccelerationStructureDeviceAddress(*m_deviceInterface, *m_device, *resources.rtTlas->getPtr());
		}
		else
		{
			TCU_THROW(InternalError, "Not implemented");
		}

		if (dsl.usePushDescriptors || dsl.sizeOfLayout == 0)
		{
			// Push descriptors don't rely on descriptor buffers, move to the next binding.
			continue;
		}

		// Write the descriptor at the right offset in the descriptor buffer memory.
		// - With inline uniform blocks, we write the uniform data into the descriptor buffer directly.
		// - With regular descriptors, the written memory is opaque to us (same goes for null descriptors).
		{
			void*		bindingHostPtr	= nullptr;
			Allocation* pAlloc			= nullptr;
			const auto	arrayOffset		= arrayIndex * getDescriptorSize(binding);

			if (dsl.stagingBufferOffset == OFFSET_UNUSED)
			{
				const auto& descriptorBuffer = *m_descriptorBuffers[dsl.bufferIndex];
				const auto bufferHostPtr = offsetPtr(descriptorBuffer.alloc->getHostPtr(), dsl.bufferOffset);

				bindingHostPtr = offsetPtr(bufferHostPtr, binding.offset);
				pAlloc = descriptorBuffer.alloc.get();
			}
			else
			{
				bindingHostPtr = offsetPtr(
					m_descriptorStagingBuffer.alloc->getHostPtr(),
					dsl.stagingBufferOffset + binding.offset);

				pAlloc = m_descriptorStagingBuffer.alloc.get();
			}

			if (binding.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
			{
				DE_ASSERT(arrayIndex == 0);

				// Inline uniform data is written in descriptor buffer directly.
				const auto numDwords = binding.descriptorCount / sizeof(deUint32);
				const auto data		 = getExpectedData(m_params.hash, setIndex, binding.binding, arrayIndex);

				deUint32* pInlineData = static_cast<deUint32*>(bindingHostPtr);

				for (deUint32 i = 0; i < numDwords; ++i)
				{
					pInlineData[i] = data + i;
				}
			}
			else if (isReplayDescriptor(binding.descriptorType))
			{
				// We're expecting that a descriptor based on replayed resources will have exactly the same binary data.
				// Copy it and compare after obtaining the new descriptor.
				//
				auto		descriptorPtr  = offsetPtr(bindingHostPtr, arrayOffset);
				const auto  descriptorSize = static_cast<size_t>(getDescriptorSize(binding));

				std::vector<deUint8> reference(descriptorSize);
				deMemcpy(reference.data(), descriptorPtr, descriptorSize);

				deMemset(descriptorPtr, 0xcc, descriptorSize);
				m_deviceInterface->getDescriptorEXT(*m_device, &descGetInfo, descriptorSize, descriptorPtr);

				if (deMemCmp(reference.data(), descriptorPtr, descriptorSize) != 0)
				{
					TCU_THROW(TestError, "Replayed descriptor differs from the captured descriptor");
				}
			}
			else
			{
				auto		descriptorPtr  = offsetPtr(bindingHostPtr, arrayOffset);
				const auto  descriptorSize = static_cast<size_t>(getDescriptorSize(binding));
				m_deviceInterface->getDescriptorEXT(*m_device, &descGetInfo, descriptorSize, descriptorPtr);
			}

			// After writing the last array element, rearrange the split combined image sampler data.
			if (mustSplitCombinedImageSampler && ((arrayIndex + 1) == arrayCount))
			{
				// We determined the size of the descriptor set layout on the VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER type,
				// so it's expected the following holds true.
				DE_ASSERT((m_descriptorBufferProperties.sampledImageDescriptorSize + m_descriptorBufferProperties.samplerDescriptorSize) ==
					m_descriptorBufferProperties.combinedImageSamplerDescriptorSize);

				std::vector<deUint8> scratchSpace(
					arrayCount * m_descriptorBufferProperties.combinedImageSamplerDescriptorSize);

				const auto descriptorArraySize = static_cast<std::size_t>(arrayCount * m_descriptorBufferProperties.combinedImageSamplerDescriptorSize);

				deMemcpy(scratchSpace.data(), bindingHostPtr, descriptorArraySize);
				deMemset(bindingHostPtr, 0, descriptorArraySize);

				const void*	combinedReadPtr = scratchSpace.data();
				void*		imageWritePtr   = bindingHostPtr;
				void*		samplerWritePtr = offsetPtr(bindingHostPtr, arrayCount * m_descriptorBufferProperties.sampledImageDescriptorSize);

				for (deUint32 i = 0; i < arrayCount; ++i)
				{
					deMemcpy(imageWritePtr,		offsetPtr(combinedReadPtr, 0),														 m_descriptorBufferProperties.sampledImageDescriptorSize);
					deMemcpy(samplerWritePtr,	offsetPtr(combinedReadPtr, m_descriptorBufferProperties.sampledImageDescriptorSize), m_descriptorBufferProperties.samplerDescriptorSize);

					combinedReadPtr	= offsetPtr(combinedReadPtr, m_descriptorBufferProperties.combinedImageSamplerDescriptorSize);
					imageWritePtr	= offsetPtr(imageWritePtr,   m_descriptorBufferProperties.sampledImageDescriptorSize);
					samplerWritePtr	= offsetPtr(samplerWritePtr, m_descriptorBufferProperties.samplerDescriptorSize);
				}
			}

			flushAlloc(*m_deviceInterface, *m_device, *pAlloc);
		}
	}
}

// Update a descriptor set with a push or a push template.
//
void DescriptorBufferTestInstance::pushDescriptorSet(
		VkCommandBuffer						cmdBuf,
		VkPipelineBindPoint					bindPoint,
		const DescriptorSetLayoutHolder&	dsl,
		deUint32							setIndex) const
{
	std::vector<PushDescriptorData>								descriptorData(dsl.bindings.size());	// Allocate empty elements upfront
	std::vector<VkWriteDescriptorSet>							descriptorWrites;
	std::vector<VkWriteDescriptorSetAccelerationStructureKHR>	descriptorWritesAccelerationStructures;

	descriptorWrites.reserve(dsl.bindings.size());
	descriptorWritesAccelerationStructures.reserve(dsl.bindings.size());

	// Fill in the descriptor data structure. It can be used by the regular and templated update path.

	for (deUint32 bindingIndex = 0; bindingIndex < u32(dsl.bindings.size()); ++bindingIndex)
	{
		const auto& binding = dsl.bindings[bindingIndex];

		VkWriteDescriptorSet write = initVulkanStructure();
		write.dstSet			= DE_NULL;	// ignored with push descriptors
		write.dstBinding		= bindingIndex;
		write.dstArrayElement	= 0;
		write.descriptorCount	= binding.descriptorCount;
		write.descriptorType	= binding.descriptorType;

		for (deUint32 arrayIndex = 0; arrayIndex < write.descriptorCount; ++arrayIndex)
		{
			DE_ASSERT(binding.perBindingResourceIndex[arrayIndex] != INDEX_INVALID);

			if ((binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) ||
				(binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER))
			{
				const auto& bufferResource = (**m_resources[binding.perBindingResourceIndex[arrayIndex]]).buffer;

				auto pInfo = &descriptorData[bindingIndex].bufferInfos[arrayIndex];
				pInfo->buffer = *bufferResource.buffer;
				pInfo->offset = 0;
				pInfo->range  = bufferResource.size;

				if (arrayIndex == 0)
				{
					write.pBufferInfo = pInfo;
				}
			}
			else if ((binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER) ||
					 (binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER))
			{
				const auto& bufferViewResource = (**m_resources[binding.perBindingResourceIndex[arrayIndex]]).bufferView;

				auto pBufferView = &descriptorData[bindingIndex].texelBufferViews[arrayIndex];
				*pBufferView = *bufferViewResource;

				if (arrayIndex == 0)
				{
					write.pTexelBufferView = pBufferView;
				}
			}
			else if ((binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) ||
					 (binding.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) ||
					 (binding.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT) ||
					 (binding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) ||
					 (binding.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER))
			{
				const auto& imageResource   = (**m_resources[binding.perBindingResourceIndex[arrayIndex]]).image;
				const auto& samplerResource = (**m_resources[binding.perBindingResourceIndex[arrayIndex]]).sampler;

				// Dereferencing unused resources will return null handles, so we can treat all these descriptors uniformly.

				auto pInfo = &descriptorData[bindingIndex].imageInfos[arrayIndex];
				pInfo->imageView   = *imageResource.imageView;
				pInfo->imageLayout = imageResource.layout;
				pInfo->sampler	   = *samplerResource;

				if (arrayIndex == 0)
				{
					write.pImageInfo = pInfo;
				}
			}
			else if (binding.descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
			{
				const ResourceHolder&				resources					= **m_resources[binding.perBindingResourceIndex[arrayIndex]];
				const VkAccelerationStructureKHR*	accelerationStructurePtr	= resources.rtTlas.get()->getPtr();

				DE_ASSERT(accelerationStructurePtr != DE_NULL && *accelerationStructurePtr != DE_NULL);

				descriptorData[bindingIndex].accelerationStructures[arrayIndex] = *accelerationStructurePtr;

				if (arrayIndex == 0)
				{
					VkWriteDescriptorSetAccelerationStructureKHR	descriptorWritesAccelerationStructure = initVulkanStructure();

					descriptorWritesAccelerationStructure.accelerationStructureCount = write.descriptorCount;
					descriptorWritesAccelerationStructure.pAccelerationStructures = descriptorData[bindingIndex].accelerationStructures;

					descriptorWritesAccelerationStructures.emplace_back(descriptorWritesAccelerationStructure);

					write.pNext = &descriptorWritesAccelerationStructures[descriptorWritesAccelerationStructures.size() - 1];
				}
			}
			else
			{
				TCU_THROW(InternalError, "Not implemented");
			}
		}

		if (m_params.variant == TestVariant::PUSH_DESCRIPTOR)
		{
			descriptorWrites.emplace_back(write);
		}
	}

	if (m_params.variant == TestVariant::PUSH_DESCRIPTOR)
	{
		m_deviceInterface->cmdPushDescriptorSetKHR(
			cmdBuf,
			bindPoint,
			*m_pipelineLayout,
			setIndex,
			u32(descriptorWrites.size()),
			descriptorWrites.data());
	}
	else if (m_params.variant == TestVariant::PUSH_TEMPLATE)
	{
		std::vector<VkDescriptorUpdateTemplateEntry> updateEntries(descriptorData.size());	// preallocate

		const auto dataBasePtr = reinterpret_cast<deUint8*>(descriptorData.data());

		for (deUint32 bindingIndex = 0; bindingIndex < u32(dsl.bindings.size()); ++bindingIndex)
		{
			const auto& binding = dsl.bindings[bindingIndex];
			const auto& data    = descriptorData[bindingIndex];

			auto& entry = updateEntries[bindingIndex];
			entry.dstBinding		= binding.binding;
			entry.dstArrayElement	= 0;
			entry.descriptorCount	= binding.descriptorCount;
			entry.descriptorType	= binding.descriptorType;

			switch(binding.descriptorType)
			{
			case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
				entry.offset = basePtrOffsetOf(dataBasePtr, data.bufferInfos);
				entry.stride = sizeof(data.bufferInfos[0]);
				break;

			case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
			case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
				entry.offset = basePtrOffsetOf(dataBasePtr, data.texelBufferViews);
				entry.stride = sizeof(data.texelBufferViews[0]);
				break;

			case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
			case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
			case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
			case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			case VK_DESCRIPTOR_TYPE_SAMPLER:
				entry.offset = basePtrOffsetOf(dataBasePtr, data.imageInfos);
				entry.stride = sizeof(data.imageInfos[0]);
				break;

			case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
				entry.offset = basePtrOffsetOf(dataBasePtr, data.accelerationStructures);
				entry.stride = sizeof(data.accelerationStructures[0]);
				break;

			default:
				DE_ASSERT(0);
				break;
			}
		}

		VkDescriptorUpdateTemplateCreateInfo createInfo = initVulkanStructure();
		createInfo.templateType					= VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS_KHR;
		createInfo.descriptorSetLayout			= *dsl.layout;
		createInfo.pipelineBindPoint			= bindPoint;
		createInfo.pipelineLayout				= *m_pipelineLayout;
		createInfo.set							= setIndex;
		createInfo.descriptorUpdateEntryCount	= u32(updateEntries.size());
		createInfo.pDescriptorUpdateEntries		= updateEntries.data();

		auto descriptorUpdateTemplate = createDescriptorUpdateTemplate(
			*m_deviceInterface,
			*m_device,
			&createInfo);

		m_deviceInterface->cmdPushDescriptorSetWithTemplateKHR(
			cmdBuf,
			*descriptorUpdateTemplate,
			*m_pipelineLayout,
			setIndex,
			dataBasePtr);
	}
}

// Perform the test accoring to the parameters. At high level, all tests perform these steps:
//
// - Create a new device and queues, query extension properties.
// - Fill descriptor set layouts and bindings, based on SimpleBinding's.
// - Create samplers, if needed. Set immutable samplers in bindings.
// - Create descriptor set layouts.
// - Create descriptor buffers.
// - Iterate over all bindings to:
//   - Create their resources (images, buffers) and initialize them
//   - Write bindings to descriptor buffer memory
//   - Fix combined image samplers for arrayed bindings (if applicable)
// - Create the pipeline layout, shaders, and the pipeline
// - Create the command buffer and record the commands (barriers omitted for brevity):
//   - Bind the pipeline and the descriptor buffers
//   - Upload descriptor buffer data (with staged uploads)
//   - Upload image data (if images are used)
//   - Push descriptors (if used)
//   - Dispatch or draw
//   - Submit the commands
//   - Map the result buffer to a host pointer
//   - Verify the result and log diagnostic on a failure
//
// Verification logic is very simple.
//
// Each successful binding read will increment the result counter. If the shader got an unexpected value, the counter
// will be less than expected. Additionally, the first failed set/binding/array index will be recorded.
//
// With capture/replay tests, iterate() will be called twice, splitting the test into capture and replay passes.
// The capture pass saves the opaque data, while the replay pass uses it and compares the results.
//
tcu::TestStatus	DescriptorBufferTestInstance::iterate()
{
	DE_ASSERT(m_params.bufferBindingCount <= m_descriptorBufferProperties.maxDescriptorBufferBindings);

	const auto& vk = *m_deviceInterface;

	if (m_testIteration == 0)
	{
		deUint32 currentSet = INDEX_INVALID;

		for (const auto& sb : m_simpleBindings)
		{
			if ((currentSet == INDEX_INVALID) || (currentSet < sb.set))
			{
				currentSet = sb.set;

				addDescriptorSetLayout();
			}

			auto&				dsl			= **m_descriptorSetLayouts.back();
			VkShaderStageFlags	stageFlags	= sb.isRayTracingAS
											? static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_RAYGEN_BIT_KHR)
											: static_cast<VkShaderStageFlags>(0u);

			Binding binding {};
			binding.binding				 = sb.binding;
			binding.descriptorType		 = sb.type;
			binding.stageFlags			 = m_params.stage | stageFlags;
			binding.inputAttachmentIndex = sb.inputAttachmentIndex;
			binding.isResultBuffer		 = sb.isResultBuffer;
			binding.isRayTracingAS		 = sb.isRayTracingAS;

			if (sb.type == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
			{
				binding.descriptorCount = sizeof(deUint32) * ConstInlineBlockDwords;
			}
			else
			{
				binding.descriptorCount = sb.count;
			}

			if ((sb.type == VK_DESCRIPTOR_TYPE_SAMPLER) ||
				(sb.type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER))
			{
				if (sb.isEmbeddedImmutableSampler)
				{
					dsl.hasEmbeddedImmutableSamplers = true;
				}
			}

			if (m_params.isPushDescriptorTest() &&
				(m_params.pushDescriptorSetIndex == (m_descriptorSetLayouts.size() - 1)))
			{
				dsl.usePushDescriptors = true;
			}

			dsl.bindings.emplace_back(binding);
		}
	}

	// We create samplers before creating the descriptor set layouts, in case we need to use
	// immutable (or embedded) samplers.

	for (deUint32 setIndex = 0; setIndex < u32(m_descriptorSetLayouts.size()); ++setIndex)
	{
		auto& dsl = **m_descriptorSetLayouts[setIndex];

		for (deUint32 bindingIndex = 0; bindingIndex < u32(dsl.bindings.size()); ++bindingIndex)
		{
			auto& binding = dsl.bindings[bindingIndex];

			if ((binding.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER) ||
				(binding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER))
			{
				for (deUint32 arrayIndex = 0; arrayIndex < binding.descriptorCount; ++arrayIndex)
				{
					if (binding.perBindingResourceIndex[arrayIndex] == INDEX_INVALID)
					{
						binding.perBindingResourceIndex[arrayIndex] = addResource();
					}

					auto& resources			= **m_resources[binding.perBindingResourceIndex[arrayIndex]];
					auto& captureReplayData = resources.captureReplay.samplerData;

					// Use CLAMP_TO_BORDER to verify that sampling outside the image will make use of the sampler's
					// properties. The border color used must match the one in glslOutputVerification().

					VkSamplerCreateInfo createInfo = initVulkanStructure();
					createInfo.magFilter				= VK_FILTER_NEAREST;
					createInfo.minFilter				= VK_FILTER_NEAREST;
					createInfo.mipmapMode				= VK_SAMPLER_MIPMAP_MODE_NEAREST;
					createInfo.addressModeU				= VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
					createInfo.addressModeV				= VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
					createInfo.addressModeW				= VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
					createInfo.mipLodBias				= 0.0f;
					createInfo.anisotropyEnable			= VK_FALSE;
					createInfo.maxAnisotropy			= 1.0f;
					createInfo.compareEnable			= VK_FALSE;
					createInfo.compareOp				= VK_COMPARE_OP_NEVER;
					createInfo.minLod					= 0.0;
					createInfo.maxLod					= 0.0;
					createInfo.borderColor				= VK_BORDER_COLOR_INT_OPAQUE_BLACK;
					createInfo.unnormalizedCoordinates	= VK_FALSE;

					VkSamplerCustomBorderColorCreateInfoEXT customBorderColorInfo = initVulkanStructure();

					const void** nextPtr = &createInfo.pNext;

					if (m_params.subcase == SubCase::CAPTURE_REPLAY_CUSTOM_BORDER_COLOR)
					{
						createInfo.borderColor = VK_BORDER_COLOR_INT_CUSTOM_EXT;

						customBorderColorInfo.format			= VK_FORMAT_R32_UINT;
						customBorderColorInfo.customBorderColor = makeClearValueColorU32(2, 0, 0, 1).color;

						addToChainVulkanStructure(&nextPtr, customBorderColorInfo);
					}

					if (isCaptureDescriptor(VK_DESCRIPTOR_TYPE_SAMPLER) ||
						isCaptureDescriptor(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER))
					{
						createInfo.flags |= VK_SAMPLER_CREATE_DESCRIPTOR_BUFFER_CAPTURE_REPLAY_BIT_EXT;

						resources.sampler = createSampler(vk, *m_device, &createInfo);

						VkSamplerCaptureDescriptorDataInfoEXT info = initVulkanStructure();
						info.sampler = *resources.sampler;

						DE_ASSERT(captureReplayData.empty());
						captureReplayData.resize(m_descriptorBufferProperties.samplerCaptureReplayDescriptorDataSize);

						VK_CHECK(m_deviceInterface->getSamplerOpaqueCaptureDescriptorDataEXT(*m_device, &info, captureReplayData.data()));
					}
					else if (isReplayDescriptor(VK_DESCRIPTOR_TYPE_SAMPLER) ||
							 isReplayDescriptor(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER))
					{
						reset(resources.sampler);

						DE_ASSERT(!captureReplayData.empty());

						VkOpaqueCaptureDescriptorDataCreateInfoEXT info = initVulkanStructure();
						info.opaqueCaptureDescriptorData = captureReplayData.data();

						createInfo.flags |= VK_SAMPLER_CREATE_DESCRIPTOR_BUFFER_CAPTURE_REPLAY_BIT_EXT;

						addToChainVulkanStructure(&nextPtr, info);

						resources.sampler = createSampler(vk, *m_device, &createInfo);
					}
					else
					{
						resources.sampler = createSampler(vk, *m_device, &createInfo);
					}
				}
			}
		}
	}

	if ((m_params.variant == TestVariant::EMBEDDED_IMMUTABLE_SAMPLERS) ||
		(m_params.subcase == SubCase::IMMUTABLE_SAMPLERS))
	{
		// Patch immutable sampler pointers, now that all memory has been allocated and pointers won't move.

		for (deUint32 setIndex = 0; setIndex < u32(m_descriptorSetLayouts.size()); ++setIndex)
		{
			auto& dsl = **m_descriptorSetLayouts[setIndex];

			for (deUint32 bindingIndex = 0; bindingIndex < u32(dsl.bindings.size()); ++bindingIndex)
			{
				auto& binding = dsl.bindings[bindingIndex];

				for (deUint32 resourceIndex = 0; resourceIndex < DE_LENGTH_OF_ARRAY(binding.perBindingResourceIndex); ++resourceIndex)
				{
					if (binding.perBindingResourceIndex[resourceIndex] != INDEX_INVALID)
					{
						const auto& resources = **m_resources[binding.perBindingResourceIndex[resourceIndex]];

						if (resources.sampler)
						{
							DE_ASSERT(resourceIndex < DE_LENGTH_OF_ARRAY(binding.immutableSamplers));

							binding.immutableSamplers[resourceIndex] = *resources.sampler;
						}
					}
				}
			}
		}
	}

	if (m_testIteration == 0)
	{
		createDescriptorSetLayouts();
		createDescriptorBuffers();
	}

	for (deUint32 setIndex = 0; setIndex < u32(m_descriptorSetLayouts.size()); ++setIndex)
	{
		auto& dsl = **m_descriptorSetLayouts[setIndex];

		if (dsl.hasEmbeddedImmutableSamplers)
		{
			// Embedded samplers are not written to the descriptor buffer directly.
			continue;
		}

		for (deUint32 bindingIndex = 0; bindingIndex < u32(dsl.bindings.size()); ++bindingIndex)
		{
			auto& binding = dsl.bindings[bindingIndex];

			// The descriptor bindings are initialized in two situations:
			// 1. in the first test iteration (which is also the capture pass of capture/replay test)
			// 2. in the replay pass, for the binding with the matching descriptor type
			//
			if ((m_testIteration == 0) ||
				(binding.isTestableDescriptor() &&
				 m_params.isCaptureReplayDescriptor(binding.descriptorType)))
			{
				initializeBinding(dsl, setIndex, binding);
			}
		}
	}

	{
		VkPipelineLayoutCreateInfo createInfo = initVulkanStructure();
		const auto dslCopy = getDescriptorSetLayouts(m_descriptorSetLayouts);
		createInfo.setLayoutCount = u32(dslCopy.size());
		createInfo.pSetLayouts = dslCopy.data();

		m_pipelineLayout = createPipelineLayout(vk, *m_device, &createInfo);
	}

	if (m_params.isCompute())
	{
		const auto					shaderModule		= createShaderModule(vk, *m_device, getShaderBinary(VK_SHADER_STAGE_COMPUTE_BIT), 0u);

		const VkPipelineShaderStageCreateInfo pipelineShaderStageParams
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType						sType;
			nullptr,												// const void*							pNext;
			0u,														// VkPipelineShaderStageCreateFlags		flags;
			VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits				stage;
			*shaderModule,											// VkShaderModule						module;
			"main",													// const char*							pName;
			nullptr,												// const VkSpecializationInfo*			pSpecializationInfo;
		};
		VkComputePipelineCreateInfo pipelineCreateInfo
		{
			VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,		// VkStructureType					sType;
			nullptr,											// const void*						pNext;
			VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,		// VkPipelineCreateFlags			flags;
			pipelineShaderStageParams,							// VkPipelineShaderStageCreateInfo	stage;
			* m_pipelineLayout,									// VkPipelineLayout					layout;
			DE_NULL,											// VkPipeline						basePipelineHandle;
			0,													// deInt32							basePipelineIndex;
		};

		vk::VkPipelineCreateFlags2CreateInfoKHR pipelineFlags2CreateInfo = vk::initVulkanStructure();
		if (m_params.useMaintenance5)
		{
			pipelineFlags2CreateInfo.flags = VK_PIPELINE_CREATE_2_DESCRIPTOR_BUFFER_BIT_EXT;
			pipelineCreateInfo.pNext = &pipelineFlags2CreateInfo;
			pipelineCreateInfo.flags = 0;
		}

		m_pipeline = createComputePipeline(vk, *m_device, DE_NULL, &pipelineCreateInfo);
	}
	else if (m_params.isRayTracing())
	{
		createRayTracingPipeline();
	}
	else
	{
		createGraphicsPipeline();
	}

	{
		auto		cmdPool			= makeCommandPool(vk, *m_device, m_queueFamilyIndex);
		auto		cmdBuf			= allocateCommandBuffer(vk, *m_device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		const auto	bindPoint		= m_params.isCompute() ? VK_PIPELINE_BIND_POINT_COMPUTE
									: m_params.isRayTracing() ? VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR
									: m_params.isGraphics() ? VK_PIPELINE_BIND_POINT_GRAPHICS
									: VK_PIPELINE_BIND_POINT_MAX_ENUM;
		const auto	dstStageMask	= m_params.isCompute() ? VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
									: m_params.isRayTracing() ? VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR
									: m_params.isGraphics() ? VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT
									: VK_PIPELINE_STAGE_2_NONE;
		const auto	dstStageMaskUp	= m_params.isCompute() ? VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
									: m_params.isRayTracing() ? VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR
									: m_params.isGraphics() ? VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
									: VK_PIPELINE_STAGE_2_NONE;

		beginCommandBuffer(vk, *cmdBuf);

		vk.cmdBindPipeline(*cmdBuf, bindPoint, *m_pipeline);

		bindDescriptorBuffers(*cmdBuf, bindPoint);

		// Check if we need any staged descriptor set uploads or push descriptors.

		for (deUint32 setIndex = 0; setIndex < m_descriptorSetLayouts.size(); ++setIndex)
		{
			const auto& dsl = **m_descriptorSetLayouts[setIndex];

			if (dsl.usePushDescriptors)
			{
				pushDescriptorSet(*cmdBuf, bindPoint, dsl, setIndex);
			}
			else if (dsl.stagingBufferOffset != OFFSET_UNUSED)
			{
				VkBufferCopy copy {};
				copy.srcOffset = dsl.stagingBufferOffset;
				copy.dstOffset = dsl.bufferOffset;
				copy.size      = dsl.sizeOfLayout;

				VkBuffer descriptorBuffer = *m_descriptorBuffers[dsl.bufferIndex]->buffer;

				vk.cmdCopyBuffer(
					*cmdBuf,
					*m_descriptorStagingBuffer.buffer,
					descriptorBuffer,
					1,	// copy regions
					&copy);

				VkBufferMemoryBarrier2 barrier = initVulkanStructure();
				barrier.srcStageMask		= VK_PIPELINE_STAGE_2_COPY_BIT;
				barrier.srcAccessMask		= VK_ACCESS_2_TRANSFER_WRITE_BIT;
				barrier.dstStageMask		= dstStageMask;
				barrier.dstAccessMask		= VK_ACCESS_2_DESCRIPTOR_BUFFER_READ_BIT_EXT;
				barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.buffer				= descriptorBuffer;
				barrier.offset				= 0;
				barrier.size				= VK_WHOLE_SIZE;

				VkDependencyInfo depInfo = initVulkanStructure();
				depInfo.bufferMemoryBarrierCount	= 1;
				depInfo.pBufferMemoryBarriers		= &barrier;

				vk.cmdPipelineBarrier2(*cmdBuf, &depInfo);
			}
		}

		// Upload image data

		for (deUint32 setIndex = 0; setIndex < u32(m_descriptorSetLayouts.size()); ++setIndex)
		{
			const auto& dsl = **m_descriptorSetLayouts[setIndex];

			for (deUint32 bindingIndex = 0; bindingIndex < u32(dsl.bindings.size()); ++bindingIndex)
			{
				const auto& binding = dsl.bindings[bindingIndex];

				if ((binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) ||
					(binding.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) ||
					(binding.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT) ||
					(binding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER))
				{
					for (deUint32 arrayIndex = 0; arrayIndex < binding.descriptorCount; ++arrayIndex)
					{
						// Need to upload the image data from a staging buffer
						const auto& dstImage  = (**m_resources[binding.perBindingResourceIndex[arrayIndex]]).image;
						const auto& srcBuffer = (**m_resources[binding.perBindingResourceIndex[arrayIndex]]).buffer;

						{
							VkImageMemoryBarrier2 barrier = initVulkanStructure();
							barrier.srcStageMask        = VK_PIPELINE_STAGE_2_NONE;
							barrier.srcAccessMask		= VK_ACCESS_2_NONE;
							barrier.dstStageMask		= VK_PIPELINE_STAGE_2_TRANSFER_BIT;
							barrier.dstAccessMask		= VK_ACCESS_2_TRANSFER_WRITE_BIT;
							barrier.oldLayout			= VK_IMAGE_LAYOUT_UNDEFINED;
							barrier.newLayout			= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
							barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
							barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
							barrier.image				= *dstImage.image;
							barrier.subresourceRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);

							VkDependencyInfo depInfo = initVulkanStructure();
							depInfo.imageMemoryBarrierCount	    = 1;
							depInfo.pImageMemoryBarriers		= &barrier;

							vk.cmdPipelineBarrier2(*cmdBuf, &depInfo);
						}
						{
							VkBufferImageCopy region {};
							// Use default buffer settings
							region.imageSubresource		= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1);
							region.imageOffset			= makeOffset3D(0, 0, 0);
							region.imageExtent			= makeExtent3D(m_renderArea.extent.width, m_renderArea.extent.height, 1);

							vk.cmdCopyBufferToImage(
								*cmdBuf,
								*srcBuffer.buffer,
								*dstImage.image,
								VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
								1,	// region count
								&region);
						}
						{
							VkImageMemoryBarrier2 barrier = initVulkanStructure();
							barrier.srcStageMask        = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
							barrier.srcAccessMask		= VK_ACCESS_2_TRANSFER_WRITE_BIT;
							barrier.dstStageMask		= dstStageMaskUp;						// beginning of the shader pipeline
							barrier.dstAccessMask		= VK_ACCESS_2_SHADER_READ_BIT;
							barrier.oldLayout			= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
							barrier.newLayout			= dstImage.layout;
							barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
							barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
							barrier.image				= *dstImage.image;
							barrier.subresourceRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);

							VkDependencyInfo depInfo = initVulkanStructure();
							depInfo.imageMemoryBarrierCount	    = 1;
							depInfo.pImageMemoryBarriers		= &barrier;

							vk.cmdPipelineBarrier2(*cmdBuf, &depInfo);
						}
					}
				}
				else if (binding.descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
				{
					for (deUint32 arrayIndex = 0; arrayIndex < binding.descriptorCount; ++arrayIndex)
					{
						ResourceHolder& resource = (**m_resources[binding.perBindingResourceIndex[arrayIndex]]);

						resource.rtBlas->build(*m_deviceInterface, *m_device, *cmdBuf);
						resource.rtTlas->build(*m_deviceInterface, *m_device, *cmdBuf);
					}
				}
			}
		}

		if (m_params.isCompute())
		{
			vk.cmdDispatch(*cmdBuf, 1, 1, 1);

			{
				auto& resultBuffer = getResultBuffer();

				VkBufferMemoryBarrier2 barrier = initVulkanStructure();
				barrier.srcStageMask        = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
				barrier.srcAccessMask		= VK_ACCESS_2_SHADER_WRITE_BIT;
				barrier.dstStageMask		= VK_PIPELINE_STAGE_2_HOST_BIT;
				barrier.dstAccessMask		= VK_ACCESS_2_HOST_READ_BIT;
				barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.buffer				= *resultBuffer.buffer;
				barrier.offset				= 0;
				barrier.size				= VK_WHOLE_SIZE;

				VkDependencyInfo depInfo = initVulkanStructure();
				depInfo.bufferMemoryBarrierCount	= 1;
				depInfo.pBufferMemoryBarriers		= &barrier;

				vk.cmdPipelineBarrier2(*cmdBuf, &depInfo);
			}
		}
		else if (m_params.isRayTracing())
		{
			cmdTraceRays(vk,
				*cmdBuf,
				&m_raygenShaderBindingTableRegion,
				&m_missShaderBindingTableRegion,
				&m_hitShaderBindingTableRegion,
				&m_callableShaderBindingTableRegion,
				1, 1, 1);

			{
				auto& resultBuffer = getResultBuffer();

				VkBufferMemoryBarrier2 barrier = initVulkanStructure();
				barrier.srcStageMask		= VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
				barrier.srcAccessMask		= VK_ACCESS_2_SHADER_WRITE_BIT;
				barrier.dstStageMask		= VK_PIPELINE_STAGE_2_HOST_BIT;
				barrier.dstAccessMask		= VK_ACCESS_2_HOST_READ_BIT;
				barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.buffer				= *resultBuffer.buffer;
				barrier.offset				= 0;
				barrier.size				= VK_WHOLE_SIZE;

				VkDependencyInfo depInfo = initVulkanStructure();
				depInfo.bufferMemoryBarrierCount	= 1;
				depInfo.pBufferMemoryBarriers		= &barrier;

				vk.cmdPipelineBarrier2(*cmdBuf, &depInfo);
			}
		}
		else
		{
			beginRenderPass(vk, *cmdBuf, *m_renderPass, *m_framebuffer, m_renderArea, tcu::Vec4());

			vk.cmdDraw(*cmdBuf, 6, 1, 0, 0);

			endRenderPass(vk, *cmdBuf);

			// Copy the rendered image to a host-visible buffer.

			{
				VkImageMemoryBarrier2 barrier = initVulkanStructure();
				barrier.srcStageMask        = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
				barrier.srcAccessMask		= VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
				barrier.dstStageMask		= VK_PIPELINE_STAGE_2_TRANSFER_BIT;
				barrier.dstAccessMask		= VK_ACCESS_2_TRANSFER_READ_BIT;
				barrier.oldLayout			= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				barrier.newLayout			= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.image				= *m_colorImage.image;
				barrier.subresourceRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);

				VkDependencyInfo depInfo = initVulkanStructure();
				depInfo.imageMemoryBarrierCount	    = 1;
				depInfo.pImageMemoryBarriers		= &barrier;

				vk.cmdPipelineBarrier2(*cmdBuf, &depInfo);
			}
			{
				VkBufferImageCopy region {};
				// Use default buffer settings
				region.imageSubresource		= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1);
				region.imageOffset			= makeOffset3D(0, 0, 0);
				region.imageExtent			= m_colorImage.info.extent;

				vk.cmdCopyImageToBuffer(
					*cmdBuf,
					*m_colorImage.image,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					*m_colorBuffer.buffer,
					1,	// region count
					&region);
			}
			{
				VkBufferMemoryBarrier2 barrier = initVulkanStructure();
				barrier.srcStageMask        = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
				barrier.srcAccessMask		= VK_ACCESS_2_TRANSFER_WRITE_BIT;
				barrier.dstStageMask		= VK_PIPELINE_STAGE_2_HOST_BIT;
				barrier.dstAccessMask		= VK_ACCESS_2_HOST_READ_BIT;
				barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.buffer				= *m_colorBuffer.buffer;
				barrier.offset				= 0;
				barrier.size				= VK_WHOLE_SIZE;

				VkDependencyInfo depInfo = initVulkanStructure();
				depInfo.bufferMemoryBarrierCount	= 1;
				depInfo.pBufferMemoryBarriers		= &barrier;

				vk.cmdPipelineBarrier2(*cmdBuf, &depInfo);
			}
		}

		endCommandBuffer(vk, *cmdBuf);
		submitCommandsAndWait(vk, *m_device, m_queue, *cmdBuf);
	}

	// Verification
	{
		const tcu::UVec4* pResultData = nullptr;

		if (m_params.isCompute() || m_params.isRayTracing())
		{
			auto& resultBuffer = getResultBuffer();

			invalidateAlloc(vk, *m_device, *resultBuffer.alloc);

			pResultData = static_cast<const tcu::UVec4*>(resultBuffer.alloc->getHostPtr());
		}
		else
		{
			pResultData = static_cast<const tcu::UVec4*>(m_colorBuffer.alloc->getHostPtr());
		}

		const auto	actual	 = pResultData->x();
		deUint32	expected = 0;

		for (const auto& sb : m_simpleBindings)
		{
			if (!(sb.isResultBuffer || sb.isRayTracingAS))
			{
				if (m_params.variant == TestVariant::MAX)
				{
					// We test enough (image, sampler) pairs to access each one at least once.
					expected = deMaxu32(m_params.samplerBufferBindingCount, m_params.resourceBufferBindingCount);
				}
				else
				{
					// Uniform blocks/buffers check 4 elements per iteration.
					if (sb.type == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
					{
						expected += ConstChecksPerBuffer * 4;
					}
					else if (sb.type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
					{
						expected += ConstChecksPerBuffer * 4 * sb.count;
					}
					else if ((sb.type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) ||
							 (sb.type == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER) ||
							 (sb.type == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER))
					{
						expected += ConstChecksPerBuffer * sb.count;
					}
					// Samplers are tested implicitly via sampled images
					else if (sb.type != VK_DESCRIPTOR_TYPE_SAMPLER)
					{
						expected += sb.count;
					}
				}
			}
		}

		if (actual != expected)
		{
			deUint32 badSet			= 0;
			deUint32 badBinding		= 0;
			deUint32 badArrayIndex	= 0;

			unpackBindingArgs(pResultData->y(), &badSet, &badBinding, &badArrayIndex);

			std::ostringstream msg;
			msg << "Wrong value in result buffer. Expected (" << expected << ") but got (" << actual << ").";
			msg << " The first wrong binding is (set = " << badSet << ", binding = " << badBinding << ")";

			if (m_params.variant == TestVariant::MAX)
			{
				deUint32 badSamplerSet		= 0;
				deUint32 badSamplerBinding	= 0;

				unpackBindingArgs(pResultData->z(), &badSamplerSet, &badSamplerBinding, nullptr);

				msg << " which used a sampler (set = " << badSamplerSet << ", binding = " << badSamplerBinding << ")";
			}
			else if (badArrayIndex > 0)
			{
				msg << " at array index " << badArrayIndex;
			}

			msg << ".";

			return tcu::TestStatus::fail(msg.str());
		}
	}

	if ((m_params.variant == TestVariant::CAPTURE_REPLAY) && (m_testIteration == 0))
	{
		// The first pass succeeded, continue to the next one where we verify replay.
		++m_testIteration;

		return tcu::TestStatus::incomplete();
	}

	return tcu::TestStatus::pass("Pass");
}

TestInstance* DescriptorBufferTestCase::createInstance (Context& context) const
{
	// Currently all tests follow the same basic execution logic.
	return new DescriptorBufferTestInstance(context, m_params, m_simpleBindings);
}

// This simple tests verifies extension properties against the spec limits.
//
tcu::TestStatus testLimits(Context& context)
{
#define CHECK_MIN_LIMIT(_struct_, _field_, _limit_) \
	if (_struct_._field_ < _limit_) { TCU_THROW(TestError, #_field_ " is less than " #_limit_); }

// Max implicitly checks nonzero too
#define CHECK_MAX_LIMIT_NON_ZERO(_struct_, _field_, _limit_) \
	if (_struct_._field_ == 0)      { TCU_THROW(TestError, #_field_ " is 0"); } \
	if (_struct_._field_ > _limit_) { TCU_THROW(TestError, #_field_ " is greater than " #_limit_); }

#define CHECK_MAX_LIMIT(_struct_, _field_, _limit_) \
	if (_struct_._field_ > _limit_) { TCU_THROW(TestError, #_field_ " is greater than " #_limit_); }

	if (context.isDeviceFunctionalitySupported("VK_EXT_descriptor_buffer"))
	{
		const auto&		features					= *findStructure<VkPhysicalDeviceDescriptorBufferFeaturesEXT>(&context.getDeviceFeatures2());
		const auto&		props						= *findStructure<VkPhysicalDeviceDescriptorBufferPropertiesEXT>(&context.getDeviceProperties2());
		const bool		hasRT						= context.isDeviceFunctionalitySupported("VK_KHR_ray_tracing_pipeline") ||
													  context.isDeviceFunctionalitySupported("VK_KHR_ray_query") ;
		const size_t	maxResourceDescriptorSize	= std::max(props.storageImageDescriptorSize,
													  std::max(props.sampledImageDescriptorSize,
													  std::max(props.robustUniformTexelBufferDescriptorSize,
													  std::max(props.robustStorageTexelBufferDescriptorSize,
													  std::max(props.robustUniformBufferDescriptorSize,
													  std::max(props.robustStorageBufferDescriptorSize,
													  std::max(props.inputAttachmentDescriptorSize,
													  std::max(props.accelerationStructureDescriptorSize, size_t(0u)))))))));

		DE_ASSERT(features.descriptorBuffer == VK_TRUE);

		// Must be queried directly from the physical device, the structure cached in the context has robustness disabled.
		VkPhysicalDeviceFeatures physDeviceFeatures {};
		context.getInstanceInterface().getPhysicalDeviceFeatures(context.getPhysicalDevice(), &physDeviceFeatures);

		if (physDeviceFeatures.robustBufferAccess)
		{
			CHECK_MAX_LIMIT(props, robustUniformTexelBufferDescriptorSize,	256);
			CHECK_MAX_LIMIT(props, robustStorageTexelBufferDescriptorSize,	256);
			CHECK_MAX_LIMIT(props, robustUniformBufferDescriptorSize,		256);
			CHECK_MAX_LIMIT(props, robustStorageBufferDescriptorSize,		256);
		}

		if (features.descriptorBufferCaptureReplay)
		{
			CHECK_MAX_LIMIT_NON_ZERO(props, bufferCaptureReplayDescriptorDataSize,		64);
			CHECK_MAX_LIMIT_NON_ZERO(props, imageCaptureReplayDescriptorDataSize,		64);
			CHECK_MAX_LIMIT_NON_ZERO(props, imageViewCaptureReplayDescriptorDataSize,	64);
			CHECK_MAX_LIMIT_NON_ZERO(props, samplerCaptureReplayDescriptorDataSize,		64);

			if (hasRT)
			{
				CHECK_MAX_LIMIT_NON_ZERO(props, accelerationStructureCaptureReplayDescriptorDataSize,	64);
			}
		}

		if (hasRT)
		{
			CHECK_MAX_LIMIT_NON_ZERO(props, accelerationStructureDescriptorSize,	256);
		}

		CHECK_MAX_LIMIT_NON_ZERO(props, descriptorBufferOffsetAlignment,	256);

		CHECK_MIN_LIMIT(props, maxDescriptorBufferBindings,				3);
		CHECK_MIN_LIMIT(props, maxResourceDescriptorBufferBindings,		1);
		CHECK_MIN_LIMIT(props, maxSamplerDescriptorBufferBindings,		1);
		CHECK_MIN_LIMIT(props, maxEmbeddedImmutableSamplerBindings,		1);
		CHECK_MIN_LIMIT(props, maxEmbeddedImmutableSamplers,			2032);

		CHECK_MAX_LIMIT_NON_ZERO(props, samplerDescriptorSize,				256);
		CHECK_MAX_LIMIT_NON_ZERO(props, combinedImageSamplerDescriptorSize,	256);
		CHECK_MAX_LIMIT_NON_ZERO(props, sampledImageDescriptorSize,			256);
		CHECK_MAX_LIMIT_NON_ZERO(props, storageImageDescriptorSize,			256);
		CHECK_MAX_LIMIT_NON_ZERO(props, uniformTexelBufferDescriptorSize,	256);
		CHECK_MAX_LIMIT_NON_ZERO(props, storageTexelBufferDescriptorSize,	256);
		CHECK_MAX_LIMIT_NON_ZERO(props, uniformBufferDescriptorSize,		256);
		CHECK_MAX_LIMIT_NON_ZERO(props, storageBufferDescriptorSize,		256);
		CHECK_MAX_LIMIT(props, inputAttachmentDescriptorSize,				256);

		CHECK_MIN_LIMIT(props, maxSamplerDescriptorBufferRange,				((1u << 11) * props.samplerDescriptorSize));
		CHECK_MIN_LIMIT(props, maxResourceDescriptorBufferRange,			(((1u << 20) - (1u << 15)) * maxResourceDescriptorSize));
		CHECK_MIN_LIMIT(props, samplerDescriptorBufferAddressSpaceSize,		(1u << 27));
		CHECK_MIN_LIMIT(props, resourceDescriptorBufferAddressSpaceSize,	(1u << 27));
		CHECK_MIN_LIMIT(props, descriptorBufferAddressSpaceSize,			(1u << 27));

		// The following requirement ensures that for split combined image sampler arrays:
		// - there's no unnecessary padding at the end, or
		// - there's no risk of overrun (if somehow the sum of image and sampler was greater).

		if ((props.combinedImageSamplerDescriptorSingleArray == VK_FALSE) &&
			((props.sampledImageDescriptorSize + props.samplerDescriptorSize) != props.combinedImageSamplerDescriptorSize))
		{
			return tcu::TestStatus::fail("For combinedImageSamplerDescriptorSingleArray, it is expected that the sampled image size "
				"and the sampler size add up to combinedImageSamplerDescriptorSize.");
		}
	}
	else
	{
		TCU_THROW(NotSupportedError, "VK_EXT_descriptor_buffer is not supported");
	}

	return tcu::TestStatus::pass("Pass");

#undef CHECK_MIN_LIMIT
#undef CHECK_MAX_LIMIT
#undef CHECK_MAX_LIMIT_NON_ZERO
}

void populateDescriptorBufferTests (tcu::TestCaseGroup* topGroup)
{
	tcu::TestContext&	testCtx		= topGroup->getTestContext();
	const uint32_t		baseSeed	= static_cast<deUint32>(testCtx.getCommandLine().getBaseSeed());;
	std::string			caseName;

	const VkQueueFlagBits choiceQueues[] {
		VK_QUEUE_GRAPHICS_BIT,
		VK_QUEUE_COMPUTE_BIT,
	};

	const VkShaderStageFlagBits choiceStages[] {
		VK_SHADER_STAGE_VERTEX_BIT,
		VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
		VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
		VK_SHADER_STAGE_GEOMETRY_BIT,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		VK_SHADER_STAGE_COMPUTE_BIT,
		VK_SHADER_STAGE_RAYGEN_BIT_KHR,
		VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
		VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
		VK_SHADER_STAGE_MISS_BIT_KHR,
		VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
		VK_SHADER_STAGE_CALLABLE_BIT_KHR,
	};

	{
		MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(testCtx, "basic", "Basic tests"));

		addFunctionCase(subGroup.get(), "limits", "Check basic device properties and limits", testLimits);

		topGroup->addChild(subGroup.release());
	}

	{
		//
		// Basic single descriptor cases -- a quick check.
		//
		MovePtr<tcu::TestCaseGroup>	subGroup		(new tcu::TestCaseGroup(testCtx, "single", "Single binding tests"));
		const uint32_t				subGroupHash	= baseSeed ^ deStringHash(subGroup->getName());

		// VK_DESCRIPTOR_TYPE_SAMPLER is tested implicitly by sampled image case.
		// *_BUFFER_DYNAMIC are not allowed with descriptor buffers.
		//
		const VkDescriptorType choiceDescriptors[] {
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
			VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
			VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
			VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK,
			VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
		};

		TestParams params {};
		params.variant				= TestVariant::SINGLE;
		params.subcase				= SubCase::NONE;
		params.bufferBindingCount	= 1;
		params.setsPerBuffer		= 1;
		params.useMaintenance5		= false;

		for (auto pQueue = choiceQueues; pQueue < DE_ARRAY_END(choiceQueues); ++pQueue)
		for (auto pStage = choiceStages; pStage < DE_ARRAY_END(choiceStages); ++pStage)
		for (auto pDescriptor = choiceDescriptors; pDescriptor < DE_ARRAY_END(choiceDescriptors); ++pDescriptor)
		{
			if ((*pQueue == VK_QUEUE_COMPUTE_BIT) && (*pStage != VK_SHADER_STAGE_COMPUTE_BIT))
			{
				// Compute queue can only use compute shaders.
				continue;
			}

			if ((*pDescriptor == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT) && (*pStage != VK_SHADER_STAGE_FRAGMENT_BIT))
			{
				// Subpass loads are only valid in fragment stage.
				continue;
			}

			params.stage		= *pStage;
			params.queue		= *pQueue;
			params.descriptor	= *pDescriptor;

			subGroup->addChild(new DescriptorBufferTestCase(testCtx, getCaseNameUpdateHash(params, subGroupHash), "", params));
		}

		params.stage			= VK_SHADER_STAGE_COMPUTE_BIT;
		params.queue			= VK_QUEUE_COMPUTE_BIT;
		params.descriptor		= VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		params.useMaintenance5	= true;

		subGroup->addChild(new DescriptorBufferTestCase(testCtx, "compute_maintenance5", "", params));
		topGroup->addChild(subGroup.release());
	}

	{
		//
		// More complex cases. Multiple sets and bindings per buffer. Immutable samplers.
		//
		MovePtr<tcu::TestCaseGroup> subGroup		(new tcu::TestCaseGroup(testCtx, "multiple", "Multiple bindings tests"));
		const uint32_t				subGroupHash	= baseSeed ^ deStringHash(subGroup->getName());
		const VkShaderStageFlags	longTestStages	= VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

		const struct {
			deUint32 bufferBindingCount;
			deUint32 setsPerBuffer;
		} caseOptions[] = {
			{  1,  1 },
			{  1,  3 },
			{  2,  4 },
			{  3,  1 },		// 3 buffer bindings is spec minimum
			{  8,  1 },
			{ 16,  1 },
			{ 32,  1 },
		};

		for (auto pQueue = choiceQueues; pQueue < DE_ARRAY_END(choiceQueues); ++pQueue)
		for (auto pStage = choiceStages; pStage < DE_ARRAY_END(choiceStages); ++pStage)
		for (auto pOptions = caseOptions; pOptions < DE_ARRAY_END(caseOptions); ++pOptions)
		{
			if ((*pQueue == VK_QUEUE_COMPUTE_BIT) && (*pStage != VK_SHADER_STAGE_COMPUTE_BIT))
			{
				// Compute queue can only use compute shaders.
				continue;
			}

			if (pOptions->bufferBindingCount >= 16 && ((*pStage) & longTestStages) == 0)
			{
				// Allow long tests for certain stages only, skip on rest stages
				continue;
			}

			TestParams params {};
			params.variant						= TestVariant::MULTIPLE;
			params.subcase						= SubCase::NONE;
			params.stage						= *pStage;
			params.queue						= *pQueue;
			params.bufferBindingCount			= pOptions->bufferBindingCount;
			params.samplerBufferBindingCount	= pOptions->bufferBindingCount;
			params.resourceBufferBindingCount	= pOptions->bufferBindingCount;
			params.setsPerBuffer				= pOptions->setsPerBuffer;
			params.descriptor					= VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR; // Optional, will be tested if supported
			params.useMaintenance5				= false;

			subGroup->addChild(new DescriptorBufferTestCase(testCtx, getCaseNameUpdateHash(params, subGroupHash), "", params));

			if ((pOptions->setsPerBuffer != 1) && (pOptions->bufferBindingCount < 4))
			{
				// For the smaller binding counts add a subcase with immutable samplers.

				params.subcase = SubCase::IMMUTABLE_SAMPLERS;

				subGroup->addChild(new DescriptorBufferTestCase(testCtx, getCaseNameUpdateHash(params, subGroupHash), "", params));
			}
		}

		topGroup->addChild(subGroup.release());
	}

	{
		//
		// These cases exercise buffers of single usage (samplers only and resources only) and tries to use
		// all available buffer bindings.
		//
		MovePtr<tcu::TestCaseGroup>	subGroup		(new tcu::TestCaseGroup(testCtx, "max", "Max sampler/resource bindings tests"));
		const uint32_t				subGroupHash	= baseSeed ^ deStringHash(subGroup->getName());

		const struct {
			deUint32 samplerBufferBindingCount;
			deUint32 resourceBufferBindingCount;
		} caseOptions[] = {
			{  1,   1 },
			{  2,   2 },
			{  4,   4 },
			{  8,   8 },
			{ 16,  16 },
			{  1,   7 },
			{  1,  15 },
			{  1,  31 },
			{  7,   1 },
			{ 15,   1 },
			{ 31,   1 },
		};

		for (auto pQueue = choiceQueues; pQueue < DE_ARRAY_END(choiceQueues); ++pQueue)
		for (auto pStage = choiceStages; pStage < DE_ARRAY_END(choiceStages); ++pStage)
		for (auto pOptions = caseOptions; pOptions < DE_ARRAY_END(caseOptions); ++pOptions)
		{
			if ((*pQueue == VK_QUEUE_COMPUTE_BIT) && (*pStage != VK_SHADER_STAGE_COMPUTE_BIT))
			{
				// Compute queue can only use compute shaders.
				continue;
			}

			if (isAllRayTracingStages(*pStage) && (pOptions->samplerBufferBindingCount > 15 || pOptions->resourceBufferBindingCount > 15))
			{
				// Limit ray tracing stages
				continue;
			}

			TestParams params {};
			params.variant					  = TestVariant::MAX;
			params.subcase					  = SubCase::NONE;
			params.stage					  = *pStage;
			params.queue					  = *pQueue;
			params.samplerBufferBindingCount  = pOptions->samplerBufferBindingCount;
			params.resourceBufferBindingCount = pOptions->resourceBufferBindingCount;
			params.bufferBindingCount		  = pOptions->samplerBufferBindingCount + pOptions->resourceBufferBindingCount;
			params.setsPerBuffer			  = 1;
			params.descriptor				  = VK_DESCRIPTOR_TYPE_MAX_ENUM;
			params.useMaintenance5			  = false;

			subGroup->addChild(new DescriptorBufferTestCase(testCtx, getCaseNameUpdateHash(params, subGroupHash), "", params));
		}

		topGroup->addChild(subGroup.release());
	}

	{
		//
		// Check embedded immutable sampler buffers/bindings.
		//
		MovePtr<tcu::TestCaseGroup>	subGroup		(new tcu::TestCaseGroup(testCtx, "embedded_imm_samplers", "Max embedded immutable samplers tests"));
		const uint32_t				subGroupHash	= baseSeed ^ deStringHash(subGroup->getName());

		const struct {
			deUint32 bufferBindingCount;
			deUint32 samplersPerBuffer;
		} caseOptions[] = {
			{  1,  1 },
			{  1,  2 },
			{  1,  4 },
			{  1,  8 },
			{  1, 16 },
			{  2,  1 },
			{  2,  2 },
			{  3,  1 },
			{  3,  3 },
			{  8,  1 },
			{  8,  4 },
		};

		for (auto pQueue = choiceQueues; pQueue < DE_ARRAY_END(choiceQueues); ++pQueue)
		for (auto pStage = choiceStages; pStage < DE_ARRAY_END(choiceStages); ++pStage)
		for (auto pOptions = caseOptions; pOptions < DE_ARRAY_END(caseOptions); ++pOptions)
		{
			if ((*pQueue == VK_QUEUE_COMPUTE_BIT) && (*pStage != VK_SHADER_STAGE_COMPUTE_BIT))
			{
				// Compute queue can only use compute shaders.
				continue;
			}

			TestParams params {};
			params.variant										= TestVariant::EMBEDDED_IMMUTABLE_SAMPLERS;
			params.subcase										= SubCase::NONE;
			params.stage										= *pStage;
			params.queue										= *pQueue;
			params.bufferBindingCount							= pOptions->bufferBindingCount + 1;
			params.samplerBufferBindingCount					= pOptions->bufferBindingCount;
			params.resourceBufferBindingCount					= 1;
			params.setsPerBuffer								= 1;
			params.embeddedImmutableSamplerBufferBindingCount	= pOptions->bufferBindingCount;
			params.embeddedImmutableSamplersPerBuffer			= pOptions->samplersPerBuffer;
			params.descriptor									= VK_DESCRIPTOR_TYPE_MAX_ENUM;
			params.useMaintenance5								= false;

			subGroup->addChild(new DescriptorBufferTestCase(testCtx, getCaseNameUpdateHash(params, subGroupHash), "", params));
		}

		topGroup->addChild(subGroup.release());
	}

	{
		//
		// Check push descriptors and push descriptors with template updates
		//
		MovePtr<tcu::TestCaseGroup> subGroupPush				(new tcu::TestCaseGroup(testCtx, "push_descriptor", "Use push descriptors in addition to descriptor buffer"));
		MovePtr<tcu::TestCaseGroup> subGroupPushTemplate		(new tcu::TestCaseGroup(testCtx, "push_template", "Use descriptor update template with push descriptors in addition to descriptor buffer"));
		const uint32_t				subGroupPushHash			= baseSeed ^ deStringHash(subGroupPush->getName());
		const uint32_t				subGroupPushTemplateHash	= baseSeed ^ deStringHash(subGroupPushTemplate->getName());

		const struct {
			deUint32 pushDescriptorSetIndex;
			deUint32 bufferBindingCount;

			// The total number of descriptor sets will be bufferBindingCount + 1, where the additional set is used for push descriptors.

		} caseOptions[] = {
			{  0,  0 },		// Only push descriptors
			{  0,  1 },
			{  0,  3 },
			{  1,  1 },
			{  0,  2 },
			{  1,  2 },
			{  2,  2 },		// index = 2 means 3 sets, where the first two are used with descriptor buffer and the last with push descriptors
			{  3,  3 },
		};

		for (auto pQueue = choiceQueues; pQueue < DE_ARRAY_END(choiceQueues); ++pQueue)
		for (auto pStage = choiceStages; pStage < DE_ARRAY_END(choiceStages); ++pStage)
		for (auto pOptions = caseOptions; pOptions < DE_ARRAY_END(caseOptions); ++pOptions)
		{
			if ((*pQueue == VK_QUEUE_COMPUTE_BIT) && (*pStage != VK_SHADER_STAGE_COMPUTE_BIT))
			{
				// Compute queue can only use compute shaders.
				continue;
			}

			TestParams params {};
			params.variant						= TestVariant::PUSH_DESCRIPTOR;
			params.subcase						= SubCase::NONE;
			params.stage						= *pStage;
			params.queue						= *pQueue;
			params.bufferBindingCount			= pOptions->bufferBindingCount;
			params.samplerBufferBindingCount	= pOptions->bufferBindingCount;
			params.resourceBufferBindingCount	= pOptions->bufferBindingCount;
			params.setsPerBuffer				= 1;
			params.pushDescriptorSetIndex		= pOptions->pushDescriptorSetIndex;
			params.descriptor					= VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR; // Optional, will be tested if supported
			params.useMaintenance5				= false;

			subGroupPush->addChild(new DescriptorBufferTestCase(testCtx, getCaseNameUpdateHash(params, subGroupPushHash), "", params));

			if (pOptions->bufferBindingCount < 2)
			{
				TestParams paramsSingleBuffer = params;

				paramsSingleBuffer.subcase = SubCase::SINGLE_BUFFER;

				subGroupPush->addChild(new DescriptorBufferTestCase(testCtx, getCaseNameUpdateHash(paramsSingleBuffer, subGroupPushHash), "", paramsSingleBuffer));
			}

			params.variant = TestVariant::PUSH_TEMPLATE;

			subGroupPushTemplate->addChild(new DescriptorBufferTestCase(testCtx, getCaseNameUpdateHash(params, subGroupPushTemplateHash), "", params));
		}

		topGroup->addChild(subGroupPush.release());
		topGroup->addChild(subGroupPushTemplate.release());
	}

	{
		//
		// Robustness tests
		//
		MovePtr<tcu::TestCaseGroup> subGroup					(new tcu::TestCaseGroup(testCtx, "robust", "Robustness tests"));
		MovePtr<tcu::TestCaseGroup> subGroupBuffer				(new tcu::TestCaseGroup(testCtx, "buffer_access", "Robust buffer access"));
		MovePtr<tcu::TestCaseGroup> subGroupNullDescriptor		(new tcu::TestCaseGroup(testCtx, "null_descriptor", "Null descriptor"));
		const uint32_t				subGroupBufferHash			= baseSeed ^ deStringHash(subGroupBuffer->getName());
		const uint32_t				subGroupNullDescriptorHash	= baseSeed ^ deStringHash(subGroupNullDescriptor->getName());

		// Robust buffer access:
		// This test will fill the buffers with zeros and always expect to read zero values back (in and out of bounds).

		// Null descriptor cases:
		// For each test, one of these descriptors will have its buffer/imageView/etc. set to null handle.
		// Reads done through a null descriptor are expected to return zeros.
		//
		const VkDescriptorType choiceNullDescriptors[] {
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
			VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
			VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
		};

		for (auto pQueue = choiceQueues; pQueue < DE_ARRAY_END(choiceQueues); ++pQueue)
		for (auto pStage = choiceStages; pStage < DE_ARRAY_END(choiceStages); ++pStage)
		{
			if ((*pQueue == VK_QUEUE_COMPUTE_BIT) && (*pStage != VK_SHADER_STAGE_COMPUTE_BIT))
			{
				// Compute queue can only use compute shaders.
				continue;
			}

			TestParams params {};
			params.variant				= TestVariant::ROBUST_BUFFER_ACCESS;
			params.stage				= *pStage;
			params.queue				= *pQueue;
			params.bufferBindingCount	= 1;
			params.setsPerBuffer		= 1;
			params.useMaintenance5		= false;

			subGroupBuffer->addChild(new DescriptorBufferTestCase(testCtx, getCaseNameUpdateHash(params, subGroupBufferHash), "", params));

			for (auto pDescriptor = choiceNullDescriptors; pDescriptor < DE_ARRAY_END(choiceNullDescriptors); ++pDescriptor)
			{
				if ((*pDescriptor == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT) && (*pStage != VK_SHADER_STAGE_FRAGMENT_BIT))
				{
					// Subpass loads are only valid in fragment stage.
					continue;
				}

				params.variant		= TestVariant::ROBUST_NULL_DESCRIPTOR;
				params.descriptor	= *pDescriptor;

				subGroupNullDescriptor->addChild(new DescriptorBufferTestCase(testCtx, getCaseNameUpdateHash(params, subGroupNullDescriptorHash), "", params));
			}
		}

		subGroup->addChild(subGroupBuffer.release());
		subGroup->addChild(subGroupNullDescriptor.release());
		topGroup->addChild(subGroup.release());
	}

	{
		//
		// Capture and replay
		//
		MovePtr<tcu::TestCaseGroup>	subGroup		(new tcu::TestCaseGroup(testCtx, "capture_replay", "Capture and replay tests"));
		const uint32_t				subGroupHash	= baseSeed ^ deStringHash(subGroup->getName());

		const VkDescriptorType choiceDescriptors[] {
			VK_DESCRIPTOR_TYPE_SAMPLER,
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,	// both sampler and image are captured
			VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
			VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
			VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
			VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
		};

		for (auto pQueue = choiceQueues; pQueue < DE_ARRAY_END(choiceQueues); ++pQueue)
		for (auto pStage = choiceStages; pStage < DE_ARRAY_END(choiceStages); ++pStage)
		for (auto pDescriptor = choiceDescriptors; pDescriptor < DE_ARRAY_END(choiceDescriptors); ++pDescriptor)
		{
			if ((*pQueue == VK_QUEUE_COMPUTE_BIT) && (*pStage != VK_SHADER_STAGE_COMPUTE_BIT))
			{
				// Compute queue can only use compute shaders.
				continue;
			}

			if ((*pDescriptor == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT) && (*pStage != VK_SHADER_STAGE_FRAGMENT_BIT))
			{
				// Subpass loads are only valid in fragment stage.
				continue;
			}

			TestParams params {};
			params.variant				= TestVariant::CAPTURE_REPLAY;
			params.subcase				= SubCase::NONE;
			params.stage				= *pStage;
			params.queue				= *pQueue;
			params.descriptor			= *pDescriptor;
			params.bufferBindingCount	= 1;
			params.setsPerBuffer		= 1;
			params.useMaintenance5		= false;

			subGroup->addChild(new DescriptorBufferTestCase(testCtx, getCaseNameUpdateHash(params, subGroupHash), "", params));

			if ((*pDescriptor == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) ||
				(*pDescriptor == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) ||
				(*pDescriptor == VK_DESCRIPTOR_TYPE_SAMPLER))
			{
				params.subcase = SubCase::CAPTURE_REPLAY_CUSTOM_BORDER_COLOR;

				subGroup->addChild(new DescriptorBufferTestCase(testCtx, getCaseNameUpdateHash(params, subGroupHash), "", params));
			}
		}

		topGroup->addChild(subGroup.release());
	}

}

} // anonymous

tcu::TestCaseGroup* createDescriptorBufferTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "descriptor_buffer", "Descriptor buffer tests.", populateDescriptorBufferTests);
}

} // BindingModel
} // vkt
