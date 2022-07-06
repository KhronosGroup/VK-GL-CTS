/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 The Khronos Group Inc.
 * Copyright (c) 2020 Google Inc.
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
 * \brief Texture conversion tests.
 *//*--------------------------------------------------------------------*/

#include "vktTextureConversionTests.hpp"
#include "vktAmberTestCase.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTextureTestUtil.hpp"
#include "vkImageUtil.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVectorUtil.hpp"
#include "deSharedPtr.hpp"

#include <cmath>
#include <memory>

namespace vkt
{
namespace texture
{

using namespace vk;

namespace
{

using namespace texture::util;
using namespace glu::TextureTestUtil;

class SnormLinearClampInstance : public TestInstance
{
public:
	struct Params
	{
		VkFormat	format;
		int			width;
		int			height;
	};
							SnormLinearClampInstance	(vkt::Context&					context,
														 de::SharedPtr<Params>			params);

	virtual tcu::TestStatus	iterate						(void) override;

protected:
	tcu::IVec4				computeColorDistance		() const;
	bool					verifyPixels				(const tcu::PixelBufferAccess&	rendered,
														 const tcu::PixelBufferAccess&	reference,
														 const ReferenceParams&			samplerParams,
														 const std::vector<float>&		texCoords)		const;

	static int				lim							(const tcu::TextureFormat&		format,
														 int							channelIdx);

private:
	const de::SharedPtr<Params>	m_params;
	const tcu::TextureFormat	m_inFormat;
	const VkFormat				m_outFormat;
	TestTexture2DSp				m_hwTexture;
	tcu::Texture2D				m_swTexture;
	TextureRenderer				m_renderer;

	const tcu::IVec4			m_cd;
	const tcu::IVec4			m_a;
	const tcu::IVec4			m_b;
	const tcu::IVec4			m_c;
	const tcu::IVec4			m_d;

public:
	static const int			textureWidth	= 7;
	static const int			textureHeight	= 7;
};

SnormLinearClampInstance::SnormLinearClampInstance (vkt::Context& context, de::SharedPtr<Params> params)
	: TestInstance	(context)
	, m_params		(params)
	, m_inFormat	(mapVkFormat(m_params->format))
	, m_outFormat	(VK_FORMAT_R32G32B32A32_SFLOAT)
	, m_hwTexture	(TestTexture2DSp(new pipeline::TestTexture2D(m_inFormat, textureWidth, textureHeight)))
	, m_swTexture	(m_inFormat, textureWidth, textureHeight, 1)
	, m_renderer	(context, VK_SAMPLE_COUNT_1_BIT, m_params->width, m_params->height, 1u, makeComponentMappingRGBA(), VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D, m_outFormat)
	, m_cd			(computeColorDistance())
	, m_a			(lim(m_inFormat, 0),			lim(m_inFormat, 1)+m_cd[1]*2,	lim(m_inFormat, 2),				lim(m_inFormat, 3)+m_cd[3]*2)
	, m_b			(lim(m_inFormat, 0)+m_cd[0]*2,	lim(m_inFormat, 1),				lim(m_inFormat, 2)+m_cd[2]*2,	lim(m_inFormat, 3)			)
	, m_c			(lim(m_inFormat, 0)+m_cd[0]*1,	lim(m_inFormat, 1)+m_cd[1]*1,	lim(m_inFormat, 2)+m_cd[2]*1,	lim(m_inFormat, 3)+m_cd[3]*1)
	, m_d			(lim(m_inFormat, 0),			lim(m_inFormat, 1),				lim(m_inFormat, 2),				lim(m_inFormat, 3)			)
{
	tcu::IVec4 data[textureWidth * textureHeight] =
	{
		m_a, m_b, m_c, m_d, m_c, m_b, m_a,
		m_b, m_a, m_c, m_d, m_c, m_a, m_b,
		m_c, m_c, m_c, m_d, m_c, m_c, m_c,
		m_d, m_d, m_d, m_c, m_d, m_d, m_d,
		m_c, m_c, m_c, m_d, m_c, m_c, m_c,
		m_b, m_a, m_c, m_d, m_c, m_a, m_b,
		m_a, m_b, m_c, m_d, m_c, m_b, m_a,
	};

	m_swTexture.allocLevel(0);

	const tcu::PixelBufferAccess& swAccess	= m_swTexture.getLevel(0);
	const tcu::PixelBufferAccess& hwAccess	= m_hwTexture->getLevel(0, 0);

	for (int y = 0; y < textureHeight; ++y)
	{
		for (int x = 0; x < textureWidth; ++x)
		{
			swAccess.setPixel(data[y*textureWidth+x], x, y);
			hwAccess.setPixel(data[y*textureWidth+x], x, y);
		}
	}

	m_renderer.add2DTexture(m_hwTexture, VK_IMAGE_ASPECT_COLOR_BIT, TextureBinding::ImageBackingMode::IMAGE_BACKING_MODE_REGULAR);
}

int SnormLinearClampInstance::lim (const tcu::TextureFormat& format, int channelIdx)
{
	auto   channelBits(getTextureFormatBitDepth(format));
	return channelBits[channelIdx] ? (-deIntMaxValue32(channelBits[channelIdx])) : (-1);
}

tcu::IVec4 SnormLinearClampInstance::computeColorDistance () const
{
	return tcu::IVec4
	(
		static_cast<int>(std::floor(static_cast<float>(-lim(m_inFormat, 0)) / 127.0f)),
		static_cast<int>(std::floor(static_cast<float>(-lim(m_inFormat, 0)) / 127.0f)),
		static_cast<int>(std::floor(static_cast<float>(-lim(m_inFormat, 0)) / 127.0f)),
		static_cast<int>(std::floor(static_cast<float>(-lim(m_inFormat, 0)) / 127.0f))
	);
}

bool SnormLinearClampInstance::verifyPixels	(const tcu::PixelBufferAccess& rendered, const tcu::PixelBufferAccess& reference, const ReferenceParams& samplerParams, const std::vector<float>& texCoords) const
{
	tcu::LodPrecision				lodPrec;
	tcu::LookupPrecision			lookupPrec;

	const int						nuc				(getNumUsedChannels(m_inFormat.order));
	const int						width			(m_renderer.getRenderWidth());
	const int						height			(m_renderer.getRenderHeight());

	const tcu::IVec4				colorDistance	(computeColorDistance());
	std::unique_ptr<deUint8[]>		errorMaskData	(new deUint8[width * height * 4 * 4]);
	tcu::PixelBufferAccess			errorMask		(mapVkFormat(m_outFormat), width, height, 1, errorMaskData.get());


	lodPrec.derivateBits			= 18;
	lodPrec.lodBits					= 5;

	lookupPrec.uvwBits				= tcu::IVec3(5,5,0);
	lookupPrec.coordBits			= tcu::IVec3(20,20,0);
	lookupPrec.colorMask			= tcu::BVec4(nuc >= 1, nuc >= 2, nuc >=3, nuc >= 4);
	lookupPrec.colorThreshold		= tcu::Vec4(0.9f/float(colorDistance[0]),
												0.9f/float(colorDistance[1]),
												0.9f/float(colorDistance[2]),
												0.9f/float(colorDistance[3]));

	const int numFailedPixels		= glu::TextureTestUtil::computeTextureLookupDiff(rendered, reference, errorMask,
																					 m_swTexture, texCoords.data(), samplerParams,
																					 lookupPrec, lodPrec, /*watchDog*/nullptr);
	if (numFailedPixels)
	{
		const int	numTotalPixels	= width * height;
		auto&		log				= m_context.getTestContext().getLog();
		const auto	formatName		= de::toLower(std::string(getFormatName(m_params->format)).substr(10));

		log << tcu::TestLog::Message << "ERROR: Result verification failed, got " << numFailedPixels << " invalid pixels!" << tcu::TestLog::EndMessage;
		log << tcu::TestLog::Message << "       " << float(numFailedPixels * 100)/float(numTotalPixels) << "% failed from " << numTotalPixels << " compared pixel count." << tcu::TestLog::EndMessage;
		log << tcu::TestLog::Message << "       ColorThreshold: " << lookupPrec.colorThreshold << ", ColorMask: " << lookupPrec.colorMask << tcu::TestLog::EndMessage;

		log << tcu::TestLog::ImageSet("VerifyResult", "Verification result");
		{
			log << tcu::TestLog::Image("Res_"+formatName, "Rendered image", rendered);
			log << tcu::TestLog::Image("Ref_"+formatName, "Reference image", reference);
			log << tcu::TestLog::Image("Err_"+formatName, "Error mask image", errorMask);
		}
		log << tcu::TestLog::EndImageSet;
	}

	int numOutOfRangePixels			= 0;
	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			const auto px = rendered.getPixel(x, y);
			if (tcu::boolAny(tcu::lessThan(px, tcu::Vec4(-1.0f))) || tcu::boolAny(tcu::greaterThan(px, tcu::Vec4(+1.0f))))
				++numOutOfRangePixels;
		}
	}

	if (numOutOfRangePixels)
	{
		auto&		log					= m_context.getTestContext().getLog();
		log << tcu::TestLog::Message << "ERROR: Found " << numOutOfRangePixels << " out of range [-1.0f, +1.0f]." << tcu::TestLog::EndMessage;
	}

	return (numFailedPixels == 0 && numOutOfRangePixels == 0);
}

tcu::TestStatus SnormLinearClampInstance::iterate (void)
{
	std::vector<float>				texCoords		(8);
	ReferenceParams					samplerParams	(TEXTURETYPE_2D);
	tcu::TextureFormat				resultFormat	(mapVkFormat(m_outFormat));

	// Setup renderers.
	const int						width			(m_renderer.getRenderWidth());
	const int						height			(m_renderer.getRenderHeight());
	std::unique_ptr<deUint8[]>		renderedData	(new deUint8[width * height * 4 * 4]);
	std::unique_ptr<deUint8[]>		referenceData	(new deUint8[width * height * 4 * 4]);
	tcu::PixelBufferAccess			rendered		(resultFormat, width, height, 1, renderedData.get());
	tcu::PixelBufferAccess			reference		(resultFormat, width, height, 1, referenceData.get());

	// Setup sampler params.
	samplerParams.sampler			= util::createSampler(tcu::Sampler::WrapMode::REPEAT_GL, tcu::Sampler::WrapMode::REPEAT_GL,
														  tcu::Sampler::FilterMode::LINEAR, tcu::Sampler::FilterMode::LINEAR, true);
	samplerParams.samplerType		= SAMPLERTYPE_FLOAT;
	samplerParams.lodMode			= LODMODE_EXACT;
	samplerParams.colorScale		= tcu::Vec4(1.0f);
	samplerParams.colorBias			= tcu::Vec4(0.0f);

	// Compute texture coordinates.
	computeQuadTexCoord2D(texCoords, tcu::Vec2(0.0f), tcu::Vec2(1.0f));

	// Peform online rendering with Vulkan.
	m_renderer.renderQuad(rendered, 0, texCoords.data(), samplerParams);

	// Perform offline rendering with software renderer.
	sampleTexture(reference, m_swTexture, texCoords.data(), samplerParams);

	return verifyPixels(rendered, reference, samplerParams, texCoords)
			? tcu::TestStatus::pass("")
			: tcu::TestStatus::fail("Pixels verification failed");
}

class SnormLinearClampTestCase : public TestCase
{
public:
	using ParamsSp = de::SharedPtr<SnormLinearClampInstance::Params>;

						SnormLinearClampTestCase	(tcu::TestContext&	testCtx,
													 const std::string&	name,
													 const std::string&	description,
													 ParamsSp			params)
		: TestCase	(testCtx, name, description)
		, m_params	(params)
	{
	}

	vkt::TestInstance*	createInstance				(vkt::Context&		context) const override
	{
		return new SnormLinearClampInstance(context, m_params);
	}

	virtual void		checkSupport				(vkt::Context&		context) const override
	{
		VkFormatProperties		formatProperties;

		context.getInstanceInterface().getPhysicalDeviceFormatProperties(
				context.getPhysicalDevice(),
				m_params->format,
				&formatProperties);

		if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
			TCU_THROW(NotSupportedError, "Linear filtering for this image format is not supported");
	}

	virtual void		initPrograms				(SourceCollections&	programCollection) const override
	{
		initializePrograms(programCollection, glu::Precision::PRECISION_HIGHP, std::vector<Program>({PROGRAM_2D_FLOAT}), DE_NULL, glu::Precision::PRECISION_HIGHP);
	}

private:
	ParamsSp	m_params;
};

void populateUfloatNegativeValuesTests (tcu::TestCaseGroup* group)
{
	tcu::TestContext&	testCtx = group->getTestContext();
	VkImageUsageFlags	usage	= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

	VkImageCreateInfo	info	=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType          sType
		DE_NULL,								// const void*              pNext
		0,										// VkImageCreateFlags       flags
		VK_IMAGE_TYPE_2D,						// VkImageType              imageType
		VK_FORMAT_B10G11R11_UFLOAT_PACK32,		// VkFormat                 format
		{50u, 50u, 1u},							// VkExtent3D               extent
		1u,										// uint32_t                 mipLevels
		1u,										// uint32_t                 arrayLayers
		VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits    samples
		VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling            tiling
		usage,									// VkImageUsageFlags        usage
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode            sharingMode
		0u,										// uint32_t                 queueFamilyIndexCount
		DE_NULL,								// const uint32_t*          pQueueFamilyIndices
		VK_IMAGE_LAYOUT_UNDEFINED				// VkImageLayout            initialLayout
	};

	group->addChild(cts_amber::createAmberTestCase(testCtx, "b10g11r11", "", "texture/conversion/ufloat_negative_values", "b10g11r11-ufloat-pack32.amber",
					std::vector<std::string>(), std::vector<VkImageCreateInfo>(1, info)));
}

void populateSnormClampTests (tcu::TestCaseGroup* group)
{
	tcu::TestContext&	testCtx	= group->getTestContext();
	VkImageUsageFlags	usage	= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

	VkImageCreateInfo	info	=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType          sType
		DE_NULL,								// const void*              pNext
		0,										// VkImageCreateFlags       flags
		VK_IMAGE_TYPE_1D,						// VkImageType              imageType
		VK_FORMAT_UNDEFINED,					// VkFormat                 format
		{1u, 1u, 1u},							// VkExtent3D               extent
		1u,										// uint32_t                 mipLevels
		1u,										// uint32_t                 arrayLayers
		VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits    samples
		VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling            tiling
		usage,									// VkImageUsageFlags        usage
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode            sharingMode
		0u,										// uint32_t                 queueFamilyIndexCount
		DE_NULL,								// const uint32_t*          pQueueFamilyIndices
		VK_IMAGE_LAYOUT_UNDEFINED				// VkImageLayout            initialLayout
	};

	struct TestParams
	{
		std::string testName;
		std::string amberFile;
		VkFormat	format;
	} params[] =
	{
		{ "a2b10g10r10_snorm_pack32",	"a2b10g10r10-snorm-pack32.amber",	VK_FORMAT_A2B10G10R10_SNORM_PACK32	},
		{ "a2r10g10b10_snorm_pack32",	"a2r10g10b10-snorm-pack32.amber",	VK_FORMAT_A2R10G10B10_SNORM_PACK32	},
		{ "a8b8g8r8_snorm_pack32",		"a8b8g8r8-snorm-pack32.amber",		VK_FORMAT_A8B8G8R8_SNORM_PACK32		},
		{ "b8g8r8a8_snorm",				"b8g8r8a8-snorm.amber",				VK_FORMAT_B8G8R8A8_SNORM			},
		{ "b8g8r8_snorm",				"b8g8r8-snorm.amber",				VK_FORMAT_B8G8R8_SNORM				},
		{ "r16g16b16a16_snorm",			"r16g16b16a16-snorm.amber",			VK_FORMAT_R16G16B16A16_SNORM		},
		{ "r16g16b16_snorm",			"r16g16b16-snorm.amber",			VK_FORMAT_R16G16B16_SNORM			},
		{ "r16g16_snorm",				"r16g16-snorm.amber",				VK_FORMAT_R16G16_SNORM				},
		{ "r16_snorm",					"r16-snorm.amber",					VK_FORMAT_R16_SNORM					},
		{ "r8g8b8a8_snorm",				"r8g8b8a8-snorm.amber",				VK_FORMAT_R8G8B8A8_SNORM			},
		{ "r8g8b8_snorm",				"r8g8b8-snorm.amber",				VK_FORMAT_R8G8B8_SNORM				},
		{ "r8g8_snorm",					"r8g8-snorm.amber",					VK_FORMAT_R8G8_SNORM				},
		{ "r8_snorm",					"r8-snorm.amber",					VK_FORMAT_R8_SNORM					},
	};

	for (const auto& param : params)
	{
		info.format = param.format;
		group->addChild(cts_amber::createAmberTestCase(testCtx, param.testName.c_str(), "", "texture/conversion/snorm_clamp", param.amberFile.c_str(),
						std::vector<std::string>(), std::vector<VkImageCreateInfo>(1, info)));
	}
}

void populateSnormLinearClampTests (tcu::TestCaseGroup* group)
{
	struct TestParams
	{
		std::string testName;
		VkFormat	format;
	}
	testParams[] =
	{
		{ "a2b10g10r10_snorm_pack32",	VK_FORMAT_A2B10G10R10_SNORM_PACK32	},
		{ "a2r10g10b10_snorm_pack32",	VK_FORMAT_A2R10G10B10_SNORM_PACK32	},
		{ "a8b8g8r8_snorm_pack32",		VK_FORMAT_A8B8G8R8_SNORM_PACK32		},
		{ "b8g8r8a8_snorm",				VK_FORMAT_B8G8R8A8_SNORM			},
		{ "b8g8r8_snorm",				VK_FORMAT_B8G8R8_SNORM				},
		{ "r16g16b16a16_snorm",			VK_FORMAT_R16G16B16A16_SNORM		},
		{ "r16g16b16_snorm",			VK_FORMAT_R16G16B16_SNORM			},
		{ "r16g16_snorm",				VK_FORMAT_R16G16_SNORM				},
		{ "r16_snorm",					VK_FORMAT_R16_SNORM					},
		{ "r8g8b8a8_snorm",				VK_FORMAT_R8G8B8A8_SNORM			},
		{ "r8g8b8_snorm",				VK_FORMAT_R8G8B8_SNORM				},
		{ "r8g8_snorm",					VK_FORMAT_R8G8_SNORM				},
		{ "r8_snorm",					VK_FORMAT_R8_SNORM					},
	};

	tcu::TestContext&	testCtx				= group->getTestContext();
	int					sizeMultipler		= 20;

	for (const auto& testParam : testParams)
	{
		const int		tw					= SnormLinearClampInstance::textureWidth * sizeMultipler;
		const int		th					= SnormLinearClampInstance::textureHeight * sizeMultipler;

		de::SharedPtr<SnormLinearClampInstance::Params> params(new SnormLinearClampInstance::Params{testParam.format, tw, th});
		group->addChild(new SnormLinearClampTestCase(testCtx, testParam.testName, {}, params));

		sizeMultipler += 2;
	}
}

void populateTextureConversionTests (tcu::TestCaseGroup* group)
{
	tcu::TestContext& testCtx = group->getTestContext();

	group->addChild(createTestGroup(testCtx, "ufloat_negative_values", "Tests for converting negative floats to unsigned floats", populateUfloatNegativeValuesTests));
	group->addChild(createTestGroup(testCtx, "snorm_clamp", "Tests for SNORM corner cases when smallest negative number gets clamped to -1", populateSnormClampTests));
	group->addChild(createTestGroup(testCtx, "snorm_clamp_linear", "Tests for SNORM corner cases when negative number gets clamped to -1 after applying linear filtering", populateSnormLinearClampTests));
}

} // anonymous namespace

tcu::TestCaseGroup* createTextureConversionTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "conversion", "Texture conversion tests.", populateTextureConversionTests);
}

} // texture
} // vkt
