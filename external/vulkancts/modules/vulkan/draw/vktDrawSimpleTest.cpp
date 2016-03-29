/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Intel Corporation
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
 * \brief Simple Draw Tests
 *//*--------------------------------------------------------------------*/

#include "vktDrawSimpleTest.hpp"

#include "vktTestCaseUtil.hpp"
#include "vktDrawTestCaseUtil.hpp"

#include "vktDrawBaseClass.hpp"

#include "tcuTestLog.hpp"
#include "tcuResource.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuRGBA.hpp"

#include "vkDefs.hpp"

namespace vkt
{
namespace Draw
{
namespace
{
class SimpleDraw : public DrawTestsBaseClass
{
public:
							SimpleDraw				(Context &context, ShaderMap shaders, vk::VkPrimitiveTopology topology);
	virtual tcu::TestStatus iterate					(void);
};

class SimpleDrawInstanced : public SimpleDraw
{
public:
							SimpleDrawInstanced		(Context &context, ShaderMap shaders, vk::VkPrimitiveTopology topology);
	tcu::TestStatus			iterate					(void);
};

SimpleDraw::SimpleDraw (Context &context, ShaderMap shaders, vk::VkPrimitiveTopology topology)
	: DrawTestsBaseClass	(context, shaders[glu::SHADERTYPE_VERTEX], shaders[glu::SHADERTYPE_FRAGMENT])
{
	m_topology = topology;

	switch (m_topology)
	{
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
			m_data.push_back(PositionColorVertex(tcu::Vec4(	 1.0f,	-1.0f,	1.0f,	1.0f), tcu::RGBA::blue().toVec()));
			m_data.push_back(PositionColorVertex(tcu::Vec4(	-1.0f,	 1.0f,	1.0f,	1.0f), tcu::RGBA::blue().toVec()));
			m_data.push_back(PositionColorVertex(tcu::Vec4(	-0.3f,	-0.3f,	1.0f,	1.0f), tcu::RGBA::blue().toVec()));
			m_data.push_back(PositionColorVertex(tcu::Vec4(	-0.3f,	 0.3f,	1.0f,	1.0f), tcu::RGBA::blue().toVec()));
			m_data.push_back(PositionColorVertex(tcu::Vec4(	 0.3f,	-0.3f,	1.0f,	1.0f), tcu::RGBA::blue().toVec()));
			m_data.push_back(PositionColorVertex(tcu::Vec4(	 0.3f,	-0.3f,	1.0f,	1.0f), tcu::RGBA::blue().toVec()));
			m_data.push_back(PositionColorVertex(tcu::Vec4(	 0.3f,	 0.3f,	1.0f,	1.0f), tcu::RGBA::blue().toVec()));
			m_data.push_back(PositionColorVertex(tcu::Vec4(	-0.3f,	 0.3f,	1.0f,	1.0f), tcu::RGBA::blue().toVec()));
			m_data.push_back(PositionColorVertex(tcu::Vec4(	-1.0f,	 1.0f,	1.0f,	1.0f), tcu::RGBA::blue().toVec()));
			break;
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
			m_data.push_back(PositionColorVertex(tcu::Vec4(	 1.0f,	-1.0f,	1.0f,	1.0f), tcu::RGBA::blue().toVec()));
			m_data.push_back(PositionColorVertex(tcu::Vec4(	-1.0f,	 1.0f,	1.0f,	1.0f), tcu::RGBA::blue().toVec()));
			m_data.push_back(PositionColorVertex(tcu::Vec4(	-0.3f,	-0.3f,	1.0f,	1.0f), tcu::RGBA::blue().toVec()));
			m_data.push_back(PositionColorVertex(tcu::Vec4(	-0.3f,	 0.3f,	1.0f,	1.0f), tcu::RGBA::blue().toVec()));
			m_data.push_back(PositionColorVertex(tcu::Vec4(	 0.3f,	-0.3f,	1.0f,	1.0f), tcu::RGBA::blue().toVec()));
			m_data.push_back(PositionColorVertex(tcu::Vec4(	 0.3f,	 0.3f,	1.0f,	1.0f), tcu::RGBA::blue().toVec()));
			m_data.push_back(PositionColorVertex(tcu::Vec4(	-0.3f,	 0.3f,	1.0f,	1.0f), tcu::RGBA::blue().toVec()));
			m_data.push_back(PositionColorVertex(tcu::Vec4(	-1.0f,	 1.0f,	1.0f,	1.0f), tcu::RGBA::blue().toVec()));
			break;
		case vk::VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
		case vk::VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
		case vk::VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
		case vk::VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
		case vk::VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
		case vk::VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
		case vk::VK_PRIMITIVE_TOPOLOGY_LAST:
			DE_FATAL("Topology not implemented");
			break;
		default:
			DE_FATAL("Unknown topology");
			break;
	}
	initialize();
}

tcu::TestStatus SimpleDraw::iterate (void)
{
	tcu::TestLog &log							= m_context.getTestContext().getLog();
	const vk::VkQueue queue						= m_context.getUniversalQueue();

	beginRenderPass();

	const vk::VkDeviceSize vertexBufferOffset	= 0;
	const vk::VkBuffer vertexBuffer				= m_vertexBuffer->object();

	m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
	m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

	switch (m_topology)
	{
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
			m_vk.cmdDraw(*m_cmdBuffer, 6, 1, 2, 0);
			break;
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
			m_vk.cmdDraw(*m_cmdBuffer, 4, 1, 2, 0);
			break;
		case vk::VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
		case vk::VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
		case vk::VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
		case vk::VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
		case vk::VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
		case vk::VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
		case vk::VK_PRIMITIVE_TOPOLOGY_LAST:
			DE_FATAL("Topology not implemented");
			break;
		default:
			DE_FATAL("Unknown topology");
			break;
	}

	m_vk.cmdEndRenderPass(*m_cmdBuffer);
	m_vk.endCommandBuffer(*m_cmdBuffer);

	vk::VkSubmitInfo submitInfo =
	{
		vk::VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType			sType;
		DE_NULL,							// const void*				pNext;
		0,										// deUint32					waitSemaphoreCount;
		DE_NULL,								// const VkSemaphore*		pWaitSemaphores;
		(const vk::VkPipelineStageFlags*)DE_NULL,
		1,										// deUint32					commandBufferCount;
		&m_cmdBuffer.get(),					// const VkCommandBuffer*	pCommandBuffers;
		0,										// deUint32					signalSemaphoreCount;
		DE_NULL								// const VkSemaphore*		pSignalSemaphores;
	};
	VK_CHECK(m_vk.queueSubmit(queue, 1, &submitInfo, DE_NULL));

	VK_CHECK(m_vk.queueWaitIdle(queue));

	// Validation
	tcu::Texture2D referenceFrame(vk::mapVkFormat(m_colorAttachmentFormat), (int)(0.5 + WIDTH), (int)(0.5 + HEIGHT));

	referenceFrame.allocLevel(0);

	const deInt32 frameWidth	= referenceFrame.getWidth();
	const deInt32 frameHeight	= referenceFrame.getHeight();

	tcu::clear(referenceFrame.getLevel(0), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

	ReferenceImageCoordinates refCoords;

	for (int y = 0; y < frameHeight; y++)
	{
		const float yCoord = (float)(y / (0.5*frameHeight)) - 1.0f;

		for (int x = 0; x < frameWidth; x++)
		{
			const float xCoord = (float)(x / (0.5*frameWidth)) - 1.0f;

			if ((yCoord >= refCoords.bottom	&&
				 yCoord <= refCoords.top	&&
				 xCoord >= refCoords.left	&&
				 xCoord <= refCoords.right))
				referenceFrame.getLevel(0).setPixel(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), x, y);
		}
	}

	const vk::VkOffset3D zeroOffset = { 0, 0, 0 };
	const tcu::ConstPixelBufferAccess renderedFrame = m_colorTargetImage->readSurface(queue, m_context.getDefaultAllocator(),
		vk::VK_IMAGE_LAYOUT_GENERAL, zeroOffset, WIDTH, HEIGHT, vk::VK_IMAGE_ASPECT_COLOR_BIT);

	qpTestResult res = QP_TEST_RESULT_PASS;

	if (!tcu::fuzzyCompare(log, "Result", "Image comparison result",
		referenceFrame.getLevel(0), renderedFrame, 0.05f,
		tcu::COMPARE_LOG_RESULT)) {
		res = QP_TEST_RESULT_FAIL;
	}

	return tcu::TestStatus(res, qpGetTestResultName(res));

}

SimpleDrawInstanced::SimpleDrawInstanced (Context &context, ShaderMap shaders, vk::VkPrimitiveTopology topology)
	: SimpleDraw	(context, shaders, topology) {}

tcu::TestStatus SimpleDrawInstanced::iterate (void)
{
	tcu::TestLog &log		= m_context.getTestContext().getLog();

	const vk::VkQueue queue = m_context.getUniversalQueue();

	beginRenderPass();

	const vk::VkDeviceSize vertexBufferOffset = 0;
	const vk::VkBuffer vertexBuffer = m_vertexBuffer->object();

	m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);

	m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

	switch (m_topology)
	{
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
			m_vk.cmdDraw(*m_cmdBuffer, 6, 4, 2, 2);
			break;
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
			m_vk.cmdDraw(*m_cmdBuffer, 4, 4, 2, 2);
			break;
		case vk::VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
		case vk::VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
		case vk::VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
		case vk::VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
		case vk::VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
		case vk::VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
		case vk::VK_PRIMITIVE_TOPOLOGY_LAST:
			DE_FATAL("Topology not implemented");
			break;
		default:
			DE_FATAL("Unknown topology");
			break;
	}

	m_vk.cmdEndRenderPass(*m_cmdBuffer);
	m_vk.endCommandBuffer(*m_cmdBuffer);

	vk::VkSubmitInfo submitInfo =
	{
		vk::VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType			sType;
		DE_NULL,							// const void*				pNext;
		0,										// deUint32					waitSemaphoreCount;
		DE_NULL,								// const VkSemaphore*		pWaitSemaphores;
		(const vk::VkPipelineStageFlags*)DE_NULL,
		1,										// deUint32					commandBufferCount;
		&m_cmdBuffer.get(),					// const VkCommandBuffer*	pCommandBuffers;
		0,										// deUint32					signalSemaphoreCount;
		DE_NULL								// const VkSemaphore*		pSignalSemaphores;
	};
	VK_CHECK(m_vk.queueSubmit(queue, 1, &submitInfo, DE_NULL));

	VK_CHECK(m_vk.queueWaitIdle(queue));

	// Validation
	VK_CHECK(m_vk.queueWaitIdle(queue));

	tcu::Texture2D referenceFrame(vk::mapVkFormat(m_colorAttachmentFormat), (int)(0.5 + WIDTH), (int)(0.5 + HEIGHT));

	referenceFrame.allocLevel(0);

	const deInt32 frameWidth	= referenceFrame.getWidth();
	const deInt32 frameHeight	= referenceFrame.getHeight();

	tcu::clear(referenceFrame.getLevel(0), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

	ReferenceImageInstancedCoordinates refInstancedCoords;

	for (int y = 0; y < frameHeight; y++)
	{
		const float yCoord = (float)(y / (0.5*frameHeight)) - 1.0f;

		for (int x = 0; x < frameWidth; x++)
		{
			const float xCoord = (float)(x / (0.5*frameWidth)) - 1.0f;

			if ((yCoord >= refInstancedCoords.bottom	&&
				 yCoord <= refInstancedCoords.top		&&
				 xCoord >= refInstancedCoords.left		&&
				 xCoord <= refInstancedCoords.right))
				referenceFrame.getLevel(0).setPixel(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), x, y);
		}
	}

	const vk::VkOffset3D zeroOffset = { 0, 0, 0 };
	const tcu::ConstPixelBufferAccess renderedFrame = m_colorTargetImage->readSurface(queue, m_context.getDefaultAllocator(),
		vk::VK_IMAGE_LAYOUT_GENERAL, zeroOffset, WIDTH, HEIGHT, vk::VK_IMAGE_ASPECT_COLOR_BIT);

	qpTestResult res = QP_TEST_RESULT_PASS;

	if (!tcu::fuzzyCompare(log, "Result", "Image comparison result",
		referenceFrame.getLevel(0), renderedFrame, 0.05f,
		tcu::COMPARE_LOG_RESULT)) {
		res = QP_TEST_RESULT_FAIL;
	}

	return tcu::TestStatus(res, qpGetTestResultName(res));
}

}	// anonymous

SimpleDrawTests::SimpleDrawTests (tcu::TestContext &testCtx)
: TestCaseGroup	(testCtx, "simple_draw", "drawing simple geometry")
{
	/* Left blank on purpose */
}

SimpleDrawTests::~SimpleDrawTests (void) {}


void SimpleDrawTests::init (void)
{
	ShaderMap shaderPaths;
	shaderPaths[glu::SHADERTYPE_VERTEX]		= "vulkan/draw/VertexFetch.vert";
	shaderPaths[glu::SHADERTYPE_FRAGMENT]	= "vulkan/draw/VertexFetch.frag";

	addChild(new InstanceFactory<SimpleDraw>(m_testCtx, "simple_draw_triangle_list", "Draws triangle list", shaderPaths, vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST));
	addChild(new InstanceFactory<SimpleDraw>(m_testCtx, "simple_draw_triangle_strip", "Draws triangle strip", shaderPaths, vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP));

	shaderPaths[glu::SHADERTYPE_VERTEX]		= "vulkan/draw/VertexFetchWithInstance.vert";
	shaderPaths[glu::SHADERTYPE_FRAGMENT]	= "vulkan/draw/VertexFetch.frag";

	addChild(new InstanceFactory<SimpleDrawInstanced>(m_testCtx, "simple_draw_instanced_triangle_list", "Draws an instanced triangle list", shaderPaths, vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST));
	addChild(new InstanceFactory<SimpleDrawInstanced>(m_testCtx, "simple_draw_instanced_triangle_strip", "Draws an instanced triangle strip", shaderPaths, vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP));
}

}	// DrawTests
}	// vkt
