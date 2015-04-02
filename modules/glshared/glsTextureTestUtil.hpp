#ifndef _GLSTEXTURETESTUTIL_HPP
#define _GLSTEXTURETESTUTIL_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL (ES) Module
 * -----------------------------------------------
 *
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
 * \brief Texture test utilities.
 *
 * About coordinates:
 *  + Quads consist of 2 triangles, rendered using explicit indices.
 *  + All TextureTestUtil functions and classes expect texture coordinates
 *    for quads to be specified in order (-1, -1), (-1, 1), (1, -1), (1, 1).
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTexture.hpp"
#include "tcuSurface.hpp"
#include "tcuPixelFormat.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuTestContext.hpp"
#include "tcuTestLog.hpp"
#include "tcuCompressedTexture.hpp"
#include "tcuTextureUtil.hpp"

#include "gluShaderProgram.hpp"
#include "gluShaderUtil.hpp"

#include "deMath.h"
#include "deInt32.h"

#include <map>

namespace tcu
{
struct LookupPrecision;
struct LodPrecision;
struct TexComparePrecision;
}

namespace deqp
{
namespace gls
{
namespace TextureTestUtil
{

enum TextureType
{
	TEXTURETYPE_2D = 0,
	TEXTURETYPE_CUBE,
	TEXTURETYPE_2D_ARRAY,
	TEXTURETYPE_3D,
	TEXTURETYPE_CUBE_ARRAY,
	TEXTURETYPE_1D,
	TEXTURETYPE_1D_ARRAY,
	TEXTURETYPE_BUFFER,

	TEXTURETYPE_LAST
};

enum SamplerType
{
	SAMPLERTYPE_FLOAT,
	SAMPLERTYPE_INT,
	SAMPLERTYPE_UINT,
	SAMPLERTYPE_SHADOW,

	SAMPLERTYPE_FETCH_FLOAT,
	SAMPLERTYPE_FETCH_INT,
	SAMPLERTYPE_FETCH_UINT,

	SAMPLERTYPE_LAST
};

SamplerType		getSamplerType		(tcu::TextureFormat format);
SamplerType		getFetchSamplerType	(tcu::TextureFormat format);

struct RenderParams
{
	enum Flags
	{
		PROJECTED		= (1<<0),
		USE_BIAS		= (1<<1),
		LOG_PROGRAMS	= (1<<2),
		LOG_UNIFORMS	= (1<<3),

		LOG_ALL			= LOG_PROGRAMS|LOG_UNIFORMS
	};

	RenderParams (TextureType texType_)
		: texType		(texType_)
		, samplerType	(SAMPLERTYPE_FLOAT)
		, flags			(0)
		, w				(1.0f)
		, bias			(0.0f)
		, ref			(0.0f)
		, colorScale	(1.0f)
		, colorBias		(0.0f)
	{
	}

	TextureType		texType;		//!< Texture type.
	SamplerType		samplerType;	//!< Sampler type.
	deUint32		flags;			//!< Feature flags.
	tcu::Vec4		w;				//!< w coordinates for quad vertices.
	float			bias;			//!< User-supplied bias.
	float			ref;			//!< Reference value for shadow lookups.

	// color = lookup() * scale + bias
	tcu::Vec4		colorScale;		//!< Scale for texture color values.
	tcu::Vec4		colorBias;		//!< Bias for texture color values.
};

enum Program
{
	PROGRAM_2D_FLOAT = 0,
	PROGRAM_2D_INT,
	PROGRAM_2D_UINT,
	PROGRAM_2D_SHADOW,

	PROGRAM_2D_FLOAT_BIAS,
	PROGRAM_2D_INT_BIAS,
	PROGRAM_2D_UINT_BIAS,
	PROGRAM_2D_SHADOW_BIAS,

	PROGRAM_1D_FLOAT,
	PROGRAM_1D_INT,
	PROGRAM_1D_UINT,
	PROGRAM_1D_SHADOW,

	PROGRAM_1D_FLOAT_BIAS,
	PROGRAM_1D_INT_BIAS,
	PROGRAM_1D_UINT_BIAS,
	PROGRAM_1D_SHADOW_BIAS,

	PROGRAM_CUBE_FLOAT,
	PROGRAM_CUBE_INT,
	PROGRAM_CUBE_UINT,
	PROGRAM_CUBE_SHADOW,

	PROGRAM_CUBE_FLOAT_BIAS,
	PROGRAM_CUBE_INT_BIAS,
	PROGRAM_CUBE_UINT_BIAS,
	PROGRAM_CUBE_SHADOW_BIAS,

	PROGRAM_1D_ARRAY_FLOAT,
	PROGRAM_1D_ARRAY_INT,
	PROGRAM_1D_ARRAY_UINT,
	PROGRAM_1D_ARRAY_SHADOW,

	PROGRAM_2D_ARRAY_FLOAT,
	PROGRAM_2D_ARRAY_INT,
	PROGRAM_2D_ARRAY_UINT,
	PROGRAM_2D_ARRAY_SHADOW,

	PROGRAM_3D_FLOAT,
	PROGRAM_3D_INT,
	PROGRAM_3D_UINT,

	PROGRAM_3D_FLOAT_BIAS,
	PROGRAM_3D_INT_BIAS,
	PROGRAM_3D_UINT_BIAS,

	PROGRAM_CUBE_ARRAY_FLOAT,
	PROGRAM_CUBE_ARRAY_INT,
	PROGRAM_CUBE_ARRAY_UINT,
	PROGRAM_CUBE_ARRAY_SHADOW,

	PROGRAM_BUFFER_FLOAT,
	PROGRAM_BUFFER_INT,
	PROGRAM_BUFFER_UINT,

	PROGRAM_LAST
};

class ProgramLibrary
{
public:
											ProgramLibrary			(const glu::RenderContext& context, tcu::TestLog& log, glu::GLSLVersion glslVersion, glu::Precision texCoordPrecision);
											~ProgramLibrary			(void);

	glu::ShaderProgram*						getProgram				(Program program);
	void									clear					(void);

private:
											ProgramLibrary			(const ProgramLibrary& other);
	ProgramLibrary&							operator=				(const ProgramLibrary& other);

	const glu::RenderContext&				m_context;
	tcu::TestLog&							m_log;
	glu::GLSLVersion						m_glslVersion;
	glu::Precision							m_texCoordPrecision;
	std::map<Program, glu::ShaderProgram*>	m_programs;
};

class TextureRenderer
{
public:
								TextureRenderer			(const glu::RenderContext& context, tcu::TestLog& log, glu::GLSLVersion glslVersion, glu::Precision texCoordPrecision);
								~TextureRenderer		(void);

	void						clear					(void); //!< Frees allocated resources. Destructor will call clear() as well.

	void						renderQuad				(int texUnit, const float* texCoord, TextureType texType);
	void						renderQuad				(int texUnit, const float* texCoord, const RenderParams& params);

private:
								TextureRenderer			(const TextureRenderer& other);
	TextureRenderer&			operator=				(const TextureRenderer& other);

	const glu::RenderContext&	m_renderCtx;
	tcu::TestLog&				m_log;
	ProgramLibrary				m_programLibrary;
};

class RandomViewport
{
public:
	int		x;
	int		y;
	int		width;
	int		height;

	RandomViewport (const tcu::RenderTarget& renderTarget, int preferredWidth, int preferredHeight, deUint32 seed);
};

inline tcu::RGBA toRGBA (const tcu::Vec4& v)
{
	return tcu::RGBA(tcu::floatToU8(v.x()),
					 tcu::floatToU8(v.y()),
					 tcu::floatToU8(v.z()),
					 tcu::floatToU8(v.w()));
}

inline tcu::RGBA toRGBAMasked (const tcu::Vec4& v, deUint8 mask)
{
	return tcu::RGBA((mask&tcu::RGBA::RED_MASK)		? tcu::floatToU8(v.x()) : 0,
					 (mask&tcu::RGBA::GREEN_MASK)	? tcu::floatToU8(v.y()) : 0,
					 (mask&tcu::RGBA::BLUE_MASK)	? tcu::floatToU8(v.z()) : 0,
					 (mask&tcu::RGBA::ALPHA_MASK)	? tcu::floatToU8(v.w()) : 0xFF); //!< \note Alpha defaults to full saturation when reading masked format
}

inline tcu::Vec4 toVec4 (const tcu::RGBA& c)
{
	return tcu::Vec4(c.getRed()		/ 255.0f,
					 c.getGreen()	/ 255.0f,
					 c.getBlue()	/ 255.0f,
					 c.getAlpha()	/ 255.0f);
}

inline deUint8 getColorMask (const tcu::PixelFormat& format)
{
	return (format.redBits		? tcu::RGBA::RED_MASK	: 0) |
		   (format.greenBits	? tcu::RGBA::GREEN_MASK	: 0) |
		   (format.blueBits		? tcu::RGBA::BLUE_MASK	: 0) |
		   (format.alphaBits	? tcu::RGBA::ALPHA_MASK	: 0);
}

inline tcu::IVec4 getBitsVec (const tcu::PixelFormat& format)
{
	return tcu::IVec4(format.redBits, format.greenBits, format.blueBits, format.alphaBits);
}

inline tcu::BVec4 getCompareMask (const tcu::PixelFormat& format)
{
	return tcu::BVec4(format.redBits	> 0,
					  format.greenBits	> 0,
					  format.blueBits	> 0,
					  format.alphaBits	> 0);
}

// \todo [2012-02-09 pyry] Move to tcuSurfaceAccess?
class SurfaceAccess
{
public:
							SurfaceAccess		(tcu::Surface& surface, const tcu::PixelFormat& colorFmt);
							SurfaceAccess		(tcu::Surface& surface, const tcu::PixelFormat& colorFmt, int x, int y, int width, int height);
							SurfaceAccess		(const SurfaceAccess& parent, int x, int y, int width, int height);

	int						getWidth			(void) const	{ return m_width;	}
	int						getHeight			(void) const	{ return m_height;	}

	void					setPixel			(const tcu::Vec4& color, int x, int y) const;

private:
	mutable tcu::Surface*	m_surface;
	deUint8					m_colorMask;
	int						m_x;
	int						m_y;
	int						m_width;
	int						m_height;
};

inline void SurfaceAccess::setPixel (const tcu::Vec4& color, int x, int y) const
{
	DE_ASSERT(de::inBounds(x, 0, m_width) && de::inBounds(y, 0, m_height));
	m_surface->setPixel(m_x+x, m_y+y, toRGBAMasked(color, m_colorMask));
}

enum LodMode
{
	LODMODE_EXACT = 0,		//!< Ideal lod computation.
	LODMODE_MIN_BOUND,		//!< Use estimation range minimum bound.
	LODMODE_MAX_BOUND,		//!< Use estimation range maximum bound.

	LODMODE_LAST
};

struct ReferenceParams : public RenderParams
{
	ReferenceParams (TextureType texType_)
		: RenderParams	(texType_)
		, sampler		()
		, lodMode		(LODMODE_EXACT)
		, minLod		(-1000.0f)
		, maxLod		(1000.0f)
		, baseLevel		(0)
		, maxLevel		(1000)
	{
	}

	ReferenceParams (TextureType texType_, const tcu::Sampler& sampler_, LodMode lodMode_ = LODMODE_EXACT)
		: RenderParams	(texType_)
		, sampler		(sampler_)
		, lodMode		(lodMode_)
		, minLod		(-1000.0f)
		, maxLod		(1000.0f)
		, baseLevel		(0)
		, maxLevel		(1000)
	{
	}

	tcu::Sampler		sampler;
	LodMode				lodMode;
	float				minLod;
	float				maxLod;
	int					baseLevel;
	int					maxLevel;
};

void			clear						(const SurfaceAccess& dst, const tcu::Vec4& color);

// Similar to sampleTexture() except uses texelFetch.
void			fetchTexture				(const SurfaceAccess& dst, const tcu::ConstPixelBufferAccess& src, const float* texCoord, const tcu::Vec4& colorScale, const tcu::Vec4& colorBias);

void			sampleTexture				(const SurfaceAccess& dst, const tcu::Texture2DView&		src, const float* texCoord, const ReferenceParams& params);
void			sampleTexture				(const SurfaceAccess& dst, const tcu::TextureCubeView&		src, const float* texCoord, const ReferenceParams& params);
void			sampleTexture				(const SurfaceAccess& dst, const tcu::Texture2DArrayView&	src, const float* texCoord, const ReferenceParams& params);
void			sampleTexture				(const SurfaceAccess& dst, const tcu::Texture3DView&		src, const float* texCoord, const ReferenceParams& params);
void			sampleTexture				(const SurfaceAccess& dst, const tcu::TextureCubeArrayView&	src, const float* texCoord, const ReferenceParams& params);
void			sampleTexture				(const SurfaceAccess& dst, const tcu::Texture1DView&		src, const float* texCoord, const ReferenceParams& params);
void			sampleTexture				(const SurfaceAccess& dst, const tcu::Texture1DArrayView&	src, const float* texCoord, const ReferenceParams& params);

void			computeQuadTexCoord1D			(std::vector<float>& dst, float left, float right);
void			computeQuadTexCoord1DArray		(std::vector<float>& dst, int layerNdx, float left, float right);
void			computeQuadTexCoord2D			(std::vector<float>& dst, const tcu::Vec2& bottomLeft, const tcu::Vec2& topRight);
void			computeQuadTexCoord2DArray		(std::vector<float>& dst, int layerNdx, const tcu::Vec2& bottomLeft, const tcu::Vec2& topRight);
void			computeQuadTexCoord3D			(std::vector<float>& dst, const tcu::Vec3& p0, const tcu::Vec3& p1, const tcu::IVec3& dirSwz);
void			computeQuadTexCoordCube			(std::vector<float>& dst, tcu::CubeFace face);
void			computeQuadTexCoordCube			(std::vector<float>& dst, tcu::CubeFace face, const tcu::Vec2& bottomLeft, const tcu::Vec2& topRight);
void			computeQuadTexCoordCubeArray	(std::vector<float>& dst, tcu::CubeFace face, const tcu::Vec2& bottomLeft, const tcu::Vec2& topRight, const tcu::Vec2& layerRange);

bool			compareImages				(tcu::TestLog& log, const char* name, const char* desc, const tcu::Surface& reference, const tcu::Surface& rendered, tcu::RGBA threshold);
bool			compareImages				(tcu::TestLog& log, const tcu::Surface& reference, const tcu::Surface& rendered, tcu::RGBA threshold);
int				measureAccuracy				(tcu::TestLog& log, const tcu::Surface& reference, const tcu::Surface& rendered, int bestScoreDiff, int worstScoreDiff);

int				computeTextureLookupDiff	(const tcu::ConstPixelBufferAccess&	result,
											 const tcu::ConstPixelBufferAccess&	reference,
											 const tcu::PixelBufferAccess&		errorMask,
											 const tcu::Texture1DView&			src,
											 const float*						texCoord,
											 const ReferenceParams&				sampleParams,
											 const tcu::LookupPrecision&		lookupPrec,
											 const tcu::LodPrecision&			lodPrec,
											 qpWatchDog*						watchDog);

int				computeTextureLookupDiff	(const tcu::ConstPixelBufferAccess&	result,
											 const tcu::ConstPixelBufferAccess&	reference,
											 const tcu::PixelBufferAccess&		errorMask,
											 const tcu::Texture2DView&			src,
											 const float*						texCoord,
											 const ReferenceParams&				sampleParams,
											 const tcu::LookupPrecision&		lookupPrec,
											 const tcu::LodPrecision&			lodPrec,
											 qpWatchDog*						watchDog);

int				computeTextureLookupDiff	(const tcu::ConstPixelBufferAccess&	result,
											 const tcu::ConstPixelBufferAccess&	reference,
											 const tcu::PixelBufferAccess&		errorMask,
											 const tcu::TextureCubeView&		src,
											 const float*						texCoord,
											 const ReferenceParams&				sampleParams,
											 const tcu::LookupPrecision&		lookupPrec,
											 const tcu::LodPrecision&			lodPrec,
											 qpWatchDog*						watchDog);

int				computeTextureLookupDiff	(const tcu::ConstPixelBufferAccess&	result,
											 const tcu::ConstPixelBufferAccess&	reference,
											 const tcu::PixelBufferAccess&		errorMask,
											 const tcu::Texture1DArrayView&		src,
											 const float*						texCoord,
											 const ReferenceParams&				sampleParams,
											 const tcu::LookupPrecision&		lookupPrec,
											 const tcu::LodPrecision&			lodPrec,
											 qpWatchDog*						watchDog);

int				computeTextureLookupDiff	(const tcu::ConstPixelBufferAccess&	result,
											 const tcu::ConstPixelBufferAccess&	reference,
											 const tcu::PixelBufferAccess&		errorMask,
											 const tcu::Texture2DArrayView&		src,
											 const float*						texCoord,
											 const ReferenceParams&				sampleParams,
											 const tcu::LookupPrecision&		lookupPrec,
											 const tcu::LodPrecision&			lodPrec,
											 qpWatchDog*						watchDog);

int				computeTextureLookupDiff	(const tcu::ConstPixelBufferAccess&	result,
											 const tcu::ConstPixelBufferAccess&	reference,
											 const tcu::PixelBufferAccess&		errorMask,
											 const tcu::Texture3DView&			src,
											 const float*						texCoord,
											 const ReferenceParams&				sampleParams,
											 const tcu::LookupPrecision&		lookupPrec,
											 const tcu::LodPrecision&			lodPrec,
											 qpWatchDog*						watchDog);

int				computeTextureLookupDiff	(const tcu::ConstPixelBufferAccess&	result,
											 const tcu::ConstPixelBufferAccess&	reference,
											 const tcu::PixelBufferAccess&		errorMask,
											 const tcu::TextureCubeArrayView&	src,
											 const float*						texCoord,
											 const ReferenceParams&				sampleParams,
											 const tcu::LookupPrecision&		lookupPrec,
											 const tcu::IVec4&					coordBits,
											 const tcu::LodPrecision&			lodPrec,
											 qpWatchDog*						watchDog);

bool			verifyTextureResult			(tcu::TestContext&					testCtx,
											 const tcu::ConstPixelBufferAccess&	result,
											 const tcu::Texture1DView&			src,
											 const float*						texCoord,
											 const ReferenceParams&				sampleParams,
											 const tcu::LookupPrecision&		lookupPrec,
											 const tcu::LodPrecision&			lodPrec,
											 const tcu::PixelFormat&			pixelFormat);

bool			verifyTextureResult			(tcu::TestContext&					testCtx,
											 const tcu::ConstPixelBufferAccess&	result,
											 const tcu::Texture2DView&			src,
											 const float*						texCoord,
											 const ReferenceParams&				sampleParams,
											 const tcu::LookupPrecision&		lookupPrec,
											 const tcu::LodPrecision&			lodPrec,
											 const tcu::PixelFormat&			pixelFormat);

bool			verifyTextureResult			(tcu::TestContext&					testCtx,
											 const tcu::ConstPixelBufferAccess&	result,
											 const tcu::TextureCubeView&		src,
											 const float*						texCoord,
											 const ReferenceParams&				sampleParams,
											 const tcu::LookupPrecision&		lookupPrec,
											 const tcu::LodPrecision&			lodPrec,
											 const tcu::PixelFormat&			pixelFormat);

bool			verifyTextureResult			(tcu::TestContext&					testCtx,
											 const tcu::ConstPixelBufferAccess&	result,
											 const tcu::Texture1DArrayView&		src,
											 const float*						texCoord,
											 const ReferenceParams&				sampleParams,
											 const tcu::LookupPrecision&		lookupPrec,
											 const tcu::LodPrecision&			lodPrec,
											 const tcu::PixelFormat&			pixelFormat);

bool			verifyTextureResult			(tcu::TestContext&					testCtx,
											 const tcu::ConstPixelBufferAccess&	result,
											 const tcu::Texture2DArrayView&		src,
											 const float*						texCoord,
											 const ReferenceParams&				sampleParams,
											 const tcu::LookupPrecision&		lookupPrec,
											 const tcu::LodPrecision&			lodPrec,
											 const tcu::PixelFormat&			pixelFormat);

bool			verifyTextureResult			(tcu::TestContext&					testCtx,
											 const tcu::ConstPixelBufferAccess&	result,
											 const tcu::Texture3DView&			src,
											 const float*						texCoord,
											 const ReferenceParams&				sampleParams,
											 const tcu::LookupPrecision&		lookupPrec,
											 const tcu::LodPrecision&			lodPrec,
											 const tcu::PixelFormat&			pixelFormat);

bool			verifyTextureResult			(tcu::TestContext&					testCtx,
											 const tcu::ConstPixelBufferAccess&	result,
											 const tcu::TextureCubeArrayView&	src,
											 const float*						texCoord,
											 const ReferenceParams&				sampleParams,
											 const tcu::LookupPrecision&		lookupPrec,
											 const tcu::IVec4&					coordBits,
											 const tcu::LodPrecision&			lodPrec,
											 const tcu::PixelFormat&			pixelFormat);

int				computeTextureCompareDiff	(const tcu::ConstPixelBufferAccess&	result,
											 const tcu::ConstPixelBufferAccess&	reference,
											 const tcu::PixelBufferAccess&		errorMask,
											 const tcu::Texture2DView&			src,
											 const float*						texCoord,
											 const ReferenceParams&				sampleParams,
											 const tcu::TexComparePrecision&	comparePrec,
											 const tcu::LodPrecision&			lodPrec,
											 const tcu::Vec3&					nonShadowThreshold);

int				computeTextureCompareDiff	(const tcu::ConstPixelBufferAccess&	result,
											 const tcu::ConstPixelBufferAccess&	reference,
											 const tcu::PixelBufferAccess&		errorMask,
											 const tcu::TextureCubeView&		src,
											 const float*						texCoord,
											 const ReferenceParams&				sampleParams,
											 const tcu::TexComparePrecision&	comparePrec,
											 const tcu::LodPrecision&			lodPrec,
											 const tcu::Vec3&					nonShadowThreshold);

int				computeTextureCompareDiff	(const tcu::ConstPixelBufferAccess&	result,
											 const tcu::ConstPixelBufferAccess&	reference,
											 const tcu::PixelBufferAccess&		errorMask,
											 const tcu::Texture2DArrayView&		src,
											 const float*						texCoord,
											 const ReferenceParams&				sampleParams,
											 const tcu::TexComparePrecision&	comparePrec,
											 const tcu::LodPrecision&			lodPrec,
											 const tcu::Vec3&					nonShadowThreshold);

// Mipmap generation comparison.

struct GenMipmapPrecision
{
	tcu::IVec3			filterBits;			//!< Bits in filtering parameters (fixed-point).
	tcu::Vec4			colorThreshold;		//!< Threshold for color value comparison.
	tcu::BVec4			colorMask;			//!< Color channel comparison mask.
};

qpTestResult	compareGenMipmapResult		(tcu::TestLog& log, const tcu::Texture2D& resultTexture, const tcu::Texture2D& level0Reference, const GenMipmapPrecision& precision);
qpTestResult	compareGenMipmapResult		(tcu::TestLog& log, const tcu::TextureCube& resultTexture, const tcu::TextureCube& level0Reference, const GenMipmapPrecision& precision);

// Utility for logging texture gradient ranges.
struct LogGradientFmt
{
	LogGradientFmt (const tcu::Vec4* min_, const tcu::Vec4* max_) : valueMin(min_), valueMax(max_) {}
	const tcu::Vec4* valueMin;
	const tcu::Vec4* valueMax;
};

std::ostream&			operator<<		(std::ostream& str, const LogGradientFmt& fmt);
inline LogGradientFmt	formatGradient	(const tcu::Vec4* minVal, const tcu::Vec4* maxVal) { return LogGradientFmt(minVal, maxVal); }

} // TextureTestUtil
} // gls
} // deqp

#endif // _GLSTEXTURETESTUTIL_HPP
