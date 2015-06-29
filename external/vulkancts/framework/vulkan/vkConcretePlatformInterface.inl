/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 */
virtual VkResult	createInstance				(const VkInstanceCreateInfo* pCreateInfo, VkInstance* pInstance) const;
virtual VkResult	destroyInstance				(VkInstance instance) const;
virtual VkResult	enumeratePhysicalDevices	(VkInstance instance, deUint32* pPhysicalDeviceCount, VkPhysicalDevice* pPhysicalDevices) const;
virtual FunctionPtr	getProcAddr					(VkPhysicalDevice physicalDevice, const char* pName) const;
