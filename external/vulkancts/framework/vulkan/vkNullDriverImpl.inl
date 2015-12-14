/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 */
VKAPI_ATTR VkResult VKAPI_CALL createInstance (const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN(*pInstance = reinterpret_cast<VkInstance>(new Instance(pCreateInfo)));
}

VKAPI_ATTR VkResult VKAPI_CALL createDevice (VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN(*pDevice = reinterpret_cast<VkDevice>(new Device(physicalDevice, pCreateInfo)));
}

VKAPI_ATTR VkResult VKAPI_CALL allocateMemory (VkDevice device, const VkMemoryAllocateInfo* pAllocateInfo, const VkAllocationCallbacks* pAllocator, VkDeviceMemory* pMemory)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN(*pMemory = VkDeviceMemory((deUint64)(deUintptr)new DeviceMemory(device, pAllocateInfo)));
}

VKAPI_ATTR VkResult VKAPI_CALL createFence (VkDevice device, const VkFenceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFence* pFence)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN(*pFence = VkFence((deUint64)(deUintptr)new Fence(device, pCreateInfo)));
}

VKAPI_ATTR VkResult VKAPI_CALL createSemaphore (VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSemaphore* pSemaphore)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN(*pSemaphore = VkSemaphore((deUint64)(deUintptr)new Semaphore(device, pCreateInfo)));
}

VKAPI_ATTR VkResult VKAPI_CALL createEvent (VkDevice device, const VkEventCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkEvent* pEvent)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN(*pEvent = VkEvent((deUint64)(deUintptr)new Event(device, pCreateInfo)));
}

VKAPI_ATTR VkResult VKAPI_CALL createQueryPool (VkDevice device, const VkQueryPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkQueryPool* pQueryPool)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN(*pQueryPool = VkQueryPool((deUint64)(deUintptr)new QueryPool(device, pCreateInfo)));
}

VKAPI_ATTR VkResult VKAPI_CALL createBuffer (VkDevice device, const VkBufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBuffer* pBuffer)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN(*pBuffer = VkBuffer((deUint64)(deUintptr)new Buffer(device, pCreateInfo)));
}

VKAPI_ATTR VkResult VKAPI_CALL createBufferView (VkDevice device, const VkBufferViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBufferView* pView)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN(*pView = VkBufferView((deUint64)(deUintptr)new BufferView(device, pCreateInfo)));
}

VKAPI_ATTR VkResult VKAPI_CALL createImage (VkDevice device, const VkImageCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImage* pImage)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN(*pImage = VkImage((deUint64)(deUintptr)new Image(device, pCreateInfo)));
}

VKAPI_ATTR VkResult VKAPI_CALL createImageView (VkDevice device, const VkImageViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImageView* pView)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN(*pView = VkImageView((deUint64)(deUintptr)new ImageView(device, pCreateInfo)));
}

VKAPI_ATTR VkResult VKAPI_CALL createShaderModule (VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkShaderModule* pShaderModule)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN(*pShaderModule = VkShaderModule((deUint64)(deUintptr)new ShaderModule(device, pCreateInfo)));
}

VKAPI_ATTR VkResult VKAPI_CALL createPipelineCache (VkDevice device, const VkPipelineCacheCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineCache* pPipelineCache)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN(*pPipelineCache = VkPipelineCache((deUint64)(deUintptr)new PipelineCache(device, pCreateInfo)));
}

VKAPI_ATTR VkResult VKAPI_CALL createPipelineLayout (VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineLayout* pPipelineLayout)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN(*pPipelineLayout = VkPipelineLayout((deUint64)(deUintptr)new PipelineLayout(device, pCreateInfo)));
}

VKAPI_ATTR VkResult VKAPI_CALL createSampler (VkDevice device, const VkSamplerCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSampler* pSampler)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN(*pSampler = VkSampler((deUint64)(deUintptr)new Sampler(device, pCreateInfo)));
}

VKAPI_ATTR VkResult VKAPI_CALL createDescriptorSetLayout (VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorSetLayout* pSetLayout)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN(*pSetLayout = VkDescriptorSetLayout((deUint64)(deUintptr)new DescriptorSetLayout(device, pCreateInfo)));
}

VKAPI_ATTR VkResult VKAPI_CALL createDescriptorPool (VkDevice device, const VkDescriptorPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorPool* pDescriptorPool)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN(*pDescriptorPool = VkDescriptorPool((deUint64)(deUintptr)new DescriptorPool(device, pCreateInfo)));
}

VKAPI_ATTR VkResult VKAPI_CALL createFramebuffer (VkDevice device, const VkFramebufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFramebuffer* pFramebuffer)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN(*pFramebuffer = VkFramebuffer((deUint64)(deUintptr)new Framebuffer(device, pCreateInfo)));
}

VKAPI_ATTR VkResult VKAPI_CALL createRenderPass (VkDevice device, const VkRenderPassCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN(*pRenderPass = VkRenderPass((deUint64)(deUintptr)new RenderPass(device, pCreateInfo)));
}

VKAPI_ATTR VkResult VKAPI_CALL createCommandPool (VkDevice device, const VkCommandPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkCommandPool* pCommandPool)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN(*pCommandPool = VkCommandPool((deUint64)(deUintptr)new CommandPool(device, pCreateInfo)));
}

VKAPI_ATTR void VKAPI_CALL destroyInstance (VkInstance instance, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(pAllocator);
	delete reinterpret_cast<Instance*>(instance);
}

VKAPI_ATTR void VKAPI_CALL destroyDevice (VkDevice device, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(pAllocator);
	delete reinterpret_cast<Device*>(device);
}

VKAPI_ATTR void VKAPI_CALL freeMemory (VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	DE_UNREF(pAllocator);
	delete reinterpret_cast<DeviceMemory*>((deUintptr)memory.getInternal());
}

VKAPI_ATTR void VKAPI_CALL destroyFence (VkDevice device, VkFence fence, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	DE_UNREF(pAllocator);
	delete reinterpret_cast<Fence*>((deUintptr)fence.getInternal());
}

VKAPI_ATTR void VKAPI_CALL destroySemaphore (VkDevice device, VkSemaphore semaphore, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	DE_UNREF(pAllocator);
	delete reinterpret_cast<Semaphore*>((deUintptr)semaphore.getInternal());
}

VKAPI_ATTR void VKAPI_CALL destroyEvent (VkDevice device, VkEvent event, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	DE_UNREF(pAllocator);
	delete reinterpret_cast<Event*>((deUintptr)event.getInternal());
}

VKAPI_ATTR void VKAPI_CALL destroyQueryPool (VkDevice device, VkQueryPool queryPool, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	DE_UNREF(pAllocator);
	delete reinterpret_cast<QueryPool*>((deUintptr)queryPool.getInternal());
}

VKAPI_ATTR void VKAPI_CALL destroyBuffer (VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	DE_UNREF(pAllocator);
	delete reinterpret_cast<Buffer*>((deUintptr)buffer.getInternal());
}

VKAPI_ATTR void VKAPI_CALL destroyBufferView (VkDevice device, VkBufferView bufferView, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	DE_UNREF(pAllocator);
	delete reinterpret_cast<BufferView*>((deUintptr)bufferView.getInternal());
}

VKAPI_ATTR void VKAPI_CALL destroyImage (VkDevice device, VkImage image, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	DE_UNREF(pAllocator);
	delete reinterpret_cast<Image*>((deUintptr)image.getInternal());
}

VKAPI_ATTR void VKAPI_CALL destroyImageView (VkDevice device, VkImageView imageView, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	DE_UNREF(pAllocator);
	delete reinterpret_cast<ImageView*>((deUintptr)imageView.getInternal());
}

VKAPI_ATTR void VKAPI_CALL destroyShaderModule (VkDevice device, VkShaderModule shaderModule, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	DE_UNREF(pAllocator);
	delete reinterpret_cast<ShaderModule*>((deUintptr)shaderModule.getInternal());
}

VKAPI_ATTR void VKAPI_CALL destroyPipelineCache (VkDevice device, VkPipelineCache pipelineCache, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	DE_UNREF(pAllocator);
	delete reinterpret_cast<PipelineCache*>((deUintptr)pipelineCache.getInternal());
}

VKAPI_ATTR void VKAPI_CALL destroyPipeline (VkDevice device, VkPipeline pipeline, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	DE_UNREF(pAllocator);
	delete reinterpret_cast<Pipeline*>((deUintptr)pipeline.getInternal());
}

VKAPI_ATTR void VKAPI_CALL destroyPipelineLayout (VkDevice device, VkPipelineLayout pipelineLayout, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	DE_UNREF(pAllocator);
	delete reinterpret_cast<PipelineLayout*>((deUintptr)pipelineLayout.getInternal());
}

VKAPI_ATTR void VKAPI_CALL destroySampler (VkDevice device, VkSampler sampler, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	DE_UNREF(pAllocator);
	delete reinterpret_cast<Sampler*>((deUintptr)sampler.getInternal());
}

VKAPI_ATTR void VKAPI_CALL destroyDescriptorSetLayout (VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	DE_UNREF(pAllocator);
	delete reinterpret_cast<DescriptorSetLayout*>((deUintptr)descriptorSetLayout.getInternal());
}

VKAPI_ATTR void VKAPI_CALL destroyDescriptorPool (VkDevice device, VkDescriptorPool descriptorPool, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	DE_UNREF(pAllocator);
	delete reinterpret_cast<DescriptorPool*>((deUintptr)descriptorPool.getInternal());
}

VKAPI_ATTR void VKAPI_CALL destroyFramebuffer (VkDevice device, VkFramebuffer framebuffer, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	DE_UNREF(pAllocator);
	delete reinterpret_cast<Framebuffer*>((deUintptr)framebuffer.getInternal());
}

VKAPI_ATTR void VKAPI_CALL destroyRenderPass (VkDevice device, VkRenderPass renderPass, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	DE_UNREF(pAllocator);
	delete reinterpret_cast<RenderPass*>((deUintptr)renderPass.getInternal());
}

VKAPI_ATTR void VKAPI_CALL destroyCommandPool (VkDevice device, VkCommandPool commandPool, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	DE_UNREF(pAllocator);
	delete reinterpret_cast<CommandPool*>((deUintptr)commandPool.getInternal());
}

VKAPI_ATTR void VKAPI_CALL getPhysicalDeviceFeatures (VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures* pFeatures)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(pFeatures);
}

VKAPI_ATTR VkResult VKAPI_CALL getPhysicalDeviceImageFormatProperties (VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties* pImageFormatProperties)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(format);
	DE_UNREF(type);
	DE_UNREF(tiling);
	DE_UNREF(usage);
	DE_UNREF(flags);
	DE_UNREF(pImageFormatProperties);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL enumerateInstanceExtensionProperties (const char* pLayerName, deUint32* pPropertyCount, VkExtensionProperties* pProperties)
{
	DE_UNREF(pLayerName);
	DE_UNREF(pPropertyCount);
	DE_UNREF(pProperties);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL enumerateDeviceExtensionProperties (VkPhysicalDevice physicalDevice, const char* pLayerName, deUint32* pPropertyCount, VkExtensionProperties* pProperties)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(pLayerName);
	DE_UNREF(pPropertyCount);
	DE_UNREF(pProperties);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL enumerateInstanceLayerProperties (deUint32* pPropertyCount, VkLayerProperties* pProperties)
{
	DE_UNREF(pPropertyCount);
	DE_UNREF(pProperties);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL enumerateDeviceLayerProperties (VkPhysicalDevice physicalDevice, deUint32* pPropertyCount, VkLayerProperties* pProperties)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(pPropertyCount);
	DE_UNREF(pProperties);
	return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL getDeviceQueue (VkDevice device, deUint32 queueFamilyIndex, deUint32 queueIndex, VkQueue* pQueue)
{
	DE_UNREF(device);
	DE_UNREF(queueFamilyIndex);
	DE_UNREF(queueIndex);
	DE_UNREF(pQueue);
}

VKAPI_ATTR VkResult VKAPI_CALL queueSubmit (VkQueue queue, deUint32 submitCount, const VkSubmitInfo* pSubmits, VkFence fence)
{
	DE_UNREF(queue);
	DE_UNREF(submitCount);
	DE_UNREF(pSubmits);
	DE_UNREF(fence);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL queueWaitIdle (VkQueue queue)
{
	DE_UNREF(queue);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL deviceWaitIdle (VkDevice device)
{
	DE_UNREF(device);
	return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL unmapMemory (VkDevice device, VkDeviceMemory memory)
{
	DE_UNREF(device);
	DE_UNREF(memory);
}

VKAPI_ATTR VkResult VKAPI_CALL flushMappedMemoryRanges (VkDevice device, deUint32 memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges)
{
	DE_UNREF(device);
	DE_UNREF(memoryRangeCount);
	DE_UNREF(pMemoryRanges);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL invalidateMappedMemoryRanges (VkDevice device, deUint32 memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges)
{
	DE_UNREF(device);
	DE_UNREF(memoryRangeCount);
	DE_UNREF(pMemoryRanges);
	return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL getDeviceMemoryCommitment (VkDevice device, VkDeviceMemory memory, VkDeviceSize* pCommittedMemoryInBytes)
{
	DE_UNREF(device);
	DE_UNREF(memory);
	DE_UNREF(pCommittedMemoryInBytes);
}

VKAPI_ATTR VkResult VKAPI_CALL bindBufferMemory (VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset)
{
	DE_UNREF(device);
	DE_UNREF(buffer);
	DE_UNREF(memory);
	DE_UNREF(memoryOffset);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL bindImageMemory (VkDevice device, VkImage image, VkDeviceMemory memory, VkDeviceSize memoryOffset)
{
	DE_UNREF(device);
	DE_UNREF(image);
	DE_UNREF(memory);
	DE_UNREF(memoryOffset);
	return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL getImageSparseMemoryRequirements (VkDevice device, VkImage image, deUint32* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements* pSparseMemoryRequirements)
{
	DE_UNREF(device);
	DE_UNREF(image);
	DE_UNREF(pSparseMemoryRequirementCount);
	DE_UNREF(pSparseMemoryRequirements);
}

VKAPI_ATTR void VKAPI_CALL getPhysicalDeviceSparseImageFormatProperties (VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkSampleCountFlagBits samples, VkImageUsageFlags usage, VkImageTiling tiling, deUint32* pPropertyCount, VkSparseImageFormatProperties* pProperties)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(format);
	DE_UNREF(type);
	DE_UNREF(samples);
	DE_UNREF(usage);
	DE_UNREF(tiling);
	DE_UNREF(pPropertyCount);
	DE_UNREF(pProperties);
}

VKAPI_ATTR VkResult VKAPI_CALL queueBindSparse (VkQueue queue, deUint32 bindInfoCount, const VkBindSparseInfo* pBindInfo, VkFence fence)
{
	DE_UNREF(queue);
	DE_UNREF(bindInfoCount);
	DE_UNREF(pBindInfo);
	DE_UNREF(fence);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL resetFences (VkDevice device, deUint32 fenceCount, const VkFence* pFences)
{
	DE_UNREF(device);
	DE_UNREF(fenceCount);
	DE_UNREF(pFences);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getFenceStatus (VkDevice device, VkFence fence)
{
	DE_UNREF(device);
	DE_UNREF(fence);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL waitForFences (VkDevice device, deUint32 fenceCount, const VkFence* pFences, VkBool32 waitAll, deUint64 timeout)
{
	DE_UNREF(device);
	DE_UNREF(fenceCount);
	DE_UNREF(pFences);
	DE_UNREF(waitAll);
	DE_UNREF(timeout);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getEventStatus (VkDevice device, VkEvent event)
{
	DE_UNREF(device);
	DE_UNREF(event);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL setEvent (VkDevice device, VkEvent event)
{
	DE_UNREF(device);
	DE_UNREF(event);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL resetEvent (VkDevice device, VkEvent event)
{
	DE_UNREF(device);
	DE_UNREF(event);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getQueryPoolResults (VkDevice device, VkQueryPool queryPool, deUint32 startQuery, deUint32 queryCount, deUintptr dataSize, void* pData, VkDeviceSize stride, VkQueryResultFlags flags)
{
	DE_UNREF(device);
	DE_UNREF(queryPool);
	DE_UNREF(startQuery);
	DE_UNREF(queryCount);
	DE_UNREF(dataSize);
	DE_UNREF(pData);
	DE_UNREF(stride);
	DE_UNREF(flags);
	return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL getImageSubresourceLayout (VkDevice device, VkImage image, const VkImageSubresource* pSubresource, VkSubresourceLayout* pLayout)
{
	DE_UNREF(device);
	DE_UNREF(image);
	DE_UNREF(pSubresource);
	DE_UNREF(pLayout);
}

VKAPI_ATTR VkResult VKAPI_CALL getPipelineCacheData (VkDevice device, VkPipelineCache pipelineCache, deUintptr* pDataSize, void* pData)
{
	DE_UNREF(device);
	DE_UNREF(pipelineCache);
	DE_UNREF(pDataSize);
	DE_UNREF(pData);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL mergePipelineCaches (VkDevice device, VkPipelineCache dstCache, deUint32 srcCacheCount, const VkPipelineCache* pSrcCaches)
{
	DE_UNREF(device);
	DE_UNREF(dstCache);
	DE_UNREF(srcCacheCount);
	DE_UNREF(pSrcCaches);
	return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL updateDescriptorSets (VkDevice device, deUint32 descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites, deUint32 descriptorCopyCount, const VkCopyDescriptorSet* pDescriptorCopies)
{
	DE_UNREF(device);
	DE_UNREF(descriptorWriteCount);
	DE_UNREF(pDescriptorWrites);
	DE_UNREF(descriptorCopyCount);
	DE_UNREF(pDescriptorCopies);
}

VKAPI_ATTR void VKAPI_CALL getRenderAreaGranularity (VkDevice device, VkRenderPass renderPass, VkExtent2D* pGranularity)
{
	DE_UNREF(device);
	DE_UNREF(renderPass);
	DE_UNREF(pGranularity);
}

VKAPI_ATTR VkResult VKAPI_CALL resetCommandPool (VkDevice device, VkCommandPool commandPool, VkCommandPoolResetFlags flags)
{
	DE_UNREF(device);
	DE_UNREF(commandPool);
	DE_UNREF(flags);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL beginCommandBuffer (VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo* pBeginInfo)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pBeginInfo);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL endCommandBuffer (VkCommandBuffer commandBuffer)
{
	DE_UNREF(commandBuffer);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL resetCommandBuffer (VkCommandBuffer commandBuffer, VkCommandBufferResetFlags flags)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(flags);
	return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL cmdBindPipeline (VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pipelineBindPoint);
	DE_UNREF(pipeline);
}

VKAPI_ATTR void VKAPI_CALL cmdSetViewport (VkCommandBuffer commandBuffer, deUint32 viewportCount, const VkViewport* pViewports)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(viewportCount);
	DE_UNREF(pViewports);
}

VKAPI_ATTR void VKAPI_CALL cmdSetScissor (VkCommandBuffer commandBuffer, deUint32 scissorCount, const VkRect2D* pScissors)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(scissorCount);
	DE_UNREF(pScissors);
}

VKAPI_ATTR void VKAPI_CALL cmdSetLineWidth (VkCommandBuffer commandBuffer, float lineWidth)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(lineWidth);
}

VKAPI_ATTR void VKAPI_CALL cmdSetDepthBias (VkCommandBuffer commandBuffer, float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(depthBiasConstantFactor);
	DE_UNREF(depthBiasClamp);
	DE_UNREF(depthBiasSlopeFactor);
}

VKAPI_ATTR void VKAPI_CALL cmdSetBlendConstants (VkCommandBuffer commandBuffer, const float blendConstants[4])
{
	DE_UNREF(commandBuffer);
	DE_UNREF(blendConstants);
}

VKAPI_ATTR void VKAPI_CALL cmdSetDepthBounds (VkCommandBuffer commandBuffer, float minDepthBounds, float maxDepthBounds)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(minDepthBounds);
	DE_UNREF(maxDepthBounds);
}

VKAPI_ATTR void VKAPI_CALL cmdSetStencilCompareMask (VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, deUint32 compareMask)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(faceMask);
	DE_UNREF(compareMask);
}

VKAPI_ATTR void VKAPI_CALL cmdSetStencilWriteMask (VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, deUint32 writeMask)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(faceMask);
	DE_UNREF(writeMask);
}

VKAPI_ATTR void VKAPI_CALL cmdSetStencilReference (VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, deUint32 reference)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(faceMask);
	DE_UNREF(reference);
}

VKAPI_ATTR void VKAPI_CALL cmdBindDescriptorSets (VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, deUint32 firstSet, deUint32 descriptorSetCount, const VkDescriptorSet* pDescriptorSets, deUint32 dynamicOffsetCount, const deUint32* pDynamicOffsets)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pipelineBindPoint);
	DE_UNREF(layout);
	DE_UNREF(firstSet);
	DE_UNREF(descriptorSetCount);
	DE_UNREF(pDescriptorSets);
	DE_UNREF(dynamicOffsetCount);
	DE_UNREF(pDynamicOffsets);
}

VKAPI_ATTR void VKAPI_CALL cmdBindIndexBuffer (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(buffer);
	DE_UNREF(offset);
	DE_UNREF(indexType);
}

VKAPI_ATTR void VKAPI_CALL cmdBindVertexBuffers (VkCommandBuffer commandBuffer, deUint32 startBinding, deUint32 bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(startBinding);
	DE_UNREF(bindingCount);
	DE_UNREF(pBuffers);
	DE_UNREF(pOffsets);
}

VKAPI_ATTR void VKAPI_CALL cmdDraw (VkCommandBuffer commandBuffer, deUint32 vertexCount, deUint32 instanceCount, deUint32 firstVertex, deUint32 firstInstance)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(vertexCount);
	DE_UNREF(instanceCount);
	DE_UNREF(firstVertex);
	DE_UNREF(firstInstance);
}

VKAPI_ATTR void VKAPI_CALL cmdDrawIndexed (VkCommandBuffer commandBuffer, deUint32 indexCount, deUint32 instanceCount, deUint32 firstIndex, deInt32 vertexOffset, deUint32 firstInstance)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(indexCount);
	DE_UNREF(instanceCount);
	DE_UNREF(firstIndex);
	DE_UNREF(vertexOffset);
	DE_UNREF(firstInstance);
}

VKAPI_ATTR void VKAPI_CALL cmdDrawIndirect (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, deUint32 drawCount, deUint32 stride)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(buffer);
	DE_UNREF(offset);
	DE_UNREF(drawCount);
	DE_UNREF(stride);
}

VKAPI_ATTR void VKAPI_CALL cmdDrawIndexedIndirect (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, deUint32 drawCount, deUint32 stride)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(buffer);
	DE_UNREF(offset);
	DE_UNREF(drawCount);
	DE_UNREF(stride);
}

VKAPI_ATTR void VKAPI_CALL cmdDispatch (VkCommandBuffer commandBuffer, deUint32 x, deUint32 y, deUint32 z)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(x);
	DE_UNREF(y);
	DE_UNREF(z);
}

VKAPI_ATTR void VKAPI_CALL cmdDispatchIndirect (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(buffer);
	DE_UNREF(offset);
}

VKAPI_ATTR void VKAPI_CALL cmdCopyBuffer (VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, deUint32 regionCount, const VkBufferCopy* pRegions)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(srcBuffer);
	DE_UNREF(dstBuffer);
	DE_UNREF(regionCount);
	DE_UNREF(pRegions);
}

VKAPI_ATTR void VKAPI_CALL cmdCopyImage (VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, deUint32 regionCount, const VkImageCopy* pRegions)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(srcImage);
	DE_UNREF(srcImageLayout);
	DE_UNREF(dstImage);
	DE_UNREF(dstImageLayout);
	DE_UNREF(regionCount);
	DE_UNREF(pRegions);
}

VKAPI_ATTR void VKAPI_CALL cmdBlitImage (VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, deUint32 regionCount, const VkImageBlit* pRegions, VkFilter filter)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(srcImage);
	DE_UNREF(srcImageLayout);
	DE_UNREF(dstImage);
	DE_UNREF(dstImageLayout);
	DE_UNREF(regionCount);
	DE_UNREF(pRegions);
	DE_UNREF(filter);
}

VKAPI_ATTR void VKAPI_CALL cmdCopyBufferToImage (VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkImage dstImage, VkImageLayout dstImageLayout, deUint32 regionCount, const VkBufferImageCopy* pRegions)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(srcBuffer);
	DE_UNREF(dstImage);
	DE_UNREF(dstImageLayout);
	DE_UNREF(regionCount);
	DE_UNREF(pRegions);
}

VKAPI_ATTR void VKAPI_CALL cmdCopyImageToBuffer (VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkBuffer dstBuffer, deUint32 regionCount, const VkBufferImageCopy* pRegions)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(srcImage);
	DE_UNREF(srcImageLayout);
	DE_UNREF(dstBuffer);
	DE_UNREF(regionCount);
	DE_UNREF(pRegions);
}

VKAPI_ATTR void VKAPI_CALL cmdUpdateBuffer (VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize dataSize, const deUint32* pData)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(dstBuffer);
	DE_UNREF(dstOffset);
	DE_UNREF(dataSize);
	DE_UNREF(pData);
}

VKAPI_ATTR void VKAPI_CALL cmdFillBuffer (VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize size, deUint32 data)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(dstBuffer);
	DE_UNREF(dstOffset);
	DE_UNREF(size);
	DE_UNREF(data);
}

VKAPI_ATTR void VKAPI_CALL cmdClearColorImage (VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearColorValue* pColor, deUint32 rangeCount, const VkImageSubresourceRange* pRanges)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(image);
	DE_UNREF(imageLayout);
	DE_UNREF(pColor);
	DE_UNREF(rangeCount);
	DE_UNREF(pRanges);
}

VKAPI_ATTR void VKAPI_CALL cmdClearDepthStencilImage (VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue* pDepthStencil, deUint32 rangeCount, const VkImageSubresourceRange* pRanges)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(image);
	DE_UNREF(imageLayout);
	DE_UNREF(pDepthStencil);
	DE_UNREF(rangeCount);
	DE_UNREF(pRanges);
}

VKAPI_ATTR void VKAPI_CALL cmdClearAttachments (VkCommandBuffer commandBuffer, deUint32 attachmentCount, const VkClearAttachment* pAttachments, deUint32 rectCount, const VkClearRect* pRects)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(attachmentCount);
	DE_UNREF(pAttachments);
	DE_UNREF(rectCount);
	DE_UNREF(pRects);
}

VKAPI_ATTR void VKAPI_CALL cmdResolveImage (VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, deUint32 regionCount, const VkImageResolve* pRegions)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(srcImage);
	DE_UNREF(srcImageLayout);
	DE_UNREF(dstImage);
	DE_UNREF(dstImageLayout);
	DE_UNREF(regionCount);
	DE_UNREF(pRegions);
}

VKAPI_ATTR void VKAPI_CALL cmdSetEvent (VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(event);
	DE_UNREF(stageMask);
}

VKAPI_ATTR void VKAPI_CALL cmdResetEvent (VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(event);
	DE_UNREF(stageMask);
}

VKAPI_ATTR void VKAPI_CALL cmdWaitEvents (VkCommandBuffer commandBuffer, deUint32 eventCount, const VkEvent* pEvents, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, deUint32 memoryBarrierCount, const void* const* ppMemoryBarriers)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(eventCount);
	DE_UNREF(pEvents);
	DE_UNREF(srcStageMask);
	DE_UNREF(dstStageMask);
	DE_UNREF(memoryBarrierCount);
	DE_UNREF(ppMemoryBarriers);
}

VKAPI_ATTR void VKAPI_CALL cmdPipelineBarrier (VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags, deUint32 memoryBarrierCount, const void* const* ppMemoryBarriers)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(srcStageMask);
	DE_UNREF(dstStageMask);
	DE_UNREF(dependencyFlags);
	DE_UNREF(memoryBarrierCount);
	DE_UNREF(ppMemoryBarriers);
}

VKAPI_ATTR void VKAPI_CALL cmdBeginQuery (VkCommandBuffer commandBuffer, VkQueryPool queryPool, deUint32 entry, VkQueryControlFlags flags)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(queryPool);
	DE_UNREF(entry);
	DE_UNREF(flags);
}

VKAPI_ATTR void VKAPI_CALL cmdEndQuery (VkCommandBuffer commandBuffer, VkQueryPool queryPool, deUint32 entry)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(queryPool);
	DE_UNREF(entry);
}

VKAPI_ATTR void VKAPI_CALL cmdResetQueryPool (VkCommandBuffer commandBuffer, VkQueryPool queryPool, deUint32 startQuery, deUint32 queryCount)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(queryPool);
	DE_UNREF(startQuery);
	DE_UNREF(queryCount);
}

VKAPI_ATTR void VKAPI_CALL cmdWriteTimestamp (VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage, VkQueryPool queryPool, deUint32 entry)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pipelineStage);
	DE_UNREF(queryPool);
	DE_UNREF(entry);
}

VKAPI_ATTR void VKAPI_CALL cmdCopyQueryPoolResults (VkCommandBuffer commandBuffer, VkQueryPool queryPool, deUint32 startQuery, deUint32 queryCount, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize stride, VkQueryResultFlags flags)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(queryPool);
	DE_UNREF(startQuery);
	DE_UNREF(queryCount);
	DE_UNREF(dstBuffer);
	DE_UNREF(dstOffset);
	DE_UNREF(stride);
	DE_UNREF(flags);
}

VKAPI_ATTR void VKAPI_CALL cmdPushConstants (VkCommandBuffer commandBuffer, VkPipelineLayout layout, VkShaderStageFlags stageFlags, deUint32 offset, deUint32 size, const void* pValues)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(layout);
	DE_UNREF(stageFlags);
	DE_UNREF(offset);
	DE_UNREF(size);
	DE_UNREF(pValues);
}

VKAPI_ATTR void VKAPI_CALL cmdBeginRenderPass (VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, VkSubpassContents contents)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pRenderPassBegin);
	DE_UNREF(contents);
}

VKAPI_ATTR void VKAPI_CALL cmdNextSubpass (VkCommandBuffer commandBuffer, VkSubpassContents contents)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(contents);
}

VKAPI_ATTR void VKAPI_CALL cmdEndRenderPass (VkCommandBuffer commandBuffer)
{
	DE_UNREF(commandBuffer);
}

VKAPI_ATTR void VKAPI_CALL cmdExecuteCommands (VkCommandBuffer commandBuffer, deUint32 commandBuffersCount, const VkCommandBuffer* pCommandBuffers)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(commandBuffersCount);
	DE_UNREF(pCommandBuffers);
}

static const tcu::StaticFunctionLibrary::Entry s_platformFunctions[] =
{
	VK_NULL_FUNC_ENTRY(vkCreateInstance,						createInstance),
	VK_NULL_FUNC_ENTRY(vkGetInstanceProcAddr,					getInstanceProcAddr),
	VK_NULL_FUNC_ENTRY(vkEnumerateInstanceExtensionProperties,	enumerateInstanceExtensionProperties),
	VK_NULL_FUNC_ENTRY(vkEnumerateInstanceLayerProperties,		enumerateInstanceLayerProperties),
};

static const tcu::StaticFunctionLibrary::Entry s_instanceFunctions[] =
{
	VK_NULL_FUNC_ENTRY(vkDestroyInstance,							destroyInstance),
	VK_NULL_FUNC_ENTRY(vkEnumeratePhysicalDevices,					enumeratePhysicalDevices),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceFeatures,					getPhysicalDeviceFeatures),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceFormatProperties,			getPhysicalDeviceFormatProperties),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceImageFormatProperties,	getPhysicalDeviceImageFormatProperties),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceProperties,				getPhysicalDeviceProperties),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceQueueFamilyProperties,	getPhysicalDeviceQueueFamilyProperties),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceMemoryProperties,			getPhysicalDeviceMemoryProperties),
	VK_NULL_FUNC_ENTRY(vkGetDeviceProcAddr,							getDeviceProcAddr),
	VK_NULL_FUNC_ENTRY(vkCreateDevice,								createDevice),
	VK_NULL_FUNC_ENTRY(vkEnumerateDeviceExtensionProperties,		enumerateDeviceExtensionProperties),
	VK_NULL_FUNC_ENTRY(vkEnumerateDeviceLayerProperties,			enumerateDeviceLayerProperties),
};

static const tcu::StaticFunctionLibrary::Entry s_deviceFunctions[] =
{
	VK_NULL_FUNC_ENTRY(vkDestroyDevice,									destroyDevice),
	VK_NULL_FUNC_ENTRY(vkGetDeviceQueue,								getDeviceQueue),
	VK_NULL_FUNC_ENTRY(vkQueueSubmit,									queueSubmit),
	VK_NULL_FUNC_ENTRY(vkQueueWaitIdle,									queueWaitIdle),
	VK_NULL_FUNC_ENTRY(vkDeviceWaitIdle,								deviceWaitIdle),
	VK_NULL_FUNC_ENTRY(vkAllocateMemory,								allocateMemory),
	VK_NULL_FUNC_ENTRY(vkFreeMemory,									freeMemory),
	VK_NULL_FUNC_ENTRY(vkMapMemory,										mapMemory),
	VK_NULL_FUNC_ENTRY(vkUnmapMemory,									unmapMemory),
	VK_NULL_FUNC_ENTRY(vkFlushMappedMemoryRanges,						flushMappedMemoryRanges),
	VK_NULL_FUNC_ENTRY(vkInvalidateMappedMemoryRanges,					invalidateMappedMemoryRanges),
	VK_NULL_FUNC_ENTRY(vkGetDeviceMemoryCommitment,						getDeviceMemoryCommitment),
	VK_NULL_FUNC_ENTRY(vkBindBufferMemory,								bindBufferMemory),
	VK_NULL_FUNC_ENTRY(vkBindImageMemory,								bindImageMemory),
	VK_NULL_FUNC_ENTRY(vkGetBufferMemoryRequirements,					getBufferMemoryRequirements),
	VK_NULL_FUNC_ENTRY(vkGetImageMemoryRequirements,					getImageMemoryRequirements),
	VK_NULL_FUNC_ENTRY(vkGetImageSparseMemoryRequirements,				getImageSparseMemoryRequirements),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceSparseImageFormatProperties,	getPhysicalDeviceSparseImageFormatProperties),
	VK_NULL_FUNC_ENTRY(vkQueueBindSparse,								queueBindSparse),
	VK_NULL_FUNC_ENTRY(vkCreateFence,									createFence),
	VK_NULL_FUNC_ENTRY(vkDestroyFence,									destroyFence),
	VK_NULL_FUNC_ENTRY(vkResetFences,									resetFences),
	VK_NULL_FUNC_ENTRY(vkGetFenceStatus,								getFenceStatus),
	VK_NULL_FUNC_ENTRY(vkWaitForFences,									waitForFences),
	VK_NULL_FUNC_ENTRY(vkCreateSemaphore,								createSemaphore),
	VK_NULL_FUNC_ENTRY(vkDestroySemaphore,								destroySemaphore),
	VK_NULL_FUNC_ENTRY(vkCreateEvent,									createEvent),
	VK_NULL_FUNC_ENTRY(vkDestroyEvent,									destroyEvent),
	VK_NULL_FUNC_ENTRY(vkGetEventStatus,								getEventStatus),
	VK_NULL_FUNC_ENTRY(vkSetEvent,										setEvent),
	VK_NULL_FUNC_ENTRY(vkResetEvent,									resetEvent),
	VK_NULL_FUNC_ENTRY(vkCreateQueryPool,								createQueryPool),
	VK_NULL_FUNC_ENTRY(vkDestroyQueryPool,								destroyQueryPool),
	VK_NULL_FUNC_ENTRY(vkGetQueryPoolResults,							getQueryPoolResults),
	VK_NULL_FUNC_ENTRY(vkCreateBuffer,									createBuffer),
	VK_NULL_FUNC_ENTRY(vkDestroyBuffer,									destroyBuffer),
	VK_NULL_FUNC_ENTRY(vkCreateBufferView,								createBufferView),
	VK_NULL_FUNC_ENTRY(vkDestroyBufferView,								destroyBufferView),
	VK_NULL_FUNC_ENTRY(vkCreateImage,									createImage),
	VK_NULL_FUNC_ENTRY(vkDestroyImage,									destroyImage),
	VK_NULL_FUNC_ENTRY(vkGetImageSubresourceLayout,						getImageSubresourceLayout),
	VK_NULL_FUNC_ENTRY(vkCreateImageView,								createImageView),
	VK_NULL_FUNC_ENTRY(vkDestroyImageView,								destroyImageView),
	VK_NULL_FUNC_ENTRY(vkCreateShaderModule,							createShaderModule),
	VK_NULL_FUNC_ENTRY(vkDestroyShaderModule,							destroyShaderModule),
	VK_NULL_FUNC_ENTRY(vkCreatePipelineCache,							createPipelineCache),
	VK_NULL_FUNC_ENTRY(vkDestroyPipelineCache,							destroyPipelineCache),
	VK_NULL_FUNC_ENTRY(vkGetPipelineCacheData,							getPipelineCacheData),
	VK_NULL_FUNC_ENTRY(vkMergePipelineCaches,							mergePipelineCaches),
	VK_NULL_FUNC_ENTRY(vkCreateGraphicsPipelines,						createGraphicsPipelines),
	VK_NULL_FUNC_ENTRY(vkCreateComputePipelines,						createComputePipelines),
	VK_NULL_FUNC_ENTRY(vkDestroyPipeline,								destroyPipeline),
	VK_NULL_FUNC_ENTRY(vkCreatePipelineLayout,							createPipelineLayout),
	VK_NULL_FUNC_ENTRY(vkDestroyPipelineLayout,							destroyPipelineLayout),
	VK_NULL_FUNC_ENTRY(vkCreateSampler,									createSampler),
	VK_NULL_FUNC_ENTRY(vkDestroySampler,								destroySampler),
	VK_NULL_FUNC_ENTRY(vkCreateDescriptorSetLayout,						createDescriptorSetLayout),
	VK_NULL_FUNC_ENTRY(vkDestroyDescriptorSetLayout,					destroyDescriptorSetLayout),
	VK_NULL_FUNC_ENTRY(vkCreateDescriptorPool,							createDescriptorPool),
	VK_NULL_FUNC_ENTRY(vkDestroyDescriptorPool,							destroyDescriptorPool),
	VK_NULL_FUNC_ENTRY(vkResetDescriptorPool,							resetDescriptorPool),
	VK_NULL_FUNC_ENTRY(vkAllocateDescriptorSets,						allocateDescriptorSets),
	VK_NULL_FUNC_ENTRY(vkFreeDescriptorSets,							freeDescriptorSets),
	VK_NULL_FUNC_ENTRY(vkUpdateDescriptorSets,							updateDescriptorSets),
	VK_NULL_FUNC_ENTRY(vkCreateFramebuffer,								createFramebuffer),
	VK_NULL_FUNC_ENTRY(vkDestroyFramebuffer,							destroyFramebuffer),
	VK_NULL_FUNC_ENTRY(vkCreateRenderPass,								createRenderPass),
	VK_NULL_FUNC_ENTRY(vkDestroyRenderPass,								destroyRenderPass),
	VK_NULL_FUNC_ENTRY(vkGetRenderAreaGranularity,						getRenderAreaGranularity),
	VK_NULL_FUNC_ENTRY(vkCreateCommandPool,								createCommandPool),
	VK_NULL_FUNC_ENTRY(vkDestroyCommandPool,							destroyCommandPool),
	VK_NULL_FUNC_ENTRY(vkResetCommandPool,								resetCommandPool),
	VK_NULL_FUNC_ENTRY(vkAllocateCommandBuffers,						allocateCommandBuffers),
	VK_NULL_FUNC_ENTRY(vkFreeCommandBuffers,							freeCommandBuffers),
	VK_NULL_FUNC_ENTRY(vkBeginCommandBuffer,							beginCommandBuffer),
	VK_NULL_FUNC_ENTRY(vkEndCommandBuffer,								endCommandBuffer),
	VK_NULL_FUNC_ENTRY(vkResetCommandBuffer,							resetCommandBuffer),
	VK_NULL_FUNC_ENTRY(vkCmdBindPipeline,								cmdBindPipeline),
	VK_NULL_FUNC_ENTRY(vkCmdSetViewport,								cmdSetViewport),
	VK_NULL_FUNC_ENTRY(vkCmdSetScissor,									cmdSetScissor),
	VK_NULL_FUNC_ENTRY(vkCmdSetLineWidth,								cmdSetLineWidth),
	VK_NULL_FUNC_ENTRY(vkCmdSetDepthBias,								cmdSetDepthBias),
	VK_NULL_FUNC_ENTRY(vkCmdSetBlendConstants,							cmdSetBlendConstants),
	VK_NULL_FUNC_ENTRY(vkCmdSetDepthBounds,								cmdSetDepthBounds),
	VK_NULL_FUNC_ENTRY(vkCmdSetStencilCompareMask,						cmdSetStencilCompareMask),
	VK_NULL_FUNC_ENTRY(vkCmdSetStencilWriteMask,						cmdSetStencilWriteMask),
	VK_NULL_FUNC_ENTRY(vkCmdSetStencilReference,						cmdSetStencilReference),
	VK_NULL_FUNC_ENTRY(vkCmdBindDescriptorSets,							cmdBindDescriptorSets),
	VK_NULL_FUNC_ENTRY(vkCmdBindIndexBuffer,							cmdBindIndexBuffer),
	VK_NULL_FUNC_ENTRY(vkCmdBindVertexBuffers,							cmdBindVertexBuffers),
	VK_NULL_FUNC_ENTRY(vkCmdDraw,										cmdDraw),
	VK_NULL_FUNC_ENTRY(vkCmdDrawIndexed,								cmdDrawIndexed),
	VK_NULL_FUNC_ENTRY(vkCmdDrawIndirect,								cmdDrawIndirect),
	VK_NULL_FUNC_ENTRY(vkCmdDrawIndexedIndirect,						cmdDrawIndexedIndirect),
	VK_NULL_FUNC_ENTRY(vkCmdDispatch,									cmdDispatch),
	VK_NULL_FUNC_ENTRY(vkCmdDispatchIndirect,							cmdDispatchIndirect),
	VK_NULL_FUNC_ENTRY(vkCmdCopyBuffer,									cmdCopyBuffer),
	VK_NULL_FUNC_ENTRY(vkCmdCopyImage,									cmdCopyImage),
	VK_NULL_FUNC_ENTRY(vkCmdBlitImage,									cmdBlitImage),
	VK_NULL_FUNC_ENTRY(vkCmdCopyBufferToImage,							cmdCopyBufferToImage),
	VK_NULL_FUNC_ENTRY(vkCmdCopyImageToBuffer,							cmdCopyImageToBuffer),
	VK_NULL_FUNC_ENTRY(vkCmdUpdateBuffer,								cmdUpdateBuffer),
	VK_NULL_FUNC_ENTRY(vkCmdFillBuffer,									cmdFillBuffer),
	VK_NULL_FUNC_ENTRY(vkCmdClearColorImage,							cmdClearColorImage),
	VK_NULL_FUNC_ENTRY(vkCmdClearDepthStencilImage,						cmdClearDepthStencilImage),
	VK_NULL_FUNC_ENTRY(vkCmdClearAttachments,							cmdClearAttachments),
	VK_NULL_FUNC_ENTRY(vkCmdResolveImage,								cmdResolveImage),
	VK_NULL_FUNC_ENTRY(vkCmdSetEvent,									cmdSetEvent),
	VK_NULL_FUNC_ENTRY(vkCmdResetEvent,									cmdResetEvent),
	VK_NULL_FUNC_ENTRY(vkCmdWaitEvents,									cmdWaitEvents),
	VK_NULL_FUNC_ENTRY(vkCmdPipelineBarrier,							cmdPipelineBarrier),
	VK_NULL_FUNC_ENTRY(vkCmdBeginQuery,									cmdBeginQuery),
	VK_NULL_FUNC_ENTRY(vkCmdEndQuery,									cmdEndQuery),
	VK_NULL_FUNC_ENTRY(vkCmdResetQueryPool,								cmdResetQueryPool),
	VK_NULL_FUNC_ENTRY(vkCmdWriteTimestamp,								cmdWriteTimestamp),
	VK_NULL_FUNC_ENTRY(vkCmdCopyQueryPoolResults,						cmdCopyQueryPoolResults),
	VK_NULL_FUNC_ENTRY(vkCmdPushConstants,								cmdPushConstants),
	VK_NULL_FUNC_ENTRY(vkCmdBeginRenderPass,							cmdBeginRenderPass),
	VK_NULL_FUNC_ENTRY(vkCmdNextSubpass,								cmdNextSubpass),
	VK_NULL_FUNC_ENTRY(vkCmdEndRenderPass,								cmdEndRenderPass),
	VK_NULL_FUNC_ENTRY(vkCmdExecuteCommands,							cmdExecuteCommands),
};

