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
	#include "deFile.h"
	#define VULKAN_JSON_CTS
	#include "vulkan_json_data.hpp"
	#include "vulkan_json_parser.hpp"
	#include "vkSafetyCriticalUtil.hpp"
	#include "vkRefUtil.hpp"
	#include "tcuCommandLine.hpp"
#endif // CTS_USES_VULKANSC

namespace vk
{

ResourceInterface::ResourceInterface (tcu::TestContext& testCtx)
	: m_testCtx(testCtx)
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
void ResourceInterface::removeRedundantObjects()
{
	// At the end of the day we only need to export objects used in pipelines.
	// Rest of the objects may be removed from m_json* structures as redundant
	std::set<VkSampler>				samplersInPipeline;
	std::set<VkShaderModule>		shadersInPipeline;
	std::set<VkRenderPass>			renderPassesInPipeline;
	std::set<VkPipelineLayout>		pipelineLayoutsInPipeline;
	std::set<VkDescriptorSetLayout>	descriptorSetLayoutsInPipeline;

	Json::CharReaderBuilder			builder;
	de::UniquePtr<Json::CharReader>	jsonReader(builder.newCharReader());

	for (auto it = begin(m_jsonPipelines); it != end(m_jsonPipelines); ++it)
	{
		if (it->find("VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO") != std::string::npos)
		{
			VkGraphicsPipelineCreateInfo	gpCI;
			deMemset(&gpCI, 0, sizeof(gpCI));
			readJSON_VkGraphicsPipelineCreateInfo(jsonReader.get(), *it, gpCI);

			for (deUint32 i = 0; i < gpCI.stageCount; ++i)
				shadersInPipeline.insert(gpCI.pStages[i].module);
			renderPassesInPipeline.insert(gpCI.renderPass);
			pipelineLayoutsInPipeline.insert(gpCI.layout);
		}
		else if (it->find("VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO") != std::string::npos)
		{
			VkComputePipelineCreateInfo	cpCI;
			deMemset(&cpCI, 0, sizeof(cpCI));
			readJSON_VkComputePipelineCreateInfo(jsonReader.get(), *it, cpCI);

			shadersInPipeline.insert(cpCI.stage.module);
			pipelineLayoutsInPipeline.insert(cpCI.layout);
		}
		else
			TCU_THROW(InternalError, "Could not recognize pipeline type");
	}
	for (auto it = begin(m_jsonShaderModules); it != end(m_jsonShaderModules); )
	{
		if (shadersInPipeline.find(it->first) == end(shadersInPipeline))
			it = m_jsonShaderModules.erase(it);
		else
			++it;
	}
	for (auto it = begin(m_jsonRenderPasses); it != end(m_jsonRenderPasses); )
	{
		if (renderPassesInPipeline.find(it->first) == end(renderPassesInPipeline))
			it = m_jsonRenderPasses.erase(it);
		else
			++it;
	}
	for (auto it = begin(m_jsonPipelineLayouts); it != end(m_jsonPipelineLayouts); )
	{
		if (pipelineLayoutsInPipeline.find(it->first) == end(pipelineLayoutsInPipeline))
		{
			it = m_jsonPipelineLayouts.erase(it);
		}
		else
		{
			VkPipelineLayoutCreateInfo	plCI;
			deMemset(&plCI, 0, sizeof(plCI));
			readJSON_VkPipelineLayoutCreateInfo(jsonReader.get(), it->second, plCI);
			for (deUint32 i = 0; i < plCI.setLayoutCount; ++i)
				descriptorSetLayoutsInPipeline.insert(plCI.pSetLayouts[i]);
			++it;
		}
	}
	for (auto it = begin(m_jsonDescriptorSetLayouts); it != end(m_jsonDescriptorSetLayouts); )
	{
		if (descriptorSetLayoutsInPipeline.find(it->first) == end(descriptorSetLayoutsInPipeline))
			it = m_jsonDescriptorSetLayouts.erase(it);
		else
		{
			VkDescriptorSetLayoutCreateInfo	dsCI;
			deMemset(&dsCI, 0, sizeof(dsCI));
			readJSON_VkDescriptorSetLayoutCreateInfo(jsonReader.get(), it->second, dsCI);

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
	for (auto it = begin(m_jsonSamplers); it != end(m_jsonSamplers); )
	{
		if (samplersInPipeline.find(it->first) == end(samplersInPipeline))
			it = m_jsonSamplers.erase(it);
		else
			++it;
	}
}

void ResourceInterface::exportDataToFile (const std::string&	fileName,
										  const std::string&	jsonMemoryReservation) const
{
	deFile*		exportFile = deFile_create(fileName.c_str(), DE_FILEMODE_CREATE | DE_FILEMODE_OPEN | DE_FILEMODE_WRITE | DE_FILEMODE_TRUNCATE );
	// write jsonMemoryReservation
	deInt64			numWritten	= 0;
	std::string		mrIntro		= "#memoryReservation_begin#\n";
	deFile_write(exportFile, mrIntro.c_str(), mrIntro.size(), &numWritten);
	deFile_write(exportFile, jsonMemoryReservation.c_str(), jsonMemoryReservation.size(), &numWritten);
	std::string		mrOutro		= "#memoryReservation_end#\n";
	deFile_write(exportFile, mrOutro.c_str(), mrOutro.size(), &numWritten);

	// write all samplers
	{
		std::string		samplersIntro = "#samplers_begin#\n";
		deFile_write(exportFile, samplersIntro.c_str(), samplersIntro.size(), &numWritten);
		for (auto it = begin(m_jsonSamplers); it != end(m_jsonSamplers); ++it)
		{
			std::ostringstream str;
			str << "#sampler " << it->first.getInternal() << std::endl;
			str << it->second << std::endl;
			deFile_write(exportFile, str.str().c_str(), str.str().size(), &numWritten);
		}
		std::string		samplersOutro = "#samplers_end#\n";
		deFile_write(exportFile, samplersOutro.c_str(), samplersOutro.size(), &numWritten);
	}

	// write all shaders
	{
		std::string		shadersIntro = "#shaders_begin#\n";
		deFile_write(exportFile, shadersIntro.c_str(), shadersIntro.size(), &numWritten);
		for (auto it = begin(m_jsonShaderModules); it != end(m_jsonShaderModules); ++it)
		{
			std::ostringstream str;
			str << "#shader " << it->first.getInternal() << std::endl;
			str << it->second << std::endl;
			deFile_write(exportFile, str.str().c_str(), str.str().size(), &numWritten);
		}
		std::string		shadersOutro = "#shaders_end#\n";
		deFile_write(exportFile, shadersOutro.c_str(), shadersOutro.size(), &numWritten);
	}

	// write all render passes
	{
		std::string		rpIntro = "#renderpasses_begin#\n";
		deFile_write(exportFile, rpIntro.c_str(), rpIntro.size(), &numWritten);
		for (auto it = begin(m_jsonRenderPasses); it != end(m_jsonRenderPasses); ++it)
		{
			std::ostringstream str;
			str << "#renderpass " << it->first.getInternal() << std::endl;
			str << it->second << std::endl;
			deFile_write(exportFile, str.str().c_str(), str.str().size(), &numWritten);
		}
		std::string		rpOutro = "#renderpasses_end#\n";
		deFile_write(exportFile, rpOutro.c_str(), rpOutro.size(), &numWritten);
	}

	// write all descriptor set layouts
	{
		std::string		rpIntro = "#descriptorsetlayouts_begin#\n";
		deFile_write(exportFile, rpIntro.c_str(), rpIntro.size(), &numWritten);
		for (auto it = begin(m_jsonDescriptorSetLayouts); it != end(m_jsonDescriptorSetLayouts); ++it)
		{
			std::ostringstream str;
			str << "#descriptorsetlayout " << it->first.getInternal() << std::endl;
			str << it->second << std::endl;
			deFile_write(exportFile, str.str().c_str(), str.str().size(), &numWritten);
		}
		std::string		rpOutro = "#descriptorsetlayouts_end#\n";
		deFile_write(exportFile, rpOutro.c_str(), rpOutro.size(), &numWritten);
	}

	// write all pipeline layouts
	{
		std::string		rpIntro = "#pipelinelayouts_begin#\n";
		deFile_write(exportFile, rpIntro.c_str(), rpIntro.size(), &numWritten);
		for (auto it = begin(m_jsonPipelineLayouts); it != end(m_jsonPipelineLayouts); ++it)
		{
			std::ostringstream str;
			str << "#pipelinelayout " << it->first.getInternal() << std::endl;
			str << it->second << std::endl;
			deFile_write(exportFile, str.str().c_str(), str.str().size(), &numWritten);
		}
		std::string		rpOutro = "#pipelinelayouts_end#\n";
		deFile_write(exportFile, rpOutro.c_str(), rpOutro.size(), &numWritten);
	}

	// write all pipelines
	{
		std::string		pipelinesIntro = "#pipelines_begin#\n";
		deFile_write(exportFile, pipelinesIntro.c_str(), pipelinesIntro.size(), &numWritten);
		for (auto it = begin(m_jsonPipelines); it != end(m_jsonPipelines); ++it)
		{
			std::ostringstream str;
			str << "#pipeline" << std::endl;
			str << *it << std::endl;
			deFile_write(exportFile, str.str().c_str(), str.str().size(), &numWritten);
		}
		std::string		pipelinesOutro = "#pipelines_end#\n";
		deFile_write(exportFile, pipelinesOutro.c_str(), pipelinesOutro.size(), &numWritten);
	}
	deFile_destroy(exportFile);
}

void ResourceInterface::importDataFromFile (const std::string&	fileName) const
{
	deFile*					importFile		= deFile_create(fileName.c_str(), DE_FILEMODE_OPEN | DE_FILEMODE_READ);
	deInt64					importSize		= deFile_getSize(importFile);
	std::vector<char>		importContents	(static_cast<std::size_t>(importSize));
	deInt64					numRead			= 0;
	deFile_read(importFile, importContents.data(), importSize, &numRead);
	std::string				importText		(importContents.data(), std::size_t(importSize));
	deFile_destroy(importFile);

	Json::CharReaderBuilder			builder;
	de::UniquePtr<Json::CharReader>	jsonReader(builder.newCharReader());

	// import VkDeviceObjectReservationCreateInfo
	while(true)
	{
		std::string			beginText		("#memoryReservation_begin#\n");
		std::string			endText			("#memoryReservation_end#\n");
		std::size_t			beginPos		= importText.find(beginText);
		if (beginPos == std::string::npos)
			break;
		std::size_t			endPos			= importText.find(endText, beginPos + beginText.size());
		if (endPos == std::string::npos)
			break;
		std::string			memoryResText	(begin(importText) + beginPos + +beginText.size(), begin(importText) + endPos);

		readJSON_VkDeviceObjectReservationCreateInfo(jsonReader.get(), memoryResText, m_memoryReservation);
		break;
	}

	// import VkSamplerCreateInfo in JSON format
	{
		std::string			beginText		("#samplers_begin#\n");
		std::string			endText			("#samplers_end#\n");
		std::size_t			beginPos		= importText.find(beginText);
		std::size_t			endPos			= importText.find(endText, beginPos + beginText.size());
		std::string			samplersText	(begin(importText) + beginPos + +beginText.size(), begin(importText) + endPos);

		std::string			shaderTag		("#sampler ");
		beginPos							= 0;
		endPos								= 0;
		while (endPos != std::string::npos)
		{
			beginPos							= samplersText.find(shaderTag, endPos);
			endPos								= samplersText.find("\n", beginPos);
			if (beginPos == std::string::npos || endPos == std::string::npos)
				continue;
			std::string			samplerNumber	(begin(samplersText) + beginPos + shaderTag.size(), begin(samplersText) + endPos);
			deUint64			samplerInternal;
			std::istringstream	str				(samplerNumber);
			str >> samplerInternal;

			beginPos							= endPos + 1;
			endPos								= samplersText.find	(shaderTag, beginPos);
			std::string							jsonSamplerModule	(begin(samplersText) + beginPos, begin(samplersText) + ((endPos != std::string::npos) ? endPos : samplersText.size()));

			m_jsonSamplers.insert({ VkSampler(samplerInternal), jsonSamplerModule });
		}
	}

	// import VkShaderModuleCreateInfo in JSON format
	{
		std::string			beginText		("#shaders_begin#\n");
		std::string			endText			("#shaders_end#\n");
		std::size_t			beginPos		= importText.find(beginText);
		std::size_t			endPos			= importText.find(endText, beginPos + beginText.size());
		std::string			shadersText		(begin(importText) + beginPos + +beginText.size(), begin(importText) + endPos);

		std::string			shaderTag		("#shader ");
		beginPos							= 0;
		endPos								= 0;
		while (endPos != std::string::npos)
		{
			beginPos							= shadersText.find(shaderTag, endPos);
			endPos								= shadersText.find("\n", beginPos);
			if (beginPos == std::string::npos || endPos == std::string::npos)
				continue;
			std::string			shaderNumber	(begin(shadersText) + beginPos + shaderTag.size(), begin(shadersText) + endPos);
			deUint64			shaderInternal;
			std::istringstream	str				(shaderNumber);
			str >> shaderInternal;

			beginPos							= endPos + 1;
			endPos								= shadersText.find(shaderTag, beginPos);
			std::string			jsonShaderModule(begin(shadersText) + beginPos, begin(shadersText) + ((endPos!= std::string::npos)? endPos : shadersText.size()));

			m_jsonShaderModules.insert({ VkShaderModule(shaderInternal), jsonShaderModule });
		}
	}

	// import render passes in JSON format
	{
		std::string			beginText		("#renderpasses_begin#\n");
		std::string			endText			("#renderpasses_end#\n");
		std::size_t			beginPos		= importText.find(beginText);
		std::size_t			endPos			= importText.find(endText, beginPos + beginText.size());
		std::string			rpText			(begin(importText) + beginPos + +beginText.size(), begin(importText) + endPos);

		std::string			rpTag			("#renderpass ");
		beginPos							= 0;
		endPos								= 0;
		while (endPos != std::string::npos)
		{
			beginPos							= rpText.find(rpTag, endPos);
			endPos								= rpText.find("\n", beginPos);
			if (beginPos == std::string::npos || endPos == std::string::npos)
				continue;
			std::string			rpNumber		(begin(rpText) + beginPos + rpTag.size(), begin(rpText) + endPos);
			deUint64			rpInternal;
			std::istringstream	str				(rpNumber);
			str >> rpInternal;

			beginPos							= endPos + 1;
			endPos								= rpText.find(rpTag, beginPos);
			std::string			jsonRenderPass	(begin(rpText) + beginPos, begin(rpText) + ((endPos!= std::string::npos)? endPos : rpText.size()));

			m_jsonRenderPasses.insert({ VkRenderPass(rpInternal), jsonRenderPass });
		}
	}

	// import descriptor set layouts in JSON format
	{
		std::string			beginText		("#descriptorsetlayouts_begin#\n");
		std::string			endText			("#descriptorsetlayouts_end#\n");
		std::size_t			beginPos		= importText.find(beginText);
		std::size_t			endPos			= importText.find(endText, beginPos + beginText.size());
		std::string			rpText			(begin(importText) + beginPos + +beginText.size(), begin(importText) + endPos);

		std::string			shaderTag		("#descriptorsetlayout ");
		beginPos							= 0;
		endPos								= 0;
		while (endPos != std::string::npos)
		{
			beginPos							= rpText.find(shaderTag, endPos);
			endPos								= rpText.find("\n", beginPos);
			if (beginPos == std::string::npos || endPos == std::string::npos)
				continue;
			std::string			rpNumber		(begin(rpText) + beginPos + shaderTag.size(), begin(rpText) + endPos);
			deUint64			rpInternal;
			std::istringstream	str				(rpNumber);
			str >> rpInternal;

			beginPos							= endPos + 1;
			endPos								= rpText.find(shaderTag, beginPos);
			std::string			jsonPipelineLay	(begin(rpText) + beginPos, begin(rpText) + ((endPos!= std::string::npos)? endPos : rpText.size()));

			m_jsonDescriptorSetLayouts.insert({ VkDescriptorSetLayout(rpInternal), jsonPipelineLay });
		}
	}

	// import pipeline layouts in JSON format
	{
		std::string			beginText		("#pipelinelayouts_begin#\n");
		std::string			endText			("#pipelinelayouts_end#\n");
		std::size_t			beginPos		= importText.find(beginText);
		std::size_t			endPos			= importText.find(endText, beginPos + beginText.size());
		std::string			rpText			(begin(importText) + beginPos + +beginText.size(), begin(importText) + endPos);

		std::string			shaderTag		("#pipelinelayout ");
		beginPos							= 0;
		endPos								= 0;
		while (endPos != std::string::npos)
		{
			beginPos							= rpText.find(shaderTag, endPos);
			endPos								= rpText.find("\n", beginPos);
			if (beginPos == std::string::npos || endPos == std::string::npos)
				continue;
			std::string			rpNumber		(begin(rpText) + beginPos + shaderTag.size(), begin(rpText) + endPos);
			deUint64			rpInternal;
			std::istringstream	str				(rpNumber);
			str >> rpInternal;

			beginPos							= endPos + 1;
			endPos								= rpText.find(shaderTag, beginPos);
			std::string			jsonPipelineLay	(begin(rpText) + beginPos, begin(rpText) + ((endPos!= std::string::npos)? endPos : rpText.size()));

			m_jsonPipelineLayouts.insert({ VkPipelineLayout(rpInternal), jsonPipelineLay });
		}
	}

	// import pipelines in JSON format
	{
		std::string			beginText		("#pipelines_begin#\n");
		std::string			endText			("#pipelines_end#\n");
		std::size_t			beginPos		= importText.find(beginText);
		std::size_t			endPos			= importText.find(endText, beginPos + beginText.size());
		std::string			pipelinesText	(importText.begin() + beginPos + +beginText.size(), importText.begin() + endPos);

		std::string			pipelineTag		("#pipeline\n");
		beginPos							= 0;
		endPos								= 0;
		while (endPos != std::string::npos)
		{
			beginPos							= pipelinesText.find(pipelineTag, endPos);
			endPos								= pipelinesText.find(pipelineTag, beginPos + pipelineTag.size());
			if (beginPos == std::string::npos)
				continue;
			std::string			jsonPipeline	(begin(pipelinesText) + beginPos + pipelineTag.size(), begin(pipelinesText) + ((endPos != std::string::npos) ? endPos : pipelinesText.size()));

			m_jsonPipelines.insert(jsonPipeline);
		}
	}
}

const VkDeviceObjectReservationCreateInfo&	ResourceInterface::getMemoryReservation () const
{
	return m_memoryReservation;
}

std::size_t	ResourceInterface::getCacheDataSize() const
{
	return m_cacheData.size();
}

const deUint8* ResourceInterface::getCacheData() const
{
	return m_cacheData.data();
}

#endif // CTS_USES_VULKANSC

ResourceInterfaceStandard::ResourceInterfaceStandard (tcu::TestContext& testCtx)
	: ResourceInterface(testCtx)
{
}

void ResourceInterfaceStandard::initDevice(DeviceInterface& deviceInterface, VkDevice device)
{
	// ResourceInterfaceStandard is a class for running VulkanSC tests on normal Vulkan driver.
	// CTS does not have vkCreateShaderModule function defined for Vulkan SC driver, but we need this function
	// So ResourceInterfaceStandard class must have its own vkCreateShaderModule function pointer
	// Moreover - we create additional function pointers for vkCreateGraphicsPipelines, vkCreateComputePipelines, etc.
	// BTW: although ResourceInterfaceStandard exists in normal Vulkan tests - only initDevice and buildProgram functions are used by Vulkan tests
	// Other functions are called from within DeviceDriverSC which does not exist in these tests ( DeviceDriver class is used instead )
	m_shaderCounter				= 0U;
	m_createShaderModuleFunc		= (CreateShaderModuleFunc)		deviceInterface.getDeviceProcAddr(device, "vkCreateShaderModule");
	m_createGraphicsPipelinesFunc	= (CreateGraphicsPipelinesFunc)	deviceInterface.getDeviceProcAddr(device, "vkCreateGraphicsPipelines");
	m_createComputePipelinesFunc	= (CreateComputePipelinesFunc)	deviceInterface.getDeviceProcAddr(device, "vkCreateComputePipelines");
#ifdef CTS_USES_VULKANSC
	if (m_testCtx.getCommandLine().isSubProcess())
	{
		VkPipelineCacheCreateInfo pCreateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,	// VkStructureType				sType;
			DE_NULL,										// const void*					pNext;
			(VkPipelineCacheCreateFlags)0u,					// VkPipelineCacheCreateFlags	flags;
			m_cacheData.size(),								// deUintptr					initialDataSize;
			m_cacheData.data()								// const void*					pInitialData;
		};
		m_pipelineCache = createPipelineCache(deviceInterface, device, &pCreateInfo, DE_NULL);
	}
#endif // CTS_USES_VULKANSC
}

void ResourceInterfaceStandard::deinitDevice()
{
#ifdef CTS_USES_VULKANSC
	m_cacheData.clear();

	if (m_testCtx.getCommandLine().isSubProcess())
	{
		m_pipelineCache.disown();
	}
#endif // CTS_USES_VULKANSC
}

#ifdef CTS_USES_VULKANSC

VkResult ResourceInterfaceStandard::createShaderModule (VkDevice							device,
														const VkShaderModuleCreateInfo*		pCreateInfo,
														const VkAllocationCallbacks*		pAllocator,
														VkShaderModule*						pShaderModule,
														bool								normalMode) const
{
	std::lock_guard<std::mutex> lock(m_pipelineMutex);

	if(normalMode)
		return m_createShaderModuleFunc(device, pCreateInfo, pAllocator, pShaderModule);

	// main process: store VkShaderModuleCreateInfo in JSON format. Shaders will be sent later for m_pipelineCache creation ( and sent through file to another process )
	*pShaderModule = VkShaderModule(++m_shaderCounter);
	m_jsonShaderModules.insert({ *pShaderModule, writeJSON_VkShaderModuleCreateInfo(*pCreateInfo) });
	return VK_SUCCESS;
}

VkPipelineIdentifierInfo makeGraphicsPipelineIdentifier (const std::string& testPath, const VkGraphicsPipelineCreateInfo& gpCI)
{
	VkPipelineIdentifierInfo	pipelineID		= resetPipelineIdentifierInfo();
	std::string					jsonPipeline	= testPath + ":" + writeJSON_VkGraphicsPipelineCreateInfo(gpCI);
	std::size_t					hashValue		= std::hash<std::string>{}(jsonPipeline);
	memcpy(pipelineID.pipelineIdentifier, &hashValue, sizeof(std::size_t));
	return pipelineID;
}

VkPipelineIdentifierInfo makeComputePipelineIdentifier (const std::string& testPath, const VkComputePipelineCreateInfo& cpCI)
{
	VkPipelineIdentifierInfo	pipelineID		= resetPipelineIdentifierInfo();
	std::string					jsonPipeline	= testPath + ":" + writeJSON_VkComputePipelineCreateInfo(cpCI);
	std::size_t					hashValue		= std::hash<std::string>{}(jsonPipeline);
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
	// TODO:
	// - VkPipelinePoolEntrySizeCreateInfo is missing
	DE_UNREF(pipelineCache);

	std::lock_guard<std::mutex> lock(m_pipelineMutex);

	// build pipeline identifiers, make a copy of pCreateInfos
	std::vector<VkPipelineIdentifierInfo>		pipelineIDs;
	std::vector<VkGraphicsPipelineCreateInfo>	pCreateInfoCopies;
	for (deUint32 i = 0; i < createInfoCount; ++i)
	{
		pipelineIDs.push_back(makeGraphicsPipelineIdentifier(m_currentTestPath, pCreateInfos[i]));
		pCreateInfoCopies.push_back(pCreateInfos[i]);
	}

	// include pipelineIdentifiers into pNext chain of pCreateInfoCopies
	for (deUint32 i = 0; i < createInfoCount; ++i)
	{
		pipelineIDs[i].pNext		= pCreateInfoCopies[i].pNext;
		pCreateInfoCopies[i].pNext	= &pipelineIDs[i];
	}

	// subprocess: load graphics pipelines from OUR m_pipelineCache cache
	if(normalMode)
		return m_createGraphicsPipelinesFunc(device, *m_pipelineCache, createInfoCount, pCreateInfoCopies.data(), pAllocator, pPipelines);

	// main process: store pipelines in JSON format. Pipelines will be sent later for m_pipelineCache creation ( and sent through file to another process )
	for (deUint32 i = 0; i < createInfoCount; ++i)
	{
		m_jsonPipelines.insert(writeJSON_VkGraphicsPipelineCreateInfo(pCreateInfoCopies[i]));
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
	// TODO:
	// - VkPipelinePoolEntrySizeCreateInfo is missing

	std::lock_guard<std::mutex> lock(m_pipelineMutex);

	// build pipeline identifiers, make a copy of pCreateInfos
	std::vector<VkPipelineIdentifierInfo>		pipelineIDs;
	std::vector<VkComputePipelineCreateInfo>	pCreateInfoCopies;
	for (deUint32 i = 0; i < createInfoCount; ++i)
	{
		pipelineIDs.push_back(makeComputePipelineIdentifier(m_currentTestPath, pCreateInfos[i]));
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
		return m_createComputePipelinesFunc(device, *m_pipelineCache, createInfoCount, pCreateInfoCopies.data(), pAllocator, pPipelines);

	// main process: store pipelines in JSON format. Pipelines will be sent later for m_pipelineCache creation ( and sent through file to another process )
	for (deUint32 i = 0; i < createInfoCount; ++i)
	{
		m_jsonPipelines.insert(writeJSON_VkComputePipelineCreateInfo(pCreateInfoCopies[i]));
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
	std::lock_guard<std::mutex> lock(m_pipelineMutex);
	m_jsonRenderPasses.insert({ *pRenderPass,  writeJSON_VkRenderPassCreateInfo(*pCreateInfo) });
}

void ResourceInterfaceStandard::createRenderPass2 (VkDevice								device,
												   const VkRenderPassCreateInfo2*		pCreateInfo,
												   const VkAllocationCallbacks*			pAllocator,
												   VkRenderPass*						pRenderPass) const
{
	DE_UNREF(device);
	DE_UNREF(pAllocator);
	std::lock_guard<std::mutex> lock(m_pipelineMutex);
	m_jsonRenderPasses.insert({ *pRenderPass,  writeJSON_VkRenderPassCreateInfo2(*pCreateInfo) });
}

void ResourceInterfaceStandard::createPipelineLayout (VkDevice								device,
													  const VkPipelineLayoutCreateInfo*		pCreateInfo,
													  const VkAllocationCallbacks*			pAllocator,
													  VkPipelineLayout*						pPipelineLayout) const
{
	DE_UNREF(device);
	DE_UNREF(pAllocator);
	std::lock_guard<std::mutex> lock(m_pipelineMutex);
	m_jsonPipelineLayouts.insert({*pPipelineLayout, writeJSON_VkPipelineLayoutCreateInfo(*pCreateInfo) });
}

void ResourceInterfaceStandard::createDescriptorSetLayout (VkDevice									device,
														   const VkDescriptorSetLayoutCreateInfo*	pCreateInfo,
														   const VkAllocationCallbacks*				pAllocator,
														   VkDescriptorSetLayout*					pSetLayout) const
{
	DE_UNREF(device);
	DE_UNREF(pAllocator);
	std::lock_guard<std::mutex> lock(m_pipelineMutex);
	m_jsonDescriptorSetLayouts.insert({ *pSetLayout, writeJSON_VkDescriptorSetLayoutCreateInfo(*pCreateInfo) });
}

void ResourceInterfaceStandard::createSampler (VkDevice							device,
											   const VkSamplerCreateInfo*		pCreateInfo,
											   const VkAllocationCallbacks*		pAllocator,
											   VkSampler*						pSampler) const
{
	DE_UNREF(device);
	DE_UNREF(pAllocator);
	std::lock_guard<std::mutex> lock(m_pipelineMutex);
	m_jsonSamplers.insert({ *pSampler,  writeJSON_VkSamplerCreateInfo(*pCreateInfo) });
}

void ResourceInterfaceStandard::importPipelineCacheData(const PlatformInterface&			vkp,
														VkInstance							instance,
														const InstanceInterface&			vki,
														VkPhysicalDevice					physicalDevice,
														deUint32							queueIndex,
														const VkPhysicalDeviceFeatures2&	enabledFeatures,
														const std::vector<const char*>		extensionPtrs)
{
	// to create a cache - we have to have working VkDevice
	const float						queuePriority			= 1.0f;
	const VkDeviceQueueCreateInfo	deviceQueueCreateInfo	=
	{
		VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		DE_NULL,
		(VkDeviceQueueCreateFlags)0u,
		queueIndex,							//queueFamilyIndex;
		1,									//queueCount;
		&queuePriority,						//pQueuePriorities;
	};

	const VkDeviceCreateInfo		deviceCreateInfo		=
	{
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,	//sType;
		&m_memoryReservation,					//pNext;
		(VkDeviceCreateFlags)0u,
		1,															//queueRecordCount;
		&deviceQueueCreateInfo,										//pRequestedQueues;
		0,															//layerCount;
		DE_NULL,													//ppEnabledLayerNames;
		(deUint32)extensionPtrs.size(),								//extensionCount;
		extensionPtrs.data(),										//ppEnabledExtensionNames;
		enabledFeatures.pNext ? DE_NULL : &enabledFeatures.features	//pEnabledFeatures;
	};

	Move<VkDevice> pcDevice = createDevice(vkp, instance, vki, physicalDevice, &deviceCreateInfo);

	// create local function pointers required to perform pipeline cache creation
	GetDeviceProcAddrFunc			getDeviceProcAddrFunc			= (GetDeviceProcAddrFunc)			vkp.getInstanceProcAddr	(instance, "vkGetDeviceProcAddr");
	CreateSamplerFunc				createSamplerFunc				= (CreateSamplerFunc)				getDeviceProcAddrFunc	(*pcDevice, "vkCreateSampler");
	DestroySamplerFunc				destroySamplerFunc				= (DestroySamplerFunc)				getDeviceProcAddrFunc	(*pcDevice, "vkDestroySampler");
	CreateShaderModuleFunc			createShaderModuleFunc			= (CreateShaderModuleFunc)			getDeviceProcAddrFunc	(*pcDevice, "vkCreateShaderModule");
	DestroyShaderModuleFunc			destroyShaderModuleFunc			= (DestroyShaderModuleFunc)			getDeviceProcAddrFunc	(*pcDevice, "vkDestroyShaderModule");
	CreateRenderPassFunc			createRenderPassFunc			= (CreateRenderPassFunc)			getDeviceProcAddrFunc	(*pcDevice, "vkCreateRenderPass");
	CreateRenderPass2Func			createRenderPass2Func			= (CreateRenderPass2Func)			getDeviceProcAddrFunc	(*pcDevice, "vkCreateRenderPass2");
	DestroyRenderPassFunc			destroyRenderPassFunc			= (DestroyRenderPassFunc)			getDeviceProcAddrFunc	(*pcDevice, "vkDestroyRenderPass");
	CreateDescriptorSetLayoutFunc	createDescriptorSetLayoutFunc	= (CreateDescriptorSetLayoutFunc)	getDeviceProcAddrFunc	(*pcDevice, "vkCreateDescriptorSetLayout");
	DestroyDescriptorSetLayoutFunc	destroyDescriptorSetLayoutFunc	= (DestroyDescriptorSetLayoutFunc)	getDeviceProcAddrFunc	(*pcDevice, "vkDestroyDescriptorSetLayout");
	CreatePipelineLayoutFunc		createPipelineLayoutFunc		= (CreatePipelineLayoutFunc)		getDeviceProcAddrFunc	(*pcDevice, "vkCreatePipelineLayout");
	DestroyPipelineLayoutFunc		destroyPipelineLayoutFunc		= (DestroyPipelineLayoutFunc)		getDeviceProcAddrFunc	(*pcDevice, "vkDestroyPipelineLayout");
	CreateGraphicsPipelinesFunc		createGraphicsPipelinesFunc		= (CreateGraphicsPipelinesFunc)		getDeviceProcAddrFunc	(*pcDevice, "vkCreateGraphicsPipelines");
	CreateComputePipelinesFunc		createComputePipelinesFunc		= (CreateComputePipelinesFunc)		getDeviceProcAddrFunc	(*pcDevice, "vkCreateComputePipelines");
	CreatePipelineCacheFunc			createPipelineCacheFunc			= (CreatePipelineCacheFunc)			getDeviceProcAddrFunc	(*pcDevice, "vkCreatePipelineCache");
	DestroyPipelineCacheFunc		destroyPipelineCacheFunc		= (DestroyPipelineCacheFunc)		getDeviceProcAddrFunc	(*pcDevice, "vkDestroyPipelineCache");
	DestroyPipelineFunc				destroyPipelineFunc				= (DestroyPipelineFunc)				getDeviceProcAddrFunc	(*pcDevice, "vkDestroyPipeline");
	GetPipelineCacheDataFunc		getPipelineCacheDataFunc		= (GetPipelineCacheDataFunc)		getDeviceProcAddrFunc	(*pcDevice, "vkGetPipelineCacheData");

	// create pipeline cache
	VkPipelineCacheCreateInfo		pcCI =
	{
		VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,	// VkStructureType				sType;
		DE_NULL,										// const void*					pNext;
		(VkPipelineCacheCreateFlags)0u,					// VkPipelineCacheCreateFlags	flags;
		0u,												// size_t						initialDataSize;
		DE_NULL											// const void*					pInitialData;
	};

	VkPipelineCache	pipelineCache;
	VK_CHECK(createPipelineCacheFunc(*pcDevice, &pcCI, DE_NULL, &pipelineCache));

	Json::CharReaderBuilder			builder;
	de::UniquePtr<Json::CharReader>	jsonReader(builder.newCharReader());

	// decode VkSamplerCreateInfo structs and create VkSamplers
	std::map<VkSampler, VkSampler> falseToRealSamplers;
	for (auto it = begin(m_jsonSamplers); it != end(m_jsonSamplers); ++it)
	{
		VkSamplerCreateInfo			sCI;
		deMemset(&sCI, 0, sizeof(sCI));
		readJSON_VkSamplerCreateInfo(jsonReader.get(), it->second, sCI);
		VkSampler				realSampler;
		VK_CHECK(createSamplerFunc(*pcDevice, &sCI, DE_NULL, &realSampler));
		falseToRealSamplers.insert({ it->first, realSampler });
	}

	// decode VkShaderModuleCreateInfo structs and create VkShaderModules
	std::map<VkShaderModule, VkShaderModule> falseToRealShaderModules;
	for (auto it = begin(m_jsonShaderModules); it != end(m_jsonShaderModules); ++it)
	{
		VkShaderModuleCreateInfo	smCI;
		deMemset(&smCI, 0, sizeof(smCI));
		std::vector<deUint8>		spirvShader;
		readJSON_VkShaderModuleCreateInfo(jsonReader.get(), it->second, smCI, spirvShader);
		VkShaderModule				realShaderModule;
		VK_CHECK(createShaderModuleFunc(*pcDevice, &smCI, DE_NULL, &realShaderModule));
		falseToRealShaderModules.insert({ it->first, realShaderModule });
	}

	// decode renderPass structs and create VkRenderPasses
	std::map<VkRenderPass, VkRenderPass> falseToRealRenderPasses;
	for (auto it = begin(m_jsonRenderPasses); it != end(m_jsonRenderPasses); ++it)
	{
		if (it->second.find("VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2") != std::string::npos)
		{
			VkRenderPassCreateInfo2	rpCI;
			deMemset(&rpCI, 0, sizeof(rpCI));
			readJSON_VkRenderPassCreateInfo2(jsonReader.get(), it->second, rpCI);
			VkRenderPass				realRenderPass;
			VK_CHECK(createRenderPass2Func(*pcDevice, &rpCI, DE_NULL, &realRenderPass));
			falseToRealRenderPasses.insert({ it->first, realRenderPass });
		}
		else if (it->second.find("VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO") != std::string::npos)
		{
			VkRenderPassCreateInfo	rpCI;
			deMemset(&rpCI, 0, sizeof(rpCI));
			readJSON_VkRenderPassCreateInfo(jsonReader.get(), it->second, rpCI);
			VkRenderPass				realRenderPass;
			VK_CHECK(createRenderPassFunc(*pcDevice, &rpCI, DE_NULL, &realRenderPass));
			falseToRealRenderPasses.insert({ it->first, realRenderPass });
		}
		else
			TCU_THROW(InternalError, "Could not recognize render pass type");
	}

	// decode descriptor set layout structs and create VkDescriptorSetLayouts
	std::map<VkDescriptorSetLayout, VkDescriptorSetLayout> falseToRealDescriptorSetLayouts;
	for (auto it = begin(m_jsonDescriptorSetLayouts); it != end(m_jsonDescriptorSetLayouts); ++it)
	{
		VkDescriptorSetLayoutCreateInfo	dsCI;
		deMemset(&dsCI, 0, sizeof(dsCI));
		readJSON_VkDescriptorSetLayoutCreateInfo(jsonReader.get(), it->second, dsCI);
		std::vector<VkDescriptorSetLayoutBinding> newDescriptorBindings;
		std::vector<std::vector<VkSampler>> realSamplers;

		// we have to replace all bindings if there is any immutable sampler defined
		bool needReplaceSamplers = false;
		for (deUint32 i = 0; i < dsCI.bindingCount; ++i)
		{
			if (dsCI.pBindings[i].pImmutableSamplers != DE_NULL)
				needReplaceSamplers = true;
		}

		if (needReplaceSamplers)
		{
			for (deUint32 i = 0; i < dsCI.bindingCount; ++i)
			{
				if (dsCI.pBindings[i].pImmutableSamplers == DE_NULL)
				{
					newDescriptorBindings.push_back(dsCI.pBindings[i]);
					continue;
				}

				realSamplers.push_back(std::vector<VkSampler>{dsCI.pBindings[i].descriptorCount});
				for (deUint32 j = 0; j < dsCI.pBindings[i].descriptorCount; ++j)
				{
					if (dsCI.pBindings[i].pImmutableSamplers[j] == DE_NULL)
					{
						realSamplers.back()[j] = DE_NULL;
						continue;
					}
					else
					{
						auto jt = falseToRealSamplers.find(dsCI.pBindings[i].pImmutableSamplers[j]);
						if (jt == end(falseToRealSamplers))
							TCU_THROW(InternalError, "VkSampler not found");
						realSamplers.back()[j] = jt->second;
					}
				}
				VkDescriptorSetLayoutBinding bCopy =
				{
					dsCI.pBindings[i].binding,			// deUint32				binding;
					dsCI.pBindings[i].descriptorType,	// VkDescriptorType		descriptorType;
					dsCI.pBindings[i].descriptorCount,	// deUint32				descriptorCount;
					dsCI.pBindings[i].stageFlags,		// VkShaderStageFlags	stageFlags;
					realSamplers.back().data()			// const VkSampler*		pImmutableSamplers;
				};
				newDescriptorBindings.push_back(bCopy);
			}
			dsCI.pBindings = newDescriptorBindings.data();
		}

		VkDescriptorSetLayout				realDescriptorSetLayout;
		VK_CHECK(createDescriptorSetLayoutFunc(*pcDevice, &dsCI, DE_NULL, &realDescriptorSetLayout));
		falseToRealDescriptorSetLayouts.insert({ it->first, realDescriptorSetLayout });
	}

	// decode pipeline layout structs and create VkPipelineLayouts. Requires creation of new pSetLayouts to bypass constness
	std::map<VkPipelineLayout, VkPipelineLayout> falseToRealPipelineLayouts;
	for (auto it = begin(m_jsonPipelineLayouts); it != end(m_jsonPipelineLayouts); ++it)
	{
		VkPipelineLayoutCreateInfo	plCI;
		deMemset(&plCI, 0, sizeof(plCI));
		readJSON_VkPipelineLayoutCreateInfo(jsonReader.get(), it->second, plCI);
		// replace descriptor set layouts with real ones
		std::vector<VkDescriptorSetLayout> newSetLayouts;
		for (deUint32 i = 0; i < plCI.setLayoutCount; ++i)
		{
			auto jt = falseToRealDescriptorSetLayouts.find(plCI.pSetLayouts[i]);
			if (jt == end(falseToRealDescriptorSetLayouts))
				TCU_THROW(InternalError, "VkDescriptorSetLayout not found");
			newSetLayouts.push_back(jt->second);
		}
		plCI.pSetLayouts = newSetLayouts.data();

		VkPipelineLayout				realPipelineLayout;
		VK_CHECK(createPipelineLayoutFunc(*pcDevice, &plCI, DE_NULL, &realPipelineLayout));
		falseToRealPipelineLayouts.insert({ it->first, realPipelineLayout });
	}

	// decode VkGraphicsPipelineCreateInfo and VkComputePipelineCreateInfo structs and create VkPipelines with a given pipeline cache
	std::vector<VkPipeline>	realPipelines;
	for (auto it = begin(m_jsonPipelines); it != end(m_jsonPipelines); ++it)
	{
		if (it->find("VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO") != std::string::npos)
		{
			VkGraphicsPipelineCreateInfo	gpCI;
			deMemset(&gpCI, 0, sizeof(gpCI));
			readJSON_VkGraphicsPipelineCreateInfo(jsonReader.get(), *it, gpCI);

			// replace VkShaderModules with real ones. Requires creation of new pStages to bypass constness
			std::vector<VkPipelineShaderStageCreateInfo> newStages;
			for (deUint32 i = 0; i < gpCI.stageCount; ++i)
			{
				VkPipelineShaderStageCreateInfo newStage = gpCI.pStages[i];
				auto jt = falseToRealShaderModules.find(gpCI.pStages[i].module);
				if(jt == end(falseToRealShaderModules))
					TCU_THROW(InternalError, "VkShaderModule not found");
				newStage.module = jt->second;
				newStages.push_back(newStage);
			}
			gpCI.pStages = newStages.data();

			// replace render pass with a real one
			{
				auto jt = falseToRealRenderPasses.find(gpCI.renderPass);
				if (jt == end(falseToRealRenderPasses))
					TCU_THROW(InternalError, "VkRenderPass not found");
				gpCI.renderPass = jt->second;
			}

			// replace pipeline layout with a real one
			{
				auto jt = falseToRealPipelineLayouts.find(gpCI.layout);
				if (jt == end(falseToRealPipelineLayouts))
					TCU_THROW(InternalError, "VkPipelineLayout not found");
				gpCI.layout = jt->second;
			}

			VkPipeline						gPipeline;
			VK_CHECK(createGraphicsPipelinesFunc(*pcDevice, pipelineCache, 1, &gpCI, DE_NULL, &gPipeline));
			realPipelines.push_back(gPipeline);
		}
		else if (it->find("VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO") != std::string::npos)
		{
			VkComputePipelineCreateInfo	cpCI;
			deMemset(&cpCI, 0, sizeof(cpCI));
			readJSON_VkComputePipelineCreateInfo(jsonReader.get(), *it, cpCI);
			// replace VkShaderModules with real ones
			{
				auto jt = falseToRealShaderModules.find(cpCI.stage.module);
				if(jt == end(falseToRealShaderModules))
					TCU_THROW(InternalError, "VkShaderModule not found");
				cpCI.stage.module = jt->second;
			}

			// replace pipeline layout with a real one
			{
				auto jt = falseToRealPipelineLayouts.find(cpCI.layout);
				if (jt == end(falseToRealPipelineLayouts))
					TCU_THROW(InternalError, "VkPipelineLayout not found");
				cpCI.layout = jt->second;
			}

			VkPipeline						cPipeline;
			VK_CHECK(createComputePipelinesFunc(*pcDevice, pipelineCache, 1, &cpCI, DE_NULL, &cPipeline));
			realPipelines.push_back(cPipeline);
		}
		else
			TCU_THROW(InternalError, "Could not recognize pipeline type");
	}

	// getPipelineCacheData() binary data, store it in m_cacheData
	std::size_t cacheSize;
	VK_CHECK(getPipelineCacheDataFunc(*pcDevice, pipelineCache, &cacheSize, DE_NULL));
	m_cacheData.resize(cacheSize);
	VK_CHECK(getPipelineCacheDataFunc(*pcDevice, pipelineCache, &cacheSize, m_cacheData.data()));

	// clean up resources - in ResourceInterfaceStandard we just simulate Vulkan SC driver after all...
	for (auto pipeline : realPipelines)
		destroyPipelineFunc(*pcDevice, pipeline, DE_NULL);
	for (auto it : falseToRealPipelineLayouts)
		destroyPipelineLayoutFunc(*pcDevice, it.second, DE_NULL);
	for (auto it : falseToRealDescriptorSetLayouts)
		destroyDescriptorSetLayoutFunc(*pcDevice, it.second, DE_NULL);
	for (auto it : falseToRealRenderPasses)
		destroyRenderPassFunc(*pcDevice, it.second, DE_NULL);
	for (auto it : falseToRealShaderModules)
		destroyShaderModuleFunc(*pcDevice, it.second, DE_NULL);
	for (auto it : falseToRealSamplers)
		destroySamplerFunc(*pcDevice, it.second, DE_NULL);
	destroyPipelineCacheFunc(*pcDevice, pipelineCache, DE_NULL);
}

void ResourceInterfaceStandard::resetObjects()
{
	m_shaderCounter = 0u;
	m_jsonSamplers.clear();
	m_jsonShaderModules.clear();
	m_jsonRenderPasses.clear();
	m_jsonPipelineLayouts.clear();
	m_jsonDescriptorSetLayouts.clear();
	m_jsonPipelines.clear();

	m_memoryReservation = resetDeviceObjectReservationCreateInfo();

	vk_json_parser::s_globalMem.clear();
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

// JSON functions

#ifdef CTS_USES_VULKANSC

static std::mutex jsonMutex;

std::string writeJSON_VkGraphicsPipelineCreateInfo(const VkGraphicsPipelineCreateInfo&	pCreateInfo)
{
	std::lock_guard<std::mutex> lock(jsonMutex);
	vk_json::_string_stream.str("");
	vk_json::_string_stream.clear();
	vk_json::print_VkGraphicsPipelineCreateInfo(pCreateInfo, "", 0);
	return vk_json::_string_stream.str();
}

std::string writeJSON_VkComputePipelineCreateInfo(const VkComputePipelineCreateInfo&	pCreateInfo)
{
	std::lock_guard<std::mutex> lock(jsonMutex);
	vk_json::_string_stream.str("");
	vk_json::_string_stream.clear();
	vk_json::print_VkComputePipelineCreateInfo(pCreateInfo, "", 0);
	return vk_json::_string_stream.str();
}

std::string writeJSON_VkRenderPassCreateInfo (const VkRenderPassCreateInfo& pCreateInfo)
{
	std::lock_guard<std::mutex> lock(jsonMutex);
	vk_json::_string_stream.str("");
	vk_json::_string_stream.clear();
	vk_json::print_VkRenderPassCreateInfo(&pCreateInfo, "", false);
	return vk_json::_string_stream.str();
}

std::string writeJSON_VkRenderPassCreateInfo2 (const VkRenderPassCreateInfo2& pCreateInfo)
{
	std::lock_guard<std::mutex> lock(jsonMutex);
	vk_json::_string_stream.str("");
	vk_json::_string_stream.clear();
	vk_json::print_VkRenderPassCreateInfo2(&pCreateInfo, "", false);
	return vk_json::_string_stream.str();
}

std::string writeJSON_VkPipelineLayoutCreateInfo (const VkPipelineLayoutCreateInfo& pCreateInfo)
{
	std::lock_guard<std::mutex> lock(jsonMutex);
	vk_json::_string_stream.str("");
	vk_json::_string_stream.clear();
	vk_json::print_VkPipelineLayoutCreateInfo(&pCreateInfo, "", false);
	return vk_json::_string_stream.str();
}

std::string writeJSON_VkDescriptorSetLayoutCreateInfo (const VkDescriptorSetLayoutCreateInfo& pCreateInfo)
{
	std::lock_guard<std::mutex> lock(jsonMutex);
	vk_json::_string_stream.str("");
	vk_json::_string_stream.clear();
	vk_json::print_VkDescriptorSetLayoutCreateInfo(&pCreateInfo, "", false);
	return vk_json::_string_stream.str();
}

std::string writeJSON_VkSamplerCreateInfo(const VkSamplerCreateInfo& pCreateInfo)
{
	std::lock_guard<std::mutex> lock(jsonMutex);
	vk_json::_string_stream.str("");
	vk_json::_string_stream.clear();
	vk_json::print_VkSamplerCreateInfo(&pCreateInfo, "", false);
	return vk_json::_string_stream.str();
}

std::string writeJSON_VkDeviceObjectReservationCreateInfo (const VkDeviceObjectReservationCreateInfo& dmrCI)
{
	std::lock_guard<std::mutex> lock(jsonMutex);
	vk_json::_string_stream.str("");
	vk_json::_string_stream.clear();
	vk_json::print_VkDeviceObjectReservationCreateInfo(&dmrCI, "", false);
	return vk_json::_string_stream.str();
}

static void print_VkShaderModuleCreateInfo (const VkShaderModuleCreateInfo* obj, const std::string& s, bool commaNeeded)
{
	DE_UNREF(s);

	for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
		vk_json::_string_stream << "{" << std::endl;
		vk_json::s_num_spaces += 4;

	vk_json::print_VkStructureType(obj->sType, "sType", 1);

	if (obj->pNext) {
		vk_json::dumpPNextChain(obj->pNext);
	}
	else {
		for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
			vk_json::_string_stream << "\"pNext\":" << "\"NULL\"" << "," << std::endl;
	}

	// VkShaderModuleCreateFlags is reserved for future use and must be 0.
	vk_json::print_uint32_t((deUint32)obj->flags, "flags", 1);
	vk_json::print_uint64_t((deUint64)obj->codeSize, "codeSize", 1);

	// pCode must be translated into base64, because JSON
	vk_json::print_void_data(obj->pCode, static_cast<int>(obj->codeSize), "pCode", 0);

	vk_json::s_num_spaces -= 4;
	for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
		if (commaNeeded)
			vk_json::_string_stream << "}," << std::endl;
		else
			vk_json::_string_stream << "}" << std::endl;
}

std::string writeJSON_VkShaderModuleCreateInfo (const VkShaderModuleCreateInfo& smCI)
{
	std::lock_guard<std::mutex> lock(jsonMutex);
	vk_json::_string_stream.str("");
	vk_json::_string_stream.clear();
	print_VkShaderModuleCreateInfo(&smCI, "", false);
	return vk_json::_string_stream.str();
}

void readJSON_VkGraphicsPipelineCreateInfo (Json::CharReader*				jsonReader,
											const std::string&				graphicsPipelineCreateInfo,
											VkGraphicsPipelineCreateInfo&	gpCI)
{
	Json::Value						jsonRoot;
	std::string						errors;
	bool							parsingSuccessful = jsonReader->parse(graphicsPipelineCreateInfo.c_str(), graphicsPipelineCreateInfo.c_str() + graphicsPipelineCreateInfo.size(), &jsonRoot, &errors);
	if (!parsingSuccessful)
		TCU_THROW(InternalError, (std::string("JSON parsing error: ") + errors).c_str());
	vk_json_parser::parse_VkGraphicsPipelineCreateInfo("", jsonRoot, gpCI);
}

void readJSON_VkComputePipelineCreateInfo (Json::CharReader*			jsonReader,
											const std::string&			computePipelineCreateInfo,
										   VkComputePipelineCreateInfo&	cpCI)
{
	Json::Value						jsonRoot;
	std::string						errors;
	bool							parsingSuccessful = jsonReader->parse(computePipelineCreateInfo.c_str(), computePipelineCreateInfo.c_str() + computePipelineCreateInfo.size(), &jsonRoot, &errors);
	if (!parsingSuccessful)
		TCU_THROW(InternalError, (std::string("JSON parsing error: ") + errors).c_str());
	vk_json_parser::parse_VkComputePipelineCreateInfo("", jsonRoot, cpCI);
}

void readJSON_VkRenderPassCreateInfo (Json::CharReader*			jsonReader,
									  const std::string&		renderPassCreateInfo,
									  VkRenderPassCreateInfo&	rpCI)
{
	Json::Value						jsonRoot;
	std::string						errors;
	bool							parsingSuccessful = jsonReader->parse(renderPassCreateInfo.c_str(), renderPassCreateInfo.c_str() + renderPassCreateInfo.size(), &jsonRoot, &errors);
	if (!parsingSuccessful)
		TCU_THROW(InternalError, (std::string("JSON parsing error: ") + errors).c_str());
	vk_json_parser::parse_VkRenderPassCreateInfo("", jsonRoot, rpCI);
}

void readJSON_VkRenderPassCreateInfo2 (Json::CharReader*		jsonReader,
									   const std::string&		renderPassCreateInfo,
									   VkRenderPassCreateInfo2&	rpCI)
{
	Json::Value						jsonRoot;
	std::string						errors;
	bool							parsingSuccessful = jsonReader->parse(renderPassCreateInfo.c_str(), renderPassCreateInfo.c_str() + renderPassCreateInfo.size(), &jsonRoot, &errors);
	if (!parsingSuccessful)
		TCU_THROW(InternalError, (std::string("JSON parsing error: ") + errors).c_str());
	vk_json_parser::parse_VkRenderPassCreateInfo2("", jsonRoot, rpCI);
}

void readJSON_VkDescriptorSetLayoutCreateInfo (Json::CharReader*				jsonReader,
											   const std::string&				descriptorSetLayoutCreateInfo,
											   VkDescriptorSetLayoutCreateInfo&	dsCI)
{
	Json::Value						jsonRoot;
	std::string						errors;
	bool							parsingSuccessful = jsonReader->parse(descriptorSetLayoutCreateInfo.c_str(), descriptorSetLayoutCreateInfo.c_str() + descriptorSetLayoutCreateInfo.size(), &jsonRoot, &errors);
	if (!parsingSuccessful)
		TCU_THROW(InternalError, (std::string("JSON parsing error: ") + errors).c_str());
	vk_json_parser::parse_VkDescriptorSetLayoutCreateInfo("", jsonRoot, dsCI);
}

void readJSON_VkPipelineLayoutCreateInfo (Json::CharReader*				jsonReader,
										  const std::string&			pipelineLayoutCreateInfo,
										  VkPipelineLayoutCreateInfo&	plCI)
{
	Json::Value						jsonRoot;
	std::string						errors;
	bool							parsingSuccessful = jsonReader->parse(pipelineLayoutCreateInfo.c_str(), pipelineLayoutCreateInfo.c_str() + pipelineLayoutCreateInfo.size(), &jsonRoot, &errors);
	if (!parsingSuccessful)
		TCU_THROW(InternalError, (std::string("JSON parsing error: ") + errors).c_str());
	vk_json_parser::parse_VkPipelineLayoutCreateInfo("", jsonRoot, plCI);
}

void readJSON_VkDeviceObjectReservationCreateInfo (Json::CharReader*					jsonReader,
												   const std::string&					deviceMemoryReservation,
												   VkDeviceObjectReservationCreateInfo&	dmrCI)
{
	Json::Value						jsonRoot;
	std::string						errors;
	bool							parsingSuccessful	= jsonReader->parse(deviceMemoryReservation.c_str(), deviceMemoryReservation.c_str() + deviceMemoryReservation.size(), &jsonRoot, &errors);
	if (!parsingSuccessful)
		TCU_THROW(InternalError, (std::string("JSON parsing error: ") + errors).c_str());
	vk_json_parser::parse_VkDeviceObjectReservationCreateInfo("", jsonRoot, dmrCI);
}

void readJSON_VkSamplerCreateInfo(Json::CharReader*		jsonReader,
								  const std::string&	samplerCreateInfo,
								  VkSamplerCreateInfo&	sCI)
{
	Json::Value						jsonRoot;
	std::string						errors;
	bool							parsingSuccessful = jsonReader->parse(samplerCreateInfo.c_str(), samplerCreateInfo.c_str() + samplerCreateInfo.size(), &jsonRoot, &errors);
	if (!parsingSuccessful)
		TCU_THROW(InternalError, (std::string("JSON parsing error: ") + errors).c_str());
	vk_json_parser::parse_VkSamplerCreateInfo("", jsonRoot, sCI);
}

static void parse_VkShaderModuleCreateInfo(const char* s, Json::Value& obj, VkShaderModuleCreateInfo& o, std::vector<deUint8>& spirvShader)
{
	DE_UNREF(s);

	vk_json_parser::parse_VkStructureType("sType", obj["sType"], (o.sType));

	o.pNext = (VkDeviceObjectReservationCreateInfo*)vk_json_parser::parsePNextChain(obj);

	vk_json_parser::parse_uint32_t("flags", obj["flags"], (o.flags));
	deUint64 codeSizeValue;
	vk_json_parser::parse_uint64_t("codeSize", obj["codeSize"], (codeSizeValue));
	o.codeSize = (deUintptr)codeSizeValue;

	// pCode is encoded using Base64.
	spirvShader = vk_json_parser::base64decode(obj["pCode"].asString());
	o.pCode = (deUint32*)spirvShader.data();
}

void readJSON_VkShaderModuleCreateInfo(Json::CharReader*			jsonReader,
									   const std::string&			shaderModuleCreate,
									   VkShaderModuleCreateInfo&	smCI,
									   std::vector<deUint8>&		spirvShader)
{
	Json::Value						jsonRoot;
	std::string						errors;
	bool							parsingSuccessful = jsonReader->parse(shaderModuleCreate.c_str(), shaderModuleCreate.c_str() + shaderModuleCreate.size(), &jsonRoot, &errors);
	if (!parsingSuccessful)
		TCU_THROW(InternalError, (std::string("JSON parsing error: ") + errors).c_str());
	parse_VkShaderModuleCreateInfo("", jsonRoot, smCI, spirvShader);
}


#endif // CTS_USES_VULKANSC

} // namespace vk
