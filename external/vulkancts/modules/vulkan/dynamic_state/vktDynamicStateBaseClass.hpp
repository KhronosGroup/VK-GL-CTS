#ifndef _VKTDYNAMICSTATEBASECLASS_HPP
#define _VKTDYNAMICSTATEBASECLASS_HPP
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
 * \brief Dynamic State Tests - Base Class
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "vktTestCase.hpp"

#include "vktDynamicStateTestCaseUtil.hpp"
#include "vktDynamicStateImageObjectUtil.hpp"
#include "vktDynamicStateBufferObjectUtil.hpp"
#include "vktDynamicStateCreateInfoUtil.hpp"

namespace vkt
{
namespace DynamicState
{

class DynamicStateBaseClass : public TestInstance
{
public:
	DynamicStateBaseClass (Context& context, const char* vertexShaderName, const char* fragmentShaderName);

protected:
	void					initialize						(void);

	virtual void			initPipeline					(const vk::VkDevice				device);

	virtual tcu::TestStatus iterate							(void);

	void					beginRenderPass					(void);

	void					beginRenderPassWithClearColor	(const vk::VkClearColorValue&	clearColor);

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

	void					setDynamicDepthStencilState		(const float					minDepthBounds = -1.0f,
															 const float					maxDepthBounds = 1.0f,
															 const deUint32					stencilFrontCompareMask = 0xffffffffu,
															 const deUint32					stencilFrontWriteMask = 0xffffffffu,
															 const deUint32					stencilFrontReference = 0,
															 const deUint32					stencilBackCompareMask = 0xffffffffu,
															 const deUint32					stencilBackWriteMask = 0xffffffffu,
															 const deUint32					stencilBackReference = 0);
	enum
	{
		WIDTH       = 128,
		HEIGHT      = 128
	};

	vk::VkFormat									m_colorAttachmentFormat;

	vk::VkPrimitiveTopology							m_topology;

	const vk::DeviceInterface&						m_vk;

	vk::Move<vk::VkPipeline>						m_pipeline;
	vk::Move<vk::VkPipelineLayout>					m_pipelineLayout;

	de::SharedPtr<Image>							m_colorTargetImage;
	vk::Move<vk::VkImageView>						m_colorTargetView;

	PipelineCreateInfo::VertexInputState			m_vertexInputState;
	de::SharedPtr<Buffer>							m_vertexBuffer;

	vk::Move<vk::VkCommandPool>						m_cmdPool;
	vk::Move<vk::VkCommandBuffer>					m_cmdBuffer;

	vk::Move<vk::VkFramebuffer>						m_framebuffer;
	vk::Move<vk::VkRenderPass>						m_renderPass;

	const std::string								m_vertexShaderName;
	const std::string								m_fragmentShaderName;
	std::vector<PositionColorVertex>				m_data;
};

} // DynamicState
} // vkt

#endif // _VKTDYNAMICSTATEBASECLASS_HPP
