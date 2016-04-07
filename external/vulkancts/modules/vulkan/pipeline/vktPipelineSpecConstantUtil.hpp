#ifndef _VKTPIPELINESPECCONSTANTUTIL_HPP
#define _VKTPIPELINESPECCONSTANTUTIL_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \brief Pipeline specialization constants test utilities
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkPrograms.hpp"
#include "vkMemUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"

namespace vkt
{
namespace pipeline
{

class Buffer
{
public:
										Buffer			(const vk::DeviceInterface&		vk,
														 const vk::VkDevice				device,
														 vk::Allocator&					allocator,
														 const vk::VkBufferCreateInfo&	bufferCreateInfo,
														 const vk::MemoryRequirement	memoryRequirement)

											: m_buffer		(createBuffer(vk, device, &bufferCreateInfo))
											, m_allocation	(allocator.allocate(getBufferMemoryRequirements(vk, device, *m_buffer), memoryRequirement))
										{
											VK_CHECK(vk.bindBufferMemory(device, *m_buffer, m_allocation->getMemory(), m_allocation->getOffset()));
										}

	const vk::VkBuffer&					get				(void) const { return *m_buffer; }
	const vk::VkBuffer&					operator*		(void) const { return get(); }
	vk::Allocation&						getAllocation	(void) const { return *m_allocation; }

private:
	const vk::Unique<vk::VkBuffer>		m_buffer;
	const de::UniquePtr<vk::Allocation>	m_allocation;

	// "deleted"
										Buffer			(const Buffer&);
	Buffer&								operator=		(const Buffer&);
};

class Image
{
public:
										Image			(const vk::DeviceInterface&		vk,
														 const vk::VkDevice				device,
														 vk::Allocator&					allocator,
														 const vk::VkImageCreateInfo&	imageCreateInfo,
														 const vk::MemoryRequirement	memoryRequirement)

											: m_image		(createImage(vk, device, &imageCreateInfo))
											, m_allocation	(allocator.allocate(getImageMemoryRequirements(vk, device, *m_image), memoryRequirement))
										{
											VK_CHECK(vk.bindImageMemory(device, *m_image, m_allocation->getMemory(), m_allocation->getOffset()));
										}

	const vk::VkImage&					get				(void) const { return *m_image; }
	const vk::VkImage&					operator*		(void) const { return get(); }
	vk::Allocation&						getAllocation	(void) const { return *m_allocation; }

private:
	const vk::Unique<vk::VkImage>		m_image;
	const de::UniquePtr<vk::Allocation>	m_allocation;

	// "deleted"
										Image			(const Image&);
	Image&								operator=		(const Image&);
};

class GraphicsPipelineBuilder
{
public:
														GraphicsPipelineBuilder			(void) : m_renderSize		(16, 16)
																							   , m_shaderStageFlags	(0u) {}

	GraphicsPipelineBuilder&							setRenderSize					(const tcu::IVec2& size) { m_renderSize = size; return *this; }
	GraphicsPipelineBuilder&							setShader						(const vk::DeviceInterface& vk, const vk::VkDevice device, const vk::VkShaderStageFlagBits stage, const vk::ProgramBinary& binary, const vk::VkSpecializationInfo* specInfo);
	vk::Move<vk::VkPipeline>							build							(const vk::DeviceInterface& vk, const vk::VkDevice device, const vk::VkPipelineLayout pipelineLayout, const vk::VkRenderPass renderPass);

private:
	tcu::IVec2											m_renderSize;
	vk::Move<vk::VkShaderModule>						m_vertexShaderModule;
	vk::Move<vk::VkShaderModule>						m_fragmentShaderModule;
	vk::Move<vk::VkShaderModule>						m_geometryShaderModule;
	vk::Move<vk::VkShaderModule>						m_tessControlShaderModule;
	vk::Move<vk::VkShaderModule>						m_tessEvaluationShaderModule;
	std::vector<vk::VkPipelineShaderStageCreateInfo>	m_shaderStages;
	vk::VkShaderStageFlags								m_shaderStageFlags;

														GraphicsPipelineBuilder			(const GraphicsPipelineBuilder&); // "deleted"
	GraphicsPipelineBuilder&							operator=						(const GraphicsPipelineBuilder&);
};

enum FeatureFlagBits
{
	FEATURE_TESSELLATION_SHADER					= 1u << 0,
	FEATURE_GEOMETRY_SHADER						= 1u << 1,
	FEATURE_SHADER_FLOAT_64						= 1u << 2,
	FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS	= 1u << 3,
	FEATURE_FRAGMENT_STORES_AND_ATOMICS			= 1u << 4,
};
typedef deUint32 FeatureFlags;

vk::VkBufferCreateInfo			makeBufferCreateInfo	(const vk::VkDeviceSize bufferSize, const vk::VkBufferUsageFlags usage);
vk::VkImageCreateInfo			makeImageCreateInfo		(const tcu::IVec2& size, const vk::VkFormat format, const vk::VkImageUsageFlags usage);
vk::Move<vk::VkCommandPool>		makeCommandPool			(const vk::DeviceInterface& vk, const vk::VkDevice device, const deUint32 queueFamilyIndex);
vk::Move<vk::VkCommandBuffer>	makeCommandBuffer		(const vk::DeviceInterface& vk, const vk::VkDevice device, const vk::VkCommandPool commandPool);
vk::Move<vk::VkDescriptorSet>	makeDescriptorSet		(const vk::DeviceInterface& vk, const vk::VkDevice device, const vk::VkDescriptorPool descriptorPool, const vk::VkDescriptorSetLayout setLayout);
vk::Move<vk::VkPipelineLayout>	makePipelineLayout		(const vk::DeviceInterface& vk, const vk::VkDevice device, const vk::VkDescriptorSetLayout descriptorSetLayout);
vk::Move<vk::VkPipeline>		makeComputePipeline		(const vk::DeviceInterface& vk, const vk::VkDevice device, const vk::VkPipelineLayout pipelineLayout, const vk::VkShaderModule shaderModule, const vk::VkSpecializationInfo* specInfo);
vk::Move<vk::VkRenderPass>		makeRenderPass			(const vk::DeviceInterface& vk, const vk::VkDevice device, const vk::VkFormat colorFormat);
vk::Move<vk::VkFramebuffer>		makeFramebuffer			(const vk::DeviceInterface& vk, const vk::VkDevice device, const vk::VkRenderPass renderPass, const vk::VkImageView colorAttachment, const deUint32 width, const deUint32 height);
vk::Move<vk::VkImageView>		makeImageView			(const vk::DeviceInterface& vk, const vk::VkDevice vkDevice, const vk::VkImage image, const vk::VkImageViewType viewType, const vk::VkFormat format);
vk::VkBufferMemoryBarrier		makeBufferMemoryBarrier	(const vk::VkAccessFlags srcAccessMask, const vk::VkAccessFlags dstAccessMask, const vk::VkBuffer buffer, const vk::VkDeviceSize offset, const vk::VkDeviceSize bufferSizeBytes);
vk::VkImageMemoryBarrier		makeImageMemoryBarrier	(const vk::VkAccessFlags srcAccessMask, const vk::VkAccessFlags dstAccessMask, const vk::VkImageLayout oldLayout, const vk::VkImageLayout newLayout, const vk::VkImage image, const vk::VkImageSubresourceRange subresourceRange);
void							beginCommandBuffer		(const vk::DeviceInterface& vk, const vk::VkCommandBuffer commandBuffer);
void							endCommandBuffer		(const vk::DeviceInterface& vk, const vk::VkCommandBuffer commandBuffer);
void							submitCommandsAndWait	(const vk::DeviceInterface& vk, const vk::VkDevice device, const vk::VkQueue queue, const vk::VkCommandBuffer commandBuffer);
void							beginRenderPass			(const vk::DeviceInterface& vk, const vk::VkCommandBuffer commandBuffer, const vk::VkRenderPass renderPass, const vk::VkFramebuffer framebuffer, const vk::VkRect2D& renderArea, const tcu::Vec4& clearColor);
void							endRenderPass			(const vk::DeviceInterface& vk, const vk::VkCommandBuffer commandBuffer);
void							requireFeatures			(const vk::InstanceInterface& vki, const vk::VkPhysicalDevice physDevice, const FeatureFlags flags);

// Ugly, brute-force replacement for the initializer list

template<typename T>
std::vector<T> makeVector (const T& o1)
{
	std::vector<T> vec;
	vec.reserve(1);
	vec.push_back(o1);
	return vec;
}

template<typename T>
std::vector<T> makeVector (const T& o1, const T& o2)
{
	std::vector<T> vec;
	vec.reserve(2);
	vec.push_back(o1);
	vec.push_back(o2);
	return vec;
}

template<typename T>
std::vector<T> makeVector (const T& o1, const T& o2, const T& o3)
{
	std::vector<T> vec;
	vec.reserve(3);
	vec.push_back(o1);
	vec.push_back(o2);
	vec.push_back(o3);
	return vec;
}

template<typename T>
std::vector<T> makeVector (const T& o1, const T& o2, const T& o3, const T& o4)
{
	std::vector<T> vec;
	vec.reserve(4);
	vec.push_back(o1);
	vec.push_back(o2);
	vec.push_back(o3);
	vec.push_back(o4);
	return vec;
}

} // pipeline
} // vkt

#endif // _VKTPIPELINESPECCONSTANTUTIL_HPP
