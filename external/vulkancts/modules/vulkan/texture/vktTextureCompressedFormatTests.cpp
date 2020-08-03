/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 * Copyright (c) 2014 The Android Open Source Project
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
 * \brief Compressed texture tests.
 *//*--------------------------------------------------------------------*/

#include "vktTextureCompressedFormatTests.hpp"

#include "deString.h"
#include "deStringUtil.hpp"
#include "tcuCompressedTexture.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuAstcUtil.hpp"
#include "vkImageUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTextureTestUtil.hpp"
#include <string>
#include <vector>

namespace vkt
{
namespace texture
{
namespace
{

using namespace vk;
using namespace glu::TextureTestUtil;
using namespace texture::util;

using std::string;
using std::vector;
using tcu::Sampler;
using tcu::TestLog;

// Compressed formats
static const struct {
	const VkFormat	format;
} formats[] =
{
	{ VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK		},
	{ VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK		},
	{ VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK	},
	{ VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK	},
	{ VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK	},
	{ VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK	},

	{ VK_FORMAT_EAC_R11_UNORM_BLOCK			},
	{ VK_FORMAT_EAC_R11_SNORM_BLOCK			},
	{ VK_FORMAT_EAC_R11G11_UNORM_BLOCK		},
	{ VK_FORMAT_EAC_R11G11_SNORM_BLOCK		},

	{ VK_FORMAT_ASTC_4x4_UNORM_BLOCK		},
	{ VK_FORMAT_ASTC_4x4_SRGB_BLOCK			},
	{ VK_FORMAT_ASTC_5x4_UNORM_BLOCK		},
	{ VK_FORMAT_ASTC_5x4_SRGB_BLOCK			},
	{ VK_FORMAT_ASTC_5x5_UNORM_BLOCK		},
	{ VK_FORMAT_ASTC_5x5_SRGB_BLOCK			},
	{ VK_FORMAT_ASTC_6x5_UNORM_BLOCK		},
	{ VK_FORMAT_ASTC_6x5_SRGB_BLOCK			},
	{ VK_FORMAT_ASTC_6x6_UNORM_BLOCK		},
	{ VK_FORMAT_ASTC_6x6_SRGB_BLOCK			},
	{ VK_FORMAT_ASTC_8x5_UNORM_BLOCK		},
	{ VK_FORMAT_ASTC_8x5_SRGB_BLOCK			},
	{ VK_FORMAT_ASTC_8x6_UNORM_BLOCK		},
	{ VK_FORMAT_ASTC_8x6_SRGB_BLOCK			},
	{ VK_FORMAT_ASTC_8x8_UNORM_BLOCK		},
	{ VK_FORMAT_ASTC_8x8_SRGB_BLOCK			},
	{ VK_FORMAT_ASTC_10x5_UNORM_BLOCK		},
	{ VK_FORMAT_ASTC_10x5_SRGB_BLOCK		},
	{ VK_FORMAT_ASTC_10x6_UNORM_BLOCK		},
	{ VK_FORMAT_ASTC_10x6_SRGB_BLOCK		},
	{ VK_FORMAT_ASTC_10x8_UNORM_BLOCK		},
	{ VK_FORMAT_ASTC_10x8_SRGB_BLOCK		},
	{ VK_FORMAT_ASTC_10x10_UNORM_BLOCK		},
	{ VK_FORMAT_ASTC_10x10_SRGB_BLOCK		},
	{ VK_FORMAT_ASTC_12x10_UNORM_BLOCK		},
	{ VK_FORMAT_ASTC_12x10_SRGB_BLOCK		},
	{ VK_FORMAT_ASTC_12x12_UNORM_BLOCK		},
	{ VK_FORMAT_ASTC_12x12_SRGB_BLOCK		},

	{ VK_FORMAT_BC1_RGB_UNORM_BLOCK			},
	{ VK_FORMAT_BC1_RGB_SRGB_BLOCK			},
	{ VK_FORMAT_BC1_RGBA_UNORM_BLOCK		},
	{ VK_FORMAT_BC1_RGBA_SRGB_BLOCK			},
	{ VK_FORMAT_BC2_UNORM_BLOCK				},
	{ VK_FORMAT_BC2_SRGB_BLOCK				},
	{ VK_FORMAT_BC3_UNORM_BLOCK				},
	{ VK_FORMAT_BC3_SRGB_BLOCK				},
	{ VK_FORMAT_BC4_UNORM_BLOCK				},
	{ VK_FORMAT_BC4_SNORM_BLOCK				},
	{ VK_FORMAT_BC5_UNORM_BLOCK				},
	{ VK_FORMAT_BC5_SNORM_BLOCK				},
	{ VK_FORMAT_BC6H_UFLOAT_BLOCK			},
	{ VK_FORMAT_BC6H_SFLOAT_BLOCK			},
	{ VK_FORMAT_BC7_UNORM_BLOCK				},
	{ VK_FORMAT_BC7_SRGB_BLOCK				}
};

static const struct {
	const int	width;
	const int	height;
	const int   depth;		// 2D test ignore depth value
	const char*	name;
} sizes[] =
{
	{ 128, 64, 8,  "pot"  },
	{ 51,  65, 17, "npot" },
};

static const struct {
	const char*								name;
	const TextureBinding::ImageBackingMode	backingMode;
} backingModes[] =
{
	{ "",			TextureBinding::IMAGE_BACKING_MODE_REGULAR	},
	{ "_sparse",	TextureBinding::IMAGE_BACKING_MODE_SPARSE	}
};

struct Compressed3DTestParameters : public Texture3DTestCaseParameters
{
										Compressed3DTestParameters	(void);
	TextureBinding::ImageBackingMode	backingMode;
};

Compressed3DTestParameters::Compressed3DTestParameters (void)
	: backingMode(TextureBinding::IMAGE_BACKING_MODE_REGULAR)
{
}

struct Compressed2DTestParameters : public Texture2DTestCaseParameters
{
										Compressed2DTestParameters	(void);
	TextureBinding::ImageBackingMode	backingMode;
};

Compressed2DTestParameters::Compressed2DTestParameters (void)
	: backingMode(TextureBinding::IMAGE_BACKING_MODE_REGULAR)
{
}

class Compressed2DTestInstance : public TestInstance
{
public:
	typedef Compressed2DTestParameters	ParameterType;

										Compressed2DTestInstance	(Context&				context,
																	 const ParameterType&	testParameters);
	tcu::TestStatus						iterate						(void);

private:
										Compressed2DTestInstance	(const Compressed2DTestInstance& other);
	Compressed2DTestInstance&			operator=					(const Compressed2DTestInstance& other);

	const ParameterType&				m_testParameters;
	const tcu::CompressedTexFormat		m_compressedFormat;
	TestTexture2DSp						m_texture;
	TextureRenderer						m_renderer;
};

Compressed2DTestInstance::Compressed2DTestInstance (Context&				context,
													const ParameterType&	testParameters)
	: TestInstance			(context)
	, m_testParameters		(testParameters)
	, m_compressedFormat	(mapVkCompressedFormat(testParameters.format))
	, m_texture				(TestTexture2DSp(new pipeline::TestTexture2D(m_compressedFormat, testParameters.width, testParameters.height)))
	, m_renderer			(context, testParameters.sampleCount, testParameters.width, testParameters.height)
{
	m_renderer.add2DTexture(m_texture, testParameters.aspectMask, testParameters.backingMode);
}

tcu::TestStatus Compressed2DTestInstance::iterate (void)
{
	tcu::TestLog&					log				= m_context.getTestContext().getLog();
	const pipeline::TestTexture2D&	texture			= m_renderer.get2DTexture(0);
	const tcu::TextureFormat		textureFormat	= texture.getTextureFormat();
	const tcu::TextureFormatInfo	formatInfo		= tcu::getTextureFormatInfo(textureFormat);

	ReferenceParams					sampleParams	(TEXTURETYPE_2D);
	tcu::Surface					rendered		(m_renderer.getRenderWidth(), m_renderer.getRenderHeight());
	vector<float>					texCoord;

	// Setup params for reference.
	sampleParams.sampler			= util::createSampler(m_testParameters.wrapS, m_testParameters.wrapT, m_testParameters.minFilter, m_testParameters.magFilter);
	sampleParams.samplerType		= SAMPLERTYPE_FLOAT;
	sampleParams.lodMode			= LODMODE_EXACT;

	if (isAstcFormat(m_compressedFormat)
		|| m_compressedFormat == tcu::COMPRESSEDTEXFORMAT_BC4_UNORM_BLOCK
		|| m_compressedFormat == tcu::COMPRESSEDTEXFORMAT_BC5_UNORM_BLOCK)
	{
		sampleParams.colorBias			= tcu::Vec4(0.0f);
		sampleParams.colorScale			= tcu::Vec4(1.0f);
	}
	else if (m_compressedFormat == tcu::COMPRESSEDTEXFORMAT_BC4_SNORM_BLOCK)
	{
		sampleParams.colorBias			= tcu::Vec4(0.5f, 0.0f, 0.0f, 0.0f);
		sampleParams.colorScale			= tcu::Vec4(0.5f, 1.0f, 1.0f, 1.0f);
	}
	else if (m_compressedFormat == tcu::COMPRESSEDTEXFORMAT_BC5_SNORM_BLOCK)
	{
		sampleParams.colorBias			= tcu::Vec4(0.5f, 0.5f, 0.0f, 0.0f);
		sampleParams.colorScale			= tcu::Vec4(0.5f, 0.5f, 1.0f, 1.0f);
	}
	else
	{
		sampleParams.colorBias			= formatInfo.lookupBias;
		sampleParams.colorScale			= formatInfo.lookupScale;
	}

	log << TestLog::Message << "Compare reference value = " << sampleParams.ref << TestLog::EndMessage;

	// Compute texture coordinates.
	computeQuadTexCoord2D(texCoord, tcu::Vec2(0.0f, 0.0f), tcu::Vec2(1.0f, 1.0f));

	m_renderer.renderQuad(rendered, 0, &texCoord[0], sampleParams);

	// Compute reference.
	const tcu::IVec4		formatBitDepth	= getTextureFormatBitDepth(vk::mapVkFormat(VK_FORMAT_R8G8B8A8_UNORM));
	const tcu::PixelFormat	pixelFormat		(formatBitDepth[0], formatBitDepth[1], formatBitDepth[2], formatBitDepth[3]);
	tcu::Surface			referenceFrame	(m_renderer.getRenderWidth(), m_renderer.getRenderHeight());
	sampleTexture(tcu::SurfaceAccess(referenceFrame, pixelFormat), m_texture->getTexture(), &texCoord[0], sampleParams);

	// Compare and log.
	tcu::RGBA threshold;

	if (isBcBitExactFormat(m_compressedFormat))
		threshold = tcu::RGBA(1, 1, 1, 1);
	else if (isBcFormat(m_compressedFormat))
		threshold = tcu::RGBA(8, 8, 8, 8);
	else
		threshold = pixelFormat.getColorThreshold() + tcu::RGBA(2, 2, 2, 2);

	const bool isOk = compareImages(log, referenceFrame, rendered, threshold);

	return isOk ? tcu::TestStatus::pass("Pass") : tcu::TestStatus::fail("Image verification failed");
}

class Compressed3DTestInstance : public TestInstance
{
public:
	typedef Compressed3DTestParameters	ParameterType;

										Compressed3DTestInstance	(Context&				context,
																	 const ParameterType&	testParameters);
	tcu::TestStatus						iterate						(void);

private:
										Compressed3DTestInstance	(const Compressed3DTestInstance& other);
	Compressed3DTestInstance&			operator=					(const Compressed3DTestInstance& other);

	const ParameterType&				m_testParameters;
	const tcu::CompressedTexFormat		m_compressedFormat;
	TestTexture3DSp						m_texture3D;
	TextureRenderer						m_renderer3D;
};

Compressed3DTestInstance::Compressed3DTestInstance (Context&				context,
													const ParameterType&	testParameters)
	: TestInstance			(context)
	, m_testParameters		(testParameters)
	, m_compressedFormat	(mapVkCompressedFormat(testParameters.format))
	, m_texture3D			(TestTexture3DSp(new pipeline::TestTexture3D(m_compressedFormat, testParameters.width, testParameters.height, testParameters.depth)))
	, m_renderer3D			(context, testParameters.sampleCount, testParameters.width, testParameters.height, testParameters.depth, makeComponentMappingRGBA(), VK_IMAGE_TYPE_3D, VK_IMAGE_VIEW_TYPE_3D)
{
	m_renderer3D.add3DTexture		(m_texture3D, testParameters.aspectMask, testParameters.backingMode);

	VkPhysicalDeviceFeatures		physicalFeatures;
	context.getInstanceInterface().getPhysicalDeviceFeatures(context.getPhysicalDevice(), &physicalFeatures);

	if (tcu::isAstcFormat(m_compressedFormat))
	{
		if (!physicalFeatures.textureCompressionASTC_LDR)
			throw tcu::NotSupportedError(std::string("Unsupported format: ") + getFormatName(testParameters.format));
	}
	else if (tcu::isEtcFormat(m_compressedFormat))
	{
		if (!physicalFeatures.textureCompressionETC2)
			throw tcu::NotSupportedError(std::string("Unsupported format: ") + getFormatName(testParameters.format));
	}
	else if(tcu::isBcFormat(m_compressedFormat))
	{
		if (!physicalFeatures.textureCompressionBC)
			throw tcu::NotSupportedError(std::string("Unsupported format: ") + getFormatName(testParameters.format));
	}
	else
	{
		DE_FATAL("Unsupported compressed format");
	}
}

tcu::TestStatus Compressed3DTestInstance::iterate (void)
{
	tcu::TestLog&					log				= m_context.getTestContext().getLog();
	const pipeline::TestTexture3D&	texture			= m_renderer3D.get3DTexture(0);
	const tcu::TextureFormat		textureFormat	= texture.getTextureFormat();
	const tcu::TextureFormatInfo	formatInfo		= tcu::getTextureFormatInfo(textureFormat);

	ReferenceParams					sampleParams	(TEXTURETYPE_3D);
	tcu::Surface					rendered		(m_renderer3D.getRenderWidth(), m_renderer3D.getRenderHeight());
	vector<float>					texCoord;

	// Setup params for reference.
	sampleParams.sampler			= util::createSampler(m_testParameters.wrapS, m_testParameters.wrapT, m_testParameters.minFilter, m_testParameters.magFilter);
	sampleParams.samplerType		= SAMPLERTYPE_FLOAT;
	sampleParams.lodMode			= LODMODE_EXACT;

	if (isAstcFormat(m_compressedFormat)
		|| m_compressedFormat == tcu::COMPRESSEDTEXFORMAT_BC4_UNORM_BLOCK
		|| m_compressedFormat == tcu::COMPRESSEDTEXFORMAT_BC5_UNORM_BLOCK)
	{
		sampleParams.colorBias			= tcu::Vec4(0.0f);
		sampleParams.colorScale			= tcu::Vec4(1.0f);
	}
	else if (m_compressedFormat == tcu::COMPRESSEDTEXFORMAT_BC4_SNORM_BLOCK)
	{
		sampleParams.colorBias			= tcu::Vec4(0.5f, 0.0f, 0.0f, 0.0f);
		sampleParams.colorScale			= tcu::Vec4(0.5f, 1.0f, 1.0f, 1.0f);
	}
	else if (m_compressedFormat == tcu::COMPRESSEDTEXFORMAT_BC5_SNORM_BLOCK)
	{
		sampleParams.colorBias			= tcu::Vec4(0.5f, 0.5f, 0.0f, 0.0f);
		sampleParams.colorScale			= tcu::Vec4(0.5f, 0.5f, 1.0f, 1.0f);
	}
	else
	{
		sampleParams.colorBias			= formatInfo.lookupBias;
		sampleParams.colorScale			= formatInfo.lookupScale;
	}

	log << TestLog::Message << "Compare reference value = " << sampleParams.ref << TestLog::EndMessage;

	constexpr deUint32		slices			= 3;
	deUint32				sliceNdx		= 0;
	float					z				= 0;
	bool					isOk			= false;
	const tcu::IVec4		formatBitDepth	= getTextureFormatBitDepth(vk::mapVkFormat(VK_FORMAT_R8G8B8A8_UNORM));
	const tcu::PixelFormat	pixelFormat	(formatBitDepth[0], formatBitDepth[1], formatBitDepth[2], formatBitDepth[3]);
	tcu::RGBA				threshold;

	if (isBcBitExactFormat(m_compressedFormat))
		threshold = tcu::RGBA(1, 1, 1, 1);
	else if (isBcSRGBFormat(m_compressedFormat))
		threshold = tcu::RGBA(9, 9, 9, 9);
	else if (isBcFormat(m_compressedFormat))
		threshold = tcu::RGBA(8, 8, 8, 8);
	else
		threshold = pixelFormat.getColorThreshold() + tcu::RGBA(2, 2, 2, 2);

	for (deUint32 s = 0; s < slices; ++s)
	{
		// Test different slices of 3D texture.

		sliceNdx = (m_testParameters.depth - 1) * s / (slices - 1);

		// Render texture.
		z = ((float)sliceNdx + 0.5f) / (float)m_testParameters.depth;
		computeQuadTexCoord3D(texCoord, tcu::Vec3(0.0f, 0.0f, z), tcu::Vec3(1.0f, 1.0f, z), tcu::IVec3(0,1,2));
		m_renderer3D.renderQuad(rendered, 0, &texCoord[0], sampleParams);

		// Compute reference.
		tcu::Surface referenceFrame	(m_renderer3D.getRenderWidth(), m_renderer3D.getRenderHeight());
		sampleTexture(tcu::SurfaceAccess(referenceFrame, pixelFormat), m_texture3D->getTexture(), &texCoord[0], sampleParams);

		// Compare and log.
		isOk = compareImages(log, referenceFrame, rendered, threshold);

		if (!isOk)
			break;
	}

	return isOk ? tcu::TestStatus::pass("Pass") : tcu::TestStatus::fail("Image verification failed");
}

void populateTextureCompressedFormatTests (tcu::TestCaseGroup* compressedTextureTests)
{
	tcu::TestContext&	testCtx	= compressedTextureTests->getTestContext();

	for (int sizeNdx = 0; sizeNdx < DE_LENGTH_OF_ARRAY(sizes); sizeNdx++)
	for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(formats); formatNdx++)
	for (int backingNdx = 0; backingNdx < DE_LENGTH_OF_ARRAY(backingModes); backingNdx++)
	{
		const string	formatStr	= de::toString(getFormatStr(formats[formatNdx].format));
		const string	nameBase	= de::toLower(formatStr.substr(10));

		Compressed2DTestParameters	testParameters;
		testParameters.format		= formats[formatNdx].format;
		testParameters.backingMode	= backingModes[backingNdx].backingMode;
		testParameters.width		= sizes[sizeNdx].width;
		testParameters.height		= sizes[sizeNdx].height;
		testParameters.minFilter	= tcu::Sampler::NEAREST;
		testParameters.magFilter	= tcu::Sampler::NEAREST;
		testParameters.aspectMask	= VK_IMAGE_ASPECT_COLOR_BIT;
		testParameters.programs.push_back(PROGRAM_2D_FLOAT);

		compressedTextureTests->addChild(new TextureTestCase<Compressed2DTestInstance>(testCtx, (nameBase + "_2d_" + sizes[sizeNdx].name + backingModes[backingNdx].name).c_str(), (formatStr + ", TEXTURETYPE_2D").c_str(), testParameters));
	}
}

void populate3DTextureCompressedFormatTests (tcu::TestCaseGroup* compressedTextureTests)
{
	tcu::TestContext&	testCtx	= compressedTextureTests->getTestContext();

	for (int sizeNdx = 0; sizeNdx < DE_LENGTH_OF_ARRAY(sizes); ++sizeNdx)
	for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(formats); ++formatNdx)
	for (int backingNdx = 0; backingNdx < DE_LENGTH_OF_ARRAY(backingModes); ++backingNdx)
	{
		const string	formatStr	= de::toString(getFormatStr(formats[formatNdx].format));
		const string	nameBase	= de::toLower(formatStr.substr(10));

		Compressed3DTestParameters	testParameters;
		testParameters.format		= formats[formatNdx].format;
		testParameters.backingMode	= backingModes[backingNdx].backingMode;
		testParameters.width		= sizes[sizeNdx].width;
		testParameters.height		= sizes[sizeNdx].height;
		testParameters.depth        = sizes[sizeNdx].depth;
		testParameters.minFilter	= tcu::Sampler::NEAREST;
		testParameters.magFilter	= tcu::Sampler::NEAREST;
		testParameters.aspectMask	= VK_IMAGE_ASPECT_COLOR_BIT;
		testParameters.programs.push_back(PROGRAM_3D_FLOAT);

		compressedTextureTests->addChild(new TextureTestCase<Compressed3DTestInstance>(testCtx, (nameBase + "_3d_" + sizes[sizeNdx].name + backingModes[backingNdx].name).c_str(), (formatStr + ", TEXTURETYPE_3D").c_str(), testParameters));
	}
}

} // anonymous

tcu::TestCaseGroup* createTextureCompressedFormatTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "compressed", "Texture compressed format tests.", populateTextureCompressedFormatTests);
}

tcu::TestCaseGroup* create3DTextureCompressedFormatTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "compressed_3D", "3D texture compressed format tests.", populate3DTextureCompressedFormatTests);
}

} // texture
} // vkt
