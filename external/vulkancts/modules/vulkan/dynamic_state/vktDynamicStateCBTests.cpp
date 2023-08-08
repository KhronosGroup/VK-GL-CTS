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
 * \brief Dynamic CB State Tests
 *//*--------------------------------------------------------------------*/

#include "vktDynamicStateCBTests.hpp"

#include "vktDynamicStateBaseClass.hpp"
#include "vktDynamicStateTestCaseUtil.hpp"

#include "vkImageUtil.hpp"
#include "vkCmdUtil.hpp"

#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuRGBA.hpp"

namespace vkt
{
namespace DynamicState
{

using namespace Draw;

namespace
{

class BlendConstantsTestInstance : public DynamicStateBaseClass
{
public:
	BlendConstantsTestInstance (Context& context, vk::PipelineConstructionType pipelineConstructionType, const ShaderMap& shaders)
		: DynamicStateBaseClass	(context, pipelineConstructionType, shaders.at(glu::SHADERTYPE_VERTEX), shaders.at(glu::SHADERTYPE_FRAGMENT), shaders.at(glu::SHADERTYPE_MESH))
	{
		m_topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

		m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, -1.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, -1.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));

		DynamicStateBaseClass::initialize();
	}

	virtual void initPipeline (const vk::VkDevice device)
	{
		const auto&							binaries	= m_context.getBinaryCollection();
		const vk::ShaderWrapper				ms			(m_isMesh ? vk::ShaderWrapper(m_vk, device, binaries.get(m_meshShaderName), 0) : vk::ShaderWrapper());
		const vk::ShaderWrapper				vs			(m_isMesh ? vk::ShaderWrapper() : vk::ShaderWrapper(m_vk, device, binaries.get(m_vertexShaderName), 0));
		const vk::ShaderWrapper				fs			(vk::ShaderWrapper(m_vk, device, binaries.get(m_fragmentShaderName), 0));
		std::vector<vk::VkViewport>			viewports	{ { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f } };
		std::vector<vk::VkRect2D>			scissors	{ { { 0u, 0u }, { 0u, 0u } } };

		const PipelineCreateInfo::ColorBlendState::Attachment	attachmentState(VK_TRUE,
																				vk::VK_BLEND_FACTOR_SRC_ALPHA, vk::VK_BLEND_FACTOR_CONSTANT_COLOR, vk::VK_BLEND_OP_ADD,
																				vk::VK_BLEND_FACTOR_SRC_ALPHA, vk::VK_BLEND_FACTOR_CONSTANT_ALPHA, vk::VK_BLEND_OP_ADD);
		const PipelineCreateInfo::ColorBlendState				colorBlendState(1, static_cast<const vk::VkPipelineColorBlendAttachmentState*>(&attachmentState));
		const PipelineCreateInfo::RasterizerState				rasterizerState;
		const PipelineCreateInfo::DepthStencilState				depthStencilState;
		const PipelineCreateInfo::DynamicState					dynamicState;

		m_pipeline.setDefaultTopology(m_topology)
				  .setDynamicState(static_cast<const vk::VkPipelineDynamicStateCreateInfo*>(&dynamicState))
				  .setDefaultMultisampleState();

#ifndef CTS_USES_VULKANSC
		if (m_isMesh)
		{
			m_pipeline
				  .setupPreRasterizationMeshShaderState(viewports,
														scissors,
														m_pipelineLayout,
														*m_renderPass,
														0u,
														vk::ShaderWrapper(),
														ms,
														static_cast<const vk::VkPipelineRasterizationStateCreateInfo*>(&rasterizerState));
		}
		else
#endif // CTS_USES_VULKANSC
		{
			m_pipeline
				  .setupVertexInputState(&m_vertexInputState)
				  .setupPreRasterizationShaderState(viewports,
													scissors,
													m_pipelineLayout,
													*m_renderPass,
													0u,
													vs,
													static_cast<const vk::VkPipelineRasterizationStateCreateInfo*>(&rasterizerState));
		}

		m_pipeline.setupFragmentShaderState(m_pipelineLayout, *m_renderPass, 0u, fs, static_cast<const vk::VkPipelineDepthStencilStateCreateInfo*>(&depthStencilState))
				  .setupFragmentOutputState(*m_renderPass, 0u, static_cast<const vk::VkPipelineColorBlendStateCreateInfo*>(&colorBlendState))
				  .setMonolithicPipelineLayout(m_pipelineLayout)
				  .buildPipeline();
	}

	virtual tcu::TestStatus iterate (void)
	{
		tcu::TestLog&		log		= m_context.getTestContext().getLog();
		const vk::VkQueue	queue	= m_context.getUniversalQueue();
		const vk::VkDevice	device	= m_context.getDevice();

		const vk::VkClearColorValue clearColor = { { 1.0f, 1.0f, 1.0f, 1.0f } };
		beginRenderPassWithClearColor(clearColor);

		m_pipeline.bind(*m_cmdBuffer);

		// bind states here
		setDynamicViewportState(WIDTH, HEIGHT);
		setDynamicRasterizationState();
		setDynamicDepthStencilState();
		setDynamicBlendState(0.33f, 0.1f, 0.66f, 0.5f);

#ifndef CTS_USES_VULKANSC
		if (m_isMesh)
		{
			const auto numVert = static_cast<uint32_t>(m_data.size());
			DE_ASSERT(numVert >= 2u);

			m_vk.cmdBindDescriptorSets(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout.get(), 0u, 1u, &m_descriptorSet.get(), 0u, nullptr);
			pushVertexOffset(0u, *m_pipelineLayout);
			m_vk.cmdDrawMeshTasksEXT(*m_cmdBuffer, numVert - 2u, 1u, 1u);
		}
		else
#endif // CTS_USES_VULKANSC
		{
			const vk::VkDeviceSize	vertexBufferOffset	= 0;
			const vk::VkBuffer		vertexBuffer		= m_vertexBuffer->object();

			m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
			m_vk.cmdDraw(*m_cmdBuffer, static_cast<deUint32>(m_data.size()), 1, 0, 0);
		}

		m_renderPass.end(m_vk, *m_cmdBuffer);
		endCommandBuffer(m_vk, *m_cmdBuffer);

		submitCommandsAndWait(m_vk, device, queue, m_cmdBuffer.get());

		//validation
		{
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

					if ((yCoord >= -1.0f && yCoord <= 1.0f && xCoord >= -1.0f && xCoord <= 1.0f))
						referenceFrame.getLevel(0).setPixel(tcu::Vec4(0.33f, 1.0f, 0.66f, 1.0f), x, y);
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
	}
};

#ifndef CTS_USES_VULKANSC
void checkMeshShaderSupport (Context& context)
{
	context.requireDeviceFunctionality("VK_EXT_mesh_shader");
}
#endif // CTS_USES_VULKANSC

} //anonymous

DynamicStateCBTests::DynamicStateCBTests (tcu::TestContext& testCtx, vk::PipelineConstructionType pipelineConstructionType)
	: TestCaseGroup					(testCtx, "cb_state", "Tests for color blend state")
	, m_pipelineConstructionType	(pipelineConstructionType)
{
	/* Left blank on purpose */
}

DynamicStateCBTests::~DynamicStateCBTests (void) {}

void DynamicStateCBTests::init (void)
{
	ShaderMap pathsBase;
	pathsBase[glu::SHADERTYPE_FRAGMENT] = "vulkan/dynamic_state/VertexFetch.frag";
	pathsBase[glu::SHADERTYPE_VERTEX] = nullptr;
	pathsBase[glu::SHADERTYPE_MESH] = nullptr;

	{
		ShaderMap shaderPaths(pathsBase);
		shaderPaths[glu::SHADERTYPE_VERTEX] = "vulkan/dynamic_state/VertexFetch.vert";
		addChild(new InstanceFactory<BlendConstantsTestInstance>(m_testCtx, "blend_constants", "Check if blend constants are working properly", m_pipelineConstructionType, shaderPaths));
	}
#ifndef CTS_USES_VULKANSC
	{
		ShaderMap shaderPaths(pathsBase);
		shaderPaths[glu::SHADERTYPE_MESH] = "vulkan/dynamic_state/VertexFetch.mesh";
		addChild(new InstanceFactory<BlendConstantsTestInstance, FunctionSupport0>(m_testCtx, "blend_constants_mesh", "Check if blend constants are working properly in mesh shaders", m_pipelineConstructionType, shaderPaths, checkMeshShaderSupport));
	}
#endif // CTS_USES_VULKANSC
}

} // DynamicState
} // vkt
