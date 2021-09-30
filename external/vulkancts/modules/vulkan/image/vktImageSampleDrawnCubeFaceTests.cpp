/*------------------------------------------------------------------------
 * ------------------------
 *
 * Copyright (c) 2021 Google LLC.
 *
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
 * \brief Sample cube faces that has been rendered to tests
 *//*--------------------------------------------------------------------*/

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"

#include "tcuVectorType.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTexture.hpp"

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkPrograms.hpp"
#include "vkMemUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBarrierUtil.hpp"

#include "vktTestCaseUtil.hpp"
#include "tcuTestLog.hpp"

#include <string>

using namespace vk;

namespace vkt
{
namespace image
{
namespace
{

using tcu::TestLog;
using tcu::IVec2;
using tcu::IVec3;
using tcu::IVec4;
using tcu::Vec2;
using tcu::Vec4;
using std::vector;
using de::MovePtr;
using tcu::TextureLevel;
using tcu::PixelBufferAccess;
using tcu::ConstPixelBufferAccess;

inline VkImageCreateInfo makeImageCreateInfo (const IVec3& size, const VkFormat& format, bool cubemap)
{
	const VkImageUsageFlags		usage		= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
											  | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
											  | VK_IMAGE_USAGE_SAMPLED_BIT;
	const VkImageCreateFlags flags = cubemap ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
	const VkImageCreateInfo	imageParams	=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,					//  VkStructureType         sType;
		DE_NULL,												//  const void*             pNext;
		flags,													//  VkImageCreateFlags      flags;
		VK_IMAGE_TYPE_2D,										//  VkImageType             imageType;
		format,													//  VkFormat                format;
		makeExtent3D(size.x(), size.y(), 1u),					//  VkExtent3D              extent;
		1u,														//  deUint32                mipLevels;
		(cubemap ? 6u : 1u),									//  deUint32                arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,									//  VkSampleCountFlagBits   samples;
		VK_IMAGE_TILING_OPTIMAL,								//  VkImageTiling           tiling;
		usage,													//  VkImageUsageFlags       usage;
		VK_SHARING_MODE_EXCLUSIVE,								//  VkSharingMode           sharingMode;
		0u,														//  deUint32                queueFamilyIndexCount;
		DE_NULL,												//  const deUint32*         pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,								//  VkImageLayout           initialLayout;
	};

	return imageParams;
}

Move<VkBuffer> makeVertexBuffer (const DeviceInterface& vk, const VkDevice device, const deUint32 queueFamilyIndex)
{
	const VkBufferCreateInfo vertexBufferParams =
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType      sType;
		DE_NULL,									// const void*          pNext;
		0u,											// VkBufferCreateFlags  flags;
		1024u,										// VkDeviceSize     size;
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,			// VkBufferUsageFlags   usage;
		VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode        sharingMode;
		1u,											// deUint32             queueFamilyIndexCount;
		&queueFamilyIndex							// const deUint32*      pQueueFamilyIndices;
	};

	Move<VkBuffer>			vertexBuffer		= createBuffer(vk, device, &vertexBufferParams);;
	return vertexBuffer;
}

class SampleDrawnCubeFaceTestInstance : public TestInstance
{
public:
					SampleDrawnCubeFaceTestInstance	(Context&			context,
													 const IVec2&		size,
													 const VkFormat		format);
	tcu::TestStatus	iterate							(void);

private:
	const tcu::IVec2&	m_size;
	const VkFormat		m_format;
};

SampleDrawnCubeFaceTestInstance::SampleDrawnCubeFaceTestInstance (Context& context, const IVec2& size, const VkFormat format)
	: TestInstance	(context)
	, m_size		(size)
	, m_format		(format)
{
}

template<typename T>
inline size_t sizeInBytes (const vector<T>& vec)
{
	return vec.size() * sizeof(vec[0]);
}

Move<VkSampler> makeSampler (const DeviceInterface& vk, const VkDevice& device)
{
	const VkSamplerCreateInfo samplerParams =
	{
		VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,		// VkStructureType          sType;
		DE_NULL,									// const void*              pNext;
		(VkSamplerCreateFlags)0,					// VkSamplerCreateFlags     flags;
		VK_FILTER_LINEAR,							// VkFilter                 magFilter;
		VK_FILTER_LINEAR,							// VkFilter                 minFilter;
		VK_SAMPLER_MIPMAP_MODE_NEAREST,				// VkSamplerMipmapMode      mipmapMode;
		VK_SAMPLER_ADDRESS_MODE_REPEAT,				// VkSamplerAddressMode     addressModeU;
		VK_SAMPLER_ADDRESS_MODE_REPEAT,				// VkSamplerAddressMode     addressModeV;
		VK_SAMPLER_ADDRESS_MODE_REPEAT,				// VkSamplerAddressMode     addressModeW;
		0.0f,										// float                    mipLodBias;
		VK_FALSE,									// VkBool32                 anisotropyEnable;
		1.0f,										// float                    maxAnisotropy;
		VK_FALSE,									// VkBool32                 compareEnable;
		VK_COMPARE_OP_ALWAYS,						// VkCompareOp              compareOp;
		0.0f,										// float                    minLod;
		0.0f,										// float                    maxLod;
		VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,	// VkBorderColor            borderColor;
		VK_FALSE,									// VkBool32                 unnormalizedCoordinates;
	};

	return createSampler(vk, device, &samplerParams);
}

// Draw a quad covering the whole framebuffer
vector<Vec4> genFullQuadVertices (void)
{
	vector<Vec4> vertices;
	vertices.push_back(Vec4(-1.0f, -1.0f, 0.0f, 1.0f));
	vertices.push_back(Vec4( 1.0f, -1.0f, 0.0f, 1.0f));
	vertices.push_back(Vec4(-1.0f,  1.0f, 0.0f, 1.0f));
	vertices.push_back(Vec4(1.0f,  -1.0f, 0.0f, 1.0f));
	vertices.push_back(Vec4(1.0f, 1.0f, 0.0f, 1.0f));
	vertices.push_back(Vec4(-1.0f,  1.0f, 0.0f, 1.0f));

	return vertices;
}

struct Vertex
{
	Vertex(Vec4 vertices_, Vec2 uv_) : vertices(vertices_), uv(uv_) {}
	Vec4 vertices;
	Vec2 uv;

	static VkVertexInputBindingDescription					getBindingDescription		(void);
	static vector<VkVertexInputAttributeDescription>		getAttributeDescriptions	(void);
};

VkVertexInputBindingDescription Vertex::getBindingDescription (void)
{
	static const VkVertexInputBindingDescription desc =
	{
		0u,										// deUint32             binding;
		static_cast<deUint32>(sizeof(Vertex)),	// deUint32             stride;
		VK_VERTEX_INPUT_RATE_VERTEX,			// VkVertexInputRate    inputRate;
	};

	return desc;
}

vector<VkVertexInputAttributeDescription> Vertex::getAttributeDescriptions (void)
{
	static const vector<VkVertexInputAttributeDescription> desc =
	{
		{
			0u,													// deUint32    location;
			0u,													// deUint32    binding;
			vk::VK_FORMAT_R32G32B32A32_SFLOAT,					// VkFormat    format;
			static_cast<deUint32>(offsetof(Vertex, vertices)),	// deUint32    offset;
		},
		{
			1u,													// deUint32    location;
			0u,													// deUint32    binding;
			vk::VK_FORMAT_R32G32_SFLOAT,						// VkFormat    format;
			static_cast<deUint32>(offsetof(Vertex, uv)),		// deUint32    offset;
		},
	};

	return desc;
}

vector<Vertex> genTextureCoordinates (void)
{
	vector<Vertex> vertices;
	vertices.push_back(Vertex(Vec4(-1.0f, -1.0f, 0.0f, 1.0f), Vec2(0.0f, 0.0f)));
	vertices.push_back(Vertex(Vec4( 1.0f, -1.0f, 0.0f, 1.0f), Vec2(1.0f, 0.0f)));
	vertices.push_back(Vertex(Vec4(-1.0f,  1.0f, 0.0f, 1.0f), Vec2(0.0f, 1.0f)));
	vertices.push_back(Vertex(Vec4(1.0f,  -1.0f, 0.0f, 1.0f), Vec2(1.0f, 0.0f)));
	vertices.push_back(Vertex(Vec4(1.0f, 1.0f, 0.0f, 1.0f), Vec2(1.0f, 1.0f)));
	vertices.push_back(Vertex(Vec4(-1.0f,  1.0f, 0.0f, 1.0f), Vec2(0.0f, 1.0f)));

	return vertices;
}

tcu::TestStatus SampleDrawnCubeFaceTestInstance::iterate (void)
{
	DE_ASSERT(m_format == VK_FORMAT_R8G8B8A8_UNORM);

	const DeviceInterface&			vk						= m_context.getDeviceInterface();
	const VkDevice					device					= m_context.getDevice();
	Allocator&						allocator				= m_context.getDefaultAllocator();
	const VkQueue					queue					= m_context.getUniversalQueue();
	const deUint32					queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
	const VkDeviceSize				bufferSize				= 1024;

	const deUint32					layerStart				= 0;
	const deUint32					layerCount				= 6;
	const deUint32					levelCount				= 1;

	const IVec3						imageSize				= {m_size.x(), m_size.y(), (deInt32)layerCount};
	const VkExtent2D				renderSize				= {deUint32(m_size.x()), deUint32(m_size.y())};
	const VkRect2D					renderArea				= makeRect2D(makeExtent3D(m_size.x(), m_size.y(), 1u));
	const vector<VkRect2D>			scissors				(1u, renderArea);
	const vector<VkViewport>		viewports				(1u, makeViewport(makeExtent3D(m_size.x(), m_size.y(), 1u)));

	const vector<Vec4>				vertices				= genFullQuadVertices();
	Move<VkBuffer>					vertexBuffer			= makeVertexBuffer(vk, device, queueFamilyIndex);
	MovePtr<Allocation>				vertexBufferAlloc		= bindBuffer(vk, device, allocator, *vertexBuffer, MemoryRequirement::HostVisible);
	const VkDeviceSize				vertexBufferOffset		= 0ull;

	deMemcpy(vertexBufferAlloc->getHostPtr(), &vertices[0], sizeInBytes(vertices));
	flushAlloc(vk, device, *vertexBufferAlloc);

	// Create a cubemap image.
	const VkImageCreateInfo			cubemapCreateInfo		= makeImageCreateInfo(imageSize, m_format, true);
	const VkImageSubresourceRange	cubemapSubresourceRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, levelCount, layerStart, layerCount);
	const ImageWithMemory			cubemapImage			(vk, device, m_context.getDefaultAllocator(), cubemapCreateInfo, MemoryRequirement::Any);
	Move<VkImageView>				cubemapImageView		= makeImageView(vk, device, *cubemapImage, VK_IMAGE_VIEW_TYPE_CUBE, m_format, cubemapSubresourceRange);

	// Create a sampler for the cubemap and bind it.
	Move<VkImageView>				sampledImageView		= makeImageView(vk, device, *cubemapImage, VK_IMAGE_VIEW_TYPE_CUBE, m_format, cubemapSubresourceRange);
	const Unique<VkSampler>			cubemapSampler			(makeSampler(vk, device));
	const VkDescriptorImageInfo		descriptorImageInfo		= makeDescriptorImageInfo(cubemapSampler.get(), *sampledImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	const auto						descriptorSetLayout	(DescriptorSetLayoutBuilder()
										.addSingleSamplerBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &cubemapSampler.get())
										.build(vk, device));

	const Unique<VkDescriptorPool>	descriptorPool			(DescriptorPoolBuilder()
										.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1u)
										.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const Unique<VkDescriptorSet>	descriptorSet			(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

	DescriptorSetUpdateBuilder()
			.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
						 VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &descriptorImageInfo)
			.update(vk, device);

	// Generate texture coordinates for the sampler.
	vector<Vertex>					uvCoordinates			= genTextureCoordinates();
	Move<VkBuffer>					uvBuffer				= makeVertexBuffer(vk, device, queueFamilyIndex);
	de::MovePtr<Allocation>			uvBufferAlloc			= bindBuffer(vk, device, allocator, *uvBuffer, MemoryRequirement::HostVisible);
	const VkDeviceSize				uvBufferOffset			= 0ull;

	deMemcpy(uvBufferAlloc->getHostPtr(), &uvCoordinates[0], static_cast<size_t>(bufferSize));
	flushAlloc(vk, device, *uvBufferAlloc);

	// Sampled values will be written to this image.
	const VkImageSubresourceRange	targetSubresourceRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, levelCount, layerStart, 1);
	const VkImageCreateInfo			targetImageCreateInfo	= makeImageCreateInfo(imageSize, m_format, false);
	const ImageWithMemory			targetImage				(vk, device, m_context.getDefaultAllocator(), targetImageCreateInfo, MemoryRequirement::Any);
	Move<VkImageView>				targetImageView			= makeImageView(vk, device, *targetImage, VK_IMAGE_VIEW_TYPE_2D, m_format, targetSubresourceRange);

	// We use a push constant to hold count for how many times the shader has written to the cubemap.
	const VkPushConstantRange		pushConstantRange		= {
			VK_SHADER_STAGE_FRAGMENT_BIT,		// VkShaderStageFlags    stageFlags;
			0u,									// uint32_t              offset;
			(deUint32)sizeof(deUint32),			// uint32_t              size;
	};

	const Move<VkCommandPool>		cmdPool					= createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
	const Move<VkCommandBuffer>		cmdBuffer				= allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	// Create two graphic pipelines. One for writing to the cubemap and the other for sampling it.
	Move<VkRenderPass>				renderPass1				= makeRenderPass (vk, device, m_format, VK_FORMAT_UNDEFINED, VK_ATTACHMENT_LOAD_OP_LOAD,
																			  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
																			  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, DE_NULL);

	Move<VkFramebuffer>				framebuffer1			= makeFramebuffer(vk, device, *renderPass1, cubemapImageView.get(), renderSize.width, renderSize.height);

	const Move<VkShaderModule>		vertexModule1			= createShaderModule (vk, device, m_context.getBinaryCollection().get("vert1"), 0u);
	const Move<VkShaderModule>		fragmentModule1			= createShaderModule (vk, device, m_context.getBinaryCollection().get("frag1"), 0u);

	const Move<VkPipelineLayout>	pipelineLayout1			= makePipelineLayout (vk, device, 0, DE_NULL, 1, &pushConstantRange);
	const Move<VkPipeline>			graphicsPipeline1		= makeGraphicsPipeline(vk, device, pipelineLayout1.get(), vertexModule1.get(),
																				   DE_NULL, DE_NULL, DE_NULL, fragmentModule1.get(), renderPass1.get(),
																				   viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0u, 0u,
																				   DE_NULL);

	Move<VkRenderPass>				renderPass2				= makeRenderPass(vk, device, m_format, VK_FORMAT_UNDEFINED, VK_ATTACHMENT_LOAD_OP_LOAD, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

	Move<VkFramebuffer>				framebuffer2			= makeFramebuffer(vk, device, *renderPass2, targetImageView.get(), renderSize.width, renderSize.height);

	Move<VkShaderModule>			vertexModule2			= createShaderModule(vk, device, m_context.getBinaryCollection().get("vert2"), 0u);
	Move<VkShaderModule>			fragmentModule2			= createShaderModule(vk, device, m_context.getBinaryCollection().get("frag2"), 0u);

	const Move<VkPipelineLayout>	pipelineLayout2			= makePipelineLayout(vk, device, *descriptorSetLayout);

	const auto						vtxBindingDescription	= Vertex::getBindingDescription();
	const auto						vtxAttrDescriptions		= Vertex::getAttributeDescriptions();

	const VkPipelineVertexInputStateCreateInfo	vertexInputInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType                             sType
		nullptr,													// const void*                                 pNext
		0u,															// VkPipelineVertexInputStateCreateFlags       flags
		1u,															// deUint32                                    vertexBindingDescriptionCount
		&vtxBindingDescription,										// const VkVertexInputBindingDescription*      pVertexBindingDescriptions
		static_cast<deUint32>(vtxAttrDescriptions.size()),			// deUint32                                    vertexAttributeDescriptionCount
		vtxAttrDescriptions.data(),									// const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions
	};

	const Move<VkPipeline>			graphicsPipeline2		= makeGraphicsPipeline(vk, device, pipelineLayout2.get(), vertexModule2.get(),
																				   DE_NULL, DE_NULL, DE_NULL, fragmentModule2.get(),
																				   renderPass2.get(), viewports, scissors,
																				   VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0u, 0u, &vertexInputInfo);

	// The values sampled in the second pipeline will be copied to this buffer.
	const VkBufferCreateInfo		resultBufferCreateInfo	= makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	Move<VkBuffer>					resultBuffer			= createBuffer(vk, device, &resultBufferCreateInfo);
	MovePtr<Allocation>				resultBufferMemory		= allocator.allocate(getBufferMemoryRequirements(vk, device, *resultBuffer), MemoryRequirement::HostVisible);
	MovePtr<TextureLevel>			resultImage				(new TextureLevel(mapVkFormat(m_format), renderSize.width, renderSize.height, 1));

	VK_CHECK(vk.bindBufferMemory(device, *resultBuffer, resultBufferMemory->getMemory(), resultBufferMemory->getOffset()));

	// Clear the cubemap faces and the target image as black.
	const Vec4						clearColor				(0.0f, 0.0f, 0.0f, 1.0f);

	clearColorImage(vk, device, m_context.getUniversalQueue(), m_context.getUniversalQueueFamilyIndex(),
					cubemapImage.get(), clearColor, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u, layerCount);

	clearColorImage(vk, device, m_context.getUniversalQueue(), m_context.getUniversalQueueFamilyIndex(),
					targetImage.get(), clearColor, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u, 1u);

	// Run the shaders twice.
	beginCommandBuffer(vk, *cmdBuffer);

	vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout2, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);

	for (int pass = 0; pass < 2; pass++)
	{
		//  Draw on the first cube map face.
		vk.cmdPushConstants(*cmdBuffer, *pipelineLayout1, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(deInt32), &pass);
		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline1);
		vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);

		beginRenderPass(vk, *cmdBuffer, *renderPass1, *framebuffer1, makeRect2D(0, 0, imageSize.x(), imageSize.y()), 0, DE_NULL);
		vk.cmdDraw(*cmdBuffer, static_cast<deUint32>(vertices.size()), 1u, 0u, 0u);
		endRenderPass(vk, *cmdBuffer);

		{
			const auto barrier = makeImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
														VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
														cubemapImage.get(), cubemapSubresourceRange);
			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, DE_NULL, 0, DE_NULL, 1u, &barrier);
		}

		// Sample the four faces around the first face.
		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline2);

		vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &uvBuffer.get(), &uvBufferOffset);

		beginRenderPass(vk, *cmdBuffer, *renderPass2, *framebuffer2, makeRect2D(0, 0, imageSize.x(), imageSize.y()), 0u, DE_NULL);
		vk.cmdDraw(*cmdBuffer, 6u, 1u, 0u, 0u);
		endRenderPass(vk, *cmdBuffer);

		if (pass == 0)
		{
			const auto	barrier		= makeImageMemoryBarrier(0u, VK_ACCESS_INPUT_ATTACHMENT_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
															 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, cubemapImage.get(), cubemapSubresourceRange);
			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, DE_NULL, 0, DE_NULL, 1u, &barrier);

			const auto	barrier2	= makeImageMemoryBarrier(0u, VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
															 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
															 targetImage.get(), targetSubresourceRange);
			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, DE_NULL, 0, DE_NULL, 1u, &barrier2);
		}
	}

	// Read the result buffer data
	copyImageToBuffer(vk, *cmdBuffer, *targetImage, *resultBuffer, IVec2(m_size.x(), m_size.y()), VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	invalidateAlloc(vk, device, *resultBufferMemory);

	tcu::clear(resultImage->getAccess(), IVec4(0));
	tcu::copy(resultImage->getAccess(), ConstPixelBufferAccess(resultImage.get()->getFormat(),
			  resultImage.get()->getSize(), resultBufferMemory->getHostPtr()));

	bool result = true;

	// The first run writes pure red and the second pure blue hence the value of the red component
	// should be 0.0 and the value in the blue channel > 0.0.
	for (deUint32 y = 0; y < renderSize.height; y++)
	{
		const deUint8* ptr = static_cast<const deUint8 *>(resultImage->getAccess().getPixelPtr(renderSize.width-1, y, 0));
		const IVec4 val = IVec4(ptr[0], ptr[1], ptr[2], ptr[3]);
		if (!(val[0] == 0 && val[1] > 0))
			result = false;
	}

	// Log attachment contents
	m_context.getTestContext().getLog() << tcu::TestLog::ImageSet("Attachment ", "")
	<< tcu::TestLog::Image("Rendered image", "Rendered image", resultImage->getAccess())
	<< tcu::TestLog::EndImageSet;

	if (result)
		return tcu::TestStatus::pass("pass");
	else
		return tcu::TestStatus::fail("fail");
}

class SampleDrawnCubeFaceTest : public TestCase
{
public:
						SampleDrawnCubeFaceTest	(tcu::TestContext&	testCtx,
												const std::string&	name,
												const std::string&	description,
												const tcu::IVec2&	size,
												const VkFormat		format);

	void				initPrograms			(SourceCollections& programCollection) const;
	TestInstance*		createInstance			(Context&			context) const;

private:
	const tcu::IVec2	m_size;
	const VkFormat		m_format;
};

SampleDrawnCubeFaceTest::SampleDrawnCubeFaceTest (tcu::TestContext&	testCtx,
												 const std::string&	name,
												 const std::string&	description,
												 const tcu::IVec2&	size,
												 const VkFormat		format)
	: TestCase	(testCtx, name, description)
	, m_size	(size)
	, m_format	(format)
{
}

void SampleDrawnCubeFaceTest::initPrograms (SourceCollections& programCollection) const
{
	std::ostringstream pipeline1VertexSrc;
	pipeline1VertexSrc
		<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
		<< "layout(location = 0) in vec4 a_position;\n"
		<< "void main (void) {\n"
		<< "    gl_Position = a_position;\n"
		<< "}\n";

	std::ostringstream pipeline1FragmentSrc;
	pipeline1FragmentSrc
			<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "layout(location = 0) out vec4 outColor;\n"
			<< "layout(push_constant) uniform constants {\n"
			<< "     int pass;\n"
			<< "} pc;\n"
			<< "void main() {\n"
			<< "   if (pc.pass == 1) {\n"
			<< "      outColor = vec4(0., 1., 1., 1.);\n"
			<< "    } else {\n"
			<< "      outColor = vec4(1., 0., 1., 1.);\n"
			<< "    }\n"
			<< "}\n";

	std::ostringstream pipeline2VertexSrc;
	pipeline2VertexSrc
		<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
		<< "layout(location = 0) in highp vec4 a_position;\n"
		<< "layout(location = 1) in vec2 inTexCoord;\n"
		<< "layout(location = 1) out vec2 fragTexCoord;\n"
		<< "void main (void) {\n"
		<< "    gl_Position = a_position;\n"
		<< "    fragTexCoord = inTexCoord;\n"
		<< "}\n";

	std::ostringstream pipeline2FragmentSrc;
	pipeline2FragmentSrc
		<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
		<< "layout(location = 0) out vec4 outColor;\n"
		<< "layout(location = 1) in vec2 fragTexCoord;\n"
		<< "layout(binding = 0) uniform samplerCube texSampler;\n"
		<< "void main() {\n"
		<< "     outColor = texture(texSampler, vec3(fragTexCoord.x, 1.0, fragTexCoord.y));\n"
		<< "     outColor += texture(texSampler, vec3(fragTexCoord.x, -1.0, fragTexCoord.y));\n"
		<< "     outColor += texture(texSampler, vec3(fragTexCoord.x, fragTexCoord.y, 1.0));\n"
		<< "     outColor += texture(texSampler, vec3(fragTexCoord.x, fragTexCoord.y, -1.0));\n"
		<< "     outColor /= 4.;\n"
		<< "}\n";

	programCollection.glslSources.add("vert1") << glu::VertexSource(pipeline1VertexSrc.str());
	programCollection.glslSources.add("vert2") << glu::VertexSource(pipeline2VertexSrc.str());
	programCollection.glslSources.add("frag1") << glu::FragmentSource(pipeline1FragmentSrc.str());
	programCollection.glslSources.add("frag2") << glu::FragmentSource(pipeline2FragmentSrc.str());
}

TestInstance* SampleDrawnCubeFaceTest::createInstance (Context& context) const
{
	return new SampleDrawnCubeFaceTestInstance(context, m_size, m_format);
}

} // anonymous ns

tcu::TestCaseGroup* createImageSampleDrawnCubeFaceTests	(tcu::TestContext& testCtx)
{
	const VkFormat		format	= VK_FORMAT_R8G8B8A8_UNORM;
	const tcu::IVec2	size	= tcu::IVec2(8, 8);

	de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "sample_cubemap", "Sample cube map faces that has been rendered to tests"));

	testGroup->addChild(new SampleDrawnCubeFaceTest(testCtx, "write_face_0", "", size, format));

	return testGroup.release();
}

} // image
} // vkt
