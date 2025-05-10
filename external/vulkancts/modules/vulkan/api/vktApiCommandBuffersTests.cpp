/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 * Copyright (c) 2015 Google Inc.
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
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkPlatform.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkPrograms.hpp"
#include "vkTypeUtil.hpp"
#include "vkAllocationCallbackUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuCommandLine.hpp"
#include "tcuImageCompare.hpp"
#include "vktApiCommandBuffersTests.hpp"
#include "vktApiBufferComputeInstance.hpp"
#include "vktApiComputeInstanceResultBuffer.hpp"
#include "deSharedPtr.hpp"
#include "deRandom.hpp"
#include <sstream>
#include <limits>

namespace vkt
{
namespace api
{
namespace
{

using namespace vk;

typedef de::SharedPtr<vk::Unique<vk::VkEvent>> VkEventSp;

// Global variables
const uint64_t INFINITE_TIMEOUT = ~(uint64_t)0u;

template <uint32_t NumBuffers>
class CommandBufferBareTestEnvironment
{
public:
    CommandBufferBareTestEnvironment(Context &context, VkCommandPoolCreateFlags commandPoolCreateFlags);

    VkCommandPool getCommandPool(void) const
    {
        return *m_commandPool;
    }
    VkCommandBuffer getCommandBuffer(uint32_t bufferIndex) const;

protected:
    Context &m_context;
    const VkDevice m_device;
    const DeviceInterface &m_vkd;
    const VkQueue m_queue;
    const uint32_t m_queueFamilyIndex;
    Allocator &m_allocator;

    Move<VkCommandPool> m_commandPool;
    Move<VkCommandBuffer> m_primaryCommandBuffers[NumBuffers];
};

template <uint32_t NumBuffers>
CommandBufferBareTestEnvironment<NumBuffers>::CommandBufferBareTestEnvironment(
    Context &context, VkCommandPoolCreateFlags commandPoolCreateFlags)
    : m_context(context)
    , m_device(context.getDevice())
    , m_vkd(context.getDeviceInterface())
    , m_queue(context.getUniversalQueue())
    , m_queueFamilyIndex(context.getUniversalQueueFamilyIndex())
    , m_allocator(context.getDefaultAllocator())
{
    m_commandPool = createCommandPool(m_vkd, m_device, commandPoolCreateFlags, m_queueFamilyIndex);

    const VkCommandBufferAllocateInfo cmdBufferAllocateInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // VkStructureType             sType;
        nullptr,                                        // const void*                 pNext;
        *m_commandPool,                                 // VkCommandPool               commandPool;
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,                // VkCommandBufferLevel        level;
        NumBuffers                                      // uint32_t                    commandBufferCount;
    };

    allocateCommandBuffers(m_vkd, m_device, &cmdBufferAllocateInfo, m_primaryCommandBuffers);
}

template <uint32_t NumBuffers>
VkCommandBuffer CommandBufferBareTestEnvironment<NumBuffers>::getCommandBuffer(uint32_t bufferIndex) const
{
    DE_ASSERT(bufferIndex < NumBuffers);
    return m_primaryCommandBuffers[bufferIndex].get();
}

class CommandBufferRenderPassTestEnvironment : public CommandBufferBareTestEnvironment<1>
{
public:
    CommandBufferRenderPassTestEnvironment(Context &context, VkCommandPoolCreateFlags commandPoolCreateFlags);

    VkRenderPass getRenderPass(void) const
    {
        return *m_renderPass;
    }
    VkFramebuffer getFrameBuffer(void) const
    {
        return *m_frameBuffer;
    }
    VkCommandBuffer getPrimaryCommandBuffer(void) const
    {
        return getCommandBuffer(0);
    }
    VkCommandBuffer getSecondaryCommandBuffer(void) const
    {
        return *m_secondaryCommandBuffer;
    }
    VkCommandBuffer getNestedCommandBuffer(void) const
    {
        return *m_nestedCommandBuffer;
    }

    void beginPrimaryCommandBuffer(VkCommandBufferUsageFlags usageFlags);
    void beginSecondaryCommandBuffer(VkCommandBufferUsageFlags usageFlags, bool framebufferHint);
    void beginNestedCommandBuffer(VkCommandBufferUsageFlags usageFlags, bool framebufferHint);
    void beginRenderPass(VkSubpassContents content);
    void submitPrimaryCommandBuffer(void);
    de::MovePtr<tcu::TextureLevel> readColorAttachment(void);

    static const VkImageType DEFAULT_IMAGE_TYPE;
    static const VkFormat DEFAULT_IMAGE_FORMAT;
    static const VkExtent3D DEFAULT_IMAGE_SIZE;
    static const VkRect2D DEFAULT_IMAGE_AREA;

protected:
    Move<VkImage> m_colorImage;
    Move<VkImageView> m_colorImageView;
    Move<VkRenderPass> m_renderPass;
    Move<VkFramebuffer> m_frameBuffer;
    de::MovePtr<Allocation> m_colorImageMemory;
    Move<VkCommandPool> m_secCommandPool;
    Move<VkCommandBuffer> m_secondaryCommandBuffer;
    Move<VkCommandBuffer> m_nestedCommandBuffer;
};

const VkImageType CommandBufferRenderPassTestEnvironment::DEFAULT_IMAGE_TYPE = VK_IMAGE_TYPE_2D;
const VkFormat CommandBufferRenderPassTestEnvironment::DEFAULT_IMAGE_FORMAT  = VK_FORMAT_R8G8B8A8_UINT;
const VkExtent3D CommandBufferRenderPassTestEnvironment::DEFAULT_IMAGE_SIZE  = {255, 255, 1};
const VkRect2D CommandBufferRenderPassTestEnvironment::DEFAULT_IMAGE_AREA    = {
    {
        0u,
        0u,
    },                                                     // VkOffset2D offset;
    {DEFAULT_IMAGE_SIZE.width, DEFAULT_IMAGE_SIZE.height}, // VkExtent2D extent;
};

CommandBufferRenderPassTestEnvironment::CommandBufferRenderPassTestEnvironment(
    Context &context, VkCommandPoolCreateFlags commandPoolCreateFlags)
    : CommandBufferBareTestEnvironment<1>(context, commandPoolCreateFlags)
{
    m_renderPass = makeRenderPass(m_vkd, m_device, DEFAULT_IMAGE_FORMAT);

    {
        const VkImageCreateInfo imageCreateInfo = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
            nullptr,                             // const void* pNext;
            0u,                                  // VkImageCreateFlags flags;
            DEFAULT_IMAGE_TYPE,                  // VkImageType imageType;
            DEFAULT_IMAGE_FORMAT,                // VkFormat format;
            DEFAULT_IMAGE_SIZE,                  // VkExtent3D extent;
            1,                                   // uint32_t mipLevels;
            1,                                   // uint32_t arrayLayers;
            VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
            VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VkImageUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
            1,                                   // uint32_t queueFamilyIndexCount;
            &m_queueFamilyIndex,                 // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED            // VkImageLayout initialLayout;
        };

        m_colorImage = createImage(m_vkd, m_device, &imageCreateInfo, nullptr);
    }

    m_colorImageMemory =
        m_allocator.allocate(getImageMemoryRequirements(m_vkd, m_device, *m_colorImage), MemoryRequirement::Any);
    VK_CHECK(m_vkd.bindImageMemory(m_device, *m_colorImage, m_colorImageMemory->getMemory(),
                                   m_colorImageMemory->getOffset()));

    {
        const VkImageViewCreateInfo imageViewCreateInfo = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
            nullptr,                                  // const void* pNext;
            0u,                                       // VkImageViewCreateFlags flags;
            *m_colorImage,                            // VkImage image;
            VK_IMAGE_VIEW_TYPE_2D,                    // VkImageViewType viewType;
            DEFAULT_IMAGE_FORMAT,                     // VkFormat format;
            {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B,
             VK_COMPONENT_SWIZZLE_A}, // VkComponentMapping components;
            {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t baseMipLevel;
                1u,                        // uint32_t mipLevels;
                0u,                        // uint32_t baseArrayLayer;
                1u,                        // uint32_t arraySize;
            },                             // VkImageSubresourceRange subresourceRange;
        };

        m_colorImageView = createImageView(m_vkd, m_device, &imageViewCreateInfo, nullptr);
    }

    {
        const VkImageView attachmentViews[1] = {*m_colorImageView};

        const VkFramebufferCreateInfo framebufferCreateInfo = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                                   // const void* pNext;
            0u,                                        // VkFramebufferCreateFlags flags;
            *m_renderPass,                             // VkRenderPass renderPass;
            1,                                         // uint32_t attachmentCount;
            attachmentViews,                           // const VkImageView* pAttachments;
            DEFAULT_IMAGE_SIZE.width,                  // uint32_t width;
            DEFAULT_IMAGE_SIZE.height,                 // uint32_t height;
            1u,                                        // uint32_t layers;
        };

        m_frameBuffer = createFramebuffer(m_vkd, m_device, &framebufferCreateInfo, nullptr);
    }

    m_secCommandPool = createCommandPool(m_vkd, m_device, commandPoolCreateFlags, m_queueFamilyIndex);

    {
        const VkCommandBufferAllocateInfo cmdBufferAllocateInfo = {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // VkStructureType             sType;
            nullptr,                                        // const void*                 pNext;
            *m_secCommandPool,                              // VkCommandPool               commandPool;
            VK_COMMAND_BUFFER_LEVEL_SECONDARY,              // VkCommandBufferLevel        level;
            1u                                              // uint32_t                    commandBufferCount;
        };

        m_secondaryCommandBuffer = allocateCommandBuffer(m_vkd, m_device, &cmdBufferAllocateInfo);
        m_nestedCommandBuffer    = allocateCommandBuffer(m_vkd, m_device, &cmdBufferAllocateInfo);
    }
}

void CommandBufferRenderPassTestEnvironment::beginRenderPass(VkSubpassContents content)
{
    vk::beginRenderPass(m_vkd, m_primaryCommandBuffers[0].get(), *m_renderPass, *m_frameBuffer, DEFAULT_IMAGE_AREA,
                        tcu::UVec4(17, 59, 163, 251), content);
}

void CommandBufferRenderPassTestEnvironment::beginPrimaryCommandBuffer(VkCommandBufferUsageFlags usageFlags)
{
    beginCommandBuffer(m_vkd, m_primaryCommandBuffers[0].get(), usageFlags);
}

void CommandBufferRenderPassTestEnvironment::beginSecondaryCommandBuffer(VkCommandBufferUsageFlags usageFlags,
                                                                         bool framebufferHint)
{
    const VkCommandBufferInheritanceInfo commandBufferInheritanceInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,   // VkStructureType                  sType;
        nullptr,                                             // const void*                      pNext;
        *m_renderPass,                                       // VkRenderPass                     renderPass;
        0u,                                                  // uint32_t                         subpass;
        (framebufferHint ? *m_frameBuffer : VK_NULL_HANDLE), // VkFramebuffer                    framebuffer;
        VK_FALSE,                                            // VkBool32                         occlusionQueryEnable;
        0u,                                                  // VkQueryControlFlags              queryFlags;
        0u                                                   // VkQueryPipelineStatisticFlags    pipelineStatistics;
    };

    const VkCommandBufferBeginInfo commandBufferBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // VkStructureType                          sType;
        nullptr,                                     // const void*                              pNext;
        usageFlags,                                  // VkCommandBufferUsageFlags                flags;
        &commandBufferInheritanceInfo                // const VkCommandBufferInheritanceInfo*    pInheritanceInfo;
    };

    VK_CHECK(m_vkd.beginCommandBuffer(*m_secondaryCommandBuffer, &commandBufferBeginInfo));
}

void CommandBufferRenderPassTestEnvironment::beginNestedCommandBuffer(VkCommandBufferUsageFlags usageFlags,
                                                                      bool framebufferHint)
{
    const VkCommandBufferInheritanceInfo commandBufferInheritanceInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,   // VkStructureType                  sType;
        nullptr,                                             // const void*                      pNext;
        *m_renderPass,                                       // VkRenderPass                     renderPass;
        0u,                                                  // uint32_t                         subpass;
        (framebufferHint ? *m_frameBuffer : VK_NULL_HANDLE), // VkFramebuffer                    framebuffer;
        VK_FALSE,                                            // VkBool32                         occlusionQueryEnable;
        0u,                                                  // VkQueryControlFlags              queryFlags;
        0u                                                   // VkQueryPipelineStatisticFlags    pipelineStatistics;
    };

    const VkCommandBufferBeginInfo commandBufferBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // VkStructureType                          sType;
        nullptr,                                     // const void*                              pNext;
        usageFlags,                                  // VkCommandBufferUsageFlags                flags;
        &commandBufferInheritanceInfo                // const VkCommandBufferInheritanceInfo*    pInheritanceInfo;
    };

    VK_CHECK(m_vkd.beginCommandBuffer(*m_nestedCommandBuffer, &commandBufferBeginInfo));
}

void CommandBufferRenderPassTestEnvironment::submitPrimaryCommandBuffer(void)
{
    submitCommandsAndWait(m_vkd, m_device, m_queue, m_primaryCommandBuffers[0].get());
}

de::MovePtr<tcu::TextureLevel> CommandBufferRenderPassTestEnvironment::readColorAttachment()
{
    Move<VkBuffer> buffer;
    de::MovePtr<Allocation> bufferAlloc;
    const tcu::TextureFormat tcuFormat = mapVkFormat(DEFAULT_IMAGE_FORMAT);
    const VkDeviceSize pixelDataSize = DEFAULT_IMAGE_SIZE.height * DEFAULT_IMAGE_SIZE.height * tcuFormat.getPixelSize();
    de::MovePtr<tcu::TextureLevel> resultLevel(
        new tcu::TextureLevel(tcuFormat, DEFAULT_IMAGE_SIZE.width, DEFAULT_IMAGE_SIZE.height));

    // Create destination buffer
    {
        const VkBufferCreateInfo bufferParams = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                              // const void* pNext;
            0u,                                   // VkBufferCreateFlags flags;
            pixelDataSize,                        // VkDeviceSize size;
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,     // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
            0u,                                   // uint32_t queueFamilyIndexCount;
            nullptr                               // const uint32_t* pQueueFamilyIndices;
        };

        buffer = createBuffer(m_vkd, m_device, &bufferParams);
        bufferAlloc =
            m_allocator.allocate(getBufferMemoryRequirements(m_vkd, m_device, *buffer), MemoryRequirement::HostVisible);
        VK_CHECK(m_vkd.bindBufferMemory(m_device, *buffer, bufferAlloc->getMemory(), bufferAlloc->getOffset()));
    }

    // Copy image to buffer
    beginPrimaryCommandBuffer(0);
    copyImageToBuffer(m_vkd, m_primaryCommandBuffers[0].get(), *m_colorImage, *buffer,
                      tcu::IVec2(DEFAULT_IMAGE_SIZE.width, DEFAULT_IMAGE_SIZE.height));
    endCommandBuffer(m_vkd, m_primaryCommandBuffers[0].get());

    submitPrimaryCommandBuffer();

    // Read buffer data
    invalidateAlloc(m_vkd, m_device, *bufferAlloc);
    tcu::copy(*resultLevel,
              tcu::ConstPixelBufferAccess(resultLevel->getFormat(), resultLevel->getSize(), bufferAlloc->getHostPtr()));

    return resultLevel;
}

// Testcases
/********* 19.1. Command Pools (5.1 in VK 1.0 Spec) ***************************/
tcu::TestStatus createPoolNullParamsTest(Context &context)
{
    const VkDevice vkDevice         = context.getDevice();
    const DeviceInterface &vk       = context.getDeviceInterface();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();

    createCommandPool(vk, vkDevice, 0u, queueFamilyIndex);

    return tcu::TestStatus::pass("Command Pool allocated correctly.");
}

#ifndef CTS_USES_VULKANSC
tcu::TestStatus createPoolNonNullAllocatorTest(Context &context)
{
    const VkDevice vkDevice                          = context.getDevice();
    const DeviceInterface &vk                        = context.getDeviceInterface();
    const uint32_t queueFamilyIndex                  = context.getUniversalQueueFamilyIndex();
    const VkAllocationCallbacks *allocationCallbacks = getSystemAllocator();

    const VkCommandPoolCreateInfo cmdPoolParams = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, // sType;
        nullptr,                                    // pNext;
        0u,                                         // flags;
        queueFamilyIndex,                           // queueFamilyIndex;
    };

    createCommandPool(vk, vkDevice, &cmdPoolParams, allocationCallbacks);

    return tcu::TestStatus::pass("Command Pool allocated correctly.");
}
#endif // CTS_USES_VULKANSC

tcu::TestStatus createPoolTransientBitTest(Context &context)
{
    const VkDevice vkDevice         = context.getDevice();
    const DeviceInterface &vk       = context.getDeviceInterface();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();

    const VkCommandPoolCreateInfo cmdPoolParams = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, // sType;
        nullptr,                                    // pNext;
        VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,       // flags;
        queueFamilyIndex,                           // queueFamilyIndex;
    };

    createCommandPool(vk, vkDevice, &cmdPoolParams, nullptr);

    return tcu::TestStatus::pass("Command Pool allocated correctly.");
}

tcu::TestStatus createPoolResetBitTest(Context &context)
{
    const VkDevice vkDevice         = context.getDevice();
    const DeviceInterface &vk       = context.getDeviceInterface();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();

    const VkCommandPoolCreateInfo cmdPoolParams = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,      // sType;
        nullptr,                                         // pNext;
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // flags;
        queueFamilyIndex,                                // queueFamilyIndex;
    };

    createCommandPool(vk, vkDevice, &cmdPoolParams, nullptr);

    return tcu::TestStatus::pass("Command Pool allocated correctly.");
}

#ifndef CTS_USES_VULKANSC
tcu::TestStatus resetPoolReleaseResourcesBitTest(Context &context)
{
    const VkDevice vkDevice         = context.getDevice();
    const DeviceInterface &vk       = context.getDeviceInterface();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();

    const VkCommandPoolCreateInfo cmdPoolParams = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, // sType;
        nullptr,                                    // pNext;
        0u,                                         // flags;
        queueFamilyIndex,                           // queueFamilyIndex;
    };

    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, vkDevice, &cmdPoolParams, nullptr));

    VK_CHECK(vk.resetCommandPool(vkDevice, *cmdPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT));

    return tcu::TestStatus::pass("Command Pool allocated correctly.");
}
#endif // CTS_USES_VULKANSC

tcu::TestStatus resetPoolNoFlagsTest(Context &context)
{
    const VkDevice vkDevice         = context.getDevice();
    const DeviceInterface &vk       = context.getDeviceInterface();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();

    const VkCommandPoolCreateInfo cmdPoolParams = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, // sType;
        nullptr,                                    // pNext;
        0u,                                         // flags;
        queueFamilyIndex,                           // queueFamilyIndex;
    };

    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, vkDevice, &cmdPoolParams, nullptr));

    VK_CHECK(vk.resetCommandPool(vkDevice, *cmdPool, 0u));

    return tcu::TestStatus::pass("Command Pool allocated correctly.");
}

#ifndef CTS_USES_VULKANSC
bool executeCommandBuffer(const VkDevice device, const DeviceInterface &vk, const VkQueue queue,
                          const VkCommandBuffer commandBuffer, const bool exitBeforeEndCommandBuffer = false)
{
    const Unique<VkEvent> event(createEvent(vk, device));
    beginCommandBuffer(vk, commandBuffer, 0u);
    {
        const VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        vk.cmdSetEvent(commandBuffer, *event, stageMask);
        if (exitBeforeEndCommandBuffer)
            return exitBeforeEndCommandBuffer;
    }
    endCommandBuffer(vk, commandBuffer);

    submitCommandsAndWait(vk, device, queue, commandBuffer);

    // check if buffer has been executed
    const VkResult result = vk.getEventStatus(device, *event);
    return result == VK_EVENT_SET;
}

tcu::TestStatus resetPoolReuseTest(Context &context)
{
    const VkDevice vkDevice         = context.getDevice();
    const DeviceInterface &vk       = context.getDeviceInterface();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();
    const VkQueue queue             = context.getUniversalQueue();

    const VkCommandPoolCreateInfo cmdPoolParams = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, // sType;
        nullptr,                                    // pNext;
        0u,                                         // flags;
        queueFamilyIndex                            // queueFamilyIndex;
    };
    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, vkDevice, &cmdPoolParams, nullptr));
    const VkCommandBufferAllocateInfo cmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // sType;
        nullptr,                                        // pNext;
        *cmdPool,                                       // commandPool;
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,                // level;
        1u                                              // bufferCount;
    };
    const Move<VkCommandBuffer> commandBuffers[] = {allocateCommandBuffer(vk, vkDevice, &cmdBufParams),
                                                    allocateCommandBuffer(vk, vkDevice, &cmdBufParams)};

#ifdef CTS_USES_VULKANSC
    bool canFinishEarlier = context.getTestContext().getCommandLine().isSubProcess();
#else
    bool canFinishEarlier           = true;
#endif // CTS_USES_VULKANSC

    if (!executeCommandBuffer(vkDevice, vk, queue, *(commandBuffers[0])) && canFinishEarlier)
        return tcu::TestStatus::fail("Failed");
    if (!executeCommandBuffer(vkDevice, vk, queue, *(commandBuffers[1]), true) && canFinishEarlier)
        return tcu::TestStatus::fail("Failed");

    VK_CHECK(vk.resetCommandPool(vkDevice, *cmdPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT));

    if (!executeCommandBuffer(vkDevice, vk, queue, *(commandBuffers[0])) && canFinishEarlier)
        return tcu::TestStatus::fail("Failed");
    if (!executeCommandBuffer(vkDevice, vk, queue, *(commandBuffers[1])) && canFinishEarlier)
        return tcu::TestStatus::fail("Failed");

    {
        const Unique<VkCommandBuffer> afterResetCommandBuffers(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));
        if (!executeCommandBuffer(vkDevice, vk, queue, *afterResetCommandBuffers) && canFinishEarlier)
            return tcu::TestStatus::fail("Failed");
    }

    return tcu::TestStatus::pass("Passed");
}
#endif // CTS_USES_VULKANSC

/******** 19.2. Command Buffer Lifetime (5.2 in VK 1.0 Spec) ******************/
tcu::TestStatus allocatePrimaryBufferTest(Context &context)
{
    const VkDevice vkDevice         = context.getDevice();
    const DeviceInterface &vk       = context.getDeviceInterface();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();

    const VkCommandPoolCreateInfo cmdPoolParams = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,      // sType;
        nullptr,                                         // pNext;
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // flags;
        queueFamilyIndex,                                // queueFamilyIndex;
    };
    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, vkDevice, &cmdPoolParams));

    // Command buffer
    const VkCommandBufferAllocateInfo cmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // sType;
        nullptr,                                        // pNext;
        *cmdPool,                                       // commandPool;
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,                // level;
        1u,                                             // bufferCount;
    };
    const Unique<VkCommandBuffer> cmdBuf(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

    return tcu::TestStatus::pass("Buffer was created correctly.");
}

tcu::TestStatus allocateManyPrimaryBuffersTest(Context &context)
{

    const VkDevice vkDevice         = context.getDevice();
    const DeviceInterface &vk       = context.getDeviceInterface();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();

    const VkCommandPoolCreateInfo cmdPoolParams = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,      // VkStructureType sType;
        nullptr,                                         // const void* pNext;
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // VkCommandPoolCreateFlags flags;
        queueFamilyIndex,                                // uint32_t queueFamilyIndex;
    };
    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, vkDevice, &cmdPoolParams));

    // \todo Determining the minimum number of command buffers should be a function of available system memory and driver capabilities.
#ifndef CTS_USES_VULKANSC
#if (DE_PTR_SIZE == 4)
    const unsigned minCommandBuffer = 1024;
#else
    const unsigned minCommandBuffer = 10000;
#endif
#else
    const unsigned minCommandBuffer  = 100;
#endif // CTS_USES_VULKANSC

    // Command buffer
    const VkCommandBufferAllocateInfo cmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        *cmdPool,                                       // VkCommandPool pool;
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,                // VkCommandBufferLevel level;
        minCommandBuffer,                               // uint32_t bufferCount;
    };

    // do not keep the handles to buffers, as they will be freed with command pool

    // allocate the minimum required amount of buffers
    Move<VkCommandBuffer> cmdBuffers[minCommandBuffer];
    allocateCommandBuffers(vk, vkDevice, &cmdBufParams, cmdBuffers);

    std::ostringstream out;
    out << "allocateManyPrimaryBuffersTest succeded: created " << minCommandBuffer << " command buffers";

    return tcu::TestStatus::pass(out.str());
}

tcu::TestStatus allocateSecondaryBufferTest(Context &context)
{
    const VkDevice vkDevice         = context.getDevice();
    const DeviceInterface &vk       = context.getDeviceInterface();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();

    const VkCommandPoolCreateInfo cmdPoolParams = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,      // sType;
        nullptr,                                         // pNext;
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // flags;
        queueFamilyIndex,                                // queueFamilyIndex;
    };
    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, vkDevice, &cmdPoolParams));

    // Command buffer
    const VkCommandBufferAllocateInfo cmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // sType;
        nullptr,                                        // pNext;
        *cmdPool,                                       // commandPool;
        VK_COMMAND_BUFFER_LEVEL_SECONDARY,              // level;
        1u,                                             // bufferCount;
    };
    const Unique<VkCommandBuffer> cmdBuf(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

    return tcu::TestStatus::pass("Buffer was created correctly.");
}

tcu::TestStatus allocateManySecondaryBuffersTest(Context &context)
{

    const VkDevice vkDevice         = context.getDevice();
    const DeviceInterface &vk       = context.getDeviceInterface();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();

    const VkCommandPoolCreateInfo cmdPoolParams = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,      // VkStructureType sType;
        nullptr,                                         // const void* pNext;
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // VkCommandPoolCreateFlags flags;
        queueFamilyIndex,                                // uint32_t queueFamilyIndex;
    };
    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, vkDevice, &cmdPoolParams));

    // \todo Determining the minimum number of command buffers should be a function of available system memory and driver capabilities.
#ifndef CTS_USES_VULKANSC
#if (DE_PTR_SIZE == 4)
    const unsigned minCommandBuffer = 1024;
#else
    const unsigned minCommandBuffer = 10000;
#endif
#else
    const unsigned minCommandBuffer  = 100;
#endif // CTS_USES_VULKANSC

    // Command buffer
    const VkCommandBufferAllocateInfo cmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        *cmdPool,                                       // VkCommandPool pool;
        VK_COMMAND_BUFFER_LEVEL_SECONDARY,              // VkCommandBufferLevel level;
        minCommandBuffer,                               // uint32_t bufferCount;
    };

    // do not keep the handles to buffers, as they will be freed with command pool

    // allocate the minimum required amount of buffers
    Move<VkCommandBuffer> cmdBuffers[minCommandBuffer];
    allocateCommandBuffers(vk, vkDevice, &cmdBufParams, cmdBuffers);

    std::ostringstream out;
    out << "allocateManySecondaryBuffersTest succeded: created " << minCommandBuffer << " command buffers";

    return tcu::TestStatus::pass(out.str());
}

tcu::TestStatus executePrimaryBufferTest(Context &context)
{
    const VkDevice vkDevice         = context.getDevice();
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkQueue queue             = context.getUniversalQueue();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();

    const VkCommandPoolCreateInfo cmdPoolParams = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,      // VkStructureType sType;
        nullptr,                                         // const void* pNext;
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // VkCommandPoolCreateFlags flags;
        queueFamilyIndex,                                // uint32_t queueFamilyIndex;
    };
    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, vkDevice, &cmdPoolParams));

    // Command buffer
    const VkCommandBufferAllocateInfo cmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        *cmdPool,                                       // VkCommandPool pool;
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,                // VkCommandBufferLevel level;
        1u,                                             // uint32_t bufferCount;
    };
    const Unique<VkCommandBuffer> primCmdBuf(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

    // create event that will be used to check if secondary command buffer has been executed
    const Unique<VkEvent> event(createEvent(vk, vkDevice));

    // reset event
    VK_CHECK(vk.resetEvent(vkDevice, *event));

    // record primary command buffer
    beginCommandBuffer(vk, *primCmdBuf, 0u);
    {
        // allow execution of event during every stage of pipeline
        VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

        // record setting event
        vk.cmdSetEvent(*primCmdBuf, *event, stageMask);
    }
    endCommandBuffer(vk, *primCmdBuf);

    submitCommandsAndWait(vk, vkDevice, queue, primCmdBuf.get());

    // check if buffer has been executed
    VkResult result = vk.getEventStatus(vkDevice, *event);
    if (result == VK_EVENT_SET)
        return tcu::TestStatus::pass("Execute Primary Command Buffer succeeded");

    return tcu::TestStatus::fail("Execute Primary Command Buffer FAILED");
}

tcu::TestStatus executeLargePrimaryBufferTest(Context &context)
{
    const VkDevice vkDevice         = context.getDevice();
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkQueue queue             = context.getUniversalQueue();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();
#ifndef CTS_USES_VULKANSC
    const uint32_t LARGE_BUFFER_SIZE = 10000;
#else
    const uint32_t LARGE_BUFFER_SIZE = 100;
#endif // CTS_USES_VULKANSC

    const VkCommandPoolCreateInfo cmdPoolParams = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,      // VkStructureType sType;
        nullptr,                                         // const void* pNext;
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // VkCommandPoolCreateFlags flags;
        queueFamilyIndex,                                // uint32_t queueFamilyIndex;
    };
    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, vkDevice, &cmdPoolParams));

    // Command buffer
    const VkCommandBufferAllocateInfo cmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        *cmdPool,                                       // VkCommandPool pool;
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,                // VkCommandBufferLevel level;
        1u,                                             // uint32_t bufferCount;
    };
    const Unique<VkCommandBuffer> primCmdBuf(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

    std::vector<VkEventSp> events;
    for (uint32_t ndx = 0; ndx < LARGE_BUFFER_SIZE; ++ndx)
        events.push_back(VkEventSp(new vk::Unique<VkEvent>(createEvent(vk, vkDevice))));

    // record primary command buffer
    beginCommandBuffer(vk, *primCmdBuf, 0u);
    {
        // set all the events
        for (uint32_t ndx = 0; ndx < LARGE_BUFFER_SIZE; ++ndx)
        {
            vk.cmdSetEvent(*primCmdBuf, events[ndx]->get(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        }
    }
    endCommandBuffer(vk, *primCmdBuf);

    submitCommandsAndWait(vk, vkDevice, queue, primCmdBuf.get());

    // check if the buffer was executed correctly - all events had their status
    // changed
    tcu::TestStatus testResult = tcu::TestStatus::incomplete();

    for (uint32_t ndx = 0; ndx < LARGE_BUFFER_SIZE; ++ndx)
    {
        if (vk.getEventStatus(vkDevice, events[ndx]->get()) != VK_EVENT_SET)
        {
            testResult = tcu::TestStatus::fail("An event was not set.");
            break;
        }
    }

    if (!testResult.isComplete())
        testResult = tcu::TestStatus::pass("All events set correctly.");

    return testResult;
}

tcu::TestStatus resetBufferImplicitlyTest(Context &context)
{
    const VkDevice vkDevice         = context.getDevice();
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkQueue queue             = context.getUniversalQueue();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();

#ifdef CTS_USES_VULKANSC
    if (context.getDeviceVulkanSC10Properties().commandPoolResetCommandBuffer == VK_FALSE)
        TCU_THROW(NotSupportedError, "commandPoolResetCommandBuffer not supported by this implementation");
#endif // CTS_USES_VULKANSC

    const VkCommandPoolCreateInfo cmdPoolParams = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,      // sType;
        nullptr,                                         // pNext;
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // flags;
        queueFamilyIndex,                                // queueFamilyIndex;
    };
    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, vkDevice, &cmdPoolParams));

    // Command buffer
    const VkCommandBufferAllocateInfo cmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // sType;
        nullptr,                                        // pNext;
        *cmdPool,                                       // pool;
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,                // level;
        1u,                                             // bufferCount;
    };
    const Unique<VkCommandBuffer> cmdBuf(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

    const Unique<VkEvent> event(createEvent(vk, vkDevice));

    // Put the command buffer in recording state.
    beginCommandBuffer(vk, *cmdBuf, 0u);
    {
        // Set the event
        vk.cmdSetEvent(*cmdBuf, *event, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    }
    endCommandBuffer(vk, *cmdBuf);

    submitCommandsAndWait(vk, vkDevice, queue, cmdBuf.get());

    // Check if the buffer was executed
    if (vk.getEventStatus(vkDevice, *event) != VK_EVENT_SET)
        return tcu::TestStatus::fail("Failed to set the event.");

    // Reset the event
    vk.resetEvent(vkDevice, *event);
    if (vk.getEventStatus(vkDevice, *event) != VK_EVENT_RESET)
        return tcu::TestStatus::fail("Failed to reset the event.");

    // Reset the command buffer by putting it in recording state again. This
    // should empty the command buffer.
    beginCommandBuffer(vk, *cmdBuf, 0u);
    endCommandBuffer(vk, *cmdBuf);

    // Submit the command buffer after resetting. It should have no commands
    // recorded, so the event should remain unsignaled.
    submitCommandsAndWait(vk, vkDevice, queue, cmdBuf.get());

    // Check if the event remained unset.
    if (vk.getEventStatus(vkDevice, *event) == VK_EVENT_RESET)
        return tcu::TestStatus::pass("Buffer was reset correctly.");
    else
        return tcu::TestStatus::fail("Buffer was not reset correctly.");
}

#ifndef CTS_USES_VULKANSC

using de::SharedPtr;
typedef SharedPtr<Unique<VkEvent>> VkEventShared;

template <typename T>
inline SharedPtr<Unique<T>> makeSharedPtr(Move<T> move)
{
    return SharedPtr<Unique<T>>(new Unique<T>(move));
}

bool submitAndCheck(Context &context, std::vector<VkCommandBuffer> &cmdBuffers, std::vector<VkEventShared> &events)
{
    const VkDevice vkDevice   = context.getDevice();
    const DeviceInterface &vk = context.getDeviceInterface();
    const VkQueue queue       = context.getUniversalQueue();
    const Unique<VkFence> fence(createFence(vk, vkDevice));

    const VkSubmitInfo submitInfo = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO,            // sType
        nullptr,                                  // pNext
        0u,                                       // waitSemaphoreCount
        nullptr,                                  // pWaitSemaphores
        nullptr,                                  // pWaitDstStageMask
        static_cast<uint32_t>(cmdBuffers.size()), // commandBufferCount
        &cmdBuffers[0],                           // pCommandBuffers
        0u,                                       // signalSemaphoreCount
        nullptr,                                  // pSignalSemaphores
    };

    VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo, fence.get()));
    VK_CHECK(vk.waitForFences(vkDevice, 1u, &fence.get(), 0u, INFINITE_TIMEOUT));

    for (int eventNdx = 0; eventNdx < static_cast<int>(events.size()); ++eventNdx)
    {
        if (vk.getEventStatus(vkDevice, **events[eventNdx]) != VK_EVENT_SET)
            return false;
        vk.resetEvent(vkDevice, **events[eventNdx]);
    }

    return true;
}

void createCommadBuffers(const DeviceInterface &vk, const VkDevice vkDevice, uint32_t bufferCount, VkCommandPool pool,
                         const VkCommandBufferLevel cmdBufferLevel, VkCommandBuffer *pCommandBuffers)
{
    const VkCommandBufferAllocateInfo cmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        pool,                                           // VkCommandPool pool;
        cmdBufferLevel,                                 // VkCommandBufferLevel level;
        bufferCount,                                    // uint32_t bufferCount;
    };
    VK_CHECK(vk.allocateCommandBuffers(vkDevice, &cmdBufParams, pCommandBuffers));
}

void addCommandsToBuffer(const DeviceInterface &vk, std::vector<VkCommandBuffer> &cmdBuffers,
                         std::vector<VkEventShared> &events)
{
    const VkCommandBufferInheritanceInfo secCmdBufInheritInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
        nullptr,
        VK_NULL_HANDLE,                    // renderPass
        0u,                                // subpass
        VK_NULL_HANDLE,                    // framebuffer
        VK_FALSE,                          // occlusionQueryEnable
        (VkQueryControlFlags)0u,           // queryFlags
        (VkQueryPipelineStatisticFlags)0u, // pipelineStatistics
    };

    const VkCommandBufferBeginInfo cmdBufBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // sType
        nullptr,                                     // pNext
        0u,                                          // flags
        &secCmdBufInheritInfo,                       // pInheritanceInfo;
    };

    for (int bufferNdx = 0; bufferNdx < static_cast<int>(cmdBuffers.size()); ++bufferNdx)
    {
        VK_CHECK(vk.beginCommandBuffer(cmdBuffers[bufferNdx], &cmdBufBeginInfo));
        vk.cmdSetEvent(cmdBuffers[bufferNdx], **events[bufferNdx % events.size()], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        endCommandBuffer(vk, cmdBuffers[bufferNdx]);
    }
}

bool executeSecondaryCmdBuffer(Context &context, VkCommandPool pool, std::vector<VkCommandBuffer> &cmdBuffersSecondary,
                               std::vector<VkEventShared> &events)
{
    const VkDevice vkDevice   = context.getDevice();
    const DeviceInterface &vk = context.getDeviceInterface();
    std::vector<VkCommandBuffer> cmdBuffer(1);

    createCommadBuffers(vk, vkDevice, 1u, pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, &cmdBuffer[0]);
    beginCommandBuffer(vk, cmdBuffer[0], 0u);
    vk.cmdExecuteCommands(cmdBuffer[0], static_cast<uint32_t>(cmdBuffersSecondary.size()), &cmdBuffersSecondary[0]);
    endCommandBuffer(vk, cmdBuffer[0]);

    bool returnValue = submitAndCheck(context, cmdBuffer, events);
    vk.freeCommandBuffers(vkDevice, pool, 1u, &cmdBuffer[0]);
    return returnValue;
}

tcu::TestStatus trimCommandPoolTest(Context &context, const VkCommandBufferLevel cmdBufferLevel)
{
    if (!context.isDeviceFunctionalitySupported("VK_KHR_maintenance1"))
        TCU_THROW(NotSupportedError, "Extension VK_KHR_maintenance1 not supported");

    const VkDevice vkDevice         = context.getDevice();
    const DeviceInterface &vk       = context.getDeviceInterface();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();

    //test parameters
    const uint32_t cmdBufferIterationCount = 300u;
    const uint32_t cmdBufferCount          = 10u;

    const VkCommandPoolCreateInfo cmdPoolParams = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,      // sType;
        nullptr,                                         // pNext;
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // flags;
        queueFamilyIndex,                                // queueFamilyIndex;
    };
    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, vkDevice, &cmdPoolParams));

    std::vector<VkEventShared> events;
    for (uint32_t ndx = 0u; ndx < cmdBufferCount; ++ndx)
        events.push_back(makeSharedPtr(createEvent(vk, vkDevice)));

    {
        std::vector<VkCommandBuffer> cmdBuffers(cmdBufferCount);
        createCommadBuffers(vk, vkDevice, cmdBufferCount, *cmdPool, cmdBufferLevel, &cmdBuffers[0]);

        for (uint32_t cmdBufferIterationrNdx = 0; cmdBufferIterationrNdx < cmdBufferIterationCount;
             ++cmdBufferIterationrNdx)
        {
            addCommandsToBuffer(vk, cmdBuffers, events);

            //Peak, situation when we use a lot more command buffers
            if (cmdBufferIterationrNdx % 10u == 0)
            {
                std::vector<VkCommandBuffer> cmdBuffersPeak(cmdBufferCount * 10u);
                createCommadBuffers(vk, vkDevice, static_cast<uint32_t>(cmdBuffersPeak.size()), *cmdPool,
                                    cmdBufferLevel, &cmdBuffersPeak[0]);
                addCommandsToBuffer(vk, cmdBuffersPeak, events);

                switch (cmdBufferLevel)
                {
                case VK_COMMAND_BUFFER_LEVEL_PRIMARY:
                    if (!submitAndCheck(context, cmdBuffersPeak, events))
                        return tcu::TestStatus::fail("Fail");
                    break;
                case VK_COMMAND_BUFFER_LEVEL_SECONDARY:
                    if (!executeSecondaryCmdBuffer(context, *cmdPool, cmdBuffersPeak, events))
                        return tcu::TestStatus::fail("Fail");
                    break;
                default:
                    DE_ASSERT(0);
                }
                vk.freeCommandBuffers(vkDevice, *cmdPool, static_cast<uint32_t>(cmdBuffersPeak.size()),
                                      &cmdBuffersPeak[0]);
            }

            vk.trimCommandPool(vkDevice, *cmdPool, (VkCommandPoolTrimFlags)0);

            switch (cmdBufferLevel)
            {
            case VK_COMMAND_BUFFER_LEVEL_PRIMARY:
                if (!submitAndCheck(context, cmdBuffers, events))
                    return tcu::TestStatus::fail("Fail");
                break;
            case VK_COMMAND_BUFFER_LEVEL_SECONDARY:
                if (!executeSecondaryCmdBuffer(context, *cmdPool, cmdBuffers, events))
                    return tcu::TestStatus::fail("Fail");
                break;
            default:
                DE_ASSERT(0);
            }

            for (uint32_t bufferNdx = cmdBufferIterationrNdx % 3u; bufferNdx < cmdBufferCount; bufferNdx += 2u)
            {
                vk.freeCommandBuffers(vkDevice, *cmdPool, 1u, &cmdBuffers[bufferNdx]);
                createCommadBuffers(vk, vkDevice, 1u, *cmdPool, cmdBufferLevel, &cmdBuffers[bufferNdx]);
            }
        }
    }

    return tcu::TestStatus::pass("Pass");
}

#endif // CTS_USES_VULKANSC

/******** 19.3. Command Buffer Recording (5.3 in VK 1.0 Spec) *****************/
tcu::TestStatus recordSinglePrimaryBufferTest(Context &context)
{
    const VkDevice vkDevice         = context.getDevice();
    const DeviceInterface &vk       = context.getDeviceInterface();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();

    const VkCommandPoolCreateInfo cmdPoolParams = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,      // VkStructureType sType;
        nullptr,                                         // const void* pNext;
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // VkCommandPoolCreateFlags flags;
        queueFamilyIndex,                                // uint32_t queueFamilyIndex;
    };
    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, vkDevice, &cmdPoolParams));

    // Command buffer
    const VkCommandBufferAllocateInfo cmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        *cmdPool,                                       // VkCommandPool pool;
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,                // VkCommandBufferLevel level;
        1u,                                             // uint32_t bufferCount;
    };
    const Unique<VkCommandBuffer> primCmdBuf(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

    // create event that will be used to check if secondary command buffer has been executed
    const Unique<VkEvent> event(createEvent(vk, vkDevice));

    // record primary command buffer
    beginCommandBuffer(vk, *primCmdBuf, 0u);
    {
        // record setting event
        vk.cmdSetEvent(*primCmdBuf, *event, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    }
    endCommandBuffer(vk, *primCmdBuf);

    return tcu::TestStatus::pass("Primary buffer recorded successfully.");
}

tcu::TestStatus recordLargePrimaryBufferTest(Context &context)
{
    const VkDevice vkDevice         = context.getDevice();
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkQueue queue             = context.getUniversalQueue();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();

    const VkCommandPoolCreateInfo cmdPoolParams = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,      // VkStructureType sType;
        nullptr,                                         // const void* pNext;
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // VkCommandPoolCreateFlags flags;
        queueFamilyIndex,                                // uint32_t queueFamilyIndex;
    };
    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, vkDevice, &cmdPoolParams));

    // Command buffer
    const VkCommandBufferAllocateInfo cmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        *cmdPool,                                       // VkCommandPool pool;
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,                // VkCommandBufferLevel level;
        1u,                                             // uint32_t bufferCount;
    };
    const Unique<VkCommandBuffer> primCmdBuf(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

    // create event that will be used to check if secondary command buffer has been executed
    const Unique<VkEvent> event(createEvent(vk, vkDevice));

    // reset event
    VK_CHECK(vk.resetEvent(vkDevice, *event));

    // record primary command buffer
    beginCommandBuffer(vk, *primCmdBuf, 0u);
    {
        // allow execution of event during every stage of pipeline
        VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

        // define minimal amount of commands to accept
#ifndef CTS_USES_VULKANSC
        const long long unsigned minNumCommands = 10000llu;
#else
        const long long unsigned minNumCommands = 1000llu;
#endif // CTS_USES_VULKANSC

        for (long long unsigned currentCommands = 0; currentCommands < minNumCommands / 2; ++currentCommands)
        {
            // record setting event
            vk.cmdSetEvent(*primCmdBuf, *event, stageMask);

            // record resetting event
            vk.cmdResetEvent(*primCmdBuf, *event, stageMask);
        }
    }
    endCommandBuffer(vk, *primCmdBuf);

    submitCommandsAndWait(vk, vkDevice, queue, primCmdBuf.get());

    return tcu::TestStatus::pass("hugeTest succeeded");
}

tcu::TestStatus recordSingleSecondaryBufferTest(Context &context)
{
    const VkDevice vkDevice         = context.getDevice();
    const DeviceInterface &vk       = context.getDeviceInterface();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();

    const VkCommandPoolCreateInfo cmdPoolParams = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,      // VkStructureType sType;
        nullptr,                                         // const void* pNext;
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // VkCommandPoolCreateFlags flags;
        queueFamilyIndex,                                // uint32_t queueFamilyIndex;
    };
    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, vkDevice, &cmdPoolParams));

    // Command buffer
    const VkCommandBufferAllocateInfo cmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        *cmdPool,                                       // VkCommandPool pool;
        VK_COMMAND_BUFFER_LEVEL_SECONDARY,              // VkCommandBufferLevel level;
        1u,                                             // uint32_t bufferCount;
    };
    const Unique<VkCommandBuffer> secCmdBuf(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

    const VkCommandBufferInheritanceInfo secCmdBufInheritInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
        nullptr,
        VK_NULL_HANDLE,                    // renderPass
        0u,                                // subpass
        VK_NULL_HANDLE,                    // framebuffer
        VK_FALSE,                          // occlusionQueryEnable
        (VkQueryControlFlags)0u,           // queryFlags
        (VkQueryPipelineStatisticFlags)0u, // pipelineStatistics
    };
    const VkCommandBufferBeginInfo secCmdBufBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        nullptr,
        0, // flags
        &secCmdBufInheritInfo,
    };

    // create event that will be used to check if secondary command buffer has been executed
    const Unique<VkEvent> event(createEvent(vk, vkDevice));

    // record primary command buffer
    VK_CHECK(vk.beginCommandBuffer(*secCmdBuf, &secCmdBufBeginInfo));
    {
        // record setting event
        vk.cmdSetEvent(*secCmdBuf, *event, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    }
    endCommandBuffer(vk, *secCmdBuf);

    return tcu::TestStatus::pass("Secondary buffer recorded successfully.");
}

tcu::TestStatus recordLargeSecondaryBufferTest(Context &context)
{
    const VkDevice vkDevice         = context.getDevice();
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkQueue queue             = context.getUniversalQueue();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();

    const VkCommandPoolCreateInfo cmdPoolParams = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,      // VkStructureType sType;
        nullptr,                                         // const void* pNext;
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // VkCommandPoolCreateFlags flags;
        queueFamilyIndex,                                // uint32_t queueFamilyIndex;
    };
    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, vkDevice, &cmdPoolParams));
    const Unique<VkCommandPool> secCmdPool(createCommandPool(vk, vkDevice, &cmdPoolParams));
    // Command buffer
    const VkCommandBufferAllocateInfo cmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        *cmdPool,                                       // VkCommandPool pool;
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,                // VkCommandBufferLevel level;
        1u,                                             // uint32_t bufferCount;
    };
    const Unique<VkCommandBuffer> primCmdBuf(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

    const VkCommandBufferAllocateInfo secCmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        *secCmdPool,                                    // VkCommandPool pool;
        VK_COMMAND_BUFFER_LEVEL_SECONDARY,              // VkCommandBufferLevel level;
        1u,                                             // uint32_t bufferCount;
    };
    const Unique<VkCommandBuffer> secCmdBuf(allocateCommandBuffer(vk, vkDevice, &secCmdBufParams));

    const VkCommandBufferInheritanceInfo secCmdBufInheritInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
        nullptr,
        VK_NULL_HANDLE,                    // renderPass
        0u,                                // subpass
        VK_NULL_HANDLE,                    // framebuffer
        VK_FALSE,                          // occlusionQueryEnable
        (VkQueryControlFlags)0u,           // queryFlags
        (VkQueryPipelineStatisticFlags)0u, // pipelineStatistics
    };
    const VkCommandBufferBeginInfo secCmdBufBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        nullptr,
        0, // flags
        &secCmdBufInheritInfo,
    };

    // create event that will be used to check if secondary command buffer has been executed
    const Unique<VkEvent> event(createEvent(vk, vkDevice));

    // reset event
    VK_CHECK(vk.resetEvent(vkDevice, *event));

    // record primary command buffer
    beginCommandBuffer(vk, *primCmdBuf, 0u);
    {
        // record secondary command buffer
        VK_CHECK(vk.beginCommandBuffer(*secCmdBuf, &secCmdBufBeginInfo));
        {
            // allow execution of event during every stage of pipeline
            VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

            // define minimal amount of commands to accept
#ifndef CTS_USES_VULKANSC
            const long long unsigned minNumCommands = 10000llu;
#else
            const long long unsigned minNumCommands = 1000llu;
#endif // CTS_USES_VULKANSC

            for (long long unsigned currentCommands = 0; currentCommands < minNumCommands / 2; ++currentCommands)
            {
                // record setting event
                vk.cmdSetEvent(*secCmdBuf, *event, stageMask);

                // record resetting event
                vk.cmdResetEvent(*secCmdBuf, *event, stageMask);
            }
        }

        // end recording of secondary buffers
        endCommandBuffer(vk, *secCmdBuf);

        // execute secondary buffer
        vk.cmdExecuteCommands(*primCmdBuf, 1, &secCmdBuf.get());
    }
    endCommandBuffer(vk, *primCmdBuf);

    submitCommandsAndWait(vk, vkDevice, queue, primCmdBuf.get());

    return tcu::TestStatus::pass("hugeTest succeeded");
}

tcu::TestStatus submitPrimaryBufferTwiceTest(Context &context)
{
    const VkDevice vkDevice         = context.getDevice();
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkQueue queue             = context.getUniversalQueue();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();

    const VkCommandPoolCreateInfo cmdPoolParams = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,      // VkStructureType sType;
        nullptr,                                         // const void* pNext;
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // VkCommandPoolCreateFlags flags;
        queueFamilyIndex,                                // uint32_t queueFamilyIndex;
    };
    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, vkDevice, &cmdPoolParams));

    // Command buffer
    const VkCommandBufferAllocateInfo cmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        *cmdPool,                                       // VkCommandPool pool;
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,                // VkCommandBufferLevel level;
        1u,                                             // uint32_t bufferCount;
    };
    const Unique<VkCommandBuffer> primCmdBuf(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

    // create event that will be used to check if secondary command buffer has been executed
    const Unique<VkEvent> event(createEvent(vk, vkDevice));

    // reset event
    VK_CHECK(vk.resetEvent(vkDevice, *event));

    // record primary command buffer
    beginCommandBuffer(vk, *primCmdBuf, 0u);
    {
        // allow execution of event during every stage of pipeline
        VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

        // record setting event
        vk.cmdSetEvent(*primCmdBuf, *event, stageMask);
    }
    endCommandBuffer(vk, *primCmdBuf);

    submitCommandsAndWait(vk, vkDevice, queue, primCmdBuf.get());

    // check if buffer has been executed
    VkResult result = vk.getEventStatus(vkDevice, *event);
    if (result != VK_EVENT_SET)
        return tcu::TestStatus::fail("Submit Twice Test FAILED");

    // reset event
    VK_CHECK(vk.resetEvent(vkDevice, *event));

    submitCommandsAndWait(vk, vkDevice, queue, primCmdBuf.get());

    // check if buffer has been executed
    result = vk.getEventStatus(vkDevice, *event);
    if (result != VK_EVENT_SET)
        return tcu::TestStatus::fail("Submit Twice Test FAILED");
    else
        return tcu::TestStatus::pass("Submit Twice Test succeeded");
}

tcu::TestStatus submitSecondaryBufferTwiceTest(Context &context)
{
    const VkDevice vkDevice         = context.getDevice();
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkQueue queue             = context.getUniversalQueue();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();

#ifdef CTS_USES_VULKANSC
    if (context.getDeviceVulkanSC10Properties().commandPoolResetCommandBuffer == VK_FALSE)
        TCU_THROW(NotSupportedError, "commandPoolResetCommandBuffer not supported by this implementation");
#endif // CTS_USES_VULKANSC

    const VkCommandPoolCreateInfo cmdPoolParams = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,      // VkStructureType sType;
        nullptr,                                         // const void* pNext;
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // VkCommandPoolCreateFlags flags;
        queueFamilyIndex,                                // uint32_t queueFamilyIndex;
    };

    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, vkDevice, &cmdPoolParams));
    const Unique<VkCommandPool> secCmdPool(createCommandPool(vk, vkDevice, &cmdPoolParams));

    // Command buffer
    const VkCommandBufferAllocateInfo cmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        *cmdPool,                                       // VkCommandPool pool;
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,                // VkCommandBufferLevel level;
        1u,                                             // uint32_t bufferCount;
    };

    const Unique<VkCommandBuffer> primCmdBuf1(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));
    const Unique<VkCommandBuffer> primCmdBuf2(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

    // Secondary Command buffer
    const VkCommandBufferAllocateInfo secCmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        *secCmdPool,                                    // VkCommandPool pool;
        VK_COMMAND_BUFFER_LEVEL_SECONDARY,              // VkCommandBufferLevel level;
        1u,                                             // uint32_t bufferCount;
    };
    const Unique<VkCommandBuffer> secCmdBuf(allocateCommandBuffer(vk, vkDevice, &secCmdBufParams));

    const VkCommandBufferInheritanceInfo secCmdBufInheritInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
        nullptr,
        VK_NULL_HANDLE,                    // renderPass
        0u,                                // subpass
        VK_NULL_HANDLE,                    // framebuffer
        VK_FALSE,                          // occlusionQueryEnable
        (VkQueryControlFlags)0u,           // queryFlags
        (VkQueryPipelineStatisticFlags)0u, // pipelineStatistics
    };
    const VkCommandBufferBeginInfo secCmdBufBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        nullptr,
        0u, // flags
        &secCmdBufInheritInfo,
    };

    // create event that will be used to check if secondary command buffer has been executed
    const Unique<VkEvent> event(createEvent(vk, vkDevice));

    // reset event
    VK_CHECK(vk.resetEvent(vkDevice, *event));

    // record first primary command buffer
    beginCommandBuffer(vk, *primCmdBuf1, 0u);
    {
        // record secondary command buffer
        VK_CHECK(vk.beginCommandBuffer(*secCmdBuf, &secCmdBufBeginInfo));
        {
            // allow execution of event during every stage of pipeline
            VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

            // record setting event
            vk.cmdSetEvent(*secCmdBuf, *event, stageMask);
        }

        // end recording of secondary buffers
        endCommandBuffer(vk, *secCmdBuf);

        // execute secondary buffer
        vk.cmdExecuteCommands(*primCmdBuf1, 1, &secCmdBuf.get());
    }
    endCommandBuffer(vk, *primCmdBuf1);

    submitCommandsAndWait(vk, vkDevice, queue, primCmdBuf1.get());

    // check if secondary buffer has been executed
    VkResult result = vk.getEventStatus(vkDevice, *event);
    if (result != VK_EVENT_SET)
        return tcu::TestStatus::fail("Submit Twice Secondary Command Buffer FAILED");

    // reset first primary buffer
    VK_CHECK(vk.resetCommandBuffer(*primCmdBuf1, 0u));

    // reset event to allow receiving it again
    VK_CHECK(vk.resetEvent(vkDevice, *event));

    // record second primary command buffer
    beginCommandBuffer(vk, *primCmdBuf2, 0u);
    {
        // execute secondary buffer
        vk.cmdExecuteCommands(*primCmdBuf2, 1, &secCmdBuf.get());
    }
    // end recording
    endCommandBuffer(vk, *primCmdBuf2);

    submitCommandsAndWait(vk, vkDevice, queue, primCmdBuf2.get());

    // check if secondary buffer has been executed
    result = vk.getEventStatus(vkDevice, *event);
    if (result != VK_EVENT_SET)
        return tcu::TestStatus::fail("Submit Twice Secondary Command Buffer FAILED");
    else
        return tcu::TestStatus::pass("Submit Twice Secondary Command Buffer succeeded");
}

tcu::TestStatus oneTimeSubmitFlagPrimaryBufferTest(Context &context)
{
    const VkDevice vkDevice         = context.getDevice();
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkQueue queue             = context.getUniversalQueue();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();

#ifdef CTS_USES_VULKANSC
    if (context.getDeviceVulkanSC10Properties().commandPoolResetCommandBuffer == VK_FALSE)
        TCU_THROW(NotSupportedError, "commandPoolResetCommandBuffer not supported by this implementation");
#endif // CTS_USES_VULKANSC

    const VkCommandPoolCreateInfo cmdPoolParams = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,      // VkStructureType sType;
        nullptr,                                         // const void* pNext;
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // VkCommandPoolCreateFlags flags;
        queueFamilyIndex,                                // uint32_t queueFamilyIndex;
    };
    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, vkDevice, &cmdPoolParams));

    // Command buffer
    const VkCommandBufferAllocateInfo cmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        *cmdPool,                                       // VkCommandPool pool;
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,                // VkCommandBufferLevel level;
        1u,                                             // uint32_t bufferCount;
    };
    const Unique<VkCommandBuffer> primCmdBuf(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

    // create event that will be used to check if secondary command buffer has been executed
    const Unique<VkEvent> event(createEvent(vk, vkDevice));

    // reset event
    VK_CHECK(vk.resetEvent(vkDevice, *event));

    // record primary command buffer
    beginCommandBuffer(vk, *primCmdBuf);
    {
        // allow execution of event during every stage of pipeline
        VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

        // record setting event
        vk.cmdSetEvent(*primCmdBuf, *event, stageMask);
    }
    endCommandBuffer(vk, *primCmdBuf);

    submitCommandsAndWait(vk, vkDevice, queue, primCmdBuf.get());

    // check if buffer has been executed
    VkResult result = vk.getEventStatus(vkDevice, *event);
    if (result != VK_EVENT_SET)
        return tcu::TestStatus::fail("oneTimeSubmitFlagPrimaryBufferTest FAILED");

    // record primary command buffer again - implicit reset because of VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
    beginCommandBuffer(vk, *primCmdBuf);
    {
        // allow execution of event during every stage of pipeline
        VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

        // record setting event
        vk.cmdSetEvent(*primCmdBuf, *event, stageMask);
    }
    endCommandBuffer(vk, *primCmdBuf);

    submitCommandsAndWait(vk, vkDevice, queue, primCmdBuf.get());

    // check if buffer has been executed
    result = vk.getEventStatus(vkDevice, *event);
    if (result != VK_EVENT_SET)
        return tcu::TestStatus::fail("oneTimeSubmitFlagPrimaryBufferTest FAILED");
    else
        return tcu::TestStatus::pass("oneTimeSubmitFlagPrimaryBufferTest succeeded");
}

tcu::TestStatus oneTimeSubmitFlagSecondaryBufferTest(Context &context)
{
    const VkDevice vkDevice         = context.getDevice();
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkQueue queue             = context.getUniversalQueue();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();

#ifdef CTS_USES_VULKANSC
    if (context.getDeviceVulkanSC10Properties().commandPoolResetCommandBuffer == VK_FALSE)
        TCU_THROW(NotSupportedError, "commandPoolResetCommandBuffer not supported by this implementation");
#endif // CTS_USES_VULKANSC

    const VkCommandPoolCreateInfo cmdPoolParams = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,      // VkStructureType sType;
        nullptr,                                         // const void* pNext;
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // VkCommandPoolCreateFlags flags;
        queueFamilyIndex,                                // uint32_t queueFamilyIndex;
    };

    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, vkDevice, &cmdPoolParams));
    const Unique<VkCommandPool> secCmdPool(createCommandPool(vk, vkDevice, &cmdPoolParams));

    // Command buffer
    const VkCommandBufferAllocateInfo cmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        *cmdPool,                                       // VkCommandPool pool;
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,                // VkCommandBufferLevel level;
        1u,                                             // uint32_t bufferCount;
    };

    const Unique<VkCommandBuffer> primCmdBuf1(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));
    const Unique<VkCommandBuffer> primCmdBuf2(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

    // Secondary Command buffer
    const VkCommandBufferAllocateInfo secCmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        *secCmdPool,                                    // VkCommandPool pool;
        VK_COMMAND_BUFFER_LEVEL_SECONDARY,              // VkCommandBufferLevel level;
        1u,                                             // uint32_t bufferCount;
    };
    const Unique<VkCommandBuffer> secCmdBuf(allocateCommandBuffer(vk, vkDevice, &secCmdBufParams));

    const VkCommandBufferInheritanceInfo secCmdBufInheritInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
        nullptr,
        VK_NULL_HANDLE,                    // renderPass
        0u,                                // subpass
        VK_NULL_HANDLE,                    // framebuffer
        VK_FALSE,                          // occlusionQueryEnable
        (VkQueryControlFlags)0u,           // queryFlags
        (VkQueryPipelineStatisticFlags)0u, // pipelineStatistics
    };
    const VkCommandBufferBeginInfo secCmdBufBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        nullptr,
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, // flags
        &secCmdBufInheritInfo,
    };

    // create event that will be used to check if secondary command buffer has been executed
    const Unique<VkEvent> event(createEvent(vk, vkDevice));

    // reset event
    VK_CHECK(vk.resetEvent(vkDevice, *event));

    // record first primary command buffer
    beginCommandBuffer(vk, *primCmdBuf1, 0u);
    {
        // record secondary command buffer
        VK_CHECK(vk.beginCommandBuffer(*secCmdBuf, &secCmdBufBeginInfo));
        {
            // allow execution of event during every stage of pipeline
            VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

            // record setting event
            vk.cmdSetEvent(*secCmdBuf, *event, stageMask);
        }

        // end recording of secondary buffers
        endCommandBuffer(vk, *secCmdBuf);

        // execute secondary buffer
        vk.cmdExecuteCommands(*primCmdBuf1, 1, &secCmdBuf.get());
    }
    endCommandBuffer(vk, *primCmdBuf1);

    submitCommandsAndWait(vk, vkDevice, queue, primCmdBuf1.get());

    // check if secondary buffer has been executed
    VkResult result = vk.getEventStatus(vkDevice, *event);
    if (result != VK_EVENT_SET)
        return tcu::TestStatus::fail("Submit Twice Secondary Command Buffer FAILED");

    // reset first primary buffer
    VK_CHECK(vk.resetCommandBuffer(*primCmdBuf1, 0u));

    // reset event to allow receiving it again
    VK_CHECK(vk.resetEvent(vkDevice, *event));

    // record secondary command buffer again
    VK_CHECK(vk.beginCommandBuffer(*secCmdBuf, &secCmdBufBeginInfo));
    {
        // allow execution of event during every stage of pipeline
        VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

        // record setting event
        vk.cmdSetEvent(*secCmdBuf, *event, stageMask);
    }
    // end recording of secondary buffers
    endCommandBuffer(vk, *secCmdBuf);

    // record second primary command buffer
    beginCommandBuffer(vk, *primCmdBuf2, 0u);
    {
        // execute secondary buffer
        vk.cmdExecuteCommands(*primCmdBuf2, 1, &secCmdBuf.get());
    }
    // end recording
    endCommandBuffer(vk, *primCmdBuf2);

    submitCommandsAndWait(vk, vkDevice, queue, primCmdBuf2.get());

    // check if secondary buffer has been executed
    result = vk.getEventStatus(vkDevice, *event);
    if (result != VK_EVENT_SET)
        return tcu::TestStatus::fail("oneTimeSubmitFlagSecondaryBufferTest FAILED");
    else
        return tcu::TestStatus::pass("oneTimeSubmitFlagSecondaryBufferTest succeeded");
}

tcu::TestStatus renderPassContinueTest(Context &context, bool framebufferHint)
{
    const DeviceInterface &vkd = context.getDeviceInterface();
    CommandBufferRenderPassTestEnvironment env(context, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    VkCommandBuffer primaryCommandBuffer   = env.getPrimaryCommandBuffer();
    VkCommandBuffer secondaryCommandBuffer = env.getSecondaryCommandBuffer();
    const uint32_t clearColor[4]           = {2, 47, 131, 211};

    const VkClearAttachment clearAttachment = {
        VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
        0,                         // uint32_t colorAttachment;
        makeClearValueColorU32(clearColor[0], clearColor[1], clearColor[2],
                               clearColor[3]) // VkClearValue clearValue;
    };

    const VkClearRect clearRect = {
        CommandBufferRenderPassTestEnvironment::DEFAULT_IMAGE_AREA, // VkRect2D rect;
        0u,                                                         // uint32_t baseArrayLayer;
        1u                                                          // uint32_t layerCount;
    };

    env.beginSecondaryCommandBuffer(VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, framebufferHint);
    vkd.cmdClearAttachments(secondaryCommandBuffer, 1, &clearAttachment, 1, &clearRect);
    endCommandBuffer(vkd, secondaryCommandBuffer);

    env.beginPrimaryCommandBuffer(0);
    env.beginRenderPass(VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
    vkd.cmdExecuteCommands(primaryCommandBuffer, 1, &secondaryCommandBuffer);
    endRenderPass(vkd, primaryCommandBuffer);

    endCommandBuffer(vkd, primaryCommandBuffer);

    env.submitPrimaryCommandBuffer();
    context.resetCommandPoolForVKSC(context.getDevice(), env.getCommandPool());

    de::MovePtr<tcu::TextureLevel> result    = env.readColorAttachment();
    tcu::PixelBufferAccess pixelBufferAccess = result->getAccess();

    for (uint32_t i = 0; i < (CommandBufferRenderPassTestEnvironment::DEFAULT_IMAGE_SIZE.width *
                              CommandBufferRenderPassTestEnvironment::DEFAULT_IMAGE_SIZE.height);
         ++i)
    {
        uint8_t *colorData = reinterpret_cast<uint8_t *>(pixelBufferAccess.getDataPtr());
        for (int colorComponent = 0; colorComponent < 4; ++colorComponent)
            if (colorData[i * 4 + colorComponent] != clearColor[colorComponent])
                return tcu::TestStatus::fail("clear value mismatch");
    }

    return tcu::TestStatus::pass("render pass continue test passed");
}

tcu::TestStatus simultaneousUseSecondaryBufferOnePrimaryBufferTest(Context &context)
{
    const VkDevice vkDevice         = context.getDevice();
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkQueue queue             = context.getUniversalQueue();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = context.getDefaultAllocator();
    const ComputeInstanceResultBuffer result(vk, vkDevice, allocator, 0.0f);

    const VkCommandPoolCreateInfo cmdPoolParams = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,      // VkStructureType sType;
        nullptr,                                         // const void* pNext;
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // VkCommandPoolCreateFlags flags;
        queueFamilyIndex,                                // uint32_t queueFamilyIndex;
    };
    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, vkDevice, &cmdPoolParams));

    // Command buffer
    const VkCommandBufferAllocateInfo cmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        *cmdPool,                                       // VkCommandPool pool;
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,                // VkCommandBufferLevel level;
        1u,                                             // uint32_t bufferCount;
    };
    const Unique<VkCommandBuffer> primCmdBuf(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

    // Secondary Command buffer params
    const VkCommandBufferAllocateInfo secCmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        *cmdPool,                                       // VkCommandPool pool;
        VK_COMMAND_BUFFER_LEVEL_SECONDARY,              // VkCommandBufferLevel level;
        1u,                                             // uint32_t bufferCount;
    };
    const Unique<VkCommandBuffer> secCmdBuf(allocateCommandBuffer(vk, vkDevice, &secCmdBufParams));

    const VkCommandBufferInheritanceInfo secCmdBufInheritInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
        nullptr,
        VK_NULL_HANDLE,
        0u, // subpass
        VK_NULL_HANDLE,
        VK_FALSE, // occlusionQueryEnable
        (VkQueryControlFlags)0u,
        (VkQueryPipelineStatisticFlags)0u,
    };
    const VkCommandBufferBeginInfo secCmdBufBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        nullptr,
        VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT, // flags
        &secCmdBufInheritInfo,
    };

    const uint32_t offset          = (0u);
    const uint32_t addressableSize = 256;
    const uint32_t dataSize        = 8;
    de::MovePtr<Allocation> bufferMem;
    const Unique<VkBuffer> buffer(createDataBuffer(context, offset, addressableSize, 0x00, dataSize, 0x5A, &bufferMem));
    // Secondary command buffer will have a compute shader that does an atomic increment to make sure that all instances of secondary buffers execute
    const Unique<VkDescriptorSetLayout> descriptorSetLayout(createDescriptorSetLayout(context));
    const Unique<VkDescriptorPool> descriptorPool(createDescriptorPool(context));
    const Unique<VkDescriptorSet> descriptorSet(
        createDescriptorSet(context, *descriptorPool, *descriptorSetLayout, *buffer, offset, result.getBuffer()));
    const VkDescriptorSet descriptorSets[] = {*descriptorSet};
    const int numDescriptorSets            = DE_LENGTH_OF_ARRAY(descriptorSets);

    const VkPipelineLayoutCreateInfo layoutCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // sType
        nullptr,                                       // pNext
        (VkPipelineLayoutCreateFlags)0,
        numDescriptorSets,          // setLayoutCount
        &descriptorSetLayout.get(), // pSetLayouts
        0u,                         // pushConstantRangeCount
        nullptr,                    // pPushConstantRanges
    };
    Unique<VkPipelineLayout> pipelineLayout(createPipelineLayout(vk, vkDevice, &layoutCreateInfo));

    const Unique<VkShaderModule> computeModule(createShaderModule(
        vk, vkDevice, context.getBinaryCollection().get("compute_increment"), (VkShaderModuleCreateFlags)0u));

    const VkPipelineShaderStageCreateInfo shaderCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        nullptr,
        (VkPipelineShaderStageCreateFlags)0,
        VK_SHADER_STAGE_COMPUTE_BIT, // stage
        *computeModule,              // shader
        "main",
        nullptr, // pSpecializationInfo
    };

    const VkComputePipelineCreateInfo pipelineCreateInfo = {
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        nullptr,
        0u,               // flags
        shaderCreateInfo, // cs
        *pipelineLayout,  // layout
        VK_NULL_HANDLE,   // basePipelineHandle
        0u,               // basePipelineIndex
    };

    const VkBufferMemoryBarrier bufferBarrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // sType
        nullptr,                                 // pNext
        VK_ACCESS_SHADER_WRITE_BIT,              // srcAccessMask
        VK_ACCESS_HOST_READ_BIT,                 // dstAccessMask
        VK_QUEUE_FAMILY_IGNORED,                 // srcQueueFamilyIndex
        VK_QUEUE_FAMILY_IGNORED,                 // destQueueFamilyIndex
        *buffer,                                 // buffer
        (VkDeviceSize)0u,                        // offset
        (VkDeviceSize)VK_WHOLE_SIZE,             // size
    };

    const Unique<VkPipeline> pipeline(createComputePipeline(vk, vkDevice, VK_NULL_HANDLE, &pipelineCreateInfo));

    // record secondary command buffer
    VK_CHECK(vk.beginCommandBuffer(*secCmdBuf, &secCmdBufBeginInfo));
    {
        vk.cmdBindPipeline(*secCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
        vk.cmdBindDescriptorSets(*secCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0, numDescriptorSets,
                                 descriptorSets, 0, 0);
        vk.cmdDispatch(*secCmdBuf, 1u, 1u, 1u);
        vk.cmdPipelineBarrier(*secCmdBuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                              (VkDependencyFlags)0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);
    }
    // end recording of secondary buffer
    endCommandBuffer(vk, *secCmdBuf);

    // record primary command buffer
    beginCommandBuffer(vk, *primCmdBuf, 0u);
    {
        // execute secondary buffer twice in same primary
        vk.cmdExecuteCommands(*primCmdBuf, 1, &secCmdBuf.get());
        vk.cmdExecuteCommands(*primCmdBuf, 1, &secCmdBuf.get());
    }
    endCommandBuffer(vk, *primCmdBuf);

    submitCommandsAndWait(vk, vkDevice, queue, primCmdBuf.get());

    uint32_t resultCount;
    result.readResultContentsTo(&resultCount);
    // check if secondary buffer has been executed
    if (resultCount == 2)
        return tcu::TestStatus::pass("Simultaneous Secondary Command Buffer Execution succeeded");
    else
        return tcu::TestStatus::fail("Simultaneous Secondary Command Buffer Execution FAILED");
}

tcu::TestStatus renderPassContinueNestedTest(Context &context, bool framebufferHint)
{
    bool maintenance7 = false;
#ifndef CTS_USES_VULKANSC
    if (context.isDeviceFunctionalitySupported("VK_KHR_maintenance7"))
    {
        const auto &features = context.getMaintenance7Features();
        maintenance7         = features.maintenance7;
    }
#endif

    if (!maintenance7)
    {
        context.requireDeviceFunctionality("VK_EXT_nested_command_buffer");
#ifndef CTS_USES_VULKANSC
        const auto &features = context.getNestedCommandBufferFeaturesEXT();
        if (!features.nestedCommandBuffer)
#endif // CTS_USES_VULKANSC
            TCU_THROW(NotSupportedError, "nestedCommandBuffer is not supported");
#ifndef CTS_USES_VULKANSC
        if (!features.nestedCommandBufferRendering)
#endif // CTS_USES_VULKANSC
            TCU_THROW(NotSupportedError, "nestedCommandBufferRendering is not supported");
    }

    const DeviceInterface &vkd = context.getDeviceInterface();
    CommandBufferRenderPassTestEnvironment env(context, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    VkCommandBuffer primaryCommandBuffer   = env.getPrimaryCommandBuffer();
    VkCommandBuffer secondaryCommandBuffer = env.getSecondaryCommandBuffer();
    VkCommandBuffer nestedCommandBuffer    = env.getNestedCommandBuffer();
    const uint32_t clearColor[4]           = {2, 47, 131, 211};

    const VkClearAttachment clearAttachment = {
        VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
        0,                         // uint32_t colorAttachment;
        makeClearValueColorU32(clearColor[0], clearColor[1], clearColor[2],
                               clearColor[3]) // VkClearValue clearValue;
    };

    const uint32_t clearRectWidth  = CommandBufferRenderPassTestEnvironment::DEFAULT_IMAGE_SIZE.width / 2;
    const uint32_t clearRectHeight = CommandBufferRenderPassTestEnvironment::DEFAULT_IMAGE_SIZE.height / 2;
    const int32_t clearRectOffsetX = clearRectWidth;
    const int32_t clearRectOffsetY = clearRectHeight;

    const VkRect2D clearRectArea[4] = {{
                                           {0u, 0u},                          // VkOffset2D offset;
                                           {clearRectWidth, clearRectHeight}, // VkExtent2D extent;
                                       },
                                       {
                                           {0u, clearRectOffsetY},                // VkOffset2D offset;
                                           {clearRectWidth, clearRectHeight + 1}, // VkExtent2D extent;
                                       },
                                       {
                                           {clearRectOffsetX, 0u},                // VkOffset2D offset;
                                           {clearRectWidth + 1, clearRectHeight}, // VkExtent2D extent;
                                       },
                                       {
                                           {clearRectOffsetX, clearRectOffsetY},      // VkOffset2D offset;
                                           {clearRectWidth + 1, clearRectHeight + 1}, // VkExtent2D extent;
                                       }};

    const VkClearRect clearRect[4] = {{
                                          clearRectArea[0], // VkRect2D rect;
                                          0u,               // uint32_t baseArrayLayer;
                                          1u                // uint32_t layerCount;
                                      },
                                      {
                                          clearRectArea[1], // VkRect2D rect;
                                          0u,               // uint32_t baseArrayLayer;
                                          1u                // uint32_t layerCount;
                                      },
                                      {
                                          clearRectArea[2], // VkRect2D rect;
                                          0u,               // uint32_t baseArrayLayer;
                                          1u                // uint32_t layerCount;
                                      },
                                      {
                                          clearRectArea[3], // VkRect2D rect;
                                          0u,               // uint32_t baseArrayLayer;
                                          1u                // uint32_t layerCount;
                                      }};

    env.beginSecondaryCommandBuffer(VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, framebufferHint);
    vkd.cmdClearAttachments(secondaryCommandBuffer, 1, &clearAttachment, 1, &clearRect[0]);
    endCommandBuffer(vkd, secondaryCommandBuffer);

    env.beginNestedCommandBuffer(VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, framebufferHint);
    vkd.cmdExecuteCommands(nestedCommandBuffer, 1, &secondaryCommandBuffer);
    vkd.cmdClearAttachments(nestedCommandBuffer, 1, &clearAttachment, 1, &clearRect[1]);
    endCommandBuffer(vkd, nestedCommandBuffer);

    env.beginPrimaryCommandBuffer(0);
#ifndef CTS_USES_VULKANSC
    env.beginRenderPass(VK_SUBPASS_CONTENTS_INLINE_AND_SECONDARY_COMMAND_BUFFERS_EXT);
#endif // CTS_USES_VULKANSC
    vkd.cmdClearAttachments(primaryCommandBuffer, 1, &clearAttachment, 1, &clearRect[2]);
    vkd.cmdExecuteCommands(primaryCommandBuffer, 1, &nestedCommandBuffer);
    vkd.cmdClearAttachments(primaryCommandBuffer, 1, &clearAttachment, 1, &clearRect[3]);
    endRenderPass(vkd, primaryCommandBuffer);

    endCommandBuffer(vkd, primaryCommandBuffer);

    env.submitPrimaryCommandBuffer();
    context.resetCommandPoolForVKSC(context.getDevice(), env.getCommandPool());

    de::MovePtr<tcu::TextureLevel> result    = env.readColorAttachment();
    tcu::PixelBufferAccess pixelBufferAccess = result->getAccess();

    for (uint32_t i = 0; i < (CommandBufferRenderPassTestEnvironment::DEFAULT_IMAGE_SIZE.width *
                              CommandBufferRenderPassTestEnvironment::DEFAULT_IMAGE_SIZE.height);
         ++i)
    {
        uint8_t *colorData = reinterpret_cast<uint8_t *>(pixelBufferAccess.getDataPtr());
        for (int colorComponent = 0; colorComponent < 4; ++colorComponent)
            if (colorData[i * 4 + colorComponent] != clearColor[colorComponent])
                return tcu::TestStatus::fail("clear value mismatch");
    }

    return tcu::TestStatus::pass("render pass continue in nested command buffer test passed");
}

tcu::TestStatus simultaneousUseNestedSecondaryBufferTest(Context &context)
{
    const VkDevice vkDevice         = context.getDevice();
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkQueue queue             = context.getUniversalQueue();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = context.getDefaultAllocator();
    const ComputeInstanceResultBuffer result(vk, vkDevice, allocator, 0.0f);

    const VkCommandPoolCreateInfo cmdPoolParams = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,      // VkStructureType sType;
        nullptr,                                         // const void* pNext;
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // VkCommandPoolCreateFlags flags;
        queueFamilyIndex,                                // uint32_t queueFamilyIndex;
    };
    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, vkDevice, &cmdPoolParams));

    // Command buffer
    const VkCommandBufferAllocateInfo cmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        *cmdPool,                                       // VkCommandPool pool;
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,                // VkCommandBufferLevel level;
        1u,                                             // uint32_t bufferCount;
    };
    const Unique<VkCommandBuffer> primCmdBuf(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

    // Secondary Command buffer params
    const VkCommandBufferAllocateInfo secCmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        *cmdPool,                                       // VkCommandPool pool;
        VK_COMMAND_BUFFER_LEVEL_SECONDARY,              // VkCommandBufferLevel level;
        1u,                                             // uint32_t bufferCount;
    };
    const Unique<VkCommandBuffer> secCmdBuf(allocateCommandBuffer(vk, vkDevice, &secCmdBufParams));
    const Unique<VkCommandBuffer> nestedCmdBuf(allocateCommandBuffer(vk, vkDevice, &secCmdBufParams));

    const VkCommandBufferInheritanceInfo secCmdBufInheritInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
        nullptr,
        VK_NULL_HANDLE,
        0u, // subpass
        VK_NULL_HANDLE,
        VK_FALSE, // occlusionQueryEnable
        (VkQueryControlFlags)0u,
        (VkQueryPipelineStatisticFlags)0u,
    };
    const VkCommandBufferBeginInfo secCmdBufBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        nullptr,
        VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT, // flags
        &secCmdBufInheritInfo,
    };

    const uint32_t offset          = (0u);
    const uint32_t addressableSize = 256;
    const uint32_t dataSize        = 8;
    de::MovePtr<Allocation> bufferMem;
    const Unique<VkBuffer> buffer(createDataBuffer(context, offset, addressableSize, 0x00, dataSize, 0x5A, &bufferMem));
    // Secondary command buffer will have a compute shader that does an atomic increment to make sure that all instances of secondary buffers execute
    const Unique<VkDescriptorSetLayout> descriptorSetLayout(createDescriptorSetLayout(context));
    const Unique<VkDescriptorPool> descriptorPool(createDescriptorPool(context));
    const Unique<VkDescriptorSet> descriptorSet(
        createDescriptorSet(context, *descriptorPool, *descriptorSetLayout, *buffer, offset, result.getBuffer()));
    const VkDescriptorSet descriptorSets[] = {*descriptorSet};
    const int numDescriptorSets            = DE_LENGTH_OF_ARRAY(descriptorSets);

    const VkPipelineLayoutCreateInfo layoutCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // sType
        nullptr,                                       // pNext
        (VkPipelineLayoutCreateFlags)0,
        numDescriptorSets,          // setLayoutCount
        &descriptorSetLayout.get(), // pSetLayouts
        0u,                         // pushConstantRangeCount
        nullptr,                    // pPushConstantRanges
    };
    Unique<VkPipelineLayout> pipelineLayout(createPipelineLayout(vk, vkDevice, &layoutCreateInfo));

    const Unique<VkShaderModule> computeModule(createShaderModule(
        vk, vkDevice, context.getBinaryCollection().get("compute_increment"), (VkShaderModuleCreateFlags)0u));

    const VkPipelineShaderStageCreateInfo shaderCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        nullptr,
        (VkPipelineShaderStageCreateFlags)0,
        VK_SHADER_STAGE_COMPUTE_BIT, // stage
        *computeModule,              // shader
        "main",
        nullptr, // pSpecializationInfo
    };

    const VkComputePipelineCreateInfo pipelineCreateInfo = {
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        nullptr,
        0u,               // flags
        shaderCreateInfo, // cs
        *pipelineLayout,  // layout
        VK_NULL_HANDLE,   // basePipelineHandle
        0u,               // basePipelineIndex
    };

    const VkBufferMemoryBarrier bufferBarrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // sType
        nullptr,                                 // pNext
        VK_ACCESS_SHADER_WRITE_BIT,              // srcAccessMask
        VK_ACCESS_HOST_READ_BIT,                 // dstAccessMask
        VK_QUEUE_FAMILY_IGNORED,                 // srcQueueFamilyIndex
        VK_QUEUE_FAMILY_IGNORED,                 // destQueueFamilyIndex
        *buffer,                                 // buffer
        (VkDeviceSize)0u,                        // offset
        (VkDeviceSize)VK_WHOLE_SIZE,             // size
    };

    const Unique<VkPipeline> pipeline(createComputePipeline(vk, vkDevice, VK_NULL_HANDLE, &pipelineCreateInfo));

    // record secondary command buffer
    VK_CHECK(vk.beginCommandBuffer(*secCmdBuf, &secCmdBufBeginInfo));
    {
        vk.cmdBindPipeline(*secCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
        vk.cmdBindDescriptorSets(*secCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0, numDescriptorSets,
                                 descriptorSets, 0, 0);
        vk.cmdDispatch(*secCmdBuf, 1u, 1u, 1u);
        vk.cmdPipelineBarrier(*secCmdBuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                              (VkDependencyFlags)0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);
    }
    // end recording of secondary buffer
    endCommandBuffer(vk, *secCmdBuf);

    // record another secondary command buffer to nest the other into
    VK_CHECK(vk.beginCommandBuffer(*nestedCmdBuf, &secCmdBufBeginInfo));
    {
        // execute nested secondary buffer twice in same secondary buffer
        vk.cmdExecuteCommands(*nestedCmdBuf, 1, &secCmdBuf.get());
        vk.cmdExecuteCommands(*nestedCmdBuf, 1, &secCmdBuf.get());
    }
    endCommandBuffer(vk, *nestedCmdBuf);

    // record primary command buffer
    beginCommandBuffer(vk, *primCmdBuf, 0u);
    {
        // execute nested buffer
        vk.cmdExecuteCommands(*primCmdBuf, 1, &nestedCmdBuf.get());
    }
    endCommandBuffer(vk, *primCmdBuf);

    submitCommandsAndWait(vk, vkDevice, queue, primCmdBuf.get());

    uint32_t resultCount;
    result.readResultContentsTo(&resultCount);
    // check if secondary buffer has been executed
    if (resultCount == 2)
        return tcu::TestStatus::pass("Simultaneous Nested Command Buffer Execution succeeded");
    else
        return tcu::TestStatus::fail("Simultaneous Nested Command Buffer Execution FAILED");
}

tcu::TestStatus simultaneousUseNestedSecondaryBufferTwiceTest(Context &context)
{
    const VkDevice vkDevice         = context.getDevice();
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkQueue queue             = context.getUniversalQueue();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = context.getDefaultAllocator();
    const ComputeInstanceResultBuffer result(vk, vkDevice, allocator, 0.0f);

    const VkCommandPoolCreateInfo cmdPoolParams = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,      // VkStructureType sType;
        nullptr,                                         // const void* pNext;
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // VkCommandPoolCreateFlags flags;
        queueFamilyIndex,                                // uint32_t queueFamilyIndex;
    };
    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, vkDevice, &cmdPoolParams));

    // Command buffer
    const VkCommandBufferAllocateInfo cmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        *cmdPool,                                       // VkCommandPool pool;
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,                // VkCommandBufferLevel level;
        1u,                                             // uint32_t bufferCount;
    };
    const Unique<VkCommandBuffer> primCmdBuf(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

    // Secondary Command buffer params
    const VkCommandBufferAllocateInfo secCmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        *cmdPool,                                       // VkCommandPool pool;
        VK_COMMAND_BUFFER_LEVEL_SECONDARY,              // VkCommandBufferLevel level;
        1u,                                             // uint32_t bufferCount;
    };
    const Unique<VkCommandBuffer> secCmdBuf(allocateCommandBuffer(vk, vkDevice, &secCmdBufParams));
    const Unique<VkCommandBuffer> nestedCmdBuf(allocateCommandBuffer(vk, vkDevice, &secCmdBufParams));

    const VkCommandBufferInheritanceInfo secCmdBufInheritInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
        nullptr,
        VK_NULL_HANDLE,
        0u, // subpass
        VK_NULL_HANDLE,
        VK_FALSE, // occlusionQueryEnable
        (VkQueryControlFlags)0u,
        (VkQueryPipelineStatisticFlags)0u,
    };
    const VkCommandBufferBeginInfo secCmdBufBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        nullptr,
        VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT, // flags
        &secCmdBufInheritInfo,
    };

    const uint32_t offset          = (0u);
    const uint32_t addressableSize = 256;
    const uint32_t dataSize        = 8;
    de::MovePtr<Allocation> bufferMem;
    const Unique<VkBuffer> buffer(createDataBuffer(context, offset, addressableSize, 0x00, dataSize, 0x5A, &bufferMem));
    // Secondary command buffer will have a compute shader that does an atomic increment to make sure that all instances of secondary buffers execute
    const Unique<VkDescriptorSetLayout> descriptorSetLayout(createDescriptorSetLayout(context));
    const Unique<VkDescriptorPool> descriptorPool(createDescriptorPool(context));
    const Unique<VkDescriptorSet> descriptorSet(
        createDescriptorSet(context, *descriptorPool, *descriptorSetLayout, *buffer, offset, result.getBuffer()));
    const VkDescriptorSet descriptorSets[] = {*descriptorSet};
    const int numDescriptorSets            = DE_LENGTH_OF_ARRAY(descriptorSets);

    const VkPipelineLayoutCreateInfo layoutCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // sType
        nullptr,                                       // pNext
        (VkPipelineLayoutCreateFlags)0,
        numDescriptorSets,          // setLayoutCount
        &descriptorSetLayout.get(), // pSetLayouts
        0u,                         // pushConstantRangeCount
        nullptr,                    // pPushConstantRanges
    };
    Unique<VkPipelineLayout> pipelineLayout(createPipelineLayout(vk, vkDevice, &layoutCreateInfo));

    const Unique<VkShaderModule> computeModule(createShaderModule(
        vk, vkDevice, context.getBinaryCollection().get("compute_increment"), (VkShaderModuleCreateFlags)0u));

    const VkPipelineShaderStageCreateInfo shaderCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        nullptr,
        (VkPipelineShaderStageCreateFlags)0,
        VK_SHADER_STAGE_COMPUTE_BIT, // stage
        *computeModule,              // shader
        "main",
        nullptr, // pSpecializationInfo
    };

    const VkComputePipelineCreateInfo pipelineCreateInfo = {
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        nullptr,
        0u,               // flags
        shaderCreateInfo, // cs
        *pipelineLayout,  // layout
        VK_NULL_HANDLE,   // basePipelineHandle
        0u,               // basePipelineIndex
    };

    const VkBufferMemoryBarrier bufferBarrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // sType
        nullptr,                                 // pNext
        VK_ACCESS_SHADER_WRITE_BIT,              // srcAccessMask
        VK_ACCESS_HOST_READ_BIT,                 // dstAccessMask
        VK_QUEUE_FAMILY_IGNORED,                 // srcQueueFamilyIndex
        VK_QUEUE_FAMILY_IGNORED,                 // destQueueFamilyIndex
        *buffer,                                 // buffer
        (VkDeviceSize)0u,                        // offset
        (VkDeviceSize)VK_WHOLE_SIZE,             // size
    };

    const Unique<VkPipeline> pipeline(createComputePipeline(vk, vkDevice, VK_NULL_HANDLE, &pipelineCreateInfo));

    // record secondary command buffer
    VK_CHECK(vk.beginCommandBuffer(*secCmdBuf, &secCmdBufBeginInfo));
    {
        vk.cmdBindPipeline(*secCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
        vk.cmdBindDescriptorSets(*secCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0, numDescriptorSets,
                                 descriptorSets, 0, 0);
        vk.cmdDispatch(*secCmdBuf, 1u, 1u, 1u);
        vk.cmdPipelineBarrier(*secCmdBuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                              (VkDependencyFlags)0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);
    }
    // end recording of secondary buffer
    endCommandBuffer(vk, *secCmdBuf);

    // record another secondary command buffer to nest the other into
    VK_CHECK(vk.beginCommandBuffer(*nestedCmdBuf, &secCmdBufBeginInfo));
    {
        // execute nested secondary buffer
        vk.cmdExecuteCommands(*nestedCmdBuf, 1, &secCmdBuf.get());
    }
    endCommandBuffer(vk, *nestedCmdBuf);

    // record primary command buffer
    beginCommandBuffer(vk, *primCmdBuf, 0u);
    {
        // execute nested secondary buffers twice in same primary buffer
        vk.cmdExecuteCommands(*primCmdBuf, 1, &nestedCmdBuf.get());
        vk.cmdExecuteCommands(*primCmdBuf, 1, &nestedCmdBuf.get());
    }
    endCommandBuffer(vk, *primCmdBuf);

    submitCommandsAndWait(vk, vkDevice, queue, primCmdBuf.get());

    uint32_t resultCount;
    result.readResultContentsTo(&resultCount);
    // check if secondary buffer has been executed
    if (resultCount == 2)
        return tcu::TestStatus::pass("Simultaneous Nested Command Buffer Execution succeeded");
    else
        return tcu::TestStatus::fail("Simultaneous Nested Command Buffer Execution FAILED");
}

enum class BadInheritanceInfoCase
{
    RANDOM_PTR = 0,
    RANDOM_PTR_CONTINUATION,
    RANDOM_DATA_PTR,
    INVALID_STRUCTURE_TYPE,
    VALID_NONSENSE_TYPE,
};

tcu::TestStatus badInheritanceInfoTest(Context &context, BadInheritanceInfoCase testCase)
{
    const auto &vkd             = context.getDeviceInterface();
    const auto device           = context.getDevice();
    const auto queue            = context.getUniversalQueue();
    const auto queueFamilyIndex = context.getUniversalQueueFamilyIndex();
    auto &allocator             = context.getDefaultAllocator();
    const ComputeInstanceResultBuffer result(vkd, device, allocator, 0.0f);

    // Command pool and command buffer.
    const auto cmdPool      = makeCommandPool(vkd, device, queueFamilyIndex);
    const auto cmdBufferPtr = allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto cmdBuffer    = cmdBufferPtr.get();

    // Buffers, descriptor set layouts and descriptor sets.
    const uint32_t offset          = 0u;
    const uint32_t addressableSize = 256u;
    const uint32_t dataSize        = 8u;

    // The uniform buffer will not be used by the shader but is needed by auxiliar functions here.
    de::MovePtr<Allocation> bufferMem;
    const Unique<VkBuffer> buffer(createDataBuffer(context, offset, addressableSize, 0x00, dataSize, 0x5A, &bufferMem));

    const Unique<VkDescriptorSetLayout> descriptorSetLayout(createDescriptorSetLayout(context));
    const Unique<VkDescriptorPool> descriptorPool(createDescriptorPool(context));
    const Unique<VkDescriptorSet> descriptorSet(
        createDescriptorSet(context, *descriptorPool, *descriptorSetLayout, *buffer, offset, result.getBuffer()));
    const VkDescriptorSet descriptorSets[] = {*descriptorSet};
    const int numDescriptorSets            = DE_LENGTH_OF_ARRAY(descriptorSets);

    // Pipeline layout.
    const auto pipelineLayout = makePipelineLayout(vkd, device, descriptorSetLayout.get());

    // Compute shader module.
    const Unique<VkShaderModule> computeModule(createShaderModule(
        vkd, device, context.getBinaryCollection().get("compute_increment"), (VkShaderModuleCreateFlags)0u));

    const VkPipelineShaderStageCreateInfo shaderCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        nullptr,
        (VkPipelineShaderStageCreateFlags)0,
        VK_SHADER_STAGE_COMPUTE_BIT, // stage
        *computeModule,              // shader
        "main",
        nullptr, // pSpecializationInfo
    };

    const VkComputePipelineCreateInfo pipelineCreateInfo = {
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        nullptr,
        0u,               // flags
        shaderCreateInfo, // cs
        *pipelineLayout,  // layout
        VK_NULL_HANDLE,   // basePipelineHandle
        0u,               // basePipelineIndex
    };

    const Unique<VkPipeline> pipeline(createComputePipeline(vkd, device, VK_NULL_HANDLE, &pipelineCreateInfo));

    // Compute to host barrier to read result.
    const VkBufferMemoryBarrier bufferBarrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // sType
        nullptr,                                 // pNext
        VK_ACCESS_SHADER_WRITE_BIT,              // srcAccessMask
        VK_ACCESS_HOST_READ_BIT,                 // dstAccessMask
        VK_QUEUE_FAMILY_IGNORED,                 // srcQueueFamilyIndex
        VK_QUEUE_FAMILY_IGNORED,                 // destQueueFamilyIndex
        *buffer,                                 // buffer
        (VkDeviceSize)0u,                        // offset
        (VkDeviceSize)VK_WHOLE_SIZE,             // size
    };

    // Record command buffer and submit it.
    VkCommandBufferBeginInfo beginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // VkStructureType sType;
        nullptr,                                     // const void* pNext;
        0u,                                          // VkCommandBufferUsageFlags flags;
        nullptr,                                     // const VkCommandBufferInheritanceInfo* pInheritanceInfo;
    };

    // Structures used in different test types.
    VkCommandBufferInheritanceInfo inheritanceInfo;
    VkBufferCreateInfo validNonsenseStructure;
    struct
    {
        VkStructureType sType;
        void *pNext;
    } invalidStructure;

    if (testCase == BadInheritanceInfoCase::RANDOM_PTR || testCase == BadInheritanceInfoCase::RANDOM_PTR_CONTINUATION)
    {
        de::Random rnd(1602600778u);
        VkCommandBufferInheritanceInfo *info;
        auto ptrData = reinterpret_cast<uint8_t *>(&info);

        // Fill pointer value with pseudorandom garbage.
        for (size_t i = 0; i < sizeof(info); ++i)
            *ptrData++ = rnd.getUint8();

        beginInfo.pInheritanceInfo = info;

        // Try to trick the implementation into reading pInheritanceInfo one more way.
        if (testCase == BadInheritanceInfoCase::RANDOM_PTR_CONTINUATION)
            beginInfo.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
    }
    else if (testCase == BadInheritanceInfoCase::RANDOM_DATA_PTR)
    {
        de::Random rnd(1602601141u);
        auto itr = reinterpret_cast<uint8_t *>(&inheritanceInfo);

        // Fill inheritance info data structure with random data.
        for (size_t i = 0; i < sizeof(inheritanceInfo); ++i)
            *itr++ = rnd.getUint8();

        beginInfo.pInheritanceInfo = &inheritanceInfo;
    }
    else if (testCase == BadInheritanceInfoCase::INVALID_STRUCTURE_TYPE)
    {
        de::Random rnd(1602658515u);
        auto ptrData           = reinterpret_cast<uint8_t *>(&(invalidStructure.pNext));
        invalidStructure.sType = VK_STRUCTURE_TYPE_MAX_ENUM;

        // Fill pNext pointer with random data.
        for (size_t i = 0; i < sizeof(invalidStructure.pNext); ++i)
            *ptrData++ = rnd.getUint8();

        beginInfo.pInheritanceInfo = reinterpret_cast<VkCommandBufferInheritanceInfo *>(&invalidStructure);
    }
    else if (testCase == BadInheritanceInfoCase::VALID_NONSENSE_TYPE)
    {
        validNonsenseStructure.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        validNonsenseStructure.pNext                 = nullptr;
        validNonsenseStructure.flags                 = 0u;
        validNonsenseStructure.size                  = 1024u;
        validNonsenseStructure.usage                 = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        validNonsenseStructure.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
        validNonsenseStructure.queueFamilyIndexCount = 0u;
        validNonsenseStructure.pQueueFamilyIndices   = nullptr;

        beginInfo.pInheritanceInfo = reinterpret_cast<VkCommandBufferInheritanceInfo *>(&validNonsenseStructure);
    }
    else
    {
        DE_ASSERT(false);
    }

    VK_CHECK(vkd.beginCommandBuffer(cmdBuffer, &beginInfo));
    {
        vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
        vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0, numDescriptorSets,
                                  descriptorSets, 0, 0);
        vkd.cmdDispatch(cmdBuffer, 1u, 1u, 1u);
        vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                               (VkDependencyFlags)0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);
    }
    endCommandBuffer(vkd, cmdBuffer);
    submitCommandsAndWait(vkd, device, queue, cmdBuffer);

    uint32_t resultCount;
    result.readResultContentsTo(&resultCount);

    // Make sure the command buffer was run.
    if (resultCount != 1u)
    {
        std::ostringstream msg;
        msg << "Invalid value found in results buffer (expected value 1u but found " << resultCount << ")";
        return tcu::TestStatus::fail(msg.str());
    }

    return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus simultaneousUseSecondaryBufferTwoPrimaryBuffersTest(Context &context)
{
    const VkDevice vkDevice         = context.getDevice();
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkQueue queue             = context.getUniversalQueue();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = context.getDefaultAllocator();
    const ComputeInstanceResultBuffer result(vk, vkDevice, allocator, 0.0f);

    const VkCommandPoolCreateInfo cmdPoolParams = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,      // VkStructureType sType;
        nullptr,                                         // const void* pNext;
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // VkCommandPoolCreateFlags flags;
        queueFamilyIndex,                                // uint32_t queueFamilyIndex;
    };
    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, vkDevice, &cmdPoolParams));

    // Command buffer
    const VkCommandBufferAllocateInfo cmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        *cmdPool,                                       // VkCommandPool pool;
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,                // VkCommandBufferLevel level;
        1u,                                             // uint32_t bufferCount;
    };
    // Two separate primary cmd buffers that will be executed with the same secondary cmd buffer
    const uint32_t numPrimCmdBufs = 2;
    const Unique<VkCommandBuffer> primCmdBufOne(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));
    const Unique<VkCommandBuffer> primCmdBufTwo(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));
    VkCommandBuffer primCmdBufs[numPrimCmdBufs];
    primCmdBufs[0] = primCmdBufOne.get();
    primCmdBufs[1] = primCmdBufTwo.get();

    // Secondary Command buffer params
    const VkCommandBufferAllocateInfo secCmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        *cmdPool,                                       // VkCommandPool pool;
        VK_COMMAND_BUFFER_LEVEL_SECONDARY,              // VkCommandBufferLevel level;
        1u,                                             // uint32_t bufferCount;
    };
    const Unique<VkCommandBuffer> secCmdBuf(allocateCommandBuffer(vk, vkDevice, &secCmdBufParams));

    const VkCommandBufferBeginInfo primCmdBufBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        nullptr,
        0, // flags
        nullptr,
    };

    const VkCommandBufferInheritanceInfo secCmdBufInheritInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
        nullptr,
        VK_NULL_HANDLE,                    // renderPass
        0u,                                // subpass
        VK_NULL_HANDLE,                    // framebuffer
        VK_FALSE,                          // occlusionQueryEnable
        (VkQueryControlFlags)0u,           // queryFlags
        (VkQueryPipelineStatisticFlags)0u, // pipelineStatistics
    };
    const VkCommandBufferBeginInfo secCmdBufBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        nullptr,
        VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT, // flags
        &secCmdBufInheritInfo,
    };

    const uint32_t offset          = (0u);
    const uint32_t addressableSize = 256;
    const uint32_t dataSize        = 8;
    de::MovePtr<Allocation> bufferMem;
    const Unique<VkBuffer> buffer(createDataBuffer(context, offset, addressableSize, 0x00, dataSize, 0x5A, &bufferMem));
    // Secondary command buffer will have a compute shader that does an atomic increment to make sure that all instances of secondary buffers execute
    const Unique<VkDescriptorSetLayout> descriptorSetLayout(createDescriptorSetLayout(context));
    const Unique<VkDescriptorPool> descriptorPool(createDescriptorPool(context));
    const Unique<VkDescriptorSet> descriptorSet(
        createDescriptorSet(context, *descriptorPool, *descriptorSetLayout, *buffer, offset, result.getBuffer()));
    const VkDescriptorSet descriptorSets[] = {*descriptorSet};
    const int numDescriptorSets            = DE_LENGTH_OF_ARRAY(descriptorSets);

    const VkPipelineLayoutCreateInfo layoutCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // sType
        nullptr,                                       // pNext
        (VkPipelineLayoutCreateFlags)0,
        numDescriptorSets,          // setLayoutCount
        &descriptorSetLayout.get(), // pSetLayouts
        0u,                         // pushConstantRangeCount
        nullptr,                    // pPushConstantRanges
    };
    Unique<VkPipelineLayout> pipelineLayout(createPipelineLayout(vk, vkDevice, &layoutCreateInfo));

    const Unique<VkShaderModule> computeModule(createShaderModule(
        vk, vkDevice, context.getBinaryCollection().get("compute_increment"), (VkShaderModuleCreateFlags)0u));

    const VkPipelineShaderStageCreateInfo shaderCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        nullptr,
        (VkPipelineShaderStageCreateFlags)0,
        VK_SHADER_STAGE_COMPUTE_BIT, // stage
        *computeModule,              // shader
        "main",
        nullptr, // pSpecializationInfo
    };

    const VkComputePipelineCreateInfo pipelineCreateInfo = {
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        nullptr,
        0u,               // flags
        shaderCreateInfo, // cs
        *pipelineLayout,  // layout
        VK_NULL_HANDLE,   // basePipelineHandle
        0u,               // basePipelineIndex
    };

    const Unique<VkPipeline> pipeline(createComputePipeline(vk, vkDevice, VK_NULL_HANDLE, &pipelineCreateInfo));

    // record secondary command buffer
    VK_CHECK(vk.beginCommandBuffer(*secCmdBuf, &secCmdBufBeginInfo));
    {
        vk.cmdBindPipeline(*secCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
        vk.cmdBindDescriptorSets(*secCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0, numDescriptorSets,
                                 descriptorSets, 0, 0);
        vk.cmdDispatch(*secCmdBuf, 1u, 1u, 1u);
    }
    // end recording of secondary buffer
    endCommandBuffer(vk, *secCmdBuf);

    // record primary command buffers
    // Insert one instance of same secondary command buffer into two separate primary command buffers
    VK_CHECK(vk.beginCommandBuffer(*primCmdBufOne, &primCmdBufBeginInfo));
    {
        vk.cmdExecuteCommands(*primCmdBufOne, 1, &secCmdBuf.get());
    }
    endCommandBuffer(vk, *primCmdBufOne);

    VK_CHECK(vk.beginCommandBuffer(*primCmdBufTwo, &primCmdBufBeginInfo));
    {
        vk.cmdExecuteCommands(*primCmdBufTwo, 1, &secCmdBuf.get());
    }
    endCommandBuffer(vk, *primCmdBufTwo);

    // create fence to wait for execution of queue
    const Unique<VkFence> fence(createFence(vk, vkDevice));

    const VkSubmitInfo submitInfo = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO, // sType
        nullptr,                       // pNext
        0u,                            // waitSemaphoreCount
        nullptr,                       // pWaitSemaphores
        nullptr,                       // pWaitDstStageMask
        numPrimCmdBufs,                // commandBufferCount
        primCmdBufs,                   // pCommandBuffers
        0u,                            // signalSemaphoreCount
        nullptr,                       // pSignalSemaphores
    };

    // submit primary buffers, the secondary should be executed too
    VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo, *fence));

    // wait for end of execution of queue
    VK_CHECK(vk.waitForFences(vkDevice, 1, &fence.get(), 0u, INFINITE_TIMEOUT));

    uint32_t resultCount;
    result.readResultContentsTo(&resultCount);
    // check if secondary buffer has been executed
    if (resultCount == 2)
        return tcu::TestStatus::pass("Simultaneous Secondary Command Buffer Execution succeeded");
    else
        return tcu::TestStatus::fail("Simultaneous Secondary Command Buffer Execution FAILED");
}

tcu::TestStatus recordBufferQueryPreciseWithFlagTest(Context &context)
{
    const VkDevice vkDevice         = context.getDevice();
    const DeviceInterface &vk       = context.getDeviceInterface();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();

    if (!context.getDeviceFeatures().inheritedQueries)
        TCU_THROW(NotSupportedError, "Inherited queries feature is not supported");

    const VkCommandPoolCreateInfo cmdPoolParams = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,      // sType;
        nullptr,                                         // pNext;
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // flags;
        queueFamilyIndex,                                // queueFamilyIndex;
    };
    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, vkDevice, &cmdPoolParams));

    // Command buffer
    const VkCommandBufferAllocateInfo primCmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // sType;
        nullptr,                                        // pNext;
        *cmdPool,                                       // pool;
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,                // level;
        1u,                                             // flags;
    };
    const Unique<VkCommandBuffer> primCmdBuf(allocateCommandBuffer(vk, vkDevice, &primCmdBufParams));

    // Secondary Command buffer params
    const VkCommandBufferAllocateInfo secCmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // sType;
        nullptr,                                        // pNext;
        *cmdPool,                                       // pool;
        VK_COMMAND_BUFFER_LEVEL_SECONDARY,              // level;
        1u,                                             // flags;
    };
    const Unique<VkCommandBuffer> secCmdBuf(allocateCommandBuffer(vk, vkDevice, &secCmdBufParams));

    const VkCommandBufferBeginInfo primBufferBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // sType
        nullptr,                                     // pNext
        0u,                                          // flags
        nullptr,
    };

    const VkCommandBufferInheritanceInfo secBufferInheritInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
        nullptr,
        VK_NULL_HANDLE,                    // renderPass
        0u,                                // subpass
        VK_NULL_HANDLE,                    // framebuffer
        VK_TRUE,                           // occlusionQueryEnable
        VK_QUERY_CONTROL_PRECISE_BIT,      // queryFlags
        (VkQueryPipelineStatisticFlags)0u, // pipelineStatistics
    };
    const VkCommandBufferBeginInfo secBufferBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // sType
        nullptr,                                     // pNext
        0u,                                          // flags
        &secBufferInheritInfo,
    };

    const VkQueryPoolCreateInfo queryPoolCreateInfo = {
        VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, // sType
        nullptr,                                  // pNext
        (VkQueryPoolCreateFlags)0,                // flags
        VK_QUERY_TYPE_OCCLUSION,                  // queryType
        1u,                                       // entryCount
        0u,                                       // pipelineStatistics
    };
    Unique<VkQueryPool> queryPool(createQueryPool(vk, vkDevice, &queryPoolCreateInfo));

    VK_CHECK(vk.beginCommandBuffer(secCmdBuf.get(), &secBufferBeginInfo));
    endCommandBuffer(vk, secCmdBuf.get());

    VK_CHECK(vk.beginCommandBuffer(primCmdBuf.get(), &primBufferBeginInfo));
    {
        vk.cmdResetQueryPool(primCmdBuf.get(), queryPool.get(), 0u, 1u);
        vk.cmdBeginQuery(primCmdBuf.get(), queryPool.get(), 0u, VK_QUERY_CONTROL_PRECISE_BIT);
        {
            vk.cmdExecuteCommands(primCmdBuf.get(), 1u, &secCmdBuf.get());
        }
        vk.cmdEndQuery(primCmdBuf.get(), queryPool.get(), 0u);
    }
    endCommandBuffer(vk, primCmdBuf.get());

    return tcu::TestStatus::pass(
        "Successfully recorded a secondary command buffer allowing a precise occlusion query.");
}

tcu::TestStatus recordBufferQueryImpreciseWithFlagTest(Context &context)
{
    const VkDevice vkDevice         = context.getDevice();
    const DeviceInterface &vk       = context.getDeviceInterface();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();

    if (!context.getDeviceFeatures().inheritedQueries)
        TCU_THROW(NotSupportedError, "Inherited queries feature is not supported");

    const VkCommandPoolCreateInfo cmdPoolParams = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,      // sType;
        nullptr,                                         // pNext;
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // flags;
        queueFamilyIndex,                                // queueFamilyIndex;
    };
    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, vkDevice, &cmdPoolParams));

    // Command buffer
    const VkCommandBufferAllocateInfo primCmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // sType;
        nullptr,                                        // pNext;
        *cmdPool,                                       // pool;
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,                // level;
        1u,                                             // flags;
    };
    const Unique<VkCommandBuffer> primCmdBuf(allocateCommandBuffer(vk, vkDevice, &primCmdBufParams));

    // Secondary Command buffer params
    const VkCommandBufferAllocateInfo secCmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // sType;
        nullptr,                                        // pNext;
        *cmdPool,                                       // pool;
        VK_COMMAND_BUFFER_LEVEL_SECONDARY,              // level;
        1u,                                             // flags;
    };
    const Unique<VkCommandBuffer> secCmdBuf(allocateCommandBuffer(vk, vkDevice, &secCmdBufParams));

    const VkCommandBufferBeginInfo primBufferBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // sType
        nullptr,                                     // pNext
        0u,                                          // flags
        nullptr,
    };

    const VkCommandBufferInheritanceInfo secBufferInheritInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
        nullptr,
        VK_NULL_HANDLE,                    // renderPass
        0u,                                // subpass
        VK_NULL_HANDLE,                    // framebuffer
        VK_TRUE,                           // occlusionQueryEnable
        VK_QUERY_CONTROL_PRECISE_BIT,      // queryFlags
        (VkQueryPipelineStatisticFlags)0u, // pipelineStatistics
    };
    const VkCommandBufferBeginInfo secBufferBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // sType
        nullptr,                                     // pNext
        0u,                                          // flags
        &secBufferInheritInfo,
    };

    const VkQueryPoolCreateInfo queryPoolCreateInfo = {
        VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, // sType
        nullptr,                                  // pNext
        0u,                                       // flags
        VK_QUERY_TYPE_OCCLUSION,                  // queryType
        1u,                                       // entryCount
        0u,                                       // pipelineStatistics
    };
    Unique<VkQueryPool> queryPool(createQueryPool(vk, vkDevice, &queryPoolCreateInfo));

    VK_CHECK(vk.beginCommandBuffer(secCmdBuf.get(), &secBufferBeginInfo));
    endCommandBuffer(vk, secCmdBuf.get());

    VK_CHECK(vk.beginCommandBuffer(primCmdBuf.get(), &primBufferBeginInfo));
    {
        vk.cmdResetQueryPool(primCmdBuf.get(), queryPool.get(), 0u, 1u);
        vk.cmdBeginQuery(primCmdBuf.get(), queryPool.get(), 0u, 0u);
        {
            vk.cmdExecuteCommands(primCmdBuf.get(), 1u, &secCmdBuf.get());
        }
        vk.cmdEndQuery(primCmdBuf.get(), queryPool.get(), 0u);
    }
    endCommandBuffer(vk, primCmdBuf.get());

    return tcu::TestStatus::pass(
        "Successfully recorded an imprecise query with a secondary command buffer allowing a precise occlusion query.");
}

tcu::TestStatus recordBufferQueryImpreciseWithoutFlagTest(Context &context)
{
    const VkDevice vkDevice         = context.getDevice();
    const DeviceInterface &vk       = context.getDeviceInterface();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();

    if (!context.getDeviceFeatures().inheritedQueries)
        TCU_THROW(NotSupportedError, "Inherited queries feature is not supported");

    const VkCommandPoolCreateInfo cmdPoolParams = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,      // sType;
        nullptr,                                         // pNext;
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // flags;
        queueFamilyIndex,                                // queueFamilyIndex;
    };
    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, vkDevice, &cmdPoolParams));

    // Command buffer
    const VkCommandBufferAllocateInfo primCmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // sType;
        nullptr,                                        // pNext;
        *cmdPool,                                       // pool;
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,                // level;
        1u,                                             // flags;
    };
    const Unique<VkCommandBuffer> primCmdBuf(allocateCommandBuffer(vk, vkDevice, &primCmdBufParams));

    // Secondary Command buffer params
    const VkCommandBufferAllocateInfo secCmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // sType;
        nullptr,                                        // pNext;
        *cmdPool,                                       // pool;
        VK_COMMAND_BUFFER_LEVEL_SECONDARY,              // level;
        1u,                                             // flags;
    };
    const Unique<VkCommandBuffer> secCmdBuf(allocateCommandBuffer(vk, vkDevice, &secCmdBufParams));

    const VkCommandBufferBeginInfo primBufferBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // sType
        nullptr,                                     // pNext
        0u,                                          // flags
        nullptr,
    };

    const VkCommandBufferInheritanceInfo secBufferInheritInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
        nullptr,
        VK_NULL_HANDLE,                    // renderPass
        0u,                                // subpass
        VK_NULL_HANDLE,                    // framebuffer
        VK_TRUE,                           // occlusionQueryEnable
        0u,                                // queryFlags
        (VkQueryPipelineStatisticFlags)0u, // pipelineStatistics
    };
    const VkCommandBufferBeginInfo secBufferBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // sType
        nullptr,                                     // pNext
        0u,                                          // flags
        &secBufferInheritInfo,
    };

    const VkQueryPoolCreateInfo queryPoolCreateInfo = {
        VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, // sType
        nullptr,                                  // pNext
        (VkQueryPoolCreateFlags)0,
        VK_QUERY_TYPE_OCCLUSION,
        1u,
        0u,
    };
    Unique<VkQueryPool> queryPool(createQueryPool(vk, vkDevice, &queryPoolCreateInfo));

    VK_CHECK(vk.beginCommandBuffer(secCmdBuf.get(), &secBufferBeginInfo));
    endCommandBuffer(vk, secCmdBuf.get());

    VK_CHECK(vk.beginCommandBuffer(primCmdBuf.get(), &primBufferBeginInfo));
    {
        vk.cmdResetQueryPool(primCmdBuf.get(), queryPool.get(), 0u, 1u);
        vk.cmdBeginQuery(primCmdBuf.get(), queryPool.get(), 0u, 0u);
        {
            vk.cmdExecuteCommands(primCmdBuf.get(), 1u, &secCmdBuf.get());
        }
        vk.cmdEndQuery(primCmdBuf.get(), queryPool.get(), 0u);
    }
    endCommandBuffer(vk, primCmdBuf.get());

    return tcu::TestStatus::pass("Successfully recorded an imprecise query with a secondary command buffer not "
                                 "allowing a precise occlusion query.");
}

/******** 19.4. Command Buffer Submission (5.4 in VK 1.0 Spec) ****************/
tcu::TestStatus submitBufferCountNonZero(Context &context)
{
    const VkDevice vkDevice         = context.getDevice();
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkQueue queue             = context.getUniversalQueue();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();

    const uint32_t BUFFER_COUNT = 5u;

    const VkCommandPoolCreateInfo cmdPoolParams = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, // sType;
        nullptr,                                    // pNext;
        0u,                                         // flags;
        queueFamilyIndex,                           // queueFamilyIndex;
    };
    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, vkDevice, &cmdPoolParams));

    // Command buffer
    const VkCommandBufferAllocateInfo cmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // sType;
        nullptr,                                        // pNext;
        *cmdPool,                                       // pool;
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,                // level;
        BUFFER_COUNT,                                   // bufferCount;
    };
    Move<VkCommandBuffer> cmdBuffers[BUFFER_COUNT];
    allocateCommandBuffers(vk, vkDevice, &cmdBufParams, cmdBuffers);

    const VkCommandBufferBeginInfo cmdBufBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // sType
        nullptr,                                     // pNext
        0u,                                          // flags
        nullptr,
    };

    std::vector<VkEventSp> events;
    for (uint32_t ndx = 0; ndx < BUFFER_COUNT; ++ndx)
    {
        events.push_back(VkEventSp(new vk::Unique<VkEvent>(createEvent(vk, vkDevice))));
    }

    VkCommandBuffer cmdBufferHandles[BUFFER_COUNT];

    // Record the command buffers
    for (uint32_t ndx = 0; ndx < BUFFER_COUNT; ++ndx)
    {
        VK_CHECK(vk.beginCommandBuffer(cmdBuffers[ndx].get(), &cmdBufBeginInfo));
        {
            vk.cmdSetEvent(cmdBuffers[ndx].get(), events[ndx]->get(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        }
        endCommandBuffer(vk, cmdBuffers[ndx].get());
        cmdBufferHandles[ndx] = cmdBuffers[ndx].get();
    }

    // We'll use a fence to wait for the execution of the queue
    const Unique<VkFence> fence(createFence(vk, vkDevice));

    const VkSubmitInfo submitInfo = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO, // sType
        nullptr,                       // pNext
        0u,                            // waitSemaphoreCount
        nullptr,                       // pWaitSemaphores
        nullptr,                       // pWaitDstStageMask
        BUFFER_COUNT,                  // commandBufferCount
        cmdBufferHandles,              // pCommandBuffers
        0u,                            // signalSemaphoreCount
        nullptr,                       // pSignalSemaphores
    };

    // Submit the alpha command buffer to the queue
    VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo, fence.get()));
    // Wait for the queue
    VK_CHECK(vk.waitForFences(vkDevice, 1u, &fence.get(), VK_TRUE, INFINITE_TIMEOUT));

    // Check if the buffers were executed
    tcu::TestStatus testResult = tcu::TestStatus::incomplete();

    for (uint32_t ndx = 0; ndx < BUFFER_COUNT; ++ndx)
    {
        if (vk.getEventStatus(vkDevice, events[ndx]->get()) != VK_EVENT_SET)
        {
            testResult = tcu::TestStatus::fail("Failed to set the event.");
            break;
        }
    }

    if (!testResult.isComplete())
        testResult = tcu::TestStatus::pass("All buffers were submitted and executed correctly.");

    return testResult;
}

tcu::TestStatus submitBufferCountEqualZero(Context &context)
{
    const VkDevice vkDevice         = context.getDevice();
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkQueue queue             = context.getUniversalQueue();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();

    const uint32_t BUFFER_COUNT = 2u;

    const VkCommandPoolCreateInfo cmdPoolParams = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, // sType;
        nullptr,                                    // pNext;
        0u,                                         // flags;
        queueFamilyIndex,                           // queueFamilyIndex;
    };
    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, vkDevice, &cmdPoolParams));

    // Command buffer
    const VkCommandBufferAllocateInfo cmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // sType;
        nullptr,                                        // pNext;
        *cmdPool,                                       // pool;
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,                // level;
        BUFFER_COUNT,                                   // bufferCount;
    };
    Move<VkCommandBuffer> cmdBuffers[BUFFER_COUNT];
    allocateCommandBuffers(vk, vkDevice, &cmdBufParams, cmdBuffers);

    const VkCommandBufferBeginInfo cmdBufBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // sType
        nullptr,                                     // pNext
        0u,                                          // flags
        nullptr,
    };

    std::vector<VkEventSp> events;
    for (uint32_t ndx = 0; ndx < BUFFER_COUNT; ++ndx)
        events.push_back(VkEventSp(new vk::Unique<VkEvent>(createEvent(vk, vkDevice))));

    // Record the command buffers
    for (uint32_t ndx = 0; ndx < BUFFER_COUNT; ++ndx)
    {
        VK_CHECK(vk.beginCommandBuffer(cmdBuffers[ndx].get(), &cmdBufBeginInfo));
        {
            vk.cmdSetEvent(cmdBuffers[ndx].get(), events[ndx]->get(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        }
        endCommandBuffer(vk, cmdBuffers[ndx].get());
    }

    // We'll use a fence to wait for the execution of the queue
    const Unique<VkFence> fenceZero(createFence(vk, vkDevice));
    const Unique<VkFence> fenceOne(createFence(vk, vkDevice));

    VkCommandBuffer cmdBuf0                = cmdBuffers[0].get();
    const VkSubmitInfo submitInfoCountZero = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO, // sType
        nullptr,                       // pNext
        0u,                            // waitSemaphoreCount
        nullptr,                       // pWaitSemaphores
        nullptr,                       // pWaitDstStageMask
        1u,                            // commandBufferCount
        &cmdBuf0,                      // pCommandBuffers
        0u,                            // signalSemaphoreCount
        nullptr,                       // pSignalSemaphores
    };

    VkCommandBuffer cmdBuf1               = cmdBuffers[1].get();
    const VkSubmitInfo submitInfoCountOne = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO, // sType
        nullptr,                       // pNext
        0u,                            // waitSemaphoreCount
        nullptr,                       // pWaitSemaphores
        nullptr,                       // pWaitDstStageMask
        1u,                            // commandBufferCount
        &cmdBuf1,                      // pCommandBuffers
        0u,                            // signalSemaphoreCount
        nullptr,                       // pSignalSemaphores
    };

    // Submit the command buffers to the queue
    // We're performing two submits to make sure that the first one has
    // a chance to be processed before we check the event's status
    VK_CHECK(vk.queueSubmit(queue, 0, &submitInfoCountZero, fenceZero.get()));
    VK_CHECK(vk.queueSubmit(queue, 1, &submitInfoCountOne, fenceOne.get()));

    const VkFence fences[] = {
        fenceZero.get(),
        fenceOne.get(),
    };

    // Wait for the queue
    VK_CHECK(vk.waitForFences(vkDevice, (uint32_t)DE_LENGTH_OF_ARRAY(fences), fences, VK_TRUE, INFINITE_TIMEOUT));

    // Check if the first buffer was executed
    tcu::TestStatus testResult = tcu::TestStatus::incomplete();

    if (vk.getEventStatus(vkDevice, events[0]->get()) == VK_EVENT_SET)
        testResult = tcu::TestStatus::fail("The first event was signaled.");
    else
        testResult = tcu::TestStatus::pass("The first submission was ignored.");

    return testResult;
}

tcu::TestStatus submitBufferWaitSingleSemaphore(Context &context)
{
    const VkDevice vkDevice         = context.getDevice();
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkQueue queue             = context.getUniversalQueue();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();

    const VkCommandPoolCreateInfo cmdPoolParams = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,      // VkStructureType sType;
        nullptr,                                         // const void* pNext;
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // VkCommandPoolCreateFlags flags;
        queueFamilyIndex,                                // uint32_t queueFamilyIndex;
    };
    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, vkDevice, &cmdPoolParams));

    // Command buffer
    const VkCommandBufferAllocateInfo cmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        *cmdPool,                                       // VkCommandPool pool;
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,                // VkCommandBufferLevel level;
        1u,                                             // uint32_t bufferCount;
    };

    // Create two command buffers
    const Unique<VkCommandBuffer> primCmdBuf1(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));
    const Unique<VkCommandBuffer> primCmdBuf2(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

    const VkCommandBufferBeginInfo primCmdBufBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // sType
        nullptr,                                     // pNext
        0,                                           // flags
        nullptr                                      // const VkCommandBufferInheritanceInfo* pInheritanceInfo;
    };

    // create two events that will be used to check if command buffers has been executed
    const Unique<VkEvent> event1(createEvent(vk, vkDevice));
    const Unique<VkEvent> event2(createEvent(vk, vkDevice));

    // reset events
    VK_CHECK(vk.resetEvent(vkDevice, *event1));
    VK_CHECK(vk.resetEvent(vkDevice, *event2));

    // record first command buffer
    VK_CHECK(vk.beginCommandBuffer(*primCmdBuf1, &primCmdBufBeginInfo));
    {
        // allow execution of event during every stage of pipeline
        VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

        // record setting event
        vk.cmdSetEvent(*primCmdBuf1, *event1, stageMask);
    }
    endCommandBuffer(vk, *primCmdBuf1);

    // record second command buffer
    VK_CHECK(vk.beginCommandBuffer(*primCmdBuf2, &primCmdBufBeginInfo));
    {
        // allow execution of event during every stage of pipeline
        VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

        // record setting event
        vk.cmdSetEvent(*primCmdBuf2, *event2, stageMask);
    }
    endCommandBuffer(vk, *primCmdBuf2);

    // create fence to wait for execution of queue
    const Unique<VkFence> fence(createFence(vk, vkDevice));

    // create semaphore for use in this test
    const Unique<VkSemaphore> semaphore(createSemaphore(vk, vkDevice));

    // create submit info for first buffer - signalling semaphore
    const VkSubmitInfo submitInfo1 = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO, // sType
        nullptr,                       // pNext
        0u,                            // waitSemaphoreCount
        nullptr,                       // pWaitSemaphores
        nullptr,                       // pWaitDstStageMask
        1,                             // commandBufferCount
        &primCmdBuf1.get(),            // pCommandBuffers
        1u,                            // signalSemaphoreCount
        &semaphore.get(),              // pSignalSemaphores
    };

    // Submit the command buffer to the queue
    VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo1, *fence));

    // wait for end of execution of queue
    VK_CHECK(vk.waitForFences(vkDevice, 1, &fence.get(), 0u, INFINITE_TIMEOUT));

    // check if buffer has been executed
    VkResult result = vk.getEventStatus(vkDevice, *event1);
    if (result != VK_EVENT_SET)
        return tcu::TestStatus::fail("Submit Buffer and Wait for Single Semaphore Test FAILED");

    const VkPipelineStageFlags waitDstStageFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    // create submit info for second buffer - waiting for semaphore
    const VkSubmitInfo submitInfo2 = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO, // sType
        nullptr,                       // pNext
        1u,                            // waitSemaphoreCount
        &semaphore.get(),              // pWaitSemaphores
        &waitDstStageFlags,            // pWaitDstStageMask
        1,                             // commandBufferCount
        &primCmdBuf2.get(),            // pCommandBuffers
        0u,                            // signalSemaphoreCount
        nullptr,                       // pSignalSemaphores
    };

    // reset fence, so it can be used again
    VK_CHECK(vk.resetFences(vkDevice, 1u, &fence.get()));

    // Submit the second command buffer to the queue
    VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo2, *fence));

    // wait for end of execution of queue
    VK_CHECK(vk.waitForFences(vkDevice, 1, &fence.get(), 0u, INFINITE_TIMEOUT));

    // check if second buffer has been executed
    // if it has been executed, it means that the semaphore was signalled - so test if passed
    result = vk.getEventStatus(vkDevice, *event1);
    if (result != VK_EVENT_SET)
        return tcu::TestStatus::fail("Submit Buffer and Wait for Single Semaphore Test FAILED");

    return tcu::TestStatus::pass("Submit Buffer and Wait for Single Semaphore Test succeeded");
}

tcu::TestStatus submitBufferWaitManySemaphores(Context &context)
{
    // This test will create numSemaphores semaphores, and signal them in NUM_SEMAPHORES submits to queue
    // After that the numSubmissions queue submissions will wait for each semaphore

    const uint32_t numSemaphores    = 10u; // it must be multiply of numSubmission
    const uint32_t numSubmissions   = 2u;
    const VkDevice vkDevice         = context.getDevice();
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkQueue queue             = context.getUniversalQueue();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();

    const VkCommandPoolCreateInfo cmdPoolParams = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,      // VkStructureType sType;
        nullptr,                                         // const void* pNext;
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // VkCommandPoolCreateFlags flags;
        queueFamilyIndex,                                // uint32_t queueFamilyIndex;
    };
    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, vkDevice, &cmdPoolParams));

    // Command buffer
    const VkCommandBufferAllocateInfo cmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        *cmdPool,                                       // VkCommandPool pool;
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,                // VkCommandBufferLevel level;
        1u,                                             // uint32_t bufferCount;
    };

    // Create command buffer
    const Unique<VkCommandBuffer> primCmdBuf(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

    const VkCommandBufferBeginInfo primCmdBufBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // sType
        nullptr,                                     // pNext
        0,                                           // flags
        nullptr                                      // const VkCommandBufferInheritanceInfo* pInheritanceInfo;
    };

    // create event that will be used to check if command buffers has been executed
    const Unique<VkEvent> event(createEvent(vk, vkDevice));

    // reset event - at creation state is undefined
    VK_CHECK(vk.resetEvent(vkDevice, *event));

    // record command buffer
    VK_CHECK(vk.beginCommandBuffer(*primCmdBuf, &primCmdBufBeginInfo));
    {
        // allow execution of event during every stage of pipeline
        VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

        // record setting event
        vk.cmdSetEvent(*primCmdBuf, *event, stageMask);
    }
    endCommandBuffer(vk, *primCmdBuf);

    // create fence to wait for execution of queue
    const Unique<VkFence> fence(createFence(vk, vkDevice));

    // numSemaphores is declared const, so this array can be static
    // the semaphores will be destroyed automatically at end of scope
    Move<VkSemaphore> semaphoreArray[numSemaphores];
    VkSemaphore semaphores[numSemaphores];

    for (uint32_t idx = 0; idx < numSemaphores; ++idx)
    {
        // create semaphores for use in this test
        semaphoreArray[idx] = createSemaphore(vk, vkDevice);
        semaphores[idx]     = semaphoreArray[idx].get();
    }

    {
        // create submit info for buffer - signal semaphores
        const VkSubmitInfo submitInfo1 = {
            VK_STRUCTURE_TYPE_SUBMIT_INFO, // sType
            nullptr,                       // pNext
            0u,                            // waitSemaphoreCount
            nullptr,                       // pWaitSemaphores
            nullptr,                       // pWaitDstStageMask
            1,                             // commandBufferCount
            &primCmdBuf.get(),             // pCommandBuffers
            numSemaphores,                 // signalSemaphoreCount
            semaphores                     // pSignalSemaphores
        };
        // Submit the command buffer to the queue
        VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo1, *fence));

        // wait for end of execution of queue
        VK_CHECK(vk.waitForFences(vkDevice, 1, &fence.get(), 0u, INFINITE_TIMEOUT));

        // check if buffer has been executed
        VkResult result = vk.getEventStatus(vkDevice, *event);
        if (result != VK_EVENT_SET)
            return tcu::TestStatus::fail("Submit Buffer and Wait for Many Semaphores Test FAILED");

        // reset event, so next buffers can set it again
        VK_CHECK(vk.resetEvent(vkDevice, *event));

        // reset fence, so it can be used again
        VK_CHECK(vk.resetFences(vkDevice, 1u, &fence.get()));
    }

    const uint32_t numberOfSemaphoresToBeWaitedByOneSubmission = numSemaphores / numSubmissions;
    const std::vector<VkPipelineStageFlags> waitDstStageFlags(numberOfSemaphoresToBeWaitedByOneSubmission,
                                                              VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

    // the following code waits for the semaphores set above - numSubmissions queues will wait for each semaphore from above
    for (uint32_t idxSubmission = 0; idxSubmission < numSubmissions; ++idxSubmission)
    {

        // create submit info for buffer - waiting for semaphore
        const VkSubmitInfo submitInfo2 = {
            VK_STRUCTURE_TYPE_SUBMIT_INFO,                                              // sType
            nullptr,                                                                    // pNext
            numberOfSemaphoresToBeWaitedByOneSubmission,                                // waitSemaphoreCount
            semaphores + (numberOfSemaphoresToBeWaitedByOneSubmission * idxSubmission), // pWaitSemaphores
            waitDstStageFlags.data(),                                                   // pWaitDstStageMask
            1,                                                                          // commandBufferCount
            &primCmdBuf.get(),                                                          // pCommandBuffers
            0u,                                                                         // signalSemaphoreCount
            nullptr,                                                                    // pSignalSemaphores
        };

        // Submit the second command buffer to the queue
        VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo2, *fence));

        // wait for 1 second.
        VK_CHECK(vk.waitForFences(vkDevice, 1, &fence.get(), 0u, 1000 * 1000 * 1000));

        // check if second buffer has been executed
        // if it has been executed, it means that the semaphore was signalled - so test if passed
        VkResult result = vk.getEventStatus(vkDevice, *event);
        if (result != VK_EVENT_SET)
            return tcu::TestStatus::fail("Submit Buffer and Wait for Many Semaphores Test FAILED");

        // reset fence, so it can be used again
        VK_CHECK(vk.resetFences(vkDevice, 1u, &fence.get()));

        // reset event, so next buffers can set it again
        VK_CHECK(vk.resetEvent(vkDevice, *event));
    }

    return tcu::TestStatus::pass("Submit Buffer and Wait for Many Semaphores Test succeeded");
}

tcu::TestStatus submitBufferNullFence(Context &context)
{
    const VkDevice vkDevice         = context.getDevice();
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkQueue queue             = context.getUniversalQueue();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();

    const short BUFFER_COUNT = 2;

    const VkCommandPoolCreateInfo cmdPoolParams = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, // sType;
        nullptr,                                    // pNext;
        0u,                                         // flags;
        queueFamilyIndex,                           // queueFamilyIndex;
    };
    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, vkDevice, &cmdPoolParams));

    // Command buffer
    const VkCommandBufferAllocateInfo cmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // sType;
        nullptr,                                        // pNext;
        *cmdPool,                                       // pool;
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,                // level;
        BUFFER_COUNT,                                   // bufferCount;
    };
    Move<VkCommandBuffer> cmdBuffers[BUFFER_COUNT];
    allocateCommandBuffers(vk, vkDevice, &cmdBufParams, cmdBuffers);

    const VkCommandBufferBeginInfo cmdBufBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // sType
        nullptr,                                     // pNext
        0u,                                          // flags
        nullptr,
    };

    std::vector<VkEventSp> events;
    for (uint32_t ndx = 0; ndx < BUFFER_COUNT; ++ndx)
        events.push_back(VkEventSp(new vk::Unique<VkEvent>(createEvent(vk, vkDevice))));

    // Record the command buffers
    for (uint32_t ndx = 0; ndx < BUFFER_COUNT; ++ndx)
    {
        VK_CHECK(vk.beginCommandBuffer(cmdBuffers[ndx].get(), &cmdBufBeginInfo));
        {
            vk.cmdSetEvent(cmdBuffers[ndx].get(), events[ndx]->get(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        }
        endCommandBuffer(vk, cmdBuffers[ndx].get());
    }

    // We'll use a fence to wait for the execution of the queue
    const Unique<VkFence> fence(createFence(vk, vkDevice));

    VkCommandBuffer cmdBuf0                = cmdBuffers[0].get();
    const VkSubmitInfo submitInfoNullFence = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO, // sType
        nullptr,                       // pNext
        0u,                            // waitSemaphoreCount
        nullptr,                       // pWaitSemaphores
        nullptr,                       // pWaitDstStageMask
        1u,                            // commandBufferCount
        &cmdBuf0,                      // pCommandBuffers
        0u,                            // signalSemaphoreCount
        nullptr,                       // pSignalSemaphores
    };

    VkCommandBuffer cmdBuf1                   = cmdBuffers[1].get();
    const VkSubmitInfo submitInfoNonNullFence = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO, // sType
        nullptr,                       // pNext
        0u,                            // waitSemaphoreCount
        nullptr,                       // pWaitSemaphores
        nullptr,                       // pWaitDstStageMask
        1u,                            // commandBufferCount
        &cmdBuf1,                      // pCommandBuffers
        0u,                            // signalSemaphoreCount
        nullptr,                       // pSignalSemaphores
    };

    // Perform two submissions - one with no fence, the other one with a valid
    // fence Hoping submitting the other buffer will give the first one time to
    // execute
    VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfoNullFence, VK_NULL_HANDLE));
    VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfoNonNullFence, fence.get()));

    // Wait for the queue
    VK_CHECK(vk.waitForFences(vkDevice, 1u, &fence.get(), VK_TRUE, INFINITE_TIMEOUT));

    tcu::TestStatus testResult = tcu::TestStatus::incomplete();

    //Fence guaranteed that all buffers submited before fence were executed
    if (vk.getEventStatus(vkDevice, events[0]->get()) != VK_EVENT_SET ||
        vk.getEventStatus(vkDevice, events[1]->get()) != VK_EVENT_SET)
    {
        testResult = tcu::TestStatus::fail("One of the buffers was not executed.");
    }
    else
    {
        testResult = tcu::TestStatus::pass("Buffers have been submitted and executed correctly.");
    }

    vk.queueWaitIdle(queue);
    return testResult;
}

tcu::TestStatus submitTwoBuffersOneBufferNullWithFence(Context &context)
{
    const VkDevice vkDevice         = context.getDevice();
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkQueue queue             = context.getUniversalQueue();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();
    const uint32_t BUFFER_COUNT     = 2u;

    const VkCommandPoolCreateInfo cmdPoolParams = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,      // sType;
        nullptr,                                         // pNext;
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // flags;
        queueFamilyIndex,                                // queueFamilyIndex;
    };
    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, vkDevice, &cmdPoolParams));

    const VkCommandBufferAllocateInfo cmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // sType;
        nullptr,                                        // pNext;
        *cmdPool,                                       // pool;
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,                // level;
        BUFFER_COUNT,                                   // bufferCount;
    };

    Move<VkCommandBuffer> cmdBuffers[BUFFER_COUNT];
    allocateCommandBuffers(vk, vkDevice, &cmdBufParams, cmdBuffers);

    const VkCommandBufferBeginInfo cmdBufBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // sType
        nullptr,                                     // pNext
        0u,                                          // flags
        nullptr,                                     // pInheritanceInfo
    };

    std::vector<VkEventSp> events;
    for (uint32_t ndx = 0; ndx < BUFFER_COUNT; ++ndx)
        events.push_back(VkEventSp(new vk::Unique<VkEvent>(createEvent(vk, vkDevice))));

    // Record the command buffers
    for (uint32_t ndx = 0; ndx < BUFFER_COUNT; ++ndx)
    {
        VK_CHECK(vk.beginCommandBuffer(cmdBuffers[ndx].get(), &cmdBufBeginInfo));
        {
            vk.cmdSetEvent(cmdBuffers[ndx].get(), events[ndx]->get(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        }
        VK_CHECK(vk.endCommandBuffer(cmdBuffers[ndx].get()));
    }

    // First command buffer
    VkCommandBuffer cmdBuf0                   = cmdBuffers[0].get();
    const VkSubmitInfo submitInfoNonNullFirst = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO, // sType
        nullptr,                       // pNext
        0u,                            // waitSemaphoreCount
        nullptr,                       // pWaitSemaphores
        nullptr,                       // pWaitDstStageMask
        1u,                            // commandBufferCount
        &cmdBuf0,                      // pCommandBuffers
        0u,                            // signalSemaphoreCount
        nullptr,                       // pSignalSemaphores
    };

    // Second command buffer
    VkCommandBuffer cmdBuf1                    = cmdBuffers[1].get();
    const VkSubmitInfo submitInfoNonNullSecond = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO, // sType
        nullptr,                       // pNext
        0u,                            // waitSemaphoreCount
        nullptr,                       // pWaitSemaphores
        nullptr,                       // pWaitDstStageMask
        1u,                            // commandBufferCount
        &cmdBuf1,                      // pCommandBuffers
        0u,                            // signalSemaphoreCount
        nullptr,                       // pSignalSemaphores
    };

    // Fence will be submitted with the null queue
    const Unique<VkFence> fence(createFence(vk, vkDevice));

    // Perform two separate queueSubmit calls on the same queue followed
    // by a third call with no submitInfos and with a valid fence
    VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfoNonNullFirst, VK_NULL_HANDLE));
    VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfoNonNullSecond, VK_NULL_HANDLE));
    VK_CHECK(vk.queueSubmit(queue, 0u, nullptr, fence.get()));

    // Wait for the queue
    VK_CHECK(vk.waitForFences(vkDevice, 1u, &fence.get(), VK_TRUE, INFINITE_TIMEOUT));

    return tcu::TestStatus::pass("Buffers have been submitted correctly");
}

/******** 19.5. Secondary Command Buffer Execution (5.6 in VK 1.0 Spec) *******/
tcu::TestStatus executeSecondaryBufferTest(Context &context)
{
    const VkDevice vkDevice         = context.getDevice();
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkQueue queue             = context.getUniversalQueue();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();

    const VkCommandPoolCreateInfo cmdPoolParams = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,      // sType;
        nullptr,                                         // pNext;
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // flags;
        queueFamilyIndex,                                // queueFamilyIndex;
    };
    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, vkDevice, &cmdPoolParams));

    // Command buffer
    const VkCommandBufferAllocateInfo cmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // sType;
        nullptr,                                        // pNext;
        *cmdPool,                                       // commandPool;
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,                // level;
        1u,                                             // bufferCount;
    };
    const Unique<VkCommandBuffer> primCmdBuf(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

    // Secondary Command buffer
    const VkCommandBufferAllocateInfo secCmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // sType;
        nullptr,                                        // pNext;
        *cmdPool,                                       // commandPool;
        VK_COMMAND_BUFFER_LEVEL_SECONDARY,              // level;
        1u,                                             // bufferCount;
    };
    const Unique<VkCommandBuffer> secCmdBuf(allocateCommandBuffer(vk, vkDevice, &secCmdBufParams));

    const VkCommandBufferBeginInfo primCmdBufBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // sType
        nullptr,                                     // pNext
        0u,                                          // flags
        nullptr,
    };

    const VkCommandBufferInheritanceInfo secCmdBufInheritInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
        nullptr,
        VK_NULL_HANDLE,                    // renderPass
        0u,                                // subpass
        VK_NULL_HANDLE,                    // framebuffer
        VK_FALSE,                          // occlusionQueryEnable
        (VkQueryControlFlags)0u,           // queryFlags
        (VkQueryPipelineStatisticFlags)0u, // pipelineStatistics
    };
    const VkCommandBufferBeginInfo secCmdBufBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // sType
        nullptr,                                     // pNext
        0u,                                          // flags
        &secCmdBufInheritInfo,
    };

    // create event that will be used to check if secondary command buffer has been executed
    const Unique<VkEvent> event(createEvent(vk, vkDevice));

    // reset event
    VK_CHECK(vk.resetEvent(vkDevice, *event));

    // record secondary command buffer
    VK_CHECK(vk.beginCommandBuffer(*secCmdBuf, &secCmdBufBeginInfo));
    {
        // allow execution of event during every stage of pipeline
        VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        // record setting event
        vk.cmdSetEvent(*secCmdBuf, *event, stageMask);
    }
    // end recording of the secondary buffer
    endCommandBuffer(vk, *secCmdBuf);

    // record primary command buffer
    VK_CHECK(vk.beginCommandBuffer(*primCmdBuf, &primCmdBufBeginInfo));
    {
        // execute secondary buffer
        vk.cmdExecuteCommands(*primCmdBuf, 1u, &secCmdBuf.get());
    }
    endCommandBuffer(vk, *primCmdBuf);

    submitCommandsAndWait(vk, vkDevice, queue, primCmdBuf.get());

    // check if secondary buffer has been executed
    VkResult result = vk.getEventStatus(vkDevice, *event);
    if (result == VK_EVENT_SET)
        return tcu::TestStatus::pass("executeSecondaryBufferTest succeeded");

    return tcu::TestStatus::fail("executeSecondaryBufferTest FAILED");
}

tcu::TestStatus executeNestedBufferTest(Context &context)
{
    const VkDevice vkDevice         = context.getDevice();
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkQueue queue             = context.getUniversalQueue();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();

    const VkCommandPoolCreateInfo cmdPoolParams = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,      // sType;
        nullptr,                                         // pNext;
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // flags;
        queueFamilyIndex,                                // queueFamilyIndex;
    };
    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, vkDevice, &cmdPoolParams));

    // Command buffer
    const VkCommandBufferAllocateInfo cmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // sType;
        nullptr,                                        // pNext;
        *cmdPool,                                       // commandPool;
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,                // level;
        1u,                                             // bufferCount;
    };
    const Unique<VkCommandBuffer> primCmdBuf(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

    // Secondary Command buffer
    const VkCommandBufferAllocateInfo secCmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // sType;
        nullptr,                                        // pNext;
        *cmdPool,                                       // commandPool;
        VK_COMMAND_BUFFER_LEVEL_SECONDARY,              // level;
        1u,                                             // bufferCount;
    };
    const Unique<VkCommandBuffer> secCmdBuf(allocateCommandBuffer(vk, vkDevice, &secCmdBufParams));

    // Nested secondary Command buffer
    const VkCommandBufferAllocateInfo nestedCmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // sType;
        nullptr,                                        // pNext;
        *cmdPool,                                       // commandPool;
        VK_COMMAND_BUFFER_LEVEL_SECONDARY,              // level;
        1u,                                             // bufferCount;
    };
    const Unique<VkCommandBuffer> nestedCmdBuf(allocateCommandBuffer(vk, vkDevice, &nestedCmdBufParams));

    const VkCommandBufferBeginInfo primCmdBufBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // sType
        nullptr,                                     // pNext
        0u,                                          // flags
        nullptr,
    };

    const VkCommandBufferInheritanceInfo secCmdBufInheritInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
        nullptr,
        VK_NULL_HANDLE,                    // renderPass
        0u,                                // subpass
        VK_NULL_HANDLE,                    // framebuffer
        VK_FALSE,                          // occlusionQueryEnable
        (VkQueryControlFlags)0u,           // queryFlags
        (VkQueryPipelineStatisticFlags)0u, // pipelineStatistics
    };
    const VkCommandBufferBeginInfo secCmdBufBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // sType
        nullptr,                                     // pNext
        0u,                                          // flags
        &secCmdBufInheritInfo,
    };

    const VkCommandBufferInheritanceInfo nestedCmdBufInheritInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
        nullptr,
        VK_NULL_HANDLE,                    // renderPass
        0u,                                // subpass
        VK_NULL_HANDLE,                    // framebuffer
        VK_FALSE,                          // occlusionQueryEnable
        (VkQueryControlFlags)0u,           // queryFlags
        (VkQueryPipelineStatisticFlags)0u, // pipelineStatistics
    };
    const VkCommandBufferBeginInfo nestedCmdBufBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // sType
        nullptr,                                     // pNext
        0u,                                          // flags
        &nestedCmdBufInheritInfo,
    };

    // create event that will be used to check if nested command buffer has been executed
    const Unique<VkEvent> event(createEvent(vk, vkDevice));

    // reset event
    VK_CHECK(vk.resetEvent(vkDevice, *event));

    // record nested command buffer
    VK_CHECK(vk.beginCommandBuffer(*nestedCmdBuf, &nestedCmdBufBeginInfo));
    {
        // allow execution of event during every stage of pipeline
        VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        // record setting event
        vk.cmdSetEvent(*nestedCmdBuf, *event, stageMask);
    }
    // end recording of the secondary buffer
    endCommandBuffer(vk, *nestedCmdBuf);

    // record secondary command buffer
    VK_CHECK(vk.beginCommandBuffer(*secCmdBuf, &secCmdBufBeginInfo));
    {
        // execute nested buffer
        vk.cmdExecuteCommands(*secCmdBuf, 1u, &nestedCmdBuf.get());
    }
    // end recording of the secondary buffer
    endCommandBuffer(vk, *secCmdBuf);

    // record primary command buffer
    VK_CHECK(vk.beginCommandBuffer(*primCmdBuf, &primCmdBufBeginInfo));
    {
        // execute secondary buffer
        vk.cmdExecuteCommands(*primCmdBuf, 1u, &secCmdBuf.get());
    }
    endCommandBuffer(vk, *primCmdBuf);

    submitCommandsAndWait(vk, vkDevice, queue, primCmdBuf.get());

    // check if secondary buffer has been executed
    VkResult result = vk.getEventStatus(vkDevice, *event);
    if (result == VK_EVENT_SET)
        return tcu::TestStatus::pass("executeNestedBufferTest succeeded");

    return tcu::TestStatus::fail("executeNestedBufferTest FAILED");
}

tcu::TestStatus executeMultipleLevelsNestedBufferTest(Context &context)
{
    const uint32_t BUFFER_COUNT     = 2u;
    const VkDevice vkDevice         = context.getDevice();
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkQueue queue             = context.getUniversalQueue();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();

    const VkCommandPoolCreateInfo cmdPoolParams = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,      // sType;
        nullptr,                                         // pNext;
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // flags;
        queueFamilyIndex,                                // queueFamilyIndex;
    };
    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, vkDevice, &cmdPoolParams));

    // Command buffer
    const VkCommandBufferAllocateInfo cmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // sType;
        nullptr,                                        // pNext;
        *cmdPool,                                       // commandPool;
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,                // level;
        1u,                                             // bufferCount;
    };
    const Unique<VkCommandBuffer> primCmdBuf(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

    // Secondary Command buffers
    const VkCommandBufferAllocateInfo secCmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        *cmdPool,                                       // VkCommandPool pool;
        VK_COMMAND_BUFFER_LEVEL_SECONDARY,              // VkCommandBufferLevel level;
        BUFFER_COUNT,                                   // uint32_t bufferCount;
    };
    Move<VkCommandBuffer> nestedBuffers[BUFFER_COUNT];
    allocateCommandBuffers(vk, vkDevice, &secCmdBufParams, nestedBuffers);

    const VkCommandBufferBeginInfo primCmdBufBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // sType
        nullptr,                                     // pNext
        0u,                                          // flags
        nullptr,
    };

    const VkCommandBufferInheritanceInfo nestedCmdBufInheritInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
        nullptr,
        VK_NULL_HANDLE,                    // renderPass
        0u,                                // subpass
        VK_NULL_HANDLE,                    // framebuffer
        VK_FALSE,                          // occlusionQueryEnable
        (VkQueryControlFlags)0u,           // queryFlags
        (VkQueryPipelineStatisticFlags)0u, // pipelineStatistics
    };
    const VkCommandBufferBeginInfo nestedCmdBufBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // sType
        nullptr,                                     // pNext
        0u,                                          // flags
        &nestedCmdBufInheritInfo,
    };

    // create event that will be used to check if secondary command buffer has been executed
    const Unique<VkEvent> event(createEvent(vk, vkDevice));

    // reset event
    VK_CHECK(vk.resetEvent(vkDevice, *event));

    // record nested secondary command buffers
    VK_CHECK(vk.beginCommandBuffer(*(nestedBuffers[0]), &nestedCmdBufBeginInfo));
    {
        // allow execution of event during every stage of pipeline
        VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        // record setting event
        vk.cmdSetEvent(*(nestedBuffers[0]), *event, stageMask);
    }
    // end recording of the secondary buffer
    endCommandBuffer(vk, *(nestedBuffers[0]));

    for (uint32_t ndx = 1; ndx < BUFFER_COUNT; ndx++)
    {
        VK_CHECK(vk.beginCommandBuffer(*(nestedBuffers[ndx]), &nestedCmdBufBeginInfo));
        {
            // execute nested buffer
            vk.cmdExecuteCommands(*(nestedBuffers[ndx]), 1u, &(nestedBuffers[ndx - 1]).get());
        }
        // end recording of the secondary buffer
        endCommandBuffer(vk, *(nestedBuffers[ndx]));
    }

    // record primary command buffer
    VK_CHECK(vk.beginCommandBuffer(*primCmdBuf, &primCmdBufBeginInfo));
    {
        // execute secondary buffer
        vk.cmdExecuteCommands(*primCmdBuf, 1u, &(nestedBuffers[BUFFER_COUNT - 1]).get());
    }
    endCommandBuffer(vk, *primCmdBuf);

    submitCommandsAndWait(vk, vkDevice, queue, primCmdBuf.get());

    // check if secondary buffer has been executed
    VkResult result = vk.getEventStatus(vkDevice, *event);
    if (result == VK_EVENT_SET)
        return tcu::TestStatus::pass("executeMultipleLevelsNestedBufferTest succeeded");

    return tcu::TestStatus::fail("executeMultipleLevelsNestedBufferTest FAILED");
}

tcu::TestStatus executeSecondaryBufferTwiceTest(Context &context)
{
    const uint32_t BUFFER_COUNT     = 10u;
    const VkDevice vkDevice         = context.getDevice();
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkQueue queue             = context.getUniversalQueue();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();

    const VkCommandPoolCreateInfo cmdPoolParams = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,      // VkStructureType sType;
        nullptr,                                         // const void* pNext;
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // VkCommandPoolCreateFlags flags;
        queueFamilyIndex,                                // uint32_t queueFamilyIndex;
    };
    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, vkDevice, &cmdPoolParams));

    // Command buffer
    const VkCommandBufferAllocateInfo cmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        *cmdPool,                                       // VkCommandPool pool;
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,                // VkCommandBufferLevel level;
        1u,                                             // uint32_t bufferCount;
    };
    const Unique<VkCommandBuffer> primCmdBufOne(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));
    const Unique<VkCommandBuffer> primCmdBufTwo(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

    // Secondary Command buffers params
    const VkCommandBufferAllocateInfo secCmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        *cmdPool,                                       // VkCommandPool pool;
        VK_COMMAND_BUFFER_LEVEL_SECONDARY,              // VkCommandBufferLevel level;
        BUFFER_COUNT,                                   // uint32_t bufferCount;
    };
    Move<VkCommandBuffer> cmdBuffers[BUFFER_COUNT];
    allocateCommandBuffers(vk, vkDevice, &secCmdBufParams, cmdBuffers);

    const VkCommandBufferBeginInfo primCmdBufBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        nullptr,
        0, // flags
        nullptr,
    };

    const VkCommandBufferInheritanceInfo secCmdBufInheritInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
        nullptr,
        VK_NULL_HANDLE,                    // renderPass
        0u,                                // subpass
        VK_NULL_HANDLE,                    // framebuffer
        VK_FALSE,                          // occlusionQueryEnable
        (VkQueryControlFlags)0u,           // queryFlags
        (VkQueryPipelineStatisticFlags)0u, // pipelineStatistics
    };
    const VkCommandBufferBeginInfo secCmdBufBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        nullptr,
        VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT, // flags
        &secCmdBufInheritInfo,
    };

    // create event that will be used to check if secondary command buffer has been executed
    const Unique<VkEvent> eventOne(createEvent(vk, vkDevice));

    // reset event
    VK_CHECK(vk.resetEvent(vkDevice, *eventOne));

    VkCommandBuffer cmdBufferHandles[BUFFER_COUNT];

    for (uint32_t ndx = 0; ndx < BUFFER_COUNT; ++ndx)
    {
        // record secondary command buffer
        VK_CHECK(vk.beginCommandBuffer(cmdBuffers[ndx].get(), &secCmdBufBeginInfo));
        {
            // set event
            vk.cmdSetEvent(cmdBuffers[ndx].get(), *eventOne, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
        }
        // end recording of secondary buffers
        endCommandBuffer(vk, cmdBuffers[ndx].get());
        cmdBufferHandles[ndx] = cmdBuffers[ndx].get();
    }

    // record primary command buffer one
    VK_CHECK(vk.beginCommandBuffer(*primCmdBufOne, &primCmdBufBeginInfo));
    {
        // execute one secondary buffer
        vk.cmdExecuteCommands(*primCmdBufOne, 1, cmdBufferHandles);
    }
    endCommandBuffer(vk, *primCmdBufOne);

    // record primary command buffer two
    VK_CHECK(vk.beginCommandBuffer(*primCmdBufTwo, &primCmdBufBeginInfo));
    {
        // execute one secondary buffer with all buffers
        vk.cmdExecuteCommands(*primCmdBufTwo, BUFFER_COUNT, cmdBufferHandles);
    }
    endCommandBuffer(vk, *primCmdBufTwo);

    // create fence to wait for execution of queue
    const Unique<VkFence> fenceOne(createFence(vk, vkDevice));
    const Unique<VkFence> fenceTwo(createFence(vk, vkDevice));

    const uint64_t semaphoreWaitValue             = 1ull;
    const VkPipelineStageFlags semaphoreWaitStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    const auto semaphore                          = createSemaphoreType(vk, vkDevice, VK_SEMAPHORE_TYPE_TIMELINE);

    // Use timeline semaphore to wait for signal from the host.
    const VkTimelineSemaphoreSubmitInfo timelineWaitSubmitInfo = {
        VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO, // VkStructureType sType;
        nullptr,                                          // const void* pNext;
        1u,                                               // uint32_t waitSemaphoreValueCount;
        &semaphoreWaitValue,                              // const uint64_t* pWaitSemaphoreValues;
        0u,                                               // uint32_t signalSemaphoreValueCount;
        nullptr,                                          // const uint64_t* pSignalSemaphoreValues;
    };

    const VkSubmitInfo submitInfo = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO, // VkStructureType sType;
        &timelineWaitSubmitInfo,       // const void* pNext;
        1u,                            // uint32_t waitSemaphoreCount;
        &semaphore.get(),              // const VkSemaphore* pWaitSemaphores;
        &semaphoreWaitStage,           // const VkPipelineStageFlags* pWaitDstStageMask;
        1u,                            // uint32_t commandBufferCount;
        &primCmdBufOne.get(),          // const VkCommandBuffer* pCommandBuffers;
        0u,                            // uint32_t signalSemaphoreCount;
        nullptr,                       // const VkSemaphore* pSignalSemaphores;
    };
    VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo, *fenceOne));

    const VkSubmitInfo submitInfo2 = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO, // VkStructureType sType;
        &timelineWaitSubmitInfo,       // const void* pNext;
        1u,                            // uint32_t waitSemaphoreCount;
        &semaphore.get(),              // const VkSemaphore* pWaitSemaphores;
        &semaphoreWaitStage,           // const VkPipelineStageFlags* pWaitDstStageMask;
        1u,                            // uint32_t commandBufferCount;
        &primCmdBufTwo.get(),          // const VkCommandBuffer* pCommandBuffers;
        0u,                            // uint32_t signalSemaphoreCount;
        nullptr,                       // const VkSemaphore* pSignalSemaphores;
    };

    VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo2, *fenceTwo));

    // Signal from host
    const vk::VkSemaphoreSignalInfo signalInfo = {
        vk::VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO, // VkStructureType sType;
        nullptr,                                     // const void* pNext;
        semaphore.get(),                             // VkSemaphore semaphore;
        semaphoreWaitValue,                          // uint64_t value;
    };

    VK_CHECK(vk.signalSemaphore(vkDevice, &signalInfo));

    // wait for end of execution of fenceOne
    VK_CHECK(vk.waitForFences(vkDevice, 1, &fenceOne.get(), 0u, INFINITE_TIMEOUT));

    // wait for end of execution of fenceTwo
    VK_CHECK(vk.waitForFences(vkDevice, 1, &fenceTwo.get(), 0u, INFINITE_TIMEOUT));

    TCU_CHECK(vk.getEventStatus(vkDevice, *eventOne) == vk::VK_EVENT_SET);

    return tcu::TestStatus::pass("executeSecondaryBufferTwiceTest succeeded");
}

/******** 19.6. Commands Allowed Inside Command Buffers (? in VK 1.0 Spec) **/
tcu::TestStatus orderBindPipelineTest(Context &context)
{
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkDevice device           = context.getDevice();
    const VkQueue queue             = context.getUniversalQueue();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = context.getDefaultAllocator();
    const ComputeInstanceResultBuffer result(vk, device, allocator);

    enum
    {
        ADDRESSABLE_SIZE = 256, // allocate a lot more than required
    };

    const tcu::Vec4 colorA1 = tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f);
    const tcu::Vec4 colorA2 = tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f);
    const tcu::Vec4 colorB1 = tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f);
    const tcu::Vec4 colorB2 = tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f);

    const uint32_t dataOffsetA = (0u);
    const uint32_t dataOffsetB = (0u);
    const uint32_t viewOffsetA = (0u);
    const uint32_t viewOffsetB = (0u);
    const uint32_t bufferSizeA = dataOffsetA + ADDRESSABLE_SIZE;
    const uint32_t bufferSizeB = dataOffsetB + ADDRESSABLE_SIZE;

    de::MovePtr<Allocation> bufferMemA;
    const Unique<VkBuffer> bufferA(
        createColorDataBuffer(dataOffsetA, bufferSizeA, colorA1, colorA2, &bufferMemA, context));

    de::MovePtr<Allocation> bufferMemB;
    const Unique<VkBuffer> bufferB(
        createColorDataBuffer(dataOffsetB, bufferSizeB, colorB1, colorB2, &bufferMemB, context));

    const Unique<VkDescriptorSetLayout> descriptorSetLayout(createDescriptorSetLayout(context));
    const Unique<VkDescriptorPool> descriptorPool(createDescriptorPool(context));
    const Unique<VkDescriptorSet> descriptorSet(createDescriptorSet(*descriptorPool, *descriptorSetLayout, *bufferA,
                                                                    viewOffsetA, *bufferB, viewOffsetB,
                                                                    result.getBuffer(), context));
    const VkDescriptorSet descriptorSets[] = {*descriptorSet};
    const int numDescriptorSets            = DE_LENGTH_OF_ARRAY(descriptorSets);

    const VkPipelineLayoutCreateInfo layoutCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // sType
        nullptr,                                       // pNext
        (VkPipelineLayoutCreateFlags)0,
        numDescriptorSets,          // setLayoutCount
        &descriptorSetLayout.get(), // pSetLayouts
        0u,                         // pushConstantRangeCount
        nullptr,                    // pPushConstantRanges
    };
    Unique<VkPipelineLayout> pipelineLayout(createPipelineLayout(vk, device, &layoutCreateInfo));

    const Unique<VkShaderModule> computeModuleGood(createShaderModule(
        vk, device, context.getBinaryCollection().get("compute_good"), (VkShaderModuleCreateFlags)0u));
    const Unique<VkShaderModule> computeModuleBad(createShaderModule(
        vk, device, context.getBinaryCollection().get("compute_bad"), (VkShaderModuleCreateFlags)0u));

    const VkPipelineShaderStageCreateInfo shaderCreateInfoGood = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        nullptr,
        (VkPipelineShaderStageCreateFlags)0,
        VK_SHADER_STAGE_COMPUTE_BIT, // stage
        *computeModuleGood,          // shader
        "main",
        nullptr, // pSpecializationInfo
    };

    const VkPipelineShaderStageCreateInfo shaderCreateInfoBad = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        nullptr,
        (vk::VkPipelineShaderStageCreateFlags)0,
        vk::VK_SHADER_STAGE_COMPUTE_BIT, // stage
        *computeModuleBad,               // shader
        "main",
        nullptr, // pSpecializationInfo
    };

    const VkComputePipelineCreateInfo createInfoGood = {
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        nullptr,
        0u,                   // flags
        shaderCreateInfoGood, // cs
        *pipelineLayout,      // layout
        VK_NULL_HANDLE,       // basePipelineHandle
        0u,                   // basePipelineIndex
    };

    const VkComputePipelineCreateInfo createInfoBad = {
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        nullptr,
        0u,                  // flags
        shaderCreateInfoBad, // cs
        *pipelineLayout,     // descriptorSetLayout.get()
        VK_NULL_HANDLE,      // basePipelineHandle
        0u,                  // basePipelineIndex
    };

    const Unique<VkPipeline> pipelineGood(createComputePipeline(vk, device, VK_NULL_HANDLE, &createInfoGood));
    const Unique<VkPipeline> pipelineBad(createComputePipeline(vk, device, VK_NULL_HANDLE, &createInfoBad));

    const VkAccessFlags inputBit                 = (VK_ACCESS_UNIFORM_READ_BIT);
    const VkBufferMemoryBarrier bufferBarriers[] = {{
                                                        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, nullptr,
                                                        VK_ACCESS_HOST_WRITE_BIT,  // srcAccessMask
                                                        inputBit,                  // dstAccessMask
                                                        VK_QUEUE_FAMILY_IGNORED,   // srcQueueFamilyIndex
                                                        VK_QUEUE_FAMILY_IGNORED,   // destQueueFamilyIndex
                                                        *bufferA,                  // buffer
                                                        (VkDeviceSize)0u,          // offset
                                                        (VkDeviceSize)bufferSizeA, // size
                                                    },
                                                    {
                                                        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, nullptr,
                                                        VK_ACCESS_HOST_WRITE_BIT,  // srcAccessMask
                                                        inputBit,                  // dstAccessMask
                                                        VK_QUEUE_FAMILY_IGNORED,   // srcQueueFamilyIndex
                                                        VK_QUEUE_FAMILY_IGNORED,   // destQueueFamilyIndex
                                                        *bufferB,                  // buffer
                                                        (VkDeviceSize)0u,          // offset
                                                        (VkDeviceSize)bufferSizeB, // size
                                                    }};

    const uint32_t numSrcBuffers = 1u;

    const uint32_t *const dynamicOffsets                = (nullptr);
    const uint32_t numDynamicOffsets                    = (0);
    const int numPreBarriers                            = numSrcBuffers;
    const vk::VkBufferMemoryBarrier *const postBarriers = result.getResultReadBarrier();
    const int numPostBarriers                           = 1;
    const tcu::Vec4 refQuadrantValue14                  = (colorA2);
    const tcu::Vec4 refQuadrantValue23                  = (colorA1);
    const tcu::Vec4 references[4]                       = {
        refQuadrantValue14,
        refQuadrantValue23,
        refQuadrantValue23,
        refQuadrantValue14,
    };
    tcu::Vec4 results[4];

    // submit and wait begin

    const tcu::UVec3 numWorkGroups = tcu::UVec3(4, 1u, 1);

    const VkCommandPoolCreateInfo cmdPoolCreateInfo = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, // sType;
        nullptr,                                    // pNext
        VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,       // flags
        queueFamilyIndex,                           // queueFamilyIndex
    };
    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, device, &cmdPoolCreateInfo));
    const VkCommandBufferAllocateInfo cmdBufCreateInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // sType
        nullptr,                                        // pNext
        *cmdPool,                                       // commandPool
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,                // level
        1u,                                             // bufferCount;
    };

    const VkCommandBufferBeginInfo cmdBufBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // sType
        nullptr,                                     // pNext
        0u,                                          // flags
        nullptr,
    };

    const Unique<VkCommandBuffer> cmd(allocateCommandBuffer(vk, device, &cmdBufCreateInfo));

    VK_CHECK(vk.beginCommandBuffer(*cmd, &cmdBufBeginInfo));

    vk.cmdBindPipeline(*cmd, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineBad);
    vk.cmdBindPipeline(*cmd, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineGood);
    vk.cmdBindDescriptorSets(*cmd, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0, numDescriptorSets,
                             descriptorSets, numDynamicOffsets, dynamicOffsets);

    if (numPreBarriers)
        vk.cmdPipelineBarrier(*cmd, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                              (VkDependencyFlags)0, 0, nullptr, numPreBarriers, bufferBarriers, 0, nullptr);

    vk.cmdDispatch(*cmd, numWorkGroups.x(), numWorkGroups.y(), numWorkGroups.z());
    vk.cmdPipelineBarrier(*cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0,
                          0, nullptr, numPostBarriers, postBarriers, 0, nullptr);
    endCommandBuffer(vk, *cmd);

    // run
    // submit second primary buffer, the secondary should be executed too
    submitCommandsAndWait(vk, device, queue, cmd.get());

    // submit and wait end
    result.readResultContentsTo(&results);

    // verify
    if (results[0] == references[0] && results[1] == references[1] && results[2] == references[2] &&
        results[3] == references[3])
    {
        return tcu::TestStatus::pass("Pass");
    }
    else if (results[0] == tcu::Vec4(-1.0f) && results[1] == tcu::Vec4(-1.0f) && results[2] == tcu::Vec4(-1.0f) &&
             results[3] == tcu::Vec4(-1.0f))
    {
        context.getTestContext().getLog()
            << tcu::TestLog::Message << "Result buffer was not written to." << tcu::TestLog::EndMessage;
        return tcu::TestStatus::fail("Result buffer was not written to");
    }
    else
    {
        context.getTestContext().getLog()
            << tcu::TestLog::Message << "Error expected [" << references[0] << ", " << references[1] << ", "
            << references[2] << ", " << references[3] << "], got [" << results[0] << ", " << results[1] << ", "
            << results[2] << ", " << results[3] << "]" << tcu::TestLog::EndMessage;
        return tcu::TestStatus::fail("Invalid result values");
    }
}

enum StateTransitionTest
{
    STT_RECORDING_TO_INITIAL = 0,
    STT_EXECUTABLE_TO_INITIAL,
    STT_RECORDING_TO_INVALID,
    STT_EXECUTABLE_TO_INVALID,
};

tcu::TestStatus executeStateTransitionTest(Context &context, StateTransitionTest type)
{
    const VkDevice vkDevice         = context.getDevice();
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkQueue queue             = context.getUniversalQueue();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();

#ifdef CTS_USES_VULKANSC
    if (context.getDeviceVulkanSC10Properties().commandPoolResetCommandBuffer == VK_FALSE)
        TCU_THROW(NotSupportedError, "commandPoolResetCommandBuffer not supported by this implementation");
#endif // CTS_USES_VULKANSC

    const Unique<VkCommandPool> cmdPool(
        createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, vkDevice, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    const Unique<VkEvent> globalEvent(createEvent(vk, vkDevice));

    VK_CHECK(vk.resetEvent(vkDevice, *globalEvent));

    switch (type)
    {
    case STT_RECORDING_TO_INITIAL:
    {
        beginCommandBuffer(vk, *cmdBuffer, 0u);
        vk.cmdSetEvent(*cmdBuffer, *globalEvent, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        break;
        // command buffer is still in recording state
    }
    case STT_EXECUTABLE_TO_INITIAL:
    {
        beginCommandBuffer(vk, *cmdBuffer, 0u);
        vk.cmdSetEvent(*cmdBuffer, *globalEvent, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        endCommandBuffer(vk, *cmdBuffer);
        break;
        // command buffer is still in executable state
    }
    case STT_RECORDING_TO_INVALID:
    {
        VkSubpassDescription subpassDescription;
        deMemset(&subpassDescription, 0, sizeof(VkSubpassDescription));
        subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        VkRenderPassCreateInfo renderPassCreateInfo{
            VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, nullptr, 0, 0, nullptr, 1, &subpassDescription, 0, nullptr};

        // Error here - renderpass and framebuffer were created localy
        Move<VkRenderPass> renderPass = createRenderPass(vk, vkDevice, &renderPassCreateInfo);

        VkFramebufferCreateInfo framebufferCreateInfo{
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, nullptr, 0, *renderPass, 0, nullptr, 16, 16, 1};
        Move<VkFramebuffer> framebuffer = createFramebuffer(vk, vkDevice, &framebufferCreateInfo);

        VkRenderPassBeginInfo renderPassBeginInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                                     nullptr,
                                                     *renderPass,
                                                     *framebuffer,
                                                     {{0, 0}, {16, 16}},
                                                     0,
                                                     nullptr};

        beginCommandBuffer(vk, *cmdBuffer, 0u);
        vk.cmdBeginRenderPass(*cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vk.cmdEndRenderPass(*cmdBuffer);

        // not executing endCommandBuffer(vk, *cmdBuffer);
        // command buffer is still in recording state
        break;
        // renderpass and framebuffer are destroyed; command buffer should be now in invalid state
    }
    case STT_EXECUTABLE_TO_INVALID:
    {
        // create event that will be used to check if command buffer has been executed
        const Unique<VkEvent> localEvent(createEvent(vk, vkDevice));
        VK_CHECK(vk.resetEvent(vkDevice, *localEvent));

        beginCommandBuffer(vk, *cmdBuffer, 0u);
        vk.cmdSetEvent(*cmdBuffer, *localEvent, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        endCommandBuffer(vk, *cmdBuffer);
        // command buffer is in executable state
        break;
        // localEvent is destroyed; command buffer should be now in invalid state
    }
    }

    VK_CHECK(vk.resetEvent(vkDevice, *globalEvent));

    VK_CHECK(vk.resetCommandBuffer(*cmdBuffer, 0u));
    // command buffer should now be back in initial state

    // verify commandBuffer
    beginCommandBuffer(vk, *cmdBuffer, 0u);
    vk.cmdSetEvent(*cmdBuffer, *globalEvent, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    endCommandBuffer(vk, *cmdBuffer);
    submitCommandsAndWait(vk, vkDevice, queue, *cmdBuffer);

    // check if buffer has been executed
    VkResult result = vk.getEventStatus(vkDevice, *globalEvent);
    if (result != VK_EVENT_SET)
        return tcu::TestStatus::fail("Submit failed");

    return tcu::TestStatus::pass("Pass");
}

// Shaders
void genComputeSource(SourceCollections &programCollection)
{
    const char *const versionDecl = glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES);
    std::ostringstream bufGood;

    bufGood << versionDecl << "\n"
            << ""
            << "layout(local_size_x = 1u, local_size_y = 1u, local_size_z = 1u) in;\n"
            << "layout(set = 0, binding = 1u, std140) uniform BufferName\n"
            << "{\n"
            << "    highp vec4 colorA;\n"
            << "    highp vec4 colorB;\n"
            << "} b_instance;\n"
            << "layout(set = 0, binding = 0, std140) writeonly buffer OutBuf\n"
            << "{\n"
            << "    highp vec4 read_colors[4];\n"
            << "} b_out;\n"
            << "void main(void)\n"
            << "{\n"
            << "    highp int quadrant_id = int(gl_WorkGroupID.x);\n"
            << "    highp vec4 result_color;\n"
            << "    if (quadrant_id == 1 || quadrant_id == 2)\n"
            << "        result_color = b_instance.colorA;\n"
            << "    else\n"
            << "        result_color = b_instance.colorB;\n"
            << "    b_out.read_colors[gl_WorkGroupID.x] = result_color;\n"
            << "}\n";

    programCollection.glslSources.add("compute_good") << glu::ComputeSource(bufGood.str());

    std::ostringstream bufBad;

    bufBad << versionDecl << "\n"
           << ""
           << "layout(local_size_x = 1u, local_size_y = 1u, local_size_z = 1u) in;\n"
           << "layout(set = 0, binding = 1u, std140) uniform BufferName\n"
           << "{\n"
           << "    highp vec4 colorA;\n"
           << "    highp vec4 colorB;\n"
           << "} b_instance;\n"
           << "layout(set = 0, binding = 0, std140) writeonly buffer OutBuf\n"
           << "{\n"
           << "    highp vec4 read_colors[4];\n"
           << "} b_out;\n"
           << "void main(void)\n"
           << "{\n"
           << "    highp int quadrant_id = int(gl_WorkGroupID.x);\n"
           << "    highp vec4 result_color;\n"
           << "    if (quadrant_id == 1 || quadrant_id == 2)\n"
           << "        result_color = b_instance.colorA;\n"
           << "    else\n"
           << "        result_color = b_instance.colorB;\n"
           << "    b_out.read_colors[gl_WorkGroupID.x] = vec4(0.0, 0.0, 0.0, 0.0);\n"
           << "}\n";

    programCollection.glslSources.add("compute_bad") << glu::ComputeSource(bufBad.str());
}

void genComputeIncrementSource(SourceCollections &programCollection)
{
    const char *const versionDecl = glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES);
    std::ostringstream bufIncrement;

    bufIncrement << versionDecl << "\n"
                 << ""
                 << "layout(local_size_x = 1u, local_size_y = 1u, local_size_z = 1u) in;\n"
                 << "layout(set = 0, binding = 0, std140) buffer InOutBuf\n"
                 << "{\n"
                 << "    coherent uint count;\n"
                 << "} b_in_out;\n"
                 << "void main(void)\n"
                 << "{\n"
                 << "    atomicAdd(b_in_out.count, 1u);\n"
                 << "}\n";

    programCollection.glslSources.add("compute_increment") << glu::ComputeSource(bufIncrement.str());
}

void genComputeIncrementSourceBadInheritance(SourceCollections &programCollection, BadInheritanceInfoCase testCase)
{
    DE_UNREF(testCase);
    return genComputeIncrementSource(programCollection);
}

void checkEventSupport(Context &context)
{
#ifndef CTS_USES_VULKANSC
    if (context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") &&
        !context.getPortabilitySubsetFeatures().events)
        TCU_THROW(NotSupportedError, "VK_KHR_portability_subset: Events are not supported by this implementation");
#else
    DE_UNREF(context);
#endif // CTS_USES_VULKANSC
}

void checkCommandBufferSimultaneousUseSupport(Context &context)
{
#ifdef CTS_USES_VULKANSC
    if (context.getDeviceVulkanSC10Properties().commandBufferSimultaneousUse == VK_FALSE)
        TCU_THROW(NotSupportedError, "commandBufferSimultaneousUse is not supported");
#else
    DE_UNREF(context);
#endif // CTS_USES_VULKANSC
}

void checkSecondaryCommandBufferNullOrImagelessFramebufferSupport(Context &context)
{
#ifdef CTS_USES_VULKANSC
    if (context.getDeviceVulkanSC10Properties().secondaryCommandBufferNullOrImagelessFramebuffer == VK_FALSE)
        TCU_THROW(NotSupportedError, "secondaryCommandBufferNullFramebuffer is not supported");
#else
    DE_UNREF(context);
#endif // CTS_USES_VULKANSC
}

void checkSecondaryCommandBufferNullOrImagelessFramebufferSupport1(Context &context, bool value)
{
    DE_UNREF(value);
#ifdef CTS_USES_VULKANSC
    if (context.getDeviceVulkanSC10Properties().secondaryCommandBufferNullOrImagelessFramebuffer == VK_FALSE)
        TCU_THROW(NotSupportedError, "secondaryCommandBufferNullFramebuffer is not supported");
#else
    DE_UNREF(context);
#endif // CTS_USES_VULKANSC
}

void checkEventAndSecondaryCommandBufferNullFramebufferSupport(Context &context)
{
    checkEventSupport(context);
    checkSecondaryCommandBufferNullOrImagelessFramebufferSupport(context);
}

void checkSimultaneousUseAndSecondaryCommandBufferNullFramebufferSupport(Context &context)
{
    checkCommandBufferSimultaneousUseSupport(context);
    checkSecondaryCommandBufferNullOrImagelessFramebufferSupport(context);
}

void checkEventAndTimelineSemaphoreAndSimultaneousUseAndSecondaryCommandBufferNullFramebufferSupport(Context &context)
{
    checkEventSupport(context);
    context.requireDeviceFunctionality("VK_KHR_timeline_semaphore");

    checkSimultaneousUseAndSecondaryCommandBufferNullFramebufferSupport(context);
}

void checkNestedCommandBufferSupport(Context &context)
{
    checkEventAndSecondaryCommandBufferNullFramebufferSupport(context);
    context.requireDeviceFunctionality("VK_EXT_nested_command_buffer");

#ifndef CTS_USES_VULKANSC
    const auto &features = context.getNestedCommandBufferFeaturesEXT();
    if (!features.nestedCommandBuffer)
#endif // CTS_USES_VULKANSC
        TCU_THROW(NotSupportedError, "nestedCommandBuffer is not supported");
}

void checkNestedCommandBufferDepthSupport(Context &context)
{
    checkNestedCommandBufferSupport(context);

#ifndef CTS_USES_VULKANSC
    const auto &properties = context.getNestedCommandBufferPropertiesEXT();
    if (properties.maxCommandBufferNestingLevel <= 1)
#endif // CTS_USES_VULKANSC
        TCU_THROW(NotSupportedError, "nestedCommandBuffer with nesting level greater than 1 is not supported");
}

void checkNestedCommandBufferRenderPassContinueSupport(Context &context, bool value)
{
    checkNestedCommandBufferSupport(context);

    DE_UNREF(value);
#ifndef CTS_USES_VULKANSC
    const auto &features = context.getNestedCommandBufferFeaturesEXT();
    if (!features.nestedCommandBufferRendering)
#endif // CTS_USES_VULKANSC
        TCU_THROW(NotSupportedError, "nestedCommandBufferRendering is not supported");
}

void checkSimultaneousUseAndNestedCommandBufferNullFramebufferSupport(Context &context)
{
    checkSimultaneousUseAndSecondaryCommandBufferNullFramebufferSupport(context);
    checkNestedCommandBufferSupport(context);
#ifndef CTS_USES_VULKANSC
    const auto &features = context.getNestedCommandBufferFeaturesEXT();
    if (!features.nestedCommandBufferSimultaneousUse)
#endif // CTS_USES_VULKANSC
        TCU_THROW(NotSupportedError, "nestedCommandBufferSimultaneousUse is not supported");
}

#ifndef CTS_USES_VULKANSC
void checkEventSupport(Context &context, const VkCommandBufferLevel)
{
    checkEventSupport(context);
}
#endif // CTS_USES_VULKANSC

struct ManyDrawsParams
{
    VkCommandBufferLevel level;
    VkExtent3D imageExtent;
    uint32_t seed;

    ManyDrawsParams(VkCommandBufferLevel level_, const VkExtent3D &extent_, uint32_t seed_)
        : level(level_)
        , imageExtent(extent_)
        , seed(seed_)
    {
    }
};

struct ManyDrawsVertex
{
    using Color = tcu::Vector<uint8_t, 4>;

    tcu::Vec2 coords;
    Color color;

    ManyDrawsVertex(const tcu::Vec2 &coords_, const Color &color_) : coords(coords_), color(color_)
    {
    }
};

VkFormat getSupportedDepthStencilFormat(const InstanceInterface &vki, VkPhysicalDevice physDev)
{
    const VkFormat formatList[] = {VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT};
    const VkFormatFeatureFlags requirements =
        (VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT);

    for (int i = 0; i < DE_LENGTH_OF_ARRAY(formatList); ++i)
    {
        const auto properties = getPhysicalDeviceFormatProperties(vki, physDev, formatList[i]);
        if ((properties.optimalTilingFeatures & requirements) == requirements)
            return formatList[i];
    }

    TCU_THROW(NotSupportedError, "No suitable depth/stencil format support");
    return VK_FORMAT_UNDEFINED;
}

class ManyDrawsCase : public TestCase
{
public:
    ManyDrawsCase(tcu::TestContext &testCtx, const std::string &name, const ManyDrawsParams &params);
    virtual ~ManyDrawsCase(void)
    {
    }

    virtual void checkSupport(Context &context) const;
    virtual void initPrograms(vk::SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const;

    static VkFormat getColorFormat(void)
    {
        return VK_FORMAT_R8G8B8A8_UINT;
    }

protected:
    ManyDrawsParams m_params;
};

class ManyDrawsInstance : public TestInstance
{
public:
    ManyDrawsInstance(Context &context, const ManyDrawsParams &params);
    virtual ~ManyDrawsInstance(void)
    {
    }

    virtual tcu::TestStatus iterate(void);

protected:
    ManyDrawsParams m_params;
};

using BufferPtr = de::MovePtr<BufferWithMemory>;
using ImagePtr  = de::MovePtr<ImageWithMemory>;

struct ManyDrawsVertexBuffers
{
    BufferPtr stagingBuffer;
    BufferPtr vertexBuffer;
};

struct ManyDrawsAllocatedData
{
    ManyDrawsVertexBuffers frontBuffers;
    ManyDrawsVertexBuffers backBuffers;
    ImagePtr colorAttachment;
    ImagePtr dsAttachment;
    BufferPtr colorCheckBuffer;
    BufferPtr stencilCheckBuffer;

    static uint32_t calcNumPixels(const VkExtent3D &extent)
    {
        DE_ASSERT(extent.depth == 1u);
        return (extent.width * extent.height);
    }
    static uint32_t calcNumVertices(const VkExtent3D &extent)
    {
        // One triangle (3 vertices) per output image pixel.
        return (calcNumPixels(extent) * 3u);
    }

    static VkDeviceSize calcVertexBufferSize(const VkExtent3D &extent)
    {
        return calcNumVertices(extent) * sizeof(ManyDrawsVertex);
    }

    static void makeVertexBuffers(const DeviceInterface &vkd, VkDevice device, Allocator &alloc, VkDeviceSize size,
                                  ManyDrawsVertexBuffers &buffers)
    {
        const auto stagingBufferInfo = makeBufferCreateInfo(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        const auto vertexBufferInfo =
            makeBufferCreateInfo(size, (VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT));

        buffers.stagingBuffer =
            BufferPtr(new BufferWithMemory(vkd, device, alloc, stagingBufferInfo, MemoryRequirement::HostVisible));
        buffers.vertexBuffer =
            BufferPtr(new BufferWithMemory(vkd, device, alloc, vertexBufferInfo, MemoryRequirement::Any));
    }

    ManyDrawsAllocatedData(const DeviceInterface &vkd, VkDevice device, Allocator &alloc, const VkExtent3D &imageExtent,
                           VkFormat colorFormat, VkFormat dsFormat)
    {
        const auto numPixels        = calcNumPixels(imageExtent);
        const auto vertexBufferSize = calcVertexBufferSize(imageExtent);

        makeVertexBuffers(vkd, device, alloc, vertexBufferSize, frontBuffers);
        makeVertexBuffers(vkd, device, alloc, vertexBufferSize, backBuffers);

        const auto colorUsage = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
        const auto dsUsage    = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        const VkImageCreateInfo colorAttachmentInfo = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
            nullptr,                             // const void* pNext;
            0u,                                  // VkImageCreateFlags flags;
            VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
            colorFormat,                         // VkFormat format;
            imageExtent,                         // VkExtent3D extent;
            1u,                                  // uint32_t mipLevels;
            1u,                                  // uint32_t arrayLayers;
            VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
            VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
            colorUsage,                          // VkImageUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
            0u,                                  // uint32_t queueFamilyIndexCount;
            nullptr,                             // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
        };
        colorAttachment =
            ImagePtr(new ImageWithMemory(vkd, device, alloc, colorAttachmentInfo, MemoryRequirement::Any));

        const VkImageCreateInfo dsAttachmentInfo = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
            nullptr,                             // const void* pNext;
            0u,                                  // VkImageCreateFlags flags;
            VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
            dsFormat,                            // VkFormat format;
            imageExtent,                         // VkExtent3D extent;
            1u,                                  // uint32_t mipLevels;
            1u,                                  // uint32_t arrayLayers;
            VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
            VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
            dsUsage,                             // VkImageUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
            0u,                                  // uint32_t queueFamilyIndexCount;
            nullptr,                             // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
        };
        dsAttachment = ImagePtr(new ImageWithMemory(vkd, device, alloc, dsAttachmentInfo, MemoryRequirement::Any));

        const auto colorCheckBufferSize =
            static_cast<VkDeviceSize>(numPixels * tcu::getPixelSize(mapVkFormat(colorFormat)));
        const auto colorCheckBufferInfo = makeBufferCreateInfo(colorCheckBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);

        colorCheckBuffer =
            BufferPtr(new BufferWithMemory(vkd, device, alloc, colorCheckBufferInfo, MemoryRequirement::HostVisible));

        const auto stencilFormat = tcu::TextureFormat(tcu::TextureFormat::S, tcu::TextureFormat::UNSIGNED_INT8);
        const auto stencilCheckBufferSize = static_cast<VkDeviceSize>(numPixels * tcu::getPixelSize(stencilFormat));
        const auto stencilCheckBufferInfo =
            makeBufferCreateInfo(stencilCheckBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);

        stencilCheckBuffer =
            BufferPtr(new BufferWithMemory(vkd, device, alloc, stencilCheckBufferInfo, MemoryRequirement::HostVisible));
    }
};

ManyDrawsCase::ManyDrawsCase(tcu::TestContext &testCtx, const std::string &name, const ManyDrawsParams &params)
    : TestCase(testCtx, name)
    , m_params(params)
{
}

void ManyDrawsCase::checkSupport(Context &context) const
{
    const auto &vki     = context.getInstanceInterface();
    const auto physDev  = context.getPhysicalDevice();
    const auto &vkd     = context.getDeviceInterface();
    const auto device   = context.getDevice();
    auto &alloc         = context.getDefaultAllocator();
    const auto dsFormat = getSupportedDepthStencilFormat(vki, physDev);

    try
    {
        ManyDrawsAllocatedData allocatedData(vkd, device, alloc, m_params.imageExtent, getColorFormat(), dsFormat);
    }
    catch (const vk::Error &err)
    {
        const auto result = err.getError();
        if (result == VK_ERROR_OUT_OF_HOST_MEMORY || result == VK_ERROR_OUT_OF_DEVICE_MEMORY)
            TCU_THROW(NotSupportedError, "Not enough memory to run this test");
        throw;
    }
}

void ManyDrawsCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::ostringstream vert;
    vert << "#version 450\n"
         << "\n"
         << "layout(location=0) in vec2 inCoords;\n"
         << "layout(location=1) in uvec4 inColor;\n"
         << "\n"
         << "layout(location=0) out flat uvec4 outColor;\n"
         << "\n"
         << "void main()\n"
         << "{\n"
         << "    gl_Position = vec4(inCoords, 0.0, 1.0);\n"
         << "    outColor = inColor;\n"
         << "}\n";

    std::ostringstream frag;
    frag << "#version 450\n"
         << "\n"
         << "layout(location=0) in flat uvec4 inColor;\n"
         << "layout(location=0) out uvec4 outColor;\n"
         << "\n"
         << "void main()\n"
         << "{\n"
         << "    outColor = inColor;\n"
         << "}\n";

    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

TestInstance *ManyDrawsCase::createInstance(Context &context) const
{
    return new ManyDrawsInstance(context, m_params);
}

ManyDrawsInstance::ManyDrawsInstance(Context &context, const ManyDrawsParams &params)
    : TestInstance(context)
    , m_params(params)
{
}

void copyAndFlush(const DeviceInterface &vkd, VkDevice device, BufferWithMemory &buffer,
                  const std::vector<ManyDrawsVertex> &vertices)
{
    auto &alloc   = buffer.getAllocation();
    void *hostPtr = alloc.getHostPtr();

    deMemcpy(hostPtr, vertices.data(), de::dataSize(vertices));
    flushAlloc(vkd, device, alloc);
}

tcu::TestStatus ManyDrawsInstance::iterate(void)
{
    const auto &vki    = m_context.getInstanceInterface();
    const auto physDev = m_context.getPhysicalDevice();
    const auto &vkd    = m_context.getDeviceInterface();
    const auto device  = m_context.getDevice();
    auto &alloc        = m_context.getDefaultAllocator();
    const auto qIndex  = m_context.getUniversalQueueFamilyIndex();
    const auto queue   = m_context.getUniversalQueue();

    const auto colorFormat        = ManyDrawsCase::getColorFormat();
    const auto dsFormat           = getSupportedDepthStencilFormat(vki, physDev);
    const auto vertexBufferSize   = ManyDrawsAllocatedData::calcVertexBufferSize(m_params.imageExtent);
    const auto vertexBufferOffset = static_cast<VkDeviceSize>(0);
    const auto numPixels          = ManyDrawsAllocatedData::calcNumPixels(m_params.imageExtent);
    const auto numVertices        = ManyDrawsAllocatedData::calcNumVertices(m_params.imageExtent);
    const auto alphaValue         = std::numeric_limits<uint8_t>::max();
    const auto pixelWidth         = 2.0f / static_cast<float>(m_params.imageExtent.width);  // Normalized size.
    const auto pixelWidthHalf     = pixelWidth / 2.0f;                                      // Normalized size.
    const auto pixelHeight        = 2.0f / static_cast<float>(m_params.imageExtent.height); // Normalized size.
    const auto useSecondary       = (m_params.level == VK_COMMAND_BUFFER_LEVEL_SECONDARY);

    // Allocate all needed data up front.
    ManyDrawsAllocatedData testData(vkd, device, alloc, m_params.imageExtent, colorFormat, dsFormat);

    // Generate random colors.
    de::Random rnd(m_params.seed);
    std::vector<ManyDrawsVertex::Color> colors;

    colors.reserve(numPixels);
    for (uint32_t i = 0; i < numPixels; ++i)
    {
#if 0
        const uint8_t red = ((i      ) & 0xFFu);
        const uint8_t green = ((i >>  8) & 0xFFu);
        const uint8_t blue = ((i >> 16) & 0xFFu);
        colors.push_back(ManyDrawsVertex::Color(red, green, blue, alphaValue));
#else
        colors.push_back(ManyDrawsVertex::Color(rnd.getUint8(), rnd.getUint8(), rnd.getUint8(), alphaValue));
#endif
    }

    // Fill vertex data. One triangle per pixel, front and back.
    std::vector<ManyDrawsVertex> frontVector;
    std::vector<ManyDrawsVertex> backVector;
    frontVector.reserve(numVertices);
    backVector.reserve(numVertices);

    for (uint32_t y = 0; y < m_params.imageExtent.height; ++y)
        for (uint32_t x = 0; x < m_params.imageExtent.width; ++x)
        {
            float x_left   = static_cast<float>(x) * pixelWidth - 1.0f;
            float x_mid    = x_left + pixelWidthHalf;
            float x_right  = x_left + pixelWidth;
            float y_top    = static_cast<float>(y) * pixelHeight - 1.0f;
            float y_bottom = y_top + pixelHeight;

            // Triangles in the "back" mesh will have different colors.
            const auto colorIdx    = y * m_params.imageExtent.width + x;
            const auto &frontColor = colors[colorIdx];
            const auto &backColor  = colors[colors.size() - 1u - colorIdx];

            const tcu::Vec2 triangle[3u] = {
                tcu::Vec2(x_left, y_top),
                tcu::Vec2(x_right, y_top),
                tcu::Vec2(x_mid, y_bottom),
            };

            frontVector.emplace_back(triangle[0], frontColor);
            frontVector.emplace_back(triangle[1], frontColor);
            frontVector.emplace_back(triangle[2], frontColor);

            backVector.emplace_back(triangle[0], backColor);
            backVector.emplace_back(triangle[1], backColor);
            backVector.emplace_back(triangle[2], backColor);
        }

    // Copy vertex data to staging buffers.
    copyAndFlush(vkd, device, *testData.frontBuffers.stagingBuffer, frontVector);
    copyAndFlush(vkd, device, *testData.backBuffers.stagingBuffer, backVector);

    // Color attachment view.
    const auto colorResourceRange  = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const auto colorAttachmentView = makeImageView(vkd, device, testData.colorAttachment->get(), VK_IMAGE_VIEW_TYPE_2D,
                                                   colorFormat, colorResourceRange);

    // Depth/stencil attachment view.
    const auto dsResourceRange =
        makeImageSubresourceRange((VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT), 0u, 1u, 0u, 1u);
    const auto dsAttachmentView =
        makeImageView(vkd, device, testData.dsAttachment->get(), VK_IMAGE_VIEW_TYPE_2D, dsFormat, dsResourceRange);

    const VkImageView attachmentArray[] = {colorAttachmentView.get(), dsAttachmentView.get()};
    const auto numAttachments           = static_cast<uint32_t>(DE_LENGTH_OF_ARRAY(attachmentArray));

    const auto renderPass  = makeRenderPass(vkd, device, colorFormat, dsFormat);
    const auto framebuffer = makeFramebuffer(vkd, device, renderPass.get(), numAttachments, attachmentArray,
                                             m_params.imageExtent.width, m_params.imageExtent.height);

    const auto vertModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("vert"), 0u);
    const auto fragModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("frag"), 0u);

    const std::vector<VkViewport> viewports(1u, makeViewport(m_params.imageExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(m_params.imageExtent));

    const auto descriptorSetLayout = DescriptorSetLayoutBuilder().build(vkd, device);
    const auto pipelineLayout      = makePipelineLayout(vkd, device, descriptorSetLayout.get());

    const VkVertexInputBindingDescription bindings[] = {
        makeVertexInputBindingDescription(0u, static_cast<uint32_t>(sizeof(ManyDrawsVertex)),
                                          VK_VERTEX_INPUT_RATE_VERTEX),
    };

    const VkVertexInputAttributeDescription attributes[] = {
        makeVertexInputAttributeDescription(0u, 0u, VK_FORMAT_R32G32_SFLOAT,
                                            static_cast<uint32_t>(offsetof(ManyDrawsVertex, coords))),
        makeVertexInputAttributeDescription(1u, 0u, VK_FORMAT_R8G8B8A8_UINT,
                                            static_cast<uint32_t>(offsetof(ManyDrawsVertex, color))),
    };

    const VkPipelineVertexInputStateCreateInfo inputState = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                   // const void* pNext;
        0u,                                                        // VkPipelineVertexInputStateCreateFlags flags;
        static_cast<uint32_t>(DE_LENGTH_OF_ARRAY(bindings)),       // uint32_t vertexBindingDescriptionCount;
        bindings, // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
        static_cast<uint32_t>(DE_LENGTH_OF_ARRAY(attributes)), // uint32_t vertexAttributeDescriptionCount;
        attributes, // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
    };

    // Stencil state: this is key for checking and obtaining the right results. The stencil buffer will be cleared to 0. The first
    // set of draws ("front" set of triangles) will pass the test and increment the stencil value to 1. The second set of draws
    // ("back" set of triangles, not really in the back because all of them have depth 0.0) will not pass the stencil test then, but
    // still increment the stencil value to 2.
    //
    // At the end of the test, if every draw command was executed correctly in the expected order, the color buffer will have the
    // colors of the front set, and the stencil buffer will be full of 2s.
    const auto stencilOpState = makeStencilOpState(VK_STENCIL_OP_INCREMENT_AND_CLAMP, VK_STENCIL_OP_INCREMENT_AND_CLAMP,
                                                   VK_STENCIL_OP_KEEP, VK_COMPARE_OP_EQUAL, 0xFFu, 0xFFu, 0u);

    const VkPipelineDepthStencilStateCreateInfo dsState = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, // VkStructureType                          sType
        nullptr,                                                    // const void*                              pNext
        0u,                                                         // VkPipelineDepthStencilStateCreateFlags   flags
        VK_FALSE,            // VkBool32                                 depthTestEnable
        VK_FALSE,            // VkBool32                                 depthWriteEnable
        VK_COMPARE_OP_NEVER, // VkCompareOp                              depthCompareOp
        VK_FALSE,            // VkBool32                                 depthBoundsTestEnable
        VK_TRUE,             // VkBool32                                 stencilTestEnable
        stencilOpState,      // VkStencilOpState                         front
        stencilOpState,      // VkStencilOpState                         back
        0.0f,                // float                                    minDepthBounds
        1.0f,                // float                                    maxDepthBounds
    };

    const auto pipeline =
        makeGraphicsPipeline(vkd, device, pipelineLayout.get(), vertModule.get(), VK_NULL_HANDLE, VK_NULL_HANDLE,
                             VK_NULL_HANDLE, fragModule.get(), renderPass.get(), viewports, scissors,
                             VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0u, 0u, &inputState, nullptr, nullptr, &dsState);

    // Command pool and buffers.
    using CmdBufferPtr    = Move<VkCommandBuffer>;
    const auto cmdPool    = makeCommandPool(vkd, device, qIndex);
    const auto secCmdPool = makeCommandPool(vkd, device, qIndex);

    CmdBufferPtr primaryCmdBufferPtr;
    CmdBufferPtr secondaryCmdBufferPtr;
    VkCommandBuffer primaryCmdBuffer;
    VkCommandBuffer secondaryCmdBuffer;
    VkCommandBuffer drawsCmdBuffer;

    primaryCmdBufferPtr = allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    primaryCmdBuffer    = primaryCmdBufferPtr.get();
    drawsCmdBuffer      = primaryCmdBuffer;
    beginCommandBuffer(vkd, primaryCmdBuffer);

    // Clear values.
    std::vector<VkClearValue> clearValues(2u);
    clearValues[0] = makeClearValueColorU32(0u, 0u, 0u, 0u);
    clearValues[1] = makeClearValueDepthStencil(1.0f, 0u);

    // Copy staging buffers to vertex buffers.
    const auto copyRegion = makeBufferCopy(0ull, 0ull, vertexBufferSize);
    vkd.cmdCopyBuffer(primaryCmdBuffer, testData.frontBuffers.stagingBuffer->get(),
                      testData.frontBuffers.vertexBuffer->get(), 1u, &copyRegion);
    vkd.cmdCopyBuffer(primaryCmdBuffer, testData.backBuffers.stagingBuffer->get(),
                      testData.backBuffers.vertexBuffer->get(), 1u, &copyRegion);

    // Use barrier for vertex reads.
    const auto vertexBarier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT);
    vkd.cmdPipelineBarrier(primaryCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0u, 1u,
                           &vertexBarier, 0u, nullptr, 0u, nullptr);

    // Change depth/stencil attachment layout.
    const auto dsBarrier = makeImageMemoryBarrier(
        0, (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT),
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, testData.dsAttachment->get(),
        dsResourceRange);
    vkd.cmdPipelineBarrier(primaryCmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           (VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT), 0u,
                           0u, nullptr, 0u, nullptr, 1u, &dsBarrier);

    beginRenderPass(vkd, primaryCmdBuffer, renderPass.get(), framebuffer.get(), scissors[0],
                    static_cast<uint32_t>(clearValues.size()), clearValues.data(),
                    (useSecondary ? VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS : VK_SUBPASS_CONTENTS_INLINE));

    if (useSecondary)
    {
        secondaryCmdBufferPtr = allocateCommandBuffer(vkd, device, secCmdPool.get(), VK_COMMAND_BUFFER_LEVEL_SECONDARY);
        secondaryCmdBuffer    = secondaryCmdBufferPtr.get();
        drawsCmdBuffer        = secondaryCmdBuffer;

        const VkCommandBufferInheritanceInfo inheritanceInfo = {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, // VkStructureType sType;
            nullptr,                                           // const void* pNext;
            renderPass.get(),                                  // VkRenderPass renderPass;
            0u,                                                // uint32_t subpass;
            framebuffer.get(),                                 // VkFramebuffer framebuffer;
            0u,                                                // VkBool32 occlusionQueryEnable;
            0u,                                                // VkQueryControlFlags queryFlags;
            0u,                                                // VkQueryPipelineStatisticFlags pipelineStatistics;
        };

        const VkCommandBufferUsageFlags usageFlags =
            (VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT | VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        const VkCommandBufferBeginInfo beginInfo = {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr,
            usageFlags,       // VkCommandBufferUsageFlags flags;
            &inheritanceInfo, // const VkCommandBufferInheritanceInfo* pInheritanceInfo;
        };

        VK_CHECK(vkd.beginCommandBuffer(secondaryCmdBuffer, &beginInfo));
    }

    // Bind pipeline.
    vkd.cmdBindPipeline(drawsCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());

    // Draw triangles in front.
    vkd.cmdBindVertexBuffers(drawsCmdBuffer, 0u, 1u, &testData.frontBuffers.vertexBuffer->get(), &vertexBufferOffset);
    for (uint32_t i = 0; i < numPixels; ++i)
        vkd.cmdDraw(drawsCmdBuffer, 3u, 1u, i * 3u, 0u);

    // Draw triangles in the "back". This should have no effect due to the stencil test.
    vkd.cmdBindVertexBuffers(drawsCmdBuffer, 0u, 1u, &testData.backBuffers.vertexBuffer->get(), &vertexBufferOffset);
    for (uint32_t i = 0; i < numPixels; ++i)
        vkd.cmdDraw(drawsCmdBuffer, 3u, 1u, i * 3u, 0u);

    if (useSecondary)
    {
        endCommandBuffer(vkd, secondaryCmdBuffer);
        vkd.cmdExecuteCommands(primaryCmdBuffer, 1u, &secondaryCmdBuffer);
    }

    endRenderPass(vkd, primaryCmdBuffer);

    // Copy color and depth/stencil attachments to verification buffers.
    const auto colorAttachmentBarrier = makeImageMemoryBarrier(
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, testData.colorAttachment->get(), colorResourceRange);
    vkd.cmdPipelineBarrier(primaryCmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &colorAttachmentBarrier);

    const auto colorResourceLayers = makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
    const auto colorCopyRegion     = makeBufferImageCopy(m_params.imageExtent, colorResourceLayers);
    vkd.cmdCopyImageToBuffer(primaryCmdBuffer, testData.colorAttachment->get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             testData.colorCheckBuffer->get(), 1u, &colorCopyRegion);

    const auto stencilAttachmentBarrier =
        makeImageMemoryBarrier(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                               VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               testData.dsAttachment->get(), dsResourceRange);
    vkd.cmdPipelineBarrier(primaryCmdBuffer,
                           (VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT),
                           VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &stencilAttachmentBarrier);

    const auto stencilResourceLayers = makeImageSubresourceLayers(VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, 1u);
    const auto stencilCopyRegion     = makeBufferImageCopy(m_params.imageExtent, stencilResourceLayers);
    vkd.cmdCopyImageToBuffer(primaryCmdBuffer, testData.dsAttachment->get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             testData.stencilCheckBuffer->get(), 1u, &stencilCopyRegion);

    const auto verificationBuffersBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    vkd.cmdPipelineBarrier(primaryCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u,
                           &verificationBuffersBarrier, 0u, nullptr, 0u, nullptr);

    endCommandBuffer(vkd, primaryCmdBuffer);
    submitCommandsAndWait(vkd, device, queue, primaryCmdBuffer);

    // Check buffer contents.
    auto &colorCheckBufferAlloc = testData.colorCheckBuffer->getAllocation();
    void *colorCheckBufferData  = colorCheckBufferAlloc.getHostPtr();
    invalidateAlloc(vkd, device, colorCheckBufferAlloc);

    auto &stencilCheckBufferAlloc = testData.stencilCheckBuffer->getAllocation();
    void *stencilCheckBufferData  = stencilCheckBufferAlloc.getHostPtr();
    invalidateAlloc(vkd, device, stencilCheckBufferAlloc);

    const auto iWidth           = static_cast<int>(m_params.imageExtent.width);
    const auto iHeight          = static_cast<int>(m_params.imageExtent.height);
    const auto colorTcuFormat   = mapVkFormat(colorFormat);
    const auto stencilTcuFormat = tcu::TextureFormat(tcu::TextureFormat::S, tcu::TextureFormat::UNSIGNED_INT8);

    tcu::TextureLevel referenceLevel(colorTcuFormat, iWidth, iHeight);
    tcu::PixelBufferAccess referenceAccess = referenceLevel.getAccess();
    tcu::TextureLevel colorErrorLevel(mapVkFormat(VK_FORMAT_R8G8B8A8_UNORM), iWidth, iHeight);
    tcu::PixelBufferAccess colorErrorAccess = colorErrorLevel.getAccess();
    tcu::TextureLevel stencilErrorLevel(mapVkFormat(VK_FORMAT_R8G8B8A8_UNORM), iWidth, iHeight);
    tcu::PixelBufferAccess stencilErrorAccess = stencilErrorLevel.getAccess();
    tcu::ConstPixelBufferAccess colorAccess(colorTcuFormat, iWidth, iHeight, 1, colorCheckBufferData);
    tcu::ConstPixelBufferAccess stencilAccess(stencilTcuFormat, iWidth, iHeight, 1, stencilCheckBufferData);
    const tcu::Vec4 green(0.0f, 1.0f, 0.0f, 1.0f);
    const tcu::Vec4 red(1.0f, 0.0f, 0.0f, 1.0f);
    const int expectedStencil = 2;
    bool colorFail            = false;
    bool stencilFail          = false;

    for (int y = 0; y < iHeight; ++y)
        for (int x = 0; x < iWidth; ++x)
        {
            const tcu::UVec4 colorValue = colorAccess.getPixelUint(x, y);
            const auto expectedPixel    = colors[y * iWidth + x];
            const tcu::UVec4 expectedValue(expectedPixel.x(), expectedPixel.y(), expectedPixel.z(), expectedPixel.w());
            const bool colorMismatch = (colorValue != expectedValue);

            const auto stencilValue    = stencilAccess.getPixStencil(x, y);
            const bool stencilMismatch = (stencilValue != expectedStencil);

            referenceAccess.setPixel(expectedValue, x, y);
            colorErrorAccess.setPixel((colorMismatch ? red : green), x, y);
            stencilErrorAccess.setPixel((stencilMismatch ? red : green), x, y);

            if (stencilMismatch)
                stencilFail = true;

            if (colorMismatch)
                colorFail = true;
        }

    if (colorFail || stencilFail)
    {
        auto &log = m_context.getTestContext().getLog();
        log << tcu::TestLog::ImageSet("Result", "") << tcu::TestLog::Image("ColorOutput", "", colorAccess)
            << tcu::TestLog::Image("ColorReference", "", referenceAccess)
            << tcu::TestLog::Image("ColorError", "", colorErrorAccess)
            << tcu::TestLog::Image("StencilError", "", stencilErrorAccess) << tcu::TestLog::EndImageSet;
        TCU_FAIL("Mismatched output and reference color or stencil; please check test log --");
    }

    return tcu::TestStatus::pass("Pass");
}

void initManyIndirectDrawsPrograms(SourceCollections &dst)
{
    std::ostringstream vert;
    vert << "#version 460\n"
         << "layout (location=0) in vec4 inPos;\n"
         << "void main (void) {\n"
         << "    gl_PointSize = 1.0;\n"
         << "    gl_Position = inPos;\n"
         << "}\n";
    dst.glslSources.add("vert") << glu::VertexSource(vert.str());

    std::ostringstream frag;
    frag << "#version 460\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "void main (void) {\n"
         << "    outColor = vec4(0.0, 0.0, 1.0, 1.0);\n"
         << "}\n";
    dst.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

tcu::TestStatus manyIndirectDrawsTest(Context &context)
{
    const auto &ctx = context.getContextCommonData();
    const tcu::IVec3 fbExtent(64, 64, 1);
    const auto vkExtent   = makeExtent3D(fbExtent);
    const auto floatExt   = fbExtent.cast<float>();
    const auto pixelCount = vkExtent.width * vkExtent.height;
    const auto fbFormat   = VK_FORMAT_R8G8B8A8_UNORM;
    const auto tcuFormat  = mapVkFormat(fbFormat);
    const auto fbUsage    = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    const tcu::Vec4 geomColor(0.0f, 0.0f, 1.0f, 1.0f); // Must match fragment shader.
    const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f); // When using 0 and 1 only, we expect exact results.
    const auto bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    // Color buffer with verification buffer.
    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, vkExtent, fbFormat, fbUsage, VK_IMAGE_TYPE_2D);

    // Vertices for each point.
    std::vector<tcu::Vec4> vertices;
    vertices.reserve(pixelCount);
    for (int y = 0; y < fbExtent.y(); ++y)
        for (int x = 0; x < fbExtent.x(); ++x)
        {
            const auto xCoord = ((static_cast<float>(x) + 0.5f) / floatExt.x()) * 2.0f - 1.0f;
            const auto yCoord = ((static_cast<float>(y) + 0.5f) / floatExt.y()) * 2.0f - 1.0f;
            vertices.push_back(tcu::Vec4(xCoord, yCoord, 0.0f, 1.0f));
        };

    // Vertex buffer
    const auto vbSize = static_cast<VkDeviceSize>(de::dataSize(vertices));
    const auto vbInfo = makeBufferCreateInfo(vbSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    BufferWithMemory vertexBuffer(ctx.vkd, ctx.device, ctx.allocator, vbInfo, MemoryRequirement::HostVisible);
    const auto vbAlloc  = vertexBuffer.getAllocation();
    void *vbData        = vbAlloc.getHostPtr();
    const auto vbOffset = static_cast<VkDeviceSize>(0);

    deMemcpy(vbData, de::dataOrNull(vertices), de::dataSize(vertices));
    flushAlloc(ctx.vkd, ctx.device, vbAlloc);

    std::vector<VkDrawIndirectCommand> indirectCommands;
    indirectCommands.reserve(pixelCount);
    const auto indirectCmdSize = static_cast<uint32_t>(sizeof(decltype(indirectCommands)::value_type));

    for (uint32_t i = 0u; i < pixelCount; ++i)
    {
        indirectCommands.push_back({
            1u, // uint32_t vertexCount;
            1u, // uint32_t instanceCount;
            i,  // uint32_t firstVertex;
            0u, // uint32_t firstInstance;
        });
    }

    // Indirect draw buffer.
    const auto ibSize = static_cast<VkDeviceSize>(de::dataSize(indirectCommands));
    const auto ibInfo = makeBufferCreateInfo(ibSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
    BufferWithMemory indirectBuffer(ctx.vkd, ctx.device, ctx.allocator, ibInfo, MemoryRequirement::HostVisible);
    const auto ibAlloc = indirectBuffer.getAllocation();
    void *ibData       = ibAlloc.getHostPtr();

    deMemcpy(ibData, de::dataOrNull(indirectCommands), de::dataSize(indirectCommands));
    flushAlloc(ctx.vkd, ctx.device, ibAlloc);

    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device);
    const auto renderPass     = makeRenderPass(ctx.vkd, ctx.device, fbFormat);
    const auto framebuffer =
        makeFramebuffer(ctx.vkd, ctx.device, *renderPass, colorBuffer.getImageView(), vkExtent.width, vkExtent.height);

    // Modules.
    const auto &binaries  = context.getBinaryCollection();
    const auto vertModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("vert"));
    const auto fragModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));

    const std::vector<VkViewport> viewports(1u, makeViewport(vkExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(vkExtent));

    // Other default values work for the current setup, including the vertex input data format.
    const auto pipeline = makeGraphicsPipeline(ctx.vkd, ctx.device, *pipelineLayout, *vertModule, VK_NULL_HANDLE,
                                               VK_NULL_HANDLE, VK_NULL_HANDLE, *fragModule, *renderPass, viewports,
                                               scissors, VK_PRIMITIVE_TOPOLOGY_POINT_LIST);

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;
    const auto secCmdBuffer =
        allocateCommandBuffer(ctx.vkd, ctx.device, *cmd.cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);

    beginSecondaryCommandBuffer(ctx.vkd, *secCmdBuffer, *renderPass, *framebuffer);
    ctx.vkd.cmdBindVertexBuffers(*secCmdBuffer, 0u, 1u, &vertexBuffer.get(), &vbOffset);
    ctx.vkd.cmdBindPipeline(*secCmdBuffer, bindPoint, *pipeline);
    for (uint32_t i = 0; i < pixelCount; ++i)
        ctx.vkd.cmdDrawIndirect(*secCmdBuffer, indirectBuffer.get(), static_cast<VkDeviceSize>(i * indirectCmdSize), 1u,
                                indirectCmdSize);
    endCommandBuffer(ctx.vkd, *secCmdBuffer);

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    beginRenderPass(ctx.vkd, cmdBuffer, *renderPass, *framebuffer, scissors.at(0u), clearColor,
                    VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
    ctx.vkd.cmdExecuteCommands(cmdBuffer, 1u, &(*secCmdBuffer));
    endRenderPass(ctx.vkd, cmdBuffer);
    copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), fbExtent.swizzle(0, 1),
                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1u,
                      VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_COLOR_BIT,
                      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
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

    return tcu::TestStatus::pass("Pass");
}

constexpr uint32_t kIndirectDispatchValueOffset = 1000000u;

void initManyIndirectDispatchesPrograms(SourceCollections &dst)
{
    std::ostringstream comp;
    comp << "#version 460\n"
         << "layout (local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
         << "layout (push_constant, std430) uniform PushConstantBlock { uint index; } pc;\n"
         << "layout (set=0, binding=0, std430) buffer OutputBlock { uint data[]; } outputValues;\n"
         << "void main (void) {\n"
         << "    outputValues.data[pc.index] += pc.index + " << kIndirectDispatchValueOffset << "u;\n"
         << "}\n";
    dst.glslSources.add("comp") << glu::ComputeSource(comp.str());
}

void checkManyIndirectDispatchesSupport(Context &context)
{
    // The device must have support for a compute queue.
    // getComputeQueue() will throw NotSupportedError if the device doesn't have one.
    context.getComputeQueue();
}

tcu::TestStatus manyIndirectDispatchesTest(Context &context)
{
    const auto &ctx       = context.getContextCommonData();
    const auto descType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    const auto bindPoint  = VK_PIPELINE_BIND_POINT_COMPUTE;
    const auto dataStages = VK_SHADER_STAGE_COMPUTE_BIT;
    const auto valueCount = 4096u;
    const auto qfIndex    = context.getComputeQueueFamilyIndex();
    const auto queue      = context.getComputeQueue();

    // Host-side buffer values.
    std::vector<uint32_t> bufferValues(valueCount, 0u);

    // Storage buffer.
    const auto sbSize = static_cast<VkDeviceSize>(de::dataSize(bufferValues));
    const auto sbInfo = makeBufferCreateInfo(sbSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    BufferWithMemory storageBuffer(ctx.vkd, ctx.device, ctx.allocator, sbInfo, MemoryRequirement::HostVisible);
    const auto sbAlloc  = storageBuffer.getAllocation();
    void *sbData        = sbAlloc.getHostPtr();
    const auto sbOffset = static_cast<VkDeviceSize>(0);

    deMemcpy(sbData, de::dataOrNull(bufferValues), de::dataSize(bufferValues));
    flushAlloc(ctx.vkd, ctx.device, sbAlloc);

    // Indirect dispatch buffer. We'll pretend to have 4096 indirect commands but all of them will launch 1 group with 1 invocation.
    const VkDispatchIndirectCommand defaultCommand{1u, 1u, 1u};
    const std::vector<VkDispatchIndirectCommand> indirectCommands(valueCount, defaultCommand);

    const auto ibSize = static_cast<VkDeviceSize>(de::dataSize(indirectCommands));
    const auto ibInfo = makeBufferCreateInfo(ibSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
    BufferWithMemory indirectBuffer(ctx.vkd, ctx.device, ctx.allocator, ibInfo, MemoryRequirement::HostVisible);
    const auto ibAlloc = indirectBuffer.getAllocation();
    void *ibData       = ibAlloc.getHostPtr();

    deMemcpy(ibData, de::dataOrNull(indirectCommands), de::dataSize(indirectCommands));
    flushAlloc(ctx.vkd, ctx.device, ibAlloc);

    // Descriptor pool, set, layout, etc.
    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(descType);
    const auto descriptorPool =
        poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

    DescriptorSetLayoutBuilder layoutBuilder;
    layoutBuilder.addSingleBinding(descType, dataStages);
    const auto setLayout     = layoutBuilder.build(ctx.vkd, ctx.device);
    const auto descriptorSet = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *setLayout);

    DescriptorSetUpdateBuilder updateBuilder;
    const auto dbDescInfo = makeDescriptorBufferInfo(storageBuffer.get(), sbOffset, sbSize);
    updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), descType, &dbDescInfo);
    updateBuilder.update(ctx.vkd, ctx.device);

    // Push constants.
    const auto pcSize  = static_cast<uint32_t>(sizeof(uint32_t));
    const auto pcRange = makePushConstantRange(dataStages, 0u, pcSize);

    // Layout and pipeline.
    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout, &pcRange);
    const auto &binaries      = context.getBinaryCollection();
    const auto compModule     = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp"));
    const auto pipeline       = makeComputePipeline(ctx.vkd, ctx.device, *pipelineLayout, *compModule);

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;
    const auto secCmdBufferPtr =
        allocateCommandBuffer(ctx.vkd, ctx.device, *cmd.cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
    const auto secCmdBuffer = *secCmdBufferPtr;

    beginSecondaryCommandBuffer(ctx.vkd, secCmdBuffer);
    ctx.vkd.cmdBindPipeline(secCmdBuffer, bindPoint, *pipeline);
    ctx.vkd.cmdBindDescriptorSets(secCmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
    for (uint32_t i = 0; i < valueCount; ++i)
    {
        ctx.vkd.cmdPushConstants(secCmdBuffer, *pipelineLayout, dataStages, 0u, pcSize, &i);
        const auto dispatchOffset = static_cast<VkDeviceSize>(i * sizeof(VkDispatchIndirectCommand));
        ctx.vkd.cmdDispatchIndirect(secCmdBuffer, indirectBuffer.get(), dispatchOffset);
    }
    endCommandBuffer(ctx.vkd, secCmdBuffer);

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    ctx.vkd.cmdExecuteCommands(cmdBuffer, 1u, &secCmdBuffer);
    {
        const auto compute2Host = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &compute2Host);
    }
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, queue, cmdBuffer);

    // Verify values.
    std::vector<uint32_t> outputValues(valueCount, 0u);
    invalidateAlloc(ctx.vkd, ctx.device, sbAlloc);
    deMemcpy(outputValues.data(), sbData, de::dataSize(outputValues));

    for (uint32_t i = 0u; i < valueCount; ++i)
    {
        const auto refValue = bufferValues[i] + i + kIndirectDispatchValueOffset;
        if (outputValues[i] != refValue)
        {
            std::ostringstream msg;
            msg << "Unexpected value found at position " << i << ": expected " << refValue << " but found "
                << outputValues[i];
            TCU_FAIL(msg.str());
        }
    }

    return tcu::TestStatus::pass("Pass");
}

struct IndirectDispatchAlignmentParams
{
    uint32_t memOffset;
    uint32_t dispatchOffset;
};

class IndirectDispatchAlignmentInstance : public vkt::TestInstance
{
public:
    IndirectDispatchAlignmentInstance(Context &context, const IndirectDispatchAlignmentParams &params)
        : vkt::TestInstance(context)
        , m_params(params)
    {
    }
    virtual ~IndirectDispatchAlignmentInstance(void)
    {
    }

    tcu::TestStatus iterate(void) override;

protected:
    const IndirectDispatchAlignmentParams m_params;
};

class IndirectDispatchAlignmentCase : public vkt::TestCase
{
public:
    IndirectDispatchAlignmentCase(tcu::TestContext &testCtx, const std::string &name,
                                  const IndirectDispatchAlignmentParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~IndirectDispatchAlignmentCase(void)
    {
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;
    void checkSupport(Context &context) const override;

protected:
    const IndirectDispatchAlignmentParams m_params;
};

void IndirectDispatchAlignmentCase::checkSupport(Context &context) const
{
    // Will throw NotSupportedError if not found.
    context.getComputeQueue();
}

void IndirectDispatchAlignmentCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::ostringstream comp;
    comp << "#version 460\n"
         << "layout (local_size_x=64, local_size_y=1, local_size_z=1) in;\n"
         << "layout (set=0, binding=0, std430) buffer OutputBlock { uint data[64]; } outputValues;\n"
         << "void main (void) {\n"
         << "    outputValues.data[gl_LocalInvocationIndex] += gl_LocalInvocationIndex + "
         << kIndirectDispatchValueOffset << "u;\n"
         << "}\n";
    programCollection.glslSources.add("comp") << glu::ComputeSource(comp.str());
}

TestInstance *IndirectDispatchAlignmentCase::createInstance(Context &context) const
{
    return new IndirectDispatchAlignmentInstance(context, m_params);
}

tcu::TestStatus IndirectDispatchAlignmentInstance::iterate(void)
{
    const auto &ctx       = m_context.getContextCommonData();
    const auto descType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    const auto bindPoint  = VK_PIPELINE_BIND_POINT_COMPUTE;
    const auto dataStages = VK_SHADER_STAGE_COMPUTE_BIT;
    const auto valueCount = 64u; // Must match compute shader.
    const auto qfIndex    = m_context.getComputeQueueFamilyIndex();
    const auto queue      = m_context.getComputeQueue();
    auto &log             = m_context.getTestContext().getLog();

    // Host-side buffer values.
    std::vector<uint32_t> bufferValues(valueCount, 0u);

    // Storage buffer.
    const auto sbSize = static_cast<VkDeviceSize>(de::dataSize(bufferValues));
    const auto sbInfo = makeBufferCreateInfo(sbSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    BufferWithMemory storageBuffer(ctx.vkd, ctx.device, ctx.allocator, sbInfo, MemoryRequirement::HostVisible);
    const auto sbAlloc  = storageBuffer.getAllocation();
    void *sbData        = sbAlloc.getHostPtr();
    const auto sbOffset = static_cast<VkDeviceSize>(0);

    deMemcpy(sbData, de::dataOrNull(bufferValues), de::dataSize(bufferValues));
    flushAlloc(ctx.vkd, ctx.device, sbAlloc);

    // Indirect dispatch buffer contents.
    const VkDispatchIndirectCommand defaultCommand{1u, 1u, 1u};
    const std::vector<VkDispatchIndirectCommand> indirectCommands(1u, defaultCommand);

    // Note the calculated indirect buffer size takes into account the dispatche offset.
    const auto ibSize         = static_cast<VkDeviceSize>(m_params.dispatchOffset + de::dataSize(indirectCommands));
    const auto ibInfo         = makeBufferCreateInfo(ibSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
    const auto indirectBuffer = makeBuffer(ctx.vkd, ctx.device, ibInfo);

    // Allocate more memory if needed and bind with an offset.
    const auto memReqs   = getBufferMemoryRequirements(ctx.vkd, ctx.device, *indirectBuffer);
    const auto memOffset = de::roundUp(static_cast<VkDeviceSize>(m_params.memOffset), memReqs.alignment);

    log << tcu::TestLog::Message << "Test parameters: memoryOffset=" << m_params.memOffset
        << " dispatchOffset=" << m_params.dispatchOffset << tcu::TestLog::EndMessage;
    log << tcu::TestLog::Message << "Buffer memory requirements: size=" << memReqs.size
        << " alignment=" << memReqs.alignment << tcu::TestLog::EndMessage;
    log << tcu::TestLog::Message << "Used memory offset: " << memOffset << tcu::TestLog::EndMessage;

    const VkMemoryRequirements allocationRequirements = {
        memOffset + memReqs.size, // VkDeviceSize size;
        memReqs.alignment,        // VkDeviceSize alignment;
        memReqs.memoryTypeBits,   // uint32_t memoryTypeBits;
    };
    const auto ibMemory = ctx.allocator.allocate(allocationRequirements, MemoryRequirement::HostVisible);
    ctx.vkd.bindBufferMemory(ctx.device, *indirectBuffer, ibMemory->getMemory(), memOffset);

    // Copy data to the buffer taking into account the dispatch offset.
    char *ibData = reinterpret_cast<char *>(ibMemory->getHostPtr()) + static_cast<size_t>(memOffset);
    deMemcpy(ibData + m_params.dispatchOffset, de::dataOrNull(indirectCommands), de::dataSize(indirectCommands));
    flushAlloc(ctx.vkd, ctx.device, *ibMemory);

    // Descriptor pool, set, layout, etc.
    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(descType);
    const auto descriptorPool =
        poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

    DescriptorSetLayoutBuilder layoutBuilder;
    layoutBuilder.addSingleBinding(descType, dataStages);
    const auto setLayout     = layoutBuilder.build(ctx.vkd, ctx.device);
    const auto descriptorSet = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *setLayout);

    DescriptorSetUpdateBuilder updateBuilder;
    const auto dbDescInfo = makeDescriptorBufferInfo(storageBuffer.get(), sbOffset, sbSize);
    updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), descType, &dbDescInfo);
    updateBuilder.update(ctx.vkd, ctx.device);

    // Layout and pipeline.
    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout);
    const auto &binaries      = m_context.getBinaryCollection();
    const auto compModule     = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp"));
    const auto pipeline       = makeComputePipeline(ctx.vkd, ctx.device, *pipelineLayout, *compModule);

    // To make it more interesting, we'll also use secondary command buffers.
    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;
    const auto secCmdBufferPtr =
        allocateCommandBuffer(ctx.vkd, ctx.device, *cmd.cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
    const auto secCmdBuffer = *secCmdBufferPtr;

    beginSecondaryCommandBuffer(ctx.vkd, secCmdBuffer);
    ctx.vkd.cmdBindPipeline(secCmdBuffer, bindPoint, *pipeline);
    ctx.vkd.cmdBindDescriptorSets(secCmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
    const auto dispatchOffset = m_params.dispatchOffset;
    ctx.vkd.cmdDispatchIndirect(secCmdBuffer, indirectBuffer.get(), dispatchOffset);
    endCommandBuffer(ctx.vkd, secCmdBuffer);

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    ctx.vkd.cmdExecuteCommands(cmdBuffer, 1u, &secCmdBuffer);
    {
        const auto compute2Host = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &compute2Host);
    }
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, queue, cmdBuffer);

    // Verify values.
    std::vector<uint32_t> outputValues(valueCount, 0u);
    invalidateAlloc(ctx.vkd, ctx.device, sbAlloc);
    deMemcpy(outputValues.data(), sbData, de::dataSize(outputValues));

    for (uint32_t i = 0u; i < valueCount; ++i)
    {
        const auto refValue = bufferValues[i] + i + kIndirectDispatchValueOffset;
        if (outputValues[i] != refValue)
        {
            std::ostringstream msg;
            msg << "Unexpected value found at position " << i << ": expected " << refValue << " but found "
                << outputValues[i];
            TCU_FAIL(msg.str());
        }
    }

    return tcu::TestStatus::pass("Pass");
}

} // namespace

tcu::TestCaseGroup *createCommandBuffersTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> commandBuffersTests(new tcu::TestCaseGroup(testCtx, "command_buffers"));

    /* 19.1. Command Pools (5.1 in VK 1.0 Spec) */
    addFunctionCase(commandBuffersTests.get(), "pool_create_null_params", createPoolNullParamsTest);
#ifndef CTS_USES_VULKANSC
    // VkAllocationCallbacks must be NULL in Vulkan SC
    addFunctionCase(commandBuffersTests.get(), "pool_create_non_null_allocator", createPoolNonNullAllocatorTest);
#endif // CTS_USES_VULKANSC
    addFunctionCase(commandBuffersTests.get(), "pool_create_transient_bit", createPoolTransientBitTest);
    addFunctionCase(commandBuffersTests.get(), "pool_create_reset_bit", createPoolResetBitTest);
#ifndef CTS_USES_VULKANSC
    addFunctionCase(commandBuffersTests.get(), "pool_reset_release_res", resetPoolReleaseResourcesBitTest);
#endif // CTS_USES_VULKANSC
    addFunctionCase(commandBuffersTests.get(), "pool_reset_no_flags_res", resetPoolNoFlagsTest);
#ifndef CTS_USES_VULKANSC
    addFunctionCase(commandBuffersTests.get(), "pool_reset_reuse", checkEventSupport, resetPoolReuseTest);
#endif // CTS_USES_VULKANSC
    /* 19.2. Command Buffer Lifetime (5.2 in VK 1.0 Spec) */
    addFunctionCase(commandBuffersTests.get(), "allocate_single_primary", allocatePrimaryBufferTest);
    addFunctionCase(commandBuffersTests.get(), "allocate_many_primary", allocateManyPrimaryBuffersTest);
    addFunctionCase(commandBuffersTests.get(), "allocate_single_secondary", allocateSecondaryBufferTest);
    addFunctionCase(commandBuffersTests.get(), "allocate_many_secondary", allocateManySecondaryBuffersTest);
    addFunctionCase(commandBuffersTests.get(), "execute_small_primary", checkEventSupport, executePrimaryBufferTest);
    addFunctionCase(commandBuffersTests.get(), "execute_large_primary", checkEventSupport,
                    executeLargePrimaryBufferTest);
    addFunctionCase(commandBuffersTests.get(), "reset_implicit", checkEventSupport, resetBufferImplicitlyTest);
#ifndef CTS_USES_VULKANSC
    addFunctionCase(commandBuffersTests.get(), "trim_command_pool", checkEventSupport, trimCommandPoolTest,
                    VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    addFunctionCase(commandBuffersTests.get(), "trim_command_pool_secondary", checkEventSupport, trimCommandPoolTest,
                    VK_COMMAND_BUFFER_LEVEL_SECONDARY);
#endif // CTS_USES_VULKANSC
    /* 19.3. Command Buffer Recording (5.3 in VK 1.0 Spec) */
    addFunctionCase(commandBuffersTests.get(), "record_single_primary", checkEventSupport,
                    recordSinglePrimaryBufferTest);
    addFunctionCase(commandBuffersTests.get(), "record_many_primary", checkEventSupport, recordLargePrimaryBufferTest);
    addFunctionCase(commandBuffersTests.get(), "record_single_secondary",
                    checkEventAndSecondaryCommandBufferNullFramebufferSupport, recordSingleSecondaryBufferTest);
    addFunctionCase(commandBuffersTests.get(), "record_many_secondary",
                    checkEventAndSecondaryCommandBufferNullFramebufferSupport, recordLargeSecondaryBufferTest);
    {
        uint32_t seed          = 1614182419u;
        const auto smallExtent = makeExtent3D(128u, 128u, 1u);

        commandBuffersTests->addChild(
            new ManyDrawsCase(testCtx, "record_many_draws_primary_1",
                              ManyDrawsParams(VK_COMMAND_BUFFER_LEVEL_PRIMARY, smallExtent, seed++)));
        commandBuffersTests->addChild(
            new ManyDrawsCase(testCtx, "record_many_draws_secondary_1",
                              ManyDrawsParams(VK_COMMAND_BUFFER_LEVEL_SECONDARY, smallExtent, seed++)));
#ifndef CTS_USES_VULKANSC
        const auto largeExtent = makeExtent3D(512u, 256u, 1u);
        commandBuffersTests->addChild(
            new ManyDrawsCase(testCtx, "record_many_draws_primary_2",
                              ManyDrawsParams(VK_COMMAND_BUFFER_LEVEL_PRIMARY, largeExtent, seed++)));
        commandBuffersTests->addChild(
            new ManyDrawsCase(testCtx, "record_many_draws_secondary_2",
                              ManyDrawsParams(VK_COMMAND_BUFFER_LEVEL_SECONDARY, largeExtent, seed++)));
#endif // CTS_USES_VULKANSC
    }
    addFunctionCase(commandBuffersTests.get(), "submit_twice_primary", checkEventSupport, submitPrimaryBufferTwiceTest);
    addFunctionCase(commandBuffersTests.get(), "submit_twice_secondary",
                    checkEventAndSecondaryCommandBufferNullFramebufferSupport, submitSecondaryBufferTwiceTest);
    addFunctionCase(commandBuffersTests.get(), "record_one_time_submit_primary", checkEventSupport,
                    oneTimeSubmitFlagPrimaryBufferTest);
    addFunctionCase(commandBuffersTests.get(), "record_one_time_submit_secondary",
                    checkEventAndSecondaryCommandBufferNullFramebufferSupport, oneTimeSubmitFlagSecondaryBufferTest);
    addFunctionCase(commandBuffersTests.get(), "render_pass_continue", renderPassContinueTest, true);
    addFunctionCase(commandBuffersTests.get(), "nested_render_pass_continue",
                    checkNestedCommandBufferRenderPassContinueSupport, renderPassContinueNestedTest, true);
    addFunctionCase(commandBuffersTests.get(), "render_pass_continue_no_fb",
                    checkSecondaryCommandBufferNullOrImagelessFramebufferSupport1, renderPassContinueTest, false);
    addFunctionCaseWithPrograms(commandBuffersTests.get(), "record_simul_use_secondary_one_primary",
                                checkSimultaneousUseAndSecondaryCommandBufferNullFramebufferSupport,
                                genComputeIncrementSource, simultaneousUseSecondaryBufferOnePrimaryBufferTest);
    addFunctionCaseWithPrograms(commandBuffersTests.get(), "record_simul_use_secondary_two_primary",
                                checkSimultaneousUseAndSecondaryCommandBufferNullFramebufferSupport,
                                genComputeIncrementSource, simultaneousUseSecondaryBufferTwoPrimaryBuffersTest);
    addFunctionCaseWithPrograms(commandBuffersTests.get(), "record_simul_use_nested",
                                checkSimultaneousUseAndNestedCommandBufferNullFramebufferSupport,
                                genComputeIncrementSource, simultaneousUseNestedSecondaryBufferTest);
    addFunctionCaseWithPrograms(commandBuffersTests.get(), "record_simul_use_twice_nested",
                                checkSimultaneousUseAndNestedCommandBufferNullFramebufferSupport,
                                genComputeIncrementSource, simultaneousUseNestedSecondaryBufferTwiceTest);
    addFunctionCase(commandBuffersTests.get(), "record_query_precise_w_flag",
                    checkSecondaryCommandBufferNullOrImagelessFramebufferSupport, recordBufferQueryPreciseWithFlagTest);
    addFunctionCase(commandBuffersTests.get(), "record_query_imprecise_w_flag",
                    checkSecondaryCommandBufferNullOrImagelessFramebufferSupport,
                    recordBufferQueryImpreciseWithFlagTest);
    addFunctionCase(commandBuffersTests.get(), "record_query_imprecise_wo_flag",
                    checkSecondaryCommandBufferNullOrImagelessFramebufferSupport,
                    recordBufferQueryImpreciseWithoutFlagTest);
    addFunctionCaseWithPrograms(commandBuffersTests.get(), "bad_inheritance_info_random",
                                genComputeIncrementSourceBadInheritance, badInheritanceInfoTest,
                                BadInheritanceInfoCase::RANDOM_PTR);
    addFunctionCaseWithPrograms(commandBuffersTests.get(), "bad_inheritance_info_random_cont",
                                genComputeIncrementSourceBadInheritance, badInheritanceInfoTest,
                                BadInheritanceInfoCase::RANDOM_PTR_CONTINUATION);
    addFunctionCaseWithPrograms(commandBuffersTests.get(), "bad_inheritance_info_random_data",
                                genComputeIncrementSourceBadInheritance, badInheritanceInfoTest,
                                BadInheritanceInfoCase::RANDOM_DATA_PTR);
    addFunctionCaseWithPrograms(commandBuffersTests.get(), "bad_inheritance_info_invalid_type",
                                genComputeIncrementSourceBadInheritance, badInheritanceInfoTest,
                                BadInheritanceInfoCase::INVALID_STRUCTURE_TYPE);
    addFunctionCaseWithPrograms(commandBuffersTests.get(), "bad_inheritance_info_valid_nonsense_type",
                                genComputeIncrementSourceBadInheritance, badInheritanceInfoTest,
                                BadInheritanceInfoCase::VALID_NONSENSE_TYPE);
    /* 19.4. Command Buffer Submission (5.4 in VK 1.0 Spec) */
    addFunctionCase(commandBuffersTests.get(), "submit_count_non_zero", checkEventSupport, submitBufferCountNonZero);
    addFunctionCase(commandBuffersTests.get(), "submit_count_equal_zero", checkEventSupport,
                    submitBufferCountEqualZero);
    addFunctionCase(commandBuffersTests.get(), "submit_wait_single_semaphore", checkEventSupport,
                    submitBufferWaitSingleSemaphore);
    addFunctionCase(commandBuffersTests.get(), "submit_wait_many_semaphores", checkEventSupport,
                    submitBufferWaitManySemaphores);
    addFunctionCase(commandBuffersTests.get(), "submit_null_fence", checkEventSupport, submitBufferNullFence);
    addFunctionCase(commandBuffersTests.get(), "submit_two_buffers_one_buffer_null_with_fence", checkEventSupport,
                    submitTwoBuffersOneBufferNullWithFence);
    /* 19.5. Secondary Command Buffer Execution (5.6 in VK 1.0 Spec) */
    addFunctionCase(commandBuffersTests.get(), "secondary_execute",
                    checkEventAndSecondaryCommandBufferNullFramebufferSupport, executeSecondaryBufferTest);
    addFunctionCase(commandBuffersTests.get(), "secondary_execute_twice",
                    checkEventAndTimelineSemaphoreAndSimultaneousUseAndSecondaryCommandBufferNullFramebufferSupport,
                    executeSecondaryBufferTwiceTest);
    /* 19.6. Commands Allowed Inside Command Buffers (? in VK 1.0 Spec) */
    addFunctionCaseWithPrograms(commandBuffersTests.get(), "order_bind_pipeline", genComputeSource,
                                orderBindPipelineTest);
    /* Verify untested transitions between command buffer states */
    addFunctionCase(commandBuffersTests.get(), "recording_to_ininitial", executeStateTransitionTest,
                    STT_RECORDING_TO_INITIAL);
    addFunctionCase(commandBuffersTests.get(), "executable_to_ininitial", executeStateTransitionTest,
                    STT_EXECUTABLE_TO_INITIAL);
    addFunctionCase(commandBuffersTests.get(), "recording_to_invalid", executeStateTransitionTest,
                    STT_RECORDING_TO_INVALID);
    addFunctionCase(commandBuffersTests.get(), "executable_to_invalid", executeStateTransitionTest,
                    STT_EXECUTABLE_TO_INVALID);
    addFunctionCaseWithPrograms(commandBuffersTests.get(), "many_indirect_draws_on_secondary",
                                initManyIndirectDrawsPrograms, manyIndirectDrawsTest);
    addFunctionCaseWithPrograms(commandBuffersTests.get(), "many_indirect_disps_on_secondary",
                                checkManyIndirectDispatchesSupport, initManyIndirectDispatchesPrograms,
                                manyIndirectDispatchesTest);

    addFunctionCase(commandBuffersTests.get(), "nested_execute", checkNestedCommandBufferSupport,
                    executeNestedBufferTest);
    addFunctionCase(commandBuffersTests.get(), "nested_execute_multiple_levels", checkNestedCommandBufferDepthSupport,
                    executeMultipleLevelsNestedBufferTest);

    // Test indirect dispatches with different offsets.
    {
        auto testGroup = commandBuffersTests.get();

        const std::vector<uint32_t> offsetsToTest{0u, 4u, 8u, 12u, 16u, 20u, 24u, 28u};

        for (const auto memOffset : offsetsToTest)
            for (const auto dispatchOffset : offsetsToTest)
            {
                IndirectDispatchAlignmentParams params{memOffset, dispatchOffset};
                const std::string testName = "indirect_compute_dispatch_offsets_" + std::to_string(memOffset) + "_" +
                                             std::to_string(dispatchOffset);
                testGroup->addChild(new IndirectDispatchAlignmentCase(testCtx, testName, params));
            }
    }

    return commandBuffersTests.release();
}

} // namespace api
} // namespace vkt
