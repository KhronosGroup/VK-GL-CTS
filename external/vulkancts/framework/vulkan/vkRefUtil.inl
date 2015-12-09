/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 */
Move<VkInstance>			createInstance				(const PlatformInterface& vk, const VkInstanceCreateInfo* pCreateInfo);
Move<VkDevice>				createDevice				(const InstanceInterface& vk, VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo);
Move<VkDeviceMemory>		allocateMemory				(const DeviceInterface& vk, VkDevice device, const VkMemoryAllocateInfo* pAllocateInfo);
Move<VkFence>				createFence					(const DeviceInterface& vk, VkDevice device, const VkFenceCreateInfo* pCreateInfo);
Move<VkSemaphore>			createSemaphore				(const DeviceInterface& vk, VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo);
Move<VkEvent>				createEvent					(const DeviceInterface& vk, VkDevice device, const VkEventCreateInfo* pCreateInfo);
Move<VkQueryPool>			createQueryPool				(const DeviceInterface& vk, VkDevice device, const VkQueryPoolCreateInfo* pCreateInfo);
Move<VkBuffer>				createBuffer				(const DeviceInterface& vk, VkDevice device, const VkBufferCreateInfo* pCreateInfo);
Move<VkBufferView>			createBufferView			(const DeviceInterface& vk, VkDevice device, const VkBufferViewCreateInfo* pCreateInfo);
Move<VkImage>				createImage					(const DeviceInterface& vk, VkDevice device, const VkImageCreateInfo* pCreateInfo);
Move<VkImageView>			createImageView				(const DeviceInterface& vk, VkDevice device, const VkImageViewCreateInfo* pCreateInfo);
Move<VkShaderModule>		createShaderModule			(const DeviceInterface& vk, VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo);
Move<VkPipelineCache>		createPipelineCache			(const DeviceInterface& vk, VkDevice device, const VkPipelineCacheCreateInfo* pCreateInfo);
Move<VkPipeline>			createGraphicsPipelines		(const DeviceInterface& vk, VkDevice device, VkPipelineCache pipelineCache, deUint32 createInfoCount, const VkGraphicsPipelineCreateInfo* pCreateInfos);
Move<VkPipeline>			createComputePipelines		(const DeviceInterface& vk, VkDevice device, VkPipelineCache pipelineCache, deUint32 createInfoCount, const VkComputePipelineCreateInfo* pCreateInfos);
Move<VkPipelineLayout>		createPipelineLayout		(const DeviceInterface& vk, VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo);
Move<VkSampler>				createSampler				(const DeviceInterface& vk, VkDevice device, const VkSamplerCreateInfo* pCreateInfo);
Move<VkDescriptorSetLayout>	createDescriptorSetLayout	(const DeviceInterface& vk, VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo);
Move<VkDescriptorPool>		createDescriptorPool		(const DeviceInterface& vk, VkDevice device, const VkDescriptorPoolCreateInfo* pCreateInfo);
Move<VkFramebuffer>			createFramebuffer			(const DeviceInterface& vk, VkDevice device, const VkFramebufferCreateInfo* pCreateInfo);
Move<VkRenderPass>			createRenderPass			(const DeviceInterface& vk, VkDevice device, const VkRenderPassCreateInfo* pCreateInfo);
Move<VkCommandPool>			createCommandPool			(const DeviceInterface& vk, VkDevice device, const VkCommandPoolCreateInfo* pCreateInfo);
