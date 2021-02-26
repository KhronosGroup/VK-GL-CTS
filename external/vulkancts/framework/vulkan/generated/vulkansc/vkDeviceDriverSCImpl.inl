/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 */

PFN_vkVoidFunction DeviceDriverSC::getDeviceProcAddr (VkDevice device, const char* pName) const
{
		return m_vk.getDeviceProcAddr(device, pName);
}

void DeviceDriverSC::destroyDevice (VkDevice device, const VkAllocationCallbacks* pAllocator) const
{
	if (m_normalMode)
		m_vk.destroyDevice(device, pAllocator);
}

void DeviceDriverSC::getDeviceQueue (VkDevice device, deUint32 queueFamilyIndex, deUint32 queueIndex, VkQueue* pQueue) const
{
	if (m_normalMode)
		m_vk.getDeviceQueue(device, queueFamilyIndex, queueIndex, pQueue);
}

VkResult DeviceDriverSC::queueSubmit (VkQueue queue, deUint32 submitCount, const VkSubmitInfo* pSubmits, VkFence fence) const
{
	if (m_normalMode)
		return m_vk.queueSubmit(queue, submitCount, pSubmits, fence);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::queueWaitIdle (VkQueue queue) const
{
	if (m_normalMode)
		return m_vk.queueWaitIdle(queue);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::deviceWaitIdle (VkDevice device) const
{
	if (m_normalMode)
		return m_vk.deviceWaitIdle(device);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::allocateMemory (VkDevice device, const VkMemoryAllocateInfo* pAllocateInfo, const VkAllocationCallbacks* pAllocator, VkDeviceMemory* pMemory) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		return m_vk.allocateMemory(device, pAllocateInfo, pAllocator, pMemory);
	else
	{
		DDSTAT_HANDLE_CREATE(deviceMemoryRequestCount,1);
		*pMemory = Handle<HANDLE_TYPE_DEVICE_MEMORY>(++m_resourceCounter);
	}
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::mapMemory (VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** ppData) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		return m_vk.mapMemory(device, memory, offset, size, flags, ppData);
	else
	{
		if(m_falseMemory.size() < (static_cast<std::size_t>(offset+size)))
			m_falseMemory.resize(static_cast<std::size_t>(offset+size));
		*ppData = (void*)m_falseMemory.data();
	}
	return VK_SUCCESS;
}

void DeviceDriverSC::unmapMemory (VkDevice device, VkDeviceMemory memory) const
{
	if (m_normalMode)
		m_vk.unmapMemory(device, memory);
}

VkResult DeviceDriverSC::flushMappedMemoryRanges (VkDevice device, deUint32 memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges) const
{
	if (m_normalMode)
		return m_vk.flushMappedMemoryRanges(device, memoryRangeCount, pMemoryRanges);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::invalidateMappedMemoryRanges (VkDevice device, deUint32 memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges) const
{
	if (m_normalMode)
		return m_vk.invalidateMappedMemoryRanges(device, memoryRangeCount, pMemoryRanges);
	return VK_SUCCESS;
}

void DeviceDriverSC::getDeviceMemoryCommitment (VkDevice device, VkDeviceMemory memory, VkDeviceSize* pCommittedMemoryInBytes) const
{
	if (m_normalMode)
		m_vk.getDeviceMemoryCommitment(device, memory, pCommittedMemoryInBytes);
}

VkResult DeviceDriverSC::bindBufferMemory (VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset) const
{
	if (m_normalMode)
		return m_vk.bindBufferMemory(device, buffer, memory, memoryOffset);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::bindImageMemory (VkDevice device, VkImage image, VkDeviceMemory memory, VkDeviceSize memoryOffset) const
{
	if (m_normalMode)
		return m_vk.bindImageMemory(device, image, memory, memoryOffset);
	return VK_SUCCESS;
}

void DeviceDriverSC::getBufferMemoryRequirements (VkDevice device, VkBuffer buffer, VkMemoryRequirements* pMemoryRequirements) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		m_vk.getBufferMemoryRequirements(device, buffer, pMemoryRequirements);
	else
	{
		pMemoryRequirements->size = 1048576;
		pMemoryRequirements->alignment = 1;
		pMemoryRequirements->memoryTypeBits = ~0U;
	}
}

void DeviceDriverSC::getImageMemoryRequirements (VkDevice device, VkImage image, VkMemoryRequirements* pMemoryRequirements) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		m_vk.getImageMemoryRequirements(device, image, pMemoryRequirements);
	else
	{
		pMemoryRequirements->size = 1048576;
		pMemoryRequirements->alignment = 1;
		pMemoryRequirements->memoryTypeBits = ~0U;
	}
}

VkResult DeviceDriverSC::createFence (VkDevice device, const VkFenceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFence* pFence) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		return m_vk.createFence(device, pCreateInfo, pAllocator, pFence);
	else
	{
		DDSTAT_HANDLE_CREATE(fenceRequestCount,1);
		*pFence = Handle<HANDLE_TYPE_FENCE>(++m_resourceCounter);
	}
	return VK_SUCCESS;
}

void DeviceDriverSC::destroyFence (VkDevice device, VkFence fence, const VkAllocationCallbacks* pAllocator) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		m_vk.destroyFence(device, fence, pAllocator);
	else
		DDSTAT_HANDLE_DESTROY(fenceRequestCount,1);
}

VkResult DeviceDriverSC::resetFences (VkDevice device, deUint32 fenceCount, const VkFence* pFences) const
{
	if (m_normalMode)
		return m_vk.resetFences(device, fenceCount, pFences);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::getFenceStatus (VkDevice device, VkFence fence) const
{
	if (m_normalMode)
		return m_vk.getFenceStatus(device, fence);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::waitForFences (VkDevice device, deUint32 fenceCount, const VkFence* pFences, VkBool32 waitAll, deUint64 timeout) const
{
	if (m_normalMode)
		return m_vk.waitForFences(device, fenceCount, pFences, waitAll, timeout);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::createSemaphore (VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSemaphore* pSemaphore) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		return m_vk.createSemaphore(device, pCreateInfo, pAllocator, pSemaphore);
	else
	{
		DDSTAT_HANDLE_CREATE(semaphoreRequestCount,1);
		*pSemaphore = Handle<HANDLE_TYPE_SEMAPHORE>(++m_resourceCounter);
	}
	return VK_SUCCESS;
}

void DeviceDriverSC::destroySemaphore (VkDevice device, VkSemaphore semaphore, const VkAllocationCallbacks* pAllocator) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		m_vk.destroySemaphore(device, semaphore, pAllocator);
	else
		DDSTAT_HANDLE_DESTROY(semaphoreRequestCount,1);
}

VkResult DeviceDriverSC::createEvent (VkDevice device, const VkEventCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkEvent* pEvent) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		return m_vk.createEvent(device, pCreateInfo, pAllocator, pEvent);
	else
	{
		DDSTAT_HANDLE_CREATE(eventRequestCount,1);
		*pEvent = Handle<HANDLE_TYPE_EVENT>(++m_resourceCounter);
	}
	return VK_SUCCESS;
}

void DeviceDriverSC::destroyEvent (VkDevice device, VkEvent event, const VkAllocationCallbacks* pAllocator) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		m_vk.destroyEvent(device, event, pAllocator);
	else
		DDSTAT_HANDLE_DESTROY(eventRequestCount,1);
}

VkResult DeviceDriverSC::getEventStatus (VkDevice device, VkEvent event) const
{
	if (m_normalMode)
		return m_vk.getEventStatus(device, event);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::setEvent (VkDevice device, VkEvent event) const
{
	if (m_normalMode)
		return m_vk.setEvent(device, event);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::resetEvent (VkDevice device, VkEvent event) const
{
	if (m_normalMode)
		return m_vk.resetEvent(device, event);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::createQueryPool (VkDevice device, const VkQueryPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkQueryPool* pQueryPool) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		return m_vk.createQueryPool(device, pCreateInfo, pAllocator, pQueryPool);
	else
		createQueryPoolHandler(device, pCreateInfo, pAllocator, pQueryPool);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::getQueryPoolResults (VkDevice device, VkQueryPool queryPool, deUint32 firstQuery, deUint32 queryCount, deUintptr dataSize, void* pData, VkDeviceSize stride, VkQueryResultFlags flags) const
{
	if (m_normalMode)
		return m_vk.getQueryPoolResults(device, queryPool, firstQuery, queryCount, dataSize, pData, stride, flags);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::createBuffer (VkDevice device, const VkBufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBuffer* pBuffer) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		return m_vk.createBuffer(device, pCreateInfo, pAllocator, pBuffer);
	else
	{
		DDSTAT_HANDLE_CREATE(bufferRequestCount,1);
		*pBuffer = Handle<HANDLE_TYPE_BUFFER>(++m_resourceCounter);
	}
	return VK_SUCCESS;
}

void DeviceDriverSC::destroyBuffer (VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* pAllocator) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		m_vk.destroyBuffer(device, buffer, pAllocator);
	else
		DDSTAT_HANDLE_DESTROY(bufferRequestCount,1);
}

VkResult DeviceDriverSC::createBufferView (VkDevice device, const VkBufferViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBufferView* pView) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		return m_vk.createBufferView(device, pCreateInfo, pAllocator, pView);
	else
	{
		DDSTAT_HANDLE_CREATE(bufferViewRequestCount,1);
		*pView = Handle<HANDLE_TYPE_BUFFER_VIEW>(++m_resourceCounter);
	}
	return VK_SUCCESS;
}

void DeviceDriverSC::destroyBufferView (VkDevice device, VkBufferView bufferView, const VkAllocationCallbacks* pAllocator) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		m_vk.destroyBufferView(device, bufferView, pAllocator);
	else
		DDSTAT_HANDLE_DESTROY(bufferViewRequestCount,1);
}

VkResult DeviceDriverSC::createImage (VkDevice device, const VkImageCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImage* pImage) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		return m_vk.createImage(device, pCreateInfo, pAllocator, pImage);
	else
	{
		DDSTAT_HANDLE_CREATE(imageRequestCount,1);
		*pImage = Handle<HANDLE_TYPE_IMAGE>(++m_resourceCounter);
	}
	return VK_SUCCESS;
}

void DeviceDriverSC::destroyImage (VkDevice device, VkImage image, const VkAllocationCallbacks* pAllocator) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		m_vk.destroyImage(device, image, pAllocator);
	else
		DDSTAT_HANDLE_DESTROY(imageRequestCount,1);
}

void DeviceDriverSC::getImageSubresourceLayout (VkDevice device, VkImage image, const VkImageSubresource* pSubresource, VkSubresourceLayout* pLayout) const
{
	if (m_normalMode)
		m_vk.getImageSubresourceLayout(device, image, pSubresource, pLayout);
}

VkResult DeviceDriverSC::createImageView (VkDevice device, const VkImageViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImageView* pView) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		return m_vk.createImageView(device, pCreateInfo, pAllocator, pView);
	else
		createImageViewHandler(device, pCreateInfo, pAllocator, pView);
	return VK_SUCCESS;
}

void DeviceDriverSC::destroyImageView (VkDevice device, VkImageView imageView, const VkAllocationCallbacks* pAllocator) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		m_vk.destroyImageView(device, imageView, pAllocator);
	else
		destroyImageViewHandler(device, imageView, pAllocator);
}

VkResult DeviceDriverSC::createPipelineCache (VkDevice device, const VkPipelineCacheCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineCache* pPipelineCache) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		return m_vk.createPipelineCache(device, pCreateInfo, pAllocator, pPipelineCache);
	else
		*pPipelineCache = Handle<HANDLE_TYPE_PIPELINE_CACHE>(++m_resourceCounter);
	return VK_SUCCESS;
}

void DeviceDriverSC::destroyPipelineCache (VkDevice device, VkPipelineCache pipelineCache, const VkAllocationCallbacks* pAllocator) const
{
	if (m_normalMode)
		m_vk.destroyPipelineCache(device, pipelineCache, pAllocator);
}

VkResult DeviceDriverSC::createGraphicsPipelines (VkDevice device, VkPipelineCache pipelineCache, deUint32 createInfoCount, const VkGraphicsPipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		return createGraphicsPipelinesHandlerNorm(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
	else
		createGraphicsPipelinesHandlerStat(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::createComputePipelines (VkDevice device, VkPipelineCache pipelineCache, deUint32 createInfoCount, const VkComputePipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		return createComputePipelinesHandlerNorm(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
	else
		createComputePipelinesHandlerStat(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
	return VK_SUCCESS;
}

void DeviceDriverSC::destroyPipeline (VkDevice device, VkPipeline pipeline, const VkAllocationCallbacks* pAllocator) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		m_vk.destroyPipeline(device, pipeline, pAllocator);
	else
		destroyPipelineHandler(device, pipeline, pAllocator);
}

VkResult DeviceDriverSC::createPipelineLayout (VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineLayout* pPipelineLayout) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		return m_vk.createPipelineLayout(device, pCreateInfo, pAllocator, pPipelineLayout);
	else
		createPipelineLayoutHandler(device, pCreateInfo, pAllocator, pPipelineLayout);
	return VK_SUCCESS;
}

void DeviceDriverSC::destroyPipelineLayout (VkDevice device, VkPipelineLayout pipelineLayout, const VkAllocationCallbacks* pAllocator) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		m_vk.destroyPipelineLayout(device, pipelineLayout, pAllocator);
	else
		DDSTAT_HANDLE_DESTROY(pipelineLayoutRequestCount,1);
}

VkResult DeviceDriverSC::createSampler (VkDevice device, const VkSamplerCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSampler* pSampler) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		return m_vk.createSampler(device, pCreateInfo, pAllocator, pSampler);
	else
		createSamplerHandler(device, pCreateInfo, pAllocator, pSampler);
	return VK_SUCCESS;
}

void DeviceDriverSC::destroySampler (VkDevice device, VkSampler sampler, const VkAllocationCallbacks* pAllocator) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		m_vk.destroySampler(device, sampler, pAllocator);
	else
		DDSTAT_HANDLE_DESTROY(samplerRequestCount,1);
}

VkResult DeviceDriverSC::createDescriptorSetLayout (VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorSetLayout* pSetLayout) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		return m_vk.createDescriptorSetLayout(device, pCreateInfo, pAllocator, pSetLayout);
	else
		createDescriptorSetLayoutHandler(device, pCreateInfo, pAllocator, pSetLayout);
	return VK_SUCCESS;
}

void DeviceDriverSC::destroyDescriptorSetLayout (VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const VkAllocationCallbacks* pAllocator) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		m_vk.destroyDescriptorSetLayout(device, descriptorSetLayout, pAllocator);
	else
		DDSTAT_HANDLE_DESTROY(descriptorSetLayoutRequestCount,1);
}

VkResult DeviceDriverSC::createDescriptorPool (VkDevice device, const VkDescriptorPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorPool* pDescriptorPool) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		return m_vk.createDescriptorPool(device, pCreateInfo, pAllocator, pDescriptorPool);
	else
	{
		DDSTAT_HANDLE_CREATE(descriptorPoolRequestCount,1);
		*pDescriptorPool = Handle<HANDLE_TYPE_DESCRIPTOR_POOL>(++m_resourceCounter);
	}
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::resetDescriptorPool (VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorPoolResetFlags flags) const
{
	if (m_normalMode)
		return m_vk.resetDescriptorPool(device, descriptorPool, flags);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::allocateDescriptorSets (VkDevice device, const VkDescriptorSetAllocateInfo* pAllocateInfo, VkDescriptorSet* pDescriptorSets) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		return m_vk.allocateDescriptorSets(device, pAllocateInfo, pDescriptorSets);
	else
	{
		DDSTAT_HANDLE_CREATE(descriptorSetRequestCount,pAllocateInfo->descriptorSetCount);
		*pDescriptorSets = Handle<HANDLE_TYPE_DESCRIPTOR_SET>(++m_resourceCounter);
	}
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::freeDescriptorSets (VkDevice device, VkDescriptorPool descriptorPool, deUint32 descriptorSetCount, const VkDescriptorSet* pDescriptorSets) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		return m_vk.freeDescriptorSets(device, descriptorPool, descriptorSetCount, pDescriptorSets);
	else
		DDSTAT_HANDLE_DESTROY(descriptorSetRequestCount,descriptorSetCount);
	return VK_SUCCESS;
}

void DeviceDriverSC::updateDescriptorSets (VkDevice device, deUint32 descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites, deUint32 descriptorCopyCount, const VkCopyDescriptorSet* pDescriptorCopies) const
{
	if (m_normalMode)
		m_vk.updateDescriptorSets(device, descriptorWriteCount, pDescriptorWrites, descriptorCopyCount, pDescriptorCopies);
}

VkResult DeviceDriverSC::createFramebuffer (VkDevice device, const VkFramebufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFramebuffer* pFramebuffer) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		return m_vk.createFramebuffer(device, pCreateInfo, pAllocator, pFramebuffer);
	else
	{
		DDSTAT_HANDLE_CREATE(framebufferRequestCount,1);
		*pFramebuffer = Handle<HANDLE_TYPE_FRAMEBUFFER>(++m_resourceCounter);
	}
	return VK_SUCCESS;
}

void DeviceDriverSC::destroyFramebuffer (VkDevice device, VkFramebuffer framebuffer, const VkAllocationCallbacks* pAllocator) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		m_vk.destroyFramebuffer(device, framebuffer, pAllocator);
	else
		DDSTAT_HANDLE_DESTROY(framebufferRequestCount,1);
}

VkResult DeviceDriverSC::createRenderPass (VkDevice device, const VkRenderPassCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		return m_vk.createRenderPass(device, pCreateInfo, pAllocator, pRenderPass);
	else
		createRenderPassHandler(device, pCreateInfo, pAllocator, pRenderPass);
	return VK_SUCCESS;
}

void DeviceDriverSC::destroyRenderPass (VkDevice device, VkRenderPass renderPass, const VkAllocationCallbacks* pAllocator) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		m_vk.destroyRenderPass(device, renderPass, pAllocator);
	else
		destroyRenderPassHandler(device, renderPass, pAllocator);
}

void DeviceDriverSC::getRenderAreaGranularity (VkDevice device, VkRenderPass renderPass, VkExtent2D* pGranularity) const
{
	if (m_normalMode)
		m_vk.getRenderAreaGranularity(device, renderPass, pGranularity);
}

VkResult DeviceDriverSC::createCommandPool (VkDevice device, const VkCommandPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkCommandPool* pCommandPool) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		return m_vk.createCommandPool(device, pCreateInfo, pAllocator, pCommandPool);
	else
	{
		DDSTAT_HANDLE_CREATE(commandPoolRequestCount,1);
		*pCommandPool = Handle<HANDLE_TYPE_COMMAND_POOL>(++m_resourceCounter);
	}
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::resetCommandPool (VkDevice device, VkCommandPool commandPool, VkCommandPoolResetFlags flags) const
{
	if (m_normalMode)
		return m_vk.resetCommandPool(device, commandPool, flags);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::allocateCommandBuffers (VkDevice device, const VkCommandBufferAllocateInfo* pAllocateInfo, VkCommandBuffer* pCommandBuffers) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		return m_vk.allocateCommandBuffers(device, pAllocateInfo, pCommandBuffers);
	else
	{
		DDSTAT_HANDLE_CREATE(commandBufferRequestCount,pAllocateInfo->commandBufferCount);
		*pCommandBuffers = (VkCommandBuffer)++m_resourceCounter;
	}
	return VK_SUCCESS;
}

void DeviceDriverSC::freeCommandBuffers (VkDevice device, VkCommandPool commandPool, deUint32 commandBufferCount, const VkCommandBuffer* pCommandBuffers) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		m_vk.freeCommandBuffers(device, commandPool, commandBufferCount, pCommandBuffers);
	else
	DDSTAT_HANDLE_DESTROY(commandBufferRequestCount,commandBufferCount);
}

VkResult DeviceDriverSC::beginCommandBuffer (VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo* pBeginInfo) const
{
	if (m_normalMode)
		return m_vk.beginCommandBuffer(commandBuffer, pBeginInfo);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::endCommandBuffer (VkCommandBuffer commandBuffer) const
{
	if (m_normalMode)
		return m_vk.endCommandBuffer(commandBuffer);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::resetCommandBuffer (VkCommandBuffer commandBuffer, VkCommandBufferResetFlags flags) const
{
	if (m_normalMode)
		return m_vk.resetCommandBuffer(commandBuffer, flags);
	return VK_SUCCESS;
}

void DeviceDriverSC::cmdBindPipeline (VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline) const
{
	if (m_normalMode)
		m_vk.cmdBindPipeline(commandBuffer, pipelineBindPoint, pipeline);
}

void DeviceDriverSC::cmdSetViewport (VkCommandBuffer commandBuffer, deUint32 firstViewport, deUint32 viewportCount, const VkViewport* pViewports) const
{
	if (m_normalMode)
		m_vk.cmdSetViewport(commandBuffer, firstViewport, viewportCount, pViewports);
}

void DeviceDriverSC::cmdSetScissor (VkCommandBuffer commandBuffer, deUint32 firstScissor, deUint32 scissorCount, const VkRect2D* pScissors) const
{
	if (m_normalMode)
		m_vk.cmdSetScissor(commandBuffer, firstScissor, scissorCount, pScissors);
}

void DeviceDriverSC::cmdSetLineWidth (VkCommandBuffer commandBuffer, float lineWidth) const
{
	if (m_normalMode)
		m_vk.cmdSetLineWidth(commandBuffer, lineWidth);
}

void DeviceDriverSC::cmdSetDepthBias (VkCommandBuffer commandBuffer, float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor) const
{
	if (m_normalMode)
		m_vk.cmdSetDepthBias(commandBuffer, depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
}

void DeviceDriverSC::cmdSetBlendConstants (VkCommandBuffer commandBuffer, const float blendConstants[4]) const
{
	if (m_normalMode)
		m_vk.cmdSetBlendConstants(commandBuffer, blendConstants);
}

void DeviceDriverSC::cmdSetDepthBounds (VkCommandBuffer commandBuffer, float minDepthBounds, float maxDepthBounds) const
{
	if (m_normalMode)
		m_vk.cmdSetDepthBounds(commandBuffer, minDepthBounds, maxDepthBounds);
}

void DeviceDriverSC::cmdSetStencilCompareMask (VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, deUint32 compareMask) const
{
	if (m_normalMode)
		m_vk.cmdSetStencilCompareMask(commandBuffer, faceMask, compareMask);
}

void DeviceDriverSC::cmdSetStencilWriteMask (VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, deUint32 writeMask) const
{
	if (m_normalMode)
		m_vk.cmdSetStencilWriteMask(commandBuffer, faceMask, writeMask);
}

void DeviceDriverSC::cmdSetStencilReference (VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, deUint32 reference) const
{
	if (m_normalMode)
		m_vk.cmdSetStencilReference(commandBuffer, faceMask, reference);
}

void DeviceDriverSC::cmdBindDescriptorSets (VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, deUint32 firstSet, deUint32 descriptorSetCount, const VkDescriptorSet* pDescriptorSets, deUint32 dynamicOffsetCount, const deUint32* pDynamicOffsets) const
{
	if (m_normalMode)
		m_vk.cmdBindDescriptorSets(commandBuffer, pipelineBindPoint, layout, firstSet, descriptorSetCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
}

void DeviceDriverSC::cmdBindIndexBuffer (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType) const
{
	if (m_normalMode)
		m_vk.cmdBindIndexBuffer(commandBuffer, buffer, offset, indexType);
}

void DeviceDriverSC::cmdBindVertexBuffers (VkCommandBuffer commandBuffer, deUint32 firstBinding, deUint32 bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets) const
{
	if (m_normalMode)
		m_vk.cmdBindVertexBuffers(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets);
}

void DeviceDriverSC::cmdDraw (VkCommandBuffer commandBuffer, deUint32 vertexCount, deUint32 instanceCount, deUint32 firstVertex, deUint32 firstInstance) const
{
	if (m_normalMode)
		m_vk.cmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}

void DeviceDriverSC::cmdDrawIndexed (VkCommandBuffer commandBuffer, deUint32 indexCount, deUint32 instanceCount, deUint32 firstIndex, deInt32 vertexOffset, deUint32 firstInstance) const
{
	if (m_normalMode)
		m_vk.cmdDrawIndexed(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void DeviceDriverSC::cmdDrawIndirect (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, deUint32 drawCount, deUint32 stride) const
{
	if (m_normalMode)
		m_vk.cmdDrawIndirect(commandBuffer, buffer, offset, drawCount, stride);
}

void DeviceDriverSC::cmdDrawIndexedIndirect (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, deUint32 drawCount, deUint32 stride) const
{
	if (m_normalMode)
		m_vk.cmdDrawIndexedIndirect(commandBuffer, buffer, offset, drawCount, stride);
}

void DeviceDriverSC::cmdDispatch (VkCommandBuffer commandBuffer, deUint32 groupCountX, deUint32 groupCountY, deUint32 groupCountZ) const
{
	if (m_normalMode)
		m_vk.cmdDispatch(commandBuffer, groupCountX, groupCountY, groupCountZ);
}

void DeviceDriverSC::cmdDispatchIndirect (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset) const
{
	if (m_normalMode)
		m_vk.cmdDispatchIndirect(commandBuffer, buffer, offset);
}

void DeviceDriverSC::cmdCopyBuffer (VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, deUint32 regionCount, const VkBufferCopy* pRegions) const
{
	if (m_normalMode)
		m_vk.cmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, regionCount, pRegions);
}

void DeviceDriverSC::cmdCopyImage (VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, deUint32 regionCount, const VkImageCopy* pRegions) const
{
	if (m_normalMode)
		m_vk.cmdCopyImage(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions);
}

void DeviceDriverSC::cmdBlitImage (VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, deUint32 regionCount, const VkImageBlit* pRegions, VkFilter filter) const
{
	if (m_normalMode)
		m_vk.cmdBlitImage(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions, filter);
}

void DeviceDriverSC::cmdCopyBufferToImage (VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkImage dstImage, VkImageLayout dstImageLayout, deUint32 regionCount, const VkBufferImageCopy* pRegions) const
{
	if (m_normalMode)
		m_vk.cmdCopyBufferToImage(commandBuffer, srcBuffer, dstImage, dstImageLayout, regionCount, pRegions);
}

void DeviceDriverSC::cmdCopyImageToBuffer (VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkBuffer dstBuffer, deUint32 regionCount, const VkBufferImageCopy* pRegions) const
{
	if (m_normalMode)
		m_vk.cmdCopyImageToBuffer(commandBuffer, srcImage, srcImageLayout, dstBuffer, regionCount, pRegions);
}

void DeviceDriverSC::cmdUpdateBuffer (VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize dataSize, const void* pData) const
{
	if (m_normalMode)
		m_vk.cmdUpdateBuffer(commandBuffer, dstBuffer, dstOffset, dataSize, pData);
}

void DeviceDriverSC::cmdFillBuffer (VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize size, deUint32 data) const
{
	if (m_normalMode)
		m_vk.cmdFillBuffer(commandBuffer, dstBuffer, dstOffset, size, data);
}

void DeviceDriverSC::cmdClearColorImage (VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearColorValue* pColor, deUint32 rangeCount, const VkImageSubresourceRange* pRanges) const
{
	if (m_normalMode)
		m_vk.cmdClearColorImage(commandBuffer, image, imageLayout, pColor, rangeCount, pRanges);
}

void DeviceDriverSC::cmdClearDepthStencilImage (VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue* pDepthStencil, deUint32 rangeCount, const VkImageSubresourceRange* pRanges) const
{
	if (m_normalMode)
		m_vk.cmdClearDepthStencilImage(commandBuffer, image, imageLayout, pDepthStencil, rangeCount, pRanges);
}

void DeviceDriverSC::cmdClearAttachments (VkCommandBuffer commandBuffer, deUint32 attachmentCount, const VkClearAttachment* pAttachments, deUint32 rectCount, const VkClearRect* pRects) const
{
	if (m_normalMode)
		m_vk.cmdClearAttachments(commandBuffer, attachmentCount, pAttachments, rectCount, pRects);
}

void DeviceDriverSC::cmdResolveImage (VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, deUint32 regionCount, const VkImageResolve* pRegions) const
{
	if (m_normalMode)
		m_vk.cmdResolveImage(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions);
}

void DeviceDriverSC::cmdSetEvent (VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask) const
{
	if (m_normalMode)
		m_vk.cmdSetEvent(commandBuffer, event, stageMask);
}

void DeviceDriverSC::cmdResetEvent (VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask) const
{
	if (m_normalMode)
		m_vk.cmdResetEvent(commandBuffer, event, stageMask);
}

void DeviceDriverSC::cmdWaitEvents (VkCommandBuffer commandBuffer, deUint32 eventCount, const VkEvent* pEvents, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, deUint32 memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, deUint32 bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, deUint32 imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers) const
{
	if (m_normalMode)
		m_vk.cmdWaitEvents(commandBuffer, eventCount, pEvents, srcStageMask, dstStageMask, memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
}

void DeviceDriverSC::cmdPipelineBarrier (VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags, deUint32 memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, deUint32 bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, deUint32 imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers) const
{
	if (m_normalMode)
		m_vk.cmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, dependencyFlags, memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
}

void DeviceDriverSC::cmdBeginQuery (VkCommandBuffer commandBuffer, VkQueryPool queryPool, deUint32 query, VkQueryControlFlags flags) const
{
	if (m_normalMode)
		m_vk.cmdBeginQuery(commandBuffer, queryPool, query, flags);
}

void DeviceDriverSC::cmdEndQuery (VkCommandBuffer commandBuffer, VkQueryPool queryPool, deUint32 query) const
{
	if (m_normalMode)
		m_vk.cmdEndQuery(commandBuffer, queryPool, query);
}

void DeviceDriverSC::cmdResetQueryPool (VkCommandBuffer commandBuffer, VkQueryPool queryPool, deUint32 firstQuery, deUint32 queryCount) const
{
	if (m_normalMode)
		m_vk.cmdResetQueryPool(commandBuffer, queryPool, firstQuery, queryCount);
}

void DeviceDriverSC::cmdWriteTimestamp (VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage, VkQueryPool queryPool, deUint32 query) const
{
	if (m_normalMode)
		m_vk.cmdWriteTimestamp(commandBuffer, pipelineStage, queryPool, query);
}

void DeviceDriverSC::cmdCopyQueryPoolResults (VkCommandBuffer commandBuffer, VkQueryPool queryPool, deUint32 firstQuery, deUint32 queryCount, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize stride, VkQueryResultFlags flags) const
{
	if (m_normalMode)
		m_vk.cmdCopyQueryPoolResults(commandBuffer, queryPool, firstQuery, queryCount, dstBuffer, dstOffset, stride, flags);
}

void DeviceDriverSC::cmdPushConstants (VkCommandBuffer commandBuffer, VkPipelineLayout layout, VkShaderStageFlags stageFlags, deUint32 offset, deUint32 size, const void* pValues) const
{
	if (m_normalMode)
		m_vk.cmdPushConstants(commandBuffer, layout, stageFlags, offset, size, pValues);
}

void DeviceDriverSC::cmdBeginRenderPass (VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, VkSubpassContents contents) const
{
	if (m_normalMode)
		m_vk.cmdBeginRenderPass(commandBuffer, pRenderPassBegin, contents);
}

void DeviceDriverSC::cmdNextSubpass (VkCommandBuffer commandBuffer, VkSubpassContents contents) const
{
	if (m_normalMode)
		m_vk.cmdNextSubpass(commandBuffer, contents);
}

void DeviceDriverSC::cmdEndRenderPass (VkCommandBuffer commandBuffer) const
{
	if (m_normalMode)
		m_vk.cmdEndRenderPass(commandBuffer);
}

void DeviceDriverSC::cmdExecuteCommands (VkCommandBuffer commandBuffer, deUint32 commandBufferCount, const VkCommandBuffer* pCommandBuffers) const
{
	if (m_normalMode)
		m_vk.cmdExecuteCommands(commandBuffer, commandBufferCount, pCommandBuffers);
}

VkResult DeviceDriverSC::bindBufferMemory2 (VkDevice device, deUint32 bindInfoCount, const VkBindBufferMemoryInfo* pBindInfos) const
{
	if (m_normalMode)
		return m_vk.bindBufferMemory2(device, bindInfoCount, pBindInfos);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::bindImageMemory2 (VkDevice device, deUint32 bindInfoCount, const VkBindImageMemoryInfo* pBindInfos) const
{
	if (m_normalMode)
		return m_vk.bindImageMemory2(device, bindInfoCount, pBindInfos);
	return VK_SUCCESS;
}

void DeviceDriverSC::getDeviceGroupPeerMemoryFeatures (VkDevice device, deUint32 heapIndex, deUint32 localDeviceIndex, deUint32 remoteDeviceIndex, VkPeerMemoryFeatureFlags* pPeerMemoryFeatures) const
{
	if (m_normalMode)
		m_vk.getDeviceGroupPeerMemoryFeatures(device, heapIndex, localDeviceIndex, remoteDeviceIndex, pPeerMemoryFeatures);
}

void DeviceDriverSC::cmdSetDeviceMask (VkCommandBuffer commandBuffer, deUint32 deviceMask) const
{
	if (m_normalMode)
		m_vk.cmdSetDeviceMask(commandBuffer, deviceMask);
}

void DeviceDriverSC::cmdDispatchBase (VkCommandBuffer commandBuffer, deUint32 baseGroupX, deUint32 baseGroupY, deUint32 baseGroupZ, deUint32 groupCountX, deUint32 groupCountY, deUint32 groupCountZ) const
{
	if (m_normalMode)
		m_vk.cmdDispatchBase(commandBuffer, baseGroupX, baseGroupY, baseGroupZ, groupCountX, groupCountY, groupCountZ);
}

void DeviceDriverSC::getImageMemoryRequirements2 (VkDevice device, const VkImageMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		m_vk.getImageMemoryRequirements2(device, pInfo, pMemoryRequirements);
	else
	{
		pMemoryRequirements->memoryRequirements.size = 1048576;
		pMemoryRequirements->memoryRequirements.alignment = 1;
		pMemoryRequirements->memoryRequirements.memoryTypeBits = ~0U;
	}
}

void DeviceDriverSC::getBufferMemoryRequirements2 (VkDevice device, const VkBufferMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		m_vk.getBufferMemoryRequirements2(device, pInfo, pMemoryRequirements);
	else
	{
		pMemoryRequirements->memoryRequirements.size = 1048576;
		pMemoryRequirements->memoryRequirements.alignment = 1;
		pMemoryRequirements->memoryRequirements.memoryTypeBits = ~0U;
	}
}

void DeviceDriverSC::getDeviceQueue2 (VkDevice device, const VkDeviceQueueInfo2* pQueueInfo, VkQueue* pQueue) const
{
	if (m_normalMode)
		m_vk.getDeviceQueue2(device, pQueueInfo, pQueue);
}

VkResult DeviceDriverSC::createSamplerYcbcrConversion (VkDevice device, const VkSamplerYcbcrConversionCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSamplerYcbcrConversion* pYcbcrConversion) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		return m_vk.createSamplerYcbcrConversion(device, pCreateInfo, pAllocator, pYcbcrConversion);
	else
	{
		DDSTAT_HANDLE_CREATE(samplerYcbcrConversionRequestCount,1);
		*pYcbcrConversion = Handle<HANDLE_TYPE_SAMPLER_YCBCR_CONVERSION>(++m_resourceCounter);
	}
	return VK_SUCCESS;
}

void DeviceDriverSC::destroySamplerYcbcrConversion (VkDevice device, VkSamplerYcbcrConversion ycbcrConversion, const VkAllocationCallbacks* pAllocator) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		m_vk.destroySamplerYcbcrConversion(device, ycbcrConversion, pAllocator);
	else
		DDSTAT_HANDLE_DESTROY(samplerYcbcrConversionRequestCount,1);
}

void DeviceDriverSC::getDescriptorSetLayoutSupport (VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, VkDescriptorSetLayoutSupport* pSupport) const
{
	if (m_normalMode)
		m_vk.getDescriptorSetLayoutSupport(device, pCreateInfo, pSupport);
}

void DeviceDriverSC::cmdDrawIndirectCount (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, deUint32 maxDrawCount, deUint32 stride) const
{
	if (m_normalMode)
		m_vk.cmdDrawIndirectCount(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
}

void DeviceDriverSC::cmdDrawIndexedIndirectCount (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, deUint32 maxDrawCount, deUint32 stride) const
{
	if (m_normalMode)
		m_vk.cmdDrawIndexedIndirectCount(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
}

VkResult DeviceDriverSC::createRenderPass2 (VkDevice device, const VkRenderPassCreateInfo2* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass) const
{
	std::lock_guard<std::mutex> lock(functionMutex);
	if (m_normalMode)
		return m_vk.createRenderPass2(device, pCreateInfo, pAllocator, pRenderPass);
	else
		createRenderPass2Handler(device, pCreateInfo, pAllocator, pRenderPass);
	return VK_SUCCESS;
}

void DeviceDriverSC::cmdBeginRenderPass2 (VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, const VkSubpassBeginInfo* pSubpassBeginInfo) const
{
	if (m_normalMode)
		m_vk.cmdBeginRenderPass2(commandBuffer, pRenderPassBegin, pSubpassBeginInfo);
}

void DeviceDriverSC::cmdNextSubpass2 (VkCommandBuffer commandBuffer, const VkSubpassBeginInfo* pSubpassBeginInfo, const VkSubpassEndInfo* pSubpassEndInfo) const
{
	if (m_normalMode)
		m_vk.cmdNextSubpass2(commandBuffer, pSubpassBeginInfo, pSubpassEndInfo);
}

void DeviceDriverSC::cmdEndRenderPass2 (VkCommandBuffer commandBuffer, const VkSubpassEndInfo* pSubpassEndInfo) const
{
	if (m_normalMode)
		m_vk.cmdEndRenderPass2(commandBuffer, pSubpassEndInfo);
}

void DeviceDriverSC::resetQueryPool (VkDevice device, VkQueryPool queryPool, deUint32 firstQuery, deUint32 queryCount) const
{
	if (m_normalMode)
		m_vk.resetQueryPool(device, queryPool, firstQuery, queryCount);
}

VkResult DeviceDriverSC::getSemaphoreCounterValue (VkDevice device, VkSemaphore semaphore, deUint64* pValue) const
{
	if (m_normalMode)
		return m_vk.getSemaphoreCounterValue(device, semaphore, pValue);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::waitSemaphores (VkDevice device, const VkSemaphoreWaitInfo* pWaitInfo, deUint64 timeout) const
{
	if (m_normalMode)
		return m_vk.waitSemaphores(device, pWaitInfo, timeout);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::signalSemaphore (VkDevice device, const VkSemaphoreSignalInfo* pSignalInfo) const
{
	if (m_normalMode)
		return m_vk.signalSemaphore(device, pSignalInfo);
	return VK_SUCCESS;
}

VkDeviceAddress DeviceDriverSC::getBufferDeviceAddress (VkDevice device, const VkBufferDeviceAddressInfo* pInfo) const
{
	if (m_normalMode)
		return m_vk.getBufferDeviceAddress(device, pInfo);
	return 0u;
}

uint64_t DeviceDriverSC::getBufferOpaqueCaptureAddress (VkDevice device, const VkBufferDeviceAddressInfo* pInfo) const
{
	if (m_normalMode)
		return m_vk.getBufferOpaqueCaptureAddress(device, pInfo);
	return 0u;
}

uint64_t DeviceDriverSC::getDeviceMemoryOpaqueCaptureAddress (VkDevice device, const VkDeviceMemoryOpaqueCaptureAddressInfo* pInfo) const
{
	if (m_normalMode)
		return m_vk.getDeviceMemoryOpaqueCaptureAddress(device, pInfo);
	return 0u;
}

void DeviceDriverSC::getCommandPoolMemoryConsumption (VkDevice device, VkCommandPool commandPool, VkCommandBuffer commandBuffer, VkCommandPoolMemoryConsumption* pConsumption) const
{
	if (m_normalMode)
		m_vk.getCommandPoolMemoryConsumption(device, commandPool, commandBuffer, pConsumption);
}

VkResult DeviceDriverSC::getFaultData (VkDevice device, VkFaultQueryBehavior faultQueryBehavior, VkBool32* pUnrecordedFaults, deUint32* pFaultCount, VkFaultData* pFaults) const
{
	if (m_normalMode)
		return m_vk.getFaultData(device, faultQueryBehavior, pUnrecordedFaults, pFaultCount, pFaults);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::createSwapchainKHR (VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain) const
{
	if (m_normalMode)
		return m_vk.createSwapchainKHR(device, pCreateInfo, pAllocator, pSwapchain);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::getSwapchainImagesKHR (VkDevice device, VkSwapchainKHR swapchain, deUint32* pSwapchainImageCount, VkImage* pSwapchainImages) const
{
	if (m_normalMode)
		return m_vk.getSwapchainImagesKHR(device, swapchain, pSwapchainImageCount, pSwapchainImages);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::acquireNextImageKHR (VkDevice device, VkSwapchainKHR swapchain, deUint64 timeout, VkSemaphore semaphore, VkFence fence, deUint32* pImageIndex) const
{
	if (m_normalMode)
		return m_vk.acquireNextImageKHR(device, swapchain, timeout, semaphore, fence, pImageIndex);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::queuePresentKHR (VkQueue queue, const VkPresentInfoKHR* pPresentInfo) const
{
	if (m_normalMode)
		return m_vk.queuePresentKHR(queue, pPresentInfo);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::getDeviceGroupPresentCapabilitiesKHR (VkDevice device, VkDeviceGroupPresentCapabilitiesKHR* pDeviceGroupPresentCapabilities) const
{
	if (m_normalMode)
		return m_vk.getDeviceGroupPresentCapabilitiesKHR(device, pDeviceGroupPresentCapabilities);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::getDeviceGroupSurfacePresentModesKHR (VkDevice device, VkSurfaceKHR surface, VkDeviceGroupPresentModeFlagsKHR* pModes) const
{
	if (m_normalMode)
		return m_vk.getDeviceGroupSurfacePresentModesKHR(device, surface, pModes);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::acquireNextImage2KHR (VkDevice device, const VkAcquireNextImageInfoKHR* pAcquireInfo, deUint32* pImageIndex) const
{
	if (m_normalMode)
		return m_vk.acquireNextImage2KHR(device, pAcquireInfo, pImageIndex);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::createSharedSwapchainsKHR (VkDevice device, deUint32 swapchainCount, const VkSwapchainCreateInfoKHR* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchains) const
{
	if (m_normalMode)
		return m_vk.createSharedSwapchainsKHR(device, swapchainCount, pCreateInfos, pAllocator, pSwapchains);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::getMemoryFdKHR (VkDevice device, const VkMemoryGetFdInfoKHR* pGetFdInfo, int* pFd) const
{
	if (m_normalMode)
		return m_vk.getMemoryFdKHR(device, pGetFdInfo, pFd);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::getMemoryFdPropertiesKHR (VkDevice device, VkExternalMemoryHandleTypeFlagBits handleType, int fd, VkMemoryFdPropertiesKHR* pMemoryFdProperties) const
{
	if (m_normalMode)
		return m_vk.getMemoryFdPropertiesKHR(device, handleType, fd, pMemoryFdProperties);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::importSemaphoreFdKHR (VkDevice device, const VkImportSemaphoreFdInfoKHR* pImportSemaphoreFdInfo) const
{
	if (m_normalMode)
		return m_vk.importSemaphoreFdKHR(device, pImportSemaphoreFdInfo);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::getSemaphoreFdKHR (VkDevice device, const VkSemaphoreGetFdInfoKHR* pGetFdInfo, int* pFd) const
{
	if (m_normalMode)
		return m_vk.getSemaphoreFdKHR(device, pGetFdInfo, pFd);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::getSwapchainStatusKHR (VkDevice device, VkSwapchainKHR swapchain) const
{
	if (m_normalMode)
		return m_vk.getSwapchainStatusKHR(device, swapchain);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::importFenceFdKHR (VkDevice device, const VkImportFenceFdInfoKHR* pImportFenceFdInfo) const
{
	if (m_normalMode)
		return m_vk.importFenceFdKHR(device, pImportFenceFdInfo);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::getFenceFdKHR (VkDevice device, const VkFenceGetFdInfoKHR* pGetFdInfo, int* pFd) const
{
	if (m_normalMode)
		return m_vk.getFenceFdKHR(device, pGetFdInfo, pFd);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::acquireProfilingLockKHR (VkDevice device, const VkAcquireProfilingLockInfoKHR* pInfo) const
{
	if (m_normalMode)
		return m_vk.acquireProfilingLockKHR(device, pInfo);
	return VK_SUCCESS;
}

void DeviceDriverSC::releaseProfilingLockKHR (VkDevice device) const
{
	if (m_normalMode)
		m_vk.releaseProfilingLockKHR(device);
}

void DeviceDriverSC::cmdSetFragmentShadingRateKHR (VkCommandBuffer commandBuffer, const VkExtent2D* pFragmentSize, const VkFragmentShadingRateCombinerOpKHR combinerOps[2]) const
{
	if (m_normalMode)
		m_vk.cmdSetFragmentShadingRateKHR(commandBuffer, pFragmentSize, combinerOps);
}

void DeviceDriverSC::cmdRefreshObjectsKHR (VkCommandBuffer commandBuffer, const VkRefreshObjectListKHR* pRefreshObjects) const
{
	if (m_normalMode)
		m_vk.cmdRefreshObjectsKHR(commandBuffer, pRefreshObjects);
}

void DeviceDriverSC::cmdSetEvent2KHR (VkCommandBuffer commandBuffer, VkEvent event, const VkDependencyInfoKHR* pDependencyInfo) const
{
	if (m_normalMode)
		m_vk.cmdSetEvent2KHR(commandBuffer, event, pDependencyInfo);
}

void DeviceDriverSC::cmdResetEvent2KHR (VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags2KHR stageMask) const
{
	if (m_normalMode)
		m_vk.cmdResetEvent2KHR(commandBuffer, event, stageMask);
}

void DeviceDriverSC::cmdWaitEvents2KHR (VkCommandBuffer commandBuffer, deUint32 eventCount, const VkEvent* pEvents, const VkDependencyInfoKHR* pDependencyInfos) const
{
	if (m_normalMode)
		m_vk.cmdWaitEvents2KHR(commandBuffer, eventCount, pEvents, pDependencyInfos);
}

void DeviceDriverSC::cmdPipelineBarrier2KHR (VkCommandBuffer commandBuffer, const VkDependencyInfoKHR* pDependencyInfo) const
{
	if (m_normalMode)
		m_vk.cmdPipelineBarrier2KHR(commandBuffer, pDependencyInfo);
}

void DeviceDriverSC::cmdWriteTimestamp2KHR (VkCommandBuffer commandBuffer, VkPipelineStageFlags2KHR stage, VkQueryPool queryPool, deUint32 query) const
{
	if (m_normalMode)
		m_vk.cmdWriteTimestamp2KHR(commandBuffer, stage, queryPool, query);
}

VkResult DeviceDriverSC::queueSubmit2KHR (VkQueue queue, deUint32 submitCount, const VkSubmitInfo2KHR* pSubmits, VkFence fence) const
{
	if (m_normalMode)
		return m_vk.queueSubmit2KHR(queue, submitCount, pSubmits, fence);
	return VK_SUCCESS;
}

void DeviceDriverSC::cmdWriteBufferMarker2AMD (VkCommandBuffer commandBuffer, VkPipelineStageFlags2KHR stage, VkBuffer dstBuffer, VkDeviceSize dstOffset, deUint32 marker) const
{
	if (m_normalMode)
		m_vk.cmdWriteBufferMarker2AMD(commandBuffer, stage, dstBuffer, dstOffset, marker);
}

void DeviceDriverSC::getQueueCheckpointData2NV (VkQueue queue, deUint32* pCheckpointDataCount, VkCheckpointData2NV* pCheckpointData) const
{
	if (m_normalMode)
		m_vk.getQueueCheckpointData2NV(queue, pCheckpointDataCount, pCheckpointData);
}

void DeviceDriverSC::cmdCopyBuffer2KHR (VkCommandBuffer commandBuffer, const VkCopyBufferInfo2KHR* pCopyBufferInfo) const
{
	if (m_normalMode)
		m_vk.cmdCopyBuffer2KHR(commandBuffer, pCopyBufferInfo);
}

void DeviceDriverSC::cmdCopyImage2KHR (VkCommandBuffer commandBuffer, const VkCopyImageInfo2KHR* pCopyImageInfo) const
{
	if (m_normalMode)
		m_vk.cmdCopyImage2KHR(commandBuffer, pCopyImageInfo);
}

void DeviceDriverSC::cmdCopyBufferToImage2KHR (VkCommandBuffer commandBuffer, const VkCopyBufferToImageInfo2KHR* pCopyBufferToImageInfo) const
{
	if (m_normalMode)
		m_vk.cmdCopyBufferToImage2KHR(commandBuffer, pCopyBufferToImageInfo);
}

void DeviceDriverSC::cmdCopyImageToBuffer2KHR (VkCommandBuffer commandBuffer, const VkCopyImageToBufferInfo2KHR* pCopyImageToBufferInfo) const
{
	if (m_normalMode)
		m_vk.cmdCopyImageToBuffer2KHR(commandBuffer, pCopyImageToBufferInfo);
}

void DeviceDriverSC::cmdBlitImage2KHR (VkCommandBuffer commandBuffer, const VkBlitImageInfo2KHR* pBlitImageInfo) const
{
	if (m_normalMode)
		m_vk.cmdBlitImage2KHR(commandBuffer, pBlitImageInfo);
}

void DeviceDriverSC::cmdResolveImage2KHR (VkCommandBuffer commandBuffer, const VkResolveImageInfo2KHR* pResolveImageInfo) const
{
	if (m_normalMode)
		m_vk.cmdResolveImage2KHR(commandBuffer, pResolveImageInfo);
}

VkResult DeviceDriverSC::displayPowerControlEXT (VkDevice device, VkDisplayKHR display, const VkDisplayPowerInfoEXT* pDisplayPowerInfo) const
{
	if (m_normalMode)
		return m_vk.displayPowerControlEXT(device, display, pDisplayPowerInfo);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::registerDeviceEventEXT (VkDevice device, const VkDeviceEventInfoEXT* pDeviceEventInfo, const VkAllocationCallbacks* pAllocator, VkFence* pFence) const
{
	if (m_normalMode)
		return m_vk.registerDeviceEventEXT(device, pDeviceEventInfo, pAllocator, pFence);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::registerDisplayEventEXT (VkDevice device, VkDisplayKHR display, const VkDisplayEventInfoEXT* pDisplayEventInfo, const VkAllocationCallbacks* pAllocator, VkFence* pFence) const
{
	if (m_normalMode)
		return m_vk.registerDisplayEventEXT(device, display, pDisplayEventInfo, pAllocator, pFence);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::getSwapchainCounterEXT (VkDevice device, VkSwapchainKHR swapchain, VkSurfaceCounterFlagBitsEXT counter, deUint64* pCounterValue) const
{
	if (m_normalMode)
		return m_vk.getSwapchainCounterEXT(device, swapchain, counter, pCounterValue);
	return VK_SUCCESS;
}

void DeviceDriverSC::cmdSetDiscardRectangleEXT (VkCommandBuffer commandBuffer, deUint32 firstDiscardRectangle, deUint32 discardRectangleCount, const VkRect2D* pDiscardRectangles) const
{
	if (m_normalMode)
		m_vk.cmdSetDiscardRectangleEXT(commandBuffer, firstDiscardRectangle, discardRectangleCount, pDiscardRectangles);
}

void DeviceDriverSC::setHdrMetadataEXT (VkDevice device, deUint32 swapchainCount, const VkSwapchainKHR* pSwapchains, const VkHdrMetadataEXT* pMetadata) const
{
	if (m_normalMode)
		m_vk.setHdrMetadataEXT(device, swapchainCount, pSwapchains, pMetadata);
}

VkResult DeviceDriverSC::setDebugUtilsObjectNameEXT (VkDevice device, const VkDebugUtilsObjectNameInfoEXT* pNameInfo) const
{
	if (m_normalMode)
		return m_vk.setDebugUtilsObjectNameEXT(device, pNameInfo);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::setDebugUtilsObjectTagEXT (VkDevice device, const VkDebugUtilsObjectTagInfoEXT* pTagInfo) const
{
	if (m_normalMode)
		return m_vk.setDebugUtilsObjectTagEXT(device, pTagInfo);
	return VK_SUCCESS;
}

void DeviceDriverSC::queueBeginDebugUtilsLabelEXT (VkQueue queue, const VkDebugUtilsLabelEXT* pLabelInfo) const
{
	if (m_normalMode)
		m_vk.queueBeginDebugUtilsLabelEXT(queue, pLabelInfo);
}

void DeviceDriverSC::queueEndDebugUtilsLabelEXT (VkQueue queue) const
{
	if (m_normalMode)
		m_vk.queueEndDebugUtilsLabelEXT(queue);
}

void DeviceDriverSC::queueInsertDebugUtilsLabelEXT (VkQueue queue, const VkDebugUtilsLabelEXT* pLabelInfo) const
{
	if (m_normalMode)
		m_vk.queueInsertDebugUtilsLabelEXT(queue, pLabelInfo);
}

void DeviceDriverSC::cmdBeginDebugUtilsLabelEXT (VkCommandBuffer commandBuffer, const VkDebugUtilsLabelEXT* pLabelInfo) const
{
	if (m_normalMode)
		m_vk.cmdBeginDebugUtilsLabelEXT(commandBuffer, pLabelInfo);
}

void DeviceDriverSC::cmdEndDebugUtilsLabelEXT (VkCommandBuffer commandBuffer) const
{
	if (m_normalMode)
		m_vk.cmdEndDebugUtilsLabelEXT(commandBuffer);
}

void DeviceDriverSC::cmdInsertDebugUtilsLabelEXT (VkCommandBuffer commandBuffer, const VkDebugUtilsLabelEXT* pLabelInfo) const
{
	if (m_normalMode)
		m_vk.cmdInsertDebugUtilsLabelEXT(commandBuffer, pLabelInfo);
}

void DeviceDriverSC::cmdSetSampleLocationsEXT (VkCommandBuffer commandBuffer, const VkSampleLocationsInfoEXT* pSampleLocationsInfo) const
{
	if (m_normalMode)
		m_vk.cmdSetSampleLocationsEXT(commandBuffer, pSampleLocationsInfo);
}

VkResult DeviceDriverSC::getImageDrmFormatModifierPropertiesEXT (VkDevice device, VkImage image, VkImageDrmFormatModifierPropertiesEXT* pProperties) const
{
	if (m_normalMode)
		return m_vk.getImageDrmFormatModifierPropertiesEXT(device, image, pProperties);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::getMemoryHostPointerPropertiesEXT (VkDevice device, VkExternalMemoryHandleTypeFlagBits handleType, const void* pHostPointer, VkMemoryHostPointerPropertiesEXT* pMemoryHostPointerProperties) const
{
	if (m_normalMode)
		return m_vk.getMemoryHostPointerPropertiesEXT(device, handleType, pHostPointer, pMemoryHostPointerProperties);
	return VK_SUCCESS;
}

VkResult DeviceDriverSC::getCalibratedTimestampsEXT (VkDevice device, deUint32 timestampCount, const VkCalibratedTimestampInfoEXT* pTimestampInfos, deUint64* pTimestamps, deUint64* pMaxDeviation) const
{
	if (m_normalMode)
		return m_vk.getCalibratedTimestampsEXT(device, timestampCount, pTimestampInfos, pTimestamps, pMaxDeviation);
	return VK_SUCCESS;
}

void DeviceDriverSC::cmdSetLineStippleEXT (VkCommandBuffer commandBuffer, deUint32 lineStippleFactor, deUint16 lineStipplePattern) const
{
	if (m_normalMode)
		m_vk.cmdSetLineStippleEXT(commandBuffer, lineStippleFactor, lineStipplePattern);
}

void DeviceDriverSC::cmdSetCullModeEXT (VkCommandBuffer commandBuffer, VkCullModeFlags cullMode) const
{
	if (m_normalMode)
		m_vk.cmdSetCullModeEXT(commandBuffer, cullMode);
}

void DeviceDriverSC::cmdSetFrontFaceEXT (VkCommandBuffer commandBuffer, VkFrontFace frontFace) const
{
	if (m_normalMode)
		m_vk.cmdSetFrontFaceEXT(commandBuffer, frontFace);
}

void DeviceDriverSC::cmdSetPrimitiveTopologyEXT (VkCommandBuffer commandBuffer, VkPrimitiveTopology primitiveTopology) const
{
	if (m_normalMode)
		m_vk.cmdSetPrimitiveTopologyEXT(commandBuffer, primitiveTopology);
}

void DeviceDriverSC::cmdSetViewportWithCountEXT (VkCommandBuffer commandBuffer, deUint32 viewportCount, const VkViewport* pViewports) const
{
	if (m_normalMode)
		m_vk.cmdSetViewportWithCountEXT(commandBuffer, viewportCount, pViewports);
}

void DeviceDriverSC::cmdSetScissorWithCountEXT (VkCommandBuffer commandBuffer, deUint32 scissorCount, const VkRect2D* pScissors) const
{
	if (m_normalMode)
		m_vk.cmdSetScissorWithCountEXT(commandBuffer, scissorCount, pScissors);
}

void DeviceDriverSC::cmdBindVertexBuffers2EXT (VkCommandBuffer commandBuffer, deUint32 firstBinding, deUint32 bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes, const VkDeviceSize* pStrides) const
{
	if (m_normalMode)
		m_vk.cmdBindVertexBuffers2EXT(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets, pSizes, pStrides);
}

void DeviceDriverSC::cmdSetDepthTestEnableEXT (VkCommandBuffer commandBuffer, VkBool32 depthTestEnable) const
{
	if (m_normalMode)
		m_vk.cmdSetDepthTestEnableEXT(commandBuffer, depthTestEnable);
}

void DeviceDriverSC::cmdSetDepthWriteEnableEXT (VkCommandBuffer commandBuffer, VkBool32 depthWriteEnable) const
{
	if (m_normalMode)
		m_vk.cmdSetDepthWriteEnableEXT(commandBuffer, depthWriteEnable);
}

void DeviceDriverSC::cmdSetDepthCompareOpEXT (VkCommandBuffer commandBuffer, VkCompareOp depthCompareOp) const
{
	if (m_normalMode)
		m_vk.cmdSetDepthCompareOpEXT(commandBuffer, depthCompareOp);
}

void DeviceDriverSC::cmdSetDepthBoundsTestEnableEXT (VkCommandBuffer commandBuffer, VkBool32 depthBoundsTestEnable) const
{
	if (m_normalMode)
		m_vk.cmdSetDepthBoundsTestEnableEXT(commandBuffer, depthBoundsTestEnable);
}

void DeviceDriverSC::cmdSetStencilTestEnableEXT (VkCommandBuffer commandBuffer, VkBool32 stencilTestEnable) const
{
	if (m_normalMode)
		m_vk.cmdSetStencilTestEnableEXT(commandBuffer, stencilTestEnable);
}

void DeviceDriverSC::cmdSetStencilOpEXT (VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp) const
{
	if (m_normalMode)
		m_vk.cmdSetStencilOpEXT(commandBuffer, faceMask, failOp, passOp, depthFailOp, compareOp);
}

void DeviceDriverSC::cmdSetVertexInputEXT (VkCommandBuffer commandBuffer, deUint32 vertexBindingDescriptionCount, const VkVertexInputBindingDescription2EXT* pVertexBindingDescriptions, deUint32 vertexAttributeDescriptionCount, const VkVertexInputAttributeDescription2EXT* pVertexAttributeDescriptions) const
{
	if (m_normalMode)
		m_vk.cmdSetVertexInputEXT(commandBuffer, vertexBindingDescriptionCount, pVertexBindingDescriptions, vertexAttributeDescriptionCount, pVertexAttributeDescriptions);
}

void DeviceDriverSC::cmdSetPatchControlPointsEXT (VkCommandBuffer commandBuffer, deUint32 patchControlPoints) const
{
	if (m_normalMode)
		m_vk.cmdSetPatchControlPointsEXT(commandBuffer, patchControlPoints);
}

void DeviceDriverSC::cmdSetRasterizerDiscardEnableEXT (VkCommandBuffer commandBuffer, VkBool32 rasterizerDiscardEnable) const
{
	if (m_normalMode)
		m_vk.cmdSetRasterizerDiscardEnableEXT(commandBuffer, rasterizerDiscardEnable);
}

void DeviceDriverSC::cmdSetDepthBiasEnableEXT (VkCommandBuffer commandBuffer, VkBool32 depthBiasEnable) const
{
	if (m_normalMode)
		m_vk.cmdSetDepthBiasEnableEXT(commandBuffer, depthBiasEnable);
}

void DeviceDriverSC::cmdSetLogicOpEXT (VkCommandBuffer commandBuffer, VkLogicOp logicOp) const
{
	if (m_normalMode)
		m_vk.cmdSetLogicOpEXT(commandBuffer, logicOp);
}

void DeviceDriverSC::cmdSetPrimitiveRestartEnableEXT (VkCommandBuffer commandBuffer, VkBool32 primitiveRestartEnable) const
{
	if (m_normalMode)
		m_vk.cmdSetPrimitiveRestartEnableEXT(commandBuffer, primitiveRestartEnable);
}

void DeviceDriverSC::cmdSetColorWriteEnableEXT (VkCommandBuffer commandBuffer, deUint32 attachmentCount, const VkBool32* pColorWriteEnables) const
{
	if (m_normalMode)
		m_vk.cmdSetColorWriteEnableEXT(commandBuffer, attachmentCount, pColorWriteEnables);
}
