/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Google Inc.
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
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPlatform.hpp"
#include "vkDebugReportUtil.hpp"

#include "tcuCommandLine.hpp"

#include "deMemory.h"

namespace vkt
{

// Default device utilities

using std::vector;
using std::string;
using namespace vk;

vector<string> getValidationLayers (const vector<VkLayerProperties>& supportedLayers)
{
	static const char*	s_magicLayer		= "VK_LAYER_LUNARG_standard_validation";
	static const char*	s_defaultLayers[]	=
	{
		"VK_LAYER_GOOGLE_threading",
		"VK_LAYER_LUNARG_parameter_validation",
		"VK_LAYER_LUNARG_device_limits",
		"VK_LAYER_LUNARG_object_tracker",
		"VK_LAYER_LUNARG_image",
		"VK_LAYER_LUNARG_core_validation",
		"VK_LAYER_LUNARG_swapchain",
		"VK_LAYER_GOOGLE_unique_objects"
	};

	vector<string>		enabledLayers;

	if (isLayerSupported(supportedLayers, RequiredLayer(s_magicLayer)))
		enabledLayers.push_back(s_magicLayer);
	else
	{
		for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(s_defaultLayers); ++ndx)
		{
			if (isLayerSupported(supportedLayers, RequiredLayer(s_defaultLayers[ndx])))
				enabledLayers.push_back(s_defaultLayers[ndx]);
		}
	}

	return enabledLayers;
}

vector<string> getValidationLayers (const PlatformInterface& vkp)
{
	return getValidationLayers(enumerateInstanceLayerProperties(vkp));
}

vector<string> getValidationLayers (const InstanceInterface& vki, VkPhysicalDevice physicalDevice)
{
	return getValidationLayers(enumerateDeviceLayerProperties(vki, physicalDevice));
}

Move<VkInstance> createInstance (const PlatformInterface& vkp, const tcu::CommandLine& cmdLine)
{
	const bool		isValidationEnabled	= cmdLine.isValidationEnabled();
	vector<string>	enabledExtensions;
	vector<string>	enabledLayers;

	if (isValidationEnabled)
	{
		if (isDebugReportSupported(vkp))
			enabledExtensions.push_back("VK_EXT_debug_report");
		else
			TCU_THROW(NotSupportedError, "VK_EXT_debug_report is not supported");

		enabledLayers = getValidationLayers(vkp);
		if (enabledLayers.empty())
			TCU_THROW(NotSupportedError, "No validation layers found");
	}

	return createDefaultInstance(vkp, enabledLayers, enabledExtensions);
}

static deUint32 findQueueFamilyIndexWithCaps (const InstanceInterface& vkInstance, VkPhysicalDevice physicalDevice, VkQueueFlags requiredCaps)
{
	const vector<VkQueueFamilyProperties>	queueProps	= getPhysicalDeviceQueueFamilyProperties(vkInstance, physicalDevice);

	for (size_t queueNdx = 0; queueNdx < queueProps.size(); queueNdx++)
	{
		if ((queueProps[queueNdx].queueFlags & requiredCaps) == requiredCaps)
			return (deUint32)queueNdx;
	}

	TCU_THROW(NotSupportedError, "No matching queue found");
}

Move<VkDevice> createDefaultDevice (const InstanceInterface&		vki,
									VkPhysicalDevice				physicalDevice,
									deUint32						queueIndex,
									const VkPhysicalDeviceFeatures&	enabledFeatures,
									const tcu::CommandLine&			cmdLine)
{
	VkDeviceQueueCreateInfo		queueInfo;
	VkDeviceCreateInfo			deviceInfo;
	vector<string>				enabledLayers;
	vector<const char*>			layerPtrs;
	const float					queuePriority	= 1.0f;

	deMemset(&queueInfo,	0, sizeof(queueInfo));
	deMemset(&deviceInfo,	0, sizeof(deviceInfo));

	if (cmdLine.isValidationEnabled())
	{
		enabledLayers = getValidationLayers(vki, physicalDevice);
		if (enabledLayers.empty())
			TCU_THROW(NotSupportedError, "No validation layers found");
	}

	layerPtrs.resize(enabledLayers.size());

	for (size_t ndx = 0; ndx < enabledLayers.size(); ++ndx)
		layerPtrs[ndx] = enabledLayers[ndx].c_str();

	queueInfo.sType							= VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueInfo.pNext							= DE_NULL;
	queueInfo.flags							= (VkDeviceQueueCreateFlags)0u;
	queueInfo.queueFamilyIndex				= queueIndex;
	queueInfo.queueCount					= 1u;
	queueInfo.pQueuePriorities				= &queuePriority;

	deviceInfo.sType						= VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceInfo.pNext						= DE_NULL;
	deviceInfo.queueCreateInfoCount			= 1u;
	deviceInfo.pQueueCreateInfos			= &queueInfo;
	deviceInfo.enabledExtensionCount		= 0u;
	deviceInfo.ppEnabledExtensionNames		= DE_NULL;
	deviceInfo.enabledLayerCount			= (deUint32)layerPtrs.size();
	deviceInfo.ppEnabledLayerNames			= (layerPtrs.empty() ? DE_NULL : &layerPtrs[0]);
	deviceInfo.pEnabledFeatures				= &enabledFeatures;

	return createDevice(vki, physicalDevice, &deviceInfo);
};

class DefaultDevice
{
public:
										DefaultDevice					(const PlatformInterface& vkPlatform, const tcu::CommandLine& cmdLine);
										~DefaultDevice					(void);

	VkInstance							getInstance						(void) const	{ return *m_instance;					}
	const InstanceInterface&			getInstanceInterface			(void) const	{ return m_instanceInterface;			}

	VkPhysicalDevice					getPhysicalDevice				(void) const	{ return m_physicalDevice;				}
	const VkPhysicalDeviceFeatures&		getDeviceFeatures				(void) const	{ return m_deviceFeatures;				}
	VkDevice							getDevice						(void) const	{ return *m_device;						}
	const DeviceInterface&				getDeviceInterface				(void) const	{ return m_deviceInterface;				}
	const VkPhysicalDeviceProperties&	getDeviceProperties				(void) const	{ return m_deviceProperties;			}

	deUint32							getUniversalQueueFamilyIndex	(void) const	{ return m_universalQueueFamilyIndex;	}
	VkQueue								getUniversalQueue				(void) const;

private:
	static VkPhysicalDeviceFeatures		filterDefaultDeviceFeatures		(const VkPhysicalDeviceFeatures& deviceFeatures);

	const Unique<VkInstance>			m_instance;
	const InstanceDriver				m_instanceInterface;

	const VkPhysicalDevice				m_physicalDevice;

	const deUint32						m_universalQueueFamilyIndex;
	const VkPhysicalDeviceFeatures		m_deviceFeatures;
	const VkPhysicalDeviceProperties	m_deviceProperties;

	const Unique<VkDevice>				m_device;
	const DeviceDriver					m_deviceInterface;
};

DefaultDevice::DefaultDevice (const PlatformInterface& vkPlatform, const tcu::CommandLine& cmdLine)
	: m_instance					(createInstance(vkPlatform, cmdLine))
	, m_instanceInterface			(vkPlatform, *m_instance)
	, m_physicalDevice				(chooseDevice(m_instanceInterface, *m_instance, cmdLine))
	, m_universalQueueFamilyIndex	(findQueueFamilyIndexWithCaps(m_instanceInterface, m_physicalDevice, VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT))
	, m_deviceFeatures				(filterDefaultDeviceFeatures(getPhysicalDeviceFeatures(m_instanceInterface, m_physicalDevice)))
	, m_deviceProperties			(getPhysicalDeviceProperties(m_instanceInterface, m_physicalDevice))
	, m_device						(createDefaultDevice(m_instanceInterface, m_physicalDevice, m_universalQueueFamilyIndex, m_deviceFeatures, cmdLine))
	, m_deviceInterface				(m_instanceInterface, *m_device)
{
}

DefaultDevice::~DefaultDevice (void)
{
}

VkQueue DefaultDevice::getUniversalQueue (void) const
{
	VkQueue	queue	= 0;
	m_deviceInterface.getDeviceQueue(*m_device, m_universalQueueFamilyIndex, 0, &queue);
	return queue;
}

VkPhysicalDeviceFeatures DefaultDevice::filterDefaultDeviceFeatures (const VkPhysicalDeviceFeatures& deviceFeatures)
{
	VkPhysicalDeviceFeatures enabledDeviceFeatures = deviceFeatures;

	// Disable robustness by default, as it has an impact on performance on some HW.
	enabledDeviceFeatures.robustBufferAccess = false;

	return enabledDeviceFeatures;
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

vk::VkInstance							Context::getInstance					(void) const { return m_device->getInstance();					}
const vk::InstanceInterface&			Context::getInstanceInterface			(void) const { return m_device->getInstanceInterface();			}
vk::VkPhysicalDevice					Context::getPhysicalDevice				(void) const { return m_device->getPhysicalDevice();			}
const vk::VkPhysicalDeviceFeatures&		Context::getDeviceFeatures				(void) const { return m_device->getDeviceFeatures();			}
const vk::VkPhysicalDeviceProperties&	Context::getDeviceProperties			(void) const { return m_device->getDeviceProperties();			}
vk::VkDevice							Context::getDevice						(void) const { return m_device->getDevice();					}
const vk::DeviceInterface&				Context::getDeviceInterface				(void) const { return m_device->getDeviceInterface();			}
deUint32								Context::getUniversalQueueFamilyIndex	(void) const { return m_device->getUniversalQueueFamilyIndex();	}
vk::VkQueue								Context::getUniversalQueue				(void) const { return m_device->getUniversalQueue();			}
vk::Allocator&							Context::getDefaultAllocator			(void) const { return *m_allocator;								}

// TestCase

void TestCase::initPrograms (SourceCollections&) const
{
}

} // vkt
