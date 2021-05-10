/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
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
 * \brief Defines class for handling resources ( programs, pipelines, files, etc. )
 *//*--------------------------------------------------------------------*/

#include "vkResourceInterface.hpp"
#include "vkQueryUtil.hpp"

#ifdef CTS_USES_VULKANSC
	#include <functional>
	#include <fstream>
	#include "vkSafetyCriticalUtil.hpp"
	#include "vkRefUtil.hpp"
	#include "tcuCommandLine.hpp"
	#include "vksCacheBuilder.hpp"
	#include "vksSerializer.hpp"
	using namespace vksc_server::json;
#endif // CTS_USES_VULKANSC

namespace vk
{

ResourceInterface::ResourceInterface (tcu::TestContext& testCtx)
	: m_testCtx			(testCtx)
#ifdef CTS_USES_VULKANSC
	, m_commandPoolIndex(0u)
	, m_resourceCounter	(0u)
	, m_statCurrent		(resetDeviceObjectReservationCreateInfo())
	, m_statMax			(resetDeviceObjectReservationCreateInfo())
#endif // CTS_USES_VULKANSC
{
}

ResourceInterface::~ResourceInterface ()
{
}

void ResourceInterface::initTestCase (const std::string& casePath)
{
	m_currentTestPath = casePath;
}

#ifdef CTS_USES_VULKANSC
deUint64 ResourceInterface::incResourceCounter ()
{
	return ++m_resourceCounter;
}

std::mutex& ResourceInterface::getStatMutex ()
{
	return m_mutex;
}

VkDeviceObjectReservationCreateInfo& ResourceInterface::getStatCurrent ()
{
	return m_statCurrent;
}

VkDeviceObjectReservationCreateInfo&	ResourceInterface::getStatMax ()
{
	return m_statMax;
}

const VkDeviceObjectReservationCreateInfo&	ResourceInterface::getStatMax () const
{
	return m_statMax;
}


void ResourceInterface::removeRedundantObjects ()
{
	// At the end of the day we only need to export objects used in pipelines.
	// Rest of the objects may be removed from m_json* structures as redundant
	std::set<VkSamplerYcbcrConversion>	samplerYcbcrConversionsInPipeline;
	std::set<VkSampler>					samplersInPipeline;
	std::set<VkShaderModule>			shadersInPipeline;
	std::set<VkRenderPass>				renderPassesInPipeline;
	std::set<VkPipelineLayout>			pipelineLayoutsInPipeline;
	std::set<VkDescriptorSetLayout>		descriptorSetLayoutsInPipeline;

	Context jsonReader;

	for (auto it = begin(m_pipelineInput.pipelines); it != end(m_pipelineInput.pipelines); ++it)
	{
		if (it->pipelineContents.find("VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO") != std::string::npos)
		{
			VkGraphicsPipelineCreateInfo	gpCI;
			deMemset(&gpCI, 0, sizeof(gpCI));
			readJSON_VkGraphicsPipelineCreateInfo(jsonReader, it->pipelineContents, gpCI);

			for (deUint32 i = 0; i < gpCI.stageCount; ++i)
				shadersInPipeline.insert(gpCI.pStages[i].module);
			renderPassesInPipeline.insert(gpCI.renderPass);
			pipelineLayoutsInPipeline.insert(gpCI.layout);
		}
		else if (it->pipelineContents.find("VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO") != std::string::npos)
		{
			VkComputePipelineCreateInfo	cpCI;
			deMemset(&cpCI, 0, sizeof(cpCI));
			readJSON_VkComputePipelineCreateInfo(jsonReader, it->pipelineContents, cpCI);

			shadersInPipeline.insert(cpCI.stage.module);
			pipelineLayoutsInPipeline.insert(cpCI.layout);
		}
		else
			TCU_THROW(InternalError, "Could not recognize pipeline type");
	}
	for (auto it = begin(m_pipelineInput.shaderModules); it != end(m_pipelineInput.shaderModules); )
	{
		if (shadersInPipeline.find(it->first) == end(shadersInPipeline))
			it = m_pipelineInput.shaderModules.erase(it);
		else
			++it;
	}
	for (auto it = begin(m_pipelineInput.renderPasses); it != end(m_pipelineInput.renderPasses); )
	{
		if (renderPassesInPipeline.find(it->first) == end(renderPassesInPipeline))
			it = m_pipelineInput.renderPasses.erase(it);
		else
			++it;
	}
	for (auto it = begin(m_pipelineInput.pipelineLayouts); it != end(m_pipelineInput.pipelineLayouts); )
	{
		if (pipelineLayoutsInPipeline.find(it->first) == end(pipelineLayoutsInPipeline))
		{
			it = m_pipelineInput.pipelineLayouts.erase(it);
		}
		else
		{
			VkPipelineLayoutCreateInfo	plCI;
			deMemset(&plCI, 0, sizeof(plCI));
			readJSON_VkPipelineLayoutCreateInfo(jsonReader, it->second, plCI);
			for (deUint32 i = 0; i < plCI.setLayoutCount; ++i)
				descriptorSetLayoutsInPipeline.insert(plCI.pSetLayouts[i]);
			++it;
		}
	}
	for (auto it = begin(m_pipelineInput.descriptorSetLayouts); it != end(m_pipelineInput.descriptorSetLayouts); )
	{
		if (descriptorSetLayoutsInPipeline.find(it->first) == end(descriptorSetLayoutsInPipeline))
			it = m_pipelineInput.descriptorSetLayouts.erase(it);
		else
		{
			VkDescriptorSetLayoutCreateInfo	dsCI;
			deMemset(&dsCI, 0, sizeof(dsCI));
			readJSON_VkDescriptorSetLayoutCreateInfo(jsonReader, it->second, dsCI);

			for (deUint32 i = 0; i < dsCI.bindingCount; ++i)
			{
				if (dsCI.pBindings[i].pImmutableSamplers == NULL)
					continue;
				for (deUint32 j = 0; j < dsCI.pBindings[i].descriptorCount; ++j)
				{
					if (dsCI.pBindings[i].pImmutableSamplers[j] == DE_NULL)
						continue;
					samplersInPipeline.insert(dsCI.pBindings[i].pImmutableSamplers[j]);
				}
			}
			++it;
		}
	}

	for (auto it = begin(m_pipelineInput.samplers); it != end(m_pipelineInput.samplers); )
	{
		if (samplersInPipeline.find(it->first) == end(samplersInPipeline))
			it = m_pipelineInput.samplers.erase(it);
		else
		{
			VkSamplerCreateInfo	sCI;
			deMemset(&sCI, 0, sizeof(sCI));
			readJSON_VkSamplerCreateInfo(jsonReader, it->second, sCI);

			if (sCI.pNext != DE_NULL)
			{
				VkSamplerYcbcrConversionInfo* info = (VkSamplerYcbcrConversionInfo*)(sCI.pNext);
				if (info->sType == VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO)
					samplerYcbcrConversionsInPipeline.insert(info->conversion);
			}
			++it;
		}
	}
	for (auto it = begin(m_pipelineInput.samplerYcbcrConversions); it != end(m_pipelineInput.samplerYcbcrConversions); )
	{
		if (samplerYcbcrConversionsInPipeline.find(it->first) == end(samplerYcbcrConversionsInPipeline))
			it = m_pipelineInput.samplerYcbcrConversions.erase(it);
		else
			++it;
	}
}

void ResourceInterface::finalizeCommandBuffers ()
{
	// We have information about command buffer sizes
	// Now we have to convert it into command pool sizes
	std::map<deUint64, std::size_t> cpToIndex;
	for (std::size_t i = 0; i < m_commandPoolMemoryConsumption.size(); ++i)
		cpToIndex.insert({ m_commandPoolMemoryConsumption[i].commandPool, i });
	for (const auto& memC : m_commandBufferMemoryConsumption)
	{
		std::size_t j = cpToIndex[memC.second.commandPool];
		m_commandPoolMemoryConsumption[j].updateValues
		(
			memC.second.commandPoolAllocated,
			memC.second.commandPoolReservedSize,
			memC.second.commandBufferAllocated
		);
		m_commandPoolMemoryConsumption[j].commandBufferCount++;
	}
}

std::vector<deUint8> ResourceInterface::exportData () const
{
	vksc_server::VulkanDataTransmittedFromMainToSubprocess vdtfmtsp(m_pipelineInput, m_statMax, m_commandPoolMemoryConsumption, m_pipelineSizes);

	return vksc_server::Serialize(vdtfmtsp);
}

void ResourceInterface::importData (std::vector<deUint8>& importText) const
{
	vksc_server::VulkanDataTransmittedFromMainToSubprocess vdtfmtsp = vksc_server::Deserialize<vksc_server::VulkanDataTransmittedFromMainToSubprocess>(importText);

	m_pipelineInput					= vdtfmtsp.pipelineCacheInput;
	m_statMax						= vdtfmtsp.memoryReservation;
	m_commandPoolMemoryConsumption	= vdtfmtsp.commandPoolMemoryConsumption;
	m_pipelineSizes					= vdtfmtsp.pipelineSizes;
}

void ResourceInterface::registerObjectHash (deUint64 handle, std::size_t hashValue) const
{
	m_objectHashes[handle] =  hashValue;
}

const std::map<deUint64,std::size_t>& ResourceInterface::getObjectHashes () const
{
	return m_objectHashes;
}

std::vector<VkPipelinePoolSize> ResourceInterface::getPipelinePoolSizes() const
{
	std::vector<VkPipelinePoolSize> result;
	for (const auto& pipDesc : m_pipelineSizes)
	{
		VkPipelinePoolSize poolSize =
		{
			VK_STRUCTURE_TYPE_PIPELINE_POOL_SIZE,	// VkStructureType	sType;
			DE_NULL,								// const void*		pNext;
			pipDesc.size,							// VkDeviceSize		poolEntrySize;
			pipDesc.count							// deUint32			poolEntryCount;
		};
		result.emplace_back(poolSize);
	}
	return result;
}

vksc_server::VulkanCommandMemoryConsumption ResourceInterface::getNextCommandPoolSize ()
{
	if (m_commandPoolMemoryConsumption.empty())
		return vksc_server::VulkanCommandMemoryConsumption();

	vksc_server::VulkanCommandMemoryConsumption result	= m_commandPoolMemoryConsumption[m_commandPoolIndex];
	// modulo operation is just a safeguard against excessive number of requests
	m_commandPoolIndex									= (m_commandPoolIndex + 1) % deUint32(m_commandPoolMemoryConsumption.size());
	return result;
}

std::size_t	ResourceInterface::getCacheDataSize () const
{
	return m_cacheData.size();
}

const deUint8* ResourceInterface::getCacheData () const
{
	return m_cacheData.data();
}

#endif // CTS_USES_VULKANSC

ResourceInterfaceStandard::ResourceInterfaceStandard (tcu::TestContext& testCtx)
	: ResourceInterface(testCtx)
{
}

void ResourceInterfaceStandard::initDevice (DeviceInterface& deviceInterface, VkDevice device)
{
	// ResourceInterfaceStandard is a class for running VulkanSC tests on normal Vulkan driver.
	// CTS does not have vkCreateShaderModule function defined for Vulkan SC driver, but we need this function
	// So ResourceInterfaceStandard class must have its own vkCreateShaderModule function pointer
	// Moreover - we create additional function pointers for vkCreateGraphicsPipelines, vkCreateComputePipelines, etc.
	// BTW: although ResourceInterfaceStandard exists in normal Vulkan tests - only initDevice and buildProgram functions are used by Vulkan tests
	// Other functions are called from within DeviceDriverSC which does not exist in these tests ( DeviceDriver class is used instead )
	m_createShaderModuleFunc[device]				= (CreateShaderModuleFunc)				deviceInterface.getDeviceProcAddr(device, "vkCreateShaderModule");
	m_createGraphicsPipelinesFunc[device]			= (CreateGraphicsPipelinesFunc)			deviceInterface.getDeviceProcAddr(device, "vkCreateGraphicsPipelines");
	m_createComputePipelinesFunc[device]			= (CreateComputePipelinesFunc)			deviceInterface.getDeviceProcAddr(device, "vkCreateComputePipelines");
#ifdef CTS_USES_VULKANSC
	if (m_testCtx.getCommandLine().isSubProcess())
	{
		VkPipelineCacheCreateInfo pCreateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,				// VkStructureType				sType;
			DE_NULL,													// const void*					pNext;
			VK_PIPELINE_CACHE_CREATE_READ_ONLY_BIT |
				VK_PIPELINE_CACHE_CREATE_USE_APPLICATION_STORAGE_BIT,	// VkPipelineCacheCreateFlags	flags;
			m_cacheData.size(),											// deUintptr					initialDataSize;
			m_cacheData.data()											// const void*					pInitialData;
		};
		m_pipelineCache[device] = de::SharedPtr<Move<VkPipelineCache>>(new Move<VkPipelineCache>(createPipelineCache(deviceInterface, device, &pCreateInfo, DE_NULL)));
	}
#endif // CTS_USES_VULKANSC
}

void ResourceInterfaceStandard::deinitDevice (VkDevice device)
{
#ifdef CTS_USES_VULKANSC
	if (m_testCtx.getCommandLine().isSubProcess())
	{
		m_pipelineCache.erase(device);
	}
#else
	DE_UNREF(device);
#endif // CTS_USES_VULKANSC
}

#ifdef CTS_USES_VULKANSC

void ResourceInterfaceStandard::registerDeviceFeatures (VkDevice							device,
														const VkDeviceCreateInfo*			pCreateInfo) const
{
	VkPhysicalDeviceFeatures2* chainedFeatures		= (VkPhysicalDeviceFeatures2*)findStructureInChain(pCreateInfo->pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
	if (chainedFeatures != NULL)
	{
		m_deviceFeatures[device]					= writeJSON_pNextChain(pCreateInfo->pNext);
	}
	else
	{
		VkPhysicalDeviceFeatures2 deviceFeatures2	= initVulkanStructure();
		if (pCreateInfo->pEnabledFeatures != NULL)
			deviceFeatures2.features = *(pCreateInfo->pEnabledFeatures);

		deviceFeatures2.pNext						= (void *)pCreateInfo->pNext;
		m_deviceFeatures[device]					= writeJSON_VkPhysicalDeviceFeatures2(deviceFeatures2);
	}

	std::vector<std::string> extensions;
	for (deUint32 i = 0; i < pCreateInfo->enabledExtensionCount; ++i)
		extensions.push_back(pCreateInfo->ppEnabledExtensionNames[i]);
	m_deviceExtensions[device]					= extensions;
}

void ResourceInterfaceStandard::unregisterDeviceFeatures(VkDevice							device) const
{
	m_deviceFeatures.erase(device);
	m_deviceExtensions.erase(device);
}

VkResult ResourceInterfaceStandard::createShaderModule (VkDevice							device,
														const VkShaderModuleCreateInfo*		pCreateInfo,
														const VkAllocationCallbacks*		pAllocator,
														VkShaderModule*						pShaderModule,
														bool								normalMode) const
{
	if (normalMode)
	{
		const auto it = m_createShaderModuleFunc.find(device);
		if (it != end(m_createShaderModuleFunc))
		{
			VkResult result = it->second(device, pCreateInfo, pAllocator, pShaderModule);
			registerObjectHash(pShaderModule->getInternal(), calculateShaderModuleHash(*pCreateInfo, getObjectHashes()));
			return result;
		}
		TCU_THROW(InternalError, "vkCreateShaderModule not defined");
	}

	// main process: store VkShaderModuleCreateInfo in JSON format. Shaders will be sent later for m_pipelineCache creation ( and sent through file to another process )
	*pShaderModule = VkShaderModule(++m_resourceCounter);
	registerObjectHash(pShaderModule->getInternal(), calculateShaderModuleHash(*pCreateInfo, getObjectHashes()));
	m_pipelineInput.shaderModules.insert({ *pShaderModule, writeJSON_VkShaderModuleCreateInfo(*pCreateInfo) });
	return VK_SUCCESS;
}

VkPipelineIdentifierInfo makeGraphicsPipelineIdentifier (const std::string& testPath, const VkGraphicsPipelineCreateInfo& gpCI, const std::map<deUint64, std::size_t>& objectHashes)
{
	DE_UNREF(testPath);
	VkPipelineIdentifierInfo	pipelineID		= resetPipelineIdentifierInfo();
	std::size_t					hashValue		= calculateGraphicsPipelineHash(gpCI, objectHashes);
//	hash_combine(hashValue, testPath);
	memcpy(pipelineID.pipelineIdentifier, &hashValue, sizeof(std::size_t));
	return pipelineID;
}

VkPipelineIdentifierInfo makeComputePipelineIdentifier (const std::string& testPath, const VkComputePipelineCreateInfo& cpCI, const std::map<deUint64, std::size_t>& objectHashes)
{
	DE_UNREF(testPath);
	VkPipelineIdentifierInfo	pipelineID		= resetPipelineIdentifierInfo();
	std::size_t					hashValue		= calculateComputePipelineHash(cpCI, objectHashes);
//	hash_combine(hashValue, testPath);
	memcpy(pipelineID.pipelineIdentifier, &hashValue, sizeof(std::size_t));
	return pipelineID;
}

VkResult ResourceInterfaceStandard::createGraphicsPipelines (VkDevice								device,
															 VkPipelineCache						pipelineCache,
															 deUint32								createInfoCount,
															 const VkGraphicsPipelineCreateInfo*	pCreateInfos,
															 const VkAllocationCallbacks*			pAllocator,
															 VkPipeline*							pPipelines,
															 bool									normalMode) const
{
	DE_UNREF(pipelineCache);

	// build pipeline identifiers, make a copy of pCreateInfos
	std::vector<VkPipelineIdentifierInfo>		pipelineIDs;
	std::vector<VkGraphicsPipelineCreateInfo>	pCreateInfoCopies;
	for (deUint32 i = 0; i < createInfoCount; ++i)
	{
		pipelineIDs.push_back(makeGraphicsPipelineIdentifier(m_currentTestPath, pCreateInfos[i], getObjectHashes()));
		pCreateInfoCopies.push_back(pCreateInfos[i]);
	}

	// include pipelineIdentifiers into pNext chain of pCreateInfoCopies
	for (deUint32 i = 0; i < createInfoCount; ++i)
	{
		pipelineIDs[i].pNext		= pCreateInfoCopies[i].pNext;
		pCreateInfoCopies[i].pNext	= &pipelineIDs[i];
	}

	// subprocess: load graphics pipelines from OUR m_pipelineCache cache
	if (normalMode)
	{
		const auto it = m_createGraphicsPipelinesFunc.find(device);
		if (it != end(m_createGraphicsPipelinesFunc))
		{
			auto pit = m_pipelineCache.find(device);
			if ( pit != end(m_pipelineCache) )
			{
				VkPipelineCache pCache = pit->second->get();
				return it->second(device, pCache, createInfoCount, pCreateInfoCopies.data(), pAllocator, pPipelines);
			}
			TCU_THROW(InternalError, "m_pipelineCache not initialized for this device");
		}
		TCU_THROW(InternalError, "vkCreateGraphicsPipelines not defined");
	}

	// main process: store pipelines in JSON format. Pipelines will be sent later for m_pipelineCache creation ( and sent through file to another process )
	for (deUint32 i = 0; i < createInfoCount; ++i)
	{
		auto it = std::find_if(begin(m_pipelineInput.pipelines), end(m_pipelineInput.pipelines), vksc_server::PipelineIdentifierEqual(pipelineIDs[i]));
		pipelineIDs[i].pNext = DE_NULL;
		if (it == end(m_pipelineInput.pipelines))
		{
			const auto& featIt = m_deviceFeatures.find(device);
			if(featIt == end(m_deviceFeatures))
				TCU_THROW(InternalError, "Can't find device features for this pipeline");
			const auto& extIt = m_deviceExtensions.find(device);
			if (extIt == end(m_deviceExtensions))
				TCU_THROW(InternalError, "Can't find device extensions for this pipeline");

			m_pipelineInput.pipelines.push_back(vksc_server::VulkanJsonPipelineDescription(
				pipelineIDs[i],
				writeJSON_VkGraphicsPipelineCreateInfo(pCreateInfoCopies[i]),
				featIt->second,
				extIt->second,
				m_currentTestPath));
		}
		else
			it->add(m_currentTestPath);
	}
	return VK_SUCCESS;
}

VkResult ResourceInterfaceStandard::createComputePipelines (VkDevice								device,
															VkPipelineCache							pipelineCache,
															deUint32								createInfoCount,
															const VkComputePipelineCreateInfo*		pCreateInfos,
															const VkAllocationCallbacks*			pAllocator,
															VkPipeline*								pPipelines,
															bool									normalMode) const
{
	DE_UNREF(pipelineCache);

	// build pipeline identifiers, make a copy of pCreateInfos
	std::vector<VkPipelineIdentifierInfo>		pipelineIDs;
	std::vector<VkComputePipelineCreateInfo>	pCreateInfoCopies;
	for (deUint32 i = 0; i < createInfoCount; ++i)
	{
		pipelineIDs.push_back(makeComputePipelineIdentifier(m_currentTestPath, pCreateInfos[i], getObjectHashes()));
		pCreateInfoCopies.push_back(pCreateInfos[i]);
	}

	// include pipelineIdentifiers into pNext chain of pCreateInfoCopies
	for (deUint32 i = 0; i < createInfoCount; ++i)
	{
		pipelineIDs[i].pNext = pCreateInfoCopies[i].pNext;
		pCreateInfoCopies[i].pNext = &pipelineIDs[i];
	}

	// subprocess: load graphics pipelines from OUR pipeline cache
	if (normalMode)
	{
		const auto it = m_createComputePipelinesFunc.find(device);
		if (it != end(m_createComputePipelinesFunc))
		{
			auto pit = m_pipelineCache.find(device);
			if ( pit != end(m_pipelineCache) )
			{
				VkPipelineCache pCache = pit->second->get();
				return it->second(device, pCache, createInfoCount, pCreateInfoCopies.data(), pAllocator, pPipelines);
			}
			TCU_THROW(InternalError, "m_pipelineCache not initialized for this device");
		}
		TCU_THROW(InternalError, "vkCreateComputePipelines not defined");
	}

	// main process: store pipelines in JSON format. Pipelines will be sent later for m_pipelineCache creation ( and sent through file to another process )
	for (deUint32 i = 0; i < createInfoCount; ++i)
	{
		auto it = std::find_if(begin(m_pipelineInput.pipelines), end(m_pipelineInput.pipelines), vksc_server::PipelineIdentifierEqual(pipelineIDs[i]));
		pipelineIDs[i].pNext = DE_NULL;
		if (it == end(m_pipelineInput.pipelines))
		{
			const auto& featIt = m_deviceFeatures.find(device);
			if (featIt == end(m_deviceFeatures))
				TCU_THROW(InternalError, "Can't find device features for this pipeline");
			const auto& extIt = m_deviceExtensions.find(device);
			if (extIt == end(m_deviceExtensions))
				TCU_THROW(InternalError, "Can't find device extensions for this pipeline");

			m_pipelineInput.pipelines.push_back(vksc_server::VulkanJsonPipelineDescription(
				pipelineIDs[i],
				writeJSON_VkComputePipelineCreateInfo(pCreateInfoCopies[i]),
				featIt->second,
				extIt->second,
				m_currentTestPath));
		}
		else
			it->add(m_currentTestPath);
	}
	return VK_SUCCESS;
}

void ResourceInterfaceStandard::createRenderPass (VkDevice								device,
												  const VkRenderPassCreateInfo*			pCreateInfo,
												  const VkAllocationCallbacks*			pAllocator,
												  VkRenderPass*							pRenderPass) const
{
	DE_UNREF(device);
	DE_UNREF(pAllocator);
	m_pipelineInput.renderPasses.insert({ *pRenderPass,  writeJSON_VkRenderPassCreateInfo(*pCreateInfo) });
}

void ResourceInterfaceStandard::createRenderPass2 (VkDevice								device,
												   const VkRenderPassCreateInfo2*		pCreateInfo,
												   const VkAllocationCallbacks*			pAllocator,
												   VkRenderPass*						pRenderPass) const
{
	DE_UNREF(device);
	DE_UNREF(pAllocator);
	m_pipelineInput.renderPasses.insert({ *pRenderPass,  writeJSON_VkRenderPassCreateInfo2(*pCreateInfo) });
}

void ResourceInterfaceStandard::createPipelineLayout (VkDevice								device,
													  const VkPipelineLayoutCreateInfo*		pCreateInfo,
													  const VkAllocationCallbacks*			pAllocator,
													  VkPipelineLayout*						pPipelineLayout) const
{
	DE_UNREF(device);
	DE_UNREF(pAllocator);
	m_pipelineInput.pipelineLayouts.insert({*pPipelineLayout, writeJSON_VkPipelineLayoutCreateInfo(*pCreateInfo) });
}

void ResourceInterfaceStandard::createDescriptorSetLayout (VkDevice									device,
														   const VkDescriptorSetLayoutCreateInfo*	pCreateInfo,
														   const VkAllocationCallbacks*				pAllocator,
														   VkDescriptorSetLayout*					pSetLayout) const
{
	DE_UNREF(device);
	DE_UNREF(pAllocator);
	m_pipelineInput.descriptorSetLayouts.insert({ *pSetLayout, writeJSON_VkDescriptorSetLayoutCreateInfo(*pCreateInfo) });
}

void ResourceInterfaceStandard::createSampler (VkDevice							device,
											   const VkSamplerCreateInfo*		pCreateInfo,
											   const VkAllocationCallbacks*		pAllocator,
											   VkSampler*						pSampler) const
{
	DE_UNREF(device);
	DE_UNREF(pAllocator);
	m_pipelineInput.samplers.insert({ *pSampler,  writeJSON_VkSamplerCreateInfo(*pCreateInfo) });
}

void ResourceInterfaceStandard::createSamplerYcbcrConversion (VkDevice									device,
															  const VkSamplerYcbcrConversionCreateInfo*	pCreateInfo,
															  const VkAllocationCallbacks*				pAllocator,
															  VkSamplerYcbcrConversion*					pYcbcrConversion) const
{
	DE_UNREF(device);
	DE_UNREF(pAllocator);
	m_pipelineInput.samplerYcbcrConversions.insert({ *pYcbcrConversion,  writeJSON_VkSamplerYcbcrConversionCreateInfo(*pCreateInfo) });
}

void ResourceInterfaceStandard::createCommandPool (VkDevice										device,
												   const VkCommandPoolCreateInfo*				pCreateInfo,
												   const VkAllocationCallbacks*					pAllocator,
												   VkCommandPool*								pCommandPool) const
{
	DE_UNREF(device);
	DE_UNREF(pCreateInfo);
	DE_UNREF(pAllocator);
	m_commandPoolMemoryConsumption.push_back(vksc_server::VulkanCommandMemoryConsumption(pCommandPool->getInternal()));
}

void ResourceInterfaceStandard::allocateCommandBuffers (VkDevice								device,
														const VkCommandBufferAllocateInfo*		pAllocateInfo,
														VkCommandBuffer*						pCommandBuffers) const
{
	DE_UNREF(device);
	for (deUint32 i = 0; i < pAllocateInfo->commandBufferCount; ++i)
	{
		m_commandBufferMemoryConsumption.insert({ pCommandBuffers[i], vksc_server::VulkanCommandMemoryConsumption(pAllocateInfo->commandPool.getInternal()) });
	}
}

void ResourceInterfaceStandard::increaseCommandBufferSize (VkCommandBuffer						commandBuffer,
														   const char*							functionName) const
{
	DE_UNREF(functionName);
	auto it = m_commandBufferMemoryConsumption.find(commandBuffer);
	if (it == end(m_commandBufferMemoryConsumption))
		TCU_THROW(InternalError, "Unregistered command buffer");

	// We could use functionName parameter to differentiate between different sizes
	it->second.updateValues(128u, 128u, 128u);
}

void ResourceInterfaceStandard::importPipelineCacheData (const PlatformInterface&			vkp,
														 VkInstance							instance,
														 const InstanceInterface&			vki,
														 VkPhysicalDevice					physicalDevice,
														 deUint32							queueIndex)
{
	std::vector<vksc_server::VulkanPipelineSize> outPipelineSizes;
	m_cacheData = vksc_server::CreatePipelineCache(	m_pipelineInput,
													outPipelineSizes,
													vksc_server::CmdLineParams{},
													vkp,
													instance,
													vki,
													physicalDevice,
													queueIndex);
	m_pipelineSizes = outPipelineSizes;
}

void ResourceInterfaceStandard::resetObjects ()
{
	m_pipelineInput						= {};
	m_objectHashes.clear();
	m_commandPoolMemoryConsumption.clear();
	m_commandPoolIndex					= 0u;
	m_commandBufferMemoryConsumption.clear();
	m_resourceCounter					= 0u;
	m_statCurrent						= resetDeviceObjectReservationCreateInfo();
	m_statMax							= resetDeviceObjectReservationCreateInfo();
	m_cacheData.clear();
	m_pipelineSizes.clear();
	runGarbageCollection();
}

void ResourceInterfaceStandard::resetPipelineCaches ()
{
	if (m_testCtx.getCommandLine().isSubProcess())
	{
		m_pipelineCache.clear();
	}
}

#endif // CTS_USES_VULKANSC

vk::ProgramBinary* ResourceInterfaceStandard::compileProgram (const vk::ProgramIdentifier&	progId,
															  const vk::GlslSource&			source,
															  glu::ShaderProgramInfo*		buildInfo,
															  const tcu::CommandLine&		commandLine)
{
	DE_UNREF(progId);
	return vk::buildProgram(source, buildInfo, commandLine);
}

vk::ProgramBinary* ResourceInterfaceStandard::compileProgram (const vk::ProgramIdentifier&	progId,
															  const vk::HlslSource&			source,
															  glu::ShaderProgramInfo*		buildInfo,
															  const tcu::CommandLine&		commandLine)
{
	DE_UNREF(progId);
	return vk::buildProgram(source, buildInfo, commandLine);
}

vk::ProgramBinary* ResourceInterfaceStandard::compileProgram (const vk::ProgramIdentifier&	progId,
															  const vk::SpirVAsmSource&		source,
															  vk::SpirVProgramInfo*			buildInfo,
															  const tcu::CommandLine&		commandLine)
{
	DE_UNREF(progId);
	return vk::assembleProgram(source, buildInfo, commandLine);
}

#ifdef CTS_USES_VULKANSC

ResourceInterfaceVKSC::ResourceInterfaceVKSC (tcu::TestContext& testCtx)
	: ResourceInterfaceStandard(testCtx)
{
		auto address = testCtx.getCommandLine().getServerAddress();
		m_address = address ? address : std::string{};
}

vksc_server::Server* ResourceInterfaceVKSC::getServer ()
{
	if (!m_server)
	{
		m_server = std::make_shared<vksc_server::Server>(m_address);
	}
	return m_server.get();
}

bool ResourceInterfaceVKSC::noServer () const
{
	return m_address.empty();
}

vk::ProgramBinary* ResourceInterfaceVKSC::compileProgram (const vk::ProgramIdentifier&	progId,
														  const vk::GlslSource&			source,
														  glu::ShaderProgramInfo*		buildInfo,
														  const tcu::CommandLine&		commandLine)
{
	if (noServer()) return ResourceInterfaceStandard::compileProgram(progId, source, buildInfo, commandLine);

	DE_UNREF(progId);
	DE_UNREF(buildInfo);

	vksc_server::CompileShaderRequest request;
	request.source.active = "glsl";
	request.source.glsl = source;
	request.commandLine = commandLine.getInitialCmdLine();
	vksc_server::CompileShaderResponse response;
	getServer()->SendRequest(request, response);

	return new ProgramBinary(PROGRAM_FORMAT_SPIRV, response.binary.size(), response.binary.data());
}

vk::ProgramBinary* ResourceInterfaceVKSC::compileProgram (const vk::ProgramIdentifier&	progId,
														  const vk::HlslSource&			source,
														  glu::ShaderProgramInfo*		buildInfo,
														  const tcu::CommandLine&		commandLine)
{
	if (noServer()) return ResourceInterfaceStandard::compileProgram(progId, source, buildInfo, commandLine);

	DE_UNREF(progId);
	DE_UNREF(buildInfo);

	vksc_server::CompileShaderRequest request;
	request.source.active = "hlsl";
	request.source.hlsl = source;
	request.commandLine = commandLine.getInitialCmdLine();
	vksc_server::CompileShaderResponse response;
	getServer()->SendRequest(request, response);

	return new ProgramBinary(PROGRAM_FORMAT_SPIRV, response.binary.size(), response.binary.data());
}

vk::ProgramBinary* ResourceInterfaceVKSC::compileProgram (const vk::ProgramIdentifier&	progId,
														  const vk::SpirVAsmSource&		source,
														  vk::SpirVProgramInfo*			buildInfo,
														  const tcu::CommandLine&		commandLine)
{
	if (noServer()) return ResourceInterfaceStandard::compileProgram(progId, source, buildInfo, commandLine);

	DE_UNREF(progId);
	DE_UNREF(buildInfo);

	vksc_server::CompileShaderRequest request;
	request.source.active = "spirv";
	request.source.spirv = source;
	request.commandLine = commandLine.getInitialCmdLine();
	vksc_server::CompileShaderResponse response;
	getServer()->SendRequest(request, response);

	return new ProgramBinary(PROGRAM_FORMAT_SPIRV, response.binary.size(), response.binary.data());
}

VkResult ResourceInterfaceVKSC::createShaderModule (VkDevice							device,
													const VkShaderModuleCreateInfo*		pCreateInfo,
													const VkAllocationCallbacks*		pAllocator,
													VkShaderModule*						pShaderModule,
													bool								normalMode) const
{
	if (noServer() || !normalMode) return ResourceInterfaceStandard::createShaderModule(device, pCreateInfo, pAllocator, pShaderModule, normalMode);

	// We will reach this place only in one case:
	// - server exists
	// - subprocess asks for creation of VkShaderModule which will be later ignored, because it will receive the whole pipeline from server
	// ( Are there any tests which receive VkShaderModule and do not use it in any pipeline ? )
	*pShaderModule = VkShaderModule(++m_resourceCounter);
	registerObjectHash(pShaderModule->getInternal(), calculateShaderModuleHash(*pCreateInfo, getObjectHashes()));
	return VK_SUCCESS;
}


void ResourceInterfaceVKSC::importPipelineCacheData (const PlatformInterface&			vkp,
													 VkInstance							instance,
													 const InstanceInterface&			vki,
													 VkPhysicalDevice					physicalDevice,
													 deUint32							queueIndex)
{
	if (noServer()) return ResourceInterfaceStandard::importPipelineCacheData(vkp, instance, vki, physicalDevice, queueIndex);

	vksc_server::CreateCacheRequest request;
	request.input = m_pipelineInput;

	vksc_server::CreateCacheResponse response;
	getServer()->SendRequest(request, response);

	if (response.status)
	{
		m_cacheData = std::move(response.binary);

		m_pipelineSizes = response.pipelineSizes;
	}
	else { TCU_THROW(InternalError, "Server did not return pipeline cache data when requested (check server log for details)"); }
}

#endif


} // namespace vk
