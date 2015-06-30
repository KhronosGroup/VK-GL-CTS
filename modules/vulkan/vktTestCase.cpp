/*-------------------------------------------------------------------------
 * drawElements Quality Program Vulkan Module
 * --------------------------------------------
 *
 * Copyright 2015 The Android Open Source Project
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
 * \brief Vulkan test case base classes
 *//*--------------------------------------------------------------------*/

#include "vktTestCase.hpp"

#include "vkRef.hpp"
#include "vkQueryUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkPlatform.hpp"

#include "deMemory.h"

namespace vkt
{

// Default device utilities

using std::vector;
using namespace vk;

static deUint32 findQueueNodeIndexWithCaps (const DeviceInterface& vkDevice, VkPhysicalDevice physicalDevice, VkQueueFlags requiredCaps)
{
	const vector<VkPhysicalDeviceQueueProperties>	queueProps	= getPhysicalDeviceInfo<VK_PHYSICAL_DEVICE_INFO_TYPE_QUEUE_PROPERTIES>(vkDevice, physicalDevice);

	for (size_t queueNdx = 0; queueNdx < queueProps.size(); queueNdx++)
	{
		if ((queueProps[queueNdx].queueFlags & requiredCaps) == requiredCaps)
			return (deUint32)queueNdx;
	}

	TCU_THROW(NotSupportedError, "No matching queue found");
}

struct DeviceCreateInfoHelper
{
	VkDeviceQueueCreateInfo		queueInfo;
	VkDeviceCreateInfo			deviceInfo;

	DeviceCreateInfoHelper (deUint32 queueIndex)
	{
		deMemset(&queueInfo, 0, sizeof(queueInfo));
		deMemset(&deviceInfo, 0, sizeof(deviceInfo));

		queueInfo.queueNodeIndex			= queueIndex;
		queueInfo.queueCount				= 1u;

		deviceInfo.sType					= VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceInfo.pNext					= DE_NULL;
		deviceInfo.queueRecordCount			= 1u;
		deviceInfo.pRequestedQueues			= &queueInfo;
		deviceInfo.extensionCount			= 0u;
		deviceInfo.ppEnabledExtensionNames	= DE_NULL;
		deviceInfo.flags					= 0u;
	}
};

class DefaultDevice
{
public:
									DefaultDevice			(const PlatformInterface& vkPlatform, const tcu::CommandLine& cmdLine);
									~DefaultDevice			(void);

	VkInstance						getInstance				(void) const	{ return *m_instance;			}
	VkPhysicalDevice				getPhysicalDevice		(void) const	{ return m_physicalDevice;		}
	VkDevice						getDevice				(void) const	{ return *m_device;				}
	const DeviceInterface&			getInterface			(void) const	{ return m_deviceInterface;		}
	deUint32						getUniversalQueueIndex	(void) const	{ return m_universalQueueIndex;	}
	VkQueue							getUniversalQueue		(void) const;

private:
	const Unique<VkInstanceT>		m_instance;
	const VkPhysicalDevice			m_physicalDevice;
	const DeviceDriver				m_deviceInterface;
	const deUint32					m_universalQueueIndex;
	const DeviceCreateInfoHelper	m_deviceCreateInfo;
	const Unique<VkDeviceT>			m_device;
};

DefaultDevice::DefaultDevice (const PlatformInterface& vkPlatform, const tcu::CommandLine& cmdLine)
	: m_instance			(createDefaultInstance(vkPlatform))
	, m_physicalDevice		(chooseDevice(vkPlatform, *m_instance, cmdLine))
	, m_deviceInterface		(vkPlatform, m_physicalDevice)
	, m_universalQueueIndex	(findQueueNodeIndexWithCaps(m_deviceInterface, m_physicalDevice, VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT|VK_QUEUE_DMA_BIT|VK_QUEUE_MEMMGR_BIT))
	, m_deviceCreateInfo	(m_universalQueueIndex)
	, m_device				(createDevice(m_deviceInterface, m_physicalDevice, &m_deviceCreateInfo.deviceInfo))
{
}

DefaultDevice::~DefaultDevice (void)
{
}

VkQueue DefaultDevice::getUniversalQueue (void) const
{
	VkQueue	queue	= 0;
	VK_CHECK(m_deviceInterface.getDeviceQueue(*m_device, m_universalQueueIndex, 0, &queue));
	return queue;
}

// Context

Context::Context (tcu::TestContext&							testCtx,
				  const vk::PlatformInterface&				platformInterface,
				  vk::ProgramCollection<vk::ProgramBinary>&	progCollection)
	: m_testCtx				(testCtx)
	, m_platformInterface	(platformInterface)
	, m_progCollection		(progCollection)
	, m_device				(new DefaultDevice(m_platformInterface, testCtx.getCommandLine()))
{
}

Context::~Context (void)
{
	delete m_device;
}

vk::VkInstance				Context::getInstance			(void) const { return m_device->getInstance();				}
vk::VkPhysicalDevice		Context::getPhysicalDevice		(void) const { return m_device->getPhysicalDevice();		}
vk::VkDevice				Context::getDevice				(void) const { return m_device->getDevice();				}
const vk::DeviceInterface&	Context::getDeviceInterface		(void) const { return m_device->getInterface();				}
deUint32					Context::getUniversalQueueIndex	(void) const { return m_device->getUniversalQueueIndex();	}
vk::VkQueue					Context::getUniversalQueue		(void) const { return m_device->getUniversalQueue();		}

// TestCase

void TestCase::initPrograms (vk::ProgramCollection<glu::ProgramSources>&) const
{
}

} // vkt
