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

#if (DE_OS == DE_OS_ANDROID) && defined(__ANDROID_API_O__) && (DE_ANDROID_API >= __ANDROID_API_O__)
#	include <android/hardware_buffer.h>
#	define USE_ANDROID_O_HARDWARE_BUFFER 1
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
	: m_fd						(-1)
	, m_win32HandleType			(WIN32HANDLETYPE_LAST)
	, m_win32Handle				(DE_NULL)
	, m_androidHardwareBuffer	(DE_NULL)
{
}

NativeHandle::NativeHandle (const NativeHandle& other)
	: m_fd						(-1)
	, m_win32HandleType			(WIN32HANDLETYPE_LAST)
	, m_win32Handle				(DE_NULL)
	, m_androidHardwareBuffer	(DE_NULL)
{
	if (other.m_fd >= 0)
	{
#if (DE_OS == DE_OS_ANDROID) || (DE_OS == DE_OS_UNIX)
		DE_ASSERT(!other.m_win32Handle.internal);
		DE_ASSERT(!other.m_androidHardwareBuffer.internal);
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
				DE_ASSERT(!other.m_androidHardwareBuffer.internal);

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
#if defined(USE_ANDROID_O_HARDWARE_BUFFER)
	else if (other.m_androidHardwareBuffer.internal)
	{
		DE_ASSERT(other.m_fd == -1);
		DE_ASSERT(!other.m_win32Handle.internal);
		m_androidHardwareBuffer = other.m_androidHardwareBuffer;
		AHardwareBuffer_acquire((AHardwareBuffer*)m_androidHardwareBuffer.internal);
	}
#endif
	else
		DE_FATAL("Native handle can't be duplicated");
}

NativeHandle::NativeHandle (int fd)
	: m_fd						(fd)
	, m_win32HandleType			(WIN32HANDLETYPE_LAST)
	, m_win32Handle				(DE_NULL)
	, m_androidHardwareBuffer	(DE_NULL)
{
}

NativeHandle::NativeHandle (Win32HandleType handleType, vk::pt::Win32Handle handle)
	: m_fd						(-1)
	, m_win32HandleType			(handleType)
	, m_win32Handle				(handle)
	, m_androidHardwareBuffer	(DE_NULL)
{
}

NativeHandle::NativeHandle (vk::pt::AndroidHardwareBufferPtr buffer)
	: m_fd						(-1)
	, m_win32HandleType			(WIN32HANDLETYPE_LAST)
	, m_win32Handle				(DE_NULL)
	, m_androidHardwareBuffer	(buffer)
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
		DE_ASSERT(!m_androidHardwareBuffer.internal);
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
				DE_ASSERT(!m_androidHardwareBuffer.internal);
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

#if defined(USE_ANDROID_O_HARDWARE_BUFFER)
	if (m_androidHardwareBuffer.internal)
	{
		DE_ASSERT(m_fd == -1);
		DE_ASSERT(!m_win32Handle.internal);
		AHardwareBuffer_release((AHardwareBuffer*)m_androidHardwareBuffer.internal);
	}
#endif

	m_fd					= -1;
	m_win32Handle			= vk::pt::Win32Handle(DE_NULL);
	m_win32HandleType		= WIN32HANDLETYPE_LAST;
	m_androidHardwareBuffer	= vk::pt::AndroidHardwareBufferPtr(DE_NULL);
}

NativeHandle& NativeHandle::operator= (int fd)
{
	reset();

	m_fd = fd;

	return *this;
}

NativeHandle& NativeHandle::operator= (vk::pt::AndroidHardwareBufferPtr buffer)
{
	reset();

	m_androidHardwareBuffer = buffer;

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
	m_androidHardwareBuffer = vk::pt::AndroidHardwareBufferPtr(DE_NULL);
}

vk::pt::Win32Handle NativeHandle::getWin32Handle (void) const
{
	DE_ASSERT(m_fd == -1);
	DE_ASSERT(!m_androidHardwareBuffer.internal);
	return m_win32Handle;
}

int NativeHandle::getFd (void) const
{
	DE_ASSERT(!m_win32Handle.internal);
	DE_ASSERT(!m_androidHardwareBuffer.internal);
	return m_fd;
}


vk::pt::AndroidHardwareBufferPtr NativeHandle::getAndroidHardwareBuffer (void) const
{
	DE_ASSERT(m_fd == -1);
	DE_ASSERT(!m_win32Handle.internal);
	return m_androidHardwareBuffer;
}

const char* externalSemaphoreTypeToName (vk::VkExternalSemaphoreHandleTypeFlagBits type)
{
	switch (type)
	{
		case vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT:
			return "opaque_fd";

		case vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT:
			return "opaque_win32";

		case vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT:
			return "opaque_win32_kmt";

		case vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT:
			return "d3d12_fenc";

		case vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT:
			return "sync_fd";

		default:
			DE_FATAL("Unknown external semaphore type");
			return DE_NULL;
	}
}

const char* externalFenceTypeToName (vk::VkExternalFenceHandleTypeFlagBits type)
{
	switch (type)
	{
		case vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_FD_BIT:
			return "opaque_fd";

		case vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_BIT:
			return "opaque_win32";

		case vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT:
			return "opaque_win32_kmt";

		case vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT:
			return "sync_fd";

		default:
			DE_FATAL("Unknown external fence type");
			return DE_NULL;
	}
}

const char* externalMemoryTypeToName (vk::VkExternalMemoryHandleTypeFlagBits type)
{
	switch (type)
	{
		case vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT:
			return "opaque_fd";

		case vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT:
			return "opaque_win32";

		case vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT:
			return "opaque_win32_kmt";

		case vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT:
			return "d3d11_texture";

		case vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT:
			return "d3d11_texture_kmt";

		case vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_HEAP_BIT:
			return "d3d12_heap";

		case vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT:
			return "d3d12_resource";

		case vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID:
			return "android_hardware_buffer";

		default:
			DE_FATAL("Unknown external memory type");
			return DE_NULL;
	}
}

bool isSupportedPermanence (vk::VkExternalSemaphoreHandleTypeFlagBits	type,
							Permanence										permanence)
{
	switch (type)
	{
		case vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT:
		case vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT:
			return permanence == PERMANENCE_PERMANENT || permanence == PERMANENCE_TEMPORARY;

		case vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT:
			return permanence == PERMANENCE_PERMANENT || permanence == PERMANENCE_TEMPORARY;

		case vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT:
			return permanence == PERMANENCE_TEMPORARY;

		default:
			DE_FATAL("Unknown external semaphore type");
			return false;
	}
}

Transference getHandelTypeTransferences (vk::VkExternalSemaphoreHandleTypeFlagBits type)
{
	switch (type)
	{
		case vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT:
		case vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT:
			return TRANSFERENCE_REFERENCE;

		case vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT:
			return TRANSFERENCE_REFERENCE;

		case vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT:
			return TRANSFERENCE_COPY;

		default:
			DE_FATAL("Unknown external semaphore type");
			return TRANSFERENCE_REFERENCE;
	}
}

bool isSupportedPermanence (vk::VkExternalFenceHandleTypeFlagBits	type,
							Permanence									permanence)
{
	switch (type)
	{
		case vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_BIT:
		case vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT:
			return permanence == PERMANENCE_PERMANENT || permanence == PERMANENCE_TEMPORARY;

		case vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_FD_BIT:
			return permanence == PERMANENCE_PERMANENT || permanence == PERMANENCE_TEMPORARY;

		case vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT:
			return permanence == PERMANENCE_TEMPORARY;

		default:
			DE_FATAL("Unknown external fence type");
			return false;
	}
}

Transference getHandelTypeTransferences (vk::VkExternalFenceHandleTypeFlagBits type)
{
	switch (type)
	{
		case vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_BIT:
		case vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT:
			return TRANSFERENCE_REFERENCE;

		case vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_FD_BIT:
			return TRANSFERENCE_REFERENCE;

		case vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT:
			return TRANSFERENCE_COPY;

		default:
			DE_FATAL("Unknown external fence type");
			return TRANSFERENCE_REFERENCE;
	}
}

int getMemoryFd (const vk::DeviceInterface&					vkd,
				 vk::VkDevice								device,
				 vk::VkDeviceMemory							memory,
				 vk::VkExternalMemoryHandleTypeFlagBits		externalType)
{
	const vk::VkMemoryGetFdInfoKHR	info	=
	{
		vk::VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR,
		DE_NULL,

		memory,
		externalType
	};
	int								fd		= -1;

	VK_CHECK(vkd.getMemoryFdKHR(device, &info, &fd));
	TCU_CHECK(fd >= 0);

	return fd;
}

void getMemoryNative (const vk::DeviceInterface&					vkd,
						 vk::VkDevice								device,
						 vk::VkDeviceMemory							memory,
						 vk::VkExternalMemoryHandleTypeFlagBits		externalType,
						 NativeHandle&								nativeHandle)
{
	if (externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT)
	{
		const vk::VkMemoryGetFdInfoKHR	info	=
		{
			vk::VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR,
			DE_NULL,

			memory,
			externalType
		};
		int								fd		= -1;

		VK_CHECK(vkd.getMemoryFdKHR(device, &info, &fd));
		TCU_CHECK(fd >= 0);
		nativeHandle = fd;
	}
	else if (externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT
		|| externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT)
	{
		const vk::VkMemoryGetWin32HandleInfoKHR	info	=
		{
			vk::VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR,
			DE_NULL,

			memory,
			externalType
		};
		vk::pt::Win32Handle						handle	(DE_NULL);

		VK_CHECK(vkd.getMemoryWin32HandleKHR(device, &info, &handle));

		switch (externalType)
		{
			case vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT:
				nativeHandle.setWin32Handle(NativeHandle::WIN32HANDLETYPE_NT, handle);
				break;

			case vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT:
				nativeHandle.setWin32Handle(NativeHandle::WIN32HANDLETYPE_KMT, handle);
				break;

			default:
				DE_FATAL("Unknown external memory handle type");
		}
	}
	else if (externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID)
	{
		const vk::VkMemoryGetAndroidHardwareBufferInfoANDROID	info	=
		{
			vk::VK_STRUCTURE_TYPE_MEMORY_GET_ANDROID_HARDWARE_BUFFER_INFO_ANDROID,
			DE_NULL,

			memory,
		};
		vk::pt::AndroidHardwareBufferPtr						ahb	(DE_NULL);

		VK_CHECK(vkd.getMemoryAndroidHardwareBufferANDROID(device, &info, &ahb));
		TCU_CHECK(ahb.internal);
		nativeHandle = ahb;
	}
	else
		DE_FATAL("Unknown external memory handle type");
}

vk::Move<vk::VkFence> createExportableFence (const vk::DeviceInterface&					vkd,
											 vk::VkDevice								device,
											 vk::VkExternalFenceHandleTypeFlagBits		externalType)
{
	const vk::VkExportFenceCreateInfo	exportCreateInfo	=
	{
		vk::VK_STRUCTURE_TYPE_EXPORT_FENCE_CREATE_INFO,
		DE_NULL,
		(vk::VkExternalFenceHandleTypeFlags)externalType
	};
	const vk::VkFenceCreateInfo				createInfo			=
	{
		vk::VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		&exportCreateInfo,
		0u
	};

	return vk::createFence(vkd, device, &createInfo);
}

int getFenceFd (const vk::DeviceInterface&					vkd,
				vk::VkDevice								device,
				vk::VkFence									fence,
				vk::VkExternalFenceHandleTypeFlagBits		externalType)
{
	const vk::VkFenceGetFdInfoKHR	info	=
	{
		vk::VK_STRUCTURE_TYPE_FENCE_GET_FD_INFO_KHR,
		DE_NULL,

		fence,
		externalType
	};
	int								fd	= -1;

	VK_CHECK(vkd.getFenceFdKHR(device, &info, &fd));
	TCU_CHECK(fd >= 0);

	return fd;
}

void getFenceNative (const vk::DeviceInterface&					vkd,
					 vk::VkDevice								device,
					 vk::VkFence								fence,
					 vk::VkExternalFenceHandleTypeFlagBits		externalType,
					 NativeHandle&								nativeHandle)
{
	if (externalType == vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT
		|| externalType == vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_FD_BIT)
	{
		const vk::VkFenceGetFdInfoKHR	info	=
		{
			vk::VK_STRUCTURE_TYPE_FENCE_GET_FD_INFO_KHR,
			DE_NULL,

			fence,
			externalType
		};
		int								fd	= -1;

		VK_CHECK(vkd.getFenceFdKHR(device, &info, &fd));
		TCU_CHECK(fd >= 0);
		nativeHandle = fd;
	}
	else if (externalType == vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_BIT
		|| externalType == vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT)
	{
		const vk::VkFenceGetWin32HandleInfoKHR	info	=
		{
			vk::VK_STRUCTURE_TYPE_FENCE_GET_WIN32_HANDLE_INFO_KHR,
			DE_NULL,

			fence,
			externalType
		};
		vk::pt::Win32Handle						handle	(DE_NULL);

		VK_CHECK(vkd.getFenceWin32HandleKHR(device, &info, &handle));

		switch (externalType)
		{
			case vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_BIT:
				nativeHandle.setWin32Handle(NativeHandle::WIN32HANDLETYPE_NT, handle);
				break;

			case vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT:
				nativeHandle.setWin32Handle(NativeHandle::WIN32HANDLETYPE_KMT, handle);
				break;

			default:
				DE_FATAL("Unknow external memory handle type");
		}
	}
	else
		DE_FATAL("Unknow external fence handle type");
}

void importFence (const vk::DeviceInterface&				vkd,
				  const vk::VkDevice						device,
				  const vk::VkFence							fence,
				  vk::VkExternalFenceHandleTypeFlagBits		externalType,
				  NativeHandle&								handle,
				  vk::VkFenceImportFlags					flags)
{
	if (externalType == vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT
		|| externalType == vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_FD_BIT)
	{
		const vk::VkImportFenceFdInfoKHR	importInfo	=
		{
			vk::VK_STRUCTURE_TYPE_IMPORT_FENCE_FD_INFO_KHR,
			DE_NULL,
			fence,
			flags,
			externalType,
			handle.getFd()
		};

		VK_CHECK(vkd.importFenceFdKHR(device, &importInfo));
		handle.disown();
	}
	else if (externalType == vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_BIT
			|| externalType == vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT)
	{
		const vk::VkImportFenceWin32HandleInfoKHR	importInfo	=
		{
			vk::VK_STRUCTURE_TYPE_IMPORT_FENCE_FD_INFO_KHR,
			DE_NULL,
			fence,
			flags,
			externalType,
			handle.getWin32Handle(),
			DE_NULL
		};

		VK_CHECK(vkd.importFenceWin32HandleKHR(device, &importInfo));
		// \note File descriptors and win32 handles behave differently, but this call wil make it seem like they would behave in same way
		handle.reset();
	}
	else
		DE_FATAL("Unknown fence external handle type");
}

vk::Move<vk::VkFence> createAndImportFence (const vk::DeviceInterface&				vkd,
											const vk::VkDevice						device,
											vk::VkExternalFenceHandleTypeFlagBits	externalType,
											NativeHandle&							handle,
											vk::VkFenceImportFlags					flags)
{
	vk::Move<vk::VkFence>	fence	(createFence(vkd, device));

	importFence(vkd, device, *fence, externalType, handle, flags);

	return fence;
}

vk::Move<vk::VkSemaphore> createExportableSemaphore (const vk::DeviceInterface&					vkd,
													 vk::VkDevice								device,
													 vk::VkExternalSemaphoreHandleTypeFlagBits	externalType)
{
	const vk::VkExportSemaphoreCreateInfo	exportCreateInfo	=
	{
		vk::VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO,
		DE_NULL,
		(vk::VkExternalSemaphoreHandleTypeFlags)externalType
	};
	const vk::VkSemaphoreCreateInfo				createInfo			=
	{
		vk::VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		&exportCreateInfo,
		0u
	};

	return vk::createSemaphore(vkd, device, &createInfo);
}

int getSemaphoreFd (const vk::DeviceInterface&					vkd,
					vk::VkDevice								device,
					vk::VkSemaphore								semaphore,
					vk::VkExternalSemaphoreHandleTypeFlagBits	externalType)
{
	const vk::VkSemaphoreGetFdInfoKHR	info	=
	{
		vk::VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR,
		DE_NULL,

		semaphore,
		externalType
	};
	int										fd	= -1;

	VK_CHECK(vkd.getSemaphoreFdKHR(device, &info, &fd));
	TCU_CHECK(fd >= 0);

	return fd;
}

void getSemaphoreNative (const vk::DeviceInterface&					vkd,
						 vk::VkDevice								device,
						 vk::VkSemaphore							semaphore,
						 vk::VkExternalSemaphoreHandleTypeFlagBits	externalType,
						 NativeHandle&								nativeHandle)
{
	if (externalType == vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT
		|| externalType == vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT)
	{
		const vk::VkSemaphoreGetFdInfoKHR	info	=
		{
			vk::VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR,
			DE_NULL,

			semaphore,
			externalType
		};
		int										fd	= -1;

		VK_CHECK(vkd.getSemaphoreFdKHR(device, &info, &fd));
		TCU_CHECK(fd >= 0);
		nativeHandle = fd;
	}
	else if (externalType == vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT
		|| externalType == vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT)
	{
		const vk::VkSemaphoreGetWin32HandleInfoKHR	info	=
		{
			vk::VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR,
			DE_NULL,

			semaphore,
			externalType
		};
		vk::pt::Win32Handle							handle	(DE_NULL);

		VK_CHECK(vkd.getSemaphoreWin32HandleKHR(device, &info, &handle));

		switch (externalType)
		{
			case vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT:
				nativeHandle.setWin32Handle(NativeHandle::WIN32HANDLETYPE_NT, handle);
				break;

			case vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT:
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
					  vk::VkExternalSemaphoreHandleTypeFlagBits		externalType,
					  NativeHandle&									handle,
					  vk::VkSemaphoreImportFlags					flags)
{
	if (externalType == vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT
		|| externalType == vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT)
	{
		const vk::VkImportSemaphoreFdInfoKHR	importInfo	=
		{
			vk::VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR,
			DE_NULL,
			semaphore,
			flags,
			externalType,
			handle.getFd()
		};

		VK_CHECK(vkd.importSemaphoreFdKHR(device, &importInfo));
		handle.disown();
	}
	else if (externalType == vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT
			|| externalType == vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT)
	{
		const vk::VkImportSemaphoreWin32HandleInfoKHR	importInfo	=
		{
			vk::VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR,
			DE_NULL,
			semaphore,
			flags,
			externalType,
			handle.getWin32Handle(),
			DE_NULL
		};

		VK_CHECK(vkd.importSemaphoreWin32HandleKHR(device, &importInfo));
		// \note File descriptors and win32 handles behave differently, but this call wil make it seem like they would behave in same way
		handle.reset();
	}
	else
		DE_FATAL("Unknown semaphore external handle type");
}

vk::Move<vk::VkSemaphore> createAndImportSemaphore (const vk::DeviceInterface&						vkd,
													const vk::VkDevice								device,
													vk::VkExternalSemaphoreHandleTypeFlagBits		externalType,
													NativeHandle&									handle,
													vk::VkSemaphoreImportFlags						flags)
{
	vk::Move<vk::VkSemaphore>	semaphore	(createSemaphore(vkd, device));

	importSemaphore(vkd, device, *semaphore, externalType, handle, flags);

	return semaphore;
}

vk::Move<vk::VkDeviceMemory> allocateExportableMemory (const vk::DeviceInterface&					vkd,
													   vk::VkDevice									device,
													   const vk::VkMemoryRequirements&				requirements,
													   vk::VkExternalMemoryHandleTypeFlagBits		externalType,
													   vk::VkBuffer									buffer,
													   deUint32&									exportedMemoryTypeIndex)
{
	exportedMemoryTypeIndex = chooseMemoryType(requirements.memoryTypeBits);
	const vk::VkMemoryDedicatedAllocateInfo	dedicatedInfo	=
	{
		vk::VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
		DE_NULL,

		(vk::VkImage)0,
		buffer
	};
	const vk::VkExportMemoryAllocateInfo	exportInfo	=
	{
		vk::VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
		!!buffer ? &dedicatedInfo : DE_NULL,
		(vk::VkExternalMemoryHandleTypeFlags)externalType
	};
	const vk::VkMemoryAllocateInfo			info		=
	{
		vk::VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		&exportInfo,
		requirements.size,
		exportedMemoryTypeIndex
	};
	return vk::allocateMemory(vkd, device, &info);
}

vk::Move<vk::VkDeviceMemory> allocateExportableMemory (const vk::DeviceInterface&					vkd,
													   vk::VkDevice									device,
													   const vk::VkMemoryRequirements&				requirements,
													   vk::VkExternalMemoryHandleTypeFlagBits		externalType,
													   vk::VkImage									image,
													   deUint32&									exportedMemoryTypeIndex)
{
	exportedMemoryTypeIndex = chooseMemoryType(requirements.memoryTypeBits);
	const vk::VkMemoryDedicatedAllocateInfo	dedicatedInfo	=
	{
		vk::VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
		DE_NULL,

		image,
		(vk::VkBuffer)0
	};
	const vk::VkExportMemoryAllocateInfo	exportInfo	=
	{
		vk::VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
		!!image ? &dedicatedInfo : DE_NULL,
		(vk::VkExternalMemoryHandleTypeFlags)externalType
	};
	const vk::VkMemoryAllocateInfo			info		=
	{
		vk::VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		&exportInfo,
		requirements.size,
		exportedMemoryTypeIndex
	};
	return vk::allocateMemory(vkd, device, &info);
}

vk::Move<vk::VkDeviceMemory> allocateExportableMemory (const vk::InstanceInterface&					vki,
													   vk::VkPhysicalDevice							physicalDevice,
													   const vk::DeviceInterface&					vkd,
													   vk::VkDevice									device,
													   const vk::VkMemoryRequirements&				requirements,
													   vk::VkExternalMemoryHandleTypeFlagBits		externalType,
													   bool											hostVisible,
													   vk::VkBuffer									buffer,
													   deUint32&									exportedMemoryTypeIndex)
{
	const vk::VkPhysicalDeviceMemoryProperties properties = vk::getPhysicalDeviceMemoryProperties(vki, physicalDevice);

	for (deUint32 memoryTypeIndex = 0; (1u << memoryTypeIndex) <= requirements.memoryTypeBits; memoryTypeIndex++)
	{
		if (((requirements.memoryTypeBits & (1u << memoryTypeIndex)) != 0)
			&& (((properties.memoryTypes[memoryTypeIndex].propertyFlags & vk::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0) == hostVisible))
		{
			const vk::VkMemoryDedicatedAllocateInfo	dedicatedInfo	=
			{
				vk::VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
				DE_NULL,

				(vk::VkImage)0,
				buffer
			};
			const vk::VkExportMemoryAllocateInfo	exportInfo	=
			{
				vk::VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
				!!buffer ? &dedicatedInfo : DE_NULL,
				(vk::VkExternalMemoryHandleTypeFlags)externalType
			};
			const vk::VkMemoryAllocateInfo			info		=
			{
				vk::VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				&exportInfo,
				requirements.size,
				memoryTypeIndex
			};

			exportedMemoryTypeIndex = memoryTypeIndex;
			return vk::allocateMemory(vkd, device, &info);
		}
	}

	TCU_THROW(NotSupportedError, "No supported memory type found");
}

static vk::Move<vk::VkDeviceMemory> importMemory (const vk::DeviceInterface&				vkd,
												  vk::VkDevice								device,
												  vk::VkBuffer								buffer,
												  vk::VkImage								image,
												  const vk::VkMemoryRequirements&			requirements,
												  vk::VkExternalMemoryHandleTypeFlagBits	externalType,
												  deUint32									memoryTypeIndex,
												  NativeHandle&								handle)
{
	const bool	isDedicated		= !!buffer || !!image;

	DE_ASSERT(!buffer || !image);

	if (externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT)
	{
		const vk::VkImportMemoryFdInfoKHR			importInfo		=
		{
			vk::VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR,
			DE_NULL,
			externalType,
			handle.getFd()
		};
		const vk::VkMemoryDedicatedAllocateInfo		dedicatedInfo	=
		{
			vk::VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
			&importInfo,
			image,
			buffer,
		};
		const vk::VkMemoryAllocateInfo				info			=
		{
			vk::VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			(isDedicated ? (const void*)&dedicatedInfo : (const void*)&importInfo),
			requirements.size,
			(memoryTypeIndex == ~0U) ? chooseMemoryType(requirements.memoryTypeBits) : memoryTypeIndex
		};
		vk::Move<vk::VkDeviceMemory> memory (vk::allocateMemory(vkd, device, &info));

		handle.disown();

		return memory;
	}
	else if (externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT
			|| externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT)
	{
		const vk::VkImportMemoryWin32HandleInfoKHR	importInfo		=
		{
			vk::VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR,
			DE_NULL,
			externalType,
			handle.getWin32Handle(),
			DE_NULL
		};
		const vk::VkMemoryDedicatedAllocateInfo		dedicatedInfo	=
		{
			vk::VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
			&importInfo,
			image,
			buffer,
		};
		const vk::VkMemoryAllocateInfo				info			=
		{
			vk::VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			(isDedicated ? (const void*)&dedicatedInfo : (const void*)&importInfo),
			requirements.size,
			(memoryTypeIndex == ~0U) ? chooseMemoryType(requirements.memoryTypeBits)  : memoryTypeIndex
		};
		vk::Move<vk::VkDeviceMemory> memory (vk::allocateMemory(vkd, device, &info));

		handle.disown();

		return memory;
	}
#if defined(USE_ANDROID_O_HARDWARE_BUFFER)
	else if (externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID)
	{
		AHardwareBuffer_Desc desc;
		AHardwareBuffer_describe(static_cast<const AHardwareBuffer*>(handle.getAndroidHardwareBuffer().internal), &desc);

		DE_ASSERT(desc.format == AHARDWAREBUFFER_FORMAT_BLOB || image != 0);

		vk::VkImportAndroidHardwareBufferInfoANDROID	importInfo =
		{
			vk::VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID,
			DE_NULL,
			handle.getAndroidHardwareBuffer()
		};
		const vk::VkMemoryDedicatedAllocateInfo		dedicatedInfo =
		{
			vk::VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR,
			&importInfo,
			image,
			buffer,
		};
		const vk::VkMemoryAllocateInfo					info =
		{
			vk::VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			(isDedicated ? (const void*)&dedicatedInfo : (const void*)&importInfo),
			requirements.size,
			(memoryTypeIndex == ~0U) ? chooseMemoryType(requirements.memoryTypeBits)  : memoryTypeIndex
		};
		vk::Move<vk::VkDeviceMemory> memory (vk::allocateMemory(vkd, device, &info));

		handle.disown();

		return memory;
	}
#endif // (USE_ANDROID_O_HARDWARE_BUFFER)
	else
	{
		DE_FATAL("Unknown external memory type");
		return vk::Move<vk::VkDeviceMemory>();
	}
}

vk::Move<vk::VkDeviceMemory> importMemory (const vk::DeviceInterface&					vkd,
										   vk::VkDevice									device,
										   const vk::VkMemoryRequirements&				requirements,
										   vk::VkExternalMemoryHandleTypeFlagBits		externalType,
										   deUint32										memoryTypeIndex,
										   NativeHandle&								handle)
{
	return importMemory(vkd, device, (vk::VkBuffer)0, (vk::VkImage)0, requirements, externalType, memoryTypeIndex, handle);
}

vk::Move<vk::VkDeviceMemory> importDedicatedMemory (const vk::DeviceInterface&					vkd,
													vk::VkDevice								device,
													vk::VkBuffer								buffer,
													const vk::VkMemoryRequirements&				requirements,
													vk::VkExternalMemoryHandleTypeFlagBits		externalType,
													deUint32									memoryTypeIndex,
													NativeHandle&								handle)
{
	return importMemory(vkd, device, buffer, (vk::VkImage)0, requirements, externalType, memoryTypeIndex, handle);
}

vk::Move<vk::VkDeviceMemory> importDedicatedMemory (const vk::DeviceInterface&					vkd,
													vk::VkDevice								device,
													vk::VkImage									image,
													const vk::VkMemoryRequirements&				requirements,
													vk::VkExternalMemoryHandleTypeFlagBits		externalType,
													deUint32									memoryTypeIndex,
													NativeHandle&								handle)
{
	return importMemory(vkd, device, (vk::VkBuffer)0, image, requirements, externalType, memoryTypeIndex, handle);
}

vk::Move<vk::VkBuffer> createExternalBuffer (const vk::DeviceInterface&					vkd,
											 vk::VkDevice								device,
											 deUint32									queueFamilyIndex,
											 vk::VkExternalMemoryHandleTypeFlagBits		externalType,
											 vk::VkDeviceSize							size,
											 vk::VkBufferCreateFlags					createFlags,
											 vk::VkBufferUsageFlags						usageFlags)
{
	const vk::VkExternalMemoryBufferCreateInfo			externalCreateInfo	=
	{
		vk::VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO,
		DE_NULL,
		(vk::VkExternalMemoryHandleTypeFlags)externalType
	};
	const vk::VkBufferCreateInfo						createInfo			=
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		&externalCreateInfo,
		createFlags,
		size,
		usageFlags,
		vk::VK_SHARING_MODE_EXCLUSIVE,
		1u,
		&queueFamilyIndex
	};

	return vk::createBuffer(vkd, device, &createInfo);
}

vk::Move<vk::VkImage> createExternalImage (const vk::DeviceInterface&					vkd,
										   vk::VkDevice									device,
										   deUint32										queueFamilyIndex,
										   vk::VkExternalMemoryHandleTypeFlagBits		externalType,
										   vk::VkFormat									format,
										   deUint32										width,
										   deUint32										height,
										   vk::VkImageTiling							tiling,
										   vk::VkImageCreateFlags						createFlags,
										   vk::VkImageUsageFlags						usageFlags,
										   deUint32 mipLevels,
										   deUint32 arrayLayers)
{
	const vk::VkExternalMemoryImageCreateInfo		externalCreateInfo	=
	{
		vk::VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
		DE_NULL,
		(vk::VkExternalMemoryHandleTypeFlags)externalType
	};
	const vk::VkImageCreateInfo						createInfo			=
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		&externalCreateInfo,
		createFlags,
		vk::VK_IMAGE_TYPE_2D,
		format,
		{ width, height, 1u, },
		mipLevels,
		arrayLayers,
		vk::VK_SAMPLE_COUNT_1_BIT,
		tiling,
		usageFlags,
		vk::VK_SHARING_MODE_EXCLUSIVE,
		1,
		&queueFamilyIndex,
		vk::VK_IMAGE_LAYOUT_UNDEFINED
	};

	return vk::createImage(vkd, device, &createInfo);
}

} // ExternalMemoryUtil
} // vkt
