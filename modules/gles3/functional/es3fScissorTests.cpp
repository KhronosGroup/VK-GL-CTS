/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 3.0 Module
 * -------------------------------------------------
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
 * \brief GLES3 Scissor tests
 *//*--------------------------------------------------------------------*/

#include "es3fScissorTests.hpp"

#include "glsScissorTests.hpp"

#include "sglrGLContext.hpp"
#include "sglrReferenceContext.hpp"
#include "sglrContextUtil.hpp"

#include "tcuVector.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuImageCompare.hpp"

#include "gluStrUtil.hpp"
#include "gluDrawUtil.hpp"

#include "glwEnums.hpp"
#include "deDefs.hpp"

namespace deqp
{
namespace gles3
{
namespace Functional
{
namespace
{

using tcu::ConstPixelBufferAccess;

class FramebufferCase : public tcu::TestCase
{
public:
									FramebufferCase			(glu::RenderContext& context, tcu::TestContext& testContext, const char* name, const char* description);
	virtual							~FramebufferCase		(void) {}

	virtual IterateResult			iterate					(void);

protected:
	// Must do its own 'readPixels', wrapper does not need to care about formats this way
	virtual ConstPixelBufferAccess	render					(sglr::Context& context, std::vector<deUint8>& pixelBuffer) = DE_NULL;

	glu::RenderContext&				m_renderContext;
};

FramebufferCase::FramebufferCase (glu::RenderContext& context, tcu::TestContext& testContext, const char* name, const char* description)
	: tcu::TestCase		(testContext, name, description)
	, m_renderContext	(context)
{
}

FramebufferCase::IterateResult FramebufferCase::iterate (void)
{
	const tcu::Vec4				clearColor				(0.0f, 0.0f, 0.0f, 1.0f);
	const tcu::RenderTarget&	renderTarget			= m_renderContext.getRenderTarget();
	const char*					failReason				= DE_NULL;

	const int					width					= 64;
	const int					height					= 64;

	tcu::TestLog&				log						= m_testCtx.getLog();
	glu::RenderContext&			renderCtx				= m_renderContext;

	tcu::ConstPixelBufferAccess glesAccess;
	tcu::ConstPixelBufferAccess refAccess;

	std::vector<deUint8>		glesFrame;
	std::vector<deUint8>		refFrame;
	deUint32					glesError;

	{
		// Render using GLES
		sglr::GLContext context(renderCtx, log, sglr::GLCONTEXT_LOG_CALLS, tcu::IVec4(0, 0, width, height));

		context.clearColor(clearColor.x(), clearColor.y(), clearColor.z(), clearColor.w());
		context.clear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);

		glesAccess = render(context, glesFrame); // Call actual render func
		glesError = context.getError();
	}

	// Render reference image
	{
		sglr::ReferenceContextBuffers	buffers	(tcu::PixelFormat(8,8,8,renderTarget.getPixelFormat().alphaBits?8:0), renderTarget.getDepthBits(), renderTarget.getStencilBits(), width, height);
		sglr::ReferenceContext			context	(sglr::ReferenceContextLimits(renderCtx), buffers.getColorbuffer(), buffers.getDepthbuffer(), buffers.getStencilbuffer());

		context.clearColor(clearColor.x(), clearColor.y(), clearColor.z(), clearColor.w());
		context.clear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);

		refAccess = render(context, refFrame);
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
		const tcu::Vec4		threshold	(0.02f, 0.02f, 0.02f, 0.02f);
		const bool			imagesOk	= tcu::floatThresholdCompare(log, "ComparisonResult", "Image comparison result", refAccess, glesAccess, threshold, tcu::COMPARE_LOG_RESULT);

		if (!imagesOk && !failReason)
			failReason = "Image comparison failed";

		// Store test result
		m_testCtx.setTestResult(imagesOk ? QP_TEST_RESULT_PASS	: QP_TEST_RESULT_FAIL,
								imagesOk ? "Pass"				: failReason);
	}

	return STOP;
}

class FramebufferClearCase : public FramebufferCase
{
public:
	enum ClearType
	{
		COLOR_FIXED = 0,
		COLOR_FLOAT,
		COLOR_INT,
		COLOR_UINT,
		DEPTH,
		STENCIL,
		DEPTH_STENCIL,

		CLEAR_TYPE_LAST
	};

									FramebufferClearCase	(glu::RenderContext& context, tcu::TestContext& testContext, ClearType clearType, const char* name, const char* description);
	virtual							~FramebufferClearCase	(void) {}

	tcu::ConstPixelBufferAccess		render					(sglr::Context& context, std::vector<deUint8>& pixelBuffer);

private:
	const ClearType					m_clearType;
};

FramebufferClearCase::FramebufferClearCase (glu::RenderContext& context, tcu::TestContext& testContext, ClearType clearType, const char* name, const char* description)
	: FramebufferCase	(context, testContext, name, description)
	, m_clearType		(clearType)
{
}

ConstPixelBufferAccess FramebufferClearCase::render (sglr::Context& context, std::vector<deUint8>& pixelBuffer)
{
	using gls::Functional::ScissorTestShader;

	ScissorTestShader	shader;
	const deUint32		shaderID			= context.createProgram(&shader);

	const int			width				= 64;
	const int			height				= 64;

	const tcu::Vec4		clearColor			(1.0f, 1.0f, 0.5f, 1.0f);
	const tcu::IVec4	clearInt			(127, -127, 0, 127);
	const tcu::UVec4	clearUint			(255, 255, 0, 255);

	const tcu::Vec4		baseColor			(0.0f, 0.0f, 0.0f, 1.0f);
	const tcu::IVec4	baseIntColor		(0, 0, 0, 0);
	const tcu::UVec4	baseUintColor		(0, 0, 0, 0);

	const int			clearStencil		= 123;
	const float			clearDepth			= 1.0f;

	deUint32			framebuf, colorbuf, dsbuf;

	deUint32			colorBufferFormat	= GL_RGBA8;
	deUint32			readFormat			= GL_RGBA;
	deUint32			readType			= GL_UNSIGNED_BYTE;
	tcu::TextureFormat	textureFormat		(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8);

	context.genFramebuffers(1, &framebuf);
	context.bindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuf);

	switch (m_clearType)
	{
		case COLOR_FLOAT:
			colorBufferFormat	= GL_RGBA16F;
			textureFormat		= tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::HALF_FLOAT);
			DE_ASSERT(!"Floating point clear not implemented");// \todo [2014-1-23 otto] pixel read format & type, nothing guaranteed, need extension...
			break;

		case COLOR_INT:
			colorBufferFormat	= GL_RGBA8I;
			readFormat			= GL_RGBA_INTEGER;
			readType			= GL_INT;
			textureFormat		= tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::SIGNED_INT32);
			pixelBuffer.resize(width*height*4*4);
			break;

		case COLOR_UINT:
			colorBufferFormat	= GL_RGBA8UI;
			readFormat			= GL_RGBA_INTEGER;
			readType			= GL_UNSIGNED_INT;
			textureFormat		= tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNSIGNED_INT32);
			pixelBuffer.resize(width*height*4*4);
			break;

		default:
			pixelBuffer.resize(width*height*1*4);
			break;
	}

	// Color
	context.genRenderbuffers(1, &colorbuf);
	context.bindRenderbuffer(GL_RENDERBUFFER, colorbuf);
	context.renderbufferStorage(GL_RENDERBUFFER, colorBufferFormat, width, height);
	context.framebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorbuf);

	// Depth/stencil
	context.genRenderbuffers(1, &dsbuf);
	context.bindRenderbuffer(GL_RENDERBUFFER, dsbuf);
	context.renderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
	context.framebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, dsbuf);

	context.clearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f-clearDepth, ~clearStencil);
	switch (m_clearType)
	{
		case COLOR_INT:		context.clearBufferiv(GL_COLOR, 0, baseIntColor.getPtr());		break;
		case COLOR_UINT:	context.clearBufferuiv(GL_COLOR, 0, baseUintColor.getPtr());	break;
		default:			context.clearBufferfv(GL_COLOR, 0, baseColor.getPtr());
	}

	context.enable(GL_SCISSOR_TEST);
	context.scissor(8, 8, 48, 48);

	switch (m_clearType)
	{
		case COLOR_FIXED:		context.clearBufferfv(GL_COLOR, 0, clearColor.getPtr());				break;
		case COLOR_FLOAT:		context.clearBufferfv(GL_COLOR, 0, clearColor.getPtr());				break;
		case COLOR_INT:			context.clearBufferiv(GL_COLOR, 0, clearInt.getPtr());					break;
		case COLOR_UINT:		context.clearBufferuiv(GL_COLOR, 0, clearUint.getPtr());				break;
		case DEPTH:				context.clearBufferfv(GL_DEPTH, 0, &clearDepth);						break;
		case STENCIL:			context.clearBufferiv(GL_STENCIL, 0, &clearStencil);					break;
		case DEPTH_STENCIL:		context.clearBufferfi(GL_DEPTH_STENCIL, 0, clearDepth, clearStencil);	break;

		default:				DE_ASSERT(false);														break;
	}

	context.disable(GL_SCISSOR_TEST);

	const bool useDepth		= (m_clearType==DEPTH   || m_clearType==DEPTH_STENCIL);
	const bool useStencil	= (m_clearType==STENCIL || m_clearType==DEPTH_STENCIL);

	if (useDepth)
		context.enable(GL_DEPTH_TEST);

	if (useStencil)
	{
		context.enable(GL_STENCIL_TEST);
		context.stencilFunc(GL_EQUAL, clearStencil, ~0u);
	}

	if (useDepth || useStencil)
	{
		shader.setColor(context, shaderID, clearColor);
		sglr::drawQuad(context, shaderID, tcu::Vec3(-1.0f, -1.0f, 0.2f), tcu::Vec3(1.0f, 1.0f, 0.2f));
	}

	context.bindFramebuffer(GL_READ_FRAMEBUFFER, framebuf);
	context.readPixels(0, 0, width, height, readFormat, readType, &pixelBuffer[0]);

	context.deleteFramebuffers(1, &framebuf);
	context.deleteRenderbuffers(1, &colorbuf);
	context.deleteRenderbuffers(1, &dsbuf);

	return tcu::PixelBufferAccess(textureFormat, width, height, 1, &pixelBuffer[0]);
}

class FramebufferBlitCase : public gls::Functional::ScissorCase
{
public:
							FramebufferBlitCase		(glu::RenderContext& context, tcu::TestContext& testContext, const tcu::Vec4& scissorArea, const char* name, const char* description);
	virtual					~FramebufferBlitCase	(void) {}

	virtual void			init					(void);

protected:
	virtual void			render					(sglr::Context& context, const tcu::IVec4& viewport);
};

FramebufferBlitCase::FramebufferBlitCase (glu::RenderContext& context, tcu::TestContext& testContext, const tcu::Vec4& scissorArea, const char* name, const char* description):
	ScissorCase(context, testContext, scissorArea, name, description)
{
}

void FramebufferBlitCase::init (void)
{
	if (m_renderContext.getRenderTarget().getNumSamples())
		throw tcu::NotSupportedError("Cannot blit to multisampled render buffer", "", __FILE__, __LINE__);
}

void FramebufferBlitCase::render (sglr::Context& context, const tcu::IVec4& viewport)
{
	deUint32			framebuf;
	deUint32			colorbuf;

	const int			fboWidth			= 64;
	const int			fboHeight			= 64;
	const tcu::Vec4		clearColor			(1.0f, 1.0f, 0.5f, 1.0f);
	const int			width				= viewport.z();
	const int			height				= viewport.w();
	const tcu::IVec4	scissorArea			(int(m_scissorArea.x()*width) + viewport.x(),
											 int(m_scissorArea.y()*height) + viewport.y(),
											 int(m_scissorArea.z()*width),
											 int(m_scissorArea.w()*height));
	const deInt32		defaultFramebuffer	= m_renderContext.getDefaultFramebuffer();

	context.genFramebuffers(1, &framebuf);
	context.bindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuf);

	context.genRenderbuffers(1, &colorbuf);
	context.bindRenderbuffer(GL_RENDERBUFFER, colorbuf);
	context.renderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, fboWidth, fboHeight);
	context.framebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorbuf);

	context.clearBufferfv(GL_COLOR, 0, clearColor.getPtr());

	context.enable(GL_SCISSOR_TEST);
	context.scissor(scissorArea.x(), scissorArea.y(), scissorArea.z(), scissorArea.w());

	// blit to default framebuffer
	context.bindFramebuffer(GL_READ_FRAMEBUFFER, framebuf);
	context.bindFramebuffer(GL_DRAW_FRAMEBUFFER, defaultFramebuffer);

	context.blitFramebuffer(0, 0, fboWidth, fboHeight, viewport.x(), viewport.y(), viewport.x() + width, viewport.y() + height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

	context.bindFramebuffer(GL_READ_FRAMEBUFFER, defaultFramebuffer);

	context.disable(GL_SCISSOR_TEST);

	context.deleteFramebuffers(1, &framebuf);
	context.deleteRenderbuffers(1, &colorbuf);
}

} // Anonymous

ScissorTests::ScissorTests (Context& context):
	TestCaseGroup	(context, "scissor", "Scissor Tests")
{
}

ScissorTests::~ScissorTests (void)
{
}

void ScissorTests::init (void)
{
	using tcu::Vec4;
	typedef gls::Functional::ScissorCase SC;

	glu::RenderContext&		rc = m_context.getRenderContext();
	tcu::TestContext&		tc = m_context.getTestContext();

	static const struct
	{
		const char*				name;
		const char*				description;
		const tcu::Vec4			scissor;
		const tcu::Vec4			render;
		const SC::PrimitiveType	type;
		const int				primitives;
	} cases[] =
	{
		{ "contained_quads",		"Triangles fully inside scissor area (single call)",		Vec4(0.1f, 0.1f, 0.8f, 0.8f), Vec4(0.2f, 0.2f, 0.6f, 0.6f), SC::TRIANGLE,	30 },
		{ "partial_quads",			"Triangles partially inside scissor area (single call)",	Vec4(0.3f, 0.3f, 0.4f, 0.4f), Vec4(0.2f, 0.2f, 0.6f, 0.6f), SC::TRIANGLE,	30 },
		{ "contained_tri",			"Triangle fully inside scissor area",						Vec4(0.1f, 0.1f, 0.8f, 0.8f), Vec4(0.2f, 0.2f, 0.6f, 0.6f), SC::TRIANGLE,	1  },
		{ "enclosing_tri",			"Triangle fully covering scissor area",						Vec4(0.4f, 0.4f, 0.2f, 0.2f), Vec4(0.2f, 0.2f, 0.6f, 0.6f), SC::TRIANGLE,	1  },
		{ "partial_tri",			"Triangle partially inside scissor area",					Vec4(0.4f, 0.4f, 0.6f, 0.6f), Vec4(0.0f, 0.0f, 1.0f, 1.0f), SC::TRIANGLE,	1  },
		{ "outside_render_tri",		"Triangle with scissor area outside render target",			Vec4(1.4f, 1.4f, 0.6f, 0.6f), Vec4(0.0f, 0.0f, 0.6f, 0.6f), SC::TRIANGLE,	1  },
		{ "partial_lines",			"Linse partially inside scissor area",						Vec4(0.4f, 0.4f, 0.6f, 0.6f), Vec4(0.0f, 0.0f, 1.0f, 1.0f), SC::LINE,		30 },
		{ "contained_line",			"Line fully inside scissor area",							Vec4(0.1f, 0.1f, 0.8f, 0.8f), Vec4(0.2f, 0.2f, 0.6f, 0.6f), SC::LINE,		1  },
		{ "partial_line",			"Line partially inside scissor area",						Vec4(0.4f, 0.4f, 0.6f, 0.6f), Vec4(0.0f, 0.0f, 1.0f, 1.0f), SC::LINE,		1  },
		{ "outside_render_line",	"Line with scissor area outside render target",				Vec4(1.4f, 1.4f, 0.6f, 0.6f), Vec4(0.0f, 0.0f, 0.6f, 0.6f), SC::LINE,		1  },
		{ "contained_point",		"Point fully inside scissor area",							Vec4(0.1f, 0.1f, 0.8f, 0.8f), Vec4(0.5f, 0.5f, 0.0f, 0.0f), SC::POINT,		1  },
		{ "partial_points",			"Points partially inside scissor area",						Vec4(0.4f, 0.4f, 0.6f, 0.6f), Vec4(0.0f, 0.0f, 1.0f, 1.0f), SC::POINT,		30 },
		{ "outside_point",			"Point fully outside scissor area",							Vec4(0.4f, 0.4f, 0.6f, 0.6f), Vec4(0.0f, 0.0f, 0.0f, 0.0f), SC::POINT,		1  },
		{ "outside_render_point",	"Point with scissor area outside render target",			Vec4(1.4f, 1.4f, 0.6f, 0.6f), Vec4(0.5f, 0.5f, 0.0f, 0.0f),	SC::POINT,		1  }
	};

	for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(cases); caseNdx++)
	{
		addChild(SC::createPrimitiveTest(rc,
										 tc,
										 cases[caseNdx].scissor,
										 cases[caseNdx].render,
										 cases[caseNdx].type,
										 cases[caseNdx].primitives,
										 cases[caseNdx].name,
										 cases[caseNdx].description));
	}

	addChild(SC::createClearTest(rc, tc, Vec4(0.1f, 0.1f, 0.8f, 0.8f), GL_DEPTH_BUFFER_BIT,		"clear_depth",		"Depth buffer clear"));
	addChild(SC::createClearTest(rc, tc, Vec4(0.1f, 0.1f, 0.8f, 0.8f), GL_STENCIL_BUFFER_BIT,	"clear_stencil",	"Stencil buffer clear"));
	addChild(SC::createClearTest(rc, tc, Vec4(0.1f, 0.1f, 0.8f, 0.8f), GL_COLOR_BUFFER_BIT,		"clear_color",		"Color buffer clear"));

	addChild(new FramebufferClearCase(rc, tc, FramebufferClearCase::COLOR_FIXED,	"clear_fixed_buffer",			"Fixed point color clear"));
	addChild(new FramebufferClearCase(rc, tc, FramebufferClearCase::COLOR_INT,		"clear_int_buffer",				"Integer color clear"));
	addChild(new FramebufferClearCase(rc, tc, FramebufferClearCase::COLOR_UINT,		"clear_uint_buffer",			"Unsigned integer buffer clear"));
	addChild(new FramebufferClearCase(rc, tc, FramebufferClearCase::DEPTH,			"clear_depth_buffer",			"Depth buffer clear"));
	addChild(new FramebufferClearCase(rc, tc, FramebufferClearCase::STENCIL,		"clear_stencil_buffer",			"Stencil buffer clear"));
	addChild(new FramebufferClearCase(rc, tc, FramebufferClearCase::DEPTH_STENCIL,	"clear_depth_stencil_buffer",	"Fixed point color buffer clear"));

	addChild(new FramebufferBlitCase(rc, tc, Vec4(0.1f, 0.1f, 0.8f, 0.8f), "framebuffer_blit_center",	"Blit to default framebuffer, scissor away edges"));
	addChild(new FramebufferBlitCase(rc, tc, Vec4(0.6f, 0.6f, 0.5f, 0.5f), "framebuffer_blit_corner",	"Blit to default framebuffer, scissor all but a corner"));
	addChild(new FramebufferBlitCase(rc, tc, Vec4(1.6f, 0.6f, 0.5f, 0.5f), "framebuffer_blit_none",		"Blit to default framebuffer, scissor area outside screen"));
}

} // Functional
} // gles3
} // deqp
