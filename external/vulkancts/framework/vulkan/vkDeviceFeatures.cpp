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
#include "vkDeviceFeatures.inl"
#include "vkDeviceFeatures.hpp"

namespace vk
{

static bool isPhysicalDeviceFeatures2Supported (const deUint32 version, const std::vector<std::string>& instanceExtensions)
{
	return isInstanceExtensionSupported(version, instanceExtensions, "VK_KHR_get_physical_device_properties2");
}

DeviceFeatures::DeviceFeatures	(const InstanceInterface&			vki,
								 const deUint32						apiVersion,
								 const VkPhysicalDevice				physicalDevice,
								 const std::vector<std::string>&	instanceExtensions,
								 const std::vector<std::string>&	deviceExtensions)
{
	deMemset(&m_coreFeatures2, 0, sizeof(m_coreFeatures2));
	m_coreFeatures2.sType	= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

	if (isPhysicalDeviceFeatures2Supported(apiVersion, instanceExtensions))
	{
		const std::vector<VkExtensionProperties>	deviceExtensionProperties	= enumerateDeviceExtensionProperties(vki, physicalDevice, DE_NULL);
		void**										nextPtr						= &m_coreFeatures2.pNext;

		for (size_t i = 0; i < DE_LENGTH_OF_ARRAY(featureStructCreatorMap); ++i)
		{
			const char* featureName = featureStructCreatorMap[i].name;

			if (de::contains(deviceExtensions.begin(), deviceExtensions.end(), featureName)
				&& verifyFeatureAddCriteria(featureStructCreatorMap[i], deviceExtensionProperties))
			{
				FeatureStruct* p = createFeatureStructWrapper(featureName);

				if (p)
				{
					*nextPtr = p->getFeatureTypeRaw();
					nextPtr = p->getFeatureTypeNext();
					m_features.push_back(p);
				}
			}
		}

		vki.getPhysicalDeviceFeatures2(physicalDevice, &m_coreFeatures2);
	}
	else
		m_coreFeatures2.features = getPhysicalDeviceFeatures(vki, physicalDevice);

	// Disable robustness by default, as it has an impact on performance on some HW.
	m_coreFeatures2.features.robustBufferAccess = false;
}

bool DeviceFeatures::verifyFeatureAddCriteria (const FeatureStructMapItem& item, const std::vector<VkExtensionProperties>& properties)
{
	bool criteriaOK = true;

	if (deStringEqual(item.name, VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME))
	{
		const size_t propSize = properties.size();

		for (size_t propIdx = 0; propIdx < propSize; ++propIdx)
		{
			if (deStringEqual(properties[propIdx].extensionName, item.name))
			{
				criteriaOK = properties[propIdx].specVersion == item.specVersion;
				break;
			}
		}
	}

	return criteriaOK;
}

bool DeviceFeatures::contains (const std::string& feature, bool throwIfNotExists) const
{
	const size_t typesSize	= m_features.size();

	for (size_t typeIdx = 0; typeIdx < typesSize; ++typeIdx)
	{
		if (deStringEqual(m_features[typeIdx]->getFeatureDesc().name, feature.c_str()))
		{
			return true;
		}
	}

	if (throwIfNotExists)
	{
		std::string msg("Feature " + feature + " is not supported");

		TCU_THROW(NotSupportedError, msg);
	}

	return false;
}

DeviceFeatures::~DeviceFeatures (void)
{
	for (size_t i = 0; i < m_features.size(); ++i)
		delete m_features[i];

	m_features.clear();
}

FeatureStruct* DeviceFeatures::createFeatureStructWrapper (const std::string& s)
{
	for (size_t i = 0; i < DE_LENGTH_OF_ARRAY(featureStructCreatorMap); ++i)
	{
		if (deStringEqual(featureStructCreatorMap[i].name, s.c_str()))
			return (*featureStructCreatorMap[i].creator)();
	}

	return DE_NULL;
}

} // vk

