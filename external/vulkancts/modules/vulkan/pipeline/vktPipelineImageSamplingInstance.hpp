#ifndef _VKTPIPELINEIMAGESAMPLINGINSTANCE_HPP
#define _VKTPIPELINEIMAGESAMPLINGINSTANCE_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Imagination Technologies Ltd.
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
 * \brief Image sampling case
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"

#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktPipelineImageUtil.hpp"
#include "vktPipelineReferenceRenderer.hpp"
#include "vktPipelineVertexUtil.hpp"
#include "vkPipelineConstructionUtil.hpp"
#include "tcuVectorUtil.hpp"
#include "deSharedPtr.hpp"

namespace vkt
{
namespace pipeline
{

enum AllocationKind
{
	ALLOCATION_KIND_SUBALLOCATED,
	ALLOCATION_KIND_DEDICATED,
};

struct ImageSamplingInstanceParams
{
	ImageSamplingInstanceParams	(const vk::PipelineConstructionType		pipelineConstructionType_,
								 const tcu::UVec2&						renderSize_,
								 vk::VkImageViewType					imageViewType_,
								 vk::VkFormat							imageFormat_,
								 const tcu::IVec3&						imageSize_,
								 int									layerCount_,
								 const vk::VkComponentMapping&			componentMapping_,
								 const vk::VkImageSubresourceRange&		subresourceRange_,
								 const vk::VkSamplerCreateInfo&			samplerParams_,
								 float									samplerLod_,
								 const std::vector<Vertex4Tex4>&		vertices_,
								 bool									separateStencilUsage_ = false,
								 vk::VkDescriptorType					samplingType_ = vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
								 int									imageCount_ = 1,
								 AllocationKind							allocationKind_ = ALLOCATION_KIND_SUBALLOCATED,
								 const vk::VkImageLayout				imageLayout_ = vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
								 const vk::VkPipelineCreateFlags		pipelineCreateFlags_ = 0u)
	: pipelineConstructionType	(pipelineConstructionType_)
	, renderSize				(renderSize_)
	, imageViewType				(imageViewType_)
	, imageFormat				(imageFormat_)
	, imageSize					(imageSize_)
	, layerCount				(layerCount_)
	, componentMapping			(componentMapping_)
	, subresourceRange			(subresourceRange_)
	, samplerParams				(samplerParams_)
	, samplerLod				(samplerLod_)
	, vertices					(vertices_)
	, separateStencilUsage		(separateStencilUsage_)
	, samplingType				(samplingType_)
	, imageCount				(imageCount_)
	, allocationKind			(allocationKind_)
	, imageLayout				(imageLayout_)
	, pipelineCreateFlags		(pipelineCreateFlags_)
	{}

	const vk::PipelineConstructionType	pipelineConstructionType;
	const tcu::UVec2					renderSize;
	vk::VkImageViewType					imageViewType;
	vk::VkFormat						imageFormat;
	const tcu::IVec3					imageSize;
	int									layerCount;
	const vk::VkComponentMapping		componentMapping;
	const vk::VkImageSubresourceRange	subresourceRange;
	const vk::VkSamplerCreateInfo		samplerParams;
	float								samplerLod;
	const std::vector<Vertex4Tex4>		vertices;
	bool								separateStencilUsage;
	vk::VkDescriptorType				samplingType;
	int									imageCount;
	AllocationKind						allocationKind;
	const vk::VkImageLayout				imageLayout;
	const vk::VkPipelineCreateFlags		pipelineCreateFlags;
};

void checkSupportImageSamplingInstance (Context& context, ImageSamplingInstanceParams params);

class ImageSamplingInstance : public vkt::TestInstance
{
public:
												ImageSamplingInstance	(Context&						context,
																		 ImageSamplingInstanceParams	params);

	virtual										~ImageSamplingInstance	(void);

	virtual tcu::TestStatus						iterate					(void);

protected:
	virtual tcu::TestStatus						verifyImage				(void);
	virtual void								setup					(void);

	typedef	vk::Unique<vk::VkImage>				UniqueImage;
	typedef	vk::Unique<vk::VkImageView>			UniqueImageView;
	typedef	de::UniquePtr<vk::Allocation>		UniqueAlloc;
	typedef	de::SharedPtr<UniqueImage>			SharedImagePtr;
	typedef	de::SharedPtr<UniqueImageView>		SharedImageViewPtr;
	typedef	de::SharedPtr<UniqueAlloc>			SharedAllocPtr;

	const AllocationKind						m_allocationKind;
	const vk::VkDescriptorType					m_samplingType;
	const vk::VkImageViewType					m_imageViewType;
	const vk::VkFormat							m_imageFormat;
	const tcu::IVec3							m_imageSize;
	const int									m_layerCount;
	const int									m_imageCount;

	const vk::VkComponentMapping				m_componentMapping;
	tcu::BVec4									m_componentMask;
	const vk::VkImageSubresourceRange			m_subresourceRange;
	const vk::VkSamplerCreateInfo				m_samplerParams;
	const float									m_samplerLod;

	std::vector<SharedImagePtr>					m_images;
	std::vector<SharedAllocPtr>					m_imageAllocs;
	std::vector<SharedImageViewPtr>				m_imageViews;
	vk::Move<vk::VkSampler>						m_sampler;
	de::MovePtr<TestTexture>					m_texture;

	const tcu::UVec2							m_renderSize;
	const vk::VkFormat							m_colorFormat;

	vk::Move<vk::VkDescriptorPool>				m_descriptorPool;
	vk::Move<vk::VkDescriptorSetLayout>			m_descriptorSetLayout;
	vk::Move<vk::VkDescriptorSet>				m_descriptorSet;

	std::vector<SharedImagePtr>					m_colorImages;
	std::vector<SharedAllocPtr>					m_colorImageAllocs;
	std::vector<SharedImageViewPtr>				m_colorAttachmentViews;
	vk::RenderPassWrapper						m_renderPass;

	vk::ShaderWrapper							m_vertexShaderModule;
	vk::ShaderWrapper							m_fragmentShaderModule;

	vk::Move<vk::VkBuffer>						m_vertexBuffer;
	std::vector<Vertex4Tex4>					m_vertices;
	de::MovePtr<vk::Allocation>					m_vertexBufferAlloc;

	vk::PipelineLayoutWrapper					m_preRasterizationStatePipelineLayout;
	vk::PipelineLayoutWrapper					m_fragmentStatePipelineLayout;
	vk::GraphicsPipelineWrapper					m_graphicsPipeline;
	const vk::PipelineConstructionType			m_pipelineConstructionType;

	vk::Move<vk::VkCommandPool>					m_cmdPool;
	vk::Move<vk::VkCommandBuffer>				m_cmdBuffer;

	const vk::VkImageLayout						m_imageLayout;
};

} // pipeline
} // vkt

#endif // _VKTPIPELINEIMAGESAMPLINGINSTANCE_HPP
