/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 Google Inc.
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
 * \brief Vulkan external memory API tests
 *//*--------------------------------------------------------------------*/

#include "vktApiExternalMemoryTests.hpp"

#include "vktTestCaseUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkPlatform.hpp"
#include "vkMemUtil.hpp"

#include "tcuTestLog.hpp"

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"
#include "deRandom.hpp"

#include "deMemory.h"

#include "vktExternalMemoryUtil.hpp"

#if (DE_OS == DE_OS_ANDROID) || (DE_OS == DE_OS_UNIX)
#	include <unistd.h>
#	include <fcntl.h>
#	include <errno.h>
#	include <sys/types.h>
#	include <sys/socket.h>
#endif

#if (DE_OS == DE_OS_WIN32)
#	define WIN32_LEAN_AND_MEAN
#	include <windows.h>
#endif

using tcu::TestLog;
using namespace vkt::ExternalMemoryUtil;

namespace vkt
{
namespace api
{
namespace
{
void writeHostMemory (const vk::DeviceInterface&	vkd,
					  vk::VkDevice					device,
					  vk::VkDeviceMemory			memory,
					  size_t						size,
					  const void*					data)
{
	void* const ptr = vk::mapMemory(vkd, device, memory, 0, size, 0);

	deMemcpy(ptr, data, size);

	vkd.unmapMemory(device, memory);
}

void checkHostMemory (const vk::DeviceInterface&	vkd,
					  vk::VkDevice					device,
					  vk::VkDeviceMemory			memory,
					  size_t						size,
					  const void*					data)
{
	void* const ptr = vk::mapMemory(vkd, device, memory, 0, size, 0);

	if (deMemCmp(ptr, data, size) != 0)
		TCU_FAIL("Memory contents don't match");

	vkd.unmapMemory(device, memory);
}

std::vector<deUint8> genTestData (deUint32 seed, size_t size)
{
	de::Random				rng		(seed);
	std::vector<deUint8>	data	(size);

	for (size_t ndx = 0; ndx < size; ndx++)
	{
		data[ndx] = rng.getUint8();
	}

	return data;
}

deUint32 chooseQueueFamilyIndex (const vk::InstanceInterface&	vki,
								 vk::VkPhysicalDevice			device,
								 vk::VkQueueFlags				requireFlags)
{
	const std::vector<vk::VkQueueFamilyProperties> properties (vk::getPhysicalDeviceQueueFamilyProperties(vki, device));

	for (deUint32 queueFamilyIndex = 0; queueFamilyIndex < (deUint32)properties.size(); queueFamilyIndex++)
	{
		if ((properties[queueFamilyIndex].queueFlags & requireFlags) == requireFlags)
			return queueFamilyIndex;
	}

	TCU_THROW(NotSupportedError, "Queue type not supported");
}

vk::Move<vk::VkInstance> createInstance (const vk::PlatformInterface&						vkp,
										 const vk::VkExternalSemaphoreHandleTypeFlagsKHX	externalSemaphoreTypes,
										 const vk::VkExternalMemoryHandleTypeFlagsKHX		externalMemoryTypes)
{
	std::vector<std::string> instanceExtensions;

	instanceExtensions.push_back("VK_KHR_get_physical_device_properties2");

	if (externalSemaphoreTypes != 0)
		instanceExtensions.push_back("VK_KHX_external_semaphore_capabilities");

	if (externalMemoryTypes != 0)
		instanceExtensions.push_back("VK_KHX_external_memory_capabilities");

	try
	{
		return vk::createDefaultInstance(vkp, std::vector<std::string>(), instanceExtensions);
	}
	catch (const vk::Error& error)
	{
		if (error.getError() == vk::VK_ERROR_EXTENSION_NOT_PRESENT)
			TCU_THROW(NotSupportedError, "Required extensions not supported");

		throw;
	}
}

vk::Move<vk::VkDevice> createDevice (const vk::InstanceInterface&						vki,
									 vk::VkPhysicalDevice								physicalDevice,
									 const vk::VkExternalSemaphoreHandleTypeFlagsKHX	externalSemaphoreTypes,
									 const vk::VkExternalMemoryHandleTypeFlagsKHX		externalMemoryTypes,
									 deUint32											queueFamilyIndex)
{
	std::vector<const char*>	deviceExtensions;

	if ((externalSemaphoreTypes
			& (vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_FENCE_FD_BIT_KHX
				| vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT_KHX)) != 0)
	{
		deviceExtensions.push_back("VK_KHX_external_semaphore_fd");
	}

	if ((externalMemoryTypes
			& vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHX) != 0)
	{
		deviceExtensions.push_back("VK_KHX_external_memory_fd");
	}

	if ((externalSemaphoreTypes
			& (vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHX
				| vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT_KHX
				| vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT_KHX)) != 0)
	{
		deviceExtensions.push_back("VK_KHX_external_semaphore_win32");
	}

	if ((externalMemoryTypes
			& (vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHX
			   | vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT_KHX
			   | vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT_KHX
			   | vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT_KHX
			   | vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_HEAP_BIT_KHX
			   | vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT_KHX)) != 0)
	{
		deviceExtensions.push_back("VK_KHX_external_memory_win32");
	}

	const float								priority				= 0.5f;
	const vk::VkDeviceQueueCreateInfo		queues[]				=
	{
		{
			vk::VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			DE_NULL,
			0u,

			queueFamilyIndex,
			1u,
			&priority
		}
	};
	const vk::VkDeviceCreateInfo			deviceCreateInfo		=
	{
		vk::VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		DE_NULL,
		0u,

		DE_LENGTH_OF_ARRAY(queues),
		queues,

		0u,
		DE_NULL,

		(deUint32)deviceExtensions.size(),
		deviceExtensions.empty() ? DE_NULL : &deviceExtensions[0],
		DE_NULL
	};

	try
	{
		return vk::createDevice(vki, physicalDevice, &deviceCreateInfo);
	}
	catch (const vk::Error& error)
	{
		if (error.getError() == vk::VK_ERROR_EXTENSION_NOT_PRESENT)
			TCU_THROW(NotSupportedError, "Required extensions not supported");

		throw;
	}
}

vk::VkQueue getQueue (const vk::DeviceInterface&	vkd,
					  vk::VkDevice					device,
					  deUint32						queueFamilyIndex)
{
	vk::VkQueue queue;

	vkd.getDeviceQueue(device, queueFamilyIndex, 0, &queue);

	return queue;
}

void checkSemaphoreSupport (const vk::InstanceInterface&					vki,
							vk::VkPhysicalDevice							device,
							vk::VkExternalSemaphoreHandleTypeFlagBitsKHX	externalType)
{
	const vk::VkPhysicalDeviceExternalSemaphoreInfoKHX	info			=
	{
		vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO_KHX,
		DE_NULL,
		externalType
	};
	vk::VkExternalSemaphorePropertiesKHX				properties	=
	{
		vk::VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES_KHX,
		DE_NULL,
		0u,
		0u,
		0u
	};

	vki.getPhysicalDeviceExternalSemaphorePropertiesKHX(device, &info, &properties);

	if ((properties.externalSemaphoreFeatures & vk::VK_EXTERNAL_SEMAPHORE_FEATURE_EXPORTABLE_BIT_KHX) == 0)
		TCU_THROW(NotSupportedError, "Semaphore doesn't support exporting in external type");

	if ((properties.externalSemaphoreFeatures & vk::VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT_KHX) == 0)
		TCU_THROW(NotSupportedError, "Semaphore doesn't support exporting in external type");
}

void checkBufferSupport (const vk::InstanceInterface&				vki,
						 vk::VkPhysicalDevice						device,
						 vk::VkExternalMemoryHandleTypeFlagBitsKHX	externalType,
						 vk::VkBufferViewCreateFlags				createFlag,
						 vk::VkBufferUsageFlags						usageFlag)
{
	const vk::VkPhysicalDeviceExternalBufferInfoKHX	info			=
	{
		vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_BUFFER_INFO_KHX,
		DE_NULL,

		createFlag,
		usageFlag,
		externalType
	};
	vk::VkExternalBufferPropertiesKHX				properties		=
	{
		vk::VK_STRUCTURE_TYPE_EXTERNAL_BUFFER_PROPERTIES_KHX,
		DE_NULL,

		{ 0u, 0u, 0u }
	};

	vki.getPhysicalDeviceExternalBufferPropertiesKHX(device, &info, &properties);

	if ((properties.externalMemoryProperties.externalMemoryFeatures & vk::VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT_KHX) == 0)
		TCU_THROW(NotSupportedError, "External handle type doesn't support exporting buffer");

	if ((properties.externalMemoryProperties.externalMemoryFeatures & vk::VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT_KHX) == 0)
		TCU_THROW(NotSupportedError, "External handle type doesn't support importing buffer");
}

void checkImageSupport (const vk::InstanceInterface&				vki,
						 vk::VkPhysicalDevice						device,
						 vk::VkExternalMemoryHandleTypeFlagBitsKHX	externalType,
						 vk::VkImageViewCreateFlags					createFlag,
						 vk::VkImageUsageFlags						usageFlag,
						 vk::VkFormat								format,
						 vk::VkImageTiling							tiling)
{
	const vk::VkPhysicalDeviceExternalImageFormatInfoKHX	externalInfo	=
	{
		vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO_KHX,
		DE_NULL,
		externalType
	};
	const vk::VkPhysicalDeviceImageFormatInfo2KHR			info			=
	{
		vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2_KHR,
		&externalInfo,

		format,
		vk::VK_IMAGE_TYPE_2D,
		tiling,
		usageFlag,
		createFlag,
	};
	vk::VkExternalImageFormatPropertiesKHX					externalProperties	=
	{
		vk::VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES_KHX,
		DE_NULL,
		{ 0u, 0u, 0u }
	};
	vk::VkImageFormatProperties2KHR							properties			=
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2_KHR,
		&externalProperties,
		{
			{ 0u, 0u, 0u },
			0u,
			0u,
			0u,
			0u
		}
	};

	vki.getPhysicalDeviceImageFormatProperties2KHR(device, &info, &properties);

	if ((externalProperties.externalMemoryProperties.externalMemoryFeatures & vk::VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT_KHX) == 0)
		TCU_THROW(NotSupportedError, "External handle type doesn't support exporting image");

	if ((externalProperties.externalMemoryProperties.externalMemoryFeatures & vk::VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT_KHX) == 0)
		TCU_THROW(NotSupportedError, "External handle type doesn't support importing image");
}

void submitDummySignal (const vk::DeviceInterface&	vkd,
						vk::VkQueue					queue,
						vk::VkSemaphore				semaphore)
{
	const vk::VkSubmitInfo submit =
	{
		vk::VK_STRUCTURE_TYPE_SUBMIT_INFO,
		DE_NULL,

		0u,
		DE_NULL,
		DE_NULL,

		0u,
		DE_NULL,

		1u,
		&semaphore
	};

	VK_CHECK(vkd.queueSubmit(queue, 1, &submit, (vk::VkFence)0u));
}

void submitDummyWait (const vk::DeviceInterface&	vkd,
					  vk::VkQueue					queue,
					  vk::VkSemaphore				semaphore)
{
	const vk::VkPipelineStageFlags	stage	= vk::VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	const vk::VkSubmitInfo			submit	=
	{
		vk::VK_STRUCTURE_TYPE_SUBMIT_INFO,
		DE_NULL,

		1u,
		&semaphore,
		&stage,

		0u,
		DE_NULL,

		0u,
		DE_NULL,
	};

	VK_CHECK(vkd.queueSubmit(queue, 1, &submit, (vk::VkFence)0u));
}

tcu::TestStatus testSemaphoreQueries (Context& context, vk::VkExternalSemaphoreHandleTypeFlagBitsKHX externalType)
{
	const vk::PlatformInterface&		vkp				(context.getPlatformInterface());
	const vk::Unique<vk::VkInstance>	instance		(createInstance(vkp, externalType, 0u));
	const vk::InstanceDriver			vki				(vkp, *instance);
	const vk::VkPhysicalDevice			device			(vk::chooseDevice(vki, *instance, context.getTestContext().getCommandLine()));

	TestLog&							log				= context.getTestContext().getLog();

	const vk::VkPhysicalDeviceExternalSemaphoreInfoKHX	info			=
	{
		vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO_KHX,
		DE_NULL,
		externalType
	};
	vk::VkExternalSemaphorePropertiesKHX				properties	=
	{
		vk::VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES_KHX,
		DE_NULL,
		0u,
		0u,
		0u
	};

	vki.getPhysicalDeviceExternalSemaphorePropertiesKHX(device, &info, &properties);
	log << TestLog::Message << properties << TestLog::EndMessage;

	TCU_CHECK(properties.pNext == DE_NULL);
	TCU_CHECK(properties.sType == vk::VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES_KHX);

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus testSemaphoreImportTwice (Context& context, vk::VkExternalSemaphoreHandleTypeFlagBitsKHX externalType)
{
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const vk::Unique<vk::VkInstance>	instance			(createInstance(vkp, externalType, 0u));
	const vk::InstanceDriver			vki					(vkp, *instance);
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, *instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	checkSemaphoreSupport(vki, physicalDevice, externalType);

	{
		const vk::Unique<vk::VkDevice>		device			(createDevice(vki, physicalDevice, externalType, 0u, queueFamilyIndex));
		const vk::DeviceDriver				vkd				(vki, *device);
		const vk::VkQueue					queue			(getQueue(vkd, *device, queueFamilyIndex));

		const vk::Unique<vk::VkSemaphore>	semaphore		(createExportableSemaphore(vkd, *device, externalType));

		if (getHandleTypePermanence(externalType) == PERMANENCE_TEMPORARY)
			submitDummySignal(vkd, queue, *semaphore);

		NativeHandle						handleA;
		getSemaphoreNative(vkd, *device, *semaphore, externalType, handleA);

		{
			NativeHandle						handleB		(handleA);
			const vk::Unique<vk::VkSemaphore>	semaphoreA	(createAndImportSemaphore(vkd, *device, externalType, handleA));
			const vk::Unique<vk::VkSemaphore>	semaphoreB	(createAndImportSemaphore(vkd, *device, externalType, handleB));

			if (getHandleTypePermanence(externalType) == PERMANENCE_TEMPORARY)
				submitDummyWait(vkd, queue, *semaphoreA);
			else if (getHandleTypePermanence(externalType) == PERMANENCE_PERMANENT)
			{
				submitDummySignal(vkd, queue, *semaphoreA);
				submitDummyWait(vkd, queue, *semaphoreB);
			}
			else
				DE_FATAL("Unknow permanence.");

			VK_CHECK(vkd.queueWaitIdle(queue));
		}

		return tcu::TestStatus::pass("Pass");
	}
}

tcu::TestStatus testSemaphoreImportReimport (Context& context, vk::VkExternalSemaphoreHandleTypeFlagBitsKHX externalType)
{
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const vk::Unique<vk::VkInstance>	instance			(createInstance(vkp, externalType, 0u));
	const vk::InstanceDriver			vki					(vkp, *instance);
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, *instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	checkSemaphoreSupport(vki, physicalDevice, externalType);

	{
		const vk::Unique<vk::VkDevice>		device			(createDevice(vki, physicalDevice, externalType, 0u, queueFamilyIndex));
		const vk::DeviceDriver				vkd				(vki, *device);
		const vk::VkQueue					queue			(getQueue(vkd, *device, queueFamilyIndex));

		const vk::Unique<vk::VkSemaphore>	semaphoreA		(createExportableSemaphore(vkd, *device, externalType));

		if (getHandleTypePermanence(externalType) == PERMANENCE_TEMPORARY)
			submitDummySignal(vkd, queue, *semaphoreA);

		NativeHandle						handleA;
		getSemaphoreNative(vkd, *device, *semaphoreA, externalType, handleA);

		NativeHandle						handleB		(handleA);
		const vk::Unique<vk::VkSemaphore>	semaphoreB	(createAndImportSemaphore(vkd, *device, externalType, handleA));

		importSemaphore(vkd, *device, *semaphoreB, externalType, handleB);

		if (getHandleTypePermanence(externalType) == PERMANENCE_TEMPORARY)
			submitDummyWait(vkd, queue, *semaphoreB);
		else if (getHandleTypePermanence(externalType) == PERMANENCE_PERMANENT)
		{
			submitDummySignal(vkd, queue, *semaphoreA);
			submitDummyWait(vkd, queue, *semaphoreB);
		}
		else
			DE_FATAL("Unknow permanence.");

		VK_CHECK(vkd.queueWaitIdle(queue));

		return tcu::TestStatus::pass("Pass");
	}
}

tcu::TestStatus testSemaphoreSignalExportImportWait (Context& context, vk::VkExternalSemaphoreHandleTypeFlagBitsKHX externalType)
{
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const vk::Unique<vk::VkInstance>	instance			(createInstance(vkp, externalType, 0u));
	const vk::InstanceDriver			vki					(vkp, *instance);
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, *instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	checkSemaphoreSupport(vki, physicalDevice, externalType);

	{
		const vk::Unique<vk::VkDevice>		device				(createDevice(vki, physicalDevice, externalType, 0u, queueFamilyIndex));
		const vk::DeviceDriver				vkd					(vki, *device);
		const vk::VkQueue					queue				(getQueue(vkd, *device, queueFamilyIndex));

		const vk::Unique<vk::VkSemaphore>	semaphoreA			(createExportableSemaphore(vkd, *device, externalType));


		submitDummySignal(vkd, queue, *semaphoreA);

		{
			NativeHandle	handle;

			getSemaphoreNative(vkd, *device, *semaphoreA, externalType, handle);

			{
				const vk::Unique<vk::VkSemaphore>	semaphoreB	(createAndImportSemaphore(vkd, *device, externalType, handle));
				submitDummyWait(vkd, queue, *semaphoreB);

				VK_CHECK(vkd.queueWaitIdle(queue));
			}
		}

		return tcu::TestStatus::pass("Pass");
	}
}

tcu::TestStatus testSemaphoreExportSignalImportWait (Context& context, vk::VkExternalSemaphoreHandleTypeFlagBitsKHX externalType)
{
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const vk::Unique<vk::VkInstance>	instance			(createInstance(vkp, externalType, 0u));
	const vk::InstanceDriver			vki					(vkp, *instance);
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, *instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	DE_ASSERT(getHandleTypePermanence(externalType) == PERMANENCE_PERMANENT);
	checkSemaphoreSupport(vki, physicalDevice, externalType);

	{
		const vk::Unique<vk::VkDevice>		device				(createDevice(vki, physicalDevice, externalType, 0u, queueFamilyIndex));
		const vk::DeviceDriver				vkd					(vki, *device);
		const vk::VkQueue					queue				(getQueue(vkd, *device, queueFamilyIndex));

		const vk::Unique<vk::VkSemaphore>	semaphoreA			(createExportableSemaphore(vkd, *device, externalType));
		NativeHandle						handle;

		getSemaphoreNative(vkd, *device, *semaphoreA, externalType, handle);

		submitDummySignal(vkd, queue, *semaphoreA);
		{
			{
				const vk::Unique<vk::VkSemaphore>	semaphoreB	(createAndImportSemaphore(vkd, *device, externalType, handle));

				submitDummyWait(vkd, queue, *semaphoreB);
				VK_CHECK(vkd.queueWaitIdle(queue));
			}
		}

		return tcu::TestStatus::pass("Pass");
	}
}

tcu::TestStatus testSemaphoreExportImportSignalWait (Context& context, vk::VkExternalSemaphoreHandleTypeFlagBitsKHX externalType)
{
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const vk::Unique<vk::VkInstance>	instance			(createInstance(vkp, externalType, 0u));
	const vk::InstanceDriver			vki					(vkp, *instance);
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, *instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	DE_ASSERT(getHandleTypePermanence(externalType) == PERMANENCE_PERMANENT);
	checkSemaphoreSupport(vki, physicalDevice, externalType);

	{
		const vk::Unique<vk::VkDevice>		device				(createDevice(vki, physicalDevice, externalType, 0u, queueFamilyIndex));
		const vk::DeviceDriver				vkd					(vki, *device);
		const vk::VkQueue					queue				(getQueue(vkd, *device, queueFamilyIndex));

		const vk::Unique<vk::VkSemaphore>	semaphoreA			(createExportableSemaphore(vkd, *device, externalType));
		NativeHandle						handle;

		getSemaphoreNative(vkd, *device, *semaphoreA, externalType, handle);

		const vk::Unique<vk::VkSemaphore>	semaphoreB	(createAndImportSemaphore(vkd, *device, externalType, handle));

		submitDummySignal(vkd, queue, *semaphoreA);
		submitDummyWait(vkd, queue, *semaphoreB);

		VK_CHECK(vkd.queueWaitIdle(queue));

		return tcu::TestStatus::pass("Pass");
	}
}

tcu::TestStatus testSemaphoreSignalImport (Context& context, vk::VkExternalSemaphoreHandleTypeFlagBitsKHX externalType)
{
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const vk::Unique<vk::VkInstance>	instance			(createInstance(vkp, externalType, 0u));
	const vk::InstanceDriver			vki					(vkp, *instance);
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, *instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	checkSemaphoreSupport(vki, physicalDevice, externalType);

	{
		const vk::Unique<vk::VkDevice>		device				(createDevice(vki, physicalDevice, externalType, 0u, queueFamilyIndex));
		const vk::DeviceDriver				vkd					(vki, *device);
		const vk::VkQueue					queue				(getQueue(vkd, *device, queueFamilyIndex));

		const vk::Unique<vk::VkSemaphore>	semaphoreA			(createExportableSemaphore(vkd, *device, externalType));
		const vk::Unique<vk::VkSemaphore>	semaphoreB			(createSemaphore(vkd, *device));
		NativeHandle						handle;

		submitDummySignal(vkd, queue, *semaphoreB);
		VK_CHECK(vkd.queueWaitIdle(queue));

		if (getHandleTypePermanence(externalType) == PERMANENCE_TEMPORARY)
			submitDummySignal(vkd, queue, *semaphoreA);

		getSemaphoreNative(vkd, *device, *semaphoreA, externalType, handle);

		importSemaphore(vkd, *device, *semaphoreB, externalType, handle);

		if (getHandleTypePermanence(externalType) == PERMANENCE_TEMPORARY)
			submitDummyWait(vkd, queue, *semaphoreB);
		else if (getHandleTypePermanence(externalType) == PERMANENCE_PERMANENT)
		{
			submitDummySignal(vkd, queue, *semaphoreA);
			submitDummyWait(vkd, queue, *semaphoreB);
		}
		else
			DE_FATAL("Unknow permanence.");

		VK_CHECK(vkd.queueWaitIdle(queue));

		return tcu::TestStatus::pass("Pass");
	}
}

tcu::TestStatus testSemaphoreSignalWaitImport (Context& context, vk::VkExternalSemaphoreHandleTypeFlagBitsKHX externalType)
{
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const vk::Unique<vk::VkInstance>	instance			(createInstance(vkp, externalType, 0u));
	const vk::InstanceDriver			vki					(vkp, *instance);
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, *instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	checkSemaphoreSupport(vki, physicalDevice, externalType);

	{
		const vk::Unique<vk::VkDevice>		device		(createDevice(vki, physicalDevice, externalType, 0u, queueFamilyIndex));
		const vk::DeviceDriver				vkd			(vki, *device);
		const vk::VkQueue					queue		(getQueue(vkd, *device, queueFamilyIndex));

		const vk::Unique<vk::VkSemaphore>	semaphoreA	(createExportableSemaphore(vkd, *device, externalType));
		const vk::Unique<vk::VkSemaphore>	semaphoreB	(createSemaphore(vkd, *device));
		NativeHandle						handle;

		if (getHandleTypePermanence(externalType) == PERMANENCE_TEMPORARY)
			submitDummySignal(vkd, queue, *semaphoreA);

		getSemaphoreNative(vkd, *device, *semaphoreA, externalType, handle);

		submitDummySignal(vkd, queue, *semaphoreB);
		submitDummyWait(vkd, queue, *semaphoreB);

		VK_CHECK(vkd.queueWaitIdle(queue));

		importSemaphore(vkd, *device, *semaphoreB, externalType, handle);

		if (getHandleTypePermanence(externalType) == PERMANENCE_TEMPORARY)
			submitDummyWait(vkd, queue, *semaphoreB);
		else if (getHandleTypePermanence(externalType) == PERMANENCE_PERMANENT)
		{
			submitDummySignal(vkd, queue, *semaphoreA);
			submitDummyWait(vkd, queue, *semaphoreB);
		}
		else
			DE_FATAL("Unknow permanence.");

		VK_CHECK(vkd.queueWaitIdle(queue));

		return tcu::TestStatus::pass("Pass");
	}
}

tcu::TestStatus testSemaphoreMultipleExports (Context& context, vk::VkExternalSemaphoreHandleTypeFlagBitsKHX externalType)
{
	const size_t						exportCount			= 4 * 1024;
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const vk::Unique<vk::VkInstance>	instance			(createInstance(vkp, externalType, 0u));
	const vk::InstanceDriver			vki					(vkp, *instance);
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, *instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	checkSemaphoreSupport(vki, physicalDevice, externalType);

	{
		const vk::Unique<vk::VkDevice>		device		(createDevice(vki, physicalDevice, externalType, 0u, queueFamilyIndex));
		const vk::DeviceDriver				vkd			(vki, *device);
		const vk::VkQueue					queue		(getQueue(vkd, *device, queueFamilyIndex));
		const vk::Unique<vk::VkSemaphore>	semaphore	(createExportableSemaphore(vkd, *device, externalType));

		if (getHandleTypePermanence(externalType) == PERMANENCE_TEMPORARY)
			submitDummySignal(vkd, queue, *semaphore);

		for (size_t exportNdx = 0; exportNdx < exportCount; exportNdx++)
		{
			NativeHandle handle;
			getSemaphoreNative(vkd, *device, *semaphore, externalType, handle);
		}

		if (getHandleTypePermanence(externalType) == PERMANENCE_TEMPORARY)
			submitDummyWait(vkd, queue, *semaphore);
		else if (getHandleTypePermanence(externalType) == PERMANENCE_PERMANENT)
		{
			submitDummySignal(vkd, queue, *semaphore);
			submitDummyWait(vkd, queue, *semaphore);
		}
		else
			DE_FATAL("Unknow permanence.");

		VK_CHECK(vkd.queueWaitIdle(queue));
	}

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus testSemaphoreMultipleImports (Context& context, vk::VkExternalSemaphoreHandleTypeFlagBitsKHX externalType)
{
	const size_t						importCount			= 4 * 1024;
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const vk::Unique<vk::VkInstance>	instance			(createInstance(vkp, externalType, 0u));
	const vk::InstanceDriver			vki					(vkp, *instance);
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, *instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	checkSemaphoreSupport(vki, physicalDevice, externalType);

	{
		const vk::Unique<vk::VkDevice>		device		(createDevice(vki, physicalDevice, externalType, 0u, queueFamilyIndex));
		const vk::DeviceDriver				vkd			(vki, *device);
		const vk::VkQueue					queue		(getQueue(vkd, *device, queueFamilyIndex));
		const vk::Unique<vk::VkSemaphore>	semaphoreA	(createExportableSemaphore(vkd, *device, externalType));
		NativeHandle						handleA;

		if (getHandleTypePermanence(externalType) == PERMANENCE_TEMPORARY)
			submitDummySignal(vkd, queue, *semaphoreA);

		getSemaphoreNative(vkd, *device, *semaphoreA, externalType, handleA);

		for (size_t importNdx = 0; importNdx < importCount; importNdx++)
		{
			NativeHandle						handleB		(handleA);
			const vk::Unique<vk::VkSemaphore>	semaphoreB	(createAndImportSemaphore(vkd, *device, externalType, handleB));
		}

		if (getHandleTypePermanence(externalType) == PERMANENCE_TEMPORARY)
			submitDummyWait(vkd, queue, *semaphoreA);
		else if (getHandleTypePermanence(externalType) == PERMANENCE_PERMANENT)
		{
			submitDummySignal(vkd, queue, *semaphoreA);
			submitDummyWait(vkd, queue, *semaphoreA);
		}
		else
			DE_FATAL("Unknow permanence.");

		VK_CHECK(vkd.queueWaitIdle(queue));
	}

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus testSemaphorePermanence (Context& context, vk::VkExternalSemaphoreHandleTypeFlagBitsKHX externalType)
{
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const vk::Unique<vk::VkInstance>	instance			(createInstance(vkp, externalType, 0u));
	const vk::InstanceDriver			vki					(vkp, *instance);
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, *instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	checkSemaphoreSupport(vki, physicalDevice, externalType);

	{
		const vk::Unique<vk::VkDevice>		device				(createDevice(vki, physicalDevice, externalType, 0u, queueFamilyIndex));
		const vk::DeviceDriver				vkd					(vki, *device);
		const vk::VkQueue					queue				(getQueue(vkd, *device, queueFamilyIndex));

		const vk::Unique<vk::VkSemaphore>	semaphoreA			(createExportableSemaphore(vkd, *device, externalType));
		NativeHandle						handle;

		if (getHandleTypePermanence(externalType) == PERMANENCE_TEMPORARY)
			submitDummySignal(vkd, queue, *semaphoreA);

		getSemaphoreNative(vkd, *device, *semaphoreA, externalType, handle);

		{
			const vk::Unique<vk::VkSemaphore>	semaphoreB			(createAndImportSemaphore(vkd, *device, externalType, handle));

			if (getHandleTypePermanence(externalType) == PERMANENCE_TEMPORARY)
			{
				submitDummyWait(vkd, queue, *semaphoreB);
				VK_CHECK(vkd.queueWaitIdle(queue));

				submitDummySignal(vkd, queue, *semaphoreA);
				submitDummySignal(vkd, queue, *semaphoreB);

				submitDummyWait(vkd, queue, *semaphoreA);
				submitDummyWait(vkd, queue, *semaphoreB);
				VK_CHECK(vkd.queueWaitIdle(queue));
			}
			else if (getHandleTypePermanence(externalType) == PERMANENCE_PERMANENT)
			{
				submitDummySignal(vkd, queue, *semaphoreA);
				submitDummyWait(vkd, queue, *semaphoreB);
				VK_CHECK(vkd.queueWaitIdle(queue));

				submitDummySignal(vkd, queue, *semaphoreB);
				submitDummyWait(vkd, queue, *semaphoreA);
				VK_CHECK(vkd.queueWaitIdle(queue));
			}
			else
				DE_FATAL("Unknow permanence.");
		}

		return tcu::TestStatus::pass("Pass");
	}
}

tcu::TestStatus testSemaphoreFdDup (Context& context, vk::VkExternalSemaphoreHandleTypeFlagBitsKHX externalType)
{
#if (DE_OS == DE_OS_ANDROID) || (DE_OS == DE_OS_UNIX)
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const vk::Unique<vk::VkInstance>	instance			(createInstance(vkp, externalType, 0u));
	const vk::InstanceDriver			vki					(vkp, *instance);
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, *instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	checkSemaphoreSupport(vki, physicalDevice, externalType);

	{
		const vk::Unique<vk::VkDevice>		device		(createDevice(vki, physicalDevice, externalType, 0u, queueFamilyIndex));
		const vk::DeviceDriver				vkd			(vki, *device);
		const vk::VkQueue					queue		(getQueue(vkd, *device, queueFamilyIndex));

		TestLog&							log			= context.getTestContext().getLog();
		const vk::Unique<vk::VkSemaphore>	semaphoreA	(createExportableSemaphore(vkd, *device, externalType));

		if (getHandleTypePermanence(externalType) == PERMANENCE_TEMPORARY)
			submitDummySignal(vkd, queue, *semaphoreA);

		{
			const NativeHandle	fd		(getSemaphoreFd(vkd, *device, *semaphoreA, externalType));
			NativeHandle		newFd	(dup(fd.getFd()));

			if (newFd.getFd() < 0)
				log << TestLog::Message << "dup() failed: '" << strerror(errno) << "'" << TestLog::EndMessage;

			TCU_CHECK_MSG(newFd.getFd() >= 0, "Failed to call dup() for semaphores fd");

			{
				const vk::Unique<vk::VkSemaphore> semaphoreB (createAndImportSemaphore(vkd, *device, externalType, newFd));

				if (getHandleTypePermanence(externalType) == PERMANENCE_TEMPORARY)
					submitDummyWait(vkd, queue, *semaphoreB);
				else if (getHandleTypePermanence(externalType) == PERMANENCE_PERMANENT)
				{
					submitDummySignal(vkd, queue, *semaphoreA);
					submitDummyWait(vkd, queue, *semaphoreB);
				}
				else
					DE_FATAL("Unknow permanence.");
			}
		}

		VK_CHECK(vkd.queueWaitIdle(queue));

		return tcu::TestStatus::pass("Pass");
	}
#else
	DE_UNREF(context);
	DE_UNREF(externalType);
	TCU_THROW(NotSupportedError, "Platform doesn't support dup()");
#endif
}

tcu::TestStatus testSemaphoreFdDup2 (Context& context, vk::VkExternalSemaphoreHandleTypeFlagBitsKHX externalType)
{
#if (DE_OS == DE_OS_ANDROID) || (DE_OS == DE_OS_UNIX)
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const vk::Unique<vk::VkInstance>	instance			(createInstance(vkp, externalType, 0u));
	const vk::InstanceDriver			vki					(vkp, *instance);
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, *instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	checkSemaphoreSupport(vki, physicalDevice, externalType);

	{
		const vk::Unique<vk::VkDevice>		device		(createDevice(vki, physicalDevice, externalType, 0u, queueFamilyIndex));
		const vk::DeviceDriver				vkd			(vki, *device);
		const vk::VkQueue					queue		(getQueue(vkd, *device, queueFamilyIndex));

		TestLog&							log			= context.getTestContext().getLog();
		const vk::Unique<vk::VkSemaphore>	semaphoreA	(createExportableSemaphore(vkd, *device, externalType));

		if (getHandleTypePermanence(externalType) == PERMANENCE_TEMPORARY)
			submitDummySignal(vkd, queue, *semaphoreA);

		{
			const NativeHandle	fd			(getSemaphoreFd(vkd, *device, *semaphoreA, externalType));
			NativeHandle		secondFd	(getSemaphoreFd(vkd, *device, *semaphoreA, externalType));
			int					newFd		(dup2(fd.getFd(), secondFd.getFd()));

			if (newFd < 0)
				log << TestLog::Message << "dup2() failed: '" << strerror(errno) << "'" << TestLog::EndMessage;

			TCU_CHECK_MSG(newFd >= 0, "Failed to call dup2() for semaphores fd");

			{
				const vk::Unique<vk::VkSemaphore> semaphoreB (createAndImportSemaphore(vkd, *device, externalType, secondFd));

				if (getHandleTypePermanence(externalType) == PERMANENCE_TEMPORARY)
					submitDummyWait(vkd, queue, *semaphoreB);
				else if (getHandleTypePermanence(externalType) == PERMANENCE_PERMANENT)
				{
					submitDummySignal(vkd, queue, *semaphoreA);
					submitDummyWait(vkd, queue, *semaphoreB);
				}
				else
					DE_FATAL("Unknow permanence.");
			}
		}

		VK_CHECK(vkd.queueWaitIdle(queue));

		return tcu::TestStatus::pass("Pass");
	}
#else
	DE_UNREF(context);
	DE_UNREF(externalType);
	TCU_THROW(NotSupportedError, "Platform doesn't support dup2()");
#endif
}

tcu::TestStatus testSemaphoreFdDup3 (Context& context, vk::VkExternalSemaphoreHandleTypeFlagBitsKHX externalType)
{
#if (DE_OS == DE_OS_UNIX) && defined(_GNU_SOURCE)
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const vk::Unique<vk::VkInstance>	instance			(createInstance(vkp, externalType, 0u));
	const vk::InstanceDriver			vki					(vkp, *instance);
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, *instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	checkSemaphoreSupport(vki, physicalDevice, externalType);

	{
		const vk::Unique<vk::VkDevice>		device		(createDevice(vki, physicalDevice, externalType, 0u, queueFamilyIndex));
		const vk::DeviceDriver				vkd			(vki, *device);
		const vk::VkQueue					queue		(getQueue(vkd, *device, queueFamilyIndex));

		TestLog&							log			= context.getTestContext().getLog();
		const vk::Unique<vk::VkSemaphore>	semaphoreA	(createExportableSemaphore(vkd, *device, externalType));

		if (getHandleTypePermanence(externalType) == PERMANENCE_TEMPORARY)
			submitDummySignal(vkd, queue, *semaphoreA);

		{
			const NativeHandle	fd			(getSemaphoreFd(vkd, *device, *semaphoreA, externalType));
			NativeHandle		secondFd	(getSemaphoreFd(vkd, *device, *semaphoreA, externalType));
			const int			newFd		(dup3(fd.getFd(), secondFd.getFd(), 0));

			if (newFd < 0)
				log << TestLog::Message << "dup3() failed: '" << strerror(errno) << "'" << TestLog::EndMessage;

			TCU_CHECK_MSG(newFd >= 0, "Failed to call dup3() for semaphores fd");

			{
				const vk::Unique<vk::VkSemaphore> semaphoreB (createAndImportSemaphore(vkd, *device, externalType, secondFd));

				if (getHandleTypePermanence(externalType) == PERMANENCE_TEMPORARY)
					submitDummyWait(vkd, queue, *semaphoreB);
				else if (getHandleTypePermanence(externalType) == PERMANENCE_PERMANENT)
				{
					submitDummySignal(vkd, queue, *semaphoreA);
					submitDummyWait(vkd, queue, *semaphoreB);
				}
				else
					DE_FATAL("Unknow permanence.");
			}
		}

		VK_CHECK(vkd.queueWaitIdle(queue));

		return tcu::TestStatus::pass("Pass");
	}
#else
	DE_UNREF(context);
	DE_UNREF(externalType);
	TCU_THROW(NotSupportedError, "Platform doesn't support dup3()");
#endif
}

tcu::TestStatus testSemaphoreFdSendOverSocket (Context& context, vk::VkExternalSemaphoreHandleTypeFlagBitsKHX externalType)
{
#if (DE_OS == DE_OS_ANDROID) || (DE_OS == DE_OS_UNIX)
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const vk::Unique<vk::VkInstance>	instance			(createInstance(vkp, externalType, 0u));
	const vk::InstanceDriver			vki					(vkp, *instance);
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, *instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	checkSemaphoreSupport(vki, physicalDevice, externalType);

	{
		const vk::Unique<vk::VkDevice>		device		(createDevice(vki, physicalDevice, externalType, 0u, queueFamilyIndex));
		const vk::DeviceDriver				vkd			(vki, *device);
		const vk::VkQueue					queue		(getQueue(vkd, *device, queueFamilyIndex));

		TestLog&							log			= context.getTestContext().getLog();
		const vk::Unique<vk::VkSemaphore>	semaphore	(createExportableSemaphore(vkd, *device, externalType));

		if (getHandleTypePermanence(externalType) == PERMANENCE_TEMPORARY)
			submitDummySignal(vkd, queue, *semaphore);

		const NativeHandle	fd	(getSemaphoreFd(vkd, *device, *semaphore, externalType));

		{
			int sv[2];

			if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0)
			{
				log << TestLog::Message << "Failed to create socket pair: '" << strerror(errno) << "'" << TestLog::EndMessage;
				TCU_FAIL("Failed to create socket pair");
			}

			{
				const NativeHandle	srcSocket	(sv[0]);
				const NativeHandle	dstSocket	(sv[1]);
				std::string			sendData	("deqp");

				// Send FD
				{
					const int			fdRaw	(fd.getFd());
					msghdr				msg;
					cmsghdr*			cmsg;
					char				buffer[CMSG_SPACE(sizeof(int))];
					iovec				iov		= { &sendData[0], sendData.length()};

					deMemset(&msg, 0, sizeof(msg));

					msg.msg_control		= buffer;
					msg.msg_controllen	= sizeof(buffer);
					msg.msg_iovlen		= 1;
					msg.msg_iov			= &iov;

					cmsg				= CMSG_FIRSTHDR(&msg);
					cmsg->cmsg_level	= SOL_SOCKET;
					cmsg->cmsg_type		= SCM_RIGHTS;
					cmsg->cmsg_len		= CMSG_LEN(sizeof(int));

					deMemcpy(CMSG_DATA(cmsg), &fdRaw, sizeof(int));
					msg.msg_controllen = cmsg->cmsg_len;

					if (sendmsg(srcSocket.getFd(), &msg, 0) < 0)
					{
						log << TestLog::Message << "Failed to send fd over socket: '" << strerror(errno) << "'" << TestLog::EndMessage;
						TCU_FAIL("Failed to send fd over socket");
					}
				}

				// Recv FD
				{
					msghdr			msg;
					char			buffer[CMSG_SPACE(sizeof(int))];
					std::string		recvData	(4, '\0');
					iovec			iov			= { &recvData[0], recvData.length() };

					deMemset(&msg, 0, sizeof(msg));

					msg.msg_control		= buffer;
					msg.msg_controllen	= sizeof(buffer);
					msg.msg_iovlen		= 1;
					msg.msg_iov			= &iov;

					const ssize_t	bytes = recvmsg(dstSocket.getFd(), &msg, 0);

					if (bytes < 0)
					{
						log << TestLog::Message << "Failed to recv fd over socket: '" << strerror(errno) << "'" << TestLog::EndMessage;
						TCU_FAIL("Failed to recv fd over socket");

					}
					else if (bytes != (ssize_t)sendData.length())
					{
						TCU_FAIL("recvmsg() returned unpexpected number of bytes");
					}
					else
					{
						const cmsghdr* const	cmsg	= CMSG_FIRSTHDR(&msg);
						int						newFd_;
						deMemcpy(&newFd_, CMSG_DATA(cmsg), sizeof(int));
						NativeHandle			newFd	(newFd_);

						TCU_CHECK(cmsg->cmsg_level == SOL_SOCKET);
						TCU_CHECK(cmsg->cmsg_type == SCM_RIGHTS);
						TCU_CHECK(cmsg->cmsg_len == CMSG_LEN(sizeof(int)));
						TCU_CHECK(recvData == sendData);
						TCU_CHECK_MSG(newFd.getFd() >= 0, "Didn't receive valid fd from socket");

						{
							const vk::Unique<vk::VkSemaphore> newSemaphore (createAndImportSemaphore(vkd, *device, externalType, newFd));

							if (getHandleTypePermanence(externalType) == PERMANENCE_TEMPORARY)
								submitDummyWait(vkd, queue, *newSemaphore);
							else if (getHandleTypePermanence(externalType) == PERMANENCE_PERMANENT)
							{
								submitDummySignal(vkd, queue, *newSemaphore);
								submitDummyWait(vkd, queue, *newSemaphore);
							}
							else
								DE_FATAL("Unknow permanence.");

							VK_CHECK(vkd.queueWaitIdle(queue));
						}
					}
				}
			}
		}
	}

	return tcu::TestStatus::pass("Pass");
#else
	DE_UNREF(context);
	DE_UNREF(externalType);
	TCU_THROW(NotSupportedError, "Platform doesn't support sending file descriptors over socket");
#endif
}

tcu::TestStatus testBufferQueries (Context& context, vk::VkExternalMemoryHandleTypeFlagBitsKHX externalType)
{
	const vk::VkBufferCreateFlags		createFlags[]	=
	{
		0u,
		vk::VK_BUFFER_CREATE_SPARSE_BINDING_BIT,
		vk::VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT,
		vk::VK_BUFFER_CREATE_SPARSE_ALIASED_BIT
	};
	const vk::VkBufferUsageFlags		usageFlags[]	=
	{
		vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		vk::VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT,
		vk::VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT,
		vk::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		vk::VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		vk::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
	};
	const vk::PlatformInterface&		vkp			(context.getPlatformInterface());
	const vk::Unique<vk::VkInstance>	instance	(createInstance(vkp, 0u, externalType));
	const vk::InstanceDriver			vki			(vkp, *instance);
	const vk::VkPhysicalDevice			device		(vk::chooseDevice(vki, *instance, context.getTestContext().getCommandLine()));

	TestLog&							log			= context.getTestContext().getLog();

	for (size_t createFlagNdx = 0; createFlagNdx < DE_LENGTH_OF_ARRAY(createFlags); createFlagNdx++)
	for (size_t usageFlagNdx = 0; usageFlagNdx < DE_LENGTH_OF_ARRAY(usageFlags); usageFlagNdx++)
	{
		const vk::VkBufferViewCreateFlags				createFlag		= createFlags[createFlagNdx];
		const vk::VkBufferUsageFlags					usageFlag		= usageFlags[usageFlagNdx];
		const vk::VkPhysicalDeviceExternalBufferInfoKHX	info			=
		{
			vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_BUFFER_INFO_KHX,
			DE_NULL,

			createFlag,
			usageFlag,
			externalType
		};
		vk::VkExternalBufferPropertiesKHX				properties		=
		{
			vk::VK_STRUCTURE_TYPE_EXTERNAL_BUFFER_PROPERTIES_KHX,
			DE_NULL,

			{ 0u, 0u, 0u }
		};

		vki.getPhysicalDeviceExternalBufferPropertiesKHX(device, &info, &properties);

		log << TestLog::Message << properties << TestLog::EndMessage;

		TCU_CHECK(properties.sType == vk::VK_STRUCTURE_TYPE_EXTERNAL_BUFFER_PROPERTIES_KHX);
		TCU_CHECK(properties.pNext == DE_NULL);
	}

	return tcu::TestStatus::pass("Pass");
}

struct MemoryTestConfig
{
												MemoryTestConfig	(vk::VkExternalMemoryHandleTypeFlagBitsKHX	externalType_,
																	 bool										hostVisible_)
		: externalType	(externalType_)
		, hostVisible	(hostVisible_)
	{
	}

	vk::VkExternalMemoryHandleTypeFlagBitsKHX	externalType;
	bool										hostVisible;
};

tcu::TestStatus testMemoryImportTwice (Context& context, MemoryTestConfig config)
{
	const vk::PlatformInterface&			vkp					(context.getPlatformInterface());
	const vk::Unique<vk::VkInstance>		instance			(createInstance(vkp, 0u, config.externalType));
	const vk::InstanceDriver				vki					(vkp, *instance);
	const vk::VkPhysicalDevice				physicalDevice		(vk::chooseDevice(vki, *instance, context.getTestContext().getCommandLine()));
	const deUint32							queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));
	const vk::Unique<vk::VkDevice>			device				(createDevice(vki, physicalDevice, 0u, config.externalType, queueFamilyIndex));
	const vk::DeviceDriver					vkd					(vki, *device);
	const vk::VkBufferUsageFlags			usage				= vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT|vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	const deUint32							seed				= 1261033864u;
	const vk::VkDeviceSize					bufferSize			= 1024;
	const std::vector<deUint8>				testData			(genTestData(seed, (size_t)bufferSize));

	checkBufferSupport(vki, physicalDevice, config.externalType, 0u, usage);

	// \note Buffer is only allocated to get memory requirements
	const vk::Unique<vk::VkBuffer>			buffer				(createExternalBuffer(vkd, *device, queueFamilyIndex, config.externalType, bufferSize, usage));
	const vk::VkMemoryRequirements			requirements		(vk::getBufferMemoryRequirements(vkd, *device, *buffer));
	const vk::Unique<vk::VkDeviceMemory>	memory				(allocateExportableMemory(vki, physicalDevice, vkd, *device, requirements, config.externalType, config.hostVisible));
	NativeHandle							handleA;
	NativeHandle							handleB;

	if (config.hostVisible)
		writeHostMemory(vkd, *device, *memory, testData.size(), &testData[0]);

	getMemoryNative(vkd, *device, *memory, config.externalType, handleA);
	getMemoryNative(vkd, *device, *memory, config.externalType, handleB);

	{
		const vk::Unique<vk::VkDeviceMemory>	memoryA	(importMemory(vkd, *device, requirements, config.externalType, handleA));
		const vk::Unique<vk::VkDeviceMemory>	memoryB	(importMemory(vkd, *device, requirements, config.externalType, handleB));

		if (config.hostVisible)
		{
			const std::vector<deUint8>		testDataA		(genTestData(seed ^ 124798807u, (size_t)bufferSize));
			const std::vector<deUint8>		testDataB		(genTestData(seed ^ 970834278u, (size_t)bufferSize));

			checkHostMemory(vkd, *device, *memoryA, testData.size(), &testData[0]);
			checkHostMemory(vkd, *device, *memoryB, testData.size(), &testData[0]);

			writeHostMemory(vkd, *device, *memoryA, testData.size(), &testDataA[0]);
			writeHostMemory(vkd, *device, *memoryB, testData.size(), &testDataB[0]);

			checkHostMemory(vkd, *device, *memoryA, testData.size(), &testDataB[0]);
			checkHostMemory(vkd, *device, *memory, testData.size(), &testDataB[0]);
		}
	}

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus testMemoryMultimpleImports (Context& context, MemoryTestConfig config)
{
	const size_t							count				= 4 * 1024;
	const vk::PlatformInterface&			vkp					(context.getPlatformInterface());
	const vk::Unique<vk::VkInstance>		instance			(createInstance(vkp, 0u, config.externalType));
	const vk::InstanceDriver				vki					(vkp, *instance);
	const vk::VkPhysicalDevice				physicalDevice		(vk::chooseDevice(vki, *instance, context.getTestContext().getCommandLine()));
	const deUint32							queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));
	const vk::Unique<vk::VkDevice>			device				(createDevice(vki, physicalDevice, 0u, config.externalType, queueFamilyIndex));
	const vk::DeviceDriver					vkd					(vki, *device);
	const vk::VkBufferUsageFlags			usage				= vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT|vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	const vk::VkDeviceSize					bufferSize			= 1024;

	checkBufferSupport(vki, physicalDevice, config.externalType, 0u, usage);

	// \note Buffer is only allocated to get memory requirements
	const vk::Unique<vk::VkBuffer>			buffer				(createExternalBuffer(vkd, *device, queueFamilyIndex, config.externalType, bufferSize, usage));
	const vk::VkMemoryRequirements			requirements		(vk::getBufferMemoryRequirements(vkd, *device, *buffer));
	const vk::Unique<vk::VkDeviceMemory>	memory				(allocateExportableMemory(vki, physicalDevice, vkd, *device, requirements, config.externalType, config.hostVisible));
	NativeHandle							handleA;

	getMemoryNative(vkd, *device, *memory, config.externalType, handleA);

	for (size_t ndx = 0; ndx < count; ndx++)
	{
		NativeHandle							handleB	(handleA);
		const vk::Unique<vk::VkDeviceMemory>	memoryB	(importMemory(vkd, *device, requirements, config.externalType, handleB));
	}

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus testMemoryMultimpleExports (Context& context, MemoryTestConfig config)
{
	const size_t							count				= 4 * 1024;
	const vk::PlatformInterface&			vkp					(context.getPlatformInterface());
	const vk::Unique<vk::VkInstance>		instance			(createInstance(vkp, 0u, config.externalType));
	const vk::InstanceDriver				vki					(vkp, *instance);
	const vk::VkPhysicalDevice				physicalDevice		(vk::chooseDevice(vki, *instance, context.getTestContext().getCommandLine()));
	const deUint32							queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));
	const vk::Unique<vk::VkDevice>			device				(createDevice(vki, physicalDevice, 0u, config.externalType, queueFamilyIndex));
	const vk::DeviceDriver					vkd					(vki, *device);
	const vk::VkBufferUsageFlags			usage				= vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT|vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	const vk::VkDeviceSize					bufferSize			= 1024;

	checkBufferSupport(vki, physicalDevice, config.externalType, 0u, usage);

	// \note Buffer is only allocated to get memory requirements
	const vk::Unique<vk::VkBuffer>			buffer				(createExternalBuffer(vkd, *device, queueFamilyIndex, config.externalType, bufferSize, usage));
	const vk::VkMemoryRequirements			requirements		(vk::getBufferMemoryRequirements(vkd, *device, *buffer));
	const vk::Unique<vk::VkDeviceMemory>	memory				(allocateExportableMemory(vki, physicalDevice, vkd, *device, requirements, config.externalType, config.hostVisible));

	for (size_t ndx = 0; ndx < count; ndx++)
	{
		NativeHandle	handle;
		getMemoryNative(vkd, *device, *memory, config.externalType, handle);
	}

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus testMemoryFdDup (Context& context, MemoryTestConfig config)
{
#if (DE_OS == DE_OS_ANDROID) || (DE_OS == DE_OS_UNIX)
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const vk::Unique<vk::VkInstance>	instance			(createInstance(vkp, 0u, config.externalType));
	const vk::InstanceDriver			vki					(vkp, *instance);
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, *instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	{
		const vk::Unique<vk::VkDevice>			device			(createDevice(vki, physicalDevice, 0u, config.externalType, queueFamilyIndex));
		const vk::DeviceDriver					vkd				(vki, *device);

		TestLog&								log				= context.getTestContext().getLog();
		const vk::VkBufferUsageFlags			usage			= vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT|vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		const vk::VkDeviceSize					bufferSize		= 1024;
		const deUint32							seed			= 851493858u;
		const std::vector<deUint8>				testData		(genTestData(seed, (size_t)bufferSize));

		checkBufferSupport(vki, physicalDevice, config.externalType, 0u, usage);

		// \note Buffer is only allocated to get memory requirements
		const vk::Unique<vk::VkBuffer>			buffer			(createExternalBuffer(vkd, *device, queueFamilyIndex, config.externalType, bufferSize, usage));
		const vk::VkMemoryRequirements			requirements	(vk::getBufferMemoryRequirements(vkd, *device, *buffer));
		const vk::Unique<vk::VkDeviceMemory>	memory			(allocateExportableMemory(vki, physicalDevice, vkd, *device, requirements, config.externalType, config.hostVisible));

		if (config.hostVisible)
			writeHostMemory(vkd, *device, *memory, testData.size(), &testData[0]);

		const NativeHandle						fd				(getMemoryFd(vkd, *device, *memory, config.externalType));
		NativeHandle							newFd			(dup(fd.getFd()));

		if (newFd.getFd() < 0)
			log << TestLog::Message << "dup() failed: '" << strerror(errno) << "'" << TestLog::EndMessage;

		TCU_CHECK_MSG(newFd.getFd() >= 0, "Failed to call dup() for memorys fd");

		{
			const vk::Unique<vk::VkDeviceMemory>	newMemory	(importMemory(vkd, *device, requirements, config.externalType, newFd));

			if (config.hostVisible)
			{
				const std::vector<deUint8>	testDataA	(genTestData(seed ^ 672929437u, (size_t)bufferSize));

				checkHostMemory(vkd, *device, *newMemory, testData.size(), &testData[0]);

				writeHostMemory(vkd, *device, *newMemory, testDataA.size(), &testDataA[0]);
				checkHostMemory(vkd, *device, *memory, testDataA.size(), &testDataA[0]);
			}
		}

		return tcu::TestStatus::pass("Pass");
	}
#else
	DE_UNREF(context);
	DE_UNREF(config);
	TCU_THROW(NotSupportedError, "Platform doesn't support dup()");
#endif
}

tcu::TestStatus testMemoryFdDup2 (Context& context, MemoryTestConfig config)
{
#if (DE_OS == DE_OS_ANDROID) || (DE_OS == DE_OS_UNIX)
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const vk::Unique<vk::VkInstance>	instance			(createInstance(vkp, 0u, config.externalType));
	const vk::InstanceDriver			vki					(vkp, *instance);
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, *instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	{
		const vk::Unique<vk::VkDevice>			device			(createDevice(vki, physicalDevice, 0u, config.externalType, queueFamilyIndex));
		const vk::DeviceDriver					vkd				(vki, *device);

		TestLog&								log				= context.getTestContext().getLog();
		const vk::VkBufferUsageFlags			usage			= vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT|vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		const vk::VkDeviceSize					bufferSize		= 1024;
		const deUint32							seed			= 224466865u;
		const std::vector<deUint8>				testData		(genTestData(seed, (size_t)bufferSize));

		checkBufferSupport(vki, physicalDevice, config.externalType, 0u, usage);

		// \note Buffer is only allocated to get memory requirements
		const vk::Unique<vk::VkBuffer>			buffer			(createExternalBuffer(vkd, *device, queueFamilyIndex, config.externalType, bufferSize, usage));
		const vk::VkMemoryRequirements			requirements	(vk::getBufferMemoryRequirements(vkd, *device, *buffer));
		const vk::Unique<vk::VkDeviceMemory>	memory			(allocateExportableMemory(vki, physicalDevice, vkd, *device, requirements, config.externalType, config.hostVisible));

		if (config.hostVisible)
			writeHostMemory(vkd, *device, *memory, testData.size(), &testData[0]);

		const NativeHandle						fd				(getMemoryFd(vkd, *device, *memory, config.externalType));
		NativeHandle							secondFd		(getMemoryFd(vkd, *device, *memory, config.externalType));
		const int								newFd			(dup2(fd.getFd(), secondFd.getFd()));

		if (newFd < 0)
			log << TestLog::Message << "dup2() failed: '" << strerror(errno) << "'" << TestLog::EndMessage;

		TCU_CHECK_MSG(newFd >= 0, "Failed to call dup2() for memorys fd");

		{
			const vk::Unique<vk::VkDeviceMemory>	newMemory	(importMemory(vkd, *device, requirements, config.externalType, secondFd));

			if (config.hostVisible)
			{
				const std::vector<deUint8>	testDataA	(genTestData(seed ^ 99012346u, (size_t)bufferSize));

				checkHostMemory(vkd, *device, *newMemory, testData.size(), &testData[0]);

				writeHostMemory(vkd, *device, *newMemory, testDataA.size(), &testDataA[0]);
				checkHostMemory(vkd, *device, *memory, testDataA.size(), &testDataA[0]);
			}
		}

		return tcu::TestStatus::pass("Pass");
	}
#else
	DE_UNREF(context);
	DE_UNREF(config);
	TCU_THROW(NotSupportedError, "Platform doesn't support dup()");
#endif
}

tcu::TestStatus testMemoryFdDup3 (Context& context, MemoryTestConfig config)
{
#if (DE_OS == DE_OS_UNIX) && defined(_GNU_SOURCE)
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const vk::Unique<vk::VkInstance>	instance			(createInstance(vkp, 0u, config.externalType));
	const vk::InstanceDriver			vki					(vkp, *instance);
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, *instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	{
		const vk::Unique<vk::VkDevice>			device			(createDevice(vki, physicalDevice, 0u, config.externalType, queueFamilyIndex));
		const vk::DeviceDriver					vkd				(vki, *device);

		TestLog&								log				= context.getTestContext().getLog();
		const vk::VkBufferUsageFlags			usage			= vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT|vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		const vk::VkDeviceSize					bufferSize		= 1024;
		const deUint32							seed			= 2554088961u;
		const std::vector<deUint8>				testData		(genTestData(seed, (size_t)bufferSize));

		checkBufferSupport(vki, physicalDevice, config.externalType, 0u, usage);

		// \note Buffer is only allocated to get memory requirements
		const vk::Unique<vk::VkBuffer>			buffer			(createExternalBuffer(vkd, *device, queueFamilyIndex, config.externalType, bufferSize, usage));
		const vk::VkMemoryRequirements			requirements	(vk::getBufferMemoryRequirements(vkd, *device, *buffer));
		const vk::Unique<vk::VkDeviceMemory>	memory			(allocateExportableMemory(vki, physicalDevice, vkd, *device, requirements, config.externalType, config.hostVisible));

		if (config.hostVisible)
			writeHostMemory(vkd, *device, *memory, testData.size(), &testData[0]);

		const NativeHandle						fd				(getMemoryFd(vkd, *device, *memory, config.externalType));
		NativeHandle							secondFd		(getMemoryFd(vkd, *device, *memory, config.externalType));
		const int								newFd			(dup3(fd.getFd(), secondFd.getFd(), 0));

		if (newFd < 0)
			log << TestLog::Message << "dup3() failed: '" << strerror(errno) << "'" << TestLog::EndMessage;

		TCU_CHECK_MSG(newFd >= 0, "Failed to call dup3() for memorys fd");

		{
			const vk::Unique<vk::VkDeviceMemory>	newMemory	(importMemory(vkd, *device, requirements, config.externalType, secondFd));

			if (config.hostVisible)
			{
				const std::vector<deUint8>	testDataA	(genTestData(seed ^ 4210342378u, (size_t)bufferSize));

				checkHostMemory(vkd, *device, *newMemory, testData.size(), &testData[0]);

				writeHostMemory(vkd, *device, *newMemory, testDataA.size(), &testDataA[0]);
				checkHostMemory(vkd, *device, *memory, testDataA.size(), &testDataA[0]);
			}
		}

		return tcu::TestStatus::pass("Pass");
	}
#else
	DE_UNREF(context);
	DE_UNREF(config);
	TCU_THROW(NotSupportedError, "Platform doesn't support dup()");
#endif
}

tcu::TestStatus testMemoryFdSendOverSocket (Context& context, MemoryTestConfig config)
{
#if (DE_OS == DE_OS_ANDROID) || (DE_OS == DE_OS_UNIX)
	const vk::PlatformInterface&				vkp					(context.getPlatformInterface());
	const vk::Unique<vk::VkInstance>			instance			(createInstance(vkp, 0u, config.externalType));
	const vk::InstanceDriver					vki					(vkp, *instance);
	const vk::VkPhysicalDevice					physicalDevice		(vk::chooseDevice(vki, *instance, context.getTestContext().getCommandLine()));
	const deUint32								queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	{
		const vk::Unique<vk::VkDevice>			device				(createDevice(vki, physicalDevice, 0u, config.externalType, queueFamilyIndex));
		const vk::DeviceDriver					vkd					(vki, *device);

		TestLog&								log					= context.getTestContext().getLog();
		const vk::VkBufferUsageFlags			usage				= vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT|vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		const vk::VkDeviceSize					bufferSize			= 1024;
		const deUint32							seed				= 3403586456u;
		const std::vector<deUint8>				testData			(genTestData(seed, (size_t)bufferSize));

		checkBufferSupport(vki, physicalDevice, config.externalType, 0u, usage);

		// \note Buffer is only allocated to get memory requirements
		const vk::Unique<vk::VkBuffer>			buffer				(createExternalBuffer(vkd, *device, queueFamilyIndex, config.externalType, bufferSize, usage));
		const vk::VkMemoryRequirements			requirements		(vk::getBufferMemoryRequirements(vkd, *device, *buffer));
		const vk::Unique<vk::VkDeviceMemory>	memory				(allocateExportableMemory(vki, physicalDevice, vkd, *device, requirements, config.externalType, config.hostVisible));

		if (config.hostVisible)
			writeHostMemory(vkd, *device, *memory, testData.size(), &testData[0]);

		const NativeHandle						fd					(getMemoryFd(vkd, *device, *memory, config.externalType));

		{
			int sv[2];

			if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0)
			{
				log << TestLog::Message << "Failed to create socket pair: '" << strerror(errno) << "'" << TestLog::EndMessage;
				TCU_FAIL("Failed to create socket pair");
			}

			{
				const NativeHandle	srcSocket	(sv[0]);
				const NativeHandle	dstSocket	(sv[1]);
				std::string			sendData	("deqp");

				// Send FD
				{
					const int			fdRaw	(fd.getFd());
					msghdr				msg;
					cmsghdr*			cmsg;
					char				tmpBuffer[CMSG_SPACE(sizeof(int))];
					iovec				iov		= { &sendData[0], sendData.length()};

					deMemset(&msg, 0, sizeof(msg));

					msg.msg_control		= tmpBuffer;
					msg.msg_controllen	= sizeof(tmpBuffer);
					msg.msg_iovlen		= 1;
					msg.msg_iov			= &iov;

					cmsg				= CMSG_FIRSTHDR(&msg);
					cmsg->cmsg_level	= SOL_SOCKET;
					cmsg->cmsg_type		= SCM_RIGHTS;
					cmsg->cmsg_len		= CMSG_LEN(sizeof(int));

					deMemcpy(CMSG_DATA(cmsg), &fdRaw, sizeof(int));
					msg.msg_controllen = cmsg->cmsg_len;

					if (sendmsg(srcSocket.getFd(), &msg, 0) < 0)
					{
						log << TestLog::Message << "Failed to send fd over socket: '" << strerror(errno) << "'" << TestLog::EndMessage;
						TCU_FAIL("Failed to send fd over socket");
					}
				}

				// Recv FD
				{
					msghdr			msg;
					char			tmpBuffer[CMSG_SPACE(sizeof(int))];
					std::string		recvData	(4, '\0');
					iovec			iov			= { &recvData[0], recvData.length() };

					deMemset(&msg, 0, sizeof(msg));

					msg.msg_control		= tmpBuffer;
					msg.msg_controllen	= sizeof(tmpBuffer);
					msg.msg_iovlen		= 1;
					msg.msg_iov			= &iov;

					const ssize_t	bytes = recvmsg(dstSocket.getFd(), &msg, 0);

					if (bytes < 0)
					{
						log << TestLog::Message << "Failed to recv fd over socket: '" << strerror(errno) << "'" << TestLog::EndMessage;
						TCU_FAIL("Failed to recv fd over socket");

					}
					else if (bytes != (ssize_t)sendData.length())
					{
						TCU_FAIL("recvmsg() returned unpexpected number of bytes");
					}
					else
					{
						const cmsghdr* const	cmsg	= CMSG_FIRSTHDR(&msg);
						int						newFd_;
						deMemcpy(&newFd_, CMSG_DATA(cmsg), sizeof(int));
						NativeHandle			newFd	(newFd_);

						TCU_CHECK(cmsg->cmsg_level == SOL_SOCKET);
						TCU_CHECK(cmsg->cmsg_type == SCM_RIGHTS);
						TCU_CHECK(cmsg->cmsg_len == CMSG_LEN(sizeof(int)));
						TCU_CHECK(recvData == sendData);
						TCU_CHECK_MSG(newFd.getFd() >= 0, "Didn't receive valid fd from socket");

						{
							const vk::Unique<vk::VkDeviceMemory> newMemory (importMemory(vkd, *device, requirements, config.externalType, newFd));

							if (config.hostVisible)
							{
								const std::vector<deUint8>	testDataA	(genTestData(seed ^ 23478978u, (size_t)bufferSize));

								checkHostMemory(vkd, *device, *newMemory, testData.size(), &testData[0]);

								writeHostMemory(vkd, *device, *newMemory, testDataA.size(), &testDataA[0]);
								checkHostMemory(vkd, *device, *memory, testDataA.size(), &testDataA[0]);
							}
						}
					}
				}
			}
		}
	}

	return tcu::TestStatus::pass("Pass");
#else
	DE_UNREF(context);
	DE_UNREF(config);
	TCU_THROW(NotSupportedError, "Platform doesn't support sending file descriptors over socket");
#endif
}

tcu::TestStatus testBufferBindExportImportBind (Context& context, vk::VkExternalMemoryHandleTypeFlagBitsKHX externalType)
{
	const vk::PlatformInterface&			vkp					(context.getPlatformInterface());
	const vk::Unique<vk::VkInstance>		instance			(createInstance(vkp, 0u, externalType));
	const vk::InstanceDriver				vki					(vkp, *instance);
	const vk::VkPhysicalDevice				physicalDevice		(vk::chooseDevice(vki, *instance, context.getTestContext().getCommandLine()));
	const deUint32							queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));
	const vk::Unique<vk::VkDevice>			device				(createDevice(vki, physicalDevice, 0u, externalType, queueFamilyIndex));
	const vk::DeviceDriver					vkd					(vki, *device);
	const vk::VkBufferUsageFlags			usage				= vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT|vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	const vk::VkDeviceSize					bufferSize			= 1024;

	checkBufferSupport(vki, physicalDevice, externalType, 0u, usage);

	// \note Buffer is only allocated to get memory requirements
	const vk::Unique<vk::VkBuffer>			bufferA				(createExternalBuffer(vkd, *device, queueFamilyIndex, externalType, bufferSize, usage));
	const vk::VkMemoryRequirements			requirements		(vk::getBufferMemoryRequirements(vkd, *device, *bufferA));
	const vk::Unique<vk::VkDeviceMemory>	memoryA				(allocateExportableMemory(vkd, *device, requirements, externalType));
	NativeHandle							handle;

	VK_CHECK(vkd.bindBufferMemory(*device, *bufferA, *memoryA, 0u));

	getMemoryNative(vkd, *device, *memoryA, externalType, handle);

	{
		const vk::Unique<vk::VkDeviceMemory>	memoryB	(importMemory(vkd, *device, requirements, externalType, handle));
		const vk::Unique<vk::VkBuffer>			bufferB	(createExternalBuffer(vkd, *device, queueFamilyIndex, externalType, bufferSize, usage));

		VK_CHECK(vkd.bindBufferMemory(*device, *bufferB, *memoryB, 0u));
	}

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus testBufferExportBindImportBind (Context& context, vk::VkExternalMemoryHandleTypeFlagBitsKHX externalType)
{
	const vk::PlatformInterface&			vkp					(context.getPlatformInterface());
	const vk::Unique<vk::VkInstance>		instance			(createInstance(vkp, 0u, externalType));
	const vk::InstanceDriver				vki					(vkp, *instance);
	const vk::VkPhysicalDevice				physicalDevice		(vk::chooseDevice(vki, *instance, context.getTestContext().getCommandLine()));
	const deUint32							queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));
	const vk::Unique<vk::VkDevice>			device				(createDevice(vki, physicalDevice, 0u, externalType, queueFamilyIndex));
	const vk::DeviceDriver					vkd					(vki, *device);
	const vk::VkBufferUsageFlags			usage				= vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT|vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	const vk::VkDeviceSize					bufferSize			= 1024;

	checkBufferSupport(vki, physicalDevice, externalType, 0u, usage);

	// \note Buffer is only allocated to get memory requirements
	const vk::Unique<vk::VkBuffer>			bufferA				(createExternalBuffer(vkd, *device, queueFamilyIndex, externalType, bufferSize, usage));
	const vk::VkMemoryRequirements			requirements		(vk::getBufferMemoryRequirements(vkd, *device, *bufferA));
	const vk::Unique<vk::VkDeviceMemory>	memoryA				(allocateExportableMemory(vkd, *device, requirements, externalType));
	NativeHandle							handle;

	getMemoryNative(vkd, *device, *memoryA, externalType, handle);
	VK_CHECK(vkd.bindBufferMemory(*device, *bufferA, *memoryA, 0u));

	{
		const vk::Unique<vk::VkDeviceMemory>	memoryB	(importMemory(vkd, *device, requirements, externalType, handle));
		const vk::Unique<vk::VkBuffer>			bufferB	(createExternalBuffer(vkd, *device, queueFamilyIndex, externalType, bufferSize, usage));

		VK_CHECK(vkd.bindBufferMemory(*device, *bufferB, *memoryB, 0u));
	}

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus testBufferExportImportBindBind (Context& context, vk::VkExternalMemoryHandleTypeFlagBitsKHX externalType)
{
	const vk::PlatformInterface&			vkp					(context.getPlatformInterface());
	const vk::Unique<vk::VkInstance>		instance			(createInstance(vkp, 0u, externalType));
	const vk::InstanceDriver				vki					(vkp, *instance);
	const vk::VkPhysicalDevice				physicalDevice		(vk::chooseDevice(vki, *instance, context.getTestContext().getCommandLine()));
	const deUint32							queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));
	const vk::Unique<vk::VkDevice>			device				(createDevice(vki, physicalDevice, 0u, externalType, queueFamilyIndex));
	const vk::DeviceDriver					vkd					(vki, *device);
	const vk::VkBufferUsageFlags			usage				= vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT|vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	const vk::VkDeviceSize					bufferSize			= 1024;

	checkBufferSupport(vki, physicalDevice, externalType, 0u, usage);

	// \note Buffer is only allocated to get memory requirements
	const vk::Unique<vk::VkBuffer>			bufferA				(createExternalBuffer(vkd, *device, queueFamilyIndex, externalType, bufferSize, usage));
	const vk::VkMemoryRequirements			requirements		(vk::getBufferMemoryRequirements(vkd, *device, *bufferA));
	const vk::Unique<vk::VkDeviceMemory>	memoryA				(allocateExportableMemory(vkd, *device, requirements, externalType));
	NativeHandle							handle;

	getMemoryNative(vkd, *device, *memoryA, externalType, handle);

	{
		const vk::Unique<vk::VkDeviceMemory>	memoryB	(importMemory(vkd, *device, requirements, externalType, handle));
		const vk::Unique<vk::VkBuffer>			bufferB	(createExternalBuffer(vkd, *device, queueFamilyIndex, externalType, bufferSize, usage));

		VK_CHECK(vkd.bindBufferMemory(*device, *bufferA, *memoryA, 0u));
		VK_CHECK(vkd.bindBufferMemory(*device, *bufferB, *memoryB, 0u));
	}

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus testImageQueries (Context& context, vk::VkExternalMemoryHandleTypeFlagBitsKHX externalType)
{
	const vk::VkImageCreateFlags		createFlags[]	=
	{
		0u,
		vk::VK_IMAGE_CREATE_SPARSE_BINDING_BIT,
		vk::VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT,
		vk::VK_IMAGE_CREATE_SPARSE_ALIASED_BIT,
		vk::VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT,
		vk::VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
		vk::VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT_KHR
	};
	const vk::VkImageUsageFlags			usageFlags[]	=
	{
		vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		vk::VK_IMAGE_USAGE_SAMPLED_BIT,
		vk::VK_IMAGE_USAGE_STORAGE_BIT,
		vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		vk::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		vk::VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
		vk::VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
	};
	const vk::PlatformInterface&		vkp				(context.getPlatformInterface());
	const vk::Unique<vk::VkInstance>	instance		(createInstance(vkp, 0u, externalType));
	const vk::InstanceDriver			vki				(vkp, *instance);
	const vk::VkPhysicalDevice			device			(vk::chooseDevice(vki, *instance, context.getTestContext().getCommandLine()));

	TestLog&							log				= context.getTestContext().getLog();

	for (size_t createFlagNdx = 0; createFlagNdx < DE_LENGTH_OF_ARRAY(createFlags); createFlagNdx++)
	for (size_t usageFlagNdx = 0; usageFlagNdx < DE_LENGTH_OF_ARRAY(usageFlags); usageFlagNdx++)
	{
		const vk::VkImageViewCreateFlags						createFlag		= createFlags[createFlagNdx];
		const vk::VkImageUsageFlags								usageFlag		= usageFlags[usageFlagNdx];
		const vk::VkFormat										format			= vk::VK_FORMAT_R8G8B8A8_UNORM;
		const vk::VkImageType									type			= vk::VK_IMAGE_TYPE_2D;
		const vk::VkImageTiling									tiling			= vk::VK_IMAGE_TILING_OPTIMAL;
		const vk::VkPhysicalDeviceExternalImageFormatInfoKHX	externalInfo	=
		{
			vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO_KHX,
			DE_NULL,
			externalType
		};
		const vk::VkPhysicalDeviceImageFormatInfo2KHR			info			=
		{
			vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2_KHR,
			&externalInfo,

			format,
			type,
			tiling,
			usageFlag,
			createFlag,
		};
		vk::VkExternalImageFormatPropertiesKHX					externalProperties	=
		{
			vk::VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES_KHX,
			DE_NULL,
			{ 0u, 0u, 0u }
		};
		vk::VkImageFormatProperties2KHR							properties			=
		{
			vk::VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2_KHR,
			&externalProperties,
			{
				{ 0u, 0u, 0u },
				0u,
				0u,
				0u,
				0u
			}
		};

		vki.getPhysicalDeviceImageFormatProperties2KHR(device, &info, &properties);

		log << TestLog::Message << externalProperties << TestLog::EndMessage;
		TCU_CHECK(externalProperties.sType == vk::VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES_KHX);
		TCU_CHECK(externalProperties.pNext == DE_NULL);
	}

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus testImageBindExportImportBind (Context& context, vk::VkExternalMemoryHandleTypeFlagBitsKHX externalType)
{
	const vk::PlatformInterface&			vkp					(context.getPlatformInterface());
	const vk::Unique<vk::VkInstance>		instance			(createInstance(vkp, 0u, externalType));
	const vk::InstanceDriver				vki					(vkp, *instance);
	const vk::VkPhysicalDevice				physicalDevice		(vk::chooseDevice(vki, *instance, context.getTestContext().getCommandLine()));
	const deUint32							queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));
	const vk::Unique<vk::VkDevice>			device				(createDevice(vki, physicalDevice, 0u, externalType, queueFamilyIndex));
	const vk::DeviceDriver					vkd					(vki, *device);
	const vk::VkImageUsageFlags				usage				= vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT|vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	const vk::VkFormat						format				= vk::VK_FORMAT_R8G8B8A8_UNORM;
	const deUint32							width				= 64u;
	const deUint32							height				= 64u;
	const vk::VkImageTiling					tiling				= vk::VK_IMAGE_TILING_OPTIMAL;

	checkImageSupport(vki, physicalDevice, externalType, 0u, usage, format, tiling);

	const vk::Unique<vk::VkImage>			imageA				(createExternalImage(vkd, *device, queueFamilyIndex, externalType, format, width, height, tiling, usage));
	const vk::VkMemoryRequirements			requirements		(vk::getImageMemoryRequirements(vkd, *device, *imageA));
	const vk::Unique<vk::VkDeviceMemory>	memoryA				(allocateExportableMemory(vkd, *device, requirements, externalType));
	NativeHandle							handle;

	VK_CHECK(vkd.bindImageMemory(*device, *imageA, *memoryA, 0u));

	getMemoryNative(vkd, *device, *memoryA, externalType, handle);

	{
		const vk::Unique<vk::VkDeviceMemory>	memoryB	(importMemory(vkd, *device, requirements, externalType, handle));
		const vk::Unique<vk::VkImage>			imageB	(createExternalImage(vkd, *device, queueFamilyIndex, externalType, format, width, height, tiling, usage));

		VK_CHECK(vkd.bindImageMemory(*device, *imageB, *memoryB, 0u));
	}

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus testImageExportBindImportBind (Context& context, vk::VkExternalMemoryHandleTypeFlagBitsKHX externalType)
{
	const vk::PlatformInterface&			vkp					(context.getPlatformInterface());
	const vk::Unique<vk::VkInstance>		instance			(createInstance(vkp, 0u, externalType));
	const vk::InstanceDriver				vki					(vkp, *instance);
	const vk::VkPhysicalDevice				physicalDevice		(vk::chooseDevice(vki, *instance, context.getTestContext().getCommandLine()));
	const deUint32							queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));
	const vk::Unique<vk::VkDevice>			device				(createDevice(vki, physicalDevice, 0u, externalType, queueFamilyIndex));
	const vk::DeviceDriver					vkd					(vki, *device);
	const vk::VkImageUsageFlags				usage				= vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT|vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	const vk::VkFormat						format				= vk::VK_FORMAT_R8G8B8A8_UNORM;
	const deUint32							width				= 64u;
	const deUint32							height				= 64u;
	const vk::VkImageTiling					tiling				= vk::VK_IMAGE_TILING_OPTIMAL;

	checkImageSupport(vki, physicalDevice, externalType, 0u, usage, format, tiling);

	const vk::Unique<vk::VkImage>			imageA				(createExternalImage(vkd, *device, queueFamilyIndex, externalType, format, width, height, tiling, usage));
	const vk::VkMemoryRequirements			requirements		(vk::getImageMemoryRequirements(vkd, *device, *imageA));
	const vk::Unique<vk::VkDeviceMemory>	memoryA				(allocateExportableMemory(vkd, *device, requirements, externalType));
	NativeHandle							handle;

	getMemoryNative(vkd, *device, *memoryA, externalType, handle);
	VK_CHECK(vkd.bindImageMemory(*device, *imageA, *memoryA, 0u));

	{
		const vk::Unique<vk::VkDeviceMemory>	memoryB	(importMemory(vkd, *device, requirements, externalType, handle));
		const vk::Unique<vk::VkImage>			imageB	(createExternalImage(vkd, *device, queueFamilyIndex, externalType, format, width, height, tiling, usage));

		VK_CHECK(vkd.bindImageMemory(*device, *imageB, *memoryB, 0u));
	}

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus testImageExportImportBindBind (Context& context, vk::VkExternalMemoryHandleTypeFlagBitsKHX externalType)
{
	const vk::PlatformInterface&			vkp					(context.getPlatformInterface());
	const vk::Unique<vk::VkInstance>		instance			(createInstance(vkp, 0u, externalType));
	const vk::InstanceDriver				vki					(vkp, *instance);
	const vk::VkPhysicalDevice				physicalDevice		(vk::chooseDevice(vki, *instance, context.getTestContext().getCommandLine()));
	const deUint32							queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));
	const vk::Unique<vk::VkDevice>			device				(createDevice(vki, physicalDevice, 0u, externalType, queueFamilyIndex));
	const vk::DeviceDriver					vkd					(vki, *device);
	const vk::VkImageUsageFlags				usage				= vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT|vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	const vk::VkFormat						format				= vk::VK_FORMAT_R8G8B8A8_UNORM;
	const deUint32							width				= 64u;
	const deUint32							height				= 64u;
	const vk::VkImageTiling					tiling				= vk::VK_IMAGE_TILING_OPTIMAL;

	checkImageSupport(vki, physicalDevice, externalType, 0u, usage, format, tiling);

	// \note Image is only allocated to get memory requirements
	const vk::Unique<vk::VkImage>			imageA				(createExternalImage(vkd, *device, queueFamilyIndex, externalType, format, width, height, tiling, usage));
	const vk::VkMemoryRequirements			requirements		(vk::getImageMemoryRequirements(vkd, *device, *imageA));
	const vk::Unique<vk::VkDeviceMemory>	memoryA				(allocateExportableMemory(vkd, *device, requirements, externalType));
	NativeHandle							handle;

	getMemoryNative(vkd, *device, *memoryA, externalType, handle);

	{
		const vk::Unique<vk::VkDeviceMemory>	memoryB	(importMemory(vkd, *device, requirements, externalType, handle));
		const vk::Unique<vk::VkImage>			imageB	(createExternalImage(vkd, *device, queueFamilyIndex, externalType, format, width, height, tiling, usage));

		VK_CHECK(vkd.bindImageMemory(*device, *imageA, *memoryA, 0u));
		VK_CHECK(vkd.bindImageMemory(*device, *imageB, *memoryB, 0u));
	}

	return tcu::TestStatus::pass("Pass");
}

de::MovePtr<tcu::TestCaseGroup> createSemaphoreTests (tcu::TestContext& testCtx, vk::VkExternalSemaphoreHandleTypeFlagBitsKHX externalType)
{
	de::MovePtr<tcu::TestCaseGroup> semaphoreGroup (new tcu::TestCaseGroup(testCtx, externalSemaphoreTypeToName(externalType), externalSemaphoreTypeToName(externalType)));

	addFunctionCase(semaphoreGroup.get(), "info",						"Test external semaphore queries.",										testSemaphoreQueries,					externalType);
	addFunctionCase(semaphoreGroup.get(), "import_twice",				"Test importing semaphore twice.",										testSemaphoreImportTwice,				externalType);
	addFunctionCase(semaphoreGroup.get(), "reimport",					"Test importing again over previously imported semaphore.",				testSemaphoreImportReimport,			externalType);
	addFunctionCase(semaphoreGroup.get(), "import_multiple_times",		"Test importing sempaher multiple times.",								testSemaphoreMultipleImports,			externalType);
	addFunctionCase(semaphoreGroup.get(), "signal_export_import_wait",	"Test signaling, exporting, importing and waiting for the sempahore.",	testSemaphoreSignalExportImportWait,	externalType);
	addFunctionCase(semaphoreGroup.get(), "signal_import",				"Test signaling and importing the semaphore.",							testSemaphoreSignalImport,				externalType);
	addFunctionCase(semaphoreGroup.get(), "permanence",					"Test semaphores permanence.",											testSemaphorePermanence,				externalType);

	if (getHandleTypePermanence(externalType) == PERMANENCE_PERMANENT)
	{
		addFunctionCase(semaphoreGroup.get(), "signal_wait_import",			"Test signaling and then waiting for the the sepmahore.",				testSemaphoreSignalWaitImport,			externalType);
		addFunctionCase(semaphoreGroup.get(), "export_signal_import_wait",	"Test exporting, signaling, importing and waiting for the semaphore.",	testSemaphoreExportSignalImportWait,	externalType);
		addFunctionCase(semaphoreGroup.get(), "export_import_signal_wait",	"Test exporting, importing, signaling and waiting for the semaphore.",	testSemaphoreExportImportSignalWait,	externalType);
	}

	if (externalType == vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_FENCE_FD_BIT_KHX
		|| externalType == vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT_KHX)
	{
		// \note Not supported on WIN32 handles
		addFunctionCase(semaphoreGroup.get(), "export_multiple_times",	"Test exporting semaphore multiple times.",		testSemaphoreMultipleExports,	externalType);

		addFunctionCase(semaphoreGroup.get(), "dup",					"Test calling dup() on exported semaphore.",	testSemaphoreFdDup,				externalType);
		addFunctionCase(semaphoreGroup.get(), "dup2",					"Test calling dup2() on exported semaphore.",	testSemaphoreFdDup2,			externalType);
		addFunctionCase(semaphoreGroup.get(), "dup3",					"Test calling dup3() on exported semaphore.",	testSemaphoreFdDup3,			externalType);
		addFunctionCase(semaphoreGroup.get(), "send_over_socket",		"Test sending semaphore fd over socket.",		testSemaphoreFdSendOverSocket,	externalType);
	}

	return semaphoreGroup;
}

de::MovePtr<tcu::TestCaseGroup> createSemaphoreTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> semaphoreGroup (new tcu::TestCaseGroup(testCtx, "semaphore", "Tests for external semaphores."));

	semaphoreGroup->addChild(createSemaphoreTests(testCtx, vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_FENCE_FD_BIT_KHX).release());
	semaphoreGroup->addChild(createSemaphoreTests(testCtx, vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT_KHX).release());
	semaphoreGroup->addChild(createSemaphoreTests(testCtx, vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHX).release());
	semaphoreGroup->addChild(createSemaphoreTests(testCtx, vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT_KHX).release());

	return semaphoreGroup;
}

de::MovePtr<tcu::TestCaseGroup> createMemoryTests (tcu::TestContext& testCtx, vk::VkExternalMemoryHandleTypeFlagBitsKHX externalType)
{
	de::MovePtr<tcu::TestCaseGroup> group (new tcu::TestCaseGroup(testCtx, externalMemoryTypeToName(externalType), "Tests for external memory"));

	addFunctionCase(group.get(), "import_twice",						"Test importing memory object twice.",			testMemoryImportTwice,			MemoryTestConfig(externalType, false));
	addFunctionCase(group.get(), "import_twice_host_visible",			"Test importing memory object twice.",			testMemoryImportTwice,			MemoryTestConfig(externalType, true));

	addFunctionCase(group.get(), "import_multiple_times",				"Test importing memory object multiple times.",	testMemoryMultimpleImports,		MemoryTestConfig(externalType, false));
	addFunctionCase(group.get(), "import_multiple_times_host_visible",	"Test importing memory object multiple times.",	testMemoryMultimpleImports,		MemoryTestConfig(externalType, true));

	if (externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHX)
	{
		addFunctionCase(group.get(), "dup",									"Test calling dup() on exported memory.",	testMemoryFdDup,			MemoryTestConfig(externalType, false));
		addFunctionCase(group.get(), "dup_host_visible",					"Test calling dup() on exported memory.",	testMemoryFdDup,			MemoryTestConfig(externalType, true));

		addFunctionCase(group.get(), "dup2",								"Test calling dup2() on exported memory.",	testMemoryFdDup2,			MemoryTestConfig(externalType, false));
		addFunctionCase(group.get(), "dup2_host_visible",					"Test calling dup2() on exported memory.",	testMemoryFdDup2,			MemoryTestConfig(externalType, true));

		addFunctionCase(group.get(), "dup3",								"Test calling dup3() on exported memory.",	testMemoryFdDup3,			MemoryTestConfig(externalType, false));
		addFunctionCase(group.get(), "dup3_host_visible",					"Test calling dup3() on exported memory.",	testMemoryFdDup3,			MemoryTestConfig(externalType, true));

		addFunctionCase(group.get(), "send_over_socket",					"Test sending memory fd over socket.",		testMemoryFdSendOverSocket,	MemoryTestConfig(externalType, false));
		addFunctionCase(group.get(), "send_over_socket_host_visible",		"Test sending memory fd over socket.",		testMemoryFdSendOverSocket,	MemoryTestConfig(externalType, true));

		// \note Not supported on WIN32 handles
		addFunctionCase(group.get(), "export_multiple_times",				"Test exporting memory multiple times.",	testMemoryMultimpleExports,	MemoryTestConfig(externalType, false));
		addFunctionCase(group.get(), "export_multiple_times_host_visible",	"Test exporting memory multiple times.",	testMemoryMultimpleExports,	MemoryTestConfig(externalType, true));
	}

	addFunctionCase(group.get(), "buffer_info",						"External buffer memory info query.",						testBufferQueries,				externalType);
	addFunctionCase(group.get(), "buffer_bind_export_import_bind",	"Test binding, exporting, importing and binding buffer.",	testBufferBindExportImportBind,	externalType);
	addFunctionCase(group.get(), "buffer_export_bind_import_bind",	"Test exporting, binding, importing and binding buffer.",	testBufferExportBindImportBind,	externalType);
	addFunctionCase(group.get(), "buffer_export_import_bind_bind",	"Test exporting, importind and binding buffer.",			testBufferExportImportBindBind,	externalType);

	addFunctionCase(group.get(), "image_info",						"External buffer memory info query.",						testImageQueries,					externalType);
	addFunctionCase(group.get(), "image_bind_export_import_bind",	"Test binding, exporting, importing and binding buffer.",	testImageBindExportImportBind,		externalType);
	addFunctionCase(group.get(), "image_export_bind_import_bind",	"Test exporting, binding, importing and binding buffer.",	testImageExportBindImportBind,		externalType);
	addFunctionCase(group.get(), "image_export_import_bind_bind",	"Test exporting, importind and binding buffer.",			testImageExportImportBindBind,		externalType);

	return group;
}

de::MovePtr<tcu::TestCaseGroup> createMemoryTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group (new tcu::TestCaseGroup(testCtx, "memory", "Tests for external memory"));

	group->addChild(createMemoryTests(testCtx, vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHX).release());
	group->addChild(createMemoryTests(testCtx, vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHX).release());
	group->addChild(createMemoryTests(testCtx, vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT_KHX).release());

	return group;
}

} // anonymous

tcu::TestCaseGroup* createExternalMemoryTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group (new tcu::TestCaseGroup(testCtx, "external", "Tests for external Vulkan objects"));

	group->addChild(createSemaphoreTests(testCtx).release());
	group->addChild(createMemoryTests(testCtx).release());

	return group.release();
}

} // api
} // vkt
