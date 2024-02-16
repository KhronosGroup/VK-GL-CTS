/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 The Khronos Group Inc.
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
 * \brief Additional tests for VK_KHR_shader_quad_control
 *//*--------------------------------------------------------------------*/

#include "vktSubgroupsQuadControlTests.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "tcuTestLog.hpp"
#include "deMath.h"
#include "tcuVectorUtil.hpp"
#include "deUniquePtr.hpp"
#include <vector>

namespace vkt
{
namespace subgroups
{

using namespace vk;

enum class TestMode
{
	QUAD_DERIVATIVES			= 0,
	REQUIRE_FULL_QUADS,
	DIVERGENT_CONDITION,
};

class DrawWithQuadControlInstanceBase : public vkt::TestInstance
{
public:
								DrawWithQuadControlInstanceBase	(Context&	context,
																 TestMode	mode);

	virtual						~DrawWithQuadControlInstanceBase(void) = default;

	virtual tcu::TestStatus		iterate							(void) override;

protected:

	virtual bool				isResultCorrect					(const tcu::ConstPixelBufferAccess& outputAccess) const = 0;

	VkImageCreateInfo			getImageCreateInfo				(VkExtent3D			extent,
																 deUint32			mipLevels,
																 VkImageUsageFlags	usage) const;

protected:

	const TestMode				m_mode;
	const VkClearColorValue		m_mipColors[5];
	tcu::UVec2					m_renderSize;
	VkPrimitiveTopology			m_topology;
	std::vector<float>			m_vertices;
};

DrawWithQuadControlInstanceBase::DrawWithQuadControlInstanceBase(Context&	context,
																 TestMode	mode)
	: vkt::TestInstance		(context)
	, m_mode				(mode)
	, m_mipColors
	{
		{ { 0.9f, 0.4f, 0.2f, 1.0f } },		// orange
		{ { 0.2f, 0.8f, 0.9f, 1.0f } },		// blue
		{ { 0.2f, 0.9f, 0.2f, 1.0f } },		// green
		{ { 0.9f, 0.9f, 0.2f, 1.0f } },		// yellow
		{ { 0.6f, 0.1f, 0.9f, 1.0f } },		// violet
	}
	, m_renderSize			(32)
	, m_topology			(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
{
}

VkImageCreateInfo DrawWithQuadControlInstanceBase::getImageCreateInfo(VkExtent3D extent, deUint32 mipLevels, VkImageUsageFlags usage) const
{
	return
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,			//	VkStructureType			sType;
		DE_NULL,										//	const void*				pNext;
		0u,												//	VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,								//	VkImageType				imageType;
		VK_FORMAT_R8G8B8A8_UNORM,						//	VkFormat				format;
		extent,											//	VkExtent3D				extent;
		mipLevels,										//	deUint32				mipLevels;
		1u,												//	deUint32				arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,							//	VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,						//	VkImageTiling			tiling;
		usage,											//	VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,						//	VkSharingMode			sharingMode;
		0u,												//	deUint32				queueFamilyIndexCount;
		DE_NULL,										//	const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,						//	VkImageLayout			initialLayout;
	};
}

tcu::TestStatus DrawWithQuadControlInstanceBase::iterate(void)
{
	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	const deUint32			queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	const VkDevice			device				= m_context.getDevice();
	Allocator&				alloc				= m_context.getDefaultAllocator();

	const VkFormat					colorFormat	{ VK_FORMAT_R8G8B8A8_UNORM };
	const std::vector<VkViewport>	viewports	{ makeViewport(m_renderSize) };
	const std::vector<VkRect2D>		scissors	{ makeRect2D(m_renderSize) };

	DE_ASSERT(!m_vertices.empty());		// derived class should specify vertex in costructor
	const VkBufferCreateInfo vertexBufferInfo = makeBufferCreateInfo(m_vertices.size() * sizeof(float), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	BufferWithMemory vertexBuffer(vk, device, alloc, vertexBufferInfo, MemoryRequirement::HostVisible);
	deMemcpy(vertexBuffer.getAllocation().getHostPtr(), m_vertices.data(), m_vertices.size() * sizeof(float));
	flushAlloc(vk, device, vertexBuffer.getAllocation());

	// create output buffer that will be used to read rendered image
	const VkDeviceSize outputBufferSize = (VkDeviceSize)m_renderSize.x() * m_renderSize.y() * tcu::getPixelSize(mapVkFormat(colorFormat));
	const VkBufferCreateInfo outputBufferInfo = makeBufferCreateInfo(outputBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	BufferWithMemory outputBuffer(vk, device, alloc, outputBufferInfo, MemoryRequirement::HostVisible);

	// create color buffer
	VkExtent3D colorImageExtent = makeExtent3D(m_renderSize.x(), m_renderSize.y(), 1u);
	const VkImageCreateInfo colorImageCreateInfo = getImageCreateInfo(colorImageExtent, 1u, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	const VkImageSubresourceRange colorSRR = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	ImageWithMemory colorImage(vk, device, alloc, colorImageCreateInfo, MemoryRequirement::Any);
	Move<VkImageView> colorImageView = makeImageView(vk, device, colorImage.get(), VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSRR);

	// create image that will be used as a texture
	deUint32 mipLevels = DE_LENGTH_OF_ARRAY(m_mipColors);
	VkExtent3D textureImageExtent = makeExtent3D(16u, 16u, 1u);
	const VkImageCreateInfo textureImageCreateInfo = getImageCreateInfo(textureImageExtent, mipLevels, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	const VkImageSubresourceRange textureSRR = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, mipLevels, 0u, 1u);
	ImageWithMemory textureImage(vk, device, alloc, textureImageCreateInfo, MemoryRequirement::Any);
	Move<VkImageView> textureImageView = makeImageView(vk, device, textureImage.get(), VK_IMAGE_VIEW_TYPE_2D, colorFormat, textureSRR);

	// create sampler
	const VkSamplerCreateInfo samplerCreateInfo
	{
		VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,								// VkStructureType			sType;
		DE_NULL,															// const void*				pNext;
		0u,																	// VkSamplerCreateFlags		flags;
		VK_FILTER_NEAREST,													// VkFilter					magFilter;
		VK_FILTER_NEAREST,													// VkFilter					minFilter;
		VK_SAMPLER_MIPMAP_MODE_NEAREST,										// VkSamplerMipmapMode		mipmapMode;
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,								// VkSamplerAddressMode		addressModeU;
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,								// VkSamplerAddressMode		addressModeV;
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,								// VkSamplerAddressMode		addressModeW;
		0.0f,																// float					mipLodBias;
		VK_FALSE,															// VkBool32					anisotropyEnable;
		1.0f,																// float					maxAnisotropy;
		DE_FALSE,															// VkBool32					compareEnable;
		VK_COMPARE_OP_ALWAYS,												// VkCompareOp				compareOp;
		0.0f,																// float					minLod;
		5.0f,																// float					maxLod;
		VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,							// VkBorderColor			borderColor;
		VK_FALSE,															// VkBool32					unnormalizedCoordinates;
	};
	Move<VkSampler> sampler = createSampler(vk, device, &samplerCreateInfo);

	const VkVertexInputBindingDescription vertexInputBindingDescription
	{
		0u,																	// deUint32				binding
		6u * sizeof(float),													// deUint32				stride
		VK_VERTEX_INPUT_RATE_VERTEX,										// VkVertexInputRate	inputRate
	};

	const VkVertexInputAttributeDescription vertexInputAttributeDescription[]
	{
		{	// position: 4 floats
			0u,																// deUint32				location
			0u,																// deUint32				binding
			VK_FORMAT_R32G32B32A32_SFLOAT,									// VkFormat				format
			0u																// deUint32				offset
		},
		{	// uv: 2 floats
			1u,																// deUint32				location
			0u,																// deUint32				binding
			VK_FORMAT_R32G32_SFLOAT,										// VkFormat				format
			4u * sizeof(float)												// deUint32				offset
		}
	};

	const VkPipelineVertexInputStateCreateInfo vertexInputState
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,			// VkStructureType								sType
		DE_NULL,															// const void*									pNext
		(VkPipelineVertexInputStateCreateFlags)0,							// VkPipelineVertexInputStateCreateFlags		flags
		1u,																	// deUint32										vertexBindingDescriptionCount
		&vertexInputBindingDescription,										// const VkVertexInputBindingDescription*		pVertexBindingDescriptions
		2u,																	// deUint32										vertexAttributeDescriptionCount
		vertexInputAttributeDescription										// const VkVertexInputAttributeDescription*		pVertexAttributeDescriptions
	};

	// create descriptor set
	DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	const Move<VkDescriptorPool> descriptorPool = poolBuilder.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1);

	DescriptorSetLayoutBuilder layoutBuilder;
	layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	const Move<VkDescriptorSetLayout> descriptorSetLayout = layoutBuilder.build(vk, device);

	const Move<VkDescriptorSet> descriptorSet = makeDescriptorSet(vk, device, descriptorPool.get(), descriptorSetLayout.get());

	// update descriptor set
	DescriptorSetUpdateBuilder updater;
	VkDescriptorImageInfo imageInfo = makeDescriptorImageInfo(*sampler, *textureImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	updater.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageInfo);
	updater.update(vk, device);

	// create shader modules, renderpass, framebuffer and pipeline
	Move<VkShaderModule>	vertShaderModule	= createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0);
	Move<VkShaderModule>	fragShaderModule	= createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0);
	Move<VkRenderPass>		renderPass			= makeRenderPass(vk, device, colorFormat);
	Move<VkPipelineLayout>	pipelineLayout		= makePipelineLayout(vk, device, *descriptorSetLayout);
	Move<VkFramebuffer>		framebuffer			= makeFramebuffer(vk, device, *renderPass, *colorImageView, m_renderSize.x(), m_renderSize.y());
	Move<VkPipeline>		graphicsPipeline	= makeGraphicsPipeline(vk, device, *pipelineLayout,
																	   *vertShaderModule, DE_NULL, DE_NULL, DE_NULL, *fragShaderModule,
																	   *renderPass, viewports, scissors, m_topology,
																	   0u, 0u, &vertexInputState);

	Move<VkCommandPool>				cmdPool		= createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
	vk::Move<vk::VkCommandBuffer>	cmdBuffer	= allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	beginCommandBuffer(vk, *cmdBuffer);

	// transition colorbuffer layout to attachment optimal
	VkImageMemoryBarrier imageBarrier = makeImageMemoryBarrier(0u, VK_ACCESS_SHADER_WRITE_BIT,
															   VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
															   colorImage.get(), colorSRR);
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u, 0u, 0u, 0u, 0u, 1u, &imageBarrier);

	// transition texture layout to transfer destination optimal
	imageBarrier = makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT,
										  VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
										  textureImage.get(), textureSRR);
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, 0u, 0u, 0u, 1u, &imageBarrier);

	// clear texture lod levels to diferent colors
	VkImageSubresourceRange textureMipSRR = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	for (deUint32 mipLevel = 0; mipLevel < mipLevels; ++mipLevel)
	{
		textureMipSRR.baseMipLevel = mipLevel;
		vk.cmdClearColorImage(*cmdBuffer, textureImage.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &m_mipColors[mipLevel], 1, &textureMipSRR);
	}

	// transition texture layout to shader read optimal
	imageBarrier = makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
										  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
										  textureImage.get(), textureSRR);
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u, 0u, 0u, 0u, 0u, 1u, &imageBarrier);

	const VkRect2D renderArea = makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y());
	beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, renderArea, tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

	const VkDeviceSize vBuffOffset = 0;
	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);
	vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vertexBuffer.get(), &vBuffOffset);
	vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u, &*descriptorSet, 0u, nullptr);

	vk.cmdDraw(*cmdBuffer, (deUint32)m_vertices.size() / 6u, 1, 0, 0);

	endRenderPass(vk, *cmdBuffer);

	// transition colorbuffer layout to transfer source optimal
	imageBarrier = makeImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
										  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
										  colorImage.get(), colorSRR);
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, 0u, 0u, 0u, 1u, &imageBarrier);

	// read back color image
	const VkImageSubresourceLayers colorSL = makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const VkBufferImageCopy copyRegion = makeBufferImageCopy(colorImageExtent, colorSL);
	vk.cmdCopyImageToBuffer(*cmdBuffer, colorImage.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, outputBuffer.get(), 1u, &copyRegion);

	endCommandBuffer(vk, *cmdBuffer);

	VkQueue queue;
	vk.getDeviceQueue(device, queueFamilyIndex, 0, &queue);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	// get output buffer
	invalidateAlloc(vk, device, outputBuffer.getAllocation());
	const tcu::TextureFormat resultFormat = mapVkFormat(colorFormat);
	tcu::ConstPixelBufferAccess outputAccess(resultFormat, m_renderSize.x(), m_renderSize.y(), 1u, outputBuffer.getAllocation().getHostPtr());

	// verify result
	if (isResultCorrect(outputAccess))
		return tcu::TestStatus::pass("Pass");

	m_context.getTestContext().getLog()
		<< tcu::TestLog::Image("Result", "Result", outputAccess);

	return tcu::TestStatus::fail("Fail");
}

class QuadDerivativesInstance : public DrawWithQuadControlInstanceBase
{
public:
					QuadDerivativesInstance				(Context&	context,
														 TestMode	mode);

	virtual			~QuadDerivativesInstance			(void) = default;

	virtual bool	isResultCorrect						(const tcu::ConstPixelBufferAccess& outputAccess) const override;
};

QuadDerivativesInstance::QuadDerivativesInstance(Context& context, TestMode mode)
	: DrawWithQuadControlInstanceBase(context, mode)
{
	// create vertex for 5 triangles - defined in order from displayed on the left to the right
	m_vertices =
	{	// position						uvCoords
		 0.0f,  1.2f, 0.0f, 1.0f,		 0.0f,  0.0f,		// uv adjusted to get lod 1
		-1.2f, -2.0f, 0.0f, 1.0f,		 1.0f,  1.0f,
		-1.2f,  1.2f, 0.0f, 1.0f,		 0.0f,  1.0f,

		-0.2f,  0.3f, 0.0f, 1.0f,		 1.0f,  1.0f,		// uv adjusted to get lod 2
		-0.7f, -0.9f, 0.0f, 1.0f,		 0.0f,  0.0f,
		-0.3f, -0.8f, 0.0f, 1.0f,		 0.0f,  1.0f,

		 0.0f,  0.2f, 0.0f, 1.0f,		10.0f, 10.0f,		// uv adjusted to get lod 5
		 0.1f, -1.0f, 0.0f, 1.0f,		 0.0f,  0.0f,
		-0.3f, -1.0f, 0.0f, 1.0f,		 0.0f, 10.0f,

		 0.2f, -0.1f, 0.0f, 1.0f,		 4.0f,  4.0f,		// uv adjusted to get lod 4
		 0.7f, -1.2f, 0.0f, 1.0f,		 0.0f,  0.0f,
		 0.2f, -1.8f, 0.0f, 1.0f,		 0.0f,  4.0f,

		-0.1f,  0.5f, 0.0f, 1.0f,		 0.0f,  0.0f,		// uv adjusted to get lod 3
		 0.8f, -0.8f, 0.0f, 1.0f,		 5.0f,  5.0f,
		 0.9f,  0.8f, 0.0f, 1.0f,		 0.0f,  5.0f,
	};
}

bool QuadDerivativesInstance::isResultCorrect(const tcu::ConstPixelBufferAccess& outputAccess) const
{
	const tcu::UVec2	fragmentOnFirstTraingle		(3, 8);
	const deUint32		expectedColorPerFragment[]	{ 0u, 1u, 4u, 3u, 2u };
	const tcu::Vec4		colorPrecision				(0.1f);

	for (deUint32 triangleIndex = 0u; triangleIndex < 5u; ++triangleIndex)
	{
		// on each triangle we are checking fragment that is 6 fragments away from fragment on previous triangle
		const tcu::UVec2	fragmentOnTraingle	(fragmentOnFirstTraingle.x() + 6u * triangleIndex, fragmentOnFirstTraingle.y());
		const deUint32		expectedMipmapIndex (expectedColorPerFragment[triangleIndex]);
		const tcu::Vec4		expectedColor		(m_mipColors[expectedMipmapIndex].float32);
		tcu::Vec4			fragmentColor		= outputAccess.getPixel(fragmentOnTraingle.x(), fragmentOnTraingle.y(), 0);

		// make sure that fragment has color from proper mipmap level
		if (tcu::boolAny(tcu::greaterThan(tcu::absDiff(fragmentColor, expectedColor), colorPrecision)))
			return false;
	}

	return true;
}

class RequireFullQuadsInstance : public DrawWithQuadControlInstanceBase
{
public:
					RequireFullQuadsInstance	(Context&	context,
												 TestMode	mode);

	virtual			~RequireFullQuadsInstance	(void) = default;

	virtual bool	isResultCorrect				(const tcu::ConstPixelBufferAccess& outputAccess) const override;
};

RequireFullQuadsInstance::RequireFullQuadsInstance(Context& context, TestMode mode)
	: DrawWithQuadControlInstanceBase(context, mode)
{
	// create vertex for 4 conected triangles with an odd angles
	m_vertices =
	{	// position						uvCoords
		-0.9f,  0.6f, 0.0f, 1.0f,		 0.0f,   1.0f,
		-0.7f, -0.8f, 0.0f, 1.0f,		 1.0f,   1.0f,
		-0.2f,  0.9f, 0.0f, 1.0f,		 0.0f,   0.0f,

		 0.0f,  0.2f, 0.0f, 1.0f,		20.0f,  20.0f,

		 0.6f,  0.5f, 0.0f, 1.0f,		21.0f,   0.0f,

		 1.2f, -0.9f, 0.0f, 1.0f,		 0.0f,  75.0f,
	};
	m_topology		= VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	m_renderSize	= tcu::UVec2(128);
}

bool RequireFullQuadsInstance::isResultCorrect(const tcu::ConstPixelBufferAccess& outputAccess) const
{
	const float		reference				(0.9f);
	deUint32		renderedCount			(0);
	deUint32		properIDsCount			(0);
	deUint32		withHelpersCount		(0);
	deUint32		withoutHelpersCount		(0);

	// ensure at least some shaders have the vote return True and are filled with read color
	for (deUint32 x = 0u; x < m_renderSize.x(); ++x)
	for (deUint32 y = 0u; y < m_renderSize.y(); ++y)
	{
		tcu::Vec4 pixel = outputAccess.getPixel(x, y, 0);
		if (pixel.x() < reference)
			continue;

		++renderedCount;

		// if blue channel is 1 then quads had proper IDs
		properIDsCount += deUint32(pixel.y() > reference);

		// at least some shaders should have voted True if any helper invocations existed
		withHelpersCount += deUint32(pixel.z() > reference);

		// at least some shaders should have voted True if there were quads without helper invocations
		withoutHelpersCount += deUint32(pixel.w() > reference);
	}

	return (renderedCount == properIDsCount) &&
		   (renderedCount == (withHelpersCount + withoutHelpersCount)) &&
		   (withoutHelpersCount > 50) &&
		   (withHelpersCount > 50);
}

class DivergentConditionInstance : public DrawWithQuadControlInstanceBase
{
public:
					DivergentConditionInstance	(Context&	context,
												 TestMode	mode);

	virtual			~DivergentConditionInstance	(void) = default;

	virtual bool	isResultCorrect				(const tcu::ConstPixelBufferAccess& outputAccess) const override;
};

DivergentConditionInstance::DivergentConditionInstance(Context& context, TestMode mode)
	: DrawWithQuadControlInstanceBase(context, mode)
{
	// create vertex for 2 triangles forming full screen quad
	m_vertices =
	{	// position						uvCoords
		-1.0f,  1.0f, 0.0f, 1.0f,		0.0f, 1.0f,
		 1.0f,  1.0f, 0.0f, 1.0f,		1.0f, 1.0f,
		-1.0f, -1.0f, 0.0f, 1.0f,		0.0f, 0.0f,
		 1.0f, -1.0f, 0.0f, 1.0f,		1.0f, 0.0f,
	};
	m_topology		= VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	m_renderSize	= tcu::UVec2(16);
}

bool DivergentConditionInstance::isResultCorrect(const tcu::ConstPixelBufferAccess& outputAccess) const
{
	bool		testPassed	(true);
	const float	reference	(0.99f);
	const float	lambda		(0.01f);

	auto checkCondition = [](int x, int y)
	{
		return (((x % 8) > 4) || (((x % 8) == 2) && bool(y % 2)));
	};

	for (deUint32 x = 0u; x < m_renderSize.x(); x += 1u)
	for (deUint32 y = 0u; y < m_renderSize.y(); y += 1u)
	{
		tcu::Vec4 pixel = outputAccess.getPixel(x, y, 0);

		// if the fragment coord does not meet the first condition then this fragment should have r and g component set to 0
		if (!checkCondition(x, y))
		{
			if ((pixel.x() > lambda) || (pixel.y() > lambda))
				testPassed = false;

			// go to next pixel
			continue;
		}

		deUint32 qx = x - (x % 2);
		deUint32 qy = y - (y % 2);
		int sum =
			(checkCondition(qx + 0, qy + 0) && checkCondition(qy + 0, qx + 0)) +
			(checkCondition(qx + 0, qy + 1) && checkCondition(qy + 1, qx + 0)) +
			(checkCondition(qx + 1, qy + 0) && checkCondition(qy + 0, qx + 1)) +
			(checkCondition(qx + 1, qy + 1) && checkCondition(qy + 1, qx + 1));
		int activeCount =
			checkCondition(qx + 0, qy + 0) +
			checkCondition(qx + 0, qy + 1) +
			checkCondition(qx + 1, qy + 0) +
			checkCondition(qx + 1, qy + 1);

		// if none of fragments in the quad meets second condition then this fragment should have r and g components set to 0
		if (sum == 0)
		{
			if ((pixel.x() > lambda) || (pixel.y() > lambda))
				testPassed = false;
			continue;
		}

		// if all active quad fragments meets second condition then this fragment should have r and g components set to 1
		if (sum == activeCount)
		{
			if ((pixel.x() < reference) || (pixel.y() < reference))
				testPassed = false;
			continue;
		}

		// if at least one active quad fragment meets second condition then this fragment should have r component set to 1 and g component to 0
		if ((pixel.x() < reference) || (pixel.y() > lambda))
			testPassed = false;
	}

	return testPassed;
}

class DrawWithQuadControlTestCase : public vkt::TestCase
{
public:
					DrawWithQuadControlTestCase	(tcu::TestContext&		testContext,
												 const std::string&		name,
												 TestMode				mode);

	virtual			~DrawWithQuadControlTestCase(void) = default;

	void			checkSupport				(Context& context) const override;
	TestInstance*	createInstance				(Context& context) const override;
	void			initPrograms				(SourceCollections& programCollection) const override;

protected:
	const TestMode		m_testMode;
};

DrawWithQuadControlTestCase::DrawWithQuadControlTestCase(tcu::TestContext&		testContext,
														 const std::string&		name,
														 TestMode				mode)
	: vkt::TestCase		(testContext, name)
	, m_testMode		(mode)
{}

void DrawWithQuadControlTestCase::checkSupport(Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_shader_quad_control");
}

TestInstance* DrawWithQuadControlTestCase::createInstance(Context& context) const
{
	if (m_testMode == TestMode::QUAD_DERIVATIVES)
		return new QuadDerivativesInstance(context, m_testMode);
	if (m_testMode == TestMode::REQUIRE_FULL_QUADS)
		return new RequireFullQuadsInstance(context, m_testMode);

	return new DivergentConditionInstance(context, m_testMode);
}

void DrawWithQuadControlTestCase::initPrograms(SourceCollections& sourceCollections) const
{
	std::string vertexSource(
		"#version 450\n"
		"layout(location = 0) in vec4 inPosition;\n"
		"layout(location = 1) in vec2 inTexCoords;\n"
		"layout(location = 0) out highp vec2 outTexCoords;\n"
		"void main(void)\n"
		"{\n"
		"\tgl_Position = inPosition;\n"
		"\toutTexCoords = inTexCoords;\n"
		"}\n");
	sourceCollections.glslSources.add("vert") << glu::VertexSource(vertexSource);

	std::string fragmentSource;
	if (m_testMode == TestMode::QUAD_DERIVATIVES)
	{
		// we are drawing few triangles and in shader we have a condition
		// that will be true for exactly one fragment in each triangle

		fragmentSource =
			"#version 450\n"
			"precision highp float;\n"
			"precision highp int;\n"
			"#extension GL_EXT_shader_quad_control: enable\n"
			"#extension GL_KHR_shader_subgroup_vote: enable\n"
			"layout(quad_derivatives) in;\n"
			"layout(location = 0) in highp vec2 inTexCoords;\n"
			"layout(location = 0) out vec4 outFragColor;\n"
			"layout(binding = 0) uniform sampler2D texSampler;\n"
			"void main (void)\n"
			"{\n"
			"\tbool conditionTrueForOneFrag = (abs(gl_FragCoord.y - 8.5) < 0.1) && (mod(gl_FragCoord.x-3.5, 6.0) < 0.1);\n"
			"\tif (subgroupQuadAny(conditionTrueForOneFrag))\n"
			"\t\toutFragColor = texture(texSampler, inTexCoords);\n"
			"\telse\n"
			"\t\toutFragColor = vec4(0.9, 0.2, 0.2, 1.0);\n"
			"}\n";
	}
	else if(m_testMode == TestMode::REQUIRE_FULL_QUADS)
	{
		// we are drawing few connected triangles at odd angles
		// RequireFullQuadsKHR ensures lots of helper lanes

		fragmentSource =
			"#version 450\n"
			"#extension GL_KHR_shader_subgroup_quad: enable\n"
			"#extension GL_KHR_shader_subgroup_vote: enable\n"
			"#extension GL_EXT_shader_quad_control: enable\n"
			"precision highp float;\n"
			"precision highp int;\n"
			"layout(full_quads) in;\n"
			"layout(location = 0) in highp vec2 inTexCoords;\n"
			"layout(location = 0) out vec4 outFragColor;\n"
			"layout(binding = 0) uniform sampler2D texSampler;\n"
			"void main (void)\n"
			"{\n"
			"\tuint quadID = gl_SubgroupInvocationID % 4;\n"
			"\tuint idSum = quadID;\n"
			"\tidSum += subgroupQuadSwapHorizontal(quadID);\n"
			"\tidSum += subgroupQuadSwapVertical(quadID);\n"
			"\tidSum += subgroupQuadSwapDiagonal(quadID);\n"
			"\toutFragColor = vec4(1.0, 0.0, 0.0, 0.0);\n"
			"\tif (idSum == 6)\n"
			"\t\toutFragColor.g = 1.0;\n"
			"\tif (subgroupQuadAny(gl_HelperInvocation))\n"
			"\t\toutFragColor.b = 1.0;\n"
			"\tif (subgroupQuadAll(!gl_HelperInvocation))\n"
			"\t\toutFragColor.a = 1.0;\n"
			"}\n";
	}
	else // TestMode::DIVERGENT_CONDITION
	{
		// draw fullscreen quad and use quadAny/quadAll
		// inside divergent control flow

		fragmentSource =
			"#version 450\n"
			"#extension GL_KHR_shader_subgroup_vote: enable\n"
			"#extension GL_EXT_shader_quad_control: enable\n"
			"precision highp float;\n"
			"precision highp int;\n"
			"layout(location = 0) out vec4 outFragColor;\n"
			"bool checkCondition(int x, int y) {\n"
			"\treturn (((x % 8) > 4) || (((x % 8) == 2) && bool(y % 2)));\n"
			"}\n"
			"void main (void)\n"
			"{\n"
			"\toutFragColor = vec4(0.0, 0.0, 0.0, 1.0);\n"
			"\tint x = int(gl_FragCoord.x);\n"
			"\tint y = int(gl_FragCoord.y);\n"
			"\tif (checkCondition(x, y))\n"
			"\t{\n"
			"\t\tbool v = checkCondition(y, x);\n"
			"\t\tif (subgroupQuadAny(v))\n"
			"\t\t\toutFragColor.r = 1.0;\n"
			"\t\tif (subgroupQuadAll(v))\n"
			"\t\t\toutFragColor.g = 1.0;\n"
			"\t}\n"
			"}\n";
	}

	const ShaderBuildOptions buildOptions(sourceCollections.usedVulkanVersion, SPIRV_VERSION_1_3, 0u);
	sourceCollections.glslSources.add("frag") << glu::FragmentSource(fragmentSource) << buildOptions;
}

tcu::TestCaseGroup* createSubgroupsQuadControlTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> quadScopeTests(new tcu::TestCaseGroup(testCtx, "shader_quad_control", "Test for VK_KHR_shader_quad_control"));

	quadScopeTests->addChild(new DrawWithQuadControlTestCase(testCtx, "quad_derivatives",	TestMode::QUAD_DERIVATIVES));
	quadScopeTests->addChild(new DrawWithQuadControlTestCase(testCtx, "require_full_quads",	TestMode::REQUIRE_FULL_QUADS));
	quadScopeTests->addChild(new DrawWithQuadControlTestCase(testCtx, "divergent_condition", TestMode::DIVERGENT_CONDITION));

	return quadScopeTests.release();
}

} // subgroups
} // vkt
