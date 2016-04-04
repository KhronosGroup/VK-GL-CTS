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
#include "vkBuilderUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"

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
					ImageSparseBindingCase	(tcu::TestContext&			testCtx,
											 const std::string&			name,
											 const std::string&			description,
											 const ImageType			imageType,
											 const tcu::UVec3&			imageSize,
											 const tcu::TextureFormat&	format);

	TestInstance*	createInstance			(Context&			context) const;

private:
	const ImageType				m_imageType;
	const tcu::UVec3			m_imageSize;
	const tcu::TextureFormat	m_format;
};

ImageSparseBindingCase::ImageSparseBindingCase (tcu::TestContext&			testCtx,
												const std::string&			name,
												const std::string&			description,
												const ImageType				imageType,
												const tcu::UVec3&			imageSize,
												const tcu::TextureFormat&	format)
	: TestCase				(testCtx, name, description)
	, m_imageType			(imageType)
	, m_imageSize			(imageSize)
	, m_format				(format)
{
}

class ImageSparseBindingInstance : public SparseResourcesBaseInstance
{
public:
					ImageSparseBindingInstance	(Context&					context,
												 const ImageType			imageType,
												 const tcu::UVec3&			imageSize,
												 const tcu::TextureFormat&	format);

	tcu::TestStatus	iterate						(void);

private:
	const ImageType				m_imageType;
	const tcu::UVec3			m_imageSize;
	const tcu::TextureFormat	m_format;
};

ImageSparseBindingInstance::ImageSparseBindingInstance (Context&					context,
														const ImageType				imageType,
														const tcu::UVec3&			imageSize,
														const tcu::TextureFormat&	format)
	: SparseResourcesBaseInstance	(context)
	, m_imageType					(imageType)
	, m_imageSize					(imageSize)
	, m_format						(format)
{
}

tcu::TestStatus ImageSparseBindingInstance::iterate (void)
{
	const InstanceInterface&	instance		= m_context.getInstanceInterface();
	const DeviceInterface&		deviceInterface = m_context.getDeviceInterface();
	const VkPhysicalDevice		physicalDevice	= m_context.getPhysicalDevice();

	// Check if device supports sparse binding
	const VkPhysicalDeviceFeatures  deviceFeatures = getPhysicalDeviceFeatures(instance, physicalDevice);

	if (deviceFeatures.sparseBinding == false)
	{
		return tcu::TestStatus(QP_TEST_RESULT_NOT_SUPPORTED, "Device does not support sparse binding");
	}

	// Check if image size does not exceed device limits
	const VkPhysicalDeviceProperties deviceProperties = getPhysicalDeviceProperties(instance, physicalDevice);

	if (isImageSizeSupported(m_imageType, m_imageSize, deviceProperties.limits) == false)
	{
		return tcu::TestStatus(QP_TEST_RESULT_NOT_SUPPORTED, "Image size not supported for device");
	}

	QueueRequirementsVec queueRequirements;
	queueRequirements.push_back(QueueRequirements(VK_QUEUE_SPARSE_BINDING_BIT, 1u));
	queueRequirements.push_back(QueueRequirements(VK_QUEUE_COMPUTE_BIT, 1u));

	// Create logical device supporting both sparse and compute queues
	if (!createDeviceSupportingQueues(queueRequirements))
	{
		return tcu::TestStatus(QP_TEST_RESULT_FAIL, "Could not create device supporting sparse and compute queue");
	}

	const VkPhysicalDeviceMemoryProperties	deviceMemoryProperties = getPhysicalDeviceMemoryProperties(instance, physicalDevice);

	// Create memory allocator for logical device
	const de::UniquePtr<Allocator> allocator(new SimpleAllocator(deviceInterface, *m_logicalDevice, deviceMemoryProperties));

	// Create queue supporting sparse binding operations
	const Queue& sparseQueue = getQueue(VK_QUEUE_SPARSE_BINDING_BIT, 0);

	// Create queue supporting compute and transfer operations
	const Queue& computeQueue = getQueue(VK_QUEUE_COMPUTE_BIT, 0);

	VkImageCreateInfo imageSparseInfo;

	imageSparseInfo.sType					= VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;					//VkStructureType		sType;
	imageSparseInfo.pNext					= DE_NULL;												//const void*			pNext;
	imageSparseInfo.flags					= VK_IMAGE_CREATE_SPARSE_BINDING_BIT;					//VkImageCreateFlags	flags;
	imageSparseInfo.imageType				= mapImageType(m_imageType);							//VkImageType			imageType;
	imageSparseInfo.format					= mapTextureFormat(m_format);							//VkFormat				format;
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

	VkImageFormatProperties imageFormatProperties;
	instance.getPhysicalDeviceImageFormatProperties(physicalDevice,
													imageSparseInfo.format,
													imageSparseInfo.imageType,
													imageSparseInfo.tiling,
													imageSparseInfo.usage,
													imageSparseInfo.flags,
													&imageFormatProperties);

	imageSparseInfo.mipLevels = getImageMaxMipLevels(imageFormatProperties, imageSparseInfo);

	// Allow sharing of sparse image by two different queue families (if necessary)
	const deUint32 queueFamilyIndices[] = { sparseQueue.queueFamilyIndex, computeQueue.queueFamilyIndex };

	if (sparseQueue.queueFamilyIndex != computeQueue.queueFamilyIndex)
	{
		imageSparseInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;	//VkSharingMode			sharingMode;
		imageSparseInfo.queueFamilyIndexCount = 2u;					//deUint32				queueFamilyIndexCount;
		imageSparseInfo.pQueueFamilyIndices = queueFamilyIndices;	//const deUint32*		pQueueFamilyIndices;
	}

	// Create sparse image
	const Unique<VkImage> imageSparse(createImage(deviceInterface, *m_logicalDevice, &imageSparseInfo));

	// Get sparse image general memory requirements
	const VkMemoryRequirements imageSparseMemRequirements = getImageMemoryRequirements(deviceInterface, *m_logicalDevice, *imageSparse);

	// Check if required image memory size does not exceed device limits
	if (imageSparseMemRequirements.size > deviceProperties.limits.sparseAddressSpaceSize)
	{
		return tcu::TestStatus(QP_TEST_RESULT_NOT_SUPPORTED, "Required memory size for sparse resource exceeds device limits");
	}

	DE_ASSERT((imageSparseMemRequirements.size % imageSparseMemRequirements.alignment) == 0);

	typedef de::SharedPtr< Unique<VkDeviceMemory> > DeviceMemoryUniquePtr;

	std::vector<VkSparseMemoryBind>		sparseMemoryBinds;
	std::vector<DeviceMemoryUniquePtr>	deviceMemUniquePtrVec;
	const deUint32						numSparseBinds	= static_cast<deUint32>(imageSparseMemRequirements.size / imageSparseMemRequirements.alignment);
	const deUint32						memoryType		= findMatchingMemoryType(deviceMemoryProperties, imageSparseMemRequirements, MemoryRequirement::Any);

	if (memoryType == NO_MATCH_FOUND)
	{
		return tcu::TestStatus(QP_TEST_RESULT_FAIL, "No matching memory type found");
	}

	for (deUint32 sparseBindNdx = 0; sparseBindNdx < numSparseBinds; ++sparseBindNdx)
	{
		const VkMemoryAllocateInfo allocInfo =
		{
			VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,	//	VkStructureType			sType;
			DE_NULL,								//	const void*				pNext;
			imageSparseMemRequirements.alignment,	//	VkDeviceSize			allocationSize;
			memoryType,								//	deUint32				memoryTypeIndex;
		};

		VkDeviceMemory deviceMemory = 0;
		VK_CHECK(deviceInterface.allocateMemory(*m_logicalDevice, &allocInfo, DE_NULL, &deviceMemory));

		deviceMemUniquePtrVec.push_back(makeVkSharedPtr(Move<VkDeviceMemory>(check<VkDeviceMemory>(deviceMemory), Deleter<VkDeviceMemory>(deviceInterface, *m_logicalDevice, DE_NULL))));

		const VkSparseMemoryBind sparseMemoryBind = makeSparseMemoryBind
		(
			imageSparseMemRequirements.alignment * sparseBindNdx,	//VkDeviceSize				resourceOffset
			imageSparseMemRequirements.alignment,					//VkDeviceSize				size
			deviceMemory,											//VkDeviceMemory			memory;
			0u,														//VkDeviceSize				memoryOffset;
			0u														//VkSparseMemoryBindFlags	flags;
		);

		sparseMemoryBinds.push_back(sparseMemoryBind);
	}

	const VkSparseImageOpaqueMemoryBindInfo opaqueBindInfo = makeSparseImageOpaqueMemoryBindInfo
	(
		*imageSparse,			// VkImage						image
		numSparseBinds,			// deUint32						bindCount
		&sparseMemoryBinds[0]   // const VkSparseMemoryBind*	pBinds
	);

	const Unique<VkSemaphore> imageMemoryBindSemaphore(makeSemaphore(deviceInterface, *m_logicalDevice));

	const VkBindSparseInfo bindSparseInfo =
	{
		VK_STRUCTURE_TYPE_BIND_SPARSE_INFO,			//VkStructureType							sType;
		DE_NULL,									//const void*								pNext;
		0u,											//deUint32									waitSemaphoreCount;
		DE_NULL,									//const VkSemaphore*						pWaitSemaphores;
		0u,											//deUint32									bufferBindCount;
		DE_NULL,									//const VkSparseBufferMemoryBindInfo*		pBufferBinds;
		1u,											//deUint32									imageOpaqueBindCount;
		&opaqueBindInfo,							//const VkSparseImageOpaqueMemoryBindInfo*	pImageOpaqueBinds;
		0u,											//deUint32									imageBindCount;
		DE_NULL,									//const VkSparseImageMemoryBindInfo*		pImageBinds;
		1u,											//deUint32									signalSemaphoreCount;
		&imageMemoryBindSemaphore.get()				//const VkSemaphore*						pSignalSemaphores;
	};

	// Submit sparse bind commands for execution
	VK_CHECK(deviceInterface.queueBindSparse(sparseQueue.queueHandle, 1u, &bindSparseInfo, DE_NULL));

	// Create command buffer for compute and transfer oparations
	const Unique<VkCommandPool>	  commandPool(makeCommandPool(deviceInterface, *m_logicalDevice, computeQueue.queueFamilyIndex));
	const Unique<VkCommandBuffer> commandBuffer(makeCommandBuffer(deviceInterface, *m_logicalDevice, *commandPool));

	// Start recording commands
	beginCommandBuffer(deviceInterface, *commandBuffer);

	const deUint32				imageSizeInBytes = getImageSizeInBytes(imageSparseInfo.extent, imageSparseInfo.arrayLayers, m_format, imageSparseInfo.mipLevels);
	const VkBufferCreateInfo	inputBufferCreateInfo = makeBufferCreateInfo(imageSizeInBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

	const de::UniquePtr<Buffer>	inputBuffer(new Buffer(deviceInterface, *m_logicalDevice, *allocator, inputBufferCreateInfo, MemoryRequirement::HostVisible));

	std::vector<deUint8> referenceData;
	referenceData.resize(imageSizeInBytes);

	for (deUint32 valueNdx = 0; valueNdx < imageSizeInBytes; ++valueNdx)
	{
		referenceData[valueNdx] = static_cast<deUint8>((valueNdx % imageSparseMemRequirements.alignment) + 1u);
	}

	deMemcpy(inputBuffer->getAllocation().getHostPtr(), &referenceData[0], imageSizeInBytes);

	flushMappedMemoryRange(deviceInterface, *m_logicalDevice, inputBuffer->getAllocation().getMemory(), inputBuffer->getAllocation().getOffset(), imageSizeInBytes);

	const VkBufferMemoryBarrier inputBufferBarrier
		= makeBufferMemoryBarrier(
		VK_ACCESS_HOST_WRITE_BIT,
		VK_ACCESS_TRANSFER_READ_BIT,
		inputBuffer->get(),
		0u,
		imageSizeInBytes);

	const VkImageSubresourceRange fullImageSubresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, imageSparseInfo.mipLevels, 0u, imageSparseInfo.arrayLayers);

	const VkImageMemoryBarrier imageSparseTransferDstBarrier
		= makeImageMemoryBarrier(
		0u,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		*imageSparse,
		fullImageSubresourceRange);

	deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 1u, &inputBufferBarrier, 1u, &imageSparseTransferDstBarrier);

	std::vector <VkBufferImageCopy> bufferImageCopy;
	bufferImageCopy.resize(imageSparseInfo.mipLevels);

	VkDeviceSize bufferOffset = 0;
	for (deUint32 mipmapNdx = 0; mipmapNdx < imageSparseInfo.mipLevels; mipmapNdx++)
	{
		bufferImageCopy[mipmapNdx] = makeBufferImageCopy(mipLevelExtents(imageSparseInfo.extent, mipmapNdx), imageSparseInfo.arrayLayers, mipmapNdx, bufferOffset);

		bufferOffset += getImageMipLevelSizeInBytes(imageSparseInfo.extent, imageSparseInfo.arrayLayers, m_format, mipmapNdx);
	}

	deviceInterface.cmdCopyBufferToImage(*commandBuffer, inputBuffer->get(), *imageSparse, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<deUint32>(bufferImageCopy.size()), &bufferImageCopy[0]);

	const VkImageMemoryBarrier imageSparseTransferSrcBarrier
		= makeImageMemoryBarrier(
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_ACCESS_TRANSFER_READ_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		*imageSparse,
		fullImageSubresourceRange);

	deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &imageSparseTransferSrcBarrier);

	const VkBufferCreateInfo	outputBufferCreateInfo = makeBufferCreateInfo(imageSizeInBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const de::UniquePtr<Buffer>	outputBuffer(new Buffer(deviceInterface, *m_logicalDevice, *allocator, outputBufferCreateInfo, MemoryRequirement::HostVisible));

	deviceInterface.cmdCopyImageToBuffer(*commandBuffer, *imageSparse, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, outputBuffer->get(), static_cast<deUint32>(bufferImageCopy.size()), &bufferImageCopy[0]);

	const VkBufferMemoryBarrier outputBufferBarrier
		= makeBufferMemoryBarrier(
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_ACCESS_HOST_READ_BIT,
		outputBuffer->get(),
		0u,
		imageSizeInBytes);

	deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, 1u, &outputBufferBarrier, 0u, DE_NULL);

	// End recording commands
	endCommandBuffer(deviceInterface, *commandBuffer);

	const VkPipelineStageFlags stageBits[] = { VK_PIPELINE_STAGE_TRANSFER_BIT };

	// Submit commands for execution and wait for completion
	submitCommandsAndWait(deviceInterface, *m_logicalDevice, computeQueue.queueHandle, *commandBuffer, 1u, &imageMemoryBindSemaphore.get(), stageBits);

	// Retrieve data from buffer to host memory
	const Allocation& allocation = outputBuffer->getAllocation();

	invalidateMappedMemoryRange(deviceInterface, *m_logicalDevice, allocation.getMemory(), allocation.getOffset(), imageSizeInBytes);

	const deUint8*  outputData = static_cast<const deUint8*>(allocation.getHostPtr());
	tcu::TestStatus testStatus = tcu::TestStatus::pass("Passed");

	if (deMemCmp(outputData, &referenceData[0], imageSizeInBytes) != 0)
	{
		testStatus = tcu::TestStatus::fail("Failed");
	}

	// Wait for sparse queue to become idle
	deviceInterface.queueWaitIdle(sparseQueue.queueHandle);

	return testStatus;
}

TestInstance* ImageSparseBindingCase::createInstance (Context& context) const
{
	return new ImageSparseBindingInstance(context, m_imageType, m_imageSize, m_format);
}

} // anonymous ns

tcu::TestCaseGroup* createImageSparseBindingTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "image_sparse_binding", "Buffer Sparse Binding"));

	static const deUint32 sizeCountPerImageType = 3u;

	struct ImageParameters
	{
		ImageType	imageType;
		tcu::UVec3	imageSizes[sizeCountPerImageType];
	};

	static const ImageParameters imageParametersArray[] =
	{
		{ IMAGE_TYPE_1D,		{ tcu::UVec3(512u, 1u,   1u ), tcu::UVec3(1024u, 1u,   1u), tcu::UVec3(11u, 1u,   1u) } },
		{ IMAGE_TYPE_1D_ARRAY,  { tcu::UVec3(512u, 1u,   64u), tcu::UVec3(1024u, 1u,   8u), tcu::UVec3(11u, 1u,   3u) } },
		{ IMAGE_TYPE_2D,		{ tcu::UVec3(512u, 256u, 1u ), tcu::UVec3(1024u, 128u, 1u), tcu::UVec3(11u, 137u, 1u) } },
		{ IMAGE_TYPE_2D_ARRAY,	{ tcu::UVec3(512u, 256u, 6u ), tcu::UVec3(1024u, 128u, 8u), tcu::UVec3(11u, 137u, 3u) } },
		{ IMAGE_TYPE_3D,		{ tcu::UVec3(512u, 256u, 6u ), tcu::UVec3(1024u, 128u, 8u), tcu::UVec3(11u, 137u, 3u) } },
		{ IMAGE_TYPE_CUBE,		{ tcu::UVec3(512u, 256u, 1u ), tcu::UVec3(1024u, 128u, 1u), tcu::UVec3(11u, 137u, 1u) } },
		{ IMAGE_TYPE_CUBE_ARRAY,{ tcu::UVec3(512u, 256u, 6u ), tcu::UVec3(1024u, 128u, 8u), tcu::UVec3(11u, 137u, 3u) } }
	};

	static const tcu::TextureFormat formats[] =
	{
		tcu::TextureFormat(tcu::TextureFormat::R,		tcu::TextureFormat::SIGNED_INT32),
		tcu::TextureFormat(tcu::TextureFormat::R,		tcu::TextureFormat::SIGNED_INT16),
		tcu::TextureFormat(tcu::TextureFormat::R,		tcu::TextureFormat::SIGNED_INT8),
		tcu::TextureFormat(tcu::TextureFormat::RGBA,	tcu::TextureFormat::UNSIGNED_INT32),
		tcu::TextureFormat(tcu::TextureFormat::RGBA,	tcu::TextureFormat::UNSIGNED_INT16),
		tcu::TextureFormat(tcu::TextureFormat::RGBA,	tcu::TextureFormat::UNSIGNED_INT8)
	};

	for (deInt32 imageTypeNdx = 0; imageTypeNdx < DE_LENGTH_OF_ARRAY(imageParametersArray); ++imageTypeNdx)
	{
		const ImageType					imageType = imageParametersArray[imageTypeNdx].imageType;
		de::MovePtr<tcu::TestCaseGroup> imageTypeGroup(new tcu::TestCaseGroup(testCtx, getImageTypeName(imageType).c_str(), ""));

		for (deInt32 formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(formats); ++formatNdx)
		{
			const tcu::TextureFormat&		format = formats[formatNdx];
			de::MovePtr<tcu::TestCaseGroup> formatGroup(new tcu::TestCaseGroup(testCtx, getShaderImageFormatQualifier(format).c_str(), ""));

			for (deInt32 imageSizeNdx = 0; imageSizeNdx < DE_LENGTH_OF_ARRAY(imageParametersArray[imageTypeNdx].imageSizes); ++imageSizeNdx)
			{
				const tcu::UVec3 imageSize = imageParametersArray[imageTypeNdx].imageSizes[imageSizeNdx];

				std::ostringstream stream;
				stream << imageSize.x() << "_" << imageSize.y() << "_" << imageSize.z();

				formatGroup->addChild(new ImageSparseBindingCase(testCtx, stream.str(), "", imageType, imageSize, format));
			}
			imageTypeGroup->addChild(formatGroup.release());
		}
		testGroup->addChild(imageTypeGroup.release());
	}

	return testGroup.release();
}

} // sparse
} // vkt
