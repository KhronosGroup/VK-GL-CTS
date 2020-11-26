/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \file  vktSparseResourcesImageSparseBinding.cpp
 * \brief Sparse fully resident images with mipmaps tests
 *//*--------------------------------------------------------------------*/

#include "vktSparseResourcesBufferSparseBinding.hpp"
#include "vktSparseResourcesTestsUtil.hpp"
#include "vktSparseResourcesBase.hpp"
#include "vktTestCaseUtil.hpp"

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkMemUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"
#include "tcuTextureUtil.hpp"

#include <string>
#include <vector>

using namespace vk;

namespace vkt
{
namespace sparse
{
namespace
{

class ImageSparseBindingCase : public TestCase
{
public:
	ImageSparseBindingCase			(tcu::TestContext&	testCtx,
									 const std::string&	name,
									 const std::string&	description,
									 const ImageType	imageType,
									 const tcu::UVec3&	imageSize,
									 const VkFormat		format,
									 const bool			useDeviceGroups = false);

	TestInstance*	createInstance	(Context&			context) const;
	virtual void	checkSupport			(Context&					context) const;

private:
	const bool			m_useDeviceGroups;
	const ImageType		m_imageType;
	const tcu::UVec3	m_imageSize;
	const VkFormat		m_format;
};

ImageSparseBindingCase::ImageSparseBindingCase (tcu::TestContext&	testCtx,
												const std::string&	name,
												const std::string&	description,
												const ImageType		imageType,
												const tcu::UVec3&	imageSize,
												const VkFormat		format,
												const bool			useDeviceGroups)

	: TestCase			(testCtx, name, description)
	, m_useDeviceGroups	(useDeviceGroups)
	, m_imageType		(imageType)
	, m_imageSize		(imageSize)
	, m_format			(format)
{
}

void ImageSparseBindingCase::checkSupport (Context& context) const
{
	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SPARSE_BINDING);

	if (!isImageSizeSupported(context.getInstanceInterface(), context.getPhysicalDevice(), m_imageType, m_imageSize))
		TCU_THROW(NotSupportedError, "Image size not supported for device");

	if (formatIsR64(m_format))
	{
		context.requireDeviceFunctionality("VK_EXT_shader_image_atomic_int64");

		if (context.getShaderImageAtomicInt64FeaturesEXT().sparseImageInt64Atomics == VK_FALSE)
		{
			TCU_THROW(NotSupportedError, "sparseImageInt64Atomics is not supported for device");
		}
	}
}

class ImageSparseBindingInstance : public SparseResourcesBaseInstance
{
public:
	ImageSparseBindingInstance	(Context&			context,
								 const ImageType	imageType,
								 const tcu::UVec3&	imageSize,
								 const VkFormat		format,
								 const bool			useDeviceGroups);

	tcu::TestStatus	iterate		(void);

private:
	const bool			m_useDeviceGroups;
	const ImageType		m_imageType;
	const tcu::UVec3	m_imageSize;
	const VkFormat		m_format;
};

ImageSparseBindingInstance::ImageSparseBindingInstance (Context&			context,
														const ImageType		imageType,
														const tcu::UVec3&	imageSize,
														const VkFormat		format,
														const bool			useDeviceGroups)

	: SparseResourcesBaseInstance	(context, useDeviceGroups)
	, m_useDeviceGroups				(useDeviceGroups)
	, m_imageType					(imageType)
	, m_imageSize					(imageSize)
	, m_format						(format)
{
}

tcu::TestStatus ImageSparseBindingInstance::iterate (void)
{
	const InstanceInterface&	instance		= m_context.getInstanceInterface();

	{
		// Create logical device supporting both sparse and compute queues
		QueueRequirementsVec queueRequirements;
		queueRequirements.push_back(QueueRequirements(VK_QUEUE_SPARSE_BINDING_BIT, 1u));
		queueRequirements.push_back(QueueRequirements(VK_QUEUE_COMPUTE_BIT, 1u));

		createDeviceSupportingQueues(queueRequirements);
	}

	const VkPhysicalDevice		physicalDevice	= getPhysicalDevice();
	VkImageCreateInfo			imageSparseInfo;
	std::vector<DeviceMemorySp>	deviceMemUniquePtrVec;

	const DeviceInterface&			deviceInterface		= getDeviceInterface();
	const Queue&					sparseQueue			= getQueue(VK_QUEUE_SPARSE_BINDING_BIT, 0);
	const Queue&					computeQueue		= getQueue(VK_QUEUE_COMPUTE_BIT, 0);
	const PlanarFormatDescription	formatDescription	= getPlanarFormatDescription(m_format);

	// Go through all physical devices
	for (deUint32 physDevID = 0; physDevID < m_numPhysicalDevices; ++physDevID)
	{
		const deUint32	firstDeviceID	= physDevID;
		const deUint32	secondDeviceID	= (firstDeviceID + 1) % m_numPhysicalDevices;

		imageSparseInfo.sType					= VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;					//VkStructureType		sType;
		imageSparseInfo.pNext					= DE_NULL;												//const void*			pNext;
		imageSparseInfo.flags					= VK_IMAGE_CREATE_SPARSE_BINDING_BIT;					//VkImageCreateFlags	flags;
		imageSparseInfo.imageType				= mapImageType(m_imageType);							//VkImageType			imageType;
		imageSparseInfo.format					= m_format;												//VkFormat				format;
		imageSparseInfo.extent					= makeExtent3D(getLayerSize(m_imageType, m_imageSize));	//VkExtent3D			extent;
		imageSparseInfo.arrayLayers				= getNumLayers(m_imageType, m_imageSize);				//deUint32				arrayLayers;
		imageSparseInfo.samples					= VK_SAMPLE_COUNT_1_BIT;								//VkSampleCountFlagBits	samples;
		imageSparseInfo.tiling					= VK_IMAGE_TILING_OPTIMAL;								//VkImageTiling			tiling;
		imageSparseInfo.initialLayout			= VK_IMAGE_LAYOUT_UNDEFINED;							//VkImageLayout			initialLayout;
		imageSparseInfo.usage					= VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
												  VK_IMAGE_USAGE_TRANSFER_DST_BIT;						//VkImageUsageFlags		usage;
		imageSparseInfo.sharingMode				= VK_SHARING_MODE_EXCLUSIVE;							//VkSharingMode			sharingMode;
		imageSparseInfo.queueFamilyIndexCount	= 0u;													//deUint32				queueFamilyIndexCount;
		imageSparseInfo.pQueueFamilyIndices		= DE_NULL;												//const deUint32*		pQueueFamilyIndices;

		if (m_imageType == IMAGE_TYPE_CUBE || m_imageType == IMAGE_TYPE_CUBE_ARRAY)
		{
			imageSparseInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		}

		{
			VkImageFormatProperties imageFormatProperties;
			if (instance.getPhysicalDeviceImageFormatProperties(physicalDevice,
				imageSparseInfo.format,
				imageSparseInfo.imageType,
				imageSparseInfo.tiling,
				imageSparseInfo.usage,
				imageSparseInfo.flags,
				&imageFormatProperties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
			{
				TCU_THROW(NotSupportedError, "Image format does not support sparse binding operations");
			}

			imageSparseInfo.mipLevels = getMipmapCount(m_format, formatDescription, imageFormatProperties, imageSparseInfo.extent);
		}

		// Create sparse image
		const Unique<VkImage> imageSparse(createImage(deviceInterface, getDevice(), &imageSparseInfo));

		// Create sparse image memory bind semaphore
		const Unique<VkSemaphore> imageMemoryBindSemaphore(createSemaphore(deviceInterface, getDevice()));

		// Get sparse image general memory requirements
		const VkMemoryRequirements imageMemoryRequirements = getImageMemoryRequirements(deviceInterface, getDevice(), *imageSparse);

		// Check if required image memory size does not exceed device limits
		if (imageMemoryRequirements.size > getPhysicalDeviceProperties(instance, getPhysicalDevice(secondDeviceID)).limits.sparseAddressSpaceSize)
			TCU_THROW(NotSupportedError, "Required memory size for sparse resource exceeds device limits");

		DE_ASSERT((imageMemoryRequirements.size % imageMemoryRequirements.alignment) == 0);

		{
			std::vector<VkSparseMemoryBind>	sparseMemoryBinds;
			const deUint32					numSparseBinds	= static_cast<deUint32>(imageMemoryRequirements.size / imageMemoryRequirements.alignment);
			const deUint32					memoryType		= findMatchingMemoryType(instance, getPhysicalDevice(secondDeviceID), imageMemoryRequirements, MemoryRequirement::Any);

			if (memoryType == NO_MATCH_FOUND)
				return tcu::TestStatus::fail("No matching memory type found");

			if (firstDeviceID != secondDeviceID)
			{
				VkPeerMemoryFeatureFlags	peerMemoryFeatureFlags = (VkPeerMemoryFeatureFlags)0;
				const deUint32				heapIndex = getHeapIndexForMemoryType(instance, getPhysicalDevice(secondDeviceID), memoryType);
				deviceInterface.getDeviceGroupPeerMemoryFeatures(getDevice(), heapIndex, firstDeviceID, secondDeviceID, &peerMemoryFeatureFlags);

				if (((peerMemoryFeatureFlags & VK_PEER_MEMORY_FEATURE_COPY_SRC_BIT) == 0) ||
					((peerMemoryFeatureFlags & VK_PEER_MEMORY_FEATURE_COPY_DST_BIT) == 0))
				{
					TCU_THROW(NotSupportedError, "Peer memory does not support COPY_SRC and COPY_DST");
				}
			}

			for (deUint32 sparseBindNdx = 0; sparseBindNdx < numSparseBinds; ++sparseBindNdx)
			{
				const VkSparseMemoryBind sparseMemoryBind = makeSparseMemoryBind(deviceInterface, getDevice(),
					imageMemoryRequirements.alignment, memoryType, imageMemoryRequirements.alignment * sparseBindNdx);

				deviceMemUniquePtrVec.push_back(makeVkSharedPtr(Move<VkDeviceMemory>(check<VkDeviceMemory>(sparseMemoryBind.memory), Deleter<VkDeviceMemory>(deviceInterface, getDevice(), DE_NULL))));

				sparseMemoryBinds.push_back(sparseMemoryBind);
			}

			const VkSparseImageOpaqueMemoryBindInfo opaqueBindInfo = makeSparseImageOpaqueMemoryBindInfo(*imageSparse, static_cast<deUint32>(sparseMemoryBinds.size()), sparseMemoryBinds.data());

			const VkDeviceGroupBindSparseInfo devGroupBindSparseInfo =
			{
				VK_STRUCTURE_TYPE_DEVICE_GROUP_BIND_SPARSE_INFO_KHR,	//VkStructureType							sType;
				DE_NULL,												//const void*								pNext;
				firstDeviceID,											//deUint32									resourceDeviceIndex;
				secondDeviceID,											//deUint32									memoryDeviceIndex;
			};

			const VkBindSparseInfo bindSparseInfo =
			{
				VK_STRUCTURE_TYPE_BIND_SPARSE_INFO,						//VkStructureType							sType;
				m_useDeviceGroups ? &devGroupBindSparseInfo : DE_NULL,	//const void*								pNext;
				0u,														//deUint32									waitSemaphoreCount;
				DE_NULL,												//const VkSemaphore*						pWaitSemaphores;
				0u,														//deUint32									bufferBindCount;
				DE_NULL,												//const VkSparseBufferMemoryBindInfo*		pBufferBinds;
				1u,														//deUint32									imageOpaqueBindCount;
				&opaqueBindInfo,										//const VkSparseImageOpaqueMemoryBindInfo*	pImageOpaqueBinds;
				0u,														//deUint32									imageBindCount;
				DE_NULL,												//const VkSparseImageMemoryBindInfo*		pImageBinds;
				1u,														//deUint32									signalSemaphoreCount;
				&imageMemoryBindSemaphore.get()							//const VkSemaphore*						pSignalSemaphores;
			};

			// Submit sparse bind commands for execution
			VK_CHECK(deviceInterface.queueBindSparse(sparseQueue.queueHandle, 1u, &bindSparseInfo, DE_NULL));
		}

		deUint32 imageSizeInBytes = 0;

		for (deUint32 planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
			for (deUint32 mipmapNdx = 0; mipmapNdx < imageSparseInfo.mipLevels; ++mipmapNdx)
				imageSizeInBytes += getImageMipLevelSizeInBytes(imageSparseInfo.extent, imageSparseInfo.arrayLayers, formatDescription, planeNdx, mipmapNdx, BUFFER_IMAGE_COPY_OFFSET_GRANULARITY);

		std::vector<VkBufferImageCopy> bufferImageCopy(formatDescription.numPlanes * imageSparseInfo.mipLevels);
		{
			deUint32 bufferOffset = 0;
			for (deUint32 planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
			{
				const VkImageAspectFlags aspect = (formatDescription.numPlanes > 1) ? getPlaneAspect(planeNdx) : VK_IMAGE_ASPECT_COLOR_BIT;

				for (deUint32 mipmapNdx = 0; mipmapNdx < imageSparseInfo.mipLevels; ++mipmapNdx)
				{
					bufferImageCopy[planeNdx*imageSparseInfo.mipLevels + mipmapNdx] =
					{
						bufferOffset,																		//	VkDeviceSize				bufferOffset;
						0u,																					//	deUint32					bufferRowLength;
						0u,																					//	deUint32					bufferImageHeight;
						makeImageSubresourceLayers(aspect, mipmapNdx, 0u, imageSparseInfo.arrayLayers),		//	VkImageSubresourceLayers	imageSubresource;
						makeOffset3D(0, 0, 0),																//	VkOffset3D					imageOffset;
						vk::getPlaneExtent(formatDescription, imageSparseInfo.extent, planeNdx, mipmapNdx)	//	VkExtent3D					imageExtent;
					};
					bufferOffset += getImageMipLevelSizeInBytes(imageSparseInfo.extent, imageSparseInfo.arrayLayers, formatDescription, planeNdx, mipmapNdx, BUFFER_IMAGE_COPY_OFFSET_GRANULARITY);
				}
			}
		}

		// Create command buffer for compute and transfer operations
		const Unique<VkCommandPool>		commandPool(makeCommandPool(deviceInterface, getDevice(), computeQueue.queueFamilyIndex));
		const Unique<VkCommandBuffer>	commandBuffer(allocateCommandBuffer(deviceInterface, getDevice(), *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

		// Start recording commands
		beginCommandBuffer(deviceInterface, *commandBuffer);

		const VkBufferCreateInfo		inputBufferCreateInfo	= makeBufferCreateInfo(imageSizeInBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
		const Unique<VkBuffer>			inputBuffer				(createBuffer(deviceInterface, getDevice(), &inputBufferCreateInfo));
		const de::UniquePtr<Allocation>	inputBufferAlloc		(bindBuffer(deviceInterface, getDevice(), getAllocator(), *inputBuffer, MemoryRequirement::HostVisible));

		std::vector<deUint8>			referenceData(imageSizeInBytes);
		for (deUint32 valueNdx = 0; valueNdx < imageSizeInBytes; ++valueNdx)
		{
			referenceData[valueNdx] = static_cast<deUint8>((valueNdx % imageMemoryRequirements.alignment) + 1u);
		}

		{
			deMemcpy(inputBufferAlloc->getHostPtr(), referenceData.data(), imageSizeInBytes);
			flushAlloc(deviceInterface, getDevice(), *inputBufferAlloc);

			const VkBufferMemoryBarrier inputBufferBarrier = makeBufferMemoryBarrier (
				VK_ACCESS_HOST_WRITE_BIT,
				VK_ACCESS_TRANSFER_READ_BIT,
				*inputBuffer,
				0u,
				imageSizeInBytes
			);
			deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 1u, &inputBufferBarrier, 0u, DE_NULL);
		}

		{
			std::vector<VkImageMemoryBarrier> imageSparseTransferDstBarriers;

			for (deUint32 planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
			{
				const VkImageAspectFlags aspect = (formatDescription.numPlanes > 1) ? getPlaneAspect(planeNdx) : VK_IMAGE_ASPECT_COLOR_BIT;

				imageSparseTransferDstBarriers.push_back( makeImageMemoryBarrier (
					0u,
					VK_ACCESS_TRANSFER_WRITE_BIT,
					VK_IMAGE_LAYOUT_UNDEFINED,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					*imageSparse,
					makeImageSubresourceRange(aspect, 0u, imageSparseInfo.mipLevels, 0u, imageSparseInfo.arrayLayers),
					sparseQueue.queueFamilyIndex != computeQueue.queueFamilyIndex ? sparseQueue.queueFamilyIndex : VK_QUEUE_FAMILY_IGNORED,
					sparseQueue.queueFamilyIndex != computeQueue.queueFamilyIndex ? computeQueue.queueFamilyIndex : VK_QUEUE_FAMILY_IGNORED
				));
			}
			deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, static_cast<deUint32>(imageSparseTransferDstBarriers.size()), imageSparseTransferDstBarriers.data());
		}

		deviceInterface.cmdCopyBufferToImage(*commandBuffer, *inputBuffer, *imageSparse, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<deUint32>(bufferImageCopy.size()), bufferImageCopy.data());

		{
			std::vector<VkImageMemoryBarrier> imageSparseTransferSrcBarriers;

			for (deUint32 planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
			{
				const VkImageAspectFlags aspect = (formatDescription.numPlanes > 1) ? getPlaneAspect(planeNdx) : VK_IMAGE_ASPECT_COLOR_BIT;

				imageSparseTransferSrcBarriers.push_back( makeImageMemoryBarrier (
					VK_ACCESS_TRANSFER_WRITE_BIT,
					VK_ACCESS_TRANSFER_READ_BIT,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					*imageSparse,
					makeImageSubresourceRange(aspect, 0u, imageSparseInfo.mipLevels, 0u, imageSparseInfo.arrayLayers)
				));
			}

			deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, static_cast<deUint32>(imageSparseTransferSrcBarriers.size()), imageSparseTransferSrcBarriers.data());
		}

		const VkBufferCreateInfo		outputBufferCreateInfo	= makeBufferCreateInfo(imageSizeInBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
		const Unique<VkBuffer>			outputBuffer			(createBuffer(deviceInterface, getDevice(), &outputBufferCreateInfo));
		const de::UniquePtr<Allocation>	outputBufferAlloc		(bindBuffer(deviceInterface, getDevice(), getAllocator(), *outputBuffer, MemoryRequirement::HostVisible));

		deviceInterface.cmdCopyImageToBuffer(*commandBuffer, *imageSparse, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *outputBuffer, static_cast<deUint32>(bufferImageCopy.size()), bufferImageCopy.data());

		{
			const VkBufferMemoryBarrier outputBufferBarrier = makeBufferMemoryBarrier
			(
				VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_ACCESS_HOST_READ_BIT,
				*outputBuffer,
				0u,
				imageSizeInBytes
			);

			deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, 1u, &outputBufferBarrier, 0u, DE_NULL);
		}

		// End recording commands
		endCommandBuffer(deviceInterface, *commandBuffer);

		const VkPipelineStageFlags stageBits[] = { VK_PIPELINE_STAGE_TRANSFER_BIT };

		// Submit commands for execution and wait for completion
		submitCommandsAndWait(deviceInterface, getDevice(), computeQueue.queueHandle, *commandBuffer, 1u, &imageMemoryBindSemaphore.get(), stageBits,
			0, DE_NULL, m_useDeviceGroups, firstDeviceID);

		// Retrieve data from buffer to host memory
		invalidateAlloc(deviceInterface, getDevice(), *outputBufferAlloc);

		// Wait for sparse queue to become idle
		deviceInterface.queueWaitIdle(sparseQueue.queueHandle);

		const deUint8* outputData = static_cast<const deUint8*>(outputBufferAlloc->getHostPtr());

		for (deUint32 planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
		{
			for (deUint32 mipmapNdx = 0; mipmapNdx < imageSparseInfo.mipLevels; ++mipmapNdx)
			{
				const deUint32	mipLevelSizeInBytes		= getImageMipLevelSizeInBytes(imageSparseInfo.extent, imageSparseInfo.arrayLayers, formatDescription, planeNdx, mipmapNdx);
				const deUint32	bufferOffset			= static_cast<deUint32>(bufferImageCopy[ planeNdx * imageSparseInfo.mipLevels + mipmapNdx].bufferOffset);
				bool			is8bitSnormComponent	= false;

				// Validate results
				for (deUint32 channelNdx = 0; channelNdx < 4; ++channelNdx)
				{
					if (!formatDescription.hasChannelNdx(channelNdx))
						continue;

					if ((formatDescription.channels[channelNdx].type == tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT) &&
						(formatDescription.channels[channelNdx].sizeBits == 8))
					{
						is8bitSnormComponent = true;
						break;
					}
				}

				if (!is8bitSnormComponent)
				{
					if (deMemCmp(outputData + bufferOffset, &referenceData[bufferOffset], mipLevelSizeInBytes) != 0)
						return tcu::TestStatus::fail("Failed");
				}
				else
				{
					for (deUint32 byte = 0; byte < mipLevelSizeInBytes; byte++)
					{
						deUint32 entryOffset = bufferOffset + byte;

						// Ignore 0x80 which is undefined data for a 8 bit snorm component
						if ((referenceData[entryOffset] != 0x80) && (deMemCmp(outputData + entryOffset, &referenceData[entryOffset], 1) != 0))
							return tcu::TestStatus::fail("Failed");
					}
				}
			}
		}
	}

	return tcu::TestStatus::pass("Passed");
}

TestInstance* ImageSparseBindingCase::createInstance (Context& context) const
{
	return new ImageSparseBindingInstance(context, m_imageType, m_imageSize, m_format, m_useDeviceGroups);
}

} // anonymous ns

tcu::TestCaseGroup* createImageSparseBindingTestsCommon(tcu::TestContext& testCtx, de::MovePtr<tcu::TestCaseGroup> testGroup, const bool useDeviceGroup = false)
{
	const std::vector<TestImageParameters> imageParameters =
	{
		{ IMAGE_TYPE_1D,			{ tcu::UVec3(512u, 1u,   1u ),	tcu::UVec3(1024u, 1u,   1u),	tcu::UVec3(11u,  1u,   1u) },	getTestFormats(IMAGE_TYPE_1D) },
		{ IMAGE_TYPE_1D_ARRAY,		{ tcu::UVec3(512u, 1u,   64u),	tcu::UVec3(1024u, 1u,   8u),	tcu::UVec3(11u,  1u,   3u) },	getTestFormats(IMAGE_TYPE_1D_ARRAY) },
		{ IMAGE_TYPE_2D,			{ tcu::UVec3(512u, 256u, 1u ),	tcu::UVec3(1024u, 128u, 1u),	tcu::UVec3(11u,  137u, 1u) },	getTestFormats(IMAGE_TYPE_2D) },
		{ IMAGE_TYPE_2D_ARRAY,		{ tcu::UVec3(512u, 256u, 6u ),	tcu::UVec3(1024u, 128u, 8u),	tcu::UVec3(11u,  137u, 3u) },	getTestFormats(IMAGE_TYPE_2D_ARRAY) },
		{ IMAGE_TYPE_3D,			{ tcu::UVec3(512u, 256u, 6u ),	tcu::UVec3(1024u, 128u, 8u),	tcu::UVec3(11u,  137u, 3u) },	getTestFormats(IMAGE_TYPE_3D) },
		{ IMAGE_TYPE_CUBE,			{ tcu::UVec3(256u, 256u, 1u ),	tcu::UVec3(128u,  128u, 1u),	tcu::UVec3(137u, 137u, 1u) },	getTestFormats(IMAGE_TYPE_CUBE) },
		{ IMAGE_TYPE_CUBE_ARRAY,	{ tcu::UVec3(256u, 256u, 6u ),	tcu::UVec3(128u,  128u, 8u),	tcu::UVec3(137u, 137u, 3u) },	getTestFormats(IMAGE_TYPE_CUBE_ARRAY) }
	};

	for (size_t imageTypeNdx = 0; imageTypeNdx < imageParameters.size(); ++imageTypeNdx)
	{
		const ImageType					imageType		= imageParameters[imageTypeNdx].imageType;
		de::MovePtr<tcu::TestCaseGroup> imageTypeGroup	(new tcu::TestCaseGroup(testCtx, getImageTypeName(imageType).c_str(), ""));

		for (size_t formatNdx = 0; formatNdx < imageParameters[imageTypeNdx].formats.size(); ++formatNdx)
		{
			VkFormat						format				= imageParameters[imageTypeNdx].formats[formatNdx].format;
			tcu::UVec3						imageSizeAlignment	= getImageSizeAlignment(format);
			de::MovePtr<tcu::TestCaseGroup> formatGroup			(new tcu::TestCaseGroup(testCtx, getImageFormatID(format).c_str(), ""));

			for (size_t imageSizeNdx = 0; imageSizeNdx < imageParameters[imageTypeNdx].imageSizes.size(); ++imageSizeNdx)
			{
				const tcu::UVec3 imageSize = imageParameters[imageTypeNdx].imageSizes[imageSizeNdx];

				// skip test for images with odd sizes for some YCbCr formats
				if ((imageSize.x() % imageSizeAlignment.x()) != 0)
					continue;
				if ((imageSize.y() % imageSizeAlignment.y()) != 0)
					continue;

				std::ostringstream	stream;
				stream << imageSize.x() << "_" << imageSize.y() << "_" << imageSize.z();

				formatGroup->addChild(new ImageSparseBindingCase(testCtx, stream.str(), "", imageType, imageSize, format, useDeviceGroup));
			}
			imageTypeGroup->addChild(formatGroup.release());
		}
		testGroup->addChild(imageTypeGroup.release());
	}

	return testGroup.release();
}

tcu::TestCaseGroup* createImageSparseBindingTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "image_sparse_binding", "Image Sparse Binding"));
	return createImageSparseBindingTestsCommon(testCtx, testGroup);
}

tcu::TestCaseGroup* createDeviceGroupImageSparseBindingTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "device_group_image_sparse_binding", "Device Group Image Sparse Binding"));
	return createImageSparseBindingTestsCommon(testCtx, testGroup, true);
}

} // sparse
} // vkt
