/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 Khronos Group
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
* \brief API Maintenance3 Check test - checks structs and function from VK_KHR_maintenance3
*//*--------------------------------------------------------------------*/

#include "tcuTestLog.hpp"

#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"

#include "vktApiMaintenance3Check.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"

#include <sstream>
#include <limits>
#include <utility>
#include <algorithm>
#include <map>
#include <set>

using namespace vk;

namespace vkt
{

namespace api
{

namespace
{
using ::std::make_pair;
using ::std::map;
using ::std::ostringstream;
using ::std::set;
using ::std::string;
using ::std::vector;

typedef vk::VkPhysicalDeviceProperties DevProp1;
typedef vk::VkPhysicalDeviceProperties2 DevProp2;
typedef vk::VkPhysicalDeviceMaintenance3Properties MaintDevProp3;
typedef vk::VkPhysicalDeviceFeatures2 DevFeat2;
#ifndef CTS_USES_VULKANSC
typedef vk::VkPhysicalDeviceInlineUniformBlockFeaturesEXT DevIubFeat;
typedef vk::VkPhysicalDeviceInlineUniformBlockPropertiesEXT DevIubProp;
#endif // CTS_USES_VULKANSC

// These variables are equal to minimal values for maxMemoryAllocationSize and maxPerSetDescriptors
constexpr uint32_t maxMemoryAllocationSize = 1073741824u;
constexpr uint32_t maxDescriptorsInSet     = 1024u;
#ifndef CTS_USES_VULKANSC
constexpr uint32_t maxReasonableInlineUniformBlocks = 64u;
#else
constexpr uint32_t maxReasonableBindingCounts = 1024u;
#endif // CTS_USES_VULKANSC
using TypeSet = set<vk::VkDescriptorType>;

// Structure representing an implementation limit, like maxPerStageDescriptorSamplers. It has a maximum value
// obtained at runtime and a remaining number of descriptors, which starts with the same count and decreases
// as we assign descriptor counts to the different types. A limit is affected by (or itself affects) one or more
// descriptor types. Note a type may be involved in several limits, and a limit may affect several types.
struct Limit
{
    Limit(const string &name_, uint32_t maxValue_, const TypeSet &affectedTypes_)
        : name(name_)
        , maxValue(maxValue_)
        , remaining(maxValue_)
        , affectedTypes(affectedTypes_)
    {
    }

    const string name;
    const uint32_t maxValue;
    uint32_t remaining;
    const TypeSet affectedTypes;
};

// Structure representing how many descriptors have been assigned to the given type. The type is "alive" during
// descriptor count assignment if more descriptors can be added to the type without hitting any limit affected
// by the type. Once at least one of the limits is reached, no more descriptors can be assigned to the type and
// the type is no longer considered "alive".
struct TypeState
{
    TypeState(vk::VkDescriptorType type_) : type(type_), alive(true), count(0u)
    {
    }

    const vk::VkDescriptorType type;
    bool alive;
    uint32_t count;
};

using TypeCounts   = map<vk::VkDescriptorType, TypeState>;
using LimitsVector = vector<Limit>;

// Get the subset of alive types from the given map.
TypeSet getAliveTypes(const TypeCounts &typeCounts)
{
    TypeSet aliveTypes;
    for (const auto &typeCount : typeCounts)
    {
        if (typeCount.second.alive)
            aliveTypes.insert(typeCount.first);
    }
    return aliveTypes;
}

// Get the subset of alive types for a specific limit, among the set of types affected by the limit.
TypeSet getAliveTypesForLimit(const Limit &limit, const TypeSet &aliveTypes)
{
    TypeSet subset;
    for (const auto &type : limit.affectedTypes)
    {
        if (aliveTypes.find(type) != aliveTypes.end())
            subset.insert(type);
    }
    return subset;
}

// Distribute descriptor counts as evenly as possible among the given set of types, taking into account the
// given limits.
void distributeCounts(LimitsVector &limits, TypeCounts &typeCounts)
{
    using IncrementsMap = map<vk::VkDescriptorType, uint32_t>;
    TypeSet aliveTypes;

    while ((aliveTypes = getAliveTypes(typeCounts)).size() > 0u)
    {
        // Calculate the maximum increment per alive descriptor type. This involves iterating over the limits and
        // finding out how many more descriptors can be distributed among the affected types that are still alive
        // for the limit. For each type, remember the lowest possible increment.
        IncrementsMap increments;
        for (const auto &type : aliveTypes)
            increments[type] = std::numeric_limits<uint32_t>::max();

        TypeSet aliveTypesForLimit;

        for (const auto &limit : limits)
        {
            if (limit.remaining == 0u)
                continue;

            aliveTypesForLimit = getAliveTypesForLimit(limit, aliveTypes);
            if (aliveTypesForLimit.empty())
                continue;

            // Distribute remaining count evenly among alive types.
            uint32_t maxIncrement = limit.remaining / static_cast<uint32_t>(aliveTypesForLimit.size());
            if (maxIncrement == 0u)
            {
                // More types than remaining descriptors. Assign 1 to the first affected types and 0 to the rest.
                uint32_t remaining = limit.remaining;
                for (const auto &type : aliveTypesForLimit)
                {
                    if (remaining > 0u && increments[type] > 0u)
                    {
                        increments[type] = 1u;
                        --remaining;
                    }
                    else
                    {
                        increments[type] = 0u;
                    }
                }
            }
            else
            {
                // Find the lowest possible increment taking into account all limits.
                for (const auto &type : aliveTypesForLimit)
                {
                    if (increments[type] > maxIncrement)
                        increments[type] = maxIncrement;
                }
            }
        }

        // Apply the calculated increments per descriptor type, decreasing the remaining descriptors for each
        // limit affected by the type, and switching types to the not-alive state when a limit is hit.
        for (const auto &inc : increments)
        {
            const vk::VkDescriptorType &type = inc.first;
            const uint32_t &increment        = inc.second;

            // Increase type count.
            auto iter = typeCounts.find(type);
            DE_ASSERT(iter != typeCounts.end());
            iter->second.count += increment;

            for (auto &limit : limits)
            {
                // Decrease remaining descriptors for affected limits.
                if (limit.affectedTypes.find(type) != limit.affectedTypes.end())
                {
                    DE_ASSERT(increment <= limit.remaining);
                    limit.remaining -= increment;
                }
                if (limit.remaining == 0u)
                {
                    // Limit hit, switch affected types to not-alive.
                    for (const auto &affectedType : limit.affectedTypes)
                    {
                        auto tc = typeCounts.find(affectedType);
                        if (tc != typeCounts.end())
                            tc->second.alive = false;
                    }
                }
            }
        }
    }
}

// Create a limits vector based on runtime limit information for the device.
LimitsVector buildLimitsVector(const DevProp1 &prop1,
#ifndef CTS_USES_VULKANSC
                               const DevIubProp &iubProp,
#endif // CTS_USES_VULKANSC
                               const MaintDevProp3 &maintProp3)
{
    static const TypeSet samplerTypes = {vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, vk::VK_DESCRIPTOR_TYPE_SAMPLER};
    static const TypeSet sampledImageTypes    = {vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                 vk::VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                                 vk::VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER};
    static const TypeSet uniformBufferTypes   = {vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                 vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC};
    static const TypeSet storageBufferTypes   = {vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                 vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC};
    static const TypeSet storageImageTypes    = {vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                 vk::VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER};
    static const TypeSet inputAttachmentTypes = {vk::VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT};
#ifndef CTS_USES_VULKANSC
    static const TypeSet inlineUniformBlockTypes = {vk::VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT};
#endif // CTS_USES_VULKANSC
    static const TypeSet dynamicUniformBuffer = {vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC};
    static const TypeSet dynamicStorageBuffer = {vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC};
    static const TypeSet allTypesButIUB       = {
        vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, vk::VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          vk::VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
        vk::VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, vk::VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
    };
    static const TypeSet allTypes = {
        vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        vk::VK_DESCRIPTOR_TYPE_SAMPLER,
        vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
        vk::VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        vk::VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
        vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        vk::VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
        vk::VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
#ifndef CTS_USES_VULKANSC
        vk::VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT,
#endif // CTS_USES_VULKANSC
    };

    LimitsVector limits = {
        {"maxPerStageDescriptorSamplers", prop1.limits.maxPerStageDescriptorSamplers, samplerTypes},
        {"maxDescriptorSetSamplers", prop1.limits.maxDescriptorSetSamplers, samplerTypes},
        {"maxPerStageDescriptorSampledImages", prop1.limits.maxPerStageDescriptorSampledImages, sampledImageTypes},
        {"maxDescriptorSetSampledImages", prop1.limits.maxDescriptorSetSampledImages, sampledImageTypes},
        {"maxPerStageDescriptorUniformBuffers", prop1.limits.maxPerStageDescriptorUniformBuffers, uniformBufferTypes},
        {"maxDescriptorSetUniformBuffers", prop1.limits.maxDescriptorSetUniformBuffers, uniformBufferTypes},
        {"maxPerStageDescriptorStorageBuffers", prop1.limits.maxPerStageDescriptorStorageBuffers, storageBufferTypes},
        {"maxDescriptorSetStorageBuffers", prop1.limits.maxDescriptorSetStorageBuffers, storageBufferTypes},
        {"maxPerStageDescriptorStorageImages", prop1.limits.maxPerStageDescriptorStorageImages, storageImageTypes},
        {"maxDescriptorSetStorageImages", prop1.limits.maxDescriptorSetStorageImages, storageImageTypes},
        {"maxPerStageDescriptorInputAttachments", prop1.limits.maxPerStageDescriptorInputAttachments,
         inputAttachmentTypes},
        {"maxDescriptorSetInputAttachments", prop1.limits.maxDescriptorSetInputAttachments, inputAttachmentTypes},
        {"maxDescriptorSetUniformBuffersDynamic", prop1.limits.maxDescriptorSetUniformBuffersDynamic,
         dynamicUniformBuffer},
        {"maxDescriptorSetStorageBuffersDynamic", prop1.limits.maxDescriptorSetStorageBuffersDynamic,
         dynamicStorageBuffer},
#ifndef CTS_USES_VULKANSC
        // Removed from Vulkan SC test set: VK_EXT_inline_uniform_block extension removed from Vulkan SC
        {"maxPerStageDescriptorInlineUniformBlocks", iubProp.maxPerStageDescriptorInlineUniformBlocks,
         inlineUniformBlockTypes},
        {"maxDescriptorSetInlineUniformBlocks", iubProp.maxDescriptorSetInlineUniformBlocks, inlineUniformBlockTypes},
#endif // CTS_USES_VULKANSC
        {"maxPerStageResources", prop1.limits.maxPerStageResources, allTypesButIUB},
        {"maxPerSetDescriptors", maintProp3.maxPerSetDescriptors, allTypes},
    };

    return limits;
}

// Create a vector of bindings by constructing the system limits and distributing descriptor counts.
vector<vk::VkDescriptorSetLayoutBinding> calculateBindings(const DevProp1 &prop1,
#ifndef CTS_USES_VULKANSC
                                                           const DevIubProp &iubProp,
#endif // CTS_USES_VULKANSC
                                                           const MaintDevProp3 &maintProp3,
                                                           const vector<vk::VkDescriptorType> &types)
{
    LimitsVector limits = buildLimitsVector(prop1,
#ifndef CTS_USES_VULKANSC
                                            iubProp,
#endif // CTS_USES_VULKANSC
                                            maintProp3);

    TypeCounts typeCounts;

    for (const auto &type : types)
        typeCounts.emplace(make_pair(type, TypeState(type)));

    distributeCounts(limits, typeCounts);

#ifdef CTS_USES_VULKANSC
    // limit the number of binding counts, so that descriptorSetLayoutBindingRequestCount and descriptorSetLayoutBindingLimit won't be too big
    for (auto &tc : typeCounts)
        tc.second.count = de::min(tc.second.count, maxReasonableBindingCounts);
#endif // CTS_USES_VULKANSC

    uint32_t bindingNumber = 0u;
    vector<vk::VkDescriptorSetLayoutBinding> bindings;
    for (const auto &tc : typeCounts)
    {
#ifndef CTS_USES_VULKANSC
        if (tc.first != VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
#else
        if (true)
#endif // CTS_USES_VULKANSC
        {
            vk::VkDescriptorSetLayoutBinding b;
            b.binding            = bindingNumber;
            b.descriptorCount    = tc.second.count;
            b.descriptorType     = tc.first;
            b.pImmutableSamplers = nullptr;
            b.stageFlags         = tc.first == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT ? vk::VK_SHADER_STAGE_FRAGMENT_BIT :
                                                                                     vk::VK_SHADER_STAGE_ALL;

            bindings.push_back(b);
        }
        else
        {
            // Inline uniform blocks are special because descriptorCount represents the size of that block.
            // The only way of creating several blocks is by adding more structures to the list instead of creating an array.
            size_t firstAdded = bindings.size();
            bindings.resize(firstAdded + tc.second.count);
            for (uint32_t i = 0u; i < tc.second.count; ++i)
            {
                vk::VkDescriptorSetLayoutBinding &b = bindings[firstAdded + i];
                b.binding                           = bindingNumber + i;
                b.descriptorCount =
                    4u; // For inline uniform blocks, this must be a multiple of 4 according to the spec.
                b.descriptorType     = tc.first;
                b.pImmutableSamplers = nullptr;
                b.stageFlags         = vk::VK_SHADER_STAGE_ALL;
            }
        }
        bindingNumber += tc.second.count;
    }

    return bindings;
}

// Get a textual description with descriptor counts per type.
string getBindingsDescription(const vector<VkDescriptorSetLayoutBinding> &bindings)
{
    map<vk::VkDescriptorType, uint32_t> typeCount;
    uint32_t totalCount = 0u;
    uint32_t count;
    for (const auto &b : bindings)
    {
        auto iter = typeCount.find(b.descriptorType);
        if (iter == typeCount.end())
            iter = typeCount.insert(make_pair(b.descriptorType, (uint32_t)0)).first;
#ifndef CTS_USES_VULKANSC
        count = ((b.descriptorType == vk::VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT) ? 1u : b.descriptorCount);
#else
        count = b.descriptorCount;
#endif // CTS_USES_VULKANSC
        iter->second += count;
        totalCount += count;
    }

    uint32_t i = 0;
    ostringstream combStr;

    combStr << "{ Descriptors: " << totalCount << ", [";
    for (const auto &tc : typeCount)
        combStr << (i++ ? ", " : " ") << tc.first << ": " << tc.second;
    combStr << " ] }";

    return combStr.str();
}

class Maintenance3StructTestInstance : public TestInstance
{
public:
    Maintenance3StructTestInstance(Context &ctx) : TestInstance(ctx)
    {
    }
    virtual tcu::TestStatus iterate(void)
    {
        tcu::TestLog &log = m_context.getTestContext().getLog();

        // set values to be a bit smaller than required minimum values
        MaintDevProp3 maintProp3 = {
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES, //VkStructureType sType;
            nullptr,                                                    //void* pNext;
            maxDescriptorsInSet - 1u,                                   //uint32_t maxPerSetDescriptors;
            maxMemoryAllocationSize - 1u                                //VkDeviceSize maxMemoryAllocationSize;
        };

        DevProp2 prop2;
        deMemset(&prop2, 0, sizeof(prop2)); // zero the structure
        prop2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        prop2.pNext = &maintProp3;

        m_context.getInstanceInterface().getPhysicalDeviceProperties2(m_context.getPhysicalDevice(), &prop2);

        if (maintProp3.maxMemoryAllocationSize < maxMemoryAllocationSize)
            return tcu::TestStatus::fail("Fail");

        if (maintProp3.maxPerSetDescriptors < maxDescriptorsInSet)
            return tcu::TestStatus::fail("Fail");

        log << tcu::TestLog::Message << "maxMemoryAllocationSize: " << maintProp3.maxMemoryAllocationSize
            << tcu::TestLog::EndMessage;
        log << tcu::TestLog::Message << "maxPerSetDescriptors: " << maintProp3.maxPerSetDescriptors
            << tcu::TestLog::EndMessage;
        return tcu::TestStatus::pass("Pass");
    }
};

class Maintenance3StructTestCase : public TestCase
{
public:
    Maintenance3StructTestCase(tcu::TestContext &testCtx) : TestCase(testCtx, "maintenance3_properties")
    {
    }

    virtual ~Maintenance3StructTestCase(void)
    {
    }
    virtual void checkSupport(Context &ctx) const
    {
        ctx.requireDeviceFunctionality("VK_KHR_maintenance3");
    }
    virtual TestInstance *createInstance(Context &ctx) const
    {
        return new Maintenance3StructTestInstance(ctx);
    }

private:
};

class Maintenance3DescriptorTestInstance : public TestInstance
{
public:
    Maintenance3DescriptorTestInstance(Context &ctx) : TestInstance(ctx)
    {
    }
    virtual tcu::TestStatus iterate(void)
    {
        const auto &vki            = m_context.getInstanceInterface();
        const auto &vkd            = m_context.getDeviceInterface();
        const auto &physicalDevice = m_context.getPhysicalDevice();
        const auto &device         = m_context.getDevice();
        auto &log                  = m_context.getTestContext().getLog();

#ifndef CTS_USES_VULKANSC
        bool iubSupported = false;

        if (m_context.isDeviceFunctionalitySupported("VK_EXT_inline_uniform_block"))
        {
            DevIubFeat iubFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_FEATURES_EXT, nullptr, 0u,
                                      0u};

            DevFeat2 features2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &iubFeatures,
                                  VkPhysicalDeviceFeatures()};

            vki.getPhysicalDeviceFeatures2(physicalDevice, &features2);
            iubSupported = (iubFeatures.inlineUniformBlock == VK_TRUE);
        }

        DevIubProp devIubProp = {
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_PROPERTIES_EXT, // VkStructureType sType;
            nullptr,                                                               // void* pNext;
            0u, // uint32_t maxInlineUniformBlockSize;
            0u, // uint32_t maxPerStageDescriptorInlineUniformBlocks;
            0u, // uint32_t maxPerStageDescriptorUpdateAfterBindInlineUniformBlocks;
            0u, // uint32_t maxDescriptorSetInlineUniformBlocks;
            0u  // uint32_t maxDescriptorSetUpdateAfterBindInlineUniformBlocks;
        };
#endif // CTS_USES_VULKANSC

        MaintDevProp3 maintProp3 = {
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES, //VkStructureType sType;
#ifndef CTS_USES_VULKANSC
            (iubSupported ? &devIubProp : nullptr), //void* pNext;
#else
            nullptr, //void* pNext;
#endif                              // CTS_USES_VULKANSC
            maxDescriptorsInSet,    //uint32_t maxPerSetDescriptors;
            maxMemoryAllocationSize //VkDeviceSize maxMemoryAllocationSize;
        };

        DevProp2 prop2 = {
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, //VkStructureType sType;
            &maintProp3,                                    //void* pNext;
            VkPhysicalDeviceProperties()                    //VkPhysicalDeviceProperties properties;
        };

        vki.getPhysicalDeviceProperties2(physicalDevice, &prop2);

        vector<VkDescriptorType> descriptorTypes = {
            VK_DESCRIPTOR_TYPE_SAMPLER,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
            VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
            VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
        };
#ifndef CTS_USES_VULKANSC
        if (iubSupported)
            descriptorTypes.push_back(VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT);
#endif // CTS_USES_VULKANSC

        // VkDescriptorSetLayoutCreateInfo setup
        vk::VkDescriptorSetLayoutCreateInfo pCreateInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, //VkStructureType sType;
            nullptr,                                             //const void* pNext;
            0u,                                                  //VkDescriptorSetLayoutCreateFlags flags;
            0u,                                                  //uint32_t bindingCount;
            nullptr                                              //const VkDescriptorSetLayoutBinding* pBindings;
        };

        // VkDescriptorSetLayoutSupport setup
        vk::VkDescriptorSetLayoutSupport pSupport = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_SUPPORT, //VkStructureType sType;
            nullptr,                                         //void* pNext;
            VK_FALSE                                         //VkBool32 supported;
        };

        // Check every combination maximizing descriptor counts.
        for (size_t combSize = 1; combSize <= descriptorTypes.size(); ++combSize)
        {
            // Create a vector of selectors with combSize elements set to true.
            vector<bool> selectors(descriptorTypes.size(), false);
            std::fill(begin(selectors), begin(selectors) + combSize, true);

            // Iterate over every permutation of selectors for that combination size.
            do
            {
                vector<vk::VkDescriptorType> types;
                for (size_t i = 0; i < selectors.size(); ++i)
                {
                    if (selectors[i])
                        types.push_back(descriptorTypes[i]);
                }

#ifndef CTS_USES_VULKANSC
                // Due to inline uniform blocks being unable to form arrays and each one of them needing its own
                // VkDescriptorSetLayoutBinding structure, we will limit when to test them.
                if (std::find(begin(types), end(types), VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT) != types.end() &&
                    devIubProp.maxPerStageDescriptorInlineUniformBlocks > maxReasonableInlineUniformBlocks &&
                    combSize > 1u && combSize < descriptorTypes.size())
                {
                    continue;
                }
#endif // CTS_USES_VULKANSC

                vector<vk::VkDescriptorSetLayoutBinding> bindings = calculateBindings(prop2.properties,
#ifndef CTS_USES_VULKANSC
                                                                                      devIubProp,
#endif
                                                                                      maintProp3, types);
                string description = getBindingsDescription(bindings);
                log << tcu::TestLog::Message << "Testing combination: " << description << tcu::TestLog::EndMessage;

                pCreateInfo.bindingCount = static_cast<uint32_t>(bindings.size());
                pCreateInfo.pBindings    = bindings.data();

                vkd.getDescriptorSetLayoutSupport(device, &pCreateInfo, &pSupport);
                if (pSupport.supported == VK_FALSE)
                {
                    ostringstream msg;
                    msg << "Failed to use the following descriptor type counts: " << description;
                    return tcu::TestStatus::fail(msg.str());
                }
            } while (std::prev_permutation(begin(selectors), end(selectors)));
        }

        return tcu::TestStatus::pass("Pass");
    }
};

class Maintenance3DescriptorTestCase : public TestCase
{

public:
    Maintenance3DescriptorTestCase(tcu::TestContext &testCtx) : TestCase(testCtx, "descriptor_set")
    {
    }
    virtual ~Maintenance3DescriptorTestCase(void)
    {
    }
    virtual void checkSupport(Context &ctx) const
    {
        ctx.requireDeviceFunctionality("VK_KHR_maintenance3");
    }
    virtual TestInstance *createInstance(Context &ctx) const
    {
        return new Maintenance3DescriptorTestInstance(ctx);
    }
};

struct CountLayoutSupportParams
{
    const VkDescriptorType descriptorType;
    const uint32_t bindingOffset; // Apply an offset to each binding number.
    const bool extraBindings;
    const bool useVariableSize;
    const bool createLayout; // Actually attempt to create the layout.
};

void checkSupportCountLayoutSupport(Context &context, CountLayoutSupportParams params)
{
    context.requireDeviceFunctionality("VK_KHR_maintenance3");
    context.requireDeviceFunctionality("VK_EXT_descriptor_indexing");

    if (params.useVariableSize)
    {
        const auto &indexingFeatures = context.getDescriptorIndexingFeatures();
        if (!indexingFeatures.descriptorBindingVariableDescriptorCount)
            TCU_THROW(NotSupportedError, "descriptorBindingVariableDescriptorCount not supported");
    }

#ifndef CTS_USES_VULKANSC
    if (params.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
        context.requireDeviceFunctionality("VK_EXT_inline_uniform_block");
#endif
}

struct SetLayoutSupportAndCount
{
    const bool supported;
    const uint32_t maxVariableDescriptorCount;
};

SetLayoutSupportAndCount getSetLayoutSupportAndCount(Context &context,
                                                     const VkDescriptorSetLayoutCreateInfo *setLayoutCreateInfo)
{
    VkDescriptorSetVariableDescriptorCountLayoutSupport countLayoutSupport = initVulkanStructure();
    VkDescriptorSetLayoutSupport setLayoutSupport                          = initVulkanStructure(&countLayoutSupport);

    // Set a garbage value in the maxVariableDescriptorCount member, to verify it's not simply left untouched by the implementation.
    countLayoutSupport.maxVariableDescriptorCount = std::numeric_limits<uint32_t>::max();
    context.getDeviceInterface().getDescriptorSetLayoutSupport(context.getDevice(), setLayoutCreateInfo,
                                                               &setLayoutSupport);
    return SetLayoutSupportAndCount{(setLayoutSupport.supported == VK_TRUE),
                                    countLayoutSupport.maxVariableDescriptorCount};
}

tcu::TestStatus testCountLayoutSupport(Context &context, CountLayoutSupportParams params)
{
    VkShaderStageFlags stages = 0u;

    // The shader stages are probably not very relevant. This is an attempt at setting some varied but plausible stages anyway.
    switch (params.descriptorType)
    {
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        stages = VK_SHADER_STAGE_COMPUTE_BIT;
        break;

    case VK_DESCRIPTOR_TYPE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        stages = VK_SHADER_STAGE_FRAGMENT_BIT;
        break;

    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
#ifndef CTS_USES_VULKANSC
    case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK:
#endif
        stages = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
        break;

    default:
        DE_ASSERT(false);
        break;
    };

    std::vector<VkDescriptorSetLayoutBinding> bindings;
    std::vector<VkDescriptorBindingFlags> bindingFlags;

    const auto &offset = params.bindingOffset;

    if (params.extraBindings)
    {
        // Add a few uniform buffers to the mix.
        const auto extraBindingCount = 3u;

        for (uint32_t i = 0u; i < extraBindingCount; ++i)
        {
            bindings.emplace_back(VkDescriptorSetLayoutBinding{
                de::sizeU32(bindings) + offset,    // uint32_t binding;
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // VkDescriptorType descriptorType;
                1u,                                // uint32_t descriptorCount;
                stages,                            // VkShaderStageFlags stageFlags;
                nullptr,                           // const VkSampler* pImmutableSamplers;
            });
            bindingFlags.push_back(0u);
        }
    }

#ifndef CTS_USES_VULKANSC
    uint32_t maxInlineUniformBlockSize = 256;
    if (params.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
    {
        DevIubProp inlineUniformProps = {
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_PROPERTIES, nullptr, 0, 0, 0, 0, 0};

        VkPhysicalDeviceProperties2 props2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &inlineUniformProps, {}};

        context.getInstanceInterface().getPhysicalDeviceProperties2(context.getPhysicalDevice(), &props2);
        maxInlineUniformBlockSize = inlineUniformProps.maxInlineUniformBlockSize;
    }
#endif // CTS_USES_VULKANSC

    // VUID-VkDescriptorSetLayoutBinding-descriptorType-02209 mandates descriptorCount to be a multiple of 4 when using inline
    // uniform blocks.
    // VUID-VkDescriptorSetLayoutBinding-descriptorType-08004 descriptorCount must be less than or equal
    // to VkPhysicalDeviceInlineUniformBlockProperties::maxInlineUniformBlockSize
    uint32_t descriptorCount;
#ifndef CTS_USES_VULKANSC
    if (params.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
    {
        descriptorCount = std::min(4u, maxInlineUniformBlockSize);
    }
    else
#endif
    {
        descriptorCount = 1u;
    }

    bindings.emplace_back(VkDescriptorSetLayoutBinding{
        de::sizeU32(bindings) + offset, // uint32_t binding;
        params.descriptorType,          // VkDescriptorType descriptorType;
        descriptorCount,                // uint32_t descriptorCount;
        stages,                         // VkShaderStageFlags stageFlags;
        nullptr,                        // const VkSampler* pImmutableSamplers;
    });
    bindingFlags.push_back(params.useVariableSize ? static_cast<VkDescriptorBindingFlags>(
                                                        VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT) :
                                                    0u);

    const VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                           // const void* pNext;
        de::sizeU32(bindingFlags),                                         // uint32_t bindingCount;
        de::dataOrNull(bindingFlags), // const VkDescriptorBindingFlags* pBindingFlags;
    };

    const VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, // VkStructureType                        sType;
        &bindingFlagsInfo,                                   // const void*                            pNext;
        0u,                                                  // VkDescriptorSetLayoutCreateFlags       flags;
        de::sizeU32(bindings),                               // uint32_t                               bindingCount;
        de::dataOrNull(bindings),                            // const VkDescriptorSetLayoutBinding*    pBindings;
    };

    // Check the layout is supported.
    const auto normalValues = getSetLayoutSupportAndCount(context, &layoutCreateInfo);
    if (!normalValues.supported)
        TCU_THROW(NotSupportedError, "Set layout not supported");

    if (!params.useVariableSize)
    {
        if (normalValues.maxVariableDescriptorCount != 0u)
            TCU_FAIL("Nonzero maxVariableDescriptorCount when using no variable descriptor counts");
    }
    else
    {
#ifndef CTS_USES_VULKANSC
        // Check that the reported number makes sense.
        if (params.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
        {
            const auto &iubProperties = context.getInlineUniformBlockProperties();
            if (normalValues.maxVariableDescriptorCount > iubProperties.maxInlineUniformBlockSize)
                TCU_FAIL("Implementation supports a max IUB size larger than maxInlineUniformBlockSize");

            // The number needs to make sense for VUID-VkDescriptorSetLayoutBinding-descriptorType-02209 too.
            if (normalValues.maxVariableDescriptorCount % 4u != 0u)
                TCU_FAIL("Implementation reports a max IUB size that is not a multiple of 4");
        }
#endif

        // Verify if we switch from one to zero descriptors we get the same reply back.
        bindings.back().descriptorCount = 0u;
        const auto zeroDescriptorValues = getSetLayoutSupportAndCount(context, &layoutCreateInfo);

        if (!zeroDescriptorValues.supported)
            TCU_FAIL("Implementation reports support with one descriptor and no support with zero descriptors");

        if (zeroDescriptorValues.maxVariableDescriptorCount != normalValues.maxVariableDescriptorCount)
            TCU_FAIL("Mismatch in maxVariableDescriptorCount when using zero and one as descriptor counts");

        // Verify we can create a descriptor set with the promised amount of descriptors.
        bindings.back().descriptorCount = normalValues.maxVariableDescriptorCount;
        const auto maxDescriptorValues  = getSetLayoutSupportAndCount(context, &layoutCreateInfo);

        if (!maxDescriptorValues.supported)
            TCU_FAIL("Implementation reports no support when using the maximum allowed size");

        if (maxDescriptorValues.maxVariableDescriptorCount != normalValues.maxVariableDescriptorCount)
            TCU_FAIL("Mismatch in maxVariableDescriptorCount when using one and the maximum descriptor counts");
    }

    if (params.createLayout)
    {
        const auto ctx                  = context.getContextCommonData();
        VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;

        // This should work.
        VK_CHECK(ctx.vkd.createDescriptorSetLayout(ctx.device, &layoutCreateInfo, nullptr, &setLayout));
        if (setLayout != VK_NULL_HANDLE)
            ctx.vkd.destroyDescriptorSetLayout(ctx.device, setLayout, nullptr);
    }

    return tcu::TestStatus::pass("Pass");
}

std::string getDescriptorTypeShortName(const VkDescriptorType descType)
{
    static const auto prefixLen = strlen("VK_DESCRIPTOR_TYPE_");
    std::string name            = getDescriptorTypeName(descType);

    name = name.substr(prefixLen);
    return de::toLower(name);
}

bool hasDescriptorIndexingStructs(const Context &ctx)
{
    return ctx.getEquivalentApiVersion() >= VK_API_VERSION_1_2 ||
           ctx.isDeviceFunctionalitySupported("VK_EXT_descriptor_indexing");
}

#ifndef CTS_USES_VULKANSC
bool hasMutableDescriptorTypeSupport(const Context &ctx)
{
    return (ctx.isDeviceFunctionalitySupported("VK_EXT_mutable_descriptor_type") ||
            ctx.isDeviceFunctionalitySupported("VK_VALVE_mutable_descriptor_type")) &&
           (ctx.getMutableDescriptorTypeFeaturesEXT().mutableDescriptorType == VK_TRUE);
}
#endif // CTS_USES_VULKANSC

VkShaderStageFlags getValidStagesForType(VkDescriptorType descriptorType)
{
    switch (descriptorType)
    {
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        return VK_SHADER_STAGE_FRAGMENT_BIT;

    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
#ifndef CTS_USES_VULKANSC
    case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
#endif // CTS_USES_VULKANSC
        return VK_SHADER_STAGE_COMPUTE_BIT;

    case VK_DESCRIPTOR_TYPE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        return VK_SHADER_STAGE_FRAGMENT_BIT;

    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
#ifndef CTS_USES_VULKANSC
    case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK:
    case VK_DESCRIPTOR_TYPE_MUTABLE_EXT:
#endif // CTS_USES_VULKANSC
        return (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    default:
        DE_ASSERT(false);
        return VK_SHADER_STAGE_FRAGMENT_BIT;
    }
}

bool isUpdateAfterBindCapableType(VkDescriptorType descriptorType)
{
    return descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC &&
           descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC &&
           descriptorType != VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
}

void requireDescriptorTypeSupport(Context &context, VkDescriptorType descriptorType)
{
#ifndef CTS_USES_VULKANSC
    if (descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
    {
        context.requireDeviceFunctionality("VK_EXT_inline_uniform_block");
        if (!context.getInlineUniformBlockFeatures().inlineUniformBlock)
            TCU_THROW(NotSupportedError, "inlineUniformBlock feature not supported");
        if (context.getInlineUniformBlockProperties().maxInlineUniformBlockSize < 4u)
            TCU_THROW(NotSupportedError, "maxInlineUniformBlockSize < 4");
    }
    else if (descriptorType == VK_DESCRIPTOR_TYPE_MUTABLE_EXT)
    {
        if (!hasMutableDescriptorTypeSupport(context))
            TCU_THROW(NotSupportedError, "VK_EXT_mutable_descriptor_type feature not supported");
    }
    else if (descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
    {
        context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
        if (!context.getAccelerationStructureFeatures().accelerationStructure)
            TCU_THROW(NotSupportedError, "accelerationStructure feature not supported");
    }
#else
    DE_UNREF(context);
    DE_UNREF(descriptorType);
#endif // CTS_USES_VULKANSC
}

void requireUpdateAfterBindFeature(Context &context, VkDescriptorType descriptorType)
{
    const auto &indexingFeatures = context.getDescriptorIndexingFeatures();

    switch (descriptorType)
    {
    case VK_DESCRIPTOR_TYPE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        if (!indexingFeatures.descriptorBindingSampledImageUpdateAfterBind)
            TCU_THROW(NotSupportedError, "descriptorBindingSampledImageUpdateAfterBind not supported");
        break;

    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        if (!indexingFeatures.descriptorBindingStorageImageUpdateAfterBind)
            TCU_THROW(NotSupportedError, "descriptorBindingStorageImageUpdateAfterBind not supported");
        break;

    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        if (!indexingFeatures.descriptorBindingUniformTexelBufferUpdateAfterBind)
            TCU_THROW(NotSupportedError, "descriptorBindingUniformTexelBufferUpdateAfterBind not supported");
        break;

    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        if (!indexingFeatures.descriptorBindingStorageTexelBufferUpdateAfterBind)
            TCU_THROW(NotSupportedError, "descriptorBindingStorageTexelBufferUpdateAfterBind not supported");
        break;

    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        if (!indexingFeatures.descriptorBindingUniformBufferUpdateAfterBind)
            TCU_THROW(NotSupportedError, "descriptorBindingUniformBufferUpdateAfterBind not supported");
        break;

    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        if (!indexingFeatures.descriptorBindingStorageBufferUpdateAfterBind)
            TCU_THROW(NotSupportedError, "descriptorBindingStorageBufferUpdateAfterBind not supported");
        break;

#ifndef CTS_USES_VULKANSC
    case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK:
        if (!context.getInlineUniformBlockFeatures().descriptorBindingInlineUniformBlockUpdateAfterBind)
            TCU_THROW(NotSupportedError, "descriptorBindingInlineUniformBlockUpdateAfterBind not supported");
        break;

    case VK_DESCRIPTOR_TYPE_MUTABLE_EXT:
        if (!indexingFeatures.descriptorBindingUniformBufferUpdateAfterBind ||
            !indexingFeatures.descriptorBindingStorageBufferUpdateAfterBind)
        {
            TCU_THROW(NotSupportedError,
                      "Uniform/storage buffer UpdateAfterBind required for mutable descriptor list not supported");
        }
        break;

    case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
        if (!context.getAccelerationStructureFeatures().descriptorBindingAccelerationStructureUpdateAfterBind)
            TCU_THROW(NotSupportedError, "descriptorBindingAccelerationStructureUpdateAfterBind not supported");
        break;
#endif // CTS_USES_VULKANSC

    default:
        TCU_THROW(InternalError, "Unexpected descriptor type for UpdateAfterBind");
    }
}

Move<VkSampler> makeStandardSampler(const DeviceInterface &vkd, VkDevice device)
{
    const VkSamplerCreateInfo samplerInfo = {
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        nullptr,
        0u,
        VK_FILTER_NEAREST,
        VK_FILTER_NEAREST,
        VK_SAMPLER_MIPMAP_MODE_NEAREST,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        0.0f,
        VK_FALSE,
        1.0f,
        VK_FALSE,
        VK_COMPARE_OP_NEVER,
        0.0f,
        0.0f,
        VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        VK_FALSE,
    };
    return createSampler(vkd, device, &samplerInfo);
}

struct DualSupportQueryResult
{
    bool supported;
    bool countStructQueried;
    uint32_t maxVariableDescriptorCount;
};

template <typename GetSupportFn>
DualSupportQueryResult queryLayoutSupportPairWithFn(GetSupportFn getSupportFn, VkDevice device,
                                                    const VkDescriptorSetLayoutCreateInfo *pCreateInfo,
                                                    bool queryVarCountStruct, bool expectNonZeroVarCount,
                                                    uint32_t requestedVarCount, VkDescriptorType varDescType)
{
    VkDescriptorSetLayoutSupport supportNullFalse = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_SUPPORT,
        nullptr,
        VK_FALSE,
    };
    getSupportFn(device, pCreateInfo, &supportNullFalse);

    VkDescriptorSetLayoutSupport supportNullTrue = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_SUPPORT,
        nullptr,
        VK_TRUE,
    };
    getSupportFn(device, pCreateInfo, &supportNullTrue);

    if ((supportNullFalse.supported != VK_TRUE && supportNullFalse.supported != VK_FALSE) ||
        (supportNullTrue.supported != VK_TRUE && supportNullTrue.supported != VK_FALSE))
    {
        TCU_FAIL("Invalid VkBool32 written to VkDescriptorSetLayoutSupport::supported when pNext is NULL");
    }

    if (supportNullFalse.supported != supportNullTrue.supported)
        TCU_FAIL("Inconsistent VkDescriptorSetLayoutSupport::supported across pre-fill values when pNext is NULL");

    uint32_t maxVarCount = 0u;

    if (queryVarCountStruct)
    {
        VkDescriptorSetVariableDescriptorCountLayoutSupport countA = initVulkanStructure();
        countA.maxVariableDescriptorCount                          = 0xDEADBEEFu;
        VkDescriptorSetLayoutSupport supportA                      = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_SUPPORT,
            &countA,
            VK_FALSE,
        };
        getSupportFn(device, pCreateInfo, &supportA);

        VkDescriptorSetVariableDescriptorCountLayoutSupport countB = initVulkanStructure();
        countB.maxVariableDescriptorCount                          = 0u;
        VkDescriptorSetLayoutSupport supportB                      = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_SUPPORT,
            &countB,
            VK_TRUE,
        };
        getSupportFn(device, pCreateInfo, &supportB);

        if ((supportA.supported != VK_TRUE && supportA.supported != VK_FALSE) ||
            (supportB.supported != VK_TRUE && supportB.supported != VK_FALSE))
        {
            TCU_FAIL("Invalid VkBool32 written to VkDescriptorSetLayoutSupport::supported with count struct");
        }

        if (supportA.supported != supportB.supported)
            TCU_FAIL("Inconsistent VkDescriptorSetLayoutSupport::supported across pre-fill values with count struct");

        if (supportA.supported != supportNullFalse.supported)
        {
            TCU_FAIL("Mismatch in VkDescriptorSetLayoutSupport::supported with and without "
                     "VkDescriptorSetVariableDescriptorCountLayoutSupport");
        }

        if (supportA.supported == VK_TRUE)
        {
            if (countA.maxVariableDescriptorCount != countB.maxVariableDescriptorCount)
                TCU_FAIL("Inconsistent maxVariableDescriptorCount across sentinel pre-fill values");

            maxVarCount = countA.maxVariableDescriptorCount;

            if (!expectNonZeroVarCount)
            {
                if (maxVarCount != 0u)
                    TCU_FAIL("Expected maxVariableDescriptorCount == 0 when variable descriptor count is inactive");
            }
            else
            {
#ifndef CTS_USES_VULKANSC
                const uint32_t zeroFloor = (varDescType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK) ? 4u : 1u;
#else
                const uint32_t zeroFloor = 1u;
                DE_UNREF(varDescType);
#endif // CTS_USES_VULKANSC
                const uint32_t minExpected = std::max(requestedVarCount, zeroFloor);
                if (maxVarCount < minExpected)
                    TCU_FAIL("maxVariableDescriptorCount is smaller than requested or minimum variable count");
            }
        }
    }

    return DualSupportQueryResult{(supportNullFalse.supported == VK_TRUE), queryVarCountStruct, maxVarCount};
}

DualSupportQueryResult queryLayoutSupportPair(const DeviceInterface &vkd, VkDevice device,
                                              const VkDescriptorSetLayoutCreateInfo *pCreateInfo,
                                              bool queryVarCountStruct, bool expectNonZeroVarCount,
                                              uint32_t requestedVarCount, VkDescriptorType varDescType)
{
    const auto fn = [&vkd](VkDevice dev, const VkDescriptorSetLayoutCreateInfo *info, VkDescriptorSetLayoutSupport *out)
    { vkd.getDescriptorSetLayoutSupport(dev, info, out); };

    return queryLayoutSupportPairWithFn(fn, device, pCreateInfo, queryVarCountStruct, expectNonZeroVarCount,
                                        requestedVarCount, varDescType);
}

enum class LayoutTopology : uint32_t
{
    SINGLE = 0,
    ASCENDING_SPARSE,
    DESCENDING,
    UNORDERED_SPARSE,
};

const char *getLayoutTopologyName(LayoutTopology topology)
{
    switch (topology)
    {
    case LayoutTopology::SINGLE:
        return "single";
    case LayoutTopology::ASCENDING_SPARSE:
        return "ascending_sparse";
    case LayoutTopology::DESCENDING:
        return "descending";
    case LayoutTopology::UNORDERED_SPARSE:
        return "unordered_sparse";
    default:
        DE_ASSERT(false);
        return "";
    }
}

enum class BindingFlagsMode : uint32_t
{
    NO_FLAGS_STRUCT = 0,
    ZERO_BINDING_FLAGS,
    NON_VARIABLE_FLAGS,
    VARIABLE_COUNT,
    VARIABLE_COUNT_UAB,
};

const char *getBindingFlagsModeName(BindingFlagsMode mode)
{
    switch (mode)
    {
    case BindingFlagsMode::NO_FLAGS_STRUCT:
        return "no_flags_struct";
    case BindingFlagsMode::ZERO_BINDING_FLAGS:
        return "zero_binding_flags";
    case BindingFlagsMode::NON_VARIABLE_FLAGS:
        return "non_variable_flags";
    case BindingFlagsMode::VARIABLE_COUNT:
        return "variable_count";
    case BindingFlagsMode::VARIABLE_COUNT_UAB:
        return "variable_count_uab";
    default:
        DE_ASSERT(false);
        return "";
    }
}

struct LayoutSupportMatrixParams
{
    VkDescriptorType descriptorType;
    LayoutTopology topology;
    BindingFlagsMode flagsMode;
    bool useImmutableSamplers;
};

void checkSupportLayoutSupportMatrix(Context &context, LayoutSupportMatrixParams params)
{
    context.requireDeviceFunctionality("VK_KHR_maintenance3");
    requireDescriptorTypeSupport(context, params.descriptorType);

    if (params.flagsMode != BindingFlagsMode::NO_FLAGS_STRUCT)
    {
        if (!hasDescriptorIndexingStructs(context))
            TCU_THROW(NotSupportedError, "Descriptor indexing structures not supported");
    }

    const auto &indexingFeatures = context.getDescriptorIndexingFeatures();

    if (params.flagsMode == BindingFlagsMode::NON_VARIABLE_FLAGS)
    {
        if (!indexingFeatures.descriptorBindingPartiallyBound)
            TCU_THROW(NotSupportedError, "descriptorBindingPartiallyBound not supported");
        if (!indexingFeatures.descriptorBindingUpdateUnusedWhilePending)
            TCU_THROW(NotSupportedError, "descriptorBindingUpdateUnusedWhilePending not supported");

        if (isUpdateAfterBindCapableType(params.descriptorType))
            requireUpdateAfterBindFeature(context, params.descriptorType);
    }
    else if (params.flagsMode == BindingFlagsMode::VARIABLE_COUNT)
    {
        if (!indexingFeatures.descriptorBindingVariableDescriptorCount)
            TCU_THROW(NotSupportedError, "descriptorBindingVariableDescriptorCount not supported");
    }
    else if (params.flagsMode == BindingFlagsMode::VARIABLE_COUNT_UAB)
    {
        if (!indexingFeatures.descriptorBindingVariableDescriptorCount)
            TCU_THROW(NotSupportedError, "descriptorBindingVariableDescriptorCount not supported");
        if (!indexingFeatures.descriptorBindingPartiallyBound)
            TCU_THROW(NotSupportedError, "descriptorBindingPartiallyBound not supported");
        if (!indexingFeatures.descriptorBindingUpdateUnusedWhilePending)
            TCU_THROW(NotSupportedError, "descriptorBindingUpdateUnusedWhilePending not supported");

        requireUpdateAfterBindFeature(context, params.descriptorType);
    }
}

tcu::TestStatus testLayoutSupportMatrix(Context &context, LayoutSupportMatrixParams params)
{
    const auto &vkd                 = context.getDeviceInterface();
    const VkDevice device           = context.getDevice();
    const bool canQueryVarCount     = hasDescriptorIndexingStructs(context);
    const bool isVariableMode       = (params.flagsMode == BindingFlagsMode::VARIABLE_COUNT ||
                                 params.flagsMode == BindingFlagsMode::VARIABLE_COUNT_UAB);
    const VkShaderStageFlags stages = getValidStagesForType(params.descriptorType);
    const VkShaderStageFlags uboStg = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    Move<VkSampler> samplerObjs[4];
    VkSampler rawSamplers[4] = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
    if (params.useImmutableSamplers)
    {
        for (size_t i = 0; i < 4u; ++i)
        {
            samplerObjs[i] = makeStandardSampler(vkd, device);
            rawSamplers[i] = *samplerObjs[i];
        }
    }

    uint32_t initialCount = 1u;
#ifndef CTS_USES_VULKANSC
    if (params.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
        initialCount = 4u;
    else
#endif // CTS_USES_VULKANSC
        if (params.useImmutableSamplers)
            initialCount = 2u;

    const VkSampler *const targetSamplers = params.useImmutableSamplers ? rawSamplers : nullptr;

    std::vector<VkDescriptorSetLayoutBinding> bindings;
    size_t targetIdx = 0u;

    switch (params.topology)
    {
    case LayoutTopology::SINGLE:
        bindings.push_back(
            VkDescriptorSetLayoutBinding{0u, params.descriptorType, initialCount, stages, targetSamplers});
        targetIdx = 0u;
        break;

    case LayoutTopology::ASCENDING_SPARSE:
        bindings.push_back(VkDescriptorSetLayoutBinding{1u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u, uboStg, nullptr});
        bindings.push_back(VkDescriptorSetLayoutBinding{5u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0u, 0u, nullptr});
        bindings.push_back(
            VkDescriptorSetLayoutBinding{10u, params.descriptorType, initialCount, stages, targetSamplers});
        targetIdx = 2u;
        break;

    case LayoutTopology::DESCENDING:
        bindings.push_back(
            VkDescriptorSetLayoutBinding{3u, params.descriptorType, initialCount, stages, targetSamplers});
        bindings.push_back(VkDescriptorSetLayoutBinding{2u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u, uboStg, nullptr});
        bindings.push_back(VkDescriptorSetLayoutBinding{1u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u, uboStg, nullptr});
        bindings.push_back(VkDescriptorSetLayoutBinding{0u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u, uboStg, nullptr});
        targetIdx = 0u;
        break;

    case LayoutTopology::UNORDERED_SPARSE:
        bindings.push_back(VkDescriptorSetLayoutBinding{4u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u, uboStg, nullptr});
        bindings.push_back(
            VkDescriptorSetLayoutBinding{12u, params.descriptorType, initialCount, stages, targetSamplers});
        bindings.push_back(VkDescriptorSetLayoutBinding{0u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0u, 0u, nullptr});
        bindings.push_back(VkDescriptorSetLayoutBinding{7u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u, uboStg, nullptr});
        targetIdx = 1u;
        break;

    default:
        DE_ASSERT(false);
        break;
    }

    VkDescriptorSetLayoutCreateFlags layoutFlags = 0u;
    std::vector<VkDescriptorBindingFlags> bindingFlags(bindings.size(), 0u);

    if (params.flagsMode == BindingFlagsMode::NON_VARIABLE_FLAGS)
    {
        const bool useUab = isUpdateAfterBindCapableType(params.descriptorType);
        if (useUab)
            layoutFlags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

        for (size_t i = 0; i < bindings.size(); ++i)
        {
            if (bindings[i].descriptorCount == 0u)
                continue;

            bindingFlags[i] =
                VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;
            if (i == targetIdx && useUab)
                bindingFlags[i] |= VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
        }
    }
    else if (params.flagsMode == BindingFlagsMode::VARIABLE_COUNT)
    {
        bindingFlags[targetIdx] = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
    }
    else if (params.flagsMode == BindingFlagsMode::VARIABLE_COUNT_UAB)
    {
        layoutFlags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        for (size_t i = 0; i < bindings.size(); ++i)
        {
            if (i == targetIdx)
            {
                bindingFlags[i] =
                    VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
                    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;
            }
            else if (bindings[i].descriptorCount > 0u)
            {
                bindingFlags[i] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
            }
        }
    }

    const void *pNextChain = nullptr;

#ifndef CTS_USES_VULKANSC
    static const VkDescriptorType kMutableBufferTypes[2] = {
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    };
    std::vector<VkMutableDescriptorTypeListEXT> mutableLists;
    VkMutableDescriptorTypeCreateInfoEXT mutableInfo = initVulkanStructure();

    if (params.descriptorType == VK_DESCRIPTOR_TYPE_MUTABLE_EXT)
    {
        mutableLists.assign(bindings.size(), VkMutableDescriptorTypeListEXT{0u, nullptr});
        mutableLists[targetIdx]                    = VkMutableDescriptorTypeListEXT{2u, kMutableBufferTypes};
        mutableInfo.pNext                          = nullptr;
        mutableInfo.mutableDescriptorTypeListCount = de::sizeU32(mutableLists);
        mutableInfo.pMutableDescriptorTypeLists    = de::dataOrNull(mutableLists);
        pNextChain                                 = &mutableInfo;
    }
#endif // CTS_USES_VULKANSC

    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        pNextChain,
        de::sizeU32(bindingFlags),
        de::dataOrNull(bindingFlags),
    };
    if (params.flagsMode != BindingFlagsMode::NO_FLAGS_STRUCT)
        pNextChain = &bindingFlagsInfo;

    VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        pNextChain,
        layoutFlags,
        de::sizeU32(bindings),
        de::dataOrNull(bindings),
    };

    bool shouldCreateLayout = !(params.useImmutableSamplers && isVariableMode);
#ifndef CTS_USES_VULKANSC
    if (params.descriptorType == VK_DESCRIPTOR_TYPE_MUTABLE_EXT && params.topology != LayoutTopology::SINGLE)
        shouldCreateLayout = false;
#endif // CTS_USES_VULKANSC

    const DualSupportQueryResult initialRes = queryLayoutSupportPair(
        vkd, device, &layoutCreateInfo, canQueryVarCount, isVariableMode, initialCount, params.descriptorType);
    if (!initialRes.supported)
        TCU_THROW(NotSupportedError, "Descriptor set layout not supported");

    if (params.flagsMode == BindingFlagsMode::ZERO_BINDING_FLAGS)
    {
        VkDescriptorSetLayoutBindingFlagsCreateInfo emptyFlagsInfo = bindingFlagsInfo;
        emptyFlagsInfo.bindingCount                                = 0u;
        emptyFlagsInfo.pBindingFlags                               = nullptr;

        VkDescriptorSetLayoutCreateInfo emptyFlagsLayoutInfo = layoutCreateInfo;
        emptyFlagsLayoutInfo.pNext                           = &emptyFlagsInfo;

        const DualSupportQueryResult emptyFlagsRes = queryLayoutSupportPair(
            vkd, device, &emptyFlagsLayoutInfo, canQueryVarCount, false, 0u, params.descriptorType);
        if (emptyFlagsRes.supported != initialRes.supported ||
            emptyFlagsRes.maxVariableDescriptorCount != initialRes.maxVariableDescriptorCount)
        {
            TCU_FAIL("Mismatch between zero-filled pBindingFlags and bindingCount == 0 in BindingFlagsCreateInfo");
        }
    }

    if (shouldCreateLayout)
    {
        const Move<VkDescriptorSetLayout> layout = createDescriptorSetLayout(vkd, device, &layoutCreateInfo);
        DE_ASSERT(*layout != VK_NULL_HANDLE);
    }

    if (isVariableMode)
    {
#ifndef CTS_USES_VULKANSC
        if (params.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
        {
            const auto &iubProps = context.getInlineUniformBlockProperties();
            if (initialRes.maxVariableDescriptorCount > iubProps.maxInlineUniformBlockSize)
                TCU_FAIL("Implementation reports max IUB size larger than maxInlineUniformBlockSize");
            if ((initialRes.maxVariableDescriptorCount % 4u) != 0u)
                TCU_FAIL("Implementation reports max IUB size that is not a multiple of 4");
        }
#endif // CTS_USES_VULKANSC

        // Sweep zero descriptor count on variable-sized binding.
        bindings[targetIdx].descriptorCount = 0u;
        const DualSupportQueryResult zeroRes =
            queryLayoutSupportPair(vkd, device, &layoutCreateInfo, canQueryVarCount, true, 0u, params.descriptorType);
        if (!zeroRes.supported)
            TCU_FAIL("Implementation reports support for initial count but no support for zero variable count");
        if (zeroRes.maxVariableDescriptorCount != initialRes.maxVariableDescriptorCount)
            TCU_FAIL("Mismatch in maxVariableDescriptorCount between initial and zero variable count");

        if (shouldCreateLayout)
        {
            const Move<VkDescriptorSetLayout> zeroLayout = createDescriptorSetLayout(vkd, device, &layoutCreateInfo);
            DE_ASSERT(*zeroLayout != VK_NULL_HANDLE);
        }

        uint32_t midCount = 1u;
        uint32_t maxCount = initialRes.maxVariableDescriptorCount;
        if (params.useImmutableSamplers)
        {
            maxCount = std::min(4u, initialRes.maxVariableDescriptorCount);
            midCount = std::min(1u, maxCount);
        }
#ifndef CTS_USES_VULKANSC
        else if (params.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
        {
            midCount = ((initialRes.maxVariableDescriptorCount / 2u) & ~3u);
            if (midCount == 0u)
                midCount = 4u;
        }
#endif // CTS_USES_VULKANSC
        else
        {
            midCount = std::min(initialRes.maxVariableDescriptorCount, 16u);
        }

        const uint32_t sweepCounts[2] = {midCount, maxCount};
        for (size_t s = 0; s < 2u; ++s)
        {
            const uint32_t count = sweepCounts[s];
            if (s == 1u && count == midCount)
                continue;

            bindings[targetIdx].descriptorCount   = count;
            const DualSupportQueryResult sweepRes = queryLayoutSupportPair(
                vkd, device, &layoutCreateInfo, canQueryVarCount, true, count, params.descriptorType);
            if (!sweepRes.supported)
                TCU_FAIL("Implementation reports no support within reported maxVariableDescriptorCount");
            if (sweepRes.maxVariableDescriptorCount != initialRes.maxVariableDescriptorCount)
                TCU_FAIL("Mismatch in maxVariableDescriptorCount across variable descriptor count sweep");

            if (shouldCreateLayout)
            {
                const Move<VkDescriptorSetLayout> maxLayout = createDescriptorSetLayout(vkd, device, &layoutCreateInfo);
                DE_ASSERT(*maxLayout != VK_NULL_HANDLE);
            }
        }

        bindings[targetIdx].descriptorCount = initialCount;
    }

    if (params.topology == LayoutTopology::DESCENDING || params.topology == LayoutTopology::UNORDERED_SPARSE)
    {
        std::vector<size_t> order(bindings.size());
        for (size_t i = 0; i < order.size(); ++i)
            order[i] = i;

        std::sort(order.begin(), order.end(),
                  [&bindings](size_t a, size_t b) { return bindings[a].binding < bindings[b].binding; });

        std::vector<VkDescriptorSetLayoutBinding> sortedBindings(bindings.size());
        std::vector<VkDescriptorBindingFlags> sortedFlags(bindingFlags.size());
        for (size_t i = 0; i < order.size(); ++i)
        {
            sortedBindings[i] = bindings[order[i]];
            sortedFlags[i]    = bindingFlags[order[i]];
        }

        const void *sortedNext = nullptr;
#ifndef CTS_USES_VULKANSC
        std::vector<VkMutableDescriptorTypeListEXT> sortedMutableLists;
        VkMutableDescriptorTypeCreateInfoEXT sortedMutableInfo = initVulkanStructure();
        if (params.descriptorType == VK_DESCRIPTOR_TYPE_MUTABLE_EXT)
        {
            sortedMutableLists.resize(mutableLists.size());
            for (size_t i = 0; i < order.size(); ++i)
                sortedMutableLists[i] = mutableLists[order[i]];

            sortedMutableInfo.pNext                          = nullptr;
            sortedMutableInfo.mutableDescriptorTypeListCount = de::sizeU32(sortedMutableLists);
            sortedMutableInfo.pMutableDescriptorTypeLists    = de::dataOrNull(sortedMutableLists);
            sortedNext                                       = &sortedMutableInfo;
        }
#endif // CTS_USES_VULKANSC

        VkDescriptorSetLayoutBindingFlagsCreateInfo sortedFlagsInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
            sortedNext,
            de::sizeU32(sortedFlags),
            de::dataOrNull(sortedFlags),
        };
        if (params.flagsMode != BindingFlagsMode::NO_FLAGS_STRUCT)
            sortedNext = &sortedFlagsInfo;

        const VkDescriptorSetLayoutCreateInfo sortedCreateInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            sortedNext,
            layoutFlags,
            de::sizeU32(sortedBindings),
            de::dataOrNull(sortedBindings),
        };

        const DualSupportQueryResult sortedRes = queryLayoutSupportPair(
            vkd, device, &sortedCreateInfo, canQueryVarCount, isVariableMode, initialCount, params.descriptorType);
        if (sortedRes.supported != initialRes.supported ||
            sortedRes.maxVariableDescriptorCount != initialRes.maxVariableDescriptorCount)
        {
            TCU_FAIL("pBindings array ordering changed vkGetDescriptorSetLayoutSupport output");
        }
    }

    return tcu::TestStatus::pass("Pass");
}

#ifndef CTS_USES_VULKANSC
enum class CreateFlagsVariant : uint32_t
{
    PUSH_DESCRIPTOR_EMPTY = 0,
    PUSH_DESCRIPTOR_UNIFORM_BUFFER,
    PUSH_DESCRIPTOR_COMBINED_IMAGE_SAMPLER_IMMUTABLE,
    PUSH_DESCRIPTOR_DESCENDING_PARTIALLY_BOUND,
    PUSH_DESCRIPTOR_MAX_PUSH_DESCRIPTORS,
    HOST_ONLY_POOL_UNIFORM_BUFFER_NO_VARIABLE,
    HOST_ONLY_POOL_UNIFORM_BUFFER_VARIABLE_COUNT,
    HOST_ONLY_POOL_MUTABLE_EXT_NO_VARIABLE,
    HOST_ONLY_POOL_MUTABLE_EXT_VARIABLE_COUNT,
    DESCRIPTOR_BUFFER_EMPTY,
    DESCRIPTOR_BUFFER_UNIFORM_BUFFER_NO_VARIABLE,
    DESCRIPTOR_BUFFER_UNIFORM_BUFFER_VARIABLE_COUNT,
    DESCRIPTOR_BUFFER_SAMPLED_IMAGE_VARIABLE_COUNT,
    DESCRIPTOR_BUFFER_INLINE_UNIFORM_BLOCK_VARIABLE_COUNT,
    DESCRIPTOR_BUFFER_EMBEDDED_IMMUTABLE_SAMPLERS,
    DESCRIPTOR_BUFFER_EMBEDDED_IMMUTABLE_SAMPLERS_SPARSE_ZERO_COUNT,
};

void checkSupportCreateFlagsVariant(Context &context, CreateFlagsVariant variant)
{
    context.requireDeviceFunctionality("VK_KHR_maintenance3");

    switch (variant)
    {
    case CreateFlagsVariant::PUSH_DESCRIPTOR_EMPTY:
    case CreateFlagsVariant::PUSH_DESCRIPTOR_UNIFORM_BUFFER:
    case CreateFlagsVariant::PUSH_DESCRIPTOR_COMBINED_IMAGE_SAMPLER_IMMUTABLE:
    case CreateFlagsVariant::PUSH_DESCRIPTOR_DESCENDING_PARTIALLY_BOUND:
    case CreateFlagsVariant::PUSH_DESCRIPTOR_MAX_PUSH_DESCRIPTORS:
    {
        context.requireDeviceFunctionality("VK_KHR_push_descriptor");
        const uint32_t maxPush = context.getPushDescriptorProperties().maxPushDescriptors;
        if (variant != CreateFlagsVariant::PUSH_DESCRIPTOR_EMPTY && maxPush == 0u)
            TCU_THROW(NotSupportedError, "maxPushDescriptors is zero");
        if (variant == CreateFlagsVariant::PUSH_DESCRIPTOR_DESCENDING_PARTIALLY_BOUND)
        {
            if (maxPush < 3u)
                TCU_THROW(NotSupportedError, "maxPushDescriptors < 3");
            if (!hasDescriptorIndexingStructs(context) ||
                !context.getDescriptorIndexingFeatures().descriptorBindingPartiallyBound)
            {
                TCU_THROW(NotSupportedError, "descriptorBindingPartiallyBound not supported");
            }
        }
        break;
    }

    case CreateFlagsVariant::HOST_ONLY_POOL_UNIFORM_BUFFER_NO_VARIABLE:
    case CreateFlagsVariant::HOST_ONLY_POOL_UNIFORM_BUFFER_VARIABLE_COUNT:
    case CreateFlagsVariant::HOST_ONLY_POOL_MUTABLE_EXT_NO_VARIABLE:
    case CreateFlagsVariant::HOST_ONLY_POOL_MUTABLE_EXT_VARIABLE_COUNT:
        if (!hasMutableDescriptorTypeSupport(context))
            TCU_THROW(NotSupportedError, "VK_EXT_mutable_descriptor_type feature not supported");
        if (variant == CreateFlagsVariant::HOST_ONLY_POOL_UNIFORM_BUFFER_VARIABLE_COUNT ||
            variant == CreateFlagsVariant::HOST_ONLY_POOL_MUTABLE_EXT_VARIABLE_COUNT)
        {
            if (!hasDescriptorIndexingStructs(context) ||
                !context.getDescriptorIndexingFeatures().descriptorBindingVariableDescriptorCount)
            {
                TCU_THROW(NotSupportedError, "descriptorBindingVariableDescriptorCount not supported");
            }
        }
        break;

    case CreateFlagsVariant::DESCRIPTOR_BUFFER_EMPTY:
    case CreateFlagsVariant::DESCRIPTOR_BUFFER_UNIFORM_BUFFER_NO_VARIABLE:
    case CreateFlagsVariant::DESCRIPTOR_BUFFER_UNIFORM_BUFFER_VARIABLE_COUNT:
    case CreateFlagsVariant::DESCRIPTOR_BUFFER_SAMPLED_IMAGE_VARIABLE_COUNT:
    case CreateFlagsVariant::DESCRIPTOR_BUFFER_INLINE_UNIFORM_BLOCK_VARIABLE_COUNT:
    case CreateFlagsVariant::DESCRIPTOR_BUFFER_EMBEDDED_IMMUTABLE_SAMPLERS:
    case CreateFlagsVariant::DESCRIPTOR_BUFFER_EMBEDDED_IMMUTABLE_SAMPLERS_SPARSE_ZERO_COUNT:
        context.requireDeviceFunctionality("VK_EXT_descriptor_buffer");
        if (!context.getDescriptorBufferFeaturesEXT().descriptorBuffer)
            TCU_THROW(NotSupportedError, "descriptorBuffer feature not supported");
        if (variant == CreateFlagsVariant::DESCRIPTOR_BUFFER_INLINE_UNIFORM_BLOCK_VARIABLE_COUNT)
            requireDescriptorTypeSupport(context, VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK);
        if (variant == CreateFlagsVariant::DESCRIPTOR_BUFFER_UNIFORM_BUFFER_VARIABLE_COUNT ||
            variant == CreateFlagsVariant::DESCRIPTOR_BUFFER_SAMPLED_IMAGE_VARIABLE_COUNT ||
            variant == CreateFlagsVariant::DESCRIPTOR_BUFFER_INLINE_UNIFORM_BLOCK_VARIABLE_COUNT)
        {
            if (!hasDescriptorIndexingStructs(context) ||
                !context.getDescriptorIndexingFeatures().descriptorBindingVariableDescriptorCount)
            {
                TCU_THROW(NotSupportedError, "descriptorBindingVariableDescriptorCount not supported");
            }
        }
        break;

    default:
        DE_ASSERT(false);
        break;
    }
}

tcu::TestStatus testCreateFlagsVariant(Context &context, CreateFlagsVariant variant)
{
    const auto &vkd             = context.getDeviceInterface();
    const VkDevice device       = context.getDevice();
    const bool canQueryVarCount = hasDescriptorIndexingStructs(context);

    Move<VkSampler> samplerObjs[2];
    VkSampler rawSamplers[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};

    VkDescriptorSetLayoutCreateFlags createFlags = 0u;
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    std::vector<VkDescriptorBindingFlags> bindingFlags;
    bool useBindingFlags   = false;
    bool isVariable        = false;
    bool createLayoutAfter = true;
    uint32_t varReqCount   = 0u;
    VkDescriptorType varTp = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

    static const VkDescriptorType kMutableBufferTypes[2] = {
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    };
    std::vector<VkMutableDescriptorTypeListEXT> mutableLists;
    VkMutableDescriptorTypeCreateInfoEXT mutableInfo = initVulkanStructure();
    bool useMutableInfo                              = false;

    switch (variant)
    {
    case CreateFlagsVariant::PUSH_DESCRIPTOR_EMPTY:
        createFlags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT;
        break;

    case CreateFlagsVariant::PUSH_DESCRIPTOR_UNIFORM_BUFFER:
        createFlags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT;
        bindings.push_back(VkDescriptorSetLayoutBinding{0u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u,
                                                        VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
        break;

    case CreateFlagsVariant::PUSH_DESCRIPTOR_COMBINED_IMAGE_SAMPLER_IMMUTABLE:
    {
        createFlags          = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT;
        const uint32_t count = std::min(2u, context.getPushDescriptorProperties().maxPushDescriptors);
        for (uint32_t i = 0; i < count; ++i)
        {
            samplerObjs[i] = makeStandardSampler(vkd, device);
            rawSamplers[i] = *samplerObjs[i];
        }
        bindings.push_back(VkDescriptorSetLayoutBinding{0u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, count,
                                                        VK_SHADER_STAGE_FRAGMENT_BIT, rawSamplers});
        break;
    }

    case CreateFlagsVariant::PUSH_DESCRIPTOR_DESCENDING_PARTIALLY_BOUND:
        createFlags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT;
        bindings.push_back(VkDescriptorSetLayoutBinding{2u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u,
                                                        VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
        bindings.push_back(VkDescriptorSetLayoutBinding{1u, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1u,
                                                        VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
        bindings.push_back(VkDescriptorSetLayoutBinding{0u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u,
                                                        VK_SHADER_STAGE_COMPUTE_BIT, nullptr});
        bindingFlags.assign(3u, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
        useBindingFlags = true;
        break;

    case CreateFlagsVariant::PUSH_DESCRIPTOR_MAX_PUSH_DESCRIPTORS:
    {
        createFlags           = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT;
        const auto &limits    = context.getDeviceProperties().limits;
        const uint32_t maxPsh = context.getPushDescriptorProperties().maxPushDescriptors;
        const uint32_t count =
            std::min(maxPsh, std::min(limits.maxPerStageDescriptorSampledImages, limits.maxDescriptorSetSampledImages));
        if (count == 0u)
            TCU_THROW(NotSupportedError, "Sampled image push descriptor limit is zero");
        bindings.push_back(VkDescriptorSetLayoutBinding{0u, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, count,
                                                        VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
        break;
    }

    case CreateFlagsVariant::HOST_ONLY_POOL_UNIFORM_BUFFER_NO_VARIABLE:
    case CreateFlagsVariant::HOST_ONLY_POOL_UNIFORM_BUFFER_VARIABLE_COUNT:
        createFlags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_HOST_ONLY_POOL_BIT_EXT;
        bindings.push_back(VkDescriptorSetLayoutBinding{3u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u,
                                                        VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
        bindings.push_back(VkDescriptorSetLayoutBinding{1u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u,
                                                        VK_SHADER_STAGE_VERTEX_BIT, nullptr});
        if (variant == CreateFlagsVariant::HOST_ONLY_POOL_UNIFORM_BUFFER_VARIABLE_COUNT)
        {
            bindingFlags    = {VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT, 0u};
            useBindingFlags = true;
            isVariable      = true;
            varReqCount     = 1u;
            varTp           = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        }
        break;

    case CreateFlagsVariant::HOST_ONLY_POOL_MUTABLE_EXT_NO_VARIABLE:
    case CreateFlagsVariant::HOST_ONLY_POOL_MUTABLE_EXT_VARIABLE_COUNT:
        createFlags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_HOST_ONLY_POOL_BIT_EXT;
        bindings.push_back(VkDescriptorSetLayoutBinding{3u, VK_DESCRIPTOR_TYPE_MUTABLE_EXT, 1u,
                                                        VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
        bindings.push_back(VkDescriptorSetLayoutBinding{1u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u,
                                                        VK_SHADER_STAGE_VERTEX_BIT, nullptr});
        mutableLists      = {VkMutableDescriptorTypeListEXT{2u, kMutableBufferTypes},
                             VkMutableDescriptorTypeListEXT{0u, nullptr}};
        useMutableInfo    = true;
        createLayoutAfter = false;
        if (variant == CreateFlagsVariant::HOST_ONLY_POOL_MUTABLE_EXT_VARIABLE_COUNT)
        {
            bindingFlags    = {VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT, 0u};
            useBindingFlags = true;
            isVariable      = true;
            varReqCount     = 1u;
            varTp           = VK_DESCRIPTOR_TYPE_MUTABLE_EXT;
        }
        break;

    case CreateFlagsVariant::DESCRIPTOR_BUFFER_EMPTY:
        createFlags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
        break;

    case CreateFlagsVariant::DESCRIPTOR_BUFFER_UNIFORM_BUFFER_NO_VARIABLE:
    case CreateFlagsVariant::DESCRIPTOR_BUFFER_UNIFORM_BUFFER_VARIABLE_COUNT:
        createFlags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
        bindings.push_back(VkDescriptorSetLayoutBinding{3u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u,
                                                        VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
        bindings.push_back(VkDescriptorSetLayoutBinding{1u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u,
                                                        VK_SHADER_STAGE_VERTEX_BIT, nullptr});
        if (variant == CreateFlagsVariant::DESCRIPTOR_BUFFER_UNIFORM_BUFFER_VARIABLE_COUNT)
        {
            bindingFlags    = {VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT, 0u};
            useBindingFlags = true;
            isVariable      = true;
            varReqCount     = 1u;
            varTp           = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        }
        break;

    case CreateFlagsVariant::DESCRIPTOR_BUFFER_SAMPLED_IMAGE_VARIABLE_COUNT:
        createFlags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
        bindings.push_back(VkDescriptorSetLayoutBinding{3u, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1u,
                                                        VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
        bindings.push_back(VkDescriptorSetLayoutBinding{1u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u,
                                                        VK_SHADER_STAGE_VERTEX_BIT, nullptr});
        bindingFlags    = {VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT, 0u};
        useBindingFlags = true;
        isVariable      = true;
        varReqCount     = 1u;
        varTp           = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        break;

    case CreateFlagsVariant::DESCRIPTOR_BUFFER_INLINE_UNIFORM_BLOCK_VARIABLE_COUNT:
        createFlags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
        bindings.push_back(VkDescriptorSetLayoutBinding{3u, VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK, 4u,
                                                        VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
        bindings.push_back(VkDescriptorSetLayoutBinding{1u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u,
                                                        VK_SHADER_STAGE_VERTEX_BIT, nullptr});
        bindingFlags    = {VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT, 0u};
        useBindingFlags = true;
        isVariable      = true;
        varReqCount     = 4u;
        varTp           = VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK;
        break;

    case CreateFlagsVariant::DESCRIPTOR_BUFFER_EMBEDDED_IMMUTABLE_SAMPLERS:
        createFlags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT |
                      VK_DESCRIPTOR_SET_LAYOUT_CREATE_EMBEDDED_IMMUTABLE_SAMPLERS_BIT_EXT;
        samplerObjs[0] = makeStandardSampler(vkd, device);
        samplerObjs[1] = makeStandardSampler(vkd, device);
        rawSamplers[0] = *samplerObjs[0];
        rawSamplers[1] = *samplerObjs[1];
        bindings.push_back(VkDescriptorSetLayoutBinding{1u, VK_DESCRIPTOR_TYPE_SAMPLER, 1u,
                                                        VK_SHADER_STAGE_FRAGMENT_BIT, &rawSamplers[1]});
        bindings.push_back(VkDescriptorSetLayoutBinding{0u, VK_DESCRIPTOR_TYPE_SAMPLER, 1u,
                                                        VK_SHADER_STAGE_FRAGMENT_BIT, &rawSamplers[0]});
        break;

    case CreateFlagsVariant::DESCRIPTOR_BUFFER_EMBEDDED_IMMUTABLE_SAMPLERS_SPARSE_ZERO_COUNT:
        createFlags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT |
                      VK_DESCRIPTOR_SET_LAYOUT_CREATE_EMBEDDED_IMMUTABLE_SAMPLERS_BIT_EXT;
        samplerObjs[0]    = makeStandardSampler(vkd, device);
        rawSamplers[0]    = *samplerObjs[0];
        createLayoutAfter = false;
        bindings.push_back(VkDescriptorSetLayoutBinding{2u, VK_DESCRIPTOR_TYPE_SAMPLER, 1u,
                                                        VK_SHADER_STAGE_FRAGMENT_BIT, &rawSamplers[0]});
        bindings.push_back(VkDescriptorSetLayoutBinding{0u, VK_DESCRIPTOR_TYPE_SAMPLER, 0u, 0u, nullptr});
        break;

    default:
        DE_ASSERT(false);
        break;
    }

    const void *pNextChain = nullptr;
    if (useMutableInfo)
    {
        mutableInfo.pNext                          = nullptr;
        mutableInfo.mutableDescriptorTypeListCount = de::sizeU32(mutableLists);
        mutableInfo.pMutableDescriptorTypeLists    = de::dataOrNull(mutableLists);
        pNextChain                                 = &mutableInfo;
    }

    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        pNextChain,
        de::sizeU32(bindingFlags),
        de::dataOrNull(bindingFlags),
    };
    if (useBindingFlags)
        pNextChain = &bindingFlagsInfo;

    VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        pNextChain,
        createFlags,
        de::sizeU32(bindings),
        de::dataOrNull(bindings),
    };

    const DualSupportQueryResult res =
        queryLayoutSupportPair(vkd, device, &layoutCreateInfo, canQueryVarCount, isVariable, varReqCount, varTp);

    if (!res.supported)
    {
        if (variant == CreateFlagsVariant::PUSH_DESCRIPTOR_MAX_PUSH_DESCRIPTORS &&
            bindings[0].descriptorCount > context.getMaintenance3Properties().maxPerSetDescriptors)
        {
            return tcu::TestStatus::pass("Pass");
        }
        TCU_THROW(NotSupportedError, "Descriptor set layout with createFlags not supported");
    }

    if (isVariable)
    {
        bindings[0].descriptorCount = 0u;
        const DualSupportQueryResult zeroRes =
            queryLayoutSupportPair(vkd, device, &layoutCreateInfo, canQueryVarCount, true, 0u, varTp);
        if (!zeroRes.supported || zeroRes.maxVariableDescriptorCount != res.maxVariableDescriptorCount)
            TCU_FAIL("Zero variable count mismatch under layout createFlags");
        bindings[0].descriptorCount = varReqCount;
    }

    if (createLayoutAfter)
    {
        const Move<VkDescriptorSetLayout> layout = createDescriptorSetLayout(vkd, device, &layoutCreateInfo);
        DE_ASSERT(*layout != VK_NULL_HANDLE);
    }

    return tcu::TestStatus::pass("Pass");
}
#endif // CTS_USES_VULKANSC

enum class EdgeCaseVariant : uint32_t
{
    EMPTY_LAYOUT = 0,
    ALL_ZERO_DESCRIPTOR_COUNTS_NO_VARIABLE,
    ZERO_COUNT_VARIABLE_BINDING_CREATE_LAYOUT,
#ifndef CTS_USES_VULKANSC
    ZERO_COUNT_VARIABLE_INLINE_UNIFORM_BLOCK,
    MUTABLE_EXT_MIXED_TYPES_NO_VARIABLE,
    MUTABLE_EXT_MIXED_TYPES_VARIABLE_COUNT,
#endif // CTS_USES_VULKANSC
    OVER_LIMIT_NON_VARIABLE_UINT32_MAX,
    OVER_LIMIT_DYNAMIC_BUFFERS,
    VARIABLE_FEATURE_DISABLED_CUSTOM_DEVICE,
#ifndef CTS_USES_VULKANSC
    KHR_ENTRY_POINT,
#endif // CTS_USES_VULKANSC
};

void checkSupportEdgeCase(Context &context, EdgeCaseVariant variant)
{
    context.requireDeviceFunctionality("VK_KHR_maintenance3");

    switch (variant)
    {
    case EdgeCaseVariant::EMPTY_LAYOUT:
    case EdgeCaseVariant::ALL_ZERO_DESCRIPTOR_COUNTS_NO_VARIABLE:
    case EdgeCaseVariant::OVER_LIMIT_NON_VARIABLE_UINT32_MAX:
        break;

    case EdgeCaseVariant::OVER_LIMIT_DYNAMIC_BUFFERS:
        if (context.getDeviceProperties().limits.maxDescriptorSetUniformBuffersDynamic ==
            std::numeric_limits<uint32_t>::max())
        {
            TCU_THROW(NotSupportedError, "maxDescriptorSetUniformBuffersDynamic is UINT32_MAX");
        }
        break;

    case EdgeCaseVariant::ZERO_COUNT_VARIABLE_BINDING_CREATE_LAYOUT:
        if (!hasDescriptorIndexingStructs(context) ||
            !context.getDescriptorIndexingFeatures().descriptorBindingVariableDescriptorCount)
        {
            TCU_THROW(NotSupportedError, "descriptorBindingVariableDescriptorCount not supported");
        }
        break;

#ifndef CTS_USES_VULKANSC
    case EdgeCaseVariant::ZERO_COUNT_VARIABLE_INLINE_UNIFORM_BLOCK:
        requireDescriptorTypeSupport(context, VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK);
        if (!hasDescriptorIndexingStructs(context) ||
            !context.getDescriptorIndexingFeatures().descriptorBindingVariableDescriptorCount)
        {
            TCU_THROW(NotSupportedError, "descriptorBindingVariableDescriptorCount not supported");
        }
        break;

    case EdgeCaseVariant::MUTABLE_EXT_MIXED_TYPES_NO_VARIABLE:
    case EdgeCaseVariant::MUTABLE_EXT_MIXED_TYPES_VARIABLE_COUNT:
        if (!hasMutableDescriptorTypeSupport(context))
            TCU_THROW(NotSupportedError, "VK_EXT_mutable_descriptor_type feature not supported");
        if (variant == EdgeCaseVariant::MUTABLE_EXT_MIXED_TYPES_VARIABLE_COUNT &&
            (!hasDescriptorIndexingStructs(context) ||
             !context.getDescriptorIndexingFeatures().descriptorBindingVariableDescriptorCount))
        {
            TCU_THROW(NotSupportedError, "descriptorBindingVariableDescriptorCount not supported");
        }
        break;
#endif // CTS_USES_VULKANSC

    case EdgeCaseVariant::VARIABLE_FEATURE_DISABLED_CUSTOM_DEVICE:
        if (!hasDescriptorIndexingStructs(context))
            TCU_THROW(NotSupportedError, "Descriptor indexing structures not supported");
        break;

#ifndef CTS_USES_VULKANSC
    case EdgeCaseVariant::KHR_ENTRY_POINT:
    {
        const auto extProps =
            enumerateDeviceExtensionProperties(context.getInstanceInterface(), context.getPhysicalDevice(), nullptr);
        if (!isExtensionStructSupported(extProps, RequiredExtension("VK_KHR_maintenance3")))
            TCU_THROW(NotSupportedError, "VK_KHR_maintenance3 extension not advertised");
        if (!isExtensionStructSupported(extProps, RequiredExtension("VK_EXT_descriptor_indexing")))
            TCU_THROW(NotSupportedError, "VK_EXT_descriptor_indexing extension not advertised");
        if (!context.getDescriptorIndexingFeatures().descriptorBindingVariableDescriptorCount)
            TCU_THROW(NotSupportedError, "descriptorBindingVariableDescriptorCount not supported");
        break;
    }
#endif // CTS_USES_VULKANSC

    default:
        DE_ASSERT(false);
        break;
    }
}

tcu::TestStatus testEdgeCase(Context &context, EdgeCaseVariant variant)
{
    const auto &vkd             = context.getDeviceInterface();
    const VkDevice device       = context.getDevice();
    const bool canQueryVarCount = hasDescriptorIndexingStructs(context);

    switch (variant)
    {
    case EdgeCaseVariant::EMPTY_LAYOUT:
    {
        const VkDescriptorSetLayoutCreateInfo emptyInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0u, 0u, nullptr,
        };
        const DualSupportQueryResult res = queryLayoutSupportPair(vkd, device, &emptyInfo, canQueryVarCount, false, 0u,
                                                                  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        if (!res.supported)
            TCU_FAIL("Empty descriptor set layout must be supported");

        if (canQueryVarCount)
        {
            const VkDescriptorSetLayoutBindingFlagsCreateInfo emptyFlags = {
                VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                nullptr,
                0u,
                nullptr,
            };
            const VkDescriptorSetLayoutCreateInfo emptyWithFlagsInfo = {
                VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, &emptyFlags, 0u, 0u, nullptr,
            };
            const DualSupportQueryResult flagsRes = queryLayoutSupportPair(
                vkd, device, &emptyWithFlagsInfo, true, false, 0u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
            if (!flagsRes.supported)
                TCU_FAIL("Empty descriptor set layout with empty BindingFlagsCreateInfo must be supported");
        }

        const Move<VkDescriptorSetLayout> layout = createDescriptorSetLayout(vkd, device, &emptyInfo);
        DE_ASSERT(*layout != VK_NULL_HANDLE);
        break;
    }

    case EdgeCaseVariant::ALL_ZERO_DESCRIPTOR_COUNTS_NO_VARIABLE:
    {
        const VkDescriptorSetLayoutBinding bindings[3] = {
            {7u, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 0u, 0u, nullptr},
            {0u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0u, 0u, nullptr},
            {3u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0u, 0u, nullptr},
        };
        const VkDescriptorSetLayoutCreateInfo createInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0u, 3u, bindings,
        };
        const DualSupportQueryResult res = queryLayoutSupportPair(vkd, device, &createInfo, canQueryVarCount, false, 0u,
                                                                  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        if (!res.supported)
            TCU_FAIL("Descriptor set layout with all-zero descriptorCounts must be supported");

        const Move<VkDescriptorSetLayout> layout = createDescriptorSetLayout(vkd, device, &createInfo);
        DE_ASSERT(*layout != VK_NULL_HANDLE);
        break;
    }

    case EdgeCaseVariant::ZERO_COUNT_VARIABLE_BINDING_CREATE_LAYOUT:
    {
        const VkDescriptorSetLayoutBinding bindings[2] = {
            {5u, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 0u, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            {1u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0u, 0u, nullptr},
        };
        const VkDescriptorBindingFlags flags[2] = {
            VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT,
            0u,
        };
        const VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
            nullptr,
            2u,
            flags,
        };
        const VkDescriptorSetLayoutCreateInfo createInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, &flagsInfo, 0u, 2u, bindings,
        };
        const DualSupportQueryResult res =
            queryLayoutSupportPair(vkd, device, &createInfo, true, true, 0u, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
        if (!res.supported)
            TCU_FAIL("Zero-count variable SAMPLED_IMAGE binding layout must be supported");

        const Move<VkDescriptorSetLayout> layout = createDescriptorSetLayout(vkd, device, &createInfo);
        DE_ASSERT(*layout != VK_NULL_HANDLE);
        break;
    }

#ifndef CTS_USES_VULKANSC
    case EdgeCaseVariant::ZERO_COUNT_VARIABLE_INLINE_UNIFORM_BLOCK:
    {
        const VkDescriptorSetLayoutBinding bindings[2] = {
            {6u, VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK, 0u, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            {2u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        };
        const VkDescriptorBindingFlags flags[2] = {
            VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT,
            0u,
        };
        const VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
            nullptr,
            2u,
            flags,
        };
        const VkDescriptorSetLayoutCreateInfo createInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, &flagsInfo, 0u, 2u, bindings,
        };
        const DualSupportQueryResult res =
            queryLayoutSupportPair(vkd, device, &createInfo, true, true, 0u, VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK);
        if (!res.supported)
            TCU_THROW(NotSupportedError, "Variable inline uniform block layout not supported");

        const auto &iubProps = context.getInlineUniformBlockProperties();
        if (res.maxVariableDescriptorCount > iubProps.maxInlineUniformBlockSize ||
            (res.maxVariableDescriptorCount % 4u) != 0u)
        {
            TCU_FAIL("Invalid maxVariableDescriptorCount for zero-count variable inline uniform block");
        }

        const Move<VkDescriptorSetLayout> layout = createDescriptorSetLayout(vkd, device, &createInfo);
        DE_ASSERT(*layout != VK_NULL_HANDLE);
        break;
    }

    case EdgeCaseVariant::MUTABLE_EXT_MIXED_TYPES_NO_VARIABLE:
    case EdgeCaseVariant::MUTABLE_EXT_MIXED_TYPES_VARIABLE_COUNT:
    {
        static const VkDescriptorType kMixedMutableTypes[4] = {
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        };
        const VkDescriptorSetLayoutBinding bindings[2] = {
            {3u, VK_DESCRIPTOR_TYPE_MUTABLE_EXT, 1u, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            {1u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        };
        const VkMutableDescriptorTypeListEXT typeLists[2] = {
            {4u, kMixedMutableTypes},
            {0u, nullptr},
        };
        const VkMutableDescriptorTypeCreateInfoEXT mutableInfo = {
            VK_STRUCTURE_TYPE_MUTABLE_DESCRIPTOR_TYPE_CREATE_INFO_EXT,
            nullptr,
            2u,
            typeLists,
        };
        const bool isVar                        = (variant == EdgeCaseVariant::MUTABLE_EXT_MIXED_TYPES_VARIABLE_COUNT);
        const VkDescriptorBindingFlags flags[2] = {
            isVar ? static_cast<VkDescriptorBindingFlags>(VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT) : 0u,
            0u,
        };
        const VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
            &mutableInfo,
            2u,
            flags,
        };
        const VkDescriptorSetLayoutCreateInfo createInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            isVar ? static_cast<const void *>(&flagsInfo) : static_cast<const void *>(&mutableInfo),
            0u,
            2u,
            bindings,
        };

        // Both supported == VK_TRUE and supported == VK_FALSE are legal for mixed mutable lists;
        // queryLayoutSupportPair verifies deterministic pSupport->pNext == nullptr agreement without crashing.
        (void)queryLayoutSupportPair(vkd, device, &createInfo, canQueryVarCount, isVar, isVar ? 1u : 0u,
                                     VK_DESCRIPTOR_TYPE_MUTABLE_EXT);
        break;
    }
#endif // CTS_USES_VULKANSC

    case EdgeCaseVariant::OVER_LIMIT_NON_VARIABLE_UINT32_MAX:
    {
        const VkDescriptorSetLayoutBinding binding = {
            0u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0x80000000u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr,
        };
        const VkDescriptorSetLayoutCreateInfo createInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0u, 1u, &binding,
        };
        (void)queryLayoutSupportPair(vkd, device, &createInfo, canQueryVarCount, false, 0u,
                                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        break;
    }

    case EdgeCaseVariant::OVER_LIMIT_DYNAMIC_BUFFERS:
    {
        const uint32_t overLimit = context.getDeviceProperties().limits.maxDescriptorSetUniformBuffersDynamic + 1u;
        const VkDescriptorSetLayoutBinding binding = {
            0u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, overLimit, VK_SHADER_STAGE_VERTEX_BIT, nullptr,
        };
        const VkDescriptorSetLayoutCreateInfo createInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0u, 1u, &binding,
        };
        (void)queryLayoutSupportPair(vkd, device, &createInfo, canQueryVarCount, false, 0u,
                                     VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);
        break;
    }

    case EdgeCaseVariant::VARIABLE_FEATURE_DISABLED_CUSTOM_DEVICE:
    {
        const InstanceWrapper instance(context);
        const auto &vki                       = instance.getDriver();
        const VkPhysicalDevice physicalDevice = instance.getPhysicalDevice();
        const auto queueProps                 = getPhysicalDeviceQueueFamilyProperties(vki, physicalDevice);
        DE_ASSERT(!queueProps.empty());

        const auto extProps           = enumerateDeviceExtensionProperties(vki, physicalDevice, nullptr);
        const uint32_t usedApiVersion = context.getUsedApiVersion();
        std::vector<const char *> enabledExts;

        if (!isCoreDeviceExtension(usedApiVersion, "VK_KHR_maintenance3") &&
            isExtensionStructSupported(extProps, RequiredExtension("VK_KHR_maintenance3")))
        {
            enabledExts.push_back("VK_KHR_maintenance3");
        }
        if (!isCoreDeviceExtension(usedApiVersion, "VK_EXT_descriptor_indexing") &&
            isExtensionStructSupported(extProps, RequiredExtension("VK_EXT_descriptor_indexing")))
        {
            enabledExts.push_back("VK_EXT_descriptor_indexing");
        }
#ifndef CTS_USES_VULKANSC
        if (isExtensionStructSupported(extProps, RequiredExtension("VK_KHR_portability_subset")))
            enabledExts.push_back("VK_KHR_portability_subset");
#endif // CTS_USES_VULKANSC

        VkPhysicalDeviceDescriptorIndexingFeatures physIndexing = initVulkanStructure();
        VkPhysicalDeviceFeatures2 physFeatures2                 = initVulkanStructure(&physIndexing);
        vki.getPhysicalDeviceFeatures2(physicalDevice, &physFeatures2);

        VkPhysicalDeviceDescriptorIndexingFeatures enabledIndexing = initVulkanStructure();
        enabledIndexing.descriptorBindingPartiallyBound            = physIndexing.descriptorBindingPartiallyBound;
        enabledIndexing.descriptorBindingVariableDescriptorCount   = VK_FALSE;

        VkPhysicalDeviceFeatures2 enabledFeatures2 = initVulkanStructure(&enabledIndexing);

        const float queuePriority                     = 1.0f;
        const VkDeviceQueueCreateInfo queueCreateInfo = {
            VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr, 0u, 0u, 1u, &queuePriority,
        };
        const VkDeviceCreateInfo deviceCreateInfo = {
            VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            &enabledFeatures2,
            0u,
            1u,
            &queueCreateInfo,
            0u,
            nullptr,
            de::sizeU32(enabledExts),
            de::dataOrNull(enabledExts),
            nullptr,
        };

        const DeviceWrapper customDevice(instance.createCustomDevice(physicalDevice, &deviceCreateInfo));
        const auto &customVkd = customDevice.getDriver();

        const VkDescriptorSetLayoutBinding bindings[2] = {
            {2u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            {0u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        };
        VkDescriptorBindingFlags flags[2]                           = {0u, 0u};
        const VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
            nullptr,
            2u,
            flags,
        };
        const VkDescriptorSetLayoutCreateInfo createInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, &flagsInfo, 0u, 2u, bindings,
        };

        const DualSupportQueryResult resZeroFlags = queryLayoutSupportPair(
            customVkd, *customDevice, &createInfo, true, false, 0u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        if (!resZeroFlags.supported)
            TCU_FAIL("Basic uniform buffer layout must be supported when variable count feature is disabled");

        if (enabledIndexing.descriptorBindingPartiallyBound == VK_TRUE)
        {
            flags[0]                                = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
            flags[1]                                = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
            const DualSupportQueryResult resPartial = queryLayoutSupportPair(
                customVkd, *customDevice, &createInfo, true, false, 0u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
            if (!resPartial.supported)
                TCU_FAIL("Partially-bound uniform buffer layout must be supported on custom device");
        }
        break;
    }

#ifndef CTS_USES_VULKANSC
    case EdgeCaseVariant::KHR_ENTRY_POINT:
    {
        const InstanceWrapper instance(context);
        const auto &vki                       = instance.getDriver();
        const VkPhysicalDevice physicalDevice = instance.getPhysicalDevice();
        const auto queueProps                 = getPhysicalDeviceQueueFamilyProperties(vki, physicalDevice);
        DE_ASSERT(!queueProps.empty());

        const auto extProps                   = enumerateDeviceExtensionProperties(vki, physicalDevice, nullptr);
        std::vector<const char *> enabledExts = {
            "VK_KHR_maintenance3",
            "VK_EXT_descriptor_indexing",
        };
        if (isExtensionStructSupported(extProps, RequiredExtension("VK_KHR_portability_subset")))
            enabledExts.push_back("VK_KHR_portability_subset");

        VkPhysicalDeviceDescriptorIndexingFeatures enabledIndexing = initVulkanStructure();
        enabledIndexing.descriptorBindingVariableDescriptorCount   = VK_TRUE;
        VkPhysicalDeviceFeatures2 enabledFeatures2                 = initVulkanStructure(&enabledIndexing);

        const float queuePriority                     = 1.0f;
        const VkDeviceQueueCreateInfo queueCreateInfo = {
            VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr, 0u, 0u, 1u, &queuePriority,
        };
        const VkDeviceCreateInfo deviceCreateInfo = {
            VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            &enabledFeatures2,
            0u,
            1u,
            &queueCreateInfo,
            0u,
            nullptr,
            de::sizeU32(enabledExts),
            de::dataOrNull(enabledExts),
            nullptr,
        };

        const DeviceWrapper customDevice(instance.createCustomDevice(physicalDevice, &deviceCreateInfo));
        const auto &customVkd       = customDevice.getDriver();
        const auto pfnGetSupportKHR = reinterpret_cast<GetDescriptorSetLayoutSupportKHRFunc>(
            customVkd.getDeviceProcAddr(*customDevice, "vkGetDescriptorSetLayoutSupportKHR"));
        if (pfnGetSupportKHR == nullptr)
            TCU_FAIL("vkGetDeviceProcAddr returned NULL for vkGetDescriptorSetLayoutSupportKHR");

        VkDescriptorSetLayoutBinding bindings[2] = {
            {3u, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1u, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            {0u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        };
        VkDescriptorBindingFlags flags[2]                           = {0u, 0u};
        const VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
            nullptr,
            2u,
            flags,
        };
        const VkDescriptorSetLayoutCreateInfo createInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, &flagsInfo, 0u, 2u, bindings,
        };

        const DualSupportQueryResult nonVarKhr = queryLayoutSupportPairWithFn(
            pfnGetSupportKHR, *customDevice, &createInfo, true, false, 0u, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
        const DualSupportQueryResult nonVarCore = queryLayoutSupportPair(customVkd, *customDevice, &createInfo, true,
                                                                         false, 0u, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
        if (nonVarKhr.supported != nonVarCore.supported ||
            nonVarKhr.maxVariableDescriptorCount != nonVarCore.maxVariableDescriptorCount)
        {
            TCU_FAIL("Mismatch between vkGetDescriptorSetLayoutSupportKHR and core entry point on non-variable layout");
        }

        flags[0]                            = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
        const DualSupportQueryResult varKhr = queryLayoutSupportPairWithFn(
            pfnGetSupportKHR, *customDevice, &createInfo, true, true, 1u, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
        const DualSupportQueryResult varCore = queryLayoutSupportPair(customVkd, *customDevice, &createInfo, true, true,
                                                                      1u, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
        if (!varKhr.supported || varKhr.supported != varCore.supported ||
            varKhr.maxVariableDescriptorCount != varCore.maxVariableDescriptorCount)
        {
            TCU_FAIL("Mismatch between vkGetDescriptorSetLayoutSupportKHR and core entry point on variable layout");
        }

        bindings[0].descriptorCount          = 0u;
        const DualSupportQueryResult zeroKhr = queryLayoutSupportPairWithFn(
            pfnGetSupportKHR, *customDevice, &createInfo, true, true, 0u, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
        if (!zeroKhr.supported || zeroKhr.maxVariableDescriptorCount != varKhr.maxVariableDescriptorCount)
            TCU_FAIL("Zero-count variable mismatch via vkGetDescriptorSetLayoutSupportKHR");
        break;
    }
#endif // CTS_USES_VULKANSC

    default:
        DE_ASSERT(false);
        break;
    }

    return tcu::TestStatus::pass("Pass");
}

} // namespace

tcu::TestCaseGroup *createMaintenance3Tests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> main3Tests(new tcu::TestCaseGroup(testCtx, "maintenance3_check"));
    main3Tests->addChild(new Maintenance3StructTestCase(testCtx));
    main3Tests->addChild(new Maintenance3DescriptorTestCase(testCtx));

    {
        const VkDescriptorType descriptorTypes[] = {
            VK_DESCRIPTOR_TYPE_SAMPLER,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
            VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
            VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
#ifndef CTS_USES_VULKANSC
            VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK,
#endif
        };

        for (const auto &descriptorType : descriptorTypes)
            for (const auto &extraBindings : {false, true})
                for (const auto &useVariableSize : {false, true})
                    for (const auto &bindingOffset : {0u, 1u})
                        for (const auto &createLayout : {false, true})
                        {
                            if (useVariableSize && (descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
                                                    descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC))
                                continue;

                            const auto extraBindingsSuffix = (extraBindings ? "_extra_bindings" : "");
                            const auto variableSizeSuffix  = (useVariableSize ? "" : "_no_variable_size");
                            const auto offsetSuffix        = (bindingOffset != 0u ? "_nonzero_binding_offset" : "");
                            const auto creationSuffix      = (createLayout ? "_create_layout" : "");
                            const auto caseName = "support_count_" + getDescriptorTypeShortName(descriptorType) +
                                                  extraBindingsSuffix + variableSizeSuffix + offsetSuffix +
                                                  creationSuffix;
                            const CountLayoutSupportParams params{descriptorType, bindingOffset, extraBindings,
                                                                  useVariableSize, createLayout};

                            addFunctionCase(main3Tests.get(), caseName.c_str(), checkSupportCountLayoutSupport,
                                            testCountLayoutSupport, params);
                        }
    }

    // Sub-Family A: Combinatorial descriptorType x topology x bindingFlagsMode [x immutableSamplers]
    {
        const VkDescriptorType matrixDescriptorTypes[] = {
            VK_DESCRIPTOR_TYPE_SAMPLER,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
            VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
            VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
#ifndef CTS_USES_VULKANSC
            VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK,
            VK_DESCRIPTOR_TYPE_MUTABLE_EXT,
            VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
#endif // CTS_USES_VULKANSC
        };

        const LayoutTopology topologies[] = {
            LayoutTopology::SINGLE,
            LayoutTopology::ASCENDING_SPARSE,
            LayoutTopology::DESCENDING,
            LayoutTopology::UNORDERED_SPARSE,
        };

        const BindingFlagsMode flagsModes[] = {
            BindingFlagsMode::NO_FLAGS_STRUCT,    BindingFlagsMode::ZERO_BINDING_FLAGS,
            BindingFlagsMode::NON_VARIABLE_FLAGS, BindingFlagsMode::VARIABLE_COUNT,
            BindingFlagsMode::VARIABLE_COUNT_UAB,
        };

        for (const auto &descriptorType : matrixDescriptorTypes)
        {
            const bool canUseImmutableSamplers = (descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
                                                  descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

            for (const bool useImmutableSamplers : {false, true})
            {
                if (useImmutableSamplers && !canUseImmutableSamplers)
                    continue;

                for (const auto &topology : topologies)
                {
                    for (const auto &flagsMode : flagsModes)
                    {
                        const bool isDynamic = (descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
                                                descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);

                        if ((flagsMode == BindingFlagsMode::VARIABLE_COUNT ||
                             flagsMode == BindingFlagsMode::VARIABLE_COUNT_UAB) &&
                            isDynamic)
                        {
                            continue;
                        }

                        if (flagsMode == BindingFlagsMode::VARIABLE_COUNT_UAB &&
                            !isUpdateAfterBindCapableType(descriptorType))
                        {
                            continue;
                        }

                        const std::string immutableSuffix = (useImmutableSamplers ? "_immutable_samplers" : "");
                        const std::string caseName = "layout_support_" + getDescriptorTypeShortName(descriptorType) +
                                                     "_" + getLayoutTopologyName(topology) + "_" +
                                                     getBindingFlagsModeName(flagsMode) + immutableSuffix;
                        const LayoutSupportMatrixParams params{descriptorType, topology, flagsMode,
                                                               useImmutableSamplers};

                        addFunctionCase(main3Tests.get(), caseName.c_str(), checkSupportLayoutSupportMatrix,
                                        testLayoutSupportMatrix, params);
                    }
                }
            }
        }
    }

#ifndef CTS_USES_VULKANSC
    // Sub-Family B: VkDescriptorSetLayoutCreateFlags interactions
    {
        struct NamedCreateFlagsVariant
        {
            const char *name;
            CreateFlagsVariant variant;
        };

        const NamedCreateFlagsVariant createFlagsCases[] = {
            {"layout_support_create_flags_push_descriptor_empty", CreateFlagsVariant::PUSH_DESCRIPTOR_EMPTY},
            {"layout_support_create_flags_push_descriptor_uniform_buffer",
             CreateFlagsVariant::PUSH_DESCRIPTOR_UNIFORM_BUFFER},
            {"layout_support_create_flags_push_descriptor_combined_image_sampler_immutable",
             CreateFlagsVariant::PUSH_DESCRIPTOR_COMBINED_IMAGE_SAMPLER_IMMUTABLE},
            {"layout_support_create_flags_push_descriptor_descending_partially_bound",
             CreateFlagsVariant::PUSH_DESCRIPTOR_DESCENDING_PARTIALLY_BOUND},
            {"layout_support_create_flags_push_descriptor_max_push_descriptors",
             CreateFlagsVariant::PUSH_DESCRIPTOR_MAX_PUSH_DESCRIPTORS},
            {"layout_support_create_flags_host_only_pool_uniform_buffer_no_variable",
             CreateFlagsVariant::HOST_ONLY_POOL_UNIFORM_BUFFER_NO_VARIABLE},
            {"layout_support_create_flags_host_only_pool_uniform_buffer_variable_count",
             CreateFlagsVariant::HOST_ONLY_POOL_UNIFORM_BUFFER_VARIABLE_COUNT},
            {"layout_support_create_flags_host_only_pool_mutable_ext_no_variable",
             CreateFlagsVariant::HOST_ONLY_POOL_MUTABLE_EXT_NO_VARIABLE},
            {"layout_support_create_flags_host_only_pool_mutable_ext_variable_count",
             CreateFlagsVariant::HOST_ONLY_POOL_MUTABLE_EXT_VARIABLE_COUNT},
            {"layout_support_create_flags_descriptor_buffer_empty", CreateFlagsVariant::DESCRIPTOR_BUFFER_EMPTY},
            {"layout_support_create_flags_descriptor_buffer_uniform_buffer_no_variable",
             CreateFlagsVariant::DESCRIPTOR_BUFFER_UNIFORM_BUFFER_NO_VARIABLE},
            {"layout_support_create_flags_descriptor_buffer_uniform_buffer_variable_count",
             CreateFlagsVariant::DESCRIPTOR_BUFFER_UNIFORM_BUFFER_VARIABLE_COUNT},
            {"layout_support_create_flags_descriptor_buffer_sampled_image_variable_count",
             CreateFlagsVariant::DESCRIPTOR_BUFFER_SAMPLED_IMAGE_VARIABLE_COUNT},
            {"layout_support_create_flags_descriptor_buffer_inline_uniform_block_variable_count",
             CreateFlagsVariant::DESCRIPTOR_BUFFER_INLINE_UNIFORM_BLOCK_VARIABLE_COUNT},
            {"layout_support_create_flags_descriptor_buffer_embedded_immutable_samplers",
             CreateFlagsVariant::DESCRIPTOR_BUFFER_EMBEDDED_IMMUTABLE_SAMPLERS},
            {"layout_support_create_flags_descriptor_buffer_embedded_immutable_samplers_sparse_zero_count",
             CreateFlagsVariant::DESCRIPTOR_BUFFER_EMBEDDED_IMMUTABLE_SAMPLERS_SPARSE_ZERO_COUNT},
        };

        for (const auto &entry : createFlagsCases)
        {
            addFunctionCase(main3Tests.get(), entry.name, checkSupportCreateFlagsVariant, testCreateFlagsVariant,
                            entry.variant);
        }
    }
#endif // CTS_USES_VULKANSC

    // Sub-Family C: Edge, boundary, custom-device, and KHR entry-point cases
    {
        struct NamedEdgeCase
        {
            const char *name;
            EdgeCaseVariant variant;
        };

        const NamedEdgeCase edgeCases[] = {
            {"layout_support_edge_empty_layout", EdgeCaseVariant::EMPTY_LAYOUT},
            {"layout_support_edge_all_zero_descriptor_counts_no_variable",
             EdgeCaseVariant::ALL_ZERO_DESCRIPTOR_COUNTS_NO_VARIABLE},
            {"layout_support_edge_zero_count_variable_binding_create_layout",
             EdgeCaseVariant::ZERO_COUNT_VARIABLE_BINDING_CREATE_LAYOUT},
#ifndef CTS_USES_VULKANSC
            {"layout_support_edge_zero_count_variable_inline_uniform_block",
             EdgeCaseVariant::ZERO_COUNT_VARIABLE_INLINE_UNIFORM_BLOCK},
            {"layout_support_edge_mutable_ext_mixed_types_no_variable",
             EdgeCaseVariant::MUTABLE_EXT_MIXED_TYPES_NO_VARIABLE},
            {"layout_support_edge_mutable_ext_mixed_types_variable_count",
             EdgeCaseVariant::MUTABLE_EXT_MIXED_TYPES_VARIABLE_COUNT},
#endif // CTS_USES_VULKANSC
            {"layout_support_edge_over_limit_non_variable_uint32_max",
             EdgeCaseVariant::OVER_LIMIT_NON_VARIABLE_UINT32_MAX},
            {"layout_support_edge_over_limit_dynamic_buffers", EdgeCaseVariant::OVER_LIMIT_DYNAMIC_BUFFERS},
            {"layout_support_edge_variable_feature_disabled_custom_device",
             EdgeCaseVariant::VARIABLE_FEATURE_DISABLED_CUSTOM_DEVICE},
#ifndef CTS_USES_VULKANSC
            {"layout_support_edge_khr_entry_point", EdgeCaseVariant::KHR_ENTRY_POINT},
#endif // CTS_USES_VULKANSC
        };

        for (const auto &entry : edgeCases)
        {
            addFunctionCase(main3Tests.get(), entry.name, checkSupportEdgeCase, testEdgeCase, entry.variant);
        }
    }

    return main3Tests.release();
}

} // namespace api
} // namespace vkt
