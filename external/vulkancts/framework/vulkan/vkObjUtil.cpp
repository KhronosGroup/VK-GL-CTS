/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2018 Google Inc.
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
 * \brief Utilities for creating commonly used Vulkan objects
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vkRefUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkQueryUtil.hpp"

#include "tcuVector.hpp"

#include "deSTLUtil.hpp"

#include <algorithm>

namespace vk
{

Move<VkPipeline> makeComputePipeline(const DeviceInterface &vk, const VkDevice device,
                                     const VkPipelineLayout pipelineLayout, const VkPipelineCreateFlags pipelineFlags,
                                     const void *pipelinePNext, const VkShaderModule shaderModule,
                                     const VkPipelineShaderStageCreateFlags shaderFlags,
                                     const VkSpecializationInfo *specializationInfo,
                                     const VkPipelineCache pipelineCache, const uint32_t subgroupSize)
{
    const VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT subgroupSizeCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO_EXT, // VkStructureType sType;
        nullptr,                                                                        // void* pNext;
        subgroupSize // uint32_t requiredSubgroupSize;
    };
    const VkPipelineShaderStageCreateInfo pipelineShaderStageParams = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,   // VkStructureType sType;
        subgroupSize != 0 ? &subgroupSizeCreateInfo : nullptr, // const void* pNext;
        shaderFlags,                                           // VkPipelineShaderStageCreateFlags flags;
        VK_SHADER_STAGE_COMPUTE_BIT,                           // VkShaderStageFlagBits stage;
        shaderModule,                                          // VkShaderModule module;
        "main",                                                // const char* pName;
        specializationInfo,                                    // const VkSpecializationInfo* pSpecializationInfo;
    };
    const VkComputePipelineCreateInfo pipelineCreateInfo = {
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, // VkStructureType sType;
        pipelinePNext,                                  // const void* pNext;
        pipelineFlags,                                  // VkPipelineCreateFlags flags;
        pipelineShaderStageParams,                      // VkPipelineShaderStageCreateInfo stage;
        pipelineLayout,                                 // VkPipelineLayout layout;
        VK_NULL_HANDLE,                                 // VkPipeline basePipelineHandle;
        0,                                              // int32_t basePipelineIndex;
    };
    return createComputePipeline(vk, device, pipelineCache, &pipelineCreateInfo);
}

Move<VkPipeline> makeComputePipeline(const DeviceInterface &vk, const VkDevice device,
                                     const VkPipelineLayout pipelineLayout, const VkShaderModule shaderModule)
{
    return makeComputePipeline(vk, device, pipelineLayout, static_cast<VkPipelineCreateFlags>(0u), nullptr,
                               shaderModule, static_cast<VkPipelineShaderStageCreateFlags>(0u), nullptr);
}

Move<VkPipeline> makeGraphicsPipeline(
    const DeviceInterface &vk, const VkDevice device, const VkPipelineLayout pipelineLayout,
    const VkShaderModule vertexShaderModule, const VkShaderModule tessellationControlShaderModule,
    const VkShaderModule tessellationEvalShaderModule, const VkShaderModule geometryShaderModule,
    const VkShaderModule fragmentShaderModule, const VkRenderPass renderPass, const std::vector<VkViewport> &viewports,
    const std::vector<VkRect2D> &scissors, const VkPrimitiveTopology topology, const uint32_t subpass,
    const uint32_t patchControlPoints, const VkPipelineVertexInputStateCreateInfo *vertexInputStateCreateInfo,
    const VkPipelineRasterizationStateCreateInfo *rasterizationStateCreateInfo,
    const VkPipelineMultisampleStateCreateInfo *multisampleStateCreateInfo,
    const VkPipelineDepthStencilStateCreateInfo *depthStencilStateCreateInfo,
    const VkPipelineColorBlendStateCreateInfo *colorBlendStateCreateInfo,
    const VkPipelineDynamicStateCreateInfo *dynamicStateCreateInfo, const void *pNext,
    const VkPipelineCreateFlags pipelineCreateFlags)
{
    const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, // VkStructureType                            sType
        nullptr,                                                     // const void*                                pNext
        0u,                                                          // VkPipelineInputAssemblyStateCreateFlags    flags
        topology, // VkPrimitiveTopology                        topology
        VK_FALSE  // VkBool32                                   primitiveRestartEnable
    };

    const VkPipelineTessellationStateCreateInfo tessStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO, // VkStructureType                           sType
        nullptr,                                                   // const void*                               pNext
        0u,                                                        // VkPipelineTessellationStateCreateFlags    flags
        patchControlPoints // uint32_t                                  patchControlPoints
    };

    const VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, // VkStructureType                             sType
        nullptr,                                               // const void*                                 pNext
        (VkPipelineViewportStateCreateFlags)0,                 // VkPipelineViewportStateCreateFlags          flags
        viewports.empty() ? 1u :
                            (uint32_t)viewports.size(),     // uint32_t                                    viewportCount
        viewports.empty() ? nullptr : &viewports[0],        // const VkViewport*                           pViewports
        viewports.empty() ? 1u : (uint32_t)scissors.size(), // uint32_t                                    scissorCount
        scissors.empty() ? nullptr : &scissors[0]           // const VkRect2D*                             pScissors
    };

    std::vector<VkDynamicState> dynamicStates;

    if (viewports.empty())
        dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
    if (scissors.empty())
        dynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);

    const VkPipelineDynamicStateCreateInfo dynamicStateCreateInfoDefault = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, // VkStructureType                      sType
        nullptr,                                              // const void*                          pNext
        0u,                                                   // VkPipelineDynamicStateCreateFlags    flags
        (uint32_t)dynamicStates.size(),                       // uint32_t                             dynamicStateCount
        dynamicStates.empty() ? nullptr : &dynamicStates[0]   // const VkDynamicState*                pDynamicStates
    };

    const VkPipelineDynamicStateCreateInfo *dynamicStateCreateInfoDefaultPtr =
        dynamicStates.empty() ? nullptr : &dynamicStateCreateInfoDefault;

    return makeGraphicsPipeline(
        vk, device, pipelineLayout, vertexShaderModule, tessellationControlShaderModule, tessellationEvalShaderModule,
        geometryShaderModule, fragmentShaderModule, renderPass, subpass, vertexInputStateCreateInfo,
        &inputAssemblyStateCreateInfo, &tessStateCreateInfo, &viewportStateCreateInfo, rasterizationStateCreateInfo,
        multisampleStateCreateInfo, depthStencilStateCreateInfo, colorBlendStateCreateInfo,
        dynamicStateCreateInfo ? dynamicStateCreateInfo : dynamicStateCreateInfoDefaultPtr, pNext, pipelineCreateFlags);
}

Move<VkPipeline> makeGraphicsPipeline(const DeviceInterface &vk, const VkDevice device,
                                      const VkPipelineLayout pipelineLayout, const VkShaderModule vertexShaderModule,
                                      const VkShaderModule tessellationControlShaderModule,
                                      const VkShaderModule tessellationEvalShaderModule,
                                      const VkShaderModule geometryShaderModule,
                                      const VkShaderModule fragmentShaderModule, const VkRenderPass renderPass,
                                      const uint32_t subpass,
                                      const VkPipelineVertexInputStateCreateInfo *vertexInputStateCreateInfo,
                                      const VkPipelineInputAssemblyStateCreateInfo *inputAssemblyStateCreateInfo,
                                      const VkPipelineTessellationStateCreateInfo *tessStateCreateInfo,
                                      const VkPipelineViewportStateCreateInfo *viewportStateCreateInfo,
                                      const VkPipelineRasterizationStateCreateInfo *rasterizationStateCreateInfo,
                                      const VkPipelineMultisampleStateCreateInfo *multisampleStateCreateInfo,
                                      const VkPipelineDepthStencilStateCreateInfo *depthStencilStateCreateInfo,
                                      const VkPipelineColorBlendStateCreateInfo *colorBlendStateCreateInfo,
                                      const VkPipelineDynamicStateCreateInfo *dynamicStateCreateInfo, const void *pNext,
                                      const VkPipelineCreateFlags pipelineCreateFlags)
{
    DE_ASSERT(tessStateCreateInfo ||
              (tessellationControlShaderModule == VK_NULL_HANDLE && tessellationEvalShaderModule == VK_NULL_HANDLE));

    const VkBool32 disableRasterization = (fragmentShaderModule == VK_NULL_HANDLE);

    VkPipelineShaderStageCreateInfo stageCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType                     sType
        nullptr,                                             // const void*                         pNext
        0u,                                                  // VkPipelineShaderStageCreateFlags    flags
        VK_SHADER_STAGE_VERTEX_BIT,                          // VkShaderStageFlagBits               stage
        VK_NULL_HANDLE,                                      // VkShaderModule                      module
        "main",                                              // const char*                         pName
        nullptr                                              // const VkSpecializationInfo*         pSpecializationInfo
    };

    std::vector<VkPipelineShaderStageCreateInfo> pipelineShaderStageParams;

    {
        stageCreateInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
        stageCreateInfo.module = vertexShaderModule;
        pipelineShaderStageParams.push_back(stageCreateInfo);
    }

    if (tessellationControlShaderModule != VK_NULL_HANDLE)
    {
        stageCreateInfo.stage  = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        stageCreateInfo.module = tessellationControlShaderModule;
        pipelineShaderStageParams.push_back(stageCreateInfo);
    }

    if (tessellationEvalShaderModule != VK_NULL_HANDLE)
    {
        stageCreateInfo.stage  = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        stageCreateInfo.module = tessellationEvalShaderModule;
        pipelineShaderStageParams.push_back(stageCreateInfo);
    }

    if (geometryShaderModule != VK_NULL_HANDLE)
    {
        stageCreateInfo.stage  = VK_SHADER_STAGE_GEOMETRY_BIT;
        stageCreateInfo.module = geometryShaderModule;
        pipelineShaderStageParams.push_back(stageCreateInfo);
    }

    if (fragmentShaderModule != VK_NULL_HANDLE)
    {
        stageCreateInfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        stageCreateInfo.module = fragmentShaderModule;
        pipelineShaderStageParams.push_back(stageCreateInfo);
    }

    const VkVertexInputBindingDescription vertexInputBindingDescription = {
        0u,                          // uint32_t             binding
        sizeof(tcu::Vec4),           // uint32_t             stride
        VK_VERTEX_INPUT_RATE_VERTEX, // VkVertexInputRate    inputRate
    };

    const VkVertexInputAttributeDescription vertexInputAttributeDescription = {
        0u,                            // uint32_t    location
        0u,                            // uint32_t    binding
        VK_FORMAT_R32G32B32A32_SFLOAT, // VkFormat    format
        0u                             // uint32_t    offset
    };

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfoDefault = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType                             sType
        nullptr,                                                   // const void*                                 pNext
        (VkPipelineVertexInputStateCreateFlags)0,                  // VkPipelineVertexInputStateCreateFlags       flags
        1u,                              // uint32_t                                    vertexBindingDescriptionCount
        &vertexInputBindingDescription,  // const VkVertexInputBindingDescription*      pVertexBindingDescriptions
        1u,                              // uint32_t                                    vertexAttributeDescriptionCount
        &vertexInputAttributeDescription // const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions
    };

    const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfoDefault = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, // VkStructureType                            sType
        nullptr,                                                     // const void*                                pNext
        0u,                                                          // VkPipelineInputAssemblyStateCreateFlags    flags
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // VkPrimitiveTopology                        topology
        VK_FALSE                             // VkBool32                                   primitiveRestartEnable
    };

    const VkViewport viewport = makeViewport(256, 256);
    const VkRect2D scissor    = makeRect2D(256, 256);

    const VkPipelineViewportStateCreateInfo viewportStateCreateInfoDefault = {
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, // VkStructureType                             sType
        nullptr,                                               // const void*                                 pNext
        (VkPipelineViewportStateCreateFlags)0,                 // VkPipelineViewportStateCreateFlags          flags
        1u,        // uint32_t                                    viewportCount
        &viewport, // const VkViewport*                           pViewports
        1u,        // uint32_t                                    scissorCount
        &scissor   // const VkRect2D*                             pScissors
    };

    const VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfoDefault = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, // VkStructureType                            sType
        nullptr,                                                    // const void*                                pNext
        0u,                                                         // VkPipelineRasterizationStateCreateFlags    flags
        VK_FALSE,                        // VkBool32                                   depthClampEnable
        disableRasterization,            // VkBool32                                   rasterizerDiscardEnable
        VK_POLYGON_MODE_FILL,            // VkPolygonMode                              polygonMode
        VK_CULL_MODE_NONE,               // VkCullModeFlags                            cullMode
        VK_FRONT_FACE_COUNTER_CLOCKWISE, // VkFrontFace                                frontFace
        VK_FALSE,                        // VkBool32                                   depthBiasEnable
        0.0f,                            // float                                      depthBiasConstantFactor
        0.0f,                            // float                                      depthBiasClamp
        0.0f,                            // float                                      depthBiasSlopeFactor
        1.0f                             // float                                      lineWidth
    };

    const VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfoDefault = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType                          sType
        nullptr,                                                  // const void*                              pNext
        0u,                                                       // VkPipelineMultisampleStateCreateFlags    flags
        VK_SAMPLE_COUNT_1_BIT, // VkSampleCountFlagBits                    rasterizationSamples
        VK_FALSE,              // VkBool32                                 sampleShadingEnable
        1.0f,                  // float                                    minSampleShading
        nullptr,               // const VkSampleMask*                      pSampleMask
        VK_FALSE,              // VkBool32                                 alphaToCoverageEnable
        VK_FALSE               // VkBool32                                 alphaToOneEnable
    };

    const VkStencilOpState stencilOpState = {
        VK_STENCIL_OP_KEEP,  // VkStencilOp    failOp
        VK_STENCIL_OP_KEEP,  // VkStencilOp    passOp
        VK_STENCIL_OP_KEEP,  // VkStencilOp    depthFailOp
        VK_COMPARE_OP_NEVER, // VkCompareOp    compareOp
        0,                   // uint32_t       compareMask
        0,                   // uint32_t       writeMask
        0                    // uint32_t       reference
    };

    const VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfoDefault = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, // VkStructureType                          sType
        nullptr,                                                    // const void*                              pNext
        0u,                                                         // VkPipelineDepthStencilStateCreateFlags   flags
        VK_FALSE,                    // VkBool32                                 depthTestEnable
        VK_FALSE,                    // VkBool32                                 depthWriteEnable
        VK_COMPARE_OP_LESS_OR_EQUAL, // VkCompareOp                              depthCompareOp
        VK_FALSE,                    // VkBool32                                 depthBoundsTestEnable
        VK_FALSE,                    // VkBool32                                 stencilTestEnable
        stencilOpState,              // VkStencilOpState                         front
        stencilOpState,              // VkStencilOpState                         back
        0.0f,                        // float                                    minDepthBounds
        1.0f,                        // float                                    maxDepthBounds
    };

    const VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {
        VK_FALSE,                // VkBool32                 blendEnable
        VK_BLEND_FACTOR_ZERO,    // VkBlendFactor            srcColorBlendFactor
        VK_BLEND_FACTOR_ZERO,    // VkBlendFactor            dstColorBlendFactor
        VK_BLEND_OP_ADD,         // VkBlendOp                colorBlendOp
        VK_BLEND_FACTOR_ZERO,    // VkBlendFactor            srcAlphaBlendFactor
        VK_BLEND_FACTOR_ZERO,    // VkBlendFactor            dstAlphaBlendFactor
        VK_BLEND_OP_ADD,         // VkBlendOp                alphaBlendOp
        VK_COLOR_COMPONENT_R_BIT // VkColorComponentFlags    colorWriteMask
            | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};

    const VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfoDefault = {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType                               sType
        nullptr,                                                  // const void*                                   pNext
        0u,                                                       // VkPipelineColorBlendStateCreateFlags          flags
        VK_FALSE,                   // VkBool32                                      logicOpEnable
        VK_LOGIC_OP_CLEAR,          // VkLogicOp                                     logicOp
        1u,                         // uint32_t                                      attachmentCount
        &colorBlendAttachmentState, // const VkPipelineColorBlendAttachmentState*    pAttachments
        {0.0f, 0.0f, 0.0f, 0.0f}    // float                                         blendConstants[4]
    };

    const VkGraphicsPipelineCreateInfo pipelineCreateInfo = {
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, // VkStructureType                                  sType
        pNext,                                           // const void*                                      pNext
        pipelineCreateFlags,                             // VkPipelineCreateFlags                            flags
        (uint32_t)pipelineShaderStageParams.size(),      // uint32_t                                         stageCount
        &pipelineShaderStageParams[0],                   // const VkPipelineShaderStageCreateInfo*           pStages
        vertexInputStateCreateInfo ?
            vertexInputStateCreateInfo :
            &vertexInputStateCreateInfoDefault, // const VkPipelineVertexInputStateCreateInfo*      pVertexInputState
        inputAssemblyStateCreateInfo ?
            inputAssemblyStateCreateInfo :
            &inputAssemblyStateCreateInfoDefault, // const VkPipelineInputAssemblyStateCreateInfo*    pInputAssemblyState
        tessStateCreateInfo,                      // const VkPipelineTessellationStateCreateInfo*     pTessellationState
        viewportStateCreateInfo ?
            viewportStateCreateInfo :
            &viewportStateCreateInfoDefault, // const VkPipelineViewportStateCreateInfo*         pViewportState
        rasterizationStateCreateInfo ?
            rasterizationStateCreateInfo :
            &rasterizationStateCreateInfoDefault, // const VkPipelineRasterizationStateCreateInfo*    pRasterizationState
        multisampleStateCreateInfo ?
            multisampleStateCreateInfo :
            &multisampleStateCreateInfoDefault, // const VkPipelineMultisampleStateCreateInfo*      pMultisampleState
        depthStencilStateCreateInfo ?
            depthStencilStateCreateInfo :
            &depthStencilStateCreateInfoDefault, // const VkPipelineDepthStencilStateCreateInfo*     pDepthStencilState
        colorBlendStateCreateInfo ?
            colorBlendStateCreateInfo :
            &colorBlendStateCreateInfoDefault, // const VkPipelineColorBlendStateCreateInfo*       pColorBlendState
        dynamicStateCreateInfo ? dynamicStateCreateInfo :
                                 nullptr, // const VkPipelineDynamicStateCreateInfo*          pDynamicState
        pipelineLayout,                   // VkPipelineLayout                                 layout
        renderPass,                       // VkRenderPass                                     renderPass
        subpass,                          // uint32_t                                         subpass
        VK_NULL_HANDLE,                   // VkPipeline                                       basePipelineHandle
        0                                 // int32_t                                          basePipelineIndex;
    };

    return createGraphicsPipeline(vk, device, VK_NULL_HANDLE, &pipelineCreateInfo);
}

#ifndef CTS_USES_VULKANSC
Move<VkPipeline> makeGraphicsPipeline(const DeviceInterface &vk, const VkDevice device,
                                      const VkPipelineLayout pipelineLayout, const VkShaderModule taskShaderModule,
                                      const VkShaderModule meshShaderModule, const VkShaderModule fragmentShaderModule,
                                      const VkRenderPass renderPass, const std::vector<VkViewport> &viewports,
                                      const std::vector<VkRect2D> &scissors, const uint32_t subpass,
                                      const VkPipelineRasterizationStateCreateInfo *rasterizationStateCreateInfo,
                                      const VkPipelineMultisampleStateCreateInfo *multisampleStateCreateInfo,
                                      const VkPipelineDepthStencilStateCreateInfo *depthStencilStateCreateInfo,
                                      const VkPipelineColorBlendStateCreateInfo *colorBlendStateCreateInfo,
                                      const VkPipelineDynamicStateCreateInfo *dynamicStateCreateInfo,
                                      const VkPipelineCreateFlags pipelineCreateFlags, const void *pNext)
{
    VkPipelineShaderStageCreateInfo stageCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType                     sType
        nullptr,                                             // const void*                         pNext
        0u,                                                  // VkPipelineShaderStageCreateFlags    flags
        VK_SHADER_STAGE_VERTEX_BIT,                          // VkShaderStageFlagBits               stage
        VK_NULL_HANDLE,                                      // VkShaderModule                      module
        "main",                                              // const char*                         pName
        nullptr                                              // const VkSpecializationInfo*         pSpecializationInfo
    };

    std::vector<VkPipelineShaderStageCreateInfo> pipelineShaderStageParams;

    if (taskShaderModule != VK_NULL_HANDLE)
    {
        stageCreateInfo.stage  = VK_SHADER_STAGE_TASK_BIT_EXT;
        stageCreateInfo.module = taskShaderModule;
        pipelineShaderStageParams.push_back(stageCreateInfo);
    }

    {
        stageCreateInfo.stage  = VK_SHADER_STAGE_MESH_BIT_EXT;
        stageCreateInfo.module = meshShaderModule;
        pipelineShaderStageParams.push_back(stageCreateInfo);
    }

    if (fragmentShaderModule != VK_NULL_HANDLE)
    {
        stageCreateInfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        stageCreateInfo.module = fragmentShaderModule;
        pipelineShaderStageParams.push_back(stageCreateInfo);
    }

    return makeGraphicsPipeline(vk, device, VK_NULL_HANDLE, pipelineLayout, pipelineCreateFlags,
                                pipelineShaderStageParams, renderPass, viewports, scissors, subpass,
                                rasterizationStateCreateInfo, multisampleStateCreateInfo, depthStencilStateCreateInfo,
                                colorBlendStateCreateInfo, dynamicStateCreateInfo, pNext);
}
#endif // CTS_USES_VULKANSC

namespace
{

// Returns true if the shader stage create info structure contains information on the fragment shader.
// We could do this with a lambda but it's a bit more clear this way.
bool isFragShaderInfo(const VkPipelineShaderStageCreateInfo &shaderInfo)
{
    return (shaderInfo.stage == VK_SHADER_STAGE_FRAGMENT_BIT);
}

} // namespace

Move<VkPipeline> makeGraphicsPipeline(const DeviceInterface &vk, const VkDevice device,
                                      const VkPipeline basePipelineHandle, const VkPipelineLayout pipelineLayout,
                                      const VkPipelineCreateFlags pipelineCreateFlags,
                                      const std::vector<VkPipelineShaderStageCreateInfo> &pipelineShaderStageParams,
                                      const VkRenderPass renderPass, const std::vector<VkViewport> &viewports,
                                      const std::vector<VkRect2D> &scissors, const uint32_t subpass,
                                      const VkPipelineRasterizationStateCreateInfo *rasterizationStateCreateInfo,
                                      const VkPipelineMultisampleStateCreateInfo *multisampleStateCreateInfo,
                                      const VkPipelineDepthStencilStateCreateInfo *depthStencilStateCreateInfo,
                                      const VkPipelineColorBlendStateCreateInfo *colorBlendStateCreateInfo,
                                      const VkPipelineDynamicStateCreateInfo *dynamicStateCreateInfo, const void *pNext,
                                      const VkPipelineVertexInputStateCreateInfo *vertexInputStateCreateInfo,
                                      const VkPipelineInputAssemblyStateCreateInfo *inputAssemblyStateCreateInfo)
{
    // Disable rasterization if no fragment shader info is found in pipelineShaderStageParams.
    const auto fragFound =
        std::any_of(begin(pipelineShaderStageParams), end(pipelineShaderStageParams), isFragShaderInfo);
    const VkBool32 disableRasterization = (!fragFound);

    const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfoDefault = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, // VkStructureType                            sType
        nullptr,                                                     // const void*                                pNext
        0u,                                                          // VkPipelineInputAssemblyStateCreateFlags    flags
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // VkPrimitiveTopology                        topology
        VK_FALSE                             // VkBool32                                   primitiveRestartEnable
    };
    const VkPipelineInputAssemblyStateCreateInfo *pInputAssemblyState =
        inputAssemblyStateCreateInfo ? inputAssemblyStateCreateInfo : &inputAssemblyStateCreateInfoDefault;
    if (dynamicStateCreateInfo)
    {
        for (uint32_t i = 0u; i < dynamicStateCreateInfo->dynamicStateCount; ++i)
        {
            if (VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY_EXT == dynamicStateCreateInfo->pDynamicStates[i])
            {
                pInputAssemblyState = nullptr;
                break;
            }
        }
    }

    const VkVertexInputBindingDescription vertexInputBindingDescription = {
        0u,                          // uint32_t             binding
        sizeof(tcu::Vec4),           // uint32_t             stride
        VK_VERTEX_INPUT_RATE_VERTEX, // VkVertexInputRate    inputRate
    };

    const VkVertexInputAttributeDescription vertexInputAttributeDescription = {
        0u,                            // uint32_t    location
        0u,                            // uint32_t    binding
        VK_FORMAT_R32G32B32A32_SFLOAT, // VkFormat    format
        0u                             // uint32_t    offset
    };

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfoDefault = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType                             sType
        nullptr,                                                   // const void*                                 pNext
        (VkPipelineVertexInputStateCreateFlags)0,                  // VkPipelineVertexInputStateCreateFlags       flags
        1u,                              // uint32_t                                    vertexBindingDescriptionCount
        &vertexInputBindingDescription,  // const VkVertexInputBindingDescription*      pVertexBindingDescriptions
        1u,                              // uint32_t                                    vertexAttributeDescriptionCount
        &vertexInputAttributeDescription // const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions
    };
    const VkPipelineVertexInputStateCreateInfo *pVertexInputState =
        vertexInputStateCreateInfo ? vertexInputStateCreateInfo : &vertexInputStateCreateInfoDefault;
    if (dynamicStateCreateInfo)
    {
        for (uint32_t i = 0u; i < dynamicStateCreateInfo->dynamicStateCount; ++i)
        {
            if (VK_DYNAMIC_STATE_VERTEX_INPUT_EXT == dynamicStateCreateInfo->pDynamicStates[i])
            {
                pInputAssemblyState = nullptr;
                break;
            }
        }
    }

    VkPipelineViewportStateCreateInfo viewportStateCreateInfo = initVulkanStructure();
    viewportStateCreateInfo.viewportCount                     = static_cast<uint32_t>(viewports.size());
    viewportStateCreateInfo.pViewports                        = de::dataOrNull(viewports);
    viewportStateCreateInfo.scissorCount                      = static_cast<uint32_t>(scissors.size());
    viewportStateCreateInfo.pScissors                         = de::dataOrNull(scissors);

    VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfoDefault = initVulkanStructure();
    rasterizationStateCreateInfoDefault.rasterizerDiscardEnable                = disableRasterization;
    rasterizationStateCreateInfoDefault.lineWidth                              = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfoDefault = initVulkanStructure();
    multisampleStateCreateInfoDefault.rasterizationSamples                 = VK_SAMPLE_COUNT_1_BIT;
    multisampleStateCreateInfoDefault.minSampleShading                     = 1.0f;

    VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfoDefault = initVulkanStructure();
    depthStencilStateCreateInfoDefault.maxDepthBounds                        = 1.0f;

    VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
    colorBlendAttachmentState.colorWriteMask =
        (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);

    VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfoDefault = initVulkanStructure();
    colorBlendStateCreateInfoDefault.attachmentCount                     = 1u;
    colorBlendStateCreateInfoDefault.pAttachments                        = &colorBlendAttachmentState;

    std::vector<VkDynamicState> dynamicStates;

    if (viewports.empty())
        dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
    if (scissors.empty())
        dynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);

    VkPipelineDynamicStateCreateInfo dynamicStateCreateInfoDefault = initVulkanStructure();
    dynamicStateCreateInfoDefault.dynamicStateCount                = static_cast<uint32_t>(dynamicStates.size());
    dynamicStateCreateInfoDefault.pDynamicStates                   = de::dataOrNull(dynamicStates);

    const VkPipelineDynamicStateCreateInfo *dynamicStateCreateInfoDefaultPtr =
        dynamicStates.empty() ? nullptr : &dynamicStateCreateInfoDefault;

    const VkGraphicsPipelineCreateInfo pipelineCreateInfo = {
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, // VkStructureType                                  sType
        pNext,                                           // const void*                                      pNext
        pipelineCreateFlags,                             // VkPipelineCreateFlags                            flags
        static_cast<uint32_t>(
            pipelineShaderStageParams.size()),     // uint32_t                                         stageCount
        de::dataOrNull(pipelineShaderStageParams), // const VkPipelineShaderStageCreateInfo*           pStages
        pVertexInputState,                         // const VkPipelineVertexInputStateCreateInfo*      pVertexInputState
        pInputAssemblyState,      // const VkPipelineInputAssemblyStateCreateInfo*    pInputAssemblyState
        nullptr,                  // const VkPipelineTessellationStateCreateInfo*     pTessellationState
        &viewportStateCreateInfo, // const VkPipelineViewportStateCreateInfo*         pViewportState
        rasterizationStateCreateInfo ?
            rasterizationStateCreateInfo :
            &rasterizationStateCreateInfoDefault, // const VkPipelineRasterizationStateCreateInfo*    pRasterizationState
        multisampleStateCreateInfo ?
            multisampleStateCreateInfo :
            &multisampleStateCreateInfoDefault, // const VkPipelineMultisampleStateCreateInfo*      pMultisampleState
        depthStencilStateCreateInfo ?
            depthStencilStateCreateInfo :
            &depthStencilStateCreateInfoDefault, // const VkPipelineDepthStencilStateCreateInfo*     pDepthStencilState
        colorBlendStateCreateInfo ?
            colorBlendStateCreateInfo :
            &colorBlendStateCreateInfoDefault, // const VkPipelineColorBlendStateCreateInfo*       pColorBlendState
        dynamicStateCreateInfo ?
            dynamicStateCreateInfo :
            dynamicStateCreateInfoDefaultPtr, // const VkPipelineDynamicStateCreateInfo*          pDynamicState
        pipelineLayout,                       // VkPipelineLayout                                 layout
        renderPass,                           // VkRenderPass                                     renderPass
        subpass,                              // uint32_t                                         subpass
        basePipelineHandle,                   // VkPipeline                                       basePipelineHandle
        -1                                    // int32_t                                          basePipelineIndex;
    };

    return createGraphicsPipeline(vk, device, VK_NULL_HANDLE, &pipelineCreateInfo);
}

Move<VkRenderPass> makeRenderPass(const DeviceInterface &vk, const VkDevice device, const VkFormat colorFormat,
                                  const VkFormat depthStencilFormat, const VkAttachmentLoadOp loadOperation,
                                  const VkImageLayout finalLayoutColor, const VkImageLayout finalLayoutDepthStencil,
                                  const VkImageLayout subpassLayoutColor, const VkImageLayout subpassLayoutDepthStencil,
                                  const VkAllocationCallbacks *const allocationCallbacks, const void *pNext)
{
    const bool hasColor                           = colorFormat != VK_FORMAT_UNDEFINED;
    const bool hasDepthStencil                    = depthStencilFormat != VK_FORMAT_UNDEFINED;
    const VkImageLayout initialLayoutColor        = loadOperation == VK_ATTACHMENT_LOAD_OP_LOAD ?
                                                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL :
                                                        VK_IMAGE_LAYOUT_UNDEFINED;
    const VkImageLayout initialLayoutDepthStencil = loadOperation == VK_ATTACHMENT_LOAD_OP_LOAD ?
                                                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL :
                                                        VK_IMAGE_LAYOUT_UNDEFINED;

    const VkAttachmentDescription colorAttachmentDescription = {
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

    const VkAttachmentDescription depthStencilAttachmentDescription = {
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

    std::vector<VkAttachmentDescription> attachmentDescriptions;

    if (hasColor)
        attachmentDescriptions.push_back(colorAttachmentDescription);
    if (hasDepthStencil)
        attachmentDescriptions.push_back(depthStencilAttachmentDescription);

    const VkAttachmentReference colorAttachmentRef = {
        0u,                // uint32_t         attachment
        subpassLayoutColor // VkImageLayout    layout
    };

    const VkAttachmentReference depthStencilAttachmentRef = {
        hasColor ? 1u : 0u,       // uint32_t         attachment
        subpassLayoutDepthStencil // VkImageLayout    layout
    };

    const VkSubpassDescription subpassDescription = {
        (VkSubpassDescriptionFlags)0,             // VkSubpassDescriptionFlags       flags
        VK_PIPELINE_BIND_POINT_GRAPHICS,          // VkPipelineBindPoint             pipelineBindPoint
        0u,                                       // uint32_t                        inputAttachmentCount
        nullptr,                                  // const VkAttachmentReference*    pInputAttachments
        hasColor ? 1u : 0u,                       // uint32_t                        colorAttachmentCount
        hasColor ? &colorAttachmentRef : nullptr, // const VkAttachmentReference*    pColorAttachments
        nullptr,                                  // const VkAttachmentReference*    pResolveAttachments
        hasDepthStencil ? &depthStencilAttachmentRef :
                          nullptr, // const VkAttachmentReference*    pDepthStencilAttachment
        0u,                        // uint32_t                        preserveAttachmentCount
        nullptr                    // const uint32_t*                 pPreserveAttachments
    };

    const VkRenderPassCreateInfo renderPassInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // VkStructureType                   sType
        pNext,                                     // const void*                       pNext
        (VkRenderPassCreateFlags)0,                // VkRenderPassCreateFlags           flags
        (uint32_t)attachmentDescriptions.size(),   // uint32_t                          attachmentCount
        attachmentDescriptions.size() > 0 ? &attachmentDescriptions[0] :
                                            nullptr, // const VkAttachmentDescription*    pAttachments
        1u,                                          // uint32_t                          subpassCount
        &subpassDescription,                         // const VkSubpassDescription*       pSubpasses
        0u,                                          // uint32_t                          dependencyCount
        nullptr                                      // const VkSubpassDependency*        pDependencies
    };

    return createRenderPass(vk, device, &renderPassInfo, allocationCallbacks);
}

Move<VkImageView> makeImageView(const DeviceInterface &vk, const VkDevice vkDevice, const VkImage image,
                                const VkImageViewType imageViewType, const VkFormat format,
                                const VkImageSubresourceRange subresourceRange,
                                const VkImageViewUsageCreateInfo *imageUsageCreateInfo)
{
    const VkImageViewCreateInfo imageViewParams = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
        imageUsageCreateInfo,                     // const void* pNext;
        0u,                                       // VkImageViewCreateFlags flags;
        image,                                    // VkImage image;
        imageViewType,                            // VkImageViewType viewType;
        format,                                   // VkFormat format;
        makeComponentMappingRGBA(),               // VkComponentMapping components;
        subresourceRange,                         // VkImageSubresourceRange subresourceRange;
    };
    return createImageView(vk, vkDevice, &imageViewParams);
}

Move<VkBufferView> makeBufferView(const DeviceInterface &vk, const VkDevice vkDevice, const VkBuffer buffer,
                                  const VkFormat format, const VkDeviceSize offset, const VkDeviceSize size)
{
    const VkBufferViewCreateInfo bufferViewParams = {
        VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO, // VkStructureType sType;
        nullptr,                                   // const void* pNext;
        0u,                                        // VkBufferViewCreateFlags flags;
        buffer,                                    // VkBuffer buffer;
        format,                                    // VkFormat format;
        offset,                                    // VkDeviceSize offset;
        size,                                      // VkDeviceSize range;
    };
    return createBufferView(vk, vkDevice, &bufferViewParams);
}

Move<VkDescriptorSet> makeDescriptorSet(const DeviceInterface &vk, const VkDevice device,
                                        const VkDescriptorPool descriptorPool, const VkDescriptorSetLayout setLayout,
                                        const void *pNext)
{
    const VkDescriptorSetAllocateInfo allocateParams = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // VkStructureType sType;
        pNext,                                          // const void* pNext;
        descriptorPool,                                 // VkDescriptorPool descriptorPool;
        1u,                                             // uint32_t setLayoutCount;
        &setLayout,                                     // const VkDescriptorSetLayout* pSetLayouts;
    };
    return allocateDescriptorSet(vk, device, &allocateParams);
}

VkBufferCreateInfo makeBufferCreateInfo(const VkDeviceSize size, const VkBufferUsageFlags usage)
{
    const VkBufferCreateInfo bufferCreateInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
        nullptr,                              // const void* pNext;
        (VkBufferCreateFlags)0,               // VkBufferCreateFlags flags;
        size,                                 // VkDeviceSize size;
        usage,                                // VkBufferUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
        0u,                                   // uint32_t queueFamilyIndexCount;
        nullptr,                              // const uint32_t* pQueueFamilyIndices;
    };
    return bufferCreateInfo;
}

VkBufferCreateInfo makeBufferCreateInfo(const VkDeviceSize size, const VkBufferUsageFlags usage,
                                        const std::vector<uint32_t> &queueFamilyIndices,
                                        const VkBufferCreateFlags createFlags, const void *pNext)
{
    const uint32_t queueFamilyIndexCount      = static_cast<uint32_t>(queueFamilyIndices.size());
    const uint32_t *pQueueFamilyIndices       = de::dataOrNull(queueFamilyIndices);
    const VkBufferCreateInfo bufferCreateInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
        pNext,                                // const void* pNext;
        createFlags,                          // VkBufferCreateFlags flags;
        size,                                 // VkDeviceSize size;
        usage,                                // VkBufferUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
        queueFamilyIndexCount,                // uint32_t queueFamilyIndexCount;
        pQueueFamilyIndices,                  // const uint32_t* pQueueFamilyIndices;
    };

    return bufferCreateInfo;
}

Move<VkPipelineLayout> makePipelineLayout(const DeviceInterface &vk, const VkDevice device,
                                          const VkDescriptorSetLayout descriptorSetLayout,
                                          const VkPushConstantRange *pushConstantRange)
{
    const uint32_t layoutCount = ((descriptorSetLayout == VK_NULL_HANDLE) ? 0u : 1u);
    const uint32_t rangeCount  = ((pushConstantRange == nullptr) ? 0u : 1u);
    return makePipelineLayout(vk, device, layoutCount, &descriptorSetLayout, rangeCount, pushConstantRange);
}

Move<VkPipelineLayout> makePipelineLayout(const DeviceInterface &vk, const VkDevice device,
                                          const std::vector<VkDescriptorSetLayout> &descriptorSetLayouts)
{
    const uint32_t setLayoutCount = descriptorSetLayouts.empty() ? 0u : (uint32_t)descriptorSetLayouts.size();
    const VkDescriptorSetLayout *descriptorSetLayout =
        descriptorSetLayouts.empty() ? nullptr : descriptorSetLayouts.data();

    return makePipelineLayout(vk, device, setLayoutCount, descriptorSetLayout);
}

Move<VkPipelineLayout> makePipelineLayout(const DeviceInterface &vk, const VkDevice device,
                                          const std::vector<vk::Move<VkDescriptorSetLayout>> &descriptorSetLayouts)
{
    // Create a list of descriptor sets without move pointers.
    std::vector<vk::VkDescriptorSetLayout> descriptorSetLayoutsUnWrapped;
    for (const auto &descriptorSetLayout : descriptorSetLayouts)
    {
        descriptorSetLayoutsUnWrapped.push_back(descriptorSetLayout.get());
    }
    return vk::makePipelineLayout(vk, device, static_cast<uint32_t>(descriptorSetLayoutsUnWrapped.size()),
                                  descriptorSetLayoutsUnWrapped.data());
}

Move<VkPipelineLayout> makePipelineLayout(const DeviceInterface &vk, const VkDevice device,
                                          const uint32_t setLayoutCount,
                                          const VkDescriptorSetLayout *descriptorSetLayout,
                                          const VkPipelineLayoutCreateFlags flags)
{
    const VkPipelineLayoutCreateInfo pipelineLayoutParams = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
        nullptr,                                       // const void* pNext;
        flags,                                         // VkPipelineLayoutCreateFlags flags;
        setLayoutCount,                                // uint32_t setLayoutCount;
        descriptorSetLayout,                           // const VkDescriptorSetLayout* pSetLayouts;
        0u,                                            // uint32_t pushConstantRangeCount;
        nullptr,                                       // const VkPushConstantRange* pPushConstantRanges;
    };

    return createPipelineLayout(vk, device, &pipelineLayoutParams);
}

Move<VkPipelineLayout> makePipelineLayout(const DeviceInterface &vk, const VkDevice device,
                                          const uint32_t setLayoutCount,
                                          const VkDescriptorSetLayout *descriptorSetLayout,
                                          const uint32_t pushConstantRangeCount,
                                          const VkPushConstantRange *pPushConstantRanges,
                                          const VkPipelineLayoutCreateFlags flags)
{
    const VkPipelineLayoutCreateInfo pipelineLayoutParams = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
        nullptr,                                       // const void* pNext;
        flags,                                         // VkPipelineLayoutCreateFlags flags;
        setLayoutCount,                                // uint32_t setLayoutCount;
        descriptorSetLayout,                           // const VkDescriptorSetLayout* pSetLayouts;
        pushConstantRangeCount,                        // uint32_t pushConstantRangeCount;
        pPushConstantRanges,                           // const VkPushConstantRange* pPushConstantRanges;
    };

    return createPipelineLayout(vk, device, &pipelineLayoutParams);
}

Move<VkFramebuffer> makeFramebuffer(const DeviceInterface &vk, const VkDevice device, const VkRenderPass renderPass,
                                    const VkImageView colorAttachment, const uint32_t width, const uint32_t height,
                                    const uint32_t layers)
{
    return makeFramebuffer(vk, device, renderPass, 1u, &colorAttachment, width, height, layers);
}

Move<VkFramebuffer> makeFramebuffer(const DeviceInterface &vk, const VkDevice device, const VkRenderPass renderPass,
                                    const uint32_t attachmentCount, const VkImageView *attachmentsArray,
                                    const uint32_t width, const uint32_t height, const uint32_t layers)
{
    const VkFramebufferCreateInfo framebufferInfo = {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // VkStructureType sType;
        nullptr,                                   // const void* pNext;
        (VkFramebufferCreateFlags)0,               // VkFramebufferCreateFlags flags;
        renderPass,                                // VkRenderPass renderPass;
        attachmentCount,                           // uint32_t attachmentCount;
        attachmentsArray,                          // const VkImageView* pAttachments;
        width,                                     // uint32_t width;
        height,                                    // uint32_t height;
        layers,                                    // uint32_t layers;
    };

    return createFramebuffer(vk, device, &framebufferInfo);
}

Move<VkCommandPool> makeCommandPool(const DeviceInterface &vk, const VkDevice device, const uint32_t queueFamilyIndex)
{
    const VkCommandPoolCreateInfo commandPoolParams = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,      // VkStructureType sType;
        nullptr,                                         // const void* pNext;
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // VkCommandPoolCreateFlags flags;
        queueFamilyIndex,                                // uint32_t queueFamilyIndex;
    };

    return createCommandPool(vk, device, &commandPoolParams);
}

VkBufferImageCopy makeBufferImageCopy(const VkExtent3D extent, const VkImageSubresourceLayers subresourceLayers)
{
    const VkBufferImageCopy copyParams = {
        0ull,                  // VkDeviceSize bufferOffset;
        0u,                    // uint32_t bufferRowLength;
        0u,                    // uint32_t bufferImageHeight;
        subresourceLayers,     // VkImageSubresourceLayers imageSubresource;
        makeOffset3D(0, 0, 0), // VkOffset3D imageOffset;
        extent,                // VkExtent3D imageExtent;
    };
    return copyParams;
}

CommandPoolWithBuffer::CommandPoolWithBuffer(const DeviceInterface &vkd, const VkDevice device,
                                             const uint32_t queueFamilyIndex)
{
    cmdPool   = makeCommandPool(vkd, device, queueFamilyIndex);
    cmdBuffer = allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
}

} // namespace vk
