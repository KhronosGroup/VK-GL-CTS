/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 * Copyright 2014 The Android Open Source Project
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
 * \brief Mipmapping tests.
 *//*--------------------------------------------------------------------*/

#include "vktTextureMipmapTests.hpp"

#include "deRandom.hpp"
#include "deString.h"
#include "gluShaderUtil.hpp"
#include "gluTextureTestUtil.hpp"
#include "tcuMatrix.hpp"
#include "tcuMatrixUtil.hpp"
#include "tcuPixelFormat.hpp"
#include "tcuTexLookupVerifier.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuCommandLine.hpp"
#include "vkImageUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTextureTestUtil.hpp"
#include "vktCustomInstancesDevices.hpp"

#include <memory>

using namespace vk;

namespace vkt
{
namespace texture
{
namespace
{

using std::string;
using std::vector;
using tcu::TestLog;
using tcu::Vec2;
using tcu::Vec3;
using tcu::Vec4;
using tcu::IVec3;
using tcu::IVec4;
using tcu::Sampler;
using tcu::TextureFormat;
using namespace texture::util;
using namespace glu::TextureTestUtil;

float getMinLodForCell (int cellNdx)
{
	static const float s_values[] =
	{
		1.0f,
		3.5f,
		2.0f,
		-2.0f,
		0.0f,
		3.0f,
		10.0f,
		4.8f,
		5.8f,
		5.7f,
		-1.9f,
		4.0f,
		6.5f,
		7.1f,
		-1e10,
		1000.f
	};
	return s_values[cellNdx % DE_LENGTH_OF_ARRAY(s_values)];
}

float getMaxLodForCell (int cellNdx)
{
	static const float s_values[] =
	{
		0.0f,
		0.2f,
		0.7f,
		0.4f,
		1.3f,
		0.0f,
		0.5f,
		1.2f,
		-2.0f,
		1.0f,
		0.1f,
		0.3f,
		2.7f,
		1.2f,
		10.0f,
		-1000.f,
		1e10f
	};
	return s_values[cellNdx % DE_LENGTH_OF_ARRAY(s_values)];
}

enum CoordType
{
	COORDTYPE_BASIC,		//!< texCoord = translateScale(position).
	COORDTYPE_BASIC_BIAS,	//!< Like basic, but with bias values.
	COORDTYPE_AFFINE,		//!< texCoord = translateScaleRotateShear(position).
	COORDTYPE_PROJECTED,	//!< Projected coordinates, w != 1

	COORDTYPE_LAST
};

struct TextureMipmapCommonTestCaseParameters
{
							TextureMipmapCommonTestCaseParameters		(void);
	CoordType				coordType;
	const char*				minFilterName;
};

TextureMipmapCommonTestCaseParameters::TextureMipmapCommonTestCaseParameters (void)
	: coordType						(COORDTYPE_BASIC)
	, minFilterName					(NULL)
{
}

struct Texture2DMipmapTestCaseParameters : public Texture2DTestCaseParameters, public TextureMipmapCommonTestCaseParameters
{
};

struct TextureCubeMipmapTestCaseParameters : public TextureCubeTestCaseParameters, public TextureMipmapCommonTestCaseParameters
{
};

struct Texture3DMipmapTestCaseParameters : public Texture3DTestCaseParameters, public TextureMipmapCommonTestCaseParameters
{
};

// Texture2DMipmapTestInstance
class Texture2DMipmapTestInstance : public TestInstance
{
public:
	typedef Texture2DMipmapTestCaseParameters	ParameterType;

									Texture2DMipmapTestInstance		(Context& context, const ParameterType& testParameters);
									~Texture2DMipmapTestInstance	(void);

	virtual tcu::TestStatus			iterate							(void);

private:
									Texture2DMipmapTestInstance		(const Texture2DMipmapTestInstance& other);
	Texture2DMipmapTestInstance&	operator=						(const Texture2DMipmapTestInstance& other);

	const ParameterType				m_testParameters;
	TestTexture2DSp					m_texture;
	TextureRenderer					m_renderer;
};

Texture2DMipmapTestInstance::Texture2DMipmapTestInstance (Context& context, const Texture2DMipmapTestCaseParameters& testParameters)
	: TestInstance		(context)
	, m_testParameters	(testParameters)
	, m_renderer		(context, testParameters.sampleCount, testParameters.width*4, testParameters.height*4)
{
	TCU_CHECK_INTERNAL(!(m_testParameters.coordType == COORDTYPE_PROJECTED && m_testParameters.sampleCount != VK_SAMPLE_COUNT_1_BIT));

	m_texture = TestTexture2DSp(new pipeline::TestTexture2D(vk::mapVkFormat(m_testParameters.format), m_testParameters.width, m_testParameters.height));

	const int numLevels = deLog2Floor32(de::max(m_testParameters.width, m_testParameters.height))+1;

	// Fill texture with colored grid.
	for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
	{
		const deUint32	step	= 0xff / (numLevels-1);
		const deUint32	inc		= deClamp32(step*levelNdx, 0x00, 0xff);
		const deUint32	dec		= 0xff - inc;
		const deUint32	rgb		= (inc << 16) | (dec << 8) | 0xff;
		const deUint32	color	= 0xff000000 | rgb;

		tcu::clear(m_texture->getLevel(levelNdx, 0), tcu::RGBA(color).toVec());
	}

	// Upload texture data.
	m_renderer.add2DTexture(m_texture, testParameters.aspectMask);
}

Texture2DMipmapTestInstance::~Texture2DMipmapTestInstance (void)
{
}

static void getBasicTexCoord2D (std::vector<float>& dst, int cellNdx)
{
	static const struct
	{
		const Vec2	bottomLeft;
		const Vec2	topRight;
	} s_basicCoords[] =
	{
		{ Vec2(-0.1f,  0.1f), Vec2( 0.8f,  1.0f) },
		{ Vec2(-0.3f, -0.6f), Vec2( 0.7f,  0.4f) },
		{ Vec2(-0.3f,  0.6f), Vec2( 0.7f, -0.9f) },
		{ Vec2(-0.8f,  0.6f), Vec2( 0.7f, -0.9f) },

		{ Vec2(-0.5f, -0.5f), Vec2( 1.5f,  1.5f) },
		{ Vec2( 1.0f, -1.0f), Vec2(-1.3f,  1.0f) },
		{ Vec2( 1.2f, -1.0f), Vec2(-1.3f,  1.6f) },
		{ Vec2( 2.2f, -1.1f), Vec2(-1.3f,  0.8f) },

		{ Vec2(-1.5f,  1.6f), Vec2( 1.7f, -1.4f) },
		{ Vec2( 2.0f,  1.6f), Vec2( 2.3f, -1.4f) },
		{ Vec2( 1.3f, -2.6f), Vec2(-2.7f,  2.9f) },
		{ Vec2(-0.8f, -6.6f), Vec2( 6.0f, -0.9f) },

		{ Vec2( -8.0f,   9.0f), Vec2(  8.3f,  -7.0f) },
		{ Vec2(-16.0f,  10.0f), Vec2( 18.3f,  24.0f) },
		{ Vec2( 30.2f,  55.0f), Vec2(-24.3f,  -1.6f) },
		{ Vec2(-33.2f,  64.1f), Vec2( 32.1f, -64.1f) },
	};

	DE_ASSERT(de::inBounds(cellNdx, 0, DE_LENGTH_OF_ARRAY(s_basicCoords)));

	const Vec2& bottomLeft	= s_basicCoords[cellNdx].bottomLeft;
	const Vec2& topRight	= s_basicCoords[cellNdx].topRight;

	computeQuadTexCoord2D(dst, bottomLeft, topRight);
}

static void getBasicTexCoord2DImageViewMinLodIntTexCoord (std::vector<float>& dst)
{
	computeQuadTexCoord2D(dst, Vec2(0.0f), Vec2(1.0f));
}

static void getAffineTexCoord2D (std::vector<float>& dst, int cellNdx)
{
	// Use basic coords as base.
	getBasicTexCoord2D(dst, cellNdx);

	// Rotate based on cell index.
	const float		angle		= 2.0f*DE_PI * ((float)cellNdx / 16.0f);
	const tcu::Mat2	rotMatrix	= tcu::rotationMatrix(angle);

	// Second and third row are sheared.
	const float		shearX		= de::inRange(cellNdx, 4, 11) ? (float)(15-cellNdx) / 16.0f : 0.0f;
	const tcu::Mat2	shearMatrix	= tcu::shearMatrix(tcu::Vec2(shearX, 0.0f));

	const tcu::Mat2	transform	= rotMatrix * shearMatrix;
	const Vec2		p0			= transform * Vec2(dst[0], dst[1]);
	const Vec2		p1			= transform * Vec2(dst[2], dst[3]);
	const Vec2		p2			= transform * Vec2(dst[4], dst[5]);
	const Vec2		p3			= transform * Vec2(dst[6], dst[7]);

	dst[0] = p0.x();	dst[1] = p0.y();
	dst[2] = p1.x();	dst[3] = p1.y();
	dst[4] = p2.x();	dst[5] = p2.y();
	dst[6] = p3.x();	dst[7] = p3.y();
}

tcu::TestStatus Texture2DMipmapTestInstance::iterate (void)
{
	const Sampler::FilterMode	magFilter		= Sampler::NEAREST;
	const int					viewportWidth	= m_renderer.getRenderWidth();
	const int					viewportHeight	= m_renderer.getRenderHeight();

	ReferenceParams				refParams		(TEXTURETYPE_2D);
	vector<float>				texCoord;

	const bool					isProjected		= m_testParameters.coordType == COORDTYPE_PROJECTED;
	const bool					useLodBias		= m_testParameters.coordType == COORDTYPE_BASIC_BIAS;

	tcu::Surface				renderedFrame	(viewportWidth, viewportHeight);

	// Viewport is divided into 4x4 grid.
	const int					gridWidth		= 4;
	const int					gridHeight		= 4;
	const int					cellWidth		= viewportWidth / gridWidth;
	const int					cellHeight		= viewportHeight / gridHeight;

	// Sampling parameters.
	refParams.sampler		= util::createSampler(m_testParameters.wrapS, m_testParameters.wrapT, m_testParameters.minFilter, magFilter);
	refParams.samplerType	= getSamplerType(vk::mapVkFormat(m_testParameters.format));
	refParams.flags			= (isProjected ? ReferenceParams::PROJECTED : 0) | (useLodBias ? ReferenceParams::USE_BIAS : 0);
	refParams.lodMode		= LODMODE_EXACT; // Use ideal lod.

	// Bias values.
	static const float s_bias[] = { 1.0f, -2.0f, 0.8f, -0.5f, 1.5f, 0.9f, 2.0f, 4.0f };

	// Projection values.
	static const Vec4 s_projections[] =
	{
		Vec4(1.2f, 1.0f, 0.7f, 1.0f),
		Vec4(1.3f, 0.8f, 0.6f, 2.0f),
		Vec4(0.8f, 1.0f, 1.7f, 0.6f),
		Vec4(1.2f, 1.0f, 1.7f, 1.5f)
	};

	// Render cells.
	for (int gridY = 0; gridY < gridHeight; gridY++)
	{
		for (int gridX = 0; gridX < gridWidth; gridX++)
		{
			const int	curX		= cellWidth*gridX;
			const int	curY		= cellHeight*gridY;
			const int	curW		= gridX+1 == gridWidth ? (viewportWidth-curX) : cellWidth;
			const int	curH		= gridY+1 == gridHeight ? (viewportHeight-curY) : cellHeight;
			const int	cellNdx		= gridY*gridWidth + gridX;

			// Compute texcoord.
			switch (m_testParameters.coordType)
			{
				case COORDTYPE_BASIC_BIAS:	// Fall-through.
				case COORDTYPE_PROJECTED:
				case COORDTYPE_BASIC:		getBasicTexCoord2D	(texCoord, cellNdx);	break;
				case COORDTYPE_AFFINE:		getAffineTexCoord2D	(texCoord, cellNdx);	break;
				default:					DE_ASSERT(DE_FALSE);
			}

			if (isProjected)
				refParams.w = s_projections[cellNdx % DE_LENGTH_OF_ARRAY(s_projections)];

			if (useLodBias)
				refParams.bias = s_bias[cellNdx % DE_LENGTH_OF_ARRAY(s_bias)];

			m_renderer.setViewport((float)curX, (float)curY, (float)curW, (float)curH);
			m_renderer.renderQuad(renderedFrame, 0, &texCoord[0], refParams);
		}
	}

	// Compare and log.
	{
		const tcu::IVec4		formatBitDepth	= getTextureFormatBitDepth(vk::mapVkFormat(VK_FORMAT_R8G8B8A8_UNORM));
		const tcu::PixelFormat	pixelFormat		(formatBitDepth[0], formatBitDepth[1], formatBitDepth[2], formatBitDepth[3]);
		const bool				isTrilinear		= m_testParameters.minFilter == Sampler::NEAREST_MIPMAP_LINEAR || m_testParameters.minFilter == Sampler::LINEAR_MIPMAP_LINEAR;
		tcu::Surface			referenceFrame	(viewportWidth, viewportHeight);
		tcu::Surface			errorMask		(viewportWidth, viewportHeight);
		tcu::LookupPrecision	lookupPrec;
		tcu::LodPrecision		lodPrec;
		int						numFailedPixels	= 0;

		lookupPrec.coordBits		= tcu::IVec3(20, 20, 0);
		lookupPrec.uvwBits			= tcu::IVec3(16, 16, 0); // Doesn't really matter since pixels are unicolored.
		lookupPrec.colorThreshold	= tcu::computeFixedPointThreshold(max(getBitsVec(pixelFormat) - (isTrilinear ? 2 : 1), tcu::IVec4(0)));
		lookupPrec.colorMask		= getCompareMask(pixelFormat);
		lodPrec.derivateBits		= 10;
		lodPrec.lodBits				= isProjected ? 6 : 8;

		for (int gridY = 0; gridY < gridHeight; gridY++)
		{
			for (int gridX = 0; gridX < gridWidth; gridX++)
			{
				const int	curX		= cellWidth*gridX;
				const int	curY		= cellHeight*gridY;
				const int	curW		= gridX+1 == gridWidth ? (viewportWidth-curX) : cellWidth;
				const int	curH		= gridY+1 == gridHeight ? (viewportHeight-curY) : cellHeight;
				const int	cellNdx		= gridY*gridWidth + gridX;

				// Compute texcoord.
				switch (m_testParameters.coordType)
				{
					case COORDTYPE_BASIC_BIAS:	// Fall-through.
					case COORDTYPE_PROJECTED:
					case COORDTYPE_BASIC:		getBasicTexCoord2D	(texCoord, cellNdx);	break;
					case COORDTYPE_AFFINE:		getAffineTexCoord2D	(texCoord, cellNdx);	break;
					default:					DE_ASSERT(DE_FALSE);
				}

				if (isProjected)
					refParams.w = s_projections[cellNdx % DE_LENGTH_OF_ARRAY(s_projections)];

				if (useLodBias)
					refParams.bias = s_bias[cellNdx % DE_LENGTH_OF_ARRAY(s_bias)];

				// Render ideal result
				sampleTexture(tcu::SurfaceAccess(referenceFrame, pixelFormat, curX, curY, curW, curH),
							  m_texture->getTexture(), &texCoord[0], refParams);

				// Compare this cell
				numFailedPixels += computeTextureLookupDiff(tcu::getSubregion(renderedFrame.getAccess(), curX, curY, curW, curH),
															tcu::getSubregion(referenceFrame.getAccess(), curX, curY, curW, curH),
															tcu::getSubregion(errorMask.getAccess(), curX, curY, curW, curH),
															m_texture->getTexture(), &texCoord[0], refParams,
															lookupPrec, lodPrec, m_context.getTestContext().getWatchDog());
			}
		}

		if (numFailedPixels > 0)
			m_context.getTestContext().getLog() << TestLog::Message << "ERROR: Image verification failed, found " << numFailedPixels << " invalid pixels!" << TestLog::EndMessage;

		m_context.getTestContext().getLog() << TestLog::ImageSet("Result", "Verification result")
											<< TestLog::Image("Rendered", "Rendered image", renderedFrame);

		if (numFailedPixels > 0)
		{
			m_context.getTestContext().getLog() << TestLog::Image("Reference", "Ideal reference", referenceFrame)
												<< TestLog::Image("ErrorMask", "Error mask", errorMask);
		}

		m_context.getTestContext().getLog() << TestLog::EndImageSet;

		{
			const bool isOk = numFailedPixels == 0;
			return isOk ? tcu::TestStatus::pass("pass") : tcu::TestStatus::fail("fail");
		}
	}
}

// TextureCubeMipmapTestInstance
class TextureCubeMipmapTestInstance : public TestInstance
{
public:
	typedef	TextureCubeMipmapTestCaseParameters	ParameterType;

									TextureCubeMipmapTestInstance	(Context& context, const ParameterType& testParameters);
									~TextureCubeMipmapTestInstance	(void);

	virtual tcu::TestStatus			iterate							(void);

private:
									TextureCubeMipmapTestInstance	(const TextureCubeMipmapTestInstance& other);
	TextureCubeMipmapTestInstance&	operator=						(const TextureCubeMipmapTestInstance& other);

	const ParameterType				m_testParameters;
	TestTextureCubeSp				m_texture;
	TextureRenderer					m_renderer;
};

TextureCubeMipmapTestInstance::TextureCubeMipmapTestInstance (Context& context, const TextureCubeMipmapTestCaseParameters& testParameters)
	: TestInstance		(context)
	, m_testParameters	(testParameters)
	, m_renderer		(context, m_testParameters.sampleCount, m_testParameters.size*2, m_testParameters.size*2)
{
	TCU_CHECK_INTERNAL(!(m_testParameters.coordType == COORDTYPE_PROJECTED && m_testParameters.sampleCount != VK_SAMPLE_COUNT_1_BIT));

	m_texture = TestTextureCubeSp(new pipeline::TestTextureCube(vk::mapVkFormat(m_testParameters.format), m_testParameters.size));

	const int numLevels = deLog2Floor32(m_testParameters.size)+1;

	// Fill texture with colored grid.
	for (int faceNdx = 0; faceNdx < tcu::CUBEFACE_LAST; faceNdx++)
	{
		for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
		{
			const deUint32	step	= 0xff / (numLevels-1);
			const deUint32	inc		= deClamp32(step*levelNdx, 0x00, 0xff);
			const deUint32	dec		= 0xff - inc;
			deUint32		rgb		= 0;

			switch (faceNdx)
			{
				case 0: rgb = (inc << 16) | (dec << 8) | 255; break;
				case 1: rgb = (255 << 16) | (inc << 8) | dec; break;
				case 2: rgb = (dec << 16) | (255 << 8) | inc; break;
				case 3: rgb = (dec << 16) | (inc << 8) | 255; break;
				case 4: rgb = (255 << 16) | (dec << 8) | inc; break;
				case 5: rgb = (inc << 16) | (255 << 8) | dec; break;
			}

			const deUint32	color	= 0xff000000 | rgb;
			tcu::clear(m_texture->getLevel(levelNdx, (tcu::CubeFace)faceNdx), tcu::RGBA(color).toVec());
		}
	}

	m_renderer.addCubeTexture(m_texture, testParameters.aspectMask);
}

TextureCubeMipmapTestInstance::~TextureCubeMipmapTestInstance (void)
{
}

static void randomPartition (vector<IVec4>& dst, de::Random& rnd, int x, int y, int width, int height)
{
	const int	minWidth	= 8;
	const int	minHeight	= 8;

	const bool	partition	= rnd.getFloat() > 0.4f;
	const bool	partitionX	= partition && width > minWidth && rnd.getBool();
	const bool	partitionY	= partition && height > minHeight && !partitionX;

	if (partitionX)
	{
		const int split = width/2 + rnd.getInt(-width/4, +width/4);
		randomPartition(dst, rnd, x, y, split, height);
		randomPartition(dst, rnd, x+split, y, width-split, height);
	}
	else if (partitionY)
	{
		const int split = height/2 + rnd.getInt(-height/4, +height/4);
		randomPartition(dst, rnd, x, y, width, split);
		randomPartition(dst, rnd, x, y+split, width, height-split);
	}
	else
		dst.push_back(IVec4(x, y, width, height));
}

static void computeGridLayout (vector<IVec4>& dst, int width, int height)
{
	de::Random rnd(7);
	randomPartition(dst, rnd, 0, 0, width, height);
}

tcu::TestStatus TextureCubeMipmapTestInstance::iterate (void)
{
	const int			viewportWidth	= m_renderer.getRenderWidth();
	const int			viewportHeight	= m_renderer.getRenderHeight();

	const bool			isProjected		= m_testParameters.coordType == COORDTYPE_PROJECTED;
	const bool			useLodBias		= m_testParameters.coordType == COORDTYPE_BASIC_BIAS;

	ReferenceParams		refParams		(TEXTURETYPE_CUBE);
	vector<float>		texCoord;
	tcu::Surface		renderedFrame	(viewportWidth, viewportHeight);

	refParams.sampler		= util::createSampler(m_testParameters.wrapS, m_testParameters.wrapT, m_testParameters.minFilter, m_testParameters.magFilter);
	refParams.samplerType	= getSamplerType(vk::mapVkFormat(m_testParameters.format));
	refParams.flags			= (isProjected ? ReferenceParams::PROJECTED : 0) | (useLodBias ? ReferenceParams::USE_BIAS : 0);
	refParams.lodMode		= LODMODE_EXACT; // Use ideal lod.

	// Compute grid.
	vector<IVec4> gridLayout;
	computeGridLayout(gridLayout, viewportWidth, viewportHeight);

	// Bias values.
	static const float s_bias[] = { 1.0f, -2.0f, 0.8f, -0.5f, 1.5f, 0.9f, 2.0f, 4.0f };

	// Projection values \note Less agressive than in 2D case due to smaller quads.
	static const Vec4 s_projections[] =
	{
		Vec4(1.2f, 1.0f, 0.7f, 1.0f),
		Vec4(1.3f, 0.8f, 0.6f, 1.1f),
		Vec4(0.8f, 1.0f, 1.2f, 0.8f),
		Vec4(1.2f, 1.0f, 1.3f, 0.9f)
	};

	// Render with GL
	for (int cellNdx = 0; cellNdx < (int)gridLayout.size(); cellNdx++)
	{
		const float			curX		= (float)gridLayout[cellNdx].x();
		const float			curY		= (float)gridLayout[cellNdx].y();
		const float			curW		= (float)gridLayout[cellNdx].z();
		const float			curH		= (float)gridLayout[cellNdx].w();
		const tcu::CubeFace	cubeFace	= (tcu::CubeFace)(cellNdx % tcu::CUBEFACE_LAST);

		DE_ASSERT(m_testParameters.coordType != COORDTYPE_AFFINE); // Not supported.
		computeQuadTexCoordCube(texCoord, cubeFace);

		if (isProjected)
		{
			refParams.flags	|= ReferenceParams::PROJECTED;
			refParams.w		 = s_projections[cellNdx % DE_LENGTH_OF_ARRAY(s_projections)];
		}

		if (useLodBias)
		{
			refParams.flags	|= ReferenceParams::USE_BIAS;
			refParams.bias	 = s_bias[cellNdx % DE_LENGTH_OF_ARRAY(s_bias)];
		}

		// Render
		m_renderer.setViewport(curX, curY, curW, curH);
		m_renderer.renderQuad(renderedFrame, 0, &texCoord[0], refParams);
	}

	// Render reference and compare
	{
		const tcu::IVec4		formatBitDepth		= getTextureFormatBitDepth(vk::mapVkFormat(VK_FORMAT_R8G8B8A8_UNORM));
		const tcu::PixelFormat	pixelFormat			(formatBitDepth[0], formatBitDepth[1], formatBitDepth[2], formatBitDepth[3]);
		tcu::Surface			referenceFrame		(viewportWidth, viewportHeight);
		tcu::Surface			errorMask			(viewportWidth, viewportHeight);
		int						numFailedPixels		= 0;
		tcu::LookupPrecision	lookupPrec;
		tcu::LodPrecision		lodPrec;

		// Params for rendering reference
		refParams.sampler					= util::createSampler(m_testParameters.wrapS, m_testParameters.wrapT, m_testParameters.minFilter, m_testParameters.magFilter);
		refParams.sampler.seamlessCubeMap	= true;
		refParams.lodMode					= LODMODE_EXACT;

		// Comparison parameters
		lookupPrec.colorMask		= getCompareMask(pixelFormat);
		lookupPrec.colorThreshold	= tcu::computeFixedPointThreshold(max(getBitsVec(pixelFormat)-2, tcu::IVec4(0)));
		lookupPrec.coordBits		= isProjected ? tcu::IVec3(8) : tcu::IVec3(10);
		lookupPrec.uvwBits			= tcu::IVec3(5,5,0);
		lodPrec.derivateBits		= 10;
		lodPrec.lodBits				= isProjected ? 3 : 6;

		for (int cellNdx = 0; cellNdx < (int)gridLayout.size(); cellNdx++)
		{
			const int				curX		= gridLayout[cellNdx].x();
			const int				curY		= gridLayout[cellNdx].y();
			const int				curW		= gridLayout[cellNdx].z();
			const int				curH		= gridLayout[cellNdx].w();
			const tcu::CubeFace		cubeFace	= (tcu::CubeFace)(cellNdx % tcu::CUBEFACE_LAST);

			DE_ASSERT(m_testParameters.coordType != COORDTYPE_AFFINE); // Not supported.
			computeQuadTexCoordCube(texCoord, cubeFace);

			if (isProjected)
			{
				refParams.flags	|= ReferenceParams::PROJECTED;
				refParams.w		 = s_projections[cellNdx % DE_LENGTH_OF_ARRAY(s_projections)];
			}

			if (useLodBias)
			{
				refParams.flags	|= ReferenceParams::USE_BIAS;
				refParams.bias	 = s_bias[cellNdx % DE_LENGTH_OF_ARRAY(s_bias)];
			}

			// Render ideal reference.
			{
				tcu::SurfaceAccess idealDst(referenceFrame, pixelFormat, curX, curY, curW, curH);
				sampleTexture(idealDst, m_texture->getTexture(), &texCoord[0], refParams);
			}

			// Compare this cell
			numFailedPixels += computeTextureLookupDiff(tcu::getSubregion(renderedFrame.getAccess(), curX, curY, curW, curH),
														tcu::getSubregion(referenceFrame.getAccess(), curX, curY, curW, curH),
														tcu::getSubregion(errorMask.getAccess(), curX, curY, curW, curH),
														m_texture->getTexture(), &texCoord[0], refParams,
														lookupPrec, lodPrec, m_context.getTestContext().getWatchDog());
		}

		if (numFailedPixels > 0)
		{
			m_context.getTestContext().getLog() << TestLog::Message << "ERROR: Image verification failed, found " << numFailedPixels << " invalid pixels!" << TestLog::EndMessage;
		}

		m_context.getTestContext().getLog() << TestLog::ImageSet("Result", "Verification result")
											<< TestLog::Image("Rendered", "Rendered image", renderedFrame);

		if (numFailedPixels > 0)
		{
			m_context.getTestContext().getLog() << TestLog::Image("Reference", "Ideal reference", referenceFrame)
												<< TestLog::Image("ErrorMask", "Error mask", errorMask);
		}

		m_context.getTestContext().getLog() << TestLog::EndImageSet;

		{
			const bool isOk = numFailedPixels == 0;
			return isOk ? tcu::TestStatus::pass("pass") : tcu::TestStatus::fail("fail");
		}
	}
}

// Texture3DMipmapTestInstance
class Texture3DMipmapTestInstance : public TestInstance
{
public:
	typedef Texture3DMipmapTestCaseParameters	ParameterType;

									Texture3DMipmapTestInstance		(Context& context, const ParameterType& testParameters);
									~Texture3DMipmapTestInstance	(void);

	virtual tcu::TestStatus			iterate							(void);

private:
									Texture3DMipmapTestInstance		(const Texture3DMipmapTestInstance& other);
	Texture3DMipmapTestInstance&	operator=						(const Texture3DMipmapTestInstance& other);

	const ParameterType				m_testParameters;
	TestTexture3DSp					m_texture;
	TextureRenderer					m_renderer;
};

Texture3DMipmapTestInstance::Texture3DMipmapTestInstance (Context& context, const Texture3DMipmapTestCaseParameters& testParameters)
	: TestInstance		(context)
	, m_testParameters	(testParameters)
	, m_renderer		(context, testParameters.sampleCount, testParameters.width*4, testParameters.height*4)
{
	TCU_CHECK_INTERNAL(!(m_testParameters.coordType == COORDTYPE_PROJECTED && m_testParameters.sampleCount != VK_SAMPLE_COUNT_1_BIT));

	const tcu::TextureFormat&	texFmt		= mapVkFormat(testParameters.format);
	tcu::TextureFormatInfo		fmtInfo		= tcu::getTextureFormatInfo(texFmt);
	const tcu::Vec4&			cScale		= fmtInfo.lookupScale;
	const tcu::Vec4&			cBias		= fmtInfo.lookupBias;
	const int					numLevels	= deLog2Floor32(de::max(de::max(testParameters.width, testParameters.height), testParameters.depth))+1;

	m_texture = TestTexture3DSp(new pipeline::TestTexture3D(vk::mapVkFormat(m_testParameters.format), m_testParameters.width, m_testParameters.height, m_testParameters.depth));

	// Fill texture with colored grid.
	for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
	{
		const deUint32	step	= 0xff / (numLevels-1);
		const deUint32	inc		= deClamp32(step*levelNdx, 0x00, 0xff);
		const deUint32	dec		= 0xff - inc;
		const deUint32	rgb		= (0xff << 16) | (dec << 8) | inc;
		const deUint32	color	= 0xff000000 | rgb;

		tcu::clear(m_texture->getLevel(levelNdx, 0), tcu::RGBA(color).toVec()*cScale + cBias);
	}

	m_renderer.add3DTexture(m_texture, testParameters.aspectMask);
}

Texture3DMipmapTestInstance::~Texture3DMipmapTestInstance (void)
{
}

static void getBasicTexCoord3D (std::vector<float>& dst, int cellNdx)
{
	static const struct
	{
		const float sScale;
		const float sBias;
		const float tScale;
		const float tBias;
		const float rScale;
		const float rBias;
	} s_params[] =
	{
	//		sScale	sBias	tScale	tBias	rScale	rBias
		{	 0.9f,	-0.1f,	 0.7f,	 0.3f,	 0.8f,	 0.9f	},
		{	 1.2f,	-0.1f,	 1.1f,	 0.3f,	 1.0f,	 0.9f	},
		{	 1.5f,	 0.7f,	 0.9f,	-0.3f,	 1.1f,	 0.1f	},
		{	 1.2f,	 0.7f,	-2.3f,	-0.3f,	 1.1f,	 0.2f	},
		{	 1.1f,	 0.8f,	-1.3f,	-0.3f,	 2.9f,	 0.9f	},
		{	 3.4f,	 0.8f,	 4.0f,	 0.0f,	-3.3f,	-1.0f	},
		{	-3.4f,	-0.1f,	-4.0f,	 0.0f,	-5.1f,	 1.0f	},
		{	-4.0f,	-0.1f,	 3.4f,	 0.1f,	 5.7f,	 0.0f	},
		{	-5.6f,	 0.0f,	 0.5f,	 1.2f,	 3.9f,	 4.0f	},
		{	 5.0f,	-2.0f,	 3.1f,	 1.2f,	 5.1f,	 0.2f	},
		{	 2.5f,	-2.0f,	 6.3f,	 3.0f,	 5.1f,	 0.2f	},
		{	-8.3f,	 0.0f,	 7.1f,	 3.0f,	 2.0f,	 0.2f	},
		{	 3.8f,	 0.0f,	 9.7f,	 1.0f,	 7.0f,	 0.7f	},
		{	13.3f,	 0.0f,	 7.1f,	 3.0f,	 2.0f,	 0.2f	},
		{	16.0f,	 8.0f,	12.7f,	 1.0f,	17.1f,	 0.7f	},
		{	15.3f,	 0.0f,	20.1f,	 3.0f,	33.0f,	 3.2f	}
	};

	const float sScale	= s_params[cellNdx%DE_LENGTH_OF_ARRAY(s_params)].sScale;
	const float sBias	= s_params[cellNdx%DE_LENGTH_OF_ARRAY(s_params)].sBias;
	const float tScale	= s_params[cellNdx%DE_LENGTH_OF_ARRAY(s_params)].tScale;
	const float tBias	= s_params[cellNdx%DE_LENGTH_OF_ARRAY(s_params)].tBias;
	const float rScale	= s_params[cellNdx%DE_LENGTH_OF_ARRAY(s_params)].rScale;
	const float rBias	= s_params[cellNdx%DE_LENGTH_OF_ARRAY(s_params)].rBias;

	dst.resize(3*4);

	dst[0] = sBias;			dst[ 1] = tBias;			dst[ 2] = rBias;
	dst[3] = sBias;			dst[ 4] = tBias+tScale;		dst[ 5] = rBias+rScale*0.5f;
	dst[6] = sBias+sScale;	dst[ 7] = tBias;			dst[ 8] = rBias+rScale*0.5f;
	dst[9] = sBias+sScale;	dst[10] = tBias+tScale;		dst[11] = rBias+rScale;
}

static void getBasicTexCoord3DImageViewMinlodIntTexCoord (std::vector<float>& dst)
{
	const float sScale	= 1.0f;
	const float sBias	= 0.0f;
	const float tScale	= 1.0f;
	const float tBias	= 0.0f;
	const float rScale	= 1.0f;
	const float rBias	= 0.0f;

	dst.resize(3*4);

	dst[0] = sBias;			dst[ 1] = tBias;			dst[ 2] = rBias;
	dst[3] = sBias;			dst[ 4] = tBias+tScale;		dst[ 5] = rBias+rScale*0.5f;
	dst[6] = sBias+sScale;	dst[ 7] = tBias;			dst[ 8] = rBias+rScale*0.5f;
	dst[9] = sBias+sScale;	dst[10] = tBias+tScale;		dst[11] = rBias+rScale;
}

static void getAffineTexCoord3D (std::vector<float>& dst, int cellNdx)
{
	// Use basic coords as base.
	getBasicTexCoord3D(dst, cellNdx);

	// Rotate based on cell index.
	const float		angleX		= 0.0f + 2.0f*DE_PI * ((float)cellNdx / 16.0f);
	const float		angleY		= 1.0f + 2.0f*DE_PI * ((float)cellNdx / 32.0f);
	const tcu::Mat3	rotMatrix	= tcu::rotationMatrixX(angleX) * tcu::rotationMatrixY(angleY);

	const Vec3		p0			= rotMatrix * Vec3(dst[0], dst[ 1], dst[ 2]);
	const Vec3		p1			= rotMatrix * Vec3(dst[3], dst[ 4], dst[ 5]);
	const Vec3		p2			= rotMatrix * Vec3(dst[6], dst[ 7], dst[ 8]);
	const Vec3		p3			= rotMatrix * Vec3(dst[9], dst[10], dst[11]);

	dst[0] = p0.x();	dst[ 1] = p0.y();	dst[ 2] = p0.z();
	dst[3] = p1.x();	dst[ 4] = p1.y();	dst[ 5] = p1.z();
	dst[6] = p2.x();	dst[ 7] = p2.y();	dst[ 8] = p2.z();
	dst[9] = p3.x();	dst[10] = p3.y();	dst[11] = p3.z();
}

tcu::TestStatus Texture3DMipmapTestInstance::iterate (void)
{
	const tcu::TextureFormat&		texFmt			= m_texture->getTextureFormat();
	const tcu::TextureFormatInfo	fmtInfo			= tcu::getTextureFormatInfo(texFmt);
	const Sampler::FilterMode		magFilter		= Sampler::NEAREST;
	const int						viewportWidth	= m_renderer.getRenderWidth();
	const int						viewportHeight	= m_renderer.getRenderHeight();

	const bool						isProjected		= m_testParameters.coordType == COORDTYPE_PROJECTED;
	const bool						useLodBias		= m_testParameters.coordType == COORDTYPE_BASIC_BIAS;

	// Viewport is divided into 4x4 grid.
	const int						gridWidth		= 4;
	const int						gridHeight		= 4;
	const int						cellWidth		= viewportWidth / gridWidth;
	const int						cellHeight		= viewportHeight / gridHeight;

	ReferenceParams					refParams		(TEXTURETYPE_3D);

	tcu::Surface					renderedFrame	(viewportWidth, viewportHeight);
	vector<float>					texCoord;

	// Sampling parameters.
	refParams.sampler		= util::createSampler(m_testParameters.wrapS, m_testParameters.wrapT, m_testParameters.minFilter, magFilter);
	refParams.samplerType	= getSamplerType(texFmt);

	refParams.colorBias		= fmtInfo.lookupBias;
	refParams.colorScale	= fmtInfo.lookupScale;
	refParams.flags			= (isProjected ? ReferenceParams::PROJECTED : 0) | (useLodBias ? ReferenceParams::USE_BIAS : 0);

	// Bias values.
	static const float s_bias[] = { 1.0f, -2.0f, 0.8f, -0.5f, 1.5f, 0.9f, 2.0f, 4.0f };

	// Projection values.
	static const Vec4 s_projections[] =
	{
		Vec4(1.2f, 1.0f, 0.7f, 1.0f),
		Vec4(1.3f, 0.8f, 0.6f, 2.0f),
		Vec4(0.8f, 1.0f, 1.7f, 0.6f),
		Vec4(1.2f, 1.0f, 1.7f, 1.5f)
	};

	// Render cells.
	for (int gridY = 0; gridY < gridHeight; gridY++)
	{
		for (int gridX = 0; gridX < gridWidth; gridX++)
		{
			const int	curX		= cellWidth*gridX;
			const int	curY		= cellHeight*gridY;
			const int	curW		= gridX+1 == gridWidth ? (viewportWidth-curX) : cellWidth;
			const int	curH		= gridY+1 == gridHeight ? (viewportHeight-curY) : cellHeight;
			const int	cellNdx		= gridY*gridWidth + gridX;

			// Compute texcoord.
			switch (m_testParameters.coordType)
			{
				case COORDTYPE_BASIC_BIAS:	// Fall-through.
				case COORDTYPE_PROJECTED:
				case COORDTYPE_BASIC:		getBasicTexCoord3D	(texCoord, cellNdx);	break;
				case COORDTYPE_AFFINE:		getAffineTexCoord3D	(texCoord, cellNdx);	break;
				default:					DE_ASSERT(DE_FALSE);
			}

			// Set projection.
			if (isProjected)
				refParams.w = s_projections[cellNdx % DE_LENGTH_OF_ARRAY(s_projections)];

			// Set LOD bias.
			if (useLodBias)
				refParams.bias = s_bias[cellNdx % DE_LENGTH_OF_ARRAY(s_bias)];

			m_renderer.setViewport((float)curX, (float)curY, (float)curW, (float)curH);
			m_renderer.renderQuad(renderedFrame, 0, &texCoord[0], refParams);
		}
	}

	// Compare and log
	{
		const tcu::IVec4		formatBitDepth	= getTextureFormatBitDepth(vk::mapVkFormat(VK_FORMAT_R8G8B8A8_UNORM));
		const tcu::PixelFormat	pixelFormat		(formatBitDepth[0], formatBitDepth[1], formatBitDepth[2], formatBitDepth[3]);
		const bool				isTrilinear		= m_testParameters.minFilter == Sampler::NEAREST_MIPMAP_LINEAR || m_testParameters.minFilter == Sampler::LINEAR_MIPMAP_LINEAR;
		tcu::Surface			referenceFrame	(viewportWidth, viewportHeight);
		tcu::Surface			errorMask		(viewportWidth, viewportHeight);
		tcu::LookupPrecision	lookupPrec;
		tcu::LodPrecision		lodPrec;
		int						numFailedPixels	= 0;

		lookupPrec.coordBits		= tcu::IVec3(20, 20, 20);
		lookupPrec.uvwBits			= tcu::IVec3(16, 16, 16); // Doesn't really matter since pixels are unicolored.
		lookupPrec.colorThreshold	= tcu::computeFixedPointThreshold(max(getBitsVec(pixelFormat) - (isTrilinear ? 2 : 1), tcu::IVec4(0)));
		lookupPrec.colorMask		= getCompareMask(pixelFormat);
		lodPrec.derivateBits		= 10;
		lodPrec.lodBits				= isProjected ? 6 : 8;

		for (int gridY = 0; gridY < gridHeight; gridY++)
		{
			for (int gridX = 0; gridX < gridWidth; gridX++)
			{
				const int	curX		= cellWidth*gridX;
				const int	curY		= cellHeight*gridY;
				const int	curW		= gridX+1 == gridWidth ? (viewportWidth-curX) : cellWidth;
				const int	curH		= gridY+1 == gridHeight ? (viewportHeight-curY) : cellHeight;
				const int	cellNdx		= gridY*gridWidth + gridX;

				switch (m_testParameters.coordType)
				{
					case COORDTYPE_BASIC_BIAS:	// Fall-through.
					case COORDTYPE_PROJECTED:
					case COORDTYPE_BASIC:		getBasicTexCoord3D	(texCoord, cellNdx);	break;
					case COORDTYPE_AFFINE:		getAffineTexCoord3D	(texCoord, cellNdx);	break;
					default:					DE_ASSERT(DE_FALSE);
				}

				if (isProjected)
					refParams.w = s_projections[cellNdx % DE_LENGTH_OF_ARRAY(s_projections)];

				if (useLodBias)
					refParams.bias = s_bias[cellNdx % DE_LENGTH_OF_ARRAY(s_bias)];

				// Render ideal result
				sampleTexture(tcu::SurfaceAccess(referenceFrame, pixelFormat, curX, curY, curW, curH),
							  m_texture->getTexture(), &texCoord[0], refParams);

				// Compare this cell
				numFailedPixels += computeTextureLookupDiff(tcu::getSubregion(renderedFrame.getAccess(), curX, curY, curW, curH),
															tcu::getSubregion(referenceFrame.getAccess(), curX, curY, curW, curH),
															tcu::getSubregion(errorMask.getAccess(), curX, curY, curW, curH),
															m_texture->getTexture(), &texCoord[0], refParams,
															lookupPrec, lodPrec, m_context.getTestContext().getWatchDog());
			}
		}

		if (numFailedPixels > 0)
			m_context.getTestContext().getLog() << TestLog::Message << "ERROR: Image verification failed, found " << numFailedPixels << " invalid pixels!" << TestLog::EndMessage;

		m_context.getTestContext().getLog() << TestLog::ImageSet("Result", "Verification result")
											<< TestLog::Image("Rendered", "Rendered image", renderedFrame);

		if (numFailedPixels > 0)
		{
			m_context.getTestContext().getLog() << TestLog::Image("Reference", "Ideal reference", referenceFrame)
												<< TestLog::Image("ErrorMask", "Error mask", errorMask);
		}

		m_context.getTestContext().getLog() << TestLog::EndImageSet;

		{
			const bool isOk = numFailedPixels == 0;
			return isOk ? tcu::TestStatus::pass("pass") : tcu::TestStatus::fail("fail");
		}
	}
}

// Texture2DLodControlTestInstance
class Texture2DLodControlTestInstance : public TestInstance
{
public:
	typedef Texture2DMipmapTestCaseParameters	ParameterType;

										Texture2DLodControlTestInstance		(Context& context, const ParameterType& testParameters);
										~Texture2DLodControlTestInstance	(void);

	virtual tcu::TestStatus				iterate								(void);

protected:
	virtual void						getReferenceParams					(ReferenceParams& params, int cellNdx) = 0;

	const int							m_texWidth;
	const int							m_texHeight;

private:
										Texture2DLodControlTestInstance		(const Texture2DLodControlTestInstance& other);
	Texture2DLodControlTestInstance&	operator=							(const Texture2DLodControlTestInstance& other);

	const ParameterType					m_testParameters;
	tcu::Sampler::FilterMode			m_minFilter;
	TestTexture2DSp						m_texture;
	TextureRenderer						m_renderer;
};

Texture2DLodControlTestInstance::Texture2DLodControlTestInstance (Context& context, const Texture2DMipmapTestCaseParameters& testParameters)
	: TestInstance		(context)
	, m_texWidth		(64) //64
	, m_texHeight		(64)//64
	, m_testParameters	(testParameters)
	, m_minFilter		(testParameters.minFilter)
	, m_texture			(DE_NULL)
	, m_renderer		(context, testParameters.sampleCount, m_texWidth*4, m_texHeight*4, vk::makeComponentMappingRGBA(), testParameters.testType > util::TextureCommonTestCaseParameters::TEST_IMAGE_VIEW_MINLOD, testParameters.testType >= util::TextureCommonTestCaseParameters::TEST_IMAGE_VIEW_MINLOD)
{
	const VkFormat	format		= VK_FORMAT_R8G8B8A8_UNORM;
	const int		numLevels	= deLog2Floor32(de::max(m_texWidth, m_texHeight))+1;

	m_texture = TestTexture2DSp(new pipeline::TestTexture2D(vk::mapVkFormat(format), m_texWidth, m_texHeight));

	// Fill texture with colored grid.
	for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
	{
		const deUint32	step	= 0xff / (numLevels-1);
		const deUint32	inc		= deClamp32(step*levelNdx, 0x00, 0xff);
		const deUint32	dec		= 0xff - inc;
		const deUint32	rgb		= (inc << 16) | (dec << 8) | 0xff;
		const deUint32	color	= 0xff000000 | rgb;

		tcu::clear(m_texture->getLevel(levelNdx, 0), tcu::RGBA(color).toVec());
	}

	m_renderer.add2DTexture(m_texture, testParameters.aspectMask);
}

Texture2DLodControlTestInstance::~Texture2DLodControlTestInstance (void)
{
}

tcu::TestStatus Texture2DLodControlTestInstance::iterate (void)
{
	const tcu::Sampler::WrapMode	wrapS			= Sampler::REPEAT_GL;
	const tcu::Sampler::WrapMode	wrapT			= Sampler::REPEAT_GL;
	const tcu::Sampler::FilterMode	magFilter		= Sampler::NEAREST;

	const tcu::Texture2D&			refTexture		= m_texture->getTexture();

	const int						viewportWidth	= m_renderer.getRenderWidth();
	const int						viewportHeight	= m_renderer.getRenderHeight();

	tcu::Sampler					sampler			= util::createSampler(wrapS, wrapT, m_minFilter, magFilter);

	ReferenceParams					refParams		(TEXTURETYPE_2D, sampler);
	vector<float>					texCoord;
	tcu::Surface					renderedFrame	(viewportWidth, viewportHeight);

	// Viewport is divided into 4x4 grid.
	const int						gridWidth		= 4;
	const int						gridHeight		= 4;
	const int						cellWidth		= viewportWidth / gridWidth;
	const int						cellHeight		= viewportHeight / gridHeight;

	refParams.maxLevel = deLog2Floor32(de::max(m_texWidth, m_texHeight));

	// Render cells.
	for (int gridY = 0; gridY < gridHeight; gridY++)
	{
		for (int gridX = 0; gridX < gridWidth; gridX++)
		{
			const int	curX		= cellWidth*gridX;
			const int	curY		= cellHeight*gridY;
			const int	curW		= gridX+1 == gridWidth ? (viewportWidth-curX) : cellWidth;
			const int	curH		= gridY+1 == gridHeight ? (viewportHeight-curY) : cellHeight;
			const int	cellNdx		= gridY*gridWidth + gridX;

			getReferenceParams(refParams,cellNdx);

			// Compute texcoord.
			getBasicTexCoord2D(texCoord, cellNdx);
			// Render
			m_renderer.setViewport((float)curX, (float)curY, (float)curW, (float)curH);
			m_renderer.getTextureBinding(0)->updateTextureViewMipLevels(refParams.baseLevel, refParams.maxLevel, refParams.imageViewMinLod);
			m_renderer.renderQuad(renderedFrame, 0, &texCoord[0], refParams);
		}
	}

	// Compare and log.
	{
		const tcu::IVec4		formatBitDepth	= getTextureFormatBitDepth(vk::mapVkFormat(VK_FORMAT_R8G8B8A8_UNORM));
		const tcu::PixelFormat	pixelFormat		(formatBitDepth[0], formatBitDepth[1], formatBitDepth[2], formatBitDepth[3]);
		const bool				isTrilinear		= m_minFilter == Sampler::NEAREST_MIPMAP_LINEAR || m_minFilter == Sampler::LINEAR_MIPMAP_LINEAR;
		tcu::LookupPrecision	lookupPrec;
		tcu::LodPrecision		lodPrec;

		lookupPrec.coordBits		= tcu::IVec3(20, 20, 0);
		lookupPrec.uvwBits			= tcu::IVec3(16, 16, 0); // Doesn't really matter since pixels are unicolored.
		lookupPrec.colorThreshold	= tcu::computeFixedPointThreshold(max(getBitsVec(pixelFormat) - (isTrilinear ? 2 : 1), tcu::IVec4(0)));
		lookupPrec.colorMask		= getCompareMask(pixelFormat);
		lodPrec.derivateBits		= 10;
		lodPrec.lodBits				= 8;

		auto compareAndLogImages = [&] (tcu::ImageViewMinLodMode imageViewLodMode = tcu::IMAGEVIEWMINLODMODE_PREFERRED)
		{
			tcu::Surface			referenceFrame	(viewportWidth, viewportHeight);
			tcu::Surface			errorMask		(viewportWidth, viewportHeight);

			int						numFailedPixels = 0;

			for (int gridY = 0; gridY < gridHeight; gridY++)
			{
				for (int gridX = 0; gridX < gridWidth; gridX++)
				{
					const int	curX		= cellWidth*gridX;
					const int	curY		= cellHeight*gridY;
					const int	curW		= gridX+1 == gridWidth ? (viewportWidth-curX) : cellWidth;
					const int	curH		= gridY+1 == gridHeight ? (viewportHeight-curY) : cellHeight;
					const int	cellNdx		= gridY*gridWidth + gridX;

					getReferenceParams(refParams,cellNdx);

					refParams.imageViewMinLodMode = imageViewLodMode;

					// Compute texcoord.
					if (refParams.samplerType == glu::TextureTestUtil::SAMPLERTYPE_FETCH_FLOAT)
						getBasicTexCoord2DImageViewMinLodIntTexCoord(texCoord);
					else
						getBasicTexCoord2D(texCoord, cellNdx);

					// Render ideal result
					sampleTexture(tcu::SurfaceAccess(referenceFrame, pixelFormat, curX, curY, curW, curH),
								  refTexture, &texCoord[0], refParams);

					// Compare this cell
					numFailedPixels += computeTextureLookupDiff(tcu::getSubregion(renderedFrame.getAccess(), curX, curY, curW, curH),
																tcu::getSubregion(referenceFrame.getAccess(), curX, curY, curW, curH),
																tcu::getSubregion(errorMask.getAccess(), curX, curY, curW, curH),
																m_texture->getTexture(), &texCoord[0], refParams,
																lookupPrec, lodPrec, m_context.getTestContext().getWatchDog());
				}
			}

			if (numFailedPixels > 0)
			{
				m_context.getTestContext().getLog() << TestLog::Image("Reference", "Ideal reference", referenceFrame)
													<< TestLog::Image("ErrorMask", "Error mask", errorMask);
			}
			return numFailedPixels;
		};

		m_context.getTestContext().getLog() << TestLog::ImageSet("Result", "Verification result")
											<< TestLog::Image("Rendered", "Rendered image", renderedFrame);

		int numFailedPixels = compareAndLogImages();

		if (numFailedPixels > 0 && refParams.imageViewMinLod > 0.0f)
		{
			numFailedPixels = compareAndLogImages(tcu::IMAGEVIEWMINLODMODE_ALTERNATIVE);
		}
		m_context.getTestContext().getLog() << TestLog::EndImageSet;

		if (numFailedPixels > 0)
			m_context.getTestContext().getLog() << TestLog::Message << "ERROR: Image verification failed, found " << numFailedPixels << " invalid pixels!" << TestLog::EndMessage;

		{
			const bool isOk = numFailedPixels == 0;
			return isOk ? tcu::TestStatus::pass("pass") : tcu::TestStatus::fail("fail");
		}
	}
}

class Texture2DMinLodTestInstance : public Texture2DLodControlTestInstance
{
public:
	Texture2DMinLodTestInstance (Context& context, const Texture2DMipmapTestCaseParameters& testParameters)
		: Texture2DLodControlTestInstance(context, testParameters)
	{
	}

protected:
	void getReferenceParams (ReferenceParams& params, int cellNdx)
	{
		params.minLod = getMinLodForCell(cellNdx);
	}
};

class Texture2DMaxLodTestInstance : public Texture2DLodControlTestInstance
{
public:
	Texture2DMaxLodTestInstance (Context& context, const Texture2DMipmapTestCaseParameters& testParameters)
		: Texture2DLodControlTestInstance(context, testParameters)
	{
	}

protected:
	void getReferenceParams (ReferenceParams& params, int cellNdx)
	{
		params.maxLod = getMaxLodForCell(cellNdx);
	}
};

class Texture2DBaseLevelTestInstance : public Texture2DLodControlTestInstance
{
public:
	Texture2DBaseLevelTestInstance (Context& context, const Texture2DMipmapTestCaseParameters& testParameters)
		: Texture2DLodControlTestInstance(context, testParameters)
		, m_testParam (testParameters)
	{
	}

protected:
	const Texture2DMipmapTestCaseParameters m_testParam;

	int getBaseLevel (int cellNdx) const
	{
		const int	numLevels	= deLog2Floor32(de::max(m_texWidth, m_texHeight))+1;
		const int	baseLevel	= (deInt32Hash(cellNdx) ^ deStringHash(m_testParam.minFilterName) ^ 0xac2f274a) % numLevels;

		return baseLevel;
	}

	void getReferenceParams (ReferenceParams& params, int cellNdx)
	{
		params.baseLevel = getBaseLevel(cellNdx);
	}
};

class Texture2DMaxLevelTestInstance : public Texture2DLodControlTestInstance
{
public:
	Texture2DMaxLevelTestInstance (Context& context, const Texture2DMipmapTestCaseParameters& testParameters)
		: Texture2DLodControlTestInstance(context, testParameters)
		, m_testParam (testParameters)
	{
	}

protected:
	const Texture2DMipmapTestCaseParameters m_testParam;

	int getMaxLevel (int cellNdx) const
	{
		const int	numLevels	= deLog2Floor32(de::max(m_texWidth, m_texHeight))+1;
		const int	maxLevel	= (deInt32Hash(cellNdx) ^ deStringHash(m_testParam.minFilterName) ^ 0x82cfa4e) % numLevels;

		return maxLevel;
	}

	void getReferenceParams (ReferenceParams& params, int cellNdx)
	{
		params.maxLevel = getMaxLevel(cellNdx);
	}
};

// TextureCubeLodControlTestInstance
class TextureCubeLodControlTestInstance : public TestInstance
{
public:
	typedef TextureCubeMipmapTestCaseParameters	ParameterType;

										TextureCubeLodControlTestInstance	(Context& context, const ParameterType& testParameters);
										~TextureCubeLodControlTestInstance	(void);

	virtual tcu::TestStatus				iterate								(void);

protected:
	virtual void						getReferenceParams					(ReferenceParams& params, int cellNdx)	= DE_NULL;

	const int							m_texSize;

private:
										TextureCubeLodControlTestInstance	(const TextureCubeLodControlTestInstance& other);
	TextureCubeLodControlTestInstance&	operator=							(const TextureCubeLodControlTestInstance& other);

	const ParameterType					m_testParameters;
	tcu::Sampler::FilterMode			m_minFilter;
	TestTextureCubeSp					m_texture;
	TextureRenderer						m_renderer;
};

TextureCubeLodControlTestInstance::TextureCubeLodControlTestInstance (Context& context, const TextureCubeMipmapTestCaseParameters& testParameters)
	: TestInstance		(context)
	, m_texSize			(64)
	, m_testParameters	(testParameters)
	, m_minFilter		(testParameters.minFilter)
	, m_texture			(DE_NULL)

	, m_renderer		(context, testParameters.sampleCount, m_texSize*2, m_texSize*2)
{
	const VkFormat	format		= VK_FORMAT_R8G8B8A8_UNORM;
	const int		numLevels	= deLog2Floor32(m_texSize)+1;

	m_texture = TestTextureCubeSp(new pipeline::TestTextureCube(vk::mapVkFormat(format), m_texSize));

	// Fill texture with colored grid.
	for (int faceNdx = 0; faceNdx < tcu::CUBEFACE_LAST; faceNdx++)
	{
		for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
		{
			const deUint32	step	= 0xff / (numLevels-1);
			const deUint32	inc		= deClamp32(step*levelNdx, 0x00, 0xff);
			const deUint32	dec		= 0xff - inc;
			deUint32		rgb		= 0;

			switch (faceNdx)
			{
				case 0: rgb = (inc << 16) | (dec << 8) | 255; break;
				case 1: rgb = (255 << 16) | (inc << 8) | dec; break;
				case 2: rgb = (dec << 16) | (255 << 8) | inc; break;
				case 3: rgb = (dec << 16) | (inc << 8) | 255; break;
				case 4: rgb = (255 << 16) | (dec << 8) | inc; break;
				case 5: rgb = (inc << 16) | (255 << 8) | dec; break;
			}

			const deUint32	color	= 0xff000000 | rgb;

			tcu::clear(m_texture->getLevel(levelNdx, (tcu::CubeFace)faceNdx), tcu::RGBA(color).toVec());
		}
	}

	m_renderer.addCubeTexture(m_texture, testParameters.aspectMask);
}

TextureCubeLodControlTestInstance::~TextureCubeLodControlTestInstance (void)
{
}

tcu::TestStatus TextureCubeLodControlTestInstance::iterate (void)
{
	const tcu::Sampler::WrapMode	wrapS			= Sampler::CLAMP_TO_EDGE;
	const tcu::Sampler::WrapMode	wrapT			= Sampler::CLAMP_TO_EDGE;
	const tcu::Sampler::FilterMode	magFilter		= Sampler::NEAREST;

	const tcu::TextureCube&			refTexture		= m_texture->getTexture();
	const int						viewportWidth	= m_renderer.getRenderWidth();
	const int						viewportHeight	= m_renderer.getRenderHeight();

	tcu::Sampler					sampler			= util::createSampler(wrapS, wrapT, m_minFilter, magFilter);
	ReferenceParams					refParams		(TEXTURETYPE_CUBE, sampler);
	vector<float>					texCoord;
	tcu::Surface					renderedFrame	(viewportWidth, viewportHeight);

	refParams.maxLevel = deLog2Floor32(m_texSize);

	// Compute grid.
	vector<tcu::IVec4> gridLayout;
	computeGridLayout(gridLayout, viewportWidth, viewportHeight);

	for (int cellNdx = 0; cellNdx < (int)gridLayout.size(); cellNdx++)
	{
		const int			curX		= gridLayout[cellNdx].x();
		const int			curY		= gridLayout[cellNdx].y();
		const int			curW		= gridLayout[cellNdx].z();
		const int			curH		= gridLayout[cellNdx].w();
		const tcu::CubeFace	cubeFace	= (tcu::CubeFace)(cellNdx % tcu::CUBEFACE_LAST);

		computeQuadTexCoordCube(texCoord, cubeFace);
		getReferenceParams(refParams, cellNdx);

		// Render with GL.
		m_renderer.setViewport((float)curX, (float)curY, (float)curW, (float)curH);
		m_renderer.getTextureBinding(0)->updateTextureViewMipLevels(refParams.baseLevel, refParams.maxLevel, refParams.imageViewMinLod);
		m_renderer.renderQuad(renderedFrame, 0, &texCoord[0], refParams);
	}

	// Render reference and compare
	{
		const tcu::IVec4		formatBitDepth		= getTextureFormatBitDepth(mapVkFormat(VK_FORMAT_R8G8B8A8_UNORM));
		const tcu::PixelFormat	pixelFormat			(formatBitDepth[0], formatBitDepth[1], formatBitDepth[2], formatBitDepth[3]);
		tcu::LookupPrecision	lookupPrec;
		tcu::LodPrecision		lodPrec;

		// Params for rendering reference
		refParams.sampler					= util::createSampler(wrapS, wrapT, m_testParameters.minFilter, magFilter);
		refParams.sampler.seamlessCubeMap	= true;
		refParams.lodMode					= LODMODE_EXACT;

		// Comparison parameters
		lookupPrec.colorMask				= getCompareMask(pixelFormat);
		lookupPrec.colorThreshold			= tcu::computeFixedPointThreshold(max(getBitsVec(pixelFormat)-2, IVec4(0)));
		lookupPrec.coordBits				= tcu::IVec3(10);
		lookupPrec.uvwBits					= tcu::IVec3(5,5,0);
		lodPrec.derivateBits				= 10;
		lodPrec.lodBits						= 6;

		auto compareAndLogImages = [&](tcu::ImageViewMinLodMode imageViewLodMode = tcu::IMAGEVIEWMINLODMODE_PREFERRED)
		{
			tcu::Surface			referenceFrame(viewportWidth, viewportHeight);
			tcu::Surface			errorMask(viewportWidth, viewportHeight);
			int						numFailedPixels = 0;

			for (int cellNdx = 0; cellNdx < (int)gridLayout.size(); cellNdx++)
			{
				const int				curX		= gridLayout[cellNdx].x();
				const int				curY		= gridLayout[cellNdx].y();
				const int				curW		= gridLayout[cellNdx].z();
				const int				curH		= gridLayout[cellNdx].w();
				const tcu::CubeFace		cubeFace	= (tcu::CubeFace)(cellNdx % tcu::CUBEFACE_LAST);

				computeQuadTexCoordCube(texCoord, cubeFace);
				getReferenceParams(refParams, cellNdx);

				refParams.imageViewMinLodMode = imageViewLodMode;

				// Render ideal reference.
				{
					tcu::SurfaceAccess idealDst(referenceFrame, pixelFormat, curX, curY, curW, curH);
					sampleTexture(idealDst, refTexture, &texCoord[0], refParams);
				}

				// Compare this cell
				numFailedPixels += computeTextureLookupDiff(tcu::getSubregion(renderedFrame.getAccess(), curX, curY, curW, curH),
															tcu::getSubregion(referenceFrame.getAccess(), curX, curY, curW, curH),
															tcu::getSubregion(errorMask.getAccess(), curX, curY, curW, curH),
															m_texture->getTexture(), &texCoord[0], refParams,
															lookupPrec, lodPrec,  m_context.getTestContext().getWatchDog());
			}
			if (numFailedPixels > 0)
			{
				m_context.getTestContext().getLog() << TestLog::Image("Reference", "Ideal reference", referenceFrame)
													<< TestLog::Image("ErrorMask", "Error mask", errorMask);
			}
			return numFailedPixels;
		};

		m_context.getTestContext().getLog() << TestLog::ImageSet("Result", "Verification result")
											<< TestLog::Image("Rendered", "Rendered image", renderedFrame);

		int numFailedPixels = compareAndLogImages();

		if (numFailedPixels > 0 && refParams.imageViewMinLod > 0.0f)
		{
			numFailedPixels = compareAndLogImages(tcu::IMAGEVIEWMINLODMODE_ALTERNATIVE);
		}
		m_context.getTestContext().getLog() << TestLog::EndImageSet;

		if (numFailedPixels > 0)
			m_context.getTestContext().getLog() << TestLog::Message << "ERROR: Image verification failed, found " << numFailedPixels << " invalid pixels!" << TestLog::EndMessage;

		{
			const bool isOk = numFailedPixels == 0;
			return isOk ? tcu::TestStatus::pass("pass") : tcu::TestStatus::fail("fail");
		}
	}
}

class TextureCubeMinLodTestInstance : public TextureCubeLodControlTestInstance
{
public:
	TextureCubeMinLodTestInstance (Context& context, const TextureCubeMipmapTestCaseParameters& testParameters)
		: TextureCubeLodControlTestInstance(context, testParameters)
	{
	}

protected:
	void getReferenceParams (ReferenceParams& params, int cellNdx)
	{
		params.minLod = getMinLodForCell(cellNdx);
	}
};

class TextureCubeMaxLodTestInstance : public TextureCubeLodControlTestInstance
{
public:
	TextureCubeMaxLodTestInstance (Context& context, const TextureCubeMipmapTestCaseParameters& testParameters)
		: TextureCubeLodControlTestInstance(context, testParameters)
	{
	}

protected:
	void getReferenceParams (ReferenceParams& params, int cellNdx)
	{
		params.maxLod = getMaxLodForCell(cellNdx);
	}
};

class TextureCubeBaseLevelTestInstance : public TextureCubeLodControlTestInstance
{
public:
	TextureCubeBaseLevelTestInstance (Context& context, const TextureCubeMipmapTestCaseParameters& testParameters)
		: TextureCubeLodControlTestInstance(context, testParameters)
		, m_testParam (testParameters)
	{
	}

protected:
	const TextureCubeMipmapTestCaseParameters m_testParam;

	int getBaseLevel (int cellNdx) const
	{
		const int	numLevels	= deLog2Floor32(m_texSize)+1;
		const int	baseLevel	= (deInt32Hash(cellNdx) ^ deStringHash(m_testParam.minFilterName) ^ 0x23fae13) % numLevels;

		return baseLevel;
	}

	void getReferenceParams (ReferenceParams& params, int cellNdx)
	{
		params.baseLevel = getBaseLevel(cellNdx);
	}
};

class TextureCubeMaxLevelTestInstance : public TextureCubeLodControlTestInstance
{
public:
	TextureCubeMaxLevelTestInstance (Context& context, const TextureCubeMipmapTestCaseParameters& testParameters)
		: TextureCubeLodControlTestInstance(context, testParameters)
		, m_testParam (testParameters)
	{
	}

protected:
	const TextureCubeMipmapTestCaseParameters m_testParam;
	int getMaxLevel (int cellNdx) const
	{
		const int	numLevels	= deLog2Floor32(m_texSize)+1;
		const int	maxLevel	= (deInt32Hash(cellNdx) ^ deStringHash(m_testParam.minFilterName) ^ 0x974e21) % numLevels;

		return maxLevel;
	}

	void getReferenceParams (ReferenceParams& params, int cellNdx)
	{
		params.maxLevel = getMaxLevel(cellNdx);
	}
};

// Texture3DLodControlTestInstance
class Texture3DLodControlTestInstance : public TestInstance
{
public:
	typedef Texture3DMipmapTestCaseParameters	ParameterType;

										Texture3DLodControlTestInstance		(Context& context, const ParameterType& testParameters);
										~Texture3DLodControlTestInstance	(void);

	virtual tcu::TestStatus				iterate								(void);

protected:
	virtual void						getReferenceParams					(ReferenceParams& params, int cellNdx)	= DE_NULL;

	const int							m_texWidth;
	const int							m_texHeight;
	const int							m_texDepth;

private:
										Texture3DLodControlTestInstance		(const Texture3DLodControlTestInstance& other);
	Texture3DLodControlTestInstance&	operator=							(const Texture3DLodControlTestInstance& other);

	const ParameterType					m_testParameters;
	tcu::Sampler::FilterMode			m_minFilter;
	TestTexture3DSp						m_texture;
	TextureRenderer						m_renderer;
};

Texture3DLodControlTestInstance::Texture3DLodControlTestInstance (Context& context, const Texture3DMipmapTestCaseParameters& testParameters)
	: TestInstance		(context)
	, m_texWidth		(32)
	, m_texHeight		(32)
	, m_texDepth		(32)
	, m_testParameters	(testParameters)
	, m_minFilter		(testParameters.minFilter)
	, m_texture			(DE_NULL)
	, m_renderer		(context, testParameters.sampleCount, m_texWidth*4, m_texHeight*4, vk::makeComponentMappingRGBA(), testParameters.testType > util::TextureCommonTestCaseParameters::TEST_IMAGE_VIEW_MINLOD, testParameters.testType >= util::TextureCommonTestCaseParameters::TEST_IMAGE_VIEW_MINLOD)
{
	const VkFormat			format		= VK_FORMAT_R8G8B8A8_UNORM;
	tcu::TextureFormatInfo	fmtInfo		= tcu::getTextureFormatInfo(mapVkFormat(format));
	const tcu::Vec4&		cScale		= fmtInfo.lookupScale;
	const tcu::Vec4&		cBias		= fmtInfo.lookupBias;
	const int				numLevels	= deLog2Floor32(de::max(de::max(m_texWidth, m_texHeight), m_texDepth))+1;

	m_texture = TestTexture3DSp(new pipeline::TestTexture3D(vk::mapVkFormat(format), m_texWidth, m_texHeight, m_texDepth));

	// Fill texture with colored grid.
	for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
	{
		const deUint32	step	= 0xff / (numLevels-1);
		const deUint32	inc		= deClamp32(step*levelNdx, 0x00, 0xff);
		const deUint32	dec		= 0xff - inc;
		const deUint32	rgb		= (inc << 16) | (dec << 8) | 0xff;
		const deUint32	color	= 0xff000000 | rgb;

		tcu::clear(m_texture->getLevel(levelNdx, 0), tcu::RGBA(color).toVec()*cScale + cBias);
	}

	m_renderer.add3DTexture(m_texture, testParameters.aspectMask);
}

Texture3DLodControlTestInstance::~Texture3DLodControlTestInstance (void)
{
}

tcu::TestStatus Texture3DLodControlTestInstance::iterate (void)
{
	const tcu::Sampler::WrapMode	wrapS			= Sampler::CLAMP_TO_EDGE;
	const tcu::Sampler::WrapMode	wrapT			= Sampler::CLAMP_TO_EDGE;
	const tcu::Sampler::WrapMode	wrapR			= Sampler::CLAMP_TO_EDGE;
	const tcu::Sampler::FilterMode	magFilter		= Sampler::NEAREST;

	const tcu::Texture3D&			refTexture		= m_texture->getTexture();
	const tcu::TextureFormat&		texFmt			= refTexture.getFormat();
	const tcu::TextureFormatInfo	fmtInfo			= tcu::getTextureFormatInfo(texFmt);
	const int						viewportWidth	= m_renderer.getRenderWidth();
	const int						viewportHeight	= m_renderer.getRenderHeight();

	tcu::Sampler					sampler			= util::createSampler(wrapS, wrapT, m_minFilter, magFilter);
	ReferenceParams					refParams		(TEXTURETYPE_3D, sampler);
	vector<float>					texCoord;
	tcu::Surface					renderedFrame	(viewportWidth, viewportHeight);

	// Viewport is divided into 4x4 grid.
	const int						gridWidth		= 4;
	const int						gridHeight		= 4;
	const int						cellWidth		= viewportWidth / gridWidth;
	const int						cellHeight		= viewportHeight / gridHeight;

	// Sampling parameters.
	refParams.sampler		= util::createSampler(wrapS, wrapT, wrapR, m_testParameters.minFilter, magFilter);
	refParams.samplerType	= getSamplerType(texFmt);
	refParams.colorBias		= fmtInfo.lookupBias;
	refParams.colorScale	= fmtInfo.lookupScale;
	refParams.maxLevel		= deLog2Floor32(de::max(de::max(m_texWidth, m_texHeight), m_texDepth));

	// Render cells.
	for (int gridY = 0; gridY < gridHeight; gridY++)
	{
		for (int gridX = 0; gridX < gridWidth; gridX++)
		{
			const int	curX		= cellWidth*gridX;
			const int	curY		= cellHeight*gridY;
			const int	curW		= gridX+1 == gridWidth ? (viewportWidth-curX) : cellWidth;
			const int	curH		= gridY+1 == gridHeight ? (viewportHeight-curY) : cellHeight;
			const int	cellNdx		= gridY*gridWidth + gridX;

			// Compute texcoord.
			getBasicTexCoord3D(texCoord, cellNdx);

			getReferenceParams(refParams,cellNdx);
			//Render
			m_renderer.setViewport((float)curX, (float)curY, (float)curW, (float)curH);
			m_renderer.getTextureBinding(0)->updateTextureViewMipLevels(refParams.baseLevel, refParams.maxLevel, refParams.imageViewMinLod);
			m_renderer.renderQuad(renderedFrame, 0, &texCoord[0], refParams);
		}
	}

	// Compare and log
	{
		const tcu::IVec4		formatBitDepth	= getTextureFormatBitDepth(mapVkFormat(VK_FORMAT_R8G8B8A8_UNORM));
		const tcu::PixelFormat	pixelFormat		(formatBitDepth[0], formatBitDepth[1], formatBitDepth[2], formatBitDepth[3]);
		const bool				isTrilinear		= m_minFilter == Sampler::NEAREST_MIPMAP_LINEAR || m_minFilter == Sampler::LINEAR_MIPMAP_LINEAR;
		tcu::LookupPrecision	lookupPrec;
		tcu::LodPrecision		lodPrec;

		lookupPrec.coordBits		= tcu::IVec3(20, 20, 20);
		lookupPrec.uvwBits			= tcu::IVec3(16, 16, 16); // Doesn't really matter since pixels are unicolored.
		lookupPrec.colorThreshold	= tcu::computeFixedPointThreshold(max(getBitsVec(pixelFormat) - (isTrilinear ? 2 : 1), tcu::IVec4(0)));
		lookupPrec.colorMask		= getCompareMask(pixelFormat);
		lodPrec.derivateBits		= 10;
		lodPrec.lodBits				= 8;

		auto compareAndLogImages = [&](tcu::ImageViewMinLodMode imageViewLodMode = tcu::IMAGEVIEWMINLODMODE_PREFERRED)
		{
			tcu::Surface			referenceFrame	(viewportWidth, viewportHeight);
			tcu::Surface			errorMask		(viewportWidth, viewportHeight);
			int						numFailedPixels = 0;

			for (int gridY = 0; gridY < gridHeight; gridY++)
			{
				for (int gridX = 0; gridX < gridWidth; gridX++)
				{
					const int	curX		= cellWidth*gridX;
					const int	curY		= cellHeight*gridY;
					const int	curW		= gridX+1 == gridWidth ? (viewportWidth-curX) : cellWidth;
					const int	curH		= gridY+1 == gridHeight ? (viewportHeight-curY) : cellHeight;
					const int	cellNdx		= gridY*gridWidth + gridX;

					getReferenceParams(refParams, cellNdx);

					refParams.imageViewMinLodMode = imageViewLodMode;

					// Compute texcoord.
					if (refParams.samplerType == glu::TextureTestUtil::SAMPLERTYPE_FETCH_FLOAT)
						getBasicTexCoord3DImageViewMinlodIntTexCoord(texCoord);
					else
						getBasicTexCoord3D(texCoord, cellNdx);

					// Render ideal result
					sampleTexture(tcu::SurfaceAccess(referenceFrame, pixelFormat, curX, curY, curW, curH),
								  refTexture, &texCoord[0], refParams);

					// Compare this cell
					numFailedPixels += computeTextureLookupDiff(tcu::getSubregion(renderedFrame.getAccess(), curX, curY, curW, curH),
																tcu::getSubregion(referenceFrame.getAccess(), curX, curY, curW, curH),
																tcu::getSubregion(errorMask.getAccess(), curX, curY, curW, curH),
																m_texture->getTexture(), &texCoord[0], refParams,
																lookupPrec, lodPrec, m_context.getTestContext().getWatchDog());
				}
			}
			if (numFailedPixels > 0)
			{
				m_context.getTestContext().getLog() << TestLog::Image("Reference", "Ideal reference", referenceFrame)
													<< TestLog::Image("ErrorMask", "Error mask", errorMask);
			}

			return numFailedPixels;
		};

		m_context.getTestContext().getLog() << TestLog::ImageSet("Result", "Verification result")
											<< TestLog::Image("Rendered", "Rendered image", renderedFrame);

		int numFailedPixels = compareAndLogImages();

		if (numFailedPixels > 0 && refParams.imageViewMinLod > 0.0f)
		{
			numFailedPixels = compareAndLogImages(tcu::IMAGEVIEWMINLODMODE_ALTERNATIVE);
		}
		m_context.getTestContext().getLog() << TestLog::EndImageSet;

		if (numFailedPixels > 0)
			m_context.getTestContext().getLog() << TestLog::Message << "ERROR: Image verification failed, found " << numFailedPixels << " invalid pixels!" << TestLog::EndMessage;

		{
			const bool isOk = numFailedPixels == 0;
			return isOk ? tcu::TestStatus::pass("pass") : tcu::TestStatus::fail("fail");
		}
	}
}

class Texture3DMinLodTestInstance : public Texture3DLodControlTestInstance
{
public:
	Texture3DMinLodTestInstance (Context& context, const Texture3DMipmapTestCaseParameters& testParameters)
		: Texture3DLodControlTestInstance(context, testParameters)
	{
	}

protected:
	void getReferenceParams (ReferenceParams& params, int cellNdx)
	{
		params.minLod = getMinLodForCell(cellNdx);
	}
};

class Texture3DMaxLodTestInstance : public Texture3DLodControlTestInstance
{
public:
	Texture3DMaxLodTestInstance (Context& context, const Texture3DMipmapTestCaseParameters& testParameters)
		: Texture3DLodControlTestInstance(context, testParameters)
	{
	}

protected:
	void getReferenceParams (ReferenceParams& params, int cellNdx)
	{
		params.maxLod = getMaxLodForCell(cellNdx);
	}
};

class Texture3DBaseLevelTestInstance : public Texture3DLodControlTestInstance
{
public:
	Texture3DBaseLevelTestInstance (Context& context, const Texture3DMipmapTestCaseParameters& testParameters)
		: Texture3DLodControlTestInstance(context, testParameters)
		,m_testParam (testParameters)
	{
	}

protected:
	const Texture3DMipmapTestCaseParameters m_testParam;

	int getBaseLevel (int cellNdx) const
	{
		const int	numLevels	= deLog2Floor32(de::max(m_texWidth, de::max(m_texHeight, m_texDepth)))+1;
		const int	baseLevel	= (deInt32Hash(cellNdx) ^ deStringHash(m_testParam.minFilterName) ^ 0x7347e9) % numLevels;

		return baseLevel;
	}

	void getReferenceParams (ReferenceParams& params, int cellNdx)
	{
		params.baseLevel = getBaseLevel(cellNdx);
	}
};

class Texture3DMaxLevelTestInstance : public Texture3DLodControlTestInstance
{
public:
	Texture3DMaxLevelTestInstance (Context& context, const Texture3DMipmapTestCaseParameters& testParameters)
		: Texture3DLodControlTestInstance(context, testParameters)
		,m_testParam (testParameters)
	{
	}

protected:
	const Texture3DMipmapTestCaseParameters m_testParam;

	int getMaxLevel (int cellNdx) const
	{
		const int	numLevels	= deLog2Floor32(de::max(m_texWidth, de::max(m_texHeight, m_texDepth)))+1;
		const int	maxLevel	= (deInt32Hash(cellNdx) ^ deStringHash(m_testParam.minFilterName) ^ 0x9111e7) % numLevels;

		return maxLevel;
	}

	void getReferenceParams (ReferenceParams& params, int cellNdx)
	{
		params.maxLevel = getMaxLevel(cellNdx);
	}
};

#ifndef CTS_USES_VULKANSC

class Texture2DImageViewMinLodTestInstance : public Texture2DLodControlTestInstance
{
public:
	Texture2DImageViewMinLodTestInstance (Context& context, const Texture2DMipmapTestCaseParameters& testParameters)
		: Texture2DLodControlTestInstance(context, testParameters)
	{
	}

protected:

	float getImageViewMinLod (int cellNdx, int baseLevel, int maxLevel) const
	{
		de::Random rnd(cellNdx + 1);

		// baselevel + 1.0 as minimum, to test that minLod is working. If we go over the maximum, use that instead.
		float minBaseLevel = de::min((float)baseLevel + 1.0f, (float)maxLevel);
		return rnd.getFloat(minBaseLevel, (float)maxLevel);
	}

	void getReferenceParams (ReferenceParams& params, int cellNdx)
	{
		params.minLod = getMinLodForCell(cellNdx);
		params.imageViewMinLod = getImageViewMinLod(cellNdx, params.baseLevel, params.maxLevel);
	}
};

class Texture2DImageViewMinLodBaseLevelTestInstance : public Texture2DLodControlTestInstance
{
public:
	Texture2DImageViewMinLodBaseLevelTestInstance (Context& context, const Texture2DMipmapTestCaseParameters& testParameters)
		: Texture2DLodControlTestInstance(context, testParameters)
		, m_testParam (testParameters)
	{
	}

protected:
	const Texture2DMipmapTestCaseParameters m_testParam;

	int getBaseLevel (int cellNdx) const
	{
		const int	numLevels	= deLog2Floor32(de::max(m_texWidth, m_texHeight))+1;
		const int	baseLevel	= (deInt32Hash(cellNdx) ^ deStringHash(m_testParam.minFilterName) ^ 0xac2f274a) % numLevels;

		return baseLevel;
	}

	float getImageViewMinLod (int cellNdx, int baseLevel, int maxLevel) const
	{
		de::Random rnd(cellNdx + 1);

		// baselevel + 1.0 as minimum, to test that minLod is working. If we go over the maximum, use that instead.
		float minValue = de::min((float)baseLevel + 1.0f, (float)maxLevel);
		return rnd.getFloat(minValue, (float)maxLevel);
	}

	void getReferenceParams (ReferenceParams& params, int cellNdx)
	{
		params.baseLevel = getBaseLevel(cellNdx);
		params.imageViewMinLod = getImageViewMinLod(cellNdx, params.baseLevel, params.maxLevel);
	}
};

class Texture3DImageViewMinLodTestInstance : public Texture3DLodControlTestInstance
{
public:
	Texture3DImageViewMinLodTestInstance (Context& context, const Texture3DMipmapTestCaseParameters& testParameters)
		: Texture3DLodControlTestInstance(context, testParameters)
	{
	}

protected:
	float getImageViewMinLod (int cellNdx, int baseLevel, int maxLevel) const
	{
		de::Random rnd(cellNdx + 1);

		// baselevel + 1.0 as minimum, to test that minLod is working. If we go over the maximum, use that instead.
		float minValue = de::min((float)baseLevel + 1.0f, (float)maxLevel);
		return rnd.getFloat(minValue, (float)maxLevel);
	}

	void getReferenceParams (ReferenceParams& params, int cellNdx)
	{
		params.minLod = getMinLodForCell(cellNdx);
		params.imageViewMinLod = getImageViewMinLod(cellNdx, params.baseLevel, params.maxLevel);
	}
};

class Texture3DImageViewMinLodBaseLevelTestInstance : public Texture3DLodControlTestInstance
{
public:
	Texture3DImageViewMinLodBaseLevelTestInstance (Context& context, const Texture3DMipmapTestCaseParameters& testParameters)
		: Texture3DLodControlTestInstance(context, testParameters)
		, m_testParam (testParameters)
	{
	}

protected:
	const Texture3DMipmapTestCaseParameters m_testParam;

	int getBaseLevel (int cellNdx) const
	{
		const int	numLevels	= deLog2Floor32(de::max(m_texWidth, de::max(m_texHeight, m_texDepth)))+1;
		const int	baseLevel	= (deInt32Hash(cellNdx) ^ deStringHash(m_testParam.minFilterName) ^ 0x7347e9) % numLevels;

		return baseLevel;
	}

	float getImageViewMinLod (int cellNdx, int baseLevel, int maxLevel) const
	{
		de::Random rnd(cellNdx + 1);

		// baselevel + 1.0 as minimum, to test that minLod is working. If we go over the maximum, use that instead.
		float minValue = de::min((float)baseLevel + 1.0f, (float)maxLevel);
		return rnd.getFloat(minValue, (float)maxLevel);
	}

	void getReferenceParams (ReferenceParams& params, int cellNdx)
	{
		params.baseLevel = getBaseLevel(cellNdx);
		params.imageViewMinLod = getImageViewMinLod(cellNdx, params.baseLevel, params.maxLevel);

	}
};

class TextureCubeImageViewMinLodTestInstance : public TextureCubeLodControlTestInstance
{
public:
	TextureCubeImageViewMinLodTestInstance (Context& context, const TextureCubeMipmapTestCaseParameters& testParameters)
		: TextureCubeLodControlTestInstance(context, testParameters)
	{
	}

protected:
	float getImageViewMinLod (int cellNdx, int baseLevel, int maxLevel) const
	{
		de::Random rnd(cellNdx + 1);

		// baselevel + 1.0 as minimum, to test that minLod is working. If we go over the maximum, use that instead.
		float minValue = de::min((float)baseLevel + 1.0f, (float)maxLevel);
		return rnd.getFloat(minValue, (float)maxLevel);
	}

	void getReferenceParams (ReferenceParams& params, int cellNdx)
	{
		params.minLod = getMinLodForCell(cellNdx);
		params.imageViewMinLod = getImageViewMinLod(cellNdx, params.baseLevel, params.maxLevel);
	}
};

class TextureCubeImageViewMinLodBaseLevelTestInstance : public TextureCubeLodControlTestInstance
{
public:
	TextureCubeImageViewMinLodBaseLevelTestInstance (Context& context, const TextureCubeMipmapTestCaseParameters& testParameters)
		: TextureCubeLodControlTestInstance(context, testParameters)
		, m_testParam (testParameters)
	{
	}

protected:
	const TextureCubeMipmapTestCaseParameters m_testParam;

	int getBaseLevel (int cellNdx) const
	{
		const int	numLevels	= deLog2Floor32(m_texSize)+1;
		const int	baseLevel	= (deInt32Hash(cellNdx) ^ deStringHash(m_testParam.minFilterName) ^ 0x23fae13) % numLevels;

		return baseLevel;
	}

	float getImageViewMinLod (int cellNdx, int baseLevel, int maxLevel) const
	{
		de::Random rnd(cellNdx + 1);

		// baselevel + 1.0 as minimum, to test that minLod is working. If we go over the maximum, use that instead.
		float minValue = de::min((float)baseLevel + 1.0f, (float)maxLevel);
		return rnd.getFloat(minValue, (float)maxLevel);
	}

	void getReferenceParams (ReferenceParams& params, int cellNdx)
	{
		params.baseLevel = getBaseLevel(cellNdx);
		params.imageViewMinLod = getImageViewMinLod(cellNdx, params.baseLevel, params.maxLevel);
	}
};

class Texture2DImageViewMinLodIntTexCoordTestInstance : public Texture2DLodControlTestInstance
{
public:
	Texture2DImageViewMinLodIntTexCoordTestInstance (Context& context, const Texture2DMipmapTestCaseParameters& testParameters)
		: Texture2DLodControlTestInstance(context, testParameters)
		, m_testParam (testParameters)
	{
	}

protected:
	const Texture2DMipmapTestCaseParameters m_testParam;

	float getImageViewMinLod (int cellNdx, int baseLevel, int maxLevel) const
	{
		de::Random rnd(cellNdx + 1);

		// baselevel + 1.0 as minimum, to test that minLod is working. If we go over the maximum, use that instead.
		float minValue = de::min((float)baseLevel + 1.0f, (float)maxLevel);
		return rnd.getFloat(minValue, (float)maxLevel);
	}

	int getLodTexelFetch (int cellNdx, int baseLevel, int maxLevel) const
	{
		de::Random rnd(cellNdx + 1);
		return rnd.getInt(baseLevel, maxLevel) - baseLevel;
	}

	void getReferenceParams (ReferenceParams& params, int cellNdx)
	{
		params.imageViewMinLod = getImageViewMinLod(cellNdx, params.baseLevel, params.maxLevel);
		params.samplerType = glu::TextureTestUtil::SAMPLERTYPE_FETCH_FLOAT;
		params.lodTexelFetch = getLodTexelFetch(cellNdx, params.baseLevel, params.maxLevel);
	}
};

class Texture2DImageViewMinLodBaseLevelIntTexCoordTestInstance : public Texture2DLodControlTestInstance
{
public:
	Texture2DImageViewMinLodBaseLevelIntTexCoordTestInstance (Context& context, const Texture2DMipmapTestCaseParameters& testParameters)
		: Texture2DLodControlTestInstance(context, testParameters)
		, m_testParam (testParameters)
	{
	}

protected:
	const Texture2DMipmapTestCaseParameters m_testParam;

	int getBaseLevel (int cellNdx) const
	{
		const int	numLevels	= deLog2Floor32(de::max(m_texWidth, m_texHeight))+1;
		const int	baseLevel	= (deInt32Hash(cellNdx) ^ deStringHash(m_testParam.minFilterName) ^ 0xac2f274a) % numLevels;

		return baseLevel;
	}

	float getImageViewMinLod (int cellNdx, int baseLevel, int maxLevel) const
	{
		de::Random rnd(cellNdx + 1);

		// baselevel + 1.0 as minimum, to test that minLod is working. If we go over the maximum, use that instead.
		float minValue = de::min((float)baseLevel + 1.0f, (float)maxLevel);
		return rnd.getFloat(minValue, (float)maxLevel);
	}

	int getLodTexelFetch (int cellNdx, int baseLevel, int maxLevel) const
	{
		de::Random rnd(cellNdx + 1);
		return rnd.getInt(baseLevel, maxLevel) - baseLevel;
	}

	void getReferenceParams (ReferenceParams& params, int cellNdx)
	{
		params.baseLevel = getBaseLevel(cellNdx);
		params.imageViewMinLod = getImageViewMinLod(cellNdx, params.baseLevel, params.maxLevel);
		params.samplerType = glu::TextureTestUtil::SAMPLERTYPE_FETCH_FLOAT;
		params.lodTexelFetch = getLodTexelFetch(cellNdx, params.baseLevel, params.maxLevel);
	}
};

class Texture2DImageViewMinLodIntTexCoordTest : public vkt::TestCase
{
public:
	Texture2DImageViewMinLodIntTexCoordTest						(tcu::TestContext&							testContext,
																 const string&								name,
																 const string&								description,
																 const Texture2DMipmapTestCaseParameters&	params);
	~Texture2DImageViewMinLodIntTexCoordTest					(void);
	void						initPrograms					(SourceCollections& sourceCollections) const;
	TestInstance*				createInstance					(Context& context) const;
	void						checkSupport					(Context& context) const;

protected:
	const Texture2DMipmapTestCaseParameters	m_params;
};

Texture2DImageViewMinLodIntTexCoordTest::Texture2DImageViewMinLodIntTexCoordTest (tcu::TestContext&							testContext,
																				  const string&								name,
																				  const string&								description,
																				  const Texture2DMipmapTestCaseParameters&	params)
	: vkt::TestCase	(testContext, name, description)
	, m_params		(params)
{
}

Texture2DImageViewMinLodIntTexCoordTest::~Texture2DImageViewMinLodIntTexCoordTest (void)
{
}

void Texture2DImageViewMinLodIntTexCoordTest::initPrograms(SourceCollections& sourceCollections) const
{
	static const char* vertShader =
		"#version 450\n"
		"layout(location = 0) in vec4 a_position;\n"
		"layout(location = 1) in vec2 a_texCoord;\n"
		"out gl_PerVertex { vec4 gl_Position; };\n"
		"\n"
		"void main (void)\n"
		"{\n"
		"	gl_Position = a_position;\n"
		"}\n";

	static const char* fragShader =
		"#version 450\n"
		"layout(location = 0) out vec4 outColor;\n"
		"layout (set=0, binding=0, std140) uniform Block \n"
		"{\n"
		"  float u_bias;\n"
		"  float u_ref;\n"
		"  vec4 u_colorScale;\n"
		"  vec4 u_colorBias;\n"
		"  int u_lod;\n"
		"};\n\n"
		"layout (set=1, binding=0) uniform sampler2D u_sampler;\n"
		"void main (void)\n"
		"{\n"
		"  ivec2 texCoord = ivec2(0,0);\n" // Sampling always from the same coord, we are only interested on the lod.
		"  outColor = texelFetch(u_sampler, texCoord, u_lod) * u_colorScale + u_colorBias;\n"
		"}\n";
	sourceCollections.glslSources.add("vertex_2D_FETCH_LOD") << glu::VertexSource(vertShader);
	sourceCollections.glslSources.add("fragment_2D_FETCH_LOD") << glu::FragmentSource(fragShader);
}

void Texture2DImageViewMinLodIntTexCoordTest::checkSupport(Context& context) const
{
	DE_ASSERT(m_params.testType > util::TextureCommonTestCaseParameters::TEST_IMAGE_VIEW_MINLOD);

	context.requireDeviceFunctionality("VK_EXT_image_view_min_lod");
	context.requireDeviceFunctionality("VK_EXT_robustness2");
	vk::VkPhysicalDeviceImageViewMinLodFeaturesEXT imageViewMinLodFeatures;
	imageViewMinLodFeatures.sType = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_VIEW_MIN_LOD_FEATURES_EXT;
	imageViewMinLodFeatures.pNext = DE_NULL;

	VkPhysicalDeviceRobustness2FeaturesEXT robustness2Features;
	robustness2Features.sType = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT;
	robustness2Features.pNext = &imageViewMinLodFeatures;
	vk::VkPhysicalDeviceFeatures2 features2;

	features2.sType = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features2.pNext = &robustness2Features;

	context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &features2);

	if (imageViewMinLodFeatures.minLod == DE_FALSE)
		TCU_THROW(NotSupportedError, "VK_EXT_image_view_min_lod minLod feature not supported");

	if (robustness2Features.robustImageAccess2 == DE_FALSE)
		TCU_THROW(NotSupportedError, "VK_EXT_robustness2 robustImageAccess2 feature not supported");
}

TestInstance* Texture2DImageViewMinLodIntTexCoordTest::createInstance(Context& context) const
{
	if (m_params.testType == util::TextureCommonTestCaseParameters::TEST_IMAGE_VIEW_MINLOD_INT_TEX_COORD)
		return new Texture2DImageViewMinLodIntTexCoordTestInstance(context, m_params);
	else
		return new Texture2DImageViewMinLodBaseLevelIntTexCoordTestInstance(context, m_params);
}

class Texture3DImageViewMinLodIntTexCoordTestInstance : public Texture3DLodControlTestInstance
{
public:
	Texture3DImageViewMinLodIntTexCoordTestInstance (Context& context, const Texture3DMipmapTestCaseParameters& testParameters)
		: Texture3DLodControlTestInstance(context, testParameters)
		, m_testParam (testParameters)
	{
	}

protected:
	const Texture3DMipmapTestCaseParameters m_testParam;

	float getImageViewMinLod (int cellNdx, int baseLevel, int maxLevel) const
	{
		de::Random rnd(cellNdx + 1);

		// baselevel + 1.0 as minimum, to test that minLod is working. If we go over the maximum, use that instead.
		float minValue = de::min((float)baseLevel + 1.0f, (float)maxLevel);
		return rnd.getFloat(minValue, (float)maxLevel);
	}

	int getLodTexelFetch (int cellNdx, int baseLevel, int maxLevel) const
	{
		de::Random rnd(cellNdx + 1);
		return rnd.getInt(baseLevel, maxLevel) - baseLevel;
	}

	void getReferenceParams (ReferenceParams& params, int cellNdx)
	{
		params.imageViewMinLod = getImageViewMinLod(cellNdx, params.baseLevel, params.maxLevel);
		params.samplerType = glu::TextureTestUtil::SAMPLERTYPE_FETCH_FLOAT;
		params.lodTexelFetch = getLodTexelFetch(cellNdx, params.baseLevel, params.maxLevel);
	}
};

class Texture3DImageViewMinLodBaseLevelIntTexCoordTestInstance : public Texture3DLodControlTestInstance
{
public:
	Texture3DImageViewMinLodBaseLevelIntTexCoordTestInstance (Context& context, const Texture3DMipmapTestCaseParameters& testParameters)
		: Texture3DLodControlTestInstance(context, testParameters)
		, m_testParam (testParameters)
	{
	}

protected:
	const Texture3DMipmapTestCaseParameters m_testParam;

	int getBaseLevel (int cellNdx) const
	{
		const int	numLevels	= deLog2Floor32(de::max(m_texWidth, de::max(m_texHeight, m_texDepth)))+1;
		const int	baseLevel	= (deInt32Hash(cellNdx) ^ deStringHash(m_testParam.minFilterName) ^ 0x7347e9) % numLevels;

		return baseLevel;
	}

	float getImageViewMinLod (int cellNdx, int baseLevel, int maxLevel) const
	{
		de::Random rnd(cellNdx + 1);

		// baselevel + 1.0 as minimum, to test that minLod is working. If we go over the maximum, use that instead.
		float minValue = de::min((float)baseLevel + 1.0f, (float)maxLevel);
		return rnd.getFloat(minValue, (float)maxLevel);
	}

	int getLodTexelFetch (int cellNdx, int baseLevel, int maxLevel) const
	{
		de::Random rnd(cellNdx + 1);
		return rnd.getInt(baseLevel, maxLevel) - baseLevel;
	}

	void getReferenceParams (ReferenceParams& params, int cellNdx)
	{
		params.baseLevel = getBaseLevel(cellNdx);
		params.imageViewMinLod = getImageViewMinLod(cellNdx, params.baseLevel, params.maxLevel);
		params.samplerType = glu::TextureTestUtil::SAMPLERTYPE_FETCH_FLOAT;
		params.lodTexelFetch = getLodTexelFetch(cellNdx, params.baseLevel, params.maxLevel);
	}
};

class Texture3DImageViewMinLodIntTexCoordTest : public vkt::TestCase
{
public:
	Texture3DImageViewMinLodIntTexCoordTest						(tcu::TestContext&							testContext,
																 const string&								name,
																 const string&								description,
																 const Texture3DMipmapTestCaseParameters&	params);
	~Texture3DImageViewMinLodIntTexCoordTest					(void);
	void						initPrograms					(SourceCollections& sourceCollections) const;
	TestInstance*				createInstance					(Context& context) const;
	void						checkSupport					(Context& context) const;

protected:
	const Texture3DMipmapTestCaseParameters	m_params;
};

Texture3DImageViewMinLodIntTexCoordTest::Texture3DImageViewMinLodIntTexCoordTest (tcu::TestContext&							testContext,
																				  const string&								name,
																				  const string&								description,
																				  const Texture3DMipmapTestCaseParameters&	params)
	: vkt::TestCase	(testContext, name, description)
	, m_params		(params)
{
}

Texture3DImageViewMinLodIntTexCoordTest::~Texture3DImageViewMinLodIntTexCoordTest (void)
{
}

void Texture3DImageViewMinLodIntTexCoordTest::initPrograms(SourceCollections& sourceCollections) const
{
	static const char* vertShader =
		"#version 450\n"
		"layout(location = 0) in vec4 a_position;\n"
		"layout(location = 1) in vec3 a_texCoord;\n"
		"out gl_PerVertex { vec4 gl_Position; };\n"
		"\n"
		"void main (void)\n"
		"{\n"
		"	gl_Position = a_position;\n"
		"}\n";

	static const char* fragShader =
		"#version 450\n"
		"layout(location = 0) out vec4 outColor;\n"
		"layout (set=0, binding=0, std140) uniform Block \n"
		"{\n"
		"  float u_bias;\n"
		"  float u_ref;\n"
		"  vec4 u_colorScale;\n"
		"  vec4 u_colorBias;\n"
		"  int u_lod;\n"
		"};\n\n"
		"layout (set=1, binding=0) uniform sampler3D u_sampler;\n"
		"void main (void)\n"
		"{\n"
		"  ivec3 texCoord = ivec3(0,0,0);\n" // Sampling always from the same coord, we are only interested on the lod.
		"  outColor = texelFetch(u_sampler, texCoord, u_lod) * u_colorScale + u_colorBias;\n"
		"}\n";
	sourceCollections.glslSources.add("vertex_3D_FETCH_LOD") << glu::VertexSource(vertShader);
	sourceCollections.glslSources.add("fragment_3D_FETCH_LOD") << glu::FragmentSource(fragShader);
}

void Texture3DImageViewMinLodIntTexCoordTest::checkSupport(Context& context) const
{
	DE_ASSERT(m_params.testType > util::TextureCommonTestCaseParameters::TEST_IMAGE_VIEW_MINLOD);

	context.requireDeviceFunctionality("VK_EXT_image_view_min_lod");
	context.requireDeviceFunctionality("VK_EXT_robustness2");
	vk::VkPhysicalDeviceImageViewMinLodFeaturesEXT imageViewMinLodFeatures;
	imageViewMinLodFeatures.sType = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_VIEW_MIN_LOD_FEATURES_EXT;
	imageViewMinLodFeatures.pNext = DE_NULL;

	VkPhysicalDeviceRobustness2FeaturesEXT robustness2Features;
	robustness2Features.sType = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT;
	robustness2Features.pNext = &imageViewMinLodFeatures;
	vk::VkPhysicalDeviceFeatures2 features2;

	features2.sType = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features2.pNext = &robustness2Features;

	context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &features2);

	if (imageViewMinLodFeatures.minLod == DE_FALSE)
		TCU_THROW(NotSupportedError, "VK_EXT_image_view_min_lod minLod feature not supported");

	if (robustness2Features.robustImageAccess2 == DE_FALSE)
		TCU_THROW(NotSupportedError, "VK_EXT_robustness2 robustImageAccess2 feature not supported");
}

TestInstance* Texture3DImageViewMinLodIntTexCoordTest::createInstance(Context& context) const
{
	if (m_params.testType == util::TextureCommonTestCaseParameters::TEST_IMAGE_VIEW_MINLOD_INT_TEX_COORD)
		return new Texture3DImageViewMinLodIntTexCoordTestInstance(context, m_params);
	else
	   return new Texture3DImageViewMinLodBaseLevelIntTexCoordTestInstance(context, m_params);
}

// Texture gather tests.
enum class GatherMinLod
{
	MINLOD_0_1,		// 0.1
	MINLOD_1_1,		// 1.1
};

struct GatherParams
{
	uint32_t		randomSeed;	// Seed for the pseudorandom number generator.
	GatherMinLod	minLod;		// Idea: make it 0.1 or 1.1
	int				component;	// 0, 1, 2, 3 for the gather operation.

	float getNumericMinLod (void) const
	{
		float lod = 0.0f;

		switch (minLod)
		{
		case GatherMinLod::MINLOD_0_1:	lod = 0.1f; break;
		case GatherMinLod::MINLOD_1_1:	lod = 1.1f; break;
		default: DE_ASSERT(false); break;
		}

		return lod;
	}

	uint32_t getMinLodInteger (void) const
	{
		uint32_t lod = 0u;

		switch (minLod)
		{
		case GatherMinLod::MINLOD_0_1:	lod = 0u; break;
		case GatherMinLod::MINLOD_1_1:	lod = 1u; break;
		default: DE_ASSERT(false); break;
		}

		return lod;
	}

	bool needsRobustness2 (void) const
	{
		return (getNumericMinLod() >= 1.0f);
	}
};

class TextureGatherMinLodTest : public vkt::TestCase
{
public:
					TextureGatherMinLodTest		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const GatherParams& params)
						: vkt::TestCase	(testCtx, name, description)
						, m_params		(params)
						{}
	virtual			~TextureGatherMinLodTest	(void) {}

	void			initPrograms				(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance				(Context& context) const override;
	void			checkSupport				(Context& context) const override;

protected:
	GatherParams	m_params;
};

class TextureGatherMinLodInstance : public vkt::TestInstance
{
public:
					TextureGatherMinLodInstance		(Context& context, const GatherParams& params)
						: vkt::TestInstance	(context)
						, m_params			(params)
						{}
	virtual			~TextureGatherMinLodInstance	(void) {}

	tcu::TestStatus	iterate							(void) override;

protected:
	GatherParams	m_params;
};

// Test idea: create texture with 3 levels, each of them having a unique nonzero color. Render gathering the color from a fixed
// position in that texture (center point). Use the minLod parameter when creating the view to control which one should be the
// output color. If minLod is 0.1, minLodInteger should be 0 and gathering from the base level is defined, so we should get the
// output color from the base level. If minLod is 1.1, gathering texels from the base level requires robustness2 and will result in
// zeros instead of the color from levels 0 or 1.
void TextureGatherMinLodTest::initPrograms (vk::SourceCollections &programCollection) const
{
	// Full screen triangle covering the whole viewport.
	std::ostringstream vert;
	vert
		<< "#version 450\n"
		<< "\n"
		<< "vec2 positions[3] = vec2[](\n"
		<< "    vec2(-1.0, -1.0),\n"
		<< "    vec2(3.0, -1.0),\n"
		<< "    vec2(-1.0, 3.0)\n"
		<< ");\n"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);\n"
		<< "}\n"
		;
	programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

	std::ostringstream frag;
	frag
		<< "#version 450\n"
		<< "\n"
		<< "layout (location=0) out vec4 outColor;\n"
		<< "layout (set=0, binding=0) uniform sampler2D u_sampler;\n"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "    const vec2 gatherCoords = vec2(0.5, 0.5);\n"
		<< "    const vec4 gatherRes = textureGather(u_sampler, gatherCoords, " << m_params.component << ");\n"
		<< "    outColor = vec4(gatherRes.xyz, 1.0);\n"
		<< "}\n";
		;
	programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

void TextureGatherMinLodTest::checkSupport (Context& context) const
{
	context.requireInstanceFunctionality("VK_KHR_get_physical_device_properties2");
	context.requireDeviceFunctionality("VK_EXT_image_view_min_lod");

	if (m_params.needsRobustness2())
	{
		context.requireDeviceFunctionality("VK_EXT_robustness2");

		VkPhysicalDeviceRobustness2FeaturesEXT	robustness2Features	= initVulkanStructure();
		VkPhysicalDeviceFeatures2				features2			= initVulkanStructure(&robustness2Features);

		context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &features2);

		if (robustness2Features.robustImageAccess2 == DE_FALSE)
			TCU_THROW(NotSupportedError, "robustImageAccess2 not supported");
	}
}

TestInstance* TextureGatherMinLodTest::createInstance (Context& context) const
{
	return new TextureGatherMinLodInstance(context, m_params);
}

// Device helper: this is needed because we sometimes need a custom device with robustImageAccess2.
class DeviceHelper
{
public:
	virtual ~DeviceHelper () {}
	virtual const DeviceInterface&	getDeviceInterface	(void) const = 0;
	virtual VkDevice				getDevice			(void) const = 0;
	virtual uint32_t				getQueueFamilyIndex	(void) const = 0;
	virtual VkQueue					getQueue			(void) const = 0;
	virtual Allocator&				getAllocator		(void) const = 0;
};

// This one just reuses the default device from the context.
class ContextDeviceHelper : public DeviceHelper
{
public:
	ContextDeviceHelper (Context& context)
		: m_deviceInterface		(context.getDeviceInterface())
		, m_device				(context.getDevice())
		, m_queueFamilyIndex	(context.getUniversalQueueFamilyIndex())
		, m_queue				(context.getUniversalQueue())
		, m_allocator			(context.getDefaultAllocator())
		{}

	virtual ~ContextDeviceHelper () {}

	const DeviceInterface&	getDeviceInterface	(void) const override	{ return m_deviceInterface;		}
	VkDevice				getDevice			(void) const override	{ return m_device;				}
	uint32_t				getQueueFamilyIndex	(void) const override	{ return m_queueFamilyIndex;	}
	VkQueue					getQueue			(void) const override	{ return m_queue;				}
	Allocator&				getAllocator		(void) const override	{ return m_allocator;			}

protected:
	const DeviceInterface&	m_deviceInterface;
	const VkDevice			m_device;
	const uint32_t			m_queueFamilyIndex;
	const VkQueue			m_queue;
	Allocator&				m_allocator;
};

// This one creates a new device with robustImageAccess2.
class RobustImageAccess2DeviceHelper : public DeviceHelper
{
public:
	RobustImageAccess2DeviceHelper (Context& context)
	{
		const auto&	vkp				= context.getPlatformInterface();
		const auto&	vki				= context.getInstanceInterface();
		const auto	instance		= context.getInstance();
		const auto	physicalDevice	= context.getPhysicalDevice();
		const auto	queuePriority	= 1.0f;

		// Queue index first.
		m_queueFamilyIndex = context.getUniversalQueueFamilyIndex();

		// Create a universal queue that supports graphics and compute.
		const VkDeviceQueueCreateInfo queueParams =
		{
			VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,	// VkStructureType				sType;
			DE_NULL,									// const void*					pNext;
			0u,											// VkDeviceQueueCreateFlags		flags;
			m_queueFamilyIndex,							// deUint32						queueFamilyIndex;
			1u,											// deUint32						queueCount;
			&queuePriority								// const float*					pQueuePriorities;
		};

		const char* extensions[] =
		{
			"VK_EXT_robustness2",
		};

		VkPhysicalDeviceImageViewMinLodFeaturesEXT	minLodfeatures		= initVulkanStructure();
		VkPhysicalDeviceRobustness2FeaturesEXT		robustness2Features	= initVulkanStructure(&minLodfeatures);
		VkPhysicalDeviceFeatures2					features2			= initVulkanStructure(&robustness2Features);

		vki.getPhysicalDeviceFeatures2(physicalDevice, &features2);

		const VkDeviceCreateInfo deviceCreateInfo =
		{
			VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,					//sType;
			&features2,												//pNext;
			0u,														//flags
			1u,														//queueRecordCount;
			&queueParams,											//pRequestedQueues;
			0u,														//layerCount;
			nullptr,												//ppEnabledLayerNames;
			static_cast<uint32_t>(de::arrayLength(extensions)),		// deUint32							enabledExtensionCount;
			extensions,												// const char* const*				ppEnabledExtensionNames;
			nullptr,												//pEnabledFeatures;
		};

		m_device	= createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(), vkp, instance, vki, physicalDevice, &deviceCreateInfo);
		m_vkd		.reset(new DeviceDriver(vkp, instance, m_device.get(), context.getUsedApiVersion()));
		m_queue		= getDeviceQueue(*m_vkd, *m_device, m_queueFamilyIndex, 0u);
		m_allocator	.reset(new SimpleAllocator(*m_vkd, m_device.get(), getPhysicalDeviceMemoryProperties(vki, physicalDevice)));
	}

	virtual ~RobustImageAccess2DeviceHelper () {}

	const DeviceInterface&	getDeviceInterface	(void) const override	{ return *m_vkd;				}
	VkDevice				getDevice			(void) const override	{ return m_device.get();		}
	uint32_t				getQueueFamilyIndex	(void) const override	{ return m_queueFamilyIndex;	}
	VkQueue					getQueue			(void) const override	{ return m_queue;				}
	Allocator&				getAllocator		(void) const override	{ return *m_allocator;			}

protected:
	Move<VkDevice>						m_device;
	std::unique_ptr<DeviceDriver>		m_vkd;
	deUint32							m_queueFamilyIndex;
	VkQueue								m_queue;
	std::unique_ptr<SimpleAllocator>	m_allocator;
};

std::unique_ptr<DeviceHelper> g_robustness2DeviceHelper;
std::unique_ptr<DeviceHelper> g_contextDeviceHelper;

DeviceHelper& getDeviceHelper (Context& context, bool needsRobustness2)
{
	if (needsRobustness2)
	{
		if (!g_robustness2DeviceHelper)
			g_robustness2DeviceHelper.reset(new RobustImageAccess2DeviceHelper(context));
		return *g_robustness2DeviceHelper;
	}

	if (!g_contextDeviceHelper)
		g_contextDeviceHelper.reset(new ContextDeviceHelper(context));
	return *g_contextDeviceHelper;
}

// Cleanup function for the test group.
void destroyDeviceHelpers (tcu::TestCaseGroup*)
{
	g_robustness2DeviceHelper.reset(nullptr);
	g_contextDeviceHelper.reset(nullptr);
}

tcu::TestStatus TextureGatherMinLodInstance::iterate (void)
{
	const auto&	deviceHelper	= getDeviceHelper(m_context, m_params.needsRobustness2());
	const auto&	vkd				= deviceHelper.getDeviceInterface();
	const auto	device			= deviceHelper.getDevice();
	const auto	queueIndex		= deviceHelper.getQueueFamilyIndex();
	const auto	queue			= deviceHelper.getQueue();
	auto&		alloc			= deviceHelper.getAllocator();

	const auto			imageFormat		= VK_FORMAT_R8G8B8A8_UNORM;
	const auto			tcuFormat		= mapVkFormat(imageFormat);
	const auto			colorExtent		= makeExtent3D(1u, 1u, 1u);
	const tcu::IVec3	iColorExtent	(static_cast<int>(colorExtent.width), static_cast<int>(colorExtent.height), static_cast<int>(colorExtent.depth));
	const auto			texExtent		= makeExtent3D(8u, 8u, 1u);
	const auto			texMipLevels	= 3u;
	const auto			colorUsage		= (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	const auto			texUsage		= (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	const auto			minLodF			= m_params.getNumericMinLod();
	const auto			minLodU			= m_params.getMinLodInteger();
	const tcu::Vec4		clearColor		(0.0f, 0.0f, 0.0f, 1.0f);

	// Color attachment: a simple 1x1 image.
	const VkImageCreateInfo colorCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
		nullptr,								//	const void*				pNext;
		0u,										//	VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
		imageFormat,							//	VkFormat				format;
		colorExtent,							//	VkExtent3D				extent;
		1u,										//	uint32_t				mipLevels;
		1u,										//	uint32_t				arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
		colorUsage,								//	VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
		0u,										//	uint32_t				queueFamilyIndexCount;
		nullptr,								//	const uint32_t*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
	};

	ImageWithMemory	colorBuffer		(vkd, device, alloc, colorCreateInfo, MemoryRequirement::Any);
	const auto		colorSRR		= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const auto		colorSRL		= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const auto		colorBufferView	= makeImageView(vkd, device, colorBuffer.get(), VK_IMAGE_VIEW_TYPE_2D, imageFormat, colorSRR);

	// Texture: an 8x8 image with several mip levels.
	const VkImageCreateInfo texCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
		nullptr,								//	const void*				pNext;
		0u,										//	VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
		imageFormat,							//	VkFormat				format;
		texExtent,								//	VkExtent3D				extent;
		texMipLevels,							//	uint32_t				mipLevels;
		1u,										//	uint32_t				arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
		texUsage,								//	VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
		0u,										//	uint32_t				queueFamilyIndexCount;
		nullptr,								//	const uint32_t*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
	};

	ImageWithMemory texture (vkd, device, alloc, texCreateInfo, MemoryRequirement::Any);
	const auto texSRR = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, texMipLevels, 0u, 1u);

	DE_ASSERT(texMipLevels > 0u);
	DE_ASSERT(minLodU < texMipLevels);

	const VkImageViewMinLodCreateInfoEXT texMinLodInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_MIN_LOD_CREATE_INFO_EXT,	//	VkStructureType	sType;
		nullptr,												//	const void*		pNext;
		minLodF,												//	float			minLod;
	};

	const VkImageViewCreateInfo texViewCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	//	VkStructureType			sType;
		&texMinLodInfo,								//	const void*				pNext;
		0u,											//	VkImageViewCreateFlags	flags;
		texture.get(),								//	VkImage					image;
		VK_IMAGE_VIEW_TYPE_2D,						//	VkImageViewType			viewType;
		imageFormat,								//	VkFormat				format;
		makeComponentMappingRGBA(),					//	VkComponentMapping		components;
		texSRR,										//	VkImageSubresourceRange	subresourceRange;
	};

	const auto texView = createImageView(vkd, device, &texViewCreateInfo);

	// Verification buffer for the color attachment.
	const auto			verifBufferSize			= static_cast<VkDeviceSize>(iColorExtent.x() * iColorExtent.y() * iColorExtent.z() * tcu::getPixelSize(tcuFormat));
	const auto			verifBufferCreateInfo	= makeBufferCreateInfo(verifBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	BufferWithMemory	verifBuffer				(vkd, device, alloc, verifBufferCreateInfo, MemoryRequirement::HostVisible);
	auto&				verifBufferAlloc		= verifBuffer.getAllocation();
	void*				verifBufferData			= verifBufferAlloc.getHostPtr();

	// Descriptor set layout.
	DescriptorSetLayoutBuilder setLayoutBuilder;
	setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	const auto setLayout = setLayoutBuilder.build(vkd, device);

	// Pipeline layout.
	const auto pipelineLayout = makePipelineLayout(vkd, device, setLayout.get());

	// Sampler.
	const VkSamplerCreateInfo samplerCreateInfo =
	{
		VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,		//	VkStructureType			sType;
		nullptr,									//	const void*				pNext;
		0u,											//	VkSamplerCreateFlags	flags;
		VK_FILTER_LINEAR,							//	VkFilter				magFilter;
		VK_FILTER_LINEAR,							//	VkFilter				minFilter;
		VK_SAMPLER_MIPMAP_MODE_LINEAR,				//	VkSamplerMipmapMode		mipmapMode;
		VK_SAMPLER_ADDRESS_MODE_REPEAT,				//	VkSamplerAddressMode	addressModeU;
		VK_SAMPLER_ADDRESS_MODE_REPEAT,				//	VkSamplerAddressMode	addressModeV;
		VK_SAMPLER_ADDRESS_MODE_REPEAT,				//	VkSamplerAddressMode	addressModeW;
		0.0f,										//	float					mipLodBias;
		VK_FALSE,									//	VkBool32				anisotropyEnable;
		0.0f,										//	float					maxAnisotropy;
		VK_FALSE,									//	VkBool32				compareEnable;
		VK_COMPARE_OP_NEVER,						//	VkCompareOp				compareOp;
		0.0f,										//	float					minLod;
		static_cast<float>(texMipLevels),			//	float					maxLod;
		VK_BORDER_COLOR_INT_TRANSPARENT_BLACK,		//	VkBorderColor			borderColor;
		VK_FALSE,									//	VkBool32				unnormalizedCoordinates;
	};
	const auto sampler = createSampler(vkd, device, &samplerCreateInfo);

	// Descriptor pool and set.
	DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	const auto descriptorPool	= poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	const auto descriptorSet	= makeDescriptorSet(vkd, device, descriptorPool.get(), setLayout.get());

	// Update descriptor set.
	DescriptorSetUpdateBuilder setUpdateBuilder;
	const auto combinedSamplerInfo = makeDescriptorImageInfo(sampler.get(), texView.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	setUpdateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &combinedSamplerInfo);
	setUpdateBuilder.update(vkd, device);

	// Render pass and framebuffer.
	const auto renderPass	= makeRenderPass(vkd, device, imageFormat);
	const auto framebuffer	= makeFramebuffer(vkd, device, renderPass.get(), colorBufferView.get(), colorExtent.width, colorExtent.height);

	// Shader modules.
	const auto&	binaries	= m_context.getBinaryCollection();
	const auto	vertModule	= createShaderModule(vkd, device, binaries.get("vert"));
	const auto	fragModule	= createShaderModule(vkd, device, binaries.get("frag"));

	// Viewports and scissors.
	std::vector<VkViewport>	viewports	(1u, makeViewport(colorExtent));
	std::vector<VkRect2D>	scissors	(1u, makeRect2D(colorExtent));

	// Pipeline.
	const VkPipelineVertexInputStateCreateInfo vertexInputState = initVulkanStructure();

	const auto pipeline = makeGraphicsPipeline(vkd, device, pipelineLayout.get(),
		vertModule.get(), DE_NULL, DE_NULL, DE_NULL, fragModule.get(),
		renderPass.get(), viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		0u/*subpass*/, 0u/*patchControlPoints*/, &vertexInputState);

	// Command pool and buffer.
	const auto cmdPool		= makeCommandPool(vkd, device, queueIndex);
	const auto cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	beginCommandBuffer(vkd, cmdBuffer);

	// Move the whole texture to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL.
	const auto preClearBarrier = makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texture.get(), texSRR);
	cmdPipelineImageMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &preClearBarrier);

	// Fill each texture mip level with a different pseudorandom nonzero color.
	std::vector<tcu::Vec4> levelColors;
	de::Random rnd (m_params.randomSeed);

	levelColors.reserve(texMipLevels);

	const auto minColor = 0.004f; // Slightly above 1/255.
	const auto maxColor = 1.0f;

	for (uint32_t level = 0u; level < texMipLevels; ++level)
	{
		const auto r			= rnd.getFloat(minColor, maxColor);
		const auto g			= rnd.getFloat(minColor, maxColor);
		const auto b			= rnd.getFloat(minColor, maxColor);
		const auto a			= rnd.getFloat(minColor, maxColor);
		const auto levelColor	= makeClearValueColorF32(r, g, b, a).color;
		const auto levelRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, level, 1u, 0u, 1u);

		levelColors.emplace_back(r, g, b, a);
		vkd.cmdClearColorImage(cmdBuffer, texture.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &levelColor, 1u, &levelRange);
	}

	// Move the whole texture to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	const auto postClearBarrier = makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, texture.get(), texSRR);
	cmdPipelineImageMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, &postClearBarrier);

	beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0u), clearColor);
	vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0u, 1u, &descriptorSet.get(), 0u, nullptr);
	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
	vkd.cmdDraw(cmdBuffer, 3u, 1u, 0u, 0u); // This has to match the vertex shader.
	endRenderPass(vkd, cmdBuffer);

	// Copy color buffer to verification buffer.
	const auto postColorBarier = makeImageMemoryBarrier(
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		colorBuffer.get(), colorSRR);
	cmdPipelineImageMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &postColorBarier);

	const auto copyRegion = makeBufferImageCopy(colorExtent, colorSRL);
	vkd.cmdCopyImageToBuffer(cmdBuffer, colorBuffer.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, verifBuffer.get(), 1u, &copyRegion);

	const auto preHostBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	cmdPipelineMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, &preHostBarrier);

	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Verify color buffer.
	invalidateAlloc(vkd, device, verifBufferAlloc);
	tcu::ConstPixelBufferAccess resultAccess (tcuFormat, iColorExtent, verifBufferData);

	const auto		resultColor		= resultAccess.getPixel(0, 0);
	const auto		srcLevelColor	= levelColors.at(minLodU);
	const auto		compColor		= srcLevelColor[m_params.component];
	const auto		expectedColor	= (m_params.needsRobustness2() // This has to match the fragment shader.
									? tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f)
									: tcu::Vec4(compColor, compColor, compColor, 1.0f));
	const auto		threshold		= (m_params.needsRobustness2()
									? 0.0f
									: 0.005f); // 1/255 < 0.005 < 2/255

	const auto		diff			= abs(resultColor - expectedColor);
	const tcu::Vec4	thresholdVec	(threshold, threshold, threshold, 0.0f);
	const auto		thresholdMet	= tcu::lessThanEqual(diff, thresholdVec);

	if (!tcu::boolAll(thresholdMet))
	{
		std::ostringstream msg;
		msg << "Unexpected output buffer color: expected " << expectedColor << " but found " << resultColor << " [diff=" << diff << "]";
		TCU_FAIL(msg.str());
	}

	return tcu::TestStatus::pass("Pass");
}

#endif // CTS_USES_VULKANSC

} // anonymous

#ifndef CTS_USES_VULKANSC

namespace util {

template <>
void checkTextureSupport (Context& context, const Texture2DMipmapTestCaseParameters& testParameters)
{
	if (testParameters.testType != TextureCommonTestCaseParameters::TEST_NORMAL)
	{
		context.requireDeviceFunctionality("VK_EXT_image_view_min_lod");
		vk::VkPhysicalDeviceImageViewMinLodFeaturesEXT imageViewMinLodFeatures;
		imageViewMinLodFeatures.sType = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_VIEW_MIN_LOD_FEATURES_EXT;
		imageViewMinLodFeatures.pNext = DE_NULL;

		vk::VkPhysicalDeviceFeatures2 features2;

		features2.sType = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		features2.pNext = &imageViewMinLodFeatures;

		context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &features2);

		if (imageViewMinLodFeatures.minLod == DE_FALSE)
			TCU_THROW(NotSupportedError, "VK_EXT_image_view_min_lod minLod feature not supported");
	}
}

template <>
void checkTextureSupport (Context& context, const TextureCubeMipmapTestCaseParameters& testParameters)
{
	if (testParameters.testType != TextureCommonTestCaseParameters::TEST_NORMAL)
	{
		context.requireDeviceFunctionality("VK_EXT_image_view_min_lod");
		vk::VkPhysicalDeviceImageViewMinLodFeaturesEXT imageViewMinLodFeatures;
		imageViewMinLodFeatures.sType = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_VIEW_MIN_LOD_FEATURES_EXT;
		imageViewMinLodFeatures.pNext = DE_NULL;

		vk::VkPhysicalDeviceFeatures2 features2;

		features2.sType = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		features2.pNext = &imageViewMinLodFeatures;

		context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &features2);

		if (imageViewMinLodFeatures.minLod == DE_FALSE)
			TCU_THROW(NotSupportedError, "VK_EXT_image_view_min_lod minLod feature not supported");
	}
}

template <>
void checkTextureSupport (Context& context, const Texture3DMipmapTestCaseParameters& testParameters)
{
	if (testParameters.testType != TextureCommonTestCaseParameters::TEST_NORMAL)
	{
		context.requireDeviceFunctionality("VK_EXT_image_view_min_lod");
		vk::VkPhysicalDeviceImageViewMinLodFeaturesEXT imageViewMinLodFeatures;
		imageViewMinLodFeatures.sType = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_VIEW_MIN_LOD_FEATURES_EXT;
		imageViewMinLodFeatures.pNext = DE_NULL;

		vk::VkPhysicalDeviceFeatures2 features2;

		features2.sType = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		features2.pNext = &imageViewMinLodFeatures;

		context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &features2);

		if (imageViewMinLodFeatures.minLod == DE_FALSE)
			TCU_THROW(NotSupportedError, "VK_EXT_image_view_min_lod minLod feature not supported");
	}
}

} // util

void populateMinLodGatherGroup (tcu::TestCaseGroup* minLodGatherGroup)
{
	using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

	const struct
	{
		GatherMinLod	minLod;
		const char*		name;
	} GatherMinLodCases[] =
	{
		{ GatherMinLod::MINLOD_0_1,		"minlod_0_1"	},
		{ GatherMinLod::MINLOD_1_1,		"minlod_1_1"	},
	};

	const struct
	{
		int				component;
		const char*		name;
	} ComponentCases[] =
	{
		{ 0,	"component_0"	},
		{ 1,	"component_1"	},
		{ 2,	"component_2"	},
		{ 3,	"component_3"	},
	};

	auto& testCtx = minLodGatherGroup->getTestContext();

	for (const auto& gatherMinLodCase : GatherMinLodCases)
	{
		GroupPtr minLodGroup (new tcu::TestCaseGroup(testCtx, gatherMinLodCase.name, ""));

		for (const auto& componentCase : ComponentCases)
		{
			const uint32_t seed	= (static_cast<uint32_t>(gatherMinLodCase.minLod) + 1000u) * 1000u
								+ (static_cast<uint32_t>(componentCase.component) + 1000u);

			GatherParams params;
			params.randomSeed	= seed;
			params.minLod		= gatherMinLodCase.minLod;
			params.component	= componentCase.component;

			minLodGroup->addChild(new TextureGatherMinLodTest(testCtx, componentCase.name, "", params));
		}

		minLodGatherGroup->addChild(minLodGroup.release());
	}
}

#endif // CTS_USES_VULKANSC

void populateTextureMipmappingTests (tcu::TestCaseGroup* textureMipmappingTests)
{
	tcu::TestContext&	testCtx		= textureMipmappingTests->getTestContext();

	static const struct
	{
		const char*				name;
		const Sampler::WrapMode	mode;
	} wrapModes[] =
	{
		{ "clamp",		Sampler::CLAMP_TO_EDGE		},
		{ "repeat",		Sampler::REPEAT_GL			},
		{ "mirror",		Sampler::MIRRORED_REPEAT_GL	}
	};

	static const struct
	{
		const char*					name;
		const Sampler::FilterMode	mode;
	} minFilterModes[] =
	{
		{ "nearest_nearest",	Sampler::NEAREST_MIPMAP_NEAREST	},
		{ "linear_nearest",		Sampler::LINEAR_MIPMAP_NEAREST	},
		{ "nearest_linear",		Sampler::NEAREST_MIPMAP_LINEAR	},
		{ "linear_linear",		Sampler::LINEAR_MIPMAP_LINEAR	}
	};

	static const struct
	{
		const char*					name;
		const Sampler::FilterMode	mode;
	} magFilterModes[] =
	{
		{ "nearest",	Sampler::NEAREST},
		{ "linear",		Sampler::LINEAR}
	};


	static const struct
	{
		const CoordType		type;
		const char*			name;
		const char*			desc;
	} coordTypes[] =
	{
		{ COORDTYPE_BASIC,		"basic",		"Mipmapping with translated and scaled coordinates" },
		{ COORDTYPE_AFFINE,		"affine",		"Mipmapping with affine coordinate transform"		},
		{ COORDTYPE_PROJECTED,	"projected",	"Mipmapping with perspective projection"			}
	};

	static const struct
	{
		const char*		name;
		const int		width;
		const int		height;
	} tex2DSizes[] =
	{
		{ DE_NULL,		64, 64 }, // Default.
		{ "npot",		63, 57 },
		{ "non_square",	32, 64 }
	};

	static const struct
	{
		const char*		name;
		const int		width;
		const int		height;
		const int		depth;
	} tex3DSizes[] =
	{
		{ DE_NULL,		32, 32, 32 }, // Default.
		{ "npot",		33, 29, 27 }
	};

	const int cubeMapSize = 64;

	static const struct
	{
		const CoordType		type;
		const char*			name;
		const char*			desc;
	} cubeCoordTypes[] =
	{
		{ COORDTYPE_BASIC,		"basic",		"Mipmapping with translated and scaled coordinates" },
		{ COORDTYPE_PROJECTED,	"projected",	"Mipmapping with perspective projection"			},
		{ COORDTYPE_BASIC_BIAS,	"bias",			"User-supplied bias value"							}
	};

	// 2D cases.
	{
		de::MovePtr<tcu::TestCaseGroup>	group2D				(new tcu::TestCaseGroup(testCtx, "2d", "2D Mipmap Filtering"));

		de::MovePtr<tcu::TestCaseGroup>	biasGroup2D			(new tcu::TestCaseGroup(testCtx, "bias", "User-supplied bias value"));
		de::MovePtr<tcu::TestCaseGroup>	minLodGroup2D		(new tcu::TestCaseGroup(testCtx, "min_lod", "Lod control: min lod"));
		de::MovePtr<tcu::TestCaseGroup>	maxLodGroup2D		(new tcu::TestCaseGroup(testCtx, "max_lod", "Lod control: max lod"));
		de::MovePtr<tcu::TestCaseGroup>	baseLevelGroup2D	(new tcu::TestCaseGroup(testCtx, "base_level", "Base level"));
		de::MovePtr<tcu::TestCaseGroup>	maxLevelGroup2D		(new tcu::TestCaseGroup(testCtx, "max_level", "Max level"));

#ifndef CTS_USES_VULKANSC
		de::MovePtr<tcu::TestCaseGroup> imageViewMinLodExtGroup2D	(new tcu::TestCaseGroup(testCtx, "image_view_min_lod", "VK_EXT_image_view_min_lod tests"));
		de::MovePtr<tcu::TestCaseGroup> imageViewMinLodGroup2D	(new tcu::TestCaseGroup(testCtx, "min_lod", "ImageView's minLod"));
		de::MovePtr<tcu::TestCaseGroup> imageViewMinLodBaseLevelGroup2D	(new tcu::TestCaseGroup(testCtx, "base_level", "ImageView's minLod with base level different than one"));
#endif // CTS_USES_VULKANSC

		for (int coordType = 0; coordType < DE_LENGTH_OF_ARRAY(coordTypes); coordType++)
		{
			de::MovePtr<tcu::TestCaseGroup>	coordTypeGroup		(new tcu::TestCaseGroup(testCtx, coordTypes[coordType].name, coordTypes[coordType].desc));

			for (int minFilter = 0; minFilter < DE_LENGTH_OF_ARRAY(minFilterModes); minFilter++)
			{
				for (int wrapMode = 0; wrapMode < DE_LENGTH_OF_ARRAY(wrapModes); wrapMode++)
				{
					// Add non_square variants to basic cases only.
					int sizeEnd = coordTypes[coordType].type == COORDTYPE_BASIC ? DE_LENGTH_OF_ARRAY(tex2DSizes) : 1;

					for (int size = 0; size < sizeEnd; size++)
					{
						Texture2DMipmapTestCaseParameters	testParameters;

						testParameters.coordType	= coordTypes[coordType].type;
						testParameters.minFilter	= minFilterModes[minFilter].mode;
						testParameters.wrapS		= wrapModes[wrapMode].mode;
						testParameters.wrapT		= wrapModes[wrapMode].mode;
						testParameters.format		= VK_FORMAT_R8G8B8A8_UNORM;
						testParameters.width		= tex2DSizes[size].width;
						testParameters.height		= tex2DSizes[size].height;
						testParameters.aspectMask	= VK_IMAGE_ASPECT_COLOR_BIT;
						testParameters.programs.push_back(PROGRAM_2D_FLOAT);

						std::ostringstream name;
						name << minFilterModes[minFilter].name
							 << "_" << wrapModes[wrapMode].name;

						if (tex2DSizes[size].name)
							name << "_" << tex2DSizes[size].name;

						coordTypeGroup->addChild(new TextureTestCase<Texture2DMipmapTestInstance>(testCtx, name.str().c_str(), "", testParameters));
					}
				}
			}

			group2D->addChild(coordTypeGroup.release());
		}

		// 2D bias variants.
		{
			for (int minFilter = 0; minFilter < DE_LENGTH_OF_ARRAY(minFilterModes); minFilter++)
			{
				Texture2DMipmapTestCaseParameters	testParameters;

				testParameters.coordType	= COORDTYPE_BASIC_BIAS;
				testParameters.minFilter	= minFilterModes[minFilter].mode;
				testParameters.magFilter	= minFilterModes[minFilter].mode;
				testParameters.wrapS		= Sampler::REPEAT_GL;
				testParameters.wrapT		= Sampler::REPEAT_GL;
				testParameters.format		= VK_FORMAT_R8G8B8A8_UNORM;
				testParameters.width		= tex2DSizes[0].width;
				testParameters.height		= tex2DSizes[0].height;
				testParameters.aspectMask	= VK_IMAGE_ASPECT_COLOR_BIT;
				testParameters.programs.push_back(PROGRAM_2D_FLOAT_BIAS);

				std::ostringstream name;
				name << minFilterModes[minFilter].name;

				biasGroup2D->addChild(new TextureTestCase<Texture2DMipmapTestInstance>(testCtx, name.str().c_str(), "", testParameters));
			}
		}

		// 2D LOD controls.
		{
			// MIN_LOD
			for (int minFilter = 0; minFilter < DE_LENGTH_OF_ARRAY(minFilterModes); minFilter++)
			{
				Texture2DMipmapTestCaseParameters	testParameters;
				testParameters.minFilter	= minFilterModes[minFilter].mode;
				testParameters.aspectMask	= VK_IMAGE_ASPECT_COLOR_BIT;
				testParameters.programs.push_back(PROGRAM_2D_FLOAT);

				minLodGroup2D->addChild(new TextureTestCase<Texture2DMinLodTestInstance>(testCtx, minFilterModes[minFilter].name, "", testParameters));
			}

			// MAX_LOD
			for (int minFilter = 0; minFilter < DE_LENGTH_OF_ARRAY(minFilterModes); minFilter++)
			{
				Texture2DMipmapTestCaseParameters	testParameters;
				testParameters.minFilter	= minFilterModes[minFilter].mode;
				testParameters.aspectMask	= VK_IMAGE_ASPECT_COLOR_BIT;
				testParameters.programs.push_back(PROGRAM_2D_FLOAT);

				maxLodGroup2D->addChild(new TextureTestCase<Texture2DMaxLodTestInstance>(testCtx, minFilterModes[minFilter].name, "", testParameters));
			}
		}

		{
			// BASE_LEVEL
			for (int minFilter = 0; minFilter < DE_LENGTH_OF_ARRAY(minFilterModes); minFilter++)
			{
				Texture2DMipmapTestCaseParameters	testParameters;
				testParameters.minFilter		= minFilterModes[minFilter].mode;
				testParameters.minFilterName	= minFilterModes[minFilter].name;
				testParameters.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT;
				testParameters.programs.push_back(PROGRAM_2D_FLOAT);

				baseLevelGroup2D->addChild(new TextureTestCase<Texture2DBaseLevelTestInstance>(testCtx, minFilterModes[minFilter].name, "", testParameters));
			}

			// MAX_LEVEL
			for (int minFilter = 0; minFilter < DE_LENGTH_OF_ARRAY(minFilterModes); minFilter++)
			{
				Texture2DMipmapTestCaseParameters	testParameters;
				testParameters.minFilter		= minFilterModes[minFilter].mode;
				testParameters.minFilterName	= minFilterModes[minFilter].name;
				testParameters.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT;
				testParameters.programs.push_back(PROGRAM_2D_FLOAT);

				maxLevelGroup2D->addChild(new TextureTestCase<Texture2DMaxLevelTestInstance>(testCtx, minFilterModes[minFilter].name, "", testParameters));
			}
		}

		// 2D VK_EXT_image_view_min_lod.
#ifndef CTS_USES_VULKANSC
		{
			// MIN_LOD
			for (int minFilter = 0; minFilter < DE_LENGTH_OF_ARRAY(minFilterModes); minFilter++)
			{
				Texture2DMipmapTestCaseParameters	testParameters;
				testParameters.minFilter			= minFilterModes[minFilter].mode;
				testParameters.aspectMask			= VK_IMAGE_ASPECT_COLOR_BIT;
				testParameters.programs.push_back(PROGRAM_2D_FLOAT);
				testParameters.testType				= util::TextureCommonTestCaseParameters::TEST_IMAGE_VIEW_MINLOD;

				imageViewMinLodGroup2D->addChild(new TextureTestCase<Texture2DImageViewMinLodTestInstance>(testCtx, minFilterModes[minFilter].name, "", testParameters));

				std::ostringstream name;
				name << minFilterModes[minFilter].name << "_integer_texel_coord";
				testParameters.testType				= util::TextureCommonTestCaseParameters::TEST_IMAGE_VIEW_MINLOD_INT_TEX_COORD;
				imageViewMinLodGroup2D->addChild(new Texture2DImageViewMinLodIntTexCoordTest(testCtx, name.str().c_str(), "", testParameters));
			}

			// BASE_LEVEL
			for (int minFilter = 0; minFilter < DE_LENGTH_OF_ARRAY(minFilterModes); minFilter++)
			{
				Texture2DMipmapTestCaseParameters	testParameters;
				testParameters.minFilter			= minFilterModes[minFilter].mode;
				testParameters.minFilterName		= minFilterModes[minFilter].name;
				testParameters.aspectMask			= VK_IMAGE_ASPECT_COLOR_BIT;
				testParameters.programs.push_back(PROGRAM_2D_FLOAT);
				testParameters.testType				= util::TextureCommonTestCaseParameters::TEST_IMAGE_VIEW_MINLOD;

				imageViewMinLodBaseLevelGroup2D->addChild(new TextureTestCase<Texture2DImageViewMinLodBaseLevelTestInstance>(testCtx, minFilterModes[minFilter].name, "", testParameters));

				std::ostringstream name;
				name << minFilterModes[minFilter].name << "_integer_texel_coord";
				testParameters.testType				= util::TextureCommonTestCaseParameters::TEST_IMAGE_VIEW_MINLOD_INT_TEX_COORD_BASELEVEL;
				imageViewMinLodBaseLevelGroup2D->addChild(new Texture2DImageViewMinLodIntTexCoordTest(testCtx, name.str().c_str(), "", testParameters));
			}

			imageViewMinLodExtGroup2D->addChild(imageViewMinLodGroup2D.release());
			imageViewMinLodExtGroup2D->addChild(imageViewMinLodBaseLevelGroup2D.release());
		}
#endif // CTS_USES_VULKANSC

		group2D->addChild(biasGroup2D.release());
		group2D->addChild(minLodGroup2D.release());
		group2D->addChild(maxLodGroup2D.release());
		group2D->addChild(baseLevelGroup2D.release());
		group2D->addChild(maxLevelGroup2D.release());
#ifndef CTS_USES_VULKANSC
		group2D->addChild(imageViewMinLodExtGroup2D.release());
#endif // CTS_USES_VULKANSC

		textureMipmappingTests->addChild(group2D.release());
	}

	// Cubemap cases.
	{
		de::MovePtr<tcu::TestCaseGroup>	groupCube			(new tcu::TestCaseGroup(testCtx, "cubemap", "Cube Mipmap Filtering"));

		de::MovePtr<tcu::TestCaseGroup>	minLodGroupCube		(new tcu::TestCaseGroup(testCtx, "min_lod", "Lod control: min lod"));
		de::MovePtr<tcu::TestCaseGroup>	maxLodGroupCube		(new tcu::TestCaseGroup(testCtx, "max_lod", "Lod control: max lod"));
		de::MovePtr<tcu::TestCaseGroup>	baseLevelGroupCube	(new tcu::TestCaseGroup(testCtx, "base_level", "Base level"));
		de::MovePtr<tcu::TestCaseGroup>	maxLevelGroupCube	(new tcu::TestCaseGroup(testCtx, "max_level", "Max level"));

#ifndef CTS_USES_VULKANSC
		de::MovePtr<tcu::TestCaseGroup> imageViewMinLodExtGroupCube	(new tcu::TestCaseGroup(testCtx, "image_view_min_lod", "VK_EXT_image_view_min_lod tests"));
		de::MovePtr<tcu::TestCaseGroup> imageViewMinLodGroupCube	(new tcu::TestCaseGroup(testCtx, "min_lod", "ImageView's minLod"));
		de::MovePtr<tcu::TestCaseGroup> imageViewMinLodBaseLevelGroupCube	(new tcu::TestCaseGroup(testCtx, "base_level", "ImageView's minLod with base level different than one"));
#endif // CTS_USES_VULKANSC

		for (int coordType = 0; coordType < DE_LENGTH_OF_ARRAY(cubeCoordTypes); coordType++)
		{
			de::MovePtr<tcu::TestCaseGroup>	coordTypeGroup	(new tcu::TestCaseGroup(testCtx, cubeCoordTypes[coordType].name, cubeCoordTypes[coordType].desc));

			for (int minFilter = 0; minFilter < DE_LENGTH_OF_ARRAY(minFilterModes); minFilter++)
			{
				for (int magFilter = 0; magFilter < DE_LENGTH_OF_ARRAY(magFilterModes); magFilter++)
				{
					for (int wrapMode = 0; wrapMode < DE_LENGTH_OF_ARRAY(wrapModes); wrapMode++)
					{
						TextureCubeMipmapTestCaseParameters	testParameters;

						testParameters.coordType		= cubeCoordTypes[coordType].type;
						testParameters.minFilter		= minFilterModes[minFilter].mode;
						testParameters.magFilter		= magFilterModes[magFilter].mode;
						testParameters.minFilterName	= minFilterModes[minFilter].name;
						testParameters.wrapS			= wrapModes[wrapMode].mode;
						testParameters.wrapT			= wrapModes[wrapMode].mode;
						testParameters.format			= VK_FORMAT_R8G8B8A8_UNORM;
						testParameters.size				= cubeMapSize;
						testParameters.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT;

						if (testParameters.coordType == COORDTYPE_BASIC_BIAS)
							testParameters.programs.push_back(PROGRAM_CUBE_FLOAT_BIAS);
						else
							testParameters.programs.push_back(PROGRAM_CUBE_FLOAT);

						std::ostringstream name;
						name << minFilterModes[minFilter].name
							 << "_" << magFilterModes[magFilter].name
							 << "_" << wrapModes[wrapMode].name;

						coordTypeGroup->addChild(new TextureTestCase<TextureCubeMipmapTestInstance>(testCtx, name.str().c_str(), "", testParameters));
					}
				}
			}

			groupCube->addChild(coordTypeGroup.release());
		}

		// Cubemap LOD controls.
		{
			// MIN_LOD
			for (int minFilter = 0; minFilter < DE_LENGTH_OF_ARRAY(minFilterModes); minFilter++)
			{
				TextureCubeMipmapTestCaseParameters	testParameters;
				testParameters.minFilter	= minFilterModes[minFilter].mode;
				testParameters.aspectMask	= VK_IMAGE_ASPECT_COLOR_BIT;
				testParameters.programs.push_back(PROGRAM_CUBE_FLOAT);

				minLodGroupCube->addChild(new TextureTestCase<TextureCubeMinLodTestInstance>(testCtx, minFilterModes[minFilter].name, "", testParameters));
			}

			// MAX_LOD
			for (int minFilter = 0; minFilter < DE_LENGTH_OF_ARRAY(minFilterModes); minFilter++)
			{
				TextureCubeMipmapTestCaseParameters	testParameters;
				testParameters.minFilter	= minFilterModes[minFilter].mode;
				testParameters.aspectMask	= VK_IMAGE_ASPECT_COLOR_BIT;
				testParameters.programs.push_back(PROGRAM_CUBE_FLOAT);

				maxLodGroupCube->addChild(new TextureTestCase<TextureCubeMaxLodTestInstance>(testCtx, minFilterModes[minFilter].name, "", testParameters));
			}
		}

		{
			// BASE_LEVEL
			for (int minFilter = 0; minFilter < DE_LENGTH_OF_ARRAY(minFilterModes); minFilter++)
			{
				TextureCubeMipmapTestCaseParameters	testParameters;
				testParameters.minFilter		= minFilterModes[minFilter].mode;
				testParameters.minFilterName	= minFilterModes[minFilter].name;
				testParameters.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT;
				testParameters.programs.push_back(PROGRAM_CUBE_FLOAT);

				baseLevelGroupCube->addChild(new TextureTestCase<TextureCubeBaseLevelTestInstance>(testCtx, minFilterModes[minFilter].name, "", testParameters));
			}

			// MAX_LEVEL
			for (int minFilter = 0; minFilter < DE_LENGTH_OF_ARRAY(minFilterModes); minFilter++)
			{
				TextureCubeMipmapTestCaseParameters	testParameters;
				testParameters.minFilter		= minFilterModes[minFilter].mode;
				testParameters.minFilterName	= minFilterModes[minFilter].name;
				testParameters.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT;
				testParameters.programs.push_back(PROGRAM_CUBE_FLOAT);

				maxLevelGroupCube->addChild(new TextureTestCase<TextureCubeMaxLevelTestInstance>(testCtx, minFilterModes[minFilter].name, "", testParameters));
			}
		}

		// Cube VK_EXT_image_view_min_lod.
#ifndef CTS_USES_VULKANSC
		{
			// MIN_LOD
			for (int minFilter = 0; minFilter < DE_LENGTH_OF_ARRAY(minFilterModes); minFilter++)
			{
				TextureCubeMipmapTestCaseParameters	testParameters;
				testParameters.minFilter			= minFilterModes[minFilter].mode;
				testParameters.aspectMask			= VK_IMAGE_ASPECT_COLOR_BIT;
				testParameters.programs.push_back(PROGRAM_CUBE_FLOAT);
				testParameters.testType				= util::TextureCommonTestCaseParameters::TEST_IMAGE_VIEW_MINLOD;

				imageViewMinLodGroupCube->addChild(new TextureTestCase<TextureCubeImageViewMinLodTestInstance>(testCtx, minFilterModes[minFilter].name, "", testParameters));
			}

			// BASE_LEVEL
			for (int minFilter = 0; minFilter < DE_LENGTH_OF_ARRAY(minFilterModes); minFilter++)
			{
				TextureCubeMipmapTestCaseParameters	testParameters;
				testParameters.minFilter			= minFilterModes[minFilter].mode;
				testParameters.minFilterName		= minFilterModes[minFilter].name;
				testParameters.aspectMask			= VK_IMAGE_ASPECT_COLOR_BIT;
				testParameters.programs.push_back(PROGRAM_CUBE_FLOAT);
				testParameters.testType				= util::TextureCommonTestCaseParameters::TEST_IMAGE_VIEW_MINLOD;

				imageViewMinLodBaseLevelGroupCube->addChild(new TextureTestCase<TextureCubeImageViewMinLodBaseLevelTestInstance>(testCtx, minFilterModes[minFilter].name, "", testParameters));
			}

			imageViewMinLodExtGroupCube->addChild(imageViewMinLodGroupCube.release());
			imageViewMinLodExtGroupCube->addChild(imageViewMinLodBaseLevelGroupCube.release());
		}
#endif // CTS_USES_VULKANSC

		groupCube->addChild(minLodGroupCube.release());
		groupCube->addChild(maxLodGroupCube.release());
		groupCube->addChild(baseLevelGroupCube.release());
		groupCube->addChild(maxLevelGroupCube.release());
#ifndef CTS_USES_VULKANSC
		groupCube->addChild(imageViewMinLodExtGroupCube.release());
#endif // CTS_USES_VULKANSC

		textureMipmappingTests->addChild(groupCube.release());
	}

	// 3D cases.
	{
		de::MovePtr<tcu::TestCaseGroup>	group3D				(new tcu::TestCaseGroup(testCtx, "3d", "3D Mipmap Filtering"));

		de::MovePtr<tcu::TestCaseGroup>	biasGroup3D			(new tcu::TestCaseGroup(testCtx, "bias", "User-supplied bias value"));
		de::MovePtr<tcu::TestCaseGroup>	minLodGroup3D		(new tcu::TestCaseGroup(testCtx, "min_lod", "Lod control: min lod"));
		de::MovePtr<tcu::TestCaseGroup>	maxLodGroup3D		(new tcu::TestCaseGroup(testCtx, "max_lod", "Lod control: max lod"));
		de::MovePtr<tcu::TestCaseGroup>	baseLevelGroup3D	(new tcu::TestCaseGroup(testCtx, "base_level", "Base level"));
		de::MovePtr<tcu::TestCaseGroup>	maxLevelGroup3D		(new tcu::TestCaseGroup(testCtx, "max_level", "Max level"));

#ifndef CTS_USES_VULKANSC
		de::MovePtr<tcu::TestCaseGroup> imageViewMinLodExtGroup3D	(new tcu::TestCaseGroup(testCtx, "image_view_min_lod", "VK_EXT_image_view_min_lod tests"));
		de::MovePtr<tcu::TestCaseGroup> imageViewMinLodGroup3D	(new tcu::TestCaseGroup(testCtx, "min_lod", "ImageView's minLod"));
		de::MovePtr<tcu::TestCaseGroup> imageViewMinLodBaseLevelGroup3D	(new tcu::TestCaseGroup(testCtx, "base_level", "ImageView's minLod with base level different than one"));
#endif // CTS_USES_VULKANSC

		for (int coordType = 0; coordType < DE_LENGTH_OF_ARRAY(coordTypes); coordType++)
		{
			de::MovePtr<tcu::TestCaseGroup>	coordTypeGroup	(new tcu::TestCaseGroup(testCtx, coordTypes[coordType].name, coordTypes[coordType].desc));

			for (int minFilter = 0; minFilter < DE_LENGTH_OF_ARRAY(minFilterModes); minFilter++)
			{
				for (int wrapMode = 0; wrapMode < DE_LENGTH_OF_ARRAY(wrapModes); wrapMode++)
				{
					// Add other size variants to basic cases only.
					int sizeEnd = coordTypes[coordType].type == COORDTYPE_BASIC ? DE_LENGTH_OF_ARRAY(tex3DSizes) : 1;

					Texture3DMipmapTestCaseParameters	testParameters;

					testParameters.coordType		= coordTypes[coordType].type;
					testParameters.minFilter		= minFilterModes[minFilter].mode;
					testParameters.minFilterName	= minFilterModes[minFilter].name;
					testParameters.wrapR			= wrapModes[wrapMode].mode;
					testParameters.wrapS			= wrapModes[wrapMode].mode;
					testParameters.wrapT			= wrapModes[wrapMode].mode;
					testParameters.format			= VK_FORMAT_R8G8B8A8_UNORM;
					testParameters.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT;
					testParameters.programs.push_back(PROGRAM_3D_FLOAT);

					for (int size = 0; size < sizeEnd; size++)
					{
						testParameters.width			= tex3DSizes[size].width;
						testParameters.height			= tex3DSizes[size].height;
						testParameters.depth			= tex3DSizes[size].depth;

						std::ostringstream name;
						name << minFilterModes[minFilter].name
							 << "_" << wrapModes[wrapMode].name;

						if (tex3DSizes[size].name)
							name << "_" << tex3DSizes[size].name;

						coordTypeGroup->addChild(new TextureTestCase<Texture3DMipmapTestInstance>(testCtx, name.str().c_str(), "", testParameters));
					}
				}
			}

			group3D->addChild(coordTypeGroup.release());
		}

		// 3D bias variants.
		{
			for (int minFilter = 0; minFilter < DE_LENGTH_OF_ARRAY(minFilterModes); minFilter++)
			{
				Texture3DMipmapTestCaseParameters	testParameters;
				testParameters.coordType			= COORDTYPE_BASIC_BIAS;
				testParameters.minFilter			= minFilterModes[minFilter].mode;
				testParameters.wrapR				= Sampler::REPEAT_GL;
				testParameters.wrapS				= Sampler::REPEAT_GL;
				testParameters.wrapT				= Sampler::REPEAT_GL;
				testParameters.format				= VK_FORMAT_R8G8B8A8_UNORM;
				testParameters.width				= tex3DSizes[0].width;
				testParameters.height				= tex3DSizes[0].height;
				testParameters.depth				= tex3DSizes[0].depth;
				testParameters.aspectMask			= VK_IMAGE_ASPECT_COLOR_BIT;
				testParameters.programs.push_back(PROGRAM_3D_FLOAT_BIAS);

				biasGroup3D->addChild(new TextureTestCase<Texture3DMipmapTestInstance>(testCtx, minFilterModes[minFilter].name, "", testParameters));
			}
		}

		// 3D LOD controls.
		{
			// MIN_LOD
			for (int minFilter = 0; minFilter < DE_LENGTH_OF_ARRAY(minFilterModes); minFilter++)
			{
				Texture3DMipmapTestCaseParameters	testParameters;
				testParameters.minFilter			= minFilterModes[minFilter].mode;
				testParameters.aspectMask			= VK_IMAGE_ASPECT_COLOR_BIT;
				testParameters.programs.push_back(PROGRAM_3D_FLOAT);

				minLodGroup3D->addChild(new TextureTestCase<Texture3DMinLodTestInstance>(testCtx, minFilterModes[minFilter].name, "", testParameters));
			}

			// MAX_LOD
			for (int minFilter = 0; minFilter < DE_LENGTH_OF_ARRAY(minFilterModes); minFilter++)
			{
				Texture3DMipmapTestCaseParameters	testParameters;
				testParameters.minFilter			= minFilterModes[minFilter].mode;
				testParameters.aspectMask			= VK_IMAGE_ASPECT_COLOR_BIT;
				testParameters.programs.push_back(PROGRAM_3D_FLOAT);

				maxLodGroup3D->addChild(new TextureTestCase<Texture3DMaxLodTestInstance>(testCtx, minFilterModes[minFilter].name, "", testParameters));
			}
		}

		{
			// BASE_LEVEL
			for (int minFilter = 0; minFilter < DE_LENGTH_OF_ARRAY(minFilterModes); minFilter++)
			{
				Texture3DMipmapTestCaseParameters	testParameters;
				testParameters.minFilter			= minFilterModes[minFilter].mode;
				testParameters.minFilterName		= minFilterModes[minFilter].name;
				testParameters.aspectMask			= VK_IMAGE_ASPECT_COLOR_BIT;
				testParameters.programs.push_back(PROGRAM_3D_FLOAT);

				baseLevelGroup3D->addChild(new TextureTestCase<Texture3DBaseLevelTestInstance>(testCtx, minFilterModes[minFilter].name, "", testParameters));
			}

			// MAX_LEVEL
			for (int minFilter = 0; minFilter < DE_LENGTH_OF_ARRAY(minFilterModes); minFilter++)
			{
				Texture3DMipmapTestCaseParameters	testParameters;
				testParameters.minFilter			= minFilterModes[minFilter].mode;
				testParameters.minFilterName		= minFilterModes[minFilter].name;
				testParameters.aspectMask			= VK_IMAGE_ASPECT_COLOR_BIT;
				testParameters.programs.push_back(PROGRAM_3D_FLOAT);

				maxLevelGroup3D->addChild(new TextureTestCase<Texture3DMaxLevelTestInstance>(testCtx, minFilterModes[minFilter].name, "", testParameters));
			}
		}

		// 3D VK_EXT_image_view_min_lod.
#ifndef CTS_USES_VULKANSC
		{
			// MIN_LOD
			for (int minFilter = 0; minFilter < DE_LENGTH_OF_ARRAY(minFilterModes); minFilter++)
			{
				Texture3DMipmapTestCaseParameters	testParameters;
				testParameters.minFilter				= minFilterModes[minFilter].mode;
				testParameters.aspectMask				= VK_IMAGE_ASPECT_COLOR_BIT;
				testParameters.programs.push_back(PROGRAM_3D_FLOAT);
				testParameters.testType				= util::TextureCommonTestCaseParameters::TEST_IMAGE_VIEW_MINLOD;

				imageViewMinLodGroup3D->addChild(new TextureTestCase<Texture3DImageViewMinLodTestInstance>(testCtx, minFilterModes[minFilter].name, "", testParameters));

				std::ostringstream name;
				name << minFilterModes[minFilter].name << "_integer_texel_coord";
				testParameters.testType				= util::TextureCommonTestCaseParameters::TEST_IMAGE_VIEW_MINLOD_INT_TEX_COORD;
				imageViewMinLodGroup3D->addChild(new Texture3DImageViewMinLodIntTexCoordTest(testCtx, name.str().c_str(), "", testParameters));
			}

			// BASE_LEVEL
			for (int minFilter = 0; minFilter < DE_LENGTH_OF_ARRAY(minFilterModes); minFilter++)
			{
				Texture3DMipmapTestCaseParameters testParameters;
				testParameters.minFilter				= minFilterModes[minFilter].mode;
				testParameters.minFilterName			= minFilterModes[minFilter].name;
				testParameters.aspectMask				= VK_IMAGE_ASPECT_COLOR_BIT;
				testParameters.programs.push_back(PROGRAM_3D_FLOAT);
				testParameters.testType					= util::TextureCommonTestCaseParameters::TEST_IMAGE_VIEW_MINLOD;


				imageViewMinLodBaseLevelGroup3D->addChild(new TextureTestCase<Texture3DImageViewMinLodBaseLevelTestInstance>(testCtx, minFilterModes[minFilter].name, "", testParameters));

				std::ostringstream name;
				name << minFilterModes[minFilter].name << "_integer_texel_coord";
				testParameters.testType					= util::TextureCommonTestCaseParameters::TEST_IMAGE_VIEW_MINLOD_INT_TEX_COORD_BASELEVEL;
				imageViewMinLodBaseLevelGroup3D->addChild(new Texture3DImageViewMinLodIntTexCoordTest(testCtx, name.str().c_str(), "", testParameters));
			}

			imageViewMinLodExtGroup3D->addChild(imageViewMinLodGroup3D.release());
			imageViewMinLodExtGroup3D->addChild(imageViewMinLodBaseLevelGroup3D.release());
		}
#endif // CTS_USES_VULKANSC

		group3D->addChild(biasGroup3D.release());
		group3D->addChild(minLodGroup3D.release());
		group3D->addChild(maxLodGroup3D.release());
		group3D->addChild(baseLevelGroup3D.release());
		group3D->addChild(maxLevelGroup3D.release());
#ifndef CTS_USES_VULKANSC
		group3D->addChild(imageViewMinLodExtGroup3D.release());
#endif // CTS_USES_VULKANSC

		textureMipmappingTests->addChild(group3D.release());
	}

#ifndef CTS_USES_VULKANSC
	{
		const auto minLodGatherGroup = createTestGroup(testCtx, "min_lod_gather", "Test minLod with textureGather operations", populateMinLodGatherGroup, destroyDeviceHelpers);
		textureMipmappingTests->addChild(minLodGatherGroup);
	}
#endif // CTS_USES_VULKANSC
}

tcu::TestCaseGroup* createTextureMipmappingTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "mipmap", "Texture mipmapping tests.", populateTextureMipmappingTests);
}

} // texture
} // vkt
