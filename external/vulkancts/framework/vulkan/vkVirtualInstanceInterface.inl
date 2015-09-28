/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 */
virtual void				destroyInstance							(VkInstance instance) const = 0;
virtual VkResult			enumeratePhysicalDevices				(VkInstance instance, deUint32* pPhysicalDeviceCount, VkPhysicalDevice* pPhysicalDevices) const = 0;
virtual VkResult			getPhysicalDeviceFeatures				(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures* pFeatures) const = 0;
virtual VkResult			getPhysicalDeviceFormatProperties		(VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties* pFormatProperties) const = 0;
virtual VkResult			getPhysicalDeviceImageFormatProperties	(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties* pImageFormatProperties) const = 0;
virtual VkResult			getPhysicalDeviceProperties				(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties* pProperties) const = 0;
virtual VkResult			getPhysicalDeviceQueueFamilyProperties	(VkPhysicalDevice physicalDevice, deUint32* pCount, VkQueueFamilyProperties* pQueueFamilyProperties) const = 0;
virtual VkResult			getPhysicalDeviceMemoryProperties		(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties* pMemoryProperties) const = 0;
virtual PFN_vkVoidFunction	getDeviceProcAddr						(VkDevice device, const char* pName) const = 0;
virtual VkResult			createDevice							(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, VkDevice* pDevice) const = 0;
virtual VkResult			enumerateDeviceExtensionProperties		(VkPhysicalDevice physicalDevice, const char* pLayerName, deUint32* pCount, VkExtensionProperties* pProperties) const = 0;
virtual VkResult			enumerateDeviceLayerProperties			(VkPhysicalDevice physicalDevice, deUint32* pCount, VkLayerProperties* pProperties) const = 0;
