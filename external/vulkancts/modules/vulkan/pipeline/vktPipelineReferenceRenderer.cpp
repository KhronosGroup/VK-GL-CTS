/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Imagination Technologies Ltd.
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
 * \brief Reference renderer.
 *//*--------------------------------------------------------------------*/

#include "vktPipelineReferenceRenderer.hpp"
#include "vktPipelineClearUtil.hpp"
#include "rrShadingContext.hpp"
#include "rrVertexAttrib.hpp"

namespace vkt
{
namespace pipeline
{

using namespace vk;

rr::TestFunc mapVkCompareOp (VkCompareOp compareFunc)
{
	switch (compareFunc)
	{
		case VK_COMPARE_OP_NEVER:			return rr::TESTFUNC_NEVER;
		case VK_COMPARE_OP_LESS:			return rr::TESTFUNC_LESS;
		case VK_COMPARE_OP_EQUAL:			return rr::TESTFUNC_EQUAL;
		case VK_COMPARE_OP_LESS_EQUAL:		return rr::TESTFUNC_LEQUAL;
		case VK_COMPARE_OP_GREATER:			return rr::TESTFUNC_GREATER;
		case VK_COMPARE_OP_NOT_EQUAL:		return rr::TESTFUNC_NOTEQUAL;
		case VK_COMPARE_OP_GREATER_EQUAL:	return rr::TESTFUNC_GEQUAL;
		case VK_COMPARE_OP_ALWAYS:			return rr::TESTFUNC_ALWAYS;
		default:
			DE_ASSERT(false);
	}
	return rr::TESTFUNC_LAST;
}

rr::PrimitiveType mapVkPrimitiveTopology (VkPrimitiveTopology primitiveTopology)
{
	switch (primitiveTopology)
	{
		case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:				return rr::PRIMITIVETYPE_POINTS;
		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:				return rr::PRIMITIVETYPE_LINES;
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:				return rr::PRIMITIVETYPE_LINE_STRIP;
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:			return rr::PRIMITIVETYPE_TRIANGLES;
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:			return rr::PRIMITIVETYPE_TRIANGLE_FAN;
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:			return rr::PRIMITIVETYPE_TRIANGLE_STRIP;
		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_ADJ:			return rr::PRIMITIVETYPE_LINES_ADJACENCY;
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_ADJ:			return rr::PRIMITIVETYPE_LINE_STRIP_ADJACENCY;
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_ADJ:		return rr::PRIMITIVETYPE_TRIANGLES_ADJACENCY;
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_ADJ:		return rr::PRIMITIVETYPE_TRIANGLE_STRIP_ADJACENCY;
		default:
			DE_ASSERT(false);
	}
	return rr::PRIMITIVETYPE_LAST;
}

ReferenceRenderer::ReferenceRenderer(int						surfaceWidth,
									 int						surfaceHeight,
									 int						numSamples,
									 const tcu::TextureFormat&	colorFormat,
									 const tcu::TextureFormat&	depthStencilFormat,
									 const rr::Program* const	program)
	: m_surfaceWidth		(surfaceWidth)
	, m_surfaceHeight		(surfaceHeight)
	, m_numSamples			(numSamples)
	, m_colorFormat			(colorFormat)
	, m_depthStencilFormat	(depthStencilFormat)
	, m_program				(program)
{
	const tcu::TextureChannelClass	formatClass				= tcu::getTextureChannelClass(colorFormat.type);
	const bool						hasDepthStencil			= (m_depthStencilFormat.order != tcu::TextureFormat::CHANNELORDER_LAST);
	const bool						hasDepthBufferOnly		= (m_depthStencilFormat.order == tcu::TextureFormat::D);
	const bool						hasStencilBufferOnly	= (m_depthStencilFormat.order == tcu::TextureFormat::S);
	const int						actualSamples			= (formatClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER || formatClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER)? 1: m_numSamples;

	m_colorBuffer.setStorage(m_colorFormat, actualSamples, m_surfaceWidth, m_surfaceHeight);
	m_resolveColorBuffer.setStorage(m_colorFormat, m_surfaceWidth, m_surfaceHeight);

	if (formatClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER)
	{
		tcu::clear(m_colorBuffer.getAccess(), defaultClearColorInt(m_colorFormat));
		tcu::clear(m_resolveColorBuffer.getAccess(), defaultClearColorInt(m_colorFormat));
	}
	else if (formatClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER)
	{
		tcu::clear(m_colorBuffer.getAccess(), defaultClearColorUint(m_colorFormat));
		tcu::clear(m_resolveColorBuffer.getAccess(), defaultClearColorUint(m_colorFormat));
	}
	else
	{
		tcu::clear(m_colorBuffer.getAccess(), defaultClearColorFloat(m_colorFormat));
		tcu::clear(m_resolveColorBuffer.getAccess(), defaultClearColorFloat(m_colorFormat));
	}

	if (hasDepthStencil)
	{
		if (hasDepthBufferOnly)
		{
			m_depthStencilBuffer.setStorage(m_depthStencilFormat, actualSamples, surfaceWidth, surfaceHeight);
			tcu::clearDepth(m_depthStencilBuffer.getAccess(), defaultClearDepth());

			m_renderTarget = new rr::RenderTarget(rr::MultisamplePixelBufferAccess::fromMultisampleAccess(m_colorBuffer.getAccess()),
												  rr::MultisamplePixelBufferAccess::fromMultisampleAccess(m_depthStencilBuffer.getAccess()));
		}
		else if (hasStencilBufferOnly)
		{
			m_depthStencilBuffer.setStorage(m_depthStencilFormat, actualSamples, surfaceWidth, surfaceHeight);
			tcu::clearStencil(m_depthStencilBuffer.getAccess(), defaultClearStencil());

			m_renderTarget = new rr::RenderTarget(rr::MultisamplePixelBufferAccess::fromMultisampleAccess(m_colorBuffer.getAccess()),
												  rr::MultisamplePixelBufferAccess(),
												  rr::MultisamplePixelBufferAccess::fromMultisampleAccess(m_depthStencilBuffer.getAccess()));
		}
		else
		{
			m_depthStencilBuffer.setStorage(m_depthStencilFormat, actualSamples, surfaceWidth, surfaceHeight);

			tcu::clearDepth(m_depthStencilBuffer.getAccess(), defaultClearDepth());
			tcu::clearStencil(m_depthStencilBuffer.getAccess(), defaultClearStencil());

			m_renderTarget = new rr::RenderTarget(rr::MultisamplePixelBufferAccess::fromMultisampleAccess(m_colorBuffer.getAccess()),
												  rr::MultisamplePixelBufferAccess::fromMultisampleAccess(m_depthStencilBuffer.getAccess()),
												  rr::MultisamplePixelBufferAccess::fromMultisampleAccess(m_depthStencilBuffer.getAccess()));
		}
	}
	else
	{
		m_renderTarget = new rr::RenderTarget(rr::MultisamplePixelBufferAccess::fromMultisampleAccess(m_colorBuffer.getAccess()));
	}
}

ReferenceRenderer::~ReferenceRenderer (void)
{
	delete m_renderTarget;
}

void ReferenceRenderer::draw (const rr::RenderState&			renderState,
							  const rr::PrimitiveType			primitive,
							  const std::vector<Vertex4RGBA>&	vertexBuffer)
{
	const rr::PrimitiveList primitives(primitive, (int)vertexBuffer.size(), 0);

	std::vector<tcu::Vec4> positions;
	std::vector<tcu::Vec4> colors;

	for (size_t vertexNdx = 0; vertexNdx < vertexBuffer.size(); vertexNdx++)
	{
		const Vertex4RGBA& v = vertexBuffer[vertexNdx];
		positions.push_back(v.position);
		colors.push_back(v.color);
	}

	rr::VertexAttrib vertexAttribs[2];

	// Position attribute
	vertexAttribs[0].type		= rr::VERTEXATTRIBTYPE_FLOAT;
	vertexAttribs[0].size		= 4;
	vertexAttribs[0].pointer	= positions.data();
	// UV attribute
	vertexAttribs[1].type		= rr::VERTEXATTRIBTYPE_FLOAT;
	vertexAttribs[1].size		= 4;
	vertexAttribs[1].pointer	= colors.data();

	rr::DrawCommand drawQuadCommand(renderState, *m_renderTarget, *m_program, 2, vertexAttribs, primitives);

	m_renderer.draw(drawQuadCommand);
}

tcu::PixelBufferAccess ReferenceRenderer::getAccess (void)
{
	rr::MultisampleConstPixelBufferAccess multiSampleAccess = rr::MultisampleConstPixelBufferAccess::fromMultisampleAccess(m_colorBuffer.getAccess());
	rr::resolveMultisampleColorBuffer(m_resolveColorBuffer.getAccess(), multiSampleAccess);

	return m_resolveColorBuffer.getAccess();
}

const rr::ViewportState ReferenceRenderer::getViewportState (void) const
{
	return rr::ViewportState(rr::WindowRectangle(0, 0, m_surfaceWidth, m_surfaceHeight));
}

} // pipeline
} // vkt
