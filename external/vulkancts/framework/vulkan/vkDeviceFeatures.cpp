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
								 const std::vector<std::string>&	deviceExtensions,
								 const deBool						enableAllFeatures)
{
	VkPhysicalDeviceRobustness2FeaturesEXT*					robustness2Features					= nullptr;
	VkPhysicalDeviceImageRobustnessFeaturesEXT*				imageRobustnessFeatures				= nullptr;
	VkPhysicalDeviceFragmentShadingRateFeaturesKHR*			fragmentShadingRateFeatures			= nullptr;
	VkPhysicalDeviceShadingRateImageFeaturesNV*				shadingRateImageFeatures			= nullptr;
	VkPhysicalDeviceFragmentDensityMapFeaturesEXT*			fragmentDensityMapFeatures			= nullptr;
	VkPhysicalDevicePageableDeviceLocalMemoryFeaturesEXT*	pageableDeviceLocalMemoryFeatures	= nullptr;

	m_coreFeatures2		= initVulkanStructure();
	m_vulkan11Features	= initVulkanStructure();
	m_vulkan12Features	= initVulkanStructure();
	m_vulkan13Features	= initVulkanStructure();

	if (isInstanceExtensionSupported(apiVersion, instanceExtensions, "VK_KHR_get_physical_device_properties2"))
	{
		const std::vector<VkExtensionProperties>	deviceExtensionProperties	= enumerateDeviceExtensionProperties(vki, physicalDevice, DE_NULL);
		void**										nextPtr						= &m_coreFeatures2.pNext;
		std::vector<FeatureStructWrapperBase*>		featuresToFillFromBlob;
		bool										vk13Supported				= (apiVersion >= VK_API_VERSION_1_3);
		bool										vk12Supported				= (apiVersion >= VK_API_VERSION_1_2);

		// since vk12 we have blob structures combining features of couple previously
		// available feature structures, that now in vk12+ must be removed from chain
		if (vk12Supported)
		{
			addToChainVulkanStructure(&nextPtr, m_vulkan11Features);
			addToChainVulkanStructure(&nextPtr, m_vulkan12Features);

			if (vk13Supported)
				addToChainVulkanStructure(&nextPtr, m_vulkan13Features);
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

				// if feature struct is part of VkPhysicalDeviceVulkan1{1,2,3}Features
				// we dont add it to the chain but store and fill later from blob data
				bool featureFilledFromBlob = false;
				if (vk12Supported)
				{
					deUint32 blobApiVersion = getBlobFeaturesVersion(p->getFeatureDesc().sType);
					if (blobApiVersion)
						featureFilledFromBlob = (apiVersion >= blobApiVersion);
				}

				if (featureFilledFromBlob)
					featuresToFillFromBlob.push_back(p);
				else
				{
					VkStructureType	structType		= p->getFeatureDesc().sType;
					void*			rawStructPtr	= p->getFeatureTypeRaw();

					if (structType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT)
						robustness2Features = reinterpret_cast<VkPhysicalDeviceRobustness2FeaturesEXT*>(rawStructPtr);
					else if (structType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_ROBUSTNESS_FEATURES_EXT)
						imageRobustnessFeatures = reinterpret_cast<VkPhysicalDeviceImageRobustnessFeaturesEXT*>(rawStructPtr);
					else if (structType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR)
						fragmentShadingRateFeatures = reinterpret_cast<VkPhysicalDeviceFragmentShadingRateFeaturesKHR*>(rawStructPtr);
					else if (structType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADING_RATE_IMAGE_FEATURES_NV)
						shadingRateImageFeatures = reinterpret_cast<VkPhysicalDeviceShadingRateImageFeaturesNV*>(rawStructPtr);
					else if (structType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT)
						fragmentDensityMapFeatures = reinterpret_cast<VkPhysicalDeviceFragmentDensityMapFeaturesEXT*>(rawStructPtr);
					else if (structType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PAGEABLE_DEVICE_LOCAL_MEMORY_FEATURES_EXT)
						pageableDeviceLocalMemoryFeatures = reinterpret_cast<VkPhysicalDevicePageableDeviceLocalMemoryFeaturesEXT*>(rawStructPtr);

					// add to chain
					*nextPtr	= rawStructPtr;
					nextPtr		= p->getFeatureTypeNext();
				}
				m_features.push_back(p);
			}
		}

		vki.getPhysicalDeviceFeatures2(physicalDevice, &m_coreFeatures2);

		// fill data from VkPhysicalDeviceVulkan1{1,2,3}Features
		if (vk12Supported)
		{
			AllFeaturesBlobs allBlobs =
			{
				m_vulkan11Features,
				m_vulkan12Features,
				m_vulkan13Features,
				// add blobs from future vulkan versions here
			};

			for (auto feature : featuresToFillFromBlob)
				feature->initializeFeatureFromBlob(allBlobs);
		}
	}
	else
		m_coreFeatures2.features = getPhysicalDeviceFeatures(vki, physicalDevice);

	// 'enableAllFeatures' is used to create a complete list of supported features.
	if (!enableAllFeatures)
	{
		// Disable robustness by default, as it has an impact on performance on some HW.
		if (robustness2Features)
		{
			robustness2Features->robustBufferAccess2	= false;
			robustness2Features->robustImageAccess2		= false;
			robustness2Features->nullDescriptor			= false;
		}
		if (imageRobustnessFeatures)
		{
			imageRobustnessFeatures->robustImageAccess	= false;
		}
		m_coreFeatures2.features.robustBufferAccess = false;

		// Disable VK_EXT_fragment_density_map and VK_NV_shading_rate_image features
		// that must: not be enabled if KHR fragment shading rate features are enabled.
		if (fragmentShadingRateFeatures &&
			(fragmentShadingRateFeatures->pipelineFragmentShadingRate ||
				fragmentShadingRateFeatures->primitiveFragmentShadingRate ||
				fragmentShadingRateFeatures->attachmentFragmentShadingRate))
		{
			if (shadingRateImageFeatures)
				shadingRateImageFeatures->shadingRateImage = false;
			if (fragmentDensityMapFeatures)
				fragmentDensityMapFeatures->fragmentDensityMap = false;
		}
	}

	// Disable pageableDeviceLocalMemory by default since it may modify the behavior
	// of device-local, and even host-local, memory allocations for all tests.
	// pageableDeviceLocalMemory will use targetted testing on a custom device.
	if (pageableDeviceLocalMemoryFeatures)
		pageableDeviceLocalMemoryFeatures->pageableDeviceLocalMemory = false;
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

