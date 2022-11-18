/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Intel Corporation
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
#include "vkCmdUtil.hpp"

namespace vkt
{
namespace Draw
{
namespace
{
class SimpleDraw : public DrawTestsBaseClass
{
public:
	typedef TestSpecBase	TestSpec;
							SimpleDraw				(Context &context, TestSpec testSpec);
	virtual tcu::TestStatus iterate					(void);
	void					draw					(vk::VkCommandBuffer cmdBuffer, deUint32 instanceCount = 1u, deUint32 firstInstance = 0u);
};

class SimpleDrawInstanced : public SimpleDraw
{
public:
	typedef TestSpec		TestSpec;
							SimpleDrawInstanced		(Context &context, TestSpec testSpec);
	tcu::TestStatus			iterate					(void);
};

SimpleDraw::SimpleDraw (Context &context, TestSpec testSpec)
	: DrawTestsBaseClass(context, testSpec.shaders[glu::SHADERTYPE_VERTEX], testSpec.shaders[glu::SHADERTYPE_FRAGMENT], testSpec.groupParams, testSpec.topology)
{
	m_data.push_back(VertexElementData(tcu::Vec4(1.0f, -1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), -1));
	m_data.push_back(VertexElementData(tcu::Vec4(-1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), -1));

	int refVertexIndex = 2;

	switch (m_topology)
	{
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
			m_data.push_back(VertexElementData(tcu::Vec4(-0.3f,	-0.3f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), refVertexIndex++));
			m_data.push_back(VertexElementData(tcu::Vec4(-0.3f,	 0.3f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), refVertexIndex++));
			m_data.push_back(VertexElementData(tcu::Vec4( 0.3f,	-0.3f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), refVertexIndex++));
			m_data.push_back(VertexElementData(tcu::Vec4( 0.3f,	-0.3f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), refVertexIndex++));
			m_data.push_back(VertexElementData(tcu::Vec4( 0.3f,	 0.3f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), refVertexIndex++));
			m_data.push_back(VertexElementData(tcu::Vec4(-0.3f,	 0.3f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), refVertexIndex++));
			break;
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
			m_data.push_back(VertexElementData(tcu::Vec4(-0.3f,	-0.3f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), refVertexIndex++));
			m_data.push_back(VertexElementData(tcu::Vec4(-0.3f,	 0.3f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), refVertexIndex++));
			m_data.push_back(VertexElementData(tcu::Vec4( 0.3f,	-0.3f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), refVertexIndex++));
			m_data.push_back(VertexElementData(tcu::Vec4( 0.3f,	 0.3f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), refVertexIndex++));
			m_data.push_back(VertexElementData(tcu::Vec4(-0.3f,	 0.3f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), refVertexIndex++));
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

	m_data.push_back(VertexElementData(tcu::Vec4(-1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), -1));

	initialize();
}

tcu::TestStatus SimpleDraw::iterate (void)
{
	tcu::TestLog&			log					= m_context.getTestContext().getLog();
	const vk::VkQueue		queue				= m_context.getUniversalQueue();
	const vk::VkDevice		device				= m_context.getDevice();
	const vk::VkDeviceSize	vertexBufferOffset	= 0;
	const vk::VkBuffer		vertexBuffer		= m_vertexBuffer->object();

#ifndef CTS_USES_VULKANSC
	if (m_groupParams->useSecondaryCmdBuffer)
	{
		// record secondary command buffer
		if (m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
		{
			beginSecondaryCmdBuffer(m_vk, vk::VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT);
			beginDynamicRender(*m_secCmdBuffer);
		}
		else
			beginSecondaryCmdBuffer(m_vk);

		m_vk.cmdBindVertexBuffers(*m_secCmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
		m_vk.cmdBindPipeline(*m_secCmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		draw(*m_secCmdBuffer);

		if (m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			endDynamicRender(*m_secCmdBuffer);

		endCommandBuffer(m_vk, *m_secCmdBuffer);

		// record primary command buffer
		beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);
		preRenderBarriers();

		if (!m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			beginDynamicRender(*m_cmdBuffer, vk::VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

		m_vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*m_secCmdBuffer);

		if (!m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			endDynamicRender(*m_cmdBuffer);

		endCommandBuffer(m_vk, *m_cmdBuffer);
	}
	else if (m_groupParams->useDynamicRendering)
	{
		beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);
		preRenderBarriers();
		beginDynamicRender(*m_cmdBuffer);

		m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
		m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		draw(*m_cmdBuffer);

		endDynamicRender(*m_cmdBuffer);
		endCommandBuffer(m_vk, *m_cmdBuffer);
	}
#endif // CTS_USES_VULKANSC

	if (!m_groupParams->useDynamicRendering)
	{
		beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);
		preRenderBarriers();
		beginLegacyRender(*m_cmdBuffer);

		m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
		m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		draw(*m_cmdBuffer);

		endLegacyRender(*m_cmdBuffer);
		endCommandBuffer(m_vk, *m_cmdBuffer);
	}

	submitCommandsAndWait(m_vk, device, queue, m_cmdBuffer.get());

	// Validation
	tcu::Texture2D referenceFrame(vk::mapVkFormat(m_colorAttachmentFormat), (int)(0.5f + static_cast<float>(WIDTH)), (int)(0.5f + static_cast<float>(HEIGHT)));

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

void SimpleDraw::draw(vk::VkCommandBuffer cmdBuffer, deUint32 instanceCount, deUint32 firstInstance)
{
	switch (m_topology)
	{
	case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
		m_vk.cmdDraw(cmdBuffer, 6, instanceCount, 2, firstInstance);
		break;
	case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
		m_vk.cmdDraw(cmdBuffer, 4, instanceCount, 2, firstInstance);
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
}

SimpleDrawInstanced::SimpleDrawInstanced (Context &context, TestSpec testSpec)
	: SimpleDraw	(context, testSpec) {}

tcu::TestStatus SimpleDrawInstanced::iterate (void)
{
	tcu::TestLog&			log						= m_context.getTestContext().getLog();
	const vk::VkQueue		queue					= m_context.getUniversalQueue();
	const vk::VkDevice		device					= m_context.getDevice();
	const vk::VkDeviceSize	vertexBufferOffset		= 0;
	const vk::VkBuffer		vertexBuffer			= m_vertexBuffer->object();

#ifndef CTS_USES_VULKANSC
	if (m_groupParams->useSecondaryCmdBuffer)
	{
		// record secondary command buffer
		if (m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
		{
			beginSecondaryCmdBuffer(m_vk, vk::VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT);
			beginDynamicRender(*m_secCmdBuffer);
		}
		else
			beginSecondaryCmdBuffer(m_vk);

		m_vk.cmdBindVertexBuffers(*m_secCmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
		m_vk.cmdBindPipeline(*m_secCmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		draw(*m_secCmdBuffer, 4u, 2u);

		if (m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			endDynamicRender(*m_secCmdBuffer);

		endCommandBuffer(m_vk, *m_secCmdBuffer);

		// record primary command buffer
		beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);
		preRenderBarriers();

		if (!m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			beginDynamicRender(*m_cmdBuffer, vk::VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

		m_vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*m_secCmdBuffer);

		if (!m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			endDynamicRender(*m_cmdBuffer);

		endCommandBuffer(m_vk, *m_cmdBuffer);
	}
	else if (m_groupParams->useDynamicRendering)
	{
		beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);
		preRenderBarriers();

		beginDynamicRender(*m_cmdBuffer);
		m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
		m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		draw(*m_cmdBuffer, 4u, 2u);
		endDynamicRender(*m_cmdBuffer);

		endCommandBuffer(m_vk, *m_cmdBuffer);
	}
#endif // CTS_USES_VULKANSC

	if (!m_groupParams->useDynamicRendering)
	{
		beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);
		preRenderBarriers();

		beginLegacyRender(*m_cmdBuffer);
		m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
		m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		draw(*m_cmdBuffer, 4u, 2u);
		endLegacyRender(*m_cmdBuffer);

		endCommandBuffer(m_vk, *m_cmdBuffer);
	}

	submitCommandsAndWait(m_vk, device, queue, m_cmdBuffer.get());

	// Validation
	VK_CHECK(m_vk.queueWaitIdle(queue));

	tcu::Texture2D referenceFrame(vk::mapVkFormat(m_colorAttachmentFormat), (int)(0.5f + static_cast<float>(WIDTH)), (int)(0.5f + static_cast<float>(HEIGHT)));

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

void checkSupport(Context& context, SimpleDraw::TestSpec testSpec)
{
	if (testSpec.groupParams->useDynamicRendering)
		context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");
}

}	// anonymous

SimpleDrawTests::SimpleDrawTests (tcu::TestContext &testCtx, const SharedGroupParams groupParams)
	: TestCaseGroup			(testCtx, "simple_draw", "drawing simple geometry")
	, m_groupParams			(groupParams)
{
	/* Left blank on purpose */
}

SimpleDrawTests::~SimpleDrawTests (void) {}

void SimpleDrawTests::init (void)
{
	{
		SimpleDraw::TestSpec testSpec
		{
			{													// ShaderMap					shaders;
				{ glu::SHADERTYPE_VERTEX, "vulkan/draw/VertexFetch.vert" },
				{ glu::SHADERTYPE_FRAGMENT, "vulkan/draw/VertexFetch.frag" }
			},
			vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,			// vk::VkPrimitiveTopology		topology;
			m_groupParams										// const SharedGroupParams		groupParams;
		};

		addChild(new InstanceFactory<SimpleDraw, FunctionSupport1<SimpleDraw::TestSpec> >
			(m_testCtx, "simple_draw_triangle_list", "Draws triangle list", testSpec, FunctionSupport1<SimpleDraw::TestSpec>::Args(checkSupport, testSpec)));
		testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		addChild(new InstanceFactory<SimpleDraw, FunctionSupport1<SimpleDraw::TestSpec> >
			(m_testCtx, "simple_draw_triangle_strip", "Draws triangle strip", testSpec, FunctionSupport1<SimpleDraw::TestSpec>::Args(checkSupport, testSpec)));
	}
	{
		SimpleDrawInstanced::TestSpec testSpec
		{
			{
				{ glu::SHADERTYPE_VERTEX, "vulkan/draw/VertexFetchInstancedFirstInstance.vert" },
				{ glu::SHADERTYPE_FRAGMENT, "vulkan/draw/VertexFetch.frag" }
			},
			vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			m_groupParams
		};

		addChild(new InstanceFactory<SimpleDrawInstanced, FunctionSupport1<SimpleDrawInstanced::TestSpec> >
			(m_testCtx, "simple_draw_instanced_triangle_list", "Draws an instanced triangle list", testSpec, FunctionSupport1<SimpleDrawInstanced::TestSpec>::Args(checkSupport, testSpec)));
		testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		addChild(new InstanceFactory<SimpleDrawInstanced, FunctionSupport1<SimpleDrawInstanced::TestSpec> >
			(m_testCtx, "simple_draw_instanced_triangle_strip", "Draws an instanced triangle strip", testSpec, FunctionSupport1<SimpleDrawInstanced::TestSpec>::Args(checkSupport, testSpec)));
	}
}

}	// DrawTests
}	// vkt
