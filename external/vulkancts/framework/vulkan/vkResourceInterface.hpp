#ifndef _VKRESOURCEINTERFACE_HPP
#define _VKRESOURCEINTERFACE_HPP
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

#include "vkDefs.hpp"
#include "tcuTestLog.hpp"
#include "tcuTestContext.hpp"
#include "vkPrograms.hpp"
#include "vkBinaryRegistry.hpp"
#ifdef CTS_USES_VULKANSC
	#include <set>
	#include <mutex>
	namespace Json
	{
		class CharReader;
	}
#endif // CTS_USES_VULKANSC

namespace vk
{

class ResourceInterface
{
public:
								ResourceInterface			(tcu::TestContext&					testCtx);
	virtual						~ResourceInterface			();

	virtual void				initDevice					(DeviceInterface&					deviceInterface,
															 VkDevice							device) = 0;
	virtual void				deinitDevice				() = 0;

	virtual void				initTestCase				(const std::string&					casePath);

	// buildProgram
	template <typename InfoType, typename IteratorType>
	vk::ProgramBinary*			buildProgram				(const std::string&						casePath,
															 IteratorType							iter,
															 const vk::BinaryRegistryReader&		prebuiltBinRegistry,
															 vk::BinaryCollection*					progCollection);

#ifdef CTS_USES_VULKANSC
	virtual VkResult			createShaderModule			(VkDevice								device,
															 const VkShaderModuleCreateInfo*		pCreateInfo,
															 const VkAllocationCallbacks*			pAllocator,
															 VkShaderModule*						pShaderModule,
															 bool									normalMode) const = 0;
	virtual VkResult			createGraphicsPipelines		(VkDevice								device,
															 VkPipelineCache						pipelineCache,
															 deUint32 createInfoCount,
															 const VkGraphicsPipelineCreateInfo*	pCreateInfos,
															 const VkAllocationCallbacks*			pAllocator,
															 VkPipeline*							pPipelines,
															 bool									normalMode) const = 0;
	virtual VkResult			createComputePipelines		(VkDevice								device,
															 VkPipelineCache						pipelineCache,
															 deUint32								createInfoCount,
															 const VkComputePipelineCreateInfo*		pCreateInfos,
															 const VkAllocationCallbacks*			pAllocator,
															 VkPipeline*							pPipelines,
															 bool									normalMode) const = 0;
	virtual void				createRenderPass			(VkDevice								device,
															 const VkRenderPassCreateInfo*			pCreateInfo,
															 const VkAllocationCallbacks*			pAllocator,
															 VkRenderPass*							pRenderPass) const = 0;
	virtual void				createRenderPass2			(VkDevice								device,
															 const VkRenderPassCreateInfo2*			pCreateInfo,
															 const VkAllocationCallbacks*			pAllocator,
															 VkRenderPass*							pRenderPass) const = 0;
	virtual void				createPipelineLayout		(VkDevice								device,
															 const VkPipelineLayoutCreateInfo*		pCreateInfo,
															 const VkAllocationCallbacks*			pAllocator,
															 VkPipelineLayout*						pPipelineLayout) const = 0;
	virtual void				createDescriptorSetLayout	(VkDevice								device,
															 const VkDescriptorSetLayoutCreateInfo*	pCreateInfo,
															 const VkAllocationCallbacks*			pAllocator,
															 VkDescriptorSetLayout*					pSetLayout) const = 0;
	virtual void				createSampler				(VkDevice								device,
															 const VkSamplerCreateInfo*				pCreateInfo,
															 const VkAllocationCallbacks*			pAllocator,
															 VkSampler*								pSampler) const = 0;

	void						removeRedundantObjects		();
	void						exportDataToFile			(const std::string&						fileName,
															 const std::string&						jsonMemoryReservation) const;
	void						importDataFromFile			(const std::string&						fileName) const;
	virtual void				importPipelineCacheData		(const PlatformInterface&				vkp,
															 VkInstance								instance,
															 const InstanceInterface&				vki,
															 VkPhysicalDevice						physicalDevice,
															 deUint32								queueIndex,
															 const VkPhysicalDeviceFeatures2&		enabledFeatures,
															 const std::vector<const char*>			extensionPtrs) = 0;
	const VkDeviceObjectReservationCreateInfo&
								getMemoryReservation		() const;
	std::size_t					getCacheDataSize			() const;
	const deUint8*				getCacheData				() const;
	virtual void				resetObjects				() = 0;
#endif // CTS_USES_VULKANSC

protected:
	virtual vk::ProgramBinary*	compileProgram				(const vk::ProgramIdentifier&			progId,
															 const vk::GlslSource&					source,
															 glu::ShaderProgramInfo*				buildInfo,
															 const tcu::CommandLine&				commandLine) = 0;
	virtual vk::ProgramBinary*	compileProgram				(const vk::ProgramIdentifier&			progId,
															 const vk::HlslSource&					source,
															 glu::ShaderProgramInfo*				buildInfo,
															 const tcu::CommandLine&				commandLine) = 0;
	virtual vk::ProgramBinary*	compileProgram				(const vk::ProgramIdentifier&			progId,
															 const vk::SpirVAsmSource&				source,
															 vk::SpirVProgramInfo*					buildInfo,
															 const tcu::CommandLine&				commandLine) = 0;

	tcu::TestContext&			m_testCtx;
	std::string					m_currentTestPath;

#ifdef CTS_USES_VULKANSC
	mutable std::mutex										m_pipelineMutex;
	mutable std::map<VkSampler, std::string>				m_jsonSamplers;
	mutable std::map<VkShaderModule, std::string>			m_jsonShaderModules;
	mutable std::map<VkRenderPass, std::string>				m_jsonRenderPasses;
	mutable	std::map<VkPipelineLayout, std::string>			m_jsonPipelineLayouts;
	mutable	std::map<VkDescriptorSetLayout, std::string>	m_jsonDescriptorSetLayouts;
	mutable std::set<std::string>							m_jsonPipelines;

	mutable VkDeviceObjectReservationCreateInfo				m_memoryReservation;
	Move<VkPipelineCache>									m_pipelineCache;
	std::vector<deUint8>									m_cacheData;
#endif // CTS_USES_VULKANSC
};

typedef VKAPI_ATTR VkResult	(VKAPI_CALL* CreateSamplerFunc)					(VkDevice device, const VkSamplerCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSampler* pSampler);
typedef VKAPI_ATTR void		(VKAPI_CALL* DestroySamplerFunc)				(VkDevice device, VkSampler sampler, const VkAllocationCallbacks* pAllocator);
typedef VKAPI_ATTR VkResult	(VKAPI_CALL* CreateShaderModuleFunc)			(VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkShaderModule* pShaderModule);
typedef VKAPI_ATTR void		(VKAPI_CALL* DestroyShaderModuleFunc)			(VkDevice device, VkShaderModule shaderModule, const VkAllocationCallbacks* pAllocator);
typedef VKAPI_ATTR VkResult	(VKAPI_CALL* CreateRenderPassFunc)				(VkDevice device, const VkRenderPassCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass);
typedef VKAPI_ATTR VkResult	(VKAPI_CALL* CreateRenderPass2Func)				(VkDevice device, const VkRenderPassCreateInfo2* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass);
typedef VKAPI_ATTR void		(VKAPI_CALL* DestroyRenderPassFunc)				(VkDevice device, VkRenderPass renderPass, const VkAllocationCallbacks* pAllocator);
typedef VKAPI_ATTR VkResult	(VKAPI_CALL* CreateDescriptorSetLayoutFunc)		(VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorSetLayout* pSetLayout);
typedef VKAPI_ATTR void		(VKAPI_CALL* DestroyDescriptorSetLayoutFunc)	(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const VkAllocationCallbacks* pAllocator);
typedef VKAPI_ATTR VkResult	(VKAPI_CALL* CreatePipelineLayoutFunc)			(VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineLayout* pPipelineLayout);
typedef VKAPI_ATTR void		(VKAPI_CALL* DestroyPipelineLayoutFunc)			(VkDevice device, VkPipelineLayout pipelineLayout, const VkAllocationCallbacks* pAllocator);
typedef VKAPI_ATTR VkResult	(VKAPI_CALL* CreateGraphicsPipelinesFunc)		(VkDevice device, VkPipelineCache pipelineCache, deUint32 createInfoCount, const VkGraphicsPipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines);
typedef VKAPI_ATTR VkResult	(VKAPI_CALL* CreateComputePipelinesFunc)		(VkDevice device, VkPipelineCache pipelineCache, deUint32 createInfoCount, const VkComputePipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines);
typedef VKAPI_ATTR void		(VKAPI_CALL* DestroyPipelineFunc)				(VkDevice device, VkPipeline pipeline, const VkAllocationCallbacks* pAllocator);
typedef VKAPI_ATTR VkResult	(VKAPI_CALL* CreatePipelineCacheFunc)			(VkDevice device, const VkPipelineCacheCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineCache* pPipelineCache);
typedef VKAPI_ATTR void		(VKAPI_CALL* DestroyPipelineCacheFunc)			(VkDevice device, VkPipelineCache pipelineCache, const VkAllocationCallbacks* pAllocator);
typedef VKAPI_ATTR VkResult	(VKAPI_CALL* GetPipelineCacheDataFunc)			(VkDevice device, VkPipelineCache pipelineCache, deUintptr* pDataSize, void* pData);

class ResourceInterfaceStandard : public ResourceInterface
{
public:
								ResourceInterfaceStandard	(tcu::TestContext&						testCtx);

	void						initDevice					(DeviceInterface&						deviceInterface,
															 VkDevice								device) override;
	void						deinitDevice				() override;
#ifdef CTS_USES_VULKANSC
	VkResult					createShaderModule			(VkDevice								device,
															 const VkShaderModuleCreateInfo*		pCreateInfo,
															 const VkAllocationCallbacks*			pAllocator,
															 VkShaderModule*						pShaderModule,
															 bool									normalMode) const override;
	VkResult					createGraphicsPipelines		(VkDevice								device,
															 VkPipelineCache						pipelineCache,
															 deUint32 createInfoCount,
															 const VkGraphicsPipelineCreateInfo*	pCreateInfos,
															 const VkAllocationCallbacks*			pAllocator,
															 VkPipeline*							pPipelines,
															 bool									normalMode) const override;
	VkResult					createComputePipelines		(VkDevice								device,
															 VkPipelineCache						pipelineCache,
															 deUint32								createInfoCount,
															 const VkComputePipelineCreateInfo*		pCreateInfos,
															 const VkAllocationCallbacks*			pAllocator,
															 VkPipeline*							pPipelines,
															 bool									normalMode) const override;
	void						createRenderPass			(VkDevice								device,
															 const VkRenderPassCreateInfo*			pCreateInfo,
															 const VkAllocationCallbacks*			pAllocator,
															 VkRenderPass*							pRenderPass) const override;
	void						createRenderPass2			(VkDevice								device,
															 const VkRenderPassCreateInfo2*			pCreateInfo,
															 const VkAllocationCallbacks*			pAllocator,
															 VkRenderPass*							pRenderPass) const override;
	void						createPipelineLayout		(VkDevice								device,
															 const VkPipelineLayoutCreateInfo*		pCreateInfo,
															 const VkAllocationCallbacks*			pAllocator,
															 VkPipelineLayout*						pPipelineLayout) const override;
	void						createDescriptorSetLayout	(VkDevice								device,
															 const VkDescriptorSetLayoutCreateInfo*	pCreateInfo,
															 const VkAllocationCallbacks*			pAllocator,
															 VkDescriptorSetLayout*					pSetLayout) const override;
	void						createSampler				(VkDevice								device,
															 const VkSamplerCreateInfo*				pCreateInfo,
															 const VkAllocationCallbacks*			pAllocator,
															 VkSampler*								pSampler) const override;

	void						importPipelineCacheData		(const PlatformInterface&				vkp,
															 VkInstance								instance,
															 const InstanceInterface&				vki,
															 VkPhysicalDevice						physicalDevice,
															 deUint32								queueIndex,
															 const VkPhysicalDeviceFeatures2&		enabledFeatures,
															 const std::vector<const char*>			extensionPtrs ) override;
	void						resetObjects				() override;

#endif // CTS_USES_VULKANSC

protected:
	vk::ProgramBinary*			compileProgram				(const vk::ProgramIdentifier&			progId,
															 const vk::GlslSource&					source,
															 glu::ShaderProgramInfo*				buildInfo,
															 const tcu::CommandLine&				commandLine) override;
	vk::ProgramBinary*			compileProgram				(const vk::ProgramIdentifier&			progId,
															 const vk::HlslSource&					source,
															 glu::ShaderProgramInfo*				buildInfo,
															 const tcu::CommandLine&				commandLine) override;
	vk::ProgramBinary*			compileProgram				(const vk::ProgramIdentifier&			progId,
															 const vk::SpirVAsmSource&				source,
															 vk::SpirVProgramInfo*					buildInfo,
															 const tcu::CommandLine&				commandLine) override;

	CreateShaderModuleFunc									m_createShaderModuleFunc;
	CreateGraphicsPipelinesFunc								m_createGraphicsPipelinesFunc;
	CreateComputePipelinesFunc								m_createComputePipelinesFunc;
	mutable deUint64										m_shaderCounter;
};

// JSON functions

#ifdef CTS_USES_VULKANSC
std::string						writeJSON_VkGraphicsPipelineCreateInfo			(const VkGraphicsPipelineCreateInfo&		pCreateInfo);
std::string						writeJSON_VkComputePipelineCreateInfo			(const VkComputePipelineCreateInfo&			pCreateInfo);
std::string						writeJSON_VkRenderPassCreateInfo				(const VkRenderPassCreateInfo&				pCreateInfo);
std::string						writeJSON_VkRenderPassCreateInfo2				(const VkRenderPassCreateInfo2&				pCreateInfo);
std::string						writeJSON_VkPipelineLayoutCreateInfo			(const VkPipelineLayoutCreateInfo&			pCreateInfo);
std::string						writeJSON_VkDescriptorSetLayoutCreateInfo		(const VkDescriptorSetLayoutCreateInfo&		pCreateInfo);
std::string						writeJSON_VkSamplerCreateInfo					(const VkSamplerCreateInfo&					pCreateInfo);
std::string						writeJSON_VkShaderModuleCreateInfo				(const VkShaderModuleCreateInfo&			smCI);
std::string						writeJSON_VkDeviceObjectReservationCreateInfo	(const VkDeviceObjectReservationCreateInfo&	dmrCI);

void							readJSON_VkGraphicsPipelineCreateInfo			(Json::CharReader*							jsonReader,
																				 const std::string&							graphicsPipelineCreateInfo,
																				 VkGraphicsPipelineCreateInfo&				gpCI);
void							readJSON_VkComputePipelineCreateInfo			(Json::CharReader*							jsonReader,
																				const std::string&							computePipelineCreateInfo,
																				 VkComputePipelineCreateInfo&				cpCI);
void							readJSON_VkRenderPassCreateInfo					(Json::CharReader*							jsonReader,
																				 const std::string&							renderPassCreateInfo,
																				 VkRenderPassCreateInfo&					rpCI);
void							readJSON_VkRenderPassCreateInfo2				(Json::CharReader*							jsonReader,
																				 const std::string&							renderPassCreateInfo,
																				 VkRenderPassCreateInfo2&					rpCI);
void							readJSON_VkDescriptorSetLayoutCreateInfo		(Json::CharReader*							jsonReader,
																				 const std::string&							descriptorSetLayoutCreateInfo,
																				 VkDescriptorSetLayoutCreateInfo&			dsCI);
void							readJSON_VkPipelineLayoutCreateInfo				(Json::CharReader*							jsonReader,
																				 const std::string&							pipelineLayoutCreateInfo,
																				 VkPipelineLayoutCreateInfo&				plCI);
void							readJSON_VkShaderModuleCreateInfo				(Json::CharReader*							jsonReader,
																				 const std::string&							shaderModuleCreate,
																				 VkShaderModuleCreateInfo&					smCI,
																				 std::vector<deUint8>&						spirvShader);
void							readJSON_VkDeviceObjectReservationCreateInfo	(Json::CharReader*							jsonReader,
																				 const std::string&							deviceMemoryReservation,
																				 VkDeviceObjectReservationCreateInfo&		dmrCI);
void							readJSON_VkSamplerCreateInfo					(Json::CharReader*							jsonReader,
																				 const std::string&							samplerCreateInfo,
																				 VkSamplerCreateInfo&						sCI);

#endif // CTS_USES_VULKANSC

template <typename InfoType, typename IteratorType>
vk::ProgramBinary* ResourceInterface::buildProgram			(const std::string&					casePath,
															 IteratorType						iter,
															 const vk::BinaryRegistryReader&	prebuiltBinRegistry,
															 vk::BinaryCollection*				progCollection)
{
	const vk::ProgramIdentifier		progId			(casePath, iter.getName());
	tcu::TestLog&					log				= m_testCtx.getLog();
	const tcu::CommandLine&			commandLine		= m_testCtx.getCommandLine();
	const tcu::ScopedLogSection		progSection		(log, iter.getName(), "Program: " + iter.getName());
	de::MovePtr<vk::ProgramBinary>	binProg;
	InfoType						buildInfo;

	try
	{
		binProg = de::MovePtr<vk::ProgramBinary>(compileProgram(progId, iter.getProgram(), &buildInfo, commandLine));
		log << buildInfo;
	}
	catch (const tcu::NotSupportedError& err)
	{
		// Try to load from cache
		log << err << tcu::TestLog::Message << "Building from source not supported, loading stored binary instead" << tcu::TestLog::EndMessage;

		binProg = de::MovePtr<vk::ProgramBinary>(prebuiltBinRegistry.loadProgram(progId));

		log << iter.getProgram();
	}
	catch (const tcu::Exception&)
	{
		// Build failed for other reason
		log << buildInfo;
		throw;
	}

	TCU_CHECK_INTERNAL(binProg);

	{
		vk::ProgramBinary* const	returnBinary = binProg.get();

		progCollection->add(progId.programName, binProg);

		return returnBinary;
	}
}

} // vk

#endif // _VKRESOURCEINTERFACE_HPP
