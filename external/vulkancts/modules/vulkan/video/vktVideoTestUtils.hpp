#ifndef _VKTVIDEOTESTUTILS_HPP
#define _VKTVIDEOTESTUTILS_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
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
 * \brief Video Encoding and Decoding Utility Functions
 *//*--------------------------------------------------------------------*/

#include "vktVideoTestUtils.hpp"

#include "vkDefs.hpp"
#include "vkRefUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPlatform.hpp"
#include "vktTestCase.hpp"
#include "vktCustomInstancesDevices.hpp"

namespace vkt
{
namespace video
{

using namespace vk;
using namespace std;

typedef de::MovePtr<Allocation> AllocationPtr;

VkVideoDecodeH264ProfileInfoKHR						getProfileOperationH264D				(StdVideoH264ProfileIdc					stdProfileIdc					= STD_VIDEO_H264_PROFILE_IDC_MAIN,
																							 VkVideoDecodeH264PictureLayoutFlagBitsKHR	pictureLayout				= VK_VIDEO_DECODE_H264_PICTURE_LAYOUT_INTERLACED_INTERLEAVED_LINES_BIT_KHR);
VkVideoEncodeH264ProfileInfoEXT						getProfileOperationH264E				(StdVideoH264ProfileIdc					stdProfileIdc					= STD_VIDEO_H264_PROFILE_IDC_MAIN);
VkVideoDecodeH265ProfileInfoKHR						getProfileOperationH265D				(StdVideoH265ProfileIdc					stdProfileIdc					= STD_VIDEO_H265_PROFILE_IDC_MAIN);
VkVideoEncodeH265ProfileInfoEXT						getProfileOperationH265E				(StdVideoH265ProfileIdc					stdProfileIdc					= STD_VIDEO_H265_PROFILE_IDC_MAIN);

de::MovePtr<VkVideoDecodeCapabilitiesKHR>			getVideoDecodeCapabilities				(void*									pNext);

de::MovePtr<VkVideoDecodeH264CapabilitiesKHR>		getVideoCapabilitiesExtensionH264D		(void);
de::MovePtr<VkVideoEncodeH264CapabilitiesEXT>		getVideoCapabilitiesExtensionH264E		(void);
de::MovePtr<VkVideoDecodeH265CapabilitiesKHR>		getVideoCapabilitiesExtensionH265D		(void);
de::MovePtr<VkVideoEncodeH265CapabilitiesEXT>		getVideoCapabilitiesExtensionH265E		(void);
de::MovePtr<VkVideoCapabilitiesKHR>					getVideoCapabilities					(const InstanceInterface&				vk,
																							 VkPhysicalDevice						physicalDevice,
																							 const VkVideoProfileInfoKHR*			videoProfile,
																							 void*									pNext);

de::MovePtr<VkVideoDecodeH264ProfileInfoKHR>		getVideoProfileExtensionH264D			(StdVideoH264ProfileIdc					stdProfileIdc					= STD_VIDEO_H264_PROFILE_IDC_MAIN,
																							 VkVideoDecodeH264PictureLayoutFlagBitsKHR	pictureLayout				= VK_VIDEO_DECODE_H264_PICTURE_LAYOUT_INTERLACED_INTERLEAVED_LINES_BIT_KHR);
de::MovePtr<VkVideoEncodeH264ProfileInfoEXT>		getVideoProfileExtensionH264E			(StdVideoH264ProfileIdc					stdProfileIdc					= STD_VIDEO_H264_PROFILE_IDC_MAIN);
de::MovePtr<VkVideoDecodeH265ProfileInfoKHR>		getVideoProfileExtensionH265D			(StdVideoH265ProfileIdc					stdProfileIdc					= STD_VIDEO_H265_PROFILE_IDC_MAIN);
de::MovePtr<VkVideoEncodeH265ProfileInfoEXT>		getVideoProfileExtensionH265E			(StdVideoH265ProfileIdc					stdProfileIdc					= STD_VIDEO_H265_PROFILE_IDC_MAIN);
de::MovePtr<VkVideoProfileInfoKHR>					getVideoProfile							(VkVideoCodecOperationFlagBitsKHR		videoCodecOperation,
																							 void*									pNext,
																							 VkVideoChromaSubsamplingFlagsKHR		chromaSubsampling				= VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR,
																							 VkVideoComponentBitDepthFlagsKHR		lumaBitDepth					= VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,
																							 VkVideoComponentBitDepthFlagsKHR		chromaBitDepth					= VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR);
de::MovePtr<VkVideoProfileListInfoKHR>				getVideoProfileList						(const VkVideoProfileInfoKHR*			videoProfile);

const VkExtensionProperties*						getVideoExtensionProperties				(const VkVideoCodecOperationFlagBitsKHR	codecOperation);
de::MovePtr<VkVideoSessionCreateInfoKHR>			getVideoSessionCreateInfo				(deUint32								queueFamilyIndex,
																							 const VkVideoProfileInfoKHR*			videoProfile,
																							 const VkExtent2D&						codedExtent						= { 1920, 1080 },
																							 VkFormat								pictureFormat					= VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
																							 VkFormat								referencePicturesFormat			= VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
																							 deUint32								maxReferencePicturesSlotsCount	= 2u,
																							 deUint32								maxReferencePicturesActiveCount	= 2u);

vector<AllocationPtr>								getAndBindVideoSessionMemory			(const DeviceInterface&					vkd,
																							 const VkDevice							device,
																							 VkVideoSessionKHR						videoSession,
																							 Allocator&								allocator);

de::MovePtr<vector<VkFormat>>						getSupportedFormats						(const InstanceInterface& vk,
																							const VkPhysicalDevice					physicalDevice,
																							const VkImageUsageFlags					imageUsageFlags,
																							const VkVideoProfileListInfoKHR*		videoProfileList);

VkVideoFormatPropertiesKHR							getSupportedFormatProperties			(const InstanceInterface& vk,
																							 const VkPhysicalDevice					physicalDevice,
																							 const VkImageUsageFlags				imageUsageFlags,
																							 const VkVideoProfileListInfoKHR*		videoProfileList,
																							 const VkFormat							format);
bool												validateVideoExtent						(const VkExtent2D&						codedExtent,
																							 const VkVideoCapabilitiesKHR&			videoCapabilities);
bool												validateFormatSupport					(const InstanceInterface&				vk,
																							 VkPhysicalDevice						physicalDevice,
																							 const VkImageUsageFlags				imageUsageFlags,
																							 const VkVideoProfileListInfoKHR*		videoProfileList,
																							 const VkFormat							format,
																							 const bool								throwException = true);

bool												validateVideoProfileList				(const InstanceInterface& vk,
																							 VkPhysicalDevice						physicalDevice,
																							 const VkVideoProfileListInfoKHR*		videoProfileList,
																							 const VkFormat							format,
																							 const VkImageUsageFlags				usage);

VkImageCreateInfo									makeImageCreateInfo						(VkFormat								format,
																							 const VkExtent2D&						extent,
																							 const deUint32*						queueFamilyIndex,
																							 const VkImageUsageFlags				usage,
																							 void*									pNext,
																							 const deUint32							arrayLayers = 1);

de::MovePtr<StdVideoH264SequenceParameterSet>		getStdVideoH264SequenceParameterSet		(uint32_t								width,
																							 uint32_t								height,
																							 StdVideoH264SequenceParameterSetVui*	stdVideoH264SequenceParameterSetVui);
de::MovePtr<StdVideoH264PictureParameterSet>		getStdVideoH264PictureParameterSet		(void);

void												cmdPipelineImageMemoryBarrier2			(const DeviceInterface&					vk,
																							 const VkCommandBuffer					commandBuffer,
																							 const VkImageMemoryBarrier2KHR*		pImageMemoryBarriers,
																							 const size_t							imageMemoryBarrierCount = 1u,
																							 const VkDependencyFlags				dependencyFlags = 0);

class VideoBaseTestInstance : public TestInstance
{
public:
									VideoBaseTestInstance		(Context&								context);
	virtual							~VideoBaseTestInstance		(void);

	VkDevice						getDeviceSupportingQueue	(const VkQueueFlags						queueFlagsRequired = 0,
																 const VkVideoCodecOperationFlagsKHR	videoCodecOperationFlags = 0,
																 const VideoDevice::VideoDeviceFlags	videoDeviceFlags = VideoDevice::VIDEO_DEVICE_FLAG_NONE);
	bool							createDeviceSupportingQueue	(const VkQueueFlags						queueFlagsRequired,
																 const VkVideoCodecOperationFlagsKHR	videoCodecOperationFlags,
																 const VideoDevice::VideoDeviceFlags	videoDeviceFlags = VideoDevice::VIDEO_DEVICE_FLAG_NONE);
	const DeviceDriver&				getDeviceDriver				(void);
	const deUint32&					getQueueFamilyIndexTransfer	(void);
	const deUint32&					getQueueFamilyIndexDecode	(void);
	const deUint32&					getQueueFamilyIndexEncode	(void);
	Allocator&						getAllocator				(void);

	std::string						getVideoDataClipA			(void);
	std::string						getVideoDataClipB			(void);
	std::string						getVideoDataClipC			(void);
	std::string						getVideoDataClipD			(void);
	std::string						getVideoDataClipH264G13		(void);
	std::string						getVideoDataClipH265G13		(void);

protected:
	de::MovePtr<vector<deUint8>>	loadVideoData				(const string&							filename);

	VideoDevice						m_videoDevice;
};

} // video
} // vkt

#endif // _VKTVIDEOTESTUTILS_HPP
