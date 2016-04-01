#ifndef _VKTPIPELINEIMAGESAMPLINGINSTANCE_HPP
#define _VKTPIPELINEIMAGESAMPLINGINSTANCE_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Imagination Technologies Ltd.
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
 * \brief Image sampling case
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"

#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktPipelineImageUtil.hpp"
#include "vktPipelineReferenceRenderer.hpp"
#include "vktPipelineVertexUtil.hpp"
#include "tcuVectorUtil.hpp"

namespace vkt
{
namespace pipeline
{

class ImageSamplingInstance : public vkt::TestInstance
{
public:
												ImageSamplingInstance	(Context&							context,
																		 const tcu::UVec2&					renderSize,
																		 vk::VkImageViewType				imageViewType,
																		 vk::VkFormat						imageFormat,
																		 const tcu::IVec3&					imageSize,
																		 int								layerCount,
																		 const vk::VkComponentMapping&		componentMapping,
																		 const vk::VkImageSubresourceRange&	subresourceRange,
																		 const vk::VkSamplerCreateInfo&		samplerParams,
																		 float								samplerLod,
																		 const std::vector<Vertex4Tex4>&	vertices);

	virtual										~ImageSamplingInstance	(void);

	virtual tcu::TestStatus						iterate					(void);

protected:
	tcu::TestStatus								verifyImage				(void);

private:
	const vk::VkImageViewType					m_imageViewType;
	const vk::VkFormat							m_imageFormat;
	const tcu::IVec3							m_imageSize;
	const int									m_layerCount;

	const vk::VkComponentMapping				m_componentMapping;
	const vk::VkImageSubresourceRange			m_subresourceRange;
	const vk::VkSamplerCreateInfo				m_samplerParams;
	const float									m_samplerLod;

	vk::Move<vk::VkImage>						m_image;
	de::MovePtr<vk::Allocation>					m_imageAlloc;
	vk::Move<vk::VkImageView>					m_imageView;
	vk::Move<vk::VkSampler>						m_sampler;
	de::MovePtr<TestTexture>					m_texture;

	const tcu::UVec2							m_renderSize;
	const vk::VkFormat							m_colorFormat;

	vk::Move<vk::VkDescriptorPool>				m_descriptorPool;
	vk::Move<vk::VkDescriptorSetLayout>			m_descriptorSetLayout;
	vk::Move<vk::VkDescriptorSet>				m_descriptorSet;

	vk::Move<vk::VkImage>						m_colorImage;
	de::MovePtr<vk::Allocation>					m_colorImageAlloc;
	vk::Move<vk::VkImageView>					m_colorAttachmentView;
	vk::Move<vk::VkRenderPass>					m_renderPass;
	vk::Move<vk::VkFramebuffer>					m_framebuffer;

	vk::Move<vk::VkShaderModule>				m_vertexShaderModule;
	vk::Move<vk::VkShaderModule>				m_fragmentShaderModule;

	vk::Move<vk::VkBuffer>						m_vertexBuffer;
	std::vector<Vertex4Tex4>					m_vertices;
	de::MovePtr<vk::Allocation>					m_vertexBufferAlloc;

	vk::Move<vk::VkPipelineLayout>				m_pipelineLayout;
	vk::Move<vk::VkPipeline>					m_graphicsPipeline;

	vk::Move<vk::VkCommandPool>					m_cmdPool;
	vk::Move<vk::VkCommandBuffer>				m_cmdBuffer;

	vk::Move<vk::VkFence>						m_fence;
};

} // pipeline
} // vkt

#endif // _VKTPIPELINEIMAGESAMPLINGINSTANCE_HPP
