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
 * \brief GLES Scissor tests
 *//*--------------------------------------------------------------------*/

#include "glsScissorTests.hpp"
#include "glsTextureTestUtil.hpp"

#include "deMath.h"

#include "tcuTestCase.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuImageCompare.hpp"
#include "tcuVector.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuTexture.hpp"

#include "sglrReferenceContext.hpp"
#include "sglrContextUtil.hpp"

#include "gluStrUtil.hpp"
#include "gluDrawUtil.hpp"

#include "glwEnums.hpp"

#include "deRandom.hpp"

namespace deqp
{
namespace gls
{
namespace Functional
{

using std::vector;

ScissorTestShader::ScissorTestShader (void)
	: sglr::ShaderProgram(sglr::pdec::ShaderProgramDeclaration()
							<< sglr::pdec::VertexAttribute("a_position", rr::GENERICVECTYPE_FLOAT)
							<< sglr::pdec::FragmentOutput(rr::GENERICVECTYPE_FLOAT)
							<< sglr::pdec::Uniform("u_color", glu::TYPE_FLOAT_VEC4)
							<< sglr::pdec::VertexSource("attribute highp vec4 a_position;\n"
														"void main (void)\n"
														"{\n"
														"	gl_Position = a_position;\n"
														"}\n")
							<< sglr::pdec::FragmentSource("uniform mediump vec4 u_color;\n"
														  "void main (void)\n"
														  "{\n"
														  "	gl_FragColor = u_color;\n"
														  "}\n"))
	, u_color			(getUniformByName("u_color"))
{
}

void ScissorTestShader::setColor (sglr::Context& ctx, deUint32 programID, const tcu::Vec4& color)
{
	ctx.useProgram(programID);
	ctx.uniform4fv(ctx.getUniformLocation(programID, "u_color"), 1, color.getPtr());
}

void ScissorTestShader::shadeVertices (const rr::VertexAttrib* inputs, rr::VertexPacket* const* packets, const int numPackets) const
{
	for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
		packets[packetNdx]->position = rr::readVertexAttribFloat(inputs[0], packets[packetNdx]->instanceNdx, packets[packetNdx]->vertexNdx);
}

void ScissorTestShader::shadeFragments (rr::FragmentPacket* packets, const int numPackets, const rr::FragmentShadingContext& context) const
{
	const tcu::Vec4 color(u_color.value.f4);

	DE_UNREF(packets);

	for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
		for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
			rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, color);
}

namespace
{

void drawPrimitives (sglr::Context& ctx, deUint32 program, const deUint32 type, const float *vertPos, int numIndices, const deUint16 *indices)
{
	const deInt32	posLoc	= ctx.getAttribLocation(program, "a_position");

	TCU_CHECK(posLoc >= 0);

	ctx.useProgram(program);
	ctx.enableVertexAttribArray(posLoc);
	ctx.vertexAttribPointer(posLoc, 4, GL_FLOAT, GL_FALSE, 0, vertPos);

	ctx.drawElements(type, numIndices, GL_UNSIGNED_SHORT, indices);
	ctx.disableVertexAttribArray(posLoc);
}

// Mark pixel pairs with a large color difference starting with the given points and moving by advance count times
vector<deUint8> findBorderPairs (const tcu::ConstPixelBufferAccess& image, const tcu::IVec2& start0, const tcu::IVec2& start1, const tcu::IVec2& advance, int count)
{
	using tcu::Vec4;
	using tcu::IVec2;

	const Vec4		threshold	(0.1f, 0.1f, 0.1f, 0.1f);
	IVec2			p0			= start0;
	IVec2			p1			= start1;
	vector<deUint8>	res;

	res.resize(count);

	for (int ndx = 0; ndx < count; ndx++)
	{
		const Vec4	diff		= abs(image.getPixel(p0.x(), p0.y()) - image.getPixel(p1.x(), p1.y()));

		res[ndx] = !boolAll(lessThanEqual(diff, threshold));
		p0  += advance;
		p1  += advance;
	}
	return res;
}

// make all elements within range of a 'true' element 'true' as well
vector<deUint8> fuzz (const vector<deUint8>& ref, int range)
{
	vector<deUint8> res;

	res.resize(ref.size());

	for (int ndx = 0; ndx < int(ref.size()); ndx++)
	{
		if (ref[ndx])
		{
			const int begin = de::max(0, ndx-range);
			const int end	= de::min(ndx+range, int(ref.size())-1);

			for (int i = begin; i <= end; i++)
				res[i] = 1;
		}
	}

	return res;
}

bool bordersEquivalent (tcu::TestLog& log, const tcu::ConstPixelBufferAccess& reference, const tcu::ConstPixelBufferAccess& result, const tcu::IVec2& start0, const tcu::IVec2& start1, const tcu::IVec2& advance, int count)
{
	const vector<deUint8>	refBorders		= fuzz(findBorderPairs(reference,	start0, start1, advance, count), 1);
	const vector<deUint8>	resBorders		= findBorderPairs(result,			start0, start1, advance, count);

	// Helps deal with primitives that are within 1px of the scissor edge and thus may (not) create an edge for findBorderPairs. This number is largely resolution-independent since the typical  triggers are points rather than edges.
	const int				errorThreshold  = 2;
	const int				floodThreshold	= 8;
	int						messageCount	= 0;

	for (int ndx = 0; ndx < int(resBorders.size()); ndx++)
	{
		if (resBorders[ndx] && !refBorders[ndx])
		{
			messageCount++;

			if (messageCount <= floodThreshold)
			{
				const tcu::IVec2 coord = start0 + advance*ndx;
				log << tcu::TestLog::Message << "No matching border near " << coord << tcu::TestLog::EndMessage;
			}
		}
	}

	if (messageCount > floodThreshold)
		log << tcu::TestLog::Message << "Omitted " << messageCount - floodThreshold << " more errors" << tcu::TestLog::EndMessage;

	return messageCount <= errorThreshold;
}

// Try to find a clear border between [area.xy, area.zw) and the rest of the image, check that the reference and result images have a roughly matching number of border pixel pairs
bool compareBorders (tcu::TestLog& log, const tcu::ConstPixelBufferAccess& reference, const tcu::ConstPixelBufferAccess& result, const tcu::IVec4& area)
{
	using tcu::IVec2;
	using tcu::IVec4;

	const IVec4			testableArea	(0, 0, reference.getWidth(), reference.getHeight());
	const tcu::BVec4	testableEdges	(area.x()>testableArea.x() && area.x()<testableArea.z(),
										 area.y()>testableArea.y() && area.y()<testableArea.w(),
										 area.z()<testableArea.z() && area.z()>testableArea.x(),
										 area.w()<testableArea.w() && area.w()>testableArea.y());
	const IVec4			testArea		(std::max(area.x(), testableArea.x()), std::max(area.y(), testableArea.y()), std::min(area.z(), testableArea.z()), std::min(area.w(), testableArea.w()) );

	if (testArea.x() > testArea.z() || testArea.y() > testArea.w()) // invalid area
		return true;

	if (testableEdges.x() &&
		!bordersEquivalent(log,
						   reference,
						   result,
						   IVec2(testArea.x(), testArea.y()),
						   IVec2(testArea.x()-1, testArea.y()),
						   IVec2(0, 1),
						   testArea.w()-testArea.y()))
		return false;

	if (testableEdges.z() &&
		!bordersEquivalent(log,
						   reference,
						   result,
						   IVec2(testArea.z(), testArea.y()),
						   IVec2(testArea.z()-1, testArea.y()),
						   IVec2(0, 1),
						   testArea.w()-testArea.y()))
		return false;

	if (testableEdges.y() &&
		!bordersEquivalent(log,
						   reference,
						   result,
						   IVec2(testArea.x(), testArea.y()),
						   IVec2(testArea.x(), testArea.y()-1),
						   IVec2(1, 0),
						   testArea.z()-testArea.x()))
		return false;

	if (testableEdges.w() &&
		!bordersEquivalent(log,
						   reference,
						   result,
						   IVec2(testArea.x(), testArea.w()),
						   IVec2(testArea.x(), testArea.w()-1),
						   IVec2(1, 0),
						   testArea.z()-testArea.x()))
		return false;

	return true;
}

} // anonymous

ScissorCase::ScissorCase (glu::RenderContext& context, tcu::TestContext& testContext, const tcu::Vec4& scissorArea, const char* name, const char* description)
	: TestCase			(testContext, name, description)
	, m_renderContext	(context)
	, m_scissorArea		(scissorArea)
{
}

ScissorCase::IterateResult ScissorCase::iterate (void)
{
	using TextureTestUtil::RandomViewport;

	const tcu::Vec4				clearColor				(0.0f, 0.0f, 0.0f, 1.0f);
	glu::RenderContext&			renderCtx				= m_renderContext;
	const tcu::RenderTarget&	renderTarget			= renderCtx.getRenderTarget();
	tcu::TestLog&				log						= m_testCtx.getLog();

	const RandomViewport		viewport				(renderTarget, 256, 256, deStringHash(getName()));

	tcu::Surface				glesFrame				(viewport.width, viewport.height);
	tcu::Surface				refFrame				(viewport.width, viewport.height);
	deUint32					glesError;

	// Render using GLES
	{
		sglr::GLContext context(renderCtx, log, sglr::GLCONTEXT_LOG_CALLS, tcu::IVec4(0, 0, renderTarget.getWidth(), renderTarget.getHeight()));

		context.viewport(viewport.x, viewport.y, viewport.width, viewport.height);

		context.clearColor(clearColor.x(), clearColor.y(), clearColor.z(), clearColor.w());
		context.clear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);

		render(context, tcu::IVec4(viewport.x, viewport.y, viewport.width, viewport.height)); // Call actual render func
		context.readPixels(glesFrame, viewport.x, viewport.y, viewport.width, viewport.height);
		glesError = context.getError();
	}

	// Render reference image
	{
		sglr::ReferenceContextBuffers	buffers	(tcu::PixelFormat(8,8,8,renderTarget.getPixelFormat().alphaBits?8:0),
												 renderTarget.getDepthBits(),
												 renderTarget.getStencilBits(),
												 renderTarget.getWidth(),
												 renderTarget.getHeight());
		sglr::ReferenceContext			context	(sglr::ReferenceContextLimits(renderCtx), buffers.getColorbuffer(), buffers.getDepthbuffer(), buffers.getStencilbuffer());

		context.viewport(viewport.x, viewport.y, viewport.width, viewport.height);

		context.clearColor(clearColor.x(), clearColor.y(), clearColor.z(), clearColor.w());
		context.clear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);

		render(context, tcu::IVec4(viewport.x, viewport.y, viewport.width, viewport.height));
		context.readPixels(refFrame, viewport.x, viewport.y, viewport.width, viewport.height);
		DE_ASSERT(context.getError() == GL_NO_ERROR);
	}

	if (glesError != GL_NO_ERROR)
	{
		log << tcu::TestLog::Message << "Unexpected error: got " << glu::getErrorStr(glesError) << tcu::TestLog::EndMessage;
		m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got unexpected error");
	}
	else
	{
		// Compare images
		const float			threshold	= 0.02f;
		const tcu::IVec4	scissorArea	(int(m_scissorArea.x()*viewport.width ),
										 int(m_scissorArea.y()*viewport.height),
										 int(m_scissorArea.x()*viewport.width ) + int(m_scissorArea.z()*viewport.width ),
										 int(m_scissorArea.y()*viewport.height) + int(m_scissorArea.w()*viewport.height));
		const bool			bordersOk	= compareBorders(log, refFrame.getAccess(), glesFrame.getAccess(), scissorArea);
		const bool			imagesOk	= tcu::fuzzyCompare(log, "ComparisonResult", "Image comparison result", refFrame, glesFrame, threshold, tcu::COMPARE_LOG_RESULT);

		if (!imagesOk)
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Image comparison failed");
		else if (!bordersOk)
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Scissor area border mismatch");
		else
			m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
	}

	return STOP;
}

// Tests scissoring with multiple primitive types
class ScissorPrimitiveCase : public ScissorCase
{
public:
								ScissorPrimitiveCase	(glu::RenderContext&	context,
														 tcu::TestContext&		testContext,
														 const tcu::Vec4&		scissorArea,
														 const tcu::Vec4&		renderArea,
														 PrimitiveType			type,
														 int					primitiveCount,
														 const char*			name,
														 const char*			description);
	virtual						~ScissorPrimitiveCase	(void){}

protected:
	virtual void				render					(sglr::Context& context, const tcu::IVec4& viewport);

private:
	const tcu::Vec4				m_renderArea;
	const PrimitiveType			m_primitiveType;
	const int					m_primitiveCount;
};

ScissorPrimitiveCase::ScissorPrimitiveCase	(glu::RenderContext&	context,
											 tcu::TestContext&		testContext,
											 const tcu::Vec4&		scissorArea,
											 const tcu::Vec4&		renderArea,
											 PrimitiveType			type,
											 int					primitiveCount,
											 const char*			name,
											 const char*			description)
	: ScissorCase		(context, testContext, scissorArea, name, description)
	, m_renderArea		(renderArea)
	, m_primitiveType	(type)
	, m_primitiveCount	(primitiveCount)
{
}

void ScissorPrimitiveCase::render (sglr::Context& context, const tcu::IVec4& viewport)
{
	const tcu::Vec4				red				(0.6f, 0.1f, 0.1f, 1.0);

	ScissorTestShader			shader;
	const int					width			= viewport.w();
	const int					height			= viewport.z();
	const deUint32				shaderID		= context.createProgram(&shader);
	const tcu::Vec4				primitiveArea	(m_renderArea.x()*2.0f-1.0f,
												 m_renderArea.x()*2.0f-1.0f,
												 m_renderArea.z()*2.0f,
												 m_renderArea.w()*2.0f);
	const tcu::IVec4			scissorArea		(int(m_scissorArea.x()*width) + viewport.x(),
												 int(m_scissorArea.y()*height) + viewport.y(),
												 int(m_scissorArea.z()*width),
												 int(m_scissorArea.w()*height));

	static const float quadPositions[] =
	{
		 0.0f,  1.0f,
		 0.0f,  0.0f,
		 1.0f,  1.0f,
		 1.0f,  0.0f
	};
	static const float triPositions[] =
	{
		 0.0f,  0.0f,
		 1.0f,  0.0f,
		 0.5f,  1.0f,
	};
	static const float linePositions[] =
	{
		 0.0f,  0.0f,
		 1.0f,  1.0f
	};
	static const float pointPosition[] =
	{
		 0.5f,  0.5f
	};

	const float*				positionSet[]	= { pointPosition, linePositions, triPositions, quadPositions };
	const int					vertexCountSet[]= { 1, 2, 3, 4 };
	const int					indexCountSet[]	= { 1, 2, 3, 6 };

	const deUint16				baseIndices[]	= { 0, 1, 2, 2, 1, 3 };
	const float*				basePositions	= positionSet[m_primitiveType];
	const int					vertexCount		= vertexCountSet[m_primitiveType];
	const int					indexCount		= indexCountSet[m_primitiveType];

	const float					scale			= 1.44f/deFloatSqrt(float(m_primitiveCount)*2.0f);
	std::vector<float>			positions		(4*vertexCount*m_primitiveCount);
	std::vector<deUint16>		indices			(indexCount*m_primitiveCount);
	de::Random					rng				(1234);

	for (int primNdx = 0; primNdx < m_primitiveCount; primNdx++)
	{
		const float dx = m_primitiveCount>1 ? rng.getFloat() : 0.0f;
		const float dy = m_primitiveCount>1 ? rng.getFloat() : 0.0f;

		for (int vertNdx = 0; vertNdx<vertexCount; vertNdx++)
		{
			const int ndx = primNdx*4*vertexCount + vertNdx*4;
			positions[ndx+0] = (basePositions[vertNdx*2 + 0]*scale + dx)*primitiveArea.z() + primitiveArea.x();
			positions[ndx+1] = (basePositions[vertNdx*2 + 1]*scale + dy)*primitiveArea.w() + primitiveArea.y();
			positions[ndx+2] = 0.2f;
			positions[ndx+3] = 1.0f;
		}

		for (int ndx = 0; ndx < indexCount; ndx++)
			indices[primNdx*indexCount + ndx] = baseIndices[ndx] + primNdx*vertexCount;
	}

	// Enable scissor test.
	context.enable(GL_SCISSOR_TEST);
	context.scissor(scissorArea.x(), scissorArea.y(), scissorArea.z(), scissorArea.w());

	shader.setColor(context, shaderID, red);

	switch (m_primitiveType)
	{
		case QUAD:			// Fall-through, no real quads...
		case TRIANGLE:		drawPrimitives(context, shaderID, GL_TRIANGLES, &positions[0], indexCount*m_primitiveCount, &indices[0]);		break;
		case LINE:			drawPrimitives(context, shaderID, GL_LINES,		&positions[0], indexCount*m_primitiveCount, &indices[0]);		break;
		case POINT:			drawPrimitives(context, shaderID, GL_POINTS,	&positions[0], indexCount*m_primitiveCount, &indices[0]);		break;
		default:			DE_ASSERT(false);																								break;
	}

	context.disable(GL_SCISSOR_TEST);
}

class ScissorClearCase : public ScissorCase
{
public:
								ScissorClearCase		(glu::RenderContext&	context,
														 tcu::TestContext&		testContext,
														 const tcu::Vec4&		scissorArea,
														 deUint32				clearMode,
														 const char*			name,
														 const char*			description);
	virtual						~ScissorClearCase		(void) {}

	virtual void				init					(void);

protected:
	virtual void				render					(sglr::Context& context, const tcu::IVec4& viewport);

private:
	const deUint32				m_clearMode; //!< Combination of the flags accepted by glClear
};

ScissorClearCase::ScissorClearCase	(glu::RenderContext&	context,
									 tcu::TestContext&		testContext,
									 const tcu::Vec4&		scissorArea,
									 deUint32				clearMode,
									 const char*			name,
									 const char*			description)
	: ScissorCase	(context, testContext, scissorArea, name, description)
	, m_clearMode	(clearMode)
{
}

void ScissorClearCase::init (void)
{
	if ((m_clearMode & GL_DEPTH_BUFFER_BIT) && m_renderContext.getRenderTarget().getDepthBits()==0)
		throw tcu::NotSupportedError("Cannot clear depth; no depth buffer present", "", __FILE__, __LINE__);
	else if ((m_clearMode & GL_STENCIL_BUFFER_BIT) && m_renderContext.getRenderTarget().getStencilBits()==0)
		throw tcu::NotSupportedError("Cannot clear stencil; no stencil buffer present", "", __FILE__, __LINE__);
}

void ScissorClearCase::render (sglr::Context& context, const tcu::IVec4& viewport)
{
	ScissorTestShader			shader;
	const deUint32				shaderID		= context.createProgram(&shader);
	const int					width			= viewport.z();
	const int					height			= viewport.w();
	const tcu::Vec4				green			(0.1f, 0.6f, 0.1f, 1.0);
	const tcu::IVec4			scissorArea		(int(m_scissorArea.x()*width) + viewport.x(),
												 int(m_scissorArea.y()*height) + viewport.y(),
												 int(m_scissorArea.z()*width),
												 int(m_scissorArea.w()*height));

	context.clearColor(0.125f, 0.25f, 0.5f, 1.0f);
	context.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	context.clearColor(0.6f, 0.1f, 0.1f, 1.0);

	context.enable(GL_SCISSOR_TEST);
	context.scissor(scissorArea.x(), scissorArea.y(), scissorArea.z(), scissorArea.w());

	context.clearDepthf(0.0f);

	if (m_clearMode & GL_DEPTH_BUFFER_BIT)
	{
		context.enable(GL_DEPTH_TEST);
		context.depthFunc(GL_GREATER);
	}

	if (m_clearMode & GL_STENCIL_BUFFER_BIT)
	{
		context.clearStencil(123);
		context.enable(GL_STENCIL_TEST);
		context.stencilFunc(GL_EQUAL, 123, ~0u);
	}

	if (m_clearMode & GL_COLOR_BUFFER_BIT)
		context.clearColor(0.1f, 0.6f, 0.1f, 1.0);

	context.clear(m_clearMode);
	context.disable(GL_SCISSOR_TEST);

	shader.setColor(context, shaderID, green);

	if (!(m_clearMode & GL_COLOR_BUFFER_BIT))
		sglr::drawQuad(context, shaderID, tcu::Vec3(-1.0f, -1.0f, 0.5f), tcu::Vec3(1.0f, 1.0f, 0.5f));

	context.disable(GL_DEPTH_TEST);
	context.disable(GL_STENCIL_TEST);
}

tcu::TestNode* ScissorCase::createPrimitiveTest (glu::RenderContext&	context,
												 tcu::TestContext&		testContext,
												 const tcu::Vec4&		scissorArea,
												 const tcu::Vec4&		renderArea,
												 PrimitiveType			type,
												 int					primitiveCount,
												 const char*			name,
												 const char*			description)
{
	return new Functional::ScissorPrimitiveCase(context, testContext, scissorArea, renderArea, type, primitiveCount, name, description);
}

tcu::TestNode* ScissorCase::createClearTest (glu::RenderContext& context, tcu::TestContext& testContext, const tcu::Vec4& scissorArea, deUint32 clearMode, const char* name, const char* description)
{
	return new Functional::ScissorClearCase(context, testContext, scissorArea, clearMode, name, description);
}

} // Functional
} // gls
} // deqp
