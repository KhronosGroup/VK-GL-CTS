/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 The Khronos Group Inc.
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
 * \file  vktGlobalPriorityQueueUtils.cpp
 * \brief Global Priority Queue Utils
 *//*--------------------------------------------------------------------*/

#include "vktGlobalPriorityQueueUtils.hpp"
#include "vkImageUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vktTestCase.hpp"
#include "deStringUtil.hpp"
#include "tcuCommandLine.hpp"
#include <vector>

using namespace vk;

namespace vkt
{
namespace synchronization
{

deUint32 findQueueFamilyIndex (const InstanceInterface&	vki,
							   VkPhysicalDevice			dev,
							   VkQueueGlobalPriorityKHR	priority,
							   VkQueueFlags				includeFlags,
							   VkQueueFlags				excludeFlags,
							   deUint32					excludeIndex)
{
	deUint32 queueFamilyPropertyCount = 0;
	vki.getPhysicalDeviceQueueFamilyProperties2(dev, &queueFamilyPropertyCount, nullptr);
	DE_ASSERT(queueFamilyPropertyCount);

	std::vector<VkQueueFamilyProperties2>					familyProperties2(queueFamilyPropertyCount);
	std::vector<VkQueueFamilyGlobalPriorityPropertiesKHR>	familyPriorityProperties(queueFamilyPropertyCount);

	for (deUint32 familyIdx = 0; familyIdx < queueFamilyPropertyCount; ++familyIdx)
	{
		VkQueueFamilyGlobalPriorityPropertiesKHR* pPriorityProps = &familyPriorityProperties[familyIdx];
		pPriorityProps->sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_GLOBAL_PRIORITY_PROPERTIES_KHR;

		VkQueueFamilyProperties2* pFamilyProps = &familyProperties2[familyIdx];
		pFamilyProps->sType	= VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
		pFamilyProps->pNext	= pPriorityProps;
	}

	vki.getPhysicalDeviceQueueFamilyProperties2(dev, &queueFamilyPropertyCount, familyProperties2.data());

	deUint32 familyFound = INVALID_UINT32;

	for (deUint32 familyIdx = 0; familyIdx < queueFamilyPropertyCount; ++familyIdx)
	{
		if (familyIdx == excludeIndex) continue;

		bool priorityMatches = false;
		for (uint32_t priorityIdx = 0; !priorityMatches && priorityIdx < familyPriorityProperties[familyIdx].priorityCount; ++priorityIdx)
		{
			priorityMatches = (priority == familyPriorityProperties[familyIdx].priorities[priorityIdx]);
		}
		if (!priorityMatches) continue;

		const VkQueueFlags queueFlags = familyProperties2[familyIdx].queueFamilyProperties.queueFlags;
		if ( ((queueFlags & includeFlags) == includeFlags) && ((queueFlags & excludeFlags) == 0) )
		{
			familyFound = familyIdx;
			break;
		}
	}

	return familyFound;
}

#define SAVEEXPR(expr, text, file, line) (text=#expr,file=__FILE__,line=__LINE__,expr)

SpecialDevice::SpecialDevice	(Context&						ctx,
								 VkQueueFlagBits				transitionFrom,
								 VkQueueFlagBits				transitionTo,
								 VkQueueGlobalPriorityKHR		priorityFrom,
								 VkQueueGlobalPriorityKHR		priorityTo,
								 bool							enableProtected,
								 bool							enableSparseBinding)
		: queueFamilyIndexFrom	(m_queueFamilyIndexFrom)
		, queueFamilyIndexTo	(m_queueFamilyIndexTo)
		, handle				(m_deviceHandle)
		, queueFrom				(m_queueFrom)
		, queueTo				(m_queueTo)
		, createResult			(m_createResult)
		, createExpression		(m_createExpression)
		, createFileName		(m_createFileName)
		, createFileLine		(m_createFileLine)
		, m_context					(ctx)
		, m_transitionFrom			(transitionFrom)
		, m_transitionTo			(transitionTo)
		, m_queueFamilyIndexFrom	(INVALID_UINT32)
		, m_queueFamilyIndexTo		(INVALID_UINT32)
		, m_deviceHandle			(VK_NULL_HANDLE)
		, m_queueFrom				(VK_NULL_HANDLE)
		, m_queueTo					(VK_NULL_HANDLE)
		, m_allocator				()
		, m_createResult			(VK_ERROR_UNKNOWN)
		, m_createExpression		(nullptr)
		, m_createFileName			(nullptr)
		, m_createFileLine			(INVALID_UINT32)
{
	const DeviceInterface&					vkd					= ctx.getDeviceInterface();
	const InstanceInterface&				vki					= ctx.getInstanceInterface();
	const VkInstance						instance			= ctx.getInstance();
	const tcu::CommandLine&					cmdLine				= ctx.getTestContext().getCommandLine();
	const VkPhysicalDevice					phys				= chooseDevice(vki, instance, cmdLine);
	const VkPhysicalDeviceMemoryProperties	memoryProperties	= getPhysicalDeviceMemoryProperties(vki, phys);


	VkQueueFlags	flagFrom	= transitionFrom;
	VkQueueFlags	flagTo		= transitionTo;
	if (enableProtected)
	{
		flagFrom |= VK_QUEUE_PROTECTED_BIT;
		flagTo	|= VK_QUEUE_PROTECTED_BIT;
	}
	if (enableSparseBinding)
	{
		flagFrom |= VK_QUEUE_SPARSE_BINDING_BIT;
		flagTo	|= VK_QUEUE_SPARSE_BINDING_BIT;
	}

	m_queueFamilyIndexFrom	= findQueueFamilyIndex(vki, phys, priorityFrom, flagFrom, getColissionFlags(transitionFrom), INVALID_UINT32);
	m_queueFamilyIndexTo	= findQueueFamilyIndex(vki, phys, priorityTo, flagTo, getColissionFlags(transitionTo), m_queueFamilyIndexFrom);

	DE_ASSERT(m_queueFamilyIndexFrom != INVALID_UINT32);
	DE_ASSERT(m_queueFamilyIndexTo != INVALID_UINT32);

	const float									queuePriorities[2] { 1.0f, 0.0f };
	VkDeviceQueueCreateInfo						queueCreateInfos[2];
	VkDeviceQueueGlobalPriorityCreateInfoKHR	priorityCreateInfos[2];
	{
		priorityCreateInfos[0].sType	= priorityCreateInfos[1].sType		= VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_KHR;
		priorityCreateInfos[0].pNext	= priorityCreateInfos[1].pNext		= nullptr;

		queueCreateInfos[0].sType		= queueCreateInfos[1].sType			= VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfos[0].flags		= queueCreateInfos[1].flags			= enableProtected ? VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT : 0;
		queueCreateInfos[0].queueCount	= queueCreateInfos[1].queueCount	= 1;

		priorityCreateInfos[0].globalPriority	= priorityFrom;
		queueCreateInfos[0].pNext		= &priorityCreateInfos[0];
		queueCreateInfos[0].queueFamilyIndex = m_queueFamilyIndexFrom;
		queueCreateInfos[0].pQueuePriorities = &queuePriorities[0];

		priorityCreateInfos[1].globalPriority	= priorityTo;
		queueCreateInfos[1].pNext		= &priorityCreateInfos[1];
		queueCreateInfos[1].queueFamilyIndex = m_queueFamilyIndexTo;
		queueCreateInfos[1].pQueuePriorities = &queuePriorities[1];
	}

	VkPhysicalDeviceProtectedMemoryFeatures memFeatures
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES,
		DE_NULL,
		VK_TRUE
	};
	VkPhysicalDeviceFeatures2 devFeatures = ctx.getDeviceFeatures2();
	if (enableProtected) devFeatures.pNext = &memFeatures;

	const std::vector<const char*>& extensions = ctx.getDeviceCreationExtensions();

	VkDeviceCreateInfo deviceCreateInfo			{};
	deviceCreateInfo.sType						= VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pNext						= &devFeatures;
	deviceCreateInfo.flags						= VkDeviceCreateFlags(0);
	deviceCreateInfo.queueCreateInfoCount		= 2;
	deviceCreateInfo.pQueueCreateInfos			= queueCreateInfos;
	deviceCreateInfo.pEnabledFeatures			= nullptr;
	deviceCreateInfo.enabledExtensionCount		= static_cast<deUint32>(extensions.size());
	deviceCreateInfo.ppEnabledExtensionNames	= de::dataOrNull(extensions);
	deviceCreateInfo.ppEnabledLayerNames		= nullptr;
	deviceCreateInfo.enabledLayerCount			= 0;

	m_createResult = SAVEEXPR(createUncheckedDevice(cmdLine.isValidationEnabled(), vki, phys, &deviceCreateInfo, nullptr, &m_deviceHandle),
							  m_createExpression, m_createFileName, m_createFileLine);
	if (VK_SUCCESS == m_createResult && VK_NULL_HANDLE != m_deviceHandle)
	{
		m_allocator		= de::MovePtr<vk::Allocator>(new SimpleAllocator(vkd, m_deviceHandle, memoryProperties));

		if (enableProtected)
		{
			VkDeviceQueueInfo2	queueInfo{};
			queueInfo.sType			= VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2;
			queueInfo.flags			= VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT;
			queueInfo.queueIndex	= 0;

			queueInfo.queueFamilyIndex = m_queueFamilyIndexFrom;
			vkd.getDeviceQueue2(m_deviceHandle, &queueInfo, &m_queueFrom);

			queueInfo.queueFamilyIndex = m_queueFamilyIndexTo;
			vkd.getDeviceQueue2(m_deviceHandle, &queueInfo, &m_queueTo);
		}
		else
		{
			vkd.getDeviceQueue(m_deviceHandle, m_queueFamilyIndexFrom, 0, &m_queueFrom);
			vkd.getDeviceQueue(m_deviceHandle, m_queueFamilyIndexTo, 0, &m_queueTo);
		}
	}
}
SpecialDevice::~SpecialDevice ()
{
	if (VK_NULL_HANDLE != m_deviceHandle)
	{
		m_context.getDeviceInterface().destroyDevice(m_deviceHandle, nullptr);
		m_createResult = VK_ERROR_UNKNOWN;
		m_deviceHandle = VK_NULL_HANDLE;
	}
}
VkQueueFlags SpecialDevice::getColissionFlags (VkQueueFlags flags)
{
	if (flags & VK_QUEUE_TRANSFER_BIT)
		return (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);

	if (flags & VK_QUEUE_COMPUTE_BIT)
		return VK_QUEUE_GRAPHICS_BIT;

	if (flags & VK_QUEUE_GRAPHICS_BIT)
		return 0;

	DE_ASSERT(false);
	return 0;
}

BufferWithMemory::BufferWithMemory	(const vk::InstanceInterface&	vki,
									 const DeviceInterface&			vkd,
									 const vk::VkPhysicalDevice		phys,
									 const VkDevice					device,
									 Allocator&						allocator,
									 const VkBufferCreateInfo&		bufferCreateInfo,
									 const MemoryRequirement		memoryRequirement,
									 const VkQueue					sparseQueue)
	: m_amISparse		((bufferCreateInfo.flags & VK_BUFFER_CREATE_SPARSE_BINDING_BIT) != 0)
	, m_buffer			(createBuffer(vkd, device, &bufferCreateInfo))
	, m_requirements	(getBufferMemoryRequirements(vkd, device, *m_buffer))
	, m_allocations		()
	, m_size			(bufferCreateInfo.size)
{
	if (m_amISparse)
	{
		DE_ASSERT(sparseQueue != VkQueue(0));
		const VkPhysicalDeviceMemoryProperties	memoryProperties	= getPhysicalDeviceMemoryProperties(vki, phys);
		const deUint32							memoryTypeIndex		= selectMatchingMemoryType(memoryProperties, m_requirements.memoryTypeBits, memoryRequirement);
		const VkDeviceSize						lastChunkSize		= m_requirements.size % m_requirements.alignment;
		const uint32_t							chunkCount			= static_cast<uint32_t>(m_requirements.size / m_requirements.alignment + (lastChunkSize ? 1 : 0));
		Move<VkFence>							fence				= createFence(vkd, device);

		std::vector<VkSparseMemoryBind>			bindings(chunkCount);

		for (uint32_t i = 0; i < chunkCount; ++i)
		{
			VkMemoryAllocateInfo		allocInfo{};
			allocInfo.sType				= VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.pNext				= nullptr;
			allocInfo.allocationSize	= m_requirements.alignment;
			allocInfo.memoryTypeIndex	= memoryTypeIndex;

			de::MovePtr<Allocation>		allocation	= allocator.allocate(allocInfo, m_requirements.alignment);

			VkSparseMemoryBind&			binding = bindings[i];
			binding.resourceOffset		= m_requirements.alignment * i;
			binding.size				= m_requirements.alignment;
			binding.memory				= allocation->getMemory();
			binding.memoryOffset		= allocation->getOffset();
			binding.flags				= 0;

			m_allocations.emplace_back(allocation.release());
		}

		VkSparseBufferMemoryBindInfo bindInfo{};
		bindInfo.buffer		= *m_buffer;
		bindInfo.bindCount	= chunkCount;
		bindInfo.pBinds		= de::dataOrNull(bindings);

		VkBindSparseInfo sparseInfo{};
		sparseInfo.sType				= VK_STRUCTURE_TYPE_BIND_SPARSE_INFO;
		sparseInfo.pNext				= nullptr;
		sparseInfo.waitSemaphoreCount	= 0;
		sparseInfo.pWaitSemaphores		= nullptr;
		sparseInfo.bufferBindCount		= 1;
		sparseInfo.pBufferBinds			= &bindInfo;
		sparseInfo.imageOpaqueBindCount	= 0;
		sparseInfo.pImageOpaqueBinds	= nullptr;
		sparseInfo.imageBindCount		= 0;
		sparseInfo.pImageBinds			= nullptr;
		sparseInfo.signalSemaphoreCount	= 0;
		sparseInfo.pSignalSemaphores	= nullptr;

		VK_CHECK(vkd.queueBindSparse(sparseQueue, 1, &sparseInfo, *fence));
		VK_CHECK(vkd.waitForFences(device, 1u, &fence.get(), DE_TRUE, ~0ull));
	}
	else
	{
		de::MovePtr<Allocation> allocation = allocator.allocate(m_requirements, memoryRequirement);
		VK_CHECK(vkd.bindBufferMemory(device, *m_buffer, allocation->getMemory(), allocation->getOffset()));
		m_allocations.emplace_back(allocation.release());
	}
}

void BufferWithMemory::assertIAmSparse () const
{
	if (m_amISparse) TCU_THROW(NotSupportedError, "Host access pointer not implemented for sparse buffers");
}

void* BufferWithMemory::getHostPtr (void) const
{
	assertIAmSparse();
	return m_allocations[0]->getHostPtr();
}

void BufferWithMemory::invalidateAlloc (const DeviceInterface& vk, const VkDevice device) const
{
	assertIAmSparse();
	::vk::invalidateAlloc(vk, device, *m_allocations[0]);
}

void BufferWithMemory::flushAlloc (const DeviceInterface& vk, const VkDevice device) const
{
	assertIAmSparse();
	::vk::flushAlloc(vk, device, *m_allocations[0]);
}

ImageWithMemory::ImageWithMemory	(const InstanceInterface&		vki,
									 const DeviceInterface&			vkd,
									 const VkPhysicalDevice			phys,
									 const VkDevice					device,
									 Allocator&						allocator,
									 const VkImageCreateInfo&		imageCreateInfo,
									 const VkQueue					sparseQueue,
									 const MemoryRequirement		memoryRequirement)
	: m_image(((imageCreateInfo.flags & VK_IMAGE_CREATE_SPARSE_BINDING_BIT) != 0)
			  ? (new image::SparseImage(vkd, device, phys, vki, imageCreateInfo, sparseQueue, allocator, mapVkFormat(imageCreateInfo.format)))
			  : (new image::Image(vkd, device, allocator, imageCreateInfo, memoryRequirement)))
{
}

} // synchronization
} // vkt
