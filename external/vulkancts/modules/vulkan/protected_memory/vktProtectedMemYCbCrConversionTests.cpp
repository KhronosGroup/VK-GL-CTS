/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 The Khronos Group Inc.
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
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
 * \brief Protected memory YCbCr image conversion tests
 *//*--------------------------------------------------------------------*/

#include "vktProtectedMemYCbCrConversionTests.hpp"

#include "tcuImageCompare.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTestLog.hpp"

#include "vkBuilderUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkPrograms.hpp"
#include "vkTypeUtil.hpp"
#include "vkYCbCrImageWithMemory.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "vktProtectedMemContext.hpp"
#include "vktProtectedMemUtils.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktYCbCrUtil.hpp"


namespace vkt
{
namespace ProtectedMem
{

namespace
{
static const vk::VkFormat	s_colorFormat	= vk::VK_FORMAT_R8G8B8A8_UNORM;

enum {
	CHECK_SIZE	= 50,
};

struct YCbCrValidationData {
	tcu::Vec4	coord;
	tcu::Vec4	minBound;
	tcu::Vec4	maxBound;
};

std::vector<tcu::Vec2> computeVertexPositions (int numValues, const tcu::IVec2& renderSize)
{
	std::vector<tcu::Vec2> positions(numValues);
	for (int valNdx = 0; valNdx < numValues; valNdx++)
	{
		const int	ix	= valNdx % renderSize.x();
		const int	iy	= valNdx / renderSize.x();
		const float	fx	= -1.0f + 2.0f*((float(ix) + 0.5f) / float(renderSize.x()));
		const float	fy	= -1.0f + 2.0f*((float(iy) + 0.5f) / float(renderSize.y()));

		positions[valNdx] = tcu::Vec2(fx, fy);
	}

	return positions;
}

void genTexCoords (std::vector<tcu::Vec2>& coords, const tcu::UVec2& size)
{
	for (deUint32 y = 0; y < size.y(); y++)
	for (deUint32 x = 0; x < size.x(); x++)
	{
		const float	fx	= (float)x;
		const float	fy	= (float)y;

		const float	fw	= (float)size.x();
		const float	fh	= (float)size.y();

		const float	s	= 1.5f * ((fx * 1.5f * fw + fx) / (1.5f * fw * 1.5f * fw)) - 0.25f;
		const float	t	= 1.5f * ((fy * 1.5f * fh + fy) / (1.5f * fh * 1.5f * fh)) - 0.25f;

		coords.push_back(tcu::Vec2(s, t));
	}
}

struct TestConfig
{
	TestConfig	(glu::ShaderType						shaderType_,
				 vk::VkFormat							format_,
				 vk::VkImageTiling						imageTiling_,
				 vk::VkFilter							textureFilter_,
				 vk::VkSamplerAddressMode				addressModeU_,
				 vk::VkSamplerAddressMode				addressModeV_,

				 vk::VkFilter							chromaFilter_,
				 vk::VkChromaLocation					xChromaOffset_,
				 vk::VkChromaLocation					yChromaOffset_,
				 bool									explicitReconstruction_,
				 bool									disjoint_,

				 vk::VkSamplerYcbcrRange				colorRange_,
				 vk::VkSamplerYcbcrModelConversion		colorModel_,
				 vk::VkComponentMapping					componentMapping_)
		: shaderType				(shaderType_)
		, format					(format_)
		, imageTiling				(imageTiling_)
		, textureFilter				(textureFilter_)
		, addressModeU				(addressModeU_)
		, addressModeV				(addressModeV_)

		, chromaFilter				(chromaFilter_)
		, xChromaOffset				(xChromaOffset_)
		, yChromaOffset				(yChromaOffset_)
		, explicitReconstruction	(explicitReconstruction_)
		, disjoint					(disjoint_)

		, colorRange				(colorRange_)
		, colorModel				(colorModel_)
		, componentMapping			(componentMapping_)
	{
	}

	glu::ShaderType							shaderType;
	vk::VkFormat							format;
	vk::VkImageTiling						imageTiling;
	vk::VkFilter							textureFilter;
	vk::VkSamplerAddressMode				addressModeU;
	vk::VkSamplerAddressMode				addressModeV;

	vk::VkFilter							chromaFilter;
	vk::VkChromaLocation					xChromaOffset;
	vk::VkChromaLocation					yChromaOffset;
	bool									explicitReconstruction;
	bool									disjoint;

	vk::VkSamplerYcbcrRange					colorRange;
	vk::VkSamplerYcbcrModelConversion		colorModel;
	vk::VkComponentMapping					componentMapping;
};

void checkSupport (Context& context, const TestConfig)
{
	checkProtectedQueueSupport(context);
}

void validateFormatSupport (ProtectedContext& context, TestConfig& config)
{
	tcu::TestLog&						log			(context.getTestContext().getLog());

	try
	{
		const vk::VkFormatProperties	properties	(vk::getPhysicalDeviceFormatProperties(context.getInstanceDriver(), context.getPhysicalDevice(), config.format));
		const vk::VkFormatFeatureFlags	features	(config.imageTiling == vk::VK_IMAGE_TILING_OPTIMAL
													? properties.optimalTilingFeatures
													: properties.linearTilingFeatures);

		if ((features & (vk::VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT | vk::VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT)) == 0)
			TCU_THROW(NotSupportedError, "Format doesn't support YCbCr conversions");

		if ((features & vk::VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) == 0)
			TCU_THROW(NotSupportedError, "Format doesn't support sampling");

		if (config.textureFilter == vk::VK_FILTER_LINEAR && ((features & vk::VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT) == 0))
			TCU_THROW(NotSupportedError, "Format doesn't support YCbCr linear chroma reconstruction");

		if (config.chromaFilter == vk::VK_FILTER_LINEAR && ((features & vk::VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT) == 0))
			TCU_THROW(NotSupportedError, "Format doesn't support YCbCr linear chroma reconstruction");

		if (config.chromaFilter != config.textureFilter && ((features & vk::VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_SEPARATE_RECONSTRUCTION_FILTER_BIT) == 0))
			TCU_THROW(NotSupportedError, "Format doesn't support different chroma and texture filters");

		if (config.explicitReconstruction && ((features & vk::VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_FORCEABLE_BIT) == 0))
			TCU_THROW(NotSupportedError, "Format doesn't support explicit chroma reconstruction");

		if (config.disjoint && ((features & vk::VK_FORMAT_FEATURE_DISJOINT_BIT) == 0))
			TCU_THROW(NotSupportedError, "Format doesn't disjoint planes");

		if (ycbcr::isXChromaSubsampled(config.format) && (config.xChromaOffset == vk::VK_CHROMA_LOCATION_COSITED_EVEN) && ((features & vk::VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT) == 0))
			TCU_THROW(NotSupportedError, "Format doesn't support cosited chroma samples");

		if (ycbcr::isXChromaSubsampled(config.format) && (config.xChromaOffset == vk::VK_CHROMA_LOCATION_MIDPOINT) && ((features & vk::VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT) == 0))
			TCU_THROW(NotSupportedError, "Format doesn't support midpoint chroma samples");

		if (ycbcr::isYChromaSubsampled(config.format) && (config.yChromaOffset == vk::VK_CHROMA_LOCATION_COSITED_EVEN) && ((features & vk::VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT) == 0))
			TCU_THROW(NotSupportedError, "Format doesn't support cosited chroma samples");

		if (ycbcr::isYChromaSubsampled(config.format) && (config.yChromaOffset == vk::VK_CHROMA_LOCATION_MIDPOINT) && ((features & vk::VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT) == 0))
			TCU_THROW(NotSupportedError, "Format doesn't support midpoint chroma samples");

		if ((features & vk::VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_BIT) != 0)
			config.explicitReconstruction = true;

		log << tcu::TestLog::Message << "FormatFeatures: " << vk::getFormatFeatureFlagsStr(features) << tcu::TestLog::EndMessage;
	}
	catch (const vk::Error& err)
	{
		if (err.getError() == vk::VK_ERROR_FORMAT_NOT_SUPPORTED)
			TCU_THROW(NotSupportedError, "Format not supported");

		throw;
	}
}

vk::Move<vk::VkSampler> createSampler (const vk::DeviceInterface&				vkd,
									   const vk::VkDevice						device,
									   const vk::VkFilter						textureFilter,
									   const vk::VkSamplerAddressMode			addressModeU,
									   const vk::VkSamplerAddressMode			addressModeV,
									   const vk::VkSamplerYcbcrConversion		conversion)
{
	const vk::VkSamplerYcbcrConversionInfo		samplerConversionInfo	=
	{
		vk::VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO,
		DE_NULL,
		conversion
	};

	const vk::VkSamplerCreateInfo	createInfo	=
	{
		vk::VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		&samplerConversionInfo,
		0u,
		textureFilter,
		textureFilter,
		vk::VK_SAMPLER_MIPMAP_MODE_NEAREST,
		addressModeU,
		addressModeV,
		vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		0.0f,
		VK_FALSE,
		1.0f,
		VK_FALSE,
		vk::VK_COMPARE_OP_ALWAYS,
		0.0f,
		0.0f,
		vk::VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
		VK_FALSE,
	};

	return createSampler(vkd, device, &createInfo);
}

vk::Move<vk::VkImageView> createImageView (const vk::DeviceInterface&				vkd,
										   const vk::VkDevice						device,
										   const vk::VkImage						image,
										   const vk::VkFormat						format,
										   const vk::VkSamplerYcbcrConversion		conversion)
{
	const vk::VkSamplerYcbcrConversionInfo		conversionInfo	=
	{
		vk::VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO,
		DE_NULL,
		conversion
	};

	const vk::VkImageViewCreateInfo				viewInfo		=
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		&conversionInfo,
		(vk::VkImageViewCreateFlags)0,
		image,
		vk::VK_IMAGE_VIEW_TYPE_2D,
		format,
		{
			vk::VK_COMPONENT_SWIZZLE_IDENTITY,
			vk::VK_COMPONENT_SWIZZLE_IDENTITY,
			vk::VK_COMPONENT_SWIZZLE_IDENTITY,
			vk::VK_COMPONENT_SWIZZLE_IDENTITY,
		},
		{ vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u },
	};

	return vk::createImageView(vkd, device, &viewInfo);
}

vk::Move<vk::VkSamplerYcbcrConversion> createConversion (const vk::DeviceInterface&					vkd,
														const vk::VkDevice							device,
														const vk::VkFormat							format,
														const vk::VkSamplerYcbcrModelConversion		colorModel,
														const vk::VkSamplerYcbcrRange				colorRange,
														const vk::VkChromaLocation					xChromaOffset,
														const vk::VkChromaLocation					yChromaOffset,
														const vk::VkFilter							chromaFilter,
														const vk::VkComponentMapping&				componentMapping,
														const bool									explicitReconstruction)
{
	const vk::VkSamplerYcbcrConversionCreateInfo	conversionInfo	=
	{
		vk::VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO,
		DE_NULL,

		format,
		colorModel,
		colorRange,
		componentMapping,
		xChromaOffset,
		yChromaOffset,
		chromaFilter,
		explicitReconstruction ? VK_TRUE : VK_FALSE
	};

	return vk::createSamplerYcbcrConversion(vkd, device, &conversionInfo);
}

void uploadYCbCrImage (ProtectedContext&					ctx,
					   const vk::VkImage					image,
					   const ycbcr::MultiPlaneImageData&	imageData,
					   const vk::VkAccessFlags				nextAccess,
					   const vk::VkImageLayout				finalLayout)
{
	const vk::DeviceInterface&				vk					= ctx.getDeviceInterface();
	const vk::VkDevice						device				= ctx.getDevice();
	const vk::VkQueue						queue				= ctx.getQueue();
	const deUint32							queueFamilyIndex	= ctx.getQueueFamilyIndex();

	const vk::Unique<vk::VkCommandPool>		cmdPool				(makeCommandPool(vk, device, PROTECTION_ENABLED, queueFamilyIndex));
	const vk::Unique<vk::VkCommandBuffer>	cmdBuffer			(vk::allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const vk::PlanarFormatDescription&		formatDesc			= imageData.getDescription();

	std::vector<de::SharedPtr<de::MovePtr<vk::BufferWithMemory> > > stagingBuffers;
	std::vector<vk::VkBufferMemoryBarrier>	bufferBarriers;

	for (deUint32 planeNdx = 0; planeNdx < imageData.getDescription().numPlanes; ++planeNdx)
	{
		de::MovePtr<vk::BufferWithMemory> buffer	(makeBuffer(ctx,
																		PROTECTION_DISABLED,
																		queueFamilyIndex,
																		(deUint32)imageData.getPlaneSize(planeNdx),
																		vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT|vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT,
																		vk::MemoryRequirement::HostVisible));

		const vk::VkBufferMemoryBarrier		bufferBarrier	=
		{
			vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
			DE_NULL,
			(vk::VkAccessFlags)0,
			vk::VK_ACCESS_TRANSFER_READ_BIT,
			queueFamilyIndex,
			queueFamilyIndex,
			**buffer,
			0,
			(deUint32)imageData.getPlaneSize(planeNdx)
		};
		bufferBarriers.push_back(bufferBarrier);

		deMemcpy(buffer->getAllocation().getHostPtr(), imageData.getPlanePtr(planeNdx), imageData.getPlaneSize(planeNdx));
		flushAlloc(vk, device, buffer->getAllocation());
		stagingBuffers.push_back(de::SharedPtr<de::MovePtr<vk::BufferWithMemory> >(new de::MovePtr<vk::BufferWithMemory>(buffer.release())));
	}


	beginCommandBuffer(vk, *cmdBuffer);

	for (deUint32 planeNdx = 0; planeNdx < imageData.getDescription().numPlanes; ++planeNdx)
	{
		const vk::VkImageAspectFlags	aspect	= formatDesc.numPlanes > 1
												? vk::getPlaneAspect(planeNdx)
												: vk::VK_IMAGE_ASPECT_COLOR_BIT;

		const vk::VkImageMemoryBarrier		preCopyBarrier	=
		{
			vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			DE_NULL,
			(vk::VkAccessFlags)0,
			vk::VK_ACCESS_TRANSFER_WRITE_BIT,
			vk::VK_IMAGE_LAYOUT_UNDEFINED,
			vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			queueFamilyIndex,
			queueFamilyIndex,
			image,
			{ aspect, 0u, 1u, 0u, 1u }
		};

		vk.cmdPipelineBarrier(*cmdBuffer,
								(vk::VkPipelineStageFlags)vk::VK_PIPELINE_STAGE_HOST_BIT,
								(vk::VkPipelineStageFlags)vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
								(vk::VkDependencyFlags)0u,
								0u, (const vk::VkMemoryBarrier*)DE_NULL,
								(deUint32)bufferBarriers.size(), &bufferBarriers[0],
								1u, &preCopyBarrier);
	}

	for (deUint32 planeNdx = 0; planeNdx < imageData.getDescription().numPlanes; ++planeNdx)
	{
		const vk::VkImageAspectFlagBits	aspect	= (formatDesc.numPlanes > 1)
												? vk::getPlaneAspect(planeNdx)
												: vk::VK_IMAGE_ASPECT_COLOR_BIT;
		const deUint32					planeW	= (formatDesc.numPlanes > 1)
												? imageData.getSize().x() / formatDesc.planes[planeNdx].widthDivisor
												: imageData.getSize().x();
		const deUint32					planeH	= (formatDesc.numPlanes > 1)
												? imageData.getSize().y() / formatDesc.planes[planeNdx].heightDivisor
												: imageData.getSize().y();
		const vk::VkBufferImageCopy		copy	=
		{
			0u,		// bufferOffset
			0u,		// bufferRowLength
			0u,		// bufferImageHeight
			{ (vk::VkImageAspectFlags)aspect, 0u, 0u, 1u },
			vk::makeOffset3D(0u, 0u, 0u),
			vk::makeExtent3D(planeW, planeH, 1u),
		};

		vk.cmdCopyBufferToImage(*cmdBuffer, ***stagingBuffers[planeNdx], image, vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &copy);
	}

	for (deUint32 planeNdx = 0; planeNdx < imageData.getDescription().numPlanes; ++planeNdx)
	{
		const vk::VkImageAspectFlags	aspect	= formatDesc.numPlanes > 1
												? vk::getPlaneAspect(planeNdx)
												: vk::VK_IMAGE_ASPECT_COLOR_BIT;

		const vk::VkImageMemoryBarrier		postCopyBarrier	=
		{
			vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			DE_NULL,
			vk::VK_ACCESS_TRANSFER_WRITE_BIT,
			nextAccess,
			vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			finalLayout,
			VK_QUEUE_FAMILY_IGNORED,
			VK_QUEUE_FAMILY_IGNORED,
			image,
			{ aspect, 0u, 1u, 0u, 1u }
		};

		vk.cmdPipelineBarrier(*cmdBuffer,
								(vk::VkPipelineStageFlags)vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
								(vk::VkPipelineStageFlags)vk::VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
								(vk::VkDependencyFlags)0u,
								0u, (const vk::VkMemoryBarrier*)DE_NULL,
								0u, (const vk::VkBufferMemoryBarrier*)DE_NULL,
								1u, &postCopyBarrier);
	}

	endCommandBuffer(vk, *cmdBuffer);

	{
		const vk::Unique<vk::VkFence>	fence		(createFence(vk, device));
		VK_CHECK(queueSubmit(ctx, PROTECTION_ENABLED, queue, *cmdBuffer, *fence, ~0ull));
	}
}

void logTestCaseInfo (tcu::TestLog& log, const TestConfig& config)
{
	log << tcu::TestLog::Message << "ShaderType: " << config.shaderType << tcu::TestLog::EndMessage;
	log << tcu::TestLog::Message << "Format: "  << config.format << tcu::TestLog::EndMessage;
	log << tcu::TestLog::Message << "ImageTiling: " << config.imageTiling << tcu::TestLog::EndMessage;
	log << tcu::TestLog::Message << "TextureFilter: " << config.textureFilter << tcu::TestLog::EndMessage;
	log << tcu::TestLog::Message << "AddressModeU: " << config.addressModeU << tcu::TestLog::EndMessage;
	log << tcu::TestLog::Message << "AddressModeV: " << config.addressModeV << tcu::TestLog::EndMessage;
	log << tcu::TestLog::Message << "ChromaFilter: " << config.chromaFilter << tcu::TestLog::EndMessage;
	log << tcu::TestLog::Message << "XChromaOffset: " << config.xChromaOffset << tcu::TestLog::EndMessage;
	log << tcu::TestLog::Message << "YChromaOffset: " << config.yChromaOffset << tcu::TestLog::EndMessage;
	log << tcu::TestLog::Message << "ExplicitReconstruction: " << (config.explicitReconstruction ? "true" : "false") << tcu::TestLog::EndMessage;
	log << tcu::TestLog::Message << "Disjoint: " << (config.disjoint ? "true" : "false") << tcu::TestLog::EndMessage;
	log << tcu::TestLog::Message << "ColorRange: " << config.colorRange << tcu::TestLog::EndMessage;
	log << tcu::TestLog::Message << "ColorModel: " << config.colorModel << tcu::TestLog::EndMessage;
	log << tcu::TestLog::Message << "ComponentMapping: " << config.componentMapping << tcu::TestLog::EndMessage;
}

void logBoundImages (tcu::TestLog& log, const tcu::UVec2 size, const std::vector<tcu::Vec4>& minBounds, const std::vector<tcu::Vec4>& maxBounds)
{
	tcu::TextureLevel	minImage	(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::FLOAT), size.x(), size.y());
	tcu::TextureLevel	maxImage	(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::FLOAT), size.x(), size.y());

	for (int y = 0; y < (int)(size.y()); y++)
	for (int x = 0; x < (int)(size.x()); x++)
	{
		const int ndx = x + y * (int)(size.x());
		minImage.getAccess().setPixel(minBounds[ndx], x, y);
		maxImage.getAccess().setPixel(maxBounds[ndx], x, y);
	}

	const tcu::Vec4	scale	(1.0f);
	const tcu::Vec4	bias	(0.0f);

	log << tcu::TestLog::Image("MinBoundImage", "MinBoundImage", minImage.getAccess(), scale, bias);
	log << tcu::TestLog::Image("MaxBoundImage", "MaxBoundImage", maxImage.getAccess(), scale, bias);
}

bool validateImage (ProtectedContext&							ctx,
					 const std::vector<YCbCrValidationData>&	refData,
					 const vk::VkSampler						sampler,
					 const vk::VkImageView						imageView,
					 const deUint32								combinedSamplerDescriptorCount)
{
	{
		tcu::TestLog&	log	(ctx.getTestContext().getLog());

		log << tcu::TestLog::Message << "Reference values:" << tcu::TestLog::EndMessage;
		for (deUint32 ndx = 0; ndx < refData.size(); ndx++)
		{
			log << tcu::TestLog::Message << (ndx + 1) << refData[ndx].coord << ": [" << refData[ndx].minBound << ", " << refData[ndx].maxBound << "]" << tcu::TestLog::EndMessage;
		}
	}

	const deUint64								oneSec				= 1000 * 1000 * 1000;

	const vk::DeviceInterface&					vk					= ctx.getDeviceInterface();
	const vk::VkDevice							device				= ctx.getDevice();
	const vk::VkQueue							queue				= ctx.getQueue();
	const deUint32								queueFamilyIndex	= ctx.getQueueFamilyIndex();

	DE_ASSERT(refData.size() >= CHECK_SIZE && CHECK_SIZE > 0);
	const deUint32								refUniformSize		= (deUint32)(sizeof(YCbCrValidationData) * refData.size());
	const de::UniquePtr<vk::BufferWithMemory>	refUniform			(makeBuffer(ctx,
																				PROTECTION_DISABLED,
																				queueFamilyIndex,
																				refUniformSize,
																				vk::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
																				vk::MemoryRequirement::HostVisible));

	// Set the reference uniform data
	{
		deMemcpy(refUniform->getAllocation().getHostPtr(), &refData[0], refUniformSize);
		flushAlloc(vk, device, refUniform->getAllocation());
	}

	const deUint32								helperBufferSize	= (deUint32)(2 * sizeof(deUint32));
	const de::MovePtr<vk::BufferWithMemory>		helperBuffer		(makeBuffer(ctx,
																				PROTECTION_ENABLED,
																				queueFamilyIndex,
																				helperBufferSize,
																				vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
																				vk::MemoryRequirement::Protected));
	const vk::Unique<vk::VkShaderModule>		resetSSBOShader		(vk::createShaderModule(vk, device, ctx.getBinaryCollection().get("ResetSSBO"), 0));
	const vk::Unique<vk::VkShaderModule>		validatorShader		(vk::createShaderModule(vk, device, ctx.getBinaryCollection().get("ImageValidator"), 0));

	// Create descriptors
	const vk::Unique<vk::VkDescriptorSetLayout>	descriptorSetLayout(vk::DescriptorSetLayoutBuilder()
		.addSingleSamplerBinding(vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, vk::VK_SHADER_STAGE_COMPUTE_BIT, &sampler)
		.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT)
		.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT)
		.build(vk, device));
	const vk::Unique<vk::VkDescriptorPool>		descriptorPool(vk::DescriptorPoolBuilder()
		.addType(vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, combinedSamplerDescriptorCount)
		.addType(vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u)
		.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u)
		.build(vk, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));
	const vk::Unique<vk::VkDescriptorSet>		descriptorSet		(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

	// Update descriptor set infirmation
	{
		vk::VkDescriptorBufferInfo	descRefUniform	= makeDescriptorBufferInfo(**refUniform, 0, refUniformSize);
		vk::VkDescriptorBufferInfo	descBuffer		= makeDescriptorBufferInfo(**helperBuffer, 0, helperBufferSize);
		vk::VkDescriptorImageInfo	descSampledImg	= makeDescriptorImageInfo(sampler, imageView, vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		vk::DescriptorSetUpdateBuilder()
			.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &descSampledImg)
			.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1u), vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &descRefUniform)
			.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(2u), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descBuffer)
			.update(vk, device);
	}

	const vk::Unique<vk::VkPipelineLayout>		pipelineLayout		(makePipelineLayout(vk, device, *descriptorSetLayout));
	const vk::Unique<vk::VkCommandPool>			cmdPool				(makeCommandPool(vk, device, PROTECTION_ENABLED, queueFamilyIndex));

	// Reset helper SSBO
	{
		const vk::Unique<vk::VkFence>			fence				(vk::createFence(vk, device));
		const vk::Unique<vk::VkPipeline>		resetSSBOPipeline	(makeComputePipeline(vk, device, *pipelineLayout, *resetSSBOShader, DE_NULL));
		const vk::Unique<vk::VkCommandBuffer>	resetCmdBuffer		(vk::allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));
		beginCommandBuffer(vk, *resetCmdBuffer);

		vk.cmdBindPipeline(*resetCmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *resetSSBOPipeline);
		vk.cmdBindDescriptorSets(*resetCmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &*descriptorSet, 0u, DE_NULL);
		vk.cmdDispatch(*resetCmdBuffer, 1u, 1u, 1u);

		endCommandBuffer(vk, *resetCmdBuffer);
		VK_CHECK(queueSubmit(ctx, PROTECTION_ENABLED, queue, *resetCmdBuffer, *fence, ~0ull));
	}

	// Create validation compute commands & submit
	vk::VkResult							queueSubmitResult;
	{
		const vk::Unique<vk::VkFence>			fence				(vk::createFence(vk, device));
		const vk::Unique<vk::VkPipeline>		validationPipeline	(makeComputePipeline(vk, device, *pipelineLayout, *validatorShader, DE_NULL));
		const vk::Unique<vk::VkCommandBuffer>	cmdBuffer			(vk::allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

		beginCommandBuffer(vk, *cmdBuffer);

		vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *validationPipeline);
		vk.cmdBindDescriptorSets(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &*descriptorSet, 0u, DE_NULL);
		vk.cmdDispatch(*cmdBuffer, CHECK_SIZE, 1u, 1u);

		endCommandBuffer(vk, *cmdBuffer);

		queueSubmitResult = queueSubmit(ctx, PROTECTION_ENABLED, queue, *cmdBuffer, *fence, oneSec * 5);
	}

	// \todo do we need to check the fence status?
	if (queueSubmitResult == vk::VK_TIMEOUT)
		return false;

	// at this point the submit result should be VK_TRUE
	VK_CHECK(queueSubmitResult);
	return true;
}

void testShaders (vk::SourceCollections& dst, const TestConfig config)
{
	const char* const	shaderHeader		=
			"layout(constant_id = 1) const float threshold = 0.01f;\n"
			"layout(set = 0, binding = 0) uniform highp sampler2D protectedImage;\n"
			"\n"
			"struct validationData {\n"
			"    highp vec4 imageCoord;\n"
			"    highp vec4 imageRefMinBound;\n"
			"    highp vec4 imageRefMaxBound;\n"
			"};\n"
			"layout(std140, set = 0, binding = 1) uniform Data\n"
			"{\n"
			"    validationData ref[250];\n"
			"};\n";

	const char* const	compareFunction	=
			"bool compare(highp vec4 value, highp vec4 minValue, highp vec4 maxValue)\n"
			"{\n"
			"    return all(greaterThanEqual(value, minValue - threshold)) && all(lessThanEqual(value, maxValue + threshold));\n"
			"}\n";

	std::map<std::string, std::string> validatorSpec;
	validatorSpec["CHECK_SIZE"]			= de::toString((deUint32)CHECK_SIZE);
	validatorSpec["SHADER_HEADER"]		= shaderHeader;
	validatorSpec["COMPARE_FUNCTION"]	= compareFunction;

	const char* const validatorShader =
		"#version 450\n"
		"\n"
		"${SHADER_HEADER}"
		"\n"
		"layout(std140, set = 0, binding = 2) buffer ProtectedHelper\n"
		"{\n"
		"    highp uint zero;\n"
		"    highp uint dummyOut;\n"
		"} helper;\n"
		"\n"
		"void error()\n"
		"{\n"
		"    for (uint x = 0u; x < 10u; x += helper.zero)\n"
		"        atomicAdd(helper.dummyOut, 1u);\n"
		"}\n"
		"\n"
		"${COMPARE_FUNCTION}"
		"\n"
		"void main(void)\n"
		"{\n"
		"    int idx = int(gl_GlobalInvocationID.x);\n"
		"    vec4 currentValue = texture(protectedImage, ref[idx].imageCoord.xy);\n"
		"    if (!compare(currentValue, ref[idx].imageRefMinBound, ref[idx].imageRefMaxBound))\n"
		"    {\n"
		"      error();\n"
		"    }\n"
		"}\n";

	const char* const resetSSBOShader =
		"#version 450\n"
		"layout(local_size_x = 1) in;\n"
		"\n"
		"layout(std140, set=0, binding=2) buffer ProtectedHelper\n"
		"{\n"
		"    highp uint zero; // set to 0\n"
		"    highp uint dummyOut;\n"
		"} helper;\n"
		"\n"
		"void main (void)\n"
		"{\n"
		"    helper.zero = 0;\n"
		"    helper.dummyOut = 0;\n"
		"}\n";

	dst.glslSources.add("ResetSSBO") << glu::ComputeSource(resetSSBOShader);
	dst.glslSources.add("ImageValidator") << glu::ComputeSource(tcu::StringTemplate(validatorShader).specialize(validatorSpec));

	if (config.shaderType == glu::SHADERTYPE_COMPUTE)
		return; // Bail early as the YCbCr image validator already have the test programs set for compute tests

	const char* const compareOperation =
			"    highp vec4 currentValue = texture(protectedImage, ref[v_idx].imageCoord.xy);\n"
			"    if (compare(currentValue, ref[v_idx].imageRefMinBound, ref[v_idx].imageRefMaxBound))\n"
			"    {\n"
			"        o_color = vec4(0.0f, 1.0f, 0.0f, 1.0f);\n"	// everything is ok, green
			"    }\n"
			"    else"
			"    {\n"
			"        o_color = vec4(1.0f, 0.0f, 0.0f, 1.0f);\n"
			"    }\n";

	std::map<std::string, std::string>	shaderSpec;
	shaderSpec["SHADER_HEADER"]		= shaderHeader;
	shaderSpec["COMPARE_FUNCTION"]	= compareFunction;
	shaderSpec["COMPARE_OPERATION"]	= compareOperation;

	if (config.shaderType == glu::SHADERTYPE_VERTEX)
	{
		const char* const vertexShader =
			"#version 450\n"
			"${SHADER_HEADER}\n"
			"\n"
			"layout(location = 0) in highp vec2 a_position;\n"
			"layout(location = 0) flat out highp vec4 o_color;\n"
			"\n"
			"${COMPARE_FUNCTION}"
			"\n"
			"void main(void)\n"
			"{\n"
			"    gl_Position = vec4(a_position, 0.0f, 1.0f);\n"
			"    gl_PointSize = 1.0f;\n"
			"    int v_idx = gl_VertexIndex;\n"
			"${COMPARE_OPERATION}"
			"}\n";

		const char* const fragmentShader =
			"#version 450\n"
			"\n"
			"layout(location = 0) flat in highp vec4 v_color;\n"
			"layout(location = 0) out highp vec4 o_color;\n"
			"\n"
			"void main(void)\n"
			"{\n"
			"    o_color = v_color;\n"
			"}\n";

		dst.glslSources.add("vert") << glu::VertexSource(tcu::StringTemplate(vertexShader).specialize(shaderSpec));
		dst.glslSources.add("frag") << glu::FragmentSource(fragmentShader);
	}
	else if (config.shaderType == glu::SHADERTYPE_FRAGMENT)
	{
		const char* const vertexShader =
			"#version 450\n"
			"layout(location = 0) in highp vec2 a_position;\n"
			"layout(location = 0) flat out highp int o_idx;\n"
			"\n"
			"void main(void)\n"
			"{\n"
			"    gl_Position = vec4(a_position, 0.0f, 1.0f);\n"
			"    gl_PointSize = 1.0f;\n"
			"    o_idx = gl_VertexIndex;\n"
			"}\n";

		const char* const fragmentShader =
			"#version 450\n"
			"${SHADER_HEADER}\n"
			"\n"
			"layout(location = 0) flat in highp int v_idx;\n"
			"layout(location = 0) out highp vec4 o_color;\n"
			"\n"
			"${COMPARE_FUNCTION}"
			"\n"
			"void main(void)\n"
			"{\n"
			"${COMPARE_OPERATION}"
			"}\n";

		dst.glslSources.add("vert") << glu::VertexSource(vertexShader);
		dst.glslSources.add("frag") << glu::FragmentSource(tcu::StringTemplate(fragmentShader).specialize(shaderSpec));
	}
}

de::MovePtr<vk::YCbCrImageWithMemory>	createYcbcrImage2D	(ProtectedContext&				context,
															 const ProtectionMode			protectionMode,
															 const deUint32					width,
															 const deUint32					height,
															 const vk::VkFormat				format,
															 const vk::VkImageCreateFlags	createFlags,
															 const vk::VkImageUsageFlags	usageFlags)
{
	const vk::DeviceInterface&	vk			= context.getDeviceInterface();
	const vk::VkDevice&			device		= context.getDevice();
	vk::Allocator&				allocator	= context.getDefaultAllocator();
	const deUint32				queueIdx	= context.getQueueFamilyIndex();
#ifndef NOT_PROTECTED
	const deUint32				flags		= (protectionMode == PROTECTION_ENABLED) ? vk::VK_IMAGE_CREATE_PROTECTED_BIT : 0x0;
	const vk::MemoryRequirement	memReq		= (protectionMode == PROTECTION_ENABLED) ? vk::MemoryRequirement::Protected : vk::MemoryRequirement::Any;
#else
	const deUint32				flags		= 0x0;
	const vk::MemoryRequirement	memReq		= vk::MemoryRequirement::Any;
	DE_UNREF(protectionMode);
#endif

	const vk::VkImageCreateInfo	params		=
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,		// VkStructureType			stype
		DE_NULL,										// const void*				pNext
		(vk::VkImageCreateFlags)(flags | createFlags),	// VkImageCreateFlags		flags
		vk::VK_IMAGE_TYPE_2D,							// VkImageType				imageType
		format,											// VkFormat					format
		{ width, height, 1 },							// VkExtent3D				extent
		1u,												// deUint32					mipLevels
		1u,												// deUint32					arrayLayers
		vk::VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits	samples
		vk::VK_IMAGE_TILING_OPTIMAL,					// VkImageTiling			tiling
		usageFlags,										// VkImageUsageFlags		usage
		vk::VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode			sharingMode
		1u,												// deUint32					queueFamilyIndexCount
		&queueIdx,										// const deUint32*			pQueueFamilyIndices
		vk::VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout			initialLayout
	};

	return de::MovePtr<vk::YCbCrImageWithMemory>(new vk::YCbCrImageWithMemory(vk, device, allocator, params, memReq));
}


void renderYCbCrToColor (ProtectedContext&							ctx,
						 const tcu::UVec2							size,
						 const vk::VkSampler						ycbcrSampler,
						 const vk::VkImageView						ycbcrImageView,
						 const vk::VkImage							colorImage,
						 const vk::VkImageView						colorImageView,
						 const std::vector<YCbCrValidationData>&	referenceData,
						 const std::vector<tcu::Vec2>&				posCoords,
						 const deUint32								combinedSamplerDescriptorCount)
{
	const vk::DeviceInterface&					vk					= ctx.getDeviceInterface();
	const vk::VkDevice							device				= ctx.getDevice();
	const vk::VkQueue							queue				= ctx.getQueue();
	const deUint32								queueFamilyIndex	= ctx.getQueueFamilyIndex();

	const vk::Unique<vk::VkRenderPass>			renderPass			(createRenderPass(ctx, s_colorFormat));
	const vk::Unique<vk::VkFramebuffer>			framebuffer			(createFramebuffer(ctx, size.x(), size.y(), *renderPass, colorImageView));
	const vk::Unique<vk::VkShaderModule>		vertexShader		(createShaderModule(vk, device, ctx.getBinaryCollection().get("vert"), 0));
	const vk::Unique<vk::VkShaderModule>		fragmentShader		(createShaderModule(vk, device, ctx.getBinaryCollection().get("frag"), 0));
	const vk::Unique<vk::VkDescriptorSetLayout>	descriptorSetLayout (vk::DescriptorSetLayoutBuilder()
																		.addSingleSamplerBinding(vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
																							 vk::VK_SHADER_STAGE_ALL,
																							 &ycbcrSampler)
																		.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, vk::VK_SHADER_STAGE_ALL)
																		.build(vk, device));
	const vk::Unique<vk::VkDescriptorPool>		descriptorPool		(vk::DescriptorPoolBuilder()
																		.addType(vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, combinedSamplerDescriptorCount)
																		.addType(vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u)
																		.build(vk, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));
	const vk::Unique<vk::VkDescriptorSet>		descriptorSet		(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));
	const vk::Unique<vk::VkPipelineLayout>		pipelineLayout		(makePipelineLayout(vk, device, *descriptorSetLayout));


	const deUint32								refUniformSize		= (deUint32)(sizeof(YCbCrValidationData) * referenceData.size());
	const de::UniquePtr<vk::BufferWithMemory>	refUniform			(makeBuffer(ctx,
																				PROTECTION_DISABLED,
																				queueFamilyIndex,
																				refUniformSize,
																				vk::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
																				vk::MemoryRequirement::HostVisible));

	// Set the reference uniform data
	{
		deMemcpy(refUniform->getAllocation().getHostPtr(), &referenceData[0], refUniformSize);
		flushAlloc(vk, device, refUniform->getAllocation());
	}

	// Update descriptor set
	{
		vk::VkDescriptorImageInfo	ycbcrSampled	(makeDescriptorImageInfo(ycbcrSampler, ycbcrImageView, vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
		vk::VkDescriptorBufferInfo	descRefUniform	= makeDescriptorBufferInfo(**refUniform, 0, refUniformSize);
		vk::DescriptorSetUpdateBuilder()
			.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &ycbcrSampled)
			.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1u), vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &descRefUniform)
			.update(vk, device);
	}

	VertexBindings							vertexBindings;
	VertexAttribs							vertexAttribs;
	de::MovePtr<vk::BufferWithMemory>		vertexBuffer;
	{
		const deUint32	bufferSize		= (deUint32)(sizeof(tcu::Vec2) * posCoords.size());
		{
			const vk::VkVertexInputBindingDescription	inputBinding	=
			{
				0u,									// deUint32					binding;
				sizeof(tcu::Vec2),					// deUint32					strideInBytes;
				vk::VK_VERTEX_INPUT_RATE_VERTEX		// VkVertexInputStepRate	inputRate;
			};
			const vk::VkVertexInputAttributeDescription	inputAttribute	=
			{
				0u,									// deUint32	location;
				0u,									// deUint32	binding;
				vk::VK_FORMAT_R32G32_SFLOAT,		// VkFormat	format;
				0u									// deUint32	offsetInBytes;
			};

			vertexBindings.push_back(inputBinding);
			vertexAttribs.push_back(inputAttribute);
		}

		vertexBuffer = makeBuffer(ctx,
								  PROTECTION_DISABLED,
								  queueFamilyIndex,
								  bufferSize,
								  vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
								  vk::MemoryRequirement::HostVisible);

		deMemcpy(vertexBuffer->getAllocation().getHostPtr(), &posCoords[0], bufferSize);
		flushAlloc(vk, device, vertexBuffer->getAllocation());
	}

	const vk::Unique<vk::VkPipeline>		pipeline			(makeGraphicsPipeline(vk,
																					  device,
																					  *pipelineLayout,
																					  *renderPass,
																					  *vertexShader,
																					  *fragmentShader,
																					  vertexBindings,
																					  vertexAttribs,
																					  size,
																					  vk::VK_PRIMITIVE_TOPOLOGY_POINT_LIST));
	const vk::Unique<vk::VkCommandPool>		cmdPool			(makeCommandPool(vk, device, PROTECTION_ENABLED, queueFamilyIndex));
	const vk::Unique<vk::VkCommandBuffer>	cmdBuffer		(vk::allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	beginCommandBuffer(vk, *cmdBuffer);
	{
		const vk::VkImageMemoryBarrier	attachmentStartBarrier =
		{
			vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			DE_NULL,
			0u,
			vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			vk::VK_IMAGE_LAYOUT_UNDEFINED,
			vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			queueFamilyIndex,
			queueFamilyIndex,
			colorImage,
			{ vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u }
		};

		vk.cmdPipelineBarrier(*cmdBuffer,
							  (vk::VkPipelineStageFlags)vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
							  (vk::VkPipelineStageFlags)vk::VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
							  (vk::VkDependencyFlags)0u,
							  0u, (const vk::VkMemoryBarrier*)DE_NULL,
							  0u, (const vk::VkBufferMemoryBarrier*)DE_NULL,
							  1u, &attachmentStartBarrier);
	}

	beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, vk::makeRect2D(0, 0, size.x(), size.y()), tcu::Vec4(0.0f, 0.0f, 0.5f, 1.0f));

	vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
	vk.cmdBindDescriptorSets(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u, &*descriptorSet, 0u, DE_NULL);

	{
		const vk::VkDeviceSize vertexBufferOffset = 0;
		vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &**vertexBuffer, &vertexBufferOffset);
	}

	vk.cmdDraw(*cmdBuffer, /*vertexCount*/ (deUint32)posCoords.size(), 1u, 0u, 0u);

	endRenderPass(vk, *cmdBuffer);

	// color attachment render end barrier
	{
		const vk::VkImageMemoryBarrier	attachmentEndBarrier =
		{
			vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			DE_NULL,
			vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			vk::VK_ACCESS_SHADER_READ_BIT,
			vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			queueFamilyIndex,
			queueFamilyIndex,
			colorImage,
			{ vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u }
		};

		vk.cmdPipelineBarrier(*cmdBuffer,
							  (vk::VkPipelineStageFlags)vk::VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
							  (vk::VkPipelineStageFlags)vk::VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
							  (vk::VkDependencyFlags)0u,
							  0u, (const vk::VkMemoryBarrier*)DE_NULL,
							  0u, (const vk::VkBufferMemoryBarrier*)DE_NULL,
							  1u, &attachmentEndBarrier);
	}

	endCommandBuffer(vk, *cmdBuffer);

	// Submit command buffer
	{
		const vk::Unique<vk::VkFence>	fence		(vk::createFence(vk, device));
		VK_CHECK(queueSubmit(ctx, PROTECTION_ENABLED, queue, *cmdBuffer, *fence, ~0ull));
	}
}

void generateYCbCrImage (ProtectedContext&				ctx,
						const TestConfig&				config,
						const tcu::UVec2				size,
						const std::vector<tcu::Vec2>&	texCoords,
						ycbcr::MultiPlaneImageData&		ycbcrSrc,
						std::vector<tcu::Vec4>&			ycbcrMinBounds,
						std::vector<tcu::Vec4>&			ycbcrMaxBounds)
{
	tcu::TestLog&						log						(ctx.getTestContext().getLog());
	const std::vector<tcu::FloatFormat>	filteringPrecision		(ycbcr::getPrecision(config.format));
	const std::vector<tcu::FloatFormat>	conversionPrecision		(ycbcr::getPrecision(config.format));
	const tcu::UVec4					bitDepth				(ycbcr::getYCbCrBitDepth(config.format));
	bool								explicitReconstruction	= config.explicitReconstruction;
	const deUint32						subTexelPrecisionBits	(vk::getPhysicalDeviceProperties(ctx.getInstanceDriver(),
																								 ctx.getPhysicalDevice()).limits.subTexelPrecisionBits);


	const vk::PlanarFormatDescription	planeInfo				(vk::getPlanarFormatDescription(config.format));

	deUint32							nullAccessData			(0u);
	ycbcr::ChannelAccess				nullAccess				(tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT, 1u, tcu::IVec3(size.x(), size.y(), 1), tcu::IVec3(0, 0, 0), &nullAccessData, 0u);
	deUint32							nullAccessAlphaData		(~0u);
	ycbcr::ChannelAccess				nullAccessAlpha			(tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT, 1u, tcu::IVec3(size.x(), size.y(), 1), tcu::IVec3(0, 0, 0), &nullAccessAlphaData, 0u);
	ycbcr::ChannelAccess				rChannelAccess			(planeInfo.hasChannelNdx(0) ? getChannelAccess(ycbcrSrc, planeInfo, size, 0) : nullAccess);
	ycbcr::ChannelAccess				gChannelAccess			(planeInfo.hasChannelNdx(1) ? getChannelAccess(ycbcrSrc, planeInfo, size, 1) : nullAccess);
	ycbcr::ChannelAccess				bChannelAccess			(planeInfo.hasChannelNdx(2) ? getChannelAccess(ycbcrSrc, planeInfo, size, 2) : nullAccess);
	ycbcr::ChannelAccess				aChannelAccess			(planeInfo.hasChannelNdx(3) ? getChannelAccess(ycbcrSrc, planeInfo, size, 3) : nullAccessAlpha);
	const bool							implicitNearestCosited	((config.chromaFilter == vk::VK_FILTER_NEAREST && !explicitReconstruction) &&
																 (config.xChromaOffset == vk::VK_CHROMA_LOCATION_COSITED_EVEN_KHR || config.yChromaOffset == vk::VK_CHROMA_LOCATION_COSITED_EVEN_KHR));

	for (deUint32 planeNdx = 0; planeNdx < planeInfo.numPlanes; planeNdx++)
		deMemset(ycbcrSrc.getPlanePtr(planeNdx), 0u, ycbcrSrc.getPlaneSize(planeNdx));

	// \todo Limit values to only values that produce defined values using selected colorRange and colorModel? The verification code handles those cases already correctly.
	if (planeInfo.hasChannelNdx(0))
	{
		for (int y = 0; y < rChannelAccess.getSize().y(); y++)
		for (int x = 0; x < rChannelAccess.getSize().x(); x++)
			rChannelAccess.setChannel(tcu::IVec3(x, y, 0), (float)x / (float)rChannelAccess.getSize().x());
	}

	if (planeInfo.hasChannelNdx(1))
	{
		for (int y = 0; y < gChannelAccess.getSize().y(); y++)
		for (int x = 0; x < gChannelAccess.getSize().x(); x++)
			gChannelAccess.setChannel(tcu::IVec3(x, y, 0), (float)y / (float)gChannelAccess.getSize().y());
	}

	if (planeInfo.hasChannelNdx(2))
	{
		for (int y = 0; y < bChannelAccess.getSize().y(); y++)
		for (int x = 0; x < bChannelAccess.getSize().x(); x++)
			bChannelAccess.setChannel(tcu::IVec3(x, y, 0), (float)(x + y) / (float)(bChannelAccess.getSize().x() + bChannelAccess.getSize().y()));
	}

	if (planeInfo.hasChannelNdx(3))
	{
		for (int y = 0; y < aChannelAccess.getSize().y(); y++)
		for (int x = 0; x < aChannelAccess.getSize().x(); x++)
			aChannelAccess.setChannel(tcu::IVec3(x, y, 0), (float)(x * y) / (float)(aChannelAccess.getSize().x() * aChannelAccess.getSize().y()));
	}

	std::vector<tcu::Vec4>				uvBounds;
	std::vector<tcu::IVec4>				ijBounds;
	ycbcr::calculateBounds(rChannelAccess, gChannelAccess, bChannelAccess, aChannelAccess, bitDepth, texCoords, filteringPrecision, conversionPrecision, subTexelPrecisionBits, config.textureFilter, config.colorModel, config.colorRange, config.chromaFilter, config.xChromaOffset, config.yChromaOffset, config.componentMapping, explicitReconstruction, config.addressModeU, config.addressModeV, ycbcrMinBounds, ycbcrMaxBounds, uvBounds, ijBounds);

	// Handle case: If implicit reconstruction and chromaFilter == NEAREST, an implementation may behave as if both chroma offsets are MIDPOINT.
	if (implicitNearestCosited)
	{
		std::vector<tcu::Vec4>			relaxedYcbcrMinBounds;
		std::vector<tcu::Vec4>			relaxedYcbcrMaxBounds;

		ycbcr::calculateBounds(rChannelAccess, gChannelAccess, bChannelAccess, aChannelAccess, bitDepth, texCoords, filteringPrecision, conversionPrecision, subTexelPrecisionBits, config.textureFilter, config.colorModel, config.colorRange, config.chromaFilter, vk::VK_CHROMA_LOCATION_MIDPOINT_KHR, vk::VK_CHROMA_LOCATION_MIDPOINT_KHR, config.componentMapping, explicitReconstruction, config.addressModeU, config.addressModeV, relaxedYcbcrMinBounds, relaxedYcbcrMaxBounds, uvBounds, ijBounds);

		DE_ASSERT(relaxedYcbcrMinBounds.size() == ycbcrMinBounds.size());
		DE_ASSERT(relaxedYcbcrMaxBounds.size() == ycbcrMaxBounds.size());

		for (size_t i = 0; i < ycbcrMinBounds.size(); i++)
		{
			ycbcrMinBounds[i] = tcu::Vec4(de::min<float>(ycbcrMinBounds[i].x(), relaxedYcbcrMinBounds[i].x()),
										  de::min<float>(ycbcrMinBounds[i].y(), relaxedYcbcrMinBounds[i].y()),
										  de::min<float>(ycbcrMinBounds[i].z(), relaxedYcbcrMinBounds[i].z()),
										  de::min<float>(ycbcrMinBounds[i].w(), relaxedYcbcrMinBounds[i].w()));

			ycbcrMaxBounds[i] = tcu::Vec4(de::max<float>(ycbcrMaxBounds[i].x(), relaxedYcbcrMaxBounds[i].x()),
										  de::max<float>(ycbcrMaxBounds[i].y(), relaxedYcbcrMaxBounds[i].y()),
										  de::max<float>(ycbcrMaxBounds[i].z(), relaxedYcbcrMaxBounds[i].z()),
										  de::max<float>(ycbcrMaxBounds[i].w(), relaxedYcbcrMaxBounds[i].w()));
		}
	}

	if (vk::isYCbCrFormat(config.format))
	{
		tcu::TextureLevel	rImage	(tcu::TextureFormat(tcu::TextureFormat::R, tcu::TextureFormat::FLOAT), rChannelAccess.getSize().x(), rChannelAccess.getSize().y());
		tcu::TextureLevel	gImage	(tcu::TextureFormat(tcu::TextureFormat::R, tcu::TextureFormat::FLOAT), gChannelAccess.getSize().x(), gChannelAccess.getSize().y());
		tcu::TextureLevel	bImage	(tcu::TextureFormat(tcu::TextureFormat::R, tcu::TextureFormat::FLOAT), bChannelAccess.getSize().x(), bChannelAccess.getSize().y());
		tcu::TextureLevel	aImage	(tcu::TextureFormat(tcu::TextureFormat::R, tcu::TextureFormat::FLOAT), aChannelAccess.getSize().x(), aChannelAccess.getSize().y());

		for (int y = 0; y < (int)rChannelAccess.getSize().y(); y++)
		for (int x = 0; x < (int)rChannelAccess.getSize().x(); x++)
			rImage.getAccess().setPixel(tcu::Vec4(rChannelAccess.getChannel(tcu::IVec3(x, y, 0))), x, y);

		for (int y = 0; y < (int)gChannelAccess.getSize().y(); y++)
		for (int x = 0; x < (int)gChannelAccess.getSize().x(); x++)
			gImage.getAccess().setPixel(tcu::Vec4(gChannelAccess.getChannel(tcu::IVec3(x, y, 0))), x, y);

		for (int y = 0; y < (int)bChannelAccess.getSize().y(); y++)
		for (int x = 0; x < (int)bChannelAccess.getSize().x(); x++)
			bImage.getAccess().setPixel(tcu::Vec4(bChannelAccess.getChannel(tcu::IVec3(x, y, 0))), x, y);

		for (int y = 0; y < (int)aChannelAccess.getSize().y(); y++)
		for (int x = 0; x < (int)aChannelAccess.getSize().x(); x++)
			aImage.getAccess().setPixel(tcu::Vec4(aChannelAccess.getChannel(tcu::IVec3(x, y, 0))), x, y);

		{
			const tcu::Vec4	scale	(1.0f);
			const tcu::Vec4	bias	(0.0f);

			log << tcu::TestLog::Image("SourceImageR", "SourceImageR", rImage.getAccess(), scale, bias);
			log << tcu::TestLog::Image("SourceImageG", "SourceImageG", gImage.getAccess(), scale, bias);
			log << tcu::TestLog::Image("SourceImageB", "SourceImageB", bImage.getAccess(), scale, bias);
			log << tcu::TestLog::Image("SourceImageA", "SourceImageA", aImage.getAccess(), scale, bias);
		}
	}
	else
	{
		tcu::TextureLevel	ycbcrSrcImage	(vk::mapVkFormat(config.format), size.x(), size.y());

		for (int y = 0; y < (int)size.y(); y++)
		for (int x = 0; x < (int)size.x(); x++)
		{
			const tcu::IVec3 pos (x, y, 0);
			ycbcrSrcImage.getAccess().setPixel(tcu::Vec4(rChannelAccess.getChannel(pos),
														 gChannelAccess.getChannel(pos),
														 bChannelAccess.getChannel(pos),
														 aChannelAccess.getChannel(pos)),
											   x, y);
		}

		log << tcu::TestLog::Image("SourceImage", "SourceImage", ycbcrSrcImage.getAccess());
	}
}

tcu::TestStatus conversionTest (Context& context, TestConfig config)
{
	std::vector<std::string>							requiredDevExt;
	requiredDevExt.push_back("VK_KHR_sampler_ycbcr_conversion");
	requiredDevExt.push_back("VK_KHR_get_memory_requirements2");
	requiredDevExt.push_back("VK_KHR_bind_memory2");
	requiredDevExt.push_back("VK_KHR_maintenance1");

	const tcu::UVec2									size					(ycbcr::isXChromaSubsampled(config.format) ? 12 : 7,
																				 ycbcr::isYChromaSubsampled(config.format) ?  8 : 13);

	ProtectedContext									ctx						(context, std::vector<std::string>(), requiredDevExt);
	const vk::DeviceInterface&							vk						= ctx.getDeviceInterface();
	const vk::VkDevice									device					= ctx.getDevice();
	const deUint32										queueFamilyIndex		= ctx.getQueueFamilyIndex();

	tcu::TestLog&										log						(context.getTestContext().getLog());

	validateFormatSupport(ctx, config);
	logTestCaseInfo(log, config);

	const vk::VkImageCreateFlagBits						ycbcrImageFlags			 = config.disjoint
																					? vk::VK_IMAGE_CREATE_DISJOINT_BIT
																					: (vk::VkImageCreateFlagBits)0u;
	const de::MovePtr<vk::YCbCrImageWithMemory>			ycbcrImage				(createYcbcrImage2D(ctx, PROTECTION_ENABLED,
																									size.x(), size.y(),
																									config.format,
																									ycbcrImageFlags,
																									vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT
																									 | vk::VK_IMAGE_USAGE_SAMPLED_BIT));
	const vk::Unique<vk::VkSamplerYcbcrConversion>		conversion				(createConversion(vk,
																								  device,
																								  config.format,
																								  config.colorModel,
																								  config.colorRange,
																								  config.xChromaOffset,
																								  config.yChromaOffset,
																								  config.chromaFilter,
																								  config.componentMapping,
																								  config.explicitReconstruction));
	const vk::Unique<vk::VkSampler>						ycbcrSampler			(createSampler(vk,
																							   device,
																							   config.textureFilter,
																							   config.addressModeU,
																							   config.addressModeV,
																							   *conversion));
	const vk::Unique<vk::VkImageView>					ycbcrImageView			(createImageView(vk, device, **ycbcrImage, config.format, *conversion));

	deUint32											combinedSamplerDescriptorCount = 1;
	{
		const vk::VkPhysicalDeviceImageFormatInfo2			imageFormatInfo				=
		{
			vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,	// sType
			DE_NULL,													// pNext
			config.format,												// format
			vk::VK_IMAGE_TYPE_2D,										// type
			vk::VK_IMAGE_TILING_OPTIMAL,								// tiling
			vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT |
			vk::VK_IMAGE_USAGE_SAMPLED_BIT,								// usage
			ycbcrImageFlags												// flags
		};

		vk::VkSamplerYcbcrConversionImageFormatProperties	samplerYcbcrConversionImage = {};
		samplerYcbcrConversionImage.sType = vk::VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_IMAGE_FORMAT_PROPERTIES;
		samplerYcbcrConversionImage.pNext = DE_NULL;

		vk::VkImageFormatProperties2						imageFormatProperties		= {};
		imageFormatProperties.sType = vk::VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
		imageFormatProperties.pNext = &samplerYcbcrConversionImage;

		VK_CHECK(context.getInstanceInterface().getPhysicalDeviceImageFormatProperties2(context.getPhysicalDevice(), &imageFormatInfo, &imageFormatProperties));
		combinedSamplerDescriptorCount = samplerYcbcrConversionImage.combinedImageSamplerDescriptorCount;
	}

	// Input attributes
	std::vector<tcu::Vec2>								texCoords;
	std::vector<tcu::Vec2>								posCoords;
	genTexCoords(texCoords, size);
	posCoords = computeVertexPositions((deUint32)texCoords.size(), size.cast<int>());

	// Input validation data
	std::vector<tcu::Vec4>								ycbcrMinBounds;
	std::vector<tcu::Vec4>								ycbcrMaxBounds;

	// Generate input ycbcr image and conversion reference
	{
		ycbcr::MultiPlaneImageData						ycbcrSrc				(config.format, size);

		generateYCbCrImage(ctx, config, size, texCoords, ycbcrSrc, ycbcrMinBounds, ycbcrMaxBounds);
		logBoundImages(log, size, ycbcrMinBounds, ycbcrMaxBounds);
		uploadYCbCrImage(ctx,
						 **ycbcrImage,
						 ycbcrSrc,
						 vk::VK_ACCESS_SHADER_READ_BIT,
						 vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}

	// Build up the reference data structure
	DE_ASSERT(posCoords.size() == ycbcrMinBounds.size());
	DE_ASSERT(posCoords.size() == ycbcrMaxBounds.size());
	DE_ASSERT(texCoords.size() >= CHECK_SIZE);
	std::vector<YCbCrValidationData>	referenceData;
	std::vector<YCbCrValidationData>	colorReferenceData;

	for (deUint32 ndx = 0; ndx < texCoords.size(); ++ndx)
	{
		YCbCrValidationData	data;
		data.coord		= texCoords[ndx].toWidth<4>();
		data.minBound	= ycbcrMinBounds[ndx];
		data.maxBound	= ycbcrMaxBounds[ndx];

		referenceData.push_back(data);

		YCbCrValidationData	colorData;
		colorData.coord		= posCoords[ndx].toWidth<4>();
		colorData.minBound	= tcu::Vec4(0.0f, 0.9f, 0.0f, 1.0f);
		colorData.maxBound	= tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f);

		colorReferenceData.push_back(colorData);
	}

	if (config.shaderType == glu::SHADERTYPE_VERTEX
		|| config.shaderType == glu::SHADERTYPE_FRAGMENT)
	{
		const de::UniquePtr<vk::ImageWithMemory>	colorImage			(createImage2D(ctx,
																				   PROTECTION_ENABLED,
																				   queueFamilyIndex,
																				   size.x(),
																				   size.y(),
																				   s_colorFormat,
																				   vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
																				    | vk::VK_IMAGE_USAGE_SAMPLED_BIT));
		const vk::Unique<vk::VkImageView>			colorImageView		(createImageView(ctx, **colorImage, s_colorFormat));
		const vk::Unique<vk::VkSampler>				colorSampler		(makeSampler(vk, device));

		renderYCbCrToColor(ctx, size, *ycbcrSampler, *ycbcrImageView, **colorImage, *colorImageView, referenceData, posCoords, combinedSamplerDescriptorCount);

		if (!validateImage(ctx, colorReferenceData, *colorSampler, *colorImageView, combinedSamplerDescriptorCount))
			return tcu::TestStatus::fail("YCbCr image conversion via fragment shader failed");
	}
	else if (config.shaderType == glu::SHADERTYPE_COMPUTE)
	{
		if (!validateImage(ctx, referenceData, *ycbcrSampler, *ycbcrImageView, combinedSamplerDescriptorCount))
			return tcu::TestStatus::fail("YCbCr image conversion via compute shader failed");
	}
	else
	{
		TCU_THROW(NotSupportedError, "Unsupported shader test type");
	}

	return tcu::TestStatus::pass("YCbCr image conversion was OK");
}

} // anonymous


tcu::TestCaseGroup*	createYCbCrConversionTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> testGroup (new tcu::TestCaseGroup(testCtx, "ycbcr", "YCbCr conversion tests"));

	struct {
		const char *			name;
		const glu::ShaderType	type;
	} shaderTypes[]	=
	{
		{ "fragment",	glu::SHADERTYPE_FRAGMENT	},
		{ "compute",	glu::SHADERTYPE_COMPUTE		}
	};

	struct RangeNamePair
	{
		const char*					name;
		vk::VkSamplerYcbcrRange		value;
	};
	struct ChromaLocationNamePair
	{
		const char*				name;
		vk::VkChromaLocation	value;
	};

	const vk::VkComponentMapping			identitySwizzle		=
	{
		vk::VK_COMPONENT_SWIZZLE_IDENTITY,
		vk::VK_COMPONENT_SWIZZLE_IDENTITY,
		vk::VK_COMPONENT_SWIZZLE_IDENTITY,
		vk::VK_COMPONENT_SWIZZLE_IDENTITY
	};

	const RangeNamePair						colorRanges[]		=
	{
		{ "itu_full",		vk::VK_SAMPLER_YCBCR_RANGE_ITU_FULL		},
		{ "itu_narrow",		vk::VK_SAMPLER_YCBCR_RANGE_ITU_NARROW	}
	};

	const ChromaLocationNamePair			chromaLocations[] =
	{
		{ "cosited",		vk::VK_CHROMA_LOCATION_COSITED_EVEN		},
		{ "midpoint",		vk::VK_CHROMA_LOCATION_MIDPOINT			}
	};

	const struct
	{
		const char* const							name;
		const vk::VkSamplerYcbcrModelConversion		value;
	}										colorModels[] =
	{
		{ "rgb_identity",	vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY		},
		{ "ycbcr_identity",	vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_IDENTITY	},
		{ "ycbcr_709",		vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_709			},
		{ "ycbcr_601",		vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_601			},
		{ "ycbcr_2020",		vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_2020		}
	};

	const struct
	{
		const char*			name;
		vk::VkImageTiling	value;
	}										imageTilings[] =
	{
		{ "tiling_linear",	vk::VK_IMAGE_TILING_LINEAR },
		{ "tiling_optimal",	vk::VK_IMAGE_TILING_OPTIMAL }
	};

	const deUint32					tilingNdx				= 1;
	const vk::VkImageTiling			tiling					= imageTilings[tilingNdx].value;
	const char*						tilingName				= imageTilings[tilingNdx].name;

	const vk::VkFormat testFormats[] =
	{
		// noChromaSubsampledFormats
		vk::VK_FORMAT_R4G4B4A4_UNORM_PACK16,
		vk::VK_FORMAT_B4G4R4A4_UNORM_PACK16,
		vk::VK_FORMAT_R5G6B5_UNORM_PACK16,
		vk::VK_FORMAT_B5G6R5_UNORM_PACK16,
		vk::VK_FORMAT_R5G5B5A1_UNORM_PACK16,
		vk::VK_FORMAT_B5G5R5A1_UNORM_PACK16,
		vk::VK_FORMAT_A1R5G5B5_UNORM_PACK16,
		vk::VK_FORMAT_R8G8B8_UNORM,
		vk::VK_FORMAT_B8G8R8_UNORM,
		vk::VK_FORMAT_R8G8B8A8_UNORM,
		vk::VK_FORMAT_B8G8R8A8_UNORM,
		vk::VK_FORMAT_A8B8G8R8_UNORM_PACK32,
		vk::VK_FORMAT_A2R10G10B10_UNORM_PACK32,
		vk::VK_FORMAT_A2B10G10R10_UNORM_PACK32,
		vk::VK_FORMAT_R16G16B16_UNORM,
		vk::VK_FORMAT_R16G16B16A16_UNORM,
		vk::VK_FORMAT_R10X6_UNORM_PACK16,
		vk::VK_FORMAT_R10X6G10X6_UNORM_2PACK16,
		vk::VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16,
		vk::VK_FORMAT_R12X4_UNORM_PACK16,
		vk::VK_FORMAT_R12X4G12X4_UNORM_2PACK16,
		vk::VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16,
		vk::VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM,
		vk::VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16,
		vk::VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16,
		vk::VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM,

		// xChromaSubsampledFormats
		vk::VK_FORMAT_G8B8G8R8_422_UNORM,
		vk::VK_FORMAT_B8G8R8G8_422_UNORM,
		vk::VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM,
		vk::VK_FORMAT_G8_B8R8_2PLANE_422_UNORM,

		vk::VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16,
		vk::VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16,
		vk::VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16,
		vk::VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16,
		vk::VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16,
		vk::VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16,
		vk::VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16,
		vk::VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16,
		vk::VK_FORMAT_G16B16G16R16_422_UNORM,
		vk::VK_FORMAT_B16G16R16G16_422_UNORM,
		vk::VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM,
		vk::VK_FORMAT_G16_B16R16_2PLANE_422_UNORM,

		// xyChromaSubsampledFormats
		vk::VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM,
		vk::VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
		vk::VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16,
		vk::VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16,
		vk::VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16,
		vk::VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16,
		vk::VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM,
		vk::VK_FORMAT_G16_B16R16_2PLANE_420_UNORM,

		// Extended YCbCr formats
		vk::VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT,
		vk::VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT,
		vk::VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT,
		vk::VK_FORMAT_G16_B16R16_2PLANE_444_UNORM_EXT,
	};

	for (size_t formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(testFormats); formatNdx++)
	{
		const vk::VkFormat				format		(testFormats[formatNdx]);
		const std::string				formatName	(de::toLower(std::string(getFormatName(format)).substr(10)));
		de::MovePtr<tcu::TestCaseGroup>	formatGroup	(new tcu::TestCaseGroup(testCtx, formatName.c_str(), ("Tests for color conversion using format " + formatName).c_str()));

		for (size_t shaderNdx = 0; shaderNdx < DE_LENGTH_OF_ARRAY(shaderTypes); shaderNdx++)
		{
			const char*						shaderTypeName	= shaderTypes[shaderNdx].name;
			de::MovePtr<tcu::TestCaseGroup>	shaderGroup (new tcu::TestCaseGroup(testCtx, shaderTypeName, "YCbCr conversion tests"));

			for (size_t modelNdx = 0; modelNdx < DE_LENGTH_OF_ARRAY(colorModels); modelNdx++)
			{
				const char* const							colorModelName	(colorModels[modelNdx].name);
				const vk::VkSamplerYcbcrModelConversion		colorModel		(colorModels[modelNdx].value);

				if (colorModel != vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY && ycbcr::getYCbCrFormatChannelCount(format) < 3)
					continue;

				de::MovePtr<tcu::TestCaseGroup> colorModelGroup (new tcu::TestCaseGroup(testCtx, colorModelName, "YCbCr conversion tests"));

				for (size_t rangeNdx = 0; rangeNdx < DE_LENGTH_OF_ARRAY(colorRanges); rangeNdx++)
				{
					const char* const					colorRangeName	(colorRanges[rangeNdx].name);
					const vk::VkSamplerYcbcrRange		colorRange		(colorRanges[rangeNdx].value);

					// Narrow range doesn't really work with formats that have less than 8 bits
					if (colorRange == vk::VK_SAMPLER_YCBCR_RANGE_ITU_NARROW)
					{
						const tcu::UVec4 bitDepth	(ycbcr::getYCbCrBitDepth(format));
						if (bitDepth[0] < 8 || bitDepth[1] < 8 || bitDepth[2] < 8)
							continue;
					}

					de::MovePtr<tcu::TestCaseGroup>		colorRangeGroup	(new tcu::TestCaseGroup(testCtx, colorRangeName, ("Tests for color range " + std::string(colorRangeName)).c_str()));

					for (size_t chromaOffsetNdx = 0; chromaOffsetNdx < DE_LENGTH_OF_ARRAY(chromaLocations); chromaOffsetNdx++)
					{
						const char* const				chromaOffsetName	(chromaLocations[chromaOffsetNdx].name);
						const vk::VkChromaLocation		chromaOffset		(chromaLocations[chromaOffsetNdx].value);


						for (deUint32 disjointNdx = 0; disjointNdx < 2; ++disjointNdx)
						{
							bool				disjoint	= (disjointNdx == 1);
							const TestConfig	config	(shaderTypes[shaderNdx].type,
														 format,
														 tiling,
														 vk::VK_FILTER_NEAREST,
														 vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
														 vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
														 vk::VK_FILTER_NEAREST,
														 chromaOffset,
														 chromaOffset,
														 false,
														 disjoint,
														 colorRange,
														 colorModel,
														 identitySwizzle);

							addFunctionCaseWithPrograms(colorRangeGroup.get(),
														std::string(tilingName) + "_" + chromaOffsetName + (disjoint ? "_disjoint" : ""),
														"",
														checkSupport,
														testShaders,
														conversionTest,
														config);
						}
					}

					colorModelGroup->addChild(colorRangeGroup.release());
				}

				shaderGroup->addChild(colorModelGroup.release());
			}

			formatGroup->addChild(shaderGroup.release());

		}
		testGroup->addChild(formatGroup.release());
	}

	return testGroup.release();
}

} // ProtectedMem
} // vkt
