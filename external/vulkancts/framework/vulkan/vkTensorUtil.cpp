/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2025 ARM Ltd.
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
 */
/*!
 * \file
 * \brief Utilities for tensors.
 */
/*--------------------------------------------------------------------*/

#include "vkTensorWithMemory.hpp"
#include "vkTensorMemoryUtil.hpp"
#include "vkTensorUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include <cstddef>
#include <cstdint>

namespace vk
{

#ifndef CTS_USES_VULKANSC

void uploadToTensor(const DeviceInterface &vk, const VkDevice device, vk::Allocator &allocator, const VkQueue queue,
                    const uint32_t queueFamilyIndex, const TensorWithMemory &tensor, const void *const hostBuffer,
                    const uint64_t hostBufferSize, const bool forceStaging)
{
    DE_ASSERT(tensor.getAllocationSize() >= hostBufferSize);

    const vk::Allocation &tensorAllocation = tensor.getAllocation();
    const bool useStagingBuffer            = forceStaging || !tensorAllocation.isHostVisible();

    if (useStagingBuffer)
    {
        // Set up staging buffer
        const vk::VkBufferCreateInfo srcBufferCreateInfo =
            makeBufferCreateInfo(hostBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        const vk::BufferWithMemory srcBuffer(vk, device, allocator, srcBufferCreateInfo,
                                             vk::MemoryRequirement::HostVisible);
        const vk::Allocation &srcAllocation = srcBuffer.getAllocation();

        // Copy from host memory into staging buffer
        memcpy(srcAllocation.getHostPtr(), hostBuffer, static_cast<size_t>(hostBufferSize));
        flushAlloc(vk, device, srcAllocation);

        // We need to use the same external memory handle types for the aliasing buffer, in case the allocation is external
        vk::VkExternalMemoryBufferCreateInfo externalCreateInfo = initVulkanStructure();
        externalCreateInfo.handleTypes                          = tensor.getExternalMemoryHandleTypes();

        // Set up destination buffer aliasing tensor memory
        vk::VkBufferCreateInfo dstBufferCreateInfo =
            makeBufferCreateInfo(hostBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        if (externalCreateInfo.handleTypes != 0)
        {
            dstBufferCreateInfo.pNext = &externalCreateInfo;
        }
        const Unique<VkBuffer> dstBuffer(createBuffer(vk, device, &dstBufferCreateInfo));
        vk.bindBufferMemory(device, *dstBuffer, tensorAllocation.getMemory(), tensorAllocation.getOffset());

        const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
        const Unique<VkCommandBuffer> cmdBuffer(
            allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
        beginCommandBuffer(vk, *cmdBuffer);

        // Copy the memory from staging buffer to destination buffer
        const vk::VkBufferCopy copyRegion{0, 0, hostBufferSize};
        vk.cmdCopyBuffer(*cmdBuffer, *srcBuffer, *dstBuffer, 1, &copyRegion);

        // Memory barrier to make the uploaded tensor memory visible
        const vk::VkMemoryBarrier postTransferBarrier =
            makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT);
        vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1,
                              &postTransferBarrier, 0, nullptr, 0, nullptr);

        endCommandBuffer(vk, *cmdBuffer);
        submitCommandsAndWait(vk, device, queue, *cmdBuffer);
    }
    else
    {
        // Copy directly from host to tensor memory
        memcpy(tensorAllocation.getHostPtr(), hostBuffer, static_cast<size_t>(hostBufferSize));
        flushAlloc(vk, device, tensorAllocation);
    }
}

void downloadFromTensor(const DeviceInterface &vk, const VkDevice device, vk::Allocator &allocator, const VkQueue queue,
                        const uint32_t queueFamilyIndex, const vk::TensorWithMemory &tensor, void *const hostBuffer,
                        uint64_t hostBufferSize, bool forceStaging)
{
    DE_ASSERT(tensor.getAllocationSize() >= hostBufferSize);

    const vk::Allocation &tensorAllocation = tensor.getAllocation();
    const bool useStagingBuffer            = forceStaging || !tensorAllocation.isHostVisible();

    if (useStagingBuffer)
    {
        // We need to use the same external memory handle types for the aliasing buffer, in case the allocation is external
        vk::VkExternalMemoryBufferCreateInfo externalCreateInfo = initVulkanStructure();
        externalCreateInfo.handleTypes                          = tensor.getExternalMemoryHandleTypes();

        // Set up source buffer aliasing tensor memory
        vk::VkBufferCreateInfo srcBufferCreateInfo =
            makeBufferCreateInfo(hostBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        if (externalCreateInfo.handleTypes != 0)
        {
            srcBufferCreateInfo.pNext = &externalCreateInfo;
        }
        const Unique<VkBuffer> srcBuffer(createBuffer(vk, device, &srcBufferCreateInfo));
        vk.bindBufferMemory(device, *srcBuffer, tensorAllocation.getMemory(), tensorAllocation.getOffset());

        // Set up readback buffer
        const vk::VkBufferCreateInfo dstBufferCreateInfo =
            makeBufferCreateInfo(hostBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        const vk::BufferWithMemory dstBuffer(vk, device, allocator, dstBufferCreateInfo,
                                             vk::MemoryRequirement::HostVisible);
        const vk::Allocation &dstAllocation = dstBuffer.getAllocation();

        const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
        const Unique<VkCommandBuffer> cmdBuffer(
            allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
        beginCommandBuffer(vk, *cmdBuffer);

        // Memory barrier to make all writes to the tensor memory visible to transfer stage
        const vk::VkMemoryBarrier preTransferBarrier =
            makeMemoryBarrier(VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1,
                              &preTransferBarrier, 0, nullptr, 0, nullptr);

        // Copy the memory from source buffer to readback buffer
        const vk::VkBufferCopy copyRegion{0, 0, hostBufferSize};
        vk.cmdCopyBuffer(*cmdBuffer, *srcBuffer, *dstBuffer, 1, &copyRegion);

        // Memory barrier to make readback buffer available to host
        const vk::VkMemoryBarrier postTransferBarrier =
            makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1,
                              &postTransferBarrier, 0, nullptr, 0, nullptr);

        endCommandBuffer(vk, *cmdBuffer);
        submitCommandsAndWait(vk, device, queue, *cmdBuffer);

        // Invalidate readback buffer allocation to make it visible to host
        invalidateAlloc(vk, device, dstAllocation);

        // Copy the tensor data from readback buffer to host
        memcpy(hostBuffer, dstAllocation.getHostPtr(), static_cast<size_t>(hostBufferSize));
    }
    else
    {
        // Copy directly from tensor memory to host
        invalidateAlloc(vk, device, tensorAllocation);
        memcpy(hostBuffer, tensorAllocation.getHostPtr(), static_cast<size_t>(hostBufferSize));
    }
}

void clearTensor(const DeviceInterface &vk, const VkDevice device, vk::Allocator &allocator, const VkQueue queue,
                 const uint32_t queueFamilyIndex, const TensorWithMemory &tensor, bool forceStaging)
{
    const vk::Allocation &tensorAllocation = tensor.getAllocation();
    const bool useStagingBuffer            = forceStaging || !tensorAllocation.isHostVisible();

    if (useStagingBuffer)
    {
        const std::vector<unsigned char> data(static_cast<size_t>(tensor.getAllocationSize()));
        uploadToTensor(vk, device, allocator, queue, queueFamilyIndex, tensor, data.data(), data.size(), forceStaging);
    }
    else
    {
        memset(tensorAllocation.getHostPtr(), 0, static_cast<size_t>(tensor.getAllocationSize()));
        flushAlloc(vk, device, tensorAllocation);
    }
}

#endif // CTS_USES_VULKANSC

} // namespace vk
