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
	m_coreProperties2 = initVulkanStructure();

	if (isInstanceExtensionSupported(apiVersion, instanceExtensions, "VK_KHR_get_physical_device_properties2"))
	{
		const std::vector<VkExtensionProperties>	deviceExtensionProperties	= enumerateDeviceExtensionProperties(vki, physicalDevice, DE_NULL);
		void**										nextPtr						= &m_coreProperties2.pNext;

		for (size_t i = 0; i < DE_LENGTH_OF_ARRAY(propertyStructCreatorMap); ++i)
		{
			const char* propertyName = propertyStructCreatorMap[i].name;

			if (de::contains(deviceExtensions.begin(), deviceExtensions.end(), propertyName))
			{
				PropertyStruct* p = createPropertyStructWrapper(propertyName);

				if (p)
				{
					*nextPtr = p->getPropertyTypeRaw();
					nextPtr = p->getPropertyTypeNext();
					m_properties.push_back(p);
				}
			}
		}

		vki.getPhysicalDeviceProperties2(physicalDevice, &m_coreProperties2);
	}
	else
		m_coreProperties2.properties = getPhysicalDeviceProperties(vki, physicalDevice);
}

bool DeviceProperties::contains (const std::string& property, bool throwIfNotExists) const
{
	const size_t typesSize	= m_properties.size();

	for (size_t typeIdx = 0; typeIdx < typesSize; ++typeIdx)
	{
		if (deStringEqual(m_properties[typeIdx]->getPropertyDesc().name, property.c_str()))
		{
			return true;
		}
	}

	if (throwIfNotExists)
	{
		std::string msg("Property " + property + " is not supported");

		TCU_THROW(NotSupportedError, msg);
	}

	return false;
}

bool DeviceProperties::isDevicePropertyInitialized (VkStructureType sType) const
{
	return findStructureInChain(&m_coreProperties2, sType) != DE_NULL;
}

DeviceProperties::~DeviceProperties (void)
{
	for (size_t i = 0; i < m_properties.size(); ++i)
		delete m_properties[i];

	m_properties.clear();
}

PropertyStruct* DeviceProperties::createPropertyStructWrapper (const std::string& s)
{
	for (size_t i = 0; i < DE_LENGTH_OF_ARRAY(propertyStructCreatorMap); ++i)
	{
		if (deStringEqual(propertyStructCreatorMap[i].name, s.c_str()))
			return (*propertyStructCreatorMap[i].creator)();
	}

	return DE_NULL;
}

} // vk

