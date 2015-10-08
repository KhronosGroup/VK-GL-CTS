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
 * \brief Dynamic State Viewport Tests
 *//*--------------------------------------------------------------------*/

#include "vktDynamicStateVPTests.hpp"
#include "vktDynamicStateBaseClass.hpp"

#include "vktDynamicStateTestCaseUtil.hpp"
#include "tcuTextureUtil.hpp"

namespace vkt
{
namespace DynamicState
{
namespace
{

class ViewportStateBaseCase : public DynamicStateBaseClass
{
public:
	ViewportStateBaseCase (Context &context, const char* vertexShaderName, const char* fragmentShaderName)
		: DynamicStateBaseClass (context, vertexShaderName, fragmentShaderName)
	{}

	void initialize(void)
	{
		m_topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

		m_data.push_back(Vec4RGBA(tcu::Vec4(-0.5f, 0.5f, 1.0f, 1.0f), vec4Green()));
		m_data.push_back(Vec4RGBA(tcu::Vec4(0.5f, 0.5f, 1.0f, 1.0f), vec4Green()));
		m_data.push_back(Vec4RGBA(tcu::Vec4(-0.5f, -0.5f, 1.0f, 1.0f), vec4Green()));
		m_data.push_back(Vec4RGBA(tcu::Vec4(0.5f, -0.5f, 1.0f, 1.0f), vec4Green()));

		DynamicStateBaseClass::initialize();
	}

	virtual tcu::Texture2D buildReferenceFrame (void)
	{
		TCU_FAIL("Implement buildReferenceFrame() method and return valid surface!");
	}

	virtual void setDynamicStates (void)
	{
		TCU_FAIL("Implement setDynamicStates() method!");
	}

	virtual tcu::TestStatus iterate (void)
	{
		tcu::TestLog &log = m_context.getTestContext().getLog();
		const vk::VkDevice device = m_context.getDevice();
		const vk::VkQueue queue = m_context.getUniversalQueue();

		beginRenderPass();

		// set states here
		setDynamicStates();

		m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

		const vk::VkDeviceSize vertexBufferOffset = 0;
		const vk::VkBuffer vertexBuffer = m_vertexBuffer->object();
		m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);

		m_vk.cmdDraw(*m_cmdBuffer, static_cast<deUint32>(m_data.size()), 1, 0, 0);

		m_vk.cmdEndRenderPass(*m_cmdBuffer);
		m_vk.endCommandBuffer(*m_cmdBuffer);

		const vk::VkCmdBuffer cmdBuffer = *m_cmdBuffer;
		VK_CHECK(m_vk.queueSubmit(queue, 1, &cmdBuffer, DE_NULL));

		// validation
		{
			VK_CHECK(m_vk.queueWaitIdle(queue));

			tcu::Texture2D referenceFrame = buildReferenceFrame();

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

class ViewportParamTestInstane : public ViewportStateBaseCase
{
public:
	ViewportParamTestInstane (Context &context, ShaderMap shaders)
		: ViewportStateBaseCase (context, shaders[glu::SHADERTYPE_VERTEX], shaders[glu::SHADERTYPE_FRAGMENT])
	{
		ViewportStateBaseCase::initialize();
	}

	virtual void setDynamicStates(void)
	{
		const vk::VkViewport viewport = { 0.0f, 0.0f, (float)WIDTH * 2, (float)HEIGHT * 2, 0.0f, 0.0f };
		const vk::VkRect2D scissor = { { 0, 0 }, { WIDTH, HEIGHT } };

		setDynamicViewportState(1, &viewport, &scissor);
		setDynamicRasterState();
		setDynamicBlendState();
		setDynamicDepthStencilState();
	}

	virtual tcu::Texture2D buildReferenceFrame (void)
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

				if (xCoord >= 0.0f && xCoord <= 1.0f && yCoord >= 0.0f && yCoord <= 1.0f)
					referenceFrame.getLevel(0).setPixel(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f), x, y);
			}
		}

		return referenceFrame;
	}
};

class ScissorParamTestInstance : public ViewportStateBaseCase
{
public:
	ScissorParamTestInstance (Context &context, ShaderMap shaders)
		: ViewportStateBaseCase (context, shaders[glu::SHADERTYPE_VERTEX], shaders[glu::SHADERTYPE_FRAGMENT])
	{
		ViewportStateBaseCase::initialize();
	}

	virtual void setDynamicStates (void)
	{
		const vk::VkViewport viewport = { 0.0f, 0.0f, (float)WIDTH, (float)HEIGHT, 0.0f, 0.0f };
		const vk::VkRect2D scissor = { { 0, 0 }, { WIDTH / 2, HEIGHT / 2 } };

		setDynamicViewportState(1, &viewport, &scissor);
		setDynamicRasterState();
		setDynamicBlendState();
		setDynamicDepthStencilState();
	}

	virtual tcu::Texture2D buildReferenceFrame (void)
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

				if (xCoord >= -0.5f && xCoord <= 0.0f && yCoord >= -0.5f && yCoord <= 0.0f)
					referenceFrame.getLevel(0).setPixel(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f), x, y);
			}
		}

		return referenceFrame;
	}
};


class ViewportArrayTestInstance : public DynamicStateBaseClass
{
protected:
	std::string m_geometryShaderName;

public:

	ViewportArrayTestInstance (Context &context, ShaderMap shaders)
		: DynamicStateBaseClass	(context, shaders[glu::SHADERTYPE_VERTEX], shaders[glu::SHADERTYPE_FRAGMENT])
		, m_geometryShaderName	(shaders[glu::SHADERTYPE_GEOMETRY])
	{
		for (int i = 0; i < 4; i++)
		{
			m_data.push_back(Vec4RGBA(tcu::Vec4(-1.0f, 1.0f, (float)i, 1.0f), vec4Green()));
			m_data.push_back(Vec4RGBA(tcu::Vec4(1.0f, 1.0f, (float)i, 1.0f), vec4Green()));
			m_data.push_back(Vec4RGBA(tcu::Vec4(-1.0f, -1.0f, (float)i, 1.0f), vec4Green()));
			m_data.push_back(Vec4RGBA(tcu::Vec4(1.0f, -1.0f, (float)i, 1.0f), vec4Green()));
		}

		DynamicStateBaseClass::initialize();
	}

	virtual void initPipeline (const vk::VkDevice device)
	{
		const vk::Unique<vk::VkShader> vs(createShader(m_vk, device,
			*createShaderModule(m_vk, device, m_context.getBinaryCollection().get(m_vertexShaderName), 0),
			"main", vk::VK_SHADER_STAGE_VERTEX));

		const vk::Unique<vk::VkShader> gs(createShader(m_vk, device,
			*createShaderModule(m_vk, device, m_context.getBinaryCollection().get(m_geometryShaderName), 0),
			"main", vk::VK_SHADER_STAGE_GEOMETRY));

		const vk::Unique<vk::VkShader> fs(createShader(m_vk, device,
			*createShaderModule(m_vk, device, m_context.getBinaryCollection().get(m_fragmentShaderName), 0),
			"main", vk::VK_SHADER_STAGE_FRAGMENT));

		const PipelineCreateInfo::ColorBlendState::Attachment vkCbAttachmentState;

		PipelineCreateInfo pipelineCreateInfo(*m_pipelineLayout, *m_renderPass, 0, 0);
		pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*vs, vk::VK_SHADER_STAGE_VERTEX));
		pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*gs, vk::VK_SHADER_STAGE_GEOMETRY));
		pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*fs, vk::VK_SHADER_STAGE_FRAGMENT));
		pipelineCreateInfo.addState(PipelineCreateInfo::VertexInputState(m_vertexInputState));
		pipelineCreateInfo.addState(PipelineCreateInfo::InputAssemblerState(m_topology));
		pipelineCreateInfo.addState(PipelineCreateInfo::ColorBlendState(1, &vkCbAttachmentState));
		pipelineCreateInfo.addState(PipelineCreateInfo::ViewportState(4));
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

		beginRenderPass();

		// set states here
		const float halfWidth = (float)WIDTH / 2;
		const float halfHeight = (float)HEIGHT / 2;
		const deInt32 quarterWidth = WIDTH / 4;
		const deInt32 quarterHeight = HEIGHT / 4;

		const vk::VkViewport viewports[4] =
		{
			{ 0.0f, 0.0f, (float)halfWidth, (float)halfHeight, 0.0f, 0.0f },
			{ halfWidth, 0.0f, (float)halfWidth, (float)halfHeight, 0.0f, 0.0f },
			{ halfWidth, halfHeight, (float)halfWidth, (float)halfHeight, 0.0f, 0.0f },
			{ 0.0f, halfHeight, (float)halfWidth, (float)halfHeight, 0.0f, 0.0f }
		};

		const vk::VkRect2D scissors[4] =
		{
			{ { quarterWidth, quarterHeight }, { quarterWidth, quarterHeight } },
			{ { halfWidth, quarterHeight }, { quarterWidth, quarterHeight } },
			{ { halfWidth, halfHeight }, { quarterWidth, quarterHeight } },
			{ { quarterWidth, halfHeight }, { quarterWidth, quarterHeight } },
		};

		setDynamicViewportState(4, viewports, scissors);
		setDynamicRasterState();
		setDynamicBlendState();
		setDynamicDepthStencilState();

		m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

		const vk::VkDeviceSize vertexBufferOffset = 0;
		const vk::VkBuffer vertexBuffer = m_vertexBuffer->object();
		m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);

		m_vk.cmdDraw(*m_cmdBuffer, static_cast<deUint32>(m_data.size()), 1, 0, 0);

		m_vk.cmdEndRenderPass(*m_cmdBuffer);
		m_vk.endCommandBuffer(*m_cmdBuffer);

		const vk::VkCmdBuffer cmdBuffer = *m_cmdBuffer;
		VK_CHECK(m_vk.queueSubmit(queue, 1, &cmdBuffer, DE_NULL));

		// validation
		{
			VK_CHECK(m_vk.queueWaitIdle(queue));

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

					if (xCoord >= -0.5f && xCoord <= 0.5f && yCoord >= -0.5f && yCoord <= 0.5f)
						referenceFrame.getLevel(0).setPixel(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f), x, y);
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

DynamicStateVPTests::DynamicStateVPTests (tcu::TestContext &testCtx)
	: TestCaseGroup (testCtx, "vp_state", "Tests for viewport state")
{
	/* Left blank on purpose */
}

DynamicStateVPTests::~DynamicStateVPTests () {}

void DynamicStateVPTests::init (void)
{
	ShaderMap shaderPaths;
	shaderPaths[glu::SHADERTYPE_VERTEX] = "vulkan/dynamic_state/VertexFetch.vert";
	shaderPaths[glu::SHADERTYPE_FRAGMENT] = "vulkan/dynamic_state/VertexFetch.frag";
		
	addChild(new InstanceFactory<ViewportParamTestInstane>(m_testCtx, "viewport", "Set viewport which is twice bigger than screen size", shaderPaths));
	addChild(new InstanceFactory<ScissorParamTestInstance>(m_testCtx, "scissor", "Perform a scissor test on 1/4 bottom-left part of the surface", shaderPaths));

	shaderPaths[glu::SHADERTYPE_GEOMETRY] = "vulkan/dynamic_state/ViewportArray.geom";
	addChild(new InstanceFactory<ViewportArrayTestInstance>(m_testCtx, "viewport_array", "Multiple viewports and scissors", shaderPaths));
}

} //DynamicState
} //vkt
