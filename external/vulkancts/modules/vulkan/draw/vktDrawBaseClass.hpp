#ifndef _VKTDRAWBASECLASS_HPP
#define _VKTDRAWBASECLASS_HPP
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
 * \brief Command draw Tests - Base Class
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vktTestCase.hpp"
#include "vktDrawGroupParams.hpp"

#include "tcuTestLog.hpp"
#include "tcuResource.hpp"
#include "tcuImageCompare.hpp"
#include "tcuCommandLine.hpp"

#include "vkRefUtil.hpp"
#include "vkImageUtil.hpp"

#include "deSharedPtr.hpp"

#include "vkPrograms.hpp"

#include "vktDrawCreateInfoUtil.hpp"
#include "vktDrawImageObjectUtil.hpp"
#include "vktDrawBufferObjectUtil.hpp"

namespace vkt
{
namespace Draw
{

struct PositionColorVertex
{
				PositionColorVertex (tcu::Vec4 position_, tcu::Vec4 color_)
					: position	(position_)
					, color		(color_)
				{}

	tcu::Vec4	position;
	tcu::Vec4	color;
};

struct VertexElementData : public PositionColorVertex
{
				VertexElementData (tcu::Vec4 position_, tcu::Vec4 color_, deUint32 refVertexIndex_)
					: PositionColorVertex	(position_, color_)
					, refVertexIndex		(refVertexIndex_)
				{
				}

	deUint32	refVertexIndex;
};

struct ReferenceImageCoordinates
{
	ReferenceImageCoordinates (void)
		: left		(-0.3)
		, right		(0.3)
		, top		(0.3)
		, bottom	(-0.3)
	{
	}

	double left;
	double right;
	double top;
	double bottom;
};

struct ReferenceImageInstancedCoordinates
{
	ReferenceImageInstancedCoordinates (void)
		: left		(-0.3)
		, right		(0.6)
		, top		(0.3)
		, bottom	(-0.6)
	{
	}

	double left;
	double right;
	double top;
	double bottom;
};

class DrawTestsBaseClass : public TestInstance
{
public:
								DrawTestsBaseClass	(Context& context,
													 const char* vertexShaderName,
													 const char* fragmentShaderName,
													 const SharedGroupParams groupParams,
													 vk::VkPrimitiveTopology topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
													 const uint32_t layers = 1u);

protected:
	void						initialize				(void);
	virtual void				initPipeline			(const vk::VkDevice device);
	void						preRenderBarriers		(void);
	void						beginLegacyRender		(vk::VkCommandBuffer cmdBuffer, const vk::VkSubpassContents content = vk::VK_SUBPASS_CONTENTS_INLINE);
	void						endLegacyRender			(vk::VkCommandBuffer cmdBuffer);
	virtual tcu::TestStatus		iterate					(void)						{ TCU_FAIL("Implement iterate() method!");	}

#ifndef CTS_USES_VULKANSC
	void						beginSecondaryCmdBuffer	(const vk::DeviceInterface& vk, const vk::VkRenderingFlagsKHR renderingFlags = 0u);
	void						beginDynamicRender		(vk::VkCommandBuffer cmdBuffer, const vk::VkRenderingFlagsKHR renderingFlags = 0u);
	void						endDynamicRender		(vk::VkCommandBuffer cmdBuffer);
#endif // CTS_USES_VULKANSC
	virtual uint32_t			getDefaultViewMask		(void) const;

	enum
	{
		WIDTH = 256,
		HEIGHT = 256
	};

	vk::VkFormat									m_colorAttachmentFormat;

	const SharedGroupParams							m_groupParams;
	const vk::VkPrimitiveTopology					m_topology;
	const uint32_t									m_layers; // For multiview.

	const vk::DeviceInterface&						m_vk;

	vk::Move<vk::VkPipeline>						m_pipeline;
	vk::Move<vk::VkPipelineLayout>					m_pipelineLayout;

	de::SharedPtr<Image>							m_colorTargetImage;
	vk::Move<vk::VkImageView>						m_colorTargetView;

	// vertex buffer for vertex colors & position
	de::SharedPtr<Buffer>							m_vertexBuffer;

	// vertex buffer with reference data used in VS
	de::SharedPtr<Buffer>							m_vertexRefDataBuffer;

	PipelineCreateInfo::VertexInputState			m_vertexInputState;

	vk::Move<vk::VkCommandPool>						m_cmdPool;
	vk::Move<vk::VkCommandBuffer>					m_cmdBuffer;
	vk::Move<vk::VkCommandBuffer>					m_secCmdBuffer;

	vk::Move<vk::VkFramebuffer>						m_framebuffer;
	vk::Move<vk::VkRenderPass>						m_renderPass;

	const std::string								m_vertexShaderName;
	const std::string								m_fragmentShaderName;

	std::vector<VertexElementData>					m_data;
};

}	// Draw
}	// vkt

#endif // _VKTDRAWBASECLASS_HPP
