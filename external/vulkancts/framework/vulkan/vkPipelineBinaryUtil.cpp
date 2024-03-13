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

#ifndef CTS_USES_VULKANSC

namespace vk
{

Move<VkPipelineBinaryKHR> makeMovablePipelineBinary (const DeviceInterface& vk, const VkDevice device, VkPipelineBinaryKHR rawPipelineBinary)
{
	return Move<VkPipelineBinaryKHR>(check<VkPipelineBinaryKHR>(rawPipelineBinary), Deleter<VkPipelineBinaryKHR>(vk, device, DE_NULL));
}

PipelineBinaryWrapper::PipelineBinaryWrapper (const DeviceInterface&	vk,
											  const VkDevice			device)
	: m_vk		(vk)
	, m_device	(device)
{
}

void PipelineBinaryWrapper::getPipelineBinaryKeys (const void* pPipelineCreateInfo, bool clearPrevious)
{
	// retrieve pipeline key count
	deUint32 keyCount = 0;
	VkPipelineBinaryKeysCreateInfoKHR pipelineBinaryKeysCreateInfo = initVulkanStructure(const_cast<void*>(pPipelineCreateInfo));
	VK_CHECK(m_vk.getPipelineBinaryKeysKHR(m_device, &pipelineBinaryKeysCreateInfo, &keyCount, DE_NULL));
	if (keyCount == 0)
		TCU_FAIL("Expected number of binary keys to be greater than 0");

	if (clearPrevious)
		m_pipelineKeys.clear();

	VkPipelineBinaryKeyKHR defaultKey;
	defaultKey.sType = VK_STRUCTURE_TYPE_PIPELINE_BINARY_KEY_KHR;
	defaultKey.pNext = nullptr;

	// if there were keys inserted then make room for more
	std::size_t previousSize = m_pipelineKeys.size();
	m_pipelineKeys.resize(previousSize + keyCount, defaultKey);

	// retrieve pipeline keys
	VK_CHECK(m_vk.getPipelineBinaryKeysKHR(m_device, &pipelineBinaryKeysCreateInfo, &keyCount, m_pipelineKeys.data() + previousSize));
}

void PipelineBinaryWrapper::createPipelineBinariesFromPipeline (VkPipeline pipeline)
{
	const std::size_t keyCount = m_pipelineKeys.size();

	VkPipelineBinaryCreateInfoKHR defaultPipelineBinaryCreateInfo = initVulkanStructure();
	defaultPipelineBinaryCreateInfo.pipeline = pipeline;

	// prepare pipeline binary info structures
	std::vector<VkPipelineBinaryCreateInfoKHR> createInfos(keyCount, defaultPipelineBinaryCreateInfo);
	for (std::size_t i = 0; i < keyCount; ++i)
		createInfos[i].pKey = &m_pipelineKeys[i];

	createPipelineBinariesFromCreateInfo(createInfos);
}

void PipelineBinaryWrapper::createPipelineBinariesFromBinaryData (const std::vector<VkPipelineBinaryDataKHR>& pipelineDataInfo)
{
	const std::size_t keyCount = m_pipelineKeys.size();

	// create binaries from data blobs
	VkPipelineBinaryCreateInfoKHR defaultPipelineBinaryCreateInfo = initVulkanStructure();
	std::vector<VkPipelineBinaryCreateInfoKHR> createInfos(keyCount, defaultPipelineBinaryCreateInfo);
	for (std::size_t i = 0; i < keyCount; ++i)
	{
		createInfos[i].pKey = &m_pipelineKeys[i];
		createInfos[i].pDataInfo = &pipelineDataInfo[i];
	}

	createPipelineBinariesFromCreateInfo(createInfos);
}

void PipelineBinaryWrapper::createPipelineBinariesFromCreateInfo (const std::vector<VkPipelineBinaryCreateInfoKHR>& createInfos)
{
	const std::size_t keyCount = m_pipelineKeys.size();

	// create pipeline binary objects
	m_pipelineBinariesRaw.resize(keyCount);
	VK_CHECK(m_vk.createPipelineBinariesKHR(m_device, (deUint32)keyCount, createInfos.data(), DE_NULL, m_pipelineBinariesRaw.data()));

	// wrap pipeline binaries to movable references to avoid leaks
	m_pipelineBinaries.resize(keyCount);
	for (std::size_t i = 0; i < keyCount; ++i)
		m_pipelineBinaries[i] = makeMovablePipelineBinary(m_vk, m_device, m_pipelineBinariesRaw[i]);
}

void PipelineBinaryWrapper::getPipelineBinaryData (std::vector<VkPipelineBinaryDataKHR>&	pipelineDataInfo,
												   std::vector<std::vector<uint8_t> >&		pipelineDataBlob)
{
	const std::size_t keyCount = m_pipelineKeys.size();
	pipelineDataInfo.resize(keyCount);
	pipelineDataBlob.resize(keyCount);

	for (std::size_t i = 0; i < keyCount; ++i)
	{
		VkPipelineBinaryDataInfoKHR pipelineBinaryDataInfo = initVulkanStructure();
		pipelineBinaryDataInfo.pipelineBinary = m_pipelineBinariesRaw[i];

		// get binary data size
		VK_CHECK(m_vk.getPipelineBinaryDataKHR(m_device, &pipelineBinaryDataInfo, &pipelineDataInfo[i].dataSize, DE_NULL));

		// alocate space for data and store pointer for it
		pipelineDataBlob[i].resize(pipelineDataInfo[i].dataSize);
		pipelineDataInfo[i].pData = pipelineDataBlob[i].data();

		// get binary data
		VK_CHECK(m_vk.getPipelineBinaryDataKHR(m_device, &pipelineBinaryDataInfo, &pipelineDataInfo[i].dataSize, pipelineDataInfo[i].pData));
	}
}

void PipelineBinaryWrapper::deletePipelineBinariesAndKeys(void)
{
	m_pipelineKeys.clear();
	m_pipelineBinaries.clear();
	m_pipelineBinariesRaw.clear();
}

void PipelineBinaryWrapper::deletePipelineBinariesKeepKeys(void)
{
	m_pipelineBinaries.clear();
	m_pipelineBinariesRaw.clear();
}

VkPipelineBinaryInfoKHR PipelineBinaryWrapper::preparePipelineBinaryInfo (deUint32 binaryIndex, deUint32 binaryCount) const
{
	binaryCount = std::min(binaryCount, static_cast<deUint32>(m_pipelineKeys.size()));
	return
	{
		VK_STRUCTURE_TYPE_PIPELINE_BINARY_INFO_KHR,
		DE_NULL,
		binaryCount,										// uint32_t							binaryCount;
		m_pipelineKeys.data() + binaryIndex,				// const VkPipelineBinaryKeyKHR*	pPipelineBinaryKeys;
		m_pipelineBinariesRaw.data() + binaryIndex			// const VkPipelineBinaryKHR*		pPipelineBinaries;
	};
}

deUint32 PipelineBinaryWrapper::getKeyCount() const
{
	return static_cast<deUint32>(m_pipelineKeys.size());
}

deUint32 PipelineBinaryWrapper::getBinariesCount() const
{
	return static_cast<deUint32>(m_pipelineBinariesRaw.size());
}

const VkPipelineBinaryKeyKHR* PipelineBinaryWrapper::getPipelineKeys() const
{
	return m_pipelineKeys.data();
}

const VkPipelineBinaryKHR* PipelineBinaryWrapper::getPipelineBinaries() const
{
	return m_pipelineBinariesRaw.data();
}

} // vk

#endif
