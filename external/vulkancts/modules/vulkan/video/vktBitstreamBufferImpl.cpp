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
 *	  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief CTS implementation of the NVIDIA BitstreamBuffer interface.
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

#include "vktBitstreamBufferImpl.hpp"

#include <cstring>

namespace vkt
{
namespace video
{

VkResult
BitstreamBufferImpl::Create(DeviceContext* devctx, deUint32 queueFamilyIndex, VkDeviceSize bufferSize, VkDeviceSize bufferOffsetAlignment, VkDeviceSize bufferSizeAlignment, VkSharedBaseObj<BitstreamBufferImpl>& vulkanBitstreamBuffer, const VkVideoProfileListInfoKHR* profileList)
{
	VkSharedBaseObj<BitstreamBufferImpl> vkBitstreamBuffer(new BitstreamBufferImpl(devctx,
																				   queueFamilyIndex,
																				   bufferOffsetAlignment,
																				   bufferSizeAlignment,
																				   profileList));
	DE_ASSERT(vkBitstreamBuffer);

	VK_CHECK(vkBitstreamBuffer->Initialize(bufferSize));

	vulkanBitstreamBuffer = vkBitstreamBuffer;

	return VK_SUCCESS;
}

VkResult BitstreamBufferImpl::Initialize(VkDeviceSize bufferSize)
{
	auto& vk	 = m_devctx->getDeviceDriver();
	auto  device = m_devctx->device;

	if (m_bufferSize >= bufferSize)
	{
		VkDeviceSize size = MemsetData(0x00, 0, m_bufferSize);
		DE_ASSERT(size == m_bufferSize);
		DE_UNREF(size);
		return VK_SUCCESS;
	}

	VkBufferCreateInfo createBufferInfo	   = {};
	createBufferInfo.sType				   = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	createBufferInfo.pNext				   = m_profileList;
	createBufferInfo.size				   = deAlignSize(bufferSize, m_bufferSizeAlignment);
	createBufferInfo.usage				   = VK_BUFFER_USAGE_VIDEO_DECODE_SRC_BIT_KHR;
	createBufferInfo.flags				   = 0;
	createBufferInfo.sharingMode		   = VK_SHARING_MODE_EXCLUSIVE;
	createBufferInfo.queueFamilyIndexCount = 1;
	createBufferInfo.pQueueFamilyIndices   = &m_queueFamilyIndex;
	m_bitstreamBuffer					   = BufferPtr(new BufferWithMemory(vk, device, m_devctx->allocator(), createBufferInfo, MemoryRequirement::HostVisible | MemoryRequirement::Coherent | MemoryRequirement::Cached));

	m_bufferSize						   = bufferSize;

	return VK_SUCCESS;
}

VkResult BitstreamBufferImpl::CopyDataToBuffer(const deUint8* pData,
											   VkDeviceSize	  size,
											   VkDeviceSize&  dstBufferOffset) const
{
	DE_ASSERT((pData != nullptr) && (size > 0) && (size < 10 * 1024 * 1024)); // 10 MiB should be big enough for any CTS test.

	dstBufferOffset = deAlignSize(dstBufferOffset, m_bufferOffsetAlignment);
	DE_ASSERT((dstBufferOffset + size) <= m_bufferSize);

	auto* bitstreamBasePtr = static_cast<deUint8*>(m_bitstreamBuffer->getAllocation().getHostPtr());
	deMemcpy(bitstreamBasePtr + dstBufferOffset, pData, size);
	vk::flushAlloc(m_devctx->getDeviceDriver(), m_devctx->device, m_bitstreamBuffer->getAllocation());

	return VK_SUCCESS;
}

VkDeviceSize BitstreamBufferImpl::GetMaxSize() const
{
	return m_bufferSize;
}

VkDeviceSize BitstreamBufferImpl::GetOffsetAlignment() const
{
	return m_bufferOffsetAlignment;
}

VkDeviceSize BitstreamBufferImpl::GetSizeAlignment() const
{
	// REVIEW: Cache this?
	auto reqs = getBufferMemoryRequirements(m_devctx->getDeviceDriver(), m_devctx->device, m_bitstreamBuffer->get());
	return reqs.alignment;
}

VkDeviceSize BitstreamBufferImpl::Resize(VkDeviceSize, VkDeviceSize, VkDeviceSize)
{
	TCU_THROW(InternalError, "Bitstream buffers should never need to be resized in the CTS");
}

VkDeviceSize BitstreamBufferImpl::Clone(VkDeviceSize, VkDeviceSize, VkDeviceSize,
										VkSharedBaseObj<VulkanBitstreamBuffer>&)
{
	TCU_THROW(InternalError, "Presentation only interface from the samples app should not be needed in CTS");
}


deUint8* BitstreamBufferImpl::CheckAccess(VkDeviceSize offset, VkDeviceSize size) const
{
	DE_ASSERT(size > 0 && offset + size < m_bufferSize);
	DE_UNREF(size);
	auto* bitstreamBasePtr = static_cast<deUint8*>(m_bitstreamBuffer->getAllocation().getHostPtr());
	return static_cast<deUint8*>(bitstreamBasePtr + offset);
}

int64_t BitstreamBufferImpl::MemsetData(deUint32 value, VkDeviceSize offset, VkDeviceSize size)
{
	if (size == 0)
	{
		return 0;
	}
	auto* bitstreamBasePtr = static_cast<deUint8*>(m_bitstreamBuffer->getAllocation().getHostPtr());
	deMemset(bitstreamBasePtr + offset, value, size);
	vk::flushAlloc(m_devctx->getDeviceDriver(), m_devctx->device, m_bitstreamBuffer->getAllocation());
	return size;
}

int64_t BitstreamBufferImpl::CopyDataToBuffer(deUint8* dstBuffer, VkDeviceSize dstOffset, VkDeviceSize srcOffset, VkDeviceSize size) const
{
	if (size == 0)
	{
		return 0;
	}
	auto* bitstreamBasePtr = static_cast<deUint8*>(m_bitstreamBuffer->getAllocation().getHostPtr());
	deMemcpy(dstBuffer + dstOffset, bitstreamBasePtr + srcOffset, size);
	return size;
}

int64_t BitstreamBufferImpl::CopyDataToBuffer(VkSharedBaseObj<VulkanBitstreamBuffer>& dstBuffer, VkDeviceSize dstOffset, VkDeviceSize srcOffset, VkDeviceSize size) const
{
	if (size == 0)
	{
		return 0;
	}
	const deUint8* readData = CheckAccess(srcOffset, size);
	DE_ASSERT(readData);
	return dstBuffer->CopyDataFromBuffer(readData, 0, dstOffset, size);
}

int64_t BitstreamBufferImpl::CopyDataFromBuffer(const deUint8* sourceBuffer, VkDeviceSize srcOffset, VkDeviceSize dstOffset, VkDeviceSize size)
{
	if (size == 0)
	{
		return 0;
	}
	auto* bitstreamBasePtr = static_cast<deUint8*>(m_bitstreamBuffer->getAllocation().getHostPtr());
	deMemcpy(bitstreamBasePtr + dstOffset, sourceBuffer + srcOffset, size);
	return size;
}

int64_t BitstreamBufferImpl::CopyDataFromBuffer(const VkSharedBaseObj<VulkanBitstreamBuffer>& sourceBuffer,
												VkDeviceSize								  srcOffset,
												VkDeviceSize								  dstOffset,
												VkDeviceSize								  size)
{
	if (size == 0)
	{
		return 0;
	}

	const deUint8* readData = sourceBuffer->GetReadOnlyDataPtr(srcOffset, size);
	DE_ASSERT(readData);

	auto* bitstreamBasePtr = static_cast<deUint8*>(m_bitstreamBuffer->getAllocation().getHostPtr());
	deMemcpy(bitstreamBasePtr + dstOffset, readData + srcOffset, size);
	return size;
}

deUint8* BitstreamBufferImpl::GetDataPtr(VkDeviceSize offset, VkDeviceSize& maxSize)
{
	deUint8* readData = CheckAccess(offset, 1);
	DE_ASSERT(readData);
	maxSize = m_bufferSize - offset;
	return readData;
}

const deUint8* BitstreamBufferImpl::GetReadOnlyDataPtr(VkDeviceSize offset, VkDeviceSize& maxSize) const
{
	deUint8* readData = CheckAccess(offset, 1);
	DE_ASSERT(readData);
	maxSize = m_bufferSize - offset;
	return readData;
}

void BitstreamBufferImpl::FlushRange(VkDeviceSize /*offset*/, VkDeviceSize size) const
{
	if (size == 0)
	{
		return;
	}

	// TOOD: Plumb the size and offset alignment caps into this class to invalidate just the range asked for in the API, rather than the whole range.
	vk::flushAlloc(m_devctx->getDeviceDriver(), m_devctx->device, m_bitstreamBuffer->getAllocation());
}

void BitstreamBufferImpl::InvalidateRange(VkDeviceSize /*offset*/, VkDeviceSize size) const
{
	if (size == 0)
	{
		return;
	}
	// TOOD: Plumb the size and offset alignment caps into this class to invalidate just the range asked for in the API, rather than the whole range.
	vk::flushAlloc(m_devctx->getDeviceDriver(), m_devctx->device, m_bitstreamBuffer->getAllocation());
}

deUint32 BitstreamBufferImpl::AddStreamMarker(deUint32 streamOffset)
{
	m_streamMarkers.push_back(streamOffset);
	return (deUint32)(m_streamMarkers.size() - 1);
}

deUint32 BitstreamBufferImpl::SetStreamMarker(deUint32 streamOffset, deUint32 index)
{
	DE_ASSERT(index < (deUint32)m_streamMarkers.size());
	if (!(index < (deUint32)m_streamMarkers.size()))
	{
		return deUint32(-1);
	}
	m_streamMarkers[index] = streamOffset;
	return index;
}

deUint32 BitstreamBufferImpl::GetStreamMarker(deUint32 index) const
{
	DE_ASSERT(index < (deUint32)m_streamMarkers.size());
	return m_streamMarkers[index];
}

deUint32 BitstreamBufferImpl::GetStreamMarkersCount() const
{
	return (deUint32)m_streamMarkers.size();
}

const deUint32* BitstreamBufferImpl::GetStreamMarkersPtr(deUint32 startIndex, deUint32& maxCount) const
{
	maxCount = (deUint32)m_streamMarkers.size() - startIndex;
	return m_streamMarkers.data() + startIndex;
}

deUint32 BitstreamBufferImpl::ResetStreamMarkers()
{
	deUint32 oldSize = (deUint32)m_streamMarkers.size();
	m_streamMarkers.clear();
	return oldSize;
}

} // namespace video
} // namespace vkt
