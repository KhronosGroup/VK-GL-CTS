/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 */
namespace refdetails
{

template<>
void Deleter<VkDeviceMemory>::operator() (VkDeviceMemory obj) const
{
	DE_TEST_ASSERT(m_deviceIface->freeMemory(m_device, obj) == VK_SUCCESS);
}

template<>
void Deleter<VkFence>::operator() (VkFence obj) const
{
	DE_TEST_ASSERT(m_deviceIface->destroyFence(m_device, obj) == VK_SUCCESS);
}

template<>
void Deleter<VkSemaphore>::operator() (VkSemaphore obj) const
{
	DE_TEST_ASSERT(m_deviceIface->destroySemaphore(m_device, obj) == VK_SUCCESS);
}

template<>
void Deleter<VkEvent>::operator() (VkEvent obj) const
{
	DE_TEST_ASSERT(m_deviceIface->destroyEvent(m_device, obj) == VK_SUCCESS);
}

template<>
void Deleter<VkQueryPool>::operator() (VkQueryPool obj) const
{
	DE_TEST_ASSERT(m_deviceIface->destroyQueryPool(m_device, obj) == VK_SUCCESS);
}

template<>
void Deleter<VkBuffer>::operator() (VkBuffer obj) const
{
	DE_TEST_ASSERT(m_deviceIface->destroyBuffer(m_device, obj) == VK_SUCCESS);
}

template<>
void Deleter<VkBufferView>::operator() (VkBufferView obj) const
{
	DE_TEST_ASSERT(m_deviceIface->destroyBufferView(m_device, obj) == VK_SUCCESS);
}

template<>
void Deleter<VkImage>::operator() (VkImage obj) const
{
	DE_TEST_ASSERT(m_deviceIface->destroyImage(m_device, obj) == VK_SUCCESS);
}

template<>
void Deleter<VkImageView>::operator() (VkImageView obj) const
{
	DE_TEST_ASSERT(m_deviceIface->destroyImageView(m_device, obj) == VK_SUCCESS);
}

template<>
void Deleter<VkAttachmentView>::operator() (VkAttachmentView obj) const
{
	DE_TEST_ASSERT(m_deviceIface->destroyAttachmentView(m_device, obj) == VK_SUCCESS);
}

template<>
void Deleter<VkShaderModule>::operator() (VkShaderModule obj) const
{
	DE_TEST_ASSERT(m_deviceIface->destroyShaderModule(m_device, obj) == VK_SUCCESS);
}

template<>
void Deleter<VkShader>::operator() (VkShader obj) const
{
	DE_TEST_ASSERT(m_deviceIface->destroyShader(m_device, obj) == VK_SUCCESS);
}

template<>
void Deleter<VkPipelineCache>::operator() (VkPipelineCache obj) const
{
	DE_TEST_ASSERT(m_deviceIface->destroyPipelineCache(m_device, obj) == VK_SUCCESS);
}

template<>
void Deleter<VkPipeline>::operator() (VkPipeline obj) const
{
	DE_TEST_ASSERT(m_deviceIface->destroyPipeline(m_device, obj) == VK_SUCCESS);
}

template<>
void Deleter<VkPipelineLayout>::operator() (VkPipelineLayout obj) const
{
	DE_TEST_ASSERT(m_deviceIface->destroyPipelineLayout(m_device, obj) == VK_SUCCESS);
}

template<>
void Deleter<VkSampler>::operator() (VkSampler obj) const
{
	DE_TEST_ASSERT(m_deviceIface->destroySampler(m_device, obj) == VK_SUCCESS);
}

template<>
void Deleter<VkDescriptorSetLayout>::operator() (VkDescriptorSetLayout obj) const
{
	DE_TEST_ASSERT(m_deviceIface->destroyDescriptorSetLayout(m_device, obj) == VK_SUCCESS);
}

template<>
void Deleter<VkDescriptorPool>::operator() (VkDescriptorPool obj) const
{
	DE_TEST_ASSERT(m_deviceIface->destroyDescriptorPool(m_device, obj) == VK_SUCCESS);
}

template<>
void Deleter<VkDynamicViewportState>::operator() (VkDynamicViewportState obj) const
{
	DE_TEST_ASSERT(m_deviceIface->destroyDynamicViewportState(m_device, obj) == VK_SUCCESS);
}

template<>
void Deleter<VkDynamicRasterState>::operator() (VkDynamicRasterState obj) const
{
	DE_TEST_ASSERT(m_deviceIface->destroyDynamicRasterState(m_device, obj) == VK_SUCCESS);
}

template<>
void Deleter<VkDynamicColorBlendState>::operator() (VkDynamicColorBlendState obj) const
{
	DE_TEST_ASSERT(m_deviceIface->destroyDynamicColorBlendState(m_device, obj) == VK_SUCCESS);
}

template<>
void Deleter<VkDynamicDepthStencilState>::operator() (VkDynamicDepthStencilState obj) const
{
	DE_TEST_ASSERT(m_deviceIface->destroyDynamicDepthStencilState(m_device, obj) == VK_SUCCESS);
}

template<>
void Deleter<VkFramebuffer>::operator() (VkFramebuffer obj) const
{
	DE_TEST_ASSERT(m_deviceIface->destroyFramebuffer(m_device, obj) == VK_SUCCESS);
}

template<>
void Deleter<VkRenderPass>::operator() (VkRenderPass obj) const
{
	DE_TEST_ASSERT(m_deviceIface->destroyRenderPass(m_device, obj) == VK_SUCCESS);
}

template<>
void Deleter<VkCmdPool>::operator() (VkCmdPool obj) const
{
	DE_TEST_ASSERT(m_deviceIface->destroyCommandPool(m_device, obj) == VK_SUCCESS);
}

template<>
void Deleter<VkCmdBuffer>::operator() (VkCmdBuffer obj) const
{
	DE_TEST_ASSERT(m_deviceIface->destroyCommandBuffer(m_device, obj) == VK_SUCCESS);
}

} // refdetails

Move<VkInstance> createInstance (const PlatformInterface& vk, const VkInstanceCreateInfo* pCreateInfo)
{
	VkInstance object = 0;
	VK_CHECK(vk.createInstance(pCreateInfo, &object));
	return Move<VkInstance>(check<VkInstance>(object), Deleter<VkInstance>(vk, object));
}

Move<VkDevice> createDevice (const InstanceInterface& vk, VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo)
{
	VkDevice object = 0;
	VK_CHECK(vk.createDevice(physicalDevice, pCreateInfo, &object));
	return Move<VkDevice>(check<VkDevice>(object), Deleter<VkDevice>(vk, object));
}

Move<VkDeviceMemory> allocMemory (const DeviceInterface& vk, VkDevice device, const VkMemoryAllocInfo* pAllocInfo)
{
	VkDeviceMemory object = 0;
	VK_CHECK(vk.allocMemory(device, pAllocInfo, &object));
	return Move<VkDeviceMemory>(check<VkDeviceMemory>(object), Deleter<VkDeviceMemory>(vk, device));
}

Move<VkFence> createFence (const DeviceInterface& vk, VkDevice device, const VkFenceCreateInfo* pCreateInfo)
{
	VkFence object = 0;
	VK_CHECK(vk.createFence(device, pCreateInfo, &object));
	return Move<VkFence>(check<VkFence>(object), Deleter<VkFence>(vk, device));
}

Move<VkSemaphore> createSemaphore (const DeviceInterface& vk, VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo)
{
	VkSemaphore object = 0;
	VK_CHECK(vk.createSemaphore(device, pCreateInfo, &object));
	return Move<VkSemaphore>(check<VkSemaphore>(object), Deleter<VkSemaphore>(vk, device));
}

Move<VkEvent> createEvent (const DeviceInterface& vk, VkDevice device, const VkEventCreateInfo* pCreateInfo)
{
	VkEvent object = 0;
	VK_CHECK(vk.createEvent(device, pCreateInfo, &object));
	return Move<VkEvent>(check<VkEvent>(object), Deleter<VkEvent>(vk, device));
}

Move<VkQueryPool> createQueryPool (const DeviceInterface& vk, VkDevice device, const VkQueryPoolCreateInfo* pCreateInfo)
{
	VkQueryPool object = 0;
	VK_CHECK(vk.createQueryPool(device, pCreateInfo, &object));
	return Move<VkQueryPool>(check<VkQueryPool>(object), Deleter<VkQueryPool>(vk, device));
}

Move<VkBuffer> createBuffer (const DeviceInterface& vk, VkDevice device, const VkBufferCreateInfo* pCreateInfo)
{
	VkBuffer object = 0;
	VK_CHECK(vk.createBuffer(device, pCreateInfo, &object));
	return Move<VkBuffer>(check<VkBuffer>(object), Deleter<VkBuffer>(vk, device));
}

Move<VkBufferView> createBufferView (const DeviceInterface& vk, VkDevice device, const VkBufferViewCreateInfo* pCreateInfo)
{
	VkBufferView object = 0;
	VK_CHECK(vk.createBufferView(device, pCreateInfo, &object));
	return Move<VkBufferView>(check<VkBufferView>(object), Deleter<VkBufferView>(vk, device));
}

Move<VkImage> createImage (const DeviceInterface& vk, VkDevice device, const VkImageCreateInfo* pCreateInfo)
{
	VkImage object = 0;
	VK_CHECK(vk.createImage(device, pCreateInfo, &object));
	return Move<VkImage>(check<VkImage>(object), Deleter<VkImage>(vk, device));
}

Move<VkImageView> createImageView (const DeviceInterface& vk, VkDevice device, const VkImageViewCreateInfo* pCreateInfo)
{
	VkImageView object = 0;
	VK_CHECK(vk.createImageView(device, pCreateInfo, &object));
	return Move<VkImageView>(check<VkImageView>(object), Deleter<VkImageView>(vk, device));
}

Move<VkAttachmentView> createAttachmentView (const DeviceInterface& vk, VkDevice device, const VkAttachmentViewCreateInfo* pCreateInfo)
{
	VkAttachmentView object = 0;
	VK_CHECK(vk.createAttachmentView(device, pCreateInfo, &object));
	return Move<VkAttachmentView>(check<VkAttachmentView>(object), Deleter<VkAttachmentView>(vk, device));
}

Move<VkShaderModule> createShaderModule (const DeviceInterface& vk, VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo)
{
	VkShaderModule object = 0;
	VK_CHECK(vk.createShaderModule(device, pCreateInfo, &object));
	return Move<VkShaderModule>(check<VkShaderModule>(object), Deleter<VkShaderModule>(vk, device));
}

Move<VkShader> createShader (const DeviceInterface& vk, VkDevice device, const VkShaderCreateInfo* pCreateInfo)
{
	VkShader object = 0;
	VK_CHECK(vk.createShader(device, pCreateInfo, &object));
	return Move<VkShader>(check<VkShader>(object), Deleter<VkShader>(vk, device));
}

Move<VkPipelineCache> createPipelineCache (const DeviceInterface& vk, VkDevice device, const VkPipelineCacheCreateInfo* pCreateInfo)
{
	VkPipelineCache object = 0;
	VK_CHECK(vk.createPipelineCache(device, pCreateInfo, &object));
	return Move<VkPipelineCache>(check<VkPipelineCache>(object), Deleter<VkPipelineCache>(vk, device));
}

Move<VkPipelineLayout> createPipelineLayout (const DeviceInterface& vk, VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo)
{
	VkPipelineLayout object = 0;
	VK_CHECK(vk.createPipelineLayout(device, pCreateInfo, &object));
	return Move<VkPipelineLayout>(check<VkPipelineLayout>(object), Deleter<VkPipelineLayout>(vk, device));
}

Move<VkSampler> createSampler (const DeviceInterface& vk, VkDevice device, const VkSamplerCreateInfo* pCreateInfo)
{
	VkSampler object = 0;
	VK_CHECK(vk.createSampler(device, pCreateInfo, &object));
	return Move<VkSampler>(check<VkSampler>(object), Deleter<VkSampler>(vk, device));
}

Move<VkDescriptorSetLayout> createDescriptorSetLayout (const DeviceInterface& vk, VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo)
{
	VkDescriptorSetLayout object = 0;
	VK_CHECK(vk.createDescriptorSetLayout(device, pCreateInfo, &object));
	return Move<VkDescriptorSetLayout>(check<VkDescriptorSetLayout>(object), Deleter<VkDescriptorSetLayout>(vk, device));
}

Move<VkDescriptorPool> createDescriptorPool (const DeviceInterface& vk, VkDevice device, VkDescriptorPoolUsage poolUsage, deUint32 maxSets, const VkDescriptorPoolCreateInfo* pCreateInfo)
{
	VkDescriptorPool object = 0;
	VK_CHECK(vk.createDescriptorPool(device, poolUsage, maxSets, pCreateInfo, &object));
	return Move<VkDescriptorPool>(check<VkDescriptorPool>(object), Deleter<VkDescriptorPool>(vk, device));
}

Move<VkDynamicViewportState> createDynamicViewportState (const DeviceInterface& vk, VkDevice device, const VkDynamicViewportStateCreateInfo* pCreateInfo)
{
	VkDynamicViewportState object = 0;
	VK_CHECK(vk.createDynamicViewportState(device, pCreateInfo, &object));
	return Move<VkDynamicViewportState>(check<VkDynamicViewportState>(object), Deleter<VkDynamicViewportState>(vk, device));
}

Move<VkDynamicRasterState> createDynamicRasterState (const DeviceInterface& vk, VkDevice device, const VkDynamicRasterStateCreateInfo* pCreateInfo)
{
	VkDynamicRasterState object = 0;
	VK_CHECK(vk.createDynamicRasterState(device, pCreateInfo, &object));
	return Move<VkDynamicRasterState>(check<VkDynamicRasterState>(object), Deleter<VkDynamicRasterState>(vk, device));
}

Move<VkDynamicColorBlendState> createDynamicColorBlendState (const DeviceInterface& vk, VkDevice device, const VkDynamicColorBlendStateCreateInfo* pCreateInfo)
{
	VkDynamicColorBlendState object = 0;
	VK_CHECK(vk.createDynamicColorBlendState(device, pCreateInfo, &object));
	return Move<VkDynamicColorBlendState>(check<VkDynamicColorBlendState>(object), Deleter<VkDynamicColorBlendState>(vk, device));
}

Move<VkDynamicDepthStencilState> createDynamicDepthStencilState (const DeviceInterface& vk, VkDevice device, const VkDynamicDepthStencilStateCreateInfo* pCreateInfo)
{
	VkDynamicDepthStencilState object = 0;
	VK_CHECK(vk.createDynamicDepthStencilState(device, pCreateInfo, &object));
	return Move<VkDynamicDepthStencilState>(check<VkDynamicDepthStencilState>(object), Deleter<VkDynamicDepthStencilState>(vk, device));
}

Move<VkFramebuffer> createFramebuffer (const DeviceInterface& vk, VkDevice device, const VkFramebufferCreateInfo* pCreateInfo)
{
	VkFramebuffer object = 0;
	VK_CHECK(vk.createFramebuffer(device, pCreateInfo, &object));
	return Move<VkFramebuffer>(check<VkFramebuffer>(object), Deleter<VkFramebuffer>(vk, device));
}

Move<VkRenderPass> createRenderPass (const DeviceInterface& vk, VkDevice device, const VkRenderPassCreateInfo* pCreateInfo)
{
	VkRenderPass object = 0;
	VK_CHECK(vk.createRenderPass(device, pCreateInfo, &object));
	return Move<VkRenderPass>(check<VkRenderPass>(object), Deleter<VkRenderPass>(vk, device));
}

Move<VkCmdPool> createCommandPool (const DeviceInterface& vk, VkDevice device, const VkCmdPoolCreateInfo* pCreateInfo)
{
	VkCmdPool object = 0;
	VK_CHECK(vk.createCommandPool(device, pCreateInfo, &object));
	return Move<VkCmdPool>(check<VkCmdPool>(object), Deleter<VkCmdPool>(vk, device));
}

Move<VkCmdBuffer> createCommandBuffer (const DeviceInterface& vk, VkDevice device, const VkCmdBufferCreateInfo* pCreateInfo)
{
	VkCmdBuffer object = 0;
	VK_CHECK(vk.createCommandBuffer(device, pCreateInfo, &object));
	return Move<VkCmdBuffer>(check<VkCmdBuffer>(object), Deleter<VkCmdBuffer>(vk, device));
}

