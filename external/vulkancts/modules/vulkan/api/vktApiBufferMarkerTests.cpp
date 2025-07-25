/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 Advanced Micro Devices, Inc.
 * Copyright (c) 2019 The Khronos Group Inc.
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
 * \brief Tests for VK_AMD_buffer_marker
 *//*--------------------------------------------------------------------*/

#include "vktApiBufferMarkerTests.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktExternalMemoryUtil.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vkPlatform.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "tcuCommandLine.hpp"
#include "deUniquePtr.hpp"
#include "deRandom.hpp"
#include "deSTLUtil.hpp"

#include <vector>

namespace vkt
{
namespace api
{
namespace
{
using namespace vk;
using de::MovePtr;
using de::UniquePtr;
using namespace vkt::ExternalMemoryUtil;

std::string genBufferMarkerDeviceId(VkQueueFlagBits testQueue, size_t offset)
{
    struct WorkingDevice
    {
    };
    return std::string(std::type_index(typeid(WorkingDevice)).name()) + "-" + std::to_string(testQueue) + "-" +
           std::to_string(offset);
}

// The goal is to find a queue family that most accurately represents the required queue flag.
// For example, if flag is VK_QUEUE_TRANSFER_BIT, we want to target transfer-only queues for
// such a test case rather than universal queues which may include VK_QUEUE_TRANSFER_BIT along
// with other queue flags.
DevCaps::QueueCreateInfo makeQueueCreateInfo(const VkQueueFlagBits testQueue)
{
    VkQueueFlags forbiddenFlags{};

    switch (testQueue)
    {
    case VK_QUEUE_TRANSFER_BIT:
        // for VK_QUEUE_TRANSFER_BIT, target transfer-only queues:
        forbiddenFlags = (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);
        break;
    case VK_QUEUE_COMPUTE_BIT:
        // for VK_QUEUE_COMPUTE_BIT, target compute only queues
        forbiddenFlags = (VK_QUEUE_GRAPHICS_BIT);
        break;
    case VK_QUEUE_GRAPHICS_BIT:
        // for VK_QUEUE_GRAPHICS_BIT, target universal queues (queues which support graphics)
        forbiddenFlags = VkQueueFlags(0);
        break;
    default:
        forbiddenFlags = (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT);
    }

    return {VkQueueFlags(testQueue), forbiddenFlags, 1u, 1.0f};
}

struct BaseTestParams
{
    VkQueueFlagBits testQueue;     // Queue type that this test case targets
    VkPipelineStageFlagBits stage; // Pipeline stage where any marker writes for this test case occur in
    uint32_t size;                 // Number of buffer markers
    bool useHostPtr;               // Whether to use host pointer as backing buffer memory
    size_t offset;                 // The offset of the data in the buffer
};

typedef InstanceFactory1WithSupport<                  //
    FunctionInstance1<BaseTestParams>,                //
    typename FunctionInstance1<BaseTestParams>::Args, //
    FunctionSupport1<BaseTestParams>>
    ApiBufferMarkerBaseTestCase;

struct BufferMarkerBaseCase : public ApiBufferMarkerBaseTestCase
{
    using ApiBufferMarkerBaseTestCase::ApiBufferMarkerBaseTestCase;
    virtual std::string getRequiredCapabilitiesId() const override
    {
        return genBufferMarkerDeviceId(VkQueueFlagBits(m_arg0.arg0.testQueue), m_arg0.arg0.offset);
    }
    virtual void initDeviceCapabilities(DevCaps &caps) override
    {
        caps.resetQueues({makeQueueCreateInfo(m_arg0.arg0.testQueue)});

        caps.addExtension("VK_AMD_buffer_marker");
        if (m_arg0.arg0.useHostPtr)
            caps.addExtension("VK_EXT_external_memory_host");

        const SimpleAllocator::OptionalOffsetParams offsetParams(
            {caps.getContextManager().getDeviceFeaturesAndProperties().getDeviceProperties().limits.nonCoherentAtomSize,
             static_cast<VkDeviceSize>(m_arg0.arg0.offset)});
        caps.setAllocatorParams(offsetParams);
    }
};

void writeHostMemory(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::VkDeviceMemory memory, size_t size,
                     size_t memorySize, const void *data)
{
    void *const ptr = vk::mapMemory(vkd, device, memory, 0, memorySize, 0);

    deMemcpy(ptr, data, size);

    flushMappedMemoryRange(vkd, device, memory, 0, memorySize);

    vkd.unmapMemory(device, memory);
}

void invalidateHostMemory(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::VkDeviceMemory memory, size_t size)
{
    vk::mapMemory(vkd, device, memory, 0, size, 0);

    invalidateMappedMemoryRange(vkd, device, memory, 0, size);

    vkd.unmapMemory(device, memory);
}

bool checkMarkerBuffer(const DeviceInterface &vk, VkDevice device, const MovePtr<vk::Allocation> &memory,
                       const std::vector<uint32_t> &expected, size_t size, bool useHostMemory)
{
    if (useHostMemory)
    {
        invalidateHostMemory(vk, device, memory->getMemory(), size);
    }
    else
    {
        invalidateAlloc(vk, device, *memory);
    }

    const uint32_t *data = reinterpret_cast<const uint32_t *>(static_cast<const char *>(memory->getHostPtr()));

    for (size_t i = 0; i < expected.size(); ++i)
    {
        if (data[i] != expected[i])
            return false;
    }

    return true;
}

uint32_t chooseExternalMarkerMemoryType(const DeviceInterface &vkd, VkDevice device,
                                        VkExternalMemoryHandleTypeFlagBits externalType, uint32_t allowedBits,
                                        MovePtr<ExternalHostMemory> &hostMemory)
{
    VkMemoryHostPointerPropertiesEXT props = {
        vk::VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT,
        nullptr,
        0u,
    };

    if (vkd.getMemoryHostPointerPropertiesEXT(device, externalType, hostMemory->data, &props) == VK_SUCCESS)
    {
        allowedBits &= props.memoryTypeBits;
    }

    return deInt32BitScan((int32_t *)&allowedBits);
}

class ExternalHostAllocation : public Allocation
{
public:
    ExternalHostAllocation(Move<VkDeviceMemory> mem, void *hostPtr, size_t offset)
        : Allocation(*mem, offset, hostPtr)
        , m_memHolder(mem)
    {
    }

private:
    const Unique<VkDeviceMemory> m_memHolder;
};

void createMarkerBufferMemory(const InstanceInterface &vki, const DeviceInterface &vkd, VkPhysicalDevice physicalDevice,
                              VkDevice device, VkBuffer buffer, size_t bufferOffset, Allocator &allocator,
                              const MemoryRequirement allocRequirement, bool externalHostPtr,
                              MovePtr<ExternalHostMemory> &hostMemory, MovePtr<Allocation> &deviceMemory)
{
    VkMemoryRequirements memReqs = getBufferMemoryRequirements(vkd, device, buffer);

    if (externalHostPtr == false)
    {
        deviceMemory = allocator.allocate(memReqs, allocRequirement);
    }
    else
    {
        const VkExternalMemoryHandleTypeFlagBits externalType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;

        const VkPhysicalDeviceExternalMemoryHostPropertiesEXT hostProps =
            getPhysicalDeviceExternalMemoryHostProperties(vki, physicalDevice);
        bufferOffset = deAlignSize(bufferOffset, static_cast<size_t>(memReqs.alignment));
        hostMemory   = MovePtr<ExternalHostMemory>(
            new ExternalHostMemory(memReqs.size + bufferOffset, hostProps.minImportedHostPointerAlignment));

        const uint32_t externalMemType =
            chooseExternalMarkerMemoryType(vkd, device, externalType, memReqs.memoryTypeBits, hostMemory);

        if (externalMemType == VK_MAX_MEMORY_TYPES)
        {
            TCU_FAIL("Failed to find compatible external host memory type for marker buffer");
        }

        const VkImportMemoryHostPointerInfoEXT importInfo = {VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT,
                                                             nullptr, externalType, hostMemory->data};

        const VkMemoryAllocateInfo info = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, (const void *)&importInfo,
                                           hostMemory->size, externalMemType};

        deviceMemory = MovePtr<Allocation>(new ExternalHostAllocation(
            allocateMemory(vkd, device, &info), (((uint8_t *)hostMemory->data) + bufferOffset), bufferOffset));
    }

    VK_CHECK(vkd.bindBufferMemory(device, buffer, deviceMemory->getMemory(), deviceMemory->getOffset()));
}

tcu::TestStatus bufferMarkerSequential(Context &context, BaseTestParams params)
{
    const DeviceInterface &vk(context.getDeviceInterface());
    const VkDevice device(context.getDevice());
    const VkDeviceSize markerBufferSize(params.size * sizeof(uint32_t));
    VkExternalMemoryBufferCreateInfo externalMemoryBufferCreateInfo = {
        VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO, nullptr,
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT};
    VkBufferCreateInfo bufferCreateInfo = makeBufferCreateInfo(markerBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    if (params.useHostPtr)
        bufferCreateInfo.pNext = &externalMemoryBufferCreateInfo;
    Move<VkBuffer> markerBuffer(createBuffer(vk, device, &bufferCreateInfo));
    MovePtr<ExternalHostMemory> hostMemory;
    MovePtr<Allocation> markerMemory;

    createMarkerBufferMemory(context.getInstanceInterface(), vk, context.getPhysicalDevice(), device, *markerBuffer,
                             params.offset, context.getDefaultAllocator(), MemoryRequirement::HostVisible,
                             params.useHostPtr, hostMemory, markerMemory);

    de::Random rng(12345 ^ params.size);
    std::vector<uint32_t> expected(params.size);

    for (size_t i = 0; i < params.size; ++i)
        expected[i] = 0;

    if (params.useHostPtr)
    {
        writeHostMemory(vk, device, markerMemory->getMemory(), static_cast<size_t>(markerBufferSize), hostMemory->size,
                        &expected[0]);
    }
    else
    {
        deMemcpy(markerMemory->getHostPtr(), &expected[0], static_cast<size_t>(markerBufferSize));
        flushMappedMemoryRange(vk, device, markerMemory->getMemory(), markerMemory->getOffset(), VK_WHOLE_SIZE);
    }

    for (size_t i = 0; i < params.size; ++i)
        expected[i] = rng.getUint32();

    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                                          context.getDeviceQueueInfo(0).familyIndex));
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    beginCommandBuffer(vk, *cmdBuffer);

    for (size_t i = 0; i < params.size; ++i)
    {
        vk.cmdWriteBufferMarkerAMD(*cmdBuffer, params.stage, *markerBuffer,
                                   static_cast<VkDeviceSize>(sizeof(uint32_t) * i), expected[i]);
    }

    const VkMemoryBarrier memoryDep = {
        VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        nullptr,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_HOST_READ_BIT,
    };

    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &memoryDep, 0,
                          nullptr, 0, nullptr);

    VK_CHECK(vk.endCommandBuffer(*cmdBuffer));

    submitCommandsAndWait(vk, device, context.getDeviceQueueInfo(0).queue, *cmdBuffer);

    if (!checkMarkerBuffer(vk, device, markerMemory, expected, params.useHostPtr ? hostMemory->size : 0,
                           params.useHostPtr))
        return tcu::TestStatus::fail("Some marker values were incorrect");

    return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus bufferMarkerOverwrite(Context &context, BaseTestParams params)
{
    const DeviceInterface &vk(context.getDeviceInterface());
    const VkDevice device(context.getDevice());
    const VkDeviceSize markerBufferSize(params.size * sizeof(uint32_t));
    VkExternalMemoryBufferCreateInfo externalMemoryBufferCreateInfo = {
        VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO, nullptr,
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT};
    VkBufferCreateInfo bufferCreateInfo = makeBufferCreateInfo(markerBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    if (params.useHostPtr)
        bufferCreateInfo.pNext = &externalMemoryBufferCreateInfo;

    Move<VkBuffer> markerBuffer(createBuffer(vk, device, &bufferCreateInfo));
    MovePtr<ExternalHostMemory> hostMemory;
    MovePtr<Allocation> markerMemory;

    createMarkerBufferMemory(context.getInstanceInterface(), vk, context.getPhysicalDevice(), device, *markerBuffer,
                             params.offset, context.getDefaultAllocator(), MemoryRequirement::HostVisible,
                             params.useHostPtr, hostMemory, markerMemory);

    de::Random rng(12345 ^ params.size);
    std::vector<uint32_t> expected(params.size);

    for (size_t i = 0; i < params.size; ++i)
        expected[i] = 0;

    if (params.useHostPtr)
    {
        writeHostMemory(vk, device, markerMemory->getMemory(), static_cast<size_t>(markerBufferSize), hostMemory->size,
                        &expected[0]);
    }
    else
    {
        deMemcpy(markerMemory->getHostPtr(), &expected[0], static_cast<size_t>(markerBufferSize));
        flushMappedMemoryRange(vk, device, markerMemory->getMemory(), markerMemory->getOffset(), VK_WHOLE_SIZE);
    }

    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                                          context.getDeviceQueueInfo(0).familyIndex));
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    beginCommandBuffer(vk, *cmdBuffer);

    for (uint32_t i = 0; i < params.size * 10; ++i)
    {
        const uint32_t slot  = rng.getUint32() % static_cast<uint32_t>(params.size);
        const uint32_t value = i;

        expected[slot] = value;

        vk.cmdWriteBufferMarkerAMD(*cmdBuffer, params.stage, *markerBuffer,
                                   static_cast<VkDeviceSize>(sizeof(uint32_t) * slot), expected[slot]);
    }

    const VkMemoryBarrier memoryDep = {
        VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        nullptr,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_HOST_READ_BIT,
    };

    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &memoryDep, 0,
                          nullptr, 0, nullptr);

    VK_CHECK(vk.endCommandBuffer(*cmdBuffer));

    submitCommandsAndWait(vk, device, context.getDeviceQueueInfo(0).queue, *cmdBuffer);

    if (!checkMarkerBuffer(vk, device, markerMemory, expected, params.useHostPtr ? hostMemory->size : 0,
                           params.useHostPtr))
        return tcu::TestStatus::fail("Some marker values were incorrect");

    return tcu::TestStatus::pass("Pass");
}

enum MemoryDepMethod
{
    MEMORY_DEP_DRAW,
    MEMORY_DEP_DISPATCH,
    MEMORY_DEP_COPY
};

struct MemoryDepParams
{
    BaseTestParams base;
    MemoryDepMethod method;
};

enum MemoryDepOwner
{
    MEMORY_DEP_OWNER_NOBODY     = 0,
    MEMORY_DEP_OWNER_MARKER     = 1,
    MEMORY_DEP_OWNER_NON_MARKER = 2
};

typedef InstanceFactory1WithSupport<                   //
    FunctionInstance1<MemoryDepParams>,                //
    typename FunctionInstance1<MemoryDepParams>::Args, //
    FunctionSupport1<MemoryDepParams>,                 //
    FunctionPrograms1<MemoryDepParams>>
    ApiBufferMarkerMemDepTestCase;

struct BufferMarkerMemDepCase : public ApiBufferMarkerMemDepTestCase
{
    using ApiBufferMarkerMemDepTestCase::ApiBufferMarkerMemDepTestCase;
    virtual std::string getRequiredCapabilitiesId() const override
    {
        return genBufferMarkerDeviceId(VkQueueFlagBits(m_arg0.arg0.base.testQueue), m_arg0.arg0.base.offset);
    }
    virtual void initDeviceCapabilities(DevCaps &caps) override
    {
        caps.resetQueues({makeQueueCreateInfo(m_arg0.arg0.base.testQueue)});

        caps.addExtension("VK_AMD_buffer_marker");
        if (m_arg0.arg0.base.useHostPtr)
            caps.addExtension("VK_EXT_external_memory_host");

        const SimpleAllocator::OptionalOffsetParams offsetParams(
            {caps.getContextManager().getDeviceFeaturesAndProperties().getDeviceProperties().limits.nonCoherentAtomSize,
             static_cast<VkDeviceSize>(m_arg0.arg0.base.offset)});
        caps.setAllocatorParams(offsetParams);
    }
};

void computeMemoryDepBarrier(const MemoryDepParams &params, MemoryDepOwner owner, VkAccessFlags *memoryDepAccess,
                             VkPipelineStageFlags *executionScope)
{
    DE_ASSERT(owner != MEMORY_DEP_OWNER_NOBODY);

    if (owner == MEMORY_DEP_OWNER_MARKER)
    {
        *memoryDepAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
        *executionScope  = params.base.stage | VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else
    {
        if (params.method == MEMORY_DEP_COPY)
        {
            *memoryDepAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
            *executionScope  = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (params.method == MEMORY_DEP_DISPATCH)
        {
            *memoryDepAccess = VK_ACCESS_SHADER_WRITE_BIT;
            *executionScope  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        }
        else
        {
            *memoryDepAccess = VK_ACCESS_SHADER_WRITE_BIT;
            *executionScope  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
    }
}

// Randomly do buffer marker writes and other operations (draws, dispatches) that shader-write to a shared buffer.  Insert pipeline barriers
// when necessary and make sure that the synchronization between marker writes and non-marker writes are correctly handled by the barriers.
tcu::TestStatus bufferMarkerMemoryDep(Context &context, MemoryDepParams params)
{
    VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    if ((params.method == MEMORY_DEP_DRAW) || (params.method == MEMORY_DEP_DISPATCH))
        usageFlags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    else
        usageFlags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    const uint32_t numIters(1000);
    const DeviceInterface &vk(context.getDeviceInterface());
    const VkDevice device(context.getDevice());
    const uint32_t size(params.base.size);
    const VkDeviceSize markerBufferSize(params.base.size * sizeof(uint32_t));
    VkExternalMemoryBufferCreateInfo externalMemoryBufferCreateInfo = {
        VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO, nullptr,
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT};
    VkBufferCreateInfo bufferCreateInfo = makeBufferCreateInfo(markerBufferSize, usageFlags);
    if (params.base.useHostPtr)
        bufferCreateInfo.pNext = &externalMemoryBufferCreateInfo;
    Move<VkBuffer> markerBuffer(createBuffer(vk, device, &bufferCreateInfo));
    MovePtr<ExternalHostMemory> hostMemory;
    MovePtr<Allocation> markerMemory;

    createMarkerBufferMemory(context.getInstanceInterface(), vk, context.getPhysicalDevice(), device, *markerBuffer,
                             params.base.offset, context.getDefaultAllocator(), MemoryRequirement::HostVisible,
                             params.base.useHostPtr, hostMemory, markerMemory);

    de::Random rng(size ^ params.base.size);
    std::vector<uint32_t> expected(params.base.size, 0);

    Move<VkDescriptorPool> descriptorPool;
    Move<VkDescriptorSetLayout> descriptorSetLayout;
    Move<VkDescriptorSet> descriptorSet;
    Move<VkPipelineLayout> pipelineLayout;
    VkShaderStageFlags pushConstantStage = 0;

    if ((params.method == MEMORY_DEP_DRAW) || (params.method == MEMORY_DEP_DISPATCH))
    {
        DescriptorPoolBuilder descriptorPoolBuilder;

        descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u);
        descriptorPool = descriptorPoolBuilder.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

        DescriptorSetLayoutBuilder setLayoutBuilder;

        setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL);
        descriptorSetLayout = setLayoutBuilder.build(vk, device);

        const VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // VkStructureType sType;
            nullptr,                                        // const void* pNext;
            *descriptorPool,                                // VkDescriptorPool descriptorPool;
            1u,                                             // uint32_t setLayoutCount;
            &descriptorSetLayout.get()                      // const VkDescriptorSetLayout* pSetLayouts;
        };

        descriptorSet = allocateDescriptorSet(vk, device, &descriptorSetAllocateInfo);

        VkDescriptorBufferInfo markerBufferInfo = {*markerBuffer, 0, VK_WHOLE_SIZE};

        VkWriteDescriptorSet writeSet[] = {{
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // VkStructureType                  sType;
            nullptr,                                // const void*                      pNext;
            descriptorSet.get(),                    // VkDescriptorSet                  dstSet;
            0,                                      // uint32_t                         dstBinding;
            0,                                      // uint32_t                         dstArrayElement;
            1,                                      // uint32_t                         descriptorCount;
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,      // VkDescriptorType                 descriptorType;
            nullptr,                                // const VkDescriptorImageInfo*     pImageInfo;
            &markerBufferInfo,                      // const VkDescriptorBufferInfo*    pBufferInfo;
            nullptr                                 // const VkBufferView*              pTexelBufferViev
        }};

        vk.updateDescriptorSets(device, DE_LENGTH_OF_ARRAY(writeSet), writeSet, 0, nullptr);

        VkDescriptorSetLayout setLayout = descriptorSetLayout.get();

        pushConstantStage =
            (params.method == MEMORY_DEP_DISPATCH ? VK_SHADER_STAGE_COMPUTE_BIT : VK_SHADER_STAGE_FRAGMENT_BIT);

        const VkPushConstantRange pushConstantRange = {
            pushConstantStage,    // VkShaderStageFlags    stageFlags;
            0u,                   // uint32_t              offset;
            2 * sizeof(uint32_t), // uint32_t              size;
        };

        const VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
            nullptr,                                       // const void* pNext;
            (VkPipelineLayoutCreateFlags)0,                // VkPipelineLayoutCreateFlags flags;
            1u,                                            // uint32_t setLayoutCount;
            &setLayout,                                    // const VkDescriptorSetLayout* pSetLayouts;
            1u,                                            // uint32_t pushConstantRangeCount;
            &pushConstantRange,                            // const VkPushConstantRange* pPushConstantRanges;
        };

        pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutInfo);
    }

    Move<VkRenderPass> renderPass;
    Move<VkFramebuffer> fbo;
    Move<VkPipeline> pipeline;
    Move<VkShaderModule> vertexModule;
    Move<VkShaderModule> fragmentModule;
    Move<VkShaderModule> computeModule;

    if (params.method == MEMORY_DEP_DRAW)
    {
        const VkSubpassDescription subpassInfo = {
            0,                               // VkSubpassDescriptionFlags       flags;
            VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint             pipelineBindPoint;
            0,                               // uint32_t                        inputAttachmentCount;
            nullptr,                         // const VkAttachmentReference*    pInputAttachments;
            0,                               // uint32_t                        colorAttachmentCount;
            nullptr,                         // const VkAttachmentReference*    pColorAttachments;
            0,                               // const VkAttachmentReference*    pResolveAttachments;
            nullptr,                         // const VkAttachmentReference*    pDepthStencilAttachment;
            0,                               // uint32_t                        preserveAttachmentCount;
            nullptr                          // const uint32_t*                 pPreserveAttachments;
        };

        const VkRenderPassCreateInfo renderPassInfo = {
            VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // VkStructureType                   sType;
            nullptr,                                   // const void*                       pNext;
            0,                                         // VkRenderPassCreateFlags           flags;
            0,                                         // uint32_t                          attachmentCount;
            nullptr,                                   // const VkAttachmentDescription*    pAttachments;
            1,                                         // uint32_t                          subpassCount;
            &subpassInfo,                              // const VkSubpassDescription*       pSubpasses;
            0u,                                        // uint32_t                          dependencyCount;
            nullptr                                    // const VkSubpassDependency*        pDependencies
        };

        renderPass = createRenderPass(vk, device, &renderPassInfo);

        const VkFramebufferCreateInfo framebufferInfo = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // VkStructureType             sType;
            nullptr,                                   // const void*                 pNext;
            0,                                         // VkFramebufferCreateFlags    flags;
            renderPass.get(),                          // VkRenderPass                renderPass;
            0,                                         // uint32_t                    attachmentCount;
            nullptr,                                   // const VkImageView*          pAttachments;
            1,                                         // uint32_t                    width;
            1,                                         // uint32_t                    height;
            1,                                         // uint32_t                    layers;
        };

        fbo = createFramebuffer(vk, device, &framebufferInfo);

        vertexModule   = createShaderModule(vk, device, context.getBinaryCollection().get("vert"), 0u);
        fragmentModule = createShaderModule(vk, device, context.getBinaryCollection().get("frag"), 0u);

        const VkPipelineVertexInputStateCreateInfo vertexInputStateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                   // const void* pNext;
            (VkPipelineVertexInputStateCreateFlags)0,                  // VkPipelineVertexInputStateCreateFlags flags;
            0,                                                         // uint32_t vertexBindingDescriptionCount;
            nullptr, // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
            0,       // uint32_t vertexAttributeDescriptionCount;
            nullptr, // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
        };

        const VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                     // const void* pNext;
            (VkPipelineInputAssemblyStateCreateFlags)0, // VkPipelineInputAssemblyStateCreateFlags flags;
            VK_PRIMITIVE_TOPOLOGY_POINT_LIST,           // VkPrimitiveTopology topology;
            VK_FALSE,                                   // VkBool32 primitiveRestartEnable;
        };

        std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

        {
            const VkPipelineShaderStageCreateInfo createInfo = {
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
                nullptr,                                             // const void* pNext;
                (VkPipelineShaderStageCreateFlags)0,                 // VkPipelineShaderStageCreateFlags flags;
                VK_SHADER_STAGE_VERTEX_BIT,                          // VkShaderStageFlagBits stage;
                vertexModule.get(),                                  // VkShaderModule module;
                "main",                                              // const char* pName;
                nullptr,                                             // const VkSpecializationInfo* pSpecializationInfo;
            };

            shaderStages.push_back(createInfo);
        }

        {
            const VkPipelineShaderStageCreateInfo createInfo = {
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
                nullptr,                                             // const void* pNext;
                (VkPipelineShaderStageCreateFlags)0,                 // VkPipelineShaderStageCreateFlags flags;
                VK_SHADER_STAGE_FRAGMENT_BIT,                        // VkShaderStageFlagBits stage;
                fragmentModule.get(),                                // VkShaderModule module;
                "main",                                              // const char* pName;
                nullptr,                                             // const VkSpecializationInfo* pSpecializationInfo;
            };

            shaderStages.push_back(createInfo);
        }

        VkViewport viewport;

        viewport.x        = 0;
        viewport.y        = 0;
        viewport.width    = 1;
        viewport.height   = 1;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor;

        scissor.offset.x      = 0;
        scissor.offset.y      = 0;
        scissor.extent.width  = 1;
        scissor.extent.height = 1;

        const VkPipelineViewportStateCreateInfo pipelineViewportStateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                               // const void* pNext;
            (VkPipelineViewportStateCreateFlags)0,                 // VkPipelineViewportStateCreateFlags flags;
            1u,                                                    // uint32_t viewportCount;
            &viewport,                                             // const VkViewport* pViewports;
            1u,                                                    // uint32_t scissorCount;
            &scissor,                                              // const VkRect2D* pScissors;
        };

        const VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                    // const void* pNext;
            (VkPipelineRasterizationStateCreateFlags)0, // VkPipelineRasterizationStateCreateFlags flags;
            VK_FALSE,                                   // VkBool32 depthClampEnable;
            VK_FALSE,                                   // VkBool32 rasterizerDiscardEnable;
            VK_POLYGON_MODE_FILL,                       // VkPolygonMode polygonMode;
            VK_CULL_MODE_NONE,                          // VkCullModeFlags cullMode;
            VK_FRONT_FACE_COUNTER_CLOCKWISE,            // VkFrontFace frontFace;
            VK_FALSE,                                   // VkBool32 depthBiasEnable;
            0.0f,                                       // float depthBiasConstantFactor;
            0.0f,                                       // float depthBiasClamp;
            0.0f,                                       // float depthBiasSlopeFactor;
            1.0f,                                       // float lineWidth;
        };

        const VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateInfo = {

            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                  // const void* pNext;
            (VkPipelineMultisampleStateCreateFlags)0,                 // VkPipelineMultisampleStateCreateFlags flags;
            VK_SAMPLE_COUNT_1_BIT,                                    // VkSampleCountFlagBits rasterizationSamples;
            VK_FALSE,                                                 // VkBool32 sampleShadingEnable;
            1.0f,                                                     // float minSampleShading;
            nullptr,                                                  // const VkSampleMask* pSampleMask;
            VK_FALSE,                                                 // VkBool32 alphaToCoverageEnable;
            VK_FALSE,                                                 // VkBool32 alphaToOneEnable;
        };

        const VkStencilOpState noStencilOp = {
            VK_STENCIL_OP_KEEP,  // VkStencilOp    failOp
            VK_STENCIL_OP_KEEP,  // VkStencilOp    passOp
            VK_STENCIL_OP_KEEP,  // VkStencilOp    depthFailOp
            VK_COMPARE_OP_NEVER, // VkCompareOp    compareOp
            0,                   // uint32_t       compareMask
            0,                   // uint32_t       writeMask
            0                    // uint32_t       reference
        };

        VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                    // const void* pNext;
            (VkPipelineDepthStencilStateCreateFlags)0,                  // VkPipelineDepthStencilStateCreateFlags flags;
            VK_FALSE,                                                   // VkBool32 depthTestEnable;
            VK_FALSE,                                                   // VkBool32 depthWriteEnable;
            VK_COMPARE_OP_ALWAYS,                                       // VkCompareOp depthCompareOp;
            VK_FALSE,                                                   // VkBool32 depthBoundsTestEnable;
            VK_FALSE,                                                   // VkBool32 stencilTestEnable;
            noStencilOp,                                                // VkStencilOpState front;
            noStencilOp,                                                // VkStencilOpState back;
            0.0f,                                                       // float minDepthBounds;
            1.0f,                                                       // float maxDepthBounds;
        };

        const VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                  // const void* pNext;
            (VkPipelineColorBlendStateCreateFlags)0,                  // VkPipelineColorBlendStateCreateFlags flags;
            VK_FALSE,                                                 // VkBool32 logicOpEnable;
            VK_LOGIC_OP_COPY,                                         // VkLogicOp logicOp;
            0,                                                        // uint32_t attachmentCount;
            nullptr,                  // const VkPipelineColorBlendAttachmentState* pAttachments;
            {0.0f, 0.0f, 0.0f, 0.0f}, // float blendConstants[4];
        };

        const VkGraphicsPipelineCreateInfo graphicsPipelineInfo = {
            VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                         // const void* pNext;
            (VkPipelineCreateFlags)0,                        // VkPipelineCreateFlags flags;
            static_cast<uint32_t>(shaderStages.size()),      // uint32_t stageCount;
            de::dataOrNull(shaderStages),                    // const VkPipelineShaderStageCreateInfo* pStages;
            &vertexInputStateInfo,           // const VkPipelineVertexInputStateCreateInfo* pVertexInputState;
            &pipelineInputAssemblyStateInfo, // const VkPipelineInputAssemblyStateCreateInfo* pInputAssemblyState;
            nullptr,                         // const VkPipelineTessellationStateCreateInfo* pTessellationState;
            &pipelineViewportStateInfo,      // const VkPipelineViewportStateCreateInfo* pViewportState;
            &pipelineRasterizationStateInfo, // const VkPipelineRasterizationStateCreateInfo* pRasterizationState;
            &pipelineMultisampleStateInfo,   // const VkPipelineMultisampleStateCreateInfo* pMultisampleState;
            &pipelineDepthStencilStateInfo,  // const VkPipelineDepthStencilStateCreateInfo* pDepthStencilState;
            &pipelineColorBlendStateInfo,    // const VkPipelineColorBlendStateCreateInfo* pColorBlendState;
            nullptr,                         // const VkPipelineDynamicStateCreateInfo* pDynamicState;
            pipelineLayout.get(),            // VkPipelineLayout layout;
            renderPass.get(),                // VkRenderPass renderPass;
            0,                               // uint32_t subpass;
            VK_NULL_HANDLE,                  // VkPipeline basePipelineHandle;
            0,                               // int32_t basePipelineIndex;
        };

        pipeline = createGraphicsPipeline(vk, device, VK_NULL_HANDLE, &graphicsPipelineInfo);
    }
    else if (params.method == MEMORY_DEP_DISPATCH)
    {
        computeModule = createShaderModule(vk, device, context.getBinaryCollection().get("comp"), 0u);

        const VkPipelineShaderStageCreateInfo shaderStageInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                             // const void* pNext;
            (VkPipelineShaderStageCreateFlags)0,                 // VkPipelineShaderStageCreateFlags flags;
            VK_SHADER_STAGE_COMPUTE_BIT,                         // VkShaderStageFlagBits stage;
            computeModule.get(),                                 // VkShaderModule module;
            "main",                                              // const char* pName;
            nullptr                                              // const VkSpecializationInfo* pSpecializationInfo;
        };

        const VkComputePipelineCreateInfo computePipelineInfo = {
            VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, // VkStructureType                    sType;
            nullptr,                                        // const void*                        pNext;
            0u,                                             // VkPipelineCreateFlags              flags;
            shaderStageInfo,                                // VkPipelineShaderStageCreateInfo    stage;
            pipelineLayout.get(),                           // VkPipelineLayout                   layout;
            VK_NULL_HANDLE,                                 // VkPipeline                         basePipelineHandle;
            0                                               // int32_t                            basePipelineIndex;
        };

        pipeline = createComputePipeline(vk, device, VK_NULL_HANDLE, &computePipelineInfo);
    }

    if (params.base.useHostPtr)
    {
        writeHostMemory(vk, device, markerMemory->getMemory(), static_cast<size_t>(markerBufferSize), hostMemory->size,
                        &expected[0]);
    }
    else
    {
        deMemcpy(markerMemory->getHostPtr(), &expected[0], static_cast<size_t>(markerBufferSize));
        flushMappedMemoryRange(vk, device, markerMemory->getMemory(), markerMemory->getOffset(), VK_WHOLE_SIZE);
    }

    const uint32_t queueFamilyIdx = context.getDeviceQueueInfo(0).familyIndex;
    const Unique<VkCommandPool> cmdPool(
        createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIdx));
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    beginCommandBuffer(vk, *cmdBuffer);

    VkDescriptorSet setHandle = *descriptorSet;

    std::vector<MemoryDepOwner> dataOwner(size, MEMORY_DEP_OWNER_NOBODY);

    if (params.method == MEMORY_DEP_DRAW)
    {
        vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
        vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0, 1, &setHandle, 0,
                                 nullptr);
    }
    else if (params.method == MEMORY_DEP_DISPATCH)
    {
        vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
        vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0, 1, &setHandle, 0,
                                 nullptr);
    }

    if (params.base.useHostPtr)
    {
        writeHostMemory(vk, device, markerMemory->getMemory(), static_cast<size_t>(markerBufferSize), hostMemory->size,
                        &expected[0]);
    }
    else
    {
        deMemcpy(markerMemory->getHostPtr(), &expected[0], static_cast<size_t>(markerBufferSize));
        flushMappedMemoryRange(vk, device, markerMemory->getMemory(), markerMemory->getOffset(), VK_WHOLE_SIZE);
    }

    uint32_t writeStages = 0;
    uint32_t writeAccess = 0;

    for (uint32_t i = 0; i < numIters; ++i)
    {
        uint32_t slot           = rng.getUint32() % size;
        MemoryDepOwner oldOwner = dataOwner[slot];
        MemoryDepOwner newOwner = static_cast<MemoryDepOwner>(1 + (rng.getUint32() % 2));

        DE_ASSERT(newOwner == MEMORY_DEP_OWNER_MARKER || newOwner == MEMORY_DEP_OWNER_NON_MARKER);
        DE_ASSERT(slot < size);

        if ((oldOwner != newOwner && oldOwner != MEMORY_DEP_OWNER_NOBODY) ||
            (oldOwner == MEMORY_DEP_OWNER_NON_MARKER && newOwner == MEMORY_DEP_OWNER_NON_MARKER))
        {
            VkBufferMemoryBarrier memoryDep = {
                VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType    sType;
                nullptr,                                 // const void*        pNext;
                0,                                       // VkAccessFlags      srcAccessMask;
                0,                                       // VkAccessFlags      dstAccessMask;
                queueFamilyIdx,                          // uint32_t           srcQueueFamilyIndex;
                queueFamilyIdx,                          // uint32_t           dstQueueFamilyIndex;
                *markerBuffer,                           // VkBuffer           buffer;
                sizeof(uint32_t) * slot,                 // VkDeviceSize       offset;
                sizeof(uint32_t)                         // VkDeviceSize       size;
            };

            VkPipelineStageFlags srcStageMask;
            VkPipelineStageFlags dstStageMask;

            computeMemoryDepBarrier(params, oldOwner, &memoryDep.srcAccessMask, &srcStageMask);
            computeMemoryDepBarrier(params, newOwner, &memoryDep.dstAccessMask, &dstStageMask);

            vk.cmdPipelineBarrier(*cmdBuffer, srcStageMask, dstStageMask, 0, 0, nullptr, 1, &memoryDep, 0, nullptr);
        }

        if (params.method == MEMORY_DEP_DRAW)
        {
            const VkRenderPassBeginInfo beginInfo = {
                VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, // VkStructureType        sType;
                nullptr,                                  // const void*            pNext;
                renderPass.get(),                         // VkRenderPass           renderPass;
                fbo.get(),                                // VkFramebuffer          framebuffer;
                {{
                     0,
                     0,
                 },
                 {1, 1}}, // VkRect2D               renderArea;
                0,        // uint32_t               clearValueCount;
                nullptr   // const VkClearValue*    pClearValues;
            };

            vk.cmdBeginRenderPass(*cmdBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
        }

        const uint32_t value = i;

        if (newOwner == MEMORY_DEP_OWNER_MARKER)
        {
            vk.cmdWriteBufferMarkerAMD(*cmdBuffer, params.base.stage, *markerBuffer, sizeof(uint32_t) * slot, value);

            writeStages |= VK_PIPELINE_STAGE_TRANSFER_BIT;
            writeAccess |= VK_ACCESS_TRANSFER_WRITE_BIT;
        }
        else
        {
            DE_ASSERT(newOwner == MEMORY_DEP_OWNER_NON_MARKER);

            if (params.method == MEMORY_DEP_COPY)
            {
                vk.cmdUpdateBuffer(*cmdBuffer, *markerBuffer, sizeof(uint32_t) * slot, sizeof(uint32_t), &value);

                writeStages |= VK_PIPELINE_STAGE_TRANSFER_BIT;
                writeAccess |= VK_ACCESS_TRANSFER_WRITE_BIT;
            }
            else if (params.method == MEMORY_DEP_DRAW)
            {
                const uint32_t pushConst[] = {slot, value};

                vk.cmdPushConstants(*cmdBuffer, *pipelineLayout, pushConstantStage, 0, sizeof(pushConst), pushConst);
                vk.cmdDraw(*cmdBuffer, 1, 1, i, 0);

                writeStages |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                writeAccess |= VK_ACCESS_SHADER_WRITE_BIT;
            }
            else
            {
                const uint32_t pushConst[] = {slot, value};

                vk.cmdPushConstants(*cmdBuffer, *pipelineLayout, pushConstantStage, 0, sizeof(pushConst), pushConst);
                vk.cmdDispatch(*cmdBuffer, 1, 1, 1);

                writeStages |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
                writeAccess |= VK_ACCESS_SHADER_WRITE_BIT;
            }
        }

        dataOwner[slot] = newOwner;
        expected[slot]  = value;

        if (params.method == MEMORY_DEP_DRAW)
        {
            vk.cmdEndRenderPass(*cmdBuffer);
        }
    }

    const VkMemoryBarrier memoryDep = {
        VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        nullptr,
        writeAccess,
        VK_ACCESS_HOST_READ_BIT,
    };

    vk.cmdPipelineBarrier(*cmdBuffer, writeStages, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &memoryDep, 0, nullptr, 0,
                          nullptr);

    VK_CHECK(vk.endCommandBuffer(*cmdBuffer));

    submitCommandsAndWait(vk, device, context.getDeviceQueueInfo(0).queue, *cmdBuffer);

    if (!checkMarkerBuffer(vk, device, markerMemory, expected, params.base.useHostPtr ? hostMemory->size : 0,
                           params.base.useHostPtr))
        return tcu::TestStatus::fail("Some marker values were incorrect");

    return tcu::TestStatus::pass("Pass");
}

void initMemoryDepPrograms(SourceCollections &programCollection, const MemoryDepParams params)
{
    if (params.method == MEMORY_DEP_DRAW)
    {
        {
            std::ostringstream src;

            src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
                << "layout(location = 0) flat out uint offset;\n"
                << "out gl_PerVertex { vec4 gl_Position; float gl_PointSize; };\n"
                << "void main() {\n"
                << "    offset = gl_VertexIndex;\n"
                << "    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);\n"
                << "    gl_PointSize = 1.0f;\n"
                << "}\n";

            programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
        }

        {
            std::ostringstream src;

            src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
                << "layout(push_constant) uniform Constants { uvec2 params; } pc;\n"
                << "layout(std430, set = 0, binding = 0) buffer Data { uint elems[]; } data;\n"
                << "layout(location = 0) flat in uint offset;\n"
                << "void main() {\n"
                << "    data.elems[pc.params.x] = pc.params.y;\n"
                << "}\n";

            programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
        }
    }
    else if (params.method == MEMORY_DEP_DISPATCH)
    {
        {
            std::ostringstream src;

            src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
                << "layout(local_size_x = 1u, local_size_y = 1u, local_size_z = 1u) in;\n"
                << "layout(push_constant) uniform Constants { uvec2 params; } pc;\n"
                << "layout(std430, set = 0, binding = 0) buffer Data { uint elems[]; } data;\n"
                << "void main() {\n"
                << "    data.elems[pc.params.x] = pc.params.y;\n"
                << "}\n";

            programCollection.glslSources.add("comp") << glu::ComputeSource(src.str());
        }
    }
}

void checkBufferMarkerSupport(Context &context, BaseTestParams params)
{
    if (params.useHostPtr)
        context.requireDeviceFunctionality("VK_EXT_external_memory_host");

    context.requireDeviceFunctionality("VK_AMD_buffer_marker");
}

void checkBufferMarkerSupport(Context &context, MemoryDepParams params)
{
    if (params.base.useHostPtr)
        context.requireDeviceFunctionality("VK_EXT_external_memory_host");

    context.requireDeviceFunctionality("VK_AMD_buffer_marker");
}

std::string getTestCaseName(const std::string base, size_t offset)
{
    if (offset == 0)
        return base;
    return base + "_offset_" + std::to_string(offset);
}

tcu::TestCaseGroup *createBufferMarkerTestsInGroup(tcu::TestContext &testCtx)
{
    // AMD_buffer_marker Tests
    tcu::TestCaseGroup *root = (new tcu::TestCaseGroup(testCtx, "buffer_marker"));

    VkQueueFlagBits queues[] = {VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_COMPUTE_BIT, VK_QUEUE_TRANSFER_BIT};
    const char *queueNames[] = {"graphics", "compute", "transfer"};

    BaseTestParams base;
    deMemset(&base, 0, sizeof(base));

    for (size_t queueNdx = 0; queueNdx < DE_LENGTH_OF_ARRAY(queues); ++queueNdx)
    {
        // Buffer marker tests for a specific queue family
        tcu::TestCaseGroup *queueGroup = (new tcu::TestCaseGroup(testCtx, queueNames[queueNdx]));

        const char *memoryNames[] = {"external_host_mem", "default_mem"};
        const bool memoryTypes[]  = {true, false};

        base.testQueue = queues[queueNdx];

        for (size_t memNdx = 0; memNdx < DE_LENGTH_OF_ARRAY(memoryTypes); ++memNdx)
        {
            tcu::TestCaseGroup *memoryGroup = (new tcu::TestCaseGroup(testCtx, memoryNames[memNdx]));

            base.useHostPtr = memoryTypes[memNdx];

            VkPipelineStageFlagBits stages[] = {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT};
            const char *stageNames[]         = {"top_of_pipe", "bottom_of_pipe"};

            for (size_t stageNdx = 0; stageNdx < DE_LENGTH_OF_ARRAY(stages); ++stageNdx)
            {
                tcu::TestCaseGroup *stageGroup = (new tcu::TestCaseGroup(testCtx, stageNames[stageNdx]));

                base.stage = stages[stageNdx];

                {
                    tcu::TestCaseGroup *sequentialGroup = (new tcu::TestCaseGroup(testCtx, "sequential"));

                    base.size   = 4;
                    base.offset = 0;

                    // Writes 4 sequential marker values into a buffer
                    addFunctionCase<BaseTestParams, BufferMarkerBaseCase>(
                        sequentialGroup, "4", checkBufferMarkerSupport, bufferMarkerSequential, base);

                    base.size   = 64;
                    base.offset = 0;

                    // Writes 64 sequential marker values into a buffer
                    addFunctionCase<BaseTestParams, BufferMarkerBaseCase>(
                        sequentialGroup, "64", checkBufferMarkerSupport, bufferMarkerSequential, base);

                    base.offset = 16;

                    // Writes 64 sequential marker values into a buffer offset by 16
                    addFunctionCase<BaseTestParams, BufferMarkerBaseCase>(
                        sequentialGroup, getTestCaseName("64", base.offset), checkBufferMarkerSupport,
                        bufferMarkerSequential, base);

                    base.size   = 65536;
                    base.offset = 0;

                    // Writes 65536 sequential marker values into a buffer
                    addFunctionCase<BaseTestParams, BufferMarkerBaseCase>(
                        sequentialGroup, "65536", checkBufferMarkerSupport, bufferMarkerSequential, base);

                    base.offset = 1024;

                    // Writes 65536 sequential marker values into a buffer offset by 1024
                    addFunctionCase<BaseTestParams, BufferMarkerBaseCase>(
                        sequentialGroup, getTestCaseName("65536", base.offset), checkBufferMarkerSupport,
                        bufferMarkerSequential, base);

                    base.offset = 0;
                    stageGroup->addChild(sequentialGroup);
                }

                {
                    tcu::TestCaseGroup *overwriteGroup = (new tcu::TestCaseGroup(testCtx, "overwrite"));

                    base.size = 1;

                    // Randomly overwrites marker values to a 1-size buffer
                    addFunctionCase<BaseTestParams, BufferMarkerBaseCase>(overwriteGroup, "1", checkBufferMarkerSupport,
                                                                          bufferMarkerOverwrite, base);

                    base.size = 4;

                    // Randomly overwrites marker values to a 4-size buffer
                    addFunctionCase<BaseTestParams, BufferMarkerBaseCase>(overwriteGroup, "4", checkBufferMarkerSupport,
                                                                          bufferMarkerOverwrite, base);

                    base.size = 64;

                    // Randomly overwrites markers values to a 64-size buffer
                    addFunctionCase<BaseTestParams, BufferMarkerBaseCase>(
                        overwriteGroup, "64", checkBufferMarkerSupport, bufferMarkerOverwrite, base);
                    base.offset = 24;

                    // Randomly overwrites markers values to a 64-size buffer at offset 24
                    addFunctionCase<BaseTestParams, BufferMarkerBaseCase>(
                        overwriteGroup, getTestCaseName("64", base.offset), checkBufferMarkerSupport,
                        bufferMarkerOverwrite, base);

                    base.offset = 0;

                    stageGroup->addChild(overwriteGroup);
                }

                {
                    tcu::TestCaseGroup *memoryDepGroup = (new tcu::TestCaseGroup(testCtx, "memory_dep"));

                    MemoryDepParams params;
                    size_t offsets[] = {0, 24};
                    deMemset(&params, 0, sizeof(params));

                    for (size_t offsetIdx = 0; offsetIdx < de::arrayLength(offsets); offsetIdx++)
                    {
                        params.base        = base;
                        params.base.size   = 128;
                        params.base.offset = offsets[offsetIdx];

                        if (params.base.testQueue == VK_QUEUE_GRAPHICS_BIT)
                        {
                            params.method = MEMORY_DEP_DRAW;

                            // Test memory dependencies between marker writes and draws
                            addFunctionCaseWithPrograms<MemoryDepParams, BufferMarkerMemDepCase>(
                                memoryDepGroup, getTestCaseName("draw", params.base.offset), checkBufferMarkerSupport,
                                initMemoryDepPrograms, bufferMarkerMemoryDep, params);
                        }

                        if (params.base.testQueue != VK_QUEUE_TRANSFER_BIT)
                        {
                            params.method = MEMORY_DEP_DISPATCH;

                            // Test memory dependencies between marker writes and compute dispatches
                            addFunctionCaseWithPrograms<MemoryDepParams, BufferMarkerMemDepCase>(
                                memoryDepGroup, getTestCaseName("dispatch", params.base.offset),
                                checkBufferMarkerSupport, initMemoryDepPrograms, bufferMarkerMemoryDep, params);
                        }

                        params.method = MEMORY_DEP_COPY;

                        // Test memory dependencies between marker writes and buffer copies
                        addFunctionCaseWithPrograms<MemoryDepParams, BufferMarkerMemDepCase>(
                            memoryDepGroup, getTestCaseName("buffer_copy", params.base.offset),
                            checkBufferMarkerSupport, initMemoryDepPrograms, bufferMarkerMemoryDep, params);
                    }

                    stageGroup->addChild(memoryDepGroup);
                }

                memoryGroup->addChild(stageGroup);
            }

            queueGroup->addChild(memoryGroup);
        }

        root->addChild(queueGroup);
    }

    return root;
}

} // namespace

tcu::TestCaseGroup *createBufferMarkerTests(tcu::TestContext &testCtx)
{
    return createBufferMarkerTestsInGroup(testCtx);
}

} // namespace api
} // namespace vkt
