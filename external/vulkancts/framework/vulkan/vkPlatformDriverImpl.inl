/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 */

VkResult PlatformDriver::createInstance (const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance) const
{
	return m_vk.createInstance(pCreateInfo, pAllocator, pInstance);
}

PFN_vkVoidFunction PlatformDriver::getInstanceProcAddr (VkInstance instance, const char* pName) const
{
	return m_vk.getInstanceProcAddr(instance, pName);
}

VkResult PlatformDriver::enumerateInstanceExtensionProperties (const char* pLayerName, deUint32* pPropertyCount, VkExtensionProperties* pProperties) const
{
	return m_vk.enumerateInstanceExtensionProperties(pLayerName, pPropertyCount, pProperties);
}

VkResult PlatformDriver::enumerateInstanceLayerProperties (deUint32* pPropertyCount, VkLayerProperties* pProperties) const
{
	return m_vk.enumerateInstanceLayerProperties(pPropertyCount, pProperties);
}

VkResult PlatformDriver::enumerateInstanceVersion (deUint32* pApiVersion) const
{
	if (m_vk.enumerateInstanceVersion)
		return m_vk.enumerateInstanceVersion(pApiVersion);

	*pApiVersion = VK_API_VERSION_1_0;
	return VK_SUCCESS;
}
