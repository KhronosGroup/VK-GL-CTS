/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2026 The Khronos Group Inc.
 * Copyright (c) 2026 Valve Corporation.
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
* \brief Descriptor Indexing pNext tests
*//*--------------------------------------------------------------------*/

#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"

#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"

#include <cstring>
#include <optional>
#include <memory>
#include <vector>
#include <thread>

namespace vkt
{
namespace DescriptorIndexing
{

namespace
{

using namespace vk;

namespace PipelineLayoutPnext
{

enum class LayoutType
{
    REGULAR = 0, // Regular descriptor set layout with a few reasonable bindings.
    PUSH,        // Like REGULAR, but with the push flag.
    EMPTY,       // Layout without bindings.
    SAMPLER,     // Single binding, containing a sampler.
};

struct Params
{
    LayoutType layoutType;
    std::optional<LayoutType> secondLayout;
    bool separateThread; // Create the pipeline layout from a different thread.
    bool partiallyBound; // Use non-zero flags for each binding (VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT).
};
using ParamsPtr = std::shared_ptr<const Params>;

void checkSupport(Context &context, ParamsPtr params)
{
    if (params->layoutType == LayoutType::PUSH ||
        (params->secondLayout.has_value() && params->secondLayout.value() == LayoutType::PUSH))
        context.requireDeviceFunctionality("VK_KHR_push_descriptor");
    context.requireDeviceFunctionality("VK_EXT_descriptor_indexing");

    if (params->partiallyBound)
    {
        const auto &descriptorIndexingFeatures = context.getDescriptorIndexingFeatures();
        if (!descriptorIndexingFeatures.descriptorBindingPartiallyBound)
            TCU_THROW(NotSupportedError, "descriptorBindingPartiallyBound not supported");
    }
}

void addBindings(std::vector<VkDescriptorSetLayoutBinding> &out, LayoutType layoutType)
{
    const auto stageFlags = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_FRAGMENT_BIT);

    if (layoutType == LayoutType::REGULAR || layoutType == LayoutType::PUSH)
    {
        out.reserve(4u);
        out.push_back(VkDescriptorSetLayoutBinding{
            de::sizeU32(out),
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            2u,
            stageFlags,
            nullptr,
        });
        out.push_back(VkDescriptorSetLayoutBinding{
            de::sizeU32(out),
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            8u,
            stageFlags,
            nullptr,
        });
        out.push_back(VkDescriptorSetLayoutBinding{
            de::sizeU32(out),
            VK_DESCRIPTOR_TYPE_SAMPLER,
            1u,
            stageFlags,
            nullptr,
        });
        out.push_back(VkDescriptorSetLayoutBinding{
            de::sizeU32(out),
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            2u,
            stageFlags,
            nullptr,
        });
    }
    else if (layoutType == LayoutType::SAMPLER)
    {
        out.reserve(1u);
        out.push_back(VkDescriptorSetLayoutBinding{
            de::sizeU32(out),
            VK_DESCRIPTOR_TYPE_SAMPLER,
            1u,
            stageFlags,
            nullptr,
        });
    }
    else if (layoutType == LayoutType::EMPTY)
    {
    }
    else
        DE_ASSERT(false);
}

VkDescriptorSetLayoutCreateFlags getCreateFlags(LayoutType layoutType)
{
    VkDescriptorSetLayoutCreateFlags flags = 0u;
    if (layoutType == LayoutType::PUSH)
        flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT;
    return flags;
}

void trashMemory(VkDescriptorSetLayoutBindingFlagsCreateInfo *bindingFlags)
{
    if (bindingFlags->bindingCount > 0u)
    {
        void *dataPtr = reinterpret_cast<void *>(const_cast<VkDescriptorBindingFlags *>(bindingFlags->pBindingFlags));
        memset(dataPtr, 0xFF, sizeof(VkDescriptorBindingFlags) * bindingFlags->bindingCount);
    }
    memset(bindingFlags, 0xFF, sizeof(*bindingFlags));
}

void trashMemory(VkDescriptorSetLayoutCreateInfo *setLayoutCreateInfo)
{
    if (setLayoutCreateInfo->bindingCount > 0u)
    {
        void *dataPtr =
            reinterpret_cast<void *>(const_cast<VkDescriptorSetLayoutBinding *>(setLayoutCreateInfo->pBindings));
        memset(dataPtr, 0xFF, sizeof(VkDescriptorSetLayoutBinding) * setLayoutCreateInfo->bindingCount);
    }
    memset(setLayoutCreateInfo, 0xFF, sizeof(*setLayoutCreateInfo));
}

void makeCustomSetLayouts(Move<VkDescriptorSetLayout> *setLayout0, Move<VkDescriptorSetLayout> *setLayout1,
                          ParamsPtr params, const DeviceInterface &vkd, VkDevice device)
{
    DE_ASSERT(setLayout0);
    DE_ASSERT(setLayout1);
    const bool hasSecondLayout = params->secondLayout.has_value();

    std::vector<VkDescriptorSetLayoutBinding> bindings0;
    std::vector<VkDescriptorSetLayoutBinding> bindings1;

    addBindings(bindings0, params->layoutType);
    if (hasSecondLayout)
        addBindings(bindings1, params->secondLayout.value());

    VkDescriptorSetLayoutBindingFlagsCreateInfo stackFlagsCreateInfo[2];
    std::unique_ptr<VkDescriptorSetLayoutBindingFlagsCreateInfo[]> heapArray;
    size_t heapIndexOffset = 200000;

    VkDescriptorSetLayoutBindingFlagsCreateInfo *bindingFlags0 = nullptr;

    if (params->separateThread)
    {
        // Make this structure be on the stack of this thread.
        bindingFlags0 = &stackFlagsCreateInfo[0];
    }
    else
    {
        // Allocate some MBs (less than 50) in the heap for the VkDescriptorSetLayoutBindingFlagsCreateInfo structures.
        heapArray.reset(new VkDescriptorSetLayoutBindingFlagsCreateInfo[1000000]);
        bindingFlags0 = &heapArray[heapIndexOffset];
    }

    VkDescriptorBindingFlags generalBindingFlags = 0u;
    if (params->partiallyBound)
        generalBindingFlags |= VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

    std::vector<VkDescriptorBindingFlags> bindingFlagsVec0(de::sizeU32(bindings0), generalBindingFlags);
    *bindingFlags0               = initVulkanStructure();
    bindingFlags0->bindingCount  = de::sizeU32(bindingFlagsVec0);
    bindingFlags0->pBindingFlags = de::dataOrNull(bindingFlagsVec0);

    VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo0{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        bindingFlags0,
        getCreateFlags(params->layoutType),
        de::sizeU32(bindings0),
        de::dataOrNull(bindings0),
    };

    *setLayout0 = createDescriptorSetLayout(vkd, device, &setLayoutCreateInfo0);

    // Trash structure memory after creating the layout.
    trashMemory(bindingFlags0);
    trashMemory(&setLayoutCreateInfo0);

    if (hasSecondLayout)
    {
        VkDescriptorSetLayoutBindingFlagsCreateInfo *bindingFlags1 = nullptr;
        if (params->separateThread)
            bindingFlags1 = &stackFlagsCreateInfo[1];
        else
            bindingFlags1 = &heapArray[heapIndexOffset * 2u];

        std::vector<VkDescriptorBindingFlags> bindingFlagsVec1(de::sizeU32(bindings1), generalBindingFlags);
        *bindingFlags1               = initVulkanStructure();
        bindingFlags1->bindingCount  = de::sizeU32(bindingFlagsVec1);
        bindingFlags1->pBindingFlags = de::dataOrNull(bindingFlagsVec1);

        VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo1{
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            bindingFlags1,
            getCreateFlags(params->secondLayout.value()),
            de::sizeU32(bindings1),
            de::dataOrNull(bindings1),
        };

        *setLayout1 = createDescriptorSetLayout(vkd, device, &setLayoutCreateInfo1);

        // Trash structure memory after creating the layout.
        trashMemory(bindingFlags1);
        trashMemory(&setLayoutCreateInfo1);
    }
}

tcu::TestStatus runTest(Context &context, ParamsPtr params)
{
    const auto ctx = context.getContextCommonData();

    Move<VkDescriptorSetLayout> setLayout0;
    Move<VkDescriptorSetLayout> setLayout1;

    // Lambda with captures that can be used by this thread or a separate thread.
    const auto makeCustomSetLayoutsFunctor = [&setLayout0, &setLayout1, &params, &ctx]()
    { makeCustomSetLayouts(&setLayout0, &setLayout1, params, ctx.vkd, ctx.device); };

    if (params->separateThread)
    {
        // Create the layouts in a separate thread, so all the structures are gone when it finishes.
        std::thread creationThread(makeCustomSetLayoutsFunctor);
        creationThread.join();
    }
    else
        makeCustomSetLayoutsFunctor();

    const bool hasSecondLayout = params->secondLayout.has_value();
    std::vector<VkDescriptorSetLayout> setLayoutHandles;

    setLayoutHandles.reserve(2u);
    setLayoutHandles.push_back(*setLayout0);
    if (hasSecondLayout)
        setLayoutHandles.push_back(*setLayout1);

    Move<VkPipelineLayout> pipelineLayout;

    const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        nullptr,
        0u,
        de::sizeU32(setLayoutHandles),
        de::dataOrNull(setLayoutHandles),
        0u,
        nullptr,
    };
    pipelineLayout = createPipelineLayout(ctx.vkd, ctx.device, &pipelineLayoutCreateInfo);

    // The important checks are done by createPipelineLayout. No need for further checks.

    return tcu::TestStatus::pass("Pass");
}

} // namespace PipelineLayoutPnext

} // anonymous namespace

void createDescriptorIndexingPnextTests(tcu::TestCaseGroup *pNextGroup)
{
    struct
    {
        PipelineLayoutPnext::LayoutType layoutType;
        const char *name;
    } layoutTypeCases[] = {
        {PipelineLayoutPnext::LayoutType::REGULAR, "regular"},
        {PipelineLayoutPnext::LayoutType::PUSH, "push"},
        {PipelineLayoutPnext::LayoutType::EMPTY, "empty"},
        {PipelineLayoutPnext::LayoutType::SAMPLER, "sampler"},
    };

    // Single layout tests.
    for (const auto layoutTypeCase : layoutTypeCases)
        for (const bool separateThread : {false, true})
            for (const bool partiallyBound : {false, true})
            {
                PipelineLayoutPnext::ParamsPtr params(new PipelineLayoutPnext::Params{
                    layoutTypeCase.layoutType,
                    std::nullopt,
                    separateThread,
                    partiallyBound,
                });
                const auto testName = std::string("one_layout_") + layoutTypeCase.name +
                                      (separateThread ? "_thread" : "") + (partiallyBound ? "_partially_bound" : "");
                addFunctionCase(pNextGroup, testName, PipelineLayoutPnext::checkSupport, PipelineLayoutPnext::runTest,
                                params);
            }

    // Multi-layout tests.
    for (const auto layoutTypeCase0 : layoutTypeCases)
        for (const auto layoutTypeCase1 : layoutTypeCases)
        {
            // VUID-VkPipelineLayoutCreateInfo-pSetLayouts-00293
            const auto PUSH = PipelineLayoutPnext::LayoutType::PUSH;
            if (layoutTypeCase0.layoutType == PUSH && layoutTypeCase1.layoutType == PUSH)
                continue;

            for (const bool separateThread : {false, true})
                for (const bool partiallyBound : {false, true})
                {
                    PipelineLayoutPnext::ParamsPtr params(new PipelineLayoutPnext::Params{
                        layoutTypeCase0.layoutType,
                        std::make_optional(layoutTypeCase1.layoutType),
                        separateThread,
                        partiallyBound,
                    });
                    const auto testName = std::string("two_layouts_") + layoutTypeCase0.name + "_" +
                                          layoutTypeCase1.name + (separateThread ? "_thread" : "") +
                                          (partiallyBound ? "_partially_bound" : "");
                    addFunctionCase(pNextGroup, testName, PipelineLayoutPnext::checkSupport,
                                    PipelineLayoutPnext::runTest, params);
                }
        }
}

} // namespace DescriptorIndexing
} // namespace vkt
