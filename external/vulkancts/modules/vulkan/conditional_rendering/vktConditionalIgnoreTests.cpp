/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 The Khronos Group Inc.
 * Copyright (c) 2022 Valve Corporation.
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
 * \brief Test for conditional rendering with commands that ignore conditions
 *//*--------------------------------------------------------------------*/

#include "vktConditionalIgnoreTests.hpp"
#include "vktConditionalRenderingTestUtil.hpp"

#include "vktTestCase.hpp"
#include "vkPrograms.hpp"
#include "vkRefUtil.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkImageWithMemory.hpp"

#include "vktTestCase.hpp"

#include "vkDefs.hpp"
#include "vkTypeUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "tcuImageCompare.hpp"
#include "tcuDefs.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuTestLog.hpp"

#include <vector>
#include <sstream>
#include <algorithm>
#include <utility>
#include <iterator>
#include <string>
#include <limits>
#include <memory>
#include <functional>
#include <cstddef>
#include <set>
#include <numeric>

namespace vkt
{
namespace conditional
{
namespace
{

using namespace vk;

//make a buffer to read an image back after rendering
std::unique_ptr<BufferWithMemory> makeBufferForImage(const DeviceInterface &vkd, const VkDevice device,
                                                     Allocator &allocator, VkFormat imageFormat, VkExtent3D imageExtent)
{
    const auto tcuFormat      = mapVkFormat(imageFormat);
    const auto outBufferSize  = static_cast<VkDeviceSize>(static_cast<uint32_t>(tcu::getPixelSize(tcuFormat)) *
                                                         imageExtent.width * imageExtent.height);
    const auto outBufferUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    const auto outBufferInfo  = makeBufferCreateInfo(outBufferSize, outBufferUsage);

    auto outBuffer = std::unique_ptr<BufferWithMemory>(
        new BufferWithMemory(vkd, device, allocator, outBufferInfo, MemoryRequirement::HostVisible));

    return outBuffer;
}

class ConditionalIgnoreClearColorTestCase : public vkt::TestCase
{
public:
    ConditionalIgnoreClearColorTestCase(tcu::TestContext &context, const std::string &name,
                                        const ConditionalData &data);
    void initPrograms(SourceCollections &) const override
    {
    }
    TestInstance *createInstance(Context &context) const override;
    void checkSupport(Context &context) const override
    {
        context.requireDeviceFunctionality("VK_EXT_conditional_rendering");
        if (m_data.conditionInherited && !context.getConditionalRenderingFeaturesEXT().inheritedConditionalRendering)
            TCU_THROW(NotSupportedError, "Device does not support inherited conditional rendering");
    }

private:
    const ConditionalData m_data;
};

class ConditionalIgnoreClearColorTestInstance : public vkt::MultiQueueRunnerTestInstance
{
public:
    ConditionalIgnoreClearColorTestInstance(Context &context, const ConditionalData &data)
        : vkt::MultiQueueRunnerTestInstance(context, vkt::COMPUTE_QUEUE)
        , m_data(data){};
    virtual tcu::TestStatus queuePass(const QueueData &queueData) override;

private:
    const ConditionalData m_data;
};

ConditionalIgnoreClearColorTestCase::ConditionalIgnoreClearColorTestCase(tcu::TestContext &context,
                                                                         const std::string &name,
                                                                         const ConditionalData &data)
    : vkt::TestCase(context, name)
    , m_data(data)
{
}

TestInstance *ConditionalIgnoreClearColorTestCase::createInstance(Context &context) const
{
    return new ConditionalIgnoreClearColorTestInstance(context, m_data);
}

tcu::TestStatus ConditionalIgnoreClearColorTestInstance::queuePass(const QueueData &queueData)
{
    const auto &vkd        = m_context.getDeviceInterface();
    const auto device      = m_context.getDevice();
    auto &alloc            = m_context.getDefaultAllocator();
    const auto imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const auto imageExtent = makeExtent3D(2, 2, 1u);
    const auto qIndex      = queueData.familyIndex;

    const auto expected = tcu::Vec4(0.0, 0.0, 0.0, 1.0);

    const VkClearColorValue clearColor      = {{0.0, 0.0, 0.0, 1.0}};
    const VkClearColorValue clearColorWrong = {{1.0, 0.0, 0.0, 1.0}};

    const tcu::IVec3 imageDim(static_cast<int>(imageExtent.width), static_cast<int>(imageExtent.height),
                              static_cast<int>(imageExtent.depth));
    const tcu::IVec2 imageSize(imageDim.x(), imageDim.y());

    de::MovePtr<ImageWithMemory> colorAttachment;
    de::MovePtr<ImageWithMemory> depthAttachment;

    //create color image
    const auto imageUsage = static_cast<VkImageUsageFlags>(
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    const VkImageCreateInfo imageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        0u,                                  // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        imageFormat,                         // VkFormat format;
        imageExtent,                         // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        1u,                                  // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        imageUsage,                          // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0,                                   // uint32_t queueFamilyIndexCount;
        nullptr,                             // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };

    const auto colorSubresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    colorAttachment =
        de::MovePtr<ImageWithMemory>(new ImageWithMemory(vkd, device, alloc, imageCreateInfo, MemoryRequirement::Any));
    auto colorAttachmentView =
        makeImageView(vkd, device, colorAttachment->get(), VK_IMAGE_VIEW_TYPE_2D, imageFormat, colorSubresourceRange);

    //buffer to read the output
    const auto outBuffer       = makeBufferForImage(vkd, device, alloc, imageFormat, imageExtent);
    const auto &outBufferAlloc = outBuffer->getAllocation();
    const void *outBufferData  = outBufferAlloc.getHostPtr();

    const auto commandPool = createCommandPool(vkd, device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, qIndex);
    auto commandBuffer     = allocateCommandBuffer(vkd, device, commandPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    auto commandBuffer2    = allocateCommandBuffer(vkd, device, commandPool.get(), VK_COMMAND_BUFFER_LEVEL_SECONDARY);
    auto commandBuffer3    = allocateCommandBuffer(vkd, device, commandPool.get(), VK_COMMAND_BUFFER_LEVEL_SECONDARY);

    auto conditionalBuffer = createConditionalRenderingBuffer(m_context, m_data);
    //prepare command buffers
    const bool useSecondaryCmdBuffer = m_data.conditionInherited || m_data.conditionInSecondaryCommandBuffer;

    if (m_data.secondaryCommandBufferNested)
    {
        m_context.requireDeviceFunctionality("VK_EXT_nested_command_buffer");
        const auto &features = m_context.getNestedCommandBufferFeaturesEXT();
        if (!features.nestedCommandBuffer)
            TCU_THROW(NotSupportedError, "nestedCommandBuffer is not supported");
    }

    VkCommandBufferInheritanceConditionalRenderingInfoEXT conditionalRenderingInheritanceInfo = initVulkanStructure();
    conditionalRenderingInheritanceInfo.conditionalRenderingEnable = m_data.conditionInherited ? VK_TRUE : VK_FALSE;

    const VkCommandBufferInheritanceInfo inheritanceInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
        &conditionalRenderingInheritanceInfo,
        VK_NULL_HANDLE,                    // renderPass
        0u,                                // subpass
        VK_NULL_HANDLE,                    // framebuffer
        VK_FALSE,                          // occlusionQueryEnable
        (VkQueryControlFlags)0u,           // queryFlags
        (VkQueryPipelineStatisticFlags)0u, // pipelineStatistics
    };

    const VkCommandBufferBeginInfo commandBufferBeginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr,
                                                             VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                                                             &inheritanceInfo};

    beginCommandBuffer(vkd, commandBuffer.get());
    //transition color and depth images
    VkImageMemoryBarrier colorTransition =
        makeImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                               colorAttachment.get()->get(), colorSubresourceRange);
    VkImageMemoryBarrier barriers[] = {colorTransition};
    cmdPipelineImageMemoryBarrier(vkd, commandBuffer.get(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                  VK_PIPELINE_STAGE_TRANSFER_BIT, barriers, DE_LENGTH_OF_ARRAY(barriers));

    //clear to the incorrect color
    vkd.cmdClearColorImage(commandBuffer.get(), colorAttachment.get()->get(), VK_IMAGE_LAYOUT_GENERAL, &clearColorWrong,
                           1, &colorSubresourceRange);

    const auto barrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
    cmdPipelineMemoryBarrier(vkd, commandBuffer.get(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             &barrier);

    //do all combinations of clears
    if (useSecondaryCmdBuffer)
    {
        if (m_data.secondaryCommandBufferNested)
        {
            vkd.beginCommandBuffer(*commandBuffer3, &commandBufferBeginInfo);
        }

        vkd.beginCommandBuffer(*commandBuffer2, &commandBufferBeginInfo);
        if (m_data.conditionInSecondaryCommandBuffer)
        {
            beginConditionalRendering(vkd, commandBuffer2.get(), *conditionalBuffer, m_data);
        }
        else
        {
            beginConditionalRendering(vkd, commandBuffer.get(), *conditionalBuffer, m_data);
        }

        //clear to the correct colors
        vkd.cmdClearColorImage(commandBuffer2.get(), colorAttachment.get()->get(), VK_IMAGE_LAYOUT_GENERAL, &clearColor,
                               1, &colorSubresourceRange);

        if (m_data.conditionInSecondaryCommandBuffer)
        {
            vkd.cmdEndConditionalRenderingEXT(commandBuffer2.get());
        }
        else
        {
            vkd.cmdEndConditionalRenderingEXT(commandBuffer.get());
        }

        vkd.endCommandBuffer(*commandBuffer2);
        if (m_data.secondaryCommandBufferNested)
        {
            vkd.cmdExecuteCommands(commandBuffer3.get(), 1, &commandBuffer2.get());
            vkd.endCommandBuffer(*commandBuffer3);
            vkd.cmdExecuteCommands(commandBuffer.get(), 1, &commandBuffer3.get());
        }
        else
        {
            vkd.cmdExecuteCommands(commandBuffer.get(), 1, &commandBuffer2.get());
        }
    }
    else
    {
        beginConditionalRendering(vkd, commandBuffer.get(), *conditionalBuffer, m_data);

        //clear to the correct colors
        vkd.cmdClearColorImage(commandBuffer.get(), colorAttachment.get()->get(), VK_IMAGE_LAYOUT_GENERAL, &clearColor,
                               1, &colorSubresourceRange);

        vkd.cmdEndConditionalRenderingEXT(commandBuffer.get());
    }
    copyImageToBuffer(vkd, commandBuffer.get(), colorAttachment.get()->get(), (*outBuffer).get(), imageSize,
                      VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

    endCommandBuffer(vkd, commandBuffer.get());
    submitCommandsAndWait(vkd, device, queueData.handle, commandBuffer.get());

    invalidateAlloc(vkd, device, outBufferAlloc);
    tcu::ConstPixelBufferAccess outPixels(mapVkFormat(imageFormat), imageDim, outBufferData);

    //the clear should happen in every case
    if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare color", "color image comparison",
                                    expected, outPixels, tcu::Vec4(0.0), tcu::COMPARE_LOG_ON_ERROR))
        return tcu::TestStatus::fail("Color image verification failed, check log for details");

    return tcu::TestStatus::pass("Pass");
}

class ConditionalIgnoreClearDepthTestCase : public vkt::TestCase
{
public:
    ConditionalIgnoreClearDepthTestCase(tcu::TestContext &context, const std::string &name,
                                        const ConditionalData &data);
    void initPrograms(SourceCollections &) const override
    {
    }
    TestInstance *createInstance(Context &context) const override;
    void checkSupport(Context &context) const override
    {
        context.requireDeviceFunctionality("VK_EXT_conditional_rendering");
        if (m_data.conditionInherited && !context.getConditionalRenderingFeaturesEXT().inheritedConditionalRendering)
            TCU_THROW(NotSupportedError, "Device does not support inherited conditional rendering");
    }

private:
    const ConditionalData m_data;
};

class ConditionalIgnoreClearDepthTestInstance : public vkt::TestInstance
{
public:
    ConditionalIgnoreClearDepthTestInstance(Context &context, const ConditionalData &data)
        : vkt::TestInstance(context)
        , m_data(data){};
    virtual tcu::TestStatus iterate(void);

private:
    const ConditionalData m_data;
};

ConditionalIgnoreClearDepthTestCase::ConditionalIgnoreClearDepthTestCase(tcu::TestContext &context,
                                                                         const std::string &name,
                                                                         const ConditionalData &data)
    : vkt::TestCase(context, name)
    , m_data(data)
{
}

TestInstance *ConditionalIgnoreClearDepthTestCase::createInstance(Context &context) const
{
    return new ConditionalIgnoreClearDepthTestInstance(context, m_data);
}

tcu::TestStatus ConditionalIgnoreClearDepthTestInstance::iterate(void)
{
    const auto &vkd        = m_context.getDeviceInterface();
    const auto device      = m_context.getDevice();
    auto &alloc            = m_context.getDefaultAllocator();
    const auto depthFormat = VK_FORMAT_D16_UNORM;
    const auto imageExtent = makeExtent3D(2, 2, 1u);
    const auto qIndex      = m_context.getUniversalQueueFamilyIndex();

    const auto expected = tcu::Vec4(0.0, 0.0, 0.0, 1.0);

    const VkClearDepthStencilValue depthClear      = {0.0, 0};
    const VkClearDepthStencilValue depthClearWrong = {1.0, 0};

    const tcu::IVec3 imageDim(static_cast<int>(imageExtent.width), static_cast<int>(imageExtent.height),
                              static_cast<int>(imageExtent.depth));
    const tcu::IVec2 imageSize(imageDim.x(), imageDim.y());

    de::MovePtr<ImageWithMemory> depthAttachment;

    //create depth image
    const auto depthImageUsage =
        static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                       VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    const VkImageCreateInfo depthImageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        0u,                                  // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        depthFormat,                         // VkFormat format;
        imageExtent,                         // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        1u,                                  // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        depthImageUsage,                     // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        nullptr,                             // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };

    const auto depthSubresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u);
    depthAttachment                  = de::MovePtr<ImageWithMemory>(
        new ImageWithMemory(vkd, device, alloc, depthImageCreateInfo, MemoryRequirement::Any));
    auto depthAttachmentView =
        makeImageView(vkd, device, depthAttachment->get(), VK_IMAGE_VIEW_TYPE_2D, depthFormat, depthSubresourceRange);

    //buffer to read the output
    const auto outDepthBuffer       = makeBufferForImage(vkd, device, alloc, depthFormat, imageExtent);
    const auto &outDepthBufferAlloc = outDepthBuffer->getAllocation();
    const void *outDepthBufferData  = outDepthBufferAlloc.getHostPtr();

    const auto commandPool = createCommandPool(vkd, device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, qIndex);
    auto commandBuffer     = allocateCommandBuffer(vkd, device, commandPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    auto commandBuffer2    = allocateCommandBuffer(vkd, device, commandPool.get(), VK_COMMAND_BUFFER_LEVEL_SECONDARY);
    auto commandBuffer3    = allocateCommandBuffer(vkd, device, commandPool.get(), VK_COMMAND_BUFFER_LEVEL_SECONDARY);

    auto conditionalBuffer = createConditionalRenderingBuffer(m_context, m_data);
    //prepare command buffers
    const bool useSecondaryCmdBuffer = m_data.conditionInherited || m_data.conditionInSecondaryCommandBuffer;

    if (m_data.secondaryCommandBufferNested)
    {
        m_context.requireDeviceFunctionality("VK_EXT_nested_command_buffer");
        const auto &features = m_context.getNestedCommandBufferFeaturesEXT();
        if (!features.nestedCommandBuffer)
            TCU_THROW(NotSupportedError, "nestedCommandBuffer is not supported");
    }

    VkCommandBufferInheritanceConditionalRenderingInfoEXT conditionalRenderingInheritanceInfo = initVulkanStructure();
    conditionalRenderingInheritanceInfo.conditionalRenderingEnable = m_data.conditionInherited ? VK_TRUE : VK_FALSE;

    const VkCommandBufferInheritanceInfo inheritanceInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
        &conditionalRenderingInheritanceInfo,
        VK_NULL_HANDLE,                    // renderPass
        0u,                                // subpass
        VK_NULL_HANDLE,                    // framebuffer
        VK_FALSE,                          // occlusionQueryEnable
        (VkQueryControlFlags)0u,           // queryFlags
        (VkQueryPipelineStatisticFlags)0u, // pipelineStatistics
    };

    const VkCommandBufferBeginInfo commandBufferBeginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr,
                                                             VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                                                             &inheritanceInfo};

    beginCommandBuffer(vkd, commandBuffer.get());
    //transition depth images
    VkImageMemoryBarrier depthTransition =
        makeImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                               depthAttachment.get()->get(), depthSubresourceRange);
    VkImageMemoryBarrier barriers[] = {depthTransition};
    cmdPipelineImageMemoryBarrier(vkd, commandBuffer.get(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                  VK_PIPELINE_STAGE_TRANSFER_BIT, barriers, DE_LENGTH_OF_ARRAY(barriers));

    //clear to the incorrect value
    vkd.cmdClearDepthStencilImage(commandBuffer.get(), depthAttachment.get()->get(), VK_IMAGE_LAYOUT_GENERAL,
                                  &depthClearWrong, 1, &depthSubresourceRange);

    const auto barrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
    cmdPipelineMemoryBarrier(vkd, commandBuffer.get(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             &barrier);

    //do all combinations of clears
    if (useSecondaryCmdBuffer)
    {
        if (m_data.secondaryCommandBufferNested)
        {
            vkd.beginCommandBuffer(*commandBuffer3, &commandBufferBeginInfo);
        }

        vkd.beginCommandBuffer(*commandBuffer2, &commandBufferBeginInfo);
        if (m_data.conditionInSecondaryCommandBuffer)
        {
            beginConditionalRendering(vkd, commandBuffer2.get(), *conditionalBuffer, m_data);
        }
        else
        {
            beginConditionalRendering(vkd, commandBuffer.get(), *conditionalBuffer, m_data);
        }

        //clear to the correct colors
        vkd.cmdClearDepthStencilImage(commandBuffer2.get(), depthAttachment.get()->get(), VK_IMAGE_LAYOUT_GENERAL,
                                      &depthClear, 1, &depthSubresourceRange);

        if (m_data.conditionInSecondaryCommandBuffer)
        {
            vkd.cmdEndConditionalRenderingEXT(commandBuffer2.get());
        }
        else
        {
            vkd.cmdEndConditionalRenderingEXT(commandBuffer.get());
        }

        vkd.endCommandBuffer(*commandBuffer2);
        if (m_data.secondaryCommandBufferNested)
        {
            vkd.cmdExecuteCommands(commandBuffer3.get(), 1, &commandBuffer2.get());
            vkd.endCommandBuffer(*commandBuffer3);
            vkd.cmdExecuteCommands(commandBuffer.get(), 1, &commandBuffer3.get());
        }
        else
        {
            vkd.cmdExecuteCommands(commandBuffer.get(), 1, &commandBuffer2.get());
        }
    }
    else
    {
        beginConditionalRendering(vkd, commandBuffer.get(), *conditionalBuffer, m_data);

        //clear to the correct colors
        vkd.cmdClearDepthStencilImage(commandBuffer.get(), depthAttachment.get()->get(), VK_IMAGE_LAYOUT_GENERAL,
                                      &depthClear, 1, &depthSubresourceRange);

        vkd.cmdEndConditionalRenderingEXT(commandBuffer.get());
    }
    copyImageToBuffer(vkd, commandBuffer.get(), depthAttachment.get()->get(), (*outDepthBuffer).get(), imageSize,
                      VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, 1, VK_IMAGE_ASPECT_DEPTH_BIT,
                      VK_IMAGE_ASPECT_DEPTH_BIT);

    endCommandBuffer(vkd, commandBuffer.get());
    submitCommandsAndWait(vkd, device, m_context.getUniversalQueue(), commandBuffer.get());

    invalidateAlloc(vkd, device, outDepthBufferAlloc);
    tcu::ConstPixelBufferAccess outDepth(mapVkFormat(depthFormat), imageDim, outDepthBufferData);

    //the clear should happen in every case
    if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare depth", "depth image comparison",
                                    expected, outDepth, tcu::Vec4(0.0), tcu::COMPARE_LOG_ON_ERROR))
        return tcu::TestStatus::fail("Depth image verification failed, check log for details");

    return tcu::TestStatus::pass("Pass");
}

struct GeneralCmdParams
{
    bool inverted;

    uint32_t getDisableValue()
    {
        return (inverted ? 1u : 0u);
    }

    VkConditionalRenderingFlagsEXT getConditionalRenderingFlags()
    {
        return static_cast<VkConditionalRenderingFlagsEXT>(inverted ? VK_CONDITIONAL_RENDERING_INVERTED_BIT_EXT : 0);
    }
};

void generalConditionalRenderingCheckSupport(Context &context, GeneralCmdParams)
{
    context.requireDeviceFunctionality("VK_EXT_conditional_rendering");
}

void pushConstantComputeShaders(vk::SourceCollections &dst, GeneralCmdParams)
{
    std::ostringstream comp;
    comp << "#version 460\n"
         << "layout (local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
         << "layout (set=0, binding=0) buffer OutBlock { uint value; } outBuffer;\n"
         << "layout (push_constant, std430) uniform PushConstantBlock { uint a; uint b; } pc;\n"
         << "void main(void) { outBuffer.value = ((pc.a == pc.b) ? 1u : 0u); }\n";
    dst.glslSources.add("comp") << glu::ComputeSource(comp.str());
}

tcu::TestStatus pushConstantTest(Context &context, GeneralCmdParams params)
{
    const auto ctx         = context.getContextCommonData();
    const auto descType    = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    const auto shaderStage = VK_SHADER_STAGE_COMPUTE_BIT;
    const auto bindPoint   = VK_PIPELINE_BIND_POINT_COMPUTE;

    const auto outBufferSize  = static_cast<VkDeviceSize>(sizeof(uint32_t));
    const auto outBufferUsage = (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    const auto outBufferInfo  = makeBufferCreateInfo(outBufferSize, outBufferUsage);
    BufferWithMemory outBuffer(ctx.vkd, ctx.device, ctx.allocator, outBufferInfo, MemoryRequirement::HostVisible);
    auto &outBufferAlloc = outBuffer.getAllocation();
    {
        memset(outBufferAlloc.getHostPtr(), 0, sizeof(uint32_t));
    }

    const auto crBufferSize  = static_cast<VkDeviceSize>(sizeof(uint32_t));
    const auto crBufferUsage = VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT;
    const auto crBufferInfo  = makeBufferCreateInfo(crBufferSize, crBufferUsage);
    BufferWithMemory crBuffer(ctx.vkd, ctx.device, ctx.allocator, crBufferInfo, MemoryRequirement::HostVisible);
    {
        auto &alloc = crBuffer.getAllocation();
        memset(alloc.getHostPtr(), params.getDisableValue(), sizeof(uint32_t));
    }

    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(descType);
    const auto descriptorPool =
        poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

    DescriptorSetLayoutBuilder setLayoutbuilder;
    setLayoutbuilder.addSingleBinding(descType, shaderStage);
    const auto setLayout     = setLayoutbuilder.build(ctx.vkd, ctx.device);
    const auto descriptorSet = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *setLayout);

    const auto binding = DescriptorSetUpdateBuilder::Location::binding;
    DescriptorSetUpdateBuilder setUpdateBuilder;
    const auto outBufferDescInfo = makeDescriptorBufferInfo(*outBuffer, 0ull, VK_WHOLE_SIZE);
    setUpdateBuilder.writeSingle(*descriptorSet, binding(0u), descType, &outBufferDescInfo);
    setUpdateBuilder.update(ctx.vkd, ctx.device);

    const auto pcSize         = DE_SIZEOF32(tcu::IVec2);
    const auto pcRange        = makePushConstantRange(shaderStage, 0u, pcSize);
    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout, &pcRange);

    const auto &binaries  = context.getBinaryCollection();
    const auto compShader = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp"));
    const auto pipeline   = makeComputePipeline(ctx.vkd, ctx.device, *pipelineLayout, *compShader);

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    const VkConditionalRenderingBeginInfoEXT conditionalRenderingBegin = {
        VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT,
        nullptr,
        *crBuffer,
        0ull,
        params.getConditionalRenderingFlags(),
    };

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    const tcu::IVec2 initialValues(3, 7); // Start with different values.
    ctx.vkd.cmdPushConstants(cmdBuffer, *pipelineLayout, shaderStage, 0u, DE_SIZEOF32(initialValues), &initialValues);
    ctx.vkd.cmdBeginConditionalRenderingEXT(cmdBuffer, &conditionalRenderingBegin);
    // Overwrite second value so it's the same as the first one.
    ctx.vkd.cmdPushConstants(cmdBuffer, *pipelineLayout, shaderStage, DE_SIZEOF32(uint32_t), DE_SIZEOF32(uint32_t),
                             &initialValues[0]);
    ctx.vkd.cmdEndConditionalRenderingEXT(cmdBuffer);
    ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
    ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *pipeline);
    ctx.vkd.cmdDispatch(cmdBuffer, 1u, 1u, 1u);
    {
        // Sync host reads.
        const auto barrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &barrier);
    }
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    invalidateAlloc(ctx.vkd, ctx.device, outBufferAlloc);

    // Make sure the output buffer contains a 1.
    const uint32_t expected = 1u;
    uint32_t result         = 0u;
    memcpy(&result, outBufferAlloc.getHostPtr(), sizeof(result));

    if (result != expected)
    {
        std::ostringstream msg;
        msg << "Unexpected value in output buffer: expected " << expected << " but found " << result;
        TCU_FAIL(msg.str());
    }

    return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus updateBufferTest(Context &context, GeneralCmdParams params)
{
    const auto ctx = context.getContextCommonData();

    const tcu::IVec2 initialValues(3, 7);
    const auto bufferSize  = static_cast<VkDeviceSize>(sizeof(initialValues));
    const auto bufferUsage = (VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const auto bufferInfo  = makeBufferCreateInfo(bufferSize, bufferUsage);
    BufferWithMemory buffer(ctx.vkd, ctx.device, ctx.allocator, bufferInfo, MemoryRequirement::HostVisible);
    auto &bufferAlloc = buffer.getAllocation();
    {
        memcpy(bufferAlloc.getHostPtr(), &initialValues, sizeof(initialValues));
    }

    const auto crBufferSize  = static_cast<VkDeviceSize>(sizeof(uint32_t));
    const auto crBufferUsage = VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT;
    const auto crBufferInfo  = makeBufferCreateInfo(crBufferSize, crBufferUsage);
    BufferWithMemory crBuffer(ctx.vkd, ctx.device, ctx.allocator, crBufferInfo, MemoryRequirement::HostVisible);
    {
        auto &alloc = crBuffer.getAllocation();
        memset(alloc.getHostPtr(), params.getDisableValue(), sizeof(uint32_t));
    }

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    const VkConditionalRenderingBeginInfoEXT conditionalRenderingBegin = {
        VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT,
        nullptr,
        *crBuffer,
        0ull,
        params.getConditionalRenderingFlags(),
    };

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    ctx.vkd.cmdBeginConditionalRenderingEXT(cmdBuffer, &conditionalRenderingBegin);
    // Overwrite second value so it's the same as the first one.
    const auto uintSize = static_cast<VkDeviceSize>(sizeof(uint32_t));
    ctx.vkd.cmdUpdateBuffer(cmdBuffer, *buffer, uintSize, uintSize, &initialValues[0]);
    ctx.vkd.cmdEndConditionalRenderingEXT(cmdBuffer);
    {
        // Sync host reads.
        const auto barrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &barrier);
    }
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    invalidateAlloc(ctx.vkd, ctx.device, bufferAlloc);

    // Make sure the output buffer contains two equal values.
    const tcu::IVec2 expected(initialValues.x(), initialValues.x());
    tcu::IVec2 result;
    memcpy(result.getPtr(), bufferAlloc.getHostPtr(), sizeof(result));

    if (result != expected)
    {
        std::ostringstream msg;
        msg << "Unexpected value in output buffer: expected " << expected << " but found " << result;
        TCU_FAIL(msg.str());
    }

    return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus fillBufferTest(Context &context, GeneralCmdParams params)
{
    const auto ctx = context.getContextCommonData();

    const tcu::UVec2 initialValues(3, 7);
    const auto bufferSize  = static_cast<VkDeviceSize>(sizeof(initialValues));
    const auto bufferUsage = (VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const auto bufferInfo  = makeBufferCreateInfo(bufferSize, bufferUsage);
    BufferWithMemory buffer(ctx.vkd, ctx.device, ctx.allocator, bufferInfo, MemoryRequirement::HostVisible);
    auto &bufferAlloc = buffer.getAllocation();
    {
        memcpy(bufferAlloc.getHostPtr(), &initialValues, sizeof(initialValues));
    }

    const auto crBufferSize  = static_cast<VkDeviceSize>(sizeof(uint32_t));
    const auto crBufferUsage = VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT;
    const auto crBufferInfo  = makeBufferCreateInfo(crBufferSize, crBufferUsage);
    BufferWithMemory crBuffer(ctx.vkd, ctx.device, ctx.allocator, crBufferInfo, MemoryRequirement::HostVisible);
    {
        auto &alloc = crBuffer.getAllocation();
        memset(alloc.getHostPtr(), params.getDisableValue(), sizeof(uint32_t));
    }

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    const VkConditionalRenderingBeginInfoEXT conditionalRenderingBegin = {
        VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT,
        nullptr,
        *crBuffer,
        0ull,
        params.getConditionalRenderingFlags(),
    };

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    ctx.vkd.cmdBeginConditionalRenderingEXT(cmdBuffer, &conditionalRenderingBegin);
    // Overwrite second value so it's the same as the first one.
    const auto uintSize = static_cast<VkDeviceSize>(sizeof(uint32_t));
    ctx.vkd.cmdFillBuffer(cmdBuffer, *buffer, uintSize, uintSize, initialValues.x());
    ctx.vkd.cmdEndConditionalRenderingEXT(cmdBuffer);
    {
        // Sync host reads.
        const auto barrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &barrier);
    }
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    invalidateAlloc(ctx.vkd, ctx.device, bufferAlloc);

    // Make sure the output buffer contains two equal values.
    const tcu::IVec2 expected(initialValues.x(), initialValues.x());
    tcu::IVec2 result;
    memcpy(result.getPtr(), bufferAlloc.getHostPtr(), sizeof(result));

    if (result != expected)
    {
        std::ostringstream msg;
        msg << "Unexpected value in output buffer: expected " << expected << " but found " << result;
        TCU_FAIL(msg.str());
    }

    return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus resolveImageTest(Context &context, GeneralCmdParams params)
{
    const auto ctx    = context.getContextCommonData();
    const auto format = VK_FORMAT_R8G8B8A8_UNORM;
    const tcu::IVec3 extent(2, 2, 1);
    const auto extentVk = makeExtent3D(extent);
    const auto colorSRR = makeDefaultImageSubresourceRange();
    const auto colorSRL = makeDefaultImageSubresourceLayers();

    const tcu::Vec4 srcColor(0.0f, 0.0f, 1.0f, 1.0f);
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);

    const auto usage = (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    const auto srcSamples = VK_SAMPLE_COUNT_4_BIT;
    const auto dstSamples = VK_SAMPLE_COUNT_1_BIT;

    const VkImageCreateInfo multiImageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        0u,
        VK_IMAGE_TYPE_2D,
        format,
        extentVk,
        1u,
        1u,
        srcSamples,
        VK_IMAGE_TILING_OPTIMAL,
        usage,
        VK_SHARING_MODE_EXCLUSIVE,
        0u,
        nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };

    ImageWithMemory multiImage(ctx.vkd, ctx.device, ctx.allocator, multiImageCreateInfo, MemoryRequirement::Any);

    VkImageCreateInfo singleImageCreateInfo = multiImageCreateInfo;
    singleImageCreateInfo.samples           = dstSamples;

    ImageWithMemory singleImage(ctx.vkd, ctx.device, ctx.allocator, singleImageCreateInfo, MemoryRequirement::Any);

    const auto tcuFormat = mapVkFormat(format);
    const auto bufferSize =
        static_cast<VkDeviceSize>(tcu::getPixelSize(tcuFormat) * extent.x() * extent.y() * extent.z());
    const auto bufferUsage      = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    const auto bufferCreateInfo = makeBufferCreateInfo(bufferSize, bufferUsage);
    BufferWithMemory buffer(ctx.vkd, ctx.device, ctx.allocator, bufferCreateInfo, MemoryRequirement::HostVisible);

    const auto crBufferSize  = static_cast<VkDeviceSize>(sizeof(uint32_t));
    const auto crBufferUsage = VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT;
    const auto crBufferInfo  = makeBufferCreateInfo(crBufferSize, crBufferUsage);
    BufferWithMemory crBuffer(ctx.vkd, ctx.device, ctx.allocator, crBufferInfo, MemoryRequirement::HostVisible);
    {
        auto &alloc = crBuffer.getAllocation();
        memset(alloc.getHostPtr(), params.getDisableValue(), sizeof(uint32_t));
    }

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    // Move both images to the general layout.
    {
        const std::vector<VkImageMemoryBarrier> barriers{
            makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                   *multiImage, colorSRR),
            makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                   *singleImage, colorSRR),
        };
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, barriers.data(), barriers.size());
    }
    // Clear multi image with the src color and the destination image with the clear color.
    {
        const auto srcColorVk   = makeClearValueColorVec4(srcColor);
        const auto clearColorVk = makeClearValueColorVec4(clearColor);

        ctx.vkd.cmdClearColorImage(cmdBuffer, *multiImage, VK_IMAGE_LAYOUT_GENERAL, &srcColorVk.color, 1u, &colorSRR);
        ctx.vkd.cmdClearColorImage(cmdBuffer, *singleImage, VK_IMAGE_LAYOUT_GENERAL, &clearColorVk.color, 1u,
                                   &colorSRR);
    }

    const VkConditionalRenderingBeginInfoEXT conditionalRenderingBegin = {
        VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT,
        nullptr,
        *crBuffer,
        0ull,
        params.getConditionalRenderingFlags(),
    };
    ctx.vkd.cmdBeginConditionalRenderingEXT(cmdBuffer, &conditionalRenderingBegin);
    {
        // Wait for the clear.
        const auto barrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT,
                                               (VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT));
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 &barrier);

        // Resolve image here.
        const VkOffset3D offset     = {0, 0, 0};
        const VkImageResolve region = {
            colorSRL, offset, colorSRL, offset, extentVk,
        };
        ctx.vkd.cmdResolveImage(cmdBuffer, *multiImage, VK_IMAGE_LAYOUT_GENERAL, *singleImage, VK_IMAGE_LAYOUT_GENERAL,
                                1u, &region);
    }
    ctx.vkd.cmdEndConditionalRenderingEXT(cmdBuffer);

    {
        // Copy resolved image to the buffer.
        const auto preCopyBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 &preCopyBarrier);

        const auto region = makeBufferImageCopy(extentVk, colorSRL);
        ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, *singleImage, VK_IMAGE_LAYOUT_GENERAL, *buffer, 1u, &region);

        const auto postCopyBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &postCopyBarrier);
    }

    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    tcu::TextureLevel referenceLevel(tcuFormat, extent.x(), extent.y(), extent.z());
    tcu::PixelBufferAccess reference = referenceLevel.getAccess();

    tcu::clear(reference, srcColor);

    auto &bufferAlloc = buffer.getAllocation();
    invalidateAlloc(ctx.vkd, ctx.device, bufferAlloc);

    tcu::ConstPixelBufferAccess result(tcuFormat, extent, bufferAlloc.getHostPtr());

    auto &log = context.getTestContext().getLog();
    const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f);
    if (!tcu::floatThresholdCompare(log, "Result", "", reference, result, threshold, tcu::COMPARE_LOG_ON_ERROR))
        TCU_FAIL("Unexpected results in output buffer; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus blitImageTest(Context &context, GeneralCmdParams params)
{
    const auto ctx    = context.getContextCommonData();
    const auto format = VK_FORMAT_R8G8B8A8_UNORM;
    const tcu::IVec3 extent(2, 2, 1);
    const auto extentVk = makeExtent3D(extent);
    const auto colorSRR = makeDefaultImageSubresourceRange();
    const auto colorSRL = makeDefaultImageSubresourceLayers();

    const tcu::Vec4 srcColor(0.0f, 0.0f, 1.0f, 1.0f);
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);

    const auto usage = (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    const VkImageCreateInfo imageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        0u,
        VK_IMAGE_TYPE_2D,
        format,
        extentVk,
        1u,
        1u,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        usage,
        VK_SHARING_MODE_EXCLUSIVE,
        0u,
        nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };

    ImageWithMemory srcImage(ctx.vkd, ctx.device, ctx.allocator, imageCreateInfo, MemoryRequirement::Any);
    ImageWithMemory dstImage(ctx.vkd, ctx.device, ctx.allocator, imageCreateInfo, MemoryRequirement::Any);

    const auto tcuFormat = mapVkFormat(format);
    const auto bufferSize =
        static_cast<VkDeviceSize>(tcu::getPixelSize(tcuFormat) * extent.x() * extent.y() * extent.z());
    const auto bufferUsage      = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    const auto bufferCreateInfo = makeBufferCreateInfo(bufferSize, bufferUsage);
    BufferWithMemory buffer(ctx.vkd, ctx.device, ctx.allocator, bufferCreateInfo, MemoryRequirement::HostVisible);

    const auto crBufferSize  = static_cast<VkDeviceSize>(sizeof(uint32_t));
    const auto crBufferUsage = VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT;
    const auto crBufferInfo  = makeBufferCreateInfo(crBufferSize, crBufferUsage);
    BufferWithMemory crBuffer(ctx.vkd, ctx.device, ctx.allocator, crBufferInfo, MemoryRequirement::HostVisible);
    {
        auto &alloc = crBuffer.getAllocation();
        memset(alloc.getHostPtr(), params.getDisableValue(), sizeof(uint32_t));
    }

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    // Move both images to the general layout.
    {
        const std::vector<VkImageMemoryBarrier> barriers{
            makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                   *srcImage, colorSRR),
            makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                   *dstImage, colorSRR),
        };
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, barriers.data(), barriers.size());
    }
    // Clear source image with the src color and the destination image with the clear color.
    {
        const auto srcColorVk   = makeClearValueColorVec4(srcColor);
        const auto clearColorVk = makeClearValueColorVec4(clearColor);

        ctx.vkd.cmdClearColorImage(cmdBuffer, *srcImage, VK_IMAGE_LAYOUT_GENERAL, &srcColorVk.color, 1u, &colorSRR);
        ctx.vkd.cmdClearColorImage(cmdBuffer, *dstImage, VK_IMAGE_LAYOUT_GENERAL, &clearColorVk.color, 1u, &colorSRR);
    }

    const VkConditionalRenderingBeginInfoEXT conditionalRenderingBegin = {
        VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT,
        nullptr,
        *crBuffer,
        0ull,
        params.getConditionalRenderingFlags(),
    };
    ctx.vkd.cmdBeginConditionalRenderingEXT(cmdBuffer, &conditionalRenderingBegin);
    {
        // Wait for the clear.
        const auto barrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT,
                                               (VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT));
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 &barrier);

        // Blit image here.
        const VkOffset3D blitOffsets[2] = {makeOffset3D(0, 0, 0), makeOffset3D(extent.x(), extent.y(), extent.z())};
        const VkImageBlit region        = {
            colorSRL,
            {blitOffsets[0], blitOffsets[1]},
            colorSRL,
            {blitOffsets[0], blitOffsets[1]},
        };
        ctx.vkd.cmdBlitImage(cmdBuffer, *srcImage, VK_IMAGE_LAYOUT_GENERAL, *dstImage, VK_IMAGE_LAYOUT_GENERAL, 1u,
                             &region, VK_FILTER_NEAREST);
    }
    ctx.vkd.cmdEndConditionalRenderingEXT(cmdBuffer);

    {
        // Copy dst image to the buffer.
        const auto preCopyBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 &preCopyBarrier);

        const auto region = makeBufferImageCopy(extentVk, colorSRL);
        ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, *dstImage, VK_IMAGE_LAYOUT_GENERAL, *buffer, 1u, &region);

        const auto postCopyBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &postCopyBarrier);
    }

    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    tcu::TextureLevel referenceLevel(tcuFormat, extent.x(), extent.y(), extent.z());
    tcu::PixelBufferAccess reference = referenceLevel.getAccess();

    tcu::clear(reference, srcColor);

    auto &bufferAlloc = buffer.getAllocation();
    invalidateAlloc(ctx.vkd, ctx.device, bufferAlloc);

    tcu::ConstPixelBufferAccess result(tcuFormat, extent, bufferAlloc.getHostPtr());

    auto &log = context.getTestContext().getLog();
    const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f);
    if (!tcu::floatThresholdCompare(log, "Result", "", reference, result, threshold, tcu::COMPARE_LOG_ON_ERROR))
        TCU_FAIL("Unexpected results in output buffer; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus copyImageTest(Context &context, GeneralCmdParams params)
{
    const auto ctx    = context.getContextCommonData();
    const auto format = VK_FORMAT_R8G8B8A8_UNORM;
    const tcu::IVec3 extent(2, 2, 1);
    const auto extentVk = makeExtent3D(extent);
    const auto colorSRR = makeDefaultImageSubresourceRange();
    const auto colorSRL = makeDefaultImageSubresourceLayers();

    const tcu::Vec4 srcColor(0.0f, 0.0f, 1.0f, 1.0f);
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);

    const auto usage = (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    const VkImageCreateInfo imageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        0u,
        VK_IMAGE_TYPE_2D,
        format,
        extentVk,
        1u,
        1u,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        usage,
        VK_SHARING_MODE_EXCLUSIVE,
        0u,
        nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };

    ImageWithMemory srcImage(ctx.vkd, ctx.device, ctx.allocator, imageCreateInfo, MemoryRequirement::Any);
    ImageWithMemory dstImage(ctx.vkd, ctx.device, ctx.allocator, imageCreateInfo, MemoryRequirement::Any);

    const auto tcuFormat = mapVkFormat(format);
    const auto bufferSize =
        static_cast<VkDeviceSize>(tcu::getPixelSize(tcuFormat) * extent.x() * extent.y() * extent.z());
    const auto bufferUsage      = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    const auto bufferCreateInfo = makeBufferCreateInfo(bufferSize, bufferUsage);
    BufferWithMemory buffer(ctx.vkd, ctx.device, ctx.allocator, bufferCreateInfo, MemoryRequirement::HostVisible);

    const auto crBufferSize  = static_cast<VkDeviceSize>(sizeof(uint32_t));
    const auto crBufferUsage = VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT;
    const auto crBufferInfo  = makeBufferCreateInfo(crBufferSize, crBufferUsage);
    BufferWithMemory crBuffer(ctx.vkd, ctx.device, ctx.allocator, crBufferInfo, MemoryRequirement::HostVisible);
    {
        auto &alloc = crBuffer.getAllocation();
        memset(alloc.getHostPtr(), params.getDisableValue(), sizeof(uint32_t));
    }

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    // Move both images to the general layout.
    {
        const std::vector<VkImageMemoryBarrier> barriers{
            makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                   *srcImage, colorSRR),
            makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                   *dstImage, colorSRR),
        };
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, barriers.data(), barriers.size());
    }
    // Clear source image with the src color and the destination image with the clear color.
    {
        const auto srcColorVk   = makeClearValueColorVec4(srcColor);
        const auto clearColorVk = makeClearValueColorVec4(clearColor);

        ctx.vkd.cmdClearColorImage(cmdBuffer, *srcImage, VK_IMAGE_LAYOUT_GENERAL, &srcColorVk.color, 1u, &colorSRR);
        ctx.vkd.cmdClearColorImage(cmdBuffer, *dstImage, VK_IMAGE_LAYOUT_GENERAL, &clearColorVk.color, 1u, &colorSRR);
    }

    const VkConditionalRenderingBeginInfoEXT conditionalRenderingBegin = {
        VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT,
        nullptr,
        *crBuffer,
        0ull,
        params.getConditionalRenderingFlags(),
    };
    ctx.vkd.cmdBeginConditionalRenderingEXT(cmdBuffer, &conditionalRenderingBegin);
    {
        // Wait for the clear.
        const auto barrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT,
                                               (VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT));
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 &barrier);

        // Copy image here.
        const VkOffset3D offset  = {0, 0, 0};
        const VkImageCopy region = {
            colorSRL, offset, colorSRL, offset, extentVk,
        };
        ctx.vkd.cmdCopyImage(cmdBuffer, *srcImage, VK_IMAGE_LAYOUT_GENERAL, *dstImage, VK_IMAGE_LAYOUT_GENERAL, 1u,
                             &region);
    }
    ctx.vkd.cmdEndConditionalRenderingEXT(cmdBuffer);

    {
        // Copy dst image to the buffer.
        const auto preCopyBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 &preCopyBarrier);

        const auto region = makeBufferImageCopy(extentVk, colorSRL);
        ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, *dstImage, VK_IMAGE_LAYOUT_GENERAL, *buffer, 1u, &region);

        const auto postCopyBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &postCopyBarrier);
    }

    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    tcu::TextureLevel referenceLevel(tcuFormat, extent.x(), extent.y(), extent.z());
    tcu::PixelBufferAccess reference = referenceLevel.getAccess();

    tcu::clear(reference, srcColor);

    auto &bufferAlloc = buffer.getAllocation();
    invalidateAlloc(ctx.vkd, ctx.device, bufferAlloc);

    tcu::ConstPixelBufferAccess result(tcuFormat, extent, bufferAlloc.getHostPtr());

    auto &log = context.getTestContext().getLog();
    const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f);
    if (!tcu::floatThresholdCompare(log, "Result", "", reference, result, threshold, tcu::COMPARE_LOG_ON_ERROR))
        TCU_FAIL("Unexpected results in output buffer; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus copyImageToBufferTest(Context &context, GeneralCmdParams params)
{
    const auto ctx    = context.getContextCommonData();
    const auto format = VK_FORMAT_R8G8B8A8_UNORM;
    const tcu::IVec3 extent(2, 2, 1);
    const auto extentVk = makeExtent3D(extent);
    const auto colorSRR = makeDefaultImageSubresourceRange();
    const auto colorSRL = makeDefaultImageSubresourceLayers();

    const tcu::Vec4 srcColor(0.0f, 0.0f, 1.0f, 1.0f);

    const auto usage = (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    const VkImageCreateInfo imageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        0u,
        VK_IMAGE_TYPE_2D,
        format,
        extentVk,
        1u,
        1u,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        usage,
        VK_SHARING_MODE_EXCLUSIVE,
        0u,
        nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };

    ImageWithMemory srcImage(ctx.vkd, ctx.device, ctx.allocator, imageCreateInfo, MemoryRequirement::Any);

    const auto tcuFormat = mapVkFormat(format);
    const auto bufferSize =
        static_cast<VkDeviceSize>(tcu::getPixelSize(tcuFormat) * extent.x() * extent.y() * extent.z());
    const auto bufferUsage      = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    const auto bufferCreateInfo = makeBufferCreateInfo(bufferSize, bufferUsage);
    BufferWithMemory buffer(ctx.vkd, ctx.device, ctx.allocator, bufferCreateInfo, MemoryRequirement::HostVisible);

    const auto crBufferSize  = static_cast<VkDeviceSize>(sizeof(uint32_t));
    const auto crBufferUsage = VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT;
    const auto crBufferInfo  = makeBufferCreateInfo(crBufferSize, crBufferUsage);
    BufferWithMemory crBuffer(ctx.vkd, ctx.device, ctx.allocator, crBufferInfo, MemoryRequirement::HostVisible);
    {
        auto &alloc = crBuffer.getAllocation();
        memset(alloc.getHostPtr(), params.getDisableValue(), sizeof(uint32_t));
    }

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    // Move image to the general layout.
    {
        const auto barrier = makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                    VK_IMAGE_LAYOUT_GENERAL, *srcImage, colorSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, &barrier);
    }
    // Clear src image with the src color.
    {
        const auto srcColorVk = makeClearValueColorVec4(srcColor);
        ctx.vkd.cmdClearColorImage(cmdBuffer, *srcImage, VK_IMAGE_LAYOUT_GENERAL, &srcColorVk.color, 1u, &colorSRR);
    }

    {
        // Copy dst image to the buffer.
        const auto preCopyBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 &preCopyBarrier);

        const VkConditionalRenderingBeginInfoEXT conditionalRenderingBegin = {
            VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT,
            nullptr,
            *crBuffer,
            0ull,
            params.getConditionalRenderingFlags(),
        };
        const auto region = makeBufferImageCopy(extentVk, colorSRL);
        ctx.vkd.cmdBeginConditionalRenderingEXT(cmdBuffer, &conditionalRenderingBegin);
        ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, *srcImage, VK_IMAGE_LAYOUT_GENERAL, *buffer, 1u, &region);
        ctx.vkd.cmdEndConditionalRenderingEXT(cmdBuffer);

        const auto postCopyBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &postCopyBarrier);
    }

    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    tcu::TextureLevel referenceLevel(tcuFormat, extent.x(), extent.y(), extent.z());
    tcu::PixelBufferAccess reference = referenceLevel.getAccess();

    tcu::clear(reference, srcColor);

    auto &bufferAlloc = buffer.getAllocation();
    invalidateAlloc(ctx.vkd, ctx.device, bufferAlloc);

    tcu::ConstPixelBufferAccess result(tcuFormat, extent, bufferAlloc.getHostPtr());

    auto &log = context.getTestContext().getLog();
    const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f);
    if (!tcu::floatThresholdCompare(log, "Result", "", reference, result, threshold, tcu::COMPARE_LOG_ON_ERROR))
        TCU_FAIL("Unexpected results in output buffer; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus clearColorImageTest(Context &context, GeneralCmdParams params)
{
    const auto ctx    = context.getContextCommonData();
    const auto format = VK_FORMAT_R8G8B8A8_UNORM;
    const tcu::IVec3 extent(2, 2, 1);
    const auto extentVk = makeExtent3D(extent);
    const auto colorSRR = makeDefaultImageSubresourceRange();
    const auto colorSRL = makeDefaultImageSubresourceLayers();

    const tcu::Vec4 srcColor(0.0f, 0.0f, 1.0f, 1.0f);

    const auto usage = (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    const VkImageCreateInfo imageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        0u,
        VK_IMAGE_TYPE_2D,
        format,
        extentVk,
        1u,
        1u,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        usage,
        VK_SHARING_MODE_EXCLUSIVE,
        0u,
        nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };

    ImageWithMemory srcImage(ctx.vkd, ctx.device, ctx.allocator, imageCreateInfo, MemoryRequirement::Any);

    const auto tcuFormat = mapVkFormat(format);
    const auto bufferSize =
        static_cast<VkDeviceSize>(tcu::getPixelSize(tcuFormat) * extent.x() * extent.y() * extent.z());
    const auto bufferUsage      = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    const auto bufferCreateInfo = makeBufferCreateInfo(bufferSize, bufferUsage);
    BufferWithMemory buffer(ctx.vkd, ctx.device, ctx.allocator, bufferCreateInfo, MemoryRequirement::HostVisible);

    const auto crBufferSize  = static_cast<VkDeviceSize>(sizeof(uint32_t));
    const auto crBufferUsage = VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT;
    const auto crBufferInfo  = makeBufferCreateInfo(crBufferSize, crBufferUsage);
    BufferWithMemory crBuffer(ctx.vkd, ctx.device, ctx.allocator, crBufferInfo, MemoryRequirement::HostVisible);
    {
        auto &alloc = crBuffer.getAllocation();
        memset(alloc.getHostPtr(), params.getDisableValue(), sizeof(uint32_t));
    }

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    // Move image to the general layout.
    {
        const auto barrier = makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                    VK_IMAGE_LAYOUT_GENERAL, *srcImage, colorSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, &barrier);
    }
    // Clear src image with the src color.
    {
        const VkConditionalRenderingBeginInfoEXT conditionalRenderingBegin = {
            VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT,
            nullptr,
            *crBuffer,
            0ull,
            params.getConditionalRenderingFlags(),
        };
        const auto srcColorVk = makeClearValueColorVec4(srcColor);
        ctx.vkd.cmdBeginConditionalRenderingEXT(cmdBuffer, &conditionalRenderingBegin);
        ctx.vkd.cmdClearColorImage(cmdBuffer, *srcImage, VK_IMAGE_LAYOUT_GENERAL, &srcColorVk.color, 1u, &colorSRR);
        ctx.vkd.cmdEndConditionalRenderingEXT(cmdBuffer);
    }

    {
        // Copy dst image to the buffer.
        const auto preCopyBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 &preCopyBarrier);

        const auto region = makeBufferImageCopy(extentVk, colorSRL);
        ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, *srcImage, VK_IMAGE_LAYOUT_GENERAL, *buffer, 1u, &region);

        const auto postCopyBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &postCopyBarrier);
    }

    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    tcu::TextureLevel referenceLevel(tcuFormat, extent.x(), extent.y(), extent.z());
    tcu::PixelBufferAccess reference = referenceLevel.getAccess();

    tcu::clear(reference, srcColor);

    auto &bufferAlloc = buffer.getAllocation();
    invalidateAlloc(ctx.vkd, ctx.device, bufferAlloc);

    tcu::ConstPixelBufferAccess result(tcuFormat, extent, bufferAlloc.getHostPtr());

    auto &log = context.getTestContext().getLog();
    const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f);
    if (!tcu::floatThresholdCompare(log, "Result", "", reference, result, threshold, tcu::COMPARE_LOG_ON_ERROR))
        TCU_FAIL("Unexpected results in output buffer; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus clearDepthStencilImageTest(Context &context, GeneralCmdParams params)
{
    const auto ctx    = context.getContextCommonData();
    const auto format = VK_FORMAT_D16_UNORM;
    const tcu::IVec3 extent(2, 2, 1);
    const auto extentVk = makeExtent3D(extent);
    const auto depthSRR = makeImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u);
    const auto depthSRL = makeImageSubresourceLayers(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, 1u);

    const auto usage = (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    const VkImageCreateInfo imageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        0u,
        VK_IMAGE_TYPE_2D,
        format,
        extentVk,
        1u,
        1u,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        usage,
        VK_SHARING_MODE_EXCLUSIVE,
        0u,
        nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };

    ImageWithMemory srcImage(ctx.vkd, ctx.device, ctx.allocator, imageCreateInfo, MemoryRequirement::Any);

    const auto tcuFormat = mapVkFormat(format);
    const auto bufferSize =
        static_cast<VkDeviceSize>(tcu::getPixelSize(tcuFormat) * extent.x() * extent.y() * extent.z());
    const auto bufferUsage      = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    const auto bufferCreateInfo = makeBufferCreateInfo(bufferSize, bufferUsage);
    BufferWithMemory buffer(ctx.vkd, ctx.device, ctx.allocator, bufferCreateInfo, MemoryRequirement::HostVisible);

    const auto crBufferSize  = static_cast<VkDeviceSize>(sizeof(uint32_t));
    const auto crBufferUsage = VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT;
    const auto crBufferInfo  = makeBufferCreateInfo(crBufferSize, crBufferUsage);
    BufferWithMemory crBuffer(ctx.vkd, ctx.device, ctx.allocator, crBufferInfo, MemoryRequirement::HostVisible);
    {
        auto &alloc = crBuffer.getAllocation();
        memset(alloc.getHostPtr(), params.getDisableValue(), sizeof(uint32_t));
    }

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    // Move image to the general layout.
    {
        const auto barrier = makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                    VK_IMAGE_LAYOUT_GENERAL, *srcImage, depthSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, &barrier);
    }
    // Clear src image with the src color.
    {
        const VkConditionalRenderingBeginInfoEXT conditionalRenderingBegin = {
            VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT,
            nullptr,
            *crBuffer,
            0ull,
            params.getConditionalRenderingFlags(),
        };
        const auto srcColorVk = makeClearDepthStencilValue(1.0f, 1);
        ctx.vkd.cmdBeginConditionalRenderingEXT(cmdBuffer, &conditionalRenderingBegin);
        ctx.vkd.cmdClearDepthStencilImage(cmdBuffer, *srcImage, VK_IMAGE_LAYOUT_GENERAL, &srcColorVk, 1u, &depthSRR);
        ctx.vkd.cmdEndConditionalRenderingEXT(cmdBuffer);
    }

    {
        // Copy dst image to the buffer.
        const auto preCopyBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 &preCopyBarrier);

        const auto region = makeBufferImageCopy(extentVk, depthSRL);
        ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, *srcImage, VK_IMAGE_LAYOUT_GENERAL, *buffer, 1u, &region);

        const auto postCopyBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &postCopyBarrier);
    }

    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    tcu::TextureLevel referenceLevel(tcuFormat, extent.x(), extent.y(), extent.z());
    tcu::PixelBufferAccess reference = referenceLevel.getAccess();

    tcu::clearDepth(reference, 1.0f);

    auto &bufferAlloc = buffer.getAllocation();
    invalidateAlloc(ctx.vkd, ctx.device, bufferAlloc);

    tcu::ConstPixelBufferAccess result(tcuFormat, extent, bufferAlloc.getHostPtr());

    auto &log             = context.getTestContext().getLog();
    const float threshold = 1.0f;
    if (!tcu::dsThresholdCompare(log, "Result", "", reference, result, threshold, tcu::COMPARE_LOG_ON_ERROR))
        TCU_FAIL("Unexpected results in output buffer; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus copyBufferToImageTest(Context &context, GeneralCmdParams params)
{
    const auto ctx    = context.getContextCommonData();
    const auto format = VK_FORMAT_R8G8B8A8_UNORM;
    const tcu::IVec3 extent(2, 2, 1);
    const auto extentVk = makeExtent3D(extent);
    const auto colorSRR = makeDefaultImageSubresourceRange();
    const auto colorSRL = makeDefaultImageSubresourceLayers();

    const tcu::Vec4 srcColor(0.0f, 0.0f, 1.0f, 1.0f);

    const auto usage = (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    const VkImageCreateInfo imageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        0u,
        VK_IMAGE_TYPE_2D,
        format,
        extentVk,
        1u,
        1u,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        usage,
        VK_SHARING_MODE_EXCLUSIVE,
        0u,
        nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };

    ImageWithMemory srcImage(ctx.vkd, ctx.device, ctx.allocator, imageCreateInfo, MemoryRequirement::Any);

    const auto tcuFormat = mapVkFormat(format);
    const auto bufferSize =
        static_cast<VkDeviceSize>(tcu::getPixelSize(tcuFormat) * extent.x() * extent.y() * extent.z());
    const auto bufferUsage      = (VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const auto bufferCreateInfo = makeBufferCreateInfo(bufferSize, bufferUsage);
    BufferWithMemory srcBuffer(ctx.vkd, ctx.device, ctx.allocator, bufferCreateInfo, MemoryRequirement::HostVisible);
    BufferWithMemory dstBuffer(ctx.vkd, ctx.device, ctx.allocator, bufferCreateInfo, MemoryRequirement::HostVisible);

    // Prepare source buffer.
    {
        auto &alloc = srcBuffer.getAllocation();
        tcu::PixelBufferAccess srcAccess(tcuFormat, extent, alloc.getHostPtr());
        tcu::clear(srcAccess, srcColor);
    }

    const auto crBufferSize  = static_cast<VkDeviceSize>(sizeof(uint32_t));
    const auto crBufferUsage = VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT;
    const auto crBufferInfo  = makeBufferCreateInfo(crBufferSize, crBufferUsage);
    BufferWithMemory crBuffer(ctx.vkd, ctx.device, ctx.allocator, crBufferInfo, MemoryRequirement::HostVisible);
    {
        auto &alloc = crBuffer.getAllocation();
        memset(alloc.getHostPtr(), params.getDisableValue(), sizeof(uint32_t));
    }

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    // Move image to the general layout.
    {
        const auto barrier = makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                    VK_IMAGE_LAYOUT_GENERAL, *srcImage, colorSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, &barrier);
    }
    // Copy buffer to image.
    {
        const VkConditionalRenderingBeginInfoEXT conditionalRenderingBegin = {
            VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT,
            nullptr,
            *crBuffer,
            0ull,
            params.getConditionalRenderingFlags(),
        };
        const VkBufferImageCopy region = {
            0ull, 0u, 0u, colorSRL, makeOffset3D(0, 0, 0), extentVk,
        };
        ctx.vkd.cmdBeginConditionalRenderingEXT(cmdBuffer, &conditionalRenderingBegin);
        ctx.vkd.cmdCopyBufferToImage(cmdBuffer, *srcBuffer, *srcImage, VK_IMAGE_LAYOUT_GENERAL, 1u, &region);
        ctx.vkd.cmdEndConditionalRenderingEXT(cmdBuffer);
    }

    {
        // Copy dst image to the buffer.
        const auto preCopyBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 &preCopyBarrier);

        const auto region = makeBufferImageCopy(extentVk, colorSRL);
        ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, *srcImage, VK_IMAGE_LAYOUT_GENERAL, *dstBuffer, 1u, &region);

        const auto postCopyBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &postCopyBarrier);
    }

    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    tcu::TextureLevel referenceLevel(tcuFormat, extent.x(), extent.y(), extent.z());
    tcu::PixelBufferAccess reference = referenceLevel.getAccess();

    tcu::clear(reference, srcColor);

    auto &bufferAlloc = dstBuffer.getAllocation();
    invalidateAlloc(ctx.vkd, ctx.device, bufferAlloc);

    tcu::ConstPixelBufferAccess result(tcuFormat, extent, bufferAlloc.getHostPtr());

    auto &log = context.getTestContext().getLog();
    const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f);
    if (!tcu::floatThresholdCompare(log, "Result", "", reference, result, threshold, tcu::COMPARE_LOG_ON_ERROR))
        TCU_FAIL("Unexpected results in output buffer; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus copyBufferTest(Context &context, GeneralCmdParams params)
{
    const auto ctx    = context.getContextCommonData();
    const auto format = VK_FORMAT_R8G8B8A8_UNORM;
    const tcu::IVec3 extent(2, 2, 1);

    const tcu::Vec4 srcColor(0.0f, 0.0f, 1.0f, 1.0f);

    const auto tcuFormat = mapVkFormat(format);
    const auto bufferSize =
        static_cast<VkDeviceSize>(tcu::getPixelSize(tcuFormat) * extent.x() * extent.y() * extent.z());
    const auto bufferUsage      = (VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const auto bufferCreateInfo = makeBufferCreateInfo(bufferSize, bufferUsage);
    BufferWithMemory srcBuffer(ctx.vkd, ctx.device, ctx.allocator, bufferCreateInfo, MemoryRequirement::HostVisible);
    BufferWithMemory dstBuffer(ctx.vkd, ctx.device, ctx.allocator, bufferCreateInfo, MemoryRequirement::HostVisible);

    // Prepare source buffer.
    {
        auto &alloc = srcBuffer.getAllocation();
        tcu::PixelBufferAccess srcAccess(tcuFormat, extent, alloc.getHostPtr());
        tcu::clear(srcAccess, srcColor);
    }

    const auto crBufferSize  = static_cast<VkDeviceSize>(sizeof(uint32_t));
    const auto crBufferUsage = VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT;
    const auto crBufferInfo  = makeBufferCreateInfo(crBufferSize, crBufferUsage);
    BufferWithMemory crBuffer(ctx.vkd, ctx.device, ctx.allocator, crBufferInfo, MemoryRequirement::HostVisible);
    {
        auto &alloc = crBuffer.getAllocation();
        memset(alloc.getHostPtr(), params.getDisableValue(), sizeof(uint32_t));
    }

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    // Copy buffers.
    {
        const VkConditionalRenderingBeginInfoEXT conditionalRenderingBegin = {
            VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT,
            nullptr,
            *crBuffer,
            0ull,
            params.getConditionalRenderingFlags(),
        };
        const VkBufferCopy region = {
            0ull,
            0ull,
            bufferSize,
        };
        ctx.vkd.cmdBeginConditionalRenderingEXT(cmdBuffer, &conditionalRenderingBegin);
        ctx.vkd.cmdCopyBuffer(cmdBuffer, *srcBuffer, *dstBuffer, 1u, &region);
        ctx.vkd.cmdEndConditionalRenderingEXT(cmdBuffer);
    }

    {
        // Sync with host reads.
        const auto postCopyBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &postCopyBarrier);
    }

    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    tcu::TextureLevel referenceLevel(tcuFormat, extent.x(), extent.y(), extent.z());
    tcu::PixelBufferAccess reference = referenceLevel.getAccess();

    tcu::clear(reference, srcColor);

    auto &bufferAlloc = dstBuffer.getAllocation();
    invalidateAlloc(ctx.vkd, ctx.device, bufferAlloc);

    tcu::ConstPixelBufferAccess result(tcuFormat, extent, bufferAlloc.getHostPtr());

    auto &log = context.getTestContext().getLog();
    const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f);
    if (!tcu::floatThresholdCompare(log, "Result", "", reference, result, threshold, tcu::COMPARE_LOG_ON_ERROR))
        TCU_FAIL("Unexpected results in output buffer; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

enum class GraphicsBindType
{
    DESCRIPTOR_SETS = 0,
    INDEX_BUFFER,
    PIPELINE,
    SHADERS,
    VERTEX_BUFFER,
};

struct GraphicsBindParams : public GeneralCmdParams
{
    GraphicsBindType graphicsBindType;
};

void graphicsBindCheckSupport(Context &context, GraphicsBindParams params)
{
    generalConditionalRenderingCheckSupport(context, static_cast<GeneralCmdParams>(params));

    if (params.graphicsBindType == GraphicsBindType::SHADERS)
        context.requireDeviceFunctionality("VK_EXT_shader_object");
}

void graphicsBindInitPrograms(vk::SourceCollections &dst, GraphicsBindParams)
{
    std::ostringstream vert;
    vert << "#version 460\n"
         << "layout (location=0) in vec4 inPos;\n"
         << "void main (void) { gl_Position = inPos; }\n";
    dst.glslSources.add("vert") << glu::VertexSource(vert.str());

    const std::string colorLetters("AB");
    for (size_t i = 0; i < colorLetters.size(); ++i)
    {
        std::ostringstream frag;
        frag << "#version 460\n"
             << "layout (location=0) out vec4 outColor;\n"
             << "layout (set=0, binding=0, std430) readonly buffer ColorBlock { vec4 colorA; vec4 colorB; } inColor;\n"
             << "void main (void) { outColor = inColor.color" << colorLetters.at(i) << "; }\n";
        dst.glslSources.add("frag" + std::to_string(i)) << glu::FragmentSource(frag.str());
    }
}

tcu::TestStatus graphicsBindTest(Context &context, GraphicsBindParams params)
{
    const auto ctx = context.getContextCommonData();

    // Color buffers.
    struct ColorBlock
    {
        tcu::Vec4 colorA;
        tcu::Vec4 colorB;
    };

    ColorBlock goodColors{tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), tcu::Vec4(0.0f, 1.0f, 1.0f, 1.0f)};
    ColorBlock badColors{tcu::Vec4(1.0f, 0.0f, 1.0f, 1.0f), tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f)};

    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);

    const auto colorsBufferSize       = static_cast<VkDeviceSize>(sizeof(ColorBlock));
    const auto colorsBufferUsage      = static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    const auto colorsBufferCreateInfo = makeBufferCreateInfo(colorsBufferSize, colorsBufferUsage);

    BufferWithMemory goodColorsBuffer(ctx.vkd, ctx.device, ctx.allocator, colorsBufferCreateInfo,
                                      MemoryRequirement::HostVisible);
    {
        auto &alloc = goodColorsBuffer.getAllocation();
        memcpy(alloc.getHostPtr(), &goodColors, sizeof(goodColors));
    }

    BufferWithMemory badColorsBuffer(ctx.vkd, ctx.device, ctx.allocator, colorsBufferCreateInfo,
                                     MemoryRequirement::HostVisible);
    {
        auto &alloc = badColorsBuffer.getAllocation();
        memcpy(alloc.getHostPtr(), &badColors, sizeof(badColors));
    }

    // Vertex buffers.
    const std::vector<tcu::Vec4> goodVertices{
        tcu::Vec4(-1.0, -1.0, 0.0f, 1.0f),
        tcu::Vec4(-1.0, 1.0, 0.0f, 1.0f),
        tcu::Vec4(1.0, -1.0, 0.0f, 1.0f),
        tcu::Vec4(1.0, 1.0, 0.0f, 1.0f),
    };
    const std::vector<tcu::Vec4> badVertices(goodVertices.size(), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

    const auto vertexBufferSize       = static_cast<VkDeviceSize>(de::dataSize(goodVertices));
    const auto vertexBufferUsage      = static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    const auto vertexBufferCreateInfo = makeBufferCreateInfo(vertexBufferSize, vertexBufferUsage);

    BufferWithMemory goodVertexBuffer(ctx.vkd, ctx.device, ctx.allocator, vertexBufferCreateInfo,
                                      MemoryRequirement::HostVisible);
    {
        auto &alloc = goodVertexBuffer.getAllocation();
        memcpy(alloc.getHostPtr(), de::dataOrNull(goodVertices), de::dataSize(goodVertices));
    }

    BufferWithMemory badVertexBuffer(ctx.vkd, ctx.device, ctx.allocator, vertexBufferCreateInfo,
                                     MemoryRequirement::HostVisible);
    {
        auto &alloc = badVertexBuffer.getAllocation();
        memcpy(alloc.getHostPtr(), de::dataOrNull(badVertices), de::dataSize(badVertices));
    }

    // Index buffers.
    const std::vector<uint32_t> badIndices(goodVertices.size(), 0u);
    std::vector<uint32_t> goodIndices = badIndices;
    std::iota(begin(goodIndices), end(goodIndices), 0u);

    const auto indexBufferSize       = static_cast<VkDeviceSize>(de::dataSize(goodIndices));
    const auto indexBufferUsage      = static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    const auto indexBufferCreateInfo = makeBufferCreateInfo(indexBufferSize, indexBufferUsage);

    BufferWithMemory goodIndexBuffer(ctx.vkd, ctx.device, ctx.allocator, indexBufferCreateInfo,
                                     MemoryRequirement::HostVisible);
    {
        auto &alloc = goodIndexBuffer.getAllocation();
        memcpy(alloc.getHostPtr(), de::dataOrNull(goodIndices), de::dataSize(goodIndices));
    }

    BufferWithMemory badIndexBuffer(ctx.vkd, ctx.device, ctx.allocator, indexBufferCreateInfo,
                                    MemoryRequirement::HostVisible);
    {
        auto &alloc = badIndexBuffer.getAllocation();
        memcpy(alloc.getHostPtr(), de::dataOrNull(badIndices), de::dataSize(badIndices));
    }

    // Fragment shaders.
    const auto &binaries = context.getBinaryCollection();
    ShaderWrapper vertShader(ctx.vkd, ctx.device, binaries.get("vert"));
    const std::vector<ShaderWrapper> fragShaders{
        ShaderWrapper(ctx.vkd, ctx.device, binaries.get("frag0")),
        ShaderWrapper(ctx.vkd, ctx.device, binaries.get("frag1")),
    };

    // Descriptors.
    const auto descType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    const auto descStages = VK_SHADER_STAGE_FRAGMENT_BIT;

    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(descType, 2u);
    const auto descriptorPool =
        poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 2u);

    DescriptorSetLayoutBuilder setLayoutBuilder;
    setLayoutBuilder.addSingleBinding(descType, descStages);
    const auto setLayout = setLayoutBuilder.build(ctx.vkd, ctx.device);

    const auto constructionType = (params.graphicsBindType == GraphicsBindType::SHADERS ?
                                       PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_UNLINKED_SPIRV :
                                       PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC);
    PipelineLayoutWrapper pipelineLayout(constructionType, ctx.vkd, ctx.device, *setLayout);

    const auto goodDescriptorSet = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *setLayout);
    const auto badDescriptorSet  = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *setLayout);

    const auto binding = DescriptorSetUpdateBuilder::Location::binding;
    DescriptorSetUpdateBuilder setUpdateBuilder;
    const auto goodDescBufferInfo = makeDescriptorBufferInfo(*goodColorsBuffer, 0ull, VK_WHOLE_SIZE);
    const auto badDescBufferInfo  = makeDescriptorBufferInfo(*badColorsBuffer, 0ull, VK_WHOLE_SIZE);
    setUpdateBuilder.writeSingle(*goodDescriptorSet, binding(0u), descType, &goodDescBufferInfo);
    setUpdateBuilder.writeSingle(*badDescriptorSet, binding(0u), descType, &badDescBufferInfo);
    setUpdateBuilder.update(ctx.vkd, ctx.device);

    // Render pass and framebuffer.
    const tcu::IVec3 extent(2, 2, 1);
    const auto extentVk        = makeExtent3D(extent);
    const auto format          = VK_FORMAT_R8G8B8A8_UNORM;
    const auto attachmentUsage = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    ImageWithBuffer attachment(ctx.vkd, ctx.device, ctx.allocator, extentVk, format, attachmentUsage, VK_IMAGE_TYPE_2D);

    const std::vector<VkViewport> viewports(1u, makeViewport(extent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(extent));

    RenderPassWrapper renderPass(constructionType, ctx.vkd, ctx.device, format);
    renderPass.createFramebuffer(ctx.vkd, ctx.device, attachment.getImage(), attachment.getImageView(), extentVk.width,
                                 extentVk.height);

    // Pipelines.
    using GraphicsPipelineWrapperPtr = std::unique_ptr<GraphicsPipelineWrapper>;
    std::vector<GraphicsPipelineWrapperPtr> pipelines;
    pipelines.reserve(2u);

    for (const auto &fragShader : fragShaders)
    {
        pipelines.emplace_back(new GraphicsPipelineWrapper(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device,
                                                           context.getDeviceExtensions(), constructionType));
        auto &pipeline = *pipelines.back();

        pipeline.setDefaultVertexInputState(true)
            .setDefaultDepthStencilState()
            .setDefaultMultisampleState()
            .setDefaultColorBlendState()
            .setDefaultRasterizationState()
            .setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
            .setupVertexInputState()
            .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, renderPass.get(), 0u, vertShader)
            .setupFragmentShaderState(pipelineLayout, renderPass.get(), 0u, fragShader)
            .setupFragmentOutputState(renderPass.get(), 0u)
            .buildPipeline();
    }

    // Conditional rendering buffer.
    const auto crBufferSize  = static_cast<VkDeviceSize>(sizeof(uint32_t));
    const auto crBufferUsage = VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT;
    const auto crBufferInfo  = makeBufferCreateInfo(crBufferSize, crBufferUsage);
    BufferWithMemory crBuffer(ctx.vkd, ctx.device, ctx.allocator, crBufferInfo, MemoryRequirement::HostVisible);
    {
        auto &alloc = crBuffer.getAllocation();
        memset(alloc.getHostPtr(), params.getDisableValue(), sizeof(uint32_t));
    }

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    const auto bindPoint          = VK_PIPELINE_BIND_POINT_GRAPHICS;
    const auto vertexBufferOffset = static_cast<VkDeviceSize>(0);

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    renderPass.begin(ctx.vkd, cmdBuffer, scissors.at(0u), clearColor);

    // Fixed block.
    {
        const auto initialSet =
            (params.graphicsBindType == GraphicsBindType::DESCRIPTOR_SETS ? badDescriptorSet.get() :
                                                                            goodDescriptorSet.get());
        ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, pipelineLayout.get(), 0u, 1u, &initialSet, 0u, nullptr);

        const auto initialIndexBuffer =
            (params.graphicsBindType == GraphicsBindType::INDEX_BUFFER ? badIndexBuffer.get() : goodIndexBuffer.get());
        ctx.vkd.cmdBindIndexBuffer(cmdBuffer, initialIndexBuffer, 0ull, VK_INDEX_TYPE_UINT32);

        const auto initialVertexBuffer =
            (params.graphicsBindType == GraphicsBindType::VERTEX_BUFFER ? badVertexBuffer.get() :
                                                                          goodVertexBuffer.get());
        ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &initialVertexBuffer, &vertexBufferOffset);

        pipelines.front()->bind(cmdBuffer);
    }

    // Conditional rendering block.
    {
        const VkConditionalRenderingBeginInfoEXT conditionalRenderingBegin = {
            VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT,
            nullptr,
            *crBuffer,
            0ull,
            params.getConditionalRenderingFlags(),
        };
        ctx.vkd.cmdBeginConditionalRenderingEXT(cmdBuffer, &conditionalRenderingBegin);
    }

    if (params.graphicsBindType == GraphicsBindType::DESCRIPTOR_SETS)
        ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, pipelineLayout.get(), 0u, 1u, &goodDescriptorSet.get(), 0u,
                                      nullptr);
    else if (params.graphicsBindType == GraphicsBindType::INDEX_BUFFER)
        ctx.vkd.cmdBindIndexBuffer(cmdBuffer, *goodIndexBuffer, 0ull, VK_INDEX_TYPE_UINT32);
    else if (params.graphicsBindType == GraphicsBindType::PIPELINE)
        pipelines.back()->bind(cmdBuffer);
    else if (params.graphicsBindType == GraphicsBindType::SHADERS)
    {
        const auto shaderStage = VK_SHADER_STAGE_FRAGMENT_BIT;
        const auto shaderObj   = pipelines.back()->getShader(shaderStage);
        ctx.vkd.cmdBindShadersEXT(cmdBuffer, 1u, &shaderStage, &shaderObj);
    }
    else if (params.graphicsBindType == GraphicsBindType::VERTEX_BUFFER)
        ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &goodVertexBuffer.get(), &vertexBufferOffset);
    else
        DE_ASSERT(false);

    ctx.vkd.cmdEndConditionalRenderingEXT(cmdBuffer);

    // Draw, finish and copy image.
    ctx.vkd.cmdDraw(cmdBuffer, de::sizeU32(goodVertices), 1u, 0u, 0u);
    renderPass.end(ctx.vkd, cmdBuffer);
    copyImageToBuffer(ctx.vkd, cmdBuffer, attachment.getImage(), attachment.getBuffer(), extent.swizzle(0, 1));

    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    const auto tcuFormat = mapVkFormat(format);
    tcu::TextureLevel refLevel(tcuFormat, extent.x(), extent.y(), extent.z());
    tcu::PixelBufferAccess reference = refLevel.getAccess();

    const bool fragShaderSwitch =
        (params.graphicsBindType == GraphicsBindType::PIPELINE || params.graphicsBindType == GraphicsBindType::SHADERS);
    tcu::clear(reference, (fragShaderSwitch ? goodColors.colorB : goodColors.colorA));

    invalidateAlloc(ctx.vkd, ctx.device, attachment.getBufferAllocation());
    tcu::ConstPixelBufferAccess result(tcuFormat, extent, attachment.getBufferAllocation().getHostPtr());

    auto &log = context.getTestContext().getLog();
    const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f);

    if (!tcu::floatThresholdCompare(log, "Result", "", reference, result, threshold, tcu::COMPARE_LOG_ON_ERROR))
        TCU_FAIL("Unexpected results in color buffer; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

} // anonymous namespace

// operations that ignore conditions
ConditionalIgnoreTests::ConditionalIgnoreTests(tcu::TestContext &testCtx) : TestCaseGroup(testCtx, "conditional_ignore")
{
}

ConditionalIgnoreTests::~ConditionalIgnoreTests(void)
{
}

void ConditionalIgnoreTests::init(void)
{
    for (int conditionNdx = 0; conditionNdx < DE_LENGTH_OF_ARRAY(conditional::s_testsData); conditionNdx++)
    {
        const ConditionalData &conditionData = conditional::s_testsData[conditionNdx];

        if (conditionData.clearInRenderPass)
            continue;

        // tests that some clear operations always happen
        addChild(new ConditionalIgnoreClearColorTestCase(
            m_testCtx, std::string("clear_color_") + de::toString(conditionData).c_str(), conditionData));

        // tests that some clear operations always happen
        addChild(new ConditionalIgnoreClearDepthTestCase(
            m_testCtx, std::string("clear_depth_") + de::toString(conditionData).c_str(), conditionData));
    }

    for (const bool inverted : {false, true})
    {
        const auto suffix = (inverted ? std::string("_inverted") : std::string());
        const GeneralCmdParams params{inverted};

        {
            const auto testName = "push_constant" + suffix;
            addFunctionCaseWithPrograms(this, testName, generalConditionalRenderingCheckSupport,
                                        pushConstantComputeShaders, pushConstantTest, params);
        }
        {
            const auto testName = "update_buffer" + suffix;
            addFunctionCase(this, testName, generalConditionalRenderingCheckSupport, updateBufferTest, params);
        }
        {
            const auto testName = "fill_buffer" + suffix;
            addFunctionCase(this, testName, generalConditionalRenderingCheckSupport, fillBufferTest, params);
        }
        {
            const auto testName = "resolve_image" + suffix;
            addFunctionCase(this, testName, generalConditionalRenderingCheckSupport, resolveImageTest, params);
        }
        {
            const auto testName = "blit_image" + suffix;
            addFunctionCase(this, testName, generalConditionalRenderingCheckSupport, blitImageTest, params);
        }
        {
            const auto testName = "copy_image" + suffix;
            addFunctionCase(this, testName, generalConditionalRenderingCheckSupport, copyImageTest, params);
        }
        {
            const auto testName = "copy_image_to_buffer" + suffix;
            addFunctionCase(this, testName, generalConditionalRenderingCheckSupport, copyImageToBufferTest, params);
        }
        {
            const auto testName = "clear_color_image" + suffix;
            addFunctionCase(this, testName, generalConditionalRenderingCheckSupport, clearColorImageTest, params);
        }
        {
            const auto testName = "clear_depth_stencil_image" + suffix;
            addFunctionCase(this, testName, generalConditionalRenderingCheckSupport, clearDepthStencilImageTest,
                            params);
        }
        {
            const auto testName = "copy_buffer_to_image" + suffix;
            addFunctionCase(this, testName, generalConditionalRenderingCheckSupport, copyBufferToImageTest, params);
        }
        {
            const auto testName = "copy_buffer" + suffix;
            addFunctionCase(this, testName, generalConditionalRenderingCheckSupport, copyBufferTest, params);
        }
        {
            const auto testName = "bind_descriptor_sets" + suffix;
            const GraphicsBindParams graphicsParams{{params.inverted}, GraphicsBindType::DESCRIPTOR_SETS};
            addFunctionCaseWithPrograms(this, testName, graphicsBindCheckSupport, graphicsBindInitPrograms,
                                        graphicsBindTest, graphicsParams);
        }
        {
            const auto testName = "bind_index_buffer" + suffix;
            const GraphicsBindParams graphicsParams{{params.inverted}, GraphicsBindType::INDEX_BUFFER};
            addFunctionCaseWithPrograms(this, testName, graphicsBindCheckSupport, graphicsBindInitPrograms,
                                        graphicsBindTest, graphicsParams);
        }
        {
            const auto testName = "bind_pipeline" + suffix;
            const GraphicsBindParams graphicsParams{{params.inverted}, GraphicsBindType::PIPELINE};
            addFunctionCaseWithPrograms(this, testName, graphicsBindCheckSupport, graphicsBindInitPrograms,
                                        graphicsBindTest, graphicsParams);
        }
        {
            const auto testName = "bind_shaders" + suffix;
            const GraphicsBindParams graphicsParams{{params.inverted}, GraphicsBindType::SHADERS};
            addFunctionCaseWithPrograms(this, testName, graphicsBindCheckSupport, graphicsBindInitPrograms,
                                        graphicsBindTest, graphicsParams);
        }
        {
            const auto testName = "bind_vertex_buffers" + suffix;
            const GraphicsBindParams graphicsParams{{params.inverted}, GraphicsBindType::VERTEX_BUFFER};
            addFunctionCaseWithPrograms(this, testName, graphicsBindCheckSupport, graphicsBindInitPrograms,
                                        graphicsBindTest, graphicsParams);
        }
    }
}

} // namespace conditional
} // namespace vkt
