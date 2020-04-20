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
 * \file
 * \brief Synchronization tests for resources shared between instances.
 *//*--------------------------------------------------------------------*/

#include "vktSynchronizationCrossInstanceSharingTests.hpp"

#include "vkDeviceUtil.hpp"
#include "vkPlatform.hpp"
#include "vkCmdUtil.hpp"
#include "vktTestCaseUtil.hpp"
#include "deSharedPtr.hpp"

#include "vktSynchronizationUtil.hpp"
#include "vktSynchronizationOperation.hpp"
#include "vktSynchronizationOperationTestData.hpp"
#include "vktSynchronizationOperationResources.hpp"
#include "vktExternalMemoryUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktCustomInstancesDevices.hpp"

#include "deRandom.hpp"

#include "tcuResultCollector.hpp"
#include "tcuTestLog.hpp"
#include "tcuCommandLine.hpp"

using tcu::TestLog;
using namespace vkt::ExternalMemoryUtil;

namespace vkt
{
namespace synchronization
{
namespace
{
using namespace vk;
using de::SharedPtr;

struct TestConfig
{
								TestConfig		(const ResourceDescription&						resource_,
												 vk::VkSemaphoreType							semaphoreType_,
												 OperationName									writeOp_,
												 OperationName									readOp_,
												 vk::VkExternalMemoryHandleTypeFlagBits			memoryHandleType_,
												 vk::VkExternalSemaphoreHandleTypeFlagBits		semaphoreHandleType_,
												 bool											dedicated_)
		: resource				(resource_)
		, semaphoreType			(semaphoreType_)
		, writeOp				(writeOp_)
		, readOp				(readOp_)
		, memoryHandleType		(memoryHandleType_)
		, semaphoreHandleType	(semaphoreHandleType_)
		, dedicated				(dedicated_)
	{
	}

	const ResourceDescription							resource;
	const vk::VkSemaphoreType							semaphoreType;
	const OperationName									writeOp;
	const OperationName									readOp;
	const vk::VkExternalMemoryHandleTypeFlagBits		memoryHandleType;
	const vk::VkExternalSemaphoreHandleTypeFlagBits		semaphoreHandleType;
	const bool											dedicated;
};

// A helper class to test for extensions upfront and throw not supported to speed up test runtimes compared to failing only
// after creating unnecessary vkInstances.  A common example of this is win32 platforms taking a long time to run _fd tests.
class NotSupportedChecker
{
public:
				NotSupportedChecker	(const Context&			 context,
									 TestConfig				 config,
									 const OperationSupport& writeOp,
									 const OperationSupport& readOp)
	: m_context	(context)
	{
		// Check instance support
		m_context.requireInstanceFunctionality("VK_KHR_get_physical_device_properties2");

		m_context.requireInstanceFunctionality("VK_KHR_external_semaphore_capabilities");
		m_context.requireInstanceFunctionality("VK_KHR_external_memory_capabilities");

		// Check device support
		if (config.dedicated)
			m_context.requireDeviceFunctionality("VK_KHR_dedicated_allocation");

		m_context.requireDeviceFunctionality("VK_KHR_external_semaphore");
		m_context.requireDeviceFunctionality("VK_KHR_external_memory");

		if (config.semaphoreType == vk::VK_SEMAPHORE_TYPE_TIMELINE_KHR)
		{
			m_context.requireDeviceFunctionality("VK_KHR_timeline_semaphore");
		}

		if (config.memoryHandleType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR
			|| config.semaphoreHandleType == vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT_KHR
			|| config.semaphoreHandleType == vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT_KHR)
		{
			m_context.requireDeviceFunctionality("VK_KHR_external_semaphore_fd");
			m_context.requireDeviceFunctionality("VK_KHR_external_memory_fd");
		}

		if (config.memoryHandleType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT)
		{
			m_context.requireDeviceFunctionality("VK_EXT_external_memory_dma_buf");
		}

		if (config.memoryHandleType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR
			|| config.memoryHandleType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT_KHR
			|| config.semaphoreHandleType == vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR
			|| config.semaphoreHandleType == vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT_KHR)
		{
			m_context.requireDeviceFunctionality("VK_KHR_external_semaphore_win32");
			m_context.requireDeviceFunctionality("VK_KHR_external_memory_win32");
		}

		TestLog&						log				= context.getTestContext().getLog();
		const vk::InstanceInterface&	vki				= context.getInstanceInterface();
		const vk::VkPhysicalDevice		physicalDevice	= context.getPhysicalDevice();

		// Check resource support
		if (config.resource.type == RESOURCE_TYPE_IMAGE)
		{
			const vk::VkPhysicalDeviceExternalImageFormatInfo	externalInfo		=
			{
				vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO,
				DE_NULL,
				config.memoryHandleType
			};
			const vk::VkPhysicalDeviceImageFormatInfo2			imageFormatInfo		=
			{
				vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
				&externalInfo,
				config.resource.imageFormat,
				config.resource.imageType,
				vk::VK_IMAGE_TILING_OPTIMAL,
				readOp.getInResourceUsageFlags() | writeOp.getOutResourceUsageFlags(),
				0u
			};
			vk::VkExternalImageFormatProperties				externalProperties	=
			{
				vk::VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES,
				DE_NULL,
				{ 0u, 0u, 0u }
			};
			vk::VkImageFormatProperties2					formatProperties	=
			{
				vk::VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
				&externalProperties,
				{
					{ 0u, 0u, 0u },
					0u,
					0u,
					0u,
					0u,
				}
			};

			{
				const vk::VkResult res = vki.getPhysicalDeviceImageFormatProperties2(physicalDevice, &imageFormatInfo, &formatProperties);

				if (res == vk::VK_ERROR_FORMAT_NOT_SUPPORTED)
					TCU_THROW(NotSupportedError, "Image format not supported");

				VK_CHECK(res); // Check other errors
			}

			log << TestLog::Message << "External image format properties: " << imageFormatInfo << "\n"<< externalProperties << TestLog::EndMessage;

			if ((externalProperties.externalMemoryProperties.externalMemoryFeatures & vk::VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT_KHR) == 0)
				TCU_THROW(NotSupportedError, "Exporting image resource not supported");

			if ((externalProperties.externalMemoryProperties.externalMemoryFeatures & vk::VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT_KHR) == 0)
				TCU_THROW(NotSupportedError, "Importing image resource not supported");

			if (!config.dedicated && (externalProperties.externalMemoryProperties.externalMemoryFeatures & vk::VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT_KHR) != 0)
			{
				TCU_THROW(NotSupportedError, "Handle requires dedicated allocation, but test uses suballocated memory");
			}
		}
		else
		{
			const vk::VkPhysicalDeviceExternalBufferInfo	info	=
			{
				vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_BUFFER_INFO,
				DE_NULL,

				0u,
				readOp.getInResourceUsageFlags() | writeOp.getOutResourceUsageFlags(),
				config.memoryHandleType
			};
			vk::VkExternalBufferProperties					properties			=
			{
				vk::VK_STRUCTURE_TYPE_EXTERNAL_BUFFER_PROPERTIES,
				DE_NULL,
				{ 0u, 0u, 0u}
			};
			vki.getPhysicalDeviceExternalBufferProperties(physicalDevice, &info, &properties);

			log << TestLog::Message << "External buffer properties: " << info << "\n" << properties << TestLog::EndMessage;

			if ((properties.externalMemoryProperties.externalMemoryFeatures & vk::VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT_KHR) == 0
				|| (properties.externalMemoryProperties.externalMemoryFeatures & vk::VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT_KHR) == 0)
				TCU_THROW(NotSupportedError, "Exporting and importing memory type not supported");

			if (!config.dedicated && (properties.externalMemoryProperties.externalMemoryFeatures & vk::VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT_KHR) != 0)
			{
				TCU_THROW(NotSupportedError, "Handle requires dedicated allocation, but test uses suballocated memory");
			}
		}

		// Check semaphore support
		{
			const vk::VkSemaphoreTypeCreateInfoKHR			semaphoreTypeInfo	=
			{
				vk::VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO_KHR,
				DE_NULL,
				config.semaphoreType,
				0,
			};
			const vk::VkPhysicalDeviceExternalSemaphoreInfo	info				=
			{
				vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO,
				&semaphoreTypeInfo,
				config.semaphoreHandleType
			};

			vk::VkExternalSemaphoreProperties				properties			=
			{
				vk::VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES,
				DE_NULL,
				0u,
				0u,
				0u
			};

			vki.getPhysicalDeviceExternalSemaphoreProperties(physicalDevice, &info, &properties);

			log << TestLog::Message << info << "\n" << properties << TestLog::EndMessage;

			if ((properties.externalSemaphoreFeatures & vk::VK_EXTERNAL_SEMAPHORE_FEATURE_EXPORTABLE_BIT_KHR) == 0
				|| (properties.externalSemaphoreFeatures & vk::VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT_KHR) == 0)
				TCU_THROW(NotSupportedError, "Exporting and importing semaphore type not supported");
		}
	}

private:

	const Context& m_context;
};

bool checkQueueFlags (vk::VkQueueFlags availableFlags, const vk::VkQueueFlags neededFlags)
{
	if ((availableFlags & (vk::VK_QUEUE_GRAPHICS_BIT | vk::VK_QUEUE_COMPUTE_BIT)) != 0)
		availableFlags |= vk::VK_QUEUE_TRANSFER_BIT;

	return (availableFlags & neededFlags) != 0;
}

class SimpleAllocation : public vk::Allocation
{
public:
								SimpleAllocation	(const vk::DeviceInterface&	vkd,
													 vk::VkDevice				device,
													 const vk::VkDeviceMemory	memory);
								~SimpleAllocation	(void);

private:
	const vk::DeviceInterface&	m_vkd;
	const vk::VkDevice			m_device;
};

SimpleAllocation::SimpleAllocation (const vk::DeviceInterface&	vkd,
									vk::VkDevice				device,
									const vk::VkDeviceMemory	memory)
	: Allocation	(memory, 0, DE_NULL)
	, m_vkd			(vkd)
	, m_device		(device)
{
}

SimpleAllocation::~SimpleAllocation (void)
{
	m_vkd.freeMemory(m_device, getMemory(), DE_NULL);
}

CustomInstance createTestInstance (Context& context)
{
	std::vector<std::string> extensions;
	extensions.push_back("VK_KHR_get_physical_device_properties2");
	extensions.push_back("VK_KHR_external_semaphore_capabilities");
	extensions.push_back("VK_KHR_external_memory_capabilities");

	return createCustomInstanceWithExtensions(context, extensions);
}

vk::Move<vk::VkDevice> createTestDevice (const Context&					context,
										 const vk::PlatformInterface&	vkp,
										 vk::VkInstance					instance,
										 const vk::InstanceInterface&	vki,
										 const vk::VkPhysicalDevice		physicalDevice)
{
	const bool										validationEnabled		= context.getTestContext().getCommandLine().isValidationEnabled();
	const float										priority				= 0.0f;
	const std::vector<vk::VkQueueFamilyProperties>	queueFamilyProperties	= vk::getPhysicalDeviceQueueFamilyProperties(vki, physicalDevice);
	std::vector<deUint32>							queueFamilyIndices		(queueFamilyProperties.size(), 0xFFFFFFFFu);
	std::vector<const char*>						extensions;

	if (context.isDeviceFunctionalitySupported("VK_KHR_dedicated_allocation"))
		extensions.push_back("VK_KHR_dedicated_allocation");

	if (context.isDeviceFunctionalitySupported("VK_KHR_get_memory_requirements2"))
		extensions.push_back("VK_KHR_get_memory_requirements2");

	if (context.isDeviceFunctionalitySupported("VK_KHR_external_semaphore"))
		extensions.push_back("VK_KHR_external_semaphore");
	if (context.isDeviceFunctionalitySupported("VK_KHR_external_memory"))
		extensions.push_back("VK_KHR_external_memory");

	if (context.isDeviceFunctionalitySupported("VK_KHR_external_semaphore_fd"))
		extensions.push_back("VK_KHR_external_semaphore_fd");
	if (context.isDeviceFunctionalitySupported("VK_KHR_external_memory_fd"))
		extensions.push_back("VK_KHR_external_memory_fd");

	if (context.isDeviceFunctionalitySupported("VK_EXT_external_memory_dma_buf"))
		extensions.push_back("VK_EXT_external_memory_dma_buf");

	if (context.isDeviceFunctionalitySupported("VK_KHR_external_semaphore_win32"))
		extensions.push_back("VK_KHR_external_semaphore_win32");
	if (context.isDeviceFunctionalitySupported("VK_KHR_external_memory_win32"))
		extensions.push_back("VK_KHR_external_memory_win32");

	if (context.isDeviceFunctionalitySupported("VK_KHR_timeline_semaphore"))
		extensions.push_back("VK_KHR_timeline_semaphore");

	try
	{
		std::vector<vk::VkDeviceQueueCreateInfo>	queues;

		for (size_t ndx = 0; ndx < queueFamilyProperties.size(); ndx++)
		{
			const vk::VkDeviceQueueCreateInfo	createInfo	=
			{
				vk::VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				DE_NULL,
				0u,

				(deUint32)ndx,
				1u,
				&priority
			};

			queues.push_back(createInfo);
		}

		const vk::VkPhysicalDeviceFeatures2	createPhysicalFeature	=
		{
			vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
			DE_NULL,
			context.getDeviceFeatures(),
		};
		const vk::VkDeviceCreateInfo		createInfo				=
		{
			vk::VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			&createPhysicalFeature,
			0u,

			(deUint32)queues.size(),
			&queues[0],

			0u,
			DE_NULL,

			(deUint32)extensions.size(),
			extensions.empty() ? DE_NULL : &extensions[0],
			0u
		};

		return vkt::createCustomDevice(validationEnabled, vkp, instance, vki, physicalDevice, &createInfo);
	}
	catch (const vk::Error& error)
	{
		if (error.getError() == vk::VK_ERROR_EXTENSION_NOT_PRESENT)
			TCU_THROW(NotSupportedError, "Required extensions not supported");
		else
			throw;
	}
}

// Class to wrap a singleton instance and device
class InstanceAndDevice
{
	InstanceAndDevice	(Context& context)
		: m_instance		(createTestInstance(context))
		, m_vki				(m_instance.getDriver())
		, m_physicalDevice	(vk::chooseDevice(m_vki, m_instance, context.getTestContext().getCommandLine()))
		, m_logicalDevice	(createTestDevice(context, context.getPlatformInterface(), m_instance, m_vki, m_physicalDevice))
	{
	}

public:

	static vk::VkInstance getInstanceA(Context& context)
	{
		if (!m_instanceA)
			m_instanceA = SharedPtr<InstanceAndDevice>(new InstanceAndDevice(context));

		return m_instanceA->m_instance;
	}
	static vk::VkInstance getInstanceB(Context& context)
	{
		if (!m_instanceB)
			m_instanceB = SharedPtr<InstanceAndDevice>(new InstanceAndDevice(context));

		return m_instanceB->m_instance;
	}
	static const vk::InstanceDriver& getDriverA()
	{
		DE_ASSERT(m_instanceA);
		return m_instanceA->m_instance.getDriver();
	}
	static const vk::InstanceDriver& getDriverB()
	{
		DE_ASSERT(m_instanceB);
		return m_instanceB->m_instance.getDriver();
	}
	static vk::VkPhysicalDevice getPhysicalDeviceA()
	{
		DE_ASSERT(m_instanceA);
		return m_instanceA->m_physicalDevice;
	}
	static vk::VkPhysicalDevice getPhysicalDeviceB()
	{
		DE_ASSERT(m_instanceB);
		return m_instanceB->m_physicalDevice;
	}
	static const Unique<vk::VkDevice>& getDeviceA()
	{
		DE_ASSERT(m_instanceA);
		return m_instanceA->m_logicalDevice;
	}
	static const Unique<vk::VkDevice>& getDeviceB()
	{
		DE_ASSERT(m_instanceB);
		return m_instanceB->m_logicalDevice;
	}
	static void collectMessagesA()
	{
		DE_ASSERT(m_instanceA);
		m_instanceA->m_instance.collectMessages();
	}
	static void collectMessagesB()
	{
		DE_ASSERT(m_instanceB);
		m_instanceB->m_instance.collectMessages();
	}
	static void destroy()
	{
		m_instanceA.clear();
		m_instanceB.clear();
	}

private:
	CustomInstance					m_instance;
	const vk::InstanceDriver&		m_vki;
	const vk::VkPhysicalDevice		m_physicalDevice;
	const Unique<vk::VkDevice>		m_logicalDevice;

	static SharedPtr<InstanceAndDevice>	m_instanceA;
	static SharedPtr<InstanceAndDevice>	m_instanceB;
};
SharedPtr<InstanceAndDevice>		InstanceAndDevice::m_instanceA;
SharedPtr<InstanceAndDevice>		InstanceAndDevice::m_instanceB;


vk::VkQueue getQueue (const vk::DeviceInterface&	vkd,
					  const vk::VkDevice			device,
					  deUint32						familyIndex)
{
	vk::VkQueue queue;

	vkd.getDeviceQueue(device, familyIndex, 0u, &queue);

	return queue;
}

vk::Move<vk::VkCommandPool> createCommandPool (const vk::DeviceInterface&	vkd,
											   vk::VkDevice					device,
											   deUint32						queueFamilyIndex)
{
	const vk::VkCommandPoolCreateInfo	createInfo			=
	{
		vk::VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		DE_NULL,

		0u,
		queueFamilyIndex
	};

	return vk::createCommandPool(vkd, device, &createInfo);
}

vk::Move<vk::VkCommandBuffer> createCommandBuffer (const vk::DeviceInterface&	vkd,
												   vk::VkDevice					device,
												   vk::VkCommandPool			commandPool)
{
	const vk::VkCommandBufferLevel			level			= vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	const vk::VkCommandBufferAllocateInfo	allocateInfo	=
	{
		vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		DE_NULL,

		commandPool,
		level,
		1u
	};

	return vk::allocateCommandBuffer(vkd, device, &allocateInfo);
}

vk::VkMemoryRequirements getMemoryRequirements(const vk::DeviceInterface&				vkd,
											   vk::VkDevice								device,
											   vk::VkImage								image,
											   bool										dedicated,
											   bool										getMemReq2Supported)
{
	vk::VkMemoryRequirements memoryRequirements = { 0u, 0u, 0u, };

	if (getMemReq2Supported)
	{
		const vk::VkImageMemoryRequirementsInfo2	requirementInfo =
		{
			vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
			DE_NULL,
			image
		};
		vk::VkMemoryDedicatedRequirements			dedicatedRequirements =
		{
			vk::VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS,
			DE_NULL,
			VK_FALSE,
			VK_FALSE
		};
		vk::VkMemoryRequirements2					requirements =
		{
			vk::VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
			&dedicatedRequirements,
			{ 0u, 0u, 0u, }
		};
		vkd.getImageMemoryRequirements2(device, &requirementInfo, &requirements);

		if (!dedicated && dedicatedRequirements.requiresDedicatedAllocation)
			TCU_THROW(NotSupportedError, "Memory requires dedicated allocation");

		memoryRequirements = requirements.memoryRequirements;
	}
	else
	{
		vkd.getImageMemoryRequirements(device, image, &memoryRequirements);
	}

	return memoryRequirements;
}

vk::VkMemoryRequirements getMemoryRequirements(const vk::DeviceInterface&				vkd,
											   vk::VkDevice								device,
											   vk::VkBuffer								buffer,
											   bool										dedicated,
											   bool										getMemReq2Supported)
{
	vk::VkMemoryRequirements memoryRequirements = { 0u, 0u, 0u, };

	if (getMemReq2Supported)
	{
		const vk::VkBufferMemoryRequirementsInfo2	requirementInfo =
		{
			vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2,
			DE_NULL,
			buffer
		};
		vk::VkMemoryDedicatedRequirements			dedicatedRequirements =
		{
			vk::VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS,
			DE_NULL,
			VK_FALSE,
			VK_FALSE
		};
		vk::VkMemoryRequirements2					requirements =
		{
			vk::VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
			&dedicatedRequirements,
			{ 0u, 0u, 0u, }
		};
		vkd.getBufferMemoryRequirements2(device, &requirementInfo, &requirements);

		if (!dedicated && dedicatedRequirements.requiresDedicatedAllocation)
			TCU_THROW(NotSupportedError, "Memory requires dedicated allocation");

		memoryRequirements = requirements.memoryRequirements;
	}
	else
	{
		vkd.getBufferMemoryRequirements(device, buffer, &memoryRequirements);
	}

	return memoryRequirements;
}

Move<VkImage> createImage(const vk::DeviceInterface&				vkd,
						  vk::VkDevice								device,
						  const ResourceDescription&				resourceDesc,
						  const vk::VkExtent3D						extent,
						  const std::vector<deUint32>&				queueFamilyIndices,
						  const OperationSupport&					readOp,
						  const OperationSupport&					writeOp,
						  vk::VkExternalMemoryHandleTypeFlagBits	externalType)
{
	const vk::VkExternalMemoryImageCreateInfo externalInfo =
	{
		vk::VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
		DE_NULL,
		(vk::VkExternalMemoryHandleTypeFlags)externalType
	};
	const vk::VkImageCreateInfo			createInfo =
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		&externalInfo,
		0u,

		resourceDesc.imageType,
		resourceDesc.imageFormat,
		extent,
		1u,
		1u,
		vk::VK_SAMPLE_COUNT_1_BIT,
		vk::VK_IMAGE_TILING_OPTIMAL,
		readOp.getInResourceUsageFlags() | writeOp.getOutResourceUsageFlags(),
		vk::VK_SHARING_MODE_EXCLUSIVE,

		(deUint32)queueFamilyIndices.size(),
		&queueFamilyIndices[0],
		vk::VK_IMAGE_LAYOUT_UNDEFINED
	};

	return vk::createImage(vkd, device, &createInfo);
}

Move<VkBuffer> createBuffer(const vk::DeviceInterface&						vkd,
							vk::VkDevice									device,
							const vk::VkDeviceSize							size,
							const vk::VkBufferUsageFlags					usage,
							const vk::VkExternalMemoryHandleTypeFlagBits	memoryHandleType,
							const std::vector<deUint32>&					queueFamilyIndices)
{
	const vk::VkExternalMemoryBufferCreateInfo	externalInfo =
	{
		vk::VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO,
		DE_NULL,
		(vk::VkExternalMemoryHandleTypeFlags)memoryHandleType
	};
	const vk::VkBufferCreateInfo				createInfo =
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		&externalInfo,
		0u,

		size,
		usage,
		vk::VK_SHARING_MODE_EXCLUSIVE,
		(deUint32)queueFamilyIndices.size(),
		&queueFamilyIndices[0]
	};
	return vk::createBuffer(vkd, device, &createInfo);
}

de::MovePtr<vk::Allocation> importAndBindMemory (const vk::DeviceInterface&					vkd,
												 vk::VkDevice								device,
												 vk::VkBuffer								buffer,
												 NativeHandle&								nativeHandle,
												 vk::VkExternalMemoryHandleTypeFlagBits	externalType,
												 deUint32									exportedMemoryTypeIndex,
												 bool										dedicated)
{
	const vk::VkMemoryRequirements	requirements	= vk::getBufferMemoryRequirements(vkd, device, buffer);
	vk::Move<vk::VkDeviceMemory>	memory			= dedicated
													? importDedicatedMemory(vkd, device, buffer, requirements, externalType, exportedMemoryTypeIndex, nativeHandle)
													: importMemory(vkd, device, requirements, externalType, exportedMemoryTypeIndex, nativeHandle);

	VK_CHECK(vkd.bindBufferMemory(device, buffer, *memory, 0u));

	return de::MovePtr<vk::Allocation>(new SimpleAllocation(vkd, device, memory.disown()));
}

de::MovePtr<vk::Allocation> importAndBindMemory (const vk::DeviceInterface&					vkd,
												 vk::VkDevice								device,
												 vk::VkImage								image,
												 NativeHandle&								nativeHandle,
												 vk::VkExternalMemoryHandleTypeFlagBits	externalType,
												 deUint32									exportedMemoryTypeIndex,
												 bool										dedicated)
{
	const vk::VkMemoryRequirements	requirements	= vk::getImageMemoryRequirements(vkd, device, image);
	vk::Move<vk::VkDeviceMemory>	memory			= dedicated
													? importDedicatedMemory(vkd, device, image, requirements, externalType, exportedMemoryTypeIndex, nativeHandle)
													: importMemory(vkd, device, requirements, externalType, exportedMemoryTypeIndex, nativeHandle);
	VK_CHECK(vkd.bindImageMemory(device, image, *memory, 0u));

	return de::MovePtr<vk::Allocation>(new SimpleAllocation(vkd, device, memory.disown()));
}

de::MovePtr<Resource> importResource (const vk::DeviceInterface&				vkd,
									  vk::VkDevice								device,
									  const ResourceDescription&				resourceDesc,
									  const std::vector<deUint32>&				queueFamilyIndices,
									  const OperationSupport&					readOp,
									  const OperationSupport&					writeOp,
									  NativeHandle&								nativeHandle,
									  vk::VkExternalMemoryHandleTypeFlagBits	externalType,
									  deUint32									exportedMemoryTypeIndex,
									  bool										dedicated)
{
	if (resourceDesc.type == RESOURCE_TYPE_IMAGE)
	{
		const vk::VkExtent3D				extent					=
		{
			(deUint32)resourceDesc.size.x(),
			de::max(1u, (deUint32)resourceDesc.size.y()),
			de::max(1u, (deUint32)resourceDesc.size.z())
		};
		const vk::VkImageSubresourceRange	subresourceRange		=
		{
			resourceDesc.imageAspect,
			0u,
			1u,
			0u,
			1u
		};
		const vk::VkImageSubresourceLayers	subresourceLayers		=
		{
			resourceDesc.imageAspect,
			0u,
			0u,
			1u
		};
		const vk:: VkExternalMemoryImageCreateInfo externalInfo =
		{
			vk::VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
			DE_NULL,
			(vk::VkExternalMemoryHandleTypeFlags)externalType
		};
		const vk::VkImageCreateInfo			createInfo				=
		{
			vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			&externalInfo,
			0u,

			resourceDesc.imageType,
			resourceDesc.imageFormat,
			extent,
			1u,
			1u,
			vk::VK_SAMPLE_COUNT_1_BIT,
			vk::VK_IMAGE_TILING_OPTIMAL,
			readOp.getInResourceUsageFlags() | writeOp.getOutResourceUsageFlags(),
			vk::VK_SHARING_MODE_EXCLUSIVE,

			(deUint32)queueFamilyIndices.size(),
			&queueFamilyIndices[0],
			vk::VK_IMAGE_LAYOUT_UNDEFINED
		};

		vk::Move<vk::VkImage>			image		= vk::createImage(vkd, device, &createInfo);
		de::MovePtr<vk::Allocation>		allocation	= importAndBindMemory(vkd, device, *image, nativeHandle, externalType, exportedMemoryTypeIndex, dedicated);

		return de::MovePtr<Resource>(new Resource(image, allocation, extent, resourceDesc.imageType, resourceDesc.imageFormat, subresourceRange, subresourceLayers));
	}
	else
	{
		const vk::VkDeviceSize							offset			= 0u;
		const vk::VkDeviceSize							size			= static_cast<vk::VkDeviceSize>(resourceDesc.size.x());
		const vk::VkBufferUsageFlags					usage			= readOp.getInResourceUsageFlags() | writeOp.getOutResourceUsageFlags();
		const vk:: VkExternalMemoryBufferCreateInfo	externalInfo	=
		{
			vk::VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO,
			DE_NULL,
			(vk::VkExternalMemoryHandleTypeFlags)externalType
		};
		const vk::VkBufferCreateInfo					createInfo		=
		{
			vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			&externalInfo,
			0u,

			size,
			usage,
			vk::VK_SHARING_MODE_EXCLUSIVE,
			(deUint32)queueFamilyIndices.size(),
			&queueFamilyIndices[0]
		};
		vk::Move<vk::VkBuffer>		buffer		= vk::createBuffer(vkd, device, &createInfo);
		de::MovePtr<vk::Allocation>	allocation	= importAndBindMemory(vkd, device, *buffer, nativeHandle, externalType, exportedMemoryTypeIndex, dedicated);

		return de::MovePtr<Resource>(new Resource(resourceDesc.type, buffer, allocation, offset, size));
	}
}

void recordWriteBarrier (const vk::DeviceInterface&	vkd,
						 vk::VkCommandBuffer		commandBuffer,
						 const Resource&			resource,
						 const SyncInfo&			writeSync,
						 deUint32					writeQueueFamilyIndex,
						 const SyncInfo&			readSync)
{
	const vk::VkPipelineStageFlags	srcStageMask		= writeSync.stageMask;
	const vk::VkAccessFlags			srcAccessMask		= writeSync.accessMask;

	const vk::VkPipelineStageFlags	dstStageMask		= readSync.stageMask;
	const vk::VkAccessFlags			dstAccessMask		= readSync.accessMask;

	const vk::VkDependencyFlags		dependencyFlags		= 0;

	if (resource.getType() == RESOURCE_TYPE_IMAGE)
	{
		const vk::VkImageMemoryBarrier	barrier				=
		{
			vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			DE_NULL,

			srcAccessMask,
			dstAccessMask,

			writeSync.imageLayout,
			readSync.imageLayout,

			writeQueueFamilyIndex,
			VK_QUEUE_FAMILY_EXTERNAL,

			resource.getImage().handle,
			resource.getImage().subresourceRange
		};

		vkd.cmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, dependencyFlags, 0u, (const vk::VkMemoryBarrier*)DE_NULL, 0u, (const vk::VkBufferMemoryBarrier*)DE_NULL, 1u, (const vk::VkImageMemoryBarrier*)&barrier);
	}
	else
	{
		const vk::VkBufferMemoryBarrier	barrier				=
		{
			vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
			DE_NULL,

			srcAccessMask,
			dstAccessMask,

			writeQueueFamilyIndex,
			VK_QUEUE_FAMILY_EXTERNAL,

			resource.getBuffer().handle,
			0u,
			VK_WHOLE_SIZE
		};

		vkd.cmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, dependencyFlags, 0u, (const vk::VkMemoryBarrier*)DE_NULL, 1u, (const vk::VkBufferMemoryBarrier*)&barrier, 0u, (const vk::VkImageMemoryBarrier*)DE_NULL);
	}
}

void recordReadBarrier (const vk::DeviceInterface&	vkd,
						vk::VkCommandBuffer			commandBuffer,
						const Resource&				resource,
						const SyncInfo&				writeSync,
						const SyncInfo&				readSync,
						deUint32					readQueueFamilyIndex)
{
	const vk::VkPipelineStageFlags	srcStageMask		= readSync.stageMask;
	const vk::VkAccessFlags			srcAccessMask		= readSync.accessMask;

	const vk::VkPipelineStageFlags	dstStageMask		= readSync.stageMask;
	const vk::VkAccessFlags			dstAccessMask		= readSync.accessMask;

	const vk::VkDependencyFlags		dependencyFlags		= 0;

	if (resource.getType() == RESOURCE_TYPE_IMAGE)
	{
		const vk::VkImageMemoryBarrier	barrier				=
		{
			vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			DE_NULL,

			srcAccessMask,
			dstAccessMask,

			writeSync.imageLayout,
			readSync.imageLayout,

			VK_QUEUE_FAMILY_EXTERNAL,
			readQueueFamilyIndex,

			resource.getImage().handle,
			resource.getImage().subresourceRange
		};

		vkd.cmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, dependencyFlags, 0u, (const vk::VkMemoryBarrier*)DE_NULL, 0u, (const vk::VkBufferMemoryBarrier*)DE_NULL, 1u, (const vk::VkImageMemoryBarrier*)&barrier);
	}
	else
	{
		const vk::VkBufferMemoryBarrier	barrier				=
		{
			vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
			DE_NULL,

			srcAccessMask,
			dstAccessMask,

			VK_QUEUE_FAMILY_EXTERNAL,
			readQueueFamilyIndex,

			resource.getBuffer().handle,
			0u,
			VK_WHOLE_SIZE
		};

		vkd.cmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, dependencyFlags, 0u, (const vk::VkMemoryBarrier*)DE_NULL, 1u, (const vk::VkBufferMemoryBarrier*)&barrier, 0u, (const vk::VkImageMemoryBarrier*)DE_NULL);
	}
}

std::vector<deUint32> getFamilyIndices (const std::vector<vk::VkQueueFamilyProperties>& properties)
{
	std::vector<deUint32> indices (properties.size(), 0);

	for (deUint32 ndx = 0; ndx < properties.size(); ndx++)
		indices[ndx] = ndx;

	return indices;
}

class SharingTestInstance : public TestInstance
{
public:
														SharingTestInstance		(Context&	context,
																				 TestConfig	config);

	virtual tcu::TestStatus								iterate					(void);

private:
	const TestConfig									m_config;

	const de::UniquePtr<OperationSupport>				m_supportWriteOp;
	const de::UniquePtr<OperationSupport>				m_supportReadOp;
	const NotSupportedChecker							m_notSupportedChecker; // Must declare before VkInstance to effectively reduce runtimes!

	const bool											m_getMemReq2Supported;

	const vk::VkInstance								m_instanceA;
	const vk::InstanceDriver&							m_vkiA;
	const vk::VkPhysicalDevice							m_physicalDeviceA;
	const std::vector<vk::VkQueueFamilyProperties>		m_queueFamiliesA;
	const std::vector<deUint32>							m_queueFamilyIndicesA;
	const vk::Unique<vk::VkDevice>&						m_deviceA;
	const vk::DeviceDriver								m_vkdA;

	const vk::VkInstance								m_instanceB;
	const vk::InstanceDriver&							m_vkiB;
	const vk::VkPhysicalDevice							m_physicalDeviceB;
	const std::vector<vk::VkQueueFamilyProperties>		m_queueFamiliesB;
	const std::vector<deUint32>							m_queueFamilyIndicesB;
	const vk::Unique<vk::VkDevice>&						m_deviceB;
	const vk::DeviceDriver								m_vkdB;

	const vk::VkExternalSemaphoreHandleTypeFlagBits		m_semaphoreHandleType;
	const vk::VkExternalMemoryHandleTypeFlagBits		m_memoryHandleType;

	// \todo Should this be moved to the group same way as in the other tests?
	PipelineCacheData									m_pipelineCacheData;
	tcu::ResultCollector								m_resultCollector;
	size_t												m_queueANdx;
	size_t												m_queueBNdx;
};

SharingTestInstance::SharingTestInstance (Context&		context,
										  TestConfig	config)
	: TestInstance				(context)
	, m_config					(config)
	, m_supportWriteOp			(makeOperationSupport(config.writeOp, config.resource))
	, m_supportReadOp			(makeOperationSupport(config.readOp, config.resource))
	, m_notSupportedChecker		(context, m_config, *m_supportWriteOp, *m_supportReadOp)
	, m_getMemReq2Supported		(context.isDeviceFunctionalitySupported("VK_KHR_get_memory_requirements2"))

	, m_instanceA				(InstanceAndDevice::getInstanceA(context))
	, m_vkiA					(InstanceAndDevice::getDriverA())
	, m_physicalDeviceA			(InstanceAndDevice::getPhysicalDeviceA())
	, m_queueFamiliesA			(vk::getPhysicalDeviceQueueFamilyProperties(m_vkiA, m_physicalDeviceA))
	, m_queueFamilyIndicesA		(getFamilyIndices(m_queueFamiliesA))
	, m_deviceA					(InstanceAndDevice::getDeviceA())
	, m_vkdA					(context.getPlatformInterface(), m_instanceA, *m_deviceA)

	, m_instanceB				(InstanceAndDevice::getInstanceB(context))
	, m_vkiB					(InstanceAndDevice::getDriverB())
	, m_physicalDeviceB			(InstanceAndDevice::getPhysicalDeviceB())
	, m_queueFamiliesB			(vk::getPhysicalDeviceQueueFamilyProperties(m_vkiB, m_physicalDeviceB))
	, m_queueFamilyIndicesB		(getFamilyIndices(m_queueFamiliesB))
	, m_deviceB					(InstanceAndDevice::getDeviceB())
	, m_vkdB					(context.getPlatformInterface(), m_instanceB, *m_deviceB)

	, m_semaphoreHandleType		(m_config.semaphoreHandleType)
	, m_memoryHandleType		(m_config.memoryHandleType)

	, m_resultCollector			(context.getTestContext().getLog())
	, m_queueANdx				(0)
	, m_queueBNdx				(0)
{
}

tcu::TestStatus SharingTestInstance::iterate (void)
{
	TestLog&								log					(m_context.getTestContext().getLog());

	try
	{
		const deUint32							queueFamilyA		= (deUint32)m_queueANdx;
		const deUint32							queueFamilyB		= (deUint32)m_queueBNdx;

		const tcu::ScopedLogSection				queuePairSection	(log,
																	"WriteQueue-" + de::toString(queueFamilyA) + "-ReadQueue-" + de::toString(queueFamilyB),
																	"WriteQueue-" + de::toString(queueFamilyA) + "-ReadQueue-" + de::toString(queueFamilyB));

		const vk::Unique<vk::VkSemaphore>		semaphoreA			(createExportableSemaphoreType(m_vkdA, *m_deviceA, m_config.semaphoreType, m_semaphoreHandleType));
		const vk::Unique<vk::VkSemaphore>		semaphoreB			(createSemaphoreType(m_vkdB, *m_deviceB, m_config.semaphoreType));

		const ResourceDescription&				resourceDesc		= m_config.resource;
		de::MovePtr<Resource>					resourceA;

		deUint32 exportedMemoryTypeIndex = ~0U;
		if (resourceDesc.type == RESOURCE_TYPE_IMAGE)
		{
			const vk::VkExtent3D				extent =
			{
				(deUint32)resourceDesc.size.x(),
				de::max(1u, (deUint32)resourceDesc.size.y()),
				de::max(1u, (deUint32)resourceDesc.size.z())
			};
			const vk::VkImageSubresourceRange	subresourceRange =
			{
				resourceDesc.imageAspect,
				0u,
				1u,
				0u,
				1u
			};
			const vk::VkImageSubresourceLayers	subresourceLayers =
			{
				resourceDesc.imageAspect,
				0u,
				0u,
				1u
			};

			vk::Move<vk::VkImage>			image					= createImage(m_vkdA, *m_deviceA, resourceDesc, extent, m_queueFamilyIndicesA,
																				  *m_supportReadOp, *m_supportWriteOp, m_memoryHandleType);
			const vk::VkMemoryRequirements	requirements			= getMemoryRequirements(m_vkdA, *m_deviceA, *image, m_config.dedicated, m_getMemReq2Supported);
											exportedMemoryTypeIndex = chooseMemoryType(requirements.memoryTypeBits);
			vk::Move<vk::VkDeviceMemory>	memory					= allocateExportableMemory(m_vkdA, *m_deviceA, requirements.size, exportedMemoryTypeIndex, m_memoryHandleType, m_config.dedicated ? *image : (vk::VkImage)0);

			VK_CHECK(m_vkdA.bindImageMemory(*m_deviceA, *image, *memory, 0u));

			de::MovePtr<vk::Allocation> allocation = de::MovePtr<vk::Allocation>(new SimpleAllocation(m_vkdA, *m_deviceA, memory.disown()));
			resourceA = de::MovePtr<Resource>(new Resource(image, allocation, extent, resourceDesc.imageType, resourceDesc.imageFormat, subresourceRange, subresourceLayers));
		}
		else
		{
			const vk::VkDeviceSize				offset					= 0u;
			const vk::VkDeviceSize				size					= static_cast<vk::VkDeviceSize>(resourceDesc.size.x());
			const vk::VkBufferUsageFlags		usage					= m_supportReadOp->getInResourceUsageFlags() | m_supportWriteOp->getOutResourceUsageFlags();
			vk::Move<vk::VkBuffer>				buffer					= createBuffer(m_vkdA, *m_deviceA, size, usage, m_memoryHandleType, m_queueFamilyIndicesA);
			const vk::VkMemoryRequirements		requirements			= getMemoryRequirements(m_vkdA, *m_deviceA, *buffer, m_config.dedicated, m_getMemReq2Supported);
												exportedMemoryTypeIndex = chooseMemoryType(requirements.memoryTypeBits);
			vk::Move<vk::VkDeviceMemory>		memory					= allocateExportableMemory(m_vkdA, *m_deviceA, requirements.size, exportedMemoryTypeIndex, m_memoryHandleType, m_config.dedicated ? *buffer : (vk::VkBuffer)0);

			VK_CHECK(m_vkdA.bindBufferMemory(*m_deviceA, *buffer, *memory, 0u));

			de::MovePtr<vk::Allocation> allocation = de::MovePtr<vk::Allocation>(new SimpleAllocation(m_vkdA, *m_deviceA, memory.disown()));
			resourceA = de::MovePtr<Resource>(new Resource(resourceDesc.type, buffer, allocation, offset, size));
		}

		NativeHandle							nativeMemoryHandle;
		getMemoryNative(m_vkdA, *m_deviceA, resourceA->getMemory(), m_memoryHandleType, nativeMemoryHandle);

		const de::UniquePtr<Resource>			resourceB			(importResource(m_vkdB, *m_deviceB, resourceDesc, m_queueFamilyIndicesB, *m_supportReadOp, *m_supportWriteOp, nativeMemoryHandle, m_memoryHandleType, exportedMemoryTypeIndex, m_config.dedicated));


		const vk::VkQueue						queueA				(getQueue(m_vkdA, *m_deviceA, queueFamilyA));
		const vk::Unique<vk::VkCommandPool>		commandPoolA		(createCommandPool(m_vkdA, *m_deviceA, queueFamilyA));
		const vk::Unique<vk::VkCommandBuffer>	commandBufferA		(createCommandBuffer(m_vkdA, *m_deviceA, *commandPoolA));
		vk::SimpleAllocator						allocatorA			(m_vkdA, *m_deviceA, vk::getPhysicalDeviceMemoryProperties(m_vkiA, m_physicalDeviceA));
		OperationContext						operationContextA	(m_context, m_vkiA, m_vkdA, m_physicalDeviceA, *m_deviceA, allocatorA, m_context.getBinaryCollection(), m_pipelineCacheData);

		if (!checkQueueFlags(m_queueFamiliesA[m_queueANdx].queueFlags , m_supportWriteOp->getQueueFlags(operationContextA)))
			TCU_THROW(NotSupportedError, "Operation not supported by the source queue");

		const vk::VkQueue						queueB				(getQueue(m_vkdB, *m_deviceB, queueFamilyB));
		const vk::Unique<vk::VkCommandPool>		commandPoolB		(createCommandPool(m_vkdB, *m_deviceB, queueFamilyB));
		const vk::Unique<vk::VkCommandBuffer>	commandBufferB		(createCommandBuffer(m_vkdB, *m_deviceB, *commandPoolB));
		vk::SimpleAllocator						allocatorB			(m_vkdB, *m_deviceB, vk::getPhysicalDeviceMemoryProperties(m_vkiB, m_physicalDeviceB));
		OperationContext						operationContextB	(m_context, m_vkiB, m_vkdB, m_physicalDeviceB, *m_deviceB, allocatorB, m_context.getBinaryCollection(), m_pipelineCacheData);

		if (!checkQueueFlags(m_queueFamiliesB[m_queueBNdx].queueFlags , m_supportReadOp->getQueueFlags(operationContextB)))
			TCU_THROW(NotSupportedError, "Operation not supported by the destination queue");

		const de::UniquePtr<Operation>			writeOp				(m_supportWriteOp->build(operationContextA, *resourceA));
		const de::UniquePtr<Operation>			readOp				(m_supportReadOp->build(operationContextB, *resourceB));

		const SyncInfo							writeSync			= writeOp->getOutSyncInfo();
		const SyncInfo							readSync			= readOp->getInSyncInfo();

		beginCommandBuffer(m_vkdA, *commandBufferA);
		writeOp->recordCommands(*commandBufferA);
		recordWriteBarrier(m_vkdA, *commandBufferA, *resourceA, writeSync, queueFamilyA, readSync);
		endCommandBuffer(m_vkdA, *commandBufferA);

		beginCommandBuffer(m_vkdB, *commandBufferB);
		recordReadBarrier(m_vkdB, *commandBufferB, *resourceB, writeSync, readSync, queueFamilyB);
		readOp->recordCommands(*commandBufferB);
		endCommandBuffer(m_vkdB, *commandBufferB);

		{
			de::Random									rng							(1234);
			const deUint64								timelineValue				= rng.getInt(1, deIntMaxValue32(32));
			const vk::VkCommandBuffer					commandBuffer				= *commandBufferA;
			const vk::VkSemaphore						semaphore					= *semaphoreA;
			const vk::VkTimelineSemaphoreSubmitInfo		semaphoreSubmitInfo	=
			{
				vk::VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,		// VkStructureType	sType;
				DE_NULL,													// const void*		pNext;
				0u,															// deUint32			waitSemaphoreValueCount
				DE_NULL,													// const deUint64*	pWaitSemaphoreValues
				1u,															// deUint32			signalSemaphoreValueCount
				&timelineValue,												// const deUint64*	pSignalSemaphoreValues
			};
			const vk::VkSubmitInfo						submitInfo					=
			{
				vk::VK_STRUCTURE_TYPE_SUBMIT_INFO,
				m_config.semaphoreType == vk::VK_SEMAPHORE_TYPE_TIMELINE_KHR ? &semaphoreSubmitInfo : DE_NULL,
				0u,
				DE_NULL,
				DE_NULL,
				1u,
				&commandBuffer,
				1u,
				&semaphore
			};

			VK_CHECK(m_vkdA.queueSubmit(queueA, 1u, &submitInfo, DE_NULL));

			{
				NativeHandle	nativeSemaphoreHandle;

				getSemaphoreNative(m_vkdA, *m_deviceA, *semaphoreA, m_semaphoreHandleType, nativeSemaphoreHandle);
				importSemaphore(m_vkdB, *m_deviceB, *semaphoreB, m_semaphoreHandleType, nativeSemaphoreHandle, 0u);
			}
		}
		{
			const deUint64								timelineValue			= 1u;
			const vk::VkCommandBuffer					commandBuffer			= *commandBufferB;
			const vk::VkSemaphore						semaphore				= *semaphoreB;
			const vk::VkPipelineStageFlags				dstStage				= readSync.stageMask;
			const vk::VkTimelineSemaphoreSubmitInfo		semaphoreSubmitInfo		=
			{
				vk::VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,		// VkStructureType	sType;
				DE_NULL,													// const void*		pNext;
				1u,															// deUint32			waitSemaphoreValueCount
				&timelineValue,												// const deUint64*	pWaitSemaphoreValues
				0u,															// deUint32			signalSemaphoreValueCount
				DE_NULL,													// const deUint64*	pSignalSemaphoreValues
			};
			const vk::VkSubmitInfo						submitInfo				=
			{
				vk::VK_STRUCTURE_TYPE_SUBMIT_INFO,
				m_config.semaphoreType == vk::VK_SEMAPHORE_TYPE_TIMELINE_KHR ? &semaphoreSubmitInfo : DE_NULL,

				1u,
				&semaphore,
				&dstStage,

				1u,
				&commandBuffer,
				0u,
				DE_NULL,
			};

			VK_CHECK(m_vkdB.queueSubmit(queueB, 1u, &submitInfo, DE_NULL));
		}

		VK_CHECK(m_vkdA.queueWaitIdle(queueA));
		VK_CHECK(m_vkdB.queueWaitIdle(queueB));

		if (m_config.semaphoreType == vk::VK_SEMAPHORE_TYPE_TIMELINE_KHR)
		{
			deUint64	valueA;
			deUint64	valueB;

			VK_CHECK(m_vkdA.getSemaphoreCounterValue(*m_deviceA, *semaphoreA, &valueA));
			VK_CHECK(m_vkdB.getSemaphoreCounterValue(*m_deviceB, *semaphoreB, &valueB));

			if (valueA != valueB)
				return tcu::TestStatus::fail("Inconsistent values between shared semaphores");
		}

		{
			const Data	expected	= writeOp->getData();
			const Data	actual		= readOp->getData();

			DE_ASSERT(expected.size == actual.size);

			if (!isIndirectBuffer(m_config.resource.type))
			{
				if (0 != deMemCmp(expected.data, actual.data, expected.size))
				{
					const size_t		maxBytesLogged = 256;
					std::ostringstream	expectedData;
					std::ostringstream	actualData;
					size_t				byteNdx = 0;

					// Find first byte difference
					for (; actual.data[byteNdx] == expected.data[byteNdx]; byteNdx++)
					{
						// Nothing
					}

					log << TestLog::Message << "First different byte at offset: " << byteNdx << TestLog::EndMessage;

					// Log 8 previous bytes before the first incorrect byte
					if (byteNdx > 8)
					{
						expectedData << "... ";
						actualData << "... ";

						byteNdx -= 8;
					}
					else
						byteNdx = 0;

					for (size_t i = 0; i < maxBytesLogged && byteNdx < expected.size; i++, byteNdx++)
					{
						expectedData << (i > 0 ? ", " : "") << (deUint32)expected.data[byteNdx];
						actualData << (i > 0 ? ", " : "") << (deUint32)actual.data[byteNdx];
					}

					if (expected.size > byteNdx)
					{
						expectedData << "...";
						actualData << "...";
					}

					log << TestLog::Message << "Expected data: (" << expectedData.str() << ")" << TestLog::EndMessage;
					log << TestLog::Message << "Actual data: (" << actualData.str() << ")" << TestLog::EndMessage;

					m_resultCollector.fail("Memory contents don't match");
				}
			}
			else
			{
				const deUint32 expectedValue = reinterpret_cast<const deUint32*>(expected.data)[0];
				const deUint32 actualValue   = reinterpret_cast<const deUint32*>(actual.data)[0];

				if (actualValue < expectedValue)
				{
					log << TestLog::Message << "Expected counter value: (" << expectedValue << ")" << TestLog::EndMessage;
					log << TestLog::Message << "Actual counter value: (" << actualValue << ")" << TestLog::EndMessage;

					m_resultCollector.fail("Counter value is smaller than expected");
				}
			}
		}
	}
	catch (const tcu::NotSupportedError& error)
	{
		log << TestLog::Message << "Not supported: " << error.getMessage() << TestLog::EndMessage;
	}
	catch (const tcu::TestError& error)
	{
		m_resultCollector.fail(std::string("Exception: ") + error.getMessage());
	}

	// Collect possible validation errors.
	InstanceAndDevice::collectMessagesA();
	InstanceAndDevice::collectMessagesB();

	// Move to next queue
	{
		m_queueBNdx++;

		if (m_queueBNdx >= m_queueFamiliesB.size())
		{
			m_queueANdx++;

			if (m_queueANdx >= m_queueFamiliesA.size())
			{
				return tcu::TestStatus(m_resultCollector.getResult(), m_resultCollector.getMessage());
			}
			else
			{
				m_queueBNdx = 0;

				return tcu::TestStatus::incomplete();
			}
		}
		else
			return tcu::TestStatus::incomplete();
	}
}

struct Progs
{
	void init (vk::SourceCollections& dst, TestConfig config) const
	{
		const de::UniquePtr<OperationSupport>	readOp	(makeOperationSupport(config.readOp, config.resource));
		const de::UniquePtr<OperationSupport>	writeOp	(makeOperationSupport(config.writeOp, config.resource));

		readOp->initPrograms(dst);
		writeOp->initPrograms(dst);
	}
};

} // anonymous

static void createTests (tcu::TestCaseGroup* group)
{
	tcu::TestContext& testCtx = group->getTestContext();
	const struct
	{
		vk::VkExternalMemoryHandleTypeFlagBits		memoryType;
		vk::VkExternalSemaphoreHandleTypeFlagBits	semaphoreType;
		const char*									nameSuffix;
	} cases[] =
	{
		{
			vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
			vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT,
			"_fd"
		},
		{
			vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
			vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT,
			"_fence_fd"
		},
		{
			vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT,
			vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT,
			"_win32_kmt"
		},
		{
			vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT,
			vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT,
			"_win32"
		},
		{
			vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
			vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT,
			"_dma_buf"
		},
	};

	const std::string semaphoreNames[vk::VK_SEMAPHORE_TYPE_LAST] =
	{
		"_binary_semaphore",
		"_timeline_semaphore",
	};

	for (size_t dedicatedNdx = 0; dedicatedNdx < 2; dedicatedNdx++)
	{
		const bool						dedicated		(dedicatedNdx == 1);
		de::MovePtr<tcu::TestCaseGroup>	dedicatedGroup	(new tcu::TestCaseGroup(testCtx, dedicated ? "dedicated" : "suballocated", ""));

		for (size_t writeOpNdx = 0; writeOpNdx < DE_LENGTH_OF_ARRAY(s_writeOps); ++writeOpNdx)
		for (size_t readOpNdx = 0; readOpNdx < DE_LENGTH_OF_ARRAY(s_readOps); ++readOpNdx)
		{
			const OperationName	writeOp		= s_writeOps[writeOpNdx];
			const OperationName	readOp		= s_readOps[readOpNdx];
			const std::string	opGroupName	= getOperationName(writeOp) + "_" + getOperationName(readOp);
			bool				empty		= true;

			de::MovePtr<tcu::TestCaseGroup> opGroup	(new tcu::TestCaseGroup(testCtx, opGroupName.c_str(), ""));

			for (size_t resourceNdx = 0; resourceNdx < DE_LENGTH_OF_ARRAY(s_resources); ++resourceNdx)
			{
				const ResourceDescription&	resource	= s_resources[resourceNdx];

				for (size_t caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(cases); caseNdx++)
				{
					for (int semaphoreType = 0; semaphoreType < vk::VK_SEMAPHORE_TYPE_LAST; semaphoreType++)
					{
						if (isResourceSupported(writeOp, resource) && isResourceSupported(readOp, resource))
						{
							const TestConfig	config (resource, (vk::VkSemaphoreType)semaphoreType, writeOp, readOp, cases[caseNdx].memoryType, cases[caseNdx].semaphoreType, dedicated);
							std::string			name	= getResourceName(resource) + semaphoreNames[semaphoreType] + cases[caseNdx].nameSuffix;

							opGroup->addChild(new InstanceFactory1<SharingTestInstance, TestConfig, Progs>(testCtx, tcu::NODETYPE_SELF_VALIDATE,  name, "", Progs(), config));
							empty = false;
						}
					}
				}
			}

			if (!empty)
				dedicatedGroup->addChild(opGroup.release());
		}

		group->addChild(dedicatedGroup.release());
	}
}

static void cleanupGroup (tcu::TestCaseGroup* group)
{
	DE_UNREF(group);
	// Destroy singleton object
	InstanceAndDevice::destroy();
}

tcu::TestCaseGroup* createCrossInstanceSharingTest (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "cross_instance", "", createTests, cleanupGroup);
}

} // synchronization
} // vkt
