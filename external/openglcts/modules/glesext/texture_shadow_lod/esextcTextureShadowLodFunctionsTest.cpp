/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2014-2019 The Khronos Group Inc.
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
 */ /*!
 * \file
 * \brief
 */ /*-------------------------------------------------------------------*/

/*!
 * \file esextcTextureShadowLodFunctionsTest.cpp
 * \brief EXT_texture_shadow_lod extension testing
 */ /*-------------------------------------------------------------------*/

#include "esextcTextureShadowLodFunctionsTest.hpp"
#include "deMath.h"
#include "glcShaderLibrary.hpp"
#include "glcShaderRenderCase.hpp"
#include "glsTextureTestUtil.hpp"
#include "gluPixelTransfer.hpp"
#include "gluStrUtil.hpp"
#include "gluTexture.hpp"
#include "gluTextureUtil.hpp"
#include "glwFunctions.hpp"
#include "tcuMatrix.hpp"
#include "tcuMatrixUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"

#include <sstream>

#include "glwEnums.hpp"
#include "glwFunctions.hpp"

namespace deqp
{
namespace Functional
{
namespace
{

using glu::TextureTestUtil::computeLodFromDerivates;

enum Function
{
	FUNCTION_TEXTURE = 0, //!< texture(), textureOffset()
	FUNCTION_TEXTURELOD,  // ...
	FUNCTION_LAST
};

// For texture(..., [bias]) functions.
inline bool functionHasAutoLod(glu::ShaderType shaderType, Function function)
{
	return (shaderType == glu::SHADERTYPE_FRAGMENT) && (function == FUNCTION_TEXTURE);
}

// For textureLod* functions.
inline bool functionHasLod(Function function)
{
	return function == FUNCTION_TEXTURELOD;
}

struct TextureLookupSpec
{
	// The texture function to use.
	Function function;

	// Min/max texture coordinates.
	tcu::Vec4 minCoord;
	tcu::Vec4 maxCoord;

	// Bias
	bool useBias;

	// Bias or Lod for *Lod* functions
	float minLodBias;
	float maxLodBias;

	// For *Offset variants
	bool	   useOffset;
	tcu::IVec3 offset;

	// Do we require an additional shadow ref "compare"
	// parameter in the texture function's parameter list? (used for
	// shadow cube array textures).
	bool  useSepRef;
	float minSepRef;
	float maxSepRef;

	TextureLookupSpec(void)
		: function(FUNCTION_LAST)
		, minCoord(0.0f)
		, maxCoord(1.0f)
		, useBias(false)
		, minLodBias(0.0f)
		, maxLodBias(0.0f)
		, useOffset(false)
		, offset(0)
		, useSepRef(false)
		, minSepRef(0.0f)
		, maxSepRef(0.0f)
	{
	}

	TextureLookupSpec(Function function_, const tcu::Vec4& minCoord_, const tcu::Vec4& maxCoord_, bool useBias_,
					  float minLodBias_, float maxLodBias_, bool useOffset_, const tcu::IVec3& offset_, bool useSepRef_,
					  float minSepRef_, float maxSepRef_)
		: function(function_)
		, minCoord(minCoord_)
		, maxCoord(maxCoord_)
		, useBias(useBias_)
		, minLodBias(minLodBias_)
		, maxLodBias(maxLodBias_)
		, useOffset(useOffset_)
		, offset(offset_)
		, useSepRef(useSepRef_)
		, minSepRef(minSepRef_)
		, maxSepRef(maxSepRef_)
	{
	}
};

// Only shadow texture types contained in EXT_texture_shadow_lod will be tested.
enum TextureType
{
	TEXTURETYPE_2D,
	TEXTURETYPE_CUBE_MAP,
	TEXTURETYPE_CUBE_MAP_ARRAY,
	TEXTURETYPE_2D_ARRAY,

	TEXTURETYPE_LAST
};

struct TextureSpec
{
	TextureType  type;   //!< Texture type (2D, cubemap, ...)
	deUint32	 format; //!< Internal format.
	int			 width;
	int			 height;
	int			 depth;
	int			 numLevels;
	tcu::Sampler sampler;

	TextureSpec(void) : type(TEXTURETYPE_LAST), format(GL_NONE), width(0), height(0), depth(0), numLevels(0)
	{
	}

	TextureSpec(TextureType type_, deUint32 format_, int width_, int height_, int depth_, int numLevels_,
				const tcu::Sampler& sampler_)
		: type(type_)
		, format(format_)
		, width(width_)
		, height(height_)
		, depth(depth_)
		, numLevels(numLevels_)
		, sampler(sampler_)
	{
	}
};

struct TexLookupParams
{
	float	  lod;
	tcu::IVec3 offset;
	tcu::Vec4  scale;
	tcu::Vec4  bias;

	TexLookupParams(void) : lod(0.0f), offset(0), scale(1.0f), bias(0.0f)
	{
	}
};

} // namespace

using tcu::IVec2;
using tcu::IVec3;
using tcu::IVec4;
using tcu::Vec2;
using tcu::Vec3;
using tcu::Vec4;

static const glu::TextureTestUtil::LodMode DEFAULT_LOD_MODE = glu::TextureTestUtil::LODMODE_EXACT;

typedef void (*TexEvalFunc)(ShaderEvalContext& c, const TexLookupParams& lookupParams);

inline float texture2DShadow(const ShaderEvalContext& c, float ref, float s, float t, float lod)
{
	return c.textures[0].tex2D->sampleCompare(c.textures[0].sampler, ref, s, t, lod);
}
inline float texture2DArrayShadow(const ShaderEvalContext& c, float ref, float s, float t, float r, float lod)
{
	return c.textures[0].tex2DArray->sampleCompare(c.textures[0].sampler, ref, s, t, r, lod);
}
inline float textureCubeShadow(const ShaderEvalContext& c, float ref, float s, float t, float r, float lod)
{
	return c.textures[0].texCube->sampleCompare(c.textures[0].sampler, ref, s, t, r, lod);
}
inline float textureCubeArrayShadow(const ShaderEvalContext& c, float ref, float s, float t, float r, float q,
									float lod)
{
	return c.textures[0].texCubeArray->sampleCompare(c.textures[0].sampler, ref, s, t, r, q, lod);
}
inline float texture2DShadowOffset(const ShaderEvalContext& c, float ref, float s, float t, float lod, IVec2 offset)
{
	return c.textures[0].tex2D->sampleCompareOffset(c.textures[0].sampler, ref, s, t, lod, offset);
}
inline float texture2DArrayShadowOffset(const ShaderEvalContext& c, float ref, float s, float t, float r, float lod,
										IVec2 offset)
{
	return c.textures[0].tex2DArray->sampleCompareOffset(c.textures[0].sampler, ref, s, t, r, lod, offset);
}

// Shadow evaluation functions
static void evalTexture2DArrayShadow(ShaderEvalContext& c, const TexLookupParams& p)
{
	c.color.x() = texture2DArrayShadow(c, c.in[0].w(), c.in[0].x(), c.in[0].y(), c.in[0].z(), p.lod);
}
static void evalTexture2DArrayShadowBias(ShaderEvalContext& c, const TexLookupParams& p)
{
	c.color.x() = texture2DArrayShadow(c, c.in[0].w(), c.in[0].x(), c.in[0].y(), c.in[0].z(), p.lod + c.in[1].x());
}
static void evalTexture2DArrayShadowOffset(ShaderEvalContext& c, const TexLookupParams& p)
{
	c.color.x() = texture2DArrayShadowOffset(c, c.in[0].w(), c.in[0].x(), c.in[0].y(), c.in[0].z(), p.lod,
											 p.offset.swizzle(0, 1));
}
static void evalTexture2DArrayShadowOffsetBias(ShaderEvalContext& c, const TexLookupParams& p)
{
	c.color.x() = texture2DArrayShadowOffset(c, c.in[0].w(), c.in[0].x(), c.in[0].y(), c.in[0].z(), p.lod + c.in[1].x(),
											 p.offset.swizzle(0, 1));
}

static void evalTexture2DArrayShadowLod(ShaderEvalContext& c, const TexLookupParams& p)
{
	(void)p;
	c.color.x() = texture2DArrayShadow(c, c.in[0].w(), c.in[0].x(), c.in[0].y(), c.in[0].z(), c.in[1].x());
}
static void evalTexture2DArrayShadowLodOffset(ShaderEvalContext& c, const TexLookupParams& p)
{
	c.color.x() = texture2DArrayShadowOffset(c, c.in[0].w(), c.in[0].x(), c.in[0].y(), c.in[0].z(), c.in[1].x(),
											 p.offset.swizzle(0, 1));
}

static void evalTextureCubeShadow(ShaderEvalContext& c, const TexLookupParams& p)
{
	c.color.x() = textureCubeShadow(c, c.in[0].w(), c.in[0].x(), c.in[0].y(), c.in[0].z(), p.lod);
}
static void evalTextureCubeShadowBias(ShaderEvalContext& c, const TexLookupParams& p)
{
	c.color.x() = textureCubeShadow(c, c.in[0].w(), c.in[0].x(), c.in[0].y(), c.in[0].z(), p.lod + c.in[1].x());
}
static void evalTextureCubeShadowLod(ShaderEvalContext& c, const TexLookupParams& p)
{
	(void)p;
	c.color.x() = textureCubeShadow(c, c.in[0].w(), c.in[0].x(), c.in[0].y(), c.in[0].z(), c.in[1].x());
}

static void evalTextureCubeArrayShadow(ShaderEvalContext& c, const TexLookupParams& p)
{
	c.color.x() = textureCubeArrayShadow(c, c.in[1].y(), c.in[0].x(), c.in[0].y(), c.in[0].z(), c.in[0].w(), p.lod);
}
static void evalTextureCubeArrayShadowBias(ShaderEvalContext& c, const TexLookupParams& p)
{
	c.color.x() =
		textureCubeArrayShadow(c, c.in[1].y(), c.in[0].x(), c.in[0].y(), c.in[0].z(), c.in[0].w(), p.lod + c.in[1].x());
}
static void evalTextureCubeArrayShadowLod(ShaderEvalContext& c, const TexLookupParams& p)
{
	(void)p;
	c.color.x() =
		textureCubeArrayShadow(c, c.in[1].y(), c.in[0].x(), c.in[0].y(), c.in[0].z(), c.in[0].w(), c.in[1].x());
}

class TexLookupEvaluator : public ShaderEvaluator
{
public:
	TexLookupEvaluator(TexEvalFunc evalFunc, const TexLookupParams& lookupParams)
		: m_evalFunc(evalFunc), m_lookupParams(lookupParams)
	{
	}

	virtual void evaluate(ShaderEvalContext& ctx)
	{
		m_evalFunc(ctx, m_lookupParams);
	}

private:
	TexEvalFunc			   m_evalFunc;
	const TexLookupParams& m_lookupParams;
};

class TextureShadowLodTestCase : public ShaderRenderCase
{
public:
	TextureShadowLodTestCase(Context& context, const char* name, const char* desc, const TextureLookupSpec& lookup,
							 const TextureSpec& texture, TexEvalFunc evalFunc, bool isVertexCase);
	~TextureShadowLodTestCase(void);

	void init(void);
	void deinit(void);

protected:
	void setupUniforms(deUint32 programID, const tcu::Vec4& constCoords);

private:
	void initTexture(void);
	void initShaderSources(void);

	TextureLookupSpec m_lookupSpec;
	TextureSpec		  m_textureSpec;

	TexLookupParams	m_lookupParams;
	TexLookupEvaluator m_evaluator;

	glu::Texture2D*		   m_texture2D;
	glu::TextureCube*	  m_textureCube;
	glu::TextureCubeArray* m_textureCubeArray;
	glu::Texture2DArray*   m_texture2DArray;
};

TextureShadowLodTestCase::TextureShadowLodTestCase(Context& context, const char* name, const char* desc,
												   const TextureLookupSpec& lookup, const TextureSpec& texture,
												   TexEvalFunc evalFunc, bool isVertexCase)
	: ShaderRenderCase(context.getTestContext(), context.getRenderContext(), context.getContextInfo(), name, desc,
					   isVertexCase, m_evaluator)
	, m_lookupSpec(lookup)
	, m_textureSpec(texture)
	, m_evaluator(evalFunc, m_lookupParams)
	, m_texture2D(DE_NULL)
	, m_textureCube(DE_NULL)
	, m_textureCubeArray(DE_NULL)
	, m_texture2DArray(DE_NULL)
{
}

TextureShadowLodTestCase::~TextureShadowLodTestCase(void)
{
	delete m_texture2D;
	delete m_textureCube;
	delete m_textureCubeArray;
	delete m_texture2DArray;
}

void TextureShadowLodTestCase::init(void)
{
	// Check extension and other features are supported with this context/platform.
	{
		glu::ContextInfo* info = glu::ContextInfo::create(m_renderCtx);

		// First check if extension is available.
		if (!info->isExtensionSupported("GL_EXT_texture_shadow_lod"))
		{
			throw tcu::NotSupportedError("EXT_texture_shadow_lod is not supported on the platform");
		}

		// Check that API support and that the various texture_cube_map_array extension is supported based on GL / ES versions.

		if (glu::isContextTypeES(m_renderCtx.getType()))
		{
			// ES
			if (!glu::contextSupports(m_renderCtx.getType(), glu::ApiType::es(3, 0)))
			{
				throw tcu::NotSupportedError(
					"EXT_texture_shadow_lod is not supported due to minimum ES version requirements");
			}

			// Check if cube map array is supported.  For ES, it is supported
			// as of ES 3.2, or with OES_texture_cube_map_array for
			// 3.1.
			if (m_textureSpec.type == TEXTURETYPE_CUBE_MAP_ARRAY)
			{
				if (!glu::contextSupports(m_renderCtx.getType(), glu::ApiType::es(3, 2)) &&
					!(glu::contextSupports(m_renderCtx.getType(), glu::ApiType::es(3, 1)) &&
					  (info->isExtensionSupported("GL_OES_texture_cube_map_array") ||
					   info->isExtensionSupported("GL_EXT_texture_cube_map_array"))))
				{
					throw tcu::NotSupportedError("GL_OES_texture_cube_map_array or GL_EXT_texture_cube_map_array is "
												 "required for this configuration and is not available.");
				}
			}
		}
		else
		{
			// GL
			if (!glu::contextSupports(m_renderCtx.getType(), glu::ApiType::core(2, 0)))
			{
				throw tcu::NotSupportedError(
					"EXT_texture_shadow_lod is not supported due to minimum GL version requirements");
			}

			// Check if cube map array is supported.  For GL, it is supported
			// as of GL 4.0 core, or with ARB_texture_cube_map_array prior.
			if (m_textureSpec.type == TEXTURETYPE_CUBE_MAP_ARRAY)
			{
				if (!glu::contextSupports(m_renderCtx.getType(), glu::ApiType::core(4, 0)) &&
					!info->isExtensionSupported("GL_ARB_texture_cube_map_array"))
				{
					throw tcu::NotSupportedError(
						"ARB_texture_cube_map_array is required for this configuration and is not available.");
				}
			}
		}
	}

	// Set up the user attributes.
	// The user attributes are set up as matrices where each row represents a component
	// in the attribute transformed against the vertex's interpolated position across the grid.
	{
		// Base coord scale & bias
		Vec4 s = m_lookupSpec.maxCoord - m_lookupSpec.minCoord;
		Vec4 b = m_lookupSpec.minCoord;

		float baseCoordTrans[] = { s.x(),		 0.0f,		   0.f, b.x(),
								   0.f,			 s.y(),		   0.f, b.y(),
								   s.z() / 2.f,  -s.z() / 2.f, 0.f, s.z() / 2.f + b.z(),
								   -s.w() / 2.f, s.w() / 2.f,  0.f, s.w() / 2.f + b.w() };

		//a_in0
		m_userAttribTransforms.push_back(tcu::Mat4(baseCoordTrans));
	}

	bool hasLodBias = functionHasLod(m_lookupSpec.function) || m_lookupSpec.useBias;

	if (hasLodBias || m_lookupSpec.useSepRef)
	{
		float s		  = 0.0f;
		float b		  = 0.0f;
		float sepRefS = 0.0f;
		float sepRefB = 0.0f;

		if (hasLodBias)
		{
			s = m_lookupSpec.maxLodBias - m_lookupSpec.minLodBias;
			b = m_lookupSpec.minLodBias;
		}

		if (m_lookupSpec.useSepRef)
		{
			sepRefS = m_lookupSpec.maxSepRef - m_lookupSpec.minSepRef;
			sepRefB = m_lookupSpec.minSepRef;
		}
		float lodCoordTrans[] = { s / 2.0f, s / 2.0f, 0.f,  b,	sepRefS / 2.0f, sepRefS / 2.0f, 0.0f, sepRefB,
								  0.0f,		0.0f,	 0.0f, 0.0f, 0.0f,			  0.0f,			  0.0f, 0.0f };

		//a_in1
		m_userAttribTransforms.push_back(tcu::Mat4(lodCoordTrans));
	}

	initShaderSources();
	initTexture();

	ShaderRenderCase::init();
}

void TextureShadowLodTestCase::initTexture(void)
{
	static const IVec4 texCubeSwz[] = { IVec4(0, 0, 1, 1), IVec4(1, 1, 0, 0), IVec4(0, 1, 0, 1),
										IVec4(1, 0, 1, 0), IVec4(0, 1, 1, 0), IVec4(1, 0, 0, 1) };
	DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(texCubeSwz) == tcu::CUBEFACE_LAST);

	tcu::TextureFormat	 texFmt		= glu::mapGLInternalFormat(m_textureSpec.format);
	tcu::TextureFormatInfo fmtInfo		= tcu::getTextureFormatInfo(texFmt);
	tcu::IVec2			   viewportSize = getViewportSize();
	bool isAutoLod = functionHasAutoLod(m_isVertexCase ? glu::SHADERTYPE_VERTEX : glu::SHADERTYPE_FRAGMENT,
										m_lookupSpec.function); // LOD can vary significantly

	switch (m_textureSpec.type)
	{
	case TEXTURETYPE_2D:
	{
		float levelStep	= isAutoLod ? 0.0f : 1.0f / (float)de::max(1, m_textureSpec.numLevels - 1);
		Vec4  cScale	   = fmtInfo.valueMax - fmtInfo.valueMin;
		Vec4  cBias		   = fmtInfo.valueMin;
		int   baseCellSize = de::min(m_textureSpec.width / 4, m_textureSpec.height / 4);

		m_texture2D = new glu::Texture2D(m_renderCtx, m_textureSpec.format, m_textureSpec.width, m_textureSpec.height);
		for (int level = 0; level < m_textureSpec.numLevels; level++)
		{
			float fA	 = float(level) * levelStep;
			float fB	 = 1.0f - fA;
			Vec4  colorA = cBias + cScale * Vec4(fA, fB, fA, fB);
			Vec4  colorB = cBias + cScale * Vec4(fB, fA, fB, fA);

			m_texture2D->getRefTexture().allocLevel(level);
			tcu::fillWithGrid(m_texture2D->getRefTexture().getLevel(level), de::max(1, baseCellSize >> level), colorA,
							  colorB);
		}
		m_texture2D->upload();

		// Compute LOD.
		float dudx =
			(m_lookupSpec.maxCoord[0] - m_lookupSpec.minCoord[0]) * (float)m_textureSpec.width / (float)viewportSize[0];
		float dvdy = (m_lookupSpec.maxCoord[1] - m_lookupSpec.minCoord[1]) * (float)m_textureSpec.height /
					 (float)viewportSize[1];
		m_lookupParams.lod = computeLodFromDerivates(DEFAULT_LOD_MODE, dudx, 0.0f, 0.0f, dvdy);

		// Append to texture list.
		m_textures.push_back(TextureBinding(m_texture2D, m_textureSpec.sampler));
		break;
	}

	case TEXTURETYPE_2D_ARRAY:
	{
		float layerStep = 1.0f / (float)m_textureSpec.depth;
		float levelStep =
			isAutoLod ? 0.0f : 1.0f / (float)(de::max(1, m_textureSpec.numLevels - 1) * m_textureSpec.depth);
		Vec4 cScale		  = fmtInfo.valueMax - fmtInfo.valueMin;
		Vec4 cBias		  = fmtInfo.valueMin;
		int  baseCellSize = de::min(m_textureSpec.width / 4, m_textureSpec.height / 4);

		m_texture2DArray = new glu::Texture2DArray(m_renderCtx, m_textureSpec.format, m_textureSpec.width,
												   m_textureSpec.height, m_textureSpec.depth);
		for (int level = 0; level < m_textureSpec.numLevels; level++)
		{
			m_texture2DArray->getRefTexture().allocLevel(level);
			tcu::PixelBufferAccess levelAccess = m_texture2DArray->getRefTexture().getLevel(level);

			for (int layer = 0; layer < levelAccess.getDepth(); layer++)
			{
				float fA	 = (float)layer * layerStep + (float)level * levelStep;
				float fB	 = 1.0f - fA;
				Vec4  colorA = cBias + cScale * Vec4(fA, fB, fA, fB);
				Vec4  colorB = cBias + cScale * Vec4(fB, fA, fB, fA);

				tcu::fillWithGrid(
					tcu::getSubregion(levelAccess, 0, 0, layer, levelAccess.getWidth(), levelAccess.getHeight(), 1),
					de::max(1, baseCellSize >> level), colorA, colorB);
			}
		}
		m_texture2DArray->upload();

		// Compute LOD.
		float dudx =
			(m_lookupSpec.maxCoord[0] - m_lookupSpec.minCoord[0]) * (float)m_textureSpec.width / (float)viewportSize[0];
		float dvdy = (m_lookupSpec.maxCoord[1] - m_lookupSpec.minCoord[1]) * (float)m_textureSpec.height /
					 (float)viewportSize[1];
		m_lookupParams.lod = computeLodFromDerivates(DEFAULT_LOD_MODE, dudx, 0.0f, 0.0f, dvdy);

		// Append to texture list.
		m_textures.push_back(TextureBinding(m_texture2DArray, m_textureSpec.sampler));
		break;
	}

	case TEXTURETYPE_CUBE_MAP:
	{
		float levelStep	= isAutoLod ? 0.0f : 1.0f / (float)de::max(1, m_textureSpec.numLevels - 1);
		Vec4  cScale	   = fmtInfo.valueMax - fmtInfo.valueMin;
		Vec4  cBias		   = fmtInfo.valueMin;
		Vec4  cCorner	  = cBias + cScale * 0.5f;
		int   baseCellSize = de::min(m_textureSpec.width / 4, m_textureSpec.height / 4);

		DE_ASSERT(m_textureSpec.width == m_textureSpec.height);
		m_textureCube = new glu::TextureCube(m_renderCtx, m_textureSpec.format, m_textureSpec.width);
		for (int level = 0; level < m_textureSpec.numLevels; level++)
		{
			float fA = float(level) * levelStep;
			float fB = 1.0f - fA;
			Vec2  f(fA, fB);

			for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
			{
				const IVec4& swzA   = texCubeSwz[face];
				IVec4		 swzB   = 1 - swzA;
				Vec4		 colorA = cBias + cScale * f.swizzle(swzA[0], swzA[1], swzA[2], swzA[3]);
				Vec4		 colorB = cBias + cScale * f.swizzle(swzB[0], swzB[1], swzB[2], swzB[3]);

				m_textureCube->getRefTexture().allocLevel((tcu::CubeFace)face, level);
				{
					const tcu::PixelBufferAccess access =
						m_textureCube->getRefTexture().getLevelFace(level, (tcu::CubeFace)face);
					const int lastPix = access.getWidth() - 1;

					tcu::fillWithGrid(access, de::max(1, baseCellSize >> level), colorA, colorB);

					// Ensure all corners have identical colors in order to avoid dealing with ambiguous corner texel filtering
					access.setPixel(cCorner, 0, 0);
					access.setPixel(cCorner, 0, lastPix);
					access.setPixel(cCorner, lastPix, 0);
					access.setPixel(cCorner, lastPix, lastPix);
				}
			}
		}
		m_textureCube->upload();

		// Compute LOD \note Assumes that only single side is accessed and R is constant major axis.
		DE_ASSERT(de::abs(m_lookupSpec.minCoord[2] - m_lookupSpec.maxCoord[2]) < 0.005);
		DE_ASSERT(de::abs(m_lookupSpec.minCoord[0]) < de::abs(m_lookupSpec.minCoord[2]) &&
				  de::abs(m_lookupSpec.maxCoord[0]) < de::abs(m_lookupSpec.minCoord[2]));
		DE_ASSERT(de::abs(m_lookupSpec.minCoord[1]) < de::abs(m_lookupSpec.minCoord[2]) &&
				  de::abs(m_lookupSpec.maxCoord[1]) < de::abs(m_lookupSpec.minCoord[2]));

		tcu::CubeFaceFloatCoords c00 =
			tcu::getCubeFaceCoords(Vec3(m_lookupSpec.minCoord[0], m_lookupSpec.minCoord[1], m_lookupSpec.minCoord[2]));
		tcu::CubeFaceFloatCoords c10 =
			tcu::getCubeFaceCoords(Vec3(m_lookupSpec.maxCoord[0], m_lookupSpec.minCoord[1], m_lookupSpec.minCoord[2]));
		tcu::CubeFaceFloatCoords c01 =
			tcu::getCubeFaceCoords(Vec3(m_lookupSpec.minCoord[0], m_lookupSpec.maxCoord[1], m_lookupSpec.minCoord[2]));
		float dudx = (c10.s - c00.s) * (float)m_textureSpec.width / (float)viewportSize[0];
		float dvdy = (c01.t - c00.t) * (float)m_textureSpec.height / (float)viewportSize[1];

		m_lookupParams.lod = computeLodFromDerivates(DEFAULT_LOD_MODE, dudx, 0.0f, 0.0f, dvdy);

		m_textures.push_back(TextureBinding(m_textureCube, m_textureSpec.sampler));
		break;
	}

	case TEXTURETYPE_CUBE_MAP_ARRAY:
	{
		float layerStep = 1.0f / (float)m_textureSpec.depth;
		float levelStep =
			isAutoLod ? 0.0f : 1.0f / (float)(de::max(1, m_textureSpec.numLevels - 1) * m_textureSpec.depth);
		Vec4 cScale		  = fmtInfo.valueMax - fmtInfo.valueMin;
		Vec4 cBias		  = fmtInfo.valueMin;
		Vec4 cCorner	  = cBias + cScale * 0.5f;
		int  baseCellSize = de::min(m_textureSpec.width / 4, m_textureSpec.height / 4);

		DE_ASSERT(m_textureSpec.width == m_textureSpec.height);
		// I think size here means width/height of cube tex
		m_textureCubeArray =
			new glu::TextureCubeArray(m_renderCtx, m_textureSpec.format, m_textureSpec.width, m_textureSpec.depth * 6);
		// mipmap level
		for (int level = 0; level < m_textureSpec.numLevels; level++)
		{
			m_textureCubeArray->getRefTexture().allocLevel(level);
			tcu::PixelBufferAccess levelAccess = m_textureCubeArray->getRefTexture().getLevel(level);

			//array layer
			DE_ASSERT((levelAccess.getDepth() % 6) == 0);

			for (int layer = 0; layer < levelAccess.getDepth() / 6; ++layer)
			{
				for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
				{
					float fA = (float)layer * layerStep + float(level) * levelStep;
					float fB = 1.0f - fA;
					Vec2  f(fA, fB);

					const IVec4& swzA   = texCubeSwz[face];
					IVec4		 swzB   = 1 - swzA;
					Vec4		 colorA = cBias + cScale * f.swizzle(swzA[0], swzA[1], swzA[2], swzA[3]);
					Vec4		 colorB = cBias + cScale * f.swizzle(swzB[0], swzB[1], swzB[2], swzB[3]);

					{
						const tcu::PixelBufferAccess access  = m_textureCubeArray->getRefTexture().getLevel(level);
						const int					 lastPix = access.getWidth() - 1;

						int layerFaceNdx = face + layer * 6;

						DE_ASSERT(levelAccess.getWidth() == levelAccess.getHeight());
						tcu::fillWithGrid(tcu::getSubregion(access, 0, 0, layerFaceNdx, levelAccess.getWidth(),
															levelAccess.getHeight(), 1),
										  de::max(1, baseCellSize >> level), colorA, colorB);

						// Ensure all corners have identical colors in order to avoid dealing with ambiguous corner texel filtering
						access.setPixel(cCorner, 0, 0, layer);
						access.setPixel(cCorner, 0, lastPix, layer);
						access.setPixel(cCorner, lastPix, 0, layer);
						access.setPixel(cCorner, lastPix, lastPix, layer);
					}
				}
			}
		}
		m_textureCubeArray->upload();

		// Compute LOD \note Assumes that only single side is accessed and R is constant major axis.
		DE_ASSERT(de::abs(m_lookupSpec.minCoord[2] - m_lookupSpec.maxCoord[2]) < 0.005);
		DE_ASSERT(de::abs(m_lookupSpec.minCoord[0]) < de::abs(m_lookupSpec.minCoord[2]) &&
				  de::abs(m_lookupSpec.maxCoord[0]) < de::abs(m_lookupSpec.minCoord[2]));
		DE_ASSERT(de::abs(m_lookupSpec.minCoord[1]) < de::abs(m_lookupSpec.minCoord[2]) &&
				  de::abs(m_lookupSpec.maxCoord[1]) < de::abs(m_lookupSpec.minCoord[2]));

		tcu::CubeFaceFloatCoords c00 =
			tcu::getCubeFaceCoords(Vec3(m_lookupSpec.minCoord[0], m_lookupSpec.minCoord[1], m_lookupSpec.minCoord[2]));
		tcu::CubeFaceFloatCoords c10 =
			tcu::getCubeFaceCoords(Vec3(m_lookupSpec.maxCoord[0], m_lookupSpec.minCoord[1], m_lookupSpec.minCoord[2]));
		tcu::CubeFaceFloatCoords c01 =
			tcu::getCubeFaceCoords(Vec3(m_lookupSpec.minCoord[0], m_lookupSpec.maxCoord[1], m_lookupSpec.minCoord[2]));
		float dudx = (c10.s - c00.s) * (float)m_textureSpec.width / (float)viewportSize[0];
		float dvdy = (c01.t - c00.t) * (float)m_textureSpec.height / (float)viewportSize[1];

		m_lookupParams.lod = computeLodFromDerivates(DEFAULT_LOD_MODE, dudx, 0.0f, 0.0f, dvdy);

		m_textures.push_back(TextureBinding(m_textureCubeArray, m_textureSpec.sampler));
		break;
	}

	default:
		DE_ASSERT(DE_FALSE);
	}

	// Set lookup scale & bias
	m_lookupParams.scale  = fmtInfo.lookupScale;
	m_lookupParams.bias   = fmtInfo.lookupBias;
	m_lookupParams.offset = m_lookupSpec.offset;
}

void TextureShadowLodTestCase::initShaderSources(void)
{
	Function	   function		   = m_lookupSpec.function;
	bool		   isVtxCase	   = m_isVertexCase;
	bool		   hasLodBias	  = functionHasLod(m_lookupSpec.function) || m_lookupSpec.useBias;
	int			   texCoordComps   = m_textureSpec.type == TEXTURETYPE_2D ? 2 : 3;
	int			   extraCoordComps = 1; // For shadow ref
	bool		   hasSepShadowRef = m_lookupSpec.useSepRef;
	glu::DataType  coordType	   = glu::getDataTypeFloatVec(texCoordComps + extraCoordComps);
	glu::Precision coordPrec	   = glu::PRECISION_HIGHP;
	const char*	coordTypeName   = glu::getDataTypeName(coordType);
	const char*	coordPrecName   = glu::getPrecisionName(coordPrec);
	glu::DataType  samplerType	 = glu::TYPE_LAST;
	const char*	baseFuncName	= DE_NULL;

	switch (m_textureSpec.type)
	{
	case TEXTURETYPE_2D:
		samplerType = glu::TYPE_SAMPLER_2D_SHADOW;
		break;
	case TEXTURETYPE_CUBE_MAP:
		samplerType = glu::TYPE_SAMPLER_CUBE_SHADOW;
		break;
	case TEXTURETYPE_CUBE_MAP_ARRAY:
		samplerType = glu::TYPE_SAMPLER_CUBE_ARRAY_SHADOW;
		break;
	case TEXTURETYPE_2D_ARRAY:
		samplerType = glu::TYPE_SAMPLER_2D_ARRAY_SHADOW;
		break;
	default:
		DE_ASSERT(DE_FALSE);
	}

	switch (m_lookupSpec.function)
	{
	case FUNCTION_TEXTURE:
		baseFuncName = "texture";
		break;
	case FUNCTION_TEXTURELOD:
		baseFuncName = "textureLod";
		break;
	default:
		DE_ASSERT(DE_FALSE);
	}

	bool			 isGL		 = glu::isContextTypeGLCore(m_renderCtx.getType());
	glu::GLSLVersion glslVersion = glu::getContextTypeGLSLVersion(m_renderCtx.getType());

	std::string shaderVersion = glu::getGLSLVersionDeclaration(glslVersion);

	std::ostringstream  vert;
	std::ostringstream  frag;
	std::ostringstream& op = isVtxCase ? vert : frag;

	std::string cubeMapArrayEXT = "";

	// Check if we need to add the texture_cube_map_array extension.
	if (m_textureSpec.type == TEXTURETYPE_CUBE_MAP_ARRAY)
	{
		if (!glu::contextSupports(m_renderCtx.getType(), glu::ApiType::es(3, 2)) &&
			!glu::contextSupports(m_renderCtx.getType(), glu::ApiType::core(4, 0)))
		{
			if (isGL)
			{
				cubeMapArrayEXT = "#extension GL_ARB_texture_cube_map_array : require\n";
			}
			else
			{
				glu::ContextInfo* info = glu::ContextInfo::create(m_renderCtx);
				if (info->isExtensionSupported("GL_EXT_texture_cube_map_array"))
				{
					cubeMapArrayEXT = "#extension GL_EXT_texture_cube_map_array : require\n";
				}
				else
				{
					cubeMapArrayEXT = "#extension GL_OES_texture_cube_map_array : require\n";
				}
			}
		}
	}

	vert << shaderVersion << "\n"
		 << "#extension GL_EXT_texture_shadow_lod : require\n\n";

	vert << cubeMapArrayEXT;

	vert << "in highp vec4 a_position;\n"
		 << "in " << coordPrecName << " " << coordTypeName << " a_in0;\n";

	if (hasLodBias || hasSepShadowRef)
	{
		vert << "in " << coordPrecName << " vec4 a_in1;\n";
	}

	frag << shaderVersion << "\n"
		 << "#extension GL_EXT_texture_shadow_lod : require\n\n";

	frag << cubeMapArrayEXT;

	frag << "out mediump vec4 o_color;\n";

	if (isVtxCase)
	{
		vert << "out mediump vec4 v_color;\n";
		frag << "in mediump vec4 v_color;\n";
	}
	else
	{
		vert << "out " << coordPrecName << " " << coordTypeName << " v_texCoord;\n";
		frag << "in " << coordPrecName << " " << coordTypeName << " v_texCoord;\n";

		if (hasLodBias || hasSepShadowRef)
		{
			vert << "out " << coordPrecName << " vec4 v_lodShadowRef;\n";
			frag << "in " << coordPrecName << " vec4 v_lodShadowRef;\n";
		}
	}

	// Uniforms
	op << "uniform highp " << glu::getDataTypeName(samplerType) << " u_sampler;\n"
	   << "uniform highp vec4 u_scale;\n"
	   << "uniform highp vec4 u_bias;\n";

	vert << "\nvoid main()\n{\n"
		 << "\tgl_Position = a_position;\n";
	frag << "\nvoid main()\n{\n";

	if (isVtxCase)
		vert << "\tv_color = ";
	else
		frag << "\to_color = ";

	// Op.
	{
		const char* texCoord = isVtxCase ? "a_in0" : "v_texCoord";
		const char* lodBias  = isVtxCase ? "a_in1" : "v_lodShadowRef";

		op << "vec4(" << baseFuncName;
		if (m_lookupSpec.useOffset)
			op << "Offset";
		op << "(u_sampler, ";

		op << texCoord;

		if (m_lookupSpec.useSepRef)
		{
			op << ", " << lodBias << ".y";
		}

		if (functionHasLod(function))
		{
			op << ", " << lodBias << ".x";
		}

		if (m_lookupSpec.useOffset)
		{
			int offsetComps = 2;

			op << ", ivec" << offsetComps << "(";
			for (int ndx = 0; ndx < offsetComps; ndx++)
			{
				if (ndx != 0)
					op << ", ";
				op << m_lookupSpec.offset[ndx];
			}
			op << ")";
		}

		if (m_lookupSpec.useBias)
			op << ", " << lodBias << ".x";

		op << ")";

		op << ", 0.0, 0.0, 1.0)";

		op << ";\n";
	}

	if (isVtxCase)
		frag << "\to_color = v_color;\n";
	else
	{
		vert << "\tv_texCoord = a_in0;\n";

		if (hasLodBias || hasSepShadowRef)
		{
			vert << "\tv_lodShadowRef = a_in1;\n";
		}
	}

	vert << "}\n";
	frag << "}\n";

	m_vertShaderSource = vert.str();
	m_fragShaderSource = frag.str();
}

void TextureShadowLodTestCase::deinit(void)
{
	ShaderRenderCase::deinit();

	delete m_texture2D;
	delete m_textureCube;
	delete m_texture2DArray;

	m_texture2D		 = DE_NULL;
	m_textureCube	= DE_NULL;
	m_texture2DArray = DE_NULL;
}

void TextureShadowLodTestCase::setupUniforms(deUint32 programID, const tcu::Vec4&)
{
	const glw::Functions& gl = m_renderCtx.getFunctions();
	gl.uniform1i(gl.getUniformLocation(programID, "u_sampler"), 0);
	gl.uniform4fv(gl.getUniformLocation(programID, "u_scale"), 1, m_lookupParams.scale.getPtr());
	gl.uniform4fv(gl.getUniformLocation(programID, "u_bias"), 1, m_lookupParams.bias.getPtr());
}

TextureShadowLodTest::TextureShadowLodTest(Context& context)
	: TestCaseGroup(context, "ext_texture_shadow_lod", "Texture Access Function Tests")
{
}

TextureShadowLodTest::~TextureShadowLodTest(void)
{
}

enum CaseFlags
{
	VERTEX   = (1 << 0),
	FRAGMENT = (1 << 1),
	BOTH	 = VERTEX | FRAGMENT
};

struct TexFuncCaseSpec
{
	const char*		  name;
	TextureLookupSpec lookupSpec;
	TextureSpec		  texSpec;
	TexEvalFunc		  evalFunc;
	deUint32		  flags;
};

#define CASE_SPEC(NAME, FUNC, MINCOORD, MAXCOORD, USEBIAS, MINLOD, MAXLOD, USEOFFSET, OFFSET, USESEPREF, MINSEPREF, \
				  MAXSEPREF, TEXSPEC, EVALFUNC, FLAGS)                                                              \
	{                                                                                                               \
		#NAME, TextureLookupSpec(FUNC, MINCOORD, MAXCOORD, USEBIAS, MINLOD, MAXLOD, USEOFFSET, OFFSET, USESEPREF,   \
								 MINSEPREF, MAXSEPREF),                                                             \
								 TEXSPEC, EVALFUNC, FLAGS                                                           \
	}

static void createCaseGroup(TestCaseGroup* parent, const char* groupName, const char* groupDesc,
							const TexFuncCaseSpec* cases, int numCases)
{
	tcu::TestCaseGroup* group = new tcu::TestCaseGroup(parent->getTestContext(), groupName, groupDesc);
	parent->addChild(group);

	for (int ndx = 0; ndx < numCases; ndx++)
	{
		std::string name = cases[ndx].name;
		if (cases[ndx].flags & VERTEX)
			group->addChild(new TextureShadowLodTestCase(parent->getContext(), (name + "_vertex").c_str(), "",
														 cases[ndx].lookupSpec, cases[ndx].texSpec, cases[ndx].evalFunc,
														 true));
		if (cases[ndx].flags & FRAGMENT)
			group->addChild(new TextureShadowLodTestCase(parent->getContext(), (name + "_fragment").c_str(), "",
														 cases[ndx].lookupSpec, cases[ndx].texSpec, cases[ndx].evalFunc,
														 false));
	}
}

void TextureShadowLodTest::init(void)
{
	// Samplers
	static const tcu::Sampler samplerShadowNoMipmap(
		tcu::Sampler::REPEAT_GL, tcu::Sampler::REPEAT_GL, tcu::Sampler::REPEAT_GL, tcu::Sampler::NEAREST,
		tcu::Sampler::NEAREST, 0.0f /* LOD threshold */, true /* normalized coords */, tcu::Sampler::COMPAREMODE_LESS,
		0 /* cmp channel */, tcu::Vec4(0.0f) /* border color */, true /* seamless cube map */);
	static const tcu::Sampler samplerShadowMipmap(
		tcu::Sampler::REPEAT_GL, tcu::Sampler::REPEAT_GL, tcu::Sampler::REPEAT_GL, tcu::Sampler::NEAREST_MIPMAP_NEAREST,
		tcu::Sampler::NEAREST, 0.0f /* LOD threshold */, true /* normalized coords */, tcu::Sampler::COMPAREMODE_LESS,
		0 /* cmp channel */, tcu::Vec4(0.0f) /* border color */, true /* seamless cube map */);

	// Default textures.
	//													Type							Format					W		H		D	L	Sampler

	static const TextureSpec tex2DArrayShadow(TEXTURETYPE_2D_ARRAY, GL_DEPTH_COMPONENT16, 128, 128, 4, 1,
											  samplerShadowNoMipmap);
	static const TextureSpec texCubeShadow(TEXTURETYPE_CUBE_MAP, GL_DEPTH_COMPONENT16, 256, 256, 1, 1,
										   samplerShadowNoMipmap);
	static const TextureSpec texCubeMipmapShadow(TEXTURETYPE_CUBE_MAP, GL_DEPTH_COMPONENT16, 256, 256, 1, 9,
												 samplerShadowMipmap);
	static const TextureSpec texCubeArrayShadow(TEXTURETYPE_CUBE_MAP_ARRAY, GL_DEPTH_COMPONENT16, 128, 128, 4, 1,
												samplerShadowNoMipmap);
	static const TextureSpec texCubeArrayMipmapShadow(TEXTURETYPE_CUBE_MAP_ARRAY, GL_DEPTH_COMPONENT16, 128, 128, 4, 8,
													  samplerShadowMipmap);
	static const TextureSpec tex2DArrayMipmapShadow(TEXTURETYPE_2D_ARRAY, GL_DEPTH_COMPONENT16, 128, 128, 4, 8,
													samplerShadowMipmap);

	// texture() cases
	static const TexFuncCaseSpec textureCases[] = {
		//		  Name							Function			MinCoord							MaxCoord							Bias?	MinLod	MaxLod	Offset?	Offset		Format					EvalFunc				Flags
		CASE_SPEC(sampler2darrayshadow, FUNCTION_TEXTURE, Vec4(-1.2f, -0.4f, -0.5f, 0.0f), Vec4(1.5f, 2.3f, 3.5f, 1.0f),
				  false, 0.0f, 0.0f, false, IVec3(0), false, 0.0f, 0.0f, tex2DArrayShadow, evalTexture2DArrayShadow,
				  VERTEX),
		CASE_SPEC(sampler2darrayshadow, FUNCTION_TEXTURE, Vec4(-1.2f, -0.4f, -0.5f, 0.0f), Vec4(1.5f, 2.3f, 3.5f, 1.0f),
				  false, 0.0f, 0.0f, false, IVec3(0), false, 0.0f, 0.0f, tex2DArrayMipmapShadow,
				  evalTexture2DArrayShadow, FRAGMENT),
		CASE_SPEC(sampler2darrayshadow_bias, FUNCTION_TEXTURE, Vec4(-1.2f, -0.4f, -0.5f, 0.0f),
				  Vec4(1.5f, 2.3f, 3.5f, 1.0f), true, -2.0f, 2.0f, false, IVec3(0), false, 0.0f, 0.0f,
				  tex2DArrayMipmapShadow, evalTexture2DArrayShadowBias, FRAGMENT),

		CASE_SPEC(samplercubearrayshadow, FUNCTION_TEXTURE, Vec4(-1.0f, -1.0f, 1.01f, -0.5f),
				  Vec4(1.0f, 1.0f, 1.01f, 3.5f), false, 0.0f, 0.0f, false, IVec3(0), true, 0.0f, 1.0f,
				  texCubeArrayShadow, evalTextureCubeArrayShadow, VERTEX),
		CASE_SPEC(samplercubearrayshadow, FUNCTION_TEXTURE, Vec4(-1.0f, -1.0f, 1.01f, -0.5f),
				  Vec4(1.0f, 1.0f, 1.01f, 3.5f), false, 0.0f, 0.0f, false, IVec3(0), true, 0.0f, 1.0f,
				  texCubeArrayMipmapShadow, evalTextureCubeArrayShadow, FRAGMENT),
		CASE_SPEC(samplercubearrayshadow_bias, FUNCTION_TEXTURE, Vec4(-1.0f, -1.0f, 1.01f, -0.5f),
				  Vec4(1.0f, 1.0f, 1.01f, 3.5f), true, -2.0, 2.0f, false, IVec3(0), true, 0.0f, 1.0f,
				  texCubeArrayMipmapShadow, evalTextureCubeArrayShadowBias, FRAGMENT),
	};
	createCaseGroup(this, "texture", "texture() Tests", textureCases, DE_LENGTH_OF_ARRAY(textureCases));

	// textureOffset() cases
	// \note _bias variants are not using mipmap thanks to wide allowed range for LOD computation
	static const TexFuncCaseSpec textureOffsetCases[] = {
		//		  Name							Function			MinCoord							MaxCoord							Bias?	MinLod	MaxLod	Offset?	Offset				Format					EvalFunc						Flags
		CASE_SPEC(sampler2darrayshadow, FUNCTION_TEXTURE, Vec4(-1.2f, -0.4f, -0.5f, 0.0f), Vec4(1.5f, 2.3f, 3.5f, 1.0f),
				  false, 0.0f, 0.0f, true, IVec3(-8, 7, 0), false, 0.0f, 0.0f, tex2DArrayShadow,
				  evalTexture2DArrayShadowOffset, VERTEX),
		CASE_SPEC(sampler2darrayshadow, FUNCTION_TEXTURE, Vec4(-1.2f, -0.4f, -0.5f, 0.0f), Vec4(1.5f, 2.3f, 3.5f, 1.0f),
				  false, 0.0f, 0.0f, true, IVec3(7, -8, 0), false, 0.0f, 0.0f, tex2DArrayMipmapShadow,
				  evalTexture2DArrayShadowOffset, FRAGMENT),
		CASE_SPEC(sampler2darrayshadow_bias, FUNCTION_TEXTURE, Vec4(-1.2f, -0.4f, -0.5f, 0.0f),
				  Vec4(1.5f, 2.3f, 3.5f, 1.0f), true, -2.0f, 2.0f, true, IVec3(7, -8, 0), false, 0.0f, 0.0f,
				  tex2DArrayMipmapShadow, evalTexture2DArrayShadowOffsetBias, FRAGMENT),
	};
	createCaseGroup(this, "textureoffset", "textureOffset() Tests", textureOffsetCases,
					DE_LENGTH_OF_ARRAY(textureOffsetCases));

	// textureLod() cases
	static const TexFuncCaseSpec textureLodCases[] = {
		//		  Name							Function				MinCoord							MaxCoord							Bias?	MinLod	MaxLod	Offset?	Offset		Format					EvalFunc				Flags
		CASE_SPEC(sampler2darrayshadow, FUNCTION_TEXTURELOD, Vec4(-1.2f, -0.4f, -0.5f, 0.0f),
				  Vec4(1.5f, 2.3f, 3.5f, 1.0f), false, -1.0f, 8.0f, false, IVec3(0), false, 0.0f, 0.0f,
				  tex2DArrayMipmapShadow, evalTexture2DArrayShadowLod, BOTH),
		CASE_SPEC(samplercubeshadow, FUNCTION_TEXTURELOD, Vec4(-1.0f, -1.0f, 1.01f, 0.0f),
				  Vec4(1.0f, 1.0f, 1.01f, 1.0f), false, -1.0f, 8.0f, false, IVec3(0), false, 0.0f, 0.0f,
				  texCubeMipmapShadow, evalTextureCubeShadowLod, BOTH),
		CASE_SPEC(samplercubearrayshadow, FUNCTION_TEXTURELOD, Vec4(-1.0f, -1.0f, 1.01f, -0.5f),
				  Vec4(1.0f, 1.0f, 1.01f, 3.5f), false, -1.0f, 8.0f, false, IVec3(0), true, 0.0f, 1.0f,
				  texCubeArrayMipmapShadow, evalTextureCubeArrayShadowLod, FRAGMENT)
	};
	createCaseGroup(this, "texturelod", "textureLod() Tests", textureLodCases, DE_LENGTH_OF_ARRAY(textureLodCases));

	// textureLodOffset() cases
	static const TexFuncCaseSpec textureLodOffsetCases[] = {
		//		  Name							Function				MinCoord							MaxCoord							Bias?	MinLod	MaxLod	Offset?	Offset				Format					EvalFunc						Flags
		CASE_SPEC(sampler2darrayshadow, FUNCTION_TEXTURELOD, Vec4(-1.2f, -0.4f, -0.5f, 0.0f),
				  Vec4(1.5f, 2.3f, 3.5f, 1.0f), false, -1.0f, 9.0f, true, IVec3(-8, 7, 0), false, 0.0f, 0.0f,
				  tex2DArrayMipmapShadow, evalTexture2DArrayShadowLodOffset, BOTH),
	};
	createCaseGroup(this, "texturelodoffset", "textureLodOffset() Tests", textureLodOffsetCases,
					DE_LENGTH_OF_ARRAY(textureLodOffsetCases));
}
} // namespace Functional
} // namespace deqp
