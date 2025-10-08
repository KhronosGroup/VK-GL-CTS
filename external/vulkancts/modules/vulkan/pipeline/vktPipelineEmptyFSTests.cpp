/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024 The Khronos Group Inc.
 * Copyright (c) 2024 Valve Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Tests for empty and missing Fragment Shaders.
 *//*--------------------------------------------------------------------*/

#include "vktPipelineEmptyFSTests.hpp"
#include "tcuImageCompare.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vktTestCase.hpp"

#include <iostream>
#include <vector>

namespace vkt::pipeline
{

using namespace vk;

namespace
{

enum class TestType
{
    Basic = 0,
    PrimitiveDiscard,
    MaskedSamples,
};

struct TestParams
{
    TestType testType;
    PipelineConstructionType pipelineConstructionType;
    VkShaderStageFlagBits lastVertexStage; // Last vertex shader stage: vertex, tessellation or geometry.
    bool emptyFS;                          // True: empty FS; False: do not include a fragment shader at all.

    TestParams(TestType type, PipelineConstructionType pipelineType, VkShaderStageFlagBits lastStage, bool noFS)
        : testType(type)
        , pipelineConstructionType(pipelineType)
        , lastVertexStage(lastStage)
        , emptyFS(noFS)
    {
        DE_ASSERT(lastIsVertex() || lastIsTessellation() || lastIsGeometry());
    }

    bool lastIsVertex(void) const
    {
        return (lastVertexStage == VK_SHADER_STAGE_VERTEX_BIT);
    }

    bool lastIsTessellation(void) const
    {
        return (lastVertexStage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT ||
                lastVertexStage == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
    }

    bool lastIsGeometry(void) const
    {
        return (lastVertexStage == VK_SHADER_STAGE_GEOMETRY_BIT);
    }
};

class EmptyFSInstance : public vkt::TestInstance
{
public:
    EmptyFSInstance(Context &context, const TestParams &params) : vkt::TestInstance(context), m_params(params)
    {
    }
    virtual ~EmptyFSInstance(void) = default;

    tcu::TestStatus iterate(void) override;

protected:
    const TestParams m_params;
};

tcu::TestStatus EmptyFSInstance::iterate(void)
{
    const auto &ctx = m_context.getContextCommonData();
    const tcu::IVec3 fbExtent(2, 2, 1);
    const auto pixelCount  = fbExtent.x() * fbExtent.y() * fbExtent.z();
    const auto vkExtent    = makeExtent3D(fbExtent);
    const auto fbFormat    = VK_FORMAT_R8G8B8A8_UNORM;
    const auto dsFormat    = VK_FORMAT_D16_UNORM;
    const auto tcuFormat   = mapVkFormat(dsFormat);
    const float depthThres = 0.000025f; // 1/65535 < depthThres < 2/65535
    const auto fbUsage     = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const auto dsUsage     = (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // Color buffer.
    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, vkExtent, fbFormat, fbUsage, VK_IMAGE_TYPE_2D);

    // Depth/stencil buffer.
    ImageWithBuffer dsBuffer(ctx.vkd, ctx.device, ctx.allocator, vkExtent, dsFormat, dsUsage, VK_IMAGE_TYPE_2D,
                             makeImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u));

    // Vertices.
    const float pixelWidth  = 2.0f / static_cast<float>(vkExtent.width);
    const float pixelHeight = 2.0f / static_cast<float>(vkExtent.height);
    const float horMargin   = pixelWidth / 4.0f;
    const float vertMargin  = pixelHeight / 4.0f;

    const auto calcCenter = [](int i, int size)
    { return (static_cast<float>(i) + 0.5f) / static_cast<float>(size) * 2.0f - 1.0f; };

    // One triangle per pixel with varying depth.
    std::vector<tcu::Vec4> vertices;
    for (int y = 0; y < fbExtent.y(); ++y)
        for (int x = 0; x < fbExtent.x(); ++x)
        {
            const float xCenter = calcCenter(x, fbExtent.x());
            const float yCenter = calcCenter(y, fbExtent.y());
            const int pixelId   = y * fbExtent.x() + x;
            const float depth   = static_cast<float>(pixelId) / static_cast<float>(pixelCount);

            // Triangle around the center.
            vertices.emplace_back(xCenter, yCenter - vertMargin, depth, 1.0f);
            vertices.emplace_back(xCenter - horMargin, yCenter + vertMargin, depth, 1.0f);
            vertices.emplace_back(xCenter + horMargin, yCenter + vertMargin, depth, 1.0f);
        }

    // Vertex buffer
    const auto vbSize = static_cast<VkDeviceSize>(de::dataSize(vertices));
    const auto vbInfo = makeBufferCreateInfo(vbSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    BufferWithMemory vertexBuffer(ctx.vkd, ctx.device, ctx.allocator, vbInfo, MemoryRequirement::HostVisible);
    const auto vbAlloc  = vertexBuffer.getAllocation();
    void *vbData        = vbAlloc.getHostPtr();
    const auto vbOffset = static_cast<VkDeviceSize>(0);

    deMemcpy(vbData, de::dataOrNull(vertices), de::dataSize(vertices));
    flushAlloc(ctx.vkd, ctx.device, vbAlloc); // strictly speaking, not needed.

    // Pipeline layout
    PipelineLayoutWrapper pipelineLayout(m_params.pipelineConstructionType, ctx.vkd, ctx.device);
    RenderPassWrapper renderPass(m_params.pipelineConstructionType, ctx.vkd, ctx.device, fbFormat, dsFormat);
    std::vector<VkImage> images{colorBuffer.getImage(), dsBuffer.getImage()};
    std::vector<VkImageView> imageViews{colorBuffer.getImageView(), dsBuffer.getImageView()};

    DE_ASSERT(images.size() == imageViews.size());
    renderPass.createFramebuffer(ctx.vkd, ctx.device, de::sizeU32(images), de::dataOrNull(images),
                                 de::dataOrNull(imageViews), vkExtent.width, vkExtent.height);

    // Modules.
    const auto &binaries  = m_context.getBinaryCollection();
    const auto vertModule = ShaderWrapper(ctx.vkd, ctx.device, binaries.get("vert"));
    const auto tescModule =
        (m_params.lastIsTessellation() ? ShaderWrapper(ctx.vkd, ctx.device, binaries.get("tesc")) : ShaderWrapper());
    const auto teseModule =
        (m_params.lastIsTessellation() ? ShaderWrapper(ctx.vkd, ctx.device, binaries.get("tese")) : ShaderWrapper());
    const auto geomModule =
        (m_params.lastIsGeometry() ? ShaderWrapper(ctx.vkd, ctx.device, binaries.get("geom")) : ShaderWrapper());
    const auto fragModule =
        (m_params.emptyFS ? ShaderWrapper(ctx.vkd, ctx.device, binaries.get("frag")) : ShaderWrapper());

    const std::vector<VkViewport> viewports(1u, makeViewport(vkExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(vkExtent));

    VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = initVulkanStructure();
    rasterizationStateCreateInfo.lineWidth                              = 1.0f;

    VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = initVulkanStructure();
    depthStencilStateCreateInfo.depthTestEnable                       = VK_TRUE;
    depthStencilStateCreateInfo.depthWriteEnable                      = VK_TRUE;
    depthStencilStateCreateInfo.depthCompareOp                        = VK_COMPARE_OP_ALWAYS;

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = initVulkanStructure();
    inputAssemblyStateCreateInfo.topology =
        (m_params.lastIsTessellation() ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    GraphicsPipelineWrapper pipelineWrapper(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device,
                                            m_context.getDeviceExtensions(), m_params.pipelineConstructionType);
    pipelineWrapper.setMonolithicPipelineLayout(pipelineLayout);
    pipelineWrapper.setDefaultVertexInputState(true);
    pipelineWrapper.setDefaultColorBlendState();
    pipelineWrapper.setDefaultMultisampleState();
    pipelineWrapper.setDefaultPatchControlPoints(3u);
    pipelineWrapper.setupVertexInputState(nullptr, &inputAssemblyStateCreateInfo);
    pipelineWrapper.setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, *renderPass, 0u, vertModule,
                                                     &rasterizationStateCreateInfo, tescModule, teseModule, geomModule);
    pipelineWrapper.setupFragmentShaderState(pipelineLayout, *renderPass, 0u, fragModule, &depthStencilStateCreateInfo);
    pipelineWrapper.setupFragmentOutputState(*renderPass);
    pipelineWrapper.buildPipeline();

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    const std::vector<VkClearValue> clearValues{makeClearValueColor(clearColor), makeClearValueDepthStencil(0.0f, 0u)};
    beginCommandBuffer(ctx.vkd, cmdBuffer);
    renderPass.begin(ctx.vkd, cmdBuffer, scissors.at(0u), de::sizeU32(clearValues), de::dataOrNull(clearValues));
    ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vbOffset);
    pipelineWrapper.bind(cmdBuffer);
    ctx.vkd.cmdDraw(cmdBuffer, de::sizeU32(vertices), 1u, 0u, 0u);
    renderPass.end(ctx.vkd, cmdBuffer);
    copyImageToBuffer(ctx.vkd, cmdBuffer, dsBuffer.getImage(), dsBuffer.getBuffer(), fbExtent.swizzle(0, 1),
                      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                      1u, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_ASPECT_DEPTH_BIT,
                      (VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT));
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    // Verify depth output.
    invalidateAlloc(ctx.vkd, ctx.device, dsBuffer.getBufferAllocation());
    tcu::PixelBufferAccess resultAccess(tcuFormat, fbExtent, dsBuffer.getBufferAllocation().getHostPtr());

    tcu::TextureLevel referenceLevel(tcuFormat, fbExtent.x(), fbExtent.y());
    auto referenceAccess = referenceLevel.getAccess();

    for (int y = 0; y < fbExtent.y(); ++y)
        for (int x = 0; x < fbExtent.x(); ++x)
        {
            const int pixelId = y * fbExtent.x() + x;
            const float depth = static_cast<float>(pixelId) / static_cast<float>(pixelCount);

            referenceAccess.setPixDepth(depth, x, y);
        }

    auto &log = m_context.getTestContext().getLog();
    if (!tcu::dsThresholdCompare(log, "DepthResult", "", referenceAccess, resultAccess, depthThres,
                                 tcu::COMPARE_LOG_EVERYTHING))
        return tcu::TestStatus::fail("Unexpected color in result buffer; check log for details");

    return tcu::TestStatus::pass("Pass");
}

class EmptyFSSelectiveDSUpdateInstance : public vkt::TestInstance
{
public:
    EmptyFSSelectiveDSUpdateInstance(Context &context, const TestParams &params)
        : vkt::TestInstance(context)
        , m_params(params)
    {
    }
    virtual ~EmptyFSSelectiveDSUpdateInstance(void) = default;

    tcu::TestStatus iterate(void) override;

protected:
    const TestParams m_params;
};

tcu::TestStatus EmptyFSSelectiveDSUpdateInstance::iterate(void)
{
    const auto &vki    = m_context.getInstanceInterface();
    const auto &vk     = m_context.getDeviceInterface();
    const auto &device = m_context.getDevice();
    const auto pd      = m_context.getPhysicalDevice();
    auto &allocator    = m_context.getDefaultAllocator();
    VkQueue queue      = m_context.getUniversalQueue();

    bool testPrimitiveDiscard = (m_params.testType == TestType::PrimitiveDiscard);

    // pick depth stencil format (one of those two has to be supported)
    VkExtent3D extent{8u, 8u, 1u};
    VkImageType imageType = VK_IMAGE_TYPE_2D;
    VkFormat dsFormat     = VK_FORMAT_D24_UNORM_S8_UINT;
    VkFormat dReadFormat  = VK_FORMAT_D24_UNORM_S8_UINT;
    VkImageUsageFlags dsUsage =
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    VkImageFormatProperties imageFormatProperties;
    auto dsFormatCheck = vki.getPhysicalDeviceImageFormatProperties(pd, dsFormat, imageType, VK_IMAGE_TILING_OPTIMAL,
                                                                    dsUsage, 0, &imageFormatProperties);
    if (dsFormatCheck != VK_SUCCESS)
    {
        dsFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;

        // when copying depth attachment data we read just depth component
        dReadFormat = VK_FORMAT_D32_SFLOAT;
    }

    // depth/stencil image with buffer
    const VkImageSubresourceRange dSRR  = makeImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u);
    const VkSampleCountFlagBits samples = testPrimitiveDiscard ? VK_SAMPLE_COUNT_1_BIT : VK_SAMPLE_COUNT_4_BIT;
    ImageWithBuffer dsImageWithBuffer(vk, device, allocator, extent, dsFormat, dsUsage, imageType, dSRR, 1, samples);

    VkImage image         = dsImageWithBuffer.getImage();
    VkImageView imageView = dsImageWithBuffer.getImageView();

    // create renderpass
    const VkAttachmentDescription attachment{0,
                                             dsFormat,
                                             samples,
                                             VK_ATTACHMENT_LOAD_OP_CLEAR,
                                             VK_ATTACHMENT_STORE_OP_STORE,
                                             VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                             VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                             VK_IMAGE_LAYOUT_UNDEFINED,
                                             VK_IMAGE_LAYOUT_GENERAL};
    const VkAttachmentReference depthStencilAttachment{0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    const VkSubpassDescription subpass{0,       VK_PIPELINE_BIND_POINT_GRAPHICS, 0, nullptr, 0, nullptr,
                                       nullptr, &depthStencilAttachment,         0, nullptr};
    VkRenderPassCreateInfo renderpassCreateInfo = initVulkanStructure();
    renderpassCreateInfo.attachmentCount        = 1u;
    renderpassCreateInfo.pAttachments           = &attachment;
    renderpassCreateInfo.subpassCount           = 1u;
    renderpassCreateInfo.pSubpasses             = &subpass;
    RenderPassWrapper renderPass(m_params.pipelineConstructionType, vk, device, &renderpassCreateInfo);

    // create framebuffer
    renderPass.createFramebuffer(vk, device, 1, &image, &imageView, extent.width, extent.height, 1u);

    // for TestType::PrimitiveDiscard vertex shader is writing to gl_CullDistance, discarding some primitives but not others;
    // for TestType::MaskedSamples vertex shader just outputs vartex position;
    // there is no fragment shader in the pipeline for both test types
    auto &bc              = m_context.getBinaryCollection();
    const auto vertModule = ShaderWrapper(vk, device, bc.get("vert"));

    const std::vector<VkViewport> viewports(1u, makeViewport(extent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(extent));
    PipelineLayoutWrapper graphicsPipelineLayout(m_params.pipelineConstructionType, vk, device);

    // create vertex buffer (just xy components)
    const float size = 3.0f;
    float vertices[]{
        0.0f,  0.0f,  // 0 - center
        0.0f,  size,  // 1 - top
        -size, 0.0f,  // 2 - left
        0.0f,  -size, // 3 - bottom
        size,  0.0f,  // 4 - right
        -size, 0.0f,  // 5 - left duplicated to prevent from culling in VS (for TestType::PrimitiveDiscard)
        size,  0.0f,  // 6 - right duplicated to prevent triangle from cull in VS
    };
    const auto vbSize = static_cast<VkDeviceSize>(DE_LENGTH_OF_ARRAY(vertices) * sizeof(float));
    const auto vbInfo = makeBufferCreateInfo(vbSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    BufferWithMemory vertexBuffer(vk, device, allocator, vbInfo, MemoryRequirement::HostVisible);
    const auto vbAlloc = vertexBuffer.getAllocation();
    deMemcpy(vbAlloc.getHostPtr(), vertices, vbSize);
    flushAlloc(vk, device, vbAlloc);

    // create index buffer for four triangles, each big enough to fill quarter of framebuffer
    uint32_t verticesIndex[]{
        0, 1, 2, // top-left triangle
        0, 2, 3, // bottom-left triangle
        0, 3, 4, // bottom-right triangle
        0, 4, 1, // top-right triangle
    };

    if (testPrimitiveDiscard)
    {
        // when primitive discard is tested VS will cull all triangles
        // that have vertex with gl_VertexIndex smaller then 5;
        // we need to prevent two triangles from beeing culled
        verticesIndex[2] = 5; // last vertex of first triangle
        verticesIndex[8] = 6; // last vertex of third triangle
    }

    // create index buffer
    const auto ibSize = static_cast<VkDeviceSize>(DE_LENGTH_OF_ARRAY(verticesIndex) * sizeof(uint32_t));
    const auto ibInfo = makeBufferCreateInfo(ibSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    BufferWithMemory indexBuffer(vk, device, allocator, ibInfo, MemoryRequirement::HostVisible);
    const auto ibAlloc = indexBuffer.getAllocation();
    deMemcpy(ibAlloc.getHostPtr(), verticesIndex, ibSize);
    flushAlloc(vk, device, ibAlloc);

    // create ssbo buffer; used only for TestType::MaskedSamples
    const auto ssboSize = static_cast<VkDeviceSize>(extent.width * extent.height * sizeof(float));
    const auto ssboInfo = makeBufferCreateInfo(ssboSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    BufferWithMemory ssboBuffer(vk, device, allocator, ssboInfo, MemoryRequirement::HostVisible);

    // define custom pipeline state
    const VkVertexInputBindingDescription vertexBinding{0u, 8u, VK_VERTEX_INPUT_RATE_VERTEX};
    const VkVertexInputAttributeDescription vertexAttribs{
        0u, // location
        0u, // binding
        VK_FORMAT_R32G32_SFLOAT,
        0u, // offset
    };
    VkPipelineVertexInputStateCreateInfo vertexInputState = initVulkanStructure();
    vertexInputState.vertexBindingDescriptionCount        = 1u;
    vertexInputState.pVertexBindingDescriptions           = &vertexBinding;
    vertexInputState.vertexAttributeDescriptionCount      = 1u;
    vertexInputState.pVertexAttributeDescriptions         = &vertexAttribs;

    VkPipelineDepthStencilStateCreateInfo depthStencilState = initVulkanStructure();
    depthStencilState.depthTestEnable                       = VK_TRUE;
    depthStencilState.depthWriteEnable                      = VK_TRUE;
    depthStencilState.depthCompareOp                        = VK_COMPARE_OP_ALWAYS;

    VkPipelineColorBlendStateCreateInfo colorBlendState = initVulkanStructure();

    // 0x5 is 0101b so we write to 3 samples out of 4 in TestType::MaskedSamples;
    // this will affect occlusion query result
    VkSampleMask sampleMask                               = 0x5;
    VkPipelineMultisampleStateCreateInfo multisampleState = initVulkanStructure();
    multisampleState.rasterizationSamples                 = samples;
    multisampleState.minSampleShading                     = 1.0f;
    multisampleState.pSampleMask                          = &sampleMask;

    // create pipeline
    GraphicsPipelineWrapper pipelineWrapper(vki, vk, pd, device, m_context.getDeviceExtensions(),
                                            m_params.pipelineConstructionType);
    pipelineWrapper.setMonolithicPipelineLayout(graphicsPipelineLayout)
        .setDefaultVertexInputState(true)
        .setDefaultMultisampleState()
        .setDefaultRasterizationState()
        .setDefaultPatchControlPoints(3u)
        .setupVertexInputState(&vertexInputState)
        .setupPreRasterizationShaderState(viewports, scissors, graphicsPipelineLayout, *renderPass, 0u, vertModule)
        .setupFragmentShaderState(graphicsPipelineLayout, *renderPass, 0u, {}, &depthStencilState, &multisampleState)
        .setupFragmentOutputState(*renderPass, 0u, &colorBlendState, &multisampleState)
        .buildPipeline();

    Move<VkShaderModule> compModule;
    Move<VkPipelineLayout> computePipelineLayout;
    Move<VkPipeline> computePipeline;

    // create descriptor set
    Move<VkDescriptorSetLayout> computeDescriptorSetLayout;
    Move<VkDescriptorPool> computeDescriptorPool;
    Move<VkDescriptorSet> computeDescriptorSet;

    if (!testPrimitiveDiscard)
    {
        computeDescriptorSetLayout =
            DescriptorSetLayoutBuilder()
                .addSingleBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                .build(vk, device);

        computeDescriptorPool = DescriptorPoolBuilder()
                                    .addType(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
                                    .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
                                    .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

        computeDescriptorSet = makeDescriptorSet(vk, device, *computeDescriptorPool, *computeDescriptorSetLayout);

        const VkDescriptorImageInfo imageDescriptorInfo =
            makeDescriptorImageInfo(VK_NULL_HANDLE, imageView, VK_IMAGE_LAYOUT_GENERAL);
        const VkDescriptorBufferInfo bufferDescriptorInfo = makeDescriptorBufferInfo(*ssboBuffer, 0ull, ssboSize);

        DescriptorSetUpdateBuilder()
            .writeSingle(*computeDescriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                         VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &imageDescriptorInfo)
            .writeSingle(*computeDescriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptorInfo)
            .update(vk, device);

        VkPipelineLayoutCreateInfo layoutCreateInfo = initVulkanStructure();
        layoutCreateInfo.setLayoutCount             = 1u;
        layoutCreateInfo.pSetLayouts                = &*computeDescriptorSetLayout;
        computePipelineLayout                       = createPipelineLayout(vk, device, &layoutCreateInfo);

        compModule      = createShaderModule(vk, device, bc.get("comp"));
        computePipeline = makeComputePipeline(vk, device, *computePipelineLayout, 0, nullptr, *compModule, 0);
    }

    // create query object
    bool usePreciseOcclusionQuery       = m_context.getDeviceFeatures().occlusionQueryPrecise;
    VkQueryPoolCreateInfo queryPoolInfo = initVulkanStructure();
    queryPoolInfo.queryType             = VK_QUERY_TYPE_OCCLUSION;
    queryPoolInfo.queryCount            = 1;
    auto queryPool                      = createQueryPool(vk, device, &queryPoolInfo);

    const tcu::Vec4 clearColor{0.0};
    const VkDeviceSize bindingOffset = 0;
    const uint32_t queueFamilyIndex  = m_context.getUniversalQueueFamilyIndex();
    auto cmdPool   = createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
    auto cmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    beginCommandBuffer(vk, *cmdBuffer);

    vk.cmdResetQueryPool(*cmdBuffer, *queryPool, 0u, 1u);

    renderPass.begin(vk, *cmdBuffer, scissors.at(0u), clearColor);
    pipelineWrapper.bind(*cmdBuffer);
    vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &*vertexBuffer, &bindingOffset);
    vk.cmdBindIndexBuffer(*cmdBuffer, *indexBuffer, 0, vk::VK_INDEX_TYPE_UINT32);

    vk.cmdBeginQuery(*cmdBuffer, *queryPool, 0u, usePreciseOcclusionQuery ? VK_QUERY_CONTROL_PRECISE_BIT : 0);
    vk.cmdDrawIndexed(*cmdBuffer, std::size(verticesIndex), 1u, 0u, 0u, 0u);
    vk.cmdEndQuery(*cmdBuffer, *queryPool, 0u);

    renderPass.end(vk, *cmdBuffer);

    if (testPrimitiveDiscard)
    {
        // copy single sampled depth to buffer
        copyImageToBuffer(vk, *cmdBuffer, dsImageWithBuffer.getImage(), dsImageWithBuffer.getBuffer(),
                          tcu::IVec2(extent.width, extent.height), VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                          VK_IMAGE_LAYOUT_GENERAL, 1u, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_ASPECT_DEPTH_BIT,
                          VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
    }
    else
    {
        // wait for multisampled depth
        auto barrier = makeMemoryBarrier(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
        vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
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
    }

    endCommandBuffer(vk, *cmdBuffer);
    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    // get occlusion query result
    VkDeviceSize queryResult = 0;
    vk.getQueryPoolResults(device, *queryPool, 0u, 1u, sizeof(VkDeviceSize), &queryResult, sizeof(VkDeviceSize),
                           VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);

    bool pass = false;
    auto &log = m_context.getTestContext().getLog();

    if (testPrimitiveDiscard)
    {
        // get depth data from buffer
        auto dsAllocation = dsImageWithBuffer.getBufferAllocation();
        invalidateAlloc(vk, device, dsAllocation);
        tcu::PixelBufferAccess resultAccess(mapVkFormat(dReadFormat), extent.width, extent.height, extent.depth,
                                            dsAllocation.getHostPtr());

        // drawing should update the depth/stencil buffer only for primitives that were not discarded by cull distance;
        // checking just centers of each quarters
        pass = (resultAccess.getPixDepth(3, 3) < 0.01) && (resultAccess.getPixDepth(7, 3) > 0.99) &&
               (resultAccess.getPixDepth(3, 7) > 0.99) && (resultAccess.getPixDepth(7, 7) < 0.01);

        if (!pass)
            log << tcu::TestLog::Image("Depth", "", resultAccess);

        // occlusion queries should only be incremented for primitives that are not discarded
        // 8 * 8 / 2 = 32
        pass &= usePreciseOcclusionQuery ? (queryResult == 32) : (queryResult > 0);
    }
    else
    {
        // get ssbo buffer
        invalidateAlloc(vk, device, ssboBuffer.getAllocation());
        const float *data = reinterpret_cast<float *>(ssboBuffer.getAllocation().getHostPtr());
        const float *end  = data + (extent.width * extent.height);

        // part of verification is done in compute shader, we just need to check if all elements have value of 2.0
        auto result = std::find_if(data, end, [](float f) { return (f < 1.99f) || (f > 2.01f); });
        pass        = (result == end);

        // 8 * 8 * 2 samples = 128
        pass &= usePreciseOcclusionQuery ? (queryResult == 128) : (queryResult > 0);
    }

    if (pass)
        return tcu::TestStatus::pass("Pass");

    log << tcu::TestLog::Message << "Query result: " << queryResult << tcu::TestLog::EndMessage;

    return tcu::TestStatus::fail("Fail");
}

class EmptyFSCase : public vkt::TestCase
{
public:
    EmptyFSCase(tcu::TestContext &testCtx, const std::string &name, const TestParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~EmptyFSCase(void) = default;

    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;
    void checkSupport(Context &context) const override;

protected:
    const TestParams m_params;
};

TestInstance *EmptyFSCase::createInstance(Context &context) const
{
    if ((m_params.testType == TestType::PrimitiveDiscard) || (m_params.testType == TestType::MaskedSamples))
        return new EmptyFSSelectiveDSUpdateInstance(context, m_params);

    return new EmptyFSInstance(context, m_params);
}

void EmptyFSCase::checkSupport(Context &context) const
{
    if (m_params.lastIsTessellation())
    {
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SHADER_TESSELLATION_AND_GEOMETRY_POINT_SIZE);
    }

    if (m_params.lastIsGeometry())
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);

    if (m_params.testType == TestType::PrimitiveDiscard)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SHADER_CULL_DISTANCE);

    const auto ctx = context.getContextCommonData();
    checkPipelineConstructionRequirements(ctx.vki, ctx.physicalDevice, m_params.pipelineConstructionType);
}

void EmptyFSCase::initPrograms(SourceCollections &programCollection) const
{
    if (m_params.testType == TestType::PrimitiveDiscard)
    {
        std::string vertSource = "#version 460\n"
                                 "layout (location=0) in vec2 inPos;\n"
                                 "out gl_PerVertex {\n"
                                 "  vec4 gl_Position;\n"
                                 "  float gl_CullDistance[1];\n"
                                 "};\n"
                                 "void main (void)\n"
                                 "{\n"
                                 // cull two triangles that dont have vertex with index 5 and 6
                                 "    gl_CullDistance[0] = 0.5 - float((gl_VertexIndex < 5));\n"
                                 "    gl_Position = vec4(inPos, 1.0, 1.0);\n"
                                 "}\n";

        programCollection.glslSources.add("vert") << glu::VertexSource(vertSource);
        return;
    }
    if (m_params.testType == TestType::MaskedSamples)
    {
        std::string vertSource = "#version 460\n"
                                 "layout (location=0) in vec2 inPos;\n"
                                 "out gl_PerVertex {\n"
                                 "  vec4 gl_Position;\n"
                                 "};\n"
                                 "void main (void)\n"
                                 "{\n"
                                 "    gl_Position = vec4(inPos, 1.0, 1.0);\n"
                                 "}\n";
        programCollection.glslSources.add("vert") << glu::VertexSource(vertSource);

        std::string compSource = "#version 460\n"
                                 "#extension GL_EXT_samplerless_texture_functions : enable\n"
                                 "layout(local_size_x = 8, local_size_y = 8) in;\n"
                                 "layout(set = 0, binding = 0) uniform texture2DMS inputImage;\n"
                                 "layout(set = 0, binding = 1) buffer Data { float v[]; };\n"
                                 "void main()\n"
                                 "{\n"
                                 "  ivec2 uv = ivec2(gl_GlobalInvocationID.xy);\n"
                                 "  float samplesOne  = texelFetch(inputImage, uv, 0).r +\n"
                                 "                      texelFetch(inputImage, uv, 2).r;\n"
                                 "  float samplesZero = texelFetch(inputImage, uv, 1).r +\n"
                                 "                      texelFetch(inputImage, uv, 3).r;\n"
                                 "  v[gl_LocalInvocationIndex] = samplesOne - samplesZero;\n" // we expect 2 - 0 = 2
                                 "}\n";
        programCollection.glslSources.add("comp") << glu::ComputeSource(compSource);
        return;
    }

    std::stringstream userOutputsDecl;
    userOutputsDecl << "layout (location=0) out float added;\n"
                    << "layout (location=1) out float multiplied;\n";

    std::ostringstream vert;
    vert << "#version 460\n"
         << "layout (location=0) in vec4 inPos;\n"
         << (m_params.lastIsVertex() ? userOutputsDecl.str() : "") << "out gl_PerVertex\n"
         << "{\n"
         << "    vec4  gl_Position;\n"
         << "    float gl_PointSize;\n"
         << "};\n"
         << "void main (void)\n"
         << "{\n"
         << "    gl_Position  = inPos;\n"
         << "    gl_PointSize = 1.0;\n"
         << (m_params.lastIsVertex() ? "    added        = inPos.x + 1000.0;\n"
                                       "    multiplied   = inPos.y * 1000.0;\n" :
                                       "")
         << "}\n";
    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

    if (m_params.lastIsTessellation())
    {
        // Add passthrough tessellation control shader.
        std::ostringstream tesc;
        tesc << "#version 460\n"
             << "layout (vertices=3) out;\n"
             << "in gl_PerVertex\n"
             << "{\n"
             << "    vec4  gl_Position;\n"
             << "    float gl_PointSize;\n"
             << "} gl_in[gl_MaxPatchVertices];\n"
             << "out gl_PerVertex\n"
             << "{\n"
             << "    vec4  gl_Position;\n"
             << "    float gl_PointSize;\n"
             << "} gl_out[];\n"
             << "void main (void)\n"
             << "{\n"
             << "    gl_TessLevelInner[0] = 1.0;\n"
             << "    gl_TessLevelInner[1] = 1.0;\n"
             << "    gl_TessLevelOuter[0] = 1.0;\n"
             << "    gl_TessLevelOuter[1] = 1.0;\n"
             << "    gl_TessLevelOuter[2] = 1.0;\n"
             << "    gl_TessLevelOuter[3] = 1.0;\n"
             << "    gl_out[gl_InvocationID].gl_Position  = gl_in[gl_InvocationID].gl_Position;\n"
             << "    gl_out[gl_InvocationID].gl_PointSize = gl_in[gl_InvocationID].gl_PointSize;\n"
             << "}\n";

        programCollection.glslSources.add("tesc") << glu::TessellationControlSource(tesc.str());

        std::ostringstream tese;
        tese << "#version 460\n"
             << "layout (triangles, fractional_odd_spacing, cw) in;\n"
             << userOutputsDecl.str() << "in gl_PerVertex\n"
             << "{\n"
             << "    vec4  gl_Position;\n"
             << "    float gl_PointSize;\n"
             << "} gl_in[gl_MaxPatchVertices];\n"
             << "out gl_PerVertex\n"
             << "{\n"
             << "    vec4  gl_Position;\n"
             << "    float gl_PointSize;\n"
             << "};\n"
             << "void main (void)\n"
             << "{\n"
             << "    vec4 pos     = (gl_TessCoord.x * gl_in[0].gl_Position) +\n"
             << "                   (gl_TessCoord.y * gl_in[1].gl_Position) +\n"
             << "                   (gl_TessCoord.z * gl_in[2].gl_Position);\n"
             << "    gl_Position  = pos;\n"
             << "    gl_PointSize = gl_in[0].gl_PointSize;\n"
             << "    added        = pos.x + 1000.0;\n"
             << "    multiplied   = pos.y * 1000.0;\n"
             << "}\n";

        programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(tese.str());
    }

    if (m_params.lastIsGeometry())
    {
        const auto vertexCount = 3u;

        std::ostringstream geom;
        geom << "#version 450\n"
             << "layout (triangles) in;\n"
             << "layout (triangle_strip, max_vertices=" << vertexCount << ") out;\n"
             << userOutputsDecl.str() << "in gl_PerVertex\n"
             << "{\n"
             << "    vec4  gl_Position;\n"
             << "    float gl_PointSize;\n"
             << "} gl_in[" << vertexCount << "];\n"
             << "out gl_PerVertex\n"
             << "{\n"
             << "    vec4  gl_Position;\n"
             << "    float gl_PointSize;\n"
             << "};\n"
             << "void main() {\n";

        for (uint32_t i = 0u; i < vertexCount; ++i)
        {
            geom << "    gl_Position  = gl_in[" << i << "].gl_Position;\n"
                 << "    gl_PointSize = gl_in[" << i << "].gl_PointSize;\n"
                 << "    added        = gl_in[" << i << "].gl_Position.x + 1000.0;\n"
                 << "    multiplied   = gl_in[" << i << "].gl_Position.y * 1000.0;\n"
                 << "    EmitVertex();\n";
        }

        geom << "}\n";
        programCollection.glslSources.add("geom") << glu::GeometrySource(geom.str());
    }

    if (m_params.emptyFS)
    {
        std::ostringstream frag;
        frag << "#version 460\n"
             << "layout (location=0) in float added;\n"
             << "layout (location=1) in float multiplied;\n"
             << "void main (void) {}\n";
        programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
    }
}

} // anonymous namespace

tcu::TestCaseGroup *createEmptyFSTests(tcu::TestContext &testCtx, PipelineConstructionType pipelineType)
{
    de::MovePtr<tcu::TestCaseGroup> emptyFSTests(new tcu::TestCaseGroup(testCtx, "empty_fs"));

    const struct
    {
        VkShaderStageFlagBits shaderStage;
        const char *name;
    } vertexStages[] = {
        {VK_SHADER_STAGE_VERTEX_BIT, "vert"},
        {VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, "tess"},
        {VK_SHADER_STAGE_VERTEX_BIT, "geom"},
    };

    for (const auto &stage : vertexStages)
        for (const bool emptyFS : {false, true})
        {
            const std::string suffix(emptyFS ? "_empty_fs" : "_no_fs");
            const std::string testName = stage.name + suffix;
            const TestParams params{TestType::Basic, pipelineType, stage.shaderStage, emptyFS};

            emptyFSTests->addChild(new EmptyFSCase(testCtx, testName, params));
        }

    TestParams params{TestType::PrimitiveDiscard, pipelineType, VK_SHADER_STAGE_VERTEX_BIT, false};
    emptyFSTests->addChild(new EmptyFSCase(testCtx, "primitive_discard", params));
    params.testType = TestType::MaskedSamples;
    emptyFSTests->addChild(new EmptyFSCase(testCtx, "masked_samples", params));

    return emptyFSTests.release();
}

} // namespace vkt::pipeline
