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
#include "vkTypeUtil.hpp"
#include "rrRenderer.hpp"
#include <memory>

namespace vkt
{
namespace drawutil
{

struct FrameBufferState
{
	FrameBufferState()										= delete;
	FrameBufferState(deUint32 renderWidth_, deUint32 renderHeight_);

	vk::VkFormat					colorFormat				= vk::VK_FORMAT_R8G8B8A8_UNORM;
	vk::VkFormat					depthFormat				= vk::VK_FORMAT_UNDEFINED;
	tcu::UVec2						renderSize;
	deUint32						numSamples				= vk::VK_SAMPLE_COUNT_1_BIT;
	vk::VkImageView					depthImageView			= 0;
};

struct PipelineState
{
	PipelineState()											= delete;
	PipelineState(const int subpixelBits);

	bool							depthClampEnable		= false;
	bool							depthTestEnable			= false;
	bool							depthWriteEnable		= false;
	rr::TestFunc					compareOp				= rr::TESTFUNC_LESS;
	bool							depthBoundsTestEnable	= false;
	bool							blendEnable				= false;
	float							lineWidth				= 1.0;
	deUint32						numPatchControlPoints	= 0;
	bool							sampleShadingEnable		= false;
	int								subpixelBits;

	// VK_EXT_depth_clip_enable
	bool							explicitDepthClipEnable	= false;
	bool							depthClipEnable			= false;
};

struct DrawCallData
{
	DrawCallData()											= delete;
	DrawCallData(const vk::VkPrimitiveTopology topology_, const std::vector<tcu::Vec4>&	vertices_);

	vk::VkPrimitiveTopology			topology;
	const std::vector<tcu::Vec4>&	vertices;
};

//! Sets up a graphics pipeline and enables simple draw calls to predefined attachments.
//! Clip volume uses wc = 1.0, which gives clip coord ranges: x = [-1, 1], y = [-1, 1], z = [0, 1]
//! Clip coords (-1,-1) map to viewport coords (0, 0).
class DrawContext
{
public:
											DrawContext				(const FrameBufferState&		framebufferState)
		: m_framebufferState					(framebufferState)
	{
	}
	virtual									~DrawContext			(void)
	{
	}

	virtual void							draw					(void) = 0;
	virtual tcu::ConstPixelBufferAccess		getColorPixels			(void) const = 0;
protected:
	const FrameBufferState&					m_framebufferState;
	std::vector<PipelineState>				m_pipelineStates;
	std::vector<DrawCallData>				m_drawCallData;
};

class ReferenceDrawContext : public DrawContext
{
public:
	ReferenceDrawContext											(const FrameBufferState&		framebufferState );
	virtual									~ReferenceDrawContext	(void);

	void									registerDrawObject		(const PipelineState&					pipelineState,
																	 std::shared_ptr<rr::VertexShader>&		vertexShader,
																	 std::shared_ptr<rr::FragmentShader>&	fragmentShader,
																	 const DrawCallData&					drawCallData);
	virtual void							draw					(void);
	virtual tcu::ConstPixelBufferAccess		getColorPixels			(void) const;
private:
	std::vector<std::shared_ptr<rr::VertexShader>>		m_vertexShaders;
	std::vector< std::shared_ptr<rr::FragmentShader>>	m_fragmentShaders;
	tcu::TextureLevel									m_refImage;
};

struct VulkanShader
{
	VulkanShader	() = delete;
	VulkanShader	(const vk::VkShaderStageFlagBits stage_, const vk::ProgramBinary& binary_);

	vk::VkShaderStageFlagBits	stage;
	const vk::ProgramBinary*	binary;
};

struct VulkanProgram
{
	VulkanProgram	(const std::vector<VulkanShader>& shaders_);

	std::vector<VulkanShader>	shaders;
	vk::VkDescriptorSetLayout	descriptorSetLayout	= 0;
	vk::VkDescriptorSet			descriptorSet		= 0;
};

struct RenderObject
{
	enum VulkanContants
	{
		MAX_NUM_SHADER_MODULES = 5,
	};

	vk::refdetails::Move<vk::VkPipelineLayout>	pipelineLayout;
	vk::refdetails::Move<vk::VkPipeline>		pipeline;
	vk::refdetails::Move<vk::VkShaderModule>	shaderModules[MAX_NUM_SHADER_MODULES];
	de::MovePtr<vk::BufferWithMemory>			vertexBuffer;
	deUint32									vertexCount;
	vk::VkDescriptorSetLayout					descriptorSetLayout = 0;
	vk::VkDescriptorSet							descriptorSet = 0;

};

class VulkanDrawContext : public DrawContext
{
public:
	VulkanDrawContext											(Context&					context,
																 const FrameBufferState&	frameBufferState);
	virtual									~VulkanDrawContext	(void);

	void									registerDrawObject	(const PipelineState&		pipelineState,
																 const VulkanProgram&		vulkanProgram,
																 const DrawCallData&		drawCallData);

	virtual void							draw				(void);
	virtual tcu::ConstPixelBufferAccess		getColorPixels		(void) const;
private:
	Context&									m_context;
	de::MovePtr<vk::ImageWithMemory>			m_colorImage;
	de::MovePtr<vk::ImageWithMemory>			m_resolveImage;
	de::MovePtr<vk::ImageWithMemory>			m_depthImage;
	de::MovePtr<vk::BufferWithMemory>			m_colorAttachmentBuffer;
	vk::refdetails::Move<vk::VkImageView>		m_colorImageView;
	vk::refdetails::Move<vk::VkImageView>		m_depthImageView;
	vk::refdetails::Move<vk::VkRenderPass>		m_renderPass;
	vk::refdetails::Move<vk::VkFramebuffer>		m_framebuffer;
	vk::refdetails::Move<vk::VkCommandPool>		m_cmdPool;
	vk::refdetails::Move<vk::VkCommandBuffer>	m_cmdBuffer;

	std::vector<std::shared_ptr<RenderObject>>	m_renderObjects;
};

std::string getPrimitiveTopologyShortName (const vk::VkPrimitiveTopology topology);

} // drawutil
} // vkt

#endif // _VKTDRAWUTIL_HPP
