/*------------------------------------------------------------------------
* Vulkan Conformance Tests
* ------------------------
*
* Copyright (c) 2023 The Khronos Group Inc.
* Copyright (c) 2023 LunarG, Inc.
* Copyright (c) 2023 Google LLC
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
*//*
 * \file
 * \brief Extended dynamic state tests
*//*--------------------------------------------------------------------*/

#include "vktPipelineBindVertexBuffers2Tests.hpp"
#include "deUniquePtr.hpp"
#include "tcuTestCase.hpp"
#include "vktTestCase.hpp"
#include "vkTypeUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkImageUtil.hpp"
#include "vktPipelineClearUtil.hpp"
#include "vkBufferWithMemory.hpp"

namespace vkt
{
namespace pipeline
{

struct TestParams
{
	deUint32	colorStride;
	deUint32	vertexStride;
	deUint32	colorOffset;
	deUint32	vertexOffset;
};

vk::VkImageCreateInfo makeImageCreateInfo (const vk::VkExtent2D extent, const vk::VkFormat format, const vk::VkImageUsageFlags usage)
{
	const vk::VkImageCreateInfo	imageParams	=
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,			// VkStructureType			sType;
		DE_NULL,											// const void*				pNext;
		(vk::VkImageCreateFlags)0u,							// VkImageCreateFlags		flags;
		vk::VK_IMAGE_TYPE_2D,								// VkImageType				imageType;
		format,												// VkFormat					format;
		vk::makeExtent3D(extent.width, extent.height, 1),	// VkExtent3D				extent;
		1u,													// deUint32					mipLevels;
		1u,													// deUint32					arrayLayers;
		vk::VK_SAMPLE_COUNT_1_BIT,							// VkSampleCountFlagBits	samples;
		vk::VK_IMAGE_TILING_OPTIMAL,						// VkImageTiling			tiling;
		usage,												// VkImageUsageFlags		usage;
		vk::VK_SHARING_MODE_EXCLUSIVE,						// VkSharingMode			sharingMode;
		0u,													// deUint32					queueFamilyIndexCount;
		DE_NULL,											// const deUint32*			pQueueFamilyIndices;
		vk::VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout			initialLayout;
	};
	return imageParams;
}

//make a buffer to read an image back after rendering
std::unique_ptr<vk::BufferWithMemory> makeBufferForImage (const vk::DeviceInterface& vkd, const vk::VkDevice device, vk::Allocator& allocator, tcu::TextureFormat tcuFormat, vk::VkExtent2D imageExtent)
{
	const auto	outBufferSize		= static_cast<vk::VkDeviceSize>(static_cast<uint32_t>(tcu::getPixelSize(tcuFormat)) * imageExtent.width * imageExtent.height);
	const auto	outBufferUsage		= vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	const auto	outBufferInfo		= makeBufferCreateInfo(outBufferSize, outBufferUsage);
	auto outBuffer = std::unique_ptr<vk::BufferWithMemory>(new vk::BufferWithMemory(vkd, device, allocator, outBufferInfo, vk::MemoryRequirement::HostVisible));

	return outBuffer;
}

vk::VkVertexInputBindingDescription makeBindingDescription(uint32_t binding, uint32_t stride, vk::VkVertexInputRate inputRate)
{
	vk::VkVertexInputBindingDescription bindingDescription;

	bindingDescription.binding = binding;
	bindingDescription.stride = stride;
	bindingDescription.inputRate = inputRate;

	return bindingDescription;
}

vk::VkVertexInputAttributeDescription makeAttributeDescription (uint32_t location, uint32_t binding, vk::VkFormat format, uint32_t offset)
{
	vk::VkVertexInputAttributeDescription attributeDescription;

	attributeDescription.location = location;
	attributeDescription.binding = binding;
	attributeDescription.format = format;
	attributeDescription.offset = offset;

	return attributeDescription;
}

void copyAndFlush(const vk::DeviceInterface& vkd, vk::VkDevice device, vk::BufferWithMemory& buffer, size_t offset, const void* src, size_t size)
{
	auto&	alloc	= buffer.getAllocation();
	auto	dst		= reinterpret_cast<char*>(alloc.getHostPtr());

	deMemcpy(dst + offset, src, size);
	vk::flushAlloc(vkd, device, alloc);
}

class BindBuffers2Instance : public vkt::TestInstance
{
public:
						BindBuffers2Instance		(Context& context, const vk::PipelineConstructionType	pipelineConstructionType, const TestParams params, const bool singleBind, const deUint32 count)
							: vkt::TestInstance				(context)
							, m_pipelineConstructionType	(pipelineConstructionType)
							, m_params						(params)
							, m_singleBind					(singleBind)
							, m_count						(count)
							{}
	virtual				~BindBuffers2Instance		(void) {}

	tcu::TestStatus		iterate						(void) override;

private:
	const vk::PipelineConstructionType	m_pipelineConstructionType;
	const TestParams					m_params;
	const bool							m_singleBind;
	const deUint32						m_count;
};

tcu::TestStatus BindBuffers2Instance::iterate (void)
{
	const vk::VkInstance			instance			= m_context.getInstance();
	const vk::InstanceDriver		instanceDriver		(m_context.getPlatformInterface(), instance);
	const vk::InstanceInterface&	vki					= m_context.getInstanceInterface();
	const vk::DeviceInterface&		vk					= m_context.getDeviceInterface();
	const vk::VkPhysicalDevice		physicalDevice		= m_context.getPhysicalDevice();
	const vk::VkDevice				device				= m_context.getDevice();
	const vk::VkQueue				queue				= m_context.getUniversalQueue();
	const deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	vk::Allocator&					allocator			= m_context.getDefaultAllocator();
	const auto&						deviceExtensions	= m_context.getDeviceExtensions();

	vk::VkExtent2D					extent				= {	32u, 32u };

	const std::vector<vk::VkViewport>	viewports	{ vk::makeViewport(extent) };
	const std::vector<vk::VkRect2D>		scissors	{ vk::makeRect2D(extent) };

	const vk::VkPipelineLayoutCreateInfo		pipelineLayoutInfo		=
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// VkStructureType				sType;
		DE_NULL,											// const void*					pNext;
		0u,													// VkPipelineLayoutCreateFlags	flags;
		0u,													// deUint32						descriptorSetCount;
		DE_NULL,											// const VkDescriptorSetLayout*	pSetLayouts;
		0u,													// deUint32						pushConstantRangeCount;
		DE_NULL												// const VkPushDescriptorRange*	pPushDescriptorRanges;
	};

	const vk::VkImageSubresourceRange				colorSubresourceRange = vk::makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const vk::Move<vk::VkImage>						colorImage			(makeImage(vk, device, makeImageCreateInfo(extent, vk::VK_FORMAT_R32G32B32A32_SFLOAT, vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT)));
	const de::MovePtr<vk::Allocation>				colorImageAlloc		(bindImage(vk, device, allocator, *colorImage, vk::MemoryRequirement::Any));
	const vk::Move<vk::VkImageView>					colorImageView		(makeImageView(vk, device, *colorImage, vk::VK_IMAGE_VIEW_TYPE_2D, vk::VK_FORMAT_R32G32B32A32_SFLOAT, colorSubresourceRange));

	const vk::PipelineLayoutWrapper					pipelineLayout		(m_pipelineConstructionType, vk, device, &pipelineLayoutInfo);
	vk::RenderPassWrapper							renderPass			(m_pipelineConstructionType, vk, device, vk::VK_FORMAT_R32G32B32A32_SFLOAT);
	renderPass.createFramebuffer(vk, device, *colorImage, *colorImageView, extent.width, extent.height);
	const vk::ShaderWrapper							vertShaderModule	= vk::ShaderWrapper(vk, device, m_context.getBinaryCollection().get("vert"));
	const vk::ShaderWrapper							fragShaderModule	= vk::ShaderWrapper(vk, device, m_context.getBinaryCollection().get("frag"));

	//buffer to read the output image
	auto											outBuffer			= makeBufferForImage(vk, device, allocator, mapVkFormat(vk::VK_FORMAT_R32G32B32A32_SFLOAT), extent);
	auto&											outBufferAlloc		= outBuffer->getAllocation();

	std::vector<vk::VkVertexInputAttributeDescription>		attributes;
	if (m_count == 2)
	{
		attributes = {
			makeAttributeDescription(0, 0, vk::VK_FORMAT_R32G32_SFLOAT, 0),
			makeAttributeDescription(1, 1, vk::VK_FORMAT_R32G32_SFLOAT, 0),
			makeAttributeDescription(2, 2, vk::VK_FORMAT_R32G32_SFLOAT, 0),
			makeAttributeDescription(3, 3, vk::VK_FORMAT_R32G32_SFLOAT, 0),
		};
	}
	else if (m_count == 3)
	{
		attributes = {
			makeAttributeDescription(0, 0, vk::VK_FORMAT_R32G32_SFLOAT, 0),
			makeAttributeDescription(1, 1, vk::VK_FORMAT_R32G32_SFLOAT, 0),
			makeAttributeDescription(2, 2, vk::VK_FORMAT_R32_SFLOAT, 0),
			makeAttributeDescription(3, 3, vk::VK_FORMAT_R32_SFLOAT, 0),
			makeAttributeDescription(4, 4, vk::VK_FORMAT_R32_SFLOAT, 0),
			makeAttributeDescription(5, 5, vk::VK_FORMAT_R32_SFLOAT, 0),
		};
	}
	else if (m_count == 4)
	{
		attributes = {
			makeAttributeDescription(0, 0, vk::VK_FORMAT_R32_SFLOAT, 0),
			makeAttributeDescription(1, 1, vk::VK_FORMAT_R32_SFLOAT, 0),
			makeAttributeDescription(2, 2, vk::VK_FORMAT_R32_SFLOAT, 0),
			makeAttributeDescription(3, 3, vk::VK_FORMAT_R32_SFLOAT, 0),
			makeAttributeDescription(4, 4, vk::VK_FORMAT_R32_SFLOAT, 0),
			makeAttributeDescription(5, 5, vk::VK_FORMAT_R32_SFLOAT, 0),
			makeAttributeDescription(6, 6, vk::VK_FORMAT_R32_SFLOAT, 0),
			makeAttributeDescription(7, 7, vk::VK_FORMAT_R32_SFLOAT, 0),
		};
	}
	else
	{
		attributes = {
			makeAttributeDescription(0, 0, vk::VK_FORMAT_R32G32B32A32_SFLOAT, 0),
			makeAttributeDescription(1, 1, vk::VK_FORMAT_R32G32B32A32_SFLOAT, 0),
		};
	}
	std::vector<vk::VkVertexInputBindingDescription>		bindings;
	for (deUint32 i = 0; i < m_count; ++i)
	{
		bindings.push_back(makeBindingDescription(i * 2, 99 /*ignored*/, vk::VK_VERTEX_INPUT_RATE_INSTANCE));
		bindings.push_back(makeBindingDescription(i * 2 + 1, 99 /*ignored*/, vk::VK_VERTEX_INPUT_RATE_VERTEX));
	};

	vk::VkPipelineVertexInputStateCreateInfo		vertexInputState	= vk::initVulkanStructure();
	vertexInputState.vertexBindingDescriptionCount = (deUint32)bindings.size();
	vertexInputState.pVertexBindingDescriptions = bindings.data();
	vertexInputState.vertexAttributeDescriptionCount = (deUint32)attributes.size();
	vertexInputState.pVertexAttributeDescriptions = attributes.data();

	vk::VkPipelineInputAssemblyStateCreateInfo		inputAssemblyState	= vk::initVulkanStructure();
	inputAssemblyState.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

	const vk::VkDynamicState						dynamicState		= vk::VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE_EXT;

	const vk::VkPipelineDynamicStateCreateInfo		dynamicStateInfo	=
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,	//	VkStructureType						sType;
		nullptr,													//	const void*							pNext;
		0u,															//	VkPipelineDynamicStateCreateFlags	flags;
		1u,															//	uint32_t							dynamicStateCount;
		&dynamicState,												//	const VkDynamicState*				pDynamicStates;
	};

	vk::GraphicsPipelineWrapper	graphicsPipelineWrapper	{ vki, vk, physicalDevice, device, deviceExtensions, m_pipelineConstructionType };
	graphicsPipelineWrapper.setDefaultDepthStencilState()
		.setDefaultColorBlendState()
		.setDefaultRasterizationState()
		.setDefaultMultisampleState()
		.setDynamicState(&dynamicStateInfo)
		.setupVertexInputState(&vertexInputState, &inputAssemblyState)
		.setupPreRasterizationShaderState(viewports,
			scissors,
			pipelineLayout,
			renderPass.get(),
			0u,
			vertShaderModule)
		.setupFragmentShaderState(pipelineLayout,
			renderPass.get(),
			0u,
			fragShaderModule)
		.setupFragmentOutputState(renderPass.get())
		.setMonolithicPipelineLayout(pipelineLayout)
		.buildPipeline();

	const vk::Move<vk::VkCommandPool>				cmdPool			(createCommandPool(vk, device, vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const vk::Move<vk::VkCommandBuffer>				cmdBuffer		(allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	deUint32										instanceCount	= 4u;
	vk::VkDeviceSize								colorStride		= m_params.colorStride * sizeof(float);
	vk::VkDeviceSize								colorOffset		= m_params.colorOffset * sizeof(float);
	vk::VkDeviceSize								vertexStride	= m_params.vertexStride * sizeof(float);
	vk::VkDeviceSize								vertexOffset	= m_params.vertexOffset * sizeof(float);

	tcu::Vec4										colors[]		=
	{
		tcu::Vec4(0.21f, 0.41f, 0.61f, 0.81f),
		tcu::Vec4(0.22f, 0.42f, 0.62f, 0.82f),
		tcu::Vec4(0.23f, 0.43f, 0.63f, 0.83f),
		tcu::Vec4(0.24f, 0.44f, 0.64f, 0.84f),
	};

	tcu::Vec4										vertices[] =
	{
		tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f),
		tcu::Vec4(0.0f, 1.0f, 0.0f, 0.0f),
		tcu::Vec4(1.0f, 0.0f, 0.0f, 0.0f),
		tcu::Vec4(1.0f, 1.0f, 0.0f, 0.0f),
	};

	std::vector<float>								colorData;
	for (deUint32 i = 0; i < colorOffset / sizeof(float); ++i)
		colorData.push_back(0);

	for (deUint32 i = 0; i < 4; ++i)
	{
		colorData.push_back(colors[i].x());
		colorData.push_back(colors[i].y());
		colorData.push_back(colors[i].z());
		colorData.push_back(colors[i].w());
		for (deUint32 j = 4; j < colorStride / sizeof(float); ++j)
			colorData.push_back(0.0f);
	}

	std::vector<float>								vertexData;
	for (deUint32 i = 0; i < vertexOffset / sizeof(float); ++i)
		vertexData.push_back(0);

	for (deUint32 i = 0; i < 4; ++i)
	{
		vertexData.push_back(vertices[i].x());
		vertexData.push_back(vertices[i].y());
		vertexData.push_back(vertices[i].z());
		vertexData.push_back(vertices[i].w());
		for (deUint32 j = 4; j < vertexStride / sizeof(float); ++j)
			vertexData.push_back(0.0f);
	}

	vk::VkClearValue								clearColorValue		= defaultClearValue(vk::VK_FORMAT_R32G32B32A32_SFLOAT);
	vk::VkDeviceSize								colorBufferSize		= colorData.size() * sizeof(float);
	vk::VkDeviceSize								vertexBufferSize	= vertexData.size() * sizeof(float);

	const auto										colorCreateInfo		= vk::makeBufferCreateInfo(colorBufferSize, vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	const auto										vertexCreateInfo	= vk::makeBufferCreateInfo(vertexBufferSize, vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	de::MovePtr<vk::BufferWithMemory>				colorBuffer			= de::MovePtr<vk::BufferWithMemory>(new vk::BufferWithMemory(vk, device, allocator, colorCreateInfo, vk::MemoryRequirement::HostVisible));
	de::MovePtr<vk::BufferWithMemory>				vertexBuffer		= de::MovePtr<vk::BufferWithMemory>(new vk::BufferWithMemory(vk, device, allocator, vertexCreateInfo, vk::MemoryRequirement::HostVisible));
	copyAndFlush(vk, device, *colorBuffer, 0, colorData.data(), colorData.size() * sizeof(float));
	copyAndFlush(vk, device, *vertexBuffer, 0, vertexData.data(), vertexData.size() * sizeof(float));

	beginCommandBuffer(vk, *cmdBuffer);
	renderPass.begin(vk, *cmdBuffer, vk::makeRect2D(0, 0, extent.width, extent.height), clearColorValue);
	graphicsPipelineWrapper.bind(*cmdBuffer);

	vk::VkBuffer		buffers[]	= { **colorBuffer, **vertexBuffer, **colorBuffer, **vertexBuffer, **colorBuffer, **vertexBuffer, **colorBuffer, **vertexBuffer };
	std::vector<vk::VkDeviceSize>	offsets	= { colorOffset, vertexOffset };
	if (m_count == 2)
	{
		offsets.push_back(colorOffset + sizeof(float) * 2);
		offsets.push_back(vertexOffset + sizeof(float) * 2);
	}
	else if (m_count == 3)
	{
		offsets.push_back(colorOffset + sizeof(float) * 2);
		offsets.push_back(vertexOffset + sizeof(float) * 2);
		offsets.push_back(colorOffset + sizeof(float) * 3);
		offsets.push_back(vertexOffset + sizeof(float) * 3);
	}
	else if (m_count == 4)
	{
		offsets.push_back(colorOffset + sizeof(float));
		offsets.push_back(vertexOffset + sizeof(float));
		offsets.push_back(colorOffset + sizeof(float) * 2);
		offsets.push_back(vertexOffset + sizeof(float) * 2);
		offsets.push_back(colorOffset + sizeof(float) * 3);
		offsets.push_back(vertexOffset + sizeof(float) * 3);
	}
	std::vector<vk::VkDeviceSize>	sizes;
	for (deUint32 i = 0; i < m_count; ++i)
	{
		sizes.push_back(colorBufferSize - offsets[i * 2]);
		sizes.push_back(vertexBufferSize - offsets[i * 2 + 1]);
	}
	vk::VkDeviceSize	strides[]	= { colorStride, vertexStride, colorStride, vertexStride, colorStride, vertexStride, colorStride, vertexStride };
	if (m_singleBind)
	{
#ifndef CTS_USES_VULKANSC
		vk.cmdBindVertexBuffers2(*cmdBuffer, 0, 2 * m_count, buffers, offsets.data(), sizes.data(), strides);
#else
		vk.cmdBindVertexBuffers2EXT(*cmdBuffer, 0, 2 * m_count, buffers, offsets.data(), sizes.data(), strides);
#endif
	}
	else
	{
		for (deUint32 i = 0; i < m_count * 2; ++i)
		{
#ifndef CTS_USES_VULKANSC
			vk.cmdBindVertexBuffers2(*cmdBuffer, i, 1, &buffers[i], &offsets[i], &sizes[i], &strides[i]);
#else
			vk.cmdBindVertexBuffers2EXT(*cmdBuffer, i, 1, &buffers[i], &offsets[i], &sizes[i], &strides[i]);
#endif
		}
	}

	vk.cmdDraw(*cmdBuffer, 4, instanceCount, 0, 0);
	renderPass.end(vk, *cmdBuffer);

	vk::copyImageToBuffer(vk, *cmdBuffer, *colorImage, (*outBuffer).get(), tcu::IVec2(extent.width, extent.height));
	endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	invalidateAlloc(vk, device, outBufferAlloc);
	const tcu::ConstPixelBufferAccess result(vk::mapVkFormat(vk::VK_FORMAT_R32G32B32A32_SFLOAT), tcu::IVec3(extent.width, extent.height, 1), (const char*)outBufferAlloc.getHostPtr());

	const deUint32 h = result.getHeight();
	const deUint32 w = result.getWidth();
	for (deUint32 y = 0; y < h; y++)
	{
		for (deUint32 x = 0; x < w; x++)
		{
			tcu::Vec4	pix = result.getPixel(x, y);

			if (x >= w / 2 && y >= h / 2 && pix != colors[0]) return tcu::TestStatus::fail("Fail");
			if (x < w / 2 && y >= h / 2 && pix != colors[colorStride == 0 ? 0 : 1]) return tcu::TestStatus::fail("Fail");
			if (x >= w / 2 && y < h / 2 && pix != colors[colorStride == 0 ? 0 : 2]) return tcu::TestStatus::fail("Fail");
			if (x < w / 2 && y < h / 2 && pix != colors[colorStride == 0 ? 0 : 3]) return tcu::TestStatus::fail("Fail");
		}
	}

	return tcu::TestStatus::pass("Pass");
}

class BindBuffers2Case : public vkt::TestCase
{
public:
					BindBuffers2Case	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const vk::PipelineConstructionType	pipelineConstructionType, const TestParams params, const bool singleBind, const deUint32 count)
						: vkt::TestCase					(testCtx, name, description)
						, m_pipelineConstructionType	(pipelineConstructionType)
						, m_params						(params)
						, m_singleBind					(singleBind)
						, m_count						(count)
						{}
	virtual			~BindBuffers2Case	(void) {}

	void			checkSupport			(vkt::Context& context) const override;
	virtual void	initPrograms			(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance			(Context& context) const override { return new BindBuffers2Instance(context, m_pipelineConstructionType, m_params, m_singleBind, m_count); }

private:
	const vk::PipelineConstructionType	m_pipelineConstructionType;
	const TestParams					m_params;
	const bool							m_singleBind;
	const deUint32						m_count;
};

void BindBuffers2Case::checkSupport(Context& context) const
{
	context.requireDeviceFunctionality("VK_EXT_extended_dynamic_state");

	vk::checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_pipelineConstructionType);
}

void BindBuffers2Case::initPrograms(vk::SourceCollections& programCollection) const
{
	std::stringstream vert;
	std::stringstream frag;

	std::string inputs;
	std::string combined;
	if (m_count == 2)
	{
		inputs =
			"layout (location=0) in vec2 rg;\n"
			"layout (location=1) in vec2 xy;\n"
			"layout (location=2) in vec2 ba;\n"
			"layout (location=3) in vec2 zw;\n";
		combined =
			"    vec4 vertex = vec4(xy, zw);\n"
			"    vec4 color = vec4(rg, ba);\n";
	}
	else if (m_count == 3)
	{
		inputs =
			"layout (location=0) in vec2 rg;\n"
			"layout (location=1) in vec2 xy;\n"
			"layout (location=2) in float b;\n"
			"layout (location=3) in float z;\n"
			"layout (location=4) in float a;\n"
			"layout (location=5) in float w;\n";
		combined =
			"    vec4 vertex = vec4(xy, z, w);\n"
			"    vec4 color = vec4(rg, b, a);\n";
	}
	else if (m_count == 4)
	{
		inputs =
			"layout (location=0) in float r;\n"
			"layout (location=1) in float x;\n"
			"layout (location=2) in float g;\n"
			"layout (location=3) in float y;\n"
			"layout (location=4) in float b;\n"
			"layout (location=5) in float z;\n"
			"layout (location=6) in float a;\n"
			"layout (location=7) in float w;\n";
		combined =
			"    vec4 vertex = vec4(x, y, z, w);\n"
			"    vec4 color = vec4(r, g, b, a);\n";
	}
	else
	{
		inputs =
			"layout (location=0) in vec4 rgba;\n"
			"layout (location=1) in vec4 xyzw;\n";
		combined =
			"    vec4 vertex = vec4(xyzw);\n"
			"    vec4 color = vec4(rgba);\n";
	}

	vert
		<< "#version 450\n"
		<< inputs
		<< "layout (location=0) out vec4 outColor;\n"
		<< "void main() {\n"
		<< "    vec2 pos = vec2(-float(gl_InstanceIndex & 1), -float((gl_InstanceIndex >> 1) & 1));\n"
		<< combined
		<< "    gl_Position = vertex + vec4(pos, 0.0f, 1.0f);\n"
		<< "    outColor = color;\n"
		<< "}\n";

	frag
		<< "#version 450\n"
		<< "layout (location=0) in vec4 inColor;\n"
		<< "layout (location=0) out vec4 outColor;\n"
		<< "void main() {\n"
		<< "    outColor = inColor;\n"
		<< "}\n";

	programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
	programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

tcu::TestCaseGroup* createCmdBindBuffers2Tests (tcu::TestContext& testCtx, vk::PipelineConstructionType pipelineConstructionType)
{
	de::MovePtr<tcu::TestCaseGroup> cmdBindBuffers2Group(new tcu::TestCaseGroup(testCtx, "bind_buffers_2", ""));

	const struct
	{
		TestParams	params;
		const char* name;
	} strideTests[] =
	{
		// Values are multiplied by sizeof(float) in the test
		{ {0u,		4u,		0u,		0u,		},	"stride_0_4_offset_0_0"		},
		{ {0u,		4u,		1u,		0u,		},	"stride_0_4_offset_1_0"		},
		{ {4u,		4u,		0u,		0u,		},	"stride_4_4_offset_0_0"		},
		{ {5u,		5u,		0u,		7u,		},	"stride_5_5_offset_0_7"		},
		{ {5u,		8u,		15u,	22u,	},	"stride_5_8_offset_15_22"	},
		{ {7u,		22u,	100u,	0u,		},	"stride_7_22_offset_100_0"	},
		{ {40u,		28u,	0u,		0u,		},	"stride_40_28_offset_0_0"	},
	};

	const struct
	{
		bool		singleBind;
		const char* name;
	} bindTests[] =
	{
		// Values are multiplied by sizeof(float) in the test
		{ true,		"single"		},
		{ false,	"separate"		},
	};

	const struct
	{
		deUint32	count;
		const char* name;
	} countTests[] =
	{
		{ 1,		"count_1"		},
		{ 2,		"count_2"		},
		{ 3,		"count_3"		},
		{ 4,		"count_4"		},
	};

	for (const auto bindTest : bindTests)
	{
		de::MovePtr<tcu::TestCaseGroup> bindGroup(new tcu::TestCaseGroup(testCtx, bindTest.name, ""));
		for (const auto& strideTest : strideTests)
		{
			de::MovePtr<tcu::TestCaseGroup> typeGroup(new tcu::TestCaseGroup(testCtx, strideTest.name, ""));
			for (const auto& countTest : countTests)
			{
				typeGroup->addChild(new BindBuffers2Case(testCtx, countTest.name, "", pipelineConstructionType, strideTest.params, bindTest.singleBind, countTest.count));
			}
			bindGroup->addChild(typeGroup.release());
		}
		cmdBindBuffers2Group->addChild(bindGroup.release());
	}

	return cmdBindBuffers2Group.release();
}

} // pipeline
} // vkt
