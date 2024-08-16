/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 The Khronos Group Inc.
 * Copyright (c) 2023 Valve Corporation.
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
 * \brief Extended dynamic state misc tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineExtendedDynamicStateMiscTests.hpp"
#include "tcuTextureUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"

#include "tcuImageCompare.hpp"

#include "deUniquePtr.hpp"

#include <sstream>
#include <vector>
#include <memory>
#include <utility>

namespace vkt
{
namespace pipeline
{

namespace
{

using namespace vk;

constexpr uint32_t kVertexCount = 4u;

void checkDynamicRasterizationSamplesSupport(Context &context)
{
#ifndef CTS_USES_VULKANSC
    if (!context.getExtendedDynamicState3FeaturesEXT().extendedDynamicState3RasterizationSamples)
        TCU_THROW(NotSupportedError, "extendedDynamicState3RasterizationSamples not supported");
#else
    DE_UNREF(context);
    TCU_THROW(NotSupportedError, "extendedDynamicState3RasterizationSamples not supported");
#endif // CTS_USES_VULKANSC
}

void sampleShadingWithDynamicSampleCountSupport(Context &context, PipelineConstructionType pipelineConstructionType)
{
    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          pipelineConstructionType);
    checkDynamicRasterizationSamplesSupport(context);
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_FRAGMENT_STORES_AND_ATOMICS);
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SAMPLE_RATE_SHADING);
}

void initFullScreenQuadVertexProgram(vk::SourceCollections &programCollection, const char *name)
{
    std::ostringstream vert;
    vert << "#version 460\n"
         << "vec2 positions[" << kVertexCount << "] = vec2[](\n"
         << "    vec2(-1.0, -1.0),\n"
         << "    vec2(-1.0,  1.0),\n"
         << "    vec2( 1.0, -1.0),\n"
         << "    vec2( 1.0,  1.0)\n"
         << ");\n"
         << "void main (void) {\n"
         << "    gl_Position = vec4(positions[gl_VertexIndex % " << kVertexCount << "], 0.0, 1.0);\n"
         << "}\n";
    programCollection.glslSources.add(name) << glu::VertexSource(vert.str());
}

void initBlueAndAtomicCounterFragmentProgram(vk::SourceCollections &programCollection, const char *name)
{
    std::ostringstream frag;
    frag << "#version 460\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "layout (set=0, binding=0) buffer InvocationCounterBlock { uint invocations; } counterBuffer;\n"
         << "void main (void) {\n"
         << "    uint sampleId = gl_SampleID;\n" // Enable sample shading for shader objects by reading gl_SampleID
         << "    atomicAdd(counterBuffer.invocations, 1u);\n"
         << "    outColor = vec4(0.0, 0.0, 1.0, 1.0);\n"
         << "}\n";
    programCollection.glslSources.add(name) << glu::FragmentSource(frag.str());
}

void sampleShadingWithDynamicSampleCountPrograms(vk::SourceCollections &programCollection, PipelineConstructionType)
{
    initFullScreenQuadVertexProgram(programCollection, "vert");
    initBlueAndAtomicCounterFragmentProgram(programCollection, "frag");
}

void verifyValueInRange(uint32_t value, uint32_t minValue, uint32_t maxValue, const char *valueDesc)
{
    if (value < minValue || value > maxValue)
    {
        std::ostringstream msg;
        msg << "Unexpected value found for " << valueDesc << ": " << value << " not in range [" << minValue << ", "
            << maxValue << "]";
        TCU_FAIL(msg.str());
    }
}
/*
 * begin cmdbuf
 * bind pipeline with sample shading disabled
 * call vkCmdSetRasterizationSamplesEXT(samples > 1)
 * draw
 * bind pipeline with sample shading enabled
 * draw
 * sample shading should work for both draws with the expected number of samples
 *
 * Each draw will use one half of the framebuffer, controlled by the viewport and scissor.
 */
tcu::TestStatus sampleShadingWithDynamicSampleCount(Context &context, PipelineConstructionType constructionType)
{
    const auto ctx = context.getContextCommonData();
    const tcu::IVec3 fbExtent(2, 2, 1);
    const auto vkExtent           = makeExtent3D(fbExtent);
    const auto colorFormat        = VK_FORMAT_R8G8B8A8_UNORM;
    const auto colorUsage         = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const auto descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    const auto descriptorStages   = VK_SHADER_STAGE_FRAGMENT_BIT;
    const auto kNumDraws          = 2u;
    const auto bindPoint          = VK_PIPELINE_BIND_POINT_GRAPHICS;
    const auto colorSRR           = makeDefaultImageSubresourceRange();
    const auto kMultiSampleCount  = VK_SAMPLE_COUNT_4_BIT;
    const auto kSingleSampleCount = VK_SAMPLE_COUNT_1_BIT;
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 0.0f);
    const tcu::Vec4 geomColor(0.0f, 0.0f, 1.0f, 1.0f); // Must match frag shader.
    const auto topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    // Color buffers.
    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, vkExtent, colorFormat, colorUsage, VK_IMAGE_TYPE_2D,
                                colorSRR, 1u, kMultiSampleCount);
    ImageWithBuffer resolveBuffer(ctx.vkd, ctx.device, ctx.allocator, vkExtent, colorFormat, colorUsage,
                                  VK_IMAGE_TYPE_2D, colorSRR, 1u, kSingleSampleCount);

    // Counter buffers.
    using BufferPtr = std::unique_ptr<BufferWithMemory>;
    using BufferVec = std::vector<BufferPtr>;

    const auto counterBufferSize = static_cast<VkDeviceSize>(sizeof(uint32_t));
    const auto counterBufferInfo = makeBufferCreateInfo(counterBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    BufferVec counterBuffers;

    for (uint32_t drawIdx = 0u; drawIdx < kNumDraws; ++drawIdx)
    {
        BufferPtr counterBuffer(new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, counterBufferInfo,
                                                     MemoryRequirement::HostVisible));
        auto &counterBufferAlloc = counterBuffer->getAllocation();
        void *counterBufferPtr   = counterBufferAlloc.getHostPtr();

        deMemset(counterBufferPtr, 0, static_cast<size_t>(counterBufferSize));
        flushAlloc(ctx.vkd, ctx.device, counterBufferAlloc);

        counterBuffers.emplace_back(std::move(counterBuffer));
    }

    // Descriptor set layout, pool and set.
    DescriptorSetLayoutBuilder setLayoutbuilder;
    setLayoutbuilder.addSingleBinding(descriptorType, descriptorStages);
    const auto setLayout = setLayoutbuilder.build(ctx.vkd, ctx.device);

    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(descriptorType, kNumDraws);
    const auto descriptorPool =
        poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, kNumDraws);

    using DescriptorSetVec = std::vector<Move<VkDescriptorSet>>;
    DescriptorSetVec descriptorSets;

    for (uint32_t drawIdx = 0u; drawIdx < kNumDraws; ++drawIdx)
    {
        descriptorSets.emplace_back(makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *setLayout));

        DescriptorSetUpdateBuilder updateBuilder;
        const auto counterBufferDescriptorInfo =
            makeDescriptorBufferInfo(counterBuffers.at(drawIdx)->get(), 0ull, counterBufferSize);
        updateBuilder.writeSingle(*descriptorSets.back(), DescriptorSetUpdateBuilder::Location::binding(0u),
                                  descriptorType, &counterBufferDescriptorInfo);
        updateBuilder.update(ctx.vkd, ctx.device);
    }

    // Render pass and framebuffer.
    const std::vector<VkAttachmentDescription> attachmentDescs{
        // Multisample attachment.
        makeAttachmentDescription(0u, colorFormat, kMultiSampleCount, VK_ATTACHMENT_LOAD_OP_CLEAR,
                                  VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                  VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),

        // Resolve attachment.
        makeAttachmentDescription(0u, colorFormat, kSingleSampleCount, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                  VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                  VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
    };

    const auto colorAttRef   = makeAttachmentReference(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    const auto resolveAttRef = makeAttachmentReference(1u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    const auto subpassDescription =
        makeSubpassDescription(0u, bindPoint, 0u, nullptr, 1u, &colorAttRef, &resolveAttRef, nullptr, 0u, nullptr);

    const VkRenderPassCreateInfo renderPassCreateInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // VkStructureType sType;
        nullptr,                                   // const void* pNext;
        0u,                                        // VkRenderPassCreateFlags flags;
        de::sizeU32(attachmentDescs),              // uint32_t attachmentCount;
        de::dataOrNull(attachmentDescs),           // const VkAttachmentDescription* pAttachments;
        1u,                                        // uint32_t subpassCount;
        &subpassDescription,                       // const VkSubpassDescription* pSubpasses;
        0u,                                        // uint32_t dependencyCount;
        nullptr,                                   // const VkSubpassDependency* pDependencies;
    };
    auto renderPass = RenderPassWrapper(constructionType, ctx.vkd, ctx.device, &renderPassCreateInfo);

    const std::vector<VkImage> images{colorBuffer.getImage(), resolveBuffer.getImage()};
    const std::vector<VkImageView> imageViews{colorBuffer.getImageView(), resolveBuffer.getImageView()};
    renderPass.createFramebuffer(ctx.vkd, ctx.device, de::sizeU32(imageViews), de::dataOrNull(images),
                                 de::dataOrNull(imageViews), vkExtent.width, vkExtent.height);

    // Pipelines.
    const auto &binaries   = context.getBinaryCollection();
    const auto &vertModule = ShaderWrapper(ctx.vkd, ctx.device, binaries.get("vert"));
    const auto &fragModule = ShaderWrapper(ctx.vkd, ctx.device, binaries.get("frag"));

    const std::vector<VkDynamicState> dynamicStates{
#ifndef CTS_USES_VULKANSC
        VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT,
#endif // CTS_USES_VULKANSC
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_VIEWPORT,
    };

    const VkPipelineDynamicStateCreateInfo dynamicStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                              // const void* pNext;
        0u,                                                   // VkPipelineDynamicStateCreateFlags flags;
        de::sizeU32(dynamicStates),                           // uint32_t dynamicStateCount;
        de::dataOrNull(dynamicStates),                        // const VkDynamicState* pDynamicStates;
    };

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = initVulkanStructureConst();

    VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                  // const void* pNext;
        0u,                                                       // VkPipelineMultisampleStateCreateFlags flags;
        VK_SAMPLE_COUNT_64_BIT,                                   // VkSampleCountFlagBits rasterizationSamples;
        VK_FALSE,                                                 // VkBool32 sampleShadingEnable;
        1.0f,                                                     // float minSampleShading;
        nullptr,                                                  // const VkSampleMask* pSampleMask;
        VK_FALSE,                                                 // VkBool32 alphaToCoverageEnable;
        VK_FALSE,                                                 // VkBool32 alphaToOneEnable;
    };

    const std::vector<VkViewport> staticViewports(1u, makeViewport(0u, 0u));
    const std::vector<VkRect2D> staticScissors(1u, makeRect2D(0u, 0u));
    const PipelineLayoutWrapper pipelineLayout(constructionType, ctx.vkd, ctx.device, *setLayout);
    const auto renderArea     = makeRect2D(fbExtent);
    const int halfWidth       = fbExtent.x() / 2;
    const uint32_t halfWidthU = static_cast<uint32_t>(halfWidth);
    const float halfWidthF    = static_cast<float>(halfWidth);
    const float heightF       = static_cast<float>(vkExtent.height);
    const std::vector<VkRect2D> dynamicScissors{makeRect2D(0, 0, halfWidthU, vkExtent.height),
                                                makeRect2D(halfWidth, 0, halfWidthU, vkExtent.height)};
    const std::vector<VkViewport> dynamicViewports{
        makeViewport(0.0f, 0.0f, halfWidthF, heightF, 0.0f, 1.0f),
        makeViewport(halfWidthF, 0.0f, halfWidthF, heightF, 0.0f, 1.0f),
    };

    using WrapperPtr = std::unique_ptr<GraphicsPipelineWrapper>;
    using WrapperVec = std::vector<WrapperPtr>;

    WrapperVec wrappers;

    for (const auto sampleShadingEnable : {false, true})
    {
        multisampleStateCreateInfo.sampleShadingEnable = sampleShadingEnable;

        WrapperPtr pipelineWrapper(new GraphicsPipelineWrapper(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device,
                                                               context.getDeviceExtensions(), constructionType));
        pipelineWrapper->setDefaultTopology(topology)
            .setDefaultRasterizationState()
            .setDefaultColorBlendState()
            .setDynamicState(&dynamicStateInfo)
            .setupVertexInputState(&vertexInputStateCreateInfo)
            .setupPreRasterizationShaderState(staticViewports, staticScissors, pipelineLayout, *renderPass, 0u,
                                              vertModule)
            .setupFragmentShaderState(pipelineLayout, *renderPass, 0u, fragModule, nullptr, &multisampleStateCreateInfo)
            .setupFragmentOutputState(*renderPass, 0u, nullptr, &multisampleStateCreateInfo)
            .setMonolithicPipelineLayout(pipelineLayout)
            .buildPipeline();

        wrappers.emplace_back(std::move(pipelineWrapper));
    }

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = cmd.cmdBuffer.get();

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    renderPass.begin(ctx.vkd, cmdBuffer, renderArea, clearColor);
    for (uint32_t drawIdx = 0u; drawIdx < kNumDraws; ++drawIdx)
    {
        wrappers.at(drawIdx)->bind(cmdBuffer);
        if (drawIdx == 0u)
        {
#ifndef CTS_USES_VULKANSC
            ctx.vkd.cmdSetRasterizationSamplesEXT(cmdBuffer, kMultiSampleCount);
#else
            DE_ASSERT(false);
#endif // CTS_USES_VULKANSC
        }
#ifndef CTS_USES_VULKANSC
        if (isConstructionTypeShaderObject(constructionType))
        {
            ctx.vkd.cmdSetScissorWithCount(cmdBuffer, 1u, &dynamicScissors.at(drawIdx));
            ctx.vkd.cmdSetViewportWithCount(cmdBuffer, 1u, &dynamicViewports.at(drawIdx));
        }
        else
#endif // CTS_USES_VULKANSC
        {
            ctx.vkd.cmdSetScissor(cmdBuffer, 0u, 1u, &dynamicScissors.at(drawIdx));
            ctx.vkd.cmdSetViewport(cmdBuffer, 0u, 1u, &dynamicViewports.at(drawIdx));
        }
        ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSets.at(drawIdx).get(),
                                      0u, nullptr);
        ctx.vkd.cmdDraw(cmdBuffer, kVertexCount, 1u, 0u, 0u);
    }
    renderPass.end(ctx.vkd, cmdBuffer);
    copyImageToBuffer(ctx.vkd, cmdBuffer, resolveBuffer.getImage(), resolveBuffer.getBuffer(), fbExtent.swizzle(0, 1),
                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1u,
                      VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_COLOR_BIT,
                      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    // Verify resolve buffer and counter buffers.
    auto &log = context.getTestContext().getLog();
    {
        const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f); // Expect exact results.
        const auto tcuFormat           = mapVkFormat(colorFormat);
        const auto &resolveBufferAlloc = resolveBuffer.getBufferAllocation();
        const auto resolveBufferData   = resolveBufferAlloc.getHostPtr();

        invalidateAlloc(ctx.vkd, ctx.device, resolveBufferAlloc);
        const tcu::ConstPixelBufferAccess resultAccess(tcuFormat, fbExtent, resolveBufferData);

        if (!tcu::floatThresholdCompare(log, "Result", "", geomColor, resultAccess, threshold,
                                        tcu::COMPARE_LOG_ON_ERROR))
            return tcu::TestStatus::fail("Unexpected color buffer results -- check log for details");
    }
    {
        std::vector<uint32_t> counterResults(kNumDraws, 0u);
        for (uint32_t drawIdx = 0u; drawIdx < kNumDraws; ++drawIdx)
        {
            const auto &bufferAlloc = counterBuffers.at(drawIdx)->getAllocation();
            invalidateAlloc(ctx.vkd, ctx.device, bufferAlloc);
            deMemcpy(&counterResults.at(drawIdx), bufferAlloc.getHostPtr(), sizeof(counterResults.at(drawIdx)));
            log << tcu::TestLog::Message << "Draw " << drawIdx << ": " << counterResults.at(drawIdx) << " invocations"
                << tcu::TestLog::EndMessage;
        }

        // The first result is run without sample shading enabled, so it can have any value from 1 to 4 invocations per pixel.
        // The second result runs with sample shading enabled, so it must have exactly 4 invocations per pixel.
        const uint32_t minInvs = (vkExtent.width * vkExtent.height) / 2u;
        const uint32_t maxInvs = minInvs * static_cast<uint32_t>(kMultiSampleCount);

        verifyValueInRange(counterResults.at(0u), minInvs, maxInvs, "invocation counter without sample shading");
        verifyValueInRange(counterResults.at(1u), maxInvs, maxInvs, "invocation counter with sample shading");
    }

    return tcu::TestStatus::pass("Pass");
}

#ifndef CTS_USES_VULKANSC
// Test that sample shading is enabled even if the sample count is dynamic and
// the product of minSampleShading and static sample count is not greater than
// 1.
//
// The backstory is that some drivers are known to have or have had an
// optimization where they enable sample shading like:
//
//  if (sampleShadingEnable && minSampleShading * rasterizationSamples > 1.0)
//
// In cases where rasterizationSamples is dynamic and only known at runtime,
// they may not enable sample rate shading. The tests will use a combination of
// minSampleShading and static rasterization sample count such that they're not
// over 1.0, but the dynamic sample count will make the number go over 1.0,
// requiring a minimum known sample count, verified using an atomic counter.

namespace DSS // Dynamic Sample Shading
{

constexpr VkFormat kFormat           = VK_FORMAT_R8G8B8A8_UNORM;
constexpr VkImageType kImageType     = VK_IMAGE_TYPE_2D;
constexpr VkImageViewType kViewType  = VK_IMAGE_VIEW_TYPE_2D;
constexpr VkImageTiling kTiling      = VK_IMAGE_TILING_OPTIMAL;
constexpr VkImageUsageFlags kMSUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // Multisample. Single sample below.
constexpr VkImageUsageFlags kSSUsage =
    (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
constexpr VkImageUsageFlags kTexUsage = (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

struct Params
{
    PipelineConstructionType constructionType;
    VkSampleCountFlagBits staticCount;
    VkSampleCountFlagBits dynamicCount;
};

} // namespace DSS

void dynamicSampleShadingPrograms(SourceCollections &dst, DSS::Params)
{
    std::ostringstream vert;
    vert << "#version 460\n"
         << "layout (location=0) in vec4 inPos;\n"
         << "layout (location=1) in vec2 inCoords;\n"
         << "layout (location=0) out vec2 outCoords;\n"
         << "void main (void) {\n"
         << "    gl_Position = inPos;\n"
         << "    outCoords   = inCoords;\n"
         << "}\n";
    dst.glslSources.add("vert") << glu::VertexSource(vert.str());

    // We use a flat-colored texture to avoid direct flat colors in the shader, in case it affects results.
    std::ostringstream frag;
    frag << "#version 460\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "layout (location=0) in vec2 inCoords;\n"
         << "layout (set=0, binding=0) uniform sampler2D tex;\n"
         << "layout (set=0, binding=1, std430) buffer CounterBlock { uint counter; } atomicCounter;\n"
         << "void main (void) {\n"
         << "    outColor = texture(tex, inCoords);\n"
         << "    atomicAdd(atomicCounter.counter, 1u);\n"
         << "}\n";
    dst.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

void dynamicSampleShadingSupport(Context &context, DSS::Params params)
{
    const auto &eds3Features = context.getExtendedDynamicState3FeaturesEXT();
    if (!eds3Features.extendedDynamicState3RasterizationSamples)
        TCU_THROW(NotSupportedError, "extendedDynamicState3RasterizationSamples not supported");

    const auto ctx              = context.getContextCommonData();
    const auto formatProperties = getPhysicalDeviceImageFormatProperties(
        ctx.vki, ctx.physicalDevice, DSS::kFormat, DSS::kImageType, DSS::kTiling, DSS::kMSUsage, 0u);
    const auto expectedCounts = static_cast<VkSampleCountFlags>(params.staticCount | params.dynamicCount);

    if ((formatProperties.sampleCounts & expectedCounts) != expectedCounts)
        TCU_THROW(NotSupportedError, "Sample counts not supported");

    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_FRAGMENT_STORES_AND_ATOMICS);
}

tcu::TestStatus dynamicSampleShadingTest(Context &context, DSS::Params params)
{
    const auto &ctx = context.getContextCommonData();
    const tcu::IVec3 fbExtent(2, 2, 1);
    const auto vkExtent  = makeExtent3D(fbExtent);
    const auto texExtent = makeExtent3D(16u, 16u, 1u);
    const auto tcuFormat = mapVkFormat(DSS::kFormat);
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    const tcu::Vec4 geomColor(0.0f, 0.0f, 1.0f, 1.0f);
    const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f); // When using 0 and 1 only, we expect exact results.
    const auto bindPoint  = VK_PIPELINE_BIND_POINT_GRAPHICS;
    const auto dataStages = VK_SHADER_STAGE_FRAGMENT_BIT;
    const auto colorSRR   = makeDefaultImageSubresourceRange();

    // Multisample color buffer with verification buffer.
    const VkImageCreateInfo msColorBufferCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        0u,                                  // VkImageCreateFlags flags;
        DSS::kImageType,                     // VkImageType imageType;
        DSS::kFormat,                        // VkFormat format;
        vkExtent,                            // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        1u,                                  // uint32_t arrayLayers;
        params.dynamicCount,                 // VkSampleCountFlagBits samples;
        DSS::kTiling,                        // VkImageTiling tiling;
        DSS::kMSUsage,                       // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        1u,                                  // uint32_t queueFamilyIndexCount;
        nullptr,                             // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };
    const auto msColorBuffer =
        ImageWithMemory(ctx.vkd, ctx.device, ctx.allocator, msColorBufferCreateInfo, MemoryRequirement::Any);
    const auto msColorBufferView =
        makeImageView(ctx.vkd, ctx.device, *msColorBuffer, DSS::kViewType, DSS::kFormat, colorSRR);

    // Resolve attachment with verification buffer.
    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, msColorBufferCreateInfo.extent,
                                msColorBufferCreateInfo.format, DSS::kSSUsage, msColorBufferCreateInfo.imageType,
                                colorSRR, msColorBufferCreateInfo.arrayLayers, VK_SAMPLE_COUNT_1_BIT,
                                msColorBufferCreateInfo.tiling);

    // Vertices.
    struct VertexData
    {
        tcu::Vec4 position;
        tcu::Vec2 texCoords;
    };

    const std::vector<VertexData> vertices{
        // Vertex position.                    Texture coordinates.
        {tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f), tcu::Vec2(0.0f, 0.0f)},
        {tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f), tcu::Vec2(0.0f, 1.0f)},
        {tcu::Vec4(1.0f, -1.0f, 0.0f, 1.0f), tcu::Vec2(1.0f, 0.0f)},
        {tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f), tcu::Vec2(1.0f, 1.0f)},
    };

    // Vertex buffer
    const auto vbSize = static_cast<VkDeviceSize>(de::dataSize(vertices));
    const auto vbInfo = makeBufferCreateInfo(vbSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    BufferWithMemory vertexBuffer(ctx.vkd, ctx.device, ctx.allocator, vbInfo, MemoryRequirement::HostVisible);
    const auto vbAlloc  = vertexBuffer.getAllocation();
    void *vbData        = vbAlloc.getHostPtr();
    const auto vbOffset = static_cast<VkDeviceSize>(0);

    deMemcpy(vbData, de::dataOrNull(vertices), de::dataSize(vertices));

    // Sampled texture.
    const VkImageCreateInfo textureCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        0u,                                  // VkImageCreateFlags flags;
        DSS::kImageType,                     // VkImageType imageType;
        DSS::kFormat,                        // VkFormat format;
        texExtent,                           // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        1u,                                  // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
        DSS::kTiling,                        // VkImageTiling tiling;
        DSS::kTexUsage,                      // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        1u,                                  // uint32_t queueFamilyIndexCount;
        nullptr,                             // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };
    const auto texture = ImageWithMemory(ctx.vkd, ctx.device, ctx.allocator, textureCreateInfo, MemoryRequirement::Any);
    const auto textureView =
        makeImageView(ctx.vkd, ctx.device, *texture, DSS::kViewType, textureCreateInfo.format, colorSRR);

    // Sampler.
    const VkSamplerCreateInfo samplerCreateInfo = {
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,   // VkStructureType sType;
        nullptr,                                 // const void* pNext;
        0u,                                      // VkSamplerCreateFlags flags;
        VK_FILTER_NEAREST,                       // VkFilter magFilter;
        VK_FILTER_NEAREST,                       // VkFilter minFilter;
        VK_SAMPLER_MIPMAP_MODE_NEAREST,          // VkSamplerMipmapMode mipmapMode;
        VK_SAMPLER_ADDRESS_MODE_REPEAT,          // VkSamplerAddressMode addressModeU;
        VK_SAMPLER_ADDRESS_MODE_REPEAT,          // VkSamplerAddressMode addressModeV;
        VK_SAMPLER_ADDRESS_MODE_REPEAT,          // VkSamplerAddressMode addressModeW;
        0.0f,                                    // float mipLodBias;
        VK_FALSE,                                // VkBool32 anisotropyEnable;
        0.0f,                                    // float maxAnisotropy;
        VK_FALSE,                                // VkBool32 compareEnable;
        VK_COMPARE_OP_NEVER,                     // VkCompareOp compareOp;
        0.0f,                                    // float minLod;
        0.0f,                                    // float maxLod;
        VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, // VkBorderColor borderColor;
        VK_FALSE,                                // VkBool32 unnormalizedCoordinates;
    };
    const auto sampler = createSampler(ctx.vkd, ctx.device, &samplerCreateInfo);

    // Atomic counter buffer.
    const auto dbSize = static_cast<VkDeviceSize>(sizeof(uint32_t));
    const auto dbInfo = makeBufferCreateInfo(dbSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    BufferWithMemory dataBuffer(ctx.vkd, ctx.device, ctx.allocator, dbInfo, MemoryRequirement::HostVisible);
    const auto dbAlloc  = dataBuffer.getAllocation();
    void *dbData        = dbAlloc.getHostPtr();
    const auto dbOffset = static_cast<VkDeviceSize>(0);

    deMemset(dbData, 0, static_cast<size_t>(dbSize));

    // Descriptor pool, set, layout, etc.
    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    const auto descriptorPool =
        poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

    DescriptorSetLayoutBuilder layoutBuilder;
    layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, dataStages);
    layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, dataStages);
    const auto setLayout     = layoutBuilder.build(ctx.vkd, ctx.device);
    const auto descriptorSet = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *setLayout);

    DescriptorSetUpdateBuilder updateBuilder;
    const auto dbDescInfo  = makeDescriptorBufferInfo(dataBuffer.get(), dbOffset, dbSize);
    const auto texDescInfo = makeDescriptorImageInfo(*sampler, *textureView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                              VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &texDescInfo);
    updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                              VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &dbDescInfo);
    updateBuilder.update(ctx.vkd, ctx.device);

    const auto pipelineLayout = PipelineLayoutWrapper(params.constructionType, ctx.vkd, ctx.device, *setLayout);

    const std::vector<VkAttachmentDescription> attachmentDescriptions{
        makeAttachmentDescription( // Multisample attachment.
            0u, DSS::kFormat, params.dynamicCount, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
        makeAttachmentDescription( // Resolve attachment.
            0u, DSS::kFormat, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
    };

    const std::vector<VkAttachmentReference> attachmentReferences{
        makeAttachmentReference(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL), // Multisample.
        makeAttachmentReference(1u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL), // Resolve.
    };

    const auto subpass = makeSubpassDescription(0u, bindPoint, 0u, nullptr, 1u, &attachmentReferences.at(0u),
                                                &attachmentReferences.at(1u), nullptr, 0u, nullptr);

    const VkRenderPassCreateInfo renderPassCreateInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // VkStructureType sType;
        nullptr,                                   // const void* pNext;
        0u,                                        // VkRenderPassCreateFlags flags;
        de::sizeU32(attachmentDescriptions),       // uint32_t attachmentCount;
        de::dataOrNull(attachmentDescriptions),    // const VkAttachmentDescription* pAttachments;
        1u,                                        // uint32_t subpassCount;
        &subpass,                                  // const VkSubpassDescription* pSubpasses;
        0u,                                        // uint32_t dependencyCount;
        nullptr,                                   // const VkSubpassDependency* pDependencies;
    };
    auto renderPass = RenderPassWrapper(params.constructionType, ctx.vkd, ctx.device, &renderPassCreateInfo);

    const std::vector<VkImage> fbImages{*msColorBuffer, colorBuffer.getImage()};
    const std::vector<VkImageView> fbImageViews{*msColorBufferView, colorBuffer.getImageView()};
    DE_ASSERT(fbImages.size() == fbImageViews.size());
    renderPass.createFramebuffer(ctx.vkd, ctx.device, de::sizeU32(fbImageViews), de::dataOrNull(fbImages),
                                 de::dataOrNull(fbImageViews), vkExtent.width, vkExtent.height);

    // Modules.
    const auto &binaries  = context.getBinaryCollection();
    const auto vertModule = ShaderWrapper(ctx.vkd, ctx.device, binaries.get("vert"));
    const auto fragModule = ShaderWrapper(ctx.vkd, ctx.device, binaries.get("frag"));

    const std::vector<VkViewport> viewports(1u, makeViewport(vkExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(vkExtent));

    const std::vector<VkDynamicState> dynamicStates{VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT};

    VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = initVulkanStructure();
    dynamicStateCreateInfo.dynamicStateCount                = de::sizeU32(dynamicStates);
    dynamicStateCreateInfo.pDynamicStates                   = de::dataOrNull(dynamicStates);

    const std::vector<VkVertexInputBindingDescription> bindingDescriptions{
        makeVertexInputBindingDescription(0u, static_cast<uint32_t>(sizeof(VertexData)), VK_VERTEX_INPUT_RATE_VERTEX),
    };

    const std::vector<VkVertexInputAttributeDescription> attributeDescriptions{
        makeVertexInputAttributeDescription(0u, 0u, vk::VK_FORMAT_R32G32B32A32_SFLOAT,
                                            static_cast<uint32_t>(offsetof(VertexData, position))),
        makeVertexInputAttributeDescription(1u, 0u, vk::VK_FORMAT_R32G32_SFLOAT,
                                            static_cast<uint32_t>(offsetof(VertexData, texCoords))),
    };

    VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = initVulkanStructure();
    vertexInputStateCreateInfo.vertexBindingDescriptionCount        = de::sizeU32(bindingDescriptions);
    vertexInputStateCreateInfo.pVertexBindingDescriptions           = de::dataOrNull(bindingDescriptions);
    vertexInputStateCreateInfo.vertexAttributeDescriptionCount      = de::sizeU32(attributeDescriptions);
    vertexInputStateCreateInfo.pVertexAttributeDescriptions         = de::dataOrNull(attributeDescriptions);

    // This is critical for the test. We need to select a minSampleShading value
    // that results in <= 1.0 when multiplied with the static sample count, but
    // > 1.0 when using the dynamic sample count.
    const auto minSampleShading = 1.0f / static_cast<float>(params.staticCount);

    VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = initVulkanStructure();
    multisampleStateCreateInfo.rasterizationSamples                 = params.staticCount;
    multisampleStateCreateInfo.sampleShadingEnable                  = VK_TRUE;
    multisampleStateCreateInfo.minSampleShading                     = minSampleShading;

    GraphicsPipelineWrapper pipeline(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device, context.getDeviceExtensions(),
                                     params.constructionType);
    pipeline.setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
        .setDefaultRasterizationState()
        .setDefaultColorBlendState()
        .setDynamicState(&dynamicStateCreateInfo)
        .setupVertexInputState(&vertexInputStateCreateInfo)
        .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, *renderPass, 0u, vertModule)
        .setupFragmentShaderState(pipelineLayout, *renderPass, 0u, fragModule, nullptr, &multisampleStateCreateInfo)
        .setupFragmentOutputState(*renderPass, 0u, nullptr, &multisampleStateCreateInfo)
        .setMonolithicPipelineLayout(pipelineLayout)
        .buildPipeline();

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);

    // Clear texture to the geometry color.
    {
        const auto preClearBarrier = makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, *texture, colorSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, &preClearBarrier);
        const auto texClearColor = makeClearValueColor(geomColor);
        ctx.vkd.cmdClearColorImage(cmdBuffer, *texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &texClearColor.color, 1u,
                                   &colorSRR);
        const auto postClearBarrier = makeImageMemoryBarrier(
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, *texture, colorSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, &postClearBarrier);
    }

    const auto clearValue = makeClearValueColor(clearColor);
    renderPass.begin(ctx.vkd, cmdBuffer, scissors.at(0u), clearValue);
    ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vbOffset);
    ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
    pipeline.bind(cmdBuffer);
    ctx.vkd.cmdSetRasterizationSamplesEXT(cmdBuffer, params.dynamicCount);
    ctx.vkd.cmdDraw(cmdBuffer, de::sizeU32(vertices), 1u, 0u, 0u);
    renderPass.end(ctx.vkd, cmdBuffer);
    copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), fbExtent.swizzle(0, 1),
                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1u,
                      VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_COLOR_BIT,
                      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    {
        const auto atomicDataBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &atomicDataBarrier);
    }
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    // Verify color output.
    invalidateAlloc(ctx.vkd, ctx.device, colorBuffer.getBufferAllocation());
    tcu::PixelBufferAccess resultAccess(tcuFormat, fbExtent, colorBuffer.getBufferAllocation().getHostPtr());

    tcu::TextureLevel referenceLevel(tcuFormat, fbExtent.x(), fbExtent.y());
    auto referenceAccess = referenceLevel.getAccess();
    tcu::clear(referenceAccess, geomColor);

    auto &log = context.getTestContext().getLog();
    if (!tcu::floatThresholdCompare(log, "Result", "", referenceAccess, resultAccess, threshold,
                                    tcu::COMPARE_LOG_ON_ERROR))
        return tcu::TestStatus::fail("Unexpected color in result buffer; check log for details");

    // Verify actual sample count.
    const uint32_t minCountPerPixel = static_cast<uint32_t>(minSampleShading * static_cast<float>(params.dynamicCount));
    const uint32_t pixelCount       = vkExtent.width * vkExtent.height;
    const uint32_t minExpectedCount = pixelCount * minCountPerPixel;
    uint32_t resultCount            = 0u;

    invalidateAlloc(ctx.vkd, ctx.device, dbAlloc);
    deMemcpy(&resultCount, dbData, sizeof(resultCount));

    if (resultCount < minExpectedCount)
    {
        std::ostringstream msg;
        msg << "Unexpected fragment shader count: expected at least " << minExpectedCount << " but found "
            << resultCount;
        TCU_FAIL(msg.str());
    }

    return tcu::TestStatus::pass("Pass");
}
#endif // CTS_USES_VULKANSC

using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

} // anonymous namespace

tcu::TestCaseGroup *createExtendedDynamicStateMiscTests(tcu::TestContext &testCtx,
                                                        vk::PipelineConstructionType pipelineConstructionType)
{
    GroupPtr miscGroup(new tcu::TestCaseGroup(testCtx, "misc"));

    addFunctionCaseWithPrograms(miscGroup.get(), "sample_shading_dynamic_sample_count",
                                sampleShadingWithDynamicSampleCountSupport, sampleShadingWithDynamicSampleCountPrograms,
                                sampleShadingWithDynamicSampleCount, pipelineConstructionType);

#ifndef CTS_USES_VULKANSC
    if (!isConstructionTypeShaderObject(pipelineConstructionType))
    {
        const std::vector<VkSampleCountFlagBits> sampleCounts{
            VK_SAMPLE_COUNT_1_BIT, VK_SAMPLE_COUNT_2_BIT,  VK_SAMPLE_COUNT_4_BIT,
            VK_SAMPLE_COUNT_8_BIT, VK_SAMPLE_COUNT_16_BIT,
        };

        for (size_t i = 0; i < sampleCounts.size(); ++i)
            for (size_t j = i + 1; j < sampleCounts.size(); ++j)
            {
                const auto staticCount = sampleCounts.at(i);
                const auto dynamicCount =
                    sampleCounts.at(j); // The actual dynamic count is always greater than the static value.

                const DSS::Params params{
                    pipelineConstructionType,
                    staticCount,
                    dynamicCount,
                };
                const auto testName = "dynamic_sample_shading_static_" + std::to_string(staticCount) + "_dynamic_" +
                                      std::to_string(dynamicCount);
                addFunctionCaseWithPrograms(miscGroup.get(), testName, dynamicSampleShadingSupport,
                                            dynamicSampleShadingPrograms, dynamicSampleShadingTest, params);
            }
    }
#endif // CTS_USES_VULKANSC

    return miscGroup.release();
}

} // namespace pipeline
} // namespace vkt
