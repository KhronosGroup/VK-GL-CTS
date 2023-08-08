/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Intel Corporation
 * Copyright (c) 2023 LunarG, Inc.
 * Copyright (c) 2023 Nintendo
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
 * \brief Dynamic State Tests - General
 *//*--------------------------------------------------------------------*/

#include "vktDynamicStateGeneralTests.hpp"

#include "vktTestCaseUtil.hpp"
#include "vktDynamicStateTestCaseUtil.hpp"
#include "vktDynamicStateBaseClass.hpp"
#include "vktDrawCreateInfoUtil.hpp"
#include "vktDrawImageObjectUtil.hpp"
#include "vktDrawBufferObjectUtil.hpp"

#include "vkImageUtil.hpp"
#include "vkCmdUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuResource.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuRGBA.hpp"

#include "vkDefs.hpp"
#include "vkCmdUtil.hpp"

namespace vkt
{
namespace DynamicState
{

using namespace Draw;

namespace
{

class StateSwitchTestInstance : public DynamicStateBaseClass
{
public:
	StateSwitchTestInstance (Context &context, vk::PipelineConstructionType pipelineConstructionType, const ShaderMap& shaders)
		: DynamicStateBaseClass (context, pipelineConstructionType, shaders.at(glu::SHADERTYPE_VERTEX), shaders.at(glu::SHADERTYPE_FRAGMENT), shaders.at(glu::SHADERTYPE_MESH))
	{
		m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, -1.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, -1.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));

		DynamicStateBaseClass::initialize();
	}

	virtual tcu::TestStatus iterate (void)
	{
		tcu::TestLog&		log		= m_context.getTestContext().getLog();
		const vk::VkQueue	queue	= m_context.getUniversalQueue();
		const vk::VkDevice	device	= m_context.getDevice();

		beginRenderPass();

		// bind states here
		vk::VkViewport viewport = { 0, 0, (float)WIDTH, (float)HEIGHT, 0.0f, 0.0f };
		vk::VkRect2D scissor_1	= { { 0, 0 }, { WIDTH / 2, HEIGHT / 2 } };
		vk::VkRect2D scissor_2	= { { WIDTH / 2, HEIGHT / 2 }, { WIDTH / 2, HEIGHT / 2 } };

		setDynamicRasterizationState();
		setDynamicBlendState();
		setDynamicDepthStencilState();

		m_pipeline.bind(*m_cmdBuffer);

#ifndef CTS_USES_VULKANSC
		if (m_isMesh)
		{
			const auto numVert = static_cast<uint32_t>(m_data.size());
			DE_ASSERT(numVert >= 2u);

			m_vk.cmdBindDescriptorSets(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout.get(), 0u, 1u, &m_descriptorSet.get(), 0u, nullptr);
			pushVertexOffset(0u, *m_pipelineLayout);

			// bind first state
			setDynamicViewportState(1, &viewport, &scissor_1);
			m_vk.cmdDrawMeshTasksEXT(*m_cmdBuffer, numVert - 2u, 1u, 1u);

			// bind second state
			setDynamicViewportState(1, &viewport, &scissor_2);
			m_vk.cmdDrawMeshTasksEXT(*m_cmdBuffer, numVert - 2u, 1u, 1u);
		}
		else
#endif // CTS_USES_VULKANSC
		{
			const vk::VkDeviceSize vertexBufferOffset	= 0;
			const vk::VkBuffer vertexBuffer				= m_vertexBuffer->object();
			m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);

			// bind first state
			setDynamicViewportState(1, &viewport, &scissor_1);
			m_vk.cmdDraw(*m_cmdBuffer, static_cast<deUint32>(m_data.size()), 1, 0, 0);

			// bind second state
			setDynamicViewportState(1, &viewport, &scissor_2);
			m_vk.cmdDraw(*m_cmdBuffer, static_cast<deUint32>(m_data.size()), 1, 0, 0);
		}

		m_renderPass.end(m_vk, *m_cmdBuffer);
		endCommandBuffer(m_vk, *m_cmdBuffer);

		submitCommandsAndWait(m_vk, device, queue, m_cmdBuffer.get());

		//validation
		tcu::Texture2D referenceFrame(vk::mapVkFormat(m_colorAttachmentFormat), (int)(0.5f + static_cast<float>(WIDTH)), (int)(0.5f + static_cast<float>(HEIGHT)));
		referenceFrame.allocLevel(0);

		const deInt32 frameWidth	= referenceFrame.getWidth();
		const deInt32 frameHeight	= referenceFrame.getHeight();

		tcu::clear(referenceFrame.getLevel(0), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

		for (int y = 0; y < frameHeight; y++)
		{
			const float yCoord = (float)(y / (0.5*frameHeight)) - 1.0f;

			for (int x = 0; x < frameWidth; x++)
			{
				const float xCoord = (float)(x / (0.5*frameWidth)) - 1.0f;

				if ((yCoord >= -1.0f && yCoord <= 0.0f && xCoord >= -1.0f && xCoord <= 0.0f) ||
					(yCoord > 0.0f && yCoord <= 1.0f && xCoord > 0.0f && xCoord < 1.0f))
					referenceFrame.getLevel(0).setPixel(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f), x, y);
			}
		}

		const vk::VkOffset3D zeroOffset					= { 0, 0, 0 };
		const tcu::ConstPixelBufferAccess renderedFrame = m_colorTargetImage->readSurface(queue, m_context.getDefaultAllocator(),
																						  vk::VK_IMAGE_LAYOUT_GENERAL, zeroOffset, WIDTH, HEIGHT,
																						  vk::VK_IMAGE_ASPECT_COLOR_BIT);

		if (!tcu::fuzzyCompare(log, "Result", "Image comparison result",
			referenceFrame.getLevel(0), renderedFrame, 0.05f,
			tcu::COMPARE_LOG_RESULT))
		{

			return tcu::TestStatus(QP_TEST_RESULT_FAIL, "Image verification failed");
		}

		return tcu::TestStatus(QP_TEST_RESULT_PASS, "Image verification passed");
	}
};

class BindOrderTestInstance : public DynamicStateBaseClass
{
public:
	BindOrderTestInstance (Context& context, vk::PipelineConstructionType pipelineConstructionType, const ShaderMap& shaders)
		: DynamicStateBaseClass (context, pipelineConstructionType, shaders.at(glu::SHADERTYPE_VERTEX), shaders.at(glu::SHADERTYPE_FRAGMENT), shaders.at(glu::SHADERTYPE_MESH))
	{
		m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, -1.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, -1.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));

		DynamicStateBaseClass::initialize();
	}

	virtual tcu::TestStatus iterate (void)
	{
		tcu::TestLog		&log	= m_context.getTestContext().getLog();
		const vk::VkQueue	queue	= m_context.getUniversalQueue();
		const vk::VkDevice	device	= m_context.getDevice();

		beginRenderPass();

		// bind states here
		vk::VkViewport viewport = { 0.0f, 0.0f, (float)WIDTH, (float)HEIGHT, 0.0f, 0.0f };
		vk::VkRect2D scissor_1	= { { 0, 0 }, { WIDTH / 2, HEIGHT / 2 } };
		vk::VkRect2D scissor_2	= { { WIDTH / 2, HEIGHT / 2 }, { WIDTH / 2, HEIGHT / 2 } };

		setDynamicRasterizationState();
		setDynamicBlendState();
		setDynamicDepthStencilState();
		setDynamicViewportState(1, &viewport, &scissor_1);

		m_pipeline.bind(*m_cmdBuffer);

#ifndef CTS_USES_VULKANSC
		if (m_isMesh)
		{
			const auto numVert = static_cast<uint32_t>(m_data.size());
			DE_ASSERT(numVert >= 2u);

			m_vk.cmdBindDescriptorSets(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout.get(), 0u, 1u, &m_descriptorSet.get(), 0u, nullptr);
			pushVertexOffset(0u, *m_pipelineLayout);

			// rebind in different order
			setDynamicBlendState();
			setDynamicRasterizationState();
			setDynamicDepthStencilState();

			// bind first state
			setDynamicViewportState(1, &viewport, &scissor_1);
			m_vk.cmdDrawMeshTasksEXT(*m_cmdBuffer, numVert - 2u, 1u, 1u);

			setDynamicViewportState(1, &viewport, &scissor_2);
			m_vk.cmdDrawMeshTasksEXT(*m_cmdBuffer, numVert - 2u, 1u, 1u);
		}
		else
#endif // CTS_USES_VULKANSC
		{
			const vk::VkDeviceSize vertexBufferOffset = 0;
			const vk::VkBuffer vertexBuffer = m_vertexBuffer->object();
			m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);

			// rebind in different order
			setDynamicBlendState();
			setDynamicRasterizationState();
			setDynamicDepthStencilState();

			// bind first state
			setDynamicViewportState(1, &viewport, &scissor_1);
			m_vk.cmdDraw(*m_cmdBuffer, static_cast<deUint32>(m_data.size()), 1, 0, 0);

			setDynamicViewportState(1, &viewport, &scissor_2);
			m_vk.cmdDraw(*m_cmdBuffer, static_cast<deUint32>(m_data.size()), 1, 0, 0);
		}

		m_renderPass.end(m_vk, *m_cmdBuffer);
		endCommandBuffer(m_vk, *m_cmdBuffer);

		submitCommandsAndWait(m_vk, device, queue, m_cmdBuffer.get());

		//validation
		tcu::Texture2D referenceFrame(vk::mapVkFormat(m_colorAttachmentFormat), (int)(0.5f + static_cast<float>(WIDTH)), (int)(0.5f + static_cast<float>(HEIGHT)));
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

				if ((yCoord >= -1.0f && yCoord <= 0.0f && xCoord >= -1.0f && xCoord <= 0.0f) ||
					(yCoord > 0.0f && yCoord <= 1.0f && xCoord > 0.0f && xCoord < 1.0f))
					referenceFrame.getLevel(0).setPixel(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f), x, y);
			}
		}

		const vk::VkOffset3D zeroOffset = { 0, 0, 0 };
		const tcu::ConstPixelBufferAccess renderedFrame = m_colorTargetImage->readSurface(queue, m_context.getDefaultAllocator(),
			vk::VK_IMAGE_LAYOUT_GENERAL, zeroOffset, WIDTH, HEIGHT, vk::VK_IMAGE_ASPECT_COLOR_BIT);

		if (!tcu::fuzzyCompare(log, "Result", "Image comparison result",
			referenceFrame.getLevel(0), renderedFrame, 0.05f,
			tcu::COMPARE_LOG_RESULT))
		{
			return tcu::TestStatus(QP_TEST_RESULT_FAIL, "Image verification failed");
		}

		return tcu::TestStatus(QP_TEST_RESULT_PASS, "Image verification passed");
	}
};

class StatePersistenceTestInstance : public DynamicStateBaseClass
{
protected:
	vk::GraphicsPipelineWrapper	m_pipelineAdditional;

public:
	StatePersistenceTestInstance (Context& context, vk::PipelineConstructionType pipelineConstructionType, const ShaderMap& shaders)
		: DynamicStateBaseClass (context, pipelineConstructionType, shaders.at(glu::SHADERTYPE_VERTEX), shaders.at(glu::SHADERTYPE_FRAGMENT), shaders.at(glu::SHADERTYPE_MESH))
		, m_pipelineAdditional	(context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(), context.getDevice(), context.getDeviceExtensions(), pipelineConstructionType)
	{
		// This test does not make sense for mesh shader variants.
		DE_ASSERT(!m_isMesh);

		m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, -1.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, -1.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));

		m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, -1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, -1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, -1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));

		DynamicStateBaseClass::initialize();
	}
	virtual void initPipeline (const vk::VkDevice device)
	{
		const vk::ShaderWrapper					vs			(vk::ShaderWrapper(m_vk, device, m_context.getBinaryCollection().get(m_vertexShaderName), 0));
		const vk::ShaderWrapper					fs			(vk::ShaderWrapper(m_vk, device, m_context.getBinaryCollection().get(m_fragmentShaderName), 0));
		std::vector<vk::VkViewport>				viewports	{ { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f } };
		std::vector<vk::VkRect2D>				scissors	{ { { 0u, 0u }, { 0u, 0u } } };

		const PipelineCreateInfo::ColorBlendState::Attachment	attachmentState;
		const PipelineCreateInfo::ColorBlendState				colorBlendState(1, static_cast<const vk::VkPipelineColorBlendAttachmentState*>(&attachmentState));
		const PipelineCreateInfo::RasterizerState				rasterizerState;
		const PipelineCreateInfo::DepthStencilState				depthStencilState;
		const PipelineCreateInfo::DynamicState					dynamicState;

		m_pipeline.setDefaultTopology(vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
				  .setDynamicState(static_cast<const vk::VkPipelineDynamicStateCreateInfo*>(&dynamicState))
				  .setDefaultMultisampleState()
				  .setupVertexInputState(&m_vertexInputState)
				  .setupPreRasterizationShaderState(viewports,
													scissors,
													m_pipelineLayout,
													*m_renderPass,
													0u,
													vs,
													static_cast<const vk::VkPipelineRasterizationStateCreateInfo*>(&rasterizerState))
				  .setupFragmentShaderState(m_pipelineLayout, *m_renderPass, 0u, fs, static_cast<const vk::VkPipelineDepthStencilStateCreateInfo*>(&depthStencilState))
				  .setupFragmentOutputState(*m_renderPass, 0u, static_cast<const vk::VkPipelineColorBlendStateCreateInfo*>(&colorBlendState))
				  .setMonolithicPipelineLayout(m_pipelineLayout)
				  .buildPipeline();

		m_pipelineAdditional.setDefaultTopology(vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
				  .setDynamicState(static_cast<const vk::VkPipelineDynamicStateCreateInfo*>(&dynamicState))
				  .setDefaultMultisampleState()
				  .setupVertexInputState(&m_vertexInputState)
				  .setupPreRasterizationShaderState(viewports,
													scissors,
													m_pipelineLayout,
													*m_renderPass,
													0u,
													vs,
													static_cast<const vk::VkPipelineRasterizationStateCreateInfo*>(&rasterizerState))
				  .setupFragmentShaderState(m_pipelineLayout, *m_renderPass, 0u, fs, static_cast<const vk::VkPipelineDepthStencilStateCreateInfo*>(&depthStencilState))
				  .setupFragmentOutputState(*m_renderPass, 0u, static_cast<const vk::VkPipelineColorBlendStateCreateInfo*>(&colorBlendState))
				  .setMonolithicPipelineLayout(m_pipelineLayout)
				  .buildPipeline();
	}

	virtual tcu::TestStatus iterate(void)
	{
		tcu::TestLog&		log			= m_context.getTestContext().getLog();
		const vk::VkQueue	queue		= m_context.getUniversalQueue();
		const vk::VkDevice	device		= m_context.getDevice();

		beginRenderPass();

		// bind states here
		const vk::VkViewport viewport	= { 0.0f, 0.0f, (float)WIDTH, (float)HEIGHT, 0.0f, 0.0f };
		const vk::VkRect2D scissor_1	= { { 0, 0 }, { WIDTH / 2, HEIGHT / 2 } };
		const vk::VkRect2D scissor_2	= { { WIDTH / 2, HEIGHT / 2 }, { WIDTH / 2, HEIGHT / 2 } };

		setDynamicRasterizationState();
		setDynamicBlendState();
		setDynamicDepthStencilState();

		m_pipeline.bind(*m_cmdBuffer);

		const vk::VkDeviceSize vertexBufferOffset = 0;
		const vk::VkBuffer vertexBuffer = m_vertexBuffer->object();
		m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);

		// bind first state
		setDynamicViewportState(1, &viewport, &scissor_1);
		// draw quad using vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP
		m_vk.cmdDraw(*m_cmdBuffer, 4, 1, 0, 0);

		m_pipelineAdditional.bind(*m_cmdBuffer);

		// bind second state
		setDynamicViewportState(1, &viewport, &scissor_2);
		// draw quad using vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
		m_vk.cmdDraw(*m_cmdBuffer, 6, 1, 4, 0);

		m_renderPass.end(m_vk, *m_cmdBuffer);
		endCommandBuffer(m_vk, *m_cmdBuffer);

		submitCommandsAndWait(m_vk, device, queue, m_cmdBuffer.get());

		//validation
		tcu::Texture2D referenceFrame(vk::mapVkFormat(m_colorAttachmentFormat), (int)(0.5f + static_cast<float>(WIDTH)), (int)(0.5f + static_cast<float>(HEIGHT)));
		referenceFrame.allocLevel(0);

		const deInt32 frameWidth	= referenceFrame.getWidth();
		const deInt32 frameHeight	= referenceFrame.getHeight();

		tcu::clear(referenceFrame.getLevel(0), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

		for (int y = 0; y < frameHeight; y++)
		{
			const float yCoord = (float)(y / (0.5*frameHeight)) - 1.0f;

			for (int x = 0; x < frameWidth; x++)
			{
				const float xCoord = (float)(x / (0.5*frameWidth)) - 1.0f;

				if (yCoord >= -1.0f && yCoord <= 0.0f && xCoord >= -1.0f && xCoord <= 0.0f)
					referenceFrame.getLevel(0).setPixel(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f), x, y);
				else if (yCoord > 0.0f && yCoord <= 1.0f && xCoord > 0.0f && xCoord < 1.0f)
					referenceFrame.getLevel(0).setPixel(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), x, y);
			}
		}

		const vk::VkOffset3D zeroOffset = { 0, 0, 0 };
		const tcu::ConstPixelBufferAccess renderedFrame = m_colorTargetImage->readSurface(queue, m_context.getDefaultAllocator(),
			vk::VK_IMAGE_LAYOUT_GENERAL, zeroOffset, WIDTH, HEIGHT, vk::VK_IMAGE_ASPECT_COLOR_BIT);

		if (!tcu::fuzzyCompare(log, "Result", "Image comparison result",
			referenceFrame.getLevel(0), renderedFrame, 0.05f,
			tcu::COMPARE_LOG_RESULT))
		{
			return tcu::TestStatus(QP_TEST_RESULT_FAIL, "Image verification failed");
		}

		return tcu::TestStatus(QP_TEST_RESULT_PASS, "Image verification passed");
	}
};

#ifndef CTS_USES_VULKANSC
void checkMeshShaderSupport (Context& context)
{
	context.requireDeviceFunctionality("VK_EXT_mesh_shader");
}
#endif // CTS_USES_VULKANSC

void checkNothing (Context&)
{
}

} //anonymous

DynamicStateGeneralTests::DynamicStateGeneralTests (tcu::TestContext& testCtx, vk::PipelineConstructionType pipelineConstructionType)
	: TestCaseGroup					(testCtx, "general_state", "General tests for dynamic states")
	, m_pipelineConstructionType	(pipelineConstructionType)
{
	/* Left blank on purpose */
}

DynamicStateGeneralTests::~DynamicStateGeneralTests (void) {}

void DynamicStateGeneralTests::init (void)
{
	ShaderMap basePaths;
	basePaths[glu::SHADERTYPE_FRAGMENT]	= "vulkan/dynamic_state/VertexFetch.frag";
	basePaths[glu::SHADERTYPE_MESH]		= nullptr;
	basePaths[glu::SHADERTYPE_VERTEX]	= nullptr;

	for (int i = 0; i < 2; ++i)
	{
		const bool					isMesh				= (i > 0);
		ShaderMap					shaderPaths			(basePaths);
		std::string					nameSuffix;
		std::string					descSuffix;
		FunctionSupport0::Function	checkSupportFunc;

		if (isMesh)
		{
#ifndef CTS_USES_VULKANSC
			shaderPaths[glu::SHADERTYPE_MESH] = "vulkan/dynamic_state/VertexFetch.mesh";
			nameSuffix = "_mesh";
			descSuffix = " using mesh shaders";
			checkSupportFunc = checkMeshShaderSupport;
#else
			continue;
#endif // CTS_USES_VULKANSC
		}
		else
		{
			shaderPaths[glu::SHADERTYPE_VERTEX] = "vulkan/dynamic_state/VertexFetch.vert";
			checkSupportFunc = checkNothing;
		}

		addChild(new InstanceFactory<StateSwitchTestInstance, FunctionSupport0>(m_testCtx, "state_switch" + nameSuffix, "Perform multiple draws with different VP states (scissor test)" + descSuffix, m_pipelineConstructionType, shaderPaths, checkSupportFunc));
		addChild(new InstanceFactory<BindOrderTestInstance, FunctionSupport0>(m_testCtx, "bind_order" + nameSuffix, "Check if binding order is not important for pipeline configuration" + descSuffix, m_pipelineConstructionType, shaderPaths, checkSupportFunc));
		if (!isMesh)
			addChild(new InstanceFactory<StatePersistenceTestInstance>(m_testCtx, "state_persistence" + nameSuffix, "Check if bound states are persistent across pipelines" + descSuffix, m_pipelineConstructionType, shaderPaths));
	}
}

} // DynamicState
} // vkt
