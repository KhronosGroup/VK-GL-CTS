#ifndef _VKPIPELINEBINARYUTIL_HPP
#define _VKPIPELINEBINARYUTIL_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024 The Khronos Group Inc.
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
 * \brief Utilities for pipeline binaries.
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include <vector>
#include <limits>

#ifndef CTS_USES_VULKANSC

namespace vk
{

class PipelineBinaryWrapper
{
public:
    PipelineBinaryWrapper(const DeviceInterface &vk, const VkDevice vkDevice);

    // Generate unique key for whole pipeline basing on create info. Note: pNext chain from PipelineCreateInfo must be available
    VkPipelineBinaryKeyKHR getPipelineKey(const void *pPipelineCreateInfo) const;

    VkResult createPipelineBinariesFromPipeline(VkPipeline pipeline);
    VkResult createPipelineBinariesFromInternalCache(const void *pPipelineCreateInfo);
    VkResult createPipelineBinariesFromBinaryData(const std::vector<VkPipelineBinaryDataKHR> &pipelineDataInfo);
    VkResult createPipelineBinariesFromCreateInfo(const VkPipelineBinaryCreateInfoKHR &createInfos);

    void getPipelineBinaryData(std::vector<VkPipelineBinaryDataKHR> &pipelineDataInfo,
                               std::vector<std::vector<uint8_t>> &pipelineDataBlob);

    void deletePipelineBinariesAndKeys(void);
    void deletePipelineBinariesKeepKeys(void);

    VkPipelineBinaryInfoKHR preparePipelineBinaryInfo(void) const;

    uint32_t getKeyCount(void) const;
    uint32_t getBinariesCount(void) const;

    const VkPipelineBinaryKeyKHR *getBinaryKeys(void) const;
    const VkPipelineBinaryKHR *getPipelineBinaries(void) const;

protected:
    const DeviceInterface &m_vk;
    const VkDevice m_device;

    std::vector<VkPipelineBinaryKeyKHR> m_binaryKeys;
    std::vector<Move<VkPipelineBinaryKHR>> m_binaries;
    std::vector<VkPipelineBinaryKHR> m_binariesRaw;
};

} // namespace vk

#endif // CTS_USES_VULKANSC

#endif // _VKPIPELINEBINARYUTIL_HPP
