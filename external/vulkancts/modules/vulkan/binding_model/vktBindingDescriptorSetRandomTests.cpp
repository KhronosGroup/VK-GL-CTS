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

static const uint32_t DIM = 8;

#ifndef CTS_USES_VULKANSC
static const VkFlags ALL_RAY_TRACING_STAGES = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR |
                                              VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR |
                                              VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR;
#endif

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
    STAGE_TASK,
    STAGE_MESH,
} Stage;

typedef enum
{
    UPDATE_AFTER_BIND_DISABLED = 0,
    UPDATE_AFTER_BIND_ENABLED,
} UpdateAfterBind;

struct DescriptorId
{
    DescriptorId(uint32_t set_, uint32_t binding_, uint32_t number_) : set(set_), binding(binding_), number(number_)
    {
    }

    bool operator<(const DescriptorId &other) const
    {
        return (set < other.set ||
                (set == other.set && (binding < other.binding || (binding == other.binding && number < other.number))));
    }

    uint32_t set;
    uint32_t binding;
    uint32_t number;
};

struct WriteInfo
{
    WriteInfo() : ptr(nullptr), expected(0u), writeGenerated(false)
    {
    }

    int32_t *ptr;
    int32_t expected;
    bool writeGenerated;
};

bool isRayTracingStageKHR(const Stage stage)
{
    switch (stage)
    {
    case STAGE_COMPUTE:
    case STAGE_VERTEX:
    case STAGE_FRAGMENT:
    case STAGE_RAYGEN_NV:
    case STAGE_TASK:
    case STAGE_MESH:
        return false;

    case STAGE_RAYGEN:
    case STAGE_INTERSECT:
    case STAGE_ANY_HIT:
    case STAGE_CLOSEST_HIT:
    case STAGE_MISS:
    case STAGE_CALLABLE:
        return true;

    default:
        TCU_THROW(InternalError, "Unknown stage specified");
    }
}

bool isMeshStage(Stage stage)
{
    return (stage == STAGE_TASK || stage == STAGE_MESH);
}

bool isVertexPipelineStage(Stage stage)
{
    return (isMeshStage(stage) || stage == STAGE_VERTEX);
}

#ifndef CTS_USES_VULKANSC
VkShaderStageFlagBits getShaderStageFlag(const Stage stage)
{
    switch (stage)
    {
    case STAGE_RAYGEN:
        return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    case STAGE_ANY_HIT:
        return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
    case STAGE_CLOSEST_HIT:
        return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    case STAGE_MISS:
        return VK_SHADER_STAGE_MISS_BIT_KHR;
    case STAGE_INTERSECT:
        return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
    case STAGE_CALLABLE:
        return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
    default:
        TCU_THROW(InternalError, "Unknown stage specified");
    }
}
#endif

VkShaderStageFlags getAllShaderStagesFor(Stage stage, tcu::TestContext &testCtx)
{
#ifndef CTS_USES_VULKANSC
    if (stage == STAGE_RAYGEN_NV)
        return VK_SHADER_STAGE_RAYGEN_BIT_NV;

    if (isRayTracingStageKHR(stage))
        return ALL_RAY_TRACING_STAGES;

    if (isMeshStage(stage))
        return (VK_SHADER_STAGE_MESH_BIT_EXT | ((stage == STAGE_TASK) ? VK_SHADER_STAGE_TASK_BIT_EXT : 0));
#else
    DE_UNREF(stage);
#endif // CTS_USES_VULKANSC

    if (testCtx.getCommandLine().isComputeOnly())
        return VK_SHADER_STAGE_COMPUTE_BIT;

    return (VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
}

VkPipelineStageFlags getAllPipelineStagesFor(Stage stage, tcu::TestContext &testCtx)
{
#ifndef CTS_USES_VULKANSC
    if (stage == STAGE_RAYGEN_NV)
        return VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV;

    if (isRayTracingStageKHR(stage))
        return VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;

    if (isMeshStage(stage))
        return (VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT |
                ((stage == STAGE_TASK) ? VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT : 0));
#else
    DE_UNREF(stage);
#endif // CTS_USES_VULKANSC

    if (testCtx.getCommandLine().isComputeOnly())
        return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

    return (VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}

bool usesAccelerationStructure(const Stage stage)
{
    return (isRayTracingStageKHR(stage) && stage != STAGE_RAYGEN && stage != STAGE_CALLABLE);
}

class RandomLayout
{
public:
    RandomLayout(uint32_t numSets)
        : layoutBindings(numSets)
        , layoutBindingFlags(numSets)
        , arraySizes(numSets)
        , variableDescriptorSizes(numSets)
    {
    }

    // These three are indexed by [set][binding]
    vector<vector<VkDescriptorSetLayoutBinding>> layoutBindings;
    vector<vector<VkDescriptorBindingFlags>> layoutBindingFlags;
    vector<vector<uint32_t>> arraySizes;
    // size of the variable descriptor (last) binding in each set
    vector<uint32_t> variableDescriptorSizes;

    // List of descriptors that will write the descriptor value instead of reading it.
    map<DescriptorId, WriteInfo> descriptorWrites;
};

struct CaseDef
{
    IndexType indexType;
    uint32_t numDescriptorSets;
    uint32_t maxPerStageUniformBuffers;
    uint32_t maxUniformBuffersDynamic;
    uint32_t maxPerStageStorageBuffers;
    uint32_t maxStorageBuffersDynamic;
    uint32_t maxPerStageSampledImages;
    uint32_t maxPerStageStorageImages;
    uint32_t maxPerStageStorageTexelBuffers;
    uint32_t maxInlineUniformBlocks;
    uint32_t maxInlineUniformBlockSize;
    uint32_t maxPerStageInputAttachments;
    Stage stage;
    UpdateAfterBind uab;
    uint32_t seed;
    VkFlags allShaderStages;
    VkFlags allPipelineStages;
    // Shared by the test case and the test instance.
    std::shared_ptr<RandomLayout> randomLayout;
};

class DescriptorSetRandomTestInstance : public TestInstance
{
public:
    DescriptorSetRandomTestInstance(Context &context, const std::shared_ptr<CaseDef> &data);
    ~DescriptorSetRandomTestInstance(void);
    tcu::TestStatus iterate(void);

private:
    // Shared pointer because the test case and the test instance need to share the random layout information. Specifically, the
    // descriptorWrites map, which is filled from the test case and used by the test instance.
    std::shared_ptr<CaseDef> m_data_ptr;
    CaseDef &m_data;
};

DescriptorSetRandomTestInstance::DescriptorSetRandomTestInstance(Context &context, const std::shared_ptr<CaseDef> &data)
    : vkt::TestInstance(context)
    , m_data_ptr(data)
    , m_data(*m_data_ptr.get())
{
}

DescriptorSetRandomTestInstance::~DescriptorSetRandomTestInstance(void)
{
}

class DescriptorSetRandomTestCase : public TestCase
{
public:
    DescriptorSetRandomTestCase(tcu::TestContext &context, const char *name, const CaseDef &data);
    ~DescriptorSetRandomTestCase(void);
    virtual void initPrograms(SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

private:
    // See DescriptorSetRandomTestInstance about the need for a shared pointer here.
    std::shared_ptr<CaseDef> m_data_ptr;
    CaseDef &m_data;
};

DescriptorSetRandomTestCase::DescriptorSetRandomTestCase(tcu::TestContext &context, const char *name,
                                                         const CaseDef &data)
    : vkt::TestCase(context, name)
    , m_data_ptr(std::make_shared<CaseDef>(data))
    , m_data(*reinterpret_cast<CaseDef *>(m_data_ptr.get()))
{
}

DescriptorSetRandomTestCase::~DescriptorSetRandomTestCase(void)
{
}

void DescriptorSetRandomTestCase::checkSupport(Context &context) const
{
    VkPhysicalDeviceProperties2 properties;
    deMemset(&properties, 0, sizeof(properties));
    properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;

#ifndef CTS_USES_VULKANSC
    void **pNextTail = &properties.pNext;
    // Get needed properties.
    VkPhysicalDeviceInlineUniformBlockPropertiesEXT inlineUniformProperties;
    deMemset(&inlineUniformProperties, 0, sizeof(inlineUniformProperties));
    inlineUniformProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_PROPERTIES_EXT;

    if (context.isDeviceFunctionalitySupported("VK_EXT_inline_uniform_block"))
    {
        *pNextTail = &inlineUniformProperties;
        pNextTail  = &inlineUniformProperties.pNext;
    }
    *pNextTail = NULL;
#endif

    context.getInstanceInterface().getPhysicalDeviceProperties2(context.getPhysicalDevice(), &properties);

    // Get needed features.
    auto features         = context.getDeviceFeatures2();
    auto indexingFeatures = context.getDescriptorIndexingFeatures();
#ifndef CTS_USES_VULKANSC
    auto inlineUniformFeatures = context.getInlineUniformBlockFeatures();
#endif

    // Check needed properties and features
    if (isVertexPipelineStage(m_data.stage) && !features.features.vertexPipelineStoresAndAtomics)
    {
        TCU_THROW(NotSupportedError, "Vertex pipeline stores and atomics not supported");
    }
#ifndef CTS_USES_VULKANSC
    else if (m_data.stage == STAGE_RAYGEN_NV)
    {
        context.requireDeviceFunctionality("VK_NV_ray_tracing");
    }
    else if (isRayTracingStageKHR(m_data.stage))
    {
        context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
        context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");

        const VkPhysicalDeviceRayTracingPipelineFeaturesKHR &rayTracingPipelineFeaturesKHR =
            context.getRayTracingPipelineFeatures();
        if (rayTracingPipelineFeaturesKHR.rayTracingPipeline == false)
            TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipeline");

        const VkPhysicalDeviceAccelerationStructureFeaturesKHR &accelerationStructureFeaturesKHR =
            context.getAccelerationStructureFeatures();
        if (accelerationStructureFeaturesKHR.accelerationStructure == false)
            TCU_THROW(TestError, "VK_KHR_ray_tracing_pipeline requires "
                                 "VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure");
    }

    if (isMeshStage(m_data.stage))
    {
        const auto &meshFeatures = context.getMeshShaderFeaturesEXT();

        if (!meshFeatures.meshShader)
            TCU_THROW(NotSupportedError, "Mesh shaders not supported");

        if (m_data.stage == STAGE_TASK && !meshFeatures.taskShader)
            TCU_THROW(NotSupportedError, "Task shaders not supported");
    }
#endif

    // Note binding 0 in set 0 is the output storage image, always present and not subject to dynamic indexing.
    if ((m_data.indexType == INDEX_TYPE_PUSHCONSTANT || m_data.indexType == INDEX_TYPE_DEPENDENT ||
         m_data.indexType == INDEX_TYPE_RUNTIME_SIZE) &&
        ((m_data.maxPerStageUniformBuffers > 0u && !features.features.shaderUniformBufferArrayDynamicIndexing) ||
         (m_data.maxPerStageStorageBuffers > 0u && !features.features.shaderStorageBufferArrayDynamicIndexing) ||
         (m_data.maxPerStageStorageImages > 1u && !features.features.shaderStorageImageArrayDynamicIndexing) ||
         (m_data.stage == STAGE_FRAGMENT && m_data.maxPerStageInputAttachments > 0u &&
          (!indexingFeatures.shaderInputAttachmentArrayDynamicIndexing)) ||
         (m_data.maxPerStageSampledImages > 0u && !indexingFeatures.shaderUniformTexelBufferArrayDynamicIndexing) ||
         (m_data.maxPerStageStorageTexelBuffers > 0u &&
          !indexingFeatures.shaderStorageTexelBufferArrayDynamicIndexing)))
    {
        TCU_THROW(NotSupportedError, "Dynamic indexing not supported");
    }

    if (m_data.numDescriptorSets > properties.properties.limits.maxBoundDescriptorSets)
    {
        TCU_THROW(NotSupportedError, "Number of descriptor sets not supported");
    }

    if ((m_data.maxPerStageUniformBuffers + m_data.maxPerStageStorageBuffers + m_data.maxPerStageSampledImages +
         m_data.maxPerStageStorageImages + m_data.maxPerStageStorageTexelBuffers + m_data.maxPerStageInputAttachments) >
        properties.properties.limits.maxPerStageResources)
    {
        TCU_THROW(NotSupportedError, "Number of descriptors not supported");
    }

    if (m_data.maxPerStageUniformBuffers > properties.properties.limits.maxPerStageDescriptorUniformBuffers ||
        m_data.maxPerStageStorageBuffers > properties.properties.limits.maxPerStageDescriptorStorageBuffers ||
        m_data.maxUniformBuffersDynamic > properties.properties.limits.maxDescriptorSetUniformBuffersDynamic ||
        m_data.maxStorageBuffersDynamic > properties.properties.limits.maxDescriptorSetStorageBuffersDynamic ||
        m_data.maxPerStageSampledImages > properties.properties.limits.maxPerStageDescriptorSampledImages ||
        (m_data.maxPerStageStorageImages + m_data.maxPerStageStorageTexelBuffers) >
            properties.properties.limits.maxPerStageDescriptorStorageImages ||
        m_data.maxPerStageInputAttachments > properties.properties.limits.maxPerStageDescriptorInputAttachments)
    {
        TCU_THROW(NotSupportedError, "Number of descriptors not supported");
    }

#ifndef CTS_USES_VULKANSC
    if (m_data.maxInlineUniformBlocks != 0 && !inlineUniformFeatures.inlineUniformBlock)
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
#endif

    if (m_data.indexType == INDEX_TYPE_RUNTIME_SIZE && !indexingFeatures.runtimeDescriptorArray)
    {
        TCU_THROW(NotSupportedError, "runtimeDescriptorArray not supported");
    }
}

// Return a random value in the range [min, max]
int32_t randRange(deRandom *rnd, int32_t min, int32_t max)
{
    if (max < 0)
        return 0;

    return (deRandom_getUint32(rnd) % (max - min + 1)) + min;
}

void chooseWritesRandomly(vk::VkDescriptorType type, RandomLayout &randomLayout, deRandom &rnd, uint32_t set,
                          uint32_t binding, uint32_t count)
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

    for (uint32_t i = 0u; i < count; ++i)
    {
        // 1/2 chance of being a write.
        if (randRange(&rnd, 1, 2) == 1)
            randomLayout.descriptorWrites[DescriptorId(set, binding, i)] = {};
    }
}

void generateRandomLayout(RandomLayout &randomLayout, const CaseDef &caseDef, deRandom &rnd)
{
    // Count the number of each resource type, to avoid overflowing the limits.
    uint32_t numUBO        = 0;
    uint32_t numUBODyn     = 0;
    uint32_t numSSBO       = 0;
    uint32_t numSSBODyn    = 0;
    uint32_t numImage      = 0;
    uint32_t numStorageTex = 0;
    uint32_t numTexBuffer  = 0;
#ifndef CTS_USES_VULKANSC
    uint32_t numInlineUniformBlocks = 0;
#endif
    uint32_t numInputAttachments = 0;

    // TODO: Consider varying these
    uint32_t minBindings = 0;
    // Try to keep the workload roughly constant while exercising higher numbered sets.
    uint32_t maxBindings = 128u / caseDef.numDescriptorSets;
    // No larger than 32 elements for dynamic indexing tests, due to 128B limit
    // for push constants (used for the indices)
    uint32_t maxArray = caseDef.indexType == INDEX_TYPE_NONE ? 0 : 32;

    // Each set has a random number of bindings, each binding has a random
    // array size and a random descriptor type.
    for (uint32_t s = 0; s < caseDef.numDescriptorSets; ++s)
    {
        vector<VkDescriptorSetLayoutBinding> &bindings  = randomLayout.layoutBindings[s];
        vector<VkDescriptorBindingFlags> &bindingsFlags = randomLayout.layoutBindingFlags[s];
        vector<uint32_t> &arraySizes                    = randomLayout.arraySizes[s];
        int numBindings                                 = randRange(&rnd, minBindings, maxBindings);

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

        bindings      = vector<VkDescriptorSetLayoutBinding>(numBindings);
        bindingsFlags = vector<VkDescriptorBindingFlags>(numBindings);
        arraySizes    = vector<uint32_t>(numBindings);
    }

    // BUFFER_DYNAMIC descriptor types cannot be used with VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT bindings in one set
    bool allowDynamicBuffers = caseDef.uab != UPDATE_AFTER_BIND_ENABLED;

    // Iterate over bindings first, then over sets. This prevents the low-limit bindings
    // from getting clustered in low-numbered sets.
    for (uint32_t b = 0; b <= maxBindings; ++b)
    {
        for (uint32_t s = 0; s < caseDef.numDescriptorSets; ++s)
        {
            vector<VkDescriptorSetLayoutBinding> &bindings = randomLayout.layoutBindings[s];
            vector<uint32_t> &arraySizes                   = randomLayout.arraySizes[s];

            if (b >= bindings.size())
            {
                continue;
            }

            VkDescriptorSetLayoutBinding &binding = bindings[b];
            binding.binding                       = b;
            binding.pImmutableSamplers            = NULL;
            binding.stageFlags                    = caseDef.allShaderStages;

            // Output image
            if (s == 0 && b == 0)
            {
                binding.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                binding.descriptorCount = 1;
                binding.stageFlags      = caseDef.allShaderStages;
                numImage++;
                arraySizes[b] = 0;
                continue;
            }

#ifndef CTS_USES_VULKANSC
            // Raytracing acceleration structure
            if (s == 0 && b == 1 && usesAccelerationStructure(caseDef.stage))
            {
                binding.descriptorType  = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
                binding.descriptorCount = 1;
                binding.stageFlags      = caseDef.allShaderStages;
                arraySizes[b]           = 0;
                continue;
            }
#endif

            binding.descriptorCount = 0;

            // Select a random type of descriptor.
            std::map<int, vk::VkDescriptorType> intToType;
            {
                int index          = 0;
                intToType[index++] = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                intToType[index++] = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                intToType[index++] = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
                intToType[index++] = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                intToType[index++] = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
#ifndef CTS_USES_VULKANSC
                intToType[index++] = VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT;
#endif
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
            default:
                DE_ASSERT(0); // Fallthrough
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
                    arraySizes[b] =
                        randRange(&rnd, 0, de::min(maxArray, caseDef.maxPerStageStorageTexelBuffers - numStorageTex));
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
                    arraySizes[b] =
                        randRange(&rnd, 0, de::min(maxArray, caseDef.maxPerStageSampledImages - numTexBuffer));
                    binding.descriptorCount = arraySizes[b] ? arraySizes[b] : 1;
                    numTexBuffer += binding.descriptorCount;
                }
                break;
#ifndef CTS_USES_VULKANSC
            case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT:
                if (caseDef.maxInlineUniformBlocks > 0)
                {
                    if (numInlineUniformBlocks < caseDef.maxInlineUniformBlocks)
                    {
                        arraySizes[b] = randRange(
                            &rnd, 1, (caseDef.maxInlineUniformBlockSize - 16) / 16); // subtract 16 for "ivec4 unused"
                        arraySizes[b] = de::min(maxArray, arraySizes[b]);
                        binding.descriptorCount =
                            (arraySizes[b] ? arraySizes[b] : 1) * 16 + 16; // add 16 for "ivec4 unused"
                        numInlineUniformBlocks++;
                    }
                    else
                    {
                        // The meaning of descriptorCount for inline uniform blocks is diferrent from usual, which means
                        // (descriptorCount == 0) doesn't mean it will be discarded.
                        // So we use a similar trick to the below by replacing with a different type of descriptor.
                        binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                    }
                }
                else
                {
                    // Plug in an unused descriptor type, so validation layers that don't
                    // support inline_uniform_block don't crash.
                    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                }
                break;
#endif
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
                if (numUBODyn < caseDef.maxUniformBuffersDynamic && numUBO < caseDef.maxPerStageUniformBuffers)
                {
                    arraySizes[b]           = randRange(&rnd, 0,
                                                        de::min(maxArray, de::min(caseDef.maxUniformBuffersDynamic - numUBODyn,
                                                                                  caseDef.maxPerStageUniformBuffers - numUBO)));
                    binding.descriptorCount = arraySizes[b] ? arraySizes[b] : 1;
                    numUBO += binding.descriptorCount;
                    numUBODyn += binding.descriptorCount;
                }
                break;
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
                if (numSSBODyn < caseDef.maxStorageBuffersDynamic && numSSBO < caseDef.maxPerStageStorageBuffers)
                {
                    arraySizes[b]           = randRange(&rnd, 0,
                                                        de::min(maxArray, de::min(caseDef.maxStorageBuffersDynamic - numSSBODyn,
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
                    arraySizes[b] = randRange(
                        &rnd, 0, de::min(maxArray, caseDef.maxPerStageInputAttachments - numInputAttachments));
                    binding.descriptorCount = arraySizes[b] ? arraySizes[b] : 1;
                    numInputAttachments += binding.descriptorCount;
                }
                break;
            }

            binding.stageFlags = ((binding.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT) ?
                                      (VkFlags)(VK_SHADER_STAGE_FRAGMENT_BIT) :
                                      caseDef.allShaderStages);
        }
    }

    for (uint32_t s = 0; s < caseDef.numDescriptorSets; ++s)
    {
        vector<VkDescriptorSetLayoutBinding> &bindings  = randomLayout.layoutBindings[s];
        vector<VkDescriptorBindingFlags> &bindingsFlags = randomLayout.layoutBindingFlags[s];
        vector<uint32_t> &variableDescriptorSizes       = randomLayout.variableDescriptorSizes;

        // Choose a variable descriptor count size. If the feature is not supported, we'll just
        // allocate the whole thing later on.
        if (bindings.size() > 0 &&
            bindings[bindings.size() - 1].descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC &&
            bindings[bindings.size() - 1].descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC &&
            bindings[bindings.size() - 1].descriptorType != VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT &&
#ifndef CTS_USES_VULKANSC
            bindings[bindings.size() - 1].descriptorType != VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR &&
#endif
            bindings[bindings.size() - 1].descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_IMAGE &&
            !(s == 0 && bindings.size() == 1) && // Don't cut out the output image binding
            randRange(&rnd, 1, 4) == 1)          // 1 in 4 chance
        {

            bindingsFlags[bindings.size() - 1] |= VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
            variableDescriptorSizes[s] = randRange(&rnd, 0, bindings[bindings.size() - 1].descriptorCount);
#ifndef CTS_USES_VULKANSC
            if (bindings[bindings.size() - 1].descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
            {
                // keep a multiple of 16B
                variableDescriptorSizes[s] &= ~0xF;
            }
#endif
        }
    }
}

class CheckDecider
{
public:
    CheckDecider(deRandom &rnd, uint32_t descriptorCount)
        : m_rnd(rnd)
        , m_count(descriptorCount)
        , m_remainder(0u)
        , m_have_remainder(false)
    {
    }

    bool shouldCheck(uint32_t arrayIndex)
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
            m_remainder = static_cast<uint32_t>(
                randRange(&m_rnd, 0, static_cast<int32_t>((m_count / kRandomChecksPerBinding) - 1)));
            m_have_remainder = true;
        }

        return (arrayIndex % m_count == m_remainder);
    }

private:
    static constexpr uint32_t kRandomChecksPerBinding = 4u;

    deRandom &m_rnd;
    uint32_t m_count;
    uint32_t m_remainder;
    bool m_have_remainder;
};

void DescriptorSetRandomTestCase::initPrograms(SourceCollections &programCollection) const
{
    const vk::ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

    deRandom rnd;
    deRandom_init(&rnd, m_data.seed);

    m_data.randomLayout.reset(new RandomLayout(m_data.numDescriptorSets));
    RandomLayout &randomLayout = *m_data.randomLayout.get();
    generateRandomLayout(randomLayout, m_data, rnd);

    std::stringstream decls, checks;

    uint32_t inputAttachments = 0;
    uint32_t descriptor       = 0;

    for (uint32_t s = 0; s < m_data.numDescriptorSets; ++s)
    {
        vector<VkDescriptorSetLayoutBinding> &bindings = randomLayout.layoutBindings[s];
        vector<VkDescriptorBindingFlags> bindingsFlags = randomLayout.layoutBindingFlags[s];
        vector<uint32_t> &arraySizes                   = randomLayout.arraySizes[s];
        vector<uint32_t> &variableDescriptorSizes      = randomLayout.variableDescriptorSizes;

        for (size_t b = 0; b < bindings.size(); ++b)
        {
            VkDescriptorSetLayoutBinding &binding = bindings[b];
            uint32_t descriptorIncrement          = 1;
#ifndef CTS_USES_VULKANSC
            if (binding.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
                descriptorIncrement = 16;
#endif

            // Construct the declaration for the binding
            if (binding.descriptorCount > 0)
            {
                std::stringstream array;
                if (m_data.indexType == INDEX_TYPE_RUNTIME_SIZE
#ifndef CTS_USES_VULKANSC
                    && binding.descriptorType != VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT
#endif
                )
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
#ifndef CTS_USES_VULKANSC
                case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT:
                    decls << "layout(set = " << s << ", binding = " << b << ") uniform inlineubodef" << s << "_" << b
                          << " { ivec4 unused; int val" << array.str() << "; } inlineubo" << s << "_" << b << ";\n";
                    break;
#endif
                case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
                case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                    decls << "layout(set = " << s << ", binding = " << b << ") uniform ubodef" << s << "_" << b
                          << " { int val; } ubo" << s << "_" << b << array.str() << ";\n";
                    break;
                case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
                case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                    decls << "layout(set = " << s << ", binding = " << b << ") buffer sbodef" << s << "_" << b
                          << " { int val; } ssbo" << s << "_" << b << array.str() << ";\n";
                    break;
                case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
                    decls << "layout(set = " << s << ", binding = " << b << ") uniform itextureBuffer texbo" << s << "_"
                          << b << array.str() << ";\n";
                    break;
                case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
                    decls << "layout(r32i, set = " << s << ", binding = " << b << ") uniform iimageBuffer image" << s
                          << "_" << b << array.str() << ";\n";
                    break;
                case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                    decls << "layout(r32i, set = " << s << ", binding = " << b << ") uniform iimage2D simage" << s
                          << "_" << b << array.str() << ";\n";
                    break;
                case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
                    decls << "layout(input_attachment_index = " << inputAttachments << ", set = " << s
                          << ", binding = " << b << ") uniform isubpassInput attachment" << s << "_" << b << array.str()
                          << ";\n";
                    inputAttachments += binding.descriptorCount;
                    break;
#ifndef CTS_USES_VULKANSC
                case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
                    DE_ASSERT(s == 0 && b == 1);
                    DE_ASSERT(bindings.size() >= 2);
                    decls << "layout(set = " << s << ", binding = " << b << ") uniform accelerationStructureEXT as" << s
                          << "_" << b << ";\n";
                    break;
#endif
                default:
                    DE_ASSERT(0);
                }

                const uint32_t arraySize = de::max(1u, arraySizes[b]);
                CheckDecider checkDecider(rnd, arraySize);

                for (uint32_t ai = 0; ai < arraySize; ++ai, descriptor += descriptorIncrement)
                {
                    // Don't access descriptors past the end of the allocated range for
                    // variable descriptor count
                    if (b == bindings.size() - 1 &&
                        (bindingsFlags[b] & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT))
                    {
#ifndef CTS_USES_VULKANSC
                        if (binding.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
                        {
                            // Convert to bytes and add 16 for "ivec4 unused" in case of inline uniform block
                            const uint32_t uboRange = ai * 16 + 16;
                            if (uboRange >= variableDescriptorSizes[s])
                                continue;
                        }
                        else
#endif
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
                        default:
                            DE_ASSERT(0);
                        }

                        const DescriptorId descriptorId(s, static_cast<uint32_t>(b), ai);
                        auto writesItr = randomLayout.descriptorWrites.find(descriptorId);

                        if (writesItr == randomLayout.descriptorWrites.end())
                        {
                            // Fetch from the descriptor.
                            switch (binding.descriptorType)
                            {
#ifndef CTS_USES_VULKANSC
                            case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT:
                                checks << "  temp = inlineubo" << s << "_" << b << ".val" << ind.str() << ";\n";
                                break;
#endif
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
                                checks << "  temp = imageLoad(simage" << s << "_" << b << ind.str()
                                       << ", ivec2(0, 0)).x;\n";
                                break;
                            case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
                                checks << "  temp = subpassLoad(attachment" << s << "_" << b << ind.str() << ").r;\n";
                                break;
                            default:
                                DE_ASSERT(0);
                            }

                            // Accumulate any incorrect values.
                            checks << "  accum |= temp - " << descriptor << ";\n";
                        }
                        else
                        {
                            // Check descriptor write. We need to confirm we are actually generating write code for this descriptor.
                            writesItr->second.writeGenerated = true;

                            // Assign each write operation to a single invocation to avoid race conditions.
                            const auto expectedInvocationID = descriptor % (DIM * DIM);
                            const std::string writeCond =
                                "if (" + de::toString(expectedInvocationID) + " == invocationID)";

                            switch (binding.descriptorType)
                            {
                            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
                            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                                checks << "  " << writeCond << " ssbo" << s << "_" << b << ind.str()
                                       << ".val = " << descriptor << ";\n";
                                break;
                            case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
                                checks << "  " << writeCond << " imageStore(image" << s << "_" << b << ind.str()
                                       << ", 0, ivec4(" << descriptor << ", 0, 0, 0));\n";
                                break;
                            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                                checks << "  " << writeCond << " imageStore(simage" << s << "_" << b << ind.str()
                                       << ", ivec2(0, 0), ivec4(" << descriptor << ", 0, 0, 0));\n";
                                break;
                            default:
                                DE_ASSERT(0);
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
    default:
        DE_ASSERT(0);
    case INDEX_TYPE_NONE:
    case INDEX_TYPE_CONSTANT:
    case INDEX_TYPE_DEPENDENT:
    case INDEX_TYPE_RUNTIME_SIZE:
        break;
    }

    switch (m_data.stage)
    {
    default:
        DE_ASSERT(0); // Fallthrough
    case STAGE_COMPUTE:
    {
        std::stringstream css;
        css << "#version 450 core\n"
               "#extension GL_EXT_nonuniform_qualifier : enable\n"
            << pushdecl.str() << decls.str()
            << "layout(local_size_x = 1, local_size_y = 1) in;\n"
               "void main()\n"
               "{\n"
               "  const int invocationID = int(gl_GlobalInvocationID.y) * "
            << DIM
            << " + int(gl_GlobalInvocationID.x);\n"
               "  int accum = 0, temp;\n"
            << checks.str()
            << "  ivec4 color = (accum != 0) ? ivec4(0,0,0,0) : ivec4(1,0,0,1);\n"
               "  imageStore(simage0_0, ivec2(gl_GlobalInvocationID.xy), color);\n"
               "}\n";

        programCollection.glslSources.add("test") << glu::ComputeSource(css.str());
        break;
    }
#ifndef CTS_USES_VULKANSC
    case STAGE_RAYGEN_NV:
    {
        std::stringstream css;
        css << "#version 460 core\n"
               "#extension GL_EXT_nonuniform_qualifier : enable\n"
               "#extension GL_NV_ray_tracing : require\n"
            << pushdecl.str() << decls.str()
            << "void main()\n"
               "{\n"
               "  const int invocationID = int(gl_LaunchIDNV.y) * "
            << DIM
            << " + int(gl_LaunchIDNV.x);\n"
               "  int accum = 0, temp;\n"
            << checks.str()
            << "  ivec4 color = (accum != 0) ? ivec4(0,0,0,0) : ivec4(1,0,0,1);\n"
               "  imageStore(simage0_0, ivec2(gl_LaunchIDNV.xy), color);\n"
               "}\n";

        programCollection.glslSources.add("test") << glu::RaygenSource(css.str());
        break;
    }
    case STAGE_RAYGEN:
    {
        std::stringstream css;
        css << "#version 460 core\n"
               "#extension GL_EXT_nonuniform_qualifier : enable\n"
               "#extension GL_EXT_ray_tracing : require\n"
            << pushdecl.str() << decls.str()
            << "void main()\n"
               "{\n"
               "  const int invocationID = int(gl_LaunchIDEXT.y) * "
            << DIM
            << " + int(gl_LaunchIDEXT.x);\n"
               "  int accum = 0, temp;\n"
            << checks.str()
            << "  ivec4 color = (accum != 0) ? ivec4(0,0,0,0) : ivec4(1,0,0,1);\n"
               "  imageStore(simage0_0, ivec2(gl_LaunchIDEXT.xy), color);\n"
               "}\n";

        programCollection.glslSources.add("test") << glu::RaygenSource(updateRayTracingGLSL(css.str())) << buildOptions;
        break;
    }
    case STAGE_INTERSECT:
    {
        {
            programCollection.glslSources.add("rgen")
                << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;
        }

        {
            std::stringstream css;
            css << "#version 460 core\n"
                   "#extension GL_EXT_nonuniform_qualifier : enable\n"
                   "#extension GL_EXT_ray_tracing : require\n"
                   "hitAttributeEXT vec3 hitAttribute;\n"
                << pushdecl.str() << decls.str()
                << "void main()\n"
                   "{\n"
                   "  const int invocationID = int(gl_LaunchIDEXT.y) * "
                << DIM
                << " + int(gl_LaunchIDEXT.x);\n"
                   "  int accum = 0, temp;\n"
                << checks.str()
                << "  ivec4 color = (accum != 0) ? ivec4(0,0,0,0) : ivec4(1,0,0,1);\n"
                   "  imageStore(simage0_0, ivec2(gl_LaunchIDEXT.xy), color);\n"
                   "  hitAttribute = vec3(0.0f, 0.0f, 0.0f);\n"
                   "  reportIntersectionEXT(1.0f, 0);\n"
                   "}\n";

            programCollection.glslSources.add("test")
                << glu::IntersectionSource(updateRayTracingGLSL(css.str())) << buildOptions;
        }

        break;
    }
    case STAGE_ANY_HIT:
    {
        {
            programCollection.glslSources.add("rgen")
                << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;
        }

        {
            std::stringstream css;
            css << "#version 460 core\n"
                   "#extension GL_EXT_nonuniform_qualifier : enable\n"
                   "#extension GL_EXT_ray_tracing : require\n"
                   "layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
                   "hitAttributeEXT vec3 attribs;\n"
                << pushdecl.str() << decls.str()
                << "void main()\n"
                   "{\n"
                   "  const int invocationID = int(gl_LaunchIDEXT.y) * "
                << DIM
                << " + int(gl_LaunchIDEXT.x);\n"
                   "  int accum = 0, temp;\n"
                << checks.str()
                << "  ivec4 color = (accum != 0) ? ivec4(0,0,0,0) : ivec4(1,0,0,1);\n"
                   "  imageStore(simage0_0, ivec2(gl_LaunchIDEXT.xy), color);\n"
                   "}\n";

            programCollection.glslSources.add("test")
                << glu::AnyHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
        }

        break;
    }
    case STAGE_CLOSEST_HIT:
    {
        {
            programCollection.glslSources.add("rgen")
                << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;
        }

        {
            std::stringstream css;
            css << "#version 460 core\n"
                   "#extension GL_EXT_nonuniform_qualifier : enable\n"
                   "#extension GL_EXT_ray_tracing : require\n"
                   "layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
                   "hitAttributeEXT vec3 attribs;\n"
                << pushdecl.str() << decls.str()
                << "void main()\n"
                   "{\n"
                   "  const int invocationID = int(gl_LaunchIDEXT.y) * "
                << DIM
                << " + int(gl_LaunchIDEXT.x);\n"
                   "  int accum = 0, temp;\n"
                << checks.str()
                << "  ivec4 color = (accum != 0) ? ivec4(0,0,0,0) : ivec4(1,0,0,1);\n"
                   "  imageStore(simage0_0, ivec2(gl_LaunchIDEXT.xy), color);\n"
                   "}\n";

            programCollection.glslSources.add("test")
                << glu::ClosestHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
        }

        break;
    }
    case STAGE_MISS:
    {
        {
            programCollection.glslSources.add("rgen")
                << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;
        }

        {
            std::stringstream css;
            css << "#version 460 core\n"
                   "#extension GL_EXT_nonuniform_qualifier : enable\n"
                   "#extension GL_EXT_ray_tracing : require\n"
                   "layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
                << pushdecl.str() << decls.str()
                << "void main()\n"
                   "{\n"
                   "  const int invocationID = int(gl_LaunchIDEXT.y) * "
                << DIM
                << " + int(gl_LaunchIDEXT.x);\n"
                   "  int accum = 0, temp;\n"
                << checks.str()
                << "  ivec4 color = (accum != 0) ? ivec4(0,0,0,0) : ivec4(1,0,0,1);\n"
                   "  imageStore(simage0_0, ivec2(gl_LaunchIDEXT.xy), color);\n"
                   "}\n";

            programCollection.glslSources.add("test")
                << glu::MissSource(updateRayTracingGLSL(css.str())) << buildOptions;
        }

        break;
    }
    case STAGE_CALLABLE:
    {
        {
            std::stringstream css;
            css << "#version 460 core\n"
                   "#extension GL_EXT_nonuniform_qualifier : enable\n"
                   "#extension GL_EXT_ray_tracing : require\n"
                   "layout(location = 0) callableDataEXT float dummy;"
                   "layout(set = 0, binding = 1) uniform accelerationStructureEXT topLevelAS;\n"
                   "\n"
                   "void main()\n"
                   "{\n"
                   "  executeCallableEXT(0, 0);\n"
                   "}\n";

            programCollection.glslSources.add("rgen")
                << glu::RaygenSource(updateRayTracingGLSL(css.str())) << buildOptions;
        }

        {
            std::stringstream css;
            css << "#version 460 core\n"
                   "#extension GL_EXT_nonuniform_qualifier : enable\n"
                   "#extension GL_EXT_ray_tracing : require\n"
                   "layout(location = 0) callableDataInEXT float dummy;"
                << pushdecl.str() << decls.str()
                << "void main()\n"
                   "{\n"
                   "  const int invocationID = int(gl_LaunchIDEXT.y) * "
                << DIM
                << " + int(gl_LaunchIDEXT.x);\n"
                   "  int accum = 0, temp;\n"
                << checks.str()
                << "  ivec4 color = (accum != 0) ? ivec4(0,0,0,0) : ivec4(1,0,0,1);\n"
                   "  imageStore(simage0_0, ivec2(gl_LaunchIDEXT.xy), color);\n"
                   "}\n";

            programCollection.glslSources.add("test")
                << glu::CallableSource(updateRayTracingGLSL(css.str())) << buildOptions;
        }
        break;
    }
#endif
    case STAGE_VERTEX:
    {
        std::stringstream vss;
        vss << "#version 450 core\n"
               "#extension GL_EXT_nonuniform_qualifier : enable\n"
            << pushdecl.str() << decls.str()
            << "void main()\n"
               "{\n"
               "  const int invocationID = gl_VertexIndex;\n"
               "  int accum = 0, temp;\n"
            << checks.str()
            << "  ivec4 color = (accum != 0) ? ivec4(0,0,0,0) : ivec4(1,0,0,1);\n"
               "  imageStore(simage0_0, ivec2(gl_VertexIndex % "
            << DIM << ", gl_VertexIndex / " << DIM
            << "), color);\n"
               "  gl_PointSize = 1.0f;\n"
               "  gl_Position = vec4(0.0f, 0.0f, 0.0f, 1.0f);\n"
               "}\n";

        programCollection.glslSources.add("test") << glu::VertexSource(vss.str());
        break;
    }
    case STAGE_TASK:
    {
        std::stringstream task;
        task << "#version 450\n"
             << "#extension GL_EXT_mesh_shader : enable\n"
             << "#extension GL_EXT_nonuniform_qualifier : enable\n"
             << pushdecl.str() << decls.str() << "layout(local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
             << "void main()\n"
             << "{\n"
             << "  const int invocationID = int(gl_GlobalInvocationID.y) * " << DIM
             << " + int(gl_GlobalInvocationID.x);\n"
             << "  int accum = 0, temp;\n"
             << checks.str() << "  ivec4 color = (accum != 0) ? ivec4(0,0,0,0) : ivec4(1,0,0,1);\n"
             << "  imageStore(simage0_0, ivec2(gl_GlobalInvocationID.xy), color);\n"
             << "  EmitMeshTasksEXT(0, 0, 0);\n"
             << "}\n";
        programCollection.glslSources.add("test") << glu::TaskSource(task.str()) << buildOptions;

        std::stringstream mesh;
        mesh << "#version 450\n"
             << "#extension GL_EXT_mesh_shader : enable\n"
             << "#extension GL_EXT_nonuniform_qualifier : enable\n"
             << "layout(local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
             << "layout(triangles) out;\n"
             << "layout(max_vertices=3, max_primitives=1) out;\n"
             << "void main()\n"
             << "{\n"
             << "  SetMeshOutputsEXT(0, 0);\n"
             << "}\n";
        programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << buildOptions;

        break;
    }
    case STAGE_MESH:
    {
        std::stringstream mesh;
        mesh << "#version 450\n"
             << "#extension GL_EXT_mesh_shader : enable\n"
             << "#extension GL_EXT_nonuniform_qualifier : enable\n"
             << pushdecl.str() << decls.str() << "layout(local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
             << "layout(triangles) out;\n"
             << "layout(max_vertices=3, max_primitives=1) out;\n"
             << "void main()\n"
             << "{\n"
             << "  const int invocationID = int(gl_GlobalInvocationID.y) * " << DIM
             << " + int(gl_GlobalInvocationID.x);\n"
             << "  int accum = 0, temp;\n"
             << checks.str() << "  ivec4 color = (accum != 0) ? ivec4(0,0,0,0) : ivec4(1,0,0,1);\n"
             << "  imageStore(simage0_0, ivec2(gl_GlobalInvocationID.xy), color);\n"
             << "}\n";
        programCollection.glslSources.add("test") << glu::MeshSource(mesh.str()) << buildOptions;

        break;
    }
    case STAGE_FRAGMENT:
    {
        std::stringstream vss;
        vss << "#version 450 core\n"
               "void main()\n"
               "{\n"
               // full-viewport quad
               "  gl_Position = vec4( 2.0*float(gl_VertexIndex&2) - 1.0, 4.0*(gl_VertexIndex&1)-1.0, 1.0 - 2.0 * "
               "float(gl_VertexIndex&1), 1);\n"
               "}\n";

        programCollection.glslSources.add("vert") << glu::VertexSource(vss.str());

        std::stringstream fss;
        fss << "#version 450 core\n"
               "#extension GL_EXT_nonuniform_qualifier : enable\n"
            << pushdecl.str() << decls.str()
            << "void main()\n"
               "{\n"
               "  const int invocationID = int(gl_FragCoord.y) * "
            << DIM
            << " + int(gl_FragCoord.x);\n"
               "  int accum = 0, temp;\n"
            << checks.str()
            << "  ivec4 color = (accum != 0) ? ivec4(0,0,0,0) : ivec4(1,0,0,1);\n"
               "  imageStore(simage0_0, ivec2(gl_FragCoord.x, gl_FragCoord.y), color);\n"
               "}\n";

        programCollection.glslSources.add("test") << glu::FragmentSource(fss.str());
        break;
    }
    }
}

TestInstance *DescriptorSetRandomTestCase::createInstance(Context &context) const
{
    return new DescriptorSetRandomTestInstance(context, m_data_ptr);
}

void appendShaderStageCreateInfo(std::vector<VkPipelineShaderStageCreateInfo> &vec, VkShaderModule module,
                                 VkShaderStageFlagBits stage)
{
    const VkPipelineShaderStageCreateInfo info = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                             // const void* pNext;
        0u,                                                  // VkPipelineShaderStageCreateFlags flags;
        stage,                                               // VkShaderStageFlagBits stage;
        module,                                              // VkShaderModule module;
        "main",                                              // const char* pName;
        nullptr,                                             // const VkSpecializationInfo* pSpecializationInfo;
    };

    vec.push_back(info);
}

tcu::TestStatus DescriptorSetRandomTestInstance::iterate(void)
{
    const InstanceInterface &vki          = m_context.getInstanceInterface();
    const DeviceInterface &vk             = m_context.getDeviceInterface();
    const VkDevice device                 = m_context.getDevice();
    const VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();
    Allocator &allocator                  = m_context.getDefaultAllocator();
    const uint32_t queueFamilyIndex       = m_context.getUniversalQueueFamilyIndex();

    deRandom rnd;
    VkPhysicalDeviceProperties2 properties = getPhysicalDeviceExtensionProperties(vki, physicalDevice);
#ifndef CTS_USES_VULKANSC
    uint32_t shaderGroupHandleSize    = 0;
    uint32_t shaderGroupBaseAlignment = 1;
#endif

    deRandom_init(&rnd, m_data.seed);
    RandomLayout &randomLayout = *m_data.randomLayout.get();

#ifndef CTS_USES_VULKANSC
    if (m_data.stage == STAGE_RAYGEN_NV)
    {
        const VkPhysicalDeviceRayTracingPropertiesNV rayTracingProperties =
            getPhysicalDeviceExtensionProperties(vki, physicalDevice);

        shaderGroupHandleSize = rayTracingProperties.shaderGroupHandleSize;
    }

    if (isRayTracingStageKHR(m_data.stage))
    {
        de::MovePtr<RayTracingProperties> rayTracingPropertiesKHR;

        rayTracingPropertiesKHR  = makeRayTracingProperties(vki, physicalDevice);
        shaderGroupHandleSize    = rayTracingPropertiesKHR->getShaderGroupHandleSize();
        shaderGroupBaseAlignment = rayTracingPropertiesKHR->getShaderGroupBaseAlignment();
    }
#endif

    // Get needed features.
    auto descriptorIndexingSupported = m_context.isDeviceFunctionalitySupported("VK_EXT_descriptor_indexing");
    auto indexingFeatures            = m_context.getDescriptorIndexingFeatures();
#ifndef CTS_USES_VULKANSC
    auto inlineUniformFeatures = m_context.getInlineUniformBlockFeatures();
#endif

    VkPipelineBindPoint bindPoint;

    switch (m_data.stage)
    {
    case STAGE_COMPUTE:
        bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
        break;
#ifndef CTS_USES_VULKANSC
    case STAGE_RAYGEN_NV:
        bindPoint = VK_PIPELINE_BIND_POINT_RAY_TRACING_NV;
        break;
#endif
    default:
        bindPoint =
#ifndef CTS_USES_VULKANSC
            isRayTracingStageKHR(m_data.stage) ? VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR :
#endif
                                                 VK_PIPELINE_BIND_POINT_GRAPHICS;
        break;
    }

    DE_ASSERT(m_data.numDescriptorSets <= 32);
    Move<vk::VkDescriptorSetLayout> descriptorSetLayouts[32];
    Move<vk::VkDescriptorPool> descriptorPools[32];
    Move<vk::VkDescriptorSet> descriptorSets[32];

    uint32_t numDescriptors = 0;
    for (uint32_t s = 0; s < m_data.numDescriptorSets; ++s)
    {
        vector<VkDescriptorSetLayoutBinding> &bindings  = randomLayout.layoutBindings[s];
        vector<VkDescriptorBindingFlags> &bindingsFlags = randomLayout.layoutBindingFlags[s];
        vector<uint32_t> &variableDescriptorSizes       = randomLayout.variableDescriptorSizes;

        VkDescriptorPoolCreateFlags poolCreateFlags        = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        VkDescriptorSetLayoutCreateFlags layoutCreateFlags = 0;

        for (size_t b = 0; b < bindings.size(); ++b)
        {
            VkDescriptorSetLayoutBinding &binding = bindings[b];
            numDescriptors += binding.descriptorCount;

            // Randomly choose some bindings to use update-after-bind, if it is supported
            if (descriptorIndexingSupported && m_data.uab == UPDATE_AFTER_BIND_ENABLED &&
                randRange(&rnd, 1, 8) == 1 && // 1 in 8 chance
                (binding.descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
                 indexingFeatures.descriptorBindingUniformBufferUpdateAfterBind) &&
                (binding.descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
                 indexingFeatures.descriptorBindingStorageImageUpdateAfterBind) &&
                (binding.descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
                 indexingFeatures.descriptorBindingStorageBufferUpdateAfterBind) &&
                (binding.descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
                 indexingFeatures.descriptorBindingUniformTexelBufferUpdateAfterBind) &&
                (binding.descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER ||
                 indexingFeatures.descriptorBindingStorageTexelBufferUpdateAfterBind) &&
#ifndef CTS_USES_VULKANSC
                (binding.descriptorType != VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT ||
                 inlineUniformFeatures.descriptorBindingInlineUniformBlockUpdateAfterBind) &&
#endif
                (binding.descriptorType != VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT) &&
                (binding.descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) &&
                (binding.descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
#ifndef CTS_USES_VULKANSC
                && (binding.descriptorType != VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
#endif
            )
            {
                bindingsFlags[b] |= VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
                layoutCreateFlags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
                poolCreateFlags |= VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
            }

            if (!indexingFeatures.descriptorBindingVariableDescriptorCount)
            {
                bindingsFlags[b] &= ~VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
            }
        }

        // Create a layout and allocate a descriptor set for it.

        const VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                           // const void* pNext;
            (uint32_t)bindings.size(),                                         // uint32_t bindingCount;
            bindings.empty() ? nullptr : bindingsFlags.data(), // const VkDescriptorBindingFlags* pBindingFlags;
        };

        const VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo = {
            vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,     //  VkStructureType sType;
            (descriptorIndexingSupported ? &bindingFlagsInfo : nullptr), //  const void* pNext;
            layoutCreateFlags,                                           //  VkDescriptorSetLayoutCreateFlags flags;
            (uint32_t)bindings.size(),                                   //  uint32_t bindingCount;
            bindings.empty() ? nullptr : bindings.data() //  const VkDescriptorSetLayoutBinding* pBindings;
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
#ifndef CTS_USES_VULKANSC
        if (m_data.maxInlineUniformBlocks > 0u)
        {
            poolBuilder.addType(VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT,
                                m_data.maxInlineUniformBlocks * m_data.maxInlineUniformBlockSize);
        }
        if (usesAccelerationStructure(m_data.stage))
        {
            poolBuilder.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1u);
        }

        VkDescriptorPoolInlineUniformBlockCreateInfoEXT inlineUniformBlockPoolCreateInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_INLINE_UNIFORM_BLOCK_CREATE_INFO_EXT, // VkStructureType sType;
            nullptr,                                                                // const void* pNext;
            m_data.maxInlineUniformBlocks, // uint32_t maxInlineUniformBlockBindings;
        };
#endif
        descriptorPools[s] = poolBuilder.build(vk, device, poolCreateFlags, 1u,
#ifndef CTS_USES_VULKANSC
                                               m_data.maxInlineUniformBlocks ? &inlineUniformBlockPoolCreateInfo :
#endif
                                                                               nullptr);

        VkDescriptorSetVariableDescriptorCountAllocateInfo variableCountInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO, // VkStructureType sType;
            nullptr,                                                                  // const void* pNext;
            0,                                                                        // uint32_t descriptorSetCount;
            nullptr, // const uint32_t* pDescriptorCounts;
        };

        const void *pNext = nullptr;
        if (bindings.size() > 0 &&
            bindingsFlags[bindings.size() - 1] & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT)
        {
            variableCountInfo.descriptorSetCount = 1;
            variableCountInfo.pDescriptorCounts  = &variableDescriptorSizes[s];
            pNext                                = &variableCountInfo;
        }

        descriptorSets[s] = makeDescriptorSet(vk, device, *descriptorPools[s], *descriptorSetLayouts[s], pNext);
    }

    // Create a buffer to hold data for all descriptors.
    VkDeviceSize align =
        std::max({properties.properties.limits.minTexelBufferOffsetAlignment,
                  properties.properties.limits.minUniformBufferOffsetAlignment,
                  properties.properties.limits.minStorageBufferOffsetAlignment, (VkDeviceSize)sizeof(uint32_t)});

    de::MovePtr<BufferWithMemory> buffer;

    buffer             = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
        vk, device, allocator,
        makeBufferCreateInfo(align * numDescriptors,
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                             VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT),
        MemoryRequirement::HostVisible));
    uint8_t *bufferPtr = (uint8_t *)buffer->getAllocation().getHostPtr();

    // Create storage images separately.
    uint32_t storageImageCount = 0u;
    vector<Move<VkImage>> storageImages;

    const VkImageCreateInfo storageImgCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        0u,                                  // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        VK_FORMAT_R32_SINT,                  // VkFormat format;
        {1u, 1u, 1u},                        // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        1u,                                  // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
            VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        1u,                                  // uint32_t queueFamilyIndexCount;
        &queueFamilyIndex,                   // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED            // VkImageLayout initialLayout;
    };

    // Create storage images.
    for (const auto &bindings : randomLayout.layoutBindings)
        for (const auto &binding : bindings)
        {
            if (binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
            {
                storageImageCount += binding.descriptorCount;
                for (uint32_t d = 0; d < binding.descriptorCount; ++d)
                {
                    storageImages.push_back(createImage(vk, device, &storageImgCreateInfo));
                }
            }
        }

    // Allocate memory for them.
    vk::VkMemoryRequirements storageImageMemReqs;
    vk.getImageMemoryRequirements(device, *storageImages.front(), &storageImageMemReqs);

    de::MovePtr<Allocation> storageImageAlloc;
    VkDeviceSize storageImageBlockSize = 0u;
    {
        VkDeviceSize mod      = (storageImageMemReqs.size % storageImageMemReqs.alignment);
        storageImageBlockSize = storageImageMemReqs.size + ((mod == 0u) ? 0u : storageImageMemReqs.alignment - mod);
    }
    storageImageMemReqs.size = storageImageBlockSize * storageImageCount;
    storageImageAlloc        = allocator.allocate(storageImageMemReqs, MemoryRequirement::Any);

    // Allocate buffer to copy storage images to.
    auto storageImgBuffer        = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
        vk, device, allocator,
        makeBufferCreateInfo(storageImageCount * sizeof(int32_t), VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        MemoryRequirement::HostVisible));
    int32_t *storageImgBufferPtr = reinterpret_cast<int32_t *>(storageImgBuffer->getAllocation().getHostPtr());

    // Create image views.
    vector<Move<VkImageView>> storageImageViews;
    {
        VkImageViewCreateInfo storageImageViewCreateInfo = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
            nullptr,                                  // const void* pNext;
            0u,                                       // VkImageViewCreateFlags flags;
            VK_NULL_HANDLE,                           // VkImage image;
            VK_IMAGE_VIEW_TYPE_2D,                    // VkImageViewType viewType;
            VK_FORMAT_R32_SINT,                       // VkFormat format;
            {                                         // VkComponentMapping channels;
             VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
             VK_COMPONENT_SWIZZLE_IDENTITY},
            {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u} // VkImageSubresourceRange subresourceRange;
        };

        for (uint32_t i = 0; i < static_cast<uint32_t>(storageImages.size()); ++i)
        {
            // Bind image memory.
            vk::VkImage img = *storageImages[i];
            VK_CHECK(vk.bindImageMemory(device, img, storageImageAlloc->getMemory(),
                                        storageImageAlloc->getOffset() + i * storageImageBlockSize));

            // Create view.
            storageImageViewCreateInfo.image = img;
            storageImageViews.push_back(createImageView(vk, device, &storageImageViewCreateInfo));
        }
    }

    // Create input attachment images.
    vector<Move<VkImage>> inputAttachments;
    const VkImageCreateInfo imgCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                                     // VkStructureType sType;
        nullptr,                                                                 // const void* pNext;
        0u,                                                                      // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                                                        // VkImageType imageType;
        VK_FORMAT_R32_SINT,                                                      // VkFormat format;
        {DIM, DIM, 1u},                                                          // VkExtent3D extent;
        1u,                                                                      // uint32_t mipLevels;
        1u,                                                                      // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,                                                   // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,                                                 // VkImageTiling tiling;
        (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT), // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,                                               // VkSharingMode sharingMode;
        1u,                                                                      // uint32_t queueFamilyIndexCount;
        &queueFamilyIndex,                                                       // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED                                                // VkImageLayout initialLayout;

    };

    uint32_t inputAttachmentCount = 0u;
    for (const auto &bindings : randomLayout.layoutBindings)
        for (const auto &binding : bindings)
        {
            if (binding.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
            {
                inputAttachmentCount += binding.descriptorCount;
                for (uint32_t d = 0; d < binding.descriptorCount; ++d)
                {
                    inputAttachments.push_back(createImage(vk, device, &imgCreateInfo));
                }
            }
        }

    de::MovePtr<Allocation> inputAttachmentAlloc;
    VkDeviceSize imageBlockSize = 0u;

    if (inputAttachmentCount > 0u)
    {
        VkMemoryRequirements imageReqs = getImageMemoryRequirements(vk, device, inputAttachments.back().get());
        VkDeviceSize mod               = imageReqs.size % imageReqs.alignment;

        // Create memory for every input attachment image.
        imageBlockSize       = imageReqs.size + ((mod == 0u) ? 0u : (imageReqs.alignment - mod));
        imageReqs.size       = imageBlockSize * inputAttachmentCount;
        inputAttachmentAlloc = allocator.allocate(imageReqs, MemoryRequirement::Any);
    }

    // Bind memory to each input attachment and create an image view.
    VkImageViewCreateInfo inputAttachmentViewParams = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
        nullptr,                                  // const void* pNext;
        0u,                                       // VkImageViewCreateFlags flags;
        VK_NULL_HANDLE,                           // VkImage image;
        VK_IMAGE_VIEW_TYPE_2D,                    // VkImageViewType viewType;
        VK_FORMAT_R32_SINT,                       // VkFormat format;
        {                                         // VkComponentMapping channels;
         VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
         VK_COMPONENT_SWIZZLE_IDENTITY},
        {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u} // VkImageSubresourceRange subresourceRange;
    };
    vector<Move<VkImageView>> inputAttachmentViews;

    for (uint32_t i = 0; i < static_cast<uint32_t>(inputAttachments.size()); ++i)
    {
        vk::VkImage img = *inputAttachments[i];
        VK_CHECK(vk.bindImageMemory(device, img, inputAttachmentAlloc->getMemory(),
                                    inputAttachmentAlloc->getOffset() + i * imageBlockSize));

        inputAttachmentViewParams.image = img;
        inputAttachmentViews.push_back(createImageView(vk, device, &inputAttachmentViewParams));
    }

    // Create a view for each descriptor. Fill descriptor 'd' with an integer value equal to 'd'. In case the descriptor would be
    // written to from the shader, store a -1 in it instead. Skip inline uniform blocks and use images for input attachments and
    // storage images.

    Move<VkCommandPool> cmdPool     = createCommandPool(vk, device, 0, queueFamilyIndex);
    const VkQueue queue             = m_context.getUniversalQueue();
    Move<VkCommandBuffer> cmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    const VkImageSubresourceRange clearRange = {
        VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
        0u,                        // uint32_t baseMipLevel;
        1u,                        // uint32_t levelCount;
        0u,                        // uint32_t baseArrayLayer;
        1u                         // uint32_t layerCount;
    };

    VkImageMemoryBarrier preInputAttachmentBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType        sType
        nullptr,                                // const void*            pNext
        0u,                                     // VkAccessFlags        srcAccessMask
        VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags        dstAccessMask
        VK_IMAGE_LAYOUT_UNDEFINED,              // VkImageLayout        oldLayout
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout        newLayout
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t                srcQueueFamilyIndex
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t                dstQueueFamilyIndex
        VK_NULL_HANDLE,                         // VkImage                image
        {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags    aspectMask
            0u,                        // uint32_t                baseMipLevel
            1u,                        // uint32_t                mipLevels,
            0u,                        // uint32_t                baseArray
            1u,                        // uint32_t                arraySize
        }};

    VkImageMemoryBarrier postInputAttachmentBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,   // VkStructureType sType;
        nullptr,                                  // const void* pNext;
        VK_ACCESS_TRANSFER_WRITE_BIT,             // VkAccessFlags srcAccessMask;
        VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,      // VkAccessFlags dstAccessMask;
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,     // VkImageLayout oldLayout;
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, // VkImageLayout newLayout;
        VK_QUEUE_FAMILY_IGNORED,                  // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                  // uint32_t dstQueueFamilyIndex;
        VK_NULL_HANDLE,                           // VkImage image;
        clearRange,                               // VkImageSubresourceRange subresourceRange;
    };

    VkImageMemoryBarrier preStorageImageBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType        sType
        nullptr,                                // const void*            pNext
        0u,                                     // VkAccessFlags        srcAccessMask
        VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags        dstAccessMask
        VK_IMAGE_LAYOUT_UNDEFINED,              // VkImageLayout        oldLayout
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout        newLayout
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t                srcQueueFamilyIndex
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t                dstQueueFamilyIndex
        VK_NULL_HANDLE,                         // VkImage                image
        {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags    aspectMask
            0u,                        // uint32_t                baseMipLevel
            1u,                        // uint32_t                mipLevels,
            0u,                        // uint32_t                baseArray
            1u,                        // uint32_t                arraySize
        }};

    VkImageMemoryBarrier postStorageImageBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,                   // VkStructureType sType;
        nullptr,                                                  // const void* pNext;
        VK_ACCESS_TRANSFER_WRITE_BIT,                             // VkAccessFlags srcAccessMask;
        (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT), // VkAccessFlags dstAccessMask;
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,                     // VkImageLayout oldLayout;
        VK_IMAGE_LAYOUT_GENERAL,                                  // VkImageLayout newLayout;
        VK_QUEUE_FAMILY_IGNORED,                                  // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                                  // uint32_t dstQueueFamilyIndex;
        VK_NULL_HANDLE,                                           // VkImage image;
        clearRange,                                               // VkImageSubresourceRange subresourceRange;
    };

    vk::VkClearColorValue clearValue;
    clearValue.uint32[0] = 0u;
    clearValue.uint32[1] = 0u;
    clearValue.uint32[2] = 0u;
    clearValue.uint32[3] = 0u;

    beginCommandBuffer(vk, *cmdBuffer, 0u);

    int descriptor           = 0;
    uint32_t attachmentIndex = 0;
    uint32_t storageImgIndex = 0;

    typedef vk::Unique<vk::VkBufferView> BufferViewHandleUp;
    typedef de::SharedPtr<BufferViewHandleUp> BufferViewHandleSp;

    vector<BufferViewHandleSp> bufferViews(de::max(1u, numDescriptors));

    for (uint32_t s = 0; s < m_data.numDescriptorSets; ++s)
    {
        vector<VkDescriptorSetLayoutBinding> &bindings = randomLayout.layoutBindings[s];
        for (size_t b = 0; b < bindings.size(); ++b)
        {
            VkDescriptorSetLayoutBinding &binding = bindings[b];

            if (binding.descriptorCount == 0)
            {
                continue;
            }
#ifndef CTS_USES_VULKANSC
            if (binding.descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
            {
                descriptor++;
            }
#endif
            else if (binding.descriptorType != VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT &&
#ifndef CTS_USES_VULKANSC
                     binding.descriptorType != VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT &&
#endif
                     binding.descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
            {
                for (uint32_t d = descriptor; d < descriptor + binding.descriptorCount; ++d)
                {
                    DescriptorId descriptorId(s, static_cast<uint32_t>(b), d - descriptor);
                    auto writeInfoItr = randomLayout.descriptorWrites.find(descriptorId);
                    int32_t *ptr      = (int32_t *)(bufferPtr + align * d);

                    if (writeInfoItr == randomLayout.descriptorWrites.end())
                    {
                        *ptr = static_cast<int32_t>(d);
                    }
                    else
                    {
                        *ptr                          = -1;
                        writeInfoItr->second.ptr      = ptr;
                        writeInfoItr->second.expected = d;
                    }

                    const vk::VkBufferViewCreateInfo viewCreateInfo = {
                        vk::VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,
                        nullptr,
                        (vk::VkBufferViewCreateFlags)0,
                        **buffer,                          // buffer
                        VK_FORMAT_R32_SINT,                // format
                        (vk::VkDeviceSize)align * d,       // offset
                        (vk::VkDeviceSize)sizeof(uint32_t) // range
                    };
                    vk::Move<vk::VkBufferView> bufferView = vk::createBufferView(vk, device, &viewCreateInfo);
                    bufferViews[d]                        = BufferViewHandleSp(new BufferViewHandleUp(bufferView));
                }
                descriptor += binding.descriptorCount;
            }
#ifndef CTS_USES_VULKANSC
            else if (binding.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
            {
                // subtract 16 for "ivec4 unused"
                DE_ASSERT(binding.descriptorCount >= 16);
                descriptor += binding.descriptorCount - 16;
            }
#endif
            else if (binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
            {
                // Storage image.
                for (uint32_t d = descriptor; d < descriptor + binding.descriptorCount; ++d)
                {
                    VkImage img = *storageImages[storageImgIndex];
                    DescriptorId descriptorId(s, static_cast<uint32_t>(b), d - descriptor);
                    int32_t *ptr = storageImgBufferPtr + storageImgIndex;

                    auto writeInfoItr  = randomLayout.descriptorWrites.find(descriptorId);
                    const bool isWrite = (writeInfoItr != randomLayout.descriptorWrites.end());

                    if (isWrite)
                    {
                        writeInfoItr->second.ptr      = ptr;
                        writeInfoItr->second.expected = static_cast<int32_t>(d);
                    }

                    preStorageImageBarrier.image  = img;
                    clearValue.int32[0]           = (isWrite ? -1 : static_cast<int32_t>(d));
                    postStorageImageBarrier.image = img;

                    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                          VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, nullptr, 0, nullptr,
                                          1, &preStorageImageBarrier);
                    vk.cmdClearColorImage(*cmdBuffer, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1,
                                          &clearRange);
                    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, m_data.allPipelineStages,
                                          (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &postStorageImageBarrier);

                    ++storageImgIndex;
                }
                descriptor += binding.descriptorCount;
            }
            else
            {
                // Input attachment.
                for (uint32_t d = descriptor; d < descriptor + binding.descriptorCount; ++d)
                {
                    VkImage img = *inputAttachments[attachmentIndex];

                    preInputAttachmentBarrier.image  = img;
                    clearValue.int32[0]              = static_cast<int32_t>(d);
                    postInputAttachmentBarrier.image = img;

                    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                          VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, nullptr, 0, nullptr,
                                          1, &preInputAttachmentBarrier);
                    vk.cmdClearColorImage(*cmdBuffer, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1,
                                          &clearRange);
                    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, (VkDependencyFlags)0, 0, nullptr, 0,
                                          nullptr, 1, &postInputAttachmentBarrier);

                    ++attachmentIndex;
                }
                descriptor += binding.descriptorCount;
            }
        }
    }

    // Flush modified memory.
    flushAlloc(vk, device, buffer->getAllocation());

    // Push constants are used for dynamic indexing. PushConstant[i] = i.
    const VkPushConstantRange pushConstRange = {
        m_data.allShaderStages, // VkShaderStageFlags    stageFlags
        0,                      // uint32_t                offset
        128                     // uint32_t                size
    };

    vector<vk::VkDescriptorSetLayout> descriptorSetLayoutsRaw(m_data.numDescriptorSets);
    for (size_t i = 0; i < m_data.numDescriptorSets; ++i)
    {
        descriptorSetLayoutsRaw[i] = descriptorSetLayouts[i].get();
    }

    const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,         //  VkStructureType sType;
        nullptr,                                               //  const void* pNext;
        (VkPipelineLayoutCreateFlags)0,                        //  VkPipelineLayoutCreateFlags flags;
        m_data.numDescriptorSets,                              //  uint32_t setLayoutCount;
        &descriptorSetLayoutsRaw[0],                           //  const VkDescriptorSetLayout* pSetLayouts;
        m_data.indexType == INDEX_TYPE_PUSHCONSTANT ? 1u : 0u, //  uint32_t pushConstantRangeCount;
        &pushConstRange,                                       //  const VkPushConstantRange* pPushConstantRanges;
    };

    Move<VkPipelineLayout> pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo, NULL);

    if (m_data.indexType == INDEX_TYPE_PUSHCONSTANT)
    {
        // PushConstant[i] = i
        for (uint32_t i = 0; i < (uint32_t)(128 / sizeof(uint32_t)); ++i)
        {
            vk.cmdPushConstants(*cmdBuffer, *pipelineLayout, m_data.allShaderStages, (uint32_t)(i * sizeof(uint32_t)),
                                (uint32_t)sizeof(uint32_t), &i);
        }
    }

    de::MovePtr<BufferWithMemory> copyBuffer;
    copyBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
        vk, device, allocator, makeBufferCreateInfo(DIM * DIM * sizeof(uint32_t), VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        MemoryRequirement::HostVisible));

    // Special case for the output storage image.
    const VkImageCreateInfo imageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        (VkImageCreateFlags)0u,              // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        VK_FORMAT_R32_SINT,                  // VkFormat format;
        {
            DIM,                 // uint32_t width;
            DIM,                 // uint32_t height;
            1u                   // uint32_t depth;
        },                       // VkExtent3D extent;
        1u,                      // uint32_t mipLevels;
        1u,                      // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,   // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL, // VkImageTiling tiling;
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
            VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        nullptr,                             // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED            // VkImageLayout initialLayout;
    };

    VkImageViewCreateInfo imageViewCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
        nullptr,                                  // const void* pNext;
        (VkImageViewCreateFlags)0u,               // VkImageViewCreateFlags flags;
        VK_NULL_HANDLE,                           // VkImage image;
        VK_IMAGE_VIEW_TYPE_2D,                    // VkImageViewType viewType;
        VK_FORMAT_R32_SINT,                       // VkFormat format;
        {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
         VK_COMPONENT_SWIZZLE_IDENTITY}, // VkComponentMapping  components;
        {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t baseMipLevel;
            1u,                        // uint32_t levelCount;
            0u,                        // uint32_t baseArrayLayer;
            1u                         // uint32_t layerCount;
        }                              // VkImageSubresourceRange subresourceRange;
    };

    de::MovePtr<ImageWithMemory> image;
    Move<VkImageView> imageView;

    image = de::MovePtr<ImageWithMemory>(
        new ImageWithMemory(vk, device, allocator, imageCreateInfo, MemoryRequirement::Any));
    imageViewCreateInfo.image = **image;
    imageView                 = createImageView(vk, device, &imageViewCreateInfo, NULL);

#ifndef CTS_USES_VULKANSC
    // Create ray tracing structures
    de::MovePtr<vk::BottomLevelAccelerationStructure> bottomLevelAccelerationStructure;
    de::MovePtr<vk::TopLevelAccelerationStructure> topLevelAccelerationStructure;
    VkStridedDeviceAddressRegionKHR raygenShaderBindingTableRegion   = makeStridedDeviceAddressRegionKHR(0, 0, 0);
    VkStridedDeviceAddressRegionKHR missShaderBindingTableRegion     = makeStridedDeviceAddressRegionKHR(0, 0, 0);
    VkStridedDeviceAddressRegionKHR hitShaderBindingTableRegion      = makeStridedDeviceAddressRegionKHR(0, 0, 0);
    VkStridedDeviceAddressRegionKHR callableShaderBindingTableRegion = makeStridedDeviceAddressRegionKHR(0, 0, 0);

    if (usesAccelerationStructure(m_data.stage))
    {
        AccelerationStructBufferProperties bufferProps;
        bufferProps.props.residency = ResourceResidency::TRADITIONAL;

        // Create bottom level acceleration structure
        {
            bottomLevelAccelerationStructure = makeBottomLevelAccelerationStructure();

            bottomLevelAccelerationStructure->setDefaultGeometryData(getShaderStageFlag(m_data.stage));

            bottomLevelAccelerationStructure->createAndBuild(vk, device, *cmdBuffer, allocator, bufferProps);
        }

        // Create top level acceleration structure
        {
            topLevelAccelerationStructure = makeTopLevelAccelerationStructure();

            topLevelAccelerationStructure->setInstanceCount(1);
            topLevelAccelerationStructure->addInstance(
                de::SharedPtr<BottomLevelAccelerationStructure>(bottomLevelAccelerationStructure.release()));

            topLevelAccelerationStructure->createAndBuild(vk, device, *cmdBuffer, allocator, bufferProps);
        }
    }
#endif

    descriptor      = 0;
    attachmentIndex = 0;
    storageImgIndex = 0;

    for (uint32_t s = 0; s < m_data.numDescriptorSets; ++s)
    {
        vector<VkDescriptorSetLayoutBinding> &bindings  = randomLayout.layoutBindings[s];
        vector<VkDescriptorBindingFlags> &bindingsFlags = randomLayout.layoutBindingFlags[s];
        vector<uint32_t> &arraySizes                    = randomLayout.arraySizes[s];
        vector<uint32_t> &variableDescriptorSizes       = randomLayout.variableDescriptorSizes;

        vector<VkDescriptorBufferInfo> bufferInfoVec(numDescriptors);
        vector<VkDescriptorImageInfo> imageInfoVec(numDescriptors);
        vector<VkBufferView> bufferViewVec(numDescriptors);
#ifndef CTS_USES_VULKANSC
        vector<VkWriteDescriptorSetInlineUniformBlockEXT> inlineInfoVec(numDescriptors);
        vector<VkWriteDescriptorSetAccelerationStructureKHR> accelerationInfoVec(numDescriptors);
#endif
        vector<uint32_t> descriptorNumber(numDescriptors);
        vector<VkWriteDescriptorSet> writesBeforeBindVec(0);
        vector<VkWriteDescriptorSet> writesAfterBindVec(0);
        int vecIndex   = 0;
        int numDynamic = 0;

#ifndef CTS_USES_VULKANSC
        vector<VkDescriptorUpdateTemplateEntry> imgTemplateEntriesBefore, imgTemplateEntriesAfter,
            bufTemplateEntriesBefore, bufTemplateEntriesAfter, texelBufTemplateEntriesBefore,
            texelBufTemplateEntriesAfter, inlineTemplateEntriesBefore, inlineTemplateEntriesAfter;
#endif
        for (size_t b = 0; b < bindings.size(); ++b)
        {
            VkDescriptorSetLayoutBinding &binding = bindings[b];
            uint32_t descriptorIncrement          = 1;
#ifndef CTS_USES_VULKANSC
            if (binding.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
                descriptorIncrement = 16;
#endif

            // Construct the declaration for the binding
            if (binding.descriptorCount > 0)
            {
                bool updateAfterBind = !!(bindingsFlags[b] & VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);
                for (uint32_t ai = 0; ai < de::max(1u, arraySizes[b]); ++ai, descriptor += descriptorIncrement)
                {
                    // Don't access descriptors past the end of the allocated range for
                    // variable descriptor count
                    if (b == bindings.size() - 1 &&
                        (bindingsFlags[b] & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT))
                    {
#ifndef CTS_USES_VULKANSC
                        if (binding.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
                        {
                            // Convert to bytes and add 16 for "ivec4 unused" in case of inline uniform block
                            const uint32_t uboRange = ai * 16 + 16;
                            if (uboRange >= variableDescriptorSizes[s])
                                continue;
                        }
                        else
#endif
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
                            imageInfoVec[vecIndex] =
                                makeDescriptorImageInfo(VK_NULL_HANDLE, *imageView, VK_IMAGE_LAYOUT_GENERAL);
                        }
                        else
                        {
                            imageInfoVec[vecIndex] = makeDescriptorImageInfo(
                                VK_NULL_HANDLE, storageImageViews[storageImgIndex].get(), VK_IMAGE_LAYOUT_GENERAL);
                        }
                        ++storageImgIndex;
                        break;
                    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
                        imageInfoVec[vecIndex] =
                            makeDescriptorImageInfo(VK_NULL_HANDLE, inputAttachmentViews[attachmentIndex].get(),
                                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                        ++attachmentIndex;
                        break;
#ifndef CTS_USES_VULKANSC
                    case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT:
                        // Handled below.
                        break;
                    case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
                        // Handled below.
                        break;
#endif
                    default:
                        // Other descriptor types.
                        bufferInfoVec[vecIndex] =
                            makeDescriptorBufferInfo(**buffer, descriptor * align, sizeof(uint32_t));
                        bufferViewVec[vecIndex] = **bufferViews[descriptor];
                        break;
                    }

                    descriptorNumber[descriptor] = descriptor;

                    VkWriteDescriptorSet w = {
                        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, //  VkStructureType sType;
                        nullptr,                                //  const void* pNext;
                        *descriptorSets[s],                     //  VkDescriptorSet dstSet;
                        (uint32_t)b,                            //  uint32_t dstBinding;
                        ai,                                     //  uint32_t dstArrayElement;
                        1u,                                     //  uint32_t descriptorCount;
                        binding.descriptorType,                 //  VkDescriptorType descriptorType;
                        &imageInfoVec[vecIndex],                //  const VkDescriptorImageInfo* pImageInfo;
                        &bufferInfoVec[vecIndex],               //  const VkDescriptorBufferInfo* pBufferInfo;
                        &bufferViewVec[vecIndex],               //  const VkBufferView* pTexelBufferView;
                    };

#ifndef CTS_USES_VULKANSC
                    if (binding.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
                    {
                        VkWriteDescriptorSetInlineUniformBlockEXT iuBlock = {
                            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK_EXT, // VkStructureType sType;
                            nullptr,                                                         // const void* pNext;
                            sizeof(uint32_t),                                                // uint32_t dataSize;
                            &descriptorNumber[descriptor],                                   // const void* pData;
                        };

                        inlineInfoVec[vecIndex] = iuBlock;
                        w.dstArrayElement       = ai * 16 + 16; // add 16 to skip "ivec4 unused"
                        w.pNext                 = &inlineInfoVec[vecIndex];
                        w.descriptorCount       = sizeof(uint32_t);
                    }

                    if (binding.descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
                    {
                        const TopLevelAccelerationStructure *topLevelAccelerationStructurePtr =
                            topLevelAccelerationStructure.get();
                        VkWriteDescriptorSetAccelerationStructureKHR accelerationStructureWriteDescriptorSet = {
                            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR, //  VkStructureType sType;
                            nullptr,                                                           //  const void* pNext;
                            w.descriptorCount, //  uint32_t accelerationStructureCount;
                            topLevelAccelerationStructurePtr
                                ->getPtr(), //  const VkAccelerationStructureKHR* pAccelerationStructures;
                        };

                        accelerationInfoVec[vecIndex] = accelerationStructureWriteDescriptorSet;
                        w.dstArrayElement             = 0;
                        w.pNext                       = &accelerationInfoVec[vecIndex];
                    }

                    VkDescriptorUpdateTemplateEntry templateEntry = {
                        (uint32_t)b,            // uint32_t dstBinding;
                        ai,                     // uint32_t dstArrayElement;
                        1u,                     // uint32_t descriptorCount;
                        binding.descriptorType, // VkDescriptorType descriptorType;
                        0,                      // size_t offset;
                        0,                      // size_t stride;
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
                        (updateAfterBind ? texelBufTemplateEntriesAfter : texelBufTemplateEntriesBefore)
                            .push_back(templateEntry);
                        break;
                    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
                    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
                        templateEntry.offset = vecIndex * sizeof(VkDescriptorBufferInfo);
                        (updateAfterBind ? bufTemplateEntriesAfter : bufTemplateEntriesBefore).push_back(templateEntry);
                        break;
                    case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT:
                        templateEntry.offset          = descriptor * sizeof(uint32_t);
                        templateEntry.dstArrayElement = ai * 16 + 16; // add 16 to skip "ivec4 dummy"
                        templateEntry.descriptorCount = sizeof(uint32_t);
                        (updateAfterBind ? inlineTemplateEntriesAfter : inlineTemplateEntriesBefore)
                            .push_back(templateEntry);
                        break;
                    case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
                        DE_ASSERT(!updateAfterBind);
                        DE_ASSERT(usesAccelerationStructure(m_data.stage));
                        break;
                    }
#endif

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
        vector<uint32_t> zeros(de::max(1, numDynamic));
        deMemset(&zeros[0], 0, numDynamic * sizeof(uint32_t));

#ifndef CTS_USES_VULKANSC
        // Randomly select between vkUpdateDescriptorSets and vkUpdateDescriptorSetWithTemplate
        if (randRange(&rnd, 1, 2) == 1 && m_context.contextSupports(vk::ApiVersion(0, 1, 1, 0)) &&
            !usesAccelerationStructure(m_data.stage))
        {
            DE_ASSERT(!usesAccelerationStructure(m_data.stage));

            VkDescriptorUpdateTemplateCreateInfo templateCreateInfo = {
                VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO, // VkStructureType sType;
                NULL,                                                     // void* pNext;
                0,                                                 // VkDescriptorUpdateTemplateCreateFlags flags;
                0,                                                 // uint32_t descriptorUpdateEntryCount;
                nullptr,                                           // uint32_t descriptorUpdateEntryCount;
                VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET, // VkDescriptorUpdateTemplateType templateType;
                descriptorSetLayouts[s].get(),                     // VkDescriptorSetLayout descriptorSetLayout;
                bindPoint,                                         // VkPipelineBindPoint pipelineBindPoint;
                VK_NULL_HANDLE,                                    // VkPipelineLayout pipelineLayout;
                0,                                                 // uint32_t set;
            };

            void *templateVectorData[] = {
                imageInfoVec.data(),
                bufferInfoVec.data(),
                bufferViewVec.data(),
                descriptorNumber.data(),
            };

            vector<VkDescriptorUpdateTemplateEntry> *templateVectorsBefore[] = {
                &imgTemplateEntriesBefore,
                &bufTemplateEntriesBefore,
                &texelBufTemplateEntriesBefore,
                &inlineTemplateEntriesBefore,
            };

            vector<VkDescriptorUpdateTemplateEntry> *templateVectorsAfter[] = {
                &imgTemplateEntriesAfter,
                &bufTemplateEntriesAfter,
                &texelBufTemplateEntriesAfter,
                &inlineTemplateEntriesAfter,
            };

            for (size_t i = 0; i < DE_LENGTH_OF_ARRAY(templateVectorsBefore); ++i)
            {
                if (templateVectorsBefore[i]->size())
                {
                    templateCreateInfo.descriptorUpdateEntryCount = (uint32_t)templateVectorsBefore[i]->size();
                    templateCreateInfo.pDescriptorUpdateEntries   = templateVectorsBefore[i]->data();
                    Move<VkDescriptorUpdateTemplate> descriptorUpdateTemplate =
                        createDescriptorUpdateTemplate(vk, device, &templateCreateInfo, NULL);
                    vk.updateDescriptorSetWithTemplate(device, descriptorSets[s].get(), *descriptorUpdateTemplate,
                                                       templateVectorData[i]);
                }
            }

            vk.cmdBindDescriptorSets(*cmdBuffer, bindPoint, *pipelineLayout, s, 1, &descriptorSets[s].get(), numDynamic,
                                     &zeros[0]);

            for (size_t i = 0; i < DE_LENGTH_OF_ARRAY(templateVectorsAfter); ++i)
            {
                if (templateVectorsAfter[i]->size())
                {
                    templateCreateInfo.descriptorUpdateEntryCount = (uint32_t)templateVectorsAfter[i]->size();
                    templateCreateInfo.pDescriptorUpdateEntries   = templateVectorsAfter[i]->data();
                    Move<VkDescriptorUpdateTemplate> descriptorUpdateTemplate =
                        createDescriptorUpdateTemplate(vk, device, &templateCreateInfo, NULL);
                    vk.updateDescriptorSetWithTemplate(device, descriptorSets[s].get(), *descriptorUpdateTemplate,
                                                       templateVectorData[i]);
                }
            }
        }
        else
#endif
        {
            if (writesBeforeBindVec.size())
            {
                vk.updateDescriptorSets(device, (uint32_t)writesBeforeBindVec.size(), &writesBeforeBindVec[0], 0, NULL);
            }

            vk.cmdBindDescriptorSets(*cmdBuffer, bindPoint, *pipelineLayout, s, 1, &descriptorSets[s].get(), numDynamic,
                                     &zeros[0]);

            if (writesAfterBindVec.size())
            {
                vk.updateDescriptorSets(device, (uint32_t)writesAfterBindVec.size(), &writesAfterBindVec[0], 0, NULL);
            }
        }
    }

    Move<VkPipeline> pipeline;
    Move<VkRenderPass> renderPass;
    Move<VkFramebuffer> framebuffer;

#ifndef CTS_USES_VULKANSC
    de::MovePtr<BufferWithMemory> sbtBuffer;
    de::MovePtr<BufferWithMemory> raygenShaderBindingTable;
    de::MovePtr<BufferWithMemory> missShaderBindingTable;
    de::MovePtr<BufferWithMemory> hitShaderBindingTable;
    de::MovePtr<BufferWithMemory> callableShaderBindingTable;
    de::MovePtr<RayTracingPipeline> rayTracingPipeline;
#endif

    // Disable interval watchdog timer for long shader compilations that can
    // happen when the number of descriptor sets gets to 32 and above.
    if (m_data.numDescriptorSets >= 32)
    {
        m_context.getTestContext().touchWatchdogAndDisableIntervalTimeLimit();
    }

    if (m_data.stage == STAGE_COMPUTE)
    {
        const Unique<VkShaderModule> shader(
            createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0));

        const VkPipelineShaderStageCreateInfo shaderCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            nullptr,
            (VkPipelineShaderStageCreateFlags)0,
            VK_SHADER_STAGE_COMPUTE_BIT, // stage
            *shader,                     // shader
            "main",
            nullptr, // pSpecializationInfo
        };

        const VkComputePipelineCreateInfo pipelineCreateInfo = {
            VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            nullptr,
            0u,               // flags
            shaderCreateInfo, // cs
            *pipelineLayout,  // layout
            VK_NULL_HANDLE,   // basePipelineHandle
            0u,               // basePipelineIndex
        };
        pipeline = createComputePipeline(vk, device, VK_NULL_HANDLE, &pipelineCreateInfo, NULL);
    }
#ifndef CTS_USES_VULKANSC
    else if (m_data.stage == STAGE_RAYGEN_NV)
    {
        const Unique<VkShaderModule> shader(
            createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0));

        const VkPipelineShaderStageCreateInfo shaderCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, //  VkStructureType sType;
            nullptr,                                             //  const void* pNext;
            (VkPipelineShaderStageCreateFlags)0,                 //  VkPipelineShaderStageCreateFlags flags;
            VK_SHADER_STAGE_RAYGEN_BIT_NV,                       //  VkShaderStageFlagBits stage;
            *shader,                                             //  VkShaderModule module;
            "main",                                              //  const char* pName;
            nullptr,                                             //  const VkSpecializationInfo* pSpecializationInfo;
        };

        VkRayTracingShaderGroupCreateInfoNV group = {
            VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV, //  VkStructureType sType;
            nullptr,                                                   //  const void* pNext;
            VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV,               //  VkRayTracingShaderGroupTypeNV type;
            0,                                                         //  uint32_t generalShader;
            VK_SHADER_UNUSED_KHR,                                      //  uint32_t closestHitShader;
            VK_SHADER_UNUSED_KHR,                                      //  uint32_t anyHitShader;
            VK_SHADER_UNUSED_KHR,                                      //  uint32_t intersectionShader;
        };

        VkRayTracingPipelineCreateInfoNV pipelineCreateInfo = {
            VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_NV, //  VkStructureType sType;
            nullptr,                                               //  const void* pNext;
            0,                                                     //  VkPipelineCreateFlags flags;
            1,                                                     //  uint32_t stageCount;
            &shaderCreateInfo,                                     //  const VkPipelineShaderStageCreateInfo* pStages;
            1,                                                     //  uint32_t groupCount;
            &group,          //  const VkRayTracingShaderGroupCreateInfoNV* pGroups;
            0,               //  uint32_t maxRecursionDepth;
            *pipelineLayout, //  VkPipelineLayout layout;
            VK_NULL_HANDLE,  //  VkPipeline basePipelineHandle;
            0u,              //  int32_t basePipelineIndex;
        };

        pipeline = createRayTracingPipelineNV(vk, device, VK_NULL_HANDLE, &pipelineCreateInfo, NULL);

        const auto allocSize = de::roundUp(static_cast<VkDeviceSize>(shaderGroupHandleSize),
                                           properties.properties.limits.nonCoherentAtomSize);

        sbtBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
            vk, device, allocator,
            makeBufferCreateInfo(allocSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_RAY_TRACING_BIT_NV),
            MemoryRequirement::HostVisible));

        const auto &alloc = sbtBuffer->getAllocation();
        const auto ptr    = reinterpret_cast<uint32_t *>(alloc.getHostPtr());

        invalidateAlloc(vk, device, alloc);
        vk.getRayTracingShaderGroupHandlesKHR(device, *pipeline, 0, 1, static_cast<uintptr_t>(allocSize), ptr);
    }
    else if (m_data.stage == STAGE_RAYGEN)
    {
        rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();

        rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                                      createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0),
                                      0);

        pipeline = rayTracingPipeline->createPipeline(vk, device, *pipelineLayout);

        raygenShaderBindingTable = rayTracingPipeline->createShaderBindingTable(
            vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
        raygenShaderBindingTableRegion =
            makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vk, device, raygenShaderBindingTable->get(), 0),
                                              shaderGroupHandleSize, shaderGroupHandleSize);
    }
    else if (m_data.stage == STAGE_INTERSECT)
    {
        rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();

        rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                                      createShaderModule(vk, device, m_context.getBinaryCollection().get("rgen"), 0),
                                      0);
        rayTracingPipeline->addShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
                                      createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0),
                                      1);

        pipeline = rayTracingPipeline->createPipeline(vk, device, *pipelineLayout);

        raygenShaderBindingTable = rayTracingPipeline->createShaderBindingTable(
            vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
        raygenShaderBindingTableRegion =
            makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vk, device, raygenShaderBindingTable->get(), 0),
                                              shaderGroupHandleSize, shaderGroupHandleSize);

        hitShaderBindingTable = rayTracingPipeline->createShaderBindingTable(
            vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1);
        hitShaderBindingTableRegion =
            makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vk, device, hitShaderBindingTable->get(), 0),
                                              shaderGroupHandleSize, shaderGroupHandleSize);
    }
    else if (m_data.stage == STAGE_ANY_HIT)
    {
        rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();

        rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                                      createShaderModule(vk, device, m_context.getBinaryCollection().get("rgen"), 0),
                                      0);
        rayTracingPipeline->addShader(VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
                                      createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0),
                                      1);

        pipeline = rayTracingPipeline->createPipeline(vk, device, *pipelineLayout);

        raygenShaderBindingTable = rayTracingPipeline->createShaderBindingTable(
            vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
        raygenShaderBindingTableRegion =
            makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vk, device, raygenShaderBindingTable->get(), 0),
                                              shaderGroupHandleSize, shaderGroupHandleSize);

        hitShaderBindingTable = rayTracingPipeline->createShaderBindingTable(
            vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1);
        hitShaderBindingTableRegion =
            makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vk, device, hitShaderBindingTable->get(), 0),
                                              shaderGroupHandleSize, shaderGroupHandleSize);
    }
    else if (m_data.stage == STAGE_CLOSEST_HIT)
    {
        rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();

        rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                                      createShaderModule(vk, device, m_context.getBinaryCollection().get("rgen"), 0),
                                      0);
        rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
                                      createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0),
                                      1);

        pipeline = rayTracingPipeline->createPipeline(vk, device, *pipelineLayout);

        raygenShaderBindingTable = rayTracingPipeline->createShaderBindingTable(
            vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
        raygenShaderBindingTableRegion =
            makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vk, device, raygenShaderBindingTable->get(), 0),
                                              shaderGroupHandleSize, shaderGroupHandleSize);

        hitShaderBindingTable = rayTracingPipeline->createShaderBindingTable(
            vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1);
        hitShaderBindingTableRegion =
            makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vk, device, hitShaderBindingTable->get(), 0),
                                              shaderGroupHandleSize, shaderGroupHandleSize);
    }
    else if (m_data.stage == STAGE_MISS)
    {
        rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();

        rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                                      createShaderModule(vk, device, m_context.getBinaryCollection().get("rgen"), 0),
                                      0);
        rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR,
                                      createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0),
                                      1);

        pipeline = rayTracingPipeline->createPipeline(vk, device, *pipelineLayout);

        raygenShaderBindingTable = rayTracingPipeline->createShaderBindingTable(
            vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
        raygenShaderBindingTableRegion =
            makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vk, device, raygenShaderBindingTable->get(), 0),
                                              shaderGroupHandleSize, shaderGroupHandleSize);

        missShaderBindingTable = rayTracingPipeline->createShaderBindingTable(
            vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1);
        missShaderBindingTableRegion =
            makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vk, device, missShaderBindingTable->get(), 0),
                                              shaderGroupHandleSize, shaderGroupHandleSize);
    }
    else if (m_data.stage == STAGE_CALLABLE)
    {
        rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();

        rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                                      createShaderModule(vk, device, m_context.getBinaryCollection().get("rgen"), 0),
                                      0);
        rayTracingPipeline->addShader(VK_SHADER_STAGE_CALLABLE_BIT_KHR,
                                      createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0),
                                      1);

        pipeline = rayTracingPipeline->createPipeline(vk, device, *pipelineLayout);

        raygenShaderBindingTable = rayTracingPipeline->createShaderBindingTable(
            vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
        raygenShaderBindingTableRegion =
            makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vk, device, raygenShaderBindingTable->get(), 0),
                                              shaderGroupHandleSize, shaderGroupHandleSize);

        callableShaderBindingTable = rayTracingPipeline->createShaderBindingTable(
            vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1);
        callableShaderBindingTableRegion =
            makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vk, device, callableShaderBindingTable->get(), 0),
                                              shaderGroupHandleSize, shaderGroupHandleSize);
    }
#endif
    else
    {
        const VkAttachmentDescription attachmentDescription = {
            // Input attachment
            (VkAttachmentDescriptionFlags)0,          // VkAttachmentDescriptionFlags    flags
            VK_FORMAT_R32_SINT,                       // VkFormat                        format
            VK_SAMPLE_COUNT_1_BIT,                    // VkSampleCountFlagBits        samples
            VK_ATTACHMENT_LOAD_OP_LOAD,               // VkAttachmentLoadOp            loadOp
            VK_ATTACHMENT_STORE_OP_STORE,             // VkAttachmentStoreOp            storeOp
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,          // VkAttachmentLoadOp            stencilLoadOp
            VK_ATTACHMENT_STORE_OP_DONT_CARE,         // VkAttachmentStoreOp            stencilStoreOp
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, // VkImageLayout                initialLayout
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL  // VkImageLayout                finalLayout
        };

        vector<VkAttachmentDescription> attachmentDescriptions(inputAttachments.size(), attachmentDescription);
        vector<VkAttachmentReference> attachmentReferences;

        attachmentReferences.reserve(inputAttachments.size());
        VkAttachmentReference attachmentReference = {0u, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        for (size_t i = 0; i < inputAttachments.size(); ++i)
        {
            attachmentReference.attachment = static_cast<uint32_t>(i);
            attachmentReferences.push_back(attachmentReference);
        }

        const VkSubpassDescription subpassDesc = {
            (VkSubpassDescriptionFlags)0,                       // VkSubpassDescriptionFlags    flags
            VK_PIPELINE_BIND_POINT_GRAPHICS,                    // VkPipelineBindPoint            pipelineBindPoint
            static_cast<uint32_t>(attachmentReferences.size()), // uint32_t                        inputAttachmentCount
            de::dataOrNull(attachmentReferences),               // const VkAttachmentReference*    pInputAttachments
            0u,                                                 // uint32_t                        colorAttachmentCount
            nullptr,                                            // const VkAttachmentReference*    pColorAttachments
            nullptr,                                            // const VkAttachmentReference*    pResolveAttachments
            nullptr, // const VkAttachmentReference*    pDepthStencilAttachment
            0u,      // uint32_t                        preserveAttachmentCount
            nullptr  // const uint32_t*                pPreserveAttachments
        };

        const VkSubpassDependency subpassDependency = {
            VK_SUBPASS_EXTERNAL,            // uint32_t                srcSubpass
            0,                              // uint32_t                dstSubpass
            VK_PIPELINE_STAGE_TRANSFER_BIT, // VkPipelineStageFlags    srcStageMask
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // dstStageMask
            VK_ACCESS_TRANSFER_WRITE_BIT, // VkAccessFlags        srcAccessMask
            VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, //    dstAccessMask
            VK_DEPENDENCY_BY_REGION_BIT               // VkDependencyFlags    dependencyFlags
        };

        const VkRenderPassCreateInfo renderPassParams = {
            VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,            // VkStructureTypei                    sType
            nullptr,                                              // const void*                        pNext
            (VkRenderPassCreateFlags)0,                           // VkRenderPassCreateFlags            flags
            static_cast<uint32_t>(attachmentDescriptions.size()), // uint32_t                            attachmentCount
            de::dataOrNull(attachmentDescriptions),               // const VkAttachmentDescription*    pAttachments
            1u,                                                   // uint32_t                            subpassCount
            &subpassDesc,                                         // const VkSubpassDescription*        pSubpasses
            1u,                                                   // uint32_t                            dependencyCount
            &subpassDependency                                    // const VkSubpassDependency*        pDependencies
        };

        renderPass = createRenderPass(vk, device, &renderPassParams);

        vector<VkImageView> rawInputAttachmentViews;
        rawInputAttachmentViews.reserve(inputAttachmentViews.size());
        transform(begin(inputAttachmentViews), end(inputAttachmentViews), back_inserter(rawInputAttachmentViews),
                  [](const Move<VkImageView> &ptr) { return ptr.get(); });

        const vk::VkFramebufferCreateInfo framebufferParams = {
            vk::VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // sType
            nullptr,                                       // pNext
            (vk::VkFramebufferCreateFlags)0,
            *renderPass,                                           // renderPass
            static_cast<uint32_t>(rawInputAttachmentViews.size()), // attachmentCount
            de::dataOrNull(rawInputAttachmentViews),               // pAttachments
            DIM,                                                   // width
            DIM,                                                   // height
            1u,                                                    // layers
        };

        framebuffer = createFramebuffer(vk, device, &framebufferParams);

        // Note: vertex input state and input assembly state will not be used for mesh pipelines.

        const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                   // const void* pNext;
            (VkPipelineVertexInputStateCreateFlags)0,                  // VkPipelineVertexInputStateCreateFlags flags;
            0u,                                                        // uint32_t vertexBindingDescriptionCount;
            nullptr, // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
            0u,      // uint32_t vertexAttributeDescriptionCount;
            nullptr  // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
        };

        const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                     // const void* pNext;
            (VkPipelineInputAssemblyStateCreateFlags)0, // VkPipelineInputAssemblyStateCreateFlags flags;
            (m_data.stage == STAGE_VERTEX) ? VK_PRIMITIVE_TOPOLOGY_POINT_LIST :
                                             VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // VkPrimitiveTopology topology;
            VK_FALSE                                                               // VkBool32 primitiveRestartEnable;
        };

        const VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                    // const void* pNext;
            (VkPipelineRasterizationStateCreateFlags)0,          // VkPipelineRasterizationStateCreateFlags flags;
            VK_FALSE,                                            // VkBool32 depthClampEnable;
            (m_data.stage == STAGE_VERTEX) ? VK_TRUE : VK_FALSE, // VkBool32 rasterizerDiscardEnable;
            VK_POLYGON_MODE_FILL,                                // VkPolygonMode polygonMode;
            VK_CULL_MODE_NONE,                                   // VkCullModeFlags cullMode;
            VK_FRONT_FACE_CLOCKWISE,                             // VkFrontFace frontFace;
            VK_FALSE,                                            // VkBool32 depthBiasEnable;
            0.0f,                                                // float depthBiasConstantFactor;
            0.0f,                                                // float depthBiasClamp;
            0.0f,                                                // float depthBiasSlopeFactor;
            1.0f                                                 // float lineWidth;
        };

        const VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType                            sType
            nullptr,               // const void*                                pNext
            0u,                    // VkPipelineMultisampleStateCreateFlags    flags
            VK_SAMPLE_COUNT_1_BIT, // VkSampleCountFlagBits                    rasterizationSamples
            VK_FALSE,              // VkBool32                                    sampleShadingEnable
            1.0f,                  // float                                    minSampleShading
            nullptr,               // const VkSampleMask*                        pSampleMask
            VK_FALSE,              // VkBool32                                    alphaToCoverageEnable
            VK_FALSE               // VkBool32                                    alphaToOneEnable
        };

        VkViewport viewport = makeViewport(DIM, DIM);
        VkRect2D scissor    = makeRect2D(DIM, DIM);

        const VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, // VkStructureType                            sType
            nullptr,                                               // const void*                                pNext
            (VkPipelineViewportStateCreateFlags)0,                 // VkPipelineViewportStateCreateFlags        flags
            1u,        // uint32_t                                    viewportCount
            &viewport, // const VkViewport*                        pViewports
            1u,        // uint32_t                                    scissorCount
            &scissor   // const VkRect2D*                            pScissors
        };

        Move<VkShaderModule> fs;
        Move<VkShaderModule> vs;
#ifndef CTS_USES_VULKANSC
        Move<VkShaderModule> ms;
        Move<VkShaderModule> ts;
#endif // CTS_USES_VULKANSC

        const auto &binaries = m_context.getBinaryCollection();

        std::vector<VkPipelineShaderStageCreateInfo> stageCreateInfos;

        if (m_data.stage == STAGE_VERTEX)
        {
            vs = createShaderModule(vk, device, binaries.get("test"));
            appendShaderStageCreateInfo(stageCreateInfos, vs.get(), VK_SHADER_STAGE_VERTEX_BIT);
        }
        else if (m_data.stage == STAGE_FRAGMENT)
        {
            vs = createShaderModule(vk, device, binaries.get("vert"));
            fs = createShaderModule(vk, device, binaries.get("test"));
            appendShaderStageCreateInfo(stageCreateInfos, vs.get(), VK_SHADER_STAGE_VERTEX_BIT);
            appendShaderStageCreateInfo(stageCreateInfos, fs.get(), VK_SHADER_STAGE_FRAGMENT_BIT);
        }
#ifndef CTS_USES_VULKANSC
        else if (m_data.stage == STAGE_TASK)
        {
            ts = createShaderModule(vk, device, binaries.get("test"));
            ms = createShaderModule(vk, device, binaries.get("mesh"));
            appendShaderStageCreateInfo(stageCreateInfos, ts.get(), vk::VK_SHADER_STAGE_TASK_BIT_EXT);
            appendShaderStageCreateInfo(stageCreateInfos, ms.get(), VK_SHADER_STAGE_MESH_BIT_EXT);
        }
        else if (m_data.stage == STAGE_MESH)
        {
            ms = createShaderModule(vk, device, binaries.get("test"));
            appendShaderStageCreateInfo(stageCreateInfos, ms.get(), VK_SHADER_STAGE_MESH_BIT_EXT);
        }
#endif // CTS_USES_VULKANSC

        const VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {
            VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                         // const void* pNext;
            (VkPipelineCreateFlags)0,                        // VkPipelineCreateFlags flags;
            static_cast<uint32_t>(stageCreateInfos.size()),  // uint32_t stageCount;
            de::dataOrNull(stageCreateInfos),                // const VkPipelineShaderStageCreateInfo* pStages;
            &vertexInputStateCreateInfo,   // const VkPipelineVertexInputStateCreateInfo* pVertexInputState;
            &inputAssemblyStateCreateInfo, // const VkPipelineInputAssemblyStateCreateInfo* pInputAssemblyState;
            nullptr,                       // const VkPipelineTessellationStateCreateInfo* pTessellationState;
            &viewportStateCreateInfo,      // const VkPipelineViewportStateCreateInfo* pViewportState;
            &rasterizationStateCreateInfo, // const VkPipelineRasterizationStateCreateInfo* pRasterizationState;
            &multisampleStateCreateInfo,   // const VkPipelineMultisampleStateCreateInfo* pMultisampleState;
            nullptr,                       // const VkPipelineDepthStencilStateCreateInfo* pDepthStencilState;
            nullptr,                       // const VkPipelineColorBlendStateCreateInfo* pColorBlendState;
            nullptr,                       // const VkPipelineDynamicStateCreateInfo* pDynamicState;
            pipelineLayout.get(),          // VkPipelineLayout layout;
            renderPass.get(),              // VkRenderPass renderPass;
            0u,                            // uint32_t subpass;
            VK_NULL_HANDLE,                // VkPipeline basePipelineHandle;
            0                              // int basePipelineIndex;
        };

        pipeline = createGraphicsPipeline(vk, device, VK_NULL_HANDLE, &graphicsPipelineCreateInfo);
    }

    const VkImageMemoryBarrier imageBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType        sType
                                               nullptr,                                // const void*            pNext
                                               0u,                           // VkAccessFlags        srcAccessMask
                                               VK_ACCESS_TRANSFER_WRITE_BIT, // VkAccessFlags        dstAccessMask
                                               VK_IMAGE_LAYOUT_UNDEFINED,    // VkImageLayout        oldLayout
                                               VK_IMAGE_LAYOUT_GENERAL,      // VkImageLayout        newLayout
                                               VK_QUEUE_FAMILY_IGNORED, // uint32_t                srcQueueFamilyIndex
                                               VK_QUEUE_FAMILY_IGNORED, // uint32_t                dstQueueFamilyIndex
                                               **image,                 // VkImage                image
                                               {
                                                   VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags    aspectMask
                                                   0u,                        // uint32_t                baseMipLevel
                                                   1u,                        // uint32_t                mipLevels,
                                                   0u,                        // uint32_t                baseArray
                                                   1u,                        // uint32_t                arraySize
                                               }};

    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &imageBarrier);

    vk.cmdBindPipeline(*cmdBuffer, bindPoint, *pipeline);

    VkImageSubresourceRange range = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    VkClearValue clearColor       = makeClearValueColorU32(0, 0, 0, 0);

    VkMemoryBarrier memBarrier = {
        VK_STRUCTURE_TYPE_MEMORY_BARRIER, // sType
        nullptr,                          // pNext
        0u,                               // srcAccessMask
        0u,                               // dstAccessMask
    };

    vk.cmdClearColorImage(*cmdBuffer, **image, VK_IMAGE_LAYOUT_GENERAL, &clearColor.color, 1, &range);

    memBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    memBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, m_data.allPipelineStages, 0, 1, &memBarrier, 0,
                          nullptr, 0, nullptr);

    if (m_data.stage == STAGE_COMPUTE)
    {
        vk.cmdDispatch(*cmdBuffer, DIM, DIM, 1);
    }
#ifndef CTS_USES_VULKANSC
    else if (m_data.stage == STAGE_RAYGEN_NV)
    {
        vk.cmdTraceRaysNV(*cmdBuffer, **sbtBuffer, 0, VK_NULL_HANDLE, 0, 0, VK_NULL_HANDLE, 0, 0, VK_NULL_HANDLE, 0, 0,
                          DIM, DIM, 1);
    }
    else if (isRayTracingStageKHR(m_data.stage))
    {
        cmdTraceRays(vk, *cmdBuffer, &raygenShaderBindingTableRegion, &missShaderBindingTableRegion,
                     &hitShaderBindingTableRegion, &callableShaderBindingTableRegion, DIM, DIM, 1);
    }
#endif
    else
    {
        beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(DIM, DIM), 0, nullptr,
                        VK_SUBPASS_CONTENTS_INLINE);
        // Draw a point cloud for vertex shader testing, and a single quad for fragment shader testing
        if (m_data.stage == STAGE_VERTEX)
        {
            vk.cmdDraw(*cmdBuffer, DIM * DIM, 1u, 0u, 0u);
        }
        else if (m_data.stage == STAGE_FRAGMENT)
        {
            vk.cmdDraw(*cmdBuffer, 4u, 1u, 0u, 0u);
        }
#ifndef CTS_USES_VULKANSC
        else if (isMeshStage(m_data.stage))
        {
            vk.cmdDrawMeshTasksEXT(*cmdBuffer, DIM, DIM, 1u);
        }
#endif // CTS_USES_VULKANSC
        endRenderPass(vk, *cmdBuffer);
    }

    memBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    memBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    vk.cmdPipelineBarrier(*cmdBuffer, m_data.allPipelineStages, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &memBarrier, 0,
                          nullptr, 0, nullptr);

    const VkBufferImageCopy copyRegion = makeBufferImageCopy(
        makeExtent3D(DIM, DIM, 1u), makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u));
    vk.cmdCopyImageToBuffer(*cmdBuffer, **image, VK_IMAGE_LAYOUT_GENERAL, **copyBuffer, 1u, &copyRegion);

    const VkBufferMemoryBarrier copyBufferBarrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType sType;
        nullptr,                                 // const void* pNext;
        VK_ACCESS_TRANSFER_WRITE_BIT,            // VkAccessFlags srcAccessMask;
        VK_ACCESS_HOST_READ_BIT,                 // VkAccessFlags dstAccessMask;
        VK_QUEUE_FAMILY_IGNORED,                 // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                 // uint32_t dstQueueFamilyIndex;
        **copyBuffer,                            // VkBuffer buffer;
        0u,                                      // VkDeviceSize offset;
        VK_WHOLE_SIZE,                           // VkDeviceSize size;
    };

    // Add a barrier to read the copy buffer after copying.
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0u, nullptr, 1u,
                          &copyBufferBarrier, 0u, nullptr);

    // Copy all storage images to the storage image buffer.
    VkBufferImageCopy storageImgCopyRegion = {
        0u,                                                                // VkDeviceSize bufferOffset;
        0u,                                                                // uint32_t bufferRowLength;
        0u,                                                                // uint32_t bufferImageHeight;
        makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u), // VkImageSubresourceLayers imageSubresource;
        makeOffset3D(0, 0, 0),                                             // VkOffset3D imageOffset;
        makeExtent3D(1u, 1u, 1u),                                          // VkExtent3D imageExtent;
    };

    for (uint32_t i = 0; i < storageImageCount; ++i)
    {
        storageImgCopyRegion.bufferOffset = sizeof(int32_t) * i;
        vk.cmdCopyImageToBuffer(*cmdBuffer, storageImages[i].get(), VK_IMAGE_LAYOUT_GENERAL, **storageImgBuffer, 1u,
                                &storageImgCopyRegion);
    }

    const VkBufferMemoryBarrier storageImgBufferBarrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType sType;
        nullptr,                                 // const void* pNext;
        VK_ACCESS_TRANSFER_WRITE_BIT,            // VkAccessFlags srcAccessMask;
        VK_ACCESS_HOST_READ_BIT,                 // VkAccessFlags dstAccessMask;
        VK_QUEUE_FAMILY_IGNORED,                 // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                 // uint32_t dstQueueFamilyIndex;
        **storageImgBuffer,                      // VkBuffer buffer;
        0u,                                      // VkDeviceSize offset;
        VK_WHOLE_SIZE,                           // VkDeviceSize size;
    };

    // Add a barrier to read the storage image buffer after copying.
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0u, nullptr, 1u,
                          &storageImgBufferBarrier, 0u, nullptr);

    const VkBufferMemoryBarrier descriptorBufferBarrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,                  // VkStructureType sType;
        nullptr,                                                  // const void* pNext;
        (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT), // VkAccessFlags srcAccessMask;
        VK_ACCESS_HOST_READ_BIT,                                  // VkAccessFlags dstAccessMask;
        VK_QUEUE_FAMILY_IGNORED,                                  // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                                  // uint32_t dstQueueFamilyIndex;
        **buffer,                                                 // VkBuffer buffer;
        0u,                                                       // VkDeviceSize offset;
        VK_WHOLE_SIZE,                                            // VkDeviceSize size;
    };

    // Add a barrier to read stored data from shader writes in descriptor memory for other types of descriptors.
    vk.cmdPipelineBarrier(*cmdBuffer, m_data.allPipelineStages, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, nullptr, 1u,
                          &descriptorBufferBarrier, 0u, nullptr);

    endCommandBuffer(vk, *cmdBuffer);

    submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

    // Re-enable watchdog interval timer here to favor virtualized vulkan
    // implementation that asynchronously creates the pipeline on the host.
    if (m_data.numDescriptorSets >= 32)
    {
        m_context.getTestContext().touchWatchdogAndEnableIntervalTimeLimit();
    }

    // Verify output image.
    uint32_t *ptr = (uint32_t *)copyBuffer->getAllocation().getHostPtr();
    invalidateAlloc(vk, device, copyBuffer->getAllocation());

    uint32_t failures = 0;
    auto &log         = m_context.getTestContext().getLog();

    for (uint32_t i = 0; i < DIM * DIM; ++i)
    {
        if (ptr[i] != 1)
        {
            failures++;
            log << tcu::TestLog::Message << "Failure in copy buffer, ptr[" << i << "] = " << ptr[i]
                << tcu::TestLog::EndMessage;
        }
    }

    // Verify descriptors with writes.
    invalidateMappedMemoryRange(vk, device, buffer->getAllocation().getMemory(), buffer->getAllocation().getOffset(),
                                VK_WHOLE_SIZE);
    invalidateMappedMemoryRange(vk, device, storageImgBuffer->getAllocation().getMemory(),
                                storageImgBuffer->getAllocation().getOffset(), VK_WHOLE_SIZE);

    for (const auto &descIdWriteInfo : randomLayout.descriptorWrites)
    {
        const auto &writeInfo = descIdWriteInfo.second;
        if (writeInfo.writeGenerated && *writeInfo.ptr != writeInfo.expected)
        {
            failures++;
            log << tcu::TestLog::Message << "Failure in write operation; expected " << writeInfo.expected
                << " and found " << *writeInfo.ptr << tcu::TestLog::EndMessage;
        }
    }

    if (failures == 0)
        return tcu::TestStatus::pass("Pass");
    else
        return tcu::TestStatus::fail("failures=" + de::toString(failures));
}

} // namespace

tcu::TestCaseGroup *createDescriptorSetRandomTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "descriptorset_random"));

    uint32_t seed = 0;

    typedef struct
    {
        uint32_t count;
        const char *name;
    } TestGroupCase;

    TestGroupCase setsCases[] = {
        // 4 descriptor sets
        {4, "sets4"},
        // 8 descriptor sets
        {8, "sets8"},
        // 16 descriptor sets
        {16, "sets16"},
        // 32 descriptor sets
        {32, "sets32"},
    };

    TestGroupCase indexCases[] = {
        // all descriptor declarations are not arrays
        {INDEX_TYPE_NONE, "noarray"},
        // constant indexing of descriptor arrays
        {INDEX_TYPE_CONSTANT, "constant"},
        // indexing descriptor arrays with push constants
        {INDEX_TYPE_PUSHCONSTANT, "unifindexed"},
        // dynamically uniform indexing descriptor arrays
        {INDEX_TYPE_DEPENDENT, "dynindexed"},
        // runtime-size declarations of descriptor arrays
        {INDEX_TYPE_RUNTIME_SIZE, "runtimesize"},
    };

    TestGroupCase uboCases[] = {
        // no ubos
        {0, "noubo"},
        // spec minmax ubo limit
        {12, "ubolimitlow"},
        // high ubo limit
        {4096, "ubolimithigh"},
    };

    TestGroupCase sboCases[] = {
        // no ssbos
        {0, "nosbo"},
        // spec minmax ssbo limit
        {4, "sbolimitlow"},
        // high ssbo limit
        {4096, "sbolimithigh"},
    };

    TestGroupCase iaCases[] = {
        // no input attachments
        {0, "noia"},
        // spec minmax input attachment limit
        {4, "ialimitlow"},
        // high input attachment limit
        {64, "ialimithigh"},
    };

    TestGroupCase sampledImgCases[] = {
        // no sampled images
        {0, "nosampledimg"},
        // spec minmax image limit
        {16, "sampledimglow"},
        // high image limit
        {4096, "sampledimghigh"},
    };

    const struct
    {
        uint32_t sImgCount;
        uint32_t sTexCount;
        const char *name;
    } sImgTexCases[] = {
        // output storage image only
        {1, 0, "outimgonly"},
        // output image low storage tex limit
        {1, 3, "outimgtexlow"},
        // minmax storage images and no storage tex
        {4, 0, "lowimgnotex"},
        // low storage image single storage texel
        {3, 1, "lowimgsingletex"},
        // high limit of storage images and texel buffers
        {2048, 2048, "storageimghigh"},
    };

    const struct
    {
        uint32_t iubCount;
        uint32_t iubSize;
        const char *name;
    } iubCases[] = {
        // no inline uniform blocks
        {0, 0, "noiub"},
        // inline uniform blocks low limit
        {4, 256, "iublimitlow"},
        // inline uniform blocks high limit
        {8, 4096, "iublimithigh"},
    };

    TestGroupCase stageCases[] = {
        // compute
        {STAGE_COMPUTE, "comp"},
        // fragment
        {STAGE_FRAGMENT, "frag"},
        // vertex
        {STAGE_VERTEX, "vert"},
#ifndef CTS_USES_VULKANSC
        // raygen_nv
        {STAGE_RAYGEN_NV, "rgnv"},
        // raygen
        {STAGE_RAYGEN, "rgen"},
        // intersect
        {STAGE_INTERSECT, "sect"},
        // any_hit
        {STAGE_ANY_HIT, "ahit"},
        // closest_hit
        {STAGE_CLOSEST_HIT, "chit"},
        // miss
        {STAGE_MISS, "miss"},
        // callable
        {STAGE_CALLABLE, "call"},
        // task
        {STAGE_TASK, "task"},
        // mesh
        {STAGE_MESH, "mesh"},
#endif
    };

    TestGroupCase uabCases[] = {
        // no update after bind
        {UPDATE_AFTER_BIND_DISABLED, "nouab"},
        // enable update after bind
        {UPDATE_AFTER_BIND_ENABLED, "uab"},
    };

    for (int setsNdx = 0; setsNdx < DE_LENGTH_OF_ARRAY(setsCases); setsNdx++)
    {
        de::MovePtr<tcu::TestCaseGroup> setsGroup(new tcu::TestCaseGroup(testCtx, setsCases[setsNdx].name));
        for (int indexNdx = 0; indexNdx < DE_LENGTH_OF_ARRAY(indexCases); indexNdx++)
        {
            de::MovePtr<tcu::TestCaseGroup> indexGroup(new tcu::TestCaseGroup(testCtx, indexCases[indexNdx].name));
            for (int uboNdx = 0; uboNdx < DE_LENGTH_OF_ARRAY(uboCases); uboNdx++)
            {
                de::MovePtr<tcu::TestCaseGroup> uboGroup(new tcu::TestCaseGroup(testCtx, uboCases[uboNdx].name));
                for (int sboNdx = 0; sboNdx < DE_LENGTH_OF_ARRAY(sboCases); sboNdx++)
                {
                    de::MovePtr<tcu::TestCaseGroup> sboGroup(new tcu::TestCaseGroup(testCtx, sboCases[sboNdx].name));
                    for (int sampledImgNdx = 0; sampledImgNdx < DE_LENGTH_OF_ARRAY(sampledImgCases); sampledImgNdx++)
                    {
                        de::MovePtr<tcu::TestCaseGroup> sampledImgGroup(
                            new tcu::TestCaseGroup(testCtx, sampledImgCases[sampledImgNdx].name));
                        for (int storageImgNdx = 0; storageImgNdx < DE_LENGTH_OF_ARRAY(sImgTexCases); ++storageImgNdx)
                        {
                            de::MovePtr<tcu::TestCaseGroup> storageImgGroup(
                                new tcu::TestCaseGroup(testCtx, sImgTexCases[storageImgNdx].name));
                            for (int iubNdx = 0; iubNdx < DE_LENGTH_OF_ARRAY(iubCases); iubNdx++)
                            {
                                de::MovePtr<tcu::TestCaseGroup> iubGroup(
                                    new tcu::TestCaseGroup(testCtx, iubCases[iubNdx].name));
                                for (int uabNdx = 0; uabNdx < DE_LENGTH_OF_ARRAY(uabCases); uabNdx++)
                                {
                                    de::MovePtr<tcu::TestCaseGroup> uabGroup(
                                        new tcu::TestCaseGroup(testCtx, uabCases[uabNdx].name));
                                    for (int stageNdx = 0; stageNdx < DE_LENGTH_OF_ARRAY(stageCases); stageNdx++)
                                    {
                                        const Stage currentStage  = static_cast<Stage>(stageCases[stageNdx].count);
                                        const auto shaderStages   = getAllShaderStagesFor(currentStage, testCtx);
                                        const auto pipelineStages = getAllPipelineStagesFor(currentStage, testCtx);

                                        de::MovePtr<tcu::TestCaseGroup> stageGroup(
                                            new tcu::TestCaseGroup(testCtx, stageCases[stageNdx].name));
                                        for (int iaNdx = 0; iaNdx < DE_LENGTH_OF_ARRAY(iaCases); ++iaNdx)
                                        {
                                            // Input attachments can only be used in the fragment stage.
                                            if (currentStage != STAGE_FRAGMENT && iaCases[iaNdx].count > 0u)
                                                continue;

                                            // Allow only one high limit or all of them.
                                            uint32_t highLimitCount = 0u;
                                            if (uboNdx == DE_LENGTH_OF_ARRAY(uboCases) - 1)
                                                ++highLimitCount;
                                            if (sboNdx == DE_LENGTH_OF_ARRAY(sboCases) - 1)
                                                ++highLimitCount;
                                            if (sampledImgNdx == DE_LENGTH_OF_ARRAY(sampledImgCases) - 1)
                                                ++highLimitCount;
                                            if (storageImgNdx == DE_LENGTH_OF_ARRAY(sImgTexCases) - 1)
                                                ++highLimitCount;
                                            if (iaNdx == DE_LENGTH_OF_ARRAY(iaCases) - 1)
                                                ++highLimitCount;

                                            if (highLimitCount > 1 && highLimitCount < 5)
                                                continue;

                                            // Allow only all, all-but-one, none or one "zero limits" at the same time, except for inline uniform blocks.
                                            uint32_t zeroLimitCount = 0u;
                                            if (uboNdx == 0)
                                                ++zeroLimitCount;
                                            if (sboNdx == 0)
                                                ++zeroLimitCount;
                                            if (sampledImgNdx == 0)
                                                ++zeroLimitCount;
                                            if (storageImgNdx == 0)
                                                ++zeroLimitCount;
                                            if (iaNdx == 0)
                                                ++zeroLimitCount;

                                            if (zeroLimitCount > 1 && zeroLimitCount < 4)
                                                continue;

                                            // Avoid using multiple storage images if no dynamic indexing is being used.
                                            if (storageImgNdx >= 2 && indexNdx < 2)
                                                continue;

                                            // Skip the case of no UBOs, SSBOs or sampled images when no dynamic indexing is being used.
                                            if ((uboNdx == 0 || sboNdx == 0 || sampledImgNdx == 0) && indexNdx < 2)
                                                continue;

                                            de::MovePtr<tcu::TestCaseGroup> iaGroup(
                                                new tcu::TestCaseGroup(testCtx, iaCases[iaNdx].name));

                                            // Generate 10 random cases when working with only 4 sets and the number of descriptors is low. Otherwise just one case.
                                            // Exception: the case of no descriptors of any kind only needs one case.
                                            const uint32_t numSeeds =
                                                (setsCases[setsNdx].count == 4 && uboNdx < 2 && sboNdx < 2 &&
                                                 sampledImgNdx < 2 && storageImgNdx < 4 && iubNdx == 0 && iaNdx < 2 &&
                                                 (uboNdx != 0 || sboNdx != 0 || sampledImgNdx != 0 ||
                                                  storageImgNdx != 0 || iaNdx != 0)) ?
                                                    10 :
                                                    1;

                                            for (uint32_t rnd = 0; rnd < numSeeds; ++rnd)
                                            {
                                                CaseDef c = {
                                                    (IndexType)indexCases[indexNdx].count, // IndexType indexType;
                                                    setsCases[setsNdx].count, // uint32_t numDescriptorSets;
                                                    uboCases[uboNdx].count,   // uint32_t maxPerStageUniformBuffers;
                                                    8,                        // uint32_t maxUniformBuffersDynamic;
                                                    sboCases[sboNdx].count,   // uint32_t maxPerStageStorageBuffers;
                                                    4,                        // uint32_t maxStorageBuffersDynamic;
                                                    sampledImgCases[sampledImgNdx]
                                                        .count, // uint32_t maxPerStageSampledImages;
                                                    sImgTexCases[storageImgNdx]
                                                        .sImgCount, // uint32_t maxPerStageStorageImages;
                                                    sImgTexCases[storageImgNdx]
                                                        .sTexCount, // uint32_t maxPerStageStorageTexelBuffers;
                                                    iubCases[iubNdx].iubCount, // uint32_t maxInlineUniformBlocks;
                                                    iubCases[iubNdx].iubSize,  // uint32_t maxInlineUniformBlockSize;
                                                    iaCases[iaNdx].count,      // uint32_t maxPerStageInputAttachments;
                                                    currentStage,              // Stage stage;
                                                    (UpdateAfterBind)uabCases[uabNdx].count, // UpdateAfterBind uab;
                                                    seed++,                                  // uint32_t seed;
                                                    shaderStages,                            // VkFlags allShaderStages;
                                                    pipelineStages, // VkFlags allPipelineStages;
                                                    nullptr,        // std::shared_ptr<RandomLayout> randomLayout;
                                                };

                                                string name = de::toString(rnd);
                                                iaGroup->addChild(
                                                    new DescriptorSetRandomTestCase(testCtx, name.c_str(), c));
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

} // namespace BindingModel
} // namespace vkt
