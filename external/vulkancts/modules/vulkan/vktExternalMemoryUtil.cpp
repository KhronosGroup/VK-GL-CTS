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
 * \brief Vulkan external memory utilities
 *//*--------------------------------------------------------------------*/

#include "vktExternalMemoryUtil.hpp"

#include "vkQueryUtil.hpp"

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

namespace vkt
{
namespace ExternalMemoryUtil
{
namespace
{
deUint32 chooseMemoryType (deUint32 bits)
{
	DE_ASSERT(bits != 0);

	for (deUint32 memoryTypeIndex = 0; (1u << memoryTypeIndex) <= bits; memoryTypeIndex++)
	{
		if ((bits & (1u << memoryTypeIndex)) != 0)
			return memoryTypeIndex;
	}

	DE_FATAL("No supported memory types");
	return -1;
}

} // anonymous

NativeHandle::NativeHandle (void)
	: m_fd				(-1)
	, m_win32HandleType	(WIN32HANDLETYPE_LAST)
	, m_win32Handle		(DE_NULL)
{
}

NativeHandle::NativeHandle (const NativeHandle& other)
	: m_fd				(-1)
	, m_win32HandleType	(WIN32HANDLETYPE_LAST)
	, m_win32Handle		(DE_NULL)
{
	if (other.m_fd >= 0)
	{
#if (DE_OS == DE_OS_ANDROID) || (DE_OS == DE_OS_UNIX)
		DE_ASSERT(!other.m_win32Handle.internal);
		m_fd = dup(other.m_fd);
		TCU_CHECK(m_fd >= 0);
#else
		DE_FATAL("Platform doesn't support file descriptors");
#endif
	}
	else if (other.m_win32Handle.internal)
	{
#if (DE_OS == DE_OS_WIN32)
		m_win32HandleType = other.m_win32HandleType;

		switch (other.m_win32HandleType)
		{
			case WIN32HANDLETYPE_NT:
			{
				DE_ASSERT(other.m_fd == -1);

				const HANDLE process = ::GetCurrentProcess();
				::DuplicateHandle(process, other.m_win32Handle.internal, process, &m_win32Handle.internal, 0, TRUE, DUPLICATE_SAME_ACCESS);

				break;
			}

			case WIN32HANDLETYPE_KMT:
			{
				m_win32Handle = other.m_win32Handle;
				break;
			}

			default:
				DE_FATAL("Unknown win32 handle type");
		}
#else
		DE_FATAL("Platform doesn't support win32 handles");
#endif
	}
	else
		DE_FATAL("Native handle can't be duplicated");
}

NativeHandle::NativeHandle (int fd)
	: m_fd				(fd)
	, m_win32HandleType	(WIN32HANDLETYPE_LAST)
	, m_win32Handle		(DE_NULL)
{
}

NativeHandle::NativeHandle (Win32HandleType handleType, vk::pt::Win32Handle handle)
	: m_fd				(-1)
	, m_win32HandleType	(handleType)
	, m_win32Handle		(handle)
{
}

NativeHandle::~NativeHandle (void)
{
	reset();
}

void NativeHandle::reset (void)
{
	if (m_fd >= 0)
	{
#if (DE_OS == DE_OS_ANDROID) || (DE_OS == DE_OS_UNIX)
		DE_ASSERT(!m_win32Handle.internal);
		::close(m_fd);
#else
		DE_FATAL("Platform doesn't support file descriptors");
#endif
	}

	if (m_win32Handle.internal)
	{
#if (DE_OS == DE_OS_WIN32)
		switch (m_win32HandleType)
		{
			case WIN32HANDLETYPE_NT:
				DE_ASSERT(m_fd == -1);
				::CloseHandle((HANDLE)m_win32Handle.internal);
				break;

			case WIN32HANDLETYPE_KMT:
				break;

			default:
				DE_FATAL("Unknown win32 handle type");
		}
#else
		DE_FATAL("Platform doesn't support win32 handles");
#endif
	}

	m_fd				= -1;
	m_win32Handle		= vk::pt::Win32Handle(DE_NULL);
	m_win32HandleType	= WIN32HANDLETYPE_LAST;
}

NativeHandle& NativeHandle::operator= (int fd)
{
	reset();

	m_fd = fd;

	return *this;
}

void NativeHandle::setWin32Handle (Win32HandleType type, vk::pt::Win32Handle handle)
{
	reset();

	m_win32HandleType	= type;
	m_win32Handle		= handle;
}

void NativeHandle::disown (void)
{
	m_fd = -1;
	m_win32Handle = vk::pt::Win32Handle(DE_NULL);
}

vk::pt::Win32Handle NativeHandle::getWin32Handle (void) const
{
	DE_ASSERT(m_fd == -1);
	return m_win32Handle;
}

int NativeHandle::getFd (void) const
{
	DE_ASSERT(!m_win32Handle.internal);

	return m_fd;
}

const char* externalSemaphoreTypeToName (vk::VkExternalSemaphoreHandleTypeFlagBitsKHX type)
{
	switch (type)
	{
		case vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT_KHX:
			return "opaque_fd";

		case vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHX:
			return "opaque_win32";

		case vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT_KHX:
			return "opaque_win32_kmt";

		case vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT_KHX:
			return "d3d12_fenc";

		case vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_FENCE_FD_BIT_KHX:
			return "fence_fd";

		default:
			DE_FATAL("Unknown external semaphore type");
			return DE_NULL;
	}
}

const char* externalMemoryTypeToName (vk::VkExternalMemoryHandleTypeFlagBitsKHX type)
{
	switch (type)
	{
		case vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHX:
			return "opaque_fd";

		case vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHX:
			return "opaque_win32";

		case vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT_KHX:
			return "opaque_win32_kmt";

		case vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT_KHX:
			return "d3d11_texture";

		case vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT_KHX:
			return "d3d11_texture_kmt";

		case vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_HEAP_BIT_KHX:
			return "d3d12_heap";

		case vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT_KHX:
			return "d3d12_resource";

		default:
			DE_FATAL("Unknown external memory type");
			return DE_NULL;
	}
}

Permanence getHandleTypePermanence (vk::VkExternalSemaphoreHandleTypeFlagBitsKHX type)
{
	switch (type)
	{
		case vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT_KHX:
		case vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHX:
		case vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT_KHX:
			return PERMANENCE_PERMANENT;

		case vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_FENCE_FD_BIT_KHX:
			return PERMANENCE_TEMPORARY;

		default:
			DE_FATAL("Unknown external memory type");
			return PERMANENCE_TEMPORARY;
	}
}

int getMemoryFd (const vk::DeviceInterface&					vkd,
				 vk::VkDevice								device,
				 vk::VkDeviceMemory							memory,
				 vk::VkExternalMemoryHandleTypeFlagBitsKHX	externalType)
{
	int fd = -1;

	VK_CHECK(vkd.getMemoryFdKHX(device, memory, externalType, &fd));
	TCU_CHECK(fd >= 0);

	return fd;
}

void getMemoryNative (const vk::DeviceInterface&					vkd,
						 vk::VkDevice								device,
						 vk::VkDeviceMemory							memory,
						 vk::VkExternalMemoryHandleTypeFlagBitsKHX	externalType,
						 NativeHandle&								nativeHandle)
{
	if (externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHX)
	{
		int fd = -1;
		VK_CHECK(vkd.getMemoryFdKHX(device, memory, externalType, &fd));
		TCU_CHECK(fd >= 0);
		nativeHandle = fd;
	}
	else if (externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHX
		|| externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT_KHX)
	{
		vk::pt::Win32Handle	handle	(DE_NULL);

		VK_CHECK(vkd.getMemoryWin32HandleKHX(device, memory, externalType, &handle));

		switch (externalType)
		{
			case vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHX:
				nativeHandle.setWin32Handle(NativeHandle::WIN32HANDLETYPE_NT, handle);
				break;

			case vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT_KHX:
				nativeHandle.setWin32Handle(NativeHandle::WIN32HANDLETYPE_KMT, handle);
				break;

			default:
				DE_FATAL("Unknow external memory handle type");
		}
	}
	else
		DE_FATAL("Unknow external memory handle type");
}

vk::Move<vk::VkSemaphore> createExportableSemaphore (const vk::DeviceInterface&						vkd,
													 vk::VkDevice									device,
													 vk::VkExternalSemaphoreHandleTypeFlagBitsKHX	externalType)
{
	const vk::VkExportSemaphoreCreateInfoKHX			exportCreateInfo	=
	{
		vk::VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO_KHX,
		DE_NULL,
		(vk::VkExternalMemoryHandleTypeFlagsKHX)externalType
	};
	const vk::VkSemaphoreCreateInfo						createInfo			=
	{
		vk::VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		&exportCreateInfo,
		0u
	};

	return vk::createSemaphore(vkd, device, &createInfo);
}

int getSemaphoreFd (const vk::DeviceInterface&						vkd,
					vk::VkDevice									device,
					vk::VkSemaphore									semaphore,
					vk::VkExternalSemaphoreHandleTypeFlagBitsKHX	externalType)
{
	int fd = -1;

	VK_CHECK(vkd.getSemaphoreFdKHX(device, semaphore, externalType, &fd));
	TCU_CHECK(fd >= 0);

	return fd;
}

void getSemaphoreNative (const vk::DeviceInterface&						vkd,
						 vk::VkDevice									device,
						 vk::VkSemaphore								semaphore,
						 vk::VkExternalSemaphoreHandleTypeFlagBitsKHX	externalType,
						 NativeHandle&									nativeHandle)
{
	if (externalType == vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_FENCE_FD_BIT_KHX
		|| externalType == vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT_KHX)
	{
		int fd = -1;
		VK_CHECK(vkd.getSemaphoreFdKHX(device, semaphore, externalType, &fd));
		TCU_CHECK(fd >= 0);
		nativeHandle = fd;
	}
	else if (externalType == vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHX
		|| externalType == vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT_KHX)
	{
		vk::pt::Win32Handle	handle	(DE_NULL);

		VK_CHECK(vkd.getSemaphoreWin32HandleKHX(device, semaphore, externalType, &handle));

		switch (externalType)
		{
			case vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHX:
				nativeHandle.setWin32Handle(NativeHandle::WIN32HANDLETYPE_NT, handle);
				break;

			case vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT_KHX:
				nativeHandle.setWin32Handle(NativeHandle::WIN32HANDLETYPE_KMT, handle);
				break;

			default:
				DE_FATAL("Unknow external memory handle type");
		}
	}
	else
		DE_FATAL("Unknow external semaphore handle type");
}

void importSemaphore (const vk::DeviceInterface&					vkd,
					  const vk::VkDevice							device,
					  const vk::VkSemaphore							semaphore,
					  vk::VkExternalSemaphoreHandleTypeFlagBitsKHX	externalType,
					  NativeHandle&									handle)
{
	if (externalType == vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_FENCE_FD_BIT_KHX
		|| externalType == vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT_KHX)
	{
		const vk::VkImportSemaphoreFdInfoKHX	importInfo	=
		{
			vk::VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHX,
			DE_NULL,
			semaphore,
			externalType,
			handle.getFd()
		};

		VK_CHECK(vkd.importSemaphoreFdKHX(device, &importInfo));
		handle.disown();
	}
	else if (externalType == vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHX
			|| externalType == vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT_KHX)
	{
		const vk::VkImportSemaphoreWin32HandleInfoKHX	importInfo	=
		{
			vk::VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHX,
			DE_NULL,
			semaphore,
			(vk::VkExternalMemoryHandleTypeFlagsKHX)externalType,
			handle.getWin32Handle()
		};

		VK_CHECK(vkd.importSemaphoreWin32HandleKHX(device, &importInfo));
		// \note File descriptors and win32 handles behave differently, but this call wil make it seem like they would behave in same way
		handle.reset();
	}
	else
		DE_FATAL("Unknown semaphore external handle type");
}

vk::Move<vk::VkSemaphore> createAndImportSemaphore (const vk::DeviceInterface&						vkd,
													const vk::VkDevice								device,
													vk::VkExternalSemaphoreHandleTypeFlagBitsKHX	externalType,
													NativeHandle&									handle)
{
	vk::Move<vk::VkSemaphore>	semaphore	(createSemaphore(vkd, device));

	importSemaphore(vkd, device, *semaphore, externalType, handle);

	return semaphore;
}

vk::Move<vk::VkDeviceMemory> allocateExportableMemory (const vk::DeviceInterface&					vkd,
													   vk::VkDevice									device,
													   const vk::VkMemoryRequirements&				requirements,
													   vk::VkExternalMemoryHandleTypeFlagBitsKHX	externalType)
{
	const vk::VkExportMemoryAllocateInfoKHX	exportInfo	=
	{
		vk::VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_KHX,
		DE_NULL,
		(vk::VkExternalMemoryHandleTypeFlagsKHX)externalType
	};
	const vk::VkMemoryAllocateInfo			info		=
	{
		vk::VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		&exportInfo,
		requirements.size,
		chooseMemoryType(requirements.memoryTypeBits)
	};
	return vk::allocateMemory(vkd, device, &info);
}

vk::Move<vk::VkDeviceMemory> allocateExportableMemory (const vk::InstanceInterface&					vki,
													   vk::VkPhysicalDevice							physicalDevice,
													   const vk::DeviceInterface&					vkd,
													   vk::VkDevice									device,
													   const vk::VkMemoryRequirements&				requirements,
													   vk::VkExternalMemoryHandleTypeFlagBitsKHX	externalType,
													   bool											hostVisible)
{
	const vk::VkPhysicalDeviceMemoryProperties properties = vk::getPhysicalDeviceMemoryProperties(vki, physicalDevice);

	for (deUint32 memoryTypeIndex = 0; (1u << memoryTypeIndex) <= requirements.memoryTypeBits; memoryTypeIndex++)
	{
		if (((requirements.memoryTypeBits & (1u << memoryTypeIndex)) != 0)
			&& (((properties.memoryTypes[memoryTypeIndex].propertyFlags & vk::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0) == hostVisible))
		{
			const vk::VkExportMemoryAllocateInfoKHX	exportInfo	=
			{
				vk::VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_KHX,
				DE_NULL,
				(vk::VkExternalMemoryHandleTypeFlagsKHX)externalType
			};
			const vk::VkMemoryAllocateInfo			info		=
			{
				vk::VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				&exportInfo,
				requirements.size,
				memoryTypeIndex
			};
			return vk::allocateMemory(vkd, device, &info);
		}
	}

	TCU_THROW(NotSupportedError, "No supported memory type found");
}

vk::Move<vk::VkDeviceMemory> importMemory (const vk::DeviceInterface&					vkd,
										   vk::VkDevice									device,
										   const vk::VkMemoryRequirements&				requirements,
										   vk::VkExternalMemoryHandleTypeFlagBitsKHX	externalType,
										   NativeHandle&								handle)
{
	if (externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHX)
	{
		const vk::VkImportMemoryFdInfoKHX	importInfo =
		{
			vk::VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHX,
			DE_NULL,
			externalType,
			handle.getFd()
		};
		const vk::VkMemoryAllocateInfo			info		=
		{
			vk::VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			&importInfo,
			requirements.size,
			chooseMemoryType(requirements.memoryTypeBits)
		};
		vk::Move<vk::VkDeviceMemory> memory (vk::allocateMemory(vkd, device, &info));

		handle.disown();

		return memory;
	}
	else if (externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHX
			|| externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT_KHX)
	{
		const vk::VkImportMemoryWin32HandleInfoKHX	importInfo =
		{
			vk::VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHX,
			DE_NULL,
			externalType,
			handle.getWin32Handle()
		};
		const vk::VkMemoryAllocateInfo			info		=
		{
			vk::VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			&importInfo,
			requirements.size,
			chooseMemoryType(requirements.memoryTypeBits)
		};
		vk::Move<vk::VkDeviceMemory> memory (vk::allocateMemory(vkd, device, &info));

		handle.disown();

		return memory;
	}
	else
	{
		DE_FATAL("Unknown external memory type");
		return vk::Move<vk::VkDeviceMemory>();
	}
}

vk::Move<vk::VkBuffer> createExternalBuffer (const vk::DeviceInterface&					vkd,
											 vk::VkDevice								device,
											 deUint32									queueFamilyIndex,
											 vk::VkExternalMemoryHandleTypeFlagBitsKHX	externalType,
											 vk::VkDeviceSize							size,
											 vk::VkBufferUsageFlags						usage)
{
	const vk::VkExternalMemoryBufferCreateInfoKHX		externalCreateInfo	=
	{
		vk::VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO_KHX,
		DE_NULL,
		(vk::VkExternalMemoryHandleTypeFlagsKHX)externalType
	};
	const vk::VkBufferCreateInfo						createInfo			=
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		&externalCreateInfo,
		0u,

		size,
		usage,
		vk::VK_SHARING_MODE_EXCLUSIVE,
		1u,
		&queueFamilyIndex
	};

	return vk::createBuffer(vkd, device, &createInfo);
}

vk::Move<vk::VkImage> createExternalImage (const vk::DeviceInterface&					vkd,
										   vk::VkDevice									device,
										   deUint32										queueFamilyIndex,
										   vk::VkExternalMemoryHandleTypeFlagBitsKHX	externalType,
										   vk::VkFormat									format,
										   deUint32										width,
										   deUint32										height,
										   vk::VkImageTiling							tiling,
										   vk::VkImageUsageFlags						usage)
{
	const vk::VkExternalMemoryImageCreateInfoKHX		externalCreateInfo	=
	{
		vk::VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHX,
		DE_NULL,
		(vk::VkExternalMemoryHandleTypeFlagsKHX)externalType
	};
	const vk::VkImageCreateInfo						createInfo			=
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		&externalCreateInfo,
		0u,

		vk::VK_IMAGE_TYPE_2D,
		format,
		{ width, height, 1u, },
		1u,
		1u,
		vk::VK_SAMPLE_COUNT_1_BIT,
		tiling,
		usage,
		vk::VK_SHARING_MODE_EXCLUSIVE,
		1,
		&queueFamilyIndex,
		vk::VK_IMAGE_LAYOUT_UNDEFINED
	};

	return vk::createImage(vkd, device, &createInfo);
}

} // ExternalMemoryUtil
} // vkt
