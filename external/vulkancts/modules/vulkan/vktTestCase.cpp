/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Google Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be
 * included in all copies or substantial portions of the Materials.
 *
 * The Materials are Confidential Information as defined by the
 * Khronos Membership Agreement until designated non-confidential by
 * Khronos, at which point this condition clause shall be removed.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
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
