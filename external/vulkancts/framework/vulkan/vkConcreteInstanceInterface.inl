/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 */
virtual void				destroyInstance							(VkInstance instance, const VkAllocationCallbacks* pAllocator) const;
virtual VkResult			enumeratePhysicalDevices				(VkInstance instance, deUint32* pPhysicalDeviceCount, VkPhysicalDevice* pPhysicalDevices) const;
virtual void				getPhysicalDeviceFeatures				(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures* pFeatures) const;
virtual void				getPhysicalDeviceFormatProperties		(VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties* pFormatProperties) const;
virtual VkResult			getPhysicalDeviceImageFormatProperties	(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties* pImageFormatProperties) const;
virtual void				getPhysicalDeviceProperties				(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties* pProperties) const;
virtual void				getPhysicalDeviceQueueFamilyProperties	(VkPhysicalDevice physicalDevice, deUint32* pQueueFamilyPropertyCount, VkQueueFamilyProperties* pQueueFamilyProperties) const;
virtual void				getPhysicalDeviceMemoryProperties		(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties* pMemoryProperties) const;
virtual PFN_vkVoidFunction	getDeviceProcAddr						(VkDevice device, const char* pName) const;
virtual VkResult			createDevice							(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice) const;
virtual VkResult			enumerateDeviceExtensionProperties		(VkPhysicalDevice physicalDevice, const char* pLayerName, deUint32* pPropertyCount, VkExtensionProperties* pProperties) const;
virtual VkResult			enumerateDeviceLayerProperties			(VkPhysicalDevice physicalDevice, deUint32* pPropertyCount, VkLayerProperties* pProperties) const;
