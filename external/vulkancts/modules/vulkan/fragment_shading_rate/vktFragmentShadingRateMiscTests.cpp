/*-------------------------------------------------------------------------
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
 * \brief Fragment Shading Rate miscellaneous tests
 *//*--------------------------------------------------------------------*/

#include "vktFragmentShadingRateMiscTests.hpp"
#include "deMemory.h"
#include "tcuVectorType.hpp"
#include "vkQueryUtil.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkSafetyCriticalUtil.hpp"

#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "tcuCommandLine.hpp"

#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"

#include "vkBuilderUtil.hpp"

#include "deUniquePtr.hpp"

#include <sstream>
#include <vector>
#include <cstddef>
#include <cmath>
#include <cstdint>

namespace vkt
{
namespace FragmentShadingRate
{

namespace
{

using namespace vk;

struct PositionColor
{
    PositionColor(const tcu::Vec4 &position_, const tcu::Vec4 &color_) : position(position_), color(color_)
    {
    }

    tcu::Vec4 position;
    tcu::Vec4 color;
};

struct TestParams
{
    bool useRobustness2;
    bool useBaseMipLevel1;
};

VkExtent3D getDefaultExtent(void)
{
    return makeExtent3D(8u, 8u, 1u);
}

void checkShadingRateSupport(Context &context, bool pipeline = false, bool primitive = false, bool attachment = false)
{
    context.requireDeviceFunctionality("VK_KHR_fragment_shading_rate");
    const auto &fsrFeatures = context.getFragmentShadingRateFeatures();

    if (pipeline && !fsrFeatures.pipelineFragmentShadingRate)
        TCU_THROW(NotSupportedError, "pipelineFragmentShadingRate not supported");

    if (primitive && !fsrFeatures.primitiveFragmentShadingRate)
        TCU_THROW(NotSupportedError, "primitiveFragmentShadingRate not supported");

    if (attachment && !fsrFeatures.attachmentFragmentShadingRate)
        TCU_THROW(NotSupportedError, "attachmentFragmentShadingRate not supported");
}

void checkEnableDisableSupport(Context &context)
{
    checkShadingRateSupport(context, true, false, true);
}

void checkNoFragSupport(Context &context)
{
    checkShadingRateSupport(context, true);
}

void initDefaultVertShader(vk::SourceCollections &programCollection, const std::string &shaderName)
{
    // Default vertex shader, including vertex color.
    std::ostringstream vert;
    vert << "#version 460\n"
         << "#extension GL_EXT_fragment_shading_rate : enable\n"
         << "layout (location=0) in vec4 inPos;\n"
         << "layout (location=1) in vec4 inColor;\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "void main (void) {\n"
         << "    gl_Position = inPos;\n"
         << "    outColor = inColor;\n"
         << "}\n";
    DE_ASSERT(!shaderName.empty());
    programCollection.glslSources.add(shaderName) << glu::VertexSource(vert.str());
}

void initDefaultFragShader(vk::SourceCollections &programCollection, const std::string &shaderName)
{
    // Default fragment shader, with vertex color.
    std::ostringstream frag;
    frag << "#version 460\n"
         << "layout (location=0) in vec4 inColor;\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "void main (void) {\n"
         << "    outColor = inColor;\n"
         << "}\n";
    DE_ASSERT(!shaderName.empty());
    programCollection.glslSources.add(shaderName) << glu::FragmentSource(frag.str());
}

void initEnableDisableShaders(vk::SourceCollections &programCollection)
{
    initDefaultVertShader(programCollection, "vert");
    initDefaultFragShader(programCollection, "frag");
}

void initNoFragShaders(vk::SourceCollections &programCollection)
{
    initDefaultVertShader(programCollection, "vert");
}

const VkPipelineVertexInputStateCreateInfo *getDefaultVertexInputStateCreateInfo(void)
{
    static VkVertexInputBindingDescription vertexBinding = {
        0u,                                           // uint32_t binding;
        static_cast<uint32_t>(sizeof(PositionColor)), // uint32_t stride;
        VK_VERTEX_INPUT_RATE_VERTEX,                  // VkVertexInputRate inputRate;
    };

    static VkVertexInputAttributeDescription inputAttributes[] = {
        {
            // position
            0u,                                                       // uint32_t location;
            0u,                                                       // uint32_t binding;
            VK_FORMAT_R32G32B32A32_SFLOAT,                            // VkFormat format;
            static_cast<uint32_t>(offsetof(PositionColor, position)), // uint32_t offset;
        },
        {
            // color
            1u,                                                    // uint32_t location;
            0u,                                                    // uint32_t binding;
            VK_FORMAT_R32G32B32A32_SFLOAT,                         // VkFormat format;
            static_cast<uint32_t>(offsetof(PositionColor, color)), // uint32_t offset;
        },
    };

    static const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                   // const void* pNext;
        0u,                                                        // VkPipelineVertexInputStateCreateFlags flags;
        1u,                                                        // uint32_t vertexBindingDescriptionCount;
        &vertexBinding, // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
        static_cast<uint32_t>(de::arrayLength(inputAttributes)), // uint32_t vertexAttributeDescriptionCount;
        inputAttributes, // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
    };

    return &vertexInputStateCreateInfo;
}

VkPipelineFragmentShadingRateStateCreateInfoKHR makeFragmentShadingRateStateCreateInfo(
    uint32_t width, uint32_t height, VkFragmentShadingRateCombinerOpKHR combiner0,
    VkFragmentShadingRateCombinerOpKHR combiner1)
{
    const VkPipelineFragmentShadingRateStateCreateInfoKHR fragmentShadingRateStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_FRAGMENT_SHADING_RATE_STATE_CREATE_INFO_KHR, // VkStructureType sType;
        nullptr,                                                                // const void* pNext;
        makeExtent2D(width, height),                                            // VkExtent2D fragmentSize;
        {
            // VkFragmentShadingRateCombinerOpKHR combinerOps[2];
            combiner0,
            combiner1,
        },
    };

    return fragmentShadingRateStateCreateInfo;
}

// Test idea: draw with VRS enabled by a fragment shading rate attachment, then bind a pipeline with VRS disabled and draw again.
// This was being incorrectly handled in RADV. Ref: https://gitlab.freedesktop.org/mesa/mesa/-/issues/9005
tcu::TestStatus testEnableDisable(Context &context)
{
    const auto ctx            = context.getContextCommonData();
    const auto &fsrProperties = context.getFragmentShadingRateProperties();
    const auto &minSize       = fsrProperties.minFragmentShadingRateAttachmentTexelSize;
    const auto &maxSize       = fsrProperties.maxFragmentShadingRateAttachmentTexelSize;
    const auto colorFormat    = VK_FORMAT_R8G8B8A8_UNORM;
    const auto colorUsage     = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const auto colorSRR       = makeDefaultImageSubresourceRange();
    const auto colorSRL       = makeDefaultImageSubresourceLayers();
    const auto bindPoint      = VK_PIPELINE_BIND_POINT_GRAPHICS;
    const auto fsrFormat      = VK_FORMAT_R8_UINT;
    const auto fsrExtent      = makeExtent3D(1u, 1u, 1u); // 1 pixel for the whole image.
    const auto fsrUsage = (VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    const auto sampleCount = VK_SAMPLE_COUNT_1_BIT;

    // Adjust image extent to an acceptable range so it's covered by a single FSR attachment pixel.
    auto vkExtent = getDefaultExtent();
    {
        vkExtent.width  = de::clamp(vkExtent.width, minSize.width, maxSize.width);
        vkExtent.height = de::clamp(vkExtent.height, minSize.height, maxSize.height);
    }
    const tcu::IVec3 fbExtent(static_cast<int>(vkExtent.width), static_cast<int>(vkExtent.height),
                              static_cast<int>(vkExtent.depth));

    vk::ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, vkExtent, colorFormat, colorUsage,
                                    VK_IMAGE_TYPE_2D);

    // Fragment shading rate attachment.
    const VkImageCreateInfo fsrAttachmentCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        0u,                                  // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        fsrFormat,                           // VkFormat format;
        fsrExtent,                           // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        1u,                                  // uint32_t arrayLayers;
        sampleCount,                         // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        fsrUsage,                            // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        nullptr,                             // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };
    ImageWithMemory fsrAttachment(ctx.vkd, ctx.device, ctx.allocator, fsrAttachmentCreateInfo, MemoryRequirement::Any);
    const auto fsrAttView =
        makeImageView(ctx.vkd, ctx.device, fsrAttachment.get(), VK_IMAGE_VIEW_TYPE_2D, fsrFormat, colorSRR);

    const auto &binaries  = context.getBinaryCollection();
    const auto vertModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("vert"));
    const auto fragModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));

    const std::vector<VkAttachmentDescription2> attachmentDescriptions{
        // Color attachment.
        {
            VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2, // VkStructureType sType;
            nullptr,                                    // const void* pNext;
            0u,                                         // VkAttachmentDescriptionFlags flags;
            colorFormat,                                // VkFormat format;
            sampleCount,                                // VkSampleCountFlagBits samples;
            VK_ATTACHMENT_LOAD_OP_CLEAR,                // VkAttachmentLoadOp loadOp;
            VK_ATTACHMENT_STORE_OP_STORE,               // VkAttachmentStoreOp storeOp;
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,            // VkAttachmentLoadOp stencilLoadOp;
            VK_ATTACHMENT_STORE_OP_DONT_CARE,           // VkAttachmentStoreOp stencilStoreOp;
            VK_IMAGE_LAYOUT_UNDEFINED,                  // VkImageLayout initialLayout;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,   // VkImageLayout finalLayout;
        },
        // FSR attachment.
        {
            VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,                   // VkStructureType sType;
            nullptr,                                                      // const void* pNext;
            0u,                                                           // VkAttachmentDescriptionFlags flags;
            fsrFormat,                                                    // VkFormat format;
            sampleCount,                                                  // VkSampleCountFlagBits samples;
            VK_ATTACHMENT_LOAD_OP_LOAD,                                   // VkAttachmentLoadOp loadOp;
            VK_ATTACHMENT_STORE_OP_STORE,                                 // VkAttachmentStoreOp storeOp;
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,                              // VkAttachmentLoadOp stencilLoadOp;
            VK_ATTACHMENT_STORE_OP_DONT_CARE,                             // VkAttachmentStoreOp stencilStoreOp;
            VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR, // VkImageLayout initialLayout;
            VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR, // VkImageLayout finalLayout;
        },
    };

    const VkAttachmentReference2 colorAttRef = {
        VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2, // VkStructureType sType;
        nullptr,                                  // const void* pNext;
        0u,                                       // uint32_t attachment;
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout layout;
        VK_IMAGE_ASPECT_COLOR_BIT,                // VkImageAspectFlags aspectMask;
    };

    const VkAttachmentReference2 fsrAttRef = {
        VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,                     // VkStructureType sType;
        nullptr,                                                      // const void* pNext;
        1u,                                                           // uint32_t attachment;
        VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR, // VkImageLayout layout;
        VK_IMAGE_ASPECT_COLOR_BIT,                                    // VkImageAspectFlags aspectMask;
    };

    const VkFragmentShadingRateAttachmentInfoKHR fsrAttInfo = {
        VK_STRUCTURE_TYPE_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR, // VkStructureType sType;
        nullptr,                                                     // const void* pNext;
        &fsrAttRef,                                    // const VkAttachmentReference2* pFragmentShadingRateAttachment;
        makeExtent2D(vkExtent.width, vkExtent.height), // VkExtent2D shadingRateAttachmentTexelSize;
    };

    const VkSubpassDescription2 subpassDescription{
        VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2, // VkStructureType sType;
        &fsrAttInfo,                             // const void* pNext;
        0u,                                      // VkSubpassDescriptionFlags flags;
        bindPoint,                               // VkPipelineBindPoint pipelineBindPoint;
        0u,                                      // uint32_t viewMask;
        0u,                                      // uint32_t inputAttachmentCount;
        nullptr,                                 // const VkAttachmentReference2* pInputAttachments;
        1u,                                      // uint32_t colorAttachmentCount;
        &colorAttRef,                            // const VkAttachmentReference2* pColorAttachments;
        nullptr,                                 // const VkAttachmentReference2* pResolveAttachments;
        nullptr,                                 // const VkAttachmentReference2* pDepthStencilAttachment;
        0u,                                      // uint32_t preserveAttachmentCount;
        nullptr,                                 // const uint32_t* pPreserveAttachments;
    };

    const VkRenderPassCreateInfo2 renderPassCreateInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2, // VkStructureType sType;
        nullptr,                                     // const void* pNext;
        0u,                                          // VkRenderPassCreateFlags flags;
        de::sizeU32(attachmentDescriptions),         // uint32_t attachmentCount;
        de::dataOrNull(attachmentDescriptions),      // const VkAttachmentDescription2* pAttachments;
        1u,                                          // uint32_t subpassCount;
        &subpassDescription,                         // const VkSubpassDescription2* pSubpasses;
        0u,                                          // uint32_t dependencyCount;
        nullptr,                                     // const VkSubpassDependency2* pDependencies;
        0u,                                          // uint32_t correlatedViewMaskCount;
        nullptr,                                     // const uint32_t* pCorrelatedViewMasks;
    };

    const auto renderPass = createRenderPass2(ctx.vkd, ctx.device, &renderPassCreateInfo);

    const std::vector<VkImageView> attachmentViews{colorBuffer.getImageView(), fsrAttView.get()};
    const auto framebuffer = makeFramebuffer(ctx.vkd, ctx.device, renderPass.get(), de::sizeU32(attachmentViews),
                                             de::dataOrNull(attachmentViews), vkExtent.width, vkExtent.height);

    const std::vector<VkViewport> viewports(1u, makeViewport(fbExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(fbExtent));

    // Use the rate according to the attachment.
    const auto fragmentShadingRateStateCreateInfo = makeFragmentShadingRateStateCreateInfo(
        1u, 1u, VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR, VK_FRAGMENT_SHADING_RATE_COMBINER_OP_REPLACE_KHR);

    const std::vector<tcu::Vec4> vertices{
        tcu::Vec4(-1.0, -1.0f, 0.0f, 1.0f),
        tcu::Vec4(-1.0, 1.0f, 0.0f, 1.0f),
        tcu::Vec4(1.0, -1.0f, 0.0f, 1.0f),
        tcu::Vec4(1.0, 1.0f, 0.0f, 1.0f),
    };

    const std::vector<tcu::Vec4> colors{
        tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f),
        tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f),
        tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f),
        tcu::Vec4(1.0f, 0.0f, 1.0f, 1.0f),
    };

    DE_ASSERT(vertices.size() == colors.size());

    // We mix them reversing the color order for the first draw.
    std::vector<PositionColor> vrsVertices;
    std::vector<PositionColor> noVrsVertices;

    vrsVertices.reserve(vertices.size());
    noVrsVertices.reserve(vertices.size());

    for (size_t i = 0; i < vertices.size(); ++i)
    {
        vrsVertices.push_back(PositionColor(vertices.at(i), colors.at(colors.size() - 1 - i)));
        noVrsVertices.push_back(PositionColor(vertices.at(i), colors.at(i)));
    }

    const auto vertexBufferSize       = static_cast<VkDeviceSize>(de::dataSize(vrsVertices));
    const auto vertexBufferUsage      = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    const auto vertexBufferCreateInfo = makeBufferCreateInfo(vertexBufferSize, vertexBufferUsage);
    const auto vertexBufferOffset     = static_cast<VkDeviceSize>(0);

    BufferWithMemory vrsVerticesBuffer(ctx.vkd, ctx.device, ctx.allocator, vertexBufferCreateInfo,
                                       MemoryRequirement::HostVisible);
    BufferWithMemory noVrsVerticesBuffer(ctx.vkd, ctx.device, ctx.allocator, vertexBufferCreateInfo,
                                         MemoryRequirement::HostVisible);
    auto &vrsVertAlloc   = vrsVerticesBuffer.getAllocation();
    auto &noVrsVertAlloc = noVrsVerticesBuffer.getAllocation();

    deMemcpy(vrsVertAlloc.getHostPtr(), de::dataOrNull(vrsVertices), de::dataSize(vrsVertices));
    deMemcpy(noVrsVertAlloc.getHostPtr(), de::dataOrNull(noVrsVertices), de::dataSize(noVrsVertices));
    flushAlloc(ctx.vkd, ctx.device, vrsVertAlloc);
    flushAlloc(ctx.vkd, ctx.device, noVrsVertAlloc);

    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device);

    // Pipeline with and without VRS.
    const auto pipelineVRS =
        makeGraphicsPipeline(ctx.vkd, ctx.device, pipelineLayout.get(), vertModule.get(), VK_NULL_HANDLE,
                             VK_NULL_HANDLE, VK_NULL_HANDLE, fragModule.get(), renderPass.get(), viewports, scissors,
                             VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0u, 0u, getDefaultVertexInputStateCreateInfo(),
                             nullptr, nullptr, nullptr, nullptr, nullptr, &fragmentShadingRateStateCreateInfo);

    const auto pipelineNoVRS =
        makeGraphicsPipeline(ctx.vkd, ctx.device, pipelineLayout.get(), vertModule.get(), VK_NULL_HANDLE,
                             VK_NULL_HANDLE, VK_NULL_HANDLE, fragModule.get(), renderPass.get(), viewports, scissors,
                             VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0u, 0u, getDefaultVertexInputStateCreateInfo());

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = cmd.cmdBuffer.get();

#if 0
      const int gl_ShadingRateFlag2VerticalPixelsEXT = 1;
      const int gl_ShadingRateFlag4VerticalPixelsEXT = 2;
      const int gl_ShadingRateFlag2HorizontalPixelsEXT = 4;
      const int gl_ShadingRateFlag4HorizontalPixelsEXT = 8;
#endif
    using ClearValueVec = std::vector<VkClearValue>;
    const uint32_t clearAttRate =
        5u; // 2x2: (gl_ShadingRateFlag2HorizontalPixelsEXT | gl_ShadingRateFlag2VerticalPixelsEXT)
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 0.0f);
    const ClearValueVec clearValues{
        makeClearValueColor(clearColor),
    };
    const auto colorCompThreshold = 0.005f; // between 1/255 and 2/255.
    const tcu::Vec4 colorThreshold(colorCompThreshold, colorCompThreshold, colorCompThreshold, colorCompThreshold);
    const auto vertexCount = de::sizeU32(vertices);

    tcu::TextureFormat fsrTextureFormat = mapVkFormat(fsrFormat);
    size_t fsrFillBufferSize = fsrExtent.width * fsrExtent.height * getNumUsedChannels(fsrTextureFormat.order) *
                               getChannelSize(fsrTextureFormat.type);
    BufferWithMemory fsrFillBuffer(ctx.vkd, ctx.device, ctx.allocator,
                                   makeBufferCreateInfo(fsrFillBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
                                   MemoryRequirement::HostVisible);
    auto fillPtr = (uint8_t *)fsrFillBuffer.getAllocation().getHostPtr();
    memset(fillPtr, clearAttRate, (size_t)fsrFillBufferSize);
    flushAlloc(ctx.vkd, ctx.device, fsrFillBuffer.getAllocation());

    const struct
    {
        VkBuffer vertexBuffer;
        VkPipeline pipeline;
    } iterations[] = {
        {vrsVerticesBuffer.get(), pipelineVRS.get()},
        {noVrsVerticesBuffer.get(), pipelineNoVRS.get()},
    };

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    {
        // Initialize the FSR attachment.
        const auto preTransferBarrier =
            makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                   fsrAttachment.get(), colorSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, &preTransferBarrier);
        const auto copyRegion = makeBufferImageCopy(fsrExtent, colorSRL);
        ctx.vkd.cmdCopyBufferToImage(cmdBuffer, fsrFillBuffer.get(), fsrAttachment.get(), VK_IMAGE_LAYOUT_GENERAL, 1u,
                                     &copyRegion);
        const auto postTransferBarrier = makeImageMemoryBarrier(
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR,
            VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR, fsrAttachment.get(),
            colorSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR, &postTransferBarrier);
    }
    {
        // Render pass.
        beginRenderPass(ctx.vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0u),
                        de::sizeU32(clearValues), de::dataOrNull(clearValues));
        for (const auto &iteration : iterations)
        {
            ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &iteration.vertexBuffer, &vertexBufferOffset);
            ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, iteration.pipeline);
            ctx.vkd.cmdDraw(cmdBuffer, vertexCount, 1u, 0u, 0u);
        }
        endRenderPass(ctx.vkd, cmdBuffer);
    }
    {
        // Copy image to verification buffer after rendering.
        const auto preTransferBarrier = makeImageMemoryBarrier(
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, colorBuffer.getImage(), colorSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, &preTransferBarrier);
        const auto copyRegion = makeBufferImageCopy(vkExtent, colorSRL);
        ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, colorBuffer.getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                     colorBuffer.getBuffer(), 1u, &copyRegion);
        const auto preHostBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &preHostBarrier);
    }
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);
    invalidateAlloc(ctx.vkd, ctx.device, colorBuffer.getBufferAllocation());

    // Create expected reference image.
    const auto tcuFormat = mapVkFormat(colorFormat);
    tcu::TextureLevel referenceLevel(tcuFormat, fbExtent.x(), fbExtent.y());
    tcu::PixelBufferAccess referenceAccess = referenceLevel.getAccess();

    const auto &xSize = fbExtent.x();
    const auto &ySize = fbExtent.y();
    const auto xSizeF = static_cast<float>(xSize);
    const auto ySizeF = static_cast<float>(ySize);

    // This must match the vertex+color combination for the second draw.
    // Red goes from 0 to 1 on the X axis, Blue goes from 0 to 1 on the Y axis.
    for (int y = 0; y < fbExtent.y(); ++y)
        for (int x = 0; x < fbExtent.x(); ++x)
        {
            const float red  = (static_cast<float>(y) + 0.5f) / ySizeF;
            const float blue = (static_cast<float>(x) + 0.5f) / xSizeF;
            const tcu::Vec4 refColor(red, 0.0f, blue, 1.0f);

            referenceAccess.setPixel(refColor, x, y);
        }

    auto &log = context.getTestContext().getLog();
    const tcu::ConstPixelBufferAccess resultAccess(tcuFormat, fbExtent, colorBuffer.getBufferAllocation().getHostPtr());

    if (!tcu::floatThresholdCompare(log, "Result", "", referenceAccess, resultAccess, colorThreshold,
                                    tcu::COMPARE_LOG_ON_ERROR))
        return tcu::TestStatus::fail("Unexpected color buffer contents -- check log for details");
    return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus testNoFrag(Context &context)
{
    const auto ctx         = context.getContextCommonData();
    const auto colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const auto colorUsage  = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const auto colorSRR    = makeDefaultImageSubresourceRange();
    const auto colorSRL    = makeDefaultImageSubresourceLayers();
    const auto depthFormat = VK_FORMAT_D16_UNORM;
    const auto depthUsage  = (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const auto depthSRR    = makeImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u);
    const auto depthSRL    = makeImageSubresourceLayers(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, 1u);
    const auto bindPoint   = VK_PIPELINE_BIND_POINT_GRAPHICS;
    const auto vkExtent    = makeExtent3D(8u, 1u, 1u);
    const tcu::IVec3 fbExtent(static_cast<int>(vkExtent.width), static_cast<int>(vkExtent.height),
                              static_cast<int>(vkExtent.depth));
    const auto imageType = VK_IMAGE_TYPE_2D;
    const tcu::IVec2 tileSize(2, 2);

    vk::ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, vkExtent, colorFormat, colorUsage, imageType,
                                    colorSRR);
    vk::ImageWithBuffer depthBuffer(ctx.vkd, ctx.device, ctx.allocator, vkExtent, depthFormat, depthUsage, imageType,
                                    depthSRR);

    const auto vertModule = createShaderModule(ctx.vkd, ctx.device, context.getBinaryCollection().get("vert"));
    const auto renderPass = makeRenderPass(ctx.vkd, ctx.device, colorFormat, depthFormat);

    const std::vector<VkImageView> attachmentViews{colorBuffer.getImageView(), depthBuffer.getImageView()};
    const auto framebuffer = makeFramebuffer(ctx.vkd, ctx.device, renderPass.get(), de::sizeU32(attachmentViews),
                                             de::dataOrNull(attachmentViews), vkExtent.width, vkExtent.height);

    const std::vector<VkViewport> viewports(1u, makeViewport(fbExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(fbExtent));

    // Use the rate from the pipeline.
    const auto fragmentShadingRateStateCreateInfo = makeFragmentShadingRateStateCreateInfo(
        static_cast<uint32_t>(tileSize.x()), static_cast<uint32_t>(tileSize.y()), // This has mandatory support.
        VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR, VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR);

    const std::vector<PositionColor> vertices{
        // Colors (second column) are irrelevant due to the lack of a frag shader.
        // In the first column we increase depth as we advance from left to right.
        {tcu::Vec4(-1.0, -1.0f, 0.0f, 1.0f), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f)},
        {tcu::Vec4(-1.0, 1.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f)},
        {tcu::Vec4(1.0, -1.0f, 1.0f, 1.0f), tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f)},
        {tcu::Vec4(1.0, 1.0f, 1.0f, 1.0f), tcu::Vec4(1.0f, 0.0f, 1.0f, 1.0f)},
    };

    const auto vertexBufferSize       = static_cast<VkDeviceSize>(de::dataSize(vertices));
    const auto vertexBufferUsage      = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    const auto vertexBufferCreateInfo = makeBufferCreateInfo(vertexBufferSize, vertexBufferUsage);
    const auto vertexBufferOffset     = static_cast<VkDeviceSize>(0);
    BufferWithMemory vertexBuffer(ctx.vkd, ctx.device, ctx.allocator, vertexBufferCreateInfo,
                                  MemoryRequirement::HostVisible);
    auto &vertexBufferAlloc = vertexBuffer.getAllocation();

    deMemcpy(vertexBufferAlloc.getHostPtr(), de::dataOrNull(vertices), de::dataSize(vertices));
    flushAlloc(ctx.vkd, ctx.device, vertexBufferAlloc);

    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device);

    const VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                    // const void* pNext;
        0u,                                                         // VkPipelineDepthStencilStateCreateFlags flags;
        VK_TRUE,                                                    // VkBool32 depthTestEnable;
        VK_TRUE,                                                    // VkBool32 depthWriteEnable;
        VK_COMPARE_OP_ALWAYS,                                       // VkCompareOp depthCompareOp;
        VK_FALSE,                                                   // VkBool32 depthBoundsTestEnable;
        VK_FALSE,                                                   // VkBool32 stencilTestEnable;
        {},                                                         // VkStencilOpState front;
        {},                                                         // VkStencilOpState back;
        0.0f,                                                       // float minDepthBounds;
        1.0f,                                                       // float maxDepthBounds;
    };

    // We need to force-enable rasterization at this step, otherwise the helper will disable it due to missing frag shader.
    const VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                    // const void* pNext;
        0u,                                                         // VkPipelineRasterizationStateCreateFlags flags;
        VK_FALSE,                                                   // VkBool32 depthClampEnable;
        VK_FALSE,                                                   // VkBool32 rasterizerDiscardEnable;
        VK_POLYGON_MODE_FILL,                                       // VkPolygonMode polygonMode;
        VK_CULL_MODE_NONE,                                          // VkCullModeFlags cullMode;
        VK_FRONT_FACE_COUNTER_CLOCKWISE,                            // VkFrontFace frontFace;
        VK_FALSE,                                                   // VkBool32 depthBiasEnable;
        0.0f,                                                       // float depthBiasConstantFactor;
        0.0f,                                                       // float depthBiasClamp;
        0.0f,                                                       // float depthBiasSlopeFactor;
        1.0f,                                                       // float lineWidth;
    };

    // Pipeline.
    const auto pipeline = makeGraphicsPipeline(
        ctx.vkd, ctx.device, pipelineLayout.get(), vertModule.get(), VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
        VK_NULL_HANDLE, renderPass.get(), viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0u, 0u,
        getDefaultVertexInputStateCreateInfo(), &rasterizationStateCreateInfo, nullptr, &depthStencilStateCreateInfo,
        nullptr, nullptr, &fragmentShadingRateStateCreateInfo);

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = cmd.cmdBuffer.get();

    using ClearValueVec = std::vector<VkClearValue>;
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 0.0f);
    const float clearDepth = 1.0f;
    const ClearValueVec clearValues{
        makeClearValueColor(clearColor),
        makeClearValueDepthStencil(clearDepth, 0u),
    };
    const auto colorCompThreshold = 0.0f; // Expect exact results.
    const tcu::Vec4 colorThreshold(colorCompThreshold, colorCompThreshold, colorCompThreshold, colorCompThreshold);
    const float depthThreshold = 0.000025f; // Between 1/65535 and 2/65535.
    const auto vertexCount     = de::sizeU32(vertices);

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    {
        // Render pass.
        beginRenderPass(ctx.vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0u),
                        de::sizeU32(clearValues), de::dataOrNull(clearValues));
        ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
        ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, pipeline.get());
        ctx.vkd.cmdDraw(cmdBuffer, vertexCount, 1u, 0u, 0u);
        endRenderPass(ctx.vkd, cmdBuffer);
    }
    {
        // Copy images to verification buffers after rendering.
        const std::vector<VkImageMemoryBarrier> preTransferBarriers = {
            makeImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                                   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                   colorBuffer.getImage(), colorSRR),

            makeImageMemoryBarrier(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                                   VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, depthBuffer.getImage(), depthSRR),
        };
        const auto preTransferStages =
            (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
             VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, preTransferStages, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      de::dataOrNull(preTransferBarriers), preTransferBarriers.size());

        const auto copyColorRegion = makeBufferImageCopy(vkExtent, colorSRL);
        const auto copyDepthRegion = makeBufferImageCopy(vkExtent, depthSRL);
        ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, colorBuffer.getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                     colorBuffer.getBuffer(), 1u, &copyColorRegion);
        ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, depthBuffer.getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                     depthBuffer.getBuffer(), 1u, &copyDepthRegion);

        const auto preHostBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &preHostBarrier);
    }
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);
    invalidateAlloc(ctx.vkd, ctx.device, colorBuffer.getBufferAllocation());
    invalidateAlloc(ctx.vkd, ctx.device, depthBuffer.getBufferAllocation());

    // Check results:
    // - Color image shouldn't have been touched.
    // - Depth buffer should have values in pairs of 2, within the accepted range.
    const auto colorTcuFormat = mapVkFormat(colorFormat);
    const auto depthTcuFormat = mapVkFormat(depthFormat);
    const tcu::ConstPixelBufferAccess colorResultAccess(colorTcuFormat, fbExtent,
                                                        colorBuffer.getBufferAllocation().getHostPtr());
    const tcu::ConstPixelBufferAccess depthResultAccess(depthTcuFormat, fbExtent,
                                                        depthBuffer.getBufferAllocation().getHostPtr());

    auto &log = context.getTestContext().getLog();
    if (!tcu::floatThresholdCompare(log, "ColorResult", "", clearColor, colorResultAccess, colorThreshold,
                                    tcu::COMPARE_LOG_ON_ERROR))
        return tcu::TestStatus::fail(
            "Unexpected color buffer contents (expected transparent black) -- check log for details");

    // Note fragment shading rate does not affect the depth buffer, only frag shader invocations.
    // When verifying the depth buffer, we'll generate the reference values normally.
    tcu::TextureLevel refDepthLevel(depthTcuFormat, fbExtent.x(), fbExtent.y(), fbExtent.z());
    tcu::PixelBufferAccess refDepthAccess = refDepthLevel.getAccess();
    const float fWidth                    = static_cast<float>(fbExtent.x());

    for (int y = 0; y < fbExtent.y(); ++y)
        for (int x = 0; x < fbExtent.x(); ++x)
        {
            // This needs to match vertex depths.
            const float depth = (static_cast<float>(x) + 0.5f) / fWidth;
            refDepthAccess.setPixDepth(depth, x, y);
        }

    if (!tcu::dsThresholdCompare(log, "DepthResult", "", refDepthAccess, depthResultAccess, depthThreshold,
                                 tcu::COMPARE_LOG_ON_ERROR))
        return tcu::TestStatus::fail("Unexpected depth buffer contents -- check log for details");

    return tcu::TestStatus::pass("Pass");
}

void checkOobSupport(Context &context, TestParams param)
{
    context.requireInstanceFunctionality("VK_KHR_get_physical_device_properties2");
    checkShadingRateSupport(context, true, false, true);

    bool allowOobFSRAttachment = false;
#ifndef CTS_USES_VULKANSC
    const VkPhysicalDeviceMaintenance7PropertiesKHR &m_maintenance7Properties = context.getMaintenance7Properties();
    allowOobFSRAttachment = m_maintenance7Properties.robustFragmentShadingRateAttachmentAccess;
#endif
    if (!allowOobFSRAttachment)
        TCU_THROW(NotSupportedError, "Fragment shading rate attachment size must match render area size");

    if (param.useRobustness2)
    {
        context.requireDeviceFunctionality("VK_EXT_robustness2");

        VkPhysicalDeviceRobustness2FeaturesEXT robustness2Features;
        robustness2Features.sType = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT;
        robustness2Features.pNext = nullptr;
        vk::VkPhysicalDeviceFeatures2 features2;

        features2.sType = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features2.pNext = &robustness2Features;

        context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &features2);

        if (robustness2Features.robustImageAccess2 == false)
            TCU_THROW(NotSupportedError, "VK_EXT_robustness2 robustImageAccess2 feature not supported");
    }
    else
    {
        context.requireDeviceFunctionality("VK_EXT_image_robustness");
    }
}

void initOOBShaders(vk::SourceCollections &programCollection, TestParams param)
{
    std::ignore = param;
    std::ostringstream vert;
    vert << "#version 460\n"
         << "vec2 positions[3] = vec2[](\n"
         << "        vec2(-1.0, -1.0),"
         << "        vec2(3.0, -1.0),"
         << "        vec2(-1.0, 3.0)"
         << ");\n"
         << "void main() {\n"
         << "        gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);\n"
         << "}";
    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

    // Default fragment shader, with vertex color.
    std::ostringstream frag;
    frag << "#version 460\n"
         << "#extension GL_EXT_fragment_shading_rate : enable\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "layout (std430, binding = 0) readonly buffer Dimensions {"
         << "    uint minW;"
         << "    uint minH;"
         << "} dimensions;\n"
         << "void main (void) {\n"
         << "    if (gl_FragCoord.x < dimensions.minW && gl_FragCoord.y < dimensions.minH) {\n"
         << "        outColor = (gl_ShadingRateEXT == (gl_ShadingRateFlag2VerticalPixelsEXT | "
            "gl_ShadingRateFlag2HorizontalPixelsEXT))"
         << "                                ? vec4(0.0, 1.0, 0.0, 1.0) : vec4(1.0, 0.0, 0.0, 1.0);"
         << "    } else {\n"
         << "        outColor = (gl_ShadingRateEXT == 0) ? vec4(0.0, 1.0, 0.0, 1.0) : vec4(1.0, 0.0, 0.0, 1.0);"
         << "    }\n"
         << "}\n";
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

Move<VkDevice> getRobustDevice(Context &context, bool robustness2)
{
    const auto &vki           = context.getInstanceInterface();
    const float queuePriority = 1.0f;
    // Create a universal queue that supports graphics and compute
    const VkDeviceQueueCreateInfo queueParams = {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, // VkStructureType              sType;
        nullptr,                                    // const void*                  pNext;
        0u,                                         // VkDeviceQueueCreateFlags     flags;
        context.getUniversalQueueFamilyIndex(),     // uint32_t                     queueFamilyIndex;
        1u,                                         // uint32_t                     queueCount;
        &queuePriority                              // const float*                 pQueuePriorities;
    };

    VkPhysicalDeviceFeatures2 features2 = getPhysicalDeviceFeatures2(vki, context.getPhysicalDevice());
    VkPhysicalDeviceRobustness2FeaturesEXT robustness2Features    = initVulkanStructure(&features2);
    robustness2Features.robustImageAccess2                        = true;
    VkPhysicalDeviceImageRobustnessFeaturesEXT robustnessFeatures = initVulkanStructure(&features2);
    robustnessFeatures.robustImageAccess                          = true;
    VkPhysicalDeviceFragmentShadingRateFeaturesKHR fsrFeatures =
        initVulkanStructure(robustness2 ? (void *)&robustness2Features : &robustnessFeatures);
    fsrFeatures.attachmentFragmentShadingRate = true;
    fsrFeatures.pipelineFragmentShadingRate   = true;
    const auto &extensionPtrs                 = context.getDeviceCreationExtensions();

    void *pNext = (void *)&fsrFeatures;
#ifdef CTS_USES_VULKANSC
    VkDeviceObjectReservationCreateInfo memReservationInfo = context.getTestContext().getCommandLine().isSubProcess() ?
                                                                 context.getResourceInterface()->getStatMax() :
                                                                 resetDeviceObjectReservationCreateInfo();
    memReservationInfo.pNext                               = pNext;
    pNext                                                  = &memReservationInfo;

    VkPhysicalDeviceVulkanSC10Features sc10Features = createDefaultSC10Features();
    sc10Features.pNext                              = pNext;
    pNext                                           = &sc10Features;

    VkPipelineCacheCreateInfo pcCI;
    std::vector<VkPipelinePoolSize> poolSizes;
    if (context.getTestContext().getCommandLine().isSubProcess())
    {
        if (context.getResourceInterface()->getCacheDataSize() > 0)
        {
            pcCI = {
                VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, // VkStructureType              sType;
                nullptr,                                      // const void*                  pNext;
                VK_PIPELINE_CACHE_CREATE_READ_ONLY_BIT |
                    VK_PIPELINE_CACHE_CREATE_USE_APPLICATION_STORAGE_BIT, // VkPipelineCacheCreateFlags   flags;
                context.getResourceInterface()->getCacheDataSize(), // uintptr_t                    initialDataSize;
                context.getResourceInterface()->getCacheData()      // const void*                  pInitialData;
            };
            memReservationInfo.pipelineCacheCreateInfoCount = 1;
            memReservationInfo.pPipelineCacheCreateInfos    = &pcCI;
        }

        poolSizes = context.getResourceInterface()->getPipelinePoolSizes();
        if (!poolSizes.empty())
        {
            memReservationInfo.pipelinePoolSizeCount = static_cast<uint32_t>(poolSizes.size());
            memReservationInfo.pPipelinePoolSizes    = poolSizes.data();
        }
    }
#endif

    const VkDeviceCreateInfo deviceParams = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, // VkStructureType                  sType;
        pNext,                                // const void*                      pNext;
        0u,                                   // VkDeviceCreateFlags              flags;
        1u,                                   // uint32_t                         queueCreateInfoCount;
        &queueParams,                         // const VkDeviceQueueCreateInfo*   pQueueCreateInfos;
        0u,                                   // uint32_t                         enabledLayerCount;
        nullptr,                              // const char* const*               ppEnabledLayerNames;
        de::sizeU32(extensionPtrs),           // uint32_t                         enabledExtensionCount;
        de::dataOrNull(extensionPtrs),        // const char* const*               ppEnabledExtensionNames;
        nullptr                               // const VkPhysicalDeviceFeatures*  pEnabledFeatures;
    };
    const auto instance = context.getInstance();

    return createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(),
                              context.getPlatformInterface(), instance, vki, context.getPhysicalDevice(),
                              &deviceParams);
}

tcu::TestStatus testOOB(Context &context, TestParams params)
{
    const auto &fsrProperties          = context.getFragmentShadingRateProperties();
    const auto &fsrAttachmentTexelSize = fsrProperties.minFragmentShadingRateAttachmentTexelSize;
    const auto colorFormat             = VK_FORMAT_R8G8B8A8_UNORM;
    const auto colorUsage              = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const auto colorSRL                = makeDefaultImageSubresourceLayers();
    const auto bindPoint               = VK_PIPELINE_BIND_POINT_GRAPHICS;
    const auto fsrFormat               = VK_FORMAT_R8_UINT;
    const auto fsrUsage = (VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    const auto sampleCount = VK_SAMPLE_COUNT_1_BIT;
    const auto &vki        = context.getInstanceInterface();
    const auto &vkp        = context.getPlatformInterface();
    const auto device      = getRobustDevice(context, params.useRobustness2);
    const auto instance    = context.getInstance();
    auto driver = de::MovePtr<DeviceDriver>(new DeviceDriver(vkp, instance, device.get(), context.getUsedApiVersion(),
                                                             context.getTestContext().getCommandLine()));
    auto queueFamilyIndex      = context.getUniversalQueueFamilyIndex();
    auto queue                 = getDeviceQueue(*driver, *device, queueFamilyIndex, 0u);
    auto alloc                 = de::MovePtr<Allocator>(new SimpleAllocator(
        *driver, device.get(), getPhysicalDeviceMemoryProperties(vki, context.getPhysicalDevice())));
    const DeviceInterface &vkd = *driver;

    struct Dims
    {
        int w;
        int h;
    } dimensions;
    dimensions.w = fsrAttachmentTexelSize.width;
    dimensions.h = fsrAttachmentTexelSize.height;

    const auto bufferSize = static_cast<VkDeviceSize>(sizeof(Dims));
    BufferWithMemory buffer(vkd, *device, *alloc, makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                            MemoryRequirement::HostVisible);
    auto &bufferAlloc = buffer.getAllocation();
    void *bufferData  = bufferAlloc.getHostPtr();
    deMemcpy(bufferData, &dimensions, sizeof(Dims));
    flushAlloc(vkd, device.get(), bufferAlloc);

    const auto outputSize = VkExtent3D{fsrAttachmentTexelSize.width * 4, fsrAttachmentTexelSize.height * 4, 1};
    const tcu::IVec3 fbExtent(static_cast<int>(outputSize.width), static_cast<int>(outputSize.height),
                              static_cast<int>(outputSize.depth));

    vk::ImageWithBuffer colorBuffer(vkd, *device, *alloc, outputSize, colorFormat, colorUsage, VK_IMAGE_TYPE_2D);

    const auto fsrExtent            = makeExtent3D(1, 1, 1u); // 1 pixel for the whole image.
    const auto mipLevelScaleFactor  = 2u;
    const auto fsrExtentScaled      = makeExtent3D(1u * mipLevelScaleFactor, 1u * mipLevelScaleFactor, 1u);
    const auto fsrImgCreationExtent = params.useBaseMipLevel1 ? fsrExtentScaled : fsrExtent;

    const auto fsrMipCount     = (params.useBaseMipLevel1 ? 2u : 1u);
    const auto fsrBaseMipLevel = (params.useBaseMipLevel1 ? 1u : 0u);

    const auto fsrSRR = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, fsrBaseMipLevel, 1u, 0u, 1u);
    // Fragment shading rate attachment.
    const VkImageCreateInfo fsrAttachmentCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, //  VkStructureType         sType;
        nullptr,                             //  const void*             pNext;
        0u,                                  //  VkImageCreateFlags      flags;
        VK_IMAGE_TYPE_2D,                    //  VkImageType             imageType;
        fsrFormat,                           //  VkFormat                format;
        fsrImgCreationExtent,                //  VkExtent3D              extent;
        fsrMipCount,                         //  uint32_t                mipLevels;
        1u,                                  //  uint32_t                arrayLayers;
        sampleCount,                         //  VkSampleCountFlagBits   samples;
        VK_IMAGE_TILING_OPTIMAL,             //  VkImageTiling           tiling;
        fsrUsage,                            //  VkImageUsageFlags       usage;
        VK_SHARING_MODE_EXCLUSIVE,           //  VkSharingMode           sharingMode;
        0u,                                  //  uint32_t                queueFamilyIndexCount;
        nullptr,                             //  const uint32_t*         pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           //  VkImageLayout           initialLayout;
    };
    ImageWithMemory fsrAttachment(vkd, *device, *alloc, fsrAttachmentCreateInfo, MemoryRequirement::Any);
    const auto fsrAttView = makeImageView(vkd, *device, fsrAttachment.get(), VK_IMAGE_VIEW_TYPE_2D, fsrFormat, fsrSRR);

    const auto &binaries  = context.getBinaryCollection();
    const auto vertModule = createShaderModule(vkd, *device, binaries.get("vert"));
    const auto fragModule = createShaderModule(vkd, *device, binaries.get("frag"));

    const std::vector<VkAttachmentDescription2> attachmentDescriptions{
        // Color attachment.
        {
            VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2, //  VkStructureType                 sType;
            nullptr,                                    //  const void*                     pNext;
            0u,                                         //  VkAttachmentDescriptionFlags    flags;
            colorFormat,                                //  VkFormat                        format;
            sampleCount,                                //  VkSampleCountFlagBits           samples;
            VK_ATTACHMENT_LOAD_OP_CLEAR,                //  VkAttachmentLoadOp              loadOp;
            VK_ATTACHMENT_STORE_OP_STORE,               //  VkAttachmentStoreOp             storeOp;
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,            //  VkAttachmentLoadOp              stencilLoadOp;
            VK_ATTACHMENT_STORE_OP_DONT_CARE,           //  VkAttachmentStoreOp             stencilStoreOp;
            VK_IMAGE_LAYOUT_UNDEFINED,                  //  VkImageLayout                   initialLayout;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,   //  VkImageLayout                   finalLayout;
        },
        // FSR attachment.
        {
            VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2, //  VkStructureType                 sType;
            nullptr,                                    //  const void*                     pNext;
            0u,                                         //  VkAttachmentDescriptionFlags    flags;
            fsrFormat,                                  //  VkFormat                        format;
            sampleCount,                                //  VkSampleCountFlagBits           samples;
            VK_ATTACHMENT_LOAD_OP_LOAD,                 //  VkAttachmentLoadOp              loadOp;
            VK_ATTACHMENT_STORE_OP_STORE,               //  VkAttachmentStoreOp             storeOp;
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,            //  VkAttachmentLoadOp              stencilLoadOp;
            VK_ATTACHMENT_STORE_OP_DONT_CARE,           //  VkAttachmentStoreOp             stencilStoreOp;
            VK_IMAGE_LAYOUT_GENERAL,                    //  VkImageLayout                   initialLayout;
            VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR, //  VkImageLayout                   finalLayout;
        },
    };

    const VkAttachmentReference2 colorAttRef = {
        VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2, //  VkStructureType     sType;
        nullptr,                                  //  const void*         pNext;
        0u,                                       //  uint32_t            attachment;
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, //  VkImageLayout       layout;
        VK_IMAGE_ASPECT_COLOR_BIT,                //  VkImageAspectFlags  aspectMask;
    };

    const VkAttachmentReference2 fsrAttRef = {
        VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,                     //  VkStructureType     sType;
        nullptr,                                                      //  const void*         pNext;
        1u,                                                           //  uint32_t            attachment;
        VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR, //  VkImageLayout       layout;
        VK_IMAGE_ASPECT_COLOR_BIT,                                    //  VkImageAspectFlags  aspectMask;
    };

    const VkFragmentShadingRateAttachmentInfoKHR fsrAttInfo = {
        VK_STRUCTURE_TYPE_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR, //  VkStructureType                 sType;
        nullptr,                                                     //  const void*                     pNext;
        &fsrAttRef, //  const VkAttachmentReference2*   pFragmentShadingRateAttachment;
        makeExtent2D(fsrAttachmentTexelSize.width,
                     fsrAttachmentTexelSize.height), //  VkExtent2D                      shadingRateAttachmentTexelSize;
    };

    const VkSubpassDescription2 subpassDescription{
        VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2, //  VkStructureType                 sType;
        &fsrAttInfo,                             //  const void*                     pNext;
        0u,                                      //  VkSubpassDescriptionFlags       flags;
        bindPoint,                               //  VkPipelineBindPoint             pipelineBindPoint;
        0u,                                      //  uint32_t                        viewMask;
        0u,                                      //  uint32_t                        inputAttachmentCount;
        nullptr,                                 //  const VkAttachmentReference2*   pInputAttachments;
        1u,                                      //  uint32_t                        colorAttachmentCount;
        &colorAttRef,                            //  const VkAttachmentReference2*   pColorAttachments;
        nullptr,                                 //  const VkAttachmentReference2*   pResolveAttachments;
        nullptr,                                 //  const VkAttachmentReference2*   pDepthStencilAttachment;
        0u,                                      //  uint32_t                        preserveAttachmentCount;
        nullptr,                                 //  const uint32_t*                 pPreserveAttachments;
    };

    const VkRenderPassCreateInfo2 renderPassCreateInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2, //  VkStructureType                 sType;
        nullptr,                                     //  const void*                     pNext;
        0u,                                          //  VkRenderPassCreateFlags         flags;
        de::sizeU32(attachmentDescriptions),         //  uint32_t                        attachmentCount;
        de::dataOrNull(attachmentDescriptions),      //  const VkAttachmentDescription2* pAttachments;
        1u,                                          //  uint32_t                        subpassCount;
        &subpassDescription,                         //  const VkSubpassDescription2*    pSubpasses;
        0u,                                          //  uint32_t                        dependencyCount;
        nullptr,                                     //  const VkSubpassDependency2*     pDependencies;
        0u,                                          //  uint32_t                        correlatedViewMaskCount;
        nullptr,                                     //  const uint32_t*                 pCorrelatedViewMasks;
    };

    const auto renderPass = createRenderPass2(vkd, *device, &renderPassCreateInfo);

    const std::vector<VkImageView> attachmentViews{colorBuffer.getImageView(), fsrAttView.get()};
    const auto framebuffer = makeFramebuffer(vkd, *device, renderPass.get(), de::sizeU32(attachmentViews),
                                             de::dataOrNull(attachmentViews), fbExtent.x(), fbExtent.y());

    const std::vector<VkViewport> viewports(1u, makeViewport(fbExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(fbExtent));

    // Use the rate according to the attachment.
    const auto fragmentShadingRateStateCreateInfo = makeFragmentShadingRateStateCreateInfo(
        1u, 1u, VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR, VK_FRAGMENT_SHADING_RATE_COMBINER_OP_REPLACE_KHR);

    DescriptorSetLayoutBuilder layoutBuilder;
    layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);

    auto descriptorSetLayout    = layoutBuilder.build(vkd, device.get());
    auto graphicsPipelineLayout = makePipelineLayout(vkd, device.get(), descriptorSetLayout.get());

    static const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, //  VkStructureType                             sType;
        nullptr, //  const void*                                 pNext;
        0u,      //  VkPipelineVertexInputStateCreateFlags       flags;
        0u,      //  uint32_t                                    vertexBindingDescriptionCount;
        nullptr, //  const VkVertexInputBindingDescription*      pVertexBindingDescriptions;
        0,       //  uint32_t                                    vertexAttributeDescriptionCount;
        nullptr, //  const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions;
    };

    const auto pipelineVRS = makeGraphicsPipeline(
        vkd, *device, graphicsPipelineLayout.get(), vertModule.get(), VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
        fragModule.get(), renderPass.get(), viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0u, 0u,
        &vertexInputStateCreateInfo, nullptr, nullptr, nullptr, nullptr, nullptr, &fragmentShadingRateStateCreateInfo);

    CommandPoolWithBuffer cmd(vkd, *device, queueFamilyIndex);
    const auto cmdBuffer = cmd.cmdBuffer.get();

#if 0
      const int gl_ShadingRateFlag2VerticalPixelsEXT = 1;
      const int gl_ShadingRateFlag4VerticalPixelsEXT = 2;
      const int gl_ShadingRateFlag2HorizontalPixelsEXT = 4;
      const int gl_ShadingRateFlag4HorizontalPixelsEXT = 8;
#endif
    using ClearValueVec = std::vector<VkClearValue>;
    const uint32_t clearAttRate =
        5u; // 2x2: (gl_ShadingRateFlag2HorizontalPixelsEXT | gl_ShadingRateFlag2VerticalPixelsEXT)
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 0.0f);
    const ClearValueVec clearValues{
        makeClearValueColor(clearColor),
    };
    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    const auto descriptorPool =
        poolBuilder.build(vkd, device.get(), VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1);
    const auto descriptorSetBuffer =
        makeDescriptorSet(vkd, device.get(), descriptorPool.get(), descriptorSetLayout.get());

    // Update descriptor sets.
    DescriptorSetUpdateBuilder updater;

    const auto bufferInfo = makeDescriptorBufferInfo(buffer.get(), 0ull, bufferSize);
    updater.writeSingle(descriptorSetBuffer.get(), DescriptorSetUpdateBuilder::Location::binding(0u),
                        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferInfo);

    updater.update(vkd, device.get());

    beginCommandBuffer(vkd, cmdBuffer);
    const auto imgSRR = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, fsrMipCount, 0u, 1u);
    auto barrier1     = makeImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                               VK_IMAGE_LAYOUT_GENERAL, fsrAttachment.get(), imgSRR);
    vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                           nullptr, 0, nullptr, 1, &barrier1);
    auto fsr_color = makeClearValueColorU32(clearAttRate, 0u, 0u, 0u).color;
    vkd.cmdClearColorImage(cmdBuffer, fsrAttachment.get(), VK_IMAGE_LAYOUT_GENERAL, &fsr_color, 1, &imgSRR);
    auto barrier2 =
        makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR,
                               VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, fsrAttachment.get(), imgSRR);
    vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR, 0, 0, nullptr, 0, nullptr, 1,
                           &barrier2);
    {
        // Render pass.
        beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0u), de::sizeU32(clearValues),
                        de::dataOrNull(clearValues));
        vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineLayout.get(), 0u, 1,
                                  &descriptorSetBuffer.get(), 0u, nullptr);
        vkd.cmdBindPipeline(cmdBuffer, bindPoint, pipelineVRS.get());
        vkd.cmdDraw(cmdBuffer, 3, 1u, 0u, 0u);
        endRenderPass(vkd, cmdBuffer);
    }
    // Copy image to verification buffer after rendering.
    const auto colorSubRes        = makeDefaultImageSubresourceRange();
    const auto preTransferBarrier = makeImageMemoryBarrier(
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, colorBuffer.getImage(), colorSubRes);
    cmdPipelineImageMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  VK_PIPELINE_STAGE_TRANSFER_BIT, &preTransferBarrier);
    const auto copyRegion = makeBufferImageCopy(makeExtent3D(fbExtent.x(), fbExtent.y(), 1), colorSRL);
    vkd.cmdCopyImageToBuffer(cmdBuffer, colorBuffer.getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             colorBuffer.getBuffer(), 1u, &copyRegion);
    const auto preHostBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    cmdPipelineMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                             &preHostBarrier);

    endCommandBuffer(vkd, cmdBuffer);
    submitCommandsAndWait(vkd, device.get(), queue, cmdBuffer);
    invalidateAlloc(vkd, device.get(), colorBuffer.getBufferAllocation());

    const auto colorTcuFormat = mapVkFormat(colorFormat);
    const tcu::ConstPixelBufferAccess colorResultAccess(colorTcuFormat, fbExtent,
                                                        colorBuffer.getBufferAllocation().getHostPtr());

    auto &log = context.getTestContext().getLog();
    if (!tcu::floatThresholdCompare(log, "Compare", "Result comparison", tcu::Vec4(0.0, 1.0, 0.0, 1.0),
                                    colorResultAccess, tcu::Vec4(0.0), tcu::COMPARE_LOG_ON_ERROR))
        return tcu::TestStatus::fail("Color output does not match reference, image added to log");

    return tcu::TestStatus::pass("Pass");
}

} // namespace

void createFragmentShadingRateMiscTests(tcu::TestCaseGroup *group)
{
    {
        const char *testName = "enable_disable_attachment";
        // Test drawing with VRS enabled by an attachment and then disabled
        addFunctionCaseWithPrograms(group, testName, checkEnableDisableSupport, initEnableDisableShaders,
                                    testEnableDisable);
    }
    {
        const char *testName = "no_frag_shader";
        // Test drawing with VRS enabled and no frag shader
        addFunctionCaseWithPrograms(group, testName, checkNoFragSupport, initNoFragShaders, testNoFrag);
    }
    {
        TestParams params = {false, false};
        addFunctionCaseWithPrograms<TestParams>(group, "test_oob_attachment", checkOobSupport, initOOBShaders, testOOB,
                                                params);
        params.useRobustness2 = true;
        addFunctionCaseWithPrograms<TestParams>(group, "test_oob_attachment_robustness2", checkOobSupport,
                                                initOOBShaders, testOOB, params);
    }
}

} // namespace FragmentShadingRate
} // namespace vkt
