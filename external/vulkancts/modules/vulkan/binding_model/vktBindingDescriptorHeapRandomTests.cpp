/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024 The Khronos Group Inc.
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
 * \brief Randomly-generated descriptor layouts driven through descriptor heaps.
 *
 * A port of the descriptorset_random stress tests to VK_EXT_descriptor_heap.
 * The shader keeps ordinary layout(set,binding) declarations; a per-stage array
 * of VkDescriptorSetAndBindingMappingEXT translates each binding to a heap
 * offset. Every descriptor is backed by data equal to its global linear index;
 * the shader reads each descriptor and accumulates any mismatch, then writes a
 * pass/fail texel. Some storage descriptors are written by the shader and
 * verified on the host.
 *//*--------------------------------------------------------------------*/

#include "vktBindingDescriptorHeapRandomTests.hpp"

// VK_EXT_descriptor_heap does not exist in Vulkan SC; keep this translation unit
// empty there. The group is only registered under the same guard.
#ifndef CTS_USES_VULKANSC

#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkCmdUtil.hpp"
#include "vkDefs.hpp"
#include "vkMemUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include "deRandom.hpp"
#include "deSharedPtr.hpp"
#include "deStringUtil.hpp"

#include <algorithm>
#include <list>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <typeinfo>
#include <vector>

namespace vkt
{
namespace BindingModel
{
namespace
{
using namespace vk;

static const uint32_t DIM = 8u;

enum IndexType
{
    INDEX_TYPE_NONE = 0,
    INDEX_TYPE_CONSTANT,
    INDEX_TYPE_PUSHCONSTANT,
    INDEX_TYPE_DEPENDENT,
    INDEX_TYPE_RUNTIME_SIZE,
};

enum Stage
{
    STAGE_COMPUTE = 0,
    STAGE_VERTEX,
    STAGE_FRAGMENT,
};

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
    bool writeGenerated = false;
};

// A randomly-generated set of descriptor bindings, shared between the test case
// (shader generation) and the test instance (resource setup / verification).
class RandomLayout
{
public:
    RandomLayout(uint32_t numSets) : layoutBindings(numSets), arraySizes(numSets), shaderBindings(numSets)
    {
    }

    // Indexed by [set][binding].
    std::vector<std::vector<VkDescriptorSetLayoutBinding>> layoutBindings;
    std::vector<std::vector<uint32_t>> arraySizes;
    // Shader/mapping binding number for each [set][binding]. An arrayed binding
    // reserves a binding-number range of size arraySize (each mapping spans
    // [firstBinding, firstBinding+bindingCount)), so binding numbers are spaced by
    // the array size to keep the per-binding mapping ranges disjoint.
    std::vector<std::vector<uint32_t>> shaderBindings;

    // Descriptors whose value is written by the shader instead of read.
    std::map<DescriptorId, WriteInfo> descriptorWrites;
};

struct CaseDef
{
    IndexType indexType;
    uint32_t numDescriptorSets;
    uint32_t maxPerStageUniformBuffers;
    uint32_t maxPerStageStorageBuffers;
    uint32_t maxPerStageSampledImages; // spent on uniform texel buffers, matching descriptorset_random
    uint32_t maxPerStageStorageImages;
    uint32_t maxPerStageStorageTexelBuffers;
    uint32_t maxPerStageInputAttachments; // fragment stage only
    Stage stage;
    uint32_t seed;
    // Shared by the test case and the test instance.
    std::shared_ptr<RandomLayout> randomLayout;
};

int32_t randRange(deRandom *rnd, int32_t min, int32_t max)
{
    if (max < 0)
        return 0;

    return (deRandom_getUint32(rnd) % (max - min + 1)) + min;
}

void chooseWritesRandomly(VkDescriptorType type, RandomLayout &randomLayout, deRandom &rnd, uint32_t set,
                          uint32_t binding, uint32_t count)
{
    switch (type)
    {
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
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
    uint32_t numUBO              = 0;
    uint32_t numSSBO             = 0;
    uint32_t numImage            = 0;
    uint32_t numStorageTex       = 0;
    uint32_t numSampled          = 0; // uniform texel buffers + combined image samplers share the sampled-image budget
    uint32_t numInputAttachments = 0;

    const uint32_t minBindings = 0;
    // Keep the workload roughly constant while exercising higher numbered sets.
    const uint32_t maxBindings = 128u / caseDef.numDescriptorSets;
    // No larger than 32 elements for dynamic indexing tests.
    const uint32_t maxArray = (caseDef.indexType == INDEX_TYPE_NONE) ? 0u : 32u;

    for (uint32_t s = 0; s < caseDef.numDescriptorSets; ++s)
    {
        int numBindings = randRange(&rnd, minBindings, maxBindings);

        // Guarantee room for the output image.
        if (s == 0 && numBindings == 0)
            numBindings = 1;

        randomLayout.layoutBindings[s] = std::vector<VkDescriptorSetLayoutBinding>(numBindings);
        randomLayout.arraySizes[s]     = std::vector<uint32_t>(numBindings);
    }

    // Iterate over bindings first, then over sets. This prevents the low-limit
    // bindings from getting clustered in low-numbered sets.
    for (uint32_t b = 0; b <= maxBindings; ++b)
    {
        for (uint32_t s = 0; s < caseDef.numDescriptorSets; ++s)
        {
            std::vector<VkDescriptorSetLayoutBinding> &bindings = randomLayout.layoutBindings[s];
            std::vector<uint32_t> &arraySizes                   = randomLayout.arraySizes[s];

            if (b >= bindings.size())
                continue;

            VkDescriptorSetLayoutBinding &binding = bindings[b];
            binding.binding                       = b;
            binding.pImmutableSamplers            = nullptr;
            binding.stageFlags                    = VK_SHADER_STAGE_COMPUTE_BIT;

            // Output image.
            if (s == 0 && b == 0)
            {
                binding.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                binding.descriptorCount = 1;
                numImage++;
                arraySizes[b] = 0;
                continue;
            }

            binding.descriptorCount = 0;

            std::map<int, VkDescriptorType> intToType;
            {
                int index          = 0;
                intToType[index++] = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                intToType[index++] = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                intToType[index++] = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
                intToType[index++] = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                intToType[index++] = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
                intToType[index++] = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                // Input attachments are only valid in the fragment stage.
                if (caseDef.stage == STAGE_FRAGMENT)
                    intToType[index++] = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
            }

            const int r = randRange(&rnd, 0, static_cast<int>(intToType.size() - 1));
            DE_ASSERT(r >= 0 && static_cast<size_t>(r) < intToType.size());

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
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                if (numSampled < caseDef.maxPerStageSampledImages)
                {
                    arraySizes[b] =
                        randRange(&rnd, 0, de::min(maxArray, caseDef.maxPerStageSampledImages - numSampled));
                    binding.descriptorCount = arraySizes[b] ? arraySizes[b] : 1;
                    numSampled += binding.descriptorCount;
                }
                break;
            case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
                // Kept non-arrayed: each input attachment maps to one render-pass attachment.
                if (numInputAttachments < caseDef.maxPerStageInputAttachments)
                {
                    arraySizes[b]           = 0;
                    binding.descriptorCount = 1;
                    numInputAttachments += 1;
                }
                break;
            }
        }
    }

    // Assign spaced shader/mapping binding numbers: an arrayed binding reserves a
    // binding-number range of size arraySize, so advance by that much.
    for (uint32_t s = 0; s < caseDef.numDescriptorSets; ++s)
    {
        const std::vector<VkDescriptorSetLayoutBinding> &bindings = randomLayout.layoutBindings[s];
        const std::vector<uint32_t> &arraySizes                   = randomLayout.arraySizes[s];
        std::vector<uint32_t> &shaderBindings                     = randomLayout.shaderBindings[s];
        shaderBindings.assign(bindings.size(), 0u);

        uint32_t nextBinding = 0u;
        for (size_t b = 0; b < bindings.size(); ++b)
        {
            if (bindings[b].descriptorCount == 0)
                continue;
            shaderBindings[b] = nextBinding;
            nextBinding += de::max(1u, arraySizes[b]);
        }
    }
}

// Limits the number of descriptors checked per binding so the generated shader
// stays a reasonable size for large arrays.
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
            DE_ASSERT(m_count >= kRandomChecksPerBinding);
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

class DescriptorHeapRandomInstance : public TestInstance
{
public:
    DescriptorHeapRandomInstance(Context &context, const std::shared_ptr<CaseDef> &data)
        : TestInstance(context)
        , m_data(data)
    {
    }

    tcu::TestStatus iterate(void) override;

private:
    std::shared_ptr<CaseDef> m_data;
};

class DescriptorHeapRandomCase : public TestCase
{
public:
    DescriptorHeapRandomCase(tcu::TestContext &testCtx, const std::string &name, const CaseDef &data)
        : TestCase(testCtx, name)
        , m_data(std::make_shared<CaseDef>(data))
    {
    }

    void initPrograms(SourceCollections &programCollection) const override;
    void checkSupport(Context &context) const override;
    std::string getRequiredCapabilitiesId() const override;
    void initDeviceCapabilities(DevCaps &caps) override;

    TestInstance *createInstance(Context &context) const override
    {
        return new DescriptorHeapRandomInstance(context, m_data);
    }

private:
    std::shared_ptr<CaseDef> m_data;
};

std::string DescriptorHeapRandomCase::getRequiredCapabilitiesId() const
{
    // Constant across all cases of this class (they share one device), but unique across CTS via the
    // mangled type name to avoid colliding with another test's capability id.
    return typeid(decltype(this)).name();
}

void DescriptorHeapRandomCase::initDeviceCapabilities(DevCaps &caps)
{
    caps.addExtension(VK_EXT_DESCRIPTOR_HEAP_EXTENSION_NAME);
    caps.addExtension(VK_KHR_SHADER_UNTYPED_POINTERS_EXTENSION_NAME);
    caps.addExtension(VK_KHR_MAINTENANCE_5_EXTENSION_NAME);
    caps.addExtension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);

    caps.addFeature(&VkPhysicalDeviceDescriptorHeapFeaturesEXT::descriptorHeap);
    caps.addFeature(&VkPhysicalDeviceShaderUntypedPointersFeaturesKHR::shaderUntypedPointers);
    caps.addFeature(&VkPhysicalDeviceMaintenance5FeaturesKHR::maintenance5);

    caps.addFeature(&VkPhysicalDeviceVulkan12Features::bufferDeviceAddress);
    caps.addFeature(&VkPhysicalDeviceVulkan12Features::runtimeDescriptorArray);
    caps.addFeature(&VkPhysicalDeviceVulkan12Features::shaderUniformTexelBufferArrayDynamicIndexing);
    caps.addFeature(&VkPhysicalDeviceVulkan12Features::shaderStorageTexelBufferArrayDynamicIndexing);

    caps.addFeature(&VkPhysicalDeviceFeatures::shaderUniformBufferArrayDynamicIndexing);
    caps.addFeature(&VkPhysicalDeviceFeatures::shaderStorageBufferArrayDynamicIndexing);
    caps.addFeature(&VkPhysicalDeviceFeatures::shaderStorageImageArrayDynamicIndexing);
    caps.addFeature(&VkPhysicalDeviceFeatures::shaderSampledImageArrayDynamicIndexing);

    // The graphics stages write the output image (and any storage-write descriptors) with imageStore.
    // Enabled unconditionally because getRequiredCapabilitiesId() is constant, so a single device is
    // shared across the comp/vert/frag cases; checkSupport still gates each stage on its feature.
    caps.addFeature(&VkPhysicalDeviceFeatures::vertexPipelineStoresAndAtomics);
    caps.addFeature(&VkPhysicalDeviceFeatures::fragmentStoresAndAtomics);
}

void DescriptorHeapRandomCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality(VK_EXT_DESCRIPTOR_HEAP_EXTENSION_NAME);
    context.requireDeviceFunctionality(VK_KHR_SHADER_UNTYPED_POINTERS_EXTENSION_NAME);
    context.requireDeviceFunctionality(VK_KHR_MAINTENANCE_5_EXTENSION_NAME);
    context.requireDeviceFunctionality(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);

    if (!context.getDescriptorHeapFeaturesEXT().descriptorHeap)
        TCU_THROW(NotSupportedError, "descriptorHeap is not supported");

    const CaseDef &data          = *m_data;
    const auto &features         = context.getDeviceFeatures();
    const auto &vulkan12Features = context.getDeviceVulkan12Features();
    const auto &limits           = context.getDeviceProperties().limits;

    if (data.numDescriptorSets > limits.maxBoundDescriptorSets)
        TCU_THROW(NotSupportedError, "Not enough bound descriptor sets supported");

    if (data.indexType == INDEX_TYPE_PUSHCONSTANT || data.indexType == INDEX_TYPE_DEPENDENT ||
        data.indexType == INDEX_TYPE_RUNTIME_SIZE)
    {
        if (data.maxPerStageUniformBuffers > 0 && !features.shaderUniformBufferArrayDynamicIndexing)
            TCU_THROW(NotSupportedError, "shaderUniformBufferArrayDynamicIndexing not supported");
        if (data.maxPerStageStorageBuffers > 0 && !features.shaderStorageBufferArrayDynamicIndexing)
            TCU_THROW(NotSupportedError, "shaderStorageBufferArrayDynamicIndexing not supported");
        if (data.maxPerStageSampledImages > 0 && !vulkan12Features.shaderUniformTexelBufferArrayDynamicIndexing)
            TCU_THROW(NotSupportedError, "shaderUniformTexelBufferArrayDynamicIndexing not supported");
        // Combined image samplers share the sampled-image budget and may be dynamically indexed.
        if (data.maxPerStageSampledImages > 0 && !features.shaderSampledImageArrayDynamicIndexing)
            TCU_THROW(NotSupportedError, "shaderSampledImageArrayDynamicIndexing not supported");
        if (data.maxPerStageStorageTexelBuffers > 0 && !vulkan12Features.shaderStorageTexelBufferArrayDynamicIndexing)
            TCU_THROW(NotSupportedError, "shaderStorageTexelBufferArrayDynamicIndexing not supported");
        // The output storage image is never array-indexed, so only guard when more than one is present.
        if (data.maxPerStageStorageImages > 1 && !features.shaderStorageImageArrayDynamicIndexing)
            TCU_THROW(NotSupportedError, "shaderStorageImageArrayDynamicIndexing not supported");
    }

    if (data.indexType == INDEX_TYPE_RUNTIME_SIZE && !vulkan12Features.runtimeDescriptorArray)
        TCU_THROW(NotSupportedError, "runtimeDescriptorArray not supported");

    if (data.stage == STAGE_VERTEX && !features.vertexPipelineStoresAndAtomics)
        TCU_THROW(NotSupportedError, "vertexPipelineStoresAndAtomics not supported");
    if (data.stage == STAGE_FRAGMENT && !features.fragmentStoresAndAtomics)
        TCU_THROW(NotSupportedError, "fragmentStoresAndAtomics not supported");

    if (data.maxPerStageInputAttachments > limits.maxPerStageDescriptorInputAttachments)
        TCU_THROW(NotSupportedError, "maxPerStageDescriptorInputAttachments too low");
}

void DescriptorHeapRandomCase::initPrograms(SourceCollections &programCollection) const
{
    CaseDef &data = *m_data;

    deRandom rnd;
    deRandom_init(&rnd, data.seed);

    data.randomLayout.reset(new RandomLayout(data.numDescriptorSets));
    RandomLayout &randomLayout = *data.randomLayout.get();
    generateRandomLayout(randomLayout, data, rnd);

    std::stringstream decls, checks;

    uint32_t descriptor           = 0;
    uint32_t inputAttachmentIndex = 0;

    for (uint32_t s = 0; s < data.numDescriptorSets; ++s)
    {
        std::vector<VkDescriptorSetLayoutBinding> &bindings = randomLayout.layoutBindings[s];
        std::vector<uint32_t> &arraySizes                   = randomLayout.arraySizes[s];

        for (size_t b = 0; b < bindings.size(); ++b)
        {
            VkDescriptorSetLayoutBinding &binding = bindings[b];

            if (binding.descriptorCount == 0)
                continue;

            const uint32_t shaderBinding = randomLayout.shaderBindings[s][b];

            std::stringstream array;
            if (data.indexType == INDEX_TYPE_RUNTIME_SIZE)
            {
                if (arraySizes[b])
                    array << "[]";
            }
            else
            {
                if (arraySizes[b])
                    array << "[" << arraySizes[b] << "]";
            }

            switch (binding.descriptorType)
            {
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                decls << "layout(set = " << s << ", binding = " << shaderBinding << ") uniform ubodef" << s << "_" << b
                      << " { int val; } ubo" << s << "_" << b << array.str() << ";\n";
                break;
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                decls << "layout(set = " << s << ", binding = " << shaderBinding << ") buffer sbodef" << s << "_" << b
                      << " { int val; } ssbo" << s << "_" << b << array.str() << ";\n";
                break;
            case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
                decls << "layout(set = " << s << ", binding = " << shaderBinding << ") uniform itextureBuffer texbo"
                      << s << "_" << b << array.str() << ";\n";
                break;
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                decls << "layout(set = " << s << ", binding = " << shaderBinding << ") uniform isampler2D combined" << s
                      << "_" << b << array.str() << ";\n";
                break;
            case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
                decls << "layout(r32i, set = " << s << ", binding = " << shaderBinding << ") uniform iimageBuffer image"
                      << s << "_" << b << array.str() << ";\n";
                break;
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                decls << "layout(r32i, set = " << s << ", binding = " << shaderBinding << ") uniform iimage2D simage"
                      << s << "_" << b << array.str() << ";\n";
                break;
            case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
                decls << "layout(input_attachment_index = " << inputAttachmentIndex << ", set = " << s
                      << ", binding = " << shaderBinding << ") uniform isubpassInput attachment" << s << "_" << b
                      << ";\n";
                inputAttachmentIndex += binding.descriptorCount;
                break;
            default:
                DE_ASSERT(0);
            }

            const uint32_t arraySize = de::max(1u, arraySizes[b]);
            CheckDecider checkDecider(rnd, arraySize);

            for (uint32_t ai = 0; ai < arraySize; ++ai, ++descriptor)
            {
                // The output image is written by every invocation, not checked.
                if (s == 0 && b == 0)
                    continue;

                if (!checkDecider.shouldCheck(ai))
                    continue;

                std::stringstream ind;
                switch (data.indexType)
                {
                case INDEX_TYPE_NONE:
                case INDEX_TYPE_CONSTANT:
                    if (arraySizes[b])
                        ind << "[" << ai << "]";
                    break;
                case INDEX_TYPE_PUSHCONSTANT:
                    // Index comes from a push-constant identity array (identity[i] == i).
                    if (arraySizes[b])
                        ind << "[pc.identity[" << ai << "]]";
                    break;
                case INDEX_TYPE_DEPENDENT:
                case INDEX_TYPE_RUNTIME_SIZE:
                    // Index is a function of the running accumulator (which is zero on success).
                    if (arraySizes[b])
                        ind << "[accum + " << ai << "]";
                    break;
                default:
                    DE_ASSERT(0);
                }

                const DescriptorId descriptorId(s, static_cast<uint32_t>(b), ai);
                auto writesItr = randomLayout.descriptorWrites.find(descriptorId);

                if (writesItr == randomLayout.descriptorWrites.end())
                {
                    switch (binding.descriptorType)
                    {
                    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                        checks << "  temp = ubo" << s << "_" << b << ind.str() << ".val;\n";
                        break;
                    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                        checks << "  temp = ssbo" << s << "_" << b << ind.str() << ".val;\n";
                        break;
                    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
                        checks << "  temp = texelFetch(texbo" << s << "_" << b << ind.str() << ", 0).x;\n";
                        break;
                    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                        checks << "  temp = texture(combined" << s << "_" << b << ind.str() << ", vec2(0.5, 0.5)).x;\n";
                        break;
                    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
                        checks << "  temp = subpassLoad(attachment" << s << "_" << b << ").r;\n";
                        break;
                    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
                        checks << "  temp = imageLoad(image" << s << "_" << b << ind.str() << ", 0).x;\n";
                        break;
                    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                        checks << "  temp = imageLoad(simage" << s << "_" << b << ind.str() << ", ivec2(0, 0)).x;\n";
                        break;
                    default:
                        DE_ASSERT(0);
                    }

                    checks << "  accum |= temp - " << descriptor << ";\n";
                }
                else
                {
                    writesItr->second.writeGenerated = true;

                    // Assign each write operation to a single invocation to avoid race conditions.
                    const auto expectedInvocationID = descriptor % (DIM * DIM);
                    const std::string writeCond     = "if (" + de::toString(expectedInvocationID) + " == invocationID)";

                    switch (binding.descriptorType)
                    {
                    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                        checks << "  " << writeCond << " ssbo" << s << "_" << b << ind.str() << ".val = " << descriptor
                               << ";\n";
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

    const ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, SPIRV_VERSION_1_4, 0u, true);

    // Common header + descriptor declarations, followed by the per-stage main().
    // Every stage computes a per-invocation id, runs the checks, and writes the pass/fail
    // color to the output storage image (simage0_0) with imageStore.
    std::string header = "#version 450 core\n"
                         "#extension GL_EXT_nonuniform_qualifier : enable\n";
    if (data.indexType == INDEX_TYPE_PUSHCONSTANT)
        header += "layout(push_constant, std430) uniform PushBlock { int identity[32]; } pc;\n";
    header += decls.str();

    std::stringstream css;
    switch (data.stage)
    {
    case STAGE_COMPUTE:
        css << header
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
        programCollection.glslSources.add("test") << glu::ComputeSource(css.str()) << buildOptions;
        break;

    case STAGE_VERTEX:
        // One point per invocation; rasterization is discarded, the work is done in the VS.
        css << header
            << "out gl_PerVertex { vec4 gl_Position; float gl_PointSize; };\n"
               "void main()\n"
               "{\n"
               "  const int invocationID = gl_VertexIndex;\n"
               "  int accum = 0, temp;\n"
            << checks.str()
            << "  ivec4 color = (accum != 0) ? ivec4(0,0,0,0) : ivec4(1,0,0,1);\n"
               "  imageStore(simage0_0, ivec2(invocationID % "
            << DIM << ", invocationID / " << DIM
            << "), color);\n"
               "  gl_Position = vec4(0.0, 0.0, 0.0, 1.0);\n"
               "  gl_PointSize = 1.0;\n"
               "}\n";
        programCollection.glslSources.add("test") << glu::VertexSource(css.str()) << buildOptions;
        break;

    case STAGE_FRAGMENT:
    {
        // Full-screen triangle vertex shader; the fragment shader does the work per pixel.
        std::stringstream vss;
        vss << "#version 450 core\n"
               "void main()\n"
               "{\n"
               "  vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);\n"
               "  gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);\n"
               "}\n";
        programCollection.glslSources.add("vert") << glu::VertexSource(vss.str()) << buildOptions;

        css << header
            << "void main()\n"
               "{\n"
               "  const int invocationID = int(gl_FragCoord.y) * "
            << DIM
            << " + int(gl_FragCoord.x);\n"
               "  int accum = 0, temp;\n"
            << checks.str()
            << "  ivec4 color = (accum != 0) ? ivec4(0,0,0,0) : ivec4(1,0,0,1);\n"
               "  imageStore(simage0_0, ivec2(gl_FragCoord.xy), color);\n"
               "}\n";
        programCollection.glslSources.add("test") << glu::FragmentSource(css.str()) << buildOptions;
        break;
    }
    default:
        DE_ASSERT(0);
    }
}

// A host-visible buffer with a queryable device address, usable as a descriptor heap or backing store.
struct HeapBuffer
{
    Move<VkBuffer> buffer;
    de::MovePtr<Allocation> allocation;
    VkDeviceAddress address = 0;
};

std::unique_ptr<HeapBuffer> createHeapBuffer(const InstanceInterface &vki, VkPhysicalDevice physicalDevice,
                                             const DeviceInterface &vkd, VkDevice device, VkDeviceSize size,
                                             VkBufferUsageFlags2KHR usage)
{
    VkBufferUsageFlags2CreateInfoKHR usageFlags2 = initVulkanStructure();
    usageFlags2.usage                            = usage | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR;

    VkBufferCreateInfo createInfo = initVulkanStructure(&usageFlags2);
    createInfo.size               = size;
    createInfo.usage              = static_cast<VkBufferUsageFlags>(usageFlags2.usage);
    createInfo.sharingMode        = VK_SHARING_MODE_EXCLUSIVE;

    auto result    = std::make_unique<HeapBuffer>();
    result->buffer = createBuffer(vkd, device, &createInfo);

    const auto memReqs = getBufferMemoryRequirements(vkd, device, *result->buffer);

    VkMemoryAllocateFlagsInfo allocFlagsInfo = initVulkanStructure();
    allocFlagsInfo.flags                     = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    result->allocation =
        allocateExtended(vki, vkd, physicalDevice, device, memReqs, MemoryRequirement::HostVisible, &allocFlagsInfo);
    vkd.bindBufferMemory(device, *result->buffer, result->allocation->getMemory(), result->allocation->getOffset());

    VkBufferDeviceAddressInfo bdaInfo = initVulkanStructure();
    bdaInfo.buffer                    = *result->buffer;
    result->address                   = vkd.getBufferDeviceAddress(device, &bdaInfo);

    return result;
}

struct ValueImage
{
    Move<VkImage> image;
    de::MovePtr<Allocation> allocation;
    uint32_t descriptor = 0;
    bool isWrite        = false;
};

std::unique_ptr<ValueImage> createValueImage(const DeviceInterface &vkd, VkDevice device, Allocator &allocator,
                                             uint32_t width, uint32_t height, VkImageUsageFlags usage)
{
    VkImageCreateInfo createInfo = initVulkanStructure();
    createInfo.imageType         = VK_IMAGE_TYPE_2D;
    createInfo.format            = VK_FORMAT_R32_SINT;
    createInfo.extent            = {width, height, 1u};
    createInfo.mipLevels         = 1u;
    createInfo.arrayLayers       = 1u;
    createInfo.samples           = VK_SAMPLE_COUNT_1_BIT;
    createInfo.tiling            = VK_IMAGE_TILING_OPTIMAL;
    createInfo.usage             = usage;
    createInfo.sharingMode       = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;

    auto result   = std::make_unique<ValueImage>();
    result->image = createImage(vkd, device, &createInfo);

    const auto memReqs = getImageMemoryRequirements(vkd, device, *result->image);
    result->allocation = allocator.allocate(memReqs, MemoryRequirement::Any);
    vkd.bindImageMemory(device, *result->image, result->allocation->getMemory(), result->allocation->getOffset());

    return result;
}

VkImageViewCreateInfo makeStorageImageViewInfo(VkImage image)
{
    VkImageViewCreateInfo viewInfo = initVulkanStructure();
    viewInfo.image                 = image;
    viewInfo.viewType              = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                = VK_FORMAT_R32_SINT;
    viewInfo.components            = makeComponentMappingRGBA();
    viewInfo.subresourceRange      = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    return viewInfo;
}

VkDeviceSize alignUp(VkDeviceSize value, VkDeviceSize alignment)
{
    return (value + alignment - 1u) & ~(alignment - 1u);
}

tcu::TestStatus DescriptorHeapRandomInstance::iterate(void)
{
    const CaseDef &data        = *m_data;
    RandomLayout &randomLayout = *data.randomLayout.get();

    const auto ctx              = m_context.getContextCommonData();
    const auto &vki             = ctx.vki;
    const auto &vkd             = ctx.vkd;
    const auto physicalDevice   = ctx.physicalDevice;
    const auto device           = ctx.device;
    const auto queue            = ctx.queue;
    const auto queueFamilyIndex = ctx.qfIndex;
    Allocator &allocator        = ctx.allocator;
    const auto &heapProperties  = m_context.getDescriptorHeapPropertiesEXT();
    const auto &limits          = m_context.getDeviceProperties().limits;

    const VkDeviceSize resourceStride =
        de::max(alignUp(heapProperties.bufferDescriptorSize, heapProperties.bufferDescriptorAlignment),
                alignUp(heapProperties.imageDescriptorSize, heapProperties.imageDescriptorAlignment));

    // Slot size for the backing store; each descriptor stores its index at the start of its slot.
    const VkDeviceSize valueAlign =
        de::max(de::max(limits.minUniformBufferOffsetAlignment, limits.minStorageBufferOffsetAlignment),
                de::max(limits.minTexelBufferOffsetAlignment, static_cast<VkDeviceSize>(sizeof(uint32_t))));

    // First pass: compute the global descriptor index of the first element of each binding.
    std::vector<std::vector<uint32_t>> bindingStart(data.numDescriptorSets);
    uint32_t numDescriptors = 0u;
    for (uint32_t s = 0; s < data.numDescriptorSets; ++s)
    {
        const auto &bindings = randomLayout.layoutBindings[s];
        const auto &sizes    = randomLayout.arraySizes[s];
        bindingStart[s].resize(bindings.size(), 0u);
        for (size_t b = 0; b < bindings.size(); ++b)
        {
            bindingStart[s][b] = numDescriptors;
            if (bindings[b].descriptorCount == 0)
                continue;
            numDescriptors += de::max(1u, sizes[b]);
        }
    }

    DE_ASSERT(numDescriptors > 0u);

    // Backing store: one value slot per descriptor.
    const auto backingBuffer =
        createHeapBuffer(vki, physicalDevice, vkd, device, valueAlign * numDescriptors,
                         VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT |
                             VK_BUFFER_USAGE_2_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_2_STORAGE_TEXEL_BUFFER_BIT);
    auto backingHostPtr = reinterpret_cast<char *>(backingBuffer->allocation->getHostPtr());

    // Resource heap.
    const VkDeviceSize heapUserSize = alignUp(resourceStride * numDescriptors, heapProperties.resourceHeapAlignment);
    const VkDeviceSize heapSize     = heapUserSize + heapProperties.minResourceHeapReservedRange;
    const auto resourceHeap =
        createHeapBuffer(vki, physicalDevice, vkd, device, heapSize, VK_BUFFER_USAGE_2_DESCRIPTOR_HEAP_BIT_EXT);
    auto resourceHeapHostPtr = reinterpret_cast<char *>(resourceHeap->allocation->getHostPtr());

    const auto storageImageUsage =
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    const auto sampledImageUsage    = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    const auto inputAttachmentUsage = VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    // Output image (descriptor 0).
    const auto outputImage = createValueImage(vkd, device, allocator, DIM, DIM, storageImageUsage);

    // Keep-alive storage for the deferred descriptor write structures.
    std::list<VkImageViewCreateInfo> viewInfos;
    std::list<VkImageDescriptorInfoEXT> imageInfos;
    std::list<VkTexelBufferDescriptorInfoEXT> texelInfos;
    std::list<VkDeviceAddressRangeEXT> addressRanges;
    std::vector<VkResourceDescriptorInfoEXT> resourceInfos;
    std::vector<VkHostAddressRangeEXT> resourceDests;

    std::list<std::unique_ptr<ValueImage>> storageImages; // writable value storage images (descriptor > 0)
    std::list<std::unique_ptr<ValueImage>> sampledImages; // read-only sampled value images
    std::list<std::unique_ptr<ValueImage>> inputAttachmentImages;
    std::vector<Move<VkImageView>> inputAttachmentViews; // live views, in input_attachment_index order

    struct ClearInfo
    {
        VkImage image;
        int32_t value;
        VkImageLayout finalLayout;
    };
    std::vector<ClearInfo> imageClears;
    bool usesSampler = false;

    // Second pass: create resources, fill backing values, build heap descriptors.
    uint32_t descriptor = 0u;
    for (uint32_t s = 0; s < data.numDescriptorSets; ++s)
    {
        const auto &bindings = randomLayout.layoutBindings[s];
        const auto &sizes    = randomLayout.arraySizes[s];

        for (size_t b = 0; b < bindings.size(); ++b)
        {
            const VkDescriptorSetLayoutBinding &binding = bindings[b];
            if (binding.descriptorCount == 0)
                continue;

            const uint32_t arraySize = de::max(1u, sizes[b]);
            for (uint32_t ai = 0; ai < arraySize; ++ai, ++descriptor)
            {
                const DescriptorId descriptorId(s, static_cast<uint32_t>(b), ai);
                const bool isWrite =
                    (randomLayout.descriptorWrites.find(descriptorId) != randomLayout.descriptorWrites.end());
                const int32_t value = isWrite ? -1 : static_cast<int32_t>(descriptor);

                VkResourceDescriptorInfoEXT resourceInfo = initVulkanStructure();
                VkHostAddressRangeEXT dst{};
                dst.address = resourceHeapHostPtr + descriptor * resourceStride;
                dst.size    = static_cast<size_t>(resourceStride);

                const bool outputImageSlot = (s == 0 && b == 0);

                switch (binding.descriptorType)
                {
                case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                {
                    *reinterpret_cast<int32_t *>(backingHostPtr + descriptor * valueAlign) = value;

                    VkDeviceAddressRangeEXT &range = addressRanges.emplace_back();
                    range.address                  = backingBuffer->address + descriptor * valueAlign;
                    range.size                     = valueAlign;

                    resourceInfo.type               = binding.descriptorType;
                    resourceInfo.data.pAddressRange = &range;
                    break;
                }
                case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
                case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
                {
                    *reinterpret_cast<int32_t *>(backingHostPtr + descriptor * valueAlign) = value;

                    VkTexelBufferDescriptorInfoEXT &texelInfo = texelInfos.emplace_back();
                    texelInfo                                 = initVulkanStructure();
                    texelInfo.format                          = VK_FORMAT_R32_SINT;
                    texelInfo.addressRange.address            = backingBuffer->address + descriptor * valueAlign;
                    texelInfo.addressRange.size               = valueAlign;

                    resourceInfo.type              = binding.descriptorType;
                    resourceInfo.data.pTexelBuffer = &texelInfo;
                    break;
                }
                case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                {
                    VkImage image = VK_NULL_HANDLE;
                    if (outputImageSlot)
                    {
                        image = *outputImage->image;
                        imageClears.push_back({image, 0, VK_IMAGE_LAYOUT_GENERAL}); // output image cleared to 0
                    }
                    else
                    {
                        auto valueImage        = createValueImage(vkd, device, allocator, 1u, 1u, storageImageUsage);
                        valueImage->descriptor = descriptor;
                        valueImage->isWrite    = isWrite;
                        image                  = *valueImage->image;
                        imageClears.push_back({image, value, VK_IMAGE_LAYOUT_GENERAL});
                        storageImages.emplace_back(std::move(valueImage));
                    }

                    VkImageViewCreateInfo &viewInfo = viewInfos.emplace_back();
                    viewInfo                        = makeStorageImageViewInfo(image);

                    VkImageDescriptorInfoEXT &imageInfo = imageInfos.emplace_back();
                    imageInfo                           = initVulkanStructure();
                    imageInfo.pView                     = &viewInfo;
                    imageInfo.layout                    = VK_IMAGE_LAYOUT_GENERAL;

                    resourceInfo.type        = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                    resourceInfo.data.pImage = &imageInfo;
                    break;
                }
                case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                {
                    // Read-only sampled image; the sampler comes from the sampler heap (shared slot 0).
                    auto valueImage        = createValueImage(vkd, device, allocator, 1u, 1u, sampledImageUsage);
                    valueImage->descriptor = descriptor;
                    const VkImage image    = *valueImage->image;
                    imageClears.push_back({image, value, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
                    sampledImages.emplace_back(std::move(valueImage));

                    VkImageViewCreateInfo &viewInfo = viewInfos.emplace_back();
                    viewInfo                        = makeStorageImageViewInfo(image);

                    VkImageDescriptorInfoEXT &imageInfo = imageInfos.emplace_back();
                    imageInfo                           = initVulkanStructure();
                    imageInfo.pView                     = &viewInfo;
                    imageInfo.layout                    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                    resourceInfo.type        = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                    resourceInfo.data.pImage = &imageInfo;
                    usesSampler              = true;
                    break;
                }
                case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
                {
                    // DIM x DIM read-only image, pre-cleared to the descriptor index and used both as a
                    // render-pass input attachment and as the heap descriptor read via subpassLoad.
                    auto valueImage        = createValueImage(vkd, device, allocator, DIM, DIM, inputAttachmentUsage);
                    valueImage->descriptor = descriptor;
                    const VkImage image    = *valueImage->image;
                    imageClears.push_back({image, value, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});

                    // Live view for the framebuffer (in input_attachment_index order).
                    VkImageViewCreateInfo fbViewInfo = makeStorageImageViewInfo(image);
                    inputAttachmentViews.push_back(createImageView(vkd, device, &fbViewInfo));
                    inputAttachmentImages.emplace_back(std::move(valueImage));

                    VkImageViewCreateInfo &viewInfo = viewInfos.emplace_back();
                    viewInfo                        = makeStorageImageViewInfo(image);

                    VkImageDescriptorInfoEXT &imageInfo = imageInfos.emplace_back();
                    imageInfo                           = initVulkanStructure();
                    imageInfo.pView                     = &viewInfo;
                    imageInfo.layout                    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                    resourceInfo.type        = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
                    resourceInfo.data.pImage = &imageInfo;
                    break;
                }
                default:
                    DE_ASSERT(0);
                }

                resourceInfos.push_back(resourceInfo);
                resourceDests.push_back(dst);
            }
        }
    }

    VK_CHECK(vkd.writeResourceDescriptorsEXT(device, static_cast<uint32_t>(resourceInfos.size()), resourceInfos.data(),
                                             resourceDests.data()));
    flushAlloc(vkd, device, *resourceHeap->allocation);
    flushAlloc(vkd, device, *backingBuffer->allocation);

    // Build one mapping per binding.
    std::vector<VkDescriptorSetAndBindingMappingEXT> mappings;
    for (uint32_t s = 0; s < data.numDescriptorSets; ++s)
    {
        const auto &bindings = randomLayout.layoutBindings[s];
        const auto &sizes    = randomLayout.arraySizes[s];
        for (size_t b = 0; b < bindings.size(); ++b)
        {
            if (bindings[b].descriptorCount == 0)
                continue;

            const uint32_t arraySize = de::max(1u, sizes[b]);

            VkDescriptorSetAndBindingMappingEXT mapping  = initVulkanStructure();
            mapping.descriptorSet                        = s;
            mapping.firstBinding                         = randomLayout.shaderBindings[s][b];
            mapping.bindingCount                         = arraySize;
            mapping.resourceMask                         = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
            mapping.source                               = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
            mapping.sourceData.constantOffset.heapOffset = static_cast<uint32_t>(bindingStart[s][b] * resourceStride);
            mapping.sourceData.constantOffset.heapArrayStride = static_cast<uint32_t>(resourceStride);
            mappings.push_back(mapping);
        }
    }

    // Pipeline (no pipeline layout; descriptors come from the heap). The same mapping info is
    // chained into every shader stage.
    VkShaderDescriptorSetAndBindingMappingInfoEXT mappingInfo = initVulkanStructure();
    mappingInfo.mappingCount                                  = static_cast<uint32_t>(mappings.size());
    mappingInfo.pMappings                                     = mappings.data();

    VkPipelineCreateFlags2CreateInfoKHR pipelineFlags2 = initVulkanStructure();
    pipelineFlags2.flags                               = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;

    Move<VkPipeline> pipeline;
    Move<VkRenderPass> renderPass;
    Move<VkFramebuffer> framebuffer;
    Move<VkShaderModule> vertModule;
    Move<VkShaderModule> mainModule;

    if (data.stage == STAGE_COMPUTE)
    {
        mainModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("test"));

        VkComputePipelineCreateInfo pipelineCreateInfo = initVulkanStructure(&pipelineFlags2);
        pipelineCreateInfo.stage                       = initVulkanStructure(&mappingInfo);
        pipelineCreateInfo.stage.stage                 = VK_SHADER_STAGE_COMPUTE_BIT;
        pipelineCreateInfo.stage.module                = *mainModule;
        pipelineCreateInfo.stage.pName                 = "main";
        pipelineCreateInfo.layout                      = VK_NULL_HANDLE;

        pipeline = createComputePipeline(vkd, device, VK_NULL_HANDLE, &pipelineCreateInfo);
    }
    else
    {
        // The shader writes the output via a storage image, so the render pass has no color
        // attachments. Any input attachments are pre-cleared images read via subpassLoad; they are the
        // only render-pass attachments (loadOp LOAD, kept in SHADER_READ_ONLY_OPTIMAL).
        const uint32_t numInputAttachments = static_cast<uint32_t>(inputAttachmentViews.size());

        std::vector<VkAttachmentDescription> attachmentDescs(numInputAttachments);
        std::vector<VkAttachmentReference> inputRefs(numInputAttachments);
        std::vector<VkImageView> framebufferViews(numInputAttachments);
        for (uint32_t i = 0; i < numInputAttachments; ++i)
        {
            attachmentDescs[i]                = {};
            attachmentDescs[i].format         = VK_FORMAT_R32_SINT;
            attachmentDescs[i].samples        = VK_SAMPLE_COUNT_1_BIT;
            attachmentDescs[i].loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD;
            attachmentDescs[i].storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachmentDescs[i].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachmentDescs[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachmentDescs[i].initialLayout  = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            attachmentDescs[i].finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            inputRefs[i]        = {i, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            framebufferViews[i] = *inputAttachmentViews[i];
        }

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.inputAttachmentCount = numInputAttachments;
        subpass.pInputAttachments    = inputRefs.empty() ? nullptr : inputRefs.data();

        VkRenderPassCreateInfo renderPassInfo = initVulkanStructure();
        renderPassInfo.attachmentCount        = numInputAttachments;
        renderPassInfo.pAttachments           = attachmentDescs.empty() ? nullptr : attachmentDescs.data();
        renderPassInfo.subpassCount           = 1u;
        renderPassInfo.pSubpasses             = &subpass;
        renderPass                            = createRenderPass(vkd, device, &renderPassInfo);

        VkFramebufferCreateInfo framebufferInfo = initVulkanStructure();
        framebufferInfo.renderPass              = *renderPass;
        framebufferInfo.attachmentCount         = numInputAttachments;
        framebufferInfo.pAttachments            = framebufferViews.empty() ? nullptr : framebufferViews.data();
        framebufferInfo.width                   = DIM;
        framebufferInfo.height                  = DIM;
        framebufferInfo.layers                  = 1u;
        framebuffer                             = createFramebuffer(vkd, device, &framebufferInfo);

        std::vector<VkPipelineShaderStageCreateInfo> stages;
        if (data.stage == STAGE_VERTEX)
        {
            mainModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("test"));
            VkPipelineShaderStageCreateInfo vs = initVulkanStructure(&mappingInfo);
            vs.stage                           = VK_SHADER_STAGE_VERTEX_BIT;
            vs.module                          = *mainModule;
            vs.pName                           = "main";
            stages.push_back(vs);
        }
        else
        {
            vertModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("vert"));
            mainModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("test"));

            VkPipelineShaderStageCreateInfo vs = initVulkanStructure(&mappingInfo);
            vs.stage                           = VK_SHADER_STAGE_VERTEX_BIT;
            vs.module                          = *vertModule;
            vs.pName                           = "main";
            stages.push_back(vs);

            VkPipelineShaderStageCreateInfo fs = initVulkanStructure(&mappingInfo);
            fs.stage                           = VK_SHADER_STAGE_FRAGMENT_BIT;
            fs.module                          = *mainModule;
            fs.pName                           = "main";
            stages.push_back(fs);
        }

        const VkPipelineVertexInputStateCreateInfo vertexInputState = initVulkanStructure();

        VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = initVulkanStructure();
        inputAssemblyState.topology =
            (data.stage == STAGE_VERTEX) ? VK_PRIMITIVE_TOPOLOGY_POINT_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        const VkViewport viewport =
            makeViewport(0.0f, 0.0f, static_cast<float>(DIM), static_cast<float>(DIM), 0.0f, 1.0f);
        const VkRect2D scissor = makeRect2D(0, 0, DIM, DIM);

        VkPipelineViewportStateCreateInfo viewportState = initVulkanStructure();
        viewportState.viewportCount                     = 1u;
        viewportState.pViewports                        = &viewport;
        viewportState.scissorCount                      = 1u;
        viewportState.pScissors                         = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterizationState = initVulkanStructure();
        rasterizationState.rasterizerDiscardEnable                = (data.stage == STAGE_VERTEX) ? VK_TRUE : VK_FALSE;
        rasterizationState.polygonMode                            = VK_POLYGON_MODE_FILL;
        rasterizationState.cullMode                               = VK_CULL_MODE_NONE;
        rasterizationState.frontFace                              = VK_FRONT_FACE_CLOCKWISE;
        rasterizationState.lineWidth                              = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisampleState = initVulkanStructure();
        multisampleState.rasterizationSamples                 = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencilState = initVulkanStructure();
        depthStencilState.maxDepthBounds                        = 1.0f;

        VkPipelineColorBlendStateCreateInfo colorBlendState = initVulkanStructure();
        colorBlendState.attachmentCount                     = 0u;

        VkGraphicsPipelineCreateInfo pipelineCreateInfo = initVulkanStructure(&pipelineFlags2);
        pipelineCreateInfo.stageCount                   = static_cast<uint32_t>(stages.size());
        pipelineCreateInfo.pStages                      = stages.data();
        pipelineCreateInfo.pVertexInputState            = &vertexInputState;
        pipelineCreateInfo.pInputAssemblyState          = &inputAssemblyState;
        pipelineCreateInfo.pViewportState               = &viewportState;
        pipelineCreateInfo.pRasterizationState          = &rasterizationState;
        pipelineCreateInfo.pMultisampleState            = &multisampleState;
        pipelineCreateInfo.pDepthStencilState           = &depthStencilState;
        pipelineCreateInfo.pColorBlendState             = &colorBlendState;
        pipelineCreateInfo.layout                       = VK_NULL_HANDLE;
        pipelineCreateInfo.renderPass                   = *renderPass;
        pipelineCreateInfo.subpass                      = 0u;

        pipeline = createGraphicsPipeline(vkd, device, VK_NULL_HANDLE, &pipelineCreateInfo);
    }

    const VkPipelineStageFlags shaderStage = (data.stage == STAGE_COMPUTE) ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT :
                                             (data.stage == STAGE_VERTEX)  ? VK_PIPELINE_STAGE_VERTEX_SHADER_BIT :
                                                                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    const VkPipelineBindPoint bindPoint =
        (data.stage == STAGE_COMPUTE) ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;

    // Output copy buffer and per-write-image readback buffer.
    const VkDeviceSize copyBufferSize = DIM * DIM * sizeof(int32_t);
    const auto copyBuffer =
        createHeapBuffer(vki, physicalDevice, vkd, device, copyBufferSize, VK_BUFFER_USAGE_2_TRANSFER_DST_BIT);

    std::vector<const ValueImage *> writeImages;
    for (const auto &img : storageImages)
        if (img->isWrite)
            writeImages.push_back(img.get());
    const VkDeviceSize readbackSize = de::max<VkDeviceSize>(1u, writeImages.size()) * sizeof(int32_t);
    const auto readbackBuffer =
        createHeapBuffer(vki, physicalDevice, vkd, device, readbackSize, VK_BUFFER_USAGE_2_TRANSFER_DST_BIT);

    VkBindHeapInfoEXT resourceHeapBindInfo   = initVulkanStructure();
    resourceHeapBindInfo.heapRange.address   = resourceHeap->address;
    resourceHeapBindInfo.heapRange.size      = heapSize;
    resourceHeapBindInfo.reservedRangeOffset = heapUserSize;
    resourceHeapBindInfo.reservedRangeSize   = heapProperties.minResourceHeapReservedRange;

    // Sampler heap with a single shared nearest sampler at slot 0, referenced by all
    // combined image samplers (their mappings use the default samplerHeapOffset = 0).
    std::unique_ptr<HeapBuffer> samplerHeap;
    VkBindHeapInfoEXT samplerHeapBindInfo = initVulkanStructure();
    if (usesSampler)
    {
        const VkDeviceSize samplerStride =
            alignUp(heapProperties.samplerDescriptorSize, heapProperties.samplerDescriptorAlignment);
        const VkDeviceSize samplerUserSize = alignUp(samplerStride, heapProperties.samplerHeapAlignment);
        const VkDeviceSize samplerHeapSize = samplerUserSize + heapProperties.minSamplerHeapReservedRange;

        samplerHeap = createHeapBuffer(vki, physicalDevice, vkd, device, samplerHeapSize,
                                       VK_BUFFER_USAGE_2_DESCRIPTOR_HEAP_BIT_EXT);

        VkSamplerCreateInfo samplerCreateInfo     = initVulkanStructure();
        samplerCreateInfo.magFilter               = VK_FILTER_NEAREST;
        samplerCreateInfo.minFilter               = VK_FILTER_NEAREST;
        samplerCreateInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerCreateInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCreateInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCreateInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCreateInfo.maxAnisotropy           = 1.0f;
        samplerCreateInfo.compareOp               = VK_COMPARE_OP_NEVER;
        samplerCreateInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;

        VkHostAddressRangeEXT samplerDst{};
        samplerDst.address = samplerHeap->allocation->getHostPtr();
        samplerDst.size    = static_cast<size_t>(samplerStride);
        VK_CHECK(vkd.writeSamplerDescriptorsEXT(device, 1u, &samplerCreateInfo, &samplerDst));
        flushAlloc(vkd, device, *samplerHeap->allocation);

        samplerHeapBindInfo.heapRange.address   = samplerHeap->address;
        samplerHeapBindInfo.heapRange.size      = samplerHeapSize;
        samplerHeapBindInfo.reservedRangeOffset = samplerUserSize;
        samplerHeapBindInfo.reservedRangeSize   = heapProperties.minSamplerHeapReservedRange;
    }

    const auto subresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

    const auto cmdPool      = makeCommandPool(vkd, device, queueFamilyIndex);
    const auto cmdBufferPtr = allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto cmdBuffer    = *cmdBufferPtr;

    beginCommandBuffer(vkd, cmdBuffer);

    // Transition every cleared image UNDEFINED -> TRANSFER_DST.
    {
        std::vector<VkImageMemoryBarrier> preClear;
        for (const auto &clear : imageClears)
            preClear.push_back(makeImageMemoryBarrier(VK_ACCESS_NONE, VK_ACCESS_TRANSFER_WRITE_BIT,
                                                      VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                      clear.image, subresourceRange));
        vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u,
                               nullptr, 0u, nullptr, static_cast<uint32_t>(preClear.size()), preClear.data());
    }

    for (const auto &clear : imageClears)
    {
        VkClearColorValue clearValue{};
        clearValue.int32[0] = clear.value;
        vkd.cmdClearColorImage(cmdBuffer, clear.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1u,
                               &subresourceRange);
    }

    // Clears (transfer writes) -> shader access, transitioning each image to its final layout.
    {
        // Input attachments (fragment only) are additionally read as INPUT_ATTACHMENT_READ.
        const VkAccessFlags readOnlyAccess =
            VK_ACCESS_SHADER_READ_BIT |
            ((data.stage == STAGE_FRAGMENT) ? (VkAccessFlags)VK_ACCESS_INPUT_ATTACHMENT_READ_BIT : (VkAccessFlags)0);

        std::vector<VkImageMemoryBarrier> postClear;
        for (const auto &clear : imageClears)
        {
            const VkAccessFlags dstAccess = (clear.finalLayout == VK_IMAGE_LAYOUT_GENERAL) ?
                                                (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT) :
                                                readOnlyAccess;
            postClear.push_back(makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, dstAccess,
                                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, clear.finalLayout,
                                                       clear.image, subresourceRange));
        }
        vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, shaderStage, 0u, 0u, nullptr, 0u, nullptr,
                               static_cast<uint32_t>(postClear.size()), postClear.data());
    }

    vkd.cmdBindResourceHeapEXT(cmdBuffer, &resourceHeapBindInfo);
    if (usesSampler)
        vkd.cmdBindSamplerHeapEXT(cmdBuffer, &samplerHeapBindInfo);

    // Push the identity index array consumed by unifindexed shaders (identity[i] == i).
    int32_t pushIdentity[32];
    for (int32_t i = 0; i < 32; ++i)
        pushIdentity[i] = i;
    if (data.indexType == INDEX_TYPE_PUSHCONSTANT)
    {
        VkPushDataInfoEXT pushDataInfo = initVulkanStructure();
        pushDataInfo.offset            = 0u;
        pushDataInfo.data.size         = sizeof(pushIdentity);
        pushDataInfo.data.address      = pushIdentity;
        vkd.cmdPushDataEXT(cmdBuffer, &pushDataInfo);
    }

    vkd.cmdBindPipeline(cmdBuffer, bindPoint, *pipeline);

    if (data.stage == STAGE_COMPUTE)
    {
        vkd.cmdDispatch(cmdBuffer, DIM, DIM, 1u);
    }
    else
    {
        beginRenderPass(vkd, cmdBuffer, *renderPass, *framebuffer, makeRect2D(0, 0, DIM, DIM));
        // One point per invocation for the vertex stage; a full-screen triangle for the fragment stage.
        vkd.cmdDraw(cmdBuffer, (data.stage == STAGE_VERTEX) ? (DIM * DIM) : 3u, 1u, 0u, 0u);
        endRenderPass(vkd, cmdBuffer);
    }

    // Shader writes -> transfer reads / host reads.
    {
        std::vector<VkImageMemoryBarrier> postDispatch;
        postDispatch.push_back(makeImageMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                                                      VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
                                                      *outputImage->image, subresourceRange));
        for (const auto *img : writeImages)
            postDispatch.push_back(makeImageMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                                                          VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, *img->image,
                                                          subresourceRange));
        const VkMemoryBarrier bufferBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        vkd.cmdPipelineBarrier(cmdBuffer, shaderStage, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_HOST_BIT, 0u,
                               1u, &bufferBarrier, 0u, nullptr, static_cast<uint32_t>(postDispatch.size()),
                               postDispatch.data());
    }

    // Copy output image.
    {
        VkBufferImageCopy region{};
        region.bufferOffset     = 0u;
        region.imageSubresource = makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
        region.imageExtent      = {DIM, DIM, 1u};
        vkd.cmdCopyImageToBuffer(cmdBuffer, *outputImage->image, VK_IMAGE_LAYOUT_GENERAL, *copyBuffer->buffer, 1u,
                                 &region);
    }

    // Copy each written storage image into the readback buffer.
    for (size_t i = 0; i < writeImages.size(); ++i)
    {
        VkBufferImageCopy region{};
        region.bufferOffset     = i * sizeof(int32_t);
        region.imageSubresource = makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
        region.imageExtent      = {1u, 1u, 1u};
        vkd.cmdCopyImageToBuffer(cmdBuffer, *writeImages[i]->image, VK_IMAGE_LAYOUT_GENERAL, *readbackBuffer->buffer,
                                 1u, &region);
    }

    // Transfer writes -> host reads.
    {
        const VkMemoryBarrier barrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &barrier,
                               0u, nullptr, 0u, nullptr);
    }

    endCommandBuffer(vkd, cmdBuffer);
    submitCommandsAndWait(vkd, device, queue, cmdBuffer);

    invalidateAlloc(vkd, device, *copyBuffer->allocation);
    invalidateAlloc(vkd, device, *readbackBuffer->allocation);
    invalidateAlloc(vkd, device, *backingBuffer->allocation);

    uint32_t failures = 0u;

    // Every output texel must be 1.
    {
        const auto outputPtr = reinterpret_cast<const int32_t *>(copyBuffer->allocation->getHostPtr());
        for (uint32_t i = 0; i < DIM * DIM; ++i)
            if (outputPtr[i] != 1)
                ++failures;
    }

    // Verify generated writes.
    {
        const auto readbackPtr = reinterpret_cast<const int32_t *>(readbackBuffer->allocation->getHostPtr());
        std::map<uint32_t, int32_t> imageWriteValues;
        for (size_t i = 0; i < writeImages.size(); ++i)
            imageWriteValues[writeImages[i]->descriptor] = readbackPtr[i];

        for (const auto &write : randomLayout.descriptorWrites)
        {
            if (!write.second.writeGenerated)
                continue;

            const DescriptorId &id = write.first;
            const uint32_t d       = bindingStart[id.set][id.binding] + id.number;
            const auto type        = randomLayout.layoutBindings[id.set][id.binding].descriptorType;

            int32_t got = 0;
            if (type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
            {
                const auto it = imageWriteValues.find(d);
                got           = (it != imageWriteValues.end()) ? it->second : -1;
            }
            else
            {
                got = *reinterpret_cast<const int32_t *>(backingHostPtr + d * valueAlign);
            }

            if (got != static_cast<int32_t>(d))
                ++failures;
        }
    }

    if (failures != 0u)
        return tcu::TestStatus::fail("Found " + de::toString(failures) + " incorrect values");

    return tcu::TestStatus::pass("Pass");
}

void populateDescriptorHeapRandomTests(tcu::TestCaseGroup *group)
{
    tcu::TestContext &testCtx = group->getTestContext();

    uint32_t seed = 0u;

    struct TestGroupCase
    {
        uint32_t count;
        const char *name;
    };

    const TestGroupCase setsCases[] = {
        {4, "sets4"},
        {8, "sets8"},
    };

    const TestGroupCase indexCases[] = {
        {INDEX_TYPE_NONE, "noarray"},
        {INDEX_TYPE_CONSTANT, "constant"},
        {INDEX_TYPE_PUSHCONSTANT, "unifindexed"},
        {INDEX_TYPE_DEPENDENT, "dynindexed"},
        {INDEX_TYPE_RUNTIME_SIZE, "runtimesize"},
    };

    const TestGroupCase uboCases[] = {
        {0, "noubo"},
        {12, "ubolimitlow"},
        {4096, "ubolimithigh"},
    };

    const TestGroupCase sboCases[] = {
        {0, "nosbo"},
        {4, "sbolimitlow"},
        {4096, "sbolimithigh"},
    };

    const TestGroupCase sampledImgCases[] = {
        {0, "nosampledimg"},
        {16, "sampledimglow"},
        {4096, "sampledimghigh"},
    };

    const struct
    {
        uint32_t sImgCount;
        uint32_t sTexCount;
        const char *name;
    } sImgTexCases[] = {
        {1, 0, "outimgonly"},
        {1, 3, "outimgtexlow"},
        {4, 0, "lowimgnotex"},
        {3, 1, "lowimgsingletex"},
    };

    const struct
    {
        Stage stage;
        const char *name;
    } stageCases[] = {
        {STAGE_COMPUTE, "comp"},
        {STAGE_VERTEX, "vert"},
        {STAGE_FRAGMENT, "frag"},
    };

    // Input attachments are fragment-only.
    const TestGroupCase iaCases[] = {
        {0, "noia"},
        {4, "ialimitlow"},
    };

    for (const auto &setsCase : setsCases)
    {
        de::MovePtr<tcu::TestCaseGroup> setsGroup(new tcu::TestCaseGroup(testCtx, setsCase.name));
        for (int indexNdx = 0; indexNdx < DE_LENGTH_OF_ARRAY(indexCases); ++indexNdx)
        {
            de::MovePtr<tcu::TestCaseGroup> indexGroup(new tcu::TestCaseGroup(testCtx, indexCases[indexNdx].name));
            for (int uboNdx = 0; uboNdx < DE_LENGTH_OF_ARRAY(uboCases); ++uboNdx)
            {
                de::MovePtr<tcu::TestCaseGroup> uboGroup(new tcu::TestCaseGroup(testCtx, uboCases[uboNdx].name));
                for (int sboNdx = 0; sboNdx < DE_LENGTH_OF_ARRAY(sboCases); ++sboNdx)
                {
                    de::MovePtr<tcu::TestCaseGroup> sboGroup(new tcu::TestCaseGroup(testCtx, sboCases[sboNdx].name));
                    for (int sampledImgNdx = 0; sampledImgNdx < DE_LENGTH_OF_ARRAY(sampledImgCases); ++sampledImgNdx)
                    {
                        de::MovePtr<tcu::TestCaseGroup> sampledImgGroup(
                            new tcu::TestCaseGroup(testCtx, sampledImgCases[sampledImgNdx].name));
                        for (int storageImgNdx = 0; storageImgNdx < DE_LENGTH_OF_ARRAY(sImgTexCases); ++storageImgNdx)
                        {
                            // Allow only one high limit or all of them.
                            uint32_t highLimitCount = 0u;
                            if (uboNdx == DE_LENGTH_OF_ARRAY(uboCases) - 1)
                                ++highLimitCount;
                            if (sboNdx == DE_LENGTH_OF_ARRAY(sboCases) - 1)
                                ++highLimitCount;
                            if (sampledImgNdx == DE_LENGTH_OF_ARRAY(sampledImgCases) - 1)
                                ++highLimitCount;
                            if (highLimitCount > 1 && highLimitCount < 3)
                                continue;

                            // Allow only none, one or all "zero limits" at the same time.
                            uint32_t zeroLimitCount = 0u;
                            if (uboNdx == 0)
                                ++zeroLimitCount;
                            if (sboNdx == 0)
                                ++zeroLimitCount;
                            if (sampledImgNdx == 0)
                                ++zeroLimitCount;
                            if (zeroLimitCount > 1 && zeroLimitCount < 3)
                                continue;

                            // Multiple storage images require dynamic indexing.
                            if (storageImgNdx >= 2 && indexNdx < 2)
                                continue;

                            // Skip zero ubo/sbo/sampled image when there is no dynamic indexing.
                            if ((uboNdx == 0 || sboNdx == 0 || sampledImgNdx == 0) && indexNdx < 2)
                                continue;

                            de::MovePtr<tcu::TestCaseGroup> storageImgGroup(
                                new tcu::TestCaseGroup(testCtx, sImgTexCases[storageImgNdx].name));

                            const uint32_t numSeeds =
                                (setsCase.count == 4 && uboNdx < 2 && sboNdx < 2 && sampledImgNdx < 2 &&
                                 (uboNdx != 0 || sboNdx != 0 || sampledImgNdx != 0 || storageImgNdx != 0)) ?
                                    3u :
                                    1u;

                            for (const auto &stageCase : stageCases)
                            {
                                de::MovePtr<tcu::TestCaseGroup> stageGroup(
                                    new tcu::TestCaseGroup(testCtx, stageCase.name));

                                for (int iaNdx = 0; iaNdx < DE_LENGTH_OF_ARRAY(iaCases); ++iaNdx)
                                {
                                    // Input attachments only exist in the fragment stage.
                                    if (iaCases[iaNdx].count > 0 && stageCase.stage != STAGE_FRAGMENT)
                                        continue;

                                    de::MovePtr<tcu::TestCaseGroup> iaGroup(
                                        new tcu::TestCaseGroup(testCtx, iaCases[iaNdx].name));

                                    for (uint32_t rnd = 0; rnd < numSeeds; ++rnd)
                                    {
                                        CaseDef c{};
                                        c.indexType         = static_cast<IndexType>(indexCases[indexNdx].count);
                                        c.numDescriptorSets = setsCase.count;
                                        c.maxPerStageUniformBuffers      = uboCases[uboNdx].count;
                                        c.maxPerStageStorageBuffers      = sboCases[sboNdx].count;
                                        c.maxPerStageSampledImages       = sampledImgCases[sampledImgNdx].count;
                                        c.maxPerStageStorageImages       = sImgTexCases[storageImgNdx].sImgCount;
                                        c.maxPerStageStorageTexelBuffers = sImgTexCases[storageImgNdx].sTexCount;
                                        c.maxPerStageInputAttachments    = iaCases[iaNdx].count;
                                        c.stage                          = stageCase.stage;
                                        c.seed                           = seed++;

                                        iaGroup->addChild(new DescriptorHeapRandomCase(testCtx, de::toString(rnd), c));
                                    }
                                    stageGroup->addChild(iaGroup.release());
                                }
                                storageImgGroup->addChild(stageGroup.release());
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
}

} // namespace

tcu::TestCaseGroup *createDescriptorHeapRandomTests(tcu::TestContext &testCtx)
{
    return createTestGroup(testCtx, "descriptor_heap_random", populateDescriptorHeapRandomTests);
}

} // namespace BindingModel
} // namespace vkt

#endif // CTS_USES_VULKANSC
