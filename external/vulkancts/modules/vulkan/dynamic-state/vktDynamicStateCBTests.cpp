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
 * \brief Dynamic CB State Tests 
 *//*--------------------------------------------------------------------*/

#include "vktDynamicStateCBTests.hpp"
#include "vktDynamicStateBaseClass.hpp"

#include "vktTestCaseUtil.hpp"
#include "tcuTextureUtil.hpp"

#include "vktDynamicStateTestCaseUtil.hpp"

namespace vkt
{
namespace DynamicState
{
namespace
{

class BlendConstantsTestInstance : public DynamicStateBaseClass
{
public:
	BlendConstantsTestInstance (Context &context, ShaderMap shaders)
		: DynamicStateBaseClass (context, shaders[glu::SHADERTYPE_VERTEX], shaders[glu::SHADERTYPE_FRAGMENT])
	{
		m_topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

		m_data.push_back(Vec4RGBA(tcu::Vec4(-1.0f, 1.0f, 1.0f, 1.0f), vec4Green()));
		m_data.push_back(Vec4RGBA(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f), vec4Green()));
		m_data.push_back(Vec4RGBA(tcu::Vec4(-1.0f, -1.0f, 1.0f, 1.0f), vec4Green()));
		m_data.push_back(Vec4RGBA(tcu::Vec4(1.0f, -1.0f, 1.0f, 1.0f), vec4Green()));

		DynamicStateBaseClass::initialize();
	}

	virtual void initPipeline (const vk::VkDevice device)
	{
		const vk::Unique<vk::VkShader> vs(createShader(m_vk, device,
			*createShaderModule(m_vk, device, m_context.getBinaryCollection().get(m_vertexShaderName), 0),
			"main", vk::VK_SHADER_STAGE_VERTEX));

		const vk::Unique<vk::VkShader> fs(createShader(m_vk, device,
			*createShaderModule(m_vk, device, m_context.getBinaryCollection().get(m_fragmentShaderName), 0),
			"main", vk::VK_SHADER_STAGE_FRAGMENT));

		const vk::VkPipelineColorBlendAttachmentState VkPipelineColorBlendAttachmentState =
			PipelineCreateInfo::ColorBlendState::Attachment(
			vk::VK_TRUE,
			vk::VK_BLEND_SRC_ALPHA, vk::VK_BLEND_CONSTANT_COLOR, vk::VK_BLEND_OP_ADD,
			vk::VK_BLEND_SRC_ALPHA, vk::VK_BLEND_CONSTANT_ALPHA, vk::VK_BLEND_OP_ADD);

		PipelineCreateInfo pipelineCreateInfo(*m_pipelineLayout, *m_renderPass, 0, 0);
		pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*vs, vk::VK_SHADER_STAGE_VERTEX));
		pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*fs, vk::VK_SHADER_STAGE_FRAGMENT));
		pipelineCreateInfo.addState(PipelineCreateInfo::VertexInputState(m_vertexInputState));
		pipelineCreateInfo.addState(PipelineCreateInfo::InputAssemblerState(m_topology));
		pipelineCreateInfo.addState(PipelineCreateInfo::ColorBlendState(1, &VkPipelineColorBlendAttachmentState));
		pipelineCreateInfo.addState(PipelineCreateInfo::ViewportState(1));
		pipelineCreateInfo.addState(PipelineCreateInfo::DepthStencilState());
		pipelineCreateInfo.addState(PipelineCreateInfo::RasterizerState());
		pipelineCreateInfo.addState(PipelineCreateInfo::MultiSampleState());
		pipelineCreateInfo.addState(PipelineCreateInfo::DynamicState());

		m_pipeline = vk::createGraphicsPipeline(m_vk, device, DE_NULL, &pipelineCreateInfo);
	}

	virtual tcu::TestStatus iterate (void)
	{
		tcu::TestLog &log = m_context.getTestContext().getLog();
		const vk::VkQueue queue = m_context.getUniversalQueue();

		const vk::VkClearColorValue clearColor = { { 1.0f, 1.0f, 1.0f, 1.0f } };
		beginRenderPassWithClearColor(clearColor);

		// bind states here
		setDynamicViewportState(WIDTH, HEIGHT);
		setDynamicRasterState();
		setDynamicDepthStencilState();
		setDynamicBlendState(0.33f, 0.1f, 0.66f, 0.5f);

		m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

		const vk::VkDeviceSize vertexBufferOffset = 0;
		const vk::VkBuffer vertexBuffer = m_vertexBuffer->object();
		m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);

		m_vk.cmdDraw(*m_cmdBuffer, static_cast<deUint32>(m_data.size()), 1, 0, 0);

		m_vk.cmdEndRenderPass(*m_cmdBuffer);
		m_vk.endCommandBuffer(*m_cmdBuffer);

		const vk::VkCmdBuffer cmdBuffer = *m_cmdBuffer;
		VK_CHECK(m_vk.queueSubmit(queue, 1, &cmdBuffer, DE_NULL));
		VK_CHECK(m_vk.queueWaitIdle(queue));

		//validation
		{
			tcu::Texture2D referenceFrame(vk::mapVkFormat(m_colorAttachmentFormat), (int)(0.5 + WIDTH), (int)(0.5 + HEIGHT));
			referenceFrame.allocLevel(0);

			const deInt32 frameWidth = referenceFrame.getWidth();
			const deInt32 frameHeight = referenceFrame.getHeight();

			tcu::clear(referenceFrame.getLevel(0), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

			for (int y = 0; y < frameHeight; y++)
			{
				const float yCoord = (float)(y / (0.5*frameHeight)) - 1.0f;

				for (int x = 0; x < frameWidth; x++)
				{
					const float xCoord = (float)(x / (0.5*frameWidth)) - 1.0f;

					if ((yCoord >= -1.0f && yCoord <= 1.0f && xCoord >= -1.0f && xCoord <= 1.0f))
						referenceFrame.getLevel(0).setPixel(tcu::Vec4(0.33f, 1.0f, 0.66f, 1.0f), x, y);
				}
			}

			const vk::VkOffset3D zeroOffset = { 0, 0, 0 };
			const tcu::ConstPixelBufferAccess renderedFrame = m_colorTargetImage->readSurface(queue, m_context.getDefaultAllocator(),
				vk::VK_IMAGE_LAYOUT_GENERAL, zeroOffset, WIDTH, HEIGHT, vk::VK_IMAGE_ASPECT_COLOR);

			qpTestResult res = QP_TEST_RESULT_PASS;

			if (!tcu::fuzzyCompare(log, "Result", "Image comparison result",
				referenceFrame.getLevel(0), renderedFrame, 0.05f,
				tcu::COMPARE_LOG_RESULT))
			{
				res = QP_TEST_RESULT_FAIL;
			}

			return tcu::TestStatus(res, qpGetTestResultName(res));
		}
	}
};

} //anonymous

DynamicStateCBTests::DynamicStateCBTests (tcu::TestContext &testCtx)
	: TestCaseGroup (testCtx, "cb_state", "Tests for color blend state")
{
	/* Left blank on purpose */
}

DynamicStateCBTests::~DynamicStateCBTests (void) {}

void DynamicStateCBTests::init (void)
{
	ShaderMap shaderPaths;
	shaderPaths[glu::SHADERTYPE_VERTEX] = "vulkan/dynamic_state/VertexFetch.vert";
	shaderPaths[glu::SHADERTYPE_FRAGMENT] = "vulkan/dynamic_state/VertexFetch.frag";
	addChild(new InstanceFactory<BlendConstantsTestInstance>(m_testCtx, "blend_constants", "Check if blend constants are working properly", shaderPaths));
}

} //DynamicState
} //vkt
