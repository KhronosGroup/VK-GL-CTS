/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 * This file was generated by /scripts/gen_framework.py
 */

virtual VkResult			createInstance							(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance) const;
virtual PFN_vkVoidFunction	getInstanceProcAddr						(VkInstance instance, const char* pName) const;
virtual VkResult			enumerateInstanceVersion				(uint32_t* pApiVersion) const;
virtual VkResult			enumerateInstanceLayerProperties		(uint32_t* pPropertyCount, VkLayerProperties* pProperties) const;
virtual VkResult			enumerateInstanceExtensionProperties	(const char* pLayerName, uint32_t* pPropertyCount, VkExtensionProperties* pProperties) const;
virtual void				getExternalComputeQueueDataNV			(VkExternalComputeQueueNV externalQueue, VkExternalComputeQueueDataParamsNV* params, void* pData) const;
