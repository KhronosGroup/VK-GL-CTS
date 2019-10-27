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
 * \file  vktSparseResourcesImageMemoryAliasing.cpp
 * \brief Sparse image memory aliasing tests
 *//*--------------------------------------------------------------------*/

#include "vktSparseResourcesImageMemoryAliasing.hpp"
#include "vktSparseResourcesTestsUtil.hpp"
#include "vktSparseResourcesBase.hpp"
#include "vktTestCaseUtil.hpp"

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkRefUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"

#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuTexVerifierUtil.hpp"

#include <deMath.h>
#include <string>
#include <vector>

using namespace vk;

namespace vkt
{
namespace sparse
{
namespace
{

const deUint32 MODULO_DIVISOR = 127;

const std::string getCoordStr	(const ImageType	imageType,
								 const std::string&	x,
								 const std::string&	y,
								 const std::string&	z)
{
	switch (imageType)
	{
		case IMAGE_TYPE_1D:
		case IMAGE_TYPE_BUFFER:
			return x;

		case IMAGE_TYPE_1D_ARRAY:
		case IMAGE_TYPE_2D:
			return "ivec2(" + x + "," + y + ")";

		case IMAGE_TYPE_2D_ARRAY:
		case IMAGE_TYPE_3D:
		case IMAGE_TYPE_CUBE:
		case IMAGE_TYPE_CUBE_ARRAY:
			return "ivec3(" + x + "," + y + "," + z + ")";

		default:
			DE_FATAL("Unexpected image type");
			return "";
	}
}

class ImageSparseMemoryAliasingCase : public TestCase
{
public:
	ImageSparseMemoryAliasingCase	(tcu::TestContext&		testCtx,
									 const std::string&		name,
									 const std::string&		description,
									 const ImageType		imageType,
									 const tcu::UVec3&		imageSize,
									 const VkFormat			format,
									 const glu::GLSLVersion	glslVersion,
									 const bool				useDeviceGroups);

	void			initPrograms	(SourceCollections&		sourceCollections) const;
	TestInstance*	createInstance	(Context&				context) const;
	virtual void	checkSupport	(Context&				context) const;


private:
	const bool				m_useDeviceGroups;
	const ImageType			m_imageType;
	const tcu::UVec3		m_imageSize;
	const VkFormat			m_format;
	const glu::GLSLVersion	m_glslVersion;
};

ImageSparseMemoryAliasingCase::ImageSparseMemoryAliasingCase	(tcu::TestContext&		testCtx,
																 const std::string&		name,
																 const std::string&		description,
																 const ImageType		imageType,
																 const tcu::UVec3&		imageSize,
																 const VkFormat			format,
																 const glu::GLSLVersion	glslVersion,
																 const bool				useDeviceGroups)
	: TestCase			(testCtx, name, description)
	, m_useDeviceGroups	(useDeviceGroups)
	, m_imageType		(imageType)
	, m_imageSize		(imageSize)
	, m_format			(format)
	, m_glslVersion		(glslVersion)
{
}

void ImageSparseMemoryAliasingCase::checkSupport (Context& context) const
{
	const InstanceInterface&	instance		= context.getInstanceInterface();
	const VkPhysicalDevice		physicalDevice	= context.getPhysicalDevice();

	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SPARSE_RESIDENCY_ALIASED);

	// Check if image size does not exceed device limits
	if (!isImageSizeSupported(instance, physicalDevice, m_imageType, m_imageSize))
		TCU_THROW(NotSupportedError, "Image size not supported for device");

	// Check if device supports sparse operations for image type
	if (!checkSparseSupportForImageType(instance, physicalDevice, m_imageType))
		TCU_THROW(NotSupportedError, "Sparse residency for image type is not supported");
}

class ImageSparseMemoryAliasingInstance : public SparseResourcesBaseInstance
{
public:
	ImageSparseMemoryAliasingInstance	(Context&			context,
										 const ImageType	imageType,
										 const tcu::UVec3&	imageSize,
										 const VkFormat		format,
										 const bool			useDeviceGroups);

	tcu::TestStatus	iterate				(void);

private:
	const bool			m_useDeviceGroups;
	const ImageType		m_imageType;
	const tcu::UVec3	m_imageSize;
	const VkFormat		m_format;
};

ImageSparseMemoryAliasingInstance::ImageSparseMemoryAliasingInstance	(Context&			context,
																		 const ImageType	imageType,
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

tcu::TestStatus ImageSparseMemoryAliasingInstance::iterate (void)
{
	const float					epsilon					= 1e-5f;
	const InstanceInterface&	instance				= m_context.getInstanceInterface();

	{
		// Create logical device supporting both sparse and compute queues
		QueueRequirementsVec queueRequirements;
		queueRequirements.push_back(QueueRequirements(VK_QUEUE_SPARSE_BINDING_BIT, 1u));
		queueRequirements.push_back(QueueRequirements(VK_QUEUE_COMPUTE_BIT, 1u));

		createDeviceSupportingQueues(queueRequirements);
	}

	const VkPhysicalDevice		physicalDevice			= getPhysicalDevice();
	const tcu::UVec3			maxWorkGroupSize		= tcu::UVec3(128u, 128u, 64u);
	const tcu::UVec3			maxWorkGroupCount		= tcu::UVec3(65535u, 65535u, 65535u);
	const deUint32				maxWorkGroupInvocations	= 128u;
	VkImageCreateInfo			imageSparseInfo;
	std::vector<DeviceMemorySp>	deviceMemUniquePtrVec;

	//vsk getting queues should be outside the loop
	//see these in all image files

	const DeviceInterface&			deviceInterface		= getDeviceInterface();
	const Queue&					sparseQueue			= getQueue(VK_QUEUE_SPARSE_BINDING_BIT, 0);
	const Queue&					computeQueue		= getQueue(VK_QUEUE_COMPUTE_BIT, 0);
	const PlanarFormatDescription	formatDescription	= getPlanarFormatDescription(m_format);

	// Go through all physical devices
	for (deUint32 physDevID = 0; physDevID < m_numPhysicalDevices; physDevID++)
	{
		const deUint32	firstDeviceID	= physDevID;
		const deUint32	secondDeviceID	= (firstDeviceID + 1) % m_numPhysicalDevices;

		imageSparseInfo.sType					= VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageSparseInfo.pNext					= DE_NULL;
		imageSparseInfo.flags					= VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT |
												  VK_IMAGE_CREATE_SPARSE_ALIASED_BIT   |
												  VK_IMAGE_CREATE_SPARSE_BINDING_BIT;
		imageSparseInfo.imageType				= mapImageType(m_imageType);
		imageSparseInfo.format					= m_format;
		imageSparseInfo.extent					= makeExtent3D(getLayerSize(m_imageType, m_imageSize));
		imageSparseInfo.arrayLayers				= getNumLayers(m_imageType, m_imageSize);
		imageSparseInfo.samples					= VK_SAMPLE_COUNT_1_BIT;
		imageSparseInfo.tiling					= VK_IMAGE_TILING_OPTIMAL;
		imageSparseInfo.initialLayout			= VK_IMAGE_LAYOUT_UNDEFINED;
		imageSparseInfo.usage					= VK_IMAGE_USAGE_TRANSFER_DST_BIT |
												  VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
												  VK_IMAGE_USAGE_STORAGE_BIT;
		imageSparseInfo.sharingMode				= VK_SHARING_MODE_EXCLUSIVE;
		imageSparseInfo.queueFamilyIndexCount	= 0u;
		imageSparseInfo.pQueueFamilyIndices		= DE_NULL;

		if (m_imageType == IMAGE_TYPE_CUBE || m_imageType == IMAGE_TYPE_CUBE_ARRAY)
			imageSparseInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

		// Check if device supports sparse operations for image format
		if (!checkSparseSupportForImageFormat(instance, physicalDevice, imageSparseInfo))
			TCU_THROW(NotSupportedError, "The image format does not support sparse operations");

		{
			// Assign maximum allowed mipmap levels to image
			VkImageFormatProperties imageFormatProperties;
			if (instance.getPhysicalDeviceImageFormatProperties(physicalDevice,
				imageSparseInfo.format,
				imageSparseInfo.imageType,
				imageSparseInfo.tiling,
				imageSparseInfo.usage,
				imageSparseInfo.flags,
				&imageFormatProperties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
			{
				TCU_THROW(NotSupportedError, "Image format does not support sparse operations");
			}

			imageSparseInfo.mipLevels = getMipmapCount(m_format, formatDescription, imageFormatProperties, imageSparseInfo.extent);
		}

		// Create sparse image
		const Unique<VkImage> imageRead(createImage(deviceInterface, getDevice(), &imageSparseInfo));
		const Unique<VkImage> imageWrite(createImage(deviceInterface, getDevice(), &imageSparseInfo));

		// Create semaphores to synchronize sparse binding operations with other operations on the sparse images
		const Unique<VkSemaphore> memoryBindSemaphoreTransfer(createSemaphore(deviceInterface, getDevice()));
		const Unique<VkSemaphore> memoryBindSemaphoreCompute(createSemaphore(deviceInterface, getDevice()));

		const VkSemaphore imageMemoryBindSemaphores[] = { memoryBindSemaphoreTransfer.get(), memoryBindSemaphoreCompute.get() };

		std::vector<VkSparseImageMemoryRequirements> sparseMemoryRequirements;

		{
			// Get sparse image general memory requirements
			const VkMemoryRequirements imageMemoryRequirements = getImageMemoryRequirements(deviceInterface, getDevice(), *imageRead);

			// Check if required image memory size does not exceed device limits
			if (imageMemoryRequirements.size > getPhysicalDeviceProperties(instance, getPhysicalDevice(secondDeviceID)).limits.sparseAddressSpaceSize)
				TCU_THROW(NotSupportedError, "Required memory size for sparse resource exceeds device limits");

			DE_ASSERT((imageMemoryRequirements.size % imageMemoryRequirements.alignment) == 0);

			const deUint32 memoryType = findMatchingMemoryType(instance, getPhysicalDevice(secondDeviceID), imageMemoryRequirements, MemoryRequirement::Any);

			if (memoryType == NO_MATCH_FOUND)
				return tcu::TestStatus::fail("No matching memory type found");

			if (firstDeviceID != secondDeviceID)
			{
				VkPeerMemoryFeatureFlags	peerMemoryFeatureFlags	= (VkPeerMemoryFeatureFlags)0;
				const deUint32				heapIndex				= getHeapIndexForMemoryType(instance, getPhysicalDevice(secondDeviceID), memoryType);
				deviceInterface.getDeviceGroupPeerMemoryFeatures(getDevice(), heapIndex, firstDeviceID, secondDeviceID, &peerMemoryFeatureFlags);

				if (((peerMemoryFeatureFlags & VK_PEER_MEMORY_FEATURE_COPY_SRC_BIT) == 0) ||
					((peerMemoryFeatureFlags & VK_PEER_MEMORY_FEATURE_COPY_DST_BIT) == 0) ||
					((peerMemoryFeatureFlags & VK_PEER_MEMORY_FEATURE_GENERIC_DST_BIT) == 0))
				{
					TCU_THROW(NotSupportedError, "Peer memory does not support COPY_SRC, COPY_DST, and GENERIC_DST");
				}
			}

			// Get sparse image sparse memory requirements
			sparseMemoryRequirements = getImageSparseMemoryRequirements(deviceInterface, getDevice(), *imageRead);

			DE_ASSERT(sparseMemoryRequirements.size() != 0);

			std::vector<VkSparseImageMemoryBind> imageResidencyMemoryBinds;
			std::vector<VkSparseMemoryBind>		 imageReadMipTailBinds;
			std::vector<VkSparseMemoryBind>		 imageWriteMipTailBinds;

			for (deUint32 planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
			{
				const VkImageAspectFlags		aspect				= (formatDescription.numPlanes > 1) ? getPlaneAspect(planeNdx) : VK_IMAGE_ASPECT_COLOR_BIT;
				const deUint32					aspectIndex			= getSparseAspectRequirementsIndex(sparseMemoryRequirements, aspect);

				if (aspectIndex == NO_MATCH_FOUND)
					TCU_THROW(NotSupportedError, "Not supported image aspect");

				VkSparseImageMemoryRequirements	aspectRequirements	= sparseMemoryRequirements[aspectIndex];

				DE_ASSERT((aspectRequirements.imageMipTailSize % imageMemoryRequirements.alignment) == 0);

				VkExtent3D						imageGranularity	= aspectRequirements.formatProperties.imageGranularity;

				// Bind memory for each layer
				for (deUint32 layerNdx = 0; layerNdx < imageSparseInfo.arrayLayers; ++layerNdx)
				{
					for (deUint32 mipLevelNdx = 0; mipLevelNdx < aspectRequirements.imageMipTailFirstLod; ++mipLevelNdx)
					{
						const VkExtent3D			mipExtent			= getPlaneExtent(formatDescription, imageSparseInfo.extent, planeNdx, mipLevelNdx);
						const tcu::UVec3			sparseBlocks		= alignedDivide(mipExtent, imageGranularity);
						const deUint32				numSparseBlocks		= sparseBlocks.x() * sparseBlocks.y() * sparseBlocks.z();
						const VkImageSubresource	subresource			= { aspect, mipLevelNdx, layerNdx };

						const VkSparseImageMemoryBind imageMemoryBind	= makeSparseImageMemoryBind(deviceInterface, getDevice(),
							imageMemoryRequirements.alignment * numSparseBlocks, memoryType, subresource, makeOffset3D(0u, 0u, 0u), mipExtent);

						deviceMemUniquePtrVec.push_back(makeVkSharedPtr(Move<VkDeviceMemory>(check<VkDeviceMemory>(imageMemoryBind.memory), Deleter<VkDeviceMemory>(deviceInterface, getDevice(), DE_NULL))));

						imageResidencyMemoryBinds.push_back(imageMemoryBind);
					}

					if (!(aspectRequirements.formatProperties.flags & VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT) && aspectRequirements.imageMipTailFirstLod < imageSparseInfo.mipLevels)
					{
						const VkSparseMemoryBind imageReadMipTailMemoryBind = makeSparseMemoryBind(deviceInterface, getDevice(),
							aspectRequirements.imageMipTailSize, memoryType, aspectRequirements.imageMipTailOffset + layerNdx * aspectRequirements.imageMipTailStride);

						deviceMemUniquePtrVec.push_back(makeVkSharedPtr(Move<VkDeviceMemory>(check<VkDeviceMemory>(imageReadMipTailMemoryBind.memory), Deleter<VkDeviceMemory>(deviceInterface, getDevice(), DE_NULL))));

						imageReadMipTailBinds.push_back(imageReadMipTailMemoryBind);

						const VkSparseMemoryBind imageWriteMipTailMemoryBind = makeSparseMemoryBind(deviceInterface, getDevice(),
							aspectRequirements.imageMipTailSize, memoryType, aspectRequirements.imageMipTailOffset + layerNdx * aspectRequirements.imageMipTailStride);

						deviceMemUniquePtrVec.push_back(makeVkSharedPtr(Move<VkDeviceMemory>(check<VkDeviceMemory>(imageWriteMipTailMemoryBind.memory), Deleter<VkDeviceMemory>(deviceInterface, getDevice(), DE_NULL))));

						imageWriteMipTailBinds.push_back(imageWriteMipTailMemoryBind);
					}
				}

				if ((aspectRequirements.formatProperties.flags & VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT) && aspectRequirements.imageMipTailFirstLod < imageSparseInfo.mipLevels)
				{
					const VkSparseMemoryBind imageReadMipTailMemoryBind = makeSparseMemoryBind(deviceInterface, getDevice(),
						aspectRequirements.imageMipTailSize, memoryType, aspectRequirements.imageMipTailOffset);

					deviceMemUniquePtrVec.push_back(makeVkSharedPtr(Move<VkDeviceMemory>(check<VkDeviceMemory>(imageReadMipTailMemoryBind.memory), Deleter<VkDeviceMemory>(deviceInterface, getDevice(), DE_NULL))));

					imageReadMipTailBinds.push_back(imageReadMipTailMemoryBind);

					const VkSparseMemoryBind imageWriteMipTailMemoryBind = makeSparseMemoryBind(deviceInterface, getDevice(),
						aspectRequirements.imageMipTailSize, memoryType, aspectRequirements.imageMipTailOffset);

					deviceMemUniquePtrVec.push_back(makeVkSharedPtr(Move<VkDeviceMemory>(check<VkDeviceMemory>(imageWriteMipTailMemoryBind.memory), Deleter<VkDeviceMemory>(deviceInterface, getDevice(), DE_NULL))));

					imageWriteMipTailBinds.push_back(imageWriteMipTailMemoryBind);
				}
			}

			const VkDeviceGroupBindSparseInfo devGroupBindSparseInfo =
			{
				VK_STRUCTURE_TYPE_DEVICE_GROUP_BIND_SPARSE_INFO_KHR,	//VkStructureType							sType;
				DE_NULL,												//const void*								pNext;
				firstDeviceID,											//deUint32									resourceDeviceIndex;
				secondDeviceID,											//deUint32									memoryDeviceIndex;
			};

			VkBindSparseInfo bindSparseInfo =
			{
				VK_STRUCTURE_TYPE_BIND_SPARSE_INFO,						//VkStructureType							sType;
				m_useDeviceGroups ? &devGroupBindSparseInfo : DE_NULL,	//const void*								pNext;
				0u,														//deUint32									waitSemaphoreCount;
				DE_NULL,												//const VkSemaphore*						pWaitSemaphores;
				0u,														//deUint32									bufferBindCount;
				DE_NULL,												//const VkSparseBufferMemoryBindInfo*		pBufferBinds;
				0u,														//deUint32									imageOpaqueBindCount;
				DE_NULL,												//const VkSparseImageOpaqueMemoryBindInfo*	pImageOpaqueBinds;
				0u,														//deUint32									imageBindCount;
				DE_NULL,												//const VkSparseImageMemoryBindInfo*		pImageBinds;
				2u,														//deUint32									signalSemaphoreCount;
				imageMemoryBindSemaphores								//const VkSemaphore*						pSignalSemaphores;
			};

			VkSparseImageMemoryBindInfo			imageResidencyBindInfo[2];
			VkSparseImageOpaqueMemoryBindInfo	imageMipTailBindInfo[2];

			if (imageResidencyMemoryBinds.size() > 0)
			{
				imageResidencyBindInfo[0].image		= *imageRead;
				imageResidencyBindInfo[0].bindCount = static_cast<deUint32>(imageResidencyMemoryBinds.size());
				imageResidencyBindInfo[0].pBinds	= imageResidencyMemoryBinds.data();

				imageResidencyBindInfo[1].image		= *imageWrite;
				imageResidencyBindInfo[1].bindCount = static_cast<deUint32>(imageResidencyMemoryBinds.size());
				imageResidencyBindInfo[1].pBinds	= imageResidencyMemoryBinds.data();

				bindSparseInfo.imageBindCount		= 2u;
				bindSparseInfo.pImageBinds			= imageResidencyBindInfo;
			}

			if (imageReadMipTailBinds.size() > 0)
			{
				imageMipTailBindInfo[0].image		= *imageRead;
				imageMipTailBindInfo[0].bindCount	= static_cast<deUint32>(imageReadMipTailBinds.size());
				imageMipTailBindInfo[0].pBinds		= imageReadMipTailBinds.data();

				imageMipTailBindInfo[1].image		= *imageWrite;
				imageMipTailBindInfo[1].bindCount	= static_cast<deUint32>(imageWriteMipTailBinds.size());
				imageMipTailBindInfo[1].pBinds		= imageWriteMipTailBinds.data();

				bindSparseInfo.imageOpaqueBindCount = 2u;
				bindSparseInfo.pImageOpaqueBinds	= imageMipTailBindInfo;
			}

			// Submit sparse bind commands for execution
			VK_CHECK(deviceInterface.queueBindSparse(sparseQueue.queueHandle, 1u, &bindSparseInfo, DE_NULL));
		}

		deUint32							imageSizeInBytes = 0;
		std::vector<std::vector<deUint32>>	planeOffsets( imageSparseInfo.mipLevels );
		std::vector<std::vector<deUint32>>	planeRowPitches( imageSparseInfo.mipLevels );

		for (deUint32 mipmapNdx = 0; mipmapNdx < imageSparseInfo.mipLevels; ++mipmapNdx)
		{
			planeOffsets[mipmapNdx].resize(formatDescription.numPlanes, 0);
			planeRowPitches[mipmapNdx].resize(formatDescription.numPlanes, 0);
		}

		for (deUint32 planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
		{
			for (deUint32 mipmapNdx = 0; mipmapNdx < imageSparseInfo.mipLevels; ++mipmapNdx)
			{
				const tcu::UVec3	gridSize			= getShaderGridSize(m_imageType, m_imageSize, mipmapNdx);
				planeOffsets[mipmapNdx][planeNdx]		= imageSizeInBytes;
				const deUint32		planeW				= gridSize.x() / (formatDescription.blockWidth * formatDescription.planes[planeNdx].widthDivisor);
				planeRowPitches[mipmapNdx][planeNdx]	= formatDescription.planes[planeNdx].elementSizeBytes * planeW;
				imageSizeInBytes						+= getImageMipLevelSizeInBytes(imageSparseInfo.extent, imageSparseInfo.arrayLayers, formatDescription, planeNdx, mipmapNdx, BUFFER_IMAGE_COPY_OFFSET_GRANULARITY);
			}
		}

		std::vector <VkBufferImageCopy>	bufferImageCopy(formatDescription.numPlanes * imageSparseInfo.mipLevels);
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

		std::vector<deUint8> referenceData(imageSizeInBytes);

		for (deUint32 planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
			for (deUint32 mipmapNdx = 0u; mipmapNdx < imageSparseInfo.mipLevels; ++mipmapNdx)
			{
				const deUint32 mipLevelSizeInBytes	= getImageMipLevelSizeInBytes(imageSparseInfo.extent, imageSparseInfo.arrayLayers, formatDescription, planeNdx, mipmapNdx, BUFFER_IMAGE_COPY_OFFSET_GRANULARITY);
				const deUint32 bufferOffset			= static_cast<deUint32>(bufferImageCopy[planeNdx*imageSparseInfo.mipLevels + mipmapNdx].bufferOffset);

				deMemset(&referenceData[bufferOffset], mipmapNdx + 1u, mipLevelSizeInBytes);
			}

		deMemcpy(inputBufferAlloc->getHostPtr(), referenceData.data(), imageSizeInBytes);

		flushAlloc(deviceInterface, getDevice(), *inputBufferAlloc);

		{
			const VkBufferMemoryBarrier inputBufferBarrier = makeBufferMemoryBarrier
			(
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

				imageSparseTransferDstBarriers.emplace_back(makeImageMemoryBarrier
				(
					0u,
					VK_ACCESS_TRANSFER_WRITE_BIT,
					VK_IMAGE_LAYOUT_UNDEFINED,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					*imageRead,
					makeImageSubresourceRange(aspect, 0u, imageSparseInfo.mipLevels, 0u, imageSparseInfo.arrayLayers),
					sparseQueue.queueFamilyIndex != computeQueue.queueFamilyIndex ? sparseQueue.queueFamilyIndex : VK_QUEUE_FAMILY_IGNORED,
					sparseQueue.queueFamilyIndex != computeQueue.queueFamilyIndex ? computeQueue.queueFamilyIndex : VK_QUEUE_FAMILY_IGNORED
				));
			}

			deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, static_cast<deUint32>(imageSparseTransferDstBarriers.size()), imageSparseTransferDstBarriers.data());
		}

		deviceInterface.cmdCopyBufferToImage(*commandBuffer, *inputBuffer, *imageRead, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<deUint32>(bufferImageCopy.size()), bufferImageCopy.data());

		{
			std::vector<VkImageMemoryBarrier> imageSparseTransferSrcBarriers;

			for (deUint32 planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
			{
				const VkImageAspectFlags aspect = (formatDescription.numPlanes > 1) ? getPlaneAspect(planeNdx) : VK_IMAGE_ASPECT_COLOR_BIT;

				imageSparseTransferSrcBarriers.emplace_back(makeImageMemoryBarrier
				(
					VK_ACCESS_TRANSFER_WRITE_BIT,
					VK_ACCESS_TRANSFER_READ_BIT,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					*imageRead,
					makeImageSubresourceRange(aspect, 0u, imageSparseInfo.mipLevels, 0u, imageSparseInfo.arrayLayers)
				));
			}

			deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, static_cast<deUint32>(imageSparseTransferSrcBarriers.size()), imageSparseTransferSrcBarriers.data());
		}

		{
			std::vector<VkImageMemoryBarrier> imageSparseShaderStorageBarriers;

			for (deUint32 planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
			{
				const VkImageAspectFlags aspect = (formatDescription.numPlanes > 1) ? getPlaneAspect(planeNdx) : VK_IMAGE_ASPECT_COLOR_BIT;

				imageSparseShaderStorageBarriers.emplace_back(makeImageMemoryBarrier
				(
					0u,
					VK_ACCESS_SHADER_WRITE_BIT,
					VK_IMAGE_LAYOUT_UNDEFINED,
					VK_IMAGE_LAYOUT_GENERAL,
					*imageWrite,
					makeImageSubresourceRange(aspect, 0u, imageSparseInfo.mipLevels, 0u, imageSparseInfo.arrayLayers)
				));
			}

			deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, static_cast<deUint32>(imageSparseShaderStorageBarriers.size()), imageSparseShaderStorageBarriers.data());
		}

		// Create descriptor set layout
		const Unique<VkDescriptorSetLayout> descriptorSetLayout(
			DescriptorSetLayoutBuilder()
			.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
			.build(deviceInterface, getDevice()));

		Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(deviceInterface, getDevice(), *descriptorSetLayout));

		Unique<VkDescriptorPool> descriptorPool(
			DescriptorPoolBuilder()
			.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, imageSparseInfo.mipLevels)
			.build(deviceInterface, getDevice(), VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, imageSparseInfo.mipLevels));

		typedef de::SharedPtr< Unique<VkImageView> >		SharedVkImageView;
		std::vector<SharedVkImageView>						imageViews;
		imageViews.resize(imageSparseInfo.mipLevels);

		typedef de::SharedPtr< Unique<VkDescriptorSet> >	SharedVkDescriptorSet;
		std::vector<SharedVkDescriptorSet>					descriptorSets;
		descriptorSets.resize(imageSparseInfo.mipLevels);

		typedef de::SharedPtr< Unique<VkPipeline> >			SharedVkPipeline;
		std::vector<SharedVkPipeline>						computePipelines;
		computePipelines.resize(imageSparseInfo.mipLevels);

		for (deUint32 mipLevelNdx = 0u; mipLevelNdx < imageSparseInfo.mipLevels; ++mipLevelNdx)
		{
			std::ostringstream name;
			name << "comp" << mipLevelNdx;

			// Create and bind compute pipeline
			Unique<VkShaderModule> shaderModule(createShaderModule(deviceInterface, getDevice(), m_context.getBinaryCollection().get(name.str()), DE_NULL));

			computePipelines[mipLevelNdx]	= makeVkSharedPtr(makeComputePipeline(deviceInterface, getDevice(), *pipelineLayout, *shaderModule));
			VkPipeline computePipeline		= **computePipelines[mipLevelNdx];

			deviceInterface.cmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);

			// Create and bind descriptor set
			descriptorSets[mipLevelNdx]		= makeVkSharedPtr(makeDescriptorSet(deviceInterface, getDevice(), *descriptorPool, *descriptorSetLayout));
			VkDescriptorSet descriptorSet	= **descriptorSets[mipLevelNdx];

			// Select which mipmap level to bind
			const VkImageSubresourceRange subresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, mipLevelNdx, 1u, 0u, imageSparseInfo.arrayLayers);

			imageViews[mipLevelNdx] = makeVkSharedPtr(makeImageView(deviceInterface, getDevice(), *imageWrite, mapImageViewType(m_imageType), imageSparseInfo.format, subresourceRange));
			VkImageView imageView	= **imageViews[mipLevelNdx];

			const VkDescriptorImageInfo descriptorImageSparseInfo = makeDescriptorImageInfo(DE_NULL, imageView, VK_IMAGE_LAYOUT_GENERAL);

			DescriptorSetUpdateBuilder()
				.writeSingle(descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descriptorImageSparseInfo)
				.update(deviceInterface, getDevice());

			deviceInterface.cmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet, 0u, DE_NULL);

			const tcu::UVec3	gridSize			= getShaderGridSize(m_imageType, m_imageSize, mipLevelNdx);
			const deUint32		xWorkGroupSize		= std::min(std::min(gridSize.x(), maxWorkGroupSize.x()), maxWorkGroupInvocations);
			const deUint32		yWorkGroupSize		= std::min(std::min(gridSize.y(), maxWorkGroupSize.y()), maxWorkGroupInvocations / xWorkGroupSize);
			const deUint32		zWorkGroupSize		= std::min(std::min(gridSize.z(), maxWorkGroupSize.z()), maxWorkGroupInvocations / (xWorkGroupSize * yWorkGroupSize));

			const deUint32		xWorkGroupCount		= gridSize.x() / xWorkGroupSize + (gridSize.x() % xWorkGroupSize ? 1u : 0u);
			const deUint32		yWorkGroupCount		= gridSize.y() / yWorkGroupSize + (gridSize.y() % yWorkGroupSize ? 1u : 0u);
			const deUint32		zWorkGroupCount		= gridSize.z() / zWorkGroupSize + (gridSize.z() % zWorkGroupSize ? 1u : 0u);

			if (maxWorkGroupCount.x() < xWorkGroupCount ||
				maxWorkGroupCount.y() < yWorkGroupCount ||
				maxWorkGroupCount.z() < zWorkGroupCount)
			{
				TCU_THROW(NotSupportedError, "Image size is not supported");
			}

			deviceInterface.cmdDispatch(*commandBuffer, xWorkGroupCount, yWorkGroupCount, zWorkGroupCount);
		}

		{
			const VkMemoryBarrier memoryBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);

			deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 1u, &memoryBarrier, 0u, DE_NULL, 0u, DE_NULL);
		}

		const VkBufferCreateInfo		outputBufferCreateInfo	= makeBufferCreateInfo(imageSizeInBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
		const Unique<VkBuffer>			outputBuffer			(createBuffer(deviceInterface, getDevice(), &outputBufferCreateInfo));
		const de::UniquePtr<Allocation>	outputBufferAlloc		(bindBuffer(deviceInterface, getDevice(), getAllocator(), *outputBuffer, MemoryRequirement::HostVisible));

		deviceInterface.cmdCopyImageToBuffer(*commandBuffer, *imageRead, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *outputBuffer, static_cast<deUint32>(bufferImageCopy.size()), bufferImageCopy.data());

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

		const VkPipelineStageFlags stageBits[] = { VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT };

		// Submit commands for execution and wait for completion
		submitCommandsAndWait(deviceInterface, getDevice(), computeQueue.queueHandle, *commandBuffer, 2u, imageMemoryBindSemaphores, stageBits,
								0, DE_NULL, m_useDeviceGroups, firstDeviceID);

		// Retrieve data from buffer to host memory
		invalidateAlloc(deviceInterface, getDevice(), *outputBufferAlloc);

		deUint8* outputData = static_cast<deUint8*>(outputBufferAlloc->getHostPtr());

		std::vector<std::vector<void*>> planePointers(imageSparseInfo.mipLevels);

		for (deUint32 mipmapNdx = 0; mipmapNdx < imageSparseInfo.mipLevels; ++mipmapNdx)
			planePointers[mipmapNdx].resize(formatDescription.numPlanes);

		for (deUint32 planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
			for (deUint32 mipmapNdx = 0; mipmapNdx < imageSparseInfo.mipLevels; ++mipmapNdx)
				planePointers[mipmapNdx][planeNdx] = outputData + static_cast<size_t>(planeOffsets[mipmapNdx][planeNdx]);

		// Wait for sparse queue to become idle
		deviceInterface.queueWaitIdle(sparseQueue.queueHandle);

		for (deUint32 channelNdx = 0; channelNdx < 4; ++channelNdx)
		{
			if (!formatDescription.hasChannelNdx(channelNdx))
				continue;

			deUint32							planeNdx			= formatDescription.channels[channelNdx].planeNdx;
			const VkImageAspectFlags			aspect				= (formatDescription.numPlanes > 1) ? getPlaneAspect(planeNdx) : VK_IMAGE_ASPECT_COLOR_BIT;
			const deUint32						aspectIndex			= getSparseAspectRequirementsIndex(sparseMemoryRequirements, aspect);

			if (aspectIndex == NO_MATCH_FOUND)
				TCU_THROW(NotSupportedError, "Not supported image aspect");

			VkSparseImageMemoryRequirements		aspectRequirements	= sparseMemoryRequirements[aspectIndex];
			float								fixedPointError		= tcu::TexVerifierUtil::computeFixedPointError(formatDescription.channels[channelNdx].sizeBits);;

			for (deUint32 mipmapNdx = 0; mipmapNdx < aspectRequirements.imageMipTailFirstLod; ++mipmapNdx)
			{
				const	tcu::UVec3						gridSize		= getShaderGridSize(m_imageType, m_imageSize, mipmapNdx);
				const	tcu::ConstPixelBufferAccess		pixelBuffer		= vk::getChannelAccess(formatDescription, gridSize, planeRowPitches[mipmapNdx].data(), (const void* const*)planePointers[mipmapNdx].data(), channelNdx);
				tcu::IVec3								pixelDivider	= pixelBuffer.getDivider();

				for (deUint32 offsetZ = 0u; offsetZ < gridSize.z(); ++offsetZ)
				for (deUint32 offsetY = 0u; offsetY < gridSize.y(); ++offsetY)
				for (deUint32 offsetX = 0u; offsetX < gridSize.x(); ++offsetX)
				{
					const deUint32	index			= offsetX + gridSize.x() * offsetY + gridSize.x() * gridSize.y() * offsetZ;
					deUint32		iReferenceValue;
					float			fReferenceValue;
					float			acceptableError	= epsilon;

					switch (channelNdx)
					{
						case 0:
						case 1:
						case 2:
							iReferenceValue = index % MODULO_DIVISOR;
							fReferenceValue = static_cast<float>(iReferenceValue) / static_cast<float>(MODULO_DIVISOR);
							break;
						case 3:
							iReferenceValue = 1u;
							fReferenceValue = 1.f;
							break;
						default:	DE_FATAL("Unexpected channel index");	break;
					}

					switch (formatDescription.channels[channelNdx].type)
					{
						case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
						case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
						{
							const tcu::UVec4 outputValue = pixelBuffer.getPixelUint(offsetX * pixelDivider.x(), offsetY * pixelDivider.y(), offsetZ * pixelDivider.z());

							if (outputValue.x() != iReferenceValue)
								return tcu::TestStatus::fail("Failed");

							break;
						}
						case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
						case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
						{
							acceptableError += fixedPointError;
							const tcu::Vec4 outputValue = pixelBuffer.getPixel(offsetX * pixelDivider.x(), offsetY * pixelDivider.y(), offsetZ * pixelDivider.z());

							if (deAbs(outputValue.x() - fReferenceValue) > acceptableError)
								return tcu::TestStatus::fail("Failed");

							break;
						}
						case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
						{
							const tcu::Vec4 outputValue = pixelBuffer.getPixel(offsetX * pixelDivider.x(), offsetY * pixelDivider.y(), offsetZ * pixelDivider.z());

							if (deAbs(outputValue.x() - fReferenceValue) > acceptableError)
								return tcu::TestStatus::fail("Failed");

							break;
						}
						default:	DE_FATAL("Unexpected channel type");	break;
					}
				}
			}

			for (deUint32 mipmapNdx = aspectRequirements.imageMipTailFirstLod; mipmapNdx < imageSparseInfo.mipLevels; ++mipmapNdx)
			{
				const deUint32 mipLevelSizeInBytes	= getImageMipLevelSizeInBytes(imageSparseInfo.extent, imageSparseInfo.arrayLayers, formatDescription, planeNdx, mipmapNdx);
				const deUint32 bufferOffset			= static_cast<deUint32>(bufferImageCopy[planeNdx*imageSparseInfo.mipLevels + mipmapNdx].bufferOffset);

				if (deMemCmp(outputData + bufferOffset, &referenceData[bufferOffset], mipLevelSizeInBytes) != 0)
					return tcu::TestStatus::fail("Failed");
			}
		}
	}

	return tcu::TestStatus::pass("Passed");
}

void ImageSparseMemoryAliasingCase::initPrograms(SourceCollections&	sourceCollections) const
{
	const char* const				versionDecl				= glu::getGLSLVersionDeclaration(m_glslVersion);
	const PlanarFormatDescription	formatDescription		= getPlanarFormatDescription(m_format);
	const std::string				imageTypeStr			= getShaderImageType(formatDescription, m_imageType);
	const std::string				formatQualifierStr		= getShaderImageFormatQualifier(m_format);
	const std::string				formatDataStr			= getShaderImageDataType(formatDescription);
	const deUint32					maxWorkGroupInvocations = 128u;
	const tcu::UVec3				maxWorkGroupSize		= tcu::UVec3(128u, 128u, 64u);
	VkExtent3D						layerExtent				= makeExtent3D(getLayerSize(m_imageType, m_imageSize));
	VkImageFormatProperties			imageFormatProperties;
	imageFormatProperties.maxMipLevels						= 20;
	const deUint32					mipLevels				= getMipmapCount(m_format, formatDescription, imageFormatProperties, layerExtent);

	std::ostringstream formatValueStr;
	switch (formatDescription.channels[0].type)
	{
		case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
		case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
			formatValueStr << "( index % " << MODULO_DIVISOR << ", index % " << MODULO_DIVISOR << ", index % " << MODULO_DIVISOR << ", 1)";
			break;
		case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
		case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
		case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
			formatValueStr << "( float( index % " << MODULO_DIVISOR << ") / " << MODULO_DIVISOR << ".0, float( index % " << MODULO_DIVISOR << ") / " << MODULO_DIVISOR << ".0, float( index % " << MODULO_DIVISOR << ") / " << MODULO_DIVISOR << ".0, 1.0)";
			break;
		default:	DE_FATAL("Unexpected channel type");	break;
	}


	for (deUint32 mipLevelNdx = 0; mipLevelNdx < mipLevels; ++mipLevelNdx)
	{
		// Create compute program
		const tcu::UVec3	gridSize		= getShaderGridSize(m_imageType, m_imageSize, mipLevelNdx);
		const deUint32		xWorkGroupSize  = std::min(std::min(gridSize.x(), maxWorkGroupSize.x()), maxWorkGroupInvocations);
		const deUint32		yWorkGroupSize  = std::min(std::min(gridSize.y(), maxWorkGroupSize.y()), maxWorkGroupInvocations / xWorkGroupSize);
		const deUint32		zWorkGroupSize  = std::min(std::min(gridSize.z(), maxWorkGroupSize.z()), maxWorkGroupInvocations / (xWorkGroupSize * yWorkGroupSize));

		std::ostringstream src;

		src << versionDecl << "\n"
			<< "layout (local_size_x = " << xWorkGroupSize << ", local_size_y = " << yWorkGroupSize << ", local_size_z = " << zWorkGroupSize << ") in; \n"
			<< "layout (binding = 0, " << formatQualifierStr << ") writeonly uniform highp " << imageTypeStr << " u_image;\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "	if( gl_GlobalInvocationID.x < " << gridSize.x() << " ) \n"
			<< "	if( gl_GlobalInvocationID.y < " << gridSize.y() << " ) \n"
			<< "	if( gl_GlobalInvocationID.z < " << gridSize.z() << " ) \n"
			<< "	{\n"
			<< "		int index = int( gl_GlobalInvocationID.x + "<< gridSize.x() << " * gl_GlobalInvocationID.y + " << gridSize.x() << " * " << gridSize.y() << " * gl_GlobalInvocationID.z );\n"
			<< "		imageStore(u_image, " << getCoordStr(m_imageType, "gl_GlobalInvocationID.x", "gl_GlobalInvocationID.y", "gl_GlobalInvocationID.z") << ","
			<< formatDataStr << formatValueStr.str() <<"); \n"
			<< "	}\n"
			<< "}\n";

		std::ostringstream name;
		name << "comp" << mipLevelNdx;
		sourceCollections.glslSources.add(name.str()) << glu::ComputeSource(src.str());
	}
}

TestInstance* ImageSparseMemoryAliasingCase::createInstance (Context& context) const
{
	return new ImageSparseMemoryAliasingInstance(context, m_imageType, m_imageSize, m_format, m_useDeviceGroups);
}

} // anonymous ns

tcu::TestCaseGroup* createImageSparseMemoryAliasingTestsCommon(tcu::TestContext& testCtx, de::MovePtr<tcu::TestCaseGroup> testGroup, const bool useDeviceGroup = false)
{

	const std::vector<TestImageParameters> imageParameters =
	{
		{ IMAGE_TYPE_2D,		{ tcu::UVec3(512u, 256u, 1u),	tcu::UVec3(128u, 128u, 1u),	tcu::UVec3(503u, 137u, 1u),	tcu::UVec3(11u, 37u, 1u) },	getTestFormats(IMAGE_TYPE_2D) },
		{ IMAGE_TYPE_2D_ARRAY,	{ tcu::UVec3(512u, 256u, 6u),	tcu::UVec3(128u, 128u, 8u),	tcu::UVec3(503u, 137u, 3u),	tcu::UVec3(11u, 37u, 3u) },	getTestFormats(IMAGE_TYPE_2D_ARRAY) },
		{ IMAGE_TYPE_CUBE,		{ tcu::UVec3(256u, 256u, 1u),	tcu::UVec3(128u, 128u, 1u),	tcu::UVec3(137u, 137u, 1u),	tcu::UVec3(11u, 11u, 1u) },	getTestFormats(IMAGE_TYPE_CUBE) },
		{ IMAGE_TYPE_CUBE_ARRAY,{ tcu::UVec3(256u, 256u, 6u),	tcu::UVec3(128u, 128u, 8u),	tcu::UVec3(137u, 137u, 3u),	tcu::UVec3(11u, 11u, 3u) },	getTestFormats(IMAGE_TYPE_CUBE_ARRAY) },
		{ IMAGE_TYPE_3D,		{ tcu::UVec3(256u, 256u, 16u),	tcu::UVec3(128u, 128u, 8u),	tcu::UVec3(503u, 137u, 3u),	tcu::UVec3(11u, 37u, 3u) },	getTestFormats(IMAGE_TYPE_3D) }
	};

	for (size_t imageTypeNdx = 0; imageTypeNdx < imageParameters.size(); ++imageTypeNdx)
	{
		const ImageType					imageType = imageParameters[imageTypeNdx].imageType;
		de::MovePtr<tcu::TestCaseGroup> imageTypeGroup(new tcu::TestCaseGroup(testCtx, getImageTypeName(imageType).c_str(), ""));

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

				std::ostringstream stream;
				stream << imageSize.x() << "_" << imageSize.y() << "_" << imageSize.z();

				formatGroup->addChild(new ImageSparseMemoryAliasingCase(testCtx, stream.str(), "", imageType, imageSize, format, glu::GLSL_VERSION_440, useDeviceGroup));
			}
			imageTypeGroup->addChild(formatGroup.release());
		}
		testGroup->addChild(imageTypeGroup.release());
	}

	return testGroup.release();
}

tcu::TestCaseGroup* createImageSparseMemoryAliasingTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "image_sparse_memory_aliasing", "Sparse Image Memory Aliasing"));
	return createImageSparseMemoryAliasingTestsCommon(testCtx, testGroup);
}

tcu::TestCaseGroup* createDeviceGroupImageSparseMemoryAliasingTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "device_group_image_sparse_memory_aliasing", "Sparse Image Memory Aliasing"));
	return createImageSparseMemoryAliasingTestsCommon(testCtx, testGroup, true);
}

} // sparse
} // vkt
