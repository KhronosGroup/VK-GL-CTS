#ifndef _VKTBITSTREAMBUFFERIMPL_HPP
#define _VKTBITSTREAMBUFFERIMPL_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 The Khronos Group Inc.
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
 * \brief Bitstream buffer implementation for the CTS.
 *//*--------------------------------------------------------------------*/
/*
 * Copyright 2023 NVIDIA Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <atomic>
#include <iostream>
#include <vector>

#include "vkvideo_parser/VulkanBitstreamBuffer.h"

#include "vktVideoTestUtils.hpp"

namespace vkt
{
namespace video
{

using BufferPtr = de::MovePtr<BufferWithMemory>;

// This class is required by the NVIDIA sample decoder interface.
// The following is a CTS implementation of the VulkanBitstreamBuffer interface upstream.
class BitstreamBufferImpl : public VulkanBitstreamBuffer
{
public:
    static VkResult Create(DeviceContext *devctx, uint32_t queueFamilyIndex, VkDeviceSize bufferSize,
                           VkDeviceSize bufferOffsetAlignment, VkDeviceSize bufferSizeAlignment,
                           VkSharedBaseObj<BitstreamBufferImpl> &vulkanBitstreamBuffer,
                           const VkVideoProfileListInfoKHR *profileList);

    int32_t AddRef() override
    {
        return ++m_refCount;
    }

    int32_t Release() override
    {
        DE_ASSERT(m_refCount > 0);
        uint32_t ret = --m_refCount;
        if (ret == 0)
        {
            delete this;
        }
        return ret;
    }

    int32_t GetRefCount() override
    {
        DE_ASSERT(m_refCount > 0);
        return m_refCount;
    }

    VkDeviceSize GetMaxSize() const override;
    VkDeviceSize GetOffsetAlignment() const override;
    VkDeviceSize GetSizeAlignment() const override;
    VkDeviceSize Resize(VkDeviceSize newSize, VkDeviceSize copySize = 0, VkDeviceSize copyOffset = 0) override;
    VkDeviceSize Clone(VkDeviceSize newSize, VkDeviceSize copySize, VkDeviceSize copyOffset,
                       VkSharedBaseObj<VulkanBitstreamBuffer> &vulkanBitstreamBuffer) override;

    int64_t MemsetData(uint32_t value, VkDeviceSize offset, VkDeviceSize size) override;
    int64_t CopyDataToBuffer(uint8_t *dstBuffer, VkDeviceSize dstOffset, VkDeviceSize srcOffset,
                             VkDeviceSize size) const override;
    int64_t CopyDataToBuffer(VkSharedBaseObj<VulkanBitstreamBuffer> &dstBuffer, VkDeviceSize dstOffset,
                             VkDeviceSize srcOffset, VkDeviceSize size) const override;
    int64_t CopyDataFromBuffer(const uint8_t *sourceBuffer, VkDeviceSize srcOffset, VkDeviceSize dstOffset,
                               VkDeviceSize size) override;
    int64_t CopyDataFromBuffer(const VkSharedBaseObj<VulkanBitstreamBuffer> &sourceBuffer, VkDeviceSize srcOffset,
                               VkDeviceSize dstOffset, VkDeviceSize size) override;
    uint8_t *GetDataPtr(VkDeviceSize offset, VkDeviceSize &maxSize) override;
    const uint8_t *GetReadOnlyDataPtr(VkDeviceSize offset, VkDeviceSize &maxSize) const override;

    void FlushRange(VkDeviceSize offset, VkDeviceSize size) const override;
    void InvalidateRange(VkDeviceSize offset, VkDeviceSize size) const override;

    VkBuffer GetBuffer() const override
    {
        return m_bitstreamBuffer->get();
    }
    VkDeviceMemory GetDeviceMemory() const override
    {
        return m_bitstreamBuffer->getAllocation().getMemory();
    }

    uint32_t AddStreamMarker(uint32_t streamOffset) override;
    uint32_t SetStreamMarker(uint32_t streamOffset, uint32_t index) override;
    uint32_t GetStreamMarker(uint32_t index) const override;
    uint32_t GetStreamMarkersCount() const override;
    const uint32_t *GetStreamMarkersPtr(uint32_t startIndex, uint32_t &maxCount) const override;
    uint32_t ResetStreamMarkers() override;

    operator VkDeviceMemory()
    {
        return GetDeviceMemory();
    }
    operator bool()
    {
        return !!m_bitstreamBuffer;
    }

    VkResult CopyDataToBuffer(const uint8_t *pData, VkDeviceSize size, VkDeviceSize &dstBufferOffset) const;

private:
    VkResult CreateBuffer(DeviceContext *ctx, uint32_t queueFamilyIndex, VkDeviceSize &bufferSize,
                          VkDeviceSize bufferSizeAlignment, const VkVideoProfileListInfoKHR *profileList);

    uint8_t *CheckAccess(VkDeviceSize offset, VkDeviceSize size) const;

    VkResult Initialize(VkDeviceSize bufferSize);

    BitstreamBufferImpl(DeviceContext *devctx, uint32_t queueFamilyIndex, VkDeviceSize bufferOffsetAlignment,
                        VkDeviceSize bufferSizeAlignment, const VkVideoProfileListInfoKHR *profileList)
        : VulkanBitstreamBuffer()
        , m_refCount(0)
        , m_devctx(devctx)
        , m_profileList(profileList)
        , m_queueFamilyIndex(queueFamilyIndex)
        , m_bufferOffsetAlignment(bufferOffsetAlignment)
        , m_bufferSizeAlignment(bufferSizeAlignment)
        , m_bufferSize(0)
        , m_streamMarkers(256)
    {
    }

private:
    std::atomic<int32_t> m_refCount;
    DeviceContext *m_devctx;
    const VkVideoProfileListInfoKHR *m_profileList;
    uint32_t m_queueFamilyIndex;
    VkDeviceSize m_bufferOffsetAlignment;
    VkDeviceSize m_bufferSizeAlignment;
    BufferPtr m_bitstreamBuffer;
    VkDeviceSize m_bufferSize;
    std::vector<uint32_t> m_streamMarkers;
};

} // namespace video
} // namespace vkt

#endif // _VKTBITSTREAMBUFFERIMPL_HPP
