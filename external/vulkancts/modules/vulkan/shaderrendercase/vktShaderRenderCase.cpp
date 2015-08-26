/*------------------------------------------------------------------------
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be included
 * in all copies or substantial portions of the Materials.
 *
 * The Materials are Confidential Information as defined by the
 * Khronos Membership Agreement until designated non-confidential by Khronos,
 * at which point this condition clause shall be removed.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 *
 *//*!
 * \file
 * \brief Vulkan ShaderRenderCase
 *//*--------------------------------------------------------------------*/

#include "vktShaderRenderCase.hpp"

#include "tcuImageCompare.hpp"
#include "tcuSurface.hpp"
#include "tcuVector.hpp"
#include "tcuTestLog.hpp"

#include "deMath.h"

#include <vector>
#include <string>

namespace vkt
{
namespace shaderrendercase
{

using namespace std;
using namespace tcu;

static const int		GRID_SIZE			= 64;
static const int		MAX_RENDER_WIDTH	= 128;
static const int		MAX_RENDER_HEIGHT	= 112;
static const tcu::Vec4	DEFAULT_CLEAR_COLOR	= tcu::Vec4(0.125f, 0.25f, 0.5f, 1.0f);

// QuadGrid.

class QuadGrid
{
public:
                            QuadGrid                (int gridSize, int screenWidth, int screenHeight, const Vec4& constCoords, const vector<Mat4>& userAttribTransforms
/*, const vector<TextureBinding>& textures*/);
                            ~QuadGrid               (void);

    int                     getGridSize             (void) const { return m_gridSize; }
    int                     getNumVertices          (void) const { return m_numVertices; }
    int                     getNumTriangles         (void) const { return m_numTriangles; }
    const Vec4&             getConstCoords          (void) const { return m_constCoords; }
    const vector<Mat4>      getUserAttribTransforms (void) const { return m_userAttribTransforms; }
	// TODO:
    //const vector<TextureBinding>&   getTextures     (void) const { return m_textures; }

    const Vec4*             getPositions            (void) const { return &m_positions[0]; }
    const float*            getAttribOne            (void) const { return &m_attribOne[0]; }
    const Vec4*             getCoords               (void) const { return &m_coords[0]; }
    const Vec4*             getUnitCoords           (void) const { return &m_unitCoords[0]; }
    const Vec4*             getUserAttrib           (int attribNdx) const { return &m_userAttribs[attribNdx][0]; }
    const deUint16*         getIndices              (void) const { return &m_indices[0]; }

    Vec4                    getCoords               (float sx, float sy) const;
    Vec4                    getUnitCoords           (float sx, float sy) const;

    int                     getNumUserAttribs       (void) const { return (int)m_userAttribTransforms.size(); }
    Vec4                    getUserAttrib           (int attribNdx, float sx, float sy) const;

private:
    int                     m_gridSize;
    int                     m_numVertices;
    int                     m_numTriangles;
    Vec4                    m_constCoords;
    vector<Mat4>            m_userAttribTransforms;
	// TODO:
    // vector<TextureBinding>  m_textures;

    vector<Vec4>            m_screenPos;
    vector<Vec4>            m_positions;
    vector<Vec4>            m_coords;           //!< Near-unit coordinates, roughly [-2.0 .. 2.0].
    vector<Vec4>            m_unitCoords;       //!< Positive-only coordinates [0.0 .. 1.5].
    vector<float>           m_attribOne;
    vector<Vec4>            m_userAttribs[ShaderEvalContext::MAX_TEXTURES];
    vector<deUint16>        m_indices;
};

QuadGrid::QuadGrid (int gridSize, int width, int height, const Vec4& constCoords, const vector<Mat4>& userAttribTransforms
/*, const vector<TextureBinding>& textures*/)
    : m_gridSize                (gridSize)
    , m_numVertices             ((gridSize + 1) * (gridSize + 1))
    , m_numTriangles            (gridSize * gridSize * 2)
    , m_constCoords             (constCoords)
    , m_userAttribTransforms    (userAttribTransforms)
//    , m_textures                (textures)
{
    Vec4 viewportScale = Vec4((float)width, (float)height, 0.0f, 0.0f);

    // Compute vertices.
    m_positions.resize(m_numVertices);
    m_coords.resize(m_numVertices);
    m_unitCoords.resize(m_numVertices);
    m_attribOne.resize(m_numVertices);
    m_screenPos.resize(m_numVertices);

    // User attributes.
    for (int i = 0; i < DE_LENGTH_OF_ARRAY(m_userAttribs); i++)
        m_userAttribs[i].resize(m_numVertices);

    for (int y = 0; y < gridSize+1; y++)
    for (int x = 0; x < gridSize+1; x++)
    {
        float               sx          = (float)x / (float)gridSize;
        float               sy          = (float)y / (float)gridSize;
        float               fx          = 2.0f * sx - 1.0f;
        float               fy          = 2.0f * sy - 1.0f;
        int                 vtxNdx      = ((y * (gridSize+1)) + x);

        m_positions[vtxNdx]     = Vec4(fx, fy, 0.0f, 1.0f);
        m_attribOne[vtxNdx]     = 1.0f;
        m_screenPos[vtxNdx]     = Vec4(sx, sy, 0.0f, 1.0f) * viewportScale;
        m_coords[vtxNdx]        = getCoords(sx, sy);
        m_unitCoords[vtxNdx]    = getUnitCoords(sx, sy);

        for (int attribNdx = 0; attribNdx < getNumUserAttribs(); attribNdx++)
            m_userAttribs[attribNdx][vtxNdx] = getUserAttrib(attribNdx, sx, sy);
    }

    // Compute indices.
    m_indices.resize(3 * m_numTriangles);
    for (int y = 0; y < gridSize; y++)
    for (int x = 0; x < gridSize; x++)
    {
        int stride = gridSize + 1;
        int v00 = (y * stride) + x;
        int v01 = (y * stride) + x + 1;
        int v10 = ((y+1) * stride) + x;
        int v11 = ((y+1) * stride) + x + 1;

        int baseNdx = ((y * gridSize) + x) * 6;
        m_indices[baseNdx + 0] = (deUint16)v10;
        m_indices[baseNdx + 1] = (deUint16)v00;
        m_indices[baseNdx + 2] = (deUint16)v01;

        m_indices[baseNdx + 3] = (deUint16)v10;
        m_indices[baseNdx + 4] = (deUint16)v01;
        m_indices[baseNdx + 5] = (deUint16)v11;
    }
}

QuadGrid::~QuadGrid (void)
{
}

inline Vec4 QuadGrid::getCoords (float sx, float sy) const
{
    float fx = 2.0f * sx - 1.0f;
    float fy = 2.0f * sy - 1.0f;
    return Vec4(fx, fy, -fx + 0.33f*fy, -0.275f*fx - fy);
}

inline Vec4 QuadGrid::getUnitCoords (float sx, float sy) const
{
    return Vec4(sx, sy, 0.33f*sx + 0.5f*sy, 0.5f*sx + 0.25f*sy);
}

inline Vec4 QuadGrid::getUserAttrib (int attribNdx, float sx, float sy) const
{
    // homogeneous normalized screen-space coordinates
    return m_userAttribTransforms[attribNdx] * Vec4(sx, sy, 0.0f, 1.0f);
}



// ShaderEvalContext.

ShaderEvalContext::ShaderEvalContext (const QuadGrid& quadGrid_)
	: constCoords(quadGrid_.getConstCoords())
	, isDiscarded(false)
	, quadGrid(quadGrid_)
{
	// TODO...
/*
    const vector<TextureBinding>& bindings = quadGrid.getTextures();
    DE_ASSERT((int)bindings.size() <= MAX_TEXTURES);

    // Fill in texture array.
    for (int ndx = 0; ndx < (int)bindings.size(); ndx++)
    {
        const TextureBinding& binding = bindings[ndx];

        if (binding.getType() == TextureBinding::TYPE_NONE)
            continue;

        textures[ndx].sampler = binding.getSampler();

        switch (binding.getType())
        {
            case TextureBinding::TYPE_2D:       textures[ndx].tex2D         = &binding.get2D()->getRefTexture();        break;
            case TextureBinding::TYPE_CUBE_MAP: textures[ndx].texCube       = &binding.getCube()->getRefTexture();      break;
            case TextureBinding::TYPE_2D_ARRAY: textures[ndx].tex2DArray    = &binding.get2DArray()->getRefTexture();   break;
            case TextureBinding::TYPE_3D:       textures[ndx].tex3D         = &binding.get3D()->getRefTexture();        break;
            default:
                DE_ASSERT(DE_FALSE);
        }
    }
*/
}

ShaderEvalContext::~ShaderEvalContext (void)
{
}

void ShaderEvalContext::reset (float sx, float sy)
{
    // Clear old values
    color       = Vec4(0.0f, 0.0f, 0.0f, 1.0f);
    isDiscarded = false;

    // Compute coords
    coords      = quadGrid.getCoords(sx, sy);
    unitCoords  = quadGrid.getUnitCoords(sx, sy);

    // Compute user attributes.
    int numAttribs = quadGrid.getNumUserAttribs();
    DE_ASSERT(numAttribs <= MAX_USER_ATTRIBS);
    for (int attribNdx = 0; attribNdx < numAttribs; attribNdx++)
        in[attribNdx] = quadGrid.getUserAttrib(attribNdx, sx, sy);
}

tcu::Vec4 ShaderEvalContext::texture2D (int unitNdx, const tcu::Vec2& texCoords)
{
	// TODO: add texture binding
/*    if (textures[unitNdx].tex2D)
        return textures[unitNdx].tex2D->sample(textures[unitNdx].sampler, texCoords.x(), texCoords.y(), 0.0f);
    else
*/
        return tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
}


// ShaderEvaluator.

ShaderEvaluator::ShaderEvaluator (void)
    : m_evalFunc(DE_NULL)
{
}

ShaderEvaluator::ShaderEvaluator (ShaderEvalFunc evalFunc)
    : m_evalFunc(evalFunc)
{
}

ShaderEvaluator::~ShaderEvaluator (void)
{
}

void ShaderEvaluator::evaluate (ShaderEvalContext& ctx)
{
    DE_ASSERT(m_evalFunc);
    m_evalFunc(ctx);
}

// ShaderRenderCase
ShaderRenderCase::ShaderRenderCase	(tcu::TestContext& testCtx,
									const string& name,
									const string& description,
									bool isVertexCase,
									ShaderEvalFunc evalFunc)
	: vkt::TestCase(testCtx, name, description)
	, m_isVertexCase(isVertexCase)
	, m_evaluator(new ShaderEvaluator(evalFunc))
{
}

ShaderRenderCase::ShaderRenderCase	(tcu::TestContext& testCtx,
									const string& name,
									const string& description,
									bool isVertexCase,
									ShaderEvaluator* evaluator)
	: vkt::TestCase(testCtx, name, description)
	, m_isVertexCase(isVertexCase)
	, m_evaluator(evaluator)
{
}

ShaderRenderCase::~ShaderRenderCase (void)
{
}

void ShaderRenderCase::initPrograms (vk::ProgramCollection<glu::ProgramSources>& programCollection) const
{
	if (!m_vertShaderSource.empty())
		programCollection.add(m_name + "_vert") << glu::VertexSource(m_vertShaderSource);

	if (!m_fragShaderSource.empty())
		programCollection.add(m_name + "_frag") << glu::FragmentSource(m_fragShaderSource);
}

TestInstance* ShaderRenderCase::createInstance (Context& context) const
{
	return new ShaderRenderCaseInstance(context, m_name, m_isVertexCase, *m_evaluator);
}

// ShaderRenderCaseInstance.

ShaderRenderCaseInstance::ShaderRenderCaseInstance (Context& context, const string& name, bool isVertexCase, ShaderEvaluator& evaluator)
	: vkt::TestInstance(context)
	, m_name(name)
	, m_isVertexCase(isVertexCase)
	, m_evaluator(evaluator)
	, m_clearColor(DEFAULT_CLEAR_COLOR)
	, m_renderSize(100, 100)
	, m_colorFormat(vk::VK_FORMAT_R8G8B8A8_UNORM)
{
}

ShaderRenderCaseInstance::~ShaderRenderCaseInstance (void)
{
}

tcu::TestStatus ShaderRenderCaseInstance::iterate (void)
{
	// Create quad grid.
	IVec2	viewportSize	= getViewportSize();
	int		width			= viewportSize.x();
	int 	height			= viewportSize.y();

	QuadGrid quadGrid(m_isVertexCase ? GRID_SIZE : 4, width, height, Vec4(0.125f, 0.25f, 0.5f, 1.0f), m_userAttribTransforms/*, m_textures*/);

	// Render result.
	Surface resImage(width, height);
	render(resImage, quadGrid);

	// Compute reference.
	Surface refImage(width, height);
	if (m_isVertexCase)
		computeVertexReference(refImage, quadGrid);
	else
		computeFragmentReference(refImage, quadGrid);

	//m_context.getTestContext().getLog() << TestLog::Image("Result", "Result", refImage.getAccess());
/*tcu::ConstPixelBufferAccess(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8), m_renderSize.x(), m_renderSize.y(), 1, imagePtr)*/

	// Compare.
	bool compareOk = compareImages(resImage, refImage, 0.05f);

	if (compareOk)
		return tcu::TestStatus::pass("Result image matches reference");
	else
		return tcu::TestStatus::fail("Image mismatch");
}

void ShaderRenderCaseInstance::setupShaderData (void)
{
	// TODO!!!
}

void ShaderRenderCaseInstance::setup (void)
{
	// TODO!!
}

void ShaderRenderCaseInstance::setupUniforms (const Vec4& constCoords)
{
	// TODO!!
	DE_UNREF(constCoords);
}

tcu::IVec2 ShaderRenderCaseInstance::getViewportSize (void) const
{
	return tcu::IVec2(de::min(m_renderSize.x(), MAX_RENDER_WIDTH),
					  de::min(m_renderSize.y(), MAX_RENDER_HEIGHT));
}

void ShaderRenderCaseInstance::setupDefaultInputs (void)
{
	// TODO!!
	// SetupUniforms: map unifrom ids and set the values
}

void ShaderRenderCaseInstance::render (Surface& result, const QuadGrid& quadGrid)
{
	// TODO!! Vk rendering
}

void ShaderRenderCaseInstance::computeVertexReference (Surface& result, const QuadGrid& quadGrid)
{
	// TODO!!
	// Buffer info.
	int					width		= result.getWidth();
	int					height		= result.getHeight();
	int					gridSize	= quadGrid.getGridSize();
	int					stride		= gridSize + 1;
	//bool				hasAlpha	= m_context.getRenderTarget().getPixelFormat().alphaBits > 0;
	bool				hasAlpha	= true;
	ShaderEvalContext	evalCtx		(quadGrid);

	// Evaluate color for each vertex.
	vector<Vec4> colors((gridSize+1)*(gridSize+1));
	for (int y = 0; y < gridSize+1; y++)
	for (int x = 0; x < gridSize+1; x++)
	{
		float				sx			= (float)x / (float)gridSize;
		float				sy			= (float)y / (float)gridSize;
		int					vtxNdx		= ((y * (gridSize+1)) + x);

		evalCtx.reset(sx, sy);
		m_evaluator.evaluate(evalCtx);
		DE_ASSERT(!evalCtx.isDiscarded); // Discard is not available in vertex shader.
		Vec4 color = evalCtx.color;

		if (!hasAlpha)
			color.w() = 1.0f;

		colors[vtxNdx] = color;
	}

	// Render quads.
	for (int y = 0; y < gridSize; y++)
	for (int x = 0; x < gridSize; x++)
	{
		float x0 = (float)x       / (float)gridSize;
		float x1 = (float)(x + 1) / (float)gridSize;
		float y0 = (float)y       / (float)gridSize;
		float y1 = (float)(y + 1) / (float)gridSize;

		float sx0 = x0 * (float)width;
		float sx1 = x1 * (float)width;
		float sy0 = y0 * (float)height;
		float sy1 = y1 * (float)height;
		float oosx = 1.0f / (sx1 - sx0);
		float oosy = 1.0f / (sy1 - sy0);

		int ix0 = deCeilFloatToInt32(sx0 - 0.5f);
		int ix1 = deCeilFloatToInt32(sx1 - 0.5f);
		int iy0 = deCeilFloatToInt32(sy0 - 0.5f);
		int iy1 = deCeilFloatToInt32(sy1 - 0.5f);

		int		v00 = (y * stride) + x;
		int		v01 = (y * stride) + x + 1;
		int		v10 = ((y + 1) * stride) + x;
		int		v11 = ((y + 1) * stride) + x + 1;
		Vec4	c00 = colors[v00];
		Vec4	c01 = colors[v01];
		Vec4	c10 = colors[v10];
		Vec4	c11 = colors[v11];

		//printf("(%d,%d) -> (%f..%f, %f..%f) (%d..%d, %d..%d)\n", x, y, sx0, sx1, sy0, sy1, ix0, ix1, iy0, iy1);

		for (int iy = iy0; iy < iy1; iy++)
		for (int ix = ix0; ix < ix1; ix++)
		{
			DE_ASSERT(deInBounds32(ix, 0, width));
			DE_ASSERT(deInBounds32(iy, 0, height));

			float		sfx		= (float)ix + 0.5f;
			float		sfy		= (float)iy + 0.5f;
			float		fx1		= deFloatClamp((sfx - sx0) * oosx, 0.0f, 1.0f);
			float		fy1		= deFloatClamp((sfy - sy0) * oosy, 0.0f, 1.0f);

			// Triangle quad interpolation.
			bool		tri		= fx1 + fy1 <= 1.0f;
			float		tx		= tri ? fx1 : (1.0f-fx1);
			float		ty		= tri ? fy1 : (1.0f-fy1);
			const Vec4&	t0		= tri ? c00 : c11;
			const Vec4&	t1		= tri ? c01 : c10;
			const Vec4&	t2		= tri ? c10 : c01;
			Vec4		color	= t0 + (t1-t0)*tx + (t2-t0)*ty;

			result.setPixel(ix, iy, tcu::RGBA(color));
		}
	}
}

void ShaderRenderCaseInstance::computeFragmentReference (Surface& result, const QuadGrid& quadGrid)
{
    // Buffer info.
	int					width		= result.getWidth();
	int					height		= result.getHeight();
	//bool				hasAlpha	= m_renderCtx.getRenderTarget().getPixelFormat().alphaBits > 0;
	bool				hasAlpha	= true;
	ShaderEvalContext	evalCtx		(quadGrid);

	// Render.
	for (int y = 0; y < height; y++)
	for (int x = 0; x < width; x++)
	{
		float sx = ((float)x + 0.5f) / (float)width;
		float sy = ((float)y + 0.5f) / (float)height;

		evalCtx.reset(sx, sy);
		m_evaluator.evaluate(evalCtx);
		// Select either clear color or computed color based on discarded bit.
		Vec4 color = evalCtx.isDiscarded ? m_clearColor : evalCtx.color;

		if (!hasAlpha)
			color.w() = 1.0f;

		result.setPixel(x, y, tcu::RGBA(color));
	}
}

bool ShaderRenderCaseInstance::compareImages (const Surface& resImage, const Surface& refImage, float errorThreshold)
{
	return tcu::fuzzyCompare(m_context.getTestContext().getLog(), "ComparisonResult", "Image comparison result", refImage, resImage, errorThreshold, tcu::COMPARE_LOG_RESULT);
}


} // shaderrendercase
} // vkt
