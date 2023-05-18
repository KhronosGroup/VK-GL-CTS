/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 The Khronos Group Inc.
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
 * \brief YCbCr filtering tests.
 *//*--------------------------------------------------------------------*/

#include "tcuVectorUtil.hpp"
#include "tcuTexVerifierUtil.hpp"
#include "tcuImageCompare.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkRefUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktYCbCrFilteringTests.hpp"
#include "vktDrawUtil.hpp"
#include "vktYCbCrUtil.hpp"
#include "gluTextureTestUtil.hpp"
#include <string>
#include <vector>

using namespace vk;
using namespace vkt::drawutil;

namespace vkt
{
namespace ycbcr
{
namespace
{

using std::vector;
using std::string;
using tcu::TestLog;
using tcu::Sampler;
using namespace glu::TextureTestUtil;

class LinearFilteringTestInstance: public TestInstance
{
public:
	LinearFilteringTestInstance(Context& context, VkFormat format, VkFilter chromaFiltering);
	~LinearFilteringTestInstance() = default;

protected:

	VkSamplerCreateInfo				getSamplerInfo				(const VkSamplerYcbcrConversionInfo*	samplerConversionInfo);
	Move<VkDescriptorSetLayout>		createDescriptorSetLayout	(VkSampler sampler);
	Move<VkDescriptorPool>			createDescriptorPool		(const deUint32 combinedSamplerDescriptorCount);
	Move<VkDescriptorSet>			createDescriptorSet			(VkDescriptorPool		descPool,
																 VkDescriptorSetLayout	descLayout);
	Move<VkSamplerYcbcrConversion>	createYCbCrConversion		(void);
	Move<VkImage>					createImage					(deUint32 width, deUint32 height);
	Move<VkImageView>				createImageView				(const VkSamplerYcbcrConversionInfo& samplerConversionInfo, VkImage image);
	void							bindImage					(VkDescriptorSet		descriptorSet,
																 VkImageView			imageView,
																 VkSampler				sampler);
	tcu::TestStatus					iterate						(void);

private:

	struct FilterCase
	{
		const tcu::UVec2 imageSize;
		const tcu::UVec2 renderSize;
	};

	const VkFormat				m_format;
	const VkFilter				m_chromaFiltering;
	const DeviceInterface&		m_vkd;
	const VkDevice				m_device;
	int							m_caseIndex;
	const vector<FilterCase>	m_cases;
};

LinearFilteringTestInstance::LinearFilteringTestInstance(Context& context, VkFormat format, VkFilter chromaFiltering)
	: TestInstance		(context)
	, m_format			(format)
	, m_chromaFiltering	(chromaFiltering)
	, m_vkd				(m_context.getDeviceInterface())
	, m_device			(m_context.getDevice())
	, m_caseIndex		(0)
	, m_cases			{
		{ { 8,  8}, {64, 64} },
		{ {64, 32}, {32, 64} }
	}
{
}

VkSamplerCreateInfo LinearFilteringTestInstance::getSamplerInfo(const VkSamplerYcbcrConversionInfo* samplerConversionInfo)
{
	return
	{
		VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		samplerConversionInfo,
		0u,
		VK_FILTER_LINEAR,							// magFilter
		VK_FILTER_LINEAR,							// minFilter
		VK_SAMPLER_MIPMAP_MODE_NEAREST,				// mipmapMode
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		// addressModeU
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		// addressModeV
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		// addressModeW
		0.0f,										// mipLodBias
		VK_FALSE,									// anisotropyEnable
		1.0f,										// maxAnisotropy
		VK_FALSE,									// compareEnable
		VK_COMPARE_OP_ALWAYS,						// compareOp
		0.0f,										// minLod
		0.0f,										// maxLod
		VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,	// borderColor
		VK_FALSE,									// unnormalizedCoords
	};
}

Move<VkDescriptorSetLayout> LinearFilteringTestInstance::createDescriptorSetLayout(VkSampler sampler)
{
	const VkDescriptorSetLayoutBinding binding =
	{
		0u,												// binding
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		1u,												// descriptorCount
		VK_SHADER_STAGE_ALL,
		&sampler
	};
	const VkDescriptorSetLayoutCreateInfo layoutInfo =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		DE_NULL,
		(VkDescriptorSetLayoutCreateFlags)0u,
		1u,
		&binding,
	};

	return ::createDescriptorSetLayout(m_vkd, m_device, &layoutInfo);
}

Move<VkDescriptorPool> LinearFilteringTestInstance::createDescriptorPool(const deUint32 combinedSamplerDescriptorCount)
{
	const VkDescriptorPoolSize poolSizes[] =
	{
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,	combinedSamplerDescriptorCount	},
	};
	const VkDescriptorPoolCreateInfo poolInfo =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		DE_NULL,
		(VkDescriptorPoolCreateFlags)VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
		1u,		// maxSets
		DE_LENGTH_OF_ARRAY(poolSizes),
		poolSizes,
	};

	return ::createDescriptorPool(m_vkd, m_device, &poolInfo);
}

Move<VkDescriptorSet> LinearFilteringTestInstance::createDescriptorSet(VkDescriptorPool			descPool,
																	   VkDescriptorSetLayout	descLayout)
{
	const VkDescriptorSetAllocateInfo allocInfo =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		DE_NULL,
		descPool,
		1u,
		&descLayout,
	};

	return allocateDescriptorSet(m_vkd, m_device, &allocInfo);
}

Move<VkSamplerYcbcrConversion> LinearFilteringTestInstance::createYCbCrConversion()
{
	const VkSamplerYcbcrConversionCreateInfo conversionInfo =
	{
		VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO,
		DE_NULL,
		m_format,
		VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY,
		VK_SAMPLER_YCBCR_RANGE_ITU_FULL,
		{
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
		},
		VK_CHROMA_LOCATION_MIDPOINT,
		VK_CHROMA_LOCATION_MIDPOINT,
		m_chromaFiltering,							// chromaFilter
		VK_FALSE,									// forceExplicitReconstruction
	};

	return createSamplerYcbcrConversion(m_vkd, m_device, &conversionInfo);
}

Move<VkImage> LinearFilteringTestInstance::createImage(deUint32 width, deUint32 height)
{
	VkImageCreateFlags			createFlags = 0u;
	const VkImageCreateInfo		createInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		DE_NULL,
		createFlags,
		VK_IMAGE_TYPE_2D,
		m_format,
		makeExtent3D(width, height, 1u),
		1u,		// mipLevels
		1u,		// arrayLayers
		VK_SAMPLE_COUNT_1_BIT,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		0u,
		(const deUint32*)DE_NULL,
		VK_IMAGE_LAYOUT_UNDEFINED,
	};

	return ::createImage(m_vkd, m_device, &createInfo);
}

Move<VkImageView> LinearFilteringTestInstance::createImageView(const VkSamplerYcbcrConversionInfo& samplerConversionInfo, VkImage image)
{
	const VkImageViewCreateInfo	viewInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		&samplerConversionInfo,
		(VkImageViewCreateFlags)0,
		image,
		VK_IMAGE_VIEW_TYPE_2D,
		m_format,
		{
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
		},
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u },
	};

	return ::createImageView(m_vkd, m_device, &viewInfo);
}

void LinearFilteringTestInstance::bindImage(VkDescriptorSet	descriptorSet,
											VkImageView		imageView,
											VkSampler		sampler)
{
	const VkDescriptorImageInfo imageInfo =
	{
		sampler,
		imageView,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	};
	const VkWriteDescriptorSet descriptorWrite =
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		DE_NULL,
		descriptorSet,
		0u,		// dstBinding
		0u,		// dstArrayElement
		1u,		// descriptorCount
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		&imageInfo,
		(const VkDescriptorBufferInfo*)DE_NULL,
		(const VkBufferView*)DE_NULL,
	};

	m_vkd.updateDescriptorSets(m_device, 1u, &descriptorWrite, 0u, DE_NULL);
}

tcu::TestStatus LinearFilteringTestInstance::iterate(void)
{
	const tcu::UVec2						imageSize			(m_cases[m_caseIndex].imageSize);
	const tcu::UVec2						renderSize			(m_cases[m_caseIndex].renderSize);
	const auto&								instInt				(m_context.getInstanceInterface());
	auto									physicalDevice		(m_context.getPhysicalDevice());
	const Unique<VkSamplerYcbcrConversion>	conversion			(createYCbCrConversion());
	const VkSamplerYcbcrConversionInfo		samplerConvInfo		{ VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO, DE_NULL, *conversion };
	const VkSamplerCreateInfo				samplerCreateInfo	(getSamplerInfo(&samplerConvInfo));
	const Unique<VkSampler>					sampler				(createSampler(m_vkd, m_device, &samplerCreateInfo));

	deUint32								combinedSamplerDescriptorCount = 1;
	{
		const VkPhysicalDeviceImageFormatInfo2			imageFormatInfo				=
		{
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,	// sType
			DE_NULL,												// pNext
			m_format,												// format
			VK_IMAGE_TYPE_2D,										// type
			VK_IMAGE_TILING_OPTIMAL,								// tiling
			VK_IMAGE_USAGE_TRANSFER_DST_BIT |
			VK_IMAGE_USAGE_SAMPLED_BIT,								// usage
			(VkImageCreateFlags)0u									// flags
		};

		VkSamplerYcbcrConversionImageFormatProperties	samplerYcbcrConversionImage = {};
		samplerYcbcrConversionImage.sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_IMAGE_FORMAT_PROPERTIES;
		samplerYcbcrConversionImage.pNext = DE_NULL;

		VkImageFormatProperties2						imageFormatProperties		= {};
		imageFormatProperties.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
		imageFormatProperties.pNext = &samplerYcbcrConversionImage;

		VK_CHECK(instInt.getPhysicalDeviceImageFormatProperties2(physicalDevice, &imageFormatInfo, &imageFormatProperties));
		combinedSamplerDescriptorCount = samplerYcbcrConversionImage.combinedImageSamplerDescriptorCount;
	}

	const Unique<VkDescriptorSetLayout>		descLayout			(createDescriptorSetLayout(*sampler));
	const Unique<VkDescriptorPool>			descPool			(createDescriptorPool(combinedSamplerDescriptorCount));
	const Unique<VkDescriptorSet>			descSet				(createDescriptorSet(*descPool, *descLayout));
	const Unique<VkImage>					testImage			(createImage(imageSize.x(), imageSize.y()));
	const vector<AllocationSp>				allocations			(allocateAndBindImageMemory(m_vkd, m_device, m_context.getDefaultAllocator(), *testImage, m_format, 0u));
	const Unique<VkImageView>				imageView			(createImageView(samplerConvInfo, *testImage));

	// create and bind image with test data
	MultiPlaneImageData imageData(m_format, imageSize);
	fillGradient(&imageData, tcu::Vec4(0.0f), tcu::Vec4(1.0f));
	uploadImage(m_vkd,
				m_device,
				m_context.getUniversalQueueFamilyIndex(),
				m_context.getDefaultAllocator(),
				*testImage,
				imageData,
				(VkAccessFlags)VK_ACCESS_SHADER_READ_BIT,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				0);
	bindImage(*descSet, *imageView, *sampler);

	const vector<tcu::Vec4> vertices =
	{
		{ -1.0f, -1.0f, 0.0f, 1.0f },
		{ +1.0f, -1.0f, 0.0f, 1.0f },
		{ -1.0f, +1.0f, 0.0f, 1.0f },
		{ +1.0f, +1.0f, 0.0f, 1.0f }
	};
	VulkanProgram program({
		VulkanShader(VK_SHADER_STAGE_VERTEX_BIT,	m_context.getBinaryCollection().get("vert")),
		VulkanShader(VK_SHADER_STAGE_FRAGMENT_BIT,	m_context.getBinaryCollection().get("frag"))
	});
	program.descriptorSet		= *descSet;
	program.descriptorSetLayout = *descLayout;

	PipelineState		pipelineState		(m_context.getDeviceProperties().limits.subPixelPrecisionBits);
	const DrawCallData	drawCallData		(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, vertices);
	FrameBufferState	frameBufferState	(renderSize.x(), renderSize.y());
	VulkanDrawContext	renderer			(m_context, frameBufferState);

	// render full screen quad
	renderer.registerDrawObject(pipelineState, program, drawCallData);
	renderer.draw();

	// get rendered image
	tcu::ConstPixelBufferAccess resImage(renderer.getColorPixels());

	// construct ChannelAccess objects required to create reference results
	const vk::PlanarFormatDescription	planeInfo				= imageData.getDescription();
	deUint32							nullAccessData			(0u);
	ChannelAccess						nullAccess				(tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT, 1u, tcu::IVec3(imageSize.x(), imageSize.y(), 1), tcu::IVec3(0, 0, 0), &nullAccessData, 0u);
	deUint32							nullAccessAlphaData		(~0u);
	ChannelAccess						nullAccessAlpha			(tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT, 1u, tcu::IVec3(imageSize.x(), imageSize.y(), 1), tcu::IVec3(0, 0, 0), &nullAccessAlphaData, 0u);
	ChannelAccess						rChannelAccess			(planeInfo.hasChannelNdx(0) ? getChannelAccess(imageData, planeInfo, imageSize, 0) : nullAccess);
	ChannelAccess						gChannelAccess			(planeInfo.hasChannelNdx(1) ? getChannelAccess(imageData, planeInfo, imageSize, 1) : nullAccess);
	ChannelAccess						bChannelAccess			(planeInfo.hasChannelNdx(2) ? getChannelAccess(imageData, planeInfo, imageSize, 2) : nullAccess);
	ChannelAccess						aChannelAccess			(planeInfo.hasChannelNdx(3) ? getChannelAccess(imageData, planeInfo, imageSize, 3) : nullAccessAlpha);
	const VkFormatProperties			formatProperties		(getPhysicalDeviceFormatProperties(instInt, physicalDevice, m_format));
	const VkFormatFeatureFlags			featureFlags			(formatProperties.optimalTilingFeatures);
	const bool							explicitReconstruction	(featureFlags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_BIT);

	// calulate texture coordinates used by fragment shader
	vector<tcu::Vec2>					sts;
	for (deUint32 y = 0; y < renderSize.y(); y++)
	for (deUint32 x = 0; x < renderSize.x(); x++)
	{
		const float s = ((float)x + 0.5f) / (float)renderSize.x();
		const float t = ((float)y + 0.5f) / (float)renderSize.y();

		sts.push_back(tcu::Vec2(s, t));
	}

	// calculate minimum and maximum values between which the results should be placed
	const tcu::UVec4					bitDepth				(getYCbCrBitDepth(m_format));
	const std::vector<tcu::FloatFormat>	filteringPrecision		(getPrecision(m_format));
	const std::vector<tcu::FloatFormat>	conversionPrecision		(getPrecision(m_format));
	const deUint32						subTexelPrecisionBits	(vk::getPhysicalDeviceProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()).limits.subTexelPrecisionBits);
	const vk::VkComponentMapping		componentMapping		= { vk::VK_COMPONENT_SWIZZLE_IDENTITY, vk::VK_COMPONENT_SWIZZLE_IDENTITY, vk::VK_COMPONENT_SWIZZLE_IDENTITY, vk::VK_COMPONENT_SWIZZLE_IDENTITY };

	std::vector<tcu::Vec4>				minBound;
	std::vector<tcu::Vec4>				maxBound;
	std::vector<tcu::Vec4>				uvBound;
	std::vector<tcu::IVec4>				ijBound;
	calculateBounds(rChannelAccess, gChannelAccess, bChannelAccess, aChannelAccess, bitDepth, sts, filteringPrecision, conversionPrecision, subTexelPrecisionBits, VK_FILTER_LINEAR, VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY, VK_SAMPLER_YCBCR_RANGE_ITU_FULL, m_chromaFiltering, VK_CHROMA_LOCATION_MIDPOINT, VK_CHROMA_LOCATION_MIDPOINT, componentMapping, explicitReconstruction, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, minBound, maxBound, uvBound, ijBound);

	// log result and reference images
	TestLog&							log						(m_context.getTestContext().getLog());
	{
		const tcu::Vec4					scale					(1.0f);
		const tcu::Vec4					bias					(0.0f);
		vector<deUint8>					minData					(renderSize.x() * renderSize.y() * sizeof(tcu::Vec4), 255);
		vector<deUint8>					maxData					(renderSize.x() * renderSize.y() * sizeof(tcu::Vec4), 255);
		tcu::TextureFormat				refFormat				(vk::mapVkFormat(frameBufferState.colorFormat));
		tcu::PixelBufferAccess			minImage				(refFormat, renderSize.x(), renderSize.y(), 1, minData.data());
		tcu::PixelBufferAccess			maxImage				(refFormat, renderSize.x(), renderSize.y(), 1, maxData.data());
		{
			deUint32					ndx						= 0;
			for (deUint32 y = 0; y < renderSize.y(); y++)
			for (deUint32 x = 0; x < renderSize.x(); x++)
			{
				minImage.setPixel(minBound[ndx], x, y);
				maxImage.setPixel(maxBound[ndx], x, y);
				ndx++;
			}
		}

		log << TestLog::Image("MinBoundImage", "MinBoundImage", minImage, scale, bias);
		log << TestLog::Image("MaxBoundImage", "MaxBoundImage", maxImage, scale, bias);
		log << TestLog::Image("ResImage", "ResImage", resImage, scale, bias);
	}

	bool								isOk					= true;
	{
		deUint32						ndx						= 0;
		VkFilter						textureFilter			= VK_FILTER_LINEAR;
		size_t							errorCount				= 0;

		for (deUint32 y = 0; y < renderSize.y(); y++)
		for (deUint32 x = 0; x < renderSize.x(); x++)
		{
			tcu::Vec4 resValue = resImage.getPixel(x, y);
			bool fail = tcu::boolAny(tcu::lessThan(resValue, minBound[ndx])) || tcu::boolAny(tcu::greaterThan(resValue, maxBound[ndx]));

			if (fail)
			{
				log << TestLog::Message << "Fail: " << sts[ndx] << " " << resValue << TestLog::EndMessage;
				log << TestLog::Message << "  Min : " << minBound[ndx] << TestLog::EndMessage;
				log << TestLog::Message << "  Max : " << maxBound[ndx] << TestLog::EndMessage;
				log << TestLog::Message << "  Threshold: " << (maxBound[ndx] - minBound[ndx]) << TestLog::EndMessage;
				log << TestLog::Message << "  UMin : " << uvBound[ndx][0] << TestLog::EndMessage;
				log << TestLog::Message << "  UMax : " << uvBound[ndx][1] << TestLog::EndMessage;
				log << TestLog::Message << "  VMin : " << uvBound[ndx][2] << TestLog::EndMessage;
				log << TestLog::Message << "  VMax : " << uvBound[ndx][3] << TestLog::EndMessage;
				log << TestLog::Message << "  IMin : " << ijBound[ndx][0] << TestLog::EndMessage;
				log << TestLog::Message << "  IMax : " << ijBound[ndx][1] << TestLog::EndMessage;
				log << TestLog::Message << "  JMin : " << ijBound[ndx][2] << TestLog::EndMessage;
				log << TestLog::Message << "  JMax : " << ijBound[ndx][3] << TestLog::EndMessage;

				if (isXChromaSubsampled(m_format))
				{
					log << TestLog::Message << "  LumaAlphaValues : " << TestLog::EndMessage;
					log << TestLog::Message << "    Offset : (" << ijBound[ndx][0] << ", " << ijBound[ndx][2] << ")" << TestLog::EndMessage;

					for (deInt32 k = ijBound[ndx][2]; k <= ijBound[ndx][3] + (textureFilter == vk::VK_FILTER_LINEAR ? 1 : 0); k++)
					{
						const deInt32		wrappedK = wrap(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, k, gChannelAccess.getSize().y());
						bool				first = true;
						std::ostringstream	line;

						for (deInt32 j = ijBound[ndx][0]; j <= ijBound[ndx][1] + (textureFilter == vk::VK_FILTER_LINEAR ? 1 : 0); j++)
						{
							const deInt32	wrappedJ = wrap(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, j, gChannelAccess.getSize().x());

							if (!first)
							{
								line << ", ";
								first = false;
							}

							line << "(" << std::setfill(' ') << std::setw(5) << gChannelAccess.getChannelUint(tcu::IVec3(wrappedJ, wrappedK, 0))
								<< ", " << std::setfill(' ') << std::setw(5) << aChannelAccess.getChannelUint(tcu::IVec3(wrappedJ, wrappedK, 0)) << ")";
						}
						log << TestLog::Message << "    " << line.str() << TestLog::EndMessage;
					}

					{
						const tcu::IVec2 chromaJRange(divFloor(ijBound[ndx][0], 2) - 1, divFloor(ijBound[ndx][1] + (textureFilter == vk::VK_FILTER_LINEAR ? 1 : 0), 2) + 1);
						const tcu::IVec2 chromaKRange(isYChromaSubsampled(m_format)
							? tcu::IVec2(divFloor(ijBound[ndx][2], 2) - 1, divFloor(ijBound[ndx][3] + (textureFilter == vk::VK_FILTER_LINEAR ? 1 : 0), 2) + 1)
							: tcu::IVec2(ijBound[ndx][2], ijBound[ndx][3] + (textureFilter == vk::VK_FILTER_LINEAR ? 1 : 0)));

						log << TestLog::Message << "  ChromaValues : " << TestLog::EndMessage;
						log << TestLog::Message << "    Offset : (" << chromaJRange[0] << ", " << chromaKRange[0] << ")" << TestLog::EndMessage;

						for (deInt32 k = chromaKRange[0]; k <= chromaKRange[1]; k++)
						{
							const deInt32		wrappedK = wrap(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, k, rChannelAccess.getSize().y());
							bool				first = true;
							std::ostringstream	line;

							for (deInt32 j = chromaJRange[0]; j <= chromaJRange[1]; j++)
							{
								const deInt32	wrappedJ = wrap(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, j, rChannelAccess.getSize().x());

								if (!first)
								{
									line << ", ";
									first = false;
								}

								line << "(" << std::setfill(' ') << std::setw(5) << rChannelAccess.getChannelUint(tcu::IVec3(wrappedJ, wrappedK, 0))
									<< ", " << std::setfill(' ') << std::setw(5) << bChannelAccess.getChannelUint(tcu::IVec3(wrappedJ, wrappedK, 0)) << ")";
							}
							log << TestLog::Message << "    " << line.str() << TestLog::EndMessage;
						}
					}
				}
				else
				{
					log << TestLog::Message << "  Values : " << TestLog::EndMessage;
					log << TestLog::Message << "    Offset : (" << ijBound[ndx][0] << ", " << ijBound[ndx][2] << ")" << TestLog::EndMessage;

					for (deInt32 k = ijBound[ndx][2]; k <= ijBound[ndx][3] + (textureFilter == vk::VK_FILTER_LINEAR ? 1 : 0); k++)
					{
						const deInt32		wrappedK = wrap(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, k, rChannelAccess.getSize().y());
						bool				first = true;
						std::ostringstream	line;

						for (deInt32 j = ijBound[ndx][0]; j <= ijBound[ndx][1] + (textureFilter == vk::VK_FILTER_LINEAR ? 1 : 0); j++)
						{
							const deInt32	wrappedJ = wrap(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, j, rChannelAccess.getSize().x());

							if (!first)
							{
								line << ", ";
								first = false;
							}

							line << "(" << std::setfill(' ') << std::setw(5) << rChannelAccess.getChannelUint(tcu::IVec3(wrappedJ, wrappedK, 0))
								<< ", " << std::setfill(' ') << std::setw(5) << gChannelAccess.getChannelUint(tcu::IVec3(wrappedJ, wrappedK, 0))
								<< ", " << std::setfill(' ') << std::setw(5) << bChannelAccess.getChannelUint(tcu::IVec3(wrappedJ, wrappedK, 0))
								<< ", " << std::setfill(' ') << std::setw(5) << aChannelAccess.getChannelUint(tcu::IVec3(wrappedJ, wrappedK, 0)) << ")";
						}
						log << TestLog::Message << "    " << line.str() << TestLog::EndMessage;
					}
				}

				errorCount++;
				isOk = false;

				if (errorCount > 30)
				{
					log << TestLog::Message << "Encountered " << errorCount << " errors. Omitting rest of the per result logs." << TestLog::EndMessage;
					break;
				}
			}
			ndx++;
		}
	}

	if (++m_caseIndex < (int)m_cases.size())
		return tcu::TestStatus::incomplete();
	if (!isOk)
		return tcu::TestStatus::fail("Result comparison failed");
	return tcu::TestStatus::pass("Pass");
}

class LinearFilteringTestCase : public vkt::TestCase
{
public:
	LinearFilteringTestCase(tcu::TestContext &context, const char* name, const char* description, VkFormat format, VkFilter chromaFiltering);

protected:
	void				checkSupport(Context& context) const;
	vkt::TestInstance*	createInstance(vkt::Context& context) const;
	void				initPrograms(SourceCollections& programCollection) const;

private:
	VkFormat			m_format;
	VkFilter			m_chromaFiltering;
};

LinearFilteringTestCase::LinearFilteringTestCase(tcu::TestContext &context, const char* name, const char* description, VkFormat format, VkFilter chromaFiltering)
	: TestCase(context, name, description)
	, m_format(format)
	, m_chromaFiltering(chromaFiltering)
{
}

void LinearFilteringTestCase::checkSupport(Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_sampler_ycbcr_conversion");

	const vk::VkPhysicalDeviceSamplerYcbcrConversionFeatures	features = context.getSamplerYcbcrConversionFeatures();
	if (features.samplerYcbcrConversion == VK_FALSE)
		TCU_THROW(NotSupportedError, "samplerYcbcrConversion feature is not supported");

	const auto&					instInt				= context.getInstanceInterface();
	auto						physicalDevice		= context.getPhysicalDevice();
	const VkFormatProperties	formatProperties	= getPhysicalDeviceFormatProperties(instInt, physicalDevice, m_format);
	const VkFormatFeatureFlags	featureFlags		= formatProperties.optimalTilingFeatures;

	if ((featureFlags & VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT) == 0)
		TCU_THROW(NotSupportedError, "YCbCr conversion is not supported for format");

	if ((featureFlags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) == 0)
		TCU_THROW(NotSupportedError, "Linear filtering not supported for format");

	if (m_chromaFiltering != VK_FILTER_LINEAR &&
		(featureFlags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_SEPARATE_RECONSTRUCTION_FILTER_BIT) == 0)
		TCU_THROW(NotSupportedError, "Different chroma, min, and mag filters not supported for format");

	if (m_chromaFiltering == VK_FILTER_LINEAR &&
		(featureFlags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT) == 0)
		TCU_THROW(NotSupportedError, "Linear chroma filtering not supported for format");
}

vkt::TestInstance* LinearFilteringTestCase::createInstance(vkt::Context& context) const
{
	return new LinearFilteringTestInstance(context, m_format, m_chromaFiltering);
}

void LinearFilteringTestCase::initPrograms(SourceCollections& programCollection) const
{
	static const char* vertShader =
		"#version 450\n"
		"precision mediump int; precision highp float;\n"
		"layout(location = 0) in vec4 a_position;\n"
		"layout(location = 0) out vec2 v_texCoord;\n"
		"out gl_PerVertex { vec4 gl_Position; };\n"
		"\n"
		"void main (void)\n"
		"{\n"
		"  v_texCoord = a_position.xy * 0.5 + 0.5;\n"
		"  gl_Position = a_position;\n"
		"}\n";

	static const char* fragShader =
		"#version 450\n"
		"precision mediump int; precision highp float;\n"
		"layout(location = 0) in vec2 v_texCoord;\n"
		"layout(location = 0) out mediump vec4 dEQP_FragColor;\n"
		"layout (set=0, binding=0) uniform sampler2D u_sampler;\n"
		"void main (void)\n"
		"{\n"
		"  dEQP_FragColor = vec4(texture(u_sampler, v_texCoord));\n"
		"}\n";

	programCollection.glslSources.add("vert") << glu::VertexSource(vertShader);
	programCollection.glslSources.add("frag") << glu::FragmentSource(fragShader);
}

} // anonymous

tcu::TestCaseGroup* createFilteringTests (tcu::TestContext& testCtx)
{
	struct YCbCrFormatData
	{
		const char* const	name;
		const VkFormat		format;
	};

	static const std::vector<YCbCrFormatData> ycbcrFormats =
	{
		{ "g8_b8_r8_3plane_420_unorm",	VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM		},
		{ "g8_b8r8_2plane_420_unorm",	VK_FORMAT_G8_B8R8_2PLANE_420_UNORM		},
	};

	de::MovePtr<tcu::TestCaseGroup> filteringTests(new tcu::TestCaseGroup(testCtx, "filtering",	"YCbCr filtering tests"));

	for (const auto& ycbcrFormat : ycbcrFormats)
	{
		{
			const std::string name = std::string("linear_sampler_") + ycbcrFormat.name;
			filteringTests->addChild(new LinearFilteringTestCase(filteringTests->getTestContext(), name.c_str(), "", ycbcrFormat.format, VK_FILTER_NEAREST));
		}

		{
			const std::string name = std::string("linear_sampler_with_chroma_linear_filtering_") + ycbcrFormat.name;
			filteringTests->addChild(new LinearFilteringTestCase(filteringTests->getTestContext(), name.c_str(), "", ycbcrFormat.format, VK_FILTER_LINEAR));
		}
	}

	return filteringTests.release();
}

} // ycbcr

} // vkt
