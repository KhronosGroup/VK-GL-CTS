#ifndef _VKTDRAWUTIL_HPP
#define _VKTDRAWUTIL_HPP
/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
 * Copyright (c) 2016 Google Inc.
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
 * \brief Utility for generating simple work
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"

#include "deUniquePtr.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkImageUtil.hpp"
#include "vkPrograms.hpp"
#include "vktTestCase.hpp"

namespace vkt
{
namespace drawutil
{

enum Constants
{
	RENDER_SIZE								= 16,
	RENDER_SIZE_LARGE						= 128,
	NUM_RENDER_PIXELS						= RENDER_SIZE * RENDER_SIZE,
	NUM_PATCH_CONTROL_POINTS				= 3,
	MAX_NUM_SHADER_MODULES					= 5,
	MAX_CLIP_DISTANCES						= 8,
	MAX_CULL_DISTANCES						= 8,
	MAX_COMBINED_CLIP_AND_CULL_DISTANCES	= 8,
};

struct Shader
{
	vk::VkShaderStageFlagBits	stage;
	const vk::ProgramBinary*	binary;

	Shader (const vk::VkShaderStageFlagBits stage_, const vk::ProgramBinary& binary_)
		: stage		(stage_)
		, binary	(&binary_)
	{
	}
};
//! Sets up a graphics pipeline and enables simple draw calls to predefined attachments.
//! Clip volume uses wc = 1.0, which gives clip coord ranges: x = [-1, 1], y = [-1, 1], z = [0, 1]
//! Clip coords (-1,-1) map to viewport coords (0, 0).
class DrawContext
{
public:
									DrawContext		(Context&						context,
													 const std::vector<Shader>&		shaders,
													 const std::vector<tcu::Vec4>&	vertices,
													 const vk::VkPrimitiveTopology	primitiveTopology,
													 const deUint32					renderSize			= static_cast<deUint32>(RENDER_SIZE),
													 const bool						depthClampEnable	= false,
													 const bool						blendEnable			= false,
													 const float					lineWidth			= 1.0f);

	void							draw			(void);
	tcu::ConstPixelBufferAccess		getColorPixels	(void) const;

private:
	Context&									m_context;
	const vk::VkFormat							m_colorFormat;
	const vk::VkFormat							m_depthFormat;
	const vk::VkImageSubresourceRange			m_colorSubresourceRange;
	const vk::VkImageSubresourceRange			m_depthSubresourceRange;
	const tcu::UVec2							m_renderSize;
	const vk::VkExtent3D						m_imageExtent;
	const vk::VkPrimitiveTopology				m_primitiveTopology;
	const bool									m_depthClampEnable;
	const bool									m_blendEnable;
	const deUint32								m_numVertices;
	const float									m_lineWidth;
	const deUint32								m_numPatchControlPoints;
	de::MovePtr<vk::BufferWithMemory>			m_vertexBuffer;
	de::MovePtr<vk::ImageWithMemory>			m_colorImage;
	de::MovePtr<vk::ImageWithMemory>			m_depthImage;
	de::MovePtr<vk::BufferWithMemory>			m_colorAttachmentBuffer;
	vk::refdetails::Move<vk::VkImageView>		m_colorImageView;
	vk::refdetails::Move<vk::VkImageView>		m_depthImageView;
	vk::refdetails::Move<vk::VkRenderPass>		m_renderPass;
	vk::refdetails::Move<vk::VkFramebuffer>		m_framebuffer;
	vk::refdetails::Move<vk::VkPipelineLayout>	m_pipelineLayout;
	vk::refdetails::Move<vk::VkPipeline>		m_pipeline;
	vk::refdetails::Move<vk::VkCommandPool>		m_cmdPool;
	vk::refdetails::Move<vk::VkCommandBuffer>	m_cmdBuffer;
	vk::refdetails::Move<vk::VkShaderModule>	m_shaderModules[MAX_NUM_SHADER_MODULES];

									DrawContext		(const DrawContext&);	// "deleted"
	DrawContext&					operator=		(const DrawContext&);	// "deleted"
};
std::string getPrimitiveTopologyShortName (const vk::VkPrimitiveTopology topology);

} // drwwutil
} // vkt

#endif // _VKTDRAWUTIL_HPP
