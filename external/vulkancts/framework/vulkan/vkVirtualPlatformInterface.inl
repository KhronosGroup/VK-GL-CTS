/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 */
virtual VkResult			createInstance							(const VkInstanceCreateInfo* pCreateInfo, VkInstance* pInstance) const = 0;
virtual PFN_vkVoidFunction	getInstanceProcAddr						(VkInstance instance, const char* pName) const = 0;
virtual VkResult			enumerateInstanceExtensionProperties	(const char* pLayerName, deUint32* pCount, VkExtensionProperties* pProperties) const = 0;
virtual VkResult			enumerateInstanceLayerProperties		(deUint32* pCount, VkLayerProperties* pProperties) const = 0;
