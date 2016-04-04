/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Imagination Technologies Ltd.
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
 * \brief Sampler Tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineSamplerTests.hpp"
#include "vktPipelineImageSamplingInstance.hpp"
#include "vktPipelineImageUtil.hpp"
#include "vktPipelineVertexUtil.hpp"
#include "vktTestCase.hpp"
#include "vkImageUtil.hpp"
#include "vkPrograms.hpp"
#include "tcuPlatform.hpp"
#include "tcuTextureUtil.hpp"
#include "deStringUtil.hpp"
#include "deMemory.h"

#include <iomanip>
#include <sstream>
#include <vector>

namespace vkt
{
namespace pipeline
{

using namespace vk;
using de::MovePtr;

namespace
{

class SamplerTest : public vkt::TestCase
{
public:
										SamplerTest				(tcu::TestContext&	testContext,
																 const char*		name,
																 const char*		description,
																 VkImageViewType	imageViewType,
																 VkFormat			imageFormat,
																 int				imageSize,
																 float				samplerLod);
	virtual								~SamplerTest			(void) {}

	virtual void						initPrograms			(SourceCollections& sourceCollections) const;
	virtual TestInstance*				createInstance			(Context& context) const;
	virtual tcu::UVec2					getRenderSize			(VkImageViewType viewType) const;
	virtual std::vector<Vertex4Tex4>	createVertices			(void) const;
	virtual VkSamplerCreateInfo			getSamplerCreateInfo	(void) const;

	static std::string					getGlslSamplerType		(const tcu::TextureFormat& format, VkImageViewType type);
	static tcu::IVec3					getImageSize			(VkImageViewType viewType, int size);
	static int							getArraySize			(VkImageViewType viewType);

protected:
	VkImageViewType						m_imageViewType;
	VkFormat							m_imageFormat;
	int									m_imageSize;
	VkImageViewCreateInfo				m_imageViewParams;
	VkSamplerCreateInfo					m_samplerParams;
	float								m_samplerLod;
};

class SamplerMagFilterTest : public SamplerTest
{
public:
									SamplerMagFilterTest	(tcu::TestContext&	testContext,
															 const char*		name,
															 const char*		description,
															 VkImageViewType	imageViewType,
															 VkFormat			imageFormat,
															 VkFilter			magFilter);
	virtual							~SamplerMagFilterTest	(void) {}
	virtual VkSamplerCreateInfo		getSamplerCreateInfo	(void) const;

private:
	VkFilter						m_magFilter;
};

class SamplerMinFilterTest : public SamplerTest
{
public:
									SamplerMinFilterTest	(tcu::TestContext&	testContext,
															 const char*		name,
															 const char*		description,
															 VkImageViewType	imageViewType,
															 VkFormat			imageFormat,
															 VkFilter			minFilter);
	virtual							~SamplerMinFilterTest	(void) {}
	virtual VkSamplerCreateInfo		getSamplerCreateInfo	(void) const;

private:
	VkFilter						m_minFilter;
};

class SamplerLodTest : public SamplerTest
{
public:
									SamplerLodTest			(tcu::TestContext&		testContext,
															 const char*			name,
															 const char*			description,
															 VkImageViewType		imageViewType,
															 VkFormat				imageFormat,
															 VkSamplerMipmapMode	mipmapMode,
															 float					minLod,
															 float					maxLod,
															 float					mipLodBias,
															 float					samplerLod);
	virtual							~SamplerLodTest			(void) {}
	virtual VkSamplerCreateInfo		getSamplerCreateInfo	(void) const;

private:
	VkSamplerMipmapMode				m_mipmapMode;
	float							m_minLod;
	float							m_maxLod;
	float							m_mipLodBias;
};

class SamplerAddressModesTest : public SamplerTest
{
public:
										SamplerAddressModesTest		(tcu::TestContext&		testContext,
																	 const char*			name,
																	 const char*			description,
																	 VkImageViewType		imageViewType,
																	 VkFormat				imageFormat,
																	 VkSamplerAddressMode	addressU,
																	 VkSamplerAddressMode	addressV,
																	 VkSamplerAddressMode	addressW,
																	 VkBorderColor			borderColor);
	virtual								~SamplerAddressModesTest	(void) {}
	virtual tcu::UVec2					getRenderSize				(VkImageViewType viewType) const;
	virtual std::vector<Vertex4Tex4>	createVertices				(void) const;
	virtual VkSamplerCreateInfo			getSamplerCreateInfo		(void) const;

private:
	VkSamplerAddressMode				m_addressU;
	VkSamplerAddressMode				m_addressV;
	VkSamplerAddressMode				m_addressW;
	VkBorderColor						m_borderColor;
};


// SamplerTest

SamplerTest::SamplerTest (tcu::TestContext&	testContext,
						  const char*		name,
						  const char*		description,
						  VkImageViewType	imageViewType,
						  VkFormat			imageFormat,
						  int				imageSize,
						  float				samplerLod)
	: vkt::TestCase		(testContext, name, description)
	, m_imageViewType	(imageViewType)
	, m_imageFormat		(imageFormat)
	, m_imageSize		(imageSize)
	, m_samplerLod		(samplerLod)
{
}

void SamplerTest::initPrograms (SourceCollections& sourceCollections) const
{
	std::ostringstream				vertexSrc;
	std::ostringstream				fragmentSrc;
	const char*						texCoordSwizzle	= DE_NULL;
	tcu::TextureFormat				format			= (isCompressedFormat(m_imageFormat)) ? tcu::getUncompressedFormat(mapVkCompressedFormat(m_imageFormat))
																						  : mapVkFormat(m_imageFormat);

	// \note We don't want to perform normalization on any compressed formats.
	//		 In case of non-sRGB LDR ASTC it would lead to lack of coverage
	//		 as uncompressed format for that is f16 but values will be in range
	//		 0..1 already.
	const tcu::TextureFormatInfo	formatInfo		= (!isCompressedFormat(m_imageFormat) ? tcu::getTextureFormatInfo(format)
																						  : tcu::getTextureFormatInfo(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8)));

	switch (m_imageViewType)
	{
		case VK_IMAGE_VIEW_TYPE_1D:
			texCoordSwizzle = "x";
			break;
		case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
		case VK_IMAGE_VIEW_TYPE_2D:
			texCoordSwizzle = "xy";
			break;
		case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
		case VK_IMAGE_VIEW_TYPE_3D:
		case VK_IMAGE_VIEW_TYPE_CUBE:
			texCoordSwizzle = "xyz";
			break;
		case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
			texCoordSwizzle = "xyzw";
			break;
		default:
			DE_ASSERT(false);
			break;
	}

	vertexSrc << "#version 440\n"
			  << "layout(location = 0) in vec4 position;\n"
			  << "layout(location = 1) in vec4 texCoords;\n"
			  << "layout(location = 0) out highp vec4 vtxTexCoords;\n"
			  << "out gl_PerVertex {\n"
			  << "	vec4 gl_Position;\n"
			  << "};\n"
			  << "void main (void)\n"
			  << "{\n"
			  << "	gl_Position = position;\n"
			  << "	vtxTexCoords = texCoords;\n"
			  << "}\n";

	fragmentSrc << "#version 440\n"
				<< "layout(set = 0, binding = 0) uniform highp " << getGlslSamplerType(format, m_imageViewType) << " texSampler;\n"
				<< "layout(location = 0) in highp vec4 vtxTexCoords;\n"
				<< "layout(location = 0) out highp vec4 fragColor;\n"
				<< "void main (void)\n"
				<< "{\n"
				<< "	fragColor = ";

	if (m_samplerLod > 0.0f)
		fragmentSrc << "textureLod(texSampler, vtxTexCoords." << texCoordSwizzle << ", " << std::fixed <<  m_samplerLod << ")";
	else
		fragmentSrc << "texture(texSampler, vtxTexCoords." << texCoordSwizzle << ")" << std::fixed;

	fragmentSrc << " * vec4" << std::scientific << formatInfo.lookupScale << " + vec4" << formatInfo.lookupBias << ";\n"
				<< "}\n";

	sourceCollections.glslSources.add("tex_vert") << glu::VertexSource(vertexSrc.str());
	sourceCollections.glslSources.add("tex_frag") << glu::FragmentSource(fragmentSrc.str());
}

TestInstance* SamplerTest::createInstance (Context& context) const
{
	const tcu::UVec2				renderSize			= getRenderSize(m_imageViewType);
	const std::vector<Vertex4Tex4>	vertices			= createVertices();
	const VkSamplerCreateInfo		samplerParams		= getSamplerCreateInfo();
	const VkComponentMapping		componentMapping	= getFormatComponentMapping(m_imageFormat);
	const VkImageSubresourceRange	subresourceRange	=
	{
		VK_IMAGE_ASPECT_COLOR_BIT,								// VkImageAspectFlags	aspectMask;
		0u,														// deUint32				baseMipLevel;
		(deUint32)deLog2Floor32(m_imageSize) + 1,				// deUint32				mipLevels;
		0u,														// deUint32				baseArrayLayer;
		(deUint32)SamplerTest::getArraySize(m_imageViewType)	// deUint32				arraySize;
	};



	return new ImageSamplingInstance(context, renderSize, m_imageViewType, m_imageFormat,
									 getImageSize(m_imageViewType, m_imageSize),
									 getArraySize(m_imageViewType),
									 componentMapping, subresourceRange,
									 samplerParams, m_samplerLod,vertices);
}

tcu::UVec2 SamplerTest::getRenderSize (VkImageViewType viewType) const
{
	if (viewType == VK_IMAGE_VIEW_TYPE_1D || viewType == VK_IMAGE_VIEW_TYPE_2D)
	{
		return tcu::UVec2(16u, 16u);
	}
	else
	{
		return tcu::UVec2(16u * 3u, 16u * 2u);
	}
}

std::vector<Vertex4Tex4> SamplerTest::createVertices (void) const
{
	std::vector<Vertex4Tex4> vertices = createTestQuadMosaic(m_imageViewType);
	// Adjust texture coordinate to avoid doing NEAREST filtering exactly on texel boundaries.
	// TODO: Would be nice to base this on number of texels and subtexel precision. But this
	// seems to work.
	for (unsigned int i = 0; i < vertices.size(); ++i) {
		vertices[i].texCoord += tcu::Vec4(0.002f, 0.002f, 0.002f, 0.0f);
	}
	return vertices;
}

VkSamplerCreateInfo SamplerTest::getSamplerCreateInfo (void) const
{
	const VkSamplerCreateInfo defaultSamplerParams =
	{
		VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,									// VkStructureType			sType;
		DE_NULL,																// const void*				pNext;
		0u,																		// VkSamplerCreateFlags		flags;
		VK_FILTER_NEAREST,														// VkFilter					magFilter;
		VK_FILTER_NEAREST,														// VkFilter					minFilter;
		VK_SAMPLER_MIPMAP_MODE_NEAREST,											// VkSamplerMipmapMode		mipmapMode;
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,									// VkSamplerAddressMode		addressModeU;
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,									// VkSamplerAddressMode		addressModeV;
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,									// VkSamplerAddressMode		addressModeW;
		0.0f,																	// float					mipLodBias;
		VK_FALSE,																// VkBool32					anisotropyEnable;
		1.0f,																	// float					maxAnisotropy;
		false,																	// VkBool32					compareEnable;
		VK_COMPARE_OP_NEVER,													// VkCompareOp				compareOp;
		0.0f,																	// float					minLod;
		0.25f,																	// float					maxLod;
		getFormatBorderColor(BORDER_COLOR_TRANSPARENT_BLACK, m_imageFormat),	// VkBorderColor			borderColor;
		false																	// VkBool32					unnormalizedCoordinates;
	};

	return defaultSamplerParams;
}

std::string SamplerTest::getGlslSamplerType (const tcu::TextureFormat& format, VkImageViewType type)
{
	std::ostringstream samplerType;

	if (tcu::getTextureChannelClass(format.type) == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER)
		samplerType << "u";
	else if (tcu::getTextureChannelClass(format.type) == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER)
		samplerType << "i";

	switch (type)
	{
		case VK_IMAGE_VIEW_TYPE_1D:
			samplerType << "sampler1D";
			break;

		case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
			samplerType << "sampler1DArray";
			break;

		case VK_IMAGE_VIEW_TYPE_2D:
			samplerType << "sampler2D";
			break;

		case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
			samplerType << "sampler2DArray";
			break;

		case VK_IMAGE_VIEW_TYPE_3D:
			samplerType << "sampler3D";
			break;

		case VK_IMAGE_VIEW_TYPE_CUBE:
			samplerType << "samplerCube";
			break;

		case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
			samplerType << "samplerCubeArray";
			break;

		default:
			DE_FATAL("Unknown image view type");
			break;
	}

	return samplerType.str();
}

tcu::IVec3 SamplerTest::getImageSize (VkImageViewType viewType, int size)
{
	switch (viewType)
	{
		case VK_IMAGE_VIEW_TYPE_1D:
		case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
			return tcu::IVec3(size, 1, 1);

		case VK_IMAGE_VIEW_TYPE_3D:
			return tcu::IVec3(size, size, 4);

		default:
			break;
	}

	return tcu::IVec3(size, size, 1);
}

int SamplerTest::getArraySize (VkImageViewType viewType)
{
	switch (viewType)
	{
		case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
		case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
		case VK_IMAGE_VIEW_TYPE_CUBE:
			return 6;

		case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
			return 36;

		default:
			break;
	}

	return 1;
}


// SamplerMagFilterTest

SamplerMagFilterTest::SamplerMagFilterTest (tcu::TestContext&	testContext,
											const char*			name,
											const char*			description,
											VkImageViewType		imageViewType,
											VkFormat			imageFormat,
											VkFilter			magFilter)
	: SamplerTest	(testContext, name, description, imageViewType, imageFormat, 8, 0.0f)
	, m_magFilter	(magFilter)
{
}

VkSamplerCreateInfo SamplerMagFilterTest::getSamplerCreateInfo (void) const
{
	VkSamplerCreateInfo samplerParams = SamplerTest::getSamplerCreateInfo();
	samplerParams.magFilter = m_magFilter;

	return samplerParams;
}


// SamplerMinFilterTest

SamplerMinFilterTest::SamplerMinFilterTest (tcu::TestContext&	testContext,
											const char*			name,
											const char*			description,
											VkImageViewType		imageViewType,
											VkFormat			imageFormat,
											VkFilter			minFilter)
	: SamplerTest	(testContext, name, description, imageViewType, imageFormat, 32, 0.0f)
	, m_minFilter	(minFilter)
{
}

VkSamplerCreateInfo SamplerMinFilterTest::getSamplerCreateInfo (void) const
{
	VkSamplerCreateInfo samplerParams = SamplerTest::getSamplerCreateInfo();
	samplerParams.minFilter = m_minFilter;
	// set minLod to epsilon, to force use of the minFilter
	samplerParams.minLod = 0.01f;

	return samplerParams;
}


// SamplerLodTest

SamplerLodTest::SamplerLodTest (tcu::TestContext&	testContext,
								const char*			name,
								const char*			description,
								VkImageViewType		imageViewType,
								VkFormat			imageFormat,
								VkSamplerMipmapMode	mipmapMode,
								float				minLod,
								float				maxLod,
								float				mipLodBias,
								float				samplerLod)
	: SamplerTest	(testContext, name, description, imageViewType, imageFormat, 32, samplerLod)
	, m_mipmapMode	(mipmapMode)
	, m_minLod		(minLod)
	, m_maxLod		(maxLod)
	, m_mipLodBias	(mipLodBias)
{
}

VkSamplerCreateInfo SamplerLodTest::getSamplerCreateInfo (void) const
{
	VkSamplerCreateInfo samplerParams = SamplerTest::getSamplerCreateInfo();

	samplerParams.mipmapMode	= m_mipmapMode;
	samplerParams.minLod		= m_minLod;
	samplerParams.maxLod		= m_maxLod;
	samplerParams.mipLodBias	= m_mipLodBias;

	return samplerParams;
}


// SamplerAddressModesTest

SamplerAddressModesTest::SamplerAddressModesTest (tcu::TestContext&		testContext,
												  const char*			name,
												  const char*			description,
												  VkImageViewType		imageViewType,
												  VkFormat				imageFormat,
												  VkSamplerAddressMode	addressU,
												  VkSamplerAddressMode	addressV,
												  VkSamplerAddressMode	addressW,
												  VkBorderColor			borderColor)
	: SamplerTest	(testContext, name, description, imageViewType, imageFormat, 8, 0.0f)
	, m_addressU	(addressU)
	, m_addressV	(addressV)
	, m_addressW	(addressW)
	, m_borderColor	(borderColor)
{
}

tcu::UVec2 SamplerAddressModesTest::getRenderSize (VkImageViewType viewType) const
{
	return 4u * SamplerTest::getRenderSize(viewType);
}

std::vector<Vertex4Tex4> SamplerAddressModesTest::createVertices (void) const
{
	std::vector<Vertex4Tex4> vertices = SamplerTest::createVertices();

	switch (m_imageViewType)
	{
		case VK_IMAGE_VIEW_TYPE_1D: case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
			for (size_t vertexNdx = 0; vertexNdx < vertices.size(); vertexNdx++)
				vertices[vertexNdx].texCoord.x() = (vertices[vertexNdx].texCoord.x() - 0.5f) * 4.0f;

			break;

		case VK_IMAGE_VIEW_TYPE_2D:
		case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
			for (size_t vertexNdx = 0; vertexNdx < vertices.size(); vertexNdx++)
				vertices[vertexNdx].texCoord.xy() = (vertices[vertexNdx].texCoord.swizzle(0, 1) - tcu::Vec2(0.5f)) * 4.0f;

			break;

		case VK_IMAGE_VIEW_TYPE_3D:
			for (size_t vertexNdx = 0; vertexNdx < vertices.size(); vertexNdx++)
				vertices[vertexNdx].texCoord.xyz() = (vertices[vertexNdx].texCoord.swizzle(0, 1, 2) - tcu::Vec3(0.5f)) * 4.0f;

			break;

		case VK_IMAGE_VIEW_TYPE_CUBE:
		case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
			break;

		default:
			DE_ASSERT(false);
	}

	return vertices;
}

VkSamplerCreateInfo SamplerAddressModesTest::getSamplerCreateInfo (void) const
{
	VkSamplerCreateInfo samplerParams = SamplerTest::getSamplerCreateInfo();
	samplerParams.addressModeU	= m_addressU;
	samplerParams.addressModeV	= m_addressV;
	samplerParams.addressModeW	= m_addressW;
	samplerParams.borderColor	= m_borderColor;

	return samplerParams;
}


// Utilities to create test nodes

std::string getFormatCaseName (const VkFormat format)
{
	const std::string fullName = getFormatName(format);

	DE_ASSERT(de::beginsWith(fullName, "VK_FORMAT_"));

	return de::toLower(fullName.substr(10));
}

MovePtr<tcu::TestCaseGroup> createSamplerMagFilterTests (tcu::TestContext& testCtx, VkImageViewType imageViewType, VkFormat imageFormat)
{
	MovePtr<tcu::TestCaseGroup> samplerMagFilterTests (new tcu::TestCaseGroup(testCtx, "mag_filter", "Tests for magnification filter"));

	if (isCompressedFormat(imageFormat) || (!isIntFormat(imageFormat) && !isUintFormat(imageFormat)))
		samplerMagFilterTests->addChild(new SamplerMagFilterTest(testCtx, "linear", "Magnifies image using VK_TEX_FILTER_LINEAR", imageViewType, imageFormat, VK_FILTER_LINEAR));
	samplerMagFilterTests->addChild(new SamplerMagFilterTest(testCtx, "nearest", "Magnifies image using VK_TEX_FILTER_NEAREST", imageViewType, imageFormat, VK_FILTER_NEAREST));

	return samplerMagFilterTests;
}

MovePtr<tcu::TestCaseGroup> createSamplerMinFilterTests (tcu::TestContext& testCtx, VkImageViewType imageViewType, VkFormat imageFormat)
{
	MovePtr<tcu::TestCaseGroup> samplerMinFilterTests (new tcu::TestCaseGroup(testCtx, "min_filter", "Tests for minification filter"));

	if (isCompressedFormat(imageFormat) || (!isIntFormat(imageFormat) && !isUintFormat(imageFormat)))
		samplerMinFilterTests->addChild(new SamplerMinFilterTest(testCtx, "linear", "Minifies image using VK_TEX_FILTER_LINEAR", imageViewType, imageFormat, VK_FILTER_LINEAR));
	samplerMinFilterTests->addChild(new SamplerMinFilterTest(testCtx, "nearest", "Minifies image using VK_TEX_FILTER_NEAREST", imageViewType, imageFormat, VK_FILTER_NEAREST));

	return samplerMinFilterTests;
}

MovePtr<tcu::TestCaseGroup> createSamplerLodTests (tcu::TestContext& testCtx, VkImageViewType imageViewType, VkFormat imageFormat, VkSamplerMipmapMode mipmapMode)
{
	struct TestCaseConfig
	{
		const char*	name;
		const char*	description;
		float		minLod;
		float		maxLod;
		float		mipLodBias;
		float		lod;
	};

	TestCaseConfig testCaseConfigs [] =
	{
		{ "equal_min_3_max_3",		"minLod = 3, maxLod = 3, mipLodBias = 0, lod = 0",		3.0f, 3.0f, 0.0f, 0.0f },
		{ "select_min_1",			"minLod = 1, maxLod = 5, mipLodBias = 0, lod = 0",		1.0f, 5.0f, 0.0f, 0.0f },
		{ "select_max_4",			"minLod = 0, maxLod = 4, mipLodBias = 0, lod = 5",		0.0f, 4.0f, 0.0f, 5.0f },
		{ "select_bias_2_1",		"minLod = 0, maxLod = 2.1, mipLodBias = 5.0, lod = 0",	0.0f, 2.1f, 5.0f, 0.0f },
		{ "select_bias_2_5",		"minLod = 0, maxLod = 5, mipLodBias = 2.5, lod = 0",	0.0f, 5.0f, 2.5f, 0.00001f },
		{ "select_bias_3_1",		"minLod = 0, maxLod = 5, mipLodBias = -0.9, lod = 4.0",	0.0f, 5.0f, -0.9f, 4.0f },
		{ "select_bias_3_7",		"minLod = 0, maxLod = 5, mipLodBias = 3.0, lod = 0.7",	0.0f, 5.0f, 3.0f, 0.7f },
	};

	MovePtr<tcu::TestCaseGroup> samplerLodTests (new tcu::TestCaseGroup(testCtx, "lod", "Tests for sampler LOD"));

	for (int configNdx = 0; configNdx < DE_LENGTH_OF_ARRAY(testCaseConfigs); configNdx++)
	{
		const TestCaseConfig& config = testCaseConfigs[configNdx];

		samplerLodTests->addChild(new SamplerLodTest(testCtx, config.name, config.description, imageViewType, imageFormat, mipmapMode, config.minLod, config.maxLod, config.mipLodBias, config.lod));
	}

	return samplerLodTests;
}

MovePtr<tcu::TestCaseGroup> createSamplerMipmapTests (tcu::TestContext& testCtx, VkImageViewType imageViewType, VkFormat imageFormat)
{
	MovePtr<tcu::TestCaseGroup> samplerMipmapTests (new tcu::TestCaseGroup(testCtx, "mipmap", "Tests for mipmap modes"));

	// Mipmap mode: nearest
	MovePtr<tcu::TestCaseGroup> mipmapNearestTests (new tcu::TestCaseGroup(testCtx, "nearest", "Uses VK_TEX_MIPMAP_MODE_NEAREST"));
	mipmapNearestTests->addChild(createSamplerLodTests(testCtx, imageViewType, imageFormat, VK_SAMPLER_MIPMAP_MODE_NEAREST).release());
	samplerMipmapTests->addChild(mipmapNearestTests.release());

	// Mipmap mode: linear
	if (isCompressedFormat(imageFormat) || (!isIntFormat(imageFormat) && !isUintFormat(imageFormat)))
	{
		MovePtr<tcu::TestCaseGroup> mipmapLinearTests(new tcu::TestCaseGroup(testCtx, "linear", "Uses VK_TEX_MIPMAP_MODE_LINEAR"));
		mipmapLinearTests->addChild(createSamplerLodTests(testCtx, imageViewType, imageFormat, VK_SAMPLER_MIPMAP_MODE_LINEAR).release());
		samplerMipmapTests->addChild(mipmapLinearTests.release());
	}

	return samplerMipmapTests;
}

std::string getAddressModesCaseName (VkSamplerAddressMode u, VkSamplerAddressMode v, VkSamplerAddressMode w, BorderColor border)
{
	static const char* borderColorNames[BORDER_COLOR_COUNT] =
	{
		"opaque_black",
		"opaque_white",
		"transparent_black",
	};

	std::ostringstream caseName;

	if (u == v && v == w)
	{
		const std::string fullName = getSamplerAddressModeName(u);
		DE_ASSERT(de::beginsWith(fullName, "VK_SAMPLER_ADDRESS_"));

		caseName << "all_";
		caseName << de::toLower(fullName.substr(19));

		if (u == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER)
		{
			caseName << "_" << borderColorNames[border];
		}
	}
	else
	{
		const std::string fullNameU = getSamplerAddressModeName(u);
		const std::string fullNameV = getSamplerAddressModeName(v);
		const std::string fullNameW = getSamplerAddressModeName(w);

		DE_ASSERT(de::beginsWith(fullNameU, "VK_SAMPLER_ADDRESS_"));
		DE_ASSERT(de::beginsWith(fullNameV, "VK_SAMPLER_ADDRESS_"));
		DE_ASSERT(de::beginsWith(fullNameW, "VK_SAMPLER_ADDRESS_"));

		caseName << "uvw"
				 << "_" << de::toLower(fullNameU.substr(19))
				 << "_" << de::toLower(fullNameV.substr(19))
				 << "_" << de::toLower(fullNameW.substr(19));
	}

	return caseName.str();
}

MovePtr<tcu::TestCaseGroup> createSamplerAddressModesTests (tcu::TestContext& testCtx, VkImageViewType imageViewType, VkFormat imageFormat)
{
	struct TestCaseConfig
	{
		VkSamplerAddressMode	u;
		VkSamplerAddressMode	v;
		VkSamplerAddressMode	w;
		BorderColor				border;
	};

	const TestCaseConfig testCaseConfigs[] =
	{
		// All address modes equal
		{ VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,			BORDER_COLOR_TRANSPARENT_BLACK },
		{ VK_SAMPLER_ADDRESS_MODE_REPEAT,				VK_SAMPLER_ADDRESS_MODE_REPEAT,					VK_SAMPLER_ADDRESS_MODE_REPEAT,					BORDER_COLOR_TRANSPARENT_BLACK },
		{ VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,		VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,		VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,		BORDER_COLOR_TRANSPARENT_BLACK },
		{ VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,	VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,	VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,	BORDER_COLOR_TRANSPARENT_BLACK },

		// All address modes equal using border color
		{ VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,		BORDER_COLOR_TRANSPARENT_BLACK },
		{ VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,		BORDER_COLOR_OPAQUE_BLACK },
		{ VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,		BORDER_COLOR_OPAQUE_WHITE },

		// Pairwise combinations of address modes not covered by previous tests
		{ VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,		VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,	VK_SAMPLER_ADDRESS_MODE_REPEAT,					BORDER_COLOR_OPAQUE_WHITE},
		{ VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,		VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,		VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,	BORDER_COLOR_OPAQUE_WHITE },
		{ VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,		VK_SAMPLER_ADDRESS_MODE_REPEAT,					VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,		BORDER_COLOR_OPAQUE_WHITE },
		{ VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,			BORDER_COLOR_OPAQUE_WHITE },
		{ VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,	VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,		VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,		BORDER_COLOR_OPAQUE_WHITE },
		{ VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,	VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,	VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,		BORDER_COLOR_OPAQUE_WHITE },
		{ VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,	VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,		VK_SAMPLER_ADDRESS_MODE_REPEAT,					BORDER_COLOR_OPAQUE_WHITE },
		{ VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,	VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,			VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,	BORDER_COLOR_OPAQUE_WHITE },
		{ VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,	VK_SAMPLER_ADDRESS_MODE_REPEAT,					VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,			BORDER_COLOR_OPAQUE_WHITE },
		{ VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,		VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,	BORDER_COLOR_OPAQUE_WHITE },
		{ VK_SAMPLER_ADDRESS_MODE_REPEAT,				VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,	VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,	BORDER_COLOR_OPAQUE_WHITE },
		{ VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,		VK_SAMPLER_ADDRESS_MODE_REPEAT,					VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,		BORDER_COLOR_OPAQUE_WHITE },
		{ VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,			VK_SAMPLER_ADDRESS_MODE_REPEAT,					BORDER_COLOR_OPAQUE_WHITE },
		{ VK_SAMPLER_ADDRESS_MODE_REPEAT,				VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,			BORDER_COLOR_OPAQUE_WHITE },
		{ VK_SAMPLER_ADDRESS_MODE_REPEAT,				VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,		BORDER_COLOR_OPAQUE_WHITE },
		{ VK_SAMPLER_ADDRESS_MODE_REPEAT,				VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,			VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,		BORDER_COLOR_OPAQUE_WHITE },
		{ VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,		VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,	VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,			BORDER_COLOR_OPAQUE_WHITE },
		{ VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,		BORDER_COLOR_OPAQUE_WHITE },
		{ VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,		VK_SAMPLER_ADDRESS_MODE_REPEAT,					BORDER_COLOR_OPAQUE_WHITE },
		{ VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		VK_SAMPLER_ADDRESS_MODE_REPEAT,					VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,	BORDER_COLOR_OPAQUE_WHITE },
		{ VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,			BORDER_COLOR_OPAQUE_WHITE },
		{ VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,	VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,		BORDER_COLOR_OPAQUE_WHITE },
	};

	MovePtr<tcu::TestCaseGroup> samplerAddressModesTests (new tcu::TestCaseGroup(testCtx, "address_modes", "Tests for address modes"));

	for (int configNdx = 0; configNdx < DE_LENGTH_OF_ARRAY(testCaseConfigs); configNdx++)
	{
		const TestCaseConfig& config = testCaseConfigs[configNdx];

		samplerAddressModesTests->addChild(new SamplerAddressModesTest(testCtx,
																	   getAddressModesCaseName(config.u, config.v, config.w, config.border).c_str(),
																	   "",
																	   imageViewType,
																	   imageFormat,
																	   config.u, config.v, config.w,
																	   getFormatBorderColor(config.border, imageFormat)));
	}

	return samplerAddressModesTests;
}

} // anonymous

tcu::TestCaseGroup* createSamplerTests (tcu::TestContext& testCtx)
{
	const struct
	{
		VkImageViewType		type;
		const char*			name;
	}
	imageViewTypes[] =
	{
		{ VK_IMAGE_VIEW_TYPE_1D,			"1d" },
		{ VK_IMAGE_VIEW_TYPE_1D_ARRAY,		"1d_array" },
		{ VK_IMAGE_VIEW_TYPE_2D,			"2d" },
		{ VK_IMAGE_VIEW_TYPE_2D_ARRAY,		"2d_array" },
		{ VK_IMAGE_VIEW_TYPE_3D,			"3d" },
		{ VK_IMAGE_VIEW_TYPE_CUBE,			"cube" },
		{ VK_IMAGE_VIEW_TYPE_CUBE_ARRAY,	"cube_array" }
	};

	const VkFormat formats[] =
	{
		// Packed formats
		VK_FORMAT_R4G4_UNORM_PACK8,
		VK_FORMAT_R4G4B4A4_UNORM_PACK16,
		VK_FORMAT_R5G6B5_UNORM_PACK16,
		VK_FORMAT_R5G5B5A1_UNORM_PACK16,
		VK_FORMAT_A2B10G10R10_UNORM_PACK32,
		VK_FORMAT_A2R10G10B10_UINT_PACK32,
		VK_FORMAT_B10G11R11_UFLOAT_PACK32,
		VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,
		VK_FORMAT_B4G4R4A4_UNORM_PACK16,
		VK_FORMAT_B5G5R5A1_UNORM_PACK16,

		// Pairwise combinations of 8-bit channel formats, UNORM/SNORM/SINT/UINT/SRGB type x 1-to-4 channels x RGBA/BGRA order
		VK_FORMAT_R8_SRGB,
		VK_FORMAT_R8G8B8_UINT,
		VK_FORMAT_B8G8R8A8_SINT,
		VK_FORMAT_R8G8_UNORM,
		VK_FORMAT_B8G8R8_SNORM,
		VK_FORMAT_R8G8B8A8_SNORM,
		VK_FORMAT_R8G8_UINT,
		VK_FORMAT_R8_SINT,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_FORMAT_B8G8R8_SRGB,
		VK_FORMAT_R8G8_SRGB,
		VK_FORMAT_R8_UINT,
		VK_FORMAT_R8G8B8A8_UINT,
		VK_FORMAT_R8G8_SINT,
		VK_FORMAT_R8_SNORM,
		VK_FORMAT_B8G8R8_SINT,
		VK_FORMAT_R8G8_SNORM,
		VK_FORMAT_B8G8R8_UNORM,
		VK_FORMAT_R8_UNORM,

		// Pairwise combinations of 16/32-bit channel formats x SINT/UINT/SFLOAT type x 1-to-4 channels
		VK_FORMAT_R32G32_SFLOAT,
		VK_FORMAT_R32G32B32_UINT,
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_FORMAT_R16G16_UINT,
		VK_FORMAT_R32G32B32A32_SINT,
		VK_FORMAT_R16G16B16_SINT,
		VK_FORMAT_R16_SFLOAT,
		VK_FORMAT_R32_SINT,
		VK_FORMAT_R32_UINT,
		VK_FORMAT_R16G16B16_SFLOAT,
		VK_FORMAT_R16G16_SINT,

		// Scaled formats
		VK_FORMAT_R8G8B8A8_SSCALED,
		VK_FORMAT_A2R10G10B10_USCALED_PACK32,

		// Compressed formats
		VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK,
		VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK,
		VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK,
		VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK,
		VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK,
		VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK,
		VK_FORMAT_EAC_R11_UNORM_BLOCK,
		VK_FORMAT_EAC_R11_SNORM_BLOCK,
		VK_FORMAT_EAC_R11G11_UNORM_BLOCK,
		VK_FORMAT_EAC_R11G11_SNORM_BLOCK,
		VK_FORMAT_ASTC_4x4_UNORM_BLOCK,
		VK_FORMAT_ASTC_5x4_SRGB_BLOCK,
		VK_FORMAT_ASTC_6x5_UNORM_BLOCK,
		VK_FORMAT_ASTC_6x6_SRGB_BLOCK,
		VK_FORMAT_ASTC_8x6_UNORM_BLOCK,
		VK_FORMAT_ASTC_8x8_SRGB_BLOCK,
		VK_FORMAT_ASTC_10x6_UNORM_BLOCK,
		VK_FORMAT_ASTC_10x8_SRGB_BLOCK,
		VK_FORMAT_ASTC_12x10_UNORM_BLOCK,
		VK_FORMAT_ASTC_12x12_SRGB_BLOCK,
	};

	de::MovePtr<tcu::TestCaseGroup> samplerTests		(new tcu::TestCaseGroup(testCtx, "sampler", "Sampler tests"));
	de::MovePtr<tcu::TestCaseGroup> viewTypeTests		(new tcu::TestCaseGroup(testCtx, "view_type", ""));

	for (int viewTypeNdx = 0; viewTypeNdx < DE_LENGTH_OF_ARRAY(imageViewTypes); viewTypeNdx++)
	{
		const VkImageViewType			viewType		= imageViewTypes[viewTypeNdx].type;
		de::MovePtr<tcu::TestCaseGroup>	viewTypeGroup	(new tcu::TestCaseGroup(testCtx, imageViewTypes[viewTypeNdx].name, (std::string("Uses a ") + imageViewTypes[viewTypeNdx].name + " view").c_str()));
		de::MovePtr<tcu::TestCaseGroup>	formatTests		(new tcu::TestCaseGroup(testCtx, "format", "Tests samplable formats"));

		for (size_t formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(formats); formatNdx++)
		{
			const VkFormat	format			= formats[formatNdx];
			const bool		isCompressed	= isCompressedFormat(format);

			if (isCompressed)
			{
				// Do not use compressed formats with 1D and 1D array textures.
				if (viewType == VK_IMAGE_VIEW_TYPE_1D || viewType == VK_IMAGE_VIEW_TYPE_1D_ARRAY)
					break;
			}

			de::MovePtr<tcu::TestCaseGroup>	formatGroup	(new tcu::TestCaseGroup(testCtx,
																				getFormatCaseName(format).c_str(),
																				(std::string("Samples a texture of format ") + getFormatName(format)).c_str()));

			if (!isCompressed)
			{
				// Do not include minFilter tests with compressed formats.
				// Randomly generated compressed textures are too noisy and will derive in false positives.
				de::MovePtr<tcu::TestCaseGroup>	minFilterTests		= createSamplerMinFilterTests(testCtx, viewType, format);
				formatGroup->addChild(minFilterTests.release());
			}

			de::MovePtr<tcu::TestCaseGroup>	magFilterTests		= createSamplerMagFilterTests(testCtx, viewType, format);
			de::MovePtr<tcu::TestCaseGroup>	mipmapTests			= createSamplerMipmapTests(testCtx, viewType, format);

			formatGroup->addChild(magFilterTests.release());
			formatGroup->addChild(mipmapTests.release());

			if (viewType != VK_IMAGE_VIEW_TYPE_CUBE && viewType != VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)
			{
				de::MovePtr<tcu::TestCaseGroup>	addressModesTests	= createSamplerAddressModesTests(testCtx, viewType, format);
				formatGroup->addChild(addressModesTests.release());
			}

			formatTests->addChild(formatGroup.release());
		}

		viewTypeGroup->addChild(formatTests.release());
		viewTypeTests->addChild(viewTypeGroup.release());
	}

	samplerTests->addChild(viewTypeTests.release());

	return samplerTests.release();
}

} // pipeline
} // vkt
