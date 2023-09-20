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
#include "vkMemUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "tcuCommandLine.hpp"
#include "tcuResource.hpp"

#include "vktCustomInstancesDevices.hpp"
#include "vktTestCase.hpp"

#include "vktVideoDecodeTests.hpp"

#include "vkMd5Sum.hpp"

using namespace vk;
using namespace std;

namespace vkt
{
namespace video
{

using namespace vk;
using namespace std;


bool videoLoggingEnabled()
{
	static int debuggingEnabled = -1; // -1 means it hasn't been checked yet
	if (debuggingEnabled == -1) {
		const char* s = getenv("CTS_DEBUG_VIDEO");
		debuggingEnabled = s != nullptr;
	}

	return debuggingEnabled > 0;
}

void cmdPipelineImageMemoryBarrier2 (const DeviceInterface&				vk,
									 const VkCommandBuffer				commandBuffer,
									 const VkImageMemoryBarrier2KHR*	pImageMemoryBarriers,
									 const size_t						imageMemoryBarrierCount,
									 const VkDependencyFlags			dependencyFlags)
{
	const deUint32				imageMemoryBarrierCount32	= static_cast<deUint32>(imageMemoryBarrierCount);
	const VkDependencyInfo		dependencyInfoKHR			=
	{
		vk::VK_STRUCTURE_TYPE_DEPENDENCY_INFO,	//  VkStructureType						sType;
		DE_NULL,								//  const void*							pNext;
		dependencyFlags,						//  VkDependencyFlags					dependencyFlags;
		0u,										//  deUint32							memoryBarrierCount;
		DE_NULL,								//  const VkMemoryBarrier2KHR*			pMemoryBarriers;
		0u,										//  deUint32							bufferMemoryBarrierCount;
		DE_NULL,								//  const VkBufferMemoryBarrier2KHR*	pBufferMemoryBarriers;
		imageMemoryBarrierCount32,				//  deUint32							imageMemoryBarrierCount;
		pImageMemoryBarriers,					//  const VkImageMemoryBarrier2KHR*		pImageMemoryBarriers;
	};

	DE_ASSERT(imageMemoryBarrierCount == imageMemoryBarrierCount32);

	vk.cmdPipelineBarrier2(commandBuffer, &dependencyInfoKHR);
}

static VkExtensionProperties makeExtensionProperties(const char* extensionName, deUint32 specVersion)
{
	const deUint32		  extensionNameLen = static_cast<deUint32>(deStrnlen(extensionName, VK_MAX_EXTENSION_NAME_SIZE));
	VkExtensionProperties result;

	deMemset(&result, 0, sizeof(result));

	deMemcpy(&result.extensionName, extensionName, extensionNameLen);

	result.specVersion = specVersion;

	return result;
}

static const VkExtensionProperties EXTENSION_PROPERTIES_H264_DECODE = makeExtensionProperties(VK_STD_VULKAN_VIDEO_CODEC_H264_DECODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_H264_DECODE_SPEC_VERSION);
static const VkExtensionProperties EXTENSION_PROPERTIES_H264_ENCODE = makeExtensionProperties(VK_STD_VULKAN_VIDEO_CODEC_H264_ENCODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_H264_ENCODE_SPEC_VERSION);
static const VkExtensionProperties EXTENSION_PROPERTIES_H265_DECODE = makeExtensionProperties(VK_STD_VULKAN_VIDEO_CODEC_H265_DECODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_H265_DECODE_SPEC_VERSION);
static const VkExtensionProperties EXTENSION_PROPERTIES_H265_ENCODE = makeExtensionProperties(VK_STD_VULKAN_VIDEO_CODEC_H265_ENCODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_H265_ENCODE_SPEC_VERSION);

bool VideoBaseTestInstance::createDeviceSupportingQueue (const VkQueueFlags queueFlagsRequired, const VkVideoCodecOperationFlagsKHR videoCodecOperationFlags, const VideoDevice::VideoDeviceFlags videoDeviceFlags)
{
	return m_videoDevice.createDeviceSupportingQueue(queueFlagsRequired, videoCodecOperationFlags, videoDeviceFlags);
}

VkDevice VideoBaseTestInstance::getDeviceSupportingQueue (const VkQueueFlags queueFlagsRequired, const VkVideoCodecOperationFlagsKHR videoCodecOperationFlags, const VideoDevice::VideoDeviceFlags videoDeviceFlags)
{
	return m_videoDevice.getDeviceSupportingQueue(queueFlagsRequired, videoCodecOperationFlags, videoDeviceFlags);
}

const DeviceDriver& VideoBaseTestInstance::getDeviceDriver (void)
{
	return m_videoDevice.getDeviceDriver();
}

deUint32 VideoBaseTestInstance::getQueueFamilyIndexTransfer (void)
{
	return m_videoDevice.getQueueFamilyIndexTransfer();
}

deUint32 VideoBaseTestInstance::getQueueFamilyIndexDecode (void)
{
	return m_videoDevice.getQueueFamilyIndexDecode();
}

deUint32 VideoBaseTestInstance::getQueueFamilyIndexEncode (void)
{
	return m_videoDevice.getQueueFamilyIndexEncode();
}

Allocator& VideoBaseTestInstance::getAllocator (void)
{
	return m_videoDevice.getAllocator();
}

de::MovePtr<vector<deUint8>> VideoBaseTestInstance::loadVideoData (const string& filename)
{
	tcu::Archive&					archive			= m_context.getTestContext().getArchive();
	de::UniquePtr<tcu::Resource>	resource		(archive.getResource(filename.c_str()));
	const int						resourceSize	= resource->getSize();
	de::MovePtr<vector<deUint8>>	result			(new vector<deUint8>(resourceSize));

	resource->read(result->data(), resource->getSize());

	return result;
}

std::string VideoBaseTestInstance::getVideoDataClipA (void)
{
	return std::string("vulkan/video/clip-a.h264");
}

std::string VideoBaseTestInstance::getVideoDataClipB (void)
{
	return std::string("vulkan/video/clip-b.h264");
}

std::string VideoBaseTestInstance::getVideoDataClipC (void)
{
	return std::string("vulkan/video/clip-c.h264");
}

std::string VideoBaseTestInstance::getVideoDataClipD (void)
{
	return std::string("vulkan/video/clip-d.h265");
}

std::string VideoBaseTestInstance::getVideoDataClipH264G13 (void)
{
	return std::string("vulkan/video/jellyfish-250-mbps-4k-uhd-GOB-IPB13.h264");
}

std::string VideoBaseTestInstance::getVideoDataClipH265G13 (void)
{
	return std::string("vulkan/video/jellyfish-250-mbps-4k-uhd-GOB-IPB13.h265");
}

const VkExtensionProperties* getVideoExtensionProperties (const VkVideoCodecOperationFlagBitsKHR codecOperation)
{
	switch (codecOperation)
	{
		case VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_EXT:	return &EXTENSION_PROPERTIES_H264_ENCODE;
		case VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_EXT:	return &EXTENSION_PROPERTIES_H265_ENCODE;
		case VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR:	return &EXTENSION_PROPERTIES_H264_DECODE;
		case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR:	return &EXTENSION_PROPERTIES_H265_DECODE;
		default:											TCU_THROW(InternalError, "Unkown codec operation");
	}
}

de::MovePtr<vector<VkFormat>> getSupportedFormats (const InstanceInterface&			vk,
												   const VkPhysicalDevice			physicalDevice,
												   const VkImageUsageFlags			imageUsageFlags,
												   const VkVideoProfileListInfoKHR*	videoProfileList)

{
	deUint32									videoFormatPropertiesCount = 0u;

	const VkPhysicalDeviceVideoFormatInfoKHR	videoFormatInfo =
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_FORMAT_INFO_KHR,	//  VkStructureType				sType;
		videoProfileList,											//  const void*					pNext;
		imageUsageFlags,											//  VkImageUsageFlags			imageUsage;
	};

	VkVideoFormatPropertiesKHR			videoFormatPropertiesKHR = {};
	videoFormatPropertiesKHR.sType = VK_STRUCTURE_TYPE_VIDEO_FORMAT_PROPERTIES_KHR;
	videoFormatPropertiesKHR.pNext = DE_NULL;


	vector<VkVideoFormatPropertiesKHR>			videoFormatProperties;
	de::MovePtr<vector<VkFormat>>				result;

	const VkResult res = vk.getPhysicalDeviceVideoFormatPropertiesKHR(physicalDevice, &videoFormatInfo, &videoFormatPropertiesCount, DE_NULL);

	if (res == VK_ERROR_FORMAT_NOT_SUPPORTED)
		return de::MovePtr<vector<VkFormat>>(DE_NULL);
	else
		VK_CHECK(res);

	videoFormatProperties.resize(videoFormatPropertiesCount, videoFormatPropertiesKHR);

	VK_CHECK(vk.getPhysicalDeviceVideoFormatPropertiesKHR(physicalDevice, &videoFormatInfo, &videoFormatPropertiesCount, videoFormatProperties.data()));

	DE_ASSERT(videoFormatPropertiesCount == videoFormatProperties.size());

	result = de::MovePtr<vector<VkFormat>>(new vector<VkFormat>);

	result->reserve(videoFormatProperties.size());

	for (const auto& videoFormatProperty : videoFormatProperties)
		result->push_back(videoFormatProperty.format);

	return result;
}

VkVideoFormatPropertiesKHR getSupportedFormatProperties (const InstanceInterface&	vk,
												   const VkPhysicalDevice			physicalDevice,
												   const VkImageUsageFlags			imageUsageFlags,
												   void*							pNext,
												   const VkFormat					format)

{
	if (format == VK_FORMAT_UNDEFINED)
		return VkVideoFormatPropertiesKHR();

	deUint32									videoFormatPropertiesCount = 0u;

	const VkPhysicalDeviceVideoFormatInfoKHR	videoFormatInfo =
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_FORMAT_INFO_KHR,	//  VkStructureType				sType;
		pNext,														//  const void*					pNext;
		imageUsageFlags,											//  VkImageUsageFlags			imageUsage;
	};

	VkVideoFormatPropertiesKHR			videoFormatPropertiesKHR = {};
	videoFormatPropertiesKHR.sType = VK_STRUCTURE_TYPE_VIDEO_FORMAT_PROPERTIES_KHR;
	videoFormatPropertiesKHR.pNext = DE_NULL;

	vector<VkVideoFormatPropertiesKHR>			videoFormatProperties;

	const VkResult res = vk.getPhysicalDeviceVideoFormatPropertiesKHR(physicalDevice, &videoFormatInfo, &videoFormatPropertiesCount, DE_NULL);

	if (res == VK_ERROR_FORMAT_NOT_SUPPORTED)
		return VkVideoFormatPropertiesKHR();
	else
		VK_CHECK(res);

	videoFormatProperties.resize(videoFormatPropertiesCount, videoFormatPropertiesKHR);

	VK_CHECK(vk.getPhysicalDeviceVideoFormatPropertiesKHR(physicalDevice, &videoFormatInfo, &videoFormatPropertiesCount, videoFormatProperties.data()));

	DE_ASSERT(videoFormatPropertiesCount == videoFormatProperties.size());

	for (const auto& videoFormatProperty : videoFormatProperties)
	{
		if (videoFormatProperty.format == format)
			return videoFormatProperty;
	};

	TCU_THROW(NotSupportedError, "Video format not found in properties list");
}


bool validateVideoExtent (const VkExtent2D& codedExtent, const VkVideoCapabilitiesKHR& videoCapabilities)
{
	if (!de::inRange(codedExtent.width, videoCapabilities.minCodedExtent.width, videoCapabilities.maxCodedExtent.width))
		TCU_THROW(NotSupportedError, "Video width does not fit capabilities");

	if (!de::inRange(codedExtent.height, videoCapabilities.minCodedExtent.height, videoCapabilities.maxCodedExtent.height))
		TCU_THROW(NotSupportedError, "Video height does not fit capabilities");

	return true;
}

bool validateFormatSupport (const InstanceInterface&			vk,
							VkPhysicalDevice					physicalDevice,
							const VkImageUsageFlags				imageUsageFlags,
							const VkVideoProfileListInfoKHR*	videoProfileList,
							const VkFormat						format,
							bool								throwException)
{
	de::MovePtr<vector<VkFormat>> supportedVideoFormats = getSupportedFormats(vk, physicalDevice, imageUsageFlags, videoProfileList);

	if (supportedVideoFormats != DE_NULL)
	{
		if (supportedVideoFormats->size() == 0)
			if (throwException)
				TCU_THROW(NotSupportedError, "Supported video formats count is 0");

		for (const auto& supportedVideoFormat : *supportedVideoFormats)
		{
			if (supportedVideoFormat == format)
				return true;
		}

		if (throwException)
			TCU_THROW(NotSupportedError, "Required format is not supported for video");
	}
	else
	{
		if (throwException)
			TCU_THROW(NotSupportedError, "Separate DPB and DST buffers expected");
	}

	return false;
}

void validateVideoProfileList (const InstanceInterface&				vk,
							   VkPhysicalDevice						physicalDevice,
							   const VkVideoProfileListInfoKHR*		videoProfileList,
							   const VkFormat						format,
							   const VkImageUsageFlags				usage)
{
	VkPhysicalDeviceImageFormatInfo2								imageFormatInfo = {};
	imageFormatInfo.sType		= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
	imageFormatInfo.pNext		= videoProfileList;
	imageFormatInfo.format		= format;
	imageFormatInfo.usage		= usage;


	VkImageFormatProperties2										imageFormatProperties = {};
	imageFormatProperties.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
	imageFormatProperties.pNext = DE_NULL;

	VK_CHECK(vk.getPhysicalDeviceImageFormatProperties2(physicalDevice, &imageFormatInfo, &imageFormatProperties));
}

VkVideoDecodeH264ProfileInfoKHR getProfileOperationH264Decode (StdVideoH264ProfileIdc stdProfileIdc, VkVideoDecodeH264PictureLayoutFlagBitsKHR pictureLayout)
{
	const VkVideoDecodeH264ProfileInfoKHR	videoProfileOperation	=
	{
		VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_PROFILE_INFO_KHR,	//  VkStructureType							sType;
		DE_NULL,												//  const void*								pNext;
		stdProfileIdc,											//  StdVideoH264ProfileIdc					stdProfileIdc;
		pictureLayout,											//  VkVideoDecodeH264PictureLayoutFlagBitsKHR	pictureLayout;
	};

	return videoProfileOperation;
}

VkVideoEncodeH264ProfileInfoEXT getProfileOperationH264Encode (StdVideoH264ProfileIdc stdProfileIdc)
{
	const VkVideoEncodeH264ProfileInfoEXT	videoProfileOperation	=
	{
		VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_PROFILE_INFO_EXT,	//  VkStructureType			sType;
		DE_NULL,												//  const void*				pNext;
		stdProfileIdc,											//  StdVideoH264ProfileIdc	stdProfileIdc;
	};

	return videoProfileOperation;
}

VkVideoDecodeH265ProfileInfoKHR getProfileOperationH265Decode (StdVideoH265ProfileIdc stdProfileIdc)
{
	const VkVideoDecodeH265ProfileInfoKHR	videoProfileOperation	=
	{
		VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_PROFILE_INFO_KHR,	//  VkStructureType			sType;
		DE_NULL,												//  const void*				pNext;
		stdProfileIdc,											//  StdVideoH265ProfileIdc	stdProfileIdc;
	};

	return videoProfileOperation;
}

VkVideoEncodeH265ProfileInfoEXT getProfileOperationH265Encode (StdVideoH265ProfileIdc stdProfileIdc)
{
	const VkVideoEncodeH265ProfileInfoEXT	videoProfileOperation	=
	{
		VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_PROFILE_INFO_EXT,	//  VkStructureType			sType;
		DE_NULL,												//  const void*				pNext;
		stdProfileIdc,											//  StdVideoH265ProfileIdc	stdProfileIdc;
	};

	return videoProfileOperation;
}

VkImageCreateInfo makeImageCreateInfo (VkFormat						format,
									   const VkExtent2D&			extent,
									   const deUint32*				queueFamilyIndex,
									   const VkImageUsageFlags		usage,
									   void*						pNext,
									   const deUint32				arrayLayers)
{


	const VkExtent3D		extent3D			= makeExtent3D(extent.width, extent.height, 1u);


	const VkImageCreateInfo	imageCreateInfo		=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,												//  VkStructureType			sType;
		pNext,																				//  const void*				pNext;
		(VkImageCreateFlags)0u,																//  VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,																	//  VkImageType				imageType;
		format,																				//  VkFormat				format;
		extent3D,																			//  VkExtent3D				extent;
		1,																					//  deUint32				mipLevels;
		arrayLayers,																		//  deUint32				arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,																//  VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,															//  VkImageTiling			tiling;
		usage,																				//  VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,															//  VkSharingMode			sharingMode;
		1u,																					//  deUint32				queueFamilyIndexCount;
		queueFamilyIndex,																	//  const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,															//  VkImageLayout			initialLayout;
	};

	return imageCreateInfo;
}

de::MovePtr<StdVideoH264SequenceParameterSet> getStdVideoH264SequenceParameterSet (deUint32								width,
																				   deUint32								height,
																				   StdVideoH264SequenceParameterSetVui*	stdVideoH264SequenceParameterSetVui)
{
	const StdVideoH264SpsFlags				stdVideoH264SpsFlags				=
	{
		0u,	//  deUint32	constraint_set0_flag:1;
		0u,	//  deUint32	constraint_set1_flag:1;
		0u,	//  deUint32	constraint_set2_flag:1;
		0u,	//  deUint32	constraint_set3_flag:1;
		0u,	//  deUint32	constraint_set4_flag:1;
		0u,	//  deUint32	constraint_set5_flag:1;
		1u,	//  deUint32	direct_8x8_inference_flag:1;
		0u,	//  deUint32	mb_adaptive_frame_field_flag:1;
		1u,	//  deUint32	frame_mbs_only_flag:1;
		0u,	//  deUint32	delta_pic_order_always_zero_flag:1;
		0u,	//  deUint32	separate_colour_plane_flag:1;
		0u,	//  deUint32	gaps_in_frame_num_value_allowed_flag:1;
		0u,	//  deUint32	qpprime_y_zero_transform_bypass_flag:1;
		0u,	//  deUint32	frame_cropping_flag:1;
		0u,	//  deUint32	seq_scaling_matrix_present_flag:1;
		0u,	//  deUint32	vui_parameters_present_flag:1;
	};

	const StdVideoH264SequenceParameterSet	stdVideoH264SequenceParameterSet	=
	{
		stdVideoH264SpsFlags,					//  StdVideoH264SpsFlags						flags;
		STD_VIDEO_H264_PROFILE_IDC_BASELINE,	//  StdVideoH264ProfileIdc						profile_idc;
		STD_VIDEO_H264_LEVEL_IDC_4_1,			//  StdVideoH264Level							level_idc;
		STD_VIDEO_H264_CHROMA_FORMAT_IDC_420,	//  StdVideoH264ChromaFormatIdc					chroma_format_idc;
		0u,										//  deUint8										seq_parameter_set_id;
		0u,										//  deUint8										bit_depth_luma_minus8;
		0u,										//  deUint8										bit_depth_chroma_minus8;
		0u,										//  deUint8										log2_max_frame_num_minus4;
		STD_VIDEO_H264_POC_TYPE_2,				//  StdVideoH264PocType							pic_order_cnt_type;
		0,										//  int32_t										offset_for_non_ref_pic;
		0,										//  int32_t										offset_for_top_to_bottom_field;
		0u,										//  deUint8										log2_max_pic_order_cnt_lsb_minus4;
		0u,										//  deUint8										num_ref_frames_in_pic_order_cnt_cycle;
		3u,										//  deUint8										max_num_ref_frames;
		0u,										//  deUint8										reserved1;
		(width + 15) / 16 - 1,					//  deUint32									pic_width_in_mbs_minus1;
		(height + 15) / 16 - 1,					//  deUint32									pic_height_in_map_units_minus1;
		0u,										//  deUint32									frame_crop_left_offset;
		0u,										//  deUint32									frame_crop_right_offset;
		0u,										//  deUint32									frame_crop_top_offset;
		0u,										//  deUint32									frame_crop_bottom_offset;
		0u,										//  deUint32									reserved2;
		DE_NULL,								//  const int32_t*								pOffsetForRefFrame;
		DE_NULL,								//  const StdVideoH264ScalingLists*				pScalingLists;
		stdVideoH264SequenceParameterSetVui,	//  const StdVideoH264SequenceParameterSetVui*	pSequenceParameterSetVui;
	};

	return de::MovePtr<StdVideoH264SequenceParameterSet>(new StdVideoH264SequenceParameterSet(stdVideoH264SequenceParameterSet));
}

de::MovePtr<StdVideoH264PictureParameterSet> getStdVideoH264PictureParameterSet (void)
{
	const StdVideoH264PpsFlags				stdVideoH264PpsFlags			=
	{
		1u,		//  deUint32	transform_8x8_mode_flag:1;
		0u,		//  deUint32	redundant_pic_cnt_present_flag:1;
		0u,		//  deUint32	constrained_intra_pred_flag:1;
		1u,		//  deUint32	deblocking_filter_control_present_flag:1;
		0u,		//  deUint32	weighted_pred_flag:1;
		0u,		//  uint32_4	bottom_field_pic_order_in_frame_present_flag:1;
		1u,		//  deUint32	entropy_coding_mode_flag:1;
		0u,		//  deUint32	pic_scaling_matrix_present_flag;
	};

	const StdVideoH264PictureParameterSet	stdVideoH264PictureParameterSet	=
	{
		stdVideoH264PpsFlags,						//  StdVideoH264PpsFlags			flags;
		0u,											//  deUint8							seq_parameter_set_id;
		0u,											//  deUint8							pic_parameter_set_id;
		2u,											//  deUint8							num_ref_idx_l0_default_active_minus1;
		0u,											//  deUint8							num_ref_idx_l1_default_active_minus1;
		STD_VIDEO_H264_WEIGHTED_BIPRED_IDC_DEFAULT,	//  StdVideoH264WeightedBipredIdc	weighted_bipred_idc;
		-16,										//  int8_t							pic_init_qp_minus26;
		0,											//  int8_t							pic_init_qs_minus26;
		-2,											//  int8_t							chroma_qp_index_offset;
		-2,											//  int8_t							second_chroma_qp_index_offset;
		DE_NULL,									//  const StdVideoH264ScalingLists*	pScalingLists;
	};

	return de::MovePtr<StdVideoH264PictureParameterSet>(new StdVideoH264PictureParameterSet(stdVideoH264PictureParameterSet));
}

std::vector<deUint8> semiplanarToYV12(const ycbcr::MultiPlaneImageData& multiPlaneImageData)
{
	DE_ASSERT(multiPlaneImageData.getFormat() == VK_FORMAT_G8_B8R8_2PLANE_420_UNORM);

	std::vector<deUint8> YV12Buffer;
	size_t plane0Size = multiPlaneImageData.getPlaneSize(0);
	size_t plane1Size = multiPlaneImageData.getPlaneSize(1);

	YV12Buffer.resize(plane0Size + plane1Size);

	// Copy the luma plane.
	deMemcpy(YV12Buffer.data(), multiPlaneImageData.getPlanePtr(0), plane0Size);

	// Deinterleave the Cr and Cb plane.
	deUint16 *plane2 = (deUint16*)multiPlaneImageData.getPlanePtr(1);
	std::vector<deUint8>::size_type idx = plane0Size;
	for (unsigned i = 0 ; i < plane1Size / 2; i ++)
		YV12Buffer[idx++] = static_cast<deUint8>(plane2[i] & 0xFF);
	for (unsigned i = 0 ; i < plane1Size / 2; i ++)
		YV12Buffer[idx++] = static_cast<deUint8>((plane2[i] >> 8) & 0xFF);

	return YV12Buffer;
}

bool imageMatchesReferenceChecksum(const ycbcr::MultiPlaneImageData& multiPlaneImageData, const std::string& referenceChecksum)
{
	std::vector<deUint8> yv12 = semiplanarToYV12(multiPlaneImageData);
	std::string checksum = MD5SumBase16(yv12.data(), yv12.size());
	return checksum == referenceChecksum;
}


namespace util {
const char* getVideoCodecString(VkVideoCodecOperationFlagBitsKHR codec)
{
	static struct {
		VkVideoCodecOperationFlagBitsKHR eCodec;
		const char* name;
	} aCodecName[] = {
			{ VK_VIDEO_CODEC_OPERATION_NONE_KHR, "None" },
			{ VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR, "AVC/H.264" },
			{ VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR, "H.265/HEVC" },
	};

	for (auto& i : aCodecName) {
		if (codec == i.eCodec)
			return aCodecName[codec].name;
	}

	return "Unknown";
}

const char* getVideoChromaFormatString(VkVideoChromaSubsamplingFlagBitsKHR chromaFormat)
{
	switch (chromaFormat) {
		case VK_VIDEO_CHROMA_SUBSAMPLING_MONOCHROME_BIT_KHR:
			return "YCbCr 400 (Monochrome)";
		case VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR:
			return "YCbCr 420";
		case VK_VIDEO_CHROMA_SUBSAMPLING_422_BIT_KHR:
			return "YCbCr 422";
		case VK_VIDEO_CHROMA_SUBSAMPLING_444_BIT_KHR:
			return "YCbCr 444";
		default:
			DE_ASSERT(false && "Unknown Chroma sub-sampled format");
	};

	return "Unknown";
}

VkVideoCodecOperationFlagsKHR getSupportedCodecs(DeviceContext& devCtx,
														deUint32 selectedVideoQueueFamily,
														VkQueueFlags queueFlagsRequired ,
														VkVideoCodecOperationFlagsKHR videoCodeOperations)
{
	deUint32 count = 0;
	auto& vkif = devCtx.context->getInstanceInterface();
	vkif.getPhysicalDeviceQueueFamilyProperties2(devCtx.phys, &count, nullptr);
	std::vector<VkQueueFamilyProperties2> queues(count);
	std::vector<VkQueueFamilyVideoPropertiesKHR> videoQueues(count);
	std::vector<VkQueueFamilyQueryResultStatusPropertiesKHR> queryResultStatus(count);
	for (std::vector<VkQueueFamilyProperties2>::size_type i = 0; i < queues.size(); i++)
	{
		queues[i].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
		videoQueues[i].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_VIDEO_PROPERTIES_KHR;
		queues[i].pNext = &videoQueues[i];
		queryResultStatus[i].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_QUERY_RESULT_STATUS_PROPERTIES_KHR;
		videoQueues[i].pNext = &queryResultStatus[i];
	}
	vkif.getPhysicalDeviceQueueFamilyProperties2(devCtx.phys, &count, queues.data());


	TCU_CHECK(selectedVideoQueueFamily < queues.size());

	const VkQueueFamilyProperties2 &q = queues[selectedVideoQueueFamily];
	const VkQueueFamilyVideoPropertiesKHR &videoQueue = videoQueues[selectedVideoQueueFamily];

	if (q.queueFamilyProperties.queueFlags & queueFlagsRequired && videoQueue.videoCodecOperations & videoCodeOperations) {
		// The video queues may or may not support queryResultStatus
		// DE_ASSERT(queryResultStatus[queueIndx].queryResultStatusSupport);
		return videoQueue.videoCodecOperations;
	}

	return VK_VIDEO_CODEC_OPERATION_NONE_KHR;
}

VkResult getVideoFormats(DeviceContext& devCtx,
								const VkVideoCoreProfile& videoProfile, VkImageUsageFlags imageUsage,
								deUint32& formatCount, VkFormat* formats,
								bool dumpData)
{
	auto& vkif = devCtx.context->getInstanceInterface();

	for (deUint32 i = 0; i < formatCount; i++) {
		formats[i] = VK_FORMAT_UNDEFINED;
	}

	const VkVideoProfileListInfoKHR videoProfiles = { VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR, nullptr, 1, videoProfile.GetProfile() };
	const VkPhysicalDeviceVideoFormatInfoKHR videoFormatInfo = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_FORMAT_INFO_KHR, const_cast<VkVideoProfileListInfoKHR *>(&videoProfiles),
																 imageUsage };

	deUint32 supportedFormatCount = 0;
	VkResult result = vkif.getPhysicalDeviceVideoFormatPropertiesKHR(devCtx.phys, &videoFormatInfo, &supportedFormatCount, nullptr);
	DE_ASSERT(result == VK_SUCCESS);
	DE_ASSERT(supportedFormatCount);

	VkVideoFormatPropertiesKHR* pSupportedFormats = new VkVideoFormatPropertiesKHR[supportedFormatCount];
	memset(pSupportedFormats, 0x00, supportedFormatCount * sizeof(VkVideoFormatPropertiesKHR));
	for (deUint32 i = 0; i < supportedFormatCount; i++) {
		pSupportedFormats[i].sType = VK_STRUCTURE_TYPE_VIDEO_FORMAT_PROPERTIES_KHR;
	}

	result = vkif.getPhysicalDeviceVideoFormatPropertiesKHR(devCtx.phys, &videoFormatInfo, &supportedFormatCount, pSupportedFormats);
	DE_ASSERT(result == VK_SUCCESS);
	if (dumpData) {
		std::cout << "\t\t\t" << ((videoProfile.GetCodecType() == VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR) ? "h264" : "h265") << "decode formats: " << std::endl;
		for (deUint32 fmt = 0; fmt < supportedFormatCount; fmt++) {
			std::cout << "\t\t\t " << fmt << ": " << std::hex << pSupportedFormats[fmt].format << std::dec << std::endl;
		}
	}

	formatCount = std::min(supportedFormatCount, formatCount);

	for (deUint32 i = 0; i < formatCount; i++) {
		formats[i] = pSupportedFormats[i].format;
	}

	delete[] pSupportedFormats;

	return result;
}

VkResult getSupportedVideoFormats(DeviceContext& devCtx,
										 const VkVideoCoreProfile& videoProfile,
										 VkVideoDecodeCapabilityFlagsKHR capabilityFlags,
										 VkFormat& pictureFormat,
										 VkFormat& referencePicturesFormat)
{
	VkResult result = VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR;
	if ((capabilityFlags & VK_VIDEO_DECODE_CAPABILITY_DPB_AND_OUTPUT_COINCIDE_BIT_KHR) != 0) {
		// NV, Intel
		VkFormat supportedDpbFormats[8];
		deUint32 formatCount = sizeof(supportedDpbFormats) / sizeof(supportedDpbFormats[0]);
		result = util::getVideoFormats(devCtx, videoProfile,
									   (VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR | VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR | VK_IMAGE_USAGE_TRANSFER_SRC_BIT),
									   formatCount, supportedDpbFormats);

		referencePicturesFormat = supportedDpbFormats[0];
		pictureFormat = supportedDpbFormats[0];

	} else if ((capabilityFlags & VK_VIDEO_DECODE_CAPABILITY_DPB_AND_OUTPUT_DISTINCT_BIT_KHR) != 0) {
		// AMD
		VkFormat supportedDpbFormats[8];
		VkFormat supportedOutFormats[8];
		deUint32 formatCount = sizeof(supportedDpbFormats) / sizeof(supportedDpbFormats[0]);
		result = util::getVideoFormats(devCtx, videoProfile,
									   VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR,
									   formatCount, supportedDpbFormats);

		DE_ASSERT(result == VK_SUCCESS);

		result = util::getVideoFormats(devCtx, videoProfile,
									   VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
									   formatCount, supportedOutFormats);

		referencePicturesFormat = supportedDpbFormats[0];
		pictureFormat = supportedOutFormats[0];

	} else {
		fprintf(stderr, "\nERROR: Unsupported decode capability flags.");
		return VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR;
	}

	DE_ASSERT(result == VK_SUCCESS);
	if (result != VK_SUCCESS) {
		fprintf(stderr, "\nERROR: GetVideoFormats() result: 0x%x\n", result);
	}

	DE_ASSERT((referencePicturesFormat != VK_FORMAT_UNDEFINED) && (pictureFormat != VK_FORMAT_UNDEFINED));
	DE_ASSERT(referencePicturesFormat == pictureFormat);

	return result;
}

const char* codecToName(VkVideoCodecOperationFlagBitsKHR codec)
{
	switch ((int32_t)codec) {
		case VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR:
			return "decode h.264";
		case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR:
			return "decode h.265";
		case VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_EXT:
			return "encode h.264";
		case VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_EXT:
			return "encode h.265";
		default:
			tcu::die("Unknown video codec");
	}

	return "";
}

VkResult getVideoCapabilities(DeviceContext& devCtx,
							  const VkVideoCoreProfile& videoProfile,
							  VkVideoCapabilitiesKHR* pVideoCapabilities)
{
	auto& vkif = devCtx.context->getInstanceInterface();
	DE_ASSERT(pVideoCapabilities->sType == VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR);
	VkVideoDecodeCapabilitiesKHR* pVideoDecodeCapabilities = (VkVideoDecodeCapabilitiesKHR*)pVideoCapabilities->pNext;
	DE_ASSERT(pVideoDecodeCapabilities->sType == VK_STRUCTURE_TYPE_VIDEO_DECODE_CAPABILITIES_KHR);
	VkVideoDecodeH264CapabilitiesKHR* pH264Capabilities = nullptr;
	VkVideoDecodeH265CapabilitiesKHR* pH265Capabilities = nullptr;

	if (videoProfile.GetCodecType() == VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR) {
		DE_ASSERT(pVideoDecodeCapabilities->pNext);
		pH264Capabilities = (VkVideoDecodeH264CapabilitiesKHR*)pVideoDecodeCapabilities->pNext;
		DE_ASSERT(pH264Capabilities->sType == VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_CAPABILITIES_KHR);
	} else if (videoProfile.GetCodecType() == VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR) {
		DE_ASSERT(pVideoDecodeCapabilities->pNext);
		pH265Capabilities = (VkVideoDecodeH265CapabilitiesKHR*)pVideoDecodeCapabilities->pNext;
		DE_ASSERT(pH265Capabilities->sType ==  VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_CAPABILITIES_KHR);
	} else {
		DE_ASSERT(false && "Unsupported codec");
		return VK_ERROR_FORMAT_NOT_SUPPORTED;
	}
	VkResult result = vkif.getPhysicalDeviceVideoCapabilitiesKHR(devCtx.phys,
																 videoProfile.GetProfile(),
																 pVideoCapabilities);
	DE_ASSERT(result == VK_SUCCESS);
	if (result != VK_SUCCESS) {
		return result;
	}

	if (videoLoggingEnabled()) {
		std::cout << "\t\t\t" << ((videoProfile.GetCodecType() == VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR) ? "h264" : "h265") << " decode capabilities: " << std::endl;

		if (pVideoCapabilities->flags & VK_VIDEO_CAPABILITY_SEPARATE_REFERENCE_IMAGES_BIT_KHR) {
			std::cout << "\t\t\t" << "Use separate reference images" << std::endl;
		}

		std::cout << "\t\t\t" << "minBitstreamBufferOffsetAlignment: " << pVideoCapabilities->minBitstreamBufferOffsetAlignment << std::endl;
		std::cout << "\t\t\t" << "minBitstreamBufferSizeAlignment: " << pVideoCapabilities->minBitstreamBufferSizeAlignment << std::endl;
		std::cout << "\t\t\t" << "pictureAccessGranularity: " << pVideoCapabilities->pictureAccessGranularity.width << " x " << pVideoCapabilities->pictureAccessGranularity.height << std::endl;
		std::cout << "\t\t\t" << "minCodedExtent: " << pVideoCapabilities->minCodedExtent.width << " x " << pVideoCapabilities->minCodedExtent.height << std::endl;
		std::cout << "\t\t\t" << "maxCodedExtent: " << pVideoCapabilities->maxCodedExtent.width  << " x " << pVideoCapabilities->maxCodedExtent.height << std::endl;
		std::cout << "\t\t\t" << "maxDpbSlots: " << pVideoCapabilities->maxDpbSlots << std::endl;
		std::cout << "\t\t\t" << "maxActiveReferencePictures: " << pVideoCapabilities->maxActiveReferencePictures << std::endl;

		if (videoProfile.GetCodecType() == VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR) {
			std::cout << "\t\t\t" << "maxLevelIdc: " << pH264Capabilities->maxLevelIdc << std::endl;
			std::cout << "\t\t\t" << "fieldOffsetGranularity: " << pH264Capabilities->fieldOffsetGranularity.x << " x " << pH264Capabilities->fieldOffsetGranularity.y << std::endl;;

			if (strncmp(pVideoCapabilities->stdHeaderVersion.extensionName,
						VK_STD_VULKAN_VIDEO_CODEC_H264_DECODE_EXTENSION_NAME,
						sizeof (pVideoCapabilities->stdHeaderVersion.extensionName) - 1U) ||
				(pVideoCapabilities->stdHeaderVersion.specVersion != VK_STD_VULKAN_VIDEO_CODEC_H264_DECODE_SPEC_VERSION)) {
				DE_ASSERT(false && "Unsupported h.264 STD version");
				return VK_ERROR_INCOMPATIBLE_DRIVER;
			}
		} else if (videoProfile.GetCodecType() == VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR) {
			std::cout << "\t\t\t" << "maxLevelIdc: " << pH265Capabilities->maxLevelIdc << std::endl;
			if (strncmp(pVideoCapabilities->stdHeaderVersion.extensionName,
						VK_STD_VULKAN_VIDEO_CODEC_H265_DECODE_EXTENSION_NAME,
						sizeof (pVideoCapabilities->stdHeaderVersion.extensionName) - 1U) ||
				(pVideoCapabilities->stdHeaderVersion.specVersion != VK_STD_VULKAN_VIDEO_CODEC_H265_DECODE_SPEC_VERSION)) {
				DE_ASSERT(false && "Unsupported h.265 STD version");
				return VK_ERROR_INCOMPATIBLE_DRIVER;
			}
		} else {
			DE_ASSERT(false && "Unsupported codec");
		}
	}

	return result;
}

VkResult getVideoDecodeCapabilities(DeviceContext& devCtx,
									const VkVideoCoreProfile& videoProfile,
									VkVideoCapabilitiesKHR& videoCapabilities,
									VkVideoDecodeCapabilitiesKHR& videoDecodeCapabilities) {

	VkVideoCodecOperationFlagsKHR videoCodec = videoProfile.GetProfile()->videoCodecOperation;

	videoDecodeCapabilities = VkVideoDecodeCapabilitiesKHR { VK_STRUCTURE_TYPE_VIDEO_DECODE_CAPABILITIES_KHR, nullptr, 0 };

	deMemset(&videoCapabilities, 0, sizeof(VkVideoCapabilitiesKHR));
	videoCapabilities.sType = VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR;
	videoCapabilities.pNext = &videoDecodeCapabilities;

	VkVideoDecodeH264CapabilitiesKHR h264Capabilities{};
	h264Capabilities.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_CAPABILITIES_KHR;

	VkVideoDecodeH265CapabilitiesKHR h265Capabilities{};
	h265Capabilities.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_CAPABILITIES_KHR;

	if (videoCodec == VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR) {
		videoDecodeCapabilities.pNext = &h264Capabilities;
	} else if (videoCodec == VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR) {
		videoDecodeCapabilities.pNext = &h265Capabilities;
	} else {
		DE_ASSERT(false && "Unsupported codec");
		return VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR;
	}
	VkResult result = util::getVideoCapabilities(devCtx, videoProfile, &videoCapabilities);
	DE_ASSERT(result == VK_SUCCESS);
	if (result != VK_SUCCESS) {
		fprintf(stderr, "\nERROR: Input is not supported. GetVideoCapabilities() result: 0x%x\n", result);
	}
	return result;
}
} //util

} // video
} // vkt
