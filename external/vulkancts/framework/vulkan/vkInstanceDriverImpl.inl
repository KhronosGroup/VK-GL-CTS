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

PFN_vkVoidFunction InstanceDriver::getDeviceProcAddr (VkDevice device, const char* pName) const
{
	return m_vk.getDeviceProcAddr(device, pName);
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

VkResult InstanceDriver::createXlibSurfaceKHR (VkInstance instance, const VkXlibSurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface) const
{
	return m_vk.createXlibSurfaceKHR(instance, pCreateInfo, pAllocator, pSurface);
}

VkBool32 InstanceDriver::getPhysicalDeviceXlibPresentationSupportKHR (VkPhysicalDevice physicalDevice, deUint32 queueFamilyIndex, pt::XlibDisplayPtr dpy, pt::XlibVisualID visualID) const
{
	return m_vk.getPhysicalDeviceXlibPresentationSupportKHR(physicalDevice, queueFamilyIndex, dpy, visualID);
}

VkResult InstanceDriver::createXcbSurfaceKHR (VkInstance instance, const VkXcbSurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface) const
{
	return m_vk.createXcbSurfaceKHR(instance, pCreateInfo, pAllocator, pSurface);
}

VkBool32 InstanceDriver::getPhysicalDeviceXcbPresentationSupportKHR (VkPhysicalDevice physicalDevice, deUint32 queueFamilyIndex, pt::XcbConnectionPtr connection, pt::XcbVisualid visual_id) const
{
	return m_vk.getPhysicalDeviceXcbPresentationSupportKHR(physicalDevice, queueFamilyIndex, connection, visual_id);
}

VkResult InstanceDriver::createWaylandSurfaceKHR (VkInstance instance, const VkWaylandSurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface) const
{
	return m_vk.createWaylandSurfaceKHR(instance, pCreateInfo, pAllocator, pSurface);
}

VkBool32 InstanceDriver::getPhysicalDeviceWaylandPresentationSupportKHR (VkPhysicalDevice physicalDevice, deUint32 queueFamilyIndex, pt::WaylandDisplayPtr display) const
{
	return m_vk.getPhysicalDeviceWaylandPresentationSupportKHR(physicalDevice, queueFamilyIndex, display);
}

VkResult InstanceDriver::createMirSurfaceKHR (VkInstance instance, const VkMirSurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface) const
{
	return m_vk.createMirSurfaceKHR(instance, pCreateInfo, pAllocator, pSurface);
}

VkBool32 InstanceDriver::getPhysicalDeviceMirPresentationSupportKHR (VkPhysicalDevice physicalDevice, deUint32 queueFamilyIndex, pt::MirConnectionPtr connection) const
{
	return m_vk.getPhysicalDeviceMirPresentationSupportKHR(physicalDevice, queueFamilyIndex, connection);
}

VkResult InstanceDriver::createAndroidSurfaceKHR (VkInstance instance, const VkAndroidSurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface) const
{
	return m_vk.createAndroidSurfaceKHR(instance, pCreateInfo, pAllocator, pSurface);
}

VkResult InstanceDriver::createWin32SurfaceKHR (VkInstance instance, const VkWin32SurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface) const
{
	return m_vk.createWin32SurfaceKHR(instance, pCreateInfo, pAllocator, pSurface);
}

VkBool32 InstanceDriver::getPhysicalDeviceWin32PresentationSupportKHR (VkPhysicalDevice physicalDevice, deUint32 queueFamilyIndex) const
{
	return m_vk.getPhysicalDeviceWin32PresentationSupportKHR(physicalDevice, queueFamilyIndex);
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
