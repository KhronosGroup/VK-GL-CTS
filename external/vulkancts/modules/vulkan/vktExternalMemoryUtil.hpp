#ifndef _VKTEXTERNALMEMORYUTIL_HPP
#define _VKTEXTERNALMEMORYUTIL_HPP
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

#include "tcuDefs.hpp"

#include "vkPlatform.hpp"
#include "vkRefUtil.hpp"

namespace vkt
{
namespace ExternalMemoryUtil
{

class NativeHandle
{
public:
	enum Win32HandleType
	{
		WIN32HANDLETYPE_NT = 0,
		WIN32HANDLETYPE_KMT,

		WIN32HANDLETYPE_LAST
	};

						NativeHandle	(void);
						NativeHandle	(const NativeHandle& other);
						NativeHandle	(int fd);
						NativeHandle	(Win32HandleType type, vk::pt::Win32Handle handle);
						~NativeHandle	(void);

	NativeHandle&		operator=		(int fd);

	void				setWin32Handle	(Win32HandleType type, vk::pt::Win32Handle handle);

	vk::pt::Win32Handle	getWin32Handle	(void) const;
	int					getFd			(void) const;
	void				disown			(void);
	void				reset			(void);

private:
	int					m_fd;
	Win32HandleType		m_win32HandleType;
	vk::pt::Win32Handle	m_win32Handle;

	// Disabled
	NativeHandle&		operator=		(const NativeHandle&);
};

const char*						externalSemaphoreTypeToName	(vk::VkExternalSemaphoreHandleTypeFlagBitsKHX type);
const char*						externalMemoryTypeToName	(vk::VkExternalMemoryHandleTypeFlagBitsKHX type);

enum Permanence
{
	PERMANENCE_PERMANENT = 0,
	PERMANENCE_TEMPORARY
};

Permanence						getHandleTypePermanence		(vk::VkExternalSemaphoreHandleTypeFlagBitsKHX type);

int								getMemoryFd					(const vk::DeviceInterface&					vkd,
															 vk::VkDevice								device,
															 vk::VkDeviceMemory							memory,
															 vk::VkExternalMemoryHandleTypeFlagBitsKHX	externalType);

void							getMemoryNative				(const vk::DeviceInterface&					vkd,
															 vk::VkDevice								device,
															 vk::VkDeviceMemory							memory,
															 vk::VkExternalMemoryHandleTypeFlagBitsKHX	externalType,
															 NativeHandle&								nativeHandle);

vk::Move<vk::VkSemaphore>		createExportableSemaphore	(const vk::DeviceInterface&						vkd,
															 vk::VkDevice									device,
															 vk::VkExternalSemaphoreHandleTypeFlagBitsKHX	externalType);

int								getSemaphoreFd				(const vk::DeviceInterface&						vkd,
															 vk::VkDevice									device,
															 vk::VkSemaphore								semaphore,
															 vk::VkExternalSemaphoreHandleTypeFlagBitsKHX	externalType);

void							getSemaphoreNative			(const vk::DeviceInterface&						vkd,
															 vk::VkDevice									device,
															 vk::VkSemaphore								semaphore,
															 vk::VkExternalSemaphoreHandleTypeFlagBitsKHX	externalType,
															 NativeHandle&									nativeHandle);

void							importSemaphore				(const vk::DeviceInterface&						vkd,
															 const vk::VkDevice								device,
															 const vk::VkSemaphore							semaphore,
															 vk::VkExternalSemaphoreHandleTypeFlagBitsKHX	externalType,
															 NativeHandle&									handle);

vk::Move<vk::VkSemaphore>		createAndImportSemaphore	(const vk::DeviceInterface&						vkd,
															 const vk::VkDevice								device,
															 vk::VkExternalSemaphoreHandleTypeFlagBitsKHX	externalType,
															 NativeHandle&									handle);

vk::Move<vk::VkDeviceMemory>	allocateExportableMemory	(const vk::DeviceInterface&					vkd,
															 vk::VkDevice								device,
															 const vk::VkMemoryRequirements&			requirements,
															 vk::VkExternalMemoryHandleTypeFlagBitsKHX	externalType);

// \note hostVisible argument is strict. Setting it to false will cause NotSupportedError to be thrown if non-host visible memory doesn't exist.
vk::Move<vk::VkDeviceMemory>	allocateExportableMemory	(const vk::InstanceInterface&				vki,
															 vk::VkPhysicalDevice						physicalDevice,
															 const vk::DeviceInterface&					vkd,
															 vk::VkDevice								device,
															 const vk::VkMemoryRequirements&			requirements,
															 vk::VkExternalMemoryHandleTypeFlagBitsKHX	externalType,
															 bool										hostVisible);

vk::Move<vk::VkDeviceMemory>	importMemory				(const vk::DeviceInterface&					vkd,
															 vk::VkDevice								device,
															 const vk::VkMemoryRequirements&			requirements,
															 vk::VkExternalMemoryHandleTypeFlagBitsKHX	externalType,
															 NativeHandle&								handle);

vk::Move<vk::VkBuffer>			createExternalBuffer		(const vk::DeviceInterface&					vkd,
															 vk::VkDevice								device,
															 deUint32									queueFamilyIndex,
															 vk::VkExternalMemoryHandleTypeFlagBitsKHX	externalType,
															 vk::VkDeviceSize							size,
															 vk::VkBufferUsageFlags						usage);

vk::Move<vk::VkImage>			createExternalImage			(const vk::DeviceInterface&					vkd,
															 vk::VkDevice								device,
															 deUint32									queueFamilyIndex,
															 vk::VkExternalMemoryHandleTypeFlagBitsKHX	externalType,
															 vk::VkFormat								format,
															 deUint32									width,
															 deUint32									height,
															 vk::VkImageTiling							tiling,
															 vk::VkImageUsageFlags						usage);

} // ExternalMemoryUtil
} // vkt

#endif // _VKTEXTERNALMEMORYUTIL_HPP
