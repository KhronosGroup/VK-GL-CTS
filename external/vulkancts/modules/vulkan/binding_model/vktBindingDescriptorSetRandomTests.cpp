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

namespace vkt
{
namespace BindingModel
{
namespace
{
using namespace vk;
using namespace std;

static const deUint32 DIM = 8;

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
} Stage;

typedef enum
{
	UPDATE_AFTER_BIND_DISABLED = 0,
	UPDATE_AFTER_BIND_ENABLED,
} UpdateAfterBind;

const VkFlags allShaderStages = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
const VkFlags allPipelineStages = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

struct CaseDef
{
	IndexType indexType;
	deUint32 numDescriptorSets;
	deUint32 maxPerStageUniformBuffers;
	deUint32 maxUniformBuffersDynamic;
	deUint32 maxPerStageStorageBuffers;
	deUint32 maxStorageBuffersDynamic;
	deUint32 maxPerStageSampledImages;
	deUint32 maxPerStageStorageImages;
	deUint32 maxInlineUniformBlocks;
	deUint32 maxInlineUniformBlockSize;
	Stage stage;
	UpdateAfterBind uab;
	deUint32 seed;
};


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
	vector<vector<VkDescriptorBindingFlagsEXT> > layoutBindingFlags;
	vector<vector<deUint32> > arraySizes;
	// size of the variable descriptor (last) binding in each set
	vector<deUint32> variableDescriptorSizes;

};


class DescriptorSetRandomTestInstance : public TestInstance
{
public:
						DescriptorSetRandomTestInstance		(Context& context, const CaseDef& data);
						~DescriptorSetRandomTestInstance	(void);
	tcu::TestStatus		iterate								(void);
private:
	CaseDef				m_data;

	enum
	{
		WIDTH = 256,
		HEIGHT = 256
	};
};

DescriptorSetRandomTestInstance::DescriptorSetRandomTestInstance (Context& context, const CaseDef& data)
	: vkt::TestInstance		(context)
	, m_data				(data)
{
}

DescriptorSetRandomTestInstance::~DescriptorSetRandomTestInstance (void)
{
}

class DescriptorSetRandomTestCase : public TestCase
{
	public:
								DescriptorSetRandomTestCase		(tcu::TestContext& context, const char* name, const char* desc, const CaseDef data);
								~DescriptorSetRandomTestCase	(void);
	virtual	void				initPrograms					(SourceCollections& programCollection) const;
	virtual TestInstance*		createInstance					(Context& context) const;
	virtual void				checkSupport					(Context& context) const;

private:
	CaseDef					m_data;
};

DescriptorSetRandomTestCase::DescriptorSetRandomTestCase (tcu::TestContext& context, const char* name, const char* desc, const CaseDef data)
	: vkt::TestCase	(context, name, desc)
	, m_data		(data)
{
}

DescriptorSetRandomTestCase::~DescriptorSetRandomTestCase	(void)
{
}

void DescriptorSetRandomTestCase::checkSupport(Context& context) const
{
	VkPhysicalDeviceInlineUniformBlockPropertiesEXT inlineUniformProperties;
	deMemset(&inlineUniformProperties, 0, sizeof(inlineUniformProperties));
	inlineUniformProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_PROPERTIES_EXT;

	VkPhysicalDeviceProperties2 properties;
	deMemset(&properties, 0, sizeof(properties));
	properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;

	if (isDeviceExtensionSupported(context.getUsedApiVersion(), context.getDeviceExtensions(), "VK_EXT_inline_uniform_block"))
	{
		properties.pNext = &inlineUniformProperties;
	}

	context.getInstanceInterface().getPhysicalDeviceProperties2(context.getPhysicalDevice(), &properties);

	VkPhysicalDeviceInlineUniformBlockFeaturesEXT inlineUniformFeatures;
	deMemset(&inlineUniformFeatures, 0, sizeof(inlineUniformFeatures));
	inlineUniformFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_FEATURES_EXT;

	VkPhysicalDeviceDescriptorIndexingFeaturesEXT indexingFeatures;
	deMemset(&indexingFeatures, 0, sizeof(indexingFeatures));
	indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;

	VkPhysicalDeviceFeatures2 features;
	deMemset(&features, 0, sizeof(features));
	features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

	if (isDeviceExtensionSupported(context.getUsedApiVersion(), context.getDeviceExtensions(), "VK_EXT_descriptor_indexing") &&
		isDeviceExtensionSupported(context.getUsedApiVersion(), context.getDeviceExtensions(), "VK_EXT_inline_uniform_block"))
	{
		indexingFeatures.pNext = &inlineUniformFeatures;
		features.pNext = &indexingFeatures;
	}
	else if (isDeviceExtensionSupported(context.getUsedApiVersion(), context.getDeviceExtensions(), "VK_EXT_descriptor_indexing"))
	{
		features.pNext = &indexingFeatures;
	}
	else if (isDeviceExtensionSupported(context.getUsedApiVersion(), context.getDeviceExtensions(), "VK_EXT_inline_uniform_block"))
	{
		features.pNext = &inlineUniformFeatures;
	}

	context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &features);
	if (m_data.stage == STAGE_VERTEX && !features.features.vertexPipelineStoresAndAtomics)
	{
		return TCU_THROW(NotSupportedError, "Vertex pipeline stores and atomics not supported");
	}

	if ((m_data.indexType == INDEX_TYPE_PUSHCONSTANT ||
		 m_data.indexType == INDEX_TYPE_DEPENDENT ||
		 m_data.indexType == INDEX_TYPE_RUNTIME_SIZE) &&
		(!features.features.shaderUniformBufferArrayDynamicIndexing ||
		 !features.features.shaderStorageBufferArrayDynamicIndexing ||
		 !features.features.shaderSampledImageArrayDynamicIndexing ||
		 !features.features.shaderStorageImageArrayDynamicIndexing ||
		 !indexingFeatures.shaderUniformTexelBufferArrayDynamicIndexing ||
		 !indexingFeatures.shaderStorageTexelBufferArrayDynamicIndexing))
	{
		TCU_THROW(NotSupportedError, "Dynamic indexing not supported");
	}

	if (m_data.numDescriptorSets > properties.properties.limits.maxBoundDescriptorSets)
	{
		TCU_THROW(NotSupportedError, "Number of descriptor sets not supported");
	}

	if ((m_data.maxPerStageUniformBuffers + m_data.maxPerStageStorageBuffers +
		m_data.maxPerStageSampledImages + m_data.maxPerStageStorageImages) >
		properties.properties.limits.maxPerStageResources)
	{
		TCU_THROW(NotSupportedError, "Number of descriptors not supported");
	}

	if (m_data.maxPerStageUniformBuffers > properties.properties.limits.maxPerStageDescriptorUniformBuffers ||
		m_data.maxPerStageStorageBuffers > properties.properties.limits.maxPerStageDescriptorStorageBuffers ||
		m_data.maxUniformBuffersDynamic  > properties.properties.limits.maxDescriptorSetUniformBuffersDynamic ||
		m_data.maxStorageBuffersDynamic  > properties.properties.limits.maxDescriptorSetStorageBuffersDynamic ||
		m_data.maxPerStageSampledImages  > properties.properties.limits.maxPerStageDescriptorSampledImages ||
		m_data.maxPerStageStorageImages  > properties.properties.limits.maxPerStageDescriptorStorageImages)
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

void generateRandomLayout(RandomLayout &randomLayout, const CaseDef &caseDef)
{
	deRandom rnd;
	deRandom_init(&rnd, caseDef.seed);

	// Count the number of each resource type, to avoid overflowing the limits.
	deUint32 numUBO = 0;
	deUint32 numUBODyn = 0;
	deUint32 numSSBO = 0;
	deUint32 numSSBODyn = 0;
	deUint32 numImage = 0;
	deUint32 numTexBuffer = 0;
	deUint32 numInlineUniformBlocks = 0;

	// TODO: Consider varying these
	deUint32 minBindings = 0;
	deUint32 maxBindings = 32;
	// No larger than 32 elements for dynamic indexing tests, due to 128B limit
	// for push constants (used for the indices)
	deUint32 maxArray = caseDef.indexType == INDEX_TYPE_NONE ? 0 : 32;

	// Each set has a random number of bindings, each binding has a random
	// array size and a random descriptor type.
	for (deUint32 s = 0; s < caseDef.numDescriptorSets; ++s)
	{
		vector<VkDescriptorSetLayoutBinding> &bindings = randomLayout.layoutBindings[s];
		vector<VkDescriptorBindingFlagsEXT> &bindingsFlags = randomLayout.layoutBindingFlags[s];
		vector<deUint32> &arraySizes = randomLayout.arraySizes[s];
		int numBindings = randRange(&rnd, minBindings, maxBindings);

		// Guarantee room for the output image
		if (s == 0 && numBindings == 0)
		{
			numBindings = 1;
		}

		bindings = vector<VkDescriptorSetLayoutBinding>(numBindings);
		bindingsFlags = vector<VkDescriptorBindingFlagsEXT>(numBindings);
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
			binding.stageFlags = allShaderStages;

			// Output image
			if (s == 0 && b == 0)
			{
				binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				binding.descriptorCount = 1;
				numImage++;
				arraySizes[b] = 0;
				continue;
			}

			binding.descriptorCount = 0;

			// Select a random type of descriptor.
			int r = randRange(&rnd, 0, (allowDynamicBuffers ? 6 : 4));
			switch (r)
			{
			default: DE_ASSERT(0); // Fallthrough
			case 0:
				binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				if (numUBO < caseDef.maxPerStageUniformBuffers)
				{
					arraySizes[b] = randRange(&rnd, 0, de::min(maxArray, caseDef.maxPerStageUniformBuffers - numUBO));
					binding.descriptorCount = arraySizes[b] ? arraySizes[b] : 1;
					numUBO += binding.descriptorCount;
				}
				break;
			case 1:
				binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				if (numSSBO < caseDef.maxPerStageStorageBuffers)
				{
					arraySizes[b] = randRange(&rnd, 0, de::min(maxArray, caseDef.maxPerStageStorageBuffers - numSSBO));
					binding.descriptorCount = arraySizes[b] ? arraySizes[b] : 1;
					numSSBO += binding.descriptorCount;
				}
				break;
			case 2:
				binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
				if (numImage < caseDef.maxPerStageStorageImages)
				{
					arraySizes[b] = randRange(&rnd, 0, de::min(maxArray, caseDef.maxPerStageStorageImages - numImage));
					binding.descriptorCount = arraySizes[b] ? arraySizes[b] : 1;
					numImage += binding.descriptorCount;
				}
				break;
			case 3:
				binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
				if (numTexBuffer < caseDef.maxPerStageSampledImages)
				{
					arraySizes[b] = randRange(&rnd, 0, de::min(maxArray, caseDef.maxPerStageSampledImages - numTexBuffer));
					binding.descriptorCount = arraySizes[b] ? arraySizes[b] : 1;
					numTexBuffer += binding.descriptorCount;
				}
				break;
			case 4:
				if (caseDef.maxInlineUniformBlocks > 0)
				{
					binding.descriptorType = VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT;
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
			case 5:
				binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
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
			case 6:
				binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
				if (numSSBODyn < caseDef.maxStorageBuffersDynamic &&
					numSSBO < caseDef.maxPerStageStorageBuffers)
				{
					arraySizes[b] = randRange(&rnd, 0, de::min(maxArray, de::min(caseDef.maxStorageBuffersDynamic - numSSBODyn,
																				 caseDef.maxPerStageStorageBuffers - numSSBO)));
					binding.descriptorCount = arraySizes[b] ? arraySizes[b] : 1;
					numSSBO += binding.descriptorCount;
					numSSBODyn += binding.descriptorCount;
				}
				break;
			}
		}
	}

	for (deUint32 s = 0; s < caseDef.numDescriptorSets; ++s)
	{
		vector<VkDescriptorSetLayoutBinding> &bindings = randomLayout.layoutBindings[s];
		vector<VkDescriptorBindingFlagsEXT> &bindingsFlags = randomLayout.layoutBindingFlags[s];
		vector<deUint32> &variableDescriptorSizes = randomLayout.variableDescriptorSizes;

		// Choose a variable descriptor count size. If the feature is not supported, we'll just
		// allocate the whole thing later on.
		if (bindings.size() > 0 &&
			bindings[bindings.size()-1].descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC &&
			bindings[bindings.size()-1].descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC &&
			!(s == 0 && bindings.size() == 1) && // Don't cut out the output image binding
			randRange(&rnd, 1,4) == 1) // 1 in 4 chance
		{

			bindingsFlags[bindings.size()-1] |= VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT;
			variableDescriptorSizes[s] = randRange(&rnd, 0,bindings[bindings.size()-1].descriptorCount);
			if (bindings[bindings.size()-1].descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
			{
				// keep a multiple of 16B
				variableDescriptorSizes[s] &= ~0xF;
			}
		}
	}
}

void DescriptorSetRandomTestCase::initPrograms (SourceCollections& programCollection) const
{
	RandomLayout randomLayout(m_data.numDescriptorSets);
	generateRandomLayout(randomLayout, m_data);

	std::stringstream decls, checks;

	deUint32 descriptor = 0;
	for (deUint32 s = 0; s < m_data.numDescriptorSets; ++s)
	{
		vector<VkDescriptorSetLayoutBinding> &bindings = randomLayout.layoutBindings[s];
		vector<VkDescriptorBindingFlagsEXT> bindingsFlags = randomLayout.layoutBindingFlags[s];
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
					decls << "layout(r32ui, set = " << s << ", binding = " << b << ") uniform uimage2D image" << s << "_" << b << array.str()  << ";\n";
					break;
				default: DE_ASSERT(0);
				}

				for (deUint32 ai = 0; ai < de::max(1u, arraySizes[b]); ++ai, descriptor += descriptorIncrement)
				{
					// Don't access descriptors past the end of the allocated range for
					// variable descriptor count
					if (b == bindings.size() - 1 &&
						(bindingsFlags[b] & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT))
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

					// Check that the value in the descriptor equals its descriptor number.
					// i.e. check "ubo[c].val == descriptor" or "ubo[pushconst[c]].val == descriptor"

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

					// For very large bindings, only check every N=201 descriptors (chosen arbitrarily)
					bool checkDescriptor = true;
					if (binding.descriptorType != VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
					{
						// For "large" bindings, only check every N=3 descriptors (chosen arbitrarily).
						// This is meant to reduce shader compile time.
						if (ai > 2 &&
							binding.descriptorCount >= 4 &&
							(ai % 3) != 0)
						{
							checkDescriptor = false;
						}
					}

					if (checkDescriptor)
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
						default: DE_ASSERT(0);
						}
						if (m_data.indexType == INDEX_TYPE_DEPENDENT || m_data.indexType == INDEX_TYPE_RUNTIME_SIZE)
						{
							// Set accum to zero, it is added to the next index.
							checks << "  accum = temp - " << descriptor << ";\n";
						}
						else
						{
							// Accumulate any incorrect values.
							checks << "  accum |= temp - " << descriptor << ";\n";
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
				"  int accum = 0, temp;\n"
				<< checks.str() <<
				"  uvec4 color = (accum != 0) ? uvec4(0,0,0,0) : uvec4(1,0,0,1);\n"
				"  imageStore(image0_0, ivec2(gl_GlobalInvocationID.xy), color);\n"
				"}\n";

			programCollection.glslSources.add("test") << glu::ComputeSource(css.str());
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
				"  int accum = 0, temp;\n"
				<< checks.str() <<
				"  uvec4 color = (accum != 0) ? uvec4(0,0,0,0) : uvec4(1,0,0,1);\n"
				"  imageStore(image0_0, ivec2(gl_VertexIndex % " << DIM << ", gl_VertexIndex / " << DIM << "), color);\n"
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
				"  int accum = 0, temp;\n"
				<< checks.str() <<
				"  uvec4 color = (accum != 0) ? uvec4(0,0,0,0) : uvec4(1,0,0,1);\n"
				"  imageStore(image0_0, ivec2(gl_FragCoord.x, gl_FragCoord.y), color);\n"
				"}\n";

			programCollection.glslSources.add("test") << glu::FragmentSource(fss.str());
			break;
		}
	}

}

TestInstance* DescriptorSetRandomTestCase::createInstance (Context& context) const
{
	return new DescriptorSetRandomTestInstance(context, m_data);
}

VkBufferImageCopy makeBufferImageCopy (const VkExtent3D					extent,
									   const VkImageSubresourceLayers	subresourceLayers)
{
	const VkBufferImageCopy copyParams =
	{
		0ull,										//	VkDeviceSize				bufferOffset;
		0u,											//	deUint32					bufferRowLength;
		0u,											//	deUint32					bufferImageHeight;
		subresourceLayers,							//	VkImageSubresourceLayers	imageSubresource;
		makeOffset3D(0, 0, 0),						//	VkOffset3D					imageOffset;
		extent,										//	VkExtent3D					imageExtent;
	};
	return copyParams;
}

tcu::TestStatus DescriptorSetRandomTestInstance::iterate (void)
{
	const DeviceInterface&	vk						= m_context.getDeviceInterface();
	const VkDevice			device					= m_context.getDevice();
	Allocator&				allocator				= m_context.getDefaultAllocator();

	RandomLayout randomLayout(m_data.numDescriptorSets);
	generateRandomLayout(randomLayout, m_data);


	VkPhysicalDeviceProperties2 properties;
	deMemset(&properties, 0, sizeof(properties));
	properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;

	m_context.getInstanceInterface().getPhysicalDeviceProperties2(m_context.getPhysicalDevice(), &properties);

	VkPhysicalDeviceInlineUniformBlockFeaturesEXT inlineUniformFeatures;
	deMemset(&inlineUniformFeatures, 0, sizeof(inlineUniformFeatures));
	inlineUniformFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_FEATURES_EXT;

	VkPhysicalDeviceDescriptorIndexingFeaturesEXT indexingFeatures;
	deMemset(&indexingFeatures, 0, sizeof(indexingFeatures));
	indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;

	VkPhysicalDeviceFeatures2 features;
	deMemset(&features, 0, sizeof(features));
	features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

	if (isDeviceExtensionSupported(m_context.getUsedApiVersion(), m_context.getDeviceExtensions(), "VK_EXT_descriptor_indexing") &&
		isDeviceExtensionSupported(m_context.getUsedApiVersion(), m_context.getDeviceExtensions(), "VK_EXT_inline_uniform_block"))
	{
		indexingFeatures.pNext = &inlineUniformFeatures;
		features.pNext = &indexingFeatures;
	}
	else if (isDeviceExtensionSupported(m_context.getUsedApiVersion(), m_context.getDeviceExtensions(), "VK_EXT_descriptor_indexing"))
	{
		features.pNext = &indexingFeatures;
	}
	else if (isDeviceExtensionSupported(m_context.getUsedApiVersion(), m_context.getDeviceExtensions(), "VK_EXT_inline_uniform_block"))
	{
		features.pNext = &inlineUniformFeatures;
	}

	m_context.getInstanceInterface().getPhysicalDeviceFeatures2(m_context.getPhysicalDevice(), &features);

	deRandom rnd;
	deRandom_init(&rnd, m_data.seed);

	VkPipelineBindPoint bindPoint = m_data.stage == STAGE_COMPUTE ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;

	DE_ASSERT(m_data.numDescriptorSets <= 32);
	Move<vk::VkDescriptorSetLayout>	descriptorSetLayouts[32];
	Move<vk::VkDescriptorPool>		descriptorPools[32];
	Move<vk::VkDescriptorSet>		descriptorSets[32];

	deUint32 numDescriptors = 0;
	for (deUint32 s = 0; s < m_data.numDescriptorSets; ++s)
	{
		vector<VkDescriptorSetLayoutBinding> &bindings = randomLayout.layoutBindings[s];
		vector<VkDescriptorBindingFlagsEXT> &bindingsFlags = randomLayout.layoutBindingFlags[s];
		vector<deUint32> &variableDescriptorSizes = randomLayout.variableDescriptorSizes;

		VkDescriptorPoolCreateFlags poolCreateFlags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		VkDescriptorSetLayoutCreateFlags layoutCreateFlags = 0;

		for (size_t b = 0; b < bindings.size(); ++b)
		{
			VkDescriptorSetLayoutBinding &binding = bindings[b];
			numDescriptors += binding.descriptorCount;

			// Randomly choose some bindings to use update-after-bind, if it is supported
			if (m_data.uab == UPDATE_AFTER_BIND_ENABLED &&
				randRange(&rnd, 1, 8) == 1 && // 1 in 8 chance
				(binding.descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER			|| indexingFeatures.descriptorBindingUniformBufferUpdateAfterBind) &&
				(binding.descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_IMAGE				|| indexingFeatures.descriptorBindingStorageImageUpdateAfterBind) &&
				(binding.descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER			|| indexingFeatures.descriptorBindingStorageBufferUpdateAfterBind) &&
				(binding.descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER		|| indexingFeatures.descriptorBindingUniformTexelBufferUpdateAfterBind) &&
				(binding.descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER		|| indexingFeatures.descriptorBindingStorageTexelBufferUpdateAfterBind) &&
				(binding.descriptorType != VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT	|| inlineUniformFeatures.descriptorBindingInlineUniformBlockUpdateAfterBind) &&
				(binding.descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) &&
				(binding.descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC))
			{
				bindingsFlags[b] |= VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT;
				layoutCreateFlags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT;
				poolCreateFlags |= VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;
			}

			if (!indexingFeatures.descriptorBindingVariableDescriptorCount)
			{
				bindingsFlags[b] &= ~VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT;
			}
		}

		// Create a layout and allocate a descriptor set for it.

		const VkDescriptorSetLayoutBindingFlagsCreateInfoEXT bindingFlagsInfo =
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT,	// VkStructureType						sType;
			DE_NULL,																// const void*							pNext;
			(deUint32)bindings.size(),												// uint32_t								bindingCount;
			bindings.empty() ? DE_NULL : &bindingsFlags[0],							// const VkDescriptorBindingFlagsEXT*	pBindingFlags;
		};

		const VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo =
		{
			vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			&bindingFlagsInfo,

			layoutCreateFlags,
			(deUint32)bindings.size(),
			bindings.empty() ? DE_NULL : &bindings[0]
		};

		descriptorSetLayouts[s] = vk::createDescriptorSetLayout(vk, device, &setLayoutCreateInfo);

		vk::DescriptorPoolBuilder poolBuilder;
		poolBuilder.addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, m_data.maxPerStageUniformBuffers);
		poolBuilder.addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, m_data.maxUniformBuffersDynamic);
		poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_data.maxPerStageStorageBuffers);
		poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, m_data.maxStorageBuffersDynamic);
		poolBuilder.addType(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, m_data.maxPerStageSampledImages);
		poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, m_data.maxPerStageStorageImages);
		poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1);
		if (m_data.maxInlineUniformBlocks)
		{
			poolBuilder.addType(VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT, m_data.maxInlineUniformBlocks * m_data.maxInlineUniformBlockSize);
		}

		VkDescriptorPoolInlineUniformBlockCreateInfoEXT inlineUniformBlockPoolCreateInfo =
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_INLINE_UNIFORM_BLOCK_CREATE_INFO_EXT,	// VkStructureType	sType;
			DE_NULL,																// const void*		pNext;
			m_data.maxInlineUniformBlocks,											// uint32_t			maxInlineUniformBlockBindings;
		};

		descriptorPools[s] = poolBuilder.build(vk, device, poolCreateFlags, 1u,
											   m_data.maxInlineUniformBlocks ? &inlineUniformBlockPoolCreateInfo : DE_NULL);

		VkDescriptorSetVariableDescriptorCountAllocateInfoEXT variableCountInfo =
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT,	// VkStructureType	sType;
			DE_NULL,																		// const void*		pNext;
			0,																				// uint32_t			descriptorSetCount;
			DE_NULL,																		// const uint32_t*	pDescriptorCounts;
		};

		const void *pNext = DE_NULL;
		if (bindings.size() > 0 &&
			bindingsFlags[bindings.size()-1] & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT)
		{
			variableCountInfo.descriptorSetCount = 1;
			variableCountInfo.pDescriptorCounts = &variableDescriptorSizes[s];
			pNext = &variableCountInfo;
		}

		descriptorSets[s] = makeDescriptorSet(vk, device, *descriptorPools[s], *descriptorSetLayouts[s], pNext);
	}


	VkDeviceSize	align = de::max(de::max(de::max(properties.properties.limits.minTexelBufferOffsetAlignment,
													properties.properties.limits.minUniformBufferOffsetAlignment),
													properties.properties.limits.minStorageBufferOffsetAlignment),
													(VkDeviceSize)sizeof(deUint32));

	de::MovePtr<BufferWithMemory> buffer;
	buffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
		vk, device, allocator, makeBufferCreateInfo(align*numDescriptors,
													VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
													VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
													VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT |
													VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT),
													MemoryRequirement::HostVisible));
	deUint8 *bufferPtr = (deUint8 *)buffer->getAllocation().getHostPtr();

	typedef vk::Unique<vk::VkBufferView>					BufferViewHandleUp;
	typedef de::SharedPtr<BufferViewHandleUp>				BufferViewHandleSp;

	vector<BufferViewHandleSp>	bufferViews(de::max(1u,numDescriptors));

	// Create a buffer and view for each descriptor. Fill descriptor 'd'
	// with an integer value equal to 'd'.
	int descriptor = 0;
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
			if (binding.descriptorType != VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
			{
				for (deUint32 d = descriptor; d < descriptor + binding.descriptorCount; ++d)
				{
					deUint32 *ptr = (deUint32 *)(bufferPtr + align*d);
					*ptr = d;

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
			else
			{
				// subtract 16 for "ivec4 dummy"
				DE_ASSERT(binding.descriptorCount >= 16);
				descriptor += binding.descriptorCount - 16;
			}
		}
	}

	flushMappedMemoryRange(vk, device, buffer->getAllocation().getMemory(), buffer->getAllocation().getOffset(), VK_WHOLE_SIZE);

	const VkQueue					queue					= m_context.getUniversalQueue();
	Move<VkCommandPool>				cmdPool					= createCommandPool(vk, device, 0, m_context.getUniversalQueueFamilyIndex());
	Move<VkCommandBuffer>			cmdBuffer				= allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	beginCommandBuffer(vk, *cmdBuffer, 0u);

	// Push constants are used for dynamic indexing. PushConstant[i] = i.

	const VkPushConstantRange pushConstRange =
	{
		allShaderStages,		// VkShaderStageFlags	stageFlags
		0,						// deUint32				offset
		128						// deUint32				size
	};

	vector<vk::VkDescriptorSetLayout>	descriptorSetLayoutsRaw(m_data.numDescriptorSets);
	for (size_t i = 0; i < m_data.numDescriptorSets; ++i)
	{
		descriptorSetLayoutsRaw[i] = descriptorSetLayouts[i].get();
	}

	const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,				// sType
		DE_NULL,													// pNext
		(VkPipelineLayoutCreateFlags)0,
		m_data.numDescriptorSets,									// setLayoutCount
		&descriptorSetLayoutsRaw[0],								// pSetLayouts
		m_data.indexType == INDEX_TYPE_PUSHCONSTANT ? 1u : 0u,		// pushConstantRangeCount
		&pushConstRange,											// pPushConstantRanges
	};

	Move<VkPipelineLayout> pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo, NULL);

	if (m_data.indexType == INDEX_TYPE_PUSHCONSTANT)
	{
		// PushConstant[i] = i
		for (deUint32 i = 0; i < (deUint32)(128 / sizeof(deUint32)); ++i)
		{
			vk.cmdPushConstants(*cmdBuffer, *pipelineLayout, allShaderStages,
								(deUint32)(i * sizeof(deUint32)), (deUint32)sizeof(deUint32), &i);
		}
	}

	de::MovePtr<BufferWithMemory> copyBuffer;
	copyBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
		vk, device, allocator, makeBufferCreateInfo(DIM*DIM*sizeof(deUint32), VK_BUFFER_USAGE_TRANSFER_DST_BIT), MemoryRequirement::HostVisible));

	const VkImageCreateInfo			imageCreateInfo			=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType;
		DE_NULL,								// const void*				pNext;
		(VkImageCreateFlags)0u,					// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						// VkImageType				imageType;
		VK_FORMAT_R32_UINT,						// VkFormat					format;
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
		VK_FORMAT_R32_UINT,							// VkFormat					format;
		{
			VK_COMPONENT_SWIZZLE_R,					// VkComponentSwizzle	r;
			VK_COMPONENT_SWIZZLE_G,					// VkComponentSwizzle	g;
			VK_COMPONENT_SWIZZLE_B,					// VkComponentSwizzle	b;
			VK_COMPONENT_SWIZZLE_A					// VkComponentSwizzle	a;
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

	descriptor = 0;
	for (deUint32 s = 0; s < m_data.numDescriptorSets; ++s)
	{
		vector<VkDescriptorSetLayoutBinding> &bindings = randomLayout.layoutBindings[s];
		vector<VkDescriptorBindingFlagsEXT> &bindingsFlags = randomLayout.layoutBindingFlags[s];
		vector<deUint32> &arraySizes = randomLayout.arraySizes[s];
		vector<deUint32> &variableDescriptorSizes = randomLayout.variableDescriptorSizes;

		vector<VkDescriptorBufferInfo> bufferInfoVec(numDescriptors);
		vector<VkDescriptorImageInfo> imageInfoVec(numDescriptors);
		vector<VkBufferView> bufferViewVec(numDescriptors);
		vector<VkWriteDescriptorSetInlineUniformBlockEXT> inlineInfoVec(numDescriptors);
		vector<deUint32> descriptorNumber(numDescriptors);
		vector<VkWriteDescriptorSet> writesBeforeBindVec(0);
		vector<VkWriteDescriptorSet> writesAfterBindVec(0);
		int vecIndex = 0;
		int numDynamic = 0;

		vector<VkDescriptorUpdateTemplateEntry> imgTemplateEntriesBefore, imgTemplateEntriesAfter,
												bufTemplateEntriesBefore, bufTemplateEntriesAfter,
												texelBufTemplateEntriesBefore, texelBufTemplateEntriesAfter,
												inlineTemplateEntriesBefore, inlineTemplateEntriesAfter;

		for (size_t b = 0; b < bindings.size(); ++b)
		{
			VkDescriptorSetLayoutBinding &binding = bindings[b];
			deUint32 descriptorIncrement = (binding.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT) ? 16 : 1;

			// Construct the declaration for the binding
			if (binding.descriptorCount > 0)
			{
				bool updateAfterBind = !!(bindingsFlags[b] & VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT);
				for (deUint32 ai = 0; ai < de::max(1u, arraySizes[b]); ++ai, descriptor += descriptorIncrement)
				{
					// Don't access descriptors past the end of the allocated range for
					// variable descriptor count
					if (b == bindings.size() - 1 &&
						(bindingsFlags[b] & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT))
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
					imageInfoVec[vecIndex] = makeDescriptorImageInfo(DE_NULL, *imageView, VK_IMAGE_LAYOUT_GENERAL);

					if (binding.descriptorType != VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
					{
						bufferInfoVec[vecIndex] = makeDescriptorBufferInfo(**buffer, descriptor*align, sizeof(deUint32));
						bufferViewVec[vecIndex] = **bufferViews[descriptor];
					}

					descriptorNumber[descriptor] = descriptor;

					VkWriteDescriptorSet w =
					{
						VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,							// sType
						DE_NULL,														// pNext
						*descriptorSets[s],												// dstSet
						(deUint32)b,													// binding
						ai,																// dstArrayElement
						1u,																// descriptorCount
						binding.descriptorType,											// descriptorType
						&imageInfoVec[vecIndex],										// pImageInfo
						&bufferInfoVec[vecIndex],										// pBufferInfo
						&bufferViewVec[vecIndex],										// pTexelBufferView
					};

					if (binding.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
					{
						VkWriteDescriptorSetInlineUniformBlockEXT inlineUniformBlock =
						{
							VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK_EXT,	// VkStructureType	sType;
							DE_NULL,															// const void*		pNext;
							sizeof(deUint32),													// uint32_t			dataSize;
							&descriptorNumber[descriptor],										// const void*		pData;
						};

						inlineInfoVec[vecIndex] = inlineUniformBlock;
						w.dstArrayElement = ai*16 + 16; // add 16 to skip "ivec4 dummy"
						w.pNext = &inlineInfoVec[vecIndex];
						w.descriptorCount = sizeof(deUint32);
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
					default: DE_ASSERT(0); // Fallthrough
					case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
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
			m_context.contextSupports(vk::ApiVersion(1, 1, 0)))
		{
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
				&imageInfoVec[0],
				&bufferInfoVec[0],
				&bufferViewVec[0],
				&descriptorNumber[0],
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

			for (size_t i = 0; i < sizeof(templateVectorsBefore) / sizeof(templateVectorsBefore[0]); ++i)
			{
				if (templateVectorsBefore[i]->size())
				{
					templateCreateInfo.descriptorUpdateEntryCount = (deUint32)templateVectorsBefore[i]->size();
					templateCreateInfo.pDescriptorUpdateEntries = &((*templateVectorsBefore[i])[0]);
					Move<VkDescriptorUpdateTemplate> descriptorUpdateTemplate = createDescriptorUpdateTemplate(vk, device, &templateCreateInfo, NULL);
					vk.updateDescriptorSetWithTemplate(device, descriptorSets[s].get(), *descriptorUpdateTemplate, templateVectorData[i]);
				}
			}

			vk.cmdBindDescriptorSets(*cmdBuffer, bindPoint, *pipelineLayout, s, 1, &descriptorSets[s].get(), numDynamic, &zeros[0]);

			for (size_t i = 0; i < sizeof(templateVectorsAfter) / sizeof(templateVectorsAfter[0]); ++i)
			{
				if (templateVectorsAfter[i]->size())
				{
					templateCreateInfo.descriptorUpdateEntryCount = (deUint32)templateVectorsAfter[i]->size();
					templateCreateInfo.pDescriptorUpdateEntries = &((*templateVectorsAfter[i])[0]);
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
	else
	{

		const vk::VkSubpassDescription		subpassDesc			=
		{
			(vk::VkSubpassDescriptionFlags)0,
			vk::VK_PIPELINE_BIND_POINT_GRAPHICS,					// pipelineBindPoint
			0u,														// inputCount
			DE_NULL,												// pInputAttachments
			0u,														// colorCount
			DE_NULL,												// pColorAttachments
			DE_NULL,												// pResolveAttachments
			DE_NULL,												// depthStencilAttachment
			0u,														// preserveCount
			DE_NULL,												// pPreserveAttachments
		};
		const vk::VkRenderPassCreateInfo	renderPassParams	=
		{
			vk::VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,			// sType
			DE_NULL,												// pNext
			(vk::VkRenderPassCreateFlags)0,
			0u,														// attachmentCount
			DE_NULL,												// pAttachments
			1u,														// subpassCount
			&subpassDesc,											// pSubpasses
			0u,														// dependencyCount
			DE_NULL,												// pDependencies
		};

		renderPass = createRenderPass(vk, device, &renderPassParams);

		const vk::VkFramebufferCreateInfo	framebufferParams	=
		{
			vk::VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// sType
			DE_NULL,										// pNext
			(vk::VkFramebufferCreateFlags)0,
			*renderPass,									// renderPass
			0u,												// attachmentCount
			DE_NULL,										// pAttachments
			DIM,											// width
			DIM,											// height
			1u,												// layers
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
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, allPipelineStages,
		0, 1, &memBarrier, 0, DE_NULL, 0, DE_NULL);

	if (m_data.stage == STAGE_COMPUTE)
	{
		vk.cmdDispatch(*cmdBuffer, DIM, DIM, 1);
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
	vk.cmdPipelineBarrier(*cmdBuffer, allPipelineStages, VK_PIPELINE_STAGE_TRANSFER_BIT,
		0, 1, &memBarrier, 0, DE_NULL, 0, DE_NULL);

	const VkBufferImageCopy copyRegion = makeBufferImageCopy(makeExtent3D(DIM, DIM, 1u),
															 makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u));
	vk.cmdCopyImageToBuffer(*cmdBuffer, **image, VK_IMAGE_LAYOUT_GENERAL, **copyBuffer, 1u, &copyRegion);

	endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

	deUint32 *ptr = (deUint32 *)copyBuffer->getAllocation().getHostPtr();
	invalidateMappedMemoryRange(vk, device, copyBuffer->getAllocation().getMemory(), copyBuffer->getAllocation().getOffset(), DIM*DIM*sizeof(deUint32));

	qpTestResult res = QP_TEST_RESULT_PASS;

	for (deUint32 i = 0; i < DIM*DIM; ++i)
	{
		if (ptr[i] != 1)
		{
			res = QP_TEST_RESULT_FAIL;
		}
	}

	return tcu::TestStatus(res, qpGetTestResultName(res));
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
		{ 12,			"ubolimitlow",		"spec minmax ubo limit"		},
		{ 4096,			"ubolimithigh",		"high ubo limit"			},
	};

	TestGroupCase sboCases[] =
	{
		{ 4,			"sbolimitlow",		"spec minmax ssbo limit"	},
		{ 4096,			"sbolimithigh",		"high ssbo limit"			},
	};

	static const struct
	{
		deUint32				texCount;
		deUint32				imgCount;
		const char*				name;
		const char*				description;
	} imgCases[] =
	{
		{ 16, 4,		"imglimitlow",		"spec minmax image limit"	},
		{ 4096, 4096,	"imglimithigh",		"high image limit"			},
	};

	static const struct
	{
		deUint32				iubCount;
		deUint32				iubSize;
		const char*				name;
		const char*				description;
	} iubCases[] =
	{
		{ 0, 0,		"noiub",			"no inline_uniform_block"			},
		{ 4, 256,	"iublimitlow",		"inline_uniform_block low limit"	},
		{ 8, 4096,	"iublimithigh",		"inline_uniform_block high limit"	},
	};

	TestGroupCase stageCases[] =
	{
		{ STAGE_COMPUTE,	"comp",		"compute"	},
		{ STAGE_FRAGMENT,	"frag",		"fragment"	},
		{ STAGE_VERTEX,		"vert",		"vertex"	},
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
					for (int imgNdx = 0; imgNdx < DE_LENGTH_OF_ARRAY(imgCases); imgNdx++)
					{
						de::MovePtr<tcu::TestCaseGroup> imgGroup(new tcu::TestCaseGroup(testCtx, imgCases[imgNdx].name, imgCases[imgNdx].description));
						for (int iubNdx = 0; iubNdx < DE_LENGTH_OF_ARRAY(iubCases); iubNdx++)
						{
							de::MovePtr<tcu::TestCaseGroup> iubGroup(new tcu::TestCaseGroup(testCtx, iubCases[iubNdx].name, iubCases[iubNdx].description));
							for (int uabNdx = 0; uabNdx < DE_LENGTH_OF_ARRAY(uabCases); uabNdx++)
							{
								de::MovePtr<tcu::TestCaseGroup> uabGroup(new tcu::TestCaseGroup(testCtx, uabCases[uabNdx].name, uabCases[uabNdx].description));
								bool updateAfterBind = (UpdateAfterBind)uabCases[uabNdx].count == UPDATE_AFTER_BIND_ENABLED;
								for (int stageNdx = 0; stageNdx < DE_LENGTH_OF_ARRAY(stageCases); stageNdx++)
								{
									de::MovePtr<tcu::TestCaseGroup> stageGroup(new tcu::TestCaseGroup(testCtx, stageCases[stageNdx].name, stageCases[stageNdx].description));
									deUint32 numSeeds = (setsCases[setsNdx].count == 4 && uboNdx == 0 && sboNdx == 0 && imgNdx == 0 && iubNdx == 0) ? 10 : 1;
									for (deUint32 rnd = 0; rnd < numSeeds; ++rnd)
									{
										CaseDef c =
										{
											(IndexType)indexCases[indexNdx].count,							// IndexType indexType;
											setsCases[setsNdx].count,										// deUint32 numDescriptorSets;
											uboCases[uboNdx].count,											// deUint32 maxPerStageUniformBuffers;
											8,																// deUint32 maxUniformBuffersDynamic;
											sboCases[sboNdx].count,											// deUint32 maxPerStageStorageBuffers;
											4,																// deUint32 maxStorageBuffersDynamic;
											imgCases[imgNdx].texCount,										// deUint32 maxPerStageSampledImages;
											imgCases[imgNdx].imgCount,										// deUint32 maxPerStageStorageImages;
											iubCases[iubNdx].iubCount,										// deUint32 maxInlineUniformBlocks;
											iubCases[iubNdx].iubSize,										// deUint32 maxInlineUniformBlockSize;
											(Stage)stageCases[stageNdx].count,								// Stage stage;
											(UpdateAfterBind)uabCases[uabNdx].count,						// UpdateAfterBind uab;
											seed++,															// deUint32 seed;
										};

										string name = de::toString(rnd);
										stageGroup->addChild(new DescriptorSetRandomTestCase(testCtx, name.c_str(), "test", c));
									}
									(updateAfterBind ? uabGroup : iubGroup)->addChild(stageGroup.release());
								}
								if (updateAfterBind)
								{
									iubGroup->addChild(uabGroup.release());
								}
							}
							imgGroup->addChild(iubGroup.release());
						}
						sboGroup->addChild(imgGroup.release());
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
