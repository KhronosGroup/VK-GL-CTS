/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 */
virtual VkResult			destroyInstance							(VkInstance instance) const = 0;
virtual VkResult			enumeratePhysicalDevices				(VkInstance instance, deUint32* pPhysicalDeviceCount, VkPhysicalDevice* pPhysicalDevices) const = 0;
virtual VkResult			getPhysicalDeviceFeatures				(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures* pFeatures) const = 0;
virtual VkResult			getPhysicalDeviceFormatProperties		(VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties* pFormatProperties) const = 0;
virtual VkResult			getPhysicalDeviceImageFormatProperties	(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageFormatProperties* pImageFormatProperties) const = 0;
virtual VkResult			getPhysicalDeviceLimits					(VkPhysicalDevice physicalDevice, VkPhysicalDeviceLimits* pLimits) const = 0;
virtual VkResult			getPhysicalDeviceProperties				(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties* pProperties) const = 0;
virtual VkResult			getPhysicalDeviceQueueCount				(VkPhysicalDevice physicalDevice, deUint32* pCount) const = 0;
virtual VkResult			getPhysicalDeviceQueueProperties		(VkPhysicalDevice physicalDevice, deUint32 count, VkPhysicalDeviceQueueProperties* pQueueProperties) const = 0;
virtual VkResult			getPhysicalDeviceMemoryProperties		(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties* pMemoryProperties) const = 0;
virtual PFN_vkVoidFunction	getDeviceProcAddr						(VkDevice device, const char* pName) const = 0;
virtual VkResult			createDevice							(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, VkDevice* pDevice) const = 0;
