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
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPlatform.hpp"

#include "deMemory.h"

namespace vkt
{

// Default device utilities

using std::vector;
using namespace vk;

static deUint32 findQueueFamilyIndexWithCaps (const InstanceInterface& vkInstance, VkPhysicalDevice physicalDevice, VkQueueFlags requiredCaps)
{
	const vector<VkPhysicalDeviceQueueProperties>	queueProps	= getPhysicalDeviceQueueProperties(vkInstance, physicalDevice);

	for (size_t queueNdx = 0; queueNdx < queueProps.size(); queueNdx++)
	{
		if ((queueProps[queueNdx].queueFlags & requiredCaps) == requiredCaps)
			return (deUint32)queueNdx;
	}

	TCU_THROW(NotSupportedError, "No matching queue found");
}

struct DeviceCreateInfoHelper
{
	VkPhysicalDeviceFeatures	enabledFeatures;
	VkDeviceQueueCreateInfo		queueInfo;
	VkDeviceCreateInfo			deviceInfo;

	DeviceCreateInfoHelper (deUint32 queueIndex)
	{
		deMemset(&enabledFeatures,	0, sizeof(enabledFeatures));
		deMemset(&queueInfo,		0, sizeof(queueInfo));
		deMemset(&deviceInfo,		0, sizeof(deviceInfo));

		// \todo [2015-07-09 pyry] What's the policy for enabling features?
		//  * Enable all supported by default, and expose that to test cases
		//  * More limited enabled set could be used for verifying that tests behave correctly

		queueInfo.queueFamilyIndex			= queueIndex;
		queueInfo.queueCount				= 1u;

		deviceInfo.sType					= VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceInfo.pNext					= DE_NULL;
		deviceInfo.queueRecordCount			= 1u;
		deviceInfo.pRequestedQueues			= &queueInfo;
		deviceInfo.extensionCount			= 0u;
		deviceInfo.ppEnabledExtensionNames	= DE_NULL;
		deviceInfo.pEnabledFeatures			= &enabledFeatures;
		deviceInfo.flags					= 0u;
	}
};

class DefaultDevice
{
public:
									DefaultDevice					(const PlatformInterface& vkPlatform, const tcu::CommandLine& cmdLine);
									~DefaultDevice					(void);

	VkInstance						getInstance						(void) const	{ return *m_instance;					}
	const InstanceInterface&		getInstanceInterface			(void) const	{ return m_instanceInterface;			}

	VkPhysicalDevice				getPhysicalDevice				(void) const	{ return m_physicalDevice;				}

	VkDevice						getDevice						(void) const	{ return *m_device;						}
	const DeviceInterface&			getDeviceInterface				(void) const	{ return m_deviceInterface;				}

	deUint32						getUniversalQueueFamilyIndex	(void) const	{ return m_universalQueueFamilyIndex;	}
	VkQueue							getUniversalQueue				(void) const;

private:
	const Unique<VkInstance>		m_instance;
	const InstanceDriver			m_instanceInterface;

	const VkPhysicalDevice			m_physicalDevice;

	const deUint32					m_universalQueueFamilyIndex;
	const DeviceCreateInfoHelper	m_deviceCreateInfo;

	const Unique<VkDevice>			m_device;
	const DeviceDriver				m_deviceInterface;
};

DefaultDevice::DefaultDevice (const PlatformInterface& vkPlatform, const tcu::CommandLine& cmdLine)
	: m_instance					(createDefaultInstance(vkPlatform))
	, m_instanceInterface			(vkPlatform, *m_instance)
	, m_physicalDevice				(chooseDevice(m_instanceInterface, *m_instance, cmdLine))
	, m_universalQueueFamilyIndex	(findQueueFamilyIndexWithCaps(m_instanceInterface, m_physicalDevice, VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT))
	, m_deviceCreateInfo			(m_universalQueueFamilyIndex)
	, m_device						(createDevice(m_instanceInterface, m_physicalDevice, &m_deviceCreateInfo.deviceInfo))
	, m_deviceInterface				(m_instanceInterface, *m_device)
{
}

DefaultDevice::~DefaultDevice (void)
{
}

VkQueue DefaultDevice::getUniversalQueue (void) const
{
	VkQueue	queue	= 0;
	VK_CHECK(m_deviceInterface.getDeviceQueue(*m_device, m_universalQueueFamilyIndex, 0, &queue));
	return queue;
}

// Allocator utilities

vk::Allocator* createAllocator (DefaultDevice* device)
{
	const VkPhysicalDeviceMemoryProperties memoryProperties = vk::getPhysicalDeviceMemoryProperties(device->getInstanceInterface(), device->getPhysicalDevice());

	// \todo [2015-07-24 jarkko] support allocator selection/configuration from command line (or compile time)
	return new SimpleAllocator(device->getDeviceInterface(), device->getDevice(), memoryProperties);
}

// Context

Context::Context (tcu::TestContext&							testCtx,
				  const vk::PlatformInterface&				platformInterface,
				  vk::ProgramCollection<vk::ProgramBinary>&	progCollection)
	: m_testCtx				(testCtx)
	, m_platformInterface	(platformInterface)
	, m_progCollection		(progCollection)
	, m_device				(new DefaultDevice(m_platformInterface, testCtx.getCommandLine()))
	, m_allocator			(createAllocator(m_device.get()))
{
}

Context::~Context (void)
{
}

vk::VkInstance					Context::getInstance					(void) const { return m_device->getInstance();					}
const vk::InstanceInterface&	Context::getInstanceInterface			(void) const { return m_device->getInstanceInterface();			}
vk::VkPhysicalDevice			Context::getPhysicalDevice				(void) const { return m_device->getPhysicalDevice();			}
vk::VkDevice					Context::getDevice						(void) const { return m_device->getDevice();					}
const vk::DeviceInterface&		Context::getDeviceInterface				(void) const { return m_device->getDeviceInterface();			}
deUint32						Context::getUniversalQueueFamilyIndex	(void) const { return m_device->getUniversalQueueFamilyIndex();	}
vk::VkQueue						Context::getUniversalQueue				(void) const { return m_device->getUniversalQueue();			}
vk::Allocator&					Context::getDefaultAllocator			(void) const { return *m_allocator;							}

// TestCase

void TestCase::initPrograms (SourceCollections&) const
{
}

} // vkt
