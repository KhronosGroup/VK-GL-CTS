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
 * \brief Dynamic Raster State Tests
 *//*--------------------------------------------------------------------*/

#include "vktDynamicStateRSTests.hpp"

#include "vktDynamicStateBaseClass.hpp"
#include "vktDynamicStateTestCaseUtil.hpp"

#include "vkImageUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"

#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuRGBA.hpp"

#include "deMath.h"

#include <vector>
#include <string>
#include <sstream>
#include <iomanip>

namespace vkt
{
namespace DynamicState
{

using namespace Draw;

namespace
{

class DepthBiasBaseCase : public TestInstance
{
public:
	DepthBiasBaseCase (Context& context, vk::PipelineConstructionType pipelineConstructionType, const char* vertexShaderName, const char* fragmentShaderName, const char* meshShaderName)
		: TestInstance						(context)
		, m_pipelineConstructionType		(pipelineConstructionType)
		, m_colorAttachmentFormat			(vk::VK_FORMAT_R8G8B8A8_UNORM)
		, m_depthStencilAttachmentFormat	(vk::VK_FORMAT_UNDEFINED)
		, m_topology						(vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
		, m_vk								(context.getDeviceInterface())
		, m_pipeline						(context.getInstanceInterface(), m_vk, context.getPhysicalDevice(), context.getDevice(), context.getDeviceExtensions(), pipelineConstructionType)
		, m_vertexShaderName				(vertexShaderName ? vertexShaderName : "")
		, m_fragmentShaderName				(fragmentShaderName)
		, m_meshShaderName					(meshShaderName ? meshShaderName : "")
		, m_isMesh							(meshShaderName != nullptr)
	{
		// Either mesh or vertex shader, but not both or none.
		DE_ASSERT((vertexShaderName != nullptr) != (meshShaderName != nullptr));
	}

protected:

	enum
	{
		WIDTH	= 128,
		HEIGHT	= 128
	};

	vk::PipelineConstructionType					m_pipelineConstructionType;
	vk::VkFormat									m_colorAttachmentFormat;
	vk::VkFormat									m_depthStencilAttachmentFormat;

	vk::VkPrimitiveTopology							m_topology;

	const vk::DeviceInterface&						m_vk;

	vk::Move<vk::VkDescriptorPool>					m_descriptorPool;
	vk::Move<vk::VkDescriptorSetLayout>				m_setLayout;
	vk::PipelineLayoutWrapper						m_pipelineLayout;
	vk::Move<vk::VkDescriptorSet>					m_descriptorSet;
	vk::GraphicsPipelineWrapper						m_pipeline;

	de::SharedPtr<Image>							m_colorTargetImage;
	vk::Move<vk::VkImageView>						m_colorTargetView;

	de::SharedPtr<Image>							m_depthStencilImage;
	vk::Move<vk::VkImageView>						m_attachmentView;

	PipelineCreateInfo::VertexInputState			m_vertexInputState;
	de::SharedPtr<Buffer>							m_vertexBuffer;

	vk::Move<vk::VkCommandPool>						m_cmdPool;
	vk::Move<vk::VkCommandBuffer>					m_cmdBuffer;

	vk::RenderPassWrapper							m_renderPass;

	std::string										m_vertexShaderName;
	std::string										m_fragmentShaderName;
	std::string										m_meshShaderName;

	std::vector<PositionColorVertex>				m_data;

	PipelineCreateInfo::DepthStencilState			m_depthStencilState;

	const bool										m_isMesh;

	void initialize (void)
	{
		const vk::VkDevice						device				= m_context.getDevice();
		const auto								vertDescType		= (m_isMesh ? vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER : vk::VK_DESCRIPTOR_TYPE_MAX_ENUM);
		std::vector<vk::VkPushConstantRange>	pcRanges;

		vk::VkFormatProperties formatProperties;
		// check for VK_FORMAT_D24_UNORM_S8_UINT support
		m_context.getInstanceInterface().getPhysicalDeviceFormatProperties(m_context.getPhysicalDevice(), vk::VK_FORMAT_D24_UNORM_S8_UINT, &formatProperties);
		if (formatProperties.optimalTilingFeatures & vk::VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			m_depthStencilAttachmentFormat = vk::VK_FORMAT_D24_UNORM_S8_UINT;
		}
		else
		{
			// check for VK_FORMAT_D32_SFLOAT_S8_UINT support
			m_context.getInstanceInterface().getPhysicalDeviceFormatProperties(m_context.getPhysicalDevice(), vk::VK_FORMAT_D32_SFLOAT_S8_UINT, &formatProperties);
			if (formatProperties.optimalTilingFeatures & vk::VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
			{
				m_depthStencilAttachmentFormat = vk::VK_FORMAT_D32_SFLOAT_S8_UINT;
			}
			else
				throw tcu::NotSupportedError("No valid depth stencil attachment available");
		}

		// The mesh shading pipeline will contain a set with vertex data.
#ifndef CTS_USES_VULKANSC
		if (m_isMesh)
		{
			vk::DescriptorSetLayoutBuilder	setLayoutBuilder;
			vk::DescriptorPoolBuilder		poolBuilder;

			setLayoutBuilder.addSingleBinding(vertDescType, vk::VK_SHADER_STAGE_MESH_BIT_EXT);
			m_setLayout = setLayoutBuilder.build(m_vk, device);

			poolBuilder.addType(vertDescType);
			m_descriptorPool = poolBuilder.build(m_vk, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

			m_descriptorSet = vk::makeDescriptorSet(m_vk, device, m_descriptorPool.get(), m_setLayout.get());
			pcRanges.push_back(vk::makePushConstantRange(vk::VK_SHADER_STAGE_MESH_BIT_EXT, 0u, static_cast<uint32_t>(sizeof(uint32_t))));
		}
#endif // CTS_USES_VULKANSC

		m_pipelineLayout = vk::PipelineLayoutWrapper(m_pipelineConstructionType, m_vk, device, m_setLayout.get(), de::dataOrNull(pcRanges));

		const vk::VkExtent3D imageExtent = { WIDTH, HEIGHT, 1 };
		ImageCreateInfo targetImageCreateInfo(vk::VK_IMAGE_TYPE_2D, m_colorAttachmentFormat, imageExtent, 1, 1, vk::VK_SAMPLE_COUNT_1_BIT, vk::VK_IMAGE_TILING_OPTIMAL,
											  vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT | vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT);

		m_colorTargetImage = Image::createAndAlloc(m_vk, device, targetImageCreateInfo, m_context.getDefaultAllocator(), m_context.getUniversalQueueFamilyIndex());

		const ImageCreateInfo depthStencilImageCreateInfo(vk::VK_IMAGE_TYPE_2D, m_depthStencilAttachmentFormat, imageExtent,
														  1, 1, vk::VK_SAMPLE_COUNT_1_BIT, vk::VK_IMAGE_TILING_OPTIMAL,
														  vk::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT);

		m_depthStencilImage = Image::createAndAlloc(m_vk, device, depthStencilImageCreateInfo, m_context.getDefaultAllocator(), m_context.getUniversalQueueFamilyIndex());

		const ImageViewCreateInfo colorTargetViewInfo(m_colorTargetImage->object(), vk::VK_IMAGE_VIEW_TYPE_2D, m_colorAttachmentFormat);
		m_colorTargetView = vk::createImageView(m_vk, device, &colorTargetViewInfo);

		const ImageViewCreateInfo attachmentViewInfo(m_depthStencilImage->object(), vk::VK_IMAGE_VIEW_TYPE_2D, m_depthStencilAttachmentFormat);
		m_attachmentView = vk::createImageView(m_vk, device, &attachmentViewInfo);

		RenderPassCreateInfo renderPassCreateInfo;
		renderPassCreateInfo.addAttachment(AttachmentDescription(m_colorAttachmentFormat,
																 vk::VK_SAMPLE_COUNT_1_BIT,
																 vk::VK_ATTACHMENT_LOAD_OP_LOAD,
																 vk::VK_ATTACHMENT_STORE_OP_STORE,
																 vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,
																 vk::VK_ATTACHMENT_STORE_OP_STORE,
																 vk::VK_IMAGE_LAYOUT_GENERAL,
																 vk::VK_IMAGE_LAYOUT_GENERAL));

		renderPassCreateInfo.addAttachment(AttachmentDescription(m_depthStencilAttachmentFormat,
																 vk::VK_SAMPLE_COUNT_1_BIT,
																 vk::VK_ATTACHMENT_LOAD_OP_LOAD,
																 vk::VK_ATTACHMENT_STORE_OP_STORE,
																 vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,
																 vk::VK_ATTACHMENT_STORE_OP_STORE,
																 vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
																 vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));

		const vk::VkAttachmentReference colorAttachmentReference =
		{
			0,
			vk::VK_IMAGE_LAYOUT_GENERAL
		};

		const vk::VkAttachmentReference depthAttachmentReference =
		{
			1,
			vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
		};

		renderPassCreateInfo.addSubpass(SubpassDescription(vk::VK_PIPELINE_BIND_POINT_GRAPHICS,
														   0,
														   0,
														   DE_NULL,
														   1,
														   &colorAttachmentReference,
														   DE_NULL,
														   depthAttachmentReference,
														   0,
														   DE_NULL));

		m_renderPass = vk::RenderPassWrapper(m_pipelineConstructionType, m_vk, device, &renderPassCreateInfo);

		const vk::VkVertexInputBindingDescription vertexInputBindingDescription =
		{
			0,
			(deUint32)sizeof(tcu::Vec4) * 2,
			vk::VK_VERTEX_INPUT_RATE_VERTEX,
		};

		const vk::VkVertexInputAttributeDescription vertexInputAttributeDescriptions[2] =
		{
			{
				0u,
				0u,
				vk::VK_FORMAT_R32G32B32A32_SFLOAT,
				0u
			},
			{
				1u,
				0u,
				vk::VK_FORMAT_R32G32B32A32_SFLOAT,
				(deUint32)(sizeof(float)* 4),
			}
		};

		m_vertexInputState = PipelineCreateInfo::VertexInputState(1,
																  &vertexInputBindingDescription,
																  2,
																  vertexInputAttributeDescriptions);

		std::vector<vk::VkViewport>		viewports	{ { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f } };
		std::vector<vk::VkRect2D>		scissors	{ { { 0u, 0u }, { 0u, 0u } } };

		// Shader modules.
		const auto&							binaries	= m_context.getBinaryCollection();
		const vk::ShaderWrapper				vs			= (m_isMesh ? vk::ShaderWrapper() : vk::ShaderWrapper(m_vk, device, binaries.get(m_vertexShaderName)));
		const vk::ShaderWrapper				ms			= (m_isMesh ? vk::ShaderWrapper(m_vk, device, binaries.get(m_meshShaderName)) : vk::ShaderWrapper());
		const vk::ShaderWrapper				fs			= vk::ShaderWrapper(m_vk, device, binaries.get(m_fragmentShaderName));

		const PipelineCreateInfo::ColorBlendState::Attachment	attachmentState;
		const PipelineCreateInfo::ColorBlendState				colorBlendState(1u, static_cast<const vk::VkPipelineColorBlendAttachmentState*>(&attachmentState));
		const PipelineCreateInfo::RasterizerState				rasterizerState;
		PipelineCreateInfo::DynamicState						dynamicState;

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

		m_pipeline.setupFragmentShaderState(m_pipelineLayout, *m_renderPass, 0u, fs, static_cast<const vk::VkPipelineDepthStencilStateCreateInfo*>(&m_depthStencilState))
				  .setupFragmentOutputState(*m_renderPass, 0u, static_cast<const vk::VkPipelineColorBlendStateCreateInfo*>(&colorBlendState))
				  .setMonolithicPipelineLayout(m_pipelineLayout)
				  .buildPipeline();


		std::vector<vk::VkImageView> attachments(2);
		attachments[0] = *m_colorTargetView;
		attachments[1] = *m_attachmentView;

		const FramebufferCreateInfo framebufferCreateInfo(*m_renderPass, attachments, WIDTH, HEIGHT, 1);

		m_renderPass.createFramebuffer(m_vk, device, &framebufferCreateInfo, {m_colorTargetImage->object(), m_depthStencilImage->object()});

		const vk::VkDeviceSize			dataSize	= m_data.size() * sizeof(PositionColorVertex);
		const vk::VkBufferUsageFlags	bufferUsage	= (m_isMesh ? vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT : vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
		m_vertexBuffer = Buffer::createAndAlloc(m_vk, device, BufferCreateInfo(dataSize, bufferUsage),
			m_context.getDefaultAllocator(), vk::MemoryRequirement::HostVisible);

		deUint8* ptr = reinterpret_cast<unsigned char *>(m_vertexBuffer->getBoundMemory().getHostPtr());
		deMemcpy(ptr, &m_data[0], static_cast<size_t>(dataSize));

		vk::flushAlloc(m_vk, device, m_vertexBuffer->getBoundMemory());

		// Update descriptor set for mesh shaders.
		if (m_isMesh)
		{
			vk::DescriptorSetUpdateBuilder	updateBuilder;
			const auto						location		= vk::DescriptorSetUpdateBuilder::Location::binding(0u);
			const auto						bufferInfo		= vk::makeDescriptorBufferInfo(m_vertexBuffer->object(), 0ull, dataSize);

			updateBuilder.writeSingle(m_descriptorSet.get(), location, vertDescType, &bufferInfo);
			updateBuilder.update(m_vk, device);
		}

		const CmdPoolCreateInfo cmdPoolCreateInfo(m_context.getUniversalQueueFamilyIndex());
		m_cmdPool = vk::createCommandPool(m_vk, device, &cmdPoolCreateInfo);
		m_cmdBuffer = vk::allocateCommandBuffer(m_vk, device, *m_cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	}

	virtual tcu::TestStatus iterate (void)
	{
		DE_ASSERT(false);
		return tcu::TestStatus::fail("Should reimplement iterate() method");
	}

	void beginRenderPass (void)
	{
		const vk::VkClearColorValue clearColor = { { 0.0f, 0.0f, 0.0f, 1.0f } };
		beginRenderPassWithClearColor(clearColor);
	}

	void beginRenderPassWithClearColor (const vk::VkClearColorValue &clearColor)
	{
		beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);

		initialTransitionColor2DImage(m_vk, *m_cmdBuffer, m_colorTargetImage->object(), vk::VK_IMAGE_LAYOUT_GENERAL,
									  vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT);
		initialTransitionDepthStencil2DImage(m_vk, *m_cmdBuffer, m_depthStencilImage->object(), vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
											 vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT);

		const ImageSubresourceRange subresourceRangeImage(vk::VK_IMAGE_ASPECT_COLOR_BIT);
		m_vk.cmdClearColorImage(*m_cmdBuffer, m_colorTargetImage->object(),
								vk::VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &subresourceRangeImage);

		const vk::VkClearDepthStencilValue depthStencilClearValue = { 0.0f, 0 };

		const ImageSubresourceRange subresourceRangeDepthStencil[2] = { vk::VK_IMAGE_ASPECT_DEPTH_BIT, vk::VK_IMAGE_ASPECT_STENCIL_BIT };

		m_vk.cmdClearDepthStencilImage(*m_cmdBuffer, m_depthStencilImage->object(),
									   vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &depthStencilClearValue, 2, subresourceRangeDepthStencil);

		const vk::VkMemoryBarrier memBarrier =
		{
			vk::VK_STRUCTURE_TYPE_MEMORY_BARRIER,
			DE_NULL,
			vk::VK_ACCESS_TRANSFER_WRITE_BIT,
			vk::VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
				vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
		};

		m_vk.cmdPipelineBarrier(*m_cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
			vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
			vk::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | vk::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
			0, 1, &memBarrier, 0, DE_NULL, 0, DE_NULL);

		transition2DImage(m_vk, *m_cmdBuffer, m_depthStencilImage->object(), vk::VK_IMAGE_ASPECT_DEPTH_BIT | vk::VK_IMAGE_ASPECT_STENCIL_BIT,
						  vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
						  vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
						  vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | vk::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

		m_renderPass.begin(m_vk, *m_cmdBuffer, vk::makeRect2D(0, 0, WIDTH, HEIGHT));
	}

	void setDynamicViewportState (const deUint32 width, const deUint32 height)
	{
		vk::VkViewport viewport = vk::makeViewport(tcu::UVec2(width, height));
		vk::VkRect2D scissor = vk::makeRect2D(tcu::UVec2(width, height));
		if (vk::isConstructionTypeShaderObject(m_pipelineConstructionType))
		{
#ifndef CTS_USES_VULKANSC
			m_vk.cmdSetViewportWithCount(*m_cmdBuffer, 1, &viewport);
			m_vk.cmdSetScissorWithCount(*m_cmdBuffer, 1, &scissor);
#else
			m_vk.cmdSetViewportWithCountEXT(*m_cmdBuffer, 1, &viewport);
			m_vk.cmdSetScissorWithCountEXT(*m_cmdBuffer, 1, &scissor);
#endif
		}
		else
		{
			m_vk.cmdSetViewport(*m_cmdBuffer, 0, 1, &viewport);
			m_vk.cmdSetScissor(*m_cmdBuffer, 0, 1, &scissor);
		}

	}

	void setDynamicViewportState (const deUint32 viewportCount, const vk::VkViewport* pViewports, const vk::VkRect2D* pScissors)
	{
		if (vk::isConstructionTypeShaderObject(m_pipelineConstructionType))
		{
#ifndef CTS_USES_VULKANSC
			m_vk.cmdSetViewportWithCount(*m_cmdBuffer, viewportCount, pViewports);
			m_vk.cmdSetScissorWithCount(*m_cmdBuffer, viewportCount, pScissors);
#else
			m_vk.cmdSetViewportWithCountEXT(*m_cmdBuffer, viewportCount, pViewports);
			m_vk.cmdSetScissorWithCountEXT(*m_cmdBuffer, viewportCount, pScissors);
#endif
		}
		else
		{
			m_vk.cmdSetViewport(*m_cmdBuffer, 0, viewportCount, pViewports);
			m_vk.cmdSetScissor(*m_cmdBuffer, 0, viewportCount, pScissors);
		}
	}

	void setDynamicRasterizationState (const float lineWidth = 1.0f,
		const float depthBiasConstantFactor = 0.0f,
		const float depthBiasClamp = 0.0f,
		const float depthBiasSlopeFactor = 0.0f)
	{
		m_vk.cmdSetLineWidth(*m_cmdBuffer, lineWidth);
		m_vk.cmdSetDepthBias(*m_cmdBuffer, depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
	}

	void setDynamicBlendState (const float const1 = 0.0f, const float const2 = 0.0f,
		const float const3 = 0.0f, const float const4 = 0.0f)
	{
		float blendConstantsants[4] = { const1, const2, const3, const4 };
		m_vk.cmdSetBlendConstants(*m_cmdBuffer, blendConstantsants);
	}

	void setDynamicDepthStencilState (const float minDepthBounds = 0.0f, const float maxDepthBounds = 1.0f,
		const deUint32 stencilFrontCompareMask = 0xffffffffu, const deUint32 stencilFrontWriteMask = 0xffffffffu,
		const deUint32 stencilFrontReference = 0, const deUint32 stencilBackCompareMask = 0xffffffffu,
		const deUint32 stencilBackWriteMask = 0xffffffffu, const deUint32 stencilBackReference = 0)
	{
		m_vk.cmdSetDepthBounds(*m_cmdBuffer, minDepthBounds, maxDepthBounds);
		m_vk.cmdSetStencilCompareMask(*m_cmdBuffer, vk::VK_STENCIL_FACE_FRONT_BIT, stencilFrontCompareMask);
		m_vk.cmdSetStencilWriteMask(*m_cmdBuffer, vk::VK_STENCIL_FACE_FRONT_BIT, stencilFrontWriteMask);
		m_vk.cmdSetStencilReference(*m_cmdBuffer, vk::VK_STENCIL_FACE_FRONT_BIT, stencilFrontReference);
		m_vk.cmdSetStencilCompareMask(*m_cmdBuffer, vk::VK_STENCIL_FACE_BACK_BIT, stencilBackCompareMask);
		m_vk.cmdSetStencilWriteMask(*m_cmdBuffer, vk::VK_STENCIL_FACE_BACK_BIT, stencilBackWriteMask);
		m_vk.cmdSetStencilReference(*m_cmdBuffer, vk::VK_STENCIL_FACE_BACK_BIT, stencilBackReference);
	}

#ifndef CTS_USES_VULKANSC
	void pushVertexOffset (const uint32_t				vertexOffset,
						   const vk::VkShaderStageFlags	stageFlags = vk::VK_SHADER_STAGE_MESH_BIT_EXT)
	{
		m_vk.cmdPushConstants(*m_cmdBuffer, *m_pipelineLayout, stageFlags, 0u, static_cast<uint32_t>(sizeof(uint32_t)), &vertexOffset);
	}
#endif // CTS_USES_VULKANSC
};

class DepthBiasParamTestInstance : public DepthBiasBaseCase
{
public:
	DepthBiasParamTestInstance (Context& context, vk::PipelineConstructionType pipelineConstructionType, const ShaderMap& shaders)
		: DepthBiasBaseCase (context, pipelineConstructionType, shaders.at(glu::SHADERTYPE_VERTEX), shaders.at(glu::SHADERTYPE_FRAGMENT), shaders.at(glu::SHADERTYPE_MESH))
	{
		m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, 1.0f, 0.5f, 1.0f), tcu::RGBA::blue().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, 1.0f, 0.5f, 1.0f), tcu::RGBA::blue().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, -1.0f, 0.5f, 1.0f), tcu::RGBA::blue().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, -1.0f, 0.5f, 1.0f), tcu::RGBA::blue().toVec()));

		m_data.push_back(PositionColorVertex(tcu::Vec4(-0.5f, 0.5f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(0.5f, 0.5f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(-0.5f, -0.5f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(0.5f, -0.5f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));

		m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, 1.0f, 0.5f, 1.0f), tcu::RGBA::red().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, 1.0f, 0.5f, 1.0f), tcu::RGBA::red().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, -1.0f, 0.5f, 1.0f), tcu::RGBA::red().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, -1.0f, 0.5f, 1.0f), tcu::RGBA::red().toVec()));

		// enable depth test
		m_depthStencilState = PipelineCreateInfo::DepthStencilState(
			VK_TRUE, VK_TRUE, vk::VK_COMPARE_OP_GREATER_OR_EQUAL);

		DepthBiasBaseCase::initialize();
	}

	virtual tcu::TestStatus iterate (void)
	{
		tcu::TestLog&		log		= m_context.getTestContext().getLog();
		const vk::VkQueue	queue	= m_context.getUniversalQueue();
		const vk::VkDevice	device	= m_context.getDevice();

		beginRenderPass();

		// set states here
		setDynamicViewportState(WIDTH, HEIGHT);
		setDynamicBlendState();
		setDynamicDepthStencilState();

		m_pipeline.bind(*m_cmdBuffer);

#ifndef CTS_USES_VULKANSC
		if (m_isMesh)
		{
			m_vk.cmdBindDescriptorSets(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout.get(), 0u, 1u, &m_descriptorSet.get(), 0u, nullptr);

			setDynamicRasterizationState(1.0f, 0.0f);
			pushVertexOffset(0u); m_vk.cmdDrawMeshTasksEXT(*m_cmdBuffer, 2u, 1u, 1u);
			pushVertexOffset(4u); m_vk.cmdDrawMeshTasksEXT(*m_cmdBuffer, 2u, 1u, 1u);

			setDynamicRasterizationState(1.0f, -1.0f);
			pushVertexOffset(8u); m_vk.cmdDrawMeshTasksEXT(*m_cmdBuffer, 2u, 1u, 1u);
		}
		else
#endif // CTS_USES_VULKANSC
		{
			const vk::VkDeviceSize vertexBufferOffset	= 0;
			const vk::VkBuffer vertexBuffer				= m_vertexBuffer->object();
			m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);

			setDynamicRasterizationState(1.0f, 0.0f);
			m_vk.cmdDraw(*m_cmdBuffer, 4, 1, 0, 0);
			m_vk.cmdDraw(*m_cmdBuffer, 4, 1, 4, 0);

			setDynamicRasterizationState(1.0f, -1.0f);
			m_vk.cmdDraw(*m_cmdBuffer, 4, 1, 8, 0);
		}

		m_renderPass.end(m_vk, *m_cmdBuffer);
		endCommandBuffer(m_vk, *m_cmdBuffer);

		submitCommandsAndWait(m_vk, device, queue, m_cmdBuffer.get());

		// validation
		{
			VK_CHECK(m_vk.queueWaitIdle(queue));

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
					else
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
	}
};

class DepthBiasClampParamTestInstance : public DepthBiasBaseCase
{
public:
	DepthBiasClampParamTestInstance (Context& context, vk::PipelineConstructionType pipelineConstructionType, const ShaderMap& shaders)
		: DepthBiasBaseCase (context, pipelineConstructionType, shaders.at(glu::SHADERTYPE_VERTEX), shaders.at(glu::SHADERTYPE_FRAGMENT), shaders.at(glu::SHADERTYPE_MESH))
	{
		m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f), tcu::RGBA::blue().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f), tcu::RGBA::blue().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f), tcu::RGBA::blue().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, -1.0f, 0.0f, 1.0f), tcu::RGBA::blue().toVec()));

		m_data.push_back(PositionColorVertex(tcu::Vec4(-0.5f, 0.5f, 0.01f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(0.5f, 0.5f, 0.01f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(-0.5f, -0.5f, 0.01f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(0.5f, -0.5f, 0.01f, 1.0f), tcu::RGBA::green().toVec()));

		// enable depth test
		m_depthStencilState = PipelineCreateInfo::DepthStencilState(VK_TRUE, VK_TRUE, vk::VK_COMPARE_OP_GREATER_OR_EQUAL);

		DepthBiasBaseCase::initialize();
	}

	virtual tcu::TestStatus iterate (void)
	{
		tcu::TestLog&		log		= m_context.getTestContext().getLog();
		const vk::VkQueue	queue	= m_context.getUniversalQueue();
		const vk::VkDevice	device	= m_context.getDevice();

		beginRenderPass();

		// set states here
		setDynamicViewportState(WIDTH, HEIGHT);
		setDynamicBlendState();
		setDynamicDepthStencilState();

		m_pipeline.bind(*m_cmdBuffer);

#ifndef CTS_USES_VULKANSC
		if (m_isMesh)
		{
			m_vk.cmdBindDescriptorSets(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout.get(), 0u, 1u, &m_descriptorSet.get(), 0u, nullptr);

			setDynamicRasterizationState(1.0f, 1000.0f, 0.005f);
			pushVertexOffset(0u); m_vk.cmdDrawMeshTasksEXT(*m_cmdBuffer, 2u, 1u, 1u);

			setDynamicRasterizationState(1.0f, 0.0f);
			pushVertexOffset(4u); m_vk.cmdDrawMeshTasksEXT(*m_cmdBuffer, 2u, 1u, 1u);
		}
		else
#endif // CTS_USES_VULKANSC
		{
			const vk::VkDeviceSize vertexBufferOffset = 0;
			const vk::VkBuffer vertexBuffer = m_vertexBuffer->object();
			m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);

			setDynamicRasterizationState(1.0f, 1000.0f, 0.005f);
			m_vk.cmdDraw(*m_cmdBuffer, 4, 1, 0, 0);

			setDynamicRasterizationState(1.0f, 0.0f);
			m_vk.cmdDraw(*m_cmdBuffer, 4, 1, 4, 0);
		}

		m_renderPass.end(m_vk, *m_cmdBuffer);
		endCommandBuffer(m_vk, *m_cmdBuffer);

		submitCommandsAndWait(m_vk, device, queue, m_cmdBuffer.get());

		// validation
		{
			tcu::Texture2D referenceFrame(vk::mapVkFormat(m_colorAttachmentFormat), (int)(0.5f + static_cast<float>(WIDTH)), (int)(0.5f + static_cast<float>(HEIGHT)));
			referenceFrame.allocLevel(0);

			const deInt32 frameWidth	= referenceFrame.getWidth();
			const deInt32 frameHeight	= referenceFrame.getHeight();

			tcu::clear(referenceFrame.getLevel(0), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

			for (int y = 0; y < frameHeight; y++)
			{
				float yCoord = (float)(y / (0.5*frameHeight)) - 1.0f;

				for (int x = 0; x < frameWidth; x++)
				{
					float xCoord = (float)(x / (0.5*frameWidth)) - 1.0f;

					if (xCoord >= -0.5f && xCoord <= 0.5f && yCoord >= -0.5f && yCoord <= 0.5f)
						referenceFrame.getLevel(0).setPixel(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f), x, y);
					else
						referenceFrame.getLevel(0).setPixel(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), x, y);
				}
			}

			const vk::VkOffset3D zeroOffset					= { 0, 0, 0 };
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

class LineWidthParamTestInstance : public DynamicStateBaseClass
{
public:
	LineWidthParamTestInstance (Context& context, vk::PipelineConstructionType pipelineConstructionType, const ShaderMap& shaders)
		: DynamicStateBaseClass (context, pipelineConstructionType, shaders.at(glu::SHADERTYPE_VERTEX), shaders.at(glu::SHADERTYPE_FRAGMENT), shaders.at(glu::SHADERTYPE_MESH))
	{
		m_topology = vk::VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

		m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, 0.0f, 0.0f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f), tcu::RGBA::green().toVec()));

		DynamicStateBaseClass::initialize();
	}

	virtual tcu::TestStatus iterate (void)
	{
		tcu::TestLog&		log		= m_context.getTestContext().getLog();
		const vk::VkQueue	queue	= m_context.getUniversalQueue();
		const vk::VkDevice	device	= m_context.getDevice();

		beginRenderPass();

		// set states here
		vk::VkPhysicalDeviceProperties deviceProperties;
		m_context.getInstanceInterface().getPhysicalDeviceProperties(m_context.getPhysicalDevice(), &deviceProperties);

		setDynamicViewportState(WIDTH, HEIGHT);
		setDynamicBlendState();
		setDynamicDepthStencilState();
		setDynamicRasterizationState(deFloatFloor(deviceProperties.limits.lineWidthRange[1]));

		m_pipeline.bind(*m_cmdBuffer);

#ifndef CTS_USES_VULKANSC
		if (m_isMesh)
		{
			const auto numVert = static_cast<uint32_t>(m_data.size());
			DE_ASSERT(numVert >= 1u);

			m_vk.cmdBindDescriptorSets(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout.get(), 0u, 1u, &m_descriptorSet.get(), 0u, nullptr);
			pushVertexOffset(0u, *m_pipelineLayout);
			m_vk.cmdDrawMeshTasksEXT(*m_cmdBuffer, numVert - 1u, 1u, 1u);
		}
		else
#endif // CTS_USES_VULKANSC
		{
			const vk::VkDeviceSize vertexBufferOffset	= 0;
			const vk::VkBuffer vertexBuffer				= m_vertexBuffer->object();
			m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);

			m_vk.cmdDraw(*m_cmdBuffer, static_cast<deUint32>(m_data.size()), 1, 0, 0);
		}

		m_renderPass.end(m_vk, *m_cmdBuffer);
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
				float yCoord = (float)(y / (0.5*frameHeight)) - 1.0f;

				for (int x = 0; x < frameWidth; x++)
				{
					float xCoord = (float)(x / (0.5*frameWidth)) - 1.0f;
					float lineHalfWidth = (float)(deFloor(deviceProperties.limits.lineWidthRange[1]) / frameHeight);

					if (xCoord >= -1.0f && xCoord <= 1.0f && yCoord >= -lineHalfWidth && yCoord <= lineHalfWidth)
						referenceFrame.getLevel(0).setPixel(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f), x, y);
				}
			}

			const vk::VkOffset3D zeroOffset = { 0, 0, 0 };
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
	}
};

// Tests that fail if both the depth bias clamp or depth constant factor stay at 0.0f instead of applying the real values.
struct DepthBiasNonZeroPushConstants
{
	float geometryDepth;
	float minDepth;
	float maxDepth;
};

struct DepthBiasNonZeroParams
{
	vk::PipelineConstructionType	pipelineConstructionType;
	float							depthBiasConstant;
	float							depthBiasClamp;
	DepthBiasNonZeroPushConstants	pushConstants;
	bool							useMeshShaders;
};

class DepthBiasNonZeroCase : public vkt::TestCase
{
private:
	DepthBiasNonZeroParams m_params;

public:
						DepthBiasNonZeroCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const DepthBiasNonZeroParams& params);
	virtual				~DepthBiasNonZeroCase	(void) {}

	void				checkSupport			(Context& context) const override;
	void				initPrograms			(vk::SourceCollections& programCollection) const override;
	TestInstance*		createInstance			(Context& context) const override;

	static tcu::Vec4	getExpectedColor		() { return tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f); }
};

class DepthBiasNonZeroInstance : public vkt::TestInstance
{
private:
	DepthBiasNonZeroParams m_params;

public:
						DepthBiasNonZeroInstance	(Context& context, const DepthBiasNonZeroParams& params);
	virtual				~DepthBiasNonZeroInstance	(void) {}

	tcu::TestStatus		iterate						(void) override;
};

DepthBiasNonZeroCase::DepthBiasNonZeroCase (tcu::TestContext& testCtx, const std::string& name, const std::string& description, const DepthBiasNonZeroParams& params)
	: vkt::TestCase		(testCtx, name, description)
	, m_params			(params)
{}

TestInstance* DepthBiasNonZeroCase::createInstance (Context& context) const
{
	return new DepthBiasNonZeroInstance(context, m_params);
}

DepthBiasNonZeroInstance::DepthBiasNonZeroInstance (Context& context, const DepthBiasNonZeroParams& params)
	: vkt::TestInstance	(context)
	, m_params			(params)
{}

void DepthBiasNonZeroCase::checkSupport (Context& context) const
{
	if (m_params.depthBiasClamp != 0.0f)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_DEPTH_BIAS_CLAMP);

	if (m_params.useMeshShaders)
		context.requireDeviceFunctionality("VK_EXT_mesh_shader");

	checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_params.pipelineConstructionType);
}

void DepthBiasNonZeroCase::initPrograms (vk::SourceCollections& programCollection) const
{
	if (m_params.useMeshShaders)
	{
		std::ostringstream mesh;
		mesh
			<< "#version 450\n"
			<< "#extension GL_EXT_mesh_shader : enable\n"
			<< "\n"
			<< "layout (push_constant, std430) uniform PushConstantBlock {\n"
			<< "	float geometryDepth;\n"
			<< "	float minDepth;\n"
			<< "	float maxDepth;\n"
			<< "} pc;\n"
			<< "\n"
			<< "vec2 positions[3] = vec2[](\n"
			<< "    vec2(-1.0, -1.0),\n"
			<< "    vec2(3.0, -1.0),\n"
			<< "    vec2(-1.0, 3.0)\n"
			<< ");\n"
			<< "\n"
			<< "layout(local_size_x=3) in;\n"
			<< "layout(triangles) out;\n"
			<< "layout(max_vertices=3, max_primitives=1) out;\n"
			<< "\n"
			<< "void main() {\n"
			<< "    SetMeshOutputsEXT(3u, 1u);\n"
			<< "    gl_MeshVerticesEXT[gl_LocalInvocationIndex].gl_Position = vec4(positions[gl_LocalInvocationIndex], pc.geometryDepth, 1.0);\n"
			<< "    gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0, 1, 2);\n"
			<< "}\n"
			;

		const vk::ShaderBuildOptions buildOptions (programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
		programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << buildOptions;
	}
	else
	{
		std::ostringstream vert;
		vert
			<< "#version 450\n"
			<< "\n"
			<< "layout (push_constant, std430) uniform PushConstantBlock {\n"
			<< "	float geometryDepth;\n"
			<< "	float minDepth;\n"
			<< "	float maxDepth;\n"
			<< "} pc;\n"
			<< "\n"
			<< "vec2 positions[3] = vec2[](\n"
			<< "    vec2(-1.0, -1.0),\n"
			<< "    vec2(3.0, -1.0),\n"
			<< "    vec2(-1.0, 3.0)\n"
			<< ");\n"
			<< "\n"
			<< "void main() {\n"
			<< "    gl_Position = vec4(positions[gl_VertexIndex], pc.geometryDepth, 1.0);\n"
			<< "}\n"
			;
		programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
	}

	const auto outColor = getExpectedColor();
	std::ostringstream frag;
	frag
		<< std::fixed << std::setprecision(1)
		<< "#version 450\n"
		<< "\n"
		<< "layout (push_constant, std430) uniform PushConstantBlock {\n"
		<< "	float geometryDepth;\n"
		<< "	float minDepth;\n"
		<< "	float maxDepth;\n"
		<< "} pc;\n"
		<< "\n"
		<< "layout (location=0) out vec4 outColor;\n"
		<< "\n"
		<< "void main() {\n"
		<< "    const float depth = gl_FragCoord.z;\n"
		<< "    if (depth >= pc.minDepth && depth <= pc.maxDepth) {\n"
		<< "	    outColor = vec4(" << outColor.x() << ", " << outColor.y() << ", " << outColor.z() << ", " << outColor.w() << ");\n"
		<< "    }\n"
		<< "}\n"
		;

	programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

tcu::TestStatus DepthBiasNonZeroInstance::iterate (void)
{
	const auto& vki			= m_context.getInstanceInterface();
	const auto&	vkd			= m_context.getDeviceInterface();
	const auto  physDevice	= m_context.getPhysicalDevice();
	const auto	device		= m_context.getDevice();
	auto&		alloc		= m_context.getDefaultAllocator();
	const auto	qIndex		= m_context.getUniversalQueueFamilyIndex();
	const auto	queue		= m_context.getUniversalQueue();

	const auto	depthFormat	= vk::VK_FORMAT_D16_UNORM;
	const auto	colorFormat	= vk::VK_FORMAT_R8G8B8A8_UNORM;
	const auto	colorUsage	= (vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	const auto	depthUsage	= (vk::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	const auto	extent		= vk::makeExtent3D(8u, 8u, 1u);
	const auto&	pcData		= m_params.pushConstants;
	const auto	pcDataSize	= static_cast<deUint32>(sizeof(pcData));
	const auto	pcStages	= ((m_params.useMeshShaders
#ifndef CTS_USES_VULKANSC
								? vk::VK_SHADER_STAGE_MESH_BIT_EXT
#else
								? 0
#endif // CTS_USES_VULKANSC
								: vk::VK_SHADER_STAGE_VERTEX_BIT)
							   | vk::VK_SHADER_STAGE_FRAGMENT_BIT);
	const auto	pcRange		= vk::makePushConstantRange(pcStages, 0u, pcDataSize);
	vk::RenderPassWrapper	renderPass	(m_params.pipelineConstructionType, vkd, device, colorFormat, depthFormat, vk::VK_ATTACHMENT_LOAD_OP_CLEAR, vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	const auto	stencilOp	= vk::makeStencilOpState(vk::VK_STENCIL_OP_KEEP, vk::VK_STENCIL_OP_KEEP, vk::VK_STENCIL_OP_KEEP, vk::VK_COMPARE_OP_NEVER, 0u, 0u, 0u);

	// Color buffer.
	const vk::VkImageCreateInfo colorBufferInfo =
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
		nullptr,									//	const void*				pNext;
		0u,											//	VkImageCreateFlags		flags;
		vk::VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
		colorFormat,								//	VkFormat				format;
		extent,										//	VkExtent3D				extent;
		1u,											//	deUint32				mipLevels;
		1u,											//	deUint32				arrayLayers;
		vk::VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
		vk::VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
		colorUsage,									//	VkImageUsageFlags		usage;
		vk::VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
		0u,											//	deUint32				queueFamilyIndexCount;
		nullptr,									//	const deUint32*			pQueueFamilyIndices;
		vk::VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
	};
	const auto colorBuffer = Image::createAndAlloc(vkd, device, colorBufferInfo, alloc, qIndex);

	// Depth buffer.
	const vk::VkImageCreateInfo depthBufferInfo =
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
		nullptr,									//	const void*				pNext;
		0u,											//	VkImageCreateFlags		flags;
		vk::VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
		depthFormat,								//	VkFormat				format;
		extent,										//	VkExtent3D				extent;
		1u,											//	deUint32				mipLevels;
		1u,											//	deUint32				arrayLayers;
		vk::VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
		vk::VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
		depthUsage,									//	VkImageUsageFlags		usage;
		vk::VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
		0u,											//	deUint32				queueFamilyIndexCount;
		nullptr,									//	const deUint32*			pQueueFamilyIndices;
		vk::VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
	};
	const auto depthBuffer = Image::createAndAlloc(vkd, device, depthBufferInfo, alloc, qIndex);

	const auto colorSubresourceRange	= vk::makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const auto colorView				= vk::makeImageView(vkd, device, colorBuffer->object(), vk::VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSubresourceRange);

	const auto depthSubresourceRange	= vk::makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u);
	const auto depthView				= vk::makeImageView(vkd, device, depthBuffer->object(), vk::VK_IMAGE_VIEW_TYPE_2D, depthFormat, depthSubresourceRange);

	// Create framebuffer.
	const std::vector<vk::VkImage>		images		{ colorBuffer->object(), depthBuffer->object()};
	const std::vector<vk::VkImageView>	attachments	{ colorView.get(), depthView.get() };
	renderPass.createFramebuffer(vkd, device, static_cast<deUint32>(attachments.size()), de::dataOrNull(images), de::dataOrNull(attachments), extent.width, extent.height);

	// Descriptor set and pipeline layout.
	vk::DescriptorSetLayoutBuilder setLayoutBuilder;
	const auto dsLayout			= setLayoutBuilder.build(vkd, device);
	const vk::PipelineLayoutWrapper pipelineLayout	(m_params.pipelineConstructionType, vkd, device, 1u, &dsLayout.get(), 1u, &pcRange);

	// Shader modules.
	vk::ShaderWrapper	vertModule;
	vk::ShaderWrapper	meshModule;
	vk::ShaderWrapper	fragModule;
	const auto&						binaries	= m_context.getBinaryCollection();

	if (binaries.contains("vert"))
		vertModule = vk::ShaderWrapper(vkd, device, binaries.get("vert"));
	if (binaries.contains("mesh"))
		meshModule = vk::ShaderWrapper(vkd, device, binaries.get("mesh"));
	fragModule = vk::ShaderWrapper(vkd, device, binaries.get("frag"), 0u);

	const std::vector<vk::VkViewport>	viewports	{ vk::makeViewport(extent) };
	const std::vector<vk::VkRect2D>		scissors	{ vk::makeRect2D(extent) };

	// Vertex input state without bindings and attributes.
	const vk::VkPipelineVertexInputStateCreateInfo vertexInputInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType                             sType
		nullptr,														// const void*                                 pNext
		0u,																// VkPipelineVertexInputStateCreateFlags       flags
		0u,																// deUint32                                    vertexBindingDescriptionCount
		nullptr,														// const VkVertexInputBindingDescription*      pVertexBindingDescriptions
		0u,																// deUint32                                    vertexAttributeDescriptionCount
		nullptr,														// const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions
	};

	// Depth/stencil state, with depth test and writes enabled.
	const vk::VkPipelineDepthStencilStateCreateInfo depthStencilStateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	// VkStructureType                          sType
		nullptr,														// const void*                              pNext
		0u,																// VkPipelineDepthStencilStateCreateFlags   flags
		VK_TRUE,														// VkBool32                                 depthTestEnable
		VK_TRUE,														// VkBool32                                 depthWriteEnable
		vk::VK_COMPARE_OP_ALWAYS,										// VkCompareOp                              depthCompareOp
		VK_FALSE,														// VkBool32                                 depthBoundsTestEnable
		VK_FALSE,														// VkBool32                                 stencilTestEnable
		stencilOp,														// VkStencilOpState                         front
		stencilOp,														// VkStencilOpState                         back
		0.0f,															// float                                    minDepthBounds
		1.0f,															// float                                    maxDepthBounds
	};

	// Rasterization state with depth bias enabled.
	const vk::VkPipelineRasterizationStateCreateInfo rasterizationInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,	// VkStructureType                            sType
		nullptr,														// const void*                                pNext
		0u,																// VkPipelineRasterizationStateCreateFlags    flags
		VK_FALSE,														// VkBool32                                   depthClampEnable
		VK_FALSE,														// VkBool32                                   rasterizerDiscardEnable
		vk::VK_POLYGON_MODE_FILL,										// VkPolygonMode                              polygonMode
		vk::VK_CULL_MODE_NONE,											// VkCullModeFlags                            cullMode
		vk::VK_FRONT_FACE_CLOCKWISE,									// VkFrontFace                                frontFace
		VK_TRUE,														// VkBool32                                   depthBiasEnable
		0.0f,															// float                                      depthBiasConstantFactor
		0.0f,															// float                                      depthBiasClamp
		0.0f,															// float                                      depthBiasSlopeFactor
		1.0f															// float                                      lineWidth
	};

	// Dynamic state.
	const std::vector<vk::VkDynamicState> dynamicStates (1u, vk::VK_DYNAMIC_STATE_DEPTH_BIAS);

	const vk::VkPipelineDynamicStateCreateInfo dynamicStateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,	//	VkStructureType						sType;
		nullptr,													//	const void*							pNext;
		0u,															//	VkPipelineDynamicStateCreateFlags	flags;
		static_cast<deUint32>(dynamicStates.size()),				//	deUint32							dynamicStateCount;
		de::dataOrNull(dynamicStates),								//	const VkDynamicState*				pDynamicStates;
	};

	// Graphics pipeline.
	vk::GraphicsPipelineWrapper pipeline(vki, vkd, physDevice, device, m_context.getDeviceExtensions(), m_params.pipelineConstructionType);

#ifndef CTS_USES_VULKANSC
	if (m_params.useMeshShaders)
	{
		pipeline.setDefaultTopology(vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
			.setDefaultColorBlendState()
			.setDynamicState(static_cast<const vk::VkPipelineDynamicStateCreateInfo*>(&dynamicStateInfo))
			.setDefaultMultisampleState()
			.setupPreRasterizationMeshShaderState(viewports, scissors, pipelineLayout, *renderPass, 0u, vk::ShaderWrapper(), meshModule, &rasterizationInfo)
			.setupFragmentShaderState(pipelineLayout, *renderPass, 0u, fragModule, static_cast<const vk::VkPipelineDepthStencilStateCreateInfo*>(&depthStencilStateInfo))
			.setupFragmentOutputState(*renderPass, 0u)
			.setMonolithicPipelineLayout(pipelineLayout)
			.buildPipeline();
	}
	else
#endif // CTS_USES_VULKANSC
	{
		pipeline.setDefaultTopology(vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
			.setDefaultColorBlendState()
			.setDynamicState(static_cast<const vk::VkPipelineDynamicStateCreateInfo*>(&dynamicStateInfo))
			.setDefaultMultisampleState()
			.setupVertexInputState(&vertexInputInfo)
			.setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, *renderPass, 0u, vertModule, &rasterizationInfo)
			.setupFragmentShaderState(pipelineLayout, *renderPass, 0u, fragModule, static_cast<const vk::VkPipelineDepthStencilStateCreateInfo*>(&depthStencilStateInfo))
			.setupFragmentOutputState(*renderPass, 0u)
			.setMonolithicPipelineLayout(pipelineLayout)
			.buildPipeline();
	}

	// Command pool and buffer.
	const auto cmdPool		= vk::makeCommandPool(vkd, device, qIndex);
	const auto cmdBufferPtr	= vk::allocateCommandBuffer(vkd, device, cmdPool.get(), vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	// Clear colors.
	const std::vector<vk::VkClearValue> clearColors =
	{
		vk::makeClearValueColorF32(0.0f, 0.0f, 0.0f, 1.0f),
		vk::makeClearValueDepthStencil(0.0f, 0u),
	};

	vk::beginCommandBuffer(vkd, cmdBuffer);
	renderPass.begin(vkd, cmdBuffer, scissors.at(0), static_cast<deUint32>(clearColors.size()), de::dataOrNull(clearColors));
	pipeline.bind(cmdBuffer);
	vkd.cmdSetDepthBias(cmdBuffer, m_params.depthBiasConstant, m_params.depthBiasClamp, 0.0f);
	vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), pcStages, 0u, pcDataSize, &pcData);
#ifndef CTS_USES_VULKANSC
	if (m_params.useMeshShaders)
	{
		vkd.cmdDrawMeshTasksEXT(cmdBuffer, 1u, 1u, 1u);
	}
	else
#endif // CTS_USES_VULKANSC
	{
		vkd.cmdDraw(cmdBuffer, 3u, 1u, 0u, 0u);
	}
	renderPass.end(vkd, cmdBuffer);
	vk::endCommandBuffer(vkd, cmdBuffer);
	vk::submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Check color buffer contents.
	const auto		offset		= vk::makeOffset3D(0, 0, 0);
	const auto		iWidth		= static_cast<int>(extent.width);
	const auto		iHeight		= static_cast<int>(extent.height);
	const auto		colorPixels	= colorBuffer->readSurface(queue, alloc, vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, offset, iWidth, iHeight, vk::VK_IMAGE_ASPECT_COLOR_BIT);
	const auto		expected	= DepthBiasNonZeroCase::getExpectedColor();
	const tcu::Vec4	threshold	(0.0f);
	auto&			log			= m_context.getTestContext().getLog();

	if (!tcu::floatThresholdCompare(log, "Result", "Result", expected, colorPixels, threshold, tcu::COMPARE_LOG_ON_ERROR))
		return tcu::TestStatus::fail("Unexpected color buffer value; check log for details");

	return tcu::TestStatus::pass("Pass");
}

void checkDepthBiasClampSupport (Context& context)
{
	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_DEPTH_BIAS_CLAMP);
}

void checkWideLinesSupport (Context& context)
{
	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_WIDE_LINES);
}

void checkMeshShaderSupport (Context& context)
{
	context.requireDeviceFunctionality("VK_EXT_mesh_shader");
}

void checkMeshAndBiasClampSupport(Context& context)
{
	checkMeshShaderSupport(context);
	checkDepthBiasClampSupport(context);
}

void checkMeshAndWideLinesSupport(Context& context)
{
	checkMeshShaderSupport(context);
	checkWideLinesSupport(context);
}

void checkNothing (Context&)
{}

} //anonymous

DynamicStateRSTests::DynamicStateRSTests (tcu::TestContext& testCtx, vk::PipelineConstructionType pipelineConstructionType)
	: TestCaseGroup					(testCtx, "rs_state", "Tests for rasterizer state")
	, m_pipelineConstructionType	(pipelineConstructionType)
{
	/* Left blank on purpose */
}

DynamicStateRSTests::~DynamicStateRSTests ()
{
}

void DynamicStateRSTests::init (void)
{
	ShaderMap basePaths;
	basePaths[glu::SHADERTYPE_FRAGMENT]	= "vulkan/dynamic_state/VertexFetch.frag";
	basePaths[glu::SHADERTYPE_VERTEX]	= nullptr;
	basePaths[glu::SHADERTYPE_MESH]		= nullptr;

	for (int i = 0; i < 2; ++i)
	{
		ShaderMap shaderPaths(basePaths);
		const bool isMesh = (i > 0);
		std::string nameSuffix;
		std::string descSuffix;

		if (isMesh)
		{
#ifndef CTS_USES_VULKANSC
			nameSuffix = "_mesh";
			descSuffix = " using mesh shaders";
			shaderPaths[glu::SHADERTYPE_MESH] = "vulkan/dynamic_state/VertexFetch.mesh";
#else
			continue;
#endif // CTS_USES_VULKANSC
		}
		else
		{
			shaderPaths[glu::SHADERTYPE_VERTEX] = "vulkan/dynamic_state/VertexFetch.vert";
		}

		addChild(new InstanceFactory<DepthBiasParamTestInstance, FunctionSupport0>(m_testCtx, "depth_bias" + nameSuffix, "Test depth bias functionality" + descSuffix, m_pipelineConstructionType, shaderPaths, (isMesh ? checkMeshShaderSupport : checkNothing)));
		addChild(new InstanceFactory<DepthBiasClampParamTestInstance, FunctionSupport0>(m_testCtx, "depth_bias_clamp" + nameSuffix, "Test depth bias clamp functionality" + descSuffix, m_pipelineConstructionType, shaderPaths, (isMesh ? checkMeshAndBiasClampSupport : checkDepthBiasClampSupport)));
		if (isMesh)
			shaderPaths[glu::SHADERTYPE_MESH] = "vulkan/dynamic_state/VertexFetchLines.mesh";
		addChild(new InstanceFactory<LineWidthParamTestInstance, FunctionSupport0>(m_testCtx, "line_width" + nameSuffix, "Draw a line with width set to max defined by physical device" + descSuffix, m_pipelineConstructionType, shaderPaths, (isMesh ? checkMeshAndWideLinesSupport : checkWideLinesSupport)));

		{
			const DepthBiasNonZeroParams params =
			{
				m_pipelineConstructionType,
				16384.0f,	//	float							depthBiasConstant;
				0.0f,		//	float							depthBiasClamp;
				{			//	DepthBiasNonZeroPushConstants	pushConstants;
					0.375f,	//		float geometryDepth;
					0.5f,	//		float minDepth;
					1.0f,	//		float maxDepth;
				},
				isMesh,
			};
			addChild(new DepthBiasNonZeroCase(m_testCtx, "nonzero_depth_bias_constant" + nameSuffix, "", params));
		}
		{
			const DepthBiasNonZeroParams params =
			{
				m_pipelineConstructionType,
				16384.0f,		//	float							depthBiasConstant;
				0.125f,			//	float							depthBiasClamp;
				{				//	DepthBiasNonZeroPushConstants	pushConstants;
					0.375f,		//		float geometryDepth;
					0.46875f,	//		float minDepth;
					0.53125f,	//		float maxDepth;
				},
				isMesh,
			};
			addChild(new DepthBiasNonZeroCase(m_testCtx, "nonzero_depth_bias_clamp" + nameSuffix, "", params));
		}
	}
}

} // DynamicState
} // vkt
