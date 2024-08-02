/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
 * Copyright (c) 2018 Google Inc.
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
 * \brief Matched attachments tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineMatchedAttachmentsTests.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkRefUtil.hpp"
#include "deUniquePtr.hpp"

namespace vkt
{
namespace pipeline
{

using namespace vk;

namespace
{

struct MatchedAttachmentsTestParams
{
    PipelineConstructionType pipelineConstructionType;
    bool usePipelineCache;
};

void checkSupport(Context &context, const MatchedAttachmentsTestParams params)
{
    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          params.pipelineConstructionType);
}

void initPrograms(SourceCollections &programCollection, const MatchedAttachmentsTestParams params)
{
    DE_UNREF(params);

    programCollection.glslSources.add("color_vert") << glu::VertexSource("#version 450\n"
                                                                         "\n"
                                                                         "void main(){\n"
                                                                         "    gl_Position = vec4(1);\n"
                                                                         "}\n");

    programCollection.glslSources.add("color_frag")
        << glu::FragmentSource("#version 450\n"
                               "\n"
                               "layout(input_attachment_index=0, set=0, binding=0) uniform subpassInput x;\n"
                               "layout(location=0) out vec4 color;\n"
                               "void main() {\n"
                               "   color = subpassLoad(x);\n"
                               "}\n");
}

tcu::TestStatus testMatchedAttachments(Context &context, const MatchedAttachmentsTestParams params)
{
    const InstanceInterface &vki          = context.getInstanceInterface();
    const DeviceInterface &vk             = context.getDeviceInterface();
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
    const VkDevice vkDevice               = context.getDevice();
    const ShaderWrapper vertexShaderModule(
        ShaderWrapper(vk, vkDevice, context.getBinaryCollection().get("color_vert"), 0));
    const ShaderWrapper fragmentShaderModule(
        ShaderWrapper(vk, vkDevice, context.getBinaryCollection().get("color_frag"), 0));

    const VkDescriptorSetLayoutBinding descriptorSetLayoutBinding = {
        0u,                                  // uint32_t              binding;
        VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, // VkDescriptorType      descriptorType;
        1u,                                  // uint32_t              descriptorCount;
        VK_SHADER_STAGE_FRAGMENT_BIT,        // VkShaderStageFlags    stageFlags;
        nullptr                              // const VkSampler*      pImmutableSamplers;
    };

    const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, // VkStructureType                        sType;
        nullptr,                                             // const void*                            pNext;
        0u,                                                  // VkDescriptorSetLayoutCreateFlags       flags;
        1u,                                                  // uint32_t                               bindingCount;
        &descriptorSetLayoutBinding                          // const VkDescriptorSetLayoutBinding*    pBindings;
    };

    const Unique<VkDescriptorSetLayout> descriptorSetLayout(
        createDescriptorSetLayout(vk, vkDevice, &descriptorSetLayoutCreateInfo, nullptr));

    const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType                 sType;
        nullptr,                                       // const void*                     pNext;
        0u,                                            // VkPipelineLayoutCreateFlags     flags;
        1u,                                            // uint32_t                        setLayoutCount;
        &(*descriptorSetLayout),                       // const VkDescriptorSetLayout*    pSetLayouts;
        0u,                                            // uint32_t                        pushConstantRangeCount;
        nullptr                                        // const VkPushConstantRange*      pPushConstantRanges;
    };

    const PipelineLayoutWrapper pipelineLayout(params.pipelineConstructionType, vk, vkDevice, &pipelineLayoutCreateInfo,
                                               nullptr);

    const VkAttachmentDescription descs[2] = {
        {
            0u,                                       // VkAttachmentDescriptionFlags    flags;
            VK_FORMAT_R8G8B8A8_UNORM,                 // VkFormat                        format;
            VK_SAMPLE_COUNT_1_BIT,                    // VkSampleCountFlagBits           samples;
            VK_ATTACHMENT_LOAD_OP_LOAD,               // VkAttachmentLoadOp              loadOp;
            VK_ATTACHMENT_STORE_OP_STORE,             // VkAttachmentStoreOp             storeOp;
            VK_ATTACHMENT_LOAD_OP_LOAD,               // VkAttachmentLoadOp              stencilLoadOp;
            VK_ATTACHMENT_STORE_OP_STORE,             // VkAttachmentStoreOp             stencilStoreOp;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout                   initialLayout;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL  // VkImageLayout                   finalLayout;
        },
        {
            0u,                           // VkAttachmentDescriptionFlags    flags;
            VK_FORMAT_R8G8B8A8_UNORM,     // VkFormat                        format;
            VK_SAMPLE_COUNT_1_BIT,        // VkSampleCountFlagBits           samples;
            VK_ATTACHMENT_LOAD_OP_LOAD,   // VkAttachmentLoadOp              loadOp;
            VK_ATTACHMENT_STORE_OP_STORE, // VkAttachmentStoreOp             storeOp;
            VK_ATTACHMENT_LOAD_OP_LOAD,   // VkAttachmentLoadOp              stencilLoadOp;
            VK_ATTACHMENT_STORE_OP_STORE, // VkAttachmentStoreOp             stencilStoreOp;
            VK_IMAGE_LAYOUT_GENERAL,      // VkImageLayout                   initialLayout;
            VK_IMAGE_LAYOUT_GENERAL       // VkImageLayout                   finalLayout;
        }};

    const VkAttachmentReference color = {
        0u,                                      // uint32_t         attachment;
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // VkImageLayout    layout;
    };

    const VkAttachmentReference input = {
        1u,                     // uint32_t         attachment;
        VK_IMAGE_LAYOUT_GENERAL // VkImageLayout    layout;
    };

    const VkSubpassDescription subpassDescription = {
        0u,                              // VkSubpassDescriptionFlags       flags;
        VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint             pipelineBindPoint;
        1u,                              // uint32_t                        inputAttachmentCount;
        &input,                          // const VkAttachmentReference*    pInputAttachments;
        1u,                              // uint32_t                        colorAttachmentCount;
        &color,                          // const VkAttachmentReference*    pColorAttachments;
        nullptr,                         // const VkAttachmentReference*    pResolveAttachments;
        nullptr,                         // const VkAttachmentReference*    pDepthStencilAttachment;
        0u,                              // uint32_t                        preserveAttachmentCount;
        nullptr                          // const uint32_t*                 pPreserveAttachments;
    };

    const VkRenderPassCreateInfo renderPassCreateInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // VkStructureType                   sType;
        nullptr,                                   // const void*                       pNext;
        0u,                                        // VkRenderPassCreateFlags           flags;
        2u,                                        // uint32_t                          attachmentCount;
        descs,                                     // const VkAttachmentDescription*    pAttachments;
        1u,                                        // uint32_t                          subpassCount;
        &subpassDescription,                       // const VkSubpassDescription*       pSubpasses;
        0u,                                        // uint32_t                          dependencyCount;
        nullptr                                    // const VkSubpassDependency*        pDependencies;
    };

    RenderPassWrapper renderPass(params.pipelineConstructionType, vk, vkDevice, &renderPassCreateInfo);

    const VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, // VkStructureType               sType;
        nullptr,                                      // const void*                   pNext;
#ifndef CTS_USES_VULKANSC
        (VkPipelineCacheCreateFlags)0u, // VkPipelineCacheCreateFlags    flags;
        0u,                             // size_t                        initialDataSize;
        nullptr                         // const void*                   pInitialData;
#else
        VK_PIPELINE_CACHE_CREATE_READ_ONLY_BIT |
            VK_PIPELINE_CACHE_CREATE_USE_APPLICATION_STORAGE_BIT, // VkPipelineCacheCreateFlags        flags;
        context.getResourceInterface()->getCacheDataSize(),       // uintptr_t                         initialDataSize;
        context.getResourceInterface()->getCacheData()            // const void*                       pInitialData;
#endif // CTS_USES_VULKANSC
    };

    const Unique<VkPipelineCache> pipelineCache(createPipelineCache(vk, vkDevice, &pipelineCacheCreateInfo));

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType                             sType;
        nullptr,                                                   // const void*                                 pNext;
        0u,                                                        // VkPipelineVertexInputStateCreateFlags       flags;
        0u,      // uint32_t                                    vertexBindingDescriptionCount;
        nullptr, // const VkVertexInputBindingDescription*      pVertexBindingDescriptions;
        0u,      // uint32_t                                    vertexAttributeDescriptionCount;
        nullptr  // const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions;
    };

    const VkDynamicState dynamicState[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    const VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, // VkStructureType                      sType;
        nullptr,                                              // const void*                          pNext;
        0u,                                                   // VkPipelineDynamicStateCreateFlags    flags;
        2u,                                                   // uint32_t                             dynamicStateCount;
        dynamicState                                          // const VkDynamicState*                pDynamicStates;
    };

    const std::vector<VkViewport> viewport{};
    const std::vector<VkRect2D> scissor{};
    GraphicsPipelineWrapper graphicsPipeline(vki, vk, physicalDevice, vkDevice, context.getDeviceExtensions(),
                                             params.pipelineConstructionType);
    graphicsPipeline.setDynamicState(&dynamicStateCreateInfo)
        .setDefaultRasterizationState()
        .setDefaultMultisampleState()
        .setDefaultColorBlendState()
        .setupVertexInputState(&vertexInputStateCreateInfo)
        .setupPreRasterizationShaderState(viewport, scissor, pipelineLayout, *renderPass, 0u, vertexShaderModule)
        .setupFragmentShaderState(pipelineLayout, *renderPass, 0u, fragmentShaderModule)
        .setupFragmentOutputState(*renderPass, 0u)
        .setMonolithicPipelineLayout(pipelineLayout)
        .buildPipeline(params.usePipelineCache ? *pipelineCache : VK_NULL_HANDLE);

    // Passes as long as createGraphicsPipeline didn't crash.
    return tcu::TestStatus::pass("Pass");
}

void addMatchedAttachmentsTestCasesWithFunctions(tcu::TestCaseGroup *group,
                                                 PipelineConstructionType pipelineConstructionType)
{
    // Input attachments are not supported with dynamic rendering
    if (!isConstructionTypeShaderObject(pipelineConstructionType))
    {
        const MatchedAttachmentsTestParams useCache = {pipelineConstructionType, true};
        addFunctionCaseWithPrograms(group, "cache", checkSupport, initPrograms, testMatchedAttachments, useCache);

        const MatchedAttachmentsTestParams noCache = {pipelineConstructionType, false};
        addFunctionCaseWithPrograms(group, "no_cache", checkSupport, initPrograms, testMatchedAttachments, noCache);
    }
}

} // namespace

tcu::TestCaseGroup *createMatchedAttachmentsTests(tcu::TestContext &testCtx,
                                                  PipelineConstructionType pipelineConstructionType)
{
    return createTestGroup(testCtx, "matched_attachments", addMatchedAttachmentsTestCasesWithFunctions,
                           pipelineConstructionType);
}

} // namespace pipeline
} // namespace vkt
