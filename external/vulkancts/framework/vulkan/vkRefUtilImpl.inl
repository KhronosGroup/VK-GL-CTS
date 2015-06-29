/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 */
Move<VkInstanceT> createInstance (const PlatformInterface& vk, const VkInstanceCreateInfo* pCreateInfo)
{
	VkInstance object = 0;
	VK_CHECK(vk.createInstance(pCreateInfo, &object));
	return Move<VkInstanceT>(vk, check<VkInstanceT>(object));
}

Move<VkDeviceT> createDevice (const DeviceInterface& vk, VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo)
{
	VkDevice object = 0;
	VK_CHECK(vk.createDevice(physicalDevice, pCreateInfo, &object));
	return Move<VkDeviceT>(vk, check<VkDeviceT>(object));
}

Move<VkFenceT> createFence (const DeviceInterface& vk, VkDevice device, const VkFenceCreateInfo* pCreateInfo)
{
	VkFence object = 0;
	VK_CHECK(vk.createFence(device, pCreateInfo, &object));
	return Move<VkFenceT>(vk, device, check<VkFenceT>(object));
}

Move<VkSemaphoreT> createSemaphore (const DeviceInterface& vk, VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo)
{
	VkSemaphore object = 0;
	VK_CHECK(vk.createSemaphore(device, pCreateInfo, &object));
	return Move<VkSemaphoreT>(vk, device, check<VkSemaphoreT>(object));
}

Move<VkEventT> createEvent (const DeviceInterface& vk, VkDevice device, const VkEventCreateInfo* pCreateInfo)
{
	VkEvent object = 0;
	VK_CHECK(vk.createEvent(device, pCreateInfo, &object));
	return Move<VkEventT>(vk, device, check<VkEventT>(object));
}

Move<VkQueryPoolT> createQueryPool (const DeviceInterface& vk, VkDevice device, const VkQueryPoolCreateInfo* pCreateInfo)
{
	VkQueryPool object = 0;
	VK_CHECK(vk.createQueryPool(device, pCreateInfo, &object));
	return Move<VkQueryPoolT>(vk, device, check<VkQueryPoolT>(object));
}

Move<VkBufferT> createBuffer (const DeviceInterface& vk, VkDevice device, const VkBufferCreateInfo* pCreateInfo)
{
	VkBuffer object = 0;
	VK_CHECK(vk.createBuffer(device, pCreateInfo, &object));
	return Move<VkBufferT>(vk, device, check<VkBufferT>(object));
}

Move<VkBufferViewT> createBufferView (const DeviceInterface& vk, VkDevice device, const VkBufferViewCreateInfo* pCreateInfo)
{
	VkBufferView object = 0;
	VK_CHECK(vk.createBufferView(device, pCreateInfo, &object));
	return Move<VkBufferViewT>(vk, device, check<VkBufferViewT>(object));
}

Move<VkImageT> createImage (const DeviceInterface& vk, VkDevice device, const VkImageCreateInfo* pCreateInfo)
{
	VkImage object = 0;
	VK_CHECK(vk.createImage(device, pCreateInfo, &object));
	return Move<VkImageT>(vk, device, check<VkImageT>(object));
}

Move<VkImageViewT> createImageView (const DeviceInterface& vk, VkDevice device, const VkImageViewCreateInfo* pCreateInfo)
{
	VkImageView object = 0;
	VK_CHECK(vk.createImageView(device, pCreateInfo, &object));
	return Move<VkImageViewT>(vk, device, check<VkImageViewT>(object));
}

Move<VkColorAttachmentViewT> createColorAttachmentView (const DeviceInterface& vk, VkDevice device, const VkColorAttachmentViewCreateInfo* pCreateInfo)
{
	VkColorAttachmentView object = 0;
	VK_CHECK(vk.createColorAttachmentView(device, pCreateInfo, &object));
	return Move<VkColorAttachmentViewT>(vk, device, check<VkColorAttachmentViewT>(object));
}

Move<VkDepthStencilViewT> createDepthStencilView (const DeviceInterface& vk, VkDevice device, const VkDepthStencilViewCreateInfo* pCreateInfo)
{
	VkDepthStencilView object = 0;
	VK_CHECK(vk.createDepthStencilView(device, pCreateInfo, &object));
	return Move<VkDepthStencilViewT>(vk, device, check<VkDepthStencilViewT>(object));
}

Move<VkShaderT> createShader (const DeviceInterface& vk, VkDevice device, const VkShaderCreateInfo* pCreateInfo)
{
	VkShader object = 0;
	VK_CHECK(vk.createShader(device, pCreateInfo, &object));
	return Move<VkShaderT>(vk, device, check<VkShaderT>(object));
}

Move<VkPipelineT> createGraphicsPipeline (const DeviceInterface& vk, VkDevice device, const VkGraphicsPipelineCreateInfo* pCreateInfo)
{
	VkPipeline object = 0;
	VK_CHECK(vk.createGraphicsPipeline(device, pCreateInfo, &object));
	return Move<VkPipelineT>(vk, device, check<VkPipelineT>(object));
}

Move<VkPipelineT> createGraphicsPipelineDerivative (const DeviceInterface& vk, VkDevice device, const VkGraphicsPipelineCreateInfo* pCreateInfo, VkPipeline basePipeline)
{
	VkPipeline object = 0;
	VK_CHECK(vk.createGraphicsPipelineDerivative(device, pCreateInfo, basePipeline, &object));
	return Move<VkPipelineT>(vk, device, check<VkPipelineT>(object));
}

Move<VkPipelineT> createComputePipeline (const DeviceInterface& vk, VkDevice device, const VkComputePipelineCreateInfo* pCreateInfo)
{
	VkPipeline object = 0;
	VK_CHECK(vk.createComputePipeline(device, pCreateInfo, &object));
	return Move<VkPipelineT>(vk, device, check<VkPipelineT>(object));
}

Move<VkPipelineLayoutT> createPipelineLayout (const DeviceInterface& vk, VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo)
{
	VkPipelineLayout object = 0;
	VK_CHECK(vk.createPipelineLayout(device, pCreateInfo, &object));
	return Move<VkPipelineLayoutT>(vk, device, check<VkPipelineLayoutT>(object));
}

Move<VkSamplerT> createSampler (const DeviceInterface& vk, VkDevice device, const VkSamplerCreateInfo* pCreateInfo)
{
	VkSampler object = 0;
	VK_CHECK(vk.createSampler(device, pCreateInfo, &object));
	return Move<VkSamplerT>(vk, device, check<VkSamplerT>(object));
}

Move<VkDescriptorSetLayoutT> createDescriptorSetLayout (const DeviceInterface& vk, VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo)
{
	VkDescriptorSetLayout object = 0;
	VK_CHECK(vk.createDescriptorSetLayout(device, pCreateInfo, &object));
	return Move<VkDescriptorSetLayoutT>(vk, device, check<VkDescriptorSetLayoutT>(object));
}

Move<VkDescriptorPoolT> createDescriptorPool (const DeviceInterface& vk, VkDevice device, VkDescriptorPoolUsage poolUsage, deUint32 maxSets, const VkDescriptorPoolCreateInfo* pCreateInfo)
{
	VkDescriptorPool object = 0;
	VK_CHECK(vk.createDescriptorPool(device, poolUsage, maxSets, pCreateInfo, &object));
	return Move<VkDescriptorPoolT>(vk, device, check<VkDescriptorPoolT>(object));
}

Move<VkDynamicVpStateT> createDynamicViewportState (const DeviceInterface& vk, VkDevice device, const VkDynamicVpStateCreateInfo* pCreateInfo)
{
	VkDynamicVpState object = 0;
	VK_CHECK(vk.createDynamicViewportState(device, pCreateInfo, &object));
	return Move<VkDynamicVpStateT>(vk, device, check<VkDynamicVpStateT>(object));
}

Move<VkDynamicRsStateT> createDynamicRasterState (const DeviceInterface& vk, VkDevice device, const VkDynamicRsStateCreateInfo* pCreateInfo)
{
	VkDynamicRsState object = 0;
	VK_CHECK(vk.createDynamicRasterState(device, pCreateInfo, &object));
	return Move<VkDynamicRsStateT>(vk, device, check<VkDynamicRsStateT>(object));
}

Move<VkDynamicCbStateT> createDynamicColorBlendState (const DeviceInterface& vk, VkDevice device, const VkDynamicCbStateCreateInfo* pCreateInfo)
{
	VkDynamicCbState object = 0;
	VK_CHECK(vk.createDynamicColorBlendState(device, pCreateInfo, &object));
	return Move<VkDynamicCbStateT>(vk, device, check<VkDynamicCbStateT>(object));
}

Move<VkDynamicDsStateT> createDynamicDepthStencilState (const DeviceInterface& vk, VkDevice device, const VkDynamicDsStateCreateInfo* pCreateInfo)
{
	VkDynamicDsState object = 0;
	VK_CHECK(vk.createDynamicDepthStencilState(device, pCreateInfo, &object));
	return Move<VkDynamicDsStateT>(vk, device, check<VkDynamicDsStateT>(object));
}

Move<VkCmdBufferT> createCommandBuffer (const DeviceInterface& vk, VkDevice device, const VkCmdBufferCreateInfo* pCreateInfo)
{
	VkCmdBuffer object = 0;
	VK_CHECK(vk.createCommandBuffer(device, pCreateInfo, &object));
	return Move<VkCmdBufferT>(vk, device, check<VkCmdBufferT>(object));
}

Move<VkFramebufferT> createFramebuffer (const DeviceInterface& vk, VkDevice device, const VkFramebufferCreateInfo* pCreateInfo)
{
	VkFramebuffer object = 0;
	VK_CHECK(vk.createFramebuffer(device, pCreateInfo, &object));
	return Move<VkFramebufferT>(vk, device, check<VkFramebufferT>(object));
}

Move<VkRenderPassT> createRenderPass (const DeviceInterface& vk, VkDevice device, const VkRenderPassCreateInfo* pCreateInfo)
{
	VkRenderPass object = 0;
	VK_CHECK(vk.createRenderPass(device, pCreateInfo, &object));
	return Move<VkRenderPassT>(vk, device, check<VkRenderPassT>(object));
}

