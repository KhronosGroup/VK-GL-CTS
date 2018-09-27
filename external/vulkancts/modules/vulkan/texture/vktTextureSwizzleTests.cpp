/*------------------------------------------------------------------------
* Vulkan Conformance Tests
* ------------------------
*
* Copyright (c) 2018 Google Inc.
* Copyright (c) 2018 The Khronos Group Inc.
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
* \brief Texture swizzle tests.
*//*--------------------------------------------------------------------*/

#include "vktTextureSwizzleTests.hpp"

#include "vkImageUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTextureTestUtil.hpp"

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
using tcu::TestLog;

struct Swizzle2DTestParameters : public Texture2DTestCaseParameters
{
										Swizzle2DTestParameters	(void);
	TextureBinding::ImageBackingMode	backingMode;
	vk::VkComponentMapping				componentMapping;
};

Swizzle2DTestParameters::Swizzle2DTestParameters (void)
{
}

class Swizzle2DTestInstance : public TestInstance
{
public:
	typedef Swizzle2DTestParameters	ParameterType;

										Swizzle2DTestInstance	(Context&				context,
																 const ParameterType&	testParameters);
	tcu::TestStatus						iterate					(void);

private:
										Swizzle2DTestInstance	(const Swizzle2DTestInstance& other);
	Swizzle2DTestInstance&				operator=				(const Swizzle2DTestInstance& other);

	const ParameterType&				m_testParameters;
	const tcu::TextureFormat			m_format;
	const tcu::CompressedTexFormat		m_compressedFormat;
	TestTexture2DSp						m_texture;
	TextureRenderer						m_renderer;
};

Swizzle2DTestInstance::Swizzle2DTestInstance (Context&				context,
											  const ParameterType&	testParameters)
	: TestInstance			(context)
	, m_testParameters		(testParameters)
	// Hard format values unused
	, m_format				(isCompressedFormat(m_testParameters.format) ? tcu::TextureFormat() : mapVkFormat(testParameters.format))
	, m_compressedFormat	(isCompressedFormat(m_testParameters.format) ? mapVkCompressedFormat(testParameters.format) : tcu::CompressedTexFormat())
	, m_texture				(TestTexture2DSp(isCompressedFormat(m_testParameters.format)
								? new pipeline::TestTexture2D(m_compressedFormat, testParameters.width, testParameters.height)
								: new pipeline::TestTexture2D(m_format, testParameters.width, testParameters.height)))
	, m_renderer			(context, testParameters.sampleCount, testParameters.width, testParameters.height, testParameters.componentMapping)
{
	m_renderer.add2DTexture(m_texture, testParameters.backingMode);
}

tcu::TestStatus Swizzle2DTestInstance::iterate (void)
{
	tcu::TestLog&					log				= m_context.getTestContext().getLog();
	const pipeline::TestTexture2D&	texture			= m_renderer.get2DTexture(0);
	const tcu::TextureFormat		textureFormat	= texture.getTextureFormat();
	const float						lookupScale		= tcu::getTextureFormatInfo(textureFormat).lookupScale[0];
	const float						lookupBias		= tcu::getTextureFormatInfo(textureFormat).lookupBias[0];

	ReferenceParams					sampleParams	(TEXTURETYPE_2D);
	tcu::Surface					rendered		(m_renderer.getRenderWidth(), m_renderer.getRenderHeight());
	vector<float>					texCoord;

	// Params for reference
	sampleParams.sampler		= util::createSampler(m_testParameters.wrapS, m_testParameters.wrapT, m_testParameters.minFilter, m_testParameters.magFilter);
	sampleParams.samplerType	= isCompressedFormat(m_testParameters.format) ? SAMPLERTYPE_FLOAT : getSamplerType(m_format);
	sampleParams.lodMode		= LODMODE_EXACT;
	sampleParams.colorBias		= tcu::Vec4(lookupBias);
	sampleParams.colorScale		= tcu::Vec4(lookupScale);

	if (sampleParams.colorBias != tcu::Vec4(0.0f))
		sampleParams.flags = RenderParams::USE_BIAS;

	log << TestLog::Message << "Compare reference value = "	<< sampleParams.ref	<< TestLog::EndMessage;
	log << TestLog::Message << "Lookup scale = "			<< lookupScale		<< TestLog::EndMessage;
	log << TestLog::Message << "Lookup bias = "				<< lookupBias		<< TestLog::EndMessage;

	computeQuadTexCoord2D(texCoord, tcu::Vec2(0.0f, 0.0f), tcu::Vec2(1.0f, 1.0f));
	m_renderer.renderQuad(rendered, 0, &texCoord[0], sampleParams);

	const tcu::IVec4		formatBitDepth	= getTextureFormatBitDepth(vk::mapVkFormat(VK_FORMAT_R8G8B8A8_UNORM));
	const tcu::PixelFormat	pixelFormat		(formatBitDepth[0], formatBitDepth[1], formatBitDepth[2], formatBitDepth[3]);
	tcu::Surface			referenceFrame	(m_renderer.getRenderWidth(), m_renderer.getRenderHeight());

	sampleTexture(tcu::SurfaceAccess(referenceFrame, pixelFormat), m_texture->getTexture(), &texCoord[0], sampleParams);

	tcu::Surface			swizzledReference	= referenceFrame;

	// Swizzle reference
	{
		const tcu::IVec4			bitDepth	= getTextureFormatBitDepth(isCompressedFormat(m_testParameters.format)
													? getUncompressedFormat(m_compressedFormat)
													: m_format);
		const deUint8				one			= deUint8(255.0f * (lookupScale + lookupBias));
		const deUint8				zero		= deUint8(255.0f * lookupBias);

		const VkComponentSwizzle	swizzle[]	=
		{
			m_testParameters.componentMapping.r,
			m_testParameters.componentMapping.g,
			m_testParameters.componentMapping.b,
			m_testParameters.componentMapping.a
		};

		log << TestLog::Message << "Format bit depth = "	<< bitDepth		<< TestLog::EndMessage;
		log << TestLog::Message << "One = "					<< int(one)		<< TestLog::EndMessage;
		log << TestLog::Message << "Zero = "				<< int(zero)	<< TestLog::EndMessage;

		for (int y = 0; y < m_testParameters.height; y++)
		for (int x = 0; x < m_testParameters.width; x++)
		{
			const tcu::RGBA		original	= referenceFrame.getPixel(x, y);
			tcu::IVec4			swizzled	= original.toIVec();

			for (int ndx = 0; ndx < 4; ndx++)
			{
				switch (swizzle[ndx])
				{
					case VK_COMPONENT_SWIZZLE_ZERO:
						swizzled[ndx] = zero;
						break;
					case VK_COMPONENT_SWIZZLE_ONE:
						swizzled[ndx] = one;
						break;
					case VK_COMPONENT_SWIZZLE_R:
						swizzled[ndx] = bitDepth[0] ? original.getRed() : zero;
						break;
					case VK_COMPONENT_SWIZZLE_G:
						swizzled[ndx] = bitDepth[1] ? original.getGreen() : zero;
						break;
					case VK_COMPONENT_SWIZZLE_B:
						swizzled[ndx] = bitDepth[2] ? original.getBlue() : zero;
						break;
					case VK_COMPONENT_SWIZZLE_A:
						swizzled[ndx] = bitDepth[3] ? original.getAlpha() : one;
						break;
					default:
						DE_ASSERT(swizzle[ndx] == VK_COMPONENT_SWIZZLE_IDENTITY);
				}
			}

			swizzledReference.setPixel(x, y, tcu::RGBA(swizzled.x(), swizzled.y(), swizzled.z(), swizzled.w()));
		}
	}

	// Compare and log
	const tcu::RGBA	threshold	= pixelFormat.getColorThreshold() + tcu::RGBA(2, 2, 2, 2);
	const bool	isOk			= compareImages(log, swizzledReference, rendered, threshold);

	return isOk ? tcu::TestStatus::pass("Pass") : tcu::TestStatus::fail("Image verification failed");
}

void populateTextureSwizzleTests (tcu::TestCaseGroup* textureSwizzleTests)
{
	tcu::TestContext&	testCtx	= textureSwizzleTests->getTestContext();

	static const struct{
		const VkFormat	format;
		const Program	program;
	} formats2D[] =
	{
		{ VK_FORMAT_R4G4_UNORM_PACK8,			PROGRAM_2D_FLOAT		},
		{ VK_FORMAT_R4G4B4A4_UNORM_PACK16,		PROGRAM_2D_FLOAT		},
		{ VK_FORMAT_R5G6B5_UNORM_PACK16,		PROGRAM_2D_FLOAT		},
		{ VK_FORMAT_R5G5B5A1_UNORM_PACK16,		PROGRAM_2D_FLOAT		},
		{ VK_FORMAT_R8_UNORM,					PROGRAM_2D_FLOAT		},
		{ VK_FORMAT_R8_SNORM,					PROGRAM_2D_FLOAT_BIAS	},
		{ VK_FORMAT_R8_USCALED,					PROGRAM_2D_UINT			},
		{ VK_FORMAT_R8_SSCALED,					PROGRAM_2D_INT_BIAS		},
		{ VK_FORMAT_R8_UINT,					PROGRAM_2D_UINT			},
		{ VK_FORMAT_R8_SINT,					PROGRAM_2D_INT_BIAS		},
		{ VK_FORMAT_R8_SRGB,					PROGRAM_2D_FLOAT		},
		{ VK_FORMAT_R8G8_UNORM,					PROGRAM_2D_FLOAT		},
		{ VK_FORMAT_R8G8_SNORM,					PROGRAM_2D_FLOAT_BIAS	},
		{ VK_FORMAT_R8G8_USCALED,				PROGRAM_2D_UINT			},
		{ VK_FORMAT_R8G8_SSCALED,				PROGRAM_2D_INT_BIAS		},
		{ VK_FORMAT_R8G8_UINT,					PROGRAM_2D_UINT			},
		{ VK_FORMAT_R8G8_SINT,					PROGRAM_2D_INT_BIAS		},
		{ VK_FORMAT_R8G8_SRGB,					PROGRAM_2D_FLOAT		},
		{ VK_FORMAT_R8G8B8_UNORM,				PROGRAM_2D_FLOAT		},
		{ VK_FORMAT_R8G8B8_SNORM,				PROGRAM_2D_FLOAT_BIAS	},
		{ VK_FORMAT_R8G8B8_USCALED,				PROGRAM_2D_UINT			},
		{ VK_FORMAT_R8G8B8_SSCALED,				PROGRAM_2D_INT_BIAS		},
		{ VK_FORMAT_R8G8B8_UINT,				PROGRAM_2D_UINT			},
		{ VK_FORMAT_R8G8B8_SINT,				PROGRAM_2D_INT_BIAS		},
		{ VK_FORMAT_R8G8B8_SRGB,				PROGRAM_2D_FLOAT		},
		{ VK_FORMAT_R8G8B8A8_UNORM,				PROGRAM_2D_FLOAT		},
		{ VK_FORMAT_R8G8B8A8_SNORM,				PROGRAM_2D_FLOAT_BIAS	},
		{ VK_FORMAT_R8G8B8A8_USCALED,			PROGRAM_2D_UINT			},
		{ VK_FORMAT_R8G8B8A8_SSCALED,			PROGRAM_2D_INT_BIAS		},
		{ VK_FORMAT_R8G8B8A8_UINT,				PROGRAM_2D_UINT			},
		{ VK_FORMAT_R8G8B8A8_SINT,				PROGRAM_2D_INT_BIAS		},
		{ VK_FORMAT_R8G8B8A8_SRGB,				PROGRAM_2D_FLOAT		},
		{ VK_FORMAT_A2R10G10B10_UNORM_PACK32,	PROGRAM_2D_FLOAT		},
		{ VK_FORMAT_A2R10G10B10_UINT_PACK32,	PROGRAM_2D_UINT			},
		{ VK_FORMAT_A2B10G10R10_USCALED_PACK32,	PROGRAM_2D_UINT			},
		{ VK_FORMAT_R16_UNORM,					PROGRAM_2D_FLOAT		},
		{ VK_FORMAT_R16_SNORM,					PROGRAM_2D_FLOAT_BIAS	},
		{ VK_FORMAT_R16_USCALED,				PROGRAM_2D_UINT			},
		{ VK_FORMAT_R16_SSCALED,				PROGRAM_2D_INT_BIAS		},
		{ VK_FORMAT_R16_UINT,					PROGRAM_2D_UINT			},
		{ VK_FORMAT_R16_SINT,					PROGRAM_2D_INT_BIAS		},
		{ VK_FORMAT_R16_SFLOAT,					PROGRAM_2D_FLOAT_BIAS	},
		{ VK_FORMAT_R16G16_UNORM,				PROGRAM_2D_FLOAT		},
		{ VK_FORMAT_R16G16_SNORM,				PROGRAM_2D_FLOAT_BIAS	},
		{ VK_FORMAT_R16G16_USCALED,				PROGRAM_2D_UINT			},
		{ VK_FORMAT_R16G16_SSCALED,				PROGRAM_2D_INT_BIAS		},
		{ VK_FORMAT_R16G16_UINT,				PROGRAM_2D_UINT			},
		{ VK_FORMAT_R16G16_SINT,				PROGRAM_2D_INT_BIAS		},
		{ VK_FORMAT_R16G16_SFLOAT,				PROGRAM_2D_FLOAT_BIAS	},
		{ VK_FORMAT_R16G16B16_UNORM,			PROGRAM_2D_FLOAT		},
		{ VK_FORMAT_R16G16B16_SNORM,			PROGRAM_2D_FLOAT_BIAS	},
		{ VK_FORMAT_R16G16B16_USCALED,			PROGRAM_2D_UINT			},
		{ VK_FORMAT_R16G16B16_SSCALED,			PROGRAM_2D_INT_BIAS		},
		{ VK_FORMAT_R16G16B16_UINT,				PROGRAM_2D_UINT			},
		{ VK_FORMAT_R16G16B16_SINT,				PROGRAM_2D_INT_BIAS		},
		{ VK_FORMAT_R16G16B16_SFLOAT,			PROGRAM_2D_FLOAT_BIAS	},
		{ VK_FORMAT_R16G16B16A16_UNORM,			PROGRAM_2D_FLOAT		},
		{ VK_FORMAT_R16G16B16A16_SNORM,			PROGRAM_2D_FLOAT_BIAS	},
		{ VK_FORMAT_R16G16B16A16_USCALED,		PROGRAM_2D_UINT			},
		{ VK_FORMAT_R16G16B16A16_SSCALED,		PROGRAM_2D_INT_BIAS		},
		{ VK_FORMAT_R16G16B16A16_UINT,			PROGRAM_2D_UINT			},
		{ VK_FORMAT_R16G16B16A16_SINT,			PROGRAM_2D_INT_BIAS		},
		{ VK_FORMAT_R16G16B16A16_SFLOAT,		PROGRAM_2D_FLOAT_BIAS	},
		{ VK_FORMAT_R32_UINT,					PROGRAM_2D_UINT			},
		{ VK_FORMAT_R32_SINT,					PROGRAM_2D_INT_BIAS		},
		{ VK_FORMAT_R32_SFLOAT,					PROGRAM_2D_FLOAT_BIAS	},
		{ VK_FORMAT_R32G32_UINT,				PROGRAM_2D_UINT			},
		{ VK_FORMAT_R32G32_SINT,				PROGRAM_2D_INT_BIAS		},
		{ VK_FORMAT_R32G32_SFLOAT,				PROGRAM_2D_FLOAT_BIAS	},
		{ VK_FORMAT_R32G32B32_UINT,				PROGRAM_2D_UINT			},
		{ VK_FORMAT_R32G32B32_SINT,				PROGRAM_2D_INT_BIAS		},
		{ VK_FORMAT_R32G32B32_SFLOAT,			PROGRAM_2D_FLOAT_BIAS	},
		{ VK_FORMAT_R32G32B32A32_UINT,			PROGRAM_2D_UINT			},
		{ VK_FORMAT_R32G32B32A32_SINT,			PROGRAM_2D_INT_BIAS		},
		{ VK_FORMAT_R32G32B32A32_SFLOAT,		PROGRAM_2D_FLOAT_BIAS	},
		{ VK_FORMAT_B10G11R11_UFLOAT_PACK32,	PROGRAM_2D_FLOAT		},
		{ VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,		PROGRAM_2D_FLOAT		},
		{ VK_FORMAT_B4G4R4A4_UNORM_PACK16,		PROGRAM_2D_FLOAT		},
		{ VK_FORMAT_B5G5R5A1_UNORM_PACK16,		PROGRAM_2D_FLOAT		},

		// Compressed formats
		{ VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK,	PROGRAM_2D_FLOAT		},
		{ VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK,		PROGRAM_2D_FLOAT		},
		{ VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK,	PROGRAM_2D_FLOAT		},
		{ VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK,	PROGRAM_2D_FLOAT		},
		{ VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK,	PROGRAM_2D_FLOAT		},
		{ VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK,	PROGRAM_2D_FLOAT		},
		{ VK_FORMAT_EAC_R11_UNORM_BLOCK,		PROGRAM_2D_FLOAT		},
		{ VK_FORMAT_EAC_R11_SNORM_BLOCK,		PROGRAM_2D_FLOAT_BIAS	},
		{ VK_FORMAT_EAC_R11G11_UNORM_BLOCK,		PROGRAM_2D_FLOAT		},
		{ VK_FORMAT_EAC_R11G11_SNORM_BLOCK,		PROGRAM_2D_FLOAT_BIAS	},
		{ VK_FORMAT_ASTC_4x4_UNORM_BLOCK,		PROGRAM_2D_FLOAT_BIAS	},
		{ VK_FORMAT_ASTC_4x4_SRGB_BLOCK,		PROGRAM_2D_FLOAT		},
		{ VK_FORMAT_ASTC_5x4_UNORM_BLOCK,		PROGRAM_2D_FLOAT_BIAS	},
		{ VK_FORMAT_ASTC_5x4_SRGB_BLOCK,		PROGRAM_2D_FLOAT		},
		{ VK_FORMAT_ASTC_5x5_UNORM_BLOCK,		PROGRAM_2D_FLOAT_BIAS	},
		{ VK_FORMAT_ASTC_5x5_SRGB_BLOCK,		PROGRAM_2D_FLOAT		},
		{ VK_FORMAT_ASTC_6x5_UNORM_BLOCK,		PROGRAM_2D_FLOAT_BIAS	},
		{ VK_FORMAT_ASTC_6x5_SRGB_BLOCK,		PROGRAM_2D_FLOAT		},
		{ VK_FORMAT_ASTC_6x6_UNORM_BLOCK,		PROGRAM_2D_FLOAT_BIAS	},
		{ VK_FORMAT_ASTC_6x6_SRGB_BLOCK,		PROGRAM_2D_FLOAT		},
		{ VK_FORMAT_ASTC_8x5_UNORM_BLOCK,		PROGRAM_2D_FLOAT_BIAS	},
		{ VK_FORMAT_ASTC_8x5_SRGB_BLOCK,		PROGRAM_2D_FLOAT		},
		{ VK_FORMAT_ASTC_8x6_UNORM_BLOCK,		PROGRAM_2D_FLOAT_BIAS	},
		{ VK_FORMAT_ASTC_8x6_SRGB_BLOCK,		PROGRAM_2D_FLOAT		},
		{ VK_FORMAT_ASTC_8x8_UNORM_BLOCK,		PROGRAM_2D_FLOAT_BIAS	},
		{ VK_FORMAT_ASTC_8x8_SRGB_BLOCK,		PROGRAM_2D_FLOAT		},
		{ VK_FORMAT_ASTC_10x5_UNORM_BLOCK,		PROGRAM_2D_FLOAT_BIAS	},
		{ VK_FORMAT_ASTC_10x5_SRGB_BLOCK,		PROGRAM_2D_FLOAT		},
		{ VK_FORMAT_ASTC_10x6_UNORM_BLOCK,		PROGRAM_2D_FLOAT_BIAS	},
		{ VK_FORMAT_ASTC_10x6_SRGB_BLOCK,		PROGRAM_2D_FLOAT		},
		{ VK_FORMAT_ASTC_10x8_UNORM_BLOCK,		PROGRAM_2D_FLOAT_BIAS	},
		{ VK_FORMAT_ASTC_10x8_SRGB_BLOCK,		PROGRAM_2D_FLOAT		},
		{ VK_FORMAT_ASTC_10x10_UNORM_BLOCK,		PROGRAM_2D_FLOAT_BIAS	},
		{ VK_FORMAT_ASTC_10x10_SRGB_BLOCK,		PROGRAM_2D_FLOAT		},
		{ VK_FORMAT_ASTC_12x10_UNORM_BLOCK,		PROGRAM_2D_FLOAT_BIAS	},
		{ VK_FORMAT_ASTC_12x10_SRGB_BLOCK,		PROGRAM_2D_FLOAT		},
		{ VK_FORMAT_ASTC_12x12_UNORM_BLOCK,		PROGRAM_2D_FLOAT_BIAS	},
		{ VK_FORMAT_ASTC_12x12_SRGB_BLOCK,		PROGRAM_2D_FLOAT		}
	};

	static const struct {
		const char*		name;
		const deUint32	width;
		const deUint32	height;
	} sizes2D[] =
	{
		{ "pot",	128,	64,	},
		{ "npot",	51,		65	},
	};

	static const struct {
		const char*								name;
		const TextureBinding::ImageBackingMode	backingMode;
	} backingModes[] =
	{
		{ "",			TextureBinding::IMAGE_BACKING_MODE_REGULAR	},
		{ "_sparse",	TextureBinding::IMAGE_BACKING_MODE_SPARSE	}
	};

	static const struct {
		const char*					name;
		const VkComponentMapping	componentMapping;
	} componentMappings[] =
	{
		{ "zzzz",	{VK_COMPONENT_SWIZZLE_ZERO,	VK_COMPONENT_SWIZZLE_ZERO,	VK_COMPONENT_SWIZZLE_ZERO,	VK_COMPONENT_SWIZZLE_ZERO}	},
		{ "oooo",	{VK_COMPONENT_SWIZZLE_ONE,	VK_COMPONENT_SWIZZLE_ONE,	VK_COMPONENT_SWIZZLE_ONE,	VK_COMPONENT_SWIZZLE_ONE}	},
		{ "rrrr",	{VK_COMPONENT_SWIZZLE_R,	VK_COMPONENT_SWIZZLE_R,		VK_COMPONENT_SWIZZLE_R,		VK_COMPONENT_SWIZZLE_R}		},
		{ "gggg",	{VK_COMPONENT_SWIZZLE_G,	VK_COMPONENT_SWIZZLE_G,		VK_COMPONENT_SWIZZLE_G,		VK_COMPONENT_SWIZZLE_G}		},
		{ "bbbb",	{VK_COMPONENT_SWIZZLE_B,	VK_COMPONENT_SWIZZLE_B,		VK_COMPONENT_SWIZZLE_B,		VK_COMPONENT_SWIZZLE_B}		},
		{ "aaaa",	{VK_COMPONENT_SWIZZLE_A,	VK_COMPONENT_SWIZZLE_A,		VK_COMPONENT_SWIZZLE_A,		VK_COMPONENT_SWIZZLE_A}		}
	};

	de::MovePtr<tcu::TestCaseGroup>	groupCompMap (new tcu::TestCaseGroup(testCtx, "component_mapping", "Component mapping swizzles"));

	// 2D Component mapping swizzles
	for (int sizeNdx = 0; sizeNdx < DE_LENGTH_OF_ARRAY(sizes2D); sizeNdx++)
	for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(formats2D); formatNdx++)
	for (int backingNdx = 0; backingNdx < DE_LENGTH_OF_ARRAY(backingModes); backingNdx++)
	for (int mappingNdx = 0; mappingNdx < DE_LENGTH_OF_ARRAY(componentMappings); mappingNdx++)
	{
		const string formatStr	= de::toString(getFormatStr(formats2D[formatNdx].format));
		const string caseDesc	= formatStr + ", TEXTURETYPE_2D";
		const string caseName	= de::toLower(formatStr.substr(10)) + "_2d"
								+ "_" + sizes2D[sizeNdx].name
								+ backingModes[backingNdx].name
								+ "_" + componentMappings[mappingNdx].name;

		Swizzle2DTestParameters	testParameters;
		testParameters.format			= formats2D[formatNdx].format;
		testParameters.backingMode		= backingModes[backingNdx].backingMode;
		testParameters.componentMapping	= componentMappings[mappingNdx].componentMapping;
		testParameters.width			= sizes2D[sizeNdx].width;
		testParameters.height			= sizes2D[sizeNdx].height;
		testParameters.minFilter		= tcu::Sampler::NEAREST;
		testParameters.magFilter		= tcu::Sampler::NEAREST;
		testParameters.programs.push_back(formats2D[formatNdx].program);

		groupCompMap->addChild(new TextureTestCase<Swizzle2DTestInstance>(testCtx, caseName.c_str(), caseDesc.c_str(), testParameters));
	}

	textureSwizzleTests->addChild(groupCompMap.release());
}

} // anonymous

tcu::TestCaseGroup* createTextureSwizzleTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "swizzle", "Texture swizzle tests.", populateTextureSwizzleTests);
}

} // texture
} // vkt
