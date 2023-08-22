/*-------------------------------------------------------------------------
* Vulkan CTS
* ----------
*
* Copyright (c) 2019 The Khronos Group Inc.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "deSTLUtil.hpp"
#include "deString.h"
#include "vkQueryUtil.hpp"
#include "vkDeviceProperties.inl"
#include "vkDeviceProperties.hpp"

namespace vk
{

DeviceProperties::DeviceProperties	(const InstanceInterface&			vki,
									 const deUint32						apiVersion,
									 const VkPhysicalDevice				physicalDevice,
									 const std::vector<std::string>&	instanceExtensions,
									 const std::vector<std::string>&	deviceExtensions)
{
	m_coreProperties2		= initVulkanStructure();
	m_vulkan11Properties	= initVulkanStructure();
	m_vulkan12Properties	= initVulkanStructure();
#ifndef CTS_USES_VULKANSC
	m_vulkan13Properties	= initVulkanStructure();
#endif // CTS_USES_VULKANSC
#ifdef CTS_USES_VULKANSC
	m_vulkanSC10Properties	= initVulkanStructure();
#endif // CTS_USES_VULKANSC

	if (isInstanceExtensionSupported(apiVersion, instanceExtensions, "VK_KHR_get_physical_device_properties2"))
	{
		const std::vector<VkExtensionProperties>	deviceExtensionProperties	= enumerateDeviceExtensionProperties(vki, physicalDevice, DE_NULL);
		void**										nextPtr						= &m_coreProperties2.pNext;
		std::vector<PropertyStructWrapperBase*>		propertiesToFillFromBlob;
		std::vector<PropertyStructWrapperBase*>		propertiesAddedWithVK;
		bool										vk11Supported				= (apiVersion >= VK_MAKE_API_VERSION(0, 1, 1, 0));
		bool										vk12Supported				= (apiVersion >= VK_MAKE_API_VERSION(0, 1, 2, 0));
#ifndef CTS_USES_VULKANSC
		bool										vk13Supported				= (apiVersion >= VK_MAKE_API_VERSION(0, 1, 3, 0));
#endif // CTS_USES_VULKANSC
#ifdef CTS_USES_VULKANSC
		bool										vksc10Supported				= (apiVersion >= VK_MAKE_API_VERSION(1, 1, 0, 0));
#endif // CTS_USES_VULKANSC

		// there are 3 properies structures that were added with vk11 (without being first part of extension)
		if (vk11Supported)
		{
			propertiesAddedWithVK =
			{
				createPropertyStructWrapper<VkPhysicalDeviceSubgroupProperties>(),
				createPropertyStructWrapper<VkPhysicalDeviceIDProperties>(),
				createPropertyStructWrapper<VkPhysicalDeviceProtectedMemoryProperties>()
			};

			for (auto pAddedWithVK : propertiesAddedWithVK)
			{
				m_properties.push_back(pAddedWithVK);

				if (!vk12Supported)
					addToChainStructWrapper(&nextPtr, pAddedWithVK);
			}
		}

		// since vk12 we have blob structures combining properties of couple previously
		// available property structures, that now in vk12 and above must be removed from chain
		if (vk12Supported)
		{
			addToChainVulkanStructure(&nextPtr, m_vulkan11Properties);
			addToChainVulkanStructure(&nextPtr, m_vulkan12Properties);

#ifndef CTS_USES_VULKANSC
			if (vk13Supported)
				addToChainVulkanStructure(&nextPtr, m_vulkan13Properties);
#endif // CTS_USES_VULKANSC
		}

		std::vector<std::string> allDeviceExtensions = deviceExtensions;
#ifdef CTS_USES_VULKANSC
		// VulkanSC: add missing core extensions to the list
		std::vector<const char*> coreExtensions;
		getCoreDeviceExtensions(apiVersion, coreExtensions);
		for (const auto& coreExt : coreExtensions)
			if (!de::contains(allDeviceExtensions.begin(), allDeviceExtensions.end(), std::string(coreExt)))
				allDeviceExtensions.push_back(coreExt);
		if (vksc10Supported)
			addToChainVulkanStructure(&nextPtr, m_vulkanSC10Properties);
#endif // CTS_USES_VULKANSC

		// iterate over data for all property that are defined in specification
		for (const auto& propertyStructCreationData : propertyStructCreationArray)
		{
			const char* propertyName = propertyStructCreationData.name;

			// check if this property is available on current device.
			if (de::contains(allDeviceExtensions.begin(), allDeviceExtensions.end(), propertyName) ||
				std::string(propertyName) == "core_property")
			{
				PropertyStructWrapperBase* p = (*propertyStructCreationData.creatorFunction)();
				if (p == DE_NULL)
					continue;

#ifdef CTS_USES_VULKANSC
				// m_vulkanSC10Properties was already added above
				if (p->getPropertyDesc().sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_SC_1_0_PROPERTIES)
					continue;
#endif // CTS_USES_VULKANSC

				// if property struct is part of VkPhysicalDeviceVulkan1{1,2}Properties
				// we dont add it to the chain but store and fill later from blob data
				bool propertyFilledFromBlob = false;
				if (vk12Supported)
				{
					deUint32 blobApiVersion = getBlobPropertiesVersion(p->getPropertyDesc().sType);
					if (blobApiVersion)
						propertyFilledFromBlob = (apiVersion >= blobApiVersion);
				}

				if (propertyFilledFromBlob)
					propertiesToFillFromBlob.push_back(p);
				else
				{
					// add to chain
					addToChainStructWrapper(&nextPtr, p);
				}
				m_properties.push_back(p);
			}
		}

		vki.getPhysicalDeviceProperties2(physicalDevice, &m_coreProperties2);

		// fill data from VkPhysicalDeviceVulkan1{1,2,3}Properties
		if (vk12Supported)
		{
			AllPropertiesBlobs allBlobs =
			{
				m_vulkan11Properties,
				m_vulkan12Properties,
#ifndef CTS_USES_VULKANSC
				m_vulkan13Properties,
#endif // CTS_USES_VULKANSC
				// add blobs from future vulkan versions here
			};

			// three properties that were added with vk11 in vk12 were merged to VkPhysicalDeviceVulkan11Properties
			propertiesToFillFromBlob.insert(propertiesToFillFromBlob.end(), propertiesAddedWithVK.begin(), propertiesAddedWithVK.end());

			for (auto property : propertiesToFillFromBlob)
				property->initializePropertyFromBlob(allBlobs);
		}
	}
	else
		m_coreProperties2.properties = getPhysicalDeviceProperties(vki, physicalDevice);
}

void DeviceProperties::addToChainStructWrapper(void*** chainPNextPtr, PropertyStructWrapperBase* structWrapper)
{
	DE_ASSERT(chainPNextPtr != DE_NULL);

	(**chainPNextPtr) = structWrapper->getPropertyTypeRaw();
	(*chainPNextPtr) = structWrapper->getPropertyTypeNext();
}

bool DeviceProperties::contains (const std::string& property, bool throwIfNotExists) const
{
	for (const auto f : m_properties)
	{
		if (deStringEqual(f->getPropertyDesc().name, property.c_str()))
			return true;
	}

	if (throwIfNotExists)
		TCU_THROW(NotSupportedError, "Property " + property + " is not supported");

	return false;
}

bool DeviceProperties::isDevicePropertyInitialized (VkStructureType sType) const
{
	for (const auto f : m_properties)
	{
		if (f->getPropertyDesc().sType == sType)
			return true;
	}
	return false;
}

DeviceProperties::~DeviceProperties (void)
{
	for (auto p : m_properties)
		delete p;

	m_properties.clear();
}

} // vk

