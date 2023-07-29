/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
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
    checkPipelineLibraryRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
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
    const DeviceInterface &vk = context.getDeviceInterface();
    const VkDevice vkDevice   = context.getDevice();
    const Unique<VkShaderModule> vertexShaderModule(
        createShaderModule(vk, vkDevice, context.getBinaryCollection().get("color_vert"), 0));
    const Unique<VkShaderModule> fragmentShaderModule(
        createShaderModule(vk, vkDevice, context.getBinaryCollection().get("color_frag"), 0));

    const VkDescriptorSetLayoutBinding descriptorSetLayoutBinding = {
        0u,                                  // uint32_t              binding;
        VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, // VkDescriptorType      descriptorType;
        1u,                                  // uint32_t              descriptorCount;
        VK_SHADER_STAGE_FRAGMENT_BIT,        // VkShaderStageFlags    stageFlags;
        DE_NULL                              // const VkSampler*      pImmutableSamplers;
    };

    const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, // VkStructureType                        sType;
        DE_NULL,                                             // const void*                            pNext;
        0u,                                                  // VkDescriptorSetLayoutCreateFlags       flags;
        1u,                                                  // uint32_t                               bindingCount;
        &descriptorSetLayoutBinding                          // const VkDescriptorSetLayoutBinding*    pBindings;
    };

    const Unique<VkDescriptorSetLayout> descriptorSetLayout(
        createDescriptorSetLayout(vk, vkDevice, &descriptorSetLayoutCreateInfo, DE_NULL));

    const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType                 sType;
        DE_NULL,                                       // const void*                     pNext;
        0u,                                            // VkPipelineLayoutCreateFlags     flags;
        1u,                                            // uint32_t                        setLayoutCount;
        &(*descriptorSetLayout),                       // const VkDescriptorSetLayout*    pSetLayouts;
        0u,                                            // uint32_t                        pushConstantRangeCount;
        DE_NULL                                        // const VkPushConstantRange*      pPushConstantRanges;
    };

    const Unique<VkPipelineLayout> pipelineLayout(
        createPipelineLayout(vk, vkDevice, &pipelineLayoutCreateInfo, DE_NULL));

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
        DE_NULL,                         // const VkAttachmentReference*    pResolveAttachments;
        DE_NULL,                         // const VkAttachmentReference*    pDepthStencilAttachment;
        0u,                              // uint32_t                        preserveAttachmentCount;
        DE_NULL                          // const uint32_t*                 pPreserveAttachments;
    };

    const VkRenderPassCreateInfo renderPassCreateInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // VkStructureType                   sType;
        DE_NULL,                                   // const void*                       pNext;
        0u,                                        // VkRenderPassCreateFlags           flags;
        2u,                                        // uint32_t                          attachmentCount;
        descs,                                     // const VkAttachmentDescription*    pAttachments;
        1u,                                        // uint32_t                          subpassCount;
        &subpassDescription,                       // const VkSubpassDescription*       pSubpasses;
        0u,                                        // uint32_t                          dependencyCount;
        DE_NULL                                    // const VkSubpassDependency*        pDependencies;
    };

    const Unique<VkRenderPass> renderPass(createRenderPass(vk, vkDevice, &renderPassCreateInfo, DE_NULL));

    const VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, // VkStructureType               sType;
        DE_NULL,                                      // const void*                   pNext;
#ifndef CTS_USES_VULKANSC
        (VkPipelineCacheCreateFlags)0u, // VkPipelineCacheCreateFlags    flags;
        0u,                             // size_t                        initialDataSize;
        DE_NULL                         // const void*                   pInitialData;
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
        DE_NULL,                                                   // const void*                                 pNext;
        0u,                                                        // VkPipelineVertexInputStateCreateFlags       flags;
        0u,      // uint32_t                                    vertexBindingDescriptionCount;
        DE_NULL, // const VkVertexInputBindingDescription*      pVertexBindingDescriptions;
        0u,      // uint32_t                                    vertexAttributeDescriptionCount;
        DE_NULL  // const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions;
    };

    const VkDynamicState dynamicState[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    const VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, // VkStructureType                      sType;
        DE_NULL,                                              // const void*                          pNext;
        0u,                                                   // VkPipelineDynamicStateCreateFlags    flags;
        2u,                                                   // uint32_t                             dynamicStateCount;
        dynamicState                                          // const VkDynamicState*                pDynamicStates;
    };

    const std::vector<VkViewport> viewport{};
    const std::vector<VkRect2D> scissor{};
    GraphicsPipelineWrapper graphicsPipeline(vk, vkDevice, params.pipelineConstructionType);
    graphicsPipeline.setDynamicState(&dynamicStateCreateInfo)
        .setDefaultRasterizationState()
        .setDefaultMultisampleState()
        .setDefaultColorBlendState()
        .setupVertexInputState(&vertexInputStateCreateInfo)
        .setupPreRasterizationShaderState(viewport, scissor, *pipelineLayout, *renderPass, 0u, *vertexShaderModule)
        .setupFragmentShaderState(*pipelineLayout, *renderPass, 0u, *fragmentShaderModule)
        .setupFragmentOutputState(*renderPass, 0u)
        .setMonolithicPipelineLayout(*pipelineLayout)
        .buildPipeline(params.usePipelineCache ? *pipelineCache : DE_NULL);

    // Passes as long as createGraphicsPipeline didn't crash.
    return tcu::TestStatus::pass("Pass");
}

void addMatchedAttachmentsTestCasesWithFunctions(tcu::TestCaseGroup *group,
                                                 PipelineConstructionType pipelineConstructionType)
{
    const MatchedAttachmentsTestParams useCache = {pipelineConstructionType, true};
    addFunctionCaseWithPrograms(group, "cache", "", checkSupport, initPrograms, testMatchedAttachments, useCache);

    const MatchedAttachmentsTestParams noCache = {pipelineConstructionType, false};
    addFunctionCaseWithPrograms(group, "no_cache", "", checkSupport, initPrograms, testMatchedAttachments, noCache);
}

} // namespace

tcu::TestCaseGroup *createMatchedAttachmentsTests(tcu::TestContext &testCtx,
                                                  PipelineConstructionType pipelineConstructionType)
{
    return createTestGroup(testCtx, "matched_attachments", "Matched attachments tests",
                           addMatchedAttachmentsTestCasesWithFunctions, pipelineConstructionType);
}

} // namespace pipeline
} // namespace vkt
