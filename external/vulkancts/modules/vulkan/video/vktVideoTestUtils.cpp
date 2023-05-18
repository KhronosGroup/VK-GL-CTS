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

using namespace vk;
using namespace std;

namespace vkt
{
namespace video
{

using namespace vk;
using namespace std;



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

static VkExtensionProperties makeExtensionProperties(const char* extensionName, deUint32	specVersion)
{
	const deUint32			extensionNameLen = static_cast<deUint32>(deStrnlen(extensionName, VK_MAX_EXTENSION_NAME_SIZE));
	VkExtensionProperties	result;

	deMemset(&result, 0, sizeof(result));

	deMemcpy(&result.extensionName, extensionName, extensionNameLen);

	result.specVersion = specVersion;

	return result;
}

static const VkExtensionProperties	EXTENSION_PROPERTIES_H264_DECODE = makeExtensionProperties(VK_STD_VULKAN_VIDEO_CODEC_H264_DECODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_H264_DECODE_SPEC_VERSION);
static const VkExtensionProperties	EXTENSION_PROPERTIES_H264_ENCODE = makeExtensionProperties(VK_STD_VULKAN_VIDEO_CODEC_H264_ENCODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_H264_ENCODE_SPEC_VERSION);
static const VkExtensionProperties	EXTENSION_PROPERTIES_H265_DECODE = makeExtensionProperties(VK_STD_VULKAN_VIDEO_CODEC_H265_DECODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_H265_DECODE_SPEC_VERSION);
static const VkExtensionProperties	EXTENSION_PROPERTIES_H265_ENCODE = makeExtensionProperties(VK_STD_VULKAN_VIDEO_CODEC_H265_ENCODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_H265_ENCODE_SPEC_VERSION);

VideoBaseTestInstance::VideoBaseTestInstance (Context& context)
	: TestInstance	(context)
	, m_videoDevice	(context)
{
}

VideoBaseTestInstance::~VideoBaseTestInstance (void)
{
}

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

const deUint32& VideoBaseTestInstance::getQueueFamilyIndexTransfer (void)
{
	return m_videoDevice.getQueueFamilyIndexTransfer();
}

const deUint32& VideoBaseTestInstance::getQueueFamilyIndexDecode (void)
{
	return m_videoDevice.getQueueFamilyIndexDecode();
}

const deUint32& VideoBaseTestInstance::getQueueFamilyIndexEncode (void)
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

de::MovePtr<VkVideoDecodeCapabilitiesKHR> getVideoDecodeCapabilities (void* pNext)
{
	const VkVideoDecodeCapabilitiesKHR	videoDecodeCapabilities =
	{
		vk::VK_STRUCTURE_TYPE_VIDEO_DECODE_CAPABILITIES_KHR,	//  VkStructureType					sType;
		pNext,													//  void*							pNext;
		0,														//  VkVideoDecodeCapabilityFlagsKHR	Flags;
	};

	return de::MovePtr<VkVideoDecodeCapabilitiesKHR>(new VkVideoDecodeCapabilitiesKHR(videoDecodeCapabilities));
}

de::MovePtr<VkVideoDecodeH264CapabilitiesKHR> getVideoCapabilitiesExtensionH264D (void)
{
	const VkVideoDecodeH264CapabilitiesKHR	videoCapabilitiesExtension =
	{
		vk::VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_CAPABILITIES_KHR,		//  VkStructureType		sType;
		DE_NULL,														//  void*				pNext;
		STD_VIDEO_H264_LEVEL_IDC_1_0,									//  StdVideoH264Level	maxLevel;
		{0, 0},															//  VkOffset2D			fieldOffsetGranularity;
	};

	return de::MovePtr<VkVideoDecodeH264CapabilitiesKHR>(new VkVideoDecodeH264CapabilitiesKHR(videoCapabilitiesExtension));
}

de::MovePtr <VkVideoEncodeH264CapabilitiesEXT> getVideoCapabilitiesExtensionH264E (void)
{
	const VkVideoEncodeH264CapabilitiesEXT		videoCapabilitiesExtension =
	{
		vk::VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_CAPABILITIES_EXT,	//  VkStructureType						sType;
		DE_NULL,													//  const void*							pNext;
		0u,															//  VkVideoEncodeH264CapabilityFlagsEXT	flags;
		0u,															//  uint8_t								maxPPictureL0ReferenceCount;
		0u,															//  uint8_t								maxBPictureL0ReferenceCount;
		0u,															//  uint8_t								maxL1ReferenceCount;
		DE_FALSE,													//  VkBool32							motionVectorsOverPicBoundariesFlag;
		0u,															//  uint32_t							maxBytesPerPicDenom;
		0u,															//  uint32_t							maxBitsPerMbDenom;
		0u,															//  uint32_t							log2MaxMvLengthHorizontal;
		0u,															//  uint32_t							log2MaxMvLengthVertical;
	};

	return de::MovePtr<VkVideoEncodeH264CapabilitiesEXT>(new VkVideoEncodeH264CapabilitiesEXT(videoCapabilitiesExtension));
}

de::MovePtr<VkVideoDecodeH265CapabilitiesKHR> getVideoCapabilitiesExtensionH265D (void)
{
	const VkVideoDecodeH265CapabilitiesKHR		videoCapabilitiesExtension =
	{
		VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_CAPABILITIES_KHR,		//  VkStructureType		sType;
		DE_NULL,													//  void*				pNext;
		STD_VIDEO_H265_LEVEL_IDC_1_0,								//  StdVideoH265Level	maxLevel;
	};

	return de::MovePtr<VkVideoDecodeH265CapabilitiesKHR>(new VkVideoDecodeH265CapabilitiesKHR(videoCapabilitiesExtension));
}

de::MovePtr <VkVideoEncodeH265CapabilitiesEXT> getVideoCapabilitiesExtensionH265E (void)
{
	const VkVideoEncodeH265CapabilitiesEXT		videoCapabilitiesExtension =
	{
		VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_CAPABILITIES_EXT,	//  VkStructureType								sType;
		DE_NULL,												//  const void*									pNext;
		0u,														//  VkVideoEncodeH265CapabilityFlagsEXT			flags;
		0u,														//  VkVideoEncodeH265CtbSizeFlagsEXT			ctbSizes;
		0u,														//  VkVideoEncodeH265TransformBlockSizeFlagsEXT	transformBlockSizes;
		0u,														//  uint8_t										maxPPictureL0ReferenceCount;
		0u,														//  uint8_t										maxBPictureL0ReferenceCount;
		0u,														//  uint8_t										maxL1ReferenceCount;
		0u,														//  uint8_t										maxSubLayersCount;
		0u,														//  uint8_t										minLog2MinLumaCodingBlockSizeMinus3;
		0u,														//  uint8_t										maxLog2MinLumaCodingBlockSizeMinus3;
		0u,														//  uint8_t										minLog2MinLumaTransformBlockSizeMinus2;
		0u,														//  uint8_t										maxLog2MinLumaTransformBlockSizeMinus2;
		0u,														//  uint8_t										minMaxTransformHierarchyDepthInter;
		0u,														//  uint8_t										maxMaxTransformHierarchyDepthInter;
		0u,														//  uint8_t										minMaxTransformHierarchyDepthIntra;
		0u,														//  uint8_t										maxMaxTransformHierarchyDepthIntra;
		0u,														//  uint8_t										maxDiffCuQpDeltaDepth;
		0u,														//  uint8_t										minMaxNumMergeCand;
		0u,														//  uint8_t										maxMaxNumMergeCand;
	};

	return de::MovePtr<VkVideoEncodeH265CapabilitiesEXT>(new VkVideoEncodeH265CapabilitiesEXT(videoCapabilitiesExtension));
}

de::MovePtr<VkVideoCapabilitiesKHR> getVideoCapabilities (const InstanceInterface&	vk,
														  VkPhysicalDevice			physicalDevice,
														  const VkVideoProfileInfoKHR*	videoProfile,
														  void*						pNext)
{
	VkVideoCapabilitiesKHR*				videoCapabilities	= new VkVideoCapabilitiesKHR
	{
		VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR,	//  VkStructureType				sType;
		pNext,										//  void*						pNext;
		0,											//  VkVideoCapabilityFlagsKHR	capabilityFlags;
		0,											//  VkDeviceSize				minBitstreamBufferOffsetAlignment;
		0,											//  VkDeviceSize				minBitstreamBufferSizeAlignment;
		{0, 0},										//  VkExtent2D					videoPictureExtentGranularity;
		{0, 0},										//  VkExtent2D					minExtent;
		{0, 0},										//  VkExtent2D					maxExtent;
		0,											//  uint32_t					maxReferencePicturesSlotsCount;
		0,											//  uint32_t					maxReferencePicturesActiveCount;
		{ { 0 }, 0 },								//  VkExtensionProperties		stdHeaderVersion;
	};
	de::MovePtr<VkVideoCapabilitiesKHR>	result				= de::MovePtr<VkVideoCapabilitiesKHR>(videoCapabilities);

	VK_CHECK(vk.getPhysicalDeviceVideoCapabilitiesKHR(physicalDevice, videoProfile, videoCapabilities));

	return result;
}

de::MovePtr<VkVideoDecodeH264ProfileInfoKHR> getVideoProfileExtensionH264D (StdVideoH264ProfileIdc stdProfileIdc, VkVideoDecodeH264PictureLayoutFlagBitsKHR pictureLayout)
{
	VkVideoDecodeH264ProfileInfoKHR*				videoCodecOperation	= new VkVideoDecodeH264ProfileInfoKHR(getProfileOperationH264D(stdProfileIdc, pictureLayout));
	de::MovePtr<VkVideoDecodeH264ProfileInfoKHR>	result				= de::MovePtr<VkVideoDecodeH264ProfileInfoKHR>(videoCodecOperation);

	return result;
}

de::MovePtr<VkVideoEncodeH264ProfileInfoEXT> getVideoProfileExtensionH264E (StdVideoH264ProfileIdc stdProfileIdc)
{
	VkVideoEncodeH264ProfileInfoEXT*				videoCodecOperation	= new VkVideoEncodeH264ProfileInfoEXT(getProfileOperationH264E(stdProfileIdc));
	de::MovePtr<VkVideoEncodeH264ProfileInfoEXT>	result				= de::MovePtr<VkVideoEncodeH264ProfileInfoEXT>(videoCodecOperation);

	return result;
}

de::MovePtr<VkVideoDecodeH265ProfileInfoKHR> getVideoProfileExtensionH265D (StdVideoH265ProfileIdc stdProfileIdc)
{
	VkVideoDecodeH265ProfileInfoKHR*				videoCodecOperation	= new VkVideoDecodeH265ProfileInfoKHR(getProfileOperationH265D(stdProfileIdc));
	de::MovePtr<VkVideoDecodeH265ProfileInfoKHR>	result				= de::MovePtr<VkVideoDecodeH265ProfileInfoKHR>(videoCodecOperation);

	return result;
}

de::MovePtr<VkVideoEncodeH265ProfileInfoEXT> getVideoProfileExtensionH265E (StdVideoH265ProfileIdc stdProfileIdc)
{
	VkVideoEncodeH265ProfileInfoEXT*				videoCodecOperation	= new VkVideoEncodeH265ProfileInfoEXT(getProfileOperationH265E(stdProfileIdc));
	de::MovePtr<VkVideoEncodeH265ProfileInfoEXT>	result				= de::MovePtr<VkVideoEncodeH265ProfileInfoEXT>(videoCodecOperation);

	return result;
}

de::MovePtr<VkVideoProfileInfoKHR> getVideoProfile (VkVideoCodecOperationFlagBitsKHR	videoCodecOperation,
													void*								pNext,
													VkVideoChromaSubsamplingFlagsKHR	chromaSubsampling,
													VkVideoComponentBitDepthFlagsKHR	lumaBitDepth,
													VkVideoComponentBitDepthFlagsKHR	chromaBitDepth)
{
	VkVideoProfileInfoKHR*				videoProfile	= new VkVideoProfileInfoKHR
	{
		VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR,		//  VkStructureType						sType;
		pNext,											//  void*								pNext;
		videoCodecOperation,							//  VkVideoCodecOperationFlagBitsKHR	videoCodecOperation;
		chromaSubsampling,								//  VkVideoChromaSubsamplingFlagsKHR	chromaSubsampling;
		lumaBitDepth,									//  VkVideoComponentBitDepthFlagsKHR	lumaBitDepth;
		chromaBitDepth,									//  VkVideoComponentBitDepthFlagsKHR	chromaBitDepth;
	};
	de::MovePtr<VkVideoProfileInfoKHR>	result			= de::MovePtr<VkVideoProfileInfoKHR>(videoProfile);

	return result;
}

de::MovePtr<VkVideoProfileListInfoKHR> getVideoProfileList (const VkVideoProfileInfoKHR* videoProfile)
{
	VkVideoProfileListInfoKHR*		videoProfileList = new VkVideoProfileListInfoKHR
	{
	VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR,		//  VkStructureType					sType;
	DE_NULL,											//  const void*						pNext;
	1,													//  uint32_t						profileCount;
	videoProfile,										// const VkVideoProfileInfoKHR*		pProfiles;
	};

	de::MovePtr<VkVideoProfileListInfoKHR>	result			= de::MovePtr<VkVideoProfileListInfoKHR>(videoProfileList);

	return result;
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

de::MovePtr<VkVideoSessionCreateInfoKHR> getVideoSessionCreateInfo (deUint32					queueFamilyIndex,
																	const VkVideoProfileInfoKHR*	videoProfile,
																	const VkExtent2D&			codedExtent,
																	VkFormat					pictureFormat,
																	VkFormat					referencePicturesFormat,
																	deUint32					maxReferencePicturesSlotsCount,
																	deUint32					maxReferencePicturesActiveCount)
{

	//FIXME: last spec version accepted by the parser function
	//const VkExtensionProperties*				extensionProperties		= getVideoExtensionProperties(videoProfile->videoCodecOperation);

	static const vk::VkExtensionProperties h264StdExtensionVersion = { VK_STD_VULKAN_VIDEO_CODEC_H264_DECODE_EXTENSION_NAME, VK_MAKE_VIDEO_STD_VERSION(1, 0, 0) };
	static const vk::VkExtensionProperties h265StdExtensionVersion = { VK_STD_VULKAN_VIDEO_CODEC_H265_DECODE_EXTENSION_NAME, VK_MAKE_VIDEO_STD_VERSION(1, 0, 0) };

	VkVideoSessionCreateInfoKHR*				videoSessionCreateInfo	= new VkVideoSessionCreateInfoKHR
	{
		VK_STRUCTURE_TYPE_VIDEO_SESSION_CREATE_INFO_KHR,	//  VkStructureType					sType;
		DE_NULL,											//  const void*						pNext;
		queueFamilyIndex,									//  uint32_t						queueFamilyIndex;
		static_cast<VkVideoSessionCreateFlagsKHR>(0),		//  VkVideoSessionCreateFlagsKHR	flags;
		videoProfile,										//  const VkVideoProfileInfoKHR*	pVideoProfile;
		pictureFormat,										//  VkFormat						pictureFormat;
		codedExtent,										//  VkExtent2D						maxCodedExtent;
		referencePicturesFormat,							//  VkFormat						referencePicturesFormat;
		maxReferencePicturesSlotsCount,						//  uint32_t						maxReferencePicturesSlotsCount;
		maxReferencePicturesActiveCount,					//  uint32_t						maxReferencePicturesActiveCount;
		videoProfile->videoCodecOperation == VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR ? &h264StdExtensionVersion : &h265StdExtensionVersion,							//  const VkExtensionProperties*	pStdHeaderVersion;
	};

	de::MovePtr<VkVideoSessionCreateInfoKHR>	result					= de::MovePtr<VkVideoSessionCreateInfoKHR>(videoSessionCreateInfo);

	return result;
}

vector<AllocationPtr> getAndBindVideoSessionMemory (const DeviceInterface&	vkd,
													const VkDevice			device,
													VkVideoSessionKHR		videoSession,
													Allocator&				allocator)
{
	deUint32	videoSessionMemoryRequirementsCount	= 0;

	DE_ASSERT(videoSession != DE_NULL);

	VK_CHECK(vkd.getVideoSessionMemoryRequirementsKHR(device, videoSession, &videoSessionMemoryRequirementsCount, DE_NULL));

	const VkVideoSessionMemoryRequirementsKHR			videoGetMemoryPropertiesKHR			=
	{
		VK_STRUCTURE_TYPE_VIDEO_SESSION_MEMORY_REQUIREMENTS_KHR,	//  VkStructureType			sType;
		DE_NULL,													//  const void*				pNext;
		0u,															//  deUint32				memoryBindIndex;
		{0ull, 0ull, 0u},											//  VkMemoryRequirements    memoryRequirements;
	};

	vector<VkVideoSessionMemoryRequirementsKHR>		videoSessionMemoryRequirements		(videoSessionMemoryRequirementsCount, videoGetMemoryPropertiesKHR);

	for (size_t ndx = 0; ndx < videoSessionMemoryRequirements.size(); ++ndx)
		videoSessionMemoryRequirements[ndx].sType = VK_STRUCTURE_TYPE_VIDEO_SESSION_MEMORY_REQUIREMENTS_KHR;

	VK_CHECK(vkd.getVideoSessionMemoryRequirementsKHR(device, videoSession, &videoSessionMemoryRequirementsCount, videoSessionMemoryRequirements.data()));

	vector<AllocationPtr>								allocations							(videoSessionMemoryRequirements.size());
	vector<VkBindVideoSessionMemoryInfoKHR>				videoBindsMemoryKHR					(videoSessionMemoryRequirements.size());

	for (size_t ndx = 0; ndx < allocations.size(); ++ndx)
	{
		const VkMemoryRequirements& requirements		= videoSessionMemoryRequirements[ndx].memoryRequirements;
		const deUint32				memoryBindIndex		= videoSessionMemoryRequirements[ndx].memoryBindIndex;
		de::MovePtr<Allocation>		alloc				= allocator.allocate(requirements, MemoryRequirement::Any);

		const VkBindVideoSessionMemoryInfoKHR	videoBindMemoryKHR	=
		{
			VK_STRUCTURE_TYPE_BIND_VIDEO_SESSION_MEMORY_INFO_KHR,	//  VkStructureType	sType;
			DE_NULL,												//  const void*		pNext;
			memoryBindIndex,										//  deUint32		memoryBindIndex;
			alloc->getMemory(),										//  VkDeviceMemory	memory;
			alloc->getOffset(),										//  VkDeviceSize	memoryOffset;
			requirements.size,										//  VkDeviceSize	memorySize;
		};

		allocations[ndx] = alloc;

		videoBindsMemoryKHR[ndx] = videoBindMemoryKHR;
	}

	VK_CHECK(vkd.bindVideoSessionMemoryKHR(device, videoSession, static_cast<deUint32>(videoBindsMemoryKHR.size()), videoBindsMemoryKHR.data()));

	return allocations;
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

bool validateVideoProfileList (const InstanceInterface&				vk,
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

	const VkResult res = vk.getPhysicalDeviceImageFormatProperties2(physicalDevice, &imageFormatInfo, &imageFormatProperties);

	if (res != VK_SUCCESS)
		return false;
	else
		return true;

}

VkVideoDecodeH264ProfileInfoKHR getProfileOperationH264D (StdVideoH264ProfileIdc stdProfileIdc, VkVideoDecodeH264PictureLayoutFlagBitsKHR pictureLayout)
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

VkVideoEncodeH264ProfileInfoEXT getProfileOperationH264E (StdVideoH264ProfileIdc stdProfileIdc)
{
	const VkVideoEncodeH264ProfileInfoEXT	videoProfileOperation	=
	{
		VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_PROFILE_INFO_EXT,	//  VkStructureType			sType;
		DE_NULL,												//  const void*				pNext;
		stdProfileIdc,											//  StdVideoH264ProfileIdc	stdProfileIdc;
	};

	return videoProfileOperation;
}

VkVideoDecodeH265ProfileInfoKHR getProfileOperationH265D (StdVideoH265ProfileIdc stdProfileIdc)
{
	const VkVideoDecodeH265ProfileInfoKHR	videoProfileOperation	=
	{
		VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_PROFILE_INFO_KHR,	//  VkStructureType			sType;
		DE_NULL,												//  const void*				pNext;
		stdProfileIdc,											//  StdVideoH265ProfileIdc	stdProfileIdc;
	};

	return videoProfileOperation;
}

VkVideoEncodeH265ProfileInfoEXT getProfileOperationH265E (StdVideoH265ProfileIdc stdProfileIdc)
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

de::MovePtr<StdVideoH264SequenceParameterSet> getStdVideoH264SequenceParameterSet (uint32_t								width,
																				   uint32_t								height,
																				   StdVideoH264SequenceParameterSetVui*	stdVideoH264SequenceParameterSetVui)
{
	const StdVideoH264SpsFlags				stdVideoH264SpsFlags				=
	{
		0u,	//  uint32_t	constraint_set0_flag:1;
		0u,	//  uint32_t	constraint_set1_flag:1;
		0u,	//  uint32_t	constraint_set2_flag:1;
		0u,	//  uint32_t	constraint_set3_flag:1;
		0u,	//  uint32_t	constraint_set4_flag:1;
		0u,	//  uint32_t	constraint_set5_flag:1;
		1u,	//  uint32_t	direct_8x8_inference_flag:1;
		0u,	//  uint32_t	mb_adaptive_frame_field_flag:1;
		1u,	//  uint32_t	frame_mbs_only_flag:1;
		0u,	//  uint32_t	delta_pic_order_always_zero_flag:1;
		0u,	//  uint32_t	separate_colour_plane_flag:1;
		0u,	//  uint32_t	gaps_in_frame_num_value_allowed_flag:1;
		0u,	//  uint32_t	qpprime_y_zero_transform_bypass_flag:1;
		0u,	//  uint32_t	frame_cropping_flag:1;
		0u,	//  uint32_t	seq_scaling_matrix_present_flag:1;
		0u,	//  uint32_t	vui_parameters_present_flag:1;
	};

	const StdVideoH264SequenceParameterSet	stdVideoH264SequenceParameterSet	=
	{
		stdVideoH264SpsFlags,					//  StdVideoH264SpsFlags						flags;
		STD_VIDEO_H264_PROFILE_IDC_BASELINE,	//  StdVideoH264ProfileIdc						profile_idc;
		STD_VIDEO_H264_LEVEL_IDC_4_1,			//  StdVideoH264Level							level_idc;
		STD_VIDEO_H264_CHROMA_FORMAT_IDC_420,	//  StdVideoH264ChromaFormatIdc					chroma_format_idc;
		0u,										//  uint8_t										seq_parameter_set_id;
		0u,										//  uint8_t										bit_depth_luma_minus8;
		0u,										//  uint8_t										bit_depth_chroma_minus8;
		0u,										//  uint8_t										log2_max_frame_num_minus4;
		STD_VIDEO_H264_POC_TYPE_2,				//  StdVideoH264PocType							pic_order_cnt_type;
		0,										//  int32_t										offset_for_non_ref_pic;
		0,										//  int32_t										offset_for_top_to_bottom_field;
		0u,										//  uint8_t										log2_max_pic_order_cnt_lsb_minus4;
		0u,										//  uint8_t										num_ref_frames_in_pic_order_cnt_cycle;
		3u,										//  uint8_t										max_num_ref_frames;
		0u,										//  uint8_t										reserved1;
		(width + 15) / 16 - 1,					//  uint32_t									pic_width_in_mbs_minus1;
		(height + 15) / 16 - 1,					//  uint32_t									pic_height_in_map_units_minus1;
		0u,										//  uint32_t									frame_crop_left_offset;
		0u,										//  uint32_t									frame_crop_right_offset;
		0u,										//  uint32_t									frame_crop_top_offset;
		0u,										//  uint32_t									frame_crop_bottom_offset;
		0u,										//  uint32_t									reserved2;
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
		1u,		//  uint32_t	transform_8x8_mode_flag:1;
		0u,		//  uint32_t	redundant_pic_cnt_present_flag:1;
		0u,		//  uint32_t	constrained_intra_pred_flag:1;
		1u,		//  uint32_t	deblocking_filter_control_present_flag:1;
		0u,		//  uint32_t	weighted_pred_flag:1;
		0u,		//  uint32_4	bottom_field_pic_order_in_frame_present_flag:1;
		1u,		//  uint32_t	entropy_coding_mode_flag:1;
		0u,		//  uint32_t	pic_scaling_matrix_present_flag;
	};

	const StdVideoH264PictureParameterSet	stdVideoH264PictureParameterSet	=
	{
		stdVideoH264PpsFlags,						//  StdVideoH264PpsFlags			flags;
		0u,											//  uint8_t							seq_parameter_set_id;
		0u,											//  uint8_t							pic_parameter_set_id;
		2u,											//  uint8_t							num_ref_idx_l0_default_active_minus1;
		0u,											//  uint8_t							num_ref_idx_l1_default_active_minus1;
		STD_VIDEO_H264_WEIGHTED_BIPRED_IDC_DEFAULT,	//  StdVideoH264WeightedBipredIdc	weighted_bipred_idc;
		-16,										//  int8_t							pic_init_qp_minus26;
		0,											//  int8_t							pic_init_qs_minus26;
		-2,											//  int8_t							chroma_qp_index_offset;
		-2,											//  int8_t							second_chroma_qp_index_offset;
		DE_NULL,									//  const StdVideoH264ScalingLists*	pScalingLists;
	};

	return de::MovePtr<StdVideoH264PictureParameterSet>(new StdVideoH264PictureParameterSet(stdVideoH264PictureParameterSet));
}

} // video
} // vkt
