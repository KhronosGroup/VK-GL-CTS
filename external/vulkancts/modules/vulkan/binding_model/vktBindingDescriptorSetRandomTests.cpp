/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 The Khronos Group Inc.
 * Copyright (c) 2018 NVIDIA Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Vulkan descriptor set tests
 *//*--------------------------------------------------------------------*/

// These tests generate random descriptor set layouts, where each descriptor
// set has a random number of bindings, each binding has a random array size
// and random descriptor type. The descriptor types are all backed by buffers
// or buffer views, and each buffer is filled with a unique integer starting
// from zero. The shader fetches from each descriptor (possibly using dynamic
// indexing of the descriptor array) and compares against the expected value.
//
// The different test cases vary the maximum number of descriptors used of
// each type. "Low" limit tests use the spec minimum maximum limit, "high"
// limit tests use up to 4k descriptors of the corresponding type. Test cases
// also vary the type indexing used, and shader stage.

#include "vktBindingDescriptorSetRandomTests.hpp"

#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkQueryUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkRayTracingUtil.hpp"

#include "vktTestGroupUtil.hpp"
#include "vktTestCase.hpp"

#include "deDefs.h"
#include "deMath.h"
#include "deRandom.h"
#include "deSharedPtr.hpp"
#include "deString.h"

#include "tcuTestCase.hpp"
#include "tcuTestLog.hpp"

#include <string>
#include <sstream>
#include <algorithm>
#include <map>
#include <utility>
#include <memory>

namespace vkt
{
namespace BindingModel
{
namespace
{
using namespace vk;
using namespace std;

static const deUint32 DIM = 8;

static const VkFlags	ALL_RAY_TRACING_STAGES	= VK_SHADER_STAGE_RAYGEN_BIT_KHR
												| VK_SHADER_STAGE_ANY_HIT_BIT_KHR
												| VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
												| VK_SHADER_STAGE_MISS_BIT_KHR
												| VK_SHADER_STAGE_INTERSECTION_BIT_KHR
												| VK_SHADER_STAGE_CALLABLE_BIT_KHR;

typedef enum
{
	INDEX_TYPE_NONE = 0,
	INDEX_TYPE_CONSTANT,
	INDEX_TYPE_PUSHCONSTANT,
	INDEX_TYPE_DEPENDENT,
	INDEX_TYPE_RUNTIME_SIZE,
} IndexType;

typedef enum
{
	STAGE_COMPUTE = 0,
	STAGE_VERTEX,
	STAGE_FRAGMENT,
	STAGE_RAYGEN_NV,
	STAGE_RAYGEN,
	STAGE_INTERSECT,
	STAGE_ANY_HIT,
	STAGE_CLOSEST_HIT,
	STAGE_MISS,
	STAGE_CALLABLE,
} Stage;

typedef enum
{
	UPDATE_AFTER_BIND_DISABLED = 0,
	UPDATE_AFTER_BIND_ENABLED,
} UpdateAfterBind;

struct DescriptorId
{
	DescriptorId (deUint32 set_, deUint32 binding_, deUint32 number_)
		: set(set_), binding(binding_), number(number_)
		{}

	bool operator< (const DescriptorId& other) const
	{
		return (set < other.set || (set == other.set && (binding < other.binding || (binding == other.binding && number < other.number))));
	}

	deUint32 set;
	deUint32 binding;
	deUint32 number;
};

struct WriteInfo
{
	WriteInfo () : ptr(nullptr), expected(0u), writeGenerated(false) {}

	deInt32*	ptr;
	deInt32		expected;
	bool		writeGenerated;
};

bool isRayTracingStageKHR (const Stage stage)
{
	switch (stage)
	{
		case STAGE_COMPUTE:
		case STAGE_VERTEX:
		case STAGE_FRAGMENT:
		case STAGE_RAYGEN_NV:
			return false;

		case STAGE_RAYGEN:
		case STAGE_INTERSECT:
		case STAGE_ANY_HIT:
		case STAGE_CLOSEST_HIT:
		case STAGE_MISS:
		case STAGE_CALLABLE:
			return true;

		default: TCU_THROW(InternalError, "Unknown stage specified");
	}
}

VkShaderStageFlagBits getShaderStageFlag (const Stage stage)
{
	switch (stage)
	{
		case STAGE_RAYGEN:		return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
		case STAGE_ANY_HIT:		return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
		case STAGE_CLOSEST_HIT:	return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
		case STAGE_MISS:		return VK_SHADER_STAGE_MISS_BIT_KHR;
		case STAGE_INTERSECT:	return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
		case STAGE_CALLABLE:	return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
		default: TCU_THROW(InternalError, "Unknown stage specified");
	}
}

bool usesAccelerationStructure (const Stage stage)
{
	return (isRayTracingStageKHR(stage) && stage != STAGE_RAYGEN && stage != STAGE_CALLABLE);
}

class RandomLayout
{
public:
	RandomLayout(deUint32 numSets) :
		layoutBindings(numSets),
		layoutBindingFlags(numSets),
		arraySizes(numSets),
		variableDescriptorSizes(numSets)
		{
		}

	// These three are indexed by [set][binding]
	vector<vector<VkDescriptorSetLayoutBinding> > layoutBindings;
	vector<vector<VkDescriptorBindingFlags> > layoutBindingFlags;
	vector<vector<deUint32> > arraySizes;
	// size of the variable descriptor (last) binding in each set
	vector<deUint32> variableDescriptorSizes;

	// List of descriptors that will write the descriptor value instead of reading it.
	map<DescriptorId, WriteInfo> descriptorWrites;

};

struct CaseDef
{
	IndexType						indexType;
	deUint32						numDescriptorSets;
	deUint32						maxPerStageUniformBuffers;
	deUint32						maxUniformBuffersDynamic;
	deUint32						maxPerStageStorageBuffers;
	deUint32						maxStorageBuffersDynamic;
	deUint32						maxPerStageSampledImages;
	deUint32						maxPerStageStorageImages;
	deUint32						maxPerStageStorageTexelBuffers;
	deUint32						maxInlineUniformBlocks;
	deUint32						maxInlineUniformBlockSize;
	deUint32						maxPerStageInputAttachments;
	Stage							stage;
	UpdateAfterBind					uab;
	deUint32						seed;
	VkFlags							allShaderStages;
	VkFlags							allPipelineStages;
	// Shared by the test case and the test instance.
	std::shared_ptr<RandomLayout>	randomLayout;
};


class DescriptorSetRandomTestInstance : public TestInstance
{
public:
								DescriptorSetRandomTestInstance		(Context& context, const std::shared_ptr<CaseDef>& data);
								~DescriptorSetRandomTestInstance	(void);
	tcu::TestStatus				iterate								(void);
private:
	// Shared pointer because the test case and the test instance need to share the random layout information. Specifically, the
	// descriptorWrites map, which is filled from the test case and used by the test instance.
	std::shared_ptr<CaseDef>	m_data_ptr;
	CaseDef&					m_data;
};

DescriptorSetRandomTestInstance::DescriptorSetRandomTestInstance (Context& context, const std::shared_ptr<CaseDef>& data)
	: vkt::TestInstance		(context)
	, m_data_ptr			(data)
	, m_data				(*m_data_ptr.get())
{
}

DescriptorSetRandomTestInstance::~DescriptorSetRandomTestInstance (void)
{
}

class DescriptorSetRandomTestCase : public TestCase
{
	public:
								DescriptorSetRandomTestCase		(tcu::TestContext& context, const char* name, const char* desc, const CaseDef& data);
								~DescriptorSetRandomTestCase	(void);
	virtual	void				initPrograms					(SourceCollections& programCollection) const;
	virtual TestInstance*		createInstance					(Context& context) const;
	virtual void				checkSupport					(Context& context) const;

private:
	// See DescriptorSetRandomTestInstance about the need for a shared pointer here.
	std::shared_ptr<CaseDef>	m_data_ptr;
	CaseDef&					m_data;
};

DescriptorSetRandomTestCase::DescriptorSetRandomTestCase (tcu::TestContext& context, const char* name, const char* desc, const CaseDef& data)
	: vkt::TestCase	(context, name, desc)
	, m_data_ptr	(std::make_shared<CaseDef>(data))
	, m_data		(*reinterpret_cast<CaseDef*>(m_data_ptr.get()))
{
}

DescriptorSetRandomTestCase::~DescriptorSetRandomTestCase	(void)
{
}

void DescriptorSetRandomTestCase::checkSupport(Context& context) const
{
	// Get needed properties.
	VkPhysicalDeviceInlineUniformBlockPropertiesEXT inlineUniformProperties;
	deMemset(&inlineUniformProperties, 0, sizeof(inlineUniformProperties));
	inlineUniformProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_PROPERTIES_EXT;

	VkPhysicalDeviceProperties2 properties;
	deMemset(&properties, 0, sizeof(properties));
	properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	void** pNextTail = &properties.pNext;

	if (context.isDeviceFunctionalitySupported("VK_EXT_inline_uniform_block"))
	{
		*pNextTail = &inlineUniformProperties;
		pNextTail = &inlineUniformProperties.pNext;
	}
	*pNextTail = NULL;

	context.getInstanceInterface().getPhysicalDeviceProperties2(context.getPhysicalDevice(), &properties);

	// Get needed features.
	auto features				= context.getDeviceFeatures2();
	auto indexingFeatures		= context.getDescriptorIndexingFeatures();
	auto inlineUniformFeatures	= context.getInlineUniformBlockFeaturesEXT();

	// Check needed properties and features
	if (m_data.stage == STAGE_VERTEX && !features.features.vertexPipelineStoresAndAtomics)
	{
		TCU_THROW(NotSupportedError, "Vertex pipeline stores and atomics not supported");
	}
	else if (m_data.stage == STAGE_RAYGEN_NV)
	{
		context.requireDeviceFunctionality("VK_NV_ray_tracing");
	}
	else if (isRayTracingStageKHR(m_data.stage))
	{
		context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
		context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");

		const VkPhysicalDeviceRayTracingPipelineFeaturesKHR&	rayTracingPipelineFeaturesKHR = context.getRayTracingPipelineFeatures();
		if (rayTracingPipelineFeaturesKHR.rayTracingPipeline == DE_FALSE)
			TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipeline");

		const VkPhysicalDeviceAccelerationStructureFeaturesKHR&	accelerationStructureFeaturesKHR = context.getAccelerationStructureFeatures();
		if (accelerationStructureFeaturesKHR.accelerationStructure == DE_FALSE)
			TCU_THROW(TestError, "VK_KHR_ray_tracing_pipeline requires VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure");
	}

	// Note binding 0 in set 0 is the output storage image, always present and not subject to dynamic indexing.
	if ((m_data.indexType == INDEX_TYPE_PUSHCONSTANT ||
		 m_data.indexType == INDEX_TYPE_DEPENDENT ||
		 m_data.indexType == INDEX_TYPE_RUNTIME_SIZE) &&
		((m_data.maxPerStageUniformBuffers > 0u && !features.features.shaderUniformBufferArrayDynamicIndexing) ||
		 (m_data.maxPerStageStorageBuffers > 0u && !features.features.shaderStorageBufferArrayDynamicIndexing) ||
		 (m_data.maxPerStageStorageImages > 1u && !features.features.shaderStorageImageArrayDynamicIndexing) ||
		 (m_data.stage == STAGE_FRAGMENT && m_data.maxPerStageInputAttachments > 0u && (!indexingFeatures.shaderInputAttachmentArrayDynamicIndexing)) ||
		 (m_data.maxPerStageSampledImages > 0u && !indexingFeatures.shaderUniformTexelBufferArrayDynamicIndexing) ||
		 (m_data.maxPerStageStorageTexelBuffers > 0u && !indexingFeatures.shaderStorageTexelBufferArrayDynamicIndexing)))
	{
		TCU_THROW(NotSupportedError, "Dynamic indexing not supported");
	}

	if (m_data.numDescriptorSets > properties.properties.limits.maxBoundDescriptorSets)
	{
		TCU_THROW(NotSupportedError, "Number of descriptor sets not supported");
	}

	if ((m_data.maxPerStageUniformBuffers + m_data.maxPerStageStorageBuffers +
		m_data.maxPerStageSampledImages + m_data.maxPerStageStorageImages +
		m_data.maxPerStageStorageTexelBuffers + m_data.maxPerStageInputAttachments) >
		properties.properties.limits.maxPerStageResources)
	{
		TCU_THROW(NotSupportedError, "Number of descriptors not supported");
	}

	if (m_data.maxPerStageUniformBuffers		> properties.properties.limits.maxPerStageDescriptorUniformBuffers ||
		m_data.maxPerStageStorageBuffers		> properties.properties.limits.maxPerStageDescriptorStorageBuffers ||
		m_data.maxUniformBuffersDynamic			> properties.properties.limits.maxDescriptorSetUniformBuffersDynamic ||
		m_data.maxStorageBuffersDynamic			> properties.properties.limits.maxDescriptorSetStorageBuffersDynamic ||
		m_data.maxPerStageSampledImages			> properties.properties.limits.maxPerStageDescriptorSampledImages ||
		(m_data.maxPerStageStorageImages +
		 m_data.maxPerStageStorageTexelBuffers)	> properties.properties.limits.maxPerStageDescriptorStorageImages ||
		m_data.maxPerStageInputAttachments		> properties.properties.limits.maxPerStageDescriptorInputAttachments)
	{
		TCU_THROW(NotSupportedError, "Number of descriptors not supported");
	}

	if (m_data.maxInlineUniformBlocks != 0 &&
		!inlineUniformFeatures.inlineUniformBlock)
	{
		TCU_THROW(NotSupportedError, "Inline uniform blocks not supported");
	}

	if (m_data.maxInlineUniformBlocks > inlineUniformProperties.maxPerStageDescriptorInlineUniformBlocks)
	{
		TCU_THROW(NotSupportedError, "Number of inline uniform blocks not supported");
	}

	if (m_data.maxInlineUniformBlocks != 0 &&
		m_data.maxInlineUniformBlockSize > inlineUniformProperties.maxInlineUniformBlockSize)
	{
		TCU_THROW(NotSupportedError, "Inline uniform block size not supported");
	}

	if (m_data.indexType == INDEX_TYPE_RUNTIME_SIZE &&
		!indexingFeatures.runtimeDescriptorArray)
	{
		TCU_THROW(NotSupportedError, "runtimeDescriptorArray not supported");
	}
}

// Return a random value in the range [min, max]
deInt32 randRange(deRandom *rnd, deInt32 min, deInt32 max)
{
	if (max < 0)
		return 0;

	return (deRandom_getUint32(rnd) % (max - min + 1)) + min;
}

void chooseWritesRandomly(vk::VkDescriptorType type, RandomLayout& randomLayout, deRandom& rnd, deUint32 set, deUint32 binding, deUint32 count)
{
	// Make sure the type supports writes.
	switch (type)
	{
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
	case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
	case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
		break;
	default:
		DE_ASSERT(false);
		break;
	}

	for (deUint32 i = 0u; i < count; ++i)
	{
		// 1/2 chance of being a write.
		if (randRange(&rnd, 1, 2) == 1)
			randomLayout.descriptorWrites[DescriptorId(set, binding, i)] = {};
	}
}

void generateRandomLayout(RandomLayout& randomLayout, const CaseDef &caseDef, deRandom& rnd)
{
	// Count the number of each resource type, to avoid overflowing the limits.
	deUint32 numUBO = 0;
	deUint32 numUBODyn = 0;
	deUint32 numSSBO = 0;
	deUint32 numSSBODyn = 0;
	deUint32 numImage = 0;
	deUint32 numStorageTex = 0;
	deUint32 numTexBuffer = 0;
	deUint32 numInlineUniformBlocks = 0;
	deUint32 numInputAttachments = 0;

	// TODO: Consider varying these
	deUint32 minBindings = 0;
	// Try to keep the workload roughly constant while exercising higher numbered sets.
	deUint32 maxBindings = 128u / caseDef.numDescriptorSets;
	// No larger than 32 elements for dynamic indexing tests, due to 128B limit
	// for push constants (used for the indices)
	deUint32 maxArray = caseDef.indexType == INDEX_TYPE_NONE ? 0 : 32;

	// Each set has a random number of bindings, each binding has a random
	// array size and a random descriptor type.
	for (deUint32 s = 0; s < caseDef.numDescriptorSets; ++s)
	{
		vector<VkDescriptorSetLayoutBinding> &bindings = randomLayout.layoutBindings[s];
		vector<VkDescriptorBindingFlags> &bindingsFlags = randomLayout.layoutBindingFlags[s];
		vector<deUint32> &arraySizes = randomLayout.arraySizes[s];
		int numBindings = randRange(&rnd, minBindings, maxBindings);

		// Guarantee room for the output image
		if (s == 0 && numBindings == 0)
		{
			numBindings = 1;
		}
		// Guarantee room for the raytracing acceleration structure
		if (s == 0 && numBindings < 2 && usesAccelerationStructure(caseDef.stage))
		{
			numBindings = 2;
		}

		bindings = vector<VkDescriptorSetLayoutBinding>(numBindings);
		bindingsFlags = vector<VkDescriptorBindingFlags>(numBindings);
		arraySizes = vector<deUint32>(numBindings);
	}

	// BUFFER_DYNAMIC descriptor types cannot be used with VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT bindings in one set
	bool allowDynamicBuffers = caseDef.uab != UPDATE_AFTER_BIND_ENABLED;

	// Iterate over bindings first, then over sets. This prevents the low-limit bindings
	// from getting clustered in low-numbered sets.
	for (deUint32 b = 0; b <= maxBindings; ++b)
	{
		for (deUint32 s = 0; s < caseDef.numDescriptorSets; ++s)
		{
			vector<VkDescriptorSetLayoutBinding> &bindings = randomLayout.layoutBindings[s];
			vector<deUint32> &arraySizes = randomLayout.arraySizes[s];

			if (b >= bindings.size())
			{
				continue;
			}

			VkDescriptorSetLayoutBinding &binding = bindings[b];
			binding.binding = b;
			binding.pImmutableSamplers = NULL;
			binding.stageFlags = caseDef.allShaderStages;

			// Output image
			if (s == 0 && b == 0)
			{
				binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				binding.descriptorCount = 1;
				binding.stageFlags = caseDef.allShaderStages;
				numImage++;
				arraySizes[b] = 0;
				continue;
			}

			// Raytracing acceleration structure
			if (s == 0 && b == 1 && usesAccelerationStructure(caseDef.stage))
			{
				binding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
				binding.descriptorCount = 1;
				binding.stageFlags = caseDef.allShaderStages;
				arraySizes[b] = 0;
				continue;
			}

			binding.descriptorCount = 0;

			// Select a random type of descriptor.
			std::map<int, vk::VkDescriptorType> intToType;
			{
				int index = 0;
				intToType[index++] = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				intToType[index++] = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				intToType[index++] = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
				intToType[index++] = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				intToType[index++] = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
				intToType[index++] = VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT;
				if (caseDef.stage == STAGE_FRAGMENT)
				{
					intToType[index++] = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
				}
				if (allowDynamicBuffers)
				{
					intToType[index++] = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
					intToType[index++] = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
				}
			}

			int r = randRange(&rnd, 0, static_cast<int>(intToType.size() - 1));
			DE_ASSERT(r >= 0 && static_cast<size_t>(r) < intToType.size());

			// Add a binding for that descriptor type if possible.
			binding.descriptorType = intToType[r];
			switch (binding.descriptorType)
			{
			default: DE_ASSERT(0); // Fallthrough
			case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
				if (numUBO < caseDef.maxPerStageUniformBuffers)
				{
					arraySizes[b] = randRange(&rnd, 0, de::min(maxArray, caseDef.maxPerStageUniformBuffers - numUBO));
					binding.descriptorCount = arraySizes[b] ? arraySizes[b] : 1;
					numUBO += binding.descriptorCount;
				}
				break;
			case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
				if (numSSBO < caseDef.maxPerStageStorageBuffers)
				{
					arraySizes[b] = randRange(&rnd, 0, de::min(maxArray, caseDef.maxPerStageStorageBuffers - numSSBO));
					binding.descriptorCount = arraySizes[b] ? arraySizes[b] : 1;
					numSSBO += binding.descriptorCount;

					chooseWritesRandomly(binding.descriptorType, randomLayout, rnd, s, b, binding.descriptorCount);
				}
				break;
			case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
				if (numStorageTex < caseDef.maxPerStageStorageTexelBuffers)
				{
					arraySizes[b] = randRange(&rnd, 0, de::min(maxArray, caseDef.maxPerStageStorageTexelBuffers - numStorageTex));
					binding.descriptorCount = arraySizes[b] ? arraySizes[b] : 1;
					numStorageTex += binding.descriptorCount;

					chooseWritesRandomly(binding.descriptorType, randomLayout, rnd, s, b, binding.descriptorCount);
				}
				break;
			case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
				if (numImage < caseDef.maxPerStageStorageImages)
				{
					arraySizes[b] = randRange(&rnd, 0, de::min(maxArray, caseDef.maxPerStageStorageImages - numImage));
					binding.descriptorCount = arraySizes[b] ? arraySizes[b] : 1;
					numImage += binding.descriptorCount;

					chooseWritesRandomly(binding.descriptorType, randomLayout, rnd, s, b, binding.descriptorCount);
				}
				break;
			case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
				if (numTexBuffer < caseDef.maxPerStageSampledImages)
				{
					arraySizes[b] = randRange(&rnd, 0, de::min(maxArray, caseDef.maxPerStageSampledImages - numTexBuffer));
					binding.descriptorCount = arraySizes[b] ? arraySizes[b] : 1;
					numTexBuffer += binding.descriptorCount;
				}
				break;
			case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT:
				if (caseDef.maxInlineUniformBlocks > 0)
				{
					if (numInlineUniformBlocks < caseDef.maxInlineUniformBlocks)
					{
						arraySizes[b] = randRange(&rnd, 1, (caseDef.maxInlineUniformBlockSize - 16) / 16); // subtract 16 for "ivec4 dummy"
						arraySizes[b] = de::min(maxArray, arraySizes[b]);
						binding.descriptorCount = (arraySizes[b] ? arraySizes[b] : 1) * 16 + 16; // add 16 for "ivec4 dummy"
						numInlineUniformBlocks++;
					}
				}
				else
				{
					// Plug in a dummy descriptor type, so validation layers that don't
					// support inline_uniform_block don't crash.
					binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				}
				break;
			case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
				if (numUBODyn < caseDef.maxUniformBuffersDynamic &&
					numUBO < caseDef.maxPerStageUniformBuffers)
				{
					arraySizes[b] = randRange(&rnd, 0, de::min(maxArray, de::min(caseDef.maxUniformBuffersDynamic - numUBODyn,
																				 caseDef.maxPerStageUniformBuffers - numUBO)));
					binding.descriptorCount = arraySizes[b] ? arraySizes[b] : 1;
					numUBO += binding.descriptorCount;
					numUBODyn += binding.descriptorCount;
				}
				break;
			case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
				if (numSSBODyn < caseDef.maxStorageBuffersDynamic &&
					numSSBO < caseDef.maxPerStageStorageBuffers)
				{
					arraySizes[b] = randRange(&rnd, 0, de::min(maxArray, de::min(caseDef.maxStorageBuffersDynamic - numSSBODyn,
																				 caseDef.maxPerStageStorageBuffers - numSSBO)));
					binding.descriptorCount = arraySizes[b] ? arraySizes[b] : 1;
					numSSBO += binding.descriptorCount;
					numSSBODyn += binding.descriptorCount;

					chooseWritesRandomly(binding.descriptorType, randomLayout, rnd, s, b, binding.descriptorCount);
				}
				break;
			case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
				if (numInputAttachments < caseDef.maxPerStageInputAttachments)
				{
					arraySizes[b] = randRange(&rnd, 0, de::min(maxArray, caseDef.maxPerStageInputAttachments - numInputAttachments));
					binding.descriptorCount = arraySizes[b] ? arraySizes[b] : 1;
					numInputAttachments += binding.descriptorCount;
				}
				break;
			}

			binding.stageFlags = ((binding.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT) ? (VkFlags)(VK_SHADER_STAGE_FRAGMENT_BIT) : caseDef.allShaderStages);
		}
	}

	for (deUint32 s = 0; s < caseDef.numDescriptorSets; ++s)
	{
		vector<VkDescriptorSetLayoutBinding> &bindings = randomLayout.layoutBindings[s];
		vector<VkDescriptorBindingFlags> &bindingsFlags = randomLayout.layoutBindingFlags[s];
		vector<deUint32> &variableDescriptorSizes = randomLayout.variableDescriptorSizes;

		// Choose a variable descriptor count size. If the feature is not supported, we'll just
		// allocate the whole thing later on.
		if (bindings.size() > 0 &&
			bindings[bindings.size()-1].descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC &&
			bindings[bindings.size()-1].descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC &&
			bindings[bindings.size()-1].descriptorType != VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT &&
			bindings[bindings.size()-1].descriptorType != VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR &&
			bindings[bindings.size()-1].descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_IMAGE &&
			!(s == 0 && bindings.size() == 1) && // Don't cut out the output image binding
			randRange(&rnd, 1,4) == 1) // 1 in 4 chance
		{

			bindingsFlags[bindings.size()-1] |= VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
			variableDescriptorSizes[s] = randRange(&rnd, 0,bindings[bindings.size()-1].descriptorCount);
			if (bindings[bindings.size()-1].descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
			{
				// keep a multiple of 16B
				variableDescriptorSizes[s] &= ~0xF;
			}
		}
	}
}

class CheckDecider
{
public:
	CheckDecider (deRandom& rnd, deUint32 descriptorCount)
		: m_rnd(rnd)
		, m_count(descriptorCount)
		, m_remainder(0u)
		, m_have_remainder(false)
	{
	}

	bool shouldCheck (deUint32 arrayIndex)
	{
		// Always check the first 3 and the last one, at least.
		if (arrayIndex <= 2u || arrayIndex == m_count - 1u)
			return true;

		if (!m_have_remainder)
		{
			// Find a random remainder for this set and binding.
			DE_ASSERT(m_count >= kRandomChecksPerBinding);

			// Because the divisor will be m_count/kRandomChecksPerBinding and the remainder will be chosen randomly for the
			// divisor, we expect to check around kRandomChecksPerBinding descriptors per binding randomly, no matter the amount of
			// descriptors in the binding.
			m_remainder = static_cast<deUint32>(randRange(&m_rnd, 0, static_cast<deInt32>((m_count / kRandomChecksPerBinding) - 1)));
			m_have_remainder = true;
		}

		return (arrayIndex % m_count == m_remainder);
	}

private:
	static constexpr deUint32 kRandomChecksPerBinding = 4u;

	deRandom&	m_rnd;
	deUint32	m_count;
	deUint32	m_remainder;
	bool		m_have_remainder;
};

void DescriptorSetRandomTestCase::initPrograms (SourceCollections& programCollection) const
{
	const vk::ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

	deRandom rnd;
	deRandom_init(&rnd, m_data.seed);

	m_data.randomLayout.reset(new RandomLayout(m_data.numDescriptorSets));
	RandomLayout& randomLayout = *m_data.randomLayout.get();
	generateRandomLayout(randomLayout, m_data, rnd);

	std::stringstream decls, checks;

	deUint32 inputAttachments	= 0;
	deUint32 descriptor			= 0;

	for (deUint32 s = 0; s < m_data.numDescriptorSets; ++s)
	{
		vector<VkDescriptorSetLayoutBinding> &bindings = randomLayout.layoutBindings[s];
		vector<VkDescriptorBindingFlags> bindingsFlags = randomLayout.layoutBindingFlags[s];
		vector<deUint32> &arraySizes = randomLayout.arraySizes[s];
		vector<deUint32> &variableDescriptorSizes = randomLayout.variableDescriptorSizes;

		for (size_t b = 0; b < bindings.size(); ++b)
		{
			VkDescriptorSetLayoutBinding &binding = bindings[b];
			deUint32 descriptorIncrement = (binding.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT) ? 16 : 1;

			// Construct the declaration for the binding
			if (binding.descriptorCount > 0)
			{
				std::stringstream array;
				if (m_data.indexType == INDEX_TYPE_RUNTIME_SIZE &&
					binding.descriptorType != VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
				{
					if (arraySizes[b])
					{
						array << "[]";
					}
				}
				else
				{
					if (arraySizes[b])
					{
						array << "[" << arraySizes[b] << "]";
					}
				}

				switch (binding.descriptorType)
				{
				case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT:
					decls << "layout(set = " << s << ", binding = " << b << ") uniform inlineubodef" << s << "_" << b << " { ivec4 dummy; int val" << array.str() << "; } inlineubo" << s << "_" << b << ";\n";
					break;
				case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
				case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
					decls << "layout(set = " << s << ", binding = " << b << ") uniform ubodef" << s << "_" << b << " { int val; } ubo" << s << "_" << b << array.str()  << ";\n";
					break;
				case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
				case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
					decls << "layout(set = " << s << ", binding = " << b << ") buffer sbodef" << s << "_" << b << " { int val; } ssbo" << s << "_" << b << array.str()  << ";\n";
					break;
				case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
					decls << "layout(set = " << s << ", binding = " << b << ") uniform itextureBuffer texbo" << s << "_" << b << array.str()  << ";\n";
					break;
				case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
					decls << "layout(r32i, set = " << s << ", binding = " << b << ") uniform iimageBuffer image" << s << "_" << b << array.str()  << ";\n";
					break;
				case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
					decls << "layout(r32i, set = " << s << ", binding = " << b << ") uniform iimage2D simage" << s << "_" << b << array.str()  << ";\n";
					break;
				case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
					decls << "layout(input_attachment_index = " << inputAttachments << ", set = " << s << ", binding = " << b << ") uniform isubpassInput attachment" << s << "_" << b << array.str()  << ";\n";
					inputAttachments += binding.descriptorCount;
					break;
				case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
					DE_ASSERT(s == 0 && b == 1);
					DE_ASSERT(bindings.size() >= 2);
					decls << "layout(set = " << s << ", binding = " << b << ") uniform accelerationStructureEXT as" << s << "_" << b << ";\n";
					break;
				default: DE_ASSERT(0);
				}

				const deUint32	arraySize		= de::max(1u, arraySizes[b]);
				CheckDecider	checkDecider	(rnd, arraySize);

				for (deUint32 ai = 0; ai < arraySize; ++ai, descriptor += descriptorIncrement)
				{
					// Don't access descriptors past the end of the allocated range for
					// variable descriptor count
					if (b == bindings.size() - 1 &&
						(bindingsFlags[b] & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT))
					{
						if (binding.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
						{
							// Convert to bytes and add 16 for "ivec4 dummy" in case of inline uniform block
							const deUint32 uboRange = ai*16 + 16;
							if (uboRange >= variableDescriptorSizes[s])
								continue;
						}
						else
						{
							if (ai >= variableDescriptorSizes[s])
								continue;
						}
					}

					if (s == 0 && b == 0)
					{
						// This is the output image, skip.
						continue;
					}

					if (s == 0 && b == 1 && usesAccelerationStructure(m_data.stage))
					{
						// This is the raytracing acceleration structure, skip.
						continue;
					}

					if (checkDecider.shouldCheck(ai))
					{
						// Check that the value in the descriptor equals its descriptor number.
						// i.e. check "ubo[c].val == descriptor" or "ubo[pushconst[c]].val == descriptor"
						// When doing a write check, write the descriptor number in the value.

						// First, construct the index. This can be a constant literal, a value
						// from a push constant, or a function of the previous descriptor value.
						std::stringstream ind;
						switch (m_data.indexType)
						{
						case INDEX_TYPE_NONE:
						case INDEX_TYPE_CONSTANT:
							// The index is just the constant literal
							if (arraySizes[b])
							{
								ind << "[" << ai << "]";
							}
							break;
						case INDEX_TYPE_PUSHCONSTANT:
							// identity is an int[], directly index it
							if (arraySizes[b])
							{
								ind << "[pc.identity[" << ai << "]]";
							}
							break;
						case INDEX_TYPE_RUNTIME_SIZE:
						case INDEX_TYPE_DEPENDENT:
							// Index is a function of the previous return value (which is reset to zero)
							if (arraySizes[b])
							{
								ind << "[accum + " << ai << "]";
							}
							break;
						default: DE_ASSERT(0);
						}

						const DescriptorId	descriptorId	(s, static_cast<deUint32>(b), ai);
						auto				writesItr		= randomLayout.descriptorWrites.find(descriptorId);

						if (writesItr == randomLayout.descriptorWrites.end())
						{
							// Fetch from the descriptor.
							switch (binding.descriptorType)
							{
							case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT:
								checks << "  temp = inlineubo" << s << "_" << b << ".val" << ind.str() << ";\n";
								break;
							case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
							case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
								checks << "  temp = ubo" << s << "_" << b << ind.str() << ".val;\n";
								break;
							case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
							case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
								checks << "  temp = ssbo" << s << "_" << b << ind.str() << ".val;\n";
								break;
							case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
								checks << "  temp = texelFetch(texbo" << s << "_" << b << ind.str() << ", 0).x;\n";
								break;
							case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
								checks << "  temp = imageLoad(image" << s << "_" << b << ind.str() << ", 0).x;\n";
								break;
							case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
								checks << "  temp = imageLoad(simage" << s << "_" << b << ind.str() << ", ivec2(0, 0)).x;\n";
								break;
							case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
								checks << "  temp = subpassLoad(attachment" << s << "_" << b << ind.str() << ").r;\n";
								break;
							default: DE_ASSERT(0);
							}

							// Accumulate any incorrect values.
							checks << "  accum |= temp - " << descriptor << ";\n";
						}
						else
						{
							// Check descriptor write. We need to confirm we are actually generating write code for this descriptor.
							writesItr->second.writeGenerated = true;

							// Assign each write operation to a single invocation to avoid race conditions.
							const auto			expectedInvocationID	= descriptor % (DIM*DIM);
							const std::string	writeCond				= "if (" + de::toString(expectedInvocationID) + " == invocationID)";

							switch (binding.descriptorType)
							{
							case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
							case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
								checks << "  " << writeCond << " ssbo" << s << "_" << b << ind.str() << ".val = " << descriptor << ";\n";
								break;
							case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
								checks << "  " << writeCond << " imageStore(image" << s << "_" << b << ind.str() << ", 0, ivec4(" << descriptor << ", 0, 0, 0));\n";
								break;
							case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
								checks << "  " << writeCond << " imageStore(simage" << s << "_" << b << ind.str() << ", ivec2(0, 0), ivec4(" << descriptor << ", 0, 0, 0));\n";
								break;
							default: DE_ASSERT(0);
							}
						}
					}
				}
			}
		}
	}

	std::stringstream pushdecl;
	switch (m_data.indexType)
	{
	case INDEX_TYPE_PUSHCONSTANT:
		pushdecl << "layout (push_constant, std430) uniform Block { int identity[32]; } pc;\n";
		break;
	default: DE_ASSERT(0);
	case INDEX_TYPE_NONE:
	case INDEX_TYPE_CONSTANT:
	case INDEX_TYPE_DEPENDENT:
	case INDEX_TYPE_RUNTIME_SIZE:
		break;
	}


	switch (m_data.stage)
	{
	default: DE_ASSERT(0); // Fallthrough
	case STAGE_COMPUTE:
		{
			std::stringstream css;
			css <<
				"#version 450 core\n"
				"#extension GL_EXT_nonuniform_qualifier : enable\n"
				<< pushdecl.str()
				<< decls.str() <<
				"layout(local_size_x = 1, local_size_y = 1) in;\n"
				"void main()\n"
				"{\n"
				"  const int invocationID = int(gl_GlobalInvocationID.y) * " << DIM << " + int(gl_GlobalInvocationID.x);\n"
				"  int accum = 0, temp;\n"
				<< checks.str() <<
				"  ivec4 color = (accum != 0) ? ivec4(0,0,0,0) : ivec4(1,0,0,1);\n"
				"  imageStore(simage0_0, ivec2(gl_GlobalInvocationID.xy), color);\n"
				"}\n";

			programCollection.glslSources.add("test") << glu::ComputeSource(css.str());
			break;
		}
	case STAGE_RAYGEN_NV:
	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_nonuniform_qualifier : enable\n"
			"#extension GL_NV_ray_tracing : require\n"
			<< pushdecl.str()
			<< decls.str() <<
			"void main()\n"
			"{\n"
			"  const int invocationID = int(gl_LaunchIDNV.y) * " << DIM << " + int(gl_LaunchIDNV.x);\n"
			"  int accum = 0, temp;\n"
			<< checks.str() <<
			"  ivec4 color = (accum != 0) ? ivec4(0,0,0,0) : ivec4(1,0,0,1);\n"
			"  imageStore(simage0_0, ivec2(gl_LaunchIDNV.xy), color);\n"
			"}\n";

		programCollection.glslSources.add("test") << glu::RaygenSource(css.str());
		break;
	}
	case STAGE_RAYGEN:
	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_nonuniform_qualifier : enable\n"
			"#extension GL_EXT_ray_tracing : require\n"
			<< pushdecl.str()
			<< decls.str() <<
			"void main()\n"
			"{\n"
			"  const int invocationID = int(gl_LaunchIDEXT.y) * " << DIM << " + int(gl_LaunchIDEXT.x);\n"
			"  int accum = 0, temp;\n"
			<< checks.str() <<
			"  ivec4 color = (accum != 0) ? ivec4(0,0,0,0) : ivec4(1,0,0,1);\n"
			"  imageStore(simage0_0, ivec2(gl_LaunchIDEXT.xy), color);\n"
			"}\n";

		programCollection.glslSources.add("test") << glu::RaygenSource(updateRayTracingGLSL(css.str())) << buildOptions;
		break;
	}
	case STAGE_INTERSECT:
	{
		{
			programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;
		}

		{
			std::stringstream css;
			css <<
				"#version 460 core\n"
				"#extension GL_EXT_nonuniform_qualifier : enable\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"hitAttributeEXT vec3 hitAttribute;\n"
				<< pushdecl.str()
				<< decls.str() <<
				"void main()\n"
				"{\n"
				"  const int invocationID = int(gl_LaunchIDEXT.y) * " << DIM << " + int(gl_LaunchIDEXT.x);\n"
				"  int accum = 0, temp;\n"
				<< checks.str() <<
				"  ivec4 color = (accum != 0) ? ivec4(0,0,0,0) : ivec4(1,0,0,1);\n"
				"  imageStore(simage0_0, ivec2(gl_LaunchIDEXT.xy), color);\n"
				"  hitAttribute = vec3(0.0f, 0.0f, 0.0f);\n"
				"  reportIntersectionEXT(1.0f, 0);\n"
				"}\n";

			programCollection.glslSources.add("test") << glu::IntersectionSource(updateRayTracingGLSL(css.str())) << buildOptions;
		}

		break;
	}
	case STAGE_ANY_HIT:
	{
		{
			programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;
		}

		{
			std::stringstream css;
			css <<
				"#version 460 core\n"
				"#extension GL_EXT_nonuniform_qualifier : enable\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
				"hitAttributeEXT vec3 attribs;\n"
				<< pushdecl.str()
				<< decls.str() <<
				"void main()\n"
				"{\n"
				"  const int invocationID = int(gl_LaunchIDEXT.y) * " << DIM << " + int(gl_LaunchIDEXT.x);\n"
				"  int accum = 0, temp;\n"
				<< checks.str() <<
				"  ivec4 color = (accum != 0) ? ivec4(0,0,0,0) : ivec4(1,0,0,1);\n"
				"  imageStore(simage0_0, ivec2(gl_LaunchIDEXT.xy), color);\n"
				"}\n";

			programCollection.glslSources.add("test") << glu::AnyHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
		}

		break;
	}
	case STAGE_CLOSEST_HIT:
	{
		{
			programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;
		}

		{
			std::stringstream css;
			css <<
				"#version 460 core\n"
				"#extension GL_EXT_nonuniform_qualifier : enable\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
				"hitAttributeEXT vec3 attribs;\n"
				<< pushdecl.str()
				<< decls.str() <<
				"void main()\n"
				"{\n"
				"  const int invocationID = int(gl_LaunchIDEXT.y) * " << DIM << " + int(gl_LaunchIDEXT.x);\n"
				"  int accum = 0, temp;\n"
				<< checks.str() <<
				"  ivec4 color = (accum != 0) ? ivec4(0,0,0,0) : ivec4(1,0,0,1);\n"
				"  imageStore(simage0_0, ivec2(gl_LaunchIDEXT.xy), color);\n"
				"}\n";

			programCollection.glslSources.add("test") << glu::ClosestHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
		}

		break;
	}
	case STAGE_MISS:
	{
		{
			programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;
		}

		{
			std::stringstream css;
			css <<
				"#version 460 core\n"
				"#extension GL_EXT_nonuniform_qualifier : enable\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"layout(location = 0) rayPayloadInEXT dummyPayload { vec4 dummy; };\n"
				<< pushdecl.str()
				<< decls.str() <<
				"void main()\n"
				"{\n"
				"  const int invocationID = int(gl_LaunchIDEXT.y) * " << DIM << " + int(gl_LaunchIDEXT.x);\n"
				"  int accum = 0, temp;\n"
				<< checks.str() <<
				"  ivec4 color = (accum != 0) ? ivec4(0,0,0,0) : ivec4(1,0,0,1);\n"
				"  imageStore(simage0_0, ivec2(gl_LaunchIDEXT.xy), color);\n"
				"}\n";

			programCollection.glslSources.add("test") << glu::MissSource(updateRayTracingGLSL(css.str())) << buildOptions;
		}

		break;
	}
	case STAGE_CALLABLE:
	{
		{
			std::stringstream css;
			css <<
				"#version 460 core\n"
				"#extension GL_EXT_nonuniform_qualifier : enable\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"layout(location = 0) callableDataEXT float dummy;"
				"layout(set = 0, binding = 1) uniform accelerationStructureEXT topLevelAS;\n"
				"\n"
				"void main()\n"
				"{\n"
				"  executeCallableEXT(0, 0);\n"
				"}\n";

			programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(css.str())) << buildOptions;
		}

		{
			std::stringstream css;
			css <<
				"#version 460 core\n"
				"#extension GL_EXT_nonuniform_qualifier : enable\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"layout(location = 0) callableDataInEXT float dummy;"
				<< pushdecl.str()
				<< decls.str() <<
				"void main()\n"
				"{\n"
				"  const int invocationID = int(gl_LaunchIDEXT.y) * " << DIM << " + int(gl_LaunchIDEXT.x);\n"
				"  int accum = 0, temp;\n"
				<< checks.str() <<
				"  ivec4 color = (accum != 0) ? ivec4(0,0,0,0) : ivec4(1,0,0,1);\n"
				"  imageStore(simage0_0, ivec2(gl_LaunchIDEXT.xy), color);\n"
				"}\n";

			programCollection.glslSources.add("test") << glu::CallableSource(updateRayTracingGLSL(css.str())) << buildOptions;
		}
		break;
	}
	case STAGE_VERTEX:
		{
			std::stringstream vss;
			vss <<
				"#version 450 core\n"
				"#extension GL_EXT_nonuniform_qualifier : enable\n"
				<< pushdecl.str()
				<< decls.str()  <<
				"void main()\n"
				"{\n"
				"  const int invocationID = gl_VertexIndex;\n"
				"  int accum = 0, temp;\n"
				<< checks.str() <<
				"  ivec4 color = (accum != 0) ? ivec4(0,0,0,0) : ivec4(1,0,0,1);\n"
				"  imageStore(simage0_0, ivec2(gl_VertexIndex % " << DIM << ", gl_VertexIndex / " << DIM << "), color);\n"
				"  gl_PointSize = 1.0f;\n"
				"  gl_Position = vec4(0.0f, 0.0f, 0.0f, 1.0f);\n"
				"}\n";

			programCollection.glslSources.add("test") << glu::VertexSource(vss.str());
			break;
		}
	case STAGE_FRAGMENT:
		{
			std::stringstream vss;
			vss <<
				"#version 450 core\n"
				"void main()\n"
				"{\n"
				// full-viewport quad
				"  gl_Position = vec4( 2.0*float(gl_VertexIndex&2) - 1.0, 4.0*(gl_VertexIndex&1)-1.0, 1.0 - 2.0 * float(gl_VertexIndex&1), 1);\n"
				"}\n";

			programCollection.glslSources.add("vert") << glu::VertexSource(vss.str());

			std::stringstream fss;
			fss <<
				"#version 450 core\n"
				"#extension GL_EXT_nonuniform_qualifier : enable\n"
				<< pushdecl.str()
				<< decls.str() <<
				"void main()\n"
				"{\n"
				"  const int invocationID = int(gl_FragCoord.y) * " << DIM << " + int(gl_FragCoord.x);\n"
				"  int accum = 0, temp;\n"
				<< checks.str() <<
				"  ivec4 color = (accum != 0) ? ivec4(0,0,0,0) : ivec4(1,0,0,1);\n"
				"  imageStore(simage0_0, ivec2(gl_FragCoord.x, gl_FragCoord.y), color);\n"
				"}\n";

			programCollection.glslSources.add("test") << glu::FragmentSource(fss.str());
			break;
		}
	}

}

TestInstance* DescriptorSetRandomTestCase::createInstance (Context& context) const
{
	return new DescriptorSetRandomTestInstance(context, m_data_ptr);
}

tcu::TestStatus DescriptorSetRandomTestInstance::iterate (void)
{
	const InstanceInterface&	vki							= m_context.getInstanceInterface();
	const DeviceInterface&		vk							= m_context.getDeviceInterface();
	const VkDevice				device						= m_context.getDevice();
	const VkPhysicalDevice		physicalDevice				= m_context.getPhysicalDevice();
	Allocator&					allocator					= m_context.getDefaultAllocator();
	const deUint32				queueFamilyIndex			= m_context.getUniversalQueueFamilyIndex();

	deRandom					rnd;
	VkPhysicalDeviceProperties2	properties					= getPhysicalDeviceExtensionProperties(vki, physicalDevice);
	deUint32					shaderGroupHandleSize		= 0;
	deUint32					shaderGroupBaseAlignment	= 1;

	deRandom_init(&rnd, m_data.seed);
	RandomLayout& randomLayout = *m_data.randomLayout.get();

	if (m_data.stage == STAGE_RAYGEN_NV)
	{
		const VkPhysicalDeviceRayTracingPropertiesNV rayTracingProperties = getPhysicalDeviceExtensionProperties(vki, physicalDevice);

		shaderGroupHandleSize = rayTracingProperties.shaderGroupHandleSize;
	}

	if (isRayTracingStageKHR(m_data.stage))
	{
		de::MovePtr<RayTracingProperties>	rayTracingPropertiesKHR;

		rayTracingPropertiesKHR		= makeRayTracingProperties(vki, physicalDevice);
		shaderGroupHandleSize		= rayTracingPropertiesKHR->getShaderGroupHandleSize();
		shaderGroupBaseAlignment	= rayTracingPropertiesKHR->getShaderGroupBaseAlignment();
	}

	// Get needed features.
	auto descriptorIndexingSupported	= m_context.isDeviceFunctionalitySupported("VK_EXT_descriptor_indexing");
	auto indexingFeatures				= m_context.getDescriptorIndexingFeatures();
	auto inlineUniformFeatures			= m_context.getInlineUniformBlockFeaturesEXT();

	VkPipelineBindPoint bindPoint;

	switch (m_data.stage)
	{
	case STAGE_COMPUTE:
		bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
		break;
	case STAGE_RAYGEN_NV:
		bindPoint = VK_PIPELINE_BIND_POINT_RAY_TRACING_NV;
		break;
	default:
		bindPoint = (isRayTracingStageKHR(m_data.stage) ? VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR : VK_PIPELINE_BIND_POINT_GRAPHICS);
		break;
	}

	DE_ASSERT(m_data.numDescriptorSets <= 32);
	Move<vk::VkDescriptorSetLayout>	descriptorSetLayouts[32];
	Move<vk::VkDescriptorPool>		descriptorPools[32];
	Move<vk::VkDescriptorSet>		descriptorSets[32];

	deUint32 numDescriptors = 0;
	for (deUint32 s = 0; s < m_data.numDescriptorSets; ++s)
	{
		vector<VkDescriptorSetLayoutBinding> &bindings = randomLayout.layoutBindings[s];
		vector<VkDescriptorBindingFlags> &bindingsFlags = randomLayout.layoutBindingFlags[s];
		vector<deUint32> &variableDescriptorSizes = randomLayout.variableDescriptorSizes;

		VkDescriptorPoolCreateFlags poolCreateFlags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		VkDescriptorSetLayoutCreateFlags layoutCreateFlags = 0;

		for (size_t b = 0; b < bindings.size(); ++b)
		{
			VkDescriptorSetLayoutBinding &binding = bindings[b];
			numDescriptors += binding.descriptorCount;

			// Randomly choose some bindings to use update-after-bind, if it is supported
			if (descriptorIndexingSupported &&
				m_data.uab == UPDATE_AFTER_BIND_ENABLED &&
				randRange(&rnd, 1, 8) == 1 && // 1 in 8 chance
				(binding.descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER			|| indexingFeatures.descriptorBindingUniformBufferUpdateAfterBind) &&
				(binding.descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_IMAGE				|| indexingFeatures.descriptorBindingStorageImageUpdateAfterBind) &&
				(binding.descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER			|| indexingFeatures.descriptorBindingStorageBufferUpdateAfterBind) &&
				(binding.descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER		|| indexingFeatures.descriptorBindingUniformTexelBufferUpdateAfterBind) &&
				(binding.descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER		|| indexingFeatures.descriptorBindingStorageTexelBufferUpdateAfterBind) &&
				(binding.descriptorType != VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT	|| inlineUniformFeatures.descriptorBindingInlineUniformBlockUpdateAfterBind) &&
				(binding.descriptorType != VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT) &&
				(binding.descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) &&
				(binding.descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC) &&
				(binding.descriptorType != VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR))
			{
				bindingsFlags[b] |= VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
				layoutCreateFlags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT;
				poolCreateFlags |= VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;
			}

			if (!indexingFeatures.descriptorBindingVariableDescriptorCount)
			{
				bindingsFlags[b] &= ~VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
			}
		}

		// Create a layout and allocate a descriptor set for it.

		const VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo =
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT,	// VkStructureType						sType;
			DE_NULL,																// const void*							pNext;
			(deUint32)bindings.size(),												// uint32_t								bindingCount;
			bindings.empty() ? DE_NULL : bindingsFlags.data(),						// const VkDescriptorBindingFlags*	pBindingFlags;
		};

		const VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo =
		{
			vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,	//  VkStructureType						sType;
			(descriptorIndexingSupported ? &bindingFlagsInfo : DE_NULL),//  const void*							pNext;
			layoutCreateFlags,											//  VkDescriptorSetLayoutCreateFlags	flags;
			(deUint32)bindings.size(),									//  deUint32							bindingCount;
			bindings.empty() ? DE_NULL : bindings.data()				//  const VkDescriptorSetLayoutBinding*	pBindings;
		};

		descriptorSetLayouts[s] = vk::createDescriptorSetLayout(vk, device, &setLayoutCreateInfo);

		vk::DescriptorPoolBuilder poolBuilder;
		poolBuilder.addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, m_data.maxPerStageUniformBuffers);
		poolBuilder.addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, m_data.maxUniformBuffersDynamic);
		poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_data.maxPerStageStorageBuffers);
		poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, m_data.maxStorageBuffersDynamic);
		poolBuilder.addType(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, m_data.maxPerStageSampledImages);
		poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, m_data.maxPerStageStorageTexelBuffers);
		poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, m_data.maxPerStageStorageImages);
		if (m_data.maxPerStageInputAttachments > 0u)
		{
			poolBuilder.addType(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, m_data.maxPerStageInputAttachments);
		}
		if (m_data.maxInlineUniformBlocks > 0u)
		{
			poolBuilder.addType(VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT, m_data.maxInlineUniformBlocks * m_data.maxInlineUniformBlockSize);
		}
		if (usesAccelerationStructure(m_data.stage))
		{
			poolBuilder.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1u);
		}

		VkDescriptorPoolInlineUniformBlockCreateInfoEXT inlineUniformBlockPoolCreateInfo =
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_INLINE_UNIFORM_BLOCK_CREATE_INFO_EXT,	// VkStructureType	sType;
			DE_NULL,																// const void*		pNext;
			m_data.maxInlineUniformBlocks,											// uint32_t			maxInlineUniformBlockBindings;
		};

		descriptorPools[s] = poolBuilder.build(vk, device, poolCreateFlags, 1u,
											   m_data.maxInlineUniformBlocks ? &inlineUniformBlockPoolCreateInfo : DE_NULL);

		VkDescriptorSetVariableDescriptorCountAllocateInfo variableCountInfo =
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,		// VkStructureType	sType;
			DE_NULL,																		// const void*		pNext;
			0,																				// uint32_t			descriptorSetCount;
			DE_NULL,																		// const uint32_t*	pDescriptorCounts;
		};

		const void *pNext = DE_NULL;
		if (bindings.size() > 0 &&
			bindingsFlags[bindings.size()-1] & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT)
		{
			variableCountInfo.descriptorSetCount = 1;
			variableCountInfo.pDescriptorCounts = &variableDescriptorSizes[s];
			pNext = &variableCountInfo;
		}

		descriptorSets[s] = makeDescriptorSet(vk, device, *descriptorPools[s], *descriptorSetLayouts[s], pNext);
	}

	// Create a buffer to hold data for all descriptors.
	VkDeviceSize	align = std::max({
		properties.properties.limits.minTexelBufferOffsetAlignment,
		properties.properties.limits.minUniformBufferOffsetAlignment,
		properties.properties.limits.minStorageBufferOffsetAlignment,
		(VkDeviceSize)sizeof(deUint32)});

	de::MovePtr<BufferWithMemory> buffer;

	buffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
		vk, device, allocator, makeBufferCreateInfo(align*numDescriptors,
													VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
													VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
													VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT |
													VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT),
													MemoryRequirement::HostVisible));
	deUint8 *bufferPtr = (deUint8 *)buffer->getAllocation().getHostPtr();

	// Create storage images separately.
	deUint32				storageImageCount		= 0u;
	vector<Move<VkImage>>	storageImages;

	const VkImageCreateInfo	storageImgCreateInfo	=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType;
		DE_NULL,								// const void*				pNext;
		0u,										// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						// VkImageType				imageType;
		VK_FORMAT_R32_SINT,						// VkFormat					format;
		{ 1u, 1u, 1u },							// VkExtent3D				extent;
		1u,										// deUint32					mipLevels;
		1u,										// deUint32					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling;
		VK_IMAGE_USAGE_STORAGE_BIT
		| VK_IMAGE_USAGE_TRANSFER_SRC_BIT
		| VK_IMAGE_USAGE_TRANSFER_DST_BIT,		// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode;
		1u,										// deUint32					queueFamilyIndexCount;
		&queueFamilyIndex,						// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED				// VkImageLayout			initialLayout;
	};

	// Create storage images.
	for (const auto& bindings	: randomLayout.layoutBindings)
	for (const auto& binding	: bindings)
	{
		if (binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
		{
			storageImageCount += binding.descriptorCount;
			for (deUint32 d = 0; d < binding.descriptorCount; ++d)
			{
				storageImages.push_back(createImage(vk, device, &storageImgCreateInfo));
			}
		}
	}

	// Allocate memory for them.
	vk::VkMemoryRequirements storageImageMemReqs;
	vk.getImageMemoryRequirements(device, *storageImages.front(), &storageImageMemReqs);

	de::MovePtr<Allocation> storageImageAlloc;
	VkDeviceSize			storageImageBlockSize = 0u;
	{
		VkDeviceSize mod = (storageImageMemReqs.size % storageImageMemReqs.alignment);
		storageImageBlockSize = storageImageMemReqs.size + ((mod == 0u) ? 0u : storageImageMemReqs.alignment - mod);
	}
	storageImageMemReqs.size = storageImageBlockSize * storageImageCount;
	storageImageAlloc = allocator.allocate(storageImageMemReqs, MemoryRequirement::Any);

	// Allocate buffer to copy storage images to.
	auto		storageImgBuffer	= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vk, device, allocator, makeBufferCreateInfo(storageImageCount * sizeof(deInt32), VK_BUFFER_USAGE_TRANSFER_DST_BIT), MemoryRequirement::HostVisible));
	deInt32*	storageImgBufferPtr	= reinterpret_cast<deInt32*>(storageImgBuffer->getAllocation().getHostPtr());

	// Create image views.
	vector<Move<VkImageView>>	storageImageViews;
	{
		VkImageViewCreateInfo		storageImageViewCreateInfo =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			0u,												// VkImageViewCreateFlags	flags;
			DE_NULL,										// VkImage					image;
			VK_IMAGE_VIEW_TYPE_2D,							// VkImageViewType			viewType;
			VK_FORMAT_R32_SINT,								// VkFormat					format;
			{												// VkComponentMapping		channels;
				VK_COMPONENT_SWIZZLE_IDENTITY,
				VK_COMPONENT_SWIZZLE_IDENTITY,
				VK_COMPONENT_SWIZZLE_IDENTITY,
				VK_COMPONENT_SWIZZLE_IDENTITY
			},
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u }	// VkImageSubresourceRange	subresourceRange;
		};

		for (deUint32 i = 0; i < static_cast<deUint32>(storageImages.size()); ++i)
		{
			// Bind image memory.
			vk::VkImage img = *storageImages[i];
			VK_CHECK(vk.bindImageMemory(device, img, storageImageAlloc->getMemory(), storageImageAlloc->getOffset() + i * storageImageBlockSize));

			// Create view.
			storageImageViewCreateInfo.image = img;
			storageImageViews.push_back(createImageView(vk, device, &storageImageViewCreateInfo));
		}
	}

	// Create input attachment images.
	vector<Move<VkImage>>	inputAttachments;
	const VkImageCreateInfo imgCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,										// VkStructureType			sType;
		DE_NULL,																	// const void*				pNext;
		0u,																			// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,															// VkImageType				imageType;
		VK_FORMAT_R32_SINT,															// VkFormat					format;
		{ DIM, DIM, 1u },															// VkExtent3D				extent;
		1u,																			// deUint32					mipLevels;
		1u,																			// deUint32					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,														// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,													// VkImageTiling			tiling;
		(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT),	// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,													// VkSharingMode			sharingMode;
		1u,																			// deUint32					queueFamilyIndexCount;
		&queueFamilyIndex,															// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED													// VkImageLayout			initialLayout;

	};

	deUint32 inputAttachmentCount = 0u;
	for (const auto& bindings	: randomLayout.layoutBindings)
	for (const auto& binding	: bindings)
	{
		if (binding.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
		{
			inputAttachmentCount += binding.descriptorCount;
			for (deUint32 d = 0; d < binding.descriptorCount; ++d)
			{
				inputAttachments.push_back(createImage(vk, device, &imgCreateInfo));
			}
		}
	}

	de::MovePtr<Allocation> inputAttachmentAlloc;
	VkDeviceSize			imageBlockSize = 0u;

	if (inputAttachmentCount > 0u)
	{
		VkMemoryRequirements	imageReqs		= getImageMemoryRequirements(vk, device, inputAttachments.back().get());
		VkDeviceSize			mod				= imageReqs.size % imageReqs.alignment;

		// Create memory for every input attachment image.
		imageBlockSize	= imageReqs.size + ((mod == 0u) ? 0u : (imageReqs.alignment - mod));
		imageReqs.size	= imageBlockSize * inputAttachmentCount;
		inputAttachmentAlloc = allocator.allocate(imageReqs, MemoryRequirement::Any);
	}

	// Bind memory to each input attachment and create an image view.
	VkImageViewCreateInfo		inputAttachmentViewParams =
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		0u,												// VkImageViewCreateFlags	flags;
		DE_NULL,										// VkImage					image;
		VK_IMAGE_VIEW_TYPE_2D,							// VkImageViewType			viewType;
		VK_FORMAT_R32_SINT,								// VkFormat					format;
		{												// VkComponentMapping		channels;
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY
		},
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u }	// VkImageSubresourceRange	subresourceRange;
	};
	vector<Move<VkImageView>>	inputAttachmentViews;

	for (deUint32 i = 0; i < static_cast<deUint32>(inputAttachments.size()); ++i)
	{
		vk::VkImage img = *inputAttachments[i];
		VK_CHECK(vk.bindImageMemory(device, img, inputAttachmentAlloc->getMemory(), inputAttachmentAlloc->getOffset() + i * imageBlockSize));

		inputAttachmentViewParams.image = img;
		inputAttachmentViews.push_back(createImageView(vk, device, &inputAttachmentViewParams));
	}

	// Create a view for each descriptor. Fill descriptor 'd' with an integer value equal to 'd'. In case the descriptor would be
	// written to from the shader, store a -1 in it instead. Skip inline uniform blocks and use images for input attachments and
	// storage images.

	Move<VkCommandPool>				cmdPool						= createCommandPool(vk, device, 0, queueFamilyIndex);
	const VkQueue					queue						= m_context.getUniversalQueue();
	Move<VkCommandBuffer>			cmdBuffer					= allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	const VkImageSubresourceRange	clearRange					=
	{
		VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
		0u,			// deUint32				baseMipLevel;
		1u,			// deUint32				levelCount;
		0u,			// deUint32				baseArrayLayer;
		1u			// deUint32				layerCount;
	};

	VkImageMemoryBarrier			preInputAttachmentBarrier	=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,				// VkStructureType		sType
		DE_NULL,											// const void*			pNext
		0u,													// VkAccessFlags		srcAccessMask
		VK_ACCESS_TRANSFER_WRITE_BIT,						// VkAccessFlags		dstAccessMask
		VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout		oldLayout
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,				// VkImageLayout		newLayout
		VK_QUEUE_FAMILY_IGNORED,							// uint32_t				srcQueueFamilyIndex
		VK_QUEUE_FAMILY_IGNORED,							// uint32_t				dstQueueFamilyIndex
		DE_NULL,											// VkImage				image
		{
			VK_IMAGE_ASPECT_COLOR_BIT,				// VkImageAspectFlags	aspectMask
			0u,										// uint32_t				baseMipLevel
			1u,										// uint32_t				mipLevels,
			0u,										// uint32_t				baseArray
			1u,										// uint32_t				arraySize
		}
	};

	VkImageMemoryBarrier			postInputAttachmentBarrier	=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			srcAccessMask;
		VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,		// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,	// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
		DE_NULL,									// VkImage					image;
		clearRange,									// VkImageSubresourceRange	subresourceRange;
	};

	VkImageMemoryBarrier			preStorageImageBarrier		=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType		sType
		DE_NULL,									// const void*			pNext
		0u,											// VkAccessFlags		srcAccessMask
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags		dstAccessMask
		VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout		oldLayout
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout		newLayout
		VK_QUEUE_FAMILY_IGNORED,					// uint32_t				srcQueueFamilyIndex
		VK_QUEUE_FAMILY_IGNORED,					// uint32_t				dstQueueFamilyIndex
		DE_NULL,									// VkImage				image
		{
			VK_IMAGE_ASPECT_COLOR_BIT,				// VkImageAspectFlags	aspectMask
			0u,										// uint32_t				baseMipLevel
			1u,										// uint32_t				mipLevels,
			0u,										// uint32_t				baseArray
			1u,										// uint32_t				arraySize
		}
	};

	VkImageMemoryBarrier			postStorageImageBarrier		=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,						// VkStructureType			sType;
		DE_NULL,													// const void*				pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,								// VkAccessFlags			srcAccessMask;
		(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT),	// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,						// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_GENERAL,									// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,									// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,									// deUint32					dstQueueFamilyIndex;
		DE_NULL,													// VkImage					image;
		clearRange,													// VkImageSubresourceRange	subresourceRange;
	};

	vk::VkClearColorValue			clearValue;
	clearValue.uint32[0] = 0u;
	clearValue.uint32[1] = 0u;
	clearValue.uint32[2] = 0u;
	clearValue.uint32[3] = 0u;

	beginCommandBuffer(vk, *cmdBuffer, 0u);

	int			descriptor		= 0;
	deUint32	attachmentIndex	= 0;
	deUint32	storageImgIndex	= 0;

	typedef vk::Unique<vk::VkBufferView>		BufferViewHandleUp;
	typedef de::SharedPtr<BufferViewHandleUp>	BufferViewHandleSp;

	vector<BufferViewHandleSp>					bufferViews(de::max(1u,numDescriptors));

	for (deUint32 s = 0; s < m_data.numDescriptorSets; ++s)
	{
		vector<VkDescriptorSetLayoutBinding> &bindings = randomLayout.layoutBindings[s];
		for (size_t b = 0; b < bindings.size(); ++b)
		{
			VkDescriptorSetLayoutBinding &binding = bindings[b];

			if (binding.descriptorCount == 0)
			{
				continue;
			}
			if (binding.descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
			{
				descriptor++;
			}
			else if (binding.descriptorType != VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT &&
					 binding.descriptorType != VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT &&
					 binding.descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
			{
				for (deUint32 d = descriptor; d < descriptor + binding.descriptorCount; ++d)
				{
					DescriptorId	descriptorId	(s, static_cast<deUint32>(b), d - descriptor);
					auto			writeInfoItr	= randomLayout.descriptorWrites.find(descriptorId);
					deInt32*		ptr				= (deInt32 *)(bufferPtr + align*d);

					if (writeInfoItr == randomLayout.descriptorWrites.end())
					{
						*ptr = static_cast<deInt32>(d);
					}
					else
					{
						*ptr = -1;
						writeInfoItr->second.ptr = ptr;
						writeInfoItr->second.expected = d;
					}

					const vk::VkBufferViewCreateInfo viewCreateInfo =
					{
						vk::VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,
						DE_NULL,
						(vk::VkBufferViewCreateFlags)0,
						**buffer,								// buffer
						VK_FORMAT_R32_SINT,						// format
						(vk::VkDeviceSize)align*d,				// offset
						(vk::VkDeviceSize)sizeof(deUint32)		// range
					};
					vk::Move<vk::VkBufferView> bufferView = vk::createBufferView(vk, device, &viewCreateInfo);
					bufferViews[d] = BufferViewHandleSp(new BufferViewHandleUp(bufferView));
				}
				descriptor += binding.descriptorCount;
			}
			else if (binding.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
			{
				// subtract 16 for "ivec4 dummy"
				DE_ASSERT(binding.descriptorCount >= 16);
				descriptor += binding.descriptorCount - 16;
			}
			else if (binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
			{
				// Storage image.
				for (deUint32 d = descriptor; d < descriptor + binding.descriptorCount; ++d)
				{
					VkImage			img				= *storageImages[storageImgIndex];
					DescriptorId	descriptorId	(s, static_cast<deUint32>(b), d - descriptor);
					deInt32*		ptr				= storageImgBufferPtr + storageImgIndex;

					auto			writeInfoItr	= randomLayout.descriptorWrites.find(descriptorId);
					const bool		isWrite			= (writeInfoItr != randomLayout.descriptorWrites.end());

					if (isWrite)
					{
						writeInfoItr->second.ptr		= ptr;
						writeInfoItr->second.expected	= static_cast<deInt32>(d);
					}

					preStorageImageBarrier.image	= img;
					clearValue.int32[0]				= (isWrite ? -1 : static_cast<deInt32>(d));
					postStorageImageBarrier.image	= img;

					vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &preStorageImageBarrier);
					vk.cmdClearColorImage(*cmdBuffer, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1, &clearRange);
					vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, m_data.allPipelineStages, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &postStorageImageBarrier);

					++storageImgIndex;
				}
				descriptor += binding.descriptorCount;
			}
			else
			{
				// Input attachment.
				for (deUint32 d = descriptor; d < descriptor + binding.descriptorCount; ++d)
				{
					VkImage img = *inputAttachments[attachmentIndex];

					preInputAttachmentBarrier.image		= img;
					clearValue.int32[0]					= static_cast<deInt32>(d);
					postInputAttachmentBarrier.image	= img;

					vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &preInputAttachmentBarrier);
					vk.cmdClearColorImage(*cmdBuffer, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1, &clearRange);
					vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &postInputAttachmentBarrier);

					++attachmentIndex;
				}
				descriptor += binding.descriptorCount;
			}
		}
	}

	// Flush modified memory.
	flushAlloc(vk, device, buffer->getAllocation());

	// Push constants are used for dynamic indexing. PushConstant[i] = i.
	const VkPushConstantRange			pushConstRange			=
	{
		m_data.allShaderStages,	// VkShaderStageFlags	stageFlags
		0,						// deUint32				offset
		128						// deUint32				size
	};

	vector<vk::VkDescriptorSetLayout>	descriptorSetLayoutsRaw	(m_data.numDescriptorSets);
	for (size_t i = 0; i < m_data.numDescriptorSets; ++i)
	{
		descriptorSetLayoutsRaw[i] = descriptorSetLayouts[i].get();
	}

	const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,				//  VkStructureType					sType;
		DE_NULL,													//  const void*						pNext;
		(VkPipelineLayoutCreateFlags)0,								//  VkPipelineLayoutCreateFlags		flags;
		m_data.numDescriptorSets,									//  deUint32						setLayoutCount;
		&descriptorSetLayoutsRaw[0],								//  const VkDescriptorSetLayout*	pSetLayouts;
		m_data.indexType == INDEX_TYPE_PUSHCONSTANT ? 1u : 0u,		//  deUint32						pushConstantRangeCount;
		&pushConstRange,											//  const VkPushConstantRange*		pPushConstantRanges;
	};

	Move<VkPipelineLayout> pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo, NULL);

	if (m_data.indexType == INDEX_TYPE_PUSHCONSTANT)
	{
		// PushConstant[i] = i
		for (deUint32 i = 0; i < (deUint32)(128 / sizeof(deUint32)); ++i)
		{
			vk.cmdPushConstants(*cmdBuffer, *pipelineLayout, m_data.allShaderStages,
								(deUint32)(i * sizeof(deUint32)), (deUint32)sizeof(deUint32), &i);
		}
	}

	de::MovePtr<BufferWithMemory> copyBuffer;
	copyBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
		vk, device, allocator, makeBufferCreateInfo(DIM*DIM*sizeof(deUint32), VK_BUFFER_USAGE_TRANSFER_DST_BIT), MemoryRequirement::HostVisible));

	// Special case for the output storage image.
	const VkImageCreateInfo			imageCreateInfo			=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType;
		DE_NULL,								// const void*				pNext;
		(VkImageCreateFlags)0u,					// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						// VkImageType				imageType;
		VK_FORMAT_R32_SINT,						// VkFormat					format;
		{
			DIM,								// deUint32	width;
			DIM,								// deUint32	height;
			1u									// deUint32	depth;
		},										// VkExtent3D				extent;
		1u,										// deUint32					mipLevels;
		1u,										// deUint32					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling;
		VK_IMAGE_USAGE_STORAGE_BIT
		| VK_IMAGE_USAGE_TRANSFER_SRC_BIT
		| VK_IMAGE_USAGE_TRANSFER_DST_BIT,		// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode;
		0u,										// deUint32					queueFamilyIndexCount;
		DE_NULL,								// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED				// VkImageLayout			initialLayout;
	};

	VkImageViewCreateInfo		imageViewCreateInfo		=
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		(VkImageViewCreateFlags)0u,					// VkImageViewCreateFlags	flags;
		DE_NULL,									// VkImage					image;
		VK_IMAGE_VIEW_TYPE_2D,						// VkImageViewType			viewType;
		VK_FORMAT_R32_SINT,							// VkFormat					format;
		{
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY
		},											// VkComponentMapping		 components;
		{
			VK_IMAGE_ASPECT_COLOR_BIT,				// VkImageAspectFlags	aspectMask;
			0u,										// deUint32				baseMipLevel;
			1u,										// deUint32				levelCount;
			0u,										// deUint32				baseArrayLayer;
			1u										// deUint32				layerCount;
		}											// VkImageSubresourceRange	subresourceRange;
	};

	de::MovePtr<ImageWithMemory> image;
	Move<VkImageView> imageView;

	image = de::MovePtr<ImageWithMemory>(new ImageWithMemory(
		vk, device, allocator, imageCreateInfo, MemoryRequirement::Any));
	imageViewCreateInfo.image = **image;
	imageView = createImageView(vk, device, &imageViewCreateInfo, NULL);

	// Create ray tracing structures
	de::MovePtr<vk::BottomLevelAccelerationStructure>	bottomLevelAccelerationStructure;
	de::MovePtr<vk::TopLevelAccelerationStructure>		topLevelAccelerationStructure;
	VkStridedDeviceAddressRegionKHR						raygenShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
	VkStridedDeviceAddressRegionKHR						missShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
	VkStridedDeviceAddressRegionKHR						hitShaderBindingTableRegion			= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
	VkStridedDeviceAddressRegionKHR						callableShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);

	if (usesAccelerationStructure(m_data.stage))
	{
		// Create bottom level acceleration structure
		{
			bottomLevelAccelerationStructure = makeBottomLevelAccelerationStructure();

			bottomLevelAccelerationStructure->setDefaultGeometryData(getShaderStageFlag(m_data.stage));

			bottomLevelAccelerationStructure->createAndBuild(vk, device, *cmdBuffer, allocator);
		}

		// Create top level acceleration structure
		{
			topLevelAccelerationStructure = makeTopLevelAccelerationStructure();

			topLevelAccelerationStructure->setInstanceCount(1);
			topLevelAccelerationStructure->addInstance(de::SharedPtr<BottomLevelAccelerationStructure>(bottomLevelAccelerationStructure.release()));

			topLevelAccelerationStructure->createAndBuild(vk, device, *cmdBuffer, allocator);
		}
	}

	descriptor		= 0;
	attachmentIndex	= 0;
	storageImgIndex = 0;

	for (deUint32 s = 0; s < m_data.numDescriptorSets; ++s)
	{
		vector<VkDescriptorSetLayoutBinding> &bindings = randomLayout.layoutBindings[s];
		vector<VkDescriptorBindingFlags> &bindingsFlags = randomLayout.layoutBindingFlags[s];
		vector<deUint32> &arraySizes = randomLayout.arraySizes[s];
		vector<deUint32> &variableDescriptorSizes = randomLayout.variableDescriptorSizes;

		vector<VkDescriptorBufferInfo> bufferInfoVec(numDescriptors);
		vector<VkDescriptorImageInfo> imageInfoVec(numDescriptors);
		vector<VkBufferView> bufferViewVec(numDescriptors);
		vector<VkWriteDescriptorSetInlineUniformBlockEXT> inlineInfoVec(numDescriptors);
		vector<VkWriteDescriptorSetAccelerationStructureKHR> accelerationInfoVec(numDescriptors);
		vector<deUint32> descriptorNumber(numDescriptors);
		vector<VkWriteDescriptorSet> writesBeforeBindVec(0);
		vector<VkWriteDescriptorSet> writesAfterBindVec(0);
		int vecIndex = 0;
		int numDynamic = 0;

		vector<VkDescriptorUpdateTemplateEntry> imgTemplateEntriesBefore,		imgTemplateEntriesAfter,
												bufTemplateEntriesBefore,		bufTemplateEntriesAfter,
												texelBufTemplateEntriesBefore,	texelBufTemplateEntriesAfter,
												inlineTemplateEntriesBefore,	inlineTemplateEntriesAfter;

		for (size_t b = 0; b < bindings.size(); ++b)
		{
			VkDescriptorSetLayoutBinding &binding = bindings[b];
			deUint32 descriptorIncrement = (binding.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT) ? 16 : 1;

			// Construct the declaration for the binding
			if (binding.descriptorCount > 0)
			{
				bool updateAfterBind = !!(bindingsFlags[b] & VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);
				for (deUint32 ai = 0; ai < de::max(1u, arraySizes[b]); ++ai, descriptor += descriptorIncrement)
				{
					// Don't access descriptors past the end of the allocated range for
					// variable descriptor count
					if (b == bindings.size() - 1 &&
						(bindingsFlags[b] & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT))
					{
						if (binding.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
						{
							// Convert to bytes and add 16 for "ivec4 dummy" in case of inline uniform block
							const deUint32 uboRange = ai*16 + 16;
							if (uboRange >= variableDescriptorSizes[s])
								continue;
						}
						else
						{
							if (ai >= variableDescriptorSizes[s])
								continue;
						}
					}

					// output image
					switch (binding.descriptorType)
					{
					case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
						// Output image. Special case.
						if (s == 0 && b == 0)
						{
							imageInfoVec[vecIndex] = makeDescriptorImageInfo(DE_NULL, *imageView, VK_IMAGE_LAYOUT_GENERAL);
						}
						else
						{
							imageInfoVec[vecIndex] = makeDescriptorImageInfo(DE_NULL, storageImageViews[storageImgIndex].get(), VK_IMAGE_LAYOUT_GENERAL);
						}
						++storageImgIndex;
						break;
					case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
						imageInfoVec[vecIndex] = makeDescriptorImageInfo(DE_NULL, inputAttachmentViews[attachmentIndex].get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
						++attachmentIndex;
						break;
					case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT:
						// Handled below.
						break;
					case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
						// Handled below.
						break;
					default:
						// Other descriptor types.
						bufferInfoVec[vecIndex] = makeDescriptorBufferInfo(**buffer, descriptor*align, sizeof(deUint32));
						bufferViewVec[vecIndex] = **bufferViews[descriptor];
						break;
					}

					descriptorNumber[descriptor] = descriptor;

					VkWriteDescriptorSet w =
					{
						VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,		//  VkStructureType					sType;
						DE_NULL,									//  const void*						pNext;
						*descriptorSets[s],							//  VkDescriptorSet					dstSet;
						(deUint32)b,								//  deUint32						dstBinding;
						ai,											//  deUint32						dstArrayElement;
						1u,											//  deUint32						descriptorCount;
						binding.descriptorType,						//  VkDescriptorType				descriptorType;
						&imageInfoVec[vecIndex],					//  const VkDescriptorImageInfo*	pImageInfo;
						&bufferInfoVec[vecIndex],					//  const VkDescriptorBufferInfo*	pBufferInfo;
						&bufferViewVec[vecIndex],					//  const VkBufferView*				pTexelBufferView;
					};

					if (binding.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
					{
						VkWriteDescriptorSetInlineUniformBlockEXT iuBlock =
						{
							VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK_EXT,	// VkStructureType	sType;
							DE_NULL,															// const void*		pNext;
							sizeof(deUint32),													// uint32_t			dataSize;
							&descriptorNumber[descriptor],										// const void*		pData;
						};

						inlineInfoVec[vecIndex] = iuBlock;
						w.dstArrayElement = ai*16 + 16; // add 16 to skip "ivec4 dummy"
						w.pNext = &inlineInfoVec[vecIndex];
						w.descriptorCount = sizeof(deUint32);
					}

					if (binding.descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
					{
						const TopLevelAccelerationStructure*			topLevelAccelerationStructurePtr		= topLevelAccelerationStructure.get();
						VkWriteDescriptorSetAccelerationStructureKHR	accelerationStructureWriteDescriptorSet	=
						{
							VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,	//  VkStructureType						sType;
							DE_NULL,															//  const void*							pNext;
							w.descriptorCount,													//  deUint32							accelerationStructureCount;
							topLevelAccelerationStructurePtr->getPtr(),							//  const VkAccelerationStructureKHR*	pAccelerationStructures;
						};

						accelerationInfoVec[vecIndex] = accelerationStructureWriteDescriptorSet;
						w.dstArrayElement = 0;
						w.pNext = &accelerationInfoVec[vecIndex];
					}

					VkDescriptorUpdateTemplateEntry templateEntry =
					{
						(deUint32)b,				// uint32_t				dstBinding;
						ai,							// uint32_t				dstArrayElement;
						1u,							// uint32_t				descriptorCount;
						binding.descriptorType,		// VkDescriptorType		descriptorType;
						0,							// size_t				offset;
						0,							// size_t				stride;
					};

					switch (binding.descriptorType)
					{
						default:
							TCU_THROW(InternalError, "Unknown descriptor type");

						case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
						case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
							templateEntry.offset = vecIndex * sizeof(VkDescriptorImageInfo);
							(updateAfterBind ? imgTemplateEntriesAfter : imgTemplateEntriesBefore).push_back(templateEntry);
							break;
						case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
						case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
							templateEntry.offset = vecIndex * sizeof(VkBufferView);
							(updateAfterBind ? texelBufTemplateEntriesAfter : texelBufTemplateEntriesBefore).push_back(templateEntry);
							break;
						case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
						case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
						case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
						case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
							templateEntry.offset = vecIndex * sizeof(VkDescriptorBufferInfo);
							(updateAfterBind ? bufTemplateEntriesAfter : bufTemplateEntriesBefore).push_back(templateEntry);
							break;
						case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT:
							templateEntry.offset = descriptor * sizeof(deUint32);
							templateEntry.dstArrayElement = ai*16 + 16; // add 16 to skip "ivec4 dummy"
							templateEntry.descriptorCount = sizeof(deUint32);
							(updateAfterBind ? inlineTemplateEntriesAfter : inlineTemplateEntriesBefore).push_back(templateEntry);
							break;
						case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
							DE_ASSERT(!updateAfterBind);
							DE_ASSERT(usesAccelerationStructure(m_data.stage));
							break;
					}

					vecIndex++;

					(updateAfterBind ? writesAfterBindVec : writesBeforeBindVec).push_back(w);

					// Count the number of dynamic descriptors in this set.
					if (binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
						binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
					{
						numDynamic++;
					}
				}
			}
		}

		// Make zeros have at least one element so &zeros[0] works
		vector<deUint32> zeros(de::max(1,numDynamic));
		deMemset(&zeros[0], 0, numDynamic * sizeof(deUint32));

		// Randomly select between vkUpdateDescriptorSets and vkUpdateDescriptorSetWithTemplate
		if (randRange(&rnd, 1, 2) == 1 &&
			m_context.contextSupports(vk::ApiVersion(1, 1, 0)) &&
			!usesAccelerationStructure(m_data.stage))
		{
			DE_ASSERT(!usesAccelerationStructure(m_data.stage));

			VkDescriptorUpdateTemplateCreateInfo templateCreateInfo =
			{
				VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO,	// VkStructureType							sType;
				NULL,														// void*									pNext;
				0,															// VkDescriptorUpdateTemplateCreateFlags	flags;
				0,															// uint32_t									descriptorUpdateEntryCount;
				DE_NULL,													// uint32_t									descriptorUpdateEntryCount;
				VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET,			// VkDescriptorUpdateTemplateType			templateType;
				descriptorSetLayouts[s].get(),								// VkDescriptorSetLayout					descriptorSetLayout;
				bindPoint,													// VkPipelineBindPoint						pipelineBindPoint;
				0,															// VkPipelineLayout							pipelineLayout;
				0,															// uint32_t									set;
			};

			void *templateVectorData[] =
			{
				imageInfoVec.data(),
				bufferInfoVec.data(),
				bufferViewVec.data(),
				descriptorNumber.data(),
			};

			vector<VkDescriptorUpdateTemplateEntry> *templateVectorsBefore[] =
			{
				&imgTemplateEntriesBefore,
				&bufTemplateEntriesBefore,
				&texelBufTemplateEntriesBefore,
				&inlineTemplateEntriesBefore,
			};

			vector<VkDescriptorUpdateTemplateEntry> *templateVectorsAfter[] =
			{
				&imgTemplateEntriesAfter,
				&bufTemplateEntriesAfter,
				&texelBufTemplateEntriesAfter,
				&inlineTemplateEntriesAfter,
			};

			for (size_t i = 0; i < DE_LENGTH_OF_ARRAY(templateVectorsBefore); ++i)
			{
				if (templateVectorsBefore[i]->size())
				{
					templateCreateInfo.descriptorUpdateEntryCount = (deUint32)templateVectorsBefore[i]->size();
					templateCreateInfo.pDescriptorUpdateEntries = templateVectorsBefore[i]->data();
					Move<VkDescriptorUpdateTemplate> descriptorUpdateTemplate = createDescriptorUpdateTemplate(vk, device, &templateCreateInfo, NULL);
					vk.updateDescriptorSetWithTemplate(device, descriptorSets[s].get(), *descriptorUpdateTemplate, templateVectorData[i]);
				}
			}

			vk.cmdBindDescriptorSets(*cmdBuffer, bindPoint, *pipelineLayout, s, 1, &descriptorSets[s].get(), numDynamic, &zeros[0]);

			for (size_t i = 0; i < DE_LENGTH_OF_ARRAY(templateVectorsAfter); ++i)
			{
				if (templateVectorsAfter[i]->size())
				{
					templateCreateInfo.descriptorUpdateEntryCount = (deUint32)templateVectorsAfter[i]->size();
					templateCreateInfo.pDescriptorUpdateEntries = templateVectorsAfter[i]->data();
					Move<VkDescriptorUpdateTemplate> descriptorUpdateTemplate = createDescriptorUpdateTemplate(vk, device, &templateCreateInfo, NULL);
					vk.updateDescriptorSetWithTemplate(device, descriptorSets[s].get(), *descriptorUpdateTemplate, templateVectorData[i]);
				}
			}

		}
		else
		{
			if (writesBeforeBindVec.size())
			{
				vk.updateDescriptorSets(device, (deUint32)writesBeforeBindVec.size(), &writesBeforeBindVec[0], 0, NULL);
			}

			vk.cmdBindDescriptorSets(*cmdBuffer, bindPoint, *pipelineLayout, s, 1, &descriptorSets[s].get(), numDynamic, &zeros[0]);

			if (writesAfterBindVec.size())
			{
				vk.updateDescriptorSets(device, (deUint32)writesAfterBindVec.size(), &writesAfterBindVec[0], 0, NULL);
			}
		}
	}

	Move<VkPipeline> pipeline;
	Move<VkRenderPass> renderPass;
	Move<VkFramebuffer> framebuffer;

	de::MovePtr<BufferWithMemory>	sbtBuffer;
	de::MovePtr<BufferWithMemory>	raygenShaderBindingTable;
	de::MovePtr<BufferWithMemory>	missShaderBindingTable;
	de::MovePtr<BufferWithMemory>	hitShaderBindingTable;
	de::MovePtr<BufferWithMemory>	callableShaderBindingTable;
	de::MovePtr<RayTracingPipeline>	rayTracingPipeline;

	if (m_data.stage == STAGE_COMPUTE)
	{
		const Unique<VkShaderModule>	shader(createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0));

		const VkPipelineShaderStageCreateInfo	shaderCreateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			DE_NULL,
			(VkPipelineShaderStageCreateFlags)0,
			VK_SHADER_STAGE_COMPUTE_BIT,								// stage
			*shader,													// shader
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
		pipeline = createComputePipeline(vk, device, DE_NULL, &pipelineCreateInfo, NULL);
	}
	else if (m_data.stage == STAGE_RAYGEN_NV)
	{
		const Unique<VkShaderModule>	shader(createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0));

		const VkPipelineShaderStageCreateInfo	shaderCreateInfo	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,		//  VkStructureType						sType;
			DE_NULL,													//  const void*							pNext;
			(VkPipelineShaderStageCreateFlags)0,						//  VkPipelineShaderStageCreateFlags	flags;
			VK_SHADER_STAGE_RAYGEN_BIT_NV,								//  VkShaderStageFlagBits				stage;
			*shader,													//  VkShaderModule						module;
			"main",														//  const char*							pName;
			DE_NULL,													//  const VkSpecializationInfo*			pSpecializationInfo;
		};

		VkRayTracingShaderGroupCreateInfoNV		group				=
		{
			VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV,	//  VkStructureType					sType;
			DE_NULL,													//  const void*						pNext;
			VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV,				//  VkRayTracingShaderGroupTypeNV	type;
			0,															//  deUint32						generalShader;
			VK_SHADER_UNUSED_NV,										//  deUint32						closestHitShader;
			VK_SHADER_UNUSED_NV,										//  deUint32						anyHitShader;
			VK_SHADER_UNUSED_NV,										//  deUint32						intersectionShader;
		};

		VkRayTracingPipelineCreateInfoNV		pipelineCreateInfo	=
		{
			VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_NV,	//  VkStructureType								sType;
			DE_NULL,												//  const void*									pNext;
			0,														//  VkPipelineCreateFlags						flags;
			1,														//  deUint32									stageCount;
			&shaderCreateInfo,										//  const VkPipelineShaderStageCreateInfo*		pStages;
			1,														//  deUint32									groupCount;
			&group,													//  const VkRayTracingShaderGroupCreateInfoNV*	pGroups;
			0,														//  deUint32									maxRecursionDepth;
			*pipelineLayout,										//  VkPipelineLayout							layout;
			(vk::VkPipeline)0,										//  VkPipeline									basePipelineHandle;
			0u,														//  deInt32										basePipelineIndex;
		};

		pipeline = createRayTracingPipelineNV(vk, device, DE_NULL, &pipelineCreateInfo, NULL);

		const auto allocSize = de::roundUp(static_cast<VkDeviceSize>(shaderGroupHandleSize), properties.properties.limits.nonCoherentAtomSize);

		sbtBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(vk, device, allocator,
			makeBufferCreateInfo(allocSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_RAY_TRACING_BIT_NV), MemoryRequirement::HostVisible));

		const auto&	alloc	= sbtBuffer->getAllocation();
		const auto	ptr		= reinterpret_cast<deUint32*>(alloc.getHostPtr());

		invalidateAlloc(vk, device, alloc);
		vk.getRayTracingShaderGroupHandlesNV(device, *pipeline, 0, 1, static_cast<deUintptr>(allocSize), ptr);
	}
	else if (m_data.stage == STAGE_RAYGEN)
	{
		rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();

		rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0), 0);

		pipeline = rayTracingPipeline->createPipeline(vk, device, *pipelineLayout);

		raygenShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
		raygenShaderBindingTableRegion			= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vk, device, raygenShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	}
	else if (m_data.stage == STAGE_INTERSECT)
	{
		rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();

		rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,		createShaderModule(vk, device, m_context.getBinaryCollection().get("rgen"), 0), 0);
		rayTracingPipeline->addShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR,	createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0), 1);

		pipeline = rayTracingPipeline->createPipeline(vk, device, *pipelineLayout);

		raygenShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
		raygenShaderBindingTableRegion			= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vk, device, raygenShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);

		hitShaderBindingTable					= rayTracingPipeline->createShaderBindingTable(vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1);
		hitShaderBindingTableRegion				= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vk, device, hitShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	}
	else if (m_data.stage == STAGE_ANY_HIT)
	{
		rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();

		rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,		createShaderModule(vk, device, m_context.getBinaryCollection().get("rgen"), 0), 0);
		rayTracingPipeline->addShader(VK_SHADER_STAGE_ANY_HIT_BIT_KHR,		createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0), 1);

		pipeline = rayTracingPipeline->createPipeline(vk, device, *pipelineLayout);

		raygenShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
		raygenShaderBindingTableRegion			= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vk, device, raygenShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);

		hitShaderBindingTable					= rayTracingPipeline->createShaderBindingTable(vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1);
		hitShaderBindingTableRegion				= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vk, device, hitShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	}
	else if (m_data.stage == STAGE_CLOSEST_HIT)
	{
		rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();

		rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,		createShaderModule(vk, device, m_context.getBinaryCollection().get("rgen"), 0), 0);
		rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,	createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0), 1);

		pipeline = rayTracingPipeline->createPipeline(vk, device, *pipelineLayout);

		raygenShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
		raygenShaderBindingTableRegion			= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vk, device, raygenShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);

		hitShaderBindingTable					= rayTracingPipeline->createShaderBindingTable(vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1);
		hitShaderBindingTableRegion				= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vk, device, hitShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	}
	else if (m_data.stage == STAGE_MISS)
	{
		rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();

		rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,	createShaderModule(vk, device, m_context.getBinaryCollection().get("rgen"), 0), 0);
		rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR,		createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0), 1);

		pipeline = rayTracingPipeline->createPipeline(vk, device, *pipelineLayout);

		raygenShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
		raygenShaderBindingTableRegion			= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vk, device, raygenShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);

		missShaderBindingTable					= rayTracingPipeline->createShaderBindingTable(vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1);
		missShaderBindingTableRegion			= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vk, device, missShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	}
	else if (m_data.stage == STAGE_CALLABLE)
	{
		rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();

		rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,	createShaderModule(vk, device, m_context.getBinaryCollection().get("rgen"), 0), 0);
		rayTracingPipeline->addShader(VK_SHADER_STAGE_CALLABLE_BIT_KHR,	createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0), 1);

		pipeline = rayTracingPipeline->createPipeline(vk, device, *pipelineLayout);

		raygenShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
		raygenShaderBindingTableRegion			= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vk, device, raygenShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);

		callableShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1);
		callableShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vk, device, callableShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	}
	else
	{
		const VkAttachmentDescription	attachmentDescription	=
		{
			// Input attachment
			(VkAttachmentDescriptionFlags)0,			// VkAttachmentDescriptionFlags	flags
			VK_FORMAT_R32_SINT,							// VkFormat						format
			VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits		samples
			VK_ATTACHMENT_LOAD_OP_LOAD,					// VkAttachmentLoadOp			loadOp
			VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp			storeOp
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp			stencilLoadOp
			VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp			stencilStoreOp
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,	// VkImageLayout				initialLayout
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL	// VkImageLayout				finalLayout
		};

		vector<VkAttachmentDescription> attachmentDescriptions	(inputAttachments.size(), attachmentDescription);
		vector<VkAttachmentReference>	attachmentReferences;

		attachmentReferences.reserve(inputAttachments.size());
		VkAttachmentReference attachmentReference =
		{
			0u,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};
		for (size_t i = 0; i < inputAttachments.size(); ++i)
		{
			attachmentReference.attachment = static_cast<deUint32>(i);
			attachmentReferences.push_back(attachmentReference);
		}

		const VkSubpassDescription		subpassDesc				=
		{
			(VkSubpassDescriptionFlags)0,											// VkSubpassDescriptionFlags	flags
			VK_PIPELINE_BIND_POINT_GRAPHICS,										// VkPipelineBindPoint			pipelineBindPoint
			static_cast<deUint32>(attachmentReferences.size()),						// deUint32						inputAttachmentCount
			(attachmentReferences.empty() ? DE_NULL : attachmentReferences.data()),	// const VkAttachmentReference*	pInputAttachments
			0u,																		// deUint32						colorAttachmentCount
			DE_NULL,																// const VkAttachmentReference*	pColorAttachments
			DE_NULL,																// const VkAttachmentReference*	pResolveAttachments
			DE_NULL,																// const VkAttachmentReference*	pDepthStencilAttachment
			0u,																		// deUint32						preserveAttachmentCount
			DE_NULL																	// const deUint32*				pPreserveAttachments
		};

		const VkSubpassDependency		subpassDependency		=
		{
			VK_SUBPASS_EXTERNAL,							// deUint32				srcSubpass
			0,												// deUint32				dstSubpass
			VK_PIPELINE_STAGE_TRANSFER_BIT,					// VkPipelineStageFlags	srcStageMask
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // dstStageMask
			VK_ACCESS_TRANSFER_WRITE_BIT,					// VkAccessFlags		srcAccessMask
			VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT  | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,	//	dstAccessMask
			VK_DEPENDENCY_BY_REGION_BIT						// VkDependencyFlags	dependencyFlags
		};

		const VkRenderPassCreateInfo	renderPassParams		=
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,				// VkStructureTypei					sType
			DE_NULL,												// const void*						pNext
			(VkRenderPassCreateFlags)0,								// VkRenderPassCreateFlags			flags
			static_cast<deUint32>(attachmentDescriptions.size()),	// deUint32							attachmentCount
			attachmentDescriptions.data(),							// const VkAttachmentDescription*	pAttachments
			1u,														// deUint32							subpassCount
			&subpassDesc,											// const VkSubpassDescription*		pSubpasses
			1u,														// deUint32							dependencyCount
			&subpassDependency										// const VkSubpassDependency*		pDependencies
		};

		renderPass = createRenderPass(vk, device, &renderPassParams);

		vector<VkImageView> rawInputAttachmentViews;
		rawInputAttachmentViews.reserve(inputAttachmentViews.size());
		transform(begin(inputAttachmentViews), end(inputAttachmentViews), back_inserter(rawInputAttachmentViews),
				  [](const Move<VkImageView>& ptr) { return ptr.get(); });

		const vk::VkFramebufferCreateInfo	framebufferParams	=
		{
			vk::VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,			// sType
			DE_NULL,												// pNext
			(vk::VkFramebufferCreateFlags)0,
			*renderPass,											// renderPass
			static_cast<deUint32>(rawInputAttachmentViews.size()),	// attachmentCount
			rawInputAttachmentViews.data(),							// pAttachments
			DIM,													// width
			DIM,													// height
			1u,														// layers
		};

		framebuffer = createFramebuffer(vk, device, &framebufferParams);

		const VkPipelineVertexInputStateCreateInfo		vertexInputStateCreateInfo		=
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,													// const void*								pNext;
			(VkPipelineVertexInputStateCreateFlags)0,					// VkPipelineVertexInputStateCreateFlags	flags;
			0u,															// deUint32									vertexBindingDescriptionCount;
			DE_NULL,													// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
			0u,															// deUint32									vertexAttributeDescriptionCount;
			DE_NULL														// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
		};

		const VkPipelineInputAssemblyStateCreateInfo	inputAssemblyStateCreateInfo	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			(VkPipelineInputAssemblyStateCreateFlags)0,						// VkPipelineInputAssemblyStateCreateFlags	flags;
			(m_data.stage == STAGE_VERTEX) ? VK_PRIMITIVE_TOPOLOGY_POINT_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // VkPrimitiveTopology						topology;
			VK_FALSE														// VkBool32									primitiveRestartEnable;
		};

		const VkPipelineRasterizationStateCreateInfo	rasterizationStateCreateInfo	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			(VkPipelineRasterizationStateCreateFlags)0,						// VkPipelineRasterizationStateCreateFlags	flags;
			VK_FALSE,														// VkBool32									depthClampEnable;
			(m_data.stage == STAGE_VERTEX) ? VK_TRUE : VK_FALSE,			// VkBool32									rasterizerDiscardEnable;
			VK_POLYGON_MODE_FILL,											// VkPolygonMode							polygonMode;
			VK_CULL_MODE_NONE,												// VkCullModeFlags							cullMode;
			VK_FRONT_FACE_CLOCKWISE,										// VkFrontFace								frontFace;
			VK_FALSE,														// VkBool32									depthBiasEnable;
			0.0f,															// float									depthBiasConstantFactor;
			0.0f,															// float									depthBiasClamp;
			0.0f,															// float									depthBiasSlopeFactor;
			1.0f															// float									lineWidth;
		};

		const VkPipelineMultisampleStateCreateInfo		multisampleStateCreateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType							sType
			DE_NULL,													// const void*								pNext
			0u,															// VkPipelineMultisampleStateCreateFlags	flags
			VK_SAMPLE_COUNT_1_BIT,										// VkSampleCountFlagBits					rasterizationSamples
			VK_FALSE,													// VkBool32									sampleShadingEnable
			1.0f,														// float									minSampleShading
			DE_NULL,													// const VkSampleMask*						pSampleMask
			VK_FALSE,													// VkBool32									alphaToCoverageEnable
			VK_FALSE													// VkBool32									alphaToOneEnable
		};

		VkViewport viewport = makeViewport(DIM, DIM);
		VkRect2D scissor = makeRect2D(DIM, DIM);

		const VkPipelineViewportStateCreateInfo			viewportStateCreateInfo				=
		{
			VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,	// VkStructureType							sType
			DE_NULL,												// const void*								pNext
			(VkPipelineViewportStateCreateFlags)0,					// VkPipelineViewportStateCreateFlags		flags
			1u,														// deUint32									viewportCount
			&viewport,												// const VkViewport*						pViewports
			1u,														// deUint32									scissorCount
			&scissor												// const VkRect2D*							pScissors
		};

		Move<VkShaderModule> fs;
		Move<VkShaderModule> vs;

		deUint32 numStages;
		if (m_data.stage == STAGE_VERTEX)
		{
			vs = createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0);
			fs = createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0); // bogus
			numStages = 1u;
		}
		else
		{
			vs = createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0);
			fs = createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0);
			numStages = 2u;
		}

		const VkPipelineShaderStageCreateInfo	shaderCreateInfo[2] =
		{
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				DE_NULL,
				(VkPipelineShaderStageCreateFlags)0,
				VK_SHADER_STAGE_VERTEX_BIT,									// stage
				*vs,														// shader
				"main",
				DE_NULL,													// pSpecializationInfo
			},
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				DE_NULL,
				(VkPipelineShaderStageCreateFlags)0,
				VK_SHADER_STAGE_FRAGMENT_BIT,								// stage
				*fs,														// shader
				"main",
				DE_NULL,													// pSpecializationInfo
			}
		};

		const VkGraphicsPipelineCreateInfo				graphicsPipelineCreateInfo		=
		{
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,	// VkStructureType									sType;
			DE_NULL,											// const void*										pNext;
			(VkPipelineCreateFlags)0,							// VkPipelineCreateFlags							flags;
			numStages,											// deUint32											stageCount;
			&shaderCreateInfo[0],								// const VkPipelineShaderStageCreateInfo*			pStages;
			&vertexInputStateCreateInfo,						// const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
			&inputAssemblyStateCreateInfo,						// const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
			DE_NULL,											// const VkPipelineTessellationStateCreateInfo*		pTessellationState;
			&viewportStateCreateInfo,							// const VkPipelineViewportStateCreateInfo*			pViewportState;
			&rasterizationStateCreateInfo,						// const VkPipelineRasterizationStateCreateInfo*	pRasterizationState;
			&multisampleStateCreateInfo,						// const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
			DE_NULL,											// const VkPipelineDepthStencilStateCreateInfo*		pDepthStencilState;
			DE_NULL,											// const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
			DE_NULL,											// const VkPipelineDynamicStateCreateInfo*			pDynamicState;
			pipelineLayout.get(),								// VkPipelineLayout									layout;
			renderPass.get(),									// VkRenderPass										renderPass;
			0u,													// deUint32											subpass;
			DE_NULL,											// VkPipeline										basePipelineHandle;
			0													// int												basePipelineIndex;
		};

		pipeline = createGraphicsPipeline(vk, device, DE_NULL, &graphicsPipelineCreateInfo);
	}

	const VkImageMemoryBarrier imageBarrier =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,				// VkStructureType		sType
		DE_NULL,											// const void*			pNext
		0u,													// VkAccessFlags		srcAccessMask
		VK_ACCESS_TRANSFER_WRITE_BIT,						// VkAccessFlags		dstAccessMask
		VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout		oldLayout
		VK_IMAGE_LAYOUT_GENERAL,							// VkImageLayout		newLayout
		VK_QUEUE_FAMILY_IGNORED,							// uint32_t				srcQueueFamilyIndex
		VK_QUEUE_FAMILY_IGNORED,							// uint32_t				dstQueueFamilyIndex
		**image,											// VkImage				image
		{
			VK_IMAGE_ASPECT_COLOR_BIT,				// VkImageAspectFlags	aspectMask
			0u,										// uint32_t				baseMipLevel
			1u,										// uint32_t				mipLevels,
			0u,										// uint32_t				baseArray
			1u,										// uint32_t				arraySize
		}
	};

	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
							(VkDependencyFlags)0,
							0, (const VkMemoryBarrier*)DE_NULL,
							0, (const VkBufferMemoryBarrier*)DE_NULL,
							1, &imageBarrier);

	vk.cmdBindPipeline(*cmdBuffer, bindPoint, *pipeline);

	VkImageSubresourceRange range = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	VkClearValue clearColor = makeClearValueColorU32(0,0,0,0);

	VkMemoryBarrier					memBarrier =
	{
		VK_STRUCTURE_TYPE_MEMORY_BARRIER,	// sType
		DE_NULL,							// pNext
		0u,									// srcAccessMask
		0u,									// dstAccessMask
	};

	vk.cmdClearColorImage(*cmdBuffer, **image, VK_IMAGE_LAYOUT_GENERAL, &clearColor.color, 1, &range);

	memBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	memBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, m_data.allPipelineStages,
		0, 1, &memBarrier, 0, DE_NULL, 0, DE_NULL);

	if (m_data.stage == STAGE_COMPUTE)
	{
		vk.cmdDispatch(*cmdBuffer, DIM, DIM, 1);
	}
	else if (m_data.stage == STAGE_RAYGEN_NV)
	{
		vk.cmdTraceRaysNV(*cmdBuffer,
			**sbtBuffer, 0,
			DE_NULL, 0, 0,
			DE_NULL, 0, 0,
			DE_NULL, 0, 0,
			DIM, DIM, 1);
	}
	else if (isRayTracingStageKHR(m_data.stage))
	{
		cmdTraceRays(vk,
			*cmdBuffer,
			&raygenShaderBindingTableRegion,
			&missShaderBindingTableRegion,
			&hitShaderBindingTableRegion,
			&callableShaderBindingTableRegion,
			DIM, DIM, 1);
	}
	else
	{
		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer,
						makeRect2D(DIM, DIM),
						0, DE_NULL, VK_SUBPASS_CONTENTS_INLINE);
		// Draw a point cloud for vertex shader testing, and a single quad for fragment shader testing
		if (m_data.stage == STAGE_VERTEX)
		{
			vk.cmdDraw(*cmdBuffer, DIM*DIM, 1u, 0u, 0u);
		}
		else
		{
			vk.cmdDraw(*cmdBuffer, 4u, 1u, 0u, 0u);
		}
		endRenderPass(vk, *cmdBuffer);
	}

	memBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	memBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
	vk.cmdPipelineBarrier(*cmdBuffer, m_data.allPipelineStages, VK_PIPELINE_STAGE_TRANSFER_BIT,
		0, 1, &memBarrier, 0, DE_NULL, 0, DE_NULL);

	const VkBufferImageCopy copyRegion = makeBufferImageCopy(makeExtent3D(DIM, DIM, 1u),
															 makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u));
	vk.cmdCopyImageToBuffer(*cmdBuffer, **image, VK_IMAGE_LAYOUT_GENERAL, **copyBuffer, 1u, &copyRegion);

	const VkBufferMemoryBarrier copyBufferBarrier =
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType	sType;
		DE_NULL,									// const void*		pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags	srcAccessMask;
		VK_ACCESS_HOST_READ_BIT,					// VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			dstQueueFamilyIndex;
		**copyBuffer,								// VkBuffer			buffer;
		0u,											// VkDeviceSize		offset;
		VK_WHOLE_SIZE,								// VkDeviceSize		size;
	};

	// Add a barrier to read the copy buffer after copying.
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0u, DE_NULL, 1u, &copyBufferBarrier, 0u, DE_NULL);

	// Copy all storage images to the storage image buffer.
	VkBufferImageCopy storageImgCopyRegion =
	{
		0u,																	// VkDeviceSize                bufferOffset;
		0u,																	// uint32_t                    bufferRowLength;
		0u,																	// uint32_t                    bufferImageHeight;
		makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u),	// VkImageSubresourceLayers    imageSubresource;
		makeOffset3D(0, 0, 0),												// VkOffset3D                  imageOffset;
		makeExtent3D(1u, 1u, 1u),											// VkExtent3D                  imageExtent;
	};

	for (deUint32 i = 0; i < storageImageCount; ++i)
	{
		storageImgCopyRegion.bufferOffset = sizeof(deInt32) * i;
		vk.cmdCopyImageToBuffer(*cmdBuffer, storageImages[i].get(), VK_IMAGE_LAYOUT_GENERAL, **storageImgBuffer, 1u, &storageImgCopyRegion);
	}

	const VkBufferMemoryBarrier storageImgBufferBarrier =
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType	sType;
		DE_NULL,									// const void*		pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags	srcAccessMask;
		VK_ACCESS_HOST_READ_BIT,					// VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			dstQueueFamilyIndex;
		**storageImgBuffer,							// VkBuffer			buffer;
		0u,											// VkDeviceSize		offset;
		VK_WHOLE_SIZE,								// VkDeviceSize		size;
	};

	// Add a barrier to read the storage image buffer after copying.
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0u, DE_NULL, 1u, &storageImgBufferBarrier, 0u, DE_NULL);

	const VkBufferMemoryBarrier descriptorBufferBarrier =
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,					// VkStructureType	sType;
		DE_NULL,													// const void*		pNext;
		(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT),	// VkAccessFlags	srcAccessMask;
		VK_ACCESS_HOST_READ_BIT,									// VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,									// deUint32			srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,									// deUint32			dstQueueFamilyIndex;
		**buffer,													// VkBuffer			buffer;
		0u,															// VkDeviceSize		offset;
		VK_WHOLE_SIZE,												// VkDeviceSize		size;
	};

	// Add a barrier to read stored data from shader writes in descriptor memory for other types of descriptors.
	vk.cmdPipelineBarrier(*cmdBuffer, m_data.allPipelineStages, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, nullptr, 1u, &descriptorBufferBarrier, 0u, nullptr);

	endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

	// Verify output image.
	deUint32 *ptr = (deUint32 *)copyBuffer->getAllocation().getHostPtr();
	invalidateAlloc(vk, device, copyBuffer->getAllocation());

	deUint32	failures = 0;
	auto&		log = m_context.getTestContext().getLog();

	for (deUint32 i = 0; i < DIM*DIM; ++i)
	{
		if (ptr[i] != 1)
		{
			failures++;
			log << tcu::TestLog::Message << "Failure in copy buffer, ptr[" << i << "] = " << ptr[i] << tcu::TestLog::EndMessage;
		}
	}

	// Verify descriptors with writes.
	invalidateMappedMemoryRange(vk, device, buffer->getAllocation().getMemory(), buffer->getAllocation().getOffset(), VK_WHOLE_SIZE);
	invalidateMappedMemoryRange(vk, device, storageImgBuffer->getAllocation().getMemory(), storageImgBuffer->getAllocation().getOffset(), VK_WHOLE_SIZE);

	for (const auto& descIdWriteInfo : randomLayout.descriptorWrites)
	{
		const auto& writeInfo = descIdWriteInfo.second;
		if (writeInfo.writeGenerated && *writeInfo.ptr != writeInfo.expected)
		{
			failures++;
			log << tcu::TestLog::Message << "Failure in write operation; expected " << writeInfo.expected << " and found " << *writeInfo.ptr << tcu::TestLog::EndMessage;
		}
	}

	if (failures == 0)
		return tcu::TestStatus::pass("Pass");
	else
		return tcu::TestStatus::fail("Fail (failures=" + de::toString(failures) + ")");
}

}	// anonymous

tcu::TestCaseGroup*	createDescriptorSetRandomTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "descriptorset_random", "Randomly-generated desciptor set layouts"));

	deUint32 seed = 0;

	typedef struct
	{
		deUint32				count;
		const char*				name;
		const char*				description;
	} TestGroupCase;

	TestGroupCase setsCases[] =
	{
		{ 4,	"sets4",	"4 descriptor sets"		},
		{ 8,	"sets8",	"8 descriptor sets"		},
		{ 16,	"sets16",	"16 descriptor sets"	},
		{ 32,	"sets32",	"32 descriptor sets"	},
	};

	TestGroupCase indexCases[] =
	{
		{ INDEX_TYPE_NONE,			"noarray",		"all descriptor declarations are not arrays"		},
		{ INDEX_TYPE_CONSTANT,		"constant",		"constant indexing of descriptor arrays"			},
		{ INDEX_TYPE_PUSHCONSTANT,	"unifindexed",	"indexing descriptor arrays with push constants"	},
		{ INDEX_TYPE_DEPENDENT,		"dynindexed",	"dynamically uniform indexing descriptor arrays"	},
		{ INDEX_TYPE_RUNTIME_SIZE,	"runtimesize",	"runtime-size declarations of descriptor arrays"	},
	};

	TestGroupCase uboCases[] =
	{
		{ 0,			"noubo",			"no ubos"					},
		{ 12,			"ubolimitlow",		"spec minmax ubo limit"		},
		{ 4096,			"ubolimithigh",		"high ubo limit"			},
	};

	TestGroupCase sboCases[] =
	{
		{ 0,			"nosbo",			"no ssbos"					},
		{ 4,			"sbolimitlow",		"spec minmax ssbo limit"	},
		{ 4096,			"sbolimithigh",		"high ssbo limit"			},
	};

	TestGroupCase iaCases[] =
	{
		{ 0,			"noia",				"no input attachments"					},
		{ 4,			"ialimitlow",		"spec minmax input attachment limit"	},
		{ 64,			"ialimithigh",		"high input attachment limit"			},
	};

	TestGroupCase sampledImgCases[] =
	{
		{ 0,			"nosampledimg",		"no sampled images"			},
		{ 16,			"sampledimglow",	"spec minmax image limit"	},
		{ 4096,			"sampledimghigh",	"high image limit"			},
	};

	const struct
	{
		deUint32	sImgCount;
		deUint32	sTexCount;
		const char* name;
		const char* description;
	} sImgTexCases[] =
	{
		{ 1,		0,		"outimgonly",		"output storage image only"							},
		{ 1,		3,		"outimgtexlow",		"output image low storage tex limit"				},
		{ 4,		0,		"lowimgnotex",		"minmax storage images and no storage tex"			},
		{ 3,		1,		"lowimgsingletex",	"low storage image single storage texel"			},
		{ 2048,		2048,	"storageimghigh",	"high limit of storage images and texel buffers"	},
	};

	const struct
	{
		deUint32				iubCount;
		deUint32				iubSize;
		const char*				name;
		const char*				description;
	} iubCases[] =
	{
		{ 0, 0,		"noiub",			"no inline uniform blocks"			},
		{ 4, 256,	"iublimitlow",		"inline uniform blocks low limit"	},
		{ 8, 4096,	"iublimithigh",		"inline uniform blocks high limit"	},
	};

	TestGroupCase stageCases[] =
	{
		{ STAGE_COMPUTE,	"comp",		"compute"		},
		{ STAGE_FRAGMENT,	"frag",		"fragment"		},
		{ STAGE_VERTEX,		"vert",		"vertex"		},
		{ STAGE_RAYGEN_NV,	"rgnv",		"raygen_nv"		},
		{ STAGE_RAYGEN,		"rgen",		"raygen"		},
		{ STAGE_INTERSECT,	"sect",		"intersect"		},
		{ STAGE_ANY_HIT,	"ahit",		"any_hit"		},
		{ STAGE_CLOSEST_HIT,"chit",		"closest_hit"	},
		{ STAGE_MISS,		"miss",		"miss"			},
		{ STAGE_CALLABLE,	"call",		"callable"		},
	};

	TestGroupCase uabCases[] =
	{
		{ UPDATE_AFTER_BIND_DISABLED,	"nouab",	"no update after bind"		},
		{ UPDATE_AFTER_BIND_ENABLED,	"uab",		"enable update after bind"	},
	};

	for (int setsNdx = 0; setsNdx < DE_LENGTH_OF_ARRAY(setsCases); setsNdx++)
	{
		de::MovePtr<tcu::TestCaseGroup> setsGroup(new tcu::TestCaseGroup(testCtx, setsCases[setsNdx].name, setsCases[setsNdx].description));
		for (int indexNdx = 0; indexNdx < DE_LENGTH_OF_ARRAY(indexCases); indexNdx++)
		{
			de::MovePtr<tcu::TestCaseGroup> indexGroup(new tcu::TestCaseGroup(testCtx, indexCases[indexNdx].name, indexCases[indexNdx].description));
			for (int uboNdx = 0; uboNdx < DE_LENGTH_OF_ARRAY(uboCases); uboNdx++)
			{
				de::MovePtr<tcu::TestCaseGroup> uboGroup(new tcu::TestCaseGroup(testCtx, uboCases[uboNdx].name, uboCases[uboNdx].description));
				for (int sboNdx = 0; sboNdx < DE_LENGTH_OF_ARRAY(sboCases); sboNdx++)
				{
					de::MovePtr<tcu::TestCaseGroup> sboGroup(new tcu::TestCaseGroup(testCtx, sboCases[sboNdx].name, sboCases[sboNdx].description));
					for (int sampledImgNdx = 0; sampledImgNdx < DE_LENGTH_OF_ARRAY(sampledImgCases); sampledImgNdx++)
					{
						de::MovePtr<tcu::TestCaseGroup> sampledImgGroup(new tcu::TestCaseGroup(testCtx, sampledImgCases[sampledImgNdx].name, sampledImgCases[sampledImgNdx].description));
						for (int storageImgNdx = 0; storageImgNdx < DE_LENGTH_OF_ARRAY(sImgTexCases); ++storageImgNdx)
						{
							de::MovePtr<tcu::TestCaseGroup> storageImgGroup(new tcu::TestCaseGroup(testCtx, sImgTexCases[storageImgNdx].name, sImgTexCases[storageImgNdx].description));
							for (int iubNdx = 0; iubNdx < DE_LENGTH_OF_ARRAY(iubCases); iubNdx++)
							{
								de::MovePtr<tcu::TestCaseGroup> iubGroup(new tcu::TestCaseGroup(testCtx, iubCases[iubNdx].name, iubCases[iubNdx].description));
								for (int uabNdx = 0; uabNdx < DE_LENGTH_OF_ARRAY(uabCases); uabNdx++)
								{
									de::MovePtr<tcu::TestCaseGroup> uabGroup(new tcu::TestCaseGroup(testCtx, uabCases[uabNdx].name, uabCases[uabNdx].description));
									for (int stageNdx = 0; stageNdx < DE_LENGTH_OF_ARRAY(stageCases); stageNdx++)
									{
										const Stage		currentStage			= static_cast<Stage>(stageCases[stageNdx].count);
										const VkFlags	rtShaderStagesNV		= currentStage == STAGE_RAYGEN_NV ? VK_SHADER_STAGE_RAYGEN_BIT_NV : 0;
										const VkFlags	rtPipelineStagesNV		= currentStage == STAGE_RAYGEN_NV ? VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV : 0;
										const VkFlags	rtShaderStagesKHR		= isRayTracingStageKHR(currentStage) ? ALL_RAY_TRACING_STAGES : 0;
										const VkFlags	rtPipelineStagesKHR		= isRayTracingStageKHR(currentStage) ? VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR : 0;
										const VkFlags	rtShaderStages			= rtShaderStagesNV | rtShaderStagesKHR;
										const VkFlags	rtPipelineStages		= rtPipelineStagesNV | rtPipelineStagesKHR;

										de::MovePtr<tcu::TestCaseGroup> stageGroup(new tcu::TestCaseGroup(testCtx, stageCases[stageNdx].name, stageCases[stageNdx].description));
										for (int iaNdx = 0; iaNdx < DE_LENGTH_OF_ARRAY(iaCases); ++iaNdx)
										{
											// Input attachments can only be used in the fragment stage.
											if (currentStage != STAGE_FRAGMENT && iaCases[iaNdx].count > 0u)
												continue;

											// Allow only one high limit or all of them.
											deUint32 highLimitCount = 0u;
											if (uboNdx == DE_LENGTH_OF_ARRAY(uboCases) - 1)					++highLimitCount;
											if (sboNdx == DE_LENGTH_OF_ARRAY(sboCases) - 1)					++highLimitCount;
											if (sampledImgNdx == DE_LENGTH_OF_ARRAY(sampledImgCases) - 1)	++highLimitCount;
											if (storageImgNdx == DE_LENGTH_OF_ARRAY(sImgTexCases) - 1)		++highLimitCount;
											if (iaNdx == DE_LENGTH_OF_ARRAY(iaCases) - 1)					++highLimitCount;

											if (highLimitCount > 1 && highLimitCount < 5)
												continue;

											// Allow only all, all-but-one, none or one "zero limits" at the same time, except for inline uniform blocks.
											deUint32 zeroLimitCount = 0u;
											if (uboNdx == 0)			++zeroLimitCount;
											if (sboNdx == 0)			++zeroLimitCount;
											if (sampledImgNdx == 0)		++zeroLimitCount;
											if (storageImgNdx == 0)		++zeroLimitCount;
											if (iaNdx == 0)				++zeroLimitCount;

											if (zeroLimitCount > 1 && zeroLimitCount < 4)
												continue;

											// Avoid using multiple storage images if no dynamic indexing is being used.
											if (storageImgNdx >= 2 && indexNdx < 2)
												continue;

											// Skip the case of no UBOs, SSBOs or sampled images when no dynamic indexing is being used.
											if ((uboNdx == 0 || sboNdx == 0 || sampledImgNdx == 0) && indexNdx < 2)
												continue;

											de::MovePtr<tcu::TestCaseGroup> iaGroup(new tcu::TestCaseGroup(testCtx, iaCases[iaNdx].name, iaCases[iaNdx].description));

											// Generate 10 random cases when working with only 4 sets and the number of descriptors is low. Otherwise just one case.
											// Exception: the case of no descriptors of any kind only needs one case.
											const deUint32 numSeeds = (setsCases[setsNdx].count == 4 && uboNdx < 2 && sboNdx < 2 && sampledImgNdx < 2 && storageImgNdx < 4 && iubNdx == 0 && iaNdx < 2 &&
																	(uboNdx != 0 || sboNdx != 0 || sampledImgNdx != 0 || storageImgNdx != 0 || iaNdx != 0)) ? 10 : 1;

											for (deUint32 rnd = 0; rnd < numSeeds; ++rnd)
											{
												const VkFlags allShaderStages	= VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
												const VkFlags allPipelineStages	= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

												CaseDef c =
												{
													(IndexType)indexCases[indexNdx].count,						// IndexType indexType;
													setsCases[setsNdx].count,									// deUint32 numDescriptorSets;
													uboCases[uboNdx].count,										// deUint32 maxPerStageUniformBuffers;
													8,															// deUint32 maxUniformBuffersDynamic;
													sboCases[sboNdx].count,										// deUint32 maxPerStageStorageBuffers;
													4,															// deUint32 maxStorageBuffersDynamic;
													sampledImgCases[sampledImgNdx].count,						// deUint32 maxPerStageSampledImages;
													sImgTexCases[storageImgNdx].sImgCount,						// deUint32 maxPerStageStorageImages;
													sImgTexCases[storageImgNdx].sTexCount,						// deUint32 maxPerStageStorageTexelBuffers;
													iubCases[iubNdx].iubCount,									// deUint32 maxInlineUniformBlocks;
													iubCases[iubNdx].iubSize,									// deUint32 maxInlineUniformBlockSize;
													iaCases[iaNdx].count,										// deUint32 maxPerStageInputAttachments;
													currentStage,												// Stage stage;
													(UpdateAfterBind)uabCases[uabNdx].count,					// UpdateAfterBind uab;
													seed++,														// deUint32 seed;
													rtShaderStages ? rtShaderStages : allShaderStages,			// VkFlags allShaderStages;
													rtPipelineStages ? rtPipelineStages : allPipelineStages,	// VkFlags allPipelineStages;
													nullptr,													// std::shared_ptr<RandomLayout> randomLayout;
												};

												string name = de::toString(rnd);
												iaGroup->addChild(new DescriptorSetRandomTestCase(testCtx, name.c_str(), "test", c));
											}
											stageGroup->addChild(iaGroup.release());
										}
										uabGroup->addChild(stageGroup.release());
									}
									iubGroup->addChild(uabGroup.release());
								}
								storageImgGroup->addChild(iubGroup.release());
							}
							sampledImgGroup->addChild(storageImgGroup.release());
						}
						sboGroup->addChild(sampledImgGroup.release());
					}
					uboGroup->addChild(sboGroup.release());
				}
				indexGroup->addChild(uboGroup.release());
			}
			setsGroup->addChild(indexGroup.release());
		}
		group->addChild(setsGroup.release());
	}
	return group.release();
}

}	// BindingModel
}	// vkt
