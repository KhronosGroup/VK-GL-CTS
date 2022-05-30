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
	#include "vkApiVersion.hpp"
	using namespace vksc_server::json;
#endif // CTS_USES_VULKANSC

namespace vk
{

ResourceInterface::ResourceInterface (tcu::TestContext& testCtx)
	: m_testCtx			(testCtx)
#ifdef CTS_USES_VULKANSC
	, m_commandPoolIndex		(0u)
	, m_resourceCounter			(0u)
	, m_statCurrent				(resetDeviceObjectReservationCreateInfo())
	, m_statMax					(resetDeviceObjectReservationCreateInfo())
	, m_enabledHandleDestroy	(true)
#endif // CTS_USES_VULKANSC
{
#ifdef CTS_USES_VULKANSC
	// pipelineCacheRequestCount does not contain one instance of createPipelineCache call that happens only in subprocess
	m_statCurrent.pipelineCacheRequestCount		= 1u;
	m_statMax.pipelineCacheRequestCount			= 1u;
#endif // CTS_USES_VULKANSC
}

ResourceInterface::~ResourceInterface ()
{
}

void ResourceInterface::initTestCase (const std::string& casePath)
{
	m_currentTestPath = casePath;
}

const std::string& ResourceInterface::getCasePath() const
{
	return m_currentTestPath;
}

#ifdef CTS_USES_VULKANSC
void ResourceInterface::initApiVersion (const deUint32 version)
{
	const ApiVersion	apiVersion	= unpackVersion(version);
	const bool			vulkanSC	= (apiVersion.variantNum == 1);

	m_version	= tcu::Maybe<deUint32>(version);
	m_vulkanSC	= vulkanSC;
}

bool ResourceInterface::isVulkanSC (void) const
{
	return m_vulkanSC.get();
}

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

void ResourceInterface::setHandleDestroy(bool value)
{
	m_enabledHandleDestroy = value;
}

bool ResourceInterface::isEnabledHandleDestroy() const
{
	return m_enabledHandleDestroy;
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
			memC.second.maxCommandPoolAllocated,
			memC.second.maxCommandPoolReservedSize,
			memC.second.maxCommandBufferAllocated
		);
		m_commandPoolMemoryConsumption[j].commandBufferCount++;
	}
	// Each m_commandPoolMemoryConsumption element must have at least one command buffer ( see DeviceDriverSC::createCommandPoolHandlerNorm() )
	// As a result we have to ensure that commandBufferRequestCount is not less than the number of command pools
	m_statMax.commandBufferRequestCount = de::max(deUint32(m_commandPoolMemoryConsumption.size()), m_statMax.commandBufferRequestCount);
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

struct PipelinePoolSizeInfo
{
	deUint32					maxTestCount;
	deUint32					size;
};

void ResourceInterface::preparePipelinePoolSizes()
{
	std::map<std::string, std::vector<PipelinePoolSizeInfo>> pipelineInfoPerTest;

	// Step 1: collect information about all pipelines in each test, group by size
	for (const auto& pipeline : m_pipelineInput.pipelines)
	{
		auto it = std::find_if(begin(m_pipelineSizes), end(m_pipelineSizes), vksc_server::PipelineIdentifierEqual(pipeline.id));
		if (it == end(m_pipelineSizes))
			TCU_THROW(InternalError, "VkPipelinePoolEntrySizeCreateInfo not created for pipelineIdentifier");

		PipelinePoolSizeInfo ppsi
		{
			it->count,
			it->size
		};

		for (const auto& test : pipeline.tests)
		{
			auto pit = pipelineInfoPerTest.find(test);
			if (pit == end(pipelineInfoPerTest))
				pit = pipelineInfoPerTest.insert({ test, std::vector<PipelinePoolSizeInfo>() }).first;
			// group by the same sizes in a test
			bool found = false;
			for (size_t i = 0; i<pit->second.size(); ++i)
			{
				if (pit->second[i].size == ppsi.size)
				{
					pit->second[i].maxTestCount += ppsi.maxTestCount;
					found = true;
					break;
				}
			}
			if(!found)
				pit->second.push_back(ppsi);
		}
	}

	// Step 2: choose pipeline pool sizes
	std::vector<PipelinePoolSizeInfo> finalPoolSizes;
	for (const auto& pInfo : pipelineInfoPerTest)
	{
		for (const auto& ppsi1 : pInfo.second)
		{
			auto it = std::find_if(begin(finalPoolSizes), end(finalPoolSizes), [&ppsi1](const PipelinePoolSizeInfo& x) { return (x.size == ppsi1.size); });
			if (it != end(finalPoolSizes))
				it->maxTestCount = de::max(it->maxTestCount, ppsi1.maxTestCount);
			else
				finalPoolSizes.push_back(ppsi1);
		}
	}

	// Step 3: convert results to VkPipelinePoolSize
	m_pipelinePoolSizes.clear();
	for (const auto& ppsi : finalPoolSizes)
	{
		VkPipelinePoolSize poolSize =
		{
			VK_STRUCTURE_TYPE_PIPELINE_POOL_SIZE,	// VkStructureType	sType;
			DE_NULL,								// const void*		pNext;
			ppsi.size,								// VkDeviceSize		poolEntrySize;
			ppsi.maxTestCount						// deUint32			poolEntryCount;
		};
		m_pipelinePoolSizes.emplace_back(poolSize);
	}
}

std::vector<VkPipelinePoolSize> ResourceInterface::getPipelinePoolSizes () const
{
	return m_pipelinePoolSizes;
}

void ResourceInterface::fillPoolEntrySize (vk::VkPipelineOfflineCreateInfo&	pipelineIdentifier) const
{
	auto it = std::find_if(begin(m_pipelineSizes), end(m_pipelineSizes), vksc_server::PipelineIdentifierEqual(pipelineIdentifier));
	if( it == end(m_pipelineSizes) )
		TCU_THROW(InternalError, "VkPipelinePoolEntrySizeCreateInfo not created for pipelineIdentifier");
	pipelineIdentifier.poolEntrySize = it->size;
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

VkPipelineCache ResourceInterface::getPipelineCache(VkDevice device) const
{
	auto pit = m_pipelineCache.find(device);
	if (pit == end(m_pipelineCache))
		TCU_THROW(InternalError, "m_pipelineCache not found for this device");
	return pit->second.get()->get();
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
		if (m_cacheData.size() > 0)
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
		if (isVulkanSC())
		{
			*pShaderModule = VkShaderModule(++m_resourceCounter);
			registerObjectHash(pShaderModule->getInternal(), calculateShaderModuleHash(*pCreateInfo, getObjectHashes()));
			return VK_SUCCESS;
		}
		else
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
	}

	// main process: store VkShaderModuleCreateInfo in JSON format. Shaders will be sent later for m_pipelineCache creation ( and sent through file to another process )
	*pShaderModule = VkShaderModule(++m_resourceCounter);
	registerObjectHash(pShaderModule->getInternal(), calculateShaderModuleHash(*pCreateInfo, getObjectHashes()));
	m_pipelineInput.shaderModules.insert({ *pShaderModule, writeJSON_VkShaderModuleCreateInfo(*pCreateInfo) });
	return VK_SUCCESS;
}

VkPipelineOfflineCreateInfo makeGraphicsPipelineIdentifier (const std::string& testPath, const VkGraphicsPipelineCreateInfo& gpCI, const std::map<deUint64, std::size_t>& objectHashes)
{
	DE_UNREF(testPath);
	VkPipelineOfflineCreateInfo	pipelineID		= resetPipelineOfflineCreateInfo();
	std::size_t					hashValue		= calculateGraphicsPipelineHash(gpCI, objectHashes);
	memcpy(pipelineID.pipelineIdentifier, &hashValue, sizeof(std::size_t));
	return pipelineID;
}

VkPipelineOfflineCreateInfo makeComputePipelineIdentifier (const std::string& testPath, const VkComputePipelineCreateInfo& cpCI, const std::map<deUint64, std::size_t>& objectHashes)
{
	DE_UNREF(testPath);
	VkPipelineOfflineCreateInfo	pipelineID		= resetPipelineOfflineCreateInfo();
	std::size_t					hashValue		= calculateComputePipelineHash(cpCI, objectHashes);
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

	// build pipeline identifiers (if required), make a copy of pCreateInfos
	std::vector<VkPipelineOfflineCreateInfo>		pipelineIDs;
	std::vector<deUint8>							idInPNextChain;
	std::vector<VkGraphicsPipelineCreateInfo>		pCreateInfoCopies;

	for (deUint32 i = 0; i < createInfoCount; ++i)
	{
		pCreateInfoCopies.push_back(pCreateInfos[i]);

		// Check if test added pipeline identifier on its own
		VkPipelineOfflineCreateInfo* idInfo = (VkPipelineOfflineCreateInfo*)findStructureInChain(pCreateInfos[i].pNext, VK_STRUCTURE_TYPE_PIPELINE_OFFLINE_CREATE_INFO);
		if (idInfo == DE_NULL)
		{
			pipelineIDs.push_back(makeGraphicsPipelineIdentifier(m_currentTestPath, pCreateInfos[i], getObjectHashes()));
			idInPNextChain.push_back(0);
		}
		else
		{
			pipelineIDs.push_back(*idInfo);
			idInPNextChain.push_back(1);
		}

		if (normalMode)
			fillPoolEntrySize(pipelineIDs.back());
	}

	// reset not used pointers, so that JSON generation does not crash
	std::vector<VkPipelineViewportStateCreateInfo>	viewportStateCopies	(createInfoCount);
	if (!normalMode)
	{
		for (deUint32 i = 0; i < createInfoCount; ++i)
		{
			bool vertexInputStateRequired		= false;
			bool inputAssemblyStateRequired		= false;
			bool tessellationStateRequired		= false;
			bool viewportStateRequired			= false;
			bool viewportStateViewportsRequired	= false;
			bool viewportStateScissorsRequired	= false;
			bool multiSampleStateRequired		= false;
			bool depthStencilStateRequired		= false;
			bool colorBlendStateRequired		= false;

			if (pCreateInfoCopies[i].pStages != DE_NULL)
			{
				for (deUint32 j = 0; j < pCreateInfoCopies[i].stageCount; ++j)
				{
					if (pCreateInfoCopies[i].pStages[j].stage == VK_SHADER_STAGE_VERTEX_BIT)
					{
						vertexInputStateRequired	= true;
						inputAssemblyStateRequired	= true;
					}
					if (pCreateInfoCopies[i].pStages[j].stage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
					{
						tessellationStateRequired	= true;
					}
				}
			}
			if (pCreateInfoCopies[i].pDynamicState != DE_NULL)
			{
				if (pCreateInfoCopies[i].pDynamicState->pDynamicStates != DE_NULL)
					for (deUint32 j = 0; j < pCreateInfoCopies[i].pDynamicState->dynamicStateCount; ++j)
					{
						if (pCreateInfoCopies[i].pDynamicState->pDynamicStates[j] == VK_DYNAMIC_STATE_VIEWPORT || pCreateInfoCopies[i].pDynamicState->pDynamicStates[j] == VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT_EXT)
						{
							viewportStateRequired			= true;
							viewportStateViewportsRequired	= true;
						}
						if (pCreateInfoCopies[i].pDynamicState->pDynamicStates[j] == VK_DYNAMIC_STATE_SCISSOR || pCreateInfoCopies[i].pDynamicState->pDynamicStates[j] == VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT_EXT)
						{
							viewportStateRequired			= true;
							viewportStateScissorsRequired	= true;
						}
						if (pCreateInfoCopies[i].pDynamicState->pDynamicStates[j] == VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE_EXT)
							viewportStateRequired = true;
					}
			}
			if (pCreateInfoCopies[i].pRasterizationState != DE_NULL)
			{
				if (pCreateInfoCopies[i].pRasterizationState->rasterizerDiscardEnable == VK_FALSE)
				{
					viewportStateRequired			= true;
					viewportStateViewportsRequired	= true;
					viewportStateScissorsRequired	= true;
					multiSampleStateRequired		= true;
					depthStencilStateRequired		= true;
					colorBlendStateRequired			= true;
				}
			}
			if (pCreateInfoCopies[i].pVertexInputState != DE_NULL && !vertexInputStateRequired)
				pCreateInfoCopies[i].pVertexInputState = DE_NULL;
			if (pCreateInfoCopies[i].pInputAssemblyState != DE_NULL && !inputAssemblyStateRequired)
				pCreateInfoCopies[i].pInputAssemblyState = DE_NULL;
			if (pCreateInfoCopies[i].pTessellationState != DE_NULL && !tessellationStateRequired)
				pCreateInfoCopies[i].pTessellationState = DE_NULL;
			if (pCreateInfoCopies[i].pViewportState != DE_NULL)
			{
				if (viewportStateRequired)
				{
					viewportStateCopies[i]		= *(pCreateInfoCopies[i].pViewportState);
					bool exchangeVP				= false;
					if (pCreateInfoCopies[i].pViewportState->pViewports != DE_NULL && !viewportStateViewportsRequired)
					{
						viewportStateCopies[i].pViewports		= DE_NULL;
						viewportStateCopies[i].viewportCount	= 0u;
						exchangeVP = true;
					}
					if (pCreateInfoCopies[i].pViewportState->pScissors != DE_NULL && !viewportStateScissorsRequired)
					{
						viewportStateCopies[i].pScissors	= DE_NULL;
						viewportStateCopies[i].scissorCount	= 0u;
						exchangeVP = true;
					}
					if (exchangeVP)
						pCreateInfoCopies[i].pViewportState = &(viewportStateCopies[i]);
				}
				else
					pCreateInfoCopies[i].pViewportState = DE_NULL;
			}
			if (pCreateInfoCopies[i].pMultisampleState != DE_NULL && !multiSampleStateRequired)
				pCreateInfoCopies[i].pMultisampleState = DE_NULL;
			if (pCreateInfoCopies[i].pDepthStencilState != DE_NULL && !depthStencilStateRequired)
				pCreateInfoCopies[i].pDepthStencilState = DE_NULL;
			if (pCreateInfoCopies[i].pColorBlendState != DE_NULL && !colorBlendStateRequired)
				pCreateInfoCopies[i].pColorBlendState = DE_NULL;
		}
	}

	// Include pipelineIdentifiers into pNext chain of pCreateInfoCopies - skip this operation if pipeline identifier was created inside test
	for (deUint32 i = 0; i < createInfoCount; ++i)
	{
		if (idInPNextChain[i] == 0)
		{
			pipelineIDs[i].pNext				= pCreateInfoCopies[i].pNext;
			pCreateInfoCopies[i].pNext			= &pipelineIDs[i];
		}
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
		m_pipelineIdentifiers.insert({ pPipelines[i], pipelineIDs[i] });

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

	// build pipeline identifiers (if required), make a copy of pCreateInfos
	std::vector<VkPipelineOfflineCreateInfo>		pipelineIDs;
	std::vector<deUint8>							idInPNextChain;
	std::vector<VkComputePipelineCreateInfo>		pCreateInfoCopies;

	for (deUint32 i = 0; i < createInfoCount; ++i)
	{
		pCreateInfoCopies.push_back(pCreateInfos[i]);

		// Check if test added pipeline identifier on its own
		VkPipelineOfflineCreateInfo* idInfo = (VkPipelineOfflineCreateInfo*)findStructureInChain(pCreateInfos[i].pNext, VK_STRUCTURE_TYPE_PIPELINE_OFFLINE_CREATE_INFO);
		if (idInfo == DE_NULL)
		{
			pipelineIDs.push_back(makeComputePipelineIdentifier(m_currentTestPath, pCreateInfos[i], getObjectHashes()));
			idInPNextChain.push_back(0);
		}
		else
		{
			pipelineIDs.push_back(*idInfo);
			idInPNextChain.push_back(1);
		}

		if (normalMode)
			fillPoolEntrySize(pipelineIDs.back());
	}

	// Include pipelineIdentifiers into pNext chain of pCreateInfoCopies - skip this operation if pipeline identifier was created inside test
	for (deUint32 i = 0; i < createInfoCount; ++i)
	{
		if (idInPNextChain[i] == 0)
		{
			pipelineIDs[i].pNext				= pCreateInfoCopies[i].pNext;
			pCreateInfoCopies[i].pNext			= &pipelineIDs[i];
		}
	}

	// subprocess: load compute pipelines from OUR pipeline cache
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
		m_pipelineIdentifiers.insert({ pPipelines[i], pipelineIDs[i] });

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

void ResourceInterfaceStandard::destroyPipeline (VkDevice								device,
												 VkPipeline								pipeline,
												 const VkAllocationCallbacks*			pAllocator) const
{
	DE_UNREF(device);
	DE_UNREF(pAllocator);

	auto it = m_pipelineIdentifiers.find(pipeline);
	if(it==end(m_pipelineIdentifiers))
		TCU_THROW(InternalError, "Can't find pipeline");

	auto pit = std::find_if(begin(m_pipelineInput.pipelines), end(m_pipelineInput.pipelines), vksc_server::PipelineIdentifierEqual(it->second));
	if (pit == end(m_pipelineInput.pipelines))
		TCU_THROW(InternalError, "Can't find pipeline identifier");
	pit->remove();
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
														   VkDeviceSize							commandSize) const
{
	auto it = m_commandBufferMemoryConsumption.find(commandBuffer);
	if (it == end(m_commandBufferMemoryConsumption))
		TCU_THROW(InternalError, "Unregistered command buffer");

	it->second.updateValues(commandSize, commandSize, commandSize);
}

void ResourceInterfaceStandard::resetCommandPool (VkDevice								device,
												  VkCommandPool							commandPool,
												  VkCommandPoolResetFlags				flags) const
{
	DE_UNREF(device);
	DE_UNREF(flags);

	for (auto& memC : m_commandBufferMemoryConsumption)
	{
		if (memC.second.commandPool == commandPool.getInternal())
			memC.second.resetValues();
	}
}

void ResourceInterfaceStandard::importPipelineCacheData (const PlatformInterface&			vkp,
														 VkInstance							instance,
														 const InstanceInterface&			vki,
														 VkPhysicalDevice					physicalDevice,
														 deUint32							queueIndex)
{
	if(!std::string(m_testCtx.getCommandLine().getPipelineCompilerPath()).empty())
	{
		m_cacheData = vksc_server::buildOfflinePipelineCache(m_pipelineInput,
															 std::string( m_testCtx.getCommandLine().getPipelineCompilerPath()),
															 std::string( m_testCtx.getCommandLine().getPipelineCompilerDataDir()),
															 std::string( m_testCtx.getCommandLine().getPipelineCompilerArgs()),
															 std::string( m_testCtx.getCommandLine().getPipelineCompilerOutputFile()),
															 std::string( m_testCtx.getCommandLine().getPipelineCompilerLogFile()),
															 std::string( m_testCtx.getCommandLine().getPipelineCompilerFilePrefix()) );
	}
	else
	{
		m_cacheData = vksc_server::buildPipelineCache(m_pipelineInput, vkp, instance, vki, physicalDevice, queueIndex);
	}

	VkPhysicalDeviceVulkanSC10Properties	vulkanSC10Properties	= initVulkanStructure();
	VkPhysicalDeviceProperties2				deviceProperties2		= initVulkanStructure(&vulkanSC10Properties);
	vki.getPhysicalDeviceProperties2(physicalDevice, &deviceProperties2);

	m_pipelineSizes	= vksc_server::extractSizesFromPipelineCache( m_pipelineInput, m_cacheData, deUint32(m_testCtx.getCommandLine().getPipelineDefaultSize()), vulkanSC10Properties.recyclePipelineMemory == VK_TRUE);
	preparePipelinePoolSizes();
}

void ResourceInterfaceStandard::resetObjects ()
{
	m_pipelineInput							= {};
	m_objectHashes.clear();
	m_commandPoolMemoryConsumption.clear();
	m_commandPoolIndex						= 0u;
	m_commandBufferMemoryConsumption.clear();
	m_resourceCounter						= 0u;
	m_statCurrent							= resetDeviceObjectReservationCreateInfo();
	m_statMax								= resetDeviceObjectReservationCreateInfo();
	// pipelineCacheRequestCount does not contain one instance of createPipelineCache call that happens only in subprocess
	m_statCurrent.pipelineCacheRequestCount	= 1u;
	m_statMax.pipelineCacheRequestCount		= 1u;
	m_cacheData.clear();
	m_pipelineIdentifiers.clear();
	m_pipelineSizes.clear();
	m_pipelinePoolSizes.clear();
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
		m_address = std::string(testCtx.getCommandLine().getServerAddress());
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
	if (noServer() || !normalMode || !isVulkanSC())
		return ResourceInterfaceStandard::createShaderModule(device, pCreateInfo, pAllocator, pShaderModule, normalMode);

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
	if (noServer())
	{
		ResourceInterfaceStandard::importPipelineCacheData(vkp, instance, vki, physicalDevice, queueIndex);
		return;
	}

	vksc_server::CreateCacheRequest request;
	request.input						= m_pipelineInput;
	std::vector<int>	caseFraction	= m_testCtx.getCommandLine().getCaseFraction();
	request.caseFraction				= caseFraction.empty() ? -1 : caseFraction[0];

	vksc_server::CreateCacheResponse response;
	getServer()->SendRequest(request, response);

	if (response.status)
	{
		m_cacheData = std::move(response.binary);

		VkPhysicalDeviceVulkanSC10Properties	vulkanSC10Properties	= initVulkanStructure();
		VkPhysicalDeviceProperties2				deviceProperties2		= initVulkanStructure(&vulkanSC10Properties);
		vki.getPhysicalDeviceProperties2(physicalDevice, &deviceProperties2);

		m_pipelineSizes	= vksc_server::extractSizesFromPipelineCache( m_pipelineInput, m_cacheData, deUint32(m_testCtx.getCommandLine().getPipelineDefaultSize()), vulkanSC10Properties.recyclePipelineMemory == VK_TRUE);
		preparePipelinePoolSizes();
	}
	else { TCU_THROW(InternalError, "Server did not return pipeline cache data when requested (check server log for details)"); }
}

MultithreadedDestroyGuard::MultithreadedDestroyGuard (de::SharedPtr<vk::ResourceInterface> resourceInterface)
	: m_resourceInterface{ resourceInterface }
{
	if (m_resourceInterface.get() != DE_NULL)
		m_resourceInterface->setHandleDestroy(false);
}

MultithreadedDestroyGuard::~MultithreadedDestroyGuard ()
{
	if (m_resourceInterface.get() != DE_NULL)
		m_resourceInterface->setHandleDestroy(true);
}

#endif // CTS_USES_VULKANSC


} // namespace vk
