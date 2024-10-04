/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
 * Copyright (c) 2023 LunarG, Inc.
 * Copyright (c) 2023 Nintendo
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
 * \brief Wrapper that can construct monolithic pipeline or use
          VK_EXT_graphics_pipeline_library for pipeline construction or use
          VK_EXT_shader_object for shader objects.
 *//*--------------------------------------------------------------------*/

#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "deSharedPtr.hpp"
#include "deSTLUtil.hpp"
#include "tcuVectorType.hpp"
#include "vkRefUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "tcuTextureUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkPipelineConstructionUtil.hpp"
#include "vkBarrierUtil.hpp"

#include <memory>
#include <set>
#include <map>

namespace vk
{

namespace
{

enum PipelineSetupState
{
    PSS_NONE                      = 0x00000000,
    PSS_VERTEX_INPUT_INTERFACE    = 0x00000001,
    PSS_PRE_RASTERIZATION_SHADERS = 0x00000002,
    PSS_FRAGMENT_SHADER           = 0x00000004,
    PSS_FRAGMENT_OUTPUT_INTERFACE = 0x00000008,
};

using TessellationDomainOriginStatePtr = std::unique_ptr<VkPipelineTessellationDomainOriginStateCreateInfo>;

} // anonymous namespace

static const VkVertexInputBindingDescription defaultVertexInputBindingDescription{
    0u,                          // uint32_t                                        binding
    sizeof(tcu::Vec4),           // uint32_t                                        stride
    VK_VERTEX_INPUT_RATE_VERTEX, // VkVertexInputRate                            inputRate
};

static const VkVertexInputAttributeDescription defaultVertexInputAttributeDescription{
    0u,                            // uint32_t                                        location
    0u,                            // uint32_t                                        binding
    VK_FORMAT_R32G32B32A32_SFLOAT, // VkFormat                                        format
    0u                             // uint32_t                                        offset
};

static const VkPipelineVertexInputStateCreateInfo defaultVertexInputState{
    VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType                                sType
    nullptr,                                                   // const void*                                    pNext
    (VkPipelineVertexInputStateCreateFlags)0,                  // VkPipelineVertexInputStateCreateFlags        flags
    1u, // uint32_t                                        vertexBindingDescriptionCount
    &defaultVertexInputBindingDescription, // const VkVertexInputBindingDescription*        pVertexBindingDescriptions
    1u, // uint32_t                                        vertexAttributeDescriptionCount
    &defaultVertexInputAttributeDescription // const VkVertexInputAttributeDescription*        pVertexAttributeDescriptions
};

static const VkStencilOpState defaultStencilOpState{
    VK_STENCIL_OP_KEEP,  // VkStencilOp                                    failOp
    VK_STENCIL_OP_KEEP,  // VkStencilOp                                    passOp
    VK_STENCIL_OP_KEEP,  // VkStencilOp                                    depthFailOp
    VK_COMPARE_OP_NEVER, // VkCompareOp                                    compareOp
    0u,                  // uint32_t                                        compareMask
    0u,                  // uint32_t                                        writeMask
    0u                   // uint32_t                                        reference
};

static const VkPipelineDepthStencilStateCreateInfo defaultDepthStencilState{
    VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, // VkStructureType                                sType
    nullptr,                                                    // const void*                                    pNext
    0u,                                                         // VkPipelineDepthStencilStateCreateFlags        flags
    VK_FALSE,                    // VkBool32                                        depthTestEnable
    VK_FALSE,                    // VkBool32                                        depthWriteEnable
    VK_COMPARE_OP_LESS_OR_EQUAL, // VkCompareOp                                    depthCompareOp
    VK_FALSE,                    // VkBool32                                        depthBoundsTestEnable
    VK_FALSE,                    // VkBool32                                        stencilTestEnable
    defaultStencilOpState,       // VkStencilOpState                                front
    defaultStencilOpState,       // VkStencilOpState                                back
    0.0f,                        // float                                        minDepthBounds
    1.0f,                        // float                                        maxDepthBounds
};

static const VkPipelineMultisampleStateCreateInfo defaultMultisampleState{
    VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType                                sType
    nullptr,                                                  // const void*                                    pNext
    0u,                                                       // VkPipelineMultisampleStateCreateFlags        flags
    VK_SAMPLE_COUNT_1_BIT, // VkSampleCountFlagBits                        rasterizationSamples
    VK_FALSE,              // VkBool32                                        sampleShadingEnable
    1.0f,                  // float                                        minSampleShading
    nullptr,               // const VkSampleMask*                            pSampleMask
    VK_FALSE,              // VkBool32                                        alphaToCoverageEnable
    VK_FALSE               // VkBool32                                        alphaToOneEnable
};

static const VkPipelineColorBlendAttachmentState defaultColorBlendAttachmentState{
    VK_FALSE,             // VkBool32                                        blendEnable
    VK_BLEND_FACTOR_ZERO, // VkBlendFactor                                srcColorBlendFactor
    VK_BLEND_FACTOR_ZERO, // VkBlendFactor                                dstColorBlendFactor
    VK_BLEND_OP_ADD,      // VkBlendOp                                    colorBlendOp
    VK_BLEND_FACTOR_ZERO, // VkBlendFactor                                srcAlphaBlendFactor
    VK_BLEND_FACTOR_ZERO, // VkBlendFactor                                dstAlphaBlendFactor
    VK_BLEND_OP_ADD,      // VkBlendOp                                    alphaBlendOp
    0xf                   // VkColorComponentFlags                        colorWriteMask
};

static const VkPipelineColorBlendStateCreateInfo defaultColorBlendState{
    VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType                                sType
    nullptr,                                                  // const void*                                    pNext
    0u,                                                       // VkPipelineColorBlendStateCreateFlags            flags
    VK_FALSE,                          // VkBool32                                        logicOpEnable
    VK_LOGIC_OP_CLEAR,                 // VkLogicOp                                    logicOp
    1u,                                // uint32_t                                        attachmentCount
    &defaultColorBlendAttachmentState, // const VkPipelineColorBlendAttachmentState*    pAttachments
    {0.0f, 0.0f, 0.0f, 0.0f}           // float                                        blendConstants[4]
};

namespace
{
#ifndef CTS_USES_VULKANSC
VkGraphicsPipelineLibraryCreateInfoEXT makeGraphicsPipelineLibraryCreateInfo(
    const VkGraphicsPipelineLibraryFlagsEXT flags)
{
    return {
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT, // VkStructureType sType;
        nullptr,                                                     // void* pNext;
        flags,                                                       // VkGraphicsPipelineLibraryFlagsEXT flags;
    };
}
#endif // CTS_USES_VULKANSC

Move<VkPipeline> makeGraphicsPipeline(const DeviceInterface &vk, VkDevice device, VkPipelineCache pipelineCache,
                                      const VkGraphicsPipelineCreateInfo *pCreateInfo,
                                      const VkAllocationCallbacks *pAllocator = nullptr)
{
    VkPipeline object  = VK_NULL_HANDLE;
    const auto retcode = vk.createGraphicsPipelines(device, pipelineCache, 1u, pCreateInfo, pAllocator, &object);

#ifndef CTS_USES_VULKANSC
    const bool allowCompileRequired =
        ((pCreateInfo->flags & VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT) != 0u);

    if (allowCompileRequired && retcode == VK_PIPELINE_COMPILE_REQUIRED)
        throw PipelineCompileRequiredError("createGraphicsPipelines returned VK_PIPELINE_COMPILE_REQUIRED");
#endif // CTS_USES_VULKANSC

    VK_CHECK(retcode);
    return Move<VkPipeline>(check<VkPipeline>(object), Deleter<VkPipeline>(vk, device, pAllocator));
}

} // namespace

bool isConstructionTypeLibrary(PipelineConstructionType pipelineConstructionType)
{
    return pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY ||
           pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_FAST_LINKED_LIBRARY;
}

bool isConstructionTypeShaderObject(PipelineConstructionType pipelineConstructionType)
{
    return pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_UNLINKED_SPIRV ||
           pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_UNLINKED_BINARY ||
           pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_LINKED_SPIRV ||
           pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_LINKED_BINARY;
}

void checkPipelineConstructionRequirements(const InstanceInterface &vki, VkPhysicalDevice physicalDevice,
                                           PipelineConstructionType pipelineConstructionType)
{
    if (pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
        return;

    const auto &supportedExtensions = enumerateCachedDeviceExtensionProperties(vki, physicalDevice);

    if (isConstructionTypeShaderObject(pipelineConstructionType))
    {
        if (!isExtensionStructSupported(supportedExtensions, RequiredExtension("VK_EXT_shader_object")))
            TCU_THROW(NotSupportedError, "VK_EXT_shader_object not supported");
        return;
    }

    if (!isExtensionStructSupported(supportedExtensions, RequiredExtension("VK_EXT_graphics_pipeline_library")))
        TCU_THROW(NotSupportedError, "VK_EXT_graphics_pipeline_library not supported");
}

PipelineCreateFlags2 translateCreateFlag(VkPipelineCreateFlags flagToTranslate)
{
    return (PipelineCreateFlags2)flagToTranslate;
}

void addToChain(void **structThatStartsChain, void *structToAddAtTheEnd)
{
    DE_ASSERT(structThatStartsChain);

    if (structToAddAtTheEnd == nullptr)
        return;

    // Cast to the base out structure which has a non-const pNext pointer.
    auto *structToAddAtTheEndCasted = reinterpret_cast<VkBaseOutStructure *>(structToAddAtTheEnd);

    // make sure that pNext pointer of structure that is added to chain is empty;
    // we are construting chains on our own and there are cases that use same
    // structure for multiple instances of GraphicsPipelineWrapper
    structToAddAtTheEndCasted->pNext = nullptr;

    uint32_t safetyCouter = 15u;
    void **structInChain  = structThatStartsChain;

    do
    {
        // check if this is free spot
        if (*structInChain == nullptr)
        {
            // attach new structure at the end
            *structInChain = structToAddAtTheEndCasted;
            return;
        }
        else if (*structInChain == structToAddAtTheEnd)
        {
            // struct is already in the chain
            return;
        }

        // Cast to the base out structure which has a non-const pNext pointer.
        auto *gpl = reinterpret_cast<VkBaseOutStructure *>(*structInChain);

        // move structure pointer one position down the pNext chain
        structInChain = reinterpret_cast<void **>(&gpl->pNext);
    } while (--safetyCouter);

    // probably safetyCouter is to small
    DE_ASSERT(false);
}

namespace
{
using PipelineShaderStageModuleIdPtr = std::unique_ptr<PipelineShaderStageModuleIdentifierCreateInfoWrapper>;
}

PipelineLayoutWrapper::PipelineLayoutWrapper(PipelineConstructionType pipelineConstructionType,
                                             const DeviceInterface &vk, VkDevice device,
                                             const std::vector<vk::Move<VkDescriptorSetLayout>> &descriptorSetLayout)
    : m_pipelineConstructionType(pipelineConstructionType)
    , m_vk(&vk)
    , m_device(device)
    , m_flags((VkPipelineLayoutCreateFlags)0u)
    , m_setLayoutCount((uint32_t)descriptorSetLayout.size())
    , m_pushConstantRangeCount(0u)
    , m_pushConstantRanges()
{
#ifndef CTS_USES_VULKANSC
    if (isConstructionTypeShaderObject(pipelineConstructionType))
        m_flags &= ~(VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT);
#endif

    m_setLayouts.resize(m_setLayoutCount);
    for (uint32_t i = 0; i < m_setLayoutCount; ++i)
        m_setLayouts[i] = *descriptorSetLayout[i];

    VkPipelineLayoutCreateInfo createInfo = vk::initVulkanStructure();
    createInfo.flags                      = m_flags;
    createInfo.setLayoutCount             = m_setLayoutCount;
    createInfo.pSetLayouts                = de::dataOrNull(m_setLayouts);
    createInfo.pushConstantRangeCount     = m_pushConstantRangeCount;
    createInfo.pPushConstantRanges        = de::dataOrNull(m_pushConstantRanges);
    m_pipelineLayout                      = createPipelineLayout(vk, device, &createInfo);
}

PipelineLayoutWrapper::PipelineLayoutWrapper(PipelineConstructionType pipelineConstructionType,
                                             const DeviceInterface &vk, VkDevice device, uint32_t setLayoutCount,
                                             const VkDescriptorSetLayout *descriptorSetLayout)
    : m_pipelineConstructionType(pipelineConstructionType)
    , m_vk(&vk)
    , m_device(device)
    , m_flags((VkPipelineLayoutCreateFlags)0u)
    , m_setLayoutCount(setLayoutCount)
    , m_pushConstantRangeCount(0u)
    , m_pushConstantRanges()
{
#ifndef CTS_USES_VULKANSC
    if (isConstructionTypeShaderObject(pipelineConstructionType))
        m_flags &= ~(VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT);
#endif

    m_setLayouts.resize(m_setLayoutCount);
    for (uint32_t i = 0; i < m_setLayoutCount; ++i)
        m_setLayouts[i] = descriptorSetLayout[i];

    VkPipelineLayoutCreateInfo createInfo = vk::initVulkanStructure();
    createInfo.flags                      = m_flags;
    createInfo.setLayoutCount             = m_setLayoutCount;
    createInfo.pSetLayouts                = de::dataOrNull(m_setLayouts);
    createInfo.pushConstantRangeCount     = m_pushConstantRangeCount;
    createInfo.pPushConstantRanges        = de::dataOrNull(m_pushConstantRanges);
    m_pipelineLayout                      = createPipelineLayout(vk, device, &createInfo);
}

PipelineLayoutWrapper::PipelineLayoutWrapper(PipelineConstructionType pipelineConstructionType,
                                             const DeviceInterface &vk, VkDevice device,
                                             const VkDescriptorSetLayout descriptorSetLayout,
                                             const VkPushConstantRange *pushConstantRange)
    : m_pipelineConstructionType(pipelineConstructionType)
    , m_vk(&vk)
    , m_device(device)
    , m_flags((VkPipelineLayoutCreateFlags)0u)
{
#ifndef CTS_USES_VULKANSC
    if (isConstructionTypeShaderObject(pipelineConstructionType))
        m_flags &= ~(VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT);
#endif

    if (descriptorSetLayout == VK_NULL_HANDLE)
    {
        m_setLayoutCount = 0;
    }
    else
    {
        m_setLayoutCount = 1;
        m_setLayouts.resize(1);
        m_setLayouts[0] = descriptorSetLayout;
    }
    if (pushConstantRange == nullptr)
    {
        m_pushConstantRangeCount = 0;
    }
    else
    {
        m_pushConstantRangeCount = 1;
        m_pushConstantRanges.resize(1);
        m_pushConstantRanges[0] = *pushConstantRange;
    }

    VkPipelineLayoutCreateInfo createInfo = vk::initVulkanStructure();
    createInfo.flags                      = m_flags;
    createInfo.setLayoutCount             = m_setLayoutCount;
    createInfo.pSetLayouts                = de::dataOrNull(m_setLayouts);
    createInfo.pushConstantRangeCount     = m_pushConstantRangeCount;
    createInfo.pPushConstantRanges        = de::dataOrNull(m_pushConstantRanges);
    m_pipelineLayout                      = createPipelineLayout(vk, device, &createInfo);
}

PipelineLayoutWrapper::PipelineLayoutWrapper(PipelineConstructionType pipelineConstructionType,
                                             const DeviceInterface &vk, VkDevice device,
                                             const VkPipelineLayoutCreateInfo *pCreateInfo,
                                             const VkAllocationCallbacks *)
    : m_pipelineConstructionType(pipelineConstructionType)
    , m_vk(&vk)
    , m_device(device)
    , m_flags(pCreateInfo->flags)
    , m_setLayoutCount(pCreateInfo->setLayoutCount)
    , m_pushConstantRangeCount(pCreateInfo->pushConstantRangeCount)
{
#ifndef CTS_USES_VULKANSC
    if (isConstructionTypeShaderObject(pipelineConstructionType))
        m_flags &= ~(VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT);
#endif

    m_setLayouts.resize(pCreateInfo->setLayoutCount);
    for (uint32_t i = 0; i < pCreateInfo->setLayoutCount; ++i)
        m_setLayouts[i] = pCreateInfo->pSetLayouts[i];
    m_pushConstantRanges.resize(pCreateInfo->pushConstantRangeCount);
    for (uint32_t i = 0; i < pCreateInfo->pushConstantRangeCount; ++i)
        m_pushConstantRanges[i] = pCreateInfo->pPushConstantRanges[i];

    VkPipelineLayoutCreateInfo createInfo = vk::initVulkanStructure();
    createInfo.flags                      = m_flags;
    createInfo.setLayoutCount             = m_setLayoutCount;
    createInfo.pSetLayouts                = m_setLayouts.data();
    createInfo.pushConstantRangeCount     = m_pushConstantRangeCount;
    createInfo.pPushConstantRanges        = m_pushConstantRanges.data();
    m_pipelineLayout                      = createPipelineLayout(vk, device, &createInfo);
}

PipelineLayoutWrapper::PipelineLayoutWrapper(PipelineConstructionType pipelineConstructionType,
                                             const DeviceInterface &vk, const VkDevice device,
                                             const uint32_t setLayoutCount,
                                             const VkDescriptorSetLayout *descriptorSetLayout,
                                             const uint32_t pushConstantRangeCount,
                                             const VkPushConstantRange *pPushConstantRanges,
                                             const VkPipelineLayoutCreateFlags flags)
    : m_pipelineConstructionType(pipelineConstructionType)
    , m_vk(&vk)
    , m_device(device)
    , m_flags(flags)
    , m_setLayoutCount(setLayoutCount)
    , m_pushConstantRangeCount(pushConstantRangeCount)
{
#ifndef CTS_USES_VULKANSC
    if (isConstructionTypeShaderObject(pipelineConstructionType))
        m_flags &= ~(VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT);
#endif

    m_setLayouts.resize(m_setLayoutCount);
    m_pushConstantRanges.resize(m_pushConstantRangeCount);
    for (uint32_t i = 0; i < m_setLayoutCount; ++i)
        m_setLayouts[i] = descriptorSetLayout[i];
    for (uint32_t i = 0; i < m_pushConstantRangeCount; ++i)
        m_pushConstantRanges[i] = pPushConstantRanges[i];

    VkPipelineLayoutCreateInfo createInfo = vk::initVulkanStructure();
    createInfo.flags                      = m_flags;
    createInfo.setLayoutCount             = m_setLayoutCount;
    createInfo.pSetLayouts                = m_setLayouts.data();
    createInfo.pushConstantRangeCount     = m_pushConstantRangeCount;
    createInfo.pPushConstantRanges        = m_pushConstantRanges.data();
    m_pipelineLayout                      = createPipelineLayout(vk, device, &createInfo);
}

PipelineLayoutWrapper::PipelineLayoutWrapper(PipelineLayoutWrapper &&rhs) noexcept
    : m_pipelineConstructionType(rhs.m_pipelineConstructionType)
    , m_vk(rhs.m_vk)
    , m_device(rhs.m_device)
    , m_flags(rhs.m_flags)
    , m_setLayoutCount(rhs.m_setLayoutCount)
    , m_setLayouts(std::move(rhs.m_setLayouts))
    , m_pushConstantRangeCount(rhs.m_pushConstantRangeCount)
    , m_pushConstantRanges(std::move(rhs.m_pushConstantRanges))
    , m_pipelineLayout(rhs.m_pipelineLayout)
{
}

PipelineLayoutWrapper &PipelineLayoutWrapper::operator=(PipelineLayoutWrapper &&rhs)
{
    m_pipelineConstructionType = rhs.m_pipelineConstructionType;
    m_vk                       = rhs.m_vk;
    m_device                   = rhs.m_device;
    m_flags                    = rhs.m_flags;
    m_setLayoutCount           = rhs.m_setLayoutCount;
    m_setLayouts               = std::move(rhs.m_setLayouts);
    m_pushConstantRangeCount   = rhs.m_pushConstantRangeCount;
    m_pushConstantRanges       = std::move(rhs.m_pushConstantRanges);
    m_pipelineLayout           = rhs.m_pipelineLayout;

    return *this;
}

void PipelineLayoutWrapper::bindDescriptorSets(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint,
                                               uint32_t firstSet, uint32_t descriptorSetCount,
                                               const VkDescriptorSet *pDescriptorSets, uint32_t dynamicOffsetCount,
                                               const uint32_t *pDynamicOffsets) const
{
    // if (!isConstructionTypeShaderObject(m_pipelineConstructionType))
    //      m_vk->cmdBindDescriptorSets2EXT(commandBuffer, &m_setLayouts[firstSet], vk::VK_SHADER_STAGE_ALL_GRAPHICS, firstSet, descriptorSetCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
    // else
    m_vk->cmdBindDescriptorSets(commandBuffer, pipelineBindPoint, *m_pipelineLayout, firstSet, descriptorSetCount,
                                pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
}

#ifndef CTS_USES_VULKANSC
RenderPassWrapper::SubpassDependency::SubpassDependency(const VkSubpassDependency &dependency)
    : srcSubpass(dependency.srcSubpass)
    , dstSubpass(dependency.dstSubpass)
    , srcStageMask(dependency.srcStageMask)
    , dstStageMask(dependency.dstStageMask)
    , srcAccessMask(dependency.srcAccessMask)
    , dstAccessMask(dependency.dstAccessMask)
    , dependencyFlags(dependency.dependencyFlags)
    , sync2(false)
{
}

RenderPassWrapper::SubpassDependency::SubpassDependency(const VkSubpassDependency2 &dependency)
    : srcSubpass(dependency.srcSubpass)
    , dstSubpass(dependency.dstSubpass)
    , srcStageMask(0u)
    , dstStageMask(0u)
    , srcAccessMask(0u)
    , dstAccessMask(0u)
    , dependencyFlags(dependency.dependencyFlags)
    , sync2(false)
{
    DE_ASSERT(dependency.viewOffset == 0);
    const auto memBarrier = findStructure<VkMemoryBarrier2>(dependency.pNext);
    if (memBarrier)
    {
        srcStageMask  = memBarrier->srcStageMask;
        dstStageMask  = memBarrier->dstStageMask;
        srcAccessMask = memBarrier->srcAccessMask;
        dstAccessMask = memBarrier->dstAccessMask;
        sync2         = true;
    }
    else
    {
        srcStageMask  = dependency.srcStageMask;
        dstStageMask  = dependency.dstStageMask;
        srcAccessMask = dependency.srcAccessMask;
        dstAccessMask = dependency.dstAccessMask;
    }
}
#endif // CTS_USES_VULKANSC

RenderPassWrapper::RenderPassWrapper(PipelineConstructionType pipelineConstructionType, const DeviceInterface &vk,
                                     VkDevice device, const VkRenderPassCreateInfo *pCreateInfo)
    : m_isDynamicRendering(vk::isConstructionTypeShaderObject(pipelineConstructionType))
#ifndef CTS_USES_VULKANSC
    , m_renderingInfo()
    , m_secondaryCommandBuffers(false)
#endif
{
    if (!m_isDynamicRendering)
    {
        m_renderPass = vk::createRenderPass(vk, device, pCreateInfo);
    }
    else
    {
#ifndef CTS_USES_VULKANSC
        const auto multiView = findStructure<VkRenderPassMultiviewCreateInfo>(pCreateInfo->pNext);
        if (multiView)
        {
            for (uint32_t i = 0; i < multiView->subpassCount; ++i)
                m_viewMasks.push_back(multiView->pViewMasks[i]);
        }

        m_attachments.resize(pCreateInfo->attachmentCount);
        m_layouts.resize(pCreateInfo->attachmentCount);
        for (uint32_t i = 0; i < pCreateInfo->attachmentCount; ++i)
        {
            m_attachments[i]                = vk::initVulkanStructure();
            m_attachments[i].flags          = pCreateInfo->pAttachments[i].flags;
            m_attachments[i].format         = pCreateInfo->pAttachments[i].format;
            m_attachments[i].samples        = pCreateInfo->pAttachments[i].samples;
            m_attachments[i].loadOp         = pCreateInfo->pAttachments[i].loadOp;
            m_attachments[i].storeOp        = pCreateInfo->pAttachments[i].storeOp;
            m_attachments[i].stencilLoadOp  = pCreateInfo->pAttachments[i].stencilLoadOp;
            m_attachments[i].stencilStoreOp = pCreateInfo->pAttachments[i].stencilStoreOp;
            m_attachments[i].initialLayout  = pCreateInfo->pAttachments[i].initialLayout;
            m_attachments[i].finalLayout    = pCreateInfo->pAttachments[i].finalLayout;
            m_layouts[i]                    = pCreateInfo->pAttachments[i].initialLayout;
        }

        m_subpasses.resize(pCreateInfo->subpassCount);
        for (uint32_t s = 0; s < pCreateInfo->subpassCount; ++s)
        {
            // Input attachments are not supported with dynamic rendering
            DE_ASSERT(pCreateInfo->pSubpasses[s].inputAttachmentCount == 0);
            auto &subpass = m_subpasses[s];
            subpass.m_colorAttachments.resize(pCreateInfo->pSubpasses[s].colorAttachmentCount);

            for (uint32_t i = 0; i < pCreateInfo->pSubpasses[s].colorAttachmentCount; ++i)
            {
                uint32_t j = pCreateInfo->pSubpasses[s].pColorAttachments[i].attachment;
                if (j < pCreateInfo->attachmentCount)
                {
                    subpass.m_colorAttachments[i].attachmentInfo = vk::initVulkanStructure();
                    subpass.m_colorAttachments[i].index          = j;
                    subpass.m_colorAttachments[i].format         = pCreateInfo->pAttachments[j].format;

                    subpass.m_colorAttachments[i].attachmentInfo.imageView = VK_NULL_HANDLE;
                    subpass.m_colorAttachments[i].attachmentInfo.imageLayout =
                        pCreateInfo->pSubpasses[s].pColorAttachments[i].layout;
                    subpass.m_colorAttachments[i].attachmentInfo.resolveMode        = vk::VK_RESOLVE_MODE_NONE;
                    subpass.m_colorAttachments[i].attachmentInfo.resolveImageView   = VK_NULL_HANDLE;
                    subpass.m_colorAttachments[i].attachmentInfo.resolveImageLayout = vk::VK_IMAGE_LAYOUT_UNDEFINED;
                    subpass.m_colorAttachments[i].attachmentInfo.loadOp     = pCreateInfo->pAttachments[j].loadOp;
                    subpass.m_colorAttachments[i].attachmentInfo.storeOp    = pCreateInfo->pAttachments[j].storeOp;
                    subpass.m_colorAttachments[i].attachmentInfo.clearValue = {};
                }
                else
                {
                    subpass.m_colorAttachments[i].index = VK_ATTACHMENT_UNUSED;
                }
            }

            if (pCreateInfo->pSubpasses[s].pDepthStencilAttachment != nullptr)
            {
                uint32_t j = pCreateInfo->pSubpasses[s].pDepthStencilAttachment->attachment;
                if (j < pCreateInfo->attachmentCount)
                {
                    subpass.m_depthStencilAttachment.attachmentInfo = vk::initVulkanStructure();
                    subpass.m_depthStencilAttachment.index          = j;
                    subpass.m_depthStencilAttachment.format         = pCreateInfo->pAttachments[j].format;

                    subpass.m_depthStencilAttachment.attachmentInfo.imageView = VK_NULL_HANDLE;
                    subpass.m_depthStencilAttachment.attachmentInfo.imageLayout =
                        pCreateInfo->pSubpasses[s].pDepthStencilAttachment->layout;
                    subpass.m_depthStencilAttachment.attachmentInfo.resolveMode        = vk::VK_RESOLVE_MODE_NONE;
                    subpass.m_depthStencilAttachment.attachmentInfo.resolveImageView   = VK_NULL_HANDLE;
                    subpass.m_depthStencilAttachment.attachmentInfo.resolveImageLayout = vk::VK_IMAGE_LAYOUT_UNDEFINED;
                    subpass.m_depthStencilAttachment.attachmentInfo.loadOp     = pCreateInfo->pAttachments[j].loadOp;
                    subpass.m_depthStencilAttachment.attachmentInfo.storeOp    = pCreateInfo->pAttachments[j].storeOp;
                    subpass.m_depthStencilAttachment.attachmentInfo.clearValue = {};
                    subpass.m_depthStencilAttachment.stencilLoadOp  = pCreateInfo->pAttachments[j].stencilLoadOp;
                    subpass.m_depthStencilAttachment.stencilStoreOp = pCreateInfo->pAttachments[j].stencilStoreOp;
                }
                else
                {
                    subpass.m_depthStencilAttachment.index = VK_ATTACHMENT_UNUSED;
                }
            }

            if (pCreateInfo->pSubpasses[s].pResolveAttachments != nullptr)
            {
                subpass.m_resolveAttachments.resize(pCreateInfo->pSubpasses[s].colorAttachmentCount);
                for (uint32_t i = 0; i < pCreateInfo->pSubpasses[s].colorAttachmentCount; ++i)
                {
                    uint32_t j = pCreateInfo->pSubpasses[s].pResolveAttachments[i].attachment;
                    if (j < pCreateInfo->attachmentCount)
                    {
                        subpass.m_resolveAttachments[i].attachmentInfo = vk::initVulkanStructure();
                        subpass.m_resolveAttachments[i].index          = j;
                        subpass.m_resolveAttachments[i].format         = pCreateInfo->pAttachments[j].format;

                        subpass.m_resolveAttachments[i].attachmentInfo.imageView = VK_NULL_HANDLE;
                        subpass.m_resolveAttachments[i].attachmentInfo.imageLayout =
                            pCreateInfo->pSubpasses[s].pResolveAttachments[i].layout;
                        subpass.m_resolveAttachments[i].attachmentInfo.resolveMode      = vk::VK_RESOLVE_MODE_NONE;
                        subpass.m_resolveAttachments[i].attachmentInfo.resolveImageView = VK_NULL_HANDLE;
                        subpass.m_resolveAttachments[i].attachmentInfo.resolveImageLayout =
                            vk::VK_IMAGE_LAYOUT_UNDEFINED;
                        subpass.m_resolveAttachments[i].attachmentInfo.loadOp  = pCreateInfo->pAttachments[j].loadOp;
                        subpass.m_resolveAttachments[i].attachmentInfo.storeOp = pCreateInfo->pAttachments[j].storeOp;
                        subpass.m_resolveAttachments[i].attachmentInfo.clearValue = {};
                    }
                    else
                    {
                        subpass.m_resolveAttachments[i].index = VK_ATTACHMENT_UNUSED;
                    }
                }
            }
        }

        m_dependencies.reserve(pCreateInfo->dependencyCount);
        for (uint32_t depIdx = 0u; depIdx < pCreateInfo->dependencyCount; ++depIdx)
            m_dependencies.emplace_back(pCreateInfo->pDependencies[depIdx]);
#endif
    }
}

RenderPassWrapper::RenderPassWrapper(PipelineConstructionType pipelineConstructionType, const DeviceInterface &vk,
                                     VkDevice device, const VkRenderPassCreateInfo2 *pCreateInfo)
    : RenderPassWrapper(vk, device, pCreateInfo, isConstructionTypeShaderObject(pipelineConstructionType))
{
}

RenderPassWrapper::RenderPassWrapper(const DeviceInterface &vk, VkDevice device,
                                     const VkRenderPassCreateInfo2 *pCreateInfo, bool dynamicRendering)
    : m_isDynamicRendering(dynamicRendering)
#ifndef CTS_USES_VULKANSC
    , m_renderingInfo()
    , m_secondaryCommandBuffers(false)
#endif
{

    if (!m_isDynamicRendering)
    {
        m_renderPass = vk::createRenderPass2(vk, device, pCreateInfo);
    }
    else
    {
#ifndef CTS_USES_VULKANSC
        const auto multiView = findStructure<VkRenderPassMultiviewCreateInfo>(pCreateInfo->pNext);
        if (multiView)
        {
            for (uint32_t i = 0; i < multiView->subpassCount; ++i)
                m_viewMasks.push_back(multiView->pViewMasks[i]);
        }

        m_attachments.resize(pCreateInfo->attachmentCount);
        m_layouts.resize(pCreateInfo->attachmentCount);
        for (uint32_t i = 0; i < pCreateInfo->attachmentCount; ++i)
        {
            m_attachments[i] = pCreateInfo->pAttachments[i];
            m_layouts[i]     = pCreateInfo->pAttachments[i].initialLayout;
        }

        m_subpasses.resize(pCreateInfo->subpassCount);
        for (uint32_t s = 0; s < pCreateInfo->subpassCount; ++s)
        {
            // Input attachments are not supported with dynamic rendering
            DE_ASSERT(pCreateInfo->pSubpasses[s].inputAttachmentCount == 0);
            auto &subpass = m_subpasses[s];
            subpass.m_colorAttachments.resize(pCreateInfo->pSubpasses[s].colorAttachmentCount);

            const auto msrtss =
                findStructure<VkMultisampledRenderToSingleSampledInfoEXT>(pCreateInfo->pSubpasses[s].pNext);
            if (msrtss)
                subpass.m_msrtss = *msrtss;

            const auto dsr = findStructure<VkSubpassDescriptionDepthStencilResolve>(pCreateInfo->pSubpasses[s].pNext);
            if (dsr)
            {
                subpass.m_dsr = *dsr;
                if (dsr->pDepthStencilResolveAttachment)
                {
                    subpass.m_depthStencilResolveAttachment      = *dsr->pDepthStencilResolveAttachment;
                    subpass.m_dsr.pDepthStencilResolveAttachment = &subpass.m_depthStencilResolveAttachment;
                }
            }

            for (uint32_t i = 0; i < pCreateInfo->pSubpasses[s].colorAttachmentCount; ++i)
            {
                uint32_t j = pCreateInfo->pSubpasses[s].pColorAttachments[i].attachment;
                if (j < pCreateInfo->attachmentCount)
                {
                    subpass.m_colorAttachments[i].attachmentInfo = vk::initVulkanStructure();
                    subpass.m_colorAttachments[i].index          = j;
                    subpass.m_colorAttachments[i].format         = pCreateInfo->pAttachments[j].format;

                    subpass.m_colorAttachments[i].attachmentInfo.imageView = VK_NULL_HANDLE;
                    subpass.m_colorAttachments[i].attachmentInfo.imageLayout =
                        pCreateInfo->pSubpasses[s].pColorAttachments[i].layout;
                    subpass.m_colorAttachments[i].attachmentInfo.resolveMode        = vk::VK_RESOLVE_MODE_NONE;
                    subpass.m_colorAttachments[i].attachmentInfo.resolveImageView   = VK_NULL_HANDLE;
                    subpass.m_colorAttachments[i].attachmentInfo.resolveImageLayout = vk::VK_IMAGE_LAYOUT_UNDEFINED;
                    subpass.m_colorAttachments[i].attachmentInfo.loadOp     = pCreateInfo->pAttachments[j].loadOp;
                    subpass.m_colorAttachments[i].attachmentInfo.storeOp    = pCreateInfo->pAttachments[j].storeOp;
                    subpass.m_colorAttachments[i].attachmentInfo.clearValue = {};
                }
                else
                {
                    subpass.m_colorAttachments[i].index = VK_ATTACHMENT_UNUSED;
                }
            }

            if (pCreateInfo->pSubpasses[s].pDepthStencilAttachment != nullptr)
            {
                uint32_t j = pCreateInfo->pSubpasses[s].pDepthStencilAttachment->attachment;
                if (j < pCreateInfo->attachmentCount)
                {
                    subpass.m_depthStencilAttachment.attachmentInfo = vk::initVulkanStructure();
                    subpass.m_depthStencilAttachment.index          = j;
                    subpass.m_depthStencilAttachment.format         = pCreateInfo->pAttachments[j].format;

                    subpass.m_depthStencilAttachment.attachmentInfo.imageView = VK_NULL_HANDLE;
                    subpass.m_depthStencilAttachment.attachmentInfo.imageLayout =
                        pCreateInfo->pSubpasses[s].pDepthStencilAttachment->layout;
                    subpass.m_depthStencilAttachment.attachmentInfo.resolveMode        = vk::VK_RESOLVE_MODE_NONE;
                    subpass.m_depthStencilAttachment.attachmentInfo.resolveImageView   = VK_NULL_HANDLE;
                    subpass.m_depthStencilAttachment.attachmentInfo.resolveImageLayout = vk::VK_IMAGE_LAYOUT_UNDEFINED;
                    subpass.m_depthStencilAttachment.attachmentInfo.loadOp     = pCreateInfo->pAttachments[j].loadOp;
                    subpass.m_depthStencilAttachment.attachmentInfo.storeOp    = pCreateInfo->pAttachments[j].storeOp;
                    subpass.m_depthStencilAttachment.attachmentInfo.clearValue = {};
                    subpass.m_depthStencilAttachment.stencilLoadOp  = pCreateInfo->pAttachments[j].stencilLoadOp;
                    subpass.m_depthStencilAttachment.stencilStoreOp = pCreateInfo->pAttachments[j].stencilStoreOp;
                }
                else
                {
                    subpass.m_depthStencilAttachment.index = VK_ATTACHMENT_UNUSED;
                }
            }

            if (pCreateInfo->pSubpasses[s].pResolveAttachments != nullptr)
            {
                subpass.m_resolveAttachments.resize(pCreateInfo->pSubpasses[s].colorAttachmentCount);
                for (uint32_t i = 0; i < pCreateInfo->pSubpasses[s].colorAttachmentCount; ++i)
                {
                    uint32_t j = pCreateInfo->pSubpasses[s].pResolveAttachments[i].attachment;
                    if (j < pCreateInfo->attachmentCount)
                    {
                        subpass.m_resolveAttachments[i].attachmentInfo = vk::initVulkanStructure();
                        subpass.m_resolveAttachments[i].index          = j;
                        subpass.m_resolveAttachments[i].format         = pCreateInfo->pAttachments[j].format;

                        subpass.m_resolveAttachments[i].attachmentInfo.imageView = VK_NULL_HANDLE;
                        subpass.m_resolveAttachments[i].attachmentInfo.imageLayout =
                            pCreateInfo->pSubpasses[s].pResolveAttachments[i].layout;
                        subpass.m_resolveAttachments[i].attachmentInfo.resolveMode      = vk::VK_RESOLVE_MODE_NONE;
                        subpass.m_resolveAttachments[i].attachmentInfo.resolveImageView = VK_NULL_HANDLE;
                        subpass.m_resolveAttachments[i].attachmentInfo.resolveImageLayout =
                            vk::VK_IMAGE_LAYOUT_UNDEFINED;
                        subpass.m_resolveAttachments[i].attachmentInfo.loadOp  = pCreateInfo->pAttachments[j].loadOp;
                        subpass.m_resolveAttachments[i].attachmentInfo.storeOp = pCreateInfo->pAttachments[j].storeOp;
                        subpass.m_resolveAttachments[i].attachmentInfo.clearValue = {};
                    }
                    else
                    {
                        subpass.m_resolveAttachments[i].index = VK_ATTACHMENT_UNUSED;
                    }
                }
            }
        }

        m_dependencies.reserve(pCreateInfo->dependencyCount);
        for (uint32_t depIdx = 0u; depIdx < pCreateInfo->dependencyCount; ++depIdx)
            m_dependencies.emplace_back(pCreateInfo->pDependencies[depIdx]);
#endif
    }
}

RenderPassWrapper::RenderPassWrapper(PipelineConstructionType pipelineConstructionType, const DeviceInterface &vk,
                                     const VkDevice device, const VkFormat colorFormat,
                                     const VkFormat depthStencilFormat, const VkAttachmentLoadOp loadOperation,
                                     const VkImageLayout finalLayoutColor, const VkImageLayout finalLayoutDepthStencil,
                                     const VkImageLayout subpassLayoutColor,
                                     const VkImageLayout subpassLayoutDepthStencil,
                                     const VkAllocationCallbacks *const allocationCallbacks)
    : m_isDynamicRendering(isConstructionTypeShaderObject(pipelineConstructionType))
#ifndef CTS_USES_VULKANSC
    , m_renderingInfo()
#endif
{

    if (!m_isDynamicRendering)
    {
        m_renderPass = vk::makeRenderPass(vk, device, colorFormat, depthStencilFormat, loadOperation, finalLayoutColor,
                                          finalLayoutDepthStencil, subpassLayoutColor, subpassLayoutDepthStencil,
                                          allocationCallbacks);
    }
    else
    {
#ifndef CTS_USES_VULKANSC
        const bool hasColor                           = colorFormat != VK_FORMAT_UNDEFINED;
        const bool hasDepthStencil                    = depthStencilFormat != VK_FORMAT_UNDEFINED;
        const VkImageLayout initialLayoutColor        = loadOperation == VK_ATTACHMENT_LOAD_OP_LOAD ?
                                                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL :
                                                            VK_IMAGE_LAYOUT_UNDEFINED;
        const VkImageLayout initialLayoutDepthStencil = loadOperation == VK_ATTACHMENT_LOAD_OP_LOAD ?
                                                            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL :
                                                            VK_IMAGE_LAYOUT_UNDEFINED;

        m_subpasses.resize(1);
        auto &subpass = m_subpasses[0];

        if (hasColor)
        {
            subpass.m_colorAttachments.resize(1);
            subpass.m_colorAttachments[0].attachmentInfo = vk::initVulkanStructure();
            subpass.m_colorAttachments[0].index          = 0u;
            subpass.m_colorAttachments[0].format         = colorFormat;

            subpass.m_colorAttachments[0].attachmentInfo.imageView          = VK_NULL_HANDLE;
            subpass.m_colorAttachments[0].attachmentInfo.imageLayout        = subpassLayoutColor;
            subpass.m_colorAttachments[0].attachmentInfo.resolveMode        = vk::VK_RESOLVE_MODE_NONE;
            subpass.m_colorAttachments[0].attachmentInfo.resolveImageView   = VK_NULL_HANDLE;
            subpass.m_colorAttachments[0].attachmentInfo.resolveImageLayout = vk::VK_IMAGE_LAYOUT_UNDEFINED;
            subpass.m_colorAttachments[0].attachmentInfo.loadOp             = loadOperation;
            subpass.m_colorAttachments[0].attachmentInfo.storeOp            = VK_ATTACHMENT_STORE_OP_STORE;
            subpass.m_colorAttachments[0].attachmentInfo.clearValue         = {};

            const VkAttachmentDescription2 colorAttachmentDescription = {
                VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
                nullptr,
                (VkAttachmentDescriptionFlags)0,  // VkAttachmentDescriptionFlags    flags
                colorFormat,                      // VkFormat                        format
                VK_SAMPLE_COUNT_1_BIT,            // VkSampleCountFlagBits           samples
                loadOperation,                    // VkAttachmentLoadOp              loadOp
                VK_ATTACHMENT_STORE_OP_STORE,     // VkAttachmentStoreOp             storeOp
                VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // VkAttachmentLoadOp              stencilLoadOp
                VK_ATTACHMENT_STORE_OP_DONT_CARE, // VkAttachmentStoreOp             stencilStoreOp
                initialLayoutColor,               // VkImageLayout                   initialLayout
                finalLayoutColor                  // VkImageLayout                   finalLayout
            };
            m_attachments.push_back(colorAttachmentDescription);
            m_layouts.push_back(colorAttachmentDescription.initialLayout);
        }
        if (hasDepthStencil)
        {
            subpass.m_depthStencilAttachment.attachmentInfo = vk::initVulkanStructure();
            subpass.m_depthStencilAttachment.index          = hasColor ? 1u : 0u;
            subpass.m_depthStencilAttachment.format         = depthStencilFormat;

            subpass.m_depthStencilAttachment.attachmentInfo.imageView          = VK_NULL_HANDLE;
            subpass.m_depthStencilAttachment.attachmentInfo.imageLayout        = subpassLayoutDepthStencil;
            subpass.m_depthStencilAttachment.attachmentInfo.resolveMode        = vk::VK_RESOLVE_MODE_NONE;
            subpass.m_depthStencilAttachment.attachmentInfo.resolveImageView   = VK_NULL_HANDLE;
            subpass.m_depthStencilAttachment.attachmentInfo.resolveImageLayout = vk::VK_IMAGE_LAYOUT_UNDEFINED;
            subpass.m_depthStencilAttachment.attachmentInfo.loadOp             = loadOperation;
            subpass.m_depthStencilAttachment.attachmentInfo.storeOp            = VK_ATTACHMENT_STORE_OP_STORE;
            subpass.m_depthStencilAttachment.attachmentInfo.clearValue         = {};
            subpass.m_depthStencilAttachment.stencilLoadOp                     = loadOperation;
            subpass.m_depthStencilAttachment.stencilStoreOp                    = VK_ATTACHMENT_STORE_OP_STORE;

            const VkAttachmentDescription2 depthStencilAttachmentDescription = {
                VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
                nullptr,
                (VkAttachmentDescriptionFlags)0, // VkAttachmentDescriptionFlags    flags
                depthStencilFormat,              // VkFormat                        format
                VK_SAMPLE_COUNT_1_BIT,           // VkSampleCountFlagBits           samples
                loadOperation,                   // VkAttachmentLoadOp              loadOp
                VK_ATTACHMENT_STORE_OP_STORE,    // VkAttachmentStoreOp             storeOp
                loadOperation,                   // VkAttachmentLoadOp              stencilLoadOp
                VK_ATTACHMENT_STORE_OP_STORE,    // VkAttachmentStoreOp             stencilStoreOp
                initialLayoutDepthStencil,       // VkImageLayout                   initialLayout
                finalLayoutDepthStencil          // VkImageLayout                   finalLayout
            };
            m_attachments.push_back(depthStencilAttachmentDescription);
            m_layouts.push_back(depthStencilAttachmentDescription.initialLayout);
        }
#endif
    }
}

RenderPassWrapper::RenderPassWrapper(RenderPassWrapper &&rhs) noexcept
    : m_isDynamicRendering(rhs.m_isDynamicRendering)
    , m_renderPass(rhs.m_renderPass)
    , m_framebuffer(rhs.m_framebuffer)
#ifndef CTS_USES_VULKANSC
    , m_subpasses(std::move(rhs.m_subpasses))
    , m_dependencies(std::move(rhs.m_dependencies))
    , m_attachments(std::move(rhs.m_attachments))
    , m_images(std::move(rhs.m_images))
    , m_imageViews(std::move(rhs.m_imageViews))
    , m_clearValues(std::move(rhs.m_clearValues))
    , m_layouts(std::move(rhs.m_layouts))
    , m_activeSubpass(rhs.m_activeSubpass)
    , m_renderingInfo(rhs.m_renderingInfo)
    , m_layers(rhs.m_layers)
    , m_viewMasks(std::move(rhs.m_viewMasks))
    , m_secondaryCommandBuffers(rhs.m_secondaryCommandBuffers)
#endif
{
}

RenderPassWrapper &RenderPassWrapper::operator=(RenderPassWrapper &&rhs) noexcept
{
    m_isDynamicRendering = rhs.m_isDynamicRendering;
    m_renderPass         = rhs.m_renderPass;
    m_framebuffer        = rhs.m_framebuffer;
#ifndef CTS_USES_VULKANSC
    m_subpasses               = std::move(rhs.m_subpasses);
    m_dependencies            = std::move(rhs.m_dependencies);
    m_attachments             = std::move(rhs.m_attachments);
    m_images                  = std::move(rhs.m_images);
    m_imageViews              = std::move(rhs.m_imageViews);
    m_clearValues             = std::move(rhs.m_clearValues);
    m_layouts                 = std::move(rhs.m_layouts);
    m_activeSubpass           = rhs.m_activeSubpass;
    m_renderingInfo           = rhs.m_renderingInfo;
    m_layers                  = rhs.m_layers;
    m_viewMasks               = std::move(rhs.m_viewMasks);
    m_secondaryCommandBuffers = rhs.m_secondaryCommandBuffers;
#endif
    return *this;
}

#ifndef CTS_USES_VULKANSC

void RenderPassWrapper::clearAttachments(const DeviceInterface &vk, const VkCommandBuffer commandBuffer) const
{
    for (uint32_t i = 0; i < (uint32_t)m_attachments.size() && i < (uint32_t)m_clearValues.size(); ++i)
    {
        const auto tcuFormat  = vk::mapVkFormat(m_attachments[i].format);
        bool hasDepthAspect   = tcu::hasDepthComponent(tcuFormat.order);
        bool hasStencilAspect = tcu::hasStencilComponent(tcuFormat.order);

        if (m_attachments[i].loadOp != vk::VK_ATTACHMENT_LOAD_OP_CLEAR &&
            !(hasStencilAspect && m_attachments[i].stencilLoadOp == vk::VK_ATTACHMENT_LOAD_OP_CLEAR))
            continue;

        vk::VkRenderingInfo renderingInfo = vk::initVulkanStructure();
        renderingInfo.renderArea          = m_renderingInfo.renderArea;
        renderingInfo.layerCount          = m_renderingInfo.layerCount;

        vk::VkRenderingAttachmentInfo attachment = vk::initVulkanStructure();
        attachment.imageView                     = m_imageViews[i];
        attachment.imageLayout                   = m_layouts[i];
        attachment.loadOp                        = vk::VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment.storeOp                       = vk::VK_ATTACHMENT_STORE_OP_STORE;
        attachment.clearValue                    = m_clearValues[i];

        if (hasDepthAspect || hasStencilAspect)
        {
            renderingInfo.pDepthAttachment   = hasDepthAspect ? &attachment : nullptr;
            renderingInfo.pStencilAttachment = hasStencilAspect ? &attachment : nullptr;
        }
        else
        {
            renderingInfo.colorAttachmentCount = 1u;
            renderingInfo.pColorAttachments    = &attachment;
        }

        vk.cmdBeginRendering(commandBuffer, &renderingInfo);
        vk.cmdEndRendering(commandBuffer);
    }
}

void RenderPassWrapper::updateLayout(VkImage updatedImage, VkImageLayout newLayout) const
{
    for (uint32_t i = 0; i < (uint32_t)m_images.size(); ++i)
        if (m_images[i] == updatedImage)
            m_layouts[i] = newLayout;
}

namespace
{

void recordImageBarrier(const DeviceInterface &vk, const VkCommandBuffer commandBuffer, const bool sync2,
                        const VkPipelineStageFlags2 srcStageMask, const VkAccessFlags2 srcAccessMask,
                        const VkPipelineStageFlags2 dstStageMask, const VkAccessFlags2 dstAccessMask,
                        const VkImageLayout prevLayout, const VkImageLayout newLayout, const VkImage image,
                        const VkImageSubresourceRange &subresourceRange)
{
    if (sync2)
    {
        const auto barrier = makeImageMemoryBarrier2(srcStageMask, srcAccessMask, dstStageMask, dstAccessMask,
                                                     prevLayout, newLayout, image, subresourceRange);

        const VkDependencyInfo depInfo = {
            VK_STRUCTURE_TYPE_DEPENDENCY_INFO, // VkStructureType sType;
            nullptr,                           // const void* pNext;
            0u,                                // VkDependencyFlags dependencyFlags;
            0u,                                // uint32_t memoryBarrierCount;
            nullptr,                           // const VkMemoryBarrier2* pMemoryBarriers;
            0u,                                // uint32_t bufferMemoryBarrierCount;
            nullptr,                           // const VkBufferMemoryBarrier2* pBufferMemoryBarriers;
            1u,                                // uint32_t imageMemoryBarrierCount;
            &barrier,                          // const VkImageMemoryBarrier2* pImageMemoryBarriers;
        };

        vk.cmdPipelineBarrier2(commandBuffer, &depInfo);
    }
    else
    {
        const auto barrier =
            makeImageMemoryBarrier(static_cast<VkAccessFlags>(srcAccessMask), static_cast<VkAccessFlags>(dstAccessMask),
                                   prevLayout, newLayout, image, subresourceRange);

        vk.cmdPipelineBarrier(commandBuffer, static_cast<VkPipelineStageFlags>(srcStageMask),
                              static_cast<VkPipelineStageFlags>(dstStageMask), 0u, 0u, nullptr, 0u, nullptr, 1u,
                              &barrier);
    }
}

} // anonymous namespace

void RenderPassWrapper::transitionLayouts(const DeviceInterface &vk, const VkCommandBuffer commandBuffer,
                                          const Subpass &subpass, bool renderPassBegin) const
{
    // Use the access and stage flags for dependencies on external subpasses in
    // the initial layout transitions for images.
    VkAccessFlags2 externalAccessFlags       = 0u;
    VkPipelineStageFlags2 externalStageFlags = 0u;
    bool sync2                               = false;

    if (renderPassBegin)
    {
        for (const auto &dep : m_dependencies)
        {
            if (dep.srcSubpass == VK_SUBPASS_EXTERNAL)
            {
                externalAccessFlags |= dep.srcAccessMask;
                externalStageFlags |= dep.srcStageMask;
            }

            if (dep.sync2)
                sync2 = true;
        }
    }

    for (uint32_t i = 0; i < (uint32_t)m_attachments.size(); ++i)
    {
        // renderPassBegin is true when vkCmdBeginRenderPass should be called in a normal renderPass, and it is false when vkCmdNextSupass should be called
        // Every image is transioned from VK_IMAGE_LAYOUT_UNDEFINED to it's first used layout, so that all images can be cleared in the beginning
        if (renderPassBegin && m_layouts[i] != vk::VK_IMAGE_LAYOUT_UNDEFINED)
            continue;

        if (m_images[i] != VK_NULL_HANDLE)
        {
            for (uint32_t j = 0; j < (uint32_t)subpass.m_colorAttachments.size(); ++j)
            {
                if (subpass.m_colorAttachments[j].index == i)
                {
                    const auto subresourceRange = makeImageSubresourceRange(
                        vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, VK_REMAINING_MIP_LEVELS, 0u, VK_REMAINING_ARRAY_LAYERS);

                    const VkPipelineStageFlags2 srcStageMask =
                        (vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT | externalStageFlags);
                    const VkAccessFlags2 srcAccessMask       = externalAccessFlags;
                    const VkPipelineStageFlags2 dstStageMask = vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                    const VkAccessFlags2 dstAccessMask       = vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                    const VkImageLayout newLayout            = subpass.m_colorAttachments[j].attachmentInfo.imageLayout;

                    recordImageBarrier(vk, commandBuffer, sync2, srcStageMask, srcAccessMask, dstStageMask,
                                       dstAccessMask, m_layouts[i], newLayout, m_images[i], subresourceRange);

                    updateLayout(m_images[i], newLayout);
                }
            }
            if (subpass.m_depthStencilAttachment.index == i)
            {
                const auto tcuFormat        = vk::mapVkFormat(subpass.m_depthStencilAttachment.format);
                const bool hasDepthAspect   = tcu::hasDepthComponent(tcuFormat.order);
                const bool hasStencilAspect = tcu::hasStencilComponent(tcuFormat.order);

                VkImageAspectFlags aspect = (VkImageAspectFlags)0u;
                if (hasDepthAspect)
                    aspect |= vk::VK_IMAGE_ASPECT_DEPTH_BIT;
                if (hasStencilAspect)
                    aspect |= vk::VK_IMAGE_ASPECT_STENCIL_BIT;

                const auto subresourceRange =
                    makeImageSubresourceRange(aspect, 0u, VK_REMAINING_MIP_LEVELS, 0u, VK_REMAINING_ARRAY_LAYERS);

                const VkPipelineStageFlags2 srcStageMask = (vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT | externalStageFlags);
                const VkAccessFlags2 srcAccessMask       = externalAccessFlags;
                const VkPipelineStageFlags2 dstStageMask =
                    (vk::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | vk::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
                const VkAccessFlags2 dstAccessMask = (vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                                      vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
                const VkImageLayout newLayout      = subpass.m_depthStencilAttachment.attachmentInfo.imageLayout;

                recordImageBarrier(vk, commandBuffer, sync2, srcStageMask, srcAccessMask, dstStageMask, dstAccessMask,
                                   m_layouts[i], newLayout, m_images[i], subresourceRange);

                updateLayout(m_images[i], newLayout);
            }
            for (uint32_t j = 0; j < (uint32_t)subpass.m_resolveAttachments.size(); ++j)
            {
                if (subpass.m_resolveAttachments[j].index == i)
                {
                    const auto subresourceRange = makeImageSubresourceRange(
                        vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, VK_REMAINING_MIP_LEVELS, 0u, VK_REMAINING_ARRAY_LAYERS);

                    const VkPipelineStageFlags2 srcStageMask =
                        (vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT | externalStageFlags);
                    const VkAccessFlags2 srcAccessMask       = externalAccessFlags;
                    const VkPipelineStageFlags2 dstStageMask = vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                    const VkAccessFlags2 dstAccessMask       = vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                    const VkImageLayout newLayout = subpass.m_resolveAttachments[j].attachmentInfo.imageLayout;

                    recordImageBarrier(vk, commandBuffer, sync2, srcStageMask, srcAccessMask, dstStageMask,
                                       dstAccessMask, m_layouts[i], newLayout, m_images[i], subresourceRange);

                    updateLayout(m_images[i], newLayout);
                }
            }
            if (subpass.m_dsr.sType == vk::VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE)
            {
                if (subpass.m_dsr.pDepthStencilResolveAttachment &&
                    i == subpass.m_dsr.pDepthStencilResolveAttachment->attachment)
                {
                    const auto tcuFormat        = vk::mapVkFormat(subpass.m_depthStencilAttachment.format);
                    const bool hasDepthAspect   = tcu::hasDepthComponent(tcuFormat.order);
                    const bool hasStencilAspect = tcu::hasStencilComponent(tcuFormat.order);

                    VkImageAspectFlags aspect = (VkImageAspectFlags)0u;
                    if (hasDepthAspect)
                        aspect |= vk::VK_IMAGE_ASPECT_DEPTH_BIT;
                    if (hasStencilAspect)
                        aspect |= vk::VK_IMAGE_ASPECT_STENCIL_BIT;

                    const auto subresourceRange =
                        makeImageSubresourceRange(aspect, 0u, VK_REMAINING_MIP_LEVELS, 0u, VK_REMAINING_ARRAY_LAYERS);

                    const VkPipelineStageFlags2 srcStageMask =
                        (vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT | externalStageFlags);
                    const VkAccessFlags2 srcAccessMask       = externalAccessFlags;
                    const VkPipelineStageFlags2 dstStageMask = (vk::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                                                vk::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
                    const VkAccessFlags2 dstAccessMask       = vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                                         vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                    const VkImageLayout newLayout = subpass.m_dsr.pDepthStencilResolveAttachment->layout;

                    recordImageBarrier(vk, commandBuffer, sync2, srcStageMask, srcAccessMask, dstStageMask,
                                       dstAccessMask, m_layouts[i], newLayout, m_images[i], subresourceRange);

                    updateLayout(m_images[i], newLayout);
                }
            }
        }
    }
}

void RenderPassWrapper::insertDependencies(const DeviceInterface &vk, const VkCommandBuffer commandBuffer,
                                           uint32_t subpassIdx) const
{
    for (const auto &dep : m_dependencies)
    {
        // Subpass self-dependencies should be handled with manual barriers inside the render pass.
        if (dep.dstSubpass != subpassIdx || dep.srcSubpass == subpassIdx)
            continue;

        if (dep.sync2)
        {
            const VkMemoryBarrier2 barrier = {
                VK_STRUCTURE_TYPE_MEMORY_BARRIER, // VkStructureType sType;
                nullptr,                          // const void* pNext;
                dep.srcStageMask,                 // VkPipelineStageFlags2 srcStageMask;
                dep.srcAccessMask,                // VkAccessFlags2 srcAccessMask;
                dep.dstStageMask,                 // VkPipelineStageFlags2 dstStageMask;
                dep.dstAccessMask,                // VkAccessFlags2 dstAccessMask;
            };
            const VkDependencyInfo depInfo = {
                VK_STRUCTURE_TYPE_DEPENDENCY_INFO, // VkStructureType sType;
                nullptr,                           // const void* pNext;
                dep.dependencyFlags,               // VkDependencyFlags dependencyFlags;
                1u,                                // uint32_t memoryBarrierCount;
                &barrier,                          // const VkMemoryBarrier2* pMemoryBarriers;
                0u,                                // uint32_t bufferMemoryBarrierCount;
                nullptr,                           // const VkBufferMemoryBarrier2* pBufferMemoryBarriers;
                0u,                                // uint32_t imageMemoryBarrierCount;
                nullptr,                           // const VkImageMemoryBarrier2* pImageMemoryBarriers;
            };
            vk.cmdPipelineBarrier2(commandBuffer, &depInfo);
        }
        else
        {
            const VkMemoryBarrier barrier = {
                VK_STRUCTURE_TYPE_MEMORY_BARRIER,              // VkStructureType sType;
                nullptr,                                       // const void* pNext;
                static_cast<VkAccessFlags>(dep.srcAccessMask), // VkAccessFlags srcAccessMask;
                static_cast<VkAccessFlags>(dep.dstAccessMask), // VkAccessFlags dstAccessMask;
            };
            vk.cmdPipelineBarrier(commandBuffer, static_cast<VkPipelineStageFlags>(dep.srcStageMask),
                                  static_cast<VkPipelineStageFlags>(dep.dstStageMask), dep.dependencyFlags, 1u,
                                  &barrier, 0u, nullptr, 0u, nullptr);
        }
    }
}

void RenderPassWrapper::fillInheritanceRenderingInfo(
    uint32_t subpassIndex, std::vector<vk::VkFormat> *colorFormats,
    vk::VkCommandBufferInheritanceRenderingInfo *inheritanceRenderingInfo) const
{
    const auto &subpass = m_subpasses[subpassIndex];
    colorFormats->resize(subpass.m_colorAttachments.size());
    for (uint32_t i = 0; i < (uint32_t)subpass.m_colorAttachments.size(); ++i)
        (*colorFormats)[i] = subpass.m_colorAttachments[i].format;

    inheritanceRenderingInfo->colorAttachmentCount    = (uint32_t)subpass.m_colorAttachments.size();
    inheritanceRenderingInfo->pColorAttachmentFormats = colorFormats->data();

    if (subpass.m_depthStencilAttachment.format != vk::VK_FORMAT_UNDEFINED)
    {
        const auto tcuFormat = vk::mapVkFormat(subpass.m_depthStencilAttachment.format);
        if (tcu::hasDepthComponent(tcuFormat.order))
            inheritanceRenderingInfo->depthAttachmentFormat = subpass.m_depthStencilAttachment.format;
        if (tcu::hasStencilComponent(tcuFormat.order))
            inheritanceRenderingInfo->stencilAttachmentFormat = subpass.m_depthStencilAttachment.format;
    }

    if (subpassIndex < (uint32_t)m_viewMasks.size())
        inheritanceRenderingInfo->viewMask = m_viewMasks[subpassIndex];
}

#endif

void RenderPassWrapper::begin(const DeviceInterface &vk, const VkCommandBuffer commandBuffer,
                              const VkRect2D &renderArea, const uint32_t clearValueCount,
                              const VkClearValue *clearValues, const VkSubpassContents contents,
                              const void *pNext) const
{
    if (!m_isDynamicRendering)
    {
        beginRenderPass(vk, commandBuffer, *m_renderPass, *m_framebuffer, renderArea, clearValueCount, clearValues,
                        contents, pNext);
    }
    else
    {
#ifndef CTS_USES_VULKANSC
        m_activeSubpass = 0;

        m_clearValues.resize(clearValueCount);
        for (uint32_t i = 0; i < clearValueCount; ++i)
            m_clearValues[i] = clearValues[i];

        for (uint32_t i = 0; i < (uint32_t)m_subpasses.size(); ++i)
            transitionLayouts(vk, commandBuffer, m_subpasses[i], true);

        insertDependencies(vk, commandBuffer, 0u);

        m_secondaryCommandBuffers = contents == vk::VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS;

        m_renderingInfo            = vk::initVulkanStructure();
        m_renderingInfo.flags      = (vk::VkRenderingFlags)0u;
        m_renderingInfo.renderArea = renderArea;
        m_renderingInfo.layerCount = m_layers;
        m_renderingInfo.viewMask   = 0x0;

        clearAttachments(vk, commandBuffer);

        beginRendering(vk, commandBuffer);
#endif
    }
}

void RenderPassWrapper::begin(const DeviceInterface &vk, const VkCommandBuffer commandBuffer,
                              const VkRect2D &renderArea, const VkClearValue &clearValue,
                              const VkSubpassContents contents) const
{
    begin(vk, commandBuffer, renderArea, 1u, &clearValue, contents);
}

void RenderPassWrapper::begin(const DeviceInterface &vk, const VkCommandBuffer commandBuffer,
                              const VkRect2D &renderArea, const tcu::Vec4 &clearColor,
                              const VkSubpassContents contents) const
{
    const VkClearValue clearValue = makeClearValueColor(clearColor);
    begin(vk, commandBuffer, renderArea, clearValue, contents);
}

void RenderPassWrapper::begin(const DeviceInterface &vk, const VkCommandBuffer commandBuffer,
                              const VkRect2D &renderArea, const tcu::Vec4 &clearColor, const float clearDepth,
                              const uint32_t clearStencil, const VkSubpassContents contents) const
{
    const VkClearValue clearValues[] = {
        makeClearValueColor(clearColor),                      // attachment 0
        makeClearValueDepthStencil(clearDepth, clearStencil), // attachment 1
    };
    begin(vk, commandBuffer, renderArea, 2, clearValues, contents);
}

void RenderPassWrapper::begin(const DeviceInterface &vk, const VkCommandBuffer commandBuffer,
                              const VkRect2D &renderArea, const VkSubpassContents contents) const
{
    begin(vk, commandBuffer, renderArea, 0u, nullptr, contents);
}

void RenderPassWrapper::begin(const DeviceInterface &vk, const VkCommandBuffer commandBuffer,
                              const VkRect2D &renderArea, const tcu::UVec4 &clearColor,
                              const VkSubpassContents contents) const
{
    const VkClearValue clearValue =
        makeClearValueColorU32(clearColor.x(), clearColor.y(), clearColor.z(), clearColor.w());

    begin(vk, commandBuffer, renderArea, clearValue, contents);
}

void RenderPassWrapper::end(const DeviceInterface &vk, const VkCommandBuffer commandBuffer) const
{
    if (!m_isDynamicRendering)
    {
        vk.cmdEndRenderPass(commandBuffer);
    }
    else
    {
#ifndef CTS_USES_VULKANSC
        vk.cmdEndRendering(commandBuffer);

        // Use dependencies for external subpasses to extract destination access
        // flags and pipeline stage flags for the final layout transition
        // barriers.
        VkAccessFlags2 externalAccessFlags       = 0u;
        VkPipelineStageFlags2 externalStageFlags = 0u;
        bool sync2                               = false;

        for (const auto &dep : m_dependencies)
        {
            if (dep.dstSubpass == VK_SUBPASS_EXTERNAL)
            {
                externalAccessFlags |= dep.dstAccessMask;
                externalStageFlags |= dep.dstStageMask;
            }
            if (dep.sync2)
                sync2 = true;
        }

        for (uint32_t i = 0; i < (uint32_t)m_attachments.size(); ++i)
        {
            if (m_layouts[i] == m_attachments[i].finalLayout)
                continue;

            const bool color = !vk::isDepthStencilFormat(m_attachments[i].format);
            VkImageAspectFlags aspect =
                color ? (vk::VkImageAspectFlags)vk::VK_IMAGE_ASPECT_COLOR_BIT : (vk::VkImageAspectFlags)0u;

            if (!color)
            {
                const bool hasDepthAspect   = tcu::hasDepthComponent(vk::mapVkFormat(m_attachments[i].format).order);
                const bool hasStencilAspect = tcu::hasStencilComponent(vk::mapVkFormat(m_attachments[i].format).order);

                if (hasDepthAspect)
                    aspect |= vk::VK_IMAGE_ASPECT_DEPTH_BIT;
                if (hasStencilAspect)
                    aspect |= vk::VK_IMAGE_ASPECT_STENCIL_BIT;
            }

            const auto subresourceRange =
                makeImageSubresourceRange(aspect, 0u, VK_REMAINING_MIP_LEVELS, 0u, VK_REMAINING_ARRAY_LAYERS);

            const VkPipelineStageFlags2 srcStageMask = (color ? vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT :
                                                                vk::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
            const VkAccessFlags2 srcAccessMask =
                (color ? vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
            const VkPipelineStageFlags2 dstStageMask = (vk::VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT | externalStageFlags);
            const VkAccessFlags2 dstAccessMask       = externalAccessFlags;
            const VkImageLayout newLayout            = m_attachments[i].finalLayout;

            recordImageBarrier(vk, commandBuffer, sync2, srcStageMask, srcAccessMask, dstStageMask, dstAccessMask,
                               m_layouts[i], newLayout, m_images[i], subresourceRange);
        }

        insertDependencies(vk, commandBuffer, VK_SUBPASS_EXTERNAL);
#endif
    }
}

void RenderPassWrapper::beginRendering(const DeviceInterface &vk, const VkCommandBuffer commandBuffer) const
{
    DE_UNREF(vk);
    DE_UNREF(commandBuffer);
#ifndef CTS_USES_VULKANSC
    const auto &subpass = m_subpasses[m_activeSubpass];
    std::vector<vk::VkRenderingAttachmentInfo> colorAttachments;
    for (uint32_t i = 0; i < (uint32_t)subpass.m_colorAttachments.size(); ++i)
    {
        colorAttachments.emplace_back();
        auto &colorAttachment = colorAttachments.back();
        colorAttachment       = vk::initVulkanStructure();
        if (subpass.m_colorAttachments[i].index == VK_ATTACHMENT_UNUSED)
            continue;
        colorAttachment        = subpass.m_colorAttachments[i].attachmentInfo;
        colorAttachment.loadOp = vk::VK_ATTACHMENT_LOAD_OP_LOAD;
        if (!subpass.m_resolveAttachments.empty() && subpass.m_resolveAttachments[i].index != VK_ATTACHMENT_UNUSED)
        {
            if (isUintFormat(subpass.m_resolveAttachments[i].format) ||
                isIntFormat(subpass.m_resolveAttachments[i].format))
                colorAttachment.resolveMode = vk::VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
            else
                colorAttachment.resolveMode = vk::VK_RESOLVE_MODE_AVERAGE_BIT;
            colorAttachment.resolveImageView   = subpass.m_resolveAttachments[i].attachmentInfo.imageView;
            colorAttachment.resolveImageLayout = subpass.m_resolveAttachments[i].attachmentInfo.imageLayout;
        }
    }

    m_renderingInfo.colorAttachmentCount = (uint32_t)colorAttachments.size();
    m_renderingInfo.pColorAttachments    = colorAttachments.data();

    subpass.m_depthStencilAttachment.attachmentInfo.loadOp = vk::VK_ATTACHMENT_LOAD_OP_LOAD;
    VkRenderingAttachmentInfo depthAttachment              = subpass.m_depthStencilAttachment.attachmentInfo;
    VkRenderingAttachmentInfo stencilAttachment            = subpass.m_depthStencilAttachment.attachmentInfo;
    stencilAttachment.storeOp                              = subpass.m_depthStencilAttachment.stencilStoreOp;

    if (depthAttachment.imageView != VK_NULL_HANDLE)
    {
        const auto tcuFormat               = vk::mapVkFormat(subpass.m_depthStencilAttachment.format);
        bool hasDepthAspect                = tcu::hasDepthComponent(tcuFormat.order);
        bool hasStencilAspect              = tcu::hasStencilComponent(tcuFormat.order);
        m_renderingInfo.pDepthAttachment   = hasDepthAspect ? &depthAttachment : nullptr;
        m_renderingInfo.pStencilAttachment = hasStencilAspect ? &stencilAttachment : nullptr;
    }
    else
    {
        m_renderingInfo.pDepthAttachment   = nullptr;
        m_renderingInfo.pStencilAttachment = nullptr;
    }

    if (m_activeSubpass < (uint32_t)m_viewMasks.size())
        m_renderingInfo.viewMask = m_viewMasks[m_activeSubpass];

    m_renderingInfo.pNext = nullptr;
    if (subpass.m_msrtss.sType == VK_STRUCTURE_TYPE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_INFO_EXT)
    {
        subpass.m_msrtss.pNext = nullptr;
        m_renderingInfo.pNext  = &subpass.m_msrtss;
    }

    if (subpass.m_dsr.sType == VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE)
    {
        depthAttachment.resolveMode   = subpass.m_dsr.depthResolveMode;
        stencilAttachment.resolveMode = subpass.m_dsr.stencilResolveMode;
        if (subpass.m_dsr.pDepthStencilResolveAttachment)
        {
            depthAttachment.resolveImageView   = m_imageViews[subpass.m_dsr.pDepthStencilResolveAttachment->attachment];
            depthAttachment.resolveImageLayout = subpass.m_dsr.pDepthStencilResolveAttachment->layout;
            stencilAttachment.resolveImageView = m_imageViews[subpass.m_dsr.pDepthStencilResolveAttachment->attachment];
            stencilAttachment.resolveImageLayout = subpass.m_dsr.pDepthStencilResolveAttachment->layout;
        }
    }

    m_renderingInfo.flags = (VkRenderingFlags)0u;

    if (m_secondaryCommandBuffers)
        m_renderingInfo.flags |= VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT;

    vk.cmdBeginRendering(commandBuffer, &m_renderingInfo);
#endif
}

void RenderPassWrapper::nextSubpass(const DeviceInterface &vk, const VkCommandBuffer commandBuffer,
                                    const VkSubpassContents contents) const
{
    if (!m_isDynamicRendering)
    {
        vk.cmdNextSubpass(commandBuffer, contents);
    }
    else
    {
#ifndef CTS_USES_VULKANSC
        vk.cmdEndRendering(commandBuffer);
        ++m_activeSubpass;
        DE_ASSERT(m_activeSubpass < (uint32_t)m_subpasses.size());

        const auto &subpass = m_subpasses[m_activeSubpass];

        transitionLayouts(vk, commandBuffer, subpass, false);

        insertDependencies(vk, commandBuffer, m_activeSubpass);

        beginRendering(vk, commandBuffer);
#endif
    }
}

void RenderPassWrapper::createFramebuffer(const DeviceInterface &vk, const VkDevice device,
                                          const VkFramebufferCreateInfo *pCreateInfo,
                                          const std::vector<vk::VkImage> &images)
{
    DE_UNREF(images);
    if (!m_isDynamicRendering)
    {
        m_framebuffer = vk::createFramebuffer(vk, device, pCreateInfo);
    }
    else
    {
#ifndef CTS_USES_VULKANSC
        m_images = images;
        m_imageViews.resize(pCreateInfo->attachmentCount);
        for (uint32_t i = 0; i < pCreateInfo->attachmentCount; ++i)
            m_imageViews[i] = pCreateInfo->pAttachments[i];

        for (auto &subpass : m_subpasses)
        {
            for (uint32_t i = 0; i < (uint32_t)subpass.m_colorAttachments.size(); ++i)
            {
                if (subpass.m_colorAttachments[i].index != VK_ATTACHMENT_UNUSED)
                    subpass.m_colorAttachments[i].attachmentInfo.imageView =
                        pCreateInfo->pAttachments[subpass.m_colorAttachments[i].index];
            }

            if (subpass.m_depthStencilAttachment.attachmentInfo.imageLayout != VK_IMAGE_LAYOUT_UNDEFINED)
            {
                if (subpass.m_depthStencilAttachment.index != VK_ATTACHMENT_UNUSED)
                    subpass.m_depthStencilAttachment.attachmentInfo.imageView =
                        pCreateInfo->pAttachments[subpass.m_depthStencilAttachment.index];
            }

            for (uint32_t i = 0; i < (uint32_t)subpass.m_resolveAttachments.size(); ++i)
            {
                if (subpass.m_resolveAttachments[i].index != VK_ATTACHMENT_UNUSED)
                    subpass.m_resolveAttachments[i].attachmentInfo.imageView =
                        pCreateInfo->pAttachments[subpass.m_resolveAttachments[i].index];
            }
        }
        m_layers = pCreateInfo->layers;
#endif
    }
}

void RenderPassWrapper::createFramebuffer(const DeviceInterface &vk, const VkDevice device,
                                          const VkFramebufferCreateInfo *pCreateInfo, vk::VkImage colorImage,
                                          vk::VkImage depthStencilImage)
{
    DE_UNREF(colorImage);
    DE_UNREF(depthStencilImage);
    if (!m_isDynamicRendering)
    {
        m_framebuffer = vk::createFramebuffer(vk, device, pCreateInfo);
    }
    else
    {
#ifndef CTS_USES_VULKANSC
        if (colorImage != VK_NULL_HANDLE)
        {
            m_images.push_back(colorImage);
            DE_ASSERT(pCreateInfo->attachmentCount > 0);
            m_imageViews.push_back(pCreateInfo->pAttachments[0]);
        }
        if (depthStencilImage != VK_NULL_HANDLE)
            m_images.push_back(depthStencilImage);
        for (auto &subpass : m_subpasses)
        {
            DE_ASSERT(subpass.m_colorAttachments.size() <= 1);
            if (pCreateInfo->pAttachments)
            {
                if (!subpass.m_colorAttachments.empty() &&
                    subpass.m_colorAttachments[0].index < pCreateInfo->attachmentCount)
                    subpass.m_colorAttachments[0].attachmentInfo.imageView =
                        pCreateInfo->pAttachments[subpass.m_colorAttachments[0].index];
                if (subpass.m_depthStencilAttachment.index < pCreateInfo->attachmentCount)
                    subpass.m_depthStencilAttachment.attachmentInfo.imageView =
                        pCreateInfo->pAttachments[subpass.m_depthStencilAttachment.index];
            }
        }
#endif
    }
}

void RenderPassWrapper::createFramebuffer(const DeviceInterface &vk, const VkDevice device, const VkImage colorImage,
                                          const VkImageView colorAttachment, const uint32_t width,
                                          const uint32_t height, const uint32_t layers)
{
    DE_UNREF(colorImage);
    if (!m_isDynamicRendering)
    {
        VkFramebufferCreateInfo createInfo = initVulkanStructure();
        createInfo.flags                   = (VkFramebufferCreateFlags)0u;
        createInfo.renderPass              = *m_renderPass;
        createInfo.attachmentCount         = (colorAttachment != VK_NULL_HANDLE) ? 1u : 0u;
        createInfo.pAttachments            = &colorAttachment;
        createInfo.width                   = width;
        createInfo.height                  = height;
        createInfo.layers                  = layers;
        m_framebuffer                      = vk::createFramebuffer(vk, device, &createInfo);
    }
    else
    {
#ifndef CTS_USES_VULKANSC
        m_images.push_back(colorImage);
        m_imageViews.push_back(colorAttachment);
        if (colorImage != VK_NULL_HANDLE)
        {
            for (auto &subpass : m_subpasses)
            {
                DE_ASSERT(subpass.m_colorAttachments.size() == 1);
                subpass.m_colorAttachments[0].attachmentInfo.imageView = colorAttachment;
            }
        }
#endif
    }
}

void RenderPassWrapper::createFramebuffer(const DeviceInterface &vk, const VkDevice device,
                                          const uint32_t attachmentCount, const VkImage *imagesArray,
                                          const VkImageView *attachmentsArray, const uint32_t width,
                                          const uint32_t height, const uint32_t layers)
{
    DE_UNREF(imagesArray);
    if (!m_isDynamicRendering)
    {
        VkFramebufferCreateInfo createInfo = initVulkanStructure();
        createInfo.flags                   = (VkFramebufferCreateFlags)0u;
        createInfo.renderPass              = *m_renderPass;
        createInfo.attachmentCount         = attachmentCount;
        createInfo.pAttachments            = attachmentsArray;
        createInfo.width                   = width;
        createInfo.height                  = height;
        createInfo.layers                  = layers;
        m_framebuffer                      = vk::createFramebuffer(vk, device, &createInfo);
    }
    else
    {
#ifndef CTS_USES_VULKANSC
        for (uint32_t i = 0; i < attachmentCount; ++i)
        {
            m_images.push_back(imagesArray[i]);
            m_imageViews.push_back(attachmentsArray[i]);
        }
        for (auto &subpass : m_subpasses)
        {
            for (uint32_t i = 0; i < (uint32_t)subpass.m_colorAttachments.size(); ++i)
            {
                if (subpass.m_colorAttachments[i].index != VK_ATTACHMENT_UNUSED)
                    subpass.m_colorAttachments[i].attachmentInfo.imageView =
                        attachmentsArray[subpass.m_colorAttachments[i].index];
            }
            if (subpass.m_depthStencilAttachment.attachmentInfo.imageLayout != VK_IMAGE_LAYOUT_UNDEFINED)
            {
                if (subpass.m_depthStencilAttachment.index != VK_ATTACHMENT_UNUSED)
                    subpass.m_depthStencilAttachment.attachmentInfo.imageView =
                        attachmentsArray[subpass.m_depthStencilAttachment.index];
            }
            for (uint32_t i = 0; i < (uint32_t)subpass.m_resolveAttachments.size(); ++i)
            {
                if (subpass.m_resolveAttachments[i].index != VK_ATTACHMENT_UNUSED)
                    subpass.m_resolveAttachments[i].attachmentInfo.imageView =
                        attachmentsArray[subpass.m_resolveAttachments[i].index];
            }
        }
#endif
    }
}

ShaderWrapper::ShaderWrapper()
    : m_vk(nullptr)
    , m_device(VK_NULL_HANDLE)
    , m_binary(nullptr)
    , m_moduleCreateFlags((VkShaderModuleCreateFlags)0u)
    , m_layout(nullptr)
    , m_specializationInfo(nullptr)
#ifndef CTS_USES_VULKANSC
    , m_shaderCreateFlags((VkShaderCreateFlagsEXT)0u)
    , m_binaryDataSize(0u)
#endif
{
}

ShaderWrapper::ShaderWrapper(const DeviceInterface &vk, VkDevice device, const vk::ProgramBinary &binary,
                             const vk::VkShaderModuleCreateFlags createFlags)
    : m_vk(&vk)
    , m_device(device)
    , m_binary(&binary)
    , m_moduleCreateFlags(createFlags)
    , m_layout(nullptr)
    , m_specializationInfo(nullptr)
#ifndef CTS_USES_VULKANSC
    , m_shaderCreateFlags((VkShaderCreateFlagsEXT)0u)
    , m_binaryDataSize(0u)
#endif
{
}

ShaderWrapper::ShaderWrapper(const ShaderWrapper &rhs) noexcept
    : m_vk(rhs.m_vk)
    , m_device(rhs.m_device)
    , m_binary(rhs.m_binary)
    , m_moduleCreateFlags(rhs.m_moduleCreateFlags)
    , m_layout(rhs.m_layout)
    , m_specializationInfo(rhs.m_specializationInfo)
#ifndef CTS_USES_VULKANSC
    , m_shaderCreateFlags(rhs.m_shaderCreateFlags)
    , m_binaryDataSize(rhs.m_binaryDataSize)
    , m_binaryData(rhs.m_binaryData)
#endif
{
}

ShaderWrapper &ShaderWrapper::operator=(const ShaderWrapper &rhs) noexcept
{
    m_vk                 = rhs.m_vk;
    m_device             = rhs.m_device;
    m_binary             = rhs.m_binary;
    m_moduleCreateFlags  = rhs.m_moduleCreateFlags;
    m_layout             = rhs.m_layout;
    m_specializationInfo = rhs.m_specializationInfo;
#ifndef CTS_USES_VULKANSC
    m_shaderCreateFlags = rhs.m_shaderCreateFlags;
    m_binaryDataSize    = rhs.m_binaryDataSize;
    m_binaryData        = rhs.m_binaryData;
#endif

    return *this;
}

vk::VkShaderModule ShaderWrapper::getModule(void) const
{
    if (!m_module)
    {
        if (!m_vk)
            return VK_NULL_HANDLE;
        m_module = createShaderModule(*m_vk, m_device, *m_binary, m_moduleCreateFlags);
    }
    return *m_module;
}

size_t ShaderWrapper::getCodeSize(void) const
{
    return m_binary->getSize();
}

void *ShaderWrapper::getBinary(void) const
{
    return (void *)m_binary->getBinary();
}

void ShaderWrapper::createModule(void)
{
    if (m_vk)
        m_module = createShaderModule(*m_vk, m_device, *m_binary, m_moduleCreateFlags);
}

void ShaderWrapper::setLayoutAndSpecialization(const PipelineLayoutWrapper *layout,
                                               const VkSpecializationInfo *specializationInfo)
{
    m_layout             = layout;
    m_specializationInfo = specializationInfo;
}

#ifndef CTS_USES_VULKANSC
void ShaderWrapper::getShaderBinary(void)
{
    m_vk->getShaderBinaryDataEXT(m_device, *m_shader, &m_binaryDataSize, nullptr);
    m_binaryData.resize(m_binaryDataSize);
    m_vk->getShaderBinaryDataEXT(m_device, *m_shader, &m_binaryDataSize, m_binaryData.data());
}
#endif

// Structure storing *CreateInfo structures that do not need to exist in memory after pipeline was constructed.
struct GraphicsPipelineWrapper::InternalData
{
    const InstanceInterface &vki;
    const DeviceInterface &vk;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    const std::vector<std::string> &deviceExtensions;
    const PipelineConstructionType pipelineConstructionType;
    const VkPipelineCreateFlags pipelineFlags;
    PipelineCreateFlags2 pipelineFlags2;
    ShaderCreateFlags shaderFlags;

    // attribute used for making sure pipeline is configured in correct order
    int setupState;

    std::vector<PipelineShaderStageModuleIdPtr> pipelineShaderIdentifiers;
    std::vector<VkPipelineShaderStageCreateInfo> pipelineShaderStages;
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState;
    VkPipelineRasterizationStateCreateInfo defaultRasterizationState;
    VkPipelineViewportStateCreateInfo viewportState;
    VkPipelineTessellationStateCreateInfo tessellationState;
    VkPipelineFragmentShadingRateStateCreateInfoKHR *pFragmentShadingRateState;
    PipelineRenderingCreateInfoWrapper pRenderingState;
    RenderingAttachmentLocationInfoWrapper pRenderingAttachmentLocation;
    RenderingInputAttachmentIndexInfoWrapper pRenderingInputAttachmentIndex;
    const VkPipelineDynamicStateCreateInfo *pDynamicState;
    PipelineRepresentativeFragmentTestCreateInfoWrapper pRepresentativeFragmentTestState;
    PipelineRobustnessCreateInfoWrapper pPipelineRobustnessState;

    TessellationDomainOriginStatePtr pTessellationDomainOrigin;
    bool useViewportState;
    bool useShaderModules;
    bool useDefaultRasterizationState;
    bool useDefaultDepthStencilState;
    bool useDefaultColorBlendState;
    bool useDefaultMultisampleState;
    bool useDefaultVertexInputState;
    bool failOnCompileWhenLinking;

#ifndef CTS_USES_VULKANSC
    VkGraphicsPipelineLibraryCreateInfoEXT pipelinePartLibraryCreateInfo[4];
    VkPipelineLibraryCreateInfoKHR finalPipelineLibraryCreateInfo;
    VkPipelineCreateFlags2CreateInfoKHR pipelinePartFlags2CreateInfo[4];
#endif
    std::vector<VkDynamicState> pipelinePartDynamicStates[4];
    VkPipelineDynamicStateCreateInfo pipelinePartDynamicStateCreateInfo[4];
    VkGraphicsPipelineCreateInfo pipelinePartCreateInfo[4];
    bool explicitLinkPipelineLayoutSet;
    VkGraphicsPipelineCreateInfo monolithicPipelineCreateInfo;

    ShaderWrapper vertexShader;
    ShaderWrapper tessellationControlShader;
    ShaderWrapper tessellationEvaluationShader;
    ShaderWrapper geometryShader;
    ShaderWrapper fragmentShader;

    ShaderWrapper meshShader;
    ShaderWrapper taskShader;

    bool tessellationShaderFeature;
    bool geometryShaderFeature;
    bool taskShaderFeature;
    bool meshShaderFeature;

    // Store all dynamic state that are used with shader objects
    std::vector<vk::VkDynamicState> shaderObjectDynamicStates;

#ifndef CTS_USES_VULKANSC
    // Store the state that a pipeline would be created with, but shader objects have to set dynamically
    struct PipelineCreateState
    {
        std::vector<VkViewport> viewports;
        std::vector<VkRect2D> scissors;
        float lineWidth = 1.0f;
        VkDepthBiasRepresentationEXT depthBiasRepresentation =
            vk::VK_DEPTH_BIAS_REPRESENTATION_LEAST_REPRESENTABLE_VALUE_FORMAT_EXT;
        VkBool32 depthBiasExact       = VK_FALSE;
        float depthBiasConstantFactor = 0.0f;
        float depthBiasClamp          = 0.0f;
        float depthBiasSlopeFactor    = 1.0f;
        float blendConstants[4]       = {0.0f, 0.0f, 0.0f, 0.0f};
        float minDepthBounds          = 0.0f;
        float maxDepthBounds          = 1.0f;
        VkStencilOpState stencilFront = {
            VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_NEVER, 0, 0, 0};
        VkStencilOpState stencilBack = {
            VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_NEVER, 0, 0, 0};
        VkCullModeFlags cullMode     = VK_CULL_MODE_NONE;
        bool depthTestEnable         = VK_FALSE;
        bool depthWriteEnable        = VK_FALSE;
        VkCompareOp depthCompareOp   = VK_COMPARE_OP_NEVER;
        bool depthBoundsTestEnable   = VK_FALSE;
        VkFrontFace frontFace        = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        bool stencilTestEnable       = VK_FALSE;
        std::vector<VkVertexInputAttributeDescription2EXT> attributes;
        std::vector<VkVertexInputBindingDescription2EXT> bindings;
        bool depthBiasEnable         = VK_FALSE;
        VkLogicOp logicOp            = VK_LOGIC_OP_CLEAR;
        uint32_t patchControlPoints  = 1;
        bool primitiveRestartEnable  = VK_FALSE;
        bool rasterizerDiscardEnable = VK_FALSE;
        bool alphaToCoverageEnable   = VK_FALSE;
        bool alphaToOneEnable        = VK_FALSE;
        std::vector<VkColorBlendAdvancedEXT> colorBlendAdvanced;
        std::vector<VkBool32> colorBlendEnables;
        std::vector<VkColorBlendEquationEXT> blendEquations;
        std::vector<VkColorComponentFlags> colorWriteMasks;
        VkConservativeRasterizationModeEXT conservativeRasterizationMode =
            VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT;
        VkCoverageModulationModeNV coverageModulationMode = VK_COVERAGE_MODULATION_MODE_NONE_NV;
        bool coverageModulationTableEnable                = VK_FALSE;
        std::vector<float> coverageModulationTable;
        VkCoverageReductionModeNV coverageReductionMode = VK_COVERAGE_REDUCTION_MODE_MERGE_NV;
        bool coverageToColorEnable                      = VK_FALSE;
        uint32_t coverageToColorLocation                = 0;
        bool depthClampEnable                           = VK_FALSE;
        bool depthClipEnable                            = VK_FALSE;
        bool negativeOneToOne                           = VK_FALSE;
        uint32_t colorWriteEnableAttachmentCount        = 0;
        std::vector<VkBool32> colorWriteEnables;
        float extraPrimitiveOverestimationSize            = 0.0f;
        VkLineRasterizationModeEXT lineRasterizationMode  = VK_LINE_RASTERIZATION_MODE_DEFAULT_EXT;
        bool stippledLineEnable                           = VK_FALSE;
        uint32_t lineStippleFactor                        = 1;
        uint16_t lineStipplePattern                       = 0x1;
        bool logicOpEnable                                = VK_FALSE;
        VkPolygonMode polygonMode                         = VK_POLYGON_MODE_FILL;
        VkProvokingVertexModeEXT provokingVertexMode      = VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT;
        VkSampleCountFlagBits rasterizationSamples        = VK_SAMPLE_COUNT_1_BIT;
        VkExtent2D fragmentShadingRateSize                = {1u, 1u};
        VkFragmentShadingRateCombinerOpKHR combinerOps[2] = {VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR,
                                                             VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR};
        uint32_t rasterizationStream                      = 0;
        bool representativeFragmentTestEnable             = VK_FALSE;
        bool sampleLocationsEnable                        = VK_FALSE;
        std::vector<VkSampleLocationEXT> pSampleLocations;
        VkSampleLocationsInfoEXT sampleLocationsInfo = vk::initVulkanStructure();
        std::vector<VkSampleMask> sampleMasks;
        bool shadingRateImageEnable             = VK_FALSE;
        VkTessellationDomainOrigin domainOrigin = VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT;
        std::vector<VkViewportSwizzleNV> viewportSwizzles;
        bool viewportWScalingEnable    = VK_FALSE;
        uint32_t viewportWScalingCount = 0;
        std::vector<VkViewportWScalingNV> viewportWScalings;
        VkCoarseSampleOrderTypeNV coarseSampleOrderType = VK_COARSE_SAMPLE_ORDER_TYPE_DEFAULT_NV;
        uint32_t coarseCustomSampleOrderCount           = 0;
        std::vector<std::vector<VkCoarseSampleLocationNV>> coarseSampleLocations;
        std::vector<VkCoarseSampleOrderCustomNV> coarseCustomSampleOrders;
        uint32_t shadingRatePaletteCount = 0;
        std::vector<std::vector<VkShadingRatePaletteEntryNV>> shadingRatePaletteEntries;
        std::vector<VkShadingRatePaletteNV> shadingRatePalettes;
        uint32_t exclusiveScissorCount = 0;
        std::vector<VkRect2D> exclussiveScissors;
        bool discardRectangleEnable = VK_FALSE;
        std::vector<VkRect2D> discardRectangles;
        VkDiscardRectangleModeEXT discardRectangleMode  = VK_DISCARD_RECTANGLE_MODE_INCLUSIVE_EXT;
        VkImageAspectFlags attachmentFeedbackLoopEnable = VK_IMAGE_ASPECT_NONE;
    } pipelineCreateState;
#endif

    // initialize with most common values
    InternalData(const InstanceInterface&            instanceInterface,
                 const DeviceInterface&                vkd,
                 VkPhysicalDevice                    physDevice,
                 VkDevice                            vkDevice,
                 const std::vector<std::string>&    deviceExts,
                 const PipelineConstructionType        constructionType,
                 const VkPipelineCreateFlags        pipelineCreateFlags)
        : vki                       (instanceInterface)
        , vk                        (vkd)
        , physicalDevice            (physDevice)
        , device                    (vkDevice)
        , deviceExtensions          (deviceExts)
        , pipelineConstructionType  (constructionType)
        , pipelineFlags             (pipelineCreateFlags)
        , pipelineFlags2            (0u)
        , shaderFlags               (0u)
        , setupState                (PSS_NONE)
        , inputAssemblyState
        {
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, // VkStructureType                                sType
            nullptr, // const void*                                    pNext
            0u, // VkPipelineInputAssemblyStateCreateFlags        flags
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // VkPrimitiveTopology                            topology
            VK_FALSE                                                        // VkBool32                                        primitiveRestartEnable
        }
        , defaultRasterizationState
        {
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, // VkStructureType                                sType
            nullptr, // const void*                                    pNext
            0u, // VkPipelineRasterizationStateCreateFlags        flags
            VK_FALSE, // VkBool32                                        depthClampEnable
            VK_FALSE, // VkBool32                                        rasterizerDiscardEnable
            VK_POLYGON_MODE_FILL, // VkPolygonMode                                polygonMode
            VK_CULL_MODE_NONE, // VkCullModeFlags                                cullMode
            VK_FRONT_FACE_COUNTER_CLOCKWISE, // VkFrontFace                                    frontFace
            VK_FALSE, // VkBool32                                        depthBiasEnable
            0.0f, // float                                        depthBiasConstantFactor
            0.0f, // float                                        depthBiasClamp
            0.0f, // float                                        depthBiasSlopeFactor
            1.0f                                                            // float                                        lineWidth
        }
        , viewportState
        {
            VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, // VkStructureType                                sType
            nullptr, // const void*                                    pNext
            (VkPipelineViewportStateCreateFlags)0, // VkPipelineViewportStateCreateFlags            flags
            1u, // uint32_t                                        viewportCount
            nullptr, // const VkViewport*                            pViewports
            1u, // uint32_t                                        scissorCount
            nullptr                                                            // const VkRect2D*                                pScissors
        }
        , tessellationState
        {
            VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO, // VkStructureType                                sType
            nullptr, // const void*                                    pNext
            0u, // VkPipelineTessellationStateCreateFlags        flags
            3u                                                                // uint32_t                                        patchControlPoints
        }
        , pFragmentShadingRateState        (nullptr)
        , pDynamicState                    (nullptr)
        , pRepresentativeFragmentTestState(nullptr)
        , pPipelineRobustnessState          (nullptr)
        , pTessellationDomainOrigin        ()
        , useViewportState                (true)
        , useShaderModules                (true)
        , useDefaultRasterizationState    (false)
        , useDefaultDepthStencilState    (false)
        , useDefaultColorBlendState        (false)
        , useDefaultMultisampleState    (false)
        , useDefaultVertexInputState    (true)
        , failOnCompileWhenLinking        (false)
        , explicitLinkPipelineLayoutSet    (false)
        , tessellationShaderFeature        (false)
        , geometryShaderFeature            (false)
        , taskShaderFeature                (false)
        , meshShaderFeature                (false)
    {
        // we need to store create info structures in InternalData
        // to be able to grab whole pipelinePartCreateInfo with valid pNext chain;
        // some tests use VkGraphicsPipelineCreateInfo to create pipeline binaries
        for (uint32_t i = 0u; i < 4u; ++i)
        {
#ifndef CTS_USES_VULKANSC
            pipelinePartFlags2CreateInfo[i] = initVulkanStructure();
#endif
            pipelinePartDynamicStateCreateInfo[i] = initVulkanStructure();
        }

        monolithicPipelineCreateInfo = initVulkanStructure();
    }

    bool extensionEnabled(const std::string &ext) const
    {
        return std::find(deviceExtensions.begin(), deviceExtensions.end(), ext) != deviceExtensions.end();
    }
};

GraphicsPipelineWrapper::GraphicsPipelineWrapper(const InstanceInterface &vki, const DeviceInterface &vk,
                                                 VkPhysicalDevice physicalDevice, VkDevice device,
                                                 const std::vector<std::string> &deviceExtensions,
                                                 const PipelineConstructionType pipelineConstructionType,
                                                 const VkPipelineCreateFlags flags)
    : m_internalData(
          new InternalData(vki, vk, physicalDevice, device, deviceExtensions, pipelineConstructionType, flags))
{
}

GraphicsPipelineWrapper::GraphicsPipelineWrapper(GraphicsPipelineWrapper &&pw) noexcept
    : m_pipelineFinal(pw.m_pipelineFinal)
    , m_internalData(pw.m_internalData)
{
    std::move(pw.m_pipelineParts, pw.m_pipelineParts + de::arrayLength(pw.m_pipelineParts), m_pipelineParts);
}

GraphicsPipelineWrapper &GraphicsPipelineWrapper::setMonolithicPipelineLayout(const PipelineLayoutWrapper &layout)
{
    // make sure pipeline was not already built
    DE_ASSERT(m_pipelineFinal.get() == VK_NULL_HANDLE);

    m_internalData->monolithicPipelineCreateInfo.layout = *layout;
    m_internalData->explicitLinkPipelineLayoutSet       = true;

    return *this;
}

GraphicsPipelineWrapper &GraphicsPipelineWrapper::setDynamicState(const VkPipelineDynamicStateCreateInfo *dynamicState)
{
    // make sure states are not yet setup - all pipeline states must know about dynamic state
    DE_ASSERT(m_internalData && m_internalData->setupState == PSS_NONE);

    m_internalData->pDynamicState                              = dynamicState;
    m_internalData->monolithicPipelineCreateInfo.pDynamicState = dynamicState;

    return *this;
}

GraphicsPipelineWrapper &GraphicsPipelineWrapper::setRepresentativeFragmentTestState(
    PipelineRepresentativeFragmentTestCreateInfoWrapper representativeFragmentTestState)
{
    // Representative fragment test state is needed by the fragment shader state.
    DE_ASSERT(m_internalData && (m_internalData->setupState < PSS_FRAGMENT_SHADER));

    m_internalData->pRepresentativeFragmentTestState = representativeFragmentTestState;
    return *this;
}

GraphicsPipelineWrapper &GraphicsPipelineWrapper::setPipelineCreateFlags2(PipelineCreateFlags2 pipelineFlags2)
{
    // make sure states are not yet setup - all pipeline states must know about createFlags2
    DE_ASSERT(m_internalData && m_internalData->setupState == PSS_NONE);

    m_internalData->pipelineFlags2 = pipelineFlags2;
    return *this;
}

GraphicsPipelineWrapper &GraphicsPipelineWrapper::setPipelineRobustnessState(
    PipelineRobustnessCreateInfoWrapper pipelineRobustnessState)
{
    // pipeline robustness is needed by vertex input state, make sure vertex input state was not setup yet
    DE_ASSERT(m_internalData && (m_internalData->setupState == PSS_NONE));

    m_internalData->pPipelineRobustnessState = pipelineRobustnessState;
    return *this;
}

GraphicsPipelineWrapper &GraphicsPipelineWrapper::setShaderCreateFlags(ShaderCreateFlags shaderFlags)
{
    // make sure states are not yet setup - all pipeline states must know about createFlags2
    DE_ASSERT(m_internalData && m_internalData->setupState == PSS_NONE);

    m_internalData->shaderFlags = shaderFlags;
    return *this;
}

std::vector<VkDynamicState> getDynamicStates(const VkPipelineDynamicStateCreateInfo *dynamicStateInfo,
                                             uint32_t setupState)
{
    static const std::set<VkDynamicState> vertexInputStates{
        VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE_EXT,
        VK_DYNAMIC_STATE_VERTEX_INPUT_EXT,
        VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY_EXT,
        VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE_EXT,
    };

    static const std::set<VkDynamicState> preRastStates{
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT_EXT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT_EXT,
        VK_DYNAMIC_STATE_LINE_WIDTH,
        VK_DYNAMIC_STATE_LINE_STIPPLE_EXT,
        VK_DYNAMIC_STATE_CULL_MODE_EXT,
        VK_DYNAMIC_STATE_FRONT_FACE_EXT,
        VK_DYNAMIC_STATE_PATCH_CONTROL_POINTS_EXT,
        VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE_EXT,
        VK_DYNAMIC_STATE_DISCARD_RECTANGLE_EXT,
        VK_DYNAMIC_STATE_DEPTH_BIAS,
        VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE_EXT,
        VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR,
#ifndef CTS_USES_VULKANSC
        VK_DYNAMIC_STATE_TESSELLATION_DOMAIN_ORIGIN_EXT,
        VK_DYNAMIC_STATE_DEPTH_CLAMP_ENABLE_EXT,
        VK_DYNAMIC_STATE_POLYGON_MODE_EXT,
        VK_DYNAMIC_STATE_RASTERIZATION_STREAM_EXT,
        VK_DYNAMIC_STATE_PROVOKING_VERTEX_MODE_EXT,
        VK_DYNAMIC_STATE_DEPTH_CLIP_NEGATIVE_ONE_TO_ONE_EXT,
        VK_DYNAMIC_STATE_DEPTH_CLIP_ENABLE_EXT,
        VK_DYNAMIC_STATE_LINE_STIPPLE_ENABLE_EXT,
        VK_DYNAMIC_STATE_LINE_STIPPLE_EXT,
        VK_DYNAMIC_STATE_CONSERVATIVE_RASTERIZATION_MODE_EXT,
        VK_DYNAMIC_STATE_EXTRA_PRIMITIVE_OVERESTIMATION_SIZE_EXT,
        VK_DYNAMIC_STATE_LINE_RASTERIZATION_MODE_EXT,
        VK_DYNAMIC_STATE_VIEWPORT_SWIZZLE_NV,
        VK_DYNAMIC_STATE_SHADING_RATE_IMAGE_ENABLE_NV,
        VK_DYNAMIC_STATE_VIEWPORT_W_SCALING_ENABLE_NV,
        VK_DYNAMIC_STATE_VIEWPORT_W_SCALING_NV,
        VK_DYNAMIC_STATE_VIEWPORT_SHADING_RATE_PALETTE_NV,
        VK_DYNAMIC_STATE_VIEWPORT_COARSE_SAMPLE_ORDER_NV,
        VK_DYNAMIC_STATE_EXCLUSIVE_SCISSOR_NV,
#endif
    };

    static const std::set<VkDynamicState> fragShaderStates{
        VK_DYNAMIC_STATE_DEPTH_BOUNDS,
        VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE_EXT,
        VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE_EXT,
        VK_DYNAMIC_STATE_DEPTH_COMPARE_OP_EXT,
        VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE_EXT,
        VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
        VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
        VK_DYNAMIC_STATE_STENCIL_REFERENCE,
        VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE_EXT,
        VK_DYNAMIC_STATE_STENCIL_OP_EXT,
        VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR,
        // Needs MSAA info here as well as fragment output state
        VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_EXT,
#ifndef CTS_USES_VULKANSC
        VK_DYNAMIC_STATE_SAMPLE_MASK_EXT,
        VK_DYNAMIC_STATE_ALPHA_TO_COVERAGE_ENABLE_EXT,
        VK_DYNAMIC_STATE_ALPHA_TO_ONE_ENABLE_EXT,
        VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_ENABLE_EXT,
        VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT,
        VK_DYNAMIC_STATE_COVERAGE_TO_COLOR_ENABLE_NV,
        VK_DYNAMIC_STATE_COVERAGE_TO_COLOR_LOCATION_NV,
        VK_DYNAMIC_STATE_COVERAGE_MODULATION_MODE_NV,
        VK_DYNAMIC_STATE_COVERAGE_MODULATION_TABLE_ENABLE_NV,
        VK_DYNAMIC_STATE_COVERAGE_MODULATION_TABLE_NV,
        VK_DYNAMIC_STATE_COVERAGE_REDUCTION_MODE_NV,
        VK_DYNAMIC_STATE_REPRESENTATIVE_FRAGMENT_TEST_ENABLE_NV,
#endif
    };

    static const std::set<VkDynamicState> fragOutputStates{
        VK_DYNAMIC_STATE_LOGIC_OP_EXT,
        VK_DYNAMIC_STATE_BLEND_CONSTANTS,
        VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT,
        VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR,
        VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_EXT,
#ifndef CTS_USES_VULKANSC
        VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT,
        VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT,
        VK_DYNAMIC_STATE_COLOR_BLEND_ADVANCED_EXT,
        VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT,
        VK_DYNAMIC_STATE_LOGIC_OP_ENABLE_EXT,
        VK_DYNAMIC_STATE_SAMPLE_MASK_EXT,
        VK_DYNAMIC_STATE_ALPHA_TO_COVERAGE_ENABLE_EXT,
        VK_DYNAMIC_STATE_ALPHA_TO_ONE_ENABLE_EXT,
        VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_ENABLE_EXT,
        VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT,
        VK_DYNAMIC_STATE_COVERAGE_TO_COLOR_ENABLE_NV,
        VK_DYNAMIC_STATE_COVERAGE_TO_COLOR_LOCATION_NV,
        VK_DYNAMIC_STATE_COVERAGE_MODULATION_MODE_NV,
        VK_DYNAMIC_STATE_COVERAGE_MODULATION_TABLE_ENABLE_NV,
        VK_DYNAMIC_STATE_COVERAGE_MODULATION_TABLE_NV,
        VK_DYNAMIC_STATE_COVERAGE_REDUCTION_MODE_NV,
        VK_DYNAMIC_STATE_REPRESENTATIVE_FRAGMENT_TEST_ENABLE_NV,
        VK_DYNAMIC_STATE_ATTACHMENT_FEEDBACK_LOOP_ENABLE_EXT,
#endif
    };

    const std::set<VkDynamicState> dynamicStates(
        dynamicStateInfo->pDynamicStates, dynamicStateInfo->pDynamicStates + dynamicStateInfo->dynamicStateCount);

    // Verify all passed states are contained in at least one of the vectors above, so they won't get lost.
    for (const auto dynState : dynamicStates)
    {
        DE_UNREF(dynState); // For release builds.
        DE_ASSERT(de::contains(vertexInputStates.begin(), vertexInputStates.end(), dynState) ||
                  de::contains(preRastStates.begin(), preRastStates.end(), dynState) ||
                  de::contains(fragShaderStates.begin(), fragShaderStates.end(), dynState) ||
                  de::contains(fragOutputStates.begin(), fragOutputStates.end(), dynState));
    }

    std::set<VkDynamicState> intersectedStates;

    if (setupState & PSS_VERTEX_INPUT_INTERFACE)
        std::set_intersection(vertexInputStates.begin(), vertexInputStates.end(), dynamicStates.begin(),
                              dynamicStates.end(), std::inserter(intersectedStates, intersectedStates.end()));

    if (setupState & PSS_PRE_RASTERIZATION_SHADERS)
        std::set_intersection(preRastStates.begin(), preRastStates.end(), dynamicStates.begin(), dynamicStates.end(),
                              std::inserter(intersectedStates, intersectedStates.end()));

    if (setupState & PSS_FRAGMENT_SHADER)
        std::set_intersection(fragShaderStates.begin(), fragShaderStates.end(), dynamicStates.begin(),
                              dynamicStates.end(), std::inserter(intersectedStates, intersectedStates.end()));

    if (setupState & PSS_FRAGMENT_OUTPUT_INTERFACE)
        std::set_intersection(fragOutputStates.begin(), fragOutputStates.end(), dynamicStates.begin(),
                              dynamicStates.end(), std::inserter(intersectedStates, intersectedStates.end()));

    const std::vector<VkDynamicState> returnedStates(begin(intersectedStates), end(intersectedStates));

    return returnedStates;
}

GraphicsPipelineWrapper &GraphicsPipelineWrapper::setDefaultTopology(const VkPrimitiveTopology topology)
{
    // topology is needed by vertex input state, make sure vertex input state was not setup yet
    DE_ASSERT(m_internalData && (m_internalData->setupState == PSS_NONE));

    m_internalData->inputAssemblyState.topology = topology;

    return *this;
}

GraphicsPipelineWrapper &GraphicsPipelineWrapper::setDefaultPatchControlPoints(const uint32_t patchControlPoints)
{
    // patchControlPoints are needed by pre-rasterization shader state, make sure pre-rasterization state was not setup yet
    DE_ASSERT(m_internalData && (m_internalData->setupState < PSS_PRE_RASTERIZATION_SHADERS));

    m_internalData->tessellationState.patchControlPoints = patchControlPoints;

    return *this;
}

GraphicsPipelineWrapper &GraphicsPipelineWrapper::setDefaultTessellationDomainOrigin(
    const VkTessellationDomainOrigin domainOrigin, bool forceExtStruct)
{
    // Tessellation domain origin is needed by pre-rasterization shader state, make sure pre-rasterization state was not setup yet
    DE_ASSERT(m_internalData && (m_internalData->setupState < PSS_PRE_RASTERIZATION_SHADERS));

    // We need the extension structure when:
    // - We want to force it.
    // - The domain origin is not the default value.
    // - We have already hooked the extension structure.
    if (forceExtStruct || domainOrigin != VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT ||
        m_internalData->pTessellationDomainOrigin)
    {
        if (!m_internalData->pTessellationDomainOrigin)
        {
            m_internalData->pTessellationDomainOrigin.reset(
                new VkPipelineTessellationDomainOriginStateCreateInfo(initVulkanStructure()));
            m_internalData->tessellationState.pNext = m_internalData->pTessellationDomainOrigin.get();
        }
        m_internalData->pTessellationDomainOrigin->domainOrigin = domainOrigin;
    }

    return *this;
}

GraphicsPipelineWrapper &GraphicsPipelineWrapper::setDefaultRasterizerDiscardEnable(const bool rasterizerDiscardEnable)
{
    // rasterizerDiscardEnable is used in pre-rasterization shader state, make sure pre-rasterization state was not setup yet
    DE_ASSERT(m_internalData && (m_internalData->setupState < PSS_PRE_RASTERIZATION_SHADERS));

    m_internalData->defaultRasterizationState.rasterizerDiscardEnable = rasterizerDiscardEnable;

    return *this;
}

GraphicsPipelineWrapper &GraphicsPipelineWrapper::setDefaultRasterizationState()
{
    // RasterizationState is used in pre-rasterization shader state, make sure this state was not setup yet
    DE_ASSERT(m_internalData && (m_internalData->setupState < PSS_PRE_RASTERIZATION_SHADERS));

    m_internalData->useDefaultRasterizationState = true;

    return *this;
}

GraphicsPipelineWrapper &GraphicsPipelineWrapper::setDefaultDepthStencilState()
{
    // DepthStencilState is used in fragment shader state, make sure fragment shader state was not setup yet
    DE_ASSERT(m_internalData && (m_internalData->setupState < PSS_FRAGMENT_SHADER));

    m_internalData->useDefaultDepthStencilState = true;

    return *this;
}

GraphicsPipelineWrapper &GraphicsPipelineWrapper::setDefaultColorBlendState()
{
    // ColorBlendState is used in fragment shader state, make sure fragment shader state was not setup yet
    DE_ASSERT(m_internalData && (m_internalData->setupState < PSS_FRAGMENT_SHADER));

    m_internalData->useDefaultColorBlendState = true;

    return *this;
}

GraphicsPipelineWrapper &GraphicsPipelineWrapper::setDefaultMultisampleState()
{
    // MultisampleState is used in fragment shader state, make sure fragment shader state was not setup yet
    DE_ASSERT(m_internalData && (m_internalData->setupState < PSS_FRAGMENT_SHADER));

    m_internalData->useDefaultMultisampleState = true;

    return *this;
}

GraphicsPipelineWrapper &GraphicsPipelineWrapper::setDefaultVertexInputState(const bool useDefaultVertexInputState)
{
    // Make sure vertex input state was not setup yet.
    DE_ASSERT(m_internalData && (m_internalData->setupState == PSS_NONE));

    m_internalData->useDefaultVertexInputState = useDefaultVertexInputState;

    return *this;
}

GraphicsPipelineWrapper &GraphicsPipelineWrapper::setDefaultViewportsCount(uint32_t viewportCount)
{
    // ViewportState is used in pre-rasterization shader state, make sure pre-rasterization state was not setup yet
    DE_ASSERT(m_internalData && (m_internalData->setupState < PSS_PRE_RASTERIZATION_SHADERS));

    m_internalData->viewportState.viewportCount = viewportCount;

    return *this;
}

GraphicsPipelineWrapper &GraphicsPipelineWrapper::setDefaultScissorsCount(uint32_t scissorCount)
{
    // ViewportState is used in pre-rasterization shader state, make sure pre-rasterization state was not setup yet
    DE_ASSERT(m_internalData && (m_internalData->setupState < PSS_PRE_RASTERIZATION_SHADERS));

    m_internalData->viewportState.scissorCount = scissorCount;

    return *this;
}

GraphicsPipelineWrapper &GraphicsPipelineWrapper::setViewportStatePnext(const void *pNext)
{
    // ViewportState is used in pre-rasterization shader state, make sure pre-rasterization state was not setup yet
    DE_ASSERT(m_internalData && (m_internalData->setupState < PSS_PRE_RASTERIZATION_SHADERS));

    m_internalData->viewportState.pNext = pNext;

    return *this;
}

#ifndef CTS_USES_VULKANSC
GraphicsPipelineWrapper &GraphicsPipelineWrapper::setRenderingColorAttachmentsInfo(
    PipelineRenderingCreateInfoWrapper pipelineRenderingCreateInfo)
{
    /* When both graphics pipeline library and dynamic rendering enabled, we just need only viewMask of VkPipelineRenderingCreateInfo
     * on non-fragment stages. But we need the rest info for setting up fragment output states.
     * This method provides a way to verify this condition.
     */
    if (!m_internalData->pRenderingState.ptr || !isConstructionTypeLibrary(m_internalData->pipelineConstructionType))
        return *this;

    DE_ASSERT(m_internalData && (m_internalData->setupState > PSS_VERTEX_INPUT_INTERFACE) &&
              (m_internalData->setupState < PSS_FRAGMENT_OUTPUT_INTERFACE) &&
              (m_internalData->pRenderingState.ptr->viewMask == pipelineRenderingCreateInfo.ptr->viewMask));

    m_internalData->pRenderingState.ptr = pipelineRenderingCreateInfo.ptr;

    return *this;
}
#endif

GraphicsPipelineWrapper &GraphicsPipelineWrapper::disableViewportState(const bool disable)
{
    // ViewportState is used in pre-rasterization shader state, make sure pre-rasterization state was not setup yet
    DE_ASSERT(m_internalData && (m_internalData->setupState < PSS_PRE_RASTERIZATION_SHADERS));

    m_internalData->useViewportState = !disable;

    return *this;
}

GraphicsPipelineWrapper &GraphicsPipelineWrapper::disableShaderModules(const bool disable)
{
    // First shader module is defined in pre-rasterization shader state, make sure pre-rasterization state was not setup yet
    DE_ASSERT(m_internalData && (m_internalData->setupState < PSS_PRE_RASTERIZATION_SHADERS));

    m_internalData->useShaderModules = !disable;

    return *this;
}

GraphicsPipelineWrapper &GraphicsPipelineWrapper::setupVertexInputState(
    const VkPipelineVertexInputStateCreateInfo *vertexInputState,
    const VkPipelineInputAssemblyStateCreateInfo *inputAssemblyState, const VkPipelineCache partPipelineCache,
    PipelineCreationFeedbackCreateInfoWrapper partCreationFeedback, PipelineBinaryInfoWrapper partBinaries,
    const bool useNullPtrs)
{
    // make sure pipeline was not already build
    DE_ASSERT(m_pipelineFinal.get() == VK_NULL_HANDLE);

    // make sure states are set in order - no need to complicate logic to support out of order specification - this state needs to be set first
    DE_ASSERT(m_internalData && (m_internalData->setupState == PSS_NONE));

    // Unreference variables that are not used in Vulkan SC. No need to put this in ifdef.
    DE_UNREF(partPipelineCache);
    DE_UNREF(partCreationFeedback);
    DE_UNREF(partBinaries);

    m_internalData->setupState = PSS_VERTEX_INPUT_INTERFACE;

    const auto pVertexInputState =
        ((vertexInputState || useNullPtrs || !m_internalData->useDefaultVertexInputState) ? vertexInputState :
                                                                                            &defaultVertexInputState);
    const auto pInputAssemblyState =
        ((inputAssemblyState || useNullPtrs) ? inputAssemblyState : &m_internalData->inputAssemblyState);

    if (!isConstructionTypeLibrary(m_internalData->pipelineConstructionType))
    {
        m_internalData->monolithicPipelineCreateInfo.pVertexInputState   = pVertexInputState;
        m_internalData->monolithicPipelineCreateInfo.pInputAssemblyState = pInputAssemblyState;
    }

#ifndef CTS_USES_VULKANSC
    // note we could just use else to if statement above but sinc
    // this section is cut out for Vulkan SC its cleaner with separate if
    if (isConstructionTypeLibrary(m_internalData->pipelineConstructionType))
    {
        auto &libraryCreateInfo = m_internalData->pipelinePartLibraryCreateInfo[0];
        libraryCreateInfo =
            makeGraphicsPipelineLibraryCreateInfo(VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT);
        void *firstStructInChain = reinterpret_cast<void *>(&libraryCreateInfo);
        addToChain(&firstStructInChain, partCreationFeedback.ptr);
        addToChain(&firstStructInChain, partBinaries.ptr);
        addToChain(&firstStructInChain, m_internalData->pPipelineRobustnessState.ptr);

        auto &dynamicStates    = m_internalData->pipelinePartDynamicStates[0];
        auto &dynamicStateInfo = m_internalData->pipelinePartDynamicStateCreateInfo[0];

        if (m_internalData->pDynamicState)
        {
            dynamicStates = getDynamicStates(m_internalData->pDynamicState, m_internalData->setupState);

            dynamicStateInfo.pDynamicStates    = dynamicStates.data();
            dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        }

        auto &pipelinePartCreateInfo = m_internalData->pipelinePartCreateInfo[0];
        pipelinePartCreateInfo       = initVulkanStructure(firstStructInChain);
        pipelinePartCreateInfo.flags =
            (m_internalData->pipelineFlags | VK_PIPELINE_CREATE_LIBRARY_BIT_KHR) & ~VK_PIPELINE_CREATE_DERIVATIVE_BIT;
        pipelinePartCreateInfo.pVertexInputState   = pVertexInputState;
        pipelinePartCreateInfo.pInputAssemblyState = pInputAssemblyState;
        pipelinePartCreateInfo.pDynamicState       = &dynamicStateInfo;

        if (m_internalData->pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY)
            pipelinePartCreateInfo.flags |= VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT;

        if (m_internalData->pipelineFlags2)
        {
            auto &flags2CreateInfo = m_internalData->pipelinePartFlags2CreateInfo[0];
            flags2CreateInfo.flags = m_internalData->pipelineFlags2 | translateCreateFlag(pipelinePartCreateInfo.flags);
            addToChain(&firstStructInChain, &flags2CreateInfo);
            pipelinePartCreateInfo.flags = 0u;
        }

        m_pipelineParts[0] = makeGraphicsPipeline(m_internalData->vk, m_internalData->device, partPipelineCache,
                                                  &pipelinePartCreateInfo);
    }
#endif // CTS_USES_VULKANSC

    return *this;
}

GraphicsPipelineWrapper &GraphicsPipelineWrapper::setupPreRasterizationShaderState(
    const std::vector<VkViewport> &viewports, const std::vector<VkRect2D> &scissors,
    const PipelineLayoutWrapper &layout, const VkRenderPass renderPass, const uint32_t subpass,
    const ShaderWrapper vertexShader, const VkPipelineRasterizationStateCreateInfo *rasterizationState,
    const ShaderWrapper tessellationControlShader, const ShaderWrapper tessellationEvalShader,
    const ShaderWrapper geometryShader, const VkSpecializationInfo *specializationInfo,
    VkPipelineFragmentShadingRateStateCreateInfoKHR *fragmentShadingRateState,
    PipelineRenderingCreateInfoWrapper rendering, const VkPipelineCache partPipelineCache,
    PipelineCreationFeedbackCreateInfoWrapper partCreationFeedback)
{
    return setupPreRasterizationShaderState2(
        viewports, scissors, layout, renderPass, subpass, vertexShader, rasterizationState, tessellationControlShader,
        tessellationEvalShader, geometryShader,
        // Reuse the same specialization info for all stages.
        specializationInfo, specializationInfo, specializationInfo, specializationInfo, fragmentShadingRateState,
        rendering, partPipelineCache, partCreationFeedback);
}

GraphicsPipelineWrapper &GraphicsPipelineWrapper::setupPreRasterizationShaderState2(
    const std::vector<VkViewport> &viewports, const std::vector<VkRect2D> &scissors,
    const PipelineLayoutWrapper &layout, const VkRenderPass renderPass, const uint32_t subpass,
    const ShaderWrapper vertexShader, const VkPipelineRasterizationStateCreateInfo *rasterizationState,
    const ShaderWrapper tessellationControlShader, const ShaderWrapper tessellationEvalShader,
    const ShaderWrapper geometryShader, const VkSpecializationInfo *vertSpecializationInfo,
    const VkSpecializationInfo *tescSpecializationInfo, const VkSpecializationInfo *teseSpecializationInfo,
    const VkSpecializationInfo *geomSpecializationInfo,
    VkPipelineFragmentShadingRateStateCreateInfoKHR *fragmentShadingRateState,
    PipelineRenderingCreateInfoWrapper rendering, const VkPipelineCache partPipelineCache,
    PipelineCreationFeedbackCreateInfoWrapper partCreationFeedback)
{
    return setupPreRasterizationShaderState3(
        viewports, scissors, layout, renderPass, subpass, vertexShader,
        PipelineShaderStageModuleIdentifierCreateInfoWrapper(), rasterizationState, tessellationControlShader,
        PipelineShaderStageModuleIdentifierCreateInfoWrapper(), tessellationEvalShader,
        PipelineShaderStageModuleIdentifierCreateInfoWrapper(), geometryShader,
        PipelineShaderStageModuleIdentifierCreateInfoWrapper(), vertSpecializationInfo, tescSpecializationInfo,
        teseSpecializationInfo, geomSpecializationInfo, fragmentShadingRateState, rendering, partPipelineCache,
        partCreationFeedback);
}

GraphicsPipelineWrapper &GraphicsPipelineWrapper::setupPreRasterizationShaderState3(
    const std::vector<VkViewport> &viewports, const std::vector<VkRect2D> &scissors,
    const PipelineLayoutWrapper &layout, const VkRenderPass renderPass, const uint32_t subpass,
    const ShaderWrapper vertexShader, PipelineShaderStageModuleIdentifierCreateInfoWrapper vertShaderModuleId,
    const VkPipelineRasterizationStateCreateInfo *rasterizationState, const ShaderWrapper tessellationControlShader,
    PipelineShaderStageModuleIdentifierCreateInfoWrapper tescShaderModuleId, const ShaderWrapper tessellationEvalShader,
    PipelineShaderStageModuleIdentifierCreateInfoWrapper teseShaderModuleId, const ShaderWrapper geometryShader,
    PipelineShaderStageModuleIdentifierCreateInfoWrapper geomShaderModuleId,
    const VkSpecializationInfo *vertSpecializationInfo, const VkSpecializationInfo *tescSpecializationInfo,
    const VkSpecializationInfo *teseSpecializationInfo, const VkSpecializationInfo *geomSpecializationInfo,
    VkPipelineFragmentShadingRateStateCreateInfoKHR *fragmentShadingRateState,
    PipelineRenderingCreateInfoWrapper rendering, const VkPipelineCache partPipelineCache,
    PipelineCreationFeedbackCreateInfoWrapper partCreationFeedback, PipelineBinaryInfoWrapper partBinaries)
{
    // make sure pipeline was not already build
    DE_ASSERT(m_pipelineFinal.get() == VK_NULL_HANDLE);

    // make sure states are set in order - no need to complicate logic to support out of order specification - this state needs to be set second
    DE_ASSERT(m_internalData && (m_internalData->setupState == PSS_VERTEX_INPUT_INTERFACE));

    // Unreference variables that are not used in Vulkan SC. No need to put this in ifdef.
    DE_UNREF(partPipelineCache);
    DE_UNREF(partCreationFeedback);
    DE_UNREF(vertShaderModuleId);
    DE_UNREF(tescShaderModuleId);
    DE_UNREF(teseShaderModuleId);
    DE_UNREF(geomShaderModuleId);
    DE_UNREF(partBinaries);

    m_internalData->setupState |= PSS_PRE_RASTERIZATION_SHADERS;
    m_internalData->pFragmentShadingRateState = fragmentShadingRateState;
    m_internalData->pRenderingState.ptr       = rendering.ptr;

    const bool hasTesc = (tessellationControlShader.isSet() || tescShaderModuleId.ptr);
    const bool hasTese = (tessellationEvalShader.isSet() || teseShaderModuleId.ptr);
    const bool hasGeom = (geometryShader.isSet() || geomShaderModuleId.ptr);

    // if patch list topology was set in VertexInputState then tessellation state should be used;
    // we can't use hasTesc for that because pipeline binaries tests don't need to provide shader modules
    auto &pInputAssemblyState = m_internalData->monolithicPipelineCreateInfo.pInputAssemblyState;
    if (isConstructionTypeLibrary(m_internalData->pipelineConstructionType))
        pInputAssemblyState = m_internalData->pipelinePartCreateInfo[0].pInputAssemblyState;
    const bool forceNullTessState =
        m_internalData->tessellationState.patchControlPoints == std::numeric_limits<uint32_t>::max();
    const bool useTessState = !forceNullTessState && pInputAssemblyState &&
                              (pInputAssemblyState->topology == VK_PRIMITIVE_TOPOLOGY_PATCH_LIST);

    const auto pRasterizationState =
        rasterizationState ?
            rasterizationState :
            (m_internalData->useDefaultRasterizationState ? &m_internalData->defaultRasterizationState : nullptr);
    const auto pTessellationState = useTessState ? &m_internalData->tessellationState : nullptr;
    const auto pViewportState     = m_internalData->useViewportState ? &m_internalData->viewportState : nullptr;

    VkPipelineCreateFlags shaderModuleIdFlags = 0u;

    m_internalData->vertexShader = vertexShader;
    m_internalData->vertexShader.setLayoutAndSpecialization(&layout, vertSpecializationInfo);
    VkShaderModule shaderModule = VK_NULL_HANDLE;
    if (m_internalData->useShaderModules && !isConstructionTypeShaderObject(m_internalData->pipelineConstructionType))
        shaderModule = m_internalData->vertexShader.getModule();

    // reserve space for all stages including fragment - this is needed when we create monolithic pipeline
    m_internalData->pipelineShaderStages = std::vector<VkPipelineShaderStageCreateInfo>(
        2u + hasTesc + hasTese + hasGeom,
        {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType                  sType
            nullptr,                                             // const void*                      pNext
            0u,                                                  // VkPipelineShaderStageCreateFlags flags
            VK_SHADER_STAGE_VERTEX_BIT,                          // VkShaderStageFlagBits            stage
            shaderModule,                                        // VkShaderModule                   module
            "main",                                              // const char*                      pName
            vertSpecializationInfo                               // const VkSpecializationInfo*      pSpecializationInfo
        });

#ifndef CTS_USES_VULKANSC
    if (vertShaderModuleId.ptr)
    {
        m_internalData->pipelineShaderIdentifiers.emplace_back(
            new PipelineShaderStageModuleIdentifierCreateInfoWrapper(vertShaderModuleId.ptr));
        m_internalData->pipelineShaderStages[0].pNext = m_internalData->pipelineShaderIdentifiers.back().get()->ptr;

        if (!vertexShader.isSet())
            shaderModuleIdFlags |= VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT;
    }
#endif // CTS_USES_VULKANSC

    std::vector<VkPipelineShaderStageCreateInfo>::iterator currStage = m_internalData->pipelineShaderStages.begin() + 1;

    if (hasTesc)
    {
        m_internalData->tessellationControlShader = tessellationControlShader;
        m_internalData->tessellationControlShader.setLayoutAndSpecialization(&layout, tescSpecializationInfo);

        shaderModule = VK_NULL_HANDLE;
        if (m_internalData->useShaderModules &&
            !isConstructionTypeShaderObject(m_internalData->pipelineConstructionType))
            shaderModule = m_internalData->tessellationControlShader.getModule();

        currStage->stage               = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        currStage->module              = shaderModule;
        currStage->pSpecializationInfo = tescSpecializationInfo;

#ifndef CTS_USES_VULKANSC
        if (tescShaderModuleId.ptr)
        {
            m_internalData->pipelineShaderIdentifiers.emplace_back(
                new PipelineShaderStageModuleIdentifierCreateInfoWrapper(tescShaderModuleId.ptr));
            currStage->pNext = m_internalData->pipelineShaderIdentifiers.back().get()->ptr;

            if (!tessellationControlShader.isSet())
                shaderModuleIdFlags |= VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT;
        }
#endif // CTS_USES_VULKANSC

        ++currStage;
    }

    if (hasTese)
    {
        m_internalData->tessellationEvaluationShader = tessellationEvalShader;
        m_internalData->tessellationEvaluationShader.setLayoutAndSpecialization(&layout, teseSpecializationInfo);

        shaderModule = VK_NULL_HANDLE;
        if (m_internalData->useShaderModules &&
            !isConstructionTypeShaderObject(m_internalData->pipelineConstructionType))
            shaderModule = m_internalData->tessellationEvaluationShader.getModule();

        currStage->stage               = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        currStage->module              = shaderModule;
        currStage->pSpecializationInfo = teseSpecializationInfo;

#ifndef CTS_USES_VULKANSC
        if (teseShaderModuleId.ptr)
        {
            m_internalData->pipelineShaderIdentifiers.emplace_back(
                new PipelineShaderStageModuleIdentifierCreateInfoWrapper(teseShaderModuleId.ptr));
            currStage->pNext = m_internalData->pipelineShaderIdentifiers.back().get()->ptr;

            if (!tessellationEvalShader.isSet())
                shaderModuleIdFlags |= VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT;
        }
#endif // CTS_USES_VULKANSC

        ++currStage;
    }

    if (hasGeom)
    {
        m_internalData->geometryShader = geometryShader;
        m_internalData->geometryShader.setLayoutAndSpecialization(&layout, geomSpecializationInfo);

        shaderModule = VK_NULL_HANDLE;
        if (m_internalData->useShaderModules &&
            !isConstructionTypeShaderObject(m_internalData->pipelineConstructionType))
            shaderModule = m_internalData->geometryShader.getModule();

        currStage->stage               = VK_SHADER_STAGE_GEOMETRY_BIT;
        currStage->module              = shaderModule;
        currStage->pSpecializationInfo = geomSpecializationInfo;

#ifndef CTS_USES_VULKANSC
        if (geomShaderModuleId.ptr)
        {
            m_internalData->pipelineShaderIdentifiers.emplace_back(
                new PipelineShaderStageModuleIdentifierCreateInfoWrapper(geomShaderModuleId.ptr));
            currStage->pNext = m_internalData->pipelineShaderIdentifiers.back().get()->ptr;

            if (!geometryShader.isSet())
                shaderModuleIdFlags |= VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT;
        }
#endif // CTS_USES_VULKANSC
    }

    if (pViewportState)
    {
        if (!viewports.empty())
        {
            pViewportState->viewportCount = (uint32_t)viewports.size();
            pViewportState->pViewports    = &viewports[0];
        }
        if (!scissors.empty())
        {
            pViewportState->scissorCount = (uint32_t)scissors.size();
            pViewportState->pScissors    = &scissors[0];
        }
    }

    // if pipeline layout was not specified with setupMonolithicPipelineLayout
    // then use layout from setupPreRasterizationShaderState for link pipeline
    if (!m_internalData->explicitLinkPipelineLayoutSet)
        m_internalData->monolithicPipelineCreateInfo.layout = *layout;

    if (!isConstructionTypeLibrary(m_internalData->pipelineConstructionType))
    {
        m_internalData->monolithicPipelineCreateInfo.renderPass          = renderPass;
        m_internalData->monolithicPipelineCreateInfo.subpass             = subpass;
        m_internalData->monolithicPipelineCreateInfo.pRasterizationState = pRasterizationState;
        m_internalData->monolithicPipelineCreateInfo.pViewportState      = pViewportState;
        m_internalData->monolithicPipelineCreateInfo.stageCount          = 1u + hasTesc + hasTese + hasGeom;
        m_internalData->monolithicPipelineCreateInfo.pStages             = m_internalData->pipelineShaderStages.data();
        m_internalData->monolithicPipelineCreateInfo.pTessellationState  = pTessellationState;
        m_internalData->monolithicPipelineCreateInfo.flags |= shaderModuleIdFlags;
    }

#ifndef CTS_USES_VULKANSC
    // note we could just use else to if statement above but sinc
    // this section is cut out for Vulkan SC its cleaner with separate if
    if (isConstructionTypeLibrary(m_internalData->pipelineConstructionType))
    {
        auto &libraryCreateInfo = m_internalData->pipelinePartLibraryCreateInfo[1];
        libraryCreateInfo =
            makeGraphicsPipelineLibraryCreateInfo(VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT);
        void *firstStructInChain = reinterpret_cast<void *>(&libraryCreateInfo);
        addToChain(&firstStructInChain, m_internalData->pFragmentShadingRateState);
        addToChain(&firstStructInChain, m_internalData->pRenderingState.ptr);
        addToChain(&firstStructInChain, partCreationFeedback.ptr);
        addToChain(&firstStructInChain, partBinaries.ptr);
        addToChain(&firstStructInChain, m_internalData->pPipelineRobustnessState.ptr);

        auto &dynamicStates    = m_internalData->pipelinePartDynamicStates[1];
        auto &dynamicStateInfo = m_internalData->pipelinePartDynamicStateCreateInfo[1];

        if (m_internalData->pDynamicState)
        {
            dynamicStates = getDynamicStates(m_internalData->pDynamicState, m_internalData->setupState);

            dynamicStateInfo.pDynamicStates    = dynamicStates.data();
            dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        }

        auto &pipelinePartCreateInfo = m_internalData->pipelinePartCreateInfo[1];
        pipelinePartCreateInfo       = initVulkanStructure(firstStructInChain);
        pipelinePartCreateInfo.flags =
            (m_internalData->pipelineFlags | VK_PIPELINE_CREATE_LIBRARY_BIT_KHR | shaderModuleIdFlags) &
            ~VK_PIPELINE_CREATE_DERIVATIVE_BIT;
        pipelinePartCreateInfo.layout              = *layout;
        pipelinePartCreateInfo.renderPass          = renderPass;
        pipelinePartCreateInfo.subpass             = subpass;
        pipelinePartCreateInfo.pRasterizationState = pRasterizationState;
        pipelinePartCreateInfo.pViewportState      = pViewportState;
        pipelinePartCreateInfo.stageCount          = 1u + hasTesc + hasTese + hasGeom;
        pipelinePartCreateInfo.pStages             = m_internalData->pipelineShaderStages.data();
        pipelinePartCreateInfo.pTessellationState  = pTessellationState;
        pipelinePartCreateInfo.pDynamicState       = &dynamicStateInfo;

        if (m_internalData->pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY)
            pipelinePartCreateInfo.flags |= VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT;

        if ((shaderModuleIdFlags & VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT) != 0)
            m_internalData->failOnCompileWhenLinking = true;

        if (m_internalData->pipelineFlags2)
        {
            auto &flags2CreateInfo = m_internalData->pipelinePartFlags2CreateInfo[1];
            flags2CreateInfo.flags = m_internalData->pipelineFlags2 | translateCreateFlag(pipelinePartCreateInfo.flags);
            addToChain(&firstStructInChain, &flags2CreateInfo);
            pipelinePartCreateInfo.flags = 0u;
        }

        m_pipelineParts[1] = makeGraphicsPipeline(m_internalData->vk, m_internalData->device, partPipelineCache,
                                                  &pipelinePartCreateInfo);
    }
#endif // CTS_USES_VULKANSC

    return *this;
}

#ifndef CTS_USES_VULKANSC
GraphicsPipelineWrapper &GraphicsPipelineWrapper::setupPreRasterizationMeshShaderState(
    const std::vector<VkViewport> &viewports, const std::vector<VkRect2D> &scissors,
    const PipelineLayoutWrapper &layout, const VkRenderPass renderPass, const uint32_t subpass,
    const ShaderWrapper taskShader, const ShaderWrapper meshShader,
    const VkPipelineRasterizationStateCreateInfo *rasterizationState,
    const VkSpecializationInfo *taskSpecializationInfo, const VkSpecializationInfo *meshSpecializationInfo,
    VkPipelineFragmentShadingRateStateCreateInfoKHR *fragmentShadingRateState,
    PipelineRenderingCreateInfoWrapper rendering, const VkPipelineCache partPipelineCache,
    VkPipelineCreationFeedbackCreateInfoEXT *partCreationFeedback)
{
    return setupPreRasterizationMeshShaderState2(
        viewports, scissors, layout, renderPass, subpass, taskShader,
        PipelineShaderStageModuleIdentifierCreateInfoWrapper(), meshShader,
        PipelineShaderStageModuleIdentifierCreateInfoWrapper(), rasterizationState, taskSpecializationInfo,
        meshSpecializationInfo, fragmentShadingRateState, rendering, partPipelineCache, partCreationFeedback);
}

GraphicsPipelineWrapper &GraphicsPipelineWrapper::setupPreRasterizationMeshShaderState2(
    const std::vector<VkViewport> &viewports, const std::vector<VkRect2D> &scissors,
    const PipelineLayoutWrapper &layout, const VkRenderPass renderPass, const uint32_t subpass,
    const ShaderWrapper taskShader, PipelineShaderStageModuleIdentifierCreateInfoWrapper taskShaderModuleId,
    const ShaderWrapper meshShader, PipelineShaderStageModuleIdentifierCreateInfoWrapper meshShaderModuleId,
    const VkPipelineRasterizationStateCreateInfo *rasterizationState,
    const VkSpecializationInfo *taskSpecializationInfo, const VkSpecializationInfo *meshSpecializationInfo,
    VkPipelineFragmentShadingRateStateCreateInfoKHR *fragmentShadingRateState,
    PipelineRenderingCreateInfoWrapper rendering, const VkPipelineCache partPipelineCache,
    VkPipelineCreationFeedbackCreateInfoEXT *partCreationFeedback)
{
    // Make sure pipeline was not already built.
    DE_ASSERT(m_pipelineFinal.get() == VK_NULL_HANDLE);

    // Make sure states are set in order - this state needs to be set first or second.
    DE_ASSERT(m_internalData && (m_internalData->setupState < PSS_PRE_RASTERIZATION_SHADERS));

    // The vertex input interface is not needed for mesh shading pipelines, so we're going to mark it as ready here.
    m_internalData->setupState |= (PSS_VERTEX_INPUT_INTERFACE | PSS_PRE_RASTERIZATION_SHADERS);
    m_internalData->pFragmentShadingRateState = fragmentShadingRateState;
    m_internalData->pRenderingState           = rendering;

    const bool hasTask         = (taskShader.isSet() || taskShaderModuleId.ptr);
    const auto taskShaderCount = static_cast<uint32_t>(hasTask);
    const auto pRasterizationState =
        rasterizationState ?
            rasterizationState :
            (m_internalData->useDefaultRasterizationState ? &m_internalData->defaultRasterizationState : nullptr);
    const auto pTessellationState = nullptr;
    const auto pViewportState     = m_internalData->useViewportState ? &m_internalData->viewportState : nullptr;

    // Reserve space for all stages including fragment. This is needed when we create monolithic pipeline.
    m_internalData->pipelineShaderStages = std::vector<VkPipelineShaderStageCreateInfo>(
        2u + taskShaderCount,
        {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType                        sType
            nullptr,                                             // const void*                            pNext
            0u,                                                  // VkPipelineShaderStageCreateFlags        flags
            VK_SHADER_STAGE_VERTEX_BIT,                          // VkShaderStageFlagBits                stage
            VK_NULL_HANDLE,                                      // VkShaderModule                        module
            "main",                                              // const char*                            pName
            nullptr, // const VkSpecializationInfo*            pSpecializationInfo
        });

    VkPipelineCreateFlags shaderModuleIdFlags = 0u;

    // Mesh shader.
    auto currStage = m_internalData->pipelineShaderStages.begin();
    {
        m_internalData->meshShader = meshShader;
        m_internalData->meshShader.setLayoutAndSpecialization(&layout, meshSpecializationInfo);

        VkShaderModule shaderModule = VK_NULL_HANDLE;
        if (m_internalData->useShaderModules &&
            !isConstructionTypeShaderObject(m_internalData->pipelineConstructionType))
            shaderModule = m_internalData->meshShader.getModule();

        auto &stageInfo = *currStage;

        stageInfo.stage               = VK_SHADER_STAGE_MESH_BIT_EXT;
        stageInfo.module              = shaderModule;
        stageInfo.pSpecializationInfo = meshSpecializationInfo;

        if (meshShaderModuleId.ptr)
        {
            m_internalData->pipelineShaderIdentifiers.emplace_back(
                new PipelineShaderStageModuleIdentifierCreateInfoWrapper(meshShaderModuleId.ptr));
            currStage->pNext = m_internalData->pipelineShaderIdentifiers.back().get()->ptr;

            if (!meshShader.isSet())
                shaderModuleIdFlags |= VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT;
        }

        ++currStage;
    }

    if (hasTask)
    {
        m_internalData->taskShader = taskShader;
        m_internalData->taskShader.setLayoutAndSpecialization(&layout, taskSpecializationInfo);

        VkShaderModule shaderModule = VK_NULL_HANDLE;
        if (m_internalData->useShaderModules &&
            !isConstructionTypeShaderObject(m_internalData->pipelineConstructionType))
            shaderModule = m_internalData->taskShader.getModule();

        auto &stageInfo = *currStage;

        stageInfo.stage               = VK_SHADER_STAGE_TASK_BIT_EXT;
        stageInfo.module              = shaderModule;
        stageInfo.pSpecializationInfo = taskSpecializationInfo;

        if (taskShaderModuleId.ptr)
        {
            m_internalData->pipelineShaderIdentifiers.emplace_back(
                new PipelineShaderStageModuleIdentifierCreateInfoWrapper(taskShaderModuleId.ptr));
            currStage->pNext = m_internalData->pipelineShaderIdentifiers.back().get()->ptr;

            if (!taskShader.isSet())
                shaderModuleIdFlags |= VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT;
        }

        ++currStage;
    }

    if (pViewportState)
    {
        if (!viewports.empty())
        {
            pViewportState->viewportCount = (uint32_t)viewports.size();
            pViewportState->pViewports    = &viewports[0];
        }
        if (!scissors.empty())
        {
            pViewportState->scissorCount = (uint32_t)scissors.size();
            pViewportState->pScissors    = &scissors[0];
        }
    }

    // if pipeline layout was not specified with setupMonolithicPipelineLayout
    // then use layout from setupPreRasterizationMeshShaderState for link pipeline
    if (!m_internalData->explicitLinkPipelineLayoutSet)
        m_internalData->monolithicPipelineCreateInfo.layout = *layout;

    if (!isConstructionTypeLibrary(m_internalData->pipelineConstructionType))
    {
        m_internalData->monolithicPipelineCreateInfo.renderPass          = renderPass;
        m_internalData->monolithicPipelineCreateInfo.subpass             = subpass;
        m_internalData->monolithicPipelineCreateInfo.pRasterizationState = pRasterizationState;
        m_internalData->monolithicPipelineCreateInfo.pViewportState      = pViewportState;
        m_internalData->monolithicPipelineCreateInfo.stageCount          = 1u + taskShaderCount;
        m_internalData->monolithicPipelineCreateInfo.pStages             = m_internalData->pipelineShaderStages.data();
        m_internalData->monolithicPipelineCreateInfo.pTessellationState  = pTessellationState;
        m_internalData->monolithicPipelineCreateInfo.flags |= shaderModuleIdFlags;
    }
    else
    {
        auto &libraryCreateInfo = m_internalData->pipelinePartLibraryCreateInfo[1];
        libraryCreateInfo =
            makeGraphicsPipelineLibraryCreateInfo(VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT);
        void *firstStructInChain = reinterpret_cast<void *>(&libraryCreateInfo);
        addToChain(&firstStructInChain, m_internalData->pFragmentShadingRateState);
        addToChain(&firstStructInChain, m_internalData->pRenderingState.ptr);
        addToChain(&firstStructInChain, partCreationFeedback);
        addToChain(&firstStructInChain, m_internalData->pPipelineRobustnessState.ptr);

        auto &dynamicStates    = m_internalData->pipelinePartDynamicStates[1];
        auto &dynamicStateInfo = m_internalData->pipelinePartDynamicStateCreateInfo[1];

        if (m_internalData->pDynamicState)
        {
            dynamicStates = getDynamicStates(m_internalData->pDynamicState, m_internalData->setupState);

            dynamicStateInfo.pDynamicStates    = dynamicStates.data();
            dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        }

        auto &pipelinePartCreateInfo = m_internalData->pipelinePartCreateInfo[1];
        pipelinePartCreateInfo       = initVulkanStructure(firstStructInChain);

        pipelinePartCreateInfo.flags =
            (m_internalData->pipelineFlags | VK_PIPELINE_CREATE_LIBRARY_BIT_KHR | shaderModuleIdFlags);
        // Remove VK_PIPELINE_CREATE_DERIVATIVE_BIT?

        pipelinePartCreateInfo.layout              = *layout;
        pipelinePartCreateInfo.renderPass          = renderPass;
        pipelinePartCreateInfo.subpass             = subpass;
        pipelinePartCreateInfo.pRasterizationState = pRasterizationState;
        pipelinePartCreateInfo.pViewportState      = pViewportState;
        pipelinePartCreateInfo.stageCount          = 1u + taskShaderCount;
        pipelinePartCreateInfo.pStages             = m_internalData->pipelineShaderStages.data();
        pipelinePartCreateInfo.pTessellationState  = pTessellationState;
        pipelinePartCreateInfo.pDynamicState       = &dynamicStateInfo;

        if (m_internalData->pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY)
            pipelinePartCreateInfo.flags |= VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT;

        if ((shaderModuleIdFlags & VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT) != 0)
            m_internalData->failOnCompileWhenLinking = true;

        if (m_internalData->pipelineFlags2)
        {
            auto &flags2CreateInfo = m_internalData->pipelinePartFlags2CreateInfo[1];
            flags2CreateInfo.flags = m_internalData->pipelineFlags2 | translateCreateFlag(pipelinePartCreateInfo.flags);
            addToChain(&firstStructInChain, &flags2CreateInfo);
            pipelinePartCreateInfo.flags = 0u;
        }

        m_pipelineParts[1] = makeGraphicsPipeline(m_internalData->vk, m_internalData->device, partPipelineCache,
                                                  &pipelinePartCreateInfo);
    }

    return *this;
}
#endif // CTS_USES_VULKANSC

GraphicsPipelineWrapper &GraphicsPipelineWrapper::setupFragmentShaderState(
    const PipelineLayoutWrapper &layout, const VkRenderPass renderPass, const uint32_t subpass,
    const ShaderWrapper fragmentShader, const VkPipelineDepthStencilStateCreateInfo *depthStencilState,
    const VkPipelineMultisampleStateCreateInfo *multisampleState, const VkSpecializationInfo *specializationInfo,
    const VkPipelineCache partPipelineCache, PipelineCreationFeedbackCreateInfoWrapper partCreationFeedback,
    RenderingInputAttachmentIndexInfoWrapper renderingInputAttachmentIndexInfo)
{
    return setupFragmentShaderState2(layout, renderPass, subpass, fragmentShader,
                                     PipelineShaderStageModuleIdentifierCreateInfoWrapper(), depthStencilState,
                                     multisampleState, specializationInfo, partPipelineCache, partCreationFeedback,
                                     renderingInputAttachmentIndexInfo);
}

GraphicsPipelineWrapper &GraphicsPipelineWrapper::setupFragmentShaderState2(
    const PipelineLayoutWrapper &layout, const VkRenderPass renderPass, const uint32_t subpass,
    const ShaderWrapper fragmentShader, PipelineShaderStageModuleIdentifierCreateInfoWrapper fragmentShaderModuleId,
    const VkPipelineDepthStencilStateCreateInfo *depthStencilState,
    const VkPipelineMultisampleStateCreateInfo *multisampleState, const VkSpecializationInfo *specializationInfo,
    const VkPipelineCache partPipelineCache, PipelineCreationFeedbackCreateInfoWrapper partCreationFeedback,
    RenderingInputAttachmentIndexInfoWrapper renderingInputAttachmentIndexInfo, PipelineBinaryInfoWrapper partBinaries)
{
    // make sure pipeline was not already build
    DE_ASSERT(m_pipelineFinal.get() == VK_NULL_HANDLE);

    // make sure states are set in order - no need to complicate logic to support out of order specification - this state needs to be set third
    DE_ASSERT(m_internalData &&
              (m_internalData->setupState == (PSS_VERTEX_INPUT_INTERFACE | PSS_PRE_RASTERIZATION_SHADERS)));

    // Unreference variables that are not used in Vulkan SC. No need to put this in ifdef.
    DE_UNREF(layout);
    DE_UNREF(renderPass);
    DE_UNREF(subpass);
    DE_UNREF(partPipelineCache);
    DE_UNREF(partCreationFeedback);
    DE_UNREF(fragmentShaderModuleId);
    DE_UNREF(partBinaries);

    m_internalData->setupState |= PSS_FRAGMENT_SHADER;
    m_internalData->pRenderingInputAttachmentIndex.ptr = renderingInputAttachmentIndexInfo.ptr;

    const auto pDepthStencilState =
        depthStencilState ? depthStencilState :
                            (m_internalData->useDefaultDepthStencilState ? &defaultDepthStencilState : nullptr);
    const auto pMultisampleState =
        multisampleState ? multisampleState :
                           (m_internalData->useDefaultMultisampleState ? &defaultMultisampleState : nullptr);
    const bool hasFrag = (fragmentShader.isSet() || fragmentShaderModuleId.ptr);

    VkPipelineCreateFlags shaderModuleIdFlags = 0u;

    uint32_t stageIndex = 1;
    if (hasFrag)
    {
        // find free space for fragment shader
        for (; stageIndex < 5; ++stageIndex)
        {
            if (m_internalData->pipelineShaderStages[stageIndex].stage == VK_SHADER_STAGE_VERTEX_BIT)
            {
                m_internalData->fragmentShader = fragmentShader;
                m_internalData->fragmentShader.setLayoutAndSpecialization(&layout, specializationInfo);

                VkShaderModule shaderModule = VK_NULL_HANDLE;
                if (m_internalData->useShaderModules &&
                    !isConstructionTypeShaderObject(m_internalData->pipelineConstructionType))
                    shaderModule = m_internalData->fragmentShader.getModule();

                m_internalData->pipelineShaderStages[stageIndex].stage               = VK_SHADER_STAGE_FRAGMENT_BIT;
                m_internalData->pipelineShaderStages[stageIndex].module              = shaderModule;
                m_internalData->pipelineShaderStages[stageIndex].pSpecializationInfo = specializationInfo;
#ifndef CTS_USES_VULKANSC
                if (fragmentShaderModuleId.ptr)
                {
                    m_internalData->pipelineShaderIdentifiers.emplace_back(
                        new PipelineShaderStageModuleIdentifierCreateInfoWrapper(fragmentShaderModuleId.ptr));
                    m_internalData->pipelineShaderStages[stageIndex].pNext =
                        m_internalData->pipelineShaderIdentifiers.back().get()->ptr;

                    if (!fragmentShader.isSet())
                        shaderModuleIdFlags |= VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT;
                }
#endif // CTS_USES_VULKANSC
                break;
            }
        }
    }

    if (!isConstructionTypeLibrary(m_internalData->pipelineConstructionType))
    {
        m_internalData->monolithicPipelineCreateInfo.pDepthStencilState = pDepthStencilState;
        m_internalData->monolithicPipelineCreateInfo.pMultisampleState  = pMultisampleState;
        m_internalData->monolithicPipelineCreateInfo.stageCount += (hasFrag ? 1u : 0u);
        m_internalData->monolithicPipelineCreateInfo.flags |= shaderModuleIdFlags;
    }

#ifndef CTS_USES_VULKANSC
    // note we could just use else to if statement above but sinc
    // this section is cut out for Vulkan SC its cleaner with separate if
    if (isConstructionTypeLibrary(m_internalData->pipelineConstructionType))
    {
        auto &libraryCreateInfo = m_internalData->pipelinePartLibraryCreateInfo[2];
        libraryCreateInfo = makeGraphicsPipelineLibraryCreateInfo(VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT);
        void *firstStructInChain = reinterpret_cast<void *>(&libraryCreateInfo);
        addToChain(&firstStructInChain, m_internalData->pFragmentShadingRateState);
        addToChain(&firstStructInChain, m_internalData->pRenderingState.ptr);
        addToChain(&firstStructInChain, m_internalData->pRenderingInputAttachmentIndex.ptr);
        addToChain(&firstStructInChain, partCreationFeedback.ptr);
        addToChain(&firstStructInChain, m_internalData->pRepresentativeFragmentTestState.ptr);
        addToChain(&firstStructInChain, partBinaries.ptr);
        addToChain(&firstStructInChain, m_internalData->pPipelineRobustnessState.ptr);

        auto &dynamicStates    = m_internalData->pipelinePartDynamicStates[2];
        auto &dynamicStateInfo = m_internalData->pipelinePartDynamicStateCreateInfo[2];

        if (m_internalData->pDynamicState)
        {
            dynamicStates = getDynamicStates(m_internalData->pDynamicState, m_internalData->setupState);

            dynamicStateInfo.pDynamicStates    = dynamicStates.data();
            dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        }

        auto &pipelinePartCreateInfo = m_internalData->pipelinePartCreateInfo[2];
        pipelinePartCreateInfo       = initVulkanStructure(firstStructInChain);
        pipelinePartCreateInfo.flags =
            (m_internalData->pipelineFlags | VK_PIPELINE_CREATE_LIBRARY_BIT_KHR | shaderModuleIdFlags) &
            ~VK_PIPELINE_CREATE_DERIVATIVE_BIT;
        pipelinePartCreateInfo.layout             = *layout;
        pipelinePartCreateInfo.renderPass         = renderPass;
        pipelinePartCreateInfo.subpass            = subpass;
        pipelinePartCreateInfo.pDepthStencilState = pDepthStencilState;
        pipelinePartCreateInfo.pMultisampleState  = pMultisampleState;
        pipelinePartCreateInfo.stageCount         = hasFrag;
        pipelinePartCreateInfo.pStages       = hasFrag ? &m_internalData->pipelineShaderStages[stageIndex] : nullptr;
        pipelinePartCreateInfo.pDynamicState = &dynamicStateInfo;

        if (m_internalData->pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY)
            pipelinePartCreateInfo.flags |= VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT;

        if ((shaderModuleIdFlags & VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT) != 0)
            m_internalData->failOnCompileWhenLinking = true;

        if (m_internalData->pipelineFlags2)
        {
            auto &flags2CreateInfo = m_internalData->pipelinePartFlags2CreateInfo[2];
            flags2CreateInfo.flags = m_internalData->pipelineFlags2 | translateCreateFlag(pipelinePartCreateInfo.flags);
            addToChain(&firstStructInChain, &flags2CreateInfo);
            pipelinePartCreateInfo.flags = 0u;
        }

        m_pipelineParts[2] = makeGraphicsPipeline(m_internalData->vk, m_internalData->device, partPipelineCache,
                                                  &pipelinePartCreateInfo);
    }
#endif // CTS_USES_VULKANSC

    return *this;
}

GraphicsPipelineWrapper &GraphicsPipelineWrapper::setupFragmentOutputState(
    const VkRenderPass renderPass, const uint32_t subpass, const VkPipelineColorBlendStateCreateInfo *colorBlendState,
    const VkPipelineMultisampleStateCreateInfo *multisampleState, const VkPipelineCache partPipelineCache,
    PipelineCreationFeedbackCreateInfoWrapper partCreationFeedback,
    RenderingAttachmentLocationInfoWrapper renderingAttachmentLocationInfo, PipelineBinaryInfoWrapper partBinaries)
{
    // make sure pipeline was not already build
    DE_ASSERT(m_pipelineFinal.get() == VK_NULL_HANDLE);

    // make sure states are set in order - no need to complicate logic to support out of order specification - this state needs to be set last
    DE_ASSERT(m_internalData && (m_internalData->setupState ==
                                 (PSS_VERTEX_INPUT_INTERFACE | PSS_PRE_RASTERIZATION_SHADERS | PSS_FRAGMENT_SHADER)));
    m_internalData->setupState |= PSS_FRAGMENT_OUTPUT_INTERFACE;
    m_internalData->pRenderingAttachmentLocation.ptr = renderingAttachmentLocationInfo.ptr;

    // Unreference variables that are not used in Vulkan SC. No need to put this in ifdef.
    DE_UNREF(renderPass);
    DE_UNREF(subpass);
    DE_UNREF(partPipelineCache);
    DE_UNREF(partCreationFeedback);
    DE_UNREF(partBinaries);

    const auto pColorBlendState = colorBlendState ?
                                      colorBlendState :
                                      (m_internalData->useDefaultColorBlendState ? &defaultColorBlendState : nullptr);
    const auto pMultisampleState =
        multisampleState ? multisampleState :
                           (m_internalData->useDefaultMultisampleState ? &defaultMultisampleState : nullptr);

    if (!isConstructionTypeLibrary(m_internalData->pipelineConstructionType))
    {
        m_internalData->monolithicPipelineCreateInfo.flags |= m_internalData->pipelineFlags;
        m_internalData->monolithicPipelineCreateInfo.pColorBlendState  = pColorBlendState;
        m_internalData->monolithicPipelineCreateInfo.pMultisampleState = pMultisampleState;
    }

#ifndef CTS_USES_VULKANSC
    // note we could just use else to if statement above but sinc
    // this section is cut out for Vulkan SC its cleaner with separate if
    if (isConstructionTypeLibrary(m_internalData->pipelineConstructionType))
    {
        auto &libraryCreateInfo = m_internalData->pipelinePartLibraryCreateInfo[3];
        libraryCreateInfo =
            makeGraphicsPipelineLibraryCreateInfo(VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT);
        void *firstStructInChain = m_internalData->pFragmentShadingRateState;
        addToChain(&firstStructInChain, m_internalData->pRenderingState.ptr);
        addToChain(&firstStructInChain, &libraryCreateInfo);
        addToChain(&firstStructInChain, partCreationFeedback.ptr);
        addToChain(&firstStructInChain, partBinaries.ptr);
        addToChain(&firstStructInChain, m_internalData->pRenderingAttachmentLocation.ptr);
        addToChain(&firstStructInChain, m_internalData->pPipelineRobustnessState.ptr);

        auto &dynamicStates    = m_internalData->pipelinePartDynamicStates[3];
        auto &dynamicStateInfo = m_internalData->pipelinePartDynamicStateCreateInfo[3];

        if (m_internalData->pDynamicState)
        {
            dynamicStates = getDynamicStates(m_internalData->pDynamicState, m_internalData->setupState);

            dynamicStateInfo.pDynamicStates    = dynamicStates.data();
            dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        }

        auto &pipelinePartCreateInfo = m_internalData->pipelinePartCreateInfo[3];
        pipelinePartCreateInfo       = initVulkanStructure(firstStructInChain);
        pipelinePartCreateInfo.flags =
            (m_internalData->pipelineFlags | VK_PIPELINE_CREATE_LIBRARY_BIT_KHR) & ~VK_PIPELINE_CREATE_DERIVATIVE_BIT;
        pipelinePartCreateInfo.renderPass        = renderPass;
        pipelinePartCreateInfo.subpass           = subpass;
        pipelinePartCreateInfo.pColorBlendState  = pColorBlendState;
        pipelinePartCreateInfo.pMultisampleState = pMultisampleState;
        pipelinePartCreateInfo.pDynamicState     = &dynamicStateInfo;

        if (m_internalData->pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY)
            pipelinePartCreateInfo.flags |= VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT;

        if (m_internalData->pipelineFlags2)
        {
            auto &flags2CreateInfo = m_internalData->pipelinePartFlags2CreateInfo[3];
            flags2CreateInfo.flags = m_internalData->pipelineFlags2 | translateCreateFlag(pipelinePartCreateInfo.flags);
            addToChain(&firstStructInChain, &flags2CreateInfo);
            pipelinePartCreateInfo.flags = 0u;
        }

        m_pipelineParts[3] = makeGraphicsPipeline(m_internalData->vk, m_internalData->device, partPipelineCache,
                                                  &pipelinePartCreateInfo);
    }
#endif // CTS_USES_VULKANSC

    return *this;
}

#ifndef CTS_USES_VULKANSC
vk::VkShaderStageFlags GraphicsPipelineWrapper::getNextStages(vk::VkShaderStageFlagBits shaderStage,
                                                              bool tessellationShaders, bool geometryShaders, bool link)
{
    if (link)
    {
        if (shaderStage == vk::VK_SHADER_STAGE_VERTEX_BIT)
        {
            if (m_internalData->tessellationControlShader.isSet())
                return vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
            if (m_internalData->geometryShader.isSet())
                return vk::VK_SHADER_STAGE_GEOMETRY_BIT;
            if (m_internalData->fragmentShader.isSet())
                return vk::VK_SHADER_STAGE_FRAGMENT_BIT;
        }
        if (shaderStage == vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
            return vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        if (shaderStage == vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
        {
            if (m_internalData->geometryShader.isSet())
                return vk::VK_SHADER_STAGE_GEOMETRY_BIT;
            if (m_internalData->fragmentShader.isSet())
                return vk::VK_SHADER_STAGE_FRAGMENT_BIT;
        }
        if (shaderStage == vk::VK_SHADER_STAGE_GEOMETRY_BIT)
        {
            if (m_internalData->fragmentShader.isSet())
                return vk::VK_SHADER_STAGE_FRAGMENT_BIT;
        }
        if (shaderStage == vk::VK_SHADER_STAGE_TASK_BIT_EXT)
        {
            if (m_internalData->meshShader.isSet())
                return vk::VK_SHADER_STAGE_MESH_BIT_EXT;
            if (m_internalData->fragmentShader.isSet())
                return vk::VK_SHADER_STAGE_FRAGMENT_BIT;
        }
        if (shaderStage == vk::VK_SHADER_STAGE_MESH_BIT_EXT)
        {
            if (m_internalData->fragmentShader.isSet())
                return vk::VK_SHADER_STAGE_FRAGMENT_BIT;
        }
    }
    else
    {
        if (shaderStage == vk::VK_SHADER_STAGE_VERTEX_BIT)
        {
            VkShaderStageFlags flags = vk::VK_SHADER_STAGE_FRAGMENT_BIT;
            if (tessellationShaders)
                flags |= vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
            if (geometryShaders)
                flags |= vk::VK_SHADER_STAGE_GEOMETRY_BIT;
            return flags;
        }
        else if (shaderStage == vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
        {
            return vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        }
        else if (shaderStage == vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
        {
            VkShaderStageFlags flags = vk::VK_SHADER_STAGE_FRAGMENT_BIT;
            if (geometryShaders)
                flags |= vk::VK_SHADER_STAGE_GEOMETRY_BIT;
            return flags;
        }
        else if (shaderStage == vk::VK_SHADER_STAGE_GEOMETRY_BIT)
        {
            return vk::VK_SHADER_STAGE_FRAGMENT_BIT;
        }
        else if (shaderStage == vk::VK_SHADER_STAGE_TASK_BIT_EXT)
        {
            return vk::VK_SHADER_STAGE_MESH_BIT_EXT;
        }
        else if (shaderStage == vk::VK_SHADER_STAGE_MESH_BIT_EXT)
        {
            return vk::VK_SHADER_STAGE_FRAGMENT_BIT;
        }
    }
    return 0;
}

vk::VkShaderCreateInfoEXT GraphicsPipelineWrapper::makeShaderCreateInfo(VkShaderStageFlagBits stage,
                                                                        ShaderWrapper &shader, bool link, bool binary,
                                                                        ShaderWrapper &other)
{
    if (binary)
        shader.getShaderBinary();

    vk::VkShaderCreateInfoEXT shaderCreateInfo = vk::initVulkanStructure();
    const auto baseFlags =
        (link ? static_cast<vk::VkShaderCreateFlagsEXT>(vk::VK_SHADER_CREATE_LINK_STAGE_BIT_EXT) : 0u);
    shaderCreateInfo.flags = (baseFlags | m_internalData->shaderFlags); // Add user-provided flags.
    shaderCreateInfo.stage = stage;
    shaderCreateInfo.nextStage =
        getNextStages(stage, m_internalData->tessellationShaderFeature, m_internalData->geometryShaderFeature, link);
    if (binary)
    {
        shaderCreateInfo.codeType = vk::VK_SHADER_CODE_TYPE_BINARY_EXT;
        shaderCreateInfo.codeSize = shader.getShaderBinaryDataSize();
        shaderCreateInfo.pCode    = shader.getShaderBinaryData();
    }
    else
    {
        shaderCreateInfo.codeType = vk::VK_SHADER_CODE_TYPE_SPIRV_EXT;
        shaderCreateInfo.codeSize = shader.getCodeSize();
        shaderCreateInfo.pCode    = shader.getBinary();
    }
    shaderCreateInfo.pName = "main";
    if (shader.getPipelineLayout() != VK_NULL_HANDLE)
    {
        shaderCreateInfo.setLayoutCount         = shader.getPipelineLayout()->getSetLayoutCount();
        shaderCreateInfo.pSetLayouts            = shader.getPipelineLayout()->getSetLayouts();
        shaderCreateInfo.pushConstantRangeCount = shader.getPipelineLayout()->getPushConstantRangeCount();
        shaderCreateInfo.pPushConstantRanges    = shader.getPipelineLayout()->getPushConstantRanges();
    }
    // Pipeline layouts and push constant ranges must match between shaders that are used together
    if (other.isSet() && shaderCreateInfo.setLayoutCount == 0)
    {
        shaderCreateInfo.setLayoutCount = other.getPipelineLayout()->getSetLayoutCount();
        shaderCreateInfo.pSetLayouts    = other.getPipelineLayout()->getSetLayouts();
    }
    if (other.isSet() && shaderCreateInfo.pushConstantRangeCount == 0)
    {
        shaderCreateInfo.pushConstantRangeCount = other.getPipelineLayout()->getPushConstantRangeCount();
        shaderCreateInfo.pPushConstantRanges    = other.getPipelineLayout()->getPushConstantRanges();
    }
    shaderCreateInfo.pSpecializationInfo = shader.getSpecializationInfo();
    return shaderCreateInfo;
}

void GraphicsPipelineWrapper::createShaders(bool linked, bool binary)
{
    std::vector<vk::VkShaderCreateInfoEXT> createInfos;
    if (m_internalData->vertexShader.isSet())
        createInfos.push_back(makeShaderCreateInfo(vk::VK_SHADER_STAGE_VERTEX_BIT, m_internalData->vertexShader, linked,
                                                   binary, m_internalData->fragmentShader));
    if (m_internalData->tessellationControlShader.isSet())
        createInfos.push_back(makeShaderCreateInfo(vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
                                                   m_internalData->tessellationControlShader, linked, binary,
                                                   m_internalData->fragmentShader));
    if (m_internalData->tessellationEvaluationShader.isSet())
        createInfos.push_back(makeShaderCreateInfo(vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
                                                   m_internalData->tessellationEvaluationShader, linked, binary,
                                                   m_internalData->fragmentShader));
    if (m_internalData->geometryShader.isSet())
        createInfos.push_back(makeShaderCreateInfo(vk::VK_SHADER_STAGE_GEOMETRY_BIT, m_internalData->geometryShader,
                                                   linked, binary, m_internalData->fragmentShader));
    if (m_internalData->fragmentShader.isSet())
        createInfos.push_back(makeShaderCreateInfo(vk::VK_SHADER_STAGE_FRAGMENT_BIT, m_internalData->fragmentShader,
                                                   linked, binary, m_internalData->vertexShader));
    if (m_internalData->taskShader.isSet())
        createInfos.push_back(makeShaderCreateInfo(vk::VK_SHADER_STAGE_TASK_BIT_EXT, m_internalData->taskShader, linked,
                                                   binary, m_internalData->fragmentShader));
    if (m_internalData->meshShader.isSet())
    {
        createInfos.push_back(makeShaderCreateInfo(vk::VK_SHADER_STAGE_MESH_BIT_EXT, m_internalData->meshShader, linked,
                                                   binary, m_internalData->fragmentShader));
        if (!m_internalData->taskShader.isSet())
            createInfos.back().flags |= vk::VK_SHADER_CREATE_NO_TASK_SHADER_BIT_EXT;
    }

    std::vector<VkShaderEXT> shaders(createInfos.size());
    m_internalData->vk.createShadersEXT(m_internalData->device, (uint32_t)createInfos.size(), createInfos.data(),
                                        nullptr, shaders.data());
    uint32_t shaderIndex = 0;
    if (m_internalData->vertexShader.isSet())
        m_internalData->vertexShader.setShader(
            Move<VkShaderEXT>(check<VkShaderEXT>(shaders[shaderIndex++]),
                              Deleter<VkShaderEXT>(m_internalData->vk, m_internalData->device, nullptr)));
    if (m_internalData->tessellationControlShader.isSet())
        m_internalData->tessellationControlShader.setShader(
            Move<VkShaderEXT>(check<VkShaderEXT>(shaders[shaderIndex++]),
                              Deleter<VkShaderEXT>(m_internalData->vk, m_internalData->device, nullptr)));
    if (m_internalData->tessellationEvaluationShader.isSet())
        m_internalData->tessellationEvaluationShader.setShader(
            Move<VkShaderEXT>(check<VkShaderEXT>(shaders[shaderIndex++]),
                              Deleter<VkShaderEXT>(m_internalData->vk, m_internalData->device, nullptr)));
    if (m_internalData->geometryShader.isSet())
        m_internalData->geometryShader.setShader(
            Move<VkShaderEXT>(check<VkShaderEXT>(shaders[shaderIndex++]),
                              Deleter<VkShaderEXT>(m_internalData->vk, m_internalData->device, nullptr)));
    if (m_internalData->fragmentShader.isSet())
        m_internalData->fragmentShader.setShader(
            Move<VkShaderEXT>(check<VkShaderEXT>(shaders[shaderIndex++]),
                              Deleter<VkShaderEXT>(m_internalData->vk, m_internalData->device, nullptr)));
    if (m_internalData->taskShader.isSet())
        m_internalData->taskShader.setShader(
            Move<VkShaderEXT>(check<VkShaderEXT>(shaders[shaderIndex++]),
                              Deleter<VkShaderEXT>(m_internalData->vk, m_internalData->device, nullptr)));
    if (m_internalData->meshShader.isSet())
        m_internalData->meshShader.setShader(
            Move<VkShaderEXT>(check<VkShaderEXT>(shaders[shaderIndex++]),
                              Deleter<VkShaderEXT>(m_internalData->vk, m_internalData->device, nullptr)));
}
#endif

void GraphicsPipelineWrapper::buildPipeline(const VkPipelineCache pipelineCache, const VkPipeline basePipelineHandle,
                                            const int32_t basePipelineIndex,
                                            PipelineCreationFeedbackCreateInfoWrapper creationFeedback, void *pNext)
{
    // make sure we are not trying to build pipeline second time
    DE_ASSERT(m_pipelineFinal.get() == VK_NULL_HANDLE);

    // make sure all states were set
    DE_ASSERT(m_internalData &&
              (m_internalData->setupState == (PSS_VERTEX_INPUT_INTERFACE | PSS_PRE_RASTERIZATION_SHADERS |
                                              PSS_FRAGMENT_SHADER | PSS_FRAGMENT_OUTPUT_INTERFACE)));

    // Unreference variables that are not used in Vulkan SC. No need to put this in ifdef.
    DE_UNREF(creationFeedback);
    DE_UNREF(pNext);

    VkGraphicsPipelineCreateInfo *pointerToCreateInfo = &m_internalData->monolithicPipelineCreateInfo;

    if (isConstructionTypeShaderObject(m_internalData->pipelineConstructionType))
    {
#ifndef CTS_USES_VULKANSC
        // Dynamic states that don't require additional extensions
        std::vector<vk::VkDynamicState> dynamicStates = {
            vk::VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,
            vk::VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,
            vk::VK_DYNAMIC_STATE_LINE_WIDTH,
            vk::VK_DYNAMIC_STATE_DEPTH_BIAS,
            vk::VK_DYNAMIC_STATE_BLEND_CONSTANTS,
            vk::VK_DYNAMIC_STATE_DEPTH_BOUNDS,
            vk::VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
            vk::VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
            vk::VK_DYNAMIC_STATE_STENCIL_REFERENCE,
            vk::VK_DYNAMIC_STATE_CULL_MODE_EXT,
            vk::VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE_EXT,
            vk::VK_DYNAMIC_STATE_DEPTH_COMPARE_OP_EXT,
            vk::VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE_EXT,
            vk::VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE_EXT,
            vk::VK_DYNAMIC_STATE_FRONT_FACE_EXT,
            vk::VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY_EXT,
            vk::VK_DYNAMIC_STATE_STENCIL_OP_EXT,
            vk::VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE_EXT,
            vk::VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE_EXT,
            vk::VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE_EXT,
            vk::VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE_EXT,
            vk::VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE_EXT,
            vk::VK_DYNAMIC_STATE_LOGIC_OP_EXT,
            vk::VK_DYNAMIC_STATE_PATCH_CONTROL_POINTS_EXT,
            vk::VK_DYNAMIC_STATE_TESSELLATION_DOMAIN_ORIGIN_EXT,
            vk::VK_DYNAMIC_STATE_DEPTH_CLAMP_ENABLE_EXT,
            vk::VK_DYNAMIC_STATE_POLYGON_MODE_EXT,
            vk::VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT,
            vk::VK_DYNAMIC_STATE_SAMPLE_MASK_EXT,
            vk::VK_DYNAMIC_STATE_ALPHA_TO_COVERAGE_ENABLE_EXT,
            vk::VK_DYNAMIC_STATE_ALPHA_TO_ONE_ENABLE_EXT,
            vk::VK_DYNAMIC_STATE_LOGIC_OP_ENABLE_EXT,
            vk::VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT,
            vk::VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT,
            vk::VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT,
            vk::VK_DYNAMIC_STATE_VERTEX_INPUT_EXT,
        };

        vk::VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures = initVulkanStructure();
        vk::VkPhysicalDeviceFeatures2 features                       = initVulkanStructure(&meshShaderFeatures);
        m_internalData->vki.getPhysicalDeviceFeatures2(m_internalData->physicalDevice, &features);

        m_internalData->tessellationShaderFeature = features.features.tessellationShader;
        m_internalData->geometryShaderFeature     = features.features.geometryShader;
        m_internalData->taskShaderFeature         = meshShaderFeatures.taskShader;
        m_internalData->meshShaderFeature         = meshShaderFeatures.meshShader;

        DE_ASSERT(m_internalData->extensionEnabled("VK_EXT_shader_object"));

        // Add dynamic states that are required for each enabled extension
        const auto dynStateFromExts = getShaderObjectDynamicStatesFromExtensions(m_internalData->deviceExtensions);
        dynamicStates.insert(end(dynamicStates), begin(dynStateFromExts), end(dynStateFromExts));

        // Remove dynamic states that were already set as dynamic for the pipeline
        // These dynamic state will already be set in the tests
        bool depthClampEnableDynamic = false;
        if (pointerToCreateInfo->pDynamicState)
        {
            for (uint32_t i = 0; i < pointerToCreateInfo->pDynamicState->dynamicStateCount; ++i)
            {
                if (pointerToCreateInfo->pDynamicState->pDynamicStates[i] == vk::VK_DYNAMIC_STATE_VIEWPORT)
                    dynamicStates.erase(std::remove(dynamicStates.begin(), dynamicStates.end(),
                                                    vk::VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT),
                                        dynamicStates.end());
                else if (pointerToCreateInfo->pDynamicState->pDynamicStates[i] == vk::VK_DYNAMIC_STATE_SCISSOR)
                    dynamicStates.erase(std::remove(dynamicStates.begin(), dynamicStates.end(),
                                                    vk::VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT),
                                        dynamicStates.end());
                else if (pointerToCreateInfo->pDynamicState->pDynamicStates[i] ==
                         vk::VK_DYNAMIC_STATE_COLOR_BLEND_ADVANCED_EXT)
                {
                    dynamicStates.erase(std::remove(dynamicStates.begin(), dynamicStates.end(),
                                                    vk::VK_DYNAMIC_STATE_COLOR_BLEND_ADVANCED_EXT),
                                        dynamicStates.end());
                    dynamicStates.erase(std::remove(dynamicStates.begin(), dynamicStates.end(),
                                                    vk::VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT),
                                        dynamicStates.end());
                }
                else if (pointerToCreateInfo->pDynamicState->pDynamicStates[i] == vk::VK_DYNAMIC_STATE_VERTEX_INPUT_EXT)
                {
                    dynamicStates.erase(
                        std::remove(dynamicStates.begin(), dynamicStates.end(), vk::VK_DYNAMIC_STATE_VERTEX_INPUT_EXT),
                        dynamicStates.end());
                    dynamicStates.erase(std::remove(dynamicStates.begin(), dynamicStates.end(),
                                                    vk::VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE_EXT),
                                        dynamicStates.end());
                }
                else
                    dynamicStates.erase(std::remove(dynamicStates.begin(), dynamicStates.end(),
                                                    pointerToCreateInfo->pDynamicState->pDynamicStates[i]),
                                        dynamicStates.end());

                if (pointerToCreateInfo->pDynamicState->pDynamicStates[i] ==
                    vk::VK_DYNAMIC_STATE_DEPTH_CLAMP_ENABLE_EXT)
                    depthClampEnableDynamic = true;
            }
        }

        m_internalData->shaderObjectDynamicStates = dynamicStates;

        // Save state needed for setting shader object dynamic state
        auto state = &m_internalData->pipelineCreateState;
        if (pointerToCreateInfo->pViewportState)
        {
            if (pointerToCreateInfo->pViewportState->pViewports)
            {
                state->viewports.resize(pointerToCreateInfo->pViewportState->viewportCount);
                for (uint32_t i = 0; i < pointerToCreateInfo->pViewportState->viewportCount; ++i)
                    state->viewports[i] = pointerToCreateInfo->pViewportState->pViewports[i];
            }

            if (pointerToCreateInfo->pViewportState->pScissors)
            {
                state->scissors.resize(pointerToCreateInfo->pViewportState->scissorCount);
                for (uint32_t i = 0; i < pointerToCreateInfo->pViewportState->scissorCount; ++i)
                    state->scissors[i] = pointerToCreateInfo->pViewportState->pScissors[i];
            }

            const auto depthClipControl = findStructure<VkPipelineViewportDepthClipControlCreateInfoEXT>(
                pointerToCreateInfo->pViewportState->pNext);
            if (depthClipControl)
                state->negativeOneToOne = depthClipControl->negativeOneToOne;
            const auto viewportShadingRate = findStructure<VkPipelineViewportShadingRateImageStateCreateInfoNV>(
                pointerToCreateInfo->pViewportState->pNext);
            if (viewportShadingRate)
            {
                state->shadingRateImageEnable  = viewportShadingRate->shadingRateImageEnable;
                state->shadingRatePaletteCount = viewportShadingRate->viewportCount;
                state->shadingRatePalettes.resize(viewportShadingRate->viewportCount);
                state->shadingRatePaletteEntries.resize(viewportShadingRate->viewportCount);
                for (uint32_t i = 0; i < viewportShadingRate->viewportCount; ++i)
                {
                    state->shadingRatePalettes[i] = viewportShadingRate->pShadingRatePalettes[i];
                    state->shadingRatePaletteEntries[i].resize(
                        viewportShadingRate->pShadingRatePalettes[i].shadingRatePaletteEntryCount);
                    for (uint32_t j = 0; j < viewportShadingRate->pShadingRatePalettes[i].shadingRatePaletteEntryCount;
                         ++j)
                        state->shadingRatePaletteEntries[i][j] =
                            viewportShadingRate->pShadingRatePalettes[i].pShadingRatePaletteEntries[j];
                    state->shadingRatePalettes[i].pShadingRatePaletteEntries =
                        state->shadingRatePaletteEntries[i].data();
                }
            }
            const auto viewportSwizzle =
                findStructure<VkPipelineViewportSwizzleStateCreateInfoNV>(pointerToCreateInfo->pViewportState->pNext);
            if (viewportSwizzle)
            {
                state->viewportSwizzles.resize(viewportSwizzle->viewportCount);
                for (uint32_t i = 0; i < viewportSwizzle->viewportCount; ++i)
                    state->viewportSwizzles[i] = viewportSwizzle->pViewportSwizzles[i];
            }
            const auto viewportWScaling =
                findStructure<VkPipelineViewportWScalingStateCreateInfoNV>(pointerToCreateInfo->pViewportState->pNext);
            if (viewportWScaling)
            {
                state->viewportWScalingEnable = viewportWScaling->viewportWScalingEnable;
                state->viewportWScalingCount  = viewportWScaling->viewportCount;
                state->viewportWScalings.resize(viewportWScaling->viewportCount);
                for (uint32_t i = 0; i < viewportWScaling->viewportCount; ++i)
                    state->viewportWScalings[i] = viewportWScaling->pViewportWScalings[i];
            }
            const auto coarseSampleOrder = findStructure<VkPipelineViewportCoarseSampleOrderStateCreateInfoNV>(
                pointerToCreateInfo->pViewportState->pNext);
            if (coarseSampleOrder)
            {
                state->coarseSampleOrderType        = coarseSampleOrder->sampleOrderType;
                state->coarseCustomSampleOrderCount = coarseSampleOrder->customSampleOrderCount;
                state->coarseCustomSampleOrders.resize(coarseSampleOrder->customSampleOrderCount);
                state->coarseSampleLocations.resize(coarseSampleOrder->customSampleOrderCount);
                for (uint32_t i = 0; i < coarseSampleOrder->customSampleOrderCount; ++i)
                {
                    state->coarseCustomSampleOrders[i] = coarseSampleOrder->pCustomSampleOrders[i];
                    state->coarseSampleLocations[i].resize(coarseSampleOrder->pCustomSampleOrders[i].sampleCount);
                    for (uint32_t j = 0; j < coarseSampleOrder->pCustomSampleOrders[i].sampleCount; ++j)
                        state->coarseSampleLocations[i][j] =
                            coarseSampleOrder->pCustomSampleOrders[i].pSampleLocations[j];
                    state->coarseCustomSampleOrders[i].pSampleLocations = state->coarseSampleLocations[i].data();
                }
            }
        }

        if (pointerToCreateInfo->pRasterizationState)
        {
            state->lineWidth               = pointerToCreateInfo->pRasterizationState->lineWidth;
            state->depthBiasConstantFactor = pointerToCreateInfo->pRasterizationState->depthBiasConstantFactor;
            state->depthBiasClamp          = pointerToCreateInfo->pRasterizationState->depthBiasClamp;
            state->depthBiasSlopeFactor    = pointerToCreateInfo->pRasterizationState->depthBiasSlopeFactor;
            state->cullMode                = pointerToCreateInfo->pRasterizationState->cullMode;
            state->frontFace               = pointerToCreateInfo->pRasterizationState->frontFace;
            state->depthBiasEnable         = pointerToCreateInfo->pRasterizationState->depthBiasEnable;
            state->rasterizerDiscardEnable = pointerToCreateInfo->pRasterizationState->rasterizerDiscardEnable;
            const auto conservative        = findStructure<VkPipelineRasterizationConservativeStateCreateInfoEXT>(
                pointerToCreateInfo->pRasterizationState->pNext);
            if (conservative)
            {
                state->conservativeRasterizationMode    = conservative->conservativeRasterizationMode;
                state->extraPrimitiveOverestimationSize = conservative->extraPrimitiveOverestimationSize;
            }
            state->depthClampEnable = pointerToCreateInfo->pRasterizationState->depthClampEnable;
            const auto depthClip    = findStructure<VkPipelineRasterizationDepthClipStateCreateInfoEXT>(
                pointerToCreateInfo->pRasterizationState->pNext);
            if (depthClip)
                state->depthClipEnable = depthClip->depthClipEnable;
            else
                state->depthClipEnable =
                    !pointerToCreateInfo->pRasterizationState->depthClampEnable && !depthClampEnableDynamic;

            const auto rasterizationLine = findStructure<VkPipelineRasterizationLineStateCreateInfoEXT>(
                pointerToCreateInfo->pRasterizationState->pNext);
            if (rasterizationLine)
            {
                state->lineRasterizationMode = rasterizationLine->lineRasterizationMode;
                state->stippledLineEnable    = rasterizationLine->stippledLineEnable;
                state->lineStippleFactor     = rasterizationLine->lineStippleFactor;
                state->lineStipplePattern    = rasterizationLine->lineStipplePattern;
            }
            const auto rasterizationStream = findStructure<VkPipelineRasterizationStateStreamCreateInfoEXT>(
                pointerToCreateInfo->pRasterizationState->pNext);
            if (rasterizationStream)
                state->rasterizationStream = rasterizationStream->rasterizationStream;
            state->polygonMode         = pointerToCreateInfo->pRasterizationState->polygonMode;
            const auto provokingVertex = findStructure<VkPipelineRasterizationProvokingVertexStateCreateInfoEXT>(
                pointerToCreateInfo->pRasterizationState->pNext);
            if (provokingVertex)
                state->provokingVertexMode = provokingVertex->provokingVertexMode;
            const auto depthBiasRepresentationInfo =
                findStructure<VkDepthBiasRepresentationInfoEXT>(pointerToCreateInfo->pRasterizationState->pNext);
            if (depthBiasRepresentationInfo)
            {
                state->depthBiasRepresentation = depthBiasRepresentationInfo->depthBiasRepresentation;
                state->depthBiasExact          = depthBiasRepresentationInfo->depthBiasExact;
            }
        }
        if (pointerToCreateInfo->pColorBlendState)
        {
            memcpy(&state->blendConstants, pointerToCreateInfo->pColorBlendState->blendConstants, sizeof(float) * 4);
            state->logicOp                = pointerToCreateInfo->pColorBlendState->logicOp;
            const auto blendAdvancedState = findStructure<VkPipelineColorBlendAdvancedStateCreateInfoEXT>(
                pointerToCreateInfo->pColorBlendState->pNext);
            if (blendAdvancedState)
            {
                state->colorBlendAdvanced.resize(pointerToCreateInfo->pColorBlendState->attachmentCount);
                for (uint32_t i = 0; i < pointerToCreateInfo->pColorBlendState->attachmentCount; ++i)
                {
                    if (pointerToCreateInfo->pColorBlendState->pAttachments)
                        state->colorBlendAdvanced[i].advancedBlendOp =
                            pointerToCreateInfo->pColorBlendState->pAttachments[i].colorBlendOp;
                    state->colorBlendAdvanced[i].srcPremultiplied = blendAdvancedState->srcPremultiplied;
                    state->colorBlendAdvanced[i].dstPremultiplied = blendAdvancedState->dstPremultiplied;
                    state->colorBlendAdvanced[i].blendOverlap     = blendAdvancedState->blendOverlap;
                    state->colorBlendAdvanced[i].clampResults     = VK_FALSE;
                }
            }
            state->colorBlendEnables.resize(pointerToCreateInfo->pColorBlendState->attachmentCount);
            state->blendEquations.resize(pointerToCreateInfo->pColorBlendState->attachmentCount);
            state->colorWriteMasks.resize(pointerToCreateInfo->pColorBlendState->attachmentCount);
            for (uint32_t i = 0; i < (uint32_t)pointerToCreateInfo->pColorBlendState->attachmentCount; ++i)
            {
                if (pointerToCreateInfo->pColorBlendState->pAttachments)
                {
                    state->colorBlendEnables[i] = pointerToCreateInfo->pColorBlendState->pAttachments[i].blendEnable;
                    state->blendEquations[i].srcColorBlendFactor =
                        pointerToCreateInfo->pColorBlendState->pAttachments[i].srcColorBlendFactor;
                    state->blendEquations[i].dstColorBlendFactor =
                        pointerToCreateInfo->pColorBlendState->pAttachments[i].dstColorBlendFactor;
                    state->blendEquations[i].colorBlendOp =
                        pointerToCreateInfo->pColorBlendState->pAttachments[i].colorBlendOp;
                    state->blendEquations[i].srcAlphaBlendFactor =
                        pointerToCreateInfo->pColorBlendState->pAttachments[i].srcAlphaBlendFactor;
                    state->blendEquations[i].dstAlphaBlendFactor =
                        pointerToCreateInfo->pColorBlendState->pAttachments[i].dstAlphaBlendFactor;
                    state->blendEquations[i].alphaBlendOp =
                        pointerToCreateInfo->pColorBlendState->pAttachments[i].alphaBlendOp;
                    state->colorWriteMasks[i] = pointerToCreateInfo->pColorBlendState->pAttachments[i].colorWriteMask;
                }

                // colorBlendOp and alphaBlendOp must not be advanced blend operations and they will be set with colorBlendAdvanced
                if (blendAdvancedState)
                {
                    state->blendEquations[i].colorBlendOp = vk::VK_BLEND_OP_ADD;
                    state->blendEquations[i].alphaBlendOp = vk::VK_BLEND_OP_ADD;
                }
            }
            state->logicOpEnable = pointerToCreateInfo->pColorBlendState->logicOpEnable;
            const auto colorWrite =
                findStructure<VkPipelineColorWriteCreateInfoEXT>(pointerToCreateInfo->pColorBlendState->pNext);
            if (colorWrite)
            {
                state->colorWriteEnableAttachmentCount = colorWrite->attachmentCount;
                state->colorWriteEnables.resize(colorWrite->attachmentCount);
                for (uint32_t i = 0; i < colorWrite->attachmentCount; ++i)
                    state->colorWriteEnables[i] = colorWrite->pColorWriteEnables[i];
            }
        }
        if (pointerToCreateInfo->pDepthStencilState)
        {
            state->minDepthBounds        = pointerToCreateInfo->pDepthStencilState->minDepthBounds;
            state->maxDepthBounds        = pointerToCreateInfo->pDepthStencilState->maxDepthBounds;
            state->stencilFront          = pointerToCreateInfo->pDepthStencilState->front;
            state->stencilBack           = pointerToCreateInfo->pDepthStencilState->back;
            state->depthBoundsTestEnable = pointerToCreateInfo->pDepthStencilState->depthBoundsTestEnable;
            state->depthCompareOp        = pointerToCreateInfo->pDepthStencilState->depthCompareOp;
            state->depthTestEnable       = pointerToCreateInfo->pDepthStencilState->depthTestEnable;
            state->depthWriteEnable      = pointerToCreateInfo->pDepthStencilState->depthWriteEnable;
            state->stencilTestEnable     = pointerToCreateInfo->pDepthStencilState->stencilTestEnable;
        }
        if (pointerToCreateInfo->pInputAssemblyState)
        {
            state->topology               = pointerToCreateInfo->pInputAssemblyState->topology;
            state->primitiveRestartEnable = pointerToCreateInfo->pInputAssemblyState->primitiveRestartEnable;
        }
        if (pointerToCreateInfo->pVertexInputState)
        {
            state->attributes.resize(pointerToCreateInfo->pVertexInputState->vertexAttributeDescriptionCount);
            state->bindings.resize(pointerToCreateInfo->pVertexInputState->vertexBindingDescriptionCount);
            for (uint32_t i = 0; i < pointerToCreateInfo->pVertexInputState->vertexAttributeDescriptionCount; ++i)
            {
                state->attributes[i] = initVulkanStructure();
                state->attributes[i].location =
                    pointerToCreateInfo->pVertexInputState->pVertexAttributeDescriptions[i].location;
                state->attributes[i].binding =
                    pointerToCreateInfo->pVertexInputState->pVertexAttributeDescriptions[i].binding;
                state->attributes[i].format =
                    pointerToCreateInfo->pVertexInputState->pVertexAttributeDescriptions[i].format;
                state->attributes[i].offset =
                    pointerToCreateInfo->pVertexInputState->pVertexAttributeDescriptions[i].offset;
            }

            const auto divisorInfo = findStructure<VkPipelineVertexInputDivisorStateCreateInfoEXT>(
                pointerToCreateInfo->pVertexInputState->pNext);

            for (uint32_t i = 0; i < pointerToCreateInfo->pVertexInputState->vertexBindingDescriptionCount; ++i)
            {
                state->bindings[i] = initVulkanStructure();
                state->bindings[i].binding =
                    pointerToCreateInfo->pVertexInputState->pVertexBindingDescriptions[i].binding;
                state->bindings[i].stride =
                    pointerToCreateInfo->pVertexInputState->pVertexBindingDescriptions[i].stride;
                state->bindings[i].inputRate =
                    pointerToCreateInfo->pVertexInputState->pVertexBindingDescriptions[i].inputRate;
                state->bindings[i].divisor = 1;
                if (divisorInfo)
                {
                    for (uint32_t j = 0; j < divisorInfo->vertexBindingDivisorCount; ++j)
                    {
                        if (state->bindings[i].binding == divisorInfo->pVertexBindingDivisors[j].binding)
                        {
                            state->bindings[i].divisor = divisorInfo->pVertexBindingDivisors[i].divisor;
                        }
                    }
                }
            }
        }
        if (pointerToCreateInfo->pTessellationState)
        {
            state->patchControlPoints           = pointerToCreateInfo->pTessellationState->patchControlPoints;
            const auto tessellationDomainOrigin = findStructure<VkPipelineTessellationDomainOriginStateCreateInfo>(
                pointerToCreateInfo->pTessellationState->pNext);
            if (tessellationDomainOrigin)
                state->domainOrigin = tessellationDomainOrigin->domainOrigin;
        }
        if (pointerToCreateInfo->pMultisampleState)
        {
            state->alphaToCoverageEnable  = pointerToCreateInfo->pMultisampleState->alphaToCoverageEnable;
            state->alphaToOneEnable       = pointerToCreateInfo->pMultisampleState->alphaToOneEnable;
            const auto coverageModulation = findStructure<VkPipelineCoverageModulationStateCreateInfoNV>(
                pointerToCreateInfo->pMultisampleState->pNext);
            if (coverageModulation)
            {
                state->coverageModulationMode        = coverageModulation->coverageModulationMode;
                state->coverageModulationTableEnable = coverageModulation->coverageModulationTableEnable;
                state->coverageModulationTable.resize(coverageModulation->coverageModulationTableCount);
                for (uint32_t i = 0; i < (uint32_t)coverageModulation->coverageModulationTableCount; ++i)
                    state->coverageModulationTable[i] = coverageModulation->pCoverageModulationTable[i];
            }
            const auto coverageReduction = findStructure<VkPipelineCoverageReductionStateCreateInfoNV>(
                pointerToCreateInfo->pMultisampleState->pNext);
            if (coverageReduction)
                state->coverageReductionMode = coverageReduction->coverageReductionMode;
            const auto coverageToColor = findStructure<VkPipelineCoverageToColorStateCreateInfoNV>(
                pointerToCreateInfo->pMultisampleState->pNext);
            if (coverageToColor)
            {
                state->coverageToColorEnable   = coverageToColor->coverageToColorEnable;
                state->coverageToColorLocation = coverageToColor->coverageToColorLocation;
            }
            state->rasterizationSamples = pointerToCreateInfo->pMultisampleState->rasterizationSamples;
            const auto sampleLocations  = findStructure<VkPipelineSampleLocationsStateCreateInfoEXT>(
                pointerToCreateInfo->pMultisampleState->pNext);
            if (sampleLocations)
            {
                state->sampleLocationsEnable = sampleLocations->sampleLocationsEnable;
                state->sampleLocationsInfo   = sampleLocations->sampleLocationsInfo;
                state->pSampleLocations.resize(sampleLocations->sampleLocationsInfo.sampleLocationsCount);
                for (uint32_t i = 0; i < sampleLocations->sampleLocationsInfo.sampleLocationsCount; ++i)
                    state->pSampleLocations[i] = sampleLocations->sampleLocationsInfo.pSampleLocations[i];
                state->sampleLocationsInfo.pSampleLocations = state->pSampleLocations.data();
            }
            state->rasterizationSamples = pointerToCreateInfo->pMultisampleState->rasterizationSamples;
            uint32_t count =
                (pointerToCreateInfo->pMultisampleState->rasterizationSamples > vk::VK_SAMPLE_COUNT_32_BIT) ? 2 : 1;
            state->sampleMasks.resize(count, 0);
            for (uint32_t i = 0; i < count; ++i)
                if (pointerToCreateInfo->pMultisampleState->pSampleMask)
                    state->sampleMasks[i] = pointerToCreateInfo->pMultisampleState->pSampleMask[i];
                else
                    state->sampleMasks[i] =
                        0xFF; // If pSampleMask is NULL, it is treated as if the mask has all bits set to 1
        }
        const auto representativeFragment =
            findStructure<VkPipelineRepresentativeFragmentTestStateCreateInfoNV>(pointerToCreateInfo->pNext);
        if (representativeFragment)
        {
            state->representativeFragmentTestEnable = representativeFragment->representativeFragmentTestEnable;
        }
        const auto fragmentShadingRate = m_internalData->pFragmentShadingRateState;
        if (fragmentShadingRate)
        {
            state->fragmentShadingRateSize = fragmentShadingRate->fragmentSize;
            state->combinerOps[0]          = fragmentShadingRate->combinerOps[0];
            state->combinerOps[1]          = fragmentShadingRate->combinerOps[1];
        }
        const auto exclusiveScissor =
            findStructure<VkPipelineViewportExclusiveScissorStateCreateInfoNV>(pointerToCreateInfo->pNext);
        if (exclusiveScissor)
        {
            state->exclusiveScissorCount = exclusiveScissor->exclusiveScissorCount;
            state->exclussiveScissors.resize(exclusiveScissor->exclusiveScissorCount);
            for (uint32_t i = 0; i < exclusiveScissor->exclusiveScissorCount; ++i)
                state->exclussiveScissors[i] = exclusiveScissor->pExclusiveScissors[i];
        }
        const auto discardRectangle =
            findStructure<VkPipelineDiscardRectangleStateCreateInfoEXT>(pointerToCreateInfo->pNext);
        if (discardRectangle)
        {
            state->discardRectangleEnable = discardRectangle->discardRectangleCount > 0;
            state->discardRectangles.resize(discardRectangle->discardRectangleCount);
            for (uint32_t i = 0; i < discardRectangle->discardRectangleCount; ++i)
                state->discardRectangles[i] = discardRectangle->pDiscardRectangles[i];
            state->discardRectangleMode = discardRectangle->discardRectangleMode;
        }
        if (pointerToCreateInfo->flags & VK_PIPELINE_CREATE_COLOR_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT)
            state->attachmentFeedbackLoopEnable |= VK_IMAGE_ASPECT_COLOR_BIT;
        if (pointerToCreateInfo->flags & VK_PIPELINE_CREATE_DEPTH_STENCIL_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT)
            state->attachmentFeedbackLoopEnable |= VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

        bool linked =
            m_internalData->pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_LINKED_SPIRV ||
            m_internalData->pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_LINKED_BINARY;
        bool binary =
            m_internalData->pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_UNLINKED_BINARY ||
            m_internalData->pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_LINKED_BINARY;
        createShaders(linked, false);
        if (binary)
        {
            createShaders(linked, true);
        }
#endif
    }
    else
    {
#ifndef CTS_USES_VULKANSC
        VkGraphicsPipelineCreateInfo linkedCreateInfo = initVulkanStructure();
        std::vector<VkPipeline> rawPipelines;
        VkPipelineLibraryCreateInfoKHR linkingInfo{
            VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR, // VkStructureType sType;
            creationFeedback.ptr,                               // const void* pNext;
            0u,                                                 // uint32_t libraryCount;
            nullptr,                                            // const VkPipeline* pLibraries;
        };

        if (isConstructionTypeLibrary(m_internalData->pipelineConstructionType))
        {
            for (const auto &pipelinePtr : m_pipelineParts)
            {
                const auto &pipeline = pipelinePtr.get();
                if (pipeline != VK_NULL_HANDLE)
                    rawPipelines.push_back(pipeline);
            }

            linkingInfo.libraryCount = static_cast<uint32_t>(rawPipelines.size());
            linkingInfo.pLibraries   = de::dataOrNull(rawPipelines);

            // If a test hits the following assert, it's likely missing a call
            // to the setMonolithicPipelineLayout() method. Related VUs:
            //   * VUID-VkGraphicsPipelineCreateInfo-flags-06642
            //   * VUID-VkGraphicsPipelineCreateInfo-None-07826
            //   * VUID-VkGraphicsPipelineCreateInfo-layout-07827
            //   * VUID-VkGraphicsPipelineCreateInfo-flags-06729
            //   * VUID-VkGraphicsPipelineCreateInfo-flags-06730
            DE_ASSERT(m_internalData->monolithicPipelineCreateInfo.layout != VK_NULL_HANDLE);
            linkedCreateInfo.layout = m_internalData->monolithicPipelineCreateInfo.layout;
            linkedCreateInfo.flags  = m_internalData->pipelineFlags;
            linkedCreateInfo.pNext  = &linkingInfo;

            pointerToCreateInfo = &linkedCreateInfo;

            if (m_internalData->pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY)
                linkedCreateInfo.flags |= VK_PIPELINE_CREATE_LINK_TIME_OPTIMIZATION_BIT_EXT;

            if (m_internalData->failOnCompileWhenLinking)
                linkedCreateInfo.flags |= VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT;
        }
        else
        {
            // note: there might be other structures in the chain already
            void *firstStructInChain = static_cast<void *>(pointerToCreateInfo);
            addToChain(&firstStructInChain, creationFeedback.ptr);
            addToChain(&firstStructInChain, m_internalData->pFragmentShadingRateState);
            addToChain(&firstStructInChain, m_internalData->pRepresentativeFragmentTestState.ptr);
            addToChain(&firstStructInChain, m_internalData->pRenderingState.ptr);
            addToChain(&firstStructInChain, m_internalData->pRenderingInputAttachmentIndex.ptr);
            addToChain(&firstStructInChain, m_internalData->pRenderingAttachmentLocation.ptr);
            addToChain(&firstStructInChain, m_internalData->pPipelineRobustnessState.ptr);
            addToChain(&firstStructInChain, pNext);
        }

        if (m_internalData->pipelineFlags2)
        {
            void *firstStructInChain = static_cast<void *>(pointerToCreateInfo);
            auto &flags2CreateInfo   = m_internalData->pipelinePartFlags2CreateInfo[0];
            flags2CreateInfo.flags   = m_internalData->pipelineFlags2 | translateCreateFlag(pointerToCreateInfo->flags);
            addToChain(&firstStructInChain, &flags2CreateInfo);
            pointerToCreateInfo->flags = 0u;
        }
#endif // CTS_USES_VULKANSC

        pointerToCreateInfo->basePipelineHandle = basePipelineHandle;
        pointerToCreateInfo->basePipelineIndex  = basePipelineIndex;

        m_pipelineFinal =
            makeGraphicsPipeline(m_internalData->vk, m_internalData->device, pipelineCache, pointerToCreateInfo);
    }
}

bool GraphicsPipelineWrapper::isShaderObjectDynamic(vk::VkDynamicState dynamicState) const
{
    return std::find(m_internalData->shaderObjectDynamicStates.begin(), m_internalData->shaderObjectDynamicStates.end(),
                     dynamicState) != m_internalData->shaderObjectDynamicStates.end();
}

void GraphicsPipelineWrapper::setShaderObjectDynamicStates(vk::VkCommandBuffer cmdBuffer) const
{
    DE_UNREF(cmdBuffer);
#ifndef CTS_USES_VULKANSC
    const auto &vk   = m_internalData->vk;
    const auto state = &m_internalData->pipelineCreateState;

    // Some dynamic state only need to be set when these conditions are met
    const bool meshOrTask        = m_internalData->meshShader.isSet() || m_internalData->taskShader.isSet();
    const bool tese              = m_internalData->tessellationEvaluationShader.isSet();
    const bool topologyPatchList = !isShaderObjectDynamic(vk::VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY) ||
                                   state->topology == VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
    const bool rasterizerDiscardDisabled =
        !isShaderObjectDynamic(vk::VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE) || !state->rasterizerDiscardEnable;
    const bool polygonModeLine =
        !isShaderObjectDynamic(vk::VK_DYNAMIC_STATE_POLYGON_MODE_EXT) || state->polygonMode == vk::VK_POLYGON_MODE_LINE;
    const bool topologyLine = !isShaderObjectDynamic(vk::VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY) ||
                              state->topology == vk::VK_PRIMITIVE_TOPOLOGY_LINE_LIST ||
                              state->topology == vk::VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY ||
                              state->topology == vk::VK_PRIMITIVE_TOPOLOGY_LINE_STRIP ||
                              state->topology == vk::VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY;
    const bool depthTestEnabled =
        !isShaderObjectDynamic(vk::VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE) || state->depthTestEnable;
    const bool depthBoundsTestEnabled =
        !isShaderObjectDynamic(vk::VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE) || state->depthBoundsTestEnable;
    const bool depthBiasEnabled =
        !isShaderObjectDynamic(vk::VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE) || state->depthBiasEnable;
    const bool stencilTestEnabled =
        !isShaderObjectDynamic(vk::VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE) || state->stencilTestEnable;
    const bool logicOpEnabled =
        !isShaderObjectDynamic(vk::VK_DYNAMIC_STATE_LOGIC_OP_ENABLE_EXT) || state->logicOpEnable;
    const bool discardRectangle =
        !isShaderObjectDynamic(vk::VK_DYNAMIC_STATE_DISCARD_RECTANGLE_ENABLE_EXT) || state->discardRectangleEnable;
    const bool sampleLocationsEnabled =
        !isShaderObjectDynamic(vk::VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_ENABLE_EXT) || state->sampleLocationsEnable;
    const bool stippledLineEnabled =
        !isShaderObjectDynamic(vk::VK_DYNAMIC_STATE_LINE_STIPPLE_ENABLE_EXT) || state->stippledLineEnable;
    bool blendFactorConstant = !isShaderObjectDynamic(vk::VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT);
    for (const auto &blend : state->blendEquations)
    {
        if (blend.srcColorBlendFactor == vk::VK_BLEND_FACTOR_CONSTANT_COLOR ||
            blend.srcColorBlendFactor == vk::VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR ||
            blend.srcColorBlendFactor == vk::VK_BLEND_FACTOR_CONSTANT_ALPHA ||
            blend.srcColorBlendFactor == vk::VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA ||
            blend.dstColorBlendFactor == vk::VK_BLEND_FACTOR_CONSTANT_COLOR ||
            blend.dstColorBlendFactor == vk::VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR ||
            blend.dstColorBlendFactor == vk::VK_BLEND_FACTOR_CONSTANT_ALPHA ||
            blend.dstColorBlendFactor == vk::VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA ||
            blend.srcAlphaBlendFactor == vk::VK_BLEND_FACTOR_CONSTANT_COLOR ||
            blend.srcAlphaBlendFactor == vk::VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR ||
            blend.srcAlphaBlendFactor == vk::VK_BLEND_FACTOR_CONSTANT_ALPHA ||
            blend.srcAlphaBlendFactor == vk::VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA ||
            blend.dstAlphaBlendFactor == vk::VK_BLEND_FACTOR_CONSTANT_COLOR ||
            blend.dstAlphaBlendFactor == vk::VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR ||
            blend.dstAlphaBlendFactor == vk::VK_BLEND_FACTOR_CONSTANT_ALPHA ||
            blend.dstAlphaBlendFactor == vk::VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA)

            blendFactorConstant = true;
    }

    for (const auto dynamicState : m_internalData->shaderObjectDynamicStates)
    {
        switch (dynamicState)
        {
        case vk::VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT_EXT:
            if (!state->viewports.empty())
                vk.cmdSetViewportWithCount(cmdBuffer, (uint32_t)state->viewports.size(), state->viewports.data());
            break;
        case vk::VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT_EXT:
            if (!state->scissors.empty())
                vk.cmdSetScissorWithCount(cmdBuffer, (uint32_t)state->scissors.size(), state->scissors.data());
            break;
        case vk::VK_DYNAMIC_STATE_LINE_WIDTH:
            if (polygonModeLine || topologyLine)
                vk.cmdSetLineWidth(cmdBuffer, state->lineWidth);
            break;
        case vk::VK_DYNAMIC_STATE_DEPTH_BIAS:
            if (rasterizerDiscardDisabled && depthBiasEnabled)
            {
                if (m_internalData->extensionEnabled("VK_EXT_depth_bias_control"))
                {
                    VkDepthBiasRepresentationInfoEXT depthBiasRepresentationInfo = vk::initVulkanStructure();
                    depthBiasRepresentationInfo.depthBiasRepresentation          = state->depthBiasRepresentation;
                    depthBiasRepresentationInfo.depthBiasExact                   = state->depthBiasExact;

                    vk::VkDepthBiasInfoEXT depthBiasInfo  = vk::initVulkanStructure(&depthBiasRepresentationInfo);
                    depthBiasInfo.depthBiasConstantFactor = state->depthBiasConstantFactor;
                    depthBiasInfo.depthBiasClamp          = state->depthBiasClamp;
                    depthBiasInfo.depthBiasSlopeFactor    = state->depthBiasSlopeFactor;
                    vk.cmdSetDepthBias2EXT(cmdBuffer, &depthBiasInfo);
                }
                else
                {
                    vk.cmdSetDepthBias(cmdBuffer, state->depthBiasConstantFactor, state->depthBiasClamp,
                                       state->depthBiasSlopeFactor);
                }
            }
            break;
        case vk::VK_DYNAMIC_STATE_BLEND_CONSTANTS:
            if (rasterizerDiscardDisabled && blendFactorConstant)
                vk.cmdSetBlendConstants(cmdBuffer, state->blendConstants);
            break;
        case vk::VK_DYNAMIC_STATE_DEPTH_BOUNDS:
            if (rasterizerDiscardDisabled && depthBoundsTestEnabled)
                vk.cmdSetDepthBounds(cmdBuffer, state->minDepthBounds, state->maxDepthBounds);
            break;
        case vk::VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK:
        {
            vk.cmdSetStencilCompareMask(cmdBuffer, vk::VK_STENCIL_FACE_FRONT_BIT, state->stencilFront.compareMask);
            vk.cmdSetStencilCompareMask(cmdBuffer, vk::VK_STENCIL_FACE_BACK_BIT, state->stencilBack.compareMask);
        }
        break;
        case vk::VK_DYNAMIC_STATE_STENCIL_WRITE_MASK:
        {
            vk.cmdSetStencilWriteMask(cmdBuffer, vk::VK_STENCIL_FACE_FRONT_BIT, state->stencilFront.writeMask);
            vk.cmdSetStencilWriteMask(cmdBuffer, vk::VK_STENCIL_FACE_BACK_BIT, state->stencilBack.writeMask);
        }
        break;
        case vk::VK_DYNAMIC_STATE_STENCIL_REFERENCE:
        {
            vk.cmdSetStencilReference(cmdBuffer, vk::VK_STENCIL_FACE_FRONT_BIT, state->stencilFront.reference);
            vk.cmdSetStencilReference(cmdBuffer, vk::VK_STENCIL_FACE_BACK_BIT, state->stencilBack.reference);
        }
        break;
        case vk::VK_DYNAMIC_STATE_CULL_MODE_EXT:
            vk.cmdSetCullMode(cmdBuffer, state->cullMode);
            break;
        case vk::VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE_EXT:
            if (rasterizerDiscardDisabled)
                vk.cmdSetDepthBoundsTestEnable(cmdBuffer, state->depthBoundsTestEnable);
            break;
        case vk::VK_DYNAMIC_STATE_DEPTH_COMPARE_OP_EXT:
            if (rasterizerDiscardDisabled && depthTestEnabled)
                vk.cmdSetDepthCompareOp(cmdBuffer, state->depthCompareOp);
            break;
        case vk::VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE_EXT:
            if (rasterizerDiscardDisabled)
                vk.cmdSetDepthTestEnable(cmdBuffer, state->depthTestEnable);
            break;
        case vk::VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE_EXT:
            if (rasterizerDiscardDisabled)
                vk.cmdSetDepthWriteEnable(cmdBuffer, state->depthWriteEnable);
            break;
        case vk::VK_DYNAMIC_STATE_FRONT_FACE_EXT:
            vk.cmdSetFrontFace(cmdBuffer, state->frontFace);
            break;
        case vk::VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY_EXT:
            if (!meshOrTask)
                vk.cmdSetPrimitiveTopology(cmdBuffer, state->topology);
            break;
        case vk::VK_DYNAMIC_STATE_STENCIL_OP_EXT:
            if (rasterizerDiscardDisabled && stencilTestEnabled)
            {
                vk.cmdSetStencilOp(cmdBuffer, vk::VK_STENCIL_FACE_FRONT_BIT, state->stencilFront.failOp,
                                   state->stencilFront.passOp, state->stencilFront.depthFailOp,
                                   state->stencilFront.compareOp);
                vk.cmdSetStencilOp(cmdBuffer, vk::VK_STENCIL_FACE_BACK_BIT, state->stencilBack.failOp,
                                   state->stencilBack.passOp, state->stencilBack.depthFailOp,
                                   state->stencilBack.compareOp);
            }
            break;
        case vk::VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE_EXT:
            if (rasterizerDiscardDisabled)
                vk.cmdSetStencilTestEnable(cmdBuffer, state->stencilTestEnable);
            break;
        case vk::VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE_EXT:
            if (!meshOrTask)
                vk.cmdSetVertexInputEXT(cmdBuffer, (uint32_t)state->bindings.size(), state->bindings.data(),
                                        (uint32_t)state->attributes.size(), state->attributes.data());
            break;
        case vk::VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE_EXT:
            if (rasterizerDiscardDisabled)
                vk.cmdSetDepthBiasEnable(cmdBuffer, state->depthBiasEnable);
            break;
        case vk::VK_DYNAMIC_STATE_LOGIC_OP_EXT:
            if (rasterizerDiscardDisabled && logicOpEnabled)
                vk.cmdSetLogicOpEXT(cmdBuffer, state->logicOp);
            break;
        case vk::VK_DYNAMIC_STATE_PATCH_CONTROL_POINTS_EXT:
            if (topologyPatchList && !meshOrTask && tese)
                vk.cmdSetPatchControlPointsEXT(cmdBuffer, state->patchControlPoints);
            break;
        case vk::VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE_EXT:
            if (!meshOrTask)
                vk.cmdSetPrimitiveRestartEnable(cmdBuffer, state->primitiveRestartEnable);
            break;
        case vk::VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE_EXT:
            vk.cmdSetRasterizerDiscardEnable(cmdBuffer, state->rasterizerDiscardEnable);
            break;
        case vk::VK_DYNAMIC_STATE_ALPHA_TO_COVERAGE_ENABLE_EXT:
            vk.cmdSetAlphaToCoverageEnableEXT(cmdBuffer, state->alphaToCoverageEnable);
            break;
        case vk::VK_DYNAMIC_STATE_ALPHA_TO_ONE_ENABLE_EXT:
            vk.cmdSetAlphaToOneEnableEXT(cmdBuffer, state->alphaToOneEnable);
            break;
        case vk::VK_DYNAMIC_STATE_COLOR_BLEND_ADVANCED_EXT:
            if (!state->colorBlendAdvanced.empty())
            {
                for (uint32_t i = 0; i < (uint32_t)state->colorBlendAdvanced.size(); ++i)
                    if (!isShaderObjectDynamic(vk::VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT) ||
                        state->colorBlendEnables[i])
                        vk.cmdSetColorBlendAdvancedEXT(cmdBuffer, i, 1, &state->colorBlendAdvanced[i]);
            }
            break;
        case vk::VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT:
            if (rasterizerDiscardDisabled)
            {
                if (!state->colorBlendEnables.empty())
                {
                    vk.cmdSetColorBlendEnableEXT(cmdBuffer, 0, (uint32_t)state->colorBlendEnables.size(),
                                                 state->colorBlendEnables.data());
                }
                else
                {
                    VkBool32 disable = VK_FALSE;
                    vk.cmdSetColorBlendEnableEXT(cmdBuffer, 0, 1u, &disable);
                }
            }
            break;
        case vk::VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT:
            if (rasterizerDiscardDisabled)
            {
                if (!state->blendEquations.empty())
                {
                    vk.cmdSetColorBlendEquationEXT(cmdBuffer, 0, (uint32_t)state->blendEquations.size(),
                                                   state->blendEquations.data());
                }
                else
                {
                    vk::VkColorBlendEquationEXT blendEquation = {
                        VK_BLEND_FACTOR_SRC_ALPHA, // VkBlendFactor srcColorBlendFactor;
                        VK_BLEND_FACTOR_DST_ALPHA, // VkBlendFactor dstColorBlendFactor;
                        VK_BLEND_OP_ADD,           // VkBlendOp colorBlendOp;
                        VK_BLEND_FACTOR_SRC_ALPHA, // VkBlendFactor srcAlphaBlendFactor;
                        VK_BLEND_FACTOR_DST_ALPHA, // VkBlendFactor dstAlphaBlendFactor;
                        VK_BLEND_OP_ADD,           // VkBlendOp alphaBlendOp;
                    };
                    vk.cmdSetColorBlendEquationEXT(cmdBuffer, 0, 1u, &blendEquation);
                }
            }
            break;
        case vk::VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT:
            if (rasterizerDiscardDisabled)
            {
                if (!state->colorWriteMasks.empty())
                {
                    vk.cmdSetColorWriteMaskEXT(cmdBuffer, 0, (uint32_t)state->colorWriteMasks.size(),
                                               state->colorWriteMasks.data());
                }
                else
                {
                    VkColorComponentFlags colorWriteMask = 0u;
                    vk.cmdSetColorWriteMaskEXT(cmdBuffer, 0, 1u, &colorWriteMask);
                }
            }
            break;
        case vk::VK_DYNAMIC_STATE_CONSERVATIVE_RASTERIZATION_MODE_EXT:
            vk.cmdSetConservativeRasterizationModeEXT(cmdBuffer, state->conservativeRasterizationMode);
            break;
        case vk::VK_DYNAMIC_STATE_COVERAGE_MODULATION_MODE_NV:
            vk.cmdSetCoverageModulationModeNV(cmdBuffer, state->coverageModulationMode);
            break;
        case vk::VK_DYNAMIC_STATE_COVERAGE_MODULATION_TABLE_ENABLE_NV:
            vk.cmdSetCoverageModulationTableEnableNV(cmdBuffer, state->coverageModulationTableEnable);
            break;
        case vk::VK_DYNAMIC_STATE_COVERAGE_MODULATION_TABLE_NV:
            if (!state->coverageModulationTable.empty())
                vk.cmdSetCoverageModulationTableNV(cmdBuffer, (uint32_t)state->coverageModulationTable.size(),
                                                   state->coverageModulationTable.data());
            break;
        case vk::VK_DYNAMIC_STATE_COVERAGE_REDUCTION_MODE_NV:
            vk.cmdSetCoverageReductionModeNV(cmdBuffer, state->coverageReductionMode);
            break;
        case vk::VK_DYNAMIC_STATE_COVERAGE_TO_COLOR_ENABLE_NV:
            vk.cmdSetCoverageToColorEnableNV(cmdBuffer, state->coverageToColorEnable);
            break;
        case vk::VK_DYNAMIC_STATE_COVERAGE_TO_COLOR_LOCATION_NV:
            vk.cmdSetCoverageToColorLocationNV(cmdBuffer, state->coverageToColorLocation);
            break;
        case vk::VK_DYNAMIC_STATE_DEPTH_CLAMP_ENABLE_EXT:
            if (rasterizerDiscardDisabled)
                vk.cmdSetDepthClampEnableEXT(cmdBuffer, state->depthClampEnable);
            break;
        case vk::VK_DYNAMIC_STATE_DEPTH_CLIP_ENABLE_EXT:
            vk.cmdSetDepthClipEnableEXT(cmdBuffer, state->depthClipEnable);
            break;
        case vk::VK_DYNAMIC_STATE_DEPTH_CLIP_NEGATIVE_ONE_TO_ONE_EXT:
            vk.cmdSetDepthClipNegativeOneToOneEXT(cmdBuffer, state->negativeOneToOne);
            break;
        case vk::VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT:
            if (state->colorWriteEnableAttachmentCount > 0)
                vk.cmdSetColorWriteEnableEXT(cmdBuffer, state->colorWriteEnableAttachmentCount,
                                             state->colorWriteEnables.data());
            else
            {
                std::vector<VkBool32> enable(state->colorBlendEnables.empty() ? 1u : state->colorBlendEnables.size(),
                                             VK_TRUE);
                vk.cmdSetColorWriteEnableEXT(cmdBuffer, (uint32_t)enable.size(), enable.data());
            }
            break;
        case vk::VK_DYNAMIC_STATE_EXTRA_PRIMITIVE_OVERESTIMATION_SIZE_EXT:
            vk.cmdSetExtraPrimitiveOverestimationSizeEXT(cmdBuffer, state->extraPrimitiveOverestimationSize);
            break;
        case vk::VK_DYNAMIC_STATE_LINE_RASTERIZATION_MODE_EXT:
            vk.cmdSetLineRasterizationModeEXT(cmdBuffer, state->lineRasterizationMode);
            break;
        case vk::VK_DYNAMIC_STATE_LINE_STIPPLE_ENABLE_EXT:
            vk.cmdSetLineStippleEnableEXT(cmdBuffer, state->stippledLineEnable);
            break;
        case vk::VK_DYNAMIC_STATE_LINE_STIPPLE_EXT:
            if (stippledLineEnabled)
                vk.cmdSetLineStippleKHR(cmdBuffer, state->lineStippleFactor, state->lineStipplePattern);
            break;
        case vk::VK_DYNAMIC_STATE_LOGIC_OP_ENABLE_EXT:
            if (rasterizerDiscardDisabled)
                vk.cmdSetLogicOpEnableEXT(cmdBuffer, state->logicOpEnable);
            break;
        case vk::VK_DYNAMIC_STATE_POLYGON_MODE_EXT:
            vk.cmdSetPolygonModeEXT(cmdBuffer, state->polygonMode);
            break;
        case vk::VK_DYNAMIC_STATE_PROVOKING_VERTEX_MODE_EXT:
            vk.cmdSetProvokingVertexModeEXT(cmdBuffer, state->provokingVertexMode);
            break;
        case vk::VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT:
            vk.cmdSetRasterizationSamplesEXT(cmdBuffer, state->rasterizationSamples);
            break;
        case vk::VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR:
            vk.cmdSetFragmentShadingRateKHR(cmdBuffer, &state->fragmentShadingRateSize, state->combinerOps);
            break;
        case vk::VK_DYNAMIC_STATE_RASTERIZATION_STREAM_EXT:
            vk.cmdSetRasterizationStreamEXT(cmdBuffer, state->rasterizationStream);
            break;
        case vk::VK_DYNAMIC_STATE_REPRESENTATIVE_FRAGMENT_TEST_ENABLE_NV:
            vk.cmdSetRepresentativeFragmentTestEnableNV(cmdBuffer, state->representativeFragmentTestEnable);
            break;
        case vk::VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_ENABLE_EXT:
            vk.cmdSetSampleLocationsEnableEXT(cmdBuffer, state->sampleLocationsEnable);
            break;
        case vk::VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_EXT:
            if (sampleLocationsEnabled)
                vk.cmdSetSampleLocationsEXT(cmdBuffer, &state->sampleLocationsInfo);
            break;
        case vk::VK_DYNAMIC_STATE_SAMPLE_MASK_EXT:
            if (!state->sampleMasks.empty())
                vk.cmdSetSampleMaskEXT(cmdBuffer, state->rasterizationSamples, state->sampleMasks.data());
            break;
        case vk::VK_DYNAMIC_STATE_SHADING_RATE_IMAGE_ENABLE_NV:
            vk.cmdSetShadingRateImageEnableNV(cmdBuffer, state->shadingRateImageEnable);
            break;
        case vk::VK_DYNAMIC_STATE_TESSELLATION_DOMAIN_ORIGIN_EXT:
            if (tese)
                vk.cmdSetTessellationDomainOriginEXT(cmdBuffer, state->domainOrigin);
            break;
        case vk::VK_DYNAMIC_STATE_VIEWPORT_SWIZZLE_NV:
            if (!state->viewportSwizzles.empty())
            {
                vk.cmdSetViewportSwizzleNV(cmdBuffer, 0, (uint32_t)state->viewportSwizzles.size(),
                                           state->viewportSwizzles.data());
            }
            else
            {
                std::vector<vk::VkViewportSwizzleNV> idSwizzles(4u);
                for (auto &swizzle : idSwizzles)
                {
                    swizzle = {
                        vk::VK_VIEWPORT_COORDINATE_SWIZZLE_POSITIVE_X_NV,
                        vk::VK_VIEWPORT_COORDINATE_SWIZZLE_POSITIVE_Y_NV,
                        vk::VK_VIEWPORT_COORDINATE_SWIZZLE_POSITIVE_Z_NV,
                        vk::VK_VIEWPORT_COORDINATE_SWIZZLE_POSITIVE_W_NV,
                    };
                }
                vk.cmdSetViewportSwizzleNV(cmdBuffer, 0u, (uint32_t)idSwizzles.size(), idSwizzles.data());
            }
            break;
        case vk::VK_DYNAMIC_STATE_VIEWPORT_W_SCALING_ENABLE_NV:
            vk.cmdSetViewportWScalingEnableNV(cmdBuffer, state->viewportWScalingEnable);
            break;
        case vk::VK_DYNAMIC_STATE_VIEWPORT_W_SCALING_NV:
            if (state->viewportWScalingCount > 0)
                vk.cmdSetViewportWScalingNV(cmdBuffer, 0, state->viewportWScalingCount,
                                            state->viewportWScalings.data());
            break;
        case vk::VK_DYNAMIC_STATE_VERTEX_INPUT_EXT:
            if (!meshOrTask)
                vk.cmdSetVertexInputEXT(cmdBuffer, (uint32_t)state->bindings.size(), state->bindings.data(),
                                        (uint32_t)state->attributes.size(), state->attributes.data());
            break;
        case vk::VK_DYNAMIC_STATE_VIEWPORT_COARSE_SAMPLE_ORDER_NV:
            vk.cmdSetCoarseSampleOrderNV(cmdBuffer, state->coarseSampleOrderType, state->coarseCustomSampleOrderCount,
                                         state->coarseCustomSampleOrders.data());
            break;
        case vk::VK_DYNAMIC_STATE_VIEWPORT_SHADING_RATE_PALETTE_NV:
            if (state->shadingRatePaletteCount > 0)
                vk.cmdSetViewportShadingRatePaletteNV(cmdBuffer, 0, state->shadingRatePaletteCount,
                                                      state->shadingRatePalettes.data());
            break;
        case vk::VK_DYNAMIC_STATE_EXCLUSIVE_SCISSOR_ENABLE_NV:
        {
            if (state->exclusiveScissorCount > 0)
            {
                std::vector<VkBool32> exclusiveScissorEnable(state->exclusiveScissorCount, VK_TRUE);
                vk.cmdSetExclusiveScissorEnableNV(cmdBuffer, 0u, state->exclusiveScissorCount,
                                                  exclusiveScissorEnable.data());
            }
            else
            {
                VkBool32 enable = VK_FALSE;
                vk.cmdSetExclusiveScissorEnableNV(cmdBuffer, 0u, 1u, &enable);
            }
            break;
        }
        case vk::VK_DYNAMIC_STATE_EXCLUSIVE_SCISSOR_NV:
            if (state->exclusiveScissorCount > 0)
                vk.cmdSetExclusiveScissorNV(cmdBuffer, 0u, state->exclusiveScissorCount,
                                            state->exclussiveScissors.data());
            break;
        case vk::VK_DYNAMIC_STATE_DISCARD_RECTANGLE_ENABLE_EXT:
            vk.cmdSetDiscardRectangleEnableEXT(cmdBuffer, state->discardRectangleEnable);
            break;
        case vk::VK_DYNAMIC_STATE_DISCARD_RECTANGLE_EXT:
            if (discardRectangle)
                vk.cmdSetDiscardRectangleEXT(cmdBuffer, 0, (uint32_t)state->discardRectangles.size(),
                                             state->discardRectangles.data());
            break;
        case vk::VK_DYNAMIC_STATE_DISCARD_RECTANGLE_MODE_EXT:
            if (discardRectangle)
                vk.cmdSetDiscardRectangleModeEXT(cmdBuffer, state->discardRectangleMode);
            break;
        case vk::VK_DYNAMIC_STATE_ATTACHMENT_FEEDBACK_LOOP_ENABLE_EXT:
            vk.cmdSetAttachmentFeedbackLoopEnableEXT(cmdBuffer, state->attachmentFeedbackLoopEnable);
            break;
        default:
            break;
        }
    }
#endif // CTS_USES_VULKANSC
}

void GraphicsPipelineWrapper::bind(vk::VkCommandBuffer cmdBuffer) const
{
    const auto &vk = m_internalData->vk;
    if (!isConstructionTypeShaderObject(m_internalData->pipelineConstructionType))
    {
        vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, getPipeline());
    }
    else
    {
#ifndef CTS_USES_VULKANSC
        const VkShaderStageFlagBits vertStage = vk::VK_SHADER_STAGE_VERTEX_BIT;
        VkShaderEXT vertShader                = m_internalData->vertexShader.getShader();
        vk.cmdBindShadersEXT(cmdBuffer, 1, &vertStage, &vertShader);
        const VkShaderStageFlagBits tescStage = vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        VkShaderEXT tescShader                = m_internalData->tessellationControlShader.getShader();
        vk.cmdBindShadersEXT(cmdBuffer, 1, &tescStage, &tescShader);
        const VkShaderStageFlagBits teseStage = vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        VkShaderEXT teseShader                = m_internalData->tessellationEvaluationShader.getShader();
        vk.cmdBindShadersEXT(cmdBuffer, 1, &teseStage, &teseShader);
        const VkShaderStageFlagBits geomStage = vk::VK_SHADER_STAGE_GEOMETRY_BIT;
        VkShaderEXT geomShader                = m_internalData->geometryShader.getShader();
        vk.cmdBindShadersEXT(cmdBuffer, 1, &geomStage, &geomShader);
        const VkShaderStageFlagBits fragStage = vk::VK_SHADER_STAGE_FRAGMENT_BIT;
        VkShaderEXT fragShader                = m_internalData->fragmentShader.getShader();
        vk.cmdBindShadersEXT(cmdBuffer, 1, &fragStage, &fragShader);
        if (m_internalData->meshShaderFeature)
        {
            const VkShaderStageFlagBits meshStage = vk::VK_SHADER_STAGE_MESH_BIT_EXT;
            VkShaderEXT meshShader                = m_internalData->meshShader.getShader();
            vk.cmdBindShadersEXT(cmdBuffer, 1, &meshStage, &meshShader);
        }
        if (m_internalData->taskShaderFeature)
        {
            const VkShaderStageFlagBits taskStage = vk::VK_SHADER_STAGE_TASK_BIT_EXT;
            VkShaderEXT taskShader                = m_internalData->taskShader.getShader();
            vk.cmdBindShadersEXT(cmdBuffer, 1, &taskStage, &taskShader);
        }
        // Set all dynamic state that would otherwise have been set with the pipeline
        setShaderObjectDynamicStates(cmdBuffer);
#endif
    }
}

bool GraphicsPipelineWrapper::wasBuild() const
{
    return !!m_pipelineFinal.get();
}

bool GraphicsPipelineWrapper::wasPipelineOrShaderObjectBuild(void) const
{
    if (!!m_pipelineFinal.get())
        return true;

#ifndef CTS_USES_VULKANSC
    if (!!m_internalData->vertexShader.getShader() || !!m_internalData->tessellationControlShader.getShader() ||
        !!m_internalData->tessellationEvaluationShader.getShader() || !!m_internalData->geometryShader.getShader() ||
        !!m_internalData->fragmentShader.getShader() || !!m_internalData->taskShader.getShader() ||
        !!m_internalData->meshShader.getShader())
        return true;
#endif

    return false;
}

VkPipeline GraphicsPipelineWrapper::getPipeline(void) const
{
    DE_ASSERT(m_pipelineFinal.get() != VK_NULL_HANDLE);
    return m_pipelineFinal.get();
}

vk::VkPipeline GraphicsPipelineWrapper::getPartialPipeline(uint32_t part) const
{
    DE_ASSERT(part < 4u);
    DE_ASSERT(m_pipelineParts[part].get() != VK_NULL_HANDLE);
    return m_pipelineParts[part].get();
}
const VkGraphicsPipelineCreateInfo &GraphicsPipelineWrapper::getPipelineCreateInfo(void) const
{
    DE_ASSERT(m_internalData);
    return m_internalData->monolithicPipelineCreateInfo;
}
const VkGraphicsPipelineCreateInfo &GraphicsPipelineWrapper::getPartialPipelineCreateInfo(uint32_t part) const
{
    DE_ASSERT(part < 4u);
    DE_ASSERT(m_internalData);
    return m_internalData->pipelinePartCreateInfo[part];
}

#ifndef CTS_USES_VULKANSC
VkShaderEXT GraphicsPipelineWrapper::getShader(VkShaderStageFlagBits stage) const
{
    switch (stage)
    {
    case VK_SHADER_STAGE_VERTEX_BIT:
        return m_internalData->vertexShader.getShader();
    case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
        return m_internalData->tessellationControlShader.getShader();
    case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
        return m_internalData->tessellationEvaluationShader.getShader();
    case VK_SHADER_STAGE_GEOMETRY_BIT:
        return m_internalData->geometryShader.getShader();
    case VK_SHADER_STAGE_FRAGMENT_BIT:
        return m_internalData->fragmentShader.getShader();
    case VK_SHADER_STAGE_MESH_BIT_EXT:
        return m_internalData->meshShader.getShader();
    case VK_SHADER_STAGE_TASK_BIT_EXT:
        return m_internalData->taskShader.getShader();
    default:
        break;
    }

    DE_ASSERT(false);
    return VK_NULL_HANDLE;
}
#endif // CTS_USES_VULKANSC

void GraphicsPipelineWrapper::destroyPipeline(void)
{
    DE_ASSERT(m_pipelineFinal.get() != VK_NULL_HANDLE);

    m_pipelineFinal = Move<VkPipeline>();
}

std::vector<VkDynamicState> getShaderObjectDynamicStatesFromExtensions(const std::vector<std::string> &extensions)
{
    std::vector<VkDynamicState> dynamicStates;

#ifndef CTS_USES_VULKANSC
    std::set<std::string> extensionSet(begin(extensions), end(extensions));

    // Add dynamic states that are required for each enabled extension
    if (extensionSet.count("VK_EXT_transform_feedback") > 0u)
        dynamicStates.push_back(VK_DYNAMIC_STATE_RASTERIZATION_STREAM_EXT);
    if (extensionSet.count("VK_EXT_blend_operation_advanced") > 0u)
        dynamicStates.push_back(VK_DYNAMIC_STATE_COLOR_BLEND_ADVANCED_EXT);
    if (extensionSet.count("VK_EXT_conservative_rasterization") > 0u)
        dynamicStates.push_back(VK_DYNAMIC_STATE_CONSERVATIVE_RASTERIZATION_MODE_EXT);
    if (extensionSet.count("VK_NV_framebuffer_mixed_samples") > 0u)
        dynamicStates.push_back(VK_DYNAMIC_STATE_COVERAGE_MODULATION_MODE_NV);
    if (extensionSet.count("VK_NV_framebuffer_mixed_samples") > 0u)
        dynamicStates.push_back(VK_DYNAMIC_STATE_COVERAGE_MODULATION_TABLE_ENABLE_NV);
    if (extensionSet.count("VK_NV_framebuffer_mixed_samples") > 0u)
        dynamicStates.push_back(VK_DYNAMIC_STATE_COVERAGE_MODULATION_TABLE_NV);
    if (extensionSet.count("VK_NV_coverage_reduction_mode") > 0u)
        dynamicStates.push_back(VK_DYNAMIC_STATE_COVERAGE_REDUCTION_MODE_NV);
    if (extensionSet.count("VK_NV_fragment_coverage_to_color") > 0u)
        dynamicStates.push_back(VK_DYNAMIC_STATE_COVERAGE_TO_COLOR_ENABLE_NV);
    if (extensionSet.count("VK_NV_fragment_coverage_to_color") > 0u)
        dynamicStates.push_back(VK_DYNAMIC_STATE_COVERAGE_TO_COLOR_LOCATION_NV);
    if (extensionSet.count("VK_EXT_depth_clip_enable") > 0u)
        dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_CLIP_ENABLE_EXT);
    if (extensionSet.count("VK_EXT_depth_clip_control") > 0u)
        dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_CLIP_NEGATIVE_ONE_TO_ONE_EXT);
    if (extensionSet.count("VK_EXT_color_write_enable") > 0u)
        dynamicStates.push_back(VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT);
    if (extensionSet.count("VK_EXT_conservative_rasterization") > 0u)
        dynamicStates.push_back(VK_DYNAMIC_STATE_EXTRA_PRIMITIVE_OVERESTIMATION_SIZE_EXT);
    if (extensionSet.count("VK_KHR_line_rasterization") > 0u || extensionSet.count("VK_EXT_line_rasterization") > 0u)
        dynamicStates.push_back(VK_DYNAMIC_STATE_LINE_RASTERIZATION_MODE_EXT);
    if (extensionSet.count("VK_KHR_line_rasterization") > 0u || extensionSet.count("VK_EXT_line_rasterization") > 0u)
        dynamicStates.push_back(VK_DYNAMIC_STATE_LINE_STIPPLE_ENABLE_EXT);
    if (extensionSet.count("VK_KHR_line_rasterization") > 0u || extensionSet.count("VK_EXT_line_rasterization") > 0u)
        dynamicStates.push_back(VK_DYNAMIC_STATE_LINE_STIPPLE_KHR);
    if (extensionSet.count("VK_EXT_provoking_vertex") > 0u)
        dynamicStates.push_back(VK_DYNAMIC_STATE_PROVOKING_VERTEX_MODE_EXT);
    if (extensionSet.count("VK_KHR_fragment_shading_rate") > 0u)
        dynamicStates.push_back(VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR);
    if (extensionSet.count("VK_NV_representative_fragment_test") > 0u)
        dynamicStates.push_back(VK_DYNAMIC_STATE_REPRESENTATIVE_FRAGMENT_TEST_ENABLE_NV);
    if (extensionSet.count("VK_EXT_sample_locations") > 0u)
        dynamicStates.push_back(VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_ENABLE_EXT);
    if (extensionSet.count("VK_EXT_sample_locations") > 0u)
        dynamicStates.push_back(VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_EXT);
    // Not working with VK_KHR_fragment_shading_rate
    /*if (extensionSet.count("VK_NV_shading_rate_image") > 0u)
        dynamicStates.push_back(VK_DYNAMIC_STATE_SHADING_RATE_IMAGE_ENABLE_NV);
    if (extensionSet.count("VK_NV_shading_rate_image") > 0u)
        dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT_COARSE_SAMPLE_ORDER_NV);
    if (extensionSet.count("VK_NV_shading_rate_image") > 0u)
        dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT_SHADING_RATE_PALETTE_NV);*/
    if (extensionSet.count("VK_NV_viewport_swizzle") > 0u)
        dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT_SWIZZLE_NV);
    if (extensionSet.count("VK_NV_clip_space_w_scaling") > 0u)
        dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT_W_SCALING_ENABLE_NV);
    if (extensionSet.count("VK_NV_clip_space_w_scaling") > 0u)
        dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT_W_SCALING_NV);
    if (extensionSet.count("VK_NV_scissor_exclusive") > 0u)
        dynamicStates.push_back(VK_DYNAMIC_STATE_EXCLUSIVE_SCISSOR_ENABLE_NV);
    if (extensionSet.count("VK_NV_scissor_exclusive") > 0u)
        dynamicStates.push_back(VK_DYNAMIC_STATE_EXCLUSIVE_SCISSOR_NV);
    if (extensionSet.count("VK_EXT_discard_rectangles") > 0u)
        dynamicStates.push_back(VK_DYNAMIC_STATE_DISCARD_RECTANGLE_ENABLE_EXT);
    if (extensionSet.count("VK_EXT_discard_rectangles") > 0u)
        dynamicStates.push_back(VK_DYNAMIC_STATE_DISCARD_RECTANGLE_EXT);
    if (extensionSet.count("VK_EXT_discard_rectangles") > 0u)
        dynamicStates.push_back(VK_DYNAMIC_STATE_DISCARD_RECTANGLE_MODE_EXT);
    if (extensionSet.count("VK_EXT_attachment_feedback_loop_dynamic_state") > 0u)
        dynamicStates.push_back(VK_DYNAMIC_STATE_ATTACHMENT_FEEDBACK_LOOP_ENABLE_EXT);
#else
    DE_UNREF(extensions);
#endif

    return dynamicStates;
}

} // namespace vk
