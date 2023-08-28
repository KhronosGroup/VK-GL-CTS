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
#include "vktCustomInstancesDevices.hpp"
#include "../compute/vktComputeTestsUtil.hpp"

#include "vktTestCaseUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkPlatform.hpp"
#include "vkMemUtil.hpp"
#include "vkApiVersion.hpp"
#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"

#include "tcuTestLog.hpp"
#include "tcuCommandLine.hpp"

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
#	include <dxgi1_2.h>
#endif

#include <chrono>

using tcu::TestLog;
using namespace vkt::ExternalMemoryUtil;

namespace vkt
{
namespace api
{
namespace
{


template<typename T, int size>
T multiplyComponents (const tcu::Vector<T, size>& v)
{
	T accum = 1;
	for (int i = 0; i < size; ++i)
		accum *= v[i];
	return accum;
}

std::string getFormatCaseName (vk::VkFormat format)
{
	return de::toLower(de::toString(getFormatStr(format)).substr(10));
}

vk::VkMemoryDedicatedRequirements getMemoryDedicatedRequirements (const vk::DeviceInterface&	vkd,
																  vk::VkDevice					device,
																  vk::VkBuffer					buffer)
{
	const vk::VkBufferMemoryRequirementsInfo2	requirementInfo			=
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2,
		DE_NULL,
		buffer
	};
	vk::VkMemoryDedicatedRequirements			dedicatedRequirements	=
	{
		vk::VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS,
		DE_NULL,
		VK_FALSE,
		VK_FALSE
	};
	vk::VkMemoryRequirements2					requirements			=
	{
		vk::VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
		&dedicatedRequirements,
		{ 0u, 0u, 0u }
	};

	vkd.getBufferMemoryRequirements2(device, &requirementInfo, &requirements);

	return dedicatedRequirements;
}

vk::VkMemoryDedicatedRequirements getMemoryDedicatedRequirements (const vk::DeviceInterface&	vkd,
																  vk::VkDevice					device,
																  vk::VkImage					image)
{
	const vk::VkImageMemoryRequirementsInfo2	requirementInfo		=
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
		DE_NULL,
		image
	};
	vk::VkMemoryDedicatedRequirements		dedicatedRequirements	=
	{
		vk::VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS,
		DE_NULL,
		VK_FALSE,
		VK_FALSE
	};
	vk::VkMemoryRequirements2				requirements			=
	{
		vk::VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
		&dedicatedRequirements,
		{ 0u, 0u, 0u }
	};

	vkd.getImageMemoryRequirements2(device, &requirementInfo, &requirements);

	return dedicatedRequirements;
}

void writeHostMemory (const vk::DeviceInterface&	vkd,
					  vk::VkDevice					device,
					  vk::VkDeviceMemory			memory,
					  size_t						size,
					  const void*					data)
{
	void* const ptr = vk::mapMemory(vkd, device, memory, 0, size, 0);

	deMemcpy(ptr, data, size);

	flushMappedMemoryRange(vkd, device, memory, 0, VK_WHOLE_SIZE);

	vkd.unmapMemory(device, memory);
}

void checkHostMemory (const vk::DeviceInterface&	vkd,
					  vk::VkDevice					device,
					  vk::VkDeviceMemory			memory,
					  size_t						size,
					  const void*					data)
{
	void* const ptr = vk::mapMemory(vkd, device, memory, 0, size, 0);

	invalidateMappedMemoryRange(vkd, device, memory, 0, VK_WHOLE_SIZE);

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

std::vector<std::string> getInstanceExtensions (const deUint32 instanceVersion,
												const vk::VkExternalSemaphoreHandleTypeFlags	externalSemaphoreTypes,
												const vk::VkExternalMemoryHandleTypeFlags		externalMemoryTypes,
												const vk::VkExternalFenceHandleTypeFlags		externalFenceTypes)
{
	std::vector<std::string> instanceExtensions;

	if (!vk::isCoreInstanceExtension(instanceVersion, "VK_KHR_get_physical_device_properties2"))
		instanceExtensions.push_back("VK_KHR_get_physical_device_properties2");

	if (externalSemaphoreTypes != 0)
		if (!vk::isCoreInstanceExtension(instanceVersion, "VK_KHR_external_semaphore_capabilities"))
			instanceExtensions.push_back("VK_KHR_external_semaphore_capabilities");

	if (externalMemoryTypes != 0)
		if (!vk::isCoreInstanceExtension(instanceVersion, "VK_KHR_external_memory_capabilities"))
			instanceExtensions.push_back("VK_KHR_external_memory_capabilities");

	if (externalFenceTypes != 0)
		if (!vk::isCoreInstanceExtension(instanceVersion, "VK_KHR_external_fence_capabilities"))
			instanceExtensions.push_back("VK_KHR_external_fence_capabilities");

	return instanceExtensions;
}

CustomInstance createTestInstance (Context&										context,
								   const vk::VkExternalSemaphoreHandleTypeFlags	externalSemaphoreTypes,
								   const vk::VkExternalMemoryHandleTypeFlags	externalMemoryTypes,
								   const vk::VkExternalFenceHandleTypeFlags		externalFenceTypes)
{
	try
	{
		return vkt::createCustomInstanceWithExtensions(context, getInstanceExtensions(context.getUsedApiVersion(), externalSemaphoreTypes, externalMemoryTypes, externalFenceTypes));
	}
	catch (const vk::Error& error)
	{
		if (error.getError() == vk::VK_ERROR_EXTENSION_NOT_PRESENT)
			TCU_THROW(NotSupportedError, "Required extensions not supported");

		throw;
	}
}

vk::Move<vk::VkDevice> createTestDevice (const Context&									context,
										 const vk::PlatformInterface&					vkp,
										 vk::VkInstance									instance,
										 const vk::InstanceInterface&					vki,
										 vk::VkPhysicalDevice							physicalDevice,
										 const vk::VkExternalSemaphoreHandleTypeFlags	externalSemaphoreTypes,
										 const vk::VkExternalMemoryHandleTypeFlags		externalMemoryTypes,
										 const vk::VkExternalFenceHandleTypeFlags		externalFenceTypes,
										 deUint32										queueFamilyIndex,
										 bool											useDedicatedAllocs = false,
										 void * protectedFeatures = DE_NULL)
{
	const deUint32				apiVersion				= context.getUsedApiVersion();
	bool						useExternalSemaphore	= false;
	bool						useExternalFence		= false;
	bool						useExternalMemory		= false;
	std::vector<const char*>	deviceExtensions;

	if ((externalSemaphoreTypes
			& (vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT
				| vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT)) != 0)
	{
		deviceExtensions.push_back("VK_KHR_external_semaphore_fd");
		useExternalSemaphore = true;
	}

	if ((externalFenceTypes
			& (vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT
				| vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_FD_BIT)) != 0)
	{
		deviceExtensions.push_back("VK_KHR_external_fence_fd");
		useExternalFence = true;
	}

	if (useDedicatedAllocs)
	{
		if (!vk::isCoreDeviceExtension(apiVersion, "VK_KHR_dedicated_allocation"))
			deviceExtensions.push_back("VK_KHR_dedicated_allocation");
		if (!vk::isCoreDeviceExtension(apiVersion, "VK_KHR_get_memory_requirements2"))
			deviceExtensions.push_back("VK_KHR_get_memory_requirements2");
	}

	if ((externalMemoryTypes
			& (vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT
				| vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT)) != 0)
	{
		deviceExtensions.push_back("VK_KHR_external_memory_fd");
		useExternalMemory = true;
	}

	if ((externalMemoryTypes
			& vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT) != 0)
	{
		deviceExtensions.push_back("VK_EXT_external_memory_dma_buf");
		useExternalMemory = true;
	}

	if ((externalSemaphoreTypes
			& (vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT
				| vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT)) != 0)
	{
		deviceExtensions.push_back("VK_KHR_external_semaphore_win32");
		useExternalSemaphore = true;
	}

	if ((externalFenceTypes
			& (vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_BIT
				| vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT)) != 0)
	{
		deviceExtensions.push_back("VK_KHR_external_fence_win32");
		useExternalFence = true;
	}

	if (externalMemoryTypes & vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_ZIRCON_VMO_BIT_FUCHSIA)
	{
		deviceExtensions.push_back("VK_FUCHSIA_external_memory");
	}

	if (externalSemaphoreTypes & vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_ZIRCON_EVENT_BIT_FUCHSIA)
	{
		deviceExtensions.push_back("VK_FUCHSIA_external_semaphore");
	}

	if ((externalMemoryTypes
			& (vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT
			   | vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT
			   | vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT
			   | vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT
			   | vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_HEAP_BIT
			   | vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT)) != 0)
	{
		deviceExtensions.push_back("VK_KHR_external_memory_win32");
		useExternalMemory = true;
	}

	if ((externalMemoryTypes
		& vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID) != 0)
	{
		deviceExtensions.push_back("VK_ANDROID_external_memory_android_hardware_buffer");
		useExternalMemory = true;
		if (!vk::isCoreDeviceExtension(apiVersion, "VK_KHR_sampler_ycbcr_conversion"))
			deviceExtensions.push_back("VK_KHR_sampler_ycbcr_conversion");
		if (!vk::isCoreDeviceExtension(apiVersion, "VK_EXT_queue_family_foreign"))
			deviceExtensions.push_back("VK_EXT_queue_family_foreign");
	}

	if (useExternalSemaphore)
	{
		if (!vk::isCoreDeviceExtension(apiVersion, "VK_KHR_external_semaphore"))
			deviceExtensions.push_back("VK_KHR_external_semaphore");
	}

	if (useExternalFence)
	{
		if (!vk::isCoreDeviceExtension(apiVersion, "VK_KHR_external_fence"))
			deviceExtensions.push_back("VK_KHR_external_fence");
	}

	if (useExternalMemory)
	{
		if (!vk::isCoreDeviceExtension(apiVersion, "VK_KHR_external_memory"))
			deviceExtensions.push_back("VK_KHR_external_memory");
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
		protectedFeatures,
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
		return createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(), vkp, instance, vki, physicalDevice, &deviceCreateInfo);
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

uint32_t getMaxInvocations(const Context& context, uint32_t idx)
{
	const auto&	vki				= context.getInstanceInterface();
	const auto	physicalDevice	= context.getPhysicalDevice();
	const auto	properties		= vk::getPhysicalDeviceProperties(vki, physicalDevice);

	return properties.limits.maxComputeWorkGroupSize[idx];
}

void checkSemaphoreSupport (const vk::InstanceInterface&				vki,
							vk::VkPhysicalDevice						device,
							vk::VkExternalSemaphoreHandleTypeFlagBits	externalType)
{
	const vk::VkPhysicalDeviceExternalSemaphoreInfo	info		=
	{
		vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO,
		DE_NULL,
		externalType
	};
	vk::VkExternalSemaphoreProperties				properties	=
	{
		vk::VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES,
		DE_NULL,
		0u,
		0u,
		0u
	};

	vki.getPhysicalDeviceExternalSemaphoreProperties(device, &info, &properties);

	if ((properties.externalSemaphoreFeatures & vk::VK_EXTERNAL_SEMAPHORE_FEATURE_EXPORTABLE_BIT) == 0)
		TCU_THROW(NotSupportedError, "Semaphore doesn't support exporting in external type");

	if ((properties.externalSemaphoreFeatures & vk::VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT) == 0)
		TCU_THROW(NotSupportedError, "Semaphore doesn't support importing in external type");
}

void checkFenceSupport (const vk::InstanceInterface&			vki,
						vk::VkPhysicalDevice					device,
						vk::VkExternalFenceHandleTypeFlagBits	externalType)
{
	const vk::VkPhysicalDeviceExternalFenceInfo	info		=
	{
		vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_FENCE_INFO,
		DE_NULL,
		externalType
	};
	vk::VkExternalFenceProperties				properties	=
	{
		vk::VK_STRUCTURE_TYPE_EXTERNAL_FENCE_PROPERTIES,
		DE_NULL,
		0u,
		0u,
		0u
	};

	vki.getPhysicalDeviceExternalFenceProperties(device, &info, &properties);

	if ((properties.externalFenceFeatures & vk::VK_EXTERNAL_FENCE_FEATURE_EXPORTABLE_BIT) == 0)
		TCU_THROW(NotSupportedError, "Fence doesn't support exporting in external type");

	if ((properties.externalFenceFeatures & vk::VK_EXTERNAL_FENCE_FEATURE_IMPORTABLE_BIT) == 0)
		TCU_THROW(NotSupportedError, "Fence doesn't support importing in external type");
}

void checkBufferSupport (const vk::InstanceInterface&				vki,
						 vk::VkPhysicalDevice						device,
						 vk::VkExternalMemoryHandleTypeFlagBits		externalType,
						 vk::VkBufferViewCreateFlags				createFlag,
						 vk::VkBufferUsageFlags						usageFlag,
						 bool										dedicated)
{
	const vk::VkPhysicalDeviceExternalBufferInfo	info		=
	{
		vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_BUFFER_INFO,
		DE_NULL,

		createFlag,
		usageFlag,
		externalType
	};
	vk::VkExternalBufferProperties					properties	=
	{
		vk::VK_STRUCTURE_TYPE_EXTERNAL_BUFFER_PROPERTIES,
		DE_NULL,

		{ 0u, 0u, 0u }
	};

	vki.getPhysicalDeviceExternalBufferProperties(device, &info, &properties);

	if ((properties.externalMemoryProperties.externalMemoryFeatures & vk::VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT) == 0)
		TCU_THROW(NotSupportedError, "External handle type doesn't support exporting buffer");

	if ((properties.externalMemoryProperties.externalMemoryFeatures & vk::VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT) == 0)
		TCU_THROW(NotSupportedError, "External handle type doesn't support importing buffer");

	if (!dedicated && (properties.externalMemoryProperties.externalMemoryFeatures & vk::VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT) != 0)
		TCU_THROW(NotSupportedError, "External handle type requires dedicated allocation");
}

void checkImageSupport (const vk::InstanceInterface&						vki,
						 vk::VkPhysicalDevice								device,
						 vk::VkExternalMemoryHandleTypeFlagBits				externalType,
						 vk::VkImageViewCreateFlags							createFlag,
						 vk::VkImageUsageFlags								usageFlag,
						 vk::VkFormat										format,
						 vk::VkImageTiling									tiling,
						 bool												dedicated)
{
	const vk::VkPhysicalDeviceExternalImageFormatInfo	externalInfo		=
	{
		vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO,
		DE_NULL,
		externalType
	};
	const vk::VkPhysicalDeviceImageFormatInfo2			info				=
	{
		vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
		&externalInfo,

		format,
		vk::VK_IMAGE_TYPE_2D,
		tiling,
		usageFlag,
		createFlag,
	};
	vk::VkExternalImageFormatProperties					externalProperties	=
	{
		vk::VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES,
		DE_NULL,
		{ 0u, 0u, 0u }
	};
	vk::VkImageFormatProperties2						properties			=
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
		&externalProperties,
		{
			{ 0u, 0u, 0u },
			0u,
			0u,
			0u,
			0u
		}
	};

	vki.getPhysicalDeviceImageFormatProperties2(device, &info, &properties);

	if ((externalProperties.externalMemoryProperties.externalMemoryFeatures & vk::VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT) == 0)
		TCU_THROW(NotSupportedError, "External handle type doesn't support exporting image");

	if ((externalProperties.externalMemoryProperties.externalMemoryFeatures & vk::VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT) == 0)
		TCU_THROW(NotSupportedError, "External handle type doesn't support importing image");

	if (!dedicated && (externalProperties.externalMemoryProperties.externalMemoryFeatures & vk::VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT) != 0)
		TCU_THROW(NotSupportedError, "External handle type requires dedicated allocation");
}

void submitEmptySignal (const vk::DeviceInterface&	vkd,
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

void tuneWorkSizeYAndPrepareCommandBuffer(  const Context&						context,
											const vk::DeviceInterface&			vk,
											vk::VkDevice						device,
											vk::VkQueue							queue,
											vk::VkCommandBuffer					cmdBuffer,
											vk::VkDescriptorSet					descriptorSet,
											vk::VkPipelineLayout				pipelineLayout,
											vk::VkPipeline						computePipeline,
											vk::VkBufferMemoryBarrier			computeFinishBarrier,
											vk::VkEvent							event,
											tcu::UVec3*							maxWorkSize)


{
	// Have it be static so we don't need to do tuning every time, especially for "export_multiple_times" tests.
	static uint32_t yWorkSize	= 1;
	uint64_t		timeElapsed = 0;
	bool			bOutLoop	= false;

	const vk::Unique<vk::VkFence>	fence(vk::createFence(vk, device));

	const vk::VkCommandBufferBeginInfo cmdBufferBeginInfo =
	{
		vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		nullptr,
		vk::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		nullptr,
	};

	while (true) {
		VK_CHECK(vk.beginCommandBuffer(cmdBuffer, &cmdBufferBeginInfo));

		/*
		 * If the handle type is VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT_KHR, the spec allowed for implementations to return -1
		 * if the fence is already signaled. Previously, to avoid getting -1 in this case, this test had used vkCmdWaitEvents and
		 * vkSetEvent after submission to get a proper file descriptor before signaling but it's not invalid to call vkSetEvent
		 * after submission. So we just use vkCmdSetEvent and check the state of the event after submission to see if it's already
		 * signaled or an error happens while trying to get a file descriptor.
		 */
		vk.cmdSetEvent(cmdBuffer, event, vk::VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

		// And now we do a simple atomic calculation to avoid signalling instantly right after submit.
		vk.cmdBindPipeline(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
		//vk.cmdBindDescriptorSets(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
		vk.cmdBindDescriptorSets(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0u, 1u, &descriptorSet, 0u, nullptr);
		vk.cmdDispatch(cmdBuffer, maxWorkSize->x(), yWorkSize, maxWorkSize->z());
		vk.cmdPipelineBarrier(cmdBuffer, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1u, &computeFinishBarrier, 0, nullptr);
		vk.endCommandBuffer(cmdBuffer);

		if (bOutLoop)
			break;

		const vk::VkSubmitInfo submit =
		{
			vk::VK_STRUCTURE_TYPE_SUBMIT_INFO,
			nullptr,

			0u,
			nullptr,
			nullptr,

			1u,
			&cmdBuffer,

			0u,
			nullptr
		};

		auto		timeStart		= std::chrono::high_resolution_clock::now();

		VK_CHECK(vk.queueSubmit(queue, 1, &submit, (vk::VkFence)*fence));
		vk.waitForFences(device, 1u, &fence.get(), true, ~0ull);

		const auto	executionTime	= std::chrono::high_resolution_clock::now() - timeStart;
		auto		elapsed			= std::chrono::duration_cast<std::chrono::milliseconds>(executionTime);

		timeElapsed = elapsed.count();

		// we do loop until we get over 9 miliseconds as an execution time.
		if (elapsed.count() > 9)
		{
			bOutLoop = true;
			continue;
		}

		yWorkSize *= 2;

		if (yWorkSize > maxWorkSize->y())
		{
			yWorkSize = maxWorkSize->y();
			bOutLoop  = true;
		}

		vk.resetCommandBuffer(cmdBuffer, 0u);
		vk.resetFences(device, 1u, &*fence);
	};

	tcu::TestLog& log = context.getTestContext().getLog();
	log << tcu::TestLog::Message
		<< "Execution time to get a native file descriptor is " << timeElapsed << "ms with Y WorkSize " << yWorkSize
		<< tcu::TestLog::EndMessage;

	return;
}

void submitAtomicCalculationsAndGetSemaphoreNative (const Context&									context,
													const vk::DeviceInterface&						vk,
													vk::VkDevice									device,
													vk::Allocator&									alloc,
													vk::VkQueue										queue,
													deUint32										queueFamilyIndex,
													vk::VkSemaphore									semaphore,
													vk::VkExternalSemaphoreHandleTypeFlagBits		externalType,
													NativeHandle&									nativeHandle)
{
	const vk::Unique<vk::VkCommandPool>		cmdPool(createCommandPool(vk, device, vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex, nullptr));
	const vk::Unique<vk::VkCommandBuffer>	cmdBuffer(allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const vk::VkEventCreateInfo eventCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,
		nullptr,
		0u
	};

	const vk::Unique<vk::VkEvent>	event(createEvent(vk, device, &eventCreateInfo, nullptr));

	const uint32_t maxXWorkSize		= getMaxInvocations(context, 0);
	const uint32_t maxYWorkSize		= getMaxInvocations(context, 1);

	tcu::UVec3 workSize				= { maxXWorkSize, maxYWorkSize, 1u };
	const uint32_t workGroupCount	= multiplyComponents(workSize);

	const vk::VkDeviceSize			outputBufferSize =	sizeof(uint32_t) * workGroupCount;
	const vk::BufferWithMemory		outputBuffer		(vk, device, alloc, makeBufferCreateInfo(outputBufferSize, vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), vk::MemoryRequirement::Local);

	// Create a compute shader
	const vk::Unique<vk::VkShaderModule>	compShader(createShaderModule(vk, device, context.getBinaryCollection().get("compute"), 0u));

	// Create descriptorSetLayout
	vk::DescriptorSetLayoutBuilder layoutBuilder;
	layoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT);
	vk::Unique<vk::VkDescriptorSetLayout>	descriptorSetLayout(layoutBuilder.build(vk, device));

	// Create compute pipeline
	const vk::Unique<vk::VkPipelineLayout>	pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));
	const vk::Unique<vk::VkPipeline>		computePipeline(makeComputePipeline(vk, device, *pipelineLayout, *compShader));

	// Create descriptor pool
	const vk::Unique<vk::VkDescriptorPool>	descriptorPool(
		vk::DescriptorPoolBuilder()
		.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.build(vk, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1));


	const vk::Move<vk::VkDescriptorSet>		descriptorSet			(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));
	const vk::VkDescriptorBufferInfo		outputBufferInfo		= makeDescriptorBufferInfo(*outputBuffer, 0ull, outputBufferSize);
	const vk::VkBufferMemoryBarrier			computeFinishBarrier	= vk::makeBufferMemoryBarrier(vk::VK_ACCESS_SHADER_WRITE_BIT, vk::VK_ACCESS_HOST_READ_BIT, *outputBuffer, 0ull, outputBufferSize);

	vk::DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &outputBufferInfo)
		.update(vk, device);

	// Now start tuning work size of Y to have time enough to get a fd at the device.
	tuneWorkSizeYAndPrepareCommandBuffer(context, vk, device, queue, *cmdBuffer, *descriptorSet, *pipelineLayout, *computePipeline, computeFinishBarrier, *event, &workSize);

	const vk::VkSubmitInfo submit =
	{
		vk::VK_STRUCTURE_TYPE_SUBMIT_INFO,
		nullptr,

		0u,
		nullptr,
		nullptr,

		1u,
		&cmdBuffer.get(),

		1u,
		&semaphore
	};


	VK_CHECK(vk.queueSubmit(queue, 1, &submit, (vk::VkFence)0u));

	getSemaphoreNative(vk, device, semaphore, externalType, nativeHandle);

	// Allow -1, that is valid if signaled properly.
	if (nativeHandle.hasValidFd() && nativeHandle.getFd() == -1)
		TCU_CHECK(vk.getEventStatus(device, *event) == vk::VK_EVENT_SET);

	VK_CHECK(vk.queueWaitIdle(queue));
}

void submitEmptyWait (const vk::DeviceInterface&	vkd,
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

void submitEmptySignal (const vk::DeviceInterface&	vkd,
						vk::VkQueue					queue,
						vk::VkFence					fence)
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

		0u,
		DE_NULL
	};

	VK_CHECK(vkd.queueSubmit(queue, 1, &submit, fence));
}

void submitAtomicCalculationsAndGetFenceNative (const Context&								context,
												const vk::DeviceInterface&					vk,
												vk::VkDevice								device,
												vk::Allocator&								alloc,
												vk::VkQueue									queue,
												deUint32									queueFamilyIndex,
												vk::VkFence									fence,
												vk::VkExternalFenceHandleTypeFlagBits		externalType,
												NativeHandle&								nativeHandle,
												bool										expectFenceUnsignaled = true)
{
	const vk::Unique<vk::VkCommandPool>		cmdPool(createCommandPool(vk, device, vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex, nullptr));
	const vk::Unique<vk::VkCommandBuffer>	cmdBuffer(allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const vk::VkEventCreateInfo eventCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,
		DE_NULL,
		0u
	};

	const vk::Unique<vk::VkEvent>	event(createEvent(vk, device, &eventCreateInfo, DE_NULL));

	const uint32_t maxXWorkSize		= getMaxInvocations(context, 0);
	const uint32_t maxYWorkSize		= getMaxInvocations(context, 1);

	tcu::UVec3 workSize				= { maxXWorkSize, maxYWorkSize, 1u };
	const uint32_t workGroupCount	= multiplyComponents(workSize);

	const vk::VkDeviceSize			outputBufferSize =	sizeof(uint32_t) * workGroupCount;
	const vk::BufferWithMemory		outputBuffer		(vk, device, alloc, makeBufferCreateInfo(outputBufferSize, vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), vk::MemoryRequirement::Local);

	// Create a compute shader
	const vk::Unique<vk::VkShaderModule>	compShader(createShaderModule(vk, device, context.getBinaryCollection().get("compute"), 0u));

	// Create descriptorSetLayout
	vk::DescriptorSetLayoutBuilder layoutBuilder;
	layoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT);
	vk::Unique<vk::VkDescriptorSetLayout>	descriptorSetLayout(layoutBuilder.build(vk, device));

	// Create compute pipeline
	const vk::Unique<vk::VkPipelineLayout>	pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));
	const vk::Unique<vk::VkPipeline>		computePipeline(makeComputePipeline(vk, device, *pipelineLayout, *compShader));

	// Create descriptor pool
	const vk::Unique<vk::VkDescriptorPool>	descriptorPool(
		vk::DescriptorPoolBuilder()
		.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.build(vk, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1));


	const vk::Move<vk::VkDescriptorSet>		descriptorSet			(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));
	const vk::VkDescriptorBufferInfo		outputBufferInfo		= makeDescriptorBufferInfo(*outputBuffer, 0ull, outputBufferSize);
	const vk::VkBufferMemoryBarrier			computeFinishBarrier	= vk::makeBufferMemoryBarrier(vk::VK_ACCESS_SHADER_WRITE_BIT, vk::VK_ACCESS_HOST_READ_BIT, *outputBuffer, 0ull, outputBufferSize);

	vk::DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &outputBufferInfo)
		.update(vk, device);

	// Now start tuning work size of Y to have time enough to get a fd at the device.
	tuneWorkSizeYAndPrepareCommandBuffer(context, vk, device, queue, *cmdBuffer, *descriptorSet, *pipelineLayout, *computePipeline, computeFinishBarrier, *event, &workSize);

	const vk::VkSubmitInfo submit =
	{
		vk::VK_STRUCTURE_TYPE_SUBMIT_INFO,
		DE_NULL,

		0u,
		DE_NULL,
		DE_NULL,

		1u,
		&cmdBuffer.get(),

		0u,
		DE_NULL
	};

	VK_CHECK(vk.queueSubmit(queue, 1, &submit, fence));

	getFenceNative(vk, device, fence, externalType, nativeHandle, expectFenceUnsignaled);

	// Allow -1, that is valid if signaled properly.
	if (nativeHandle.hasValidFd() && nativeHandle.getFd() == -1)
		TCU_CHECK(vk.getEventStatus(device, *event) == vk::VK_EVENT_SET);

	VK_CHECK(vk.queueWaitIdle(queue));
}

struct TestSemaphoreQueriesParameters
{
	vk::VkSemaphoreType							semaphoreType;
	vk::VkExternalSemaphoreHandleTypeFlagBits	externalType;

	TestSemaphoreQueriesParameters (vk::VkSemaphoreType							semaphoreType_,
									vk::VkExternalSemaphoreHandleTypeFlagBits	externalType_)
		: semaphoreType	(semaphoreType_)
		, externalType	(externalType_)
	{}
};

tcu::TestStatus testSemaphoreQueries (Context& context, const TestSemaphoreQueriesParameters params)
{
	const CustomInstance				instance		(createTestInstance(context, params.externalType, 0u, 0u));
	const vk::InstanceDriver&			vki				(instance.getDriver());
	const vk::VkPhysicalDevice			device			(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));

	TestLog&							log				= context.getTestContext().getLog();

	const vk::VkSemaphoreTypeCreateInfo				semaphoreTypeInfo	=
	{
		vk::VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
		DE_NULL,
		params.semaphoreType,
		0,
	};
	const vk::VkPhysicalDeviceExternalSemaphoreInfo	info				=
	{
		vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO,
		&semaphoreTypeInfo,
		params.externalType
	};
	vk::VkExternalSemaphoreProperties				properties			=
	{
		vk::VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES,
		DE_NULL,
		0u,
		0u,
		0u
	};

	vki.getPhysicalDeviceExternalSemaphoreProperties(device, &info, &properties);
	log << TestLog::Message << properties << TestLog::EndMessage;

	TCU_CHECK(properties.pNext == DE_NULL);
	TCU_CHECK(properties.sType == vk::VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES);

	if (params.semaphoreType == vk::VK_SEMAPHORE_TYPE_TIMELINE)
	{
		context.requireDeviceFunctionality("VK_KHR_timeline_semaphore");

		if (properties.compatibleHandleTypes & vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT)
			return tcu::TestStatus::fail("Timeline semaphores are not compatible with SYNC_FD");

		if (properties.exportFromImportedHandleTypes & vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT)
			return tcu::TestStatus::fail("Timeline semaphores imported from SYNC_FD");
	}

	return tcu::TestStatus::pass("Pass");
}

struct SemaphoreTestConfig
{
													SemaphoreTestConfig	(vk::VkExternalSemaphoreHandleTypeFlagBits	externalType_,
																		 Permanence										permanence_)
		: externalType		(externalType_)
		, permanence		(permanence_)
	{
	}

	vk::VkExternalSemaphoreHandleTypeFlagBits	externalType;
	Permanence									permanence;
};

template<class TestConfig> void initProgramsToGetNativeFd(vk::SourceCollections& dst, const TestConfig)
{
	const tcu::IVec3 localSize = { 64, 1, 1 };

	std::ostringstream src;
	src << "#version 310 es\n"
		<< "layout (local_size_x = " << localSize.x() << ", local_size_y = " << localSize.y() << ", local_size_z = " << localSize.z() << ") in;\n"
		<< "layout(binding = 0) writeonly buffer Output {\n"
		<< "    uint values[];\n"
		<< "};\n"
		<< "\n"
		<< "void main (void) {\n"
		<< "    uint offset = gl_NumWorkGroups.x*gl_NumWorkGroups.y*gl_WorkGroupID.z + gl_NumWorkGroups.x*gl_WorkGroupID.y + gl_WorkGroupID.x;\n"
		<< "\n"
		<< "      atomicAdd(values[offset], 1u);\n"
		<< "}\n";

	dst.glslSources.add("compute") << glu::ComputeSource(src.str());
}

tcu::TestStatus testSemaphoreWin32Create (Context&					context,
										  const SemaphoreTestConfig	config)
{
#if (DE_OS == DE_OS_WIN32)
	const Transference					transference		(getHandelTypeTransferences(config.externalType));
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const CustomInstance				instance			(createTestInstance(context, config.externalType, 0u, 0u));
	const vk::InstanceDriver&			vki					(instance.getDriver());
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	checkSemaphoreSupport(vki, physicalDevice, config.externalType);

	{
		const vk::Unique<vk::VkDevice>					device			(createTestDevice(context, vkp, instance, vki, physicalDevice, config.externalType, 0u, 0u, queueFamilyIndex));
		const vk::DeviceDriver							vkd				(vkp, instance, *device, context.getUsedApiVersion());
		const vk::VkQueue								queue			(getQueue(vkd, *device, queueFamilyIndex));
		const vk::VkExportSemaphoreWin32HandleInfoKHR	win32ExportInfo	=
		{
			vk::VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,
			DE_NULL,

			(vk::pt::Win32SecurityAttributesPtr)DE_NULL,
			DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
			(vk::pt::Win32LPCWSTR)DE_NULL
		};
		const vk::VkExportSemaphoreCreateInfo			exportCreateInfo=
		{
			vk::VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO,
			&win32ExportInfo,
			(vk::VkExternalMemoryHandleTypeFlags)config.externalType
		};
		const vk::VkSemaphoreCreateInfo					createInfo		=
		{
			vk::VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			&exportCreateInfo,
			0u
		};
		const vk::Unique<vk::VkSemaphore>				semaphore		(vk::createSemaphore(vkd, *device, &createInfo));

		if (transference == TRANSFERENCE_COPY)
			submitEmptySignal(vkd, queue, *semaphore);

		NativeHandle									handleA;
		getSemaphoreNative(vkd, *device, *semaphore, config.externalType, handleA);

		{
			const vk::VkSemaphoreImportFlags			flags			= config.permanence == PERMANENCE_TEMPORARY ? vk::VK_SEMAPHORE_IMPORT_TEMPORARY_BIT : (vk::VkSemaphoreImportFlagBits)0u;
			const vk::Unique<vk::VkSemaphore>			semaphoreA		(createAndImportSemaphore(vkd, *device, config.externalType, handleA, flags));

			if (transference == TRANSFERENCE_COPY)
				submitEmptyWait(vkd, queue, *semaphoreA);
			else if (transference == TRANSFERENCE_REFERENCE)
			{
				submitEmptySignal(vkd, queue, *semaphore);
				submitEmptyWait(vkd, queue, *semaphoreA);
			}
			else
				DE_FATAL("Unknown transference.");

			VK_CHECK(vkd.queueWaitIdle(queue));
		}

		return tcu::TestStatus::pass("Pass");
	}
#else
	DE_UNREF(context);
	DE_UNREF(config);
	TCU_THROW(NotSupportedError, "Platform doesn't support win32 handles");
#endif
}

tcu::TestStatus testSemaphoreImportTwice (Context&					context,
										  const SemaphoreTestConfig	config)
{
	const Transference					transference		(getHandelTypeTransferences(config.externalType));
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const CustomInstance				instance			(createTestInstance(context, config.externalType, 0u, 0u));
	const vk::InstanceDriver&			vki					(instance.getDriver());
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	checkSemaphoreSupport(vki, physicalDevice, config.externalType);

	{
		const vk::Unique<vk::VkDevice>		device			(createTestDevice(context, vkp, instance, vki, physicalDevice, config.externalType, 0u, 0u, queueFamilyIndex));
		const vk::DeviceDriver				vkd				(vkp, instance, *device, context.getUsedApiVersion());
		vk::SimpleAllocator					alloc			(vkd, *device, vk::getPhysicalDeviceMemoryProperties(vki, physicalDevice));
		const vk::VkQueue					queue			(getQueue(vkd, *device, queueFamilyIndex));
		const vk::Unique<vk::VkSemaphore>	semaphore		(createExportableSemaphore(vkd, *device, config.externalType));
		NativeHandle						handleA;

		if (transference == TRANSFERENCE_COPY)
		{
			submitAtomicCalculationsAndGetSemaphoreNative(context, vkd, *device, alloc, queue, queueFamilyIndex, *semaphore, config.externalType, handleA);
			if (handleA.hasValidFd() && handleA.getFd() == -1)
				return tcu::TestStatus::pass("Pass: got -1 as a file descriptor, which is valid with a handle type of copy transference");
		}
		else
			getSemaphoreNative(vkd, *device, *semaphore, config.externalType, handleA);

		{
			NativeHandle						handleB		(handleA);
			const vk::VkSemaphoreImportFlags	flags		= config.permanence == PERMANENCE_TEMPORARY ? vk::VK_SEMAPHORE_IMPORT_TEMPORARY_BIT : (vk::VkSemaphoreImportFlagBits)0u;
			const vk::Unique<vk::VkSemaphore>	semaphoreA	(createAndImportSemaphore(vkd, *device, config.externalType, handleA, flags));
			const vk::Unique<vk::VkSemaphore>	semaphoreB	(createAndImportSemaphore(vkd, *device, config.externalType, handleB, flags));

			if (transference == TRANSFERENCE_COPY)
				submitEmptyWait(vkd, queue, *semaphoreA);
			else if (transference == TRANSFERENCE_REFERENCE)
			{
				submitEmptySignal(vkd, queue, *semaphoreA);
				submitEmptyWait(vkd, queue, *semaphoreB);
			}
			else
				DE_FATAL("Unknown transference.");

			VK_CHECK(vkd.queueWaitIdle(queue));
		}

		return tcu::TestStatus::pass("Pass");
	}
}

tcu::TestStatus testSemaphoreImportReimport (Context&					context,
											 const SemaphoreTestConfig	config)
{
	const Transference					transference		(getHandelTypeTransferences(config.externalType));
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const CustomInstance				instance			(createTestInstance(context, config.externalType, 0u, 0u));
	const vk::InstanceDriver&			vki					(instance.getDriver());
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	checkSemaphoreSupport(vki, physicalDevice, config.externalType);

	{
		const vk::Unique<vk::VkDevice>		device			(createTestDevice(context, vkp, instance, vki, physicalDevice, config.externalType, 0u, 0u, queueFamilyIndex));
		const vk::DeviceDriver				vkd				(vkp, instance, *device, context.getUsedApiVersion());
		vk::SimpleAllocator					alloc			(vkd, *device, vk::getPhysicalDeviceMemoryProperties(vki, physicalDevice));
		const vk::VkQueue					queue			(getQueue(vkd, *device, queueFamilyIndex));

		const vk::Unique<vk::VkSemaphore>	semaphoreA		(createExportableSemaphore(vkd, *device, config.externalType));
		NativeHandle						handleA;

		if (transference == TRANSFERENCE_COPY)
		{
			submitAtomicCalculationsAndGetSemaphoreNative(context, vkd, *device, alloc, queue, queueFamilyIndex, *semaphoreA, config.externalType, handleA);
			if (handleA.hasValidFd() && handleA.getFd() == -1)
				return tcu::TestStatus::pass("Pass: got -1 as a file descriptor, which is valid with a handle type of copy transference");
		}
		else
			getSemaphoreNative(vkd, *device, *semaphoreA, config.externalType, handleA);

		NativeHandle						handleB		(handleA);
		const vk::VkSemaphoreImportFlags	flags		= config.permanence == PERMANENCE_TEMPORARY ? vk::VK_SEMAPHORE_IMPORT_TEMPORARY_BIT : (vk::VkSemaphoreImportFlagBits)0u;
		const vk::Unique<vk::VkSemaphore>	semaphoreB	(createAndImportSemaphore(vkd, *device, config.externalType, handleA, flags));

		importSemaphore(vkd, *device, *semaphoreB, config.externalType, handleB, flags);

		if (transference == TRANSFERENCE_COPY)
			submitEmptyWait(vkd, queue, *semaphoreB);
		else if (transference == TRANSFERENCE_REFERENCE)
		{
			submitEmptySignal(vkd, queue, *semaphoreA);
			submitEmptyWait(vkd, queue, *semaphoreB);
		}
		else
			DE_FATAL("Unknown transference.");

		VK_CHECK(vkd.queueWaitIdle(queue));

		return tcu::TestStatus::pass("Pass");
	}
}

tcu::TestStatus testSemaphoreSignalExportImportWait (Context&					context,
													 const SemaphoreTestConfig	config)
{
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const CustomInstance				instance			(createTestInstance(context, config.externalType, 0u, 0u));
	const vk::InstanceDriver&			vki					(instance.getDriver());
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));
	const Transference					transference		(getHandelTypeTransferences(config.externalType));

	checkSemaphoreSupport(vki, physicalDevice, config.externalType);

	{
		const vk::Unique<vk::VkDevice>		device				(createTestDevice(context, vkp, instance, vki, physicalDevice, config.externalType, 0u, 0u, queueFamilyIndex));
		const vk::DeviceDriver				vkd					(vkp, instance, *device, context.getUsedApiVersion());
		vk::SimpleAllocator					alloc				(vkd, *device, vk::getPhysicalDeviceMemoryProperties(vki, physicalDevice));
		const vk::VkQueue					queue				(getQueue(vkd, *device, queueFamilyIndex));
		const vk::Unique<vk::VkSemaphore>	semaphoreA			(createExportableSemaphore(vkd, *device, config.externalType));
		{
			NativeHandle	handle;

			submitAtomicCalculationsAndGetSemaphoreNative(context, vkd, *device, alloc, queue, queueFamilyIndex, *semaphoreA, config.externalType, handle);
			if (transference == TRANSFERENCE_COPY && handle.hasValidFd() && handle.getFd() == -1)
				return tcu::TestStatus::pass("Pass: got -1 as a file descriptor, which is valid with a handle type of copy transference");

			{
				const vk::VkSemaphoreImportFlags	flags		= config.permanence == PERMANENCE_TEMPORARY ? vk::VK_SEMAPHORE_IMPORT_TEMPORARY_BIT : (vk::VkSemaphoreImportFlagBits)0u;
				const vk::Unique<vk::VkSemaphore>	semaphoreB	(createAndImportSemaphore(vkd, *device, config.externalType, handle, flags));
				submitEmptyWait(vkd, queue, *semaphoreB);

				VK_CHECK(vkd.queueWaitIdle(queue));
			}
		}

		return tcu::TestStatus::pass("Pass");
	}
}

tcu::TestStatus testSemaphoreExportSignalImportWait (Context&					context,
													 const SemaphoreTestConfig	config)
{
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const CustomInstance				instance			(createTestInstance(context, config.externalType, 0u, 0u));
	const vk::InstanceDriver&			vki					(instance.getDriver());
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));
	const vk::VkSemaphoreImportFlags	flags				= config.permanence == PERMANENCE_TEMPORARY ? vk::VK_SEMAPHORE_IMPORT_TEMPORARY_BIT : (vk::VkSemaphoreImportFlagBits)0u;

	DE_ASSERT(getHandelTypeTransferences(config.externalType) == TRANSFERENCE_REFERENCE);
	checkSemaphoreSupport(vki, physicalDevice, config.externalType);

	{
		const vk::Unique<vk::VkDevice>		device				(createTestDevice(context, vkp, instance, vki, physicalDevice, config.externalType, 0u, 0u, queueFamilyIndex));
		const vk::DeviceDriver				vkd					(vkp, instance, *device, context.getUsedApiVersion());
		const vk::VkQueue					queue				(getQueue(vkd, *device, queueFamilyIndex));

		const vk::Unique<vk::VkSemaphore>	semaphoreA			(createExportableSemaphore(vkd, *device, config.externalType));
		NativeHandle						handle;

		getSemaphoreNative(vkd, *device, *semaphoreA, config.externalType, handle);

		submitEmptySignal(vkd, queue, *semaphoreA);
		{
			{
				const vk::Unique<vk::VkSemaphore>	semaphoreB	(createAndImportSemaphore(vkd, *device, config.externalType, handle, flags));

				submitEmptyWait(vkd, queue, *semaphoreB);
				VK_CHECK(vkd.queueWaitIdle(queue));
			}
		}

		return tcu::TestStatus::pass("Pass");
	}
}

tcu::TestStatus testSemaphoreExportImportSignalWait (Context&					context,
													 const SemaphoreTestConfig	config)
{
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const CustomInstance				instance			(createTestInstance(context, config.externalType, 0u, 0u));
	const vk::InstanceDriver&			vki					(instance.getDriver());
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	DE_ASSERT(getHandelTypeTransferences(config.externalType) == TRANSFERENCE_REFERENCE);
	checkSemaphoreSupport(vki, physicalDevice, config.externalType);

	{
		const vk::VkSemaphoreImportFlags	flags		= config.permanence == PERMANENCE_TEMPORARY ? vk::VK_SEMAPHORE_IMPORT_TEMPORARY_BIT : (vk::VkSemaphoreImportFlagBits)0u;
		const vk::Unique<vk::VkDevice>		device		(createTestDevice(context, vkp, instance, vki, physicalDevice, config.externalType, 0u, 0u, queueFamilyIndex));
		const vk::DeviceDriver				vkd			(vkp, instance, *device, context.getUsedApiVersion());
		const vk::VkQueue					queue		(getQueue(vkd, *device, queueFamilyIndex));

		const vk::Unique<vk::VkSemaphore>	semaphoreA	(createExportableSemaphore(vkd, *device, config.externalType));
		NativeHandle						handle;

		getSemaphoreNative(vkd, *device, *semaphoreA, config.externalType, handle);

		const vk::Unique<vk::VkSemaphore>	semaphoreB	(createAndImportSemaphore(vkd, *device, config.externalType, handle, flags));

		submitEmptySignal(vkd, queue, *semaphoreA);
		submitEmptyWait(vkd, queue, *semaphoreB);

		VK_CHECK(vkd.queueWaitIdle(queue));

		return tcu::TestStatus::pass("Pass");
	}
}

tcu::TestStatus testSemaphoreSignalImport (Context&						context,
										   const SemaphoreTestConfig	config)
{
	const Transference					transference		(getHandelTypeTransferences(config.externalType));
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const CustomInstance				instance			(createTestInstance(context, config.externalType, 0u, 0u));
	const vk::InstanceDriver&			vki					(instance.getDriver());
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	checkSemaphoreSupport(vki, physicalDevice, config.externalType);

	{
		const vk::VkSemaphoreImportFlags	flags		= config.permanence == PERMANENCE_TEMPORARY ? vk::VK_SEMAPHORE_IMPORT_TEMPORARY_BIT : (vk::VkSemaphoreImportFlagBits)0u;
		const vk::Unique<vk::VkDevice>		device		(createTestDevice(context, vkp, instance, vki, physicalDevice, config.externalType, 0u, 0u, queueFamilyIndex));
		const vk::DeviceDriver				vkd			(vkp, instance, *device, context.getUsedApiVersion());
		vk::SimpleAllocator					alloc		(vkd, *device, vk::getPhysicalDeviceMemoryProperties(vki, physicalDevice));
		const vk::VkQueue					queue		(getQueue(vkd, *device, queueFamilyIndex));

		const vk::Unique<vk::VkSemaphore>	semaphoreA	(createExportableSemaphore(vkd, *device, config.externalType));
		const vk::Unique<vk::VkSemaphore>	semaphoreB	(createSemaphore(vkd, *device));
		NativeHandle						handle;

		submitEmptySignal(vkd, queue, *semaphoreB);
		VK_CHECK(vkd.queueWaitIdle(queue));

		if (transference == TRANSFERENCE_COPY)
		{
			submitAtomicCalculationsAndGetSemaphoreNative(context, vkd, *device, alloc, queue, queueFamilyIndex, *semaphoreA, config.externalType, handle);
			if (handle.hasValidFd() && handle.getFd() == -1)
				return tcu::TestStatus::pass("Pass: got -1 as a file descriptor, which is valid with a handle type of copy transference");
		}
		else
			getSemaphoreNative(vkd, *device, *semaphoreA, config.externalType, handle);

		importSemaphore(vkd, *device, *semaphoreB, config.externalType, handle, flags);

		if (transference == TRANSFERENCE_COPY)
			submitEmptyWait(vkd, queue, *semaphoreB);
		else if (transference == TRANSFERENCE_REFERENCE)
		{
			submitEmptySignal(vkd, queue, *semaphoreA);
			submitEmptyWait(vkd, queue, *semaphoreB);
		}
		else
			DE_FATAL("Unknown transference.");

		VK_CHECK(vkd.queueWaitIdle(queue));

		return tcu::TestStatus::pass("Pass");
	}
}

tcu::TestStatus testSemaphoreSignalWaitImport (Context&						context,
											   const SemaphoreTestConfig	config)
{
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const CustomInstance				instance			(createTestInstance(context, config.externalType, 0u, 0u));
	const vk::InstanceDriver&			vki					(instance.getDriver());
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	checkSemaphoreSupport(vki, physicalDevice, config.externalType);

	{
		const vk::VkSemaphoreImportFlags	flags		= config.permanence == PERMANENCE_TEMPORARY ? vk::VK_SEMAPHORE_IMPORT_TEMPORARY_BIT : (vk::VkSemaphoreImportFlagBits)0u;
		const vk::Unique<vk::VkDevice>		device		(createTestDevice(context, vkp, instance, vki, physicalDevice, config.externalType, 0u, 0u, queueFamilyIndex));
		const vk::DeviceDriver				vkd			(vkp, instance, *device, context.getUsedApiVersion());
		const vk::VkQueue					queue		(getQueue(vkd, *device, queueFamilyIndex));

		const vk::Unique<vk::VkSemaphore>	semaphoreA	(createExportableSemaphore(vkd, *device, config.externalType));
		const vk::Unique<vk::VkSemaphore>	semaphoreB	(createSemaphore(vkd, *device));
		NativeHandle						handle;

		getSemaphoreNative(vkd, *device, *semaphoreA, config.externalType, handle);

		submitEmptySignal(vkd, queue, *semaphoreB);
		submitEmptyWait(vkd, queue, *semaphoreB);

		VK_CHECK(vkd.queueWaitIdle(queue));

		importSemaphore(vkd, *device, *semaphoreB, config.externalType, handle, flags);

		submitEmptySignal(vkd, queue, *semaphoreA);
		submitEmptyWait(vkd, queue, *semaphoreB);

		VK_CHECK(vkd.queueWaitIdle(queue));

		return tcu::TestStatus::pass("Pass");
	}
}

tcu::TestStatus testSemaphoreImportSyncFdSignaled (Context&						context,
												   const SemaphoreTestConfig	config)
{
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const CustomInstance				instance			(createTestInstance(context, config.externalType, 0u, 0u));
	const vk::InstanceDriver&			vki					(instance.getDriver());
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));
	const vk::VkSemaphoreImportFlags	flags				= config.permanence == PERMANENCE_TEMPORARY ? vk::VK_SEMAPHORE_IMPORT_TEMPORARY_BIT : (vk::VkSemaphoreImportFlagBits)0u;

	checkSemaphoreSupport(vki, physicalDevice, config.externalType);

	{
		const vk::Unique<vk::VkDevice>		device		(createTestDevice(context, vkp, instance, vki, physicalDevice, config.externalType, 0u, 0u, queueFamilyIndex));
		const vk::DeviceDriver				vkd			(vkp, instance, *device, context.getUsedApiVersion());
		const vk::VkQueue					queue		(getQueue(vkd, *device, queueFamilyIndex));
		NativeHandle						handle		= -1;
		const vk::Unique<vk::VkSemaphore>	semaphore	(createAndImportSemaphore(vkd, *device, config.externalType, handle, flags));

		submitEmptyWait(vkd, queue, *semaphore);
		VK_CHECK(vkd.queueWaitIdle(queue));

		return tcu::TestStatus::pass("Pass");
	}
}

tcu::TestStatus testSemaphoreMultipleExports (Context&					context,
											  const SemaphoreTestConfig	config)
{
	const size_t						exportCount			= 1024;
	const Transference					transference		(getHandelTypeTransferences(config.externalType));
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const CustomInstance				instance			(createTestInstance(context, config.externalType, 0u, 0u));
	const vk::InstanceDriver&			vki					(instance.getDriver());
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	checkSemaphoreSupport(vki, physicalDevice, config.externalType);

	{
		const vk::Unique<vk::VkDevice>		device		(createTestDevice(context, vkp, instance, vki, physicalDevice, config.externalType, 0u, 0u, queueFamilyIndex));
		const vk::DeviceDriver				vkd			(vkp, instance, *device, context.getUsedApiVersion());
		vk::SimpleAllocator					alloc		(vkd, *device, vk::getPhysicalDeviceMemoryProperties(vki, physicalDevice));
		const vk::VkQueue					queue		(getQueue(vkd, *device, queueFamilyIndex));
		const vk::Unique<vk::VkSemaphore>	semaphore	(createExportableSemaphore(vkd, *device, config.externalType));

		for (size_t exportNdx = 0; exportNdx < exportCount; exportNdx++)
		{
			NativeHandle handle;

			// Need to touch watchdog due to how long one iteration takes
			context.getTestContext().touchWatchdog();

			if (transference == TRANSFERENCE_COPY)
			{
				submitAtomicCalculationsAndGetSemaphoreNative(context, vkd, *device, alloc, queue, queueFamilyIndex, *semaphore, config.externalType, handle);
				if (handle.hasValidFd() && handle.getFd() == -1)
					return tcu::TestStatus::pass("Pass: got -1 as a file descriptor, which is valid with a handle type of copy transference");
			}
			else
				getSemaphoreNative(vkd, *device, *semaphore, config.externalType, handle);
		}

		submitEmptySignal(vkd, queue, *semaphore);
		submitEmptyWait(vkd, queue, *semaphore);

		VK_CHECK(vkd.queueWaitIdle(queue));
	}

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus testSemaphoreMultipleImports (Context&					context,
											  const SemaphoreTestConfig	config)
{
	const size_t						importCount			= 4 * 1024;
	const Transference					transference		(getHandelTypeTransferences(config.externalType));
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const CustomInstance				instance			(createTestInstance(context, config.externalType, 0u, 0u));
	const vk::InstanceDriver&			vki					(instance.getDriver());
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	checkSemaphoreSupport(vki, physicalDevice, config.externalType);

	{
		const vk::VkSemaphoreImportFlags	flags		= config.permanence == PERMANENCE_TEMPORARY ? vk::VK_SEMAPHORE_IMPORT_TEMPORARY_BIT : (vk::VkSemaphoreImportFlagBits)0u;
		const vk::Unique<vk::VkDevice>		device		(createTestDevice(context, vkp, instance, vki, physicalDevice, config.externalType, 0u, 0u, queueFamilyIndex));
		const vk::DeviceDriver				vkd			(vkp, instance, *device, context.getUsedApiVersion());
		vk::SimpleAllocator					alloc		(vkd, *device, vk::getPhysicalDeviceMemoryProperties(vki, physicalDevice));
		const vk::VkQueue					queue		(getQueue(vkd, *device, queueFamilyIndex));
		const vk::Unique<vk::VkSemaphore>	semaphoreA	(createExportableSemaphore(vkd, *device, config.externalType));
		NativeHandle						handleA;

		if (transference == TRANSFERENCE_COPY)
		{
			submitAtomicCalculationsAndGetSemaphoreNative(context, vkd, *device, alloc, queue, queueFamilyIndex, *semaphoreA, config.externalType, handleA);
			if (handleA.hasValidFd() && handleA.getFd() == -1)
				return tcu::TestStatus::pass("Pass: got -1 as a file descriptor, which is valid with a handle type of copy transference");
		}
		else
			getSemaphoreNative(vkd, *device, *semaphoreA, config.externalType, handleA);

		for (size_t importNdx = 0; importNdx < importCount; importNdx++)
		{
			NativeHandle						handleB		(handleA);
			const vk::Unique<vk::VkSemaphore>	semaphoreB	(createAndImportSemaphore(vkd, *device, config.externalType, handleB, flags));
		}

		if (transference == TRANSFERENCE_COPY)
		{
			importSemaphore(vkd, *device, *semaphoreA, config.externalType, handleA, flags);
			submitEmptyWait(vkd, queue, *semaphoreA);
		}
		else if (transference == TRANSFERENCE_REFERENCE)
		{
			submitEmptySignal(vkd, queue, *semaphoreA);
			submitEmptyWait(vkd, queue, *semaphoreA);
		}
		else
			DE_FATAL("Unknown transference.");

		VK_CHECK(vkd.queueWaitIdle(queue));
	}

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus testSemaphoreTransference (Context&						context,
										   const SemaphoreTestConfig	config)
{
	const Transference					transference		(getHandelTypeTransferences(config.externalType));
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const CustomInstance				instance			(createTestInstance(context, config.externalType, 0u, 0u));
	const vk::InstanceDriver&			vki					(instance.getDriver());
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	checkSemaphoreSupport(vki, physicalDevice, config.externalType);

	{
		const vk::VkSemaphoreImportFlags	flags		= config.permanence == PERMANENCE_TEMPORARY ? vk::VK_SEMAPHORE_IMPORT_TEMPORARY_BIT : (vk::VkSemaphoreImportFlagBits)0u;
		const vk::Unique<vk::VkDevice>		device		(createTestDevice(context,  vkp, instance, vki, physicalDevice, config.externalType, 0u, 0u, queueFamilyIndex));
		const vk::DeviceDriver				vkd			(vkp, instance, *device, context.getUsedApiVersion());
		vk::SimpleAllocator					alloc		(vkd, *device, vk::getPhysicalDeviceMemoryProperties(vki, physicalDevice));
		const vk::VkQueue					queue		(getQueue(vkd, *device, queueFamilyIndex));

		const vk::Unique<vk::VkSemaphore>	semaphoreA	(createExportableSemaphore(vkd, *device, config.externalType));
		NativeHandle						handle;

		submitAtomicCalculationsAndGetSemaphoreNative(context, vkd, *device, alloc, queue, queueFamilyIndex, *semaphoreA, config.externalType, handle);
		if (transference == TRANSFERENCE_COPY && handle.hasValidFd() && handle.getFd() == -1)
			return tcu::TestStatus::pass("Pass: got -1 as a file descriptor, which is valid with a handle type of copy transference");

		{
			const vk::Unique<vk::VkSemaphore>	semaphoreB			(createAndImportSemaphore(vkd, *device, config.externalType, handle, flags));

			if (config.permanence == PERMANENCE_PERMANENT)
			{
				if (transference == TRANSFERENCE_COPY)
				{
					submitEmptySignal(vkd, queue, *semaphoreA);
					submitEmptyWait(vkd, queue, *semaphoreB);
					VK_CHECK(vkd.queueWaitIdle(queue));

					submitEmptySignal(vkd, queue, *semaphoreB);

					submitEmptyWait(vkd, queue, *semaphoreA);
					submitEmptyWait(vkd, queue, *semaphoreB);
					VK_CHECK(vkd.queueWaitIdle(queue));
				}
				else if (transference== TRANSFERENCE_REFERENCE)
				{
					submitEmptyWait(vkd, queue, *semaphoreB);
					VK_CHECK(vkd.queueWaitIdle(queue));

					submitEmptySignal(vkd, queue, *semaphoreA);
					submitEmptyWait(vkd, queue, *semaphoreB);

					submitEmptySignal(vkd, queue, *semaphoreB);
					submitEmptyWait(vkd, queue, *semaphoreA);
					VK_CHECK(vkd.queueWaitIdle(queue));
				}
				else
					DE_FATAL("Unknown transference.");
			}
			else if (config.permanence == PERMANENCE_TEMPORARY)
			{
				if (transference == TRANSFERENCE_COPY)
				{
					submitEmptySignal(vkd, queue, *semaphoreA);
					submitEmptyWait(vkd, queue, *semaphoreB);
					VK_CHECK(vkd.queueWaitIdle(queue));

					submitEmptySignal(vkd, queue, *semaphoreB);

					submitEmptyWait(vkd, queue, *semaphoreA);
					submitEmptyWait(vkd, queue, *semaphoreB);
					VK_CHECK(vkd.queueWaitIdle(queue));
				}
				else if (transference== TRANSFERENCE_REFERENCE)
				{
					submitEmptyWait(vkd, queue, *semaphoreB);
					VK_CHECK(vkd.queueWaitIdle(queue));

					submitEmptySignal(vkd, queue, *semaphoreA);
					submitEmptySignal(vkd, queue, *semaphoreB);

					submitEmptyWait(vkd, queue, *semaphoreB);
					submitEmptyWait(vkd, queue, *semaphoreA);
					VK_CHECK(vkd.queueWaitIdle(queue));
				}
				else
					DE_FATAL("Unknown transference.");
			}
			else
				DE_FATAL("Unknown permanence.");
		}

		return tcu::TestStatus::pass("Pass");
	}
}

tcu::TestStatus testSemaphoreFdDup (Context&					context,
									const SemaphoreTestConfig	config)
{
#if (DE_OS == DE_OS_ANDROID) || (DE_OS == DE_OS_UNIX)
	const Transference					transference		(getHandelTypeTransferences(config.externalType));
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const CustomInstance				instance			(createTestInstance(context, config.externalType, 0u, 0u));
	const vk::InstanceDriver&			vki					(instance.getDriver());
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	checkSemaphoreSupport(vki, physicalDevice, config.externalType);

	{
		const vk::VkSemaphoreImportFlags	flags		= config.permanence == PERMANENCE_TEMPORARY ? vk::VK_SEMAPHORE_IMPORT_TEMPORARY_BIT : (vk::VkSemaphoreImportFlagBits)0u;
		const vk::Unique<vk::VkDevice>		device		(createTestDevice(context, vkp, instance, vki, physicalDevice, config.externalType, 0u, 0u, queueFamilyIndex));
		const vk::DeviceDriver				vkd			(vkp, instance, *device, context.getUsedApiVersion());
		vk::SimpleAllocator					alloc		(vkd, *device, vk::getPhysicalDeviceMemoryProperties(vki, physicalDevice));
		const vk::VkQueue					queue		(getQueue(vkd, *device, queueFamilyIndex));

		TestLog&							log			= context.getTestContext().getLog();
		const vk::Unique<vk::VkSemaphore>	semaphoreA	(createExportableSemaphore(vkd, *device, config.externalType));

		{
			NativeHandle		fd;

			if (transference == TRANSFERENCE_COPY)
			{
				submitAtomicCalculationsAndGetSemaphoreNative(context, vkd, *device, alloc, queue, queueFamilyIndex, *semaphoreA, config.externalType, fd);
				if (fd.getFd() == -1)
					return tcu::TestStatus::pass("Pass: got -1 as a file descriptor, which is valid with a handle type of copy transference");
			}
			else
				getSemaphoreNative(vkd, *device, *semaphoreA, config.externalType, fd);

			NativeHandle		newFd	(dup(fd.getFd()));

			if (newFd.getFd() < 0)
				log << TestLog::Message << "dup() failed: '" << strerror(errno) << "'" << TestLog::EndMessage;

			TCU_CHECK_MSG(newFd.getFd() >= 0, "Failed to call dup() for semaphores fd");

			{
				const vk::Unique<vk::VkSemaphore> semaphoreB (createAndImportSemaphore(vkd, *device, config.externalType, newFd, flags));

				if (transference == TRANSFERENCE_COPY)
					submitEmptyWait(vkd, queue, *semaphoreB);
				else if (transference == TRANSFERENCE_REFERENCE)
				{
					submitEmptySignal(vkd, queue, *semaphoreA);
					submitEmptyWait(vkd, queue, *semaphoreB);
				}
				else
					DE_FATAL("Unknown permanence.");

				VK_CHECK(vkd.queueWaitIdle(queue));
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

tcu::TestStatus testSemaphoreFdDup2 (Context&					context,
									 const SemaphoreTestConfig	config)
{
#if (DE_OS == DE_OS_ANDROID) || (DE_OS == DE_OS_UNIX)
	const Transference					transference		(getHandelTypeTransferences(config.externalType));
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const CustomInstance				instance			(createTestInstance(context, config.externalType, 0u, 0u));
	const vk::InstanceDriver&			vki					(instance.getDriver());
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	checkSemaphoreSupport(vki, physicalDevice, config.externalType);

	{
		const vk::VkSemaphoreImportFlags	flags		= config.permanence == PERMANENCE_TEMPORARY ? vk::VK_SEMAPHORE_IMPORT_TEMPORARY_BIT : (vk::VkSemaphoreImportFlagBits)0u;
		const vk::Unique<vk::VkDevice>		device		(createTestDevice(context, vkp, instance, vki, physicalDevice, config.externalType, 0u, 0u, queueFamilyIndex));
		const vk::DeviceDriver				vkd			(vkp, instance, *device, context.getUsedApiVersion());
		vk::SimpleAllocator					alloc		(vkd, *device, vk::getPhysicalDeviceMemoryProperties(vki, physicalDevice));
		const vk::VkQueue					queue		(getQueue(vkd, *device, queueFamilyIndex));

		TestLog&							log			= context.getTestContext().getLog();
		const vk::Unique<vk::VkSemaphore>	semaphoreA	(createExportableSemaphore(vkd, *device, config.externalType));
		const vk::Unique<vk::VkSemaphore>	semaphoreB	(createExportableSemaphore(vkd, *device, config.externalType));

		{
			NativeHandle		fd, secondFd;

			if (transference == TRANSFERENCE_COPY)
			{
				submitAtomicCalculationsAndGetSemaphoreNative(context, vkd, *device, alloc, queue, queueFamilyIndex, *semaphoreA, config.externalType, fd);
				submitAtomicCalculationsAndGetSemaphoreNative(context, vkd, *device, alloc, queue, queueFamilyIndex, *semaphoreB, config.externalType, secondFd);
				if (fd.getFd() == -1 || secondFd.getFd() == -1)
					return tcu::TestStatus::pass("Pass: got -1 as a file descriptor, which is valid with a handle type of copy transference");
			}
			else
			{
				getSemaphoreNative(vkd, *device, *semaphoreA, config.externalType, fd);
				getSemaphoreNative(vkd, *device, *semaphoreB, config.externalType, secondFd);
			}

			int					newFd		(dup2(fd.getFd(), secondFd.getFd()));

			if (newFd < 0)
				log << TestLog::Message << "dup2() failed: '" << strerror(errno) << "'" << TestLog::EndMessage;

			TCU_CHECK_MSG(newFd >= 0, "Failed to call dup2() for fences fd");

			{
				const vk::Unique<vk::VkSemaphore> semaphoreC (createAndImportSemaphore(vkd, *device, config.externalType, secondFd, flags));

				if (transference == TRANSFERENCE_COPY)
					submitEmptyWait(vkd, queue, *semaphoreC);
				else if (transference == TRANSFERENCE_REFERENCE)
				{
					submitEmptySignal(vkd, queue, *semaphoreA);
					submitEmptyWait(vkd, queue, *semaphoreC);
				}
				else
					DE_FATAL("Unknown permanence.");

				VK_CHECK(vkd.queueWaitIdle(queue));
			}
		}

		return tcu::TestStatus::pass("Pass");
	}
#else
	DE_UNREF(context);
	DE_UNREF(config);
	TCU_THROW(NotSupportedError, "Platform doesn't support dup2()");
#endif
}

tcu::TestStatus testSemaphoreFdDup3 (Context&					context,
									 const SemaphoreTestConfig	config)
{
#if (DE_OS == DE_OS_UNIX) && defined(_GNU_SOURCE)
	const Transference					transference		(getHandelTypeTransferences(config.externalType));
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const CustomInstance				instance			(createTestInstance(context, config.externalType, 0u, 0u));
	const vk::InstanceDriver&			vki					(instance.getDriver());
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	checkSemaphoreSupport(vki, physicalDevice, config.externalType);

	{
		const vk::Unique<vk::VkDevice>		device		(createTestDevice(context, vkp, instance, vki, physicalDevice, config.externalType, 0u, 0u, queueFamilyIndex));
		const vk::DeviceDriver				vkd			(vkp, instance, *device, context.getUsedApiVersion());
		vk::SimpleAllocator					alloc		(vkd, *device, vk::getPhysicalDeviceMemoryProperties(vki, physicalDevice));
		const vk::VkQueue					queue		(getQueue(vkd, *device, queueFamilyIndex));

		TestLog&							log			= context.getTestContext().getLog();
		const vk::Unique<vk::VkSemaphore>	semaphoreA	(createExportableSemaphore(vkd, *device, config.externalType));
		const vk::Unique<vk::VkSemaphore>	semaphoreB	(createExportableSemaphore(vkd, *device, config.externalType));

		{
			NativeHandle						fd, secondFd;

			if (transference == TRANSFERENCE_COPY)
			{
				submitAtomicCalculationsAndGetSemaphoreNative(context, vkd, *device, alloc, queue, queueFamilyIndex, *semaphoreA, config.externalType, fd);
				submitAtomicCalculationsAndGetSemaphoreNative(context, vkd, *device, alloc, queue, queueFamilyIndex, *semaphoreB, config.externalType, secondFd);
				if (fd.getFd() == -1 || secondFd.getFd() == -1)
					return tcu::TestStatus::pass("Pass: got -1 as a file descriptor, which is valid with a handle type of copy transference");
			}
			else
			{
				getSemaphoreNative(vkd, *device, *semaphoreA, config.externalType, fd);
				getSemaphoreNative(vkd, *device, *semaphoreB, config.externalType, secondFd);
			}

			const vk::VkSemaphoreImportFlags	flags		= config.permanence == PERMANENCE_TEMPORARY ? vk::VK_SEMAPHORE_IMPORT_TEMPORARY_BIT : (vk::VkSemaphoreImportFlagBits)0u;
			const int							newFd		(dup3(fd.getFd(), secondFd.getFd(), 0));

			if (newFd < 0)
				log << TestLog::Message << "dup3() failed: '" << strerror(errno) << "'" << TestLog::EndMessage;

			TCU_CHECK_MSG(newFd >= 0, "Failed to call dup3() for fences fd");

			{
				const vk::Unique<vk::VkSemaphore> semaphoreC (createAndImportSemaphore(vkd, *device, config.externalType, secondFd, flags));

				if (transference == TRANSFERENCE_COPY)
					submitEmptyWait(vkd, queue, *semaphoreC);
				else if (transference == TRANSFERENCE_REFERENCE)
				{
					submitEmptySignal(vkd, queue, *semaphoreA);
					submitEmptyWait(vkd, queue, *semaphoreC);
				}
				else
					DE_FATAL("Unknown permanence.");

				VK_CHECK(vkd.queueWaitIdle(queue));
			}
		}

		return tcu::TestStatus::pass("Pass");
	}
#else
	DE_UNREF(context);
	DE_UNREF(config);
	TCU_THROW(NotSupportedError, "Platform doesn't support dup3()");
#endif
}

tcu::TestStatus testSemaphoreFdSendOverSocket (Context&						context,
											   const SemaphoreTestConfig	config)
{
#if (DE_OS == DE_OS_ANDROID) || (DE_OS == DE_OS_UNIX)
	const Transference					transference		(getHandelTypeTransferences(config.externalType));
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const CustomInstance				instance			(createTestInstance(context, config.externalType, 0u, 0u));
	const vk::InstanceDriver&			vki					(instance.getDriver());
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	checkSemaphoreSupport(vki, physicalDevice, config.externalType);

	{
		const vk::Unique<vk::VkDevice>		device		(createTestDevice(context, vkp, instance, vki, physicalDevice, config.externalType, 0u, 0u, queueFamilyIndex));
		const vk::DeviceDriver				vkd			(vkp, instance, *device, context.getUsedApiVersion());
		vk::SimpleAllocator					alloc		(vkd, *device, vk::getPhysicalDeviceMemoryProperties(vki, physicalDevice));
		const vk::VkQueue					queue		(getQueue(vkd, *device, queueFamilyIndex));

		TestLog&							log			= context.getTestContext().getLog();
		const vk::Unique<vk::VkSemaphore>	semaphore	(createExportableSemaphore(vkd, *device, config.externalType));
		NativeHandle						fd;

		if (transference == TRANSFERENCE_COPY)
		{
			submitAtomicCalculationsAndGetSemaphoreNative(context, vkd, *device, alloc, queue, queueFamilyIndex, *semaphore, config.externalType, fd);
			if (fd.getFd() == -1)
				return tcu::TestStatus::pass("Pass: got -1 as a file descriptor, which is valid with a handle type of copy transference");
		}
		else
			getSemaphoreNative(vkd, *device, *semaphore, config.externalType, fd);

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
						const vk::VkSemaphoreImportFlags	flags	= config.permanence == PERMANENCE_TEMPORARY ? vk::VK_SEMAPHORE_IMPORT_TEMPORARY_BIT : (vk::VkSemaphoreImportFlagBits)0u;
						const cmsghdr* const				cmsg	= CMSG_FIRSTHDR(&msg);
						int									newFd_;
						deMemcpy(&newFd_, CMSG_DATA(cmsg), sizeof(int));
						NativeHandle						newFd	(newFd_);

						TCU_CHECK(cmsg->cmsg_level == SOL_SOCKET);
						TCU_CHECK(cmsg->cmsg_type == SCM_RIGHTS);
						TCU_CHECK(cmsg->cmsg_len == CMSG_LEN(sizeof(int)));
						TCU_CHECK(recvData == sendData);
						TCU_CHECK_MSG(newFd.getFd() >= 0, "Didn't receive valid fd from socket");

						{
							const vk::Unique<vk::VkSemaphore> newSemaphore (createAndImportSemaphore(vkd, *device, config.externalType, newFd, flags));

							if (transference == TRANSFERENCE_COPY)
								submitEmptyWait(vkd, queue, *newSemaphore);
							else if (transference == TRANSFERENCE_REFERENCE)
							{
								submitEmptySignal(vkd, queue, *newSemaphore);
								submitEmptyWait(vkd, queue, *newSemaphore);
							}
							else
								DE_FATAL("Unknown permanence.");

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
	DE_UNREF(config);
	TCU_THROW(NotSupportedError, "Platform doesn't support sending file descriptors over socket");
#endif
}

tcu::TestStatus testFenceQueries (Context& context, vk::VkExternalFenceHandleTypeFlagBits externalType)
{
	const CustomInstance							instance	(createTestInstance(context, 0u, 0u, externalType));
	const vk::InstanceDriver&						vki			(instance.getDriver());
	const vk::VkPhysicalDevice						device		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));

	TestLog&										log			= context.getTestContext().getLog();

	const vk::VkPhysicalDeviceExternalFenceInfo		info		=
	{
		vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_FENCE_INFO,
		DE_NULL,
		externalType
	};
	vk::VkExternalFenceProperties					properties	=
	{
		vk::VK_STRUCTURE_TYPE_EXTERNAL_FENCE_PROPERTIES,
		DE_NULL,
		0u,
		0u,
		0u
	};

	vki.getPhysicalDeviceExternalFenceProperties(device, &info, &properties);
	log << TestLog::Message << properties << TestLog::EndMessage;

	TCU_CHECK(properties.pNext == DE_NULL);
	TCU_CHECK(properties.sType == vk::VK_STRUCTURE_TYPE_EXTERNAL_FENCE_PROPERTIES);

	return tcu::TestStatus::pass("Pass");
}

struct FenceTestConfig
{
												FenceTestConfig	(vk::VkExternalFenceHandleTypeFlagBits	externalType_,
																 Permanence									permanence_)
		: externalType		(externalType_)
		, permanence		(permanence_)
	{
	}

	vk::VkExternalFenceHandleTypeFlagBits	externalType;
	Permanence								permanence;
};

tcu::TestStatus testFenceWin32Create (Context&				context,
									  const FenceTestConfig	config)
{
#if (DE_OS == DE_OS_WIN32)
	const Transference					transference		(getHandelTypeTransferences(config.externalType));
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const CustomInstance				instance			(createTestInstance(context, 0u, 0u, config.externalType));
	const vk::InstanceDriver&			vki					(instance.getDriver());
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	checkFenceSupport(vki, physicalDevice, config.externalType);

	{
		const vk::Unique<vk::VkDevice>				device			(createTestDevice(context, vkp, instance, vki, physicalDevice, 0u, 0u, config.externalType, queueFamilyIndex));
		const vk::DeviceDriver						vkd				(vkp, instance, *device, context.getUsedApiVersion());
		const vk::VkQueue							queue			(getQueue(vkd, *device, queueFamilyIndex));
		const vk::VkExportFenceWin32HandleInfoKHR	win32ExportInfo	=
		{
			vk::VK_STRUCTURE_TYPE_EXPORT_FENCE_WIN32_HANDLE_INFO_KHR,
			DE_NULL,

			(vk::pt::Win32SecurityAttributesPtr)DE_NULL,
			DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
			(vk::pt::Win32LPCWSTR)DE_NULL
		};
		const vk::VkExportFenceCreateInfo			exportCreateInfo=
		{
			vk::VK_STRUCTURE_TYPE_EXPORT_FENCE_CREATE_INFO,
			&win32ExportInfo,
			(vk::VkExternalFenceHandleTypeFlags)config.externalType
		};
		const vk::VkFenceCreateInfo					createInfo		=
		{
			vk::VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			&exportCreateInfo,
			0u
		};
		const vk::Unique<vk::VkFence>				fence			(vk::createFence(vkd, *device, &createInfo));

		if (transference == TRANSFERENCE_COPY)
			submitEmptySignal(vkd, queue, *fence);

		NativeHandle								handleA;
		getFenceNative(vkd, *device, *fence, config.externalType, handleA);

		{
			const vk::VkFenceImportFlags			flags			= config.permanence == PERMANENCE_TEMPORARY ? vk::VK_FENCE_IMPORT_TEMPORARY_BIT : (vk::VkFenceImportFlagBits)0u;
			const vk::Unique<vk::VkFence>			fenceA			(createAndImportFence(vkd, *device, config.externalType, handleA, flags));

			if (transference == TRANSFERENCE_COPY)
				VK_CHECK(vkd.waitForFences(*device, 1u, &*fenceA, VK_TRUE, ~0ull));
			else if (transference == TRANSFERENCE_REFERENCE)
			{
				submitEmptySignal(vkd, queue, *fence);
				VK_CHECK(vkd.waitForFences(*device, 1u, &*fenceA, VK_TRUE, ~0ull));
			}
			else
				DE_FATAL("Unknown transference.");

			VK_CHECK(vkd.queueWaitIdle(queue));
		}

		return tcu::TestStatus::pass("Pass");
	}
#else
	DE_UNREF(context);
	DE_UNREF(config);
	TCU_THROW(NotSupportedError, "Platform doesn't support win32 handles");
#endif
}

tcu::TestStatus testFenceImportTwice (Context&				context,
									  const FenceTestConfig	config)
{
	const Transference					transference		(getHandelTypeTransferences(config.externalType));
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const CustomInstance				instance			(createTestInstance(context, 0u, 0u, config.externalType));
	const vk::InstanceDriver&			vki					(instance.getDriver());
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	checkFenceSupport(vki, physicalDevice, config.externalType);

	{
		const vk::Unique<vk::VkDevice>	device		(createTestDevice(context, vkp, instance, vki, physicalDevice, 0u, 0u, config.externalType, queueFamilyIndex));
		const vk::DeviceDriver			vkd			(vkp, instance, *device, context.getUsedApiVersion());
		vk::SimpleAllocator				alloc		(vkd, *device, vk::getPhysicalDeviceMemoryProperties(vki, physicalDevice));
		const vk::VkQueue				queue		(getQueue(vkd, *device, queueFamilyIndex));
		const vk::Unique<vk::VkFence>	fence		(createExportableFence(vkd, *device, config.externalType));
		NativeHandle					handleA;

		if (transference == TRANSFERENCE_COPY)
		{
			submitAtomicCalculationsAndGetFenceNative(context, vkd, *device, alloc, queue, queueFamilyIndex, *fence, config.externalType, handleA);
			if (handleA.hasValidFd() && handleA.getFd() == -1)
				return tcu::TestStatus::pass("Pass: got -1 as a file descriptor, which is valid with a handle type of copy transference");
		}
		else
			getFenceNative(vkd, *device, *fence, config.externalType, handleA);

		{
			NativeHandle					handleB	(handleA);
			const vk::VkFenceImportFlags	flags	= config.permanence == PERMANENCE_TEMPORARY ? vk::VK_FENCE_IMPORT_TEMPORARY_BIT : (vk::VkFenceImportFlagBits)0u;
			const vk::Unique<vk::VkFence>	fenceA	(createAndImportFence(vkd, *device, config.externalType, handleA, flags));
			const vk::Unique<vk::VkFence>	fenceB	(createAndImportFence(vkd, *device, config.externalType, handleB, flags));

			if (transference == TRANSFERENCE_COPY)
				VK_CHECK(vkd.waitForFences(*device, 1u, &*fenceA, VK_TRUE, ~0ull));
			else if (transference == TRANSFERENCE_REFERENCE)
			{
				submitEmptySignal(vkd, queue, *fenceA);
				VK_CHECK(vkd.waitForFences(*device, 1u, &*fenceB, VK_TRUE, ~0ull));
			}
			else
				DE_FATAL("Unknown transference.");

			VK_CHECK(vkd.queueWaitIdle(queue));
		}

		return tcu::TestStatus::pass("Pass");
	}
}

tcu::TestStatus testFenceImportReimport (Context&				context,
										 const FenceTestConfig	config)
{
	const Transference					transference		(getHandelTypeTransferences(config.externalType));
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const CustomInstance				instance			(createTestInstance(context, 0u, 0u, config.externalType));
	const vk::InstanceDriver&			vki					(instance.getDriver());
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	checkFenceSupport(vki, physicalDevice, config.externalType);

	{
		const vk::Unique<vk::VkDevice>	device	(createTestDevice(context, vkp, instance, vki, physicalDevice, 0u, 0u, config.externalType, queueFamilyIndex));
		const vk::DeviceDriver			vkd		(vkp, instance, *device, context.getUsedApiVersion());
		vk::SimpleAllocator				alloc	(vkd, *device, vk::getPhysicalDeviceMemoryProperties(vki, physicalDevice));
		const vk::VkQueue				queue	(getQueue(vkd, *device, queueFamilyIndex));

		const vk::Unique<vk::VkFence>	fenceA	(createExportableFence(vkd, *device, config.externalType));
		NativeHandle					handleA;

		if (transference == TRANSFERENCE_COPY)
		{
			submitAtomicCalculationsAndGetFenceNative(context, vkd, *device, alloc, queue, queueFamilyIndex, *fenceA, config.externalType, handleA);
			if (handleA.hasValidFd() && handleA.getFd() == -1)
				return tcu::TestStatus::pass("Pass: got -1 as a file descriptor, which is valid with a handle type of copy transference");
		}
		else
			getFenceNative(vkd, *device, *fenceA, config.externalType, handleA);

		NativeHandle					handleB	(handleA);
		const vk::VkFenceImportFlags	flags	= config.permanence == PERMANENCE_TEMPORARY ? vk::VK_FENCE_IMPORT_TEMPORARY_BIT : (vk::VkFenceImportFlagBits)0u;
		const vk::Unique<vk::VkFence>	fenceB	(createAndImportFence(vkd, *device, config.externalType, handleA, flags));

		importFence(vkd, *device, *fenceB, config.externalType, handleB, flags);

		if (transference == TRANSFERENCE_COPY)
			VK_CHECK(vkd.waitForFences(*device, 1u, &*fenceB, VK_TRUE, ~0ull));
		else if (transference == TRANSFERENCE_REFERENCE)
		{
			submitEmptySignal(vkd, queue, *fenceA);
			VK_CHECK(vkd.waitForFences(*device, 1u, &*fenceB, VK_TRUE, ~0ull));
		}
		else
			DE_FATAL("Unknown transference.");

		VK_CHECK(vkd.queueWaitIdle(queue));

		return tcu::TestStatus::pass("Pass");
	}
}

tcu::TestStatus testFenceSignalExportImportWait (Context&				context,
												 const FenceTestConfig	config)
{
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const CustomInstance				instance			(createTestInstance(context, 0u, 0u, config.externalType));
	const vk::InstanceDriver&			vki					(instance.getDriver());
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	checkFenceSupport(vki, physicalDevice, config.externalType);

	{
		const vk::Unique<vk::VkDevice>	device	(createTestDevice(context, vkp, instance, vki, physicalDevice, 0u, 0u, config.externalType, queueFamilyIndex));
		const vk::DeviceDriver			vkd		(vkp, instance, *device, context.getUsedApiVersion());
		vk::SimpleAllocator				alloc	(vkd, *device, vk::getPhysicalDeviceMemoryProperties(vki, physicalDevice));
		const vk::VkQueue				queue	(getQueue(vkd, *device, queueFamilyIndex));
		const vk::Unique<vk::VkFence>	fenceA	(createExportableFence(vkd, *device, config.externalType));

		{
			NativeHandle	handle;

			submitAtomicCalculationsAndGetFenceNative(context, vkd, *device, alloc, queue, queueFamilyIndex, *fenceA, config.externalType, handle);
			if (handle.hasValidFd() && handle.getFd() == -1)
				return tcu::TestStatus::pass("Pass: got -1 as a file descriptor, which is valid with a handle type of copy transference");

			{
				const vk::VkFenceImportFlags	flags	= config.permanence == PERMANENCE_TEMPORARY ? vk::VK_FENCE_IMPORT_TEMPORARY_BIT : (vk::VkFenceImportFlagBits)0u;
				const vk::Unique<vk::VkFence>	fenceB	(createAndImportFence(vkd, *device, config.externalType, handle, flags));
				VK_CHECK(vkd.waitForFences(*device, 1u, &*fenceB, VK_TRUE, ~0ull));

				VK_CHECK(vkd.queueWaitIdle(queue));
			}
		}

		return tcu::TestStatus::pass("Pass");
	}
}

tcu::TestStatus testFenceImportSyncFdSignaled (Context&					context,
											   const FenceTestConfig	config)
{
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const CustomInstance				instance			(createTestInstance(context, 0u, 0u, config.externalType));
	const vk::InstanceDriver&			vki					(instance.getDriver());
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));
	const vk::VkFenceImportFlags		flags				= config.permanence == PERMANENCE_TEMPORARY ? vk::VK_FENCE_IMPORT_TEMPORARY_BIT : (vk::VkFenceImportFlagBits)0u;

	checkFenceSupport(vki, physicalDevice, config.externalType);

	{
		const vk::Unique<vk::VkDevice>	device	(createTestDevice(context, vkp, instance, vki, physicalDevice, 0u, 0u, config.externalType, queueFamilyIndex));
		const vk::DeviceDriver			vkd		(vkp, instance, *device, context.getUsedApiVersion());
		NativeHandle					handle	= -1;
		const vk::Unique<vk::VkFence>	fence	(createAndImportFence(vkd, *device, config.externalType, handle, flags));

		if (vkd.waitForFences(*device, 1u, &*fence, VK_TRUE, 0) != vk::VK_SUCCESS)
			return tcu::TestStatus::pass("Imported -1 sync fd isn't signaled");

		return tcu::TestStatus::pass("Pass");
	}
}

tcu::TestStatus testFenceExportSignalImportWait (Context&				context,
												 const FenceTestConfig	config)
{
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const CustomInstance				instance			(createTestInstance(context, 0u, 0u, config.externalType));
	const vk::InstanceDriver&			vki					(instance.getDriver());
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));
	const vk::VkFenceImportFlags		flags				= config.permanence == PERMANENCE_TEMPORARY ? vk::VK_FENCE_IMPORT_TEMPORARY_BIT : (vk::VkFenceImportFlagBits)0u;

	DE_ASSERT(getHandelTypeTransferences(config.externalType) == TRANSFERENCE_REFERENCE);
	checkFenceSupport(vki, physicalDevice, config.externalType);

	{
		const vk::Unique<vk::VkDevice>	device	(createTestDevice(context, vkp, instance, vki, physicalDevice, 0u, 0u, config.externalType, queueFamilyIndex));
		const vk::DeviceDriver			vkd		(vkp, instance, *device, context.getUsedApiVersion());
		const vk::VkQueue				queue	(getQueue(vkd, *device, queueFamilyIndex));

		const vk::Unique<vk::VkFence>	fenceA	(createExportableFence(vkd, *device, config.externalType));
		NativeHandle					handle;

		getFenceNative(vkd, *device, *fenceA, config.externalType, handle);

		submitEmptySignal(vkd, queue, *fenceA);
		{
			{
				const vk::Unique<vk::VkFence>	fenceB	(createAndImportFence(vkd, *device, config.externalType, handle, flags));

				VK_CHECK(vkd.waitForFences(*device, 1u, &*fenceB, VK_TRUE, ~0ull));
				VK_CHECK(vkd.queueWaitIdle(queue));
			}
		}

		return tcu::TestStatus::pass("Pass");
	}
}

tcu::TestStatus testFenceExportImportSignalWait (Context&				context,
												 const FenceTestConfig	config)
{
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const CustomInstance				instance			(createTestInstance(context, 0u, 0u, config.externalType));
	const vk::InstanceDriver&			vki					(instance.getDriver());
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	DE_ASSERT(getHandelTypeTransferences(config.externalType) == TRANSFERENCE_REFERENCE);
	checkFenceSupport(vki, physicalDevice, config.externalType);

	{
		const vk::VkFenceImportFlags	flags		= config.permanence == PERMANENCE_TEMPORARY ? vk::VK_FENCE_IMPORT_TEMPORARY_BIT : (vk::VkFenceImportFlagBits)0u;
		const vk::Unique<vk::VkDevice>	device		(createTestDevice(context, vkp, instance, vki, physicalDevice, 0u, 0u, config.externalType, queueFamilyIndex));
		const vk::DeviceDriver			vkd			(vkp, instance, *device, context.getUsedApiVersion());
		const vk::VkQueue				queue		(getQueue(vkd, *device, queueFamilyIndex));

		const vk::Unique<vk::VkFence>	fenceA	(createExportableFence(vkd, *device, config.externalType));
		NativeHandle					handle;

		getFenceNative(vkd, *device, *fenceA, config.externalType, handle);

		const vk::Unique<vk::VkFence>	fenceB	(createAndImportFence(vkd, *device, config.externalType, handle, flags));

		submitEmptySignal(vkd, queue, *fenceA);
		VK_CHECK(vkd.waitForFences(*device, 1u, &*fenceB, VK_TRUE, ~0ull));

		VK_CHECK(vkd.queueWaitIdle(queue));

		return tcu::TestStatus::pass("Pass");
	}
}

tcu::TestStatus testFenceSignalImport (Context&					context,
									   const FenceTestConfig	config)
{
	const Transference					transference		(getHandelTypeTransferences(config.externalType));
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const CustomInstance				instance			(createTestInstance(context, 0u, 0u, config.externalType));
	const vk::InstanceDriver&			vki					(instance.getDriver());
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	checkFenceSupport(vki, physicalDevice, config.externalType);

	{
		const vk::VkFenceImportFlags	flags	= config.permanence == PERMANENCE_TEMPORARY ? vk::VK_FENCE_IMPORT_TEMPORARY_BIT : (vk::VkFenceImportFlagBits)0u;
		const vk::Unique<vk::VkDevice>	device	(createTestDevice(context, vkp, instance, vki, physicalDevice, 0u, 0u, config.externalType, queueFamilyIndex));
		const vk::DeviceDriver			vkd		(vkp, instance, *device, context.getUsedApiVersion());
		vk::SimpleAllocator				alloc	(vkd, *device, vk::getPhysicalDeviceMemoryProperties(vki, physicalDevice));
		const vk::VkQueue				queue	(getQueue(vkd, *device, queueFamilyIndex));

		const vk::Unique<vk::VkFence>	fenceA	(createExportableFence(vkd, *device, config.externalType));
		const vk::Unique<vk::VkFence>	fenceB	(createFence(vkd, *device));
		NativeHandle					handle;

		submitEmptySignal(vkd, queue, *fenceB);
		VK_CHECK(vkd.queueWaitIdle(queue));

		if (transference == TRANSFERENCE_COPY)
		{
			submitAtomicCalculationsAndGetFenceNative(context, vkd, *device, alloc, queue, queueFamilyIndex, *fenceA, config.externalType, handle);
			if (handle.hasValidFd() && handle.getFd() == -1)
				return tcu::TestStatus::pass("Pass: got -1 as a file descriptor, which is valid with a handle type of copy transference");
		}
		else
			getFenceNative(vkd, *device, *fenceA, config.externalType, handle);

		importFence(vkd, *device, *fenceB, config.externalType, handle, flags);

		if (transference == TRANSFERENCE_COPY)
			VK_CHECK(vkd.waitForFences(*device, 1u, &*fenceB, VK_TRUE, ~0ull));
		else if (transference == TRANSFERENCE_REFERENCE)
		{
			submitEmptySignal(vkd, queue, *fenceA);
			VK_CHECK(vkd.waitForFences(*device, 1u, &*fenceB, VK_TRUE, ~0ull));
		}
		else
			DE_FATAL("Unknown transference.");

		VK_CHECK(vkd.queueWaitIdle(queue));

		return tcu::TestStatus::pass("Pass");
	}
}

tcu::TestStatus testFenceReset (Context&				context,
								const FenceTestConfig	config)
{
	const Transference					transference		(getHandelTypeTransferences(config.externalType));
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const CustomInstance				instance			(createTestInstance(context, 0u, 0u, config.externalType));
	const vk::InstanceDriver&			vki					(instance.getDriver());
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	checkFenceSupport(vki, physicalDevice, config.externalType);

	{
		const vk::VkFenceImportFlags	flags	= config.permanence == PERMANENCE_TEMPORARY ? vk::VK_FENCE_IMPORT_TEMPORARY_BIT : (vk::VkFenceImportFlagBits)0u;
		const vk::Unique<vk::VkDevice>	device	(createTestDevice(context, vkp, instance, vki, physicalDevice, 0u, 0u, config.externalType, queueFamilyIndex));
		const vk::DeviceDriver			vkd		(vkp, instance, *device, context.getUsedApiVersion());
		vk::SimpleAllocator				alloc	(vkd, *device, vk::getPhysicalDeviceMemoryProperties(vki, physicalDevice));
		const vk::VkQueue				queue	(getQueue(vkd, *device, queueFamilyIndex));

		const vk::Unique<vk::VkFence>	fenceA	(createExportableFence(vkd, *device, config.externalType));
		const vk::Unique<vk::VkFence>	fenceB	(createFence(vkd, *device));
		const vk::Unique<vk::VkFence>	fenceC	(createFence(vkd, *device));
		NativeHandle					handle;

		submitEmptySignal(vkd, queue, *fenceB);
		VK_CHECK(vkd.queueWaitIdle(queue));

		submitAtomicCalculationsAndGetFenceNative(context, vkd, *device, alloc, queue, queueFamilyIndex, *fenceA, config.externalType, handle);
		if (handle.hasValidFd() && handle.getFd() == -1)
			return tcu::TestStatus::pass("Pass: got -1 as a file descriptor, which is valid with a handle type of copy transference");

		NativeHandle					handleB	(handle);
		importFence(vkd, *device, *fenceB, config.externalType, handleB, flags);
		importFence(vkd, *device, *fenceC, config.externalType, handle, flags);

		VK_CHECK(vkd.queueWaitIdle(queue));
		VK_CHECK(vkd.resetFences(*device, 1u, &*fenceB));

		if (config.permanence == PERMANENCE_TEMPORARY || transference == TRANSFERENCE_COPY)
		{
			// vkResetFences() should restore fenceBs prior payload and reset that no affecting fenceCs payload
			// or fenceB should be separate copy of the payload and not affect fenceC
			VK_CHECK(vkd.waitForFences(*device, 1u, &*fenceC, VK_TRUE, ~0ull));

			// vkResetFences() should have restored fenceBs prior state and should be now reset
			// or fenceB should have it's separate payload
			submitEmptySignal(vkd, queue, *fenceB);
			VK_CHECK(vkd.waitForFences(*device, 1u, &*fenceB, VK_TRUE, ~0ull));
		}
		else if (config.permanence == PERMANENCE_PERMANENT)
		{
			DE_ASSERT(transference == TRANSFERENCE_REFERENCE);

			// Reset fences should have reset all of the fences
			submitEmptySignal(vkd, queue, *fenceC);

			VK_CHECK(vkd.waitForFences(*device, 1u, &*fenceA, VK_TRUE, ~0ull));
			VK_CHECK(vkd.waitForFences(*device, 1u, &*fenceB, VK_TRUE, ~0ull));
			VK_CHECK(vkd.waitForFences(*device, 1u, &*fenceC, VK_TRUE, ~0ull));
		}
		else
			DE_FATAL("Unknown permanence");

		VK_CHECK(vkd.queueWaitIdle(queue));

		return tcu::TestStatus::pass("Pass");
	}
}

tcu::TestStatus testFenceSignalWaitImport (Context&					context,
										   const FenceTestConfig	config)
{
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const CustomInstance				instance			(createTestInstance(context, 0u, 0u, config.externalType));
	const vk::InstanceDriver&			vki					(instance.getDriver());
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	checkFenceSupport(vki, physicalDevice, config.externalType);

	{
		const vk::VkFenceImportFlags	flags		= config.permanence == PERMANENCE_TEMPORARY ? vk::VK_FENCE_IMPORT_TEMPORARY_BIT : (vk::VkFenceImportFlagBits)0u;
		const vk::Unique<vk::VkDevice>	device		(createTestDevice(context, vkp, instance, vki, physicalDevice, 0u, 0u, config.externalType, queueFamilyIndex));
		const vk::DeviceDriver			vkd			(vkp, instance, *device, context.getUsedApiVersion());
		const vk::VkQueue				queue		(getQueue(vkd, *device, queueFamilyIndex));

		const vk::Unique<vk::VkFence>	fenceA	(createExportableFence(vkd, *device, config.externalType));
		const vk::Unique<vk::VkFence>	fenceB	(createFence(vkd, *device));
		NativeHandle					handle;

		getFenceNative(vkd, *device, *fenceA, config.externalType, handle);

		submitEmptySignal(vkd, queue, *fenceB);
		VK_CHECK(vkd.waitForFences(*device, 1u, &*fenceB, VK_TRUE, ~0ull));

		VK_CHECK(vkd.queueWaitIdle(queue));

		importFence(vkd, *device, *fenceB, config.externalType, handle, flags);

		submitEmptySignal(vkd, queue, *fenceA);
		VK_CHECK(vkd.waitForFences(*device, 1u, &*fenceB, VK_TRUE, ~0ull));

		VK_CHECK(vkd.queueWaitIdle(queue));

		return tcu::TestStatus::pass("Pass");
	}
}

tcu::TestStatus testFenceMultipleExports (Context&				context,
										  const FenceTestConfig	config)
{
	const size_t						exportCount			= 1024;
	const Transference					transference		(getHandelTypeTransferences(config.externalType));
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const CustomInstance				instance			(createTestInstance(context, 0u, 0u, config.externalType));
	const vk::InstanceDriver&			vki					(instance.getDriver());
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	checkFenceSupport(vki, physicalDevice, config.externalType);

	{
		const vk::Unique<vk::VkDevice>	device	(createTestDevice(context, vkp, instance, vki, physicalDevice, 0u, 0u, config.externalType, queueFamilyIndex));
		const vk::DeviceDriver			vkd		(vkp, instance, *device, context.getUsedApiVersion());
		vk::SimpleAllocator				alloc	(vkd, *device, vk::getPhysicalDeviceMemoryProperties(vki, physicalDevice));
		const vk::VkQueue				queue	(getQueue(vkd, *device, queueFamilyIndex));
		const vk::Unique<vk::VkFence>	fence	(createExportableFence(vkd, *device, config.externalType));

		for (size_t exportNdx = 0; exportNdx < exportCount; exportNdx++)
		{
			NativeHandle handle;

			// Need to touch watchdog due to how long one iteration takes
			context.getTestContext().touchWatchdog();

			if (transference == TRANSFERENCE_COPY)
			{
				submitAtomicCalculationsAndGetFenceNative(context, vkd, *device, alloc, queue, queueFamilyIndex, *fence, config.externalType, handle, exportNdx == 0 /* expect fence to be signaled after first pass */);
				if (handle.hasValidFd() && handle.getFd() == -1)
					return tcu::TestStatus::pass("Pass: got -1 as a file descriptor, which is valid with a handle type of copy transference");
			}
			else
				getFenceNative(vkd, *device, *fence, config.externalType, handle, exportNdx == 0 /* expect fence to be signaled after first pass */);
		}

		submitEmptySignal(vkd, queue, *fence);
		VK_CHECK(vkd.waitForFences(*device, 1u, &*fence, VK_TRUE, ~0ull));

		VK_CHECK(vkd.queueWaitIdle(queue));
	}

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus testFenceMultipleImports (Context&				context,
										  const FenceTestConfig	config)
{
	const size_t						importCount			= 4 * 1024;
	const Transference					transference		(getHandelTypeTransferences(config.externalType));
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const CustomInstance				instance			(createTestInstance(context, 0u, 0u, config.externalType));
	const vk::InstanceDriver&			vki					(instance.getDriver());
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	checkFenceSupport(vki, physicalDevice, config.externalType);

	{
		const vk::VkFenceImportFlags	flags	= config.permanence == PERMANENCE_TEMPORARY ? vk::VK_FENCE_IMPORT_TEMPORARY_BIT : (vk::VkFenceImportFlagBits)0u;
		const vk::Unique<vk::VkDevice>	device	(createTestDevice(context, vkp, instance, vki, physicalDevice, 0u, 0u, config.externalType, queueFamilyIndex));
		const vk::DeviceDriver			vkd		(vkp, instance, *device, context.getUsedApiVersion());
		vk::SimpleAllocator				alloc	(vkd, *device, vk::getPhysicalDeviceMemoryProperties(vki, physicalDevice));
		const vk::VkQueue				queue	(getQueue(vkd, *device, queueFamilyIndex));
		const vk::Unique<vk::VkFence>	fenceA	(createExportableFence(vkd, *device, config.externalType));
		NativeHandle					handleA;

		if (transference == TRANSFERENCE_COPY)
		{
			submitAtomicCalculationsAndGetFenceNative(context, vkd, *device, alloc, queue, queueFamilyIndex, *fenceA, config.externalType, handleA);
			if (handleA.hasValidFd() && handleA.getFd() == -1)
				return tcu::TestStatus::pass("Pass: got -1 as a file descriptor, which is valid with a handle type of copy transference");
		}
		else
			getFenceNative(vkd, *device, *fenceA, config.externalType, handleA);

		for (size_t importNdx = 0; importNdx < importCount; importNdx++)
		{
			NativeHandle					handleB		(handleA);
			const vk::Unique<vk::VkFence>	fenceB	(createAndImportFence(vkd, *device, config.externalType, handleB, flags));
		}

		if (transference == TRANSFERENCE_COPY)
		{
			importFence(vkd, *device, *fenceA, config.externalType, handleA, flags);
			VK_CHECK(vkd.waitForFences(*device, 1u, &*fenceA, VK_TRUE, ~0ull));
		}
		else if (transference == TRANSFERENCE_REFERENCE)
		{
			submitEmptySignal(vkd, queue, *fenceA);
			VK_CHECK(vkd.waitForFences(*device, 1u, &*fenceA, VK_TRUE, ~0ull));
		}
		else
			DE_FATAL("Unknown transference.");

		VK_CHECK(vkd.queueWaitIdle(queue));
	}

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus testFenceTransference (Context&					context,
									   const FenceTestConfig	config)
{
	const Transference					transference		(getHandelTypeTransferences(config.externalType));
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const CustomInstance				instance			(createTestInstance(context, 0u, 0u, config.externalType));
	const vk::InstanceDriver&			vki					(instance.getDriver());
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	checkFenceSupport(vki, physicalDevice, config.externalType);

	{
		const vk::VkFenceImportFlags	flags	= config.permanence == PERMANENCE_TEMPORARY ? vk::VK_FENCE_IMPORT_TEMPORARY_BIT : (vk::VkFenceImportFlagBits)0u;
		const vk::Unique<vk::VkDevice>	device	(createTestDevice(context, vkp, instance, vki, physicalDevice, 0u, 0u, config.externalType, queueFamilyIndex));
		const vk::DeviceDriver			vkd		(vkp, instance, *device, context.getUsedApiVersion());
		vk::SimpleAllocator				alloc	(vkd, *device, vk::getPhysicalDeviceMemoryProperties(vki, physicalDevice));
		const vk::VkQueue				queue	(getQueue(vkd, *device, queueFamilyIndex));

		const vk::Unique<vk::VkFence>	fenceA	(createExportableFence(vkd, *device, config.externalType));
		NativeHandle					handle;

		submitAtomicCalculationsAndGetFenceNative(context, vkd, *device, alloc, queue, queueFamilyIndex, *fenceA, config.externalType, handle);
		if (handle.hasValidFd() && handle.getFd() == -1)
			return tcu::TestStatus::pass("Pass: got -1 as a file descriptor, which is valid with a handle type of copy transference");

		{
			const vk::Unique<vk::VkFence>	fenceB	(createAndImportFence(vkd, *device, config.externalType, handle, flags));

			if (config.permanence == PERMANENCE_PERMANENT)
			{
				if (transference == TRANSFERENCE_COPY)
				{
					submitEmptySignal(vkd, queue, *fenceA);
					VK_CHECK(vkd.waitForFences(*device, 1u, &*fenceB, VK_TRUE, ~0ull));
					VK_CHECK(vkd.queueWaitIdle(queue));

					VK_CHECK(vkd.resetFences(*device, 1u, &*fenceB));
					submitEmptySignal(vkd, queue, *fenceB);

					VK_CHECK(vkd.waitForFences(*device, 1u, &*fenceA, VK_TRUE, ~0ull));
					VK_CHECK(vkd.waitForFences(*device, 1u, &*fenceB, VK_TRUE, ~0ull));
					VK_CHECK(vkd.queueWaitIdle(queue));
				}
				else if (transference== TRANSFERENCE_REFERENCE)
				{
					VK_CHECK(vkd.waitForFences(*device, 1u, &*fenceB, VK_TRUE, ~0ull));
					VK_CHECK(vkd.queueWaitIdle(queue));

					VK_CHECK(vkd.resetFences(*device, 1u, &*fenceB));
					submitEmptySignal(vkd, queue, *fenceA);
					VK_CHECK(vkd.waitForFences(*device, 1u, &*fenceB, VK_TRUE, ~0ull));

					VK_CHECK(vkd.resetFences(*device, 1u, &*fenceA));
					submitEmptySignal(vkd, queue, *fenceB);
					VK_CHECK(vkd.waitForFences(*device, 1u, &*fenceA, VK_TRUE, ~0ull));
					VK_CHECK(vkd.queueWaitIdle(queue));
				}
				else
					DE_FATAL("Unknown transference.");
			}
			else if (config.permanence == PERMANENCE_TEMPORARY)
			{
				if (transference == TRANSFERENCE_COPY)
				{
					submitEmptySignal(vkd, queue, *fenceA);
					VK_CHECK(vkd.waitForFences(*device, 1u, &*fenceB, VK_TRUE, ~0ull));
					VK_CHECK(vkd.queueWaitIdle(queue));

					VK_CHECK(vkd.resetFences(*device, 1u, &*fenceB));
					submitEmptySignal(vkd, queue, *fenceB);

					VK_CHECK(vkd.waitForFences(*device, 1u, &*fenceA, VK_TRUE, ~0ull));
					VK_CHECK(vkd.waitForFences(*device, 1u, &*fenceB, VK_TRUE, ~0ull));
					VK_CHECK(vkd.queueWaitIdle(queue));
				}
				else if (transference == TRANSFERENCE_REFERENCE)
				{
					VK_CHECK(vkd.waitForFences(*device, 1u, &*fenceB, VK_TRUE, ~0ull));
					VK_CHECK(vkd.queueWaitIdle(queue));

					VK_CHECK(vkd.resetFences(*device, 1u, &*fenceA));
					VK_CHECK(vkd.resetFences(*device, 1u, &*fenceB));
					submitEmptySignal(vkd, queue, *fenceA);
					submitEmptySignal(vkd, queue, *fenceB);

					VK_CHECK(vkd.waitForFences(*device, 1u, &*fenceB, VK_TRUE, ~0ull));
					VK_CHECK(vkd.waitForFences(*device, 1u, &*fenceA, VK_TRUE, ~0ull));
					VK_CHECK(vkd.queueWaitIdle(queue));
				}
				else
					DE_FATAL("Unknown transference.");
			}
			else
				DE_FATAL("Unknown permanence.");
		}

		return tcu::TestStatus::pass("Pass");
	}
}

tcu::TestStatus testFenceFdDup (Context&				context,
								const FenceTestConfig	config)
{
#if (DE_OS == DE_OS_ANDROID) || (DE_OS == DE_OS_UNIX)
	const Transference					transference		(getHandelTypeTransferences(config.externalType));
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const CustomInstance				instance			(createTestInstance(context, 0u, 0u, config.externalType));
	const vk::InstanceDriver&			vki					(instance.getDriver());
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	checkFenceSupport(vki, physicalDevice, config.externalType);

	{
		const vk::VkFenceImportFlags	flags	= config.permanence == PERMANENCE_TEMPORARY ? vk::VK_FENCE_IMPORT_TEMPORARY_BIT : (vk::VkFenceImportFlagBits)0u;
		const vk::Unique<vk::VkDevice>	device	(createTestDevice(context, vkp, instance, vki, physicalDevice, 0u, 0u, config.externalType, queueFamilyIndex));
		const vk::DeviceDriver			vkd		(vkp, instance, *device, context.getUsedApiVersion());
		vk::SimpleAllocator				alloc	(vkd, *device, vk::getPhysicalDeviceMemoryProperties(vki, physicalDevice));
		const vk::VkQueue				queue	(getQueue(vkd, *device, queueFamilyIndex));

		TestLog&						log		= context.getTestContext().getLog();
		const vk::Unique<vk::VkFence>	fenceA	(createExportableFence(vkd, *device, config.externalType));

		{
			NativeHandle		fd;

			if (transference == TRANSFERENCE_COPY)
			{
				submitAtomicCalculationsAndGetFenceNative(context, vkd, *device, alloc, queue, queueFamilyIndex, *fenceA, config.externalType, fd);
				if (fd.getFd() == -1)
					return tcu::TestStatus::pass("Pass: got -1 as a file descriptor, which is valid with a handle type of copy transference");
			}
			else
				getFenceNative(vkd, *device, *fenceA, config.externalType, fd);

			NativeHandle		newFd	(dup(fd.getFd()));

			if (newFd.getFd() < 0)
				log << TestLog::Message << "dup() failed: '" << strerror(errno) << "'" << TestLog::EndMessage;

			TCU_CHECK_MSG(newFd.getFd() >= 0, "Failed to call dup() for fences fd");

			{
				const vk::Unique<vk::VkFence> fenceB (createAndImportFence(vkd, *device, config.externalType, newFd, flags));

				if (transference == TRANSFERENCE_COPY)
					VK_CHECK(vkd.waitForFences(*device, 1u, &*fenceB, VK_TRUE, ~0ull));
				else if (transference == TRANSFERENCE_REFERENCE)
				{
					submitEmptySignal(vkd, queue, *fenceA);
					VK_CHECK(vkd.waitForFences(*device, 1u, &*fenceB, VK_TRUE, ~0ull));
				}
				else
					DE_FATAL("Unknown permanence.");

				VK_CHECK(vkd.queueWaitIdle(queue));
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

tcu::TestStatus testFenceFdDup2 (Context&				context,
								 const FenceTestConfig	config)
{
#if (DE_OS == DE_OS_ANDROID) || (DE_OS == DE_OS_UNIX)
	const Transference					transference		(getHandelTypeTransferences(config.externalType));
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const CustomInstance				instance			(createTestInstance(context, 0u, 0u, config.externalType));
	const vk::InstanceDriver&			vki					(instance.getDriver());
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	checkFenceSupport(vki, physicalDevice, config.externalType);

	{
		const vk::VkFenceImportFlags	flags	= config.permanence == PERMANENCE_TEMPORARY ? vk::VK_FENCE_IMPORT_TEMPORARY_BIT : (vk::VkFenceImportFlagBits)0u;
		const vk::Unique<vk::VkDevice>	device	(createTestDevice(context, vkp, instance, vki, physicalDevice, 0u, 0u, config.externalType, queueFamilyIndex));
		const vk::DeviceDriver			vkd		(vkp, instance, *device, context.getUsedApiVersion());
		vk::SimpleAllocator				alloc	(vkd, *device, vk::getPhysicalDeviceMemoryProperties(vki, physicalDevice));
		const vk::VkQueue				queue	(getQueue(vkd, *device, queueFamilyIndex));

		TestLog&						log		= context.getTestContext().getLog();
		const vk::Unique<vk::VkFence>	fenceA	(createExportableFence(vkd, *device, config.externalType));
		const vk::Unique<vk::VkFence>	fenceB	(createExportableFence(vkd, *device, config.externalType));

		{
			NativeHandle		fd, secondFd;

			if (transference == TRANSFERENCE_COPY)
			{
				submitAtomicCalculationsAndGetFenceNative(context, vkd, *device, alloc, queue, queueFamilyIndex, *fenceA, config.externalType, fd);
				submitAtomicCalculationsAndGetFenceNative(context, vkd, *device, alloc, queue, queueFamilyIndex, *fenceB, config.externalType, secondFd);
				if (fd.getFd() == -1 || secondFd.getFd() == -1)
					return tcu::TestStatus::pass("Pass: got -1 as a file descriptor, which is valid with a handle type of copy transference");
			}
			else
			{
				getFenceNative(vkd, *device, *fenceA, config.externalType, fd);
				getFenceNative(vkd, *device, *fenceB, config.externalType, secondFd);
			}

			int					newFd		(dup2(fd.getFd(), secondFd.getFd()));

			if (newFd < 0)
				log << TestLog::Message << "dup2() failed: '" << strerror(errno) << "'" << TestLog::EndMessage;

			TCU_CHECK_MSG(newFd >= 0, "Failed to call dup2() for fences fd");

			{
				const vk::Unique<vk::VkFence> fenceC (createAndImportFence(vkd, *device, config.externalType, secondFd, flags));

				if (transference == TRANSFERENCE_COPY)
					VK_CHECK(vkd.waitForFences(*device, 1u, &*fenceC, VK_TRUE, ~0ull));
				else if (transference == TRANSFERENCE_REFERENCE)
				{
					submitEmptySignal(vkd, queue, *fenceA);
					VK_CHECK(vkd.waitForFences(*device, 1u, &*fenceC, VK_TRUE, ~0ull));
				}
				else
					DE_FATAL("Unknown permanence.");

				VK_CHECK(vkd.queueWaitIdle(queue));
			}
		}

		return tcu::TestStatus::pass("Pass");
	}
#else
	DE_UNREF(context);
	DE_UNREF(config);
	TCU_THROW(NotSupportedError, "Platform doesn't support dup2()");
#endif
}

tcu::TestStatus testFenceFdDup3 (Context&				context,
								 const FenceTestConfig	config)
{
#if (DE_OS == DE_OS_UNIX) && defined(_GNU_SOURCE)
	const Transference					transference		(getHandelTypeTransferences(config.externalType));
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const CustomInstance				instance			(createTestInstance(context, 0u, 0u, config.externalType));
	const vk::InstanceDriver&			vki					(instance.getDriver());
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	checkFenceSupport(vki, physicalDevice, config.externalType);

	{
		const vk::Unique<vk::VkDevice>	device	(createTestDevice(context, vkp, instance, vki, physicalDevice, 0u, 0u, config.externalType, queueFamilyIndex));
		const vk::DeviceDriver			vkd		(vkp, instance, *device, context.getUsedApiVersion());
		vk::SimpleAllocator				alloc	(vkd, *device, vk::getPhysicalDeviceMemoryProperties(vki, physicalDevice));
		const vk::VkQueue				queue	(getQueue(vkd, *device, queueFamilyIndex));

		TestLog&						log		= context.getTestContext().getLog();
		const vk::Unique<vk::VkFence>	fenceA	(createExportableFence(vkd, *device, config.externalType));
		const vk::Unique<vk::VkFence>	fenceB	(createExportableFence(vkd, *device, config.externalType));

		{
			NativeHandle					fd, secondFd;

			if (transference == TRANSFERENCE_COPY)
			{
				submitAtomicCalculationsAndGetFenceNative(context, vkd, *device, alloc, queue, queueFamilyIndex, *fenceA, config.externalType, fd);
				submitAtomicCalculationsAndGetFenceNative(context, vkd, *device, alloc, queue, queueFamilyIndex, *fenceB, config.externalType, secondFd);
				if (fd.getFd() == -1 || secondFd.getFd() == -1)
					return tcu::TestStatus::pass("Pass: got -1 as a file descriptor, which is valid with a handle type of copy transference");
			}
			else
			{
				getFenceNative(vkd, *device, *fenceA, config.externalType, fd);
				getFenceNative(vkd, *device, *fenceB, config.externalType, secondFd);
			}

			const vk::VkFenceImportFlags	flags		= config.permanence == PERMANENCE_TEMPORARY ? vk::VK_FENCE_IMPORT_TEMPORARY_BIT : (vk::VkFenceImportFlagBits)0u;
			const int						newFd		(dup3(fd.getFd(), secondFd.getFd(), 0));

			if (newFd < 0)
				log << TestLog::Message << "dup3() failed: '" << strerror(errno) << "'" << TestLog::EndMessage;

			TCU_CHECK_MSG(newFd >= 0, "Failed to call dup3() for fences fd");

			{
				const vk::Unique<vk::VkFence> fenceC (createAndImportFence(vkd, *device, config.externalType, secondFd, flags));

				if (transference == TRANSFERENCE_COPY)
					VK_CHECK(vkd.waitForFences(*device, 1u, &*fenceC, VK_TRUE, ~0ull));
				else if (transference == TRANSFERENCE_REFERENCE)
				{
					submitEmptySignal(vkd, queue, *fenceA);
					VK_CHECK(vkd.waitForFences(*device, 1u, &*fenceC, VK_TRUE, ~0ull));
				}
				else
					DE_FATAL("Unknown permanence.");

				VK_CHECK(vkd.queueWaitIdle(queue));
			}
		}

		return tcu::TestStatus::pass("Pass");
	}
#else
	DE_UNREF(context);
	DE_UNREF(config);
	TCU_THROW(NotSupportedError, "Platform doesn't support dup3()");
#endif
}

tcu::TestStatus testFenceFdSendOverSocket (Context&					context,
										   const FenceTestConfig	config)
{
#if (DE_OS == DE_OS_ANDROID) || (DE_OS == DE_OS_UNIX)
	const Transference					transference		(getHandelTypeTransferences(config.externalType));
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const CustomInstance				instance			(createTestInstance(context, 0u, 0u, config.externalType));
	const vk::InstanceDriver&			vki					(instance.getDriver());
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	checkFenceSupport(vki, physicalDevice, config.externalType);

	{
		const vk::Unique<vk::VkDevice>	device	(createTestDevice(context, vkp, instance, vki, physicalDevice, 0u, 0u, config.externalType, queueFamilyIndex));
		const vk::DeviceDriver			vkd		(vkp, instance, *device, context.getUsedApiVersion());
		vk::SimpleAllocator				alloc	(vkd, *device, vk::getPhysicalDeviceMemoryProperties(vki, physicalDevice));
		const vk::VkQueue				queue	(getQueue(vkd, *device, queueFamilyIndex));

		TestLog&						log		= context.getTestContext().getLog();
		const vk::Unique<vk::VkFence>	fence	(createExportableFence(vkd, *device, config.externalType));
		NativeHandle					fd;

		if (transference == TRANSFERENCE_COPY)
		{
			submitAtomicCalculationsAndGetFenceNative(context, vkd, *device, alloc, queue, queueFamilyIndex, *fence, config.externalType, fd);
			if (fd.getFd() == -1)
				return tcu::TestStatus::pass("Pass: got -1 as a file descriptor, which is valid with a handle type of copy transference");
		}
		else
			getFenceNative(vkd, *device, *fence, config.externalType, fd);

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
						const vk::VkFenceImportFlags		flags	= config.permanence == PERMANENCE_TEMPORARY ? vk::VK_FENCE_IMPORT_TEMPORARY_BIT : (vk::VkFenceImportFlagBits)0u;
						const cmsghdr* const				cmsg	= CMSG_FIRSTHDR(&msg);
						int									newFd_;
						deMemcpy(&newFd_, CMSG_DATA(cmsg), sizeof(int));
						NativeHandle						newFd	(newFd_);

						TCU_CHECK(cmsg->cmsg_level == SOL_SOCKET);
						TCU_CHECK(cmsg->cmsg_type == SCM_RIGHTS);
						TCU_CHECK(cmsg->cmsg_len == CMSG_LEN(sizeof(int)));
						TCU_CHECK(recvData == sendData);
						TCU_CHECK_MSG(newFd.getFd() >= 0, "Didn't receive valid fd from socket");

						{
							const vk::Unique<vk::VkFence> newFence (createAndImportFence(vkd, *device, config.externalType, newFd, flags));

							if (transference == TRANSFERENCE_COPY)
								VK_CHECK(vkd.waitForFences(*device, 1u, &*newFence, VK_TRUE, ~0ull));
							else if (transference == TRANSFERENCE_REFERENCE)
							{
								submitEmptySignal(vkd, queue, *newFence);
								VK_CHECK(vkd.waitForFences(*device, 1u, &*newFence, VK_TRUE, ~0ull));
							}
							else
								DE_FATAL("Unknown permanence.");

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
	DE_UNREF(config);
	TCU_THROW(NotSupportedError, "Platform doesn't support sending file descriptors over socket");
#endif
}

tcu::TestStatus testBufferQueries (Context& context, vk::VkExternalMemoryHandleTypeFlagBits externalType)
{
	const vk::VkBufferCreateFlags		createFlags[]		=
	{
		0u,
		vk::VK_BUFFER_CREATE_SPARSE_BINDING_BIT,
		vk::VK_BUFFER_CREATE_SPARSE_BINDING_BIT|vk::VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT,
		vk::VK_BUFFER_CREATE_SPARSE_BINDING_BIT|vk::VK_BUFFER_CREATE_SPARSE_ALIASED_BIT
	};
	const vk::VkBufferUsageFlags		usageFlags[]		=
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
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const CustomInstance				instance			(createTestInstance(context, 0u, externalType, 0u));
	const vk::InstanceDriver&			vki					(instance.getDriver());
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const vk::VkPhysicalDeviceFeatures	deviceFeatures		(vk::getPhysicalDeviceFeatures(vki, physicalDevice));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	// VkDevice is only created if physical device claims to support any of these types.
	vk::Move<vk::VkDevice>				device;
	de::MovePtr<vk::DeviceDriver>		vkd;
	bool								deviceHasDedicated	= false;

	TestLog&							log					= context.getTestContext().getLog();

	for (size_t createFlagNdx = 0; createFlagNdx < DE_LENGTH_OF_ARRAY(createFlags); createFlagNdx++)
	for (size_t usageFlagNdx = 0; usageFlagNdx < DE_LENGTH_OF_ARRAY(usageFlags); usageFlagNdx++)
	{
		const vk::VkBufferViewCreateFlags				createFlag		= createFlags[createFlagNdx];
		const vk::VkBufferUsageFlags					usageFlag		= usageFlags[usageFlagNdx];
		const vk::VkPhysicalDeviceExternalBufferInfo	info			=
		{
			vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_BUFFER_INFO,
			DE_NULL,
			createFlag,
			usageFlag,
			externalType
		};
		vk::VkExternalBufferProperties					properties		=
		{
			vk::VK_STRUCTURE_TYPE_EXTERNAL_BUFFER_PROPERTIES,
			DE_NULL,
			{ 0u, 0u, 0u }
		};

		if (((createFlag & vk::VK_BUFFER_CREATE_SPARSE_BINDING_BIT) != 0) &&
			(deviceFeatures.sparseBinding == VK_FALSE))
			continue;

		if (((createFlag & vk::VK_BUFFER_CREATE_SPARSE_ALIASED_BIT) != 0) &&
			(deviceFeatures.sparseResidencyAliased == VK_FALSE))
			continue;

		if (((createFlag & vk::VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT) != 0) &&
			(deviceFeatures.sparseResidencyBuffer == VK_FALSE))
			continue;

		vki.getPhysicalDeviceExternalBufferProperties(physicalDevice, &info, &properties);

		log << TestLog::Message << properties << TestLog::EndMessage;

		TCU_CHECK(properties.sType == vk::VK_STRUCTURE_TYPE_EXTERNAL_BUFFER_PROPERTIES);
		TCU_CHECK(properties.pNext == DE_NULL);
		// \todo [2017-06-06 pyry] Can we validate anything else? Compatible types?

		if ((properties.externalMemoryProperties.externalMemoryFeatures & (vk::VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT|vk::VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT)) != 0)
		{
			const bool	requiresDedicated	= (properties.externalMemoryProperties.externalMemoryFeatures & vk::VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT) != 0;

			if (!device || (requiresDedicated && !deviceHasDedicated))
			{
				// \note We need to re-create with dedicated mem extensions if previous device instance didn't have them
				try
				{
					device				= createTestDevice(context, vkp, instance, vki, physicalDevice, 0u, externalType, 0u, queueFamilyIndex, requiresDedicated);
					vkd					= de::MovePtr<vk::DeviceDriver>(new vk::DeviceDriver(vkp, instance, *device, context.getUsedApiVersion()));
					deviceHasDedicated	= requiresDedicated;
				}
				catch (const tcu::NotSupportedError& e)
				{
					log << e;
					TCU_FAIL("Physical device claims to support handle type but required extensions are not supported");
				}
			}
		}

		if ((properties.externalMemoryProperties.externalMemoryFeatures & vk::VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT) != 0)
		{
			DE_ASSERT(!!device);
			DE_ASSERT(vkd);

			if (deviceHasDedicated)
			{
				const vk::Unique<vk::VkBuffer>				buffer						(createExternalBuffer(*vkd, *device, queueFamilyIndex, externalType, 1024u, createFlag, usageFlag));
				const vk::VkMemoryDedicatedRequirements		reqs						(getMemoryDedicatedRequirements(*vkd, *device, *buffer));
				const bool									propertiesRequiresDedicated	= (properties.externalMemoryProperties.externalMemoryFeatures & vk::VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT) != 0;
				const bool									objectRequiresDedicated		= (reqs.requiresDedicatedAllocation != VK_FALSE);

				if (propertiesRequiresDedicated != objectRequiresDedicated)
					TCU_FAIL("vkGetPhysicalDeviceExternalBufferProperties and vkGetBufferMemoryRequirements2 report different dedicated requirements");
			}
			else
			{
				// We can't query whether dedicated memory is required or not on per-object basis.
				// This check should be redundant as the code above tries to create device with
				// VK_KHR_dedicated_allocation & VK_KHR_get_memory_requirements2 if dedicated memory
				// is required. However, checking again doesn't hurt.
				TCU_CHECK((properties.externalMemoryProperties.externalMemoryFeatures & vk::VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT) == 0);
			}
		}
	}

	return tcu::TestStatus::pass("Pass");
}

struct MemoryTestConfig
{
												MemoryTestConfig	(vk::VkExternalMemoryHandleTypeFlagBits	externalType_,
																	 bool										hostVisible_,
																	 bool										dedicated_)
		: externalType	(externalType_)
		, hostVisible	(hostVisible_)
		, dedicated		(dedicated_)
	{
	}

	vk::VkExternalMemoryHandleTypeFlagBits	externalType;
	bool									hostVisible;
	bool									dedicated;
};

#if (DE_OS == DE_OS_WIN32)
deUint32 chooseWin32MemoryType(deUint32 bits)
{
	if (bits == 0)
		TCU_THROW(NotSupportedError, "No compatible memory type found");

	return deCtz32(bits);
}
#endif

tcu::TestStatus testMemoryWin32Create (Context& context, MemoryTestConfig config)
{
#if (DE_OS == DE_OS_WIN32)
	const vk::PlatformInterface&				vkp					(context.getPlatformInterface());
	const CustomInstance						instance			(createTestInstance(context, 0u, config.externalType, 0u));
	const vk::InstanceDriver&					vki					(instance.getDriver());
	const vk::VkPhysicalDevice					physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32								queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));
	const vk::Unique<vk::VkDevice>				device				(createTestDevice(context, vkp, instance, vki, physicalDevice, 0u, config.externalType, 0u, queueFamilyIndex));
	const vk::DeviceDriver						vkd					(vkp, instance, *device, context.getUsedApiVersion());
	const vk::VkBufferUsageFlags				usage				= vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT|vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	const deUint32								seed				= 1261033864u;
	const vk::VkDeviceSize						bufferSize			= 1024;
	const std::vector<deUint8>					testData			(genTestData(seed, (size_t)bufferSize));

	const vk::VkPhysicalDeviceMemoryProperties	memoryProps			= vk::getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice());
	const deUint32								compatibleMemTypes	= vk::getCompatibleMemoryTypes(memoryProps, config.hostVisible ? vk::MemoryRequirement::HostVisible : vk::MemoryRequirement::Any);

	checkBufferSupport(vki, physicalDevice, config.externalType, 0u, usage, config.dedicated);

	// \note Buffer is only allocated to get memory requirements
	const vk::Unique<vk::VkBuffer>				buffer					(createExternalBuffer(vkd, *device, queueFamilyIndex, config.externalType, bufferSize, 0u, usage));
	const vk::VkMemoryRequirements				requirements			(getBufferMemoryRequirements(vkd, *device, *buffer));
	const vk::VkExportMemoryWin32HandleInfoKHR	win32Info				=
	{
		vk::VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR,
		DE_NULL,

		(vk::pt::Win32SecurityAttributesPtr)DE_NULL,
		DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
		(vk::pt::Win32LPCWSTR)DE_NULL
	};
	const vk::VkExportMemoryAllocateInfo		exportInfo			=
	{
		vk::VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
		&win32Info,
		(vk::VkExternalMemoryHandleTypeFlags)config.externalType
	};

	const deUint32								exportedMemoryTypeIndex	= chooseWin32MemoryType(requirements.memoryTypeBits & compatibleMemTypes);
	const vk::VkMemoryAllocateInfo				info					=
	{
		vk::VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		&exportInfo,
		requirements.size,
		exportedMemoryTypeIndex
	};
	const vk::Unique<vk::VkDeviceMemory>		memory					(vk::allocateMemory(vkd, *device, &info));
	NativeHandle								handleA;

	if (config.hostVisible)
		writeHostMemory(vkd, *device, *memory, testData.size(), &testData[0]);

	getMemoryNative(vkd, *device, *memory, config.externalType, handleA);

	{
		const vk::Unique<vk::VkDeviceMemory>	memoryA	(importMemory(vkd, *device, requirements, config.externalType, exportedMemoryTypeIndex, handleA));

		if (config.hostVisible)
		{
			const std::vector<deUint8>		testDataA		(genTestData(seed ^ 124798807u, (size_t)bufferSize));
			const std::vector<deUint8>		testDataB		(genTestData(seed ^ 970834278u, (size_t)bufferSize));

			checkHostMemory(vkd, *device, *memoryA, testData.size(), &testData[0]);
			checkHostMemory(vkd, *device, *memory,  testData.size(), &testData[0]);

			writeHostMemory(vkd, *device, *memoryA, testDataA.size(), &testDataA[0]);
			writeHostMemory(vkd, *device, *memory,  testDataA.size(), &testDataB[0]);

			checkHostMemory(vkd, *device, *memoryA, testData.size(), &testDataB[0]);
			checkHostMemory(vkd, *device, *memory,  testData.size(), &testDataB[0]);
		}
	}

	return tcu::TestStatus::pass("Pass");
#else
	DE_UNREF(context);
	DE_UNREF(config);
	TCU_THROW(NotSupportedError, "Platform doesn't support win32 handles");
#endif
}

deUint32 getExportedMemoryTypeIndex(const vk::InstanceDriver& vki, const vk::VkPhysicalDevice physicalDevice, bool hostVisible, deUint32 memoryBits)
{
	if (hostVisible)
	{
		const vk::VkPhysicalDeviceMemoryProperties properties(vk::getPhysicalDeviceMemoryProperties(vki, physicalDevice));
		return chooseHostVisibleMemoryType(memoryBits, properties);
	}

	return chooseMemoryType(memoryBits);
}

tcu::TestStatus testMemoryImportTwice (Context& context, MemoryTestConfig config)
{
	const vk::PlatformInterface&			vkp					(context.getPlatformInterface());
	const CustomInstance					instance			(createTestInstance(context, 0u, config.externalType, 0u));
	const vk::InstanceDriver&				vki					(instance.getDriver());
	const vk::VkPhysicalDevice				physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32							queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));
	const vk::Unique<vk::VkDevice>			device				(createTestDevice(context, vkp, instance, vki, physicalDevice, 0u, config.externalType, 0u, queueFamilyIndex));
	const vk::DeviceDriver					vkd					(vkp, instance, *device, context.getUsedApiVersion());
	const vk::VkBufferUsageFlags			usage				= vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT|vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	const deUint32							seed				= 1261033864u;
	const vk::VkDeviceSize					bufferSize			= 1024;
	const std::vector<deUint8>				testData			(genTestData(seed, (size_t)bufferSize));

	checkBufferSupport(vki, physicalDevice, config.externalType, 0u, usage, config.dedicated);

	// \note Buffer is only allocated to get memory requirements
	const vk::Unique<vk::VkBuffer>				buffer					(createExternalBuffer(vkd, *device, queueFamilyIndex, config.externalType, bufferSize, 0u, usage));
	const vk::VkMemoryRequirements				requirements			(getBufferMemoryRequirements(vkd, *device, *buffer));
	deUint32									exportedMemoryTypeIndex	(getExportedMemoryTypeIndex(vki, physicalDevice, config.hostVisible, requirements.memoryTypeBits));
	const vk::Unique<vk::VkDeviceMemory>		memory					(allocateExportableMemory(vkd, *device, requirements.size, exportedMemoryTypeIndex, config.externalType, config.dedicated ? *buffer : (vk::VkBuffer)0));
	NativeHandle								handleA;

	if (config.hostVisible)
		writeHostMemory(vkd, *device, *memory, testData.size(), &testData[0]);

	getMemoryNative(vkd, *device, *memory, config.externalType, handleA);

	// Need to query again memory type index since we are forced to have same type bits as the ahb buffer
	// Avoids VUID-VkMemoryAllocateInfo-memoryTypeIndex-02385
	if (config.externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID)
	{
		vk::VkAndroidHardwareBufferPropertiesANDROID	ahbProperties	=
		{
			vk::VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID,	// VkStructureType	sType
			DE_NULL,															// void*			pNext
			0u,																	// VkDeviceSize		allocationSize
			0u																	// uint32_t			memoryTypeBits
		};
		vkd.getAndroidHardwareBufferPropertiesANDROID(device.get(), handleA.getAndroidHardwareBuffer(), &ahbProperties);

		exportedMemoryTypeIndex	= chooseMemoryType(ahbProperties.memoryTypeBits);
	}

	{
		const vk::Unique<vk::VkBuffer>			bufferA	(createExternalBuffer(vkd, *device, queueFamilyIndex, config.externalType, bufferSize, 0u, usage));
		const vk::Unique<vk::VkBuffer>			bufferB	(createExternalBuffer(vkd, *device, queueFamilyIndex, config.externalType, bufferSize, 0u, usage));
		NativeHandle							handleB	(handleA);
		const vk::Unique<vk::VkDeviceMemory>	memoryA	(config.dedicated
														 ? importDedicatedMemory(vkd, *device, *bufferA, requirements, config.externalType, exportedMemoryTypeIndex, handleA)
														 : importMemory(vkd, *device, requirements, config.externalType, exportedMemoryTypeIndex, handleA));
		const vk::Unique<vk::VkDeviceMemory>	memoryB	(config.dedicated
														 ? importDedicatedMemory(vkd, *device, *bufferB, requirements, config.externalType, exportedMemoryTypeIndex, handleB)
														 : importMemory(vkd, *device, requirements, config.externalType, exportedMemoryTypeIndex, handleB));

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

tcu::TestStatus testMemoryMultipleImports (Context& context, MemoryTestConfig config)
{
	const size_t							count				= 4 * 1024;
	const vk::PlatformInterface&			vkp					(context.getPlatformInterface());
	const CustomInstance					instance			(createTestInstance(context, 0u, config.externalType, 0u));
	const vk::InstanceDriver&				vki					(instance.getDriver());
	const vk::VkPhysicalDevice				physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32							queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));
	const vk::Unique<vk::VkDevice>			device				(createTestDevice(context, vkp, instance, vki, physicalDevice, 0u, config.externalType, 0u, queueFamilyIndex));
	const vk::DeviceDriver					vkd					(vkp, instance, *device, context.getUsedApiVersion());
	const vk::VkBufferUsageFlags			usage				= vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT|vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	const vk::VkDeviceSize					bufferSize			= 1024;

	checkBufferSupport(vki, physicalDevice, config.externalType, 0u, usage, config.dedicated);

	// \note Buffer is only allocated to get memory requirements
	const vk::Unique<vk::VkBuffer>			buffer					(createExternalBuffer(vkd, *device, queueFamilyIndex, config.externalType, bufferSize, 0u, usage));
	const vk::VkMemoryRequirements			requirements			(getBufferMemoryRequirements(vkd, *device, *buffer));
	deUint32								exportedMemoryTypeIndex	(getExportedMemoryTypeIndex(vki, physicalDevice, config.hostVisible, requirements.memoryTypeBits));
	const vk::Unique<vk::VkDeviceMemory>	memory					(allocateExportableMemory(vkd, *device, requirements.size, exportedMemoryTypeIndex, config.externalType, config.dedicated ? *buffer : (vk::VkBuffer)0));
	NativeHandle							handleA;

	getMemoryNative(vkd, *device, *memory, config.externalType, handleA);

	// Need to query again memory type index since we are forced to have same type bits as the ahb buffer
	// Avoids VUID-VkMemoryAllocateInfo-memoryTypeIndex-02385
	if (config.externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID)
	{
		vk::VkAndroidHardwareBufferPropertiesANDROID	ahbProperties	=
		{
			vk::VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID,	// VkStructureType	sType
			DE_NULL,															// void*			pNext
			0u,																	// VkDeviceSize		allocationSize
			0u																	// uint32_t			memoryTypeBits
		};
		vkd.getAndroidHardwareBufferPropertiesANDROID(device.get(), handleA.getAndroidHardwareBuffer(), &ahbProperties);

		exportedMemoryTypeIndex	= chooseMemoryType(ahbProperties.memoryTypeBits);
	}

	for (size_t ndx = 0; ndx < count; ndx++)
	{
		const vk::Unique<vk::VkBuffer>			bufferB	(createExternalBuffer(vkd, *device, queueFamilyIndex, config.externalType, bufferSize, 0u, usage));
		NativeHandle							handleB	(handleA);
		const vk::Unique<vk::VkDeviceMemory>	memoryB	(config.dedicated
														 ? importDedicatedMemory(vkd, *device, *bufferB, requirements, config.externalType, exportedMemoryTypeIndex, handleB)
														 : importMemory(vkd, *device, requirements, config.externalType, exportedMemoryTypeIndex, handleB));
	}

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus testMemoryMultipleExports (Context& context, MemoryTestConfig config)
{
	const size_t							count				= 4 * 1024;
	const vk::PlatformInterface&			vkp					(context.getPlatformInterface());
	const CustomInstance					instance			(createTestInstance(context, 0u, config.externalType, 0u));
	const vk::InstanceDriver&				vki					(instance.getDriver());
	const vk::VkPhysicalDevice				physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32							queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));
	const vk::Unique<vk::VkDevice>			device				(createTestDevice(context, vkp, instance, vki, physicalDevice, 0u, config.externalType, 0u, queueFamilyIndex));
	const vk::DeviceDriver					vkd					(vkp, instance, *device, context.getUsedApiVersion());
	const vk::VkBufferUsageFlags			usage				= vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT|vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	const vk::VkDeviceSize					bufferSize			= 1024;

	checkBufferSupport(vki, physicalDevice, config.externalType, 0u, usage, config.dedicated);

	// \note Buffer is only allocated to get memory requirements
	const vk::Unique<vk::VkBuffer>			buffer					(createExternalBuffer(vkd, *device, queueFamilyIndex, config.externalType, bufferSize, 0u, usage));
	const vk::VkMemoryRequirements			requirements			(getBufferMemoryRequirements(vkd, *device, *buffer));
	const deUint32							exportedMemoryTypeIndex	(getExportedMemoryTypeIndex(vki, physicalDevice, config.hostVisible, requirements.memoryTypeBits));
	const vk::Unique<vk::VkDeviceMemory>	memory					(allocateExportableMemory(vkd, *device, requirements.size, exportedMemoryTypeIndex, config.externalType, config.dedicated ? *buffer : (vk::VkBuffer)0));

	for (size_t ndx = 0; ndx < count; ndx++)
	{
		NativeHandle	handle;
		getMemoryNative(vkd, *device, *memory, config.externalType, handle);
	}

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus testMemoryFdProperties (Context& context, MemoryTestConfig config)
{
	const vk::PlatformInterface&			vkp					(context.getPlatformInterface());
	const CustomInstance					instance			(createTestInstance(context, 0u, config.externalType, 0u));
	const vk::InstanceDriver&				vki					(instance.getDriver());
	const vk::VkPhysicalDevice				physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32							queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));
	const vk::Unique<vk::VkDevice>			device				(createTestDevice(context, vkp, instance, vki, physicalDevice, 0u, config.externalType, 0u, queueFamilyIndex));
	const vk::DeviceDriver					vkd					(vkp, instance, *device, context.getUsedApiVersion());
	const vk::VkBufferUsageFlags			usage				= vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT|vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	const vk::VkDeviceSize					bufferSize			= 1024;

	checkBufferSupport(vki, physicalDevice, config.externalType, 0u, usage, config.dedicated);

	// \note Buffer is only allocated to get memory requirements
	const vk::Unique<vk::VkBuffer>			buffer					(createExternalBuffer(vkd, *device, queueFamilyIndex, config.externalType, bufferSize, 0u, usage));
	const vk::VkMemoryRequirements			requirements			(getBufferMemoryRequirements(vkd, *device, *buffer));
	const deUint32							exportedMemoryTypeIndex	(getExportedMemoryTypeIndex(vki, physicalDevice, config.hostVisible, requirements.memoryTypeBits));
	const vk::Unique<vk::VkDeviceMemory>	memory					(allocateExportableMemory(vkd, *device, requirements.size, exportedMemoryTypeIndex, config.externalType, config.dedicated ? *buffer : (vk::VkBuffer)0));

	vk::VkMemoryFdPropertiesKHR	properties;
	NativeHandle				handle;

	getMemoryNative(vkd, *device, *memory, config.externalType, handle);
	properties.sType = vk::VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR;
	vk::VkResult res = vkd.getMemoryFdPropertiesKHR(*device, config.externalType, handle.getFd(), &properties);

	switch (config.externalType)
	{
		case vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT:
			TCU_CHECK_MSG(res == vk::VK_SUCCESS, "vkGetMemoryFdPropertiesKHR failed for VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT");
			break;
		default:
			// Invalid external memory type for this test.
			DE_ASSERT(false);
			break;
	}

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus testMemoryFdDup (Context& context, MemoryTestConfig config)
{
#if (DE_OS == DE_OS_ANDROID) || (DE_OS == DE_OS_UNIX)
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const CustomInstance				instance			(createTestInstance(context, 0u, config.externalType, 0u));
	const vk::InstanceDriver&			vki					(instance.getDriver());
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	{
		const vk::Unique<vk::VkDevice>			device			(createTestDevice(context, vkp, instance, vki, physicalDevice, 0u, config.externalType, 0u, queueFamilyIndex));
		const vk::DeviceDriver					vkd				(vkp, instance, *device, context.getUsedApiVersion());

		TestLog&								log				= context.getTestContext().getLog();
		const vk::VkBufferUsageFlags			usage			= vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT|vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		const vk::VkDeviceSize					bufferSize		= 1024;
		const deUint32							seed			= 851493858u;
		const std::vector<deUint8>				testData		(genTestData(seed, (size_t)bufferSize));

		checkBufferSupport(vki, physicalDevice, config.externalType, 0u, usage, config.dedicated);

		// \note Buffer is only allocated to get memory requirements
		const vk::Unique<vk::VkBuffer>			buffer					(createExternalBuffer(vkd, *device, queueFamilyIndex, config.externalType, bufferSize, 0u, usage));
		const vk::VkMemoryRequirements			requirements			(getBufferMemoryRequirements(vkd, *device, *buffer));
		const deUint32							exportedMemoryTypeIndex	(getExportedMemoryTypeIndex(vki, physicalDevice, config.hostVisible, requirements.memoryTypeBits));
		const vk::Unique<vk::VkDeviceMemory>	memory					(allocateExportableMemory(vkd, *device, requirements.size, exportedMemoryTypeIndex, config.externalType, config.dedicated ? *buffer : (vk::VkBuffer)0));

		if (config.hostVisible)
			writeHostMemory(vkd, *device, *memory, testData.size(), &testData[0]);

		const NativeHandle						fd				(getMemoryFd(vkd, *device, *memory, config.externalType));
		NativeHandle							newFd			(dup(fd.getFd()));

		if (newFd.getFd() < 0)
			log << TestLog::Message << "dup() failed: '" << strerror(errno) << "'" << TestLog::EndMessage;

		TCU_CHECK_MSG(newFd.getFd() >= 0, "Failed to call dup() for memorys fd");

		{
			const vk::Unique<vk::VkBuffer>			newBuffer	(createExternalBuffer(vkd, *device, queueFamilyIndex, config.externalType, bufferSize, 0u, usage));
			const vk::Unique<vk::VkDeviceMemory>	newMemory	(config.dedicated
																 ? importDedicatedMemory(vkd, *device, *newBuffer, requirements, config.externalType, exportedMemoryTypeIndex, newFd)
																 : importMemory(vkd, *device, requirements, config.externalType, exportedMemoryTypeIndex, newFd));

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
	const CustomInstance				instance			(createTestInstance(context, 0u, config.externalType, 0u));
	const vk::InstanceDriver&			vki					(instance.getDriver());
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	{
		const vk::Unique<vk::VkDevice>			device			(createTestDevice(context, vkp, instance, vki, physicalDevice, 0u, config.externalType, 0u, queueFamilyIndex));
		const vk::DeviceDriver					vkd				(vkp, instance, *device, context.getUsedApiVersion());

		TestLog&								log				= context.getTestContext().getLog();
		const vk::VkBufferUsageFlags			usage			= vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT|vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		const vk::VkDeviceSize					bufferSize		= 1024;
		const deUint32							seed			= 224466865u;
		const std::vector<deUint8>				testData		(genTestData(seed, (size_t)bufferSize));

		checkBufferSupport(vki, physicalDevice, config.externalType, 0u, usage, config.dedicated);

		// \note Buffer is only allocated to get memory requirements
		const vk::Unique<vk::VkBuffer>			buffer					(createExternalBuffer(vkd, *device, queueFamilyIndex, config.externalType, bufferSize, 0u, usage));
		const vk::VkMemoryRequirements			requirements			(getBufferMemoryRequirements(vkd, *device, *buffer));
		const deUint32							exportedMemoryTypeIndex	(getExportedMemoryTypeIndex(vki, physicalDevice, config.hostVisible, requirements.memoryTypeBits));
		const vk::Unique<vk::VkDeviceMemory>	memory					(allocateExportableMemory(vkd, *device, requirements.size, exportedMemoryTypeIndex, config.externalType, config.dedicated ? *buffer : (vk::VkBuffer)0));

		if (config.hostVisible)
			writeHostMemory(vkd, *device, *memory, testData.size(), &testData[0]);

		const NativeHandle						fd				(getMemoryFd(vkd, *device, *memory, config.externalType));
		NativeHandle							secondFd		(getMemoryFd(vkd, *device, *memory, config.externalType));
		const int								newFd			(dup2(fd.getFd(), secondFd.getFd()));

		if (newFd < 0)
			log << TestLog::Message << "dup2() failed: '" << strerror(errno) << "'" << TestLog::EndMessage;

		TCU_CHECK_MSG(newFd >= 0, "Failed to call dup2() for memorys fd");

		{
			const vk::Unique<vk::VkBuffer>			newBuffer	(createExternalBuffer(vkd, *device, queueFamilyIndex, config.externalType, bufferSize, 0u, usage));
			const vk::Unique<vk::VkDeviceMemory>	newMemory	(config.dedicated
																 ? importDedicatedMemory(vkd, *device, *newBuffer, requirements, config.externalType, exportedMemoryTypeIndex, secondFd)
																 : importMemory(vkd, *device, requirements, config.externalType, exportedMemoryTypeIndex, secondFd));

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
	const CustomInstance				instance			(createTestInstance(context, 0u, config.externalType, 0u));
	const vk::InstanceDriver&			vki					(instance.getDriver());
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	{
		const vk::Unique<vk::VkDevice>			device			(createTestDevice(context, vkp, instance, vki, physicalDevice, 0u, config.externalType, 0u, queueFamilyIndex));
		const vk::DeviceDriver					vkd				(vkp, instance, *device, context.getUsedApiVersion());

		TestLog&								log				= context.getTestContext().getLog();
		const vk::VkBufferUsageFlags			usage			= vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT|vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		const vk::VkDeviceSize					bufferSize		= 1024;
		const deUint32							seed			= 2554088961u;
		const std::vector<deUint8>				testData		(genTestData(seed, (size_t)bufferSize));

		checkBufferSupport(vki, physicalDevice, config.externalType, 0u, usage, config.dedicated);

		// \note Buffer is only allocated to get memory requirements
		const vk::Unique<vk::VkBuffer>			buffer					(createExternalBuffer(vkd, *device, queueFamilyIndex, config.externalType, bufferSize, 0u, usage));
		const vk::VkMemoryRequirements			requirements			(getBufferMemoryRequirements(vkd, *device, *buffer));
		const deUint32							exportedMemoryTypeIndex	(getExportedMemoryTypeIndex(vki, physicalDevice, config.hostVisible, requirements.memoryTypeBits));
		const vk::Unique<vk::VkDeviceMemory>	memory					(allocateExportableMemory(vkd, *device, requirements.size, exportedMemoryTypeIndex, config.externalType, config.dedicated ? *buffer : (vk::VkBuffer)0));

		if (config.hostVisible)
			writeHostMemory(vkd, *device, *memory, testData.size(), &testData[0]);

		const NativeHandle						fd				(getMemoryFd(vkd, *device, *memory, config.externalType));
		NativeHandle							secondFd		(getMemoryFd(vkd, *device, *memory, config.externalType));
		const int								newFd			(dup3(fd.getFd(), secondFd.getFd(), 0));

		if (newFd < 0)
			log << TestLog::Message << "dup3() failed: '" << strerror(errno) << "'" << TestLog::EndMessage;

		TCU_CHECK_MSG(newFd >= 0, "Failed to call dup3() for memorys fd");

		{
			const vk::Unique<vk::VkBuffer>			newBuffer	(createExternalBuffer(vkd, *device, queueFamilyIndex, config.externalType, bufferSize, 0u, usage));
			const vk::Unique<vk::VkDeviceMemory>	newMemory	(config.dedicated
																 ? importDedicatedMemory(vkd, *device, *newBuffer, requirements, config.externalType, exportedMemoryTypeIndex, secondFd)
																 : importMemory(vkd, *device, requirements, config.externalType, exportedMemoryTypeIndex, secondFd));

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
	const CustomInstance						instance			(createTestInstance(context, 0u, config.externalType, 0u));
	const vk::InstanceDriver&					vki					(instance.getDriver());
	const vk::VkPhysicalDevice					physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32								queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	{
		const vk::Unique<vk::VkDevice>			device				(createTestDevice(context, vkp, instance, vki, physicalDevice, 0u, config.externalType, 0u, queueFamilyIndex));
		const vk::DeviceDriver					vkd					(vkp, instance, *device, context.getUsedApiVersion());

		TestLog&								log					= context.getTestContext().getLog();
		const vk::VkBufferUsageFlags			usage				= vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT|vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		const vk::VkDeviceSize					bufferSize			= 1024;
		const deUint32							seed				= 3403586456u;
		const std::vector<deUint8>				testData			(genTestData(seed, (size_t)bufferSize));

		checkBufferSupport(vki, physicalDevice, config.externalType, 0u, usage, config.dedicated);

		// \note Buffer is only allocated to get memory requirements
		const vk::Unique<vk::VkBuffer>			buffer					(createExternalBuffer(vkd, *device, queueFamilyIndex, config.externalType, bufferSize, 0u, usage));
		const vk::VkMemoryRequirements			requirements			(getBufferMemoryRequirements(vkd, *device, *buffer));
		const deUint32							exportedMemoryTypeIndex	(getExportedMemoryTypeIndex(vki, physicalDevice, config.hostVisible, requirements.memoryTypeBits));
		const vk::Unique<vk::VkDeviceMemory>	memory					(allocateExportableMemory(vkd, *device, requirements.size, exportedMemoryTypeIndex, config.externalType, config.dedicated ? *buffer : (vk::VkBuffer)0));

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
							const vk::Unique<vk::VkBuffer>			newBuffer	(createExternalBuffer(vkd, *device, queueFamilyIndex, config.externalType, bufferSize, 0u, usage));
							const vk::Unique<vk::VkDeviceMemory>	newMemory	(config.dedicated
																				 ? importDedicatedMemory(vkd, *device, *newBuffer, requirements, config.externalType, exportedMemoryTypeIndex, newFd)
																				 : importMemory(vkd, *device, requirements, config.externalType, exportedMemoryTypeIndex, newFd));

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

struct BufferTestConfig
{
											BufferTestConfig	(vk::VkExternalMemoryHandleTypeFlagBits		externalType_,
																 bool										dedicated_)
		: externalType	(externalType_)
		, dedicated		(dedicated_)
	{
	}

	vk::VkExternalMemoryHandleTypeFlagBits	externalType;
	bool									dedicated;
};

tcu::TestStatus testBufferBindExportImportBind (Context&				context,
												const BufferTestConfig	config)
{
	const vk::PlatformInterface&			vkp					(context.getPlatformInterface());
	const CustomInstance					instance			(createTestInstance(context, 0u, config.externalType, 0u));
	const vk::InstanceDriver&				vki					(instance.getDriver());
	const vk::VkPhysicalDevice				physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32							queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));
	const vk::Unique<vk::VkDevice>			device				(createTestDevice(context, vkp, instance, vki, physicalDevice, 0u, config.externalType, 0u, queueFamilyIndex, config.dedicated));
	const vk::DeviceDriver					vkd					(vkp, instance, *device, context.getUsedApiVersion());
	const vk::VkBufferUsageFlags			usage				= vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT|vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	const vk::VkDeviceSize					bufferSize			= 1024;

	checkBufferSupport(vki, physicalDevice, config.externalType, 0u, usage, config.dedicated);

	// \note Buffer is only allocated to get memory requirements
	const vk::Unique<vk::VkBuffer>			bufferA					(createExternalBuffer(vkd, *device, queueFamilyIndex, config.externalType, bufferSize, 0u, usage));
	const vk::VkMemoryRequirements			requirements			(getBufferMemoryRequirements(vkd, *device, *bufferA));
	deUint32								exportedMemoryTypeIndex	(chooseMemoryType(requirements.memoryTypeBits));
	const vk::Unique<vk::VkDeviceMemory>	memoryA					(allocateExportableMemory(vkd, *device, requirements.size, exportedMemoryTypeIndex, config.externalType, config.dedicated ? *bufferA : (vk::VkBuffer)0));
	NativeHandle							handle;

	VK_CHECK(vkd.bindBufferMemory(*device, *bufferA, *memoryA, 0u));

	getMemoryNative(vkd, *device, *memoryA, config.externalType, handle);

	// Need to query again memory type index since we are forced to have same type bits as the ahb buffer
	// Avoids VUID-VkMemoryAllocateInfo-memoryTypeIndex-02385
	if (config.externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID)
	{
		vk::VkAndroidHardwareBufferPropertiesANDROID	ahbProperties	=
		{
			vk::VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID,	// VkStructureType	sType
			DE_NULL,															// void*			pNext
			0u,																	// VkDeviceSize		allocationSize
			0u																	// uint32_t			memoryTypeBits
		};
		vkd.getAndroidHardwareBufferPropertiesANDROID(device.get(), handle.getAndroidHardwareBuffer(), &ahbProperties);

		exportedMemoryTypeIndex	= chooseMemoryType(ahbProperties.memoryTypeBits);
	}

	{
		const vk::Unique<vk::VkBuffer>			bufferB	(createExternalBuffer(vkd, *device, queueFamilyIndex, config.externalType, bufferSize, 0u, usage));
		const vk::Unique<vk::VkDeviceMemory>	memoryB	(config.dedicated
														 ? importDedicatedMemory(vkd, *device, *bufferB, requirements, config.externalType, exportedMemoryTypeIndex, handle)
														 : importMemory(vkd, *device, requirements, config.externalType, exportedMemoryTypeIndex, handle));

		VK_CHECK(vkd.bindBufferMemory(*device, *bufferB, *memoryB, 0u));
	}

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus testBufferExportBindImportBind (Context&				context,
												const BufferTestConfig	config)
{
	const vk::PlatformInterface&			vkp					(context.getPlatformInterface());
	const CustomInstance					instance			(createTestInstance(context, 0u, config.externalType, 0u));
	const vk::InstanceDriver&				vki					(instance.getDriver());
	const vk::VkPhysicalDevice				physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32							queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));
	const vk::Unique<vk::VkDevice>			device				(createTestDevice(context, vkp, instance, vki, physicalDevice, 0u, config.externalType, 0u, queueFamilyIndex, config.dedicated));
	const vk::DeviceDriver					vkd					(vkp, instance, *device, context.getUsedApiVersion());
	const vk::VkBufferUsageFlags			usage				= vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT|vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	const vk::VkDeviceSize					bufferSize			= 1024;

	checkBufferSupport(vki, physicalDevice, config.externalType, 0u, usage, config.dedicated);

	// \note Buffer is only allocated to get memory requirements
	const vk::Unique<vk::VkBuffer>			bufferA					(createExternalBuffer(vkd, *device, queueFamilyIndex, config.externalType, bufferSize, 0u, usage));
	const vk::VkMemoryRequirements			requirements			(getBufferMemoryRequirements(vkd, *device, *bufferA));
	deUint32								exportedMemoryTypeIndex	(chooseMemoryType(requirements.memoryTypeBits));
	const vk::Unique<vk::VkDeviceMemory>	memoryA					(allocateExportableMemory(vkd, *device, requirements.size, exportedMemoryTypeIndex, config.externalType, config.dedicated ? *bufferA : (vk::VkBuffer)0));
	NativeHandle							handle;

	getMemoryNative(vkd, *device, *memoryA, config.externalType, handle);
	VK_CHECK(vkd.bindBufferMemory(*device, *bufferA, *memoryA, 0u));

	// Need to query again memory type index since we are forced to have same type bits as the ahb buffer
	// Avoids VUID-VkMemoryAllocateInfo-memoryTypeIndex-02385
	if (config.externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID)
	{
		vk::VkAndroidHardwareBufferPropertiesANDROID	ahbProperties	=
		{
			vk::VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID,	// VkStructureType	sType
			DE_NULL,															// void*			pNext
			0u,																	// VkDeviceSize		allocationSize
			0u																	// uint32_t			memoryTypeBits
		};
		vkd.getAndroidHardwareBufferPropertiesANDROID(device.get(), handle.getAndroidHardwareBuffer(), &ahbProperties);

		exportedMemoryTypeIndex	= chooseMemoryType(ahbProperties.memoryTypeBits);
	}

	{
		const vk::Unique<vk::VkBuffer>			bufferB	(createExternalBuffer(vkd, *device, queueFamilyIndex, config.externalType, bufferSize, 0u, usage));
		const vk::Unique<vk::VkDeviceMemory>	memoryB	(config.dedicated
														 ? importDedicatedMemory(vkd, *device, *bufferB, requirements, config.externalType, exportedMemoryTypeIndex, handle)
														 : importMemory(vkd, *device, requirements, config.externalType, exportedMemoryTypeIndex, handle));

		VK_CHECK(vkd.bindBufferMemory(*device, *bufferB, *memoryB, 0u));
	}

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus testBufferExportImportBindBind (Context&				context,
												const BufferTestConfig	config)
{
	const vk::PlatformInterface&			vkp					(context.getPlatformInterface());
	const CustomInstance					instance			(createTestInstance(context, 0u, config.externalType, 0u));
	const vk::InstanceDriver&				vki					(instance.getDriver());
	const vk::VkPhysicalDevice				physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32							queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));
	const vk::Unique<vk::VkDevice>			device				(createTestDevice(context, vkp, instance, vki, physicalDevice, 0u, config.externalType, 0u, queueFamilyIndex, config.dedicated));
	const vk::DeviceDriver					vkd					(vkp, instance, *device, context.getUsedApiVersion());
	const vk::VkBufferUsageFlags			usage				= vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT|vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	const vk::VkDeviceSize					bufferSize			= 1024;

	checkBufferSupport(vki, physicalDevice, config.externalType, 0u, usage, config.dedicated);

	// \note Buffer is only allocated to get memory requirements
	const vk::Unique<vk::VkBuffer>			bufferA					(createExternalBuffer(vkd, *device, queueFamilyIndex, config.externalType, bufferSize, 0u, usage));
	const vk::VkMemoryRequirements			requirements			(getBufferMemoryRequirements(vkd, *device, *bufferA));
	deUint32								exportedMemoryTypeIndex	(chooseMemoryType(requirements.memoryTypeBits));
	const vk::Unique<vk::VkDeviceMemory>	memoryA					(allocateExportableMemory(vkd, *device, requirements.size, exportedMemoryTypeIndex, config.externalType, config.dedicated ? *bufferA : (vk::VkBuffer)0));
	NativeHandle							handle;

	getMemoryNative(vkd, *device, *memoryA, config.externalType, handle);

	// Need to query again memory type index since we are forced to have same type bits as the ahb buffer
	// Avoids VUID-VkMemoryAllocateInfo-memoryTypeIndex-02385
	if (config.externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID)
	{
		vk::VkAndroidHardwareBufferPropertiesANDROID	ahbProperties	=
		{
			vk::VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID,	// VkStructureType	sType
			DE_NULL,															// void*			pNext
			0u,																	// VkDeviceSize		allocationSize
			0u																	// uint32_t			memoryTypeBits
		};
		vkd.getAndroidHardwareBufferPropertiesANDROID(device.get(), handle.getAndroidHardwareBuffer(), &ahbProperties);

		exportedMemoryTypeIndex	= chooseMemoryType(ahbProperties.memoryTypeBits);
	}

	{
		const vk::Unique<vk::VkBuffer>			bufferB	(createExternalBuffer(vkd, *device, queueFamilyIndex, config.externalType, bufferSize, 0u, usage));
		const vk::Unique<vk::VkDeviceMemory>	memoryB	(config.dedicated
														 ? importDedicatedMemory(vkd, *device, *bufferB, requirements, config.externalType, exportedMemoryTypeIndex, handle)
														 : importMemory(vkd, *device, requirements, config.externalType, exportedMemoryTypeIndex, handle));

		VK_CHECK(vkd.bindBufferMemory(*device, *bufferA, *memoryA, 0u));
		VK_CHECK(vkd.bindBufferMemory(*device, *bufferB, *memoryB, 0u));
	}

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus testImageQueries (Context& context, vk::VkExternalMemoryHandleTypeFlagBits externalType)
{
	const vk::VkImageCreateFlags		createFlags[]		=
	{
		0u,
		vk::VK_IMAGE_CREATE_SPARSE_BINDING_BIT,
		vk::VK_IMAGE_CREATE_SPARSE_BINDING_BIT|vk::VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT,
		vk::VK_IMAGE_CREATE_SPARSE_BINDING_BIT|vk::VK_IMAGE_CREATE_SPARSE_ALIASED_BIT,
		vk::VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT,
		vk::VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT
	};
	const vk::VkImageUsageFlags			usageFlags[]		=
	{
		vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		vk::VK_IMAGE_USAGE_SAMPLED_BIT,
		vk::VK_IMAGE_USAGE_STORAGE_BIT,
		vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		vk::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		vk::VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		vk::VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		vk::VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
		vk::VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
	};
	const vk::PlatformInterface&		vkp					(context.getPlatformInterface());
	const CustomInstance				instance			(createTestInstance(context, 0u, externalType, 0u));
	const vk::InstanceDriver&			vki					(instance.getDriver());
	const vk::VkPhysicalDevice			physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const vk::VkPhysicalDeviceFeatures	deviceFeatures		(vk::getPhysicalDeviceFeatures(vki, physicalDevice));
	const deUint32						queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));

	// VkDevice is only created if physical device claims to support any of these types.
	vk::Move<vk::VkDevice>				device;
	de::MovePtr<vk::DeviceDriver>		vkd;
	bool								deviceHasDedicated	= false;

	TestLog&							log					= context.getTestContext().getLog();

	for (size_t createFlagNdx = 0; createFlagNdx < DE_LENGTH_OF_ARRAY(createFlags); createFlagNdx++)
	for (size_t usageFlagNdx = 0; usageFlagNdx < DE_LENGTH_OF_ARRAY(usageFlags); usageFlagNdx++)
	{
		const vk::VkImageViewCreateFlags						createFlag		= createFlags[createFlagNdx];
		const vk::VkImageUsageFlags								usageFlag		= usageFlags[usageFlagNdx];
		const vk::VkFormat										format			=
				(usageFlags[usageFlagNdx] & vk::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) ? vk::VK_FORMAT_D16_UNORM : vk::VK_FORMAT_R8G8B8A8_UNORM;
		const vk::VkImageType									type			= vk::VK_IMAGE_TYPE_2D;
		const vk::VkImageTiling									tiling			= vk::VK_IMAGE_TILING_OPTIMAL;
		const vk::VkPhysicalDeviceExternalImageFormatInfo		externalInfo	=
		{
			vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO,
			DE_NULL,
			externalType
		};
		const vk::VkPhysicalDeviceImageFormatInfo2				info			=
		{
			vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
			&externalInfo,

			format,
			type,
			tiling,
			usageFlag,
			createFlag,
		};
		vk::VkExternalImageFormatProperties						externalProperties	=
		{
			vk::VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES,
			DE_NULL,
			{ 0u, 0u, 0u }
		};
		vk::VkImageFormatProperties2							properties			=
		{
			vk::VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
			&externalProperties,
			{
				{ 0u, 0u, 0u },
				0u,
				0u,
				0u,
				0u
			}
		};

		if (((createFlag & vk::VK_IMAGE_CREATE_SPARSE_BINDING_BIT) != 0) &&
			(deviceFeatures.sparseBinding == VK_FALSE))
			continue;

		if (((createFlag & vk::VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT) != 0) &&
			(deviceFeatures.sparseResidencyImage2D == VK_FALSE))
			continue;

		if (((createFlag & vk::VK_IMAGE_CREATE_SPARSE_ALIASED_BIT) != 0) &&
			(deviceFeatures.sparseResidencyAliased == VK_FALSE))
			continue;

		if (vki.getPhysicalDeviceImageFormatProperties2(physicalDevice, &info, &properties) == vk::VK_ERROR_FORMAT_NOT_SUPPORTED) {
			continue;
		}

		log << TestLog::Message << externalProperties << TestLog::EndMessage;
		TCU_CHECK(externalProperties.sType == vk::VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES);
		TCU_CHECK(externalProperties.pNext == DE_NULL);
		// \todo [2017-06-06 pyry] Can we validate anything else? Compatible types?

		if ((externalProperties.externalMemoryProperties.externalMemoryFeatures & (vk::VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT|vk::VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT)) != 0)
		{
			const bool	requiresDedicated	= (externalProperties.externalMemoryProperties.externalMemoryFeatures & vk::VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT) != 0;

			if (!device || (requiresDedicated && !deviceHasDedicated))
			{
				// \note We need to re-create with dedicated mem extensions if previous device instance didn't have them
				try
				{
					device				= createTestDevice(context, vkp, instance, vki, physicalDevice, 0u, externalType, 0u, queueFamilyIndex, requiresDedicated);
					vkd					= de::MovePtr<vk::DeviceDriver>(new vk::DeviceDriver(vkp, instance, *device, context.getUsedApiVersion()));
					deviceHasDedicated	= requiresDedicated;
				}
				catch (const tcu::NotSupportedError& e)
				{
					log << e;
					TCU_FAIL("Physical device claims to support handle type but required extensions are not supported");
				}
			}
		}

		if ((externalProperties.externalMemoryProperties.externalMemoryFeatures & vk::VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT) != 0)
		{
			DE_ASSERT(!!device);
			DE_ASSERT(vkd);

			if (deviceHasDedicated)
			{
				// Memory requirements cannot be queried without binding the image.
				if (externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID)
					continue;

				const vk::Unique<vk::VkImage>				image						(createExternalImage(*vkd, *device, queueFamilyIndex, externalType, format, 16u, 16u, tiling, createFlag, usageFlag));
				const vk::VkMemoryDedicatedRequirements		reqs						(getMemoryDedicatedRequirements(*vkd, *device, *image));
				const bool									propertiesRequiresDedicated	= (externalProperties.externalMemoryProperties.externalMemoryFeatures & vk::VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT) != 0;
				const bool									objectRequiresDedicated		= (reqs.requiresDedicatedAllocation != VK_FALSE);

				if (propertiesRequiresDedicated != objectRequiresDedicated)
					TCU_FAIL("vkGetPhysicalDeviceExternalBufferProperties and vkGetBufferMemoryRequirements2 report different dedicated requirements");
			}
			else
			{
				// We can't query whether dedicated memory is required or not on per-object basis.
				// This check should be redundant as the code above tries to create device with
				// VK_KHR_dedicated_allocation & VK_KHR_get_memory_requirements2 if dedicated memory
				// is required. However, checking again doesn't hurt.
				TCU_CHECK((externalProperties.externalMemoryProperties.externalMemoryFeatures & vk::VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT) == 0);
			}
		}
	}

	return tcu::TestStatus::pass("Pass");
}

struct ImageTestConfig
{
											ImageTestConfig	(vk::VkExternalMemoryHandleTypeFlagBits		externalType_,
															 bool										dedicated_)
		: externalType	(externalType_)
		, dedicated		(dedicated_)
	{
	}

	vk::VkExternalMemoryHandleTypeFlagBits	externalType;
	bool									dedicated;
};

tcu::TestStatus testImageBindExportImportBind (Context&					context,
											   const ImageTestConfig	config)
{
	const vk::PlatformInterface&			vkp					(context.getPlatformInterface());
	const CustomInstance					instance			(createTestInstance(context, 0u, config.externalType, 0u));
	const vk::InstanceDriver&				vki					(instance.getDriver());
	const vk::VkPhysicalDevice				physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32							queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));
	const vk::Unique<vk::VkDevice>			device				(createTestDevice(context, vkp, instance, vki, physicalDevice, 0u, config.externalType, 0u, queueFamilyIndex, config.dedicated));
	const vk::DeviceDriver					vkd					(vkp, instance, *device, context.getUsedApiVersion());
	const vk::VkImageUsageFlags				usage				= vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT | vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT | (config.externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID ? vk::VK_IMAGE_USAGE_SAMPLED_BIT : 0);
	const vk::VkFormat						format				= vk::VK_FORMAT_R8G8B8A8_UNORM;
	const deUint32							width				= 64u;
	const deUint32							height				= 64u;
	const vk::VkImageTiling					tiling				= vk::VK_IMAGE_TILING_OPTIMAL;

	checkImageSupport(vki, physicalDevice, config.externalType, 0u, usage, format, tiling, config.dedicated);

	const vk::Unique<vk::VkImage>			imageA					(createExternalImage(vkd, *device, queueFamilyIndex, config.externalType, format, width, height, tiling, 0u, usage));
	const vk::VkMemoryRequirements			requirements			(getImageMemoryRequirements(vkd, *device, *imageA, config.externalType));
	const deUint32							exportedMemoryTypeIndex	(chooseMemoryType(requirements.memoryTypeBits));
	const vk::Unique<vk::VkDeviceMemory>	memoryA					(allocateExportableMemory(vkd, *device, requirements.size, exportedMemoryTypeIndex, config.externalType, config.dedicated ? *imageA : (vk::VkImage)0));
	NativeHandle							handle;

	VK_CHECK(vkd.bindImageMemory(*device, *imageA, *memoryA, 0u));

	getMemoryNative(vkd, *device, *memoryA, config.externalType, handle);

	{
		const vk::Unique<vk::VkImage>			imageB	(createExternalImage(vkd, *device, queueFamilyIndex, config.externalType, format, width, height, tiling, 0u, usage));
		const deUint32							idx		= config.externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID ? ~0u : exportedMemoryTypeIndex;
		const vk::Unique<vk::VkDeviceMemory>	memoryB	(config.dedicated
														 ? importDedicatedMemory(vkd, *device, *imageB, requirements, config.externalType, idx, handle)
														 : importMemory(vkd, *device, requirements, config.externalType, idx, handle));

		VK_CHECK(vkd.bindImageMemory(*device, *imageB, *memoryB, 0u));
	}

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus testImageExportBindImportBind (Context&					context,
											   const ImageTestConfig	config)
{
	const vk::PlatformInterface&			vkp					(context.getPlatformInterface());
	const CustomInstance					instance			(createTestInstance(context, 0u, config.externalType, 0u));
	const vk::InstanceDriver&				vki					(instance.getDriver());
	const vk::VkPhysicalDevice				physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32							queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));
	const vk::Unique<vk::VkDevice>			device				(createTestDevice(context, vkp, instance, vki, physicalDevice, 0u, config.externalType, 0u, queueFamilyIndex, config.dedicated));
	const vk::DeviceDriver					vkd					(vkp, instance, *device, context.getUsedApiVersion());
	const vk::VkImageUsageFlags				usage				= vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT | vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT | (config.externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID ? vk::VK_IMAGE_USAGE_SAMPLED_BIT : 0);
	const vk::VkFormat						format				= vk::VK_FORMAT_R8G8B8A8_UNORM;
	const deUint32							width				= 64u;
	const deUint32							height				= 64u;
	const vk::VkImageTiling					tiling				= vk::VK_IMAGE_TILING_OPTIMAL;

	checkImageSupport(vki, physicalDevice, config.externalType, 0u, usage, format, tiling, config.dedicated);

	const vk::Unique<vk::VkImage>			imageA					(createExternalImage(vkd, *device, queueFamilyIndex, config.externalType, format, width, height, tiling, 0u, usage));
	const vk::VkMemoryRequirements			requirements			(getImageMemoryRequirements(vkd, *device, *imageA, config.externalType));
	const deUint32							exportedMemoryTypeIndex	(chooseMemoryType(requirements.memoryTypeBits));
	const vk::Unique<vk::VkDeviceMemory>	memoryA					(allocateExportableMemory(vkd, *device, requirements.size, exportedMemoryTypeIndex, config.externalType, config.dedicated ? *imageA : (vk::VkImage)0));
	NativeHandle							handle;

	if (config.externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID && config.dedicated)
	{
		// AHB required the image memory to be bound first.
		VK_CHECK(vkd.bindImageMemory(*device, *imageA, *memoryA, 0u));
		getMemoryNative(vkd, *device, *memoryA, config.externalType, handle);
	}
	else
	{
		getMemoryNative(vkd, *device, *memoryA, config.externalType, handle);
		VK_CHECK(vkd.bindImageMemory(*device, *imageA, *memoryA, 0u));
	}

	{
		const vk::Unique<vk::VkImage>			imageB	(createExternalImage(vkd, *device, queueFamilyIndex, config.externalType, format, width, height, tiling, 0u, usage));
		const deUint32							idx		= config.externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID ? ~0u : exportedMemoryTypeIndex;
		const vk::Unique<vk::VkDeviceMemory>	memoryB	(config.dedicated
														 ? importDedicatedMemory(vkd, *device, *imageB, requirements, config.externalType, idx, handle)
														 : importMemory(vkd, *device, requirements, config.externalType, idx, handle));

		VK_CHECK(vkd.bindImageMemory(*device, *imageB, *memoryB, 0u));
	}

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus testImageExportImportBindBind (Context&					context,
											   const ImageTestConfig	config)
{
	const vk::PlatformInterface&			vkp					(context.getPlatformInterface());
	const CustomInstance					instance			(createTestInstance(context, 0u, config.externalType, 0u));
	const vk::InstanceDriver&				vki					(instance.getDriver());
	const vk::VkPhysicalDevice				physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));
	const deUint32							queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, 0u));
	const vk::Unique<vk::VkDevice>			device				(createTestDevice(context, vkp, instance, vki, physicalDevice, 0u, config.externalType, 0u, queueFamilyIndex, config.dedicated));
	const vk::DeviceDriver					vkd					(vkp, instance, *device, context.getUsedApiVersion());
	const vk::VkImageUsageFlags				usage				= vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT | vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT | (config.externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID ? vk::VK_IMAGE_USAGE_SAMPLED_BIT : 0);
	const vk::VkFormat						format				= vk::VK_FORMAT_R8G8B8A8_UNORM;
	const deUint32							width				= 64u;
	const deUint32							height				= 64u;
	const vk::VkImageTiling					tiling				= vk::VK_IMAGE_TILING_OPTIMAL;

	checkImageSupport(vki, physicalDevice, config.externalType, 0u, usage, format, tiling, config.dedicated);

	if (config.externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID && config.dedicated)
	{
		// AHB required the image memory to be bound first, which is not possible in this test.
		TCU_THROW(NotSupportedError, "Unsupported for Android Hardware Buffer");
	}

	// \note Image is only allocated to get memory requirements
	const vk::Unique<vk::VkImage>			imageA					(createExternalImage(vkd, *device, queueFamilyIndex, config.externalType, format, width, height, tiling, 0u, usage));
	const vk::VkMemoryRequirements			requirements			(getImageMemoryRequirements(vkd, *device, *imageA, config.externalType));
	const deUint32							exportedMemoryTypeIndex	(chooseMemoryType(requirements.memoryTypeBits));
	const vk::Unique<vk::VkDeviceMemory>	memoryA					(allocateExportableMemory(vkd, *device, requirements.size, exportedMemoryTypeIndex, config.externalType, config.dedicated ? *imageA : (vk::VkImage)0));
	NativeHandle							handle;

	getMemoryNative(vkd, *device, *memoryA, config.externalType, handle);

	{
		const vk::Unique<vk::VkImage>			imageB	(createExternalImage(vkd, *device, queueFamilyIndex, config.externalType, format, width, height, tiling, 0u, usage));
		const deUint32							idx		= config.externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID ? ~0u : exportedMemoryTypeIndex;
		const vk::Unique<vk::VkDeviceMemory>	memoryB	(config.dedicated
														 ? importDedicatedMemory(vkd, *device, *imageB, requirements, config.externalType, idx, handle)
														 : importMemory(vkd, *device, requirements, config.externalType, idx, handle));

		VK_CHECK(vkd.bindImageMemory(*device, *imageA, *memoryA, 0u));
		VK_CHECK(vkd.bindImageMemory(*device, *imageB, *memoryB, 0u));
	}

	return tcu::TestStatus::pass("Pass");
}

template<class TestConfig> void checkEvent (Context& context, const TestConfig)
{
	if (context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") && !context.getPortabilitySubsetFeatures().events)
		TCU_THROW(NotSupportedError, "VK_KHR_portability_subset: Events are not supported by this implementation");
}

template<class TestConfig> void checkSupport (Context& context, const TestConfig config)
{
	const Transference transference (getHandelTypeTransferences(config.externalType));
	if (transference == TRANSFERENCE_COPY)
		checkEvent(context, config);
}

de::MovePtr<tcu::TestCaseGroup> createFenceTests (tcu::TestContext& testCtx, vk::VkExternalFenceHandleTypeFlagBits externalType)
{
	const struct
	{
		const char* const	name;
		const Permanence	permanence;
	} permanences[] =
	{
		{ "temporary", PERMANENCE_TEMPORARY	},
		{ "permanent", PERMANENCE_PERMANENT	}
	};

	de::MovePtr<tcu::TestCaseGroup> fenceGroup (new tcu::TestCaseGroup(testCtx, externalFenceTypeToName(externalType), externalFenceTypeToName(externalType)));

	addFunctionCase(fenceGroup.get(), "info",	"Test external fence queries.",	testFenceQueries,	externalType);

	for (size_t permanenceNdx = 0; permanenceNdx < DE_LENGTH_OF_ARRAY(permanences); permanenceNdx++)
	{
		const Permanence		permanence		(permanences[permanenceNdx].permanence);
		const char* const		permanenceName	(permanences[permanenceNdx].name);
		const FenceTestConfig	config			(externalType, permanence);

		if (!isSupportedPermanence(externalType, permanence))
			continue;

		if (externalType == vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_BIT
			|| externalType == vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT)
		{
			addFunctionCase(fenceGroup.get(), std::string("create_win32_") + permanenceName,	"Test creating fence with win32 properties.",	testFenceWin32Create,	config);
		}

		addFunctionCaseWithPrograms(fenceGroup.get(), std::string("import_twice_") + permanenceName,				"Test importing fence twice.",											checkSupport,	initProgramsToGetNativeFd,	testFenceImportTwice,				config);
		addFunctionCaseWithPrograms(fenceGroup.get(), std::string("reimport_") + permanenceName,					"Test importing again over previously imported fence.",					checkSupport,	initProgramsToGetNativeFd,	testFenceImportReimport,			config);
		addFunctionCaseWithPrograms(fenceGroup.get(), std::string("import_multiple_times_") + permanenceName,		"Test importing fence multiple times.",									checkSupport,	initProgramsToGetNativeFd,	testFenceMultipleImports,			config);
		addFunctionCaseWithPrograms(fenceGroup.get(), std::string("signal_export_import_wait_") + permanenceName,	"Test signaling, exporting, importing and waiting for the sempahore.",	checkEvent,		initProgramsToGetNativeFd,	testFenceSignalExportImportWait,	config);
		addFunctionCaseWithPrograms(fenceGroup.get(), std::string("signal_import_") + permanenceName,				"Test signaling and importing the fence.",								checkSupport,	initProgramsToGetNativeFd,	testFenceSignalImport,				config);
		addFunctionCaseWithPrograms(fenceGroup.get(), std::string("reset_") + permanenceName,						"Test resetting the fence.",											checkEvent,		initProgramsToGetNativeFd,	testFenceReset,						config);
		addFunctionCaseWithPrograms(fenceGroup.get(), std::string("transference_") + permanenceName,				"Test fences transference.",											checkEvent,		initProgramsToGetNativeFd,	testFenceTransference,				config);

		if (externalType == vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT)
		{
			addFunctionCaseWithPrograms(fenceGroup.get(), std::string("import_signaled_") + permanenceName,			"Test import signaled fence fd.",										initProgramsToGetNativeFd,	testFenceImportSyncFdSignaled,		config);
		}

		if (externalType == vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT
			|| externalType == vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_FD_BIT)
		{
			// \note Not supported on WIN32 handles
			addFunctionCaseWithPrograms(fenceGroup.get(), std::string("export_multiple_times_") + permanenceName,	"Test exporting fence multiple times.",		checkSupport,	initProgramsToGetNativeFd,	testFenceMultipleExports,	config);

			addFunctionCaseWithPrograms(fenceGroup.get(), std::string("dup_") + permanenceName,						"Test calling dup() on exported fence.",	checkSupport,	initProgramsToGetNativeFd,	testFenceFdDup,				config);
			addFunctionCaseWithPrograms(fenceGroup.get(), std::string("dup2_") + permanenceName,					"Test calling dup2() on exported fence.",	checkSupport,	initProgramsToGetNativeFd,	testFenceFdDup2,			config);
			addFunctionCaseWithPrograms(fenceGroup.get(), std::string("dup3_") + permanenceName,					"Test calling dup3() on exported fence.",	checkSupport,	initProgramsToGetNativeFd,	testFenceFdDup3,			config);
			addFunctionCaseWithPrograms(fenceGroup.get(), std::string("send_over_socket_") + permanenceName,		"Test sending fence fd over socket.",		checkSupport,	initProgramsToGetNativeFd,	testFenceFdSendOverSocket,	config);
		}

		if (getHandelTypeTransferences(externalType) == TRANSFERENCE_REFERENCE)
		{
			addFunctionCase(fenceGroup.get(), std::string("signal_wait_import_") + permanenceName,			"Test signaling and then waiting for the the sepmahore.",			testFenceSignalWaitImport,			config);
			addFunctionCase(fenceGroup.get(), std::string("export_signal_import_wait_") + permanenceName,	"Test exporting, signaling, importing and waiting for the fence.",	testFenceExportSignalImportWait,	config);
			addFunctionCase(fenceGroup.get(), std::string("export_import_signal_wait_") + permanenceName,	"Test exporting, importing, signaling and waiting for the fence.",	testFenceExportImportSignalWait,	config);
		}
	}

	return fenceGroup;
}

void generateFailureText (TestLog& log, vk::VkFormat format, vk::VkImageUsageFlags usage, vk::VkImageCreateFlags create, vk::VkImageTiling tiling = static_cast<vk::VkImageTiling>(0), deUint32 width = 0, deUint32 height = 0, std::string exception = "")
{
	std::ostringstream combination;
	combination << "Test failure with combination: ";
	combination << " Format: "		<< getFormatName(format);
	combination << " Usageflags: "	<< vk::getImageUsageFlagsStr(usage);
	combination << " Createflags: "	<< vk::getImageCreateFlagsStr(create);
	combination << " Tiling: "		<< getImageTilingStr(tiling);
	if (width != 0 && height != 0)
		combination << " Size: " << "(" << width << ", " << height << ")";
	if (!exception.empty())
		combination << "Error message: " << exception;

	log << TestLog::Message << combination.str() << TestLog::EndMessage;
}

bool ValidateAHardwareBuffer (TestLog& log, vk::VkFormat format, deUint64 requiredAhbUsage, const vk::DeviceDriver& vkd, const vk::VkDevice& device, vk::VkImageUsageFlags usageFlag, vk::VkImageCreateFlags createFlag, deUint32 layerCount, bool& enableMaxLayerTest)
{
	DE_UNREF(createFlag);

	AndroidHardwareBufferExternalApi* ahbApi = AndroidHardwareBufferExternalApi::getInstance();

	if (!ahbApi)
	{
		TCU_THROW(NotSupportedError, "Platform doesn't support Android Hardware Buffer handles");
	}

#if (DE_OS == DE_OS_ANDROID)
	// If CubeMap create flag is used and AHB doesn't support CubeMap return false.
	if(!AndroidHardwareBufferExternalApi::supportsCubeMap() && (createFlag & vk::VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT))
		return false;
#endif

	vk::pt::AndroidHardwareBufferPtr ahb = ahbApi->allocate(64u, 64u, layerCount, ahbApi->vkFormatToAhbFormat(format), requiredAhbUsage);
	if (ahb.internal == DE_NULL)
	{
		enableMaxLayerTest = false;
		// try again with layerCount '1'
		ahb = ahbApi->allocate(64u, 64u, 1u, ahbApi->vkFormatToAhbFormat(format), requiredAhbUsage);
		if (ahb.internal == DE_NULL)
		{
			return false;
		}
	}
	NativeHandle nativeHandle(ahb);

	const vk::VkComponentMapping mappingA = { vk::VK_COMPONENT_SWIZZLE_IDENTITY, vk::VK_COMPONENT_SWIZZLE_IDENTITY, vk::VK_COMPONENT_SWIZZLE_IDENTITY, vk::VK_COMPONENT_SWIZZLE_IDENTITY };
	const vk::VkComponentMapping mappingB = { vk::VK_COMPONENT_SWIZZLE_R, vk::VK_COMPONENT_SWIZZLE_G, vk::VK_COMPONENT_SWIZZLE_B, vk::VK_COMPONENT_SWIZZLE_A };

	for (int variantIdx = 0; variantIdx < 2; ++variantIdx)
	{
		// Both mappings should be equivalent and work.
		const vk::VkComponentMapping& mapping = ((variantIdx == 0) ? mappingA : mappingB);

		vk::VkAndroidHardwareBufferFormatPropertiesANDROID formatProperties =
		{
			vk::VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID,
			DE_NULL,
			vk::VK_FORMAT_UNDEFINED,
			0u,
			0u,
			mapping,
			vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY,
			vk::VK_SAMPLER_YCBCR_RANGE_ITU_FULL,
			vk::VK_CHROMA_LOCATION_COSITED_EVEN,
			vk::VK_CHROMA_LOCATION_COSITED_EVEN
		};

		vk::VkAndroidHardwareBufferPropertiesANDROID bufferProperties =
		{
			vk::VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID,
			&formatProperties,
			0u,
			0u
		};

		try
		{
			VK_CHECK(vkd.getAndroidHardwareBufferPropertiesANDROID(device, ahb, &bufferProperties));
			TCU_CHECK(formatProperties.format != vk::VK_FORMAT_UNDEFINED);
			TCU_CHECK(formatProperties.format == format);
			TCU_CHECK(formatProperties.externalFormat != 0u);
			TCU_CHECK((formatProperties.formatFeatures & vk::VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0u);
			TCU_CHECK((formatProperties.formatFeatures & (vk::VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT | vk::VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT)) != 0u);
		}
		catch (const tcu::Exception& exception)
		{
			log << TestLog::Message << "Failure validating Android Hardware Buffer. See error message and combination: " << TestLog::EndMessage;
			generateFailureText(log, format, usageFlag, createFlag, static_cast<vk::VkImageTiling>(0), 0, 0, exception.getMessage());
			return false;
		}
	}

	return true;
}

tcu::TestStatus testAndroidHardwareBufferImageFormat (Context& context, vk::VkFormat format)
{
	AndroidHardwareBufferExternalApi* ahbApi = AndroidHardwareBufferExternalApi::getInstance();
	if (!ahbApi)
	{
		TCU_THROW(NotSupportedError, "Platform doesn't support Android Hardware Buffer handles");
	}

	bool testsFailed = false;

	const vk::VkExternalMemoryHandleTypeFlagBits  externalMemoryType  =	vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID;
	const vk::PlatformInterface&				  vkp					(context.getPlatformInterface());
	const CustomInstance						  instance				(createTestInstance(context, 0u, externalMemoryType, 0u));
	const vk::InstanceDriver&					  vki					(instance.getDriver());
	const vk::VkPhysicalDevice					  physicalDevice		(vk::chooseDevice(vki, instance, context.getTestContext().getCommandLine()));

	vk::VkPhysicalDeviceProtectedMemoryFeatures		protectedFeatures;
	protectedFeatures.sType				= vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES;
	protectedFeatures.pNext				= DE_NULL;
	protectedFeatures.protectedMemory	= VK_FALSE;

	vk::VkPhysicalDeviceFeatures2					deviceFeatures;
	deviceFeatures.sType		= vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	deviceFeatures.pNext		= &protectedFeatures;

	vki.getPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures);

	const deUint32								  queueFamilyIndex		(chooseQueueFamilyIndex(vki, physicalDevice, 0u));
	const vk::Unique<vk::VkDevice>				  device				(createTestDevice(context, vkp, instance, vki, physicalDevice, 0u, externalMemoryType, 0u, queueFamilyIndex, false, &protectedFeatures));
	const vk::DeviceDriver						  vkd					(vkp, instance, *device, context.getUsedApiVersion());
	TestLog&									  log				  = context.getTestContext().getLog();
	const vk::VkPhysicalDeviceLimits			  limits			  = getPhysicalDeviceProperties(vki, physicalDevice).limits;

	const vk::VkImageUsageFlagBits framebufferUsageFlag = vk::isDepthStencilFormat(format) ? vk::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
																						   : vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	const vk::VkImageUsageFlagBits				  usageFlags[]		  =
	{
		vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		vk::VK_IMAGE_USAGE_SAMPLED_BIT,
		vk::VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
		framebufferUsageFlag,
	};
	const vk::VkImageCreateFlagBits				  createFlags[]		  =
	{
		vk::VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT,
		vk::VK_IMAGE_CREATE_EXTENDED_USAGE_BIT,
		vk::VK_IMAGE_CREATE_PROTECTED_BIT,
		vk::VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
	};
	const vk::VkImageTiling						  tilings[]			  =
	{
		vk::VK_IMAGE_TILING_OPTIMAL,
		vk::VK_IMAGE_TILING_LINEAR,
	};
	deUint64 mustSupportAhbUsageFlags = ahbApi->mustSupportAhbUsageFlags();
	const size_t	one							= 1u;
	const size_t	numOfUsageFlags				= DE_LENGTH_OF_ARRAY(usageFlags);
	const size_t	numOfCreateFlags			= DE_LENGTH_OF_ARRAY(createFlags);
	const size_t	numOfFlagCombos				= one << (numOfUsageFlags + numOfCreateFlags);
	const size_t	numOfTilings				= DE_LENGTH_OF_ARRAY(tilings);

	for (size_t combo = 0; combo < numOfFlagCombos; combo++)
	{
		vk::VkImageUsageFlags	usage				= 0;
		vk::VkImageCreateFlags	createFlag			= 0;
		deUint64				requiredAhbUsage	= 0;
		bool					enableMaxLayerTest	= true;
		for (size_t usageNdx = 0; usageNdx < numOfUsageFlags; usageNdx++)
		{
			if ((combo & (one << usageNdx)) == 0)
				continue;
			usage |= usageFlags[usageNdx];
			requiredAhbUsage |= ahbApi->vkUsageToAhbUsage(usageFlags[usageNdx]);
		}
		for (size_t createFlagNdx = 0; createFlagNdx < numOfCreateFlags; createFlagNdx++)
		{
			const size_t	bit	= numOfUsageFlags + createFlagNdx;
			if ((combo & (one << bit)) == 0)
				continue;
			if (((createFlags[createFlagNdx] & vk::VK_IMAGE_CREATE_PROTECTED_BIT) == vk::VK_IMAGE_CREATE_PROTECTED_BIT ) &&
				(protectedFeatures.protectedMemory == VK_FALSE))
				continue;

			createFlag |= createFlags[createFlagNdx];
			requiredAhbUsage |= ahbApi->vkCreateToAhbUsage(createFlags[createFlagNdx]);
		}

		// Only test a combination if the usage flags include at least one of the AHARDWAREBUFFER_USAGE_GPU_* flag.
		if ((requiredAhbUsage & mustSupportAhbUsageFlags) == 0u)
			continue;

		// Only test a combination if AHardwareBuffer can be successfully allocated for it.
		if (!ValidateAHardwareBuffer(log, format, requiredAhbUsage, vkd, *device, usage, createFlag, limits.maxImageArrayLayers, enableMaxLayerTest))
			continue;

		bool foundAnyUsableTiling = false;
		for (size_t tilingIndex = 0; tilingIndex < numOfTilings; tilingIndex++)
		{
			const vk::VkImageTiling tiling = tilings[tilingIndex];

			const vk::VkPhysicalDeviceExternalImageFormatInfo	externalInfo		=
			{
				vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO,
				DE_NULL,
				vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID
			};
			const vk::VkPhysicalDeviceImageFormatInfo2			info				=
			{
				vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
				&externalInfo,
				format,
				vk::VK_IMAGE_TYPE_2D,
				tiling,
				usage,
				createFlag,
			};

			vk::VkAndroidHardwareBufferUsageANDROID				ahbUsageProperties	=
			{
				vk::VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_USAGE_ANDROID,
				DE_NULL,
				0u
			};
			vk::VkExternalImageFormatProperties					externalProperties	=
			{
				vk::VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES,
				&ahbUsageProperties,
				{ 0u, 0u, 0u }
			};
			vk::VkImageFormatProperties2						properties			=
			{
				vk::VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
				&externalProperties,
				{
					{ 0u, 0u, 0u },
					0u,
					0u,
					0u,
					0u
				}
			};

			if (vki.getPhysicalDeviceImageFormatProperties2(physicalDevice, &info, &properties) == vk::VK_ERROR_FORMAT_NOT_SUPPORTED)
			{
				log << TestLog::Message << "Tiling " << tiling << " is not supported." << TestLog::EndMessage;
				continue;
			}

			foundAnyUsableTiling = true;

			try
			{
				TCU_CHECK((externalProperties.externalMemoryProperties.externalMemoryFeatures & vk::VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT) != 0);
				TCU_CHECK((externalProperties.externalMemoryProperties.externalMemoryFeatures & vk::VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT) != 0);
				TCU_CHECK((externalProperties.externalMemoryProperties.externalMemoryFeatures & vk::VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT) != 0);
				deUint32 maxWidth   = properties.imageFormatProperties.maxExtent.width;
				deUint32 maxHeight  = properties.imageFormatProperties.maxExtent.height;
				TCU_CHECK(maxWidth >= 4096);
				TCU_CHECK(maxHeight >= 4096);
				// Even if not requested, at least one of GPU_* usage flags must be present.
				TCU_CHECK((ahbUsageProperties.androidHardwareBufferUsage & mustSupportAhbUsageFlags) != 0u);
				// The AHB usage flags corresponding to the create and usage flags used in info must be present.
				TCU_CHECK((ahbUsageProperties.androidHardwareBufferUsage & requiredAhbUsage) == requiredAhbUsage);
			}
			catch (const tcu::Exception& exception)
			{
				generateFailureText(log, format, usage, createFlag, tiling, 0, 0, exception.getMessage());
				testsFailed = true;
				continue;
			}

			log << TestLog::Message << "Required flags: " << std::hex << requiredAhbUsage << " Actual flags: " << std::hex << ahbUsageProperties.androidHardwareBufferUsage
				<< TestLog::EndMessage;

			struct ImageSize
			{
				deUint32 width;
				deUint32 height;
			};
			ImageSize sizes[] =
			{
				{64u, 64u},
				{1024u, 2096u},
			};

			deUint32 exportedMemoryTypeIndex = 0;

			if (createFlag & vk::VK_IMAGE_CREATE_PROTECTED_BIT)
			{
				const vk::VkPhysicalDeviceMemoryProperties memProperties(vk::getPhysicalDeviceMemoryProperties(vki, physicalDevice));

				for (deUint32 memoryTypeIndex = 0; memoryTypeIndex < VK_MAX_MEMORY_TYPES; memoryTypeIndex++)
				{
					if (memProperties.memoryTypes[memoryTypeIndex].propertyFlags & vk::VK_MEMORY_PROPERTY_PROTECTED_BIT)
					{
						exportedMemoryTypeIndex = memoryTypeIndex;
						break;
					}
				}
			}

			for (size_t i = 0; i < DE_LENGTH_OF_ARRAY(sizes); i++)
			{
				try
				{
					const vk::Unique<vk::VkImage>			image					(createExternalImage(vkd, *device, queueFamilyIndex, externalMemoryType, format, sizes[i].width, sizes[i].height, tiling, createFlag, usage));
					const vk::VkMemoryRequirements			requirements			(getImageMemoryRequirements(vkd, *device, *image, externalMemoryType));
					const vk::Unique<vk::VkDeviceMemory>	memory					(allocateExportableMemory(vkd, *device, requirements.size, exportedMemoryTypeIndex, externalMemoryType, *image));
					NativeHandle							handle;

					VK_CHECK(vkd.bindImageMemory(*device, *image, *memory, 0u));
					getMemoryNative(vkd, *device, *memory, externalMemoryType, handle);

					deUint32 ahbFormat = 0;
					deUint64 anhUsage  = 0;
					ahbApi->describe(handle.getAndroidHardwareBuffer(), DE_NULL, DE_NULL, DE_NULL, &ahbFormat, &anhUsage, DE_NULL);
					TCU_CHECK(ahbFormat == ahbApi->vkFormatToAhbFormat(format));
					TCU_CHECK((anhUsage & requiredAhbUsage) == requiredAhbUsage);

					// Let watchdog know we're alive
					context.getTestContext().touchWatchdog();
				}
				catch (const tcu::Exception& exception)
				{
					generateFailureText(log, format, usage, createFlag, tiling, sizes[i].width, sizes[i].height, exception.getMessage());
					testsFailed = true;
					continue;
				}
			}

			if (properties.imageFormatProperties.maxMipLevels >= 7u)
			{
				try
				{
					const vk::Unique<vk::VkImage>			image					(createExternalImage(vkd, *device, queueFamilyIndex, externalMemoryType, format, 64u, 64u, tiling, createFlag, usage, 7u));
					const vk::VkMemoryRequirements			requirements			(getImageMemoryRequirements(vkd, *device, *image, externalMemoryType));
					const vk::Unique<vk::VkDeviceMemory>	memory					(allocateExportableMemory(vkd, *device, requirements.size, exportedMemoryTypeIndex, externalMemoryType, *image));
					NativeHandle							handle;

					VK_CHECK(vkd.bindImageMemory(*device, *image, *memory, 0u));
					getMemoryNative(vkd, *device, *memory, externalMemoryType, handle);

					deUint32 ahbFormat = 0;
					deUint64 anhUsage  = 0;
					ahbApi->describe(handle.getAndroidHardwareBuffer(), DE_NULL, DE_NULL, DE_NULL, &ahbFormat, &anhUsage, DE_NULL);
					TCU_CHECK(ahbFormat == ahbApi->vkFormatToAhbFormat(format));
					TCU_CHECK((anhUsage & requiredAhbUsage) == requiredAhbUsage);
				}
				catch (const tcu::Exception& exception)
				{
					generateFailureText(log, format, usage, createFlag, tiling, 64, 64, exception.getMessage());
					testsFailed = true;
					continue;
				}
			}

			if ((properties.imageFormatProperties.maxArrayLayers > 1u) && enableMaxLayerTest)
			{
				try
				{
					const vk::Unique<vk::VkImage>			image					(createExternalImage(vkd, *device, queueFamilyIndex, externalMemoryType, format, 64u, 64u, tiling, createFlag, usage, 1u, properties.imageFormatProperties.maxArrayLayers));
					const vk::VkMemoryRequirements			requirements			(getImageMemoryRequirements(vkd, *device, *image, externalMemoryType));
					const vk::Unique<vk::VkDeviceMemory>	memory					(allocateExportableMemory(vkd, *device, requirements.size, exportedMemoryTypeIndex, externalMemoryType, *image));
					NativeHandle							handle;

					VK_CHECK(vkd.bindImageMemory(*device, *image, *memory, 0u));
					getMemoryNative(vkd, *device, *memory, externalMemoryType, handle);

					deUint32 ahbFormat = 0;
					deUint64 anhUsage  = 0;
					ahbApi->describe(handle.getAndroidHardwareBuffer(), DE_NULL, DE_NULL, DE_NULL, &ahbFormat, &anhUsage, DE_NULL);
					TCU_CHECK(ahbFormat == ahbApi->vkFormatToAhbFormat(format));
					TCU_CHECK((anhUsage & requiredAhbUsage) == requiredAhbUsage);
				}
				catch (const tcu::Exception& exception)
				{
					generateFailureText(log, format, usage, createFlag, tiling, 64, 64, exception.getMessage());
					testsFailed = true;
					continue;
				}
			}
		}

		if (!foundAnyUsableTiling)
		{
			generateFailureText(log, format, usage, createFlag, static_cast<vk::VkImageTiling>(0));
			testsFailed = true;
			continue;
		}
	}

	if (testsFailed)
		return tcu::TestStatus::fail("Failure in at least one subtest. Check log for failed tests.");
	else
		return tcu::TestStatus::pass("Pass");
}

de::MovePtr<tcu::TestCaseGroup> createFenceTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> fenceGroup (new tcu::TestCaseGroup(testCtx, "fence", "Tests for external fences."));

	fenceGroup->addChild(createFenceTests(testCtx, vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT).release());
	fenceGroup->addChild(createFenceTests(testCtx, vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_FD_BIT).release());
	fenceGroup->addChild(createFenceTests(testCtx, vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_BIT).release());
	fenceGroup->addChild(createFenceTests(testCtx, vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT).release());

	return fenceGroup;
}

de::MovePtr<tcu::TestCaseGroup> createSemaphoreTests (tcu::TestContext& testCtx, vk::VkExternalSemaphoreHandleTypeFlagBits externalType)
{
	const struct
	{
		const char* const	name;
		const Permanence	permanence;
	} permanences[] =
	{
		{ "temporary", PERMANENCE_TEMPORARY	},
		{ "permanent", PERMANENCE_PERMANENT	}
	};
	const struct
	{
		const char* const	name;
		vk::VkSemaphoreType	type;
	} semaphoreTypes[] =
	{
		{ "binary",		vk::VK_SEMAPHORE_TYPE_BINARY },
		{ "timeline",	vk::VK_SEMAPHORE_TYPE_TIMELINE },
	};

	de::MovePtr<tcu::TestCaseGroup> semaphoreGroup (new tcu::TestCaseGroup(testCtx, externalSemaphoreTypeToName(externalType), externalSemaphoreTypeToName(externalType)));

	for (size_t semaphoreTypeIdx = 0; semaphoreTypeIdx < DE_LENGTH_OF_ARRAY(permanences); semaphoreTypeIdx++)
	{
		addFunctionCase(semaphoreGroup.get(), std::string("info_") + semaphoreTypes[semaphoreTypeIdx].name,
						"Test external semaphore queries.",	testSemaphoreQueries,
						TestSemaphoreQueriesParameters(semaphoreTypes[semaphoreTypeIdx].type, externalType));
	}

	for (size_t permanenceNdx = 0; permanenceNdx < DE_LENGTH_OF_ARRAY(permanences); permanenceNdx++)
	{
		const Permanence			permanence		(permanences[permanenceNdx].permanence);
		const char* const			permanenceName	(permanences[permanenceNdx].name);
		const SemaphoreTestConfig	config			(externalType, permanence);

		if (!isSupportedPermanence(externalType, permanence))
			continue;

		if (externalType == vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT
			|| externalType == vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT)
		{
			addFunctionCase(semaphoreGroup.get(), std::string("create_win32_") + permanenceName,	"Test creating semaphore with win32 properties.",	testSemaphoreWin32Create,	config);
		}

		addFunctionCaseWithPrograms(semaphoreGroup.get(), std::string("import_twice_") + permanenceName,				"Test importing semaphore twice.",										checkSupport,	initProgramsToGetNativeFd,	testSemaphoreImportTwice,				config);
		addFunctionCaseWithPrograms(semaphoreGroup.get(), std::string("reimport_") + permanenceName,					"Test importing again over previously imported semaphore.",				checkSupport,	initProgramsToGetNativeFd,	testSemaphoreImportReimport,			config);
		addFunctionCaseWithPrograms(semaphoreGroup.get(), std::string("import_multiple_times_") + permanenceName,		"Test importing semaphore multiple times.",								checkSupport,	initProgramsToGetNativeFd,	testSemaphoreMultipleImports,			config);
		addFunctionCaseWithPrograms(semaphoreGroup.get(), std::string("signal_export_import_wait_") + permanenceName,	"Test signaling, exporting, importing and waiting for the sempahore.",	checkEvent,		initProgramsToGetNativeFd,	testSemaphoreSignalExportImportWait,	config);
		addFunctionCaseWithPrograms(semaphoreGroup.get(), std::string("signal_import_") + permanenceName,				"Test signaling and importing the semaphore.",							checkSupport,	initProgramsToGetNativeFd,	testSemaphoreSignalImport,				config);
		addFunctionCaseWithPrograms(semaphoreGroup.get(), std::string("transference_") + permanenceName,				"Test semaphores transference.",										checkEvent,		initProgramsToGetNativeFd,	testSemaphoreTransference,				config);

		if (externalType == vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT)
		{
			addFunctionCaseWithPrograms(semaphoreGroup.get(), std::string("import_signaled_") + permanenceName,			"Test import signaled semaphore fd.",										initProgramsToGetNativeFd,	testSemaphoreImportSyncFdSignaled,	config);
		}


		if (externalType == vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT
			|| externalType == vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT)
		{
			// \note Not supported on WIN32 handles
			addFunctionCaseWithPrograms(semaphoreGroup.get(), std::string("export_multiple_times_") + permanenceName,	"Test exporting semaphore multiple times.",		checkSupport,	initProgramsToGetNativeFd,	testSemaphoreMultipleExports,	config);

			addFunctionCaseWithPrograms(semaphoreGroup.get(), std::string("dup_") + permanenceName,						"Test calling dup() on exported semaphore.",	checkSupport,	initProgramsToGetNativeFd,	testSemaphoreFdDup,				config);
			addFunctionCaseWithPrograms(semaphoreGroup.get(), std::string("dup2_") + permanenceName,					"Test calling dup2() on exported semaphore.",	checkSupport,	initProgramsToGetNativeFd,	testSemaphoreFdDup2,			config);
			addFunctionCaseWithPrograms(semaphoreGroup.get(), std::string("dup3_") + permanenceName,					"Test calling dup3() on exported semaphore.",	checkSupport,	initProgramsToGetNativeFd,	testSemaphoreFdDup3,			config);
			addFunctionCaseWithPrograms(semaphoreGroup.get(), std::string("send_over_socket_") + permanenceName,		"Test sending semaphore fd over socket.",		checkSupport,	initProgramsToGetNativeFd,	testSemaphoreFdSendOverSocket,	config);
		}

		if (getHandelTypeTransferences(externalType) == TRANSFERENCE_REFERENCE)
		{
			addFunctionCase(semaphoreGroup.get(), std::string("signal_wait_import_") + permanenceName,			"Test signaling and then waiting for the the sepmahore.",				testSemaphoreSignalWaitImport,			config);
			addFunctionCase(semaphoreGroup.get(), std::string("export_signal_import_wait_") + permanenceName,	"Test exporting, signaling, importing and waiting for the semaphore.",	testSemaphoreExportSignalImportWait,	config);
			addFunctionCase(semaphoreGroup.get(), std::string("export_import_signal_wait_") + permanenceName,	"Test exporting, importing, signaling and waiting for the semaphore.",	checkEvent,		testSemaphoreExportImportSignalWait,	config);
		}
	}

	return semaphoreGroup;
}

de::MovePtr<tcu::TestCaseGroup> createSemaphoreTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> semaphoreGroup (new tcu::TestCaseGroup(testCtx, "semaphore", "Tests for external semaphores."));

	semaphoreGroup->addChild(createSemaphoreTests(testCtx, vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT).release());
	semaphoreGroup->addChild(createSemaphoreTests(testCtx, vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT).release());
	semaphoreGroup->addChild(createSemaphoreTests(testCtx, vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT).release());
	semaphoreGroup->addChild(createSemaphoreTests(testCtx, vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT).release());
	semaphoreGroup->addChild(createSemaphoreTests(testCtx, vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_ZIRCON_EVENT_BIT_FUCHSIA).release());

	return semaphoreGroup;
}

de::MovePtr<tcu::TestCaseGroup> createMemoryTests (tcu::TestContext& testCtx, vk::VkExternalMemoryHandleTypeFlagBits externalType)
{
	de::MovePtr<tcu::TestCaseGroup> group (new tcu::TestCaseGroup(testCtx, externalMemoryTypeToName(externalType), "Tests for external memory"));

	for (size_t dedicatedNdx = 0; dedicatedNdx < 2; dedicatedNdx++)
	{
		const bool						dedicated		(dedicatedNdx == 1);
		de::MovePtr<tcu::TestCaseGroup>	dedicatedGroup	(new tcu::TestCaseGroup(testCtx, dedicated ? "dedicated" : "suballocated", ""));

		for (size_t hostVisibleNdx = 0; hostVisibleNdx < 2; hostVisibleNdx++)
		{
			const bool						hostVisible			(hostVisibleNdx == 1);
			de::MovePtr<tcu::TestCaseGroup>	hostVisibleGroup	(new tcu::TestCaseGroup(testCtx, hostVisible ? "host_visible" : "device_only", ""));
			const MemoryTestConfig			memoryConfig		(externalType, hostVisible, dedicated);

			if (externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT
				|| externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT)
			{
				addFunctionCase(hostVisibleGroup.get(), "create_win32",	"Test creating memory with win32 properties .",		testMemoryWin32Create,	memoryConfig);
			}

			addFunctionCase(hostVisibleGroup.get(), "import_twice",				"Test importing memory object twice.",			testMemoryImportTwice,		memoryConfig);
			addFunctionCase(hostVisibleGroup.get(), "import_multiple_times",	"Test importing memory object multiple times.",	testMemoryMultipleImports,	memoryConfig);

			if (externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT
				|| externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT)
			{
				addFunctionCase(hostVisibleGroup.get(), "dup",						"Test calling dup() on exported memory.",	testMemoryFdDup,			memoryConfig);
				addFunctionCase(hostVisibleGroup.get(), "dup2",						"Test calling dup2() on exported memory.",	testMemoryFdDup2,			memoryConfig);
				addFunctionCase(hostVisibleGroup.get(), "dup3",						"Test calling dup3() on exported memory.",	testMemoryFdDup3,			memoryConfig);
				addFunctionCase(hostVisibleGroup.get(), "send_over_socket",			"Test sending memory fd over socket.",		testMemoryFdSendOverSocket,	memoryConfig);
				// \note Not supported on WIN32 handles
				addFunctionCase(hostVisibleGroup.get(), "export_multiple_times",	"Test exporting memory multiple times.",	testMemoryMultipleExports,	memoryConfig);
			}

			if (externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT)
			{
				addFunctionCase(hostVisibleGroup.get(), "fd_properties",			"Test obtaining the FD memory properties",	testMemoryFdProperties,		memoryConfig);
			}

			dedicatedGroup->addChild(hostVisibleGroup.release());
		}

		{
			de::MovePtr<tcu::TestCaseGroup>	bufferGroup		(new tcu::TestCaseGroup(testCtx, "buffer", ""));
			const BufferTestConfig			bufferConfig	(externalType, dedicated);

			addFunctionCase(bufferGroup.get(), "info",						"External buffer memory info query.",						testBufferQueries,				externalType);
			addFunctionCase(bufferGroup.get(), "bind_export_import_bind",	"Test binding, exporting, importing and binding buffer.",	testBufferBindExportImportBind,	bufferConfig);
			addFunctionCase(bufferGroup.get(), "export_bind_import_bind",	"Test exporting, binding, importing and binding buffer.",	testBufferExportBindImportBind,	bufferConfig);
			addFunctionCase(bufferGroup.get(), "export_import_bind_bind",	"Test exporting, importing and binding buffer.",			testBufferExportImportBindBind,	bufferConfig);

			dedicatedGroup->addChild(bufferGroup.release());
		}

		{
			de::MovePtr<tcu::TestCaseGroup> imageGroup	(new tcu::TestCaseGroup(testCtx, "image", ""));
			const ImageTestConfig			imageConfig	(externalType, dedicated);

			addFunctionCase(imageGroup.get(), "info",						"External image memory info query.",						testImageQueries,				externalType);
			addFunctionCase(imageGroup.get(), "bind_export_import_bind",	"Test binding, exporting, importing and binding image.",	testImageBindExportImportBind,	imageConfig);
			addFunctionCase(imageGroup.get(), "export_bind_import_bind",	"Test exporting, binding, importing and binding image.",	testImageExportBindImportBind,	imageConfig);
			addFunctionCase(imageGroup.get(), "export_import_bind_bind",	"Test exporting, importing and binding image.",				testImageExportImportBindBind,	imageConfig);

			dedicatedGroup->addChild(imageGroup.release());
		}

		group->addChild(dedicatedGroup.release());
	}

	if (externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID)
	{
		de::MovePtr<tcu::TestCaseGroup>	formatGroup	(new tcu::TestCaseGroup(testCtx, "image_formats", "Test minimum image format support"));

		const vk::VkFormat	ahbFormats[]	=
		{
			vk::VK_FORMAT_R8G8B8_UNORM,
			vk::VK_FORMAT_R8G8B8A8_UNORM,
			vk::VK_FORMAT_R5G6B5_UNORM_PACK16,
			vk::VK_FORMAT_R16G16B16A16_SFLOAT,
			vk::VK_FORMAT_A2B10G10R10_UNORM_PACK32,
			vk::VK_FORMAT_D16_UNORM,
			vk::VK_FORMAT_X8_D24_UNORM_PACK32,
			vk::VK_FORMAT_D24_UNORM_S8_UINT,
			vk::VK_FORMAT_D32_SFLOAT,
			vk::VK_FORMAT_D32_SFLOAT_S8_UINT,
			vk::VK_FORMAT_S8_UINT,
			vk::VK_FORMAT_R8_UNORM,
			vk::VK_FORMAT_R16_UINT,
			vk::VK_FORMAT_R16G16_UINT,
			vk::VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16,
		};
		const size_t		numOfAhbFormats	= DE_LENGTH_OF_ARRAY(ahbFormats);

		for (size_t ahbFormatNdx = 0; ahbFormatNdx < numOfAhbFormats; ahbFormatNdx++)
		{
			const vk::VkFormat	format			= ahbFormats[ahbFormatNdx];
			const std::string	testCaseName	= getFormatCaseName(format);

			addFunctionCase(formatGroup.get(), testCaseName, "", testAndroidHardwareBufferImageFormat, format);
		}

		group->addChild(formatGroup.release());
	}

	return group;
}

de::MovePtr<tcu::TestCaseGroup> createMemoryTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group (new tcu::TestCaseGroup(testCtx, "memory", "Tests for external memory"));

	group->addChild(createMemoryTests(testCtx, vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT).release());
	group->addChild(createMemoryTests(testCtx, vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT).release());
	group->addChild(createMemoryTests(testCtx, vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT).release());
	group->addChild(createMemoryTests(testCtx, vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID).release());
	group->addChild(createMemoryTests(testCtx, vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT).release());
	group->addChild(createMemoryTests(testCtx, vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_ZIRCON_VMO_BIT_FUCHSIA).release());

	return group;
}

} // anonymous

tcu::TestCaseGroup* createExternalMemoryTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group (new tcu::TestCaseGroup(testCtx, "external", "Tests for external Vulkan objects"));

	group->addChild(createSemaphoreTests(testCtx).release());
	group->addChild(createMemoryTests(testCtx).release());
	group->addChild(createFenceTests(testCtx).release());

	return group.release();
}

} // api
} // vkt
