/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
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
 * \brief Vulkan SC utilities
 *//*--------------------------------------------------------------------*/

#include "vkSafetyCriticalUtil.hpp"
#include <set>

#ifdef CTS_USES_VULKANSC

namespace vk
{
struct MemoryArea
{
    MemoryArea(const void *data_, std::size_t size_) : data(data_), size(size_)
    {
    }

    const void *data;
    std::size_t size;
};
} // namespace vk

namespace std
{
template <>
struct hash<vk::MemoryArea>
{
    std::size_t operator()(const vk::MemoryArea &s) const noexcept
    {
        std::size_t seed = 0;
        std::hash<unsigned char> hasher;
        for (std::size_t i = 0; i < s.size; ++i)
        {
            unsigned char *v = (unsigned char *)s.data + i;
            seed ^= hasher(*v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
};
} // namespace std

namespace vk
{

VkDeviceObjectReservationCreateInfo resetDeviceObjectReservationCreateInfo()
{
    VkDeviceObjectReservationCreateInfo result = {
        VK_STRUCTURE_TYPE_DEVICE_OBJECT_RESERVATION_CREATE_INFO, // VkStructureType sType;
        DE_NULL,                                                 // const void* pNext;
        0u,                                                      // uint32_t pipelineCacheCreateInfoCount;
        DE_NULL, // const VkPipelineCacheCreateInfo* pPipelineCacheCreateInfos;
        0u,      // uint32_t pipelinePoolSizeCount;
        DE_NULL, // const VkPipelinePoolSize* pPipelinePoolSizes;
        0u,      // uint32_t semaphoreRequestCount;
        0u,      // uint32_t commandBufferRequestCount;
        0u,      // uint32_t fenceRequestCount;
        0u,      // uint32_t deviceMemoryRequestCount;
        0u,      // uint32_t bufferRequestCount;
        0u,      // uint32_t imageRequestCount;
        0u,      // uint32_t eventRequestCount;
        0u,      // uint32_t queryPoolRequestCount;
        0u,      // uint32_t bufferViewRequestCount;
        0u,      // uint32_t imageViewRequestCount;
        0u,      // uint32_t layeredImageViewRequestCount;
        0u,      // uint32_t pipelineCacheRequestCount;
        0u,      // uint32_t pipelineLayoutRequestCount;
        0u,      // uint32_t renderPassRequestCount;
        0u,      // uint32_t graphicsPipelineRequestCount;
        0u,      // uint32_t computePipelineRequestCount;
        0u,      // uint32_t descriptorSetLayoutRequestCount;
        0u,      // uint32_t samplerRequestCount;
        0u,      // uint32_t descriptorPoolRequestCount;
        0u,      // uint32_t descriptorSetRequestCount;
        0u,      // uint32_t framebufferRequestCount;
        0u,      // uint32_t commandPoolRequestCount;
        0u,      // uint32_t samplerYcbcrConversionRequestCount;
        0u,      // uint32_t surfaceRequestCount;
        0u,      // uint32_t swapchainRequestCount;
        0u,      // uint32_t displayModeRequestCount;
        0u,      // uint32_t subpassDescriptionRequestCount;
        0u,      // uint32_t attachmentDescriptionRequestCount;
        0u,      // uint32_t descriptorSetLayoutBindingRequestCount;
        0u,      // uint32_t descriptorSetLayoutBindingLimit;
        0u,      // uint32_t maxImageViewMipLevels;
        0u,      // uint32_t maxImageViewArrayLayers;
        0u,      // uint32_t maxLayeredImageViewMipLevels;
        0u,      // uint32_t maxOcclusionQueriesPerPool;
        0u,      // uint32_t maxPipelineStatisticsQueriesPerPool;
        0u,      // uint32_t maxTimestampQueriesPerPool;
        0u,      // uint32_t maxImmutableSamplersPerDescriptorSetLayout;
    };
    return result;
}

VkPipelineOfflineCreateInfo resetPipelineOfflineCreateInfo()
{
    VkPipelineOfflineCreateInfo pipelineID = {
        VK_STRUCTURE_TYPE_PIPELINE_OFFLINE_CREATE_INFO,         // VkStructureType sType;
        DE_NULL,                                                // const void* pNext;
        {0},                                                    // uint8_t pipelineIdentifier[VK_UUID_SIZE];
        VK_PIPELINE_MATCH_CONTROL_APPLICATION_UUID_EXACT_MATCH, // VkPipelineMatchControl matchControl;
        0u                                                      // VkDeviceSize poolEntrySize;

    };
    for (uint32_t i = 0; i < VK_UUID_SIZE; ++i)
        pipelineID.pipelineIdentifier[i] = 0U;

    return pipelineID;
}

void applyPipelineIdentifier(VkPipelineOfflineCreateInfo &pipelineID, const std::string &value)
{
    for (uint32_t i = 0; i < VK_UUID_SIZE && i < value.size(); ++i)
        pipelineID.pipelineIdentifier[i] = uint8_t(value[i]);
}

VkPhysicalDeviceVulkanSC10Features createDefaultSC10Features()
{
    VkPhysicalDeviceVulkanSC10Features result = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_SC_1_0_FEATURES, // VkStructureType sType;
        DE_NULL,                                                  // void* pNext;
        VK_FALSE                                                  // VkBool32 shaderAtomicInstructions;
    };
    return result;
}

void hashPNextChain(std::size_t &seed, const void *pNext, const std::map<uint64_t, std::size_t> &objectHashes)
{
    VkBaseInStructure *pBase = (VkBaseInStructure *)pNext;
    if (pNext != DE_NULL)
    {
        switch (pBase->sType)
        {
        case VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_STENCIL_LAYOUT:
        {
            VkAttachmentDescriptionStencilLayout *ptr = (VkAttachmentDescriptionStencilLayout *)pNext;
            hash_combine(seed, uint32_t(ptr->stencilInitialLayout), uint32_t(ptr->stencilFinalLayout));
            break;
        }
        case VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO:
        {
            VkDescriptorSetLayoutBindingFlagsCreateInfo *ptr = (VkDescriptorSetLayoutBindingFlagsCreateInfo *)pNext;
            if (ptr->pBindingFlags != DE_NULL)
                for (uint32_t i = 0; i < ptr->bindingCount; ++i)
                    hash_combine(seed, ptr->pBindingFlags[i]);
            break;
        }
        case VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_ADVANCED_STATE_CREATE_INFO_EXT:
        {
            VkPipelineColorBlendAdvancedStateCreateInfoEXT *ptr =
                (VkPipelineColorBlendAdvancedStateCreateInfoEXT *)pNext;
            hash_combine(seed, ptr->srcPremultiplied, ptr->dstPremultiplied, uint32_t(ptr->blendOverlap));
            break;
        }
        case VK_STRUCTURE_TYPE_PIPELINE_COLOR_WRITE_CREATE_INFO_EXT:
        {
            VkPipelineColorWriteCreateInfoEXT *ptr = (VkPipelineColorWriteCreateInfoEXT *)pNext;
            if (ptr->pColorWriteEnables != DE_NULL)
                for (uint32_t i = 0; i < ptr->attachmentCount; ++i)
                    hash_combine(seed, ptr->pColorWriteEnables[i]);
            break;
        }
        case VK_STRUCTURE_TYPE_PIPELINE_DISCARD_RECTANGLE_STATE_CREATE_INFO_EXT:
        {
            VkPipelineDiscardRectangleStateCreateInfoEXT *ptr = (VkPipelineDiscardRectangleStateCreateInfoEXT *)pNext;
            hash_combine(seed, ptr->flags, uint32_t(ptr->discardRectangleMode));
            if (ptr->pDiscardRectangles != DE_NULL)
                for (uint32_t i = 0; i < ptr->discardRectangleCount; ++i)
                    hash_combine(seed, ptr->pDiscardRectangles[i].offset.x, ptr->pDiscardRectangles[i].offset.y,
                                 ptr->pDiscardRectangles[i].extent.width, ptr->pDiscardRectangles[i].extent.height);
            break;
        }
        case VK_STRUCTURE_TYPE_PIPELINE_FRAGMENT_SHADING_RATE_STATE_CREATE_INFO_KHR:
        {
            VkPipelineFragmentShadingRateStateCreateInfoKHR *ptr =
                (VkPipelineFragmentShadingRateStateCreateInfoKHR *)pNext;
            hash_combine(seed, ptr->fragmentSize.width, ptr->fragmentSize.height, uint32_t(ptr->combinerOps[0]),
                         uint32_t(ptr->combinerOps[1]));
            break;
        }
        case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT:
        {
            VkPipelineRasterizationConservativeStateCreateInfoEXT *ptr =
                (VkPipelineRasterizationConservativeStateCreateInfoEXT *)pNext;
            hash_combine(seed, ptr->flags, uint32_t(ptr->conservativeRasterizationMode),
                         ptr->extraPrimitiveOverestimationSize);
            break;
        }
        case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT:
        {
            VkPipelineRasterizationDepthClipStateCreateInfoEXT *ptr =
                (VkPipelineRasterizationDepthClipStateCreateInfoEXT *)pNext;
            hash_combine(seed, ptr->flags, ptr->depthClipEnable);
            break;
        }
        case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO_EXT:
        {
            VkPipelineRasterizationLineStateCreateInfoEXT *ptr = (VkPipelineRasterizationLineStateCreateInfoEXT *)pNext;
            hash_combine(seed, uint32_t(ptr->lineRasterizationMode), ptr->stippledLineEnable, ptr->lineStippleFactor,
                         ptr->lineStipplePattern);
            break;
        }
        case VK_STRUCTURE_TYPE_PIPELINE_SAMPLE_LOCATIONS_STATE_CREATE_INFO_EXT:
        {
            VkPipelineSampleLocationsStateCreateInfoEXT *ptr = (VkPipelineSampleLocationsStateCreateInfoEXT *)pNext;
            hash_combine(seed, ptr->sampleLocationsEnable, uint32_t(ptr->sampleLocationsInfo.sampleLocationsPerPixel),
                         ptr->sampleLocationsInfo.sampleLocationGridSize.width,
                         ptr->sampleLocationsInfo.sampleLocationGridSize.height);
            if (ptr->sampleLocationsInfo.pSampleLocations != DE_NULL)
                for (uint32_t i = 0; i < ptr->sampleLocationsInfo.sampleLocationsCount; ++i)
                    hash_combine(seed, ptr->sampleLocationsInfo.pSampleLocations[i].x,
                                 ptr->sampleLocationsInfo.pSampleLocations[i].y);
            break;
        }
        case VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO_EXT:
        {
            VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT *ptr =
                (VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT *)pNext;
            hash_combine(seed, ptr->requiredSubgroupSize);
            break;
        }
        case VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_DOMAIN_ORIGIN_STATE_CREATE_INFO:
        {
            VkPipelineTessellationDomainOriginStateCreateInfo *ptr =
                (VkPipelineTessellationDomainOriginStateCreateInfo *)pNext;
            hash_combine(seed, uint32_t(ptr->domainOrigin));
            break;
        }
        case VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_KHR:
        {
            VkPipelineVertexInputDivisorStateCreateInfoEXT *ptr =
                (VkPipelineVertexInputDivisorStateCreateInfoEXT *)pNext;
            if (ptr->pVertexBindingDivisors != DE_NULL)
                for (uint32_t i = 0; i < ptr->vertexBindingDivisorCount; ++i)
                    hash_combine(seed, ptr->pVertexBindingDivisors[i].binding, ptr->pVertexBindingDivisors[i].divisor);
            break;
        }
        case VK_STRUCTURE_TYPE_RENDER_PASS_INPUT_ATTACHMENT_ASPECT_CREATE_INFO:
        {
            VkRenderPassInputAttachmentAspectCreateInfo *ptr = (VkRenderPassInputAttachmentAspectCreateInfo *)pNext;
            if (ptr->pAspectReferences != DE_NULL)
                for (uint32_t i = 0; i < ptr->aspectReferenceCount; ++i)
                    hash_combine(seed, ptr->pAspectReferences[i].subpass,
                                 ptr->pAspectReferences[i].inputAttachmentIndex, ptr->pAspectReferences[i].aspectMask);
            break;
        }
        case VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO:
        {
            VkRenderPassMultiviewCreateInfo *ptr = (VkRenderPassMultiviewCreateInfo *)pNext;
            if (ptr->pViewMasks != DE_NULL)
                for (uint32_t i = 0; i < ptr->subpassCount; ++i)
                    hash_combine(seed, ptr->pViewMasks[i]);
            if (ptr->pViewOffsets != DE_NULL)
                for (uint32_t i = 0; i < ptr->dependencyCount; ++i)
                    hash_combine(seed, ptr->pViewOffsets[i]);
            if (ptr->pCorrelationMasks != DE_NULL)
                for (uint32_t i = 0; i < ptr->correlationMaskCount; ++i)
                    hash_combine(seed, ptr->pCorrelationMasks[i]);
            break;
        }
        case VK_STRUCTURE_TYPE_SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT:
        {
            VkSamplerCustomBorderColorCreateInfoEXT *ptr = (VkSamplerCustomBorderColorCreateInfoEXT *)pNext;
            for (uint32_t i = 0; i < 4; ++i)
                hash_combine(seed, ptr->customBorderColor.uint32[i]);
            hash_combine(seed, uint32_t(ptr->format));
            break;
        }
        case VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO:
        {
            VkSamplerReductionModeCreateInfo *ptr = (VkSamplerReductionModeCreateInfo *)pNext;
            hash_combine(seed, uint32_t(ptr->reductionMode));
            break;
        }
        case VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO:
        {
            VkSamplerYcbcrConversionInfo *ptr = (VkSamplerYcbcrConversionInfo *)pNext;
            {
                auto it = objectHashes.find(ptr->conversion.getInternal());
                if (it != end(objectHashes))
                    hash_combine(seed, it->second);
            }
            break;
        }
        default:
            break;
        }
        hashPNextChain(seed, pBase->pNext, objectHashes);
    }
}

bool graphicsPipelineHasDynamicState(const VkGraphicsPipelineCreateInfo &gpCI, VkDynamicState state)
{
    if (gpCI.pDynamicState == DE_NULL)
        return false;

    if (gpCI.pDynamicState->pDynamicStates == DE_NULL)
        return false;

    for (uint32_t i = 0; i < gpCI.pDynamicState->dynamicStateCount; ++i)
        if (gpCI.pDynamicState->pDynamicStates[i] == state)
            return true;

    return false;
}

std::size_t calculateGraphicsPipelineHash(const VkGraphicsPipelineCreateInfo &gpCI,
                                          const std::map<uint64_t, std::size_t> &objectHashes)
{
    std::size_t seed = 0;

    hashPNextChain(seed, gpCI.pNext, objectHashes);

    hash_combine(seed, gpCI.flags);

    bool vertexInputStateRequired       = false;
    bool inputAssemblyStateRequired     = false;
    bool tessellationStateRequired      = false;
    bool viewportStateRequired          = false;
    bool viewportStateViewportsRequired = false;
    bool viewportStateScissorsRequired  = false;
    bool multiSampleStateRequired       = false;
    bool depthStencilStateRequired      = false;
    bool colorBlendStateRequired        = false;

    if (gpCI.pStages != DE_NULL)
    {
        for (uint32_t i = 0; i < gpCI.stageCount; ++i)
        {
            hashPNextChain(seed, gpCI.pStages[i].pNext, objectHashes);

            hash_combine(seed, uint32_t(gpCI.pStages[i].flags), uint32_t(gpCI.pStages[i].stage));
            auto it = objectHashes.find(gpCI.pStages[i].module.getInternal());
            if (it != end(objectHashes))
                hash_combine(seed, it->second);

            hash_combine(seed, std::string(gpCI.pStages[i].pName));

            if (gpCI.pStages[i].pSpecializationInfo != DE_NULL)
            {
                if (gpCI.pStages[i].pSpecializationInfo->pMapEntries != DE_NULL)
                {
                    for (uint32_t j = 0; j < gpCI.pStages[i].pSpecializationInfo->mapEntryCount; ++j)
                        hash_combine(seed, gpCI.pStages[i].pSpecializationInfo->pMapEntries[j].constantID,
                                     gpCI.pStages[i].pSpecializationInfo->pMapEntries[j].offset,
                                     gpCI.pStages[i].pSpecializationInfo->pMapEntries[j].size);

                    hash_combine(seed, MemoryArea(gpCI.pStages[i].pSpecializationInfo->pData,
                                                  gpCI.pStages[i].pSpecializationInfo->dataSize));
                }
            }
            if (gpCI.pStages[i].stage == VK_SHADER_STAGE_VERTEX_BIT)
            {
                vertexInputStateRequired   = true;
                inputAssemblyStateRequired = true;
            }
            if (gpCI.pStages[i].stage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
            {
                tessellationStateRequired = true;
            }
        }
    }
    if (gpCI.pDynamicState != DE_NULL)
    {
        if (gpCI.pDynamicState->pDynamicStates != DE_NULL)
            for (uint32_t i = 0; i < gpCI.pDynamicState->dynamicStateCount; ++i)
            {
                if (gpCI.pDynamicState->pDynamicStates[i] == VK_DYNAMIC_STATE_VIEWPORT ||
                    gpCI.pDynamicState->pDynamicStates[i] == VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT_EXT)
                {
                    viewportStateRequired          = true;
                    viewportStateViewportsRequired = true;
                }
                if (gpCI.pDynamicState->pDynamicStates[i] == VK_DYNAMIC_STATE_SCISSOR ||
                    gpCI.pDynamicState->pDynamicStates[i] == VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT_EXT)
                {
                    viewportStateRequired         = true;
                    viewportStateScissorsRequired = true;
                }
                if (gpCI.pDynamicState->pDynamicStates[i] == VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE_EXT)
                    viewportStateRequired = true;
            }
    }
    if (gpCI.pRasterizationState != DE_NULL)
    {
        if (gpCI.pRasterizationState->rasterizerDiscardEnable == VK_FALSE)
        {
            viewportStateRequired          = true;
            viewportStateViewportsRequired = true;
            viewportStateScissorsRequired  = true;
            multiSampleStateRequired       = true;
            depthStencilStateRequired      = true;
            colorBlendStateRequired        = true;
        }
    }

    if (vertexInputStateRequired && gpCI.pVertexInputState != DE_NULL)
    {
        hashPNextChain(seed, gpCI.pVertexInputState->pNext, objectHashes);
        hash_combine(seed, gpCI.pVertexInputState->flags);
        if (gpCI.pVertexInputState->pVertexBindingDescriptions != DE_NULL)
            for (uint32_t i = 0; i < gpCI.pVertexInputState->vertexBindingDescriptionCount; ++i)
                hash_combine(seed, gpCI.pVertexInputState->pVertexBindingDescriptions[i].binding,
                             gpCI.pVertexInputState->pVertexBindingDescriptions[i].stride,
                             uint32_t(gpCI.pVertexInputState->pVertexBindingDescriptions[i].inputRate));
        if (gpCI.pVertexInputState->pVertexAttributeDescriptions != DE_NULL)
            for (uint32_t i = 0; i < gpCI.pVertexInputState->vertexAttributeDescriptionCount; ++i)
                hash_combine(seed, gpCI.pVertexInputState->pVertexAttributeDescriptions[i].location,
                             gpCI.pVertexInputState->pVertexAttributeDescriptions[i].binding,
                             uint32_t(gpCI.pVertexInputState->pVertexAttributeDescriptions[i].format),
                             gpCI.pVertexInputState->pVertexAttributeDescriptions[i].offset);
    }

    if (inputAssemblyStateRequired && gpCI.pInputAssemblyState != DE_NULL)
    {
        hashPNextChain(seed, gpCI.pInputAssemblyState->pNext, objectHashes);
        hash_combine(seed, uint32_t(gpCI.pInputAssemblyState->flags), uint32_t(gpCI.pInputAssemblyState->topology),
                     gpCI.pInputAssemblyState->primitiveRestartEnable);
    }
    if (tessellationStateRequired && gpCI.pTessellationState != DE_NULL)
    {
        hashPNextChain(seed, gpCI.pTessellationState->pNext, objectHashes);
        hash_combine(seed, gpCI.pTessellationState->flags, gpCI.pTessellationState->patchControlPoints);
    }
    if (viewportStateRequired && gpCI.pViewportState != DE_NULL)
    {
        hashPNextChain(seed, gpCI.pViewportState->pNext, objectHashes);
        hash_combine(seed, gpCI.pViewportState->flags);

        if (viewportStateViewportsRequired && gpCI.pViewportState->pViewports != DE_NULL)
            for (uint32_t i = 0; i < gpCI.pViewportState->viewportCount; ++i)
                hash_combine(seed, gpCI.pViewportState->pViewports[i].x, gpCI.pViewportState->pViewports[i].y,
                             gpCI.pViewportState->pViewports[i].width, gpCI.pViewportState->pViewports[i].height,
                             gpCI.pViewportState->pViewports[i].minDepth, gpCI.pViewportState->pViewports[i].maxDepth);

        if (viewportStateScissorsRequired && gpCI.pViewportState->pScissors != DE_NULL)
            for (uint32_t i = 0; i < gpCI.pViewportState->scissorCount; ++i)
                hash_combine(seed, gpCI.pViewportState->pScissors[i].offset.x,
                             gpCI.pViewportState->pScissors[i].offset.y, gpCI.pViewportState->pScissors[i].extent.width,
                             gpCI.pViewportState->pScissors[i].extent.height);
    }
    if (gpCI.pRasterizationState != DE_NULL)
    {
        hashPNextChain(seed, gpCI.pRasterizationState->pNext, objectHashes);
        hash_combine(seed, uint32_t(gpCI.pRasterizationState->flags), gpCI.pRasterizationState->depthClampEnable,
                     gpCI.pRasterizationState->rasterizerDiscardEnable, uint32_t(gpCI.pRasterizationState->polygonMode),
                     uint32_t(gpCI.pRasterizationState->cullMode), uint32_t(gpCI.pRasterizationState->frontFace),
                     gpCI.pRasterizationState->depthBiasEnable, gpCI.pRasterizationState->depthBiasConstantFactor,
                     gpCI.pRasterizationState->depthBiasClamp, gpCI.pRasterizationState->depthBiasSlopeFactor,
                     gpCI.pRasterizationState->lineWidth);
    }
    if (multiSampleStateRequired && gpCI.pMultisampleState != DE_NULL)
    {
        hashPNextChain(seed, gpCI.pMultisampleState->pNext, objectHashes);
        hash_combine(seed, uint32_t(gpCI.pMultisampleState->flags),
                     uint32_t(gpCI.pMultisampleState->rasterizationSamples),
                     gpCI.pMultisampleState->sampleShadingEnable, gpCI.pMultisampleState->minSampleShading);
        if (gpCI.pMultisampleState->pSampleMask != DE_NULL)
            for (int i = 0; i < ((gpCI.pMultisampleState->rasterizationSamples + 31) / 32); i++)
                hash_combine(seed, gpCI.pMultisampleState->pSampleMask[i]);
        hash_combine(seed, gpCI.pMultisampleState->alphaToCoverageEnable, gpCI.pMultisampleState->alphaToOneEnable);
    }
    if (depthStencilStateRequired && gpCI.pDepthStencilState != DE_NULL)
    {
        hashPNextChain(seed, gpCI.pDepthStencilState->pNext, objectHashes);
        hash_combine(seed, uint32_t(gpCI.pDepthStencilState->flags), gpCI.pDepthStencilState->depthTestEnable,
                     gpCI.pDepthStencilState->depthWriteEnable, uint32_t(gpCI.pDepthStencilState->depthCompareOp),
                     gpCI.pDepthStencilState->depthBoundsTestEnable, gpCI.pDepthStencilState->stencilTestEnable);
        if (gpCI.pDepthStencilState->stencilTestEnable)
        {
            hash_combine(seed, uint32_t(gpCI.pDepthStencilState->front.failOp),
                         uint32_t(gpCI.pDepthStencilState->front.passOp),
                         uint32_t(gpCI.pDepthStencilState->front.depthFailOp),
                         uint32_t(gpCI.pDepthStencilState->front.compareOp), gpCI.pDepthStencilState->front.compareMask,
                         gpCI.pDepthStencilState->front.writeMask, gpCI.pDepthStencilState->front.reference);
            hash_combine(seed, uint32_t(gpCI.pDepthStencilState->back.failOp),
                         uint32_t(gpCI.pDepthStencilState->back.passOp),
                         uint32_t(gpCI.pDepthStencilState->back.depthFailOp),
                         uint32_t(gpCI.pDepthStencilState->back.compareOp), gpCI.pDepthStencilState->back.compareMask,
                         gpCI.pDepthStencilState->back.writeMask, gpCI.pDepthStencilState->back.reference);
        }
        hash_combine(seed, gpCI.pDepthStencilState->minDepthBounds, gpCI.pDepthStencilState->maxDepthBounds);
    }
    if (colorBlendStateRequired && gpCI.pColorBlendState != DE_NULL)
    {
        hashPNextChain(seed, gpCI.pColorBlendState->pNext, objectHashes);
        hash_combine(seed, uint32_t(gpCI.pColorBlendState->flags), gpCI.pColorBlendState->logicOpEnable,
                     uint32_t(gpCI.pColorBlendState->logicOp));

        bool hashBlendConstants              = false;
        std::set<VkBlendFactor> constFactors = {
            VK_BLEND_FACTOR_CONSTANT_COLOR, VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR, VK_BLEND_FACTOR_CONSTANT_ALPHA,
            VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA};
        if (gpCI.pColorBlendState->pAttachments != DE_NULL)
        {
            for (uint32_t i = 0; i < gpCI.pColorBlendState->attachmentCount; ++i)
            {
                hash_combine(seed, gpCI.pColorBlendState->pAttachments[i].blendEnable,
                             uint32_t(gpCI.pColorBlendState->pAttachments[i].srcColorBlendFactor),
                             uint32_t(gpCI.pColorBlendState->pAttachments[i].dstColorBlendFactor),
                             uint32_t(gpCI.pColorBlendState->pAttachments[i].colorBlendOp),
                             uint32_t(gpCI.pColorBlendState->pAttachments[i].srcAlphaBlendFactor),
                             uint32_t(gpCI.pColorBlendState->pAttachments[i].dstAlphaBlendFactor),
                             uint32_t(gpCI.pColorBlendState->pAttachments[i].alphaBlendOp),
                             uint32_t(gpCI.pColorBlendState->pAttachments[i].colorWriteMask));
                if (constFactors.find(gpCI.pColorBlendState->pAttachments[i].srcColorBlendFactor) != end(constFactors))
                    hashBlendConstants = true;
                if (constFactors.find(gpCI.pColorBlendState->pAttachments[i].dstColorBlendFactor) != end(constFactors))
                    hashBlendConstants = true;
                if (constFactors.find(gpCI.pColorBlendState->pAttachments[i].srcAlphaBlendFactor) != end(constFactors))
                    hashBlendConstants = true;
                if (constFactors.find(gpCI.pColorBlendState->pAttachments[i].dstAlphaBlendFactor) != end(constFactors))
                    hashBlendConstants = true;
            }
        }
        // omit blendConstants when VK_DYNAMIC_STATE_BLEND_CONSTANTS is present
        if (hashBlendConstants && !graphicsPipelineHasDynamicState(gpCI, VK_DYNAMIC_STATE_BLEND_CONSTANTS))
            for (uint32_t i = 0; i < 4; ++i)
                hash_combine(seed, gpCI.pColorBlendState->blendConstants[i]);
    }
    if (gpCI.pDynamicState != DE_NULL)
    {
        hashPNextChain(seed, gpCI.pDynamicState->pNext, objectHashes);
        hash_combine(seed, gpCI.pDynamicState->flags);
        if (gpCI.pDynamicState->pDynamicStates != DE_NULL)
            for (uint32_t i = 0; i < gpCI.pDynamicState->dynamicStateCount; ++i)
                hash_combine(seed, uint32_t(gpCI.pDynamicState->pDynamicStates[i]));
    }

    {
        auto it = objectHashes.find(gpCI.layout.getInternal());
        if (it != end(objectHashes))
            hash_combine(seed, it->second);
    }

    {
        auto it = objectHashes.find(gpCI.renderPass.getInternal());
        if (it != end(objectHashes))
            hash_combine(seed, it->second);
    }

    hash_combine(seed, gpCI.subpass);

    {
        auto it = objectHashes.find(gpCI.basePipelineHandle.getInternal());
        if (it != end(objectHashes))
            hash_combine(seed, it->second);
    }
    hash_combine(seed, gpCI.basePipelineIndex);

    return seed;
}

std::size_t calculateComputePipelineHash(const VkComputePipelineCreateInfo &cpCI,
                                         const std::map<uint64_t, std::size_t> &objectHashes)
{
    std::size_t seed = 0;

    hashPNextChain(seed, cpCI.pNext, objectHashes);

    hash_combine(seed, cpCI.flags);

    {
        hash_combine(seed, uint32_t(cpCI.stage.flags), uint32_t(cpCI.stage.stage));
        auto it = objectHashes.find(cpCI.stage.module.getInternal());
        if (it != end(objectHashes))
            hash_combine(seed, it->second);

        hash_combine(seed, std::string(cpCI.stage.pName));

        if (cpCI.stage.pSpecializationInfo != DE_NULL)
        {
            if (cpCI.stage.pSpecializationInfo->pMapEntries != DE_NULL)
            {
                for (uint32_t j = 0; j < cpCI.stage.pSpecializationInfo->mapEntryCount; ++j)
                    hash_combine(seed, cpCI.stage.pSpecializationInfo->pMapEntries[j].constantID,
                                 cpCI.stage.pSpecializationInfo->pMapEntries[j].offset,
                                 cpCI.stage.pSpecializationInfo->pMapEntries[j].size);

                hash_combine(
                    seed, MemoryArea(cpCI.stage.pSpecializationInfo->pData, cpCI.stage.pSpecializationInfo->dataSize));
            }
        }
    }

    {
        auto it = objectHashes.find(cpCI.layout.getInternal());
        if (it != end(objectHashes))
            hash_combine(seed, it->second);
    }

    {
        auto it = objectHashes.find(cpCI.basePipelineHandle.getInternal());
        if (it != end(objectHashes))
            hash_combine(seed, it->second);
    }
    hash_combine(seed, cpCI.basePipelineIndex);

    return seed;
}

std::size_t calculateSamplerYcbcrConversionHash(const VkSamplerYcbcrConversionCreateInfo &scCI,
                                                const std::map<uint64_t, std::size_t> &objectHashes)
{
    DE_UNREF(objectHashes);
    std::size_t seed = 0;
    hashPNextChain(seed, scCI.pNext, objectHashes);
    hash_combine(seed, uint32_t(scCI.format), uint32_t(scCI.ycbcrModel), uint32_t(scCI.ycbcrRange),
                 uint32_t(scCI.components.r), uint32_t(scCI.components.g), uint32_t(scCI.components.b),
                 uint32_t(scCI.components.a), uint32_t(scCI.xChromaOffset), uint32_t(scCI.yChromaOffset),
                 uint32_t(scCI.chromaFilter), scCI.forceExplicitReconstruction);
    return seed;
}

std::size_t calculateSamplerHash(const VkSamplerCreateInfo &sCI, const std::map<uint64_t, std::size_t> &objectHashes)
{
    std::size_t seed = 0;
    hashPNextChain(seed, sCI.pNext, objectHashes);
    hash_combine(seed, uint32_t(sCI.flags), uint32_t(sCI.magFilter), uint32_t(sCI.minFilter), uint32_t(sCI.mipmapMode),
                 uint32_t(sCI.addressModeU), uint32_t(sCI.addressModeV), uint32_t(sCI.addressModeW), sCI.mipLodBias,
                 sCI.anisotropyEnable, sCI.maxAnisotropy, sCI.compareEnable, uint32_t(sCI.compareOp), sCI.minLod,
                 sCI.maxLod, uint32_t(sCI.borderColor), sCI.unnormalizedCoordinates);
    return seed;
}

std::size_t calculateDescriptorSetLayoutHash(const VkDescriptorSetLayoutCreateInfo &sCI,
                                             const std::map<uint64_t, std::size_t> &objectHashes)
{
    std::size_t seed = 0;

    hashPNextChain(seed, sCI.pNext, objectHashes);

    hash_combine(seed, uint32_t(sCI.flags));
    if (sCI.pBindings != DE_NULL)
    {
        for (uint32_t i = 0; i < sCI.bindingCount; ++i)
        {
            hash_combine(seed, sCI.pBindings[i].binding, uint32_t(sCI.pBindings[i].descriptorType),
                         sCI.pBindings[i].descriptorCount, uint32_t(sCI.pBindings[i].stageFlags));
            if (sCI.pBindings[i].pImmutableSamplers != DE_NULL)
            {
                for (uint32_t j = 0; j < sCI.pBindings[i].descriptorCount; ++j)
                {
                    auto it = objectHashes.find(sCI.pBindings[i].pImmutableSamplers[j].getInternal());
                    if (it != end(objectHashes))
                        hash_combine(seed, it->second);
                }
            }
        }
    }
    return seed;
}

std::size_t calculatePipelineLayoutHash(const VkPipelineLayoutCreateInfo &pCI,
                                        const std::map<uint64_t, std::size_t> &objectHashes)
{
    std::size_t seed = 0;

    hashPNextChain(seed, pCI.pNext, objectHashes);

    hash_combine(seed, uint32_t(pCI.flags));

    if (pCI.pSetLayouts != DE_NULL)
    {
        for (uint32_t i = 0; i < pCI.setLayoutCount; ++i)
        {
            auto it = objectHashes.find(pCI.pSetLayouts[i].getInternal());
            if (it != end(objectHashes))
                hash_combine(seed, it->second);
        }
    }
    if (pCI.pPushConstantRanges != DE_NULL)
    {
        for (uint32_t i = 0; i < pCI.pushConstantRangeCount; ++i)
        {
            hash_combine(seed, uint32_t(pCI.pPushConstantRanges[i].stageFlags));
            hash_combine(seed, pCI.pPushConstantRanges[i].offset);
            hash_combine(seed, pCI.pPushConstantRanges[i].size);
        }
    }

    return seed;
}

std::size_t calculateShaderModuleHash(const VkShaderModuleCreateInfo &sCI,
                                      const std::map<uint64_t, std::size_t> &objectHashes)
{
    std::size_t seed = 0;

    hashPNextChain(seed, sCI.pNext, objectHashes);

    hash_combine(seed, uint32_t(sCI.flags));
    hash_combine(seed, MemoryArea(sCI.pCode, sCI.codeSize));

    return seed;
}

std::size_t calculateRenderPassHash(const VkRenderPassCreateInfo &rCI,
                                    const std::map<uint64_t, std::size_t> &objectHashes)
{
    std::size_t seed = 0;

    hashPNextChain(seed, rCI.pNext, objectHashes);

    hash_combine(seed, uint32_t(rCI.flags));

    if (rCI.pAttachments != DE_NULL)
        for (uint32_t i = 0; i < rCI.attachmentCount; ++i)
            hash_combine(seed, uint32_t(rCI.pAttachments[i].flags), uint32_t(rCI.pAttachments[i].format),
                         uint32_t(rCI.pAttachments[i].samples), uint32_t(rCI.pAttachments[i].loadOp),
                         uint32_t(rCI.pAttachments[i].storeOp), uint32_t(rCI.pAttachments[i].stencilLoadOp),
                         uint32_t(rCI.pAttachments[i].stencilStoreOp), uint32_t(rCI.pAttachments[i].initialLayout),
                         uint32_t(rCI.pAttachments[i].finalLayout));

    if (rCI.pSubpasses != DE_NULL)
    {
        for (uint32_t i = 0; i < rCI.subpassCount; ++i)
        {
            hash_combine(seed, uint32_t(rCI.pSubpasses[i].flags), uint32_t(rCI.pSubpasses[i].pipelineBindPoint));
            if (rCI.pSubpasses[i].pInputAttachments != DE_NULL)
                for (uint32_t j = 0; j < rCI.pSubpasses[i].inputAttachmentCount; ++j)
                    hash_combine(seed, rCI.pSubpasses[i].pInputAttachments[j].attachment,
                                 uint32_t(rCI.pSubpasses[i].pInputAttachments[j].layout));
            if (rCI.pSubpasses[i].pColorAttachments != DE_NULL)
                for (uint32_t j = 0; j < rCI.pSubpasses[i].colorAttachmentCount; ++j)
                    hash_combine(seed, rCI.pSubpasses[i].pColorAttachments[j].attachment,
                                 uint32_t(rCI.pSubpasses[i].pColorAttachments[j].layout));
            if (rCI.pSubpasses[i].pResolveAttachments != DE_NULL)
                for (uint32_t j = 0; j < rCI.pSubpasses[i].colorAttachmentCount; ++j)
                    hash_combine(seed, rCI.pSubpasses[i].pResolveAttachments[j].attachment,
                                 uint32_t(rCI.pSubpasses[i].pResolveAttachments[j].layout));
            if (rCI.pSubpasses[i].pDepthStencilAttachment != DE_NULL)
                hash_combine(seed, rCI.pSubpasses[i].pDepthStencilAttachment->attachment,
                             uint32_t(rCI.pSubpasses[i].pDepthStencilAttachment->layout));
            if (rCI.pSubpasses[i].pPreserveAttachments != DE_NULL)
                for (uint32_t j = 0; j < rCI.pSubpasses[i].preserveAttachmentCount; ++j)
                    hash_combine(seed, rCI.pSubpasses[i].pPreserveAttachments[j]);
        }
    }
    if (rCI.pDependencies != DE_NULL)
        for (uint32_t i = 0; i < rCI.dependencyCount; ++i)
            hash_combine(seed, rCI.pDependencies[i].srcSubpass, rCI.pDependencies[i].dstSubpass,
                         uint32_t(rCI.pDependencies[i].srcStageMask), uint32_t(rCI.pDependencies[i].dstStageMask),
                         uint64_t(rCI.pDependencies[i].srcAccessMask), uint64_t(rCI.pDependencies[i].dstAccessMask),
                         uint32_t(rCI.pDependencies[i].dependencyFlags));

    return seed;
}

std::size_t calculateRenderPass2Hash(const VkRenderPassCreateInfo2 &rCI,
                                     const std::map<uint64_t, std::size_t> &objectHashes)
{
    std::size_t seed = 0;

    hashPNextChain(seed, rCI.pNext, objectHashes);

    hash_combine(seed, rCI.flags);

    if (rCI.pAttachments != DE_NULL)
        for (uint32_t i = 0; i < rCI.attachmentCount; ++i)
            hash_combine(seed, uint32_t(rCI.pAttachments[i].flags), uint32_t(rCI.pAttachments[i].format),
                         uint32_t(rCI.pAttachments[i].samples), uint32_t(rCI.pAttachments[i].loadOp),
                         uint32_t(rCI.pAttachments[i].storeOp), uint32_t(rCI.pAttachments[i].stencilLoadOp),
                         uint32_t(rCI.pAttachments[i].stencilStoreOp), uint32_t(rCI.pAttachments[i].initialLayout),
                         uint32_t(rCI.pAttachments[i].finalLayout));

    if (rCI.pSubpasses != DE_NULL)
    {
        for (uint32_t i = 0; i < rCI.subpassCount; ++i)
        {
            hash_combine(seed, uint32_t(rCI.pSubpasses[i].flags), uint32_t(rCI.pSubpasses[i].pipelineBindPoint));
            if (rCI.pSubpasses[i].pInputAttachments != DE_NULL)
                for (uint32_t j = 0; j < rCI.pSubpasses[i].inputAttachmentCount; ++j)
                    hash_combine(seed, rCI.pSubpasses[i].pInputAttachments[j].attachment,
                                 uint32_t(rCI.pSubpasses[i].pInputAttachments[j].layout));
            if (rCI.pSubpasses[i].pColorAttachments != DE_NULL)
                for (uint32_t j = 0; j < rCI.pSubpasses[i].colorAttachmentCount; ++j)
                    hash_combine(seed, rCI.pSubpasses[i].pColorAttachments[j].attachment,
                                 uint32_t(rCI.pSubpasses[i].pColorAttachments[j].layout));
            if (rCI.pSubpasses[i].pResolveAttachments != DE_NULL)
                for (uint32_t j = 0; j < rCI.pSubpasses[i].colorAttachmentCount; ++j)
                    hash_combine(seed, rCI.pSubpasses[i].pResolveAttachments[j].attachment,
                                 uint32_t(rCI.pSubpasses[i].pResolveAttachments[j].layout));
            if (rCI.pSubpasses[i].pDepthStencilAttachment != DE_NULL)
                hash_combine(seed, rCI.pSubpasses[i].pDepthStencilAttachment->attachment,
                             uint32_t(rCI.pSubpasses[i].pDepthStencilAttachment->layout));
            if (rCI.pSubpasses[i].pPreserveAttachments != DE_NULL)
                for (uint32_t j = 0; j < rCI.pSubpasses[i].preserveAttachmentCount; ++j)
                    hash_combine(seed, rCI.pSubpasses[i].pPreserveAttachments[j]);
        }
    }
    if (rCI.pDependencies != DE_NULL)
        for (uint32_t i = 0; i < rCI.dependencyCount; ++i)
            hash_combine(seed, rCI.pDependencies[i].srcSubpass, rCI.pDependencies[i].dstSubpass,
                         uint32_t(rCI.pDependencies[i].srcStageMask), uint32_t(rCI.pDependencies[i].dstStageMask),
                         uint64_t(rCI.pDependencies[i].srcAccessMask), uint64_t(rCI.pDependencies[i].dstAccessMask),
                         uint32_t(rCI.pDependencies[i].dependencyFlags));

    if (rCI.pCorrelatedViewMasks != DE_NULL)
        for (uint32_t i = 0; i < rCI.correlatedViewMaskCount; ++i)
            hash_combine(seed, rCI.pCorrelatedViewMasks[i]);

    return seed;
}

VkGraphicsPipelineCreateInfo prepareSimpleGraphicsPipelineCI(
    VkPipelineVertexInputStateCreateInfo &vertexInputStateCreateInfo,
    std::vector<VkPipelineShaderStageCreateInfo> &shaderStageCreateInfos,
    VkPipelineInputAssemblyStateCreateInfo &inputAssemblyStateCreateInfo,
    VkPipelineViewportStateCreateInfo &viewPortStateCreateInfo,
    VkPipelineRasterizationStateCreateInfo &rasterizationStateCreateInfo,
    VkPipelineMultisampleStateCreateInfo &multisampleStateCreateInfo,
    VkPipelineColorBlendAttachmentState &colorBlendAttachmentState,
    VkPipelineColorBlendStateCreateInfo &colorBlendStateCreateInfo,
    VkPipelineDynamicStateCreateInfo &dynamicStateCreateInfo, std::vector<VkDynamicState> &dynamicStates,
    VkPipelineLayout pipelineLayout, VkRenderPass renderPass)
{
    vertexInputStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType                             sType;
        DE_NULL,                                                   // const void*                                 pNext;
        (VkPipelineVertexInputStateCreateFlags)0,                  // VkPipelineVertexInputStateCreateFlags       flags;
        0u,      // uint32_t                                    vertexBindingDescriptionCount;
        DE_NULL, // const VkVertexInputBindingDescription*      pVertexBindingDescriptions;
        0u,      // uint32_t                                    vertexAttributeDescriptionCount;
        DE_NULL  // const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions;
    };

    inputAssemblyStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, // VkStructureType                            sType;
        DE_NULL,                                    // const void*                                pNext;
        (VkPipelineInputAssemblyStateCreateFlags)0, // VkPipelineInputAssemblyStateCreateFlags    flags;
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,       // VkPrimitiveTopology                        topology;
        VK_FALSE // VkBool32                                   primitiveRestartEnable;
    };

    viewPortStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, // VkStructureType                       sType;
        DE_NULL,                                               // const void*                           pNext;
        (VkPipelineViewportStateCreateFlags)0,                 // VkPipelineViewportStateCreateFlags    flags;
        1,                                                     // uint32_t                              viewportCount;
        DE_NULL,                                               // const VkViewport*                     pViewports;
        1,                                                     // uint32_t                              scissorCount;
        DE_NULL                                                // const VkRect2D*                       pScissors;
    };

    rasterizationStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, // VkStructureType                            sType;
        DE_NULL,                                                    // const void*                                pNext;
        (VkPipelineRasterizationStateCreateFlags)0,                 // VkPipelineRasterizationStateCreateFlags    flags;
        VK_FALSE,                // VkBool32                                   depthClampEnable;
        VK_FALSE,                // VkBool32                                   rasterizerDiscardEnable;
        VK_POLYGON_MODE_FILL,    // VkPolygonMode                              polygonMode;
        VK_CULL_MODE_BACK_BIT,   // VkCullModeFlags                            cullMode;
        VK_FRONT_FACE_CLOCKWISE, // VkFrontFace                                frontFace;
        VK_FALSE,                // VkBool32                                   depthBiasEnable;
        0.0f,                    // float                                      depthBiasConstantFactor;
        0.0f,                    // float                                      depthBiasClamp;
        0.0f,                    // float                                      depthBiasSlopeFactor;
        1.0f                     // float                                      lineWidth;
    };

    multisampleStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType                          sType;
        DE_NULL,                                                  // const void*                              pNext;
        (VkPipelineMultisampleStateCreateFlags)0,                 // VkPipelineMultisampleStateCreateFlags    flags;
        VK_SAMPLE_COUNT_1_BIT, // VkSampleCountFlagBits                    rasterizationSamples;
        VK_FALSE,              // VkBool32                                 sampleShadingEnable;
        0.0f,                  // float                                    minSampleShading;
        DE_NULL,               // const VkSampleMask*                      pSampleMask;
        VK_FALSE,              // VkBool32                                 alphaToCoverageEnable;
        VK_FALSE               // VkBool32                                 alphaToOneEnable;
    };

    colorBlendAttachmentState = {
        VK_FALSE,                   // VkBool32                 blendEnable;
        VK_BLEND_FACTOR_ZERO,       // VkBlendFactor            srcColorBlendFactor;
        VK_BLEND_FACTOR_ZERO,       // VkBlendFactor            dstColorBlendFactor;
        VK_BLEND_OP_ADD,            // VkBlendOp                colorBlendOp;
        VK_BLEND_FACTOR_ZERO,       // VkBlendFactor            srcAlphaBlendFactor;
        VK_BLEND_FACTOR_ZERO,       // VkBlendFactor            dstAlphaBlendFactor;
        VK_BLEND_OP_ADD,            // VkBlendOp                alphaBlendOp;
        (VkColorComponentFlags)0xFu // VkColorComponentFlags    colorWriteMask;
    };

    colorBlendStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType                               sType;
        DE_NULL,                                 // const void*                                   pNext;
        (VkPipelineColorBlendStateCreateFlags)0, // VkPipelineColorBlendStateCreateFlags          flags;
        false,                                   // VkBool32                                      logicOpEnable;
        VK_LOGIC_OP_CLEAR,                       // VkLogicOp                                     logicOp;
        1,                                       // uint32_t                                      attachmentCount;
        &colorBlendAttachmentState,              // const VkPipelineColorBlendAttachmentState*    pAttachments;
        {1.0f, 1.0f, 1.0f, 1.0f}                 // float                                         blendConstants[4];
    };

    dynamicStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, // VkStructureType                      sType;
        DE_NULL,                                              // const void*                          pNext;
        (VkPipelineDynamicStateCreateFlags)0u,                // VkPipelineDynamicStateCreateFlags    flags;
        uint32_t(dynamicStates.size()),                       // uint32_t                             dynamicStateCount;
        dynamicStates.data()                                  // const VkDynamicState*                pDynamicStates;
    };

    VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, // VkStructureType                                  sType;
        DE_NULL,                                         // const void*                                      pNext;
        (VkPipelineCreateFlags)0,                        // VkPipelineCreateFlags                            flags;
        uint32_t(shaderStageCreateInfos.size()),         // uint32_t                                         stageCount;
        shaderStageCreateInfos.data(),                   // const VkPipelineShaderStageCreateInfo*           pStages;
        &vertexInputStateCreateInfo,   // const VkPipelineVertexInputStateCreateInfo*      pVertexInputState;
        &inputAssemblyStateCreateInfo, // const VkPipelineInputAssemblyStateCreateInfo*    pInputAssemblyState;
        DE_NULL,                       // const VkPipelineTessellationStateCreateInfo*     pTessellationState;
        &viewPortStateCreateInfo,      // const VkPipelineViewportStateCreateInfo*         pViewportState;
        &rasterizationStateCreateInfo, // const VkPipelineRasterizationStateCreateInfo*    pRasterizationState;
        &multisampleStateCreateInfo,   // const VkPipelineMultisampleStateCreateInfo*      pMultisampleState;
        DE_NULL,                       // const VkPipelineDepthStencilStateCreateInfo*     pDepthStencilState;
        &colorBlendStateCreateInfo,    // const VkPipelineColorBlendStateCreateInfo*       pColorBlendState;
        &dynamicStateCreateInfo,       // const VkPipelineDynamicStateCreateInfo*          pDynamicState;
        pipelineLayout,                // VkPipelineLayout                                 layout;
        renderPass,                    // VkRenderPass                                     renderPass;
        0u,                            // uint32_t                                         subpass;
        VK_NULL_HANDLE,                // VkPipeline                                       basePipelineHandle;
        0                              // int                                              basePipelineIndex;
    };

    return graphicsPipelineCreateInfo;
}

VkComputePipelineCreateInfo prepareSimpleComputePipelineCI(const VkPipelineShaderStageCreateInfo &shaderStageCreateInfo,
                                                           VkPipelineLayout pipelineLayout)
{
    const VkComputePipelineCreateInfo pipelineCreateInfo = {
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, // VkStructureType                    sType
        DE_NULL,                                        // const void*                        pNext
        0u,                                             // VkPipelineCreateFlags            flags
        shaderStageCreateInfo,                          // VkPipelineShaderStageCreateInfo    stage
        pipelineLayout,                                 // VkPipelineLayout                    layout
        VK_NULL_HANDLE,                                 // VkPipeline                        basePipelineHandle
        0u,                                             // int32_t                            basePipelineIndex
    };
    return pipelineCreateInfo;
}

VkRenderPassCreateInfo prepareSimpleRenderPassCI(VkFormat format, VkAttachmentDescription &attachmentDescription,
                                                 VkAttachmentReference &attachmentReference,
                                                 VkSubpassDescription &subpassDescription)
{
    attachmentDescription = {
        (VkAttachmentDescriptionFlags)0u,        // VkAttachmentDescriptionFlags    flags;
        format,                                  // VkFormat                        format;
        VK_SAMPLE_COUNT_1_BIT,                   // VkSampleCountFlagBits           samples;
        VK_ATTACHMENT_LOAD_OP_CLEAR,             // VkAttachmentLoadOp              loadOp;
        VK_ATTACHMENT_STORE_OP_STORE,            // VkAttachmentStoreOp             storeOp;
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,         // VkAttachmentLoadOp              stencilLoadOp;
        VK_ATTACHMENT_STORE_OP_DONT_CARE,        // VkAttachmentStoreOp             stencilStoreOp;
        VK_IMAGE_LAYOUT_UNDEFINED,               // VkImageLayout                   initialLayout;
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // VkImageLayout                   finalLayout;
    };

    attachmentReference = {
        0u,                                      // uint32_t attachment;
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // VkImageLayout layout;
    };

    subpassDescription = {
        (VkSubpassDescriptionFlags)0u,   // VkSubpassDescriptionFlags       flags;
        VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint             pipelineBindPoint
        0u,                              // uint32_t                        inputAttachmentCount
        DE_NULL,                         // const VkAttachmentReference*    pInputAttachments
        1u,                              // uint32_t                        colorAttachmentCount
        &attachmentReference,            // const VkAttachmentReference*    pColorAttachments
        DE_NULL,                         // const VkAttachmentReference*    pResolveAttachments
        DE_NULL,                         // const VkAttachmentReference*    pDepthStencilAttachment
        0u,                              // uint32_t                        preserveAttachmentCount
        DE_NULL                          // const uint32_t*                 pPreserveAttachments
    };

    const VkRenderPassCreateInfo renderPassCreateInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // VkStructureType                   sType;
        DE_NULL,                                   // const void*                       pNext;
        (VkRenderPassCreateFlags)0u,               // VkRenderPassCreateFlags           flags;
        1u,                                        // uint32_t                          attachmentCount
        &attachmentDescription,                    // const VkAttachmentDescription*    pAttachments
        1u,                                        // uint32_t                          subpassCount
        &subpassDescription,                       // const VkSubpassDescription*       pSubpasses
        0u,                                        // uint32_t                          dependencyCount
        DE_NULL                                    // const VkSubpassDependency*        pDependencies
    };

    return renderPassCreateInfo;
}

VkFormat getRenderTargetFormat(const InstanceInterface &vk, const VkPhysicalDevice &device)
{
    const VkFormatFeatureFlags featureFlags = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
    VkFormatProperties formatProperties;
    vk.getPhysicalDeviceFormatProperties(device, VK_FORMAT_B8G8R8A8_UNORM, &formatProperties);
    if ((formatProperties.linearTilingFeatures & featureFlags) ||
        (formatProperties.optimalTilingFeatures & featureFlags))
        return VK_FORMAT_B8G8R8A8_UNORM;
    vk.getPhysicalDeviceFormatProperties(device, VK_FORMAT_R8G8B8A8_UNORM, &formatProperties);
    if ((formatProperties.linearTilingFeatures & featureFlags) ||
        formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)
        return VK_FORMAT_R8G8B8A8_UNORM;
    TCU_THROW(NotSupportedError, "Device does not support VK_FORMAT_B8G8R8A8_UNORM nor VK_FORMAT_R8G8B8A8_UNORM");
    return VK_FORMAT_UNDEFINED;
}

} // namespace vk

#else
DE_EMPTY_CPP_FILE
#endif // CTS_USES_VULKANSC
