#ifndef _VKTGEOMETRYTESTSUTIL_HPP
#define _VKTGEOMETRYTESTSUTIL_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2014 The Android Open Source Project
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
 * \brief Geometry Utilities
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vkObjUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkRef.hpp"
#include "vkPrograms.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vktTestCase.hpp"

#include "tcuVector.hpp"
#include "tcuTexture.hpp"

#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"

namespace vkt
{
namespace geometry
{

struct PrimitiveTestSpec
{
	vk::VkPrimitiveTopology		primitiveType;
	const char*					name;
	vk::VkPrimitiveTopology		outputType;
};

class GraphicsPipelineBuilder
{
public:
								GraphicsPipelineBuilder	(void) : m_renderSize			(0, 0)
															   , m_shaderStageFlags		(0u)
															   , m_cullModeFlags		(vk::VK_CULL_MODE_NONE)
															   , m_frontFace			(vk::VK_FRONT_FACE_COUNTER_CLOCKWISE)
															   , m_patchControlPoints	(1u)
															   , m_blendEnable			(false)
															   , m_primitiveTopology	(vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST) {}

	GraphicsPipelineBuilder&	setRenderSize					(const tcu::IVec2& size) { m_renderSize = size; return *this; }
	GraphicsPipelineBuilder&	setShader						(const vk::DeviceInterface& vk, const vk::VkDevice device, const vk::VkShaderStageFlagBits stage, const vk::ProgramBinary& binary, const vk::VkSpecializationInfo* specInfo);
	GraphicsPipelineBuilder&	setPatchControlPoints			(const deUint32 controlPoints) { m_patchControlPoints = controlPoints; return *this; }
	GraphicsPipelineBuilder&	setCullModeFlags				(const vk::VkCullModeFlags cullModeFlags) { m_cullModeFlags = cullModeFlags; return *this; }
	GraphicsPipelineBuilder&	setFrontFace					(const vk::VkFrontFace frontFace) { m_frontFace = frontFace; return *this; }
	GraphicsPipelineBuilder&	setBlend						(const bool enable) { m_blendEnable = enable; return *this; }

	//! Applies only to pipelines without tessellation shaders.
	GraphicsPipelineBuilder&	setPrimitiveTopology			(const vk::VkPrimitiveTopology topology) { m_primitiveTopology = topology; return *this; }

	GraphicsPipelineBuilder&	addVertexBinding				(const vk::VkVertexInputBindingDescription vertexBinding) { m_vertexInputBindings.push_back(vertexBinding); return *this; }
	GraphicsPipelineBuilder&	addVertexAttribute				(const vk::VkVertexInputAttributeDescription vertexAttribute) { m_vertexInputAttributes.push_back(vertexAttribute); return *this; }

	//! Basic vertex input configuration (uses biding 0, location 0, etc.)
	GraphicsPipelineBuilder&	setVertexInputSingleAttribute	(const vk::VkFormat vertexFormat, const deUint32 stride);

	vk::Move<vk::VkPipeline>	build							(const vk::DeviceInterface& vk, const vk::VkDevice device, const vk::VkPipelineLayout pipelineLayout, const vk::VkRenderPass renderPass);

private:
	tcu::IVec2											m_renderSize;
	vk::Move<vk::VkShaderModule>						m_vertexShaderModule;
	vk::Move<vk::VkShaderModule>						m_fragmentShaderModule;
	vk::Move<vk::VkShaderModule>						m_geometryShaderModule;
	vk::Move<vk::VkShaderModule>						m_tessControlShaderModule;
	vk::Move<vk::VkShaderModule>						m_tessEvaluationShaderModule;
	std::vector<vk::VkPipelineShaderStageCreateInfo>	m_shaderStages;
	std::vector<vk::VkVertexInputBindingDescription>	m_vertexInputBindings;
	std::vector<vk::VkVertexInputAttributeDescription>	m_vertexInputAttributes;
	vk::VkShaderStageFlags								m_shaderStageFlags;
	vk::VkCullModeFlags									m_cullModeFlags;
	vk::VkFrontFace										m_frontFace;
	deUint32											m_patchControlPoints;
	bool												m_blendEnable;
	vk::VkPrimitiveTopology								m_primitiveTopology;

	GraphicsPipelineBuilder (const GraphicsPipelineBuilder&); // "deleted"
	GraphicsPipelineBuilder& operator= (const GraphicsPipelineBuilder&);
};

template<typename T>
inline std::size_t sizeInBytes (const std::vector<T>& vec)
{
	return vec.size() * sizeof(vec[0]);
}

std::string						inputTypeToGLString			(const vk::VkPrimitiveTopology& inputType);
std::string						outputTypeToGLString		(const vk::VkPrimitiveTopology& outputType);
std::size_t						calcOutputVertices			(const vk::VkPrimitiveTopology& inputType);

vk::VkImageCreateInfo			makeImageCreateInfo			(const tcu::IVec2& size, const vk::VkFormat format, const vk::VkImageUsageFlags usage, const deUint32 numArrayLayers = 1u);
vk::VkBufferImageCopy			makeBufferImageCopy			(const vk::VkDeviceSize& bufferOffset, const vk::VkImageSubresourceLayers& imageSubresource, const vk::VkOffset3D& imageOffset, const vk::VkExtent3D& imageExtent);

bool							compareWithFileImage		(Context& context, const tcu::ConstPixelBufferAccess& resultImage, std::string name);

void							fillBuffer					(const vk::DeviceInterface& vk, const vk::VkDevice device, const vk::Allocation& alloc, const vk::VkDeviceSize size, const vk::VkDeviceSize offset, const vk::VkFormat format, const tcu::Vec4& color);
void							fillBuffer					(const vk::DeviceInterface& vk, const vk::VkDevice device, const vk::Allocation& alloc, const vk::VkDeviceSize size, const vk::VkDeviceSize offset, const vk::VkFormat format, const float depth);
vk::VkBool32					checkPointSize				(const vk::InstanceInterface& vki, const vk::VkPhysicalDevice physDevice);

} //vkt
} //geometry

#endif // _VKTGEOMETRYTESTSUTIL_HPP
