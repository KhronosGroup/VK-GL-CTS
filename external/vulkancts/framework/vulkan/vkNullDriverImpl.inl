/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 */
VkResult createInstance (const VkInstanceCreateInfo* pCreateInfo, VkInstance* pInstance)
{
	VK_NULL_RETURN(*pInstance = reinterpret_cast<VkInstance>(new Instance(pCreateInfo)));
}

VkResult createDevice (VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, VkDevice* pDevice)
{
	VK_NULL_RETURN(*pDevice = reinterpret_cast<VkDevice>(new Device(physicalDevice, pCreateInfo)));
}

VkResult allocMemory (VkDevice device, const VkMemoryAllocInfo* pAllocInfo, VkDeviceMemory* pMem)
{
	VK_NULL_RETURN(*pMem = VkDeviceMemory((deUint64)(deUintptr)new DeviceMemory(device, pAllocInfo)));
}

VkResult createFence (VkDevice device, const VkFenceCreateInfo* pCreateInfo, VkFence* pFence)
{
	VK_NULL_RETURN(*pFence = VkFence((deUint64)(deUintptr)new Fence(device, pCreateInfo)));
}

VkResult createSemaphore (VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo, VkSemaphore* pSemaphore)
{
	VK_NULL_RETURN(*pSemaphore = VkSemaphore((deUint64)(deUintptr)new Semaphore(device, pCreateInfo)));
}

VkResult createEvent (VkDevice device, const VkEventCreateInfo* pCreateInfo, VkEvent* pEvent)
{
	VK_NULL_RETURN(*pEvent = VkEvent((deUint64)(deUintptr)new Event(device, pCreateInfo)));
}

VkResult createQueryPool (VkDevice device, const VkQueryPoolCreateInfo* pCreateInfo, VkQueryPool* pQueryPool)
{
	VK_NULL_RETURN(*pQueryPool = VkQueryPool((deUint64)(deUintptr)new QueryPool(device, pCreateInfo)));
}

VkResult createBuffer (VkDevice device, const VkBufferCreateInfo* pCreateInfo, VkBuffer* pBuffer)
{
	VK_NULL_RETURN(*pBuffer = VkBuffer((deUint64)(deUintptr)new Buffer(device, pCreateInfo)));
}

VkResult createBufferView (VkDevice device, const VkBufferViewCreateInfo* pCreateInfo, VkBufferView* pView)
{
	VK_NULL_RETURN(*pView = VkBufferView((deUint64)(deUintptr)new BufferView(device, pCreateInfo)));
}

VkResult createImage (VkDevice device, const VkImageCreateInfo* pCreateInfo, VkImage* pImage)
{
	VK_NULL_RETURN(*pImage = VkImage((deUint64)(deUintptr)new Image(device, pCreateInfo)));
}

VkResult createImageView (VkDevice device, const VkImageViewCreateInfo* pCreateInfo, VkImageView* pView)
{
	VK_NULL_RETURN(*pView = VkImageView((deUint64)(deUintptr)new ImageView(device, pCreateInfo)));
}

VkResult createShaderModule (VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, VkShaderModule* pShaderModule)
{
	VK_NULL_RETURN(*pShaderModule = VkShaderModule((deUint64)(deUintptr)new ShaderModule(device, pCreateInfo)));
}

VkResult createShader (VkDevice device, const VkShaderCreateInfo* pCreateInfo, VkShader* pShader)
{
	VK_NULL_RETURN(*pShader = VkShader((deUint64)(deUintptr)new Shader(device, pCreateInfo)));
}

VkResult createPipelineCache (VkDevice device, const VkPipelineCacheCreateInfo* pCreateInfo, VkPipelineCache* pPipelineCache)
{
	VK_NULL_RETURN(*pPipelineCache = VkPipelineCache((deUint64)(deUintptr)new PipelineCache(device, pCreateInfo)));
}

VkResult createPipelineLayout (VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo, VkPipelineLayout* pPipelineLayout)
{
	VK_NULL_RETURN(*pPipelineLayout = VkPipelineLayout((deUint64)(deUintptr)new PipelineLayout(device, pCreateInfo)));
}

VkResult createSampler (VkDevice device, const VkSamplerCreateInfo* pCreateInfo, VkSampler* pSampler)
{
	VK_NULL_RETURN(*pSampler = VkSampler((deUint64)(deUintptr)new Sampler(device, pCreateInfo)));
}

VkResult createDescriptorSetLayout (VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, VkDescriptorSetLayout* pSetLayout)
{
	VK_NULL_RETURN(*pSetLayout = VkDescriptorSetLayout((deUint64)(deUintptr)new DescriptorSetLayout(device, pCreateInfo)));
}

VkResult createDescriptorPool (VkDevice device, const VkDescriptorPoolCreateInfo* pCreateInfo, VkDescriptorPool* pDescriptorPool)
{
	VK_NULL_RETURN(*pDescriptorPool = VkDescriptorPool((deUint64)(deUintptr)new DescriptorPool(device, pCreateInfo)));
}

VkResult createFramebuffer (VkDevice device, const VkFramebufferCreateInfo* pCreateInfo, VkFramebuffer* pFramebuffer)
{
	VK_NULL_RETURN(*pFramebuffer = VkFramebuffer((deUint64)(deUintptr)new Framebuffer(device, pCreateInfo)));
}

VkResult createRenderPass (VkDevice device, const VkRenderPassCreateInfo* pCreateInfo, VkRenderPass* pRenderPass)
{
	VK_NULL_RETURN(*pRenderPass = VkRenderPass((deUint64)(deUintptr)new RenderPass(device, pCreateInfo)));
}

VkResult createCommandPool (VkDevice device, const VkCmdPoolCreateInfo* pCreateInfo, VkCmdPool* pCmdPool)
{
	VK_NULL_RETURN(*pCmdPool = VkCmdPool((deUint64)(deUintptr)new CmdPool(device, pCreateInfo)));
}

VkResult createCommandBuffer (VkDevice device, const VkCmdBufferCreateInfo* pCreateInfo, VkCmdBuffer* pCmdBuffer)
{
	VK_NULL_RETURN(*pCmdBuffer = reinterpret_cast<VkCmdBuffer>(new CmdBuffer(device, pCreateInfo)));
}

void destroyInstance (VkInstance instance)
{
	delete reinterpret_cast<Instance*>(instance);
}

void destroyDevice (VkDevice device)
{
	delete reinterpret_cast<Device*>(device);
}

void freeMemory (VkDevice device, VkDeviceMemory mem)
{
	DE_UNREF(device);
	delete reinterpret_cast<DeviceMemory*>((deUintptr)mem.getInternal());
}

void destroyFence (VkDevice device, VkFence fence)
{
	DE_UNREF(device);
	delete reinterpret_cast<Fence*>((deUintptr)fence.getInternal());
}

void destroySemaphore (VkDevice device, VkSemaphore semaphore)
{
	DE_UNREF(device);
	delete reinterpret_cast<Semaphore*>((deUintptr)semaphore.getInternal());
}

void destroyEvent (VkDevice device, VkEvent event)
{
	DE_UNREF(device);
	delete reinterpret_cast<Event*>((deUintptr)event.getInternal());
}

void destroyQueryPool (VkDevice device, VkQueryPool queryPool)
{
	DE_UNREF(device);
	delete reinterpret_cast<QueryPool*>((deUintptr)queryPool.getInternal());
}

void destroyBuffer (VkDevice device, VkBuffer buffer)
{
	DE_UNREF(device);
	delete reinterpret_cast<Buffer*>((deUintptr)buffer.getInternal());
}

void destroyBufferView (VkDevice device, VkBufferView bufferView)
{
	DE_UNREF(device);
	delete reinterpret_cast<BufferView*>((deUintptr)bufferView.getInternal());
}

void destroyImage (VkDevice device, VkImage image)
{
	DE_UNREF(device);
	delete reinterpret_cast<Image*>((deUintptr)image.getInternal());
}

void destroyImageView (VkDevice device, VkImageView imageView)
{
	DE_UNREF(device);
	delete reinterpret_cast<ImageView*>((deUintptr)imageView.getInternal());
}

void destroyShaderModule (VkDevice device, VkShaderModule shaderModule)
{
	DE_UNREF(device);
	delete reinterpret_cast<ShaderModule*>((deUintptr)shaderModule.getInternal());
}

void destroyShader (VkDevice device, VkShader shader)
{
	DE_UNREF(device);
	delete reinterpret_cast<Shader*>((deUintptr)shader.getInternal());
}

void destroyPipelineCache (VkDevice device, VkPipelineCache pipelineCache)
{
	DE_UNREF(device);
	delete reinterpret_cast<PipelineCache*>((deUintptr)pipelineCache.getInternal());
}

void destroyPipeline (VkDevice device, VkPipeline pipeline)
{
	DE_UNREF(device);
	delete reinterpret_cast<Pipeline*>((deUintptr)pipeline.getInternal());
}

void destroyPipelineLayout (VkDevice device, VkPipelineLayout pipelineLayout)
{
	DE_UNREF(device);
	delete reinterpret_cast<PipelineLayout*>((deUintptr)pipelineLayout.getInternal());
}

void destroySampler (VkDevice device, VkSampler sampler)
{
	DE_UNREF(device);
	delete reinterpret_cast<Sampler*>((deUintptr)sampler.getInternal());
}

void destroyDescriptorSetLayout (VkDevice device, VkDescriptorSetLayout descriptorSetLayout)
{
	DE_UNREF(device);
	delete reinterpret_cast<DescriptorSetLayout*>((deUintptr)descriptorSetLayout.getInternal());
}

void destroyDescriptorPool (VkDevice device, VkDescriptorPool descriptorPool)
{
	DE_UNREF(device);
	delete reinterpret_cast<DescriptorPool*>((deUintptr)descriptorPool.getInternal());
}

void destroyFramebuffer (VkDevice device, VkFramebuffer framebuffer)
{
	DE_UNREF(device);
	delete reinterpret_cast<Framebuffer*>((deUintptr)framebuffer.getInternal());
}

void destroyRenderPass (VkDevice device, VkRenderPass renderPass)
{
	DE_UNREF(device);
	delete reinterpret_cast<RenderPass*>((deUintptr)renderPass.getInternal());
}

void destroyCommandPool (VkDevice device, VkCmdPool cmdPool)
{
	DE_UNREF(device);
	delete reinterpret_cast<CmdPool*>((deUintptr)cmdPool.getInternal());
}

void destroyCommandBuffer (VkDevice device, VkCmdBuffer commandBuffer)
{
	DE_UNREF(device);
	delete reinterpret_cast<CmdBuffer*>(commandBuffer);
}

VkResult getPhysicalDeviceFeatures (VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures* pFeatures)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(pFeatures);
	return VK_SUCCESS;
}

VkResult getPhysicalDeviceFormatProperties (VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties* pFormatProperties)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(format);
	DE_UNREF(pFormatProperties);
	return VK_SUCCESS;
}

VkResult getPhysicalDeviceImageFormatProperties (VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties* pImageFormatProperties)
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

VkResult enumerateInstanceExtensionProperties (const char* pLayerName, deUint32* pCount, VkExtensionProperties* pProperties)
{
	DE_UNREF(pLayerName);
	DE_UNREF(pCount);
	DE_UNREF(pProperties);
	return VK_SUCCESS;
}

VkResult enumerateDeviceExtensionProperties (VkPhysicalDevice physicalDevice, const char* pLayerName, deUint32* pCount, VkExtensionProperties* pProperties)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(pLayerName);
	DE_UNREF(pCount);
	DE_UNREF(pProperties);
	return VK_SUCCESS;
}

VkResult enumerateInstanceLayerProperties (deUint32* pCount, VkLayerProperties* pProperties)
{
	DE_UNREF(pCount);
	DE_UNREF(pProperties);
	return VK_SUCCESS;
}

VkResult enumerateDeviceLayerProperties (VkPhysicalDevice physicalDevice, deUint32* pCount, VkLayerProperties* pProperties)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(pCount);
	DE_UNREF(pProperties);
	return VK_SUCCESS;
}

VkResult getDeviceQueue (VkDevice device, deUint32 queueFamilyIndex, deUint32 queueIndex, VkQueue* pQueue)
{
	DE_UNREF(device);
	DE_UNREF(queueFamilyIndex);
	DE_UNREF(queueIndex);
	DE_UNREF(pQueue);
	return VK_SUCCESS;
}

VkResult queueSubmit (VkQueue queue, deUint32 cmdBufferCount, const VkCmdBuffer* pCmdBuffers, VkFence fence)
{
	DE_UNREF(queue);
	DE_UNREF(cmdBufferCount);
	DE_UNREF(pCmdBuffers);
	DE_UNREF(fence);
	return VK_SUCCESS;
}

VkResult queueWaitIdle (VkQueue queue)
{
	DE_UNREF(queue);
	return VK_SUCCESS;
}

VkResult deviceWaitIdle (VkDevice device)
{
	DE_UNREF(device);
	return VK_SUCCESS;
}

void unmapMemory (VkDevice device, VkDeviceMemory mem)
{
	DE_UNREF(device);
	DE_UNREF(mem);
}

VkResult flushMappedMemoryRanges (VkDevice device, deUint32 memRangeCount, const VkMappedMemoryRange* pMemRanges)
{
	DE_UNREF(device);
	DE_UNREF(memRangeCount);
	DE_UNREF(pMemRanges);
	return VK_SUCCESS;
}

VkResult invalidateMappedMemoryRanges (VkDevice device, deUint32 memRangeCount, const VkMappedMemoryRange* pMemRanges)
{
	DE_UNREF(device);
	DE_UNREF(memRangeCount);
	DE_UNREF(pMemRanges);
	return VK_SUCCESS;
}

VkResult getDeviceMemoryCommitment (VkDevice device, VkDeviceMemory memory, VkDeviceSize* pCommittedMemoryInBytes)
{
	DE_UNREF(device);
	DE_UNREF(memory);
	DE_UNREF(pCommittedMemoryInBytes);
	return VK_SUCCESS;
}

VkResult bindBufferMemory (VkDevice device, VkBuffer buffer, VkDeviceMemory mem, VkDeviceSize memOffset)
{
	DE_UNREF(device);
	DE_UNREF(buffer);
	DE_UNREF(mem);
	DE_UNREF(memOffset);
	return VK_SUCCESS;
}

VkResult bindImageMemory (VkDevice device, VkImage image, VkDeviceMemory mem, VkDeviceSize memOffset)
{
	DE_UNREF(device);
	DE_UNREF(image);
	DE_UNREF(mem);
	DE_UNREF(memOffset);
	return VK_SUCCESS;
}

VkResult getImageSparseMemoryRequirements (VkDevice device, VkImage image, deUint32* pNumRequirements, VkSparseImageMemoryRequirements* pSparseMemoryRequirements)
{
	DE_UNREF(device);
	DE_UNREF(image);
	DE_UNREF(pNumRequirements);
	DE_UNREF(pSparseMemoryRequirements);
	return VK_SUCCESS;
}

VkResult getPhysicalDeviceSparseImageFormatProperties (VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, deUint32 samples, VkImageUsageFlags usage, VkImageTiling tiling, deUint32* pNumProperties, VkSparseImageFormatProperties* pProperties)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(format);
	DE_UNREF(type);
	DE_UNREF(samples);
	DE_UNREF(usage);
	DE_UNREF(tiling);
	DE_UNREF(pNumProperties);
	DE_UNREF(pProperties);
	return VK_SUCCESS;
}

VkResult queueBindSparseBufferMemory (VkQueue queue, VkBuffer buffer, deUint32 numBindings, const VkSparseMemoryBindInfo* pBindInfo)
{
	DE_UNREF(queue);
	DE_UNREF(buffer);
	DE_UNREF(numBindings);
	DE_UNREF(pBindInfo);
	return VK_SUCCESS;
}

VkResult queueBindSparseImageOpaqueMemory (VkQueue queue, VkImage image, deUint32 numBindings, const VkSparseMemoryBindInfo* pBindInfo)
{
	DE_UNREF(queue);
	DE_UNREF(image);
	DE_UNREF(numBindings);
	DE_UNREF(pBindInfo);
	return VK_SUCCESS;
}

VkResult queueBindSparseImageMemory (VkQueue queue, VkImage image, deUint32 numBindings, const VkSparseImageMemoryBindInfo* pBindInfo)
{
	DE_UNREF(queue);
	DE_UNREF(image);
	DE_UNREF(numBindings);
	DE_UNREF(pBindInfo);
	return VK_SUCCESS;
}

VkResult resetFences (VkDevice device, deUint32 fenceCount, const VkFence* pFences)
{
	DE_UNREF(device);
	DE_UNREF(fenceCount);
	DE_UNREF(pFences);
	return VK_SUCCESS;
}

VkResult getFenceStatus (VkDevice device, VkFence fence)
{
	DE_UNREF(device);
	DE_UNREF(fence);
	return VK_SUCCESS;
}

VkResult waitForFences (VkDevice device, deUint32 fenceCount, const VkFence* pFences, VkBool32 waitAll, deUint64 timeout)
{
	DE_UNREF(device);
	DE_UNREF(fenceCount);
	DE_UNREF(pFences);
	DE_UNREF(waitAll);
	DE_UNREF(timeout);
	return VK_SUCCESS;
}

VkResult queueSignalSemaphore (VkQueue queue, VkSemaphore semaphore)
{
	DE_UNREF(queue);
	DE_UNREF(semaphore);
	return VK_SUCCESS;
}

VkResult queueWaitSemaphore (VkQueue queue, VkSemaphore semaphore)
{
	DE_UNREF(queue);
	DE_UNREF(semaphore);
	return VK_SUCCESS;
}

VkResult getEventStatus (VkDevice device, VkEvent event)
{
	DE_UNREF(device);
	DE_UNREF(event);
	return VK_SUCCESS;
}

VkResult setEvent (VkDevice device, VkEvent event)
{
	DE_UNREF(device);
	DE_UNREF(event);
	return VK_SUCCESS;
}

VkResult resetEvent (VkDevice device, VkEvent event)
{
	DE_UNREF(device);
	DE_UNREF(event);
	return VK_SUCCESS;
}

VkResult getQueryPoolResults (VkDevice device, VkQueryPool queryPool, deUint32 startQuery, deUint32 queryCount, deUintptr* pDataSize, void* pData, VkQueryResultFlags flags)
{
	DE_UNREF(device);
	DE_UNREF(queryPool);
	DE_UNREF(startQuery);
	DE_UNREF(queryCount);
	DE_UNREF(pDataSize);
	DE_UNREF(pData);
	DE_UNREF(flags);
	return VK_SUCCESS;
}

VkResult getImageSubresourceLayout (VkDevice device, VkImage image, const VkImageSubresource* pSubresource, VkSubresourceLayout* pLayout)
{
	DE_UNREF(device);
	DE_UNREF(image);
	DE_UNREF(pSubresource);
	DE_UNREF(pLayout);
	return VK_SUCCESS;
}

deUintptr getPipelineCacheSize (VkDevice device, VkPipelineCache pipelineCache)
{
	DE_UNREF(device);
	DE_UNREF(pipelineCache);
	return VK_SUCCESS;
}

VkResult getPipelineCacheData (VkDevice device, VkPipelineCache pipelineCache, void* pData)
{
	DE_UNREF(device);
	DE_UNREF(pipelineCache);
	DE_UNREF(pData);
	return VK_SUCCESS;
}

VkResult mergePipelineCaches (VkDevice device, VkPipelineCache destCache, deUint32 srcCacheCount, const VkPipelineCache* pSrcCaches)
{
	DE_UNREF(device);
	DE_UNREF(destCache);
	DE_UNREF(srcCacheCount);
	DE_UNREF(pSrcCaches);
	return VK_SUCCESS;
}

VkResult resetDescriptorPool (VkDevice device, VkDescriptorPool descriptorPool)
{
	DE_UNREF(device);
	DE_UNREF(descriptorPool);
	return VK_SUCCESS;
}

void updateDescriptorSets (VkDevice device, deUint32 writeCount, const VkWriteDescriptorSet* pDescriptorWrites, deUint32 copyCount, const VkCopyDescriptorSet* pDescriptorCopies)
{
	DE_UNREF(device);
	DE_UNREF(writeCount);
	DE_UNREF(pDescriptorWrites);
	DE_UNREF(copyCount);
	DE_UNREF(pDescriptorCopies);
}

VkResult getRenderAreaGranularity (VkDevice device, VkRenderPass renderPass, VkExtent2D* pGranularity)
{
	DE_UNREF(device);
	DE_UNREF(renderPass);
	DE_UNREF(pGranularity);
	return VK_SUCCESS;
}

VkResult resetCommandPool (VkDevice device, VkCmdPool cmdPool, VkCmdPoolResetFlags flags)
{
	DE_UNREF(device);
	DE_UNREF(cmdPool);
	DE_UNREF(flags);
	return VK_SUCCESS;
}

VkResult beginCommandBuffer (VkCmdBuffer cmdBuffer, const VkCmdBufferBeginInfo* pBeginInfo)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(pBeginInfo);
	return VK_SUCCESS;
}

VkResult endCommandBuffer (VkCmdBuffer cmdBuffer)
{
	DE_UNREF(cmdBuffer);
	return VK_SUCCESS;
}

VkResult resetCommandBuffer (VkCmdBuffer cmdBuffer, VkCmdBufferResetFlags flags)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(flags);
	return VK_SUCCESS;
}

void cmdBindPipeline (VkCmdBuffer cmdBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(pipelineBindPoint);
	DE_UNREF(pipeline);
}

void cmdSetViewport (VkCmdBuffer cmdBuffer, deUint32 viewportCount, const VkViewport* pViewports)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(viewportCount);
	DE_UNREF(pViewports);
}

void cmdSetScissor (VkCmdBuffer cmdBuffer, deUint32 scissorCount, const VkRect2D* pScissors)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(scissorCount);
	DE_UNREF(pScissors);
}

void cmdSetLineWidth (VkCmdBuffer cmdBuffer, float lineWidth)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(lineWidth);
}

void cmdSetDepthBias (VkCmdBuffer cmdBuffer, float depthBias, float depthBiasClamp, float slopeScaledDepthBias)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(depthBias);
	DE_UNREF(depthBiasClamp);
	DE_UNREF(slopeScaledDepthBias);
}

void cmdSetBlendConstants (VkCmdBuffer cmdBuffer, const float blendConst)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(blendConst);
}

void cmdSetDepthBounds (VkCmdBuffer cmdBuffer, float minDepthBounds, float maxDepthBounds)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(minDepthBounds);
	DE_UNREF(maxDepthBounds);
}

void cmdSetStencilCompareMask (VkCmdBuffer cmdBuffer, VkStencilFaceFlags faceMask, deUint32 stencilCompareMask)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(faceMask);
	DE_UNREF(stencilCompareMask);
}

void cmdSetStencilWriteMask (VkCmdBuffer cmdBuffer, VkStencilFaceFlags faceMask, deUint32 stencilWriteMask)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(faceMask);
	DE_UNREF(stencilWriteMask);
}

void cmdSetStencilReference (VkCmdBuffer cmdBuffer, VkStencilFaceFlags faceMask, deUint32 stencilReference)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(faceMask);
	DE_UNREF(stencilReference);
}

void cmdBindDescriptorSets (VkCmdBuffer cmdBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, deUint32 firstSet, deUint32 setCount, const VkDescriptorSet* pDescriptorSets, deUint32 dynamicOffsetCount, const deUint32* pDynamicOffsets)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(pipelineBindPoint);
	DE_UNREF(layout);
	DE_UNREF(firstSet);
	DE_UNREF(setCount);
	DE_UNREF(pDescriptorSets);
	DE_UNREF(dynamicOffsetCount);
	DE_UNREF(pDynamicOffsets);
}

void cmdBindIndexBuffer (VkCmdBuffer cmdBuffer, VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(buffer);
	DE_UNREF(offset);
	DE_UNREF(indexType);
}

void cmdBindVertexBuffers (VkCmdBuffer cmdBuffer, deUint32 startBinding, deUint32 bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(startBinding);
	DE_UNREF(bindingCount);
	DE_UNREF(pBuffers);
	DE_UNREF(pOffsets);
}

void cmdDraw (VkCmdBuffer cmdBuffer, deUint32 vertexCount, deUint32 instanceCount, deUint32 firstVertex, deUint32 firstInstance)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(vertexCount);
	DE_UNREF(instanceCount);
	DE_UNREF(firstVertex);
	DE_UNREF(firstInstance);
}

void cmdDrawIndexed (VkCmdBuffer cmdBuffer, deUint32 indexCount, deUint32 instanceCount, deUint32 firstIndex, deInt32 vertexOffset, deUint32 firstInstance)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(indexCount);
	DE_UNREF(instanceCount);
	DE_UNREF(firstIndex);
	DE_UNREF(vertexOffset);
	DE_UNREF(firstInstance);
}

void cmdDrawIndirect (VkCmdBuffer cmdBuffer, VkBuffer buffer, VkDeviceSize offset, deUint32 count, deUint32 stride)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(buffer);
	DE_UNREF(offset);
	DE_UNREF(count);
	DE_UNREF(stride);
}

void cmdDrawIndexedIndirect (VkCmdBuffer cmdBuffer, VkBuffer buffer, VkDeviceSize offset, deUint32 count, deUint32 stride)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(buffer);
	DE_UNREF(offset);
	DE_UNREF(count);
	DE_UNREF(stride);
}

void cmdDispatch (VkCmdBuffer cmdBuffer, deUint32 x, deUint32 y, deUint32 z)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(x);
	DE_UNREF(y);
	DE_UNREF(z);
}

void cmdDispatchIndirect (VkCmdBuffer cmdBuffer, VkBuffer buffer, VkDeviceSize offset)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(buffer);
	DE_UNREF(offset);
}

void cmdCopyBuffer (VkCmdBuffer cmdBuffer, VkBuffer srcBuffer, VkBuffer destBuffer, deUint32 regionCount, const VkBufferCopy* pRegions)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(srcBuffer);
	DE_UNREF(destBuffer);
	DE_UNREF(regionCount);
	DE_UNREF(pRegions);
}

void cmdCopyImage (VkCmdBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage destImage, VkImageLayout destImageLayout, deUint32 regionCount, const VkImageCopy* pRegions)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(srcImage);
	DE_UNREF(srcImageLayout);
	DE_UNREF(destImage);
	DE_UNREF(destImageLayout);
	DE_UNREF(regionCount);
	DE_UNREF(pRegions);
}

void cmdBlitImage (VkCmdBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage destImage, VkImageLayout destImageLayout, deUint32 regionCount, const VkImageBlit* pRegions, VkTexFilter filter)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(srcImage);
	DE_UNREF(srcImageLayout);
	DE_UNREF(destImage);
	DE_UNREF(destImageLayout);
	DE_UNREF(regionCount);
	DE_UNREF(pRegions);
	DE_UNREF(filter);
}

void cmdCopyBufferToImage (VkCmdBuffer cmdBuffer, VkBuffer srcBuffer, VkImage destImage, VkImageLayout destImageLayout, deUint32 regionCount, const VkBufferImageCopy* pRegions)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(srcBuffer);
	DE_UNREF(destImage);
	DE_UNREF(destImageLayout);
	DE_UNREF(regionCount);
	DE_UNREF(pRegions);
}

void cmdCopyImageToBuffer (VkCmdBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkBuffer destBuffer, deUint32 regionCount, const VkBufferImageCopy* pRegions)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(srcImage);
	DE_UNREF(srcImageLayout);
	DE_UNREF(destBuffer);
	DE_UNREF(regionCount);
	DE_UNREF(pRegions);
}

void cmdUpdateBuffer (VkCmdBuffer cmdBuffer, VkBuffer destBuffer, VkDeviceSize destOffset, VkDeviceSize dataSize, const deUint32* pData)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(destBuffer);
	DE_UNREF(destOffset);
	DE_UNREF(dataSize);
	DE_UNREF(pData);
}

void cmdFillBuffer (VkCmdBuffer cmdBuffer, VkBuffer destBuffer, VkDeviceSize destOffset, VkDeviceSize fillSize, deUint32 data)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(destBuffer);
	DE_UNREF(destOffset);
	DE_UNREF(fillSize);
	DE_UNREF(data);
}

void cmdClearColorImage (VkCmdBuffer cmdBuffer, VkImage image, VkImageLayout imageLayout, const VkClearColorValue* pColor, deUint32 rangeCount, const VkImageSubresourceRange* pRanges)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(image);
	DE_UNREF(imageLayout);
	DE_UNREF(pColor);
	DE_UNREF(rangeCount);
	DE_UNREF(pRanges);
}

void cmdClearDepthStencilImage (VkCmdBuffer cmdBuffer, VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue* pDepthStencil, deUint32 rangeCount, const VkImageSubresourceRange* pRanges)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(image);
	DE_UNREF(imageLayout);
	DE_UNREF(pDepthStencil);
	DE_UNREF(rangeCount);
	DE_UNREF(pRanges);
}

void cmdClearColorAttachment (VkCmdBuffer cmdBuffer, deUint32 colorAttachment, VkImageLayout imageLayout, const VkClearColorValue* pColor, deUint32 rectCount, const VkRect3D* pRects)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(colorAttachment);
	DE_UNREF(imageLayout);
	DE_UNREF(pColor);
	DE_UNREF(rectCount);
	DE_UNREF(pRects);
}

void cmdClearDepthStencilAttachment (VkCmdBuffer cmdBuffer, VkImageAspectFlags aspectMask, VkImageLayout imageLayout, const VkClearDepthStencilValue* pDepthStencil, deUint32 rectCount, const VkRect3D* pRects)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(aspectMask);
	DE_UNREF(imageLayout);
	DE_UNREF(pDepthStencil);
	DE_UNREF(rectCount);
	DE_UNREF(pRects);
}

void cmdResolveImage (VkCmdBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage destImage, VkImageLayout destImageLayout, deUint32 regionCount, const VkImageResolve* pRegions)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(srcImage);
	DE_UNREF(srcImageLayout);
	DE_UNREF(destImage);
	DE_UNREF(destImageLayout);
	DE_UNREF(regionCount);
	DE_UNREF(pRegions);
}

void cmdSetEvent (VkCmdBuffer cmdBuffer, VkEvent event, VkPipelineStageFlags stageMask)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(event);
	DE_UNREF(stageMask);
}

void cmdResetEvent (VkCmdBuffer cmdBuffer, VkEvent event, VkPipelineStageFlags stageMask)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(event);
	DE_UNREF(stageMask);
}

void cmdWaitEvents (VkCmdBuffer cmdBuffer, deUint32 eventCount, const VkEvent* pEvents, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags destStageMask, deUint32 memBarrierCount, const void* const* ppMemBarriers)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(eventCount);
	DE_UNREF(pEvents);
	DE_UNREF(srcStageMask);
	DE_UNREF(destStageMask);
	DE_UNREF(memBarrierCount);
	DE_UNREF(ppMemBarriers);
}

void cmdPipelineBarrier (VkCmdBuffer cmdBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags destStageMask, VkBool32 byRegion, deUint32 memBarrierCount, const void* const* ppMemBarriers)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(srcStageMask);
	DE_UNREF(destStageMask);
	DE_UNREF(byRegion);
	DE_UNREF(memBarrierCount);
	DE_UNREF(ppMemBarriers);
}

void cmdBeginQuery (VkCmdBuffer cmdBuffer, VkQueryPool queryPool, deUint32 slot, VkQueryControlFlags flags)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(queryPool);
	DE_UNREF(slot);
	DE_UNREF(flags);
}

void cmdEndQuery (VkCmdBuffer cmdBuffer, VkQueryPool queryPool, deUint32 slot)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(queryPool);
	DE_UNREF(slot);
}

void cmdResetQueryPool (VkCmdBuffer cmdBuffer, VkQueryPool queryPool, deUint32 startQuery, deUint32 queryCount)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(queryPool);
	DE_UNREF(startQuery);
	DE_UNREF(queryCount);
}

void cmdWriteTimestamp (VkCmdBuffer cmdBuffer, VkTimestampType timestampType, VkBuffer destBuffer, VkDeviceSize destOffset)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(timestampType);
	DE_UNREF(destBuffer);
	DE_UNREF(destOffset);
}

void cmdCopyQueryPoolResults (VkCmdBuffer cmdBuffer, VkQueryPool queryPool, deUint32 startQuery, deUint32 queryCount, VkBuffer destBuffer, VkDeviceSize destOffset, VkDeviceSize destStride, VkQueryResultFlags flags)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(queryPool);
	DE_UNREF(startQuery);
	DE_UNREF(queryCount);
	DE_UNREF(destBuffer);
	DE_UNREF(destOffset);
	DE_UNREF(destStride);
	DE_UNREF(flags);
}

void cmdPushConstants (VkCmdBuffer cmdBuffer, VkPipelineLayout layout, VkShaderStageFlags stageFlags, deUint32 start, deUint32 length, const void* values)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(layout);
	DE_UNREF(stageFlags);
	DE_UNREF(start);
	DE_UNREF(length);
	DE_UNREF(values);
}

void cmdBeginRenderPass (VkCmdBuffer cmdBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, VkRenderPassContents contents)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(pRenderPassBegin);
	DE_UNREF(contents);
}

void cmdNextSubpass (VkCmdBuffer cmdBuffer, VkRenderPassContents contents)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(contents);
}

void cmdEndRenderPass (VkCmdBuffer cmdBuffer)
{
	DE_UNREF(cmdBuffer);
}

void cmdExecuteCommands (VkCmdBuffer cmdBuffer, deUint32 cmdBuffersCount, const VkCmdBuffer* pCmdBuffers)
{
	DE_UNREF(cmdBuffer);
	DE_UNREF(cmdBuffersCount);
	DE_UNREF(pCmdBuffers);
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
	VK_NULL_FUNC_ENTRY(vkAllocMemory,									allocMemory),
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
	VK_NULL_FUNC_ENTRY(vkQueueBindSparseBufferMemory,					queueBindSparseBufferMemory),
	VK_NULL_FUNC_ENTRY(vkQueueBindSparseImageOpaqueMemory,				queueBindSparseImageOpaqueMemory),
	VK_NULL_FUNC_ENTRY(vkQueueBindSparseImageMemory,					queueBindSparseImageMemory),
	VK_NULL_FUNC_ENTRY(vkCreateFence,									createFence),
	VK_NULL_FUNC_ENTRY(vkDestroyFence,									destroyFence),
	VK_NULL_FUNC_ENTRY(vkResetFences,									resetFences),
	VK_NULL_FUNC_ENTRY(vkGetFenceStatus,								getFenceStatus),
	VK_NULL_FUNC_ENTRY(vkWaitForFences,									waitForFences),
	VK_NULL_FUNC_ENTRY(vkCreateSemaphore,								createSemaphore),
	VK_NULL_FUNC_ENTRY(vkDestroySemaphore,								destroySemaphore),
	VK_NULL_FUNC_ENTRY(vkQueueSignalSemaphore,							queueSignalSemaphore),
	VK_NULL_FUNC_ENTRY(vkQueueWaitSemaphore,							queueWaitSemaphore),
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
	VK_NULL_FUNC_ENTRY(vkCreateShader,									createShader),
	VK_NULL_FUNC_ENTRY(vkDestroyShader,									destroyShader),
	VK_NULL_FUNC_ENTRY(vkCreatePipelineCache,							createPipelineCache),
	VK_NULL_FUNC_ENTRY(vkDestroyPipelineCache,							destroyPipelineCache),
	VK_NULL_FUNC_ENTRY(vkGetPipelineCacheSize,							getPipelineCacheSize),
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
	VK_NULL_FUNC_ENTRY(vkAllocDescriptorSets,							allocDescriptorSets),
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
	VK_NULL_FUNC_ENTRY(vkCreateCommandBuffer,							createCommandBuffer),
	VK_NULL_FUNC_ENTRY(vkDestroyCommandBuffer,							destroyCommandBuffer),
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
	VK_NULL_FUNC_ENTRY(vkCmdClearColorAttachment,						cmdClearColorAttachment),
	VK_NULL_FUNC_ENTRY(vkCmdClearDepthStencilAttachment,				cmdClearDepthStencilAttachment),
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

