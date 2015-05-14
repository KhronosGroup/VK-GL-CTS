/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 */

VkResult DeviceDriver::getPhysicalDeviceInfo (VkPhysicalDevice physicalDevice, VkPhysicalDeviceInfoType infoType, deUintptr* pDataSize, void* pData) const
{
	return m_vk.getPhysicalDeviceInfo(physicalDevice, infoType, pDataSize, pData);
}

VkResult DeviceDriver::createDevice (VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, VkDevice* pDevice) const
{
	return m_vk.createDevice(physicalDevice, pCreateInfo, pDevice);
}

VkResult DeviceDriver::destroyDevice (VkDevice device) const
{
	return m_vk.destroyDevice(device);
}

VkResult DeviceDriver::getGlobalExtensionInfo (VkExtensionInfoType infoType, deUint32 extensionIndex, deUintptr* pDataSize, void* pData) const
{
	return m_vk.getGlobalExtensionInfo(infoType, extensionIndex, pDataSize, pData);
}

VkResult DeviceDriver::getPhysicalDeviceExtensionInfo (VkPhysicalDevice physicalDevice, VkExtensionInfoType infoType, deUint32 extensionIndex, deUintptr* pDataSize, void* pData) const
{
	return m_vk.getPhysicalDeviceExtensionInfo(physicalDevice, infoType, extensionIndex, pDataSize, pData);
}

VkResult DeviceDriver::enumerateLayers (VkPhysicalDevice physicalDevice, deUintptr maxStringSize, deUintptr* pLayerCount, char* const* pOutLayers, void* pReserved) const
{
	return m_vk.enumerateLayers(physicalDevice, maxStringSize, pLayerCount, pOutLayers, pReserved);
}

VkResult DeviceDriver::getDeviceQueue (VkDevice device, deUint32 queueNodeIndex, deUint32 queueIndex, VkQueue* pQueue) const
{
	return m_vk.getDeviceQueue(device, queueNodeIndex, queueIndex, pQueue);
}

VkResult DeviceDriver::queueSubmit (VkQueue queue, deUint32 cmdBufferCount, const VkCmdBuffer* pCmdBuffers, VkFence fence) const
{
	return m_vk.queueSubmit(queue, cmdBufferCount, pCmdBuffers, fence);
}

VkResult DeviceDriver::queueAddMemReferences (VkQueue queue, deUint32 count, const VkDeviceMemory* pMems) const
{
	return m_vk.queueAddMemReferences(queue, count, pMems);
}

VkResult DeviceDriver::queueRemoveMemReferences (VkQueue queue, deUint32 count, const VkDeviceMemory* pMems) const
{
	return m_vk.queueRemoveMemReferences(queue, count, pMems);
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

VkResult DeviceDriver::setMemoryPriority (VkDevice device, VkDeviceMemory mem, VkMemoryPriority priority) const
{
	return m_vk.setMemoryPriority(device, mem, priority);
}

VkResult DeviceDriver::mapMemory (VkDevice device, VkDeviceMemory mem, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** ppData) const
{
	return m_vk.mapMemory(device, mem, offset, size, flags, ppData);
}

VkResult DeviceDriver::unmapMemory (VkDevice device, VkDeviceMemory mem) const
{
	return m_vk.unmapMemory(device, mem);
}

VkResult DeviceDriver::flushMappedMemory (VkDevice device, VkDeviceMemory mem, VkDeviceSize offset, VkDeviceSize size) const
{
	return m_vk.flushMappedMemory(device, mem, offset, size);
}

VkResult DeviceDriver::pinSystemMemory (VkDevice device, const void* pSysMem, deUintptr memSize, VkDeviceMemory* pMem) const
{
	return m_vk.pinSystemMemory(device, pSysMem, memSize, pMem);
}

VkResult DeviceDriver::getMultiDeviceCompatibility (VkPhysicalDevice physicalDevice0, VkPhysicalDevice physicalDevice1, VkPhysicalDeviceCompatibilityInfo* pInfo) const
{
	return m_vk.getMultiDeviceCompatibility(physicalDevice0, physicalDevice1, pInfo);
}

VkResult DeviceDriver::openSharedMemory (VkDevice device, const VkMemoryOpenInfo* pOpenInfo, VkDeviceMemory* pMem) const
{
	return m_vk.openSharedMemory(device, pOpenInfo, pMem);
}

VkResult DeviceDriver::openSharedSemaphore (VkDevice device, const VkSemaphoreOpenInfo* pOpenInfo, VkSemaphore* pSemaphore) const
{
	return m_vk.openSharedSemaphore(device, pOpenInfo, pSemaphore);
}

VkResult DeviceDriver::openPeerMemory (VkDevice device, const VkPeerMemoryOpenInfo* pOpenInfo, VkDeviceMemory* pMem) const
{
	return m_vk.openPeerMemory(device, pOpenInfo, pMem);
}

VkResult DeviceDriver::openPeerImage (VkDevice device, const VkPeerImageOpenInfo* pOpenInfo, VkImage* pImage, VkDeviceMemory* pMem) const
{
	return m_vk.openPeerImage(device, pOpenInfo, pImage, pMem);
}

VkResult DeviceDriver::destroyObject (VkDevice device, VkObjectType objType, VkObject object) const
{
	return m_vk.destroyObject(device, objType, object);
}

VkResult DeviceDriver::getObjectInfo (VkDevice device, VkObjectType objType, VkObject object, VkObjectInfoType infoType, deUintptr* pDataSize, void* pData) const
{
	return m_vk.getObjectInfo(device, objType, object, infoType, pDataSize, pData);
}

VkResult DeviceDriver::queueBindObjectMemory (VkQueue queue, VkObjectType objType, VkObject object, deUint32 allocationIdx, VkDeviceMemory mem, VkDeviceSize memOffset) const
{
	return m_vk.queueBindObjectMemory(queue, objType, object, allocationIdx, mem, memOffset);
}

VkResult DeviceDriver::queueBindObjectMemoryRange (VkQueue queue, VkObjectType objType, VkObject object, deUint32 allocationIdx, VkDeviceSize rangeOffset, VkDeviceSize rangeSize, VkDeviceMemory mem, VkDeviceSize memOffset) const
{
	return m_vk.queueBindObjectMemoryRange(queue, objType, object, allocationIdx, rangeOffset, rangeSize, mem, memOffset);
}

VkResult DeviceDriver::queueBindImageMemoryRange (VkQueue queue, VkImage image, deUint32 allocationIdx, const VkImageMemoryBindInfo* pBindInfo, VkDeviceMemory mem, VkDeviceSize memOffset) const
{
	return m_vk.queueBindImageMemoryRange(queue, image, allocationIdx, pBindInfo, mem, memOffset);
}

VkResult DeviceDriver::createFence (VkDevice device, const VkFenceCreateInfo* pCreateInfo, VkFence* pFence) const
{
	return m_vk.createFence(device, pCreateInfo, pFence);
}

VkResult DeviceDriver::resetFences (VkDevice device, deUint32 fenceCount, VkFence* pFences) const
{
	return m_vk.resetFences(device, fenceCount, pFences);
}

VkResult DeviceDriver::getFenceStatus (VkDevice device, VkFence fence) const
{
	return m_vk.getFenceStatus(device, fence);
}

VkResult DeviceDriver::waitForFences (VkDevice device, deUint32 fenceCount, const VkFence* pFences, deUint32 waitAll, deUint64 timeout) const
{
	return m_vk.waitForFences(device, fenceCount, pFences, waitAll, timeout);
}

VkResult DeviceDriver::createSemaphore (VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo, VkSemaphore* pSemaphore) const
{
	return m_vk.createSemaphore(device, pCreateInfo, pSemaphore);
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

VkResult DeviceDriver::getQueryPoolResults (VkDevice device, VkQueryPool queryPool, deUint32 startQuery, deUint32 queryCount, deUintptr* pDataSize, void* pData, VkQueryResultFlags flags) const
{
	return m_vk.getQueryPoolResults(device, queryPool, startQuery, queryCount, pDataSize, pData, flags);
}

VkResult DeviceDriver::getFormatInfo (VkDevice device, VkFormat format, VkFormatInfoType infoType, deUintptr* pDataSize, void* pData) const
{
	return m_vk.getFormatInfo(device, format, infoType, pDataSize, pData);
}

VkResult DeviceDriver::createBuffer (VkDevice device, const VkBufferCreateInfo* pCreateInfo, VkBuffer* pBuffer) const
{
	return m_vk.createBuffer(device, pCreateInfo, pBuffer);
}

VkResult DeviceDriver::createBufferView (VkDevice device, const VkBufferViewCreateInfo* pCreateInfo, VkBufferView* pView) const
{
	return m_vk.createBufferView(device, pCreateInfo, pView);
}

VkResult DeviceDriver::createImage (VkDevice device, const VkImageCreateInfo* pCreateInfo, VkImage* pImage) const
{
	return m_vk.createImage(device, pCreateInfo, pImage);
}

VkResult DeviceDriver::getImageSubresourceInfo (VkDevice device, VkImage image, const VkImageSubresource* pSubresource, VkSubresourceInfoType infoType, deUintptr* pDataSize, void* pData) const
{
	return m_vk.getImageSubresourceInfo(device, image, pSubresource, infoType, pDataSize, pData);
}

VkResult DeviceDriver::createImageView (VkDevice device, const VkImageViewCreateInfo* pCreateInfo, VkImageView* pView) const
{
	return m_vk.createImageView(device, pCreateInfo, pView);
}

VkResult DeviceDriver::createColorAttachmentView (VkDevice device, const VkColorAttachmentViewCreateInfo* pCreateInfo, VkColorAttachmentView* pView) const
{
	return m_vk.createColorAttachmentView(device, pCreateInfo, pView);
}

VkResult DeviceDriver::createDepthStencilView (VkDevice device, const VkDepthStencilViewCreateInfo* pCreateInfo, VkDepthStencilView* pView) const
{
	return m_vk.createDepthStencilView(device, pCreateInfo, pView);
}

VkResult DeviceDriver::createShader (VkDevice device, const VkShaderCreateInfo* pCreateInfo, VkShader* pShader) const
{
	return m_vk.createShader(device, pCreateInfo, pShader);
}

VkResult DeviceDriver::createGraphicsPipeline (VkDevice device, const VkGraphicsPipelineCreateInfo* pCreateInfo, VkPipeline* pPipeline) const
{
	return m_vk.createGraphicsPipeline(device, pCreateInfo, pPipeline);
}

VkResult DeviceDriver::createGraphicsPipelineDerivative (VkDevice device, const VkGraphicsPipelineCreateInfo* pCreateInfo, VkPipeline basePipeline, VkPipeline* pPipeline) const
{
	return m_vk.createGraphicsPipelineDerivative(device, pCreateInfo, basePipeline, pPipeline);
}

VkResult DeviceDriver::createComputePipeline (VkDevice device, const VkComputePipelineCreateInfo* pCreateInfo, VkPipeline* pPipeline) const
{
	return m_vk.createComputePipeline(device, pCreateInfo, pPipeline);
}

VkResult DeviceDriver::storePipeline (VkDevice device, VkPipeline pipeline, deUintptr* pDataSize, void* pData) const
{
	return m_vk.storePipeline(device, pipeline, pDataSize, pData);
}

VkResult DeviceDriver::loadPipeline (VkDevice device, deUintptr dataSize, const void* pData, VkPipeline* pPipeline) const
{
	return m_vk.loadPipeline(device, dataSize, pData, pPipeline);
}

VkResult DeviceDriver::loadPipelineDerivative (VkDevice device, deUintptr dataSize, const void* pData, VkPipeline basePipeline, VkPipeline* pPipeline) const
{
	return m_vk.loadPipelineDerivative(device, dataSize, pData, basePipeline, pPipeline);
}

VkResult DeviceDriver::createPipelineLayout (VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo, VkPipelineLayout* pPipelineLayout) const
{
	return m_vk.createPipelineLayout(device, pCreateInfo, pPipelineLayout);
}

VkResult DeviceDriver::createSampler (VkDevice device, const VkSamplerCreateInfo* pCreateInfo, VkSampler* pSampler) const
{
	return m_vk.createSampler(device, pCreateInfo, pSampler);
}

VkResult DeviceDriver::createDescriptorSetLayout (VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, VkDescriptorSetLayout* pSetLayout) const
{
	return m_vk.createDescriptorSetLayout(device, pCreateInfo, pSetLayout);
}

VkResult DeviceDriver::beginDescriptorPoolUpdate (VkDevice device, VkDescriptorUpdateMode updateMode) const
{
	return m_vk.beginDescriptorPoolUpdate(device, updateMode);
}

VkResult DeviceDriver::endDescriptorPoolUpdate (VkDevice device, VkCmdBuffer cmd) const
{
	return m_vk.endDescriptorPoolUpdate(device, cmd);
}

VkResult DeviceDriver::createDescriptorPool (VkDevice device, VkDescriptorPoolUsage poolUsage, deUint32 maxSets, const VkDescriptorPoolCreateInfo* pCreateInfo, VkDescriptorPool* pDescriptorPool) const
{
	return m_vk.createDescriptorPool(device, poolUsage, maxSets, pCreateInfo, pDescriptorPool);
}

VkResult DeviceDriver::resetDescriptorPool (VkDevice device, VkDescriptorPool descriptorPool) const
{
	return m_vk.resetDescriptorPool(device, descriptorPool);
}

VkResult DeviceDriver::allocDescriptorSets (VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorSetUsage setUsage, deUint32 count, const VkDescriptorSetLayout* pSetLayouts, VkDescriptorSet* pDescriptorSets, deUint32* pCount) const
{
	return m_vk.allocDescriptorSets(device, descriptorPool, setUsage, count, pSetLayouts, pDescriptorSets, pCount);
}

void DeviceDriver::clearDescriptorSets (VkDevice device, VkDescriptorPool descriptorPool, deUint32 count, const VkDescriptorSet* pDescriptorSets) const
{
	m_vk.clearDescriptorSets(device, descriptorPool, count, pDescriptorSets);
}

void DeviceDriver::updateDescriptors (VkDevice device, VkDescriptorSet descriptorSet, deUint32 updateCount, const void** ppUpdateArray) const
{
	m_vk.updateDescriptors(device, descriptorSet, updateCount, ppUpdateArray);
}

VkResult DeviceDriver::createDynamicViewportState (VkDevice device, const VkDynamicVpStateCreateInfo* pCreateInfo, VkDynamicVpState* pState) const
{
	return m_vk.createDynamicViewportState(device, pCreateInfo, pState);
}

VkResult DeviceDriver::createDynamicRasterState (VkDevice device, const VkDynamicRsStateCreateInfo* pCreateInfo, VkDynamicRsState* pState) const
{
	return m_vk.createDynamicRasterState(device, pCreateInfo, pState);
}

VkResult DeviceDriver::createDynamicColorBlendState (VkDevice device, const VkDynamicCbStateCreateInfo* pCreateInfo, VkDynamicCbState* pState) const
{
	return m_vk.createDynamicColorBlendState(device, pCreateInfo, pState);
}

VkResult DeviceDriver::createDynamicDepthStencilState (VkDevice device, const VkDynamicDsStateCreateInfo* pCreateInfo, VkDynamicDsState* pState) const
{
	return m_vk.createDynamicDepthStencilState(device, pCreateInfo, pState);
}

VkResult DeviceDriver::createCommandBuffer (VkDevice device, const VkCmdBufferCreateInfo* pCreateInfo, VkCmdBuffer* pCmdBuffer) const
{
	return m_vk.createCommandBuffer(device, pCreateInfo, pCmdBuffer);
}

VkResult DeviceDriver::beginCommandBuffer (VkCmdBuffer cmdBuffer, const VkCmdBufferBeginInfo* pBeginInfo) const
{
	return m_vk.beginCommandBuffer(cmdBuffer, pBeginInfo);
}

VkResult DeviceDriver::endCommandBuffer (VkCmdBuffer cmdBuffer) const
{
	return m_vk.endCommandBuffer(cmdBuffer);
}

VkResult DeviceDriver::resetCommandBuffer (VkCmdBuffer cmdBuffer) const
{
	return m_vk.resetCommandBuffer(cmdBuffer);
}

void DeviceDriver::cmdBindPipeline (VkCmdBuffer cmdBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline) const
{
	m_vk.cmdBindPipeline(cmdBuffer, pipelineBindPoint, pipeline);
}

void DeviceDriver::cmdBindDynamicStateObject (VkCmdBuffer cmdBuffer, VkStateBindPoint stateBindPoint, VkDynamicStateObject dynamicState) const
{
	m_vk.cmdBindDynamicStateObject(cmdBuffer, stateBindPoint, dynamicState);
}

void DeviceDriver::cmdBindDescriptorSets (VkCmdBuffer cmdBuffer, VkPipelineBindPoint pipelineBindPoint, deUint32 firstSet, deUint32 setCount, const VkDescriptorSet* pDescriptorSets, deUint32 dynamicOffsetCount, const deUint32* pDynamicOffsets) const
{
	m_vk.cmdBindDescriptorSets(cmdBuffer, pipelineBindPoint, firstSet, setCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
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

void DeviceDriver::cmdBlitImage (VkCmdBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage destImage, VkImageLayout destImageLayout, deUint32 regionCount, const VkImageBlit* pRegions) const
{
	m_vk.cmdBlitImage(cmdBuffer, srcImage, srcImageLayout, destImage, destImageLayout, regionCount, pRegions);
}

void DeviceDriver::cmdCopyBufferToImage (VkCmdBuffer cmdBuffer, VkBuffer srcBuffer, VkImage destImage, VkImageLayout destImageLayout, deUint32 regionCount, const VkBufferImageCopy* pRegions) const
{
	m_vk.cmdCopyBufferToImage(cmdBuffer, srcBuffer, destImage, destImageLayout, regionCount, pRegions);
}

void DeviceDriver::cmdCopyImageToBuffer (VkCmdBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkBuffer destBuffer, deUint32 regionCount, const VkBufferImageCopy* pRegions) const
{
	m_vk.cmdCopyImageToBuffer(cmdBuffer, srcImage, srcImageLayout, destBuffer, regionCount, pRegions);
}

void DeviceDriver::cmdCloneImageData (VkCmdBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage destImage, VkImageLayout destImageLayout) const
{
	m_vk.cmdCloneImageData(cmdBuffer, srcImage, srcImageLayout, destImage, destImageLayout);
}

void DeviceDriver::cmdUpdateBuffer (VkCmdBuffer cmdBuffer, VkBuffer destBuffer, VkDeviceSize destOffset, VkDeviceSize dataSize, const deUint32* pData) const
{
	m_vk.cmdUpdateBuffer(cmdBuffer, destBuffer, destOffset, dataSize, pData);
}

void DeviceDriver::cmdFillBuffer (VkCmdBuffer cmdBuffer, VkBuffer destBuffer, VkDeviceSize destOffset, VkDeviceSize fillSize, deUint32 data) const
{
	m_vk.cmdFillBuffer(cmdBuffer, destBuffer, destOffset, fillSize, data);
}

void DeviceDriver::cmdClearColorImage (VkCmdBuffer cmdBuffer, VkImage image, VkImageLayout imageLayout, VkClearColor color, deUint32 rangeCount, const VkImageSubresourceRange* pRanges) const
{
	m_vk.cmdClearColorImage(cmdBuffer, image, imageLayout, color, rangeCount, pRanges);
}

void DeviceDriver::cmdClearDepthStencil (VkCmdBuffer cmdBuffer, VkImage image, VkImageLayout imageLayout, float depth, deUint32 stencil, deUint32 rangeCount, const VkImageSubresourceRange* pRanges) const
{
	m_vk.cmdClearDepthStencil(cmdBuffer, image, imageLayout, depth, stencil, rangeCount, pRanges);
}

void DeviceDriver::cmdResolveImage (VkCmdBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage destImage, VkImageLayout destImageLayout, deUint32 regionCount, const VkImageResolve* pRegions) const
{
	m_vk.cmdResolveImage(cmdBuffer, srcImage, srcImageLayout, destImage, destImageLayout, regionCount, pRegions);
}

void DeviceDriver::cmdSetEvent (VkCmdBuffer cmdBuffer, VkEvent event, VkPipeEvent pipeEvent) const
{
	m_vk.cmdSetEvent(cmdBuffer, event, pipeEvent);
}

void DeviceDriver::cmdResetEvent (VkCmdBuffer cmdBuffer, VkEvent event, VkPipeEvent pipeEvent) const
{
	m_vk.cmdResetEvent(cmdBuffer, event, pipeEvent);
}

void DeviceDriver::cmdWaitEvents (VkCmdBuffer cmdBuffer, VkWaitEvent waitEvent, deUint32 eventCount, const VkEvent* pEvents, deUint32 memBarrierCount, const void** ppMemBarriers) const
{
	m_vk.cmdWaitEvents(cmdBuffer, waitEvent, eventCount, pEvents, memBarrierCount, ppMemBarriers);
}

void DeviceDriver::cmdPipelineBarrier (VkCmdBuffer cmdBuffer, VkWaitEvent waitEvent, deUint32 pipeEventCount, const VkPipeEvent* pPipeEvents, deUint32 memBarrierCount, const void** ppMemBarriers) const
{
	m_vk.cmdPipelineBarrier(cmdBuffer, waitEvent, pipeEventCount, pPipeEvents, memBarrierCount, ppMemBarriers);
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

void DeviceDriver::cmdInitAtomicCounters (VkCmdBuffer cmdBuffer, VkPipelineBindPoint pipelineBindPoint, deUint32 startCounter, deUint32 counterCount, const deUint32* pData) const
{
	m_vk.cmdInitAtomicCounters(cmdBuffer, pipelineBindPoint, startCounter, counterCount, pData);
}

void DeviceDriver::cmdLoadAtomicCounters (VkCmdBuffer cmdBuffer, VkPipelineBindPoint pipelineBindPoint, deUint32 startCounter, deUint32 counterCount, VkBuffer srcBuffer, VkDeviceSize srcOffset) const
{
	m_vk.cmdLoadAtomicCounters(cmdBuffer, pipelineBindPoint, startCounter, counterCount, srcBuffer, srcOffset);
}

void DeviceDriver::cmdSaveAtomicCounters (VkCmdBuffer cmdBuffer, VkPipelineBindPoint pipelineBindPoint, deUint32 startCounter, deUint32 counterCount, VkBuffer destBuffer, VkDeviceSize destOffset) const
{
	m_vk.cmdSaveAtomicCounters(cmdBuffer, pipelineBindPoint, startCounter, counterCount, destBuffer, destOffset);
}

VkResult DeviceDriver::createFramebuffer (VkDevice device, const VkFramebufferCreateInfo* pCreateInfo, VkFramebuffer* pFramebuffer) const
{
	return m_vk.createFramebuffer(device, pCreateInfo, pFramebuffer);
}

VkResult DeviceDriver::createRenderPass (VkDevice device, const VkRenderPassCreateInfo* pCreateInfo, VkRenderPass* pRenderPass) const
{
	return m_vk.createRenderPass(device, pCreateInfo, pRenderPass);
}

void DeviceDriver::cmdBeginRenderPass (VkCmdBuffer cmdBuffer, const VkRenderPassBegin* pRenderPassBegin) const
{
	m_vk.cmdBeginRenderPass(cmdBuffer, pRenderPassBegin);
}

void DeviceDriver::cmdEndRenderPass (VkCmdBuffer cmdBuffer, VkRenderPass renderPass) const
{
	m_vk.cmdEndRenderPass(cmdBuffer, renderPass);
}
