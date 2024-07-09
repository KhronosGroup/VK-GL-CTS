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

#include "vkPipelineBinaryUtil.hpp"
#include "vkQueryUtil.hpp"
#include "deSTLUtil.hpp"
#include <vector>

#ifndef CTS_USES_VULKANSC

namespace vk
{

Move<VkPipelineBinaryKHR> makeMovablePipelineBinary(const DeviceInterface &vk, const VkDevice device,
                                                    VkPipelineBinaryKHR rawPipelineBinary)
{
    return Move<VkPipelineBinaryKHR>(check<VkPipelineBinaryKHR>(rawPipelineBinary),
                                     Deleter<VkPipelineBinaryKHR>(vk, device, DE_NULL));
}

PipelineBinaryWrapper::PipelineBinaryWrapper(const DeviceInterface &vk, const VkDevice device)
    : m_vk(vk)
    , m_device(device)
{
}

VkPipelineBinaryKeyKHR PipelineBinaryWrapper::getPipelineKey(const void *pPipelineCreateInfo) const
{
    VkPipelineBinaryKeyKHR pipelineKey         = initVulkanStructure();
    VkPipelineCreateInfoKHR pipelineCreateInfo = initVulkanStructure(const_cast<void *>(pPipelineCreateInfo));

    VK_CHECK(m_vk.getPipelineKeyKHR(m_device, &pipelineCreateInfo, &pipelineKey));

    return pipelineKey;
}

VkResult PipelineBinaryWrapper::createPipelineBinariesFromPipeline(VkPipeline pipeline)
{
    VkPipelineBinaryCreateInfoKHR pipelineBinaryCreateInfo = initVulkanStructure();
    pipelineBinaryCreateInfo.pipeline                      = pipeline;

    return createPipelineBinariesFromCreateInfo(pipelineBinaryCreateInfo);
}

VkResult PipelineBinaryWrapper::createPipelineBinariesFromInternalCache(const void *pPipelineCreateInfo)
{
    VkPipelineCreateInfoKHR pipelineCreateInfo = initVulkanStructure();
    pipelineCreateInfo.pNext                   = const_cast<void *>(pPipelineCreateInfo);

    VkPipelineBinaryCreateInfoKHR pipelineBinaryCreateInfo = initVulkanStructure();
    pipelineBinaryCreateInfo.pPipelineCreateInfo           = &pipelineCreateInfo;

    return createPipelineBinariesFromCreateInfo(pipelineBinaryCreateInfo);
}

VkResult PipelineBinaryWrapper::createPipelineBinariesFromBinaryData(
    const std::vector<VkPipelineBinaryDataKHR> &pipelineDataInfo)
{
    // for graphics pipeline libraries not all pipeline stages have to have binaries
    std::size_t keyCount = m_binaryKeys.size();
    if (keyCount == 0)
        return VK_SUCCESS;

    VkPipelineBinaryKeysAndDataKHR binaryKeysAndData{
        (uint32_t)keyCount,     // uint32_t binaryCount;
        m_binaryKeys.data(),    // const VkPipelineBinaryKeyKHR* pPipelineBinaryKeys;
        pipelineDataInfo.data() // const VkPipelineBinaryDataKHR* pPipelineBinaryData;
    };
    VkPipelineBinaryCreateInfoKHR pipelineBinaryCreateInfo = initVulkanStructure();
    pipelineBinaryCreateInfo.pKeysAndDataInfo              = &binaryKeysAndData;

    return createPipelineBinariesFromCreateInfo(pipelineBinaryCreateInfo);
}

VkResult PipelineBinaryWrapper::createPipelineBinariesFromCreateInfo(const VkPipelineBinaryCreateInfoKHR &createInfos)
{
    // check how many binaries will be created
    VkPipelineBinaryHandlesInfoKHR binaryHandlesInfo = initVulkanStructure();
    VkResult result = m_vk.createPipelineBinariesKHR(m_device, &createInfos, NULL, &binaryHandlesInfo);
    if (result != VK_SUCCESS)
        return result;

    // create pipeline binary objects
    std::size_t binaryCount = binaryHandlesInfo.pipelineBinaryCount;
    m_binariesRaw.resize(binaryCount);
    binaryHandlesInfo.pPipelineBinaries = m_binariesRaw.data();
    result = m_vk.createPipelineBinariesKHR(m_device, &createInfos, DE_NULL, &binaryHandlesInfo);
    if (result != VK_SUCCESS)
        return result;

    // wrap pipeline binaries to movable references to avoid leaks
    m_binaries.resize(binaryCount);
    for (std::size_t i = 0; i < binaryCount; ++i)
        m_binaries[i] = makeMovablePipelineBinary(m_vk, m_device, m_binariesRaw[i]);

    return result;
}

void PipelineBinaryWrapper::getPipelineBinaryData(std::vector<VkPipelineBinaryDataKHR> &pipelineDataInfo,
                                                  std::vector<std::vector<uint8_t>> &pipelineDataBlob)
{
    // for graphics pipeline libraries not all pipeline stages have to have binaries
    const std::size_t binaryCount = m_binariesRaw.size();
    if (binaryCount == 0)
        return;

    m_binaryKeys.resize(binaryCount);
    pipelineDataInfo.resize(binaryCount);
    pipelineDataBlob.resize(binaryCount);

    for (std::size_t i = 0; i < binaryCount; ++i)
    {
        VkPipelineBinaryDataInfoKHR binaryInfo = initVulkanStructure();
        binaryInfo.pipelineBinary              = m_binariesRaw[i];

        // get binary key and data size
        size_t binaryDataSize = 0;
        m_binaryKeys[i]       = initVulkanStructure();
        VK_CHECK(m_vk.getPipelineBinaryDataKHR(m_device, &binaryInfo, &m_binaryKeys[i], &binaryDataSize, NULL));
        DE_ASSERT(binaryDataSize > 0);

        pipelineDataInfo[i].dataSize = binaryDataSize;
        pipelineDataBlob[i].resize(binaryDataSize);
        pipelineDataInfo[i].pData = pipelineDataBlob[i].data();

        // get binary data
        VK_CHECK(m_vk.getPipelineBinaryDataKHR(m_device, &binaryInfo, &m_binaryKeys[i], &binaryDataSize,
                                               pipelineDataBlob[i].data()));
    }
}

void PipelineBinaryWrapper::deletePipelineBinariesAndKeys(void)
{
    m_binaryKeys.clear();
    m_binaries.clear();
    m_binariesRaw.clear();
}

void PipelineBinaryWrapper::deletePipelineBinariesKeepKeys(void)
{
    m_binaries.clear();
    m_binariesRaw.clear();
}

VkPipelineBinaryInfoKHR PipelineBinaryWrapper::preparePipelineBinaryInfo(void) const
{
    const std::size_t binaryCount = m_binariesRaw.size();

    // VUID-VkPipelineBinaryInfoKHR-binaryCount-arraylength
    // binaryCount must be greater than 0
    DE_ASSERT(binaryCount > 0);

    return {
        VK_STRUCTURE_TYPE_PIPELINE_BINARY_INFO_KHR, DE_NULL,
        static_cast<uint32_t>(binaryCount), // uint32_t binaryCount;
        de::dataOrNull(m_binariesRaw)       // const VkPipelineBinaryKHR* pPipelineBinaries;
    };
}

uint32_t PipelineBinaryWrapper::getKeyCount() const
{
    return static_cast<uint32_t>(m_binaryKeys.size());
}

uint32_t PipelineBinaryWrapper::getBinariesCount() const
{
    return static_cast<uint32_t>(m_binariesRaw.size());
}

const VkPipelineBinaryKeyKHR *PipelineBinaryWrapper::getBinaryKeys() const
{
    return m_binaryKeys.data();
}

const VkPipelineBinaryKHR *PipelineBinaryWrapper::getPipelineBinaries() const
{
    return m_binariesRaw.data();
}

} // namespace vk

#endif
