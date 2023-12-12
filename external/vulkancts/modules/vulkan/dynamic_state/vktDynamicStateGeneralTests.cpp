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
#include "vkObjUtil.hpp"
#include "vkBarrierUtil.hpp"

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

void initStaticStencilMaskZeroPrograms (vk::SourceCollections& dst, vk::PipelineConstructionType)
{
	std::ostringstream vert;
	vert
		<< "#version 460\n"
		<< "layout (location=0) in vec4 inPos;\n"
		<< "layout (location=1) in vec4 inColor;\n"
		<< "layout (location=0) out vec4 outColor;\n"
		<< "void main (void) {\n"
		<< "    gl_Position = inPos;\n"
		<< "    outColor = inColor;\n"
		<< "}\n"
		;
	dst.glslSources.add("vert") << glu::VertexSource(vert.str());

	// Fragment shader such that it will actually discard all fragments.
	std::ostringstream frag;
	frag
		<< "#version 460\n"
		<< "layout (location=0) in vec4 inColor;\n"
		<< "layout (location=0) out vec4 outColor;\n"
		<< "void main (void) {\n"
		<< "    if (inColor == vec4(0.0, 0.0, 1.0, 1.0)) {\n"
		<< "        discard;\n"
		<< "    }\n"
		<< "    outColor = inColor;\n"
		<< "}\n"
		;
	dst.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

void checkStaticStencilMaskZeroSupport (vkt::Context& context, vk::PipelineConstructionType pipelineConstructionType)
{
	const auto&	vki				= context.getInstanceInterface();
	const auto	physicalDevice	= context.getPhysicalDevice();

	checkPipelineConstructionRequirements(vki, physicalDevice, pipelineConstructionType);
}

// Find a suitable format for the depth/stencil buffer.
vk::VkFormat chooseDepthStencilFormat (const vk::InstanceInterface& vki, vk::VkPhysicalDevice physDev)
{
	// The spec mandates support for one of these two formats.
	const vk::VkFormat candidates[] = { vk::VK_FORMAT_D32_SFLOAT_S8_UINT, vk::VK_FORMAT_D24_UNORM_S8_UINT };

	for (const auto& format : candidates)
	{
		const auto properties = getPhysicalDeviceFormatProperties(vki, physDev, format);
		if ((properties.optimalTilingFeatures & vk::VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0u)
			return format;
	}

	TCU_FAIL("No suitable depth/stencil format found");
	return vk::VK_FORMAT_UNDEFINED; // Unreachable.
}

// On some implementations, when the static state is 0, the pipeline is created
// such that it is unable to avoid writing to stencil when a pixel is discarded.
tcu::TestStatus staticStencilMaskZeroProgramsTest (Context& context, vk::PipelineConstructionType pipelineConstructionType)
{
	const auto&			ctx			= context.getContextCommonData();
	const tcu::IVec3	fbExtent	(1, 1, 1);
	const auto			pixelCount	= fbExtent.x() * fbExtent.y() * fbExtent.z();
	const auto			vkExtent	= vk::makeExtent3D(fbExtent);
	const auto			fbFormat	= vk::VK_FORMAT_R8G8B8A8_UNORM;
	const auto			tcuFormat	= mapVkFormat(fbFormat);
	const auto			fbUsage		= (vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	const auto			dsFormat	= chooseDepthStencilFormat(ctx.vki, ctx.physicalDevice);
	const auto			depthFormat	= vk::getDepthCopyFormat(dsFormat);
	const auto			stencilFmt	= vk::getStencilCopyFormat(dsFormat);
	const auto			dsUsage		= (vk::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	const tcu::Vec4		clearColor	(0.0f, 0.0f, 0.0f, 1.0f);
	const float			clearDepth	= 1.0f;
	const uint32_t		clearStenc	= 0u;
	const tcu::Vec4		geomColor	(0.0f, 0.0f, 1.0f, 1.0f); // Must match frag shader discard color.
	const tcu::Vec4		threshold	(0.0f, 0.0f, 0.0f, 0.0f); // When using 0 and 1 only, we expect exact results.
	const auto			colorSRR	= vk::makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const auto			dsSRR		= vk::makeImageSubresourceRange((vk::VK_IMAGE_ASPECT_DEPTH_BIT | vk::VK_IMAGE_ASPECT_STENCIL_BIT), 0u, 1u, 0u, 1u);
	const auto			colorSRL	= vk::makeImageSubresourceLayers(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const auto			depthSRL	= vk::makeImageSubresourceLayers(vk::VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, 1u);
	const auto			stencilSRL	= vk::makeImageSubresourceLayers(vk::VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, 1u);

	// Color buffer with verification buffer.
	vk::ImageWithBuffer colorBuffer (
		ctx.vkd,
		ctx.device,
		ctx.allocator,
		vkExtent,
		fbFormat,
		fbUsage,
		vk::VK_IMAGE_TYPE_2D);

	// Depth/stencil buffer.
	const vk::VkImageCreateInfo dsCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
		nullptr,									//	const void*				pNext;
		0u,											//	VkImageCreateFlags		flags;
		vk::VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
		dsFormat,									//	VkFormat				format;
		vkExtent,									//	VkExtent3D				extent;
		1u,											//	uint32_t				mipLevels;
		1u,											//	uint32_t				arrayLayers;
		vk::VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
		vk::VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
		dsUsage,									//	VkImageUsageFlags		usage;
		vk::VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
		0u,											//	uint32_t				queueFamilyIndexCount;
		nullptr,									//	const uint32_t*			pQueueFamilyIndices;
		vk::VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
	};
	vk::ImageWithMemory dsBuffer (ctx.vkd, ctx.device, ctx.allocator, dsCreateInfo, vk::MemoryRequirement::Any);
	const auto dsImageView = vk::makeImageView(ctx.vkd, ctx.device, dsBuffer.get(), vk::VK_IMAGE_VIEW_TYPE_2D, dsFormat, dsSRR);

	// Verification buffers for depth and stencil.
	const auto depthVerifBufferSize = static_cast<vk::VkDeviceSize>(tcu::getPixelSize(depthFormat) * pixelCount);
	const auto depthVerifBufferInfo = vk::makeBufferCreateInfo(depthVerifBufferSize, vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	vk::BufferWithMemory depthVerifBuffer (ctx.vkd, ctx.device, ctx.allocator, depthVerifBufferInfo, vk::MemoryRequirement::HostVisible);

	const auto stencilVerifBufferSize = static_cast<vk::VkDeviceSize>(tcu::getPixelSize(stencilFmt) * pixelCount);
	const auto stencilVerifBufferInfo = vk::makeBufferCreateInfo(stencilVerifBufferSize, vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	vk::BufferWithMemory stencilVerifBuffer (ctx.vkd, ctx.device, ctx.allocator, stencilVerifBufferInfo, vk::MemoryRequirement::HostVisible);

	// Vertices.
	const std::vector<PositionColorVertex> vertices
	{
		PositionColorVertex(tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f), geomColor),
		PositionColorVertex(tcu::Vec4(-1.0f,  3.0f, 0.0f, 1.0f), geomColor),
		PositionColorVertex(tcu::Vec4( 3.0f, -1.0f, 0.0f, 1.0f), geomColor),
	};

	// Vertex buffer
	const auto				vbSize			= static_cast<vk::VkDeviceSize>(de::dataSize(vertices));
	const auto				vbInfo			= vk::makeBufferCreateInfo(vbSize, vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	vk::BufferWithMemory	vertexBuffer	(ctx.vkd, ctx.device, ctx.allocator, vbInfo, vk::MemoryRequirement::HostVisible);
	const auto				vbAlloc			= vertexBuffer.getAllocation();
	void*					vbData			= vbAlloc.getHostPtr();
	const auto				vbOffset		= static_cast<vk::VkDeviceSize>(0);

	deMemcpy(vbData, de::dataOrNull(vertices), de::dataSize(vertices));
	flushAlloc(ctx.vkd, ctx.device, vbAlloc); // strictly speaking, not needed.

	const std::vector<vk::VkImage> framebufferImages
	{
		colorBuffer.getImage(),
		dsBuffer.get(),
	};

	const std::vector<vk::VkImageView> framebufferViews
	{
		colorBuffer.getImageView(),
		dsImageView.get(),
	};

	const auto	pipelineLayout	= vk::PipelineLayoutWrapper(pipelineConstructionType, ctx.vkd, ctx.device);
	auto		renderPass		= vk::RenderPassWrapper(pipelineConstructionType, ctx.vkd, ctx.device, fbFormat, dsFormat);

	DE_ASSERT(framebufferImages.size() == framebufferViews.size());
	renderPass.createFramebuffer(ctx.vkd, ctx.device, de::sizeU32(framebufferViews), de::dataOrNull(framebufferImages), de::dataOrNull(framebufferViews), vkExtent.width, vkExtent.height);

	// Modules.
	const auto&	binaries		= context.getBinaryCollection();
	const auto	vertModule		= vk::ShaderWrapper(ctx.vkd, ctx.device, binaries.get("vert"));
	const auto	fragModule		= vk::ShaderWrapper(ctx.vkd, ctx.device, binaries.get("frag"));

	const std::vector<vk::VkViewport>	viewports	(1u, makeViewport(vkExtent));
	const std::vector<vk::VkRect2D>		scissors	(1u, makeRect2D(vkExtent));

	const auto stencilStaticWriteMask	= 0u; // This is key for this test and what was causing issues in some implementations.
	const auto stencilDynamicWriteMask	= 0xFFu;

	const std::vector<vk::VkDynamicState> dynamicStates { vk::VK_DYNAMIC_STATE_STENCIL_WRITE_MASK };

	// The stencil op state is such that it will overwrite the stencil value for all non-discarded fragments.
	// However, the fragment shader should discard all fragments.
	const auto stencilOpState = vk::makeStencilOpState(vk::VK_STENCIL_OP_REPLACE,
													   vk::VK_STENCIL_OP_REPLACE,
													   vk::VK_STENCIL_OP_REPLACE,
													   vk::VK_COMPARE_OP_ALWAYS,
													   0xFFu,
													   stencilStaticWriteMask,
													   0xFFu);

	const auto vtxBindingDesc = vk::makeVertexInputBindingDescription(
		0u, static_cast<uint32_t>(sizeof(PositionColorVertex)), vk::VK_VERTEX_INPUT_RATE_VERTEX);

	const std::vector<vk::VkVertexInputAttributeDescription> vtxAttributes
	{
		vk::makeVertexInputAttributeDescription(0u, 0u, vk::VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<uint32_t>(offsetof(PositionColorVertex, position))),
		vk::makeVertexInputAttributeDescription(1u, 0u, vk::VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<uint32_t>(offsetof(PositionColorVertex, color))),
	};

	const vk::VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	//	VkStructureType								sType;
		nullptr,														//	const void*									pNext;
		0u,																//	VkPipelineVertexInputStateCreateFlags		flags;
		1u,																//	uint32_t									vertexBindingDescriptionCount;
		&vtxBindingDesc,												//	const VkVertexInputBindingDescription*		pVertexBindingDescriptions;
		de::sizeU32(vtxAttributes),										//	uint32_t									vertexAttributeDescriptionCount;
		de::dataOrNull(vtxAttributes),									//	const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
	};

	const vk::VkPipelineDepthStencilStateCreateInfo depthStencilState =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	//	VkStructureType							sType;
		nullptr,														//	const void*								pNext;
		0u,																//	VkPipelineDepthStencilStateCreateFlags	flags;
		VK_TRUE,														//	VkBool32								depthTestEnable;
		VK_TRUE,														//	VkBool32								depthWriteEnable;
		vk::VK_COMPARE_OP_LESS,											//	VkCompareOp								depthCompareOp;
		VK_FALSE,														//	VkBool32								depthBoundsTestEnable;
		VK_TRUE,														//	VkBool32								stencilTestEnable;
		stencilOpState,													//	VkStencilOpState						front;
		stencilOpState,													//	VkStencilOpState						back;
		0.0f,															//	float									minDepthBounds;
		1.0f,															//	float									maxDepthBounds;
	};

	const vk::VkPipelineDynamicStateCreateInfo dynamicStateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,	//	VkStructureType						sType;
		nullptr,													//	const void*							pNext;
		0u,															//	VkPipelineDynamicStateCreateFlags	flags;
		de::sizeU32(dynamicStates),									//	uint32_t							dynamicStateCount;
		de::dataOrNull(dynamicStates),								//	const VkDynamicState*				pDynamicStates;
	};

	vk::GraphicsPipelineWrapper pipeline (ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device, context.getDeviceExtensions(), pipelineConstructionType);
	pipeline
		.setDefaultTopology(vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
		.setDefaultMultisampleState()
		.setDefaultRasterizationState()
		.setDefaultColorBlendState()
		.setDynamicState(&dynamicStateInfo)
		.setupVertexInputState(&vertexInputStateCreateInfo)
		.setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, *renderPass, 0u, vertModule)
		.setupFragmentShaderState(pipelineLayout, *renderPass, 0u, fragModule, &depthStencilState)
		.setupFragmentOutputState(*renderPass)
		.buildPipeline()
		;

	vk::CommandPoolWithBuffer cmd (ctx.vkd, ctx.device, ctx.qfIndex);
	const auto cmdBuffer = *cmd.cmdBuffer;

	const std::vector<vk::VkClearValue> clearValues
	{
		vk::makeClearValueColor(clearColor),
		vk::makeClearValueDepthStencil(clearDepth, clearStenc),
	};

	beginCommandBuffer(ctx.vkd, cmdBuffer);
	renderPass.begin(ctx.vkd, cmdBuffer, scissors.at(0u), de::sizeU32(clearValues), de::dataOrNull(clearValues));
	ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vbOffset);
	pipeline.bind(cmdBuffer);
	ctx.vkd.cmdSetStencilWriteMask(cmdBuffer, vk::VK_STENCIL_FACE_FRONT_AND_BACK, stencilDynamicWriteMask);	// Write every value.
	ctx.vkd.cmdDraw(cmdBuffer, de::sizeU32(vertices), 1u, 0u, 0u);
	renderPass.end(ctx.vkd, cmdBuffer);
	{
		// Insert barriers and copy images to the different verification buffers.
		const std::vector<vk::VkImageMemoryBarrier> imageBarriers
		{
			// Color barrier.
			vk::makeImageMemoryBarrier(vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
									   vk::VK_ACCESS_TRANSFER_READ_BIT,
									   vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
									   vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
									   colorBuffer.getImage(),
									   colorSRR),
			// Depth/stencil barrier.
			vk::makeImageMemoryBarrier(vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
									   vk::VK_ACCESS_TRANSFER_READ_BIT,
									   vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
									   vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
									   dsBuffer.get(),
									   dsSRR),
		};
		const auto srcPipelineStages =
			(	vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
			|	vk::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
			|	vk::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT		);

		ctx.vkd.cmdPipelineBarrier(cmdBuffer,
								   srcPipelineStages,
								   vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
								   0u, 0u, nullptr, 0u, nullptr,
								   de::sizeU32(imageBarriers), de::dataOrNull(imageBarriers));

		const auto colorRegion = vk::makeBufferImageCopy(vkExtent, colorSRL);
		ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, colorBuffer.getImage(), vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
									 colorBuffer.getBuffer(), 1u, &colorRegion);

		const auto depthRegion = vk::makeBufferImageCopy(vkExtent, depthSRL);
		ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, dsBuffer.get(), vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
									 depthVerifBuffer.get(), 1u, &depthRegion);

		const auto stencilRegion = vk::makeBufferImageCopy(vkExtent, stencilSRL);
		ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, dsBuffer.get(), vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
									 stencilVerifBuffer.get(), 1u, &stencilRegion);
	}
	{
		// Transfer to host sync barrier.
		const auto transfer2Host = vk::makeMemoryBarrier(vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_ACCESS_HOST_READ_BIT);
		ctx.vkd.cmdPipelineBarrier(cmdBuffer,
								   vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
								   vk::VK_PIPELINE_STAGE_HOST_BIT,
								   0u, 1u, &transfer2Host, 0u, nullptr, 0u, nullptr);
	}
	endCommandBuffer(ctx.vkd, cmdBuffer);
	submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

	auto& log = context.getTestContext().getLog();

	// Verify color output.
	vk::invalidateAlloc(ctx.vkd, ctx.device, colorBuffer.getBufferAllocation());
	tcu::PixelBufferAccess resColorAccess (tcuFormat, fbExtent, colorBuffer.getBufferAllocation().getHostPtr());

	tcu::TextureLevel	refColorLevel	(tcuFormat, fbExtent.x(), fbExtent.y());
	auto				refColorAccess	= refColorLevel.getAccess();
	tcu::clear(refColorAccess, clearColor);	// All fragments should have been discarded.

	if (!tcu::floatThresholdCompare(log, "ColorResult", "", refColorAccess, resColorAccess, threshold, tcu::COMPARE_LOG_ON_ERROR))
		return tcu::TestStatus::fail("Unexpected color in result buffer; check log for details");

	// Verify depth.
	vk::invalidateAlloc(ctx.vkd, ctx.device, depthVerifBuffer.getAllocation());
	tcu::PixelBufferAccess resDepthAccess (depthFormat, fbExtent, depthVerifBuffer.getAllocation().getHostPtr());

	tcu::TextureLevel	refDepthLevel	(depthFormat, fbExtent.x(), fbExtent.y());
	auto				refDepthAccess	= refDepthLevel.getAccess();
	tcu::clearDepth(refDepthAccess, clearDepth);	// All fragments should have been discarded.

	if (!tcu::dsThresholdCompare(log, "DepthResult", "", refDepthAccess, resDepthAccess, 0.0f, tcu::COMPARE_LOG_ON_ERROR))
		return tcu::TestStatus::fail("Unexpected depth in result buffer; check log for details");

	// Verify stencil.
	vk::invalidateAlloc(ctx.vkd, ctx.device, stencilVerifBuffer.getAllocation());
	tcu::PixelBufferAccess resStencilAccess (stencilFmt, fbExtent, stencilVerifBuffer.getAllocation().getHostPtr());

	tcu::TextureLevel	refStencilLevel		(stencilFmt, fbExtent.x(), fbExtent.y());
	auto				refStencilAccess	= refStencilLevel.getAccess();
	tcu::clearStencil(refStencilAccess, static_cast<int>(clearStenc));	// All fragments should have been discarded.

	if (!tcu::dsThresholdCompare(log, "StencilResult", "", refStencilAccess, resStencilAccess, 0.0f, tcu::COMPARE_LOG_ON_ERROR))
		return tcu::TestStatus::fail("Unexpected depth in result buffer; check log for details");

	return tcu::TestStatus::pass("Pass");
}

} //anonymous

// General tests for dynamic states
DynamicStateGeneralTests::DynamicStateGeneralTests (tcu::TestContext& testCtx, vk::PipelineConstructionType pipelineConstructionType)
	: TestCaseGroup					(testCtx, "general_state")
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
		FunctionSupport0::Function	checkSupportFunc;

		if (isMesh)
		{
#ifndef CTS_USES_VULKANSC
			shaderPaths[glu::SHADERTYPE_MESH] = "vulkan/dynamic_state/VertexFetch.mesh";
			nameSuffix = "_mesh";
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

		// Perform multiple draws with different VP states (scissor test)
		addChild(new InstanceFactory<StateSwitchTestInstance, FunctionSupport0>(m_testCtx, "state_switch" + nameSuffix, m_pipelineConstructionType, shaderPaths, checkSupportFunc));
		// Check if binding order is not important for pipeline configuration
		addChild(new InstanceFactory<BindOrderTestInstance, FunctionSupport0>(m_testCtx, "bind_order" + nameSuffix, m_pipelineConstructionType, shaderPaths, checkSupportFunc));
		if (!isMesh) {
			// Check if bound states are persistent across pipelines
			addChild(new InstanceFactory<StatePersistenceTestInstance>(m_testCtx, "state_persistence" + nameSuffix, m_pipelineConstructionType, shaderPaths));
		}
	}

	addFunctionCaseWithPrograms(this, "static_stencil_mask_zero", checkStaticStencilMaskZeroSupport, initStaticStencilMaskZeroPrograms, staticStencilMaskZeroProgramsTest, m_pipelineConstructionType);
}

} // DynamicState
} // vkt
