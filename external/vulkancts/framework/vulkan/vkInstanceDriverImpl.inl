/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 */

VkResult InstanceDriver::destroyInstance (VkInstance instance) const
{
	return m_vk.destroyInstance(instance);
}

VkResult InstanceDriver::enumeratePhysicalDevices (VkInstance instance, deUint32* pPhysicalDeviceCount, VkPhysicalDevice* pPhysicalDevices) const
{
	return m_vk.enumeratePhysicalDevices(instance, pPhysicalDeviceCount, pPhysicalDevices);
}

VkResult InstanceDriver::getPhysicalDeviceFeatures (VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures* pFeatures) const
{
	return m_vk.getPhysicalDeviceFeatures(physicalDevice, pFeatures);
}

VkResult InstanceDriver::getPhysicalDeviceFormatProperties (VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties* pFormatProperties) const
{
	return m_vk.getPhysicalDeviceFormatProperties(physicalDevice, format, pFormatProperties);
}

VkResult InstanceDriver::getPhysicalDeviceImageFormatProperties (VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageFormatProperties* pImageFormatProperties) const
{
	return m_vk.getPhysicalDeviceImageFormatProperties(physicalDevice, format, type, tiling, usage, pImageFormatProperties);
}

VkResult InstanceDriver::getPhysicalDeviceLimits (VkPhysicalDevice physicalDevice, VkPhysicalDeviceLimits* pLimits) const
{
	return m_vk.getPhysicalDeviceLimits(physicalDevice, pLimits);
}

VkResult InstanceDriver::getPhysicalDeviceProperties (VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties* pProperties) const
{
	return m_vk.getPhysicalDeviceProperties(physicalDevice, pProperties);
}

VkResult InstanceDriver::getPhysicalDeviceQueueCount (VkPhysicalDevice physicalDevice, deUint32* pCount) const
{
	return m_vk.getPhysicalDeviceQueueCount(physicalDevice, pCount);
}

VkResult InstanceDriver::getPhysicalDeviceQueueProperties (VkPhysicalDevice physicalDevice, deUint32 count, VkPhysicalDeviceQueueProperties* pQueueProperties) const
{
	return m_vk.getPhysicalDeviceQueueProperties(physicalDevice, count, pQueueProperties);
}

VkResult InstanceDriver::getPhysicalDeviceMemoryProperties (VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties* pMemoryProperties) const
{
	return m_vk.getPhysicalDeviceMemoryProperties(physicalDevice, pMemoryProperties);
}

PFN_vkVoidFunction InstanceDriver::getDeviceProcAddr (VkDevice device, const char* pName) const
{
	return m_vk.getDeviceProcAddr(device, pName);
}

VkResult InstanceDriver::createDevice (VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, VkDevice* pDevice) const
{
	return m_vk.createDevice(physicalDevice, pCreateInfo, pDevice);
}
