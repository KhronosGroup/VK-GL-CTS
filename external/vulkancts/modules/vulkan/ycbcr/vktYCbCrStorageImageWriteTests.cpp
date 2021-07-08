/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 Google Inc.
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
 * \brief Testing compute shader writing to separate planes of a multiplanar format
 *//*--------------------------------------------------------------------*/

#include "vktYCbCrStorageImageWriteTests.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktYCbCrUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkImageUtil.hpp"
#include "tcuTexVerifierUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "tcuTestLog.hpp"

namespace vkt
{
namespace ycbcr
{
namespace
{

using namespace vk;

struct TestParameters
{
	VkFormat			format;
	tcu::UVec3			size;
	VkImageCreateFlags	flags;

	TestParameters (VkFormat			format_,
					const tcu::UVec3&	size_,
					VkImageCreateFlags	flags_)
		: format			(format_)
		, size				(size_)
		, flags				(flags_)
	{
	}

	TestParameters (void)
		: format			(VK_FORMAT_UNDEFINED)
		, flags				(0u)
	{
	}
};

void checkSupport (Context& context, const TestParameters params)
{
	const bool							disjoint = (params.flags & VK_IMAGE_CREATE_DISJOINT_BIT) != 0;
	std::vector<std::string>			reqExts;

	if (disjoint)
	{
		if (!isCoreDeviceExtension(context.getUsedApiVersion(), "VK_KHR_bind_memory2"))
			reqExts.push_back("VK_KHR_bind_memory2");
		if (!isCoreDeviceExtension(context.getUsedApiVersion(), "VK_KHR_get_memory_requirements2"))
			reqExts.push_back("VK_KHR_get_memory_requirements2");
	}

	for ( const auto& extIter : reqExts )
	{
		if (!context.isDeviceFunctionalitySupported(extIter))
			TCU_THROW(NotSupportedError, (extIter + " is not supported").c_str());
	}

	{
		const VkFormatProperties	formatProperties = getPhysicalDeviceFormatProperties(context.getInstanceInterface(),
			context.getPhysicalDevice(),
			params.format);

		if ((formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) == 0)
			TCU_THROW(NotSupportedError, "Storage images are not supported for this format");

		if (disjoint && ((formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DISJOINT_BIT) == 0))
			TCU_THROW(NotSupportedError, "Disjoint planes are not supported for this format");
	}
}

template<typename T>
inline de::SharedPtr<vk::Unique<T> > makeVkSharedPtr(vk::Move<T> vkMove)
{
	return de::SharedPtr<vk::Unique<T> >(new vk::Unique<T>(vkMove));
}

tcu::UVec3 computeWorkGroupSize(const VkExtent3D& planeExtent)
{
	const deUint32		maxComputeWorkGroupInvocations	= 128u;
	const tcu::UVec3	maxComputeWorkGroupSize			= tcu::UVec3(128u, 128u, 64u);

	const deUint32		xWorkGroupSize					= std::min(std::min(planeExtent.width, maxComputeWorkGroupSize.x()), maxComputeWorkGroupInvocations);
	const deUint32		yWorkGroupSize					= std::min(std::min(planeExtent.height, maxComputeWorkGroupSize.y()), maxComputeWorkGroupInvocations / xWorkGroupSize);
	const deUint32		zWorkGroupSize					= std::min(std::min(planeExtent.depth, maxComputeWorkGroupSize.z()), maxComputeWorkGroupInvocations / (xWorkGroupSize*yWorkGroupSize));

	return tcu::UVec3(xWorkGroupSize, yWorkGroupSize, zWorkGroupSize);
}

Move<VkPipeline> makeComputePipeline (const DeviceInterface&		vk,
									  const VkDevice				device,
									  const VkPipelineLayout		pipelineLayout,
									  const VkShaderModule			shaderModule,
									  const VkSpecializationInfo*	specializationInfo)
{
	const VkPipelineShaderStageCreateInfo pipelineShaderStageParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType						sType;
		DE_NULL,												// const void*							pNext;
		0u,														// VkPipelineShaderStageCreateFlags		flags;
		VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits				stage;
		shaderModule,											// VkShaderModule						module;
		"main",													// const char*							pName;
		specializationInfo,										// const VkSpecializationInfo*			pSpecializationInfo;
	};
	const VkComputePipelineCreateInfo pipelineCreateInfo =
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,		// VkStructureType					sType;
		DE_NULL,											// const void*						pNext;
		0u,													// VkPipelineCreateFlags			flags;
		pipelineShaderStageParams,							// VkPipelineShaderStageCreateInfo	stage;
		pipelineLayout,										// VkPipelineLayout					layout;
		DE_NULL,											// VkPipeline						basePipelineHandle;
		0,													// deInt32							basePipelineIndex;
	};
	return createComputePipeline(vk, device, DE_NULL , &pipelineCreateInfo);
}

vk::VkFormat getPlaneCompatibleFormatForWriting(const vk::PlanarFormatDescription& formatInfo, deUint32 planeNdx)
{
	DE_ASSERT(planeNdx < formatInfo.numPlanes);
	vk::VkFormat result = formatInfo.planes[planeNdx].planeCompatibleFormat;

	// redirect result for some of the YCbCr image formats
	static const std::pair<vk::VkFormat, vk::VkFormat> ycbcrFormats[] =
	{
		{ VK_FORMAT_G8B8G8R8_422_UNORM_KHR,						VK_FORMAT_R8G8B8A8_UNORM		},
		{ VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16_KHR,	VK_FORMAT_R16G16B16A16_UNORM	},
		{ VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16_KHR,	VK_FORMAT_R16G16B16A16_UNORM	},
		{ VK_FORMAT_G16B16G16R16_422_UNORM_KHR,					VK_FORMAT_R16G16B16A16_UNORM	},
		{ VK_FORMAT_B8G8R8G8_422_UNORM_KHR,						VK_FORMAT_R8G8B8A8_UNORM		},
		{ VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16_KHR,	VK_FORMAT_R16G16B16A16_UNORM	},
		{ VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16_KHR,	VK_FORMAT_R16G16B16A16_UNORM	},
		{ VK_FORMAT_B16G16R16G16_422_UNORM_KHR,					VK_FORMAT_R16G16B16A16_UNORM	}
	};
	auto it = std::find_if(std::begin(ycbcrFormats), std::end(ycbcrFormats), [result](const std::pair<vk::VkFormat, vk::VkFormat>& p) { return p.first == result; });
	if (it != std::end(ycbcrFormats))
		result = it->second;
	return result;
}

tcu::TestStatus testStorageImageWrite (Context& context, TestParameters params)
{
	const DeviceInterface&						vkd						= context.getDeviceInterface();
	const VkDevice								device					= context.getDevice();
	const deUint32								queueFamilyIndex		= context.getUniversalQueueFamilyIndex();
	const VkQueue								queue					= context.getUniversalQueue();
	const PlanarFormatDescription				formatDescription		= getPlanarFormatDescription(params.format);

	VkImageCreateInfo							imageCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		DE_NULL,
		params.flags,
		VK_IMAGE_TYPE_2D,
		params.format,
		makeExtent3D(params.size.x(), params.size.y(), params.size.z()),
		1u,			// mipLevels
		1u,			// arrayLayers
		VK_SAMPLE_COUNT_1_BIT,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		0u,
		(const deUint32*)DE_NULL,
		VK_IMAGE_LAYOUT_UNDEFINED,
	};

	// check if we need to create VkImageView with different VkFormat than VkImage format
	VkFormat planeCompatibleFormat0 = getPlaneCompatibleFormatForWriting(formatDescription, 0);
	if (planeCompatibleFormat0 != getPlaneCompatibleFormat(formatDescription, 0))
	{
		imageCreateInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
	}

	const Unique<VkImage>						image					(createImage(vkd, device, &imageCreateInfo));
	// allocate memory for the whole image, or for each separate plane ( if the params.flags include VK_IMAGE_CREATE_DISJOINT_BIT )
	const std::vector<AllocationSp>				allocations				(allocateAndBindImageMemory(vkd, device, context.getDefaultAllocator(), *image, params.format, params.flags, MemoryRequirement::Any));

	// Create descriptor set layout
	const Unique<VkDescriptorSetLayout>			descriptorSetLayout		(DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
		.build(vkd, device));
	const Unique<VkPipelineLayout>				pipelineLayout			(makePipelineLayout(vkd, device, *descriptorSetLayout));

	// Create descriptor sets
	const Unique<VkDescriptorPool>				descriptorPool			(DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1u)
		.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, vk::PlanarFormatDescription::MAX_PLANES));

	// Create command buffer for compute and transfer operations
	const Unique<VkCommandPool>					commandPool				(makeCommandPool(vkd, device, queueFamilyIndex));
	const Unique<VkCommandBuffer>				commandBuffer			(allocateCommandBuffer(vkd, device, *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	std::vector<de::SharedPtr<vk::Unique<vk::VkShaderModule>>>			shaderModules;
	std::vector<de::SharedPtr<vk::Unique<vk::VkPipeline>>>				computePipelines;
	std::vector<de::SharedPtr<vk::Unique<vk::VkDescriptorSet>>>			descriptorSets;
	std::vector<de::SharedPtr<vk::Unique<vk::VkImageView>>>				imageViews;

	deUint32									imageSizeInBytes		= 0;
	deUint32									planeOffsets[PlanarFormatDescription::MAX_PLANES];
	deUint32									planeRowPitches[PlanarFormatDescription::MAX_PLANES];
	void*										planePointers[PlanarFormatDescription::MAX_PLANES];

	{
		// Start recording commands
		beginCommandBuffer(vkd, *commandBuffer);

		for (deUint32 planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
		{
			const VkImageAspectFlags		aspect						= (formatDescription.numPlanes > 1) ? getPlaneAspect(planeNdx) : VK_IMAGE_ASPECT_COLOR_BIT;
			const VkImageSubresourceRange	subresourceRange			= makeImageSubresourceRange(aspect, 0u, 1u, 0u, 1u);
			VkFormat						planeCompatibleFormat		= getPlaneCompatibleFormatForWriting(formatDescription, planeNdx);
			vk::PlanarFormatDescription		compatibleFormatDescription = (planeCompatibleFormat != getPlaneCompatibleFormat(formatDescription, planeNdx)) ? getPlanarFormatDescription(planeCompatibleFormat) : formatDescription;
			const tcu::UVec3				compatibleShaderGridSize	( params.size.x() / formatDescription.blockWidth, params.size.y() / formatDescription.blockHeight, params.size.z() / 1u);
			VkExtent3D						shaderExtent				= getPlaneExtent(compatibleFormatDescription, VkExtent3D{ compatibleShaderGridSize.x(), compatibleShaderGridSize.y(), compatibleShaderGridSize.z() }, planeNdx, 0u);

			// Create and bind compute pipeline
			std::ostringstream shaderName;
			shaderName << "comp" << planeNdx;
			auto							shaderModule			= makeVkSharedPtr(createShaderModule(vkd, device, context.getBinaryCollection().get(shaderName.str()), DE_NULL));
			shaderModules.push_back(shaderModule);
			auto							computePipeline			= makeVkSharedPtr(makeComputePipeline(vkd, device, *pipelineLayout, shaderModule->get(), DE_NULL));
			computePipelines.push_back(computePipeline);
			vkd.cmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline->get());

			auto							descriptorSet			= makeVkSharedPtr(makeDescriptorSet(vkd, device, *descriptorPool, *descriptorSetLayout));
			descriptorSets.push_back(descriptorSet);

			auto							imageView				= makeVkSharedPtr(makeImageView(vkd, device, *image, VK_IMAGE_VIEW_TYPE_2D, planeCompatibleFormat, subresourceRange));
			imageViews.push_back(imageView);
			const VkDescriptorImageInfo		imageInfo				= makeDescriptorImageInfo(DE_NULL, imageView->get(), VK_IMAGE_LAYOUT_GENERAL);

			DescriptorSetUpdateBuilder()
				.writeSingle(descriptorSet->get(), DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &imageInfo)
				.update(vkd, device);

			vkd.cmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet->get(), 0u, DE_NULL);

			{
				const VkImageMemoryBarrier imageLayoutChangeBarrier = makeImageMemoryBarrier(0u, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, *image, subresourceRange, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED);
				vkd.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &imageLayoutChangeBarrier);
			}

			{
				const tcu::UVec3 workGroupSize = computeWorkGroupSize(shaderExtent);

				const deUint32 xWorkGroupCount = shaderExtent.width / workGroupSize.x() + (shaderExtent.width % workGroupSize.x() ? 1u : 0u);
				const deUint32 yWorkGroupCount = shaderExtent.height / workGroupSize.y() + (shaderExtent.height % workGroupSize.y() ? 1u : 0u);
				const deUint32 zWorkGroupCount = shaderExtent.depth / workGroupSize.z() + (shaderExtent.depth % workGroupSize.z() ? 1u : 0u);

				const tcu::UVec3 maxComputeWorkGroupCount = tcu::UVec3(65535u, 65535u, 65535u);

				if (maxComputeWorkGroupCount.x() < xWorkGroupCount ||
					maxComputeWorkGroupCount.y() < yWorkGroupCount ||
					maxComputeWorkGroupCount.z() < zWorkGroupCount)
				{
					TCU_THROW(NotSupportedError, "Image size is not supported");
				}

				vkd.cmdDispatch(*commandBuffer, xWorkGroupCount, yWorkGroupCount, zWorkGroupCount);
			}

			{
				const VkImageMemoryBarrier imageTransferBarrier = makeImageMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *image, subresourceRange);
				vkd.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &imageTransferBarrier);
			}
		}

		for (deUint32 planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
		{
			planeOffsets[planeNdx]		= imageSizeInBytes;
			const deUint32	planeW		= imageCreateInfo.extent.width / (formatDescription.blockWidth * formatDescription.planes[planeNdx].widthDivisor);
			planeRowPitches[planeNdx]	= formatDescription.planes[planeNdx].elementSizeBytes * planeW;
			imageSizeInBytes			+= getPlaneSizeInBytes(formatDescription, makeExtent3D( params.size.x(), params.size.y(), params.size.z()) , planeNdx, 0u, BUFFER_IMAGE_COPY_OFFSET_GRANULARITY);
		}

		const VkBufferCreateInfo		outputBufferCreateInfo	= makeBufferCreateInfo(imageSizeInBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
		const Unique<VkBuffer>			outputBuffer			( createBuffer(vkd, device, &outputBufferCreateInfo) );
		const de::UniquePtr<Allocation>	outputBufferAlloc		( bindBuffer(vkd, device, context.getDefaultAllocator(), *outputBuffer, MemoryRequirement::HostVisible) );
		std::vector<VkBufferImageCopy>	bufferImageCopy			( formatDescription.numPlanes );

		for (deUint32 planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
		{
			const VkImageAspectFlags	aspect = (formatDescription.numPlanes > 1) ? getPlaneAspect(planeNdx) : VK_IMAGE_ASPECT_COLOR_BIT;

			bufferImageCopy[planeNdx] =
			{
				planeOffsets[planeNdx],																								//	VkDeviceSize				bufferOffset;
				0u,																													//	deUint32					bufferRowLength;
				0u,																													//	deUint32					bufferImageHeight;
				makeImageSubresourceLayers(aspect, 0u, 0u, 1u),																		//	VkImageSubresourceLayers	imageSubresource;
				makeOffset3D(0, 0, 0),																								//	VkOffset3D					imageOffset;
				getPlaneExtent(formatDescription, makeExtent3D(params.size.x(), params.size.y(), params.size.z()), planeNdx, 0u)	//	VkExtent3D					imageExtent;
			};
		}
		vkd.cmdCopyImageToBuffer(*commandBuffer, *image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *outputBuffer, static_cast<deUint32>(bufferImageCopy.size()), bufferImageCopy.data());

		{
			const VkBufferMemoryBarrier outputBufferHostReadBarrier = makeBufferMemoryBarrier
			(
				VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_ACCESS_HOST_READ_BIT,
				*outputBuffer,
				0u,
				imageSizeInBytes
			);

			vkd.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, 1u, &outputBufferHostReadBarrier, 0u, DE_NULL);
		}

		// End recording commands
		endCommandBuffer(vkd, *commandBuffer);

		// Submit commands for execution and wait for completion
		submitCommandsAndWait(vkd, device, queue, *commandBuffer);

		// Retrieve data from buffer to host memory
		invalidateAlloc(vkd, device, *outputBufferAlloc);
		deUint8*					outputData = static_cast<deUint8*>(outputBufferAlloc->getHostPtr());

		for (deUint32 planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
			planePointers[planeNdx] = outputData + static_cast<size_t>(planeOffsets[planeNdx]);
	}

	// write result images to log file
	for (deUint32 channelNdx = 0; channelNdx < 4; ++channelNdx)
	{
		if (!formatDescription.hasChannelNdx(channelNdx))
			continue;
		deUint32					planeNdx					= formatDescription.channels[channelNdx].planeNdx;
		vk::VkFormat				planeCompatibleFormat		= getPlaneCompatibleFormatForWriting(formatDescription, planeNdx);
		vk::PlanarFormatDescription	compatibleFormatDescription	= (planeCompatibleFormat != getPlaneCompatibleFormat(formatDescription, planeNdx)) ? getPlanarFormatDescription(planeCompatibleFormat) : formatDescription;
		const tcu::UVec3			compatibleShaderGridSize	( params.size.x() / formatDescription.blockWidth, params.size.y() / formatDescription.blockHeight, params.size.z() / 1u );
		tcu::ConstPixelBufferAccess	pixelBuffer					= vk::getChannelAccess(compatibleFormatDescription, compatibleShaderGridSize, planeRowPitches, (const void* const*)planePointers, channelNdx);
		std::ostringstream str;
		str << "image" << channelNdx;
		context.getTestContext().getLog() << tcu::LogImage(str.str(), str.str(), pixelBuffer);;
	}

	// verify data
	const float					epsilon = 1e-5f;
	for (deUint32 channelNdx = 0; channelNdx < 4; ++channelNdx)
	{
		if (!formatDescription.hasChannelNdx(channelNdx))
			continue;

		deUint32							planeNdx					= formatDescription.channels[channelNdx].planeNdx;
		vk::VkFormat						planeCompatibleFormat		= getPlaneCompatibleFormatForWriting(formatDescription, planeNdx);
		vk::PlanarFormatDescription			compatibleFormatDescription	= (planeCompatibleFormat != getPlaneCompatibleFormat(formatDescription, planeNdx)) ? getPlanarFormatDescription(planeCompatibleFormat) : formatDescription;
		const tcu::UVec3					compatibleShaderGridSize	( params.size.x() / formatDescription.blockWidth, params.size.y() / formatDescription.blockHeight, params.size.z() / 1u );
		VkExtent3D							compatibleImageSize			{ imageCreateInfo.extent.width / formatDescription.blockWidth, imageCreateInfo.extent.height / formatDescription.blockHeight, imageCreateInfo.extent.depth / 1u };
		tcu::ConstPixelBufferAccess			pixelBuffer					= vk::getChannelAccess(compatibleFormatDescription, compatibleShaderGridSize, planeRowPitches, (const void* const*)planePointers, channelNdx);
		VkExtent3D							planeExtent					= getPlaneExtent(compatibleFormatDescription, compatibleImageSize, planeNdx, 0u);
		tcu::IVec3							pixelDivider				= pixelBuffer.getDivider();

		for (deUint32 offsetZ = 0u; offsetZ < planeExtent.depth; ++offsetZ)
		for (deUint32 offsetY = 0u; offsetY < planeExtent.height; ++offsetY)
		for (deUint32 offsetX = 0u; offsetX < planeExtent.width; ++offsetX)
		{
			deUint32	iReferenceValue;
			float		fReferenceValue;
			switch (channelNdx)
			{
				case 0:
					iReferenceValue = offsetX % 127u;
					fReferenceValue = static_cast<float>(iReferenceValue) / 127.f;
					break;
				case 1:
					iReferenceValue = offsetY % 127u;
					fReferenceValue = static_cast<float>(iReferenceValue) / 127.f;
					break;
				case 2:
					iReferenceValue = offsetZ % 127u;
					fReferenceValue = static_cast<float>(iReferenceValue) / 127.f;
					break;
				case 3:
					iReferenceValue = 0u;
					fReferenceValue = 0.f;
					break;
				default:	DE_FATAL("Unexpected channel index");	break;
			}
			float acceptableError = epsilon;

			switch (formatDescription.channels[channelNdx].type)
			{
				case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
				case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
				{
					tcu::UVec4 outputValue = pixelBuffer.getPixelUint(offsetX * pixelDivider.x(), offsetY * pixelDivider.y(), 0);

					if (outputValue.x() != iReferenceValue)
						return tcu::TestStatus::fail("Failed");

					break;
				}
				case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
				case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
				{
					float fixedPointError = tcu::TexVerifierUtil::computeFixedPointError(formatDescription.channels[channelNdx].sizeBits);
					acceptableError += fixedPointError;
					tcu::Vec4 outputValue = pixelBuffer.getPixel(offsetX * pixelDivider.x(), offsetY * pixelDivider.y(), 0);

					if (deAbs(outputValue.x() - fReferenceValue) > acceptableError)
						return tcu::TestStatus::fail("Failed");

					break;
				}
				case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
				{
					const tcu::Vec4 outputValue = pixelBuffer.getPixel(offsetX * pixelDivider.x(), offsetY * pixelDivider.y(), 0);

					if (deAbs( outputValue.x() - fReferenceValue) > acceptableError)
						return tcu::TestStatus::fail("Failed");

					break;
				}
				default:	DE_FATAL("Unexpected channel type");	break;
			}
		}
	}
	return tcu::TestStatus::pass("Passed");
}

std::string getShaderImageType (const vk::PlanarFormatDescription& description)
{
	std::string	formatPart;

	// all PlanarFormatDescription types have at least one channel ( 0 ) and all channel types are the same :
	switch (description.channels[0].type)
	{
		case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
			formatPart = "i";
			break;
		case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
			formatPart = "u";
			break;
		case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
		case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
		case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
			break;

		default:
			DE_FATAL("Unexpected channel type");
	}

	return formatPart + "image2D";
}

std::string getShaderImageDataType (const vk::PlanarFormatDescription& description)
{
	switch (description.channels[0].type)
	{
	case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
		return "uvec4";
	case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
		return "ivec4";
	case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
	case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
	case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
		return "vec4";
	default:
		DE_FATAL("Unexpected channel type");
		return "";
	}
}

std::string getFormatValueString	(const std::vector<std::pair<deUint32, deUint32>>& channelsOnPlane,
									 const std::vector<std::string>& formatValueStrings)
{
	std::string result = "( ";
	deUint32 i;
	for (i=0; i<channelsOnPlane.size(); ++i)
	{
		result += formatValueStrings[channelsOnPlane[i].first];
		if (i < 3)
			result += ", ";
	}
	for (; i < 4; ++i)
	{
		result += "0";
		if (i < 3)
			result += ", ";
	}
	result += " )";
	return result;
}

std::string getShaderImageFormatQualifier (VkFormat format)
{
	switch (format)
	{
		case VK_FORMAT_R8_SINT:										return "r8i";
		case VK_FORMAT_R16_SINT:									return "r16i";
		case VK_FORMAT_R32_SINT:									return "r32i";
		case VK_FORMAT_R8_UINT:										return "r8ui";
		case VK_FORMAT_R16_UINT:									return "r16ui";
		case VK_FORMAT_R32_UINT:									return "r32ui";
		case VK_FORMAT_R8_SNORM:									return "r8_snorm";
		case VK_FORMAT_R16_SNORM:									return "r16_snorm";
		case VK_FORMAT_R8_UNORM:									return "r8";
		case VK_FORMAT_R16_UNORM:									return "r16";

		case VK_FORMAT_R8G8_SINT:									return "rg8i";
		case VK_FORMAT_R16G16_SINT:									return "rg16i";
		case VK_FORMAT_R32G32_SINT:									return "rg32i";
		case VK_FORMAT_R8G8_UINT:									return "rg8ui";
		case VK_FORMAT_R16G16_UINT:									return "rg16ui";
		case VK_FORMAT_R32G32_UINT:									return "rg32ui";
		case VK_FORMAT_R8G8_SNORM:									return "rg8_snorm";
		case VK_FORMAT_R16G16_SNORM:								return "rg16_snorm";
		case VK_FORMAT_R8G8_UNORM:									return "rg8";
		case VK_FORMAT_R16G16_UNORM:								return "rg16";

		case VK_FORMAT_R8G8B8A8_SINT:								return "rgba8i";
		case VK_FORMAT_R16G16B16A16_SINT:							return "rgba16i";
		case VK_FORMAT_R32G32B32A32_SINT:							return "rgba32i";
		case VK_FORMAT_R8G8B8A8_UINT:								return "rgba8ui";
		case VK_FORMAT_R16G16B16A16_UINT:							return "rgba16ui";
		case VK_FORMAT_R32G32B32A32_UINT:							return "rgba32ui";
		case VK_FORMAT_R8G8B8A8_SNORM:								return "rgba8_snorm";
		case VK_FORMAT_R16G16B16A16_SNORM:							return "rgba16_snorm";
		case VK_FORMAT_R8G8B8A8_UNORM:								return "rgba8";
		case VK_FORMAT_R16G16B16A16_UNORM:							return "rgba16";

		case VK_FORMAT_G8B8G8R8_422_UNORM:							return "rgba8";
		case VK_FORMAT_B8G8R8G8_422_UNORM:							return "rgba8";
		case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:					return "rgba8";
		case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:					return "rgba8";
		case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM:					return "rgba8";
		case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:					return "rgba8";
		case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM:					return "rgba8";
		case VK_FORMAT_R10X6_UNORM_PACK16:							return "r16";
		case VK_FORMAT_R10X6G10X6_UNORM_2PACK16:					return "rg16";
		case VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16:			return "rgba16";
		case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16:		return "rgba16";
		case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16:		return "rgba16";
		case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:	return "rgba16";
		case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:	return "rgba16";
		case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:	return "rgba16";
		case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:	return "rgba16";
		case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16:	return "rgba16";
		case VK_FORMAT_R12X4_UNORM_PACK16:							return "r16";
		case VK_FORMAT_R12X4G12X4_UNORM_2PACK16:					return "rg16";
		case VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16:			return "rgba16";
		case VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16:		return "rgba16";
		case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16:		return "rgba16";
		case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:	return "rgba16";
		case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:	return "rgba16";
		case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:	return "rgba16";
		case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:	return "rgba16";
		case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16:	return "rgba16";
		case VK_FORMAT_G16B16G16R16_422_UNORM:						return "rgba16";
		case VK_FORMAT_B16G16R16G16_422_UNORM:						return "rgba16";
		case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM:				return "rgba16";
		case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:					return "rgba16";
		case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM:				return "rgba16";
		case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM:					return "rgba16";
		case VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM:				return "rgba16";
		case VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT:				return "rgba8";
		case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT:return "rgba16";
		case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT:return "rgba16";
		case VK_FORMAT_G16_B16R16_2PLANE_444_UNORM_EXT:				return "rgba16";

		default:
			DE_FATAL("Unexpected texture format");
			return "error";
	}
}

void initPrograms (SourceCollections& sourceCollections, TestParameters params)
{
	// Create compute program
	const char* const				versionDecl			= glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440);
	const PlanarFormatDescription	formatDescription	= getPlanarFormatDescription(params.format);
	const std::string				imageTypeStr		= getShaderImageType(formatDescription);
	const std::string				formatDataStr		= getShaderImageDataType(formatDescription);
	const tcu::UVec3				shaderGridSize		( params.size.x(), params.size.y(), params.size.z() );

	std::vector<std::string>		formatValueStrings;
	switch (formatDescription.channels[0].type)
	{
	case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
	case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
		formatValueStrings = {
			"int(gl_GlobalInvocationID.x) % 127",
			"int(gl_GlobalInvocationID.y) % 127",
			"int(gl_GlobalInvocationID.z) % 127",
			"1"
		};
		break;
	case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
	case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
	case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
		formatValueStrings = {
			"float(int(gl_GlobalInvocationID.x) % 127) / 127.0" ,
			"float(int(gl_GlobalInvocationID.y) % 127) / 127.0",
			"float(int(gl_GlobalInvocationID.z) % 127) / 127.0",
			"1.0"
		};
		break;
	default:	DE_ASSERT(false);	break;
	}

	for (deUint32 planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
	{
		VkFormat						planeCompatibleFormat		= getPlaneCompatibleFormatForWriting(formatDescription, planeNdx);
		vk::PlanarFormatDescription		compatibleFormatDescription	= (planeCompatibleFormat != getPlaneCompatibleFormat(formatDescription, planeNdx)) ? getPlanarFormatDescription(planeCompatibleFormat) : formatDescription;
		VkExtent3D						compatibleShaderGridSize	{ shaderGridSize.x() / formatDescription.blockWidth, shaderGridSize.y() / formatDescription.blockHeight, shaderGridSize.z() / 1u };

		std::vector<std::pair<deUint32, deUint32>> channelsOnPlane;
		for (deUint32 channelNdx = 0; channelNdx < 4; ++channelNdx)
		{
			if (!formatDescription.hasChannelNdx(channelNdx))
				continue;
			if (formatDescription.channels[channelNdx].planeNdx != planeNdx)
				continue;
			channelsOnPlane.push_back({ channelNdx,formatDescription.channels[channelNdx].offsetBits });
		}
		// reorder channels for multi-planar images
		if (formatDescription.numPlanes > 1)
			std::sort(begin(channelsOnPlane), end(channelsOnPlane), [](const std::pair<deUint32, deUint32>& lhs, const std::pair<deUint32, deUint32>& rhs) { return lhs.second < rhs.second; });
		std::string			formatValueStr		= getFormatValueString(channelsOnPlane, formatValueStrings);
		VkExtent3D			shaderExtent		= getPlaneExtent(compatibleFormatDescription, compatibleShaderGridSize, planeNdx, 0);
		const std::string	formatQualifierStr	= getShaderImageFormatQualifier(formatDescription.planes[planeNdx].planeCompatibleFormat);
		const tcu::UVec3	workGroupSize		= computeWorkGroupSize(shaderExtent);

		std::ostringstream src;
		src << versionDecl << "\n"
			<< "layout (local_size_x = " << workGroupSize.x() << ", local_size_y = " << workGroupSize.y() << ", local_size_z = " << workGroupSize.z() << ") in; \n"
			<< "layout (binding = 0, " << formatQualifierStr << ") writeonly uniform highp " << imageTypeStr << " u_image;\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "	if( gl_GlobalInvocationID.x < " << shaderExtent.width << " ) \n"
			<< "	if( gl_GlobalInvocationID.y < " << shaderExtent.height << " ) \n"
			<< "	if( gl_GlobalInvocationID.z < " << shaderExtent.depth << " ) \n"
			<< "	{\n"
			<< "		imageStore(u_image, ivec2( gl_GlobalInvocationID.x, gl_GlobalInvocationID.y ) ,"
			<< formatDataStr << formatValueStr << ");\n"
			<< "	}\n"
			<< "}\n";
		std::ostringstream shaderName;
		shaderName << "comp" << planeNdx;
		sourceCollections.glslSources.add(shaderName.str()) << glu::ComputeSource(src.str());
	}
}

tcu::TestCaseGroup* populateStorageImageWriteFormatGroup (tcu::TestContext& testCtx, de::MovePtr<tcu::TestCaseGroup> testGroup)
{
	const std::vector<tcu::UVec3>	availableSizes{ tcu::UVec3(512u, 512u, 1u), tcu::UVec3(1024u, 128u, 1u), tcu::UVec3(66u, 32u, 1u) };

	auto addTests = [&](int formatNdx)
	{
		const VkFormat					format				= (VkFormat)formatNdx;
		tcu::UVec3						imageSizeAlignment	= getImageSizeAlignment(format);
		std::string						formatName			= de::toLower(de::toString(format).substr(10));
		de::MovePtr<tcu::TestCaseGroup> formatGroup			( new tcu::TestCaseGroup(testCtx, formatName.c_str(), "") );

		for (size_t sizeNdx = 0; sizeNdx < availableSizes.size(); sizeNdx++)
		{
			const tcu::UVec3 imageSize = availableSizes[sizeNdx];

			// skip test for images with odd sizes for some YCbCr formats
			if ((imageSize.x() % imageSizeAlignment.x()) != 0)
				continue;
			if ((imageSize.y() % imageSizeAlignment.y()) != 0)
				continue;

			std::ostringstream stream;
			stream << imageSize.x() << "_" << imageSize.y() << "_" << imageSize.z();
			de::MovePtr<tcu::TestCaseGroup> sizeGroup(new tcu::TestCaseGroup(testCtx, stream.str().c_str(), ""));

			addFunctionCaseWithPrograms(sizeGroup.get(), "joint", "", checkSupport, initPrograms, testStorageImageWrite, TestParameters(format, imageSize, 0u));
			addFunctionCaseWithPrograms(sizeGroup.get(), "disjoint", "", checkSupport, initPrograms, testStorageImageWrite, TestParameters(format, imageSize, (VkImageCreateFlags)VK_IMAGE_CREATE_DISJOINT_BIT));

			formatGroup->addChild(sizeGroup.release());
		}
		testGroup->addChild(formatGroup.release());
	};

	for (int formatNdx = VK_YCBCR_FORMAT_FIRST; formatNdx < VK_YCBCR_FORMAT_LAST; formatNdx++)
	{
		addTests(formatNdx);
	}

	for (int formatNdx = VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT; formatNdx <= VK_FORMAT_G16_B16R16_2PLANE_444_UNORM_EXT; formatNdx++)
	{
		addTests(formatNdx);
	}

	return testGroup.release();
}

} // namespace

tcu::TestCaseGroup* createStorageImageWriteTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "storage_image_write", "Writing to YCbCr storage images"));
	return populateStorageImageWriteFormatGroup(testCtx, testGroup);
}

} // ycbcr
} // vkt
