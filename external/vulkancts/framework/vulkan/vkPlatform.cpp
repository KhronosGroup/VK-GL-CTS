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

#include "vkPlatform.hpp"
#include "tcuFunctionLibrary.hpp"
#ifdef CTS_USES_VULKANSC
#include "vkSafetyCriticalUtil.hpp"
#endif // CTS_USES_VULKANSC

namespace vk
{

PlatformDriver::PlatformDriver (const tcu::FunctionLibrary& library)
{
	m_vk.getInstanceProcAddr	= (GetInstanceProcAddrFunc)library.getFunction("vkGetInstanceProcAddr");

#define GET_PROC_ADDR(NAME) m_vk.getInstanceProcAddr(DE_NULL, NAME)
#include "vkInitPlatformFunctionPointers.inl"
#undef GET_PROC_ADDR
}

PlatformDriver::~PlatformDriver (void)
{
}

InstanceDriver::InstanceDriver (const PlatformInterface&		platformInterface,
								VkInstance						instance)
{
	loadFunctions(platformInterface, instance);
}

void InstanceDriver::loadFunctions (const PlatformInterface&	platformInterface,
									VkInstance					instance)
{
#define GET_PROC_ADDR(NAME) platformInterface.getInstanceProcAddr(instance, NAME)
#include "vkInitInstanceFunctionPointers.inl"
#undef GET_PROC_ADDR
}

InstanceDriver::~InstanceDriver (void)
{
}

DeviceDriver::DeviceDriver (const PlatformInterface&	platformInterface,
							VkInstance					instance,
							VkDevice					device)
{
	m_vk.getDeviceProcAddr = (GetDeviceProcAddrFunc)platformInterface.getInstanceProcAddr(instance, "vkGetDeviceProcAddr");

#define GET_PROC_ADDR(NAME) m_vk.getDeviceProcAddr(device, NAME)
#include "vkInitDeviceFunctionPointers.inl"
#undef GET_PROC_ADDR
}

DeviceDriver::~DeviceDriver (void)
{
}

#ifdef CTS_USES_VULKANSC
VkResult DeviceDriver::createShaderModule (VkDevice							device,
										   const VkShaderModuleCreateInfo*	pCreateInfo,
										   const VkAllocationCallbacks*		pAllocator,
										   VkShaderModule*					pShaderModule) const
{
	// this should not be called - Vulkan SC implementation uses DeviceDriverSC instead
	DE_UNREF(device);
	DE_UNREF(pCreateInfo);
	DE_UNREF(pAllocator);
	DE_UNREF(pShaderModule);
	TCU_THROW(InternalError, "Wrong DeviceDriver called in VulkanSC");
}
#endif

#ifdef CTS_USES_VULKANSC

DeviceDriverSC::DeviceDriverSC (const PlatformInterface&				platformInterface,
								VkInstance								instance,
								VkDevice								device,
								const tcu::CommandLine&					cmdLine,
								de::SharedPtr<vk::ResourceInterface>	resourceInterface)
	: DeviceDriver(platformInterface, instance, device)
	, m_normalMode(cmdLine.isSubProcess())
	, m_resourceInterface(resourceInterface)
	, m_resourceCounter(0U)
	, m_statCurrent(resetDeviceObjectReservationCreateInfo())
	, m_statMax(resetDeviceObjectReservationCreateInfo())
{
	if(!cmdLine.isSubProcess())
		m_falseMemory.resize(64u * 1024u * 1024u, 0u);
	m_resourceInterface->initDevice(*this, device);
}

DeviceDriverSC::~DeviceDriverSC(void)
{
	m_resourceInterface->deinitDevice();
}

void DeviceDriverSC::createDescriptorSetLayoutHandler (VkDevice									device,
													   const VkDescriptorSetLayoutCreateInfo*	pCreateInfo,
													   const VkAllocationCallbacks*				pAllocator,
													   VkDescriptorSetLayout*					pSetLayout) const
{
	DE_UNREF(device);
	DE_UNREF(pAllocator);

	DDSTAT_HANDLE_CREATE(descriptorSetLayoutRequestCount,1);
	m_statMax.descriptorSetLayoutBindingRequestCount = de::max(m_statMax.descriptorSetLayoutBindingRequestCount, pCreateInfo->bindingCount + 1);
	*pSetLayout = VkDescriptorSetLayout(++m_resourceCounter);
	m_resourceInterface->createDescriptorSetLayout(device, pCreateInfo, pAllocator, pSetLayout);

}

void DeviceDriverSC::createImageViewHandler (VkDevice						device,
											 const VkImageViewCreateInfo*	pCreateInfo,
											 const VkAllocationCallbacks*	pAllocator,
											 VkImageView*					pView) const
{
	DE_UNREF(device);
	DE_UNREF(pAllocator);

	DDSTAT_HANDLE_CREATE(imageViewRequestCount,1);
	if (pCreateInfo->subresourceRange.layerCount > 1)
		DDSTAT_HANDLE_CREATE(layeredImageViewRequestCount,1);
	m_statMax.maxImageViewMipLevels		= de::max(m_statMax.maxImageViewMipLevels, pCreateInfo->subresourceRange.levelCount);
	m_statMax.maxImageViewArrayLayers	= de::max(m_statMax.maxImageViewArrayLayers, pCreateInfo->subresourceRange.layerCount);
	if(pCreateInfo->subresourceRange.layerCount > 1)
		m_statMax.maxLayeredImageViewMipLevels = de::max(m_statMax.maxLayeredImageViewMipLevels, pCreateInfo->subresourceRange.levelCount);

	*pView = VkImageView(++m_resourceCounter);
	m_imageViews.insert({ *pView, *pCreateInfo });
}

void DeviceDriverSC::destroyImageViewHandler (VkDevice						device,
											  VkImageView					imageView,
											  const VkAllocationCallbacks*	pAllocator) const
{
	DE_UNREF(device);
	DE_UNREF(pAllocator);

	auto it = m_imageViews.find(imageView);
	if (it != end(m_imageViews))
	{
		DDSTAT_HANDLE_DESTROY(imageViewRequestCount, 1);
		if(it->second.subresourceRange.layerCount > 1)
			DDSTAT_HANDLE_DESTROY(layeredImageViewRequestCount,1);
		m_imageViews.erase(it);
	}
}

void DeviceDriverSC::createQueryPoolHandler (VkDevice						device,
											 const VkQueryPoolCreateInfo*	pCreateInfo,
											 const VkAllocationCallbacks*	pAllocator,
											 VkQueryPool*					pQueryPool) const
{
	DE_UNREF(device);
	DE_UNREF(pAllocator);
	DDSTAT_HANDLE_CREATE(queryPoolRequestCount, 1);
	switch (pCreateInfo->queryType)
	{
		case VK_QUERY_TYPE_OCCLUSION:
			m_statMax.maxOcclusionQueriesPerPool = de::max(m_statMax.maxOcclusionQueriesPerPool, pCreateInfo->queryCount);
			break;
		case VK_QUERY_TYPE_PIPELINE_STATISTICS:
			m_statMax.maxPipelineStatisticsQueriesPerPool = de::max(m_statMax.maxPipelineStatisticsQueriesPerPool, pCreateInfo->queryCount);
			break;
		case VK_QUERY_TYPE_TIMESTAMP:
			m_statMax.maxTimestampQueriesPerPool = de::max(m_statMax.maxTimestampQueriesPerPool, pCreateInfo->queryCount);
			break;
		default:
			break;
	}
	// We don't have to track queryPool resources as we do with image views because they're not removed from memory in Vulkan SC.
	*pQueryPool = VkQueryPool(++m_resourceCounter);
}

void DeviceDriverSC::createPipelineLayoutHandler (VkDevice								device,
												  const VkPipelineLayoutCreateInfo*		pCreateInfo,
												  const VkAllocationCallbacks*			pAllocator,
												  VkPipelineLayout*						pPipelineLayout) const
{
	DDSTAT_HANDLE_CREATE(pipelineLayoutRequestCount, 1);
	*pPipelineLayout = VkPipelineLayout(++m_resourceCounter);
	m_resourceInterface->createPipelineLayout(device, pCreateInfo, pAllocator, pPipelineLayout);
}

VkResult DeviceDriverSC::createGraphicsPipelinesHandlerNorm (VkDevice								device,
															 VkPipelineCache						pipelineCache,
															 deUint32								createInfoCount,
															 const VkGraphicsPipelineCreateInfo*	pCreateInfos,
															 const VkAllocationCallbacks*			pAllocator,
															 VkPipeline*							pPipelines) const
{
	return m_resourceInterface->createGraphicsPipelines(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines, m_normalMode);
}

void DeviceDriverSC::createGraphicsPipelinesHandlerStat (VkDevice									device,
														 VkPipelineCache							pipelineCache,
														 deUint32									createInfoCount,
														 const VkGraphicsPipelineCreateInfo*		pCreateInfos,
														 const VkAllocationCallbacks*				pAllocator,
														 VkPipeline*								pPipelines) const
{
	DE_UNREF(device);
	DE_UNREF(pipelineCache);
	DE_UNREF(pAllocator);

	DDSTAT_HANDLE_CREATE(graphicsPipelineRequestCount, createInfoCount);
	for (deUint32 i = 0; i < createInfoCount; ++i)
	{
		pPipelines[i] = VkPipeline(++m_resourceCounter);
		m_graphicsPipelines.insert({ pPipelines[i], pCreateInfos[i] });
	}

	m_resourceInterface->createGraphicsPipelines(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines, m_normalMode);
}

VkResult DeviceDriverSC::createComputePipelinesHandlerNorm (VkDevice								device,
															VkPipelineCache							pipelineCache,
															deUint32								createInfoCount,
															const VkComputePipelineCreateInfo*		pCreateInfos,
															const VkAllocationCallbacks*			pAllocator,
															VkPipeline*								pPipelines) const
{
	return m_resourceInterface->createComputePipelines(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines, m_normalMode);
}

void DeviceDriverSC::createComputePipelinesHandlerStat (VkDevice									device,
														VkPipelineCache								pipelineCache,
														deUint32									createInfoCount,
														const VkComputePipelineCreateInfo*			pCreateInfos,
														const VkAllocationCallbacks*				pAllocator,
														VkPipeline*									pPipelines) const
{
	DE_UNREF(device);
	DE_UNREF(pipelineCache);
	DE_UNREF(pAllocator);

	DDSTAT_HANDLE_CREATE(computePipelineRequestCount, createInfoCount);
	for (deUint32 i = 0; i < createInfoCount; ++i)
	{
		pPipelines[i] = VkPipeline(++m_resourceCounter);
		m_computePipelines.insert({ pPipelines[i], pCreateInfos[i] });
	}

	m_resourceInterface->createComputePipelines(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines, m_normalMode);
}

void DeviceDriverSC::destroyPipelineHandler (VkDevice						device,
											 VkPipeline						pipeline,
											 const VkAllocationCallbacks*	pAllocator) const
{
	DE_UNREF(device);
	DE_UNREF(pAllocator);

	auto it = m_graphicsPipelines.find(pipeline);
	if (it != end(m_graphicsPipelines))
	{
		DDSTAT_HANDLE_DESTROY(graphicsPipelineRequestCount, 1);
		m_graphicsPipelines.erase(it);
		return;
	}

	auto it2 = m_computePipelines.find(pipeline);
	if (it2 != end(m_computePipelines))
	{
		DDSTAT_HANDLE_DESTROY(computePipelineRequestCount, 1);
		m_computePipelines.erase(it2);
	}
}

void DeviceDriverSC::createRenderPassHandler (VkDevice						device,
											  const VkRenderPassCreateInfo*	pCreateInfo,
											  const VkAllocationCallbacks*	pAllocator,
											  VkRenderPass*					pRenderPass) const
{
	DE_UNREF(device);
	DE_UNREF(pAllocator);

	DDSTAT_HANDLE_CREATE(renderPassRequestCount, 1);
	DDSTAT_HANDLE_CREATE(subpassDescriptionRequestCount,	pCreateInfo->subpassCount);
	DDSTAT_HANDLE_CREATE(attachmentDescriptionRequestCount,	pCreateInfo->attachmentCount);

	*pRenderPass = VkRenderPass(++m_resourceCounter);
	m_renderPasses.insert({ *pRenderPass, *pCreateInfo });
	m_resourceInterface->createRenderPass(device, pCreateInfo, pAllocator, pRenderPass);
}

void DeviceDriverSC::createRenderPass2Handler (VkDevice							device,
											   const VkRenderPassCreateInfo2*	pCreateInfo,
											   const VkAllocationCallbacks*		pAllocator,
											   VkRenderPass*					pRenderPass) const
{
	DE_UNREF(device);
	DE_UNREF(pAllocator);

	DDSTAT_HANDLE_CREATE(renderPassRequestCount, 1);
	DDSTAT_HANDLE_CREATE(subpassDescriptionRequestCount,	pCreateInfo->subpassCount);
	DDSTAT_HANDLE_CREATE(attachmentDescriptionRequestCount,	pCreateInfo->attachmentCount);

	*pRenderPass = VkRenderPass(++m_resourceCounter);
	m_renderPasses2.insert({ *pRenderPass, *pCreateInfo });
	m_resourceInterface->createRenderPass2(device, pCreateInfo, pAllocator, pRenderPass);
}

void DeviceDriverSC::destroyRenderPassHandler (VkDevice						device,
											   VkRenderPass					renderPass,
											   const VkAllocationCallbacks*	pAllocator) const
{
	DE_UNREF(device);
	DE_UNREF(pAllocator);

	auto it = m_renderPasses.find(renderPass);
	if (it != end(m_renderPasses))
	{
		DDSTAT_HANDLE_DESTROY(renderPassRequestCount, 1);
		DDSTAT_HANDLE_DESTROY(subpassDescriptionRequestCount, it->second.subpassCount);
		DDSTAT_HANDLE_DESTROY(attachmentDescriptionRequestCount, it->second.attachmentCount);
		m_renderPasses.erase(it);
		return;
	}

	auto it2 = m_renderPasses2.find(renderPass);
	if (it2 != end(m_renderPasses2))
	{
		DDSTAT_HANDLE_DESTROY(renderPassRequestCount, 1);
		DDSTAT_HANDLE_DESTROY(subpassDescriptionRequestCount, it2->second.subpassCount);
		DDSTAT_HANDLE_DESTROY(attachmentDescriptionRequestCount, it2->second.attachmentCount);
		m_renderPasses2.erase(it2);
	}
}

void DeviceDriverSC::createSamplerHandler (VkDevice							device,
										   const VkSamplerCreateInfo*		pCreateInfo,
										   const VkAllocationCallbacks*		pAllocator,
										   VkSampler*						pSampler) const
{
	DDSTAT_HANDLE_CREATE(samplerRequestCount, 1);
	*pSampler = VkSampler(++m_resourceCounter);
	m_resourceInterface->createSampler(device, pCreateInfo, pAllocator, pSampler);
}

VkResult DeviceDriverSC::createShaderModule (VkDevice							device,
											 const VkShaderModuleCreateInfo*	pCreateInfo,
											 const VkAllocationCallbacks*		pAllocator,
											 VkShaderModule*					pShaderModule) const
{
	return m_resourceInterface->createShaderModule(device, pCreateInfo, pAllocator, pShaderModule, m_normalMode);
}

de::SharedPtr<ResourceInterface> DeviceDriverSC::gerResourceInterface() const
{
	return m_resourceInterface;
}

VkDeviceObjectReservationCreateInfo DeviceDriverSC::getStatistics () const
{
	return m_statMax;
}

void DeviceDriverSC::resetStatistics() const
{
	m_resourceCounter	= 0u;
	m_statCurrent		= resetDeviceObjectReservationCreateInfo();
	m_statMax			= resetDeviceObjectReservationCreateInfo();

	// these objects should be empty when function is invoked, but we will clear it anyway
	m_imageViews.clear();
	m_renderPasses.clear();
	m_renderPasses2.clear();
	m_graphicsPipelines.clear();
	m_computePipelines.clear();
}

#endif // CTS_USES_VULKANSC

#include "vkPlatformDriverImpl.inl"
#include "vkInstanceDriverImpl.inl"
#include "vkDeviceDriverImpl.inl"
#ifdef CTS_USES_VULKANSC
#include "vkDeviceDriverSCImpl.inl"
#endif // CTS_USES_VULKANSC

wsi::Display* Platform::createWsiDisplay (wsi::Type) const
{
	TCU_THROW(NotSupportedError, "WSI not supported");
}

bool Platform::hasDisplay (wsi::Type) const
{
	return false;
}

void Platform::describePlatform (std::ostream& dst) const
{
	dst << "vk::Platform::describePlatform() not implemented";
}

} // vk
