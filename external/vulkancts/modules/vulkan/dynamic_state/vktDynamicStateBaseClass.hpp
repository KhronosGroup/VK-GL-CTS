#ifndef _VKTDYNAMICSTATEBASECLASS_HPP
#define _VKTDYNAMICSTATEBASECLASS_HPP
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
* \brief Dynamic State Tests - Base Class
*//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "vktTestCase.hpp"

#include "vktDynamicStateTestCaseUtil.hpp"
#include "vktDrawImageObjectUtil.hpp"
#include "vktDrawBufferObjectUtil.hpp"
#include "vktDrawCreateInfoUtil.hpp"
#include "vkPipelineConstructionUtil.hpp"

namespace vkt
{
namespace DynamicState
{

class DynamicStateBaseClass : public TestInstance
{
public:
	DynamicStateBaseClass (Context& context,
						   vk::PipelineConstructionType pipelineConstructionType,
						   const char* vertexShaderName,
						   const char* fragmentShaderName,
						   const char* meshShaderName = nullptr);

protected:
	void					initialize						(void);

	virtual void			initRenderPass					(const vk::VkDevice				device);
	virtual void			initFramebuffer					(const vk::VkDevice				device);
	virtual void			initPipeline					(const vk::VkDevice				device);

	virtual tcu::TestStatus iterate							(void);

	void					beginRenderPass					(void);

	void					beginRenderPassWithClearColor	(const vk::VkClearColorValue&	clearColor,
															 const bool						skipBeginCmdBuffer	= false);

	void					setDynamicViewportState			(const deUint32					width,
															const deUint32					height);

	void					setDynamicViewportState			(deUint32						viewportCount,
															 const vk::VkViewport*			pViewports,
															 const vk::VkRect2D*			pScissors);

	void					setDynamicRasterizationState	(const float					lineWidth = 1.0f,
															 const float					depthBiasConstantFactor = 0.0f,
															 const float					depthBiasClamp = 0.0f,
															 const float					depthBiasSlopeFactor = 0.0f);

	void					setDynamicBlendState			(const float					const1 = 0.0f, const float const2 = 0.0f,
															 const float					const3 = 0.0f, const float const4 = 0.0f);

	void					setDynamicDepthStencilState		(const float					minDepthBounds = 0.0f,
															 const float					maxDepthBounds = 1.0f,
															 const deUint32					stencilFrontCompareMask = 0xffffffffu,
															 const deUint32					stencilFrontWriteMask = 0xffffffffu,
															 const deUint32					stencilFrontReference = 0,
															 const deUint32					stencilBackCompareMask = 0xffffffffu,
															 const deUint32					stencilBackWriteMask = 0xffffffffu,
															 const deUint32					stencilBackReference = 0);

#ifndef CTS_USES_VULKANSC
	void					pushVertexOffset				(const uint32_t					vertexOffset,
															 const vk::VkPipelineLayout		pipelineLayout,
															 const vk::VkShaderStageFlags	stageFlags = vk::VK_SHADER_STAGE_MESH_BIT_EXT);
#endif // CTS_USES_VULKANSC

	enum
	{
		WIDTH       = 128,
		HEIGHT      = 128
	};

	vk::PipelineConstructionType							m_pipelineConstructionType;
	vk::VkFormat											m_colorAttachmentFormat;

	vk::VkPrimitiveTopology									m_topology;

	const vk::DeviceInterface&								m_vk;

	vk::Move<vk::VkDescriptorPool>							m_descriptorPool;
	vk::Move<vk::VkDescriptorSetLayout>						m_meshSetLayout;
	vk::Move<vk::VkDescriptorSetLayout>						m_otherSetLayout;
	vk::PipelineLayoutWrapper								m_pipelineLayout;
	vk::Move<vk::VkDescriptorSet>							m_descriptorSet;
	vk::GraphicsPipelineWrapper								m_pipeline;

	de::SharedPtr<Draw::Image>								m_colorTargetImage;
	vk::Move<vk::VkImageView>								m_colorTargetView;

	Draw::PipelineCreateInfo::VertexInputState				m_vertexInputState;
	de::SharedPtr<Draw::Buffer>								m_vertexBuffer;

	vk::Move<vk::VkCommandPool>								m_cmdPool;
	vk::Move<vk::VkCommandBuffer>							m_cmdBuffer;

	vk::RenderPassWrapper									m_renderPass;

	const std::string										m_vertexShaderName;
	const std::string										m_fragmentShaderName;
	const std::string										m_meshShaderName;
	std::vector<PositionColorVertex>						m_data;
	bool													m_isMesh;

	Draw::PipelineCreateInfo::ColorBlendState::Attachment	m_attachmentState;
};

} // DynamicState
} // vkt

#endif // _VKTDYNAMICSTATEBASECLASS_HPP
