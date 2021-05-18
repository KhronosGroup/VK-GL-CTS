/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 */
VKAPI_ATTR VkResult VKAPI_CALL createInstance (const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pInstance = allocateHandle<Instance, VkInstance>(pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createDevice (VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pDevice = allocateHandle<Device, VkDevice>(physicalDevice, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createFence (VkDevice device, const VkFenceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFence* pFence)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pFence = allocateNonDispHandle<Fence, VkFence>(device, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createSemaphore (VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSemaphore* pSemaphore)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pSemaphore = allocateNonDispHandle<Semaphore, VkSemaphore>(device, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createEvent (VkDevice device, const VkEventCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkEvent* pEvent)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pEvent = allocateNonDispHandle<Event, VkEvent>(device, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createQueryPool (VkDevice device, const VkQueryPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkQueryPool* pQueryPool)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pQueryPool = allocateNonDispHandle<QueryPool, VkQueryPool>(device, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createBuffer (VkDevice device, const VkBufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBuffer* pBuffer)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pBuffer = allocateNonDispHandle<Buffer, VkBuffer>(device, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createBufferView (VkDevice device, const VkBufferViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBufferView* pView)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pView = allocateNonDispHandle<BufferView, VkBufferView>(device, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createImage (VkDevice device, const VkImageCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImage* pImage)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pImage = allocateNonDispHandle<Image, VkImage>(device, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createImageView (VkDevice device, const VkImageViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImageView* pView)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pView = allocateNonDispHandle<ImageView, VkImageView>(device, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createPipelineCache (VkDevice device, const VkPipelineCacheCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineCache* pPipelineCache)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pPipelineCache = allocateNonDispHandle<PipelineCache, VkPipelineCache>(device, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createPipelineLayout (VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineLayout* pPipelineLayout)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pPipelineLayout = allocateNonDispHandle<PipelineLayout, VkPipelineLayout>(device, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createSampler (VkDevice device, const VkSamplerCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSampler* pSampler)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pSampler = allocateNonDispHandle<Sampler, VkSampler>(device, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createDescriptorSetLayout (VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorSetLayout* pSetLayout)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pSetLayout = allocateNonDispHandle<DescriptorSetLayout, VkDescriptorSetLayout>(device, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createDescriptorPool (VkDevice device, const VkDescriptorPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorPool* pDescriptorPool)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pDescriptorPool = allocateNonDispHandle<DescriptorPool, VkDescriptorPool>(device, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createFramebuffer (VkDevice device, const VkFramebufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFramebuffer* pFramebuffer)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pFramebuffer = allocateNonDispHandle<Framebuffer, VkFramebuffer>(device, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createRenderPass (VkDevice device, const VkRenderPassCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pRenderPass = allocateNonDispHandle<RenderPass, VkRenderPass>(device, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createCommandPool (VkDevice device, const VkCommandPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkCommandPool* pCommandPool)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pCommandPool = allocateNonDispHandle<CommandPool, VkCommandPool>(device, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createSamplerYcbcrConversion (VkDevice device, const VkSamplerYcbcrConversionCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSamplerYcbcrConversion* pYcbcrConversion)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pYcbcrConversion = allocateNonDispHandle<SamplerYcbcrConversion, VkSamplerYcbcrConversion>(device, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createRenderPass2 (VkDevice device, const VkRenderPassCreateInfo2* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pRenderPass = allocateNonDispHandle<RenderPass, VkRenderPass>(device, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createSwapchainKHR (VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pSwapchain = allocateNonDispHandle<SwapchainKHR, VkSwapchainKHR>(device, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createDisplayPlaneSurfaceKHR (VkInstance instance, const VkDisplaySurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pSurface = allocateNonDispHandle<SurfaceKHR, VkSurfaceKHR>(instance, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createDebugUtilsMessengerEXT (VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pMessenger)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pMessenger = allocateNonDispHandle<DebugUtilsMessengerEXT, VkDebugUtilsMessengerEXT>(instance, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createHeadlessSurfaceEXT (VkInstance instance, const VkHeadlessSurfaceCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pSurface = allocateNonDispHandle<SurfaceKHR, VkSurfaceKHR>(instance, pCreateInfo, pAllocator)));
}

VKAPI_ATTR void VKAPI_CALL destroyInstance (VkInstance instance, const VkAllocationCallbacks* pAllocator)
{
	freeHandle<Instance, VkInstance>(instance, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL destroyDevice (VkDevice device, const VkAllocationCallbacks* pAllocator)
{
	freeHandle<Device, VkDevice>(device, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL destroyFence (VkDevice device, VkFence fence, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	freeNonDispHandle<Fence, VkFence>(fence, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL destroySemaphore (VkDevice device, VkSemaphore semaphore, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	freeNonDispHandle<Semaphore, VkSemaphore>(semaphore, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL destroyEvent (VkDevice device, VkEvent event, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	freeNonDispHandle<Event, VkEvent>(event, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL destroyBuffer (VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	freeNonDispHandle<Buffer, VkBuffer>(buffer, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL destroyBufferView (VkDevice device, VkBufferView bufferView, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	freeNonDispHandle<BufferView, VkBufferView>(bufferView, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL destroyImage (VkDevice device, VkImage image, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	freeNonDispHandle<Image, VkImage>(image, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL destroyImageView (VkDevice device, VkImageView imageView, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	freeNonDispHandle<ImageView, VkImageView>(imageView, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL destroyPipelineCache (VkDevice device, VkPipelineCache pipelineCache, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	freeNonDispHandle<PipelineCache, VkPipelineCache>(pipelineCache, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL destroyPipeline (VkDevice device, VkPipeline pipeline, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	freeNonDispHandle<Pipeline, VkPipeline>(pipeline, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL destroyPipelineLayout (VkDevice device, VkPipelineLayout pipelineLayout, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	freeNonDispHandle<PipelineLayout, VkPipelineLayout>(pipelineLayout, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL destroySampler (VkDevice device, VkSampler sampler, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	freeNonDispHandle<Sampler, VkSampler>(sampler, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL destroyDescriptorSetLayout (VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	freeNonDispHandle<DescriptorSetLayout, VkDescriptorSetLayout>(descriptorSetLayout, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL destroyFramebuffer (VkDevice device, VkFramebuffer framebuffer, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	freeNonDispHandle<Framebuffer, VkFramebuffer>(framebuffer, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL destroyRenderPass (VkDevice device, VkRenderPass renderPass, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	freeNonDispHandle<RenderPass, VkRenderPass>(renderPass, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL destroySamplerYcbcrConversion (VkDevice device, VkSamplerYcbcrConversion ycbcrConversion, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	freeNonDispHandle<SamplerYcbcrConversion, VkSamplerYcbcrConversion>(ycbcrConversion, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL destroySurfaceKHR (VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(instance);
	freeNonDispHandle<SurfaceKHR, VkSurfaceKHR>(surface, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL destroyDebugUtilsMessengerEXT (VkInstance instance, VkDebugUtilsMessengerEXT messenger, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(instance);
	freeNonDispHandle<DebugUtilsMessengerEXT, VkDebugUtilsMessengerEXT>(messenger, pAllocator);
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

VKAPI_ATTR VkResult VKAPI_CALL getQueryPoolResults (VkDevice device, VkQueryPool queryPool, deUint32 firstQuery, deUint32 queryCount, deUintptr dataSize, void* pData, VkDeviceSize stride, VkQueryResultFlags flags)
{
	DE_UNREF(device);
	DE_UNREF(queryPool);
	DE_UNREF(firstQuery);
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

VKAPI_ATTR void VKAPI_CALL cmdSetViewport (VkCommandBuffer commandBuffer, deUint32 firstViewport, deUint32 viewportCount, const VkViewport* pViewports)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(firstViewport);
	DE_UNREF(viewportCount);
	DE_UNREF(pViewports);
}

VKAPI_ATTR void VKAPI_CALL cmdSetScissor (VkCommandBuffer commandBuffer, deUint32 firstScissor, deUint32 scissorCount, const VkRect2D* pScissors)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(firstScissor);
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

VKAPI_ATTR void VKAPI_CALL cmdBindVertexBuffers (VkCommandBuffer commandBuffer, deUint32 firstBinding, deUint32 bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(firstBinding);
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

VKAPI_ATTR void VKAPI_CALL cmdDispatch (VkCommandBuffer commandBuffer, deUint32 groupCountX, deUint32 groupCountY, deUint32 groupCountZ)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(groupCountX);
	DE_UNREF(groupCountY);
	DE_UNREF(groupCountZ);
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

VKAPI_ATTR void VKAPI_CALL cmdUpdateBuffer (VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize dataSize, const void* pData)
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

VKAPI_ATTR void VKAPI_CALL cmdWaitEvents (VkCommandBuffer commandBuffer, deUint32 eventCount, const VkEvent* pEvents, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, deUint32 memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, deUint32 bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, deUint32 imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(eventCount);
	DE_UNREF(pEvents);
	DE_UNREF(srcStageMask);
	DE_UNREF(dstStageMask);
	DE_UNREF(memoryBarrierCount);
	DE_UNREF(pMemoryBarriers);
	DE_UNREF(bufferMemoryBarrierCount);
	DE_UNREF(pBufferMemoryBarriers);
	DE_UNREF(imageMemoryBarrierCount);
	DE_UNREF(pImageMemoryBarriers);
}

VKAPI_ATTR void VKAPI_CALL cmdPipelineBarrier (VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags, deUint32 memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, deUint32 bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, deUint32 imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(srcStageMask);
	DE_UNREF(dstStageMask);
	DE_UNREF(dependencyFlags);
	DE_UNREF(memoryBarrierCount);
	DE_UNREF(pMemoryBarriers);
	DE_UNREF(bufferMemoryBarrierCount);
	DE_UNREF(pBufferMemoryBarriers);
	DE_UNREF(imageMemoryBarrierCount);
	DE_UNREF(pImageMemoryBarriers);
}

VKAPI_ATTR void VKAPI_CALL cmdBeginQuery (VkCommandBuffer commandBuffer, VkQueryPool queryPool, deUint32 query, VkQueryControlFlags flags)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(queryPool);
	DE_UNREF(query);
	DE_UNREF(flags);
}

VKAPI_ATTR void VKAPI_CALL cmdEndQuery (VkCommandBuffer commandBuffer, VkQueryPool queryPool, deUint32 query)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(queryPool);
	DE_UNREF(query);
}

VKAPI_ATTR void VKAPI_CALL cmdResetQueryPool (VkCommandBuffer commandBuffer, VkQueryPool queryPool, deUint32 firstQuery, deUint32 queryCount)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(queryPool);
	DE_UNREF(firstQuery);
	DE_UNREF(queryCount);
}

VKAPI_ATTR void VKAPI_CALL cmdWriteTimestamp (VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage, VkQueryPool queryPool, deUint32 query)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pipelineStage);
	DE_UNREF(queryPool);
	DE_UNREF(query);
}

VKAPI_ATTR void VKAPI_CALL cmdCopyQueryPoolResults (VkCommandBuffer commandBuffer, VkQueryPool queryPool, deUint32 firstQuery, deUint32 queryCount, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize stride, VkQueryResultFlags flags)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(queryPool);
	DE_UNREF(firstQuery);
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

VKAPI_ATTR void VKAPI_CALL cmdExecuteCommands (VkCommandBuffer commandBuffer, deUint32 commandBufferCount, const VkCommandBuffer* pCommandBuffers)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(commandBufferCount);
	DE_UNREF(pCommandBuffers);
}

VKAPI_ATTR VkResult VKAPI_CALL enumerateInstanceVersion (deUint32* pApiVersion)
{
	DE_UNREF(pApiVersion);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL bindBufferMemory2 (VkDevice device, deUint32 bindInfoCount, const VkBindBufferMemoryInfo* pBindInfos)
{
	DE_UNREF(device);
	DE_UNREF(bindInfoCount);
	DE_UNREF(pBindInfos);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL bindImageMemory2 (VkDevice device, deUint32 bindInfoCount, const VkBindImageMemoryInfo* pBindInfos)
{
	DE_UNREF(device);
	DE_UNREF(bindInfoCount);
	DE_UNREF(pBindInfos);
	return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL getDeviceGroupPeerMemoryFeatures (VkDevice device, deUint32 heapIndex, deUint32 localDeviceIndex, deUint32 remoteDeviceIndex, VkPeerMemoryFeatureFlags* pPeerMemoryFeatures)
{
	DE_UNREF(device);
	DE_UNREF(heapIndex);
	DE_UNREF(localDeviceIndex);
	DE_UNREF(remoteDeviceIndex);
	DE_UNREF(pPeerMemoryFeatures);
}

VKAPI_ATTR void VKAPI_CALL cmdSetDeviceMask (VkCommandBuffer commandBuffer, deUint32 deviceMask)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(deviceMask);
}

VKAPI_ATTR void VKAPI_CALL cmdDispatchBase (VkCommandBuffer commandBuffer, deUint32 baseGroupX, deUint32 baseGroupY, deUint32 baseGroupZ, deUint32 groupCountX, deUint32 groupCountY, deUint32 groupCountZ)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(baseGroupX);
	DE_UNREF(baseGroupY);
	DE_UNREF(baseGroupZ);
	DE_UNREF(groupCountX);
	DE_UNREF(groupCountY);
	DE_UNREF(groupCountZ);
}

VKAPI_ATTR VkResult VKAPI_CALL enumeratePhysicalDeviceGroups (VkInstance instance, deUint32* pPhysicalDeviceGroupCount, VkPhysicalDeviceGroupProperties* pPhysicalDeviceGroupProperties)
{
	DE_UNREF(instance);
	DE_UNREF(pPhysicalDeviceGroupCount);
	DE_UNREF(pPhysicalDeviceGroupProperties);
	return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL getImageMemoryRequirements2 (VkDevice device, const VkImageMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements)
{
	DE_UNREF(device);
	DE_UNREF(pInfo);
	DE_UNREF(pMemoryRequirements);
}

VKAPI_ATTR void VKAPI_CALL getBufferMemoryRequirements2 (VkDevice device, const VkBufferMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements)
{
	DE_UNREF(device);
	DE_UNREF(pInfo);
	DE_UNREF(pMemoryRequirements);
}

VKAPI_ATTR void VKAPI_CALL getPhysicalDeviceFeatures2 (VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures2* pFeatures)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(pFeatures);
}

VKAPI_ATTR void VKAPI_CALL getPhysicalDeviceProperties2 (VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties2* pProperties)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(pProperties);
}

VKAPI_ATTR void VKAPI_CALL getPhysicalDeviceFormatProperties2 (VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties2* pFormatProperties)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(format);
	DE_UNREF(pFormatProperties);
}

VKAPI_ATTR VkResult VKAPI_CALL getPhysicalDeviceImageFormatProperties2 (VkPhysicalDevice physicalDevice, const VkPhysicalDeviceImageFormatInfo2* pImageFormatInfo, VkImageFormatProperties2* pImageFormatProperties)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(pImageFormatInfo);
	DE_UNREF(pImageFormatProperties);
	return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL getPhysicalDeviceQueueFamilyProperties2 (VkPhysicalDevice physicalDevice, deUint32* pQueueFamilyPropertyCount, VkQueueFamilyProperties2* pQueueFamilyProperties)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(pQueueFamilyPropertyCount);
	DE_UNREF(pQueueFamilyProperties);
}

VKAPI_ATTR void VKAPI_CALL getPhysicalDeviceMemoryProperties2 (VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties2* pMemoryProperties)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(pMemoryProperties);
}

VKAPI_ATTR void VKAPI_CALL getDeviceQueue2 (VkDevice device, const VkDeviceQueueInfo2* pQueueInfo, VkQueue* pQueue)
{
	DE_UNREF(device);
	DE_UNREF(pQueueInfo);
	DE_UNREF(pQueue);
}

VKAPI_ATTR void VKAPI_CALL getPhysicalDeviceExternalBufferProperties (VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalBufferInfo* pExternalBufferInfo, VkExternalBufferProperties* pExternalBufferProperties)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(pExternalBufferInfo);
	DE_UNREF(pExternalBufferProperties);
}

VKAPI_ATTR void VKAPI_CALL getPhysicalDeviceExternalFenceProperties (VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalFenceInfo* pExternalFenceInfo, VkExternalFenceProperties* pExternalFenceProperties)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(pExternalFenceInfo);
	DE_UNREF(pExternalFenceProperties);
}

VKAPI_ATTR void VKAPI_CALL getPhysicalDeviceExternalSemaphoreProperties (VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo* pExternalSemaphoreInfo, VkExternalSemaphoreProperties* pExternalSemaphoreProperties)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(pExternalSemaphoreInfo);
	DE_UNREF(pExternalSemaphoreProperties);
}

VKAPI_ATTR void VKAPI_CALL getDescriptorSetLayoutSupport (VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, VkDescriptorSetLayoutSupport* pSupport)
{
	DE_UNREF(device);
	DE_UNREF(pCreateInfo);
	DE_UNREF(pSupport);
}

VKAPI_ATTR void VKAPI_CALL cmdDrawIndirectCount (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, deUint32 maxDrawCount, deUint32 stride)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(buffer);
	DE_UNREF(offset);
	DE_UNREF(countBuffer);
	DE_UNREF(countBufferOffset);
	DE_UNREF(maxDrawCount);
	DE_UNREF(stride);
}

VKAPI_ATTR void VKAPI_CALL cmdDrawIndexedIndirectCount (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, deUint32 maxDrawCount, deUint32 stride)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(buffer);
	DE_UNREF(offset);
	DE_UNREF(countBuffer);
	DE_UNREF(countBufferOffset);
	DE_UNREF(maxDrawCount);
	DE_UNREF(stride);
}

VKAPI_ATTR void VKAPI_CALL cmdBeginRenderPass2 (VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, const VkSubpassBeginInfo* pSubpassBeginInfo)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pRenderPassBegin);
	DE_UNREF(pSubpassBeginInfo);
}

VKAPI_ATTR void VKAPI_CALL cmdNextSubpass2 (VkCommandBuffer commandBuffer, const VkSubpassBeginInfo* pSubpassBeginInfo, const VkSubpassEndInfo* pSubpassEndInfo)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pSubpassBeginInfo);
	DE_UNREF(pSubpassEndInfo);
}

VKAPI_ATTR void VKAPI_CALL cmdEndRenderPass2 (VkCommandBuffer commandBuffer, const VkSubpassEndInfo* pSubpassEndInfo)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pSubpassEndInfo);
}

VKAPI_ATTR void VKAPI_CALL resetQueryPool (VkDevice device, VkQueryPool queryPool, deUint32 firstQuery, deUint32 queryCount)
{
	DE_UNREF(device);
	DE_UNREF(queryPool);
	DE_UNREF(firstQuery);
	DE_UNREF(queryCount);
}

VKAPI_ATTR VkResult VKAPI_CALL getSemaphoreCounterValue (VkDevice device, VkSemaphore semaphore, deUint64* pValue)
{
	DE_UNREF(device);
	DE_UNREF(semaphore);
	DE_UNREF(pValue);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL waitSemaphores (VkDevice device, const VkSemaphoreWaitInfo* pWaitInfo, deUint64 timeout)
{
	DE_UNREF(device);
	DE_UNREF(pWaitInfo);
	DE_UNREF(timeout);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL signalSemaphore (VkDevice device, const VkSemaphoreSignalInfo* pSignalInfo)
{
	DE_UNREF(device);
	DE_UNREF(pSignalInfo);
	return VK_SUCCESS;
}

VKAPI_ATTR VkDeviceAddress VKAPI_CALL getBufferDeviceAddress (VkDevice device, const VkBufferDeviceAddressInfo* pInfo)
{
	DE_UNREF(device);
	DE_UNREF(pInfo);
	return VK_SUCCESS;
}

VKAPI_ATTR uint64_t VKAPI_CALL getBufferOpaqueCaptureAddress (VkDevice device, const VkBufferDeviceAddressInfo* pInfo)
{
	DE_UNREF(device);
	DE_UNREF(pInfo);
	return VK_SUCCESS;
}

VKAPI_ATTR uint64_t VKAPI_CALL getDeviceMemoryOpaqueCaptureAddress (VkDevice device, const VkDeviceMemoryOpaqueCaptureAddressInfo* pInfo)
{
	DE_UNREF(device);
	DE_UNREF(pInfo);
	return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL getCommandPoolMemoryConsumption (VkDevice device, VkCommandPool commandPool, VkCommandBuffer commandBuffer, VkCommandPoolMemoryConsumption* pConsumption)
{
	DE_UNREF(device);
	DE_UNREF(commandPool);
	DE_UNREF(commandBuffer);
	DE_UNREF(pConsumption);
}

VKAPI_ATTR VkResult VKAPI_CALL getFaultData (VkDevice device, VkFaultQueryBehavior faultQueryBehavior, VkBool32* pUnrecordedFaults, deUint32* pFaultCount, VkFaultData* pFaults)
{
	DE_UNREF(device);
	DE_UNREF(faultQueryBehavior);
	DE_UNREF(pUnrecordedFaults);
	DE_UNREF(pFaultCount);
	DE_UNREF(pFaults);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getPhysicalDeviceSurfaceSupportKHR (VkPhysicalDevice physicalDevice, deUint32 queueFamilyIndex, VkSurfaceKHR surface, VkBool32* pSupported)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(queueFamilyIndex);
	DE_UNREF(surface);
	DE_UNREF(pSupported);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getPhysicalDeviceSurfaceCapabilitiesKHR (VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR* pSurfaceCapabilities)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(surface);
	DE_UNREF(pSurfaceCapabilities);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getPhysicalDeviceSurfaceFormatsKHR (VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, deUint32* pSurfaceFormatCount, VkSurfaceFormatKHR* pSurfaceFormats)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(surface);
	DE_UNREF(pSurfaceFormatCount);
	DE_UNREF(pSurfaceFormats);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getPhysicalDeviceSurfacePresentModesKHR (VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, deUint32* pPresentModeCount, VkPresentModeKHR* pPresentModes)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(surface);
	DE_UNREF(pPresentModeCount);
	DE_UNREF(pPresentModes);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getSwapchainImagesKHR (VkDevice device, VkSwapchainKHR swapchain, deUint32* pSwapchainImageCount, VkImage* pSwapchainImages)
{
	DE_UNREF(device);
	DE_UNREF(swapchain);
	DE_UNREF(pSwapchainImageCount);
	DE_UNREF(pSwapchainImages);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL acquireNextImageKHR (VkDevice device, VkSwapchainKHR swapchain, deUint64 timeout, VkSemaphore semaphore, VkFence fence, deUint32* pImageIndex)
{
	DE_UNREF(device);
	DE_UNREF(swapchain);
	DE_UNREF(timeout);
	DE_UNREF(semaphore);
	DE_UNREF(fence);
	DE_UNREF(pImageIndex);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL queuePresentKHR (VkQueue queue, const VkPresentInfoKHR* pPresentInfo)
{
	DE_UNREF(queue);
	DE_UNREF(pPresentInfo);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getDeviceGroupPresentCapabilitiesKHR (VkDevice device, VkDeviceGroupPresentCapabilitiesKHR* pDeviceGroupPresentCapabilities)
{
	DE_UNREF(device);
	DE_UNREF(pDeviceGroupPresentCapabilities);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getDeviceGroupSurfacePresentModesKHR (VkDevice device, VkSurfaceKHR surface, VkDeviceGroupPresentModeFlagsKHR* pModes)
{
	DE_UNREF(device);
	DE_UNREF(surface);
	DE_UNREF(pModes);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getPhysicalDevicePresentRectanglesKHR (VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, deUint32* pRectCount, VkRect2D* pRects)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(surface);
	DE_UNREF(pRectCount);
	DE_UNREF(pRects);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL acquireNextImage2KHR (VkDevice device, const VkAcquireNextImageInfoKHR* pAcquireInfo, deUint32* pImageIndex)
{
	DE_UNREF(device);
	DE_UNREF(pAcquireInfo);
	DE_UNREF(pImageIndex);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getPhysicalDeviceDisplayPropertiesKHR (VkPhysicalDevice physicalDevice, deUint32* pPropertyCount, VkDisplayPropertiesKHR* pProperties)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(pPropertyCount);
	DE_UNREF(pProperties);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getPhysicalDeviceDisplayPlanePropertiesKHR (VkPhysicalDevice physicalDevice, deUint32* pPropertyCount, VkDisplayPlanePropertiesKHR* pProperties)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(pPropertyCount);
	DE_UNREF(pProperties);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getDisplayPlaneSupportedDisplaysKHR (VkPhysicalDevice physicalDevice, deUint32 planeIndex, deUint32* pDisplayCount, VkDisplayKHR* pDisplays)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(planeIndex);
	DE_UNREF(pDisplayCount);
	DE_UNREF(pDisplays);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getDisplayModePropertiesKHR (VkPhysicalDevice physicalDevice, VkDisplayKHR display, deUint32* pPropertyCount, VkDisplayModePropertiesKHR* pProperties)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(display);
	DE_UNREF(pPropertyCount);
	DE_UNREF(pProperties);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getDisplayPlaneCapabilitiesKHR (VkPhysicalDevice physicalDevice, VkDisplayModeKHR mode, deUint32 planeIndex, VkDisplayPlaneCapabilitiesKHR* pCapabilities)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(mode);
	DE_UNREF(planeIndex);
	DE_UNREF(pCapabilities);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getMemoryFdKHR (VkDevice device, const VkMemoryGetFdInfoKHR* pGetFdInfo, int* pFd)
{
	DE_UNREF(device);
	DE_UNREF(pGetFdInfo);
	DE_UNREF(pFd);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getMemoryFdPropertiesKHR (VkDevice device, VkExternalMemoryHandleTypeFlagBits handleType, int fd, VkMemoryFdPropertiesKHR* pMemoryFdProperties)
{
	DE_UNREF(device);
	DE_UNREF(handleType);
	DE_UNREF(fd);
	DE_UNREF(pMemoryFdProperties);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL importSemaphoreFdKHR (VkDevice device, const VkImportSemaphoreFdInfoKHR* pImportSemaphoreFdInfo)
{
	DE_UNREF(device);
	DE_UNREF(pImportSemaphoreFdInfo);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getSemaphoreFdKHR (VkDevice device, const VkSemaphoreGetFdInfoKHR* pGetFdInfo, int* pFd)
{
	DE_UNREF(device);
	DE_UNREF(pGetFdInfo);
	DE_UNREF(pFd);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getSwapchainStatusKHR (VkDevice device, VkSwapchainKHR swapchain)
{
	DE_UNREF(device);
	DE_UNREF(swapchain);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL importFenceFdKHR (VkDevice device, const VkImportFenceFdInfoKHR* pImportFenceFdInfo)
{
	DE_UNREF(device);
	DE_UNREF(pImportFenceFdInfo);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getFenceFdKHR (VkDevice device, const VkFenceGetFdInfoKHR* pGetFdInfo, int* pFd)
{
	DE_UNREF(device);
	DE_UNREF(pGetFdInfo);
	DE_UNREF(pFd);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL enumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR (VkPhysicalDevice physicalDevice, deUint32 queueFamilyIndex, deUint32* pCounterCount, VkPerformanceCounterKHR* pCounters, VkPerformanceCounterDescriptionKHR* pCounterDescriptions)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(queueFamilyIndex);
	DE_UNREF(pCounterCount);
	DE_UNREF(pCounters);
	DE_UNREF(pCounterDescriptions);
	return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL getPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR (VkPhysicalDevice physicalDevice, const VkQueryPoolPerformanceCreateInfoKHR* pPerformanceQueryCreateInfo, deUint32* pNumPasses)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(pPerformanceQueryCreateInfo);
	DE_UNREF(pNumPasses);
}

VKAPI_ATTR VkResult VKAPI_CALL acquireProfilingLockKHR (VkDevice device, const VkAcquireProfilingLockInfoKHR* pInfo)
{
	DE_UNREF(device);
	DE_UNREF(pInfo);
	return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL releaseProfilingLockKHR (VkDevice device)
{
	DE_UNREF(device);
}

VKAPI_ATTR VkResult VKAPI_CALL getPhysicalDeviceSurfaceCapabilities2KHR (VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSurfaceInfo2KHR* pSurfaceInfo, VkSurfaceCapabilities2KHR* pSurfaceCapabilities)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(pSurfaceInfo);
	DE_UNREF(pSurfaceCapabilities);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getPhysicalDeviceSurfaceFormats2KHR (VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSurfaceInfo2KHR* pSurfaceInfo, deUint32* pSurfaceFormatCount, VkSurfaceFormat2KHR* pSurfaceFormats)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(pSurfaceInfo);
	DE_UNREF(pSurfaceFormatCount);
	DE_UNREF(pSurfaceFormats);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getPhysicalDeviceDisplayProperties2KHR (VkPhysicalDevice physicalDevice, deUint32* pPropertyCount, VkDisplayProperties2KHR* pProperties)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(pPropertyCount);
	DE_UNREF(pProperties);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getPhysicalDeviceDisplayPlaneProperties2KHR (VkPhysicalDevice physicalDevice, deUint32* pPropertyCount, VkDisplayPlaneProperties2KHR* pProperties)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(pPropertyCount);
	DE_UNREF(pProperties);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getDisplayModeProperties2KHR (VkPhysicalDevice physicalDevice, VkDisplayKHR display, deUint32* pPropertyCount, VkDisplayModeProperties2KHR* pProperties)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(display);
	DE_UNREF(pPropertyCount);
	DE_UNREF(pProperties);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getDisplayPlaneCapabilities2KHR (VkPhysicalDevice physicalDevice, const VkDisplayPlaneInfo2KHR* pDisplayPlaneInfo, VkDisplayPlaneCapabilities2KHR* pCapabilities)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(pDisplayPlaneInfo);
	DE_UNREF(pCapabilities);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getPhysicalDeviceFragmentShadingRatesKHR (VkPhysicalDevice physicalDevice, deUint32* pFragmentShadingRateCount, VkPhysicalDeviceFragmentShadingRateKHR* pFragmentShadingRates)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(pFragmentShadingRateCount);
	DE_UNREF(pFragmentShadingRates);
	return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL cmdSetFragmentShadingRateKHR (VkCommandBuffer commandBuffer, const VkExtent2D* pFragmentSize, const VkFragmentShadingRateCombinerOpKHR combinerOps[2])
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pFragmentSize);
	DE_UNREF(combinerOps);
}

VKAPI_ATTR void VKAPI_CALL cmdRefreshObjectsKHR (VkCommandBuffer commandBuffer, const VkRefreshObjectListKHR* pRefreshObjects)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pRefreshObjects);
}

VKAPI_ATTR VkResult VKAPI_CALL getPhysicalDeviceRefreshableObjectTypesKHR (VkPhysicalDevice physicalDevice, deUint32* pRefreshableObjectCount, VkObjectType* pRefreshableObjectTypes)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(pRefreshableObjectCount);
	DE_UNREF(pRefreshableObjectTypes);
	return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL cmdSetEvent2KHR (VkCommandBuffer commandBuffer, VkEvent event, const VkDependencyInfoKHR* pDependencyInfo)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(event);
	DE_UNREF(pDependencyInfo);
}

VKAPI_ATTR void VKAPI_CALL cmdResetEvent2KHR (VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags2KHR stageMask)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(event);
	DE_UNREF(stageMask);
}

VKAPI_ATTR void VKAPI_CALL cmdWaitEvents2KHR (VkCommandBuffer commandBuffer, deUint32 eventCount, const VkEvent* pEvents, const VkDependencyInfoKHR* pDependencyInfos)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(eventCount);
	DE_UNREF(pEvents);
	DE_UNREF(pDependencyInfos);
}

VKAPI_ATTR void VKAPI_CALL cmdPipelineBarrier2KHR (VkCommandBuffer commandBuffer, const VkDependencyInfoKHR* pDependencyInfo)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pDependencyInfo);
}

VKAPI_ATTR void VKAPI_CALL cmdWriteTimestamp2KHR (VkCommandBuffer commandBuffer, VkPipelineStageFlags2KHR stage, VkQueryPool queryPool, deUint32 query)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(stage);
	DE_UNREF(queryPool);
	DE_UNREF(query);
}

VKAPI_ATTR VkResult VKAPI_CALL queueSubmit2KHR (VkQueue queue, deUint32 submitCount, const VkSubmitInfo2KHR* pSubmits, VkFence fence)
{
	DE_UNREF(queue);
	DE_UNREF(submitCount);
	DE_UNREF(pSubmits);
	DE_UNREF(fence);
	return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL cmdWriteBufferMarker2AMD (VkCommandBuffer commandBuffer, VkPipelineStageFlags2KHR stage, VkBuffer dstBuffer, VkDeviceSize dstOffset, deUint32 marker)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(stage);
	DE_UNREF(dstBuffer);
	DE_UNREF(dstOffset);
	DE_UNREF(marker);
}

VKAPI_ATTR void VKAPI_CALL getQueueCheckpointData2NV (VkQueue queue, deUint32* pCheckpointDataCount, VkCheckpointData2NV* pCheckpointData)
{
	DE_UNREF(queue);
	DE_UNREF(pCheckpointDataCount);
	DE_UNREF(pCheckpointData);
}

VKAPI_ATTR void VKAPI_CALL cmdCopyBuffer2KHR (VkCommandBuffer commandBuffer, const VkCopyBufferInfo2KHR* pCopyBufferInfo)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pCopyBufferInfo);
}

VKAPI_ATTR void VKAPI_CALL cmdCopyImage2KHR (VkCommandBuffer commandBuffer, const VkCopyImageInfo2KHR* pCopyImageInfo)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pCopyImageInfo);
}

VKAPI_ATTR void VKAPI_CALL cmdCopyBufferToImage2KHR (VkCommandBuffer commandBuffer, const VkCopyBufferToImageInfo2KHR* pCopyBufferToImageInfo)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pCopyBufferToImageInfo);
}

VKAPI_ATTR void VKAPI_CALL cmdCopyImageToBuffer2KHR (VkCommandBuffer commandBuffer, const VkCopyImageToBufferInfo2KHR* pCopyImageToBufferInfo)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pCopyImageToBufferInfo);
}

VKAPI_ATTR void VKAPI_CALL cmdBlitImage2KHR (VkCommandBuffer commandBuffer, const VkBlitImageInfo2KHR* pBlitImageInfo)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pBlitImageInfo);
}

VKAPI_ATTR void VKAPI_CALL cmdResolveImage2KHR (VkCommandBuffer commandBuffer, const VkResolveImageInfo2KHR* pResolveImageInfo)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pResolveImageInfo);
}

VKAPI_ATTR VkResult VKAPI_CALL releaseDisplayEXT (VkPhysicalDevice physicalDevice, VkDisplayKHR display)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(display);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getPhysicalDeviceSurfaceCapabilities2EXT (VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkSurfaceCapabilities2EXT* pSurfaceCapabilities)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(surface);
	DE_UNREF(pSurfaceCapabilities);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL displayPowerControlEXT (VkDevice device, VkDisplayKHR display, const VkDisplayPowerInfoEXT* pDisplayPowerInfo)
{
	DE_UNREF(device);
	DE_UNREF(display);
	DE_UNREF(pDisplayPowerInfo);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL registerDeviceEventEXT (VkDevice device, const VkDeviceEventInfoEXT* pDeviceEventInfo, const VkAllocationCallbacks* pAllocator, VkFence* pFence)
{
	DE_UNREF(device);
	DE_UNREF(pDeviceEventInfo);
	DE_UNREF(pAllocator);
	DE_UNREF(pFence);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL registerDisplayEventEXT (VkDevice device, VkDisplayKHR display, const VkDisplayEventInfoEXT* pDisplayEventInfo, const VkAllocationCallbacks* pAllocator, VkFence* pFence)
{
	DE_UNREF(device);
	DE_UNREF(display);
	DE_UNREF(pDisplayEventInfo);
	DE_UNREF(pAllocator);
	DE_UNREF(pFence);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getSwapchainCounterEXT (VkDevice device, VkSwapchainKHR swapchain, VkSurfaceCounterFlagBitsEXT counter, deUint64* pCounterValue)
{
	DE_UNREF(device);
	DE_UNREF(swapchain);
	DE_UNREF(counter);
	DE_UNREF(pCounterValue);
	return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL cmdSetDiscardRectangleEXT (VkCommandBuffer commandBuffer, deUint32 firstDiscardRectangle, deUint32 discardRectangleCount, const VkRect2D* pDiscardRectangles)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(firstDiscardRectangle);
	DE_UNREF(discardRectangleCount);
	DE_UNREF(pDiscardRectangles);
}

VKAPI_ATTR void VKAPI_CALL setHdrMetadataEXT (VkDevice device, deUint32 swapchainCount, const VkSwapchainKHR* pSwapchains, const VkHdrMetadataEXT* pMetadata)
{
	DE_UNREF(device);
	DE_UNREF(swapchainCount);
	DE_UNREF(pSwapchains);
	DE_UNREF(pMetadata);
}

VKAPI_ATTR VkResult VKAPI_CALL setDebugUtilsObjectNameEXT (VkDevice device, const VkDebugUtilsObjectNameInfoEXT* pNameInfo)
{
	DE_UNREF(device);
	DE_UNREF(pNameInfo);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL setDebugUtilsObjectTagEXT (VkDevice device, const VkDebugUtilsObjectTagInfoEXT* pTagInfo)
{
	DE_UNREF(device);
	DE_UNREF(pTagInfo);
	return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL queueBeginDebugUtilsLabelEXT (VkQueue queue, const VkDebugUtilsLabelEXT* pLabelInfo)
{
	DE_UNREF(queue);
	DE_UNREF(pLabelInfo);
}

VKAPI_ATTR void VKAPI_CALL queueEndDebugUtilsLabelEXT (VkQueue queue)
{
	DE_UNREF(queue);
}

VKAPI_ATTR void VKAPI_CALL queueInsertDebugUtilsLabelEXT (VkQueue queue, const VkDebugUtilsLabelEXT* pLabelInfo)
{
	DE_UNREF(queue);
	DE_UNREF(pLabelInfo);
}

VKAPI_ATTR void VKAPI_CALL cmdBeginDebugUtilsLabelEXT (VkCommandBuffer commandBuffer, const VkDebugUtilsLabelEXT* pLabelInfo)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pLabelInfo);
}

VKAPI_ATTR void VKAPI_CALL cmdEndDebugUtilsLabelEXT (VkCommandBuffer commandBuffer)
{
	DE_UNREF(commandBuffer);
}

VKAPI_ATTR void VKAPI_CALL cmdInsertDebugUtilsLabelEXT (VkCommandBuffer commandBuffer, const VkDebugUtilsLabelEXT* pLabelInfo)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pLabelInfo);
}

VKAPI_ATTR void VKAPI_CALL submitDebugUtilsMessageEXT (VkInstance instance, VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData)
{
	DE_UNREF(instance);
	DE_UNREF(messageSeverity);
	DE_UNREF(messageTypes);
	DE_UNREF(pCallbackData);
}

VKAPI_ATTR void VKAPI_CALL cmdSetSampleLocationsEXT (VkCommandBuffer commandBuffer, const VkSampleLocationsInfoEXT* pSampleLocationsInfo)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pSampleLocationsInfo);
}

VKAPI_ATTR void VKAPI_CALL getPhysicalDeviceMultisamplePropertiesEXT (VkPhysicalDevice physicalDevice, VkSampleCountFlagBits samples, VkMultisamplePropertiesEXT* pMultisampleProperties)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(samples);
	DE_UNREF(pMultisampleProperties);
}

VKAPI_ATTR VkResult VKAPI_CALL getImageDrmFormatModifierPropertiesEXT (VkDevice device, VkImage image, VkImageDrmFormatModifierPropertiesEXT* pProperties)
{
	DE_UNREF(device);
	DE_UNREF(image);
	DE_UNREF(pProperties);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getMemoryHostPointerPropertiesEXT (VkDevice device, VkExternalMemoryHandleTypeFlagBits handleType, const void* pHostPointer, VkMemoryHostPointerPropertiesEXT* pMemoryHostPointerProperties)
{
	DE_UNREF(device);
	DE_UNREF(handleType);
	DE_UNREF(pHostPointer);
	DE_UNREF(pMemoryHostPointerProperties);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getPhysicalDeviceCalibrateableTimeDomainsEXT (VkPhysicalDevice physicalDevice, deUint32* pTimeDomainCount, VkTimeDomainEXT* pTimeDomains)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(pTimeDomainCount);
	DE_UNREF(pTimeDomains);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getCalibratedTimestampsEXT (VkDevice device, deUint32 timestampCount, const VkCalibratedTimestampInfoEXT* pTimestampInfos, deUint64* pTimestamps, deUint64* pMaxDeviation)
{
	DE_UNREF(device);
	DE_UNREF(timestampCount);
	DE_UNREF(pTimestampInfos);
	DE_UNREF(pTimestamps);
	DE_UNREF(pMaxDeviation);
	return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL cmdSetLineStippleEXT (VkCommandBuffer commandBuffer, deUint32 lineStippleFactor, deUint16 lineStipplePattern)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(lineStippleFactor);
	DE_UNREF(lineStipplePattern);
}

VKAPI_ATTR void VKAPI_CALL cmdSetCullModeEXT (VkCommandBuffer commandBuffer, VkCullModeFlags cullMode)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(cullMode);
}

VKAPI_ATTR void VKAPI_CALL cmdSetFrontFaceEXT (VkCommandBuffer commandBuffer, VkFrontFace frontFace)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(frontFace);
}

VKAPI_ATTR void VKAPI_CALL cmdSetPrimitiveTopologyEXT (VkCommandBuffer commandBuffer, VkPrimitiveTopology primitiveTopology)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(primitiveTopology);
}

VKAPI_ATTR void VKAPI_CALL cmdSetViewportWithCountEXT (VkCommandBuffer commandBuffer, deUint32 viewportCount, const VkViewport* pViewports)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(viewportCount);
	DE_UNREF(pViewports);
}

VKAPI_ATTR void VKAPI_CALL cmdSetScissorWithCountEXT (VkCommandBuffer commandBuffer, deUint32 scissorCount, const VkRect2D* pScissors)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(scissorCount);
	DE_UNREF(pScissors);
}

VKAPI_ATTR void VKAPI_CALL cmdBindVertexBuffers2EXT (VkCommandBuffer commandBuffer, deUint32 firstBinding, deUint32 bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes, const VkDeviceSize* pStrides)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(firstBinding);
	DE_UNREF(bindingCount);
	DE_UNREF(pBuffers);
	DE_UNREF(pOffsets);
	DE_UNREF(pSizes);
	DE_UNREF(pStrides);
}

VKAPI_ATTR void VKAPI_CALL cmdSetDepthTestEnableEXT (VkCommandBuffer commandBuffer, VkBool32 depthTestEnable)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(depthTestEnable);
}

VKAPI_ATTR void VKAPI_CALL cmdSetDepthWriteEnableEXT (VkCommandBuffer commandBuffer, VkBool32 depthWriteEnable)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(depthWriteEnable);
}

VKAPI_ATTR void VKAPI_CALL cmdSetDepthCompareOpEXT (VkCommandBuffer commandBuffer, VkCompareOp depthCompareOp)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(depthCompareOp);
}

VKAPI_ATTR void VKAPI_CALL cmdSetDepthBoundsTestEnableEXT (VkCommandBuffer commandBuffer, VkBool32 depthBoundsTestEnable)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(depthBoundsTestEnable);
}

VKAPI_ATTR void VKAPI_CALL cmdSetStencilTestEnableEXT (VkCommandBuffer commandBuffer, VkBool32 stencilTestEnable)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(stencilTestEnable);
}

VKAPI_ATTR void VKAPI_CALL cmdSetStencilOpEXT (VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(faceMask);
	DE_UNREF(failOp);
	DE_UNREF(passOp);
	DE_UNREF(depthFailOp);
	DE_UNREF(compareOp);
}

VKAPI_ATTR void VKAPI_CALL cmdSetVertexInputEXT (VkCommandBuffer commandBuffer, deUint32 vertexBindingDescriptionCount, const VkVertexInputBindingDescription2EXT* pVertexBindingDescriptions, deUint32 vertexAttributeDescriptionCount, const VkVertexInputAttributeDescription2EXT* pVertexAttributeDescriptions)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(vertexBindingDescriptionCount);
	DE_UNREF(pVertexBindingDescriptions);
	DE_UNREF(vertexAttributeDescriptionCount);
	DE_UNREF(pVertexAttributeDescriptions);
}

VKAPI_ATTR void VKAPI_CALL cmdSetPatchControlPointsEXT (VkCommandBuffer commandBuffer, deUint32 patchControlPoints)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(patchControlPoints);
}

VKAPI_ATTR void VKAPI_CALL cmdSetRasterizerDiscardEnableEXT (VkCommandBuffer commandBuffer, VkBool32 rasterizerDiscardEnable)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(rasterizerDiscardEnable);
}

VKAPI_ATTR void VKAPI_CALL cmdSetDepthBiasEnableEXT (VkCommandBuffer commandBuffer, VkBool32 depthBiasEnable)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(depthBiasEnable);
}

VKAPI_ATTR void VKAPI_CALL cmdSetLogicOpEXT (VkCommandBuffer commandBuffer, VkLogicOp logicOp)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(logicOp);
}

VKAPI_ATTR void VKAPI_CALL cmdSetPrimitiveRestartEnableEXT (VkCommandBuffer commandBuffer, VkBool32 primitiveRestartEnable)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(primitiveRestartEnable);
}

VKAPI_ATTR void VKAPI_CALL cmdSetColorWriteEnableEXT (VkCommandBuffer commandBuffer, deUint32 attachmentCount, const VkBool32* pColorWriteEnables)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(attachmentCount);
	DE_UNREF(pColorWriteEnables);
}

static const tcu::StaticFunctionLibrary::Entry s_platformFunctions[] =
{
	VK_NULL_FUNC_ENTRY(vkCreateInstance,						createInstance),
	VK_NULL_FUNC_ENTRY(vkGetInstanceProcAddr,					getInstanceProcAddr),
	VK_NULL_FUNC_ENTRY(vkEnumerateInstanceExtensionProperties,	enumerateInstanceExtensionProperties),
	VK_NULL_FUNC_ENTRY(vkEnumerateInstanceLayerProperties,		enumerateInstanceLayerProperties),
	VK_NULL_FUNC_ENTRY(vkEnumerateInstanceVersion,				enumerateInstanceVersion),
};

static const tcu::StaticFunctionLibrary::Entry s_instanceFunctions[] =
{
	VK_NULL_FUNC_ENTRY(vkDestroyInstance,												destroyInstance),
	VK_NULL_FUNC_ENTRY(vkEnumeratePhysicalDevices,										enumeratePhysicalDevices),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceFeatures,										getPhysicalDeviceFeatures),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceFormatProperties,								getPhysicalDeviceFormatProperties),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceImageFormatProperties,						getPhysicalDeviceImageFormatProperties),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceProperties,									getPhysicalDeviceProperties),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceQueueFamilyProperties,						getPhysicalDeviceQueueFamilyProperties),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceMemoryProperties,								getPhysicalDeviceMemoryProperties),
	VK_NULL_FUNC_ENTRY(vkCreateDevice,													createDevice),
	VK_NULL_FUNC_ENTRY(vkEnumerateDeviceExtensionProperties,							enumerateDeviceExtensionProperties),
	VK_NULL_FUNC_ENTRY(vkEnumerateDeviceLayerProperties,								enumerateDeviceLayerProperties),
	VK_NULL_FUNC_ENTRY(vkEnumeratePhysicalDeviceGroups,									enumeratePhysicalDeviceGroups),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceFeatures2,									getPhysicalDeviceFeatures2),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceProperties2,									getPhysicalDeviceProperties2),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceFormatProperties2,							getPhysicalDeviceFormatProperties2),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceImageFormatProperties2,						getPhysicalDeviceImageFormatProperties2),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceQueueFamilyProperties2,						getPhysicalDeviceQueueFamilyProperties2),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceMemoryProperties2,							getPhysicalDeviceMemoryProperties2),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceExternalBufferProperties,						getPhysicalDeviceExternalBufferProperties),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceExternalFenceProperties,						getPhysicalDeviceExternalFenceProperties),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceExternalSemaphoreProperties,					getPhysicalDeviceExternalSemaphoreProperties),
	VK_NULL_FUNC_ENTRY(vkDestroySurfaceKHR,												destroySurfaceKHR),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceSurfaceSupportKHR,							getPhysicalDeviceSurfaceSupportKHR),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceSurfaceCapabilitiesKHR,						getPhysicalDeviceSurfaceCapabilitiesKHR),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceSurfaceFormatsKHR,							getPhysicalDeviceSurfaceFormatsKHR),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceSurfacePresentModesKHR,						getPhysicalDeviceSurfacePresentModesKHR),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDevicePresentRectanglesKHR,							getPhysicalDevicePresentRectanglesKHR),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceDisplayPropertiesKHR,							getPhysicalDeviceDisplayPropertiesKHR),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceDisplayPlanePropertiesKHR,					getPhysicalDeviceDisplayPlanePropertiesKHR),
	VK_NULL_FUNC_ENTRY(vkGetDisplayPlaneSupportedDisplaysKHR,							getDisplayPlaneSupportedDisplaysKHR),
	VK_NULL_FUNC_ENTRY(vkGetDisplayModePropertiesKHR,									getDisplayModePropertiesKHR),
	VK_NULL_FUNC_ENTRY(vkCreateDisplayModeKHR,											createDisplayModeKHR),
	VK_NULL_FUNC_ENTRY(vkGetDisplayPlaneCapabilitiesKHR,								getDisplayPlaneCapabilitiesKHR),
	VK_NULL_FUNC_ENTRY(vkCreateDisplayPlaneSurfaceKHR,									createDisplayPlaneSurfaceKHR),
	VK_NULL_FUNC_ENTRY(vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR,	enumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR,			getPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceSurfaceCapabilities2KHR,						getPhysicalDeviceSurfaceCapabilities2KHR),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceSurfaceFormats2KHR,							getPhysicalDeviceSurfaceFormats2KHR),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceDisplayProperties2KHR,						getPhysicalDeviceDisplayProperties2KHR),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceDisplayPlaneProperties2KHR,					getPhysicalDeviceDisplayPlaneProperties2KHR),
	VK_NULL_FUNC_ENTRY(vkGetDisplayModeProperties2KHR,									getDisplayModeProperties2KHR),
	VK_NULL_FUNC_ENTRY(vkGetDisplayPlaneCapabilities2KHR,								getDisplayPlaneCapabilities2KHR),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceFragmentShadingRatesKHR,						getPhysicalDeviceFragmentShadingRatesKHR),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceRefreshableObjectTypesKHR,					getPhysicalDeviceRefreshableObjectTypesKHR),
	VK_NULL_FUNC_ENTRY(vkReleaseDisplayEXT,												releaseDisplayEXT),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceSurfaceCapabilities2EXT,						getPhysicalDeviceSurfaceCapabilities2EXT),
	VK_NULL_FUNC_ENTRY(vkCreateDebugUtilsMessengerEXT,									createDebugUtilsMessengerEXT),
	VK_NULL_FUNC_ENTRY(vkDestroyDebugUtilsMessengerEXT,									destroyDebugUtilsMessengerEXT),
	VK_NULL_FUNC_ENTRY(vkSubmitDebugUtilsMessageEXT,									submitDebugUtilsMessageEXT),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceMultisamplePropertiesEXT,						getPhysicalDeviceMultisamplePropertiesEXT),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceCalibrateableTimeDomainsEXT,					getPhysicalDeviceCalibrateableTimeDomainsEXT),
	VK_NULL_FUNC_ENTRY(vkCreateHeadlessSurfaceEXT,										createHeadlessSurfaceEXT),
};

static const tcu::StaticFunctionLibrary::Entry s_deviceFunctions[] =
{
	VK_NULL_FUNC_ENTRY(vkGetDeviceProcAddr,							getDeviceProcAddr),
	VK_NULL_FUNC_ENTRY(vkDestroyDevice,								destroyDevice),
	VK_NULL_FUNC_ENTRY(vkGetDeviceQueue,							getDeviceQueue),
	VK_NULL_FUNC_ENTRY(vkQueueSubmit,								queueSubmit),
	VK_NULL_FUNC_ENTRY(vkQueueWaitIdle,								queueWaitIdle),
	VK_NULL_FUNC_ENTRY(vkDeviceWaitIdle,							deviceWaitIdle),
	VK_NULL_FUNC_ENTRY(vkAllocateMemory,							allocateMemory),
	VK_NULL_FUNC_ENTRY(vkMapMemory,									mapMemory),
	VK_NULL_FUNC_ENTRY(vkUnmapMemory,								unmapMemory),
	VK_NULL_FUNC_ENTRY(vkFlushMappedMemoryRanges,					flushMappedMemoryRanges),
	VK_NULL_FUNC_ENTRY(vkInvalidateMappedMemoryRanges,				invalidateMappedMemoryRanges),
	VK_NULL_FUNC_ENTRY(vkGetDeviceMemoryCommitment,					getDeviceMemoryCommitment),
	VK_NULL_FUNC_ENTRY(vkBindBufferMemory,							bindBufferMemory),
	VK_NULL_FUNC_ENTRY(vkBindImageMemory,							bindImageMemory),
	VK_NULL_FUNC_ENTRY(vkGetBufferMemoryRequirements,				getBufferMemoryRequirements),
	VK_NULL_FUNC_ENTRY(vkGetImageMemoryRequirements,				getImageMemoryRequirements),
	VK_NULL_FUNC_ENTRY(vkCreateFence,								createFence),
	VK_NULL_FUNC_ENTRY(vkDestroyFence,								destroyFence),
	VK_NULL_FUNC_ENTRY(vkResetFences,								resetFences),
	VK_NULL_FUNC_ENTRY(vkGetFenceStatus,							getFenceStatus),
	VK_NULL_FUNC_ENTRY(vkWaitForFences,								waitForFences),
	VK_NULL_FUNC_ENTRY(vkCreateSemaphore,							createSemaphore),
	VK_NULL_FUNC_ENTRY(vkDestroySemaphore,							destroySemaphore),
	VK_NULL_FUNC_ENTRY(vkCreateEvent,								createEvent),
	VK_NULL_FUNC_ENTRY(vkDestroyEvent,								destroyEvent),
	VK_NULL_FUNC_ENTRY(vkGetEventStatus,							getEventStatus),
	VK_NULL_FUNC_ENTRY(vkSetEvent,									setEvent),
	VK_NULL_FUNC_ENTRY(vkResetEvent,								resetEvent),
	VK_NULL_FUNC_ENTRY(vkCreateQueryPool,							createQueryPool),
	VK_NULL_FUNC_ENTRY(vkGetQueryPoolResults,						getQueryPoolResults),
	VK_NULL_FUNC_ENTRY(vkCreateBuffer,								createBuffer),
	VK_NULL_FUNC_ENTRY(vkDestroyBuffer,								destroyBuffer),
	VK_NULL_FUNC_ENTRY(vkCreateBufferView,							createBufferView),
	VK_NULL_FUNC_ENTRY(vkDestroyBufferView,							destroyBufferView),
	VK_NULL_FUNC_ENTRY(vkCreateImage,								createImage),
	VK_NULL_FUNC_ENTRY(vkDestroyImage,								destroyImage),
	VK_NULL_FUNC_ENTRY(vkGetImageSubresourceLayout,					getImageSubresourceLayout),
	VK_NULL_FUNC_ENTRY(vkCreateImageView,							createImageView),
	VK_NULL_FUNC_ENTRY(vkDestroyImageView,							destroyImageView),
	VK_NULL_FUNC_ENTRY(vkCreatePipelineCache,						createPipelineCache),
	VK_NULL_FUNC_ENTRY(vkDestroyPipelineCache,						destroyPipelineCache),
	VK_NULL_FUNC_ENTRY(vkCreateGraphicsPipelines,					createGraphicsPipelines),
	VK_NULL_FUNC_ENTRY(vkCreateComputePipelines,					createComputePipelines),
	VK_NULL_FUNC_ENTRY(vkDestroyPipeline,							destroyPipeline),
	VK_NULL_FUNC_ENTRY(vkCreatePipelineLayout,						createPipelineLayout),
	VK_NULL_FUNC_ENTRY(vkDestroyPipelineLayout,						destroyPipelineLayout),
	VK_NULL_FUNC_ENTRY(vkCreateSampler,								createSampler),
	VK_NULL_FUNC_ENTRY(vkDestroySampler,							destroySampler),
	VK_NULL_FUNC_ENTRY(vkCreateDescriptorSetLayout,					createDescriptorSetLayout),
	VK_NULL_FUNC_ENTRY(vkDestroyDescriptorSetLayout,				destroyDescriptorSetLayout),
	VK_NULL_FUNC_ENTRY(vkCreateDescriptorPool,						createDescriptorPool),
	VK_NULL_FUNC_ENTRY(vkResetDescriptorPool,						resetDescriptorPool),
	VK_NULL_FUNC_ENTRY(vkAllocateDescriptorSets,					allocateDescriptorSets),
	VK_NULL_FUNC_ENTRY(vkFreeDescriptorSets,						freeDescriptorSets),
	VK_NULL_FUNC_ENTRY(vkUpdateDescriptorSets,						updateDescriptorSets),
	VK_NULL_FUNC_ENTRY(vkCreateFramebuffer,							createFramebuffer),
	VK_NULL_FUNC_ENTRY(vkDestroyFramebuffer,						destroyFramebuffer),
	VK_NULL_FUNC_ENTRY(vkCreateRenderPass,							createRenderPass),
	VK_NULL_FUNC_ENTRY(vkDestroyRenderPass,							destroyRenderPass),
	VK_NULL_FUNC_ENTRY(vkGetRenderAreaGranularity,					getRenderAreaGranularity),
	VK_NULL_FUNC_ENTRY(vkCreateCommandPool,							createCommandPool),
	VK_NULL_FUNC_ENTRY(vkResetCommandPool,							resetCommandPool),
	VK_NULL_FUNC_ENTRY(vkAllocateCommandBuffers,					allocateCommandBuffers),
	VK_NULL_FUNC_ENTRY(vkFreeCommandBuffers,						freeCommandBuffers),
	VK_NULL_FUNC_ENTRY(vkBeginCommandBuffer,						beginCommandBuffer),
	VK_NULL_FUNC_ENTRY(vkEndCommandBuffer,							endCommandBuffer),
	VK_NULL_FUNC_ENTRY(vkResetCommandBuffer,						resetCommandBuffer),
	VK_NULL_FUNC_ENTRY(vkCmdBindPipeline,							cmdBindPipeline),
	VK_NULL_FUNC_ENTRY(vkCmdSetViewport,							cmdSetViewport),
	VK_NULL_FUNC_ENTRY(vkCmdSetScissor,								cmdSetScissor),
	VK_NULL_FUNC_ENTRY(vkCmdSetLineWidth,							cmdSetLineWidth),
	VK_NULL_FUNC_ENTRY(vkCmdSetDepthBias,							cmdSetDepthBias),
	VK_NULL_FUNC_ENTRY(vkCmdSetBlendConstants,						cmdSetBlendConstants),
	VK_NULL_FUNC_ENTRY(vkCmdSetDepthBounds,							cmdSetDepthBounds),
	VK_NULL_FUNC_ENTRY(vkCmdSetStencilCompareMask,					cmdSetStencilCompareMask),
	VK_NULL_FUNC_ENTRY(vkCmdSetStencilWriteMask,					cmdSetStencilWriteMask),
	VK_NULL_FUNC_ENTRY(vkCmdSetStencilReference,					cmdSetStencilReference),
	VK_NULL_FUNC_ENTRY(vkCmdBindDescriptorSets,						cmdBindDescriptorSets),
	VK_NULL_FUNC_ENTRY(vkCmdBindIndexBuffer,						cmdBindIndexBuffer),
	VK_NULL_FUNC_ENTRY(vkCmdBindVertexBuffers,						cmdBindVertexBuffers),
	VK_NULL_FUNC_ENTRY(vkCmdDraw,									cmdDraw),
	VK_NULL_FUNC_ENTRY(vkCmdDrawIndexed,							cmdDrawIndexed),
	VK_NULL_FUNC_ENTRY(vkCmdDrawIndirect,							cmdDrawIndirect),
	VK_NULL_FUNC_ENTRY(vkCmdDrawIndexedIndirect,					cmdDrawIndexedIndirect),
	VK_NULL_FUNC_ENTRY(vkCmdDispatch,								cmdDispatch),
	VK_NULL_FUNC_ENTRY(vkCmdDispatchIndirect,						cmdDispatchIndirect),
	VK_NULL_FUNC_ENTRY(vkCmdCopyBuffer,								cmdCopyBuffer),
	VK_NULL_FUNC_ENTRY(vkCmdCopyImage,								cmdCopyImage),
	VK_NULL_FUNC_ENTRY(vkCmdBlitImage,								cmdBlitImage),
	VK_NULL_FUNC_ENTRY(vkCmdCopyBufferToImage,						cmdCopyBufferToImage),
	VK_NULL_FUNC_ENTRY(vkCmdCopyImageToBuffer,						cmdCopyImageToBuffer),
	VK_NULL_FUNC_ENTRY(vkCmdUpdateBuffer,							cmdUpdateBuffer),
	VK_NULL_FUNC_ENTRY(vkCmdFillBuffer,								cmdFillBuffer),
	VK_NULL_FUNC_ENTRY(vkCmdClearColorImage,						cmdClearColorImage),
	VK_NULL_FUNC_ENTRY(vkCmdClearDepthStencilImage,					cmdClearDepthStencilImage),
	VK_NULL_FUNC_ENTRY(vkCmdClearAttachments,						cmdClearAttachments),
	VK_NULL_FUNC_ENTRY(vkCmdResolveImage,							cmdResolveImage),
	VK_NULL_FUNC_ENTRY(vkCmdSetEvent,								cmdSetEvent),
	VK_NULL_FUNC_ENTRY(vkCmdResetEvent,								cmdResetEvent),
	VK_NULL_FUNC_ENTRY(vkCmdWaitEvents,								cmdWaitEvents),
	VK_NULL_FUNC_ENTRY(vkCmdPipelineBarrier,						cmdPipelineBarrier),
	VK_NULL_FUNC_ENTRY(vkCmdBeginQuery,								cmdBeginQuery),
	VK_NULL_FUNC_ENTRY(vkCmdEndQuery,								cmdEndQuery),
	VK_NULL_FUNC_ENTRY(vkCmdResetQueryPool,							cmdResetQueryPool),
	VK_NULL_FUNC_ENTRY(vkCmdWriteTimestamp,							cmdWriteTimestamp),
	VK_NULL_FUNC_ENTRY(vkCmdCopyQueryPoolResults,					cmdCopyQueryPoolResults),
	VK_NULL_FUNC_ENTRY(vkCmdPushConstants,							cmdPushConstants),
	VK_NULL_FUNC_ENTRY(vkCmdBeginRenderPass,						cmdBeginRenderPass),
	VK_NULL_FUNC_ENTRY(vkCmdNextSubpass,							cmdNextSubpass),
	VK_NULL_FUNC_ENTRY(vkCmdEndRenderPass,							cmdEndRenderPass),
	VK_NULL_FUNC_ENTRY(vkCmdExecuteCommands,						cmdExecuteCommands),
	VK_NULL_FUNC_ENTRY(vkBindBufferMemory2,							bindBufferMemory2),
	VK_NULL_FUNC_ENTRY(vkBindImageMemory2,							bindImageMemory2),
	VK_NULL_FUNC_ENTRY(vkGetDeviceGroupPeerMemoryFeatures,			getDeviceGroupPeerMemoryFeatures),
	VK_NULL_FUNC_ENTRY(vkCmdSetDeviceMask,							cmdSetDeviceMask),
	VK_NULL_FUNC_ENTRY(vkCmdDispatchBase,							cmdDispatchBase),
	VK_NULL_FUNC_ENTRY(vkGetImageMemoryRequirements2,				getImageMemoryRequirements2),
	VK_NULL_FUNC_ENTRY(vkGetBufferMemoryRequirements2,				getBufferMemoryRequirements2),
	VK_NULL_FUNC_ENTRY(vkGetDeviceQueue2,							getDeviceQueue2),
	VK_NULL_FUNC_ENTRY(vkCreateSamplerYcbcrConversion,				createSamplerYcbcrConversion),
	VK_NULL_FUNC_ENTRY(vkDestroySamplerYcbcrConversion,				destroySamplerYcbcrConversion),
	VK_NULL_FUNC_ENTRY(vkGetDescriptorSetLayoutSupport,				getDescriptorSetLayoutSupport),
	VK_NULL_FUNC_ENTRY(vkCmdDrawIndirectCount,						cmdDrawIndirectCount),
	VK_NULL_FUNC_ENTRY(vkCmdDrawIndexedIndirectCount,				cmdDrawIndexedIndirectCount),
	VK_NULL_FUNC_ENTRY(vkCreateRenderPass2,							createRenderPass2),
	VK_NULL_FUNC_ENTRY(vkCmdBeginRenderPass2,						cmdBeginRenderPass2),
	VK_NULL_FUNC_ENTRY(vkCmdNextSubpass2,							cmdNextSubpass2),
	VK_NULL_FUNC_ENTRY(vkCmdEndRenderPass2,							cmdEndRenderPass2),
	VK_NULL_FUNC_ENTRY(vkResetQueryPool,							resetQueryPool),
	VK_NULL_FUNC_ENTRY(vkGetSemaphoreCounterValue,					getSemaphoreCounterValue),
	VK_NULL_FUNC_ENTRY(vkWaitSemaphores,							waitSemaphores),
	VK_NULL_FUNC_ENTRY(vkSignalSemaphore,							signalSemaphore),
	VK_NULL_FUNC_ENTRY(vkGetBufferDeviceAddress,					getBufferDeviceAddress),
	VK_NULL_FUNC_ENTRY(vkGetBufferOpaqueCaptureAddress,				getBufferOpaqueCaptureAddress),
	VK_NULL_FUNC_ENTRY(vkGetDeviceMemoryOpaqueCaptureAddress,		getDeviceMemoryOpaqueCaptureAddress),
	VK_NULL_FUNC_ENTRY(vkGetCommandPoolMemoryConsumption,			getCommandPoolMemoryConsumption),
	VK_NULL_FUNC_ENTRY(vkGetFaultData,								getFaultData),
	VK_NULL_FUNC_ENTRY(vkCreateSwapchainKHR,						createSwapchainKHR),
	VK_NULL_FUNC_ENTRY(vkGetSwapchainImagesKHR,						getSwapchainImagesKHR),
	VK_NULL_FUNC_ENTRY(vkAcquireNextImageKHR,						acquireNextImageKHR),
	VK_NULL_FUNC_ENTRY(vkQueuePresentKHR,							queuePresentKHR),
	VK_NULL_FUNC_ENTRY(vkGetDeviceGroupPresentCapabilitiesKHR,		getDeviceGroupPresentCapabilitiesKHR),
	VK_NULL_FUNC_ENTRY(vkGetDeviceGroupSurfacePresentModesKHR,		getDeviceGroupSurfacePresentModesKHR),
	VK_NULL_FUNC_ENTRY(vkAcquireNextImage2KHR,						acquireNextImage2KHR),
	VK_NULL_FUNC_ENTRY(vkCreateSharedSwapchainsKHR,					createSharedSwapchainsKHR),
	VK_NULL_FUNC_ENTRY(vkGetMemoryFdKHR,							getMemoryFdKHR),
	VK_NULL_FUNC_ENTRY(vkGetMemoryFdPropertiesKHR,					getMemoryFdPropertiesKHR),
	VK_NULL_FUNC_ENTRY(vkImportSemaphoreFdKHR,						importSemaphoreFdKHR),
	VK_NULL_FUNC_ENTRY(vkGetSemaphoreFdKHR,							getSemaphoreFdKHR),
	VK_NULL_FUNC_ENTRY(vkGetSwapchainStatusKHR,						getSwapchainStatusKHR),
	VK_NULL_FUNC_ENTRY(vkImportFenceFdKHR,							importFenceFdKHR),
	VK_NULL_FUNC_ENTRY(vkGetFenceFdKHR,								getFenceFdKHR),
	VK_NULL_FUNC_ENTRY(vkAcquireProfilingLockKHR,					acquireProfilingLockKHR),
	VK_NULL_FUNC_ENTRY(vkReleaseProfilingLockKHR,					releaseProfilingLockKHR),
	VK_NULL_FUNC_ENTRY(vkCmdSetFragmentShadingRateKHR,				cmdSetFragmentShadingRateKHR),
	VK_NULL_FUNC_ENTRY(vkCmdRefreshObjectsKHR,						cmdRefreshObjectsKHR),
	VK_NULL_FUNC_ENTRY(vkCmdSetEvent2KHR,							cmdSetEvent2KHR),
	VK_NULL_FUNC_ENTRY(vkCmdResetEvent2KHR,							cmdResetEvent2KHR),
	VK_NULL_FUNC_ENTRY(vkCmdWaitEvents2KHR,							cmdWaitEvents2KHR),
	VK_NULL_FUNC_ENTRY(vkCmdPipelineBarrier2KHR,					cmdPipelineBarrier2KHR),
	VK_NULL_FUNC_ENTRY(vkCmdWriteTimestamp2KHR,						cmdWriteTimestamp2KHR),
	VK_NULL_FUNC_ENTRY(vkQueueSubmit2KHR,							queueSubmit2KHR),
	VK_NULL_FUNC_ENTRY(vkCmdWriteBufferMarker2AMD,					cmdWriteBufferMarker2AMD),
	VK_NULL_FUNC_ENTRY(vkGetQueueCheckpointData2NV,					getQueueCheckpointData2NV),
	VK_NULL_FUNC_ENTRY(vkCmdCopyBuffer2KHR,							cmdCopyBuffer2KHR),
	VK_NULL_FUNC_ENTRY(vkCmdCopyImage2KHR,							cmdCopyImage2KHR),
	VK_NULL_FUNC_ENTRY(vkCmdCopyBufferToImage2KHR,					cmdCopyBufferToImage2KHR),
	VK_NULL_FUNC_ENTRY(vkCmdCopyImageToBuffer2KHR,					cmdCopyImageToBuffer2KHR),
	VK_NULL_FUNC_ENTRY(vkCmdBlitImage2KHR,							cmdBlitImage2KHR),
	VK_NULL_FUNC_ENTRY(vkCmdResolveImage2KHR,						cmdResolveImage2KHR),
	VK_NULL_FUNC_ENTRY(vkDisplayPowerControlEXT,					displayPowerControlEXT),
	VK_NULL_FUNC_ENTRY(vkRegisterDeviceEventEXT,					registerDeviceEventEXT),
	VK_NULL_FUNC_ENTRY(vkRegisterDisplayEventEXT,					registerDisplayEventEXT),
	VK_NULL_FUNC_ENTRY(vkGetSwapchainCounterEXT,					getSwapchainCounterEXT),
	VK_NULL_FUNC_ENTRY(vkCmdSetDiscardRectangleEXT,					cmdSetDiscardRectangleEXT),
	VK_NULL_FUNC_ENTRY(vkSetHdrMetadataEXT,							setHdrMetadataEXT),
	VK_NULL_FUNC_ENTRY(vkSetDebugUtilsObjectNameEXT,				setDebugUtilsObjectNameEXT),
	VK_NULL_FUNC_ENTRY(vkSetDebugUtilsObjectTagEXT,					setDebugUtilsObjectTagEXT),
	VK_NULL_FUNC_ENTRY(vkQueueBeginDebugUtilsLabelEXT,				queueBeginDebugUtilsLabelEXT),
	VK_NULL_FUNC_ENTRY(vkQueueEndDebugUtilsLabelEXT,				queueEndDebugUtilsLabelEXT),
	VK_NULL_FUNC_ENTRY(vkQueueInsertDebugUtilsLabelEXT,				queueInsertDebugUtilsLabelEXT),
	VK_NULL_FUNC_ENTRY(vkCmdBeginDebugUtilsLabelEXT,				cmdBeginDebugUtilsLabelEXT),
	VK_NULL_FUNC_ENTRY(vkCmdEndDebugUtilsLabelEXT,					cmdEndDebugUtilsLabelEXT),
	VK_NULL_FUNC_ENTRY(vkCmdInsertDebugUtilsLabelEXT,				cmdInsertDebugUtilsLabelEXT),
	VK_NULL_FUNC_ENTRY(vkCmdSetSampleLocationsEXT,					cmdSetSampleLocationsEXT),
	VK_NULL_FUNC_ENTRY(vkGetImageDrmFormatModifierPropertiesEXT,	getImageDrmFormatModifierPropertiesEXT),
	VK_NULL_FUNC_ENTRY(vkGetMemoryHostPointerPropertiesEXT,			getMemoryHostPointerPropertiesEXT),
	VK_NULL_FUNC_ENTRY(vkGetCalibratedTimestampsEXT,				getCalibratedTimestampsEXT),
	VK_NULL_FUNC_ENTRY(vkCmdSetLineStippleEXT,						cmdSetLineStippleEXT),
	VK_NULL_FUNC_ENTRY(vkCmdSetCullModeEXT,							cmdSetCullModeEXT),
	VK_NULL_FUNC_ENTRY(vkCmdSetFrontFaceEXT,						cmdSetFrontFaceEXT),
	VK_NULL_FUNC_ENTRY(vkCmdSetPrimitiveTopologyEXT,				cmdSetPrimitiveTopologyEXT),
	VK_NULL_FUNC_ENTRY(vkCmdSetViewportWithCountEXT,				cmdSetViewportWithCountEXT),
	VK_NULL_FUNC_ENTRY(vkCmdSetScissorWithCountEXT,					cmdSetScissorWithCountEXT),
	VK_NULL_FUNC_ENTRY(vkCmdBindVertexBuffers2EXT,					cmdBindVertexBuffers2EXT),
	VK_NULL_FUNC_ENTRY(vkCmdSetDepthTestEnableEXT,					cmdSetDepthTestEnableEXT),
	VK_NULL_FUNC_ENTRY(vkCmdSetDepthWriteEnableEXT,					cmdSetDepthWriteEnableEXT),
	VK_NULL_FUNC_ENTRY(vkCmdSetDepthCompareOpEXT,					cmdSetDepthCompareOpEXT),
	VK_NULL_FUNC_ENTRY(vkCmdSetDepthBoundsTestEnableEXT,			cmdSetDepthBoundsTestEnableEXT),
	VK_NULL_FUNC_ENTRY(vkCmdSetStencilTestEnableEXT,				cmdSetStencilTestEnableEXT),
	VK_NULL_FUNC_ENTRY(vkCmdSetStencilOpEXT,						cmdSetStencilOpEXT),
	VK_NULL_FUNC_ENTRY(vkCmdSetVertexInputEXT,						cmdSetVertexInputEXT),
	VK_NULL_FUNC_ENTRY(vkCmdSetPatchControlPointsEXT,				cmdSetPatchControlPointsEXT),
	VK_NULL_FUNC_ENTRY(vkCmdSetRasterizerDiscardEnableEXT,			cmdSetRasterizerDiscardEnableEXT),
	VK_NULL_FUNC_ENTRY(vkCmdSetDepthBiasEnableEXT,					cmdSetDepthBiasEnableEXT),
	VK_NULL_FUNC_ENTRY(vkCmdSetLogicOpEXT,							cmdSetLogicOpEXT),
	VK_NULL_FUNC_ENTRY(vkCmdSetPrimitiveRestartEnableEXT,			cmdSetPrimitiveRestartEnableEXT),
	VK_NULL_FUNC_ENTRY(vkCmdSetColorWriteEnableEXT,					cmdSetColorWriteEnableEXT),
};

