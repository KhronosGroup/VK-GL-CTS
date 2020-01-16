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

#if (DE_OS == DE_OS_ANDROID)
#   include <sys/system_properties.h>
#endif

#if (DE_OS == DE_OS_ANDROID) && defined(__ANDROID_API_O__) && (DE_ANDROID_API >= __ANDROID_API_O__)
#	include <android/hardware_buffer.h>
#	include "deDynamicLibrary.hpp"
#	define BUILT_WITH_ANDROID_HARDWARE_BUFFER 1
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
	, m_hostPtr					(DE_NULL)
{
}

NativeHandle::NativeHandle (const NativeHandle& other)
	: m_fd						(-1)
	, m_win32HandleType			(WIN32HANDLETYPE_LAST)
	, m_win32Handle				(DE_NULL)
	, m_androidHardwareBuffer	(DE_NULL)
	, m_hostPtr					(DE_NULL)
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
	else if (other.m_androidHardwareBuffer.internal)
	{
		DE_ASSERT(other.m_fd == -1);
		DE_ASSERT(!other.m_win32Handle.internal);
		m_androidHardwareBuffer = other.m_androidHardwareBuffer;
		AndroidHardwareBufferExternalApi::getInstance()->acquire(m_androidHardwareBuffer);
	}
	else
		DE_FATAL("Native handle can't be duplicated");
}

NativeHandle::NativeHandle (int fd)
	: m_fd						(fd)
	, m_win32HandleType			(WIN32HANDLETYPE_LAST)
	, m_win32Handle				(DE_NULL)
	, m_androidHardwareBuffer	(DE_NULL)
	, m_hostPtr					(DE_NULL)
{
}

NativeHandle::NativeHandle (Win32HandleType handleType, vk::pt::Win32Handle handle)
	: m_fd						(-1)
	, m_win32HandleType			(handleType)
	, m_win32Handle				(handle)
	, m_androidHardwareBuffer	(DE_NULL)
	, m_hostPtr					(DE_NULL)
{
}

NativeHandle::NativeHandle (vk::pt::AndroidHardwareBufferPtr buffer)
	: m_fd						(-1)
	, m_win32HandleType			(WIN32HANDLETYPE_LAST)
	, m_win32Handle				(DE_NULL)
	, m_androidHardwareBuffer	(buffer)
	, m_hostPtr					(DE_NULL)
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
	if (m_androidHardwareBuffer.internal)
	{
		DE_ASSERT(m_fd == -1);
		DE_ASSERT(!m_win32Handle.internal);
		AndroidHardwareBufferExternalApi::getInstance()->release(m_androidHardwareBuffer);
	}
	m_fd					= -1;
	m_win32Handle			= vk::pt::Win32Handle(DE_NULL);
	m_win32HandleType		= WIN32HANDLETYPE_LAST;
	m_androidHardwareBuffer	= vk::pt::AndroidHardwareBufferPtr(DE_NULL);
	m_hostPtr				= DE_NULL;
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

void NativeHandle::setHostPtr(void* hostPtr)
{
	reset();

	m_hostPtr = hostPtr;
}

void NativeHandle::disown (void)
{
	m_fd = -1;
	m_win32Handle = vk::pt::Win32Handle(DE_NULL);
	m_androidHardwareBuffer = vk::pt::AndroidHardwareBufferPtr(DE_NULL);
	m_hostPtr = DE_NULL;
}

vk::pt::Win32Handle NativeHandle::getWin32Handle (void) const
{
	DE_ASSERT(m_fd == -1);
	DE_ASSERT(!m_androidHardwareBuffer.internal);
	DE_ASSERT(m_hostPtr == DE_NULL);

	return m_win32Handle;
}

int NativeHandle::getFd (void) const
{
	DE_ASSERT(!m_win32Handle.internal);
	DE_ASSERT(!m_androidHardwareBuffer.internal);
	DE_ASSERT(m_hostPtr == DE_NULL);
	return m_fd;
}

vk::pt::AndroidHardwareBufferPtr NativeHandle::getAndroidHardwareBuffer (void) const
{
	DE_ASSERT(m_fd == -1);
	DE_ASSERT(!m_win32Handle.internal);
	DE_ASSERT(m_hostPtr == DE_NULL);
	return m_androidHardwareBuffer;
}

void* NativeHandle::getHostPtr(void) const
{
	DE_ASSERT(m_fd == -1);
	DE_ASSERT(!m_win32Handle.internal);
	return m_hostPtr;
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

		case vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT:
			return "dma_buf";

		case vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT:
			return "host_allocation";

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
	if (externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT
		|| externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT)
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
		if (!AndroidHardwareBufferExternalApi::getInstance())
		{
			TCU_THROW(NotSupportedError, "Platform doesn't support Android Hardware Buffer handles");
		}
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
					 NativeHandle&								nativeHandle,
					 bool										expectFenceUnsignaled)
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

		if (externalType == vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT)
		{
			TCU_CHECK(!expectFenceUnsignaled || (fd >= 0));
		}
		else
		{
			TCU_CHECK(fd >= 0);
		}

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
			vk::VK_STRUCTURE_TYPE_IMPORT_FENCE_WIN32_HANDLE_INFO_KHR,
			DE_NULL,
			fence,
			flags,
			externalType,
			handle.getWin32Handle(),
			(vk::pt::Win32LPCWSTR)DE_NULL
		};

		VK_CHECK(vkd.importFenceWin32HandleKHR(device, &importInfo));
		// \note Importing a fence payload from Windows handles does not transfer ownership of the handle to the Vulkan implementation,
		//   so we do not disown the handle until after all use has complete.
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

vk::Move<vk::VkSemaphore> createExportableSemaphoreType (const vk::DeviceInterface&					vkd,
														 vk::VkDevice								device,
														 vk::VkSemaphoreType						semaphoreType,
														 vk::VkExternalSemaphoreHandleTypeFlagBits	externalType)
{
	const vk::VkSemaphoreTypeCreateInfo		semaphoreTypeCreateInfo	=
	{
		vk::VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
		DE_NULL,
		semaphoreType,
		0,
	};
	const vk::VkExportSemaphoreCreateInfo	exportCreateInfo	=
	{
		vk::VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO,
		&semaphoreTypeCreateInfo,
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
			vk::VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,
			DE_NULL,
			semaphore,
			flags,
			externalType,
			handle.getWin32Handle(),
			(vk::pt::Win32LPCWSTR)DE_NULL
		};

		VK_CHECK(vkd.importSemaphoreWin32HandleKHR(device, &importInfo));
		// \note Importing a semaphore payload from Windows handles does not transfer ownership of the handle to the Vulkan implementation,
		//   so we do not disown the handle until after all use has complete.
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

	if (externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT
		|| externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT)
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
			(vk::pt::Win32LPCWSTR)DE_NULL
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

		// The handle's owned reference must also be released. Do not discard the handle below.
		if (externalType != vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT)
			handle.disown();

		return memory;
	}
	else if (externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID)
	{
		AndroidHardwareBufferExternalApi* ahbApi = AndroidHardwareBufferExternalApi::getInstance();
		if (!ahbApi)
		{
			TCU_THROW(NotSupportedError, "Platform doesn't support Android Hardware Buffer handles");
		}

		deUint32 ahbFormat = 0;
		ahbApi->describe(handle.getAndroidHardwareBuffer(), DE_NULL, DE_NULL, DE_NULL, &ahbFormat, DE_NULL, DE_NULL);
		DE_ASSERT(ahbApi->ahbFormatIsBlob(ahbFormat) || image != 0);

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

		return memory;
	}
	else if (externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID)
	{
		AndroidHardwareBufferExternalApi* ahbApi = AndroidHardwareBufferExternalApi::getInstance();
		if (!ahbApi)
		{
			TCU_THROW(NotSupportedError, "Platform doesn't support Android Hardware Buffer handles");
		}

		deUint32 ahbFormat = 0;
		ahbApi->describe(handle.getAndroidHardwareBuffer(), DE_NULL, DE_NULL, DE_NULL, &ahbFormat, DE_NULL, DE_NULL);
		DE_ASSERT(ahbApi->ahbFormatIsBlob(ahbFormat) || image != 0);

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

		return memory;
	}
	else if (externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT)
	{
		DE_ASSERT(memoryTypeIndex != ~0U);

		const vk::VkImportMemoryHostPointerInfoEXT	importInfo		=
		{
			vk::VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT,
			DE_NULL,
			externalType,
			handle.getHostPtr()
		};
		const vk::VkMemoryDedicatedAllocateInfo		dedicatedInfo	=
		{
			vk::VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
			&importInfo,
			image,
			buffer,
		};
		const vk::VkMemoryAllocateInfo					info =
		{
			vk::VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			(isDedicated ? (const void*)&dedicatedInfo : (const void*)&importInfo),
			requirements.size,
			memoryTypeIndex
		};
		vk::Move<vk::VkDeviceMemory> memory (vk::allocateMemory(vkd, device, &info));

		return memory;
	}
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
										   deUint32										mipLevels,
										   deUint32										arrayLayers)
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

#if (DE_OS == DE_OS_ANDROID)
#  if defined(__ANDROID_API_P__) && (DE_ANDROID_API >= __ANDROID_API_P__)
#      define BUILT_WITH_ANDROID_P_HARDWARE_BUFFER 1
#  endif

static deInt32 androidGetSdkVersion()
{
	static deInt32 sdkVersion = -1;
	if (sdkVersion < 0)
	{
		char value[128] = {0};
		__system_property_get("ro.build.version.sdk", value);
		sdkVersion = static_cast<deInt32>(strtol(value, DE_NULL, 10));
		printf("SDK Version is %d\n", sdkVersion);
	}
	return sdkVersion;
}

static deInt32 checkAnbApiBuild()
{
	deInt32 sdkVersion = androidGetSdkVersion();
#if !defined(BUILT_WITH_ANDROID_HARDWARE_BUFFER)
	// When testing AHB on Android-O and newer the CTS must be compiled against API26 or newer.
	DE_TEST_ASSERT(!(sdkVersion >= 26)); /* __ANDROID_API_O__ */
#endif // !defined(BUILT_WITH_ANDROID_HARDWARE_BUFFER)
#if !defined(BUILT_WITH_ANDROID_P_HARDWARE_BUFFER)
	// When testing AHB on Android-P and newer the CTS must be compiled against API28 or newer.
	DE_TEST_ASSERT(!(sdkVersion >= 28)); /*__ANDROID_API_P__ */
#endif // !defined(BUILT_WITH_ANDROID_P_HARDWARE_BUFFER)
	return sdkVersion;
}

bool AndroidHardwareBufferExternalApi::supportsAhb()
{
	return (checkAnbApiBuild() >= __ANDROID_API_O__);
}

bool AndroidHardwareBufferExternalApi::supportsCubeMap()
{
	return (checkAnbApiBuild() >= 28);
}

AndroidHardwareBufferExternalApi::AndroidHardwareBufferExternalApi()
{
	deInt32 sdkVersion = checkAnbApiBuild();
	if(sdkVersion >= __ANDROID_API_O__)
	{
#if defined(BUILT_WITH_ANDROID_HARDWARE_BUFFER)
		if (!loadAhbDynamicApis(sdkVersion))
		{
			// Couldn't load  Android AHB system APIs.
			DE_TEST_ASSERT(false);
		}
#else
		// Invalid Android AHB APIs configuration. Please check the instructions on how to build NDK for Android.
		DE_TEST_ASSERT(false);
#endif // defined(BUILT_WITH_ANDROID_HARDWARE_BUFFER)
	}
}

AndroidHardwareBufferExternalApi::~AndroidHardwareBufferExternalApi()
{
}

#if defined(BUILT_WITH_ANDROID_HARDWARE_BUFFER)
typedef int  (*pfn_system_property_get)(const char *, char *);
typedef int  (*pfnAHardwareBuffer_allocate)(const AHardwareBuffer_Desc* desc, AHardwareBuffer** outBuffer);
typedef void (*pfnAHardwareBuffer_describe)(const AHardwareBuffer* buffer, AHardwareBuffer_Desc* outDesc);
typedef void (*pfnAHardwareBuffer_acquire)(AHardwareBuffer* buffer);
typedef void (*pfnAHardwareBuffer_release)(AHardwareBuffer* buffer);

struct AhbFunctions
{
	pfnAHardwareBuffer_allocate allocate;
	pfnAHardwareBuffer_describe describe;
	pfnAHardwareBuffer_acquire  acquire;
	pfnAHardwareBuffer_release  release;
};

static AhbFunctions ahbFunctions;

static bool ahbFunctionsLoaded(AhbFunctions* pAhbFunctions)
{
	static bool ahbApiLoaded = false;
	if (ahbApiLoaded ||
	    ((pAhbFunctions->allocate != DE_NULL) &&
		(pAhbFunctions->describe != DE_NULL) &&
		(pAhbFunctions->acquire  != DE_NULL) &&
		(pAhbFunctions->release  != DE_NULL)))
	{
		ahbApiLoaded = true;
		return true;
	}
	return false;
}

bool AndroidHardwareBufferExternalApi::loadAhbDynamicApis(deInt32 sdkVersion)
{
	if(sdkVersion >= __ANDROID_API_O__)
	{
		if (!ahbFunctionsLoaded(&ahbFunctions))
		{
			static de::DynamicLibrary libnativewindow("libnativewindow.so");
			ahbFunctions.allocate = reinterpret_cast<pfnAHardwareBuffer_allocate>(libnativewindow.getFunction("AHardwareBuffer_allocate"));
			ahbFunctions.describe = reinterpret_cast<pfnAHardwareBuffer_describe>(libnativewindow.getFunction("AHardwareBuffer_describe"));
			ahbFunctions.acquire  = reinterpret_cast<pfnAHardwareBuffer_acquire>(libnativewindow.getFunction("AHardwareBuffer_acquire"));
			ahbFunctions.release  = reinterpret_cast<pfnAHardwareBuffer_release>(libnativewindow.getFunction("AHardwareBuffer_release"));

			return ahbFunctionsLoaded(&ahbFunctions);

		}
		else
		{
			return true;
		}
	}

	return false;
}

class AndroidHardwareBufferExternalApi26 : public  AndroidHardwareBufferExternalApi
{
public:

	virtual vk::pt::AndroidHardwareBufferPtr allocate(deUint32 width, deUint32  height, deUint32 layers, deUint32  format, deUint64 usage);
	virtual void acquire(vk::pt::AndroidHardwareBufferPtr buffer);
	virtual void release(vk::pt::AndroidHardwareBufferPtr buffer);
	virtual void describe(const vk::pt::AndroidHardwareBufferPtr buffer,
				  deUint32* width,
				  deUint32* height,
				  deUint32* layers,
				  deUint32* format,
				  deUint64* usage,
				  deUint32* stride);
	virtual deUint64 vkUsageToAhbUsage(vk::VkImageUsageFlagBits vkFlag);
	virtual deUint64 vkCreateToAhbUsage(vk::VkImageCreateFlagBits vkFlag);
	virtual deUint32 vkFormatToAhbFormat(vk::VkFormat vkFormat);
	virtual deUint64 mustSupportAhbUsageFlags();
	virtual bool     ahbFormatIsBlob(deUint32 ahbFormat) { return (ahbFormat == AHARDWAREBUFFER_FORMAT_BLOB); };

	AndroidHardwareBufferExternalApi26() : AndroidHardwareBufferExternalApi() {};
	virtual ~AndroidHardwareBufferExternalApi26() {};

private:
	// Stop the compiler generating methods of copy the object
	AndroidHardwareBufferExternalApi26(AndroidHardwareBufferExternalApi26 const& copy);            // Not Implemented
	AndroidHardwareBufferExternalApi26& operator=(AndroidHardwareBufferExternalApi26 const& copy); // Not Implemented
};

vk::pt::AndroidHardwareBufferPtr AndroidHardwareBufferExternalApi26::allocate(	deUint32 width,
																				deUint32 height,
																				deUint32 layers,
																				deUint32 format,
																				deUint64 usage)
{
	AHardwareBuffer_Desc hbufferdesc = {
		width,
		height,
		layers,   // number of images
		format,
		usage,
		0u,       // Stride in pixels, ignored for AHardwareBuffer_allocate()
		0u,       // Initialize to zero, reserved for future use
		0u        // Initialize to zero, reserved for future use
	};

	AHardwareBuffer* hbuffer  = DE_NULL;
	ahbFunctions.allocate(&hbufferdesc, &hbuffer);

	return vk::pt::AndroidHardwareBufferPtr(hbuffer);
}

void AndroidHardwareBufferExternalApi26::acquire(vk::pt::AndroidHardwareBufferPtr buffer)
{
	ahbFunctions.acquire(static_cast<AHardwareBuffer*>(buffer.internal));
}

void AndroidHardwareBufferExternalApi26::release(vk::pt::AndroidHardwareBufferPtr buffer)
{
	ahbFunctions.release(static_cast<AHardwareBuffer*>(buffer.internal));
}

void AndroidHardwareBufferExternalApi26::describe( const vk::pt::AndroidHardwareBufferPtr buffer,
													deUint32* width,
													deUint32* height,
													deUint32* layers,
													deUint32* format,
													deUint64* usage,
													deUint32* stride)
{
	AHardwareBuffer_Desc desc;
	ahbFunctions.describe(static_cast<const AHardwareBuffer*>(buffer.internal), &desc);
	if (width)  *width  = desc.width;
	if (height) *height = desc.height;
	if (layers) *layers = desc.layers;
	if (format) *format = desc.format;
	if (usage)  *usage  = desc.usage;
	if (stride) *stride = desc.stride;
}

deUint64 AndroidHardwareBufferExternalApi26::vkUsageToAhbUsage(vk::VkImageUsageFlagBits vkFlags)
{
	switch(vkFlags)
	{
	  case vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT:
	  case vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT:
		// No AHB equivalent.
		return 0u;
	  case vk::VK_IMAGE_USAGE_SAMPLED_BIT:
		return AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;
	  case vk::VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT:
		return AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;
	  case vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT:
		return AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT;
	  default:
		  return 0u;
	}
}

deUint64 AndroidHardwareBufferExternalApi26::vkCreateToAhbUsage(vk::VkImageCreateFlagBits vkFlags)
{
	switch(vkFlags)
	{
	  case vk::VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT:
	  case vk::VK_IMAGE_CREATE_EXTENDED_USAGE_BIT:
		// No AHB equivalent.
		return 0u;
	  case vk::VK_IMAGE_CREATE_PROTECTED_BIT:
		return AHARDWAREBUFFER_USAGE_PROTECTED_CONTENT;
	  default:
		return 0u;
	}
}

deUint32 AndroidHardwareBufferExternalApi26::vkFormatToAhbFormat(vk::VkFormat vkFormat)
{
	 switch(vkFormat)
	 {
	   case vk::VK_FORMAT_R8G8B8A8_UNORM:
		 return AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
	   case vk::VK_FORMAT_R8G8B8_UNORM:
		 return AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM;
	   case vk::VK_FORMAT_R5G6B5_UNORM_PACK16:
		 return AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM;
	   case vk::VK_FORMAT_R16G16B16A16_SFLOAT:
		 return AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT;
	   case vk::VK_FORMAT_A2B10G10R10_UNORM_PACK32:
		 return AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM;
	   default:
		 return 0u;
	 }
}

deUint64 AndroidHardwareBufferExternalApi26::mustSupportAhbUsageFlags()
{
	return (AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE | AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT);
}

#if defined(BUILT_WITH_ANDROID_P_HARDWARE_BUFFER)
class AndroidHardwareBufferExternalApi28 : public  AndroidHardwareBufferExternalApi26
{
public:

	virtual deUint64 vkCreateToAhbUsage(vk::VkImageCreateFlagBits vkFlag);
	virtual deUint64 mustSupportAhbUsageFlags();

	AndroidHardwareBufferExternalApi28() : AndroidHardwareBufferExternalApi26() {};
	virtual ~AndroidHardwareBufferExternalApi28() {};

private:
	// Stop the compiler generating methods of copy the object
	AndroidHardwareBufferExternalApi28(AndroidHardwareBufferExternalApi28 const& copy);            // Not Implemented
	AndroidHardwareBufferExternalApi28& operator=(AndroidHardwareBufferExternalApi28 const& copy); // Not Implemented
};

deUint64 AndroidHardwareBufferExternalApi28::vkCreateToAhbUsage(vk::VkImageCreateFlagBits vkFlags)
{
	switch(vkFlags)
	{
	  case vk::VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT:
		return AHARDWAREBUFFER_USAGE_GPU_CUBE_MAP;
	  default:
		return AndroidHardwareBufferExternalApi26::vkCreateToAhbUsage(vkFlags);
	}
}

deUint64 AndroidHardwareBufferExternalApi28::mustSupportAhbUsageFlags()
{
	return AndroidHardwareBufferExternalApi26::mustSupportAhbUsageFlags() | AHARDWAREBUFFER_USAGE_GPU_CUBE_MAP | AHARDWAREBUFFER_USAGE_GPU_MIPMAP_COMPLETE;
}

#endif // defined(BUILT_WITH_ANDROID_P_HARDWARE_BUFFER)
#endif // defined(BUILT_WITH_ANDROID_HARDWARE_BUFFER)
#endif // (DE_OS == DE_OS_ANDROID)

AndroidHardwareBufferExternalApi* AndroidHardwareBufferExternalApi::getInstance()
{
#if (DE_OS == DE_OS_ANDROID)
	deInt32 sdkVersion = checkAnbApiBuild();
#if defined(BUILT_WITH_ANDROID_HARDWARE_BUFFER)
#  if defined(__ANDROID_API_P__) && (DE_ANDROID_API >= __ANDROID_API_P__)
	if (sdkVersion >= __ANDROID_API_P__ )
	{
		static AndroidHardwareBufferExternalApi28 api28Instance;
		return &api28Instance;
	}
#  endif
#  if defined(__ANDROID_API_O__) && (DE_ANDROID_API >= __ANDROID_API_O__)
	if (sdkVersion >= __ANDROID_API_O__ )
	{
		static AndroidHardwareBufferExternalApi26 api26Instance;
		return &api26Instance;
	}
#  endif
#endif // defined(BUILT_WITH_ANDROID_HARDWARE_BUFFER)
	DE_UNREF(sdkVersion);
#endif // DE_OS == DE_OS_ANDROID
	return DE_NULL;
}

vk::VkPhysicalDeviceExternalMemoryHostPropertiesEXT getPhysicalDeviceExternalMemoryHostProperties(const vk::InstanceInterface&	vki,
																								  vk::VkPhysicalDevice			physicalDevice)
{
	vk::VkPhysicalDeviceExternalMemoryHostPropertiesEXT externalProps =
	{
		vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT,
		DE_NULL,
		0u,
	};

	vk::VkPhysicalDeviceProperties2 props2;
	deMemset(&props2, 0, sizeof(props2));
	props2.sType = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	props2.pNext = &externalProps;

	vki.getPhysicalDeviceProperties2(physicalDevice, &props2);

	return externalProps;
}

} // ExternalMemoryUtil
} // vkt
