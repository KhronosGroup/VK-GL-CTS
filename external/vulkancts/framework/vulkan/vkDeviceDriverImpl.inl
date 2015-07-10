/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 */

VkResult DeviceDriver::destroyDevice (VkDevice device) const
{
	return m_vk.destroyDevice(device);
}

VkResult DeviceDriver::getGlobalExtensionProperties (const char* pLayerName, deUint32* pCount, VkExtensionProperties* pProperties) const
{
	return m_vk.getGlobalExtensionProperties(pLayerName, pCount, pProperties);
}

VkResult DeviceDriver::getPhysicalDeviceExtensionProperties (VkPhysicalDevice physicalDevice, const char* pLayerName, deUint32* pCount, VkExtensionProperties* pProperties) const
{
	return m_vk.getPhysicalDeviceExtensionProperties(physicalDevice, pLayerName, pCount, pProperties);
}

VkResult DeviceDriver::getGlobalLayerProperties (deUint32* pCount, VkLayerProperties* pProperties) const
{
	return m_vk.getGlobalLayerProperties(pCount, pProperties);
}

VkResult DeviceDriver::getPhysicalDeviceLayerProperties (VkPhysicalDevice physicalDevice, deUint32* pCount, VkLayerProperties* pProperties) const
{
	return m_vk.getPhysicalDeviceLayerProperties(physicalDevice, pCount, pProperties);
}

VkResult DeviceDriver::getDeviceQueue (VkDevice device, deUint32 queueFamilyIndex, deUint32 queueIndex, VkQueue* pQueue) const
{
	return m_vk.getDeviceQueue(device, queueFamilyIndex, queueIndex, pQueue);
}

VkResult DeviceDriver::queueSubmit (VkQueue queue, deUint32 cmdBufferCount, const VkCmdBuffer* pCmdBuffers, VkFence fence) const
{
	return m_vk.queueSubmit(queue, cmdBufferCount, pCmdBuffers, fence);
}

VkResult DeviceDriver::queueWaitIdle (VkQueue queue) const
{
	return m_vk.queueWaitIdle(queue);
}

VkResult DeviceDriver::deviceWaitIdle (VkDevice device) const
{
	return m_vk.deviceWaitIdle(device);
}

VkResult DeviceDriver::allocMemory (VkDevice device, const VkMemoryAllocInfo* pAllocInfo, VkDeviceMemory* pMem) const
{
	return m_vk.allocMemory(device, pAllocInfo, pMem);
}

VkResult DeviceDriver::freeMemory (VkDevice device, VkDeviceMemory mem) const
{
	return m_vk.freeMemory(device, mem);
}

VkResult DeviceDriver::mapMemory (VkDevice device, VkDeviceMemory mem, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** ppData) const
{
	return m_vk.mapMemory(device, mem, offset, size, flags, ppData);
}

VkResult DeviceDriver::unmapMemory (VkDevice device, VkDeviceMemory mem) const
{
	return m_vk.unmapMemory(device, mem);
}

VkResult DeviceDriver::flushMappedMemoryRanges (VkDevice device, deUint32 memRangeCount, const VkMappedMemoryRange* pMemRanges) const
{
	return m_vk.flushMappedMemoryRanges(device, memRangeCount, pMemRanges);
}

VkResult DeviceDriver::invalidateMappedMemoryRanges (VkDevice device, deUint32 memRangeCount, const VkMappedMemoryRange* pMemRanges) const
{
	return m_vk.invalidateMappedMemoryRanges(device, memRangeCount, pMemRanges);
}

VkResult DeviceDriver::getDeviceMemoryCommitment (VkDevice device, VkDeviceMemory memory, VkDeviceSize* pCommittedMemoryInBytes) const
{
	return m_vk.getDeviceMemoryCommitment(device, memory, pCommittedMemoryInBytes);
}

VkResult DeviceDriver::bindBufferMemory (VkDevice device, VkBuffer buffer, VkDeviceMemory mem, VkDeviceSize memOffset) const
{
	return m_vk.bindBufferMemory(device, buffer, mem, memOffset);
}

VkResult DeviceDriver::bindImageMemory (VkDevice device, VkImage image, VkDeviceMemory mem, VkDeviceSize memOffset) const
{
	return m_vk.bindImageMemory(device, image, mem, memOffset);
}

VkResult DeviceDriver::getBufferMemoryRequirements (VkDevice device, VkBuffer buffer, VkMemoryRequirements* pMemoryRequirements) const
{
	return m_vk.getBufferMemoryRequirements(device, buffer, pMemoryRequirements);
}

VkResult DeviceDriver::getImageMemoryRequirements (VkDevice device, VkImage image, VkMemoryRequirements* pMemoryRequirements) const
{
	return m_vk.getImageMemoryRequirements(device, image, pMemoryRequirements);
}

VkResult DeviceDriver::getImageSparseMemoryRequirements (VkDevice device, VkImage image, deUint32* pNumRequirements, VkSparseImageMemoryRequirements* pSparseMemoryRequirements) const
{
	return m_vk.getImageSparseMemoryRequirements(device, image, pNumRequirements, pSparseMemoryRequirements);
}

VkResult DeviceDriver::getPhysicalDeviceSparseImageFormatProperties (VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, deUint32 samples, VkImageUsageFlags usage, VkImageTiling tiling, deUint32* pNumProperties, VkSparseImageFormatProperties* pProperties) const
{
	return m_vk.getPhysicalDeviceSparseImageFormatProperties(physicalDevice, format, type, samples, usage, tiling, pNumProperties, pProperties);
}

VkResult DeviceDriver::queueBindSparseBufferMemory (VkQueue queue, VkBuffer buffer, deUint32 numBindings, const VkSparseMemoryBindInfo* pBindInfo) const
{
	return m_vk.queueBindSparseBufferMemory(queue, buffer, numBindings, pBindInfo);
}

VkResult DeviceDriver::queueBindSparseImageOpaqueMemory (VkQueue queue, VkImage image, deUint32 numBindings, const VkSparseMemoryBindInfo* pBindInfo) const
{
	return m_vk.queueBindSparseImageOpaqueMemory(queue, image, numBindings, pBindInfo);
}

VkResult DeviceDriver::queueBindSparseImageMemory (VkQueue queue, VkImage image, deUint32 numBindings, const VkSparseImageMemoryBindInfo* pBindInfo) const
{
	return m_vk.queueBindSparseImageMemory(queue, image, numBindings, pBindInfo);
}

VkResult DeviceDriver::createFence (VkDevice device, const VkFenceCreateInfo* pCreateInfo, VkFence* pFence) const
{
	return m_vk.createFence(device, pCreateInfo, pFence);
}

VkResult DeviceDriver::destroyFence (VkDevice device, VkFence fence) const
{
	return m_vk.destroyFence(device, fence);
}

VkResult DeviceDriver::resetFences (VkDevice device, deUint32 fenceCount, const VkFence* pFences) const
{
	return m_vk.resetFences(device, fenceCount, pFences);
}

VkResult DeviceDriver::getFenceStatus (VkDevice device, VkFence fence) const
{
	return m_vk.getFenceStatus(device, fence);
}

VkResult DeviceDriver::waitForFences (VkDevice device, deUint32 fenceCount, const VkFence* pFences, VkBool32 waitAll, deUint64 timeout) const
{
	return m_vk.waitForFences(device, fenceCount, pFences, waitAll, timeout);
}

VkResult DeviceDriver::createSemaphore (VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo, VkSemaphore* pSemaphore) const
{
	return m_vk.createSemaphore(device, pCreateInfo, pSemaphore);
}

VkResult DeviceDriver::destroySemaphore (VkDevice device, VkSemaphore semaphore) const
{
	return m_vk.destroySemaphore(device, semaphore);
}

VkResult DeviceDriver::queueSignalSemaphore (VkQueue queue, VkSemaphore semaphore) const
{
	return m_vk.queueSignalSemaphore(queue, semaphore);
}

VkResult DeviceDriver::queueWaitSemaphore (VkQueue queue, VkSemaphore semaphore) const
{
	return m_vk.queueWaitSemaphore(queue, semaphore);
}

VkResult DeviceDriver::createEvent (VkDevice device, const VkEventCreateInfo* pCreateInfo, VkEvent* pEvent) const
{
	return m_vk.createEvent(device, pCreateInfo, pEvent);
}

VkResult DeviceDriver::destroyEvent (VkDevice device, VkEvent event) const
{
	return m_vk.destroyEvent(device, event);
}

VkResult DeviceDriver::getEventStatus (VkDevice device, VkEvent event) const
{
	return m_vk.getEventStatus(device, event);
}

VkResult DeviceDriver::setEvent (VkDevice device, VkEvent event) const
{
	return m_vk.setEvent(device, event);
}

VkResult DeviceDriver::resetEvent (VkDevice device, VkEvent event) const
{
	return m_vk.resetEvent(device, event);
}

VkResult DeviceDriver::createQueryPool (VkDevice device, const VkQueryPoolCreateInfo* pCreateInfo, VkQueryPool* pQueryPool) const
{
	return m_vk.createQueryPool(device, pCreateInfo, pQueryPool);
}

VkResult DeviceDriver::destroyQueryPool (VkDevice device, VkQueryPool queryPool) const
{
	return m_vk.destroyQueryPool(device, queryPool);
}

VkResult DeviceDriver::getQueryPoolResults (VkDevice device, VkQueryPool queryPool, deUint32 startQuery, deUint32 queryCount, deUintptr* pDataSize, void* pData, VkQueryResultFlags flags) const
{
	return m_vk.getQueryPoolResults(device, queryPool, startQuery, queryCount, pDataSize, pData, flags);
}

VkResult DeviceDriver::createBuffer (VkDevice device, const VkBufferCreateInfo* pCreateInfo, VkBuffer* pBuffer) const
{
	return m_vk.createBuffer(device, pCreateInfo, pBuffer);
}

VkResult DeviceDriver::destroyBuffer (VkDevice device, VkBuffer buffer) const
{
	return m_vk.destroyBuffer(device, buffer);
}

VkResult DeviceDriver::createBufferView (VkDevice device, const VkBufferViewCreateInfo* pCreateInfo, VkBufferView* pView) const
{
	return m_vk.createBufferView(device, pCreateInfo, pView);
}

VkResult DeviceDriver::destroyBufferView (VkDevice device, VkBufferView bufferView) const
{
	return m_vk.destroyBufferView(device, bufferView);
}

VkResult DeviceDriver::createImage (VkDevice device, const VkImageCreateInfo* pCreateInfo, VkImage* pImage) const
{
	return m_vk.createImage(device, pCreateInfo, pImage);
}

VkResult DeviceDriver::destroyImage (VkDevice device, VkImage image) const
{
	return m_vk.destroyImage(device, image);
}

VkResult DeviceDriver::getImageSubresourceLayout (VkDevice device, VkImage image, const VkImageSubresource* pSubresource, VkSubresourceLayout* pLayout) const
{
	return m_vk.getImageSubresourceLayout(device, image, pSubresource, pLayout);
}

VkResult DeviceDriver::createImageView (VkDevice device, const VkImageViewCreateInfo* pCreateInfo, VkImageView* pView) const
{
	return m_vk.createImageView(device, pCreateInfo, pView);
}

VkResult DeviceDriver::destroyImageView (VkDevice device, VkImageView imageView) const
{
	return m_vk.destroyImageView(device, imageView);
}

VkResult DeviceDriver::createAttachmentView (VkDevice device, const VkAttachmentViewCreateInfo* pCreateInfo, VkAttachmentView* pView) const
{
	return m_vk.createAttachmentView(device, pCreateInfo, pView);
}

VkResult DeviceDriver::destroyAttachmentView (VkDevice device, VkAttachmentView attachmentView) const
{
	return m_vk.destroyAttachmentView(device, attachmentView);
}

VkResult DeviceDriver::createShaderModule (VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, VkShaderModule* pShaderModule) const
{
	return m_vk.createShaderModule(device, pCreateInfo, pShaderModule);
}

VkResult DeviceDriver::destroyShaderModule (VkDevice device, VkShaderModule shaderModule) const
{
	return m_vk.destroyShaderModule(device, shaderModule);
}

VkResult DeviceDriver::createShader (VkDevice device, const VkShaderCreateInfo* pCreateInfo, VkShader* pShader) const
{
	return m_vk.createShader(device, pCreateInfo, pShader);
}

VkResult DeviceDriver::destroyShader (VkDevice device, VkShader shader) const
{
	return m_vk.destroyShader(device, shader);
}

VkResult DeviceDriver::createPipelineCache (VkDevice device, const VkPipelineCacheCreateInfo* pCreateInfo, VkPipelineCache* pPipelineCache) const
{
	return m_vk.createPipelineCache(device, pCreateInfo, pPipelineCache);
}

VkResult DeviceDriver::destroyPipelineCache (VkDevice device, VkPipelineCache pipelineCache) const
{
	return m_vk.destroyPipelineCache(device, pipelineCache);
}

deUintptr DeviceDriver::getPipelineCacheSize (VkDevice device, VkPipelineCache pipelineCache) const
{
	return m_vk.getPipelineCacheSize(device, pipelineCache);
}

VkResult DeviceDriver::getPipelineCacheData (VkDevice device, VkPipelineCache pipelineCache, void* pData) const
{
	return m_vk.getPipelineCacheData(device, pipelineCache, pData);
}

VkResult DeviceDriver::mergePipelineCaches (VkDevice device, VkPipelineCache destCache, deUint32 srcCacheCount, const VkPipelineCache* pSrcCaches) const
{
	return m_vk.mergePipelineCaches(device, destCache, srcCacheCount, pSrcCaches);
}

VkResult DeviceDriver::createGraphicsPipelines (VkDevice device, VkPipelineCache pipelineCache, deUint32 count, const VkGraphicsPipelineCreateInfo* pCreateInfos, VkPipeline* pPipelines) const
{
	return m_vk.createGraphicsPipelines(device, pipelineCache, count, pCreateInfos, pPipelines);
}

VkResult DeviceDriver::createComputePipelines (VkDevice device, VkPipelineCache pipelineCache, deUint32 count, const VkComputePipelineCreateInfo* pCreateInfos, VkPipeline* pPipelines) const
{
	return m_vk.createComputePipelines(device, pipelineCache, count, pCreateInfos, pPipelines);
}

VkResult DeviceDriver::destroyPipeline (VkDevice device, VkPipeline pipeline) const
{
	return m_vk.destroyPipeline(device, pipeline);
}

VkResult DeviceDriver::createPipelineLayout (VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo, VkPipelineLayout* pPipelineLayout) const
{
	return m_vk.createPipelineLayout(device, pCreateInfo, pPipelineLayout);
}

VkResult DeviceDriver::destroyPipelineLayout (VkDevice device, VkPipelineLayout pipelineLayout) const
{
	return m_vk.destroyPipelineLayout(device, pipelineLayout);
}

VkResult DeviceDriver::createSampler (VkDevice device, const VkSamplerCreateInfo* pCreateInfo, VkSampler* pSampler) const
{
	return m_vk.createSampler(device, pCreateInfo, pSampler);
}

VkResult DeviceDriver::destroySampler (VkDevice device, VkSampler sampler) const
{
	return m_vk.destroySampler(device, sampler);
}

VkResult DeviceDriver::createDescriptorSetLayout (VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, VkDescriptorSetLayout* pSetLayout) const
{
	return m_vk.createDescriptorSetLayout(device, pCreateInfo, pSetLayout);
}

VkResult DeviceDriver::destroyDescriptorSetLayout (VkDevice device, VkDescriptorSetLayout descriptorSetLayout) const
{
	return m_vk.destroyDescriptorSetLayout(device, descriptorSetLayout);
}

VkResult DeviceDriver::createDescriptorPool (VkDevice device, VkDescriptorPoolUsage poolUsage, deUint32 maxSets, const VkDescriptorPoolCreateInfo* pCreateInfo, VkDescriptorPool* pDescriptorPool) const
{
	return m_vk.createDescriptorPool(device, poolUsage, maxSets, pCreateInfo, pDescriptorPool);
}

VkResult DeviceDriver::destroyDescriptorPool (VkDevice device, VkDescriptorPool descriptorPool) const
{
	return m_vk.destroyDescriptorPool(device, descriptorPool);
}

VkResult DeviceDriver::resetDescriptorPool (VkDevice device, VkDescriptorPool descriptorPool) const
{
	return m_vk.resetDescriptorPool(device, descriptorPool);
}

VkResult DeviceDriver::allocDescriptorSets (VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorSetUsage setUsage, deUint32 count, const VkDescriptorSetLayout* pSetLayouts, VkDescriptorSet* pDescriptorSets, deUint32* pCount) const
{
	return m_vk.allocDescriptorSets(device, descriptorPool, setUsage, count, pSetLayouts, pDescriptorSets, pCount);
}

VkResult DeviceDriver::updateDescriptorSets (VkDevice device, deUint32 writeCount, const VkWriteDescriptorSet* pDescriptorWrites, deUint32 copyCount, const VkCopyDescriptorSet* pDescriptorCopies) const
{
	return m_vk.updateDescriptorSets(device, writeCount, pDescriptorWrites, copyCount, pDescriptorCopies);
}

VkResult DeviceDriver::createDynamicViewportState (VkDevice device, const VkDynamicViewportStateCreateInfo* pCreateInfo, VkDynamicViewportState* pState) const
{
	return m_vk.createDynamicViewportState(device, pCreateInfo, pState);
}

VkResult DeviceDriver::destroyDynamicViewportState (VkDevice device, VkDynamicViewportState dynamicViewportState) const
{
	return m_vk.destroyDynamicViewportState(device, dynamicViewportState);
}

VkResult DeviceDriver::createDynamicRasterState (VkDevice device, const VkDynamicRasterStateCreateInfo* pCreateInfo, VkDynamicRasterState* pState) const
{
	return m_vk.createDynamicRasterState(device, pCreateInfo, pState);
}

VkResult DeviceDriver::destroyDynamicRasterState (VkDevice device, VkDynamicRasterState dynamicRasterState) const
{
	return m_vk.destroyDynamicRasterState(device, dynamicRasterState);
}

VkResult DeviceDriver::createDynamicColorBlendState (VkDevice device, const VkDynamicColorBlendStateCreateInfo* pCreateInfo, VkDynamicColorBlendState* pState) const
{
	return m_vk.createDynamicColorBlendState(device, pCreateInfo, pState);
}

VkResult DeviceDriver::destroyDynamicColorBlendState (VkDevice device, VkDynamicColorBlendState dynamicColorBlendState) const
{
	return m_vk.destroyDynamicColorBlendState(device, dynamicColorBlendState);
}

VkResult DeviceDriver::createDynamicDepthStencilState (VkDevice device, const VkDynamicDepthStencilStateCreateInfo* pCreateInfo, VkDynamicDepthStencilState* pState) const
{
	return m_vk.createDynamicDepthStencilState(device, pCreateInfo, pState);
}

VkResult DeviceDriver::destroyDynamicDepthStencilState (VkDevice device, VkDynamicDepthStencilState dynamicDepthStencilState) const
{
	return m_vk.destroyDynamicDepthStencilState(device, dynamicDepthStencilState);
}

VkResult DeviceDriver::createFramebuffer (VkDevice device, const VkFramebufferCreateInfo* pCreateInfo, VkFramebuffer* pFramebuffer) const
{
	return m_vk.createFramebuffer(device, pCreateInfo, pFramebuffer);
}

VkResult DeviceDriver::destroyFramebuffer (VkDevice device, VkFramebuffer framebuffer) const
{
	return m_vk.destroyFramebuffer(device, framebuffer);
}

VkResult DeviceDriver::createRenderPass (VkDevice device, const VkRenderPassCreateInfo* pCreateInfo, VkRenderPass* pRenderPass) const
{
	return m_vk.createRenderPass(device, pCreateInfo, pRenderPass);
}

VkResult DeviceDriver::destroyRenderPass (VkDevice device, VkRenderPass renderPass) const
{
	return m_vk.destroyRenderPass(device, renderPass);
}

VkResult DeviceDriver::getRenderAreaGranularity (VkDevice device, VkRenderPass renderPass, VkExtent2D* pGranularity) const
{
	return m_vk.getRenderAreaGranularity(device, renderPass, pGranularity);
}

VkResult DeviceDriver::createCommandPool (VkDevice device, const VkCmdPoolCreateInfo* pCreateInfo, VkCmdPool* pCmdPool) const
{
	return m_vk.createCommandPool(device, pCreateInfo, pCmdPool);
}

VkResult DeviceDriver::destroyCommandPool (VkDevice device, VkCmdPool cmdPool) const
{
	return m_vk.destroyCommandPool(device, cmdPool);
}

VkResult DeviceDriver::resetCommandPool (VkDevice device, VkCmdPool cmdPool, VkCmdPoolResetFlags flags) const
{
	return m_vk.resetCommandPool(device, cmdPool, flags);
}

VkResult DeviceDriver::createCommandBuffer (VkDevice device, const VkCmdBufferCreateInfo* pCreateInfo, VkCmdBuffer* pCmdBuffer) const
{
	return m_vk.createCommandBuffer(device, pCreateInfo, pCmdBuffer);
}

VkResult DeviceDriver::destroyCommandBuffer (VkDevice device, VkCmdBuffer commandBuffer) const
{
	return m_vk.destroyCommandBuffer(device, commandBuffer);
}

VkResult DeviceDriver::beginCommandBuffer (VkCmdBuffer cmdBuffer, const VkCmdBufferBeginInfo* pBeginInfo) const
{
	return m_vk.beginCommandBuffer(cmdBuffer, pBeginInfo);
}

VkResult DeviceDriver::endCommandBuffer (VkCmdBuffer cmdBuffer) const
{
	return m_vk.endCommandBuffer(cmdBuffer);
}

VkResult DeviceDriver::resetCommandBuffer (VkCmdBuffer cmdBuffer, VkCmdBufferResetFlags flags) const
{
	return m_vk.resetCommandBuffer(cmdBuffer, flags);
}

void DeviceDriver::cmdBindPipeline (VkCmdBuffer cmdBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline) const
{
	m_vk.cmdBindPipeline(cmdBuffer, pipelineBindPoint, pipeline);
}

void DeviceDriver::cmdBindDynamicViewportState (VkCmdBuffer cmdBuffer, VkDynamicViewportState dynamicViewportState) const
{
	m_vk.cmdBindDynamicViewportState(cmdBuffer, dynamicViewportState);
}

void DeviceDriver::cmdBindDynamicRasterState (VkCmdBuffer cmdBuffer, VkDynamicRasterState dynamicRasterState) const
{
	m_vk.cmdBindDynamicRasterState(cmdBuffer, dynamicRasterState);
}

void DeviceDriver::cmdBindDynamicColorBlendState (VkCmdBuffer cmdBuffer, VkDynamicColorBlendState dynamicColorBlendState) const
{
	m_vk.cmdBindDynamicColorBlendState(cmdBuffer, dynamicColorBlendState);
}

void DeviceDriver::cmdBindDynamicDepthStencilState (VkCmdBuffer cmdBuffer, VkDynamicDepthStencilState dynamicDepthStencilState) const
{
	m_vk.cmdBindDynamicDepthStencilState(cmdBuffer, dynamicDepthStencilState);
}

void DeviceDriver::cmdBindDescriptorSets (VkCmdBuffer cmdBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, deUint32 firstSet, deUint32 setCount, const VkDescriptorSet* pDescriptorSets, deUint32 dynamicOffsetCount, const deUint32* pDynamicOffsets) const
{
	m_vk.cmdBindDescriptorSets(cmdBuffer, pipelineBindPoint, layout, firstSet, setCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
}

void DeviceDriver::cmdBindIndexBuffer (VkCmdBuffer cmdBuffer, VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType) const
{
	m_vk.cmdBindIndexBuffer(cmdBuffer, buffer, offset, indexType);
}

void DeviceDriver::cmdBindVertexBuffers (VkCmdBuffer cmdBuffer, deUint32 startBinding, deUint32 bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets) const
{
	m_vk.cmdBindVertexBuffers(cmdBuffer, startBinding, bindingCount, pBuffers, pOffsets);
}

void DeviceDriver::cmdDraw (VkCmdBuffer cmdBuffer, deUint32 firstVertex, deUint32 vertexCount, deUint32 firstInstance, deUint32 instanceCount) const
{
	m_vk.cmdDraw(cmdBuffer, firstVertex, vertexCount, firstInstance, instanceCount);
}

void DeviceDriver::cmdDrawIndexed (VkCmdBuffer cmdBuffer, deUint32 firstIndex, deUint32 indexCount, deInt32 vertexOffset, deUint32 firstInstance, deUint32 instanceCount) const
{
	m_vk.cmdDrawIndexed(cmdBuffer, firstIndex, indexCount, vertexOffset, firstInstance, instanceCount);
}

void DeviceDriver::cmdDrawIndirect (VkCmdBuffer cmdBuffer, VkBuffer buffer, VkDeviceSize offset, deUint32 count, deUint32 stride) const
{
	m_vk.cmdDrawIndirect(cmdBuffer, buffer, offset, count, stride);
}

void DeviceDriver::cmdDrawIndexedIndirect (VkCmdBuffer cmdBuffer, VkBuffer buffer, VkDeviceSize offset, deUint32 count, deUint32 stride) const
{
	m_vk.cmdDrawIndexedIndirect(cmdBuffer, buffer, offset, count, stride);
}

void DeviceDriver::cmdDispatch (VkCmdBuffer cmdBuffer, deUint32 x, deUint32 y, deUint32 z) const
{
	m_vk.cmdDispatch(cmdBuffer, x, y, z);
}

void DeviceDriver::cmdDispatchIndirect (VkCmdBuffer cmdBuffer, VkBuffer buffer, VkDeviceSize offset) const
{
	m_vk.cmdDispatchIndirect(cmdBuffer, buffer, offset);
}

void DeviceDriver::cmdCopyBuffer (VkCmdBuffer cmdBuffer, VkBuffer srcBuffer, VkBuffer destBuffer, deUint32 regionCount, const VkBufferCopy* pRegions) const
{
	m_vk.cmdCopyBuffer(cmdBuffer, srcBuffer, destBuffer, regionCount, pRegions);
}

void DeviceDriver::cmdCopyImage (VkCmdBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage destImage, VkImageLayout destImageLayout, deUint32 regionCount, const VkImageCopy* pRegions) const
{
	m_vk.cmdCopyImage(cmdBuffer, srcImage, srcImageLayout, destImage, destImageLayout, regionCount, pRegions);
}

void DeviceDriver::cmdBlitImage (VkCmdBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage destImage, VkImageLayout destImageLayout, deUint32 regionCount, const VkImageBlit* pRegions, VkTexFilter filter) const
{
	m_vk.cmdBlitImage(cmdBuffer, srcImage, srcImageLayout, destImage, destImageLayout, regionCount, pRegions, filter);
}

void DeviceDriver::cmdCopyBufferToImage (VkCmdBuffer cmdBuffer, VkBuffer srcBuffer, VkImage destImage, VkImageLayout destImageLayout, deUint32 regionCount, const VkBufferImageCopy* pRegions) const
{
	m_vk.cmdCopyBufferToImage(cmdBuffer, srcBuffer, destImage, destImageLayout, regionCount, pRegions);
}

void DeviceDriver::cmdCopyImageToBuffer (VkCmdBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkBuffer destBuffer, deUint32 regionCount, const VkBufferImageCopy* pRegions) const
{
	m_vk.cmdCopyImageToBuffer(cmdBuffer, srcImage, srcImageLayout, destBuffer, regionCount, pRegions);
}

void DeviceDriver::cmdUpdateBuffer (VkCmdBuffer cmdBuffer, VkBuffer destBuffer, VkDeviceSize destOffset, VkDeviceSize dataSize, const deUint32* pData) const
{
	m_vk.cmdUpdateBuffer(cmdBuffer, destBuffer, destOffset, dataSize, pData);
}

void DeviceDriver::cmdFillBuffer (VkCmdBuffer cmdBuffer, VkBuffer destBuffer, VkDeviceSize destOffset, VkDeviceSize fillSize, deUint32 data) const
{
	m_vk.cmdFillBuffer(cmdBuffer, destBuffer, destOffset, fillSize, data);
}

void DeviceDriver::cmdClearColorImage (VkCmdBuffer cmdBuffer, VkImage image, VkImageLayout imageLayout, const VkClearColorValue* pColor, deUint32 rangeCount, const VkImageSubresourceRange* pRanges) const
{
	m_vk.cmdClearColorImage(cmdBuffer, image, imageLayout, pColor, rangeCount, pRanges);
}

void DeviceDriver::cmdClearDepthStencilImage (VkCmdBuffer cmdBuffer, VkImage image, VkImageLayout imageLayout, float depth, deUint32 stencil, deUint32 rangeCount, const VkImageSubresourceRange* pRanges) const
{
	m_vk.cmdClearDepthStencilImage(cmdBuffer, image, imageLayout, depth, stencil, rangeCount, pRanges);
}

void DeviceDriver::cmdClearColorAttachment (VkCmdBuffer cmdBuffer, deUint32 colorAttachment, VkImageLayout imageLayout, const VkClearColorValue* pColor, deUint32 rectCount, const VkRect3D* pRects) const
{
	m_vk.cmdClearColorAttachment(cmdBuffer, colorAttachment, imageLayout, pColor, rectCount, pRects);
}

void DeviceDriver::cmdClearDepthStencilAttachment (VkCmdBuffer cmdBuffer, VkImageAspectFlags imageAspectMask, VkImageLayout imageLayout, float depth, deUint32 stencil, deUint32 rectCount, const VkRect3D* pRects) const
{
	m_vk.cmdClearDepthStencilAttachment(cmdBuffer, imageAspectMask, imageLayout, depth, stencil, rectCount, pRects);
}

void DeviceDriver::cmdResolveImage (VkCmdBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage destImage, VkImageLayout destImageLayout, deUint32 regionCount, const VkImageResolve* pRegions) const
{
	m_vk.cmdResolveImage(cmdBuffer, srcImage, srcImageLayout, destImage, destImageLayout, regionCount, pRegions);
}

void DeviceDriver::cmdSetEvent (VkCmdBuffer cmdBuffer, VkEvent event, VkPipelineStageFlags stageMask) const
{
	m_vk.cmdSetEvent(cmdBuffer, event, stageMask);
}

void DeviceDriver::cmdResetEvent (VkCmdBuffer cmdBuffer, VkEvent event, VkPipelineStageFlags stageMask) const
{
	m_vk.cmdResetEvent(cmdBuffer, event, stageMask);
}

void DeviceDriver::cmdWaitEvents (VkCmdBuffer cmdBuffer, deUint32 eventCount, const VkEvent* pEvents, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags destStageMask, deUint32 memBarrierCount, const void* const* ppMemBarriers) const
{
	m_vk.cmdWaitEvents(cmdBuffer, eventCount, pEvents, srcStageMask, destStageMask, memBarrierCount, ppMemBarriers);
}

void DeviceDriver::cmdPipelineBarrier (VkCmdBuffer cmdBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags destStageMask, VkBool32 byRegion, deUint32 memBarrierCount, const void* const* ppMemBarriers) const
{
	m_vk.cmdPipelineBarrier(cmdBuffer, srcStageMask, destStageMask, byRegion, memBarrierCount, ppMemBarriers);
}

void DeviceDriver::cmdBeginQuery (VkCmdBuffer cmdBuffer, VkQueryPool queryPool, deUint32 slot, VkQueryControlFlags flags) const
{
	m_vk.cmdBeginQuery(cmdBuffer, queryPool, slot, flags);
}

void DeviceDriver::cmdEndQuery (VkCmdBuffer cmdBuffer, VkQueryPool queryPool, deUint32 slot) const
{
	m_vk.cmdEndQuery(cmdBuffer, queryPool, slot);
}

void DeviceDriver::cmdResetQueryPool (VkCmdBuffer cmdBuffer, VkQueryPool queryPool, deUint32 startQuery, deUint32 queryCount) const
{
	m_vk.cmdResetQueryPool(cmdBuffer, queryPool, startQuery, queryCount);
}

void DeviceDriver::cmdWriteTimestamp (VkCmdBuffer cmdBuffer, VkTimestampType timestampType, VkBuffer destBuffer, VkDeviceSize destOffset) const
{
	m_vk.cmdWriteTimestamp(cmdBuffer, timestampType, destBuffer, destOffset);
}

void DeviceDriver::cmdCopyQueryPoolResults (VkCmdBuffer cmdBuffer, VkQueryPool queryPool, deUint32 startQuery, deUint32 queryCount, VkBuffer destBuffer, VkDeviceSize destOffset, VkDeviceSize destStride, VkQueryResultFlags flags) const
{
	m_vk.cmdCopyQueryPoolResults(cmdBuffer, queryPool, startQuery, queryCount, destBuffer, destOffset, destStride, flags);
}

void DeviceDriver::cmdPushConstants (VkCmdBuffer cmdBuffer, VkPipelineLayout layout, VkShaderStageFlags stageFlags, deUint32 start, deUint32 length, const void* values) const
{
	m_vk.cmdPushConstants(cmdBuffer, layout, stageFlags, start, length, values);
}

void DeviceDriver::cmdBeginRenderPass (VkCmdBuffer cmdBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, VkRenderPassContents contents) const
{
	m_vk.cmdBeginRenderPass(cmdBuffer, pRenderPassBegin, contents);
}

void DeviceDriver::cmdNextSubpass (VkCmdBuffer cmdBuffer, VkRenderPassContents contents) const
{
	m_vk.cmdNextSubpass(cmdBuffer, contents);
}

void DeviceDriver::cmdEndRenderPass (VkCmdBuffer cmdBuffer) const
{
	m_vk.cmdEndRenderPass(cmdBuffer);
}

void DeviceDriver::cmdExecuteCommands (VkCmdBuffer cmdBuffer, deUint32 cmdBuffersCount, const VkCmdBuffer* pCmdBuffers) const
{
	m_vk.cmdExecuteCommands(cmdBuffer, cmdBuffersCount, pCmdBuffers);
}
