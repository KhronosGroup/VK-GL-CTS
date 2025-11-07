/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 The Khronos Group Inc.
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
 * \brief Robust Index Buffer Access Tests
 *//*--------------------------------------------------------------------*/

#include "vktRobustnessIndexAccessTests.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vktRobustnessUtil.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "tcuTestLog.hpp"
#include "deMath.h"
#include "tcuVectorUtil.hpp"
#include "deUniquePtr.hpp"
#include <algorithm>
#include <numeric>
#include <tuple>
#include <vector>

namespace vkt::robustness
{

using namespace vk;

#ifndef CTS_USES_VULKANSC
typedef de::MovePtr<vk::DeviceDriver> DeviceDriverPtr;
#else
typedef de::MovePtr<vk::DeviceDriverSC, vk::DeinitDeviceDeleter> DeviceDriverPtr;
#endif // CTS_USES_VULKANSC

enum TestMode
{
    TM_DRAW_INDEXED = 0,
    TM_DRAW_INDEXED_INDIRECT,
    TM_DRAW_INDEXED_INDIRECT_COUNT,
    TM_DRAW_MULTI_INDEXED,
};

enum OOTypes
{
    OO_NONE,
    OO_INDEX,
    OO_SIZE,
    OO_WHOLE_SIZE
};

struct TestParams
{
    TestMode mode                 = TM_DRAW_INDEXED;
    OOTypes ooType                = OO_NONE;
    uint32_t leadingCount         = 0;
    uint32_t robustnessVersion    = 2;
    bool useDeviceAddressCommands = false;
};

// helper function that executes cmdCopyImageToBuffer or cmdCopyImageToMemoryKHR
static void copyImageToMemory(const DeviceInterface &vk, VkCommandBuffer cmdBuffer, VkImage image,
                              const VkExtent3D &imageExtent, VkBuffer buffer, VkDeviceAddress bufferAddress,
                              [[maybe_unused]] VkDeviceSize bufferSize)
{
    const VkImageSubresourceLayers colorSL = makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
    if (bufferAddress == 0)
    {
        const VkBufferImageCopy copyRegion = makeBufferImageCopy(imageExtent, colorSL);
        vk.cmdCopyImageToBuffer(cmdBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1u, &copyRegion);
    }

#ifndef CTS_USES_VULKANSC
    if (bufferAddress)
    {
        VkDeviceAddressRangeKHR addressRange{bufferAddress, bufferSize};
        VkDeviceMemoryImageCopyKHR region = initVulkanStructure();
        region.addressRange               = addressRange;
        region.imageSubresource           = colorSL;
        region.imageLayout                = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        region.imageOffset                = makeOffset3D(0, 0, 0);
        region.imageExtent                = imageExtent;

        VkCopyDeviceMemoryImageInfoKHR copyMemoryInfo = initVulkanStructure();
        copyMemoryInfo.image                          = image;
        copyMemoryInfo.regionCount                    = 1u;
        copyMemoryInfo.pRegions                       = &region;
        vk.cmdCopyImageToMemoryKHR(cmdBuffer, &copyMemoryInfo);
    }
#endif // CTS_USES_VULKANSC
}

class DrawIndexedInstance : public vkt::TestInstance
{
public:
    DrawIndexedInstance(Context &context,
#ifdef CTS_USES_VULKANSC
                        de::MovePtr<CustomInstance> customInstance,
#endif // CTS_USES_VULKANSC
                        Move<VkDevice> device, DeviceDriverPtr deviceDriver, const TestParams &testParams);

    virtual ~DrawIndexedInstance(void) = default;

    virtual tcu::TestStatus iterate(void);

protected:
#ifdef CTS_USES_VULKANSC
    de::MovePtr<CustomInstance> m_customInstance;
#endif // CTS_USES_VULKANSC
    Move<VkDevice> m_device;
    DeviceDriverPtr m_deviceDriver;
    const TestParams m_params;
};

DrawIndexedInstance::DrawIndexedInstance(Context &context,
#ifdef CTS_USES_VULKANSC
                                         de::MovePtr<CustomInstance> customInstance,
#endif // CTS_USES_VULKANSC
                                         Move<VkDevice> device, DeviceDriverPtr deviceDriver,
                                         const TestParams &testParams)
    : vkt::TestInstance(context)
#ifdef CTS_USES_VULKANSC
    , m_customInstance(customInstance)
#endif
    , m_device(device)
    , m_deviceDriver(deviceDriver)
    , m_params(testParams)
{
}

tcu::TestStatus DrawIndexedInstance::iterate(void)
{
    const DeviceInterface &vk       = *m_deviceDriver;
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    const auto &vki                 = m_context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice =
        chooseDevice(vki, m_context.getInstance(), m_context.getTestContext().getCommandLine());
    SimpleAllocator memAlloc(vk, *m_device, getPhysicalDeviceMemoryProperties(vki, physicalDevice));

    // this is testsed - first index in index buffer is outside of bounds
    const uint32_t oobFirstIndex = std::numeric_limits<uint32_t>::max() - 100;

    const VkFormat colorFormat{VK_FORMAT_R8G8B8A8_UNORM};
    const tcu::UVec2 renderSize{16};
    const std::vector<VkViewport> viewports{makeViewport(renderSize)};
    const std::vector<VkRect2D> scissors{makeRect2D(renderSize)};

    VkBufferUsageFlags commonUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (m_params.useDeviceAddressCommands)
        commonUsage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    MemoryRequirement memReq = m_params.useDeviceAddressCommands ?
                                   MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress :
                                   MemoryRequirement::HostVisible;

    // create vertex buffer
    const std::vector<float> vertices{
        0.0f, -0.8f, 0.0f, 1.0f, 0.0f,  0.8f,  0.0f, 1.0f, 0.8f,  -0.8f, 0.0f, 1.0f,
        0.8f, 0.8f,  0.0f, 1.0f, -0.8f, -0.8f, 0.0f, 1.0f, -0.8f, 0.8f,  0.0f, 1.0f,
    };
    VkDeviceSize vertexBufferSize = vertices.size() * sizeof(float);
    const auto vertexBufferInfo =
        makeBufferCreateInfo(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | commonUsage);
    BufferWithMemory vertexBuffer(vk, *m_device, memAlloc, vertexBufferInfo, memReq);
    deMemcpy(vertexBuffer.getAllocation().getHostPtr(), vertices.data(), vertices.size() * sizeof(float));
    flushAlloc(vk, *m_device, vertexBuffer.getAllocation());

    // create index buffer for 6 points
    // 4--0--2
    // |  |  |
    // 5--1--3
    const std::vector<uint32_t> index = {0, 1, 2, 3, 4, 5};
    VkDeviceSize indexBufferSize      = index.size() * sizeof(uint32_t);
    const auto indexBufferInfo = makeBufferCreateInfo(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | commonUsage);
    BufferWithMemory indexBuffer(vk, *m_device, memAlloc, indexBufferInfo, memReq);
    deMemcpy(indexBuffer.getAllocation().getHostPtr(), index.data(), index.size() * sizeof(uint32_t));
    flushAlloc(vk, *m_device, indexBuffer.getAllocation());

    // create indirect buffer
    const vk::VkDrawIndexedIndirectCommand drawIndirectCommand{
        (uint32_t)index.size(), // indexCount
        1u,                     // instanceCount
        oobFirstIndex,          // firstIndex
        0u,                     // vertexOffset
        0u,                     // firstInstance
    };
    VkDeviceSize indirectBufferSize = sizeof(drawIndirectCommand);
    const auto indirectBufferInfo =
        makeBufferCreateInfo(indirectBufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | commonUsage);
    BufferWithMemory indirectBuffer(vk, *m_device, memAlloc, indirectBufferInfo, memReq);
    if ((m_params.mode == TM_DRAW_INDEXED_INDIRECT) || (m_params.mode == TM_DRAW_INDEXED_INDIRECT_COUNT))
    {
        deMemcpy(indirectBuffer.getAllocation().getHostPtr(), &drawIndirectCommand, sizeof(drawIndirectCommand));
        flushAlloc(vk, *m_device, indirectBuffer.getAllocation());
    }

    // create indirect count buffer
    VkDeviceSize indirectCountBufferSize = sizeof(uint32_t);
    const auto indirectCountBufferInfo =
        makeBufferCreateInfo(indirectCountBufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | commonUsage);
    BufferWithMemory indirectCountBuffer(vk, *m_device, memAlloc, indirectCountBufferInfo, memReq);
    if (m_params.mode == TM_DRAW_INDEXED_INDIRECT_COUNT)
    {
        *(reinterpret_cast<uint32_t *>(indirectCountBuffer.getAllocation().getHostPtr())) = 1;
        flushAlloc(vk, *m_device, indirectCountBuffer.getAllocation());
    }

    // create output buffer that will be used to read rendered image
    const VkDeviceSize outputBufferSize = renderSize.x() * renderSize.y() * tcu::getPixelSize(mapVkFormat(colorFormat));
    const auto outputBufferInfo =
        makeBufferCreateInfo(outputBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | commonUsage);
    BufferWithMemory outputBuffer(vk, *m_device, memAlloc, outputBufferInfo, memReq);
    VkDeviceAddress outputBufferAddress = 0ull;

#ifndef CTS_USES_VULKANSC
    VkDeviceAddress vertexBufferAddress        = 0ull;
    VkDeviceAddress indexBufferAddress         = 0ull;
    VkDeviceAddress indirectBufferAddress      = 0ull;
    VkDeviceAddress indirectCountBufferAddress = 0ull;

    if (m_params.useDeviceAddressCommands)
    {
        vertexBufferAddress        = getBufferDeviceAddress(vk, *m_device, *vertexBuffer);
        indexBufferAddress         = getBufferDeviceAddress(vk, *m_device, *indexBuffer);
        indirectBufferAddress      = getBufferDeviceAddress(vk, *m_device, *indirectBuffer);
        indirectCountBufferAddress = getBufferDeviceAddress(vk, *m_device, *indirectCountBuffer);
        outputBufferAddress        = getBufferDeviceAddress(vk, *m_device, *outputBuffer);
    }
#endif

    // create color buffer
    VkExtent3D imageExtent = makeExtent3D(renderSize.x(), renderSize.y(), 1u);
    const VkImageCreateInfo imageCreateInfo{
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                                   // VkStructureType sType;
        nullptr,                                                               // const void* pNext;
        0u,                                                                    // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                                                      // VkImageType imageType;
        colorFormat,                                                           // VkFormat format;
        imageExtent,                                                           // VkExtent3D extent;
        1u,                                                                    // uint32_t mipLevels;
        1u,                                                                    // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,                                                 // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,                                               // VkImageTiling tiling;
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,                                             // VkSharingMode sharingMode;
        0u,                                                                    // uint32_t queueFamilyIndexCount;
        nullptr,                                                               // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,                                             // VkImageLayout initialLayout;
    };
    const VkImageSubresourceRange colorSRR = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    ImageWithMemory colorImage(vk, *m_device, memAlloc, imageCreateInfo, MemoryRequirement::Any);
    auto colorImageView = makeImageView(vk, *m_device, colorImage.get(), VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSRR);

    // create shader modules, renderpass, framebuffer and pipeline
    auto vertShaderModule = createShaderModule(vk, *m_device, m_context.getBinaryCollection().get("vert"));
    auto fragShaderModule = createShaderModule(vk, *m_device, m_context.getBinaryCollection().get("frag"));
    auto renderPass       = makeRenderPass(vk, *m_device, colorFormat);
    auto pipelineLayout   = makePipelineLayout(vk, *m_device, VK_NULL_HANDLE);
    auto framebuffer = makeFramebuffer(vk, *m_device, *renderPass, *colorImageView, renderSize.x(), renderSize.y());
    Move<VkPipeline> graphicsPipeline = makeGraphicsPipeline(
        vk, *m_device, *pipelineLayout, *vertShaderModule, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
        *fragShaderModule, *renderPass, viewports, scissors, VK_PRIMITIVE_TOPOLOGY_POINT_LIST);

    Move<VkCommandPool> cmdPool =
        createCommandPool(vk, *m_device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
    vk::Move<vk::VkCommandBuffer> cmdBuffer =
        allocateCommandBuffer(vk, *m_device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    beginCommandBuffer(vk, *cmdBuffer);

    // transition colorbuffer layout
    VkImageMemoryBarrier imageBarrier =
        makeImageMemoryBarrier(0u, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, colorImage.get(), colorSRR);
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u, 0u,
                          0u, 0u, 0u, 1u, &imageBarrier);

    const VkRect2D renderArea = makeRect2D(0, 0, renderSize.x(), renderSize.y());
    beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, renderArea, tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);

    if (!m_params.useDeviceAddressCommands)
    {
        const VkDeviceSize vBuffOffset = 0;
        vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vertexBuffer.get(), &vBuffOffset);
        vk.cmdBindIndexBuffer(*cmdBuffer, indexBuffer.get(), 0, VK_INDEX_TYPE_UINT32);

        // we will draw all points at index 0
        if (m_params.mode == TM_DRAW_INDEXED)
            vk.cmdDrawIndexed(*cmdBuffer, (uint32_t)index.size(), 1, oobFirstIndex, 0, 0);
        else if (m_params.mode == TM_DRAW_INDEXED_INDIRECT)
            vk.cmdDrawIndexedIndirect(*cmdBuffer, indirectBuffer.get(), 0, 1, 0);
        else if (m_params.mode == TM_DRAW_INDEXED_INDIRECT_COUNT)
            vk.cmdDrawIndexedIndirectCount(*cmdBuffer, indirectBuffer.get(), 0, indirectCountBuffer.get(), 0, 1,
                                           sizeof(VkDrawIndexedIndirectCommand));
        else if (m_params.mode == TM_DRAW_MULTI_INDEXED)
        {
#ifndef CTS_USES_VULKANSC
            VkMultiDrawIndexedInfoEXT indexInfo[]{
                {oobFirstIndex, 3, 0},
                {oobFirstIndex - 3, 3, 0},
            };
            vk.cmdDrawMultiIndexedEXT(*cmdBuffer, 2, indexInfo, 1, 0, sizeof(VkMultiDrawIndexedInfoEXT), nullptr);
#endif // CTS_USES_VULKANSC
        }
    }
#ifndef CTS_USES_VULKANSC
    if (m_params.useDeviceAddressCommands)
    {
        // use different valid addressFlags in some cases to test them
        VkAddressCommandFlagsKHR addressFlags = VK_ADDRESS_COMMAND_NEVER_ALIASES_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT;
        if (m_params.mode == TM_DRAW_INDEXED)
            addressFlags |= VK_ADDRESS_COMMAND_NEVER_ALIASES_STORAGE_BUFFER_BIT_KHR;
        if (m_params.mode == TM_DRAW_INDEXED_INDIRECT)
            addressFlags |= VK_ADDRESS_COMMAND_FULLY_BOUND_BIT_KHR;

        VkBindVertexBuffer3InfoKHR vertexBuffer3Info = initVulkanStructure();
        vertexBuffer3Info.addressRange               = {vertexBufferAddress, vertexBufferSize, 4u * sizeof(float)};
        vertexBuffer3Info.addressFlags               = addressFlags;
        vk.cmdBindVertexBuffers3KHR(*cmdBuffer, 0, 1, &vertexBuffer3Info);

        VkBindIndexBuffer3InfoKHR bindIndexBuffer3Info = initVulkanStructure();
        bindIndexBuffer3Info.addressRange              = {indexBufferAddress, indexBufferSize};
        bindIndexBuffer3Info.indexType                 = VK_INDEX_TYPE_UINT32;
        bindIndexBuffer3Info.addressFlags              = addressFlags;
        vk.cmdBindIndexBuffer3KHR(*cmdBuffer, &bindIndexBuffer3Info);

        // we will draw all points at index 0
        if (m_params.mode == TM_DRAW_INDEXED)
            vk.cmdDrawIndexed(*cmdBuffer, (uint32_t)index.size(), 1, oobFirstIndex, 0, 0);
        else if (m_params.mode == TM_DRAW_INDEXED_INDIRECT)
        {
            VkDrawIndirect2InfoKHR drawIndirect2Info = initVulkanStructure();
            drawIndirect2Info.addressRange           = {indirectBufferAddress, indirectBufferSize, 0};
            drawIndirect2Info.addressFlags           = addressFlags;
            drawIndirect2Info.drawCount              = 1u;

            vk.cmdDrawIndexedIndirect2KHR(*cmdBuffer, &drawIndirect2Info);
        }
        else if (m_params.mode == TM_DRAW_INDEXED_INDIRECT_COUNT)
        {
            VkDrawIndirectCount2InfoKHR drawIndirectCount2Info = initVulkanStructure();
            drawIndirectCount2Info.addressRange                = {indirectBufferAddress, indirectBufferSize,
                                                                  sizeof(VkDrawIndexedIndirectCommand)};
            drawIndirectCount2Info.countAddressRange           = {indirectCountBufferAddress, indirectCountBufferSize};
            drawIndirectCount2Info.maxDrawCount                = 1;

            vk.cmdDrawIndexedIndirectCount2KHR(*cmdBuffer, &drawIndirectCount2Info);
        }
        else if (m_params.mode == TM_DRAW_MULTI_INDEXED)
            DE_ASSERT(false);
    }
#endif // CTS_USES_VULKANSC

    endRenderPass(vk, *cmdBuffer);

    // wait till data is transfered to image
    imageBarrier = makeImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, colorImage.get(), colorSRR);
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
                          0u, 0u, 0u, 0u, 1u, &imageBarrier);

    // read back color image
    copyImageToMemory(vk, *cmdBuffer, *colorImage, imageExtent, *outputBuffer, outputBufferAddress, outputBufferSize);

    auto bufferBarrier = makeBufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
                                                 outputBuffer.get(), 0u, VK_WHOLE_SIZE);

    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, 0u, 1u,
                          &bufferBarrier, 0u, 0u);

    endCommandBuffer(vk, *cmdBuffer);

    VkQueue queue;
    vk.getDeviceQueue(*m_device, queueFamilyIndex, 0, &queue);
    submitCommandsAndWait(vk, *m_device, queue, *cmdBuffer);

    // for robustBufferAccess (the original feature) OOB access will return undefined value;
    // we can only expect that above drawing will be executed without errors (we can't expect any specific result)
    if (m_params.robustnessVersion < 2u)
        return tcu::TestStatus::pass("Pass");

    // get output buffer
    invalidateAlloc(vk, *m_device, outputBuffer.getAllocation());
    const tcu::TextureFormat resultFormat = mapVkFormat(colorFormat);
    tcu::ConstPixelBufferAccess outputAccess(resultFormat, renderSize.x(), renderSize.y(), 1u,
                                             outputBuffer.getAllocation().getHostPtr());

    // for VK_EXT_robustness2 OOB access should return 0 and we can verify
    // that single fragment is drawn in the middle-top part of the image
    tcu::UVec4 expectedValue(51, 255, 127, 255);
    bool fragmentFound = false;

    for (uint32_t x = 0u; x < renderSize.x(); ++x)
        for (uint32_t y = 0u; y < renderSize.y(); ++y)
        {
            tcu::UVec4 pixel = outputAccess.getPixelUint(x, y, 0);

            if (tcu::boolAll(tcu::lessThan(tcu::absDiff(pixel, expectedValue), tcu::UVec4(2))))
            {
                if (fragmentFound)
                {
                    m_context.getTestContext().getLog()
                        << tcu::TestLog::Message << "Expected single fragment with: " << expectedValue
                        << " color, got more, second at " << tcu::UVec2(x, y) << tcu::TestLog::EndMessage
                        << tcu::TestLog::Image("Result", "Result", outputAccess);
                    return tcu::TestStatus::fail("Fail");
                }
                else if ((y < 3) && (x > 5) && (x < 10))
                    fragmentFound = true;
                else
                {
                    m_context.getTestContext().getLog()
                        << tcu::TestLog::Message
                        << "Expected fragment in the middle-top of the image, got at: " << tcu::UVec2(x, y)
                        << tcu::TestLog::EndMessage << tcu::TestLog::Image("Result", "Result", outputAccess);
                    return tcu::TestStatus::fail("Fail");
                }
            }
        }

    if (fragmentFound)
        return tcu::TestStatus::pass("Pass");
    return tcu::TestStatus::fail("Fail");
}

class DrawIndexedTestCase : public vkt::TestCase
{
public:
    DrawIndexedTestCase(tcu::TestContext &testContext, const std::string &name, const TestParams &params);

    virtual ~DrawIndexedTestCase(void) = default;

    void checkSupport(Context &context) const override;
    TestInstance *createInstance(Context &context) const override;
    void initPrograms(SourceCollections &programCollection) const override;

protected:
    void createDeviceAndDriver(Context &context,
#ifdef CTS_USES_VULKANSC
                               de::MovePtr<CustomInstance> &customInstance,
#endif // CTS_USES_VULKANSC
                               Move<VkDevice> &device, DeviceDriverPtr &driver) const;
    const TestParams m_params;
};

DrawIndexedTestCase::DrawIndexedTestCase(tcu::TestContext &testContext, const std::string &name,
                                         const TestParams &params)

    : vkt::TestCase(testContext, name)
    , m_params(params)
{
}

void DrawIndexedTestCase::checkSupport(Context &context) const
{
    if (context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") &&
        !context.getDeviceFeatures().robustBufferAccess)
        TCU_THROW(NotSupportedError,
                  "VK_KHR_portability_subset: robustBufferAccess not supported by this implementation");

    if (m_params.mode == TestMode::TM_DRAW_INDEXED_INDIRECT_COUNT)
        context.requireDeviceFunctionality("VK_KHR_draw_indirect_count");
    if (m_params.mode == TestMode::TM_DRAW_MULTI_INDEXED)
        context.requireDeviceFunctionality("VK_EXT_multi_draw");
    if (m_params.robustnessVersion == 2)
    {
        if (!context.isDeviceFunctionalitySupported("VK_KHR_robustness2") &&
            !context.isDeviceFunctionalitySupported("VK_EXT_robustness2"))

            TCU_THROW(NotSupportedError, "VK_KHR_robustness2 and VK_EXT_robustness2 are not supported");

        const auto &vki           = context.getInstanceInterface();
        const auto physicalDevice = context.getPhysicalDevice();

        VkPhysicalDeviceRobustness2FeaturesEXT robustness2Features = initVulkanStructure();
        VkPhysicalDeviceFeatures2 features2                        = initVulkanStructure(&robustness2Features);

        vki.getPhysicalDeviceFeatures2(physicalDevice, &features2);

        if (!robustness2Features.robustBufferAccess2)
            TCU_THROW(NotSupportedError, "robustBufferAccess2 not supported");
    }
    if (m_params.useDeviceAddressCommands)
        context.requireDeviceFunctionality("VK_KHR_device_address_commands");
}

void DrawIndexedTestCase::createDeviceAndDriver(Context &context,
#ifdef CTS_USES_VULKANSC
                                                de::MovePtr<CustomInstance> &customInstance,
#endif // CTS_USES_VULKANSC
                                                Move<VkDevice> &device, DeviceDriverPtr &driver) const
{
    VkPhysicalDeviceFeatures2 features2   = initVulkanStructure();
    features2.features.robustBufferAccess = true;

    void **nextPtr = &features2.pNext;

#ifndef CTS_USES_VULKANSC
    VkPhysicalDeviceMultiDrawFeaturesEXT multiDrawFeatures = initVulkanStructure();
    if (m_params.mode == TestMode::TM_DRAW_MULTI_INDEXED)
    {
        multiDrawFeatures.multiDraw = true;
        addToChainVulkanStructure(&nextPtr, multiDrawFeatures);
    }

    VkPhysicalDeviceDeviceAddressCommandsFeaturesKHR addressCommandsFeatures = initVulkanStructure();
    if (m_params.useDeviceAddressCommands)
    {
        addressCommandsFeatures.deviceAddressCommands = true;
        addToChainVulkanStructure(&nextPtr, addressCommandsFeatures);
    }
#endif // CTS_USES_VULKANSC

    VkPhysicalDeviceRobustness2FeaturesEXT robustness2Features = initVulkanStructure();
    if (m_params.robustnessVersion > 1u)
    {
        robustness2Features.robustBufferAccess2 = true;
        addToChainVulkanStructure(&nextPtr, robustness2Features);
    }

    uint32_t apiVersion                               = context.getUsedApiVersion();
    VkPhysicalDeviceVulkan12Features vulkan12Features = initVulkanStructure();
    if ((m_params.mode == TestMode::TM_DRAW_INDEXED_INDIRECT_COUNT) && (apiVersion > VK_MAKE_API_VERSION(0, 1, 1, 0)))
    {
        vulkan12Features.drawIndirectCount = true;
        addToChainVulkanStructure(&nextPtr, vulkan12Features);
    }

#ifndef CTS_USES_VULKANSC
    device = createRobustBufferAccessDevice(context, &features2);
    driver = DeviceDriverPtr(new DeviceDriver(context.getPlatformInterface(), context.getInstance(), *device,
                                              context.getUsedApiVersion(), context.getTestContext().getCommandLine()));
#else
    customInstance = de::MovePtr<CustomInstance>(new CustomInstance(createCustomInstanceFromContext(context)));
    device         = createRobustBufferAccessDevice(context, *customInstance, &features2);
    driver         = DeviceDriverPtr(new DeviceDriverSC(context.getPlatformInterface(), *customInstance, *device,
                                                        context.getTestContext().getCommandLine(),
                                                        context.getResourceInterface(), context.getDeviceVulkanSC10Properties(),
                                                        context.getDeviceProperties(), context.getUsedApiVersion()),
                                     vk::DeinitDeviceDeleter(context.getResourceInterface().get(), *device));
#endif // CTS_USES_VULKANSC
}

TestInstance *DrawIndexedTestCase::createInstance(Context &context) const
{
    Move<VkDevice> device;
    DeviceDriverPtr deviceDriver;

#ifndef CTS_USES_VULKANSC
    createDeviceAndDriver(context, device, deviceDriver);
#else
    de::MovePtr<CustomInstance> customInstance;
    createDeviceAndDriver(context, customInstance, device, deviceDriver);
#endif // CTS_USES_VULKANSC

    return new DrawIndexedInstance(context,
#ifdef CTS_USES_VULKANSC
                                   customInstance,
#endif // CTS_USES_VULKANSC
                                   device, deviceDriver, m_params);
}

void DrawIndexedTestCase::initPrograms(SourceCollections &sourceCollections) const
{
    std::string vertexSource("#version 450\n"
                             "layout(location = 0) in vec4 inPosition;\n"
                             "void main(void)\n"
                             "{\n"
                             "\tgl_Position = inPosition;\n"
                             "\tgl_PointSize = 1.0;\n"
                             "}\n");
    sourceCollections.glslSources.add("vert") << glu::VertexSource(vertexSource);

    std::string fragmentSource("#version 450\n"
                               "precision highp float;\n"
                               "layout(location = 0) out vec4 fragColor;\n"
                               "void main (void)\n"
                               "{\n"
                               "\tfragColor = vec4(0.2, 1.0, 0.5, 1.0);\n"
                               "}\n");

    sourceCollections.glslSources.add("frag") << glu::FragmentSource(fragmentSource);
}

class BindIndexBuffer2Instance : public vkt::TestInstance
{
public:
    BindIndexBuffer2Instance(Context &c,
#ifdef CTS_USES_VULKANSC
                             de::MovePtr<CustomInstance> customInstance,
#endif // CTS_USES_VULKANSC
                             Move<VkDevice> device, DeviceDriverPtr driver, const TestParams &params);
    virtual ~BindIndexBuffer2Instance(void) = default;

    virtual tcu::TestStatus iterate(void) override;

protected:
#ifdef CTS_USES_VULKANSC
    const de::MovePtr<CustomInstance> m_customInstance;
#endif // CTS_USES_VULKANSC
    const Move<VkDevice> m_device;
    const DeviceDriverPtr m_driver;
    const TestParams m_params;
    VkPhysicalDevice m_physDevice;
    SimpleAllocator m_allocator;

protected:
    inline const DeviceInterface &getDeviceInterface() const
    {
        return *m_driver;
    }
    inline VkDevice getDevice() const
    {
        return *m_device;
    }
    inline VkPhysicalDevice getPhysicalDevice() const
    {
        return m_physDevice;
    }
    inline Allocator &getAllocator()
    {
        return m_allocator;
    }
    VkQueue getQueue() const;
};

BindIndexBuffer2Instance::BindIndexBuffer2Instance(Context &c,
#ifdef CTS_USES_VULKANSC
                                                   de::MovePtr<CustomInstance> customInstance,
#endif
                                                   Move<VkDevice> device, DeviceDriverPtr driver,
                                                   const TestParams &params)
    : vkt::TestInstance(c)
#ifdef CTS_USES_VULKANSC
    , m_customInstance(customInstance)
#endif
    , m_device(device)
    , m_driver(driver)
    , m_params(params)
    , m_physDevice(chooseDevice(c.getInstanceInterface(), c.getInstance(), c.getTestContext().getCommandLine()))
    , m_allocator(getDeviceInterface(), getDevice(),
                  getPhysicalDeviceMemoryProperties(c.getInstanceInterface(), m_physDevice))
{
}

VkQueue BindIndexBuffer2Instance::getQueue() const
{
    VkQueue queue = VK_NULL_HANDLE;
    getDeviceInterface().getDeviceQueue(getDevice(), m_context.getUniversalQueueFamilyIndex(), 0, &queue);
    return queue;
}

class BindIndexBuffer2TestCase : public DrawIndexedTestCase
{
public:
    BindIndexBuffer2TestCase(tcu::TestContext &testContext, const std::string &name, const TestParams &params);
    ~BindIndexBuffer2TestCase(void) = default;

    void checkSupport(Context &context) const override;
    TestInstance *createInstance(Context &context) const override;
    void initPrograms(SourceCollections &programs) const override;
};

BindIndexBuffer2TestCase::BindIndexBuffer2TestCase(tcu::TestContext &testContext, const std::string &name,
                                                   const TestParams &params)
    : DrawIndexedTestCase(testContext, name, params)
{
}

#ifdef CTS_USES_VULKANSC
#define DEPENDENT_MAINTENANCE_5_EXTENSION_NAME "VK_KHR_maintenance5"
#else
#define DEPENDENT_MAINTENANCE_5_EXTENSION_NAME VK_KHR_MAINTENANCE_5_EXTENSION_NAME
#endif

void BindIndexBuffer2TestCase::checkSupport(Context &context) const
{
    DrawIndexedTestCase::checkSupport(context);
    context.requireDeviceFunctionality(DEPENDENT_MAINTENANCE_5_EXTENSION_NAME);
}

void BindIndexBuffer2TestCase::initPrograms(SourceCollections &programs) const
{
    const std::string vertexSource("#version 450\n"
                                   "layout(location = 0) in vec4 inPosition;\n"
                                   "void main(void) {\n"
                                   "   gl_Position = inPosition;\n"
                                   "   gl_PointSize = 1.0;\n"
                                   "}\n");
    programs.glslSources.add("vert") << glu::VertexSource(vertexSource);

    const std::string fragmentSource("#version 450\n"
                                     "layout(location = 0) out vec4 fragColor;\n"
                                     "void main (void) {\n"
                                     "   fragColor = vec4(1.0);\n"
                                     "}\n");
    programs.glslSources.add("frag") << glu::FragmentSource(fragmentSource);
}

TestInstance *BindIndexBuffer2TestCase::createInstance(Context &context) const
{
    Move<VkDevice> device;
    DeviceDriverPtr deviceDriver;

#ifndef CTS_USES_VULKANSC
    createDeviceAndDriver(context, device, deviceDriver);
#else
    de::MovePtr<CustomInstance> customInstance =
        de::MovePtr<CustomInstance>(new CustomInstance(createCustomInstanceFromContext(context)));
    createDeviceAndDriver(context, customInstance, device, deviceDriver);
#endif // CTS_USES_VULKANSC

    return new BindIndexBuffer2Instance(context,
#ifdef CTS_USES_VULKANSC
                                        customInstance,
#endif // CTS_USES_VULKANSC
                                        device, deviceDriver, m_params);
}

tcu::TestStatus BindIndexBuffer2Instance::iterate(void)
{
    const DeviceInterface &vk     = this->getDeviceInterface();
    const VkDevice device         = this->getDevice();
    Allocator &allocator          = this->getAllocator();
    const VkQueue queue           = this->getQueue();
    const uint32_t queueFamilyIdx = m_context.getUniversalQueueFamilyIndex();
    tcu::TestLog &log             = m_context.getTestContext().getLog();

    const VkFormat colorFormat{VK_FORMAT_R32G32B32A32_SFLOAT};
    const tcu::UVec2 renderSize{64, 64};
    const std::vector<VkViewport> viewports{makeViewport(renderSize)};
    const std::vector<VkRect2D> scissors{makeRect2D(renderSize)};

    // build vertices data
    std::vector<tcu::Vec4> vertices{// first triangle in 2nd quarter, it should not be drawn
                                    {-1.0f, 0.1f, 0.0f, 1.0f},
                                    {-1.0f, 1.0f, 0.0f, 1.0f},
                                    {-0.1f, 0.1f, 0.0f, 1.0f},

                                    // second triangle in 2nd quarter, it should not be drawn
                                    {-0.1f, 0.1f, 0.0f, 1.0f},
                                    {-1.0f, 1.0f, 0.0f, 1.0f},
                                    {-0.1f, 1.0f, 0.0f, 1.0f},

                                    // first triangle in 3rd quarter, it must be drawn
                                    {0.0f, -1.0f, 0.0f, 1.0f},
                                    {-1.0f, -1.0f, 0.0f, 1.0f},
                                    {-1.0f, 0.0f, 0.0f, 1.0f},

                                    // second triangle in 3rd quarter if robustness works as expected,
                                    // otherwise will be drawn in 1st quarter as well
                                    {0.0f, -1.0f, 0.0f, 1.0f},
                                    {-1.0f, 0.0f, 0.0f, 1.0f},
                                    {1.0f, 1.0f, 0.0f, 1.0f}};

    const VkBufferUsageFlags commonUsage =
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        (m_params.useDeviceAddressCommands ? VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT : 0);

    MemoryRequirement memReq = m_params.useDeviceAddressCommands ?
                                   MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress :
                                   MemoryRequirement::HostVisible;

    // create vertex buffer
    const VkDeviceSize vertexBufferSize = vertices.size() * sizeof(tcu::Vec4);
    const VkBufferCreateInfo vertexBufferInfo =
        makeBufferCreateInfo(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | commonUsage);
    BufferWithMemory vertexBuffer(vk, device, allocator, vertexBufferInfo, memReq);
    deMemcpy(vertexBuffer.getAllocation().getHostPtr(), vertices.data(), (size_t)vertexBufferSize);

    // build index data
    const uint32_t leadingCount = m_params.leadingCount;
    std::vector<uint32_t> indices(leadingCount * 6 + 6);
    for (uint32_t j = 0; j < leadingCount; ++j)
        for (uint32_t k = 0; k < 6; ++k)
        {
            indices[j * 6 + k] = k;
        }
    std::iota(std::next(indices.begin(), (leadingCount * 6)), indices.end(), 6u);

    const uint32_t firstIndex        = 0;
    const uint32_t indexCount        = 6;
    const VkDeviceSize bindingOffset = leadingCount * 6 * sizeof(uint32_t);
    VkDeviceSize indexBindingSize    = 6 * sizeof(uint32_t);
    VkDeviceSize indexBufferSize     = indices.size() * sizeof(uint32_t);
    switch (m_params.ooType)
    {
    case OOTypes::OO_NONE:
        // default values already set
        break;
    case OOTypes::OO_INDEX:
        indices.back() = 33; // out of range index
        break;
    case OOTypes::OO_SIZE:
        indexBindingSize = 5 * sizeof(uint32_t);
        break;
    case OOTypes::OO_WHOLE_SIZE:
        indexBindingSize = VK_WHOLE_SIZE;
        indexBufferSize  = (indices.size() - 1) * sizeof(uint32_t);
        break;
    }

    // create index buffer
    const VkBufferCreateInfo indexBufferInfo =
        makeBufferCreateInfo(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | commonUsage);
    BufferWithMemory indexBuffer(vk, device, allocator, indexBufferInfo, memReq);
    deMemcpy(indexBuffer.getAllocation().getHostPtr(), indices.data(), size_t(indexBufferSize));

    // create indirect buffer
    const VkDrawIndexedIndirectCommand drawIndirectCommand{
        indexCount, // indexCount
        1u,         // instanceCount
        firstIndex, // firstIndex
        0u,         // vertexOffset
        0u,         // firstInstance
    };
    const VkDeviceSize indirectBufferSize = sizeof(drawIndirectCommand);
    const VkBufferCreateInfo indirectBufferInfo =
        makeBufferCreateInfo(indirectBufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | commonUsage);
    BufferWithMemory indirectBuffer(vk, *m_device, allocator, indirectBufferInfo, memReq);
    if ((m_params.mode == TM_DRAW_INDEXED_INDIRECT) || (m_params.mode == TM_DRAW_INDEXED_INDIRECT_COUNT))
    {
        deMemcpy(indirectBuffer.getAllocation().getHostPtr(), &drawIndirectCommand, indirectBufferSize);
    }

    // create indirect count buffer
    const VkDeviceSize indirectCountBufferSize = sizeof(uint32_t);
    const VkBufferCreateInfo indirectCountBufferInfo =
        makeBufferCreateInfo(indirectCountBufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | commonUsage);
    BufferWithMemory indirectCountBuffer(vk, *m_device, allocator, indirectCountBufferInfo, memReq);
    if (m_params.mode == TM_DRAW_INDEXED_INDIRECT_COUNT)
    {
        *static_cast<uint32_t *>(indirectCountBuffer.getAllocation().getHostPtr()) = 1u;
    }

    // create output buffer that will be used to read rendered image
    const VkDeviceSize outputBufferSize = renderSize.x() * renderSize.y() * tcu::getPixelSize(mapVkFormat(colorFormat));
    const VkBufferCreateInfo outputBufferInfo =
        makeBufferCreateInfo(outputBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | commonUsage);
    BufferWithMemory outputBuffer(vk, device, allocator, outputBufferInfo, memReq);
    VkDeviceAddress outputBufferAddress = 0ull;

#ifndef CTS_USES_VULKANSC
    VkDeviceAddress vertexBufferAddress        = 0ull;
    VkDeviceAddress indexBufferAddress         = 0ull;
    VkDeviceAddress indirectBufferAddress      = 0ull;
    VkDeviceAddress indirectCountBufferAddress = 0ull;

    if (m_params.useDeviceAddressCommands)
    {
        vertexBufferAddress        = getBufferDeviceAddress(vk, *m_device, *vertexBuffer);
        indexBufferAddress         = getBufferDeviceAddress(vk, *m_device, *indexBuffer);
        indirectBufferAddress      = getBufferDeviceAddress(vk, *m_device, *indirectBuffer);
        indirectCountBufferAddress = getBufferDeviceAddress(vk, *m_device, *indirectCountBuffer);
        outputBufferAddress        = getBufferDeviceAddress(vk, *m_device, *outputBuffer);
    }
#endif

    // create color buffer
    const VkExtent3D imageExtent = makeExtent3D(renderSize.x(), renderSize.y(), 1u);
    const VkImageCreateInfo imageCreateInfo{
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                                   // VkStructureType sType;
        nullptr,                                                               // const void* pNext;
        0u,                                                                    // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                                                      // VkImageType imageType;
        colorFormat,                                                           // VkFormat format;
        imageExtent,                                                           // VkExtent3D extent;
        1u,                                                                    // uint32_t mipLevels;
        1u,                                                                    // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,                                                 // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,                                               // VkImageTiling tiling;
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,                                             // VkSharingMode sharingMode;
        0u,                                                                    // uint32_t queueFamilyIndexCount;
        nullptr,                                                               // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,                                             // VkImageLayout initialLayout;
    };
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    const VkImageSubresourceRange colorSRR = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    ImageWithMemory colorImage(vk, *m_device, allocator, imageCreateInfo, MemoryRequirement::Any);
    auto colorImageView = makeImageView(vk, *m_device, *colorImage, VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSRR);

    // create shader modules, renderpass, framebuffer and pipeline
    auto vertShaderModule                 = createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"));
    auto fragShaderModule                 = createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"));
    Move<VkRenderPass> renderPass         = makeRenderPass(vk, device, colorFormat);
    Move<VkPipelineLayout> pipelineLayout = makePipelineLayout(vk, device);
    auto framebuffer = makeFramebuffer(vk, device, *renderPass, *colorImageView, renderSize.x(), renderSize.y());
    Move<VkPipeline> graphicsPipeline = makeGraphicsPipeline(
        vk, device, *pipelineLayout, *vertShaderModule, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
        *fragShaderModule, *renderPass, viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    auto cmdPool   = createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIdx);
    auto cmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    beginCommandBuffer(vk, *cmdBuffer);

    // transition colorbuffer layout
    VkImageMemoryBarrier imageBarrier =
        makeImageMemoryBarrier(0u, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, colorImage.get(), colorSRR);
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u, 0u,
                          0u, 0u, 0u, 1u, &imageBarrier);

    const VkRect2D renderArea = makeRect2D(0, 0, renderSize.x(), renderSize.y());
    beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, renderArea, clearColor);

    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);

    if (!m_params.useDeviceAddressCommands)
        vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vertexBuffer.get(), &static_cast<const VkDeviceSize &>(0));

#ifndef CTS_USES_VULKANSC
    if (m_params.useDeviceAddressCommands)
    {
        VkBindVertexBuffer3InfoKHR vertexBuffer3Info = initVulkanStructure();
        vertexBuffer3Info.addressRange               = {vertexBufferAddress, vertexBufferSize, sizeof(tcu::Vec4)};
        vk.cmdBindVertexBuffers3KHR(*cmdBuffer, 0, 1, &vertexBuffer3Info);

        VkBindIndexBuffer3InfoKHR bindIndexBuffer3Info = initVulkanStructure();
        bindIndexBuffer3Info.addressRange              = {indexBufferAddress + bindingOffset, indexBindingSize};
        bindIndexBuffer3Info.indexType                 = VK_INDEX_TYPE_UINT32;
        vk.cmdBindIndexBuffer3KHR(*cmdBuffer, &bindIndexBuffer3Info);
    }
    else
        vk.cmdBindIndexBuffer2(*cmdBuffer, indexBuffer.get(), bindingOffset, indexBindingSize, VK_INDEX_TYPE_UINT32);
#else
    DE_UNREF(bindingOffset);
    DE_UNREF(indexBindingSize);
#endif

    // we will draw all points at index 0
    if (!m_params.useDeviceAddressCommands)
    {
        switch (m_params.mode)
        {
        case TM_DRAW_INDEXED:
            vk.cmdDrawIndexed(*cmdBuffer, indexCount, 1u, firstIndex, 0, 0);
            break;

        case TM_DRAW_INDEXED_INDIRECT:
            vk.cmdDrawIndexedIndirect(*cmdBuffer, indirectBuffer.get(), 0, 1, uint32_t(sizeof(drawIndirectCommand)));
            break;

        case TM_DRAW_INDEXED_INDIRECT_COUNT:
            vk.cmdDrawIndexedIndirectCount(*cmdBuffer, indirectBuffer.get(), 0, indirectCountBuffer.get(), 0, 1,
                                           uint32_t(sizeof(drawIndirectCommand)));
            break;

        case TM_DRAW_MULTI_INDEXED:
#ifndef CTS_USES_VULKANSC
        {
            const VkMultiDrawIndexedInfoEXT indexInfo[/* { firstIndex, indexCount, vertexOffset } */]{
                {firstIndex + 3, 3, 0},
                {firstIndex, 3, 0},
            };
            vk.cmdDrawMultiIndexedEXT(*cmdBuffer, DE_LENGTH_OF_ARRAY(indexInfo), indexInfo, 1, 0,
                                      sizeof(VkMultiDrawIndexedInfoEXT), nullptr);
        }
#endif
        break;
        }
    }

#ifndef CTS_USES_VULKANSC
    if (m_params.useDeviceAddressCommands)
    {
        if (m_params.mode == TM_DRAW_INDEXED)
            vk.cmdDrawIndexed(*cmdBuffer, (uint32_t)indexCount, 1, firstIndex, 0, 0);
        else if (m_params.mode == TM_DRAW_INDEXED_INDIRECT)
        {
            VkDrawIndirect2InfoKHR drawIndirect2Info = initVulkanStructure();
            drawIndirect2Info.addressRange = {indirectBufferAddress, indirectBufferSize, sizeof(drawIndirectCommand)};
            drawIndirect2Info.drawCount    = 1u;

            vk.cmdDrawIndexedIndirect2KHR(*cmdBuffer, &drawIndirect2Info);
        }
        else if (m_params.mode == TM_DRAW_INDEXED_INDIRECT_COUNT)
        {
            VkDrawIndirectCount2InfoKHR drawIndirectCount2Info = initVulkanStructure();
            drawIndirectCount2Info.addressRange                = {indirectBufferAddress, indirectBufferSize,
                                                                  sizeof(drawIndirectCommand)};
            drawIndirectCount2Info.countAddressRange           = {indirectCountBufferAddress, indirectCountBufferSize};
            drawIndirectCount2Info.maxDrawCount                = 1;

            vk.cmdDrawIndexedIndirectCount2KHR(*cmdBuffer, &drawIndirectCount2Info);
        }
        else // TM_DRAW_MULTI_INDEXED
            DE_ASSERT(false);
    }
#endif // CTS_USES_VULKANSC

    endRenderPass(vk, *cmdBuffer);

    // wait till data is transfered to image
    imageBarrier = makeImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *colorImage, colorSRR);
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
                          0u, 0u, 0u, 0u, 1u, &imageBarrier);

    // read back color image
    copyImageToMemory(vk, *cmdBuffer, *colorImage, imageExtent, *outputBuffer, outputBufferAddress, outputBufferSize);

    endCommandBuffer(vk, *cmdBuffer);
    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    // get output buffer
    invalidateAlloc(vk, device, outputBuffer.getAllocation());
    const tcu::TextureFormat resultFormat = mapVkFormat(colorFormat);
    tcu::ConstPixelBufferAccess resultAccess(resultFormat, renderSize.x(), renderSize.y(), 1u,
                                             outputBuffer.getAllocation().getHostPtr());

    // neither one triangle should be drawn in the second quarter, they are omitted by the offset or the firstIndex parameters
    const tcu::Vec4 p11 = resultAccess.getPixel((1 * renderSize.x()) / 8, (5 * renderSize.y()) / 8);
    const tcu::Vec4 p12 = resultAccess.getPixel((3 * renderSize.x()) / 8, (7 * renderSize.y()) / 8);
    const bool c1       = p11.x() == clearColor.x() && p11.y() == clearColor.y() && p11.z() == clearColor.z() &&
                    p12.x() == clearColor.x() && p12.y() == clearColor.y() && p12.z() == clearColor.z();

    // small triangle in the third quarter must be drawn always
    const tcu::Vec4 p2 = resultAccess.getPixel((1 * renderSize.x()) / 8, (1 * renderSize.y()) / 8);
    const bool c2      = p2.x() != clearColor.x() && p2.y() != clearColor.y() && p2.z() != clearColor.z();

    // if robustness works, then the origin of coordinate system will be read in shader instead of a value that an index points (1,1)
    const tcu::Vec4 p3 = resultAccess.getPixel((3 * renderSize.x()) / 4, (3 * renderSize.y()) / 4);
    const bool c3      = p3.x() == clearColor.x() && p3.y() == clearColor.y() && p3.z() == clearColor.z();

    bool verdict = false;
    switch (m_params.ooType)
    {
    case OOTypes::OO_NONE:
        verdict = c1 && c2 && !c3;
        break;
    default:
        verdict = c1 && c2 && c3;
        break;
    }

    log << tcu::TestLog::ImageSet("Result", "") << tcu::TestLog::Image(std::to_string(m_params.mode), "", resultAccess)
        << tcu::TestLog::EndImageSet;
    return (*(verdict ? &tcu::TestStatus::pass : &tcu::TestStatus::fail))(std::string());
}

tcu::TestCaseGroup *createCmdBindIndexBuffer2Tests(tcu::TestContext &testCtx)
{
    const std::pair<const char *, TestMode> modes[]{
        {"draw_indexed", TestMode::TM_DRAW_INDEXED},
        {"draw_indexed_indirect", TestMode::TM_DRAW_INDEXED_INDIRECT},
        {"draw_indexed_indirect_count", TestMode::TM_DRAW_INDEXED_INDIRECT_COUNT},
        {"draw_multi_indexed", TestMode::TM_DRAW_MULTI_INDEXED},
    };

    const std::pair<std::string, OOTypes> OutOfTypes[]{
        {"oo_none", OOTypes::OO_NONE},
        {"oo_index", OOTypes::OO_INDEX},
        {"oo_size", OOTypes::OO_SIZE},
        {"oo_whole_size", OOTypes::OO_WHOLE_SIZE},
    };

    const uint32_t offsets[] = {0, 100};

    // Test access outside of the buffer with using the vkCmdBindIndexBuffer2 function from
    // VK_KHR_maintenance5 and with vkCmdBindIndexBuffer3KHR from VK_KHR_device_address_commands.
    de::MovePtr<tcu::TestCaseGroup> gRoot(new tcu::TestCaseGroup(testCtx, "bind_index_buffer2"));
    for (uint32_t offset : offsets)
    {
        de::MovePtr<tcu::TestCaseGroup> gOffset(
            new tcu::TestCaseGroup(testCtx, ("offset_" + std::to_string(offset)).c_str()));
        for (const auto &mode : modes)
        {
            de::MovePtr<tcu::TestCaseGroup> gMode(new tcu::TestCaseGroup(testCtx, mode.first));
            for (const auto &ooType : OutOfTypes)
            {
                TestParams p;
                p.mode         = mode.second;
                p.ooType       = ooType.second;
                p.leadingCount = offset;

                gMode->addChild(new BindIndexBuffer2TestCase(testCtx, ooType.first, p));

#ifndef CTS_USES_VULKANSC
                // skip testing VK_WHOLE_SIZE for device_address_commands, as it is not supported
                if (ooType.second == OOTypes::OO_WHOLE_SIZE)
                    continue;

                // limit number of tests repeated for device_address_commands
                if ((mode.second != TestMode::TM_DRAW_MULTI_INDEXED) && (offset == 100))
                {
                    p.useDeviceAddressCommands = true;
                    gMode->addChild(new BindIndexBuffer2TestCase(testCtx, ooType.first + "_device_address", p));
                }
#endif // CTS_USES_VULKANSC
            }
            gOffset->addChild(gMode.release());
        }
        gRoot->addChild(gOffset.release());
    }

    return gRoot.release();
}

tcu::TestCaseGroup *createIndexAccessTests(tcu::TestContext &testCtx)
{
    // Test access outside of the buffer for indices
    de::MovePtr<tcu::TestCaseGroup> indexAccessTests(new tcu::TestCaseGroup(testCtx, "index_access"));

    const std::pair<std::string, TestMode> testModes[]{
        {"draw_indexed", TestMode::TM_DRAW_INDEXED},
        {"draw_indexed_indirect", TestMode::TM_DRAW_INDEXED_INDIRECT},
        {"draw_indexed_indirect_count", TestMode::TM_DRAW_INDEXED_INDIRECT_COUNT},
        {"draw_multi_indexed", TestMode::TM_DRAW_MULTI_INDEXED},
    };

    for (const auto &[n, mode] : testModes)
    {
        TestParams params;
        params.mode              = mode;
        params.robustnessVersion = 2;

        std::string name = n + "_" + std::to_string(params.robustnessVersion);
        indexAccessTests->addChild(new DrawIndexedTestCase(testCtx, name, params));

#ifndef CTS_USES_VULKANSC
        if (mode != TestMode::TM_DRAW_MULTI_INDEXED)
        {
            params.useDeviceAddressCommands = true;
            indexAccessTests->addChild(new DrawIndexedTestCase(testCtx, name + "_device_address", params));
        }
#endif // CTS_USES_VULKANSC
    }

    return indexAccessTests.release();
}

} // namespace vkt::robustness
