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
 *	  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Video Encoding and Decoding Capabilities tests
 *//*--------------------------------------------------------------------*/

#include "vktVideoCapabilitiesTests.hpp"
#include "vktVideoTestUtils.hpp"

#include "vkDefs.hpp"
#include "vkTypeUtil.hpp"


#include "vktTestCase.hpp"
#include "vktCustomInstancesDevices.hpp"


namespace vkt
{
namespace video
{
namespace
{
using namespace vk;
using namespace std;

enum TestType
{
	TEST_TYPE_QUEUE_SUPPORT_QUERY,							// Test case 1
	TEST_TYPE_H264_DECODE_DST_VIDEO_FORMAT_SUPPORT_QUERY,	// Test case 2 iteration 1 ?
	TEST_TYPE_H264_DECODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY,	// Test case 2 iteration 2 ?
	TEST_TYPE_H264_ENCODE_SRC_VIDEO_FORMAT_SUPPORT_QUERY,	// Test case 3 iteration 1
	TEST_TYPE_H264_ENCODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY,	// Test case 3 iteration 2
	TEST_TYPE_H265_DECODE_DST_VIDEO_FORMAT_SUPPORT_QUERY,	// Test case 4a iteration 1 ?
	TEST_TYPE_H265_DECODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY,	// Test case 4a iteration 2 ?
	TEST_TYPE_H265_ENCODE_SRC_VIDEO_FORMAT_SUPPORT_QUERY,	// Test case 4b iteration 1
	TEST_TYPE_H265_ENCODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY,	// Test case 4b iteration 2
	TEST_TYPE_H264_DECODE_CAPABILITIES_QUERY,				// Test case 5a
	TEST_TYPE_H264_ENCODE_CAPABILITIES_QUERY,				// Test case 5b
	TEST_TYPE_H265_DECODE_CAPABILITIES_QUERY,				// Test case 5c
	TEST_TYPE_H265_ENCODE_CAPABILITIES_QUERY,				// Test case 5d
	TEST_TYPE_LAST
};

struct CaseDef
{
	TestType	testType;
};

#define VALIDATE_FIELD_EQUAL(A,B,X) if (deMemCmp(&A.X, &B.X, sizeof(A.X)) != 0) TCU_FAIL("Unequal " #A "." #X)

class VideoQueueQueryTestInstance : public VideoBaseTestInstance
{
public:
					VideoQueueQueryTestInstance		(Context& context, const CaseDef& data);
					~VideoQueueQueryTestInstance	(void);

	tcu::TestStatus	iterate							(void);

private:
	CaseDef			m_caseDef;
};

VideoQueueQueryTestInstance::VideoQueueQueryTestInstance (Context& context, const CaseDef& data)
	: VideoBaseTestInstance	(context)
	, m_caseDef				(data)
{
    DE_UNREF(m_caseDef);
}

VideoQueueQueryTestInstance::~VideoQueueQueryTestInstance (void)
{
}

tcu::TestStatus VideoQueueQueryTestInstance::iterate (void)
{
	const InstanceInterface&					vk								= m_context.getInstanceInterface();
	const VkPhysicalDevice						physicalDevice					= m_context.getPhysicalDevice();
	deUint32									queueFamilyPropertiesCount		= 0u;
	vector<VkQueueFamilyProperties2>			queueFamilyProperties2;
	vector<VkQueueFamilyVideoPropertiesKHR>		videoQueueFamilyProperties2;
	bool										encodePass = false;
	bool										decodePass = false;

	vk.getPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyPropertiesCount, DE_NULL);

	if(queueFamilyPropertiesCount == 0u)
		TCU_FAIL("Device reports an empty set of queue family properties");

	queueFamilyProperties2.resize(queueFamilyPropertiesCount);
	videoQueueFamilyProperties2.resize(queueFamilyPropertiesCount);

	for (size_t ndx = 0; ndx < queueFamilyPropertiesCount; ++ndx)
	{
		queueFamilyProperties2[ndx].sType							= VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
		queueFamilyProperties2[ndx].pNext							= &videoQueueFamilyProperties2[ndx];
		videoQueueFamilyProperties2[ndx].sType						= VK_STRUCTURE_TYPE_QUEUE_FAMILY_VIDEO_PROPERTIES_KHR;
		videoQueueFamilyProperties2[ndx].pNext						= DE_NULL;
		videoQueueFamilyProperties2[ndx].videoCodecOperations		= 0;
	}

	vk.getPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyPropertiesCount, queueFamilyProperties2.data());

	if (queueFamilyPropertiesCount != queueFamilyProperties2.size())
		TCU_FAIL("Device returns less queue families than initially reported");

	for (uint32_t ndx = 0; ndx < queueFamilyPropertiesCount; ++ndx)
	{
		const uint32_t						queueCount					= queueFamilyProperties2[ndx].queueFamilyProperties.queueCount;
		const VkQueueFlags					queueFlags					= queueFamilyProperties2[ndx].queueFamilyProperties.queueFlags;
		const VkVideoCodecOperationFlagsKHR	queueVideoCodecOperations	= videoQueueFamilyProperties2[ndx].videoCodecOperations;

		if ((queueFlags & VK_QUEUE_VIDEO_ENCODE_BIT_KHR) != 0)
		{
			if (!VideoDevice::isVideoEncodeOperation(queueVideoCodecOperations))
				TCU_FAIL("Invalid codec operations for encode queue");

			if (queueCount == 0)
				TCU_FAIL("Video encode queue returned queueCount is zero");

			encodePass = true;
		}

		if ((queueFlags & VK_QUEUE_VIDEO_DECODE_BIT_KHR) != 0)
		{
			if (!VideoDevice::isVideoDecodeOperation(queueVideoCodecOperations))
				TCU_FAIL("Invalid codec operations for decode queue");

			if (queueCount == 0)
				TCU_FAIL("Video decode queue returned queueCount is zero");

			decodePass = true;
		}
	}

	if (!m_context.isDeviceFunctionalitySupported("VK_KHR_video_encode_queue"))
		encodePass = false;

	if (!m_context.isDeviceFunctionalitySupported("VK_KHR_video_decode_queue"))
		decodePass = false;

	if (encodePass || decodePass)
		return tcu::TestStatus::pass("Pass");
	else
		return tcu::TestStatus::fail("Neither encode, nor decode is available");
}

template<typename ProfileOperation>
class VideoFormatPropertiesQueryTestInstance : public VideoBaseTestInstance
{
public:
									VideoFormatPropertiesQueryTestInstance	(Context& context, const CaseDef& data);
									~VideoFormatPropertiesQueryTestInstance	(void);
	tcu::TestStatus					iterate									(void);

private:
	ProfileOperation				getProfileOperation						(void);

	CaseDef							m_caseDef;
	VkVideoCodecOperationFlagsKHR	m_videoCodecOperation;
	VkImageUsageFlags				m_imageUsageFlags;
};

template<typename ProfileOperation>
VideoFormatPropertiesQueryTestInstance<ProfileOperation>::VideoFormatPropertiesQueryTestInstance (Context& context, const CaseDef& data)
	: VideoBaseTestInstance	(context)
	, m_caseDef				(data)
{
	switch (m_caseDef.testType)
	{
		case TEST_TYPE_H264_DECODE_DST_VIDEO_FORMAT_SUPPORT_QUERY:
		case TEST_TYPE_H264_DECODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:	m_videoCodecOperation = VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR; break;
		case TEST_TYPE_H264_ENCODE_SRC_VIDEO_FORMAT_SUPPORT_QUERY:
		case TEST_TYPE_H264_ENCODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:	m_videoCodecOperation = VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_EXT; break;
		case TEST_TYPE_H265_DECODE_DST_VIDEO_FORMAT_SUPPORT_QUERY:
		case TEST_TYPE_H265_DECODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:	m_videoCodecOperation = VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR; break;
		case TEST_TYPE_H265_ENCODE_SRC_VIDEO_FORMAT_SUPPORT_QUERY:
		case TEST_TYPE_H265_ENCODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:	m_videoCodecOperation = VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_EXT; break;
		default: TCU_THROW(InternalError, "Unknown testType");
	}

	switch (m_caseDef.testType)
	{
		case TEST_TYPE_H264_DECODE_DST_VIDEO_FORMAT_SUPPORT_QUERY:	m_imageUsageFlags = VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR; break;
		case TEST_TYPE_H264_DECODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:	m_imageUsageFlags = VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR; break;
		case TEST_TYPE_H264_ENCODE_SRC_VIDEO_FORMAT_SUPPORT_QUERY:	m_imageUsageFlags = VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR; break;
		case TEST_TYPE_H264_ENCODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:	m_imageUsageFlags = VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR; break;
		case TEST_TYPE_H265_DECODE_DST_VIDEO_FORMAT_SUPPORT_QUERY:	m_imageUsageFlags = VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR; break;
		case TEST_TYPE_H265_DECODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:	m_imageUsageFlags = VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR; break;
		case TEST_TYPE_H265_ENCODE_SRC_VIDEO_FORMAT_SUPPORT_QUERY:	m_imageUsageFlags = VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR; break;
		case TEST_TYPE_H265_ENCODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:	m_imageUsageFlags = VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR; break;
		default: TCU_THROW(InternalError, "Unknown testType");
	}
}

template<typename ProfileOperation>
VideoFormatPropertiesQueryTestInstance<ProfileOperation>::~VideoFormatPropertiesQueryTestInstance (void)
{
}

template<>
VkVideoDecodeH264ProfileInfoKHR VideoFormatPropertiesQueryTestInstance<VkVideoDecodeH264ProfileInfoKHR>::getProfileOperation (void)
{
	return getProfileOperationH264Decode();
}

template<>
VkVideoEncodeH264ProfileInfoEXT VideoFormatPropertiesQueryTestInstance<VkVideoEncodeH264ProfileInfoEXT>::getProfileOperation ()
{
	return getProfileOperationH264Encode();
}

template<>
VkVideoDecodeH265ProfileInfoKHR VideoFormatPropertiesQueryTestInstance<VkVideoDecodeH265ProfileInfoKHR>::getProfileOperation ()
{
	return getProfileOperationH265Decode();
}

template<>
VkVideoEncodeH265ProfileInfoEXT VideoFormatPropertiesQueryTestInstance<VkVideoEncodeH265ProfileInfoEXT>::getProfileOperation ()
{
	return getProfileOperationH265Encode();
}

template<typename ProfileOperation>
tcu::TestStatus VideoFormatPropertiesQueryTestInstance<ProfileOperation>::iterate (void)
{
	const InstanceInterface&					vk							= m_context.getInstanceInterface();
	const VkPhysicalDevice						physicalDevice				= m_context.getPhysicalDevice();
	deUint32									videoFormatPropertiesCount	= 0u;
	bool										testResult					= false;

	const ProfileOperation						videoProfileOperation		= getProfileOperation();
	const VkVideoCodecOperationFlagBitsKHR		videoCodecOperation			= static_cast<VkVideoCodecOperationFlagBitsKHR>(m_videoCodecOperation);
	const VkVideoProfileInfoKHR						videoProfile				=
	{
		VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR,			//  VkStructureType						sType;
		(void*)&videoProfileOperation,						//  void*								pNext;
		videoCodecOperation,								//  VkVideoCodecOperationFlagBitsKHR	videoCodecOperation;
		VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR,			//  VkVideoChromaSubsamplingFlagsKHR	chromaSubsampling;
		VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,				//  VkVideoComponentBitDepthFlagsKHR	lumaBitDepth;
		VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,				//  VkVideoComponentBitDepthFlagsKHR	chromaBitDepth;
	};
	const VkVideoProfileListInfoKHR					videoProfiles				=
	{
		VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR,	//  VkStructureType				sType;
		DE_NULL,										//  void*						pNext;
		1u,												//  deUint32					profilesCount;
		&videoProfile,									//  const VkVideoProfileInfoKHR*	pProfiles;
	};

	const VkPhysicalDeviceVideoFormatInfoKHR	videoFormatInfo				=
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_FORMAT_INFO_KHR,	//  VkStructureType				sType;
		const_cast<VkVideoProfileListInfoKHR *>(&videoProfiles),													//  const void*					pNext;
		m_imageUsageFlags,											//  VkImageUsageFlags			imageUsage;
	};
	const VkImageUsageFlags						imageUsageFlagsDPB			= VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR | VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR;
	const bool									imageUsageDPB				= (videoFormatInfo.imageUsage & imageUsageFlagsDPB) != 0;

	{
		const VkResult result = vk.getPhysicalDeviceVideoFormatPropertiesKHR(physicalDevice, &videoFormatInfo, &videoFormatPropertiesCount, DE_NULL);

		if (result != VK_SUCCESS)
		{
			ostringstream failMsg;

			failMsg << "Failed query call to vkGetPhysicalDeviceVideoFormatPropertiesKHR with " << result;

			return tcu::TestStatus::fail(failMsg.str());
		}

		if (videoFormatPropertiesCount == 0)
			return tcu::TestStatus::fail("vkGetPhysicalDeviceVideoFormatPropertiesKHR reports 0 formats");
	}

	{
		const VkVideoFormatPropertiesKHR		videoFormatPropertiesKHR	=
		{
			VK_STRUCTURE_TYPE_VIDEO_FORMAT_PROPERTIES_KHR,	//  VkStructureType		sType;
			DE_NULL,										//  void*				pNext;
			VK_FORMAT_MAX_ENUM,								//  VkFormat			format;
			vk::makeComponentMappingIdentity(),				//  VkComponentMapping	componentMapping;
			(VkImageCreateFlags)0u,							//  VkImageCreateFlags	imageCreateFlags;
			VK_IMAGE_TYPE_2D,								//  VkImageType			imageType;
			VK_IMAGE_TILING_OPTIMAL,						//  VkImageTiling		imageTiling;
			VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR
			| VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR,		//  VkImageUsageFlags	imageUsageFlags;
		};
		std::vector<VkVideoFormatPropertiesKHR>	videoFormatProperties		(videoFormatPropertiesCount, videoFormatPropertiesKHR);

		const VkResult result = vk.getPhysicalDeviceVideoFormatPropertiesKHR(physicalDevice, &videoFormatInfo, &videoFormatPropertiesCount, videoFormatProperties.data());

		if (result != VK_SUCCESS)
		{
			ostringstream failMsg;

			failMsg << "Failed query data call to vkGetPhysicalDeviceVideoFormatPropertiesKHR with " << result;

			return tcu::TestStatus::fail(failMsg.str());
		}

		if (videoFormatPropertiesCount == 0)
			return tcu::TestStatus::fail("vkGetPhysicalDeviceVideoFormatPropertiesKHR reports 0 formats supported for chosen encding/decoding");

		if (videoFormatPropertiesCount != videoFormatProperties.size())
			return tcu::TestStatus::fail("Number of formats returned is less than reported.");

		for (const auto& videoFormatProperty: videoFormatProperties)
		{
			if (videoFormatProperty.format == VK_FORMAT_MAX_ENUM)
				return tcu::TestStatus::fail("Format is not written");

			if (videoFormatProperty.format == VK_FORMAT_UNDEFINED)
			{
				if (!imageUsageDPB)
					TCU_FAIL("VK_FORMAT_UNDEFINED is allowed only for DPB image usage");

				if (videoFormatProperties.size() != 1)
					TCU_FAIL("VK_FORMAT_UNDEFINED must be the only format returned for opaque DPB");

				testResult = true;

				break;
			}

			if (videoFormatProperty.format == VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM ||
				videoFormatProperty.format == VK_FORMAT_G8_B8R8_2PLANE_420_UNORM)
			{
				testResult = true;

				break;
			}
		}
	}

	if (testResult)
		return tcu::TestStatus::pass("Pass");
	else
		return tcu::TestStatus::fail("Fail");
}

typedef VideoFormatPropertiesQueryTestInstance<VkVideoDecodeH264ProfileInfoKHR> VideoFormatPropertiesQueryH264DecodeTestInstance;
typedef VideoFormatPropertiesQueryTestInstance<VkVideoEncodeH264ProfileInfoEXT> VideoFormatPropertiesQueryH264EncodeTestInstance;
typedef VideoFormatPropertiesQueryTestInstance<VkVideoDecodeH265ProfileInfoKHR> VideoFormatPropertiesQueryH265DecodeTestInstance;
typedef VideoFormatPropertiesQueryTestInstance<VkVideoEncodeH265ProfileInfoEXT> VideoFormatPropertiesQueryH265EncodeTestInstance;

class VideoCapabilitiesQueryTestInstance : public VideoBaseTestInstance
{
public:
					VideoCapabilitiesQueryTestInstance	(Context& context, const CaseDef& data);
					~VideoCapabilitiesQueryTestInstance	(void);

protected:
	void			validateVideoCapabilities			(const VkVideoCapabilitiesKHR&			videoCapabilitiesKHR,
														 const VkVideoCapabilitiesKHR&			videoCapabilitiesKHRSecond);
	void			validateVideoDecodeCapabilities		(const VkVideoDecodeCapabilitiesKHR&	videoDecodeCapabilitiesKHR,
														 const VkVideoDecodeCapabilitiesKHR&	videoDecodeCapabilitiesKHRSecond);
	void			validateVideoEncodeCapabilities		(const VkVideoEncodeCapabilitiesKHR&	videoEncodeCapabilitiesKHR,
														 const VkVideoEncodeCapabilitiesKHR&	videoEncodeCapabilitiesKHRSecond);
	void			validateExtensionProperties			(const VkExtensionProperties&			extensionProperties,
														 const VkExtensionProperties&			extensionPropertiesSecond);
	CaseDef			m_caseDef;
};

VideoCapabilitiesQueryTestInstance::VideoCapabilitiesQueryTestInstance (Context& context, const CaseDef& data)
	: VideoBaseTestInstance	(context)
	, m_caseDef				(data)
{
	DE_UNREF(m_caseDef);
}

VideoCapabilitiesQueryTestInstance::~VideoCapabilitiesQueryTestInstance (void)
{
}

void VideoCapabilitiesQueryTestInstance::validateVideoCapabilities (const VkVideoCapabilitiesKHR&	videoCapabilitiesKHR,
																	const VkVideoCapabilitiesKHR&	videoCapabilitiesKHRSecond)
{
	VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, sType);
	VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, flags);
	VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, minBitstreamBufferOffsetAlignment);
	VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, minBitstreamBufferSizeAlignment);
	VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, pictureAccessGranularity);
	VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, minCodedExtent);
	VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxCodedExtent);
	VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxDpbSlots);
	VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxActiveReferencePictures);
	validateExtensionProperties(videoCapabilitiesKHR.stdHeaderVersion, videoCapabilitiesKHRSecond.stdHeaderVersion);

	const VkVideoCapabilityFlagsKHR videoCapabilityFlagsKHR	= VK_VIDEO_CAPABILITY_PROTECTED_CONTENT_BIT_KHR
															| VK_VIDEO_CAPABILITY_SEPARATE_REFERENCE_IMAGES_BIT_KHR;

	if ((videoCapabilitiesKHR.flags & ~videoCapabilityFlagsKHR) != 0)
		TCU_FAIL("Undeclared videoCapabilitiesKHR.flags returned");

	if (!deIsPowerOfTwo64(videoCapabilitiesKHR.minBitstreamBufferOffsetAlignment))
		TCU_FAIL("Expected to be Power-Of-Two: videoCapabilitiesKHR.minBitstreamBufferOffsetAlignment");

	if (!deIsPowerOfTwo64(videoCapabilitiesKHR.minBitstreamBufferSizeAlignment))
		TCU_FAIL("Expected to be Power-Of-Two: videoCapabilitiesKHR.minBitstreamBufferSizeAlignment");

	if (videoCapabilitiesKHR.minBitstreamBufferOffsetAlignment == 0)
		TCU_FAIL("Expected to be non zero: videoCapabilitiesKHR.minBitstreamBufferOffsetAlignment");

	if (videoCapabilitiesKHR.minBitstreamBufferSizeAlignment == 0)
		TCU_FAIL("Expected to be non zero: videoCapabilitiesKHR.minBitstreamBufferSizeAlignment");

	if (videoCapabilitiesKHR.pictureAccessGranularity.width == 0)
		TCU_FAIL("Expected to be non-zero: videoCapabilitiesKHR.pictureAccessGranularity.width");

	if (videoCapabilitiesKHR.pictureAccessGranularity.height == 0)
		TCU_FAIL("Expected to be non-zero: videoCapabilitiesKHR.pictureAccessGranularity.height");

	if (videoCapabilitiesKHR.minCodedExtent.width == 0 || videoCapabilitiesKHR.minCodedExtent.height == 0)
		TCU_FAIL("Invalid videoCapabilitiesKHR.minCodedExtent");

	if (videoCapabilitiesKHR.maxCodedExtent.width < videoCapabilitiesKHR.minCodedExtent.width)
		TCU_FAIL("Invalid videoCapabilitiesKHR.maxCodedExtent.width");

	if (videoCapabilitiesKHR.maxCodedExtent.height < videoCapabilitiesKHR.minCodedExtent.height)
		TCU_FAIL("Invalid videoCapabilitiesKHR.maxCodedExtent.height");

	if (videoCapabilitiesKHR.maxDpbSlots == 0)
		TCU_FAIL("Invalid videoCapabilitiesKHR.maxDpbSlots");

	if (videoCapabilitiesKHR.maxActiveReferencePictures == 0)
		TCU_FAIL("Invalid videoCapabilitiesKHR.maxActiveReferencePictures");
}

void VideoCapabilitiesQueryTestInstance::validateVideoDecodeCapabilities (const VkVideoDecodeCapabilitiesKHR&	videoDecodeCapabilitiesKHR,
																		  const VkVideoDecodeCapabilitiesKHR&	videoDecodeCapabilitiesKHRSecond)
{
	const VkVideoDecodeCapabilityFlagsKHR	videoDecodeCapabilitiesFlags	= VK_VIDEO_DECODE_CAPABILITY_DPB_AND_OUTPUT_COINCIDE_BIT_KHR
																			| VK_VIDEO_DECODE_CAPABILITY_DPB_AND_OUTPUT_DISTINCT_BIT_KHR;

	VALIDATE_FIELD_EQUAL(videoDecodeCapabilitiesKHR, videoDecodeCapabilitiesKHRSecond, sType);
	VALIDATE_FIELD_EQUAL(videoDecodeCapabilitiesKHR, videoDecodeCapabilitiesKHRSecond, flags);

	if ((videoDecodeCapabilitiesKHR.flags & ~videoDecodeCapabilitiesFlags) != 0)
		TCU_FAIL("Undefined videoDecodeCapabilitiesKHR.flags");
}

void VideoCapabilitiesQueryTestInstance::validateVideoEncodeCapabilities (const VkVideoEncodeCapabilitiesKHR&	videoEncodeCapabilitiesKHR,
																		  const VkVideoEncodeCapabilitiesKHR&	videoEncodeCapabilitiesKHRSecond)
{
	VALIDATE_FIELD_EQUAL(videoEncodeCapabilitiesKHR, videoEncodeCapabilitiesKHRSecond, sType);
	VALIDATE_FIELD_EQUAL(videoEncodeCapabilitiesKHR, videoEncodeCapabilitiesKHRSecond, flags);
	VALIDATE_FIELD_EQUAL(videoEncodeCapabilitiesKHR, videoEncodeCapabilitiesKHRSecond, rateControlModes);
	VALIDATE_FIELD_EQUAL(videoEncodeCapabilitiesKHR, videoEncodeCapabilitiesKHRSecond, maxRateControlLayers);
	VALIDATE_FIELD_EQUAL(videoEncodeCapabilitiesKHR, videoEncodeCapabilitiesKHRSecond, maxQualityLevels);
	VALIDATE_FIELD_EQUAL(videoEncodeCapabilitiesKHR, videoEncodeCapabilitiesKHRSecond, encodeInputPictureGranularity);
	VALIDATE_FIELD_EQUAL(videoEncodeCapabilitiesKHR, videoEncodeCapabilitiesKHRSecond, supportedEncodeFeedbackFlags);

	const VkVideoEncodeCapabilityFlagsKHR		videoEncodeCapabilityFlags		= VK_VIDEO_ENCODE_CAPABILITY_PRECEDING_EXTERNALLY_ENCODED_BYTES_BIT_KHR;

	if ((videoEncodeCapabilitiesKHR.flags & ~videoEncodeCapabilityFlags) != 0)
		TCU_FAIL("Undeclared VkVideoEncodeCapabilitiesKHR.flags returned");

	if (videoEncodeCapabilitiesKHR.maxRateControlLayers == 0)
		TCU_FAIL("videoEncodeCapabilitiesKHR.maxRateControlLayers is zero. Implementations must report at least 1.");

	if (videoEncodeCapabilitiesKHR.maxQualityLevels == 0)
		TCU_FAIL("videoEncodeCapabilitiesKHR.maxQualityLevels is zero. Implementations must report at least 1.");
}

void VideoCapabilitiesQueryTestInstance::validateExtensionProperties (const VkExtensionProperties&	extensionProperties,
																	  const VkExtensionProperties&	extensionPropertiesSecond)
{
	VALIDATE_FIELD_EQUAL(extensionProperties, extensionPropertiesSecond, specVersion);

	for (size_t ndx = 0; ndx < VK_MAX_EXTENSION_NAME_SIZE; ++ndx)
	{
		if (extensionProperties.extensionName[ndx] != extensionPropertiesSecond.extensionName[ndx])
			TCU_FAIL("Unequal extensionProperties.extensionName");

		if (extensionProperties.extensionName[ndx] == 0)
			return;
	}

	TCU_FAIL("Non-zero terminated string extensionProperties.extensionName");
}


class VideoCapabilitiesQueryH264DecodeTestInstance : public VideoCapabilitiesQueryTestInstance
{
public:
					VideoCapabilitiesQueryH264DecodeTestInstance	(Context& context, const CaseDef& data);
	virtual			~VideoCapabilitiesQueryH264DecodeTestInstance	(void);
	tcu::TestStatus	iterate											(void);

protected:
	void			validateVideoCapabilitiesExt					(const VkVideoDecodeH264CapabilitiesKHR& videoCapabilitiesKHR,
																	 const VkVideoDecodeH264CapabilitiesKHR& videoCapabilitiesKHRSecond);
};

VideoCapabilitiesQueryH264DecodeTestInstance::VideoCapabilitiesQueryH264DecodeTestInstance (Context& context, const CaseDef& data)
	: VideoCapabilitiesQueryTestInstance(context, data)
{
}

VideoCapabilitiesQueryH264DecodeTestInstance::~VideoCapabilitiesQueryH264DecodeTestInstance (void)
{
}

tcu::TestStatus VideoCapabilitiesQueryH264DecodeTestInstance::iterate (void)
{
	const InstanceInterface&				vk						= m_context.getInstanceInterface();
	const VkPhysicalDevice					physicalDevice			= m_context.getPhysicalDevice();
	const VkVideoCodecOperationFlagBitsKHR	videoCodecOperation		= VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR;
	const VkVideoDecodeH264ProfileInfoKHR		videoProfileOperation	=
	{
		VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_PROFILE_INFO_KHR,	//  VkStructureType							sType;
		DE_NULL,												//  const void*								pNext;
		STD_VIDEO_H264_PROFILE_IDC_BASELINE,					//  StdVideoH264ProfileIdc					stdProfileIdc;
		VK_VIDEO_DECODE_H264_PICTURE_LAYOUT_PROGRESSIVE_KHR,	//  VkVideoDecodeH264PictureLayoutFlagsEXT	pictureLayout;
	};
	const VkVideoProfileInfoKHR					videoProfile			=
	{
		VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR,			//  VkStructureType						sType;
		(void*)&videoProfileOperation,					//  void*								pNext;
		videoCodecOperation,							//  VkVideoCodecOperationFlagBitsKHR	videoCodecOperation;
		VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR,		//  VkVideoChromaSubsamplingFlagsKHR	chromaSubsampling;
		VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,			//  VkVideoComponentBitDepthFlagsKHR	lumaBitDepth;
		VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,			//  VkVideoComponentBitDepthFlagsKHR	chromaBitDepth;
	};

	VkVideoDecodeH264CapabilitiesKHR		videoDecodeH264Capabilities[2];
	VkVideoDecodeCapabilitiesKHR			videoDecodeCapabilities[2];
	VkVideoCapabilitiesKHR					videoCapabilites[2];

	for (size_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(videoCapabilites); ++ndx)
	{
		const deUint8 filling = (ndx == 0) ? 0x00 : 0xFF;

		deMemset(&videoCapabilites[ndx], filling, sizeof(videoCapabilites[ndx]));
		deMemset(&videoDecodeCapabilities[ndx], filling, sizeof(videoDecodeCapabilities[ndx]));
		deMemset(&videoDecodeH264Capabilities[ndx], filling, sizeof(videoDecodeH264Capabilities[ndx]));

		videoCapabilites[ndx].sType				= VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR;
		videoCapabilites[ndx].pNext				= &videoDecodeCapabilities[ndx];
		videoDecodeCapabilities[ndx].sType		= VK_STRUCTURE_TYPE_VIDEO_DECODE_CAPABILITIES_KHR;
		videoDecodeCapabilities[ndx].pNext		= &videoDecodeH264Capabilities[ndx];
		videoDecodeH264Capabilities[ndx].sType	= VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_CAPABILITIES_KHR;
		videoDecodeH264Capabilities[ndx].pNext	= DE_NULL;

		VkResult result = vk.getPhysicalDeviceVideoCapabilitiesKHR(physicalDevice, &videoProfile, &videoCapabilites[ndx]);

		if (result != VK_SUCCESS)
		{
			ostringstream failMsg;

			failMsg << "Failed query call to vkGetPhysicalDeviceVideoCapabilitiesKHR with " << result << " at iteration " << ndx;

			return tcu::TestStatus::fail(failMsg.str());
		}
	}

	validateVideoCapabilities(videoCapabilites[0], videoCapabilites[1]);
	validateExtensionProperties(videoCapabilites[0].stdHeaderVersion, *getVideoExtensionProperties(videoCodecOperation));
	validateVideoDecodeCapabilities(videoDecodeCapabilities[0], videoDecodeCapabilities[1]);
	validateVideoCapabilitiesExt(videoDecodeH264Capabilities[0], videoDecodeH264Capabilities[1]);

	return tcu::TestStatus::pass("Pass");
}

void VideoCapabilitiesQueryH264DecodeTestInstance::validateVideoCapabilitiesExt (const VkVideoDecodeH264CapabilitiesKHR&	videoCapabilitiesKHR,
																				 const VkVideoDecodeH264CapabilitiesKHR&	videoCapabilitiesKHRSecond)
{
	VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, sType);
	VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxLevelIdc);
	VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, fieldOffsetGranularity);
}


class VideoCapabilitiesQueryH264EncodeTestInstance : public VideoCapabilitiesQueryTestInstance
{
public:
					VideoCapabilitiesQueryH264EncodeTestInstance	(Context& context, const CaseDef& data);
	virtual			~VideoCapabilitiesQueryH264EncodeTestInstance	(void);
	tcu::TestStatus	iterate											(void);

private:
	void			validateVideoCapabilitiesExt					(const VkVideoEncodeH264CapabilitiesEXT&	videoCapabilitiesKHR,
																	 const VkVideoEncodeH264CapabilitiesEXT&	videoCapabilitiesKHRSecond);
};

VideoCapabilitiesQueryH264EncodeTestInstance::VideoCapabilitiesQueryH264EncodeTestInstance (Context& context, const CaseDef& data)
	: VideoCapabilitiesQueryTestInstance(context, data)
{
}

VideoCapabilitiesQueryH264EncodeTestInstance::~VideoCapabilitiesQueryH264EncodeTestInstance (void)
{
}

tcu::TestStatus VideoCapabilitiesQueryH264EncodeTestInstance::iterate (void)
{
	const InstanceInterface&				vk						= m_context.getInstanceInterface();
	const VkPhysicalDevice					physicalDevice			= m_context.getPhysicalDevice();
	const VkVideoCodecOperationFlagBitsKHR	videoCodecOperation		= VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_EXT;
	const VkVideoEncodeH264ProfileInfoEXT		videoProfileOperation	=
	{
		VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_PROFILE_INFO_EXT,	//  VkStructureType			sType;
		DE_NULL,											//  const void*				pNext;
		STD_VIDEO_H264_PROFILE_IDC_BASELINE,				//  StdVideoH264ProfileIdc	stdProfileIdc;
	};
	const VkVideoProfileInfoKHR				videoProfile				=
	{
		VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR,			//  VkStructureType						sType;
		(void*)&videoProfileOperation,					//  void*								pNext;
		videoCodecOperation,							//  VkVideoCodecOperationFlagBitsKHR	videoCodecOperation;
		VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR,		//  VkVideoChromaSubsamplingFlagsKHR	chromaSubsampling;
		VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,			//  VkVideoComponentBitDepthFlagsKHR	lumaBitDepth;
		VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,			//  VkVideoComponentBitDepthFlagsKHR	chromaBitDepth;
	};
	VkVideoEncodeH264CapabilitiesEXT	videoEncodeH264Capabilities[2];
	VkVideoEncodeCapabilitiesKHR		videoEncodeCapabilities[2];
	VkVideoCapabilitiesKHR				videoCapabilites[2];

	for (size_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(videoCapabilites); ++ndx)
	{
		const deUint8 filling = (ndx == 0) ? 0x00 : 0xFF;

		deMemset(&videoCapabilites[ndx], filling, sizeof(videoCapabilites[ndx]));
		deMemset(&videoEncodeCapabilities[ndx], filling, sizeof(videoEncodeCapabilities[ndx]));
		deMemset(&videoEncodeH264Capabilities[ndx], filling, sizeof(videoEncodeH264Capabilities[ndx]));

		videoCapabilites[ndx].sType				= VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR;
		videoCapabilites[ndx].pNext				= &videoEncodeCapabilities[ndx];
		videoEncodeCapabilities[ndx].sType		= VK_STRUCTURE_TYPE_VIDEO_ENCODE_CAPABILITIES_KHR;
		videoEncodeCapabilities[ndx].pNext		= &videoEncodeH264Capabilities[ndx];
		videoEncodeH264Capabilities[ndx].sType	= VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_CAPABILITIES_EXT;
		videoEncodeH264Capabilities[ndx].pNext	= DE_NULL;

		VkResult result = vk.getPhysicalDeviceVideoCapabilitiesKHR(physicalDevice, &videoProfile, &videoCapabilites[ndx]);

		if (result != VK_SUCCESS)
		{
			ostringstream failMsg;

			failMsg << "Failed query call to vkGetPhysicalDeviceVideoCapabilitiesKHR with " << result << " at iteration " << ndx;

			return tcu::TestStatus::fail(failMsg.str());
		}
	}

	validateVideoCapabilities(videoCapabilites[0], videoCapabilites[1]);
	validateVideoEncodeCapabilities(videoEncodeCapabilities[0], videoEncodeCapabilities[1]);
	validateExtensionProperties(videoCapabilites[0].stdHeaderVersion, *getVideoExtensionProperties(videoCodecOperation));
	validateVideoCapabilitiesExt(videoEncodeH264Capabilities[0], videoEncodeH264Capabilities[1]);

	return tcu::TestStatus::pass("Pass");
}

void VideoCapabilitiesQueryH264EncodeTestInstance::validateVideoCapabilitiesExt (const VkVideoEncodeH264CapabilitiesEXT& videoCapabilitiesKHR, const VkVideoEncodeH264CapabilitiesEXT& videoCapabilitiesKHRSecond)
{
	const VkVideoEncodeH264CapabilityFlagsEXT	videoCapabilityFlags			= VK_VIDEO_ENCODE_H264_CAPABILITY_HRD_COMPLIANCE_BIT_EXT
																				| VK_VIDEO_ENCODE_H264_CAPABILITY_PREDICTION_WEIGHT_TABLE_GENERATED_BIT_EXT
																				| VK_VIDEO_ENCODE_H264_CAPABILITY_ROW_UNALIGNED_SLICE_BIT_EXT
																				| VK_VIDEO_ENCODE_H264_CAPABILITY_DIFFERENT_SLICE_TYPE_BIT_EXT
																				| VK_VIDEO_ENCODE_H264_CAPABILITY_B_FRAME_IN_L0_LIST_BIT_EXT
																				| VK_VIDEO_ENCODE_H264_CAPABILITY_B_FRAME_IN_L1_LIST_BIT_EXT
																				| VK_VIDEO_ENCODE_H264_CAPABILITY_PER_PICTURE_TYPE_MIN_MAX_QP_BIT_EXT
																				| VK_VIDEO_ENCODE_H264_CAPABILITY_PER_SLICE_CONSTANT_QP_BIT_EXT
																				| VK_VIDEO_ENCODE_H264_CAPABILITY_GENERATE_PREFIX_NALU_BIT_EXT;


	VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, sType);
	VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, flags);
	VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxLevelIdc);
	VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxSliceCount);
	VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxPPictureL0ReferenceCount);
	VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxBPictureL0ReferenceCount);
	VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxL1ReferenceCount);
	VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxTemporalLayerCount);
	VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, expectDyadicTemporalLayerPattern);
	VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, minQp);
	VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxQp);
	VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, prefersGopRemainingFrames);
	VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, requiresGopRemainingFrames);

	if (videoCapabilitiesKHR.flags == 0)
		TCU_FAIL("videoCapabilitiesKHR.flags must not be 0");

	if ((videoCapabilitiesKHR.flags & ~videoCapabilityFlags) != 0)
		TCU_FAIL("Undefined videoCapabilitiesKHR.flags");
}


class VideoCapabilitiesQueryH265DecodeTestInstance : public VideoCapabilitiesQueryTestInstance
{
public:
					VideoCapabilitiesQueryH265DecodeTestInstance	(Context& context, const CaseDef& data);
	virtual			~VideoCapabilitiesQueryH265DecodeTestInstance	(void);
	tcu::TestStatus	iterate											(void);

protected:
	void			validateVideoCapabilitiesExt					(const VkVideoDecodeH265CapabilitiesKHR&	videoCapabilitiesKHR,
																	 const VkVideoDecodeH265CapabilitiesKHR&	videoCapabilitiesKHRSecond);
};

VideoCapabilitiesQueryH265DecodeTestInstance::VideoCapabilitiesQueryH265DecodeTestInstance (Context& context, const CaseDef& data)
	: VideoCapabilitiesQueryTestInstance(context, data)
{
}

VideoCapabilitiesQueryH265DecodeTestInstance::~VideoCapabilitiesQueryH265DecodeTestInstance (void)
{
}

tcu::TestStatus VideoCapabilitiesQueryH265DecodeTestInstance::iterate (void)
{
	const InstanceInterface&				vk						= m_context.getInstanceInterface();
	const VkPhysicalDevice					physicalDevice			= m_context.getPhysicalDevice();
	const VkVideoCodecOperationFlagBitsKHR	videoCodecOperation		= VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR;
	const VkVideoDecodeH265ProfileInfoKHR		videoProfileOperation	=
	{
		VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_PROFILE_INFO_KHR,	//  VkStructureType			sType;
		DE_NULL,											//  const void*				pNext;
		STD_VIDEO_H265_PROFILE_IDC_MAIN,					//  StdVideoH265ProfileIdc	stdProfileIdc;
	};
	const VkVideoProfileInfoKHR					videoProfile			=
	{
		VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR,			//  VkStructureType						sType;
		(void*)&videoProfileOperation,					//  void*								pNext;
		videoCodecOperation,							//  VkVideoCodecOperationFlagBitsKHR	videoCodecOperation;
		VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR,		//  VkVideoChromaSubsamplingFlagsKHR	chromaSubsampling;
		VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,			//  VkVideoComponentBitDepthFlagsKHR	lumaBitDepth;
		VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,			//  VkVideoComponentBitDepthFlagsKHR	chromaBitDepth;
	};
	VkVideoDecodeH265CapabilitiesKHR	videoDecodeH265Capabilities[2];
	VkVideoDecodeCapabilitiesKHR		videoDecodeCapabilities[2];
	VkVideoCapabilitiesKHR				videoCapabilites[2];

	for (size_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(videoCapabilites); ++ndx)
	{
		const deUint8 filling = (ndx == 0) ? 0x00 : 0xFF;

		deMemset(&videoCapabilites[ndx], filling, sizeof(videoCapabilites[ndx]));
		deMemset(&videoDecodeCapabilities[ndx], filling, sizeof(videoDecodeCapabilities[ndx]));
		deMemset(&videoDecodeH265Capabilities[ndx], filling, sizeof(videoDecodeH265Capabilities[ndx]));

		videoCapabilites[ndx].sType				= VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR;
		videoCapabilites[ndx].pNext				= &videoDecodeCapabilities[ndx];
		videoDecodeCapabilities[ndx].sType		= VK_STRUCTURE_TYPE_VIDEO_DECODE_CAPABILITIES_KHR;
		videoDecodeCapabilities[ndx].pNext		= &videoDecodeH265Capabilities[ndx];
		videoDecodeH265Capabilities[ndx].sType	= VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_CAPABILITIES_KHR;
		videoDecodeH265Capabilities[ndx].pNext	= DE_NULL;

		VkResult result = vk.getPhysicalDeviceVideoCapabilitiesKHR(physicalDevice, &videoProfile, &videoCapabilites[ndx]);

		if (result != VK_SUCCESS)
		{
			ostringstream failMsg;

			failMsg << "Failed query call to vkGetPhysicalDeviceVideoCapabilitiesKHR with " << result << " at iteration " << ndx;

			return tcu::TestStatus::fail(failMsg.str());
		}
	}

	validateVideoCapabilities(videoCapabilites[0], videoCapabilites[1]);
	validateExtensionProperties(videoCapabilites[0].stdHeaderVersion, *getVideoExtensionProperties(videoCodecOperation));
	validateVideoDecodeCapabilities(videoDecodeCapabilities[0], videoDecodeCapabilities[1]);
	validateVideoCapabilitiesExt(videoDecodeH265Capabilities[0], videoDecodeH265Capabilities[1]);

	return tcu::TestStatus::pass("Pass");
}

void VideoCapabilitiesQueryH265DecodeTestInstance::validateVideoCapabilitiesExt (const VkVideoDecodeH265CapabilitiesKHR&	videoCapabilitiesKHR,
																				 const VkVideoDecodeH265CapabilitiesKHR&	videoCapabilitiesKHRSecond)
{
	VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, sType);
	VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxLevelIdc);
}

class VideoCapabilitiesQueryH265EncodeTestInstance : public VideoCapabilitiesQueryTestInstance
{
public:
					VideoCapabilitiesQueryH265EncodeTestInstance	(Context& context, const CaseDef& data);
	virtual			~VideoCapabilitiesQueryH265EncodeTestInstance	(void);
	tcu::TestStatus	iterate											(void);

protected:
	void			validateVideoCapabilitiesExt					(const VkVideoEncodeH265CapabilitiesEXT&	videoCapabilitiesKHR,
																	 const VkVideoEncodeH265CapabilitiesEXT&	videoCapabilitiesKHRSecond);
};

VideoCapabilitiesQueryH265EncodeTestInstance::VideoCapabilitiesQueryH265EncodeTestInstance (Context& context, const CaseDef& data)
	: VideoCapabilitiesQueryTestInstance(context, data)
{
}

VideoCapabilitiesQueryH265EncodeTestInstance::~VideoCapabilitiesQueryH265EncodeTestInstance (void)
{
}

tcu::TestStatus VideoCapabilitiesQueryH265EncodeTestInstance::iterate (void)
{
	const InstanceInterface&				vk						= m_context.getInstanceInterface();
	const VkPhysicalDevice					physicalDevice			= m_context.getPhysicalDevice();
	const VkVideoCodecOperationFlagBitsKHR	videoCodecOperation		= VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_EXT;
	const VkVideoEncodeH265ProfileInfoEXT		videoProfileOperation	=
	{
		VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_PROFILE_INFO_EXT,	//  VkStructureType			sType;
		DE_NULL,											//  const void*				pNext;
		STD_VIDEO_H265_PROFILE_IDC_MAIN,					//  StdVideoH265ProfileIdc	stdProfileIdc;
	};
	const VkVideoProfileInfoKHR					videoProfile			=
	{
		VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR,			//  VkStructureType						sType;
		(void*)&videoProfileOperation,					//  void*								pNext;
		videoCodecOperation,							//  VkVideoCodecOperationFlagBitsKHR	videoCodecOperation;
		VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR,		//  VkVideoChromaSubsamplingFlagsKHR	chromaSubsampling;
		VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,			//  VkVideoComponentBitDepthFlagsKHR	lumaBitDepth;
		VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,			//  VkVideoComponentBitDepthFlagsKHR	chromaBitDepth;
	};
	VkVideoEncodeH265CapabilitiesEXT	videoEncodeH265Capabilities[2];
	VkVideoEncodeCapabilitiesKHR		videoEncodeCapabilities[2];
	VkVideoCapabilitiesKHR				videoCapabilites[2];

	for (size_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(videoCapabilites); ++ndx)
	{
		const deUint8 filling = (ndx == 0) ? 0x00 : 0xFF;

		deMemset(&videoCapabilites[ndx], filling, sizeof(videoCapabilites[ndx]));
		deMemset(&videoEncodeCapabilities[ndx], filling, sizeof(videoEncodeCapabilities[ndx]));
		deMemset(&videoEncodeH265Capabilities[ndx], filling, sizeof(videoEncodeH265Capabilities[ndx]));

		videoCapabilites[ndx].sType				= VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR;
		videoCapabilites[ndx].pNext				= &videoEncodeCapabilities[ndx];
		videoEncodeCapabilities[ndx].sType		= VK_STRUCTURE_TYPE_VIDEO_ENCODE_CAPABILITIES_KHR;
		videoEncodeCapabilities[ndx].pNext		= &videoEncodeH265Capabilities[ndx];
		videoEncodeH265Capabilities[ndx].sType	= VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_CAPABILITIES_EXT;
		videoEncodeH265Capabilities[ndx].pNext	= DE_NULL;

		VkResult result = vk.getPhysicalDeviceVideoCapabilitiesKHR(physicalDevice, &videoProfile, &videoCapabilites[ndx]);

		if (result != VK_SUCCESS)
		{
			ostringstream failMsg;

			failMsg << "Failed query call to vkGetPhysicalDeviceVideoCapabilitiesKHR with " << result << " at iteration " << ndx;

			return tcu::TestStatus::fail(failMsg.str());
		}
	}

	validateVideoCapabilities(videoCapabilites[0], videoCapabilites[1]);
	validateVideoEncodeCapabilities(videoEncodeCapabilities[0], videoEncodeCapabilities[1]);
	validateExtensionProperties(videoCapabilites[0].stdHeaderVersion, *getVideoExtensionProperties(videoCodecOperation));
	validateVideoCapabilitiesExt(videoEncodeH265Capabilities[0], videoEncodeH265Capabilities[1]);

	return tcu::TestStatus::pass("Pass");
}

void VideoCapabilitiesQueryH265EncodeTestInstance::validateVideoCapabilitiesExt (const VkVideoEncodeH265CapabilitiesEXT&	videoCapabilitiesKHR,
																				 const VkVideoEncodeH265CapabilitiesEXT&	videoCapabilitiesKHRSecond)
{
	const VkVideoEncodeH265CtbSizeFlagsEXT				ctbSizeFlags			= VK_VIDEO_ENCODE_H265_CTB_SIZE_16_BIT_EXT
																				| VK_VIDEO_ENCODE_H265_CTB_SIZE_32_BIT_EXT
																				| VK_VIDEO_ENCODE_H265_CTB_SIZE_64_BIT_EXT;
	const VkVideoEncodeH265TransformBlockSizeFlagsEXT	transformBlockSizes		= VK_VIDEO_ENCODE_H265_TRANSFORM_BLOCK_SIZE_4_BIT_EXT
																				| VK_VIDEO_ENCODE_H265_TRANSFORM_BLOCK_SIZE_8_BIT_EXT
																				| VK_VIDEO_ENCODE_H265_TRANSFORM_BLOCK_SIZE_16_BIT_EXT
																				| VK_VIDEO_ENCODE_H265_TRANSFORM_BLOCK_SIZE_32_BIT_EXT;

	VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, sType);
	VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, flags);
	VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxLevelIdc);
	VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxSliceSegmentCount);
	VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxTiles);
	VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxPPictureL0ReferenceCount);
	VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxBPictureL0ReferenceCount);
	VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxL1ReferenceCount);
	VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxSubLayerCount);
	VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, expectDyadicTemporalSubLayerPattern);
	VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, minQp);
	VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxQp);
	VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, prefersGopRemainingFrames);
	VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, requiresGopRemainingFrames);

	if (videoCapabilitiesKHR.flags != 0)
		TCU_FAIL("videoCapabilitiesKHR.flags must be 0");

	if (videoCapabilitiesKHR.ctbSizes == 0)
		TCU_FAIL("Invalid videoCapabilitiesKHR.ctbSizes");

	if ((videoCapabilitiesKHR.ctbSizes & ~ctbSizeFlags) != 0)
		TCU_FAIL("Undefined videoCapabilitiesKHR.ctbSizeFlags");

	if (videoCapabilitiesKHR.transformBlockSizes == 0)
		TCU_FAIL("Invalid videoCapabilitiesKHR.transformBlockSizes");

	if ((videoCapabilitiesKHR.transformBlockSizes & ~transformBlockSizes) != 0)
		TCU_FAIL("Undefined videoCapabilitiesKHR.transformBlockSizes");
}


class VideoCapabilitiesQueryTestCase : public TestCase
{
	public:
							VideoCapabilitiesQueryTestCase	(tcu::TestContext& context, const char* name, const char* desc, const CaseDef caseDef);
							~VideoCapabilitiesQueryTestCase	(void);

	virtual TestInstance*	createInstance					(Context& context) const;
	virtual void			checkSupport					(Context& context) const;

private:
	CaseDef					m_caseDef;
};

VideoCapabilitiesQueryTestCase::VideoCapabilitiesQueryTestCase (tcu::TestContext& context, const char* name, const char* desc, const CaseDef caseDef)
	: vkt::TestCase	(context, name, desc)
	, m_caseDef		(caseDef)
{
}

VideoCapabilitiesQueryTestCase::~VideoCapabilitiesQueryTestCase	(void)
{
}

void VideoCapabilitiesQueryTestCase::checkSupport (Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_video_queue");

	switch (m_caseDef.testType)
	{
		case TEST_TYPE_QUEUE_SUPPORT_QUERY:							break;
		case TEST_TYPE_H264_DECODE_DST_VIDEO_FORMAT_SUPPORT_QUERY:
		case TEST_TYPE_H264_DECODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:	context.requireDeviceFunctionality("VK_KHR_video_decode_h264"); break;
		case TEST_TYPE_H264_ENCODE_SRC_VIDEO_FORMAT_SUPPORT_QUERY:
		case TEST_TYPE_H264_ENCODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:	context.requireDeviceFunctionality("VK_EXT_video_encode_h264"); break;
		case TEST_TYPE_H265_DECODE_DST_VIDEO_FORMAT_SUPPORT_QUERY:
		case TEST_TYPE_H265_DECODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:	context.requireDeviceFunctionality("VK_KHR_video_decode_h265"); break;
		case TEST_TYPE_H265_ENCODE_SRC_VIDEO_FORMAT_SUPPORT_QUERY:
		case TEST_TYPE_H265_ENCODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:	context.requireDeviceFunctionality("VK_EXT_video_encode_h265"); break;
		case TEST_TYPE_H264_DECODE_CAPABILITIES_QUERY:				context.requireDeviceFunctionality("VK_KHR_video_decode_h264"); break;
		case TEST_TYPE_H264_ENCODE_CAPABILITIES_QUERY:				context.requireDeviceFunctionality("VK_EXT_video_encode_h264"); break;
		case TEST_TYPE_H265_DECODE_CAPABILITIES_QUERY:				context.requireDeviceFunctionality("VK_KHR_video_decode_h265"); break;
		case TEST_TYPE_H265_ENCODE_CAPABILITIES_QUERY:				context.requireDeviceFunctionality("VK_EXT_video_encode_h265"); break;
		default:													TCU_THROW(NotSupportedError, "Unknown TestType");
	}
}

TestInstance* VideoCapabilitiesQueryTestCase::createInstance (Context& context) const
{
	switch (m_caseDef.testType)
	{
		case TEST_TYPE_QUEUE_SUPPORT_QUERY:							return new VideoQueueQueryTestInstance(context, m_caseDef);
		case TEST_TYPE_H264_DECODE_DST_VIDEO_FORMAT_SUPPORT_QUERY:	return new VideoFormatPropertiesQueryH264DecodeTestInstance(context, m_caseDef);
		case TEST_TYPE_H264_DECODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:	return new VideoFormatPropertiesQueryH264DecodeTestInstance(context, m_caseDef);
		case TEST_TYPE_H264_ENCODE_SRC_VIDEO_FORMAT_SUPPORT_QUERY:	return new VideoFormatPropertiesQueryH264EncodeTestInstance(context, m_caseDef);
		case TEST_TYPE_H264_ENCODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:	return new VideoFormatPropertiesQueryH264EncodeTestInstance(context, m_caseDef);
		case TEST_TYPE_H265_DECODE_DST_VIDEO_FORMAT_SUPPORT_QUERY:	return new VideoFormatPropertiesQueryH265DecodeTestInstance(context, m_caseDef);
		case TEST_TYPE_H265_DECODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:	return new VideoFormatPropertiesQueryH265DecodeTestInstance(context, m_caseDef);
		case TEST_TYPE_H265_ENCODE_SRC_VIDEO_FORMAT_SUPPORT_QUERY:	return new VideoFormatPropertiesQueryH265EncodeTestInstance(context, m_caseDef);
		case TEST_TYPE_H265_ENCODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:	return new VideoFormatPropertiesQueryH265EncodeTestInstance(context, m_caseDef);
		case TEST_TYPE_H264_DECODE_CAPABILITIES_QUERY:				return new VideoCapabilitiesQueryH264DecodeTestInstance(context, m_caseDef);
		case TEST_TYPE_H264_ENCODE_CAPABILITIES_QUERY:				return new VideoCapabilitiesQueryH264EncodeTestInstance(context, m_caseDef);
		case TEST_TYPE_H265_DECODE_CAPABILITIES_QUERY:				return new VideoCapabilitiesQueryH265DecodeTestInstance(context, m_caseDef);
		case TEST_TYPE_H265_ENCODE_CAPABILITIES_QUERY:				return new VideoCapabilitiesQueryH265EncodeTestInstance(context, m_caseDef);
		default:													TCU_THROW(NotSupportedError, "Unknown TestType");
	}
}

const char* getTestName (const TestType testType)
{
	switch (testType)
	{
		case TEST_TYPE_QUEUE_SUPPORT_QUERY:							return "queue_support_query";
		case TEST_TYPE_H264_DECODE_DST_VIDEO_FORMAT_SUPPORT_QUERY:	return "h264_decode_dst_video_format_support_query";
		case TEST_TYPE_H264_DECODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:	return "h264_decode_dpb_video_format_support_query";
		case TEST_TYPE_H264_ENCODE_SRC_VIDEO_FORMAT_SUPPORT_QUERY:	return "h264_encode_src_video_format_support_query";
		case TEST_TYPE_H264_ENCODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:	return "h264_encode_dpb_video_format_support_query";
		case TEST_TYPE_H265_DECODE_DST_VIDEO_FORMAT_SUPPORT_QUERY:	return "h265_decode_dst_video_format_support_query";
		case TEST_TYPE_H265_DECODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:	return "h265_decode_spb_video_format_support_query";
		case TEST_TYPE_H265_ENCODE_SRC_VIDEO_FORMAT_SUPPORT_QUERY:	return "h265_encode_src_video_format_support_query";
		case TEST_TYPE_H265_ENCODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:	return "h265_encode_dpb_video_format_support_query";
		case TEST_TYPE_H264_DECODE_CAPABILITIES_QUERY:				return "h264_decode_capabilities_query";
		case TEST_TYPE_H264_ENCODE_CAPABILITIES_QUERY:				return "h264_encode_capabilities_query";
		case TEST_TYPE_H265_DECODE_CAPABILITIES_QUERY:				return "h265_decode_capabilities_query";
		case TEST_TYPE_H265_ENCODE_CAPABILITIES_QUERY:				return "h265_encode_capabilities_query";
		default:													TCU_THROW(NotSupportedError, "Unknown TestType");
	}
}
}	// anonymous

tcu::TestCaseGroup*	createVideoCapabilitiesTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group	(new tcu::TestCaseGroup(testCtx, "capabilities", "Video encoding and decoding capability query tests"));

	for (int testTypeNdx = 0; testTypeNdx < TEST_TYPE_LAST; ++testTypeNdx)
	{
		const TestType	testType	= static_cast<TestType>(testTypeNdx);
		const CaseDef	caseDef		=
		{
			testType,	//  TestType	testType;
		};

		group->addChild(new VideoCapabilitiesQueryTestCase(testCtx, getTestName(testType), "", caseDef));
	}

	return group.release();
}
}	// video
}	// vkt
