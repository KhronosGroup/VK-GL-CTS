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
 * \brief Early pipeline destroying tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineEarlyDestroyTests.hpp"
#include "vkComputePipelineConstructionUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkRefUtil.hpp"
#include "vkObjUtil.hpp"
#include "deUniquePtr.hpp"
#include "tcuTexture.hpp"
#include "vkImageUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkCmdUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkBarrierUtil.hpp"

namespace vkt
{
namespace pipeline
{

using namespace vk;

namespace
{

struct TestParams
{
    PipelineConstructionType pipelineConstructionType;
    bool usePipelineCache;
    bool useMaintenance5;
    bool destroyLayout;
    bool useCompute;
};

inline vk::VkQueue getQueue(Context &ctx, bool useCompute)
{
    return useCompute ? ctx.getComputeQueue() : ctx.getUniversalQueue();
}

inline uint32_t getQueueNdx(Context &ctx, bool useCompute)
{
    return useCompute ? ctx.getComputeQueueFamilyIndex() : ctx.getUniversalQueueFamilyIndex();
}

class EarlyDestroyTestInstance : public TestInstance
{
public:
    EarlyDestroyTestInstance(Context &context, const TestParams &params);

    tcu::TestStatus iterate(void) override;

private:
    TestParams m_params;
};

EarlyDestroyTestInstance::EarlyDestroyTestInstance(Context &context, const TestParams &params)
    : TestInstance(context)
    , m_params(params)
{
}

tcu::TestStatus EarlyDestroyTestInstance::iterate(void)
{
    const InstanceInterface &vki          = m_context.getInstanceInterface();
    const DeviceInterface &vk             = m_context.getDeviceInterface();
    const VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();
    const VkDevice vkDevice               = m_context.getDevice();
    const VkQueue queue                   = getQueue(m_context, m_params.useCompute);
    const uint32_t qfNdx                  = getQueueNdx(m_context, m_params.useCompute);

    // Cmd buffer and pool
    const Unique<VkCommandPool> cmdPool(createCommandPool(
        vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, qfNdx));
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, vkDevice, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    // Render target setup
    const uint32_t width                   = 32;
    const uint32_t height                  = 32;
    const VkFormat format                  = VK_FORMAT_R8G8B8A8_UNORM;
    const tcu::TextureFormat textureFormat = mapVkFormat(format);
    const VkDeviceSize imageSize           = width * height * textureFormat.getPixelSize();
    VkImageUsageFlags imageUsage           = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (m_params.useCompute)
        imageUsage |= VK_IMAGE_USAGE_STORAGE_BIT;
    else
        imageUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    const VkImageCreateInfo imageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        (VkImageCreateFlags)0u,              // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        format,                              // VkFormat format;
        {width, height, 1u},                 // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        1u,                                  // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        imageUsage,                          // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        nullptr,                             // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED            // VkImageLayout initialLayout;
    };

    // Render target image and view
    ImageWithMemory image(vk, vkDevice, m_context.getDefaultAllocator(), imageCreateInfo, MemoryRequirement::Any);
    const Unique<VkImageView> imageView(
        makeImageView(vk, vkDevice, *image, VK_IMAGE_VIEW_TYPE_2D, format,
                      makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u)));

    // Readback buffer
    BufferWithMemory buffer(vk, vkDevice, m_context.getDefaultAllocator(),
                            makeBufferCreateInfo(imageSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                            MemoryRequirement::HostVisible);

    // Descriptors
    Move<VkDescriptorSetLayout> descriptorSetLayout;
    Move<VkDescriptorPool> descriptorPool;
    Move<VkDescriptorSet> descriptorSet;

    if (m_params.useCompute)
    {
        DescriptorSetLayoutBuilder layoutBuilder;
        layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
        descriptorSetLayout = layoutBuilder.build(vk, vkDevice);

        DescriptorPoolBuilder poolBuilder;
        poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        descriptorPool = poolBuilder.build(vk, vkDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

        descriptorSet = makeDescriptorSet(vk, vkDevice, *descriptorPool, *descriptorSetLayout);

        DescriptorSetUpdateBuilder updateBuilder;
        VkDescriptorImageInfo descImageInfo =
            makeDescriptorImageInfo(VK_NULL_HANDLE, *imageView, VK_IMAGE_LAYOUT_GENERAL);
        updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                                  VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descImageInfo);
        updateBuilder.update(vk, vkDevice);
    }

    // Pipeline cache
    const VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                      // const void* pNext;
#ifndef CTS_USES_VULKANSC
        (VkPipelineCacheCreateFlags)0u, // VkPipelineCacheCreateFlags flags;
        0u,                             // size_t initialDataSize;
        nullptr                         // const void* pInitialData;
#else
        VK_PIPELINE_CACHE_CREATE_READ_ONLY_BIT |
            VK_PIPELINE_CACHE_CREATE_USE_APPLICATION_STORAGE_BIT, // VkPipelineCacheCreateFlags flags;
        m_context.getResourceInterface()->getCacheDataSize(),     // uintptr_t initialDataSize;
        m_context.getResourceInterface()->getCacheData()          // const void* pInitialData;
#endif // CTS_USES_VULKANSC
    };
    const Unique<VkPipelineCache> pipelineCache(createPipelineCache(vk, vkDevice, &pipelineCacheCreateInfo));
    const VkPipelineCache validCache = m_params.usePipelineCache ? *pipelineCache : VK_NULL_HANDLE;

    int numTests = m_params.destroyLayout ? 3 : 1;
    for (int i = 0; i < numTests; ++i)
    {
        // Pipeline layout
        const uint32_t setLayoutCount            = m_params.useCompute ? 1u : 0u;
        const VkDescriptorSetLayout *pSetLayouts = m_params.useCompute ? &descriptorSetLayout.get() : nullptr;

        const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
            nullptr,                                       // const void* pNext;
            0u,                                            // VkPipelineLayoutCreateFlags flags;
            setLayoutCount,                                // uint32_t setLayoutCount;
            pSetLayouts,                                   // const VkDescriptorSetLayout* pSetLayouts;
            0u,                                            // uint32_t pushConstantRangeCount;
            nullptr                                        // const VkPushConstantRange* pPushConstantRanges;
        };

        PipelineLayoutWrapper pipelineLayout(m_params.pipelineConstructionType, vk, vkDevice, &pipelineLayoutCreateInfo,
                                             nullptr);

        // Scope for pipeline wrappers
        {
            // Objects needed for Graphics construction
            de::MovePtr<RenderPassWrapper> renderPass;
            de::MovePtr<GraphicsPipelineWrapper> graphicsPipeline;

            // Objects needed for Compute construction
            de::MovePtr<ComputePipelineWrapper> computePipeline;

            if (m_params.useCompute)
            {
                const ComputePipelineConstructionType computeConstructionType =
                    graphicsToComputeConstructionType(m_params.pipelineConstructionType);
                computePipeline = de::MovePtr<ComputePipelineWrapper>(new ComputePipelineWrapper(
                    vk, vkDevice, computeConstructionType, m_context.getBinaryCollection().get("comp")));

                computePipeline->setDescriptorSetLayout(*descriptorSetLayout);
                computePipeline->buildPipeline(validCache);
            }
            else
            {
                renderPass = de::MovePtr<RenderPassWrapper>(
                    new RenderPassWrapper(m_params.pipelineConstructionType, vk, vkDevice, format));
                renderPass->createFramebuffer(vk, vkDevice, *image, *imageView, width, height, 1u);

                const ShaderWrapper vertexShaderModule(
                    ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("color_vert"), 0));

                const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
                    VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
                    nullptr,                                                   // const void* pNext;
                    0u,      // VkPipelineVertexInputStateCreateFlags flags;
                    0u,      // uint32_t vertexBindingDescriptionCount;
                    nullptr, // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
                    0u,      // uint32_t vertexAttributeDescriptionCount;
                    nullptr  // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
                };
                const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {
                    VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, // VkStructureType sType;
                    nullptr,                                                     // const void* pNext;
                    0u,                                   // VkPipelineInputAssemblyStateCreateFlags flags;
                    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // VkPrimitiveTopology topology;
                    VK_FALSE                              // VkBool32 primitiveRestartEnable;
                };
                const VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {
                    VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, // VkStructureType sType;
                    nullptr,                                                    // const void* pNext;
                    0u,                      // VkPipelineRasterizationStateCreateFlags flags;
                    VK_FALSE,                // VkBool32 depthClampEnable;
                    VK_TRUE,                 // VkBool32 rasterizerDiscardEnable;
                    VK_POLYGON_MODE_FILL,    // VkPolygonMode polygonMode;
                    VK_CULL_MODE_BACK_BIT,   // VkCullModeFlags cullMode;
                    VK_FRONT_FACE_CLOCKWISE, // VkFrontFace frontFace;
                    VK_FALSE,                // VkBool32 depthBiasEnable;
                    0.0f,                    // float depthBiasConstantFactor;
                    0.0f,                    // float depthBiasClamp;
                    0.0f,                    // float depthBiasSlopeFactor;
                    1.0f                     // float lineWidth;
                };
                const VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {
                    VK_FALSE,             // VkBool32 blendEnable;
                    VK_BLEND_FACTOR_ZERO, // VkBlendFactor srcColorBlendFactor;
                    VK_BLEND_FACTOR_ZERO, // VkBlendFactor dstColorBlendFactor;
                    VK_BLEND_OP_ADD,      // VkBlendOp colorBlendOp;
                    VK_BLEND_FACTOR_ZERO, // VkBlendFactor srcAlphaBlendFactor;
                    VK_BLEND_FACTOR_ZERO, // VkBlendFactor dstAlphaBlendFactor;
                    VK_BLEND_OP_ADD,      // VkBlendOp alphaBlendOp;
                    0xf                   // VkColorComponentFlags colorWriteMask;
                };
                const VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {
                    VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType sType;
                    nullptr,                                                  // const void* pNext;
                    0u,                         // VkPipelineColorBlendStateCreateFlags flags;
                    VK_FALSE,                   // VkBool32 logicOpEnable;
                    VK_LOGIC_OP_CLEAR,          // VkLogicOp logicOp;
                    1u,                         // uint32_t attachmentCount;
                    &colorBlendAttachmentState, // const VkPipelineColorBlendAttachmentState* pAttachments;
                    {0.0f, 0.0f, 0.0f, 0.0f}    // float blendConstants[4];
                };

                const std::vector<VkViewport> viewports{};
                const std::vector<VkRect2D> scissors{};

                graphicsPipeline = de::MovePtr<GraphicsPipelineWrapper>(new GraphicsPipelineWrapper(
                    vki, vk, physicalDevice, vkDevice, m_context.getDeviceExtensions(),
                    m_params.pipelineConstructionType, VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT));

#ifndef CTS_USES_VULKANSC
                if (m_params.useMaintenance5)
                    graphicsPipeline->setPipelineCreateFlags2(VK_PIPELINE_CREATE_2_DISABLE_OPTIMIZATION_BIT_KHR);
#endif

                graphicsPipeline->disableViewportState()
                    .setDefaultMultisampleState()
                    .setDefaultDepthStencilState()
                    .setupVertexInputState(&vertexInputStateCreateInfo, &inputAssemblyStateCreateInfo)
                    .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, renderPass->get(), 0u,
                                                      vertexShaderModule, &rasterizationStateCreateInfo)
                    // Uninitialized so the pipeline wrapper does not add fragment stage.
                    // This avoids running into VUID-VkGraphicsPipelineCreateInfo-pStages-06894 due to enabled rasterizerDiscard
                    .setupFragmentShaderState(pipelineLayout, renderPass->get(), 0u, vk::ShaderWrapper())
                    .setupFragmentOutputState(renderPass->get(), 0u, &colorBlendStateCreateInfo)
                    .setMonolithicPipelineLayout(pipelineLayout)
                    .buildPipeline(validCache);
            }

            if (m_params.destroyLayout)
            {
                // This will destroy the pipelineLayout when going out of enclosing scope
                pipelineLayout.destroy();
            }
            const VkCommandBufferBeginInfo cmdBufferBeginInfo = {
                VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // VkStructureType sType;
                nullptr,                                     // const void* pNext;
                0u,                                          // VkCommandBufferUsageFlags flags;
                nullptr                                      // const VkCommandBufferInheritanceInfo* pInheritanceInfo;
            };
            if (!m_params.destroyLayout)
            {
                VK_CHECK(vk.beginCommandBuffer(*cmdBuffer, &cmdBufferBeginInfo));
                VK_CHECK(vk.endCommandBuffer(*cmdBuffer));
            }
            else
            {
                VK_CHECK(vk.beginCommandBuffer(*cmdBuffer, &cmdBufferBeginInfo));
                const tcu::Vec4 clearColor = {0.2f, 0.6f, 0.8f, 1.0f};
                VkClearValue clearValue    = {
                    {{clearColor.x(), clearColor.y(), clearColor.z(), clearColor.w()}} // float float32[4];
                };

                if (!m_params.useCompute)
                {

                    VkClearAttachment attachment = {
                        VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                        0u,                        // uint32_t colorAttachment;
                        clearValue                 // VkClearValue clearValue;
                    };
                    const VkRect2D renderArea = {{0, 0}, {width, height}};
                    const VkClearRect rect    = {
                        renderArea, // VkRect2D                        rect
                        0u,         // uint32_t                        baseArrayLayer
                        1u          // uint32_t                        layerCount
                    };
                    renderPass->begin(vk, *cmdBuffer, renderArea, clearValue);
                    vk.cmdClearAttachments(*cmdBuffer, 1, &attachment, 1, &rect);
                    renderPass->end(vk, *cmdBuffer);
                }
                else // compute
                {
                    const VkImageMemoryBarrier preBarrier = makeImageMemoryBarrier(
                        0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, *image,
                        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u));

                    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                          0u, 0u, nullptr, 0u, nullptr, 1u, &preBarrier);

                    VkImageSubresourceRange range =
                        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
                    vk.cmdClearColorImage(*cmdBuffer, *image, VK_IMAGE_LAYOUT_GENERAL, &clearValue.color, 1u, &range);
                }

                vk::VkAccessFlags accessMask =
                    m_params.useCompute ? VK_ACCESS_TRANSFER_WRITE_BIT : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                vk::VkImageLayout layout =
                    m_params.useCompute ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                vk::copyImageToBuffer(vk, *cmdBuffer, *image, *buffer, tcu::IVec2(width, height), accessMask, layout);
                VK_CHECK(vk.endCommandBuffer(*cmdBuffer));

                vk::submitCommandsAndWait(vk, vkDevice, queue, *cmdBuffer);
                VK_CHECK(vk.resetCommandBuffer(*cmdBuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT));
                const auto &imageBufferAlloc = buffer.getAllocation();
                vk::invalidateAlloc(vk, vkDevice, imageBufferAlloc);

                const auto imageBufferPtr =
                    reinterpret_cast<const char *>(imageBufferAlloc.getHostPtr()) + imageBufferAlloc.getOffset();
                const tcu::ConstPixelBufferAccess imagePixels(textureFormat, width, height, 1u, imageBufferPtr);

#ifdef CTS_USES_VULKANSC
                if (m_context.getTestContext().getCommandLine().isSubProcess())
#endif // CTS_USES_VULKANSC
                {
                    for (int z = 0; z < imagePixels.getDepth(); ++z)
                        for (int y = 0; y < imagePixels.getHeight(); ++y)
                            for (int x = 0; x < imagePixels.getWidth(); ++x)
                            {
                                const auto pixel = imagePixels.getPixel(x, y, z);
                                if (pixel != clearColor)
                                {
                                    std::ostringstream msg;
                                    msg << "Pixel value mismatch after clear."
                                        << " diff: " << pixel << " vs " << clearColor;

                                    return tcu::TestStatus::fail(
                                        msg.str() /*"Pixel value mismatch after framebuffer clear."*/);
                                }
                            }
                }
            }

        } // End of pipeline wrapper scope
    }

    // Passes as long as no crash occurred.
    return tcu::TestStatus::pass("Pass");
}

class EarlyDestroyTestCase : public TestCase
{
public:
    EarlyDestroyTestCase(tcu::TestContext &testCtx, const std::string &name, const TestParams &params)
        : TestCase(testCtx, name)
        , m_params(params)
    {
    }

    void initPrograms(SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;
    void checkSupport(Context &context) const override;

private:
    TestParams m_params;
};

void EarlyDestroyTestCase::initPrograms(SourceCollections &programCollection) const
{
    if (m_params.useCompute)
    {
        programCollection.glslSources.add("comp")
            << glu::ComputeSource("#version 450\n"
                                  "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
                                  "layout(set = 0, binding = 0, rgba8) writeonly uniform image2D u_image;\n"
                                  "void main()\n"
                                  "{\n"
                                  "    imageStore(u_image, ivec2(gl_GlobalInvocationID.xy), vec4(0, 1, 0, 1));\n"
                                  "}\n");
    }
    else // graphics
    {
        programCollection.glslSources.add("color_vert")
            << glu::VertexSource("#version 450\n"
                                 "vec2 vertices[3];\n"
                                 "\n"
                                 "void main()\n"
                                 "{\n"
                                 "   vertices[0] = vec2(-1.0, -1.0);\n"
                                 "   vertices[1] = vec2( 1.0, -1.0);\n"
                                 "   vertices[2] = vec2( 0.0,  1.0);\n"
                                 "   gl_Position = vec4(vertices[gl_VertexIndex % 3], 0.0, 1.0);\n"
                                 "}\n");

        programCollection.glslSources.add("color_frag")
            << glu::FragmentSource("#version 450\n"
                                   "\n"
                                   "layout(location = 0) out vec4 uFragColor;\n"
                                   "\n"
                                   "void main()\n"
                                   "{\n"
                                   "   uFragColor = vec4(0,1,0,1);\n"
                                   "}\n");
    }
}

TestInstance *EarlyDestroyTestCase::createInstance(Context &context) const
{
    return new EarlyDestroyTestInstance(context, m_params);
}

void EarlyDestroyTestCase::checkSupport(Context &context) const
{
    if (m_params.useMaintenance5)
        context.requireDeviceFunctionality("VK_KHR_maintenance5");

    if (m_params.useCompute)
    {
        ComputePipelineConstructionType computeType =
            graphicsToComputeConstructionType(m_params.pipelineConstructionType);
        checkShaderObjectRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), computeType);
    }
    else
    {
        checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                              m_params.pipelineConstructionType);
    }
}

void addEarlyDestroyTests(tcu::TestCaseGroup *group, PipelineConstructionType pipelineConstructionType)
{
    const bool compCompatible =
        (pipelineConstructionType == vk::PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC ||
         pipelineConstructionType == vk::PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_UNLINKED_SPIRV);

    const bool usePipelineCacheOptions[] = {false, true};
    const bool destroyLayoutOptions[]    = {false, true};
    const bool useComputeOptions[]       = {false, true};

    for (bool useCompute : useComputeOptions)
    {
        if (useCompute && !compCompatible)
            continue;

        std::string computePostfix = useCompute ? "_compute" : "";
        for (bool destroyLayout : destroyLayoutOptions)
        {
            for (bool useCache : usePipelineCacheOptions)
            {
                std::string name = (useCache ? "cache" : "no_cache");

                if (destroyLayout)
                    name += "_destroy_layout";
                name += computePostfix;

                TestParams params{pipelineConstructionType, useCache,
                                  false, // useMaintenance5
                                  destroyLayout, useCompute};

                group->addChild(new EarlyDestroyTestCase(group->getTestContext(), name, params));
            }
        }
    }

    {
        TestParams params{
            pipelineConstructionType,
            false, // usePipelineCache
            true,  // useMaintenance5
            true,  // destroyLayout
            false  // useCompute
        };
        group->addChild(
            new EarlyDestroyTestCase(group->getTestContext(), "no_cache_destroy_layout_maintenance5", params));
    }
}

} // namespace

tcu::TestCaseGroup *createEarlyDestroyTests(tcu::TestContext &testCtx,
                                            PipelineConstructionType pipelineConstructionType)
{
    return createTestGroup(testCtx, "early_destroy", addEarlyDestroyTests, pipelineConstructionType);
}

} // namespace pipeline
} // namespace vkt
