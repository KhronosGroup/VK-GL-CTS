/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 The Khronos Group Inc.
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
 * \brief Sparse resource operations on transfer queue tests
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
#include "tcuImageCompare.hpp"
#include "tcuTestContext.hpp"
#include "tcuTestLog.hpp"

#include <string>
#include <vector>

using namespace vk;

namespace vkt
{
namespace sparse
{
namespace
{

class SparseResourceTransferQueueCase : public TestCase
{
public:
	SparseResourceTransferQueueCase	(tcu::TestContext&	testCtx,
									 const std::string&	name,
									 const std::string&	description,
									 const ImageType	imageType,
									 const tcu::UVec3&	imageSize,
									 const VkFormat		format);

	TestInstance*	createInstance	(Context&				context) const;
	virtual void	checkSupport	(Context&				context) const;

private:
	const ImageType		m_imageType;
	const tcu::UVec3	m_imageSize;
	const VkFormat		m_format;
};

SparseResourceTransferQueueCase::SparseResourceTransferQueueCase	(tcu::TestContext&	testCtx,
																	 const std::string&	name,
																	 const std::string&	description,
																	 const ImageType	imageType,
																	 const tcu::UVec3&	imageSize,
																	 const VkFormat		format)

	: TestCase(testCtx, name, description)
	, m_imageType(imageType)
	, m_imageSize(imageSize)
	, m_format(format)
{
}

void SparseResourceTransferQueueCase::checkSupport(Context& context) const
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

class SparseResourceTransferQueueInstance : public SparseResourcesBaseInstance
{
public:
	SparseResourceTransferQueueInstance	(Context&			context,
										 const ImageType	imageType,
										 const tcu::UVec3&	imageSize,
										 const VkFormat		format);

	tcu::TestStatus	iterate(void);

private:
	// Test params
	const ImageType		m_imageType;
	const tcu::UVec3	m_imageSize;
	const VkFormat		m_format;

	// Copy
	Move<VkImage>				m_sparseImage;
	std::vector<DeviceMemorySp>	m_deviceMemUniquePtrVec;
	VkImageCreateInfo			m_sparseInfo;

};

SparseResourceTransferQueueInstance::SparseResourceTransferQueueInstance	(Context&			context,
																			 const ImageType	imageType,
																			 const tcu::UVec3&	imageSize,
																			 const VkFormat		format)

	: SparseResourcesBaseInstance(context, false)
	, m_imageType(imageType)
	, m_imageSize(imageSize)
	, m_format(format)
	, m_sparseImage()
	, m_deviceMemUniquePtrVec()
	, m_sparseInfo()
{
}

tcu::TestStatus SparseResourceTransferQueueInstance::iterate(void)
{
	const InstanceInterface& instance = m_context.getInstanceInterface();

	{
		// Create logical device supporting both sparse and compute queues
		QueueRequirementsVec queueRequirements;
		queueRequirements.push_back(QueueRequirements(VK_QUEUE_SPARSE_BINDING_BIT | VK_QUEUE_GRAPHICS_BIT, 1u));
		queueRequirements.push_back(QueueRequirements(VK_QUEUE_COMPUTE_BIT, 1u));

		createDeviceSupportingQueues(queueRequirements);
	}

	const VkPhysicalDevice		physicalDevice	= getPhysicalDevice();
	const DeviceInterface&		deviceInterface	= getDeviceInterface();
	const Queue&				universalQueue	= getQueue(VK_QUEUE_SPARSE_BINDING_BIT | VK_QUEUE_GRAPHICS_BIT, 0);
	const Queue&				transferQueue	= getQueue(VK_QUEUE_COMPUTE_BIT, 0);

	const PlanarFormatDescription	formatDescription	= getPlanarFormatDescription(m_format);

	// Filling sparse image info
	{
		m_sparseInfo.sType					= VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;					//VkStructureType		sType;
		m_sparseInfo.pNext					= DE_NULL;												//const void*			pNext;
		m_sparseInfo.flags					= VK_IMAGE_CREATE_SPARSE_BINDING_BIT;					//VkImageCreateFlags	flags;
		m_sparseInfo.imageType				= mapImageType(m_imageType);							//VkImageType			imageType;
		m_sparseInfo.format					= m_format;												//VkFormat				format;
		m_sparseInfo.extent					= makeExtent3D(getLayerSize(m_imageType, m_imageSize));	//VkExtent3D			extent;
		m_sparseInfo.arrayLayers			= getNumLayers(m_imageType, m_imageSize);				//deUint32				arrayLayers;
		m_sparseInfo.samples				= VK_SAMPLE_COUNT_1_BIT;								//VkSampleCountFlagBits	samples;
		m_sparseInfo.tiling					= VK_IMAGE_TILING_OPTIMAL;								//VkImageTiling			tiling;
		m_sparseInfo.initialLayout			= VK_IMAGE_LAYOUT_UNDEFINED;							//VkImageLayout			initialLayout;
		m_sparseInfo.usage					= VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
											  VK_IMAGE_USAGE_TRANSFER_DST_BIT,						//VkImageUsageFlags		usage;
		m_sparseInfo.sharingMode			= VK_SHARING_MODE_EXCLUSIVE;							//VkSharingMode			sharingMode;
		m_sparseInfo.queueFamilyIndexCount	= 0u;													//deUint32				queueFamilyIndexCount;
		m_sparseInfo.pQueueFamilyIndices	= DE_NULL;												//const deUint32*		pQueueFamilyIndices;

		if (m_imageType == IMAGE_TYPE_CUBE || m_imageType == IMAGE_TYPE_CUBE_ARRAY)
		{
			m_sparseInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		}

		VkImageFormatProperties imageFormatProperties;
		if (instance.getPhysicalDeviceImageFormatProperties(physicalDevice,
			m_sparseInfo.format,
			m_sparseInfo.imageType,
			m_sparseInfo.tiling,
			m_sparseInfo.usage,
			m_sparseInfo.flags,
			&imageFormatProperties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
		{
			TCU_THROW(NotSupportedError, "Image format does not support sparse binding operations");
		}

		m_sparseInfo.mipLevels = getMipmapCount(m_format, formatDescription, imageFormatProperties, m_sparseInfo.extent);
	}

	// Creating and binding sparse image
	m_sparseImage	= createImage(deviceInterface, getDevice(), &m_sparseInfo);
	const Unique<VkSemaphore>	imageMemoryBindSemaphore(createSemaphore(deviceInterface, getDevice()));
	const VkMemoryRequirements	imageMemoryRequirements	= getImageMemoryRequirements(deviceInterface, getDevice(), *m_sparseImage);
	if (imageMemoryRequirements.size > getPhysicalDeviceProperties(instance, getPhysicalDevice()).limits.sparseAddressSpaceSize)
		TCU_THROW(NotSupportedError, "Required memory size for sparse resource exceeds device limits");

	DE_ASSERT((imageMemoryRequirements.size % imageMemoryRequirements.alignment) == 0);

	{
		std::vector<VkSparseMemoryBind>	sparseMemoryBinds;
		const deUint32					numSparseBinds	= static_cast<deUint32>(imageMemoryRequirements.size / imageMemoryRequirements.alignment);
		const deUint32					memoryType		= findMatchingMemoryType(instance, getPhysicalDevice(), imageMemoryRequirements, MemoryRequirement::Any);

		if (memoryType == NO_MATCH_FOUND)
			return tcu::TestStatus::fail("No matching memory type found");

		for (deUint32 sparseBindNdx = 0; sparseBindNdx < numSparseBinds; ++sparseBindNdx)
		{
			const VkSparseMemoryBind sparseMemoryBind	= makeSparseMemoryBind(deviceInterface, getDevice(),
				imageMemoryRequirements.alignment, memoryType, imageMemoryRequirements.alignment * sparseBindNdx);

			m_deviceMemUniquePtrVec.push_back(makeVkSharedPtr(Move<VkDeviceMemory>(check<VkDeviceMemory>(sparseMemoryBind.memory), Deleter<VkDeviceMemory>(deviceInterface, getDevice(), DE_NULL))));

			sparseMemoryBinds.push_back(sparseMemoryBind);
		}

		const VkSparseImageOpaqueMemoryBindInfo opaqueBindInfo = makeSparseImageOpaqueMemoryBindInfo(*m_sparseImage, static_cast<deUint32>(sparseMemoryBinds.size()), sparseMemoryBinds.data());

		const VkBindSparseInfo bindSparseInfo =
		{
			VK_STRUCTURE_TYPE_BIND_SPARSE_INFO,		//VkStructureType							sType;
			DE_NULL,								//const void*								pNext;
			0u,										//deUint32									waitSemaphoreCount;
			DE_NULL,								//const VkSemaphore*						pWaitSemaphores;
			0u,										//deUint32									bufferBindCount;
			DE_NULL,								//const VkSparseBufferMemoryBindInfo*		pBufferBinds;
			1u,										//deUint32									imageOpaqueBindCount;
			&opaqueBindInfo,						//const VkSparseImageOpaqueMemoryBindInfo*	pImageOpaqueBinds;
			0u,										//deUint32									imageBindCount;
			DE_NULL,								//const VkSparseImageMemoryBindInfo*		pImageBinds;
			1u,										//deUint32									signalSemaphoreCount;
			&imageMemoryBindSemaphore.get()			//const VkSemaphore*						pSignalSemaphores;
		};

		// Submit sparse bind commands for execution
		VK_CHECK(deviceInterface.queueBindSparse(universalQueue.queueHandle, 1u, &bindSparseInfo, DE_NULL));
	}

	// Uploading
	deUint32 imageSizeInBytes	= 0;

	for (deUint32 planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
		for (deUint32 mipmapNdx = 0; mipmapNdx < m_sparseInfo.mipLevels; ++mipmapNdx)
			imageSizeInBytes	+= getImageMipLevelSizeInBytes(m_sparseInfo.extent, m_sparseInfo.arrayLayers, formatDescription, planeNdx, mipmapNdx, BUFFER_IMAGE_COPY_OFFSET_GRANULARITY);

	std::vector<VkBufferImageCopy> bufferImageCopy(formatDescription.numPlanes * m_sparseInfo.mipLevels);
	{
		deUint32 bufferOffset = 0;
		for (deUint32 planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
		{
			const VkImageAspectFlags aspect = (formatDescription.numPlanes > 1) ? getPlaneAspect(planeNdx) : VK_IMAGE_ASPECT_COLOR_BIT;

			for (deUint32 mipmapNdx = 0; mipmapNdx < m_sparseInfo.mipLevels; ++mipmapNdx)
			{
				bufferImageCopy[planeNdx * m_sparseInfo.mipLevels + mipmapNdx] =
				{
					bufferOffset,																	//	VkDeviceSize				bufferOffset;
					0u,																				//	deUint32					bufferRowLength;
					0u,																				//	deUint32					bufferImageHeight;
					makeImageSubresourceLayers(aspect, mipmapNdx, 0u, m_sparseInfo.arrayLayers),	//	VkImageSubresourceLayers	imageSubresource;
					makeOffset3D(0, 0, 0),															//	VkOffset3D					imageOffset;
					vk::getPlaneExtent(formatDescription, m_sparseInfo.extent, planeNdx, mipmapNdx)	//	VkExtent3D					imageExtent;
				};
				bufferOffset	+= getImageMipLevelSizeInBytes(m_sparseInfo.extent, m_sparseInfo.arrayLayers, formatDescription, planeNdx, mipmapNdx, BUFFER_IMAGE_COPY_OFFSET_GRANULARITY);
			}
		}
	}

	// Create command buffer for compute and transfer operations
	const Unique<VkCommandPool>		commandPool(makeCommandPool(deviceInterface, getDevice(), transferQueue.queueFamilyIndex));
	const Unique<VkCommandBuffer>	commandBuffer(allocateCommandBuffer(deviceInterface, getDevice(), *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	// Start recording commands
	beginCommandBuffer(deviceInterface, *commandBuffer);

	const VkBufferCreateInfo		inputBufferCreateInfo = makeBufferCreateInfo(imageSizeInBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	const Unique<VkBuffer>			inputBuffer(createBuffer(deviceInterface, getDevice(), &inputBufferCreateInfo));
	const de::UniquePtr<Allocation>	inputBufferAlloc(bindBuffer(deviceInterface, getDevice(), getAllocator(), *inputBuffer, MemoryRequirement::HostVisible));

	std::vector<deUint8>			referenceData(imageSizeInBytes);
	for (deUint32 valueNdx = 0; valueNdx < imageSizeInBytes; ++valueNdx)
	{
		referenceData[valueNdx]	= static_cast<deUint8>((valueNdx % imageMemoryRequirements.alignment) + 1u);
	}

	{
		deMemcpy(inputBufferAlloc->getHostPtr(), referenceData.data(), imageSizeInBytes);
		flushAlloc(deviceInterface, getDevice(), *inputBufferAlloc);

		const VkBufferMemoryBarrier inputBufferBarrier = makeBufferMemoryBarrier(
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

			imageSparseTransferDstBarriers.push_back(makeImageMemoryBarrier(
				0u,
				VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				*m_sparseImage,
				makeImageSubresourceRange(aspect, 0u, m_sparseInfo.mipLevels, 0u, m_sparseInfo.arrayLayers),
				universalQueue.queueFamilyIndex	!= transferQueue.queueFamilyIndex ? universalQueue.queueFamilyIndex	: VK_QUEUE_FAMILY_IGNORED,
				universalQueue.queueFamilyIndex	!= transferQueue.queueFamilyIndex ? transferQueue.queueFamilyIndex	: VK_QUEUE_FAMILY_IGNORED
			));
		}
		deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, static_cast<deUint32>(imageSparseTransferDstBarriers.size()), imageSparseTransferDstBarriers.data());
	}

	deviceInterface.cmdCopyBufferToImage(*commandBuffer, *inputBuffer, *m_sparseImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<deUint32>(bufferImageCopy.size()), bufferImageCopy.data());

	const VkBufferCreateInfo		outputBufferCreateInfo	= makeBufferCreateInfo(imageSizeInBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const Unique<VkBuffer>			outputBuffer(createBuffer(deviceInterface, getDevice(), &outputBufferCreateInfo));
	const de::UniquePtr<Allocation>	outputBufferAlloc(bindBuffer(deviceInterface, getDevice(), getAllocator(), *outputBuffer, MemoryRequirement::HostVisible));
	// Reading back from sparse image
	{
		std::vector<VkImageMemoryBarrier> imageSparseTransferSrcBarriers;

		for (deUint32 planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
		{
			const VkImageAspectFlags aspect = (formatDescription.numPlanes > 1) ? getPlaneAspect(planeNdx) : VK_IMAGE_ASPECT_COLOR_BIT;

			imageSparseTransferSrcBarriers.push_back(makeImageMemoryBarrier(
				VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_ACCESS_TRANSFER_READ_BIT,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				*m_sparseImage,
				makeImageSubresourceRange(aspect, 0u, m_sparseInfo.mipLevels, 0u, m_sparseInfo.arrayLayers)
			));
		}

		deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, static_cast<deUint32>(imageSparseTransferSrcBarriers.size()), imageSparseTransferSrcBarriers.data());
		deviceInterface.cmdCopyImageToBuffer(*commandBuffer, *m_sparseImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *outputBuffer, static_cast<deUint32>(bufferImageCopy.size()), bufferImageCopy.data());
	}

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
	submitCommandsAndWait(deviceInterface, getDevice(), transferQueue.queueHandle, *commandBuffer, 1u, &imageMemoryBindSemaphore.get(), stageBits, 0, DE_NULL);

	// Retrieve data from buffer to host memory
	invalidateAlloc(deviceInterface, getDevice(), *outputBufferAlloc);

	// Wait for sparse queue to become idle
	deviceInterface.queueWaitIdle(universalQueue.queueHandle);

	const deUint8*	outputData		= static_cast<const deUint8*>(outputBufferAlloc->getHostPtr());
	bool			ignoreLsb6Bits	= areLsb6BitsDontCare(m_sparseInfo.format);
	bool			ignoreLsb4Bits	= areLsb4BitsDontCare(m_sparseInfo.format);

	for (deUint32 planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
	{
		for (deUint32 mipmapNdx = 0; mipmapNdx < m_sparseInfo.mipLevels; ++mipmapNdx)
		{
			const deUint32 mipLevelSizeInBytes	= getImageMipLevelSizeInBytes(m_sparseInfo.extent, m_sparseInfo.arrayLayers, formatDescription, planeNdx, mipmapNdx);
			const deUint32 bufferOffset			= static_cast<deUint32>(bufferImageCopy[planeNdx * m_sparseInfo.mipLevels + mipmapNdx].bufferOffset);

			// Validate results
			for (size_t byteNdx = 0; byteNdx < mipLevelSizeInBytes; byteNdx++)
			{
				const deUint8	res = *(outputData + bufferOffset + byteNdx);
				const deUint8	ref = referenceData[bufferOffset + byteNdx];

				deUint8 mask = 0xFF;

				if (!(byteNdx & 0x01) && (ignoreLsb6Bits))
					mask = 0xC0;
				else if (!(byteNdx & 0x01) && (ignoreLsb4Bits))
					mask = 0xF0;

				if ((res & mask) != (ref & mask))
				{
					return tcu::TestStatus::fail("Failed");
				}
			}
		}
	}

	return tcu::TestStatus::pass("Passed");
}

TestInstance* SparseResourceTransferQueueCase::createInstance(Context& context) const
{
	return new SparseResourceTransferQueueInstance(context, m_imageType, m_imageSize, m_format);
}

} // anonymous ns

tcu::TestCaseGroup* createTransferQueueTests(tcu::TestContext& testCtx)
{
	const std::vector<TestImageParameters> imageParameters
	{
		{ IMAGE_TYPE_1D,			{ tcu::UVec3(512u, 1u,   1u),	tcu::UVec3(1024u, 1u,   1u),	tcu::UVec3(11u,  1u,   1u) },	getTestFormats(IMAGE_TYPE_1D)			},
		{ IMAGE_TYPE_1D_ARRAY,		{ tcu::UVec3(512u, 1u,   64u),	tcu::UVec3(1024u, 1u,   8u),	tcu::UVec3(11u,  1u,   3u) },	getTestFormats(IMAGE_TYPE_1D_ARRAY)		},
		{ IMAGE_TYPE_2D,			{ tcu::UVec3(512u, 256u, 1u),	tcu::UVec3(1024u, 128u, 1u),	tcu::UVec3(11u,  137u, 1u) },	getTestFormats(IMAGE_TYPE_2D)			},
		{ IMAGE_TYPE_2D_ARRAY,		{ tcu::UVec3(512u, 256u, 6u),	tcu::UVec3(1024u, 128u, 8u),	tcu::UVec3(11u,  137u, 3u) },	getTestFormats(IMAGE_TYPE_2D_ARRAY)		},
		{ IMAGE_TYPE_3D,			{ tcu::UVec3(512u, 256u, 6u),	tcu::UVec3(1024u, 128u, 8u),	tcu::UVec3(11u,  137u, 3u) },	getTestFormats(IMAGE_TYPE_3D)			},
		{ IMAGE_TYPE_CUBE,			{ tcu::UVec3(256u, 256u, 1u),	tcu::UVec3(128u,  128u, 1u),	tcu::UVec3(137u, 137u, 1u) },	getTestFormats(IMAGE_TYPE_CUBE)			},
		{ IMAGE_TYPE_CUBE_ARRAY,	{ tcu::UVec3(256u, 256u, 6u),	tcu::UVec3(128u,  128u, 8u),	tcu::UVec3(137u, 137u, 3u) },	getTestFormats(IMAGE_TYPE_CUBE_ARRAY)	}
	};

	de::MovePtr<tcu::TestCaseGroup> transferGroup(new tcu::TestCaseGroup(testCtx, "transfer_queue", "Sparse resources on transfer queue operation tests."));

	for (size_t imageTypeNdx = 0; imageTypeNdx < imageParameters.size(); ++imageTypeNdx)
	{
		const ImageType					imageType	= imageParameters[imageTypeNdx].imageType;
		de::MovePtr<tcu::TestCaseGroup>	imageTypeGroup(new tcu::TestCaseGroup(testCtx, getImageTypeName(imageType).c_str(), ""));

		for (size_t formatNdx = 0; formatNdx < imageParameters[imageTypeNdx].formats.size(); ++formatNdx)
		{
			VkFormat						format = imageParameters[imageTypeNdx].formats[formatNdx].format;
			tcu::UVec3						imageSizeAlignment = getImageSizeAlignment(format);
			de::MovePtr<tcu::TestCaseGroup>	formatGroup(new tcu::TestCaseGroup(testCtx, getImageFormatID(format).c_str(), ""));

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

				formatGroup->addChild(new SparseResourceTransferQueueCase(testCtx, stream.str(), "", imageType, imageSize, format));
			}
			imageTypeGroup->addChild(formatGroup.release());
		}
		transferGroup->addChild(imageTypeGroup.release());
	}

	return transferGroup.release();
}

} // sparse
} // vkt
