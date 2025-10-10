/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024 LunarG, Inc.
 * Copyright (c) 2024 Google LLC
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
 * \brief Tests vkCmdClearAttachments with unused attachments.
 *//*--------------------------------------------------------------------*/

#include "vktRenderPassRemainingArrayLayersTests.hpp"

#include "vktRenderPassTestsUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "tcuTextureUtil.hpp"
#include <sstream>
#include <functional>
#include <vector>
#include <string>
#include <memory>

namespace vkt
{
namespace renderpass
{

namespace
{

struct TestParams
{
    TestParams(const uint32_t base_layer_, const uint32_t additional_layers_, const bool multi_layered_framebuffer_,
               const bool write_gl_layer_, const SharedGroupParams groupParams_)
        : groupParams(groupParams_)
        , baseLayer(base_layer_)
        , additionalLayers(additional_layers_)
        , multiLayeredFramebuffer(multi_layered_framebuffer_)
        , writeGlLayer(write_gl_layer_)
    {
    }
    const SharedGroupParams groupParams;
    const uint32_t baseLayer;
    const uint32_t additionalLayers;
    const bool multiLayeredFramebuffer;
    const bool writeGlLayer;
};

class RemainingArrayLayersTestInstance : public vkt::TestInstance
{
public:
    RemainingArrayLayersTestInstance(Context &context, const TestParams &testParams)
        : vkt::TestInstance(context)
        , m_testParams(testParams)
    {
    }
    virtual ~RemainingArrayLayersTestInstance(void)
    {
    }
    virtual tcu::TestStatus iterate(void);

    template <typename RenderpassSubpass>
    void beginRenderPass(const DeviceInterface &vk, const vk::Move<vk::VkCommandBuffer> &cmdBuffer);
    template <typename RenderpassSubpass>
    void endRenderPass(const DeviceInterface &vk, const vk::Move<vk::VkCommandBuffer> &cmdBuffer);

private:
    const TestParams m_testParams;
    Move<VkRenderPass> m_renderPass;
    Move<VkFramebuffer> m_framebuffer;
};

// Create a render pass for this use case.
template <typename AttachmentDesc, typename AttachmentRef, typename SubpassDesc, typename SubpassDep,
          typename RenderPassCreateInfo>
Move<VkRenderPass> createRenderPass(const DeviceInterface &vk, VkDevice vkDevice)
{
    const AttachmentDesc attachmentDescription(
        nullptr,                                 // const void*                      pNext
        (VkAttachmentDescriptionFlags)0,         // VkAttachmentDescriptionFlags     flags
        VK_FORMAT_R8G8B8A8_UNORM,                // VkFormat                         format
        VK_SAMPLE_COUNT_1_BIT,                   // VkSampleCountFlagBits            samples
        VK_ATTACHMENT_LOAD_OP_CLEAR,             // VkAttachmentLoadOp               loadOp
        VK_ATTACHMENT_STORE_OP_STORE,            // VkAttachmentStoreOp              storeOp
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,         // VkAttachmentLoadOp               stencilLoadOp
        VK_ATTACHMENT_STORE_OP_DONT_CARE,        // VkAttachmentStoreOp              stencilStoreOp
        VK_IMAGE_LAYOUT_UNDEFINED,               // VkImageLayout                    initialLayout
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // VkImageLayout                    finalLayout
    );

    // Mark attachments as used or not depending on the test parameters.
    AttachmentRef attachmentReference =
        AttachmentRef(nullptr,                                  // const void*            pNext
                      0u,                                       // uint32_t                attachment
                      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout        layout
                      VK_IMAGE_ASPECT_COLOR_BIT                 // VkImageAspectFlags    aspectMask
        );

    // Create subpass description with the previous color attachment references.
    const SubpassDesc subpassDescription(
        nullptr,
        (VkSubpassDescriptionFlags)0,    // VkSubpassDescriptionFlags        flags
        VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint                pipelineBindPoint
        0u,                              // uint32_t                            viewMask
        0u,                              // uint32_t                            inputAttachmentCount
        nullptr,                         // const VkAttachmentReference*        pInputAttachments
        1u,                              // uint32_t                            colorAttachmentCount
        &attachmentReference,            // const VkAttachmentReference*        pColorAttachments
        nullptr,                         // const VkAttachmentReference*        pResolveAttachments
        nullptr,                         // const VkAttachmentReference*        pDepthStencilAttachment
        0u,                              // uint32_t                            preserveAttachmentCount
        nullptr                          // const uint32_t*                    pPreserveAttachments
    );

    const RenderPassCreateInfo renderPassInfo(nullptr,                     // const void*                        pNext
                                              (VkRenderPassCreateFlags)0u, // VkRenderPassCreateFlags            flags
                                              1u, // uint32_t                            attachmentCount
                                              &attachmentDescription, // const VkAttachmentDescription*    pAttachments
                                              1u,                  // uint32_t                            subpassCount
                                              &subpassDescription, // const VkSubpassDescription*        pSubpasses
                                              0u,      // uint32_t                            dependencyCount
                                              nullptr, // const VkSubpassDependency*        pDependencies
                                              0u,      // uint32_t                            correlatedViewMaskCount
                                              nullptr  // const uint32_t*                    pCorrelatedViewMasks
    );

    return renderPassInfo.createRenderPass(vk, vkDevice);
}

template <typename RenderpassSubpass>
void RemainingArrayLayersTestInstance::beginRenderPass(const DeviceInterface &vk,
                                                       const vk::Move<vk::VkCommandBuffer> &cmdBuffer)
{
    const auto clear_value                = makeClearValueColor(tcu::Vec4(0.0f));
    const VkRenderPassBeginInfo beginInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                             nullptr,
                                             *m_renderPass,
                                             *m_framebuffer,
                                             {{0u, 0u}, {32u, 32u}},
                                             1u,
                                             &clear_value};
    const typename RenderpassSubpass::SubpassBeginInfo subpassBeginInfo(nullptr, VK_SUBPASS_CONTENTS_INLINE);
    RenderpassSubpass::cmdBeginRenderPass(vk, *cmdBuffer, &beginInfo, &subpassBeginInfo);
}

template <typename RenderpassSubpass>
void RemainingArrayLayersTestInstance::endRenderPass(const DeviceInterface &vk,
                                                     const vk::Move<vk::VkCommandBuffer> &cmdBuffer)
{
    const typename RenderpassSubpass::SubpassEndInfo subpassEndInfo(nullptr);
    RenderpassSubpass::cmdEndRenderPass(vk, *cmdBuffer, &subpassEndInfo);
}

tcu::TestStatus RemainingArrayLayersTestInstance::iterate(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    auto &alloc                     = m_context.getDefaultAllocator();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    tcu::TestLog &log               = m_context.getTestContext().getLog();

    const uint32_t render_size = 32u;

    const uint32_t depth = 1u + m_testParams.baseLayer + m_testParams.additionalLayers;

    const VkImageCreateInfo imageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                                   // VkStructureType sType;
        nullptr,                                                               // const void* pNext;
        VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT,                               // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_3D,                                                      // VkImageType imageType;
        VK_FORMAT_R8G8B8A8_UNORM,                                              // VkFormat format;
        {render_size, render_size, depth},                                     // VkExtent3D extent;
        1u,                                                                    // uint32_t mipLevels;
        1u,                                                                    // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,                                                 // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,                                               // VkImageTiling tiling;
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,                                             // VkSharingMode sharingMode;
        0u,                                                                    // uint32_t queueFamilyIndexCount;
        nullptr,                                                               // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED                                              // VkImageLayout initialLayout;
    };

    const auto image = de::MovePtr<ImageWithMemory>(
        new ImageWithMemory(vk, device, alloc, imageCreateInfo, vk::MemoryRequirement::Any));

    const VkImageViewCreateInfo imageViewCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
        nullptr,                                  // const void* pNext;
        0u,                                       // VkImageViewCreateFlags flags;
        **image,                                  // VkImage image;
        VK_IMAGE_VIEW_TYPE_2D_ARRAY,              // VkImageViewType viewType;
        VK_FORMAT_R8G8B8A8_UNORM,                 // VkFormat format;
        makeComponentMappingRGBA(),               // VkChannelMapping channels;
        {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, m_testParams.baseLayer,
         VK_REMAINING_ARRAY_LAYERS} // VkImageSubresourceRange subresourceRange;
    };

    const auto imageView = createImageView(vk, device, &imageViewCreateInfo, nullptr);

    if (m_testParams.groupParams->renderingType == RENDERING_TYPE_RENDERPASS_LEGACY)
        m_renderPass = createRenderPass<AttachmentDescription1, AttachmentReference1, SubpassDescription1,
                                        SubpassDependency1, RenderPassCreateInfo1>(vk, device);
    else if (m_testParams.groupParams->renderingType == RENDERING_TYPE_RENDERPASS2)
        m_renderPass = createRenderPass<AttachmentDescription2, AttachmentReference2, SubpassDescription2,
                                        SubpassDependency2, RenderPassCreateInfo2>(vk, device);

    uint32_t framebufferLayers = m_testParams.multiLayeredFramebuffer ? depth - m_testParams.baseLayer : 1u;

    const VkFramebufferCreateInfo framebufferParams = {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // VkStructureType sType;
        nullptr,                                   // const void* pNext;
        0u,                                        // VkFramebufferCreateFlags flags;
        *m_renderPass,                             // VkRenderPass renderPass;
        1u,                                        // uint32_t attachmentCount;
        &*imageView,                               // const VkImageView* pAttachments;
        render_size,                               // uint32_t width;
        render_size,                               // uint32_t height;
        framebufferLayers                          // uint32_t layers;
    };

    m_framebuffer = createFramebuffer(vk, device, &framebufferParams);

    const auto vertexShaderModule = createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0);
    vk::Move<vk::VkShaderModule> geometryShaderModule;
    if (m_testParams.writeGlLayer)
        geometryShaderModule = createShaderModule(vk, device, m_context.getBinaryCollection().get("geom"), 0);
    const auto fragmentShaderModule = createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0);

    // Create pipeline.
    const std::vector<VkViewport> viewports(1, makeViewport(render_size, render_size));
    const std::vector<VkRect2D> scissors(1, makeRect2D(render_size, render_size));

    const VkPipelineLayoutCreateInfo pipelineLayoutParams = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
        nullptr,                                       // const void* pNext;
        0u,                                            // VkPipelineLayoutCreateFlags flags;
        0u,                                            // uint32_t setLayoutCount;
        0u,                                            // const VkDescriptorSetLayout* pSetLayouts;
        0u,                                            // uint32_t pushConstantRangeCount;
        nullptr                                        // const VkPushConstantRange* pPushConstantRanges;
    };
    const auto pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutParams);

    const VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {
        VK_FALSE,                // VkBool32                    blendEnable
        VK_BLEND_FACTOR_ZERO,    // VkBlendFactor            srcColorBlendFactor
        VK_BLEND_FACTOR_ZERO,    // VkBlendFactor            dstColorBlendFactor
        VK_BLEND_OP_ADD,         // VkBlendOp                colorBlendOp
        VK_BLEND_FACTOR_ZERO,    // VkBlendFactor            srcAlphaBlendFactor
        VK_BLEND_FACTOR_ZERO,    // VkBlendFactor            dstAlphaBlendFactor
        VK_BLEND_OP_ADD,         // VkBlendOp                alphaBlendOp
        VK_COLOR_COMPONENT_R_BIT // VkColorComponentFlags    colorWriteMask
            | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};

    const VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType                              sType
        nullptr,                                                  // const void*                                  pNext
        0u,                                                       // VkPipelineColorBlendStateCreateFlags         flags
        VK_FALSE,                   // VkBool32                                     logicOpEnable
        VK_LOGIC_OP_CLEAR,          // VkLogicOp                                    logicOp
        1u,                         // uint32_t                                     attachmentCount
        &colorBlendAttachmentState, // const VkPipelineColorBlendAttachmentState*   pAttachments
        {0.0f, 0.0f, 0.0f, 0.0f}    // float                                        blendConstants[4]
    };

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                   // const void* pNext;
        (VkPipelineVertexInputStateCreateFlags)0u,                 // VkPipelineVertexInputStateCreateFlags     flags;
        0u,                                                        // uint32_t vertexBindingDescriptionCount;
        nullptr, // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
        0u,      // uint32_t vertexAttributeDescriptionCount;
        nullptr  // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
    };

    const auto graphicsPipeline = makeGraphicsPipeline(
        vk,                    // const DeviceInterface&                            vk
        device,                // const VkDevice                                    device
        *pipelineLayout,       // const VkPipelineLayout                            pipelineLayout
        *vertexShaderModule,   // const VkShaderModule                              vertexShaderModule
        VK_NULL_HANDLE,        // const VkShaderModule                              tessellationControlModule
        VK_NULL_HANDLE,        // const VkShaderModule                              tessellationEvalModule
        *geometryShaderModule, // const VkShaderModule                              geometryShaderModule
        *fragmentShaderModule, // const VkShaderModule                              fragmentShaderModule
        *m_renderPass,         // const VkRenderPass                                renderPass
        viewports,             // const std::vector<VkViewport>&                    viewports
        scissors,              // const std::vector<VkRect2D>&                      scissors
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // const VkPrimitiveTopology                        topology
        0u,                                  // const uint32_t                                    subpass
        0u,                                  // const uint32_t                                    patchControlPoints
        &vertexInputStateCreateInfo, // const VkPipelineVertexInputStateCreateInfo*      vertexInputStateCreateInfo
        nullptr,                     // const VkPipelineRasterizationStateCreateInfo*    rasterizationStateCreateInfo
        nullptr,                     // const VkPipelineMultisampleStateCreateInfo*      multisampleStateCreateInfo
        nullptr,                     // const VkPipelineDepthStencilStateCreateInfo*     depthStencilStateCreateInfo
        &colorBlendStateCreateInfo); // const void*                                      pNext

    const vk::VkCommandPoolCreateInfo cmdPoolInfo = {
        vk::VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,      // sType
        nullptr,                                             // pNext
        vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // flags
        queueFamilyIndex,                                    // queuefamilyindex
    };

    const vk::Move<vk::VkCommandPool> cmdPool(createCommandPool(vk, device, &cmdPoolInfo));
    const vk::Move<vk::VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    uint32_t instanceCount = m_testParams.writeGlLayer ? framebufferLayers : 1u;
    const vk::VkDeviceSize colorOutputBufferSize =
        render_size * render_size * tcu::getPixelSize(vk::mapVkFormat(VK_FORMAT_R8G8B8A8_UNORM)) * instanceCount;
    de::MovePtr<vk::BufferWithMemory> colorOutputBuffer = de::MovePtr<vk::BufferWithMemory>(new vk::BufferWithMemory(
        vk, device, alloc, makeBufferCreateInfo(colorOutputBufferSize, vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        vk::MemoryRequirement::HostVisible));

    // Draw
    vk::beginCommandBuffer(vk, *cmdBuffer);

    if (m_testParams.groupParams->renderingType == RENDERING_TYPE_RENDERPASS_LEGACY)
        beginRenderPass<RenderpassSubpass1>(vk, cmdBuffer);
    else
        beginRenderPass<RenderpassSubpass2>(vk, cmdBuffer);

    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);

    vk.cmdDraw(*cmdBuffer, 3u, instanceCount, 0u, 0u);

    if (m_testParams.groupParams->renderingType == RENDERING_TYPE_RENDERPASS_LEGACY)
        endRenderPass<RenderpassSubpass1>(vk, cmdBuffer);
    else
        endRenderPass<RenderpassSubpass2>(vk, cmdBuffer);

    const auto subresourceRange = makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    vk::VkImageMemoryBarrier postImageBarrier = vk::makeImageMemoryBarrier(
        vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, vk::VK_ACCESS_TRANSFER_READ_BIT,
        vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, vk::VK_IMAGE_LAYOUT_GENERAL, **image, subresourceRange);
    vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          vk::VK_PIPELINE_STAGE_TRANSFER_BIT, (vk::VkDependencyFlags)0u, 0u, nullptr, 0u, nullptr, 1u,
                          &postImageBarrier);

    const vk::VkBufferImageCopy copyRegion = {
        0u, // VkDeviceSize bufferOffset;
        0u, // uint32_t bufferRowLength;
        0u, // uint32_t bufferImageHeight;
        {
            vk::VK_IMAGE_ASPECT_COLOR_BIT,        // VkImageAspectFlags aspect;
            0u,                                   // uint32_t mipLevel;
            0u,                                   // uint32_t baseArrayLayer;
            1u,                                   // uint32_t layerCount;
        },                                        // VkImageSubresourceLayers imageSubresource;
        {0, 0, (int32_t)m_testParams.baseLayer},  // VkOffset3D imageOffset;
        {render_size, render_size, instanceCount} // VkExtent3D imageExtent;
    };
    vk.cmdCopyImageToBuffer(*cmdBuffer, **image, vk::VK_IMAGE_LAYOUT_GENERAL, **colorOutputBuffer, 1u, &copyRegion);

    vk::endCommandBuffer(vk, *cmdBuffer);
    submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

    invalidateAlloc(vk, device, colorOutputBuffer->getAllocation());
    tcu::ConstPixelBufferAccess resultBuffer =
        tcu::ConstPixelBufferAccess(vk::mapVkFormat(VK_FORMAT_R8G8B8A8_UNORM), render_size, render_size, instanceCount,
                                    (const void *)colorOutputBuffer->getAllocation().getHostPtr());

    const tcu::Vec4 white = tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f);

    for (uint32_t k = 0; k < instanceCount; ++k)
    {
        for (uint32_t j = 0; j < render_size; ++j)
        {
            for (uint32_t i = 0; i < render_size; ++i)
            {
                const tcu::Vec4 color = resultBuffer.getPixel(i, j, k).asFloat();
                if (color != white)
                {
                    log << tcu::TestLog::Message << "Color at (" << i << ", " << j
                        << ") is expected to be (1.0, 1.0, 1.0, 1.0), but was (" << color << ")"
                        << tcu::TestLog::EndMessage;
                    return tcu::TestStatus::fail("Fail");
                }
            }
        }
    }

    return tcu::TestStatus::pass("Pass");
}

class RemainingArrayLayersTest : public vkt::TestCase
{
public:
    RemainingArrayLayersTest(tcu::TestContext &testContext, const std::string &name, const TestParams &testParams)
        : vkt::TestCase(testContext, name)
        , m_testParams(testParams)
    {
    }
    virtual ~RemainingArrayLayersTest(void)
    {
    }
    virtual void initPrograms(SourceCollections &sourceCollections) const;
    virtual TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

private:
    const TestParams m_testParams;
};

void RemainingArrayLayersTest::initPrograms(SourceCollections &sourceCollections) const
{
    std::stringstream vert;
    std::stringstream geom;
    std::stringstream frag;

    vert << "#version 450\n"
         << "layout(location = 0) out int layerIndex;\n"
         << "void main() {\n"
         << "    vec2 pos = vec2(float(gl_VertexIndex & 1), float((gl_VertexIndex >> 1) & 1)) * 4.0f - 1.0f;\n"
         << "    gl_Position = vec4(pos, 0.0f, 1.0f);\n"
         << "    layerIndex = gl_InstanceIndex;\n"
         << "}\n";

    geom << "#version 450\n"
         << "\n"
         << "layout(location = 0) in int layerIndex[];\n"
         << "layout(triangles) in;\n"
         << "layout(triangle_strip, max_vertices = 3) out;\n"
         << "\n"
         << "void main() {\n"
         << "    for (int i = 0; i < 3; i++) {\n"
         << "        gl_Position = gl_in[i].gl_Position;\n"
         << "        gl_Layer = layerIndex[i];\n"
         << "        EmitVertex();\n"
         << "    }\n"
         << "    EndPrimitive();\n"
         << "}\n";

    frag << "#version 450\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "void main() {\n"
         << "    outColor = vec4(1.0f);\n"
         << "}\n";

    sourceCollections.glslSources.add("vert") << glu::VertexSource(vert.str());
    sourceCollections.glslSources.add("geom") << glu::GeometrySource(geom.str());
    sourceCollections.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

TestInstance *RemainingArrayLayersTest::createInstance(Context &context) const
{
    return new RemainingArrayLayersTestInstance(context, m_testParams);
}

void RemainingArrayLayersTest::checkSupport(Context &context) const
{
    // Check for renderpass2 extension if used
    if (m_testParams.groupParams->renderingType == RENDERING_TYPE_RENDERPASS2)
        context.requireDeviceFunctionality("VK_KHR_create_renderpass2");

    // Check for dynamic_rendering extension if used
    if (m_testParams.groupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING)
        context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");

    if (m_testParams.writeGlLayer)
        context.requireDeviceCoreFeature(vkt::DeviceCoreFeature::DEVICE_CORE_FEATURE_GEOMETRY_SHADER);
}

} // namespace

tcu::TestCaseGroup *createRenderPassRemainingArrayLayersTests(tcu::TestContext &testCtx,
                                                              const SharedGroupParams groupParams)
{
    de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "remaining_array_layers"));

    constexpr struct LayerTest
    {
        uint32_t baseLayer;
        uint32_t additionalLayers;
        const char *name;
    } layerTests[] = {
        {1, 1, "1_1"},
        {2, 2, "2_2"},
        {4, 1, "4_1"},
        {1, 4, "1_4"},
    };

    const struct FramebufferTest
    {
        bool multiLayer;
        bool useGlLayer;
        const char *name;
    } framebufferTests[] = {
        {false, false, "single_layer_fb"},
        {true, false, "multi_layer_fb"},
        {true, true, "multi_layer_fb_gl_layer"},
    };

    for (const auto framebufferTest : framebufferTests)
    {
        de::MovePtr<tcu::TestCaseGroup> layerGroup(new tcu::TestCaseGroup(testCtx, framebufferTest.name));
        for (const auto &layerTest : layerTests)
        {
            const TestParams testParams(layerTest.baseLayer, layerTest.additionalLayers, framebufferTest.multiLayer,
                                        framebufferTest.useGlLayer, groupParams);

            layerGroup->addChild(new RemainingArrayLayersTest(testCtx, layerTest.name, testParams));
        }
        testGroup->addChild(layerGroup.release());
    }

    return testGroup.release();
}

} // namespace renderpass
} // namespace vkt
