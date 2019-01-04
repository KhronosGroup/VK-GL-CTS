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
	m_renderer.add2DTexture(m_texture, testParameters.backingMode);
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

struct Compressed3DTestParameters : public Texture3DTestCaseParameters
{
										Compressed3DTestParameters	(void);
	TextureBinding::ImageBackingMode	backingMode;
};

Compressed3DTestParameters::Compressed3DTestParameters (void)
	: backingMode(TextureBinding::IMAGE_BACKING_MODE_REGULAR)
{
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
	TestTexture3DSp						m_texture;
	TextureRenderer						m_renderer;
};

Compressed3DTestInstance::Compressed3DTestInstance (Context&				context,
													const ParameterType&	testParameters)
	: TestInstance			(context)
	, m_testParameters		(testParameters)
	, m_compressedFormat	(mapVkCompressedFormat(testParameters.format))
	, m_texture				(TestTexture3DSp(new pipeline::TestTexture3D(m_compressedFormat, testParameters.width, testParameters.height, testParameters.depth)))
	, m_renderer			(context, testParameters.sampleCount, testParameters.width, testParameters.height)
{
	m_renderer.add3DTexture(m_texture, testParameters.backingMode);
}

tcu::TestStatus Compressed3DTestInstance::iterate (void)
{
	tcu::TestLog&					log				= m_context.getTestContext().getLog();
	const pipeline::TestTexture3D&	texture			= m_renderer.get3DTexture(0);
	const tcu::TextureFormat		textureFormat	= texture.getTextureFormat();
	const tcu::TextureFormatInfo	formatInfo		= tcu::getTextureFormatInfo(textureFormat);

	ReferenceParams					sampleParams	(TEXTURETYPE_3D);
	tcu::Surface					rendered		(m_renderer.getRenderWidth(), m_renderer.getRenderHeight());
	tcu::Vec3						texCoord[4];
	const float* const				texCoordPtr = (const float*)&texCoord[0];

	// Setup params for reference.
	sampleParams.sampler			= util::createSampler(m_testParameters.wrapS, m_testParameters.wrapT, m_testParameters.wrapR, m_testParameters.minFilter, m_testParameters.magFilter);
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
	{
		texCoord[0] = tcu::Vec3(0.0f,	0.0f,	0.0f);
		texCoord[1] = tcu::Vec3(0.0f,	1.0f,	0.5f);
		texCoord[2] = tcu::Vec3(1.0f,	0.0f,	0.5f);
		texCoord[3] = tcu::Vec3(1.0f,	1.0f,	1.0f);
	}

	m_renderer.renderQuad(rendered, 0, texCoordPtr, sampleParams);

	// Compute reference.
	const tcu::IVec4		formatBitDepth	= getTextureFormatBitDepth(vk::mapVkFormat(VK_FORMAT_R8G8B8A8_UNORM));
	const tcu::PixelFormat	pixelFormat		(formatBitDepth[0], formatBitDepth[1], formatBitDepth[2], formatBitDepth[3]);
	tcu::Surface			referenceFrame	(m_renderer.getRenderWidth(), m_renderer.getRenderHeight());
	sampleTexture(tcu::SurfaceAccess(referenceFrame, pixelFormat), (tcu::Texture3DView)m_texture->getTexture(), texCoordPtr, sampleParams);

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

void populateTextureCompressedFormatTests (tcu::TestCaseGroup* compressedTextureTests)
{
	tcu::TestContext&	testCtx	= compressedTextureTests->getTestContext();

	// ETC2 and EAC compressed formats.
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
		const int	depth;
		const char*	name;
	} sizes[] =
	{
		{ 128, 64, 64, "pot"  },
		{ 51,  65, 33, "npot" },
	};

	static const struct {
		const char*								name;
		const TextureBinding::ImageBackingMode	backingMode;
	} backingModes[] =
	{
		{ "",			TextureBinding::IMAGE_BACKING_MODE_REGULAR	},
		{ "_sparse",	TextureBinding::IMAGE_BACKING_MODE_SPARSE	}
	};

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
		testParameters.programs.push_back(PROGRAM_2D_FLOAT);

		compressedTextureTests->addChild(new TextureTestCase<Compressed2DTestInstance>(testCtx, (nameBase + "_2d_" + sizes[sizeNdx].name + backingModes[backingNdx].name).c_str(), (formatStr + ", TEXTURETYPE_2D").c_str(), testParameters));

		Compressed3DTestParameters		testParameters3D;
		testParameters3D.format			= formats[formatNdx].format;
		testParameters3D.backingMode	= backingModes[backingNdx].backingMode;
		testParameters3D.width			= sizes[sizeNdx].width;
		testParameters3D.height			= sizes[sizeNdx].height;
		testParameters3D.depth			= sizes[sizeNdx].depth;
		testParameters3D.minFilter		= tcu::Sampler::NEAREST;
		testParameters3D.magFilter		= tcu::Sampler::NEAREST;
		testParameters3D.programs.push_back(PROGRAM_3D_FLOAT);

		compressedTextureTests->addChild(new TextureTestCase<Compressed3DTestInstance>(testCtx, (nameBase + "_3d_" + sizes[sizeNdx].name + backingModes[backingNdx].name).c_str(), (formatStr + ", TEXTURETYPE_3D").c_str(), testParameters3D));
	}
}

} // anonymous

tcu::TestCaseGroup* createTextureCompressedFormatTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "compressed", "Texture compressed format tests.", populateTextureCompressedFormatTests);
}

} // texture
} // vkt
