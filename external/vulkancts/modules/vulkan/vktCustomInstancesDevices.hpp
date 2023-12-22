#ifndef _VKTCUSTOMINSTANCESDEVICES_HPP
#define _VKTCUSTOMINSTANCESDEVICES_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2019 Valve Corporation.
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
 * \brief Auxiliary functions to help create custom devices and instances.
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vktTestCase.hpp"

#include <vector>
#include <memory>

namespace vk
{
	class PlatformInterface;
	class InstanceInterface;
}

namespace tcu
{
	class CommandLine;
}

namespace vkt
{

std::vector<const char*> getValidationLayers (const vk::PlatformInterface& vkp);

std::vector<const char*> getValidationLayers (const vk::InstanceInterface& vki, vk::VkPhysicalDevice physicalDevice);

class CustomInstance
{
public:
								CustomInstance			();
#ifndef CTS_USES_VULKANSC
								CustomInstance			(Context& context, vk::Move<vk::VkInstance> instance, std::unique_ptr<vk::DebugReportRecorder>& recorder);
#else
								CustomInstance			(Context& context, vk::Move<vk::VkInstance> instance);
#endif // CTS_USES_VULKANSC
								CustomInstance			(CustomInstance&& other);
								~CustomInstance			();
	CustomInstance&				operator=				(CustomInstance&& other);
								operator vk::VkInstance	() const;
	void						swap					(CustomInstance& other);
	const vk::InstanceDriver&	getDriver				() const;
	void						collectMessages			();

								CustomInstance			(const CustomInstance& other) = delete;
	CustomInstance&				operator=				(const CustomInstance& other) = delete;
private:
	Context*									m_context;
#ifndef CTS_USES_VULKANSC
	std::unique_ptr<vk::DebugReportRecorder>	m_recorder;
#endif // CTS_USES_VULKANSC
	vk::Move<vk::VkInstance>					m_instance;
#ifndef CTS_USES_VULKANSC
	std::unique_ptr<vk::InstanceDriver>			m_driver;
	vk::Move<vk::VkDebugReportCallbackEXT>		m_callback;
#else
	std::unique_ptr<vk::InstanceDriverSC>		m_driver;
#endif // CTS_USES_VULKANSC
};

class UncheckedInstance
{
public:
						UncheckedInstance		();
#ifndef CTS_USES_VULKANSC
						UncheckedInstance		(Context& context, vk::VkInstance instance, const vk::VkAllocationCallbacks* pAllocator, std::unique_ptr<vk::DebugReportRecorder>& recorder);
#else
						UncheckedInstance		(Context& context, vk::VkInstance instance, const vk::VkAllocationCallbacks* pAllocator);
#endif // CTS_USES_VULKANSC
						UncheckedInstance		(UncheckedInstance&& other);
						~UncheckedInstance		();
	UncheckedInstance&	operator=				(UncheckedInstance&& other);
						operator vk::VkInstance	() const;
						operator bool			() const;
	void				swap					(UncheckedInstance& other);

						UncheckedInstance		(const UncheckedInstance& other) = delete;
	UncheckedInstance&	operator=				(const UncheckedInstance& other) = delete;
private:
	Context*									m_context;
#ifndef CTS_USES_VULKANSC
	std::unique_ptr<vk::DebugReportRecorder>	m_recorder;
#endif // CTS_USES_VULKANSC
	const vk::VkAllocationCallbacks*			m_allocator;
	vk::VkInstance								m_instance;
#ifndef CTS_USES_VULKANSC
	std::unique_ptr<vk::InstanceDriver>			m_driver;
	vk::Move<vk::VkDebugReportCallbackEXT>		m_callback;
#else
	std::unique_ptr<vk::InstanceDriverSC>		m_driver;
#endif // CTS_USES_VULKANSC
};

// Custom instances.

CustomInstance createCustomInstanceWithExtensions (Context& context, const std::vector<std::string>& extension, const vk::VkAllocationCallbacks* pAllocator = DE_NULL, bool allowLayers = true);

CustomInstance createCustomInstanceWithExtension (Context& context, const std::string& extension, const vk::VkAllocationCallbacks* pAllocator = DE_NULL, bool allowLayers = true);

CustomInstance createCustomInstanceFromContext (Context& context, const vk::VkAllocationCallbacks* pAllocator = DE_NULL, bool allowLayers = true);

CustomInstance createCustomInstanceFromInfo (Context& context, const vk::VkInstanceCreateInfo* instanceCreateInfo, const vk::VkAllocationCallbacks* pAllocator = DE_NULL, bool allowLayers = true);

// Unchecked instance: creation allowed to fail.

vk::VkResult createUncheckedInstance (Context& context, const vk::VkInstanceCreateInfo* instanceCreateInfo, const vk::VkAllocationCallbacks* pAllocator, UncheckedInstance* instance, bool allowLayers = true);

// Custom devices.

vk::Move<vk::VkDevice> createCustomDevice (bool validationEnabled, const vk::PlatformInterface& vkp, vk::VkInstance instance, const vk::InstanceInterface& vki, vk::VkPhysicalDevice physicalDevice, const vk::VkDeviceCreateInfo* pCreateInfo, const vk::VkAllocationCallbacks* pAllocator = DE_NULL);

// Unchecked device: creation allowed to fail.

vk::VkResult createUncheckedDevice (bool validationEnabled, const vk::InstanceInterface& vki, vk::VkPhysicalDevice physicalDevice, const vk::VkDeviceCreateInfo* pCreateInfo, const vk::VkAllocationCallbacks* pAllocator, vk::VkDevice* pDevice);

class CustomInstanceWrapper
{
public:
	CustomInstanceWrapper(Context& context);
	CustomInstanceWrapper(Context& context, const std::vector<std::string> extensions);
	vkt::CustomInstance instance;
};

class VideoDevice
{
public:
#ifndef CTS_USES_VULKANSC
	typedef vk::VkVideoCodecOperationFlagsKHR VideoCodecOperationFlags;
#else
	typedef uint32_t VideoCodecOperationFlags;
#endif // CTS_USES_VULKANSC

	enum VideoDeviceFlagBits
	{
		VIDEO_DEVICE_FLAG_NONE									= 0,
		VIDEO_DEVICE_FLAG_QUERY_WITH_STATUS_FOR_DECODE_SUPPORT	= 0x00000001,
		VIDEO_DEVICE_FLAG_REQUIRE_YCBCR_OR_NOT_SUPPORTED		= 0x00000002,
		VIDEO_DEVICE_FLAG_REQUIRE_SYNC2_OR_NOT_SUPPORTED		= 0x00000004
	};
	typedef uint32_t VideoDeviceFlags;

	static void						checkSupport				(Context&						context,
																 const VideoCodecOperationFlags	videoCodecOperation);
	static vk::VkQueueFlags			getQueueFlags				(const VideoCodecOperationFlags	videoCodecOperationFlags);

	static void						addVideoDeviceExtensions	(std::vector<const char*>&		deviceExtensions,
																 const uint32_t					apiVersion,
																 const vk::VkQueueFlags			queueFlagsRequired,
																 const VideoCodecOperationFlags	videoCodecOperationFlags);
	static bool						isVideoEncodeOperation		(const VideoCodecOperationFlags	videoCodecOperationFlags);
	static bool						isVideoDecodeOperation		(const VideoCodecOperationFlags	videoCodecOperationFlags);
	static bool						isVideoOperation			(const VideoCodecOperationFlags	videoCodecOperationFlags);

									VideoDevice					(Context&						context);
									VideoDevice					(Context&						context,
																 const VideoCodecOperationFlags	videoCodecOperation,
																 const VideoDeviceFlags			videoDeviceFlags = VIDEO_DEVICE_FLAG_NONE);
	virtual							~VideoDevice				(void);

	vk::VkDevice					getDeviceSupportingQueue	(const vk::VkQueueFlags			queueFlagsRequired = 0,
																 const VideoCodecOperationFlags	videoCodecOperationFlags = 0,
																 const VideoDeviceFlags			videoDeviceFlags = VIDEO_DEVICE_FLAG_NONE);
	bool							createDeviceSupportingQueue	(const vk::VkQueueFlags			queueFlagsRequired,
																 const VideoCodecOperationFlags	videoCodecOperationFlags,
																 const VideoDeviceFlags			videoDeviceFlags = VIDEO_DEVICE_FLAG_NONE);
	const vk::DeviceDriver&			getDeviceDriver				(void);
	deUint32					getQueueFamilyIndexTransfer	(void) const;
	deUint32					getQueueFamilyIndexDecode	(void) const;
	deUint32					getQueueFamilyIndexEncode	(void) const;
	deUint32					getQueueFamilyVideo			(void) const;
	vk::Allocator&					getAllocator				(void);

protected:
	Context&						m_context;

	vk::Move<vk::VkDevice>			m_logicalDevice;
	de::MovePtr<vk::DeviceDriver>	m_deviceDriver;
	de::MovePtr<vk::Allocator>		m_allocator;
	deUint32						m_queueFamilyTransfer;
	deUint32						m_queueFamilyDecode;
	deUint32						m_queueFamilyEncode;
	VideoCodecOperationFlags		m_videoCodecOperation;
};

}

#endif // _VKTCUSTOMINSTANCESDEVICES_HPP
