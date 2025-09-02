/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
 * Copyright (c) 2016 Google Inc.
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
 * \brief Utility for generating simple work
 *//*--------------------------------------------------------------------*/

#include "vktShaderObjectUtil.hpp"

namespace vkt
{
namespace shaderobjutil
{

using namespace de;
using namespace tcu;
using namespace vk;

#ifndef CTS_USES_VULKANSC

std::vector<std::string> getDeviceCreationExtensions(Context &context)
{
    const auto &extList = context.getDeviceCreationExtensions();
    std::vector<std::string> ret(begin(extList), end(extList));
    return ret;
}

VkVertexInputBindingDescription2EXT makeVertexInputBindingDescription2(
    const VkVertexInputBindingDescription &description)
{
    const VkVertexInputBindingDescription2EXT desc2 = {
        VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT, // VkStructureType sType;
        nullptr,                                                  // void* pNext;
        description.binding,                                      // uint32_t binding;
        description.stride,                                       // uint32_t stride;
        description.inputRate,                                    // VkVertexInputRate inputRate;
        1u,                                                       // uint32_t divisor;
    };
    return desc2;
}

VkVertexInputAttributeDescription2EXT makeVertexInputAttributeDescription2(
    const VkVertexInputAttributeDescription &description)
{
    const VkVertexInputAttributeDescription2EXT desc2 = {
        VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT, // VkStructureType sType;
        nullptr,                                                    // void* pNext;
        description.location,                                       // uint32_t location;
        description.binding,                                        // uint32_t binding;
        description.format,                                         // VkFormat format;
        description.offset,                                         // uint32_t offset;
    };
    return desc2;
}

void bindShaderObjectState(const DeviceInterface &vkd, const std::vector<std::string> &deviceExtensions,
                           const VkCommandBuffer cmdBuffer, const std::vector<VkViewport> &viewports,
                           const std::vector<VkRect2D> &scissors, const VkPrimitiveTopology topology,
                           const uint32_t patchControlPoints,
                           const VkPipelineVertexInputStateCreateInfo *vertexInputStateCreateInfo,
                           const VkPipelineRasterizationStateCreateInfo *rasterizationStateCreateInfo,
                           const VkPipelineMultisampleStateCreateInfo *multisampleStateCreateInfo,
                           const VkPipelineDepthStencilStateCreateInfo *depthStencilStateCreateInfo,
                           const VkPipelineColorBlendStateCreateInfo *colorBlendStateCreateInfo)
{
    if (vertexInputStateCreateInfo)
    {
        // This is not used with mesh shaders.
        const auto &srcBindingDescs = vertexInputStateCreateInfo->pVertexBindingDescriptions;
        const auto &srcBindingCount = vertexInputStateCreateInfo->vertexBindingDescriptionCount;

        const auto &srcAttributeDescs = vertexInputStateCreateInfo->pVertexAttributeDescriptions;
        const auto &srcAttributeCount = vertexInputStateCreateInfo->vertexAttributeDescriptionCount;

        std::vector<VkVertexInputBindingDescription2EXT> bindingDescriptions;
        bindingDescriptions.reserve(srcBindingCount);
        std::transform(srcBindingDescs, srcBindingDescs + srcBindingCount, std::back_inserter(bindingDescriptions),
                       [](const VkVertexInputBindingDescription &description)
                       { return makeVertexInputBindingDescription2(description); });

        std::vector<VkVertexInputAttributeDescription2EXT> attributeDescriptions;
        attributeDescriptions.reserve(srcAttributeCount);
        std::transform(srcAttributeDescs, srcAttributeDescs + srcAttributeCount,
                       std::back_inserter(attributeDescriptions),
                       [](const VkVertexInputAttributeDescription &description)
                       { return makeVertexInputAttributeDescription2(description); });

        vkd.cmdSetVertexInputEXT(cmdBuffer, de::sizeU32(bindingDescriptions), de::dataOrNull(bindingDescriptions),
                                 de::sizeU32(attributeDescriptions), de::dataOrNull(attributeDescriptions));
    }

    if (vertexInputStateCreateInfo)
    {
        // This is not used with mesh shaders.
        vkd.cmdSetPrimitiveTopology(cmdBuffer, topology);
        vkd.cmdSetPrimitiveRestartEnable(cmdBuffer, VK_FALSE);

        if (patchControlPoints > 0u)
        {
            vkd.cmdSetPatchControlPointsEXT(cmdBuffer, patchControlPoints);
            vkd.cmdSetTessellationDomainOriginEXT(cmdBuffer, VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT);
        }
    }

    {
        vkd.cmdSetViewportWithCount(cmdBuffer, de::sizeU32(viewports), de::dataOrNull(viewports));
        vkd.cmdSetScissorWithCount(cmdBuffer, de::sizeU32(scissors), de::dataOrNull(scissors));
    }

    {
        const auto depthClampEnable =
            (rasterizationStateCreateInfo ? rasterizationStateCreateInfo->depthClampEnable : VK_FALSE);
        const auto rasterizerDiscardEnable =
            (rasterizationStateCreateInfo ? rasterizationStateCreateInfo->rasterizerDiscardEnable : VK_FALSE);
        const auto polygonMode =
            (rasterizationStateCreateInfo ? rasterizationStateCreateInfo->polygonMode : VK_POLYGON_MODE_FILL);
        const auto cullMode = (rasterizationStateCreateInfo ? rasterizationStateCreateInfo->cullMode :
                                                              static_cast<VkCullModeFlags>(VK_CULL_MODE_NONE));
        const auto frontFace =
            (rasterizationStateCreateInfo ? rasterizationStateCreateInfo->frontFace : VK_FRONT_FACE_COUNTER_CLOCKWISE);
        const auto depthBiasEnable =
            (rasterizationStateCreateInfo ? rasterizationStateCreateInfo->depthBiasEnable : VK_FALSE);
        const auto depthBiasConstantFactor =
            (rasterizationStateCreateInfo ? rasterizationStateCreateInfo->depthBiasConstantFactor : 0.0f);
        const auto depthBiasClamp =
            (rasterizationStateCreateInfo ? rasterizationStateCreateInfo->depthBiasClamp : 0.0f);
        const auto depthBiasSlopeFactor =
            (rasterizationStateCreateInfo ? rasterizationStateCreateInfo->depthBiasSlopeFactor : 0.0f);
        const auto lineWidth = (rasterizationStateCreateInfo ? rasterizationStateCreateInfo->lineWidth : 1.0f);

        vkd.cmdSetDepthClampEnableEXT(cmdBuffer, depthClampEnable);
        vkd.cmdSetRasterizerDiscardEnable(cmdBuffer, rasterizerDiscardEnable);
        vkd.cmdSetPolygonModeEXT(cmdBuffer, polygonMode);
        vkd.cmdSetCullMode(cmdBuffer, cullMode);
        vkd.cmdSetFrontFace(cmdBuffer, frontFace);
        vkd.cmdSetDepthBiasEnable(cmdBuffer, depthBiasEnable);
        vkd.cmdSetDepthBias(cmdBuffer, depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
        vkd.cmdSetLineWidth(cmdBuffer, lineWidth);
    }

    {
        const auto rasterizationSamples =
            (multisampleStateCreateInfo ? multisampleStateCreateInfo->rasterizationSamples : VK_SAMPLE_COUNT_1_BIT);
        const auto defaultSampleMask = 0xFFFFFFFFu;
        const auto pSampleMask =
            (multisampleStateCreateInfo ? multisampleStateCreateInfo->pSampleMask : &defaultSampleMask);
        const auto alphaToCoverageEnable =
            (multisampleStateCreateInfo ? multisampleStateCreateInfo->alphaToCoverageEnable : VK_FALSE);
        const auto alphaToOneEnable =
            (multisampleStateCreateInfo ? multisampleStateCreateInfo->alphaToOneEnable : VK_FALSE);

        vkd.cmdSetRasterizationSamplesEXT(cmdBuffer, rasterizationSamples);
        vkd.cmdSetSampleMaskEXT(cmdBuffer, rasterizationSamples, pSampleMask);
        vkd.cmdSetAlphaToCoverageEnableEXT(cmdBuffer, alphaToCoverageEnable);
        vkd.cmdSetAlphaToOneEnableEXT(cmdBuffer, alphaToOneEnable);
    }

    {
        const auto defaultStencilOp = makeStencilOpState(VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP,
                                                         VK_COMPARE_OP_NEVER, 0u, 0u, 0u);

        const auto depthTestEnable =
            (depthStencilStateCreateInfo ? depthStencilStateCreateInfo->depthTestEnable : VK_FALSE);
        const auto depthWriteEnable =
            (depthStencilStateCreateInfo ? depthStencilStateCreateInfo->depthWriteEnable : VK_FALSE);
        const auto depthCompareOp =
            (depthStencilStateCreateInfo ? depthStencilStateCreateInfo->depthCompareOp : VK_COMPARE_OP_NEVER);
        const auto depthBoundsTestEnable =
            (depthStencilStateCreateInfo ? depthStencilStateCreateInfo->depthBoundsTestEnable : VK_FALSE);
        const auto stencilTestEnable =
            (depthStencilStateCreateInfo ? depthStencilStateCreateInfo->stencilTestEnable : VK_FALSE);
        const auto stencilFront = (depthStencilStateCreateInfo ? depthStencilStateCreateInfo->front : defaultStencilOp);
        const auto stencilBack  = (depthStencilStateCreateInfo ? depthStencilStateCreateInfo->back : defaultStencilOp);
        const auto minDepthBounds = (depthStencilStateCreateInfo ? depthStencilStateCreateInfo->minDepthBounds : 0.0f);
        const auto maxDepthBounds = (depthStencilStateCreateInfo ? depthStencilStateCreateInfo->maxDepthBounds : 0.0f);

        vkd.cmdSetDepthTestEnable(cmdBuffer, depthTestEnable);
        vkd.cmdSetDepthWriteEnable(cmdBuffer, depthWriteEnable);
        vkd.cmdSetDepthCompareOp(cmdBuffer, depthCompareOp);
        vkd.cmdSetDepthBoundsTestEnable(cmdBuffer, depthBoundsTestEnable);
        vkd.cmdSetStencilTestEnable(cmdBuffer, stencilTestEnable);

        vkd.cmdSetStencilOp(cmdBuffer, VK_STENCIL_FACE_FRONT_BIT, stencilFront.failOp, stencilFront.passOp,
                            stencilFront.depthFailOp, stencilFront.compareOp);
        vkd.cmdSetStencilCompareMask(cmdBuffer, VK_STENCIL_FACE_FRONT_BIT, stencilFront.compareMask);
        vkd.cmdSetStencilWriteMask(cmdBuffer, VK_STENCIL_FACE_FRONT_BIT, stencilFront.writeMask);
        vkd.cmdSetStencilReference(cmdBuffer, VK_STENCIL_FACE_FRONT_BIT, stencilFront.reference);

        vkd.cmdSetStencilOp(cmdBuffer, VK_STENCIL_FACE_BACK_BIT, stencilBack.failOp, stencilBack.passOp,
                            stencilBack.depthFailOp, stencilBack.compareOp);
        vkd.cmdSetStencilCompareMask(cmdBuffer, VK_STENCIL_FACE_BACK_BIT, stencilBack.compareMask);
        vkd.cmdSetStencilWriteMask(cmdBuffer, VK_STENCIL_FACE_BACK_BIT, stencilBack.writeMask);
        vkd.cmdSetStencilReference(cmdBuffer, VK_STENCIL_FACE_BACK_BIT, stencilBack.reference);

        vkd.cmdSetDepthBounds(cmdBuffer, minDepthBounds, maxDepthBounds);
    }

    {
        const auto logicOpEnable = (colorBlendStateCreateInfo ? colorBlendStateCreateInfo->logicOpEnable : VK_FALSE);
        const auto logicOp       = (colorBlendStateCreateInfo ? colorBlendStateCreateInfo->logicOp : VK_LOGIC_OP_CLEAR);

        vkd.cmdSetLogicOpEnableEXT(cmdBuffer, logicOpEnable);
        vkd.cmdSetLogicOpEXT(cmdBuffer, logicOp);

        std::vector<VkBool32> colorWriteEnables;
        std::vector<VkColorComponentFlags> colorWriteMasks;
        std::vector<VkBool32> colorBlendEnables;
        std::vector<VkColorBlendEquationEXT> colorBlendEquations;

        if (!colorBlendStateCreateInfo)
        {
            const auto defaultWriteMask = (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);
            const auto defaultBlendEq =
                makeColorBlendEquationEXT(VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
                                          VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD);

            colorWriteEnables.push_back(VK_TRUE);
            colorWriteMasks.push_back(defaultWriteMask);
            colorBlendEnables.push_back(VK_FALSE);
            colorBlendEquations.push_back(defaultBlendEq);
        }
        else
        {
            const auto &attCount     = colorBlendStateCreateInfo->attachmentCount;
            const auto &pAttachments = colorBlendStateCreateInfo->pAttachments;

            colorWriteEnables.reserve(attCount);
            colorWriteMasks.reserve(attCount);
            colorBlendEnables.reserve(attCount);
            colorBlendEquations.reserve(attCount);

            colorWriteEnables = std::vector<VkBool32>(attCount, VK_TRUE);

            std::transform(pAttachments, pAttachments + attCount, std::back_inserter(colorWriteMasks),
                           [](const VkPipelineColorBlendAttachmentState &attState) { return attState.colorWriteMask; });

            std::transform(pAttachments, pAttachments + attCount, std::back_inserter(colorBlendEnables),
                           [](const VkPipelineColorBlendAttachmentState &attState) { return attState.blendEnable; });

            std::transform(pAttachments, pAttachments + attCount, std::back_inserter(colorBlendEquations),
                           [](const VkPipelineColorBlendAttachmentState &attState)
                           {
                               return makeColorBlendEquationEXT(
                                   attState.srcColorBlendFactor, attState.dstColorBlendFactor, attState.colorBlendOp,
                                   attState.srcAlphaBlendFactor, attState.dstAlphaBlendFactor, attState.alphaBlendOp);
                           });
        }

        vkd.cmdSetColorWriteEnableEXT(cmdBuffer, de::sizeU32(colorWriteEnables), de::dataOrNull(colorWriteEnables));
        vkd.cmdSetColorWriteMaskEXT(cmdBuffer, 0u, de::sizeU32(colorWriteMasks), de::dataOrNull(colorWriteMasks));
        vkd.cmdSetColorBlendEnableEXT(cmdBuffer, 0u, de::sizeU32(colorBlendEnables), de::dataOrNull(colorBlendEnables));
        vkd.cmdSetColorBlendEquationEXT(cmdBuffer, 0u, de::sizeU32(colorBlendEquations),
                                        de::dataOrNull(colorBlendEquations));
    }

    // Extra states with default values depending on enabled extensions.
    const auto extraDynStates = getShaderObjectDynamicStatesFromExtensions(deviceExtensions);
    for (const auto dynState : extraDynStates)
    {
        switch (dynState)
        {
        case VK_DYNAMIC_STATE_RASTERIZATION_STREAM_EXT:
            vkd.cmdSetRasterizationStreamEXT(cmdBuffer, 0u);
            break;
        case VK_DYNAMIC_STATE_COLOR_BLEND_ADVANCED_EXT:
            break;
        case VK_DYNAMIC_STATE_CONSERVATIVE_RASTERIZATION_MODE_EXT:
            vkd.cmdSetConservativeRasterizationModeEXT(cmdBuffer, VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT);
            break;
        case VK_DYNAMIC_STATE_COVERAGE_MODULATION_MODE_NV:
            vkd.cmdSetCoverageModulationModeNV(cmdBuffer, VK_COVERAGE_MODULATION_MODE_NONE_NV);
            break;
        case VK_DYNAMIC_STATE_COVERAGE_MODULATION_TABLE_ENABLE_NV:
            vkd.cmdSetCoverageModulationTableEnableNV(cmdBuffer, VK_FALSE);
            break;
        case VK_DYNAMIC_STATE_COVERAGE_MODULATION_TABLE_NV:
            // CoverageModulationTableEnable is false, so we can skip this.
            //vkd.cmdSetCoverageModulationTableNV(cmdBuffer, 0u, nullptr);
            break;
        case VK_DYNAMIC_STATE_COVERAGE_REDUCTION_MODE_NV:
            vkd.cmdSetCoverageReductionModeNV(cmdBuffer, VK_COVERAGE_REDUCTION_MODE_MERGE_NV);
            break;
        case VK_DYNAMIC_STATE_COVERAGE_TO_COLOR_ENABLE_NV:
            vkd.cmdSetCoverageToColorEnableNV(cmdBuffer, VK_FALSE);
            break;
        case VK_DYNAMIC_STATE_COVERAGE_TO_COLOR_LOCATION_NV:
            vkd.cmdSetCoverageToColorLocationNV(cmdBuffer, 0u);
            break;
        case VK_DYNAMIC_STATE_DEPTH_CLIP_ENABLE_EXT:
            vkd.cmdSetDepthClipEnableEXT(cmdBuffer, VK_FALSE);
            break;
        case VK_DYNAMIC_STATE_DEPTH_CLIP_NEGATIVE_ONE_TO_ONE_EXT:
            vkd.cmdSetDepthClipNegativeOneToOneEXT(cmdBuffer, VK_FALSE);
            break;
        case VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT:
            break;
        case VK_DYNAMIC_STATE_EXTRA_PRIMITIVE_OVERESTIMATION_SIZE_EXT:
            vkd.cmdSetExtraPrimitiveOverestimationSizeEXT(cmdBuffer, 0.0f);
            break;
        case VK_DYNAMIC_STATE_LINE_RASTERIZATION_MODE_EXT:
            vkd.cmdSetLineRasterizationModeEXT(cmdBuffer, VK_LINE_RASTERIZATION_MODE_DEFAULT_KHR);
            break;
        case VK_DYNAMIC_STATE_LINE_STIPPLE_ENABLE_EXT:
            vkd.cmdSetLineStippleEnableEXT(cmdBuffer, VK_FALSE);
            break;
        case VK_DYNAMIC_STATE_LINE_STIPPLE_EXT:
            break;
        case VK_DYNAMIC_STATE_PROVOKING_VERTEX_MODE_EXT:
            vkd.cmdSetProvokingVertexModeEXT(cmdBuffer, VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT);
            break;
        case VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR:
        {
            const auto fsrSize                                      = makeExtent2D(1u, 1u);
            const VkFragmentShadingRateCombinerOpKHR combinerOps[2] = {VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR,
                                                                       VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR};
            vkd.cmdSetFragmentShadingRateKHR(cmdBuffer, &fsrSize, combinerOps);
        }
        break;
        case VK_DYNAMIC_STATE_REPRESENTATIVE_FRAGMENT_TEST_ENABLE_NV:
            vkd.cmdSetRepresentativeFragmentTestEnableNV(cmdBuffer, VK_FALSE);
            break;
        case VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_ENABLE_EXT:
            vkd.cmdSetSampleLocationsEnableEXT(cmdBuffer, VK_FALSE);
            break;
        case VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_EXT:
            break;
        case VK_DYNAMIC_STATE_VIEWPORT_SWIZZLE_NV:
        {
            const VkViewportSwizzleNV defaultSwizzle{
                VK_VIEWPORT_COORDINATE_SWIZZLE_POSITIVE_X_NV,
                VK_VIEWPORT_COORDINATE_SWIZZLE_POSITIVE_Y_NV,
                VK_VIEWPORT_COORDINATE_SWIZZLE_POSITIVE_Z_NV,
                VK_VIEWPORT_COORDINATE_SWIZZLE_POSITIVE_W_NV,
            };
            const std::vector<vk::VkViewportSwizzleNV> idSwizzles(viewports.size(), defaultSwizzle);
            vkd.cmdSetViewportSwizzleNV(cmdBuffer, 0u, de::sizeU32(idSwizzles), de::dataOrNull(idSwizzles));
        }
        break;
        case VK_DYNAMIC_STATE_VIEWPORT_W_SCALING_ENABLE_NV:
            vkd.cmdSetViewportWScalingEnableNV(cmdBuffer, VK_FALSE);
            break;
        case VK_DYNAMIC_STATE_VIEWPORT_W_SCALING_NV:
            break;
        case VK_DYNAMIC_STATE_EXCLUSIVE_SCISSOR_ENABLE_NV:
        {
            const VkBool32 enable = VK_FALSE;
            vkd.cmdSetExclusiveScissorEnableNV(cmdBuffer, 0u, 1u, &enable);
        }
        break;
        case VK_DYNAMIC_STATE_EXCLUSIVE_SCISSOR_NV:
            break;
        case VK_DYNAMIC_STATE_DISCARD_RECTANGLE_ENABLE_EXT:
            vkd.cmdSetDiscardRectangleEnableEXT(cmdBuffer, VK_FALSE);
            break;
        case VK_DYNAMIC_STATE_DISCARD_RECTANGLE_EXT:
            break;
        case VK_DYNAMIC_STATE_DISCARD_RECTANGLE_MODE_EXT:
            break;
        case VK_DYNAMIC_STATE_ATTACHMENT_FEEDBACK_LOOP_ENABLE_EXT:
            vkd.cmdSetAttachmentFeedbackLoopEnableEXT(cmdBuffer, 0u);
            break;
        case VK_DYNAMIC_STATE_DEPTH_CLAMP_RANGE_EXT:
            vkd.cmdSetDepthClampRangeEXT(cmdBuffer, VK_DEPTH_CLAMP_MODE_VIEWPORT_RANGE_EXT, nullptr);
            break;
        default:
            DE_ASSERT(false);
            break;
        }
    }
}
#endif
} // namespace shaderobjutil
} // namespace vkt
