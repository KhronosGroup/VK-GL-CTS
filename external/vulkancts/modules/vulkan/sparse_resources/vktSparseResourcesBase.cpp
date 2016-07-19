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
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"

using namespace vk;

namespace vkt
{
namespace sparse
{

struct QueueFamilyQueuesCount
{
	QueueFamilyQueuesCount() : queueCount(0u) {};

	deUint32 queueCount;
};

SparseResourcesBaseInstance::SparseResourcesBaseInstance (Context &context) 
	: TestInstance(context) 
{
}

bool SparseResourcesBaseInstance::createDeviceSupportingQueues (const QueueRequirementsVec&	queueRequirements)
{
	const InstanceInterface&	instance		= m_context.getInstanceInterface();
	const DeviceInterface&		deviceInterface = m_context.getDeviceInterface();
	const VkPhysicalDevice		physicalDevice	= m_context.getPhysicalDevice();

	deUint32 queueFamilyPropertiesCount = 0u;
	instance.getPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertiesCount, DE_NULL);

	if (queueFamilyPropertiesCount == 0u)
	{
		return false;
	}

	std::vector<VkQueueFamilyProperties> queueFamilyProperties;
	queueFamilyProperties.resize(queueFamilyPropertiesCount);

	instance.getPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertiesCount, &queueFamilyProperties[0]);

	typedef std::map<deUint32, QueueFamilyQueuesCount>	SelectedQueuesMap;
	typedef std::map<deUint32, std::vector<float> >		QueuePrioritiesMap;

	SelectedQueuesMap	selectedQueueFamilies;
	QueuePrioritiesMap	queuePriorities;

	for (deUint32 queueReqNdx = 0; queueReqNdx < queueRequirements.size(); ++queueReqNdx)
	{
		const QueueRequirements queueRequirement = queueRequirements[queueReqNdx];

		deUint32 queueFamilyIndex	= 0u;
		deUint32 queuesFoundCount	= 0u;
		do
		{
			queueFamilyIndex = findMatchingQueueFamilyIndex(queueFamilyProperties, queueRequirement.queueFlags, queueFamilyIndex);

			if (queueFamilyIndex == NO_MATCH_FOUND)
				return false;

			const deUint32 queuesPerFamilyCount = deMin32(queueFamilyProperties[queueFamilyIndex].queueCount, queueRequirement.queueCount - queuesFoundCount);

			selectedQueueFamilies[queueFamilyIndex].queueCount = deMax32(queuesPerFamilyCount, selectedQueueFamilies[queueFamilyIndex].queueCount);

			for (deUint32 queueNdx = 0; queueNdx < queuesPerFamilyCount; ++queueNdx)
			{
				Queue queue;
				queue.queueFamilyIndex	= queueFamilyIndex;
				queue.queueIndex		= queueNdx;

				m_queues[queueRequirement.queueFlags].push_back(queue);
			}

			queuesFoundCount += queuesPerFamilyCount;

			++queueFamilyIndex;
		} while (queuesFoundCount < queueRequirement.queueCount);
	}

	std::vector<VkDeviceQueueCreateInfo> queueInfos;

	for (SelectedQueuesMap::iterator queueFamilyIter = selectedQueueFamilies.begin(); queueFamilyIter != selectedQueueFamilies.end(); ++queueFamilyIter)
	{
		VkDeviceQueueCreateInfo queueInfo;
		deMemset(&queueInfo, 0, sizeof(queueInfo));

		for (deUint32 queueNdx = 0; queueNdx < queueFamilyIter->second.queueCount; ++queueNdx)
		{
			queuePriorities[queueFamilyIter->first].push_back(1.0f);
		}

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

	for (QueuesMap::iterator queuesIter = m_queues.begin(); queuesIter != m_queues.end(); ++queuesIter)
	{
		for (deUint32 queueNdx = 0u; queueNdx < queuesIter->second.size(); ++queueNdx)
		{
			Queue& queue = queuesIter->second[queueNdx];

			VkQueue	queueHandle = 0;
			deviceInterface.getDeviceQueue(*m_logicalDevice, queue.queueFamilyIndex, queue.queueIndex, &queueHandle);

			queue.queueHandle = queueHandle;
		}
	}

	return true;
}

const Queue& SparseResourcesBaseInstance::getQueue (const VkQueueFlags queueFlags, const deUint32 queueIndex)
{
	return m_queues[queueFlags][queueIndex];
}

deUint32 SparseResourcesBaseInstance::findMatchingMemoryType (const VkPhysicalDeviceMemoryProperties&	deviceMemoryProperties,
															  const VkMemoryRequirements&				objectMemoryRequirements,
															  const MemoryRequirement&					memoryRequirement) const
{
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

deUint32 SparseResourcesBaseInstance::findMatchingQueueFamilyIndex (const QueueFamilyPropertiesVec& queueFamilyProperties,
																	const VkQueueFlags				queueFlags,
																	const deUint32					startIndex)	const
{
	for (deUint32 queueNdx = startIndex; queueNdx < queueFamilyProperties.size(); ++queueNdx)
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
