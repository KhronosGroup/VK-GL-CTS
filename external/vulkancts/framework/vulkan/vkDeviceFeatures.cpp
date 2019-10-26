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

DeviceFeatures::DeviceFeatures	(const InstanceInterface&			vki,
								 const deUint32						apiVersion,
								 const VkPhysicalDevice				physicalDevice,
								 const std::vector<std::string>&	instanceExtensions,
								 const std::vector<std::string>&	deviceExtensions)
{
	m_coreFeatures2		= initVulkanStructure();
	m_vulkan11Features	= initVulkanStructure();
	m_vulkan12Features	= initVulkanStructure();

	if (isInstanceExtensionSupported(apiVersion, instanceExtensions, "VK_KHR_get_physical_device_properties2"))
	{
		const std::vector<VkExtensionProperties>	deviceExtensionProperties	= enumerateDeviceExtensionProperties(vki, physicalDevice, DE_NULL);
		void**										nextPtr						= &m_coreFeatures2.pNext;
		std::vector<FeatureStructWrapperBase*>		featuresToFillFromBlob;
		bool										vk12Supported				= (apiVersion >= VK_MAKE_VERSION(1, 2, 0));

		// in vk12 we have blob structures combining features of couple previously
		// available feature structures, that now in vk12 must be removed from chain
		if (vk12Supported)
		{
			addToChainVulkanStructure(&nextPtr, m_vulkan11Features);
			addToChainVulkanStructure(&nextPtr, m_vulkan12Features);
		}

		// iterate over data for all feature that are defined in specification
		for (const auto& featureStructCreationData : featureStructCreationArray)
		{
			const char* featureName = featureStructCreationData.name;

			// check if this feature is available on current device
			if (de::contains(deviceExtensions.begin(), deviceExtensions.end(), featureName) &&
				verifyFeatureAddCriteria(featureStructCreationData, deviceExtensionProperties))
			{
				FeatureStructWrapperBase* p = (*featureStructCreationData.creatorFunction)();
				if (p == DE_NULL)
					continue;

				// if feature struct is part of VkPhysicalDeviceVulkan1{1,2}Features
				// we dont add it to the chain but store and fill later from blob data
				bool featureFilledFromBlob = false;
				if (vk12Supported)
					featureFilledFromBlob = isPartOfBlobFeatures(p->getFeatureDesc().sType);

				if (featureFilledFromBlob)
					featuresToFillFromBlob.push_back(p);
				else
				{
					// add to chain
					*nextPtr = p->getFeatureTypeRaw();
					nextPtr = p->getFeatureTypeNext();
				}
				m_features.push_back(p);
			}
		}

		vki.getPhysicalDeviceFeatures2(physicalDevice, &m_coreFeatures2);

		// fill data from VkPhysicalDeviceVulkan1{1,2}Features
		if (vk12Supported)
		{
			AllBlobs allBlobs =
			{
				m_vulkan11Features,
				m_vulkan12Features,
				// add blobs from future vulkan versions here
			};

			for (auto feature : featuresToFillFromBlob)
				feature->initializeFromBlob(allBlobs);
		}
	}
	else
		m_coreFeatures2.features = getPhysicalDeviceFeatures(vki, physicalDevice);

	// Disable robustness by default, as it has an impact on performance on some HW.
	m_coreFeatures2.features.robustBufferAccess = false;
}

bool DeviceFeatures::verifyFeatureAddCriteria (const FeatureStructCreationData& item, const std::vector<VkExtensionProperties>& properties)
{
	if (deStringEqual(item.name, VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME))
	{
		for (const auto& property : properties)
		{
			if (deStringEqual(property.extensionName, item.name))
				return (property.specVersion == item.specVersion);
		}
	}

	return true;
}

bool DeviceFeatures::contains (const std::string& feature, bool throwIfNotExists) const
{
	for (const auto f : m_features)
	{
		if (deStringEqual(f->getFeatureDesc().name, feature.c_str()))
			return true;
	}

	if (throwIfNotExists)
		TCU_THROW(NotSupportedError, "Feature " + feature + " is not supported");

	return false;
}

bool DeviceFeatures::isDeviceFeatureInitialized (VkStructureType sType) const
{
	for (const auto f : m_features)
	{
		if (f->getFeatureDesc().sType == sType)
			return true;
	}
	return false;
}

DeviceFeatures::~DeviceFeatures (void)
{
	for (auto p : m_features)
		delete p;

	m_features.clear();
}

} // vk

