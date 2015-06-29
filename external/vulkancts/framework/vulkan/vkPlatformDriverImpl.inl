/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 */

VkResult PlatformDriver::createInstance (const VkInstanceCreateInfo* pCreateInfo, VkInstance* pInstance) const
{
	return m_vk.createInstance(pCreateInfo, pInstance);
}

VkResult PlatformDriver::destroyInstance (VkInstance instance) const
{
	return m_vk.destroyInstance(instance);
}

VkResult PlatformDriver::enumeratePhysicalDevices (VkInstance instance, deUint32* pPhysicalDeviceCount, VkPhysicalDevice* pPhysicalDevices) const
{
	return m_vk.enumeratePhysicalDevices(instance, pPhysicalDeviceCount, pPhysicalDevices);
}

FunctionPtr PlatformDriver::getProcAddr (VkPhysicalDevice physicalDevice, const char* pName) const
{
	return m_vk.getProcAddr(physicalDevice, pName);
}
