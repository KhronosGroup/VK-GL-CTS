#ifndef _VKTENSORUTIL_HPP
#define _VKTENSORUTIL_HPP
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

#include "vkDefs.hpp"

#include "vkTensorWithMemory.hpp"
#include "vkTensorMemoryUtil.hpp"
#include <cstdint>

namespace vk
{

#ifndef CTS_USES_VULKANSC

// Upload data from host memory to tensor memory.
// If tensor memory is host visible, copy to it directly.
// If tensor memory is not host visible, copy to it through a staging buffer.
void uploadToTensor(const DeviceInterface &vk, const VkDevice device, vk::Allocator &allocator, const VkQueue queue,
                    const uint32_t queueFamilyIndex, const TensorWithMemory &tensor, const void *const hostBuffer,
                    uint64_t hostBufferSize, bool forceStaging = false);

// Download data from tensor memory to host memory.
// If tensor memory is host visible, copy from it directly.
// If tensor memory is not host visible, copy from it through a staging buffer.
void downloadFromTensor(const DeviceInterface &vk, const VkDevice device, vk::Allocator &allocator, const VkQueue queue,
                        const uint32_t queueFamilyIndex, const vk::TensorWithMemory &tensor, void *const hostBuffer,
                        uint64_t hostBufferSize, bool forceStaging = false);

// Clear tensor memory to all zeroes.
// If tensor memory is host visible, memset it directly.
// If tensor memory is not host visible, memset a host visible staging buffer and copy from it.
void clearTensor(const DeviceInterface &vk, const VkDevice device, vk::Allocator &allocator, const VkQueue queue,
                 const uint32_t queueFamilyIndex, const TensorWithMemory &tensor, bool forceStaging = false);

#endif // CTS_USES_VULKANSC

} // namespace vk
#endif // _VKTENSORUTIL_HPP
