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
#include "deSharedPtr.hpp"
#include "deDefs.hpp"
#include <map>
#ifdef CTS_USES_VULKANSC
	#include "vksClient.hpp"
	#include "tcuMaybe.hpp"
//	#include "vksStructsVKSC.hpp"
#endif // CTS_USES_VULKANSC

namespace vk
{

class ResourceInterface
{
public:
																ResourceInterface			(tcu::TestContext&						testCtx);
	virtual														~ResourceInterface			();

	virtual void												initDevice					(DeviceInterface&						deviceInterface,
																							 VkDevice								device) = 0;
	// use deinitDevice when your DeviceDriverSC is created and removed inside TestInstance
	virtual void												deinitDevice				(VkDevice								device) = 0;

	virtual void												initTestCase				(const std::string&						casePath);
	const std::string&											getCasePath					() const;

	// buildProgram
	template <typename InfoType, typename IteratorType>
	vk::ProgramBinary*											buildProgram				(const std::string&						casePath,
																							 IteratorType							iter,
																							 const vk::BinaryRegistryReader&		prebuiltBinRegistry,
																							 vk::BinaryCollection*					progCollection);

#ifdef CTS_USES_VULKANSC
	void														initApiVersion				(const deUint32							version);
	bool														isVulkanSC					(void) const;

	deUint64													incResourceCounter			();
	std::mutex&													getStatMutex				();
	VkDeviceObjectReservationCreateInfo&						getStatCurrent				();
	VkDeviceObjectReservationCreateInfo&						getStatMax					();
	const VkDeviceObjectReservationCreateInfo&					getStatMax					() const;
	void														setHandleDestroy			(bool									value);
	bool														isEnabledHandleDestroy		() const;

	virtual void												registerDeviceFeatures		(VkDevice								device,
																							 const VkDeviceCreateInfo*				pCreateInfo) const = 0;
	virtual void												unregisterDeviceFeatures	(VkDevice								device) const = 0;
	virtual VkResult											createShaderModule			(VkDevice								device,
																							 const VkShaderModuleCreateInfo*		pCreateInfo,
																							 const VkAllocationCallbacks*			pAllocator,
																							 VkShaderModule*						pShaderModule,
																							 bool									normalMode) const = 0;
	virtual VkResult											createGraphicsPipelines		(VkDevice								device,
																							 VkPipelineCache						pipelineCache,
																							 deUint32 createInfoCount,
																							 const VkGraphicsPipelineCreateInfo*	pCreateInfos,
																							 const VkAllocationCallbacks*			pAllocator,
																							 VkPipeline*							pPipelines,
																							 bool									normalMode) const = 0;
	virtual VkResult											createComputePipelines		(VkDevice								device,
																							 VkPipelineCache						pipelineCache,
																							 deUint32								createInfoCount,
																							 const VkComputePipelineCreateInfo*		pCreateInfos,
																							 const VkAllocationCallbacks*			pAllocator,
																							 VkPipeline*							pPipelines,
																							 bool									normalMode) const = 0;
	virtual void												destroyPipeline				(VkDevice								device,
																							 VkPipeline								pipeline,
																							 const VkAllocationCallbacks*			pAllocator) const = 0;
	virtual void												createRenderPass			(VkDevice								device,
																							 const VkRenderPassCreateInfo*			pCreateInfo,
																							 const VkAllocationCallbacks*			pAllocator,
																							 VkRenderPass*							pRenderPass) const = 0;
	virtual void												createRenderPass2			(VkDevice								device,
																							 const VkRenderPassCreateInfo2*			pCreateInfo,
																							 const VkAllocationCallbacks*			pAllocator,
																							 VkRenderPass*							pRenderPass) const = 0;
	virtual void												createPipelineLayout		(VkDevice								device,
																							 const VkPipelineLayoutCreateInfo*		pCreateInfo,
																							 const VkAllocationCallbacks*			pAllocator,
																							 VkPipelineLayout*						pPipelineLayout) const = 0;
	virtual void												createDescriptorSetLayout	(VkDevice								device,
																							 const VkDescriptorSetLayoutCreateInfo*	pCreateInfo,
																							 const VkAllocationCallbacks*			pAllocator,
																							 VkDescriptorSetLayout*					pSetLayout) const = 0;
	virtual void												createSampler				(VkDevice								device,
																							 const VkSamplerCreateInfo*				pCreateInfo,
																							 const VkAllocationCallbacks*			pAllocator,
																							 VkSampler*								pSampler) const = 0;
	virtual void												createSamplerYcbcrConversion(VkDevice								device,
																							 const VkSamplerYcbcrConversionCreateInfo*	pCreateInfo,
																							 const VkAllocationCallbacks*			pAllocator,
																							 VkSamplerYcbcrConversion*				pYcbcrConversion) const = 0;
	virtual void												createCommandPool			(VkDevice								device,
																							 const VkCommandPoolCreateInfo*			pCreateInfo,
																							 const VkAllocationCallbacks*			pAllocator,
																							 VkCommandPool*							pCommandPool) const = 0;
	virtual void												allocateCommandBuffers		(VkDevice								device,
																							 const VkCommandBufferAllocateInfo*		pAllocateInfo,
																							 VkCommandBuffer*						pCommandBuffers) const = 0;
	virtual void												increaseCommandBufferSize	(VkCommandBuffer						commandBuffer,
																							 VkDeviceSize							commandSize) const = 0;
	virtual void												resetCommandPool			(VkDevice								device,
																							 VkCommandPool							commandPool,
																							 VkCommandPoolResetFlags				flags) const = 0;

	void														removeRedundantObjects		();
	void														finalizeCommandBuffers		();
	std::vector<deUint8>										exportData					() const;
	void														importData					(std::vector<deUint8>&					importText) const;
	virtual void												importPipelineCacheData		(const PlatformInterface&				vkp,
																							 VkInstance								instance,
																							 const InstanceInterface&				vki,
																							 VkPhysicalDevice						physicalDevice,
																							 deUint32								queueIndex) = 0;
	void														registerObjectHash			(deUint64								handle,
																							 std::size_t							hashValue) const;
	const std::map<deUint64, std::size_t>&						getObjectHashes				() const;

	void														preparePipelinePoolSizes	();
	std::vector<VkPipelinePoolSize>								getPipelinePoolSizes		() const;
	void														fillPoolEntrySize			(vk::VkPipelineOfflineCreateInfo&		pipelineIdentifier) const;
	vksc_server::VulkanCommandMemoryConsumption					getNextCommandPoolSize		();
	std::size_t													getCacheDataSize			() const;
	const deUint8*												getCacheData				() const;
	VkPipelineCache												getPipelineCache			(VkDevice								device) const;
	virtual void												resetObjects				() = 0;
	virtual void												resetPipelineCaches			() = 0;
#endif // CTS_USES_VULKANSC

protected:
	virtual vk::ProgramBinary*									compileProgram				(const vk::ProgramIdentifier&			progId,
																							 const vk::GlslSource&					source,
																							 glu::ShaderProgramInfo*				buildInfo,
																							 const tcu::CommandLine&				commandLine) = 0;
	virtual vk::ProgramBinary*									compileProgram				(const vk::ProgramIdentifier&			progId,
																							 const vk::HlslSource&					source,
																							 glu::ShaderProgramInfo*				buildInfo,
																							 const tcu::CommandLine&				commandLine) = 0;
	virtual vk::ProgramBinary*									compileProgram				(const vk::ProgramIdentifier&			progId,
																							 const vk::SpirVAsmSource&				source,
																							 vk::SpirVProgramInfo*					buildInfo,
																							 const tcu::CommandLine&				commandLine) = 0;

	tcu::TestContext&											m_testCtx;
	std::string													m_currentTestPath;

#ifdef CTS_USES_VULKANSC
	mutable vksc_server::VulkanPipelineCacheInput					m_pipelineInput;
	mutable std::map<deUint64, std::size_t>							m_objectHashes;
	mutable std::vector<vksc_server::VulkanCommandMemoryConsumption>
																	m_commandPoolMemoryConsumption;
	mutable deUint32												m_commandPoolIndex;
	mutable std::map<VkCommandBuffer, vksc_server::VulkanCommandMemoryConsumption>
																	m_commandBufferMemoryConsumption;
	mutable std::map<VkDevice, std::string>							m_deviceFeatures;
	mutable std::map<VkDevice, std::vector<std::string>>			m_deviceExtensions;

	std::map<VkDevice,de::SharedPtr<Move<VkPipelineCache>>>			m_pipelineCache;

	mutable std::mutex												m_mutex;
	mutable deUint64												m_resourceCounter;
	mutable VkDeviceObjectReservationCreateInfo						m_statCurrent;
	mutable VkDeviceObjectReservationCreateInfo						m_statMax;

	std::vector<deUint8>											m_cacheData;
	mutable std::map<VkPipeline, VkPipelineOfflineCreateInfo>		m_pipelineIdentifiers;
	mutable std::vector<vksc_server::VulkanPipelineSize>			m_pipelineSizes;
	std::vector<VkPipelinePoolSize>									m_pipelinePoolSizes;
	tcu::Maybe<deUint32>											m_version;
	tcu::Maybe<bool>												m_vulkanSC;
	bool															m_enabledHandleDestroy;
#endif // CTS_USES_VULKANSC
};

typedef VKAPI_ATTR VkResult	(VKAPI_CALL* CreateSamplerYcbcrConversionFunc)		(VkDevice device, const VkSamplerYcbcrConversionCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSamplerYcbcrConversion* pYcbcrConversion);
typedef VKAPI_ATTR void		(VKAPI_CALL* DestroySamplerYcbcrConversionFunc)		(VkDevice device, VkSamplerYcbcrConversion ycbcrConversion, const VkAllocationCallbacks* pAllocator);
typedef VKAPI_ATTR VkResult	(VKAPI_CALL* CreateSamplerFunc)						(VkDevice device, const VkSamplerCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSampler* pSampler);
typedef VKAPI_ATTR void		(VKAPI_CALL* DestroySamplerFunc)					(VkDevice device, VkSampler sampler, const VkAllocationCallbacks* pAllocator);
typedef VKAPI_ATTR VkResult	(VKAPI_CALL* CreateShaderModuleFunc)				(VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkShaderModule* pShaderModule);
typedef VKAPI_ATTR void		(VKAPI_CALL* DestroyShaderModuleFunc)				(VkDevice device, VkShaderModule shaderModule, const VkAllocationCallbacks* pAllocator);
typedef VKAPI_ATTR VkResult	(VKAPI_CALL* CreateRenderPassFunc)					(VkDevice device, const VkRenderPassCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass);
typedef VKAPI_ATTR VkResult	(VKAPI_CALL* CreateRenderPass2Func)					(VkDevice device, const VkRenderPassCreateInfo2* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass);
typedef VKAPI_ATTR void		(VKAPI_CALL* DestroyRenderPassFunc)					(VkDevice device, VkRenderPass renderPass, const VkAllocationCallbacks* pAllocator);
typedef VKAPI_ATTR VkResult	(VKAPI_CALL* CreateDescriptorSetLayoutFunc)			(VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorSetLayout* pSetLayout);
typedef VKAPI_ATTR void		(VKAPI_CALL* DestroyDescriptorSetLayoutFunc)		(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const VkAllocationCallbacks* pAllocator);
typedef VKAPI_ATTR VkResult	(VKAPI_CALL* CreatePipelineLayoutFunc)				(VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineLayout* pPipelineLayout);
typedef VKAPI_ATTR void		(VKAPI_CALL* DestroyPipelineLayoutFunc)				(VkDevice device, VkPipelineLayout pipelineLayout, const VkAllocationCallbacks* pAllocator);
typedef VKAPI_ATTR VkResult	(VKAPI_CALL* CreateGraphicsPipelinesFunc)			(VkDevice device, VkPipelineCache pipelineCache, deUint32 createInfoCount, const VkGraphicsPipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines);
typedef VKAPI_ATTR VkResult	(VKAPI_CALL* CreateComputePipelinesFunc)			(VkDevice device, VkPipelineCache pipelineCache, deUint32 createInfoCount, const VkComputePipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines);
typedef VKAPI_ATTR void		(VKAPI_CALL* DestroyPipelineFunc)					(VkDevice device, VkPipeline pipeline, const VkAllocationCallbacks* pAllocator);
typedef VKAPI_ATTR VkResult	(VKAPI_CALL* CreatePipelineCacheFunc)				(VkDevice device, const VkPipelineCacheCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineCache* pPipelineCache);
typedef VKAPI_ATTR void		(VKAPI_CALL* DestroyPipelineCacheFunc)				(VkDevice device, VkPipelineCache pipelineCache, const VkAllocationCallbacks* pAllocator);
typedef VKAPI_ATTR VkResult	(VKAPI_CALL* GetPipelineCacheDataFunc)				(VkDevice device, VkPipelineCache pipelineCache, deUintptr* pDataSize, void* pData);
#ifdef CTS_USES_VULKANSC
typedef VKAPI_ATTR void		(VKAPI_CALL* GetCommandPoolMemoryConsumptionFunc)	(VkDevice device, VkCommandPool commandPool, VkCommandBuffer commandBuffer, VkCommandPoolMemoryConsumption* pConsumption);
#endif // CTS_USES_VULKANSC

class ResourceInterfaceStandard : public ResourceInterface
{
public:
								ResourceInterfaceStandard	(tcu::TestContext&						testCtx);

	void						initDevice					(DeviceInterface&						deviceInterface,
															 VkDevice								device) override;
	void						deinitDevice				(VkDevice								device) override;

#ifdef CTS_USES_VULKANSC
	void						registerDeviceFeatures		(VkDevice								device,
															const VkDeviceCreateInfo*				pCreateInfo) const override;
	void						unregisterDeviceFeatures	(VkDevice								device) const override;
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
	void						destroyPipeline				(VkDevice								device,
															 VkPipeline								pipeline,
															 const VkAllocationCallbacks*			pAllocator) const override;
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
	void						createSamplerYcbcrConversion(VkDevice								device,
															 const VkSamplerYcbcrConversionCreateInfo*	pCreateInfo,
															 const VkAllocationCallbacks*			pAllocator,
															 VkSamplerYcbcrConversion*				pYcbcrConversion) const override;
	void						createCommandPool			(VkDevice								device,
															 const VkCommandPoolCreateInfo*			pCreateInfo,
															 const VkAllocationCallbacks*			pAllocator,
															 VkCommandPool*							pCommandPool) const override;
	void						allocateCommandBuffers		(VkDevice								device,
															 const VkCommandBufferAllocateInfo*		pAllocateInfo,
															 VkCommandBuffer*						pCommandBuffers) const override;
	void						increaseCommandBufferSize	(VkCommandBuffer						commandBuffer,
															 VkDeviceSize							commandSize) const override;
	void						resetCommandPool			(VkDevice								device,
															 VkCommandPool							commandPool,
															 VkCommandPoolResetFlags				flags) const override;
	void						importPipelineCacheData		(const PlatformInterface&				vkp,
															 VkInstance								instance,
															 const InstanceInterface&				vki,
															 VkPhysicalDevice						physicalDevice,
															 deUint32								queueIndex) override;
	void						resetObjects				() override;
	void						resetPipelineCaches			() override;
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

	std::map<VkDevice,CreateShaderModuleFunc>				m_createShaderModuleFunc;
	std::map<VkDevice,CreateGraphicsPipelinesFunc>			m_createGraphicsPipelinesFunc;
	std::map<VkDevice,CreateComputePipelinesFunc>			m_createComputePipelinesFunc;
};

#ifdef CTS_USES_VULKANSC

class ResourceInterfaceVKSC : public ResourceInterfaceStandard
{
public:
								ResourceInterfaceVKSC		(tcu::TestContext&						testCtx);

	VkResult					createShaderModule			(VkDevice								device,
															 const VkShaderModuleCreateInfo*		pCreateInfo,
															 const VkAllocationCallbacks*			pAllocator,
															 VkShaderModule*						pShaderModule,
															 bool									normalMode) const override;

	void						importPipelineCacheData		(const PlatformInterface&				vkp,
															 VkInstance								instance,
															 const InstanceInterface&				vki,
															 VkPhysicalDevice						physicalDevice,
															 deUint32								queueIndex) override;

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

private:
	vksc_server::Server*		getServer					();
	bool						noServer					() const;

	std::string								m_address;
	std::shared_ptr<vksc_server::Server>	m_server;
};

class MultithreadedDestroyGuard
{
public:
	MultithreadedDestroyGuard	(de::SharedPtr<vk::ResourceInterface> resourceInterface);
	~MultithreadedDestroyGuard	();
private:
	de::SharedPtr<vk::ResourceInterface> m_resourceInterface;
};

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
