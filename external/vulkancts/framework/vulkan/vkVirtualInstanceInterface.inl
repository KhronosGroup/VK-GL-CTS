/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 */
virtual void				destroyInstance									(VkInstance instance, const VkAllocationCallbacks* pAllocator) const = 0;
virtual VkResult			enumeratePhysicalDevices						(VkInstance instance, deUint32* pPhysicalDeviceCount, VkPhysicalDevice* pPhysicalDevices) const = 0;
virtual void				getPhysicalDeviceFeatures						(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures* pFeatures) const = 0;
virtual void				getPhysicalDeviceFormatProperties				(VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties* pFormatProperties) const = 0;
virtual VkResult			getPhysicalDeviceImageFormatProperties			(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties* pImageFormatProperties) const = 0;
virtual void				getPhysicalDeviceProperties						(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties* pProperties) const = 0;
virtual void				getPhysicalDeviceQueueFamilyProperties			(VkPhysicalDevice physicalDevice, deUint32* pQueueFamilyPropertyCount, VkQueueFamilyProperties* pQueueFamilyProperties) const = 0;
virtual void				getPhysicalDeviceMemoryProperties				(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties* pMemoryProperties) const = 0;
virtual PFN_vkVoidFunction	getDeviceProcAddr								(VkDevice device, const char* pName) const = 0;
virtual VkResult			createDevice									(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice) const = 0;
virtual VkResult			enumerateDeviceExtensionProperties				(VkPhysicalDevice physicalDevice, const char* pLayerName, deUint32* pPropertyCount, VkExtensionProperties* pProperties) const = 0;
virtual VkResult			enumerateDeviceLayerProperties					(VkPhysicalDevice physicalDevice, deUint32* pPropertyCount, VkLayerProperties* pProperties) const = 0;
virtual void				getPhysicalDeviceSparseImageFormatProperties	(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkSampleCountFlagBits samples, VkImageUsageFlags usage, VkImageTiling tiling, deUint32* pPropertyCount, VkSparseImageFormatProperties* pProperties) const = 0;
