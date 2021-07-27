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

VKAPI_ATTR VkResult VKAPI_CALL createShaderModule (VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkShaderModule* pShaderModule)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pShaderModule = allocateNonDispHandle<ShaderModule, VkShaderModule>(device, pCreateInfo, pAllocator)));
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

VKAPI_ATTR VkResult VKAPI_CALL createDescriptorUpdateTemplate (VkDevice device, const VkDescriptorUpdateTemplateCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorUpdateTemplate* pDescriptorUpdateTemplate)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pDescriptorUpdateTemplate = allocateNonDispHandle<DescriptorUpdateTemplate, VkDescriptorUpdateTemplate>(device, pCreateInfo, pAllocator)));
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

VKAPI_ATTR VkResult VKAPI_CALL createDeferredOperationKHR (VkDevice device, const VkAllocationCallbacks* pAllocator, VkDeferredOperationKHR* pDeferredOperation)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pDeferredOperation = allocateNonDispHandle<DeferredOperationKHR, VkDeferredOperationKHR>(device, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createDebugReportCallbackEXT (VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugReportCallbackEXT* pCallback)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pCallback = allocateNonDispHandle<DebugReportCallbackEXT, VkDebugReportCallbackEXT>(instance, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createCuModuleNVX (VkDevice device, const VkCuModuleCreateInfoNVX* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkCuModuleNVX* pModule)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pModule = allocateNonDispHandle<CuModuleNVX, VkCuModuleNVX>(device, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createCuFunctionNVX (VkDevice device, const VkCuFunctionCreateInfoNVX* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkCuFunctionNVX* pFunction)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pFunction = allocateNonDispHandle<CuFunctionNVX, VkCuFunctionNVX>(device, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createDebugUtilsMessengerEXT (VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pMessenger)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pMessenger = allocateNonDispHandle<DebugUtilsMessengerEXT, VkDebugUtilsMessengerEXT>(instance, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createValidationCacheEXT (VkDevice device, const VkValidationCacheCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkValidationCacheEXT* pValidationCache)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pValidationCache = allocateNonDispHandle<ValidationCacheEXT, VkValidationCacheEXT>(device, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createAccelerationStructureNV (VkDevice device, const VkAccelerationStructureCreateInfoNV* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkAccelerationStructureNV* pAccelerationStructure)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pAccelerationStructure = allocateNonDispHandle<AccelerationStructureNV, VkAccelerationStructureNV>(device, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createHeadlessSurfaceEXT (VkInstance instance, const VkHeadlessSurfaceCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pSurface = allocateNonDispHandle<SurfaceKHR, VkSurfaceKHR>(instance, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createIndirectCommandsLayoutNV (VkDevice device, const VkIndirectCommandsLayoutCreateInfoNV* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkIndirectCommandsLayoutNV* pIndirectCommandsLayout)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pIndirectCommandsLayout = allocateNonDispHandle<IndirectCommandsLayoutNV, VkIndirectCommandsLayoutNV>(device, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createPrivateDataSlotEXT (VkDevice device, const VkPrivateDataSlotCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPrivateDataSlotEXT* pPrivateDataSlot)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pPrivateDataSlot = allocateNonDispHandle<PrivateDataSlotEXT, VkPrivateDataSlotEXT>(device, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createAccelerationStructureKHR (VkDevice device, const VkAccelerationStructureCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkAccelerationStructureKHR* pAccelerationStructure)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pAccelerationStructure = allocateNonDispHandle<AccelerationStructureKHR, VkAccelerationStructureKHR>(device, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createAndroidSurfaceKHR (VkInstance instance, const VkAndroidSurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pSurface = allocateNonDispHandle<SurfaceKHR, VkSurfaceKHR>(instance, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createVideoSessionKHR (VkDevice device, const VkVideoSessionCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkVideoSessionKHR* pVideoSession)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pVideoSession = allocateNonDispHandle<VideoSessionKHR, VkVideoSessionKHR>(device, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createVideoSessionParametersKHR (VkDevice device, const VkVideoSessionParametersCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkVideoSessionParametersKHR* pVideoSessionParameters)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pVideoSessionParameters = allocateNonDispHandle<VideoSessionParametersKHR, VkVideoSessionParametersKHR>(device, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createImagePipeSurfaceFUCHSIA (VkInstance instance, const VkImagePipeSurfaceCreateInfoFUCHSIA* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pSurface = allocateNonDispHandle<SurfaceKHR, VkSurfaceKHR>(instance, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createStreamDescriptorSurfaceGGP (VkInstance instance, const VkStreamDescriptorSurfaceCreateInfoGGP* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pSurface = allocateNonDispHandle<SurfaceKHR, VkSurfaceKHR>(instance, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createIOSSurfaceMVK (VkInstance instance, const VkIOSSurfaceCreateInfoMVK* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pSurface = allocateNonDispHandle<SurfaceKHR, VkSurfaceKHR>(instance, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createMacOSSurfaceMVK (VkInstance instance, const VkMacOSSurfaceCreateInfoMVK* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pSurface = allocateNonDispHandle<SurfaceKHR, VkSurfaceKHR>(instance, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createMetalSurfaceEXT (VkInstance instance, const VkMetalSurfaceCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pSurface = allocateNonDispHandle<SurfaceKHR, VkSurfaceKHR>(instance, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createViSurfaceNN (VkInstance instance, const VkViSurfaceCreateInfoNN* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pSurface = allocateNonDispHandle<SurfaceKHR, VkSurfaceKHR>(instance, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createWaylandSurfaceKHR (VkInstance instance, const VkWaylandSurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pSurface = allocateNonDispHandle<SurfaceKHR, VkSurfaceKHR>(instance, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createWin32SurfaceKHR (VkInstance instance, const VkWin32SurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pSurface = allocateNonDispHandle<SurfaceKHR, VkSurfaceKHR>(instance, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createXcbSurfaceKHR (VkInstance instance, const VkXcbSurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pSurface = allocateNonDispHandle<SurfaceKHR, VkSurfaceKHR>(instance, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createXlibSurfaceKHR (VkInstance instance, const VkXlibSurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface)
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

VKAPI_ATTR void VKAPI_CALL freeMemory (VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	freeNonDispHandle<DeviceMemory, VkDeviceMemory>(memory, pAllocator);
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

VKAPI_ATTR void VKAPI_CALL destroyQueryPool (VkDevice device, VkQueryPool queryPool, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	freeNonDispHandle<QueryPool, VkQueryPool>(queryPool, pAllocator);
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

VKAPI_ATTR void VKAPI_CALL destroyShaderModule (VkDevice device, VkShaderModule shaderModule, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	freeNonDispHandle<ShaderModule, VkShaderModule>(shaderModule, pAllocator);
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

VKAPI_ATTR void VKAPI_CALL destroyDescriptorPool (VkDevice device, VkDescriptorPool descriptorPool, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	freeNonDispHandle<DescriptorPool, VkDescriptorPool>(descriptorPool, pAllocator);
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

VKAPI_ATTR void VKAPI_CALL destroyCommandPool (VkDevice device, VkCommandPool commandPool, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	freeNonDispHandle<CommandPool, VkCommandPool>(commandPool, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL destroySamplerYcbcrConversion (VkDevice device, VkSamplerYcbcrConversion ycbcrConversion, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	freeNonDispHandle<SamplerYcbcrConversion, VkSamplerYcbcrConversion>(ycbcrConversion, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL destroyDescriptorUpdateTemplate (VkDevice device, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	freeNonDispHandle<DescriptorUpdateTemplate, VkDescriptorUpdateTemplate>(descriptorUpdateTemplate, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL destroySurfaceKHR (VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(instance);
	freeNonDispHandle<SurfaceKHR, VkSurfaceKHR>(surface, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL destroySwapchainKHR (VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	freeNonDispHandle<SwapchainKHR, VkSwapchainKHR>(swapchain, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL destroyDeferredOperationKHR (VkDevice device, VkDeferredOperationKHR operation, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	freeNonDispHandle<DeferredOperationKHR, VkDeferredOperationKHR>(operation, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL destroyDebugReportCallbackEXT (VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(instance);
	freeNonDispHandle<DebugReportCallbackEXT, VkDebugReportCallbackEXT>(callback, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL destroyCuModuleNVX (VkDevice device, VkCuModuleNVX module, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	freeNonDispHandle<CuModuleNVX, VkCuModuleNVX>(module, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL destroyCuFunctionNVX (VkDevice device, VkCuFunctionNVX function, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	freeNonDispHandle<CuFunctionNVX, VkCuFunctionNVX>(function, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL destroyDebugUtilsMessengerEXT (VkInstance instance, VkDebugUtilsMessengerEXT messenger, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(instance);
	freeNonDispHandle<DebugUtilsMessengerEXT, VkDebugUtilsMessengerEXT>(messenger, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL destroyValidationCacheEXT (VkDevice device, VkValidationCacheEXT validationCache, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	freeNonDispHandle<ValidationCacheEXT, VkValidationCacheEXT>(validationCache, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL destroyAccelerationStructureNV (VkDevice device, VkAccelerationStructureNV accelerationStructure, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	freeNonDispHandle<AccelerationStructureNV, VkAccelerationStructureNV>(accelerationStructure, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL destroyIndirectCommandsLayoutNV (VkDevice device, VkIndirectCommandsLayoutNV indirectCommandsLayout, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	freeNonDispHandle<IndirectCommandsLayoutNV, VkIndirectCommandsLayoutNV>(indirectCommandsLayout, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL destroyPrivateDataSlotEXT (VkDevice device, VkPrivateDataSlotEXT privateDataSlot, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	freeNonDispHandle<PrivateDataSlotEXT, VkPrivateDataSlotEXT>(privateDataSlot, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL destroyAccelerationStructureKHR (VkDevice device, VkAccelerationStructureKHR accelerationStructure, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	freeNonDispHandle<AccelerationStructureKHR, VkAccelerationStructureKHR>(accelerationStructure, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL destroyVideoSessionKHR (VkDevice device, VkVideoSessionKHR videoSession, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	freeNonDispHandle<VideoSessionKHR, VkVideoSessionKHR>(videoSession, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL destroyVideoSessionParametersKHR (VkDevice device, VkVideoSessionParametersKHR videoSessionParameters, const VkAllocationCallbacks* pAllocator)
{
	DE_UNREF(device);
	freeNonDispHandle<VideoSessionParametersKHR, VkVideoSessionParametersKHR>(videoSessionParameters, pAllocator);
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

VKAPI_ATTR void VKAPI_CALL getImageSparseMemoryRequirements2 (VkDevice device, const VkImageSparseMemoryRequirementsInfo2* pInfo, deUint32* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements)
{
	DE_UNREF(device);
	DE_UNREF(pInfo);
	DE_UNREF(pSparseMemoryRequirementCount);
	DE_UNREF(pSparseMemoryRequirements);
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

VKAPI_ATTR void VKAPI_CALL getPhysicalDeviceSparseImageFormatProperties2 (VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSparseImageFormatInfo2* pFormatInfo, deUint32* pPropertyCount, VkSparseImageFormatProperties2* pProperties)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(pFormatInfo);
	DE_UNREF(pPropertyCount);
	DE_UNREF(pProperties);
}

VKAPI_ATTR void VKAPI_CALL trimCommandPool (VkDevice device, VkCommandPool commandPool, VkCommandPoolTrimFlags flags)
{
	DE_UNREF(device);
	DE_UNREF(commandPool);
	DE_UNREF(flags);
}

VKAPI_ATTR void VKAPI_CALL getDeviceQueue2 (VkDevice device, const VkDeviceQueueInfo2* pQueueInfo, VkQueue* pQueue)
{
	DE_UNREF(device);
	DE_UNREF(pQueueInfo);
	DE_UNREF(pQueue);
}

VKAPI_ATTR void VKAPI_CALL updateDescriptorSetWithTemplate (VkDevice device, VkDescriptorSet descriptorSet, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const void* pData)
{
	DE_UNREF(device);
	DE_UNREF(descriptorSet);
	DE_UNREF(descriptorUpdateTemplate);
	DE_UNREF(pData);
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

VKAPI_ATTR void VKAPI_CALL cmdPushDescriptorSetKHR (VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, deUint32 set, deUint32 descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pipelineBindPoint);
	DE_UNREF(layout);
	DE_UNREF(set);
	DE_UNREF(descriptorWriteCount);
	DE_UNREF(pDescriptorWrites);
}

VKAPI_ATTR void VKAPI_CALL cmdPushDescriptorSetWithTemplateKHR (VkCommandBuffer commandBuffer, VkDescriptorUpdateTemplate descriptorUpdateTemplate, VkPipelineLayout layout, deUint32 set, const void* pData)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(descriptorUpdateTemplate);
	DE_UNREF(layout);
	DE_UNREF(set);
	DE_UNREF(pData);
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

VKAPI_ATTR VkResult VKAPI_CALL waitForPresentKHR (VkDevice device, VkSwapchainKHR swapchain, deUint64 presentId, deUint64 timeout)
{
	DE_UNREF(device);
	DE_UNREF(swapchain);
	DE_UNREF(presentId);
	DE_UNREF(timeout);
	return VK_SUCCESS;
}

VKAPI_ATTR uint32_t VKAPI_CALL getDeferredOperationMaxConcurrencyKHR (VkDevice device, VkDeferredOperationKHR operation)
{
	DE_UNREF(device);
	DE_UNREF(operation);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getDeferredOperationResultKHR (VkDevice device, VkDeferredOperationKHR operation)
{
	DE_UNREF(device);
	DE_UNREF(operation);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL deferredOperationJoinKHR (VkDevice device, VkDeferredOperationKHR operation)
{
	DE_UNREF(device);
	DE_UNREF(operation);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getPipelineExecutablePropertiesKHR (VkDevice device, const VkPipelineInfoKHR* pPipelineInfo, deUint32* pExecutableCount, VkPipelineExecutablePropertiesKHR* pProperties)
{
	DE_UNREF(device);
	DE_UNREF(pPipelineInfo);
	DE_UNREF(pExecutableCount);
	DE_UNREF(pProperties);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getPipelineExecutableStatisticsKHR (VkDevice device, const VkPipelineExecutableInfoKHR* pExecutableInfo, deUint32* pStatisticCount, VkPipelineExecutableStatisticKHR* pStatistics)
{
	DE_UNREF(device);
	DE_UNREF(pExecutableInfo);
	DE_UNREF(pStatisticCount);
	DE_UNREF(pStatistics);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getPipelineExecutableInternalRepresentationsKHR (VkDevice device, const VkPipelineExecutableInfoKHR* pExecutableInfo, deUint32* pInternalRepresentationCount, VkPipelineExecutableInternalRepresentationKHR* pInternalRepresentations)
{
	DE_UNREF(device);
	DE_UNREF(pExecutableInfo);
	DE_UNREF(pInternalRepresentationCount);
	DE_UNREF(pInternalRepresentations);
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

VKAPI_ATTR void VKAPI_CALL debugReportMessageEXT (VkInstance instance, VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, deUint64 object, deUintptr location, deInt32 messageCode, const char* pLayerPrefix, const char* pMessage)
{
	DE_UNREF(instance);
	DE_UNREF(flags);
	DE_UNREF(objectType);
	DE_UNREF(object);
	DE_UNREF(location);
	DE_UNREF(messageCode);
	DE_UNREF(pLayerPrefix);
	DE_UNREF(pMessage);
}

VKAPI_ATTR VkResult VKAPI_CALL debugMarkerSetObjectTagEXT (VkDevice device, const VkDebugMarkerObjectTagInfoEXT* pTagInfo)
{
	DE_UNREF(device);
	DE_UNREF(pTagInfo);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL debugMarkerSetObjectNameEXT (VkDevice device, const VkDebugMarkerObjectNameInfoEXT* pNameInfo)
{
	DE_UNREF(device);
	DE_UNREF(pNameInfo);
	return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL cmdDebugMarkerBeginEXT (VkCommandBuffer commandBuffer, const VkDebugMarkerMarkerInfoEXT* pMarkerInfo)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pMarkerInfo);
}

VKAPI_ATTR void VKAPI_CALL cmdDebugMarkerEndEXT (VkCommandBuffer commandBuffer)
{
	DE_UNREF(commandBuffer);
}

VKAPI_ATTR void VKAPI_CALL cmdDebugMarkerInsertEXT (VkCommandBuffer commandBuffer, const VkDebugMarkerMarkerInfoEXT* pMarkerInfo)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pMarkerInfo);
}

VKAPI_ATTR void VKAPI_CALL cmdBindTransformFeedbackBuffersEXT (VkCommandBuffer commandBuffer, deUint32 firstBinding, deUint32 bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(firstBinding);
	DE_UNREF(bindingCount);
	DE_UNREF(pBuffers);
	DE_UNREF(pOffsets);
	DE_UNREF(pSizes);
}

VKAPI_ATTR void VKAPI_CALL cmdBeginTransformFeedbackEXT (VkCommandBuffer commandBuffer, deUint32 firstCounterBuffer, deUint32 counterBufferCount, const VkBuffer* pCounterBuffers, const VkDeviceSize* pCounterBufferOffsets)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(firstCounterBuffer);
	DE_UNREF(counterBufferCount);
	DE_UNREF(pCounterBuffers);
	DE_UNREF(pCounterBufferOffsets);
}

VKAPI_ATTR void VKAPI_CALL cmdEndTransformFeedbackEXT (VkCommandBuffer commandBuffer, deUint32 firstCounterBuffer, deUint32 counterBufferCount, const VkBuffer* pCounterBuffers, const VkDeviceSize* pCounterBufferOffsets)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(firstCounterBuffer);
	DE_UNREF(counterBufferCount);
	DE_UNREF(pCounterBuffers);
	DE_UNREF(pCounterBufferOffsets);
}

VKAPI_ATTR void VKAPI_CALL cmdBeginQueryIndexedEXT (VkCommandBuffer commandBuffer, VkQueryPool queryPool, deUint32 query, VkQueryControlFlags flags, deUint32 index)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(queryPool);
	DE_UNREF(query);
	DE_UNREF(flags);
	DE_UNREF(index);
}

VKAPI_ATTR void VKAPI_CALL cmdEndQueryIndexedEXT (VkCommandBuffer commandBuffer, VkQueryPool queryPool, deUint32 query, deUint32 index)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(queryPool);
	DE_UNREF(query);
	DE_UNREF(index);
}

VKAPI_ATTR void VKAPI_CALL cmdDrawIndirectByteCountEXT (VkCommandBuffer commandBuffer, deUint32 instanceCount, deUint32 firstInstance, VkBuffer counterBuffer, VkDeviceSize counterBufferOffset, deUint32 counterOffset, deUint32 vertexStride)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(instanceCount);
	DE_UNREF(firstInstance);
	DE_UNREF(counterBuffer);
	DE_UNREF(counterBufferOffset);
	DE_UNREF(counterOffset);
	DE_UNREF(vertexStride);
}

VKAPI_ATTR void VKAPI_CALL cmdCuLaunchKernelNVX (VkCommandBuffer commandBuffer, const VkCuLaunchInfoNVX* pLaunchInfo)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pLaunchInfo);
}

VKAPI_ATTR uint32_t VKAPI_CALL getImageViewHandleNVX (VkDevice device, const VkImageViewHandleInfoNVX* pInfo)
{
	DE_UNREF(device);
	DE_UNREF(pInfo);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getImageViewAddressNVX (VkDevice device, VkImageView imageView, VkImageViewAddressPropertiesNVX* pProperties)
{
	DE_UNREF(device);
	DE_UNREF(imageView);
	DE_UNREF(pProperties);
	return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL cmdDrawIndirectCountAMD (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, deUint32 maxDrawCount, deUint32 stride)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(buffer);
	DE_UNREF(offset);
	DE_UNREF(countBuffer);
	DE_UNREF(countBufferOffset);
	DE_UNREF(maxDrawCount);
	DE_UNREF(stride);
}

VKAPI_ATTR void VKAPI_CALL cmdDrawIndexedIndirectCountAMD (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, deUint32 maxDrawCount, deUint32 stride)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(buffer);
	DE_UNREF(offset);
	DE_UNREF(countBuffer);
	DE_UNREF(countBufferOffset);
	DE_UNREF(maxDrawCount);
	DE_UNREF(stride);
}

VKAPI_ATTR VkResult VKAPI_CALL getShaderInfoAMD (VkDevice device, VkPipeline pipeline, VkShaderStageFlagBits shaderStage, VkShaderInfoTypeAMD infoType, deUintptr* pInfoSize, void* pInfo)
{
	DE_UNREF(device);
	DE_UNREF(pipeline);
	DE_UNREF(shaderStage);
	DE_UNREF(infoType);
	DE_UNREF(pInfoSize);
	DE_UNREF(pInfo);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getPhysicalDeviceExternalImageFormatPropertiesNV (VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkExternalMemoryHandleTypeFlagsNV externalHandleType, VkExternalImageFormatPropertiesNV* pExternalImageFormatProperties)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(format);
	DE_UNREF(type);
	DE_UNREF(tiling);
	DE_UNREF(usage);
	DE_UNREF(flags);
	DE_UNREF(externalHandleType);
	DE_UNREF(pExternalImageFormatProperties);
	return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL cmdBeginConditionalRenderingEXT (VkCommandBuffer commandBuffer, const VkConditionalRenderingBeginInfoEXT* pConditionalRenderingBegin)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pConditionalRenderingBegin);
}

VKAPI_ATTR void VKAPI_CALL cmdEndConditionalRenderingEXT (VkCommandBuffer commandBuffer)
{
	DE_UNREF(commandBuffer);
}

VKAPI_ATTR void VKAPI_CALL cmdSetViewportWScalingNV (VkCommandBuffer commandBuffer, deUint32 firstViewport, deUint32 viewportCount, const VkViewportWScalingNV* pViewportWScalings)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(firstViewport);
	DE_UNREF(viewportCount);
	DE_UNREF(pViewportWScalings);
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

VKAPI_ATTR VkResult VKAPI_CALL getRefreshCycleDurationGOOGLE (VkDevice device, VkSwapchainKHR swapchain, VkRefreshCycleDurationGOOGLE* pDisplayTimingProperties)
{
	DE_UNREF(device);
	DE_UNREF(swapchain);
	DE_UNREF(pDisplayTimingProperties);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getPastPresentationTimingGOOGLE (VkDevice device, VkSwapchainKHR swapchain, deUint32* pPresentationTimingCount, VkPastPresentationTimingGOOGLE* pPresentationTimings)
{
	DE_UNREF(device);
	DE_UNREF(swapchain);
	DE_UNREF(pPresentationTimingCount);
	DE_UNREF(pPresentationTimings);
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

VKAPI_ATTR VkResult VKAPI_CALL mergeValidationCachesEXT (VkDevice device, VkValidationCacheEXT dstCache, deUint32 srcCacheCount, const VkValidationCacheEXT* pSrcCaches)
{
	DE_UNREF(device);
	DE_UNREF(dstCache);
	DE_UNREF(srcCacheCount);
	DE_UNREF(pSrcCaches);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getValidationCacheDataEXT (VkDevice device, VkValidationCacheEXT validationCache, deUintptr* pDataSize, void* pData)
{
	DE_UNREF(device);
	DE_UNREF(validationCache);
	DE_UNREF(pDataSize);
	DE_UNREF(pData);
	return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL cmdBindShadingRateImageNV (VkCommandBuffer commandBuffer, VkImageView imageView, VkImageLayout imageLayout)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(imageView);
	DE_UNREF(imageLayout);
}

VKAPI_ATTR void VKAPI_CALL cmdSetViewportShadingRatePaletteNV (VkCommandBuffer commandBuffer, deUint32 firstViewport, deUint32 viewportCount, const VkShadingRatePaletteNV* pShadingRatePalettes)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(firstViewport);
	DE_UNREF(viewportCount);
	DE_UNREF(pShadingRatePalettes);
}

VKAPI_ATTR void VKAPI_CALL cmdSetCoarseSampleOrderNV (VkCommandBuffer commandBuffer, VkCoarseSampleOrderTypeNV sampleOrderType, deUint32 customSampleOrderCount, const VkCoarseSampleOrderCustomNV* pCustomSampleOrders)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(sampleOrderType);
	DE_UNREF(customSampleOrderCount);
	DE_UNREF(pCustomSampleOrders);
}

VKAPI_ATTR void VKAPI_CALL getAccelerationStructureMemoryRequirementsNV (VkDevice device, const VkAccelerationStructureMemoryRequirementsInfoNV* pInfo, VkMemoryRequirements2KHR* pMemoryRequirements)
{
	DE_UNREF(device);
	DE_UNREF(pInfo);
	DE_UNREF(pMemoryRequirements);
}

VKAPI_ATTR VkResult VKAPI_CALL bindAccelerationStructureMemoryNV (VkDevice device, deUint32 bindInfoCount, const VkBindAccelerationStructureMemoryInfoNV* pBindInfos)
{
	DE_UNREF(device);
	DE_UNREF(bindInfoCount);
	DE_UNREF(pBindInfos);
	return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL cmdBuildAccelerationStructureNV (VkCommandBuffer commandBuffer, const VkAccelerationStructureInfoNV* pInfo, VkBuffer instanceData, VkDeviceSize instanceOffset, VkBool32 update, VkAccelerationStructureNV dst, VkAccelerationStructureNV src, VkBuffer scratch, VkDeviceSize scratchOffset)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pInfo);
	DE_UNREF(instanceData);
	DE_UNREF(instanceOffset);
	DE_UNREF(update);
	DE_UNREF(dst);
	DE_UNREF(src);
	DE_UNREF(scratch);
	DE_UNREF(scratchOffset);
}

VKAPI_ATTR void VKAPI_CALL cmdCopyAccelerationStructureNV (VkCommandBuffer commandBuffer, VkAccelerationStructureNV dst, VkAccelerationStructureNV src, VkCopyAccelerationStructureModeKHR mode)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(dst);
	DE_UNREF(src);
	DE_UNREF(mode);
}

VKAPI_ATTR void VKAPI_CALL cmdTraceRaysNV (VkCommandBuffer commandBuffer, VkBuffer raygenShaderBindingTableBuffer, VkDeviceSize raygenShaderBindingOffset, VkBuffer missShaderBindingTableBuffer, VkDeviceSize missShaderBindingOffset, VkDeviceSize missShaderBindingStride, VkBuffer hitShaderBindingTableBuffer, VkDeviceSize hitShaderBindingOffset, VkDeviceSize hitShaderBindingStride, VkBuffer callableShaderBindingTableBuffer, VkDeviceSize callableShaderBindingOffset, VkDeviceSize callableShaderBindingStride, deUint32 width, deUint32 height, deUint32 depth)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(raygenShaderBindingTableBuffer);
	DE_UNREF(raygenShaderBindingOffset);
	DE_UNREF(missShaderBindingTableBuffer);
	DE_UNREF(missShaderBindingOffset);
	DE_UNREF(missShaderBindingStride);
	DE_UNREF(hitShaderBindingTableBuffer);
	DE_UNREF(hitShaderBindingOffset);
	DE_UNREF(hitShaderBindingStride);
	DE_UNREF(callableShaderBindingTableBuffer);
	DE_UNREF(callableShaderBindingOffset);
	DE_UNREF(callableShaderBindingStride);
	DE_UNREF(width);
	DE_UNREF(height);
	DE_UNREF(depth);
}

VKAPI_ATTR VkResult VKAPI_CALL getRayTracingShaderGroupHandlesKHR (VkDevice device, VkPipeline pipeline, deUint32 firstGroup, deUint32 groupCount, deUintptr dataSize, void* pData)
{
	DE_UNREF(device);
	DE_UNREF(pipeline);
	DE_UNREF(firstGroup);
	DE_UNREF(groupCount);
	DE_UNREF(dataSize);
	DE_UNREF(pData);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getRayTracingShaderGroupHandlesNV (VkDevice device, VkPipeline pipeline, deUint32 firstGroup, deUint32 groupCount, deUintptr dataSize, void* pData)
{
	DE_UNREF(device);
	DE_UNREF(pipeline);
	DE_UNREF(firstGroup);
	DE_UNREF(groupCount);
	DE_UNREF(dataSize);
	DE_UNREF(pData);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getAccelerationStructureHandleNV (VkDevice device, VkAccelerationStructureNV accelerationStructure, deUintptr dataSize, void* pData)
{
	DE_UNREF(device);
	DE_UNREF(accelerationStructure);
	DE_UNREF(dataSize);
	DE_UNREF(pData);
	return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL cmdWriteAccelerationStructuresPropertiesNV (VkCommandBuffer commandBuffer, deUint32 accelerationStructureCount, const VkAccelerationStructureNV* pAccelerationStructures, VkQueryType queryType, VkQueryPool queryPool, deUint32 firstQuery)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(accelerationStructureCount);
	DE_UNREF(pAccelerationStructures);
	DE_UNREF(queryType);
	DE_UNREF(queryPool);
	DE_UNREF(firstQuery);
}

VKAPI_ATTR VkResult VKAPI_CALL compileDeferredNV (VkDevice device, VkPipeline pipeline, deUint32 shader)
{
	DE_UNREF(device);
	DE_UNREF(pipeline);
	DE_UNREF(shader);
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

VKAPI_ATTR void VKAPI_CALL cmdWriteBufferMarkerAMD (VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage, VkBuffer dstBuffer, VkDeviceSize dstOffset, deUint32 marker)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pipelineStage);
	DE_UNREF(dstBuffer);
	DE_UNREF(dstOffset);
	DE_UNREF(marker);
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

VKAPI_ATTR void VKAPI_CALL cmdDrawMeshTasksNV (VkCommandBuffer commandBuffer, deUint32 taskCount, deUint32 firstTask)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(taskCount);
	DE_UNREF(firstTask);
}

VKAPI_ATTR void VKAPI_CALL cmdDrawMeshTasksIndirectNV (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, deUint32 drawCount, deUint32 stride)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(buffer);
	DE_UNREF(offset);
	DE_UNREF(drawCount);
	DE_UNREF(stride);
}

VKAPI_ATTR void VKAPI_CALL cmdDrawMeshTasksIndirectCountNV (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, deUint32 maxDrawCount, deUint32 stride)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(buffer);
	DE_UNREF(offset);
	DE_UNREF(countBuffer);
	DE_UNREF(countBufferOffset);
	DE_UNREF(maxDrawCount);
	DE_UNREF(stride);
}

VKAPI_ATTR void VKAPI_CALL cmdSetExclusiveScissorNV (VkCommandBuffer commandBuffer, deUint32 firstExclusiveScissor, deUint32 exclusiveScissorCount, const VkRect2D* pExclusiveScissors)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(firstExclusiveScissor);
	DE_UNREF(exclusiveScissorCount);
	DE_UNREF(pExclusiveScissors);
}

VKAPI_ATTR void VKAPI_CALL cmdSetCheckpointNV (VkCommandBuffer commandBuffer, const void* pCheckpointMarker)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pCheckpointMarker);
}

VKAPI_ATTR void VKAPI_CALL getQueueCheckpointDataNV (VkQueue queue, deUint32* pCheckpointDataCount, VkCheckpointDataNV* pCheckpointData)
{
	DE_UNREF(queue);
	DE_UNREF(pCheckpointDataCount);
	DE_UNREF(pCheckpointData);
}

VKAPI_ATTR VkResult VKAPI_CALL initializePerformanceApiINTEL (VkDevice device, const VkInitializePerformanceApiInfoINTEL* pInitializeInfo)
{
	DE_UNREF(device);
	DE_UNREF(pInitializeInfo);
	return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL uninitializePerformanceApiINTEL (VkDevice device)
{
	DE_UNREF(device);
}

VKAPI_ATTR VkResult VKAPI_CALL cmdSetPerformanceMarkerINTEL (VkCommandBuffer commandBuffer, const VkPerformanceMarkerInfoINTEL* pMarkerInfo)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pMarkerInfo);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL cmdSetPerformanceStreamMarkerINTEL (VkCommandBuffer commandBuffer, const VkPerformanceStreamMarkerInfoINTEL* pMarkerInfo)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pMarkerInfo);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL cmdSetPerformanceOverrideINTEL (VkCommandBuffer commandBuffer, const VkPerformanceOverrideInfoINTEL* pOverrideInfo)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pOverrideInfo);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL acquirePerformanceConfigurationINTEL (VkDevice device, const VkPerformanceConfigurationAcquireInfoINTEL* pAcquireInfo, VkPerformanceConfigurationINTEL* pConfiguration)
{
	DE_UNREF(device);
	DE_UNREF(pAcquireInfo);
	DE_UNREF(pConfiguration);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL releasePerformanceConfigurationINTEL (VkDevice device, VkPerformanceConfigurationINTEL configuration)
{
	DE_UNREF(device);
	DE_UNREF(configuration);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL queueSetPerformanceConfigurationINTEL (VkQueue queue, VkPerformanceConfigurationINTEL configuration)
{
	DE_UNREF(queue);
	DE_UNREF(configuration);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getPerformanceParameterINTEL (VkDevice device, VkPerformanceParameterTypeINTEL parameter, VkPerformanceValueINTEL* pValue)
{
	DE_UNREF(device);
	DE_UNREF(parameter);
	DE_UNREF(pValue);
	return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL setLocalDimmingAMD (VkDevice device, VkSwapchainKHR swapChain, VkBool32 localDimmingEnable)
{
	DE_UNREF(device);
	DE_UNREF(swapChain);
	DE_UNREF(localDimmingEnable);
}

VKAPI_ATTR VkDeviceAddress VKAPI_CALL getBufferDeviceAddressEXT (VkDevice device, const VkBufferDeviceAddressInfo* pInfo)
{
	DE_UNREF(device);
	DE_UNREF(pInfo);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getPhysicalDeviceToolPropertiesEXT (VkPhysicalDevice physicalDevice, deUint32* pToolCount, VkPhysicalDeviceToolPropertiesEXT* pToolProperties)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(pToolCount);
	DE_UNREF(pToolProperties);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getPhysicalDeviceCooperativeMatrixPropertiesNV (VkPhysicalDevice physicalDevice, deUint32* pPropertyCount, VkCooperativeMatrixPropertiesNV* pProperties)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(pPropertyCount);
	DE_UNREF(pProperties);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV (VkPhysicalDevice physicalDevice, deUint32* pCombinationCount, VkFramebufferMixedSamplesCombinationNV* pCombinations)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(pCombinationCount);
	DE_UNREF(pCombinations);
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

VKAPI_ATTR void VKAPI_CALL getGeneratedCommandsMemoryRequirementsNV (VkDevice device, const VkGeneratedCommandsMemoryRequirementsInfoNV* pInfo, VkMemoryRequirements2* pMemoryRequirements)
{
	DE_UNREF(device);
	DE_UNREF(pInfo);
	DE_UNREF(pMemoryRequirements);
}

VKAPI_ATTR void VKAPI_CALL cmdPreprocessGeneratedCommandsNV (VkCommandBuffer commandBuffer, const VkGeneratedCommandsInfoNV* pGeneratedCommandsInfo)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pGeneratedCommandsInfo);
}

VKAPI_ATTR void VKAPI_CALL cmdExecuteGeneratedCommandsNV (VkCommandBuffer commandBuffer, VkBool32 isPreprocessed, const VkGeneratedCommandsInfoNV* pGeneratedCommandsInfo)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(isPreprocessed);
	DE_UNREF(pGeneratedCommandsInfo);
}

VKAPI_ATTR void VKAPI_CALL cmdBindPipelineShaderGroupNV (VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline, deUint32 groupIndex)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pipelineBindPoint);
	DE_UNREF(pipeline);
	DE_UNREF(groupIndex);
}

VKAPI_ATTR VkResult VKAPI_CALL acquireDrmDisplayEXT (VkPhysicalDevice physicalDevice, deInt32 drmFd, VkDisplayKHR display)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(drmFd);
	DE_UNREF(display);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getDrmDisplayEXT (VkPhysicalDevice physicalDevice, deInt32 drmFd, deUint32 connectorId, VkDisplayKHR* display)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(drmFd);
	DE_UNREF(connectorId);
	DE_UNREF(display);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL setPrivateDataEXT (VkDevice device, VkObjectType objectType, deUint64 objectHandle, VkPrivateDataSlotEXT privateDataSlot, deUint64 data)
{
	DE_UNREF(device);
	DE_UNREF(objectType);
	DE_UNREF(objectHandle);
	DE_UNREF(privateDataSlot);
	DE_UNREF(data);
	return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL getPrivateDataEXT (VkDevice device, VkObjectType objectType, deUint64 objectHandle, VkPrivateDataSlotEXT privateDataSlot, deUint64* pData)
{
	DE_UNREF(device);
	DE_UNREF(objectType);
	DE_UNREF(objectHandle);
	DE_UNREF(privateDataSlot);
	DE_UNREF(pData);
}

VKAPI_ATTR void VKAPI_CALL cmdSetFragmentShadingRateEnumNV (VkCommandBuffer commandBuffer, VkFragmentShadingRateNV shadingRate, const VkFragmentShadingRateCombinerOpKHR combinerOps[2])
{
	DE_UNREF(commandBuffer);
	DE_UNREF(shadingRate);
	DE_UNREF(combinerOps);
}

VKAPI_ATTR VkResult VKAPI_CALL acquireWinrtDisplayNV (VkPhysicalDevice physicalDevice, VkDisplayKHR display)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(display);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getWinrtDisplayNV (VkPhysicalDevice physicalDevice, deUint32 deviceRelativeId, VkDisplayKHR* pDisplay)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(deviceRelativeId);
	DE_UNREF(pDisplay);
	return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL cmdSetVertexInputEXT (VkCommandBuffer commandBuffer, deUint32 vertexBindingDescriptionCount, const VkVertexInputBindingDescription2EXT* pVertexBindingDescriptions, deUint32 vertexAttributeDescriptionCount, const VkVertexInputAttributeDescription2EXT* pVertexAttributeDescriptions)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(vertexBindingDescriptionCount);
	DE_UNREF(pVertexBindingDescriptions);
	DE_UNREF(vertexAttributeDescriptionCount);
	DE_UNREF(pVertexAttributeDescriptions);
}

VKAPI_ATTR VkResult VKAPI_CALL getDeviceSubpassShadingMaxWorkgroupSizeHUAWEI (VkDevice device, VkRenderPass renderpass, VkExtent2D* pMaxWorkgroupSize)
{
	DE_UNREF(device);
	DE_UNREF(renderpass);
	DE_UNREF(pMaxWorkgroupSize);
	return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL cmdSubpassShadingHUAWEI (VkCommandBuffer commandBuffer)
{
	DE_UNREF(commandBuffer);
}

VKAPI_ATTR void VKAPI_CALL cmdBindInvocationMaskHUAWEI (VkCommandBuffer commandBuffer, VkImageView imageView, VkImageLayout imageLayout)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(imageView);
	DE_UNREF(imageLayout);
}

VKAPI_ATTR VkResult VKAPI_CALL getMemoryRemoteAddressNV (VkDevice device, const VkMemoryGetRemoteAddressInfoNV* pMemoryGetRemoteAddressInfo, VkRemoteAddressNV* pAddress)
{
	DE_UNREF(device);
	DE_UNREF(pMemoryGetRemoteAddressInfo);
	DE_UNREF(pAddress);
	return VK_SUCCESS;
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

VKAPI_ATTR void VKAPI_CALL cmdDrawMultiEXT (VkCommandBuffer commandBuffer, deUint32 drawCount, const VkMultiDrawInfoEXT* pVertexInfo, deUint32 instanceCount, deUint32 firstInstance, deUint32 stride)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(drawCount);
	DE_UNREF(pVertexInfo);
	DE_UNREF(instanceCount);
	DE_UNREF(firstInstance);
	DE_UNREF(stride);
}

VKAPI_ATTR void VKAPI_CALL cmdDrawMultiIndexedEXT (VkCommandBuffer commandBuffer, deUint32 drawCount, const VkMultiDrawIndexedInfoEXT* pIndexInfo, deUint32 instanceCount, deUint32 firstInstance, deUint32 stride, const deInt32* pVertexOffset)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(drawCount);
	DE_UNREF(pIndexInfo);
	DE_UNREF(instanceCount);
	DE_UNREF(firstInstance);
	DE_UNREF(stride);
	DE_UNREF(pVertexOffset);
}

VKAPI_ATTR void VKAPI_CALL cmdBuildAccelerationStructuresKHR (VkCommandBuffer commandBuffer, deUint32 infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkAccelerationStructureBuildRangeInfoKHR* const* ppBuildRangeInfos)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(infoCount);
	DE_UNREF(pInfos);
	DE_UNREF(ppBuildRangeInfos);
}

VKAPI_ATTR void VKAPI_CALL cmdBuildAccelerationStructuresIndirectKHR (VkCommandBuffer commandBuffer, deUint32 infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkDeviceAddress* pIndirectDeviceAddresses, const deUint32* pIndirectStrides, const deUint32* const* ppMaxPrimitiveCounts)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(infoCount);
	DE_UNREF(pInfos);
	DE_UNREF(pIndirectDeviceAddresses);
	DE_UNREF(pIndirectStrides);
	DE_UNREF(ppMaxPrimitiveCounts);
}

VKAPI_ATTR VkResult VKAPI_CALL buildAccelerationStructuresKHR (VkDevice device, VkDeferredOperationKHR deferredOperation, deUint32 infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkAccelerationStructureBuildRangeInfoKHR* const* ppBuildRangeInfos)
{
	DE_UNREF(device);
	DE_UNREF(deferredOperation);
	DE_UNREF(infoCount);
	DE_UNREF(pInfos);
	DE_UNREF(ppBuildRangeInfos);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL copyAccelerationStructureKHR (VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyAccelerationStructureInfoKHR* pInfo)
{
	DE_UNREF(device);
	DE_UNREF(deferredOperation);
	DE_UNREF(pInfo);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL copyAccelerationStructureToMemoryKHR (VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyAccelerationStructureToMemoryInfoKHR* pInfo)
{
	DE_UNREF(device);
	DE_UNREF(deferredOperation);
	DE_UNREF(pInfo);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL copyMemoryToAccelerationStructureKHR (VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyMemoryToAccelerationStructureInfoKHR* pInfo)
{
	DE_UNREF(device);
	DE_UNREF(deferredOperation);
	DE_UNREF(pInfo);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL writeAccelerationStructuresPropertiesKHR (VkDevice device, deUint32 accelerationStructureCount, const VkAccelerationStructureKHR* pAccelerationStructures, VkQueryType queryType, deUintptr dataSize, void* pData, deUintptr stride)
{
	DE_UNREF(device);
	DE_UNREF(accelerationStructureCount);
	DE_UNREF(pAccelerationStructures);
	DE_UNREF(queryType);
	DE_UNREF(dataSize);
	DE_UNREF(pData);
	DE_UNREF(stride);
	return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL cmdCopyAccelerationStructureKHR (VkCommandBuffer commandBuffer, const VkCopyAccelerationStructureInfoKHR* pInfo)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pInfo);
}

VKAPI_ATTR void VKAPI_CALL cmdCopyAccelerationStructureToMemoryKHR (VkCommandBuffer commandBuffer, const VkCopyAccelerationStructureToMemoryInfoKHR* pInfo)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pInfo);
}

VKAPI_ATTR void VKAPI_CALL cmdCopyMemoryToAccelerationStructureKHR (VkCommandBuffer commandBuffer, const VkCopyMemoryToAccelerationStructureInfoKHR* pInfo)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pInfo);
}

VKAPI_ATTR VkDeviceAddress VKAPI_CALL getAccelerationStructureDeviceAddressKHR (VkDevice device, const VkAccelerationStructureDeviceAddressInfoKHR* pInfo)
{
	DE_UNREF(device);
	DE_UNREF(pInfo);
	return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL cmdWriteAccelerationStructuresPropertiesKHR (VkCommandBuffer commandBuffer, deUint32 accelerationStructureCount, const VkAccelerationStructureKHR* pAccelerationStructures, VkQueryType queryType, VkQueryPool queryPool, deUint32 firstQuery)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(accelerationStructureCount);
	DE_UNREF(pAccelerationStructures);
	DE_UNREF(queryType);
	DE_UNREF(queryPool);
	DE_UNREF(firstQuery);
}

VKAPI_ATTR void VKAPI_CALL getDeviceAccelerationStructureCompatibilityKHR (VkDevice device, const VkAccelerationStructureVersionInfoKHR* pVersionInfo, VkAccelerationStructureCompatibilityKHR* pCompatibility)
{
	DE_UNREF(device);
	DE_UNREF(pVersionInfo);
	DE_UNREF(pCompatibility);
}

VKAPI_ATTR void VKAPI_CALL getAccelerationStructureBuildSizesKHR (VkDevice device, VkAccelerationStructureBuildTypeKHR buildType, const VkAccelerationStructureBuildGeometryInfoKHR* pBuildInfo, const deUint32* pMaxPrimitiveCounts, VkAccelerationStructureBuildSizesInfoKHR* pSizeInfo)
{
	DE_UNREF(device);
	DE_UNREF(buildType);
	DE_UNREF(pBuildInfo);
	DE_UNREF(pMaxPrimitiveCounts);
	DE_UNREF(pSizeInfo);
}

VKAPI_ATTR void VKAPI_CALL cmdTraceRaysKHR (VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR* pRaygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pMissShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pHitShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pCallableShaderBindingTable, deUint32 width, deUint32 height, deUint32 depth)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pRaygenShaderBindingTable);
	DE_UNREF(pMissShaderBindingTable);
	DE_UNREF(pHitShaderBindingTable);
	DE_UNREF(pCallableShaderBindingTable);
	DE_UNREF(width);
	DE_UNREF(height);
	DE_UNREF(depth);
}

VKAPI_ATTR VkResult VKAPI_CALL getRayTracingCaptureReplayShaderGroupHandlesKHR (VkDevice device, VkPipeline pipeline, deUint32 firstGroup, deUint32 groupCount, deUintptr dataSize, void* pData)
{
	DE_UNREF(device);
	DE_UNREF(pipeline);
	DE_UNREF(firstGroup);
	DE_UNREF(groupCount);
	DE_UNREF(dataSize);
	DE_UNREF(pData);
	return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL cmdTraceRaysIndirectKHR (VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR* pRaygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pMissShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pHitShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pCallableShaderBindingTable, VkDeviceAddress indirectDeviceAddress)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pRaygenShaderBindingTable);
	DE_UNREF(pMissShaderBindingTable);
	DE_UNREF(pHitShaderBindingTable);
	DE_UNREF(pCallableShaderBindingTable);
	DE_UNREF(indirectDeviceAddress);
}

VKAPI_ATTR VkDeviceSize VKAPI_CALL getRayTracingShaderGroupStackSizeKHR (VkDevice device, VkPipeline pipeline, deUint32 group, VkShaderGroupShaderKHR groupShader)
{
	DE_UNREF(device);
	DE_UNREF(pipeline);
	DE_UNREF(group);
	DE_UNREF(groupShader);
	return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL cmdSetRayTracingPipelineStackSizeKHR (VkCommandBuffer commandBuffer, deUint32 pipelineStackSize)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pipelineStackSize);
}

VKAPI_ATTR VkResult VKAPI_CALL getAndroidHardwareBufferPropertiesANDROID (VkDevice device, const struct pt::AndroidHardwareBufferPtr buffer, VkAndroidHardwareBufferPropertiesANDROID* pProperties)
{
	DE_UNREF(device);
	DE_UNREF(buffer);
	DE_UNREF(pProperties);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getPhysicalDeviceVideoCapabilitiesKHR (VkPhysicalDevice physicalDevice, const VkVideoProfileKHR* pVideoProfile, VkVideoCapabilitiesKHR* pCapabilities)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(pVideoProfile);
	DE_UNREF(pCapabilities);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getPhysicalDeviceVideoFormatPropertiesKHR (VkPhysicalDevice physicalDevice, const VkPhysicalDeviceVideoFormatInfoKHR* pVideoFormatInfo, deUint32* pVideoFormatPropertyCount, VkVideoFormatPropertiesKHR* pVideoFormatProperties)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(pVideoFormatInfo);
	DE_UNREF(pVideoFormatPropertyCount);
	DE_UNREF(pVideoFormatProperties);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getVideoSessionMemoryRequirementsKHR (VkDevice device, VkVideoSessionKHR videoSession, deUint32* pVideoSessionMemoryRequirementsCount, VkVideoGetMemoryPropertiesKHR* pVideoSessionMemoryRequirements)
{
	DE_UNREF(device);
	DE_UNREF(videoSession);
	DE_UNREF(pVideoSessionMemoryRequirementsCount);
	DE_UNREF(pVideoSessionMemoryRequirements);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL bindVideoSessionMemoryKHR (VkDevice device, VkVideoSessionKHR videoSession, deUint32 videoSessionBindMemoryCount, const VkVideoBindMemoryKHR* pVideoSessionBindMemories)
{
	DE_UNREF(device);
	DE_UNREF(videoSession);
	DE_UNREF(videoSessionBindMemoryCount);
	DE_UNREF(pVideoSessionBindMemories);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL updateVideoSessionParametersKHR (VkDevice device, VkVideoSessionParametersKHR videoSessionParameters, const VkVideoSessionParametersUpdateInfoKHR* pUpdateInfo)
{
	DE_UNREF(device);
	DE_UNREF(videoSessionParameters);
	DE_UNREF(pUpdateInfo);
	return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL cmdBeginVideoCodingKHR (VkCommandBuffer commandBuffer, const VkVideoBeginCodingInfoKHR* pBeginInfo)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pBeginInfo);
}

VKAPI_ATTR void VKAPI_CALL cmdEndVideoCodingKHR (VkCommandBuffer commandBuffer, const VkVideoEndCodingInfoKHR* pEndCodingInfo)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pEndCodingInfo);
}

VKAPI_ATTR void VKAPI_CALL cmdControlVideoCodingKHR (VkCommandBuffer commandBuffer, const VkVideoCodingControlInfoKHR* pCodingControlInfo)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pCodingControlInfo);
}

VKAPI_ATTR void VKAPI_CALL cmdDecodeVideoKHR (VkCommandBuffer commandBuffer, const VkVideoDecodeInfoKHR* pFrameInfo)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pFrameInfo);
}

VKAPI_ATTR void VKAPI_CALL cmdEncodeVideoKHR (VkCommandBuffer commandBuffer, const VkVideoEncodeInfoKHR* pEncodeInfo)
{
	DE_UNREF(commandBuffer);
	DE_UNREF(pEncodeInfo);
}

VKAPI_ATTR VkResult VKAPI_CALL getMemoryZirconHandleFUCHSIA (VkDevice device, const VkMemoryGetZirconHandleInfoFUCHSIA* pGetZirconHandleInfo, pt::zx_handle_t* pZirconHandle)
{
	DE_UNREF(device);
	DE_UNREF(pGetZirconHandleInfo);
	DE_UNREF(pZirconHandle);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getMemoryZirconHandlePropertiesFUCHSIA (VkDevice device, VkExternalMemoryHandleTypeFlagBits handleType, pt::zx_handle_t zirconHandle, VkMemoryZirconHandlePropertiesFUCHSIA* pMemoryZirconHandleProperties)
{
	DE_UNREF(device);
	DE_UNREF(handleType);
	DE_UNREF(zirconHandle);
	DE_UNREF(pMemoryZirconHandleProperties);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL importSemaphoreZirconHandleFUCHSIA (VkDevice device, const VkImportSemaphoreZirconHandleInfoFUCHSIA* pImportSemaphoreZirconHandleInfo)
{
	DE_UNREF(device);
	DE_UNREF(pImportSemaphoreZirconHandleInfo);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getSemaphoreZirconHandleFUCHSIA (VkDevice device, const VkSemaphoreGetZirconHandleInfoFUCHSIA* pGetZirconHandleInfo, pt::zx_handle_t* pZirconHandle)
{
	DE_UNREF(device);
	DE_UNREF(pGetZirconHandleInfo);
	DE_UNREF(pZirconHandle);
	return VK_SUCCESS;
}

VKAPI_ATTR VkBool32 VKAPI_CALL getPhysicalDeviceWaylandPresentationSupportKHR (VkPhysicalDevice physicalDevice, deUint32 queueFamilyIndex, pt::WaylandDisplayPtr display)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(queueFamilyIndex);
	DE_UNREF(display);
	return VK_SUCCESS;
}

VKAPI_ATTR VkBool32 VKAPI_CALL getPhysicalDeviceWin32PresentationSupportKHR (VkPhysicalDevice physicalDevice, deUint32 queueFamilyIndex)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(queueFamilyIndex);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getMemoryWin32HandleKHR (VkDevice device, const VkMemoryGetWin32HandleInfoKHR* pGetWin32HandleInfo, pt::Win32Handle* pHandle)
{
	DE_UNREF(device);
	DE_UNREF(pGetWin32HandleInfo);
	DE_UNREF(pHandle);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getMemoryWin32HandlePropertiesKHR (VkDevice device, VkExternalMemoryHandleTypeFlagBits handleType, pt::Win32Handle handle, VkMemoryWin32HandlePropertiesKHR* pMemoryWin32HandleProperties)
{
	DE_UNREF(device);
	DE_UNREF(handleType);
	DE_UNREF(handle);
	DE_UNREF(pMemoryWin32HandleProperties);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL importSemaphoreWin32HandleKHR (VkDevice device, const VkImportSemaphoreWin32HandleInfoKHR* pImportSemaphoreWin32HandleInfo)
{
	DE_UNREF(device);
	DE_UNREF(pImportSemaphoreWin32HandleInfo);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getSemaphoreWin32HandleKHR (VkDevice device, const VkSemaphoreGetWin32HandleInfoKHR* pGetWin32HandleInfo, pt::Win32Handle* pHandle)
{
	DE_UNREF(device);
	DE_UNREF(pGetWin32HandleInfo);
	DE_UNREF(pHandle);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL importFenceWin32HandleKHR (VkDevice device, const VkImportFenceWin32HandleInfoKHR* pImportFenceWin32HandleInfo)
{
	DE_UNREF(device);
	DE_UNREF(pImportFenceWin32HandleInfo);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getFenceWin32HandleKHR (VkDevice device, const VkFenceGetWin32HandleInfoKHR* pGetWin32HandleInfo, pt::Win32Handle* pHandle)
{
	DE_UNREF(device);
	DE_UNREF(pGetWin32HandleInfo);
	DE_UNREF(pHandle);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getMemoryWin32HandleNV (VkDevice device, VkDeviceMemory memory, VkExternalMemoryHandleTypeFlagsNV handleType, pt::Win32Handle* pHandle)
{
	DE_UNREF(device);
	DE_UNREF(memory);
	DE_UNREF(handleType);
	DE_UNREF(pHandle);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getPhysicalDeviceSurfacePresentModes2EXT (VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSurfaceInfo2KHR* pSurfaceInfo, deUint32* pPresentModeCount, VkPresentModeKHR* pPresentModes)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(pSurfaceInfo);
	DE_UNREF(pPresentModeCount);
	DE_UNREF(pPresentModes);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL acquireFullScreenExclusiveModeEXT (VkDevice device, VkSwapchainKHR swapchain)
{
	DE_UNREF(device);
	DE_UNREF(swapchain);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL releaseFullScreenExclusiveModeEXT (VkDevice device, VkSwapchainKHR swapchain)
{
	DE_UNREF(device);
	DE_UNREF(swapchain);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getDeviceGroupSurfacePresentModes2EXT (VkDevice device, const VkPhysicalDeviceSurfaceInfo2KHR* pSurfaceInfo, VkDeviceGroupPresentModeFlagsKHR* pModes)
{
	DE_UNREF(device);
	DE_UNREF(pSurfaceInfo);
	DE_UNREF(pModes);
	return VK_SUCCESS;
}

VKAPI_ATTR VkBool32 VKAPI_CALL getPhysicalDeviceXcbPresentationSupportKHR (VkPhysicalDevice physicalDevice, deUint32 queueFamilyIndex, pt::XcbConnectionPtr connection, pt::XcbVisualid visual_id)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(queueFamilyIndex);
	DE_UNREF(connection);
	DE_UNREF(visual_id);
	return VK_SUCCESS;
}

VKAPI_ATTR VkBool32 VKAPI_CALL getPhysicalDeviceXlibPresentationSupportKHR (VkPhysicalDevice physicalDevice, deUint32 queueFamilyIndex, pt::XlibDisplayPtr dpy, pt::XlibVisualID visualID)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(queueFamilyIndex);
	DE_UNREF(dpy);
	DE_UNREF(visualID);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL acquireXlibDisplayEXT (VkPhysicalDevice physicalDevice, pt::XlibDisplayPtr dpy, VkDisplayKHR display)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(dpy);
	DE_UNREF(display);
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getRandROutputDisplayEXT (VkPhysicalDevice physicalDevice, pt::XlibDisplayPtr dpy, pt::RROutput rrOutput, VkDisplayKHR* pDisplay)
{
	DE_UNREF(physicalDevice);
	DE_UNREF(dpy);
	DE_UNREF(rrOutput);
	DE_UNREF(pDisplay);
	return VK_SUCCESS;
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
	VK_NULL_FUNC_ENTRY(vkDestroyInstance,													destroyInstance),
	VK_NULL_FUNC_ENTRY(vkEnumeratePhysicalDevices,											enumeratePhysicalDevices),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceFeatures,											getPhysicalDeviceFeatures),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceFormatProperties,									getPhysicalDeviceFormatProperties),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceImageFormatProperties,							getPhysicalDeviceImageFormatProperties),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceProperties,										getPhysicalDeviceProperties),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceQueueFamilyProperties,							getPhysicalDeviceQueueFamilyProperties),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceMemoryProperties,									getPhysicalDeviceMemoryProperties),
	VK_NULL_FUNC_ENTRY(vkCreateDevice,														createDevice),
	VK_NULL_FUNC_ENTRY(vkEnumerateDeviceExtensionProperties,								enumerateDeviceExtensionProperties),
	VK_NULL_FUNC_ENTRY(vkEnumerateDeviceLayerProperties,									enumerateDeviceLayerProperties),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceSparseImageFormatProperties,						getPhysicalDeviceSparseImageFormatProperties),
	VK_NULL_FUNC_ENTRY(vkEnumeratePhysicalDeviceGroups,										enumeratePhysicalDeviceGroups),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceFeatures2,										getPhysicalDeviceFeatures2),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceProperties2,										getPhysicalDeviceProperties2),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceFormatProperties2,								getPhysicalDeviceFormatProperties2),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceImageFormatProperties2,							getPhysicalDeviceImageFormatProperties2),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceQueueFamilyProperties2,							getPhysicalDeviceQueueFamilyProperties2),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceMemoryProperties2,								getPhysicalDeviceMemoryProperties2),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceSparseImageFormatProperties2,						getPhysicalDeviceSparseImageFormatProperties2),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceExternalBufferProperties,							getPhysicalDeviceExternalBufferProperties),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceExternalFenceProperties,							getPhysicalDeviceExternalFenceProperties),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceExternalSemaphoreProperties,						getPhysicalDeviceExternalSemaphoreProperties),
	VK_NULL_FUNC_ENTRY(vkDestroySurfaceKHR,													destroySurfaceKHR),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceSurfaceSupportKHR,								getPhysicalDeviceSurfaceSupportKHR),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceSurfaceCapabilitiesKHR,							getPhysicalDeviceSurfaceCapabilitiesKHR),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceSurfaceFormatsKHR,								getPhysicalDeviceSurfaceFormatsKHR),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceSurfacePresentModesKHR,							getPhysicalDeviceSurfacePresentModesKHR),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDevicePresentRectanglesKHR,								getPhysicalDevicePresentRectanglesKHR),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceDisplayPropertiesKHR,								getPhysicalDeviceDisplayPropertiesKHR),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceDisplayPlanePropertiesKHR,						getPhysicalDeviceDisplayPlanePropertiesKHR),
	VK_NULL_FUNC_ENTRY(vkGetDisplayPlaneSupportedDisplaysKHR,								getDisplayPlaneSupportedDisplaysKHR),
	VK_NULL_FUNC_ENTRY(vkGetDisplayModePropertiesKHR,										getDisplayModePropertiesKHR),
	VK_NULL_FUNC_ENTRY(vkCreateDisplayModeKHR,												createDisplayModeKHR),
	VK_NULL_FUNC_ENTRY(vkGetDisplayPlaneCapabilitiesKHR,									getDisplayPlaneCapabilitiesKHR),
	VK_NULL_FUNC_ENTRY(vkCreateDisplayPlaneSurfaceKHR,										createDisplayPlaneSurfaceKHR),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceFeatures2KHR,										getPhysicalDeviceFeatures2),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceProperties2KHR,									getPhysicalDeviceProperties2),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceFormatProperties2KHR,								getPhysicalDeviceFormatProperties2),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceImageFormatProperties2KHR,						getPhysicalDeviceImageFormatProperties2),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceQueueFamilyProperties2KHR,						getPhysicalDeviceQueueFamilyProperties2),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceMemoryProperties2KHR,								getPhysicalDeviceMemoryProperties2),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceSparseImageFormatProperties2KHR,					getPhysicalDeviceSparseImageFormatProperties2),
	VK_NULL_FUNC_ENTRY(vkEnumeratePhysicalDeviceGroupsKHR,									enumeratePhysicalDeviceGroups),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceExternalBufferPropertiesKHR,						getPhysicalDeviceExternalBufferProperties),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceExternalSemaphorePropertiesKHR,					getPhysicalDeviceExternalSemaphoreProperties),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceExternalFencePropertiesKHR,						getPhysicalDeviceExternalFenceProperties),
	VK_NULL_FUNC_ENTRY(vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR,		enumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR,				getPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceSurfaceCapabilities2KHR,							getPhysicalDeviceSurfaceCapabilities2KHR),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceSurfaceFormats2KHR,								getPhysicalDeviceSurfaceFormats2KHR),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceDisplayProperties2KHR,							getPhysicalDeviceDisplayProperties2KHR),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceDisplayPlaneProperties2KHR,						getPhysicalDeviceDisplayPlaneProperties2KHR),
	VK_NULL_FUNC_ENTRY(vkGetDisplayModeProperties2KHR,										getDisplayModeProperties2KHR),
	VK_NULL_FUNC_ENTRY(vkGetDisplayPlaneCapabilities2KHR,									getDisplayPlaneCapabilities2KHR),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceFragmentShadingRatesKHR,							getPhysicalDeviceFragmentShadingRatesKHR),
	VK_NULL_FUNC_ENTRY(vkCreateDebugReportCallbackEXT,										createDebugReportCallbackEXT),
	VK_NULL_FUNC_ENTRY(vkDestroyDebugReportCallbackEXT,										destroyDebugReportCallbackEXT),
	VK_NULL_FUNC_ENTRY(vkDebugReportMessageEXT,												debugReportMessageEXT),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceExternalImageFormatPropertiesNV,					getPhysicalDeviceExternalImageFormatPropertiesNV),
	VK_NULL_FUNC_ENTRY(vkReleaseDisplayEXT,													releaseDisplayEXT),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceSurfaceCapabilities2EXT,							getPhysicalDeviceSurfaceCapabilities2EXT),
	VK_NULL_FUNC_ENTRY(vkCreateDebugUtilsMessengerEXT,										createDebugUtilsMessengerEXT),
	VK_NULL_FUNC_ENTRY(vkDestroyDebugUtilsMessengerEXT,										destroyDebugUtilsMessengerEXT),
	VK_NULL_FUNC_ENTRY(vkSubmitDebugUtilsMessageEXT,										submitDebugUtilsMessageEXT),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceMultisamplePropertiesEXT,							getPhysicalDeviceMultisamplePropertiesEXT),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceCalibrateableTimeDomainsEXT,						getPhysicalDeviceCalibrateableTimeDomainsEXT),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceToolPropertiesEXT,								getPhysicalDeviceToolPropertiesEXT),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceCooperativeMatrixPropertiesNV,					getPhysicalDeviceCooperativeMatrixPropertiesNV),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV,	getPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV),
	VK_NULL_FUNC_ENTRY(vkCreateHeadlessSurfaceEXT,											createHeadlessSurfaceEXT),
	VK_NULL_FUNC_ENTRY(vkAcquireDrmDisplayEXT,												acquireDrmDisplayEXT),
	VK_NULL_FUNC_ENTRY(vkGetDrmDisplayEXT,													getDrmDisplayEXT),
	VK_NULL_FUNC_ENTRY(vkAcquireWinrtDisplayNV,												acquireWinrtDisplayNV),
	VK_NULL_FUNC_ENTRY(vkGetWinrtDisplayNV,													getWinrtDisplayNV),
	VK_NULL_FUNC_ENTRY(vkCreateAndroidSurfaceKHR,											createAndroidSurfaceKHR),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceVideoCapabilitiesKHR,								getPhysicalDeviceVideoCapabilitiesKHR),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceVideoFormatPropertiesKHR,							getPhysicalDeviceVideoFormatPropertiesKHR),
	VK_NULL_FUNC_ENTRY(vkCreateImagePipeSurfaceFUCHSIA,										createImagePipeSurfaceFUCHSIA),
	VK_NULL_FUNC_ENTRY(vkCreateStreamDescriptorSurfaceGGP,									createStreamDescriptorSurfaceGGP),
	VK_NULL_FUNC_ENTRY(vkCreateIOSSurfaceMVK,												createIOSSurfaceMVK),
	VK_NULL_FUNC_ENTRY(vkCreateMacOSSurfaceMVK,												createMacOSSurfaceMVK),
	VK_NULL_FUNC_ENTRY(vkCreateMetalSurfaceEXT,												createMetalSurfaceEXT),
	VK_NULL_FUNC_ENTRY(vkCreateViSurfaceNN,													createViSurfaceNN),
	VK_NULL_FUNC_ENTRY(vkCreateWaylandSurfaceKHR,											createWaylandSurfaceKHR),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceWaylandPresentationSupportKHR,					getPhysicalDeviceWaylandPresentationSupportKHR),
	VK_NULL_FUNC_ENTRY(vkCreateWin32SurfaceKHR,												createWin32SurfaceKHR),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceWin32PresentationSupportKHR,						getPhysicalDeviceWin32PresentationSupportKHR),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceSurfacePresentModes2EXT,							getPhysicalDeviceSurfacePresentModes2EXT),
	VK_NULL_FUNC_ENTRY(vkCreateXcbSurfaceKHR,												createXcbSurfaceKHR),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceXcbPresentationSupportKHR,						getPhysicalDeviceXcbPresentationSupportKHR),
	VK_NULL_FUNC_ENTRY(vkCreateXlibSurfaceKHR,												createXlibSurfaceKHR),
	VK_NULL_FUNC_ENTRY(vkGetPhysicalDeviceXlibPresentationSupportKHR,						getPhysicalDeviceXlibPresentationSupportKHR),
	VK_NULL_FUNC_ENTRY(vkAcquireXlibDisplayEXT,												acquireXlibDisplayEXT),
	VK_NULL_FUNC_ENTRY(vkGetRandROutputDisplayEXT,											getRandROutputDisplayEXT),
};

static const tcu::StaticFunctionLibrary::Entry s_deviceFunctions[] =
{
	VK_NULL_FUNC_ENTRY(vkGetDeviceProcAddr,									getDeviceProcAddr),
	VK_NULL_FUNC_ENTRY(vkDestroyDevice,										destroyDevice),
	VK_NULL_FUNC_ENTRY(vkGetDeviceQueue,									getDeviceQueue),
	VK_NULL_FUNC_ENTRY(vkQueueSubmit,										queueSubmit),
	VK_NULL_FUNC_ENTRY(vkQueueWaitIdle,										queueWaitIdle),
	VK_NULL_FUNC_ENTRY(vkDeviceWaitIdle,									deviceWaitIdle),
	VK_NULL_FUNC_ENTRY(vkAllocateMemory,									allocateMemory),
	VK_NULL_FUNC_ENTRY(vkFreeMemory,										freeMemory),
	VK_NULL_FUNC_ENTRY(vkMapMemory,											mapMemory),
	VK_NULL_FUNC_ENTRY(vkUnmapMemory,										unmapMemory),
	VK_NULL_FUNC_ENTRY(vkFlushMappedMemoryRanges,							flushMappedMemoryRanges),
	VK_NULL_FUNC_ENTRY(vkInvalidateMappedMemoryRanges,						invalidateMappedMemoryRanges),
	VK_NULL_FUNC_ENTRY(vkGetDeviceMemoryCommitment,							getDeviceMemoryCommitment),
	VK_NULL_FUNC_ENTRY(vkBindBufferMemory,									bindBufferMemory),
	VK_NULL_FUNC_ENTRY(vkBindImageMemory,									bindImageMemory),
	VK_NULL_FUNC_ENTRY(vkGetBufferMemoryRequirements,						getBufferMemoryRequirements),
	VK_NULL_FUNC_ENTRY(vkGetImageMemoryRequirements,						getImageMemoryRequirements),
	VK_NULL_FUNC_ENTRY(vkGetImageSparseMemoryRequirements,					getImageSparseMemoryRequirements),
	VK_NULL_FUNC_ENTRY(vkQueueBindSparse,									queueBindSparse),
	VK_NULL_FUNC_ENTRY(vkCreateFence,										createFence),
	VK_NULL_FUNC_ENTRY(vkDestroyFence,										destroyFence),
	VK_NULL_FUNC_ENTRY(vkResetFences,										resetFences),
	VK_NULL_FUNC_ENTRY(vkGetFenceStatus,									getFenceStatus),
	VK_NULL_FUNC_ENTRY(vkWaitForFences,										waitForFences),
	VK_NULL_FUNC_ENTRY(vkCreateSemaphore,									createSemaphore),
	VK_NULL_FUNC_ENTRY(vkDestroySemaphore,									destroySemaphore),
	VK_NULL_FUNC_ENTRY(vkCreateEvent,										createEvent),
	VK_NULL_FUNC_ENTRY(vkDestroyEvent,										destroyEvent),
	VK_NULL_FUNC_ENTRY(vkGetEventStatus,									getEventStatus),
	VK_NULL_FUNC_ENTRY(vkSetEvent,											setEvent),
	VK_NULL_FUNC_ENTRY(vkResetEvent,										resetEvent),
	VK_NULL_FUNC_ENTRY(vkCreateQueryPool,									createQueryPool),
	VK_NULL_FUNC_ENTRY(vkDestroyQueryPool,									destroyQueryPool),
	VK_NULL_FUNC_ENTRY(vkGetQueryPoolResults,								getQueryPoolResults),
	VK_NULL_FUNC_ENTRY(vkCreateBuffer,										createBuffer),
	VK_NULL_FUNC_ENTRY(vkDestroyBuffer,										destroyBuffer),
	VK_NULL_FUNC_ENTRY(vkCreateBufferView,									createBufferView),
	VK_NULL_FUNC_ENTRY(vkDestroyBufferView,									destroyBufferView),
	VK_NULL_FUNC_ENTRY(vkCreateImage,										createImage),
	VK_NULL_FUNC_ENTRY(vkDestroyImage,										destroyImage),
	VK_NULL_FUNC_ENTRY(vkGetImageSubresourceLayout,							getImageSubresourceLayout),
	VK_NULL_FUNC_ENTRY(vkCreateImageView,									createImageView),
	VK_NULL_FUNC_ENTRY(vkDestroyImageView,									destroyImageView),
	VK_NULL_FUNC_ENTRY(vkCreateShaderModule,								createShaderModule),
	VK_NULL_FUNC_ENTRY(vkDestroyShaderModule,								destroyShaderModule),
	VK_NULL_FUNC_ENTRY(vkCreatePipelineCache,								createPipelineCache),
	VK_NULL_FUNC_ENTRY(vkDestroyPipelineCache,								destroyPipelineCache),
	VK_NULL_FUNC_ENTRY(vkGetPipelineCacheData,								getPipelineCacheData),
	VK_NULL_FUNC_ENTRY(vkMergePipelineCaches,								mergePipelineCaches),
	VK_NULL_FUNC_ENTRY(vkCreateGraphicsPipelines,							createGraphicsPipelines),
	VK_NULL_FUNC_ENTRY(vkCreateComputePipelines,							createComputePipelines),
	VK_NULL_FUNC_ENTRY(vkDestroyPipeline,									destroyPipeline),
	VK_NULL_FUNC_ENTRY(vkCreatePipelineLayout,								createPipelineLayout),
	VK_NULL_FUNC_ENTRY(vkDestroyPipelineLayout,								destroyPipelineLayout),
	VK_NULL_FUNC_ENTRY(vkCreateSampler,										createSampler),
	VK_NULL_FUNC_ENTRY(vkDestroySampler,									destroySampler),
	VK_NULL_FUNC_ENTRY(vkCreateDescriptorSetLayout,							createDescriptorSetLayout),
	VK_NULL_FUNC_ENTRY(vkDestroyDescriptorSetLayout,						destroyDescriptorSetLayout),
	VK_NULL_FUNC_ENTRY(vkCreateDescriptorPool,								createDescriptorPool),
	VK_NULL_FUNC_ENTRY(vkDestroyDescriptorPool,								destroyDescriptorPool),
	VK_NULL_FUNC_ENTRY(vkResetDescriptorPool,								resetDescriptorPool),
	VK_NULL_FUNC_ENTRY(vkAllocateDescriptorSets,							allocateDescriptorSets),
	VK_NULL_FUNC_ENTRY(vkFreeDescriptorSets,								freeDescriptorSets),
	VK_NULL_FUNC_ENTRY(vkUpdateDescriptorSets,								updateDescriptorSets),
	VK_NULL_FUNC_ENTRY(vkCreateFramebuffer,									createFramebuffer),
	VK_NULL_FUNC_ENTRY(vkDestroyFramebuffer,								destroyFramebuffer),
	VK_NULL_FUNC_ENTRY(vkCreateRenderPass,									createRenderPass),
	VK_NULL_FUNC_ENTRY(vkDestroyRenderPass,									destroyRenderPass),
	VK_NULL_FUNC_ENTRY(vkGetRenderAreaGranularity,							getRenderAreaGranularity),
	VK_NULL_FUNC_ENTRY(vkCreateCommandPool,									createCommandPool),
	VK_NULL_FUNC_ENTRY(vkDestroyCommandPool,								destroyCommandPool),
	VK_NULL_FUNC_ENTRY(vkResetCommandPool,									resetCommandPool),
	VK_NULL_FUNC_ENTRY(vkAllocateCommandBuffers,							allocateCommandBuffers),
	VK_NULL_FUNC_ENTRY(vkFreeCommandBuffers,								freeCommandBuffers),
	VK_NULL_FUNC_ENTRY(vkBeginCommandBuffer,								beginCommandBuffer),
	VK_NULL_FUNC_ENTRY(vkEndCommandBuffer,									endCommandBuffer),
	VK_NULL_FUNC_ENTRY(vkResetCommandBuffer,								resetCommandBuffer),
	VK_NULL_FUNC_ENTRY(vkCmdBindPipeline,									cmdBindPipeline),
	VK_NULL_FUNC_ENTRY(vkCmdSetViewport,									cmdSetViewport),
	VK_NULL_FUNC_ENTRY(vkCmdSetScissor,										cmdSetScissor),
	VK_NULL_FUNC_ENTRY(vkCmdSetLineWidth,									cmdSetLineWidth),
	VK_NULL_FUNC_ENTRY(vkCmdSetDepthBias,									cmdSetDepthBias),
	VK_NULL_FUNC_ENTRY(vkCmdSetBlendConstants,								cmdSetBlendConstants),
	VK_NULL_FUNC_ENTRY(vkCmdSetDepthBounds,									cmdSetDepthBounds),
	VK_NULL_FUNC_ENTRY(vkCmdSetStencilCompareMask,							cmdSetStencilCompareMask),
	VK_NULL_FUNC_ENTRY(vkCmdSetStencilWriteMask,							cmdSetStencilWriteMask),
	VK_NULL_FUNC_ENTRY(vkCmdSetStencilReference,							cmdSetStencilReference),
	VK_NULL_FUNC_ENTRY(vkCmdBindDescriptorSets,								cmdBindDescriptorSets),
	VK_NULL_FUNC_ENTRY(vkCmdBindIndexBuffer,								cmdBindIndexBuffer),
	VK_NULL_FUNC_ENTRY(vkCmdBindVertexBuffers,								cmdBindVertexBuffers),
	VK_NULL_FUNC_ENTRY(vkCmdDraw,											cmdDraw),
	VK_NULL_FUNC_ENTRY(vkCmdDrawIndexed,									cmdDrawIndexed),
	VK_NULL_FUNC_ENTRY(vkCmdDrawIndirect,									cmdDrawIndirect),
	VK_NULL_FUNC_ENTRY(vkCmdDrawIndexedIndirect,							cmdDrawIndexedIndirect),
	VK_NULL_FUNC_ENTRY(vkCmdDispatch,										cmdDispatch),
	VK_NULL_FUNC_ENTRY(vkCmdDispatchIndirect,								cmdDispatchIndirect),
	VK_NULL_FUNC_ENTRY(vkCmdCopyBuffer,										cmdCopyBuffer),
	VK_NULL_FUNC_ENTRY(vkCmdCopyImage,										cmdCopyImage),
	VK_NULL_FUNC_ENTRY(vkCmdBlitImage,										cmdBlitImage),
	VK_NULL_FUNC_ENTRY(vkCmdCopyBufferToImage,								cmdCopyBufferToImage),
	VK_NULL_FUNC_ENTRY(vkCmdCopyImageToBuffer,								cmdCopyImageToBuffer),
	VK_NULL_FUNC_ENTRY(vkCmdUpdateBuffer,									cmdUpdateBuffer),
	VK_NULL_FUNC_ENTRY(vkCmdFillBuffer,										cmdFillBuffer),
	VK_NULL_FUNC_ENTRY(vkCmdClearColorImage,								cmdClearColorImage),
	VK_NULL_FUNC_ENTRY(vkCmdClearDepthStencilImage,							cmdClearDepthStencilImage),
	VK_NULL_FUNC_ENTRY(vkCmdClearAttachments,								cmdClearAttachments),
	VK_NULL_FUNC_ENTRY(vkCmdResolveImage,									cmdResolveImage),
	VK_NULL_FUNC_ENTRY(vkCmdSetEvent,										cmdSetEvent),
	VK_NULL_FUNC_ENTRY(vkCmdResetEvent,										cmdResetEvent),
	VK_NULL_FUNC_ENTRY(vkCmdWaitEvents,										cmdWaitEvents),
	VK_NULL_FUNC_ENTRY(vkCmdPipelineBarrier,								cmdPipelineBarrier),
	VK_NULL_FUNC_ENTRY(vkCmdBeginQuery,										cmdBeginQuery),
	VK_NULL_FUNC_ENTRY(vkCmdEndQuery,										cmdEndQuery),
	VK_NULL_FUNC_ENTRY(vkCmdResetQueryPool,									cmdResetQueryPool),
	VK_NULL_FUNC_ENTRY(vkCmdWriteTimestamp,									cmdWriteTimestamp),
	VK_NULL_FUNC_ENTRY(vkCmdCopyQueryPoolResults,							cmdCopyQueryPoolResults),
	VK_NULL_FUNC_ENTRY(vkCmdPushConstants,									cmdPushConstants),
	VK_NULL_FUNC_ENTRY(vkCmdBeginRenderPass,								cmdBeginRenderPass),
	VK_NULL_FUNC_ENTRY(vkCmdNextSubpass,									cmdNextSubpass),
	VK_NULL_FUNC_ENTRY(vkCmdEndRenderPass,									cmdEndRenderPass),
	VK_NULL_FUNC_ENTRY(vkCmdExecuteCommands,								cmdExecuteCommands),
	VK_NULL_FUNC_ENTRY(vkBindBufferMemory2,									bindBufferMemory2),
	VK_NULL_FUNC_ENTRY(vkBindImageMemory2,									bindImageMemory2),
	VK_NULL_FUNC_ENTRY(vkGetDeviceGroupPeerMemoryFeatures,					getDeviceGroupPeerMemoryFeatures),
	VK_NULL_FUNC_ENTRY(vkCmdSetDeviceMask,									cmdSetDeviceMask),
	VK_NULL_FUNC_ENTRY(vkCmdDispatchBase,									cmdDispatchBase),
	VK_NULL_FUNC_ENTRY(vkGetImageMemoryRequirements2,						getImageMemoryRequirements2),
	VK_NULL_FUNC_ENTRY(vkGetBufferMemoryRequirements2,						getBufferMemoryRequirements2),
	VK_NULL_FUNC_ENTRY(vkGetImageSparseMemoryRequirements2,					getImageSparseMemoryRequirements2),
	VK_NULL_FUNC_ENTRY(vkTrimCommandPool,									trimCommandPool),
	VK_NULL_FUNC_ENTRY(vkGetDeviceQueue2,									getDeviceQueue2),
	VK_NULL_FUNC_ENTRY(vkCreateSamplerYcbcrConversion,						createSamplerYcbcrConversion),
	VK_NULL_FUNC_ENTRY(vkDestroySamplerYcbcrConversion,						destroySamplerYcbcrConversion),
	VK_NULL_FUNC_ENTRY(vkCreateDescriptorUpdateTemplate,					createDescriptorUpdateTemplate),
	VK_NULL_FUNC_ENTRY(vkDestroyDescriptorUpdateTemplate,					destroyDescriptorUpdateTemplate),
	VK_NULL_FUNC_ENTRY(vkUpdateDescriptorSetWithTemplate,					updateDescriptorSetWithTemplate),
	VK_NULL_FUNC_ENTRY(vkGetDescriptorSetLayoutSupport,						getDescriptorSetLayoutSupport),
	VK_NULL_FUNC_ENTRY(vkCmdDrawIndirectCount,								cmdDrawIndirectCount),
	VK_NULL_FUNC_ENTRY(vkCmdDrawIndexedIndirectCount,						cmdDrawIndexedIndirectCount),
	VK_NULL_FUNC_ENTRY(vkCreateRenderPass2,									createRenderPass2),
	VK_NULL_FUNC_ENTRY(vkCmdBeginRenderPass2,								cmdBeginRenderPass2),
	VK_NULL_FUNC_ENTRY(vkCmdNextSubpass2,									cmdNextSubpass2),
	VK_NULL_FUNC_ENTRY(vkCmdEndRenderPass2,									cmdEndRenderPass2),
	VK_NULL_FUNC_ENTRY(vkResetQueryPool,									resetQueryPool),
	VK_NULL_FUNC_ENTRY(vkGetSemaphoreCounterValue,							getSemaphoreCounterValue),
	VK_NULL_FUNC_ENTRY(vkWaitSemaphores,									waitSemaphores),
	VK_NULL_FUNC_ENTRY(vkSignalSemaphore,									signalSemaphore),
	VK_NULL_FUNC_ENTRY(vkGetBufferDeviceAddress,							getBufferDeviceAddress),
	VK_NULL_FUNC_ENTRY(vkGetBufferOpaqueCaptureAddress,						getBufferOpaqueCaptureAddress),
	VK_NULL_FUNC_ENTRY(vkGetDeviceMemoryOpaqueCaptureAddress,				getDeviceMemoryOpaqueCaptureAddress),
	VK_NULL_FUNC_ENTRY(vkCreateSwapchainKHR,								createSwapchainKHR),
	VK_NULL_FUNC_ENTRY(vkDestroySwapchainKHR,								destroySwapchainKHR),
	VK_NULL_FUNC_ENTRY(vkGetSwapchainImagesKHR,								getSwapchainImagesKHR),
	VK_NULL_FUNC_ENTRY(vkAcquireNextImageKHR,								acquireNextImageKHR),
	VK_NULL_FUNC_ENTRY(vkQueuePresentKHR,									queuePresentKHR),
	VK_NULL_FUNC_ENTRY(vkGetDeviceGroupPresentCapabilitiesKHR,				getDeviceGroupPresentCapabilitiesKHR),
	VK_NULL_FUNC_ENTRY(vkGetDeviceGroupSurfacePresentModesKHR,				getDeviceGroupSurfacePresentModesKHR),
	VK_NULL_FUNC_ENTRY(vkAcquireNextImage2KHR,								acquireNextImage2KHR),
	VK_NULL_FUNC_ENTRY(vkCreateSharedSwapchainsKHR,							createSharedSwapchainsKHR),
	VK_NULL_FUNC_ENTRY(vkGetDeviceGroupPeerMemoryFeaturesKHR,				getDeviceGroupPeerMemoryFeatures),
	VK_NULL_FUNC_ENTRY(vkCmdSetDeviceMaskKHR,								cmdSetDeviceMask),
	VK_NULL_FUNC_ENTRY(vkCmdDispatchBaseKHR,								cmdDispatchBase),
	VK_NULL_FUNC_ENTRY(vkTrimCommandPoolKHR,								trimCommandPool),
	VK_NULL_FUNC_ENTRY(vkGetMemoryFdKHR,									getMemoryFdKHR),
	VK_NULL_FUNC_ENTRY(vkGetMemoryFdPropertiesKHR,							getMemoryFdPropertiesKHR),
	VK_NULL_FUNC_ENTRY(vkImportSemaphoreFdKHR,								importSemaphoreFdKHR),
	VK_NULL_FUNC_ENTRY(vkGetSemaphoreFdKHR,									getSemaphoreFdKHR),
	VK_NULL_FUNC_ENTRY(vkCmdPushDescriptorSetKHR,							cmdPushDescriptorSetKHR),
	VK_NULL_FUNC_ENTRY(vkCmdPushDescriptorSetWithTemplateKHR,				cmdPushDescriptorSetWithTemplateKHR),
	VK_NULL_FUNC_ENTRY(vkCreateDescriptorUpdateTemplateKHR,					createDescriptorUpdateTemplate),
	VK_NULL_FUNC_ENTRY(vkDestroyDescriptorUpdateTemplateKHR,				destroyDescriptorUpdateTemplate),
	VK_NULL_FUNC_ENTRY(vkUpdateDescriptorSetWithTemplateKHR,				updateDescriptorSetWithTemplate),
	VK_NULL_FUNC_ENTRY(vkCreateRenderPass2KHR,								createRenderPass2),
	VK_NULL_FUNC_ENTRY(vkCmdBeginRenderPass2KHR,							cmdBeginRenderPass2),
	VK_NULL_FUNC_ENTRY(vkCmdNextSubpass2KHR,								cmdNextSubpass2),
	VK_NULL_FUNC_ENTRY(vkCmdEndRenderPass2KHR,								cmdEndRenderPass2),
	VK_NULL_FUNC_ENTRY(vkGetSwapchainStatusKHR,								getSwapchainStatusKHR),
	VK_NULL_FUNC_ENTRY(vkImportFenceFdKHR,									importFenceFdKHR),
	VK_NULL_FUNC_ENTRY(vkGetFenceFdKHR,										getFenceFdKHR),
	VK_NULL_FUNC_ENTRY(vkAcquireProfilingLockKHR,							acquireProfilingLockKHR),
	VK_NULL_FUNC_ENTRY(vkReleaseProfilingLockKHR,							releaseProfilingLockKHR),
	VK_NULL_FUNC_ENTRY(vkGetImageMemoryRequirements2KHR,					getImageMemoryRequirements2),
	VK_NULL_FUNC_ENTRY(vkGetBufferMemoryRequirements2KHR,					getBufferMemoryRequirements2),
	VK_NULL_FUNC_ENTRY(vkGetImageSparseMemoryRequirements2KHR,				getImageSparseMemoryRequirements2),
	VK_NULL_FUNC_ENTRY(vkCreateSamplerYcbcrConversionKHR,					createSamplerYcbcrConversion),
	VK_NULL_FUNC_ENTRY(vkDestroySamplerYcbcrConversionKHR,					destroySamplerYcbcrConversion),
	VK_NULL_FUNC_ENTRY(vkBindBufferMemory2KHR,								bindBufferMemory2),
	VK_NULL_FUNC_ENTRY(vkBindImageMemory2KHR,								bindImageMemory2),
	VK_NULL_FUNC_ENTRY(vkGetDescriptorSetLayoutSupportKHR,					getDescriptorSetLayoutSupport),
	VK_NULL_FUNC_ENTRY(vkCmdDrawIndirectCountKHR,							cmdDrawIndirectCount),
	VK_NULL_FUNC_ENTRY(vkCmdDrawIndexedIndirectCountKHR,					cmdDrawIndexedIndirectCount),
	VK_NULL_FUNC_ENTRY(vkGetSemaphoreCounterValueKHR,						getSemaphoreCounterValue),
	VK_NULL_FUNC_ENTRY(vkWaitSemaphoresKHR,									waitSemaphores),
	VK_NULL_FUNC_ENTRY(vkSignalSemaphoreKHR,								signalSemaphore),
	VK_NULL_FUNC_ENTRY(vkCmdSetFragmentShadingRateKHR,						cmdSetFragmentShadingRateKHR),
	VK_NULL_FUNC_ENTRY(vkWaitForPresentKHR,									waitForPresentKHR),
	VK_NULL_FUNC_ENTRY(vkGetBufferDeviceAddressKHR,							getBufferDeviceAddress),
	VK_NULL_FUNC_ENTRY(vkGetBufferOpaqueCaptureAddressKHR,					getBufferOpaqueCaptureAddress),
	VK_NULL_FUNC_ENTRY(vkGetDeviceMemoryOpaqueCaptureAddressKHR,			getDeviceMemoryOpaqueCaptureAddress),
	VK_NULL_FUNC_ENTRY(vkCreateDeferredOperationKHR,						createDeferredOperationKHR),
	VK_NULL_FUNC_ENTRY(vkDestroyDeferredOperationKHR,						destroyDeferredOperationKHR),
	VK_NULL_FUNC_ENTRY(vkGetDeferredOperationMaxConcurrencyKHR,				getDeferredOperationMaxConcurrencyKHR),
	VK_NULL_FUNC_ENTRY(vkGetDeferredOperationResultKHR,						getDeferredOperationResultKHR),
	VK_NULL_FUNC_ENTRY(vkDeferredOperationJoinKHR,							deferredOperationJoinKHR),
	VK_NULL_FUNC_ENTRY(vkGetPipelineExecutablePropertiesKHR,				getPipelineExecutablePropertiesKHR),
	VK_NULL_FUNC_ENTRY(vkGetPipelineExecutableStatisticsKHR,				getPipelineExecutableStatisticsKHR),
	VK_NULL_FUNC_ENTRY(vkGetPipelineExecutableInternalRepresentationsKHR,	getPipelineExecutableInternalRepresentationsKHR),
	VK_NULL_FUNC_ENTRY(vkCmdSetEvent2KHR,									cmdSetEvent2KHR),
	VK_NULL_FUNC_ENTRY(vkCmdResetEvent2KHR,									cmdResetEvent2KHR),
	VK_NULL_FUNC_ENTRY(vkCmdWaitEvents2KHR,									cmdWaitEvents2KHR),
	VK_NULL_FUNC_ENTRY(vkCmdPipelineBarrier2KHR,							cmdPipelineBarrier2KHR),
	VK_NULL_FUNC_ENTRY(vkCmdWriteTimestamp2KHR,								cmdWriteTimestamp2KHR),
	VK_NULL_FUNC_ENTRY(vkQueueSubmit2KHR,									queueSubmit2KHR),
	VK_NULL_FUNC_ENTRY(vkCmdWriteBufferMarker2AMD,							cmdWriteBufferMarker2AMD),
	VK_NULL_FUNC_ENTRY(vkGetQueueCheckpointData2NV,							getQueueCheckpointData2NV),
	VK_NULL_FUNC_ENTRY(vkCmdCopyBuffer2KHR,									cmdCopyBuffer2KHR),
	VK_NULL_FUNC_ENTRY(vkCmdCopyImage2KHR,									cmdCopyImage2KHR),
	VK_NULL_FUNC_ENTRY(vkCmdCopyBufferToImage2KHR,							cmdCopyBufferToImage2KHR),
	VK_NULL_FUNC_ENTRY(vkCmdCopyImageToBuffer2KHR,							cmdCopyImageToBuffer2KHR),
	VK_NULL_FUNC_ENTRY(vkCmdBlitImage2KHR,									cmdBlitImage2KHR),
	VK_NULL_FUNC_ENTRY(vkCmdResolveImage2KHR,								cmdResolveImage2KHR),
	VK_NULL_FUNC_ENTRY(vkDebugMarkerSetObjectTagEXT,						debugMarkerSetObjectTagEXT),
	VK_NULL_FUNC_ENTRY(vkDebugMarkerSetObjectNameEXT,						debugMarkerSetObjectNameEXT),
	VK_NULL_FUNC_ENTRY(vkCmdDebugMarkerBeginEXT,							cmdDebugMarkerBeginEXT),
	VK_NULL_FUNC_ENTRY(vkCmdDebugMarkerEndEXT,								cmdDebugMarkerEndEXT),
	VK_NULL_FUNC_ENTRY(vkCmdDebugMarkerInsertEXT,							cmdDebugMarkerInsertEXT),
	VK_NULL_FUNC_ENTRY(vkCmdBindTransformFeedbackBuffersEXT,				cmdBindTransformFeedbackBuffersEXT),
	VK_NULL_FUNC_ENTRY(vkCmdBeginTransformFeedbackEXT,						cmdBeginTransformFeedbackEXT),
	VK_NULL_FUNC_ENTRY(vkCmdEndTransformFeedbackEXT,						cmdEndTransformFeedbackEXT),
	VK_NULL_FUNC_ENTRY(vkCmdBeginQueryIndexedEXT,							cmdBeginQueryIndexedEXT),
	VK_NULL_FUNC_ENTRY(vkCmdEndQueryIndexedEXT,								cmdEndQueryIndexedEXT),
	VK_NULL_FUNC_ENTRY(vkCmdDrawIndirectByteCountEXT,						cmdDrawIndirectByteCountEXT),
	VK_NULL_FUNC_ENTRY(vkCreateCuModuleNVX,									createCuModuleNVX),
	VK_NULL_FUNC_ENTRY(vkCreateCuFunctionNVX,								createCuFunctionNVX),
	VK_NULL_FUNC_ENTRY(vkDestroyCuModuleNVX,								destroyCuModuleNVX),
	VK_NULL_FUNC_ENTRY(vkDestroyCuFunctionNVX,								destroyCuFunctionNVX),
	VK_NULL_FUNC_ENTRY(vkCmdCuLaunchKernelNVX,								cmdCuLaunchKernelNVX),
	VK_NULL_FUNC_ENTRY(vkGetImageViewHandleNVX,								getImageViewHandleNVX),
	VK_NULL_FUNC_ENTRY(vkGetImageViewAddressNVX,							getImageViewAddressNVX),
	VK_NULL_FUNC_ENTRY(vkCmdDrawIndirectCountAMD,							cmdDrawIndirectCountAMD),
	VK_NULL_FUNC_ENTRY(vkCmdDrawIndexedIndirectCountAMD,					cmdDrawIndexedIndirectCountAMD),
	VK_NULL_FUNC_ENTRY(vkGetShaderInfoAMD,									getShaderInfoAMD),
	VK_NULL_FUNC_ENTRY(vkCmdBeginConditionalRenderingEXT,					cmdBeginConditionalRenderingEXT),
	VK_NULL_FUNC_ENTRY(vkCmdEndConditionalRenderingEXT,						cmdEndConditionalRenderingEXT),
	VK_NULL_FUNC_ENTRY(vkCmdSetViewportWScalingNV,							cmdSetViewportWScalingNV),
	VK_NULL_FUNC_ENTRY(vkDisplayPowerControlEXT,							displayPowerControlEXT),
	VK_NULL_FUNC_ENTRY(vkRegisterDeviceEventEXT,							registerDeviceEventEXT),
	VK_NULL_FUNC_ENTRY(vkRegisterDisplayEventEXT,							registerDisplayEventEXT),
	VK_NULL_FUNC_ENTRY(vkGetSwapchainCounterEXT,							getSwapchainCounterEXT),
	VK_NULL_FUNC_ENTRY(vkGetRefreshCycleDurationGOOGLE,						getRefreshCycleDurationGOOGLE),
	VK_NULL_FUNC_ENTRY(vkGetPastPresentationTimingGOOGLE,					getPastPresentationTimingGOOGLE),
	VK_NULL_FUNC_ENTRY(vkCmdSetDiscardRectangleEXT,							cmdSetDiscardRectangleEXT),
	VK_NULL_FUNC_ENTRY(vkSetHdrMetadataEXT,									setHdrMetadataEXT),
	VK_NULL_FUNC_ENTRY(vkSetDebugUtilsObjectNameEXT,						setDebugUtilsObjectNameEXT),
	VK_NULL_FUNC_ENTRY(vkSetDebugUtilsObjectTagEXT,							setDebugUtilsObjectTagEXT),
	VK_NULL_FUNC_ENTRY(vkQueueBeginDebugUtilsLabelEXT,						queueBeginDebugUtilsLabelEXT),
	VK_NULL_FUNC_ENTRY(vkQueueEndDebugUtilsLabelEXT,						queueEndDebugUtilsLabelEXT),
	VK_NULL_FUNC_ENTRY(vkQueueInsertDebugUtilsLabelEXT,						queueInsertDebugUtilsLabelEXT),
	VK_NULL_FUNC_ENTRY(vkCmdBeginDebugUtilsLabelEXT,						cmdBeginDebugUtilsLabelEXT),
	VK_NULL_FUNC_ENTRY(vkCmdEndDebugUtilsLabelEXT,							cmdEndDebugUtilsLabelEXT),
	VK_NULL_FUNC_ENTRY(vkCmdInsertDebugUtilsLabelEXT,						cmdInsertDebugUtilsLabelEXT),
	VK_NULL_FUNC_ENTRY(vkCmdSetSampleLocationsEXT,							cmdSetSampleLocationsEXT),
	VK_NULL_FUNC_ENTRY(vkGetImageDrmFormatModifierPropertiesEXT,			getImageDrmFormatModifierPropertiesEXT),
	VK_NULL_FUNC_ENTRY(vkCreateValidationCacheEXT,							createValidationCacheEXT),
	VK_NULL_FUNC_ENTRY(vkDestroyValidationCacheEXT,							destroyValidationCacheEXT),
	VK_NULL_FUNC_ENTRY(vkMergeValidationCachesEXT,							mergeValidationCachesEXT),
	VK_NULL_FUNC_ENTRY(vkGetValidationCacheDataEXT,							getValidationCacheDataEXT),
	VK_NULL_FUNC_ENTRY(vkCmdBindShadingRateImageNV,							cmdBindShadingRateImageNV),
	VK_NULL_FUNC_ENTRY(vkCmdSetViewportShadingRatePaletteNV,				cmdSetViewportShadingRatePaletteNV),
	VK_NULL_FUNC_ENTRY(vkCmdSetCoarseSampleOrderNV,							cmdSetCoarseSampleOrderNV),
	VK_NULL_FUNC_ENTRY(vkCreateAccelerationStructureNV,						createAccelerationStructureNV),
	VK_NULL_FUNC_ENTRY(vkDestroyAccelerationStructureNV,					destroyAccelerationStructureNV),
	VK_NULL_FUNC_ENTRY(vkGetAccelerationStructureMemoryRequirementsNV,		getAccelerationStructureMemoryRequirementsNV),
	VK_NULL_FUNC_ENTRY(vkBindAccelerationStructureMemoryNV,					bindAccelerationStructureMemoryNV),
	VK_NULL_FUNC_ENTRY(vkCmdBuildAccelerationStructureNV,					cmdBuildAccelerationStructureNV),
	VK_NULL_FUNC_ENTRY(vkCmdCopyAccelerationStructureNV,					cmdCopyAccelerationStructureNV),
	VK_NULL_FUNC_ENTRY(vkCmdTraceRaysNV,									cmdTraceRaysNV),
	VK_NULL_FUNC_ENTRY(vkCreateRayTracingPipelinesNV,						createRayTracingPipelinesNV),
	VK_NULL_FUNC_ENTRY(vkGetRayTracingShaderGroupHandlesKHR,				getRayTracingShaderGroupHandlesKHR),
	VK_NULL_FUNC_ENTRY(vkGetRayTracingShaderGroupHandlesNV,					getRayTracingShaderGroupHandlesNV),
	VK_NULL_FUNC_ENTRY(vkGetAccelerationStructureHandleNV,					getAccelerationStructureHandleNV),
	VK_NULL_FUNC_ENTRY(vkCmdWriteAccelerationStructuresPropertiesNV,		cmdWriteAccelerationStructuresPropertiesNV),
	VK_NULL_FUNC_ENTRY(vkCompileDeferredNV,									compileDeferredNV),
	VK_NULL_FUNC_ENTRY(vkGetMemoryHostPointerPropertiesEXT,					getMemoryHostPointerPropertiesEXT),
	VK_NULL_FUNC_ENTRY(vkCmdWriteBufferMarkerAMD,							cmdWriteBufferMarkerAMD),
	VK_NULL_FUNC_ENTRY(vkGetCalibratedTimestampsEXT,						getCalibratedTimestampsEXT),
	VK_NULL_FUNC_ENTRY(vkCmdDrawMeshTasksNV,								cmdDrawMeshTasksNV),
	VK_NULL_FUNC_ENTRY(vkCmdDrawMeshTasksIndirectNV,						cmdDrawMeshTasksIndirectNV),
	VK_NULL_FUNC_ENTRY(vkCmdDrawMeshTasksIndirectCountNV,					cmdDrawMeshTasksIndirectCountNV),
	VK_NULL_FUNC_ENTRY(vkCmdSetExclusiveScissorNV,							cmdSetExclusiveScissorNV),
	VK_NULL_FUNC_ENTRY(vkCmdSetCheckpointNV,								cmdSetCheckpointNV),
	VK_NULL_FUNC_ENTRY(vkGetQueueCheckpointDataNV,							getQueueCheckpointDataNV),
	VK_NULL_FUNC_ENTRY(vkInitializePerformanceApiINTEL,						initializePerformanceApiINTEL),
	VK_NULL_FUNC_ENTRY(vkUninitializePerformanceApiINTEL,					uninitializePerformanceApiINTEL),
	VK_NULL_FUNC_ENTRY(vkCmdSetPerformanceMarkerINTEL,						cmdSetPerformanceMarkerINTEL),
	VK_NULL_FUNC_ENTRY(vkCmdSetPerformanceStreamMarkerINTEL,				cmdSetPerformanceStreamMarkerINTEL),
	VK_NULL_FUNC_ENTRY(vkCmdSetPerformanceOverrideINTEL,					cmdSetPerformanceOverrideINTEL),
	VK_NULL_FUNC_ENTRY(vkAcquirePerformanceConfigurationINTEL,				acquirePerformanceConfigurationINTEL),
	VK_NULL_FUNC_ENTRY(vkReleasePerformanceConfigurationINTEL,				releasePerformanceConfigurationINTEL),
	VK_NULL_FUNC_ENTRY(vkQueueSetPerformanceConfigurationINTEL,				queueSetPerformanceConfigurationINTEL),
	VK_NULL_FUNC_ENTRY(vkGetPerformanceParameterINTEL,						getPerformanceParameterINTEL),
	VK_NULL_FUNC_ENTRY(vkSetLocalDimmingAMD,								setLocalDimmingAMD),
	VK_NULL_FUNC_ENTRY(vkGetBufferDeviceAddressEXT,							getBufferDeviceAddressEXT),
	VK_NULL_FUNC_ENTRY(vkCmdSetLineStippleEXT,								cmdSetLineStippleEXT),
	VK_NULL_FUNC_ENTRY(vkResetQueryPoolEXT,									resetQueryPool),
	VK_NULL_FUNC_ENTRY(vkCmdSetCullModeEXT,									cmdSetCullModeEXT),
	VK_NULL_FUNC_ENTRY(vkCmdSetFrontFaceEXT,								cmdSetFrontFaceEXT),
	VK_NULL_FUNC_ENTRY(vkCmdSetPrimitiveTopologyEXT,						cmdSetPrimitiveTopologyEXT),
	VK_NULL_FUNC_ENTRY(vkCmdSetViewportWithCountEXT,						cmdSetViewportWithCountEXT),
	VK_NULL_FUNC_ENTRY(vkCmdSetScissorWithCountEXT,							cmdSetScissorWithCountEXT),
	VK_NULL_FUNC_ENTRY(vkCmdBindVertexBuffers2EXT,							cmdBindVertexBuffers2EXT),
	VK_NULL_FUNC_ENTRY(vkCmdSetDepthTestEnableEXT,							cmdSetDepthTestEnableEXT),
	VK_NULL_FUNC_ENTRY(vkCmdSetDepthWriteEnableEXT,							cmdSetDepthWriteEnableEXT),
	VK_NULL_FUNC_ENTRY(vkCmdSetDepthCompareOpEXT,							cmdSetDepthCompareOpEXT),
	VK_NULL_FUNC_ENTRY(vkCmdSetDepthBoundsTestEnableEXT,					cmdSetDepthBoundsTestEnableEXT),
	VK_NULL_FUNC_ENTRY(vkCmdSetStencilTestEnableEXT,						cmdSetStencilTestEnableEXT),
	VK_NULL_FUNC_ENTRY(vkCmdSetStencilOpEXT,								cmdSetStencilOpEXT),
	VK_NULL_FUNC_ENTRY(vkGetGeneratedCommandsMemoryRequirementsNV,			getGeneratedCommandsMemoryRequirementsNV),
	VK_NULL_FUNC_ENTRY(vkCmdPreprocessGeneratedCommandsNV,					cmdPreprocessGeneratedCommandsNV),
	VK_NULL_FUNC_ENTRY(vkCmdExecuteGeneratedCommandsNV,						cmdExecuteGeneratedCommandsNV),
	VK_NULL_FUNC_ENTRY(vkCmdBindPipelineShaderGroupNV,						cmdBindPipelineShaderGroupNV),
	VK_NULL_FUNC_ENTRY(vkCreateIndirectCommandsLayoutNV,					createIndirectCommandsLayoutNV),
	VK_NULL_FUNC_ENTRY(vkDestroyIndirectCommandsLayoutNV,					destroyIndirectCommandsLayoutNV),
	VK_NULL_FUNC_ENTRY(vkCreatePrivateDataSlotEXT,							createPrivateDataSlotEXT),
	VK_NULL_FUNC_ENTRY(vkDestroyPrivateDataSlotEXT,							destroyPrivateDataSlotEXT),
	VK_NULL_FUNC_ENTRY(vkSetPrivateDataEXT,									setPrivateDataEXT),
	VK_NULL_FUNC_ENTRY(vkGetPrivateDataEXT,									getPrivateDataEXT),
	VK_NULL_FUNC_ENTRY(vkCmdSetFragmentShadingRateEnumNV,					cmdSetFragmentShadingRateEnumNV),
	VK_NULL_FUNC_ENTRY(vkCmdSetVertexInputEXT,								cmdSetVertexInputEXT),
	VK_NULL_FUNC_ENTRY(vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI,		getDeviceSubpassShadingMaxWorkgroupSizeHUAWEI),
	VK_NULL_FUNC_ENTRY(vkCmdSubpassShadingHUAWEI,							cmdSubpassShadingHUAWEI),
	VK_NULL_FUNC_ENTRY(vkCmdBindInvocationMaskHUAWEI,						cmdBindInvocationMaskHUAWEI),
	VK_NULL_FUNC_ENTRY(vkGetMemoryRemoteAddressNV,							getMemoryRemoteAddressNV),
	VK_NULL_FUNC_ENTRY(vkCmdSetPatchControlPointsEXT,						cmdSetPatchControlPointsEXT),
	VK_NULL_FUNC_ENTRY(vkCmdSetRasterizerDiscardEnableEXT,					cmdSetRasterizerDiscardEnableEXT),
	VK_NULL_FUNC_ENTRY(vkCmdSetDepthBiasEnableEXT,							cmdSetDepthBiasEnableEXT),
	VK_NULL_FUNC_ENTRY(vkCmdSetLogicOpEXT,									cmdSetLogicOpEXT),
	VK_NULL_FUNC_ENTRY(vkCmdSetPrimitiveRestartEnableEXT,					cmdSetPrimitiveRestartEnableEXT),
	VK_NULL_FUNC_ENTRY(vkCmdSetColorWriteEnableEXT,							cmdSetColorWriteEnableEXT),
	VK_NULL_FUNC_ENTRY(vkCmdDrawMultiEXT,									cmdDrawMultiEXT),
	VK_NULL_FUNC_ENTRY(vkCmdDrawMultiIndexedEXT,							cmdDrawMultiIndexedEXT),
	VK_NULL_FUNC_ENTRY(vkCreateAccelerationStructureKHR,					createAccelerationStructureKHR),
	VK_NULL_FUNC_ENTRY(vkDestroyAccelerationStructureKHR,					destroyAccelerationStructureKHR),
	VK_NULL_FUNC_ENTRY(vkCmdBuildAccelerationStructuresKHR,					cmdBuildAccelerationStructuresKHR),
	VK_NULL_FUNC_ENTRY(vkCmdBuildAccelerationStructuresIndirectKHR,			cmdBuildAccelerationStructuresIndirectKHR),
	VK_NULL_FUNC_ENTRY(vkBuildAccelerationStructuresKHR,					buildAccelerationStructuresKHR),
	VK_NULL_FUNC_ENTRY(vkCopyAccelerationStructureKHR,						copyAccelerationStructureKHR),
	VK_NULL_FUNC_ENTRY(vkCopyAccelerationStructureToMemoryKHR,				copyAccelerationStructureToMemoryKHR),
	VK_NULL_FUNC_ENTRY(vkCopyMemoryToAccelerationStructureKHR,				copyMemoryToAccelerationStructureKHR),
	VK_NULL_FUNC_ENTRY(vkWriteAccelerationStructuresPropertiesKHR,			writeAccelerationStructuresPropertiesKHR),
	VK_NULL_FUNC_ENTRY(vkCmdCopyAccelerationStructureKHR,					cmdCopyAccelerationStructureKHR),
	VK_NULL_FUNC_ENTRY(vkCmdCopyAccelerationStructureToMemoryKHR,			cmdCopyAccelerationStructureToMemoryKHR),
	VK_NULL_FUNC_ENTRY(vkCmdCopyMemoryToAccelerationStructureKHR,			cmdCopyMemoryToAccelerationStructureKHR),
	VK_NULL_FUNC_ENTRY(vkGetAccelerationStructureDeviceAddressKHR,			getAccelerationStructureDeviceAddressKHR),
	VK_NULL_FUNC_ENTRY(vkCmdWriteAccelerationStructuresPropertiesKHR,		cmdWriteAccelerationStructuresPropertiesKHR),
	VK_NULL_FUNC_ENTRY(vkGetDeviceAccelerationStructureCompatibilityKHR,	getDeviceAccelerationStructureCompatibilityKHR),
	VK_NULL_FUNC_ENTRY(vkGetAccelerationStructureBuildSizesKHR,				getAccelerationStructureBuildSizesKHR),
	VK_NULL_FUNC_ENTRY(vkCmdTraceRaysKHR,									cmdTraceRaysKHR),
	VK_NULL_FUNC_ENTRY(vkCreateRayTracingPipelinesKHR,						createRayTracingPipelinesKHR),
	VK_NULL_FUNC_ENTRY(vkGetRayTracingCaptureReplayShaderGroupHandlesKHR,	getRayTracingCaptureReplayShaderGroupHandlesKHR),
	VK_NULL_FUNC_ENTRY(vkCmdTraceRaysIndirectKHR,							cmdTraceRaysIndirectKHR),
	VK_NULL_FUNC_ENTRY(vkGetRayTracingShaderGroupStackSizeKHR,				getRayTracingShaderGroupStackSizeKHR),
	VK_NULL_FUNC_ENTRY(vkCmdSetRayTracingPipelineStackSizeKHR,				cmdSetRayTracingPipelineStackSizeKHR),
	VK_NULL_FUNC_ENTRY(vkGetAndroidHardwareBufferPropertiesANDROID,			getAndroidHardwareBufferPropertiesANDROID),
	VK_NULL_FUNC_ENTRY(vkGetMemoryAndroidHardwareBufferANDROID,				getMemoryAndroidHardwareBufferANDROID),
	VK_NULL_FUNC_ENTRY(vkCreateVideoSessionKHR,								createVideoSessionKHR),
	VK_NULL_FUNC_ENTRY(vkDestroyVideoSessionKHR,							destroyVideoSessionKHR),
	VK_NULL_FUNC_ENTRY(vkGetVideoSessionMemoryRequirementsKHR,				getVideoSessionMemoryRequirementsKHR),
	VK_NULL_FUNC_ENTRY(vkBindVideoSessionMemoryKHR,							bindVideoSessionMemoryKHR),
	VK_NULL_FUNC_ENTRY(vkCreateVideoSessionParametersKHR,					createVideoSessionParametersKHR),
	VK_NULL_FUNC_ENTRY(vkUpdateVideoSessionParametersKHR,					updateVideoSessionParametersKHR),
	VK_NULL_FUNC_ENTRY(vkDestroyVideoSessionParametersKHR,					destroyVideoSessionParametersKHR),
	VK_NULL_FUNC_ENTRY(vkCmdBeginVideoCodingKHR,							cmdBeginVideoCodingKHR),
	VK_NULL_FUNC_ENTRY(vkCmdEndVideoCodingKHR,								cmdEndVideoCodingKHR),
	VK_NULL_FUNC_ENTRY(vkCmdControlVideoCodingKHR,							cmdControlVideoCodingKHR),
	VK_NULL_FUNC_ENTRY(vkCmdDecodeVideoKHR,									cmdDecodeVideoKHR),
	VK_NULL_FUNC_ENTRY(vkCmdEncodeVideoKHR,									cmdEncodeVideoKHR),
	VK_NULL_FUNC_ENTRY(vkGetMemoryZirconHandleFUCHSIA,						getMemoryZirconHandleFUCHSIA),
	VK_NULL_FUNC_ENTRY(vkGetMemoryZirconHandlePropertiesFUCHSIA,			getMemoryZirconHandlePropertiesFUCHSIA),
	VK_NULL_FUNC_ENTRY(vkImportSemaphoreZirconHandleFUCHSIA,				importSemaphoreZirconHandleFUCHSIA),
	VK_NULL_FUNC_ENTRY(vkGetSemaphoreZirconHandleFUCHSIA,					getSemaphoreZirconHandleFUCHSIA),
	VK_NULL_FUNC_ENTRY(vkGetMemoryWin32HandleKHR,							getMemoryWin32HandleKHR),
	VK_NULL_FUNC_ENTRY(vkGetMemoryWin32HandlePropertiesKHR,					getMemoryWin32HandlePropertiesKHR),
	VK_NULL_FUNC_ENTRY(vkImportSemaphoreWin32HandleKHR,						importSemaphoreWin32HandleKHR),
	VK_NULL_FUNC_ENTRY(vkGetSemaphoreWin32HandleKHR,						getSemaphoreWin32HandleKHR),
	VK_NULL_FUNC_ENTRY(vkImportFenceWin32HandleKHR,							importFenceWin32HandleKHR),
	VK_NULL_FUNC_ENTRY(vkGetFenceWin32HandleKHR,							getFenceWin32HandleKHR),
	VK_NULL_FUNC_ENTRY(vkGetMemoryWin32HandleNV,							getMemoryWin32HandleNV),
	VK_NULL_FUNC_ENTRY(vkAcquireFullScreenExclusiveModeEXT,					acquireFullScreenExclusiveModeEXT),
	VK_NULL_FUNC_ENTRY(vkReleaseFullScreenExclusiveModeEXT,					releaseFullScreenExclusiveModeEXT),
	VK_NULL_FUNC_ENTRY(vkGetDeviceGroupSurfacePresentModes2EXT,				getDeviceGroupSurfacePresentModes2EXT),
};

