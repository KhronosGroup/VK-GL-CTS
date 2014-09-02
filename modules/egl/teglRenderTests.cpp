/*-------------------------------------------------------------------------
 * drawElements Quality Program EGL Module
 * ---------------------------------------
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
 * \brief Rendering tests for different config and api combinations.
 * \todo [2013-03-19 pyry] GLES1 and VG support.
 *//*--------------------------------------------------------------------*/

#include "teglRenderTests.hpp"
#include "teglRenderCase.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuTestLog.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuSurface.hpp"

#include "deRandom.hpp"
#include "deSharedPtr.hpp"
#include "deSemaphore.hpp"
#include "deThread.hpp"
#include "deString.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <set>

#include <EGL/eglext.h>

#if !defined(EGL_OPENGL_ES3_BIT_KHR)
#	define EGL_OPENGL_ES3_BIT_KHR	0x0040
#endif
#if !defined(EGL_CONTEXT_MAJOR_VERSION_KHR)
#	define EGL_CONTEXT_MAJOR_VERSION_KHR EGL_CONTEXT_CLIENT_VERSION
#endif

#if defined(DEQP_SUPPORT_GLES2)
#	include <GLES2/gl2.h>
#elif defined(DEQP_SUPPORT_GLES3)
#	include <GLES3/gl3.h>
#endif

#include "rrRenderer.hpp"
#include "rrFragmentOperations.hpp"

#if defined(DEQP_SUPPORT_GLES2) || defined(DEQP_SUPPORT_GLES3)
#      include "gluDefs.hpp"
#else
       // \todo [pyry] Move renderer to common utils
       // \note [jarkko] gluDefs is required for GLU_CHECK_MSG
#      error "Reference renderer requires GLES2 or GLES3 support"
#endif

namespace deqp
{
namespace egl
{

using std::string;
using std::vector;
using std::set;

using tcu::Vec4;

using tcu::TestLog;

static const tcu::Vec4	CLEAR_COLOR		= tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
static const float		CLEAR_DEPTH		= 1.0f;
static const int		CLEAR_STENCIL	= 0;

namespace
{

enum PrimitiveType
{
	PRIMITIVETYPE_TRIANGLE = 0,	//!< Triangles, requires 3 coordinates per primitive
//	PRIMITIVETYPE_POINT,		//!< Points, requires 1 coordinate per primitive (w is used as size)
//	PRIMITIVETYPE_LINE,			//!< Lines, requires 2 coordinates per primitive

	PRIMITIVETYPE_LAST
};

enum BlendMode
{
	BLENDMODE_NONE = 0,			//!< No blending
	BLENDMODE_ADDITIVE,			//!< Blending with ONE, ONE
	BLENDMODE_SRC_OVER,			//!< Blending with SRC_ALPHA, ONE_MINUS_SRC_ALPHA

	BLENDMODE_LAST
};

enum DepthMode
{
	DEPTHMODE_NONE = 0,			//!< No depth test or depth writes
	DEPTHMODE_LESS,				//!< Depth test with less & depth write

	DEPTHMODE_LAST
};

enum StencilMode
{
	STENCILMODE_NONE = 0,		//!< No stencil test or write
	STENCILMODE_LEQUAL_INC,		//!< Stencil test with LEQUAL, increment on pass

	STENCILMODE_LAST
};

struct DrawPrimitiveOp
{
	PrimitiveType	type;
	int				count;
	vector<Vec4>	positions;
	vector<Vec4>	colors;
	BlendMode		blend;
	DepthMode		depth;
	StencilMode		stencil;
	int				stencilRef;
};

void randomizeDrawOp (de::Random& rnd, DrawPrimitiveOp& drawOp)
{
	const int	minStencilRef	= 0;
	const int	maxStencilRef	= 8;
	const int	minPrimitives	= 2;
	const int	maxPrimitives	= 4;

	const float	maxTriOffset	= 1.0f;
	const float	minDepth		= -1.0f; // \todo [pyry] Reference doesn't support Z clipping yet
	const float	maxDepth		= 1.0f;

	const float	minRGB			= 0.2f;
	const float	maxRGB			= 0.9f;
	const float	minAlpha		= 0.3f;
	const float	maxAlpha		= 1.0f;

	drawOp.type			= (PrimitiveType)rnd.getInt(0, PRIMITIVETYPE_LAST-1);
	drawOp.count		= rnd.getInt(minPrimitives, maxPrimitives);
	drawOp.blend		= (BlendMode)rnd.getInt(0, BLENDMODE_LAST-1);
	drawOp.depth		= (DepthMode)rnd.getInt(0, DEPTHMODE_LAST-1);
	drawOp.stencil		= (StencilMode)rnd.getInt(0, STENCILMODE_LAST-1);
	drawOp.stencilRef	= rnd.getInt(minStencilRef, maxStencilRef);

	if (drawOp.type == PRIMITIVETYPE_TRIANGLE)
	{
		drawOp.positions.resize(drawOp.count*3);
		drawOp.colors.resize(drawOp.count*3);

		for (int triNdx = 0; triNdx < drawOp.count; triNdx++)
		{
			const float		cx		= rnd.getFloat(-1.0f, 1.0f);
			const float		cy		= rnd.getFloat(-1.0f, 1.0f);

			for (int coordNdx = 0; coordNdx < 3; coordNdx++)
			{
				tcu::Vec4&	position	= drawOp.positions[triNdx*3 + coordNdx];
				tcu::Vec4&	color		= drawOp.colors[triNdx*3 + coordNdx];

				position.x()	= cx + rnd.getFloat(-maxTriOffset, maxTriOffset);
				position.y()	= cy + rnd.getFloat(-maxTriOffset, maxTriOffset);
				position.z()	= rnd.getFloat(minDepth, maxDepth);
				position.w()	= 1.0f;

				color.x()		= rnd.getFloat(minRGB, maxRGB);
				color.y()		= rnd.getFloat(minRGB, maxRGB);
				color.z()		= rnd.getFloat(minRGB, maxRGB);
				color.w()		= rnd.getFloat(minAlpha, maxAlpha);
			}
		}
	}
	else
		DE_ASSERT(false);
}

// Reference rendering code

class ReferenceShader : public rr::VertexShader, public rr::FragmentShader
{
public:
	enum
	{
		VaryingLoc_Color = 0
	};

	ReferenceShader ()
		: rr::VertexShader	(2, 1)		// color and pos in => color out
		, rr::FragmentShader(1, 1)		// color in => color out
	{
		this->rr::VertexShader::m_inputs[0].type		= rr::GENERICVECTYPE_FLOAT;
		this->rr::VertexShader::m_inputs[1].type		= rr::GENERICVECTYPE_FLOAT;

		this->rr::VertexShader::m_outputs[0].type		= rr::GENERICVECTYPE_FLOAT;
		this->rr::VertexShader::m_outputs[0].flatshade	= false;

		this->rr::FragmentShader::m_inputs[0].type		= rr::GENERICVECTYPE_FLOAT;
		this->rr::FragmentShader::m_inputs[0].flatshade	= false;

		this->rr::FragmentShader::m_outputs[0].type		= rr::GENERICVECTYPE_FLOAT;
	}

	void shadeVertices (const rr::VertexAttrib* inputs, rr::VertexPacket* const* packets, const int numPackets) const
	{
		for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
		{
			const int positionAttrLoc = 0;
			const int colorAttrLoc = 1;

			rr::VertexPacket& packet = *packets[packetNdx];

			// Transform to position
			packet.position = rr::readVertexAttribFloat(inputs[positionAttrLoc], packet.instanceNdx, packet.vertexNdx);

			// Pass color to FS
			packet.outputs[VaryingLoc_Color] = rr::readVertexAttribFloat(inputs[colorAttrLoc], packet.instanceNdx, packet.vertexNdx);
		}
	}

	void shadeFragments (rr::FragmentPacket* packets, const int numPackets, const rr::FragmentShadingContext& context) const
	{
		for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
		{
			rr::FragmentPacket& packet = packets[packetNdx];

			for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
				rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, rr::readVarying<float>(packet, context, VaryingLoc_Color, fragNdx));
		}
	}
};

void toReferenceRenderState (rr::RenderState& state, const DrawPrimitiveOp& drawOp)
{
	state.cullMode	= rr::CULLMODE_NONE;

	if (drawOp.blend != BLENDMODE_NONE)
	{
		state.fragOps.blendMode = rr::BLENDMODE_STANDARD;

		switch (drawOp.blend)
		{
			case BLENDMODE_ADDITIVE:
				state.fragOps.blendRGBState.srcFunc		= rr::BLENDFUNC_ONE;
				state.fragOps.blendRGBState.dstFunc		= rr::BLENDFUNC_ONE;
				state.fragOps.blendRGBState.equation	= rr::BLENDEQUATION_ADD;
				state.fragOps.blendAState				= state.fragOps.blendRGBState;
				break;

			case BLENDMODE_SRC_OVER:
				state.fragOps.blendRGBState.srcFunc		= rr::BLENDFUNC_SRC_ALPHA;
				state.fragOps.blendRGBState.dstFunc		= rr::BLENDFUNC_ONE_MINUS_SRC_ALPHA;
				state.fragOps.blendRGBState.equation	= rr::BLENDEQUATION_ADD;
				state.fragOps.blendAState				= state.fragOps.blendRGBState;
				break;

			default:
				DE_ASSERT(false);
		}
	}

	if (drawOp.depth != DEPTHMODE_NONE)
	{
		state.fragOps.depthTestEnabled = true;

		DE_ASSERT(drawOp.depth == DEPTHMODE_LESS);
		state.fragOps.depthFunc = rr::TESTFUNC_LESS;
	}

	if (drawOp.stencil != STENCILMODE_NONE)
	{
		state.fragOps.stencilTestEnabled = true;

		DE_ASSERT(drawOp.stencil == STENCILMODE_LEQUAL_INC);
		state.fragOps.stencilStates[0].func		= rr::TESTFUNC_LEQUAL;
		state.fragOps.stencilStates[0].sFail	= rr::STENCILOP_KEEP;
		state.fragOps.stencilStates[0].dpFail	= rr::STENCILOP_INCR;
		state.fragOps.stencilStates[0].dpPass	= rr::STENCILOP_INCR;
		state.fragOps.stencilStates[0].ref		= drawOp.stencilRef;
		state.fragOps.stencilStates[1]			= state.fragOps.stencilStates[0];
	}
}

tcu::TextureFormat getColorFormat (const tcu::PixelFormat& colorBits)
{
	using tcu::TextureFormat;

	DE_ASSERT(de::inBounds(colorBits.redBits,	0, 0xff) &&
			  de::inBounds(colorBits.greenBits,	0, 0xff) &&
			  de::inBounds(colorBits.blueBits,	0, 0xff) &&
			  de::inBounds(colorBits.alphaBits,	0, 0xff));

#define PACK_FMT(R, G, B, A) (((R) << 24) | ((G) << 16) | ((B) << 8) | (A))

	// \note [pyry] This may not hold true on some implementations - best effort guess only.
	switch (PACK_FMT(colorBits.redBits, colorBits.greenBits, colorBits.blueBits, colorBits.alphaBits))
	{
		case PACK_FMT(8,8,8,8):		return TextureFormat(TextureFormat::RGBA,	TextureFormat::UNORM_INT8);
		case PACK_FMT(8,8,8,0):		return TextureFormat(TextureFormat::RGB,	TextureFormat::UNORM_INT8);
		case PACK_FMT(4,4,4,4):		return TextureFormat(TextureFormat::RGBA,	TextureFormat::UNORM_SHORT_4444);
		case PACK_FMT(5,5,5,1):		return TextureFormat(TextureFormat::RGBA,	TextureFormat::UNORM_SHORT_5551);
		case PACK_FMT(5,6,5,0):		return TextureFormat(TextureFormat::RGB,	TextureFormat::UNORM_SHORT_565);

		// \note Defaults to RGBA8
		default:					return TextureFormat(TextureFormat::RGBA,	TextureFormat::UNORM_INT8);
	}

#undef PACK_FMT
}

tcu::TextureFormat getDepthFormat (const int depthBits)
{
	switch (depthBits)
	{
		case 0:		return tcu::TextureFormat();
		case 8:		return tcu::TextureFormat(tcu::TextureFormat::D, tcu::TextureFormat::UNORM_INT8);
		case 16:	return tcu::TextureFormat(tcu::TextureFormat::D, tcu::TextureFormat::UNORM_INT16);
		case 24:	return tcu::TextureFormat(tcu::TextureFormat::D, tcu::TextureFormat::UNSIGNED_INT_24_8);
		case 32:
		default:	return tcu::TextureFormat(tcu::TextureFormat::D, tcu::TextureFormat::FLOAT);
	}
}

tcu::TextureFormat getStencilFormat (int stencilBits)
{
	switch (stencilBits)
	{
		case 0:		return tcu::TextureFormat();
		case 8:
		default:	return tcu::TextureFormat(tcu::TextureFormat::S, tcu::TextureFormat::UNSIGNED_INT8);
	}
}

void renderReference (const tcu::PixelBufferAccess& dst, const vector<DrawPrimitiveOp>& drawOps, const tcu::PixelFormat& colorBits, const int depthBits, const int stencilBits, const int numSamples)
{
	const int						width			= dst.getWidth();
	const int						height			= dst.getHeight();

	tcu::TextureLevel				colorBuffer;
	tcu::TextureLevel				depthBuffer;
	tcu::TextureLevel				stencilBuffer;

	rr::Renderer					referenceRenderer;
	rr::VertexAttrib				attributes[2];
	const ReferenceShader			shader;

	attributes[0].type				= rr::VERTEXATTRIBTYPE_FLOAT;
	attributes[0].size				= 4;
	attributes[0].stride			= 0;
	attributes[0].instanceDivisor	= 0;

	attributes[1].type				= rr::VERTEXATTRIBTYPE_FLOAT;
	attributes[1].size				= 4;
	attributes[1].stride			= 0;
	attributes[1].instanceDivisor	= 0;

	// Initialize buffers.
	colorBuffer.setStorage(getColorFormat(colorBits), numSamples, width, height);
	rr::clearMultisampleColorBuffer(colorBuffer, CLEAR_COLOR, rr::WindowRectangle(0, 0, width, height));

	if (depthBits > 0)
	{
		depthBuffer.setStorage(getDepthFormat(depthBits), numSamples, width, height);
		rr::clearMultisampleDepthBuffer(depthBuffer, CLEAR_DEPTH, rr::WindowRectangle(0, 0, width, height));
	}

	if (stencilBits > 0)
	{
		stencilBuffer.setStorage(getStencilFormat(stencilBits), numSamples, width, height);
		rr::clearMultisampleStencilBuffer(stencilBuffer, CLEAR_STENCIL, rr::WindowRectangle(0, 0, width, height));
	}

	const rr::RenderTarget renderTarget(rr::MultisamplePixelBufferAccess::fromMultisampleAccess(colorBuffer.getAccess()),
										rr::MultisamplePixelBufferAccess::fromMultisampleAccess(depthBuffer.getAccess()),
										rr::MultisamplePixelBufferAccess::fromMultisampleAccess(stencilBuffer.getAccess()));

	for (vector<DrawPrimitiveOp>::const_iterator drawOp = drawOps.begin(); drawOp != drawOps.end(); drawOp++)
	{
		// Translate state
		rr::RenderState renderState((rr::ViewportState)(rr::MultisamplePixelBufferAccess::fromMultisampleAccess(colorBuffer.getAccess())));
		toReferenceRenderState(renderState, *drawOp);

		DE_ASSERT(drawOp->type == PRIMITIVETYPE_TRIANGLE);

		attributes[0].pointer = &drawOp->positions[0];
		attributes[1].pointer = &drawOp->colors[0];

		referenceRenderer.draw(
			rr::DrawCommand(
				renderState,
				renderTarget,
				rr::Program(static_cast<const rr::VertexShader*>(&shader), static_cast<const rr::FragmentShader*>(&shader)),
				2,
				attributes,
				rr::PrimitiveList(rr::PRIMITIVETYPE_TRIANGLES, drawOp->count * 3, 0)));
	}

	rr::resolveMultisampleColorBuffer(dst, rr::MultisamplePixelBufferAccess::fromMultisampleAccess(colorBuffer.getAccess()));
}

// API rendering code

class Program
{
public:
					Program				(void) {}
	virtual			~Program			(void) {}

	virtual void	setup				(void) const = DE_NULL;
};

typedef de::SharedPtr<Program> ProgramSp;

#if defined(DEQP_SUPPORT_GLES2) || defined(DEQP_SUPPORT_GLES3)

static const char* s_vertexSrc =
	"attribute highp vec4 a_position;\n"
	"attribute mediump vec4 a_color;\n"
	"varying mediump vec4 v_color;\n"
	"void main (void)\n"
	"{\n"
	"	gl_Position = a_position;\n"
	"	v_color = a_color;\n"
	"}\n";

static const char* s_fragmentSrc =
	"varying mediump vec4 v_color;\n"
	"void main (void)\n"
	"{\n"
	"	gl_FragColor = v_color;\n"
	"}\n";

static deUint32 createShader (deUint32 shaderType, const char* source)
{
	deUint32 shader = glCreateShader(shaderType);
	glShaderSource(shader, 1, &source, DE_NULL);
	glCompileShader(shader);

	int compileStatus = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compileStatus);

	if (!compileStatus)
	{
		glDeleteShader(shader);
		return 0;
	}

	return shader;
}

static deUint32 createProgram (deUint32 vertexShader, deUint32 fragmentShader)
{
	deUint32 program = glCreateProgram();
	glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);
	glLinkProgram(program);

	int linkStatus = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);

	if (!linkStatus)
	{
		glDeleteProgram(program);
		return 0;
	}

	return program;
}

class GLES2Program : public Program
{
public:
	GLES2Program (void)
		: m_program			(0)
		, m_vertexShader	(0)
		, m_fragmentShader	(0)
		, m_positionLoc		(0)
		, m_colorLoc		(0)
	{
		m_vertexShader		= createShader(GL_VERTEX_SHADER, s_vertexSrc);
		m_fragmentShader	= createShader(GL_FRAGMENT_SHADER, s_fragmentSrc);

		if (!m_vertexShader || !m_fragmentShader)
		{
			glDeleteShader(m_vertexShader);
			glDeleteShader(m_fragmentShader);
			throw tcu::TestError("Failed to compile shaders");
		}

		m_program = createProgram(m_vertexShader, m_fragmentShader);
		if (!m_program)
		{
			glDeleteShader(m_vertexShader);
			glDeleteShader(m_fragmentShader);
			throw tcu::TestError("Failed to link program");
		}

		m_positionLoc	= glGetAttribLocation(m_program, "a_position");
		m_colorLoc		= glGetAttribLocation(m_program, "a_color");
	}

	~GLES2Program (void)
	{
	}

	void setup (void) const
	{
		glUseProgram(m_program);
		glEnableVertexAttribArray(m_positionLoc);
		glEnableVertexAttribArray(m_colorLoc);
		GLU_CHECK_MSG("Program setup failed");
	}

	int			getPositionLoc		(void) const { return m_positionLoc;	}
	int			getColorLoc			(void) const { return m_colorLoc;		}

private:
	deUint32	m_program;
	deUint32	m_vertexShader;
	deUint32	m_fragmentShader;
	int			m_positionLoc;
	int			m_colorLoc;
};

void clearGLES2 (const tcu::Vec4& color, const float depth, const int stencil)
{
	glClearColor(color.x(), color.y(), color.z(), color.w());
	glClearDepthf(depth);
	glClearStencil(stencil);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
}

void drawGLES2 (const Program& program, const DrawPrimitiveOp& drawOp)
{
	const GLES2Program& gles2Program = dynamic_cast<const GLES2Program&>(program);

	switch (drawOp.blend)
	{
		case BLENDMODE_NONE:
			glDisable(GL_BLEND);
			break;

		case BLENDMODE_ADDITIVE:
			glEnable(GL_BLEND);
			glBlendFunc(GL_ONE, GL_ONE);
			break;

		case BLENDMODE_SRC_OVER:
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			break;

		default:
			DE_ASSERT(false);
	}

	switch (drawOp.depth)
	{
		case DEPTHMODE_NONE:
			glDisable(GL_DEPTH_TEST);
			break;

		case DEPTHMODE_LESS:
			glEnable(GL_DEPTH_TEST);
			break;

		default:
			DE_ASSERT(false);
	}

	switch (drawOp.stencil)
	{
		case STENCILMODE_NONE:
			glDisable(GL_STENCIL_TEST);
			break;

		case STENCILMODE_LEQUAL_INC:
			glEnable(GL_STENCIL_TEST);
			glStencilFunc(GL_LEQUAL, drawOp.stencilRef, ~0u);
			glStencilOp(GL_KEEP, GL_INCR, GL_INCR);
			break;

		default:
			DE_ASSERT(false);
	}

	glVertexAttribPointer(gles2Program.getPositionLoc(), 4, GL_FLOAT, GL_FALSE, 0, &drawOp.positions[0]);
	glVertexAttribPointer(gles2Program.getColorLoc(), 4, GL_FLOAT, GL_FALSE, 0, &drawOp.colors[0]);

	DE_ASSERT(drawOp.type == PRIMITIVETYPE_TRIANGLE);
	glDrawArrays(GL_TRIANGLES, 0, drawOp.count*3);
}

static void readPixelsGLES2 (tcu::Surface& dst)
{
	glReadPixels(0, 0, dst.getWidth(), dst.getHeight(), GL_RGBA, GL_UNSIGNED_BYTE, dst.getAccess().getDataPtr());
}

#endif

Program* createProgram (EGLint api)
{
	switch (api)
	{
#if defined(DEQP_SUPPORT_GLES2) || defined(DEQP_SUPPORT_GLES3)
		case EGL_OPENGL_ES2_BIT:		return new GLES2Program();
		case EGL_OPENGL_ES3_BIT_KHR:	return new GLES2Program();
#endif
		default:
			throw tcu::NotSupportedError("Unsupported API");
	}
}

void draw (EGLint api, const Program& program, const DrawPrimitiveOp& drawOp)
{
	switch (api)
	{
#if defined(DEQP_SUPPORT_GLES2) || defined(DEQP_SUPPORT_GLES3)
		case EGL_OPENGL_ES2_BIT:		drawGLES2(program, drawOp);		break;
		case EGL_OPENGL_ES3_BIT_KHR:	drawGLES2(program, drawOp);		break;
#endif
		default:
			throw tcu::NotSupportedError("Unsupported API");
	}
}

void clear (EGLint api, const tcu::Vec4& color, const float depth, const int stencil)
{
	switch (api)
	{
#if defined(DEQP_SUPPORT_GLES2) || defined(DEQP_SUPPORT_GLES3)
		case EGL_OPENGL_ES2_BIT:		clearGLES2(color, depth, stencil);		break;
		case EGL_OPENGL_ES3_BIT_KHR:	clearGLES2(color, depth, stencil);		break;
#endif
		default:
			throw tcu::NotSupportedError("Unsupported API");
	}
}

static void readPixels (EGLint api, tcu::Surface& dst)
{
	switch (api)
	{
#if defined(DEQP_SUPPORT_GLES2) || defined(DEQP_SUPPORT_GLES3)
		case EGL_OPENGL_ES2_BIT:		readPixelsGLES2(dst);		break;
		case EGL_OPENGL_ES3_BIT_KHR:	readPixelsGLES2(dst);		break;
#endif
		default:
			throw tcu::NotSupportedError("Unsupported API");
	}
}

tcu::PixelFormat getPixelFormat (const tcu::egl::Display& display, EGLConfig config)
{
	tcu::PixelFormat fmt;
	display.describeConfig(config, fmt);
	return fmt;
}

} // anonymous

// SingleThreadRenderCase

class SingleThreadRenderCase : public MultiContextRenderCase
{
public:
						SingleThreadRenderCase		(EglTestContext& eglTestCtx, const char* name, const char* description, EGLint api, EGLint surfaceType, const std::vector<EGLint>& configIds, int numContextsPerApi);

private:
	virtual void		executeForContexts			(tcu::egl::Display& display, tcu::egl::Surface& surface, EGLConfig config, const std::vector<std::pair<EGLint, tcu::egl::Context*> >& contexts);
};

// SingleThreadColorClearCase

SingleThreadRenderCase::SingleThreadRenderCase (EglTestContext& eglTestCtx, const char* name, const char* description, EGLint api, EGLint surfaceType, const std::vector<EGLint>& configIds, int numContextsPerApi)
	: MultiContextRenderCase(eglTestCtx, name, description, api, surfaceType, configIds, numContextsPerApi)
{
}

void SingleThreadRenderCase::executeForContexts (tcu::egl::Display& display, tcu::egl::Surface& surface, EGLConfig config, const std::vector<std::pair<EGLint, tcu::egl::Context*> >& contexts)
{
	const int				width		= surface.getWidth();
	const int				height		= surface.getHeight();
	const int				numContexts	= (int)contexts.size();
	const int				drawsPerCtx	= 2;
	const int				numIters	= 2;
	const float				threshold	= 0.02f;

	const tcu::PixelFormat	pixelFmt	= getPixelFormat(display, config);
	const int				depthBits	= display.getConfigAttrib(config, EGL_DEPTH_SIZE);
	const int				stencilBits	= display.getConfigAttrib(config, EGL_STENCIL_SIZE);
	const int				numSamples	= display.getConfigAttrib(config, EGL_SAMPLES);

	TestLog&				log			= m_testCtx.getLog();

	tcu::Surface			refFrame	(width, height);
	tcu::Surface			frame		(width, height);

	de::Random				rnd			(deStringHash(getName()) ^ deInt32Hash(numContexts));
	vector<ProgramSp>		programs	(contexts.size());
	vector<DrawPrimitiveOp>	drawOps;

	// Log basic information about config.
	log << TestLog::Message << "EGL_RED_SIZE = "		<< pixelFmt.redBits << TestLog::EndMessage;
	log << TestLog::Message << "EGL_GREEN_SIZE = "		<< pixelFmt.greenBits << TestLog::EndMessage;
	log << TestLog::Message << "EGL_BLUE_SIZE = "		<< pixelFmt.blueBits << TestLog::EndMessage;
	log << TestLog::Message << "EGL_ALPHA_SIZE = "		<< pixelFmt.alphaBits << TestLog::EndMessage;
	log << TestLog::Message << "EGL_DEPTH_SIZE = "		<< depthBits << TestLog::EndMessage;
	log << TestLog::Message << "EGL_STENCIL_SIZE = "	<< stencilBits << TestLog::EndMessage;
	log << TestLog::Message << "EGL_SAMPLES = "			<< numSamples << TestLog::EndMessage;

	// Generate draw ops.
	drawOps.resize(numContexts*drawsPerCtx*numIters);
	for (vector<DrawPrimitiveOp>::iterator drawOp = drawOps.begin(); drawOp != drawOps.end(); ++drawOp)
		randomizeDrawOp(rnd, *drawOp);

	// Create and setup programs per context
	for (int ctxNdx = 0; ctxNdx < numContexts; ctxNdx++)
	{
		EGLint				api			= contexts[ctxNdx].first;
		tcu::egl::Context*	context		= contexts[ctxNdx].second;

		eglMakeCurrent(display.getEGLDisplay(), surface.getEGLSurface(), surface.getEGLSurface(), context->getEGLContext());
		TCU_CHECK_EGL();

		programs[ctxNdx] = ProgramSp(createProgram(api));
		programs[ctxNdx]->setup();
	}

	// Clear to black using first context.
	{
		EGLint				api			= contexts[0].first;
		tcu::egl::Context*	context		= contexts[0].second;

		eglMakeCurrent(display.getEGLDisplay(), surface.getEGLSurface(), surface.getEGLSurface(), context->getEGLContext());
		TCU_CHECK_EGL();

		clear(api, CLEAR_COLOR, CLEAR_DEPTH, CLEAR_STENCIL);
	}

	// Render.
	for (int iterNdx = 0; iterNdx < numIters; iterNdx++)
	{
		for (int ctxNdx = 0; ctxNdx < numContexts; ctxNdx++)
		{
			EGLint				api			= contexts[ctxNdx].first;
			tcu::egl::Context*	context		= contexts[ctxNdx].second;

			eglMakeCurrent(display.getEGLDisplay(), surface.getEGLSurface(), surface.getEGLSurface(), context->getEGLContext());
			TCU_CHECK_EGL();

			for (int drawNdx = 0; drawNdx < drawsPerCtx; drawNdx++)
			{
				const DrawPrimitiveOp& drawOp = drawOps[iterNdx*numContexts*drawsPerCtx + ctxNdx*drawsPerCtx + drawNdx];
				draw(api, *programs[ctxNdx], drawOp);
			}
		}
	}

	// Read pixels using first context. \todo [pyry] Randomize?
	{
		EGLint				api		= contexts[0].first;
		tcu::egl::Context*	context	= contexts[0].second;

		eglMakeCurrent(display.getEGLDisplay(), surface.getEGLSurface(), surface.getEGLSurface(), context->getEGLContext());
		TCU_CHECK_EGL();

		readPixels(api, frame);
	}

	// Render reference.
	// \note Reference image is always generated using single-sampling.
	renderReference(refFrame.getAccess(), drawOps, pixelFmt, depthBits, stencilBits, 1);

	// Compare images
	{
		bool imagesOk = tcu::fuzzyCompare(log, "ComparisonResult", "Image comparison result", refFrame, frame, threshold, tcu::COMPARE_LOG_RESULT);

		if (!imagesOk)
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Image comparison failed");
	}
}

// MultiThreadRenderCase

class MultiThreadRenderCase : public MultiContextRenderCase
{
public:
						MultiThreadRenderCase		(EglTestContext& eglTestCtx, const char* name, const char* description, EGLint api, EGLint surfaceType, const std::vector<EGLint>& configIds, int numContextsPerApi);

private:
	virtual void		executeForContexts			(tcu::egl::Display& display, tcu::egl::Surface& surface, EGLConfig config, const std::vector<std::pair<EGLint, tcu::egl::Context*> >& contexts);
};

class RenderTestThread;

typedef de::SharedPtr<RenderTestThread>	RenderTestThreadSp;
typedef de::SharedPtr<de::Semaphore>	SemaphoreSp;

struct DrawOpPacket
{
	DrawOpPacket (void)
		: drawOps	(DE_NULL)
		, numOps	(0)
	{
	}

	const DrawPrimitiveOp*	drawOps;
	int						numOps;
	SemaphoreSp				wait;
	SemaphoreSp				signal;
};

class RenderTestThread : public de::Thread
{
public:
	RenderTestThread (tcu::egl::Display& display, tcu::egl::Surface& surface, tcu::egl::Context& context, EGLint api, const Program& program, const std::vector<DrawOpPacket>& packets)
		: m_display	(display)
		, m_surface	(surface)
		, m_context	(context)
		, m_api		(api)
		, m_program	(program)
		, m_packets	(packets)
	{
	}

	void run (void)
	{
		for (std::vector<DrawOpPacket>::const_iterator packetIter = m_packets.begin(); packetIter != m_packets.end(); packetIter++)
		{
			// Wait until it is our turn.
			packetIter->wait->decrement();

			// Acquire context.
			eglMakeCurrent(m_display.getEGLDisplay(), m_surface.getEGLSurface(), m_surface.getEGLSurface(), m_context.getEGLContext());

			// Execute rendering.
			for (int ndx = 0; ndx < packetIter->numOps; ndx++)
				draw(m_api, m_program, packetIter->drawOps[ndx]);

			// Release context.
			eglMakeCurrent(m_display.getEGLDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

			// Signal completion.
			packetIter->signal->increment();
		}
	}

private:
	tcu::egl::Display&					m_display;
	tcu::egl::Surface&					m_surface;
	tcu::egl::Context&					m_context;
	EGLint								m_api;
	const Program&						m_program;
	const std::vector<DrawOpPacket>&	m_packets;
};

MultiThreadRenderCase::MultiThreadRenderCase (EglTestContext& eglTestCtx, const char* name, const char* description, EGLint api, EGLint surfaceType, const std::vector<EGLint>& configIds, int numContextsPerApi)
	: MultiContextRenderCase(eglTestCtx, name, description, api, surfaceType, configIds, numContextsPerApi)
{
}

void MultiThreadRenderCase::executeForContexts (tcu::egl::Display& display, tcu::egl::Surface& surface, EGLConfig config, const std::vector<std::pair<EGLint, tcu::egl::Context*> >& contexts)
{
	const int				width				= surface.getWidth();
	const int				height				= surface.getHeight();
	const int				numContexts			= (int)contexts.size();
	const int				opsPerPacket		= 2;
	const int				packetsPerThread	= 2;
	const int				numThreads			= numContexts;
	const int				numPackets			= numThreads * packetsPerThread;
	const float				threshold			= 0.02f;

	const tcu::PixelFormat	pixelFmt			= getPixelFormat(display, config);
	const int				depthBits			= display.getConfigAttrib(config, EGL_DEPTH_SIZE);
	const int				stencilBits			= display.getConfigAttrib(config, EGL_STENCIL_SIZE);
	const int				numSamples			= display.getConfigAttrib(config, EGL_SAMPLES);

	TestLog&				log					= m_testCtx.getLog();

	tcu::Surface			refFrame			(width, height);
	tcu::Surface			frame				(width, height);

	de::Random				rnd					(deStringHash(getName()) ^ deInt32Hash(numContexts));

	// Resources that need cleanup
	vector<ProgramSp>				programs	(numContexts);
	vector<SemaphoreSp>				semaphores	(numPackets+1);
	vector<DrawPrimitiveOp>			drawOps		(numPackets*opsPerPacket);
	vector<vector<DrawOpPacket> >	packets		(numThreads);
	vector<RenderTestThreadSp>		threads		(numThreads);

	// Log basic information about config.
	log << TestLog::Message << "EGL_RED_SIZE = "		<< pixelFmt.redBits << TestLog::EndMessage;
	log << TestLog::Message << "EGL_GREEN_SIZE = "		<< pixelFmt.greenBits << TestLog::EndMessage;
	log << TestLog::Message << "EGL_BLUE_SIZE = "		<< pixelFmt.blueBits << TestLog::EndMessage;
	log << TestLog::Message << "EGL_ALPHA_SIZE = "		<< pixelFmt.alphaBits << TestLog::EndMessage;
	log << TestLog::Message << "EGL_DEPTH_SIZE = "		<< depthBits << TestLog::EndMessage;
	log << TestLog::Message << "EGL_STENCIL_SIZE = "	<< stencilBits << TestLog::EndMessage;
	log << TestLog::Message << "EGL_SAMPLES = "			<< numSamples << TestLog::EndMessage;

	// Initialize semaphores.
	for (vector<SemaphoreSp>::iterator sem = semaphores.begin(); sem != semaphores.end(); ++sem)
		*sem = SemaphoreSp(new de::Semaphore(0));

	// Create draw ops.
	for (vector<DrawPrimitiveOp>::iterator drawOp = drawOps.begin(); drawOp != drawOps.end(); ++drawOp)
		randomizeDrawOp(rnd, *drawOp);

	// Create packets.
	for (int threadNdx = 0; threadNdx < numThreads; threadNdx++)
	{
		packets[threadNdx].resize(packetsPerThread);

		for (int packetNdx = 0; packetNdx < packetsPerThread; packetNdx++)
		{
			DrawOpPacket& packet = packets[threadNdx][packetNdx];

			// Threads take turns with packets.
			packet.wait		= semaphores[packetNdx*numThreads + threadNdx];
			packet.signal	= semaphores[packetNdx*numThreads + threadNdx + 1];
			packet.numOps	= opsPerPacket;
			packet.drawOps	= &drawOps[(packetNdx*numThreads + threadNdx)*opsPerPacket];
		}
	}

	// Create and setup programs per context
	for (int ctxNdx = 0; ctxNdx < numContexts; ctxNdx++)
	{
		EGLint				api			= contexts[ctxNdx].first;
		tcu::egl::Context*	context		= contexts[ctxNdx].second;

		eglMakeCurrent(display.getEGLDisplay(), surface.getEGLSurface(), surface.getEGLSurface(), context->getEGLContext());
		TCU_CHECK_EGL();

		programs[ctxNdx] = ProgramSp(createProgram(api));
		programs[ctxNdx]->setup();

		// Release context
		eglMakeCurrent(display.getEGLDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	}

	// Clear to black using first context.
	{
		EGLint				api			= contexts[0].first;
		tcu::egl::Context*	context		= contexts[0].second;

		eglMakeCurrent(display.getEGLDisplay(), surface.getEGLSurface(), surface.getEGLSurface(), context->getEGLContext());
		TCU_CHECK_EGL();

		clear(api, CLEAR_COLOR, CLEAR_DEPTH, CLEAR_STENCIL);

		// Release context
		eglMakeCurrent(display.getEGLDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	}

	// Create and launch threads (actual rendering starts once first semaphore is signaled).
	for (int threadNdx = 0; threadNdx < numThreads; threadNdx++)
	{
		threads[threadNdx] = RenderTestThreadSp(new RenderTestThread(display, surface, *contexts[threadNdx].second, contexts[threadNdx].first, *programs[threadNdx], packets[threadNdx]));
		threads[threadNdx]->start();
	}

	// Signal start and wait until complete.
	semaphores.front()->increment();
	semaphores.back()->decrement();

	// Read pixels using first context. \todo [pyry] Randomize?
	{
		EGLint				api		= contexts[0].first;
		tcu::egl::Context*	context	= contexts[0].second;

		eglMakeCurrent(display.getEGLDisplay(), surface.getEGLSurface(), surface.getEGLSurface(), context->getEGLContext());
		TCU_CHECK_EGL();

		readPixels(api, frame);
	}

	// Join threads.
	for (int threadNdx = 0; threadNdx < numThreads; threadNdx++)
		threads[threadNdx]->join();

	// Render reference.
	renderReference(refFrame.getAccess(), drawOps, pixelFmt, depthBits, stencilBits, 1);

	// Compare images
	{
		bool imagesOk = tcu::fuzzyCompare(log, "ComparisonResult", "Image comparison result", refFrame, frame, threshold, tcu::COMPARE_LOG_RESULT);

		if (!imagesOk)
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Image comparison failed");
	}
}

RenderTests::RenderTests (EglTestContext& eglTestCtx)
	: TestCaseGroup(eglTestCtx, "render", "Basic rendering with different client APIs")
{
}

RenderTests::~RenderTests (void)
{
}

struct RenderGroupSpec
{
	const char*		name;
	const char*		desc;
	EGLint			apiBits;
	int				numContextsPerApi;
};

template <class RenderClass>
static void createRenderGroups (EglTestContext& eglTestCtx, tcu::TestCaseGroup* group, const RenderGroupSpec* first, const RenderGroupSpec* last)
{
	for (const RenderGroupSpec* groupIter = first; groupIter != last; groupIter++)
	{
		tcu::TestCaseGroup* configGroup = new tcu::TestCaseGroup(eglTestCtx.getTestContext(), groupIter->name, groupIter->desc);
		group->addChild(configGroup);

		vector<RenderConfigIdSet>	configSets;
		eglu::FilterList			filters;
		filters << (eglu::ConfigRenderableType() & groupIter->apiBits);
		getDefaultRenderConfigIdSets(configSets, eglTestCtx.getConfigs(), filters);

		for (vector<RenderConfigIdSet>::const_iterator setIter = configSets.begin(); setIter != configSets.end(); setIter++)
			configGroup->addChild(new RenderClass(eglTestCtx, setIter->getName(), "", groupIter->apiBits, setIter->getSurfaceTypeMask(), setIter->getConfigIds(), groupIter->numContextsPerApi));
	}
}

void RenderTests::init (void)
{
	static const RenderGroupSpec singleContextCases[] =
	{
//		{ "gles1",			"Primitive rendering using GLES1",												EGL_OPENGL_ES_BIT,										1 },
		{ "gles2",			"Primitive rendering using GLES2",												EGL_OPENGL_ES2_BIT,										1 },
		{ "gles3",			"Primitive rendering using GLES3",												EGL_OPENGL_ES3_BIT_KHR,									1 },
//		{ "vg",				"Primitive rendering using OpenVG",												EGL_OPENVG_BIT,											1 }
	};

	static const RenderGroupSpec multiContextCases[] =
	{
//		{ "gles1",				"Primitive rendering using multiple GLES1 contexts to shared surface",		EGL_OPENGL_ES_BIT,												3 },
		{ "gles2",				"Primitive rendering using multiple GLES2 contexts to shared surface",		EGL_OPENGL_ES2_BIT,												3 },
		{ "gles3",				"Primitive rendering using multiple GLES3 contexts to shared surface",		EGL_OPENGL_ES3_BIT_KHR,											3 },
//		{ "vg",					"Primitive rendering using multiple OpenVG contexts to shared surface",		EGL_OPENVG_BIT,													3 },
//		{ "gles1_gles2",		"Primitive rendering using multiple APIs to shared surface",				EGL_OPENGL_ES_BIT|EGL_OPENGL_ES2_BIT,							1 },
//		{ "gles1_gles2_gles3",	"Primitive rendering using multiple APIs to shared surface",				EGL_OPENGL_ES_BIT|EGL_OPENGL_ES2_BIT|EGL_OPENGL_ES3_BIT_KHR,	1 },
		{ "gles2_gles3",		"Primitive rendering using multiple APIs to shared surface",				EGL_OPENGL_ES2_BIT|EGL_OPENGL_ES3_BIT_KHR,						1 },
//		{ "gles1_vg",			"Primitive rendering using multiple APIs to shared surface",				EGL_OPENGL_ES_BIT|EGL_OPENVG_BIT,								1 },
//		{ "gles2_vg",			"Primitive rendering using multiple APIs to shared surface",				EGL_OPENGL_ES2_BIT|EGL_OPENVG_BIT,								1 },
//		{ "gles3_vg",			"Primitive rendering using multiple APIs to shared surface",				EGL_OPENGL_ES3_BIT_KHR|EGL_OPENVG_BIT,							1 },
//		{ "gles1_gles2_vg",		"Primitive rendering using multiple APIs to shared surface",				EGL_OPENGL_ES_BIT|EGL_OPENGL_ES2_BIT|EGL_OPENVG_BIT,			1 }
	};

	tcu::TestCaseGroup* singleContextGroup = new tcu::TestCaseGroup(m_testCtx, "single_context", "Single-context rendering");
	addChild(singleContextGroup);
	createRenderGroups<SingleThreadRenderCase>(m_eglTestCtx, singleContextGroup, &singleContextCases[0], &singleContextCases[DE_LENGTH_OF_ARRAY(singleContextCases)]);

	tcu::TestCaseGroup* multiContextGroup = new tcu::TestCaseGroup(m_testCtx, "multi_context", "Multi-context rendering with shared surface");
	addChild(multiContextGroup);
	createRenderGroups<SingleThreadRenderCase>(m_eglTestCtx, multiContextGroup, &multiContextCases[0], &multiContextCases[DE_LENGTH_OF_ARRAY(multiContextCases)]);

	tcu::TestCaseGroup* multiThreadGroup = new tcu::TestCaseGroup(m_testCtx, "multi_thread", "Multi-thread rendering with shared surface");
	addChild(multiThreadGroup);
	createRenderGroups<MultiThreadRenderCase>(m_eglTestCtx, multiThreadGroup, &multiContextCases[0], &multiContextCases[DE_LENGTH_OF_ARRAY(multiContextCases)]);
}

} // egl
} // deqp
