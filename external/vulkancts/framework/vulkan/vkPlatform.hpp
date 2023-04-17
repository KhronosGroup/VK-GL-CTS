#ifndef _VKPLATFORM_HPP
#define _VKPLATFORM_HPP
/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2015 Google Inc.
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
 * \brief Vulkan platform abstraction.
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "deUniquePtr.hpp"

#include <ostream>
#include <deSharedPtr.hpp>
#ifdef CTS_USES_VULKANSC
#include <mutex>
	#include <vector>
	#include <map>
	#include "vkResourceInterface.hpp"
#include "tcuCommandLine.hpp"
#endif // CTS_USES_VULKANSC

namespace tcu
{
class FunctionLibrary;
}

namespace vk
{

class Library
{
public:
										Library					(void) {}
	virtual								~Library				(void) {}

	virtual const PlatformInterface&	getPlatformInterface	(void) const = 0;
	virtual const tcu::FunctionLibrary&	getFunctionLibrary		(void) const = 0;
};

class PlatformDriver : public PlatformInterface
{
public:
				PlatformDriver	(const tcu::FunctionLibrary& library);
				~PlatformDriver	(void);

#include "vkConcretePlatformInterface.inl"

				virtual	GetInstanceProcAddrFunc	getGetInstanceProcAddr  () const {
					return m_vk.getInstanceProcAddr;
				}

protected:
	struct Functions
	{
#include "vkPlatformFunctionPointers.inl"
	};

	Functions	m_vk;
};

class InstanceDriver : public InstanceInterface
{
public:
						InstanceDriver		(const PlatformInterface&	platformInterface,
											 VkInstance					instance);
	virtual				~InstanceDriver		(void);

#include "vkConcreteInstanceInterface.inl"

protected:
	void				loadFunctions		(const PlatformInterface&	platformInterface,
											 VkInstance					instance);

	struct Functions
	{
#include "vkInstanceFunctionPointers.inl"
	};

	Functions	m_vk;
};

#ifdef CTS_USES_VULKANSC

class InstanceDriverSC : public InstanceDriver
{
public:
						InstanceDriverSC	(const PlatformInterface&				platformInterface,
											 VkInstance								instance,
											 const tcu::CommandLine&				cmdLine,
											 de::SharedPtr<vk::ResourceInterface>	resourceInterface);

	virtual VkResult	createDevice		(VkPhysicalDevice						physicalDevice,
											 const VkDeviceCreateInfo*				pCreateInfo,
											 const VkAllocationCallbacks*			pAllocator,
											 VkDevice*								pDevice) const;
protected:
	mutable std::mutex						functionMutex;
	bool									m_normalMode;
	de::SharedPtr<vk::ResourceInterface>	m_resourceInterface;
};

#endif // CTS_USES_VULKANSC

class DeviceDriver : public DeviceInterface
{
public:
						DeviceDriver		(const PlatformInterface&			platformInterface,
											 VkInstance							instance,
											 VkDevice							device);
	virtual				~DeviceDriver		(void);

#include "vkConcreteDeviceInterface.inl"

#ifdef CTS_USES_VULKANSC
	virtual VkResult	createShaderModule	(VkDevice							device,
											 const VkShaderModuleCreateInfo*	pCreateInfo,
											 const VkAllocationCallbacks*		pAllocator,
											 VkShaderModule*					pShaderModule) const;
#endif // CTS_USES_VULKANSC

protected:
	struct Functions
	{
#include "vkDeviceFunctionPointers.inl"
	};

	Functions	m_vk;
};

#ifdef CTS_USES_VULKANSC

#define DDSTAT_LOCK() std::lock_guard<std::mutex> statLock(m_resourceInterface->getStatMutex())
#define DDSTAT_HANDLE_CREATE(VAR_NAME,VAR_VALUE) do { m_resourceInterface->getStatCurrent().VAR_NAME += (VAR_VALUE); m_resourceInterface->getStatMax().VAR_NAME = de::max(m_resourceInterface->getStatMax().VAR_NAME, m_resourceInterface->getStatCurrent().VAR_NAME); } while(0)
#define DDSTAT_HANDLE_DESTROY_IF(VAR_VARIABLE,VAR_NAME,VAR_VALUE) if(VAR_VARIABLE.getInternal()!=DE_NULL && m_resourceInterface->isEnabledHandleDestroy()) m_resourceInterface->getStatCurrent().VAR_NAME -= (VAR_VALUE)
#define DDSTAT_HANDLE_DESTROY(VAR_NAME,VAR_VALUE) if( m_resourceInterface->isEnabledHandleDestroy() ) m_resourceInterface->getStatCurrent().VAR_NAME -= (VAR_VALUE)

class DeviceDriverSC : public DeviceDriver
{
public:
										DeviceDriverSC						(const		PlatformInterface&					platformInterface,
																			 VkInstance										instance,
																			 VkDevice										device,
																			 const tcu::CommandLine&						cmdLine,
																			 de::SharedPtr<vk::ResourceInterface>			resourceInterface,
																			 const VkPhysicalDeviceVulkanSC10Properties&	physicalDeviceVulkanSC10Properties,
																			 const VkPhysicalDeviceProperties&				physicalDeviceProperties);
	virtual								~DeviceDriverSC						(void);

#include "vkConcreteDeviceInterface.inl"

	// Functions ending with Handler() and HandlerStat() work only when we gather statistics ( in a main process ).
	// Functions ending with HandlerNorm() work in normal mode ( in subprocess, when real test is performed )
	// Method createShaderModule() works in both modes, and ResourceInterface is responsible for distinguishing modes
	void								destroyDeviceHandler					(VkDevice								device,
																				 const VkAllocationCallbacks*			pAllocator) const;
	VkResult							createDescriptorSetLayoutHandlerNorm	(VkDevice								device,
																				 const VkDescriptorSetLayoutCreateInfo*	pCreateInfo,
																				 const VkAllocationCallbacks*			pAllocator,
																				 VkDescriptorSetLayout*					pSetLayout) const;
	void								createDescriptorSetLayoutHandlerStat	(VkDevice								device,
																				 const VkDescriptorSetLayoutCreateInfo*	pCreateInfo,
																				 const VkAllocationCallbacks*			pAllocator,
																				 VkDescriptorSetLayout*					pSetLayout) const;
	void								destroyDescriptorSetLayoutHandler		(VkDevice								device,
																				 VkDescriptorSetLayout					descriptorSetLayout,
																				 const VkAllocationCallbacks*			pAllocator) const;
	void								allocateDescriptorSetsHandlerStat		(VkDevice								device,
																				 const VkDescriptorSetAllocateInfo*		pAllocateInfo,
																				 VkDescriptorSet*						pDescriptorSets) const;
	void								freeDescriptorSetsHandlerStat			(VkDevice								device,
																				 VkDescriptorPool						descriptorPool,
																				 uint32_t								descriptorSetCount,
																				 const VkDescriptorSet*					pDescriptorSets) const;
	void								resetDescriptorPoolHandlerStat			(VkDevice								device,
																				 VkDescriptorPool						descriptorPool,
																				 VkDescriptorPoolResetFlags				flags) const;
	void								createImageViewHandler					(VkDevice								device,
																				 const VkImageViewCreateInfo*			pCreateInfo,
																				 const VkAllocationCallbacks*			pAllocator,
																				 VkImageView*							pView) const;
	void								destroyImageViewHandler					(VkDevice								device,
																				 VkImageView							imageView,
																				 const VkAllocationCallbacks*			pAllocator) const;
	void								createQueryPoolHandler					(VkDevice								device,
																				 const VkQueryPoolCreateInfo*			pCreateInfo,
																				 const VkAllocationCallbacks*			pAllocator,
																				 VkQueryPool*							pQueryPool) const ;
	VkResult							createPipelineLayoutHandlerNorm			(VkDevice								device,
																				 const VkPipelineLayoutCreateInfo*		pCreateInfo,
																				 const VkAllocationCallbacks*			pAllocator,
																				 VkPipelineLayout*						pPipelineLayout) const;
	void								createPipelineLayoutHandlerStat			(VkDevice								device,
																				 const VkPipelineLayoutCreateInfo*		pCreateInfo,
																				 const VkAllocationCallbacks*			pAllocator,
																				 VkPipelineLayout*						pPipelineLayout) const;
	VkResult							createGraphicsPipelinesHandlerNorm		(VkDevice								device,
																				 VkPipelineCache						pipelineCache,
																				 deUint32								createInfoCount,
																				 const VkGraphicsPipelineCreateInfo*	pCreateInfos,
																				 const VkAllocationCallbacks*			pAllocator,
																				 VkPipeline*							pPipelines) const;
	void								createGraphicsPipelinesHandlerStat		(VkDevice								device,
																				 VkPipelineCache						pipelineCache,
																				 deUint32								createInfoCount,
																				 const VkGraphicsPipelineCreateInfo*	pCreateInfos,
																				 const VkAllocationCallbacks*			pAllocator,
																				 VkPipeline*							pPipelines) const;
	VkResult							createComputePipelinesHandlerNorm		(VkDevice								device,
																				 VkPipelineCache						pipelineCache,
																				 deUint32								createInfoCount,
																				 const VkComputePipelineCreateInfo*		pCreateInfos,
																				 const VkAllocationCallbacks*			pAllocator,
																				 VkPipeline*							pPipelines) const;
	void								createComputePipelinesHandlerStat		(VkDevice								device,
																				 VkPipelineCache						pipelineCache,
																				 deUint32								createInfoCount,
																				 const VkComputePipelineCreateInfo*		pCreateInfos,
																				 const VkAllocationCallbacks*			pAllocator,
																				 VkPipeline*							pPipelines) const;
	void								destroyPipelineHandler					(VkDevice								device,
																				 VkPipeline								pipeline,
																				 const VkAllocationCallbacks*			pAllocator) const;
	VkResult							createFramebufferHandlerNorm			(VkDevice								device,
																				 const VkFramebufferCreateInfo*			pCreateInfo,
																				 const VkAllocationCallbacks*			pAllocator,
																				 VkFramebuffer*							pFramebuffer) const;
	void								createFramebufferHandlerStat			(VkDevice								device,
																				 const VkFramebufferCreateInfo*			pCreateInfo,
																				 const VkAllocationCallbacks*			pAllocator,
																				 VkFramebuffer*							pFramebuffer) const;
	VkResult							createRenderPassHandlerNorm				(VkDevice								device,
																				 const VkRenderPassCreateInfo*			pCreateInfo,
																				 const VkAllocationCallbacks*			pAllocator,
																				 VkRenderPass*							pRenderPass) const;
	void								createRenderPassHandlerStat				(VkDevice								device,
																				 const VkRenderPassCreateInfo*			pCreateInfo,
																				 const VkAllocationCallbacks*			pAllocator,
																				 VkRenderPass*							pRenderPass) const;
	VkResult							createRenderPass2HandlerNorm			(VkDevice								device,
																				 const VkRenderPassCreateInfo2*			pCreateInfo,
																				 const VkAllocationCallbacks*			pAllocator,
																				 VkRenderPass*							pRenderPass) const;
	void								createRenderPass2HandlerStat			(VkDevice								device,
																				 const VkRenderPassCreateInfo2*			pCreateInfo,
																				 const VkAllocationCallbacks*			pAllocator,
																				 VkRenderPass*							pRenderPass) const;
	void								destroyRenderPassHandler				(VkDevice								device,
																				 VkRenderPass							renderPass,
																				 const VkAllocationCallbacks*			pAllocator) const;
	VkResult							createSamplerHandlerNorm				(VkDevice								device,
																				 const VkSamplerCreateInfo*				pCreateInfo,
																				 const VkAllocationCallbacks*			pAllocator,
																				 VkSampler*								pSampler) const;
	void								createSamplerHandlerStat				(VkDevice								device,
																				 const VkSamplerCreateInfo*				pCreateInfo,
																				 const VkAllocationCallbacks*			pAllocator,
																				 VkSampler*								pSampler) const;
	VkResult							createSamplerYcbcrConversionHandlerNorm	(VkDevice								device,
																				 const VkSamplerYcbcrConversionCreateInfo*	pCreateInfo,
																				 const VkAllocationCallbacks*			pAllocator,
																				 VkSamplerYcbcrConversion*				pYcbcrConversion) const;
	void								createSamplerYcbcrConversionHandlerStat	(VkDevice								device,
																				 const VkSamplerYcbcrConversionCreateInfo*	pCreateInfo,
																				 const VkAllocationCallbacks*			pAllocator,
																				 VkSamplerYcbcrConversion*				pYcbcrConversion) const;
	void								getDescriptorSetLayoutSupportHandler	(VkDevice								device,
																				 const VkDescriptorSetLayoutCreateInfo*	pCreateInfo,
																				 VkDescriptorSetLayoutSupport*			pSupport) const;
	virtual VkResult					createShaderModule						(VkDevice								device,
																				 const	VkShaderModuleCreateInfo*		pCreateInfo,
																				 const VkAllocationCallbacks*			pAllocator,
																				 VkShaderModule*						pShaderModule) const;

	VkResult							createCommandPoolHandlerNorm			(VkDevice								device,
																				 const VkCommandPoolCreateInfo*			pCreateInfo,
																				 const VkAllocationCallbacks*			pAllocator,
																				 VkCommandPool*							pCommandPool) const;
	VkResult							resetCommandPoolHandlerNorm				(VkDevice								device,
																				 VkCommandPool							commandPool,
																				 VkCommandPoolResetFlags				flags) const;
	void								createCommandPoolHandlerStat			(VkDevice								device,
																				 const VkCommandPoolCreateInfo*			pCreateInfo,
																				 const VkAllocationCallbacks*			pAllocator,
																				 VkCommandPool*							pCommandPool) const;
	void								resetCommandPoolHandlerStat				(VkDevice								device,
																				 VkCommandPool							commandPool,
																				 VkCommandPoolResetFlags				flags) const;
	void								allocateCommandBuffersHandler			(VkDevice								device,
																				 const VkCommandBufferAllocateInfo*		pAllocateInfo,
																				 VkCommandBuffer*						pCommandBuffers) const;
	void								freeCommandBuffersHandler				(VkDevice								device,
																				 VkCommandPool							commandPool,
																				 deUint32								commandBufferCount,
																				 const VkCommandBuffer*					pCommandBuffers) const;
	void								increaseCommandBufferSize				(VkCommandBuffer						commandBuffer,
																				 VkDeviceSize							commandSize) const;
	void								checkFramebufferSupport					(const VkFramebufferCreateInfo*			pCreateInfo) const;
	void								checkRenderPassSupport					(deUint32								attachmentCount,
																				 deUint32								subpassCount,
																				 deUint32								dependencyCount) const;
	void								checkSubpassSupport						(deUint32								inputAttachmentCount,
																				 deUint32								preserveAttachmentCount) const;


	de::SharedPtr<ResourceInterface>	gerResourceInterface					() const;
	void								reset									() const;

protected:
	mutable std::mutex															functionMutex;
	bool																		m_normalMode;

	de::SharedPtr<vk::ResourceInterface>										m_resourceInterface;

	mutable std::vector<deUint8>												m_falseMemory;
	mutable std::map<VkImageView, VkImageViewCreateInfo>						m_imageViews;
	mutable std::map<VkDescriptorSetLayout, VkDescriptorSetLayoutCreateInfo>	m_descriptorSetLayouts;
	mutable std::map<VkRenderPass, VkRenderPassCreateInfo>						m_renderPasses;
	mutable std::map<VkRenderPass, VkRenderPassCreateInfo2>						m_renderPasses2;
	mutable std::map<VkPipeline, VkGraphicsPipelineCreateInfo>					m_graphicsPipelines;
	mutable std::map<VkPipeline, VkComputePipelineCreateInfo>					m_computePipelines;
	mutable std::map<VkDescriptorSet, VkDescriptorPool>							m_descriptorSetsInPool;
	VkPhysicalDeviceVulkanSC10Properties										m_physicalDeviceVulkanSC10Properties;
	VkPhysicalDeviceProperties													m_physicalDeviceProperties;

	VkDeviceSize																m_commandDefaultSize;
	VkDeviceSize																m_commandBufferMinimumSize;
	VkDeviceSize																m_commandPoolMinimumSize;
};

class DeinitDeviceDeleter : public Deleter<DeviceDriverSC>
{
public:
										DeinitDeviceDeleter					(ResourceInterface* resourceInterface, const VkDevice& device)
		: m_resourceInterface(resourceInterface)
		, m_device(device)
	{
	}
										DeinitDeviceDeleter					(void)
		: m_resourceInterface(DE_NULL)
		, m_device(DE_NULL)
	{}

	void								operator()							(DeviceDriverSC* obj) const
	{
		if (m_resourceInterface != DE_NULL)
			m_resourceInterface->deinitDevice(m_device);
		delete obj;
	}
private:
	ResourceInterface*	m_resourceInterface;
	VkDevice			m_device;
};

#endif // CTS_USES_VULKANSC

// Single device driver pointer type which will differ in SC and non-SC mode helping clearing the code
#ifndef CTS_USES_VULKANSC
typedef de::MovePtr<DeviceDriver> DeviceDriverPtr;
#else
typedef de::MovePtr<DeviceDriverSC, vk::DeinitDeviceDeleter> DeviceDriverPtr;
#endif // CTS_USES_VULKANSC

// Defined in vkWsiPlatform.hpp
namespace wsi
{
class Display;
} // wsi

/*--------------------------------------------------------------------*//*!
 * \brief Vulkan platform interface
 *//*--------------------------------------------------------------------*/
class Platform
{
public:
	enum LibraryType
	{
		LIBRARY_TYPE_VULKAN						= 0,
		LIBRARY_TYPE_LAST
	};

							Platform			(void) {}
							~Platform			(void) {}
#ifdef DE_PLATFORM_USE_LIBRARY_TYPE
	virtual Library*		createLibrary		(LibraryType libraryType = LIBRARY_TYPE_VULKAN, const char* libraryPath = DE_NULL) const = 0;
#else
	virtual Library*		createLibrary		(const char* libraryPath = DE_NULL) const = 0;
#endif

	virtual wsi::Display*	createWsiDisplay	(wsi::Type wsiType) const;
	virtual bool			hasDisplay			(wsi::Type wsiType) const;
	virtual void			describePlatform	(std::ostream& dst) const;
};

} // vk

#endif // _VKPLATFORM_HPP
