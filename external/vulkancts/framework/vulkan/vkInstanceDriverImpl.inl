/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 */

void InstanceDriver::destroyInstance (VkInstance instance) const
{
	m_vk.destroyInstance(instance);
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

VkResult InstanceDriver::getPhysicalDeviceImageFormatProperties (VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties* pImageFormatProperties) const
{
	return m_vk.getPhysicalDeviceImageFormatProperties(physicalDevice, format, type, tiling, usage, flags, pImageFormatProperties);
}

VkResult InstanceDriver::getPhysicalDeviceProperties (VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties* pProperties) const
{
	return m_vk.getPhysicalDeviceProperties(physicalDevice, pProperties);
}

VkResult InstanceDriver::getPhysicalDeviceQueueFamilyProperties (VkPhysicalDevice physicalDevice, deUint32* pCount, VkQueueFamilyProperties* pQueueFamilyProperties) const
{
	return m_vk.getPhysicalDeviceQueueFamilyProperties(physicalDevice, pCount, pQueueFamilyProperties);
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

VkResult InstanceDriver::enumerateDeviceExtensionProperties (VkPhysicalDevice physicalDevice, const char* pLayerName, deUint32* pCount, VkExtensionProperties* pProperties) const
{
	return m_vk.enumerateDeviceExtensionProperties(physicalDevice, pLayerName, pCount, pProperties);
}

VkResult InstanceDriver::enumerateDeviceLayerProperties (VkPhysicalDevice physicalDevice, deUint32* pCount, VkLayerProperties* pProperties) const
{
	return m_vk.enumerateDeviceLayerProperties(physicalDevice, pCount, pProperties);
}
