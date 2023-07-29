/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
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
          VK_EXT_graphics_pipeline_library for pipeline construction
 *//*--------------------------------------------------------------------*/

#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "deSharedPtr.hpp"
#include "tcuVector.hpp"
#include "tcuVectorType.hpp"
#include "vkPipelineConstructionUtil.hpp"

#include <memory>

namespace vk
{

enum PipelineSetupState
{
    PSS_NONE                      = 0x00000000,
    PSS_VERTEX_INPUT_INTERFACE    = 0x00000001,
    PSS_PRE_RASTERIZATION_SHADERS = 0x00000002,
    PSS_FRAGMENT_SHADER           = 0x00000004,
    PSS_FRAGMENT_OUTPUT_INTERFACE = 0x00000008,
};

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
    DE_NULL,                                                   // const void*                                    pNext
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
    DE_NULL,                                                    // const void*                                    pNext
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
    DE_NULL,                                                  // const void*                                    pNext
    0u,                                                       // VkPipelineMultisampleStateCreateFlags        flags
    VK_SAMPLE_COUNT_1_BIT, // VkSampleCountFlagBits                        rasterizationSamples
    VK_FALSE,              // VkBool32                                        sampleShadingEnable
    1.0f,                  // float                                        minSampleShading
    DE_NULL,               // const VkSampleMask*                            pSampleMask
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
    DE_NULL,                                                  // const void*                                    pNext
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
        DE_NULL,                                                     // void* pNext;
        flags,                                                       // VkGraphicsPipelineLibraryFlagsEXT flags;
    };
}
#endif // CTS_USES_VULKANSC

Move<VkPipeline> makeGraphicsPipeline(const DeviceInterface &vk, VkDevice device, VkPipelineCache pipelineCache,
                                      const VkGraphicsPipelineCreateInfo *pCreateInfo,
                                      const VkAllocationCallbacks *pAllocator = nullptr)
{
    VkPipeline object  = 0;
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

void checkPipelineLibraryRequirements(const InstanceInterface &vki, VkPhysicalDevice physicalDevice,
                                      PipelineConstructionType pipelineConstructionType)
{
    if (pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
        return;

    const auto supportedExtensions = enumerateDeviceExtensionProperties(vki, physicalDevice, DE_NULL);
    if (!isExtensionStructSupported(supportedExtensions, RequiredExtension("VK_EXT_graphics_pipeline_library")))
        TCU_THROW(NotSupportedError, "VK_EXT_graphics_pipeline_library not supported");
}

void addToChain(void **structThatStartsChain, void *structToAddAtTheEnd)
{
    DE_ASSERT(structThatStartsChain);

    if (structToAddAtTheEnd == DE_NULL)
        return;

    // just cast to randomly picked structure that has non const pNext pointer
    auto *structToAddAtTheEndCasted = reinterpret_cast<VkPhysicalDevicePointClippingProperties *>(structToAddAtTheEnd);

    // make sure that pNext pointer of structure that is added to chain is empty;
    // we are construting chains on our own and there are cases that use same
    // structure for multiple instances of GraphicsPipelineWrapper
    structToAddAtTheEndCasted->pNext = DE_NULL;

    uint32_t safetyCouter = 10u;
    void **structInChain  = structThatStartsChain;

    do
    {
        // check if this is free spot
        if (*structInChain == DE_NULL)
        {
            // attach new structure at the end
            *structInChain = structToAddAtTheEndCasted;
            return;
        }

        // just cast to randomly picked structure that has non const pNext pointer
        auto *gpl = reinterpret_cast<VkPhysicalDevicePointClippingProperties *>(*structInChain);

        // move structure pointer one position down the pNext chain
        structInChain = &gpl->pNext;
    } while (--safetyCouter);

    // probably safetyCouter is to small
    DE_ASSERT(false);
}

namespace
{
using PipelineShaderStageModuleIdPtr = std::unique_ptr<PipelineShaderStageModuleIdentifierCreateInfoWrapper>;
}

// Structure storing *CreateInfo structures that do not need to exist in memory after pipeline was constructed.
struct GraphicsPipelineWrapper::InternalData
{
    const DeviceInterface &vk;
    VkDevice device;
    const PipelineConstructionType pipelineConstructionType;
    const VkPipelineCreateFlags pipelineFlags;

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
    const VkPipelineDynamicStateCreateInfo *pDynamicState;

    bool useViewportState;
    bool useDefaultRasterizationState;
    bool useDefaultDepthStencilState;
    bool useDefaultColorBlendState;
    bool useDefaultMultisampleState;
    bool failOnCompileWhenLinking;

    VkGraphicsPipelineCreateInfo monolithicPipelineCreateInfo;

    // initialize with most common values
    InternalData(const DeviceInterface& vkd, VkDevice vkDevice, const PipelineConstructionType constructionType, const VkPipelineCreateFlags pipelineCreateFlags)
        : vk                        (vkd)
        , device                    (vkDevice)
        , pipelineConstructionType    (constructionType)
        , pipelineFlags                (pipelineCreateFlags)
        , setupState                (PSS_NONE)
        , inputAssemblyState
        {
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, // VkStructureType                                sType
            DE_NULL, // const void*                                    pNext
            0u, // VkPipelineInputAssemblyStateCreateFlags        flags
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // VkPrimitiveTopology                            topology
            VK_FALSE                                                        // VkBool32                                        primitiveRestartEnable
        }
        , defaultRasterizationState
        {
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, // VkStructureType                                sType
            DE_NULL, // const void*                                    pNext
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
            DE_NULL, // const void*                                    pNext
            (VkPipelineViewportStateCreateFlags)0, // VkPipelineViewportStateCreateFlags            flags
            1u, // uint32_t                                        viewportCount
            DE_NULL, // const VkViewport*                            pViewports
            1u, // uint32_t                                        scissorCount
            DE_NULL                                                            // const VkRect2D*                                pScissors
        }
        , tessellationState
        {
            VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO, // VkStructureType                                sType
            DE_NULL, // const void*                                    pNext
            0u, // VkPipelineTessellationStateCreateFlags        flags
            3u                                                                // uint32_t                                        patchControlPoints
        }
        , pFragmentShadingRateState        (DE_NULL)
        , pDynamicState                    (DE_NULL)
        , useViewportState                (true)
        , useDefaultRasterizationState    (false)
        , useDefaultDepthStencilState    (false)
        , useDefaultColorBlendState        (false)
        , useDefaultMultisampleState    (false)
        , failOnCompileWhenLinking        (false)
    {
        monolithicPipelineCreateInfo = initVulkanStructure();
    }
};

GraphicsPipelineWrapper::GraphicsPipelineWrapper(const DeviceInterface &vk, VkDevice device,
                                                 const PipelineConstructionType pipelineConstructionType,
                                                 const VkPipelineCreateFlags flags)
    : m_internalData(new InternalData(vk, device, pipelineConstructionType, flags))
{
}

GraphicsPipelineWrapper::GraphicsPipelineWrapper(GraphicsPipelineWrapper &&pw) noexcept
    : m_pipelineFinal(pw.m_pipelineFinal)
    , m_internalData(pw.m_internalData)
{
    std::move(pw.m_pipelineParts, pw.m_pipelineParts + 4, m_pipelineParts);
}

GraphicsPipelineWrapper &GraphicsPipelineWrapper::setMonolithicPipelineLayout(const VkPipelineLayout layout)
{
    // make sure pipeline was not already built
    DE_ASSERT(m_pipelineFinal.get() == DE_NULL);

    m_internalData->monolithicPipelineCreateInfo.layout = layout;

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

GraphicsPipelineWrapper &GraphicsPipelineWrapper::setDepthClipControl(
    PipelineViewportDepthClipControlCreateInfoWrapper &depthClipControlCreateInfo)
{
    // ViewportState is used in pre-rasterization shader state, make sure pre-rasterization state was not setup yet
    DE_ASSERT(m_internalData && (m_internalData->setupState < PSS_PRE_RASTERIZATION_SHADERS));

    m_internalData->viewportState.pNext = depthClipControlCreateInfo.ptr;

    return *this;
}

GraphicsPipelineWrapper &GraphicsPipelineWrapper::disableViewportState()
{
    // ViewportState is used in pre-rasterization shader state, make sure pre-rasterization state was not setup yet
    DE_ASSERT(m_internalData && (m_internalData->setupState < PSS_PRE_RASTERIZATION_SHADERS));

    m_internalData->useViewportState = false;

    return *this;
}

GraphicsPipelineWrapper &GraphicsPipelineWrapper::setupVertexInputStete(
    const VkPipelineVertexInputStateCreateInfo *vertexInputState,
    const VkPipelineInputAssemblyStateCreateInfo *inputAssemblyState, const VkPipelineCache partPipelineCache,
    PipelineCreationFeedbackCreateInfoWrapper partCreationFeedback)
{
    // make sure pipeline was not already build
    DE_ASSERT(m_pipelineFinal.get() == DE_NULL);

    // make sure states are set in order - no need to complicate logic to support out of order specification - this state needs to be set first
    DE_ASSERT(m_internalData && (m_internalData->setupState == PSS_NONE));

    // Unreference variables that are not used in Vulkan SC. No need to put this in ifdef.
    DE_UNREF(partPipelineCache);
    DE_UNREF(partCreationFeedback);

    m_internalData->setupState = PSS_VERTEX_INPUT_INTERFACE;

    const auto pVertexInputState   = vertexInputState ? vertexInputState : &defaultVertexInputState;
    const auto pInputAssemblyState = inputAssemblyState ? inputAssemblyState : &m_internalData->inputAssemblyState;

    if (m_internalData->pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
    {
        m_internalData->monolithicPipelineCreateInfo.pVertexInputState   = pVertexInputState;
        m_internalData->monolithicPipelineCreateInfo.pInputAssemblyState = pInputAssemblyState;
    }

#ifndef CTS_USES_VULKANSC
    // note we could just use else to if statement above but sinc
    // this section is cut out for Vulkan SC its cleaner with separate if
    if (m_internalData->pipelineConstructionType != PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
    {
        auto libraryCreateInfo =
            makeGraphicsPipelineLibraryCreateInfo(VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT);
        void *firstStructInChain = reinterpret_cast<void *>(&libraryCreateInfo);
        addToChain(&firstStructInChain, partCreationFeedback.ptr);

        VkGraphicsPipelineCreateInfo pipelinePartCreateInfo = initVulkanStructure();
        pipelinePartCreateInfo.pNext                        = firstStructInChain;
        pipelinePartCreateInfo.flags               = m_internalData->pipelineFlags | VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
        pipelinePartCreateInfo.pVertexInputState   = pVertexInputState;
        pipelinePartCreateInfo.pInputAssemblyState = pInputAssemblyState;
        pipelinePartCreateInfo.pDynamicState       = m_internalData->pDynamicState;

        if (m_internalData->pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY)
            pipelinePartCreateInfo.flags |= VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT;

        m_pipelineParts[0] = createGraphicsPipeline(m_internalData->vk, m_internalData->device, partPipelineCache,
                                                    &pipelinePartCreateInfo);
    }
#endif // CTS_USES_VULKANSC

    return *this;
}

GraphicsPipelineWrapper &GraphicsPipelineWrapper::setupPreRasterizationShaderState(
    const std::vector<VkViewport> &viewports, const std::vector<VkRect2D> &scissors, const VkPipelineLayout layout,
    const VkRenderPass renderPass, const uint32_t subpass, const VkShaderModule vertexShaderModule,
    const VkPipelineRasterizationStateCreateInfo *rasterizationState,
    const VkShaderModule tessellationControlShaderModule, const VkShaderModule tessellationEvalShaderModule,
    const VkShaderModule geometryShaderModule, const VkSpecializationInfo *specializationInfo,
    PipelineRenderingCreateInfoWrapper rendering, const VkPipelineCache partPipelineCache,
    PipelineCreationFeedbackCreateInfoWrapper partCreationFeedback)
{
    return setupPreRasterizationShaderState2(viewports, scissors, layout, renderPass, subpass, vertexShaderModule,
                                             rasterizationState, tessellationControlShaderModule,
                                             tessellationEvalShaderModule, geometryShaderModule,
                                             // Reuse the same specialization info for all stages.
                                             specializationInfo, specializationInfo, specializationInfo,
                                             specializationInfo, rendering, partPipelineCache, partCreationFeedback);
}

GraphicsPipelineWrapper &GraphicsPipelineWrapper::setupPreRasterizationShaderState2(
    const std::vector<VkViewport> &viewports, const std::vector<VkRect2D> &scissors, const VkPipelineLayout layout,
    const VkRenderPass renderPass, const uint32_t subpass, const VkShaderModule vertexShaderModule,
    const VkPipelineRasterizationStateCreateInfo *rasterizationState,
    const VkShaderModule tessellationControlShaderModule, const VkShaderModule tessellationEvalShaderModule,
    const VkShaderModule geometryShaderModule, const VkSpecializationInfo *vertSpecializationInfo,
    const VkSpecializationInfo *tescSpecializationInfo, const VkSpecializationInfo *teseSpecializationInfo,
    const VkSpecializationInfo *geomSpecializationInfo, PipelineRenderingCreateInfoWrapper rendering,
    const VkPipelineCache partPipelineCache, PipelineCreationFeedbackCreateInfoWrapper partCreationFeedback)
{
    return setupPreRasterizationShaderState3(
        viewports, scissors, layout, renderPass, subpass, vertexShaderModule,
        PipelineShaderStageModuleIdentifierCreateInfoWrapper(), rasterizationState, tessellationControlShaderModule,
        PipelineShaderStageModuleIdentifierCreateInfoWrapper(), tessellationEvalShaderModule,
        PipelineShaderStageModuleIdentifierCreateInfoWrapper(), geometryShaderModule,
        PipelineShaderStageModuleIdentifierCreateInfoWrapper(), vertSpecializationInfo, tescSpecializationInfo,
        teseSpecializationInfo, geomSpecializationInfo, rendering, partPipelineCache, partCreationFeedback);
}

GraphicsPipelineWrapper &GraphicsPipelineWrapper::setupPreRasterizationShaderState3(
    const std::vector<VkViewport> &viewports, const std::vector<VkRect2D> &scissors, const VkPipelineLayout layout,
    const VkRenderPass renderPass, const uint32_t subpass, const VkShaderModule vertexShaderModule,
    PipelineShaderStageModuleIdentifierCreateInfoWrapper vertShaderModuleId,
    const VkPipelineRasterizationStateCreateInfo *rasterizationState,
    const VkShaderModule tessellationControlShaderModule,
    PipelineShaderStageModuleIdentifierCreateInfoWrapper tescShaderModuleId,
    const VkShaderModule tessellationEvalShaderModule,
    PipelineShaderStageModuleIdentifierCreateInfoWrapper teseShaderModuleId, const VkShaderModule geometryShaderModule,
    PipelineShaderStageModuleIdentifierCreateInfoWrapper geomShaderModuleId,
    const VkSpecializationInfo *vertSpecializationInfo, const VkSpecializationInfo *tescSpecializationInfo,
    const VkSpecializationInfo *teseSpecializationInfo, const VkSpecializationInfo *geomSpecializationInfo,
    PipelineRenderingCreateInfoWrapper rendering, const VkPipelineCache partPipelineCache,
    PipelineCreationFeedbackCreateInfoWrapper partCreationFeedback)
{
    // make sure pipeline was not already build
    DE_ASSERT(m_pipelineFinal.get() == DE_NULL);

    // make sure states are set in order - no need to complicate logic to support out of order specification - this state needs to be set second
    DE_ASSERT(m_internalData && (m_internalData->setupState == PSS_VERTEX_INPUT_INTERFACE));

    // Unreference variables that are not used in Vulkan SC. No need to put this in ifdef.
    DE_UNREF(partPipelineCache);
    DE_UNREF(partCreationFeedback);
    DE_UNREF(vertShaderModuleId);
    DE_UNREF(tescShaderModuleId);
    DE_UNREF(teseShaderModuleId);
    DE_UNREF(geomShaderModuleId);

    m_internalData->setupState |= PSS_PRE_RASTERIZATION_SHADERS;
    m_internalData->pRenderingState.ptr = rendering.ptr;

    const bool hasTesc = (tessellationControlShaderModule != DE_NULL || tescShaderModuleId.ptr);
    const bool hasTese = (tessellationEvalShaderModule != DE_NULL || teseShaderModuleId.ptr);
    const bool hasGeom = (geometryShaderModule != DE_NULL || geomShaderModuleId.ptr);

    const auto pRasterizationState =
        rasterizationState ?
            rasterizationState :
            (m_internalData->useDefaultRasterizationState ? &m_internalData->defaultRasterizationState : DE_NULL);
    const auto pTessellationState = (hasTesc || hasTese) ? &m_internalData->tessellationState : DE_NULL;
    const auto pViewportState     = m_internalData->useViewportState ? &m_internalData->viewportState : DE_NULL;

    VkPipelineCreateFlags shaderModuleIdFlags = 0u;

    // reserve space for all stages including fragment - this is needed when we create monolithic pipeline
    m_internalData->pipelineShaderStages = std::vector<VkPipelineShaderStageCreateInfo>(
        2u + hasTesc + hasTese + hasGeom,
        {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType                        sType
            DE_NULL,                                             // const void*                            pNext
            0u,                                                  // VkPipelineShaderStageCreateFlags        flags
            VK_SHADER_STAGE_VERTEX_BIT,                          // VkShaderStageFlagBits                stage
            vertexShaderModule,                                  // VkShaderModule                        module
            "main",                                              // const char*                            pName
            vertSpecializationInfo // const VkSpecializationInfo*            pSpecializationInfo
        });

#ifndef CTS_USES_VULKANSC
    if (vertShaderModuleId.ptr)
    {
        m_internalData->pipelineShaderIdentifiers.emplace_back(
            new PipelineShaderStageModuleIdentifierCreateInfoWrapper(vertShaderModuleId.ptr));
        m_internalData->pipelineShaderStages[0].pNext = m_internalData->pipelineShaderIdentifiers.back().get()->ptr;

        if (vertexShaderModule == DE_NULL)
            shaderModuleIdFlags |= VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT;
    }
#endif // CTS_USES_VULKANSC

    std::vector<VkPipelineShaderStageCreateInfo>::iterator currStage = m_internalData->pipelineShaderStages.begin() + 1;

    if (hasTesc)
    {
        currStage->stage               = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        currStage->module              = tessellationControlShaderModule;
        currStage->pSpecializationInfo = tescSpecializationInfo;

#ifndef CTS_USES_VULKANSC
        if (tescShaderModuleId.ptr)
        {
            m_internalData->pipelineShaderIdentifiers.emplace_back(
                new PipelineShaderStageModuleIdentifierCreateInfoWrapper(tescShaderModuleId.ptr));
            currStage->pNext = m_internalData->pipelineShaderIdentifiers.back().get()->ptr;

            if (tessellationControlShaderModule == DE_NULL)
                shaderModuleIdFlags |= VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT;
        }
#endif // CTS_USES_VULKANSC

        ++currStage;
    }

    if (hasTese)
    {
        currStage->stage               = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        currStage->module              = tessellationEvalShaderModule;
        currStage->pSpecializationInfo = teseSpecializationInfo;

#ifndef CTS_USES_VULKANSC
        if (teseShaderModuleId.ptr)
        {
            m_internalData->pipelineShaderIdentifiers.emplace_back(
                new PipelineShaderStageModuleIdentifierCreateInfoWrapper(teseShaderModuleId.ptr));
            currStage->pNext = m_internalData->pipelineShaderIdentifiers.back().get()->ptr;

            if (tessellationEvalShaderModule == DE_NULL)
                shaderModuleIdFlags |= VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT;
        }
#endif // CTS_USES_VULKANSC

        ++currStage;
    }

    if (hasGeom)
    {
        currStage->stage               = VK_SHADER_STAGE_GEOMETRY_BIT;
        currStage->module              = geometryShaderModule;
        currStage->pSpecializationInfo = geomSpecializationInfo;

#ifndef CTS_USES_VULKANSC
        if (geomShaderModuleId.ptr)
        {
            m_internalData->pipelineShaderIdentifiers.emplace_back(
                new PipelineShaderStageModuleIdentifierCreateInfoWrapper(geomShaderModuleId.ptr));
            currStage->pNext = m_internalData->pipelineShaderIdentifiers.back().get()->ptr;

            if (geometryShaderModule == DE_NULL)
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

    if (m_internalData->pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
    {
        // make sure we dont overwrite layout specified with setupMonolithicPipelineLayout
        if (m_internalData->monolithicPipelineCreateInfo.layout == 0)
            m_internalData->monolithicPipelineCreateInfo.layout = layout;

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
    if (m_internalData->pipelineConstructionType != PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
    {
        auto libraryCreateInfo =
            makeGraphicsPipelineLibraryCreateInfo(VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT);
        void *firstStructInChain = reinterpret_cast<void *>(&libraryCreateInfo);
        addToChain(&firstStructInChain, m_internalData->pRenderingState.ptr);
        addToChain(&firstStructInChain, partCreationFeedback.ptr);

        VkGraphicsPipelineCreateInfo pipelinePartCreateInfo = initVulkanStructure();
        pipelinePartCreateInfo.pNext                        = firstStructInChain;
        pipelinePartCreateInfo.flags =
            m_internalData->pipelineFlags | VK_PIPELINE_CREATE_LIBRARY_BIT_KHR | shaderModuleIdFlags;
        pipelinePartCreateInfo.layout              = layout;
        pipelinePartCreateInfo.renderPass          = renderPass;
        pipelinePartCreateInfo.subpass             = subpass;
        pipelinePartCreateInfo.pRasterizationState = pRasterizationState;
        pipelinePartCreateInfo.pViewportState      = pViewportState;
        pipelinePartCreateInfo.stageCount          = 1u + hasTesc + hasTese + hasGeom;
        pipelinePartCreateInfo.pStages             = m_internalData->pipelineShaderStages.data();
        pipelinePartCreateInfo.pTessellationState  = pTessellationState;
        pipelinePartCreateInfo.pDynamicState       = m_internalData->pDynamicState;

        if (m_internalData->pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY)
            pipelinePartCreateInfo.flags |= VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT;

        if ((shaderModuleIdFlags & VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT) != 0)
            m_internalData->failOnCompileWhenLinking = true;

        m_pipelineParts[1] = makeGraphicsPipeline(m_internalData->vk, m_internalData->device, partPipelineCache,
                                                  &pipelinePartCreateInfo);
    }
#endif // CTS_USES_VULKANSC

    return *this;
}

GraphicsPipelineWrapper &GraphicsPipelineWrapper::setupFragmentShaderState(
    const VkPipelineLayout layout, const VkRenderPass renderPass, const uint32_t subpass,
    const VkShaderModule fragmentShaderModule, const VkPipelineDepthStencilStateCreateInfo *depthStencilState,
    const VkPipelineMultisampleStateCreateInfo *multisampleState,
    VkPipelineFragmentShadingRateStateCreateInfoKHR *fragmentShadingRateState,
    const VkSpecializationInfo *specializationInfo, const VkPipelineCache partPipelineCache,
    PipelineCreationFeedbackCreateInfoWrapper partCreationFeedback)
{
    return setupFragmentShaderState2(layout, renderPass, subpass, fragmentShaderModule,
                                     PipelineShaderStageModuleIdentifierCreateInfoWrapper(), depthStencilState,
                                     multisampleState, fragmentShadingRateState, specializationInfo, partPipelineCache,
                                     partCreationFeedback);
}

GraphicsPipelineWrapper &GraphicsPipelineWrapper::setupFragmentShaderState2(
    const VkPipelineLayout layout, const VkRenderPass renderPass, const uint32_t subpass,
    const VkShaderModule fragmentShaderModule,
    PipelineShaderStageModuleIdentifierCreateInfoWrapper fragmentShaderModuleId,
    const VkPipelineDepthStencilStateCreateInfo *depthStencilState,
    const VkPipelineMultisampleStateCreateInfo *multisampleState,
    VkPipelineFragmentShadingRateStateCreateInfoKHR *fragmentShadingRateState,
    const VkSpecializationInfo *specializationInfo, const VkPipelineCache partPipelineCache,
    PipelineCreationFeedbackCreateInfoWrapper partCreationFeedback)
{
    // make sure pipeline was not already build
    DE_ASSERT(m_pipelineFinal.get() == DE_NULL);

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

    m_internalData->setupState |= PSS_FRAGMENT_SHADER;
    m_internalData->pFragmentShadingRateState = fragmentShadingRateState;

    const auto pDepthStencilState =
        depthStencilState ? depthStencilState :
                            (m_internalData->useDefaultDepthStencilState ? &defaultDepthStencilState : DE_NULL);
    const auto pMultisampleState =
        multisampleState ? multisampleState :
                           (m_internalData->useDefaultMultisampleState ? &defaultMultisampleState : DE_NULL);
    const bool hasFrag = (fragmentShaderModule != DE_NULL || fragmentShaderModuleId.ptr);

    VkPipelineCreateFlags shaderModuleIdFlags = 0u;

    uint32_t stageIndex = 1;
    if (hasFrag)
    {
        // find free space for fragment shader
        for (; stageIndex < 5; ++stageIndex)
        {
            if (m_internalData->pipelineShaderStages[stageIndex].stage == VK_SHADER_STAGE_VERTEX_BIT)
            {
                m_internalData->pipelineShaderStages[stageIndex].stage               = VK_SHADER_STAGE_FRAGMENT_BIT;
                m_internalData->pipelineShaderStages[stageIndex].module              = fragmentShaderModule;
                m_internalData->pipelineShaderStages[stageIndex].pSpecializationInfo = specializationInfo;
#ifndef CTS_USES_VULKANSC
                if (fragmentShaderModuleId.ptr)
                {
                    m_internalData->pipelineShaderIdentifiers.emplace_back(
                        new PipelineShaderStageModuleIdentifierCreateInfoWrapper(fragmentShaderModuleId.ptr));
                    m_internalData->pipelineShaderStages[stageIndex].pNext =
                        m_internalData->pipelineShaderIdentifiers.back().get()->ptr;

                    if (fragmentShaderModule == DE_NULL)
                        shaderModuleIdFlags |= VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT;
                }
#endif // CTS_USES_VULKANSC
                break;
            }
        }
    }

    if (m_internalData->pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
    {
        m_internalData->monolithicPipelineCreateInfo.pDepthStencilState = pDepthStencilState;
        m_internalData->monolithicPipelineCreateInfo.pMultisampleState  = pMultisampleState;
        m_internalData->monolithicPipelineCreateInfo.stageCount += (hasFrag ? 1u : 0u);
        m_internalData->monolithicPipelineCreateInfo.flags |= shaderModuleIdFlags;
    }

#ifndef CTS_USES_VULKANSC
    // note we could just use else to if statement above but sinc
    // this section is cut out for Vulkan SC its cleaner with separate if
    if (m_internalData->pipelineConstructionType != PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
    {
        auto libraryCreateInfo =
            makeGraphicsPipelineLibraryCreateInfo(VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT);
        void *firstStructInChain = reinterpret_cast<void *>(&libraryCreateInfo);
        addToChain(&firstStructInChain, m_internalData->pFragmentShadingRateState);
        addToChain(&firstStructInChain, m_internalData->pRenderingState.ptr);
        addToChain(&firstStructInChain, partCreationFeedback.ptr);

        VkGraphicsPipelineCreateInfo pipelinePartCreateInfo = initVulkanStructure();
        pipelinePartCreateInfo.pNext                        = firstStructInChain;
        pipelinePartCreateInfo.flags =
            m_internalData->pipelineFlags | VK_PIPELINE_CREATE_LIBRARY_BIT_KHR | shaderModuleIdFlags;
        pipelinePartCreateInfo.layout             = layout;
        pipelinePartCreateInfo.renderPass         = renderPass;
        pipelinePartCreateInfo.subpass            = subpass;
        pipelinePartCreateInfo.pDepthStencilState = pDepthStencilState;
        pipelinePartCreateInfo.pMultisampleState  = pMultisampleState;
        pipelinePartCreateInfo.stageCount         = hasFrag;
        pipelinePartCreateInfo.pStages       = hasFrag ? &m_internalData->pipelineShaderStages[stageIndex] : DE_NULL;
        pipelinePartCreateInfo.pDynamicState = m_internalData->pDynamicState;

        if (m_internalData->pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY)
            pipelinePartCreateInfo.flags |= VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT;

        if ((shaderModuleIdFlags & VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT) != 0)
            m_internalData->failOnCompileWhenLinking = true;

        m_pipelineParts[2] = makeGraphicsPipeline(m_internalData->vk, m_internalData->device, partPipelineCache,
                                                  &pipelinePartCreateInfo);
    }
#endif // CTS_USES_VULKANSC

    return *this;
}

GraphicsPipelineWrapper &GraphicsPipelineWrapper::setupFragmentOutputState(
    const VkRenderPass renderPass, const uint32_t subpass, const VkPipelineColorBlendStateCreateInfo *colorBlendState,
    const VkPipelineMultisampleStateCreateInfo *multisampleState, const VkPipelineCache partPipelineCache,
    PipelineCreationFeedbackCreateInfoWrapper partCreationFeedback)
{
    // make sure pipeline was not already build
    DE_ASSERT(m_pipelineFinal.get() == DE_NULL);

    // make sure states are set in order - no need to complicate logic to support out of order specification - this state needs to be set last
    DE_ASSERT(m_internalData && (m_internalData->setupState ==
                                 (PSS_VERTEX_INPUT_INTERFACE | PSS_PRE_RASTERIZATION_SHADERS | PSS_FRAGMENT_SHADER)));
    m_internalData->setupState |= PSS_FRAGMENT_OUTPUT_INTERFACE;

    // Unreference variables that are not used in Vulkan SC. No need to put this in ifdef.
    DE_UNREF(renderPass);
    DE_UNREF(subpass);
    DE_UNREF(partPipelineCache);
    DE_UNREF(partCreationFeedback);

    void *firstStructInChain = DE_NULL;
    addToChain(&firstStructInChain, m_internalData->pFragmentShadingRateState);

#ifndef CTS_USES_VULKANSC
    addToChain(&firstStructInChain, m_internalData->pRenderingState.ptr);
#endif // CTS_USES_VULKANSC

    const auto pColorBlendState = colorBlendState ?
                                      colorBlendState :
                                      (m_internalData->useDefaultColorBlendState ? &defaultColorBlendState : DE_NULL);
    const auto pMultisampleState =
        multisampleState ? multisampleState :
                           (m_internalData->useDefaultMultisampleState ? &defaultMultisampleState : DE_NULL);

    if (m_internalData->pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
    {
        m_internalData->monolithicPipelineCreateInfo.pNext = firstStructInChain;
        m_internalData->monolithicPipelineCreateInfo.flags |= m_internalData->pipelineFlags;
        m_internalData->monolithicPipelineCreateInfo.pColorBlendState  = pColorBlendState;
        m_internalData->monolithicPipelineCreateInfo.pMultisampleState = pMultisampleState;
    }

#ifndef CTS_USES_VULKANSC
    // note we could just use else to if statement above but sinc
    // this section is cut out for Vulkan SC its cleaner with separate if
    if (m_internalData->pipelineConstructionType != PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
    {
        auto libraryCreateInfo =
            makeGraphicsPipelineLibraryCreateInfo(VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT);
        addToChain(&firstStructInChain, &libraryCreateInfo);
        addToChain(&firstStructInChain, partCreationFeedback.ptr);

        VkGraphicsPipelineCreateInfo pipelinePartCreateInfo = initVulkanStructure();
        pipelinePartCreateInfo.pNext                        = firstStructInChain;
        pipelinePartCreateInfo.flags             = m_internalData->pipelineFlags | VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
        pipelinePartCreateInfo.renderPass        = renderPass;
        pipelinePartCreateInfo.subpass           = subpass;
        pipelinePartCreateInfo.pColorBlendState  = pColorBlendState;
        pipelinePartCreateInfo.pMultisampleState = pMultisampleState;
        pipelinePartCreateInfo.pDynamicState     = m_internalData->pDynamicState;

        if (m_internalData->pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY)
            pipelinePartCreateInfo.flags |= VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT;

        m_pipelineParts[3] = createGraphicsPipeline(m_internalData->vk, m_internalData->device, partPipelineCache,
                                                    &pipelinePartCreateInfo);
    }
#endif // CTS_USES_VULKANSC

    return *this;
}

void GraphicsPipelineWrapper::buildPipeline(const VkPipelineCache pipelineCache, const VkPipeline basePipelineHandle,
                                            const int32_t basePipelineIndex,
                                            PipelineCreationFeedbackCreateInfoWrapper creationFeedback)
{
    // make sure we are not trying to build pipeline second time
    DE_ASSERT(m_pipelineFinal.get() == DE_NULL);

    // make sure all states were set
    DE_ASSERT(m_internalData &&
              (m_internalData->setupState == (PSS_VERTEX_INPUT_INTERFACE | PSS_PRE_RASTERIZATION_SHADERS |
                                              PSS_FRAGMENT_SHADER | PSS_FRAGMENT_OUTPUT_INTERFACE)));

    // Unreference variables that are not used in Vulkan SC. No need to put this in ifdef.
    DE_UNREF(creationFeedback);

    VkGraphicsPipelineCreateInfo *pointerToCreateInfo = &m_internalData->monolithicPipelineCreateInfo;

#ifndef CTS_USES_VULKANSC
    VkGraphicsPipelineCreateInfo linkedCreateInfo = initVulkanStructure();
    VkPipeline rawPipelines[4];
    VkPipelineLibraryCreateInfoKHR linkingInfo{
        VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR, // VkStructureType sType;
        creationFeedback.ptr,                               // const void* pNext;
        4,                                                  // uint32_t libraryCount;
        rawPipelines                                        // const VkPipeline* pLibraries;
    };

    if (m_internalData->pipelineConstructionType != PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
    {
        for (uint32_t i = 0; i < 4; ++i)
            rawPipelines[i] = m_pipelineParts[i].get();

        linkedCreateInfo.flags  = m_internalData->pipelineFlags;
        linkedCreateInfo.layout = m_internalData->monolithicPipelineCreateInfo.layout;
        pointerToCreateInfo     = &linkedCreateInfo;

        linkedCreateInfo.pNext = &linkingInfo;

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
    }
#endif // CTS_USES_VULKANSC

    pointerToCreateInfo->basePipelineHandle = basePipelineHandle;
    pointerToCreateInfo->basePipelineIndex  = basePipelineIndex;

    m_pipelineFinal =
        makeGraphicsPipeline(m_internalData->vk, m_internalData->device, pipelineCache, pointerToCreateInfo);

    // pipeline was created - we can free CreateInfo structures
    m_internalData.clear();
}

bool GraphicsPipelineWrapper::wasBuild() const
{
    return !!m_pipelineFinal.get();
}

VkPipeline GraphicsPipelineWrapper::getPipeline() const
{
    DE_ASSERT(m_pipelineFinal.get() != DE_NULL);
    return m_pipelineFinal.get();
}

void GraphicsPipelineWrapper::destroyPipeline(void)
{
    DE_ASSERT(m_pipelineFinal.get() != DE_NULL);

    m_pipelineFinal = Move<VkPipeline>();
}

} // namespace vk
