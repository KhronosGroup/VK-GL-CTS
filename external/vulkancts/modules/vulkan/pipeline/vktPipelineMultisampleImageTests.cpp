/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \brief Multisample image Tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineMultisampleImageTests.hpp"
#include "vktPipelineMakeUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktPipelineVertexUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkMemUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkPrograms.hpp"
#include "vkImageUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBufferWithMemory.hpp"

#include "tcuImageCompare.hpp"
#include "tcuTexture.hpp"
#include "tcuVectorType.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuRGBA.hpp"

#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"

#include <cmath>
#include <string>
#include <vector>

namespace vkt
{
namespace pipeline
{
namespace
{
using namespace vk;
using de::MovePtr;
using de::SharedPtr;
using de::UniquePtr;
using tcu::IVec2;
using tcu::IVec3;
using tcu::Vec4;

typedef SharedPtr<Unique<VkImageView>> ImageViewSp;
typedef SharedPtr<Unique<VkPipeline>> PipelineSp;

//! Test case parameters
struct CaseDef
{
    PipelineConstructionType pipelineConstructionType;
    IVec2 renderSize;
    int numLayers;
    VkFormat colorFormat;
    VkSampleCountFlagBits numSamples;
    bool colorSamples;
};

struct CaseDef3d
{
    PipelineConstructionType pipelineConstructionType;
    IVec3 renderSize;
    int numLayers;
    VkFormat colorFormat;
    VkSampleCountFlagBits numSamples;
};

template <typename T>
inline SharedPtr<Unique<T>> makeSharedPtr(Move<T> move)
{
    return SharedPtr<Unique<T>>(new Unique<T>(move));
}

template <typename T>
inline VkDeviceSize sizeInBytes(const std::vector<T> &vec)
{
    return vec.size() * sizeof(vec[0]);
}

//! Create a vector of derived pipelines, each with an increasing subpass index
std::vector<PipelineSp> makeGraphicsPipelines(const DeviceInterface &vk, const VkDevice device,
                                              const uint32_t numSubpasses, const VkPipelineLayout pipelineLayout,
                                              const VkRenderPass renderPass, const ShaderWrapper vertexModule,
                                              const ShaderWrapper fragmentModule, const IVec2 renderSize,
                                              const VkSampleCountFlagBits numSamples,
                                              const VkPrimitiveTopology topology)
{
    const VkVertexInputBindingDescription vertexInputBindingDescription = {
        0u,                          // uint32_t binding;
        sizeof(Vertex4RGBA),         // uint32_t stride;
        VK_VERTEX_INPUT_RATE_VERTEX, // VkVertexInputRate inputRate;
    };

    const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[] = {
        {
            0u,                            // uint32_t location;
            0u,                            // uint32_t binding;
            VK_FORMAT_R32G32B32A32_SFLOAT, // VkFormat format;
            0u,                            // uint32_t offset;
        },
        {
            1u,                            // uint32_t location;
            0u,                            // uint32_t binding;
            VK_FORMAT_R32G32B32A32_SFLOAT, // VkFormat format;
            sizeof(Vec4),                  // uint32_t offset;
        },
    };

    const VkPipelineVertexInputStateCreateInfo vertexInputStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType                             sType;
        nullptr,                                                   // const void*                                 pNext;
        (VkPipelineVertexInputStateCreateFlags)0,                  // VkPipelineVertexInputStateCreateFlags       flags;
        1u,                             // uint32_t                                    vertexBindingDescriptionCount;
        &vertexInputBindingDescription, // const VkVertexInputBindingDescription*      pVertexBindingDescriptions;
        DE_LENGTH_OF_ARRAY(
            vertexInputAttributeDescriptions), // uint32_t                                    vertexAttributeDescriptionCount;
        vertexInputAttributeDescriptions, // const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions;
    };

    const VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, // VkStructureType                             sType;
        nullptr,                                    // const void*                                 pNext;
        (VkPipelineInputAssemblyStateCreateFlags)0, // VkPipelineInputAssemblyStateCreateFlags     flags;
        topology,                                   // VkPrimitiveTopology                         topology;
        VK_FALSE, // VkBool32                                    primitiveRestartEnable;
    };

    const VkViewport viewport = makeViewport(renderSize);
    const VkRect2D scissor    = makeRect2D(renderSize);

    const VkPipelineViewportStateCreateInfo pipelineViewportStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, // VkStructureType                             sType;
        nullptr,                                               // const void*                                 pNext;
        (VkPipelineViewportStateCreateFlags)0,                 // VkPipelineViewportStateCreateFlags          flags;
        1u,        // uint32_t                                    viewportCount;
        &viewport, // const VkViewport*                           pViewports;
        1u,        // uint32_t                                    scissorCount;
        &scissor,  // const VkRect2D*                             pScissors;
    };

    const VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, // VkStructureType                          sType;
        nullptr,                                                    // const void*                              pNext;
        (VkPipelineRasterizationStateCreateFlags)0,                 // VkPipelineRasterizationStateCreateFlags  flags;
        VK_FALSE,                        // VkBool32                                 depthClampEnable;
        VK_FALSE,                        // VkBool32                                 rasterizerDiscardEnable;
        VK_POLYGON_MODE_FILL,            // VkPolygonMode polygonMode;
        VK_CULL_MODE_NONE,               // VkCullModeFlags cullMode;
        VK_FRONT_FACE_COUNTER_CLOCKWISE, // VkFrontFace frontFace;
        VK_FALSE,                        // VkBool32 depthBiasEnable;
        0.0f,                            // float depthBiasConstantFactor;
        0.0f,                            // float depthBiasClamp;
        0.0f,                            // float depthBiasSlopeFactor;
        1.0f,                            // float lineWidth;
    };

    const VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                  // const void* pNext;
        (VkPipelineMultisampleStateCreateFlags)0,                 // VkPipelineMultisampleStateCreateFlags flags;
        numSamples,                                               // VkSampleCountFlagBits rasterizationSamples;
        VK_FALSE,                                                 // VkBool32 sampleShadingEnable;
        0.0f,                                                     // float minSampleShading;
        nullptr,                                                  // const VkSampleMask* pSampleMask;
        VK_FALSE,                                                 // VkBool32 alphaToCoverageEnable;
        VK_FALSE                                                  // VkBool32 alphaToOneEnable;
    };

    const VkStencilOpState stencilOpState = makeStencilOpState(VK_STENCIL_OP_KEEP,   // stencil fail
                                                               VK_STENCIL_OP_KEEP,   // depth & stencil pass
                                                               VK_STENCIL_OP_KEEP,   // depth only fail
                                                               VK_COMPARE_OP_ALWAYS, // compare op
                                                               0u,                   // compare mask
                                                               0u,                   // write mask
                                                               0u);                  // reference

    VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                    // const void* pNext;
        (VkPipelineDepthStencilStateCreateFlags)0,                  // VkPipelineDepthStencilStateCreateFlags flags;
        VK_FALSE,                                                   // VkBool32 depthTestEnable;
        VK_FALSE,                                                   // VkBool32 depthWriteEnable;
        VK_COMPARE_OP_LESS,                                         // VkCompareOp depthCompareOp;
        VK_FALSE,                                                   // VkBool32 depthBoundsTestEnable;
        VK_FALSE,                                                   // VkBool32 stencilTestEnable;
        stencilOpState,                                             // VkStencilOpState front;
        stencilOpState,                                             // VkStencilOpState back;
        0.0f,                                                       // float minDepthBounds;
        1.0f,                                                       // float maxDepthBounds;
    };

    const VkColorComponentFlags colorComponentsAll =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    // Number of blend attachments must equal the number of color attachments during any subpass.
    const VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState = {
        VK_FALSE,             // VkBool32 blendEnable;
        VK_BLEND_FACTOR_ONE,  // VkBlendFactor srcColorBlendFactor;
        VK_BLEND_FACTOR_ZERO, // VkBlendFactor dstColorBlendFactor;
        VK_BLEND_OP_ADD,      // VkBlendOp colorBlendOp;
        VK_BLEND_FACTOR_ONE,  // VkBlendFactor srcAlphaBlendFactor;
        VK_BLEND_FACTOR_ZERO, // VkBlendFactor dstAlphaBlendFactor;
        VK_BLEND_OP_ADD,      // VkBlendOp alphaBlendOp;
        colorComponentsAll,   // VkColorComponentFlags colorWriteMask;
    };

    const VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                  // const void* pNext;
        (VkPipelineColorBlendStateCreateFlags)0,                  // VkPipelineColorBlendStateCreateFlags flags;
        VK_FALSE,                                                 // VkBool32 logicOpEnable;
        VK_LOGIC_OP_COPY,                                         // VkLogicOp logicOp;
        1u,                                                       // uint32_t attachmentCount;
        &pipelineColorBlendAttachmentState, // const VkPipelineColorBlendAttachmentState* pAttachments;
        {0.0f, 0.0f, 0.0f, 0.0f},           // float blendConstants[4];
    };

    const VkPipelineShaderStageCreateInfo pShaderStages[] = {
        {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                             // const void* pNext;
            (VkPipelineShaderStageCreateFlags)0,                 // VkPipelineShaderStageCreateFlags flags;
            VK_SHADER_STAGE_VERTEX_BIT,                          // VkShaderStageFlagBits stage;
            vertexModule.getModule(),                            // VkShaderModule module;
            "main",                                              // const char* pName;
            nullptr,                                             // const VkSpecializationInfo* pSpecializationInfo;
        },
        {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                             // const void* pNext;
            (VkPipelineShaderStageCreateFlags)0,                 // VkPipelineShaderStageCreateFlags flags;
            VK_SHADER_STAGE_FRAGMENT_BIT,                        // VkShaderStageFlagBits stage;
            fragmentModule.getModule(),                          // VkShaderModule module;
            "main",                                              // const char* pName;
            nullptr,                                             // const VkSpecializationInfo* pSpecializationInfo;
        }};

    DE_ASSERT(numSubpasses > 0u);

    std::vector<VkGraphicsPipelineCreateInfo> graphicsPipelineInfos(0);
    std::vector<VkPipeline> rawPipelines(numSubpasses, VK_NULL_HANDLE);

    {
#ifndef CTS_USES_VULKANSC
        const VkPipelineCreateFlags firstPipelineFlags =
            (numSubpasses > 1u ? VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT : VkPipelineCreateFlagBits(0));
#else
        const VkPipelineCreateFlags firstPipelineFlags = VkPipelineCreateFlagBits(0);
#endif // CTS_USES_VULKANSC

        VkGraphicsPipelineCreateInfo createInfo = {
            VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                         // const void* pNext;
            firstPipelineFlags,                              // VkPipelineCreateFlags flags;
            DE_LENGTH_OF_ARRAY(pShaderStages),               // uint32_t stageCount;
            pShaderStages,                                   // const VkPipelineShaderStageCreateInfo* pStages;
            &vertexInputStateInfo,           // const VkPipelineVertexInputStateCreateInfo* pVertexInputState;
            &pipelineInputAssemblyStateInfo, // const VkPipelineInputAssemblyStateCreateInfo* pInputAssemblyState;
            nullptr,                         // const VkPipelineTessellationStateCreateInfo* pTessellationState;
            &pipelineViewportStateInfo,      // const VkPipelineViewportStateCreateInfo* pViewportState;
            &pipelineRasterizationStateInfo, // const VkPipelineRasterizationStateCreateInfo* pRasterizationState;
            &pipelineMultisampleStateInfo,   // const VkPipelineMultisampleStateCreateInfo* pMultisampleState;
            &pipelineDepthStencilStateInfo,  // const VkPipelineDepthStencilStateCreateInfo* pDepthStencilState;
            &pipelineColorBlendStateInfo,    // const VkPipelineColorBlendStateCreateInfo* pColorBlendState;
            nullptr,                         // const VkPipelineDynamicStateCreateInfo* pDynamicState;
            pipelineLayout,                  // VkPipelineLayout layout;
            renderPass,                      // VkRenderPass renderPass;
            0u,                              // uint32_t subpass;
            VK_NULL_HANDLE,                  // VkPipeline basePipelineHandle;
            0u,                              // int32_t basePipelineIndex;
        };

        graphicsPipelineInfos.push_back(createInfo);

#ifndef CTS_USES_VULKANSC
        createInfo.flags             = VK_PIPELINE_CREATE_DERIVATIVE_BIT;
        createInfo.basePipelineIndex = 0;
#endif // CTS_USES_VULKANSC

        for (uint32_t subpassNdx = 1u; subpassNdx < numSubpasses; ++subpassNdx)
        {
            createInfo.subpass = subpassNdx;
            graphicsPipelineInfos.push_back(createInfo);
        }
    }

    VK_CHECK(vk.createGraphicsPipelines(device, VK_NULL_HANDLE, static_cast<uint32_t>(graphicsPipelineInfos.size()),
                                        &graphicsPipelineInfos[0], nullptr, &rawPipelines[0]));

    std::vector<PipelineSp> pipelines;

    for (std::vector<VkPipeline>::const_iterator it = rawPipelines.begin(); it != rawPipelines.end(); ++it)
        pipelines.push_back(
            makeSharedPtr(Move<VkPipeline>(check<VkPipeline>(*it), Deleter<VkPipeline>(vk, device, nullptr))));

    return pipelines;
}

//! Create a vector of pipelines, each with an increasing subpass index
void preparePipelineWrapper(GraphicsPipelineWrapper &gpw, const uint32_t subpassNdx,
                            const PipelineLayoutWrapper &pipelineLayout, const VkRenderPass renderPass,
                            const ShaderWrapper vertexModule, const ShaderWrapper fragmentModule,
                            const IVec2 renderSize, const VkSampleCountFlagBits numSamples,
                            const VkPrimitiveTopology topology)
{
    const VkVertexInputBindingDescription vertexInputBindingDescription = {
        0u,                          // uint32_t binding;
        sizeof(Vertex4RGBA),         // uint32_t stride;
        VK_VERTEX_INPUT_RATE_VERTEX, // VkVertexInputRate inputRate;
    };

    const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[] = {
        {
            0u,                            // uint32_t location;
            0u,                            // uint32_t binding;
            VK_FORMAT_R32G32B32A32_SFLOAT, // VkFormat format;
            0u,                            // uint32_t offset;
        },
        {
            1u,                            // uint32_t location;
            0u,                            // uint32_t binding;
            VK_FORMAT_R32G32B32A32_SFLOAT, // VkFormat format;
            sizeof(Vec4),                  // uint32_t offset;
        },
    };

    const VkPipelineVertexInputStateCreateInfo vertexInputStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType                             sType;
        nullptr,                                                   // const void*                                 pNext;
        (VkPipelineVertexInputStateCreateFlags)0,                  // VkPipelineVertexInputStateCreateFlags       flags;
        1u,                             // uint32_t                                    vertexBindingDescriptionCount;
        &vertexInputBindingDescription, // const VkVertexInputBindingDescription*      pVertexBindingDescriptions;
        DE_LENGTH_OF_ARRAY(
            vertexInputAttributeDescriptions), // uint32_t                                    vertexAttributeDescriptionCount;
        vertexInputAttributeDescriptions, // const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions;
    };

    const std::vector<VkViewport> viewport{makeViewport(renderSize)};
    const std::vector<VkRect2D> scissor{makeRect2D(renderSize)};

    const VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                  // const void* pNext;
        (VkPipelineMultisampleStateCreateFlags)0,                 // VkPipelineMultisampleStateCreateFlags flags;
        numSamples,                                               // VkSampleCountFlagBits rasterizationSamples;
        VK_FALSE,                                                 // VkBool32 sampleShadingEnable;
        0.0f,                                                     // float minSampleShading;
        nullptr,                                                  // const VkSampleMask* pSampleMask;
        VK_FALSE,                                                 // VkBool32 alphaToCoverageEnable;
        VK_FALSE                                                  // VkBool32 alphaToOneEnable;
    };

    const VkColorComponentFlags colorComponentsAll =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    // Number of blend attachments must equal the number of color attachments during any subpass.
    const VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState = {
        VK_FALSE,             // VkBool32 blendEnable;
        VK_BLEND_FACTOR_ONE,  // VkBlendFactor srcColorBlendFactor;
        VK_BLEND_FACTOR_ZERO, // VkBlendFactor dstColorBlendFactor;
        VK_BLEND_OP_ADD,      // VkBlendOp colorBlendOp;
        VK_BLEND_FACTOR_ONE,  // VkBlendFactor srcAlphaBlendFactor;
        VK_BLEND_FACTOR_ZERO, // VkBlendFactor dstAlphaBlendFactor;
        VK_BLEND_OP_ADD,      // VkBlendOp alphaBlendOp;
        colorComponentsAll,   // VkColorComponentFlags colorWriteMask;
    };

    const VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                  // const void* pNext;
        (VkPipelineColorBlendStateCreateFlags)0,                  // VkPipelineColorBlendStateCreateFlags flags;
        VK_FALSE,                                                 // VkBool32 logicOpEnable;
        VK_LOGIC_OP_COPY,                                         // VkLogicOp logicOp;
        1u,                                                       // uint32_t attachmentCount;
        &pipelineColorBlendAttachmentState, // const VkPipelineColorBlendAttachmentState* pAttachments;
        {0.0f, 0.0f, 0.0f, 0.0f},           // float blendConstants[4];
    };

    gpw.setDefaultTopology(topology)
        .setDefaultRasterizationState()
        .setDefaultDepthStencilState()
        .setupVertexInputState(&vertexInputStateInfo)
        .setupPreRasterizationShaderState(viewport, scissor, pipelineLayout, renderPass, subpassNdx, vertexModule)
        .setupFragmentShaderState(pipelineLayout, renderPass, subpassNdx, fragmentModule, nullptr,
                                  &pipelineMultisampleStateInfo)
        .setupFragmentOutputState(renderPass, subpassNdx, &pipelineColorBlendStateInfo, &pipelineMultisampleStateInfo)
        .setMonolithicPipelineLayout(pipelineLayout)
        .buildPipeline();
}

//! Make a render pass with one subpass per color attachment and one attachment per image layer.
RenderPassWrapper makeMultisampleRenderPass(const DeviceInterface &vk, const VkDevice device,
                                            const PipelineConstructionType pipelineConstructionType,
                                            const VkFormat colorFormat, const VkSampleCountFlagBits numSamples,
                                            const uint32_t numLayers)
{
    const VkAttachmentDescription colorAttachmentDescription = {
        (VkAttachmentDescriptionFlags)0,          // VkAttachmentDescriptionFlags flags;
        colorFormat,                              // VkFormat format;
        numSamples,                               // VkSampleCountFlagBits samples;
        VK_ATTACHMENT_LOAD_OP_CLEAR,              // VkAttachmentLoadOp loadOp;
        VK_ATTACHMENT_STORE_OP_STORE,             // VkAttachmentStoreOp storeOp;
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,          // VkAttachmentLoadOp stencilLoadOp;
        VK_ATTACHMENT_STORE_OP_DONT_CARE,         // VkAttachmentStoreOp stencilStoreOp;
        VK_IMAGE_LAYOUT_UNDEFINED,                // VkImageLayout initialLayout;
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout finalLayout;
    };
    const std::vector<VkAttachmentDescription> attachmentDescriptions(numLayers, colorAttachmentDescription);

    // Create a subpass for each attachment (each attachement is a layer of an arrayed image).

    std::vector<VkAttachmentReference> colorAttachmentReferences(numLayers);
    std::vector<VkSubpassDescription> subpasses;

    for (uint32_t i = 0; i < numLayers; ++i)
    {
        const VkAttachmentReference attachmentRef = {
            i,                                       // uint32_t attachment;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // VkImageLayout layout;
        };
        colorAttachmentReferences[i] = attachmentRef;

        const VkSubpassDescription subpassDescription = {
            (VkSubpassDescriptionFlags)0,    // VkSubpassDescriptionFlags flags;
            VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint pipelineBindPoint;
            0u,                              // uint32_t inputAttachmentCount;
            nullptr,                         // const VkAttachmentReference* pInputAttachments;
            1u,                              // uint32_t colorAttachmentCount;
            &colorAttachmentReferences[i],   // const VkAttachmentReference* pColorAttachments;
            nullptr,                         // const VkAttachmentReference* pResolveAttachments;
            nullptr,                         // const VkAttachmentReference* pDepthStencilAttachment;
            0u,                              // uint32_t preserveAttachmentCount;
            nullptr                          // const uint32_t* pPreserveAttachments;
        };
        subpasses.push_back(subpassDescription);
    }

    const VkRenderPassCreateInfo renderPassInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,            // VkStructureType sType;
        nullptr,                                              // const void* pNext;
        (VkRenderPassCreateFlags)0,                           // VkRenderPassCreateFlags flags;
        static_cast<uint32_t>(attachmentDescriptions.size()), // uint32_t attachmentCount;
        &attachmentDescriptions[0],                           // const VkAttachmentDescription* pAttachments;
        static_cast<uint32_t>(subpasses.size()),              // uint32_t subpassCount;
        &subpasses[0],                                        // const VkSubpassDescription* pSubpasses;
        0u,                                                   // uint32_t dependencyCount;
        nullptr                                               // const VkSubpassDependency* pDependencies;
    };

    return RenderPassWrapper(pipelineConstructionType, vk, device, &renderPassInfo);
}

//! A single-attachment, single-subpass render pass.
RenderPassWrapper makeSimpleRenderPass(const DeviceInterface &vk, const VkDevice device,
                                       const PipelineConstructionType pipelineConstructionType,
                                       const VkFormat colorFormat)
{
    const VkAttachmentDescription colorAttachmentDescription = {
        (VkAttachmentDescriptionFlags)0,          // VkAttachmentDescriptionFlags flags;
        colorFormat,                              // VkFormat format;
        VK_SAMPLE_COUNT_1_BIT,                    // VkSampleCountFlagBits samples;
        VK_ATTACHMENT_LOAD_OP_CLEAR,              // VkAttachmentLoadOp loadOp;
        VK_ATTACHMENT_STORE_OP_STORE,             // VkAttachmentStoreOp storeOp;
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,          // VkAttachmentLoadOp stencilLoadOp;
        VK_ATTACHMENT_STORE_OP_DONT_CARE,         // VkAttachmentStoreOp stencilStoreOp;
        VK_IMAGE_LAYOUT_UNDEFINED,                // VkImageLayout initialLayout;
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout finalLayout;
    };

    const VkAttachmentReference colorAttachmentRef = {
        0u,                                      // uint32_t attachment;
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // VkImageLayout layout;
    };

    const VkSubpassDescription subpassDescription = {
        (VkSubpassDescriptionFlags)0,    // VkSubpassDescriptionFlags flags;
        VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint pipelineBindPoint;
        0u,                              // uint32_t inputAttachmentCount;
        nullptr,                         // const VkAttachmentReference* pInputAttachments;
        1u,                              // uint32_t colorAttachmentCount;
        &colorAttachmentRef,             // const VkAttachmentReference* pColorAttachments;
        nullptr,                         // const VkAttachmentReference* pResolveAttachments;
        nullptr,                         // const VkAttachmentReference* pDepthStencilAttachment;
        0u,                              // uint32_t preserveAttachmentCount;
        nullptr                          // const uint32_t* pPreserveAttachments;
    };

    const VkRenderPassCreateInfo renderPassInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // VkStructureType sType;
        nullptr,                                   // const void* pNext;
        (VkRenderPassCreateFlags)0,                // VkRenderPassCreateFlags flags;
        1u,                                        // uint32_t attachmentCount;
        &colorAttachmentDescription,               // const VkAttachmentDescription* pAttachments;
        1u,                                        // uint32_t subpassCount;
        &subpassDescription,                       // const VkSubpassDescription* pSubpasses;
        0u,                                        // uint32_t dependencyCount;
        nullptr                                    // const VkSubpassDependency* pDependencies;
    };

    return RenderPassWrapper(pipelineConstructionType, vk, device, &renderPassInfo);
}

Move<VkImage> makeImage(const DeviceInterface &vk, const VkDevice device, const VkFormat format, const IVec2 &size,
                        const uint32_t numLayers, const VkSampleCountFlagBits samples, const VkImageUsageFlags usage)
{
    const VkImageCreateInfo imageParams = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        (VkImageCreateFlags)0,               // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        format,                              // VkFormat format;
        makeExtent3D(size.x(), size.y(), 1), // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        numLayers,                           // uint32_t arrayLayers;
        samples,                             // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        usage,                               // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        nullptr,                             // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };
    return createImage(vk, device, &imageParams);
}

//! Make a simplest sampler.
Move<VkSampler> makeSampler(const DeviceInterface &vk, const VkDevice device)
{
    const VkSamplerCreateInfo samplerParams = {
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,   // VkStructureType sType;
        nullptr,                                 // const void* pNext;
        (VkSamplerCreateFlags)0,                 // VkSamplerCreateFlags flags;
        VK_FILTER_NEAREST,                       // VkFilter magFilter;
        VK_FILTER_NEAREST,                       // VkFilter minFilter;
        VK_SAMPLER_MIPMAP_MODE_NEAREST,          // VkSamplerMipmapMode mipmapMode;
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // VkSamplerAddressMode addressModeU;
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // VkSamplerAddressMode addressModeV;
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // VkSamplerAddressMode addressModeW;
        0.0f,                                    // float mipLodBias;
        VK_FALSE,                                // VkBool32 anisotropyEnable;
        1.0f,                                    // float maxAnisotropy;
        VK_FALSE,                                // VkBool32 compareEnable;
        VK_COMPARE_OP_ALWAYS,                    // VkCompareOp compareOp;
        0.0f,                                    // float minLod;
        0.0f,                                    // float maxLod;
        VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, // VkBorderColor borderColor;
        VK_FALSE,                                // VkBool32 unnormalizedCoordinates;
    };
    return createSampler(vk, device, &samplerParams);
}

inline VkImageSubresourceRange makeColorSubresourceRange(const int baseArrayLayer, const int layerCount)
{
    return makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, static_cast<uint32_t>(baseArrayLayer),
                                     static_cast<uint32_t>(layerCount));
}

inline VkImageSubresourceLayers makeColorSubresourceLayers(const int baseArrayLayer, const int layerCount)
{
    return makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, static_cast<uint32_t>(baseArrayLayer),
                                      static_cast<uint32_t>(layerCount));
}

void checkImageFormatRequirements(const InstanceInterface &vki, const VkPhysicalDevice physDevice,
                                  const VkSampleCountFlagBits sampleCount, const VkFormat format,
                                  const VkImageUsageFlags usage)
{
    VkPhysicalDeviceFeatures features;
    vki.getPhysicalDeviceFeatures(physDevice, &features);

    if (((usage & VK_IMAGE_USAGE_STORAGE_BIT) != 0) && !features.shaderStorageImageMultisample)
        TCU_THROW(NotSupportedError, "Multisampled storage images are not supported");

    VkImageFormatProperties imageFormatProperties;
    const VkResult imageFormatResult =
        vki.getPhysicalDeviceImageFormatProperties(physDevice, format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, usage,
                                                   (VkImageCreateFlags)0, &imageFormatProperties);

    if (imageFormatResult == VK_ERROR_FORMAT_NOT_SUPPORTED)
        TCU_THROW(NotSupportedError, "Image format is not supported");

    if ((imageFormatProperties.sampleCounts & sampleCount) != sampleCount)
        TCU_THROW(NotSupportedError, "Requested sample count is not supported");
}

//! The default foreground color.
inline Vec4 getPrimitiveColor(void)
{
    return Vec4(1.0f, 0.0f, 0.0f, 1.0f);
}

//! Get a reference clear value based on color format.
VkClearValue getClearValue(const VkFormat format)
{
    if (isUintFormat(format) || isIntFormat(format))
        return makeClearValueColorU32(16, 32, 64, 96);
    else
        return makeClearValueColorF32(0.0f, 0.0f, 1.0f, 1.0f);
}

std::string getColorFormatStr(const int numComponents, const bool isUint, const bool isSint)
{
    std::ostringstream str;
    if (numComponents == 1)
        str << (isUint ? "uint" : isSint ? "int" : "float");
    else
        str << (isUint ? "u" : isSint ? "i" : "") << "vec" << numComponents;

    return str.str();
}

std::string getSamplerTypeStr(const int numLayers, const bool isUint, const bool isSint)
{
    std::ostringstream str;
    str << (isUint ? "u" : isSint ? "i" : "") << "sampler2DMS" << (numLayers > 1 ? "Array" : "");
    return str.str();
}

//! Generate a gvec4 color literal.
template <typename T>
std::string getColorStr(const T *data, int numComponents, const bool isUint, const bool isSint)
{
    const int maxIndex = 3; // 4 components max

    std::ostringstream str;
    str << (isUint ? "u" : isSint ? "i" : "") << "vec4(";

    for (int i = 0; i < numComponents; ++i)
    {
        str << data[i] << (i < maxIndex ? ", " : "");
    }

    for (int i = numComponents; i < maxIndex + 1; ++i)
    {
        str << (i == maxIndex ? 1 : 0) << (i < maxIndex ? ", " : "");
    }

    str << ")";
    return str.str();
}

//! Clear color literal value used by the sampling shader.
std::string getReferenceClearColorStr(const VkFormat format, const int numComponents, const bool isUint,
                                      const bool isSint)
{
    const VkClearColorValue clearColor = getClearValue(format).color;
    if (isUint)
        return getColorStr(clearColor.uint32, numComponents, isUint, isSint);
    else if (isSint)
        return getColorStr(clearColor.int32, numComponents, isUint, isSint);
    else
        return getColorStr(clearColor.float32, numComponents, isUint, isSint);
}

//! Primitive color literal value used by the sampling shader.
std::string getReferencePrimitiveColorStr(int numComponents, const bool isUint, const bool isSint)
{
    const Vec4 color = getPrimitiveColor();
    return getColorStr(color.getPtr(), numComponents, isUint, isSint);
}

inline int getNumSamples(const VkSampleCountFlagBits samples)
{
    return static_cast<int>(samples); // enum bitmask actually matches the number of samples
}

//! A flat-colored shape with sharp angles to make antialiasing visible.
std::vector<Vertex4RGBA> genTriangleVertices(void)
{
    static const Vertex4RGBA data[] = {
        {
            Vec4(-1.0f, 0.0f, 0.0f, 1.0f),
            getPrimitiveColor(),
        },
        {
            Vec4(0.8f, 0.2f, 0.0f, 1.0f),
            getPrimitiveColor(),
        },
        {
            Vec4(0.8f, -0.2f, 0.0f, 1.0f),
            getPrimitiveColor(),
        },
    };
    return std::vector<Vertex4RGBA>(data, data + DE_LENGTH_OF_ARRAY(data));
}

Vec4 sampleIndexToColor(uint32_t index)
{
    Vec4 res = Vec4(0.0f, 0.0f, 0.0f, 1.0f);

    if (index & 0x01)
        res += Vec4(0.5f, 0.0f, 0.0f, 0.0f);
    if (index & 0x02)
        res += Vec4(0.0f, 0.5f, 0.0f, 0.0f);
    if (index & 0x04)
        res += Vec4(0.0f, 0.0f, 0.5f, 0.0f);

    if (index & 0x08)
        res += Vec4(0.5f, 0.0f, 0.0f, 0.0f);
    if (index & 0x10)
        res += Vec4(0.0f, 0.5f, 0.0f, 0.0f);
    if (index & 0x20)
        res += Vec4(0.0f, 0.0f, 0.5f, 0.0f);

    return res;
}

float *getStandardSampleLocations(VkSampleCountFlagBits samples)
{
    static float standardSampleLocations_1[1 * 2] = {
        0.5f,
        0.5f,
    };

    static float standardSampleLocations_2[2 * 2] = {
        0.75f,
        0.75f,
        0.25f,
        0.25f,
    };

    static float standardSampleLocations_4[4 * 2] = {
        0.375f, 0.125f, 0.875f, 0.375f, 0.125f, 0.625f, 0.625f, 0.875f,
    };

    static float standardSampleLocations_8[8 * 2] = {
        0.5625f, 0.3125f, 0.4375f, 0.6875f, 0.8125f, 0.5625f, 0.3125f, 0.1875f,
        0.1875f, 0.8125f, 0.0625f, 0.4375f, 0.6875f, 0.9375f, 0.9375f, 0.0625f,
    };

    static float standardSampleLocations_16[16 * 2] = {
        0.5625f, 0.5625f, 0.4375f, 0.3125f, 0.3125f, 0.625f, 0.75f,  0.4375f, 0.1875f, 0.375f, 0.625f,
        0.8125f, 0.8125f, 0.6875f, 0.6875f, 0.1875f, 0.375f, 0.875f, 0.5f,    0.0625f, 0.25f,  0.125f,
        0.125f,  0.75f,   0.0f,    0.5f,    0.9375f, 0.25f,  0.875f, 0.9375f, 0.0625f, 0.0f,
    };

    switch (samples)
    {
    case VK_SAMPLE_COUNT_1_BIT:
        return standardSampleLocations_1;
    case VK_SAMPLE_COUNT_2_BIT:
        return standardSampleLocations_2;
    case VK_SAMPLE_COUNT_4_BIT:
        return standardSampleLocations_4;
    case VK_SAMPLE_COUNT_8_BIT:
        return standardSampleLocations_8;
    case VK_SAMPLE_COUNT_16_BIT:
        return standardSampleLocations_16;
    default:
        TCU_THROW(InternalError, "Unknown multisample bit configuration requested");
    }
}

//! A flat-colored shapes plotted at standard sample points.
std::vector<Vertex4RGBA> genPerSampleTriangleVertices(VkSampleCountFlagBits samples)
{
    float *coordinates       = getStandardSampleLocations(samples);
    const float triangleSize = 1.0f / (static_cast<float>(samples) * 2.0f);
    std::vector<Vertex4RGBA> res;

    for (uint32_t i = 0; i < static_cast<uint32_t>(samples); i++)
    {
        Vertex4RGBA data[] = {
            {
                Vec4(0 + coordinates[i * 2 + 0] * 2 - 1, -triangleSize + coordinates[i * 2 + 1] * 2 - 1, 0.0f, 1.0f),
                sampleIndexToColor(i),
            },
            {
                Vec4(-triangleSize + coordinates[i * 2 + 0] * 2 - 1, triangleSize + coordinates[i * 2 + 1] * 2 - 1,
                     0.0f, 1.0f),
                sampleIndexToColor(i),
            },
            {
                Vec4(triangleSize + coordinates[i * 2 + 0] * 2 - 1, triangleSize + coordinates[i * 2 + 1] * 2 - 1, 0.0f,
                     1.0f),
                sampleIndexToColor(i),
            },
        };
        res.push_back(data[0]);
        res.push_back(data[1]);
        res.push_back(data[2]);
    }
    return res;
}

//! A full-viewport quad. Use with TRIANGLE_STRIP topology.
std::vector<Vertex4RGBA> genFullQuadVertices(void)
{
    static const Vertex4RGBA data[] = {
        {
            Vec4(-1.0f, -1.0f, 0.0f, 1.0f),
            Vec4(), // unused
        },
        {
            Vec4(-1.0f, 1.0f, 0.0f, 1.0f),
            Vec4(), // unused
        },
        {
            Vec4(1.0f, -1.0f, 0.0f, 1.0f),
            Vec4(), // unused
        },
        {
            Vec4(1.0f, 1.0f, 0.0f, 1.0f),
            Vec4(), // unused
        },
    };
    return std::vector<Vertex4RGBA>(data, data + DE_LENGTH_OF_ARRAY(data));
}

std::string getShaderImageFormatQualifier(const tcu::TextureFormat &format)
{
    const char *orderPart;
    const char *typePart;

    switch (format.order)
    {
    case tcu::TextureFormat::R:
        orderPart = "r";
        break;
    case tcu::TextureFormat::RG:
        orderPart = "rg";
        break;
    case tcu::TextureFormat::RGB:
        orderPart = "rgb";
        break;
    case tcu::TextureFormat::RGBA:
        orderPart = "rgba";
        break;

    default:
        DE_ASSERT(false);
        orderPart = nullptr;
    }

    switch (format.type)
    {
    case tcu::TextureFormat::FLOAT:
        typePart = "32f";
        break;
    case tcu::TextureFormat::HALF_FLOAT:
        typePart = "16f";
        break;

    case tcu::TextureFormat::UNSIGNED_INT32:
        typePart = "32ui";
        break;
    case tcu::TextureFormat::UNSIGNED_INT16:
        typePart = "16ui";
        break;
    case tcu::TextureFormat::UNSIGNED_INT8:
        typePart = "8ui";
        break;

    case tcu::TextureFormat::SIGNED_INT32:
        typePart = "32i";
        break;
    case tcu::TextureFormat::SIGNED_INT16:
        typePart = "16i";
        break;
    case tcu::TextureFormat::SIGNED_INT8:
        typePart = "8i";
        break;

    case tcu::TextureFormat::UNORM_INT16:
        typePart = "16";
        break;
    case tcu::TextureFormat::UNORM_INT8:
        typePart = "8";
        break;

    case tcu::TextureFormat::SNORM_INT16:
        typePart = "16_snorm";
        break;
    case tcu::TextureFormat::SNORM_INT8:
        typePart = "8_snorm";
        break;

    default:
        DE_ASSERT(false);
        typePart = nullptr;
    }

    return std::string() + orderPart + typePart;
}

std::string getShaderMultisampledImageType(const tcu::TextureFormat &format, const int numLayers)
{
    const std::string formatPart =
        tcu::getTextureChannelClass(format.type) == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER ? "u" :
        tcu::getTextureChannelClass(format.type) == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER   ? "i" :
                                                                                                "";

    std::ostringstream str;
    str << formatPart << "image2DMS" << (numLayers > 1 ? "Array" : "");

    return str.str();
}

void addSimpleVertexAndFragmentPrograms(SourceCollections &programCollection, const CaseDef caseDef)
{
    const int numComponents = tcu::getNumUsedChannels(mapVkFormat(caseDef.colorFormat).order);
    const bool isUint       = isUintFormat(caseDef.colorFormat);
    const bool isSint       = isIntFormat(caseDef.colorFormat);

    // Vertex shader
    {
        std::ostringstream src;
        src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
            << "\n"
            << "layout(location = 0) in  vec4 in_position;\n"
            << "layout(location = 1) in  vec4 in_color;\n"
            << "layout(location = 0) out vec4 o_color;\n"
            << "\n"
            << "out gl_PerVertex {\n"
            << "    vec4 gl_Position;\n"
            << "};\n"
            << "\n"
            << "void main(void)\n"
            << "{\n"
            << "    gl_Position = in_position;\n"
            << "    o_color     = in_color;\n"
            << "}\n";

        programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
    }

    // Fragment shader
    {
        const std::string colorFormat = getColorFormatStr(numComponents, isUint, isSint);

        std::ostringstream src;
        src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
            << "\n"
            << "layout(location = 0) in  vec4 in_color;\n"
            << "layout(location = 0) out " << colorFormat << " o_color;\n"
            << "\n"
            << "void main(void)\n"
            << "{\n"
            << "    o_color = " << colorFormat << "(" // float color will be converted to int/uint here if needed
            << (numComponents == 1 ? "in_color.r" :
                numComponents == 2 ? "in_color.rg" :
                numComponents == 3 ? "in_color.rgb" :
                                     "in_color")
            << ");\n"
            << "}\n";

        programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
    }
}

//! Synchronously render to a multisampled color image.
void renderMultisampledImage(Context &context, const CaseDef &caseDef, const VkImage colorImage)
{
    const InstanceInterface &vki          = context.getInstanceInterface();
    const DeviceInterface &vk             = context.getDeviceInterface();
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
    const VkDevice device                 = context.getDevice();
    const VkQueue queue                   = context.getUniversalQueue();
    const uint32_t queueFamilyIndex       = context.getUniversalQueueFamilyIndex();
    Allocator &allocator                  = context.getDefaultAllocator();

    const Unique<VkCommandPool> cmdPool(
        createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
    const Unique<VkCommandBuffer> cmdBuffer(makeCommandBuffer(vk, device, *cmdPool));

    {
        // Create an image view (attachment) for each layer of the image
        std::vector<ImageViewSp> colorAttachments;
        std::vector<VkImage> images;
        std::vector<VkImageView> attachmentHandles;
        for (int i = 0; i < caseDef.numLayers; ++i)
        {
            colorAttachments.push_back(makeSharedPtr(makeImageView(
                vk, device, colorImage, VK_IMAGE_VIEW_TYPE_2D, caseDef.colorFormat, makeColorSubresourceRange(i, 1))));
            images.push_back(colorImage);
            attachmentHandles.push_back(**colorAttachments.back());
        }

        // Vertex buffer
        const std::vector<Vertex4RGBA> vertices =
            caseDef.colorSamples ? genPerSampleTriangleVertices(caseDef.numSamples) : genTriangleVertices();
        const VkDeviceSize vertexBufferSize = sizeInBytes(vertices);
        const Unique<VkBuffer> vertexBuffer(
            makeBuffer(vk, device, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT));
        const UniquePtr<Allocation> vertexBufferAlloc(
            bindBuffer(vk, device, allocator, *vertexBuffer, MemoryRequirement::HostVisible));

        {
            deMemcpy(vertexBufferAlloc->getHostPtr(), &vertices[0], static_cast<std::size_t>(vertexBufferSize));
            flushAlloc(vk, device, *vertexBufferAlloc);
        }

        const ShaderWrapper vertexModule(ShaderWrapper(vk, device, context.getBinaryCollection().get("vert"), 0u));
        const ShaderWrapper fragmentModule(ShaderWrapper(vk, device, context.getBinaryCollection().get("frag"), 0u));
        RenderPassWrapper renderPass(makeMultisampleRenderPass(
            vk, device, caseDef.pipelineConstructionType, caseDef.colorFormat, caseDef.numSamples, caseDef.numLayers));
        renderPass.createFramebuffer(vk, device, caseDef.numLayers, &images[0], &attachmentHandles[0],
                                     static_cast<uint32_t>(caseDef.renderSize.x()),
                                     static_cast<uint32_t>(caseDef.renderSize.y()));
        const PipelineLayoutWrapper pipelineLayout(caseDef.pipelineConstructionType, vk, device);
        const bool isMonolithic(caseDef.pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC);
        std::vector<PipelineSp> pipelinesSp;
        std::vector<GraphicsPipelineWrapper> pipelineWrapper;

        if (isMonolithic)
        {
            pipelinesSp = makeGraphicsPipelines(vk, device, caseDef.numLayers, *pipelineLayout, *renderPass,
                                                vertexModule, fragmentModule, caseDef.renderSize, caseDef.numSamples,
                                                VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        }
        else
        {
            // we can't create a vector of derived pipelines with GraphicsPipelineWrapper
            pipelineWrapper.reserve(caseDef.numLayers);
            for (int subpassNdx = 0; subpassNdx < caseDef.numLayers; ++subpassNdx)
            {
                pipelineWrapper.emplace_back(vki, vk, physicalDevice, device, context.getDeviceExtensions(),
                                             caseDef.pipelineConstructionType);
                preparePipelineWrapper(pipelineWrapper.back(), subpassNdx, pipelineLayout, *renderPass, vertexModule,
                                       fragmentModule, caseDef.renderSize, caseDef.numSamples,
                                       VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
            }
        }

        beginCommandBuffer(vk, *cmdBuffer);

        const std::vector<VkClearValue> clearValues(caseDef.numLayers, getClearValue(caseDef.colorFormat));

        renderPass.begin(vk, *cmdBuffer, makeRect2D(0, 0, caseDef.renderSize.x(), caseDef.renderSize.y()),
                         (uint32_t)clearValues.size(), &clearValues[0]);
        {
            const VkDeviceSize vertexBufferOffset = 0ull;
            vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
        }

        for (int layerNdx = 0; layerNdx < caseDef.numLayers; ++layerNdx)
        {
            if (layerNdx != 0)
                renderPass.nextSubpass(vk, *cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);

            if (isMonolithic)
                vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, **pipelinesSp[layerNdx]);
            else
                pipelineWrapper[layerNdx].bind(*cmdBuffer);
            vk.cmdDraw(*cmdBuffer, static_cast<uint32_t>(vertices.size()), 1u, 0u, 0u);
        }

        renderPass.end(vk, *cmdBuffer);

        endCommandBuffer(vk, *cmdBuffer);
        submitCommandsAndWait(vk, device, queue, *cmdBuffer);
    }
}

namespace SampledImage
{

void initPrograms(SourceCollections &programCollection, const CaseDef caseDef)
{
    // Pass 1: Render to texture

    addSimpleVertexAndFragmentPrograms(programCollection, caseDef);

    // Pass 2: Sample texture

    // Vertex shader
    {
        std::ostringstream src;
        src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
            << "\n"
            << "layout(location = 0) in  vec4  in_position;\n"
            << "\n"
            << "out gl_PerVertex {\n"
            << "    vec4 gl_Position;\n"
            << "};\n"
            << "\n"
            << "void main(void)\n"
            << "{\n"
            << "    gl_Position = in_position;\n"
            << "}\n";

        programCollection.glslSources.add("sample_vert") << glu::VertexSource(src.str());
    }

    // Fragment shader
    {
        const int numComponents          = tcu::getNumUsedChannels(mapVkFormat(caseDef.colorFormat).order);
        const bool isUint                = isUintFormat(caseDef.colorFormat);
        const bool isSint                = isIntFormat(caseDef.colorFormat);
        const std::string texelFormatStr = (isUint ? "uvec4" : isSint ? "ivec4" : "vec4");
        const std::string refClearColor = getReferenceClearColorStr(caseDef.colorFormat, numComponents, isUint, isSint);
        const std::string refPrimitiveColor = getReferencePrimitiveColorStr(numComponents, isUint, isSint);
        const std::string samplerTypeStr    = getSamplerTypeStr(caseDef.numLayers, isUint, isSint);

        std::ostringstream src;
        src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
            << "\n"
            << "layout(location = 0) out uvec2 o_status;\n"
            << "\n"
            << "layout(set = 0, binding = 0) uniform " << samplerTypeStr << " colorTexture;\n"
            << "\n"
            << "void main(void)\n"
            << "{\n"
            << "    uint clearColorCount = 0;\n"
            << "    uint primitiveColorCount = 0;\n"
            << "\n";

        if (caseDef.numLayers == 1)
            src << "    for (int sampleNdx = 0; sampleNdx < " << caseDef.numSamples << "; ++sampleNdx) {\n"
                << "        " << texelFormatStr
                << " color = texelFetch(colorTexture, ivec2(gl_FragCoord.xy), sampleNdx);\n"
                << "        if (color == " << refClearColor << ")\n"
                << "            ++clearColorCount;\n"
                << "        else if (color == " << refPrimitiveColor << ")\n"
                << "            ++primitiveColorCount;\n"
                << "    }\n";
        else
            src << "    for (int layerNdx = 0; layerNdx < " << caseDef.numLayers << "; ++layerNdx)\n"
                << "    for (int sampleNdx = 0; sampleNdx < " << caseDef.numSamples << "; ++sampleNdx) {\n"
                << "        " << texelFormatStr
                << " color = texelFetch(colorTexture, ivec3(gl_FragCoord.xy, layerNdx), sampleNdx);\n"
                << "        if (color == " << refClearColor << ")\n"
                << "            ++clearColorCount;\n"
                << "        else if (color == " << refPrimitiveColor << ")\n"
                << "            ++primitiveColorCount;\n"
                << "    }\n";

        src << "\n"
            << "    o_status = uvec2(clearColorCount, primitiveColorCount);\n"
            << "}\n";

        programCollection.glslSources.add("sample_frag") << glu::FragmentSource(src.str());
    }
}

void checkSupport(Context &context, const CaseDef caseDef)
{
    const VkImageUsageFlags colorImageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    checkImageFormatRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), caseDef.numSamples,
                                 caseDef.colorFormat, colorImageUsage);
    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          caseDef.pipelineConstructionType);

#ifndef CTS_USES_VULKANSC
    if (context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") &&
        !context.getPortabilitySubsetFeatures().multisampleArrayImage &&
        (caseDef.numSamples != VK_SAMPLE_COUNT_1_BIT) && (caseDef.numLayers != 1))
    {
        TCU_THROW(
            NotSupportedError,
            "VK_KHR_portability_subset: Implementation does not support image array with multiple samples per texel");
    }
#endif // CTS_USES_VULKANSC
}

tcu::TestStatus test(Context &context, const CaseDef caseDef)
{
    const InstanceInterface &vki          = context.getInstanceInterface();
    const DeviceInterface &vk             = context.getDeviceInterface();
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
    const VkDevice device                 = context.getDevice();
    const VkQueue queue                   = context.getUniversalQueue();
    const uint32_t queueFamilyIndex       = context.getUniversalQueueFamilyIndex();
    Allocator &allocator                  = context.getDefaultAllocator();

    const VkImageUsageFlags colorImageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    {
        tcu::TestLog &log = context.getTestContext().getLog();
        log << tcu::LogSection("Description", "") << tcu::TestLog::Message
            << "Rendering to a multisampled image. Expecting all samples to be either a clear color or a primitive "
               "color."
            << tcu::TestLog::EndMessage << tcu::TestLog::Message
            << "Sampling from the texture with texelFetch (OpImageFetch)." << tcu::TestLog::EndMessage
            << tcu::TestLog::EndSection;
    }

    // Multisampled color image
    const Unique<VkImage> colorImage(makeImage(vk, device, caseDef.colorFormat, caseDef.renderSize, caseDef.numLayers,
                                               caseDef.numSamples, colorImageUsage));
    const UniquePtr<Allocation> colorImageAlloc(bindImage(vk, device, allocator, *colorImage, MemoryRequirement::Any));

    const Unique<VkCommandPool> cmdPool(
        createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
    const Unique<VkCommandBuffer> cmdBuffer(makeCommandBuffer(vk, device, *cmdPool));

    // Step 1: Render to texture
    {
        renderMultisampledImage(context, caseDef, *colorImage);
    }

    // Step 2: Sample texture
    {
        // Color image view
        const VkImageViewType colorImageViewType =
            (caseDef.numLayers == 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY);
        const Unique<VkImageView> colorImageView(makeImageView(vk, device, *colorImage, colorImageViewType,
                                                               caseDef.colorFormat,
                                                               makeColorSubresourceRange(0, caseDef.numLayers)));
        const Unique<VkSampler> colorSampler(makeSampler(vk, device));

        // Checksum image
        const VkFormat checksumFormat = VK_FORMAT_R8G8_UINT;
        const Unique<VkImage> checksumImage(
            makeImage(vk, device, checksumFormat, caseDef.renderSize, 1u, VK_SAMPLE_COUNT_1_BIT,
                      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT));
        const UniquePtr<Allocation> checksumImageAlloc(
            bindImage(vk, device, allocator, *checksumImage, MemoryRequirement::Any));
        const Unique<VkImageView> checksumImageView(makeImageView(vk, device, *checksumImage, VK_IMAGE_VIEW_TYPE_2D,
                                                                  checksumFormat, makeColorSubresourceRange(0, 1)));

        // Checksum buffer (for host reading)
        const VkDeviceSize checksumBufferSize =
            caseDef.renderSize.x() * caseDef.renderSize.y() * tcu::getPixelSize(mapVkFormat(checksumFormat));
        const Unique<VkBuffer> checksumBuffer(
            makeBuffer(vk, device, checksumBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
        const UniquePtr<Allocation> checksumBufferAlloc(
            bindBuffer(vk, device, allocator, *checksumBuffer, MemoryRequirement::HostVisible));

        zeroBuffer(vk, device, *checksumBufferAlloc, checksumBufferSize);

        // Vertex buffer
        const std::vector<Vertex4RGBA> vertices = genFullQuadVertices();
        const VkDeviceSize vertexBufferSize     = sizeInBytes(vertices);
        const Unique<VkBuffer> vertexBuffer(
            makeBuffer(vk, device, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT));
        const UniquePtr<Allocation> vertexBufferAlloc(
            bindBuffer(vk, device, allocator, *vertexBuffer, MemoryRequirement::HostVisible));

        {
            deMemcpy(vertexBufferAlloc->getHostPtr(), &vertices[0], static_cast<std::size_t>(vertexBufferSize));
            flushAlloc(vk, device, *vertexBufferAlloc);
        }

        // Descriptors
        // \note OpImageFetch doesn't use a sampler, but in GLSL texelFetch needs a sampler2D which translates to a combined image sampler in Vulkan.

        const Unique<VkDescriptorSetLayout> descriptorSetLayout(
            DescriptorSetLayoutBuilder()
                .addSingleSamplerBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT,
                                         &colorSampler.get())
                .build(vk, device));

        const Unique<VkDescriptorPool> descriptorPool(
            DescriptorPoolBuilder()
                .addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

        const Unique<VkDescriptorSet> descriptorSet(
            makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));
        const VkDescriptorImageInfo imageDescriptorInfo =
            makeDescriptorImageInfo(VK_NULL_HANDLE, *colorImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        DescriptorSetUpdateBuilder()
            .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageDescriptorInfo)
            .update(vk, device);

        const ShaderWrapper vertexModule(
            ShaderWrapper(vk, device, context.getBinaryCollection().get("sample_vert"), 0u));
        const ShaderWrapper fragmentModule(
            ShaderWrapper(vk, device, context.getBinaryCollection().get("sample_frag"), 0u));
        RenderPassWrapper renderPass(caseDef.pipelineConstructionType, vk, device, checksumFormat);
        renderPass.createFramebuffer(vk, device, 1u, &checksumImage.get(), &checksumImageView.get(),
                                     static_cast<uint32_t>(caseDef.renderSize.x()),
                                     static_cast<uint32_t>(caseDef.renderSize.y()));
        const PipelineLayoutWrapper pipelineLayout(caseDef.pipelineConstructionType, vk, device, *descriptorSetLayout);

        const bool isMonolithic(caseDef.pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC);
        std::vector<PipelineSp> pipelinesSp;
        std::vector<GraphicsPipelineWrapper> pipelineWrapper;

        if (isMonolithic)
        {
            pipelinesSp =
                makeGraphicsPipelines(vk, device, 1u, *pipelineLayout, *renderPass, vertexModule, fragmentModule,
                                      caseDef.renderSize, VK_SAMPLE_COUNT_1_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
        }
        else
        {
            pipelineWrapper.emplace_back(vki, vk, physicalDevice, device, context.getDeviceExtensions(),
                                         caseDef.pipelineConstructionType);
            preparePipelineWrapper(pipelineWrapper.back(), 0u, pipelineLayout, *renderPass, vertexModule,
                                   fragmentModule, caseDef.renderSize, VK_SAMPLE_COUNT_1_BIT,
                                   VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
        }

        beginCommandBuffer(vk, *cmdBuffer);

        // Prepare for sampling in the fragment shader
        {
            const VkImageMemoryBarrier barriers[] = {
                {
                    VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,          // VkStructureType sType;
                    nullptr,                                         // const void* pNext;
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,            // VkAccessFlags outputMask;
                    VK_ACCESS_SHADER_READ_BIT,                       // VkAccessFlags inputMask;
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,        // VkImageLayout oldLayout;
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,        // VkImageLayout newLayout;
                    VK_QUEUE_FAMILY_IGNORED,                         // uint32_t srcQueueFamilyIndex;
                    VK_QUEUE_FAMILY_IGNORED,                         // uint32_t destQueueFamilyIndex;
                    *colorImage,                                     // VkImage image;
                    makeColorSubresourceRange(0, caseDef.numLayers), // VkImageSubresourceRange subresourceRange;
                },
            };

            vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u, 0u, nullptr, 0u, nullptr,
                                  DE_LENGTH_OF_ARRAY(barriers), barriers);
        }

        renderPass.begin(vk, *cmdBuffer, makeRect2D(0, 0, caseDef.renderSize.x(), caseDef.renderSize.y()),
                         tcu::UVec4(0u));

        if (isMonolithic)
            vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, **pipelinesSp.back());
        else
            pipelineWrapper.back().bind(*cmdBuffer);
        vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u,
                                 &descriptorSet.get(), 0u, nullptr);
        {
            const VkDeviceSize vertexBufferOffset = 0ull;
            vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
        }

        vk.cmdDraw(*cmdBuffer, static_cast<uint32_t>(vertices.size()), 1u, 0u, 0u);
        renderPass.end(vk, *cmdBuffer);

        copyImageToBuffer(vk, *cmdBuffer, *checksumImage, *checksumBuffer, caseDef.renderSize);

        endCommandBuffer(vk, *cmdBuffer);
        submitCommandsAndWait(vk, device, queue, *cmdBuffer);

        // Verify result

        {
            invalidateAlloc(vk, device, *checksumBufferAlloc);

            const tcu::ConstPixelBufferAccess access(mapVkFormat(checksumFormat), caseDef.renderSize.x(),
                                                     caseDef.renderSize.y(), 1, checksumBufferAlloc->getHostPtr());
            const uint32_t numExpectedChecksum = getNumSamples(caseDef.numSamples) * caseDef.numLayers;
            bool multipleColorsPerTexelFound   = false;

            for (int y = 0; y < caseDef.renderSize.y(); ++y)
                for (int x = 0; x < caseDef.renderSize.x(); ++x)
                {
                    uint32_t clearColorCount     = access.getPixelUint(x, y).x();
                    uint32_t primitiveColorCount = access.getPixelUint(x, y).y();

                    if ((clearColorCount + primitiveColorCount) != numExpectedChecksum)
                        return tcu::TestStatus::fail("Some samples have incorrect color");

                    if ((clearColorCount > 0) && (primitiveColorCount > 0))
                        multipleColorsPerTexelFound = true;
                }

            // For a multisampled image, we are expecting some texels to have samples of both clear color and primitive color
            if (!multipleColorsPerTexelFound)
                return tcu::TestStatus::fail(
                    "Could not find texels with samples of both clear color and primitive color");
        }
    }

    return tcu::TestStatus::pass("OK");
}

} // namespace SampledImage

namespace Image3d
{

void initPrograms(SourceCollections &programCollection, const CaseDef3d caseDef)
{
    DE_UNREF(caseDef);

    std::ostringstream vert;
    {
        vert << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
             << "layout(location = 0) in vec4 inPosition;\n"
             << "void main()\n"
             << "{\n"
             << "    gl_Position = inPosition;\n"
             << "}\n";
    }
    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

    std::ostringstream frag;
    {
        frag << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
             << "layout(location = 0) out vec4 outColor;\n"
             << "\n"
             << "layout(push_constant) uniform PushConsts {\n"
             << "    int width;\n"
             << "    int height;\n"
             << "    int numSamples;\n"
             << "} pc;\n"
             << "\n"
             << "void main()\n"
             << "{\n"
             << "    int s = gl_SampleID;\n"
             << "\n"
             << "    float R = float(int(gl_FragCoord.x) + s) / float(pc.width + pc.numSamples);\n"
             << "    float G = float(int(gl_FragCoord.y) + s) / float(pc.height + pc.numSamples);\n"
             << "    float B = (pc.numSamples > 1) ? float(s) / float(pc.numSamples - 1) : 0.0;\n"
             << "    float A = 1.0f;\n"
             << "\n"
             << "    outColor = vec4(R, G, B, A);\n"
             << "}\n";
    }
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

void checkSupport(Context &context, const CaseDef3d caseDef)
{
    const InstanceInterface &vki          = context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();

    const VkImageUsageFlags colorImageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    const VkSampleCountFlagBits sampleCount = caseDef.numSamples;

    {
        VkImageFormatProperties srcImageFormatProperties;
        const VkResult srcImageFormatResult = vki.getPhysicalDeviceImageFormatProperties(
            physicalDevice, caseDef.colorFormat, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, colorImageUsage,
            (VkImageCreateFlags)0, &srcImageFormatProperties);

        if (srcImageFormatResult == VK_ERROR_FORMAT_NOT_SUPPORTED)
            TCU_THROW(NotSupportedError, "Image format is not supported");

        if ((srcImageFormatProperties.sampleCounts & sampleCount) != sampleCount)
            TCU_THROW(NotSupportedError, "Requested sample count is not supported");
    }

    {
        VkImageFormatProperties dstImageFormatProperties;
        const VkResult dstImageFormatResult = vki.getPhysicalDeviceImageFormatProperties(
            physicalDevice, caseDef.colorFormat, VK_IMAGE_TYPE_3D, VK_IMAGE_TILING_OPTIMAL, colorImageUsage,
            (VkImageCreateFlags)0, &dstImageFormatProperties);

        if (dstImageFormatResult == VK_ERROR_FORMAT_NOT_SUPPORTED)
            TCU_THROW(NotSupportedError, "Image format is not supported");
    }

    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          caseDef.pipelineConstructionType);
}

tcu::TestStatus test(Context &context, const CaseDef3d caseDef)
{
    const InstanceInterface &vki = context.getInstanceInterface();
    const DeviceInterface &vkd   = context.getDeviceInterface();
    const auto phyDevice         = context.getPhysicalDevice();
    const VkDevice device        = context.getDevice();
    Allocator &alloc             = context.getDefaultAllocator();
    const uint32_t queueIndex    = context.getUniversalQueueFamilyIndex();
    const VkQueue queue          = context.getUniversalQueue();

    const IVec3 size                    = caseDef.renderSize;
    const VkSampleCountFlagBits samples = caseDef.numSamples;
    const VkExtent3D msImageExtent      = makeExtent3D(size.x(), size.y(), 1u);
    const VkRect2D renderArea           = makeRect2D(size.x(), size.y());
    const Vec4 srcClearColor(tcu::RGBA::black().toVec());
    const Vec4 dstClearColor(tcu::RGBA::green().toVec());
    const auto dstClearColorValue         = makeClearValueColorVec4(dstClearColor);
    const auto colorSubresourceRange      = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const VkDeviceSize vertexBufferOffset = 0u;

    // Create a multisampled image of type 2D
    VkImageCreateInfo srcImageParams = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        VkImageCreateFlags(0u),              // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        caseDef.colorFormat,                 // VkFormat format;
        msImageExtent,                       // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        1u,                                  // uint32_t arrayLayers;
        samples,                             // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
            VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        nullptr,                             // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };

    const ImageWithMemory multisampledImage{vkd, device, alloc, srcImageParams, MemoryRequirement::Any};

    // Create a normal image of type 3D
    VkImageCreateInfo dstImageParams = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,        // VkStructureType sType;
        nullptr,                                    // const void* pNext;
        VkImageCreateFlags(0u),                     // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_3D,                           // VkImageType imageType;
        caseDef.colorFormat,                        // VkFormat format;
        makeExtent3D(size.x(), size.y(), size.z()), // VkExtent3D extent;
        1u,                                         // uint32_t mipLevels;
        1u,                                         // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,                      // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,                    // VkImageTiling tiling;
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT             // VkImageUsageFlags usage;
            | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_SHARING_MODE_EXCLUSIVE, // VkSharingMode sharingMode;
        0u,                        // uint32_t queueFamilyIndexCount;
        nullptr,                   // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED, // VkImageLayout initialLayout;
    };

    const ImageWithMemory dst3dImage{vkd, device, alloc, dstImageParams, MemoryRequirement::Any};

    std::vector<tcu::Vec4> vertices;
    {
        const tcu::Vec4 a(-1.0f, -1.0f, 0.0f, 1.0f);
        const tcu::Vec4 b(1.0f, -1.0f, 0.0f, 1.0f);
        const tcu::Vec4 c(1.0f, 1.0f, 0.0f, 1.0f);
        const tcu::Vec4 d(-1.0f, 1.0f, 0.0f, 1.0f);

        vertices.push_back(a);
        vertices.push_back(c);
        vertices.push_back(b);
        vertices.push_back(a);
        vertices.push_back(c);
        vertices.push_back(d);
    }

    // Create vertex buffer
    const VkDeviceSize vertexDataSize = vertices.size() * sizeof(tcu::Vec4);
    const BufferWithMemory vertexBuffer{vkd, device, alloc,
                                        makeBufferCreateInfo(vertexDataSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
                                        MemoryRequirement::HostVisible};
    {
        const auto &vertexBufferAlloc = vertexBuffer.getAllocation();
        const auto vertexDataPtr =
            reinterpret_cast<char *>(vertexBufferAlloc.getHostPtr()) + vertexBufferAlloc.getOffset();
        deMemcpy(vertexDataPtr, de::dataOrNull(vertices), static_cast<size_t>(vertexDataSize));
        flushAlloc(vkd, device, vertexBufferAlloc);
    }

    // Initialize samples
    const uint32_t width      = static_cast<uint32_t>(size.x());
    const uint32_t height     = static_cast<uint32_t>(size.y());
    const uint32_t numSamples = static_cast<uint32_t>(caseDef.numSamples);

    std::vector<std::vector<tcu::Vec4>> sampleVals(width * height, std::vector<tcu::Vec4>(numSamples));

    for (uint32_t y = 0; y < height; ++y)
    {
        for (uint32_t x = 0; x < width; ++x)
        {
            const uint32_t pixelIndex = y * width + x;

            for (uint32_t s = 0; s < numSamples; ++s)
            {
                float R = static_cast<float>(x + s) / static_cast<float>(width + numSamples);
                float G = static_cast<float>(y + s) / static_cast<float>(height + numSamples);
                float B = (numSamples > 1) ? static_cast<float>(s) / static_cast<float>(numSamples - 1) : 0.0f;
                float A = 1.0f;

                sampleVals[pixelIndex][s] = tcu::Vec4(R, G, B, A);
            }
        }
    }

    // Push constants
    const struct ImageInfo
    {
        int32_t width;
        int32_t height;
        int32_t numSamples;
    } pushConstantData = {
        static_cast<int32_t>(size.x()),
        static_cast<int32_t>(size.y()),
        static_cast<int32_t>(caseDef.numSamples),
    };
    const auto pushConstantSize = static_cast<uint32_t>(sizeof(ImageInfo));

    // Shader modules
    const auto vertexModule = ShaderWrapper(vkd, device, context.getBinaryCollection().get("vert"), 0u);
    const auto fragModule   = ShaderWrapper(vkd, device, context.getBinaryCollection().get("frag"), 0u);

    RenderPassWrapper renderPass;

    // Render pass
    {
        const VkAttachmentDescription colorAttachment = {
            0u,                                      // VkAttachmentDescriptionFlags flags;
            caseDef.colorFormat,                     // VkFormat format;
            samples,                                 // VkSampleCountFlagBits samples;
            VK_ATTACHMENT_LOAD_OP_CLEAR,             // VkAttachmentLoadOp loadOp;
            VK_ATTACHMENT_STORE_OP_STORE,            // VkAttachmentStoreOp storeOp;
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,         // VkAttachmentLoadOp stencilLoadOp;
            VK_ATTACHMENT_STORE_OP_DONT_CARE,        // VkAttachmentStoreOp stencilStoreOp;
            VK_IMAGE_LAYOUT_UNDEFINED,               // VkImageLayout initialLayout;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // VkImageLayout finalLayout;
        };

        const VkAttachmentReference colorRef = {
            0u,                                       // uint32_t attachment;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout layout;
        };

        const VkSubpassDescription subpass = {
            0u,                              // VkSubpassDescriptionFlags flags;
            VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint pipelineBindPoint;
            0u,                              // uint32_t inputAttachmentCount;
            nullptr,                         // const VkAttachmentReference* pInputAttachments;
            1u,                              // uint32_t colorAttachmentCount;
            &colorRef,                       // const VkAttachmentReference* pColorAttachments;
            nullptr,                         // const VkAttachmentReference* pResolveAttachments;
            nullptr,                         // const VkAttachmentReference* pDepthStencilAttachment;
            0u,                              // uint32_t preserveAttachmentCount;
            nullptr,                         // const uint32_t* pPreserveAttachments;
        };

        const VkRenderPassCreateInfo renderPassInfo = {
            VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // VkStructureType sType;
            nullptr,                                   // const void* pNext;
            0u,                                        // VkRenderPassCreateFlags flags;
            1u,                                        // uint32_t attachmentCount;
            &colorAttachment,                          // const VkAttachmentDescription* pAttachments;
            1u,                                        // uint32_t subpassCount;
            &subpass,                                  // const VkSubpassDescription* pSubpasses;
            0u,                                        // uint32_t dependencyCount;
            nullptr,                                   // const VkSubpassDependency* pDependencies;
        };

        renderPass = RenderPassWrapper(caseDef.pipelineConstructionType, vkd, device, &renderPassInfo);
    }

    // Framebuffer
    const auto msImageView = makeImageView(vkd, device, multisampledImage.get(), VK_IMAGE_VIEW_TYPE_2D,
                                           caseDef.colorFormat, colorSubresourceRange);

    renderPass.createFramebuffer(vkd, device, 1u, &multisampledImage.get(), &msImageView.get(), msImageExtent.width,
                                 msImageExtent.height, msImageExtent.depth);

    // Pipeline
    const VkPushConstantRange pushConstantRange = {
        VK_SHADER_STAGE_FRAGMENT_BIT, // VkShaderStageFlags stageFlags;
        0u,                           // uint32_t offset;
        pushConstantSize,             // uint32_t size;
    };
    const PipelineLayoutWrapper pipelineLayout(caseDef.pipelineConstructionType, vkd, device, VK_NULL_HANDLE,
                                               &pushConstantRange);
    GraphicsPipelineWrapper graphicsPipeline(vki, vkd, phyDevice, device, context.getDeviceExtensions(),
                                             caseDef.pipelineConstructionType);

    {
        const std::vector<VkViewport> viewports{makeViewport(msImageExtent)};
        const std::vector<VkRect2D> scissors{renderArea};

        const VkPipelineMultisampleStateCreateInfo multisampleStateParams = {
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                  // const void* pNext;
            0u,                                                       // VkPipelineMultisampleStateCreateFlags flags;
            samples,                                                  // VkSampleCountFlagBits rasterizationSamples;
            VK_FALSE,                                                 // VkBool32 sampleShadingEnable;
            0.0f,                                                     // float minSampleShading;
            nullptr,                                                  // const VkSampleMask* pSampleMask;
            VK_FALSE,                                                 // VkBool32 alphaToCoverageEnable;
            VK_FALSE                                                  // VkBool32 alphaToOneEnable;
        };

        graphicsPipeline.setDefaultDepthStencilState()
            .setDefaultRasterizationState()
            .setDefaultColorBlendState()
            .setupVertexInputState()
            .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, *renderPass, 0u, vertexModule)
            .setupFragmentShaderState(pipelineLayout, *renderPass, 0u, fragModule, nullptr, &multisampleStateParams)
            .setupFragmentOutputState(*renderPass, 0u, nullptr, &multisampleStateParams)
            .setMonolithicPipelineLayout(pipelineLayout)
            .buildPipeline();
    }

    // Command buffer
    Move<VkCommandPool> cmdPool =
        createCommandPool(vkd, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueIndex);
    Move<VkCommandBuffer> cmdBufferPtr =
        allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto cmdBuffer = cmdBufferPtr.get();

    // Execute upload commands
    {
        beginCommandBuffer(vkd, cmdBuffer);

        renderPass.begin(vkd, cmdBuffer, renderArea, srcClearColor);

        graphicsPipeline.bind(cmdBuffer);

        vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);

        vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), VK_SHADER_STAGE_FRAGMENT_BIT, 0u, pushConstantSize,
                             &pushConstantData);

        vkd.cmdDraw(cmdBuffer, de::sizeU32(vertices), 1u, 0u, 0u);

        renderPass.end(vkd, cmdBuffer);

        endCommandBuffer(vkd, cmdBuffer);
        submitCommandsAndWait(vkd, device, queue, cmdBuffer);
    }

    context.resetCommandPoolForVKSC(device, *cmdPool);

    // Multisampled 2D image has been rendered
    // Now, resolve multisampled 2D to a 3D image

    // Resolve region - full
    const auto colorSubresourceLayers = makeDefaultImageSubresourceLayers();
    const auto resolveRegionOffset    = makeOffset3D(0u, 0u, 0u);

    const VkImageResolve resolveRegion = {
        colorSubresourceLayers, // VkImageSubresourceLayers	srcSubresource
        resolveRegionOffset,    // VkOffset3D					srcOffset
        colorSubresourceLayers, // VkImageSubresourceLayers	dstSubresource
        resolveRegionOffset,    // VkOffset3D					dstOffset
        msImageExtent           // VkExtent3D					extent
    };

    const VkBufferImageCopy copyRegion = {
        0u,                                        // VkDeviceSize				bufferOffset
        0u,                                        // uint32_t					bufferRowLength
        0u,                                        // uint32_t					bufferImageHeight
        colorSubresourceLayers,                    // VkImageSubresourceLayers	imageSubresource
        resolveRegionOffset,                       // VkOffset3D					imageOffset
        makeExtent3D(size.x(), size.y(), size.z()) // VkExtent3D					imageExtent
    };

    // Output buffer
    const VkDeviceSize resultBufferSize = static_cast<VkDeviceSize>(
        static_cast<uint32_t>(getPixelSize(mapVkFormat(caseDef.colorFormat))) * size.x() * size.y() * size.z());
    const auto resultBufferInfo = makeBufferCreateInfo(resultBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const BufferWithMemory resultBuffer{vkd, device, alloc, resultBufferInfo, MemoryRequirement::HostVisible};

    const auto srcImageBarrier = makeImageMemoryBarrier(
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, multisampledImage.get(), colorSubresourceRange);
    const auto dstImageBarrier1 =
        makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, dst3dImage.get(), colorSubresourceRange);

    const auto dstImageBarrier2 = makeImageMemoryBarrier(
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst3dImage.get(), colorSubresourceRange);

    const auto dstImageBarrier3 = makeImageMemoryBarrier(
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, dst3dImage.get(), colorSubresourceRange);

    // Execute resolve commands
    {
        beginCommandBuffer(vkd, cmdBuffer);

        vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                               (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1u, &srcImageBarrier);
        vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                               (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1u, &dstImageBarrier1);

        vkd.cmdClearColorImage(cmdBuffer, dst3dImage.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               &dstClearColorValue.color, 1u, &colorSubresourceRange);

        vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                               (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1u, &dstImageBarrier3);

        vkd.cmdResolveImage(cmdBuffer, multisampledImage.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst3dImage.get(),
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &resolveRegion);

        vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                               (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1u, &dstImageBarrier2);

        vkd.cmdCopyImageToBuffer(cmdBuffer, dst3dImage.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, resultBuffer.get(),
                                 1u, &copyRegion);

        endCommandBuffer(vkd, cmdBuffer);
        submitCommandsAndWait(vkd, device, queue, cmdBuffer);
    }

    // Get results
    const auto &resultBufferAlloc = resultBuffer.getAllocation();
    invalidateAlloc(vkd, device, resultBufferAlloc);

    const auto resultsBufferPtr =
        reinterpret_cast<const char *>(resultBufferAlloc.getHostPtr()) + resultBufferAlloc.getOffset();

    const tcu::ConstPixelBufferAccess resultPixels{mapVkFormat(caseDef.colorFormat), size.x(), size.y(), size.z(),
                                                   resultsBufferPtr};

    // Reference images against each depth slices of the 3d image
    const uint32_t numSlices3d         = static_cast<uint32_t>(size.z());
    const tcu::TextureFormat tcuFormat = mapVkFormat(caseDef.colorFormat);
    std::vector<tcu::TextureLevel> refImages(numSlices3d, {tcuFormat, size.x(), size.y()});

    // Initialize the reference images
    for (uint32_t z = 0; z < numSlices3d; ++z)
    {
        tcu::PixelBufferAccess refPixels = refImages[z].getAccess();

        if (z == 0)
        {
            for (uint32_t y = 0; y < height; ++y)
            {
                for (uint32_t x = 0; x < width; ++x)
                {
                    const auto &pixelSamples = sampleVals[y * width + x];

                    // Average resolve
                    tcu::Vec4 sum(0.0f);
                    for (const auto &sample : pixelSamples)
                        sum += sample;
                    refPixels.setPixel(sum / static_cast<float>(numSamples), x, y);
                }
            }
        }
        else
        {
            tcu::clear(refPixels, dstClearColor);
        }
    }

    // Verification
    for (uint32_t sliceNdx = 0u; sliceNdx < numSlices3d; sliceNdx++)
    {
        tcu::ConstPixelBufferAccess resultImageSlice(tcuFormat, size.x(), size.y(), 1u,
                                                     resultPixels.getPixelPtr(0, 0, sliceNdx));
        const std::string imageSetName = "Result_" + de::toString(sliceNdx);
        if (!tcu::floatThresholdCompare(context.getTestContext().getLog(), imageSetName.c_str(),
                                        "Image comparison result", refImages[sliceNdx].getAccess(), resultImageSlice,
                                        tcu::Vec4(0.01f), tcu::COMPARE_LOG_ON_ERROR))
            return tcu::TestStatus::fail("Fail");
    }

    return tcu::TestStatus::pass("Pass");
}

} // namespace Image3d

namespace StorageImage
{

void initPrograms(SourceCollections &programCollection, const CaseDef caseDef)
{
    // Vertex & fragment

    addSimpleVertexAndFragmentPrograms(programCollection, caseDef);

    // Compute
    {
        const std::string imageTypeStr =
            getShaderMultisampledImageType(mapVkFormat(caseDef.colorFormat), caseDef.numLayers);
        const std::string formatQualifierStr = getShaderImageFormatQualifier(mapVkFormat(caseDef.colorFormat));
        const std::string signednessPrefix   = isUintFormat(caseDef.colorFormat) ? "u" :
                                               isIntFormat(caseDef.colorFormat)  ? "i" :
                                                                                   "";
        const std::string gvec4Expr          = signednessPrefix + "vec4";
        const std::string texelCoordStr      = (caseDef.numLayers == 1 ? "ivec2(gx, gy)" : "ivec3(gx, gy, gz)");

        std::ostringstream src;
        src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
            << "layout(local_size_x = 1) in;\n"
            << "layout(set = 0, binding = 0, " << formatQualifierStr << ") uniform " << imageTypeStr << " u_msImage;\n"
            << "\n"
            << "void main(void)\n"
            << "{\n"
            << "    int gx = int(gl_GlobalInvocationID.x);\n"
            << "    int gy = int(gl_GlobalInvocationID.y);\n"
            << "    int gz = int(gl_GlobalInvocationID.z);\n"
            << "\n"
            << "    " << gvec4Expr << " prevColor = imageLoad(u_msImage, " << texelCoordStr << ", 0);\n"
            << "    for (int sampleNdx = 1; sampleNdx < " << caseDef.numSamples << "; ++sampleNdx) {\n"
            << "        " << gvec4Expr << " color = imageLoad(u_msImage, " << texelCoordStr << ", sampleNdx);\n"
            << "        imageStore(u_msImage, " << texelCoordStr << ", sampleNdx, prevColor);\n"
            << "        prevColor = color;\n"
            << "    }\n"
            << "    imageStore(u_msImage, " << texelCoordStr << ", 0, prevColor);\n"
            << "}\n";

        programCollection.glslSources.add("comp") << glu::ComputeSource(src.str());
    }
}

//! Render a MS image, resolve it, and copy result to resolveBuffer.
void renderAndResolve(Context &context, const CaseDef &caseDef, const VkBuffer resolveBuffer, const bool useComputePass)
{
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkDevice device           = context.getDevice();
    const VkQueue queue             = context.getUniversalQueue();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = context.getDefaultAllocator();

    // Multisampled color image
    const Unique<VkImage> colorImage(makeImage(vk, device, caseDef.colorFormat, caseDef.renderSize, caseDef.numLayers,
                                               caseDef.numSamples,
                                               VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
                                                   VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT));
    const UniquePtr<Allocation> colorImageAlloc(bindImage(vk, device, allocator, *colorImage, MemoryRequirement::Any));

    const Unique<VkImage> resolveImage(makeImage(vk, device, caseDef.colorFormat, caseDef.renderSize, caseDef.numLayers,
                                                 VK_SAMPLE_COUNT_1_BIT,
                                                 VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT));
    const UniquePtr<Allocation> resolveImageAlloc(
        bindImage(vk, device, allocator, *resolveImage, MemoryRequirement::Any));

    const Unique<VkCommandPool> cmdPool(
        createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
    const Unique<VkCommandBuffer> cmdBuffer(makeCommandBuffer(vk, device, *cmdPool));

    // Working image barrier, we change it based on which rendering stages were executed so far.
    VkImageMemoryBarrier colorImageBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,          // VkStructureType sType;
        nullptr,                                         // const void* pNext;
        (VkAccessFlags)0,                                // VkAccessFlags outputMask;
        (VkAccessFlags)0,                                // VkAccessFlags inputMask;
        VK_IMAGE_LAYOUT_UNDEFINED,                       // VkImageLayout oldLayout;
        VK_IMAGE_LAYOUT_UNDEFINED,                       // VkImageLayout newLayout;
        VK_QUEUE_FAMILY_IGNORED,                         // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                         // uint32_t destQueueFamilyIndex;
        *colorImage,                                     // VkImage image;
        makeColorSubresourceRange(0, caseDef.numLayers), // VkImageSubresourceRange subresourceRange;
    };

    // Pass 1: Render an image
    {
        renderMultisampledImage(context, caseDef, *colorImage);

        colorImageBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        colorImageBarrier.oldLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    // Pass 2: Compute shader
    if (useComputePass)
    {
        // Descriptors

        Unique<VkDescriptorSetLayout> descriptorSetLayout(
            DescriptorSetLayoutBuilder()
                .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                .build(vk, device));

        Unique<VkDescriptorPool> descriptorPool(
            DescriptorPoolBuilder()
                .addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1u)
                .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

        const Unique<VkImageView> colorImageView(makeImageView(
            vk, device, *colorImage, (caseDef.numLayers == 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY),
            caseDef.colorFormat, makeColorSubresourceRange(0, caseDef.numLayers)));
        const Unique<VkDescriptorSet> descriptorSet(
            makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));
        const VkDescriptorImageInfo descriptorImageInfo =
            makeDescriptorImageInfo(VK_NULL_HANDLE, *colorImageView, VK_IMAGE_LAYOUT_GENERAL);

        DescriptorSetUpdateBuilder()
            .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                         VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descriptorImageInfo)
            .update(vk, device);

        const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));
        const Unique<VkShaderModule> shaderModule(
            createShaderModule(vk, device, context.getBinaryCollection().get("comp"), 0));
        const Unique<VkPipeline> pipeline(makeComputePipeline(vk, device, *pipelineLayout, *shaderModule));

        beginCommandBuffer(vk, *cmdBuffer);

        // Image layout for load/stores
        {
            colorImageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            colorImageBarrier.newLayout     = VK_IMAGE_LAYOUT_GENERAL;

            vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u,
                                  &colorImageBarrier);

            colorImageBarrier.srcAccessMask = colorImageBarrier.dstAccessMask;
            colorImageBarrier.oldLayout     = colorImageBarrier.newLayout;
        }
        // Dispatch
        {
            vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
            vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u,
                                     &descriptorSet.get(), 0u, nullptr);
            vk.cmdDispatch(*cmdBuffer, caseDef.renderSize.x(), caseDef.renderSize.y(), caseDef.numLayers);
        }

        endCommandBuffer(vk, *cmdBuffer);
        submitCommandsAndWait(vk, device, queue, *cmdBuffer);
    }

    // Resolve and verify the image
    {
        beginCommandBuffer(vk, *cmdBuffer);

        // Prepare for resolve
        {
            colorImageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            colorImageBarrier.newLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

            const VkImageMemoryBarrier barriers[] = {
                colorImageBarrier,
                {
                    VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,          // VkStructureType sType;
                    nullptr,                                         // const void* pNext;
                    (VkAccessFlags)0,                                // VkAccessFlags outputMask;
                    VK_ACCESS_TRANSFER_WRITE_BIT,                    // VkAccessFlags inputMask;
                    VK_IMAGE_LAYOUT_UNDEFINED,                       // VkImageLayout oldLayout;
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,            // VkImageLayout newLayout;
                    VK_QUEUE_FAMILY_IGNORED,                         // uint32_t srcQueueFamilyIndex;
                    VK_QUEUE_FAMILY_IGNORED,                         // uint32_t destQueueFamilyIndex;
                    *resolveImage,                                   // VkImage image;
                    makeColorSubresourceRange(0, caseDef.numLayers), // VkImageSubresourceRange subresourceRange;
                },
            };

            const VkPipelineStageFlags srcStageMask =
                (colorImageBarrier.srcAccessMask == VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT) ?
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT :
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

            vk.cmdPipelineBarrier(*cmdBuffer, srcStageMask, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u,
                                  nullptr, DE_LENGTH_OF_ARRAY(barriers), barriers);

            colorImageBarrier.srcAccessMask = colorImageBarrier.dstAccessMask;
            colorImageBarrier.oldLayout     = colorImageBarrier.newLayout;
        }
        // Resolve the image
        {
            const VkImageResolve resolveRegion = {
                makeColorSubresourceLayers(0, caseDef.numLayers), // VkImageSubresourceLayers    srcSubresource;
                makeOffset3D(0, 0, 0),                            // VkOffset3D                  srcOffset;
                makeColorSubresourceLayers(0, caseDef.numLayers), // VkImageSubresourceLayers    dstSubresource;
                makeOffset3D(0, 0, 0),                            // VkOffset3D                  dstOffset;
                makeExtent3D(caseDef.renderSize.x(), caseDef.renderSize.y(), 1u), // VkExtent3D                  extent;
            };

            vk.cmdResolveImage(*cmdBuffer, *colorImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *resolveImage,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &resolveRegion);
        }

        copyImageToBuffer(vk, *cmdBuffer, *resolveImage, resolveBuffer, caseDef.renderSize,
                          VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, caseDef.numLayers);

        endCommandBuffer(vk, *cmdBuffer);
        submitCommandsAndWait(vk, device, queue, *cmdBuffer);
    }
}

//! Exact image compare, but allow for some error when color format is integer.
bool compareImages(tcu::TestLog &log, const CaseDef &caseDef, const tcu::ConstPixelBufferAccess layeredReferenceImage,
                   const tcu::ConstPixelBufferAccess layeredActualImage)
{
    DE_ASSERT(caseDef.numSamples > 1);

    const Vec4 goodColor      = Vec4(0.0f, 1.0f, 0.0f, 1.0f);
    const Vec4 badColor       = Vec4(1.0f, 0.0f, 0.0f, 1.0f);
    const bool isAnyIntFormat = isIntFormat(caseDef.colorFormat) || isUintFormat(caseDef.colorFormat);

    // There should be no mismatched pixels for non-integer formats. Otherwise we may get a wrong color in a location where sample coverage isn't exactly 0 or 1.
    const int badPixelTolerance = (isAnyIntFormat ? 2 * caseDef.renderSize.x() : 0);
    int goodLayers              = 0;

    for (int layerNdx = 0; layerNdx < caseDef.numLayers; ++layerNdx)
    {
        const tcu::ConstPixelBufferAccess referenceImage =
            tcu::getSubregion(layeredReferenceImage, 0, 0, layerNdx, caseDef.renderSize.x(), caseDef.renderSize.y(), 1);
        const tcu::ConstPixelBufferAccess actualImage =
            tcu::getSubregion(layeredActualImage, 0, 0, layerNdx, caseDef.renderSize.x(), caseDef.renderSize.y(), 1);
        const std::string imageName = "color layer " + de::toString(layerNdx);

        tcu::TextureLevel errorMaskStorage(tcu::TextureFormat(tcu::TextureFormat::RGB, tcu::TextureFormat::UNORM_INT8),
                                           caseDef.renderSize.x(), caseDef.renderSize.y());
        tcu::PixelBufferAccess errorMask = errorMaskStorage.getAccess();
        int numBadPixels                 = 0;

        for (int y = 0; y < caseDef.renderSize.y(); ++y)
            for (int x = 0; x < caseDef.renderSize.x(); ++x)
            {
                if (isAnyIntFormat && (referenceImage.getPixelInt(x, y) == actualImage.getPixelInt(x, y)))
                    errorMask.setPixel(goodColor, x, y);
                else if (referenceImage.getPixel(x, y) == actualImage.getPixel(x, y))
                    errorMask.setPixel(goodColor, x, y);
                else
                {
                    ++numBadPixels;
                    errorMask.setPixel(badColor, x, y);
                }
            }

        if (numBadPixels <= badPixelTolerance)
        {
            ++goodLayers;

            log << tcu::TestLog::ImageSet(imageName, imageName) << tcu::TestLog::Image("Result", "Result", actualImage)
                << tcu::TestLog::EndImageSet;
        }
        else
        {
            log << tcu::TestLog::ImageSet(imageName, imageName) << tcu::TestLog::Image("Result", "Result", actualImage)
                << tcu::TestLog::Image("Reference", "Reference", referenceImage)
                << tcu::TestLog::Image("ErrorMask", "Error mask", errorMask) << tcu::TestLog::EndImageSet;
        }
    }

    if (goodLayers == caseDef.numLayers)
    {
        log << tcu::TestLog::Message << "All rendered images are correct." << tcu::TestLog::EndMessage;
        return true;
    }
    else
    {
        log << tcu::TestLog::Message << "FAILED: Some rendered images were incorrect." << tcu::TestLog::EndMessage;
        return false;
    }
}

void checkSupport(Context &context, const CaseDef caseDef)
{
    const VkImageUsageFlags colorImageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

    checkImageFormatRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), caseDef.numSamples,
                                 caseDef.colorFormat, colorImageUsage);
    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          caseDef.pipelineConstructionType);
}

tcu::TestStatus test(Context &context, const CaseDef caseDef)
{
    const DeviceInterface &vk = context.getDeviceInterface();
    const VkDevice device     = context.getDevice();
    Allocator &allocator      = context.getDefaultAllocator();

    {
        tcu::TestLog &log = context.getTestContext().getLog();
        log << tcu::LogSection("Description", "") << tcu::TestLog::Message
            << "Rendering to a multisampled image. Image will be processed with a compute shader using OpImageRead and "
               "OpImageWrite."
            << tcu::TestLog::EndMessage << tcu::TestLog::Message
            << "Expecting the processed image to be roughly the same as the input image (deviation may occur for "
               "integer formats)."
            << tcu::TestLog::EndMessage << tcu::TestLog::EndSection;
    }

    // Host-readable buffer
    const VkDeviceSize resolveBufferSize = caseDef.renderSize.x() * caseDef.renderSize.y() * caseDef.numLayers *
                                           tcu::getPixelSize(mapVkFormat(caseDef.colorFormat));
    const Unique<VkBuffer> resolveImageOneBuffer(
        makeBuffer(vk, device, resolveBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
    const UniquePtr<Allocation> resolveImageOneBufferAlloc(
        bindBuffer(vk, device, allocator, *resolveImageOneBuffer, MemoryRequirement::HostVisible));
    const Unique<VkBuffer> resolveImageTwoBuffer(
        makeBuffer(vk, device, resolveBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
    const UniquePtr<Allocation> resolveImageTwoBufferAlloc(
        bindBuffer(vk, device, allocator, *resolveImageTwoBuffer, MemoryRequirement::HostVisible));

    zeroBuffer(vk, device, *resolveImageOneBufferAlloc, resolveBufferSize);
    zeroBuffer(vk, device, *resolveImageTwoBufferAlloc, resolveBufferSize);

    // Render: repeat the same rendering twice to avoid non-essential API calls and layout transitions (e.g. copy).
    {
        renderAndResolve(context, caseDef, *resolveImageOneBuffer, false); // Pass 1: render a basic multisampled image
        renderAndResolve(context, caseDef, *resolveImageTwoBuffer,
                         true); // Pass 2: the same but altered with a compute shader
    }

    // Verify
    {
        invalidateAlloc(vk, device, *resolveImageOneBufferAlloc);
        invalidateAlloc(vk, device, *resolveImageTwoBufferAlloc);

        const tcu::PixelBufferAccess layeredImageOne(mapVkFormat(caseDef.colorFormat), caseDef.renderSize.x(),
                                                     caseDef.renderSize.y(), caseDef.numLayers,
                                                     resolveImageOneBufferAlloc->getHostPtr());
        const tcu::ConstPixelBufferAccess layeredImageTwo(mapVkFormat(caseDef.colorFormat), caseDef.renderSize.x(),
                                                          caseDef.renderSize.y(), caseDef.numLayers,
                                                          resolveImageTwoBufferAlloc->getHostPtr());

        // Check all layers
        if (!compareImages(context.getTestContext().getLog(), caseDef, layeredImageOne, layeredImageTwo))
            return tcu::TestStatus::fail("Rendered images are not correct");
    }

    return tcu::TestStatus::pass("OK");
}

} // namespace StorageImage

namespace StandardSamplePosition
{

void initPrograms(SourceCollections &programCollection, const CaseDef caseDef)
{
    // Pass 1: Render to texture

    addSimpleVertexAndFragmentPrograms(programCollection, caseDef);

    // Pass 2: Sample texture

    // Vertex shader
    {
        std::ostringstream src;
        src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
            << "\n"
            << "layout(location = 0) in  vec4  in_position;\n"
            << "\n"
            << "out gl_PerVertex {\n"
            << "    vec4 gl_Position;\n"
            << "};\n"
            << "\n"
            << "void main(void)\n"
            << "{\n"
            << "    gl_Position = in_position;\n"
            << "}\n";

        programCollection.glslSources.add("sample_vert") << glu::VertexSource(src.str());
    }

    // Fragment shader
    {
        std::ostringstream src;
        src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
            << "\n"
            << "layout(location = 0) out uint o_status;\n"
            << "\n"
            << "layout(set = 0, binding = 0) uniform sampler2DMS colorTexture;\n"
            << "\n"
            << "void main(void)\n"
            << "{\n"
            << "    uint result = 0;\n"
            << "    vec4 a, b;\n\n"
            << "\n";

        for (uint32_t sampleNdx = 0; sampleNdx < (uint32_t)caseDef.numSamples; sampleNdx++)
        {
            Vec4 expectedColor = sampleIndexToColor(sampleNdx);

            src << "    a = texelFetch(colorTexture, ivec2(0,0), " << sampleNdx
                << ");\n"
                   "    b = vec4("
                << expectedColor.x() << ", " << expectedColor.y() << ", " << expectedColor.z()
                << ", 1.0);\n"
                   "    if (abs(a.x - b.x) > 0.1 || abs(a.y - b.y) > 0.1 || abs(a.z - b.z) > 0.1) result++;\n";
        }

        src << "\n"
            << "    o_status = result;\n"
            << "}\n";

        programCollection.glslSources.add("sample_frag") << glu::FragmentSource(src.str());
    }
}

void checkSupport(Context &context, const CaseDef caseDef)
{
    const VkImageUsageFlags colorImageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    const VkPhysicalDeviceProperties props =
        getPhysicalDeviceProperties(context.getInstanceInterface(), context.getPhysicalDevice());

    checkImageFormatRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), caseDef.numSamples,
                                 caseDef.colorFormat, colorImageUsage);

    if (!props.limits.standardSampleLocations)
        TCU_THROW(NotSupportedError, "Device does not support standard sample locations.");

    if (caseDef.numSamples == VK_SAMPLE_COUNT_32_BIT || caseDef.numSamples == VK_SAMPLE_COUNT_64_BIT)
    {
        TCU_THROW(InternalError, "Standard does not define sample positions for 32x or 64x multisample modes");
    }

    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          caseDef.pipelineConstructionType);
}

tcu::TestStatus test(Context &context, const CaseDef caseDef)
{
    const InstanceInterface &vki          = context.getInstanceInterface();
    const DeviceInterface &vk             = context.getDeviceInterface();
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
    const VkDevice device                 = context.getDevice();
    const VkQueue queue                   = context.getUniversalQueue();
    const uint32_t queueFamilyIndex       = context.getUniversalQueueFamilyIndex();
    Allocator &allocator                  = context.getDefaultAllocator();

    const VkImageUsageFlags colorImageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    {
        tcu::TestLog &log = context.getTestContext().getLog();
        log << tcu::LogSection("Description", "") << tcu::TestLog::Message
            << "Rendering to a multisampled image. Expecting samples to have specified colors."
            << tcu::TestLog::EndMessage << tcu::TestLog::Message
            << "Sampling from the texture with texelFetch (OpImageFetch)." << tcu::TestLog::EndMessage
            << tcu::TestLog::EndSection;
    }

    // Multisampled color image
    const Unique<VkImage> colorImage(makeImage(vk, device, caseDef.colorFormat, caseDef.renderSize, caseDef.numLayers,
                                               caseDef.numSamples, colorImageUsage));
    const UniquePtr<Allocation> colorImageAlloc(bindImage(vk, device, allocator, *colorImage, MemoryRequirement::Any));

    const Unique<VkCommandPool> cmdPool(
        createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
    const Unique<VkCommandBuffer> cmdBuffer(makeCommandBuffer(vk, device, *cmdPool));

    // Step 1: Render to texture
    {
        renderMultisampledImage(context, caseDef, *colorImage);
    }

    // Step 2: Sample texture
    {
        // Color image view
        const VkImageViewType colorImageViewType =
            (caseDef.numLayers == 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY);
        const Unique<VkImageView> colorImageView(makeImageView(vk, device, *colorImage, colorImageViewType,
                                                               caseDef.colorFormat,
                                                               makeColorSubresourceRange(0, caseDef.numLayers)));
        const Unique<VkSampler> colorSampler(makeSampler(vk, device));

        // Checksum image
        const VkFormat checksumFormat = VK_FORMAT_R8_UINT;
        const Unique<VkImage> checksumImage(
            makeImage(vk, device, checksumFormat, caseDef.renderSize, 1u, VK_SAMPLE_COUNT_1_BIT,
                      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT));
        const UniquePtr<Allocation> checksumImageAlloc(
            bindImage(vk, device, allocator, *checksumImage, MemoryRequirement::Any));
        const Unique<VkImageView> checksumImageView(makeImageView(vk, device, *checksumImage, VK_IMAGE_VIEW_TYPE_2D,
                                                                  checksumFormat, makeColorSubresourceRange(0, 1)));

        // Checksum buffer (for host reading)
        const VkDeviceSize checksumBufferSize =
            caseDef.renderSize.x() * caseDef.renderSize.y() * tcu::getPixelSize(mapVkFormat(checksumFormat));
        const Unique<VkBuffer> checksumBuffer(
            makeBuffer(vk, device, checksumBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
        const UniquePtr<Allocation> checksumBufferAlloc(
            bindBuffer(vk, device, allocator, *checksumBuffer, MemoryRequirement::HostVisible));

        zeroBuffer(vk, device, *checksumBufferAlloc, checksumBufferSize);

        // Vertex buffer
        const std::vector<Vertex4RGBA> vertices = genFullQuadVertices();
        const VkDeviceSize vertexBufferSize     = sizeInBytes(vertices);
        const Unique<VkBuffer> vertexBuffer(
            makeBuffer(vk, device, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT));
        const UniquePtr<Allocation> vertexBufferAlloc(
            bindBuffer(vk, device, allocator, *vertexBuffer, MemoryRequirement::HostVisible));

        {
            deMemcpy(vertexBufferAlloc->getHostPtr(), &vertices[0], static_cast<std::size_t>(vertexBufferSize));
            flushAlloc(vk, device, *vertexBufferAlloc);
        }

        // Descriptors
        // \note OpImageFetch doesn't use a sampler, but in GLSL texelFetch needs a sampler2D which translates to a combined image sampler in Vulkan.

        const Unique<VkDescriptorSetLayout> descriptorSetLayout(
            DescriptorSetLayoutBuilder()
                .addSingleSamplerBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT,
                                         &colorSampler.get())
                .build(vk, device));

        const Unique<VkDescriptorPool> descriptorPool(
            DescriptorPoolBuilder()
                .addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

        const Unique<VkDescriptorSet> descriptorSet(
            makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));
        const VkDescriptorImageInfo imageDescriptorInfo =
            makeDescriptorImageInfo(VK_NULL_HANDLE, *colorImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        DescriptorSetUpdateBuilder()
            .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageDescriptorInfo)
            .update(vk, device);

        const ShaderWrapper vertexModule(
            ShaderWrapper(vk, device, context.getBinaryCollection().get("sample_vert"), 0u));
        const ShaderWrapper fragmentModule(
            ShaderWrapper(vk, device, context.getBinaryCollection().get("sample_frag"), 0u));
        RenderPassWrapper renderPass(
            makeSimpleRenderPass(vk, device, caseDef.pipelineConstructionType, checksumFormat));
        renderPass.createFramebuffer(vk, device, 1u, &checksumImage.get(), &checksumImageView.get(),
                                     static_cast<uint32_t>(caseDef.renderSize.x()),
                                     static_cast<uint32_t>(caseDef.renderSize.y()));
        const PipelineLayoutWrapper pipelineLayout(caseDef.pipelineConstructionType, vk, device, *descriptorSetLayout);
        const bool isMonolithic(caseDef.pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC);
        std::vector<PipelineSp> pipelinesSp;
        std::vector<GraphicsPipelineWrapper> pipelineWrapper;

        if (isMonolithic)
        {
            pipelinesSp =
                makeGraphicsPipelines(vk, device, 1u, *pipelineLayout, *renderPass, vertexModule, fragmentModule,
                                      caseDef.renderSize, VK_SAMPLE_COUNT_1_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
        }
        else
        {
            pipelineWrapper.emplace_back(vki, vk, physicalDevice, device, context.getDeviceExtensions(),
                                         caseDef.pipelineConstructionType);
            preparePipelineWrapper(pipelineWrapper.back(), 0u, pipelineLayout, *renderPass, vertexModule,
                                   fragmentModule, caseDef.renderSize, VK_SAMPLE_COUNT_1_BIT,
                                   VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
        }

        beginCommandBuffer(vk, *cmdBuffer);

        // Prepare for sampling in the fragment shader
        {
            const VkImageMemoryBarrier barriers[] = {
                {
                    VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,          // VkStructureType sType;
                    nullptr,                                         // const void* pNext;
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,            // VkAccessFlags outputMask;
                    VK_ACCESS_SHADER_READ_BIT,                       // VkAccessFlags inputMask;
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,        // VkImageLayout oldLayout;
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,        // VkImageLayout newLayout;
                    VK_QUEUE_FAMILY_IGNORED,                         // uint32_t srcQueueFamilyIndex;
                    VK_QUEUE_FAMILY_IGNORED,                         // uint32_t destQueueFamilyIndex;
                    *colorImage,                                     // VkImage image;
                    makeColorSubresourceRange(0, caseDef.numLayers), // VkImageSubresourceRange subresourceRange;
                },
            };

            vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u, 0u, nullptr, 0u, nullptr,
                                  DE_LENGTH_OF_ARRAY(barriers), barriers);
        }

        renderPass.begin(vk, *cmdBuffer, makeRect2D(0, 0, caseDef.renderSize.x(), caseDef.renderSize.y()),
                         tcu::UVec4(0u));

        if (isMonolithic)
            vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, **pipelinesSp.back());
        else
            pipelineWrapper.back().bind(*cmdBuffer);
        vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u,
                                 &descriptorSet.get(), 0u, nullptr);
        {
            const VkDeviceSize vertexBufferOffset = 0ull;
            vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
        }

        vk.cmdDraw(*cmdBuffer, static_cast<uint32_t>(vertices.size()), 1u, 0u, 0u);
        renderPass.end(vk, *cmdBuffer);

        copyImageToBuffer(vk, *cmdBuffer, *checksumImage, *checksumBuffer, caseDef.renderSize);

        endCommandBuffer(vk, *cmdBuffer);
        submitCommandsAndWait(vk, device, queue, *cmdBuffer);

        // Verify result

        {
            invalidateAlloc(vk, device, *checksumBufferAlloc);

            const tcu::ConstPixelBufferAccess access(mapVkFormat(checksumFormat), caseDef.renderSize.x(),
                                                     caseDef.renderSize.y(), 1, checksumBufferAlloc->getHostPtr());

            uint32_t result = access.getPixelUint(0, 0).x();

            if (result)
                return tcu::TestStatus::fail(std::to_string(result) + " multisamples have unexpected color.");
        }
    }

    return tcu::TestStatus::pass("OK");
}

} // namespace StandardSamplePosition

namespace SamplesMappingOrder
{

void initPrograms(SourceCollections &programCollection, const CaseDef caseDef)
{
    std::string vert = "#version 450\n"
                       "void main(void) {\n"
                       "    gl_Position = vec4(float(gl_VertexIndex & 1) * 2.0 - 1.0,\n"
                       "                       float((gl_VertexIndex >> 1) & 1) * 2.0 - 1.0, 0.0, 1.0);\n"
                       "}\n";
    programCollection.glslSources.add("vert") << glu::VertexSource(vert);

    std::string frag = "#version 450\n"
                       "layout(location = 0) out vec4 outColor;\n"
                       "void main(void) {\n"
                       // normalize coordinates and sample id to <0; 1> range
                       "    outColor = vec4(gl_FragCoord.xy / 16.0, gl_SampleID / 64.0, 1.0);\n"
                       "}\n";
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag);

    std::string comp = "#version 450\n"
                       "#extension GL_EXT_samplerless_texture_functions : enable\n"
                       "layout(local_size_x = 16, local_size_y = 16) in;\n"
                       "layout(set = 0, binding = 0) uniform texture2DMS inputImage;\n"
                       "layout(set = 0, binding = 1) buffer Data { float v[]; };\n"
                       "void main()\n"
                       "{\n"
                       "  ivec2 uv = ivec2(gl_GlobalInvocationID.xy);\n"
                       "  float samplesMulSum = 0.1;\n"
                       "  for (int i = 1 ; i < ${NUM_SAMPLES} ; i++) {\n"
                       "      vec4 currSample = texelFetch(inputImage, uv, i);\n"
                       // to make sure samples are in same order for each fragment we calculate
                       // weighted sum of z component of all samples and later on host check if
                       // same value was caluclated for all fragments
                       "      samplesMulSum += float(i) * currSample.z;\n"
                       "  }\n"
                       "  v[gl_LocalInvocationIndex] = samplesMulSum;\n"
                       "}\n";
    std::string_view numSamples = "${NUM_SAMPLES}";
    comp.replace(comp.find(numSamples), numSamples.length(), std::to_string(caseDef.numSamples));
    programCollection.glslSources.add("comp") << glu::ComputeSource(comp);
}

void checkSupport(Context &context, const CaseDef caseDef)
{
    const VkImageUsageFlags colorImageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    checkImageFormatRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), caseDef.numSamples,
                                 caseDef.colorFormat, colorImageUsage);

    if (!context.getDeviceFeatures().shaderStorageImageMultisample)
        TCU_THROW(NotSupportedError, "Device does not support shaderStorageImageMultisample.");

    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          caseDef.pipelineConstructionType);
}

tcu::TestStatus test(Context &context, const CaseDef caseDef)
{
    const auto &vki    = context.getInstanceInterface();
    const auto &vk     = context.getDeviceInterface();
    const auto &device = context.getDevice();
    const auto pd      = context.getPhysicalDevice();
    auto &allocator    = context.getDefaultAllocator();
    VkQueue queue      = context.getUniversalQueue();

    const auto format  = caseDef.colorFormat;
    const auto samples = caseDef.numSamples;
    const int32_t w    = caseDef.renderSize.x();
    const int32_t h    = caseDef.renderSize.y();

    VkImageCreateInfo imageCreateInfo = initVulkanStructure();
    imageCreateInfo.imageType         = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format            = format;
    imageCreateInfo.extent            = makeExtent3D(w, h, 1);
    imageCreateInfo.mipLevels         = 1u;
    imageCreateInfo.arrayLayers       = 1u;
    imageCreateInfo.samples           = samples;
    imageCreateInfo.usage             = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    // create multisampled image for color attachment
    ImageWithMemory imageWithMemory(vk, device, allocator, imageCreateInfo, MemoryRequirement::Local);
    const auto cSRR = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    auto imageView  = makeImageView(vk, device, *imageWithMemory, VK_IMAGE_VIEW_TYPE_2D, format, cSRR);

    // create renderpass
    const VkAttachmentDescription attachment{0,
                                             format,
                                             samples,
                                             VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                             VK_ATTACHMENT_STORE_OP_STORE,
                                             VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                             VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                             VK_IMAGE_LAYOUT_UNDEFINED,
                                             VK_IMAGE_LAYOUT_GENERAL};
    const VkAttachmentReference colorAttachment{0, VK_IMAGE_LAYOUT_GENERAL};
    const VkSubpassDescription subpass{
        0, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, nullptr, 1, &colorAttachment, nullptr, nullptr, 0, nullptr};
    VkRenderPassCreateInfo renderpassCreateInfo = initVulkanStructure();
    renderpassCreateInfo.attachmentCount        = 1u;
    renderpassCreateInfo.pAttachments           = &attachment;
    renderpassCreateInfo.subpassCount           = 1u;
    renderpassCreateInfo.pSubpasses             = &subpass;
    RenderPassWrapper renderPass(caseDef.pipelineConstructionType, vk, device, &renderpassCreateInfo);

    // create framebuffer
    renderPass.createFramebuffer(vk, device, 1u, &*imageWithMemory, &*imageView, w, h);

    auto &bc              = context.getBinaryCollection();
    const auto vertModule = ShaderWrapper(vk, device, bc.get("vert"));
    const auto fragModule = ShaderWrapper(vk, device, bc.get("frag"));

    const std::vector<VkViewport> viewports(1u, makeViewport(caseDef.renderSize));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(caseDef.renderSize));
    PipelineLayoutWrapper graphicsPipelineLayout(caseDef.pipelineConstructionType, vk, device);

    // create ssbo buffer used in compute shader for partial verification
    const auto ssboSize = static_cast<VkDeviceSize>(w * h * sizeof(float));
    const auto ssboInfo =
        makeBufferCreateInfo(ssboSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    BufferWithMemory ssboBuffer(vk, device, allocator, ssboInfo, MemoryRequirement::HostVisible);

    VkPipelineColorBlendAttachmentState colorBlendAttachmentState;
    deMemset(&colorBlendAttachmentState, 0x00, sizeof(VkPipelineColorBlendAttachmentState));
    colorBlendAttachmentState.colorWriteMask = (VkColorComponentFlags)0xFu;

    VkPipelineVertexInputStateCreateInfo vertexInputState     = initVulkanStructure();
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = initVulkanStructure();
    inputAssemblyState.topology                               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    VkPipelineColorBlendStateCreateInfo colorBlendState = initVulkanStructure();
    colorBlendState.attachmentCount                     = 1;
    colorBlendState.pAttachments                        = &colorBlendAttachmentState;

    VkSampleMask sampleMask                               = 0xFF;
    VkPipelineMultisampleStateCreateInfo multisampleState = initVulkanStructure();
    multisampleState.rasterizationSamples                 = samples;
    multisampleState.minSampleShading                     = 1.0f;
    multisampleState.pSampleMask                          = &sampleMask;

    // create graphics pipeline
    GraphicsPipelineWrapper pipelineWrapper(vki, vk, pd, device, context.getDeviceExtensions(),
                                            caseDef.pipelineConstructionType);
    pipelineWrapper.setMonolithicPipelineLayout(graphicsPipelineLayout)
        .setDefaultRasterizationState()
        .setDefaultDepthStencilState()
        .setupVertexInputState(&vertexInputState, &inputAssemblyState)
        .setupPreRasterizationShaderState(viewports, scissors, graphicsPipelineLayout, *renderPass, 0u, vertModule)
        .setupFragmentShaderState(graphicsPipelineLayout, *renderPass, 0u, fragModule, nullptr, &multisampleState)
        .setupFragmentOutputState(*renderPass, 0u, &colorBlendState, &multisampleState)
        .buildPipeline();

    Move<VkShaderModule> compModule;
    Move<VkPipelineLayout> computePipelineLayout;
    Move<VkPipeline> computePipeline;

    // create descriptor set
    auto computeDescriptorSetLayout =
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device);
    auto computeDescriptorPool = DescriptorPoolBuilder()
                                     .addType(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
                                     .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
                                     .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    auto computeDescriptorSet = makeDescriptorSet(vk, device, *computeDescriptorPool, *computeDescriptorSetLayout);
    const VkDescriptorImageInfo imageDescriptorInfo =
        makeDescriptorImageInfo(VK_NULL_HANDLE, *imageView, VK_IMAGE_LAYOUT_GENERAL);
    const VkDescriptorBufferInfo bufferDescriptorInfo = makeDescriptorBufferInfo(*ssboBuffer, 0ull, ssboSize);
    DescriptorSetUpdateBuilder()
        .writeSingle(*computeDescriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                     VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &imageDescriptorInfo)
        .writeSingle(*computeDescriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptorInfo)
        .update(vk, device);

    // create compute pipeline
    VkPipelineLayoutCreateInfo layoutCreateInfo = initVulkanStructure();
    layoutCreateInfo.setLayoutCount             = 1u;
    layoutCreateInfo.pSetLayouts                = &*computeDescriptorSetLayout;
    computePipelineLayout                       = createPipelineLayout(vk, device, &layoutCreateInfo);
    compModule                                  = createShaderModule(vk, device, bc.get("comp"));
    computePipeline = makeComputePipeline(vk, device, *computePipelineLayout, 0, nullptr, *compModule, 0);

    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();

    auto cmdPool   = createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
    auto cmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    beginCommandBuffer(vk, *cmdBuffer);
    renderPass.begin(vk, *cmdBuffer, scissors.at(0u));
    pipelineWrapper.bind(*cmdBuffer);
    vk.cmdDraw(*cmdBuffer, 4u, 1u, 0u, 0u);
    renderPass.end(vk, *cmdBuffer);

    // wait for multisampled image
    auto barrier = makeMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, (VkDependencyFlags)0, 1u, &barrier, 0, 0, 0, 0);

    // read each sample using compute shader
    vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipelineLayout, 0u, 1u,
                             &*computeDescriptorSet, 0u, nullptr);
    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipeline);
    vk.cmdDispatch(*cmdBuffer, 1u, 1u, 1u);

    // wait for ssbo
    barrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                          (VkDependencyFlags)0, 1u, &barrier, 0, 0, 0, 0);

    endCommandBuffer(vk, *cmdBuffer);
    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    // get ssbo buffer
    invalidateAlloc(vk, device, ssboBuffer.getAllocation());
    const float *data = reinterpret_cast<float *>(ssboBuffer.getAllocation().getHostPtr());

    // in compute shader for each fragment we calculated value that is based on order of the samples;
    // here we need to check if same value was calculated for all fragments
    const float epsilon = 0.001f;
    for (int32_t i = 1; i < w * h; i++)
    {
        if (std::fabs(data[0] - data[i]) > epsilon)
            return tcu::TestStatus::fail(std::to_string(i));
    }

    return tcu::TestStatus::pass("OK");
}

} // namespace SamplesMappingOrder

std::string getSizeLayerString(const IVec2 &size, const int numLayers)
{
    std::ostringstream str;
    str << size.x() << "x" << size.y() << "_" << numLayers;
    return str.str();
}

std::string getSizeLayerString(const IVec3 &size, const int numLayers)
{
    std::ostringstream str;
    str << size.x() << "x" << size.y() << "x" << size.z() << "_" << numLayers;
    return str.str();
}

std::string getFormatString(const VkFormat format)
{
    std::string name(getFormatName(format));
    return de::toLower(name.substr(10));
}

void addTestCasesWithFunctions(tcu::TestCaseGroup *group, FunctionSupport1<CaseDef>::Function checkSupport,
                               FunctionPrograms1<CaseDef>::Function initPrograms,
                               FunctionInstance1<CaseDef>::Function testFunc,
                               PipelineConstructionType pipelineConstructionType)
{
    const IVec2 size[] = {
        IVec2(64, 64),
        IVec2(79, 31),
    };
    const int numLayers[]                 = {1, 4};
    const VkSampleCountFlagBits samples[] = {
        VK_SAMPLE_COUNT_2_BIT,  VK_SAMPLE_COUNT_4_BIT,  VK_SAMPLE_COUNT_8_BIT,
        VK_SAMPLE_COUNT_16_BIT, VK_SAMPLE_COUNT_32_BIT, VK_SAMPLE_COUNT_64_BIT,
    };
    const VkFormat format[] = {
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_R32_UINT,
        VK_FORMAT_R16G16_SINT,
        VK_FORMAT_R32G32B32A32_SFLOAT,
    };

    for (int sizeNdx = 0; sizeNdx < DE_LENGTH_OF_ARRAY(size); ++sizeNdx)
        for (int layerNdx = 0; layerNdx < DE_LENGTH_OF_ARRAY(numLayers); ++layerNdx)
        {
            MovePtr<tcu::TestCaseGroup> sizeLayerGroup(new tcu::TestCaseGroup(
                group->getTestContext(), getSizeLayerString(size[sizeNdx], numLayers[layerNdx]).c_str()));
            for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(format); ++formatNdx)
            {
                MovePtr<tcu::TestCaseGroup> formatGroup(
                    new tcu::TestCaseGroup(group->getTestContext(), getFormatString(format[formatNdx]).c_str()));
                for (int samplesNdx = 0; samplesNdx < DE_LENGTH_OF_ARRAY(samples); ++samplesNdx)
                {
                    std::ostringstream caseName;
                    caseName << "samples_" << getNumSamples(samples[samplesNdx]);

                    const CaseDef caseDef{
                        pipelineConstructionType, // PipelineConstructionType pipelineConstructionType;
                        size[sizeNdx],            // IVec2 renderSize;
                        numLayers[layerNdx],      // int numLayers;
                        format[formatNdx],        // VkFormat colorFormat;
                        samples[samplesNdx],      // VkSampleCountFlagBits numSamples;
                        false,                    // bool colorQuad;
                    };

                    addFunctionCaseWithPrograms(formatGroup.get(), caseName.str(), checkSupport, initPrograms, testFunc,
                                                caseDef);
                }
                sizeLayerGroup->addChild(formatGroup.release());
            }
            group->addChild(sizeLayerGroup.release());
        }
}

void addTestCasesWithFunctions3d(tcu::TestCaseGroup *group, FunctionSupport1<CaseDef3d>::Function checkSupport,
                                 FunctionPrograms1<CaseDef3d>::Function initPrograms,
                                 FunctionInstance1<CaseDef3d>::Function testFunc,
                                 PipelineConstructionType pipelineConstructionType)
{
    const IVec3 size[]                    = {IVec3(64, 64, 8)};
    const int numLayers[]                 = {1};
    const VkSampleCountFlagBits samples[] = {
        VK_SAMPLE_COUNT_2_BIT,  VK_SAMPLE_COUNT_4_BIT,  VK_SAMPLE_COUNT_8_BIT,
        VK_SAMPLE_COUNT_16_BIT, VK_SAMPLE_COUNT_32_BIT, VK_SAMPLE_COUNT_64_BIT,
    };
    const VkFormat format[] = {
        VK_FORMAT_R8G8B8A8_UNORM,
    };

    for (int sizeNdx = 0; sizeNdx < DE_LENGTH_OF_ARRAY(size); ++sizeNdx)
        for (int layerNdx = 0; layerNdx < DE_LENGTH_OF_ARRAY(numLayers); ++layerNdx)
        {
            MovePtr<tcu::TestCaseGroup> sizeLayerGroup(new tcu::TestCaseGroup(
                group->getTestContext(), getSizeLayerString(size[sizeNdx], numLayers[layerNdx]).c_str()));
            for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(format); ++formatNdx)
            {
                MovePtr<tcu::TestCaseGroup> formatGroup(
                    new tcu::TestCaseGroup(group->getTestContext(), getFormatString(format[formatNdx]).c_str()));
                for (int samplesNdx = 0; samplesNdx < DE_LENGTH_OF_ARRAY(samples); ++samplesNdx)
                {
                    std::ostringstream caseName;
                    caseName << "samples_" << getNumSamples(samples[samplesNdx]);

                    const CaseDef3d caseDef{
                        pipelineConstructionType, // PipelineConstructionType pipelineConstructionType;
                        size[sizeNdx],            // IVec3 renderSize;
                        numLayers[layerNdx],      // int numLayers;
                        format[formatNdx],        // VkFormat colorFormat;
                        samples[samplesNdx],      // VkSampleCountFlagBits numSamples;
                    };

                    addFunctionCaseWithPrograms(formatGroup.get(), caseName.str(), checkSupport, initPrograms, testFunc,
                                                caseDef);
                }
                sizeLayerGroup->addChild(formatGroup.release());
            }
            group->addChild(sizeLayerGroup.release());
        }
}

void addStandardSamplePositionTestCasesWithFunctions(tcu::TestCaseGroup *group,
                                                     FunctionSupport1<CaseDef>::Function checkSupport,
                                                     FunctionPrograms1<CaseDef>::Function initPrograms,
                                                     FunctionInstance1<CaseDef>::Function testFunc,
                                                     PipelineConstructionType pipelineConstructionType)
{
    const VkSampleCountFlagBits samples[] = {
        VK_SAMPLE_COUNT_2_BIT,  VK_SAMPLE_COUNT_4_BIT,  VK_SAMPLE_COUNT_8_BIT,
        VK_SAMPLE_COUNT_16_BIT, VK_SAMPLE_COUNT_32_BIT, VK_SAMPLE_COUNT_64_BIT,
    };
    const VkFormat format[] = {
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_R32G32B32A32_SFLOAT,
    };

    for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(format); ++formatNdx)
    {
        MovePtr<tcu::TestCaseGroup> formatGroup(
            new tcu::TestCaseGroup(group->getTestContext(), getFormatString(format[formatNdx]).c_str()));
        for (int samplesNdx = 0; samplesNdx < DE_LENGTH_OF_ARRAY(samples); ++samplesNdx)
        {
            std::ostringstream caseName;
            caseName << "samples_" << getNumSamples(samples[samplesNdx]);

            const CaseDef caseDef{
                pipelineConstructionType, // PipelineConstructionType pipelineConstructionType;
                IVec2(1, 1),              // IVec2 renderSize;
                1,                        // int numLayers;
                format[formatNdx],        // VkFormat colorFormat;
                samples[samplesNdx],      // VkSampleCountFlagBits numSamples;
                true,                     // bool colorQuad;
            };

            addFunctionCaseWithPrograms(formatGroup.get(), caseName.str(), checkSupport, initPrograms, testFunc,
                                        caseDef);
        }
        group->addChild(formatGroup.release());
    }
}

void addSamplesMappingOrderTestCasesWithFunctions(tcu::TestCaseGroup *group,
                                                  FunctionSupport1<CaseDef>::Function checkSupport,
                                                  FunctionPrograms1<CaseDef>::Function initPrograms,
                                                  FunctionInstance1<CaseDef>::Function testFunc,
                                                  PipelineConstructionType pipelineConstructionType)
{
    const VkSampleCountFlagBits samples[]{
        VK_SAMPLE_COUNT_2_BIT,  VK_SAMPLE_COUNT_4_BIT,  VK_SAMPLE_COUNT_8_BIT,
        VK_SAMPLE_COUNT_16_BIT, VK_SAMPLE_COUNT_32_BIT, VK_SAMPLE_COUNT_64_BIT,
    };

    for (auto s : samples)
    {
        std::string caseName = "samples_";
        caseName += std::to_string(getNumSamples(s));

        const CaseDef caseDef{
            pipelineConstructionType, // PipelineConstructionType pipelineConstructionType;
            IVec2(16, 16),            // IVec2 renderSize;
            1,                        // int numLayers;
            VK_FORMAT_R8G8B8A8_UNORM, // VkFormat colorFormat;
            s,                        // VkSampleCountFlagBits numSamples;
            true,                     // bool colorQuad;
        };

        addFunctionCaseWithPrograms(group, caseName, checkSupport, initPrograms, testFunc, caseDef);
    }
}

void createSampledImageTestsInGroup(tcu::TestCaseGroup *group, PipelineConstructionType pipelineConstructionType)
{
    addTestCasesWithFunctions(group, SampledImage::checkSupport, SampledImage::initPrograms, SampledImage::test,
                              pipelineConstructionType);
}

void create3dImageTestsInGroup(tcu::TestCaseGroup *group, PipelineConstructionType pipelineConstructionType)
{
    addTestCasesWithFunctions3d(group, Image3d::checkSupport, Image3d::initPrograms, Image3d::test,
                                pipelineConstructionType);
}

void createStorageImageTestsInGroup(tcu::TestCaseGroup *group, PipelineConstructionType pipelineConstructionType)
{
    addTestCasesWithFunctions(group, StorageImage::checkSupport, StorageImage::initPrograms, StorageImage::test,
                              pipelineConstructionType);
}

void createStandardSamplePositionTestsInGroup(tcu::TestCaseGroup *group,
                                              PipelineConstructionType pipelineConstructionType)
{
    addStandardSamplePositionTestCasesWithFunctions(group, StandardSamplePosition::checkSupport,
                                                    StandardSamplePosition::initPrograms, StandardSamplePosition::test,
                                                    pipelineConstructionType);
}

void createSamplesMappingOrderTestsInGroup(tcu::TestCaseGroup *group, PipelineConstructionType pipelineConstructionType)
{
    addSamplesMappingOrderTestCasesWithFunctions(group, SamplesMappingOrder::checkSupport,
                                                 SamplesMappingOrder::initPrograms, SamplesMappingOrder::test,
                                                 pipelineConstructionType);
}

} // namespace

//! Render to a multisampled image and sample from it in a fragment shader.
tcu::TestCaseGroup *createMultisampleSampledImageTests(tcu::TestContext &testCtx,
                                                       PipelineConstructionType pipelineConstructionType)
{
    return createTestGroup(testCtx, "sampled_image", createSampledImageTestsInGroup, pipelineConstructionType);
}

//! Render to a multisampled image and access it with load/stores in a compute shader.
tcu::TestCaseGroup *createMultisampleStorageImageTests(tcu::TestContext &testCtx,
                                                       PipelineConstructionType pipelineConstructionType)
{
    return createTestGroup(testCtx, "storage_image", createStorageImageTestsInGroup, pipelineConstructionType);
}

//! Render to a multisampled image and verify standard multisample positions.
tcu::TestCaseGroup *createMultisampleStandardSamplePositionTests(tcu::TestContext &testCtx,
                                                                 PipelineConstructionType pipelineConstructionType)
{
    return createTestGroup(testCtx, "standardsampleposition", createStandardSamplePositionTestsInGroup,
                           pipelineConstructionType);
}

//! Render to a multisampled image and verify if all samples are mapped in order
tcu::TestCaseGroup *createMultisampleSamplesMappingOrderTests(tcu::TestContext &testCtx,
                                                              PipelineConstructionType pipelineConstructionType)
{
    return createTestGroup(testCtx, "samples_mapping_order", createSamplesMappingOrderTestsInGroup,
                           pipelineConstructionType);
}

//! Render to a multisampled image and resolve it to a 3D image
tcu::TestCaseGroup *createMultisample3dImageTests(tcu::TestContext &testCtx,
                                                  PipelineConstructionType pipelineConstructionType)
{
    return createTestGroup(testCtx, "3d", create3dImageTestsInGroup, pipelineConstructionType);
}

} // namespace pipeline
} // namespace vkt
