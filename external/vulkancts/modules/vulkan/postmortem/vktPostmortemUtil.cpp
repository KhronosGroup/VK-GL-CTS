/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 The Khronos Group Inc.
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
 * \file
 * \brief Utilties for experimental crash postmortem tests
 *//*--------------------------------------------------------------------*/


#include "vktPostmortemTests.hpp"
#include "vktPostmortemShaderTimeoutTests.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTestCase.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkDefs.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "deUniquePtr.hpp"
#include "tcuCommandLine.hpp"
#include "vktCustomInstancesDevices.hpp"

#include "vktPostmortemUtil.hpp"

using namespace vk;

namespace vkt
{
namespace postmortem
{
namespace
{

Move<VkDevice> createPostmortemDevice(Context& context)
{
	const float queuePriority = 1.0f;

	// Create a universal queue that supports graphics and compute
	const VkDeviceQueueCreateInfo	queueParams =
	{
		VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,	// VkStructureType				sType;
		DE_NULL,									// const void*					pNext;
		0u,											// VkDeviceQueueCreateFlags		flags;
		context.getUniversalQueueFamilyIndex(),		// deUint32						queueFamilyIndex;
		1u,											// deUint32						queueCount;
		&queuePriority								// const float*					pQueuePriorities;
	};

	const VkDeviceCreateInfo		deviceParams =
	{
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,	// VkStructureType					sType;
		DE_NULL,								// const void*						pNext;
		0u,										// VkDeviceCreateFlags				flags;
		1u,										// deUint32							queueCreateInfoCount;
		&queueParams,							// const VkDeviceQueueCreateInfo*	pQueueCreateInfos;
		0u,										// deUint32							enabledLayerCount;
		DE_NULL,								// const char* const*				ppEnabledLayerNames;
		0u,										// deUint32							enabledExtensionCount;
		DE_NULL,								// const char* const*				ppEnabledExtensionNames;
        DE_NULL									// const VkPhysicalDeviceFeatures*	pEnabledFeatures;
	};

	return createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(), context.getPlatformInterface(),
							  context.getInstance(), context.getInstanceInterface(), context.getPhysicalDevice(), &deviceParams);
}
}

PostmortemTestInstance::PostmortemTestInstance(Context& context)
	: TestInstance(context), m_logicalDevice(createPostmortemDevice(context)),
	m_deviceDriver(context.getPlatformInterface(), context.getInstance(), *m_logicalDevice, context.getUsedApiVersion()),
	m_queueFamilyIndex(0),
	m_queue(getDeviceQueue(m_deviceDriver, *m_logicalDevice, m_queueFamilyIndex, 0)),
	m_allocator(m_deviceDriver, *m_logicalDevice, getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice()))
{

}

PostmortemTestInstance::~PostmortemTestInstance()
{

}

}
}
