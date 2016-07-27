/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 *
 *//*!
 * \file  vktSparseResourcesBase.cpp
 * \brief Sparse Resources Base Instance
 *//*--------------------------------------------------------------------*/

#include "vktSparseResourcesBase.hpp"
#include "vktSparseResourcesTestsUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"

using namespace vk;

namespace vkt
{
namespace sparse
{

struct QueueFamilyQueuesCount
{
	QueueFamilyQueuesCount() : queueCount(0u), counter(0u) {};

	deUint32		queueCount;
	deUint32		counter;
};

SparseResourcesBaseInstance::SparseResourcesBaseInstance (Context &context) 
	: TestInstance(context)
{
}

void SparseResourcesBaseInstance::createDeviceSupportingQueues(const QueueRequirementsVec& queueRequirements)
{
	const InstanceInterface&	instance		= m_context.getInstanceInterface();
	const DeviceInterface&		deviceInterface = m_context.getDeviceInterface();
	const VkPhysicalDevice		physicalDevice	= m_context.getPhysicalDevice();

	deUint32 queueFamilyPropertiesCount = 0u;
	instance.getPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertiesCount, DE_NULL);

	if(queueFamilyPropertiesCount == 0u)
		TCU_THROW(ResourceError, "Device reports an empty set of queue family properties");

	std::vector<VkQueueFamilyProperties> queueFamilyProperties;
	queueFamilyProperties.resize(queueFamilyPropertiesCount);

	instance.getPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertiesCount, &queueFamilyProperties[0]);

	if (queueFamilyPropertiesCount == 0u)
		TCU_THROW(ResourceError, "Device reports an empty set of queue family properties");

	typedef std::map<deUint32, QueueFamilyQueuesCount>	SelectedQueuesMap;
	typedef std::map<deUint32, std::vector<float> >		QueuePrioritiesMap;

	SelectedQueuesMap	selectedQueueFamilies;
	QueuePrioritiesMap	queuePriorities;

	for (deUint32 queueReqNdx = 0; queueReqNdx < queueRequirements.size(); ++queueReqNdx)
	{
		const QueueRequirements queueRequirement = queueRequirements[queueReqNdx];
		const deUint32			queueFamilyIndex = findMatchingQueueFamilyIndex(queueFamilyProperties, queueRequirement.queueFlags);

		if (queueFamilyIndex == NO_MATCH_FOUND)
			TCU_THROW(NotSupportedError, "No match found for queue requirements");

		selectedQueueFamilies[queueFamilyIndex].queueCount += queueRequirement.queueCount;
		for (deUint32 queueNdx = 0; queueNdx < queueRequirement.queueCount; ++queueNdx)
		{
			queuePriorities[queueFamilyIndex].push_back(1.0f);
		}
	}

	std::vector<VkDeviceQueueCreateInfo> queueInfos;

	for (SelectedQueuesMap::iterator queueFamilyIter = selectedQueueFamilies.begin(); queueFamilyIter != selectedQueueFamilies.end(); ++queueFamilyIter)
	{
		VkDeviceQueueCreateInfo queueInfo;
		deMemset(&queueInfo, 0, sizeof(queueInfo));

		queueInfo.sType				= VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueInfo.pNext				= DE_NULL;
		queueInfo.flags				= (VkDeviceQueueCreateFlags)0u;
		queueInfo.queueFamilyIndex	= queueFamilyIter->first;
		queueInfo.queueCount		= queueFamilyIter->second.queueCount;
		queueInfo.pQueuePriorities  = &queuePriorities[queueFamilyIter->first][0];

		queueInfos.push_back(queueInfo);
	}

	VkDeviceCreateInfo deviceInfo;
	deMemset(&deviceInfo, 0, sizeof(deviceInfo));

	VkPhysicalDeviceFeatures deviceFeatures;
	instance.getPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);

	deviceInfo.sType					= VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceInfo.pNext					= DE_NULL;
	deviceInfo.enabledExtensionCount	= 0u;
	deviceInfo.ppEnabledExtensionNames	= DE_NULL;
	deviceInfo.enabledLayerCount		= 0u;
	deviceInfo.ppEnabledLayerNames		= DE_NULL;
	deviceInfo.pEnabledFeatures			= &deviceFeatures;
	deviceInfo.queueCreateInfoCount		= (deUint32)selectedQueueFamilies.size();
	deviceInfo.pQueueCreateInfos		= &queueInfos[0];

	m_logicalDevice = vk::createDevice(instance, physicalDevice, &deviceInfo);

	for (deUint32 queueReqNdx = 0; queueReqNdx < queueRequirements.size(); ++queueReqNdx)
	{
		const QueueRequirements queueRequirement = queueRequirements[queueReqNdx];
		const deUint32			queueFamilyIndex = findMatchingQueueFamilyIndex(queueFamilyProperties, queueRequirement.queueFlags);

		if (queueFamilyIndex == NO_MATCH_FOUND)
			TCU_THROW(NotSupportedError, "No match found for queue requirements");

		for (deUint32 queueNdx = 0; queueNdx < queueRequirement.queueCount; ++queueNdx)
		{
			VkQueue	queueHandle = 0;
			deviceInterface.getDeviceQueue(*m_logicalDevice, queueFamilyIndex, selectedQueueFamilies[queueFamilyIndex].counter++, &queueHandle);

			Queue queue;
			queue.queueHandle		= queueHandle;
			queue.queueFamilyIndex	= queueFamilyIndex;

			m_queues[queueRequirement.queueFlags].push_back(queue);
		}
	}
}

const Queue& SparseResourcesBaseInstance::getQueue (const VkQueueFlags queueFlags, const deUint32 queueIndex)
{
	return m_queues[queueFlags][queueIndex];
}

deUint32 SparseResourcesBaseInstance::findMatchingMemoryType (const InstanceInterface&		instance,
															  const VkPhysicalDevice		physicalDevice,
															  const VkMemoryRequirements&	objectMemoryRequirements,
															  const MemoryRequirement&		memoryRequirement) const
{
	const VkPhysicalDeviceMemoryProperties deviceMemoryProperties = getPhysicalDeviceMemoryProperties(instance, physicalDevice);

	for (deUint32 memoryTypeNdx = 0; memoryTypeNdx < deviceMemoryProperties.memoryTypeCount; ++memoryTypeNdx)
	{
		if ((objectMemoryRequirements.memoryTypeBits & (1u << memoryTypeNdx)) != 0 &&
			memoryRequirement.matchesHeap(deviceMemoryProperties.memoryTypes[memoryTypeNdx].propertyFlags))
		{
			return memoryTypeNdx;
		}
	}

	return NO_MATCH_FOUND;
}

bool SparseResourcesBaseInstance::checkSparseSupportForImageType (const InstanceInterface&	instance,
																	const VkPhysicalDevice		physicalDevice,
																	const ImageType				imageType) const
{
	const VkPhysicalDeviceFeatures deviceFeatures = getPhysicalDeviceFeatures(instance, physicalDevice);

	if (!deviceFeatures.sparseBinding)
		return false;

	switch (mapImageType(imageType))
	{
		case VK_IMAGE_TYPE_2D:
			return deviceFeatures.sparseResidencyImage2D == VK_TRUE;
		case VK_IMAGE_TYPE_3D:
			return deviceFeatures.sparseResidencyImage3D == VK_TRUE;
		default:
			DE_ASSERT(0);
			return false;
	};
}

bool SparseResourcesBaseInstance::checkSparseSupportForImageFormat (const InstanceInterface&	instance,
																	const VkPhysicalDevice		physicalDevice,
																	const VkImageCreateInfo&	imageInfo) const
{
	const std::vector<VkSparseImageFormatProperties> sparseImageFormatPropVec = getPhysicalDeviceSparseImageFormatProperties(
		instance, physicalDevice, imageInfo.format, imageInfo.imageType, imageInfo.samples, imageInfo.usage, imageInfo.tiling);

	return sparseImageFormatPropVec.size() > 0u;
}

bool SparseResourcesBaseInstance::checkImageFormatFeatureSupport (const vk::InstanceInterface&		instance,
																  const vk::VkPhysicalDevice		physicalDevice,
																  const vk::VkFormat				format,
																  const vk::VkFormatFeatureFlags	featureFlags) const
{
	const VkFormatProperties formatProperties = getPhysicalDeviceFormatProperties(instance, physicalDevice, format);

	return (formatProperties.optimalTilingFeatures & featureFlags) == featureFlags;
}

deUint32 SparseResourcesBaseInstance::getSparseAspectRequirementsIndex (const std::vector<VkSparseImageMemoryRequirements>&	requirements,
																		const VkImageAspectFlags							aspectFlags) const
{
	for (deUint32 memoryReqNdx = 0; memoryReqNdx < requirements.size(); ++memoryReqNdx)
	{
		if (requirements[memoryReqNdx].formatProperties.aspectMask & aspectFlags)
		{
			return memoryReqNdx;
		}
	}

	return NO_MATCH_FOUND;
}

deUint32 SparseResourcesBaseInstance::findMatchingQueueFamilyIndex (const QueueFamilyPropertiesVec& queueFamilyProperties,
																	const VkQueueFlags				queueFlags)	const
{
	for (deUint32 queueNdx = 0; queueNdx < queueFamilyProperties.size(); ++queueNdx)
	{
		if ((queueFamilyProperties[queueNdx].queueFlags & queueFlags) == queueFlags)
		{
			return queueNdx;
		}
	}

	return NO_MATCH_FOUND;
}

} // sparse
} // vkt
