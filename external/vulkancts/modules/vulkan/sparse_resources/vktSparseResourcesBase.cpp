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
#include "vktCustomInstancesDevices.hpp"
#include "vkMemUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkDeviceUtil.hpp"

using namespace vk;

namespace vkt
{
namespace sparse
{
namespace
{

struct QueueFamilyQueuesCount
{
	QueueFamilyQueuesCount() : queueCount(0u) {}

	deUint32 queueCount;
};

deUint32 findMatchingQueueFamilyIndex	(const std::vector<VkQueueFamilyProperties>&	queueFamilyProperties,
										 const VkQueueFlags								queueFlags,
										 const deUint32									startIndex)
{
	for (deUint32 queueNdx = startIndex; queueNdx < queueFamilyProperties.size(); ++queueNdx)
	{
		if ((queueFamilyProperties[queueNdx].queueFlags & queueFlags) == queueFlags)
			return queueNdx;
	}

	return NO_MATCH_FOUND;
}

} // anonymous

void SparseResourcesBaseInstance::createDeviceSupportingQueues(const QueueRequirementsVec& queueRequirements, bool requireShaderImageAtomicInt64Features, bool requireMaintenance5)
{
	typedef std::map<VkQueueFlags, std::vector<Queue> >	QueuesMap;
	typedef std::map<deUint32, QueueFamilyQueuesCount>	SelectedQueuesMap;
	typedef std::map<deUint32, std::vector<float> >		QueuePrioritiesMap;

	std::vector<VkPhysicalDeviceGroupProperties>		devGroupProperties;
	std::vector<const char*>							deviceExtensions;
	VkDeviceGroupDeviceCreateInfo						deviceGroupInfo =
	{
		VK_STRUCTURE_TYPE_DEVICE_GROUP_DEVICE_CREATE_INFO,			//stype
		DE_NULL,													//pNext
		0,															//physicalDeviceCount
		DE_NULL														//physicalDevices
	};
	m_physicalDevices.push_back(m_context.getPhysicalDevice());

	// If requested, create an intance with device groups
	if (m_useDeviceGroups)
	{
		const std::vector<std::string>	requiredExtensions { "VK_KHR_device_group_creation", "VK_KHR_get_physical_device_properties2" };
		m_deviceGroupInstance	=		createCustomInstanceWithExtensions(m_context, requiredExtensions);
		devGroupProperties		=		enumeratePhysicalDeviceGroups(m_context.getInstanceInterface(), m_deviceGroupInstance);
		m_numPhysicalDevices	=		devGroupProperties[m_deviceGroupIdx].physicalDeviceCount;

		m_physicalDevices.clear();
		for (size_t physDeviceID = 0; physDeviceID < m_numPhysicalDevices; physDeviceID++)
		{
			m_physicalDevices.push_back(devGroupProperties[m_deviceGroupIdx].physicalDevices[physDeviceID]);
		}
		if (m_numPhysicalDevices < 2)
			TCU_THROW(NotSupportedError, "Sparse binding device group tests not supported with 1 physical device");

		deviceGroupInfo.physicalDeviceCount = devGroupProperties[m_deviceGroupIdx].physicalDeviceCount;
		deviceGroupInfo.pPhysicalDevices = devGroupProperties[m_deviceGroupIdx].physicalDevices;

		if (!isCoreDeviceExtension(m_context.getUsedApiVersion(), "VK_KHR_device_group"))
			deviceExtensions.push_back("VK_KHR_device_group");
	}
	else
	{
		m_context.requireInstanceFunctionality("VK_KHR_get_physical_device_properties2");
	}

	const VkInstance&					instance(m_useDeviceGroups ? m_deviceGroupInstance : m_context.getInstance());
	InstanceDriver						instanceDriver(m_context.getPlatformInterface(), instance);
	const VkPhysicalDevice				physicalDevice = getPhysicalDevice();
	deUint32 queueFamilyPropertiesCount = 0u;
	instanceDriver.getPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertiesCount, DE_NULL);

	if(queueFamilyPropertiesCount == 0u)
		TCU_THROW(ResourceError, "Device reports an empty set of queue family properties");

	std::vector<VkQueueFamilyProperties> queueFamilyProperties;
	queueFamilyProperties.resize(queueFamilyPropertiesCount);

	instanceDriver.getPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertiesCount, &queueFamilyProperties[0]);

	if (queueFamilyPropertiesCount == 0u)
		TCU_THROW(ResourceError, "Device reports an empty set of queue family properties");

	SelectedQueuesMap	selectedQueueFamilies;
	QueuePrioritiesMap	queuePriorities;

	for (deUint32 queueReqNdx = 0; queueReqNdx < queueRequirements.size(); ++queueReqNdx)
	{
		const QueueRequirements& queueRequirement = queueRequirements[queueReqNdx];

		deUint32 queueFamilyIndex	= 0u;
		deUint32 queuesFoundCount	= 0u;

		do
		{
			queueFamilyIndex = findMatchingQueueFamilyIndex(queueFamilyProperties, queueRequirement.queueFlags, queueFamilyIndex);

			if (queueFamilyIndex == NO_MATCH_FOUND)
				TCU_THROW(NotSupportedError, "No match found for queue requirements");

			const deUint32 queuesPerFamilyCount = deMin32(queueFamilyProperties[queueFamilyIndex].queueCount, queueRequirement.queueCount - queuesFoundCount);

			selectedQueueFamilies[queueFamilyIndex].queueCount = deMax32(queuesPerFamilyCount, selectedQueueFamilies[queueFamilyIndex].queueCount);

			for (deUint32 queueNdx = 0; queueNdx < queuesPerFamilyCount; ++queueNdx)
			{
				Queue queue				= {DE_NULL, 0, 0};
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
		for (deUint32 queueNdx = 0; queueNdx < queueFamilyIter->second.queueCount; ++queueNdx)
			queuePriorities[queueFamilyIter->first].push_back(1.0f);

		const VkDeviceQueueCreateInfo queueInfo =
		{
			VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,		// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			(VkDeviceQueueCreateFlags)0u,					// VkDeviceQueueCreateFlags	flags;
			queueFamilyIter->first,							// uint32_t					queueFamilyIndex;
			queueFamilyIter->second.queueCount,				// uint32_t					queueCount;
			&queuePriorities[queueFamilyIter->first][0],	// const float*				pQueuePriorities;
		};

		queueInfos.push_back(queueInfo);
	}

	vk::VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT	shaderImageAtomicInt64Features	= m_context.getShaderImageAtomicInt64FeaturesEXT();
	vk::VkPhysicalDeviceMaintenance5FeaturesKHR				maintenance5Features			= m_context.getMaintenance5Features();
	shaderImageAtomicInt64Features.pNext	= nullptr;
	maintenance5Features.pNext				= nullptr;

	const VkPhysicalDeviceFeatures	deviceFeatures	= getPhysicalDeviceFeatures(instanceDriver, physicalDevice);
	vk::VkPhysicalDeviceFeatures2	deviceFeatures2	= getPhysicalDeviceFeatures2(instanceDriver, physicalDevice);

	const bool useFeatures2 = (requireShaderImageAtomicInt64Features || requireMaintenance5);

	void* pNext = nullptr;

	if (useFeatures2)
	{
		pNext = &deviceFeatures2;

		if (m_useDeviceGroups)
		{
			deviceGroupInfo.pNext = deviceFeatures2.pNext;
			deviceFeatures2.pNext = &deviceGroupInfo;
		}

		if (requireShaderImageAtomicInt64Features)
		{
			shaderImageAtomicInt64Features.pNext = deviceFeatures2.pNext;
			deviceFeatures2.pNext = &shaderImageAtomicInt64Features;

			deviceExtensions.push_back("VK_EXT_shader_image_atomic_int64");
		}

		if (requireMaintenance5)
		{
			maintenance5Features.pNext = deviceFeatures2.pNext;
			deviceFeatures2.pNext = &maintenance5Features;

			deviceExtensions.push_back("VK_KHR_maintenance5");
		}
	}
	else if (m_useDeviceGroups)
	{
		pNext = &deviceGroupInfo;
	}
	const VkDeviceCreateInfo		deviceInfo		=
	{
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,								// VkStructureType					sType;
		pNext,																// const void*						pNext;
		(VkDeviceCreateFlags)0,												// VkDeviceCreateFlags				flags;
		static_cast<deUint32>(queueInfos.size())	,						// uint32_t							queueCreateInfoCount;
		&queueInfos[0],														// const VkDeviceQueueCreateInfo*	pQueueCreateInfos;
		0u,																	// uint32_t							enabledLayerCount;
		DE_NULL,															// const char* const*				ppEnabledLayerNames;
		deUint32(deviceExtensions.size()),									// uint32_t							enabledExtensionCount;
		deviceExtensions.size() ? &deviceExtensions[0] : DE_NULL,			// const char* const*				ppEnabledExtensionNames;
		useFeatures2 ? nullptr : &deviceFeatures,	// const VkPhysicalDeviceFeatures*	pEnabledFeatures;
	};

	m_logicalDevice = createCustomDevice(m_context.getTestContext().getCommandLine().isValidationEnabled(), m_context.getPlatformInterface(), instance, instanceDriver, physicalDevice, &deviceInfo);
	m_deviceDriver	= de::MovePtr<DeviceDriver>(new DeviceDriver(m_context.getPlatformInterface(), instance, *m_logicalDevice, m_context.getUsedApiVersion()));
	m_allocator		= de::MovePtr<Allocator>(new SimpleAllocator(*m_deviceDriver, *m_logicalDevice, getPhysicalDeviceMemoryProperties(instanceDriver, physicalDevice)));

	for (QueuesMap::iterator queuesIter = m_queues.begin(); queuesIter != m_queues.end(); ++queuesIter)
	{
		for (deUint32 queueNdx = 0u; queueNdx < queuesIter->second.size(); ++queueNdx)
		{
			Queue& queue = queuesIter->second[queueNdx];

			queue.queueHandle = getDeviceQueue(*m_deviceDriver, *m_logicalDevice, queue.queueFamilyIndex, queue.queueIndex);
		}
	}
}

const Queue& SparseResourcesBaseInstance::getQueue (const VkQueueFlags queueFlags, const deUint32 queueIndex) const
{
	return m_queues.find(queueFlags)->second[queueIndex];
}

} // sparse
} // vkt
