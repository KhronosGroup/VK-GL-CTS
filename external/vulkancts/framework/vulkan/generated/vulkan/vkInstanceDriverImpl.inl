/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 */

void InstanceDriver::destroyInstance (VkInstance instance, const VkAllocationCallbacks* pAllocator) const
{
	m_vk.destroyInstance(instance, pAllocator);
}

VkResult InstanceDriver::enumeratePhysicalDevices (VkInstance instance, deUint32* pPhysicalDeviceCount, VkPhysicalDevice* pPhysicalDevices) const
{
	return m_vk.enumeratePhysicalDevices(instance, pPhysicalDeviceCount, pPhysicalDevices);
}

void InstanceDriver::getPhysicalDeviceFeatures (VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures* pFeatures) const
{
	m_vk.getPhysicalDeviceFeatures(physicalDevice, pFeatures);
}

void InstanceDriver::getPhysicalDeviceFormatProperties (VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties* pFormatProperties) const
{
	m_vk.getPhysicalDeviceFormatProperties(physicalDevice, format, pFormatProperties);
}

VkResult InstanceDriver::getPhysicalDeviceImageFormatProperties (VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties* pImageFormatProperties) const
{
	return m_vk.getPhysicalDeviceImageFormatProperties(physicalDevice, format, type, tiling, usage, flags, pImageFormatProperties);
}

void InstanceDriver::getPhysicalDeviceProperties (VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties* pProperties) const
{
	m_vk.getPhysicalDeviceProperties(physicalDevice, pProperties);
}

void InstanceDriver::getPhysicalDeviceQueueFamilyProperties (VkPhysicalDevice physicalDevice, deUint32* pQueueFamilyPropertyCount, VkQueueFamilyProperties* pQueueFamilyProperties) const
{
	m_vk.getPhysicalDeviceQueueFamilyProperties(physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
}

void InstanceDriver::getPhysicalDeviceMemoryProperties (VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties* pMemoryProperties) const
{
	m_vk.getPhysicalDeviceMemoryProperties(physicalDevice, pMemoryProperties);
}

VkResult InstanceDriver::createDevice (VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice) const
{
	return m_vk.createDevice(physicalDevice, pCreateInfo, pAllocator, pDevice);
}

VkResult InstanceDriver::enumerateDeviceExtensionProperties (VkPhysicalDevice physicalDevice, const char* pLayerName, deUint32* pPropertyCount, VkExtensionProperties* pProperties) const
{
	return m_vk.enumerateDeviceExtensionProperties(physicalDevice, pLayerName, pPropertyCount, pProperties);
}

VkResult InstanceDriver::enumerateDeviceLayerProperties (VkPhysicalDevice physicalDevice, deUint32* pPropertyCount, VkLayerProperties* pProperties) const
{
	return m_vk.enumerateDeviceLayerProperties(physicalDevice, pPropertyCount, pProperties);
}

void InstanceDriver::getPhysicalDeviceSparseImageFormatProperties (VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkSampleCountFlagBits samples, VkImageUsageFlags usage, VkImageTiling tiling, deUint32* pPropertyCount, VkSparseImageFormatProperties* pProperties) const
{
	m_vk.getPhysicalDeviceSparseImageFormatProperties(physicalDevice, format, type, samples, usage, tiling, pPropertyCount, pProperties);
}

VkResult InstanceDriver::enumeratePhysicalDeviceGroups (VkInstance instance, deUint32* pPhysicalDeviceGroupCount, VkPhysicalDeviceGroupProperties* pPhysicalDeviceGroupProperties) const
{
	return m_vk.enumeratePhysicalDeviceGroups(instance, pPhysicalDeviceGroupCount, pPhysicalDeviceGroupProperties);
}

void InstanceDriver::getPhysicalDeviceFeatures2 (VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures2* pFeatures) const
{
	vk::VkPhysicalDeviceProperties props;
	m_vk.getPhysicalDeviceProperties(physicalDevice, &props);
	if (props.apiVersion >= VK_API_VERSION_1_1)
		m_vk.getPhysicalDeviceFeatures2(physicalDevice, pFeatures);
	else
		m_vk.getPhysicalDeviceFeatures2KHR(physicalDevice, pFeatures);
}

void InstanceDriver::getPhysicalDeviceProperties2 (VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties2* pProperties) const
{
	vk::VkPhysicalDeviceProperties props;
	m_vk.getPhysicalDeviceProperties(physicalDevice, &props);
	if (props.apiVersion >= VK_API_VERSION_1_1)
		m_vk.getPhysicalDeviceProperties2(physicalDevice, pProperties);
	else
		m_vk.getPhysicalDeviceProperties2KHR(physicalDevice, pProperties);
}

void InstanceDriver::getPhysicalDeviceFormatProperties2 (VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties2* pFormatProperties) const
{
	vk::VkPhysicalDeviceProperties props;
	m_vk.getPhysicalDeviceProperties(physicalDevice, &props);
	if (props.apiVersion >= VK_API_VERSION_1_1)
		m_vk.getPhysicalDeviceFormatProperties2(physicalDevice, format, pFormatProperties);
	else
		m_vk.getPhysicalDeviceFormatProperties2KHR(physicalDevice, format, pFormatProperties);
}

VkResult InstanceDriver::getPhysicalDeviceImageFormatProperties2 (VkPhysicalDevice physicalDevice, const VkPhysicalDeviceImageFormatInfo2* pImageFormatInfo, VkImageFormatProperties2* pImageFormatProperties) const
{
	vk::VkPhysicalDeviceProperties props;
	m_vk.getPhysicalDeviceProperties(physicalDevice, &props);
	if (props.apiVersion >= VK_API_VERSION_1_1)
		return m_vk.getPhysicalDeviceImageFormatProperties2(physicalDevice, pImageFormatInfo, pImageFormatProperties);
	else
		return m_vk.getPhysicalDeviceImageFormatProperties2KHR(physicalDevice, pImageFormatInfo, pImageFormatProperties);
}

void InstanceDriver::getPhysicalDeviceQueueFamilyProperties2 (VkPhysicalDevice physicalDevice, deUint32* pQueueFamilyPropertyCount, VkQueueFamilyProperties2* pQueueFamilyProperties) const
{
	vk::VkPhysicalDeviceProperties props;
	m_vk.getPhysicalDeviceProperties(physicalDevice, &props);
	if (props.apiVersion >= VK_API_VERSION_1_1)
		m_vk.getPhysicalDeviceQueueFamilyProperties2(physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
	else
		m_vk.getPhysicalDeviceQueueFamilyProperties2KHR(physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
}

void InstanceDriver::getPhysicalDeviceMemoryProperties2 (VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties2* pMemoryProperties) const
{
	vk::VkPhysicalDeviceProperties props;
	m_vk.getPhysicalDeviceProperties(physicalDevice, &props);
	if (props.apiVersion >= VK_API_VERSION_1_1)
		m_vk.getPhysicalDeviceMemoryProperties2(physicalDevice, pMemoryProperties);
	else
		m_vk.getPhysicalDeviceMemoryProperties2KHR(physicalDevice, pMemoryProperties);
}

void InstanceDriver::getPhysicalDeviceSparseImageFormatProperties2 (VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSparseImageFormatInfo2* pFormatInfo, deUint32* pPropertyCount, VkSparseImageFormatProperties2* pProperties) const
{
	vk::VkPhysicalDeviceProperties props;
	m_vk.getPhysicalDeviceProperties(physicalDevice, &props);
	if (props.apiVersion >= VK_API_VERSION_1_1)
		m_vk.getPhysicalDeviceSparseImageFormatProperties2(physicalDevice, pFormatInfo, pPropertyCount, pProperties);
	else
		m_vk.getPhysicalDeviceSparseImageFormatProperties2KHR(physicalDevice, pFormatInfo, pPropertyCount, pProperties);
}

void InstanceDriver::getPhysicalDeviceExternalBufferProperties (VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalBufferInfo* pExternalBufferInfo, VkExternalBufferProperties* pExternalBufferProperties) const
{
	vk::VkPhysicalDeviceProperties props;
	m_vk.getPhysicalDeviceProperties(physicalDevice, &props);
	if (props.apiVersion >= VK_API_VERSION_1_1)
		m_vk.getPhysicalDeviceExternalBufferProperties(physicalDevice, pExternalBufferInfo, pExternalBufferProperties);
	else
		m_vk.getPhysicalDeviceExternalBufferPropertiesKHR(physicalDevice, pExternalBufferInfo, pExternalBufferProperties);
}

void InstanceDriver::getPhysicalDeviceExternalFenceProperties (VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalFenceInfo* pExternalFenceInfo, VkExternalFenceProperties* pExternalFenceProperties) const
{
	vk::VkPhysicalDeviceProperties props;
	m_vk.getPhysicalDeviceProperties(physicalDevice, &props);
	if (props.apiVersion >= VK_API_VERSION_1_1)
		m_vk.getPhysicalDeviceExternalFenceProperties(physicalDevice, pExternalFenceInfo, pExternalFenceProperties);
	else
		m_vk.getPhysicalDeviceExternalFencePropertiesKHR(physicalDevice, pExternalFenceInfo, pExternalFenceProperties);
}

void InstanceDriver::getPhysicalDeviceExternalSemaphoreProperties (VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo* pExternalSemaphoreInfo, VkExternalSemaphoreProperties* pExternalSemaphoreProperties) const
{
	vk::VkPhysicalDeviceProperties props;
	m_vk.getPhysicalDeviceProperties(physicalDevice, &props);
	if (props.apiVersion >= VK_API_VERSION_1_1)
		m_vk.getPhysicalDeviceExternalSemaphoreProperties(physicalDevice, pExternalSemaphoreInfo, pExternalSemaphoreProperties);
	else
		m_vk.getPhysicalDeviceExternalSemaphorePropertiesKHR(physicalDevice, pExternalSemaphoreInfo, pExternalSemaphoreProperties);
}

void InstanceDriver::destroySurfaceKHR (VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks* pAllocator) const
{
	m_vk.destroySurfaceKHR(instance, surface, pAllocator);
}

VkResult InstanceDriver::getPhysicalDeviceSurfaceSupportKHR (VkPhysicalDevice physicalDevice, deUint32 queueFamilyIndex, VkSurfaceKHR surface, VkBool32* pSupported) const
{
	return m_vk.getPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamilyIndex, surface, pSupported);
}

VkResult InstanceDriver::getPhysicalDeviceSurfaceCapabilitiesKHR (VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR* pSurfaceCapabilities) const
{
	return m_vk.getPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, pSurfaceCapabilities);
}

VkResult InstanceDriver::getPhysicalDeviceSurfaceFormatsKHR (VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, deUint32* pSurfaceFormatCount, VkSurfaceFormatKHR* pSurfaceFormats) const
{
	return m_vk.getPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, pSurfaceFormatCount, pSurfaceFormats);
}

VkResult InstanceDriver::getPhysicalDeviceSurfacePresentModesKHR (VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, deUint32* pPresentModeCount, VkPresentModeKHR* pPresentModes) const
{
	return m_vk.getPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, pPresentModeCount, pPresentModes);
}

VkResult InstanceDriver::getPhysicalDevicePresentRectanglesKHR (VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, deUint32* pRectCount, VkRect2D* pRects) const
{
	return m_vk.getPhysicalDevicePresentRectanglesKHR(physicalDevice, surface, pRectCount, pRects);
}

VkResult InstanceDriver::getPhysicalDeviceDisplayPropertiesKHR (VkPhysicalDevice physicalDevice, deUint32* pPropertyCount, VkDisplayPropertiesKHR* pProperties) const
{
	return m_vk.getPhysicalDeviceDisplayPropertiesKHR(physicalDevice, pPropertyCount, pProperties);
}

VkResult InstanceDriver::getPhysicalDeviceDisplayPlanePropertiesKHR (VkPhysicalDevice physicalDevice, deUint32* pPropertyCount, VkDisplayPlanePropertiesKHR* pProperties) const
{
	return m_vk.getPhysicalDeviceDisplayPlanePropertiesKHR(physicalDevice, pPropertyCount, pProperties);
}

VkResult InstanceDriver::getDisplayPlaneSupportedDisplaysKHR (VkPhysicalDevice physicalDevice, deUint32 planeIndex, deUint32* pDisplayCount, VkDisplayKHR* pDisplays) const
{
	return m_vk.getDisplayPlaneSupportedDisplaysKHR(physicalDevice, planeIndex, pDisplayCount, pDisplays);
}

VkResult InstanceDriver::getDisplayModePropertiesKHR (VkPhysicalDevice physicalDevice, VkDisplayKHR display, deUint32* pPropertyCount, VkDisplayModePropertiesKHR* pProperties) const
{
	return m_vk.getDisplayModePropertiesKHR(physicalDevice, display, pPropertyCount, pProperties);
}

VkResult InstanceDriver::createDisplayModeKHR (VkPhysicalDevice physicalDevice, VkDisplayKHR display, const VkDisplayModeCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDisplayModeKHR* pMode) const
{
	return m_vk.createDisplayModeKHR(physicalDevice, display, pCreateInfo, pAllocator, pMode);
}

VkResult InstanceDriver::getDisplayPlaneCapabilitiesKHR (VkPhysicalDevice physicalDevice, VkDisplayModeKHR mode, deUint32 planeIndex, VkDisplayPlaneCapabilitiesKHR* pCapabilities) const
{
	return m_vk.getDisplayPlaneCapabilitiesKHR(physicalDevice, mode, planeIndex, pCapabilities);
}

VkResult InstanceDriver::createDisplayPlaneSurfaceKHR (VkInstance instance, const VkDisplaySurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface) const
{
	return m_vk.createDisplayPlaneSurfaceKHR(instance, pCreateInfo, pAllocator, pSurface);
}

VkResult InstanceDriver::enumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR (VkPhysicalDevice physicalDevice, deUint32 queueFamilyIndex, deUint32* pCounterCount, VkPerformanceCounterKHR* pCounters, VkPerformanceCounterDescriptionKHR* pCounterDescriptions) const
{
	return m_vk.enumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR(physicalDevice, queueFamilyIndex, pCounterCount, pCounters, pCounterDescriptions);
}

void InstanceDriver::getPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR (VkPhysicalDevice physicalDevice, const VkQueryPoolPerformanceCreateInfoKHR* pPerformanceQueryCreateInfo, deUint32* pNumPasses) const
{
	m_vk.getPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR(physicalDevice, pPerformanceQueryCreateInfo, pNumPasses);
}

VkResult InstanceDriver::getPhysicalDeviceSurfaceCapabilities2KHR (VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSurfaceInfo2KHR* pSurfaceInfo, VkSurfaceCapabilities2KHR* pSurfaceCapabilities) const
{
	return m_vk.getPhysicalDeviceSurfaceCapabilities2KHR(physicalDevice, pSurfaceInfo, pSurfaceCapabilities);
}

VkResult InstanceDriver::getPhysicalDeviceSurfaceFormats2KHR (VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSurfaceInfo2KHR* pSurfaceInfo, deUint32* pSurfaceFormatCount, VkSurfaceFormat2KHR* pSurfaceFormats) const
{
	return m_vk.getPhysicalDeviceSurfaceFormats2KHR(physicalDevice, pSurfaceInfo, pSurfaceFormatCount, pSurfaceFormats);
}

VkResult InstanceDriver::getPhysicalDeviceDisplayProperties2KHR (VkPhysicalDevice physicalDevice, deUint32* pPropertyCount, VkDisplayProperties2KHR* pProperties) const
{
	return m_vk.getPhysicalDeviceDisplayProperties2KHR(physicalDevice, pPropertyCount, pProperties);
}

VkResult InstanceDriver::getPhysicalDeviceDisplayPlaneProperties2KHR (VkPhysicalDevice physicalDevice, deUint32* pPropertyCount, VkDisplayPlaneProperties2KHR* pProperties) const
{
	return m_vk.getPhysicalDeviceDisplayPlaneProperties2KHR(physicalDevice, pPropertyCount, pProperties);
}

VkResult InstanceDriver::getDisplayModeProperties2KHR (VkPhysicalDevice physicalDevice, VkDisplayKHR display, deUint32* pPropertyCount, VkDisplayModeProperties2KHR* pProperties) const
{
	return m_vk.getDisplayModeProperties2KHR(physicalDevice, display, pPropertyCount, pProperties);
}

VkResult InstanceDriver::getDisplayPlaneCapabilities2KHR (VkPhysicalDevice physicalDevice, const VkDisplayPlaneInfo2KHR* pDisplayPlaneInfo, VkDisplayPlaneCapabilities2KHR* pCapabilities) const
{
	return m_vk.getDisplayPlaneCapabilities2KHR(physicalDevice, pDisplayPlaneInfo, pCapabilities);
}

VkResult InstanceDriver::getPhysicalDeviceFragmentShadingRatesKHR (VkPhysicalDevice physicalDevice, deUint32* pFragmentShadingRateCount, VkPhysicalDeviceFragmentShadingRateKHR* pFragmentShadingRates) const
{
	return m_vk.getPhysicalDeviceFragmentShadingRatesKHR(physicalDevice, pFragmentShadingRateCount, pFragmentShadingRates);
}

VkResult InstanceDriver::createDebugReportCallbackEXT (VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugReportCallbackEXT* pCallback) const
{
	return m_vk.createDebugReportCallbackEXT(instance, pCreateInfo, pAllocator, pCallback);
}

void InstanceDriver::destroyDebugReportCallbackEXT (VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* pAllocator) const
{
	m_vk.destroyDebugReportCallbackEXT(instance, callback, pAllocator);
}

void InstanceDriver::debugReportMessageEXT (VkInstance instance, VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, deUint64 object, deUintptr location, deInt32 messageCode, const char* pLayerPrefix, const char* pMessage) const
{
	m_vk.debugReportMessageEXT(instance, flags, objectType, object, location, messageCode, pLayerPrefix, pMessage);
}

VkResult InstanceDriver::getPhysicalDeviceExternalImageFormatPropertiesNV (VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkExternalMemoryHandleTypeFlagsNV externalHandleType, VkExternalImageFormatPropertiesNV* pExternalImageFormatProperties) const
{
	return m_vk.getPhysicalDeviceExternalImageFormatPropertiesNV(physicalDevice, format, type, tiling, usage, flags, externalHandleType, pExternalImageFormatProperties);
}

VkResult InstanceDriver::releaseDisplayEXT (VkPhysicalDevice physicalDevice, VkDisplayKHR display) const
{
	return m_vk.releaseDisplayEXT(physicalDevice, display);
}

VkResult InstanceDriver::getPhysicalDeviceSurfaceCapabilities2EXT (VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkSurfaceCapabilities2EXT* pSurfaceCapabilities) const
{
	return m_vk.getPhysicalDeviceSurfaceCapabilities2EXT(physicalDevice, surface, pSurfaceCapabilities);
}

VkResult InstanceDriver::createDebugUtilsMessengerEXT (VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pMessenger) const
{
	return m_vk.createDebugUtilsMessengerEXT(instance, pCreateInfo, pAllocator, pMessenger);
}

void InstanceDriver::destroyDebugUtilsMessengerEXT (VkInstance instance, VkDebugUtilsMessengerEXT messenger, const VkAllocationCallbacks* pAllocator) const
{
	m_vk.destroyDebugUtilsMessengerEXT(instance, messenger, pAllocator);
}

void InstanceDriver::submitDebugUtilsMessageEXT (VkInstance instance, VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData) const
{
	m_vk.submitDebugUtilsMessageEXT(instance, messageSeverity, messageTypes, pCallbackData);
}

void InstanceDriver::getPhysicalDeviceMultisamplePropertiesEXT (VkPhysicalDevice physicalDevice, VkSampleCountFlagBits samples, VkMultisamplePropertiesEXT* pMultisampleProperties) const
{
	m_vk.getPhysicalDeviceMultisamplePropertiesEXT(physicalDevice, samples, pMultisampleProperties);
}

VkResult InstanceDriver::getPhysicalDeviceCalibrateableTimeDomainsEXT (VkPhysicalDevice physicalDevice, deUint32* pTimeDomainCount, VkTimeDomainEXT* pTimeDomains) const
{
	return m_vk.getPhysicalDeviceCalibrateableTimeDomainsEXT(physicalDevice, pTimeDomainCount, pTimeDomains);
}

VkResult InstanceDriver::getPhysicalDeviceToolPropertiesEXT (VkPhysicalDevice physicalDevice, deUint32* pToolCount, VkPhysicalDeviceToolPropertiesEXT* pToolProperties) const
{
	return m_vk.getPhysicalDeviceToolPropertiesEXT(physicalDevice, pToolCount, pToolProperties);
}

VkResult InstanceDriver::getPhysicalDeviceCooperativeMatrixPropertiesNV (VkPhysicalDevice physicalDevice, deUint32* pPropertyCount, VkCooperativeMatrixPropertiesNV* pProperties) const
{
	return m_vk.getPhysicalDeviceCooperativeMatrixPropertiesNV(physicalDevice, pPropertyCount, pProperties);
}

VkResult InstanceDriver::getPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV (VkPhysicalDevice physicalDevice, deUint32* pCombinationCount, VkFramebufferMixedSamplesCombinationNV* pCombinations) const
{
	return m_vk.getPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV(physicalDevice, pCombinationCount, pCombinations);
}

VkResult InstanceDriver::createHeadlessSurfaceEXT (VkInstance instance, const VkHeadlessSurfaceCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface) const
{
	return m_vk.createHeadlessSurfaceEXT(instance, pCreateInfo, pAllocator, pSurface);
}

VkResult InstanceDriver::acquireWinrtDisplayNV (VkPhysicalDevice physicalDevice, VkDisplayKHR display) const
{
	return m_vk.acquireWinrtDisplayNV(physicalDevice, display);
}

VkResult InstanceDriver::getWinrtDisplayNV (VkPhysicalDevice physicalDevice, deUint32 deviceRelativeId, VkDisplayKHR* pDisplay) const
{
	return m_vk.getWinrtDisplayNV(physicalDevice, deviceRelativeId, pDisplay);
}

VkResult InstanceDriver::createAndroidSurfaceKHR (VkInstance instance, const VkAndroidSurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface) const
{
	return m_vk.createAndroidSurfaceKHR(instance, pCreateInfo, pAllocator, pSurface);
}

VkResult InstanceDriver::createImagePipeSurfaceFUCHSIA (VkInstance instance, const VkImagePipeSurfaceCreateInfoFUCHSIA* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface) const
{
	return m_vk.createImagePipeSurfaceFUCHSIA(instance, pCreateInfo, pAllocator, pSurface);
}

VkResult InstanceDriver::createStreamDescriptorSurfaceGGP (VkInstance instance, const VkStreamDescriptorSurfaceCreateInfoGGP* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface) const
{
	return m_vk.createStreamDescriptorSurfaceGGP(instance, pCreateInfo, pAllocator, pSurface);
}

VkResult InstanceDriver::createIOSSurfaceMVK (VkInstance instance, const VkIOSSurfaceCreateInfoMVK* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface) const
{
	return m_vk.createIOSSurfaceMVK(instance, pCreateInfo, pAllocator, pSurface);
}

VkResult InstanceDriver::createMacOSSurfaceMVK (VkInstance instance, const VkMacOSSurfaceCreateInfoMVK* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface) const
{
	return m_vk.createMacOSSurfaceMVK(instance, pCreateInfo, pAllocator, pSurface);
}

VkResult InstanceDriver::createMetalSurfaceEXT (VkInstance instance, const VkMetalSurfaceCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface) const
{
	return m_vk.createMetalSurfaceEXT(instance, pCreateInfo, pAllocator, pSurface);
}

VkResult InstanceDriver::createViSurfaceNN (VkInstance instance, const VkViSurfaceCreateInfoNN* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface) const
{
	return m_vk.createViSurfaceNN(instance, pCreateInfo, pAllocator, pSurface);
}

VkResult InstanceDriver::createWaylandSurfaceKHR (VkInstance instance, const VkWaylandSurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface) const
{
	return m_vk.createWaylandSurfaceKHR(instance, pCreateInfo, pAllocator, pSurface);
}

VkBool32 InstanceDriver::getPhysicalDeviceWaylandPresentationSupportKHR (VkPhysicalDevice physicalDevice, deUint32 queueFamilyIndex, pt::WaylandDisplayPtr display) const
{
	return m_vk.getPhysicalDeviceWaylandPresentationSupportKHR(physicalDevice, queueFamilyIndex, display);
}

VkResult InstanceDriver::createWin32SurfaceKHR (VkInstance instance, const VkWin32SurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface) const
{
	return m_vk.createWin32SurfaceKHR(instance, pCreateInfo, pAllocator, pSurface);
}

VkBool32 InstanceDriver::getPhysicalDeviceWin32PresentationSupportKHR (VkPhysicalDevice physicalDevice, deUint32 queueFamilyIndex) const
{
	return m_vk.getPhysicalDeviceWin32PresentationSupportKHR(physicalDevice, queueFamilyIndex);
}

VkResult InstanceDriver::getPhysicalDeviceSurfacePresentModes2EXT (VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSurfaceInfo2KHR* pSurfaceInfo, deUint32* pPresentModeCount, VkPresentModeKHR* pPresentModes) const
{
	return m_vk.getPhysicalDeviceSurfacePresentModes2EXT(physicalDevice, pSurfaceInfo, pPresentModeCount, pPresentModes);
}

VkResult InstanceDriver::createXcbSurfaceKHR (VkInstance instance, const VkXcbSurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface) const
{
	return m_vk.createXcbSurfaceKHR(instance, pCreateInfo, pAllocator, pSurface);
}

VkBool32 InstanceDriver::getPhysicalDeviceXcbPresentationSupportKHR (VkPhysicalDevice physicalDevice, deUint32 queueFamilyIndex, pt::XcbConnectionPtr connection, pt::XcbVisualid visual_id) const
{
	return m_vk.getPhysicalDeviceXcbPresentationSupportKHR(physicalDevice, queueFamilyIndex, connection, visual_id);
}

VkResult InstanceDriver::createXlibSurfaceKHR (VkInstance instance, const VkXlibSurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface) const
{
	return m_vk.createXlibSurfaceKHR(instance, pCreateInfo, pAllocator, pSurface);
}

VkBool32 InstanceDriver::getPhysicalDeviceXlibPresentationSupportKHR (VkPhysicalDevice physicalDevice, deUint32 queueFamilyIndex, pt::XlibDisplayPtr dpy, pt::XlibVisualID visualID) const
{
	return m_vk.getPhysicalDeviceXlibPresentationSupportKHR(physicalDevice, queueFamilyIndex, dpy, visualID);
}

VkResult InstanceDriver::acquireXlibDisplayEXT (VkPhysicalDevice physicalDevice, pt::XlibDisplayPtr dpy, VkDisplayKHR display) const
{
	return m_vk.acquireXlibDisplayEXT(physicalDevice, dpy, display);
}

VkResult InstanceDriver::getRandROutputDisplayEXT (VkPhysicalDevice physicalDevice, pt::XlibDisplayPtr dpy, pt::RROutput rrOutput, VkDisplayKHR* pDisplay) const
{
	return m_vk.getRandROutputDisplayEXT(physicalDevice, dpy, rrOutput, pDisplay);
}
