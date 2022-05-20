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
 * \brief Dynamic State Viewport Tests
 *//*--------------------------------------------------------------------*/

#include "vktDynamicStateVPTests.hpp"

#include "vktDynamicStateBaseClass.hpp"
#include "vktDynamicStateTestCaseUtil.hpp"

#include "vkImageUtil.hpp"
#include "vkCmdUtil.hpp"

#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuRGBA.hpp"

namespace vkt
{
namespace DynamicState
{

using namespace Draw;

namespace
{

class ViewportStateBaseCase : public DynamicStateBaseClass
{
public:
	ViewportStateBaseCase (Context& context, const char* vertexShaderName, const char* fragmentShaderName, const char* meshShaderName)
		: DynamicStateBaseClass	(context, vertexShaderName, fragmentShaderName, meshShaderName)
	{}

	void initialize(void)
	{
		m_data.push_back(PositionColorVertex(tcu::Vec4(-0.5f, 0.5f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(0.5f, 0.5f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(-0.5f, -0.5f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(0.5f, -0.5f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));

		DynamicStateBaseClass::initialize();
	}

	virtual tcu::Texture2D buildReferenceFrame (void)
	{
		DE_ASSERT(false);
		return tcu::Texture2D(tcu::TextureFormat(), 0, 0);
	}

	virtual void setDynamicStates (void)
	{
		DE_ASSERT(false);
	}

	virtual tcu::TestStatus iterate (void)
	{
		tcu::TestLog&		log		= m_context.getTestContext().getLog();
		const vk::VkQueue	queue	= m_context.getUniversalQueue();
		const vk::VkDevice	device	= m_context.getDevice();

		beginRenderPass();

		// set states here
		setDynamicStates();

		m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

		if (m_isMesh)
		{
			const auto numVert = static_cast<uint32_t>(m_data.size());
			DE_ASSERT(numVert >= 2u);

			m_vk.cmdBindDescriptorSets(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout.get(), 0u, 1u, &m_descriptorSet.get(), 0u, nullptr);
			pushVertexOffset(0u, *m_pipelineLayout);
			m_vk.cmdDrawMeshTasksEXT(*m_cmdBuffer, numVert - 2u, 1u, 1u);
		}
		else
		{
			const vk::VkDeviceSize vertexBufferOffset = 0;
			const vk::VkBuffer vertexBuffer = m_vertexBuffer->object();
			m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);

			m_vk.cmdDraw(*m_cmdBuffer, static_cast<deUint32>(m_data.size()), 1, 0, 0);
		}

		endRenderPass(m_vk, *m_cmdBuffer);
		endCommandBuffer(m_vk, *m_cmdBuffer);

		submitCommandsAndWait(m_vk, device, queue, m_cmdBuffer.get());

		// validation
		{
			tcu::Texture2D referenceFrame = buildReferenceFrame();

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

class ViewportParamTestInstance : public ViewportStateBaseCase
{
public:
	ViewportParamTestInstance (Context& context, const ShaderMap& shaders)
		: ViewportStateBaseCase (context, shaders.at(glu::SHADERTYPE_VERTEX), shaders.at(glu::SHADERTYPE_FRAGMENT), shaders.at(glu::SHADERTYPE_MESH))
	{
		ViewportStateBaseCase::initialize();
	}

	virtual void setDynamicStates(void)
	{
		const vk::VkViewport viewport	= { 0.0f, 0.0f, static_cast<float>(WIDTH) * 2.0f, static_cast<float>(HEIGHT) * 2.0f, 0.0f, 0.0f };
		const vk::VkRect2D scissor		= { { 0, 0 }, { WIDTH, HEIGHT } };

		setDynamicViewportState(1, &viewport, &scissor);
		setDynamicRasterizationState();
		setDynamicBlendState();
		setDynamicDepthStencilState();
	}

	virtual tcu::Texture2D buildReferenceFrame (void)
	{
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
	ScissorParamTestInstance (Context& context, const ShaderMap& shaders)
		: ViewportStateBaseCase (context, shaders.at(glu::SHADERTYPE_VERTEX), shaders.at(glu::SHADERTYPE_FRAGMENT), shaders.at(glu::SHADERTYPE_MESH))
	{
		ViewportStateBaseCase::initialize();
	}

	virtual void setDynamicStates (void)
	{
		const vk::VkViewport viewport	= { 0.0f, 0.0f, (float)WIDTH, (float)HEIGHT, 0.0f, 0.0f };
		const vk::VkRect2D scissor		= { { 0, 0 }, { WIDTH / 2, HEIGHT / 2 } };

		setDynamicViewportState(1, &viewport, &scissor);
		setDynamicRasterizationState();
		setDynamicBlendState();
		setDynamicDepthStencilState();
	}

	virtual tcu::Texture2D buildReferenceFrame (void)
	{
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

	static constexpr uint32_t kNumViewports = 4u;

	ViewportArrayTestInstance (Context& context, const ShaderMap& shaders)
		: DynamicStateBaseClass	(context, shaders.at(glu::SHADERTYPE_VERTEX), shaders.at(glu::SHADERTYPE_FRAGMENT), shaders.at(glu::SHADERTYPE_MESH))
		, m_geometryShaderName	(shaders.at(glu::SHADERTYPE_GEOMETRY) ? shaders.at(glu::SHADERTYPE_GEOMETRY) : "")
	{
		if (m_isMesh)
			DE_ASSERT(m_geometryShaderName.empty());
		else
			DE_ASSERT(!m_geometryShaderName.empty());

		for (uint32_t i = 0u; i < kNumViewports; i++)
		{
			m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, 1.0f, (float)i / 3.0f, 1.0f), tcu::RGBA::green().toVec()));
			m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, 1.0f, (float)i / 3.0f, 1.0f), tcu::RGBA::green().toVec()));
			m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, -1.0f, (float)i / 3.0f, 1.0f), tcu::RGBA::green().toVec()));
			m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, -1.0f, (float)i / 3.0f, 1.0f), tcu::RGBA::green().toVec()));
		}

		DynamicStateBaseClass::initialize();
	}

	virtual void initPipeline (const vk::VkDevice device)
	{
		PipelineCreateInfo pipelineCreateInfo(*m_pipelineLayout, *m_renderPass, 0, 0);

		// Shader modules.
		vk::Move<vk::VkShaderModule>	vs;
		vk::Move<vk::VkShaderModule>	gs;
		vk::Move<vk::VkShaderModule>	fs;
		vk::Move<vk::VkShaderModule>	ms;
		const auto&						binaries = m_context.getBinaryCollection();

		if (m_isMesh)
		{
			ms = vk::createShaderModule(m_vk, device, binaries.get(m_meshShaderName));
			pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*ms, "main", vk::VK_SHADER_STAGE_MESH_BIT_EXT));
		}
		else
		{
			vs = vk::createShaderModule(m_vk, device, binaries.get(m_vertexShaderName));
			gs = vk::createShaderModule(m_vk, device, binaries.get(m_geometryShaderName));
			pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*vs, "main", vk::VK_SHADER_STAGE_VERTEX_BIT));
			pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*gs, "main", vk::VK_SHADER_STAGE_GEOMETRY_BIT));
		}

		fs = vk::createShaderModule(m_vk, device, binaries.get(m_fragmentShaderName));
		pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*fs, "main", vk::VK_SHADER_STAGE_FRAGMENT_BIT));

		const PipelineCreateInfo::ColorBlendState::Attachment vkCbAttachmentState;

		pipelineCreateInfo.addState(PipelineCreateInfo::VertexInputState(m_vertexInputState));
		pipelineCreateInfo.addState(PipelineCreateInfo::InputAssemblerState(m_topology));
		pipelineCreateInfo.addState(PipelineCreateInfo::ColorBlendState(1, &vkCbAttachmentState));
		pipelineCreateInfo.addState(PipelineCreateInfo::ViewportState(kNumViewports));
		pipelineCreateInfo.addState(PipelineCreateInfo::DepthStencilState());
		pipelineCreateInfo.addState(PipelineCreateInfo::RasterizerState());
		pipelineCreateInfo.addState(PipelineCreateInfo::MultiSampleState());
		pipelineCreateInfo.addState(PipelineCreateInfo::DynamicState());

		m_pipeline = vk::createGraphicsPipeline(m_vk, device, DE_NULL, &pipelineCreateInfo);
	}

	virtual tcu::TestStatus iterate (void)
	{
		tcu::TestLog&		log		= m_context.getTestContext().getLog();
		const vk::VkQueue	queue	= m_context.getUniversalQueue();
		const vk::VkDevice	device	= m_context.getDevice();

		beginRenderPass();

		// set states here
		const float halfWidth		= (float)WIDTH / 2;
		const float halfHeight		= (float)HEIGHT / 2;
		const deInt32 quarterWidth	= WIDTH / 4;
		const deInt32 quarterHeight = HEIGHT / 4;

		const vk::VkViewport viewports[kNumViewports] =
		{
			{ 0.0f, 0.0f, (float)halfWidth, (float)halfHeight, 0.0f, 0.0f },
			{ halfWidth, 0.0f, (float)halfWidth, (float)halfHeight, 0.0f, 0.0f },
			{ halfWidth, halfHeight, (float)halfWidth, (float)halfHeight, 0.0f, 0.0f },
			{ 0.0f, halfHeight, (float)halfWidth, (float)halfHeight, 0.0f, 0.0f }
		};

		const vk::VkRect2D scissors[kNumViewports] =
		{
			{ { quarterWidth, quarterHeight }, { quarterWidth, quarterHeight } },
			{ { (deInt32)halfWidth, quarterHeight }, { quarterWidth, quarterHeight } },
			{ { (deInt32)halfWidth, (deInt32)halfHeight }, { quarterWidth, quarterHeight } },
			{ { quarterWidth, (deInt32)halfHeight }, { quarterWidth, quarterHeight } },
		};

		setDynamicViewportState(kNumViewports, viewports, scissors);
		setDynamicRasterizationState();
		setDynamicBlendState();
		setDynamicDepthStencilState();

		m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

		DE_ASSERT(m_data.size() % kNumViewports == 0u);
		const uint32_t vertsPerViewport = static_cast<uint32_t>(m_data.size() / kNumViewports);

		if (!m_isMesh)
		{
			const vk::VkDeviceSize vertexBufferOffset = 0;
			const vk::VkBuffer vertexBuffer = m_vertexBuffer->object();
			m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);

			for (uint32_t i = 0u; i < kNumViewports; ++i)
			{
				const uint32_t firstVertex = i * vertsPerViewport;
				m_vk.cmdDraw(*m_cmdBuffer, vertsPerViewport, 1, firstVertex, 0);
			}
		}
		else
		{
			DE_ASSERT(vertsPerViewport >= 2u);

			m_vk.cmdBindDescriptorSets(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout.get(), 0u, 1u, &m_descriptorSet.get(), 0u, nullptr);

			for (uint32_t i = 0u; i < kNumViewports; ++i)
			{
				const uint32_t firstVertex = i * vertsPerViewport;
				pushVertexOffset(firstVertex, *m_pipelineLayout);
				m_vk.cmdDrawMeshTasksEXT(*m_cmdBuffer, vertsPerViewport - 2u, 1u, 1u);
			}
		}

		endRenderPass(m_vk, *m_cmdBuffer);
		endCommandBuffer(m_vk, *m_cmdBuffer);

		submitCommandsAndWait(m_vk, device, queue, m_cmdBuffer.get());

		// validation
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

					if (xCoord >= -0.5f && xCoord <= 0.5f && yCoord >= -0.5f && yCoord <= 0.5f)
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
	}
};

void checkGeometryAndMultiViewportSupport (Context& context)
{
	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);
	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_MULTI_VIEWPORT);
}

void checkMeshShaderSupport (Context& context)
{
	context.requireDeviceFunctionality("VK_EXT_mesh_shader");
}

void checkMeshAndMultiViewportSupport (Context& context)
{
	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_MULTI_VIEWPORT);
	checkMeshShaderSupport(context);
}

void checkNothing (Context&)
{
}

} //anonymous

DynamicStateVPTests::DynamicStateVPTests (tcu::TestContext& testCtx)
	: TestCaseGroup (testCtx, "vp_state", "Tests for viewport state")
{
	/* Left blank on purpose */
}

DynamicStateVPTests::~DynamicStateVPTests ()
{
}

void DynamicStateVPTests::init (void)
{
	ShaderMap basePaths;
	basePaths[glu::SHADERTYPE_FRAGMENT]	= "vulkan/dynamic_state/VertexFetch.frag";
	basePaths[glu::SHADERTYPE_GEOMETRY]	= nullptr;
	basePaths[glu::SHADERTYPE_VERTEX]	= nullptr;
	basePaths[glu::SHADERTYPE_MESH]		= nullptr;

	for (int i = 0; i < 2; ++i)
	{
		const bool					isMesh					= (i > 0);
		ShaderMap					shaderPaths(basePaths);
		std::string					nameSuffix;
		std::string					descSuffix;
		FunctionSupport0::Function	checkSupportFunc;

		if (isMesh)
		{
			shaderPaths[glu::SHADERTYPE_MESH] = "vulkan/dynamic_state/VertexFetch.mesh";
			nameSuffix = "_mesh";
			descSuffix = " using mesh shaders";
			checkSupportFunc = checkMeshShaderSupport;
		}
		else
		{
			shaderPaths[glu::SHADERTYPE_VERTEX] = "vulkan/dynamic_state/VertexFetch.vert";
			checkSupportFunc = checkNothing;
		}

		addChild(new InstanceFactory<ViewportParamTestInstance, FunctionSupport0>(m_testCtx, "viewport" + nameSuffix, "Set viewport which is twice bigger than screen size" + descSuffix, shaderPaths, checkSupportFunc));
		addChild(new InstanceFactory<ScissorParamTestInstance, FunctionSupport0>(m_testCtx, "scissor" + nameSuffix, "Perform a scissor test on 1/4 bottom-left part of the surface" + descSuffix, shaderPaths, checkSupportFunc));

		if (isMesh)
		{
			shaderPaths[glu::SHADERTYPE_MESH] = "vulkan/dynamic_state/VertexFetchViewportArray.mesh";
			checkSupportFunc = checkMeshAndMultiViewportSupport;
		}
		else
		{
			shaderPaths[glu::SHADERTYPE_GEOMETRY] = "vulkan/dynamic_state/ViewportArray.geom";
			checkSupportFunc = checkGeometryAndMultiViewportSupport;
		}
		addChild(new InstanceFactory<ViewportArrayTestInstance, FunctionSupport0>(m_testCtx, "viewport_array" + nameSuffix, "Multiple viewports and scissors" + descSuffix, shaderPaths, checkSupportFunc));
	}
}

} // DynamicState
} // vkt
