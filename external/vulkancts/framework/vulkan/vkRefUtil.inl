/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 */
Move<VkInstanceT>				createInstance						(const PlatformInterface& vk, const VkInstanceCreateInfo* pCreateInfo);
Move<VkDeviceT>					createDevice						(const DeviceInterface& vk, VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo);
Move<VkDeviceMemoryT>			allocMemory							(const DeviceInterface& vk, VkDevice device, const VkMemoryAllocInfo* pAllocInfo);
Move<VkFenceT>					createFence							(const DeviceInterface& vk, VkDevice device, const VkFenceCreateInfo* pCreateInfo);
Move<VkSemaphoreT>				createSemaphore						(const DeviceInterface& vk, VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo);
Move<VkEventT>					createEvent							(const DeviceInterface& vk, VkDevice device, const VkEventCreateInfo* pCreateInfo);
Move<VkQueryPoolT>				createQueryPool						(const DeviceInterface& vk, VkDevice device, const VkQueryPoolCreateInfo* pCreateInfo);
Move<VkBufferT>					createBuffer						(const DeviceInterface& vk, VkDevice device, const VkBufferCreateInfo* pCreateInfo);
Move<VkBufferViewT>				createBufferView					(const DeviceInterface& vk, VkDevice device, const VkBufferViewCreateInfo* pCreateInfo);
Move<VkImageT>					createImage							(const DeviceInterface& vk, VkDevice device, const VkImageCreateInfo* pCreateInfo);
Move<VkImageViewT>				createImageView						(const DeviceInterface& vk, VkDevice device, const VkImageViewCreateInfo* pCreateInfo);
Move<VkColorAttachmentViewT>	createColorAttachmentView			(const DeviceInterface& vk, VkDevice device, const VkColorAttachmentViewCreateInfo* pCreateInfo);
Move<VkDepthStencilViewT>		createDepthStencilView				(const DeviceInterface& vk, VkDevice device, const VkDepthStencilViewCreateInfo* pCreateInfo);
Move<VkShaderT>					createShader						(const DeviceInterface& vk, VkDevice device, const VkShaderCreateInfo* pCreateInfo);
Move<VkPipelineT>				createGraphicsPipeline				(const DeviceInterface& vk, VkDevice device, const VkGraphicsPipelineCreateInfo* pCreateInfo);
Move<VkPipelineT>				createGraphicsPipelineDerivative	(const DeviceInterface& vk, VkDevice device, const VkGraphicsPipelineCreateInfo* pCreateInfo, VkPipeline basePipeline);
Move<VkPipelineT>				createComputePipeline				(const DeviceInterface& vk, VkDevice device, const VkComputePipelineCreateInfo* pCreateInfo);
Move<VkPipelineLayoutT>			createPipelineLayout				(const DeviceInterface& vk, VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo);
Move<VkSamplerT>				createSampler						(const DeviceInterface& vk, VkDevice device, const VkSamplerCreateInfo* pCreateInfo);
Move<VkDescriptorSetLayoutT>	createDescriptorSetLayout			(const DeviceInterface& vk, VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo);
Move<VkDescriptorPoolT>			createDescriptorPool				(const DeviceInterface& vk, VkDevice device, VkDescriptorPoolUsage poolUsage, deUint32 maxSets, const VkDescriptorPoolCreateInfo* pCreateInfo);
Move<VkDynamicVpStateT>			createDynamicViewportState			(const DeviceInterface& vk, VkDevice device, const VkDynamicVpStateCreateInfo* pCreateInfo);
Move<VkDynamicRsStateT>			createDynamicRasterState			(const DeviceInterface& vk, VkDevice device, const VkDynamicRsStateCreateInfo* pCreateInfo);
Move<VkDynamicCbStateT>			createDynamicColorBlendState		(const DeviceInterface& vk, VkDevice device, const VkDynamicCbStateCreateInfo* pCreateInfo);
Move<VkDynamicDsStateT>			createDynamicDepthStencilState		(const DeviceInterface& vk, VkDevice device, const VkDynamicDsStateCreateInfo* pCreateInfo);
Move<VkCmdBufferT>				createCommandBuffer					(const DeviceInterface& vk, VkDevice device, const VkCmdBufferCreateInfo* pCreateInfo);
Move<VkFramebufferT>			createFramebuffer					(const DeviceInterface& vk, VkDevice device, const VkFramebufferCreateInfo* pCreateInfo);
Move<VkRenderPassT>				createRenderPass					(const DeviceInterface& vk, VkDevice device, const VkRenderPassCreateInfo* pCreateInfo);
