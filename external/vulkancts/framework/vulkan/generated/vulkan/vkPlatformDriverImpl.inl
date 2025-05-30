/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 * This file was generated by /scripts/gen_framework.py
 */


VkResult PlatformDriver::createInstance (const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance) const
{
    return m_vk.createInstance(pCreateInfo, pAllocator, pInstance);
}

PFN_vkVoidFunction PlatformDriver::getInstanceProcAddr (VkInstance instance, const char* pName) const
{
    return m_vk.getInstanceProcAddr(instance, pName);
}

VkResult PlatformDriver::enumerateInstanceVersion (uint32_t* pApiVersion) const
{
    if (m_vk.enumerateInstanceVersion)
        return m_vk.enumerateInstanceVersion(pApiVersion);

    *pApiVersion = VK_API_VERSION_1_0;
    return VK_SUCCESS;
}

VkResult PlatformDriver::enumerateInstanceLayerProperties (uint32_t* pPropertyCount, VkLayerProperties* pProperties) const
{
    return m_vk.enumerateInstanceLayerProperties(pPropertyCount, pProperties);
}

VkResult PlatformDriver::enumerateInstanceExtensionProperties (const char* pLayerName, uint32_t* pPropertyCount, VkExtensionProperties* pProperties) const
{
    return m_vk.enumerateInstanceExtensionProperties(pLayerName, pPropertyCount, pProperties);
}

void PlatformDriver::getExternalComputeQueueDataNV (VkExternalComputeQueueNV externalQueue, VkExternalComputeQueueDataParamsNV* params, void* pData) const
{
    m_vk.getExternalComputeQueueDataNV(externalQueue, params, pData);
}
